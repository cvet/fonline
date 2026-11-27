from __future__ import annotations

import json
import subprocess
import sys
from collections.abc import Callable
from pathlib import Path

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

import mechanics_recipes  # noqa: E402


SANDBOX = "Examples/MechanicsSandbox"
RECIPE_SCRIPT = f"{SANDBOX}/Scripts/Recipes/PersistentState.fos"
RECIPE_MANAGED = f"{SANDBOX}/Managed/Recipes/PersistentState.cs"
RECIPE_CONTENT = f"{SANDBOX}/Content/PersistentState.fopro"
RECIPE_TEST = f"{SANDBOX}/Scripts/Tests/Test_PersistentState.fos"
COVER_SCRIPT = f"{SANDBOX}/Scripts/Recipes/CoverTransactions.fos"
SMOKE = f"{SANDBOX}/mechanics-smoke.json"
MANIFEST = f"{SANDBOX}/recipes.json"
RECIPE_PAGE = "Docs/en/how-to/mechanics/persistent-state.md"
RECIPE_MIRROR = "Docs/ru/how-to/mechanics/persistent-state.md"
COVER_PAGE = "Docs/en/how-to/mechanics/cover-transactions.md"
COVER_MIRROR = "Docs/ru/how-to/mechanics/cover-transactions.md"
HUB = "Docs/en/how-to/mechanics/index.md"
HUB_MIRROR = "Docs/ru/how-to/mechanics/index.md"

SCRIPT_TEXT = """namespace RecipePersistentState
{

///@ Property Critter Server int32 SampleCounter Mutable Persistent

#if SERVER

[[ModuleInit]]
void InitPersistentState()
{
    Game.OnCritterInit.Subscribe(OnCritterInit);
}

[[Event]]
void OnCritterInit(Critter critter, bool firstTime)
{
    if (firstTime) {
        critter.SampleCounter = 1;
    }
}

#endif

}
"""

MANAGED_TEXT = """namespace RecipePersistentState;

public static class PersistentState
{
    [ModuleInit]
    internal static void InitPersistentState()
    {
        Game.OnCritterInit.Subscribe(OnCritterInit);
    }
}
"""

CONTENT_TEXT = """[ProtoCritter]
$Name = SampleCritter
SampleCounter = 0
"""

TEST_TEXT = """namespace TestPersistentState
{

[[Test]]
void CounterStartsAtOne()
{
}

}
"""

COVER_TEXT = """namespace RecipeCoverTransactions
{

[[Async]]
void MoveBetween(Critter first, Critter second)
{
    Game.Sync(first, second);
}

}
"""

RECIPE_SECTIONS_RU = (
    "Проверенные исходные пути",
    "Задача",
    "Используемые примитивы Engine",
    "Минимальный пример",
    "Пошагово",
    "Полномочия и синхронизация",
    "Сохранение и миграция",
    "Ошибки и антипаттерны",
    "Маршрут проверки",
    "Managed C#",
    "Как это расширяет большой проект",
    "См. также",
)


def _api_model() -> dict[str, object]:
    def symbol(identifier: str, kind: str, name: str, description: str, family: str | None = None) -> dict[str, object]:
        return {"id": identifier, "family_id": family or identifier, "kind": kind, "name": name, "description": description}

    return {
        "schema_version": 2,
        "symbols": [
            symbol("script.event.server.Game.OnCritterInit", "event", "OnCritterInit", "Fires after a critter is initialized"),
            symbol("script.method.server.Entity.IsPersistent#0123456789abcdef", "method", "IsPersistent", "Returns persistence",
                   "script.method.server.Entity.IsPersistent"),
            symbol("script.method.server.Game.Sync", "method", "Sync", "Covers entities"),
        ],
    }


def _manifest() -> dict[str, object]:
    return {
        "schema_version": 1,
        "sandbox": SANDBOX,
        "recipes": [
            {
                "slug": "persistent-state",
                "title": "Custom persistent character state",
                "kind": "primitive",
                "backends": {"angelscript": "runtime-verified", "managed": "shape"},
                "page": RECIPE_PAGE,
                "files": {
                    "angelscript": [RECIPE_SCRIPT],
                    "managed": [RECIPE_MANAGED],
                    "content": [RECIPE_CONTENT],
                    "tests": [RECIPE_TEST],
                },
                "symbols": ["script.event.server.Game.OnCritterInit", "script.method.server.Entity.IsPersistent"],
                "builtins": ["Subscribe"],
                "smoke": {"manifest": SMOKE, "scenarios": ["persistent-state", "persistent-state-restart"]},
            }
        ],
        "cross_cutting": [
            {
                "slug": "cover-transactions",
                "page": COVER_PAGE,
                "files": {"angelscript": [COVER_SCRIPT]},
                "symbols": ["script.method.server.Game.Sync"],
            }
        ],
    }


def _smoke() -> dict[str, object]:
    def scenario(identifier: str) -> dict[str, object]:
        return {
            "id": identifier,
            "processes": [{"id": "server", "command": ["{server}"], "required_markers": ["[Testing] COMPLETE status=passed"]}],
        }

    return {
        "schema_version": 1,
        "name": "mechanics-smoke",
        "default_timeout_seconds": 60,
        "scenarios": [scenario("persistent-state"), scenario("persistent-state-restart")],
    }


def _recipe_page(russian: bool = False) -> str:
    sections = RECIPE_SECTIONS_RU if russian else mechanics_recipes.RECIPE_SECTIONS
    locale = "ru" if russian else "en"
    title = "Собственное сохраняемое состояние персонажа" if russian else "Custom Persistent Character State"
    filler = [f"{'Строка' if russian else 'Line'} {index}." for index in range(110)]
    body = {
        0: [f"- `{RECIPE_SCRIPT}`", f"- `{RECIPE_MANAGED}`"],
        1: ["A counter that survives a restart."],
        2: [
            "| Symbol | Side | Why |",
            "| --- | --- | --- |",
            "| `script.event.server.Game.OnCritterInit` | server | sets the first value |",
            "| `script.method.server.Entity.IsPersistent` | server | checks the record |",
        ],
        3: ["```angelscript", "///@ Property Critter Server int32 SampleCounter Mutable Persistent", "```"],
        4: [
            "1. Subscribe on module init:",
            "",
            "   ```angelscript",
            "   [[ModuleInit]]",
            "   void InitPersistentState()",
            "   {",
            "       Game.OnCritterInit.Subscribe(OnCritterInit);",
            "   }",
            "   ```",
            "",
            "2. Author the default:",
            "",
            "```ini",
            "[ProtoCritter]",
            "$Name = SampleCritter",
            "```",
        ],
        5: ["The server owns the value."],
        6: ["The property is `Persistent`."],
        7: ["- first pitfall", "- second pitfall", "- third pitfall"],
        8: ["```bash", "python3 validate.py", "```"],
        9: [
            "```csharp",
            "    [ModuleInit]",
            "    internal static void InitPersistentState()",
            "    {",
            "        Game.OnCritterInit.Subscribe(OnCritterInit);",
            "    }",
            "```",
        ],
        10: filler,
        11: ["- [Hub](index.md)"],
    }
    lines = [
        "---",
        "layout: default",
        f"title: {title}",
        f"locale: {locale}",
        "document_id: recipe-persistent-state",
        f"permalink: /Docs/{locale}/how-to/mechanics/persistent-state.html",
        "recipe_kind: primitive",
        "---",
        "",
        f"# {title}",
        "",
        "| Backend | Status |",
        "| --- | --- |",
        "| AngelScript | `runtime-verified` |",
        "| Managed C# | `shape` |",
    ]
    for index, section in enumerate(sections):
        lines += ["", f"## {section}", "", *body[index]]
    return "\n".join(lines) + "\n"


def _cover_page(russian: bool = False) -> str:
    locale = "ru" if russian else "en"
    title = "Захват нескольких сущностей" if russian else "Lock Several Entities for One Action"
    lines = [
        "---",
        "layout: default",
        f"title: {title}",
        f"locale: {locale}",
        "document_id: mechanics-cover-transactions",
        f"permalink: /Docs/{locale}/how-to/mechanics/cover-transactions.html",
        "---",
        "",
        f"# {title}",
        "",
        "## Проверенные исходные пути" if russian else "## Source paths inspected",
        "",
        f"- `{COVER_SCRIPT}`",
        "",
        "## Шаги" if russian else "## Steps",
        "",
        "`script.method.server.Game.Sync` covers both critters.",
        "",
        "```angelscript",
        "    Game.Sync(first, second);",
        "```",
        "",
        *[f"Line {index}." for index in range(140)],
    ]
    return "\n".join(lines) + "\n"


def _hub(russian: bool = False) -> str:
    locale = "ru" if russian else "en"
    title = "Создание игровых механик" if russian else "Build Game Mechanics"
    header = "| Рецепт | Вид | AngelScript | Managed C# |" if russian else "| Recipe | Kind | AngelScript | Managed C# |"
    lines = [
        "---",
        "layout: default",
        f"title: {title}",
        f"locale: {locale}",
        "document_id: mechanics-recipes",
        f"permalink: /Docs/{locale}/how-to/mechanics/",
        "---",
        "",
        f"# {title}",
        "",
        "## Проверенные исходные пути" if russian else "## Source paths inspected",
        "",
        f"- `{MANIFEST}`",
        "",
        "## Рецепты" if russian else "## Recipes",
        "",
        header,
        "| --- | --- | --- | --- |",
        "| [Persistent state](persistent-state.md) | `primitive` | `runtime-verified` | `shape` |",
        "",
        "- [Cover transactions](cover-transactions.md)",
    ]
    return "\n".join(lines) + "\n"


def _write(root: Path, relative: str, text: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def _write_json(root: Path, relative: str, value: object) -> None:
    _write(root, relative, json.dumps(value, indent=2) + "\n")


def _replace(root: Path, relative: str, old: str, new: str) -> None:
    path = root / relative
    text = path.read_text(encoding="utf-8")
    assert old in text, (relative, old)
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


def _edit_manifest(root: Path, change: Callable[[dict], None]) -> None:
    manifest = json.loads((root / MANIFEST).read_text(encoding="utf-8"))
    change(manifest)
    _write_json(root, MANIFEST, manifest)


@pytest.fixture
def tree(tmp_path: Path) -> Path:
    root = tmp_path.resolve()
    _write_json(root, mechanics_recipes.DEFAULT_API_MODEL, _api_model())
    _write_json(root, MANIFEST, _manifest())
    _write_json(root, SMOKE, _smoke())
    _write(root, RECIPE_SCRIPT, SCRIPT_TEXT)
    _write(root, RECIPE_MANAGED, MANAGED_TEXT)
    _write(root, RECIPE_CONTENT, CONTENT_TEXT)
    _write(root, RECIPE_TEST, TEST_TEXT)
    _write(root, COVER_SCRIPT, COVER_TEXT)
    _write(root, RECIPE_PAGE, _recipe_page())
    _write(root, RECIPE_MIRROR, _recipe_page(russian=True))
    _write(root, COVER_PAGE, _cover_page())
    _write(root, COVER_MIRROR, _cover_page(russian=True))
    _write(root, HUB, _hub())
    _write(root, HUB_MIRROR, _hub(russian=True))
    return root


def _findings(root: Path, severity: str = "error") -> list[mechanics_recipes.Finding]:
    return [finding for finding in mechanics_recipes.validate(root) if finding.severity == severity]


def _assert_single(root: Path, check: str, fragment: str) -> None:
    findings = _findings(root)
    assert [finding.check for finding in findings] == [check], [finding.render() for finding in findings]
    assert fragment in findings[0].message, findings[0].render()


def test_fixture_tree_passes_without_findings(tree: Path) -> None:
    assert mechanics_recipes.validate(tree) == []


def test_cli_reports_success_failure_and_configuration_errors(tree: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert mechanics_recipes.main(["--root", str(tree), "--check"]) == 0
    assert "0 error(s), 0 warning(s)" in capsys.readouterr().out

    _replace(tree, RECIPE_PAGE, "- third pitfall\n", "")
    assert mechanics_recipes.main(["--root", str(tree), "--check"]) == 1
    assert "error[page] Docs/en/how-to/mechanics/persistent-state.md" in capsys.readouterr().out

    assert mechanics_recipes.main(["--root", str(tree), "--list"]) == 0
    listed = capsys.readouterr().out
    assert "persistent-state\tprimitive\tangelscript=runtime-verified managed=shape" in listed
    assert "cover-transactions\tcross-cutting" in listed

    (tree / MANIFEST).unlink()
    assert mechanics_recipes.main(["--root", str(tree), "--check"]) == 2
    assert "recipe manifest not found" in capsys.readouterr().err


def test_cli_requires_one_action() -> None:
    with pytest.raises(SystemExit):
        mechanics_recipes.create_parser().parse_args([])
    with pytest.raises(SystemExit):
        mechanics_recipes.create_parser().parse_args(["--check", "--list"])


def test_missing_api_model_is_a_configuration_error(tree: Path) -> None:
    (tree / mechanics_recipes.DEFAULT_API_MODEL).unlink()
    with pytest.raises(mechanics_recipes.ConfigurationError, match="API model"):
        mechanics_recipes.validate(tree)


# (a) Manifest structure and schema version


@pytest.mark.parametrize(
    ("change", "fragment"),
    [
        (lambda manifest: manifest.update(schema_version=2), "schema_version must be 1"),
        (lambda manifest: manifest.update(schema_version=True), "schema_version must be 1"),
        (lambda manifest: manifest.update(extra=1), "unknown field(s): extra"),
        (lambda manifest: manifest.update(recipes=[]), "recipes must be a non-empty array"),
        (lambda manifest: manifest.update(sandbox="Examples/Other"), "sandbox must name the manifest directory"),
        (lambda manifest: manifest["recipes"][0].update(kind="engine-primitive"), "kind must be one of primitive, pattern"),
        (lambda manifest: manifest["recipes"][0]["backends"].update(managed="verified"), "backends.managed must be one of"),
        (lambda manifest: manifest["recipes"][0]["backends"].pop("managed"), "backends must name exactly angelscript, managed"),
        (lambda manifest: manifest["recipes"][0].pop("smoke"), "is missing field(s): smoke"),
        (lambda manifest: manifest["recipes"][0].update(slug="Persistent_State"), "slug must be a lowercase kebab-case string"),
        (lambda manifest: manifest["recipes"][0].update(page="Docs/en/how-to/other.md"), "page must be Docs/en/how-to/mechanics/persistent-state.md"),
        (lambda manifest: manifest["recipes"][0]["files"].update(scripts=[RECIPE_SCRIPT]), "unknown categories: scripts"),
        (lambda manifest: manifest["recipes"][0]["files"].update(angelscript=[RECIPE_MANAGED]), "must end with .fos"),
        (lambda manifest: manifest["recipes"][0]["files"].update(angelscript=["Examples\\MechanicsSandbox\\A.fos"]), "not a repository-relative POSIX path"),
        (lambda manifest: manifest["recipes"][0].update(symbols=[]), "symbols must not be empty"),
        (lambda manifest: manifest["recipes"][0]["smoke"].update(scenarios=["missing"]), "names smoke scenario 'missing'"),
        (lambda manifest: manifest["cross_cutting"][0].update(slug="persistent-state", page=RECIPE_PAGE), "duplicate slug: persistent-state"),
    ],
)
def test_manifest_negative_controls(tree: Path, change: Callable[[dict], None], fragment: str) -> None:
    _edit_manifest(tree, change)
    messages = [finding.message for finding in _findings(tree) if finding.check == "manifest"]
    assert any(fragment in message for message in messages), messages


def test_unreadable_manifest_is_reported(tree: Path) -> None:
    _write(tree, MANIFEST, "{")
    _assert_single(tree, "manifest", "not readable JSON")


# (b) Listed files


def test_missing_listed_file_is_reported(tree: Path) -> None:
    (tree / RECIPE_CONTENT).unlink()
    _assert_single(tree, "files", "does not exist")


def test_listed_file_outside_the_sandbox_is_reported(tree: Path) -> None:
    _write(tree, "Examples/Other/Outside.fos", SCRIPT_TEXT)
    _edit_manifest(tree, lambda manifest: manifest["recipes"][0]["files"]["angelscript"].append("Examples/Other/Outside.fos"))
    _assert_single(tree, "files", "outside the example directory")


def test_missing_smoke_manifest_is_reported(tree: Path) -> None:
    (tree / SMOKE).unlink()
    _assert_single(tree, "files", "smoke manifest of recipe persistent-state does not exist")


# (c) Page template


def test_missing_page_is_reported(tree: Path) -> None:
    (tree / RECIPE_PAGE).unlink()
    findings = _findings(tree)
    assert any(finding.check == "page" and "does not exist" in finding.message for finding in findings)


@pytest.mark.parametrize(
    ("old", "new", "fragment"),
    [
        ("layout: default\ntitle:", "title:", "front matter keys must be exactly"),
        ("recipe_kind: primitive", "recipe_kind: pattern", "front matter recipe_kind must be 'primitive'"),
        ("document_id: recipe-persistent-state", "document_id: mechanics-persistent-state", "front matter document_id must be"),
        ("title: Custom Persistent Character State", "title: Persistent State", "does not match the manifest title"),
        ("# Custom Persistent Character State", "# Another Title", "the first heading must be"),
        ("## Authority and sync", "## Authority and cover", "## sections must be exactly, in order"),
        ("## Test route", "## Managed C# notes", "## sections must be exactly, in order"),
        ("- third pitfall\n", "", "needs at least 3 top-level list items, found 2"),
        ("| Managed C# | `shape` |", "| Managed C# | `compile-verified` |", "shows Managed C# as `compile-verified`"),
        ("| AngelScript | `runtime-verified` |\n", "", "no table row shows AngelScript with status `runtime-verified`"),
    ],
)
def test_page_negative_controls(tree: Path, old: str, new: str, fragment: str) -> None:
    _replace(tree, RECIPE_PAGE, old, new)
    messages = [finding.message for finding in _findings(tree) if finding.check == "page"]
    assert any(fragment in message for message in messages), messages


def test_swapped_sections_are_reported(tree: Path) -> None:
    text = (tree / RECIPE_PAGE).read_text(encoding="utf-8")
    text = text.replace("## Problem", "## PLACEHOLDER").replace("## Engine primitives used", "## Problem").replace("## PLACEHOLDER", "## Engine primitives used")
    _write(tree, RECIPE_PAGE, text)
    messages = [finding.message for finding in _findings(tree) if finding.check == "page"]
    assert any("## sections must be exactly, in order" in message for message in messages), messages


def test_page_outside_the_line_target_is_a_warning(tree: Path) -> None:
    _replace(tree, RECIPE_PAGE, "\n".join(f"Line {index}." for index in range(110)), "Short.")
    assert _findings(tree) == []
    warnings = _findings(tree, "warning")
    assert len(warnings) == 1 and "the target is 150-300" in warnings[0].message


def test_headings_inside_fences_do_not_count(tree: Path) -> None:
    _replace(tree, RECIPE_PAGE, "```bash\npython3 validate.py\n```", "```bash\n## not a heading\npython3 validate.py\n```")
    _replace(tree, RECIPE_MIRROR, "```bash\npython3 validate.py\n```", "```bash\n## not a heading\npython3 validate.py\n```")
    assert _findings(tree) == []


def test_cross_cutting_page_requires_the_plain_front_matter(tree: Path) -> None:
    _replace(tree, COVER_PAGE, "permalink: /Docs/en/how-to/mechanics/cover-transactions.html", "permalink: /Docs/en/how-to/mechanics/cover-transactions.html\nrecipe_kind: pattern")
    messages = [finding.message for finding in _findings(tree) if finding.check == "page"]
    assert any("front matter keys must be exactly layout, title, locale, document_id, permalink" in message for message in messages), messages


# (d) Verbatim excerpts


@pytest.mark.parametrize(
    ("relative", "old", "new"),
    [
        (RECIPE_SCRIPT, "void InitPersistentState()", "void InitState()"),
        (RECIPE_SCRIPT, "    Game.OnCritterInit.Subscribe(OnCritterInit);\n}", "    Game.OnCritterInit.Subscribe(OnCritterInit);\n\n}"),
        (RECIPE_SCRIPT, "///@ Property Critter Server int32 SampleCounter Mutable Persistent", "///@ Property Critter Server int64 SampleCounter Mutable Persistent"),
        (RECIPE_MANAGED, "        Game.OnCritterInit.Subscribe(OnCritterInit);", "        Game.OnCritterInit.Subscribe(Init);"),
        (RECIPE_CONTENT, "$Name = SampleCritter", "$Name = OtherCritter"),
    ],
)
def test_source_drift_breaks_the_excerpt(tree: Path, relative: str, old: str, new: str) -> None:
    _replace(tree, relative, old, new)
    findings = _findings(tree)
    assert findings and {finding.check for finding in findings} == {"excerpt"}, [finding.render() for finding in findings]


def test_edited_excerpt_line_is_reported_with_the_longest_match(tree: Path) -> None:
    _replace(tree, RECIPE_PAGE, "       Game.OnCritterInit.Subscribe(OnCritterInit);", "       Game.OnCritterInit.Subscribe(Other);")
    _replace(tree, RECIPE_MIRROR, "       Game.OnCritterInit.Subscribe(OnCritterInit);", "       Game.OnCritterInit.Subscribe(Other);")
    _assert_single(tree, "excerpt", f"the longest match is 3 of 5 line(s) from {RECIPE_SCRIPT}:8")


def test_non_contiguous_excerpt_is_reported(tree: Path) -> None:
    _replace(tree, RECIPE_PAGE, "   void InitPersistentState()\n   {\n", "   void InitPersistentState()\n")
    _replace(tree, RECIPE_MIRROR, "   void InitPersistentState()\n   {\n", "   void InitPersistentState()\n")
    _assert_single(tree, "excerpt", "first differing line: '    Game.OnCritterInit.Subscribe(OnCritterInit);'")


def test_reindented_excerpt_is_reported(tree: Path) -> None:
    old = "```csharp\n    [ModuleInit]\n    internal static void InitPersistentState()\n    {\n        Game.OnCritterInit.Subscribe(OnCritterInit);\n    }\n```"
    new = "```csharp\n[ModuleInit]\ninternal static void InitPersistentState()\n{\n    Game.OnCritterInit.Subscribe(OnCritterInit);\n}\n```"
    _replace(tree, RECIPE_PAGE, old, new)
    _replace(tree, RECIPE_MIRROR, old, new)
    _assert_single(tree, "excerpt", "fenced csharp block")


def test_excerpt_from_a_file_of_another_entry_is_reported(tree: Path) -> None:
    fence = "```angelscript\n    Game.Sync(first, second);\n```"
    _replace(tree, RECIPE_PAGE, "## Authority and sync\n", f"## Authority and sync\n\n{fence}\n")
    _replace(tree, RECIPE_MIRROR, "## Полномочия и синхронизация\n", f"## Полномочия и синхронизация\n\n{fence}\n")
    _assert_single(tree, "excerpt", "its first line occurs in none of them")


def test_excerpt_language_needs_a_matching_file(tree: Path) -> None:
    fence = "```json\n{\"schema_version\": 1}\n```"
    _replace(tree, RECIPE_PAGE, "## Problem\n", f"## Problem\n\n{fence}\n")
    _replace(tree, RECIPE_MIRROR, "## Задача\n", f"## Задача\n\n{fence}\n")
    _assert_single(tree, "excerpt", "has no .json file listed for recipe persistent-state to quote")


def test_free_languages_are_not_compared(tree: Path) -> None:
    fence = "```text\nanything goes here\n```"
    _replace(tree, RECIPE_PAGE, "## Problem\n", f"## Problem\n\n{fence}\n")
    _replace(tree, RECIPE_MIRROR, "## Задача\n", f"## Задача\n\n{fence}\n")
    assert _findings(tree) == []


def test_unclosed_fence_is_reported(tree: Path) -> None:
    _write(tree, RECIPE_PAGE, _recipe_page() + "```text\n")
    messages = [finding.message for finding in _findings(tree) if finding.check == "page"]
    assert any("is not closed" in message for message in messages), messages


# (e) Symbols


def test_manifest_symbol_missing_from_the_api_model_is_reported(tree: Path) -> None:
    _edit_manifest(tree, lambda manifest: manifest["recipes"][0]["symbols"].append("script.method.server.Critter.Removed"))
    _replace(tree, RECIPE_PAGE, "| server | checks the record |", "| server | checks the record |\n| `script.method.server.Critter.Removed` | server | gone |")
    _assert_single(tree, "symbol", "script.method.server.Critter.Removed of recipe persistent-state is not an id or family_id in the API model")


def test_manifest_symbol_not_cited_by_the_page_is_reported(tree: Path) -> None:
    _replace(tree, RECIPE_PAGE, "| `script.method.server.Entity.IsPersistent` | server | checks the record |\n", "")
    _assert_single(tree, "symbol", "script.method.server.Entity.IsPersistent of recipe persistent-state is not cited by the page")


def test_page_symbol_absent_from_the_manifest_is_reported(tree: Path) -> None:
    _replace(tree, RECIPE_PAGE, "A counter that survives a restart.", "A counter that survives a restart, unlike `script.method.server.Game.Sync`.")
    _assert_single(tree, "symbol", "cites script.method.server.Game.Sync, which is not in the manifest symbols")


def test_exact_overload_ids_are_accepted(tree: Path) -> None:
    exact = "script.method.server.Entity.IsPersistent#0123456789abcdef"
    _edit_manifest(tree, lambda manifest: manifest["recipes"][0].update(symbols=["script.event.server.Game.OnCritterInit", exact]))
    _replace(tree, RECIPE_PAGE, "`script.method.server.Entity.IsPersistent`", f"`{exact}`")
    assert _findings(tree) == []


# (f) Russian mirror


def test_missing_mirror_is_reported(tree: Path) -> None:
    (tree / RECIPE_MIRROR).unlink()
    _assert_single(tree, "mirror", "Russian mirror of Docs/en/how-to/mechanics/persistent-state.md does not exist")


@pytest.mark.parametrize(
    ("old", "new", "fragment"),
    [
        ("## См. также", "### См. также", "has 11 ## headings"),
        ("python3 validate.py", "python validate.py", "fenced blocks differ"),
        ("```bash", "```powershell", "fenced blocks differ"),
        ("locale: ru", "locale: en", "front matter locale must be 'ru'"),
        ("recipe_kind: primitive", "recipe_kind: pattern", "front matter recipe_kind must be 'primitive'"),
        ("permalink: /Docs/ru/how-to/mechanics/persistent-state.html", "permalink: /Docs/en/how-to/mechanics/persistent-state.html", "front matter permalink"),
    ],
)
def test_mirror_negative_controls(tree: Path, old: str, new: str, fragment: str) -> None:
    _replace(tree, RECIPE_MIRROR, old, new)
    _assert_single(tree, "mirror", fragment)


# (g) Project names


@pytest.mark.parametrize(
    ("relative", "text"),
    [
        (RECIPE_PAGE, "Taken from Last Frontier."),
        (RECIPE_MIRROR, "Как в LastFrontier."),
        (HUB, "See fonline-tla."),
        (RECIPE_SCRIPT, "// TLA keeps this differently"),
        (RECIPE_MANAGED, "// Tla::Helpers"),
        (f"{SANDBOX}/README.md", "Mirrors LF_Server."),
        (COVER_PAGE, "Ported from the lastfrontier repository."),
    ],
)
def test_project_names_are_reported(tree: Path, relative: str, text: str) -> None:
    path = tree / relative
    existing = path.read_text(encoding="utf-8") if path.is_file() else ""
    _write(tree, relative, existing + text + "\n")
    findings = _findings(tree)
    assert [finding.check for finding in findings] == ["project-name"], [finding.render() for finding in findings]
    assert findings[0].path.startswith(relative + ":")


def test_ordinary_words_are_not_project_names(tree: Path) -> None:
    _write(tree, f"{SANDBOX}/README.md", "An atlas of settlements; self_check and halfLF_ stay clean. Atlantic.\n")
    assert _findings(tree) == []


def test_build_outputs_are_not_scanned(tree: Path) -> None:
    _write(tree, f"{SANDBOX}/Build/windows/Generated.txt", "LF_Generated\n")
    _write(tree, f"{SANDBOX}/Binary.png", "Last Frontier\n")
    assert _findings(tree) == []


# Section index


def test_hub_must_link_every_page(tree: Path) -> None:
    _replace(tree, HUB, "- [Cover transactions](cover-transactions.md)", "- Cover transactions")
    _assert_single(tree, "hub", "does not link Docs/en/how-to/mechanics/cover-transactions.md")


def test_hub_row_must_repeat_the_manifest_statuses(tree: Path) -> None:
    _replace(tree, HUB, "| `primitive` | `runtime-verified` | `shape` |", "| `primitive` | `shape` | `runtime-verified` |")
    findings = _findings(tree)
    assert {finding.check for finding in findings} == {"hub"}
    assert any("shows angelscript `shape`" in finding.message for finding in findings), [finding.render() for finding in findings]


def test_hub_mirror_row_must_repeat_the_manifest_statuses(tree: Path) -> None:
    _replace(tree, HUB_MIRROR, "| `primitive` | `runtime-verified` | `shape` |", "| `primitive` | `runtime-verified` | `compile-verified` |")
    _assert_single(tree, "mirror", "does not show managed `shape`")


def test_hub_front_matter_and_mirror_are_required(tree: Path) -> None:
    _replace(tree, HUB, "document_id: mechanics-recipes", "document_id: mechanics")
    messages = [finding.message for finding in _findings(tree) if finding.check == "hub"]
    assert any("front matter must be exactly" in message for message in messages), messages

    _replace(tree, HUB, "document_id: mechanics", "document_id: mechanics-recipes")
    (tree / HUB_MIRROR).unlink()
    _assert_single(tree, "mirror", "Russian mirror of Docs/en/how-to/mechanics/index.md does not exist")


def test_missing_hub_is_reported(tree: Path) -> None:
    (tree / HUB).unlink()
    _assert_single(tree, "hub", "section index does not exist")


# Markdown parsing


def test_list_item_fences_lose_only_their_own_indentation() -> None:
    document = mechanics_recipes.parse_markdown("1. Step:\n\n   ```angelscript\n   void A()\n   {\n       B();\n   }\n   ```\n")
    assert [(fence.language, fence.body) for fence in document.fences] == [("angelscript", ["void A()", "{", "    B();", "}"])]
    assert document.headings == []


# Real repository


REAL_MANIFEST = ENGINE_ROOT / mechanics_recipes.DEFAULT_MANIFEST


def test_real_hub_and_its_mirror_are_structurally_valid() -> None:
    assert mechanics_recipes.validate_hub(ENGINE_ROOT) == []

    hub = (ENGINE_ROOT / HUB).read_text(encoding="utf-8")
    for page in ("persistent-state.md", "validated-action.md", "timed-effect.md", "cover-transactions.md"):
        assert f"]({page})" in hub, page


@pytest.mark.skipif(not REAL_MANIFEST.is_file(), reason=f"{mechanics_recipes.DEFAULT_MANIFEST} is not in this checkout yet; the example lands separately")
class TestRealRepository:
    def test_every_recipe_passes_every_gate(self) -> None:
        findings = mechanics_recipes.validate(ENGINE_ROOT)
        assert [finding.render() for finding in findings if finding.severity == "error"] == []

    def test_command_line_check_passes(self) -> None:
        result = subprocess.run(
            [sys.executable, str(ENGINE_ROOT / "BuildTools/mechanics_recipes.py"), "--check"],
            cwd=ENGINE_ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=120,
        )
        assert result.returncode == 0, result.stdout + result.stderr
