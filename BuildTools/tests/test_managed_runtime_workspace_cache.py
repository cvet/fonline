from __future__ import annotations

import inspect
from pathlib import Path
import re
import shutil
import sys
import tarfile
import urllib.error

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


CACHE_URL = "https://ci.example/cache/workspaces"
# The expression MSBuild matches against custom build step output; a match fails the step whatever its exit code
MSBUILD_CANONICAL_ERROR = re.compile(
    r"^\s*(((?P<origin>(((\d+>)?[a-zA-Z]?:[^:]*)|([^:]*))):)|())(?P<subcategory>(()|([^:]*? )))"
    r"(?P<category>(error|warning))( \s*(?P<code>[^: ]*))?\s*:(?P<text>.*)$",
    re.IGNORECASE,
)


class FakeWorkspaceCache:
    def __init__(self) -> None:
        self.archives: dict[str, bytes] = {}
        self.fetched: list[str] = []

    def fetch(self, name: str, target: Path) -> bool:
        self.fetched.append(name)

        if name not in self.archives:
            return False

        target.write_bytes(self.archives[name])
        return True

    def store(self, name: str, source: Path) -> None:
        self.archives[name] = source.read_bytes()


class FakeRuntimeBuild:
    def __init__(self) -> None:
        self.calls = 0
        self.reused_markers: list[str] = []

    # Runs whenever it is called, like the real build: whether a published tree makes it unnecessary is setup_mono's call
    def __call__(self, os_name: str, arch: str, config: str, env: dict[str, str]) -> None:
        layout = _buildtools.resolve_mono_layout(os_name, arch, config, env)
        self.calls += 1
        self.reused_markers.extend(marker.name for marker in (layout.clone_marker, layout.built_marker) if marker.exists())
        make_published_tree(layout.output_dir, f"built by call {self.calls}")
        layout.ready_marker.touch()


@pytest.fixture
def cache(monkeypatch: pytest.MonkeyPatch) -> FakeWorkspaceCache:
    fake = FakeWorkspaceCache()
    monkeypatch.setenv(_buildtools.WORKSPACE_CACHE_VAR, CACHE_URL)
    monkeypatch.setattr(_buildtools, "workspace_cache_fetch", fake.fetch)
    monkeypatch.setattr(_buildtools, "workspace_cache_store", fake.store)
    monkeypatch.setattr(_buildtools, "describe_mono_host_toolchain", lambda: ["host=test"])
    return fake


@pytest.fixture
def build(monkeypatch: pytest.MonkeyPatch) -> FakeRuntimeBuild:
    fake = FakeRuntimeBuild()
    monkeypatch.setattr(_buildtools, "build_mono", fake)
    return fake


def make_env(workspace: Path, **overrides: str) -> dict[str, str]:
    return {"FO_WORKSPACE": str(workspace), "FO_DOTNET_RUNTIME": "v10.0.11", "FO_MANAGED_RUNTIME_PREBUILT": "", **overrides}


def make_published_tree(output_dir: Path, content: str) -> None:
    headers = output_dir / "include" / "mono-2.0" / "mono" / "jit"
    headers.mkdir(parents=True)
    (headers / "jit.h").write_text("header", encoding="utf-8")
    netcoreapp = output_dir / "lib" / "netcoreapp"
    netcoreapp.mkdir(parents=True)
    (netcoreapp / "System.Private.CoreLib.dll").write_text(content, encoding="utf-8")
    (output_dir / "lib" / "libmonosgen-2.0.a").write_text(content, encoding="utf-8")


def tree_files(root: Path) -> dict[str, str]:
    return {path.relative_to(root).as_posix(): path.read_text(encoding="utf-8") for path in sorted(root.rglob("*")) if path.is_file()}


def test_a_cold_miss_builds_and_publishes_the_tree_that_the_next_job_restores(
    tmp_path: Path, cache: FakeWorkspaceCache, build: FakeRuntimeBuild,
) -> None:
    first = make_env(tmp_path / "first")
    _buildtools.setup_mono("linux", "x64", "Release", first)

    assert build.calls == 1
    assert len(cache.archives) == 1
    assert not list((tmp_path / "first").glob("*.tar.gz"))

    second = make_env(tmp_path / "second")
    _buildtools.setup_mono("linux", "x64", "Release", second)

    first_layout = _buildtools.resolve_mono_layout("linux", "x64", "Release", first)
    second_layout = _buildtools.resolve_mono_layout("linux", "x64", "Release", second)
    assert build.calls == 1
    assert cache.fetched == [next(iter(cache.archives))] * 2
    assert second_layout.ready_marker.is_file()
    assert tree_files(second_layout.output_dir) == tree_files(first_layout.output_dir)
    assert not list(second_layout.output_dir.parent.glob(".*-cache-*"))
    assert not list((tmp_path / "second").glob("*.tar.gz"))


def test_a_ready_workspace_neither_fetches_nor_publishes(tmp_path: Path, cache: FakeWorkspaceCache, build: FakeRuntimeBuild) -> None:
    env = make_env(tmp_path / "workspace")
    _buildtools.setup_mono("linux", "x64", "Release", env)
    cache.fetched.clear()
    cache.archives.clear()

    _buildtools.setup_mono("linux", "x64", "Release", env)

    assert cache.fetched == []
    assert cache.archives == {}
    assert build.calls == 1


def test_a_tree_restored_from_the_cache_is_not_rebuilt_by_another_build_directory(
    tmp_path: Path, cache: FakeWorkspaceCache, build: FakeRuntimeBuild,
) -> None:
    # A restored tree carries its ready marker but none of the clone and build markers, and a second build directory
    # sharing the workspace used to take that for an unfinished source build
    _buildtools.setup_mono("linux", "x64", "Release", make_env(tmp_path / "first"))
    env = make_env(tmp_path / "second")
    _buildtools.setup_mono("linux", "x64", "Release", env)
    layout = _buildtools.resolve_mono_layout("linux", "x64", "Release", env)
    assert not layout.clone_marker.exists()
    assert not layout.built_marker.exists()
    cache.fetched.clear()

    _buildtools.setup_mono("linux", "x64", "Release", env)

    assert build.calls == 1
    assert cache.fetched == []


def test_a_ready_marker_without_its_tree_is_published_again(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, cache: FakeWorkspaceCache, build: FakeRuntimeBuild,
) -> None:
    monkeypatch.delenv(_buildtools.WORKSPACE_CACHE_VAR)
    env = make_env(tmp_path / "workspace")
    _buildtools.setup_mono("linux", "x64", "Release", env)
    layout = _buildtools.resolve_mono_layout("linux", "x64", "Release", env)
    shutil.rmtree(layout.output_dir)

    _buildtools.setup_mono("linux", "x64", "Release", env)

    assert build.calls == 2
    assert layout.ready_marker.is_file()
    assert (layout.output_dir / "lib" / "netcoreapp" / "System.Private.CoreLib.dll").read_text(encoding="utf-8") == "built by call 2"


def test_an_incomplete_cached_tree_is_a_miss(tmp_path: Path, cache: FakeWorkspaceCache, build: FakeRuntimeBuild) -> None:
    env = make_env(tmp_path / "workspace")
    name = _buildtools.build_mono_workspace_cache_name("linux", "x64", "Release", env)
    incomplete = tmp_path / "incomplete" / "linux.x64.Release"
    (incomplete / "include" / "mono-2.0").mkdir(parents=True)
    archive_path = tmp_path / "incomplete.tar.gz"

    with tarfile.open(archive_path, "w:gz") as archive:
        archive.add(incomplete, arcname=incomplete.name)

    cache.archives[name] = archive_path.read_bytes()
    _buildtools.setup_mono("linux", "x64", "Release", env)

    layout = _buildtools.resolve_mono_layout("linux", "x64", "Release", env)
    assert build.calls == 1
    assert (layout.output_dir / "lib" / "netcoreapp" / "System.Private.CoreLib.dll").read_text(encoding="utf-8") == "built by call 1"
    assert cache.archives[name] != archive_path.read_bytes()


@pytest.mark.parametrize("cache_enabled", [True, False])
def test_a_miss_builds_from_a_fresh_clone_before_publishing_to_the_cache(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, cache: FakeWorkspaceCache, build: FakeRuntimeBuild, cache_enabled: bool,
) -> None:
    # A persistent workspace keeps source and objects under markers that predate the current build code, and a tree
    # published from them would reach every job under the new key
    if not cache_enabled:
        monkeypatch.delenv(_buildtools.WORKSPACE_CACHE_VAR)

    env = make_env(tmp_path / "workspace")
    layout = _buildtools.resolve_mono_layout("linux", "x64", "Release", env)
    layout.workspace.mkdir(parents=True)
    layout.clone_marker.touch()
    layout.built_marker.touch()

    _buildtools.setup_mono("linux", "x64", "Release", env)

    assert build.calls == 1
    assert build.reused_markers == ([] if cache_enabled else [layout.clone_marker.name, layout.built_marker.name])
    assert len(cache.archives) == int(cache_enabled)


def test_a_local_runtime_source_is_never_shared(tmp_path: Path, cache: FakeWorkspaceCache, build: FakeRuntimeBuild) -> None:
    env = make_env(tmp_path / "workspace", FO_DOTNET_RUNTIME_ROOT=str(tmp_path / "runtime-checkout"))

    _buildtools.setup_mono("linux", "x64", "Release", env)

    assert build.calls == 1
    assert cache.fetched == []
    assert cache.archives == {}


def test_without_a_configured_cache_the_runtime_is_only_built(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, cache: FakeWorkspaceCache, build: FakeRuntimeBuild,
) -> None:
    monkeypatch.delenv(_buildtools.WORKSPACE_CACHE_VAR)

    _buildtools.setup_mono("linux", "x64", "Release", make_env(tmp_path / "workspace"))

    assert build.calls == 1
    assert cache.fetched == []
    assert cache.archives == {}


def test_the_cache_name_separates_everything_that_changes_the_published_tree(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, cache: FakeWorkspaceCache,
) -> None:
    env = make_env(tmp_path, FO_EMSCRIPTEN_VERSION="6.0.8", FO_ANDROID_NDK_VERSION="android-ndk-r29")

    def name(os_name: str = "linux", arch: str = "x64", config: str = "Release", **changes: str) -> str:
        return _buildtools.build_mono_workspace_cache_name(os_name, arch, config, {**env, **changes})

    base = name()
    assert base.startswith("managed-runtime-v10.0.11-linux.x64.Release-") and base.endswith(".tar.gz")
    assert name() == base

    variants = [
        name(FO_DOTNET_RUNTIME="v10.0.12"),
        name(os_name="android"),
        name(arch="arm64"),
        name(config="Debug"),
        name(os_name="browser", arch="wasm"),
        name(os_name="android", FO_ANDROID_NDK_VERSION="android-ndk-r30"),
        name(os_name="browser", arch="wasm", FO_EMSCRIPTEN_VERSION="6.0.9"),
    ]
    assert len({base, *variants}) == len(variants) + 1

    # A target pin reaches only the targets whose archives that toolchain compiles
    assert name(FO_EMSCRIPTEN_VERSION="6.0.9", FO_ANDROID_NDK_VERSION="android-ndk-r30") == base

    monkeypatch.setattr(_buildtools, "describe_mono_host_toolchain", lambda: ["host=test", "clang=Ubuntu clang version 20.1.8"])
    assert name() != base

    monkeypatch.setattr(_buildtools, "describe_mono_host_toolchain", lambda: ["host=test"])
    monkeypatch.setattr(_buildtools, "fingerprint_module_functions", lambda source, roots: "changed build code")
    assert name() != base


BUILD_CODE_SOURCE = '''
CONSTANT = 1
UNRELATED_CONSTANT = 2


def root():
    # Explains the call
    helper()
    return CONSTANT  # trailing note


def helper():
    return """
#include <runtime.h>
"""


def unrelated():
    return UNRELATED_CONSTANT
'''


@pytest.mark.parametrize("edit,changes", [
    (("helper()", "helper(1)"), True),
    (("CONSTANT = 1", "CONSTANT = 3"), True),
    (("#include <runtime.h>", "#include <other.h>"), True),
    (("return UNRELATED_CONSTANT", "return None"), False),
    (("UNRELATED_CONSTANT = 2", "UNRELATED_CONSTANT = 4"), False),
    (("# Explains the call", "# Explains it differently"), False),
    (("# trailing note", "# another note"), False),
])
def test_build_code_fingerprint_follows_only_the_code_the_root_reaches(edit: tuple[str, str], changes: bool) -> None:
    edited = BUILD_CODE_SOURCE.replace(*edit, 1)
    assert edited != BUILD_CODE_SOURCE

    before = _buildtools.fingerprint_module_functions(BUILD_CODE_SOURCE, ("root",))
    after = _buildtools.fingerprint_module_functions(edited, ("root",))
    assert (before != after) == changes


def test_the_runtime_build_code_in_the_key_covers_every_patch_and_no_cache_code() -> None:
    reached = _buildtools.collect_module_dependencies(inspect.getsource(_buildtools), ("build_mono",))
    patches = {name for name in vars(_buildtools) if name.startswith("patch_runtime_")}

    assert patches and patches <= set(reached)
    assert {
        "run_runtime_build",
        "resolve_runtime_pack_class_library_dir",
        "copy_interop_shim_libraries",
        "copy_browser_runtime_glue",
        "MONO_RUNTIME_SUBSET",
    } <= set(reached)
    assert not {"setup_mono", "workspace_cache_fetch", "workspace_cache_store_tree", "build_mono_workspace_cache_name"} & set(reached)


def refuse_with_http_error(code: int, reason: str):
    def refuse(url: str, path: Path) -> None:
        raise urllib.error.HTTPError(url, code, reason, None, None)

    return refuse


@pytest.mark.parametrize("step", ["fetch", "store"])
def test_a_cache_failure_is_logged_without_the_shape_msbuild_fails_a_build_step_on(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str], step: str
) -> None:
    # A Windows runtime build that missed the cache built and published the runtime, then failed on its own miss line
    monkeypatch.setenv(_buildtools.WORKSPACE_CACHE_VAR, CACHE_URL)
    archive = tmp_path / "runtime.tar.gz"

    if step == "fetch":
        monkeypatch.setattr(_buildtools, "fetch_url", refuse_with_http_error(404, "Not Found"))
        assert not _buildtools.workspace_cache_fetch("runtime.tar.gz", archive)
    else:
        archive.write_bytes(b"runtime")
        monkeypatch.setattr(_buildtools, "upload_url", refuse_with_http_error(500, "Internal Server Error"))
        _buildtools.workspace_cache_store("runtime.tar.gz", archive)

    lines = capsys.readouterr().out.splitlines()
    assert any("runtime.tar.gz" in line and "HTTP Error" in line for line in lines)
    assert not [line for line in lines if MSBUILD_CANONICAL_ERROR.match(line)]
