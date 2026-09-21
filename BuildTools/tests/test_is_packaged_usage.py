from __future__ import annotations

import re
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ENGINE_ROOT / "Source"
IS_PACKAGED_PATTERN = re.compile(r"\bIsPackaged\s*\(\s*\)")

ALLOWED_OCCURRENCES = {
    "Source/Common/Common.cpp": 1,
    "Source/Common/FileSystem.cpp": 1,
    "Source/Common/Settings.cpp": 1,
    "Source/Frontend/ApplicationInit.cpp": 2,
    "Source/Tests/Test_ClientEngine.cpp": 1,
    "Source/Tests/Test_Common.cpp": 1,
    "Source/Tests/Test_ServerEngine.cpp": 1,
}


def test_is_packaged_is_limited_to_bootstrap_and_physical_layout() -> None:
    """Runtime policy must consume the loaded Common.Packaged snapshot."""

    occurrences: dict[str, int] = {}

    for source_path in SOURCE_ROOT.rglob("*.cpp"):
        count = len(IS_PACKAGED_PATTERN.findall(source_path.read_text(encoding="utf-8", errors="replace")))

        if count != 0:
            relative_path = source_path.relative_to(ENGINE_ROOT).as_posix()
            occurrences[relative_path] = count

    assert occurrences == ALLOWED_OCCURRENCES
