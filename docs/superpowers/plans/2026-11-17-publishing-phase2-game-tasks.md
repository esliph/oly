# Publishing, phase 2 (game tasks, template and local mode) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A game created from `templates/game/` in its own folder builds, tests and runs with the tasks of
`tools/oly/Spurfile`, on host tools or in Docker, against a pinned version or a local engine checkout, and
`./spur consumer-test` proves it from this repository.

**Architecture:** `tools/oly/Spurfile` is the whole game-side toolchain; it is run as
`spur -f <tools>/Spurfile -C <game root> <task>` with environment variables that phase 3's launcher will set. It finds
its assets through one helper that maps an `oly-tools` path to the checkout path in local mode. The template is three
overlays (`common`, `web`, `native`) with `@NAME@` placeholders. The npm step keeps `node_modules` in sync with a marker
file, and in local mode compiles the checkout's TS packages over the installed tarballs without touching
`package.json`. `./spur consumer-test` stages the npm tarballs on a local HTTP server and runs the whole cycle in the
`wasm` and `dev` images, then in Docker mode from the host.

**Tech Stack:** POSIX sh (`spur` 0.3.1), CMake 3.25 + `FetchContent`, Emscripten 3.1.74, npm, TypeScript 5.6.3,
esbuild 0.24.2, node `--test`, SDL3 via the engine, Docker Compose, Python 3 (`http.server`, staging only).

**Spec:** `docs/superpowers/specs/2026-11-17-publishing-design.md` (§5.3, §7, §8.3, §8.4, §9, §10 local run, §11,
§13 step 2). **Depends on:** `docs/superpowers/plans/2026-11-17-publishing-phase1-subproject.md`.

## Global Constraints

- Engine work stays Docker-only (`./spur`); the game tasks run on whatever "host" they are started on, which in this
  phase's tests is the `wasm` or `dev` container.
- A game repository holds no copy of the engine's tasks (spec §2): the template has no `Spurfile`.
- Docker is never needed by `new`, `doctor` and the default `build`, `test` and `run` (spec §8.4).
- `package.json` is not changed by local mode (spec §7).
- `oly build` fails if `oly.env` and `package.json` name different versions (spec §9).
- `OLY_ENGINE_DIR` switches C++, `oly-schema`, TS packages and tasks to the checkout, with no file of the game edited
  (spec §2, §7).
- Template target name: the game name with `-` replaced by `_` (`meu-jogo` → `meu_jogo`); game names match
  `^[a-z][a-z0-9-]*$`.
- Comments and blank lines follow the `code-comments` and `blank-lines` skills; TS in `strict`; C++ follows
  `docs/development/code-style.md` (the clang-format hook formats `.cpp`/`.hpp` under `templates/` too).

## Shared contract (all tasks)

Environment of a `tools/oly/Spurfile` task:

| Variable | Set by | Meaning |
|---|---|---|
| `OLY_TOOLS_DIR` | launcher (phase 3), tests | an extracted `oly-tools` root, or `$OLY_ENGINE_DIR/tools/oly` in local mode |
| `OLY_ENGINE_DIR` | user | local mode: the checkout |
| `OLY_VERSION` | launcher, tests | only for `new`; every other task reads `oly.env` |
| `OLY_RELEASE_URL` | user, tests | release base, default `https://github.com/esliph/oly/releases`; files at `$OLY_RELEASE_URL/download/v<V>/<file>` |
| `OLY_ENGINE_SOURCE_DIR` | release staging (phase 4) | C++ source only, passed as `FETCHCONTENT_SOURCE_DIR_OLY` |
| `OLY_IMAGE_REGISTRY` | tests | image prefix, default `ghcr.io/esliph` |

`oly.env` keys: `OLY_VERSION`, `OLY_ENVIRONMENT` (`web|native`), `OLY_GAME` (target name), `OLY_RUNNER` (`host|docker`).

Asset paths (`_asset <archive path> <checkout path>`): `spur` ↔ `spur`; `toolchain.env` ↔ `toolchain.env`;
`template` ↔ `templates/game`; `cmake/mingw-w64.cmake` ↔ `cmake/toolchains/mingw-w64.cmake`; `bin/oly-schema[.exe]` ↔
built from the checkout into `<game>/build/oly-schema/` (Task 3). The archive paths are the `oly-tools` layout of spec
§4.3 plus `cmake/mingw-w64.cmake`, which phase 4 packages.

Build directories in a game: `build/cmake` (host), `build/cmake-docker` (Docker mode, which sets `OLY_IN_DOCKER=1` in
the container), `build/cmake-windows[-docker]` (`--platform windows`), `build/web/` (web bundle). Build type `Debug`
(a game in development keeps `OLY_ASSERT` on).

## Review Focus

- A game switched from local mode back to the pinned version keeps the checkout's TS packages in `node_modules`, or a
  Windows host reuses `node_modules` installed by Docker (esbuild ships a binary per platform). Expect the next `build`
  to reinstall. Test: Task 3 Step 1 (`sync` after a local `sync` reinstalls and the marker reads `pinned`).
- The same game is built with `--docker` and `--host`: one CMake cache seen from two absolute paths stops the configure.
  Expect both to work side by side. Test: Task 5 Step 1 builds the same game in both modes.
- `FETCHCONTENT_SOURCE_DIR_OLY` stays in the CMake cache after the user unsets `OLY_ENGINE_DIR`, so the "pinned" build
  silently uses the checkout. Expect every configure to pass the variable, empty when not local. Test: Task 2 Step 1
  greps the cache after a non-local configure.
- Git Bash rewrites `/game`-style arguments and passes `/c/...` paths to node and Docker. Expect `cygpath -m` on every
  path given to node, JSON or `docker -v`, and `MSYS_NO_PATHCONV=1` around `docker`. Test: phase 4's Windows staged run
  (no Linux test can exercise MSYS).
- `oly new` with a bad name, an existing folder, or no `--environment` leaves a half-created folder. Expect an error
  before anything is written. Test: Task 1 Step 1.

---

### Task 1: `tools/oly/Spurfile` foundation and `new`

**Files:**
- Create: `tools/oly/Spurfile` (helpers and task `new`)
- Create: `templates/game/common/{oly.env,CMakeLists.txt,game.oly,gitignore,native/movement.cpp,native/systems.hpp}`
- Create: `tools/oly/tests/assert.sh`, `tools/oly/tests/new_test.sh`, `tools/oly/tests/run.sh`
- Modify: `Spurfile` (task `oly-tools-test`, in the `wasm` image: `sh tools/oly/tests/run.sh`)
- Modify: `CMakeLists.txt` (`project(Oly VERSION 0.1.0 ...)`, spec §4.1)

**Interfaces:**
- Produces helpers in `tools/oly/Spurfile`: `_fail <code> <message>`, `_platform` (prints `linux-x64` for `Linux`,
  `windows-x64` for `MINGW*|MSYS*`, else fails `unsupported host <uname -s>: oly runs on Linux and on Windows in Git
  Bash`), `_exe` (`.exe` on Windows, else empty), `_host_path <path>` (`cygpath -m` when `cygpath` exists, else the
  path), `_asset <archive path> <checkout path>`, `_env_value <key>` (reads `oly.env` of the current directory, fails
  `oly.env has no <key>`), `_version_at_least <found> <minimum>` (numeric per dot field, a non-numeric tail such as
  `-ubuntu1` ignored).
- Produces task `new <name> --environment web|native`: needs `OLY_VERSION`; copies `common/` then the environment's
  overlay into `<name>/`, renames `gitignore` → `.gitignore` and `game.oly` → `<name>.oly`, replaces `@GAME_NAME@`,
  `@GAME_TARGET@`, `@OLY_VERSION@`, `@OLY_ENVIRONMENT@`, `@OLY_RELEASE_URL@` in every file; on web, runs `npm install
  --no-audit --no-fund` in `<name>/` to create `package-lock.json` (Task 3 adds the web overlay).
- Template `oly.env`: `OLY_VERSION=@OLY_VERSION@`, `OLY_ENVIRONMENT=@OLY_ENVIRONMENT@`, `OLY_GAME=@GAME_TARGET@`,
  `OLY_RUNNER=host`.
- Template `CMakeLists.txt`: spec §6 shape; reads `OLY_VERSION` from `oly.env` with `file(STRINGS ... REGEX)` and adds
  `oly.env` to `CMAKE_CONFIGURE_DEPENDS`, `FATAL_ERROR "oly.env has no OLY_VERSION"` when absent; `GIT_SHALLOW TRUE`;
  `enable_testing()`; `DIMENSION 2d`. The environment-specific `oly_add_game` and `add_test` come from an
  `include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/game.cmake)` that each overlay provides.
- Template `game.oly`: `tag Player` and `action move: axis2d`. `native/movement.cpp`: `OLY_SYSTEM(movePlayer)` moves
  every `Player`'s `Position` by the `move` axis times `PlayerSpeed = 200.0f` (px/s) times the fixed delta time;
  `native/systems.hpp` declares `void movePlayer(oly::app::SystemContext& context);` for the native shell.

- [ ] **Step 1: Write the failing tests** (`tools/oly/tests/new_test.sh`, each in a fresh `mktemp -d`, running
`sh spur -f tools/oly/Spurfile -C <tmp> new ...` with `OLY_TOOLS_DIR=$PWD/tools/oly OLY_ENGINE_DIR=$PWD OLY_VERSION=0.1.0`)

| Case | Assertion |
|---|---|
| `new meu-jogo --environment native` | exit 0; `meu-jogo/oly.env` has `OLY_GAME=meu_jogo` and `OLY_ENVIRONMENT=native`; `meu-jogo/meu-jogo.oly` exists; `grep -rE '@[A-Z_]+@' meu-jogo` finds nothing; `.gitignore` exists |
| `new Meu_Jogo --environment native` | exit 64, stderr `game name must match [a-z][a-z0-9-]*: Meu_Jogo`, no folder created |
| `new meu-jogo` | exit 64, `--environment is web or native`, no folder |
| `new meu-jogo --environment native` twice | second exits 64, `meu-jogo already exists`, first folder unchanged |
| `new meu-jogo --environment native` without `OLY_VERSION` | exit 64, `OLY_VERSION is not set` |
| `sh spur -f tools/oly/Spurfile platform` on Linux | prints `linux-x64` |

- [ ] **Step 2: Run to see them fail**

Run: `./spur oly-tools-test`
Expected: FAIL — `tools/oly/Spurfile` does not exist.

- [ ] **Step 3: Implement the helpers, `new`, `platform` and the common template**

All validation happens before the first `mkdir`. `sed` uses `|` as delimiter (URLs carry `/`). `platform` prints
`_platform`.

- [ ] **Step 4: Run the tests**

Run: `./spur oly-tools-test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/oly templates/game/common Spurfile CMakeLists.txt
git commit -m "feat(oly): add the game tasks with new and the common template"
```

### Task 2: Native games: `build`, `test`, `run`, `clean`

**Files:**
- Create: `templates/game/native/{cmake/game.cmake,desktop/main.cpp}`
- Modify: `tools/oly/Spurfile` (tasks `build`, `test`, `run`, `clean`, native branch)
- Modify: `Spurfile` (task `consumer-test`, argument `native`, in the `dev` image)
- Create: `tools/oly/tests/configure_test.sh`

**Interfaces:**
- Consumes: Task 1 helpers and `oly.env`.
- Produces: `build`, `test`, `run`, `clean` in `tools/oly/Spurfile`. Native `build`: `cmake -S . -B build/cmake
  -DCMAKE_BUILD_TYPE=Debug -DOLY_BUILD_NATIVE_HOST=ON -DFETCHCONTENT_SOURCE_DIR_OLY=<OLY_ENGINE_DIR or
  OLY_ENGINE_SOURCE_DIR or empty>` (`-G Ninja` when `_platform` is `windows-x64`, spec §8.3), then `cmake --build`.
  `test`: `build`, then `ctest --test-dir build/cmake --output-on-failure`. `run`: `build`, then
  `build/cmake/games/$OLY_GAME$(_exe) "$@"`. `clean`: `rm -rf build`.
- Produces: template `cmake/game.cmake` (native): `oly_add_game(@GAME_TARGET@ ENVIRONMENT native DIMENSION 2d SCHEMA
  ... SOURCES native/*.cpp desktop/*.cpp DEPENDS ecs app input native_host)`, `target_include_directories(... native)`,
  `add_test(NAME @GAME_TARGET@.headless COMMAND @GAME_TARGET@ --frames 120)` with `SDL_VIDEO_DRIVER=dummy`.
- Produces: `desktop/main.cpp` — the native guide's smallest game (`docs/guide/environments/native/getting-started.md`,
  "The smallest game") drawing a 16 px white square per `Player`, with `movePlayer` added to `Phase::Update` and the
  `--frames <n>` option of `games/drift/desktop/main.cpp` (`maxFrames`, `frameSeconds = 1.0 / 60`, usage exit 64).
- Produces: `./spur consumer-test native`: in `dev`, `new consumer-game --environment native` under
  `build/consumer/native/` (deleted first) in local mode, then `test`.

- [ ] **Step 1: Write the failing tests**

`tools/oly/tests/configure_test.sh`: with a stub `cmake` on `PATH` that appends its arguments to a file, run `build` in
a native game created by `new` without `OLY_ENGINE_DIR` and `OLY_ENGINE_SOURCE_DIR`; assert the recorded configure
line contains `-DFETCHCONTENT_SOURCE_DIR_OLY=` followed by a space or the end (empty, so a stale cache entry is
cleared), and with `OLY_ENGINE_DIR=/x` it contains `-DFETCHCONTENT_SOURCE_DIR_OLY=/x`.

Then the integration check: `./spur consumer-test native`.

- [ ] **Step 2: Run to see them fail**

Run: `./spur oly-tools-test` → FAIL (no `build` task). `./spur consumer-test native` → FAIL (no task).

- [ ] **Step 3: Implement the native branch, the native overlay and `consumer-test native`**

- [ ] **Step 4: Run**

Run: `./spur oly-tools-test` → PASS. `./spur consumer-test native` → PASS, ctest prints
`consumer_game.headless ... Passed`.

- [ ] **Step 5: Commit**

```bash
git add tools/oly templates/game/native Spurfile
git commit -m "feat(oly): build, test and run native games"
```

### Task 3: Web games, npm sync and local mode

**Files:**
- Create: `templates/game/web/{cmake/game.cmake,package.json,tsconfig.json,web/tsconfig.json,web/tsconfig.browser.json,web/tests/tsconfig.json,web/src/game.ts,web/src/main.ts,web/index.html,web/tests/smoke.test.ts}`
- Modify: `tools/oly/Spurfile` (web branch of `build`, `test`, `run`; task `sync`; `oly-schema` resolution)
- Modify: `engine/environments/web/clients/typescript/core/package.json`, `engine/environments/web/host/js/package.json`
  (`"files": ["dist"]`; web-host's `"@oly/core"` dependency becomes the exact version `0.1.0`)
- Modify: `package-lock.json` (regenerated in the `wasm` image: `npm install --package-lock-only`)
- Modify: `Spurfile` (`consumer-test web`)
- Create: `tools/oly/tests/pack_test.sh`

**Interfaces:**
- Produces: template `package.json` — `name` `@GAME_NAME@`, `private`, `type: module`, dependencies `@oly/core` and
  `@oly/web-host` at `@OLY_RELEASE_URL@/download/v@OLY_VERSION@/oly-core-@OLY_VERSION@.tgz` and
  `.../oly-web-host-@OLY_VERSION@.tgz`; devDependencies `typescript 5.6.3`, `esbuild 0.24.2`, `@types/node 20.17.6`
  (the engine's own pins).
- Produces: template TS — the shape of the web guide's game (`docs/guide/environments/web/getting-started.md` steps 3,
  7–9), self-contained: root `tsconfig.json` is a solution file referencing the three `web/` configs, which inline the
  engine's `tsconfig.base.json` options (no `extends` into the engine); `game.ts` exports `installGame(engine, drawer)`
  binding WASD, spawning one `Player` with `Position {x: 320, y: 180}`, adding the native `movePlayer` to
  `Phase.Update` and a render system that calls `drawer.square(x, y, 16)`; `main.ts` loads `./@GAME_TARGET@.mjs` on a
  640×360 canvas `game`.
- Produces: `web/tests/smoke.test.ts` — loads the module from `OLY_GAME_MODULE`; with `KeyD` held for 60 steps at
  60 Hz, `Position.x` of the player is `320 + 200 ± 1e-3`; a recording drawer saw one square on the last frame.
- Produces: template `cmake/game.cmake` (web): `oly_add_game(... ENVIRONMENT web ... DEPENDS ecs app capi TYPESCRIPT
  ${CMAKE_CURRENT_SOURCE_DIR}/web/schema.generated.ts)` and `add_test(NAME @GAME_TARGET@.smoke COMMAND ${OLY_NODE}
  --test ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/tests/)` with `OLY_GAME_MODULE=$<TARGET_FILE:@GAME_TARGET@>`.
- Produces: task `sync` — `npm install` when `package-lock.json` is missing or older than `package.json`; else `npm ci`
  when `node_modules/.oly-install` is missing, older than the lock, or does not read `<_platform> <mode>`; then, in
  local mode, the overlay; then writes `node_modules/.oly-install` = `<_platform> local|pinned`.
- Produces: the overlay — for `core` then `web-host`, writes `build/oly-local/tsconfig.<package>.json` that `extends`
  the checkout's `tsconfig.base.json` with `composite: false`, `rootDir` = the package's `src`, `outDir` =
  `node_modules/@oly/<package>/dist`, the package's own `lib`, and for `web-host` `paths: {"@oly/core":
  ["<game>/node_modules/@oly/core/dist/index.d.ts"]}` (module resolution walks up from the checkout's source, which
  must not need the checkout's `node_modules`); empties `node_modules/@oly/<package>`, copies its `package.json`, runs
  `node_modules/.bin/tsc -p`. Every path in the JSON goes through `_host_path`. Nothing is written into the checkout.
- Produces: `oly-schema` resolution — `$OLY_TOOLS_DIR/bin/oly-schema$(_exe)` when not local; in local mode, configure
  the checkout into `build/oly-schema` (`-DOLY_BUILD_TESTS=OFF -DOLY_WERROR=OFF -DCMAKE_BUILD_TYPE=Release`, `-G Ninja`
  on Windows) and build target `oly-schema`; the tool is `build/oly-schema/bin/oly-schema$(_exe)`.
- Produces: web `build` — version check (`package.json` must contain `oly-core-<OLY_VERSION>.tgz` and
  `oly-web-host-<OLY_VERSION>.tgz`, else fail `oly.env pins <V> but package.json has other @oly tarballs: run oly
  upgrade <V>`); `sync`; requires `EMSDK` (else fail `EMSDK is not set: install emsdk <EMSDK_VERSION> and source
  emsdk_env.sh`); configure `build/cmake` with `$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`,
  `-DOLY_SCHEMA_TOOL`, `-DFETCHCONTENT_SOURCE_DIR_OLY`; build target `oly_typescript_schemas`, then all;
  `node_modules/.bin/tsc -b`; esbuild `web/dist/src/main.js` (the flags of the engine `Spurfile`'s `_esbuild`) into
  `build/web/main.js`, plus `web/index.html` and `build/cmake/games/$OLY_GAME.{mjs,wasm}`. `test`: `build` + ctest.
  `run`: `build`, then esbuild `--servedir=build/web --serve=0.0.0.0:8000`.
- Produces: `./spur consumer-test web` (in `wasm`): `npm run build`; `npm pack -w @oly/core -w @oly/web-host` into
  `build/consumer/release/download/v<V>/` (`V` from `project(Oly VERSION)`); `python3 -m http.server 8765 --bind
  127.0.0.1 --directory build/consumer/release` in the background, stopped by a `trap`; `OLY_RELEASE_URL=
  http://127.0.0.1:8765`; `new consumer-game --environment web` in local mode, `test`, then checks that
  `node_modules/.oly-install` reads `linux-x64 local` and `build/web/` holds `index.html main.js consumer_game.mjs
  consumer_game.wasm`.

- [ ] **Step 1: Write the failing tests**

`tools/oly/tests/pack_test.sh`: `npm pack --dry-run --json -w @oly/core -w @oly/web-host` lists `dist/index.js` for
both and no `src/` path.

`consumer-test web` ends with the sync checks, while its staging server still runs: `sync` without `OLY_ENGINE_DIR`
leaves `.oly-install` reading `linux-x64 pinned` and `node_modules/@oly/core/dist/index.js` equal (`cmp`) to the file in
the staged `.tgz`; after `touch package.json`, `sync` rewrites `package-lock.json` (newer mtime).

- [ ] **Step 2: Run to see them fail**

Run: `./spur consumer-test web` → FAIL (no task). `./spur oly-tools-test` → FAIL on the pack listing (no `files`).

- [ ] **Step 3: Implement**

- [ ] **Step 4: Run**

Run: `./spur consumer-test web` → PASS, ctest prints `consumer_game.smoke ... Passed`. Then `./spur oly-tools-test`
→ PASS. Then `./spur games-test` → PASS (the `files` and exact-version changes must not break the workspaces).

- [ ] **Step 5: Commit**

```bash
git add tools/oly templates/game/web Spurfile package-lock.json engine/environments/web/clients/typescript/core/package.json engine/environments/web/host/js/package.json
git commit -m "feat(oly): build, test and serve web games, with local mode for the TS packages"
```

### Task 4: `doctor` and the toolchain checks

**Files:**
- Modify: `tools/oly/Spurfile` (task `doctor`; `build` runs the same checks first)
- Create: `tools/oly/tests/doctor_test.sh`
- Modify: `cmake/tests/toolchain.cmake` (case `cmake_minimum_matches` also reads `templates/game/common/CMakeLists.txt`)

**Interfaces:**
- Consumes: `toolchain.env` through `_asset`, `_version_at_least`.
- Produces: `doctor [--environment web|native]` — inside a game, its environment; outside, both. One line per check,
  `ok`, `missing` or `old`, with the found and needed version and, when not ok, what to install. Checks: `cmake` ≥
  `CMAKE_MINIMUM`; web: `emcc --version` equals `EMSDK_VERSION`, `node`, `npm`; native on Linux: `g++` ≥
  `GCC_MINIMUM`; native on Windows: `g++ -dumpmachine` contains `mingw32` and `g++` ≥ `MINGW_GCC_MINIMUM`; on Windows,
  both environments: `ninja` ≥ `NINJA_MINIMUM`. With `OLY_RUNNER=docker` or `--docker`: only `docker compose version`.
  Exit 1 when any line is not `ok`.

- [ ] **Step 1: Write the failing tests** — with stub executables first on `PATH`:

| Stubs | Expected |
|---|---|
| `g++ -dumpversion` → `13.2.0`, cmake 3.30.5 | exit 1, a line `old` naming `g++ 13.2.0` and `14` |
| `emcc` reporting `3.1.70` | exit 1, `old` (or `untested`) naming `3.1.70` and `3.1.74` |
| no `emcc` | exit 1, `missing emcc` with `install emsdk 3.1.74` |
| all at the `toolchain.env` versions | exit 0, every line `ok` |
| `cmake 3.24.0` | exit 1 |
| `_version_at_least 14.2.0-ubuntu1 14` | true; `1.10.1` vs `1.11` false; `3.1.74` vs `3.1.74` true |

Plus `./spur test -R toolchain.cmake_minimum_matches` fails when the template's literal is edited to 3.24 (check by
hand, then revert).

- [ ] **Step 2: Run to see them fail** — `./spur oly-tools-test` → FAIL.

- [ ] **Step 3: Implement `doctor`; `build` calls it and stops on exit 1.**

- [ ] **Step 4: Run** — `./spur oly-tools-test`, `./spur test -R '^oly\.cmake\.toolchain'`, `./spur consumer-test`
→ PASS (the images pass every check).

- [ ] **Step 5: Commit**

```bash
git add tools/oly cmake/tests/toolchain.cmake
git commit -m "feat(oly): check the host tools against toolchain.env"
```

### Task 5: Docker mode, `--platform windows` and the game's CI

**Files:**
- Create: `tools/oly/docker-compose.yaml`
- Create: `templates/game/common/.github/workflows/ci.yml`
- Modify: `tools/oly/Spurfile` (runner selection, Docker dispatch, `--platform windows`)
- Modify: `Spurfile` (`consumer-test docker`, on the host; no argument runs `native`, `web`, `docker` in that order)
- Modify: `docs/development/commands.md` (`consumer-test`, `oly-tools-test`)

**Interfaces:**
- Produces: runner selection — a leading `--docker` or `--host` argument, else `OLY_RUNNER` of `oly.env`, else `host`.
  `new` and `doctor` without `--docker` never use Docker.
- Produces: `tools/oly/docker-compose.yaml` — services `wasm`, `dev`, `windows`, image
  `${OLY_IMAGE_REGISTRY:-ghcr.io/esliph}/oly-<service>:${OLY_VERSION:?}`, volume `${OLY_GAME_DIR:?}:/game`,
  `working_dir: /game`, `environment: OLY_IN_DOCKER=1`; `wasm` publishes `8000:8000`.
- Produces: the Docker dispatch — `MSYS_NO_PATHCONV=1 docker compose -f "$(_host_path <compose file>)" run --rm
  [--service-ports for web run] [-v "$(_host_path $OLY_ENGINE_DIR):/oly" -e OLY_ENGINE_DIR=/oly -e
  OLY_TOOLS_DIR=/oly/tools/oly] <service> sh <spur> -f <Spurfile> <task> --host <args>`, with `OLY_GAME_DIR` and
  `OLY_VERSION` exported for interpolation; `<spur>` and `<Spurfile>` are `/oly/...` in local mode and `/oly-tools/...`
  otherwise (phase 4 bakes `oly-tools` into the images at `/oly-tools`). Service: web → `wasm`, native → `dev`,
  `--platform windows` → `windows`. Native `run --docker` fails: `a desktop window cannot open in the container: run
  build/cmake-docker/games/<game> on the host`.
- Produces: `--platform windows` (native only) — configure `build/cmake-windows` with the MinGW toolchain file
  (`_asset cmake/mingw-w64.cmake cmake/toolchains/mingw-w64.cmake`); on a Windows host it is the default platform and
  needs no toolchain file. `test` runs ctest (under Wine through the toolchain's emulator in the `windows` image).
- Produces: template `ci.yml` — on push and pull request, `ubuntu-latest`: checkout; `curl -fsSL
  https://github.com/esliph/oly/releases/latest/download/install.sh | sh`; `~/.oly/bin/oly test --docker`.
- Produces: `./spur consumer-test docker` (host): tags `oly-engine-wasm`, `oly-engine-dev` as
  `oly-local/oly-wasm:<V>`, `oly-local/oly-dev:<V>`; with `OLY_IMAGE_REGISTRY=oly-local` and local mode, runs `test
  --docker` in the native and web games left by the two earlier runs (the web game reuses the `node_modules` the `wasm`
  run installed: same platform, so `sync` makes no network call).

- [ ] **Step 1: Write the failing test** — `./spur consumer-test docker`, and in it, after the Docker run, `test --host`
of the native game again inside `dev` (both `build/cmake` and `build/cmake-docker` exist and both pass).

- [ ] **Step 2: Run to see it fail** — `./spur consumer-test docker` → FAIL (no task).

- [ ] **Step 3: Implement.**

- [ ] **Step 4: Run** — `./spur consumer-test` (all three) → PASS; `./spur oly-tools-test` → PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/oly templates/game/common/.github Spurfile docs/development/commands.md
git commit -m "feat(oly): run game tasks in Docker and cross-build native games for Windows"
```
