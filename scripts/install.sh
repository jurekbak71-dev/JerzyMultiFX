#!/bin/bash
set -euo pipefail
# Run from a verified RPi binary package. Never formats disks or changes old audio services.
root=$(cd "$(dirname "$0")/.." && pwd)
if [[ ${1:-} == --help ]]; then
 echo 'sudo bash scripts/install.sh [--enable --device ALSA_DEVICE] [--ble]'; exit 0
fi
[[ $(id -u) == 0 ]] || { echo 'Uruchom przez sudo.'; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo 'Wymagany system Linux ARM64.'; exit 1; }
[[ -f /etc/debian_version ]] || { echo 'Wspierany Debian/Raspberry Pi OS 13 (trixie), 64-bit.'; exit 1; }
[[ $(getconf GNU_LIBC_VERSION | cut -d' ' -f2) == 2.4[1-9]* ]] || { echo 'Ta paczka wymaga glibc >= 2.41 (Debian 13).'; exit 1; }
enable=0;ble=0;device=''
while (($#)); do case "$1" in --enable) enable=1;; --ble) ble=1;; --device) shift;device=${1:?Brak identyfikatora ALSA};; *) echo "Nieznana flaga: $1";exit 1;; esac;shift;done
if ((enable)); then
 [[ -n $device && $device =~ ^[A-Za-z0-9_:,.-]+$ ]] || { echo 'Wybierz urządzenie z arecord -l i podaj --device. Nie zakładamy hw:0,0.';exit 1; }
 if pgrep -x jackd >/dev/null; then echo 'Działa już JACK. Nie zmieniono go. Zintegruj routing ręcznie przed włączeniem nowych usług.';exit 1;fi
fi
(cd "$root" && sha256sum -c SHA256SUMS)
backup=/var/backups/jerzy-multifx/$(date -u +%Y%m%dT%H%M%SZ)
mkdir -p "$backup";chmod 700 "$backup"
paths=()
for path in opt/jerzy-multifx etc/jerzy-multifx etc/systemd/system/jerzy-multifx.service etc/systemd/system/jerzy-multifx-web.service etc/systemd/system/jerzy-multifx-jack.service etc/systemd/system/jerzy-multifx-ble.service;do [[ ! -e /$path ]] || paths+=("$path");done
if ((${#paths[@]}));then tar -C / -czf "$backup/previous.tar.gz" "${paths[@]}";tar -tzf "$backup/previous.tar.gz" > "$backup/manifest.txt";fi
for unit in jerzy-multifx-web jerzy-multifx-ble jerzy-multifx jerzy-multifx-jack;do systemctl is-enabled "$unit.service" > "$backup/$unit.enabled" 2>/dev/null || true;done
# Dependencies preserve apt signature/TLS validation.
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends jackd2 libjack-jackd2-0 alsa-utils python3 avahi-daemon
if ((ble));then apt-get install -y --no-install-recommends bluez rfkill python3-dbus python3-gi;fi
id jerzymultifx >/dev/null 2>&1 || useradd --system --create-home --home-dir /var/lib/jerzy-multifx --groups audio jerzymultifx
mkdir -p /opt/jerzy-multifx /etc/jerzy-multifx /var/lib/jerzy-multifx
cp -a "$root/bin" "$root/rpi" "$root/ui" "$root/scripts" "$root/source.tar.gz" /opt/jerzy-multifx/
chown -R root:root /opt/jerzy-multifx
chown -R jerzymultifx:jerzymultifx /var/lib/jerzy-multifx
if [[ ! -f /etc/jerzy-multifx/audio.env ]];then printf 'MULTIFX_DEVICE=%s\n' "${device:-NOT_CONFIGURED}" > /etc/jerzy-multifx/audio.env;fi
if [[ -n $device ]];then printf 'MULTIFX_DEVICE=%s\n' "$device" > /etc/jerzy-multifx/audio.env;fi
for unit in "$root"/rpi/*.service;do install -m 644 "$unit" /etc/systemd/system/;done
systemctl daemon-reload
if ((enable));then systemctl enable --now jerzy-multifx-jack.service jerzy-multifx.service jerzy-multifx-web.service;fi
if ((ble));then python3 /opt/jerzy-multifx/rpi/bluetooth_recovery.py --install;systemctl enable jerzy-multifx-ble.service;echo 'BLE zainstalowane. Skonfiguruj parowanie BlueZ i uruchom usługę po sprawdzeniu adaptera.';fi
echo "Zainstalowano. Backup: $backup. Dane użytkownika pozostają w /var/lib/jerzy-multifx."
echo 'Panel otwiera się automatycznie, bez wpisywania klucza.'
