#!/bin/sh

set -eu

base_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
monitor_socket=${KAPO_MONITOR_SOCKET:-/tmp/kapo-qemu-monitor.sock}

mkdir -p "$base_dir/work"
rm -f "$monitor_socket"

exec qemu-system-x86_64 \
	-enable-kvm \
	-cpu host \
	-machine q35 \
	-smp 1 \
	-m 2G \
	-kernel "$base_dir/kernel/bzImage" \
	-initrd "$base_dir/rootfs/initramfs.cpio.gz" \
	-append "console=ttyS0 panic=-1" \
	-device qemu-xhci,id=xhci \
	-virtfs local,path="$base_dir/work",mount_tag=hostshare,security_model=mapped-xattr,id=hostshare \
	-monitor "unix:$monitor_socket,server=on,wait=off" \
	-nographic \
	-no-reboot
