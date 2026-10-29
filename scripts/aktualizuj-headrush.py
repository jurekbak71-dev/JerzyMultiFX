#!/usr/bin/env python3
"""Installer for the existing jerzy/default JACK/HeadRush deployment; no network changes."""
import base64, ctypes, hashlib, io, json, os, platform, pwd, shlex, shutil, socket, subprocess, sys, tarfile, time
from pathlib import Path

OLD = ['jerzy-audio-platform-core.service', 'jerzy-audio-platform-web.service', 'jerzy-audio-platform-midi.service']
NEW = ['jerzy-multifx-preview-core.service', 'jerzy-multifx-preview-web.service', 'jerzy-multifx-bluetooth-midi.service']
BRIDGE='jerzy-multifx-usb-midi.service'

def ctl(*args, check=True):
    return subprocess.run(['systemctl', '--user', *args], text=True, capture_output=True, check=check)

def state(unit):
    return {'active': ctl('is-active', unit, check=False).stdout.strip() in ('active','activating','reloading'),
            'enabled': ctl('is-enabled', unit, check=False).stdout.strip() == 'enabled'}

def unpack(raw, destination):
    with tarfile.open(fileobj=io.BytesIO(raw), mode='r:gz') as archive:
        for m in archive.getmembers():
            parts = Path(m.name).parts
            if m.name.startswith('/') or '..' in parts or not (m.isfile() or m.isdir()):
                raise RuntimeError('Nieprawidłowa zawartość paczki.')
        archive.extractall(destination, filter='data')
    roots = list(destination.glob('*/bin/jerzy-multifx'))
    if len(roots) != 1:
        raise RuntimeError('Brak programu w paczce.')
    root = roots[0].parents[1]
    for line in (root / 'SHA256SUMS').read_text().splitlines():
        digest, name = line.split('  ', 1)
        target = root / name
        if Path(name).is_absolute() or '..' in Path(name).parts or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            raise RuntimeError('Uszkodzony plik programu.')
    return root

def jack_ports():
    lib = ctypes.CDLL('libjack.so.0')
    lib.jack_client_open.restype = ctypes.c_void_p
    lib.jack_client_open.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint)]
    lib.jack_get_ports.restype = ctypes.POINTER(ctypes.c_char_p)
    lib.jack_get_ports.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_ulong]
    lib.jack_free.argtypes = [ctypes.c_void_p]
    lib.jack_client_close.argtypes = [ctypes.c_void_p]
    status = ctypes.c_uint()
    client = lib.jack_client_open(b'JerzyInstallerCheck', 1, ctypes.byref(status))
    if not client:
        raise RuntimeError('Brak połączenia z obecną obsługą HeadRush. Niczego nie zmieniono.')
    names = None
    try:
        names = lib.jack_get_ports(client, None, None, 0)
        result = set()
        if names:
            i = 0
            while names[i]:
                result.add(names[i].decode()); i += 1
        return result
    finally:
        if names: lib.jack_free(names)
        lib.jack_client_close(client)

def main():
    if '--help' in sys.argv:
        print('Uruchom jako jerzy: python3 JerzyMultiFX-AKTUALIZUJ.py. Nie używaj sudo.'); return
    if os.geteuid() == 0 or pwd.getpwuid(os.geteuid()).pw_name != 'jerzy':
        raise RuntimeError('Zaloguj się jako jerzy. Uruchom bez sudo.')
    if platform.machine() != 'aarch64' or 'VERSION_CODENAME=trixie' not in Path('/etc/os-release').read_text():
        raise RuntimeError('Potrzebny jest system ARM64 Trixie. Niczego nie zmieniono.')
    os.environ['JACK_DEFAULT_SERVER'] = 'default'
    required = {'system:capture_1', 'system:capture_2', 'system:playback_1', 'system:playback_2'}
    try:ports=jack_ports()
    except RuntimeError:
        print('Obsługa HeadRush nie odpowiada. Włącz HeadRush i sprawdź kabel USB.',flush=True)
        devices=subprocess.run(['aplay','-l'],text=True,capture_output=True)
        if 'HeadRush Core' not in devices.stdout:
            input('Po włączeniu HeadRush naciśnij Enter, aby spróbować ponownie. ')
        print('Ponownie uruchamiam obecną obsługę dźwięku. Ustawienia pozostają bez zmian.',flush=True)
        subprocess.run(['sudo','systemctl','restart','jerzy-jack.service'],check=True)
        time.sleep(3);ports=jack_ports()
    if not required.issubset(ports):
        raise RuntimeError('HeadRush nie udostępnia oczekiwanych wejść i wyjść.')
    previous = {u:state(u) for u in OLD+NEW+[BRIDGE]}
    units = Path.home()/'.config/systemd/user'
    # Existing MultiFX owns these ports during an update; unrelated listeners are not replaced.
    if not previous[NEW[0]]['active']:
        for port in [8097,9817]:
            with socket.socket() as sock:
                try:sock.bind(('0.0.0.0',port))
                except OSError:raise RuntimeError('Inny program zajmuje panel lub port audio. Prześlij zdjęcie komunikatu.')
    previous_data=None
    environment=ctl('show',NEW[0],'--property=Environment','--value',check=False).stdout
    for item in shlex.split(environment):
        if item.startswith('MULTIFX_DATA='):
            candidate=Path(item.split('=',1)[1]).resolve()
            if candidate.is_relative_to(Path.home()/'.local/share/jerzy-multifx-preview') and candidate.is_dir():previous_data=candidate
    raw = base64.b64decode(PAYLOAD)
    if hashlib.sha256(raw).hexdigest() != PAYLOAD_SHA256:
        raise RuntimeError('Instalator jest uszkodzony. Pobierz go ponownie.')
    base = Path.home()/'.local/share/jerzy-multifx-preview'
    base.mkdir(parents=True, exist_ok=True)
    run = base / time.strftime('instalacja-%Y%m%d-%H%M%S')
    run.mkdir(mode=0o700)
    root = unpack(raw, run)
    collector=root/'scripts/collect_rpi_diagnostics.py'
    def inventory(phase):
        if collector.is_file():
            try:
                subprocess.run(['python3',str(collector),'--output',str(run/('diagnostyka-'+phase+'.json'))],
                               timeout=55,check=False)
            except (OSError,subprocess.TimeoutExpired):
                print('Raport diagnostyczny nie powstał; instalacja trwa dalej.',flush=True)
    inventory('przed')
    binary = root/'bin/jerzy-multifx'
    binary.chmod(0o755)
    (root/'bin/jerzy-multifx-midi').chmod(0o755)
    for executable in (binary,root/'bin/jerzy-multifx-midi'):
        deps=subprocess.run(['ldd',str(executable)],text=True,capture_output=True)
        if deps.returncode or 'not found' in deps.stdout+deps.stderr:raise RuntimeError('Brakuje składników potrzebnych do uruchomienia. Stara instalacja nadal działa.')
    data = run/'dane'; data.mkdir(mode=0o700)
    if previous_data:
        for directory in ('presets','ir'):
            if (previous_data/directory).is_dir():shutil.copytree(previous_data/directory,data/directory)
        for name in ('token','midi-inputs-disabled.txt','audio-routing.txt','tempo.txt','input-mode.txt','last-state.jmfx'):
            if (previous_data/name).is_file():shutil.copy2(previous_data/name,data/name)
    backup=run/'poprzednie-pliki-uslug';backup.mkdir(mode=0o700)
    for unit in NEW+[BRIDGE]:
        if (units/unit).is_file():shutil.copy2(units/unit,backup/unit)

    (run/'poprzednie-uslugi.json').write_text(json.dumps(previous, indent=2))
    rollback = run/'COFNIJ.sh'
    lines=['#!/bin/bash','set -e','systemctl --user disable --now '+' '.join(NEW+[BRIDGE])+' || true']
    for unit in NEW+[BRIDGE]:
        if (backup/unit).exists():lines.append('cp -- '+shlex.quote(str(backup/unit))+' '+shlex.quote(str(units/unit)))
        else:lines.append('rm -f -- '+shlex.quote(str(units/unit)))
    lines.append('systemctl --user daemon-reload')
    for unit,old in previous.items():
        if old['enabled']:lines.append('systemctl --user enable '+unit)
        if old['active']:lines.append('systemctl --user start '+unit)
    lines.append("echo 'Przywrócono poprzedni program. Sieć i HeadRush bez zmian.'")
    rollback.write_text('\n'.join(lines)+'\n'); rollback.chmod(0o700)
    print('Instaluję nowy MultiFX. Wi-Fi i ustawienia HeadRush pozostają bez zmian.', flush=True)
    linger = subprocess.run(['loginctl', 'show-user', 'jerzy', '-p', 'Linger', '--value'], text=True, capture_output=True)
    if linger.stdout.strip() != 'yes':
        print('Wpisz hasło użytkownika jerzy, aby program działał również po wylogowaniu.', flush=True)
        subprocess.run(['sudo', 'loginctl', 'enable-linger', 'jerzy'], check=True)
    if not Path('/usr/bin/a2jmidid').is_file():
        print('Dodaję obsługę USB MIDI. W razie pytania wpisz hasło jerzy.',flush=True)
        subprocess.run(['sudo','apt-get','update'],check=True)
        subprocess.run(['sudo','apt-get','install','-y','--no-install-recommends','a2jmidid'],check=True)
    if not shutil.which('rfkill'):
        subprocess.run(['sudo','apt-get','install','-y','--no-install-recommends','rfkill'],check=True)
    units.mkdir(parents=True, exist_ok=True)
    def quote(path):
        return '"' + str(path).replace('\\','\\\\').replace('"','\\"').replace('%','%%') + '"'
    common = '\nRestart=always\nRestartSec=3\nTimeoutStopSec=5\nNoNewPrivileges=true\n\n[Install]\nWantedBy=default.target\n'
    core = '[Unit]\nDescription=Jerzy MultiFX Preview on existing HeadRush JACK\n\n[Service]\nEnvironment=JACK_DEFAULT_SERVER=default\nEnvironment="MULTIFX_DATA='+str(data)+'"\nExecStart='+quote(binary)+common
    web = '[Unit]\nDescription=Jerzy MultiFX Preview panel\nAfter='+NEW[0]+'\nWants='+NEW[0]+'\n\n[Service]\nExecStart=/usr/bin/python3 '+quote(root/'rpi/web.py')+' --host 0.0.0.0 --data '+quote(data)+' --ui '+quote(root/'ui')+common
    changed = False
    try:
        # Old presets, program files, Bluetooth relay, JACK, and hardware broker remain untouched.
        changed = True
        for u in NEW+OLD+[BRIDGE]:
            if previous[u]['active'] or previous[u]['enabled'] or (units/u).exists():ctl('stop',u)
        for u in OLD:
            if previous[u]['enabled']:ctl('disable',u)
        external_bridge=subprocess.run(['pgrep','-u',str(os.getuid()),'-x','a2jmidid'],capture_output=True).returncode==0
        if any(p.startswith('JerzyAudioPlatform:') for p in jack_ports()):
            raise RuntimeError('Stary silnik nie zatrzymał się. Przerwano wymianę.')
        (units/NEW[0]).write_text(core)
        (units/NEW[1]).write_text(web)
        (units/NEW[2]).write_text('[Unit]\nDescription=MultiFX Bluetooth MIDI relay\n\n[Service]\nEnvironment=JACK_DEFAULT_SERVER=default\nExecStart=/usr/bin/python3 '+quote(root/'rpi/bluetooth_relay.py')+' --binary '+quote(root/'bin/jerzy-multifx-midi')+' --data '+quote(data)+common)
        if not external_bridge:
            (units/BRIDGE).write_text('[Unit]\nDescription=USB MIDI to existing JACK\n\n[Service]\nEnvironment=JACK_DEFAULT_SERVER=default\nExecStart=/usr/bin/a2jmidid -e\nRestart=always\nRestartSec=3\n\n[Install]\nWantedBy=default.target\n')
        radio = subprocess.run(['sudo','python3',str(root/'rpi/bluetooth_recovery.py'),'--install'],text=True,capture_output=True,check=True)
        radio_backup=radio.stdout.strip().splitlines()[-1]
        if not radio_backup.startswith('/var/backups/jerzy-multifx-midi/update-'):
            raise RuntimeError('Nie potwierdzono kopii konfiguracji Bluetooth.')
        with rollback.open('a') as f:
            f.write('sudo python3 /usr/local/lib/jerzy-multifx/bluetooth_recovery.py --restore '+shlex.quote(radio_backup)+'\n')
        ctl('daemon-reload')
        ctl('enable', '--now', *NEW)
        if not external_bridge:ctl('enable','--now',BRIDGE)
        sys.path.insert(0, str(root/'rpi'))
        from control import rpc
        import urllib.request
        end = time.monotonic() + 40
        while True:
            try:
                token = (data/'token').read_text().strip()
                request = urllib.request.Request('http://127.0.0.1:8097/api/state', headers={'Authorization':'Bearer '+token})
                with urllib.request.urlopen(request, timeout=2) as response:
                    result = json.load(response)
                if len(result['values']) < 900: raise ValueError('Niepełny stan silnika')
                bridge_status=json.loads((data/'bluetooth-midi-bridge.json').read_text())
                if not bridge_status.get('running') or not bridge_status.get('connected'):raise ValueError('Most MIDI jeszcze się uruchamia')
                ports = jack_ports()
                if not {'JerzyMultiFX:input_L', 'JerzyMultiFX:output_L', 'JerzyMultiFX:midi_control'}.issubset(ports):
                    raise ValueError('Brak portów audio')
                break
            except (OSError, ValueError, KeyError):
                if time.monotonic() >= end: raise RuntimeError('Nowy program nie potwierdził działania.')
                time.sleep(1)
    except BaseException:
        if changed:
            subprocess.run(['bash', str(rollback)], check=False)
        raise
    if previous_data and not (data/'last-state.jmfx').is_file() and (previous_data/'last-state.jmfx').is_file():
        try:
            if not rpc('KEEP_MIDI '+str(previous_data/'last-state.jmfx')).get('ok'):print('Poprzednie przypisania MIDI pozostają w kopii starej wersji; można nauczyć je ponownie w panelu.')
        except (OSError,ValueError):print('Poprzednie przypisania MIDI pozostają w kopii starej wersji.')
    inventory('po')
    print('Raporty sprzętu: '+str(run/'diagnostyka-przed.json')+' oraz '+str(run/'diagnostyka-po.json'))
    print('\nGOTOWE. Na komputerze otwórz: http://bassambient.local:8097')
    print('Panel otwiera się automatycznie — bez wpisywania klucza.')
    print('Autostart włączony. Po restarcie Pi wraca ostatnie brzmienie i przypisania MIDI. Presety i poprzednia wersja zachowane.')
    print('Powrót do poprzedniej wersji:')
    print('bash '+str(rollback))
    print('Wersja próbna — może być słyszalne opóźnienie dźwięku.')

if __name__ == '__main__':
    try: main()
    except (Exception, KeyboardInterrupt) as error:
        print('\nSTOP: '+str(error)+'\nZrób zdjęcie tego komunikatu.', file=sys.stderr)
        sys.exit(1)
