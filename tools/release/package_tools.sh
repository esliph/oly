#!/bin/sh
# package_tools.sh <version> <platform> <oly-schema file> <out dir>: writes <out dir>/oly-tools-<version>-<platform>.tar.gz,
# the archive the launcher extracts into versions/<version> (spec §4.3), with the engine's own files from this checkout.

version=${1:-}
platform=${2:-}
schema_tool=${3:-}
out=${4:-}
root=$(cd "$(dirname "$0")/../.." && pwd)

fail() {
  printf 'package_tools.sh: %s\n' "$2" >&2
  exit "$1"
}

[ "$#" -eq 4 ] || fail 64 "usage: package_tools.sh <version> <linux-x64|windows-x64> <oly-schema file> <out dir>"

case $platform in
  linux-x64) executable=oly-schema ;;
  windows-x64) executable=oly-schema.exe ;;
  *) fail 64 "platform is linux-x64 or windows-x64: $platform" ;;
esac

[ -f "$schema_tool" ] || fail 1 "no oly-schema at $schema_tool"

mkdir -p "$out" || fail 1 "could not create $out"
out=$(cd "$out" && pwd)
staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT

mkdir "$staging/bin" "$staging/cmake"
cp "$root/spur" "$root/toolchain.env" "$root/tools/oly/docker-compose.yaml" "$staging/"
cp "$root/tools/oly/Spurfile" "$staging/Spurfile"
sed "s/^OLY_LAUNCHER_VERSION=.*/OLY_LAUNCHER_VERSION=$version/" "$root/tools/oly/oly" > "$staging/oly" || fail 1 "could not write the launcher"
cp -R "$root/templates/game" "$staging/template"
cp "$root/cmake/toolchains/mingw-w64.cmake" "$staging/cmake/"
cp "$schema_tool" "$staging/bin/$executable"
chmod 755 "$staging/spur" "$staging/oly" "$staging/bin/$executable"

tar --owner=0 --group=0 --numeric-owner -czf "$out/oly-tools-$version-$platform.tar.gz" -C "$staging" spur Spurfile oly toolchain.env bin template docker-compose.yaml cmake \
  || fail 1 "could not write the archive"
