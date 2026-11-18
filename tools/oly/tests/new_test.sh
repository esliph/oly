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

touch "$work/meu-jogo/kept"
run_task "$work" new meu-jogo --environment native
check "existing folder: exit 64" exit_is 64
check "existing folder: message" stderr_has "meu-jogo already exists"
check "existing folder: left unchanged" test -f "$work/meu-jogo/kept"

work_bad=$(mktemp -d)
trap 'rm -rf "$logs" "$work" "$work_bad"' EXIT

run_task "$work_bad" new Meu_Jogo --environment native
check "bad name: exit 64" exit_is 64
check "bad name: message" stderr_has "game name must match [a-z][a-z0-9-]*: Meu_Jogo"
check "bad name: no folder" test -z "$(ls "$work_bad")"

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
