# Concepts

Each page explains the rules a game relies on. Read them in this order; each one ends with a link to the next. The API of a concept is in `reference/cpp/` and `reference/typescript/`, under the same file name.

| Page | What it teaches |
|---|---|
| [Environments and hosts](../../guide/README.md) | what the core, an environment, a host and a client are, and how a game is assembled |
| [Schema](schema.md) | how a `.oly` file declares components, tags, events, actions and states |
| [World and entities](world-and-entities.md) | who owns the `World`, what a handle is, which changes are direct and which are deferred, observers and implied components |
| [Queries](queries.md) | how to visit many entities at once |
| [Engine, loop and systems](engine-and-systems.md) | how a frame runs the fixed step, the phases and the systems |
| [Events](events.md) | how systems and handlers talk through batches and rounds |
| [Input](input.md) | how actions turn physical input into pressed, held and released |
| [Schedule](schedule.md) | how to run code after a delay or every interval |
| [States](states.md) | how to switch between game states and scope entities to them |
| [Performance](../../guide/concepts/performance.md) | what changes cost and how to keep the hot path fast |
| [Plugins](../../guide/concepts/plugins.md) | how a game turns on engine-level features |
| [Transforms](../../guide/concepts/transforms.md) | how `Position`, `Parent` and `WorldTransform` work together |
| [Web boundary](../../guide/environments/web/boundary.md) | why work crosses between C++ and TypeScript by block |
