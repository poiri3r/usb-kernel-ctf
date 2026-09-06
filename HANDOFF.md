# Handoff

This document contains the current checkpoint needed to build the QEMU-only
participant release. Read it after `docs/SPEC.md` and `STATUS.md`.

Updated: 2026-09-06 KST

## Repository checkpoint

- Branch: `main`
- Checkpoint: the commit containing this document
- Base commit before the Direction 2 documentation checkpoint:
  `866ea5cb5792eb587b16e6d744c5b4caa0e59066`
- Remote: private `poiri3r/usb-kernel-ctf`
- This checkpoint includes the full-chain implementation, RP2040 firmware,
  participant pre-release, build scripts, and documentation

## Current outcome

- Active direction: build Direction 2, a QEMU-only software edition, before
  deciding whether to resume the RP2040 hardware edition.
- Direction 1 remains preserved in `docs/SPEC.md`; do not delete the firmware,
  hardware validation bundle, or hardware documentation.
- Direction 2 is a confirmed design but is NOT IMPLEMENTED.
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

## Direction 2 trust boundary

- The organizer supplies one trusted USB emulator that runs as root and exposes
  the helper-leak, seq-leak, and Stage 2 modes through `dummy_hcd` and Raw Gadget.
- The daemon starts disconnected and waits for an explicit readiness signal from
  the UID 65534 participant exploit before the first enumeration. Every later
  mode transition also requires `KAPO_NEXT_STAGE`, disconnect, participant-side
  preparation, and a stage-specific readiness signal before reconnect. Seq
  grooming occurs while disconnected before seq reconnect. Stage 2 live-pipe
  grooming and hole creation instead occur after Stage 2 reconnect and driver
  bind, immediately before the vulnerable ioctl, so enumeration cannot consume
  the intended hole.
- The emulator owns only USB transport, descriptors, reconnects, deserialization
  of the transfer context calculated by the participant and serialized/sent by
  the custom driver, and bulk payload generation from that context. All
  helper/seq/pipe grooming, leak interpretation, context calculation, and exploit
  triggers belong to the participant exploit. Do not copy the existing
  gadget-side grooming code into the trusted daemon.
- The participant supplies only a guest exploit binary/source, executed as UID
  65534 from `/work`.
- Never execute a participant-supplied USB emulator as root in the ticket guest;
  it could read `/flag` without exploiting the kernel.
- The distributed image contains only `KAPO{test_flag}`. The real-flag image is
  organizer-only and is never distributed.
- The participant and ticket environments use the same QEMU kernel, config,
  driver, helper, and trusted emulator; only the flag differs.
- Current `participant-release/` is a Direction 1 RP2040 pre-release, not the
  completed Direction 2 archive.

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
- Ubuntu 24.04.4 base rootfs used by `scripts/build-participant-rootfs.sh`:
  `/tmp/kapo-participant-input/ubuntu-base-24.04.4-base-amd64.tar.gz`
- Ubuntu base SHA-256:
  `c1e67ef7b17a6300e136118bd1dc04725009cb376c1aad10abcf8cd453628d58`
- Static BusyBox SHA-256:
  `dbac288c29ba568459550a2da9e7ae0ded6b1fc728ee9fad3044c44e62d6ac14`

Temporary paths are convenience inputs only. Rebuild commands are documented in
`firmware/README.md`, `scripts/build-target-kernel.sh`, and
`scripts/build-validation-bundle.sh`.

For Direction 2, use the Ubuntu base and static BusyBox above as inputs and adapt
`scripts/build-participant-rootfs.sh`; do not depend on the temporary directory
names in the final build procedure. Record a persistent download/source location
and verify these hashes before declaring the release reproducible.

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

- Direction 2 trusted-emulator implementation and boot integration
- Direction 2 participant archive and symbol-exposure hardening
- independent teammate solve using only the participant archive
- physical RP2040 USB enumeration
- helper and seq leaks over the physical device
- physical Stage 2 bulk overflow, page-cache write, UID 0, or flag acquisition
- repeated physical-run stability
- current-source Raw Gadget full-chain regression
- participant `attach-rp2040.sh` across physical disconnect/reconnect

## First resume action

1. Re-read `docs/SPEC.md` section 0 and preserve Direction 1 files unchanged.
2. Inspect `scripts/raw-gadget-short-config.c` and `firmware/main.c` and reuse only
   their USB transport, descriptor, reconnect, and context-based payload behavior.
   Do not reuse the Raw Gadget payload-file bypass that ignores the participant's
   transfer context. Implement the smallest organizer-owned Raw Gadget daemon
   that provides all three modes without any heap grooming or flag access.
3. Start that daemon as root from a Direction 2 initramfs, then drop to the UID
   65534 participant environment with `/work` mounted. Keep the daemon disconnected
   until the participant exploit finishes initial grooming and sends a readiness
   signal.
4. Verify enumeration, both leaks, Stage 2, UID 0, and `KAPO{test_flag}` on one
   cold boot before packaging or symbol stripping.
5. After the functional path passes, build a participant archive without the
   organizer exploit/harness, internal documents, real flag, RP2040 firmware, or
   hardware attach scripts.
6. Run cold boot 10/10, perform an independent diff/archive review, then hand only
   the participant archive to a teammate for a black-box solve.
