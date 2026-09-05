# Handoff

This document records the current checkpoint and verification limits; read it
before resuming work or moving to another machine.

Updated: 2026-09-05 KST

From: Codex on desktop

Branch: `main`
Checkpoint base commit: `50c2d35`

## Session authorization

- On 2026-09-05 the user explicitly authorized repository-local file changes,
  builds, tests, Git add/commit, remote inspection, network operations required
  for this project, and pushes to the configured existing `origin` remote.
- The authorization includes `scripts/session-finish.sh` and its commit/push
  actions. It excludes unrelated file deletion and system-wide changes.
- After the interrupted finish attempt, `git status`, `git remote -v`, recent
  commits, reflog, index lock, and staged/unstaged diffs were checked. HEAD and
  local `origin/main` remained `50c2d35`, no index lock or Git process remained,
  and no duplicate commit had been created.

## Previous progress-save checkpoint

- Scope: save the existing progress without changing implementation or design.
- Local `origin/main` equals HEAD. No remote query, pull, commit, or push was
  performed; server freshness and cross-machine handoff are not confirmed.
- Six files were already staged at entry: `HANDOFF.md`, `STATUS.md`,
  `docs/SPEC.md`, `helpers/kapo-helper.c`, `scripts/raw-gadget-short-config.c`,
  and `scripts/test-helper-page-lifetime.c`. Preserve these existing changes.
- This save adds only checkpoint text to `STATUS.md` and `HANDOFF.md` in the
  working tree; the pre-existing index is preserved.
- The verification results below are historical records. Builds and runtime
  tests were not rerun, and original runtime logs were not revalidated during
  this save. `git diff --check` and `git diff --cached --check`: PASS.
- First resume action: run `git status --short --branch`, then establish
  availability of the historical logs and build environment before relying on
  previous verification results.
- `README.md` marks firmware upload complete, while `STATUS.md` records RP2040
  hardware testing as NOT TESTED. Upload artifacts and the scope of that
  checkbox remain unverified; do not infer hardware validation from it.
- The Changed files and Verification commands sections below retain cumulative
  history; the six-file list above identifies the pending changes at this save.

## Current state

- Stage 1 uses a short configuration response to disclose the uninitialized
  tail of the advertised-size `kmalloc()` buffer through the standard raw
  descriptor readers.
- The parser receives only the actual transfer length, preserving the intended
  path to interface creation and driver matching.
- `scripts/test-stage1-leak.py` models same-cache reuse with a canonical kernel
  pointer and verifies it appears only in the userspace export, not parser input.
- `usb/class/customdriver.c` contains the Stage 2 record-size under-allocation,
  a userspace device node, and an ioctl trigger.
- Modified USB core objects compile against the local Linux 6.8 source.
- QEMU Linux 6.8.12 boots with KASAN and `slub_debug=FZ`. A Raw Gadget on
  `dummy_hcd` advertises 1024 configuration bytes, returns 25, and still creates
  vendor interface `ff/42/01`.
- Sysfs exports 1042 bytes (18-byte device descriptor plus 1024-byte
  configuration). Runtime kmalloc tracing proves the USB buffer reuses the exact
  address of a freed 640-byte pipe ring in kmalloc-1k. Filled pipe entries expose
  canonical vmemmap `struct page *` values from configuration offset `0x28` in
  five of five reconnects.
- A 32-byte configuration and `/proc/self/stat` grooming expose the upper seven
  bytes of `proc_single_show` at configuration offset `0x19` in five of five
  reconnects. The known symbol offset restores the endpoint-overwritten low
  byte and provides a KASLR slide.
- Closing the groom pipes clears each released buffer's `ops`, so this lifetime
  does not preserve `anon_pipe_buf_ops`. Only the page-pointer candidate is
  runtime-confirmed.
- No RP2040 test has been performed.
- The built-in `kapo_ctf` driver matches `ff/42/01`. QEMU logs
  `KAPO CTF interface ready at minor 192`, creates `/dev/kapo_ctf0`, and binds
  the interface to `/sys/bus/usb/drivers/kapo_ctf`.
- Stage 2 ioctl asks the device for count 128, allocates a 1024-byte buffer by
  using the record pointer size instead of the record element size, and receives
  a 2048-byte record array. The source now presents this as an ordinary
  single-pointer `sizeof` mistake and no longer labels the intentional bug.
  KASAN previously reported a 2048-byte slab-out-of-bounds write; a non-KASAN
  `slub_debug=FZ` boot previously reported the right redzone overwritten.
- `scripts/raw-gadget-short-config.c` keeps its no-argument pipe mode and `seq`
  mode with the original `A` response. An optional payload file can now be given
  as `[seq] [payload-file]`; it must contain exactly 2048 bytes and is loaded
  before Raw Gadget setup. Both modes returned 2048 from `trigger-stage2` in the
  rebuilt Linux 6.8.12 KASAN QEMU runtime.
- In a redzone-free single-CPU boot, a 64-ring live-pipe spray freed address
  `ffff8880041fc800`; the vulnerable allocation reused that exact address. The
  next still-live ring was at `ffff8880041fcc00`, so the 2048-byte receive
  reaches it after crossing the 1024-byte vulnerable object.
- `scripts/verify-pipe-buffer-layout.sh` compiles the exact Linux 6.8.12 source
  headers with the target-equivalent build's generated headers. It requires
  source/output version 6.8.12, a matching output `source` link, and the output
  `.config` SHA-256. The required config hash is
  `46ef8ef94a8bd7dd2f91525c26e2b286173c25f6818bb2d065d7db58ecf527a0`.
  It derived a 40-byte `struct pipe_buffer` with `page=0`, `offset=8`, `len=12`,
  `ops=16`, `flags=24`, and `private=32` byte offsets.
- `scripts/make-stage2-pipe-payload.c` accepts a new output path plus runtime
  `page`, `ops`, and `offset` values. It writes exactly 2048 bytes: a zeroed
  1024-byte vulnerable allocation prefix, one reconstructed entry at byte 1024
  (`len=1`, `PIPE_BUF_FLAG_CAN_MERGE`, `private=0`), and a zeroed tail. It
  self-checks every preserved region and serialized field before writing. It
  rejects negative addresses and offsets above 4094 so `offset + len` stays
  within one page. On write/close failure it removes only the output path it
  created. No dynamic address or payload binary is tracked.
- `helpers/kapo-helper.c` is a minimal dedicated helper that prints a fixed
  message and accepts no input. It is made root-owned mode `4755` only in a
  temporary QEMU initramfs; no helper binary or rootfs is tracked.
- `scripts/raw-gadget-short-config.c helper <file>` fills pipe entry 0 with an
  anonymous padding page and entry 1 with one byte spliced from the helper's
  executable page at file offset `0x1000`. It releases 256 rings in reverse
  order before Raw Gadget initialization. A static non-PIE helper has `main`
  in this executable `LOAD` segment. In a KASLR-enabled, redzone-free Linux
  6.8.12 QEMU boot, the raw descriptor's configuration offset `0x28` contained
  a canonical vmemmap candidate on five of five cold boots. All five preserved
  logs contain the same value, `ffffea0001f6ce00`; the logs do not independently
  prove that it identifies the helper executable page.
- `scripts/test-helper-page-lifetime.c` splices an executable-page byte into a
  live pipe, closes the source descriptor, waits 10 seconds, and verifies the
  same byte can still be read. The target-equivalent QEMU test passed. This
  establishes only the observable live-pipe lifetime/reference behavior, not a
  reconstructed entry or write primitive.
- The selected final escalation design is a Dirty Pipe-style page-cache write
  against a dedicated root-owned SUID helper. Reconstruct the adjacent live
  `pipe_buffer` with a leaked helper-page `struct page *`, the KASLR-adjusted
  `page_cache_pipe_buf_ops`, and `PIPE_BUF_FLAG_CAN_MERGE`. This design is
  selected. The live-pipe reference is PASS. The helper page identity is
  UNVERIFIED, and the controlled page-cache write and privilege escalation
  remain NOT TESTED.
- The planned participant build keeps `kapo_ctf` built into the kernel. The
  public bundle should contain the exact `bzImage`, symbolized non-DWARF
  `vmlinux`, `System.map`, kernel config, Ubuntu 24.04 rootfs, helper copy,
  QEMU launcher, RP2040 firmware template, and hashes. Do not publish a
  standalone `customdriver.ko`.

## Next action

Add a host-to-device control transfer carrying the Stage 1 `page`, KASLR-derived
`page_cache_pipe_buf_ops`, and patch offset. Place a one-entry helper-backed live
pipe immediately after the freed kmalloc-1k slot, then validate page identity,
the reconstructed entry, and the page-cache write in target-equivalent QEMU.
Do not claim page-cache write or privilege escalation as confirmed until runtime
tested.

## Changed files

- `scripts/raw-gadget-short-config.c`: accept an optional exact-size Stage 2
  payload file and require a full 2048-byte EP write while preserving
  no-argument and `seq` behavior.
- `docs/SPEC.md`: record the post-concealment QEMU runtime PASS while preserving
  the page-cache primitive's NOT TESTED boundary.
- `STATUS.md`: record payload validation and post-concealment QEMU regression.
- `HANDOFF.md`: record this task and the next reproducible action.
- `scripts/verify-pipe-buffer-layout.sh`: compile target headers to derive the
  pipe buffer layout from the exact target source and output tree.
- `scripts/make-stage2-pipe-payload.c`: generate and self-check the exact-size
  Stage 2 pipe-entry payload without tracking dynamic addresses or binaries.
- `helpers/kapo-helper.c`: source-only fixed-output helper for the temporary
  root-owned SUID test artifact.
- `scripts/test-helper-page-lifetime.c`: observable file-page live-pipe
  reference retention test.

## Verification commands

- `python3 scripts/test-stage1-leak.py`: PASS
- `git diff --check`: PASS
- Linux 6.8 object build using a temporary hard-linked source tree:
  `make ... drivers/usb/core/config.o drivers/usb/core/message.o`: PASS
- Linux 6.8.12 KASAN kernel `bzImage` build and QEMU TCG boot: PASS
- Raw Gadget short configuration enumeration and 1042-byte sysfs read: PASS
- Runtime kmalloc trace of exact pipe-ring/USB-buffer address reuse: PASS
- Five repeated pipe groom runs with vmemmap page pointers from configuration
  offset `0x28`: PASS
- Five repeated seq_operations groom runs with upper seven bytes of
  `proc_single_show` at configuration offset `0x19`: PASS
- Built-in `customdriver.o`, interface binding, and `custom_probe()` log: PASS
- Final `customdriver.o` rebuild with temporary libelf include/library paths:
  PASS (the immediately preceding invocation without those paths failed)
- Post-concealment `customdriver.o` rebuild: PASS. Disassembly shows allocation
  length `count << 3` and bulk receive length `count << 4`.
- Dynamic and static builds of `raw-gadget-short-config.c` with
  `-Wall -Wextra -Werror`: PASS.
- Host CLI tests for no-argument mode, `seq`, exact payload in both modes,
  2047-byte and 2049-byte inputs, missing files, directory read errors, and
  extra arguments: PASS. The exact-size cases reached `/dev/raw-gadget`; the
  host has no Raw Gadget device node.
- The first incremental KASAN `bzImage` command without the temporary libelf
  paths failed. Re-running with the preserved include/library paths: PASS.
- Post-concealment Linux 6.8.12 KASAN QEMU Stage 2 runtime: PASS. The rebuilt
  kernel bound `ff/42/01` to `kapo_ctf`; file payloads in default and `seq` modes
  both made `trigger-stage2` print `received 2048 bytes`. KASAN also reproduced
  the 2048-byte write into the 1024-byte kmalloc-1k allocation.
- Stage 2 KASAN slab-out-of-bounds and SLUB redzone reports: PASS
- Stage 2 exact freed-slot reuse and adjacent live-pipe reach: PASS
- `scripts/verify-pipe-buffer-layout.sh /tmp/usb-kernel-ctf-src.IavzRx
  /tmp/usb-kernel-runtime-nokasan-out`: PASS. The target is Linux 6.8.12 and
  the derived layout is size 40 with offsets 0/8/12/16/24/32. The source link
  matched and config SHA-256 was
  `46ef8ef94a8bd7dd2f91525c26e2b286173c25f6818bb2d065d7db58ecf527a0`.
- Dynamic and static `-Wall -Wextra -Werror` builds of
  `make-stage2-pipe-payload.c`: PASS. A generated payload was verified for its
  2048-byte size, zeroed 1024-byte prefix/tail, and little-endian field values.
  Zero/negative page/ops, offset 4095 or greater, and nonnumeric input were
  rejected.
- The generated file was accepted by `raw-gadget-short-config` in Linux 6.8.12
  KASAN QEMU. `trigger-stage2` returned 2048 and KASAN reported the expected
  allocation-boundary write. This proves file delivery only; the live entry
  lifetime and page-cache write remain NOT TESTED.
- Target-equivalent Linux 6.8.12 helper-page leak candidate: UNVERIFIED. Build `kapo-helper`,
  `test-helper-page-lifetime`, and `raw-gadget-short-config` statically with
  `-Wall -Wextra -Werror`; create a temporary initramfs under `fakeroot` with
  `/kapo-helper` owner `0:0` and mode `4755`; build the helper static non-PIE;
  boot with 1 vCPU, 2 GiB, KASLR enabled, no KASAN and no `slub_debug`. Run
  `short-config helper /kapo-helper`, then read eight bytes at sysfs descriptor
  offset 58 (configuration offset `0x28`). A canonical vmemmap candidate was
  nonzero in five of five cold boots, but all preserved logs have the same value
  and do not independently establish the candidate's page identity.
- Target-equivalent live helper pipe reference: PASS. The source descriptor was
  closed after `splice()` and the held pipe returned its original executable-page byte
  after 10 seconds.
- Controlled page-cache write, UID 0, test flag, and dual configuration
  enumeration: NOT TESTED
- Participant release bundle: NOT BUILT
- Initial build attempts: FAIL until missing `flex`, `bison`, and libelf
  development files were unpacked into `/tmp`; no system packages were changed.

## Notes

- Stage 1 pointer candidates and Stage 2 live-object reach are reproduced in
  QEMU; helper page identity, the SUID-helper page-cache primitive, and the final
  exploit remain hypotheses.
- The current protocol has no guest-to-device path for runtime `page` and `ops`
  values. The preloaded file-payload test therefore does not model the complete
  KASLR-aware chain.
- A dual-configuration device (32-byte seq leak plus 1024-byte helper-page leak)
  may remove an extra reconnect, but this is an Unverified optimization. Keep
  the existing separate modes until it passes QEMU and RP2040 tests.
- Do not commit the real flag, VM images, credentials, or build artifacts.
