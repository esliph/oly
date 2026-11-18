. "$(dirname "$0")/assert.sh"

work=$(mktemp -d)
trap 'rm -rf "$logs" "$work"' EXIT

# A stub docker that records its arguments and the variables the compose file interpolates.
stubs=$work/stubs
mkdir "$stubs"
cat > "$stubs/docker" <<'STUB'
#!/bin/sh
echo "$* | version=$OLY_VERSION game=$OLY_GAME_DIR pathconv=$MSYS_NO_PATHCONV" >> "$DOCKER_LOG"
STUB
chmod +x "$stubs/docker"
cat > "$stubs/cmake" <<'STUB'
#!/bin/sh
if [ "$1" = --version ]; then
  echo "cmake version 3.30.5"
  exit 0
fi
echo "$*" >> "$CMAKE_LOG"
STUB
chmod +x "$stubs/cmake"
printf '#!/bin/sh\necho 14.2.0\n' > "$stubs/g++"
chmod +x "$stubs/g++"
cp "$stubs/g++" "$stubs/x86_64-w64-mingw32-g++-posix"
export DOCKER_LOG=$work/docker.log
export CMAKE_LOG=$work/cmake.log
PATH=$stubs:$PATH

run_task "$work" new native-game --environment native
mkdir "$work/web-game"
printf 'OLY_VERSION=0.1.0\nOLY_ENVIRONMENT=web\nOLY_GAME=web_game\nOLY_RUNNER=host\n' > "$work/web-game/oly.env"

docker_line_has() {
  grep -qF -- "$1" "$DOCKER_LOG"
}

no_docker_call() {
  [ ! -s "$DOCKER_LOG" ]
}

: > "$DOCKER_LOG"
TEST_ENGINE_DIR=/x
run_task "$work/native-game" build --docker
check "local mode: build --docker succeeds" exit_is 0
check "local mode: runs in a throwaway container" docker_line_has " run --rm "
check "local mode: uses the compose file of the checkout" docker_line_has "compose -f /x/tools/oly/docker-compose.yaml"
check "local mode: mounts the checkout" docker_line_has "-v /x:/oly -e OLY_ENGINE_DIR=/oly"
check "local mode: takes the tools from the checkout" docker_line_has "-e OLY_TOOLS_DIR=/oly/tools/oly"
check "local mode: native games run in dev, with the game tasks of the checkout" docker_line_has " dev sh /oly/spur -f /oly/tools/oly/Spurfile build --host"
check "local mode: exports the version and the game for the compose file" docker_line_has "version=0.1.0 game=$work/native-game"
check "local mode: stops Git Bash from rewriting the arguments" docker_line_has "pathconv=1"

: > "$DOCKER_LOG"
TEST_ENGINE_DIR=
run_task "$work/native-game" build --docker
check "pinned: takes the tools baked into the image" docker_line_has " dev sh /oly-tools/spur -f /oly-tools/Spurfile build --host"
check "pinned: names the tools directory" docker_line_has "-e OLY_TOOLS_DIR=/oly-tools"
check "pinned: mounts no checkout" sh -c "! grep -qF -- ':/oly ' '$DOCKER_LOG'"

: > "$DOCKER_LOG"
run_task "$work/web-game" run --docker
check "web run: publishes the port" docker_line_has "--service-ports"
check "web run: runs in wasm" docker_line_has " wasm sh /oly-tools/spur"

: > "$DOCKER_LOG"
run_task "$work/web-game" test --docker
check "web test: no port is published" sh -c "! grep -qF -- '--service-ports' '$DOCKER_LOG'"

: > "$DOCKER_LOG"
run_task "$work/native-game" run --docker
check "native run: exit 64" exit_is 64
check "native run: says to run the executable on the host" stderr_has "a desktop window cannot open in the container: run build/cmake-docker/games/native_game on the host"
check "native run: no container started" no_docker_call

: > "$DOCKER_LOG"
run_task "$work/native-game" test --docker --platform windows
check "windows: runs in the windows image" docker_line_has " windows sh /oly-tools/spur"
check "windows: passes the platform on, after --host" docker_line_has "test --host --platform windows"

run_task "$work/web-game" build --docker --platform windows
check "windows web game: exit 64" exit_is 64
check "windows web game: message" stderr_has "--platform windows is for native games"

run_task "$work/native-game" build --platform linux
check "unknown platform: exit 64" exit_is 64
check "unknown platform: message" stderr_has "--platform is windows"

printf 'OLY_VERSION=0.1.0\nOLY_ENVIRONMENT=native\nOLY_GAME=native_game\nOLY_RUNNER=docker\n' > "$work/native-game/oly.env"
: > "$DOCKER_LOG"
run_task "$work/native-game" build
check "OLY_RUNNER=docker: build uses docker without a flag" docker_line_has " dev sh /oly-tools/spur"

: > "$DOCKER_LOG"
: > "$CMAKE_LOG"
run_task "$work/native-game" build --host
check "--host overrides OLY_RUNNER=docker: no container" no_docker_call
check "--host overrides OLY_RUNNER=docker: builds on the host" sh -c "grep -q -- '-S . -B build/cmake ' '$CMAKE_LOG'"

: > "$CMAKE_LOG"
OLY_IN_DOCKER=1
export OLY_IN_DOCKER
run_task "$work/native-game" build --host
unset OLY_IN_DOCKER
check "in the container: its own build directory" sh -c "grep -q -- '-B build/cmake-docker ' '$CMAKE_LOG'"

: > "$CMAKE_LOG"
OLY_IN_DOCKER=1
export OLY_IN_DOCKER
run_task "$work/native-game" build --host --platform windows
unset OLY_IN_DOCKER
check "windows in the container: its own build directory" sh -c "grep -q -- '-B build/cmake-windows-docker ' '$CMAKE_LOG'"
check "windows in the container: the MinGW toolchain file" sh -c "grep -q -- '-DCMAKE_TOOLCHAIN_FILE=.*mingw-w64.cmake' '$CMAKE_LOG'"

finish
