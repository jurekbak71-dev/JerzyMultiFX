#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
platform=${1:-native}
jobs=${MULTIFX_BUILD_JOBS:-3}
args=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-O2)
case "$platform" in
 native) build=.build/native;;
 arm64) build=.build/rpi-arm64;args+=(-DCMAKE_TOOLCHAIN_FILE=cmake/arm64.cmake -DCMAKE_SKIP_RPATH=ON);unset CFLAGS CXXFLAGS LDFLAGS;export LD_LIBRARY_PATH="$root/.build/toolchains/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}";;
 windows) build=.build/windows;args+=(-DCMAKE_TOOLCHAIN_FILE=cmake/windows-x64.cmake -DMULTIFX_VST3=ON);unset CFLAGS CXXFLAGS LDFLAGS;;
 *) echo 'Usage: bash scripts/build.sh native|windows|arm64';exit 2;;
esac
if [[ $platform == windows ]];then
 sdk=${VST3_SDK_ROOT:-$root/dependencies/vst3sdk}
 if [[ ! -f $sdk/CMakeLists.txt && -f /workspace/.setup/dependencies/vst3sdk/CMakeLists.txt ]];then sdk=/workspace/.setup/dependencies/vst3sdk;fi
 if [[ ! -f $sdk/CMakeLists.txt ]];then
  sdk="$root/.build/dependencies/vst3sdk"
  git clone --no-checkout https://github.com/steinbergmedia/vst3sdk.git "$sdk"
  git -C "$sdk" checkout --detach 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
  git -C "$sdk" submodule update --init --recursive
 fi
 if [[ -d $sdk/.git || -f $sdk/.git ]];then revision=$(git -C "$sdk" rev-parse HEAD);else revision=$(cat "$sdk/REVISION");fi
 [[ $revision == 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96 ]] || { echo 'SDK revision mismatch';exit 1; }
 args+=(-DVST3_SDK_ROOT="$sdk")
fi
cmake -S . -B "$build" "${args[@]}"
cmake --build "$build" --parallel "$jobs"
if [[ $platform == native ]];then ctest --test-dir "$build" --output-on-failure --no-tests=error;fi
