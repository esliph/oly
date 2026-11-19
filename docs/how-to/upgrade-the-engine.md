# Upgrade the engine

[Docs](../README.md#how-to-guides) · Reference: [The `oly` command](../reference/cli.md)

Use this to move a game to another engine release. It assumes a game made by `oly new` ([Your first web game](../tutorials/first-web-game.md) or [Your first desktop game](../tutorials/first-desktop-game.md)), with its `oly.env` and, for a web game, its `package.json`.

## 1. Check the version the game uses

```sh
oly --version
```

Inside a game, the second line is `engine <version> (oly.env)`: the release the game pins.

## 2. Upgrade

```sh
oly upgrade <version>
```

`oly upgrade` downloads the tools of `<version>` first, so a version that does not exist leaves the game as it was. Then it rewrites `OLY_VERSION` in `oly.env` and, for a web game, the `@oly/core` and `@oly/web-host` URLs in `package.json`, and installs the npm packages again. A desktop game has no `package.json`: `oly` prints `oly.env now pins <version>` and stops there.

If the game is already on `<version>`, `oly` prints `already on <version>` and changes nothing. If `oly.env` and `package.json` name different versions, run `oly upgrade` with the version of `oly.env` to bring `package.json` to it.

Do not edit the version by hand. A web game names it twice, in `oly.env` and in the tarball URLs of `package.json`, and `oly build` stops when the two differ ([Errors](../reference/errors.md#oly)).

## 3. Test

```sh
oly test
```

While the engine is `0.x`, a minor release can break compatibility, so run the tests before you trust the new version.

## See also

- [The `oly` command](../reference/cli.md): every command and option.
- [The game template](../reference/game-template.md#olyenv): the keys of `oly.env`.
- [Errors](../reference/errors.md#oly): the messages of `oly upgrade`.
