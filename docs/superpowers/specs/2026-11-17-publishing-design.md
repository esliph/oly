# Oly — publishing and the `oly` CLI — design

Date: 2026-11-17. Written on `feat/publish` at `6ff02cf`; implemented on the branch that holds the finished plugins
project (§13).
Specs that remain valid: `2026-11-13-multi-environment-design.md`, `2026-11-13-spur-task-runner-design.md`,
`2026-11-16-plugins-design.md`. This design is project 2 of the order set in the plugins design (§1): it publishes the
engine after the plugin mechanism has changed the public API.

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice during the design,
which the user may review; the rest is a direct consequence of the two.

## 1. Intent

Today a game builds only inside this repository: `oly_add_game` lives in `cmake/`, the modules are internal targets,
there is no `install()` or `export`, `oly-schema` is looked up in `build/schema-tool/bin/` of this repository, and the
toolchain exists only in Docker through `./spur`.

The goal is that the user's own games live in **other repositories** and install the engine from there
**[user decision]**. Third-party users are not the audience yet ("for now"), so nothing here needs a public package
registry or a compatibility promise to strangers.

While making a game, the user expects to change the engine at the same time **[user decision]**. A game therefore pins
a released version by default and can switch to a local engine checkout with one variable (§7) **[user decision:
option B]**.

Docker is **not** a requirement to create, build or run a game **[user decision]**: the game uses the tools installed
on the machine by default, and Docker is an option (§8).

## 2. Goals, non-goals, anti-goals

**Goals**

- One version number covers everything a release publishes: the engine source (the git tag), the `oly-tools` archives,
  the npm tarballs and the Docker images **[author decision]**.
- A global CLI `oly`, installed once per machine, creates games and runs their tasks **[user decision]**. It is a
  version launcher: each game runs the tools of the engine version it pins (§5) **[author decision]**.
- A game repository holds no copy of the engine's tasks, so tasks never drift between games.
- The engine works as a CMake subproject fetched by `FetchContent` (§6).
- A local engine checkout replaces the pinned version for C++, `oly-schema` and the npm packages, without editing any
  file of the game (§7).
- One file, `toolchain.env`, holds the toolchain versions; the Dockerfiles, `oly doctor` and the CMake configure check
  read it (§8) **[user decision]**.
- A consumer test proves, before every release, that a game created by `oly new` builds and runs against that version.
- The user guide explains how to start a game in its own repository.

**Non-goals**

- Public package registries: npm registry, vcpkg, Conan.
- Prebuilt engine libraries and `find_package(Oly)`: they need one build per toolchain version, and the local mode would
  need a reinstall after every engine change.
- MSVC. On Windows, native games build with MinGW **[user decision]**.
- macOS, PowerShell and `cmd`. On Windows, `oly` runs in Git Bash, like the engine today **[author decision]**.
- A CI run on every push of the engine. CI here is the release pipeline (§4).
- Moving `arena` and `drift` out of this repository. They keep testing the engine at `HEAD`; the consumer test covers the
  external path **[author decision]**.
- Packaging plugins separately. Plugins live in `plugins/` and come with the engine source (§6).
- A plugin declared in a game's own repository (`oly_add_plugin` called by the game). It becomes its own project when a
  game needs it **[user decision]**; until then it is a limit in `limits.md`.

**Anti-goals** (these make the project a failure even if it works)

- Docker required to create, build or run a game.
- A version written in two places that can diverge without an error.
- A copy of the engine's tasks inside each game.
- A published version whose consumer test did not pass.
- A change to how the engine itself is developed: the engine repository stays Docker-only through `./spur`
  (`CLAUDE.md`).

## 3. Overview

```
publishing (engine)          installing (machine)          game repository              running
───────────────────          ────────────────────          ───────────────              ───────
tag v0.3.0                   install.sh → ~/.oly/bin/oly   oly new meu-jogo             oly build
  → release pipeline         ~/.oly/versions/0.3.0/          oly.env (version)            host tools (default)
  → GHCR images                (downloaded on demand)        CMakeLists (FetchContent)    or --docker
  → Release: oly-tools,                                      package.json (web)         oly run / oly test
    npm .tgz, install.sh
```

## 4. Publishing

### 4.1 Versioning

- One version for the whole release, in semver. While the version is `0.x`, a minor bump may break compatibility
  **[author decision]**.
- The first published version is `0.1.0` **[author decision]**. `project(Oly VERSION 1.0)` in `CMakeLists.txt` becomes
  `0.1.0`, matching the `package.json` files.
- Releases are cut from `master` **[author decision]**.
- `./spur release <version>` writes the version into `project(Oly VERSION)` and the `package.json` of `@oly/core` and
  `@oly/web-host` and commits. It creates no tag: the pipeline does, only on success (§4.2).

### 4.2 Release pipeline

A GitHub Actions workflow `release.yml`, started by hand (`workflow_dispatch`) on `master` with the version as input
**[author decision]**. The tag is the C++ package, so a tag pushed before the checks would be a published version that
did not pass them; the pipeline creates the tag only at the end. The repository `esliph/oly` is public, so the games
download everything below without a token.

1. Check that the input version, `project(Oly VERSION)` and both `package.json` versions are equal, and that the tag
   `v<version>` does not exist yet; fail otherwise.
2. Build the `dev`, `wasm` and `windows` images, with the versions from `toolchain.env` as build arguments.
3. Run the engine tests inside them (the same tasks as `./spur`).
4. Build `oly-schema` for `linux-x64` (`dev` image) and `windows-x64` (`windows` image, MinGW, statically linked).
5. Run the consumer test (§10): local mode, staged artifacts on Linux, staged artifacts on Windows.
6. Only if every step passed, create the tag `v<version>` on the tested commit and publish:
   - `ghcr.io/esliph/oly-dev:<version>`, `ghcr.io/esliph/oly-wasm:<version>`, `ghcr.io/esliph/oly-windows:<version>`;
   - a GitHub Release `v<version>` with `oly-tools-<version>-linux-x64.tar.gz`,
     `oly-tools-<version>-windows-x64.tar.gz`, `oly-core-<version>.tgz`, `oly-web-host-<version>.tgz` (from
     `npm pack`) and `install.sh`.

The C++ source is not a separate artifact: the tag is the package **[user decision: approach 1]**.

### 4.3 `oly-tools` archive

One per platform. Its root is the contract between the launcher and a version (§5.2):

```
spur                 the runner, pinned per engine version
Spurfile             the game tasks (§5.3)
toolchain.env        the toolchain versions of this release
bin/oly-schema       prebuilt for the platform (.exe on Windows)
template/            common/, web/, native/ (§9)
docker-compose.yaml  services for --docker (§8.4)
```

`oly-schema` ships prebuilt **[author decision]**: it is a host tool with no ABI link to the game, and shipping it means
a web game on Windows needs no native C++ compiler at all.

## 5. The `oly` CLI

### 5.1 Installing

```
curl -fsSL https://github.com/esliph/oly/releases/latest/download/install.sh | sh
```

`install.sh` puts the launcher in `~/.oly/bin/oly` and adds `~/.oly/bin` to `PATH` in the shell profile. It needs only
`sh`, `curl` and `tar`, which Git Bash has.

### 5.2 Launcher

The launcher is a POSIX sh script, like `spur` **[author decision]**. It knows only how to find and fetch a version;
everything else belongs to the version's `Spurfile`, so the launcher rarely changes.

```
~/.oly/
  bin/oly               the launcher
  versions/0.3.0/       an extracted oly-tools archive (§4.3)
  versions/0.4.0/
```

Inside a game, `oly <task> [args]`:

1. Finds the game root by walking up to `oly.env` and reads `OLY_VERSION`.
2. If `~/.oly/versions/<version>/` is missing, downloads `oly-tools-<version>-<platform>.tar.gz` from the Release and
   extracts it.
3. Runs `~/.oly/versions/<version>/spur -f ~/.oly/versions/<version>/Spurfile -C <game root> <task> [args]`, with
   `OLY_TOOLS_DIR` set to that version folder.

With `OLY_ENGINE_DIR` set, step 2 is skipped and the tools come from the checkout (§7).

### 5.3 Commands

| Command | Where it lives | What it does |
|---|---|---|
| `oly new <name> --environment web\|native [--version V]` | launcher, then the version's `Spurfile` | creates `<name>/` from the template of `V` (default: the latest release) |
| `oly upgrade <V>` | launcher | rewrites `OLY_VERSION` and the npm tarball URLs, then reinstalls the npm packages |
| `oly self-update` | launcher | replaces the launcher with the latest release's |
| `oly --version` | launcher | the launcher version and, inside a game, the pinned engine version |
| `oly doctor` | `Spurfile` | checks the host tools against `toolchain.env` |
| `oly build` | `Spurfile` | builds the game (§11) |
| `oly test` | `Spurfile` | builds and runs the game's tests |
| `oly run` | `Spurfile` | web: serves on port 8000; native: runs the executable |
| `oly clean` | `Spurfile` | removes `build/` |

Every `Spurfile` command takes `--docker` and `--host` (§8.4). Native builds target the host platform; `--platform
windows` builds the `.exe` with MinGW, on the host or in the `windows` image.

## 6. The engine as a CMake subproject

The game fetches the engine with `FetchContent`; `oly_add_game` and the `oly::` targets come with it.

```cmake
cmake_minimum_required(VERSION 3.25)
project(MeuJogo LANGUAGES CXX C)

include(FetchContent)
FetchContent_Declare(oly
  GIT_REPOSITORY https://github.com/esliph/oly.git
  GIT_TAG v${OLY_VERSION})
FetchContent_MakeAvailable(oly)

file(GLOB nativeSources CONFIGURE_DEPENDS native/*.cpp)
oly_add_game(meu_jogo
  ENVIRONMENT web
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/meu-jogo.oly
  SOURCES ${nativeSources}
  DEPENDS ecs app capi)
```

`OLY_VERSION` comes from `oly.env`: the template's `CMakeLists.txt` reads it, so a bare `cmake` run and an IDE see the
same version as `oly` **[author decision]**. `DIMENSION` and `PLUGINS` come from the plugins design (§7.2 there).

Changes in the engine:

- **`OLY_ENVIRONMENT` visible to the game.** Today it is set in the engine's directory scope, so `oly_add_game` called
  from the game sees it empty and fails. It must be derived where the game can see it (for example from `EMSCRIPTEN`
  inside `oly_add_game`).
- **Defaults depend on `PROJECT_IS_TOP_LEVEL`.** As a subproject: `OLY_BUILD_TESTS`, `OLY_BUILD_BENCH` and
  `OLY_BUILD_GAMES` are off, the boundary lint is not registered, `OLY_SANITIZE` adds no global flags.
- **`OLY_WERROR` is on only at top level** **[author decision]**. `oly_warnings` still applies `-Wall -Wextra
  -Wpedantic` to the game target; the game turns `-Werror` on if it wants.
- **`OLY_SCHEMA_TOOL` is an input.** `oly` passes the prebuilt `oly-schema` of the version (or the one built from the
  checkout, §7). The in-repository default stays.
- **Native games** need `OLY_BUILD_NATIVE_HOST`; the game tasks turn it on for a native game.
- **No path relative to the engine root** in what a game calls. The plugins design already requires this of plugins
  (§3 there).
- The engine's `CMakePresets.json` stays container-only (`/emsdk/...`). The game tasks pass the configure options
  directly, with the Emscripten toolchain file found through `$EMSDK` **[author decision]**: a preset file in each game
  would be one more copy to drift.

## 7. Local mode

`OLY_ENGINE_DIR=../oly oly build` **[user decision: option B]**:

| Part | Pinned version | Local mode |
|---|---|---|
| Tasks and template | `~/.oly/versions/<V>/` | `$OLY_ENGINE_DIR/tools/oly/` and `$OLY_ENGINE_DIR/templates/game/` |
| C++ engine | `FetchContent` at `v<V>` | `-DFETCHCONTENT_SOURCE_DIR_OLY=$OLY_ENGINE_DIR` |
| `oly-schema` | prebuilt in the version | built from the checkout with the host compiler (MinGW on Windows) or with `--docker` |
| npm packages | `.tgz` of the Release | built from the checkout and installed over the tarballs; `package.json` is not changed |
| Docker image | `ghcr.io/esliph/oly-*:<V>` | the same published image, with the checkout mounted at `/oly` |

A change to an engine Dockerfile reaches the games only with the next release.

## 8. Toolchain and Docker

### 8.1 `toolchain.env`

A file at the engine root with the toolchain versions, for example `EMSDK_VERSION=3.1.74`, `GCC_VERSION=14`,
`CMAKE_VERSION=3.30.5`, `CMAKE_MINIMUM=3.25`, the MinGW GCC minimum and the Ninja minimum **[user decision]**. Readers:

- the three Dockerfiles, as build arguments (today the versions are written inside each Dockerfile, and the MinGW one is
  whatever Debian `trixie` ships);
- `oly doctor`, from the copy in `oly-tools`;
- the CMake configure of a game.

### 8.2 Configure check

- **Emscripten:** `EMSCRIPTEN_VERSION` must equal `EMSDK_VERSION`, otherwise the configure fails with a message naming
  both versions **[author decision]**. Reason: the engine is tested only with that version, and Emscripten changes link
  flags and runtime behaviour between versions. `-DOLY_ALLOW_UNTESTED_TOOLCHAIN=ON` turns the error into a warning.
- **GCC, MinGW, CMake, Ninja:** a minimum version, since the engine is compiled from source by the game's own compiler.
  `Dockerfile.windows` fails its build if Debian's MinGW is below the minimum; it does not install a pinned MinGW from
  elsewhere **[author decision]**.

### 8.3 Windows hosts

The supported Windows host toolchain is MSYS2 UCRT64 (GCC, POSIX threads) with Ninja, used from Git Bash
**[author decision]**. Ninja is a required host tool on Windows for both environments: CMake's default generator there
is Visual Studio, which does not drive MinGW or Emscripten. `oly doctor` checks the MinGW GCC and Ninja versions, and
the guide tells the user what to install.

### 8.4 When Docker is used

| Where | Docker |
|---|---|
| Developing the engine | always, through `./spur` (unchanged) |
| Release pipeline | builds the images, runs the tests in them, publishes them |
| A game, default | not used: host tools, checked against `toolchain.env` |
| A game, `--docker` | the same task inside the image of the pinned version, with the game folder mounted |
| A game's CI | recommended: `oly test --docker`, so the runner installs no toolchain |

A game can make Docker its default with `OLY_RUNNER=docker` in `oly.env`; `--host` overrides it. `oly new`, `oly
doctor` and the default `oly build`, `oly test` and `oly run` never need Docker.

## 9. Game repository

`oly new meu-jogo --environment web` creates:

```
meu-jogo/
  oly.env                    OLY_VERSION=0.3.0, OLY_RUNNER=host
  CMakeLists.txt             FetchContent + oly_add_game
  meu-jogo.oly
  native/                    per-entity C++ systems
  web/src/                   game.ts (installGame, no DOM), main.ts (browser shell), index.html   [web]
  web/tests/                 smoke test against the real WASM                                    [web]
  package.json, tsconfig.json   @oly/core and @oly/web-host from the Release tarballs            [web]
  desktop/                   main.cpp and render                                                 [native]
  .github/workflows/ci.yml   installs oly and runs oly test --docker
  .gitignore
```

The template lives in the engine at `templates/game/{common,web,native}/` and the game tasks at `tools/oly/`
**[author decision]**. The template game is minimal: a square that moves with the keyboard, in the shape `arena` and
`drift` already use (DOM-free game separate from the browser shell; gameplay separate from the SDL shell).

The npm tarball URLs carry the version, so the version appears in `oly.env` and in `package.json`. `oly upgrade` rewrites
both, and `oly build` fails if they differ.

## 10. Consumer test

The consumer test creates a game with `oly new` for `web` and for `native`, builds it and runs its test (web: node
smoke test against the WASM; native: headless with `--frames N` and `SDL_VIDEO_DRIVER=dummy`). It runs in three ways
**[user decision: option C]**:

| Run | Where | What it proves |
|---|---|---|
| Local mode | `./spur consumer-test` in the engine (Docker) and in the pipeline | the template, the game tasks and the subproject build work from the checkout |
| Staged, Linux | pipeline, Linux job | the artifacts just built work as a game downloads them: launcher, `oly-tools` layout, prebuilt `oly-schema`, npm tarballs |
| Staged, Windows | pipeline, `windows-latest` job with Git Bash, MSYS2 UCRT64, emsdk and Ninja | the same on the user's platform: MSYS path conversion, host MinGW, emsdk on Windows, the `.exe` `oly-schema` |

Staging points the launcher's download base at the files the pipeline just built (a variable such as
`OLY_RELEASE_URL`, name left to the builder) instead of the GitHub Release. The engine source comes from the commit
being released through `FETCHCONTENT_SOURCE_DIR_OLY`, because the tag does not exist until the checks pass (§4.2).
`FetchContent` at a real tag is therefore exercised only after publishing, by the games themselves.

## 11. Running a game

```
web, host tools
oly build
  1. read oly.env, fetch the version's tools if missing
  2. configure with Emscripten: FetchContent fetches the engine (cache in build/_deps), oly_add_game generates the C++
     schema with the prebuilt oly-schema
  3. build: meu_jogo.mjs and meu_jogo.wasm
  4. oly-schema --emit ts: web/schema.generated.ts
  5. npm ci, tsc, esbuild: build/web/ (index.html, main.js, .mjs, .wasm)
oly run     serve build/web/ on port 8000
oly test    ctest + node --test against the real WASM

native, host tools
oly build   GCC (Linux) or MinGW (Windows); SDL3 comes through the engine
oly run     runs the executable
oly test    headless run
```

## 12. Docs

- `docs/guide/` (rule of `CLAUDE.md`, following `docs/development/guide-style.md`): the getting-started pages start
  from `oly new` in a separate repository; a page for the `oly` CLI, the local mode and `--docker`.
- `docs/architecture/`: a new `publishing.md` (release, versioning, `oly-tools`, launcher, local mode, `toolchain.env`);
  `build-and-tests.md` (subproject defaults, consumer test); `repository.md` (`tools/oly/`, `templates/game/`,
  `toolchain.env`, `.github/`); `decisions.md`; `limits.md` (no MSVC, no macOS, Git Bash only); `glossary.md`.
- `docs/development/commands.md`: `./spur release`, `./spur consumer-test`.
- `CLAUDE.md`: the structure, and "study and benchmark project" reworded (the plugins design left this to publishing).

## 13. Order of work **[author decision]**

Implementation starts after the whole plugins project is merged, all three of its phases, `essentials2d`,
`essentials3d` and `DIMENSION` included **[user decision]**. It changes `oly_add_game`, the `oly-schema` inputs and the
`Engine` registration, which `0.1.0` freezes; and the template uses `essentials2d`'s `Position` instead of declaring
its own.

1. **Engine as a subproject** (§6) and `toolchain.env` with the configure check (§8.1, §8.2).
2. **Game tasks, template and local mode** (§7, §9), with `./spur consumer-test` (§10) running locally.
3. **Launcher and `install.sh`** (§5).
4. **Release pipeline** (§4): version check, images, `oly-tools`, npm tarballs, `./spur release`.
5. **Docs** (§12).

## 14. Left to the builder

- How `oly_add_game` derives `OLY_ENVIRONMENT`, and how the template's `CMakeLists.txt` reads `oly.env`.
- How the local mode installs the checkout's npm packages, under the constraint that `package.json` does not change.
- Path conversion in Git Bash when `--docker` mounts folders (MSYS converts arguments that start with `/`).
- How the launcher resolves "the latest release" and detects the platform.
- The internal shape of the game `Spurfile` and of the `release.yml` jobs.
- The exact minimum versions of GCC, MinGW and CMake in `toolchain.env`.
