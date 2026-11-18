#!/bin/sh
# stage.sh <version> <out root>: lays the release out as GitHub serves it: <out root>/download/v<version>/ with every
# asset and <out root>/latest/download/ with what the launcher and install.sh read. The npm tarballs must already be in
# the first folder, and both oly-schema built (./spur stage does both).

version=${1:-}
out=${2:-}
root=$(cd "$(dirname "$0")/../.." && pwd)

fail() {
  printf 'stage.sh: %s\n' "$2" >&2
  exit "$1"
}

[ "$#" -eq 2 ] || fail 64 "usage: stage.sh <version> <out root>"
release=$out/download/v$version
latest=$out/latest/download

linux_tool=$root/build/schema-tool-linux/bin/oly-schema
windows_tool=$root/build/schema-tool-windows/bin/oly-schema.exe
for file in "$linux_tool" "$windows_tool" "$release/oly-core-$version.tgz" "$release/oly-web-host-$version.tgz"; do
  [ -f "$file" ] || fail 1 "missing $file"
done

mkdir -p "$latest" || fail 1 "could not create $latest"
sh "$root/tools/release/package_tools.sh" "$version" linux-x64 "$linux_tool" "$release" || exit 1
sh "$root/tools/release/package_tools.sh" "$version" windows-x64 "$windows_tool" "$release" || exit 1
cp "$root/tools/oly/install.sh" "$release/install.sh"

# The launcher in the repository is version dev; a release carries its own.
sed "s/^OLY_LAUNCHER_VERSION=.*/OLY_LAUNCHER_VERSION=$version/" "$root/tools/oly/oly" > "$latest/oly" || fail 1 "could not write $latest/oly"
printf '%s\n' "$version" > "$latest/VERSION"
cp "$root/tools/oly/install.sh" "$latest/install.sh"
