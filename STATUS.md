# Project Status

Last updated: 2026-09-04 KST

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
  1024-byte accounted allocation reuses a freed default pipe ring and discloses
  residual vmemmap `struct page *` values through sysfs in five of five runs
  while enumeration reaches the vendor interface.
- A separate 32-byte `seq_operations` grooming mode discloses the upper seven
  bytes of `proc_single_show` in five of five runs; its known symbol offset
  supplies the overwritten low byte for KASLR-slide calculation.
- The minimal `kapo_ctf` driver binds interface `ff/42/01`; QEMU runtime logs
  confirm `custom_probe()` executes after the Stage 1 short response.
- Stage 2 exposes `/dev/kapo_ctf0`. Its ioctl reads record count 128, allocates
  a 1024-byte pointer table, and receives a 2048-byte record array. QEMU reports
  the intended write as both KASAN slab-out-of-bounds and SLUB redzone damage.
- A redzone-free QEMU trace confirms a freed live-pipe spray slot at
  `ffff8880041fc800` is reused by the Stage 2 allocation. Its 2048-byte receive
  crosses into the still-live ring at `ffff8880041fcc00`.

## In progress

- Stage 2 page-oriented pipe primitive validation.
- Real Linux 6.8 runtime and RP2040 validation of the Stage 1 leak.

## Next

1. Verify physical RP2040 enumeration and `probe()` continue after the short
   configuration response.
2. Replace the fixed Raw Gadget payload with controlled pipe metadata and test
   a bounded page-oriented pipe read/write primitive.

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
- Raw Gadget 25/1024-byte short configuration enumeration: PASS
- Sysfs raw descriptor length (18-byte device + 1024-byte configuration): PASS
- Runtime kmalloc trace showing exact freed pipe-ring address reused by USB: PASS
- Freed kmalloc-1k pipe-ring vmemmap `struct page *` disclosure, five times: PASS
- Freed 32-byte `seq_operations` upper-seven-byte `proc_single_show`
  disclosure, five times: PASS
- Freed pipe-ring `anon_pipe_buf_ops` disclosure: FAIL (`pipe_buf_release()`
  clears `ops`; do not use this grooming lifetime for a text pointer)
- Linux 6.8.12 `customdriver.o` build: PASS
- Final `customdriver.o` rebuild: initial invocation without the temporary
  libelf include/library paths FAIL; corrected invocation PASS
- Raw Gadget interface `ff/42/01` binding and `custom_probe()` execution: PASS
- RP2040 enumeration and hardware leak test: NOT TESTED
- Stage 2 device node, vendor count request, and bulk transfer: PASS
- Stage 2 KASAN 2048-byte write into 1024-byte allocation: PASS
- Stage 2 SLUB right-redzone overwrite: PASS
- Stage 2 exact freed-slot reuse and adjacent live-pipe reach: PASS
- Stage 2 controlled pipe metadata and page primitive: NOT TESTED
