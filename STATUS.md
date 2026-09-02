# Project Status

Last updated: 2026-09-03 KST

## Done

- Linux 6.8 USB driver source baseline imported under `usb/`.
- Initial challenge design recorded in `docs/SPEC.md`.
- Desktop-notebook Git handoff workflow and safe session scripts added.

## In progress

- Stage 1 USB configuration parser primitive design.
- Stage 2 custom driver primitive design.

## Next

1. Identify the exact Stage 1 descriptor field and parser path.
2. Verify enumeration continues after the Stage 1 trigger.
3. Implement the minimum `custom_probe()` skeleton and matching condition.

## Verification

- Session script syntax: PASS
- Dirty-tree pull protection: PASS
- Kernel build and hardware tests: NOT TESTED
