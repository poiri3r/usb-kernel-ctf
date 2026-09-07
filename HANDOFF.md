# Handoff

This document contains the QEMU-only release-candidate checkpoint. Read it
after `docs/SPEC.md` and `STATUS.md`.

Updated: 2026-09-07 KST

## Active task

- The Jeopardy TCP/QEMU launcher and server-only flag injection path are
  implemented and locally runtime validated.
- One TCP connection uploads one base64 ELF, then receives one disposable QEMU
  serial console as UID 65534. Only the session ELF is shared through `/work`.
- Public test initramfs and an externally injected server test flag both reached
  `FULL_CHAIN_PASS`. Disconnect and SIGTERM cleanup both passed.
- The next task is an independent black-box solve using only
  `testkernelctf.tar.xz`, followed by difficulty scoring.

## Repository checkpoint

- Branch: `main`
- Base commit: `68248a0`
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
- Server runtime: `scripts/jeopardy_qemu_server.py`,
  `scripts/test_jeopardy_qemu_server.py`, `deploy/kapo-jeopardy.service`
- Server flag boundary: `scripts/build-participant-rootfs.sh`,
  `docs/JEOPARDY-SERVER.md`

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

The runtime regression used the current `scripts/test-full-chain.c` linked with
`scripts/kapo-helper-shellcode.S`, copied only temporarily into
`testkernelctf/work/`. Each run invoked:

```sh
/work/test-full-chain /sys/bus/usb/devices/1-1/descriptors \
  /dev/kapo_ctf0 /kapo-helper \
  0xffffffff81329030 0xffffffff822285a0 0x1790
```

Logs: `/tmp/kapo-rc-validation-1.log` through
`/tmp/kapo-rc-validation-5.log`. The temporary exploit was removed from the
release directory after validation.

## Verification

PASS:

- `scripts/trusted-usb-emulator.c`: static build with
  `-Wall -Wextra -Werror -pthread`
- `scripts/test-full-chain.c`: static build with warnings as errors
- Linux 6.8.12 `bzImage` rebuild with fixed `kapo@builder`, build version 1,
  and `2026-09-07 00:00:00 UTC`
- two initramfs rebuilds from the recorded inputs were byte-for-byte identical
- the reproducibly rebuilt release candidate passed QEMU 8.2.2, 1 vCPU,
  2 GiB TCG cold boot full chain 5/5; every run reached helper-page leak,
  KASLR leak, page-cache write, UID 0, `KAPO{test_flag}`, and `FULL_CHAIN_PASS`
- `nm` reports no symbols for distributed `vmlinux` and helper
- `testkernelctf/SHA256SUMS`: PASS
- initramfs ownership/modes: root-owned flag `0400`, helper `4755`, emulator
  `0755`
- archive listing and internal-material exclusion check: PASS

NOT TESTED / remaining:

- KVM execution path
- independent teammate LLM-only black-box solve
- physical RP2040 full chain
- real event flag and production systemd runtime

## Jeopardy runtime verification

- Local TCP public-image full chain: PASS (`/tmp/kapo-jeopardy-local-2.log`).
- Server test-flag image build: PASS; `/flag` was root-owned mode `0400` and
  contained only `KAPO{server_injection_test}`.
- Builder temporary rootfs and cpio cleanup after flag injection: PASS.
- Server test-flag TCP full chain: PASS
  (`/tmp/kapo-jeopardy-server-flag.log`).
- Client disconnect cleanup: PASS. Launcher SIGTERM cleanup: PASS.
- Python compile and three launcher unit tests: PASS.

## Next action

Perform a fresh black-box solve with only `testkernelctf.tar.xz`. Record the
solve path, elapsed time, missing information, unintended shortcuts, and
difficulty. Do not inspect this repository or internal documents during that
solve.
