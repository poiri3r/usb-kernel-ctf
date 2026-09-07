#!/usr/bin/env python3

import argparse
import base64
import importlib.util
import socket
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("jeopardy_qemu_server.py")
SPEC = importlib.util.spec_from_file_location("jeopardy_qemu_server", MODULE_PATH)
SERVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SERVER)


class CommandTest(unittest.TestCase):
    def test_server_qemu_exposes_only_session_directory(self):
        config = argparse.Namespace(
            accel="tcg",
            qemu="qemu-system-x86_64",
            kernel=Path("bzImage"),
            initrd=Path("initramfs.cpio.gz"),
        )
        command = SERVER.qemu_command(config, Path("/tmp/session"))

        self.assertIn("-monitor", command)
        self.assertEqual(command[command.index("-monitor") + 1], "none")
        self.assertIn("-nic", command)
        self.assertEqual(command[command.index("-nic") + 1], "none")
        self.assertIn("-virtfs", command)
        self.assertIn("local,path=/tmp/session,", command[command.index("-virtfs") + 1])
        self.assertNotIn("-share", command)

    def test_upload_rejects_non_elf(self):
        left, right = socket.socketpair()
        self.addCleanup(left.close)
        self.addCleanup(right.close)
        right.sendall(b"bm90IGFuIGVsZg==\n")

        with self.assertRaisesRegex(ValueError, "not an ELF"):
            SERVER.receive_exploit(left)

    def test_upload_rejects_console_input_before_boot(self):
        left, right = socket.socketpair()
        self.addCleanup(left.close)
        self.addCleanup(right.close)
        encoded = base64.b64encode(b"\x7fELFpayload")
        right.sendall(encoded + b"\nid\n")

        with self.assertRaisesRegex(ValueError, "wait for the QEMU console"):
            SERVER.receive_exploit(left)


if __name__ == "__main__":
    unittest.main()
