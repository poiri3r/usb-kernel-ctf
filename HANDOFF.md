# Handoff

This document contains the current checkpoint needed to resume hardware
validation. Read it after `docs/SPEC.md` and `STATUS.md`.

Updated: 2026-09-06 KST

## Repository checkpoint

- Branch: `main`
- Checkpoint: the commit containing this document
- Remote: private `poiri3r/usb-kernel-ctf`
- This checkpoint includes the full-chain implementation, RP2040 firmware,
  participant pre-release, build scripts, and documentation

## Current outcome

- Existing target-equivalent QEMU full chain: PASS 10/10.
- Current Linux 6.8.12 source: rebuilt successfully after driver review fixes and
  hardware-stage support.
- Current validation initramfs: boot smoke PASS with 1 vCPU and 2 GiB; the log
  reached `HARDWARE_READY` and started the harness as UID 65534.
- RP2040 firmware: ARM build PASS with Pico SDK 2.3.1; host descriptor/payload
  protocol self-test PASS.
- Local validation artifacts: `dist/hardware-validation/`; all entries in
  `SHA256SUMS` pass.
- Physical RP2040 enumeration and exploit chain: NOT TESTED.
- Participant pre-release: `participant-release/`; Ubuntu 24.04.4 boot, UID/GID
  65534 shell, 9p work share, hashes, and firmware-template build pass.

## Artifact contents

`dist/hardware-validation/` is ignored by Git and contains:

- `kapo-rp2040.uf2`
- `bzImage`
- `vmlinux`
- `System.map`
- `kernel.config`
- `initramfs.cpio.gz`
- `SHA256SUMS`
- `README.md`

The initramfs contains the unprivileged hardware harness, a mode-4755 test
helper, and root-only `KAPO{test_flag}`. It is an internal validation image, not
the participant release.

`participant-release/` is the participant-only download folder. It contains the
exact kernel artifacts, a separate Ubuntu 24.04.4 initramfs, helper analysis
copy, QEMU/USB scripts, and a minimal non-exploit firmware template. It does not
contain the completed RP2040 UF2 or organizer full-chain harness.

## Reproduction inputs

- Linux source: `/tmp/usb-kernel-ctf-src.IavzRx`
- Linux output: `/tmp/usb-kernel-runtime-nokasan-out`
- Pico SDK 2.3.1: `/tmp/kapo-pico-sdk-2.3.1`
- RP2040 build: `/tmp/kapo-rp2040-build`
- Static BusyBox used for initramfs:
  `/tmp/usb-kernel-runtime-tools/root/usr/bin/busybox`

Temporary paths are convenience inputs only. Rebuild commands are documented in
`firmware/README.md`, `scripts/build-target-kernel.sh`, and
`scripts/build-validation-bundle.sh`.

## Verification boundary

Confirmed in the current session:

- current kernel image rebuild: PASS
- current kernel/initramfs boot smoke: PASS
- RP2040 ARM build and host protocol self-test: PASS
- static hardware harness build with warnings as errors: PASS
- all relevant shell scripts pass `sh -n`
- bundle hashes pass `sha256sum -c SHA256SUMS`
- `git diff --check` and `git diff --cached --check`: PASS
- participant Ubuntu 24.04.4 rootfs boot, UID/GID 65534 shell, and 9p work mount:
  PASS
- participant firmware template build to UF2: PASS
- participant release hashes and organizer-exploit exclusion check: PASS

Not confirmed:

- physical RP2040 USB enumeration
- helper and seq leaks over the physical device
- physical Stage 2 bulk overflow, page-cache write, UID 0, or flag acquisition
- repeated physical-run stability
- current-source Raw Gadget full-chain regression
- participant `attach-rp2040.sh` across physical disconnect/reconnect

## First resume action

1. Read `docs/HARDWARE-VALIDATION.md`.
2. Flash `dist/hardware-validation/kapo-rp2040.uf2`.
3. Boot `bzImage` with `initramfs.cpio.gz`, 1 vCPU, and 2 GiB RAM while keeping
   the board disconnected or in reset.
4. Pass the board through and connect/reset it only after `HARDWARE_READY`.
5. Save serial output and the exact VM USB passthrough command, then update
   `STATUS.md` and this file.

An independent Codex read-only review of the complete uncommitted checkpoint
reported zero actionable findings. Repeat review after any future diff change
before participant release.
