#!/bin/sh

set -eu

if [ "$#" -ne 4 ]; then
	echo "usage: $0 <kernel-output> <initramfs> <output-dir> <archive>" >&2
	exit 1
fi

kernel_output=$1
initramfs=$2
output=$3
archive=$4
project_dir=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
source_date_epoch=${SOURCE_DATE_EPOCH:-1788739200}

for file in "$kernel_output/arch/x86/boot/bzImage" "$initramfs"; do
	[ -f "$file" ] || {
		echo "missing required file: $file" >&2
		exit 1
	}
done
[ ! -e "$output" ] || {
	echo "$output already exists" >&2
	exit 1
}
[ ! -e "$archive" ] || {
	echo "$archive already exists" >&2
	exit 1
}

mkdir -p "$output/kernel" "$output/rootfs"
install -m 0644 "$project_dir/participant/testkernelctf/README.md" "$output/README.md"
install -m 0755 "$project_dir/participant/testkernelctf/run-qemu.sh" "$output/run-qemu.sh"
install -m 0644 "$kernel_output/arch/x86/boot/bzImage" "$output/kernel/bzImage"
install -m 0644 "$initramfs" "$output/rootfs/initramfs.cpio.gz"
(
	cd "$output"
	find . -type f ! -path './SHA256SUMS' -print \
		| LC_ALL=C sort | xargs sha256sum >SHA256SUMS
)
find "$output" -exec touch -h -d "@$source_date_epoch" {} +
tar --sort=name --mtime="@$source_date_epoch" --owner=0 --group=0 \
	--numeric-owner -C "$(dirname "$output")" -czf "$archive" "$(basename "$output")"
