#!/bin/sh

export PATH=/usr/sbin:/usr/bin:/sbin:/bin

/bin/busybox mount -t proc -o hidepid=2 proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t devtmpfs devtmpfs /dev
/bin/busybox mkdir -p /tmp /work
/bin/busybox chmod 1777 /tmp
/bin/busybox mkfifo /tmp/yoasobi-ready
/bin/busybox chmod 0622 /tmp/yoasobi-ready
/usr/sbin/kapo-usb-emulator /tmp/yoasobi-ready >/dev/console 2>&1 &

if ! /bin/busybox mount -t 9p -o trans=virtio,version=9p2000.L \
	hostshare /work; then
	echo "warning: host work directory is unavailable"
fi

echo "KAPO participant VM"
echo "User: nobody (uid 65534)"
echo "Shared directory: /work"
echo "Run your exploit from /work."
exec /bin/busybox su -s /bin/bash nobody
