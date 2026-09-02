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

## In progress

- Stage 2 custom driver primitive design.
- Real Linux 6.8 runtime and RP2040 validation of the Stage 1 leak.

## Next

1. Boot the modified Linux 6.8 kernel with KASAN and SLUB debugging.
2. Reuse the advertised `wTotalLength` kmalloc cache with an object containing
   a known kernel pointer, then verify it through sysfs `descriptors`.
3. Verify physical RP2040 enumeration and `probe()` continue after the short
   configuration response.
4. Prototype the Stage 2 record-array under-allocation in `customdriver.c` only
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
- Linux 6.8 runtime/KASAN leak test: NOT TESTED
- RP2040 enumeration and hardware leak test: NOT TESTED
