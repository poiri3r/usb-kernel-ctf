# Project Status

Last updated: 2026-09-03 KST

## Done

- Linux 6.8 USB driver source baseline imported under `usb/`.
- Initial challenge design recorded in `docs/SPEC.md`.
- Desktop-notebook Git handoff workflow and safe session scripts added.
- Session scripts support a repository-local GitHub Desktop Git executable for authenticated sync.
- Stage 1 short-configuration heap disclosure implemented in
  `usb/core/message.c` and `usb/core/config.c`.
- A host regression model verifies that parsing remains bounded by the received
  length while the raw descriptor export contains a groomed canonical pointer.
- A QEMU Linux 6.8.12 guest with KASAN, SLUB debugging, `dummy_hcd`, and Raw
  Gadget reproduces the short configuration disclosure. The advertised
  1024-byte allocation reuses a freed default pipe ring and discloses its
  residual `struct page *` through sysfs in five of five runs while enumeration
  reaches the vendor interface.
- The minimal `kapo_ctf` driver binds interface `ff/42/01`; QEMU runtime logs
  confirm `custom_probe()` executes after the Stage 1 short response.

## In progress

- Stage 2 custom driver primitive design.
- Real Linux 6.8 runtime and RP2040 validation of the Stage 1 leak.

## Next

1. Verify physical RP2040 enumeration and `probe()` continue after the short
   configuration response.
2. Prototype the Stage 2 record-array under-allocation in `customdriver.c` only
   after choosing a repeatable target-object lifetime and cache.

## Verification

- Session script syntax: PASS
- Dirty-tree pull protection: PASS
- GitHub Desktop authenticated push: PASS
- Stage 1 host poisoned-allocation model: PASS
- Linux 6.8 `drivers/usb/core/config.o` and `message.o` build: PASS
- Stage 1 regression model repeated five times: PASS
- Changed-diff checkpatch: 0 errors, 0 warnings; one expected camel-case check
  for the USB-standard `wTotalLength` field
- First Linux 6.8 build attempt without `flex`: FAIL (missing build tool)
- Second build setup attempt without usable libelf development files: FAIL
- QEMU Linux 6.8.12 boot with KASAN and `slub_debug=FZ`: PASS
- Raw Gadget 18/1024-byte short configuration enumeration: PASS
- Sysfs raw descriptor length (18-byte device + 1024-byte configuration): PASS
- Freed kmalloc-1k pipe-ring `struct page *` disclosure, repeated five times: PASS
- Freed pipe-ring `anon_pipe_buf_ops` disclosure: FAIL (`pipe_buf_release()`
  clears `ops`; do not use this grooming lifetime for a text pointer)
- Linux 6.8.12 `customdriver.o` build: PASS
- Raw Gadget interface `ff/42/01` binding and `custom_probe()` execution: PASS
- RP2040 enumeration and hardware leak test: NOT TESTED
