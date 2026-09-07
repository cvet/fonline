from __future__ import annotations

import contextlib
import hashlib
import io
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
    def test_description_omits_ide_and_linter_suppressions(self) -> None:
        self.assertEqual(
            docs_api._description(
                [
                    "ReSharper disable once CppInconsistentNaming",
                    "Describes the exported method.",
                    "NOLINTNEXTLINE(readability-identifier-naming)",
                ]
            ),
            "Describes the exported method.",
        )

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

    def test_ref_type_member_comments_are_preserved(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source_path = root / "Source/Common/TestRefType.h"
            source_path.parent.mkdir(parents=True)
            source_path.write_text(
                "// Script-visible mutable sample state.\n"
                "///@ ExportRefType Common RefCounted Export = Enabled, Count, Reset\n"
                "class SampleState final : public RefCounted<SampleState>\n"
                "{\n"
                "public:\n"
                "    // Reports whether the sample is enabled.\n"
                "    bool Enabled {};\n"
                "    int32_t Count {}; // Current sample count.\n"
                "\n"
                "    // Restores the initial sample state.\n"
                "    void Reset();\n"
                "};\n",
                encoding="utf-8",
            )

            model = docs_api.generate_api_model(root)
            descriptions = {
                symbol["id"]: symbol["description"]
                for symbol in model["symbols"]
            }
            self.assertEqual(
                descriptions["script.ref-type.common.SampleState"],
                "Script-visible mutable sample state.",
            )
            self.assertEqual(
                descriptions["script.ref-field.common.SampleState.Enabled"],
                "Reports whether the sample is enabled.",
            )
            self.assertEqual(
                descriptions["script.ref-field.common.SampleState.Count"],
                "Current sample count.",
            )
            self.assertEqual(
                descriptions["script.ref-method.common.SampleState.Reset"],
                "Restores the initial sample state.",
            )

    def test_value_field_doc_describes_layout_fields_without_changing_the_runtime_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source_path = root / "Source/Common/TestValueType.h"
            source_path.parent.mkdir(parents=True)
            source_path.write_text(
                "// Script-visible sample pair.\n"
                "///@ ExportValueType Layout = int32-x+int32-y\n"
                "///@ ValueFieldDoc SamplePair x // Horizontal sample component.\n"
                "///@ ValueFieldDoc SamplePair y // Vertical sample component.\n"
                "struct SamplePair\n"
                "{\n"
                "    int32_t x {};\n"
                "    int32_t y {};\n"
                "};\n",
                encoding="utf-8",
            )

            model = docs_api.generate_api_model(root)
            hash_with_docs = docs_api.codegen.compatibility_hasher.hexdigest()
            descriptions = {symbol["id"]: symbol["description"] for symbol in model["symbols"]}
            self.assertEqual(descriptions["script.value-field.SamplePair.x"], "Horizontal sample component.")
            self.assertEqual(descriptions["script.value-field.SamplePair.y"], "Vertical sample component.")
            self.assertEqual(
                next(symbol for symbol in model["symbols"] if symbol["id"] == "script.value-field.SamplePair.x")["source"]["line"],
                3,
            )

            source_path.write_text(
                "// Script-visible sample pair.\n"
                "///@ ExportValueType Layout = int32-x+int32-y\n"
                "struct SamplePair\n"
                "{\n"
                "    int32_t x {};\n"
                "    int32_t y {};\n"
                "};\n",
                encoding="utf-8",
            )
            docs_api.generate_api_model(root)
            self.assertEqual(docs_api.codegen.compatibility_hasher.hexdigest(), hash_with_docs)

            source_path.write_text(
                "// Script-visible sample pair.\n"
                "///@ ExportValueType Layout = int32-x+int32-y\n"
                "///@ ValueFieldDoc SamplePair z // Unknown component.\n"
                "struct SamplePair\n"
                "{\n"
                "    int32_t x {};\n"
                "    int32_t y {};\n"
                "};\n",
                encoding="utf-8",
            )
            with contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(AssertionError, "Unknown layout field SamplePair.z"):
                docs_api.generate_api_model(root)

    def test_enum_value_doc_describes_values_without_changing_the_runtime_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source_path = root / "Source/Common/TestEnum.h"
            source_path.parent.mkdir(parents=True)
            source_path.write_text(
                "// Sample operating mode.\n"
                "///@ ExportEnum\n"
                "enum class SampleMode : uint8_t\n"
                "{\n"
                "    None = 0,\n"
                "    Active = 1,\n"
                "};\n"
                "///@ EnumValueDoc SampleMode None // No sample mode is selected.\n"
                "///@ EnumValueDoc SampleMode Active // Sample processing is enabled.\n",
                encoding="utf-8",
            )

            model = docs_api.generate_api_model(root)
            hash_with_docs = docs_api.codegen.compatibility_hasher.hexdigest()
            values = {
                symbol["id"]: symbol
                for symbol in model["symbols"]
                if symbol["kind"] == "enum-value"
            }
            self.assertEqual(
                values["script.enum-value.SampleMode.None"]["description"],
                "No sample mode is selected.",
            )
            self.assertEqual(
                values["script.enum-value.SampleMode.Active"]["description"],
                "Sample processing is enabled.",
            )
            self.assertEqual(values["script.enum-value.SampleMode.None"]["source"]["line"], 8)

            source_path.write_text(
                "// Sample operating mode.\n"
                "///@ ExportEnum\n"
                "enum class SampleMode : uint8_t\n"
                "{\n"
                "    None = 0,\n"
                "    Active = 1,\n"
                "};\n",
                encoding="utf-8",
            )
            docs_api.generate_api_model(root)
            self.assertEqual(docs_api.codegen.compatibility_hasher.hexdigest(), hash_with_docs)

            source_path.write_text(
                "// Sample operating mode.\n"
                "///@ ExportEnum\n"
                "enum class SampleMode : uint8_t\n"
                "{\n"
                "    None = 0,\n"
                "};\n"
                "///@ EnumValueDoc SampleMode Missing // Unknown value.\n",
                encoding="utf-8",
            )
            with contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(
                AssertionError,
                "Unknown enum value SampleMode.Missing",
            ):
                docs_api.generate_api_model(root)

    def test_engine_model_matches_independent_source_inventory(self) -> None:
        model = docs_api.generate_api_model(ENGINE_ROOT)
        inventory = docs_inventory.generate_inventory(ENGINE_ROOT)
        counts = model["summary"]["symbols_by_kind"]

        self.assertEqual(counts["method"], inventory["script_api"]["export_method_declarations"])
        self.assertEqual(counts["setting"], inventory["settings"]["declaration_count"])
        self.assertGreater(counts["property"], 0)
        self.assertGreater(counts["event"], 0)
        self.assertEqual(
            [symbol["id"] for symbol in model["symbols"] if symbol["kind"] == "event" and not symbol["description"]],
            [],
        )
        self.assertEqual(
            [symbol["id"] for symbol in model["symbols"] if symbol["kind"] == "property" and not symbol["description"]],
            [],
        )
        for source_path in (
            "Source/Scripting/ClientCritterScriptMethods.cpp",
            "Source/Scripting/ClientEntityScriptMethods.cpp",
            "Source/Scripting/ClientGlobalScriptMethods.cpp",
            "Source/Scripting/ClientImGuiScriptMethods.cpp",
            "Source/Scripting/ClientItemScriptMethods.cpp",
            "Source/Scripting/ClientMapScriptMethods.cpp",
            "Source/Scripting/CommonGlobalScriptMethods.cpp",
            "Source/Scripting/CommonImGuiScriptMethods.cpp",
            "Source/Scripting/MapperGlobalScriptMethods.cpp",
        ):
            with self.subTest(described_method_source=source_path):
                self.assertEqual(
                    [
                        symbol["id"]
                        for symbol in model["symbols"]
                        if symbol["kind"] == "method"
                        and symbol["source"]["path"] == source_path
                        and not symbol["description"]
                    ],
                    [],
                )
        self.assertEqual(
            [symbol["id"] for symbol in model["symbols"] if symbol["kind"] == "value-type" and not symbol["description"]],
            [],
        )
        self.assertEqual(
            [symbol["id"] for symbol in model["symbols"] if symbol["kind"] == "value-field" and not symbol["description"]],
            [],
        )
        self.assertEqual(
            [symbol["id"] for symbol in model["symbols"] if symbol["kind"] == "migration-rule" and not symbol["description"]],
            [],
        )
        self.assertEqual(
            [symbol["id"] for symbol in model["symbols"] if symbol["kind"] == "entity" and not symbol["description"]],
            [],
        )
        self.assertEqual(
            [symbol["id"] for symbol in model["symbols"] if symbol["kind"] == "enum" and not symbol["description"]],
            [],
        )
        self.assertEqual(
            [
                symbol["id"]
                for symbol in model["symbols"]
                if symbol["kind"] == "enum-value" and symbol["generated"] and not symbol["description"]
            ],
            [],
        )
        imgui_values = [
            symbol
            for symbol in model["symbols"]
            if symbol["kind"] == "enum-value" and symbol["receiver"].startswith("ImGui_")
        ]
        self.assertEqual(len(imgui_values), 302)
        self.assertEqual([symbol["id"] for symbol in imgui_values if not symbol["description"]], [])
        self.assertEqual(
            sum(symbol["source"]["path"] == "ThirdParty/imgui/imgui.h" for symbol in imgui_values),
            234,
        )
        self.assertEqual(
            sum(symbol["source"]["path"] == "Source/Common/ImGuiExt/ImGuiStuff.h" for symbol in imgui_values),
            68,
        )
        style_alpha = next(
            symbol for symbol in imgui_values if symbol["id"] == "script.enum-value.ImGui_StyleVar.Alpha"
        )
        self.assertEqual(style_alpha["description"], "Global alpha applies to everything in Dear ImGui.")
        self.assertEqual(style_alpha["source"]["path"], "ThirdParty/imgui/imgui.h")
        key_codes = [
            symbol
            for symbol in model["symbols"]
            if symbol["kind"] == "enum-value" and symbol["receiver"] == "KeyCode"
        ]
        self.assertEqual(len(key_codes), 105)
        self.assertEqual([symbol["id"] for symbol in key_codes if not symbol["description"]], [])
        self.assertEqual(
            sum(symbol["source"]["path"] == "Source/Frontend/Application.cpp" for symbol in key_codes),
            103,
        )
        self.assertEqual(
            sum(symbol["source"]["path"] == "Source/Frontend/Application.h" for symbol in key_codes),
            2,
        )
        self.assertEqual(
            [symbol["id"] for symbol in model["symbols"] if not symbol["description"]],
            [],
        )
        self.assertEqual(model["summary"]["missing_description_count"], 0)
        critter_property = next(
            symbol for symbol in model["symbols"] if symbol["id"] == "script.property.Critter.Condition"
        )
        critter_property_id = next(
            symbol
            for symbol in model["symbols"]
            if symbol["id"] == "script.enum-value.CritterProperty.Condition"
        )
        self.assertEqual(critter_property_id["description"], critter_property["description"])
        self.assertEqual(critter_property_id["source"], critter_property["source"])
        for kind in ("ref-type", "ref-field", "ref-method"):
            with self.subTest(described_reference_kind=kind):
                self.assertEqual(
                    [symbol["id"] for symbol in model["symbols"] if symbol["kind"] == kind and not symbol["description"]],
                    [],
                )
        self.assertTrue(all(not Path(source).is_absolute() for source in model["metadata_source_files"]))
        self.assertEqual(model["summary"]["explicit_contract_declaration_count"], 2)
        self.assertEqual(model["summary"]["explicit_contract_symbol_count"], 2509)
        self.assertEqual(model["summary"]["default_contract_symbol_count"], 0)
        self.assertEqual(
            model["summary"]["symbols_by_stability"],
            {"experimental": 2508, "internal": 1},
        )
        debugger_symbol = next(
            symbol for symbol in model["symbols"] if symbol["id"] == "script.method.common.Game.BreakIntoDebugger"
        )
        self.assertEqual(debugger_symbol["stability"], "internal")
        self.assertTrue(debugger_symbol["contract"]["explicit"])
        experimental_symbols = [symbol for symbol in model["symbols"] if symbol["stability"] == "experimental"]
        self.assertTrue(all(symbol["since"] == "2022.1.0.wip" for symbol in experimental_symbols))
        self.assertTrue(all(symbol["contract"]["selector"] == "scope:native-codegen" for symbol in experimental_symbols))

    def test_scope_contract_pins_inventory_and_allows_exact_override_without_hash_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source_path = root / "Source/Scripting/TestMethods.cpp"
            source_path.parent.mkdir(parents=True)
            source_path.write_text(
                "///@ ExportEntity Game ServerEngine ClientEngine Global\n"
                "///@ ExportMethod\n"
                "FO_SCRIPT_API bool Common_Game_IsReady(ptr<BaseEngine> engine);\n"
                "///@ ExportMethod\n"
                "FO_SCRIPT_API void Common_Game_Debug(ptr<BaseEngine> engine);\n",
                encoding="utf-8",
            )

            baseline = docs_api.generate_api_model(root)
            baseline_hash = docs_api.codegen.compatibility_hasher.hexdigest()
            symbol_ids = sorted(str(symbol["id"]) for symbol in baseline["symbols"])
            inventory_sha256 = hashlib.sha256("\n".join(symbol_ids).encode("utf-8")).hexdigest()
            contract_path = root / "Source/Common/ApiContracts.inc"
            contract_path.parent.mkdir(parents=True)

            contract_path.write_text(
                "/// Reviewed scope.\n"
                f"///@ ApiContract scope:native-codegen experimental Since=0.1.0 SymbolCount={len(symbol_ids) + 1} "
                f"InventorySha256={inventory_sha256}\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "SymbolCount is stale"):
                docs_api.generate_api_model(root)

            contract_path.write_text(
                "/// Reviewed scope.\n"
                f"///@ ApiContract scope:native-codegen experimental Since=0.1.0 SymbolCount={len(symbol_ids)} "
                f"InventorySha256={'0' * 64}\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "InventorySha256 is stale"):
                docs_api.generate_api_model(root)

            contract_path.write_text(
                "/// Reviewed scope.\n"
                f"///@ ApiContract scope:native-codegen experimental Since=0.1.0 SymbolCount={len(symbol_ids)} "
                f"InventorySha256={inventory_sha256}\n"
                "/// Development-only override.\n"
                "///@ ApiContract script.method.common.Game.Debug internal\n",
                encoding="utf-8",
            )
            model = docs_api.generate_api_model(root)
            classified_hash = docs_api.codegen.compatibility_hasher.hexdigest()

            self.assertEqual(classified_hash, baseline_hash)
            self.assertEqual(model["scope"]["default_stability"], "experimental")
            self.assertEqual(model["summary"]["explicit_contract_declaration_count"], 2)
            self.assertEqual(model["summary"]["explicit_contract_symbol_count"], len(symbol_ids))
            self.assertEqual(model["summary"]["default_contract_symbol_count"], 0)
            self.assertEqual(
                next(symbol for symbol in model["symbols"] if symbol["id"] == "script.method.common.Game.Debug")[
                    "stability"
                ],
                "internal",
            )
            self.assertTrue(
                all(
                    symbol["stability"] == "experimental"
                    for symbol in model["symbols"]
                    if symbol["id"] != "script.method.common.Game.Debug"
                )
            )

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
        self.assertEqual(parsed["scope"]["default_stability"], "experimental")
        self.assertEqual(
            parsed["summary"]["symbol_count"],
            parsed["summary"]["described_symbol_count"] + parsed["summary"]["missing_description_count"],
        )


if __name__ == "__main__":
    unittest.main()
