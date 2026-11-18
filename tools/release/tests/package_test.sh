. "$(dirname "$0")/../../oly/tests/assert.sh"

# Needs the tree that ./spur stage 0.1.0 leaves in build/release/0.1.0.
version=0.1.0
stage=$repository/build/release/$version
release=$stage/download/v$version
latest=$stage/latest/download

if [ ! -d "$release" ]; then
  printf 'package_test: run ./spur stage %s first (no %s)\n' "$version" "$release" >&2
  exit 1
fi

# listing <platform>: the files of the archive, one per line, sorted.
listing() {
  tar -tzf "$release/oly-tools-$version-$1.tar.gz" | grep -v '/$' | LC_ALL=C sort
}

# expected_listing <executable suffix>
expected_listing() {
  {
    printf '%s\n' Spurfile "bin/oly-schema$1" cmake/mingw-w64.cmake docker-compose.yaml spur toolchain.env
    (cd "$repository/templates/game" && find . -type f | sed 's|^\./|template/|')
  } | LC_ALL=C sort
}

# only_system_libraries <file>: ldd names nothing but the C library, the math library and the loader.
only_system_libraries() {
  ldd "$1" | awk '{print $1}' | while IFS= read -r library; do
    case ${library##*/} in
      libc.so.6 | libm.so.6 | linux-vdso.so.1 | ld-linux-x86-64.so.2) ;;
      *) printf 'unexpected library: %s\n' "$library" >&2; exit 1 ;;
    esac
  done
}

# names_no_runtime_dll <file>: a statically linked exe has no import of the MinGW runtime.
names_no_runtime_dll() {
  ! grep -aqE 'libstdc\+\+-6\.dll|libgcc_s|libwinpthread' "$1"
}

# the_tool_runs <oly-schema>: generates C++ for a one-line schema.
the_tool_runs() {
  schema_directory=$(mktemp -d)
  printf 'tag Player\n' > "$schema_directory/x.oly"
  "$1" "$schema_directory/x.oly" --emit "cpp=$schema_directory" > /dev/null
  status=$?
  rm -rf "$schema_directory"
  return "$status"
}

assets() {
  (cd "$release" && ls | LC_ALL=C sort)
}

check "the release holds the five assets and nothing else" test "$(assets)" = "install.sh
oly-core-$version.tgz
oly-tools-$version-linux-x64.tar.gz
oly-tools-$version-windows-x64.tar.gz
oly-web-host-$version.tgz"
check "the linux-x64 archive has the layout" test "$(listing linux-x64)" = "$(expected_listing '')"
check "the windows-x64 archive has the layout" test "$(listing windows-x64)" = "$(expected_listing .exe)"

extracted=$(mktemp -d)
mkdir "$extracted/linux" "$extracted/windows"
tar -xzf "$release/oly-tools-$version-linux-x64.tar.gz" -C "$extracted/linux"
tar -xzf "$release/oly-tools-$version-windows-x64.tar.gz" -C "$extracted/windows"

check "the linux-x64 oly-schema runs" the_tool_runs "$extracted/linux/bin/oly-schema"
check "the linux-x64 oly-schema links only libc, libm and the loader" only_system_libraries "$extracted/linux/bin/oly-schema"
check "the windows oly-schema imports no MinGW runtime" names_no_runtime_dll "$extracted/windows/bin/oly-schema.exe"
check "spur is executable in the archive" test -x "$extracted/linux/spur"
rm -rf "$extracted"

check "latest/download/VERSION is the version" test "$(cat "$latest/VERSION")" = "$version"
check "latest/download/oly carries the version" grep -qx "OLY_LAUNCHER_VERSION=$version" "$latest/oly"
check "latest/download/oly parses" sh -n "$latest/oly"
check "latest/download/oly ends with its closing line" test "$(tail -n 1 "$latest/oly")" = "# end of oly"
check "latest/download/install.sh is the release's" cmp -s "$latest/install.sh" "$release/install.sh"

empty=$(mktemp -d)
status=0
sh "$repository/tools/release/stage.sh" "$version" "$empty" > "$logs/out" 2> "$logs/err" || status=$?
check "stage.sh stops when the npm tarballs are missing" exit_is 1
check "stage.sh names the missing tarball" stderr_has "oly-core-$version.tgz"
rm -rf "$empty"

finish
