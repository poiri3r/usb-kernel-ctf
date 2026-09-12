#!/bin/sh

set -eu

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
	echo "usage: $0 <ubuntu-base.tar.gz> <static-busybox> <initramfs-output> [flag-file]" >&2
	exit 1
fi

ubuntu_base=$1
busybox=$2
initramfs_output=$3
flag_file=${4:-}
project_dir=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
rootfs=
archive=

cleanup() {
	status=$?
	trap - EXIT HUP INT TERM
	[ -z "$rootfs" ] || rm -rf -- "$rootfs" || :
	[ -z "$archive" ] || rm -f -- "$archive" "$archive.gz" || :
	exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

rootfs=$(mktemp -d /tmp/kapo-participant-root.XXXXXX)
archive=$(mktemp /tmp/kapo-participant-initramfs.XXXXXX.cpio)
source_date_epoch=${SOURCE_DATE_EPOCH:-1788739200}

for file in "$ubuntu_base" "$busybox"; do
	[ -f "$file" ] || {
		echo "missing required file: $file" >&2
		exit 1
	}
done
if [ -n "$flag_file" ]; then
	[ -f "$flag_file" ] || {
		echo "missing required file: $flag_file" >&2
		exit 1
	}
	flag_pattern='^kapo\{[^{}[:space:]]+\}$'
	[ "$(LC_ALL=C grep -Ec "$flag_pattern" "$flag_file")" -eq 1 ] &&
		! LC_ALL=C grep -Evq "$flag_pattern" "$flag_file" || {
		echo "flag file must contain exactly one kapo{...} line" >&2
		exit 1
	}
fi
[ ! -e "$initramfs_output" ] || {
	echo "$initramfs_output already exists" >&2
	exit 1
}
tar -xzf "$ubuntu_base" -C "$rootfs" --no-same-owner
mkdir -p "$rootfs/dev" "$rootfs/proc" "$rootfs/sys" "$rootfs/tmp" \
	"$rootfs/work"
chmod 0755 "$rootfs"
install -m 0755 "$busybox" "$rootfs/bin/busybox"
install -m 0755 "$project_dir/scripts/participant-init.sh" "$rootfs/init"
gcc -static -no-pie -Wall -Wextra -Werror \
	"$project_dir/scripts/kapo.c" -o "$rootfs/kapo"
gcc -static -Wall -Wextra -Werror -pthread \
	"$project_dir/scripts/trusted-usb-emulator.c" \
	-o "$rootfs/usr/sbin/kapo-usb-emulator"
strip --strip-all "$rootfs/usr/sbin/kapo-usb-emulator" "$rootfs/kapo"
chmod 0700 "$rootfs/usr/sbin/kapo-usb-emulator"
if [ -n "$flag_file" ]; then
	install -m 0400 "$flag_file" "$rootfs/flag"
else
	printf '%s\n' 'kapo{test_flag}' >"$rootfs/flag"
fi
chmod 4755 "$rootfs/kapo"
chmod 0400 "$rootfs/flag"
chmod 1777 "$rootfs/tmp" "$rootfs/work"
find "$rootfs" -xdev -exec touch -h -d "@$source_date_epoch" {} +

(cd "$rootfs" && find . -xdev -print0 | LC_ALL=C sort -z \
	| cpio --null -o --format=newc --owner=0:0 --reproducible >"$archive")
gzip -n -9 "$archive"
install -m 0644 "$archive.gz" "$initramfs_output"
