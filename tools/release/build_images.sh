#!/bin/sh
# build_images.sh <version> <registry> <staged root>: builds <registry>/oly-<image>:<version> for dev, wasm and windows:
# the engine images ./spur images made, plus the linux-x64 oly-tools of the staged release at /oly-tools.

version=${1:-}
registry=${2:-}
staged=${3:-}
root=$(cd "$(dirname "$0")/../.." && pwd)

fail() {
  printf 'build_images.sh: %s\n' "$2" >&2
  exit "$1"
}

[ "$#" -eq 3 ] || fail 64 "usage: build_images.sh <version> <registry> <staged root>"
archive=$staged/download/v$version/oly-tools-$version-linux-x64.tar.gz
[ -f "$archive" ] || fail 1 "missing $archive"

context=$(mktemp -d)
trap 'rm -rf "$context"' EXIT
mkdir "$context/oly-tools"
tar -xzf "$archive" -C "$context/oly-tools" || fail 1 "could not extract $archive"

if command -v cygpath > /dev/null 2>&1; then
  context=$(cygpath -m "$context")
fi

for image in dev wasm windows; do
  docker build -f "$root/tools/release/Dockerfile.tools" --build-arg "BASE=oly-engine-$image" -t "$registry/oly-$image:$version" "$context" \
    || fail 1 "could not build $registry/oly-$image:$version"
done
