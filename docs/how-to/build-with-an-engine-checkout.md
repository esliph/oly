# Build with an engine checkout

[Docs](../README.md#how-to-guides) · Reference: [The `oly` command](../reference/cli.md#local-mode)

Use this to change the engine and a game together, without a release in between. It assumes a game made by `oly new` and a checkout of the engine repository, for example in the folder next to the game.

## 1. Point `oly` at the checkout

```sh
OLY_ENGINE_DIR=../oly oly test
```

This is local mode. A relative path is relative to the folder where you run `oly`. If the path is not a directory, `oly` stops with `OLY_ENGINE_DIR is not a directory: <path>`.

To keep it for a whole terminal session, export the variable:

```sh
export OLY_ENGINE_DIR=../oly
oly build
```

## 2. Know what changes

In local mode the tools, the template, the C++ source, `oly-schema` and the npm packages come from the checkout, and `oly.env` and `package.json` stay as they are. The table of what each part is in a pinned version and in local mode is in [Local mode](../reference/cli.md#local-mode).

`oly --version` prints `engine <path> (local)` instead of the pinned version. Outside a game, `oly new` and `oly doctor` use the version in `project(Oly VERSION ...)` of the checkout's `CMakeLists.txt`.

With `--docker`, the image is the one of the version, with the checkout mounted at `/oly`. A change to an engine Dockerfile reaches a game only with the next release.

## 3. Go back to the pinned version

```sh
unset OLY_ENGINE_DIR
oly build
```

A web game installs the npm packages again when the mode changes, so the checkout's packages do not stay in `node_modules`.

## See also

- [Local mode](../reference/cli.md#local-mode) and [Environment variables](../reference/cli.md#environment-variables).
- [Upgrade the engine](upgrade-the-engine.md): move the game to a release.
- [Commands](../reference/contributor/commands.md): the `./spur` tasks inside the engine repository.
