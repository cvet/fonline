from __future__ import annotations

import re
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ENGINE_ROOT / "Source"
IS_PACKAGED_PATTERN = re.compile(r"\bIsPackaged\s*\(\s*\)")


def test_packaged_marker_is_owned_by_application_initialization() -> None:
    """Runtime code consumes settings; only bootstrap reads the binary marker."""
    occurrences: set[str] = set()
    markers: set[str] = set()
    for source_path in SOURCE_ROOT.rglob("*"):
        if source_path.suffix not in {".h", ".cpp"}:
            continue
        text = source_path.read_text(encoding="utf-8")
        relative = source_path.relative_to(ENGINE_ROOT).as_posix()
        if IS_PACKAGED_PATTERN.search(text):
            occurrences.add(relative)
        if "PACKAGED_BUILD_NAME" in text:
            markers.add(relative)

    assert occurrences == {"Source/Frontend/ApplicationInit.cpp"}
    assert markers == {"Source/Frontend/ApplicationInit.cpp"}
    startup = (SOURCE_ROOT / "Frontend/ApplicationInit.cpp").read_text(encoding="utf-8")
    assert "static auto IsPackaged() -> bool" in startup
    common = (SOURCE_ROOT / "Common/Common.h").read_text(encoding="utf-8")
    assert "GetPackagedRuntimeName" not in common


def test_metadata_reader_owns_no_mount_or_startup_compatibility_policy() -> None:
    source = (SOURCE_ROOT / "Common/Settings.cpp").read_text(encoding="utf-8")
    reader = source[source.index("auto ReadResourceConfig("):source.index("\nvoid PreserveLocalConfig(")]
    assert re.search(r"ReadResourceConfig\(const FileSystem&\s+\w+,\s*bool\s+\w+\)", reader)
    for policy in ("client_overlays", "AddPackSource", "AddClientPackSource", "FO_COMPATIBILITY_VERSION"):
        assert policy not in reader
    startup = (SOURCE_ROOT / "Frontend/ApplicationInit.cpp").read_text(encoding="utf-8")
    assert "standalone_client && content" in startup
    assert 'starts_with("#" FO_COMPATIBILITY_VERSION' in startup
