import contextlib
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import check_links


class CheckLinksTest(unittest.TestCase):
    def setUp(self):
        self.temporaryDirectory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporaryDirectory.cleanup)
        self.root = self.temporaryDirectory.name

    def write(self, relativePath, content):
        path = os.path.join(self.root, relativePath)
        os.makedirs(os.path.dirname(path), exist_ok=True)

        with open(path, "w", encoding="utf-8", newline="") as handle:
            handle.write(content)

    def run_check(self):
        output = io.StringIO()

        with contextlib.redirect_stdout(output):
            code = check_links.main(["--root", self.root])

        return code, output.getvalue()

    def test_valid_relative_link_passes(self):
        self.write("a.md", "[b](b.md)\n")
        self.write("b.md", "# B\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_missing_file_fails(self):
        self.write("a.md", "text\n[b](missing.md)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "a.md:2 | missing.md | file not found\n"))

    def test_missing_anchor_fails(self):
        self.write("b.md", "# Title\n")
        self.write("a.md", "[b](b.md#other)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "a.md:1 | b.md#other | anchor not found\n"))

    def test_same_file_anchor(self):
        self.write("a.md", "# Title\n[x](#title)\n[y](#nope)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "a.md:3 | #nope | anchor not found\n"))

    def test_slug_rules(self):
        cases = {
            "Systems in C++": "systems-in-c",
            "`oly new`": "oly-new",
            "Menu → playing → game over": "menu--playing--game-over",
            "Diátaxis types": "diátaxis-types",
            "WASM and TypeScript": "wasm-and-typescript",
            "[Link](x.md) text": "link-text",
            "snake_case name": "snake_case-name",
            "**Bold** part": "bold-part",
            "Text <b>bold</b> end": "text-bold-end",
            "`a<b>c`": "abc",
            "`World::get<T>`": "worldgett",
            "`actions::<name>`": "actionsname",
        }

        for heading, expected in cases.items():
            with self.subTest(heading=heading):
                self.assertEqual(check_links.slugify(heading), expected)

    def test_duplicate_headings_get_suffix(self):
        anchors = check_links.anchors_of("## Next\n\n## Next\n")

        self.assertIn("next", anchors)
        self.assertIn("next-1", anchors)

    def test_html_anchor(self):
        anchors = check_links.anchors_of('<a name="custom"></a>\n<a id="other"></a>\n')

        self.assertIn("custom", anchors)
        self.assertIn("other", anchors)

    def test_setext_heading(self):
        anchors = check_links.anchors_of("Title\n=====\n\nparagraph\nSub\n---\n\n---\n")

        self.assertEqual(anchors, {"title", "sub"})

    def test_code_is_ignored(self):
        lines = [
            "```",
            "[x](m.md)",
            "# Fake",
            "```",
            "~~~",
            "[x](m.md)",
            "~~~",
            "````",
            "```",
            "[x](m.md)",
            "```",
            "````",
            "- item",
            "",
            "    ```",
            "    [x](m.md)",
            "    ```",
            "",
            "`[x](m.md)` and ``[x](m.md)`` and ``a ` [x](m.md)``",
            "[y](#fake)",
        ]
        self.write("a.md", "\n".join(lines) + "\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, f"a.md:{len(lines)} | #fake | anchor not found\n"))

    def test_unclosed_fence_runs_to_end(self):
        self.write("a.md", "text\n```\n[x](missing.md)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_link_forms(self):
        self.write("b.md", "# B\n")
        self.write("b c.md", "# B\n")
        self.write("pic.png", "")
        self.write("a.md", '[t](b.md "Title")\n[t](<b c.md>)\n[t](b%20c.md)\n![i](pic.png)\n[ref]: b.md\n')
        self.write("z.md", '[t](nb.md "Title")\n[t](<nb c.md>)\n[t](nb%20c.md)\n![i](npic.png)\n[ref]: nb.md\n')

        code, output = self.run_check()

        expected = (
            "z.md:1 | nb.md | file not found\n"
            "z.md:2 | <nb c.md> | file not found\n"
            "z.md:3 | nb%20c.md | file not found\n"
            "z.md:4 | npic.png | file not found\n"
            "z.md:5 | nb.md | file not found\n"
        )
        self.assertEqual((code, output), (1, expected))

    def test_directory_link_passes(self):
        self.write("sub/x.txt", "")
        self.write("a.md", "[d](sub/)\n[d](sub)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_case_mismatch_fails(self):
        self.write("Guide.md", "# Guide\n")
        self.write("a.md", "[g](guide.md)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "a.md:1 | guide.md | file not found\n"))

    def test_external_and_scheme_links_skipped(self):
        self.write("a.md", "[a](https://x/missing)\n[b](mailto:a@b)\n[c](//host/x)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_non_markdown_anchor_not_checked(self):
        self.write("code.cpp", "int main() {}\n")
        self.write("a.md", "[x](code.cpp#L10)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_root_relative_link(self):
        self.write("docs/a.md", "# A\n")
        self.write("sub/b.md", "[x](/docs/a.md)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_ignored_directories(self):
        for directory in (".git", "node_modules", "build", "build-asan", ".superpowers"):
            self.write(f"{directory}/x.md", "[x](missing.md)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_crlf_and_bom(self):
        self.write("b.md", "﻿# Title\r\n")
        self.write("a.md", "[b](b.md#title)\r\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_output_sorted(self):
        self.write("z.md", "[x](m.md)\n")
        self.write("a.md", "[x](m1.md)\ntext\n[x](m3.md)\n")

        code, output = self.run_check()

        expected = (
            "a.md:1 | m1.md | file not found\n"
            "a.md:3 | m3.md | file not found\n"
            "z.md:1 | m.md | file not found\n"
        )
        self.assertEqual((code, output), (1, expected))

    def test_missing_root_fails(self):
        missing = os.path.join(self.root, "missing")
        errors = io.StringIO()

        with contextlib.redirect_stderr(errors):
            code = check_links.main(["--root", missing])

        self.assertEqual((code, errors.getvalue()), (1, f"{missing}: root folder not found\n"))

    def test_link_to_untracked_file_fails_in_a_git_repository(self):
        if shutil.which("git") is None:
            self.skipTest("git is not installed")

        self.write(".gitignore", "ignored.txt\n")
        self.write("tracked.md", "# Tracked\n")
        self.write("untracked.md", "# Untracked\n")
        self.write("ignored.txt", "")
        self.write("a.md", "[t](tracked.md)\n[u](untracked.md)\n[i](ignored.txt)\n[d](sub/)\n")
        self.write("sub/x.md", "# X\n")
        subprocess.run(["git", "init", "-q"], cwd=self.root, check=True)
        subprocess.run(["git", "add", ".gitignore", "tracked.md", "a.md", "sub/x.md"], cwd=self.root, check=True)

        code, output = self.run_check()

        expected = "a.md:2 | untracked.md | file not found\na.md:3 | ignored.txt | file not found\n"
        self.assertEqual((code, output), (1, expected))

    def test_heading_inside_blockquote_has_anchor(self):
        anchors = check_links.anchors_of("> ## Quoted\n>\n> text\n")

        self.assertEqual(anchors, {"quoted"})

    def test_indented_fence_outside_a_list_is_code_block(self):
        self.write("a.md", "text\n\n    ```\n\n[x](missing.md)\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "a.md:5 | missing.md | file not found\n"))

    def test_nested_brackets_and_parentheses_in_links(self):
        self.write("b.md", "# B\n")
        self.write("a(b).md", "# AB\n")
        self.write("a.md", "[a [b]](b.md)\n[t](a(b).md)\n[a [b]](nb.md)\n[t](na(b).md)\n")

        code, output = self.run_check()

        expected = "a.md:3 | nb.md | file not found\na.md:4 | na(b).md | file not found\n"
        self.assertEqual((code, output), (1, expected))

    def test_intraword_double_underscore_is_not_strong(self):
        cases = {
            "x__y__z": "x__y__z",
            "__strong__ text": "strong-text",
        }

        for heading, expected in cases.items():
            with self.subTest(heading=heading):
                self.assertEqual(check_links.slugify(heading), expected)

    def test_non_ascii_target_prints_on_a_narrow_console(self):
        self.write("a.md", "[x](a→.md)\n")
        console = io.TextIOWrapper(io.BytesIO(), encoding="cp1252", errors="strict", write_through=True)

        with contextlib.redirect_stdout(console):
            code = check_links.main(["--root", self.root])

        self.assertEqual((code, console.buffer.getvalue()), (1, b"a.md:1 | a\\u2192.md | file not found\n"))


if __name__ == "__main__":
    unittest.main()
