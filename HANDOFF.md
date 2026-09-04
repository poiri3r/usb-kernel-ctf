# Handoff

Updated: 2026-09-05 KST

From: Codex on desktop

Branch: `main`
Base commit: `fab22ad`

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
- The selected final escalation design is a Dirty Pipe-style page-cache write
  against a dedicated root-owned SUID helper. Reconstruct the adjacent live
  `pipe_buffer` with a leaked helper-page `struct page *`, the KASLR-adjusted
  `page_cache_pipe_buf_ops`, and `PIPE_BUF_FLAG_CAN_MERGE`. This design is
  selected but NOT TESTED.
- The planned participant build keeps `kapo_ctf` built into the kernel. The
  public bundle should contain the exact `bzImage`, symbolized non-DWARF
  `vmlinux`, `System.map`, kernel config, Ubuntu 24.04 rootfs, helper copy,
  QEMU launcher, RP2040 firmware template, and hashes. Do not publish a
  standalone `customdriver.ko`.

## Next action

Create the dedicated SUID helper and derive a file-backed page pointer leak.
Use the existing payload generator only after validating the helper page
reference and live entry lifetime in target-equivalent QEMU. Do not claim the
page-cache write or privilege escalation as confirmed until runtime tested.

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
- SUID-helper page leak, controlled page-cache write, UID 0, test flag, and dual
  configuration enumeration: NOT TESTED
- Participant release bundle: NOT BUILT
- Initial build attempts: FAIL until missing `flex`, `bison`, and libelf
  development files were unpacked into `/tmp`; no system packages were changed.

## Notes

- Stage 1 pointer leaks and Stage 2 live-object reach are reproduced in QEMU;
  the SUID-helper page-cache primitive and final exploit remain hypotheses.
- A dual-configuration device (32-byte seq leak plus 1024-byte helper-page leak)
  may remove an extra reconnect, but this is an Unverified optimization. Keep
  the existing separate modes until it passes QEMU and RP2040 tests.
- Do not commit the real flag, VM images, credentials, or build artifacts.
