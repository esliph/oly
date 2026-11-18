# Publishing, phase 5 (docs) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A reader starts a game in their own repository from the guide alone, and the architecture docs explain the
release, the launcher, local mode and `toolchain.env`.

**Architecture:** The guide's getting-started pages switch from "a folder in `games/` of this repository" to `install.sh`
+ `oly new` in a separate repository; a new CLI page covers every `oly` command, local mode and `--docker`. A new
`docs/architecture/publishing.md` holds the why; the other architecture pages get the facts that changed. Phases 1–4
already added their commands and errors to `commands.md` and `troubleshooting.md`.

**Tech Stack:** Markdown.

**Spec:** `docs/superpowers/specs/2026-11-17-publishing-design.md` (§12, §13 step 5).
**Depends on:** `docs/superpowers/plans/2026-11-17-publishing-phase4-release.md`.

## Global Constraints

- Guide pages follow `docs/development/guide-style.md`: game-agnostic (`<game>`, `Player`, `Position`), simple words,
  active voice, tables for options, a new page goes into `docs/guide/README.md` and its neighbours' navigation lines.
- Glossary names only; new terms enter `docs/architecture/glossary.md` first: `oly-tools`, launcher, pinned version,
  local mode, `toolchain.env`, consumer test, staged release.
- Every command and message quoted in the docs is copied from the code of phases 1–4, not paraphrased.
- The guide says what the reader installs per platform: Linux: `cmake`, GCC ≥ `GCC_MINIMUM`, emsdk `EMSDK_VERSION` for
  web; Windows: Git Bash, MSYS2 UCRT64 (`gcc`, `cmake`, `ninja`), emsdk for web; Docker optional. Values come from
  `toolchain.env` at the time of writing, with a link to the file instead of repeating it where a table allows.

## Review Focus

- A reader on Windows follows the Linux steps and runs `oly` from PowerShell or `cmd`. Expect the page to say Git Bash
  first and the MSYS2 packages by name (spec §2 non-goals). Check: Task 1 Step 3.
- A reader of the old getting-started looks for `./spur serve <game>`. Expect the engine-repository path to stay for
  contributors, in one section, linked from the new pages. Check: Task 1 Step 3.
- Links break when pages move. Expect every relative link in `docs/` to resolve. Check: Task 3 Step 2.
- `CLAUDE.md` still says the engine is only a "study and benchmark project" while it now publishes. Expect the wording
  of spec §12. Check: Task 3.
- The CLI page drifts from `oly --help`-style truth (`oly` prints no help today). Expect the page to list exactly the
  commands of spec §5.3. Check: Task 1 Step 3 compares against `tools/oly/oly` and `tools/oly/Spurfile`.

---

### Task 1: The guide

**Files:**
- Create: `docs/guide/cli.md` (install, `new`, `doctor`, `build`, `test`, `run`, `clean`, `upgrade`, `self-update`,
  `--version`; `--docker`/`--host` and `OLY_RUNNER`; `--platform windows`; local mode with `OLY_ENGINE_DIR`; the game
  repository layout of spec §9; the game's CI)
- Modify: `docs/guide/environments/web/getting-started.md`, `docs/guide/environments/native/getting-started.md` (start
  from `oly new <game> --environment web|native`; the hand-written folder steps become "what `oly new` created"; the
  in-repository path moves to a short "Inside the engine repository" section)
- Modify: `docs/guide/environments/web/testing.md` ("Register the test with ctest": the template already does it)
- Modify: `docs/guide/README.md` (index row for the CLI page; "Start here" unchanged), `docs/guide/cheat-sheet.md`
  (the `oly` commands), `docs/guide/troubleshooting.md` (the launcher's and the game tasks' errors: not inside a game,
  download failure, version mismatch, `EMSDK is not set`, the `doctor` lines)

- [ ] **Step 1: Write `cli.md` and update the pages listed above.**
- [ ] **Step 2: Follow the web page by hand** in a temp folder with the staged release of phase 4
(`OLY_RELEASE_URL` pointing at it) inside the `wasm` image: every command on the page runs and prints what the page says.
- [ ] **Step 3: Check** the Review Focus items for this task: Git Bash and MSYS2 named before the first command; the
"Inside the engine repository" section exists on both pages; the command list equals the `case` arms of
`tools/oly/oly` plus the tasks in `tools/oly/Spurfile` (`grep -E '^[a-z-]+:' tools/oly/Spurfile`).
- [ ] **Step 4: Commit**

```bash
git add docs/guide
git commit -m "docs(guide): start a game in its own repository with oly"
```

### Task 2: Architecture

**Files:**
- Create: `docs/architecture/publishing.md` (versioning, the release pipeline and why the tag comes last, the
  `oly-tools` layout, the launcher and its contract with a version, local mode and its table, `toolchain.env` and the
  configure check, the images with `/oly-tools`, the consumer test's three runs)
- Modify: `docs/architecture/build-and-tests.md` (subproject defaults, `OLY_SCHEMA_TOOL` as input, `subproject-test`,
  `consumer-test`, `staged-test`), `repository.md` (`tools/oly/`, `tools/release/`, `templates/game/`, `toolchain.env`,
  `.github/`), `decisions.md` (one entry per **[author decision]** of the spec that survived implementation, plus the
  plan's own: `OLY_RELEASE_URL` with GitHub's layout, `VERSION` and `oly` assets, `cmake/mingw-w64.cmake` in
  `oly-tools`, emsdk Clang as the `wasm` image's host compiler, `Debug` game builds, the `node_modules/.oly-install`
  marker), `limits.md` (no MSVC, no macOS, Git Bash only, a plugin declared by a game is not supported yet, `FetchContent`
  at a real tag is first exercised by games after publishing), `glossary.md` (the terms of the Global Constraints),
  `README.md` (index row for `publishing.md`)

- [ ] **Step 1: Write and update.**
- [ ] **Step 2: Commit**

```bash
git add docs/architecture
git commit -m "docs(architecture): describe publishing, the launcher and local mode"
```

### Task 3: `CLAUDE.md` and the link check

**Files:**
- Modify: `CLAUDE.md` (structure: `tools/oly/`, `templates/game/`, `toolchain.env`; "study and benchmark project"
  becomes "study and benchmark project, published for the author's own games (`docs/architecture/publishing.md`)";
  `./spur images` replaces `docker compose build`)
- Modify: `docs/architecture/README.md` line 7 with the same wording

- [ ] **Step 1: Update both files.**
- [ ] **Step 2: Link check** — run in `dev`:

```sh
find docs CLAUDE.md README.md -name '*.md' | while read -r page; do
  grep -oE '\]\([^)#]+' "$page" | sed 's/^](//' | grep -vE '^(https?:|mailto:)' | while read -r target; do
    [ -e "$(dirname "$page")/$target" ] || echo "$page -> $target"
  done
done
```

Expected: no output.

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md docs/architecture/README.md
git commit -m "docs: describe Oly as a published engine"
```
