#!/bin/sh
# Builds two commits of the Enxame in git worktrees and alternates their runs, to compare them in one session.
# Runs on the host (git fails inside the container, which mounts only the checkout).
#
#   sh games/enxame/compare/compare.sh <commit-A> <commit-B>
#   sh games/enxame/compare/compare.sh --overhead <commit>
#
# Results go to games/enxame/build/compare-results/<stamp>-<a7>-<b7>/. ENXAME_REPETITIONS (default 10) and ENXAME_STEPS
# exist for a short check of this script: the baseline uses 10 repetitions and the frozen run length.
set -eu

fail() {
  echo "compare: $1" >&2
  exit 1
}

resolve() {
  git rev-parse --verify "$1^{commit}" 2> /dev/null || fail "not a commit: $1"
}

cd "$(git rev-parse --show-toplevel)"

mode=compare

if [ "${1:-}" = "--overhead" ]; then
  mode=overhead
  shift
fi

if [ "$mode" = compare ] && [ "$#" -ne 2 ]; then
  fail "usage: compare.sh <commit-A> <commit-B> | compare.sh --overhead <commit>"
fi

if [ "$mode" = overhead ] && [ "$#" -ne 1 ]; then
  fail "usage: compare.sh <commit-A> <commit-B> | compare.sh --overhead <commit>"
fi

commitA="$(resolve "$1")"
commitB="$commitA"

if [ "$mode" = compare ]; then
  commitB="$(resolve "$2")"

  # A run that changes the workload is not a comparison of the engine.
  git diff --quiet "$commitA" "$commitB" -- games/enxame/native/workloads/ || fail "the workloads differ between $1 and $2: measure $1 with $2's workloads first"
fi

mkdir -p games/enxame/build/compare
work="$(mktemp -d games/enxame/build/compare/run.XXXXXX)"

cleanup() {
  for side in a b; do
    if [ -d "$work/$side" ]; then
      git worktree remove --force "$work/$side" > /dev/null 2>&1 || true
    fi
  done

  rm -rf "$work"
  git worktree prune
}

trap cleanup EXIT INT TERM

stamp="$(date +%Y%m%d-%H%M%S)"
results="games/enxame/build/compare-results/$stamp-$(printf '%s' "$commitA" | cut -c1-7)-$(printf '%s' "$commitB" | cut -c1-7)"
git worktree add --detach "$work/a" "$commitA" > /dev/null

if [ "$mode" = compare ]; then
  git worktree add --detach "$work/b" "$commitB" > /dev/null
  ./spur dev env ENXAME_REPETITIONS="${ENXAME_REPETITIONS:-10}" ENXAME_STEPS="${ENXAME_STEPS:-}" sh games/enxame/compare/inside.sh compare "$work/a" "$work/b" "$commitA" "$commitB" "$results"
else
  ./spur dev env ENXAME_REPETITIONS="${ENXAME_REPETITIONS:-10}" ENXAME_STEPS="${ENXAME_STEPS:-}" sh games/enxame/compare/inside.sh overhead "$work/a" "$commitA" "$results"
fi

echo "results: $results"
