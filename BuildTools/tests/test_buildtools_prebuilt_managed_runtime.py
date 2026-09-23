from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


@pytest.fixture(autouse=True)
def no_workspace_cache(monkeypatch: pytest.MonkeyPatch) -> None:
    # CI jobs configure the cache for the whole job, and these tests exercise the build it would otherwise replace
    monkeypatch.delenv(_buildtools.WORKSPACE_CACHE_VAR, raising=False)


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
    assert (workspace / f"READY_v10.0.11_windows.x64.Release{_buildtools.MONO_WINDOWS_SOURCE_MARKER_SUFFIX}").is_file()


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


def test_mono_cmake_args_enable_overridable_allocators(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(_buildtools.os, "name", "nt")
    assert _buildtools.resolve_mono_cmake_args() == [f"/p:CMakeArgs={_buildtools.MONO_OVERRIDABLE_ALLOCATORS_CMAKE}"]
    monkeypatch.setattr(_buildtools.os, "name", "posix")
    assert _buildtools.resolve_mono_cmake_args() == [f"-p:CMakeArgs={_buildtools.MONO_OVERRIDABLE_ALLOCATORS_CMAKE}"]
    assert _buildtools.MONO_OVERRIDABLE_ALLOCATORS_CMAKE == "-DENABLE_OVERRIDABLE_ALLOCATORS=1"


def test_ready_marker_suffixes_match_the_cmake_stage() -> None:
    # The suffix is the cache key for an already-prepared host, so a rename that reaches only one of
    # the two places leaves runners serving a runtime built the old way
    stage = (BUILDTOOLS_DIR / "cmake" / "stages" / "ThirdParty.cmake").read_text(encoding="utf-8")

    for suffix in (
        _buildtools.MONO_BROWSER_SUBSET_MARKER_SUFFIX,
        _buildtools.MONO_ANDROID_SOURCE_MARKER_SUFFIX,
        _buildtools.MONO_APPLE_SOURCE_MARKER_SUFFIX,
        _buildtools.MONO_LINUX_SOURCE_MARKER_SUFFIX,
        _buildtools.MONO_WINDOWS_SOURCE_MARKER_SUFFIX,
        _buildtools.MONO_SUBSET_MARKER_SUFFIX,
    ):
        assert f"READY_${{FO_MONO_RUNTIME_VERSION}}_${{FO_MONO_TRIPLET}}{suffix})" in stage, suffix


@pytest.mark.skipif(shutil.which("cmake") is None, reason="CMake is required")
@pytest.mark.parametrize("target,flags", [
    ("linux", ["FO_LINUX"]), ("windows", ["FO_WINDOWS"]), ("browser", ["FO_WEB"]), ("android", ["FO_ANDROID"]),
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
    ("linux", False),
    pytest.param("windows", False, marks=pytest.mark.skipif(os.name != "nt", reason="setup-mono refuses a Windows runtime on another host")),
    ("browser", False),
    ("android", False), ("osx", False), ("ios", False), ("iossimulator", False),
    ("osx", True), ("ios", True), ("iossimulator", True),
])
def test_source_patch_cache_rebuilds_and_republishes_once(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, target: str, legacy_apple_patch: bool) -> None:
    env = setup_mono_env(tmp_path, "")
    workspace = Path(env["FO_WORKSPACE"])
    runtime = workspace / "runtime"
    runtime.mkdir(parents=True)
    triplet = f"{target}.x64.Release"
    if target == "browser":
        previous_suffix = _buildtools.MONO_SUBSET_MARKER_SUFFIX + "_wasmglue"
    elif legacy_apple_patch:
        previous_suffix = _buildtools.MONO_SUBSET_MARKER_SUFFIX + "_apple_sources"
    else:
        previous_suffix = _buildtools.MONO_SUBSET_MARKER_SUFFIX
    (workspace / "CLONED_v10.0.11").touch()
    for phase in ("BUILT", "READY"):
        (workspace / f"{phase}_v10.0.11_{triplet}{previous_suffix}").touch()
    published = make_published_tree(workspace / "output/mono", triplet)
    archive = published / "lib/libmonosgen-2.0.a"
    calls = []

    def reject_clone(*args, **kwargs):
        raise AssertionError("source patch invalidation must retain the cloned source")

    def build(command, path, **kwargs):
        calls.append("build")
        assert path == runtime and kwargs["target_os"] == target
        assert f"{'/p:' if os.name == 'nt' else '-p:'}CMakeArgs={_buildtools.MONO_OVERRIDABLE_ALLOCATORS_CMAKE}" in command
        out = runtime / "artifacts/obj/mono" / triplet / "out/lib"
        out.mkdir(parents=True)
        (out / "libmonosgen-2.0.a").write_text("patched runtime", encoding="utf-8")
        rid = f"{'win' if target == 'windows' else target}-x64"
        framework = runtime / "artifacts/bin" / f"microsoft.netcore.app.runtime.{rid}" / "Release/runtimes" / rid / "lib/net10.0"
        framework.mkdir(parents=True)
        (framework / "System.Runtime.dll").write_text("framework", encoding="utf-8")
        corelib = runtime / "artifacts/bin/mono" / triplet / "IL/System.Private.CoreLib.dll"
        corelib.parent.mkdir(parents=True)
        corelib.write_text("patched corelib", encoding="utf-8")

    monkeypatch.setattr(_buildtools, "clone_git_repo", reject_clone)
    patch_names = (
        "patch_runtime_zlib_warning_level",
        "patch_runtime_browser_asm_compiler",
        "patch_runtime_linux_signal_actions",
        "patch_runtime_apple_sources",
        "patch_runtime_ios_sources",
        "patch_runtime_android_sources",
        "patch_runtime_android_x86_atomics",
        "patch_runtime_windows_embedded_debug_info",
        "patch_runtime_windows_suspend_retry",
        "patch_runtime_isa_is_supported_fallback",
    )
    for name in patch_names:
        monkeypatch.setattr(_buildtools, name, lambda path, name=name: calls.append(name))
    monkeypatch.setattr(_buildtools, "run_runtime_build", build)
    monkeypatch.setattr(_buildtools, "copy_interop_shim_libraries", lambda *args: calls.append("publish"))
    monkeypatch.setattr(_buildtools, "copy_browser_runtime_glue", lambda *args: None)
    resolve = _buildtools.resolve_mono_marker_suffix
    with monkeypatch.context() as old_markers:
        old_markers.setattr(_buildtools, "resolve_mono_marker_suffix", lambda _target: previous_suffix)
        _buildtools.setup_mono(target, "x64", "Release", env)
    assert calls == [] and archive.read_text() == "archive"
    _buildtools.setup_mono(target, "x64", "Release", env)
    assert archive.read_text() == "patched runtime"
    expected_patch = (["patch_runtime_browser_asm_compiler"] if target == "browser" else
                      ["patch_runtime_linux_signal_actions"] if target == "linux" else
                      ["patch_runtime_android_sources", "patch_runtime_android_x86_atomics",
                       "patch_runtime_isa_is_supported_fallback"] if target == "android" else
                      ["patch_runtime_windows_embedded_debug_info", "patch_runtime_windows_suspend_retry",
                       "patch_runtime_isa_is_supported_fallback"]
                      if target == "windows" else
                      ["patch_runtime_apple_sources"])
    if target in ("ios", "iossimulator"):
        expected_patch.append("patch_runtime_ios_sources")
    assert calls == ["patch_runtime_zlib_warning_level", *expected_patch, "build", "publish"]
    for phase in ("BUILT", "READY"):
        assert (workspace / f"{phase}_v10.0.11_{triplet}{resolve(target)}").is_file()
    calls.clear()
    _buildtools.setup_mono(target, "x64", "Release", env)
    assert calls == []


def test_isa_is_supported_fallback_answers_false_once(tmp_path: Path) -> None:
    # Without it an ISA class the JIT does not implement runs CoreLib's recursive IsSupported body
    intrinsics = tmp_path / "src" / "mono" / "mono" / "mini" / "intrinsics.c"
    intrinsics.parent.mkdir(parents=True)
    intrinsics.write_text("\tins = mono_emit_simd_intrinsics (cfg, cmethod, fsig, args);\n"
                          "\t/* Fallback if SIMD is disabled */\n\tif (in_corlib) {}\n", encoding="utf-8")
    _buildtools.patch_runtime_isa_is_supported_fallback(tmp_path)
    _buildtools.patch_runtime_isa_is_supported_fallback(tmp_path)
    text = intrinsics.read_text(encoding="utf-8")
    assert text.count(_buildtools.MONO_ISA_FALLBACK_PATCH_MARKER) == 1
    assert text.index("System.Runtime.Intrinsics.X86") < text.index("/* Fallback if SIMD is disabled */")
    assert "m_class_get_nested_in (isa_klass)" in text


WINDOWS_SUSPEND_SOURCE = (
    "void\nmono_threads_suspend_init (void)\n{\n}\n\n"
    "gboolean\nmono_threads_suspend_begin_async_suspend (MonoThreadInfo *info, gboolean interrupt_kernel)\n{\n"
    "\tresult = SuspendThread (handle);\n"
    "\tTHREADS_SUSPEND_DEBUG (\"SUSPEND %p -> %u\\n\", GUINT_TO_POINTER (id), result);\n"
    "\tif (result == (DWORD)-1) {\n\t}\n"
    "\tif (!GetThreadContext (handle, context)) {\n\t\tresult = ResumeThread (handle);\n\t}\n}\n"
)


def write_windows_suspend_source(root: Path, text: str) -> Path:
    source = root / "src" / "mono" / "mono" / "utils" / "mono-threads-windows.c"
    source.parent.mkdir(parents=True)
    source.write_text(text, encoding="utf-8")
    return source


def test_windows_suspend_retry_replaces_both_refusals_once(tmp_path: Path) -> None:
    # A stop-the-world that skips a running thread leaves the heap corrupt, so neither refusal may reach the skip unretried
    source = write_windows_suspend_source(tmp_path, WINDOWS_SUSPEND_SOURCE)
    _buildtools.patch_runtime_windows_suspend_retry(tmp_path)
    _buildtools.patch_runtime_windows_suspend_retry(tmp_path)
    text = source.read_text(encoding="utf-8")
    assert text.count(_buildtools.MONO_WINDOWS_SUSPEND_RETRY_PATCH_MARKER) == 1
    assert "result = SuspendThread (handle);" not in text.split("mono_threads_suspend_begin_async_suspend (")[1]
    assert "result = fo_suspend_thread_retrying (handle, id);" in text
    assert "if (!fo_get_thread_context_retrying (handle, id, context)) {" in text
    assert text.index("fo_thread_is_alive (HANDLE handle)") < text.index("mono_threads_suspend_begin_async_suspend (")
    assert text.count("mono_threads_suspend_init (void)") == 1


def test_windows_suspend_retry_reports_without_heap_or_locks(tmp_path: Path) -> None:
    # A thread already stopped may hold the heap, a stdio or the loader lock, so a report that took one would hang the collection
    source = write_windows_suspend_source(tmp_path, WINDOWS_SUSPEND_SOURCE)
    _buildtools.patch_runtime_windows_suspend_retry(tmp_path)
    text = source.read_text(encoding="utf-8")
    helpers = text.split("mono_threads_suspend_begin_async_suspend (")[0]
    retrying = helpers.split("fo_thread_is_alive (HANDLE handle)")[1]
    assert "WriteFile (GetStdHandle (STD_ERROR_HANDLE)" in retrying
    assert "GetProcAddress" not in retrying
    for call in ("g_error", "g_warning", "g_strdup", "printf", "malloc", "GetThreadDescription", "LocalFree"):
        assert call not in retrying


@pytest.mark.parametrize(
    ("needle", "moved"),
    [
        ("result = SuspendThread (handle);", "result = SuspendThreadEx (handle);"),
        ("mono_threads_suspend_init (void)\n{\n}", "mono_threads_suspend_init (void)\n{\n\tinit ();\n}"),
    ],
)
def test_windows_suspend_retry_refuses_a_moved_anchor(tmp_path: Path, needle: str, moved: str) -> None:
    write_windows_suspend_source(tmp_path, WINDOWS_SUSPEND_SOURCE.replace(needle, moved))
    with pytest.raises(SystemExit, match="Mono Windows thread suspension"):
        _buildtools.patch_runtime_windows_suspend_retry(tmp_path)


def test_local_tasks_mark_is_discarded_for_every_configuration(tmp_path: Path) -> None:
    # A mark left by a build for another target keeps the target's own task projects out of the set
    tasks = tmp_path / "artifacts" / "obj" / "tasks"
    for config in ("Debug", "Release"):
        (tasks / config).mkdir(parents=True)
        (tasks / config / "build-semaphore.txt").write_text("done", encoding="utf-8")
    (tasks / "Release" / "keep.txt").write_text("other", encoding="utf-8")
    _buildtools.discard_runtime_local_tasks_semaphore(tmp_path)
    _buildtools.discard_runtime_local_tasks_semaphore(tmp_path)
    assert not list(tasks.glob("*/build-semaphore.txt"))
    assert (tasks / "Release" / "keep.txt").is_file()


def test_isa_is_supported_fallback_refuses_a_moved_anchor(tmp_path: Path) -> None:
    intrinsics = tmp_path / "src" / "mono" / "mono" / "mini" / "intrinsics.c"
    intrinsics.parent.mkdir(parents=True)
    intrinsics.write_text("\t/* moved */\n", encoding="utf-8")
    with pytest.raises(SystemExit):
        _buildtools.patch_runtime_isa_is_supported_fallback(tmp_path)
    assert intrinsics.read_text(encoding="utf-8") == "\t/* moved */\n"


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


def test_browser_mono_inherits_the_identified_c_compiler_for_asm(tmp_path: Path) -> None:
    cmake_lists = tmp_path / "src/mono/mono/utils/CMakeLists.txt"
    cmake_lists.parent.mkdir(parents=True)
    cmake_lists.write_text(
        "elseif(HOST_WASM)\n"
        '    set (CMAKE_ASM_COMPILER_VERSION "${CMAKE_C_COMPILER_VERSION}")\n'
        '    set (CMAKE_ASM_COMPILER_TARGET "${CMAKE_C_COMPILER_TARGET}")\n'
        "    enable_language(ASM)\n"
        "endif()\n",
        encoding="utf-8",
    )

    _buildtools.patch_runtime_browser_asm_compiler(tmp_path)
    patched = cmake_lists.read_text(encoding="utf-8")

    assert 'set (CMAKE_ASM_COMPILER_ID "${CMAKE_C_COMPILER_ID}")' in patched
    assert "(FOnline Patch) Generic ASM uses the already identified Emscripten C compiler" in patched
    assert patched.index("CMAKE_ASM_COMPILER_ID") < patched.index("enable_language(ASM)")

    _buildtools.patch_runtime_browser_asm_compiler(tmp_path)
    assert cmake_lists.read_text(encoding="utf-8") == patched


@pytest.mark.parametrize("anchor_count", [0, 2])
def test_browser_mono_asm_patch_requires_a_unique_anchor(tmp_path: Path, anchor_count: int) -> None:
    cmake_lists = tmp_path / "src/mono/mono/utils/CMakeLists.txt"
    cmake_lists.parent.mkdir(parents=True)
    anchor = 'elseif(HOST_WASM)\n    set (CMAKE_ASM_COMPILER_VERSION "${CMAKE_C_COMPILER_VERSION}")\n'
    original = anchor * anchor_count
    cmake_lists.write_text(original, encoding="utf-8")

    with pytest.raises(SystemExit, match="unique anchor not found"):
        _buildtools.patch_runtime_browser_asm_compiler(tmp_path)

    assert cmake_lists.read_text(encoding="utf-8") == original


def test_mono_linux_signal_actions_are_initialized_for_msan(tmp_path: Path) -> None:
    source = tmp_path / "src/mono/mono/mini/mini-posix.c"
    source.parent.mkdir(parents=True)
    source.write_text(
        "static GHashTable *mono_saved_signal_handlers = NULL;\n"
        "struct sigaction *handler_to_save = (struct sigaction *)g_malloc (sizeof (struct sigaction));\n"
        "\tstruct sigaction previous_sa;\n\n#ifdef MONO_ARCH_USE_SIGACTION\n"
        "\tstruct sigaction *saved_action = get_saved_signal_handler (signo);\n\n\tif (!saved_action) {\n",
        encoding="utf-8",
    )

    _buildtools.patch_runtime_linux_signal_actions(tmp_path)
    patched = source.read_text(encoding="utf-8")

    assert _buildtools.MONO_LINUX_SIGNAL_ACTION_PATCH_MARKER in patched
    assert "memset (action, 0, sizeof (*action));" in patched
    assert "__msan_unpoison (action, sizeof (*action));" in patched
    assert patched.count("initialize_signal_action (") == 4

    _buildtools.patch_runtime_linux_signal_actions(tmp_path)
    assert source.read_text(encoding="utf-8") == patched


def test_mono_linux_signal_patch_fails_loudly_when_an_anchor_moves(tmp_path: Path) -> None:
    source = tmp_path / "src/mono/mono/mini/mini-posix.c"
    source.parent.mkdir(parents=True)
    source.write_text("static GHashTable *mono_saved_signal_handlers = NULL;\n", encoding="utf-8")

    with pytest.raises(SystemExit, match="unique anchor not found"):
        _buildtools.patch_runtime_linux_signal_actions(tmp_path)


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


WINDOWS_CONFIGURE_COMPILER = (
    "if (MSVC)\n"
    "  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX,ASM_MASM>:/Zi>) # enable debugging information\n"
    "endif (MSVC)\n"
)
WINDOWS_MONO_CMAKE_LISTS = (
    "cmake_minimum_required(VERSION 3.20)\nproject(mono C)\n"
    'set(CMAKE_C_FLAGS "")\nset(CMAKE_C_FLAGS_DEBUG "/Zi /Ob0 /Od /RTC1")\nset(MSVC ON)\n'
    "if(MSVC)\n"
    '  if(CMAKE_BUILD_TYPE STREQUAL "Release")\n'
    "    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/Zi>) # enable debugging information\n"
    "  endif()\n"
    "endif()\n"
    "add_library(mono STATIC mono.c)\n"
)


def write_windows_runtime_debug_info_fixture(root: Path) -> None:
    native = root / "eng" / "native"
    native.mkdir(parents=True)
    (native / "configurecompiler.cmake").write_text(WINDOWS_CONFIGURE_COMPILER, encoding="utf-8")
    mono = root / "src" / "mono"
    mono.mkdir(parents=True)
    (mono / "CMakeLists.txt").write_text(WINDOWS_MONO_CMAKE_LISTS, encoding="utf-8")
    (mono / "mono.c").write_text("int mono_probe;\n", encoding="utf-8")
    libs = root / "src" / "native" / "libs"
    libs.mkdir(parents=True)
    (libs / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\nproject(LibsNative C)\n"
        'set(CMAKE_C_FLAGS "")\nset(CMAKE_C_FLAGS_DEBUG "/Zi /Ob0 /Od /RTC1")\nset(MSVC ON)\n'
        'include("${CMAKE_CURRENT_LIST_DIR}/../../../eng/native/configurecompiler.cmake")\n'
        "add_library(native STATIC native.c)\n",
        encoding="utf-8",
    )
    (libs / "native.c").write_text("int native_probe;\n", encoding="utf-8")
    (root / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\nproject(RuntimeDebugInfo C)\n"
        "add_subdirectory(src/mono)\nadd_subdirectory(src/native/libs)\n",
        encoding="utf-8",
    )


def runtime_debug_info_sources(root: Path) -> dict[Path, str]:
    paths = (root / "eng/native/configurecompiler.cmake", root / "src/mono/CMakeLists.txt")
    return {path: path.read_text(encoding="utf-8") for path in paths}


@pytest.mark.skipif(shutil.which("cmake") is None or shutil.which("ninja") is None, reason="CMake and Ninja are required")
def test_windows_runtime_objects_embed_their_debug_info_in_every_configuration(tmp_path: Path) -> None:
    # A /Zi object keeps its debug info in a compiler PDB the published archive does not carry, so each
    # consumer link reports LNK4099 for it; /Z7 is the format that survives the archive
    write_windows_runtime_debug_info_fixture(tmp_path)

    def debug_info_flags(name: str, config: str) -> dict[str, list[str]]:
        build = tmp_path / f"{name}-{config}"
        result = subprocess.run(
            ["cmake", "-S", str(tmp_path), "-B", str(build), "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={config}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
            capture_output=True, text=True, check=True,
        )
        assert "warning" not in result.stderr.lower(), result.stderr
        commands = json.loads((build / "compile_commands.json").read_text())
        return {Path(entry["file"]).name: [flag for flag in entry["command"].split() if flag in ("/Zi", "/Z7")] for entry in commands}

    for config in ("Debug", "Release"):
        for source, flags in debug_info_flags("original", config).items():
            assert "/Zi" in flags, (source, config, flags)

    _buildtools.patch_runtime_windows_embedded_debug_info(tmp_path)
    patched = runtime_debug_info_sources(tmp_path)
    _buildtools.patch_runtime_windows_embedded_debug_info(tmp_path)
    assert runtime_debug_info_sources(tmp_path) == patched
    assert "$<$<COMPILE_LANGUAGE:ASM_MASM>:/Zi>" in patched[tmp_path / "eng/native/configurecompiler.cmake"]

    for config in ("Debug", "Release"):
        flags = debug_info_flags("patched", config)
        assert set(flags) == {"mono.c", "native.c"}
        for source, source_flags in flags.items():
            assert source_flags and set(source_flags) == {"/Z7"}, (source, config, source_flags)


@pytest.mark.parametrize("moved", ["configurecompiler", "mono-release", "mono-debug"])
def test_windows_debug_info_patch_requires_every_anchor_before_writing(tmp_path: Path, moved: str) -> None:
    write_windows_runtime_debug_info_fixture(tmp_path)
    configure_compiler = tmp_path / "eng/native/configurecompiler.cmake"
    mono = tmp_path / "src/mono/CMakeLists.txt"

    if moved == "configurecompiler":
        configure_compiler.write_text(WINDOWS_CONFIGURE_COMPILER.replace("C,CXX,ASM_MASM", "C,CXX"), encoding="utf-8")
    elif moved == "mono-release":
        mono.write_text(WINDOWS_MONO_CMAKE_LISTS.replace(":/Zi>", ":/Z7>"), encoding="utf-8")
    else:
        mono.write_text(WINDOWS_MONO_CMAKE_LISTS.replace('if(CMAKE_BUILD_TYPE STREQUAL "Release")', 'if(CMAKE_BUILD_TYPE MATCHES "Release")'), encoding="utf-8")

    originals = runtime_debug_info_sources(tmp_path)
    with pytest.raises(SystemExit, match="unique anchor not found"):
        _buildtools.patch_runtime_windows_embedded_debug_info(tmp_path)
    assert runtime_debug_info_sources(tmp_path) == originals


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
        assert f"{'/p:' if os.name == 'nt' else '-p:'}CMakeArgs={_buildtools.MONO_OVERRIDABLE_ALLOCATORS_CMAKE}" in args
        calls.append(args)

    monkeypatch.setattr(_buildtools, "run_marker_step", run_build_only)
    monkeypatch.setattr(_buildtools, "run_runtime_build", check_patched_before_build)
    monkeypatch.setattr(_buildtools, "patch_runtime_linux_signal_actions", lambda path: None)
    _buildtools.setup_mono("linux", "x64", "Release", env)
    assert len(calls) == 1


def test_runtime_revision_uses_a_distinct_ready_marker(tmp_path: Path) -> None:
    tree = make_published_tree(tmp_path / "prebuilt", "windows.x64.Release")
    env = setup_mono_env(tmp_path, tree)
    _buildtools.setup_mono("windows", "x64", "Release", env)
    env["FO_DOTNET_RUNTIME"] = "v10.0.12"
    _buildtools.setup_mono("windows", "x64", "Release", env)
    markers = sorted(path.name for path in (tmp_path / "workspace").glob("READY_*"))
    suffix = _buildtools.MONO_WINDOWS_SOURCE_MARKER_SUFFIX
    assert markers == [f"READY_v10.0.11_windows.x64.Release{suffix}", f"READY_v10.0.12_windows.x64.Release{suffix}"]


@pytest.mark.parametrize("missing", [None, "corelib", "class_libraries"])
@pytest.mark.parametrize("target,rid", [("linux", "linux-x64"), ("browser", "browser-wasm"), ("android", "android-arm64")])
def test_publish_takes_target_class_libraries_and_replaces_old_files_only_after_input_validation(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, missing: str | None, target: str, rid: str,
) -> None:
    env = setup_mono_env(tmp_path, "")
    workspace = Path(env["FO_WORKSPACE"])
    runtime = workspace / "runtime"
    arch = rid.split("-", 1)[1]
    triplet = f"{target}.{arch}.Release"
    output = workspace / "output" / "mono" / triplet
    output.mkdir(parents=True)
    (output / "removed-library.so").write_text("stale", encoding="utf-8")
    inputs = runtime / "artifacts" / "obj" / "mono" / triplet / "out"
    inputs.mkdir(parents=True)
    (inputs / "current-library.so").write_text("current", encoding="utf-8")
    # The SDK dotnet/runtime builds itself with carries a host-OS shared framework that must never be published
    sdk_framework = runtime / ".dotnet" / "shared" / "Microsoft.NETCore.App" / "10.0.9"
    sdk_framework.mkdir(parents=True)
    (sdk_framework / "System.Runtime.dll").write_text("host sdk", encoding="utf-8")
    (sdk_framework / "System.Net.Http.dll").write_text("host sdk", encoding="utf-8")
    if missing != "class_libraries":
        pack = runtime / "artifacts" / "bin" / f"microsoft.netcore.app.runtime.{rid}" / "Release" / "runtimes" / rid / "lib" / "net10.0"
        pack.mkdir(parents=True)
        (pack / "System.Runtime.dll").write_text("target", encoding="utf-8")
    corelib = runtime / "artifacts" / "bin" / "mono" / triplet / "IL" / "System.Private.CoreLib.dll"
    if missing != "corelib":
        corelib.parent.mkdir(parents=True)
        corelib.write_text("mono corelib", encoding="utf-8")

    def run_publish_only(marker: Path, label: str, action: object) -> None:
        if label.startswith("Publish runtime"):
            action()

    monkeypatch.setattr(_buildtools, "run_marker_step", run_publish_only)
    monkeypatch.setattr(_buildtools, "copy_interop_shim_libraries", lambda *args: None)
    monkeypatch.setattr(_buildtools, "copy_browser_runtime_glue", lambda *args: None)
    if missing is not None:
        match = "System.Private.CoreLib not found" if missing == "corelib" else f"{rid} runtime pack"
        with pytest.raises(SystemExit, match=match):
            _buildtools.setup_mono(target, arch, "Release", env)
        assert (output / "removed-library.so").is_file()
    else:
        _buildtools.setup_mono(target, arch, "Release", env)
        netcoreapp = output / "lib" / "netcoreapp"
        assert not (output / "removed-library.so").exists()
        assert (output / "current-library.so").is_file()
        assert sorted(path.name for path in netcoreapp.iterdir()) == ["System.Private.CoreLib.dll", "System.Runtime.dll"]
        assert (netcoreapp / "System.Runtime.dll").read_text() == "target"
        assert (netcoreapp / "System.Private.CoreLib.dll").read_text() == "mono corelib"
