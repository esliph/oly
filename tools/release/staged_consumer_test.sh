#!/bin/sh
# staged_consumer_test.sh <version> <web|native> <work dir>: what a user does with a release, against the staged tree that
# OLY_RELEASE_URL serves. It creates a game through the pipe that reads the launcher, checks the host, builds the game and
# runs its tests with the game's own ./oly, then repairs that ./oly with oly upgrade.
# OLY_ENGINE_SOURCE_DIR is the commit being released: its tag does not exist yet. OLY_ENGINE_DIR must be unset, or the
# run would use the checkout's tools and prove nothing about the downloaded ones.

version=${1:-}
environment=${2:-}
work=${3:-}

fail() {
  printf 'staged_consumer_test.sh: %s\n' "$2" >&2
  exit "$1"
}

# Git Bash paths (/c/...) mean nothing to node.
host_path() {
  if command -v cygpath > /dev/null 2>&1; then
    cygpath -m "$1"
  else
    printf '%s\n' "$1"
  fi
}

[ "$#" -eq 3 ] || fail 64 "usage: staged_consumer_test.sh <version> <web|native> <work dir>"
case $environment in
  web | native) ;;
  *) fail 64 "environment is web or native: $environment" ;;
esac
[ -z "${OLY_ENGINE_DIR:-}" ] || fail 64 "OLY_ENGINE_DIR is set: this run proves the downloaded tools, not the checkout's"
[ -n "${OLY_RELEASE_URL:-}" ] || fail 64 "OLY_RELEASE_URL is not set"
[ -n "${OLY_ENGINE_SOURCE_DIR:-}" ] || fail 64 "OLY_ENGINE_SOURCE_DIR is not set"

mkdir -p "$work" || fail 1 "could not create $work"
[ -z "$(ls -A "$work")" ] || fail 64 "$work is not empty: a stale launcher or game would be tested instead"
work=$(cd "$work" && pwd)

# Every @oly package of the game comes from the staged tarballs: a lookup in the registry fails on a port nothing uses.
printf '@oly:registry=http://127.0.0.1:9\n' > "$work/npmrc"
NPM_CONFIG_USERCONFIG=$(host_path "$work/npmrc")
export NPM_CONFIG_USERCONFIG

OLY_HOME=$work/home
export OLY_HOME

game=$work/staged-$environment
latest=$OLY_RELEASE_URL/latest/download/oly

# A failed curl hands sh an empty script, which exits 0 and creates nothing: the game's launcher is what proves the run.
(cd "$work" && curl -fsSL "$latest" | sh -s -- new "staged-$environment" --environment "$environment") || fail 1 "creating the game through the pipe failed"
[ -f "$game/oly" ] || fail 1 "the pipe created no game at $game: is $latest readable?"

[ "$(cd "$game" && sh ./oly --version | head -n 1)" = "oly $version" ] || fail 1 "the game's ./oly is not oly $version"
cmp -s "$game/oly" "$OLY_HOME/versions/$version/oly" || fail 1 "the game's ./oly is not the launcher of oly-tools $version"

(cd "$game" && sh ./oly doctor) || fail 1 "oly doctor found the host unfit for $environment"
(cd "$game" && sh ./oly test) || fail 1 "oly test failed in $game"

[ -f "$OLY_HOME/versions/$version/spur" ] || fail 1 "the launcher did not install oly-tools $version into $OLY_HOME/versions"
if [ "$environment" = web ]; then
  grep -q ' pinned$' "$game/node_modules/.oly-install" || fail 1 "node_modules/.oly-install is not a pinned install: $(cat "$game/node_modules/.oly-install")"
  grep -qF "$(host_path "$OLY_HOME")/versions/$version/bin/oly-schema" "$game/build/cmake/CMakeCache.txt" || fail 1 "the game was not built with the downloaded oly-schema"
fi

# The other way to a game: a release other than the latest, read from download/v<version>/.
(cd "$work" && curl -fsSL "$OLY_RELEASE_URL/download/v$version/oly" | sh -s -- new "pinned-$environment" --environment "$environment" --version "$version") || fail 1 "creating the game from download/v$version/oly failed"
curl -fsSL "$OLY_RELEASE_URL/download/v$version/oly" -o "$work/pinned-oly" || fail 1 "could not download download/v$version/oly"
cmp -s "$work/pinned-oly" "$work/pinned-$environment/oly" || fail 1 "the game made from download/v$version/oly does not hold that asset"
cmp -s "$work/pinned-oly" "$game/oly" || fail 1 "the launcher of download/v$version/ is not the latest's"

# The file sh is reading is the one upgrade replaces.
printf '# edited\n' >> "$game/oly"
(cd "$game" && sh ./oly upgrade "$version") || fail 1 "oly upgrade $version failed to repair ./oly"
cmp -s "$game/oly" "$OLY_HOME/versions/$version/oly" || fail 1 "oly upgrade $version did not restore ./oly"
