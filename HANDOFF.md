# Handoff

This document contains the QEMU-only release-candidate checkpoint. Read it
after `docs/SPEC.md` and `STATUS.md`.

Updated: 2026-09-07 KST

## Repository checkpoint

- Branch: `main`
- Base commit: `a4e5e3e`
- Remote: private `poiri3r/usb-kernel-ctf`
- Direction 1 remains preserved in `participant-release/`, `firmware/`, and the
  hardware documentation.
- Direction 2 release candidate: `testkernelctf/` and `testkernelctf.tar.xz`.

## Current outcome

- The trusted Raw Gadget emulator starts as root during boot and waits
  disconnected on `/tmp/kapo-usb-ready`.
- It provides helper-leak, seq-leak, and Stage 2 modes. It handles only USB
  transport, descriptors, reconnects, transfer-context deserialization, and
  context-derived bulk payload generation.
- Helper/seq/pipe grooming, leak interpretation, context calculation, and all
  exploit triggers remain in the UID 65534 participant process.
- The Linux 6.8.12 release-candidate kernel includes the `custom_devnode()` disconnect NULL
  guard found during final QEMU testing.
- `CONFIG_KALLSYMS_ALL` is disabled. `System.map` is not distributed. The
  distributed `vmlinux` and helper are fully stripped.
- The release contains only `KAPO{test_flag}`; the real flag and organizer
  exploit are not present.

## Release-candidate artifacts

`testkernelctf/` contains:

- `README.md`
- `SHA256SUMS`
- `run-qemu.sh`
- `kernel/bzImage`
- `kernel/vmlinux` (stripped)
- `kernel/kernel.config`
- `rootfs/initramfs.cpio.gz`
- `helper/kapo-helper` (stripped analysis copy)
- empty `work/`

Archive:

- Path: `testkernelctf.tar.xz`
- SHA-256: `b161e85ed2bba672d4af68c919e838b9b950f2ea94a90997b5082b57bbaeedc8`

Excluded from the Direction 2 archive:

- `docs/SPEC.md`, `STATUS.md`, `HANDOFF.md`
- organizer full-chain exploit and harness
- RP2040 firmware/template and USB passthrough scripts
- `System.map`
- real flag, secrets, and credentials

## Changed files

- Design and state: `docs/SPEC.md`, `STATUS.md`, `HANDOFF.md`, `README.md`
- Kernel and runtime: `usb/class/customdriver.c`,
  `scripts/trusted-usb-emulator.c`, `scripts/participant-init.sh`,
  `scripts/test-full-chain.c`
- Reproducible builders: `scripts/build-target-kernel.sh`,
  `scripts/build-participant-rootfs.sh`, `scripts/build-qemu-release.sh`
- Release candidate: `testkernelctf.tar.xz` and every tracked file under
  `testkernelctf/`

## Reproduction inputs and commands

- Linux source: `/tmp/usb-kernel-ctf-src.IavzRx`
- Linux output: `/tmp/kapo-release-kernel-out`
- Ubuntu base: `/tmp/kapo-participant-input/ubuntu-base-24.04.4-base-amd64.tar.gz`
  (`c1e67ef7b17a6300e136118bd1dc04725009cb376c1aad10abcf8cd453628d58`)
- Static BusyBox: `/tmp/usb-kernel-runtime-tools/root/usr/bin/busybox`
  (`dbac288c29ba568459550a2da9e7ae0ded6b1fc728ee9fad3044c44e62d6ac14`)
- The WSL lacked `libelf-dev`; its Ubuntu package was extracted under `/tmp`
  and supplied with `HOSTCFLAGS`/`HOSTLDFLAGS` without changing repository files.

Commands used for the reproducible rebuild, with `release_build_root` and
`dep_root` set to fresh `/tmp` directories:

```sh
HOSTCFLAGS="-I$dep_root/root/usr/include" \
HOSTLDFLAGS="-L$dep_root/root/usr/lib/x86_64-linux-gnu" \
./scripts/build-target-kernel.sh \
  /tmp/usb-kernel-ctf-src.IavzRx /tmp/kapo-release-kernel-out

./scripts/build-participant-rootfs.sh \
  /tmp/kapo-participant-input/ubuntu-base-24.04.4-base-amd64.tar.gz \
  /tmp/usb-kernel-runtime-tools/root/usr/bin/busybox \
  "$release_build_root/initramfs.cpio.gz" "$release_build_root/kapo-helper"

./scripts/build-qemu-release.sh /tmp/kapo-release-kernel-out \
  "$release_build_root/initramfs.cpio.gz" "$release_build_root/kapo-helper" \
  "$release_build_root/package/testkernelctf" \
  "$release_build_root/package/testkernelctf.tar.xz"
```

## Verification

PASS:

- `scripts/trusted-usb-emulator.c`: static build with
  `-Wall -Wextra -Werror -pthread`
- `scripts/test-full-chain.c`: static build with warnings as errors
- Linux 6.8.12 `bzImage` rebuild with fixed `kapo@builder`, build version 1,
  and `2026-09-07 00:00:00 UTC`
- two initramfs rebuilds from the recorded inputs were byte-for-byte identical
- before the reproducibility rebuild, the same implementation passed 1 vCPU,
  2 GiB QEMU TCG cold boot full chain 5/5; every run reached helper-page leak,
  KASLR leak, page-cache write, UID 0, `KAPO{test_flag}`, and `FULL_CHAIN_PASS`
- `nm` reports no symbols for distributed `vmlinux` and helper
- `testkernelctf/SHA256SUMS`: PASS
- initramfs ownership/modes: root-owned flag `0400`, helper `4755`, emulator
  `0755`
- archive listing and internal-material exclusion check: PASS

NOT TESTED / remaining:

- runtime regression of the newly rebuilt release candidate; this WSL currently
  has no `qemu-system-x86_64` in PATH
- KVM execution path
- independent teammate LLM-only black-box solve
- physical RP2040 full chain
- real-flag image and event ticket procedure

## Next action

Run the newly rebuilt `testkernelctf.tar.xz` through TCG cold boot full chain
5/5. If it passes, give only that archive to teammates for an LLM-only black-box
solve. Collect solve time, missing-file reports, and unintended shortcuts. Do
not send this repository, `docs/SPEC.md`, `STATUS.md`, `HANDOFF.md`, or files
under `scripts/`.
