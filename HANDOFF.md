# Handoff

Updated: 2026-09-05 KST

From: Codex on desktop

Branch: `main`
Base commit: `90ff347`

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
- In a redzone-free single-CPU boot, a 64-ring live-pipe spray freed address
  `ffff8880041fc800`; the vulnerable allocation reused that exact address. The
  next still-live ring was at `ffff8880041fcc00`, so the 2048-byte receive
  reaches it after crossing the 1024-byte vulnerable object.
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

Change `scripts/raw-gadget-short-config.c` so its Stage 2 bulk response can read
exactly 2048 payload bytes from a file named on the command line instead of
always filling the response with `A`. Preserve the current default mode and
verify that `scripts/trigger-stage2.c` still returns 2048. Do not start the SUID
helper until this controllable-payload step passes.

## Changed files

- `usb/class/customdriver.c`: hide the Stage 2 source-level giveaway behind a
  single-pointer `sizeof` mistake while preserving the x86_64 1:2 size mismatch.
- `docs/SPEC.md`: record the concealed Stage 2 expression, selected SUID-helper
  page-cache design, distribution plan, and current validation boundary.
- `STATUS.md`: update progress, validation results, and remaining work.
- `HANDOFF.md`: record this task and the next reproducible action.

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
- Post-concealment QEMU Stage 2 runtime regression: NOT TESTED.
- Stage 2 KASAN slab-out-of-bounds and SLUB redzone reports: PASS
- Stage 2 exact freed-slot reuse and adjacent live-pipe reach: PASS
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
