# Publishing, phase 4 (release pipeline) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A hand-started GitHub Actions workflow tests a version on Linux and Windows exactly as a game will download it
and, only when every check passed, tags the commit and publishes the images, the `oly-tools` archives, the npm tarballs,
the launcher and `install.sh` under that one version.

**Architecture:** Shell scripts in `tools/release/` do every step that can run on a laptop: set and check the version,
build the `oly-tools` archive, stage a release tree with GitHub's URL layout, and run the staged consumer test. The
engine `Spurfile` wraps them (`release`, `stage`, `staged-test`), so the whole pipeline but the publishing runs locally
first. `release.yml` calls the same tasks: a Linux job (images, engine tests, staging, local and staged consumer tests,
Docker mode), a Windows job (staged consumer test on Git Bash with MSYS2 UCRT64, emsdk and Ninja), and a publish job
gated on both.

**Tech Stack:** POSIX sh, GitHub Actions (`workflow_dispatch`, `msys2/setup-msys2`, `mymindstorm/setup-emsdk`,
`actions/setup-node`, `actions/upload-artifact`/`download-artifact`), Docker, GHCR, `gh release`, `npm pack`.

**Spec:** `docs/superpowers/specs/2026-11-17-publishing-design.md` (§4, §10, §13 step 4).
**Depends on:** `docs/superpowers/plans/2026-11-17-publishing-phase3-launcher.md`.

## Global Constraints

- One version for the git tag, `oly-tools`, npm tarballs and images (spec §2). Semver `MAJOR.MINOR.PATCH`; the first is
  `0.1.0` (already in `CMakeLists.txt` and both `package.json` since phase 2).
- Releases from `master` only; the tag `v<V>` is created only at the end, on the tested commit (spec §4.2).
- "A published version whose consumer test did not pass" is an anti-goal: the publish job `needs` both test jobs, and
  nothing before it writes outside the run.
- Release assets: `oly-tools-<V>-linux-x64.tar.gz`, `oly-tools-<V>-windows-x64.tar.gz`, `oly-core-<V>.tgz`,
  `oly-web-host-<V>.tgz`, `install.sh` (spec §4.2), plus `oly` (the launcher) and `VERSION` (phase 3's contract).
- Images: `ghcr.io/esliph/oly-dev:<V>`, `oly-wasm:<V>`, `oly-windows:<V>`, each the engine image plus the linux-x64
  `oly-tools` at `/oly-tools` (`ENV OLY_TOOLS_DIR=/oly-tools`), which phase 2's Docker mode runs.
- `oly-tools` archive layout (spec §4.3 plus one file): `spur`, `Spurfile`, `toolchain.env`, `bin/oly-schema[.exe]`,
  `template/{common,web,native}`, `docker-compose.yaml`, `cmake/mingw-w64.cmake`.
- Publishing is outward-facing: the executor runs the workflow with `dry_run: true` only. Publishing `0.1.0` waits for
  the user's explicit go.
- Comments and blank lines follow the `code-comments` and `blank-lines` skills.

## Review Focus

- The linux-x64 `oly-schema` links a newer glibc or libstdc++ than the user's machine has. Expect it to run on any
  glibc-based x86-64 Linux from the last years. Test: Task 2 Step 1 asserts `ldd` lists only `libc`, `libm` and the
  loader (static libstdc++ and libgcc).
- Bumping `package.json` versions without the root `package-lock.json` makes `npm ci` in `./spur ts` fail on the next
  run. Expect `./spur release` to leave the lock consistent. Test: Task 1 Step 1 (`check_version.sh` also reads the lock).
- `@oly/web-host` in a game resolves `@oly/core` from the public registry instead of the sibling tarball. Expect no
  registry request for `@oly/*`. Test: Task 3 runs `npm` with a user config whose `@oly:registry` is
  `http://127.0.0.1:9` (nothing listens there), so a registry lookup fails the run.
- The tag exists already, or the dispatch ran on another branch. Expect the run to stop before any build. Test: Task 4
  Step 1 (`actionlint` plus a dry run on a branch, which must fail at `check`).
- The images pushed are not the ones tested. Expect the publish job to push the saved, tested images, not a rebuild.
  Test: the Linux job writes the three image IDs into the artifact, and the publish job fails before any push when a
  loaded ID differs (Task 4).

---

### Task 1: Set and check the version

**Files:**
- Create: `tools/release/set_version.sh`, `tools/release/check_version.sh`
- Create: `tools/release/tests/version_test.sh`, `tools/release/tests/run.sh`
- Modify: `Spurfile` (tasks `release <V>` on the host, `release-test` in `dev`)

**Interfaces:**
- Produces: `set_version.sh <V> <root>` — writes `V` into `project(Oly VERSION ...)`, the `version` of
  `@oly/core` and `@oly/web-host`, and web-host's `@oly/core` dependency.
- Produces: `check_version.sh <V> <root>` — exit 64 `version must be MAJOR.MINOR.PATCH: <V>` for a bad form; exit 1
  naming the first file that differs (`project(Oly VERSION) is 0.1.0, expected 0.2.0`, and the same for each
  `package.json` field and the two workspace entries of `package-lock.json`); exit 0 otherwise.
- Produces: `./spur release <V>` (host) — `set_version.sh`, `./spur wasm npm install --package-lock-only --no-audit
  --no-fund`, `check_version.sh`, then `git commit -am "chore(release): <V>"`. No tag (spec §4.1).

- [ ] **Step 1: Write the failing tests** — `version_test.sh` copies the five files into a temp root and asserts:
`check_version.sh 0.1.0` exit 0; `1.0` exit 64; each file edited alone to `0.0.9` gives exit 1 with that file's
message; `set_version.sh 0.2.0` then `check_version.sh 0.2.0` exit 0 except for the lock (the lock is the npm step's
job: expect the lock message).

- [ ] **Step 2: Run to see them fail** — `./spur release-test` → FAIL (no task).

- [ ] **Step 3: Implement.** `sed -E` on the exact lines. `check_version.sh` reads the lock without `node` (the `dev`
image has none): the `"version"` line two lines below each `"engine/environments/web/clients/typescript/core": {` and
`"engine/environments/web/host/js": {` key (`grep -A2`), the shape `package-lock.json` has today (lines 22–28, read
2026-11-17).

- [ ] **Step 4: Run** — `./spur release-test` → PASS; `sh tools/release/check_version.sh 0.1.0 .` → exit 0 on the
real tree.

- [ ] **Step 5: Commit**

```bash
git add tools/release Spurfile
git commit -m "feat(release): set and check the release version"
```

### Task 2: Build the `oly-tools` archives and stage a release

**Files:**
- Create: `tools/release/package_tools.sh`, `tools/release/stage.sh`, `tools/release/Dockerfile.tools`
- Modify: `CMakePresets.json` (configure and build presets `schema-tool-linux`, `schema-tool-windows`)
- Modify: `Spurfile` (`_image_for`: `schema-tool-linux` → `dev`, `schema-tool-windows` → `windows`; task `stage <V>`)
- Create: `tools/release/tests/package_test.sh`

**Interfaces:**
- Produces: preset `schema-tool-linux` — inherits `schema-tool`, `CMAKE_EXE_LINKER_FLAGS=-static-libstdc++
  -static-libgcc`, built in `dev`. Preset `schema-tool-windows` — inherits `base`, the MinGW toolchain file, Release,
  tests, bench, games and native host off, `OLY_WERROR` off; build target `oly-schema` (static through the toolchain).
- Produces: `package_tools.sh <V> <platform> <oly-schema file> <out dir>` → `<out dir>/oly-tools-<V>-<platform>.tar.gz`
  with the Global Constraints layout (`Spurfile` from `tools/oly/Spurfile`, `docker-compose.yaml` from
  `tools/oly/docker-compose.yaml`, `template/` from `templates/game/`, `cmake/mingw-w64.cmake` from
  `cmake/toolchains/`).
- Produces: `stage.sh <V> <out root>` → `<out root>/download/v<V>/` holding every release asset and
  `<out root>/latest/download/` holding `VERSION`, `oly` and `install.sh`; the launcher's `OLY_LAUNCHER_VERSION=dev`
  becomes `<V>`.
- Produces: `./spur stage <V>` (host) — builds both `oly-schema` presets, `npm run build` and `npm pack` in `wasm`, then
  `stage.sh <V> build/release/<V>`.
- Produces: `Dockerfile.tools` — `ARG BASE`, `FROM ${BASE}`, `COPY oly-tools/ /oly-tools/`, `ENV
  OLY_TOOLS_DIR=/oly-tools`; built with the extracted linux-x64 archive as context.

- [ ] **Step 1: Write the failing tests** — `package_test.sh`, after `./spur stage 0.1.0`: `tar -tzf` of each archive
lists exactly the layout (sorted list compared to a fixed list in the test); `bin/oly-schema` in the Linux archive runs
`--help`-free smoke: `oly-schema <tmp>/x.oly --emit cpp=<tmp>` on `tag Player` exits 0; `ldd` of it lists only `libc`,
`libm`, `linux-vdso` and the loader; `objdump -p` of the `.exe` (in `windows`) imports no `libstdc++-6.dll` or
`libgcc_s`; `latest/download/oly` has `OLY_LAUNCHER_VERSION=0.1.0`; `latest/download/VERSION` is `0.1.0`.

- [ ] **Step 2: Run to see them fail** — `./spur stage 0.1.0` → FAIL (no task).

- [ ] **Step 3: Implement.**

- [ ] **Step 4: Run** — `./spur stage 0.1.0` then `./spur release-test` → PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/release CMakePresets.json Spurfile
git commit -m "feat(release): package oly-tools and stage a release tree"
```

### Task 3: Staged consumer test

**Files:**
- Create: `tools/release/staged_consumer_test.sh`
- Modify: `Spurfile` (task `staged-test <V> [web|native|docker]`)
- Modify: `docs/development/commands.md` (`release`, `stage`, `staged-test`, `release-test`)

**Interfaces:**
- Consumes: a staged tree served at `OLY_RELEASE_URL`, `OLY_ENGINE_SOURCE_DIR` = the commit being released (spec §10:
  the tag does not exist yet).
- Produces: `staged_consumer_test.sh <V> <environment> <work dir>` — fresh `OLY_HOME` under `<work dir>`; `curl -fsSL
  $OLY_RELEASE_URL/latest/download/install.sh | sh`; `$OLY_HOME/bin/oly doctor --environment <environment>`;
  `oly new staged-<environment> --environment <environment> --version <V>`; `oly test`. `OLY_ENGINE_DIR` must be unset
  (the script fails if it is set): this run proves the downloaded tools, not the checkout's.
- Produces: `./spur staged-test <V> web` (in `wasm`) and `native` (in `dev`) — serve `build/release/<V>` with
  `python3 -m http.server` as in phase 2, export `NPM_CONFIG_USERCONFIG=<work dir>/npmrc` holding
  `@oly:registry=http://127.0.0.1:9` (the variable name `npm_config_@oly:registry` is not a valid sh identifier), run
  the script. The Windows job does the same.
  `docker` (host) — build `oly-local/oly-<image>:<V>` from `Dockerfile.tools` over the local `cpp_services-*` images,
  then `oly test --docker` with `OLY_IMAGE_REGISTRY=oly-local` in the two games the other runs left.

- [ ] **Step 1: Write the failing test** — the task itself: `./spur staged-test 0.1.0`.

- [ ] **Step 2: Run to see it fail** — FAIL (no task).

- [ ] **Step 3: Implement.**

- [ ] **Step 4: Run** — `./spur stage 0.1.0 && ./spur staged-test 0.1.0` → PASS for `web`, `native`, `docker`.

- [ ] **Step 5: Commit**

```bash
git add tools/release Spurfile docs/development/commands.md
git commit -m "test(release): run the consumer test against a staged release"
```

### Task 4: `release.yml`

**Files:**
- Create: `.github/workflows/release.yml`
- Modify: `docs/development/commands.md` (how to cut a release)

**Interfaces:**
- Consumes: every task above.
- Produces: `workflow_dispatch` inputs `version` (string, required) and `dry_run` (boolean, default `true`).
  Permissions: `contents: write`, `packages: write` only on the publish job; `contents: read` elsewhere.
- Produces jobs:
  - `check` (ubuntu-latest): fails unless `github.ref == 'refs/heads/master'`; `check_version.sh`; fails if `git
    ls-remote --tags origin v<V>` prints anything.
  - `linux` (needs `check`): `./spur images`; `./spur test`, `games-test`, `native-games-test`, `windows-test`;
    `./spur subproject-test`; `./spur stage <V>`; `./spur consumer-test`; `./spur staged-test <V>`; builds the three
    `ghcr.io/esliph/oly-<image>:<V>` images from `Dockerfile.tools`, writes their IDs to `image-ids.txt`, `docker save
    | gzip`; uploads the staged tree, the saved images and `image-ids.txt`.
  - `windows` (needs `linux`, windows-latest, `defaults.run.shell: bash` = Git Bash): `msys2/setup-msys2` with
    `msystem: UCRT64` and `mingw-w64-ucrt-x86_64-gcc`, `-cmake`, `-ninja`, its `bin` prepended to `PATH`; emsdk at the
    `EMSDK_VERSION` read from `toolchain.env` in a prior step; node 20; downloads the staged tree; serves it with
    `python -m http.server`; runs `staged_consumer_test.sh <V> web` and `native` with `OLY_ENGINE_SOURCE_DIR` = the
    checkout.
  - `publish` (needs `linux` and `windows`, `if: ${{ !inputs.dry_run }}`): loads the saved images and fails unless their
    IDs equal `image-ids.txt`; logs in to GHCR with `GITHUB_TOKEN`; pushes the three tags; `git tag v<V> ${{ github.sha }}` and pushes it; `gh
    release create v<V> --target ${{ github.sha }}` with every file of `download/v<V>/` plus `latest/download/VERSION`
    and `oly`.

- [ ] **Step 1: Lint** — `docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/repo" -w /repo rhysd/actionlint:1.7.4`
(Git Bash: `MSYS_NO_PATHCONV=1`). Expected: no output.

- [ ] **Step 2: Commit and merge to `master`** through the usual review (the workflow can be dispatched only from the
default branch's copy).

```bash
git add .github/workflows/release.yml docs/development/commands.md
git commit -m "ci(release): add the release workflow"
```

- [ ] **Step 3: Dry run** — `gh workflow run release.yml -f version=0.1.0 -f dry_run=true`, then `gh run watch`.
Expected: `check`, `linux`, `windows` pass; `publish` skipped. Dispatching on a non-`master` ref fails at `check`.
Stop here and report to the user; publishing `0.1.0` (`dry_run=false`) is their call.
