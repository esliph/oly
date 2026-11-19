# Cut a release

[Docs](../../README.md#how-to-guides) · Reference: [Commands](../../reference/contributor/commands.md#the-release-tasks)

Use this to publish a version of the engine: the tag, the release assets and the images. It assumes the images are built ([Set up the toolchain](set-up-the-toolchain.md)), a working tree with no changes and the right to run workflows on the repository. Nothing here publishes until the last step: the `release.yml` workflow does that, only after every check passed on Linux and on Windows.

## 1. Write the version into the tree

On a branch, on the host:

```sh
./spur release <version>
```

This writes `<version>` into `project(Oly VERSION)`, `@oly/core`, `@oly/web-host` and `package-lock.json`, and commits. It creates no tag. It refuses a working tree with changes, and a version that is not `MAJOR.MINOR.PATCH` exits with 64. If npm or the check fails after the files were rewritten, `git checkout .` drops the changes.

Then update [`CHANGELOG.md`](../../../CHANGELOG.md) in the same branch: move the unreleased entries under a `## <version>` heading (with its `### Breaking` and `### Migration` lists filled in), and add a new `(unreleased)` section above it for the next version.

Merge the branch into `master`: releases are cut from `master`.

## 2. Check on your machine (optional)

```sh
./spur stage <version>
./spur staged-test <version>
```

`stage` builds the release assets into `build/release/<version>`; `staged-test` creates, builds and tests a game from them.

## 3. Run the release workflow

In GitHub: Actions, `release`, `Run workflow` on `master`, with the `version` input. `dry_run` starts true: `check`, `build`, `engine`, `staged-linux` and `windows` run and `publish` is skipped. A dry run also starts from another branch, to try a change to the workflow before it reaches `master`.

Run it again with `dry_run` false. `publish` tags `v<version>` on the tested commit, pushes the images (`ghcr.io/esliph/oly-{dev,wasm,windows}:<version>`) and creates the release with its assets.

## 4. Make the images public

The first publish of each image creates a GHCR package that is private until its visibility is set to public (package settings on GitHub). Do that once, or games cannot pull the image without a token.

## If a step fails

- The `check` job fails when the version of the tree differs from the input and, without `dry_run`, when the dispatch is not on `master` or the tag exists (with `dry_run`, an existing tag is a warning).
- `publish` pushes the images the `staged-linux` job tested and saved. It fails before any push when a loaded image ID differs.
- One release runs at a time. When `publish` fails after pushing the images, use "Re-run failed jobs": the tag and the release are created only if missing, and an existing release gets its assets replaced.
- A patch of an older line is not made the latest release, so `latest/download` keeps serving the newest version.

## See also

- [Publishing](../../explanation/architecture/publishing.md): why the tag comes last and what each job proves.
- [Commands](../../reference/contributor/commands.md#the-release-tasks): the release tasks.
