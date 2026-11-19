## What and why

<!-- What the change does and why. For a feature or a public API change, link the accepted issue: Closes #123 -->

## How it was tested

<!-- The tests added or changed, and the commands run. For a hot path change, the benchmark before and after, same machine, release preset. -->

## Checklist

- [ ] The branch starts from `develop` and the pull request targets `develop`.
- [ ] A feature or public API change has an accepted issue.
- [ ] `./spur test` and `./spur games-test` pass locally.
- [ ] `./spur format-check` passes.
- [ ] Each behaviour change has a test.
- [ ] A hot path change reports the benchmark before and after.
- [ ] A public API change updates `docs/`, as `docs/reference/contributor/docs-style.md` says; a change of design updates `docs/explanation/architecture/`.
