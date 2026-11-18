. "$(dirname "$0")/assert.sh"

cd "$repository"
[ -d node_modules ] || npm ci --no-audit --no-fund > /dev/null
node_modules/.bin/tsc -b engine/environments/web/clients/typescript/core engine/environments/web/host/js

npm pack --dry-run --json -w @oly/core -w @oly/web-host > "$logs/pack.json" 2> /dev/null

# lists_file <package name> <file>
lists_file() {
  node -e 'const packed = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8")); const entry = packed.find((item) => item.name === process.argv[2]); process.exit(entry && entry.files.some((file) => file.path === process.argv[3]) ? 0 : 1)' "$logs/pack.json" "$1" "$2"
}

# lists_nothing_under <package name> <prefix>
lists_nothing_under() {
  node -e 'const packed = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8")); const entry = packed.find((item) => item.name === process.argv[2]); process.exit(entry && !entry.files.some((file) => file.path.startsWith(process.argv[3])) ? 0 : 1)' "$logs/pack.json" "$1" "$2"
}

check "@oly/core packs dist/index.js" lists_file @oly/core dist/index.js
check "@oly/web-host packs dist/index.js" lists_file @oly/web-host dist/index.js
check "@oly/core packs no src/" lists_nothing_under @oly/core src/
check "@oly/web-host packs no src/" lists_nothing_under @oly/web-host src/

finish
