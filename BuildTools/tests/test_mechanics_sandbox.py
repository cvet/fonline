"""Check the mechanics sandbox example: the recipe manifest, the copied harness core, backend declaration parity,
the configuration and smoke manifest it relies on, and the README fences."""

from __future__ import annotations

import importlib.util
import json
import os
import re
import sys
from pathlib import Path

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]
SANDBOX_REL = "Examples/MechanicsSandbox"
SANDBOX = ENGINE_ROOT / SANDBOX_REL
RECIPES = SANDBOX / "recipes.json"
MAIN_CONFIG = SANDBOX / "FOnlineMechanicsSandbox.fomain"
SMOKE_MANIFEST = SANDBOX / "mechanics-smoke.json"
HARNESS_ORIGINAL = ENGINE_ROOT / "Examples/GameplayTestHarness/ScriptTests/AngelScript/Testing.fos"
HARNESS_COPY = SANDBOX / "Scripts/Testing.fos"
API_MODEL = ENGINE_ROOT / "Docs/generated/api.json"
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

import docs_snippets  # noqa: E402
import gameplay_test_runner  # noqa: E402


EXPECTED_RECIPES = [
    "persistent-state",
    "validated-action",
    "timed-effect",
    "data-driven-item",
    "equip-slot",
    "derived-stats",
    "container-transfer",
    "map-transition",
    "npc-patrol",
    "long-action",
]
EXPECTED_CROSS_CUTTING = ["cover-transactions"]
RECIPE_KEYS = {"slug", "title", "kind", "backends", "page", "files", "symbols", "metadata", "builtins", "smoke"}
CROSS_CUTTING_KEYS = RECIPE_KEYS - {"kind", "backends"}
FILE_KINDS = {"angelscript", "managed", "content", "maps", "tests"}
BACKEND_STATUSES = {"shape", "compile-verified", "runtime-verified"}
METADATA_RE = re.compile(r"^\s*(///@ .+?)\s*$", re.MULTILINE)
REGISTER_RE = re.compile(r'Testing::RegisterTest\("([a-z_]+)", "([a-z_]+)"')
SETTING_RE = re.compile(r"^\s*///@ Setting \w+ \w+ ([\w.]+)\s*$", re.MULTILINE)
HARNESS_FAILURE_MARKERS = ["[Testing] FAIL", "[Testing] ERROR", "[Testing] COMPLETE status=failed"]
SUB_CONFIGS = ["RecipeTests", "RecipeRestart", "RecipeNetworkTests", "RecipeClient"]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _manifest() -> dict:
    return json.loads(_read(RECIPES))


def _entries() -> list[dict]:
    manifest = _manifest()
    return [*manifest["recipes"], *manifest["cross_cutting"]]


def _sandbox_path(relative: str) -> Path:
    assert relative.startswith(SANDBOX_REL + "/"), relative
    return ENGINE_ROOT / relative


def _module(entry: dict) -> Path:
    return _sandbox_path(entry["files"]["angelscript"][0])


def _metadata(path: Path) -> list[str]:
    return METADATA_RE.findall(_read(path))


def _config_sections(text: str) -> tuple[dict[str, str], dict[str, dict[str, str]]]:
    root: dict[str, str] = {}
    sub_configs: dict[str, dict[str, str]] = {}
    current: dict[str, str] | None = root
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("["):
            current = {} if stripped == "[SubConfig]" else None
            continue
        if current is None:
            continue
        key, _, value = stripped.partition("=")
        key = key.strip()
        if key == "Name" and current is not root:
            sub_configs[value.strip()] = current
            continue
        current[key] = value.strip()
    return root, sub_configs


def test_recipe_manifest_lists_the_expected_recipes() -> None:
    manifest = _manifest()

    assert manifest["schema_version"] == 1
    assert manifest["sandbox"] == SANDBOX_REL
    assert [entry["slug"] for entry in manifest["recipes"]] == EXPECTED_RECIPES
    assert [entry["slug"] for entry in manifest["cross_cutting"]] == EXPECTED_CROSS_CUTTING

    for entry in manifest["recipes"]:
        assert set(entry) == RECIPE_KEYS, entry["slug"]
        assert entry["kind"] in {"primitive", "pattern"}
        assert set(entry["backends"]) == {"angelscript", "managed"}
        assert set(entry["backends"].values()) <= BACKEND_STATUSES
    for entry in manifest["cross_cutting"]:
        assert set(entry) == CROSS_CUTTING_KEYS, entry["slug"]

    for entry in _entries():
        assert entry["page"] == f"Docs/en/how-to/mechanics/{entry['slug']}.md"
        assert entry["title"].strip() == entry["title"] and entry["title"]
        assert all(isinstance(item, str) and item for item in entry["builtins"])


def test_every_recipe_has_existing_files_of_each_kind() -> None:
    for entry in _entries():
        assert set(entry["files"]) == FILE_KINDS, entry["slug"]
        for kind, paths in entry["files"].items():
            assert paths, (entry["slug"], kind)
            assert len(paths) == len(set(paths)), (entry["slug"], kind)
            for relative in paths:
                assert _sandbox_path(relative).is_file(), relative

        module = _module(entry)
        assert module.parent == SANDBOX / "Scripts/Recipes"
        assert entry["files"]["managed"][0] == f"{SANDBOX_REL}/Managed/Recipes/{module.stem}.cs"
        assert entry["files"]["tests"][0] == f"{SANDBOX_REL}/Scripts/Tests/Test_{module.stem}.fos"
        assert re.search(rf"^namespace {module.stem}$", _read(module), re.MULTILINE), module


def test_recipe_symbols_exist_in_the_api_model() -> None:
    known = {symbol["id"] for symbol in json.loads(_read(API_MODEL))["symbols"]}

    for entry in _entries():
        assert entry["symbols"], entry["slug"]
        assert len(entry["symbols"]) == len(set(entry["symbols"])), entry["slug"]
        missing = [symbol for symbol in entry["symbols"] if symbol not in known]
        assert not missing, (entry["slug"], missing)
        assert not any(symbol in known for symbol in entry["builtins"]), entry["slug"]


def test_manifest_metadata_is_the_recipe_module_declaration_list() -> None:
    for entry in _entries():
        assert entry["metadata"] == _metadata(_module(entry)), entry["slug"]


def test_managed_shapes_declare_the_same_metadata_as_angelscript() -> None:
    for entry in _entries():
        angelscript = _metadata(_module(entry))
        managed = _metadata(_sandbox_path(entry["files"]["managed"][0]))
        assert managed == angelscript, entry["slug"]


def test_harness_core_is_a_byte_identical_copy() -> None:
    # Git may check either file out with CRLF; the repository content is compared
    original = HARNESS_ORIGINAL.read_bytes().replace(b"\r\n", b"\n")
    copy = HARNESS_COPY.read_bytes().replace(b"\r\n", b"\n")
    assert copy == original


def test_recipes_do_not_reference_each_other() -> None:
    modules = [_module(entry) for entry in _entries()]
    for module in modules:
        text = _read(module)
        for other in modules:
            if other != module:
                assert f"{other.stem}::" not in text, (module.name, other.stem)


def test_config_defines_every_declared_setting_and_the_test_sub_configs() -> None:
    root, sub_configs = _config_sections(_read(MAIN_CONFIG))

    declared: set[str] = set()
    for script in (SANDBOX / "Scripts").rglob("*.fos"):
        declared.update(SETTING_RE.findall(_read(script)))
    assert declared, "no project settings found"
    assert declared <= set(root), sorted(declared - set(root))

    assert root["Testing.Enabled"] == "False"
    assert root["AngelScript.MutableGlobalsAllowedNamespaces"] == "Testing"
    assert list(sub_configs) == SUB_CONFIGS
    assert sub_configs["RecipeTests"]["Server.DbStorage"] == "Memory"
    assert sub_configs["RecipeRestart"]["Server.DbStorage"].startswith("JSON ")
    for name in ("RecipeTests", "RecipeRestart", "RecipeNetworkTests"):
        assert sub_configs[name]["Testing.Enabled"] == "True", name
    assert sub_configs["RecipeClient"]["Sandbox.ClientAutomation"] == "True"
    assert "FO_DATABASE_JSON ON" in _read(SANDBOX / "CMakeLists.txt")


def test_generated_config_is_current() -> None:
    name = "mechanics_sandbox_generate_config"
    spec = importlib.util.spec_from_file_location(name, SANDBOX / "generate_config.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)

    assert _read(MAIN_CONFIG) == module.render_config(ENGINE_ROOT)
    assert "generate_config.py" in _read(SANDBOX / "CMakeLists.txt")


def test_channel_key_is_a_local_ignored_test_identity() -> None:
    root, _ = _config_sections(_read(MAIN_CONFIG))
    ignored = _read(SANDBOX / ".gitignore").splitlines()

    assert root["ServerNetwork.ChannelSecretKey"] == "$TARGET_FILE{Workspace/SandboxChannel.key}"
    assert root["ClientNetwork.ChannelServerKeys"] == "$TARGET_FILE{Workspace/SandboxChannel.pub}"
    assert "/Workspace/" in ignored and "/Engine/" in ignored and "/Build/" in ignored


def test_smoke_manifest_covers_every_registered_test() -> None:
    manifest = gameplay_test_runner.load_manifest(SMOKE_MANIFEST)
    scenario_ids = [scenario["id"] for scenario in manifest["scenarios"]]

    for marker in ("FATAL ERROR!", "ScriptException", *HARNESS_FAILURE_MARKERS):
        assert marker in manifest["forbidden_markers"]

    for entry in _entries():
        assert entry["smoke"]["manifest"] == f"{SANDBOX_REL}/mechanics-smoke.json"
        assert set(entry["smoke"]["scenarios"]) <= set(scenario_ids), entry["slug"]

    registered: set[str] = set()
    for test_file in (SANDBOX / "Scripts/Tests").glob("Test_*.fos"):
        registered.update(f"{suite}.{name}" for suite, name in REGISTER_RE.findall(_read(test_file)))

    required: set[str] = set()
    for scenario in manifest["scenarios"]:
        server = scenario["processes"][0]
        assert server["id"] == "server"
        assert "[Testing] COMPLETE status=passed" in server["required_markers"], scenario["id"]
        for marker in server["required_markers"]:
            if marker.startswith("[Testing] PASS "):
                required.add(marker.removeprefix("[Testing] PASS "))

    assert required == registered


def test_restart_scenarios_share_one_runner_controlled_database() -> None:
    manifest = gameplay_test_runner.load_manifest(SMOKE_MANIFEST)
    scenario_ids = [scenario["id"] for scenario in manifest["scenarios"]]
    by_id = {scenario["id"]: scenario for scenario in manifest["scenarios"]}

    for entry in _entries():
        restart = [scenario for scenario in entry["smoke"]["scenarios"] if "-restart-" in scenario]
        if not restart:
            continue
        write_id = f"{entry['slug']}-restart-write"
        verify_id = f"{entry['slug']}-restart-verify"
        assert restart == [write_id, verify_id]
        assert scenario_ids.index(write_id) < scenario_ids.index(verify_id)
        commands = [by_id[scenario]["processes"][0]["command"] for scenario in restart]
        for command in commands:
            assert command[command.index("-ApplySubConfig") + 1] == "RecipeRestart"
            assert command[command.index("-Server.DbStorage") + 1] == f"JSON {{restart_db}}/{entry['slug']}"


def _authored_files() -> list[Path]:
    # The local Engine link and build trees are not part of the example and must not be walked
    files: list[Path] = []
    for directory, subdirectories, names in os.walk(SANDBOX):
        subdirectories[:] = sorted(name for name in subdirectories if name not in {"Engine", "Build", "__pycache__"})
        files.extend(Path(directory) / name for name in sorted(names))
    return files


def test_example_files_end_with_one_newline_and_no_bom() -> None:
    for path in _authored_files():
        if path.suffix in {".fos", ".cs", ".fopro", ".fomap", ".fomain", ".json", ".md", ".py", ".txt"}:
            raw = path.read_bytes().replace(b"\r\n", b"\n")
            assert not raw.startswith(b"\xef\xbb\xbf"), path
            assert raw.endswith(b"\n") and not raw.endswith(b"\n\n"), path


@pytest.mark.parametrize("readme", ["README.md", "README.ru.md"])
def test_readme_fences_parse(readme: str) -> None:
    text = _read(SANDBOX / readme).replace("\r\n", "\n")
    fences = re.findall(r"^```(\w*)\n(.*?)^```$", text, re.MULTILINE | re.DOTALL)
    assert fences

    for language, body in fences:
        assert language in docs_snippets.EXPECTED_LANGUAGES, language
        docs_snippets._validate_static(docs_snippets.EXPECTED_LANGUAGES[language][1], body)


@pytest.mark.parametrize("readme", ["README.md", "README.ru.md"])
def test_readme_shell_fences_pass_their_parsers(readme: str) -> None:
    try:
        docs_snippets._find_bash()
        docs_snippets._find_powershell()
    except ValueError as error:
        pytest.skip(str(error))

    text = _read(SANDBOX / readme).replace("\r\n", "\n")
    fences = re.findall(r"^```(bash|powershell)\n(.*?)^```$", text, re.MULTILINE | re.DOTALL)
    snippets = [
        {"harness": docs_snippets.EXPECTED_LANGUAGES[language][1], "_body": body, "path": readme, "start_line": index}
        for index, (language, body) in enumerate(fences)
    ]
    assert snippets
    assert docs_snippets._run_external(snippets) == []


def test_readmes_keep_the_same_fences() -> None:
    fence_re = re.compile(r"^```\w*\n.*?^```$", re.MULTILINE | re.DOTALL)
    english = fence_re.findall(_read(SANDBOX / "README.md").replace("\r\n", "\n"))
    russian = fence_re.findall(_read(SANDBOX / "README.ru.md").replace("\r\n", "\n"))
    assert english == russian
