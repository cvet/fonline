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
