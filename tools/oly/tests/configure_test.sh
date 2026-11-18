. "$(dirname "$0")/assert.sh"

work=$(mktemp -d)
trap 'rm -rf "$logs" "$work"' EXIT

# A stub cmake that records its arguments, one line per call.
stubs=$work/stubs
mkdir "$stubs"
cat > "$stubs/cmake" <<'STUB'
#!/bin/sh
echo "$*" >> "$CMAKE_LOG"
STUB
chmod +x "$stubs/cmake"
export CMAKE_LOG=$work/cmake.log
PATH=$stubs:$PATH

run_task "$work" new meu-jogo --environment native


: > "$CMAKE_LOG"
TEST_ENGINE_DIR=
run_task "$work/meu-jogo" build
check "pinned: build succeeds" exit_is 0
check "pinned: the source directory is passed empty" sh -c "grep -- '^-S ' \"\$CMAKE_LOG\" | grep -Eq -- '-DFETCHCONTENT_SOURCE_DIR_OLY=( |\$)'"
check "pinned: native host is on" sh -c "grep -q -- '-DOLY_BUILD_NATIVE_HOST=ON' \"\$CMAKE_LOG\""
check "pinned: the build type is Debug" sh -c "grep -q -- '-DCMAKE_BUILD_TYPE=Debug' \"\$CMAKE_LOG\""

: > "$CMAKE_LOG"
TEST_ENGINE_DIR=/x
run_task "$work/meu-jogo" build
check "local: build succeeds" exit_is 0
check "local: the source directory is the checkout" sh -c "grep -q -- '-DFETCHCONTENT_SOURCE_DIR_OLY=/x' \"\$CMAKE_LOG\""

TEST_ENGINE_DIR=
OLY_ENGINE_SOURCE_DIR=/staged
export OLY_ENGINE_SOURCE_DIR
: > "$CMAKE_LOG"
run_task "$work/meu-jogo" build
check "staged source: the source directory is the staged one" sh -c "grep -q -- '-DFETCHCONTENT_SOURCE_DIR_OLY=/staged' \"\$CMAKE_LOG\""

finish
