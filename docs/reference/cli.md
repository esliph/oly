# The `oly` command

[Reference](../README.md#reference) · Concept: [Environments and hosts](../explanation/concepts/environments-and-hosts.md)

`oly` creates a game in its own repository, builds it, tests it and runs it. A game does not live in the engine repository: it pins one engine version, and `oly` downloads the tools of that version when it needs them. The messages of `oly` are in [Errors](errors.md#oly).

## Install

Install these before the first command. Versions are in [`toolchain.env`](../../toolchain.env).

| | Linux | Windows |
|---|---|---|
| Shell | any POSIX shell | **Git Bash**. `oly` is a shell script: run it from Git Bash, not from PowerShell or `cmd` |
| Needed by `oly` itself | `curl`, `tar` | `curl`, `tar` (Git Bash has them) |
| Native game | `cmake`, `git`, `make`, GCC `GCC_MINIMUM` or newer, and the X11 and Wayland development headers that SDL3 needs to open a window (the `Dockerfile` of the engine lists them). Wine, to run the tests of a `--platform windows` build | **MSYS2 UCRT64**: `mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-cmake`, `mingw-w64-ucrt-x86_64-ninja`. Put its `bin` folder on the `PATH` of Git Bash |
| Web game | `cmake`, `git`, `make`, GCC, Node.js with npm, emsdk `EMSDK_VERSION` (source `emsdk_env.sh`) | the three MSYS2 packages above, Node.js with npm, emsdk `EMSDK_VERSION` (source `emsdk_env.sh`) |
| Docker | optional: see [`--docker`](#docker) | optional |

`oly doctor` checks `cmake`, `ninja` (Windows only) and, for a native game, `g++` or, for a web game, `emcc`, `node` and `npm`. It does not check `curl`, `tar`, `git`, `make`, the X11 and Wayland headers, Wine or Docker (`--docker` checks `docker compose`).

Not supported: MSVC, macOS and shells other than Git Bash on Windows.

## Create a game

There is nothing to install first. This line downloads the launcher of the latest release and runs its `new` command:

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/oly | sh -s -- new my-game --environment web
```

The game then holds the launcher as `./oly`: commit it, and every clone works with no step before it. No step changes the `PATH` or a shell profile. A failed `curl` creates nothing, and the pipe still exits 0: check that `my-game/oly` exists.

A repository committed from Windows does not keep the executable bit of `./oly`. Run it as `sh ./oly`, or set the bit once and commit it: `git update-index --chmod=+x oly`.

## Commands

Inside a game, `oly` finds the game by walking up from the current folder to `oly.env`. The tutorials use these commands in order: [Your first web game](../tutorials/first-web-game.md) and [Your first desktop game](../tutorials/first-desktop-game.md).

| Command | What it does |
|---|---|
| `oly new <game> --environment web\|native [--version V]` | creates `<game>/` from the template of the engine release |
| `oly doctor [--environment web\|native] [--docker\|--host]` | checks the host tools against `toolchain.env`; exits 1 when a line is not `ok` |
| `oly build` | builds the game |
| `oly test` | builds the game and runs its tests |
| `oly run [arguments]` | builds the game and runs it. Web: serves `build/web/` on port 8000. Desktop: runs the executable and passes it the arguments |
| `oly clean` | removes `build/` |
| `oly sync` | installs the npm packages of a web game, as `package.json` and the mode ([local mode](#local-mode)) ask. `oly build` runs it for you |
| `oly platform` | prints the host platform: `linux-x64` or `windows-x64` |
| `oly upgrade <version>` | moves the game to another engine release and replaces `./oly` with that release's launcher: [Upgrade the engine](../how-to/upgrade-the-engine.md) |
| `oly --version` | prints the launcher version and, inside a game, the engine version it pins |

### `oly new`

`<game>` is the name of the folder and of the game: lowercase letters, digits and `-`, starting with a letter. `oly new` makes the folder in the current directory and, for a web game, installs the npm packages. `test`, `all`, `clean`, `help`, `install`, `package`, `oly` and `oly-*` are CMake or engine target names, so a game cannot use them. The files it writes: [The game template](game-template.md).

| Option | Meaning |
|---|---|
| `--environment web\|native` | where the game runs (required): [web](../tutorials/first-web-game.md) or [desktop](../tutorials/first-desktop-game.md) |
| `--version <version>` | the engine release to start from. Default: the latest release |

### `oly doctor`

Outside a game, `oly doctor` checks the tools of both environments. Each line starts with one of `ok`, `old` (older than `toolchain.env` needs), `untested` (newer than the version the engine is tested with), `missing` or `wrong` (for example `g++` that does not target MinGW on Windows: put the `bin` folder of MSYS2 UCRT64 first on the `PATH` of Git Bash), then the tool and what to install. A line that is not `ok` makes it exit with 1. The configure step of the engine checks the same versions: [Errors](errors.md#at-build-time).

### `oly build`, `oly test` and `oly run`

They build in `Debug`, so contract errors stop the game ([Errors](errors.md#at-run-time)), and run at most 6 compile jobs at a time. Put the options right after the command name.

| Option | Meaning |
|---|---|
| `--docker` | run the task in the Docker image of the pinned version: [`--docker`](#docker) |
| `--host` | run the task with the tools of the host, even when `oly.env` says `OLY_RUNNER=docker` |
| `--platform windows` | build the `.exe` of a desktop game with MinGW. On Linux this cross-compiles with `mingw-w64` and runs the Windows tools and tests under Wine; on Windows it is the normal build |

`--platform windows` is for desktop games only.

## Docker

By default `oly` uses the tools of your machine. With `--docker`, it runs the same task in the image `ghcr.io/esliph/oly-<dev|wasm|windows>:<version>`, with the game folder mounted at `/game`. The image brings the compiler, CMake, emsdk and Node.js, so the host needs Docker with the compose plugin. A web game still runs `npm install` on the host in `oly new` and `oly upgrade`, so it needs Node.js with npm there too.

```sh
./oly test --docker
```

To make Docker the default of a game, set `OLY_RUNNER=docker` in `oly.env`. `--host` overrides it. `oly new`, `oly doctor` and `oly upgrade` never start a container.

| Task | In the container |
|---|---|
| web game | the `wasm` image; `oly run --docker` publishes port 8000 |
| desktop game | the `dev` image |
| desktop game with `--platform windows` | the `windows` image; the tests run under Wine |

A desktop window cannot open in a container: `oly run --docker` on a desktop game stops with that message. Run it with `oly run --host`.

`oly doctor --docker` checks only that `docker compose` works.

## Local mode

To change the engine and a game together, point `OLY_ENGINE_DIR` at a checkout of the engine ([Build with an engine checkout](../how-to/build-with-an-engine-checkout.md)):

```sh
OLY_ENGINE_DIR=../oly ./oly test
```

This is **local mode**. The tools, the template, the C++ source, `oly-schema` and the npm packages come from the checkout, and `oly.env` and `package.json` stay as they are. A relative path is relative to the folder where you run `oly`.

| Part | Pinned version (default) | Local mode |
|---|---|---|
| Tools and template | `~/.oly/versions/<version>/` | `tools/oly/` and `templates/game/` of the checkout |
| C++ engine | fetched at the tag `v<version>` | the checkout |
| `oly-schema` | web games: prebuilt in the release. Desktop games: built with the game from the fetched engine source | web games: built from the checkout. Desktop games: built with the game from the checkout |
| npm packages | the tarballs of the release | built from the checkout and installed over the tarballs |
| Docker image | `ghcr.io/esliph/oly-*:<version>` | the same image, with the checkout mounted at `/oly` |

Unset `OLY_ENGINE_DIR` and run `oly build` to go back to the pinned version. A change to an engine Dockerfile reaches a game only with the next release.

## Environment variables

| Variable | Meaning |
|---|---|
| `OLY_ENGINE_DIR` | an engine checkout: [local mode](#local-mode) |
| `OLY_HOME` | where `oly` keeps the downloaded versions. Default: `~/.oly` |
| `OLY_RELEASE_URL` | where `oly` downloads releases from. Default: `https://github.com/esliph/oly/releases` |
| `EMSDK` | set by emsdk's `emsdk_env.sh`; a web game needs it on the host |

The keys of `oly.env` are in [The game template](game-template.md#olyenv).

## See also

- [Upgrade the engine](../how-to/upgrade-the-engine.md), [Set up the CI of a game](../how-to/set-up-game-ci.md) and [Build with an engine checkout](../how-to/build-with-an-engine-checkout.md).
- [Errors](errors.md#oly): the messages of `oly`.
- [The game template](game-template.md): every file of a new game.
