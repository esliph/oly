import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import check_readme_example

TEMPLATE_PATH = "templates/x.cpp"
TEMPLATE_BODY = "int main() {\n  return 0;\n}\n"


class CheckReadmeExampleTest(unittest.TestCase):
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
            code = check_readme_example.main(["--root", self.root])

        return code, output.getvalue()

    def example_readme(self, body, marker=TEMPLATE_PATH):
        return f"# Oly\n\n## Minimal example\n\n<!-- template: {marker} -->\n```cpp\n{body}```\n\n## License\n"

    def test_matching_block_passes(self):
        self.write(TEMPLATE_PATH, TEMPLATE_BODY)
        self.write("README.md", self.example_readme(TEMPLATE_BODY))

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_different_block_fails(self):
        self.write(TEMPLATE_PATH, TEMPLATE_BODY)
        self.write("README.md", self.example_readme(TEMPLATE_BODY.replace("0", "1")))

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "README.md:6 | templates/x.cpp | block differs from the template\n"))

    def test_crlf_template_matches(self):
        self.write(TEMPLATE_PATH, TEMPLATE_BODY.replace("\n", "\r\n"))
        self.write("README.md", self.example_readme(TEMPLATE_BODY))

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_trailing_blank_lines_ignored(self):
        self.write(TEMPLATE_PATH, TEMPLATE_BODY + "\n")
        self.write("README.md", self.example_readme(TEMPLATE_BODY))

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))

    def test_block_without_marker_fails(self):
        self.write("README.md", "## Minimal example\n\ntext\n```cpp\nint x;\n```\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "README.md:4 | - | block names no template\n"))

    def test_missing_template_fails(self):
        self.write("README.md", self.example_readme(TEMPLATE_BODY, marker="templates/missing.cpp"))

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "README.md:6 | templates/missing.cpp | template not found\n"))

    def test_missing_section_fails(self):
        self.write("README.md", "# Oly\n\n## Install\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "README.md | ## Minimal example | section not found\n"))

    def test_section_without_blocks_fails(self):
        self.write("README.md", "## Minimal example\n\nOnly prose.\n\n## License\n")

        code, output = self.run_check()

        self.assertEqual((code, output), (1, "README.md | ## Minimal example | section has no code block\n"))

    def test_blocks_outside_section_ignored(self):
        self.write(TEMPLATE_PATH, TEMPLATE_BODY)
        install = "## Install\n\n```sh\ncurl x | sh\n```\n\n"
        self.write("README.md", install + self.example_readme(TEMPLATE_BODY))

        code, output = self.run_check()

        self.assertEqual((code, output), (0, ""))


if __name__ == "__main__":
    unittest.main()
