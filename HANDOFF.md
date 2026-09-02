# Handoff

Updated: 2026-09-03 KST

From: notebook

Branch: `main`
Base commit: `41fdd1d`

## Current state

- `docs/SPEC.md` contains the current two-stage challenge design.
- `usb/class/customdriver.c` is intentionally empty.
- Git session scripts are ready for the first desktop handoff.
- No kernel build or RP2040 test has been performed.

## Next action

On desktop, pull `main`, run `./scripts/session-start.sh`, then inspect
`usb/core/config.c` to identify the configuration descriptor fields suitable
for the Stage 1 candidate OOB read.

## Notes

- Treat Stage 1 and Stage 2 primitives as hypotheses until reproduced.
- Do not commit the real flag, VM images, credentials, or build artifacts.
