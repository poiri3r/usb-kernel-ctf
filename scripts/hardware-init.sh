#!/bin/sh

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /tmp
chmod 1777 /tmp

watch_kapo_device() {
	while :; do
		found=0
		for vendor in /sys/bus/usb/devices/*/idVendor; do
			[ -f "$vendor" ] || continue
			device=${vendor%/idVendor}
			[ "$(cat "$vendor")" = cafe ] || continue
			[ "$(cat "$device/idProduct")" = 6801 ] || continue
			ln -sf "$device/descriptors" /tmp/kapo-descriptors
			found=1
			break
		done
		[ "$found" -eq 1 ] || rm -f /tmp/kapo-descriptors
		sleep 0.1
	done
}

watch_kapo_device &
echo "HARDWARE_READY: connect or reset the flashed RP2040 now"
/test-full-chain --hardware /tmp/kapo-descriptors /dev/kapo_ctf0 \
	/kapo-helper 0xffffffff81329050 0xffffffff82228560 0x1790
result=$?
if [ "$result" -eq 0 ]; then
	echo HARDWARE_FULL_CHAIN_PASS
else
	echo HARDWARE_FULL_CHAIN_FAIL result="$result"
fi
poweroff -f
