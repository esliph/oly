# Sourced by launcher_test.sh: builds a release tree laid out as GitHub serves it, so OLY_RELEASE_URL=file://<tree> works.

# The stub spur prints what the launcher handed it, so a test can read it back.
write_stub_spur() {
  cat > "$1/spur" << 'STUB'
#!/bin/sh
printf 'tools=%s version=%s args=%s\n' "$OLY_TOOLS_DIR" "$OLY_VERSION" "$*"
STUB
}

# add_release <tree> <version>: download/v<version>/oly-tools-<version>-linux-x64.tar.gz holding a stub spur and an empty Spurfile.
add_release() {
  staging=$(mktemp -d)
  write_stub_spur "$staging"
  : > "$staging/Spurfile"
  mkdir -p "$1/download/v$2"
  tar -czf "$1/download/v$2/oly-tools-$2-linux-x64.tar.gz" -C "$staging" spur Spurfile
  rm -rf "$staging"
}

# set_latest <tree> <version>
set_latest() {
  mkdir -p "$1/latest/download"
  printf '%s\n' "$2" > "$1/latest/download/VERSION"
}
