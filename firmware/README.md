# RP2040 Firmware

This firmware uses the RP2040 USB controller directly so configuration
descriptors can advertise 1024 or 32 bytes while returning only the 25-byte
configuration, interface, and endpoint prefix.

It starts in helper-page leak mode and advances through seq leak mode to Stage 2
when the guest sends vendor request `0x5c`. Each transition disconnects USB for
two seconds before reconnecting.

Build with Pico SDK 2.3.1:

```sh
PICO_SDK_PATH=/path/to/pico-sdk ./firmware/build.sh
```

Flash `build/kapo_rp2040.uf2` to an RP2040 Zero in BOOTSEL mode. Start the guest
hardware harness before connecting or resetting the board so helper heap
grooming runs before the first enumeration.
