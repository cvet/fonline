from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools as _buildtools  # noqa: E402


@pytest.mark.parametrize("platform_name", ["web", *_buildtools.ANDROID_ABI_BY_PLATFORM])
def test_debug_wrappers_generate_commands_accepted_by_the_actual_packager(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, platform_name: str,
) -> None:
    project = tmp_path / "project with spaces"
    project.mkdir()
    (project / "Sample.fomain").write_text("Common.GameName = Sample Game\n", encoding="utf-8")
    env = {
        "FO_PROJECT_ROOT": str(project),
        "FO_ENGINE_ROOT": str(BUILDTOOLS_DIR.parent),
        "FO_OUTPUT": str(tmp_path / "output with spaces"),
        "FO_WORKSPACE": str(tmp_path / "workspace with spaces"),
    }
    monkeypatch.setattr(_buildtools, "resolve_build_hash", lambda _env: "fixture-hash")
    parsed = []

    def parse_command(command):
        result = subprocess.run(
            [str(command[0]), "-c", "import json, package; print(json.dumps(vars(package.parse_args())))",
             *map(str, command[2:])],
            cwd=BUILDTOOLS_DIR, capture_output=True, text=True, timeout=30,
        )
        assert result.returncode == 0, result.stderr
        parsed.append(json.loads(result.stdout))

    monkeypatch.setattr(_buildtools, "run", parse_command)
    configs = ["LocalTest", "RemoteSceneLaunch", "Staging"]
    if platform_name == "web":
        _buildtools.package_web_debug(env, "Sample", configs)
    else:
        _buildtools.package_android_debug(env, "Sample", platform_name, configs)

    assert [args["config"] for args in parsed] == configs
    for args in parsed:
        assert args["target"] == "Client"
        assert args["resource_pack_compress_level"] == 1
        assert args["bundle_compress_level"] is None
        assert args["input"] == [env["FO_OUTPUT"], str(project)]
        assert args["maincfg"] == str(project / "Sample.fomain")
        assert args["nicename"] == "Sample Game"
        assert args["platform"] == ("Web" if platform_name == "web" else "Android")
        assert args["arch"] == ("wasm" if platform_name == "web" else _buildtools.ANDROID_ABI_BY_PLATFORM[platform_name])
        assert args["pack"] == ("Raw+WebServer" if platform_name == "web" else "Raw")
