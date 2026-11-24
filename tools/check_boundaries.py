#!/usr/bin/env python3
"""Rejects includes that cross the boundaries between Oly modules, plugins and environments, Emscripten outside the web environment, generated-code imports, the core's independence from environments and plugins, game names in plugins and engine test games outside engine/.

Usage: python3 -I tools/check_boundaries.py --root <repo> --manifest <json>
Exits 0 with no violations; exits 1 printing "<file>:<line>: <reason>" per violation.
"""
import argparse
import json
import os
import re
import sys

IGNORED_DIRECTORIES = {"build", "node_modules", ".git", ".superpowers", "dist", "dist-tests", ".claude"}
CPP_EXTENSIONS = (".hpp", ".cpp", ".h", ".c", ".inl")
TS_EXTENSION = ".ts"

INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s+"oly/([a-z0-9_]+)/([^"]*)"')
# The engine and its schema tooling know no environment; environments depend on them.
CORE_PREFIXES = ("engine/runtime/", "engine/schema/")
ENVIRONMENTS_PREFIX = "engine/environments/"
MODULE_INCLUDE_DIRECTORY = re.compile(r"^(.+)/include/oly/([a-z0-9_]+)$")

# Plugins depend on the engine, never the reverse; the engine's own test plugins live under engine/testing/fixture/.
ENGINE_PREFIX = "engine/"
PLUGINS_PREFIX = "plugins/"
GAMES_DIRECTORY = "games"
ENGINE_TEST_GAME_PATTERN = re.compile(r"\boly_add_engine_test_game\s*\(")

EMSCRIPTEN_PATTERN = re.compile(r"__EMSCRIPTEN(?:_[a-z]+)?__|\bEM_[A-Z_]+|\bEMSCRIPTEN_[A-Z_]+|<emscripten[./]")
EMSCRIPTEN_ALLOWED_PREFIX = "engine/environments/web/"
# OLY_API is the core's only Emscripten-aware line (EMSCRIPTEN_KEEPALIVE on WASM).
OLY_API_HEADER = "engine/runtime/capi/include/oly/capi/c_api.h"

TS_IMPORT_PATTERN = re.compile(r"""(?:\bfrom\s+|\bimport\s*\(?\s*)["'](\.{1,2}/[^"']*|@oly/[^"']*)["']""")


def load_owners(manifest):
    entries = manifest["modules"] + manifest.get("plugins", []) + manifest["games"]
    owners = [(entry["directory"].rstrip("/") + "/", entry["name"], set(entry["depends"])) for entry in entries]
    # Longest prefix first: owner_of keeps the first match, which is therefore the most specific directory.
    return sorted(owners, key=lambda owner: len(owner[0]), reverse=True)


def owner_of(relative_path, owners):
    for directory, name, depends in owners:
        if relative_path.startswith(directory):
            return name, depends
    return None, set()


def repository_layout(root):
    module_directories = {}
    package_directories = {}

    for current, directories, files in os.walk(root):
        directories[:] = [directory for directory in directories if directory not in IGNORED_DIRECTORIES]
        relative_directory = os.path.relpath(current, root).replace(os.sep, "/")
        module_match = MODULE_INCLUDE_DIRECTORY.match(relative_directory)

        if module_match is not None:
            module_directories[module_match.group(2)] = module_match.group(1) + "/"
        if "package.json" in files:
            try:
                with open(os.path.join(current, "package.json"), encoding="utf-8") as handle:
                    name = json.load(handle).get("name")
            except ValueError:
                name = None
            if name:
                package_directories[name] = relative_directory + "/"

    return module_directories, package_directories


def environment_of(relative_path):
    if not relative_path.startswith(ENVIRONMENTS_PREFIX):
        return None
    return relative_path[len(ENVIRONMENTS_PREFIX):].split("/")[0]


def environment_violation(relative_path, target_directory, target_name):
    if target_directory is None:
        return None
    target_environment = environment_of(target_directory)

    if target_environment is None:
        return None
    for prefix in CORE_PREFIXES:
        if relative_path.startswith(prefix):
            return f"{prefix.rstrip('/')} cannot depend on environment {target_environment} ({target_name})"
    source_environment = environment_of(relative_path)

    if source_environment is not None and source_environment != target_environment:
        return f"environment {source_environment} cannot depend on environment {target_environment} ({target_name})"
    return None


def plugin_violation(relative_path, target_directory, target_name):
    if target_directory is None or not target_directory.startswith(PLUGINS_PREFIX):
        return None
    if relative_path.startswith(ENGINE_PREFIX):
        return f"engine cannot depend on plugin {target_name}"
    return None


def game_names_pattern(root):
    games_directory = os.path.join(root, GAMES_DIRECTORY)

    if not os.path.isdir(games_directory):
        return None
    names = sorted(name for name in os.listdir(games_directory) if os.path.isdir(os.path.join(games_directory, name)))

    if not names:
        return None
    # Whole words in any identifier style: "arena_game" names the game, "arenas" does not.
    return re.compile(r"(?<![A-Za-z0-9])(" + "|".join(re.escape(name) for name in names) + r")(?![A-Za-z0-9])")


def check_game_names(path, relative_path, pattern):
    violations = [f"{relative_path}: game name {name} in plugins/" for name in sorted(set(pattern.findall(relative_path)))]
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line_number, line in enumerate(handle, start=1):
            for name in sorted(set(pattern.findall(line))):
                violations.append(f"{relative_path}:{line_number}: game name {name} in plugins/")

    return violations


def check_engine_test_game(path, relative_path):
    violations = []
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line_number, line in enumerate(handle, start=1):
            if ENGINE_TEST_GAME_PATTERN.search(line):
                violations.append(f"{relative_path}:{line_number}: oly_add_engine_test_game is only for engine/")

    return violations


def emscripten_violation(relative_path, line):
    if relative_path.startswith(EMSCRIPTEN_ALLOWED_PREFIX) or relative_path == OLY_API_HEADER:
        return None
    matches = [match.group(0) for match in EMSCRIPTEN_PATTERN.finditer(line)]

    if not matches:
        return None
    # A test or a benchmark may skip a case on WASM, but never calls Emscripten.
    if ("/tests/" in relative_path or "/bench/" in relative_path) and all(match == "__EMSCRIPTEN__" for match in matches):
        return None
    return f"Emscripten ({matches[0]}) is allowed only in engine/environments/web/ and OLY_API"


def repository_files(root):
    for current, directories, files in os.walk(root):
        directories[:] = [directory for directory in directories if directory not in IGNORED_DIRECTORIES]

        for file_name in files:
            yield os.path.join(current, file_name)


def check_cpp_file(path, relative_path, owners, module_directories):
    violations = []
    owner, depends = owner_of(relative_path, owners)
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line_number, line in enumerate(handle, start=1):
            emscripten_message = emscripten_violation(relative_path, line)

            if emscripten_message is not None:
                violations.append(f"{relative_path}:{line_number}: {emscripten_message}")
            match = INCLUDE_PATTERN.match(line)

            if match is None:
                continue
            included_module, included_path = match.group(1), match.group(2)

            if owner is not None and included_module != owner and included_module not in depends:
                violations.append(f"{relative_path}:{line_number}: {owner} includes oly/{included_module} without declaring {included_module} in DEPENDS")
            included_directory = module_directories.get(included_module)
            inside_included_module = owner == included_module or (included_directory is not None and relative_path.startswith(included_directory))

            if included_path.startswith("detail/") and not inside_included_module:
                violations.append(f"{relative_path}:{line_number}: oly/{included_module}/detail is internal to module {included_module}")
            environment_message = environment_violation(relative_path, module_directories.get(included_module), f"oly/{included_module}")

            if environment_message is not None:
                violations.append(f"{relative_path}:{line_number}: {environment_message}")
            plugin_message = plugin_violation(relative_path, included_directory, included_module)

            if plugin_message is not None:
                violations.append(f"{relative_path}:{line_number}: {plugin_message}")

    return violations


def package_root(path, root):
    current = os.path.dirname(path)

    while os.path.abspath(current) != os.path.abspath(root):
        if os.path.exists(os.path.join(current, "package.json")):
            return current
        current = os.path.dirname(current)
    return None


def check_ts_file(path, relative_path, root, package_directories):
    violations = []
    inside_engine = relative_path.startswith("engine/")
    inside_core = relative_path.startswith("engine/environments/web/clients/typescript/core/")
    package_directory = package_root(path, root)
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line_number, line in enumerate(handle, start=1):
            for match in TS_IMPORT_PATTERN.finditer(line):
                specifier = match.group(1)

                if specifier.startswith("@oly/core/internal") and not inside_engine:
                    violations.append(f"{relative_path}:{line_number}: @oly/core/internal can only be used inside engine/")
                if specifier.startswith("@oly/core/generated") and not relative_path.endswith(".generated.ts") and not inside_core:
                    violations.append(f"{relative_path}:{line_number}: @oly/core/generated can only be imported by generated code (*.generated.ts)")
                if specifier.startswith("@oly/web-host") and inside_core:
                    violations.append(f"{relative_path}:{line_number}: @oly/core cannot depend on @oly/web-host")
                if specifier.startswith("@oly/"):
                    package_name = "/".join(specifier.split("/")[:2])
                    environment_message = environment_violation(relative_path, package_directories.get(package_name), package_name)

                    if environment_message is not None:
                        violations.append(f"{relative_path}:{line_number}: {environment_message}")
                if specifier.startswith(".") and package_directory is not None:
                    resolved = os.path.abspath(os.path.join(os.path.dirname(path), specifier))

                    if os.path.commonpath([resolved, os.path.abspath(package_directory)]) != os.path.abspath(package_directory):
                        violations.append(f"{relative_path}:{line_number}: relative import crosses packages; use the package name")

    return violations


def check(root, manifest):
    owners = load_owners(manifest)
    module_directories, package_directories = repository_layout(root)
    game_names = game_names_pattern(root)
    violations = []

    for path in repository_files(root):
        relative_path = os.path.relpath(path, root).replace(os.sep, "/")

        if path.endswith(TS_EXTENSION):
            violations.extend(check_ts_file(path, relative_path, root, package_directories))
        elif path.endswith(CPP_EXTENSIONS):
            violations.extend(check_cpp_file(path, relative_path, owners, module_directories))
        elif os.path.basename(path) == "CMakeLists.txt" and not relative_path.startswith(ENGINE_PREFIX):
            violations.extend(check_engine_test_game(path, relative_path))

        if game_names is not None and relative_path.startswith(PLUGINS_PREFIX):
            violations.extend(check_game_names(path, relative_path, game_names))

    return sorted(violations)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True)
    parser.add_argument("--manifest", required=True)
    arguments = parser.parse_args(argv)

    with open(arguments.manifest, encoding="utf-8") as handle:
        manifest = json.load(handle)

    violations = check(arguments.root, manifest)

    for violation in violations:
        print(violation)
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
