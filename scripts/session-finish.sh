#!/usr/bin/env bash
set -euo pipefail

if (( $# < 2 )); then
	printf 'Usage: %s "commit message" <file> [file ...]\n' "$0" >&2
	exit 2
fi

commit_message=$1
shift

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

git symbolic-ref --quiet HEAD >/dev/null || {
	printf '%s\n' 'ERROR: detached HEAD; refusing to commit.' >&2
	exit 1
}

git diff --check
git add -- "$@"
git diff --cached --check

if git diff --cached --quiet; then
	printf '%s\n' 'ERROR: selected files contain no changes.' >&2
	exit 1
fi

if ! git diff --quiet || [[ -n $(git ls-files --others --exclude-standard) ]]; then
	printf '%s\n' 'ERROR: unselected changes remain; refusing to hand off.' >&2
	git status --short >&2
	exit 1
fi

git diff --cached --stat
git commit -m "$commit_message"
run_sync_git push
