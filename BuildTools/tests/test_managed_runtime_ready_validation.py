from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import buildtools as _buildtools
from managed_runtime_fixtures import ENGINE_ROOT, TARGETS, published_runtime_files, write_published_runtime


@pytest.fixture(autouse=True)
def isolate_cache(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv(_buildtools.WORKSPACE_CACHE_VAR, raising=False)


def make_env(workspace: Path, prebuilt: str = "") -> dict[str, str]:
    return {"FO_WORKSPACE": str(workspace), "FO_DOTNET_RUNTIME": "v10.0.11", "FO_MANAGED_RUNTIME_PREBUILT": prebuilt}


@pytest.mark.parametrize("target", TARGETS)
@pytest.mark.parametrize("damage", ("missing", "empty"))
def test_ready_does_not_certify_an_empty_directory(tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
                                                 target: str, damage: str) -> None:
    env = make_env(tmp_path / "workspace")
    layout = _buildtools.resolve_mono_layout(target, "x64", "Release", env)
    layout.output_dir.mkdir(parents=True)
    if damage == "missing":
        write_published_runtime(layout.output_dir, target)
        archive = "monosgen-2.0.lib" if target == "windows" else "libmonosgen-2.0.a"
        (layout.output_dir / "lib" / archive).unlink()
    layout.ready_marker.touch()
    layout.clone_marker.touch()
    layout.built_marker.touch()
    calls = []

    def republish(*args: object) -> None:
        assert not layout.ready_marker.exists()
        assert layout.clone_marker.is_file() and layout.built_marker.is_file()
        calls.append(args)
        write_published_runtime(layout.output_dir, target, "recovered")
        layout.ready_marker.touch()

    if target == "windows":
        monkeypatch.setattr(_buildtools, "os", SimpleNamespace(name="nt", environ=_buildtools.os.environ))
    monkeypatch.setattr(_buildtools, "build_mono", republish)
    _buildtools.setup_mono(target, "x64", "Release", env)
    assert len(calls) == 1
    assert layout.ready_marker.is_file()
    assert (layout.output_dir / "lib/netcoreapp/System.Private.CoreLib.dll").read_text() == "recovered"


@pytest.mark.parametrize("target,relative", [(target, relative) for target in TARGETS
                                           for relative in published_runtime_files(target)])
@pytest.mark.parametrize("damage", ("missing", "empty"))
def test_incomplete_prebuilt_input_is_rejected_before_copy(tmp_path: Path, target: str,
                                                         relative: str, damage: str) -> None:
    tree = tmp_path / "arbitrary-directory-name"
    write_published_runtime(tree, target)
    if damage == "missing":
        (tree / relative).unlink()
    else:
        (tree / relative).write_bytes(b"")
    env = make_env(tmp_path / "workspace", str(tree))
    layout = _buildtools.resolve_mono_layout(target, "x64", "Release", env)
    with pytest.raises(SystemExit, match="not a published runtime tree"):
        _buildtools.setup_mono(target, "x64", "Release", env)
    assert not layout.output_dir.exists()
    assert not layout.ready_marker.exists()


@pytest.mark.parametrize("target", TARGETS)
def test_complete_prebuilt_input_accepts_an_arbitrary_directory_name(tmp_path: Path, target: str) -> None:
    tree = tmp_path / "arbitrary-directory-name"
    write_published_runtime(tree, target)
    env = make_env(tmp_path / "workspace", str(tree))
    _buildtools.setup_mono(target, "x64", "Release", env)
    layout = _buildtools.resolve_mono_layout(target, "x64", "Release", env)
    assert layout.ready_marker.is_file()
    assert all((layout.output_dir / relative).is_file() for relative in published_runtime_files(target))


@pytest.mark.parametrize("target", TARGETS)
def test_an_incomplete_fresh_publication_never_keeps_ready(tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
                                                        target: str) -> None:
    env = make_env(tmp_path / "workspace")
    layout = _buildtools.resolve_mono_layout(target, "x64", "Release", env)

    def incomplete_build(*args: object) -> None:
        layout.output_dir.mkdir(parents=True)
        layout.ready_marker.touch()

    if target == "windows":
        monkeypatch.setattr(_buildtools, "os", SimpleNamespace(name="nt", environ=_buildtools.os.environ))
    monkeypatch.setattr(_buildtools, "build_mono", incomplete_build)
    with pytest.raises(SystemExit, match="Incomplete published managed runtime"):
        _buildtools.setup_mono(target, "x64", "Release", env)
    assert not layout.ready_marker.exists()


@pytest.mark.parametrize("target", TARGETS)
def test_a_complete_ready_tree_is_reused_without_a_source_build(tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
                                                              target: str) -> None:
    env = make_env(tmp_path / "workspace")
    layout = _buildtools.resolve_mono_layout(target, "x64", "Release", env)
    write_published_runtime(layout.output_dir, target)
    layout.ready_marker.touch()

    def reject_build(*args: object) -> None:
        raise AssertionError("a complete READY tree must not be rebuilt")

    if target == "windows":
        monkeypatch.setattr(_buildtools, "os", SimpleNamespace(name="nt", environ=_buildtools.os.environ))
    monkeypatch.setattr(_buildtools, "build_mono", reject_build)
    _buildtools.setup_mono(target, "x64", "Release", env)
    assert not layout.clone_marker.exists() and not layout.built_marker.exists()


@pytest.mark.parametrize("target", TARGETS)
def test_missing_archive_is_republished_from_existing_source_outputs(tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
                                                                   target: str) -> None:
    env = make_env(tmp_path / "workspace")
    layout = _buildtools.resolve_mono_layout(target, "x64", "Release", env)
    write_published_runtime(layout.output_dir, target)
    archive = "monosgen-2.0.lib" if target == "windows" else "libmonosgen-2.0.a"
    (layout.output_dir / "lib" / archive).unlink()
    for marker in (layout.clone_marker, layout.built_marker, layout.ready_marker):
        marker.touch()
    source_output = layout.runtime_root / "artifacts/obj/mono" / layout.runtime_triplet / "out"
    write_published_runtime(source_output, target, "source outputs")
    rid = f"{'win' if target == 'windows' else target}-x64"
    framework = layout.runtime_root / "artifacts/bin" / f"microsoft.netcore.app.runtime.{rid}" / "Release/runtimes" / rid / "lib/net10.0"
    framework.mkdir(parents=True)
    (framework / "System.Runtime.dll").write_text("target classes", encoding="utf-8")
    corelib = layout.runtime_root / "artifacts/bin/mono" / layout.runtime_triplet / "IL/System.Private.CoreLib.dll"
    corelib.parent.mkdir(parents=True)
    corelib.write_text("target corelib", encoding="utf-8")

    def reject_rebuild(*args: object, **kwargs: object) -> None:
        raise AssertionError("republication must reuse the already built source outputs")

    if target == "windows":
        monkeypatch.setattr(_buildtools, "os", SimpleNamespace(name="nt", environ=_buildtools.os.environ))
    monkeypatch.setattr(_buildtools, "clone_git_repo", reject_rebuild)
    monkeypatch.setattr(_buildtools, "run_runtime_build", reject_rebuild)
    monkeypatch.setattr(_buildtools, "copy_interop_shim_libraries", lambda *args: None)
    monkeypatch.setattr(_buildtools, "copy_browser_runtime_glue", lambda *args: None)
    _buildtools.setup_mono(target, "x64", "Release", env)
    assert (layout.output_dir / "lib" / archive).read_text() == "source outputs"
    assert (layout.output_dir / "lib/netcoreapp/System.Private.CoreLib.dll").read_text() == "target corelib"
    assert all(marker.is_file() for marker in (layout.clone_marker, layout.built_marker, layout.ready_marker))


@pytest.mark.skipif(shutil.which("cmake") is None, reason="CMake is required")
@pytest.mark.parametrize("target", TARGETS)
def test_fixture_archives_match_the_real_link_inputs(tmp_path: Path, target: str) -> None:
    stage = (ENGINE_ROOT / "BuildTools/cmake/stages/ThirdParty.cmake").read_text(encoding="utf-8")
    start = stage.rindex("    if(FO_WEB)\n", 0, stage.index("        SetValue(FO_MANAGED_SHIM_LIBS System.Native)"))
    end = stage.index("    AddCustomCommand(OUTPUT ${FO_DOTNET_DIR}/${FO_MONO_READY_MARKER}", start)
    flags = {"windows": "FO_WINDOWS", "linux": "FO_LINUX", "browser": "FO_WEB",
             "android": "FO_ANDROID", "osx": "FO_MAC", "ios": "FO_IOS", "iossimulator": "FO_IOS"}
    prefix, suffix = ("", ".lib") if target == "windows" else ("lib", ".a")
    result_path = tmp_path / "archives.txt"
    script = tmp_path / "consumer.cmake"
    script.write_text(
        "macro(SetValue name)\n  set(${name} ${ARGN})\nendmacro()\n"
        "macro(AppendList name)\n  list(APPEND ${name} ${ARGN})\nendmacro()\n"
        "macro(AddLinkOptionsList)\nendmacro()\n"
        f"set({flags[target]} ON)\nset(FO_MANAGED_RUNTIME_DIR /fixture)\n"
        f"set(CMAKE_STATIC_LIBRARY_PREFIX {prefix})\nset(CMAKE_STATIC_LIBRARY_SUFFIX {suffix})\n"
        + stage[start:end] + f'\nfile(WRITE "{result_path.as_posix()}" "${{FO_MANAGED_RUNTIME_ARCHIVES}}")\n',
        encoding="utf-8",
    )
    result = subprocess.run([shutil.which("cmake"), "-P", str(script)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    actual = {path.removeprefix("/fixture/") for path in result_path.read_text().split(";")}
    expected = {path for path in published_runtime_files(target) if path.endswith((".a", ".lib"))}
    assert actual == expected
