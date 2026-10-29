#!/bin/bash
set -euo pipefail
package=/usr/share/jerzy-multifx
if [[ ${1:-} == --help ]]; then
  echo 'sudo jerzy-multifx-setup [--enable --device hw:NAZWA] [--ble]'
  echo 'Bez --enable instaluje pliki, tworzy kopię zapasową i nie uruchamia audio.'
  exit 0
fi
[[ $(id -u) == 0 ]] || { echo 'Uruchom przez sudo.'; exit 1; }
[[ -r "$package/SHA256SUMS" && -x "$package/bin/jerzy-multifx" ]] || { echo 'Brak kompletnej paczki ARM64.'; exit 1; }
cd "$package"
sha256sum -c SHA256SUMS >/dev/null
exec bash "$package/scripts/install.sh" "$@"
