# Handoff

Updated: 2026-09-04 KST

From: Codex on desktop

Branch: `main`
Base commit: `c6bfa45`

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
- Stage 2 ioctl asks the device for count 128, allocates a 1024-byte pointer
  table, and receives a 2048-byte record array. KASAN reports a 2048-byte
  slab-out-of-bounds write; a non-KASAN `slub_debug=FZ` boot reports the right
  redzone overwritten.
- In a redzone-free single-CPU boot, a 64-ring live-pipe spray freed address
  `ffff8880041fc800`; the vulnerable allocation reused that exact address. The
  next still-live ring was at `ffff8880041fcc00`, so the 2048-byte receive
  reaches it after crossing the 1024-byte vulnerable object.

## Next action

Make the Raw Gadget Stage 2 payload controllable, reconstruct one adjacent
`pipe_buffer` entry using the Stage 1 KASLR/vmemmap information, and test a
bounded page-oriented pipe operation. Keep final privilege escalation a
hypothesis until the read/write behavior is externally observed and repeatable.

## Changed files

- `usb/core/message.c`: leave configuration backing allocations uncleared.
- `usb/core/config.c`: parse the received length but preserve the advertised
  allocation length for raw descriptor readers.
- `scripts/test-stage1-leak.py`: source contract and poisoned-tail leak model.
- `scripts/raw-gadget-short-config.c`: Raw Gadget short-response trigger with
  kmalloc-1k pipe-ring grooming.
- `usb/class/customdriver.c`, `usb/class/Kconfig`, `usb/class/Makefile`:
  `ff/42/01` driver, device node, ioctl trigger, and intended record-size
  under-allocation.
- `scripts/trigger-stage2.c`: minimal userspace ioctl trigger.
- `scripts/test-stage2-adjacency.c`: live-pipe spray and controlled-hole trace
  harness.
- `docs/SPEC.md`: Stage 1 data flow, validation state, and Stage 2 hypothesis.
- `STATUS.md`, `HANDOFF.md`: progress and reproducible next action.

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
- Stage 2 KASAN slab-out-of-bounds and SLUB redzone reports: PASS
- Stage 2 exact freed-slot reuse and adjacent live-pipe reach: PASS
- Initial build attempts: FAIL until missing `flex`, `bison`, and libelf
  development files were unpacked into `/tmp`; no system packages were changed.

## Notes

- Stage 1 pointer leaks and Stage 2 live-object reach are reproduced in QEMU;
  the controlled page primitive and final exploit remain hypotheses.
- Do not commit the real flag, VM images, credentials, or build artifacts.
