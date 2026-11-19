# Contributing to Oly

Thank you for your interest. Oly is a study and benchmark project, published for the author's own games: every data representation decision has a measured cost, and contributions are judged by that same rule. This page says what is welcome, how to set up the toolchain and what a pull request needs to be merged.

By taking part you agree to follow the [code of conduct](CODE_OF_CONDUCT.md). Security problems are never reported in a public issue: see [SECURITY.md](SECURITY.md).

## What is welcome

| Kind | How |
|---|---|
| Bug fix | Open a pull request directly. If the fix is not small, open an issue first with the reproduction. |
| Test, benchmark or documentation | Open a pull request directly. |
| New feature, new plugin, change to a public API | **Open an issue first** and wait until it is accepted. A pull request for a feature that was not discussed may be closed, however good it is, because it may not fit the scope ([limits.md](docs/reference/limits.md)) or the design ([decisions.md](docs/explanation/architecture/decisions.md)). |
| Performance change | Open an issue with the measurement that motivates it. A performance change is merged on numbers, not on reasoning. |

Questions about using the engine go in an issue too.

## Before you start

1. Read the [architecture overview](docs/explanation/architecture/README.md). It explains the parts and the boundaries between them. [docs/README.md](docs/README.md) lists every other document.
2. Use the names of the [glossary](docs/reference/glossary.md). No aliases, and no compatibility `using` with old names.
3. The specs in [`docs/superpowers/specs/`](docs/superpowers/specs) define the requirements (`R-*`) and invariants (`I-*`). A change that breaks one is a change to the spec, and is discussed in an issue first.

## Toolchain

The host needs only Git Bash and Docker. Never install a compiler on the host: [Set up the toolchain](docs/how-to/contributor/set-up-the-toolchain.md) builds the engine and runs its tests, and [Commands](docs/reference/contributor/commands.md) lists every `./spur` task.

## Workflow

1. Fork the repository and create a branch from **`develop`**. `master` only receives releases.
2. Make the change, with its tests and documentation (below).
3. Run the checks of [the pull request template](.github/pull_request_template.md) locally.
4. Open the pull request against `develop`. CI (`.github/workflows/ci.yml`) builds the `dev` and `wasm` images and runs `./spur format-check`, `./spur test` and `./spur games-test`. The Windows cross-build, the subproject and the consumer tests run in the release workflow; when a change touches them (`cmake/`, `tools/`, `templates/`, the native host), run them locally too ([commands](docs/reference/contributor/commands.md)).

Keep one change per pull request. A refactoring that a fix needs goes in its own commit, before the fix.

## Code rules

The rules are in [code-style.md](docs/reference/contributor/code-style.md). The boundaries between modules and plugins are in [modules-and-boundaries.md](docs/explanation/architecture/modules-and-boundaries.md); the boundary lint runs with the tests.

## Tests

Every behaviour change comes with a test. New suites, new cases and contract violations (`contract_death`) each have a place and a registration: [`.claude/skills/add-test-case/SKILL.md`](.claude/skills/add-test-case/SKILL.md) describes the three cases step by step, and is readable without Claude Code.

A change to `table`, `archetype`, `query*`, `command_buffer` or `world` keeps `checkInvariants` covering what it changes.

## Performance

The performance targets are ratios against a baseline measured in the same run ([docs/reference/benchmarks/README.md](docs/reference/benchmarks/README.md)). A change on a hot path (the ECS iteration, transitions, the `CommandBuffer` flush, the client boundary) states in the pull request the benchmark it ran before and after, on the same machine, in the `release` preset. [Run the benchmarks](docs/how-to/contributor/run-benchmarks.md) shows how. Numbers from different machines or different runs are not compared.

## Documentation

A change that adds, changes or removes public API updates [`docs/`](docs/README.md) in the same pull request. [docs-style.md](docs/reference/contributor/docs-style.md) lists the public surface and where each kind of change goes.

## Commits

The history uses [Conventional Commits](https://www.conventionalcommits.org/): `type(scope): subject`, with types such as `feat`, `fix`, `test`, `docs`, `build`, `ci` and `refactor`, and the module or area as the scope (`ecs`, `app`, `capi`, `oly`, `release`). The subject is short, in the imperative, and says what changed, not which issue or plan it came from. The body, when needed, says why.

```
fix(ecs): keep the query cache when an archetype is created during flush
```

## Pull request

The checklist is in the [pull request template](.github/pull_request_template.md), which GitHub fills into the description of every pull request.

## Working with Claude Code

The repository carries a [Claude Code](https://claude.com/claude-code) setup in `.claude/` and `CLAUDE.md`: a hook that formats each edited C++ file in the `dev` container, the project skills (`add-test-case`, `bench-compare`) and two review agents (`spec-conformance-reviewer`, `perf-reviewer`). It is optional. The rules it enforces are the ones on this page, and every pull request is held to them whatever tool wrote it.

## License

Oly is distributed under the [MIT license](LICENSE). By opening a pull request you agree that your contribution is licensed under the same terms.
