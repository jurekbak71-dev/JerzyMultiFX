#!/usr/bin/env python3
"""Installer for the existing jerzy/default JACK/HeadRush deployment; no network changes."""
import base64, ctypes, hashlib, io, json, os, platform, shutil, socket, subprocess, sys, tarfile, time
from pathlib import Path

OLD = ['jerzy-audio-platform-core.service', 'jerzy-audio-platform-web.service']
NEW = ['jerzy-multifx-preview-core.service', 'jerzy-multifx-preview-web.service']

def ctl(*args, check=True):
    return subprocess.run(['systemctl', '--user', *args], text=True, capture_output=True, check=check)

def state(unit):
    return {'active': ctl('is-active', unit, check=False).stdout.strip() == 'active',
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
    if os.geteuid() == 0 or Path.home() != Path('/home/jerzy'):
        raise RuntimeError('Zaloguj się jako jerzy. Uruchom bez sudo.')
    if platform.machine() != 'aarch64' or 'VERSION_CODENAME=trixie' not in Path('/etc/os-release').read_text():
        raise RuntimeError('Potrzebny jest system ARM64 Trixie. Niczego nie zmieniono.')
    os.environ['JACK_DEFAULT_SERVER'] = 'default'
    required = {'system:capture_1', 'system:capture_2', 'system:playback_1', 'system:playback_2'}
    if not required.issubset(jack_ports()):
        raise RuntimeError('HeadRush nie udostępnia oczekiwanych wejść i wyjść.')
    old_state = {u: state(u) for u in OLD}
    if not old_state[OLD[0]]['active']:
        raise RuntimeError('Nie znaleziono działającej starej instalacji. Prześlij zdjęcie tego komunikatu.')
    units = Path.home() / '.config/systemd/user'
    if any((units/u).exists() for u in NEW):
        raise RuntimeError('Nowy MultiFX jest już zainstalowany. Nie nadpisano Twoich plików.')
    for port in [8097, 9817]:
        with socket.socket() as s:
            try: s.bind(('0.0.0.0', port))
            except OSError: raise RuntimeError('Działa już inny panel lub MultiFX. Prześlij zdjęcie komunikatu.')
    raw = base64.b64decode(PAYLOAD)
    if hashlib.sha256(raw).hexdigest() != PAYLOAD_SHA256:
        raise RuntimeError('Instalator jest uszkodzony. Pobierz go ponownie.')
    base = Path.home()/'.local/share/jerzy-multifx-preview'
    base.mkdir(parents=True, exist_ok=True)
    run = base / time.strftime('instalacja-%Y%m%d-%H%M%S')
    run.mkdir(mode=0o700)
    root = unpack(raw, run)
    binary = root/'bin/jerzy-multifx'
    binary.chmod(0o755)
    deps = subprocess.run(['ldd', str(binary)], text=True, capture_output=True)
    if deps.returncode or 'not found' in deps.stdout + deps.stderr:
        raise RuntimeError('Brakuje składników potrzebnych do uruchomienia. Stara instalacja nadal działa.')
    data = run/'dane'; data.mkdir(mode=0o700)
    (run/'poprzednie-uslugi.json').write_text(json.dumps(old_state, indent=2))
    rollback = run/'COFNIJ.sh'
    lines = ['#!/bin/bash', 'set -e', 'systemctl --user disable --now ' + ' '.join(NEW) + ' || true']
    for unit, old in old_state.items():
        if old['enabled']: lines.append('systemctl --user enable ' + unit)
        if old['active']: lines.append('systemctl --user start ' + unit)
    lines.append("echo 'Przywrócono poprzedni program. Sieć i HeadRush bez zmian.'")
    rollback.write_text('\n'.join(lines)+'\n'); rollback.chmod(0o700)
    print('Instaluję nowy MultiFX. Wi-Fi i ustawienia HeadRush pozostają bez zmian.', flush=True)
    linger = subprocess.run(['loginctl', 'show-user', 'jerzy', '-p', 'Linger', '--value'], text=True, capture_output=True)
    if linger.stdout.strip() != 'yes':
        print('Wpisz hasło użytkownika jerzy, aby program działał również po wylogowaniu.', flush=True)
        subprocess.run(['sudo', 'loginctl', 'enable-linger', 'jerzy'], check=True)
    units.mkdir(parents=True, exist_ok=True)
    def quote(path):
        return '"' + str(path).replace('\\','\\\\').replace('"','\\"').replace('%','%%') + '"'
    common = '\nRestart=on-failure\nRestartSec=3\nTimeoutStopSec=5\nNoNewPrivileges=true\n\n[Install]\nWantedBy=default.target\n'
    core = '[Unit]\nDescription=Jerzy MultiFX Preview on existing HeadRush JACK\n\n[Service]\nEnvironment=JACK_DEFAULT_SERVER=default\nEnvironment="MULTIFX_DATA='+str(data)+'"\nExecStart='+quote(binary)+common
    web = '[Unit]\nDescription=Jerzy MultiFX Preview panel\nAfter='+NEW[0]+'\nRequires='+NEW[0]+'\n\n[Service]\nExecStart=/usr/bin/python3 '+quote(root/'rpi/web.py')+' --host 0.0.0.0 --data '+quote(data)+' --ui '+quote(root/'ui')+common
    changed = False
    try:
        # Old presets, program files, Bluetooth relay, JACK, and hardware broker remain untouched.
        changed = True
        ctl('stop', OLD[1], OLD[0])
        ctl('disable', *OLD)
        if any(p.startswith('JerzyAudioPlatform:') for p in jack_ports()):
            raise RuntimeError('Stary silnik nie zatrzymał się. Przerwano wymianę.')
        (units/NEW[0]).write_text(core)
        (units/NEW[1]).write_text(web)
        ctl('daemon-reload')
        ctl('enable', '--now', *NEW)
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
    print('\nGOTOWE. Na komputerze otwórz: http://bassambient.local:8097')
    print('Wklej na stronie ten klucz dostępu (nie wysyłaj go innym):\n'+token)
    print('Stary program zachowany. Powrót do niego:')
    print('bash '+str(rollback))
    print('Wersja próbna — może być słyszalne opóźnienie dźwięku.')

if __name__ == '__main__':
    try: main()
    except (Exception, KeyboardInterrupt) as error:
        print('\nSTOP: '+str(error)+'\nZrób zdjęcie tego komunikatu. Zasłoń klucz dostępu.', file=sys.stderr)
        sys.exit(1)
