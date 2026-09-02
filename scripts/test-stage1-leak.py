#!/usr/bin/env python3
"""Host-side regression model for the Stage 1 short-config heap leak."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
ADVERTISED = 64
RECEIVED = 18
KERNEL_POINTER = 0xFFFF_8880_1234_5678


def require_source_contract() -> None:
    message = (ROOT / "usb/core/message.c").read_text()
    config = (ROOT / "usb/core/config.c").read_text()

    required = {
        "configuration descriptor allocations remain uncleared":
            "if (type != USB_DT_CONFIG)" in message,
        "parser receives only transferred bytes":
            "bigbuffer, received);" in config,
        "raw readers retain advertised allocation length":
            "desc.wTotalLength = cpu_to_le16(length);" in config,
    }
    failed = [description for description, ok in required.items() if not ok]
    if failed:
        raise SystemExit("FAIL: " + "; ".join(failed))


def model_short_transfer() -> None:
    # Model a kmalloc allocation reusing a same-size object containing a
    # canonical direct-map pointer.  USB overwrites only RECEIVED bytes.
    allocation = bytearray(b"\xa5" * ADVERTISED)
    pointer_offset = 32
    allocation[pointer_offset:pointer_offset + 8] = struct.pack(
        "<Q", KERNEL_POINTER
    )

    descriptor = bytearray(RECEIVED)
    descriptor[0:9] = bytes([
        9, 2, ADVERTISED, 0, 1, 1, 0, 0x80, 50,
    ])
    descriptor[9:18] = bytes([
        9, 4, 0, 0, 0, 0xff, 0x42, 1, 0,
    ])
    allocation[:RECEIVED] = descriptor

    parser_view = allocation[:RECEIVED]
    userspace_view = allocation[:ADVERTISED]

    assert len(parser_view) == RECEIVED
    assert parser_view[9] == 9 and parser_view[10] == 4
    assert struct.pack("<Q", KERNEL_POINTER) not in parser_view
    assert struct.pack("<Q", KERNEL_POINTER) in userspace_view


if __name__ == "__main__":
    require_source_contract()
    model_short_transfer()
    print("PASS: parser uses 18 received bytes and enumeration model remains valid")
    print("PASS: 64-byte descriptor export contains a groomed canonical heap pointer")
    print("NOTE: host model only; kernel build, KASAN, and RP2040 tests remain required")
