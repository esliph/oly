# The launcher committed in the game's repository — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `oly new` writes the version's launcher into the game as `./oly`, `oly upgrade` replaces it, and the global
install (`install.sh`, `self-update`, `~/.oly/bin`) goes away.

**Architecture:** the launcher of a version travels inside its `oly-tools` archive (`<archive>/oly`, stamped). The
`new` task of `tools/oly/Spurfile` copies it into the game; `upgrade_command` in `tools/oly/oly` copies it from
`$OLY_HOME/versions/<version>/oly` over `<game>/oly` through a temporary file. A game is created by piping
`latest/download/oly` into `sh`. The release tests, the consumer tests and the template's CI run the game's own
`sh ./oly`.

**Tech Stack:** POSIX sh, `spur`/`Spurfile`, GitHub Actions, the Docker images of the engine (`./spur …`).

**Spec:** `docs/superpowers/specs/2026-11-23-repo-local-launcher-design.md`

## Global Constraints

- POSIX sh only in `tools/oly/oly`, the `Spurfile`s and `tools/release/*.sh`; run every command through `./spur …` from
  Git Bash (no toolchain on the host).
- The version stamp is `sed "s/^OLY_LAUNCHER_VERSION=.*/OLY_LAUNCHER_VERSION=$version/"`; `./oly` has mode 755.
- The game's `.gitattributes` holds exactly the line `oly text eol=lf`.
- The documented entry is `curl -fsSL https://github.com/esliph/oly/releases/latest/download/oly | sh -s -- new my-game --environment web`.
- The launcher keeps three commands of its own: `new`, `upgrade`, `--version`. `launcher_end` (`# end of oly`) stays,
  and is checked on every launcher copy `upgrade` writes.
- Nothing writes to a shell profile or to the `PATH`. `OLY_HOME` still moves the cache `~/.oly/versions`.
- Where the mode of `./oly` is unknown (template CI, release and consumer tests) it runs as `sh ./oly`.
- No support for releases or games from before this change: no minimum-version guard, no fallback download, no
  migration section.
- `docs/superpowers/` is history: not edited.
- Comments and commit messages in English. Commit subjects short, in the repository's `type(scope): subject` form, with
  no spec, plan or task reference.
- Code style (personal `code-style` skill): blank lines separate steps, not statements; a multi-line control block gets
  one before and after; never after `{` or before `}`; at most one in a row; consecutive guards stay together; no
  whitespace changes in untouched code. Comments only for reason, measurement, external constraint or non-local
  invariant; no narration of the edit; a comment touched by the change remains true.
- Every user-facing doc change follows `docs/reference/contributor/docs-style.md`.

## Review Focus

1. **A pipe whose `curl` fails:** `curl … | sh -s -- new …` with a 404 gives `sh` an empty script, which exits 0 and
   creates nothing. A person expects a failure. The staged consumer test checks that `./oly` exists after the pipe
   (Task 4, Step 1).
2. **A version whose `oly-tools` has no `oly`** (any archive built before Task 3): `oly upgrade` to it must stop with a
   message and leave `./oly`, `oly.env` and `package.json` as they were, not leave a game pinned to a version with
   the old launcher. Test in Task 2 ("version without a launcher").
3. **A cut or broken launcher in the cache:** `upgrade` must keep the old `./oly` whole and leave no `oly.oly-new`.
   Test in Task 2 ("truncated launcher").
4. **The template's `gitattributes` checked out with CRLF on Windows:** without a pin in the engine's own
   `.gitattributes`, a game created in local mode from a Windows checkout gets `oly text eol=lf\r`. Task 1 adds the pin
   and the `new` test reads the line with `grep -qxF`.
5. **Repairing the file that is running:** `./oly upgrade <its own version>` replaces `./oly` while `sh` reads it.
   Test in Task 2 on Linux ("repair from the game's own launcher") and in the staged test, which the `windows` job of
   `release.yml` runs (Task 4).

## Facts checked against the tree at `2c259ae`

Read once while writing this plan, so an executor does not rediscover them:

- `tools/oly/tests/assert.sh` `run_task` runs the `Spurfile` in local mode unless `TEST_ENGINE_DIR` is set (empty =
  pinned); `OLY_TOOLS_DIR` is fixed at `$repository/tools/oly`, which has no `template/`.
- `tools/oly/tests/fake_release.sh` `add_release` packs only `spur` and `Spurfile`; `add_launcher_asset` is used only
  by the `self-update` and `install.sh` cases of `launcher_test.sh`.
- `new` in `tools/oly/Spurfile` rewrites every file of the game through `$file.oly-new` + `mv`, which drops the mode.
- `tools/oly/oly` has no `@NAME@` text, so `new_test.sh`'s `no_placeholder_left` still holds with `./oly` in the game.
- `release.yml`'s `windows` job runs `tools/release/staged_consumer_test.sh` for `web` and `native`, and a `dry_run`
  dispatch may start from a branch other than `master` (`check` job, step "Releases are cut from master").
- The `0.1.0` in `CMakeLists.txt` is the version `./spur stage` and `release-test` use.
- `build/` is ignored; `./spur stage`, `staged-test` and `consumer-test` `rm -rf` their own folders at start. The tests
  below create only `mktemp` folders, removed by their `trap … EXIT`. No step creates a worktree, container or server
  that outlives it.

---

### Task 1: `oly new` writes `./oly` and `.gitattributes`

**Files:**
- Modify: `tools/oly/Spurfile` (task `new`)
- Create: `templates/game/common/gitattributes`
- Modify: `.gitattributes` (engine root)
- Modify: `tools/oly/tests/assert.sh` (`run_task`)
- Test: `tools/oly/tests/new_test.sh`

**Interfaces:**
- Produces: a new game holds `oly` (mode 755, byte for byte the launcher of `_asset oly tools/oly/oly`) and
  `.gitattributes`. `run_task` honours `TEST_TOOLS_DIR` (default `$repository/tools/oly`) for `OLY_TOOLS_DIR`.

- [ ] **Step 1: Let `run_task` take another tools directory**

In `assert.sh`, `OLY_TOOLS_DIR=${TEST_TOOLS_DIR-$repository/tools/oly}`, the same shape as `TEST_ENGINE_DIR`.

- [ ] **Step 2: Write the failing tests in `new_test.sh`**

After the existing native-game checks (local mode):

```sh
check "native game: ./oly is the checkout's launcher" cmp -s "$work/meu-jogo/oly" "$repository/tools/oly/oly"
check "native game: ./oly is executable" test -x "$work/meu-jogo/oly"
check "native game: .gitattributes pins the launcher to LF" has_line "oly text eol=lf" "$work/meu-jogo/.gitattributes"
check "native game: gitattributes is renamed" test ! -e "$work/meu-jogo/gitattributes"
```

A pinned case, with a tools folder laid out as an `oly-tools` archive (`template/` and a launcher stamped `0.1.0`,
written by `sed` so it is mode 644 and `new` must set the bit):

```sh
tools=$work/tools
work_pinned=$work/pinned
mkdir -p "$tools" "$work_pinned"
cp -R "$repository/templates/game" "$tools/template"
sed "s/^OLY_LAUNCHER_VERSION=.*/OLY_LAUNCHER_VERSION=0.1.0/" "$repository/tools/oly/oly" > "$tools/oly"

TEST_ENGINE_DIR= TEST_TOOLS_DIR=$tools run_task "$work_pinned" new pinned-game --environment native
check "pinned game: exit 0" exit_is 0
check "pinned game: ./oly is the version's launcher" cmp -s "$work_pinned/pinned-game/oly" "$tools/oly"
check "pinned game: ./oly is executable" test -x "$work_pinned/pinned-game/oly"
```

`TEST_ENGINE_DIR= TEST_TOOLS_DIR=… run_task` sets both for that call only; `run_task` reads them with `-`, so the
empty `TEST_ENGINE_DIR` means pinned.

- [ ] **Step 3: Run the tests and see the new checks fail**

Run: `./spur oly-tools-test`
Expected: `new_test.sh` prints `FAIL` for the five `./oly` and `.gitattributes` checks above ("gitattributes is
renamed" and "pinned game: exit 0" pass already); the other suites pass.

- [ ] **Step 4: Implement**

- `templates/game/common/gitattributes`: the one line `oly text eol=lf`.
- `.gitattributes` (engine): add `templates/game/common/gitattributes text eol=lf` (Review Focus 4).
- `new` in `tools/oly/Spurfile`: `mv "$name/gitattributes" "$name/.gitattributes"` beside the `gitignore` rename. After
  the placeholder loop (not before: the loop would rewrite the launcher and drop its mode), copy
  `$(_asset oly tools/oly/oly)` to `$name/oly` and `chmod 755` it; a failure is
  `_fail 1 "could not copy the launcher into $name"`. One comment on why the copy follows the loop: `upgrade` compares
  `./oly` byte for byte with the version's launcher.

- [ ] **Step 5: Run the tests**

Run: `./spur oly-tools-test`
Expected: every suite ends without `FAIL`; exit 0.

- [ ] **Step 6: Commit**

```bash
git add .gitattributes templates/game/common/gitattributes tools/oly/Spurfile tools/oly/tests/assert.sh tools/oly/tests/new_test.sh
git commit -m "feat(oly): write the launcher into a new game"
```

---

### Task 2: `oly upgrade` replaces `./oly`; `self-update` goes away

**Files:**
- Modify: `tools/oly/oly` (`upgrade_command`, remove `self_update` and its `case`)
- Modify: `tools/oly/tests/fake_release.sh` (`add_release`)
- Test: `tools/oly/tests/launcher_test.sh`

**Interfaces:**
- Consumes: from Task 1, nothing in code; the same file layout (`<game>/oly`).
- Produces, in `tools/oly/oly`:
  - `replace_launcher <version> <game root>`: copies `$oly_home/versions/<version>/oly` to `<root>/oly.oly-new`,
    checks it (`sh -n`, last line `= "$launcher_end"`), `chmod +x`, `mv` over `<root>/oly`. Missing source:
    `fail 1 "oly-tools <version> has no launcher to copy into <root>"`. Any other failure: removes the temporary file,
    `fail 1 "could not replace <root>/oly with the launcher of <version>"`.
  - `add_release <tree> <version> [truncated|no-launcher]` in `fake_release.sh`: the archive also holds `oly`, the
    repository's launcher stamped `<version>`; `truncated` keeps its first 200 lines, `no-launcher` leaves it out.

- [ ] **Step 1: Give the fake releases a launcher**

`add_release` stamps `$repository/tools/oly/oly` with the version into its staging folder and adds `oly` to the `tar`
list; the third argument as in Interfaces.

- [ ] **Step 2: Write the failing tests in `launcher_test.sh`**

Remove the `self-update` blocks (from `mkdir -p "$work/bin"` to the `self-update broken` checks, and the
`self-update truncated` block with its `head -n 196`). Keep `add_launcher_asset` and its uses in the `install.sh`
blocks: Task 3 removes both. Where a removed block reset `launcher=$original_launcher`, the variable must still name
`$repository/tools/oly/oly` for the cases after it.

Add, in the existing upgrade cases:

```sh
check "upgrade web: ./oly is the version's launcher" cmp -s "$work/web/oly" "$home/versions/0.2.0/oly"
check "upgrade web: ./oly is executable" test -x "$work/web/oly"
check "upgrade native: creates the missing ./oly" cmp -s "$work/native/oly" "$home/versions/0.2.0/oly"
check "upgrade native: ./oly prints its version" test "$(sh "$work/native/oly" --version | head -n 1)" = "oly 0.2.0"
```

"upgrade same version": copy `$home/versions/0.1.0/oly` into `$work/same/oly` and keep a copy before the run; add
`check "upgrade same version: ./oly unchanged" cmp -s …`. "upgrade to a missing version": add a check that
`$work/native/oly` still equals `$home/versions/0.2.0/oly`.

New cases (each `make_game` with `0.1.0`, the version's launcher copied in where the case says so):

| Case | Setup | Checks |
|---|---|---|
| repair from the game's own launcher | `repair` with the 0.1.0 launcher plus `printf '# edited\n' >>`; `launcher=$work/repair/oly` for the run, then back | exit 0; stdout is not `already on 0.1.0`; `./oly` equals `versions/0.1.0/oly`; no `$work/repair/oly.oly-new` |
| lost launcher, same version | `lost`, no `./oly` | exit 0; `./oly` equals `versions/0.1.0/oly` |
| truncated launcher | `add_release "$tree" 0.6.0 truncated`; `cut` with the 0.1.0 launcher, saved copy | exit 1; stderr has `could not replace`; `./oly` equals the saved copy; `OLY_VERSION=0.1.0`; no `oly.oly-new` |
| version without a launcher | `add_release "$tree" 0.7.0 no-launcher`; `old` with the 0.1.0 launcher | exit 1; stderr has `has no launcher`; `./oly` and `oly.env` unchanged |
| self-update is gone | `run_oly "$work/empty" self-update` | exit 64; stderr has `not inside an Oly game` |

- [ ] **Step 3: Run the tests and see them fail**

Run: `./spur oly-tools-test`
Expected: `launcher_test.sh` prints `FAIL` for the new `./oly` checks, "repair", "lost", "truncated", "version without
a launcher" and the stderr check of "self-update is gone" (today it says `runs from a checkout`); `new_test.sh`
passes.

- [ ] **Step 4: Implement in `tools/oly/oly`**

- Delete `self_update` and the `self-update)` case.
- Add `replace_launcher` (Interfaces). It is the shape `self_update` had: the copy is checked before `mv`, so `./oly`
  is never half written and the running shell keeps reading the old file.
- `upgrade_command`: move the "fetched before anything is rewritten" `install_tools` above the "already on" test. The
  test also requires `cmp -s "$oly_home/versions/$target/oly" "$root/oly"`. Then `replace_launcher "$target" "$root"`
  before `rewrite "$root/oly.env"`, so a version without a launcher leaves the game as it was. Update the comments of
  both moved lines so they stay true (the "already on" check now reads the cache).

- [ ] **Step 5: Run the tests**

Run: `./spur oly-tools-test`
Expected: no `FAIL`; exit 0.

- [ ] **Step 6: Commit**

```bash
git add tools/oly/oly tools/oly/tests/fake_release.sh tools/oly/tests/launcher_test.sh
git commit -m "feat(oly): upgrade replaces the game's launcher; drop self-update"
```

---

### Task 3: the release carries the launcher in `oly-tools`; `install.sh` goes away

**Files:**
- Modify: `tools/release/package_tools.sh`
- Modify: `tools/release/stage.sh`
- Modify: `tools/release/tests/package_test.sh`
- Delete: `tools/oly/install.sh`
- Modify: `tools/oly/tests/launcher_test.sh` (remove every `install_oly` case and the custom-`OLY_HOME` install case)
- Modify: `tools/oly/tests/fake_release.sh` (remove `add_launcher_asset`)
- Modify: `.github/workflows/release.yml` (the comment that names `install.sh`, step "Create the release")

**Interfaces:**
- Produces: `oly-tools-<version>-<platform>.tar.gz` holds `oly` at its root, stamped `<version>`, mode 755. The staged
  release holds six assets: `VERSION`, `oly`, `oly-core-<v>.tgz`, both `oly-tools`, `oly-web-host-<v>.tgz`.
  `latest/download/` holds `oly` and `VERSION`.

- [ ] **Step 1: Write the failing checks in `package_test.sh`**

- `expected_listing` adds `oly`.
- The assets check expects the six assets (no `install.sh`); its description says six.
- After extracting both archives:

```sh
check "the archive's oly is the release's launcher" cmp -s "$extracted/linux/oly" "$latest/oly"
check "the archive's oly is executable" test -x "$extracted/linux/oly"
check "the windows-x64 archive's oly is the release's launcher" cmp -s "$extracted/windows/oly" "$latest/oly"
```

- Remove the `latest/download/install.sh is the release's` check.

- [ ] **Step 2: Run and see them fail**

Run: `./spur stage 0.1.0 && ./spur release-test`
Expected: `package_test.sh` fails on the listing, the assets and the three `oly` checks.

- [ ] **Step 3: Implement**

- `package_tools.sh`: write the stamped launcher to `$staging/oly` with the Global Constraints `sed`, `chmod 755` it
  with `spur`, and add `oly` to the `tar` list. Its header comment adds nothing: the archive's root is documented in
  `publishing.md` (Task 5).
- `stage.sh`: remove both `cp … install.sh`; its header and closing comments name the launcher and the pipe of a new
  game instead of `install.sh` and `OLY_INSTALL_VERSION` (`download/v<version>/oly` is what
  `curl …/download/v<version>/oly | sh -s -- new … --version <version>` reads).
- Delete `tools/oly/install.sh`; in `launcher_test.sh` remove `install_home`, `install_oly`, `path_lines` and every case
  that runs `install.sh`; in `fake_release.sh` remove `add_launcher_asset`.
- `release.yml`, step "Create the release": the comment reads that the launcher and the pipe that creates a game read
  `latest/download`.

- [ ] **Step 4: Run the tests**

Run: `./spur oly-tools-test && ./spur stage 0.1.0 && ./spur release-test`
Expected: no `FAIL` in either suite; exit 0. Then `git grep -n "install\.sh" -- tools .github Spurfile templates`
prints only the lines Task 4 rewrites (`Spurfile` `staged-docker`, `staged_consumer_test.sh`, the template `ci.yml`).

- [ ] **Step 5: Commit**

```bash
git add -A tools/release tools/oly .github/workflows/release.yml
git commit -m "feat(release): ship the launcher in oly-tools; drop install.sh"
```

---

### Task 4: the consumers run the game's own `./oly`

**Files:**
- Modify: `tools/release/staged_consumer_test.sh`
- Modify: `Spurfile` (engine root: `staged-docker`, `consumer-native`, `consumer-web`, `consumer-docker`)
- Modify: `templates/game/common/.github/workflows/ci.yml`

**Interfaces:**
- Consumes: Task 1 (`./oly` in a new game), Task 2 (`upgrade` repairs `./oly`), Task 3 (the staged tree has
  `latest/download/oly` and `oly` in `oly-tools`).

- [ ] **Step 1: Rewrite the body of `staged_consumer_test.sh`**

Keep the argument checks, the npm config and `OLY_HOME=$work/home`. Replace everything from the `install.sh` download to
the end with, in this order (`game=$work/staged-$environment`, `latest=$OLY_RELEASE_URL/latest/download/oly`):

1. `(cd "$work" && curl -fsSL "$latest" | sh -s -- new "staged-$environment" --environment "$environment")`, then
   `[ -f "$game/oly" ] || fail 1 …` (Review Focus 1: an empty pipe exits 0).
2. `sh ./oly --version` from the game prints `oly $version` on its first line, and `cmp -s "$game/oly"
   "$OLY_HOME/versions/$version/oly"`.
3. `(cd "$game" && sh ./oly doctor)` and `(cd "$game" && sh ./oly test)`, each with its `fail 1` message.
4. The existing checks of `$OLY_HOME/versions/$version/spur` and, for web, `.oly-install` and `CMakeCache.txt`.
5. The release folder's launcher: `curl -fsSL "$OLY_RELEASE_URL/download/v$version/oly" | sh -s -- new
   "pinned-$environment" --environment "$environment" --version "$version"` from `$work`, then
   `cmp -s "$work/pinned-$environment/oly" "$game/oly"`.
6. The repair while running: `printf '# edited\n' >> "$game/oly"`, `(cd "$game" && sh ./oly upgrade "$version")`, then
   `cmp -s "$game/oly" "$OLY_HOME/versions/$version/oly"`.

The header comment says what the run does now (creates the game through the pipe, runs the game's `./oly`, repairs
it); drop the sentence about `install.sh`.

- [ ] **Step 2: The root `Spurfile` and the template CI**

- `staged-docker`: remove the `profile` folder, the `install.sh` download and run; keep the exports (`OLY_HOME` is the
  cache, `OLY_RELEASE_URL` reaches the tools). The three runs become `(cd "$native" && sh ./oly test --docker)`, the same
  in `$web`, and `(cd "$native" && sh ./oly test --docker --platform windows)`.
- `consumer-native` and `consumer-web`: keep `sh "$launcher" new …`; the test run becomes `(cd <game> && sh ./oly test)`,
  preceded by `cmp -s <game>/oly "$launcher" || _fail 1 "<game>/oly is not the checkout's launcher"`.
- `consumer-docker`: the three runs become `sh ./oly test --docker …` from each game; drop `launcher=`.
- `templates/game/common/.github/workflows/ci.yml`: remove the "Install oly" step; the remaining step runs
  `sh ./oly test --docker`.

- [ ] **Step 3: Run the staged and consumer tests**

Run: `./spur stage 0.1.0 && ./spur staged-test 0.1.0`
Expected: web, native and docker finish with exit 0; `build/staged/web/staged-web/oly` and
`build/staged/native/pinned-native/oly` exist.

Run: `./spur consumer-test`
Expected: exit 0.

- [ ] **Step 4: Commit**

```bash
git add tools/release/staged_consumer_test.sh Spurfile templates/game/common/.github/workflows/ci.yml
git commit -m "test(release): run the game's own launcher in the consumer tests"
```

- [ ] **Step 5: The Windows run (ask the user first)**

The repair of a running `./oly` on Windows is proved only by the `windows` job of `release.yml`. Pushing the branch and
dispatching is outward-facing: ask the user, then
`gh workflow run release.yml --ref <branch> -f version=0.1.0 -f dry_run=true` and watch it with `gh run watch`.
Expected: the `windows` job passes. If it fails on the `mv` of the running file, stop and report: §5.1 of the spec
assumed it works.

---

### Task 5: documentation and changelog

**Files:** every page below; nothing under `docs/superpowers/`.

The rule for commands: a command the reader types (code blocks, "run …") is written `./oly <command>`; a command named
as a thing in prose or in a table column ("`oly upgrade` rewrites …") stays `oly <command>`. Creating a game is the
pipe of the Global Constraints.

- [ ] **Step 1: Reference pages**

- `docs/reference/cli.md`: "Install" keeps the table of host tools and drops "Install the launcher once", the
  `install.sh` block and its paragraph. A new section "Create a game" gives the pipe, says the game then holds `./oly`
  (commit it) and that no step changes the `PATH` or a profile; it states the fix for a repository committed from
  Windows (`git update-index --chmod=+x oly`, once, committed). Commands table: drop `oly self-update`; commands in
  code blocks as `./oly`; `oly upgrade` also replaces `./oly`. Environment variables: `OLY_HOME` is "where `oly` keeps
  the downloaded versions. Default: `~/.oly`"; drop `OLY_INSTALL_VERSION`. Fix the `#install` anchors other pages link
  to (`first-web-game.md`, `first-desktop-game.md`, `set-up-game-ci.md`, `errors.md`).
- `docs/reference/glossary.md`, row "launcher": the `oly` script (POSIX sh) committed at the root of a game as `./oly`.
- `docs/reference/errors.md`: remove the `this launcher runs from a checkout (version dev)` row; add rows for the two
  new `upgrade` messages (`oly-tools <version> has no launcher …`, `could not replace … with the launcher of …`).
- `docs/reference/game-template.md`: rows for `oly` (copied from the version, not from `common/`) and
  `common/gitattributes` → `.gitattributes`; the rename sentence names three renames; the `ci.yml` row runs
  `sh ./oly test --docker` and installs nothing.

- [ ] **Step 2: How-to pages and tutorials**

- `docs/how-to/set-up-game-ci.md`: the job does two things (checkout, `sh ./oly test --docker`); why `sh ./oly`.
- `docs/how-to/upgrade-the-engine.md`: `./oly upgrade` also replaces `./oly`, which arrives as a diff to commit with
  `oly.env` and `package.json`; `./oly upgrade <the version of oly.env>` repairs a lost or edited `./oly`.
- `docs/how-to/build-with-an-engine-checkout.md`: `OLY_ENGINE_DIR=../oly ./oly test`; a game created in local mode
  holds the `dev` launcher until the next `./oly upgrade`.
- `docs/tutorials/first-web-game.md`, `first-desktop-game.md`: create the game with the pipe; then `./oly …`.
- `README.md` quick start: the pipe, then `cd my-game && ./oly test`.
- The other pages that show typed `oly` commands (`docs/README.md`, `docs/how-to/fix-common-problems.md`,
  `test-a-desktop-game.md`, `test-a-web-game.md`, `export-your-own-c-functions.md`, `docs/reference/cmake.md`,
  `docs/explanation/concepts/environments-and-hosts.md`, `docs/explanation/architecture/schema-and-codegen.md`) follow
  the rule above.

- [ ] **Step 3: Explanation and contributor pages**

- `docs/explanation/architecture/publishing.md`: the diagram puts `oly` in the game's repository and removes
  `install.sh` from the release; the release table drops `install.sh` and `oly`'s row says the pipe and the launcher
  read it; the `oly-tools` block adds `oly  the launcher of this version, which oly new and oly upgrade copy`; "The
  launcher" shows `~/.oly/versions/` only, lists `new`, `--version` and `doctor` outside a game, and three commands of
  its own.
- `docs/explanation/architecture/decisions.md`: the launcher row names three commands; the `VERSION` and `oly` row
  drops `self-update` (the pipe and `latest_version` read them); a new row: the launcher is committed in each game |
  a clone works with no step before it and nothing touches the `PATH` or a profile | the file is copied into each game
  and changes only with `oly upgrade`.
- `docs/explanation/architecture/build-and-tests.md`: `staged-test` creates the game through the pipe and runs its
  `./oly`; `oly-tools-test` no longer names `install.sh`.
- `docs/reference/contributor/commands.md`: `oly-tools-test` row; the `stage` list of assets (six, `install.sh` gone,
  `latest/download/` holds `oly` and `VERSION`); `staged-test` as in `build-and-tests.md`.
- `docs/reference/contributor/repository.md`: `oly/` holds the launcher, the game `Spurfile`, `docker-compose.yaml`
  and their shell tests.
- `SECURITY.md`: "the `oly` launcher" without `install.sh`.
- `CHANGELOG.md`, `## Unreleased` → `### Breaking`: the launcher is committed in each game as `./oly` (`oly new`
  writes it, `oly upgrade` replaces it); `install.sh` and `oly self-update` are gone. `### Migration` stays `None.`

- [ ] **Step 4: Verify**

Run: `git grep -nE "install\.sh|self-update|\.oly/bin|OLY_INSTALL_VERSION|launcher once" -- ':!docs/superpowers'`
Expected: no output.

Run: `git grep -n "#install" -- docs README.md`
Expected: every hit names an anchor that exists in `docs/reference/cli.md`.

- [ ] **Step 5: Commit**

```bash
git add -A docs ':!docs/superpowers' README.md SECURITY.md CHANGELOG.md
git commit -m "docs: the launcher lives in the game's repository"
```
