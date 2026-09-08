from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


def test_windows_runtime_properties_pass_powershell_parameter_binding(tmp_path, monkeypatch):
    powershell = shutil.which("powershell") or shutil.which("pwsh")
    if powershell is None:
        pytest.skip("PowerShell is required for actual runtime parameter binding")
    runtime = tmp_path / "runtime with spaces"
    runtime.mkdir()
    script = runtime / "build.ps1"
    # These conflicting names and remaining-argument binding match dotnet/runtime eng/build.ps1
    script.write_text(
        "[CmdletBinding(PositionalBinding=$false)]\n"
        "Param([switch]$pack, [switch]$pgoinstrument,\n"
        " [Parameter(ValueFromRemainingArguments=$true)][String[]]$properties)\n"
        "ConvertTo-Json -Compress -InputObject @($properties)\n",
        encoding="utf-8",
    )

    def bind(arguments):
        command = "$ErrorView='NormalView'; & '" + str(script).replace("'", "''") + "' " + " ".join(arguments)
        return subprocess.run(
            [powershell, "-NoProfile", "-NonInteractive", "-Command", command],
            cwd=runtime, capture_output=True, text=True, timeout=30,
        )

    negative = bind(["-p:UseSharedCompilation=false"])
    assert negative.returncode != 0
    assert "AmbiguousParameter" in negative.stdout + negative.stderr

    observed = []

    def run_windows_command(command, *, cwd, env):
        assert command[:3] == ["cmd", "/d", "/c"]
        assert Path(command[3]).is_file()
        assert cwd == runtime
        result = bind(command[4:])
        assert result.returncode == 0, result.stdout + result.stderr
        observed.extend(json.loads(result.stdout))

    # Only cmd.exe dispatch is adapted on non-Windows hosts; PowerShell parses the actual build arguments
    monkeypatch.setattr(buildtools, "os", SimpleNamespace(name="nt", environ=os.environ))
    monkeypatch.setattr(buildtools, "resolve_visual_studio_2022_dev_cmd", lambda: None)
    monkeypatch.setattr(buildtools, "run", run_windows_command)
    buildtools.run_runtime_build(["/p:ExistingProperty=preserved"], runtime)
    assert observed == ["/p:ExistingProperty=preserved", "/p:UseSharedCompilation=false"]
