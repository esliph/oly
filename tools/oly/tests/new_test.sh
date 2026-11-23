. "$(dirname "$0")/assert.sh"

work=$(mktemp -d)
trap 'rm -rf "$logs" "$work"' EXIT

no_placeholder_left() {
  ! grep -rEq '@[A-Z_]+@' "$1"
}

has_line() {
  grep -qxF -- "$1" "$2"
}

run_task "$work" new meu-jogo --environment native
check "native game: exit 0" exit_is 0
check "native game: oly.env has the target" has_line OLY_GAME=meu_jogo "$work/meu-jogo/oly.env"
check "native game: oly.env has the environment" has_line OLY_ENVIRONMENT=native "$work/meu-jogo/oly.env"
check "native game: oly.env has the version" has_line OLY_VERSION=0.1.0 "$work/meu-jogo/oly.env"
check "native game: the schema is renamed" test -f "$work/meu-jogo/meu-jogo.oly"
check "native game: no placeholder is left" no_placeholder_left "$work/meu-jogo"
check "native game: .gitignore exists" test -f "$work/meu-jogo/.gitignore"
check "native game: the CI workflow exists" test -f "$work/meu-jogo/.github/workflows/ci.yml"
check "native game: ./oly is the checkout's launcher" cmp -s "$work/meu-jogo/oly" "$repository/tools/oly/oly"
check "native game: ./oly is executable" test -x "$work/meu-jogo/oly"
check "native game: .gitattributes pins the launcher to LF" has_line "oly text eol=lf" "$work/meu-jogo/.gitattributes"
check "native game: gitattributes is renamed" test ! -e "$work/meu-jogo/gitattributes"

touch "$work/meu-jogo/kept"
run_task "$work" new meu-jogo --environment native
check "existing folder: exit 64" exit_is 64
check "existing folder: message" stderr_has "meu-jogo already exists"
check "existing folder: left unchanged" test -f "$work/meu-jogo/kept"

tools=$work/tools
work_pinned=$work/pinned
mkdir -p "$tools" "$work_pinned"
cp -R "$repository/templates/game" "$tools/template"
sed "s/^OLY_LAUNCHER_VERSION=.*/OLY_LAUNCHER_VERSION=0.1.0/" "$repository/tools/oly/oly" > "$tools/oly"

TEST_ENGINE_DIR= TEST_TOOLS_DIR=$tools run_task "$work_pinned" new pinned-game --environment native
check "pinned game: exit 0" exit_is 0
check "pinned game: ./oly is the version's launcher" cmp -s "$work_pinned/pinned-game/oly" "$tools/oly"
check "pinned game: ./oly is executable" test -x "$work_pinned/pinned-game/oly"

work_bad=$(mktemp -d)
trap 'rm -rf "$logs" "$work" "$work_bad"' EXIT

run_task "$work_bad" new Meu_Jogo --environment native
check "bad name: exit 64" exit_is 64
check "bad name: message" stderr_has "game name must match [a-z][a-z0-9-]*: Meu_Jogo"
check "bad name: no folder" test -z "$(ls "$work_bad")"

for reserved in test all clean help install package oly oly-ecs; do
  run_task "$work_bad" new "$reserved" --environment native
  check "reserved name $reserved: exit 64" exit_is 64
  check "reserved name $reserved: message" stderr_has "is a CMake or engine target name"
  check "reserved name $reserved: no folder" test -z "$(ls "$work_bad")"
done

run_task "$work_bad" new meu-jogo
check "no environment: exit 64" exit_is 64
check "no environment: message" stderr_has "--environment is web or native"
check "no environment: no folder" test -z "$(ls "$work_bad")"

TEST_VERSION=
run_task "$work_bad" new meu-jogo --environment native
unset TEST_VERSION
check "no version: exit 64" exit_is 64
check "no version: message" stderr_has "OLY_VERSION is not set"
check "no version: no folder" test -z "$(ls "$work_bad")"

run_task "$work_bad" platform
check "platform on Linux" stdout_is linux-x64

finish
