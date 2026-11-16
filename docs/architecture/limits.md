# Known limitations and out of scope

- **Out of the engine's scope**: render, physics, audio, camera, UI, assets, networking, serialization, editor, threads and gamepad. Render and collision belong to the game ([games.md](games.md)).
- **No shared library for foreign clients yet.** The C API headers are valid C and a C test drives an engine through them (`capi.foreign_client`), but nothing builds a `.so` or `.dll` that another language could load.
- **Windows is checked under Wine and by hand only.** The whole test suite runs under Wine (`./spur windows-test`), but no real Windows machine runs it, and the `.exe` is opened by hand only.
- **The Linux window is checked by hand only.** The tests use SDL's dummy video driver; the X11 and Wayland backends are checked to be compiled in, not to open a window.
- **ECS v1**: no fixed-size chunks, no change detection, no enableable components, no sparse storage. `ComponentId` is only valid inside the process.
- **Observers do not see direct field writes.** A write through `get`, `tryGet` or a query runs no observer; only `add`, `remove`, `destroy`, `spawn` and the flush do.
- **A cycle of observers can loop forever.** Commands an observer records are applied until nothing is pending; observers that always record more commands never stop. Nothing guards it.
- **An implied component leaves with its last implier**, even when the game added it explicitly before.
- **With implications, a flush can differ from applying its commands one by one.** The flush works on the net change, which keeps no order between components: an implied component present before and after keeps its value (one by one, losing its last implier and getting a new one value-initializes it), and a component the buffer adds stays even when the buffer removes its implier after it.
- **NF-P5** is not met at N = 100000 ([performance.md](performance.md)).
- **Two `Engine`s on the same `World`** share the scope tag: a state change in one cleans up the scoped entities of the other. `capi` avoids this by creating one `World` per `OlyEngine`.
- **Arena workarounds** (recorded in `games/arena/README.md`): the right-click menu is prevented by `main.ts` itself, because `attachBrowserInput` only prevents it when `MouseRight` is bound to an action.
