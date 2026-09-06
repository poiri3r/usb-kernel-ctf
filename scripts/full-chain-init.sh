#!/bin/sh

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /tmp
chmod 1777 /tmp

fail_runtime() {
	echo "FULL_CHAIN_INIT_FAIL $1"
	for log in /tmp/helper-leak-gadget.log /tmp/seq-gadget.log /tmp/stage2-gadget.log; do
		[ ! -f "$log" ] || cat "$log"
	done
	poweroff -f
	exit 1
}

wait_for_present() {
	attempts=0
	while [ ! -e "$1" ]; do
		attempts=$((attempts + 1))
		[ "$attempts" -lt 300 ] || fail_runtime "timeout waiting for $1"
		sleep 0.1
	done
}

wait_for_absent() {
	attempts=0
	while [ -e "$1" ]; do
		attempts=$((attempts + 1))
		[ "$attempts" -lt 300 ] || fail_runtime "timeout removing $1"
		sleep 0.1
	done
}

echo "RUNTIME_READY $(uname -r)"
stat -c 'HELPER mode=%a owner=%u:%g size=%s' /kapo-helper
stat -c 'FLAG mode=%a owner=%u:%g' /flag

/short-config helper /kapo-helper >/tmp/helper-leak-gadget.log 2>&1 &
helper_leak_pid=$!
wait_for_present /dev/kapo_ctf0
chmod 666 /dev/kapo_ctf0

/test-full-chain /sys/bus/usb/devices/1-1/descriptors /dev/kapo_ctf0 \
	/kapo-helper 0xffffffff81329050 0xffffffff82228560 0x1790 \
	>/tmp/full-chain.log 2>&1 &
exploit_pid=$!
wait_for_present /tmp/kapo-helper-complete
kill "$helper_leak_pid"
wait "$helper_leak_pid" 2>/dev/null
wait_for_absent /sys/bus/usb/devices/1-1/descriptors

/short-config seq >/tmp/seq-gadget.log 2>&1 &
seq_pid=$!
wait_for_present /dev/kapo_ctf0
chmod 666 /dev/kapo_ctf0
wait_for_present /tmp/kapo-seq-complete
kill "$seq_pid"
wait "$seq_pid" 2>/dev/null
wait_for_absent /sys/bus/usb/devices/1-1/descriptors

/short-config stage2 >/tmp/stage2-gadget.log 2>&1 &
stage2_pid=$!
wait_for_present /dev/kapo_ctf0
chmod 666 /dev/kapo_ctf0

wait "$exploit_pid"
result=$?
kill "$stage2_pid" 2>/dev/null
wait "$stage2_pid" 2>/dev/null
cat /tmp/full-chain.log
if [ "$result" -eq 0 ] && grep -Fqx 'KAPO{test_flag}' /tmp/full-chain.log; then
	echo FULL_CHAIN_INIT_PASS
else
	echo FULL_CHAIN_INIT_FAIL result="$result"
	cat /tmp/helper-leak-gadget.log
	cat /tmp/seq-gadget.log
	cat /tmp/stage2-gadget.log
fi
poweroff -f
