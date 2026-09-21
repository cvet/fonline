from __future__ import annotations

from pathlib import Path
import sys


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


def make_bootstrap(runtime_root: Path, *, sdk: bool, shared: bool) -> Path:
    bootstrap_root = runtime_root / ".dotnet"
    if sdk:
        (bootstrap_root / "sdk" / "10.0.109").mkdir(parents=True)
        (bootstrap_root / "sdk" / "10.0.109" / "dotnet.dll").write_bytes(b"")
    if shared:
        (bootstrap_root / "shared" / "Microsoft.NETCore.App" / "10.0.9").mkdir(parents=True)
    return bootstrap_root


def test_sdk_without_its_shared_runtime_is_removed(tmp_path):
    bootstrap_root = make_bootstrap(tmp_path / "runtime", sdk=True, shared=False)

    buildtools.remove_incomplete_runtime_bootstrap(tmp_path / "runtime")

    assert not bootstrap_root.exists()


def test_complete_bootstrap_is_kept(tmp_path):
    bootstrap_root = make_bootstrap(tmp_path / "runtime", sdk=True, shared=True)

    buildtools.remove_incomplete_runtime_bootstrap(tmp_path / "runtime")

    assert (bootstrap_root / "sdk" / "10.0.109" / "dotnet.dll").is_file()


def test_tree_without_an_sdk_is_left_for_the_installer(tmp_path):
    bootstrap_root = make_bootstrap(tmp_path / "runtime", sdk=False, shared=True)
    (bootstrap_root / "sdk").mkdir()

    buildtools.remove_incomplete_runtime_bootstrap(tmp_path / "runtime")
    buildtools.remove_incomplete_runtime_bootstrap(tmp_path / "absent")

    assert (bootstrap_root / "shared" / "Microsoft.NETCore.App" / "10.0.9").is_dir()
