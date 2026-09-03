# Handoff

Updated: 2026-09-03 KST

From: Codex on desktop

Branch: `main`
Base commit: `e35208f`

## Current state

- Stage 1 uses a short configuration response to disclose the uninitialized
  tail of the advertised-size `kmalloc()` buffer through the standard raw
  descriptor readers.
- The parser receives only the actual transfer length, preserving the intended
  path to interface creation and driver matching.
- `scripts/test-stage1-leak.py` models same-cache reuse with a canonical kernel
  pointer and verifies it appears only in the userspace export, not parser input.
- `usb/class/customdriver.c` is intentionally empty.
- Modified USB core objects compile against the local Linux 6.8 source.
- QEMU Linux 6.8.12 boots with KASAN and `slub_debug=FZ`. A Raw Gadget on
  `dummy_hcd` advertises 1024 configuration bytes, returns 18, and still creates
  vendor interface `ff/42/01`.
- Sysfs exports 1042 bytes (18-byte device descriptor plus 1024-byte
  configuration). Pipe-ring grooming exposes a canonical direct-map
  `struct page *` candidate at configuration offset `0x200` in five of five
  reconnects.
- Closing the groom pipes clears each released buffer's `ops`, so this lifetime
  does not preserve `anon_pipe_buf_ops`. Only the page-pointer candidate is
  runtime-confirmed.
- No RP2040 test has been performed.
- The built-in `kapo_ctf` driver matches `ff/42/01`. QEMU logs
  `KAPO CTF interface reached probe`, and sysfs links the interface to
  `/sys/bus/usb/drivers/kapo_ctf`.

## Next action

Repeat the validated 18/1024-byte response and `probe()` path on RP2040
hardware. In parallel, define the smallest Stage 2 bulk protocol and verify its
allocation size against a live kmalloc-1k pipe ring before adding the intended
under-allocation.

After Stage 1 runtime validation, prototype Stage 2 as a record count/size
under-allocation heap overflow driven by a bulk response: allocate a pointer
table but receive a full record array.  First measure whether the vulnerable
allocation and a live `pipe_buffer` ring array share a practical cache, then
target the adjacent pipe entry's `page`, `offset`, and `len`.  Do not treat the
candidate as confirmed until repeated runs show a stable target and external
trigger.

## Changed files

- `usb/core/message.c`: leave configuration backing allocations uncleared.
- `usb/core/config.c`: parse the received length but preserve the advertised
  allocation length for raw descriptor readers.
- `scripts/test-stage1-leak.py`: source contract and poisoned-tail leak model.
- `scripts/raw-gadget-short-config.c`: Raw Gadget short-response trigger with
  kmalloc-1k pipe-ring grooming.
- `usb/class/customdriver.c`, `usb/class/Kconfig`, `usb/class/Makefile`: minimal
  `ff/42/01` driver registration and observable probe/disconnect skeleton.
- `docs/SPEC.md`: Stage 1 data flow, validation state, and Stage 2 hypothesis.
- `STATUS.md`, `HANDOFF.md`: progress and reproducible next action.

## Verification commands

- `python3 scripts/test-stage1-leak.py`: PASS
- `git diff --check`: PASS
- Linux 6.8 object build using a temporary hard-linked source tree:
  `make ... drivers/usb/core/config.o drivers/usb/core/message.o`: PASS
- Linux 6.8.12 KASAN kernel `bzImage` build and QEMU TCG boot: PASS
- Raw Gadget short configuration enumeration and 1042-byte sysfs read: PASS
- Five repeated groom/reconnect runs with a canonical direct-map page pointer
  at configuration offset `0x200`: PASS
- Built-in `customdriver.o`, interface binding, and `custom_probe()` log: PASS
- Initial build attempts: FAIL until missing `flex`, `bison`, and libelf
  development files were unpacked into `/tmp`; no system packages were changed.

## Notes

- Treat the Stage 1 kernel-pointer usefulness and Stage 2 primitive as
  hypotheses until reproduced in the booted target kernel.
- Do not commit the real flag, VM images, credentials, or build artifacts.
