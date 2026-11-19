# Support

## Where to ask

Open an [issue](https://github.com/esliph/oly/issues/new/choose) for a question about using the engine, a bug or a missing piece of documentation. Search the open and closed issues first: the answer may already be there.

Before you ask, look at [Fix common problems](docs/how-to/fix-common-problems.md) and the [errors reference](docs/reference/errors.md). Both list the messages the engine and the `oly` command print, with the cause and the fix.

## What is not support

- A security problem is never reported in a public issue. Follow [SECURITY.md](SECURITY.md).
- A new feature or a change to a public API starts as a feature issue. See [CONTRIBUTING.md](CONTRIBUTING.md).
- Oly does not draw, play audio or do physics: see [Limits](docs/reference/limits.md).

## What to include in a question

- The Oly version: `OLY_VERSION` in the `oly.env` file of the game, or the commit of the engine checkout.
- The environment (`web` or `native`) and the operating system.
- The full output of the failing command or the full error message.
- The smallest code or commands that show the problem.
- What you expected and what happened instead.
