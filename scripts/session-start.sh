#!/usr/bin/env bash
set -euo pipefail

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"

sync_git_bin=$(git config --local --get kapo.syncGitBin || true)
sync_work_tree=$(git config --local --get kapo.syncWorkTree || true)

run_sync_git() {
	if [[ -n $sync_git_bin ]]; then
		"$sync_git_bin" -C "$sync_work_tree" "$@"
	else
		git "$@"
	fi
}

if [[ -n $(git status --porcelain) ]]; then
	printf '%s\n' 'ERROR: working tree is not clean; refusing to pull.' >&2
	git status --short --branch >&2
	exit 1
fi

run_sync_git pull --ff-only

printf '\n%s\n' '--- STATUS.md ---'
sed -n '1,240p' STATUS.md
printf '\n%s\n' '--- HANDOFF.md ---'
sed -n '1,240p' HANDOFF.md
