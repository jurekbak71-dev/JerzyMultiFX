#!/usr/bin/env python3
"""Root-owned, bounded recovery of the Bluetooth adapter; no pairing/network changes."""
import argparse, json, os, shutil, subprocess, sys, tempfile, time
from pathlib import Path
BASE=Path('/var/backups/jerzy-multifx-midi')
PROGRAM=Path('/usr/local/lib/jerzy-multifx/bluetooth_recovery.py')
SERVICE='jerzy-multifx-bluetooth-ready.service'
TIMER='jerzy-multifx-bluetooth-ready.timer'
FILES=(PROGRAM,Path('/etc/systemd/system')/SERVICE,Path('/etc/systemd/system')/TIMER)

def run(*args,timeout=10,check=False):
    return subprocess.run(args,text=True,capture_output=True,timeout=timeout,check=check,env={**os.environ,'LC_ALL':'C'})
def powered():
    return 'Powered: yes' in run('bluetoothctl','--timeout','4','show',timeout=5).stdout

def recover():
    if powered():return True
    if 'Hard blocked: yes' in run('rfkill','list','bluetooth').stdout:
        raise RuntimeError('Radio Bluetooth zablokowane sprzętowo; automatyczna naprawa nie może usunąć tej blokady.')
    def attempt():
        run('rfkill','unblock','bluetooth')
        for _ in range(3):
            time.sleep(.7)  # Unblock is asynchronous; an immediate Power On can return Busy.
            run('bluetoothctl','--timeout','4','power','on',timeout=5)
            if powered():return True
        return False
    if attempt():return True
    active=run('systemctl','is-active','jerzy-audio-hardware.service').returncode==0
    try:
        if active:run('systemctl','stop','jerzy-audio-hardware.service',check=True)
        run('systemctl','restart','bluetooth.service',check=True)
        return attempt()
    finally:
        if active:run('systemctl','start','jerzy-audio-hardware.service',check=True)

def restore(backup):
    backup=Path(backup)
    if backup.is_symlink() or not backup.resolve().is_relative_to(BASE.resolve()) or backup.stat().st_uid!=0:
        raise ValueError('Invalid recovery backup')
    saved=json.loads((backup/'manifest.json').read_text())
    run('systemctl','disable','--now',TIMER)
    run('systemctl','stop',SERVICE)
    for i,path in enumerate(FILES):
        if saved['files'][i]:shutil.copy2(backup/str(i),path)
        else:path.unlink(missing_ok=True)
    run('systemctl','daemon-reload',check=True)
    if saved['enabled']:run('systemctl','enable',TIMER,check=True)
    if saved['active']:run('systemctl','start',TIMER,check=True)

def install():
    BASE.mkdir(parents=True,exist_ok=True,mode=0o700)
    backup=Path(tempfile.mkdtemp(prefix='update-',dir=BASE))
    saved={'files':[p.exists() for p in FILES],
           'active':run('systemctl','is-active',TIMER).returncode==0,
           'enabled':run('systemctl','is-enabled',TIMER).returncode==0}
    for i,path in enumerate(FILES):
        if path.is_symlink():raise ValueError('Unsafe recovery path')
        if saved['files'][i]:shutil.copy2(path,backup/str(i))
    (backup/'manifest.json').write_text(json.dumps(saved))
    try:
        PROGRAM.parent.mkdir(parents=True,exist_ok=True,mode=0o755)
        shutil.copyfile(Path(__file__),PROGRAM);PROGRAM.chmod(0o755)
        FILES[1].write_text('[Unit]\nDescription=Recover MultiFX Bluetooth radio\nAfter=bluetooth.service systemd-rfkill.service\nWants=bluetooth.service\n[Service]\nType=oneshot\nExecStart=/usr/bin/python3 '+str(PROGRAM)+'\nTimeoutStartSec=180\n')
        FILES[2].write_text('[Unit]\nDescription=Keep MultiFX Bluetooth available\n[Timer]\nOnBootSec=5\nOnUnitInactiveSec=20\nUnit='+SERVICE+'\n[Install]\nWantedBy=timers.target\n')
        for path in FILES[1:]:path.chmod(0o644)
        run('systemctl','daemon-reload',check=True)
        run('systemctl','enable','--now',TIMER,check=True)
        # The timer recovers the radio asynchronously; installing must not block USB audio.
    except BaseException:
        restore(backup);raise
    print(str(backup))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--install',action='store_true');parser.add_argument('--restore',type=Path);args=parser.parse_args()
    if os.geteuid()!=0:raise SystemExit('Root privileges required for Bluetooth radio recovery.')
    try:
        if args.install:install()
        elif args.restore:restore(args.restore)
        elif not recover():raise RuntimeError('Radio nadal nie jest włączone; systemd ponowi próbę.')
    except Exception as error:print(str(error),file=sys.stderr);sys.exit(1)
