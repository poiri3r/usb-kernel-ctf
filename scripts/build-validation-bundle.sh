#!/bin/sh

set -eu

if [ "$#" -ne 4 ]; then
	echo "usage: $0 <kernel-output> <firmware-build> <static-busybox> <output>" >&2
	exit 1
fi

kernel_output=$1
firmware_build=$2
busybox=$3
output=$4
project_dir=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
rootfs=$(mktemp -d /tmp/kapo-validation-root.XXXXXX)
archive=$(mktemp /tmp/kapo-validation-initramfs.XXXXXX.cpio)

[ ! -e "$output" ] || {
	echo "$output already exists" >&2
	exit 1
}
for file in \
	"$kernel_output/arch/x86/boot/bzImage" \
	"$kernel_output/vmlinux" \
	"$kernel_output/System.map" \
	"$kernel_output/.config" \
	"$firmware_build/kapo_rp2040.uf2" \
	"$busybox"; do
	[ -f "$file" ] || {
		echo "missing required file: $file" >&2
		exit 1
	}
done

mkdir -p "$output" "$rootfs/bin" "$rootfs/dev" "$rootfs/proc" \
	"$rootfs/sys" "$rootfs/tmp"
chmod 0755 "$rootfs"
install -m 0755 "$busybox" "$rootfs/bin/busybox"
for applet in sh mount mkdir chmod cat poweroff sleep ln rm; do
	ln -s busybox "$rootfs/bin/$applet"
done
gcc -static -Wall -Wextra -Werror \
	"$project_dir/scripts/test-full-chain.c" \
	"$project_dir/scripts/kapo-helper-shellcode.S" \
	-o "$rootfs/test-full-chain"
gcc -static -no-pie -Wall -Wextra -Werror \
	"$project_dir/helpers/kapo-helper.c" -o "$rootfs/kapo-helper"
install -m 0755 "$project_dir/scripts/hardware-init.sh" "$rootfs/init"
printf '%s\n' 'KAPO{test_flag}' >"$rootfs/flag"
chmod 0755 "$rootfs/test-full-chain"
chmod 4755 "$rootfs/kapo-helper"
chmod 0400 "$rootfs/flag"

(cd "$rootfs" && find . -print0 \
	| cpio --null -o --format=newc --owner=0:0 >"$archive")
gzip -9 "$archive"
install -m 0644 "$archive.gz" "$output/initramfs.cpio.gz"
install -m 0644 "$kernel_output/arch/x86/boot/bzImage" "$output/bzImage"
objcopy --strip-debug "$kernel_output/vmlinux" "$output/vmlinux"
install -m 0644 "$kernel_output/System.map" "$output/System.map"
install -m 0644 "$kernel_output/.config" "$output/kernel.config"
install -m 0644 "$firmware_build/kapo_rp2040.uf2" "$output/kapo-rp2040.uf2"
install -m 0644 "$project_dir/docs/HARDWARE-VALIDATION.md" "$output/README.md"
(
	cd "$output"
	sha256sum bzImage vmlinux System.map kernel.config kapo-rp2040.uf2 \
		initramfs.cpio.gz >SHA256SUMS
)
