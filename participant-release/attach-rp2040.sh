#!/bin/sh

set -eu

monitor_socket=${KAPO_MONITOR_SOCKET:-/tmp/kapo-qemu-monitor.sock}

[ -S "$monitor_socket" ] || {
	echo "QEMU monitor not found: $monitor_socket" >&2
	exit 1
}

python3 - "$monitor_socket" <<'PY'
import socket
import sys

with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as monitor:
    monitor.settimeout(1)
    monitor.connect(sys.argv[1])
    monitor.sendall(
        b"device_add usb-host,vendorid=0xcafe,productid=0x6801,id=kapo,bus=xhci.0\n"
        b"info usb\n"
    )
    try:
        while data := monitor.recv(4096):
            sys.stdout.buffer.write(data)
    except TimeoutError:
        pass
PY
