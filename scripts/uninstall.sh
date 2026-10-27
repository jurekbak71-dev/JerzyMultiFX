#!/bin/bash
set -euo pipefail
[[ $(id -u) == 0 ]] || { echo 'Uruchom przez sudo.';exit 1; }
for unit in jerzy-multifx-web jerzy-multifx-ble jerzy-multifx jerzy-multifx-jack;do systemctl disable --now "$unit.service" 2>/dev/null || true;done
backup=/var/backups/jerzy-multifx/uninstall-$(date -u +%Y%m%dT%H%M%SZ)
mkdir -p "$backup";chmod 700 "$backup"
[[ ! -d /opt/jerzy-multifx ]] || mv /opt/jerzy-multifx "$backup/application"
for unit in /etc/systemd/system/jerzy-multifx{,-web,-ble,-jack}.service;do [[ ! -f $unit ]] || mv "$unit" "$backup/";done
systemctl daemon-reload
echo "Wyłączono wyłącznie usługi Jerzy MultiFX. Dane i konfiguracja zachowane. Backup: $backup"
