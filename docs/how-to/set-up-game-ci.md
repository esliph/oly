# Set up the CI of a game

[Docs](../README.md#how-to-guides) · Reference: [The `oly` command](../reference/cli.md)

Use this to run the tests of a game on every push and pull request with GitHub Actions. It assumes a game made by `oly new` and kept in a GitHub repository.

## 1. Use the workflow the template wrote

`oly new` writes `.github/workflows/ci.yml` ([The game template](../reference/game-template.md#common)). It runs on `push` and `pull_request`, and its single job does two things on an `ubuntu-latest` runner:

1. Checks out the repository with `actions/checkout@v4`.
2. Runs the tests in Docker:

```yaml
- name: Test in Docker, so the runner installs no toolchain
  run: sh ./oly test --docker
```

`sh ./oly test --docker` runs the launcher the game committed, so the runner installs nothing and the executable bit does not matter. It runs in the image of the version `oly.env` pins ([Docker](../reference/cli.md#docker)), so the runner needs no compiler, emsdk or Node.js. Commit the workflow with the rest of the game.

## 2. Keep it on the pinned version

The CI tests the version the game pins. When you move the game to another release, run `./oly upgrade <version>` and commit `oly.env` and `package.json` together ([Upgrade the engine](upgrade-the-engine.md)).

## See also

- [Docker](../reference/cli.md#docker): how `--docker` chooses the image.
- [The game template](../reference/game-template.md#common): every file `oly new` writes.
