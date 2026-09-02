# Handoff

Updated: 2026-09-03 KST

From: Codex on desktop

Branch: `main`
Base commit: `777d259`

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
- No booted-kernel, KASAN, or RP2040 test has been performed.

## Next action

Build and boot a Linux 6.8 kernel containing the modified USB core.  Enable
KASAN and SLUB debugging, groom the configuration buffer's kmalloc cache with a
known-pointer object, and use an RP2040 configuration descriptor whose
`wTotalLength` selects that cache while its second response ends after a valid
vendor interface.  Read `/sys/bus/usb/devices/<device>/descriptors`, confirm the
pointer is useful for a KASLR or direct-map calculation, and confirm the custom
driver's `probe()` is reached.

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
- `docs/SPEC.md`: Stage 1 data flow, validation state, and Stage 2 hypothesis.
- `STATUS.md`, `HANDOFF.md`: progress and reproducible next action.

## Verification commands

- `python3 scripts/test-stage1-leak.py`: PASS
- `git diff --check`: PASS
- Linux 6.8 object build using a temporary hard-linked source tree:
  `make ... drivers/usb/core/config.o drivers/usb/core/message.o`: PASS
- Initial build attempts: FAIL until missing `flex`, `bison`, and libelf
  development files were unpacked into `/tmp`; no system packages were changed.

## Notes

- Treat the Stage 1 kernel-pointer usefulness and Stage 2 primitive as
  hypotheses until reproduced in the booted target kernel.
- Do not commit the real flag, VM images, credentials, or build artifacts.
