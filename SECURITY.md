# Security policy

## Supported versions

Oly is in `0.x`. Only the latest release receives fixes; a fix is published as a new release, not as a patch of an older one.

## Reporting a vulnerability

Do not open a public issue. Report it privately through GitHub: **Security** tab of the repository, **Report a vulnerability** ([direct link](https://github.com/esliph/oly/security/advisories/new)).

Include what an attacker controls (a `.oly` schema, a game's input, a release download, a container), what they gain, and the steps or code that reproduce it.

Oly is maintained by one person in their spare time. Expect an answer within two weeks. Once the problem is confirmed, the fix and the advisory are published together, crediting the reporter unless they prefer otherwise.

## Scope

In scope: the engine (`engine/`, `plugins/`), the `oly-schema` generator, the `oly` launcher and `install.sh`, the release pipeline and the published images and packages.

Out of scope: the example games in `games/`, and problems that need the attacker to already run code on the machine or inside the game.
