# KAPO USB Kernel CTF

Exploit the provided Linux 6.8.12 kernel as UID 65534 and read `/flag`. The
local image contains `KAPO{test_flag}`.

## Run

```sh
sha256sum -c SHA256SUMS
./run-qemu.sh
```

The guest mounts `work/` at `/work` and opens an unprivileged shell. Put your
exploit and source in that directory. KVM is used when available; otherwise the
launcher falls back to TCG.

## USB emulator contract

The organizer-owned USB emulator is already running as root, disconnected. It
only handles USB transport, descriptors, stage changes, and a Stage 2 payload
derived from the transfer context sent by the driver. It performs no heap
grooming.

After preparing each stage, write exactly one line to `/tmp/kapo-usb-ready`:

1. `helper` for the first enumeration.
2. Use the driver's next-stage ioctl, then write `seq`.
3. Use the next-stage ioctl again, then write `stage2`.

## Files

- `kernel/bzImage`: exact challenge kernel
- `kernel/vmlinux`: matching stripped ELF image
- `kernel/kernel.config`: matching kernel configuration
- `rootfs/initramfs.cpio.gz`: Ubuntu 24.04.4 challenge rootfs
- `helper/kapo-helper`: stripped analysis copy of the SUID helper

Ticket validation uses these original artifacts and runs only the submitted
guest exploit as UID 65534. Replacing the initramfs or trusted emulator is not a
valid solution.
