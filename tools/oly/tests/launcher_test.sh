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

add_release "$tree" 0.2.0

run_oly "$work/empty" new demo --environment web
check "new: exit 0" exit_is 0
check "new: runs the latest version in the current folder" stdout_is "tools=$home/versions/0.1.0 version=0.1.0 args=-f $home/versions/0.1.0/Spurfile -C $work/empty new demo --environment web"

run_oly "$work/empty" new demo --environment web --version 0.2.0
check "new --version: exit 0" exit_is 0
check "new --version: runs that version without the option" stdout_is "tools=$home/versions/0.2.0 version=0.2.0 args=-f $home/versions/0.2.0/Spurfile -C $work/empty new demo --environment web"

# make_web_game <name> <version>: a game with package.json, the way a web game has it.
make_web_game() {
  make_game "$1" "$2"
  cat > "$work/$1/package.json" << JSON
{
  "dependencies": {
    "@oly/core": "$tree/download/v$2/oly-core-$2.tgz",
    "@oly/web-host": "$tree/download/v$2/oly-web-host-$2.tgz"
  }
}
JSON
}

make_web_game web 0.1.0
run_oly "$work/web" upgrade 0.2.0
check "upgrade web: exit 0" exit_is 0
check "upgrade web: oly.env pins the new version" grep -qxF OLY_VERSION=0.2.0 "$work/web/oly.env"
check "upgrade web: package.json names only the new version" grep -q 0.2.0 "$work/web/package.json"
check "upgrade web: package.json lost the old version" test -z "$(grep 0.1.0 "$work/web/package.json")"
check "upgrade web: both tarballs moved" test "$(grep -c "v0.2.0/oly-.*-0.2.0.tgz" "$work/web/package.json")" -eq 2
check "upgrade web: runs sync of the new version" stdout_is "tools=$home/versions/0.2.0 version=0.2.0 args=-f $home/versions/0.2.0/Spurfile -C $work/web sync"

make_game native 0.1.0
run_oly "$work/native" upgrade 0.2.0
check "upgrade native: exit 0" exit_is 0
check "upgrade native: oly.env pins the new version" grep -qxF OLY_VERSION=0.2.0 "$work/native/oly.env"
check "upgrade native: no package.json is created" test ! -e "$work/native/package.json"
check "upgrade native: no sync" test -z "$(grep sync "$logs/out")"

make_web_game same 0.1.0
cp "$work/same/oly.env" "$work/same-oly.env"
cp "$work/same/package.json" "$work/same-package.json"
run_oly "$work/same" upgrade 0.1.0
check "upgrade same version: exit 0" exit_is 0
check "upgrade same version: message" stdout_is "already on 0.1.0"
check "upgrade same version: oly.env unchanged" cmp -s "$work/same/oly.env" "$work/same-oly.env"
check "upgrade same version: package.json unchanged" cmp -s "$work/same/package.json" "$work/same-package.json"

run_oly "$work/empty" upgrade 0.2.0
check "upgrade outside a game: exit 64" exit_is 64

run_oly "$work/native" upgrade 9.9.9
check "upgrade to a missing version: exit 1" exit_is 1
check "upgrade to a missing version: oly.env unchanged" grep -qxF OLY_VERSION=0.2.0 "$work/native/oly.env"

mkdir -p "$work/bin"
cp "$launcher" "$work/bin/oly"
add_launcher_asset "$tree" "$launcher" 0.2.0
original_launcher=$launcher
launcher=$work/bin/oly
run_oly "$work" self-update
check "self-update: exit 0" exit_is 0
run_oly "$work/empty" --version
check "self-update: the launcher is the release's" stdout_is "oly 0.2.0"
check "self-update: the launcher is executable" test -x "$work/bin/oly"
check "self-update: no temporary file is left" test "$(ls "$work/bin")" = oly

cp "$work/bin/oly" "$work/before-oly"
add_launcher_asset "$tree" "$original_launcher" 0.3.0 broken
run_oly "$work" self-update
check "self-update broken: exit 1" exit_is 1
check "self-update broken: the old launcher stays" cmp -s "$work/bin/oly" "$work/before-oly"
check "self-update broken: no temporary file is left" test "$(ls "$work/bin")" = oly
launcher=$original_launcher

mkdir "$work/installed"
add_launcher_asset "$tree" "$launcher" 0.2.0
install_home=$work/installed
install_oly() {
  status=0
  env -u OLY_HOME HOME="$install_home" SHELL=${1:-/bin/bash} OLY_RELEASE_URL="file://$tree" sh "$repository/tools/oly/install.sh" > "$logs/out" 2> "$logs/err" || status=$?
}

path_lines() {
  grep -c 'PATH=".*/.oly/bin:' "$install_home/$1"
}

install_oly
check "install: exit 0" exit_is 0
check "install: the launcher is in place and executable" test -x "$install_home/.oly/bin/oly"
check "install: .bashrc has one PATH line" test "$(path_lines .bashrc)" -eq 1
check "install: message names the path" grep -qF "oly installed in $install_home/.oly/bin/oly" "$logs/out"

install_oly
check "install twice: exit 0" exit_is 0
check "install twice: still one PATH line" test "$(path_lines .bashrc)" -eq 1

install_home=$work/installed-zsh
mkdir "$install_home"
install_oly /usr/bin/zsh
check "install with zsh: exit 0" exit_is 0
check "install with zsh: .zshrc has the PATH line" test "$(path_lines .zshrc)" -eq 1
check "install with zsh: .bashrc is not created" test ! -e "$install_home/.bashrc"

finish
