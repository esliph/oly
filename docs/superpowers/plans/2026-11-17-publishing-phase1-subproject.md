# Publishing, phase 1 (engine as a subproject and `toolchain.env`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A CMake project outside this repository fetches the engine with `FetchContent` and builds a `web` or `native`
game with `oly_add_game`, and one file, `toolchain.env`, holds the toolchain versions that the Dockerfiles and the
configure check read.

**Architecture:** `cmake/OlyToolchain.cmake` reads `toolchain.env` and checks the compiler, Emscripten, CMake and Ninja
against it at configure time; its check function takes every input as an argument, so script-mode tests drive it like
`cmake/tests/dimension.cmake`. The Dockerfiles take the versions as build arguments that Compose fills from
`toolchain.env`. The engine's defaults depend on `PROJECT_IS_TOP_LEVEL`, `oly_add_game` derives the environment from
`EMSCRIPTEN` and sets its own web link options, so the game's directory scope needs nothing from the engine's. A
consumer fixture in `cmake/tests/consumer/` and the task `./spur subproject-test` prove the path from outside.

**Tech Stack:** CMake 3.25 (`FetchContent`, `PROJECT_IS_TOP_LEVEL`, script mode), Docker Compose build args, POSIX sh
(`Spurfile`), Emscripten 3.1.74, GCC 14.

**Spec:** `docs/superpowers/specs/2026-11-17-publishing-design.md` (§6, §8.1, §8.2, §13 step 1).

## Global Constraints

- The engine repository stays Docker-only through `./spur` (spec §2 anti-goal). Every existing test passes after each
  task: `./spur test`, `./spur games-test`, `./spur native-games-test`, `./spur windows-test`.
- Branch `feat/publish` from `develop` (the plugins project is merged there).
- "A version written in two places that can diverge without an error" is an anti-goal: every copy of a version is either
  read from `toolchain.env` or checked against it by a test.
- `toolchain.env` values: `EMSDK_VERSION=3.1.74`, `GCC_VERSION=14`, `GCC_MINIMUM=14`, `MINGW_GCC_MINIMUM=14`,
  `CMAKE_VERSION=3.30.5`, `CMAKE_MINIMUM=3.25`, `NINJA_MINIMUM=1.11`. The minimums equal the versions the images test
  with; the comment in the file says so and that they were not bisected.
- Spec §8.2: Emscripten must equal `EMSDK_VERSION`, else the configure fails naming both; `-DOLY_ALLOW_UNTESTED_TOOLCHAIN=ON`
  turns that error into a warning. GCC, MinGW, CMake and Ninja are minimums, always errors.
- Spec §6, as a subproject: `OLY_BUILD_TESTS`, `OLY_BUILD_BENCH`, `OLY_BUILD_GAMES` off; no boundary lint; `OLY_SANITIZE`
  adds no global flags; `OLY_WERROR` on only at top level; `OLY_SCHEMA_TOOL` is an input.
- Comments and blank lines follow the `code-comments` and `blank-lines` skills; CMake and sh have no formatter here.

## Review Focus

- A web game built as a subproject has no `-sALLOW_MEMORY_GROWTH=1`: that flag is a directory-scope link option of the
  engine today, and the game target lives in the game's directory. Expect a web game to grow its memory like `arena`.
  Test: Task 3 Step 1 asserts the flag in the consumer's link command.
- The game's own warnings become errors because `OLY_WERROR` defaults on. Expect warnings, not a failed build. Test: the
  consumer fixture has one deliberate warning (Task 3 Step 1).
- `cmake_minimum_required(VERSION 3.25)` is a literal that cannot read `toolchain.env`. Expect the literal and
  `CMAKE_MINIMUM` to never drift. Test: `oly.cmake.toolchain.cmake_minimum_matches` (Task 1).
- `docker compose build` run by hand, without `toolchain.env`, builds an image with an empty version. Expect an error
  naming the missing variable. Test: Task 2 Step 4.
- A subproject configure fetches doctest, benchmark or adds the engine's ctest tests. Expect none of them. Test: Task 3
  Step 1 checks `_deps/doctest-src` is absent and `ctest -N` lists no `ecs.` test.

---

### Task 1: `toolchain.env` and the configure check

**Files:**
- Create: `toolchain.env`
- Create: `cmake/OlyToolchain.cmake`
- Create: `cmake/tests/toolchain.cmake`
- Create: `cmake/tests/toolchain/fixture.env`
- Modify: `CMakeLists.txt` (include, call the check, register the script tests next to `dimensionCases`)
- Modify: `cmake/OlyOptions.cmake` (option `OLY_ALLOW_UNTESTED_TOOLCHAIN`, default `OFF`)
- Modify: `Dockerfile.wasm` (host compiler: `ENV CC=clang CXX=clang++`, with a `clang` link beside the existing
  `clang++` one)
- Modify: `docs/guide/troubleshooting.md` (the four new configure errors)

**Interfaces:**
- Produces: `toolchain.env` at the engine root, `KEY=VALUE` lines, `#` comments, keys of the Global Constraints.
- Produces: `oly_read_toolchain(<file>)` — sets `OLY_TOOLCHAIN_<KEY>` in the caller's scope for each line; a missing
  required key is `FATAL_ERROR "<file> has no <KEY>"`.
- Produces: `oly_check_toolchain(FILE <file> COMPILER_ID <id> COMPILER_VERSION <v> CMAKE_VERSION <v>
  [EMSCRIPTEN_VERSION <v>] [TARGET_WINDOWS] [NINJA_VERSION <v>] [ALLOW_UNTESTED])` — pure: no variable read except its
  arguments.
- Produces: `oly_check_build_toolchain()` — collects the real values (`EMSCRIPTEN_VERSION`, `CMAKE_CXX_COMPILER_ID`,
  `CMAKE_CXX_COMPILER_VERSION`, `CMAKE_SYSTEM_NAME STREQUAL Windows`, `CMAKE_VERSION`, `<CMAKE_MAKE_PROGRAM> --version`
  when `CMAKE_GENERATOR MATCHES Ninja`, `OLY_ALLOW_UNTESTED_TOOLCHAIN`) and calls `oly_check_toolchain` with
  `${PROJECT_SOURCE_DIR}/toolchain.env`, added to `CMAKE_CONFIGURE_DEPENDS`.

- [ ] **Step 1: Write the failing script tests**

`cmake/tests/toolchain/fixture.env` holds the Global Constraints values. `cmake/tests/toolchain.cmake` includes
`OlyToolchain.cmake`, runs one `CASE` and prints `toolchain ok` after a check that did not stop. Register them in
`CMakeLists.txt` the same way as `dimensionCases` (`oly.cmake.toolchain.<case>`, `PASS_REGULAR_EXPRESSION`):

| Case | Call | Expected output |
|---|---|---|
| `emscripten_tested` | `EMSCRIPTEN_VERSION 3.1.74`, `COMPILER_ID Clang` | `toolchain ok` |
| `emscripten_untested` | `EMSCRIPTEN_VERSION 3.1.70` | `Emscripten 3\.1\.70 is not the tested version 3\.1\.74` |
| `emscripten_untested_allowed` | the same with `ALLOW_UNTESTED` | `untested Emscripten 3\.1\.70.*toolchain ok` (warning, then continues) |
| `gcc_at_minimum` | `GNU 14.2.0` | `toolchain ok` |
| `gcc_below` | `GNU 13.2.0` | `GCC 13\.2\.0 is older than the minimum 14` |
| `mingw_below` | `GNU 13.2.0 TARGET_WINDOWS` | `MinGW GCC 13\.2\.0 is older than the minimum 14` |
| `clang_unchecked` | `Clang 15.0.0`, no Emscripten | `toolchain ok` |
| `ninja_below` | `GNU 14.2.0 NINJA_VERSION 1.10.1` | `Ninja 1\.10\.1 is older than the minimum 1\.11` |
| `cmake_below` | `CMAKE_VERSION 3.24.0` | `CMake 3\.24\.0 is older than the minimum 3\.25` |
| `missing_key` | a temporary file without `NINJA_MINIMUM` | `has no NINJA_MINIMUM` |
| `real_file` | `oly_read_toolchain` on `${PROJECT_SOURCE_DIR}/toolchain.env` | `toolchain ok` |
| `cmake_minimum_matches` | reads the `cmake_minimum_required(VERSION x)` literal of the root `CMakeLists.txt` | `toolchain ok`, else `cmake_minimum_required says x, toolchain.env says y` |

Every error message ends with `(toolchain.env)`. The Emscripten error ends with `: install emsdk 3.1.74, or configure
with -DOLY_ALLOW_UNTESTED_TOOLCHAIN=ON`.

- [ ] **Step 2: Run them to see them fail**

Run: `./spur test -R '^oly\.cmake\.toolchain\.'`
Expected: FAIL — `OlyToolchain.cmake` and `toolchain.env` do not exist.

- [ ] **Step 3: Write `toolchain.env` and `cmake/OlyToolchain.cmake`**

Versions compare with `VERSION_LESS` / `VERSION_EQUAL`. The Emscripten branch replaces the compiler check (the
compiler id there is Clang, versioned by emsdk). Ninja's version is the first line of `--version`. In `CMakeLists.txt`,
call `oly_check_build_toolchain()` right after the includes. Before writing the Emscripten branch, confirm the toolchain
file sets the variable: `./spur wasm grep -n EMSCRIPTEN_VERSION /emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`.
The script tests get the repository root as `-DROOT=${PROJECT_SOURCE_DIR}` next to `-DCASE`.

The `wasm` image builds `oly-schema` (preset `schema-tool`, and the games' local mode in phase 2) with the default
`/usr/bin/c++`, which is Ubuntu's GCC 11.4 (`build/schema-tool/CMakeFiles/3.30.5/CMakeCXXCompiler.cmake`, read
2026-11-17): below `GCC_MINIMUM`, so the check would stop it. `Dockerfile.wasm` makes emsdk's Clang the host compiler
(`ENV CC=clang CXX=clang++`), which the `native-clang*` presets already test; the comment there says why. Rebuild
the image before Step 4 (`docker compose build wasm`; Task 2 turns this into `./spur images`) and delete `build/schema-tool` so its cache picks the new compiler.

- [ ] **Step 4: Run the script tests and the full suites**

Run: `./spur test -R '^oly\.cmake\.'` then `./spur test`, `./spur games-test`, `./spur windows-test`
Expected: all PASS (the images already match `toolchain.env`), and `grep CMAKE_CXX_COMPILER_ID
build/schema-tool/CMakeFiles/*/CMakeCXXCompiler.cmake` says `Clang`.

- [ ] **Step 5: Add the four errors to `docs/guide/troubleshooting.md`**, each with its fix (install the named version;
for Emscripten also the escape option), following `docs/development/guide-style.md`.

- [ ] **Step 6: Commit**

```bash
git add toolchain.env cmake/OlyToolchain.cmake cmake/OlyOptions.cmake cmake/tests/toolchain.cmake cmake/tests/toolchain CMakeLists.txt Dockerfile.wasm docs/guide/troubleshooting.md
git commit -m "feat(build): check the toolchain against toolchain.env at configure"
```

### Task 2: The images read `toolchain.env`

**Files:**
- Modify: `Dockerfile`, `Dockerfile.wasm`, `Dockerfile.windows`, `docker-compose.yaml`
- Modify: `Spurfile` (`_in` passes `--env-file toolchain.env`; new task `images`)
- Modify: `docs/development/commands.md`, `docs/architecture/build-and-tests.md` (toolchain section)

**Interfaces:**
- Consumes: `toolchain.env` (Task 1).
- Produces: build args `GCC_VERSION` (dev: `FROM gcc:${GCC_VERSION}`), `EMSDK_VERSION` and `CMAKE_VERSION` (wasm:
  `FROM emscripten/emsdk:${EMSDK_VERSION}`, `pip install cmake==${CMAKE_VERSION}`), `MINGW_GCC_MINIMUM` (windows). No
  `ARG` has a default.
- Produces: `./spur images` — `docker compose --env-file toolchain.env build "$@"`, on the host.

- [ ] **Step 1: Write the failing check**

Run: `docker compose --env-file toolchain.env build --build-arg MINGW_GCC_MINIMUM=99 windows`
Expected today: the build succeeds — there is no check yet. It must fail after Step 3.

- [ ] **Step 2: Add the args.** In `docker-compose.yaml`, every service's `build.args` uses `${NAME:?set in
toolchain.env; build with ./spur images}`. `_in` runs `docker compose --env-file toolchain.env run ...`.

- [ ] **Step 3: Make `Dockerfile.windows` fail below the minimum** with a `RUN` after the apt install that compares
`x86_64-w64-mingw32-g++-posix -dumpversion` to `MINGW_GCC_MINIMUM` with `dpkg --compare-versions ... ge` and exits 1
printing `MinGW GCC <found> is older than MINGW_GCC_MINIMUM <minimum> (toolchain.env)`. It installs no MinGW from
elsewhere (spec §8.2).

- [ ] **Step 4: Verify**

Run: the Step 1 command — Expected: FAIL with the message of Step 3.
Run: `docker compose build windows` (no `--env-file`) — Expected: FAIL naming `MINGW_GCC_MINIMUM` (the `:?` text).
Run: `./spur images` then `./spur test`, `./spur games-test`, `./spur windows-test` — Expected: PASS.
Run: `./spur dev g++ -dumpversion` → `14`; `./spur wasm emcc --version` → first line contains `3.1.74`.

- [ ] **Step 5: Docs.** `commands.md`: `./spur images` replaces every `docker compose build`; `build-and-tests.md`: the
images take their versions from `toolchain.env`.

- [ ] **Step 6: Commit**

```bash
git add Dockerfile Dockerfile.wasm Dockerfile.windows docker-compose.yaml Spurfile docs/development/commands.md docs/architecture/build-and-tests.md
git commit -m "build(docker): take the toolchain versions from toolchain.env"
```

### Task 3: The engine as a CMake subproject

**Files:**
- Create: `cmake/tests/consumer/CMakeLists.txt`, `cmake/tests/consumer/consumer.oly`, `cmake/tests/consumer/native/main.cpp`
- Modify: `cmake/OlyOptions.cmake`, `cmake/OlyGame.cmake`, `cmake/OlyPlugin.cmake` (`oly_schema_tool` message),
  `CMakeLists.txt` (plugins condition, manifest and lint only at top level)
- Modify: `Spurfile` (task `subproject-test`)
- Modify: `docs/development/commands.md`, `docs/guide/troubleshooting.md` (the new `oly_schema_tool` message)

**Interfaces:**
- Consumes: `oly_check_build_toolchain()` (Task 1) runs in the subproject too.
- Produces: `oly_environment(<outputVariable>)` in `OlyGame.cmake` — `web` when `EMSCRIPTEN`, else `native`;
  `_oly_add_game_target` compares `ENVIRONMENT` against it, and `OlyOptions.cmake` sets `OLY_ENVIRONMENT` through it for
  the engine's own scope.
- Produces: a game sets `FETCHCONTENT_SOURCE_DIR_OLY=<checkout>` to build against a checkout, and `OLY_SCHEMA_TOOL=<path>`
  for a web game. A native game builds the `oly-schema` target of the subproject (`oly_schema_tool` prefers the target).
- Produces: `./spur subproject-test [native|web]` (no argument: both, from the host).

- [ ] **Step 1: Write the consumer fixture and the task (the failing test)**

`cmake/tests/consumer/CMakeLists.txt` is the shape of spec §6: `cmake_minimum_required(VERSION 3.25)`,
`project(OlyConsumer LANGUAGES CXX C)`, `FetchContent_Declare(oly GIT_REPOSITORY https://github.com/esliph/oly.git
GIT_TAG v0.0.0)` (never fetched: the test always sets `FETCHCONTENT_SOURCE_DIR_OLY`), then `oly_add_game(consumer
ENVIRONMENT <web when EMSCRIPTEN, else native> DIMENSION 2d SCHEMA consumer.oly ...)` with `DEPENDS ecs app capi` on web
and `SOURCES native/main.cpp DEPENDS ecs app` on native. `consumer.oly` is `tag Player`. `native/main.cpp` calls
`setUpGame`, spawns `Player{}, Position{1.0f, 2.0f}`, prints `consumer ok` and returns 0; it also holds one unused local
variable with a comment saying the warning is there on purpose to prove that a subproject does not add `-Werror` to the
game (spec §6).

`subproject-test` in `Spurfile`:
- `native` (`_in dev`): configure `cmake/tests/consumer` into `build/subproject-native` with
  `-DFETCHCONTENT_SOURCE_DIR_OLY=$PWD`; build with output tee'd to a log; fail unless the log contains
  `unused variable`; run `build/subproject-native/games/consumer` and expect `consumer ok`; fail if
  `build/subproject-native/_deps/doctest-src` exists or `ctest --test-dir build/subproject-native -N` lists a test.
- `web` (`_in wasm`): `spur schema-tool`, then configure into `build/subproject-web` with the Emscripten toolchain file
  and `-DOLY_SCHEMA_TOOL=$PWD/build/schema-tool/bin/oly-schema`; build with `--verbose` tee'd to a log; fail unless the
  link line of `consumer` contains `-sALLOW_MEMORY_GROWTH=1`; then
  `node --input-type=module -e 'const m = await import(process.argv[1]); await m.default(); console.log("consumer ok")'`
  on `build/subproject-web/games/consumer.mjs` (absolute path as a `file://` URL).
- no argument, on the host: `spur subproject-test native && spur subproject-test web`.

- [ ] **Step 2: Run it to see it fail**

Run: `./spur subproject-test native`
Expected: FAIL — `consumer is a native game, but this build is ` (the engine's `OLY_ENVIRONMENT` does not reach the
game's scope).

- [ ] **Step 3: Implement the subproject defaults**

- `OlyOptions.cmake`: `OLY_BUILD_TESTS` defaults to `${PROJECT_IS_TOP_LEVEL}`; `OLY_BUILD_BENCH` and `OLY_BUILD_GAMES`
  stay `OFF`; `OLY_WERROR` defaults to `${PROJECT_IS_TOP_LEVEL}`; the `OLY_SANITIZE` global flags apply only when
  `PROJECT_IS_TOP_LEVEL` (as a subproject the option is ignored: global flags would instrument the game's whole build).
- `CMakeLists.txt`: add `plugins/` also when `NOT PROJECT_IS_TOP_LEVEL` (a web game needs `essentials2d`); the manifest
  and `oly.lint.boundaries*` only when `PROJECT_IS_TOP_LEVEL`.
- `OlyGame.cmake`: `oly_environment`; the web branch adds `-sALLOW_MEMORY_GROWTH=1` to the game's link options (the
  global one is the engine's directory scope, which does not reach the game's directory).
- `oly_schema_tool`: when not top level, the error says `pass -DOLY_SCHEMA_TOOL=<path to oly-schema>`; at top level it
  keeps today's text.
- No new path relative to the engine root in anything a game calls (spec §6): check with
  `grep -n 'PROJECT_SOURCE_DIR\|CMAKE_SOURCE_DIR' cmake/OlyGame.cmake cmake/OlyPlugin.cmake` — `OLY_SCHEMA_TOOL`'s
  default may keep it, nothing else.

- [ ] **Step 4: Run the subproject test and every suite**

Run: `./spur subproject-test`
Expected: both print `consumer ok`.
Run: `./spur test`, `./spur games-test`, `./spur native-games-test`, `./spur windows-test`
Expected: PASS, and `ctest -N` of `debug-asan` still lists `oly.lint.boundaries` (top level unchanged).

- [ ] **Step 5: Docs.** `commands.md`: `./spur subproject-test`. `troubleshooting.md`: the subproject `OLY_SCHEMA_TOOL`
message.

- [ ] **Step 6: Commit**

```bash
git add cmake CMakeLists.txt Spurfile docs/development/commands.md docs/guide/troubleshooting.md
git commit -m "feat(build): build the engine as a FetchContent subproject"
```
