#!/usr/bin/env python3
"""Rejects code blocks of the README's "Minimal example" section that differ from the template file they name.

Usage: python3 -I tools/check_readme_example.py --root <repo>
A block names its template in the line right before its opening fence: <!-- template: <path from the root> -->.
Exits 0 when every block equals its template after trimming; exits 1 printing "<file>:<line> | <template> | <reason>" per problem.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from check_links import FENCE_PATTERN, iter_lines

README_NAME = "README.md"
SECTION_HEADING = "## Minimal example"
SECTION_PREFIX = "## "
MARKER_PATTERN = re.compile(r"^<!-- template: (\S+) -->$")


def section_blocks(text):
    """Returns the code blocks of the section as (fence line number, marker line or None, content), or None without the section."""
    blocks = []
    in_section = False
    found = False
    previous_line = ""
    previous_inside = False
    block = None

    for line_number, line, inside_fence in iter_lines(text):
        if not inside_fence and line.startswith(SECTION_PREFIX):
            in_section = line.rstrip() == SECTION_HEADING
            found = found or in_section

        if in_section and inside_fence and not previous_inside:
            block = (line_number, previous_line, [])
        elif in_section and inside_fence:
            block[2].append(line)
        if block is not None and not inside_fence:
            blocks.append(finish(block))
            block = None

        previous_line = line
        previous_inside = inside_fence

    if block is not None:
        blocks.append(finish(block))

    return blocks if found else None


def finish(block):
    line_number, marker_line, lines = block
    if lines and FENCE_PATTERN.match(lines[-1]):
        lines = lines[:-1]
    return line_number, marker_line, "\n".join(lines)


def read_text(path):
    with open(path, encoding="utf-8-sig", errors="replace", newline="") as handle:
        return handle.read().replace("\r\n", "\n")


def check(root):
    readme_text = read_text(os.path.join(root, README_NAME))
    blocks = section_blocks(readme_text)
    if blocks is None:
        return [f"{README_NAME} | {SECTION_HEADING} | section not found"]
    if not blocks:
        return [f"{README_NAME} | {SECTION_HEADING} | section has no code block"]

    problems = []
    for line_number, marker_line, content in blocks:
        marker = MARKER_PATTERN.match(marker_line)
        if marker is None:
            problems.append(f"{README_NAME}:{line_number} | - | block names no template")
            continue

        template = marker.group(1)
        template_path = os.path.join(root, template)
        if not os.path.isfile(template_path):
            problems.append(f"{README_NAME}:{line_number} | {template} | template not found")
        elif read_text(template_path).strip() != content.strip():
            problems.append(f"{README_NAME}:{line_number} | {template} | block differs from the template")

    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True)
    arguments = parser.parse_args(argv)

    problems = check(arguments.root)
    for message in problems:
        print(message)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
