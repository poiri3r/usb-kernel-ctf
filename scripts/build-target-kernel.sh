#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
	echo "usage: $0 <linux-6.8.12-source> <kernel-output>" >&2
	exit 1
fi

source_dir=$1
output_dir=$2
project_dir=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1788739200}
KBUILD_BUILD_USER=${KBUILD_BUILD_USER:-kapo}
KBUILD_BUILD_HOST=${KBUILD_BUILD_HOST:-builder}
KBUILD_BUILD_TIMESTAMP=${KBUILD_BUILD_TIMESTAMP:-$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%d %H:%M:%S UTC')}
KBUILD_BUILD_VERSION=${KBUILD_BUILD_VERSION:-1}
export SOURCE_DATE_EPOCH KBUILD_BUILD_USER KBUILD_BUILD_HOST \
	KBUILD_BUILD_TIMESTAMP KBUILD_BUILD_VERSION

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
sed -i 's/\r$//' "$source_dir/drivers/usb/class/Kconfig" \
	"$source_dir/drivers/usb/class/Makefile"

pipe_patch=$project_dir/kernel-patches/0001-pipe-clear-resized-ring.patch
if grep -Fq 'kfree_sensitive(pipe->bufs);' "$source_dir/fs/pipe.c"; then
	:
elif patch --dry-run --silent --batch --forward -d "$source_dir" -p1 <"$pipe_patch"; then
	patch --silent --batch --forward -d "$source_dir" -p1 <"$pipe_patch"
else
	echo "kernel source does not match the pipe resize hardening patch" >&2
	exit 1
fi

make -C "$source_dir" O="$output_dir" -j4 bzImage
