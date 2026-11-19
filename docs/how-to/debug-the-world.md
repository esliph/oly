# Debug the world

Use this when a test or a game shows a wrong `World`: a missing component, a stale handle or a crash far from its cause. It assumes you have a `World` you can reach, in a test or in a tool.

## 1. Check the invariants after each step

`checkInvariants` catches corruption at the step that caused it, so call it after each structural step of a test.

```cpp
#include "oly/ecs/diagnostics.hpp"

std::string why;
if (!oly::ecs::checkInvariants(world, &why)) {
  FAIL(why);
}
```

`why` names the broken invariant and the object, for example `I-3 e=4:1`.

In TypeScript the call is a method of the world, and it returns `null` when the world is consistent:

```ts
const problem = world.checkInvariants();
if (problem !== null) {
  throw new Error(problem);
}
```

## 2. Read the statistics

When the invariants hold but the world is not what you expect, look at its shape.

```cpp
oly::ecs::WorldStats statistics = oly::ecs::stats(world);
```

`statistics.archetypes` lists each archetype with its entity count and its memory, and `statistics.liveEntities` counts the live entities. What archetype churn costs: [Performance](../explanation/concepts/performance.md).

## 3. Test the rest

Run `checkInvariants` inside a test of your game: [Test a desktop game](test-a-desktop-game.md) in C++, [Test a web game](test-a-web-game.md) in TypeScript.

## See also

- [Direct and deferred changes](../explanation/concepts/world-and-entities.md#direct-and-deferred-changes): why a handle can be alive or dead at a given moment.
- [Debugging the world in C++](../reference/cpp/world-and-entities.md#debugging-the-world): `checkInvariants` and `stats`.
- [Debugging the world in TypeScript](../reference/typescript/world-and-entities.md#debugging-the-world): `World.checkInvariants`.
