from __future__ import annotations

import contextlib
import copy
import io
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_reference  # noqa: E402
import docs_description_translations  # noqa: E402
import docs_localization  # noqa: E402


def _symbol(kind: str, symbol_id: str, name: str, **extra: object) -> dict[str, object]:
    symbol: dict[str, object] = {
        "id": symbol_id,
        "family_id": symbol_id,
        "kind": kind,
        "name": name,
        "runtime_sides": ["server"],
        "receiver": extra.pop("receiver", None),
        "signature": extra.pop("signature", name),
        "description": extra.pop("description", "Documented"),
        "flags": [],
        "stability": "internal",
        "since": None,
        "deprecated": None,
        "examples": [],
        "source": {"path": "Source/Test.cpp", "line": 7},
        "contract": {
            "explicit": False,
            "selector": None,
            "source": None,
            "notes": "",
        },
    }
    symbol.update(extra)
    return symbol


def _model() -> dict[str, object]:
    symbols = [
        _symbol(
            "method",
            "script.method.common.Game.Run#one",
            "Run",
            receiver="Game",
            signature="void Game.Run(string value)",
            description="Needs | escaping <now> {{ safely }}\nSecond line",
            declared_target="common",
            receivers=["Game"],
            arguments=[],
            stability="experimental",
            since="0.1.0",
            examples=["Docs/ContractExample.md#run"],
            contract={
                "explicit": True,
                "selector": "script.method.common.Game.Run",
                "source": {"path": "Source/ApiContracts.inc", "line": 3},
                "notes": "Reviewed contract",
            },
            **{"return": {"type": "void", "nullable": False}},
        ),
        _symbol(
            "property",
            "script.property.Critter.Name",
            "Name",
            receiver="Critter",
            signature="string Critter.Name",
            access="common",
            type="string",
            nullable=False,
            mutability="read-only",
            persistent=True,
        ),
        _symbol(
            "event",
            "script.event.server.Game.OnStart",
            "OnStart",
            receiver="Game",
            signature="event Game.OnStart()",
            declared_target="server",
            arguments=[],
        ),
        _symbol(
            "entity",
            "script.entity.Critter",
            "Critter",
            signature="entity Critter",
            capabilities={"prototypes": True},
        ),
        _symbol(
            "enum",
            "script.enum.Direction",
            "Direction",
            signature="enum Direction : uint8",
            underlying_type="uint8",
            generated=False,
        ),
        _symbol(
            "enum-value",
            "script.enum-value.Direction.North",
            "North",
            receiver="Direction",
            signature="Direction.North = 0",
            parent_id="script.enum.Direction",
            value="0",
            evaluated_value=0,
            generated=False,
        ),
        _symbol("value-type", "script.value-type.Point", "Point", signature="value type Point", native_type="Point"),
        _symbol(
            "value-field",
            "script.value-field.Point.X",
            "X",
            receiver="Point",
            signature="int32 Point.X",
            parent_id="script.value-type.Point",
            type="int32",
            mutability="value",
        ),
        _symbol(
            "ref-type",
            "script.ref-type.client.Layer",
            "Layer",
            signature="ref type Layer",
            declared_target="client",
        ),
        _symbol(
            "ref-field",
            "script.ref-field.client.Layer.Enabled",
            "Enabled",
            receiver="Layer",
            signature="bool Layer.Enabled",
            parent_id="script.ref-type.client.Layer",
            type="bool",
            mutability="mutable",
        ),
        _symbol(
            "ref-method",
            "script.ref-method.client.Layer.Clear",
            "Clear",
            receiver="Layer",
            signature="void Layer.Clear()",
            parent_id="script.ref-type.client.Layer",
            arguments=[],
            **{"return": {"type": "void", "nullable": False}},
        ),
        _symbol(
            "setting",
            "setting.server.Test.Token",
            "Test.Token",
            signature="string Test.Token",
            declared_target="server",
            group="Test",
            type="string",
            setting_kind="variable",
            mutability="runtime-variable",
            default_values=[],
            command_line_redacted_by_default=True,
            redaction_rule="test rule",
        ),
        _symbol(
            "migration-rule",
            "migration.Property.Critter.OldName",
            "OldName",
            signature="Property Critter OldName Name",
            rule_kind="Property",
            scope="Critter",
            replacement="Name",
        ),
    ]
    return {
        "schema_version": docs_reference.docs_api.SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_api.py",
        "source_parser": "BuildTools/codegen.py",
        "scope": {
            "repository": "cvet/fonline",
            "surface": "engine-native-codegen",
            "included": ["test symbols"],
            "excluded": ["test exclusions"],
        },
        "summary": {
            "symbol_count": len(symbols),
            "described_symbol_count": len(symbols),
            "missing_description_count": 0,
            "symbols_without_source_count": 0,
            "metadata_source_file_count": 1,
            "symbols_by_stability": {"experimental": 1, "internal": len(symbols) - 1},
            "explicit_contract_declaration_count": 1,
            "explicit_contract_symbol_count": 1,
            "default_contract_symbol_count": len(symbols) - 1,
        },
        "symbols": symbols,
    }


def _write_translation_catalog(root: Path, model: dict[str, object]) -> None:
    inventory = docs_description_translations.inventory_model("api", model)
    catalog = {
        "schema_version": docs_description_translations.SCHEMA_VERSION,
        "source_locale": "en",
        "target_locale": "ru",
        "enforcement": "registered-translations-current",
        "domains": {
            "api": {
                "entries": {
                    locator: {
                        "source_sha256": docs_localization.normalized_sha256(str(source)),
                        "translation": f"Перевод: {source}",
                    }
                    for locator, source in inventory.items()
                }
            }
        },
    }
    path = root / docs_description_translations.DEFAULT_CATALOG
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(catalog, ensure_ascii=False),
        encoding="utf-8",
    )


class DocumentationReferenceTests(unittest.TestCase):
    def test_all_symbol_kinds_render_with_escaped_content_and_source_links(self) -> None:
        model = _model()
        pages = docs_reference.generate_reference_pages(model)

        self.assertEqual(set(pages), set(docs_reference.OUTPUT_PATHS))
        rendered = "\n".join(pages.values())
        methods_page = pages[f"{docs_reference.DEFAULT_OUTPUT_DIR}/methods.md"]
        index_page = pages[f"{docs_reference.DEFAULT_OUTPUT_DIR}/index.md"]
        settings_page = pages[f"{docs_reference.DEFAULT_OUTPUT_DIR}/settings.md"]
        for symbol in model["symbols"]:
            self.assertIn(f"<code>{symbol['id']}</code>", rendered)
        self.assertIn(
            "Needs &#124; escaping &lt;now&gt; &#123;&#123; safely &#125;&#125;<br>Second line",
            methods_page,
        )
        self.assertNotIn("{{", methods_page)
        self.assertIn(
            "https://github.com/cvet/fonline/blob/master/Source/Test.cpp#L7",
            methods_page,
        )
        self.assertIn("<code>experimental</code> (explicit)", methods_page)
        self.assertIn("since <code>0.1.0</code>", methods_page)
        self.assertIn(
            "[Docs/ContractExample.md#run](../../../ContractExample.md#run)",
            methods_page,
        )
        self.assertIn(
            "https://github.com/cvet/fonline/blob/master/Source/ApiContracts.inc#L3",
            methods_page,
        )
        self.assertIn("Reviewed contract", methods_page)
        self.assertIn("Explicitly classified symbols", index_page)
        self.assertIn("It is not a semantic credential", settings_page)

        russian_methods = pages[f"{docs_reference.RUSSIAN_OUTPUT_DIR}/methods.md"]
        self.assertIn("locale: ru", russian_methods)
        self.assertIn("Нативные методы скриптов", russian_methods)
        self.assertIn("ID символа", russian_methods)
        self.assertIn("docs-translation:", russian_methods)

        for filename, _, _ in docs_reference.PAGE_DEFINITIONS:
            canonical = pages[f"{docs_reference.DEFAULT_OUTPUT_DIR}/{filename}"]
            legacy = pages[f"{docs_reference.LEGACY_OUTPUT_DIR}/{filename}"]
            self.assertIn(f"../../en/reference/script-api/{filename}", legacy)
            self.assertIn(f"../../ru/reference/script-api/{filename}", legacy)
            for heading in re.findall(r"^(#{2,3} .+)$", canonical, flags=re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)

    def test_generation_is_deterministic_and_anchors_are_unique_per_page(self) -> None:
        first = docs_reference.generate_reference_pages(_model())
        second = docs_reference.generate_reference_pages(_model())

        self.assertEqual(first, second)
        for content in first.values():
            anchors = re.findall(r'<a id="([^"]+)"></a>', content)
            self.assertEqual(len(anchors), len(set(anchors)))
            self.assertTrue(content.endswith("\n"))

    def test_russian_pages_render_translated_descriptions_and_contract_notes(self) -> None:
        model = _model()
        russian_model = copy.deepcopy(model)
        method = russian_model["symbols"][0]
        method["description"] = "Переведенное описание"
        method["contract"]["notes"] = "Переведенная заметка контракта"

        pages = docs_reference.generate_reference_pages(model, russian_model)
        russian_methods = pages[f"{docs_reference.RUSSIAN_OUTPUT_DIR}/methods.md"]

        self.assertIn("Переведенное описание", russian_methods)
        self.assertIn("Переведенная заметка контракта", russian_methods)
        self.assertNotIn("Reviewed contract", russian_methods)

    def test_write_and_check_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            model_path = root / docs_reference.DEFAULT_MODEL
            model_path.parent.mkdir(parents=True)
            model = _model()
            model_path.write_text(json.dumps(model), encoding="utf-8")
            _write_translation_catalog(root, model)

            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(docs_reference.main(["--root", str(root), "--write"]), 0)
                self.assertEqual(docs_reference.main(["--root", str(root), "--check"]), 0)
                (root / docs_reference.OUTPUT_PATHS[0]).write_text("stale\n", encoding="utf-8")
                self.assertEqual(docs_reference.main(["--root", str(root), "--check"]), 1)

    def test_unknown_symbol_kind_is_rejected(self) -> None:
        model = _model()
        model["symbols"].append(_symbol("future-kind", "future.symbol", "Future"))

        with self.assertRaisesRegex(ValueError, "Unsupported API symbol kinds: future-kind"):
            docs_reference.generate_reference_pages(model)

    def test_engine_model_renders_every_symbol_once_as_an_anchor(self) -> None:
        pages = docs_reference.render_reference_pages(ENGINE_ROOT)
        rendered = "\n".join(pages[path] for path in docs_reference.CANONICAL_OUTPUT_PATHS)
        model = json.loads((ENGINE_ROOT / docs_reference.DEFAULT_MODEL).read_text(encoding="utf-8"))
        english_index = pages[f"{docs_reference.DEFAULT_OUTPUT_DIR}/index.md"]
        russian_index = pages[f"{docs_reference.RUSSIAN_OUTPUT_DIR}/index.md"]

        anchors = re.findall(r'<a id="symbol-[^"]+"></a>', rendered)
        self.assertEqual(len(anchors), model["summary"]["symbol_count"])
        self.assertFalse(any(line.endswith(" ") for line in rendered.splitlines()))
        self.assertIn("## Scope contract", english_index)
        self.assertIn("2494 stable IDs", english_index)
        self.assertIn("## Контракт области", russian_index)
        self.assertIn("2494 стабильных ID", russian_index)
        self.assertNotIn("The complete current inventory", russian_index)

    def test_engine_russian_pages_pin_hashes_and_use_semantic_overlay(self) -> None:
        pages = docs_reference.render_reference_pages(ENGINE_ROOT)
        russian_methods = pages[f"{docs_reference.RUSSIAN_OUTPUT_DIR}/methods.md"]
        russian_settings = pages[f"{docs_reference.RUSSIAN_OUTPUT_DIR}/settings.md"]

        self.assertIn("SyncScope: требует self", russian_methods)
        self.assertIn("Отладочная ловушка только для разработки", russian_methods)
        self.assertIn("Если true, звук отключен", russian_settings)
        for (_, document_id, _), english_path, russian_path in zip(
            docs_reference.PAGE_DEFINITIONS,
            docs_reference.CANONICAL_OUTPUT_PATHS,
            docs_reference.RUSSIAN_OUTPUT_PATHS,
            strict=True,
        ):
            english = pages[english_path]
            russian = pages[russian_path]
            self.assertIn(
                docs_localization.translation_metadata_line(
                    document_id,
                    english_path,
                    docs_localization.normalized_sha256(english),
                ),
                russian,
            )
            self.assertEqual(
                re.findall(r"```[^\n]*\n(.*?)```", english, re.DOTALL),
                re.findall(r"```[^\n]*\n(.*?)```", russian, re.DOTALL),
            )


if __name__ == "__main__":
    unittest.main()
