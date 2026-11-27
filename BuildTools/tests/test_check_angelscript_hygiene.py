"""Tests for check_angelscript_hygiene."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS.parent
sys.path.insert(0, str(BUILDTOOLS))
import check_angelscript_hygiene as hygiene  # noqa: E402


CHECKER = BUILDTOOLS / "check_angelscript_hygiene.py"
GOOD = "namespace Good\n{\n\n#if SERVER\nvoid Run()\n{\n}\n#endif\n\n}\n"


def rules(rel: str, text: str | bytes, config: hygiene.HygieneConfig | None = None) -> list[tuple[str, int]]:
    raw = text if isinstance(text, bytes) else text.encode("utf-8")
    findings = hygiene.check_script(rel, raw, config or hygiene.HygieneConfig())
    return [(item.rule, item.line) for item in findings]


def project(tmp_path: Path, files: dict[str, str | bytes]) -> Path:
    root = tmp_path.resolve()

    for name, payload in files.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(payload if isinstance(payload, bytes) else payload.encode("utf-8"))

    return root


def run_cli(*args: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run([sys.executable, str(CHECKER), *(str(arg) for arg in args)],
                          capture_output=True, text=True, encoding="utf-8")


def test_conforming_module_passes() -> None:
    assert rules("Scripts/Good.fos", GOOD) == []
    assert rules("Scripts/Good.fos", GOOD.replace("\n", "\r\n")) == []


def test_namespace_must_match_the_file_stem() -> None:
    assert rules("Scripts/Good.fos", "namespace Other\n{\n}\n") == [("namespace-matches-file", 1)]
    assert rules("Scripts/Good.fos", "// namespace Other\nconst int A = 1;\n") == [("namespace-matches-file", 1)]
    assert rules("Scripts/Good.fos", "namespace Good\n{\nnamespace Inner\n{\n}\n}\n") == []


def test_namespace_exception_and_optional_namespace_come_from_config(tmp_path: Path) -> None:
    config_path = tmp_path.resolve() / "hygiene.json"
    config_path.write_text(json.dumps({
        "schema_version": 1,
        "rules": {"namespace-matches-file": {
            "require_namespace": False,
            "exceptions": [{"path": "Scripts/ColorExt.fos", "namespace": "Color",
                            "reason": "Extends the engine Color namespace"}],
        }},
    }), encoding="utf-8")
    config = hygiene.load_config(config_path)

    assert rules("Scripts/ColorExt.fos", "namespace Color\n{\n}\n", config) == []
    assert rules("Scripts/ColorExt.fos", "namespace ColorExt\n{\n}\n", config) == [("namespace-matches-file", 1)]
    assert rules("Scripts/Data.fos", "///@ Enum Data A = 1\n", config) == []


def test_include_is_rejected_only_in_code() -> None:
    text = "namespace Good\n{\n#include \"Other.fos\"\n// #include \"Comment.fos\"\nstring S = \"#include\";\n}\n"

    assert rules("Scripts/Good.fos", text) == [("no-include", 3)]


@pytest.mark.parametrize("directive", ["#ifdef SERVER", "#ifndef CLIENT", "#if defined(MAPPER)",
                                       "#if !defined SERVER", "#  ifdef SERVER"])
def test_side_macros_need_a_value_test(directive: str) -> None:
    text = f"namespace Good\n{{\n{directive}\n#endif\n}}\n"

    assert rules("Scripts/Good.fos", text) == [("side-macro-value-test", 3)]


def test_other_macros_and_value_tests_are_accepted() -> None:
    text = "namespace Good\n{\n#ifdef DEBUG_TRACE\n#endif\n#if SERVER || CLIENT\n#elif !MAPPER\n#endif\n}\n"

    assert rules("Scripts/Good.fos", text) == []


def test_preprocessor_blocks_must_balance() -> None:
    text = "namespace Good\n{\n#endif\n#else\n#if SERVER\n}\n"

    assert rules("Scripts/Good.fos", text) == [
        ("preprocessor-balance", 3), ("preprocessor-balance", 4), ("preprocessor-balance", 5)]


def test_sort_directive_belongs_to_the_first_line_with_an_integer() -> None:
    assert rules("Scripts/Good.fos", "// Sort 10\n" + GOOD) == []
    assert rules("Scripts/Good.fos", "// Sort -5 keeps order\n" + GOOD) == []
    assert rules("Scripts/Good.fos", "// Sort by distance\n" + GOOD) == [("sort-directive", 1)]
    assert rules("Scripts/Good.fos", GOOD.replace("{\n\n", "{\n// Sort 3\n", 1)) == [("sort-directive", 3)]


def test_file_ending_rejects_bom_missing_and_extra_line_endings() -> None:
    assert rules("Scripts/Good.fos", b"\xef\xbb\xbf" + GOOD.encode()) == [("file-ending", 1)]
    assert rules("Scripts/Good.fos", GOOD.rstrip("\n")) == [("file-ending", 10)]
    assert rules("Scripts/Good.fos", GOOD + "\n") == [("file-ending", 12)]
    assert rules("Scripts/Good.fos", b"\xff\xfe") == [("file-ending", 1)]


def test_optional_rules_are_off_until_enabled() -> None:
    text = "namespace Good\n{\n// Run(1, 2);\n// return value;\n///@ Event Server Game OnX()\n}\n"
    config = hygiene.HygieneConfig()

    assert rules("Scripts/Good.fos", text, config) == []

    config.enabled["commented-out-code"] = True
    config.enabled["file-size"] = True
    config.max_lines = 5

    assert rules("Scripts/Good.fos", text, config) == [
        ("commented-out-code", 3), ("commented-out-code", 4), ("file-size", 1)]


def test_every_default_rule_can_be_disabled(tmp_path: Path) -> None:
    config_path = tmp_path.resolve() / "hygiene.json"
    config_path.write_text(json.dumps({
        "schema_version": 1,
        "rules": {rule: {"enabled": False} for rule in hygiene.RULES},
    }), encoding="utf-8")
    text = "\xef\xbb\xbfnamespace Bad\n{\n#include \"X.fos\"\n#ifdef SERVER\n// Sort 2\n"

    assert rules("Scripts/Good.fos", text.encode("latin-1"), hygiene.load_config(config_path)) == []


@pytest.mark.parametrize(("config", "message"), [
    ({"rules": {}}, "schema_version"),
    ({"schema_version": 1, "rules": {"tla-naming": {"enabled": True}}}, "unknown rule"),
    ({"schema_version": 1, "rules": {"no-include": {"enabled": "yes"}}}, "true or false"),
    ({"schema_version": 1, "rules": {"no-include": {"max_lines": 3}}}, "unknown options"),
    ({"schema_version": 1, "rules": {"file-size": {"max_lines": 0}}}, "positive integer"),
    ({"schema_version": 1, "rules": {"namespace-matches-file": {"exceptions": [{"path": "A.fos", "namespace": "B"}]}}},
     "path, namespace and reason"),
    ({"schema_version": 1, "extra": 1}, "unknown configuration keys"),
])
def test_invalid_configuration_is_rejected(tmp_path: Path, config: dict, message: str) -> None:
    config_path = tmp_path.resolve() / "hygiene.json"
    config_path.write_text(json.dumps(config), encoding="utf-8")

    with pytest.raises(hygiene.ConfigError, match=message):
        hygiene.load_config(config_path)


def test_engine_examples_pass_with_default_rules() -> None:
    findings, errors = hygiene.check_hygiene(ENGINE_ROOT, ["Examples"], hygiene.HygieneConfig())

    assert errors == []
    assert findings == [], [hygiene.format_finding(item) for item in findings]


def test_cli_ratchet_excludes_and_exit_codes(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Scripts/Good.fos": GOOD,
        "Scripts/Legacy.fos": "namespace Old\n{\n#ifdef SERVER\n#endif\n}\n",
        "Scripts/Generated/Screens.fos": "namespace Wrong\n{\n}\n",
    })
    config_path = root / "hygiene.json"
    config_path.write_text(json.dumps({"schema_version": 1, "exclude": ["Scripts/Generated/*"]}), encoding="utf-8")
    common = ("--root", root, "--scripts", "Scripts", "--config", config_path)

    failing = run_cli(*common)

    assert failing.returncode == 1
    assert "Scripts/Legacy.fos:1: [namespace-matches-file]" in failing.stdout
    assert "Scripts/Legacy.fos:3: [side-macro-value-test]" in failing.stdout
    assert "Generated" not in failing.stdout

    draft = root / "hygiene.tsv"

    assert run_cli(*common, "--write-baseline", draft).returncode == 0
    assert run_cli(*common, "--baseline", draft).returncode == 2

    draft.write_text(draft.read_text(encoding="utf-8").replace("\t1\t\n", "\t1\tLegacy module awaiting its rename\n"),
                     encoding="utf-8")

    assert run_cli(*common, "--baseline", draft).returncode == 0

    (root / "Scripts/Legacy.fos").write_text("namespace Old\n{\n#ifdef SERVER\n#endif\n#ifdef CLIENT\n#endif\n}\n",
                                             encoding="utf-8")
    regressed = run_cli(*common, "--baseline", draft)

    assert regressed.returncode == 1
    assert "Scripts/Legacy.fos:5: [side-macro-value-test]" in regressed.stdout

    assert run_cli("--root", root, "--scripts", "Missing").returncode == 2
    assert run_cli("--root", root, "--scripts", "Scripts", "--config", root / "absent.json").returncode == 2


def test_cli_lists_rules_with_defaults() -> None:
    listed = run_cli("--list-rules")

    assert listed.returncode == 0
    assert [line.split("\t")[:2] for line in listed.stdout.splitlines()] == [
        [rule, "on" if default else "off"] for rule, (default, _) in hygiene.RULES.items()]
