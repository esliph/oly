# Publishing

How a game outside this repository gets the engine: a versioned release, the `oly` launcher that fetches it, local mode for engine work, and the file that fixes the toolchain. The commands are in the [user guide](../guide/cli.md) and in [commands](../development/commands.md); this page says why they are shaped this way.

```
engine repository                          a game's repository
──────────────────                         ──────────────────────────────
tag v0.1.0 (the C++ package)               oly.env        OLY_VERSION=0.1.0
GitHub Release v0.1.0                      CMakeLists.txt FetchContent at v0.1.0
  oly-tools-0.1.0-<platform>.tar.gz   ◄──  oly (launcher, ~/.oly/bin) downloads and runs it
  oly-core-0.1.0.tgz, oly-web-host-…       package.json   the two tarballs
  install.sh, oly, VERSION
ghcr.io/esliph/oly-{dev,wasm,windows}:0.1.0 ◄── --docker
```

## Versioning

- One version for the whole release, in semver: the C++ package, `@oly/core`, `@oly/web-host` and the images carry it. While it is `0.x`, a minor release may break compatibility.
- The version is written in three places that must agree: `project(Oly VERSION)` in `CMakeLists.txt`, and the `package.json` of `@oly/core` and `@oly/web-host`. `./spur release <version>` rewrites them (and `package-lock.json`) and commits. It creates no tag.
- Releases are cut from `master`.

## The tag comes last

The tag **is** the C++ package: a game fetches the engine with `FetchContent` at `v<version>`, and no other file holds the C++ source. A tag pushed before the checks would be a published version that did not pass them, so the pipeline (`.github/workflows/release.yml`, started by hand with the version as input) creates the tag only at the end, on the commit it tested.

| Job | What it does |
|---|---|
| `check` | the dispatch is on `master`; the input version equals the tree's three versions; the tag does not exist |
| `linux` | builds the three images from `toolchain.env`; runs the engine tests in them; stages the release; tests the release scripts and archives; runs the consumer test in local mode and staged; saves the tested images |
| `windows` | runs the staged consumer test on `windows-latest`, with Git Bash, MSYS2 UCRT64, Ninja and emsdk |
| `publish` | only without `dry_run`, and only if the others passed: loads the saved images and checks they are the tested ones, pushes them, tags the commit and creates the release |

`dry_run` is on by default: everything runs and nothing is published. One release runs at a time. A failed `publish` is re-run as "Re-run failed jobs": the tag and the release are created only when missing.

The repository is public, so a game downloads every asset without a token. The first push of each image creates a GHCR package that is private until its visibility is set to public.

## The release

| Asset | Content |
|---|---|
| `oly-tools-<version>-linux-x64.tar.gz`, `oly-tools-<version>-windows-x64.tar.gz` | the tools of one platform ([below](#oly-tools)) |
| `oly-core-<version>.tgz`, `oly-web-host-<version>.tgz` | `npm pack` of the two web packages; a game's `package.json` names their URLs |
| `install.sh` | puts the launcher on the machine |
| `oly` | the launcher, stamped with the version |
| `VERSION` | the version as text, which `latest/download/VERSION` serves for "the latest release" |

The images are `ghcr.io/esliph/oly-dev`, `oly-wasm` and `oly-windows`, tagged with the version. Each one has the linux-x64 `oly-tools` of its release baked at `/oly-tools` (`tools/release/Dockerfile.tools`).

### `oly-tools`

One archive per platform. Its root is the contract between the launcher and a version:

```
spur                 the runner, pinned per engine version
Spurfile             the game tasks: new, build, test, run, clean, doctor, sync
toolchain.env        the toolchain versions of this release
bin/oly-schema       prebuilt for the platform (.exe on Windows)
template/            common/, web/, native/: what oly new copies
cmake/mingw-w64.cmake  the toolchain file of the Windows cross-build
docker-compose.yaml  the services of --docker
```

`oly-schema` ships prebuilt. It is a host tool with no ABI link to the game, and shipping it means a web game on Windows needs no native C++ compiler to generate the schema. The Linux one links statically: a dynamic one needs the glibc of the image it was built in, and the `wasm` image has an older one. The Windows one is a static MinGW build. `cmake/mingw-w64.cmake` is in the archive because a game on Linux that builds the `.exe` has no engine checkout to take it from.

The archives are built in the `dev` container, because Git Bash on Windows cannot give `bin/oly-schema` the executable bit.

## The launcher

`oly` is a POSIX sh script, like `spur`. It knows only how to find and fetch a version: everything else belongs to the `Spurfile` of that version, so the launcher rarely changes and every engine version brings its own tasks.

```
~/.oly/
  bin/oly              the launcher
  versions/0.1.0/      an extracted oly-tools archive
```

Its contract with a version, for a command inside a game:

1. Walk up from the current folder to `oly.env` and read `OLY_VERSION`. Outside a game only `new`, `self-update`, `--version` and `doctor` run.
2. If `~/.oly/versions/<version>/spur` is missing, download `oly-tools-<version>-<platform>.tar.gz` from `$OLY_RELEASE_URL/download/v<version>/` and extract it. The archive is extracted beside its final folder and moved in whole, so an interrupted run never leaves a half-filled version that the next run trusts.
3. Run `sh <version>/spur -f <version>/Spurfile -C <game root> <task> [arguments]` with `OLY_GAME_DIR`, `OLY_TOOLS_DIR` and `OLY_VERSION` in the environment.

The launcher keeps only four commands that cannot live in a version. `new` needs a version before there is a game, `upgrade` changes which version the game uses, `self-update` replaces the launcher, and `--version` must answer when no tools are installed. `new` and `doctor` outside a game use the latest release, which the launcher reads from `latest/download/VERSION`.

`OLY_RELEASE_URL` (default `https://github.com/esliph/oly/releases`) is the base of every download. A folder served over HTTP, or over `file://`, with GitHub's layout (`download/v<version>/...` and `latest/download/...`) works as a release. This is what lets the staged tests run before anything exists on GitHub.

## Local mode

`OLY_ENGINE_DIR=<checkout>` replaces every part of a version with the checkout, so a change in the engine shows in a game without a release.

| Part | Pinned version | Local mode |
|---|---|---|
| Tasks and template | `~/.oly/versions/<V>/` | `tools/oly/` and `templates/game/` |
| C++ engine | `FetchContent` at `v<V>` | `-DFETCHCONTENT_SOURCE_DIR_OLY=<checkout>` |
| `oly-schema` | prebuilt in the version | built from the checkout with the host compiler, or in the container |
| npm packages | the tarballs of the release | the checkout's packages compiled over the installed tarballs; `package.json` is not changed |
| Docker image | `ghcr.io/esliph/oly-*:<V>` | the same image, the checkout mounted at `/oly` |

`FETCHCONTENT_SOURCE_DIR_OLY` is passed on every configure, empty when there is no checkout: a value left in the CMake cache by an earlier local run would otherwise keep building the checkout. `node_modules/.oly-install` records the platform and the mode of the last install (`linux-x64 pinned`, `windows-x64 local`). When either changes, `node_modules` is removed and installed again: esbuild ships a binary per platform, and a checkout's packages must not outlive local mode.

A change to an engine Dockerfile reaches a game only with the next release, since local mode uses the published image.

## `toolchain.env`

One file at the engine root holds the toolchain versions: `EMSDK_VERSION`, `GCC_VERSION`, `GCC_MINIMUM`, `MINGW_GCC_MINIMUM`, `CMAKE_VERSION`, `CMAKE_MINIMUM` and `NINJA_MINIMUM`. A version is written there and nowhere else. Three readers:

| Reader | Use |
|---|---|
| `./spur images` and the Dockerfiles | the versions are build arguments, with no default |
| `oly doctor`, from the copy in `oly-tools` | one line per tool, `ok`, `old`, `untested`, `missing` or `wrong` |
| the configure of a game (`cmake/OlyToolchain.cmake`) | stops the build with a message that names both versions |

**Emscripten** must equal `EMSDK_VERSION`: the engine is tested with that one version, and Emscripten changes link flags and run-time behaviour between versions. `-DOLY_ALLOW_UNTESTED_TOOLCHAIN=ON` turns the error into a warning. **GCC, MinGW GCC, CMake and Ninja** have a minimum, since the game's own compiler builds the engine. The minimums are the versions the images test with, not bisected: an older toolchain is untested, not known to fail. `Dockerfile.windows` fails its build if Debian's MinGW is below the minimum.

The `wasm` image builds `oly-schema` with emsdk's Clang (`CC=clang`), because Ubuntu's default GCC there is below `GCC_MINIMUM`.

## When Docker is used

| Where | Docker |
|---|---|
| Developing the engine | always, through `./spur` |
| The release pipeline | builds the images, tests in them, publishes them |
| A game, default | not used: host tools, checked against `toolchain.env` |
| A game, `--docker` or `OLY_RUNNER=docker` | the same task in the image of the pinned version, the game mounted at `/game` |
| A game's CI | `oly test --docker`, so the runner installs no toolchain |

With `--docker` the launcher's `Spurfile` runs again inside the container with `--host`, from `/oly-tools` (or `/oly` in local mode). Each runner has its own build directory (`build/cmake`, `build/cmake-docker`, with `-windows` for the cross-build), because one CMake cache cannot serve a container path and a host path.

## Game builds are `Debug`

`oly build`, `oly test` and `oly run` configure with `CMAKE_BUILD_TYPE=Debug`, so `OLY_ASSERT` stops a contract violation in a game. The engine's release presets are for measuring, not for the game's everyday loop.

## The engine as a subproject

A game fetches the engine with `FetchContent`; `oly_add_game` and the `oly::` targets come with it. When the engine is not the top-level project:

- `OLY_BUILD_TESTS`, `OLY_BUILD_BENCH` and `OLY_BUILD_GAMES` are off, the boundary lint is not registered and `OLY_SANITIZE` adds no global flags;
- `OLY_WERROR` is off: `oly_warnings` still applies `-Wall -Wextra -Wpedantic` to the game target, and the game turns `-Werror` on if it wants;
- `OLY_SCHEMA_TOOL` is an input: `oly` passes the prebuilt `oly-schema` of the version, or the one it built from the checkout;
- `OLY_ENVIRONMENT` is derived where the game sees it, and a native game needs `OLY_BUILD_NATIVE_HOST`, which the game tasks turn on;
- nothing a game calls uses a path relative to the engine root.

`CMakePresets.json` stays container-only. The game tasks pass the configure options directly, with the Emscripten toolchain file found through `$EMSDK`.

## The consumer test

The consumer test creates a game with `oly new`, for `web` and for `native`, builds it and runs its test: a node smoke test against the real WASM for the web game, a headless run (`--frames`, `SDL_VIDEO_DRIVER=dummy`) for the native one. It runs three times, each proving more:

| Run | Where | What it proves |
|---|---|---|
| Local mode | `./spur consumer-test`; the `linux` job | the template, the game tasks and the subproject build work from the checkout |
| Staged, Linux | `./spur staged-test <version>`; the `linux` job | the files just built work as a game downloads them: launcher, `oly-tools` layout, prebuilt `oly-schema`, npm tarballs |
| Staged, Windows | the `windows` job | the same on the user's platform: Git Bash path conversion, host MinGW, emsdk on Windows, the `.exe` `oly-schema` |

A staged release is the release files written to a folder with GitHub's layout (`./spur stage <version>`). `OLY_RELEASE_URL` points the launcher at it, and the engine source comes from the commit being released (`OLY_ENGINE_SOURCE_DIR`, passed as `FETCHCONTENT_SOURCE_DIR_OLY`), because the tag does not exist yet. The staged run sets the npm `@oly` registry to an address that refuses connections, so a game that tried to fetch an `@oly` package from the registry would fail.

## Limits

`FetchContent` at a real tag is the one step the tests do not run before publishing: games exercise it after the release exists. The rest is in [limits.md](limits.md).

See also: [build-and-tests.md](build-and-tests.md), [repository.md](repository.md), [decisions.md](decisions.md).
