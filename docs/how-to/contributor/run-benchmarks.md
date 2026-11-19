# Run the benchmarks

[Docs](../../README.md#how-to-guides) · Reference: [Benchmark results](../../reference/benchmarks/README.md)

Use this to measure the engine and record the result in `docs/reference/benchmarks/`. It assumes the images are built ([Set up the toolchain](set-up-the-toolchain.md)). The environment (Docker on WSL2) is noisy: always compare ratios measured in the same run, never the absolute times of two runs.

## 1. Run an ECS benchmark

```sh
PRESET=release ./spur build
./spur dev ./build/release/bench/ecs_bench_iterate --benchmark_repetitions=10 --benchmark_report_aggregates_only=true --benchmark_out=docs/reference/benchmarks/ecs/<file>.json
```

The executables are `ecs_bench_iterate`, `ecs_bench_transitions`, `ecs_bench_fragmented` and `ecs_bench_flush`. Their sources are in `engine/runtime/ecs/bench/`.

## 2. Compute the ratio

```sh
./spur dev python3 -I tools/bench_ratio.py --json docs/reference/benchmarks/ecs/<file>.json --name <bench> --baseline <bench> --max <ratio>
```

The tool prints `ratio=<x>`. It exits with 0 when the ratio is at most `--max`, with 1 when it is greater and with 2 when a name does not exist in the JSON. Both benchmarks come from the same file.

The `/bench-compare <prefix>` skill runs steps 1 and 2 for NF-P1, NF-P2 and NF-P5, and writes the result into `docs/reference/benchmarks/`.

## 3. Run a WASM boundary benchmark

The boundary, loop and event benchmarks are in `engine/testing/bench/` (`boundary.ts`, `loop.ts`, `events.ts`). They run in Node against a game built in Release:

```sh
./spur ts
PRESET=wasm-games-release ./spur build
./spur wasm node engine/testing/dist/bench/boundary.js --module build/wasm-games-release/games/fixture_game.mjs --out docs/reference/benchmarks/bridge/<file>.json --commit $(git rev-parse --short HEAD)
```

Replace `boundary.js` with `loop.js` or `events.js` for the other two.

## 4. Run the other tools

- `tools/perf_fragmented.sh <ecs_bench_fragmented> <output>` runs `perf stat` of `BM_IterateFragmented` for each archetype count and writes one CSV line per count. Run it in the `perf` service (privileged), on the `relwithdebinfo` build.
- `tools/run_ladder.sh <rounds> <bench> [variants...]` runs the native × WASM ladder interleaved and writes one JSON per round and variant into `docs/reference/benchmarks/wasm/runs/<bench>`. Run it on the host, with the binaries of every preset of the ladder already built.

## See also

- [Benchmark results](../../reference/benchmarks/README.md): the targets and the recorded runs.
- [Commands](../../reference/contributor/commands.md): the `./spur` tasks.
- [Keep the hot path fast](../keep-the-hot-path-fast.md): what the numbers mean for a game.
