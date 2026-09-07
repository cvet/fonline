from __future__ import annotations

import os
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import managed_runtime_identity as identity
import package


def make_runtime(root: Path) -> Path:
    runtime = root / "ManagedRuntime"
    (runtime / "lib").mkdir(parents=True)
    (runtime / "lib" / "CoreLib.dll").write_bytes(b"runtime assembly")
    return runtime


def test_identity_covers_content_and_paths_but_not_mtime(tmp_path: Path) -> None:
    runtime = make_runtime(tmp_path)
    original = identity.runtime_identity(runtime)
    assembly = runtime / "lib" / "CoreLib.dll"
    os.utime(assembly, (1000, 1000))
    assert identity.runtime_identity(runtime) == original
    assembly.write_bytes(b"different assembly")
    assert identity.runtime_identity(runtime) != original
    assembly.write_bytes(b"runtime assembly")
    assembly.rename(assembly.with_name("Other.dll"))
    assert identity.runtime_identity(runtime) != original


@pytest.mark.parametrize("missing_binary", [False, True])
@pytest.mark.parametrize("changed", [False, True])
def test_payload_is_available_only_to_matching_companions(tmp_path: Path, changed: bool, missing_binary: bool) -> None:
    binary_dir = tmp_path / "Binaries" / "Client-Linux-x64"
    runtime = make_runtime(binary_dir)
    runtime_id = identity.runtime_identity(runtime)
    (binary_dir / "LF_ClientLib.managed-runtime-id").write_text(runtime_id)
    (binary_dir / "LF_ClientLib.build-hash").write_text("build")
    (binary_dir / "LF_ClientLib.so").write_bytes(b"native runtime")
    if changed:
        (runtime / "lib" / "CoreLib.dll").write_bytes(b"new runtime")
    packager = make_packager(tmp_path)
    if missing_binary:
        (binary_dir / "LF_ClientLib.so").unlink()
        (binary_dir / "LF_ClientLib.managed-runtime-id").unlink()
        packager.package_all_client_runtime_update_payloads()
        assert not Path(packager.target_output_path).exists()
    elif changed:
        with pytest.raises(ValueError, match="differ from the compiled client"):
            packager.package_all_client_runtime_update_payloads()
        assert not Path(packager.target_output_path).exists()
    else:
        packager.package_all_client_runtime_update_payloads()
        root = Path(packager.target_output_path) / "PlatformBinaries"
        assert (root / ("Linux-x64-Managed-" + runtime_id) / "Game.so").read_bytes() == b"native runtime"
        assert not (root / "Linux-x64").exists()


def test_generated_identity_does_not_touch_unchanged_header(tmp_path: Path) -> None:
    header = tmp_path / "identity.h"
    identity.write_if_changed(header, "identity\n")
    modified = header.stat().st_mtime_ns
    identity.write_if_changed(header, "identity\n")
    assert header.stat().st_mtime_ns == modified


def make_packager(tmp_path: Path):
    packager = package.Packager.__new__(package.Packager)
    packager.args = SimpleNamespace(input=[str(tmp_path)], devname="LF", nicename="Game", buildhash="build")
    packager.target_output_path = str(tmp_path / "output")
    packager.platform_binaries_dir = "PlatformBinaries"
    packager.embedded_data = b""
    packager.config_data = ""
    packager.make_embedded_data_for_target = lambda target: b""
    packager.read_config_data = lambda target: (None, "")
    packager.patch_packaged_binary = lambda *args: None
    return packager


@pytest.mark.parametrize("default_present", [False, True])
@pytest.mark.parametrize("headless_revision", [None, "old-build", "build"])
def test_each_native_variant_requires_its_own_build_revision(
    tmp_path: Path, default_present: bool, headless_revision: str | None,
) -> None:
    binary_dir = tmp_path / "Binaries" / "Client-Linux-x64"
    runtime_id = identity.runtime_identity(make_runtime(binary_dir))
    expected = set()
    if default_present:
        (binary_dir / "LF_ClientLib.so").write_bytes(b"gui")
        (binary_dir / "LF_ClientLib.build-hash").write_text("build")
        (binary_dir / "LF_ClientLib.managed-runtime-id").write_text(runtime_id)
        expected.add("Game.so")
    (binary_dir / "LF_ClientLibHeadless.so").write_bytes(b"headless")
    if headless_revision is not None:
        (binary_dir / "LF_ClientLibHeadless.build-hash").write_text(headless_revision)
    if headless_revision == "build":
        (binary_dir / "LF_ClientLibHeadless.managed-runtime-id").write_text(runtime_id)
        expected.add("Game_Headless.so")
    packager = make_packager(tmp_path)
    packager.package_all_client_runtime_update_payloads()
    payloads = Path(packager.target_output_path) / "PlatformBinaries" / ("Linux-x64-Managed-" + runtime_id)
    assert {path.name for path in payloads.glob("*.so")} == expected


@pytest.mark.parametrize(("entry_suffix", "output_suffix"), [
    ("-Debug_Profiling_Total", "_Debug_Profiling_Total"),
    ("-Profiling_OnDemand_Custom", "_Profiling_OnDemand_Custom"),
    ("-DebugProfile", "_DebugProfile"),
    ("-Profiling_Total", "_Profiling"),
    ("-Profiling_Total-Debug-Steam", "_Profiling_Steam"),
    ("-Debug-Steam", "_Steam"),
])
def test_native_payload_postfix_preserves_reserved_word_prefixes(
    tmp_path: Path, entry_suffix: str, output_suffix: str,
) -> None:
    binary_dir = tmp_path / "Binaries" / ("Client-Linux-x64" + entry_suffix)
    runtime_id = identity.runtime_identity(make_runtime(binary_dir))
    (binary_dir / "LF_ClientLib.so").write_bytes(b"native runtime")
    (binary_dir / "LF_ClientLib.build-hash").write_text("build")
    (binary_dir / "LF_ClientLib.managed-runtime-id").write_text(runtime_id)
    packager = make_packager(tmp_path)
    packager.package_all_client_runtime_update_payloads()
    payloads = Path(packager.target_output_path) / "PlatformBinaries" / ("Linux-x64-Managed-" + runtime_id)
    assert {path.name for path in payloads.glob("*.so")} == {"Game" + output_suffix + ".so"}
