#!/bin/sh
# Runs in the dev container, where the checkout is mounted at /app. compare.sh starts it; it never runs on the host.
#
#   inside.sh compare <dir-a> <dir-b> <commit-a> <commit-b> <results>
#   inside.sh overhead <dir> <commit> <results>
#
# <dir-*> are checkouts of the two commits and <results> a folder, all relative to /app. The environment holds
# ENXAME_REPETITIONS (default 10) and ENXAME_STEPS (empty: the frozen run length).
set -eu

repetitions="${ENXAME_REPETITIONS:-10}"
warmUpFolder="${TMPDIR:-/tmp}"
stepArguments=""

if [ -n "${ENXAME_STEPS:-}" ]; then
  stepArguments="--steps $ENXAME_STEPS"
fi

fail() {
  echo "compare: $1" >&2
  exit 1
}

build() {
  (cd "/app/$1/games/enxame" && OLY_ENGINE_DIR="/app/$1" OLY_IN_DOCKER=1 sh ./oly build --release)
}

binaryOf() {
  echo "/app/$1/games/enxame/build/cmake-release-docker/games/enxame"
}

# run <binary> <commit> <scenario> <size> <repetition> <side folder> [extra arguments...]
run() {
  binary="$1"
  commit="$2"
  scenario="$3"
  size="$4"
  repetition="$5"
  folder="$6"
  shift 6
  # stepArguments is deliberately unquoted: it holds an option and its number.
  "$binary" --headless --scenario "$scenario" --size "$size" $stepArguments --commit "$commit" --out "$folder/$scenario-$size-$repetition.json" "$@" > /dev/null
}

# The first process of a session runs cold, and the repetitions alternate A then B, so it would always be A's. One
# discarded pair absorbs that before the first repetition. The caller runs the pair; this removes its output.
discardWarmUp() {
  rm -f "$warmUpFolder/$1-$2-warmup.json"
}

summarize() {
  python3 -I /app/games/enxame/compare/summarize.py "$1" --a-label "$2" --b-label "$3"
}

compare() {
  dirA="$1"
  dirB="$2"
  commitA="$3"
  commitB="$4"
  results="/app/$5"
  build "$dirA"
  build "$dirB"
  binaryA="$(binaryOf "$dirA")"
  binaryB="$(binaryOf "$dirB")"
  runs="$("$binaryA" --list)"
  [ "$runs" = "$("$binaryB" --list)" ] || fail "the two commits list different runs"
  mkdir -p "$results/a" "$results/b"

  echo "$runs" | while read -r scenario size; do
    run "$binaryA" "$commitA" "$scenario" "$size" warmup "$warmUpFolder"
    run "$binaryB" "$commitB" "$scenario" "$size" warmup "$warmUpFolder"
    discardWarmUp "$scenario" "$size"
    repetition=1

    while [ "$repetition" -le "$repetitions" ]; do
      run "$binaryA" "$commitA" "$scenario" "$size" "$repetition" "$results/a"
      run "$binaryB" "$commitB" "$scenario" "$size" "$repetition" "$results/b"
      repetition=$((repetition + 1))
    done
  done

  summarize "$results" A B
}

overhead() {
  dir="$1"
  commit="$2"
  results="/app/$3"
  build "$dir"
  binary="$(binaryOf "$dir")"
  mkdir -p "$results/a" "$results/b"
  run "$binary" "$commit" S1 10000 warmup "$warmUpFolder" --timing off
  run "$binary" "$commit" S1 10000 warmup "$warmUpFolder" --timing on
  discardWarmUp S1 10000
  repetition=1

  while [ "$repetition" -le "$repetitions" ]; do
    run "$binary" "$commit" S1 10000 "$repetition" "$results/a" --timing off
    run "$binary" "$commit" S1 10000 "$repetition" "$results/b" --timing on
    repetition=$((repetition + 1))
  done

  summarize "$results" "timing off" "timing on"
}

mode="${1:-}"
[ "$#" -gt 0 ] && shift

case "$mode" in
  compare)
    [ "$#" -eq 5 ] || fail "usage: inside.sh compare <dir-a> <dir-b> <commit-a> <commit-b> <results>"
    compare "$@"
    ;;
  overhead)
    [ "$#" -eq 3 ] || fail "usage: inside.sh overhead <dir> <commit> <results>"
    overhead "$@"
    ;;
  *)
    fail "usage: inside.sh compare|overhead ..."
    ;;
esac
