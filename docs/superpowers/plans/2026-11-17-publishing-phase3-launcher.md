# Publishing, phase 3 (the `oly` launcher and `install.sh`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One `oly` command per machine finds the game it is run in, fetches the `oly-tools` of the version the game
pins, and runs that version's tasks; `install.sh` puts it on the `PATH`.

**Architecture:** `tools/oly/oly` is a POSIX sh launcher that knows only how to locate a game, resolve a version,
download and extract an `oly-tools` archive atomically into `~/.oly/versions/<V>/`, and exec that version's `spur` with
the variables of phase 2's contract. `new`, `upgrade`, `self-update` and `--version` are its own; every other word is a
task of the version's `Spurfile`. With `OLY_ENGINE_DIR` it skips the download and runs the checkout's tools.
`./spur consumer-test` switches to entering through the launcher.

**Tech Stack:** POSIX sh, `curl` and `tar` (what Git Bash has), `spur` 0.3.1.

**Spec:** `docs/superpowers/specs/2026-11-17-publishing-design.md` (§5, §7, §13 step 3).
**Depends on:** `docs/superpowers/plans/2026-11-17-publishing-phase2-game-tasks.md`.

## Global Constraints

- The launcher needs only `sh`, `curl`, `tar` (spec §5.1). No `jq`, no `python`, no `realpath -m`.
- The launcher rarely changes: game logic belongs in the version's `Spurfile` (spec §5.2).
- Release URLs: `$OLY_RELEASE_URL/download/v<V>/<file>` and `$OLY_RELEASE_URL/latest/download/<file>`, default
  `OLY_RELEASE_URL=https://github.com/esliph/oly/releases` (GitHub's own layout, so a staging folder served over HTTP or
  `file://` mimics it). "Latest" is the text asset `VERSION`; the launcher itself is the asset `oly` (phase 4 publishes
  both).
- `OLY_HOME` (default `$HOME/.oly`) is the install root, so tests run in a temporary home.
- Platform names: `linux-x64`, `windows-x64` (phase 2's `_platform`; the launcher has its own copy: it runs before any
  version is present).
- Comments and blank lines follow the `code-comments` and `blank-lines` skills.

## Review Focus

- A download that fails halfway (network drop, 404, corrupted archive) leaves `~/.oly/versions/<V>/` half filled, and
  every later run uses the broken folder. Expect nothing under `versions/<V>` and a message naming the URL. Test: Task 1
  Step 1, cases `missing_version` and `corrupt_archive`.
- `oly` run in a subfolder of the game, or in a folder inside `build/`, runs tasks with the wrong `-C`. Expect the game
  root. Test: Task 1 Step 1, `nested_dir`.
- `self-update` replaced by a truncated download leaves no working `oly`. Expect the old launcher kept. Test: Task 2
  Step 1, `self_update_broken`.
- `install.sh` run twice appends `PATH` to the profile twice. Expect one line. Test: Task 2 Step 1, `install_twice`.
- `oly upgrade` on a native game (no `package.json`) or with the version already pinned fails or edits nothing useful.
  Expect a native upgrade to rewrite `oly.env` only, and the same version to be a no-op with exit 0. Test: Task 2 Step 1.

---

### Task 1: Locate, fetch and run

**Files:**
- Create: `tools/oly/oly`
- Create: `tools/oly/tests/launcher_test.sh`, `tools/oly/tests/fake_release.sh` (builds a fake release tree in a
  temporary folder: `download/v0.1.0/oly-tools-0.1.0-linux-x64.tar.gz` with a stub `spur` that prints `tools=<
  OLY_TOOLS_DIR> version=<OLY_VERSION> args=<"$@">` and an empty `Spurfile`, and `latest/download/VERSION` = `0.1.0`)

**Interfaces:**
- Consumes: phase 2's task environment (`OLY_TOOLS_DIR`, `OLY_ENGINE_DIR`, `OLY_VERSION`, `OLY_RELEASE_URL`).
- Produces: `tools/oly/oly`, first line `#!/bin/sh`, a line `OLY_LAUNCHER_VERSION=dev` that phase 4's packaging
  replaces with the release version.
- Produces: game root = the nearest folder from `$PWD` up that holds `oly.env`; outside a game a game task fails 64
  with `not inside an Oly game: no oly.env in <PWD> or above`.
- Produces: tools of a version: in local mode `OLY_TOOLS_DIR=$OLY_ENGINE_DIR/tools/oly`, runner
  `$OLY_ENGINE_DIR/spur`; else `OLY_TOOLS_DIR=$OLY_HOME/versions/<V>`, runner `$OLY_TOOLS_DIR/spur`. Run as
  `sh <runner> -f "$OLY_TOOLS_DIR/Spurfile" -C <root> <task> <args>` with `OLY_TOOLS_DIR` and `OLY_VERSION` exported,
  through `exec`.
- Produces: fetch — download to `$OLY_HOME/versions/.download-<V>-$$/`, extract there, require `spur` and `Spurfile`,
  then `mv` to `versions/<V>`; on any failure remove the temporary folder and fail 1 with `could not install oly-tools
  <V> from <url>`.
- Produces: `oly --version` — `oly <OLY_LAUNCHER_VERSION>`; inside a game a second line `engine <V> (oly.env)`, or
  `engine <OLY_ENGINE_DIR> (local)` in local mode.
- Produces: `doctor` outside a game runs with the latest version (local mode: the checkout).

- [ ] **Step 1: Write the failing tests** (`launcher_test.sh`; `OLY_HOME` and the release tree in `mktemp -d`,
`OLY_RELEASE_URL=file://<tree>`)

| Case | Setup | Assertion |
|---|---|---|
| `outside_game` | `cd` to an empty folder, `oly build` | exit 64, the "not inside an Oly game" message |
| `nested_dir` | game with `oly.env` `OLY_VERSION=0.1.0`, `cd` into `game/a/b`, `oly build --docker` | stub prints `tools=$OLY_HOME/versions/0.1.0 version=0.1.0 args=-f ... -C <game> build --docker` |
| `cached` | after `nested_dir`, delete the archive from the tree, `oly build` | still exit 0 |
| `missing_version` | `oly.env` pins `9.9.9` | exit 1, message names `.../download/v9.9.9/oly-tools-9.9.9-linux-x64.tar.gz`; `versions/` holds no `9.9.9` and no `.download-*` |
| `corrupt_archive` | release `0.3.0` whose archive is a text file | exit 1, nothing left under `versions/` for `0.3.0` |
| `local_mode` | `OLY_ENGINE_DIR=<repo>`, `oly platform` in a game pinning `9.9.9` | prints `linux-x64`; `versions/` empty |
| `version_outside` | `oly --version` outside a game | `oly dev`, one line |
| `version_inside` | inside a game pinning `0.1.0` | second line `engine 0.1.0 (oly.env)` |

- [ ] **Step 2: Run to see them fail** — `./spur oly-tools-test` → FAIL (no `tools/oly/oly`).

- [ ] **Step 3: Implement `tools/oly/oly`.** Platform: `uname -s`, as phase 2's `_platform`. Errors go to stderr as
`oly: <message>`.

- [ ] **Step 4: Run** — `./spur oly-tools-test` → PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/oly/oly tools/oly/tests
git commit -m "feat(oly): add the version launcher"
```

### Task 2: `new`, `upgrade`, `self-update` and `install.sh`

**Files:**
- Modify: `tools/oly/oly`
- Create: `tools/oly/install.sh`
- Modify: `tools/oly/tests/launcher_test.sh`, `tools/oly/tests/fake_release.sh` (a `0.2.0` release and a `latest`
  `oly` asset whose `OLY_LAUNCHER_VERSION=0.2.0`)
- Modify: `Spurfile` (`consumer-test` enters through `tools/oly/oly` with `OLY_ENGINE_DIR=$PWD`)
- Modify: `docs/development/commands.md`

**Interfaces:**
- Produces: `oly new <name> --environment web|native [--version V]` — `V` defaults to `latest/download/VERSION`, or in
  local mode to the checkout's `project(Oly VERSION)`; strips `--version V` and runs the version's `new` task with
  `-C "$PWD"` and `OLY_VERSION=V`.
- Produces: `oly upgrade <V>` — inside a game; fetches `V`; same version as `oly.env` → prints `already on <V>`, exit
  0; else rewrites `OLY_VERSION=` in `oly.env` and, when `package.json` exists, every `v<old>/oly-core-<old>.tgz` and
  `v<old>/oly-web-host-<old>.tgz` to `<V>`; then runs `V`'s `sync` task when `package.json` exists.
- Produces: `oly self-update` — downloads `latest/download/oly` beside the running launcher as a temporary file,
  requires `sh -n` to pass and the file to contain `OLY_LAUNCHER_VERSION=`, then `chmod +x` and `mv` over the launcher;
  on failure the old file stays and the exit is 1.
- Produces: `install.sh` — `OLY_HOME` default `$HOME/.oly`; downloads `latest/download/oly` (or `download/v$OLY_INSTALL_
  VERSION/oly` when set) to `$OLY_HOME/bin/oly`; adds `export PATH="$OLY_HOME/bin:$PATH"` to `~/.zshrc` when `$SHELL`
  ends in `zsh`, else `~/.bashrc` (Git Bash reads it), only when the file does not already contain `$OLY_HOME/bin`;
  prints `oly installed in <path>; open a new terminal or run: export PATH=...`. Uses `OLY_RELEASE_URL` like the
  launcher.

- [ ] **Step 1: Write the failing tests** (added to `launcher_test.sh`)

| Case | Assertion |
|---|---|
| `new_latest` | outside a game, `oly new demo --environment web` → stub prints `version=0.1.0` and `-C <PWD> new demo --environment web` |
| `new_pinned` | `oly new demo --environment web --version 0.2.0` → `version=0.2.0`, `--version` not passed through |
| `upgrade_web` | game with `oly.env` 0.1.0 and a `package.json` holding both 0.1.0 URLs; `oly upgrade 0.2.0` → both files name 0.2.0 only; stub printed `sync` |
| `upgrade_native` | game without `package.json` → `oly.env` says 0.2.0; stub did not print `sync` |
| `upgrade_same` | `oly upgrade 0.1.0` on a 0.1.0 game → exit 0, `already on 0.1.0`, files unchanged |
| `self_update` | copy the launcher to a temp `bin/oly`, run its `self-update` → `oly --version` prints `oly 0.2.0` |
| `self_update_broken` | the release's `oly` has a syntax error → exit 1, `bin/oly` byte-identical to before |
| `install_fresh` | `HOME=<tmp> SHELL=/bin/bash sh install.sh` → `<tmp>/.oly/bin/oly` executable, `.bashrc` has one PATH line |
| `install_twice` | run it again → still one PATH line |

- [ ] **Step 2: Run to see them fail** — `./spur oly-tools-test` → FAIL.

- [ ] **Step 3: Implement**, then change `consumer-test` to `tools/oly/oly new ... --version <V>` and `tools/oly/oly
test` (and `test --docker`), with `OLY_ENGINE_DIR=$PWD`.

- [ ] **Step 4: Run** — `./spur oly-tools-test` → PASS; `./spur consumer-test` → PASS.

- [ ] **Step 5: Docs** — `commands.md`: running a game task from a checkout through `tools/oly/oly` with
`OLY_ENGINE_DIR`.

- [ ] **Step 6: Commit**

```bash
git add tools/oly Spurfile docs/development/commands.md
git commit -m "feat(oly): create, upgrade and self-update through the launcher, and add install.sh"
```
