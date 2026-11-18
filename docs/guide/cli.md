# The `oly` command

[Guide index](README.md)

`oly` creates a game in its own repository, builds it, tests it and runs it. A game does not live in the engine repository: it pins one engine version, and `oly` downloads the tools of that version when it needs them.

## What you install

Install these before the first command. Versions are in [`toolchain.env`](../../toolchain.env).

| | Linux | Windows |
|---|---|---|
| Shell | any POSIX shell | **Git Bash**. `oly` is a shell script: run it from Git Bash, not from PowerShell or `cmd` |
| Needed by `oly` itself | `curl`, `tar` | `curl`, `tar` (Git Bash has them) |
| Native game | `cmake`, GCC `GCC_MINIMUM` or newer | **MSYS2 UCRT64**: `mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-cmake`, `mingw-w64-ucrt-x86_64-ninja`. Put its `bin` folder on the `PATH` of Git Bash |
| Web game | `cmake`, GCC, Node.js with npm, emsdk `EMSDK_VERSION` (source `emsdk_env.sh`) | the three MSYS2 packages above, Node.js with npm, emsdk `EMSDK_VERSION` (source `emsdk_env.sh`) |
| Docker | optional: see [`--docker`](#docker) | optional |

`oly doctor` checks all of this and says what is missing.

Not supported: MSVC, macOS and shells other than Git Bash on Windows.

Install the launcher once:

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/install.sh | sh
```

`install.sh` puts `oly` in `~/.oly/bin` and adds that folder to the `PATH` of your shell profile (`~/.bashrc`, or `~/.zshrc` for zsh). Open a new terminal afterwards. `OLY_INSTALL_VERSION=<version>` installs the launcher of another release; `OLY_HOME` moves `~/.oly`.

## Make a game

```sh
oly doctor --environment web
oly new <game> --environment web
cd <game>
oly test
oly run
```

`<game>` is the name of the folder and of the game: lowercase letters, digits and `-`, starting with a letter. `oly new` makes the folder in the current directory and, for a web game, installs the npm packages.

| Option | Meaning |
|---|---|
| `--environment web\|native` | where the game runs (required): [web](environments/web/getting-started.md) or [desktop](environments/native/getting-started.md) |
| `--version <version>` | the engine release to start from. Default: the latest release |

## The commands

Inside a game, `oly` finds the game by walking up from the current folder to `oly.env`.

| Command | What it does |
|---|---|
| `oly new <game> --environment web\|native [--version V]` | creates `<game>/` from the template of the engine release |
| `oly doctor [--environment web\|native] [--docker\|--host]` | checks the host tools against `toolchain.env`; exits 1 when a line is not `ok` |
| `oly build` | builds the game |
| `oly test` | builds the game and runs its tests |
| `oly run [arguments]` | builds the game and runs it. Web: serves `build/web/` on port 8000. Desktop: runs the executable and passes it the arguments |
| `oly clean` | removes `build/` |
| `oly upgrade <version>` | moves the game to another engine release |
| `oly self-update` | replaces the launcher with the one of the latest release |
| `oly --version` | prints the launcher version and, inside a game, the engine version it pins |

`oly doctor` outside a game checks the tools of both environments. Each line starts with one of `ok`, `old`, `untested`, `missing` or `wrong`, then the tool and what to install.

`oly build`, `oly test` and `oly run` build in `Debug`, so contract errors stop the game ([Troubleshooting](troubleshooting.md)).

### Options of `build`, `test` and `run`

Put them right after the command name.

| Option | Meaning |
|---|---|
| `--docker` | run the task in the Docker image of the pinned version: [`--docker`](#docker) |
| `--host` | run the task with the tools of the host, even when `oly.env` says `OLY_RUNNER=docker` |
| `--platform windows` | build the `.exe` of a desktop game with MinGW. On Linux this cross-compiles; on Windows it is the normal build |

`--platform windows` is for desktop games only.

## Docker

By default `oly` uses the tools of your machine. With `--docker`, it runs the same task in the image `ghcr.io/esliph/oly-<dev|wasm|windows>:<version>`, with the game folder mounted at `/game`. The image brings the compiler, CMake, emsdk and Node.js, so the host needs only Docker with the compose plugin.

```sh
oly test --docker
```

To make Docker the default of a game, set `OLY_RUNNER=docker` in `oly.env`. `--host` overrides it. `oly new`, `oly doctor` and `oly upgrade` never start a container.

| Task | In the container |
|---|---|
| web game | the `wasm` image; `oly run --docker` publishes port 8000 |
| desktop game | the `dev` image |
| desktop game with `--platform windows` | the `windows` image; the tests run under Wine |

A desktop window cannot open in a container: `oly run --docker` on a desktop game stops and prints the path of the executable to run on the host.

`oly doctor --docker` checks only that `docker compose` works.

## The game repository

`oly new <game> --environment web` creates:

```
<game>/
  oly.env                     OLY_VERSION, OLY_ENVIRONMENT, OLY_GAME, OLY_RUNNER
  CMakeLists.txt              fetches the engine at the version of oly.env
  cmake/game.cmake            oly_add_game and the test
  <game>.oly                  the schema
  native/                     C++ systems
  web/src/                    game.ts (installGame, no DOM), main.ts (browser shell)
  web/index.html
  web/tests/                  a smoke test against the real WASM
  package.json, tsconfig.json
  .github/workflows/ci.yml
  .gitignore
```

A desktop game has no `web/`, `package.json` or `tsconfig.json`. It has `desktop/main.cpp`, the SDL shell, instead.

`oly.env` is the one file that names the engine version:

| Key | Meaning |
|---|---|
| `OLY_VERSION` | the engine release the game builds with |
| `OLY_ENVIRONMENT` | `web` or `native` |
| `OLY_GAME` | the name of the CMake target |
| `OLY_RUNNER` | `host` (default) or `docker` |

A web game names the version a second time: the `@oly/core` and `@oly/web-host` URLs in `package.json` carry it. `oly build` stops when the two differ. Do not edit them by hand: use `oly upgrade`.

Commit everything except what `.gitignore` lists. The template game is minimal: a square that moves with the keyboard. [Getting started on the web](environments/web/getting-started.md) and [on the desktop](environments/native/getting-started.md) walk through it.

## Upgrade the engine

```sh
oly upgrade <version>
```

`oly upgrade` downloads the tools of `<version>` first, so a version that does not exist leaves the game as it was. Then it rewrites `OLY_VERSION` in `oly.env` and, for a web game, the URLs in `package.json`, and installs the npm packages again. Run `oly test` afterwards: while the engine is `0.x`, a minor release can break compatibility.

## The game's CI

The template has `.github/workflows/ci.yml`. It installs `oly` and runs `oly test --docker`, so the runner needs no compiler, emsdk or Node.js.

## Build with an engine checkout

To change the engine and a game together, point `OLY_ENGINE_DIR` at a checkout of the engine:

```sh
OLY_ENGINE_DIR=../oly oly test
```

This is **local mode**. The tools, the template, the C++ source, `oly-schema` and the npm packages come from the checkout, and `oly.env` and `package.json` stay as they are. A relative path is relative to the folder where you run `oly`.

| Part | Pinned version (default) | Local mode |
|---|---|---|
| Tools and template | `~/.oly/versions/<version>/` | `tools/oly/` and `templates/game/` of the checkout |
| C++ engine | fetched at the tag `v<version>` | the checkout |
| `oly-schema` | prebuilt in the release | built from the checkout |
| npm packages | the tarballs of the release | built from the checkout and installed over the tarballs |
| Docker image | `ghcr.io/esliph/oly-*:<version>` | the same image, with the checkout mounted at `/oly` |

Unset `OLY_ENGINE_DIR` and run `oly build` to go back to the pinned version.

A change to an engine Dockerfile reaches a game only with the next release.

## Environment variables

| Variable | Meaning |
|---|---|
| `OLY_ENGINE_DIR` | an engine checkout: [local mode](#build-with-an-engine-checkout) |
| `OLY_HOME` | where `oly` keeps the launcher and the downloaded versions. Default: `~/.oly` |
| `OLY_RELEASE_URL` | where `oly` downloads releases from. Default: `https://github.com/esliph/oly/releases` |
| `OLY_INSTALL_VERSION` | for `install.sh`: the launcher version to install |
| `EMSDK` | set by emsdk's `emsdk_env.sh`; a web game needs it on the host |

## Errors

The messages of `oly` are in [Troubleshooting](troubleshooting.md#the-oly-command).

## Next

[Getting started on the web](environments/web/getting-started.md) or [on the desktop](environments/native/getting-started.md): the template, file by file.
