# Oly — the launcher committed in the game's repository — design

Date: 2026-11-23. Written on `ci/artifact-promotion` at `2c259ae`.
Builds on `2026-11-17-publishing-design.md` (the launcher, the release, `oly-tools`), which stays valid except where
this design replaces the global install.

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice during the design,
which the user may review; the rest is a direct consequence of the two.

## 1. Intent

Today a player of the engine installs `oly` once per machine: `curl …/install.sh | sh` puts the launcher in
`~/.oly/bin` and appends a `PATH` line to `~/.bashrc` or `~/.zshrc`. A fresh clone of a game does nothing until that
step has run on the machine, and the template's CI repeats it on every run.

The goal is that the launcher lives **in the game's repository**, committed at its root as `./oly`, and every command
runs from there **[user decision]**. Two reasons, both the user's:

- no change to the `PATH` or to a shell profile;
- a clone of the game works with no step before it: `git clone … && cd game && ./oly test`.

## 2. Goals, non-goals, anti-goals

Goals:

- `oly new` writes `./oly` into the new game, the launcher of the version the game pins.
- A game is created with no install: the launcher is piped from the release
  (`curl -fsSL …/latest/download/oly | sh -s -- new <game> --environment web`).
- `oly upgrade <version>` replaces `./oly` with the launcher of `<version>`, so the launcher follows the engine version
  of `oly.env` and the change arrives as a diff the user commits.
- The template's CI runs the committed launcher; it installs nothing.

Non-goals:

- Moving the downloaded versions into the repository. They stay in the shared cache `~/.oly/versions`, which
  `OLY_HOME` still moves **[user decision]**.
- Changing the tasks of the `Spurfile`, the contract between the launcher and a version (`publishing.md`, "The
  launcher"), local mode or `--docker`.
- A stub that downloads the real launcher (approach B below).

Anti-goals (a failure even if it works):

- Anything that writes to a shell profile or to the `PATH`.
- A clone that needs a step before `./oly` works — including on Linux after the game was committed from Windows, where
  Git does not record the executable bit (§5.5).
- Two ways to install that must be documented, tested in the release and kept coherent (approach C below).

## 3. Approaches

| | A. The launcher itself committed as `./oly` | B. A fixed stub committed, which downloads the launcher | C. A, keeping `install.sh` and `self-update` |
|---|---|---|---|
| What the game holds | the whole launcher (~300 lines of sh) | a few lines that never change | as A |
| Update | `oly upgrade` rewrites `./oly` | none for the stub; the launcher is cached per version | as A, plus `self-update` for the global one |
| Cost | the file is duplicated in each game | one more download and one more versioned artefact, for a launcher that is already thin | two install paths in docs, tests and release |

**Picked: A [user decision].** B adds an indirection with no problem it solves: the launcher already knows only how to
find and fetch a version. C keeps a path neither of the user's two reasons needs, and the engine is `0.x`, so removing
the global install costs little now.

## 4. What changes

### 4.1 The launcher in `oly-tools`

`tools/release/package_tools.sh` adds `oly` to the root of each `oly-tools-<version>-<platform>.tar.gz`, stamped with
the version the same way `stage.sh` stamps `latest/download/oly` today
(`sed "s/^OLY_LAUNCHER_VERSION=.*/OLY_LAUNCHER_VERSION=$version/"`), mode 755. The archive's root contract
(`publishing.md`, "`oly-tools`") gains the line `oly  the launcher of this version, which oly new and oly upgrade copy`.

The launcher of a version therefore reaches a game from the `oly-tools` already in the cache, never from a second
download.

### 4.2 `oly new` writes `./oly`

The `new` task of `tools/oly/Spurfile` copies the launcher into the new game after the template, with mode 755:

- pinned: `$OLY_TOOLS_DIR/oly`;
- local mode: `$OLY_ENGINE_DIR/tools/oly/oly`, version `dev`.

It goes through the existing `_asset` helper (`_asset oly tools/oly/oly`), like the template.

The template gains `common/gitattributes`, renamed to `.gitattributes` by `new` as `gitignore` is, with the line
`oly text eol=lf` (§5.5) **[author decision]**.

### 4.3 Creating a game with no install

The documented entry is the launcher piped from the release:

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/oly | sh -s -- new my-game --environment web
```

The launcher already runs this way: it reads `$0` only in `self-update`, which goes away (§4.5). The same pipe runs
`doctor` outside a game and `upgrade` in a game whose `./oly` was lost (§4.4).

### 4.4 `oly upgrade` replaces `./oly`

After `install_tools "$target"` and before `oly.env` is rewritten, `upgrade_command` replaces `$root/oly` with
`$oly_home/versions/$target/oly`: copied to a temporary file beside it, `chmod +x`, then `mv` over `$root/oly`, so the
file is never half written (the same shape `self_update` uses today). A missing `$root/oly` is created.

The "already on `<version>`" early exit also requires `$root/oly` to equal the version's launcher; otherwise the upgrade
runs, so a game whose `./oly` was lost or edited is repaired by `oly upgrade <its own version>`.

`$root/oly` is the target regardless of which file is running: a pipe or `./oly` itself.

Comparing `./oly` with the version's launcher needs `versions/<target>/oly` in the cache, so the "already on" check may
now download the version where it used to answer offline. Accepted: the same command downloads it anyway when an upgrade
is due.

### 4.5 What goes away

- `tools/oly/install.sh`, its copies in `stage.sh` (`download/v<version>/` and `latest/download/`) and its checks in
  `tools/release/tests/package_test.sh` **[author decision: removed, not kept deprecated; the engine is 0.x and the
  only user is the author]**.
- `oly self-update`: the `self-update` case and `self_update()` in the launcher, its tests in `launcher_test.sh`, its
  row in `docs/reference/errors.md`. The launcher keeps three commands of its own: `new`, `upgrade`, `--version`.
- The comment naming `install.sh` in `.github/workflows/release.yml` (around line 316); the asset glob there drops the
  file by itself.
- `tools/oly/tests/fake_release.sh` puts a stamped `oly` in its fake `oly-tools` archives, so the tests of §4.2 and
  §4.4 have a launcher to copy.
- `launcher_end` stays: it is the check that a piped or copied launcher is whole (`package_test.sh`, §4.4 uses it on the
  copy too) **[author decision]**.

The release still publishes `oly` and `VERSION` in `latest/download/` and `download/v<version>/`: the pipe of §4.3 reads
the first, the launcher reads the second.

### 4.6 The template's CI

`templates/game/common/.github/workflows/ci.yml` drops the "Install oly" step and runs `sh ./oly test --docker`.
`sh ./oly` and not `./oly`: it works whether or not Git kept the executable bit (§5.5).

### 4.7 The release tests

`tools/release/staged_consumer_test.sh` stops downloading `install.sh`. It creates the game through the pipe of §4.3,
against the staged `OLY_RELEASE_URL`, with a fresh `OLY_HOME`, then runs `sh ./oly doctor` and `sh ./oly test` from
inside the game. It checks that `./oly` exists, is the release's launcher (`./oly --version` prints `oly <version>`)
and that the version landed in `$OLY_HOME/versions/<version>`. The second `install.sh` run, with
`OLY_INSTALL_VERSION`, becomes a check that `download/v<version>/oly` piped with `new --version <version>` gives the
same launcher.

To prove §5.1, the test then appends a comment line to `./oly` and runs `sh ./oly upgrade <version>` from the game: the
edited file no longer equals the version's launcher, so the repair runs while `./oly` is the running file. It checks
that `./oly` equals the release's launcher afterwards. This runs in both environments, on Linux and in the `windows`
job **[author decision: the web game pays one extra `sync`]**.

The `staged-docker` task of the root `Spurfile` (around line 510) stops installing the launcher and runs each game's
`sh ./oly test --docker` (and `--platform windows` for the native one).

`consumer-test` (local mode) keeps entering through `sh tools/oly/oly new …` and then runs `sh ./oly test` in the game,
so it exercises the copied launcher too.

### 4.8 Documentation

Every page that names `install.sh`, `self-update`, `~/.oly/bin` or "install the launcher once" changes in the same
change, following `docs/reference/contributor/docs-style.md`:

- `README.md` (quick start), `docs/reference/cli.md` (Install → "Create a game"; commands written `./oly`; `OLY_HOME`
  becomes "where `oly` keeps the downloaded versions"; `OLY_INSTALL_VERSION` removed), `docs/reference/glossary.md`
  (launcher), `docs/reference/errors.md`, `docs/reference/game-template.md` (the new files `oly` and `.gitattributes`),
  `docs/how-to/set-up-game-ci.md`, `docs/how-to/upgrade-the-engine.md` (the launcher changes with the upgrade, as a
  diff to commit), `docs/tutorials/first-web-game.md` and `first-desktop-game.md`;
- `docs/explanation/architecture/publishing.md` (diagram, the release table, "The launcher", `oly-tools` contents) and
  `decisions.md` (the rows on the launcher and on `oly` as a release asset; a new row for this decision with its
  trade-off);
- contributor pages: `commands.md`, `repository.md`, `build-and-tests.md`;
- `SECURITY.md` scope: "the `oly` launcher" without `install.sh`;
- `CHANGELOG.md`: an entry under the next version (the launcher is committed in the game; `install.sh` and
  `self-update` are gone).

The plans and specs under `docs/superpowers/` are history and are not edited.

## 5. Constraints and edge cases

### 5.1 Running the launcher while it is replaced

`oly upgrade` run as `./oly upgrade` replaces the file the shell is reading. `mv` swaps the directory entry, and the
running shell keeps the old file open, which is what `self_update` relies on today. The `windows` job of the release
already runs the staged consumer test on `windows-latest` in Git Bash; §4.7 makes that test run `./oly upgrade` from
the game's own launcher, so the Windows behaviour is tested, not assumed.

### 5.2 Local mode

`OLY_ENGINE_DIR=../oly ./oly test` keeps working: the game's launcher, pinned or `dev`, defers to the checkout's
`spur` and `Spurfile`. `oly new` in local mode writes the `dev` launcher; the next `oly upgrade` replaces it with a
release's.

### 5.3 Releases and games from before this change

None are supported **[user decision]**: the author is the only user, so `v0.1.0` is treated as if it had not been
published, and the next release is cut with this change in it. There is no minimum-version guard, no fallback download
of `download/v<version>/oly`, and no migration in `upgrade-the-engine.md`. A version whose `oly-tools` has no `oly` is
simply not a version the launcher is tested against. Whether the next release recuts `0.1.0` or is `0.2.0` belongs to
cutting the release, not to this design.

### 5.4 The cache

`~/.oly/versions/<version>` is created on first use by `install_tools`, unchanged. It is a cache, not an install: no
file outside it is written and deleting it only costs a download.

### 5.5 The executable bit and the line endings

Git for Windows defaults to `core.autocrlf=true`, so without a pin a game cloned on Windows gets `./oly` with CRLF and
`sh` fails on the `\r`. The engine pins its own launcher for this reason (`tools/oly/oly text eol=lf` in its
`.gitattributes`); the game's `.gitattributes` does the same (§4.2). With the pin, the working copy is LF and the
`cmp` of §4.4 compares like with like.

Git Bash on Windows commits files with `core.fileMode=false`, so a game committed from Windows reaches a Linux clone
with `./oly` as mode 644. Everything this design writes invokes the launcher as `sh ./oly` where it cannot know the mode
(the template CI, the release tests). The docs write `./oly` for the interactive case and state, in `cli.md`, the one
fix for a repository committed from Windows: `git update-index --chmod=+x oly` once, committed **[author decision:
document the fix rather than have `oly new` run `git init` to set the mode; `oly new` does not create the repository
today and this design does not change that]**.

## 6. Left to the builder

- The order of the tasks and the commits.
- The exact wording of the docs, within `docs-style.md`.
- Whether `upgrade` compares `./oly` with the version's launcher by `cmp` or by its `OLY_LAUNCHER_VERSION` line (§4.4);
  `cmp` also catches a hand edit.
- New cases in `tools/oly/tests/` (`new_test.sh`: the game has an executable `./oly` of the version; `launcher_test.sh`:
  `upgrade` writes, replaces and repairs `./oly`, keeps it whole on a failed copy, and creates it when missing), added
  with the `add-test-case` skill where it applies.
