# Sourced by the *_test.sh files: runs a task of tools/oly/Spurfile in a game directory and checks the result.

repository=$(cd "$(dirname "$0")/../../.." && pwd)
logs=$(mktemp -d)
failures=0
trap 'rm -rf "$logs"' EXIT

# run_task <game directory> <task> [arguments...]: leaves the exit code in $status; the output is in $logs/out and $logs/err.
run_task() {
  game=$1
  shift
  status=0
  OLY_GAME_DIR=$game OLY_TOOLS_DIR=${TEST_TOOLS_DIR-$repository/tools/oly} OLY_ENGINE_DIR=${TEST_ENGINE_DIR-$repository} \
    OLY_VERSION=${TEST_VERSION-0.1.0} \
    sh "$repository/spur" -f "$repository/tools/oly/Spurfile" "$@" > "$logs/out" 2> "$logs/err" || status=$?
}

# check <description> <command...>
check() {
  description=$1
  shift
  if "$@"; then
    printf 'ok   %s\n' "$description"
  else
    printf 'FAIL %s\n' "$description"
    failures=$((failures + 1))
  fi
}

exit_is() {
  [ "$status" -eq "$1" ]
}

stdout_is() {
  [ "$(cat "$logs/out")" = "$1" ]
}

stderr_has() {
  grep -qF -- "$1" "$logs/err"
}

finish() {
  if [ "$failures" -gt 0 ]; then
    printf '%s failed\n' "$failures"
    exit 1
  fi
}
