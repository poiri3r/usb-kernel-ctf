# KAPO USB Kernel CTF — Participant Package

> Pre-release: the physical RP2040 path is pending organizer validation.

This directory is the complete participant download. Files outside this
directory are organizer material and are not part of the challenge package.

## Contents

- `kernel/`: exact Linux 6.8.12 target image, symbols, and configuration
- `rootfs/initramfs.cpio.gz`: Ubuntu 24.04.4 test environment
- `helper/kapo-helper`: analysis copy of the root-owned SUID target
- `firmware-template/`: minimal Pico SDK project to extend for RP2040 Zero
- `run-qemu.sh`: starts the VM with a shared `work/` directory
- `attach-rp2040.sh`: attaches a flashed `cafe:6801` USB device to the VM
- `SHA256SUMS`: hashes of the distributed artifacts

No completed exploit or working attack firmware is included.

## Requirements

- Linux host with QEMU/KVM, Python 3, and USB access
- RP2040 Zero and a data-capable USB cable
- Pico SDK 2.3.1 and an Arm GNU toolchain

## Start the target

1. Run `./run-qemu.sh`. Keep the RP2040 disconnected or held in reset.
2. Put exploit source or a static exploit binary in `work/`. It appears as
   `/work` inside the guest.
3. The guest shell runs as UID 65534. Start heap grooming before attaching the
   board.
4. Flash your firmware, connect the RP2040, then run `./attach-rp2040.sh` in a
   second host terminal.
5. Use the USB descriptor disclosure and `/dev/kapo_ctf0` to obtain UID 0 and
   read `/flag`.

The local success value is `kapo{test_flag}`. Ticket validation uses the same
kernel, configuration, driver, helper, and rootfs layout with a different flag.

## Ticket submission

Submit the RP2040 firmware image and source, guest exploit binary and source,
build/run instructions, and hashes for every submitted file.
