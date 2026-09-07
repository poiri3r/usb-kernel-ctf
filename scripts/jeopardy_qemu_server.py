#!/usr/bin/env python3

import argparse
import base64
import binascii
import os
import signal
import shutil
import socket
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


class QemuServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

    def __init__(self, address, handler, config):
        super().__init__(address, handler)
        self.config = config
        self.slots = threading.BoundedSemaphore(config.max_sessions)
        self.stopping = threading.Event()
        self.processes = set()
        self.process_lock = threading.Lock()

    def register_process(self, process):
        with self.process_lock:
            self.processes.add(process)

    def unregister_process(self, process):
        with self.process_lock:
            self.processes.discard(process)

    def stop_processes(self):
        with self.process_lock:
            processes = tuple(self.processes)
        for process in processes:
            stop_process(process)


class QemuHandler(socketserver.BaseRequestHandler):
    def handle(self):
        if not self.server.slots.acquire(blocking=False):
            self.request.sendall(b"Server busy. Try again later.\n")
            return
        try:
            self.run_qemu()
        finally:
            self.server.slots.release()

    def run_qemu(self):
        config = self.server.config
        self.request.sendall(
            b"KAPO kernel pwn instance\n"
            b"Send one line containing a base64 ELF (maximum 4 MiB).\n"
        )
        try:
            exploit = receive_exploit(self.request)
        except (ValueError, socket.timeout) as error:
            safe_send(self.request, f"Upload rejected: {error}\n".encode())
            return

        process = None
        disconnected = threading.Event()
        with tempfile.TemporaryDirectory(prefix="kapo-session-") as work:
            Path(work).chmod(0o755)
            exploit_path = Path(work) / "exploit"
            exploit_path.write_bytes(exploit)
            exploit_path.chmod(0o755)
            try:
                process = subprocess.Popen(
                    qemu_command(config, Path(work)),
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )
                self.server.register_process(process)
                self.request.settimeout(None)
                safe_send(self.request, b"Exploit staged as /work/exploit. Booting QEMU...\n\n")
                output = threading.Thread(
                    target=copy_output,
                    args=(process.stdout, self.request, disconnected),
                    daemon=True,
                )
                input_thread = threading.Thread(
                    target=copy_input,
                    args=(self.request, process.stdin, disconnected),
                    daemon=True,
                )
                output.start()
                input_thread.start()

                deadline = time.monotonic() + config.timeout
                while (process.poll() is None and not disconnected.is_set()
                       and not self.server.stopping.is_set()):
                    if time.monotonic() >= deadline:
                        safe_send(self.request, b"\nSession timed out.\n")
                        break
                    time.sleep(0.1)
            finally:
                if process is not None:
                    stop_process(process)
                    self.server.unregister_process(process)


def receive_exploit(client):
    limit = (4 * 1024 * 1024 + 2) // 3 * 4
    client.settimeout(60)
    encoded = bytearray()
    while b"\n" not in encoded:
        chunk = client.recv(65536)
        if not chunk:
            raise ValueError("connection closed before upload")
        encoded.extend(chunk)
        if len(encoded) > limit + 1:
            raise ValueError("encoded payload is too large")
    line, pending = bytes(encoded).split(b"\n", 1)
    if pending:
        raise ValueError("wait for the QEMU console before sending input")
    try:
        exploit = base64.b64decode(line, validate=True)
    except binascii.Error as error:
        raise ValueError("invalid base64") from error
    if not exploit.startswith(b"\x7fELF"):
        raise ValueError("payload is not an ELF")
    if len(exploit) > 4 * 1024 * 1024:
        raise ValueError("payload is too large")
    return exploit


def copy_input(client, qemu_stdin, disconnected):
    try:
        while data := client.recv(65536):
            qemu_stdin.write(data)
            qemu_stdin.flush()
    except (BrokenPipeError, ConnectionError, OSError):
        pass
    finally:
        disconnected.set()


def copy_output(qemu_stdout, client, disconnected):
    try:
        while data := os.read(qemu_stdout.fileno(), 65536):
            client.sendall(data)
    except (BrokenPipeError, ConnectionError, OSError):
        pass
    finally:
        disconnected.set()


def safe_send(client, data):
    try:
        client.sendall(data)
    except OSError:
        pass


def stop_process(process):
    if process.poll() is not None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()


def qemu_command(config, work):
    if config.accel == "auto":
        kvm = os.access("/dev/kvm", os.R_OK | os.W_OK)
        accel, cpu = ("kvm", "host") if kvm else ("tcg", "max")
    else:
        accel = config.accel
        cpu = "host" if accel == "kvm" else "max"
    return [
        config.qemu,
        "-accel", accel,
        "-cpu", cpu,
        "-machine", "q35",
        "-smp", "1",
        "-m", "2G",
        "-kernel", str(config.kernel),
        "-initrd", str(config.initrd),
        "-append", "console=ttyS0 panic=-1",
        "-virtfs", f"local,path={work},mount_tag=hostshare,security_model=mapped-xattr,id=hostshare",
        "-monitor", "none",
        "-nic", "none",
        "-nographic",
        "-no-reboot",
    ]


def parse_args():
    project = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(
        description="Expose one disposable KAPO QEMU serial console per TCP connection."
    )
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=31337)
    parser.add_argument("--kernel", type=Path,
                        default=project / "testkernelctf/kernel/bzImage")
    parser.add_argument("--initrd", type=Path,
                        default=project / "testkernelctf/rootfs/initramfs.cpio.gz")
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--accel", choices=("auto", "kvm", "tcg"), default="auto")
    parser.add_argument("--max-sessions", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=900)
    args = parser.parse_args()
    if args.max_sessions < 1 or args.timeout < 1 or not 0 < args.port < 65536:
        parser.error("port, max-sessions, and timeout must be positive")
    for path in (args.kernel, args.initrd):
        if not path.is_file():
            parser.error(f"missing file: {path}")
    if shutil.which(args.qemu) is None:
        parser.error(f"QEMU executable not found: {args.qemu}")
    return args


def main():
    if os.geteuid() == 0:
        sys.exit("refusing to run QEMU service as root")
    config = parse_args()
    with QemuServer((config.bind, config.port), QemuHandler, config) as server:
        previous_sigterm = signal.getsignal(signal.SIGTERM)

        def terminate(_signum, _frame):
            server.stopping.set()
            server.stop_processes()
            raise KeyboardInterrupt

        signal.signal(signal.SIGTERM, terminate)
        print(f"listening on {config.bind}:{config.port}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.stopping.set()
            server.stop_processes()
            signal.signal(signal.SIGTERM, previous_sigterm)


if __name__ == "__main__":
    main()
