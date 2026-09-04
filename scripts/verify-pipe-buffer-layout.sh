#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
set -eu

expected_config_sha256=46ef8ef94a8bd7dd2f91525c26e2b286173c25f6818bb2d065d7db58ecf527a0

if [ "$#" -ne 2 ]; then
	echo "usage: $0 <linux-source> <linux-output>" >&2
	exit 1
fi

source=$1
output=$2
assembly=$(mktemp)
trap 'rm -f "$assembly"' EXIT

[ "$(make -s -C "$source" kernelversion)" = "6.8.12" ] || {
	echo "linux source is not 6.8.12" >&2
	exit 1
}
[ "$(awk -F'"' '/UTS_RELEASE/ { print $2 }' \
	"$output/include/generated/utsrelease.h")" = "6.8.12" ] || {
	echo "linux output is not 6.8.12" >&2
	exit 1
}
[ "$(readlink -f "$source")" = "$(readlink -f "$output/source")" ] || {
	echo "linux source and output do not match" >&2
	exit 1
}
config_sha256=$(sha256sum "$output/.config" | awk '{ print $1 }')
[ "$config_sha256" = "$expected_config_sha256" ] || {
	echo "linux output config does not match target" >&2
	exit 1
}

gcc -nostdinc -isystem "$(gcc -print-file-name=include)" \
	-I"$source/arch/x86/include" \
	-I"$output/arch/x86/include/generated" \
	-I"$source/include" -I"$output/include" \
	-I"$source/arch/x86/include/uapi" \
	-I"$output/arch/x86/include/generated/uapi" \
	-I"$source/include/uapi" -I"$output/include/generated/uapi" \
	-include "$source/include/linux/compiler-version.h" \
	-include "$source/include/linux/kconfig.h" \
	-I"$source/ubuntu/include" \
	-include "$source/include/linux/compiler_types.h" \
	-D__KERNEL__ -std=gnu11 -m64 -mno-red-zone -mcmodel=kernel -fno-pie \
	-x c -S -o "$assembly" - <<'EOF'
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/log2.h>
#include <linux/mount.h>
#include <linux/pseudo_fs.h>
#include <linux/magic.h>
#include <linux/pipe_fs_i.h>
const unsigned int pipe_buffer_size = sizeof(struct pipe_buffer);
const unsigned int pipe_buffer_page = __builtin_offsetof(struct pipe_buffer, page);
const unsigned int pipe_buffer_offset = __builtin_offsetof(struct pipe_buffer, offset);
const unsigned int pipe_buffer_len = __builtin_offsetof(struct pipe_buffer, len);
const unsigned int pipe_buffer_ops = __builtin_offsetof(struct pipe_buffer, ops);
const unsigned int pipe_buffer_flags = __builtin_offsetof(struct pipe_buffer, flags);
const unsigned int pipe_buffer_private = __builtin_offsetof(struct pipe_buffer, private);
_Static_assert(sizeof(struct pipe_buffer) == 40, "pipe_buffer size");
_Static_assert(__builtin_offsetof(struct pipe_buffer, page) == 0, "page");
_Static_assert(__builtin_offsetof(struct pipe_buffer, offset) == 8, "offset");
_Static_assert(__builtin_offsetof(struct pipe_buffer, len) == 12, "len");
_Static_assert(__builtin_offsetof(struct pipe_buffer, ops) == 16, "ops");
_Static_assert(__builtin_offsetof(struct pipe_buffer, flags) == 24, "flags");
_Static_assert(__builtin_offsetof(struct pipe_buffer, private) == 32, "private");
EOF

value()
{
	awk -v label="$1" '
		$0 == label ":" { found = 1; next }
		found && $1 == ".long" { print $2; exit }
		found && $1 == ".zero" { print 0; exit }
	' "$assembly"
}

for field in size page offset len ops flags private; do
	value=$(value "pipe_buffer_$field")
	[ -n "$value" ] || {
		echo "failed to derive pipe_buffer_$field" >&2
		exit 1
	}
	printf '%s=%s\n' "$field" "$value"
done
printf 'config_sha256=%s\n' "$config_sha256"
