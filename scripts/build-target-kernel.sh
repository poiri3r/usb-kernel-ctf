#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
	echo "usage: $0 <linux-6.8.12-source> <kernel-output>" >&2
	exit 1
fi

source_dir=$1
output_dir=$2
project_dir=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)

[ "$(make -s -C "$source_dir" kernelversion)" = 6.8.12 ] || {
	echo "kernel source must be Linux 6.8.12" >&2
	exit 1
}
[ -f "$output_dir/.config" ] || {
	echo "$output_dir/.config is required" >&2
	exit 1
}

install -m 0644 "$project_dir/usb/core/config.c" \
	"$source_dir/drivers/usb/core/config.c"
install -m 0644 "$project_dir/usb/core/message.c" \
	"$source_dir/drivers/usb/core/message.c"
install -m 0644 "$project_dir/usb/class/customdriver.c" \
	"$source_dir/drivers/usb/class/customdriver.c"
install -m 0644 "$project_dir/usb/class/Kconfig" \
	"$source_dir/drivers/usb/class/Kconfig"
install -m 0644 "$project_dir/usb/class/Makefile" \
	"$source_dir/drivers/usb/class/Makefile"

make -C "$source_dir" O="$output_dir" -j4 bzImage
