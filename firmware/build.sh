#!/bin/sh

set -eu

: "${PICO_SDK_PATH:?set PICO_SDK_PATH to Pico SDK 2.3.1}"

build_dir=${1:-build}
cmake -S "$(dirname "$0")" -B "$build_dir" -G Ninja \
	-DPICO_BOARD=pico -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --target kapo_rp2040
