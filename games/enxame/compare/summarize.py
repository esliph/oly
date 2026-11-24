"""Summarizes the runs of compare.sh: one JSON per scenario and size, and summary.md.

Usage: summarize.py <results> [--a-label <text>] [--b-label <text>]

<results> holds a/ and b/ with one run JSON per scenario, size and repetition, named <scenario>-<size>-<repetition>.json.
"""

import argparse
import json
import os
import re
import statistics
import sys

RUN_NAME = re.compile(r"^(?P<key>S\d+-\d+)-(?P<repetition>\d+)\.json$")
STEP_FIELDS = ("p50", "p99", "max")


def loadRuns(results, side):
    runs = {}
    directory = os.path.join(results, side)

    if not os.path.isdir(directory):
        return runs

    for name in os.listdir(directory):
        match = RUN_NAME.match(name)

        if match:
            with open(os.path.join(directory, name), encoding="utf-8") as handle:
                runs.setdefault(match["key"], {})[int(match["repetition"])] = json.load(handle)

    return runs


def flattenScalars(prefix, value, scalars):
    if isinstance(value, bool):
        return

    if isinstance(value, (int, float)):
        scalars[prefix] = value
    elif isinstance(value, dict):
        for name, child in value.items():
            flattenScalars(f"{prefix}.{name}" if prefix else name, child, scalars)


def median(values):
    return statistics.median(values) if values else None


def sideSummary(runsByRepetition, commit, label):
    runs = [runsByRepetition[repetition] for repetition in sorted(runsByRepetition)]
    systems = {}
    measures = {}

    for run in runs:
        for system in run["systems"]:
            systems.setdefault(system["name"], []).append(system["median"])

        scalars = {}
        flattenScalars("", run["measures"], scalars)

        for name, value in scalars.items():
            measures.setdefault(name, []).append(value)

    return {
        "commit": commit,
        "label": label,
        "runs": runs,
        "median": {
            **{field: median([run["step"][field] for run in runs]) for field in STEP_FIELDS},
            "systems": {name: median(values) for name, values in systems.items()},
            "measures": {name: median(values) for name, values in measures.items()},
        },
    }


def quartileSpread(values):
    if len(values) < 2:
        return 0.0

    quartiles = statistics.quantiles(values, n=4, method="inclusive")

    return quartiles[2] - quartiles[0]


def pairRatios(aRuns, bRuns, field):
    ratios = []

    for repetition in sorted(set(aRuns) & set(bRuns)):
        before = aRuns[repetition]["step"][field]

        if before:
            ratios.append(bRuns[repetition]["step"][field] / before)

    return {
        "values": ratios,
        "min": min(ratios) if ratios else None,
        "max": max(ratios) if ratios else None,
        "iqr": quartileSpread(ratios),
    }


def medianRatio(a, b, field):
    return b["median"][field] / a["median"][field] if a["median"][field] else None


def worldHashes(runs):
    return {run["measures"]["worldHash"] for run in runs if "worldHash" in run["measures"]}


def keyOrder(key):
    scenario, size = key.split("-")

    return int(scenario[1:]), int(size)


def summarizeKey(aRuns, bRuns, aLabel, bLabel):
    first = next(iter(aRuns.values()))
    a = sideSummary(aRuns, first["commit"], aLabel)
    b = sideSummary(bRuns, next(iter(bRuns.values()))["commit"], bLabel)
    result = {
        "scenario": first["scenario"],
        "size": first["size"],
        "a": a,
        "b": b,
        "ratio": {field: medianRatio(a, b, field) for field in ("p50", "p99")},
        "pairs": {field: pairRatios(aRuns, bRuns, field) for field in ("p50", "p99")},
    }
    aHashes = worldHashes(a["runs"])
    bHashes = worldHashes(b["runs"])

    if aHashes or bHashes:
        result["worldHashEqual"] = len(aHashes) == 1 and aHashes == bHashes

    return result


def formatNumber(value, pattern="{:.3f}"):
    return "-" if value is None else pattern.format(value)


def spreadText(pairs):
    return f"{formatNumber(pairs['min'])} to {formatNumber(pairs['max'])} (IQR {formatNumber(pairs['iqr'])})"


def summaryRow(result):
    hashes = result.get("worldHashEqual")
    hashText = "-" if hashes is None else ("equal" if hashes else "DIFFERENT")

    return (
        f"| {result['scenario']} | {result['size']} | {formatNumber(result['a']['median']['p50'], '{:.0f}')} "
        f"| {formatNumber(result['b']['median']['p50'], '{:.0f}')} | {formatNumber(result['ratio']['p50'])} "
        f"| {spreadText(result['pairs']['p50'])} | {formatNumber(result['ratio']['p99'])} "
        f"| {spreadText(result['pairs']['p99'])} | {hashText} |"
    )


def writeSummary(results, summaries, aLabel, bLabel):
    lines = [
        f"# {bLabel} against {aLabel}",
        "",
        f"Ratios are {bLabel} over {aLabel}. Step times are the median over repetitions, in nanoseconds.",
        "",
        f"| scenario | size | {aLabel} p50 | {bLabel} p50 | ratio p50 | pairs p50 | ratio p99 | pairs p99 | world hash |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    lines += [summaryRow(summaries[key]) for key in sorted(summaries, key=keyOrder)]
    overridden = any(run.get("stepsOverridden") for result in summaries.values() for side in ("a", "b") for run in result[side]["runs"])

    if overridden:
        lines += ["", "The runs used `--steps`: a short check of the script, not the frozen length. These numbers are not a baseline."]

    with open(os.path.join(results, "summary.md"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


def summarize(results, aLabel, bLabel):
    aRuns = loadRuns(results, "a")
    bRuns = loadRuns(results, "b")
    summaries = {}

    for key in sorted(set(aRuns) & set(bRuns), key=keyOrder):
        summaries[key] = summarizeKey(aRuns[key], bRuns[key], aLabel, bLabel)

        with open(os.path.join(results, f"{key}.json"), "w", encoding="utf-8", newline="\n") as handle:
            json.dump(summaries[key], handle, indent=2)
            handle.write("\n")

    writeSummary(results, summaries, aLabel, bLabel)

    return summaries


def main(arguments):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("results")
    parser.add_argument("--a-label", default="A")
    parser.add_argument("--b-label", default="B")
    options = parser.parse_args(arguments)
    summaries = summarize(options.results, options.a_label, options.b_label)

    if not summaries:
        print(f"summarize: no run found in {options.results}", file=sys.stderr)

        return 1

    print(f"wrote {len(summaries)} summaries and summary.md in {options.results}")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
