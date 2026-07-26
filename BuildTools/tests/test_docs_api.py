from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_api  # noqa: E402
import docs_inventory  # noqa: E402


class DocumentationApiModelTests(unittest.TestCase):
    def test_overload_ids_and_source_locations_are_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source_path = root / "Source/Scripting/TestMethods.cpp"
            source_path.parent.mkdir(parents=True)
            source_path.write_text(
                "///@ ExportEntity Game ServerEngine ClientEngine Global\n"
                "///@ ExportMethod\n"
                "FO_SCRIPT_API bool Common_Game_IsReady(ptr<BaseEngine> engine, string_view label);\n"
                "///@ ExportMethod\n"
                "FO_SCRIPT_API bool Common_Game_IsReady(ptr<BaseEngine> engine, int32_t value = 1);\n",
                encoding="utf-8",
            )

            first_model = docs_api.generate_api_model(root)
            second_model = docs_api.generate_api_model(root)

            self.assertEqual(first_model, second_model)
            methods = [symbol for symbol in first_model["symbols"] if symbol["kind"] == "method"]
            self.assertEqual(len(methods), 2)
            self.assertEqual({symbol["family_id"] for symbol in methods}, {"script.method.common.Game.IsReady"})
            self.assertEqual(len({symbol["id"] for symbol in methods}), 2)
            self.assertTrue(all("#" in symbol["id"] for symbol in methods))
            self.assertEqual({symbol["source"]["line"] for symbol in methods}, {2, 4})
            self.assertTrue(all(symbol["stability"] == "internal" for symbol in methods))
            self.assertTrue(all(symbol["runtime_sides"] == ["server", "client", "mapper"] for symbol in methods))

    def test_engine_model_matches_independent_source_inventory(self) -> None:
        model = docs_api.generate_api_model(ENGINE_ROOT)
        inventory = docs_inventory.generate_inventory(ENGINE_ROOT)
        counts = model["summary"]["symbols_by_kind"]

        self.assertEqual(counts["method"], inventory["script_api"]["export_method_declarations"])
        self.assertEqual(counts["setting"], inventory["settings"]["declaration_count"])
        self.assertGreater(counts["property"], 0)
        self.assertGreater(counts["event"], 0)
        self.assertTrue(all(not Path(source).is_absolute() for source in model["metadata_source_files"]))
        self.assertEqual(model["summary"]["explicit_contract_declaration_count"], 1)
        self.assertEqual(model["summary"]["explicit_contract_symbol_count"], 1)
        debugger_symbol = next(
            symbol for symbol in model["symbols"] if symbol["id"] == "script.method.common.Game.BreakIntoDebugger"
        )
        self.assertEqual(debugger_symbol["stability"], "internal")
        self.assertTrue(debugger_symbol["contract"]["explicit"])

    def test_source_contracts_apply_to_families_and_deprecated_symbols_without_hash_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source_path = root / "Source/Scripting/TestMethods.cpp"
            source_path.parent.mkdir(parents=True)
            source_path.write_text(
                "///@ ExportEntity Game ServerEngine ClientEngine Global\n"
                "///@ ExportMethod\n"
                "FO_SCRIPT_API bool Common_Game_IsReady(ptr<BaseEngine> engine, string_view label);\n"
                "///@ ExportMethod\n"
                "FO_SCRIPT_API bool Common_Game_IsReady(ptr<BaseEngine> engine, int32_t value = 1);\n"
                "///@ ExportMethod\n"
                "FO_SCRIPT_API void Common_Game_Legacy(ptr<BaseEngine> engine);\n"
                "///@ ExportMethod\n"
                "FO_SCRIPT_API void Common_Game_Current(ptr<BaseEngine> engine);\n",
                encoding="utf-8",
            )
            example_path = root / "Docs/ContractExample.md"
            example_path.parent.mkdir(parents=True)
            example_path.write_text("# Contract example\n", encoding="utf-8")

            docs_api.generate_api_model(root)
            baseline_hash = docs_api.codegen.compatibility_hasher.hexdigest()

            contract_path = root / "Source/Common/ApiContracts.inc"
            contract_path.parent.mkdir(parents=True)
            contract_path.write_text(
                "/// Family contract.\n"
                "///@ ApiContract script.method.common.Game.IsReady experimental "
                "Since=0.1.0 Example=Docs/ContractExample.md\n"
                "///@ ApiContract script.method.common.Game.Legacy deprecated "
                "DeprecatedSince=0.2.0 Replacement=script.method.common.Game.Current Removal=1.0.0\n",
                encoding="utf-8",
            )

            model = docs_api.generate_api_model(root)
            classified_hash = docs_api.codegen.compatibility_hasher.hexdigest()
            ready_methods = [
                symbol for symbol in model["symbols"] if symbol["family_id"] == "script.method.common.Game.IsReady"
            ]
            legacy_method = next(
                symbol for symbol in model["symbols"] if symbol["id"] == "script.method.common.Game.Legacy"
            )

            self.assertEqual(classified_hash, baseline_hash)
            self.assertEqual(len(ready_methods), 2)
            self.assertTrue(all(symbol["stability"] == "experimental" for symbol in ready_methods))
            self.assertTrue(all(symbol["since"] == "0.1.0" for symbol in ready_methods))
            self.assertTrue(all(symbol["contract"]["explicit"] for symbol in ready_methods))
            self.assertEqual(ready_methods[0]["examples"], ["Docs/ContractExample.md"])
            self.assertEqual(legacy_method["stability"], "deprecated")
            self.assertEqual(
                legacy_method["deprecated"],
                {
                    "since": "0.2.0",
                    "replacement": "script.method.common.Game.Current",
                    "removal": "1.0.0",
                },
            )
            self.assertEqual(model["summary"]["explicit_contract_declaration_count"], 2)
            self.assertEqual(model["summary"]["explicit_contract_symbol_count"], 3)

    def test_unknown_contract_selector_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source_path = root / "Source/Scripting/TestMethods.cpp"
            source_path.parent.mkdir(parents=True)
            source_path.write_text(
                "///@ ExportEntity Game ServerEngine ClientEngine Global\n"
                "///@ ApiContract script.method.common.Game.Missing internal\n",
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "selector does not match"):
                docs_api.generate_api_model(root)

    def test_rendered_model_has_stable_json_format(self) -> None:
        rendered = docs_api.render_api_model(ENGINE_ROOT)
        parsed = json.loads(rendered)

        self.assertTrue(rendered.endswith("\n"))
        self.assertEqual(parsed["schema_version"], docs_api.SCHEMA_VERSION)
        self.assertEqual(parsed["generated_by"], "BuildTools/docs_api.py")
        self.assertEqual(parsed["source_parser"], "BuildTools/codegen.py")
        self.assertEqual(parsed["scope"]["default_stability"], "internal")
        self.assertEqual(
            parsed["summary"]["symbol_count"],
            parsed["summary"]["described_symbol_count"] + parsed["summary"]["missing_description_count"],
        )


if __name__ == "__main__":
    unittest.main()
