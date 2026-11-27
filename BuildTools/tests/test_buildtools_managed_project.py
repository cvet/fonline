"""managed-project-path: the generated managed project location from the settings ManagedScriptBaker reads."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
BUILDTOOLS_PY = BUILDTOOLS_DIR / "buildtools.py"

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402
import foconfig  # noqa: E402


FIXTURE_CONFIG = "\n".join([
    "# Fixture main config",
    "Common.GameName = Fixture",
    "ManagedScript.ProjectName = Fixture",
    "ManagedScript.GeneratedDir = Generated/Managed",
    "",
    "[SubConfig]",
    "Name = BuildTree",
    "ManagedScript.GeneratedDir =",
    "",
    "[SubConfig]",
    "Name = Renamed",
    "ManagedScript.ProjectName = Renamed",
    "",
    "[SubConfig]",
    "Name = Combined",
    "Parent = Renamed BuildTree",
    "",
])


def write_config(root: Path, text: str = FIXTURE_CONFIG, name: str = "Fixture.fomain") -> Path:
    path = root / "Project" / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def run_command(*argv: str, launch=None) -> int:
    args = buildtools.create_parser().parse_args(["managed-project-path", *argv])

    if launch is None:
        return buildtools.managed_project_path(args, launch=_forbidden_launch)

    return buildtools.managed_project_path(args, launch=launch)


def _forbidden_launch(path: Path) -> None:
    raise AssertionError(f"nothing may be launched without --open: {path}")


@pytest.fixture
def root(tmp_path: Path) -> Path:
    return tmp_path.resolve()


def test_relative_generated_dir_is_rooted_at_the_config(root: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    config = write_config(root)
    monkeypatch.chdir(root)

    paths = buildtools.resolve_managed_project_paths(config, None, None)

    generated = (root / "Project" / "Generated" / "Managed").resolve()
    assert paths == buildtools.ManagedProjectPaths(
        generated,
        generated / "Fixture.gen.csproj",
        generated / "Fixture.gen.sln",
        generated / "FOnline.ManagedHost.gen.csproj",
    )


def test_absolute_generated_dir_is_used_as_written(root: Path) -> None:
    target = (root / "Elsewhere").resolve()
    config = write_config(root, f"ManagedScript.GeneratedDir = {target}\n")

    paths = buildtools.resolve_managed_project_paths(config, None, None)

    assert paths.generated_dir == target
    assert paths.project.name == "FOnline.gen.csproj"


def test_empty_generated_dir_needs_the_build_tree(root: Path, capsys: pytest.CaptureFixture[str]) -> None:
    config = write_config(root)

    assert run_command(str(config), "--sub-config", "BuildTree") == 2
    assert "pass --build-dir" in capsys.readouterr().err

    paths = buildtools.resolve_managed_project_paths(config, "BuildTree", root / "Build")
    assert paths.generated_dir == (root / "Build" / "GeneratedSource" / "Managed").resolve()
    assert paths.solution.name == "Fixture.gen.sln"


def test_sub_config_parents_and_own_values_apply_in_order(root: Path) -> None:
    config = write_config(root)

    paths = buildtools.resolve_managed_project_paths(config, "Combined", root / "Build")

    assert paths.generated_dir == (root / "Build" / "GeneratedSource" / "Managed").resolve()
    assert paths.project.name == "Renamed.gen.csproj"
    assert buildtools.resolve_managed_project_paths(config, "NONE", None).project.name == "Fixture.gen.csproj"


@pytest.mark.parametrize(("text", "sub_config", "message"), [
    (FIXTURE_CONFIG, "Missing", "Sub config 'Missing' not found"),
    ("[SubConfig]\nName = Child\nParent = Later\n[SubConfig]\nName = Later\n", "Child", "Parent sub config 'Later'"),
    ("ManagedScript.ProjectName =\n", None, "ManagedScript.ProjectName setting is empty"),
    ("ManagedScript.GeneratedDir = $ENV{GENERATED}\n", None, "does not resolve"),
    ("ManagedScript.ProjectName += Suffix\n", None, "does not resolve"),
])
def test_unresolvable_configs_exit_2(root: Path, capsys: pytest.CaptureFixture[str], text: str, sub_config: str | None, message: str) -> None:
    config = write_config(root, text)
    options = ["--sub-config", sub_config] if sub_config else []

    assert run_command(str(config), "--build-dir", str(root / "Build"), *options) == 2
    assert message in capsys.readouterr().err


def test_missing_config_exits_2(root: Path) -> None:
    assert run_command(str(root / "Absent.fomain")) == 2


def test_text_output_lists_every_path(root: Path, capsys: pytest.CaptureFixture[str]) -> None:
    config = write_config(root)

    assert run_command(str(config)) == 0

    generated = (root / "Project" / "Generated" / "Managed").resolve()
    assert capsys.readouterr().out.splitlines() == [
        f"generated-dir: {generated}",
        f"project: {generated / 'Fixture.gen.csproj'}",
        f"solution: {generated / 'Fixture.gen.sln'}",
        f"host-project: {generated / 'FOnline.ManagedHost.gen.csproj'}",
    ]


def test_json_output_reports_existence(root: Path, capsys: pytest.CaptureFixture[str]) -> None:
    config = write_config(root)
    generated = root / "Project" / "Generated" / "Managed"
    generated.mkdir(parents=True)
    (generated / "Fixture.gen.csproj").write_text("<Project />\n", encoding="utf-8")

    assert run_command(str(config), "--json") == 0

    report = json.loads(capsys.readouterr().out)
    assert report["project"] == str(generated.resolve() / "Fixture.gen.csproj")
    assert (report["projectExists"], report["solutionExists"]) == (True, False)


def test_require_existing_fails_until_the_baker_ran(root: Path, capsys: pytest.CaptureFixture[str]) -> None:
    config = write_config(root)
    generated = root / "Project" / "Generated" / "Managed"

    assert run_command(str(config), "--require-existing") == 1
    assert "run CompileManagedScripts or BakeResources first" in capsys.readouterr().err

    generated.mkdir(parents=True)
    (generated / "Fixture.gen.csproj").write_text("<Project />\n", encoding="utf-8")
    (generated / "Fixture.gen.sln").write_text("\n", encoding="utf-8")

    assert run_command(str(config), "--require-existing") == 0


def test_open_launches_only_an_existing_solution(root: Path) -> None:
    config = write_config(root)
    launched: list[Path] = []

    assert run_command(str(config), "--open", launch=launched.append) == 1
    assert launched == []

    generated = root / "Project" / "Generated" / "Managed"
    generated.mkdir(parents=True)
    (generated / "Fixture.gen.csproj").write_text("<Project />\n", encoding="utf-8")
    (generated / "Fixture.gen.sln").write_text("\n", encoding="utf-8")

    assert run_command(str(config), "--open", launch=launched.append) == 0
    assert launched == [generated.resolve() / "Fixture.gen.sln"]


def test_open_failure_exits_1(root: Path, capsys: pytest.CaptureFixture[str]) -> None:
    config = write_config(root)
    generated = root / "Project" / "Generated" / "Managed"
    generated.mkdir(parents=True)
    (generated / "Fixture.gen.csproj").write_text("<Project />\n", encoding="utf-8")
    (generated / "Fixture.gen.sln").write_text("\n", encoding="utf-8")

    def missing_opener(path: Path) -> None:
        raise FileNotFoundError("xdg-open")

    assert run_command(str(config), "--open", launch=missing_opener) == 1
    assert "cannot open" in capsys.readouterr().err


@pytest.mark.parametrize(("os_name", "platform_name", "expected"), [
    ("posix", "linux", ["xdg-open"]),
    ("posix", "darwin", ["open"]),
])
def test_desktop_association_uses_the_platform_opener(monkeypatch: pytest.MonkeyPatch, os_name: str, platform_name: str, expected: list[str]) -> None:
    calls: list[list[str]] = []
    monkeypatch.setattr(buildtools.os, "name", os_name)
    monkeypatch.setattr(buildtools.sys, "platform", platform_name)
    monkeypatch.setattr(buildtools.subprocess, "run", lambda command, check: calls.append(command))

    buildtools.open_with_desktop_association(Path("Game.gen.sln"))

    assert calls == [[*expected, "Game.gen.sln"]]


def test_desktop_association_on_windows_uses_startfile(monkeypatch: pytest.MonkeyPatch) -> None:
    opened: list[str] = []
    monkeypatch.setattr(buildtools.os, "name", "nt")
    monkeypatch.setattr(buildtools.os, "startfile", opened.append, raising=False)

    buildtools.open_with_desktop_association(Path("Game.gen.sln"))

    assert opened == ["Game.gen.sln"]


def test_command_is_registered_and_prints_without_the_environment_summary(root: Path) -> None:
    config = write_config(root)
    result = subprocess.run([sys.executable, str(BUILDTOOLS_PY), "managed-project-path", str(config)], capture_output=True, text=True, check=False)

    assert result.returncode == 0, result.stderr
    assert result.stdout.splitlines()[0].startswith("generated-dir: ")
    assert "FO_PROJECT_ROOT" not in result.stdout


def test_foconfig_resolves_sub_config_inheritance() -> None:
    parser = foconfig.ConfigParser()
    parser.loadFromLines([
        "A = main\n",
        "B = main\n",
        "[SubConfig]\n",
        "Name = First\n",
        "A = first\n",
        "C = first\n",
        "[SubConfig]\n",
        "Name = Second\n",
        "C = second\n",
        "[SubConfig]\n",
        "Name = Child\n",
        "Parent = First Second\n",
        "B = child\n",
    ])

    assert parser.resolveSettings(None) == {"A": "main", "B": "main"}
    assert parser.resolve_settings("Child") == {"A": "first", "B": "child", "C": "second"}

    with pytest.raises(KeyError):
        parser.resolveSettings("Unknown")
