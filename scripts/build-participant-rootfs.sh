#!/bin/sh

set -eu

if [ "$#" -ne 4 ]; then
	echo "usage: $0 <ubuntu-base.tar.gz> <static-busybox> <initramfs-output> <helper-output>" >&2
	exit 1
fi

ubuntu_base=$1
busybox=$2
initramfs_output=$3
helper_output=$4
project_dir=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
rootfs=$(mktemp -d /tmp/kapo-participant-root.XXXXXX)
archive=$(mktemp /tmp/kapo-participant-initramfs.XXXXXX.cpio)

for file in "$ubuntu_base" "$busybox"; do
	[ -f "$file" ] || {
		echo "missing required file: $file" >&2
		exit 1
	}
done
[ ! -e "$initramfs_output" ] || {
	echo "$initramfs_output already exists" >&2
	exit 1
}
[ ! -e "$helper_output" ] || {
	echo "$helper_output already exists" >&2
	exit 1
}

tar -xzf "$ubuntu_base" -C "$rootfs" --no-same-owner
mkdir -p "$rootfs/dev" "$rootfs/proc" "$rootfs/sys" "$rootfs/tmp" \
	"$rootfs/work"
chmod 0755 "$rootfs"
install -m 0755 "$busybox" "$rootfs/bin/busybox"
install -m 0755 "$project_dir/scripts/participant-init.sh" "$rootfs/init"
gcc -static -no-pie -Wall -Wextra -Werror \
	"$project_dir/helpers/kapo-helper.c" -o "$rootfs/kapo-helper"
install -m 0755 "$rootfs/kapo-helper" "$helper_output"
printf '%s\n' 'KAPO{test_flag}' >"$rootfs/flag"
chmod 4755 "$rootfs/kapo-helper"
chmod 0400 "$rootfs/flag"
chmod 1777 "$rootfs/tmp" "$rootfs/work"

(cd "$rootfs" && find . -xdev -print0 \
	| cpio --null -o --format=newc --owner=0:0 >"$archive")
gzip -9 "$archive"
install -m 0644 "$archive.gz" "$initramfs_output"
