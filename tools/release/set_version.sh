#!/bin/sh
# set_version.sh <version> <root>: writes the version into project(Oly VERSION), the version of @oly/core and
# @oly/web-host and the @oly/core dependency of @oly/web-host. package-lock.json is left to npm (./spur release).

version=${1:-}
root=${2:-}
core=engine/environments/web/clients/typescript/core/package.json
host=engine/environments/web/host/js/package.json

if ! printf '%s' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  printf 'version must be MAJOR.MINOR.PATCH: %s\n' "$version" >&2
  exit 64
fi
if [ ! -d "$root" ]; then
  printf 'usage: set_version.sh <version> <root>\n' >&2
  exit 64
fi

# rewrite <file under the root> <sed -E expression>: through a temporary file, so a failed sed never truncates the file.
rewrite() {
  file=$root/$1
  if ! sed -E "$2" "$file" > "$file.new" || ! mv "$file.new" "$file"; then
    rm -f "$file.new"
    printf 'could not rewrite %s\n' "$file" >&2
    exit 1
  fi
}

rewrite CMakeLists.txt "s/^(project\(Oly VERSION )[0-9.]+/\1$version/"
rewrite package.json "s/^(  \"version\": \")[^\"]*/\1$version/"
rewrite "$core" "s/^(  \"version\": \")[^\"]*/\1$version/"
rewrite "$host" "s/^(  \"version\": \")[^\"]*/\1$version/"
rewrite "$host" "s/(\"@oly\/core\": \")[^\"]*/\1$version/"
