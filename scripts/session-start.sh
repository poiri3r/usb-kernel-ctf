#!/usr/bin/env bash
set -euo pipefail

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"

if [[ -n $(git status --porcelain) ]]; then
	printf '%s\n' 'ERROR: working tree is not clean; refusing to pull.' >&2
	git status --short --branch >&2
	exit 1
fi

git pull --ff-only

printf '\n%s\n' '--- STATUS.md ---'
sed -n '1,240p' STATUS.md
printf '\n%s\n' '--- HANDOFF.md ---'
sed -n '1,240p' HANDOFF.md
