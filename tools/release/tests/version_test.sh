. "$(dirname "$0")/../../oly/tests/assert.sh"

current=$(sed -n 's/^project(Oly VERSION \([0-9.]*\).*/\1/p' "$repository/CMakeLists.txt")
core=engine/environments/web/clients/typescript/core/package.json
host=engine/environments/web/host/js/package.json
check_script=$repository/tools/release/check_version.sh
set_script=$repository/tools/release/set_version.sh

# fresh_root: copies the files that carry the version into a new folder, as if the tree were on 0.1.0, and leaves its
# path in $root.
fresh_root() {
  root=$(mktemp -d)

  for file in CMakeLists.txt package.json package-lock.json $core $host; do
    mkdir -p "$root/$(dirname "$file")"
    cp "$repository/$file" "$root/$file"
    sed -i "s/$current/0.1.0/g" "$root/$file"
  done
}

# check_in <root> <version>: leaves the exit code in $status and the messages in $logs/err.
check_in() {
  status=0
  sh "$check_script" "$2" "$1" > "$logs/out" 2> "$logs/err" || status=$?
}

# differs_alone <description> <file> <sed expression> <message>: edits one file to 0.0.9 and expects exactly that message.
differs_alone() {
  fresh_root
  sed -i "$3" "$root/$2"
  check_in "$root" 0.1.0
  check "$1" exit_is 1
  check "$1: message" stderr_has "$4"
  rm -rf "$root"
}

fresh_root
check_in "$root" 0.1.0
check "a tree on 0.1.0 passes" exit_is 0
check_in "$root" 1.0
check "a version without a patch is refused" exit_is 64
check "a version without a patch: message" stderr_has "version must be MAJOR.MINOR.PATCH: 1.0"
rm -rf "$root"

check_in "$repository" "$current"
check "the real tree is on its own version" exit_is 0

differs_alone "CMakeLists.txt" CMakeLists.txt 's/^project(Oly VERSION [0-9.]*/project(Oly VERSION 0.0.9/' "project(Oly VERSION) is 0.0.9, expected 0.1.0"
differs_alone "root package.json" package.json '0,/"version": "0.1.0"/s//"version": "0.0.9"/' "oly version is 0.0.9, expected 0.1.0"
differs_alone "lock, top-level version" package-lock.json '0,/"version": "0.1.0"/s//"version": "0.0.9"/' "package-lock.json oly version is 0.0.9, expected 0.1.0"
differs_alone "lock entry of the root workspace" package-lock.json '/^    "": {/,/^    }/s/"version": "0.1.0"/"version": "0.0.9"/' "package-lock.json root entry version is 0.0.9, expected 0.1.0"
differs_alone "@oly/core package.json" $core '0,/"version": "0.1.0"/s//"version": "0.0.9"/' "@oly/core version is 0.0.9, expected 0.1.0"
differs_alone "@oly/web-host package.json" $host '0,/"version": "0.1.0"/s//"version": "0.0.9"/' "@oly/web-host version is 0.0.9, expected 0.1.0"
differs_alone "@oly/web-host dependency" $host 's/"@oly\/core": "0.1.0"/"@oly\/core": "0.0.9"/' "@oly/web-host dependency on @oly/core is 0.0.9, expected 0.1.0"
differs_alone "lock entry of @oly/core" package-lock.json '/typescript\/core": {/,/}/s/"version": "0.1.0"/"version": "0.0.9"/' "package-lock.json @oly/core version is 0.0.9, expected 0.1.0"
differs_alone "lock entry of @oly/web-host" package-lock.json '/web\/host\/js": {/,/^    }/s/"version": "0.1.0"/"version": "0.0.9"/' "package-lock.json @oly/web-host version is 0.0.9, expected 0.1.0"
differs_alone "lock dependency of @oly/web-host" package-lock.json '/web\/host\/js": {/,/^    }/s/"@oly\/core": "0.1.0"/"@oly\/core": "0.0.9"/' "package-lock.json @oly/web-host dependency on @oly/core is 0.0.9, expected 0.1.0"

fresh_root
status=0
sh "$set_script" 0.2.0 "$root" > "$logs/out" 2> "$logs/err" || status=$?
check "set_version exits 0" exit_is 0
check "set_version keeps the rest of the CMake line" grep -q '^project(Oly VERSION 0.2.0 LANGUAGES CXX C)' "$root/CMakeLists.txt"
check_in "$root" 0.2.0
check "after set_version only the lock is behind" exit_is 1
check "after set_version only the lock is behind: message" stderr_has "package-lock.json oly version is 0.1.0, expected 0.2.0"
check "set_version writes the root package.json" grep -q '^  "version": "0.2.0"' "$root/package.json"
sh "$set_script" 1.0 "$root" > "$logs/out" 2> "$logs/err" || status=$?
check "set_version refuses a bad version" exit_is 64
check "set_version leaves the CMake line alone on a bad version" grep -q '^project(Oly VERSION 0.2.0 ' "$root/CMakeLists.txt"
rm -rf "$root"

finish
