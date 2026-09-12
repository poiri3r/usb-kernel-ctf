# RP2040 Hardware Validation Bundle

This internal bundle contains the target kernel, a test initramfs, and the
RP2040 firmware used to validate the physical USB path. It is not the participant
release.

## Files

- `kapo-rp2040.uf2`: RP2040 Zero firmware
- `bzImage`, `vmlinux`, `System.map`, `kernel.config`: Linux 6.8.12 target
- `initramfs.cpio.gz`: test rootfs with `kapo{test_flag}` and the hardware harness
- `SHA256SUMS`: bundle hashes

## Flash

1. Hold BOOTSEL while connecting the RP2040 Zero.
2. Copy `kapo-rp2040.uf2` to the mounted RPI-RP2 drive.
3. Disconnect the board after flashing.

## Boot and connect

Boot the kernel with 1 vCPU, 2 GiB RAM, KASLR enabled, and the supplied
initramfs. Pass the RP2040 through to the VM, but keep it disconnected or held in
reset until the serial console prints:

```text
HARDWARE_READY: connect or reset the flashed RP2040 now
```

Then connect or reset the board. The firmware enumerates in helper leak mode,
disconnects and reconnects in seq leak mode after vendor request `0x5c`, and
repeats once more in Stage 2 mode. Success ends with:

```text
PAGE_CACHE_WRITE_PASS
kapo{test_flag}
FULL_CHAIN_PASS
HARDWARE_FULL_CHAIN_PASS
```

The exact VM USB passthrough command remains environment-specific and must be
recorded after the first physical run.
