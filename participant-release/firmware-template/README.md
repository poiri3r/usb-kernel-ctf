# RP2040 Firmware Template

This minimal Pico SDK 2.3.1 project intentionally contains no USB attack logic.
Implement the device behavior required by the challenge, then build with:

```sh
PICO_SDK_PATH=/path/to/pico-sdk ./build.sh
```

Flash `build/kapo_firmware.uf2` in BOOTSEL mode. The target driver matches a
vendor-specific interface with class `0xff`, subclass `0x42`, protocol `0x01`,
and exposes `/dev/kapo_ctf0` after a successful enumeration.
