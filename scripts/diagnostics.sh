#!/bin/bash
set -u
uname -a
getconf GNU_LIBC_VERSION
command -v arecord >/dev/null && arecord -l
command -v aplay >/dev/null && aplay -l
command -v lsusb >/dev/null && lsusb
command -v aconnect >/dev/null && aconnect -l
for unit in jerzy-multifx-jack jerzy-multifx jerzy-multifx-web jerzy-multifx-ble;do systemctl --no-pager status "$unit.service" || true;done
# Deliberately no environment dump, token, or credential output.
echo 'Token panelu jest pomijany w diagnostyce.'
