import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import summarize


class SummarizeTest(unittest.TestCase):
    def setUp(self):
        self.temporaryDirectory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporaryDirectory.cleanup)
        self.results = self.temporaryDirectory.name

    def writeRun(self, side, scenario, size, repetition, p50, worldHash=None, stepsOverridden=False):
        directory = os.path.join(self.results, side)
        os.makedirs(directory, exist_ok=True)
        measures = {} if worldHash is None else {"worldHash": worldHash}
        run = {
            "commit": side,
            "scenario": scenario,
            "size": size,
            "stepsOverridden": stepsOverridden,
            "step": {"p50": p50, "p99": p50 * 2, "max": p50 * 3},
            "systems": [],
            "flushes": [],
            "measures": measures,
        }

        with open(os.path.join(directory, f"{scenario}-{size}-{repetition}.json"), "w", encoding="utf-8") as handle:
            json.dump(run, handle)

    def summarizeResults(self):
        return summarize.summarize(self.results, "A", "B")["S1-1000"]

    def testRatioIsTheMedianOfBOverTheMedianOfA(self):
        for repetition, (a, b) in enumerate(zip([100, 110, 120, 130], [200, 220, 240, 260]), start=1):
            self.writeRun("a", "S1", 1000, repetition, a)
            self.writeRun("b", "S1", 1000, repetition, b)

        result = self.summarizeResults()

        self.assertEqual(result["ratio"]["p50"], 2.0)
        self.assertEqual(result["pairs"]["p50"]["min"], 2.0)
        self.assertEqual(result["pairs"]["p50"]["max"], 2.0)
        self.assertEqual(result["pairs"]["p50"]["iqr"], 0.0)

    def testPairsIqrUsesInclusiveQuartiles(self):
        for repetition, b in enumerate([100, 110, 120, 130], start=1):
            self.writeRun("a", "S1", 1000, repetition, 100)
            self.writeRun("b", "S1", 1000, repetition, b)

        result = self.summarizeResults()

        self.assertAlmostEqual(result["pairs"]["p50"]["iqr"], 0.15, places=9)
        self.assertAlmostEqual(result["pairs"]["p50"]["min"], 1.0, places=9)
        self.assertAlmostEqual(result["pairs"]["p50"]["max"], 1.3, places=9)

    def testWorldHashEqualIsFalseWhenBDiffers(self):
        for repetition in (1, 2):
            self.writeRun("a", "S1", 1000, repetition, 100, worldHash="0x1")
            self.writeRun("b", "S1", 1000, repetition, 100, worldHash="0x2")

        self.assertFalse(self.summarizeResults()["worldHashEqual"])

    def testWorldHashEqualIsTrueWhenEveryRunAgrees(self):
        for repetition in (1, 2):
            self.writeRun("a", "S1", 1000, repetition, 100, worldHash="0x1")
            self.writeRun("b", "S1", 1000, repetition, 100, worldHash="0x1")

        self.assertTrue(self.summarizeResults()["worldHashEqual"])

    def testSummaryNamesTheStepOverride(self):
        self.writeRun("a", "S1", 1000, 1, 100, stepsOverridden=True)
        self.writeRun("b", "S1", 1000, 1, 100, stepsOverridden=True)

        summarize.summarize(self.results, "A", "B")

        with open(os.path.join(self.results, "summary.md"), encoding="utf-8") as handle:
            self.assertIn("--steps", handle.read())


if __name__ == "__main__":
    unittest.main()
