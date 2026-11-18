. "$(dirname "$0")/assert.sh"

work=$(mktemp -d)
trap 'rm -rf "$logs" "$work"' EXIT

# stub <name> <output>: a command that prints <output> whatever its arguments.
stub() {
  printf '#!/bin/sh\necho "%s"\n' "$2" > "$stubs/$1"
  chmod +x "$stubs/$1"
}

# The tools of a host that passes every check; a case then changes the one it is about.
baseline() {
  stubs=$(mktemp -d "$work/stubs.XXXXXX")
  stub cmake "cmake version 3.30.5"
  stub g++ 14.2.0
  stub emcc "emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 3.1.74 (0123abcd)"
  stub node v20.18.0
  stub npm 10.8.2
}

# run_doctor <arguments...>: the PATH holds the stubs and the base system only, so a tool of the container cannot answer.
run_doctor() {
  saved_path=$PATH
  PATH=$stubs:/usr/bin:/bin
  run_task "$work/game" doctor "$@"
  PATH=$saved_path
}

mkdir "$work/game"

has_line() {
  grep -Eq -- "$1" "$logs/out"
}

every_line_is_ok() {
  ! grep -Evq '^ok ' "$logs/out"
}

baseline
run_doctor
check "all tools at the toolchain.env versions: exit 0" exit_is 0
check "all tools at the toolchain.env versions: every line is ok" every_line_is_ok
check "all tools at the toolchain.env versions: it prints ok lines" has_line "^ok "

baseline
stub g++ 13.2.0
run_doctor
check "g++ 13.2.0: exit 1" exit_is 1
check "g++ 13.2.0: an old line names it and the minimum" has_line '^old +g\+\+ 13\.2\.0.*14'

baseline
stub emcc "emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 3.1.70 (0123abcd)"
run_doctor
check "emcc 3.1.70: exit 1" exit_is 1
check "emcc 3.1.70: the line names both versions" has_line '^(old|untested) +emcc.*3\.1\.70.*3\.1\.74'

baseline
rm "$stubs/emcc"
run_doctor
check "no emcc: exit 1" exit_is 1
check "no emcc: says what to install" has_line '^missing +emcc.*install emsdk 3\.1\.74'

baseline
stub cmake "cmake version 3.24.0"
run_doctor
check "cmake 3.24.0: exit 1" exit_is 1

mkdir "$work/native-game"
printf 'OLY_VERSION=0.1.0\nOLY_ENVIRONMENT=native\nOLY_GAME=meu_jogo\nOLY_RUNNER=host\n' > "$work/native-game/oly.env"
baseline
rm "$stubs/emcc" "$stubs/node" "$stubs/npm"
saved_path=$PATH
PATH=$stubs:/usr/bin:/bin
run_task "$work/native-game" doctor
PATH=$saved_path
check "inside a native game: web tools are not checked" exit_is 0

baseline
stub docker "Docker Compose version v2.29.1"
run_doctor --docker
check "--docker: exit 0" exit_is 0
check "--docker: only docker compose is checked" has_line '^ok +docker compose'
check "--docker: no other tool is named" sh -c "! grep -Eq 'cmake|g\+\+|emcc' '$logs/out'"

baseline
run_doctor --docker
check "--docker without docker: exit 1" exit_is 1
check "--docker without docker: missing docker compose" has_line '^missing +docker compose'

run_task "$work/game" version-at-least 14.2.0-ubuntu1 14
check "a tail such as -ubuntu1 is ignored" exit_is 0

run_task "$work/game" version-at-least 1.10.1 1.11
check "1.10.1 is older than 1.11" exit_is 1

run_task "$work/game" version-at-least 3.1.74 3.1.74
check "an equal version is enough" exit_is 0

finish
