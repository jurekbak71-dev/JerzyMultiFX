#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
export LD_LIBRARY_PATH="$root/.build/toolchains/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export WINEPREFIX="${MF_WINEPREFIX:-$root/.build/wine-clean}"
export WINEDLLPATH="$root/.build/toolchains/usr/lib/x86_64-linux-gnu/wine"
export WINEDEBUG="${MF_WINEDEBUG:--all}"
exec "$root/.build/toolchains/usr/bin/proot" -b "$root/.build/toolchains/usr/share/wine:/usr/share/wine" -b "$root/.build/toolchains/usr/lib/wine:/usr/lib/wine" "$root/.build/toolchains/usr/lib/wine/wine64" "$@"
