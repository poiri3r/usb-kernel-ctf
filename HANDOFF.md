# Handoff

This document contains the QEMU-only release-candidate checkpoint. Read it
after `docs/SPEC.md` and `STATUS.md`.

Updated: 2026-09-07 KST

## Active task

- Direction 2 implementation, local validation, and independent review are
  complete, and implementation commit `60d0e37` is pushed. Release-gate solve
  validation and production deployment are the active tasks.
- The Jeopardy TCP/QEMU launcher and server-only flag injection path are
  implemented and locally runtime validated.
- One TCP connection uploads one base64 ELF, then receives one disposable QEMU
  serial console as UID 65534. Only the session ELF is shared through `/work`.
- Public test initramfs and an externally injected server test flag both reached
  `FULL_CHAIN_PASS`. Disconnect and SIGTERM cleanup both passed.
- Local and TCP launchers now use the same explicit e1000 device with a QEMU
  `user,restrict=on` backend. This preserves the intended heap layout while
  blocking guest-initiated external networking. The unchanged single-hole,
  single-target exploit passed five fresh TCP boots and one local launcher boot.
- Regression evidence is in `/tmp/kapo-nonic-tcp-1.log`; final TCP evidence is
  `/tmp/kapo-restricted-final-{1..5}.log`. Every final run reached
  `PAGE_CACHE_WRITE_PASS`, UID 0, `KAPO{test_flag}`, and `FULL_CHAIN_PASS`.
- Three independent solves of the prior archive all bypassed the intended seq
  leak by resizing a live helper-backed pipe ring.
- The target kernel now clears only the old ring freed by `pipe_resize_ring()`.
  The prior A/B/C shortcut exploits fail 3/3, while the intended page-only + seq
  leak chain passes hardened cold boot 5/5.
- The Stage 2 ioctl accepts only runtime `page_cache_pipe_buf_ops`; an alternate
  ops regression reaches both leaks and is rejected with `EINVAL`.
- QEMU seq mode exposes eight equivalent 32-byte configurations so allocator
  noise does not make the intended leak intermittent. Hardware mode stays at one.
- A previously independent exploit was adapted to the pre-partial full-entry
  hardened archive in
  163 seconds and again obtained the flag using the seq leak and runtime
  `page_cache_pipe_buf_ops`. This is regression time, not a fresh solve sample.
- A fresh participant-only solve of that full-entry archive succeeded in 1,190
  seconds with the intended full-entry chain and no reported shortcut.
- The current candidate makes the in-guest emulator root-only (`0700` plus
  procfs `hidepid=2`) and reduces QEMU Stage 2 to a 16-byte overwrite of only
  `page/offset/len`. The organizer chain passes TCG cold boot 5/5.
- Three fresh archive-only partial-overwrite sessions reconstructed the partial
  overwrite mechanics and final page-cache goal but did not obtain the guest
  flag: 971, 1,637, and 1,787 seconds.
  Their common blocker was reliable Stage 1 reclaim of the helper-backed
  `pipe_buffer` page/ops data. Reported difficulty was about 8.5/10.
  Evidence: `/tmp/kapo-partial-solve-a/attempt.log` (SHA-256
  `2e4dfa2977d97ef47a5b2900eec6bed4cd70e60e002b9da216c44b3ba841551e`),
  `/tmp/kapo-partial-solve-b/REPORT.md` (`21ac0606875c3b233185410421142f497c7430028f62e8c3b815efe9beb78ac1`),
  and `/tmp/kapo-partial-solve-c/REPORT.md` (`7b0ae8d33b11bae29a4f4f8a34ad062d9ebbf8e26ca66e34ae9668a6161afb34`).

## Repository checkpoint

- Branch: `main`
- Previous implementation commit: `60d0e37`
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
- `docs/WRITEUP.md` is the internal concise explanation of the intended
  primitive chain and must remain outside the participant archive.

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
- archive `work/` containing only `.gitkeep`

The local checkout currently also has the user's untracked
`testkernelctf/work/test-full-chain` test binary. It is preserved as user work,
is not staged, and is explicitly absent from `testkernelctf.tar.xz`.

Archive:

- Path: `testkernelctf.tar.xz`
- SHA-256: `b630c93bfbb129dbc1ab1b761039582037449d6bbca134f19d37a53f919f5456`

Excluded from the Direction 2 archive:

- `docs/SPEC.md`, `STATUS.md`, `HANDOFF.md`
- organizer full-chain exploit and harness
- RP2040 firmware/template and USB passthrough scripts
- `System.map`
- real flag, secrets, and credentials

## Changed files

- Participant hint surface: `testkernelctf/README.md`
- Kernel hardening: `kernel-patches/0001-pipe-clear-resized-ring.patch`,
  `scripts/build-target-kernel.sh`, `usb/class/customdriver.c`
- Emulator and organizer stability: `scripts/trusted-usb-emulator.c`,
  `scripts/test-full-chain.c`, `scripts/participant-init.sh`,
  `scripts/build-participant-rootfs.sh`
- Release kernel/metadata/artifact: `testkernelctf/kernel/bzImage`,
  `testkernelctf/kernel/vmlinux`, `testkernelctf/SHA256SUMS`, `testkernelctf.tar.xz`
- Design and state: `docs/SPEC.md`, `STATUS.md`, `HANDOFF.md`, `README.md`

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

Partial-overwrite intended-chain logs: `/tmp/kapo-partial-five-1.log` through
`/tmp/kapo-partial-five-5.log`. Root-only guest check:
`/tmp/kapo-partial-root-only.log`. Old full-entry regression:
`/tmp/kapo-partial-old-full.log`.

## Verification

PASS:

- `scripts/trusted-usb-emulator.c`: static build with
  `-Wall -Wextra -Werror -pthread`
- `scripts/test-full-chain.c`: static build with warnings as errors
- root-owned emulator mode `0700`; UID 65534 read denied; procfs `hidepid=2`
- QEMU Stage 2 520-byte request, 1040-byte receive, 16-byte adjacent overwrite
- post-review QEMU harness regression after restoring the Direction 1 hardware
  branch: `PAGE_CACHE_WRITE_PASS`, UID 0, test flag, `FULL_CHAIN_PASS`
- Linux 6.8.12 `bzImage` rebuild with fixed `kapo@builder`, build version 1,
  and `2026-09-07 00:00:00 UTC`
- two initramfs rebuilds from the recorded inputs were byte-for-byte identical
- the reproducibly rebuilt release candidate passed QEMU 8.2.2, 1 vCPU,
  2 GiB TCG cold boot full chain 5/5; every run reached helper-page leak,
  KASLR leak, page-cache write, UID 0, `KAPO{test_flag}`, and `FULL_CHAIN_PASS`
- prior A/B/C resize shortcut exploits fail at the helper leak on the hardened
  kernel (`/tmp/kapo-hardened-shortcut-{a,b,c}.log`)
- alternate pipe ops reaches the helper and seq leaks but the final ioctl returns
  `EINVAL` (`/tmp/kapo-final2-wrong-ops.log`)
- independent source review found zero new issues: resize is the only live-ring
  free path, normal close clears each entry's `ops`, and the separate FUSE path
  is disabled in the target config
- a second rootfs and release build matched the tracked initramfs and archive
  byte-for-byte
- `nm` reports no symbols for distributed `vmlinux` and helper
- `testkernelctf/SHA256SUMS`: PASS
- initramfs ownership/modes: root-owned flag `0400`, helper `4755`, emulator
  `0700`
- archive listing and internal-material exclusion check: PASS

NOT TESTED / remaining:

- KVM execution path
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

## Hint-reduced black-box results

- Session A: PASS in 1,026 seconds; clean VM 2/2.
- Session B: PASS in 823 seconds; fresh-boot reproduction PASS.
- Session C: PASS in 658 seconds; no blocker.
- All three bypassed the seq KASLR leak by leaking helper page and runtime pipe
  ops together in the helper stage.
- Mean: 836 seconds. Median: 823 seconds. Range: 658--1,026 seconds.
- All three sessions received only the new archive and reported no use of the
  repository, prior exploit, internal source, or validation logs.
- AI-enabled difficulty: approximately 7/10; observed solve time 10--20 minutes.

## Pre-partial full-entry hardened black-box results

- Fresh session C: PASS in 1,190 seconds (19m50s), difficulty 8/10.
- It observed stale `ops = 0` after normal pipe close, then used a helper
  page-only descriptor leak, a separate seq callback leak for KASLR, runtime
  `page_cache_pipe_buf_ops`, Stage 2 overflow, and the SUID helper overwrite.
- No shortcut was reported. Source SHA-256:
  `c708284cd2a850ebccf73f073d04232c9f0d58e2de02c292824176f405ac6a42`.
  Binary SHA-256:
  `07a136bcaf412052d7eb0da4ab70183f86202c00b71d368eea9798682947ee16`.
- Evidence: `/tmp/kapo-final-solve-c/REPORT.md` and
  `/tmp/kapo-final-solve-c/success.log`.
- Adaptation regression A: PASS in 163 seconds with seq leak and exact
  page-cache ops; excluded from fresh solve-time scoring.

## Next action

Obtain one successful independent solve of the final partial archive to satisfy
the current release gate. Then build the private initramfs from the real flag on
the production host, install the launcher and systemd unit, and run one
end-to-end TCP flag acquisition and cleanup smoke test. The current 30-minute
solve sample is 0/3.
