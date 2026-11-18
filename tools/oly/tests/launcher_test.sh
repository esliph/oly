. "$(dirname "$0")/assert.sh"
. "$(dirname "$0")/fake_release.sh"

launcher=$repository/tools/oly/oly
work=$(mktemp -d)
home=$work/home
tree=$work/release
trap 'rm -rf "$logs" "$work"' EXIT

add_release "$tree" 0.1.0
set_latest "$tree" 0.1.0

# make_game <name> <version>: a folder with an oly.env, the way `oly new` leaves one.
make_game() {
  mkdir -p "$work/$1"
  printf 'OLY_VERSION=%s\nOLY_ENVIRONMENT=native\nOLY_GAME=%s\nOLY_RUNNER=host\n' "$2" "$1" > "$work/$1/oly.env"
}

# run_oly <directory> <arguments...>: runs the launcher from the directory; LOCAL_ENGINE turns local mode on.
run_oly() {
  directory=$1
  shift
  status=0
  if [ -n "${LOCAL_ENGINE:-}" ]; then
    (cd "$directory" && env OLY_ENGINE_DIR="$LOCAL_ENGINE" OLY_HOME="$home" OLY_RELEASE_URL="file://$tree" sh "$launcher" "$@") > "$logs/out" 2> "$logs/err" || status=$?
  else
    (cd "$directory" && env -u OLY_ENGINE_DIR OLY_HOME="$home" OLY_RELEASE_URL="file://$tree" sh "$launcher" "$@") > "$logs/out" 2> "$logs/err" || status=$?
  fi
}

versions_are() {
  [ "$(ls -A "$home/versions" 2> /dev/null | tr '\n' ' ')" = "$1" ]
}

mkdir "$work/empty"
run_oly "$work/empty" build
check "outside a game: exit 64" exit_is 64
check "outside a game: message" stderr_has "not inside an Oly game: no oly.env in $work/empty or above"

make_game game 0.1.0
mkdir -p "$work/game/a/b"
run_oly "$work/game/a/b" build --docker
check "nested folder: exit 0" exit_is 0
check "nested folder: tools, version and arguments" stdout_is "tools=$home/versions/0.1.0 version=0.1.0 args=-f $home/versions/0.1.0/Spurfile -C $work/game build --docker"
check "nested folder: only the version is installed" versions_are "0.1.0 "

rm "$tree/download/v0.1.0/oly-tools-0.1.0-linux-x64.tar.gz"
run_oly "$work/game" build
check "cached version: exit 0 without the archive" exit_is 0

make_game missing 9.9.9
run_oly "$work/missing" build
check "missing version: exit 1" exit_is 1
check "missing version: message names the URL" stderr_has "file://$tree/download/v9.9.9/oly-tools-9.9.9-linux-x64.tar.gz"
check "missing version: nothing is left behind" versions_are "0.1.0 "

mkdir -p "$tree/download/v0.3.0"
printf 'not an archive\n' > "$tree/download/v0.3.0/oly-tools-0.3.0-linux-x64.tar.gz"
make_game corrupt 0.3.0
run_oly "$work/corrupt" build
check "corrupt archive: exit 1" exit_is 1
check "corrupt archive: message" stderr_has "could not install oly-tools 0.3.0"
check "corrupt archive: nothing is left behind" versions_are "0.1.0 "

mkdir -p "$tree/download/v0.4.0"
tar -czf "$tree/download/v0.4.0/oly-tools-0.4.0-linux-x64.tar.gz" -C "$work/game" oly.env
make_game incomplete 0.4.0
run_oly "$work/incomplete" build
check "archive without spur: exit 1" exit_is 1
check "archive without spur: nothing is left behind" versions_are "0.1.0 "

make_game pinned-elsewhere 9.9.9
LOCAL_ENGINE=$repository
run_oly "$work/pinned-elsewhere" platform
check "local mode: exit 0" exit_is 0
check "local mode: runs the checkout's tasks" stdout_is linux-x64
check "local mode: installs nothing" versions_are "0.1.0 "
LOCAL_ENGINE=

run_oly "$work/empty" --version
check "version outside a game: exit 0" exit_is 0
check "version outside a game: one line" stdout_is "oly dev"

run_oly "$work/game" --version
check "version in a game: launcher and engine" stdout_is "oly dev
engine 0.1.0 (oly.env)"

LOCAL_ENGINE=$repository
run_oly "$work/game" --version
check "version in a game, local mode: names the checkout" stdout_is "oly dev
engine $repository (local)"
LOCAL_ENGINE=

finish
