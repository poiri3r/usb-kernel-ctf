#!/bin/sh

set -eu

base_dir=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
monitor_socket=${KAPO_MONITOR_SOCKET:-/tmp/kapo-qemu-monitor.sock}

if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
	accel=kvm
	cpu=host
else
	accel=tcg
	cpu=max
fi

mkdir -p "$base_dir/work"
rm -f "$monitor_socket"

exec qemu-system-x86_64 \
	-accel "$accel" \
	-cpu "$cpu" \
	-machine q35 \
	-smp 1 \
	-m 2G \
	-kernel "$base_dir/kernel/bzImage" \
	-initrd "$base_dir/rootfs/initramfs.cpio.gz" \
	-append "console=ttyS0 panic=-1" \
	-virtfs local,path="$base_dir/work",mount_tag=hostshare,security_model=mapped-xattr,id=hostshare \
	-monitor "unix:$monitor_socket,server=on,wait=off" \
	-netdev user,restrict=on,id=kapo_net \
	-device e1000,netdev=kapo_net \
	-nographic \
	-no-reboot
