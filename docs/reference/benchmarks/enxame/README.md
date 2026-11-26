# Enxame phase 0 baseline

[Reference](../../../README.md#reference) · Related: [Benchmark results](../README.md)

What the Enxame scenarios measured at the end of phase 0, and how much of it is noise. Later phases compare against these numbers; the noise table says how close to 1 a ratio has to be before it means anything.

## Method

- Commit measured (C0): `5eec212`, the commit that completed `games/enxame/compare/`.
- Command: `sh games/enxame/compare/compare.sh 5eec212 5eec212`, an A/A run: the same commit built twice in two worktrees (`oly build --release`), then 10 repetitions of each run, A then B, one process at a time.
- Frozen sizes and steps of each scenario; no step override.
- Machine: Docker container (gcc:14) on WSL2/Windows 11, 12 CPUs at ~3.6 GHz, L1d 32 KiB, L2 512 KiB, L3 16 MiB.
- The raw summaries are the `5eec212-aa-<scenario>-<size>.json` files in this folder; each holds every repetition's run.

## Baseline

Median over the 10 repetitions of A, in microseconds. Absolute numbers are context only.

| scenario | size | step p50 (µs) | step p99 (µs) | step max (µs) | slowest systems (median µs) |
|---|---|---|---|---|---|
| S1 | 1000 | 153.0 | 296.9 | 478.6 | `rebuildSpatialHash` 105.7, `moveHorde` 45.5, `propagateTransforms` 0.9 |
| S1 | 10000 | 786.8 | 1537.6 | 2143.4 | `moveHorde` 553.7, `rebuildSpatialHash` 222.6, `propagateTransforms` 6.7 |
| S1 | 100000 | 13836.1 | 16158.7 | 17402.3 | `moveHorde` 12418.8, `rebuildSpatialHash` 1308.6, `propagateTransforms` 99.6 |
| S2 | 50000 | 51.2 | 108.3 | 153.1 | `moveBeltItems` 26.7, `propagateTransforms` 23.3, `runAssemblers` 0.5 |
| S3 | 10000 | 931.8 | 1163.4 | 1530.1 | `moveHorde` 533.6, `rebuildSpatialHash` 204.6, `toggleStatus` 45.5 |
| S5 | 2000 | 24.5 | 209.4 | 1727.2 | `moveHorde` 12.8, `rebuildSpatialHash` 8.0, `fireTurrets` 2.1 |
| S6 | 1000 | 120.2 | 152.3 | 215.1 | `propagateTransforms` 110.9, `moveVehicles` 9.0 |
| S8 | 2000 | 2.3 | 875.4 | 2412.1 | `propagateTransforms` 2.0, `runWaves` 0.0 |

Measures of each scenario:

- **S2**: allocations per step (`operator new` only): p50 21, max 38, 13127 in total.
- **S3**: 1002 transitions per step at p50 (max 1104); the `UpdateAfter` flush costs 135.8 µs at the median.
- **S5**: the world hash is equal in every A run, in every B run and between A and B (`0x21c16d888746fd84`); 42 turrets, no enemy left.
- **S6**: 5000 visited entities; `propagateTransforms` costs 22.2 ns per visited entity. The review measured 26–36 ns, but on children alone: context only.
- **S8**: after the last wave (50), 257 archetypes, 589824 bytes of columns, 5654528 bytes resident (median).

## Noise

Per-pair ratios `B_i / A_i` of the A/A run: both sides are the same commit, so any distance from 1 is noise.

| scenario | size | p50 min | p50 max | p50 IQR | p99 min | p99 max | p99 IQR |
|---|---|---|---|---|---|---|---|
| S1 | 1000 | 0.570 | 1.011 | 0.079 | 0.176 | 1.240 | 0.337 |
| S1 | 10000 | 0.957 | 1.406 | 0.028 | 0.691 | 5.226 | 0.631 |
| S1 | 100000 | 0.924 | 1.023 | 0.029 | 0.385 | 1.131 | 0.045 |
| S2 | 50000 | 0.994 | 1.011 | 0.007 | 0.949 | 1.244 | 0.048 |
| S3 | 10000 | 0.979 | 1.013 | 0.009 | 0.782 | 1.191 | 0.136 |
| S5 | 2000 | 0.992 | 1.013 | 0.010 | 0.903 | 1.143 | 0.017 |
| S6 | 1000 | 0.979 | 1.013 | 0.006 | 0.813 | 1.096 | 0.104 |
| S8 | 2000 | 0.955 | 1.082 | 0.082 | 0.985 | 1.042 | 0.015 |

Rule: a later gate closer to 1 than the spread of its scenario cannot be trusted. The p99 columns are the wide ones (S1 at 10000 reaches 5.2 in one pair); gate on p50 and read p99 as context.

## Timing overhead

`compare.sh --overhead 5eec212`: S1 at 10000, 10 repetitions, `Engine::setTimingEnabled(false)` against `true`.

| timing off p50 (µs) | timing on p50 (µs) | ratio p50 | pairs p50 | ratio p99 |
|---|---|---|---|---|
| 753.6 | 751.1 | 0.997 | 0.981 to 1.017 (IQR 0.013) | 1.013 |

The ratio sits inside the noise of the same scenario (IQR 0.028 in the A/A run): the "~1 µs per step" hypothesis is not contradicted, and the overhead is not measurable at this size. The raw summary is `5eec212-overhead-S1-10000.json`.

## Phase 1

The cost of declared access and the new dispatch against phase 0. Not a gate: the ratios are recorded, with no target.

- Command: `sh games/enxame/compare/compare.sh 5eec212 ca934ce`, A is phase 0 (`5eec212`) and B is the end of phase 1 (`ca934ce`), built in two worktrees (`oly build --release`), 10 repetitions each, alternating, same machine and method as above.
- The raw summaries are the `5eec212-ca934ce-<scenario>-<size>.json` files in this folder.

Ratios are B over A. Step times are the median over repetitions.

| scenario | size | A p50 (µs) | B p50 (µs) | ratio p50 | pairs p50 | ratio p99 | against the A/A noise of p50 |
|---|---|---|---|---|---|---|---|
| S1 | 1000 | 152.5 | 153.4 | 1.006 | 0.998 to 1.015 | 0.942 | inside |
| S1 | 10000 | 764.2 | 767.3 | 1.004 | 0.973 to 1.021 | 0.936 | inside |
| S1 | 100000 | 14390.5 | 14273.6 | 0.992 | 0.954 to 1.042 | 0.963 | inside |
| S2 | 50000 | 51.4 | 50.7 | 0.985 | 0.900 to 0.994 | 1.006 | outside, 0.9 points faster than the A/A minimum |
| S3 | 10000 | 951.6 | 967.0 | 1.016 | 0.999 to 1.033 | 0.985 | outside, 0.3 points slower than the A/A maximum |
| S5 | 2000 | 25.3 | 25.5 | 1.009 | 0.913 to 1.176 | 0.973 | inside |
| S6 | 1000 | 122.1 | 128.1 | 1.049 | 0.905 to 1.071 | 1.029 | outside, 3.6 points slower than the A/A maximum |
| S8 | 2000 | 2.45 | 2.50 | 1.019 | 0.668 to 1.411 | 1.017 | inside |

- **S5 world hash**: equal on both sides (`0x21c16d888746fd84`). S5 is the only scenario whose phase 0 run wrote a hash, so the summary marks the others `DIFFERENT`: the A side has none. Their phase 0 values are recorded as the `enxame.hash.*` tests of `games/enxame/tests`, which pass at `ca934ce`.
- Six of the eight p50 ratios are inside the A/A spread of phase 0 (0.985 to 1.019 for the ones outside it or at its edge). S6 leaves it, by 3.6 points (`propagateTransforms` runs in its own phase now, with its own flush); S2 and S3 leave it by 1 point or less. The p99 ratios are inside the wide p99 spread of every scenario.

## Blocked scenarios

- **S4**: blocked, the engine has no stable ids. The baseline is taken in phase 2.
- **S7**: blocked, the engine has no assets. The baseline is taken in phase 5.

## Limits

- Absolute numbers are context; compare ratios measured in the same session.
- Allocations count `operator new` only.
- One machine, one container: the noise above is this environment's.
