# Sourced by launcher_test.sh: builds a release tree laid out as GitHub serves it, so OLY_RELEASE_URL=file://<tree> works.

# The stub spur prints what the launcher handed it, so a test can read it back.
write_stub_spur() {
  cat > "$1/spur" << 'STUB'
#!/bin/sh
printf 'tools=%s version=%s args=%s\n' "$OLY_TOOLS_DIR" "$OLY_VERSION" "$*"
STUB
}

# add_release <tree> <version> [truncated|no-launcher]: download/v<version>/oly-tools-<version>-linux-x64.tar.gz holding a
# stub spur, an empty Spurfile and the repository's launcher stamped with the version. truncated keeps its first 200 lines;
# no-launcher leaves it out.
add_release() {
  staging=$(mktemp -d)
  write_stub_spur "$staging"
  : > "$staging/Spurfile"
  files="spur Spurfile"
  if [ "${3:-}" != no-launcher ]; then
    sed "s/^OLY_LAUNCHER_VERSION=.*/OLY_LAUNCHER_VERSION=$2/" "$repository/tools/oly/oly" > "$staging/oly"
    if [ "${3:-}" = truncated ]; then
      head -n 200 "$staging/oly" > "$staging/oly.cut"
      mv "$staging/oly.cut" "$staging/oly"
    fi
    files="$files oly"
  fi
  mkdir -p "$1/download/v$2"
  tar -czf "$1/download/v$2/oly-tools-$2-linux-x64.tar.gz" -C "$staging" $files
  rm -rf "$staging"
}

# set_latest <tree> <version>
set_latest() {
  mkdir -p "$1/latest/download"
  printf '%s\n' "$2" > "$1/latest/download/VERSION"
}

# add_launcher_asset <tree> <launcher file> <version> [broken]: the `oly` asset of the latest release, with that launcher version.
add_launcher_asset() {
  mkdir -p "$1/latest/download"
  sed "s/^OLY_LAUNCHER_VERSION=.*/OLY_LAUNCHER_VERSION=$3/" "$2" > "$1/latest/download/oly"
  if [ "${4:-}" = broken ]; then
    printf 'if then\n' >> "$1/latest/download/oly"
  fi
}
