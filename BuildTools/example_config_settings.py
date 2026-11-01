"""Read runtime-owned setting names for the generated example configurations."""

from __future__ import annotations

from pathlib import Path
import re

from check_native_source_layout import _mask_source


_RUNTIME_SETTING = re.compile(
    r"^\s*_appliedSettings\.emplace\(SettingNames::([A-Za-z][A-Za-z0-9]*)_([A-Za-z][A-Za-z0-9_]*)\);\s*$",
    re.MULTILINE,
)


def parse_runtime_managed_settings(path: Path) -> set[str]:
    source = _mask_source(path.read_text(encoding="utf-8-sig"))
    return {f"{group}.{name}" for group, name in _RUNTIME_SETTING.findall(source)}


_ENUM_DECLARATION = re.compile(r"\benum\s+class\s+([A-Za-z_]\w*)\s*(?::\s*[\w:\s]+?)?\{([^{}]*)\}")
_ENUM_ENTRY = re.compile(r"^([A-Za-z_]\w*)\s*(?:=\s*(.+))?$", re.DOTALL)
_ENUM_CONSTANT = re.compile(r"^([A-Za-z_]\w*)::([A-Za-z_]\w*)$")


def parse_enum_constants(source_root: Path) -> dict[str, int]:
    constants: dict[str, int] = {}
    ambiguous: set[str] = set()

    for header in sorted(source_root.rglob("*.h")):
        source = _mask_source(header.read_text(encoding="utf-8-sig"))

        for enum_name, body in _ENUM_DECLARATION.findall(source):
            next_value: int | None = 0

            for raw_entry in body.split(","):
                entry = raw_entry.strip()

                if not entry:
                    continue

                match = _ENUM_ENTRY.match(entry)

                if match is None:
                    next_value = None
                    continue

                if match.group(2) is not None:
                    try:
                        next_value = int(match.group(2).strip(), 0)
                    except ValueError:
                        next_value = None

                if next_value is None:
                    continue

                key = f"{enum_name}::{match.group(1)}"

                if constants.get(key, next_value) != next_value:
                    ambiguous.add(key)

                constants[key] = next_value
                next_value += 1

    for key in ambiguous:
        del constants[key]

    return constants


def resolve_enum_constant(value: str, constants: dict[str, int]) -> str:
    if _ENUM_CONSTANT.match(value) is None:
        return value

    if value not in constants:
        raise ValueError(f"unresolved enum constant in setting default: {value}")

    return str(constants[value])
