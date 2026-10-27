#!/bin/bash
set -euo pipefail
[[ $(id -u) == 0 ]] || { echo 'Uruchom przez sudo.';exit 1; }
archive=${1:?Podaj /var/backups/jerzy-multifx/.../previous.tar.gz}
[[ $archive == /var/backups/jerzy-multifx/*/previous.tar.gz && -f $archive && $(stat -c %u "$archive") == 0 ]] || { echo 'Nieprawidłowe archiwum.';exit 1; }
python3 - "$archive" <<'PY'
import sys,tarfile
with tarfile.open(sys.argv[1]) as t:
 for m in t:
  if m.name.startswith('/') or '..' in m.name.split('/') or m.issym() or m.islnk():raise SystemExit('Unsafe backup entry')
PY
"$(dirname "$0")/uninstall.sh"
tar -C / -xzf "$archive"
systemctl daemon-reload
echo 'Przywrócono pliki. Sprawdź audio.env i ręcznie uruchom wcześniej wybrane usługi; stare platformy nie zostały zmienione.'
