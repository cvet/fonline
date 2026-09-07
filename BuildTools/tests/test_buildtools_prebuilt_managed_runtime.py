from __future__ import annotations

from pathlib import Path
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


def make_published_tree(root: Path, triplet: str) -> Path:
    tree = root / triplet
    (tree / "lib").mkdir(parents=True)
    (tree / "include" / "mono-2.0").mkdir(parents=True)
    (tree / "lib" / "libmonosgen-2.0.a").write_text("archive", encoding="utf-8")
    return tree


def setup_mono_env(tmp_path: Path, prebuilt: Path | str) -> dict[str, str]:
    return {
        "FO_WORKSPACE": str(tmp_path / "workspace"),
        "FO_DOTNET_RUNTIME": "v10.0.11",
        "FO_MANAGED_RUNTIME_PREBUILT": str(prebuilt),
    }


def test_prebuilt_runtime_is_adopted_instead_of_being_built(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    prebuilt_root = tmp_path / "prebuilt"
    make_published_tree(prebuilt_root, "windows.x64.Release")

    def fail_clone(*args: object, **kwargs: object) -> None:
        raise AssertionError("a prebuilt runtime must not clone dotnet/runtime")

    monkeypatch.setattr(_buildtools, "clone_git_repo", fail_clone)
    _buildtools.setup_mono("windows", "x64", "Release", setup_mono_env(tmp_path, prebuilt_root))

    workspace = tmp_path / "workspace"
    assert (workspace / "output" / "mono" / "windows.x64.Release" / "lib" / "libmonosgen-2.0.a").is_file()
    assert (workspace / "READY_v10.0.11_windows.x64.Release_mono_runtime_corelib_libs_native_nogl").is_file()


def test_prebuilt_runtime_accepts_a_single_triplet_tree(tmp_path: Path) -> None:
    tree = make_published_tree(tmp_path / "prebuilt", "windows.x64.Release")

    _buildtools.setup_mono("windows", "x64", "Release", setup_mono_env(tmp_path, tree))

    assert (tmp_path / "workspace" / "output" / "mono" / "windows.x64.Release" / "include" / "mono-2.0").is_dir()


def test_prebuilt_runtime_must_be_a_published_tree(tmp_path: Path) -> None:
    empty = tmp_path / "prebuilt"
    empty.mkdir()

    with pytest.raises(SystemExit, match="not a published runtime tree"):
        _buildtools.setup_mono("windows", "x64", "Release", setup_mono_env(tmp_path, empty))


def test_windows_target_without_a_prebuilt_runtime_is_refused_off_windows(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(_buildtools.os, "name", "posix")
    env = setup_mono_env(tmp_path, "")
    env["FO_MANAGED_RUNTIME_PREBUILT"] = ""

    with pytest.raises(SystemExit, match="no Windows cross-target"):
        _buildtools.setup_mono("windows", "x64", "Release", env)


def test_ready_marker_suffixes_match_the_cmake_stage() -> None:
    # The suffix is the cache key for an already-prepared host, so a rename that reaches only one of
    # the two places leaves runners serving a runtime built the old way
    stage = (BUILDTOOLS_DIR / "cmake" / "stages" / "ThirdParty.cmake").read_text(encoding="utf-8")

    for suffix in (_buildtools.MONO_BROWSER_SUBSET_MARKER_SUFFIX, _buildtools.MONO_SUBSET_MARKER_SUFFIX):
        assert f"READY_${{FO_MONO_RUNTIME_VERSION}}_${{FO_MONO_TRIPLET}}{suffix})" in stage, suffix


def test_mono_whole_program_optimization_is_dropped(tmp_path: Path) -> None:
    # A published archive carrying MSVC whole-program IL is linkable by exactly one linker build:
    # link.exe rejects IL from another toolset and lld-link cannot read it at all
    cmake_lists = tmp_path / "src" / "mono" / "CMakeLists.txt"
    cmake_lists.parent.mkdir(parents=True)
    cmake_lists.write_text(
        "  if(CMAKE_BUILD_TYPE STREQUAL \"Release\")\n"
        "    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/GL>) # whole program optimization\n"
        "    add_link_options(/LTCG)    # link-time code generation\n"
        "  endif()\n",
        encoding="utf-8",
    )

    _buildtools.patch_runtime_sources(tmp_path)
    patched = cmake_lists.read_text(encoding="utf-8")

    assert "/GL>" not in patched
    assert "/LTCG" not in patched
    assert _buildtools.PATCH_MARKER in patched

    # Re-running over an already patched tree must not abort the build on the missing anchor
    _buildtools.patch_runtime_sources(tmp_path)
    assert cmake_lists.read_text(encoding="utf-8") == patched


def test_mono_patch_fails_loudly_when_the_anchor_moves(tmp_path: Path) -> None:
    cmake_lists = tmp_path / "src" / "mono" / "CMakeLists.txt"
    cmake_lists.parent.mkdir(parents=True)
    cmake_lists.write_text("  add_link_options(/LTCG)    # link-time code generation\n", encoding="utf-8")

    with pytest.raises(SystemExit):
        _buildtools.patch_runtime_sources(tmp_path)


def test_framework_versions_sort_numerically_with_release_after_preview() -> None:
    versions = ["9.0.0", "10.0.0-preview.10", "10.0.0-preview.2", "10.0.0", "10.0.11"]
    assert sorted(versions, key=_buildtools.runtime_framework_version_key) == [
        "9.0.0", "10.0.0-preview.2", "10.0.0-preview.10", "10.0.0", "10.0.11"
    ]


def test_runtime_revision_uses_a_distinct_ready_marker(tmp_path: Path) -> None:
    tree = make_published_tree(tmp_path / "prebuilt", "windows.x64.Release")
    env = setup_mono_env(tmp_path, tree)
    _buildtools.setup_mono("windows", "x64", "Release", env)
    env["FO_DOTNET_RUNTIME"] = "v10.0.12"
    _buildtools.setup_mono("windows", "x64", "Release", env)
    markers = sorted(path.name for path in (tmp_path / "workspace").glob("READY_*"))
    assert markers == [
        "READY_v10.0.11_windows.x64.Release_mono_runtime_corelib_libs_native_nogl",
        "READY_v10.0.12_windows.x64.Release_mono_runtime_corelib_libs_native_nogl",
    ]


@pytest.mark.parametrize("missing_corelib", [False, True])
def test_publish_replaces_old_files_only_after_input_validation(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, missing_corelib: bool) -> None:
    env = setup_mono_env(tmp_path, "")
    workspace = Path(env["FO_WORKSPACE"])
    runtime = workspace / "runtime"
    triplet = "linux.x64.Release"
    output = workspace / "output" / "mono" / triplet
    output.mkdir(parents=True)
    (output / "removed-library.so").write_text("stale", encoding="utf-8")
    inputs = runtime / "artifacts" / "obj" / "mono" / triplet / "out"
    inputs.mkdir(parents=True)
    (inputs / "current-library.so").write_text("current", encoding="utf-8")
    frameworks = runtime / ".dotnet" / "shared" / "Microsoft.NETCore.App"
    for version in ("9.0.0", "10.0.0-preview.2", "10.0.0"):
        folder = frameworks / version
        folder.mkdir(parents=True)
        (folder / "System.Runtime.dll").write_text(version, encoding="utf-8")
    corelib = runtime / "artifacts" / "bin" / "mono" / triplet / "IL" / "System.Private.CoreLib.dll"
    if not missing_corelib:
        corelib.parent.mkdir(parents=True)
        corelib.write_text("mono corelib", encoding="utf-8")

    def run_publish_only(marker: Path, label: str, action: object) -> None:
        if label.startswith("Publish runtime"):
            action()

    monkeypatch.setattr(_buildtools, "run_marker_step", run_publish_only)
    monkeypatch.setattr(_buildtools, "copy_interop_shim_libraries", lambda *args: None)
    if missing_corelib:
        with pytest.raises(SystemExit, match="System.Private.CoreLib not found"):
            _buildtools.setup_mono("linux", "x64", "Release", env)
        assert (output / "removed-library.so").is_file()
    else:
        _buildtools.setup_mono("linux", "x64", "Release", env)
        assert not (output / "removed-library.so").exists()
        assert (output / "current-library.so").is_file()
        assert (output / "lib" / "netcoreapp" / "System.Runtime.dll").read_text() == "10.0.0"
        assert (output / "lib" / "netcoreapp" / "System.Private.CoreLib.dll").read_text() == "mono corelib"
