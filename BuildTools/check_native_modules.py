"""Check the layout of standalone engine-owned native modules.

A header module under ``Source/`` must be one half of a ``Name.h`` +
``Name.cpp`` pair with both members registered in the owning CMake source
list, or carry an exact reviewed header-only entry in
``BuildTools/NativeModuleExceptions.json``.  A ``.cpp`` without a matching
header is a valid cpp-only unit when it is registered in an owning CMake source
list; the absence of a header never excuses registration.  Test units
(``Source/Tests/Test_*.cpp``) are not standalone modules, and reviewed
implementation exceptions remain exact.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


_OWNER_FILES = (
    "BuildTools/cmake/stages/EngineSources.cmake",
    "BuildTools/cmake/stages/CoreLibs.cmake",
    "BuildTools/cmake/stages/Applications.cmake",
)
_EXCEPTIONS_FILE = "BuildTools/NativeModuleExceptions.json"
_ENGINE_ROOT_TOKEN = "${FO_ENGINE_ROOT}"
_SOURCE_PREFIX = _ENGINE_ROOT_TOKEN + "/Source/"
_TEST_UNIT_PREFIX = "Source/Tests/Test_"
_CANONICAL_PATH_RE = re.compile(r"Source/[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*")
_QUOTED_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
_SET_VALUE_RE = re.compile(
    r"SetValue\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+\"((?:[^\"\\]|\\.)*)\"", re.DOTALL)
_VARIABLE_RE = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")
_SOURCE_LIST_RE = re.compile(
    r"\b(?:AppendList|SetValue)\(\s*(?:[A-Z0-9_]*_SOURCE|SOURCES|FO_SOURCE_META_FILES)\s+(.*?)\)",
    re.DOTALL,
)
_APPLICATION_SOURCE_RE = re.compile(
    r'\bAddSharedApplication\(\s*\S+\s+"((?:[^"\\]|\\.)*)"')
_TARGET_SOURCES_RE = re.compile(
    r"\btarget_sources\(\s*\S+\s+(?:PRIVATE|PUBLIC|INTERFACE)\s+(.*?)\)",
    re.DOTALL,
)


def check_modules(root: Path) -> list[str]:
    """Validate the native module layout under *root* and return sorted diagnostics."""
    root = Path(root)

    if not (root / "Source").is_dir():
        return ["Source: native source directory does not exist"]

    files = _scan_sources(root)

    if not files:
        return ["Source: native source inventory is empty"]

    registered = _collect_registrations(root)
    header_only, implementation_only, issues = _load_exceptions(root, files)

    for path in sorted(files):
        if _is_test_unit(path):
            continue

        if path in header_only or path in implementation_only:
            continue

        if _counterpart(path) in files:
            if path not in registered:
                issues.append(
                    f"{path}: pair member is not registered in the owning CMake source lists "
                    f"({', '.join(_OWNER_FILES)})")
            continue

        if path.endswith(".cpp"):
            if path not in registered:
                issues.append(
                    f"{path}: cpp-only unit is not registered in the owning CMake "
                    f"source lists ({', '.join(_OWNER_FILES)})")
            continue

        issues.append(
            f"{path}: standalone native module is unpaired; add the matching "
            f"pair member or record a reviewed exception in {_EXCEPTIONS_FILE}")

    return sorted(set(issues))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1],
                        help="engine repository root to check")
    args = parser.parse_args(argv)

    issues = check_modules(args.root)
    for issue in issues:
        print(issue)

    return 1 if issues else 0


def _scan_sources(root: Path) -> set[str]:
    source_dir = root / "Source"
    if not source_dir.is_dir():
        return set()

    return {
        path.relative_to(root).as_posix()
        for path in source_dir.rglob("*")
        if path.is_file() and path.suffix in (".h", ".cpp")
    }


def _collect_registrations(root: Path) -> set[str]:
    definitions: dict[str, str] = {}
    literals: list[str] = []

    for owner in _OWNER_FILES:
        owner_path = root / owner
        if not owner_path.is_file():
            continue

        try:
            text = _strip_comments(owner_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError):
            continue

        for match in _SET_VALUE_RE.finditer(text):
            definitions.setdefault(match.group(1), match.group(2))
        for source_list in _SOURCE_LIST_RE.finditer(text):
            literals.extend(_QUOTED_RE.findall(source_list.group(1)))
        literals.extend(_APPLICATION_SOURCE_RE.findall(text))
        for source_list in _TARGET_SOURCES_RE.finditer(text):
            literals.extend(_QUOTED_RE.findall(source_list.group(1)))

    registered: set[str] = set()

    for literal in literals:
        resolved = _resolve_variables(literal, definitions)
        if resolved.startswith(_SOURCE_PREFIX):
            registered.add("Source/" + resolved[len(_SOURCE_PREFIX):])

    return registered


def _load_exceptions(root: Path, files: set[str]) -> tuple[dict[str, str], dict[str, str], list[str]]:
    header_only: dict[str, str] = {}
    implementation_only: dict[str, str] = {}
    issues: list[str] = []

    config_path = root / _EXCEPTIONS_FILE
    if not config_path.is_file():
        issues.append(f"{_EXCEPTIONS_FILE}: native module exception configuration is missing")
        return header_only, implementation_only, issues

    try:
        raw = json.loads(config_path.read_text(encoding="utf-8"), object_pairs_hook=_unique_keys)
    except (OSError, ValueError) as ex:
        issues.append(f"{_EXCEPTIONS_FILE}: exception configuration is unreadable: {ex}")
        return header_only, implementation_only, issues

    if not isinstance(raw, dict):
        issues.append(f"{_EXCEPTIONS_FILE}: exception configuration root must be an object")
        return header_only, implementation_only, issues

    for key in sorted(set(raw) - {"Version", "HeaderOnly", "ImplementationOnly"}):
        issues.append(f"{_EXCEPTIONS_FILE}: unknown configuration key {key}")

    if type(raw.get("Version")) is not int or raw["Version"] != 1:
        issues.append(
            f"{_EXCEPTIONS_FILE}: unsupported configuration version "
            f"{raw.get('Version')!r}; expected 1")

    for section_name, target in (("HeaderOnly", header_only),
                                 ("ImplementationOnly", implementation_only)):
        section = raw.get(section_name)
        if not isinstance(section, dict):
            issues.append(f"{_EXCEPTIONS_FILE}: section {section_name} must be an object")
            continue

        for entry_path, reason in section.items():
            if not isinstance(reason, str) or not reason.strip():
                issues.append(
                    f"{_EXCEPTIONS_FILE}: {section_name} entry {entry_path!r} "
                    f"needs a non-empty reason")

            if not _is_canonical(entry_path, section_name):
                issues.append(
                    f"{_EXCEPTIONS_FILE}: {section_name} entry {entry_path!r} "
                    f"is not a canonical exact source path")
                continue

            target[entry_path] = reason if isinstance(reason, str) else ""

            if not (root / entry_path).is_file():
                issues.append(
                    f"{_EXCEPTIONS_FILE}: {section_name} entry {entry_path} is stale; "
                    f"the file does not exist")
                continue

            if _counterpart(entry_path) in files:
                issues.append(
                    f"{_EXCEPTIONS_FILE}: {section_name} entry {entry_path} is invalid; "
                    f"the file forms a complete module pair")

    return header_only, implementation_only, issues


def _is_canonical(path: str, section_name: str) -> bool:
    if not isinstance(path, str) or not _CANONICAL_PATH_RE.fullmatch(path):
        return False

    if any(part in (".", "..") for part in path.split("/")):
        return False

    if section_name == "HeaderOnly":
        return path.endswith(".h")

    return path.endswith(".cpp")


def _unique_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}

    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate configuration key {key}")

        result[key] = value

    return result


def _is_test_unit(path: str) -> bool:
    return (path.startswith(_TEST_UNIT_PREFIX) and path.endswith(".cpp")
            and "/" not in path[len(_TEST_UNIT_PREFIX):])


def _counterpart(path: str) -> str:
    if path.endswith(".h"):
        return path[:-2] + ".cpp"

    return path[:-4] + ".h"


def _resolve_variables(value: str, definitions: dict[str, str]) -> str:
    for _ in range(16):
        expanded = _VARIABLE_RE.sub(
            lambda match: definitions.get(match.group(1), match.group(0)), value)

        if expanded == value:
            break

        value = expanded

    return value


def _strip_comments(text: str) -> str:
    cleaned: list[str] = []
    in_quotes = False
    index = 0

    while index < len(text):
        char = text[index]

        if in_quotes and char == "\\" and index + 1 < len(text):
            cleaned.append(text[index:index + 2])
            index += 2
            continue

        if char == '"':
            in_quotes = not in_quotes

        if char == "#" and not in_quotes:
            bracket = re.match(r"#\[(=*)\[", text[index:])

            if bracket:
                closing = "]" + bracket.group(1) + "]"
                end = text.find(closing, index + bracket.end())
                index = len(text) if end < 0 else end + len(closing)
            else:
                end = text.find("\n", index)
                index = len(text) if end < 0 else end

            cleaned.append("\n")
            continue

        cleaned.append(char)
        index += 1

    return "".join(cleaned)


if __name__ == "__main__":
    raise SystemExit(main())
