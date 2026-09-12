# Jeopardy server deployment

This internal document describes the server-only flag boundary and the
disposable QEMU TCP service. Do not include it in the participant archive.

## Repository layout

The deployment checkout must preserve these repository-relative paths:

- `kernel/bzImage`: server kernel
- `server/initramfs.cpio.gz`: private server image containing the real flag
- `scripts/jeopardy_qemu_server.py`: TCP/QEMU launcher
- `deploy/kapo-jeopardy.service`: systemd unit template

The committed unit expects the checkout at `/opt/kapo`. A merge deployment may
restart the service directly from that checkout; the launcher defaults resolve
to the same `kernel/` and `server/` paths. `participant/` is packaging input and
is not read by the running service.

## Build the server initramfs

Keep the raw flag file outside the repository and pass its path as the fourth
argument. The builder accepts exactly one `kapo{...}` line. The resulting
`server/initramfs.cpio.gz` may be committed only to the private server
repository. Omitting the fourth argument produces the public
`kapo{test_flag}` image.

```sh
./scripts/build-participant-rootfs.sh \
  /secure/ubuntu-base-24.04.4-base-amd64.tar.gz \
  /secure/busybox \
  server/initramfs.cpio.gz \
  /secure/flag
```

Never copy the raw flag file, organizer exploit, build tree, or credentials
into the deployment checkout. The compressed server initramfs is the only
committed file that contains the real flag.

## Install the service

Install the launcher, kernel, server initramfs, and unit at the paths used by
`deploy/kapo-jeopardy.service`, then run:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now kapo-jeopardy.service
```

The unit uses a dynamic unprivileged account, a private temporary directory,
read-only host filesystem view, no host devices, no Linux capabilities, and
resource limits. It intentionally uses TCG; enable KVM only after defining a
similarly restricted device policy.

## Participant connection

The TCP endpoint first accepts one base64-encoded ELF line. After QEMU boots,
the same connection becomes the UID 65534 serial console and the ELF is
available as `/work/exploit`. Each connection has its own temporary directory
and QEMU process. Disconnecting, timing out, or stopping the service terminates
that QEMU and removes the directory.

The launcher keeps an explicit e1000 device so the local and remote PCI layouts
match. Its QEMU user-network backend uses `restrict=on`, which blocks
guest-initiated external networking; do not remove or replace these arguments
without rerunning the Stage 2 cold-boot stability test.

With GNU `base64` and netcat, connect using:

```sh
{ base64 -w0 ./exploit; printf '\n'; cat; } | nc challenge.example 13001
```

Wait for the shell prompt before typing `/work/exploit` and its arguments.
