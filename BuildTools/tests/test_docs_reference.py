from __future__ import annotations

import contextlib
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


class DocumentationReferenceTests(unittest.TestCase):
    def test_all_symbol_kinds_render_with_escaped_content_and_source_links(self) -> None:
        model = _model()
        pages = docs_reference.generate_reference_pages(model)

        self.assertEqual(set(pages), set(docs_reference.OUTPUT_PATHS))
        rendered = "\n".join(pages.values())
        for symbol in model["symbols"]:
            self.assertIn(f"<code>{symbol['id']}</code>", rendered)
        self.assertIn(
            "Needs &#124; escaping &lt;now&gt; &#123;&#123; safely &#125;&#125;<br>Second line",
            pages["Docs/generated/api/methods.md"],
        )
        self.assertNotIn("{{", pages["Docs/generated/api/methods.md"])
        self.assertIn(
            "https://github.com/cvet/fonline/blob/master/Source/Test.cpp#L7",
            pages["Docs/generated/api/methods.md"],
        )
        self.assertIn("<code>experimental</code> (explicit)", pages["Docs/generated/api/methods.md"])
        self.assertIn("since <code>0.1.0</code>", pages["Docs/generated/api/methods.md"])
        self.assertIn(
            "[Docs/ContractExample.md#run](../../ContractExample.md#run)",
            pages["Docs/generated/api/methods.md"],
        )
        self.assertIn(
            "https://github.com/cvet/fonline/blob/master/Source/ApiContracts.inc#L3",
            pages["Docs/generated/api/methods.md"],
        )
        self.assertIn("Reviewed contract", pages["Docs/generated/api/methods.md"])
        self.assertIn("Explicitly classified symbols", pages["Docs/generated/api/index.md"])
        self.assertIn("It is not a semantic credential", pages["Docs/generated/api/settings.md"])

    def test_generation_is_deterministic_and_anchors_are_unique_per_page(self) -> None:
        first = docs_reference.generate_reference_pages(_model())
        second = docs_reference.generate_reference_pages(_model())

        self.assertEqual(first, second)
        for content in first.values():
            anchors = re.findall(r'<a id="([^"]+)"></a>', content)
            self.assertEqual(len(anchors), len(set(anchors)))
            self.assertTrue(content.endswith("\n"))

    def test_write_and_check_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            model_path = root / docs_reference.DEFAULT_MODEL
            model_path.parent.mkdir(parents=True)
            model_path.write_text(json.dumps(_model()), encoding="utf-8")

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
        rendered = "\n".join(pages.values())
        model = json.loads((ENGINE_ROOT / docs_reference.DEFAULT_MODEL).read_text(encoding="utf-8"))

        anchors = re.findall(r'<a id="symbol-[^"]+"></a>', rendered)
        self.assertEqual(len(anchors), model["summary"]["symbol_count"])


if __name__ == "__main__":
    unittest.main()
