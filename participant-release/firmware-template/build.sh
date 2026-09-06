#!/bin/sh

set -eu

: "${PICO_SDK_PATH:?set PICO_SDK_PATH to Pico SDK 2.3.1}"

cmake -S "$(dirname "$0")" -B "$(dirname "$0")/build" -G Ninja \
	-DPICO_BOARD=pico -DCMAKE_BUILD_TYPE=Release
cmake --build "$(dirname "$0")/build" --target kapo_firmware
