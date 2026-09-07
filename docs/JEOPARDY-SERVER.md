# Jeopardy server deployment

This internal document describes the server-only flag boundary and the
disposable QEMU TCP service. Do not include it in the participant archive.

## Build the server initramfs

Keep the real flag outside the repository and pass its path as the fifth
argument. The builder accepts exactly one `KAPO{...}` line. Omitting the fifth
argument produces the public `KAPO{test_flag}` image.

```sh
./scripts/build-participant-rootfs.sh \
  /secure/ubuntu-base-24.04.4-base-amd64.tar.gz \
  /secure/busybox \
  /secure/kapo-server-initramfs.cpio.gz \
  /secure/kapo-helper \
  /secure/flag
```

Only `bzImage` and the resulting server initramfs need to be installed below
`/opt/kapo`. Never copy the flag file, participant archive, repository, build
tree, organizer exploit, or credentials into that directory.

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

With GNU `base64` and netcat, connect using:

```sh
{ base64 -w0 ./exploit; printf '\n'; cat; } | nc challenge.example 31337
```

Wait for the shell prompt before typing `/work/exploit` and its arguments.
