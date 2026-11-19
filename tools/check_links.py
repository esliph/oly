#!/usr/bin/env python3
"""Rejects relative links and anchors of markdown files that do not resolve the way GitHub resolves them.

Usage: python3 -I tools/check_links.py --root <repo>
Exits 0 with no broken link; exits 1 printing "<file>:<line> | <target> | <reason>" per broken link, sorted by file then line.
"""
import argparse
import os
import re
import sys
import urllib.parse

IGNORED_DIRECTORIES = {".git", "node_modules", ".superpowers"}
IGNORED_DIRECTORY_PREFIX = "build"
MARKDOWN_EXTENSION = ".md"

FENCE_PATTERN = re.compile(r"^\s*(`{3,}|~{3,})(.*)$")
INLINE_CODE_PATTERN = re.compile(r"(?<!`)(`+)(?!`).+?(?<!`)\1(?!`)")
INLINE_LINK_PATTERN = re.compile(r"""\[[^\]]*\]\(\s*(<[^>]*>|[^)\s]*)(?:\s+(?:"[^"]*"|'[^']*'))?\s*\)""")
REFERENCE_DEFINITION_PATTERN = re.compile(r"^ {0,3}\[[^\]]+\]:\s*(<[^>]*>|\S+)")
ATX_HEADING_PATTERN = re.compile(r"^ {0,3}#{1,6}[ \t]+(.*?)(?:[ \t]+#+)?[ \t]*$")
SETEXT_UNDERLINE_PATTERN = re.compile(r"^ {0,3}(?:=+|-+)[ \t]*$")
HTML_ANCHOR_PATTERN = re.compile(r"""<a\s[^>]*?\b(?:name|id)\s*=\s*["']([^"']+)["']""")
SCHEME_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:")

LINK_TEXT_PATTERN = re.compile(r"\[([^\]]*)\]\([^)]*\)")
STRONG_PATTERN = re.compile(r"(\*\*|__)(?=\S)(.+?)(?<=\S)\1")
EMPHASIS_PATTERN = re.compile(r"(?<![\w*])\*(?=\S)(.+?)(?<=\S)\*(?!\w)|(?<![\w_])_(?=\S)(.+?)(?<=\S)_(?![\w_])")
HTML_TAG_PATTERN = re.compile(r"<[^>]+>")


def iter_lines(text):
    """Yields (line number, line, inside a fence or on a fence line), tracking fences the way GitHub renders them."""
    fence_character = None
    fence_length = 0
    for line_number, line in enumerate(text.split("\n"), start=1):
        line = line.rstrip("\r")
        match = FENCE_PATTERN.match(line)
        if fence_character is None:
            opens = match is not None and not (match.group(1)[0] == "`" and "`" in match.group(2))
            if opens:
                fence_character = match.group(1)[0]
                fence_length = len(match.group(1))
            yield line_number, line, opens
            continue

        closes = match is not None and match.group(1)[0] == fence_character and len(match.group(1)) >= fence_length and not match.group(2).strip()
        if closes:
            fence_character = None
        yield line_number, line, True


def without_inline_code(line):
    return INLINE_CODE_PATTERN.sub(lambda match: " " * len(match.group(0)), line)


def slugify(heading):
    code_spans = []

    def hide_code_span(match):
        fence = len(match.group(1))
        code_spans.append(match.group(0)[fence:-fence])
        return f"\ue000{len(code_spans) - 1}\ue001"

    text = INLINE_CODE_PATTERN.sub(hide_code_span, heading)
    text = LINK_TEXT_PATTERN.sub(r"\1", text)
    text = STRONG_PATTERN.sub(r"\2", text)
    text = EMPHASIS_PATTERN.sub(lambda match: match.group(1) or match.group(2), text)
    text = HTML_TAG_PATTERN.sub("", text)
    text = re.sub(r"\ue000(\d+)\ue001", lambda match: code_spans[int(match.group(1))], text)
    text = text.strip().lower()
    return "".join(character for character in text if character.isalnum() or character in "-_ ").replace(" ", "-")


def anchors_of(text):
    anchors = set()
    occurrences = {}
    previous_line = ""
    for _, line, inside_fence in iter_lines(text):
        if inside_fence:
            previous_line = ""
            continue

        stripped_line = without_inline_code(line)
        heading = None
        atx_match = ATX_HEADING_PATTERN.match(line)
        if atx_match is not None:
            heading = atx_match.group(1)
        elif SETEXT_UNDERLINE_PATTERN.match(line) and previous_line.strip():
            heading = previous_line

        if heading is not None:
            slug = slugify(heading)
            count = occurrences.get(slug, 0)
            occurrences[slug] = count + 1
            anchors.add(slug if count == 0 else f"{slug}-{count}")
            previous_line = ""
        else:
            previous_line = line

        for match in HTML_ANCHOR_PATTERN.finditer(stripped_line):
            anchors.add(match.group(1))

    return anchors


def links_of(text):
    links = []
    for line_number, line, inside_fence in iter_lines(text):
        if inside_fence:
            continue

        stripped_line = without_inline_code(line)
        definition = REFERENCE_DEFINITION_PATTERN.match(stripped_line)
        if definition is not None:
            links.append((line_number, definition.group(1)))
        for match in INLINE_LINK_PATTERN.finditer(stripped_line):
            links.append((line_number, match.group(1)))

    return links


def read_markdown(path):
    with open(path, encoding="utf-8-sig", errors="replace") as handle:
        return handle.read().replace("\r\n", "\n")


def markdown_files(root):
    for current, directories, files in os.walk(root):
        directories[:] = [
            directory for directory in directories if directory not in IGNORED_DIRECTORIES and not directory.startswith(IGNORED_DIRECTORY_PREFIX)
        ]
        for file_name in files:
            if file_name.endswith(MARKDOWN_EXTENSION):
                yield os.path.join(current, file_name)


class Resolver:
    def __init__(self, root):
        self._root = os.path.abspath(root)
        self._listings = {}
        self._anchors = {}

    def resolve(self, source_path, target_path):
        """Returns the existing path a link points to, comparing each component with the case on disk, or None."""
        if target_path.startswith("/"):
            joined = os.path.join(self._root, target_path.lstrip("/"))
        else:
            joined = os.path.join(os.path.dirname(source_path), target_path)

        relative = os.path.relpath(os.path.normpath(joined), self._root)
        if relative == os.curdir:
            return self._root
        if relative.split(os.sep)[0] == os.pardir:
            return None

        current = self._root
        for component in relative.split(os.sep):
            if component not in self._listing(current):
                return None
            current = os.path.join(current, component)

        return current

    def has_anchor(self, path, fragment):
        if path not in self._anchors:
            self._anchors[path] = anchors_of(read_markdown(path))
        return fragment in self._anchors[path] or fragment.lower() in self._anchors[path]

    def _listing(self, directory):
        if directory not in self._listings:
            try:
                self._listings[directory] = set(os.listdir(directory))
            except OSError:
                self._listings[directory] = set()
        return self._listings[directory]


def broken_reason(resolver, source_path, raw_target):
    target = raw_target[1:-1] if raw_target.startswith("<") and raw_target.endswith(">") else raw_target
    if not target or SCHEME_PATTERN.match(target) or target.startswith("//"):
        return None

    path_part, _, fragment = target.partition("#")
    path_part = urllib.parse.unquote(path_part)
    resolved = source_path if not path_part else resolver.resolve(source_path, path_part)
    if resolved is None:
        return "file not found"
    if fragment and resolved.endswith(MARKDOWN_EXTENSION) and os.path.isfile(resolved):
        if not resolver.has_anchor(resolved, urllib.parse.unquote(fragment)):
            return "anchor not found"

    return None


def check(root):
    resolver = Resolver(root)
    broken = []
    for path in markdown_files(root):
        relative_path = os.path.relpath(path, root).replace(os.sep, "/")
        for line_number, raw_target in links_of(read_markdown(path)):
            reason = broken_reason(resolver, path, raw_target)
            if reason is not None:
                broken.append((relative_path, line_number, raw_target, reason))

    return [f"{file}:{line} | {target} | {reason}" for file, line, target, reason in sorted(broken)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True)
    arguments = parser.parse_args(argv)

    broken = check(arguments.root)
    for message in broken:
        print(message)
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
