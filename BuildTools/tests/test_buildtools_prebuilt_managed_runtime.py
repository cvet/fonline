from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
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

    for suffix in (
        _buildtools.MONO_BROWSER_SUBSET_MARKER_SUFFIX,
        _buildtools.MONO_ANDROID_SOURCE_MARKER_SUFFIX,
        _buildtools.MONO_APPLE_SOURCE_MARKER_SUFFIX,
        _buildtools.MONO_SUBSET_MARKER_SUFFIX,
    ):
        assert f"READY_${{FO_MONO_RUNTIME_VERSION}}_${{FO_MONO_TRIPLET}}{suffix})" in stage, suffix


@pytest.mark.skipif(shutil.which("cmake") is None, reason="CMake is required")
@pytest.mark.parametrize("target,flags", [
    ("linux", []), ("windows", []), ("browser", ["FO_WEB"]), ("android", ["FO_ANDROID"]),
    ("osx", ["FO_MAC"]), ("ios", ["FO_IOS"]), ("iossimulator", ["FO_IOS"]),
])
def test_cmake_selects_the_same_platform_cache_key(tmp_path: Path, target: str, flags: list[str]) -> None:
    stage = (BUILDTOOLS_DIR / "cmake/stages/ThirdParty.cmake").read_text(encoding="utf-8")
    start = stage.index("    if(FO_WEB)\n", stage.index("# Managed scripting runtime (Mono)"))
    end = stage.index("    endif()", start) + len("    endif()")
    script = tmp_path / "marker.cmake"
    script.write_text(
        "macro(SetValue name)\n  set(${name} ${ARGN})\nendmacro()\n"
        "set(FO_MONO_RUNTIME_VERSION v10.0.11)\n"
        f"set(FO_MONO_TRIPLET {target}.x64.Release)\n"
        + "".join(f"set({flag} ON)\n" for flag in flags)
        + stage[start:end]
        + f'\nfile(WRITE "{(tmp_path / "marker.txt").as_posix()}" "${{FO_MONO_READY_MARKER}}")\n',
        encoding="utf-8",
    )
    result = subprocess.run([shutil.which("cmake"), "-P", str(script)], capture_output=True, text=True)
    assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
    assert (tmp_path / "marker.txt").read_text() == f"READY_v10.0.11_{target}.x64.Release{_buildtools.resolve_mono_marker_suffix(target)}"


@pytest.mark.parametrize("target,legacy_apple_patch", [
    ("android", False), ("osx", False), ("ios", False), ("iossimulator", False),
    ("osx", True), ("ios", True), ("iossimulator", True),
])
def test_source_patch_cache_rebuilds_and_republishes_once(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, target: str, legacy_apple_patch: bool) -> None:
    env = setup_mono_env(tmp_path, "")
    workspace = Path(env["FO_WORKSPACE"])
    runtime = workspace / "runtime"
    runtime.mkdir(parents=True)
    triplet = f"{target}.x64.Release"
    previous_suffix = _buildtools.MONO_SUBSET_MARKER_SUFFIX + ("_apple_sources" if legacy_apple_patch else "")
    (workspace / "CLONED_v10.0.11").touch()
    for phase in ("BUILT", "READY"):
        (workspace / f"{phase}_v10.0.11_{triplet}{previous_suffix}").touch()
    published = make_published_tree(workspace / "output/mono", triplet)
    archive = published / "lib/libmonosgen-2.0.a"
    calls = []

    def reject_clone(*args, **kwargs):
        raise AssertionError("source patch invalidation must retain the cloned source")

    def build(_command, path, **kwargs):
        calls.append("build")
        assert path == runtime and kwargs["target_os"] == target
        out = runtime / "artifacts/obj/mono" / triplet / "out/lib"
        out.mkdir(parents=True)
        (out / "libmonosgen-2.0.a").write_text("patched runtime", encoding="utf-8")
        framework = runtime / ".dotnet/shared/Microsoft.NETCore.App/10.0.11"
        framework.mkdir(parents=True)
        (framework / "System.Runtime.dll").write_text("framework", encoding="utf-8")
        corelib = runtime / "artifacts/bin/mono" / triplet / "IL/System.Private.CoreLib.dll"
        corelib.parent.mkdir(parents=True)
        corelib.write_text("patched corelib", encoding="utf-8")

    monkeypatch.setattr(_buildtools, "clone_git_repo", reject_clone)
    for name in ("patch_runtime_zlib_warning_level", "patch_runtime_apple_sources", "patch_runtime_ios_sources", "patch_runtime_android_sources", "patch_runtime_android_x86_atomics"):
        monkeypatch.setattr(_buildtools, name, lambda path, name=name: calls.append(name))
    monkeypatch.setattr(_buildtools, "run_runtime_build", build)
    monkeypatch.setattr(_buildtools, "copy_interop_shim_libraries", lambda *args: calls.append("publish"))
    resolve = _buildtools.resolve_mono_marker_suffix
    with monkeypatch.context() as old_markers:
        old_markers.setattr(_buildtools, "resolve_mono_marker_suffix", lambda _target: previous_suffix)
        _buildtools.setup_mono(target, "x64", "Release", env)
    assert calls == [] and archive.read_text() == "archive"
    _buildtools.setup_mono(target, "x64", "Release", env)
    assert archive.read_text() == "patched runtime"
    expected_patch = ["patch_runtime_android_sources", "patch_runtime_android_x86_atomics"] if target == "android" else ["patch_runtime_apple_sources"]
    if target in ("ios", "iossimulator"):
        expected_patch.append("patch_runtime_ios_sources")
    assert calls == ["patch_runtime_zlib_warning_level", *expected_patch, "build", "publish"]
    for phase in ("BUILT", "READY"):
        assert (workspace / f"{phase}_v10.0.11_{triplet}{resolve(target)}").is_file()
    calls.clear()
    _buildtools.setup_mono(target, "x64", "Release", env)
    assert calls == []


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


@pytest.mark.skipif(shutil.which("cmake") is None or shutil.which("ninja") is None, reason="CMake and Ninja are required")
@pytest.mark.parametrize("msvc", [False, True])
def test_mono_zlib_keeps_its_own_warning_level_without_changing_siblings(tmp_path: Path, msvc: bool) -> None:
    external = tmp_path / "src" / "native" / "external"
    zlib = external / "zlib-ng"
    zlib.mkdir(parents=True)
    (zlib / "zlib.c").write_text("int zlib_probe;\n", encoding="utf-8")
    (zlib / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "if(MSVC)\n  add_compile_options(/W3 /w34242 /WX)\nendif()\n"
        "add_library(zlib STATIC zlib.c)\n",
        encoding="utf-8",
    )
    wrapper = external / "zlib-ng.cmake"
    wrapper.write_text(
        "include(FetchContent)\n"
        'FetchContent_Declare(fetchzlibng SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/zlib-ng")\n'
        "FetchContent_MakeAvailable(fetchzlibng)\n",
        encoding="utf-8",
    )
    (tmp_path / "mono.c").write_text("int mono_probe;\n", encoding="utf-8")
    (tmp_path / "sibling.c").write_text("int sibling_probe;\n", encoding="utf-8")
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\nproject(WarningLevels LANGUAGES C)\n"
        'set(CMAKE_C_FLAGS "")\n'
        f"set(MSVC {'ON' if msvc else 'OFF'})\n"
        "if(MSVC)\n  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/W4> /WX)\nendif()\n"
        "add_library(mono STATIC mono.c)\n"
        'include("src/native/external/zlib-ng.cmake")\n'
        "add_library(sibling STATIC sibling.c)\n",
        encoding="utf-8",
    )
    def configure(name: str) -> dict[str, list[str]]:
        build = tmp_path / name
        result = subprocess.run(
            ["cmake", "-S", str(tmp_path), "-B", str(build), "-G", "Ninja", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
            capture_output=True, text=True, check=True,
        )
        assert "warning" not in result.stderr.lower(), result.stderr
        return {Path(entry["file"]).name: entry["command"].split() for entry in json.loads((build / "compile_commands.json").read_text())}

    original_commands = configure("original-build")
    assert original_commands["zlib.c"].count("/W4") == int(msvc)
    assert original_commands["zlib.c"].count("/W3") == int(msvc)

    _buildtools.patch_runtime_zlib_warning_level(tmp_path)
    patched = wrapper.read_text(encoding="utf-8")
    _buildtools.patch_runtime_zlib_warning_level(tmp_path)
    assert wrapper.read_text(encoding="utf-8") == patched

    commands = configure("patched-build")
    assert "/W4" not in commands["zlib.c"]
    assert commands["zlib.c"].count("/W3") == int(msvc)
    assert commands["zlib.c"].count("/w34242") == int(msvc)
    for source in ("mono.c", "sibling.c"):
        assert commands[source].count("/W4") == int(msvc)
        assert "/W3" not in commands[source]
    for command in commands.values():
        assert command.count("/WX") == int(msvc)


@pytest.mark.parametrize("anchor_count", [0, 2])
def test_zlib_warning_patch_requires_a_unique_anchor(tmp_path: Path, anchor_count: int) -> None:
    wrapper = tmp_path / "src" / "native" / "external" / "zlib-ng.cmake"
    wrapper.parent.mkdir(parents=True)
    original = "FetchContent_MakeAvailable(fetchzlibng)\n" * anchor_count
    wrapper.write_text(original, encoding="utf-8")
    with pytest.raises(SystemExit, match="unique anchor not found"):
        _buildtools.patch_runtime_zlib_warning_level(tmp_path)
    assert wrapper.read_text(encoding="utf-8") == original


def test_runtime_rebuild_patches_an_existing_clone_before_compilation(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    env = setup_mono_env(tmp_path, "")
    runtime = Path(env["FO_WORKSPACE"]) / "runtime"
    wrapper = runtime / "src" / "native" / "external" / "zlib-ng.cmake"
    wrapper.parent.mkdir(parents=True)
    wrapper.write_text("FetchContent_MakeAvailable(fetchzlibng)\n", encoding="utf-8")
    calls = []

    def run_build_only(marker: Path, label: str, action: object) -> None:
        if label == "Build runtime":
            action()

    def check_patched_before_build(args: list[str], runtime_root: Path, *, target_os: str) -> None:
        assert runtime_root == runtime
        assert target_os == "linux"
        assert "list(REMOVE_ITEM fo_zlib_compile_options" in wrapper.read_text(encoding="utf-8")
        calls.append(args)

    monkeypatch.setattr(_buildtools, "run_marker_step", run_build_only)
    monkeypatch.setattr(_buildtools, "run_runtime_build", check_patched_before_build)
    _buildtools.setup_mono("linux", "x64", "Release", env)
    assert len(calls) == 1


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
