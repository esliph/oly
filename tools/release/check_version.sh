#!/bin/sh
# check_version.sh <version> <root>: exits 1 naming the first place that does not carry the version: project(Oly
# VERSION), the three package.json files, or the four entries of package-lock.json that carry it. The lock is read with
# sed because the dev image has no node.

version=${1:-}
root=${2:-}
core=engine/environments/web/clients/typescript/core
host=engine/environments/web/host/js

if ! printf '%s' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  printf 'version must be MAJOR.MINOR.PATCH: %s\n' "$version" >&2
  exit 64
fi
if [ ! -d "$root" ]; then
  printf 'usage: check_version.sh <version> <root>\n' >&2
  exit 64
fi

# expect <what> <found>
expect() {
  if [ "$2" != "$version" ]; then
    printf '%s is %s, expected %s\n' "$1" "${2:-missing}" "$version" >&2
    exit 1
  fi
}

# The "version" field of a package.json, and the version of the @oly/core dependency.
package_version() {
  sed -n 's/^  "version": "\([^"]*\)".*/\1/p' "$root/$1" | head -n 1
}

dependency_version() {
  sed -n 's/.*"@oly\/core": "\([^"]*\)".*/\1/p' "$root/$1" | head -n 1
}

# lock_entry <workspace path>: the lines of its entry in package-lock.json, up to the closing brace at entry depth.
lock_entry() {
  sed -n "\|\"$1\": {|,/^    }/p" "$root/package-lock.json"
}

expect "project(Oly VERSION)" "$(sed -n 's/^project(Oly VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
expect "oly version" "$(package_version package.json)"
expect "@oly/core version" "$(package_version "$core/package.json")"
expect "@oly/web-host version" "$(package_version "$host/package.json")"
expect "@oly/web-host dependency on @oly/core" "$(dependency_version "$host/package.json")"
expect "package-lock.json oly version" "$(sed -n 's/^  "version": "\([^"]*\)".*/\1/p' "$root/package-lock.json" | head -n 1)"
expect "package-lock.json root entry version" "$(lock_entry "" | sed -n 's/^ *"version": "\([^"]*\)".*/\1/p' | head -n 1)"
expect "package-lock.json @oly/core version" "$(lock_entry "$core" | sed -n 's/^ *"version": "\([^"]*\)".*/\1/p' | head -n 1)"
expect "package-lock.json @oly/web-host version" "$(lock_entry "$host" | sed -n 's/^ *"version": "\([^"]*\)".*/\1/p' | head -n 1)"
expect "package-lock.json @oly/web-host dependency on @oly/core" "$(lock_entry "$host" | sed -n 's/.*"@oly\/core": "\([^"]*\)".*/\1/p' | head -n 1)"
