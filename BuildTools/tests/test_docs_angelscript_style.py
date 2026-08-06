from __future__ import annotations

import json
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

import buildtools  # noqa: E402


EN_GUIDE = "Docs/en/how-to/scripting/style-and-refactoring.md"
RU_GUIDE = "Docs/ru/how-to/scripting/style-and-refactoring.md"
LEGACY_GUIDE = "Docs/AngelScriptStyle.md"


class AngelScriptStyleDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    @staticmethod
    def _fences(markdown: str) -> list[str]:
        return re.findall(r"^```[^\n]*\n.*?^```$", markdown, flags=re.MULTILINE | re.DOTALL)

    def test_guides_define_reusable_scope_and_locale_parity(self) -> None:
        english = self._read(EN_GUIDE)
        russian = self._read(RU_GUIDE)
        legacy = self._read(LEGACY_GUIDE)

        headings = (
            "## Contract status",
            "## Scope and ownership",
            "## Fast convention",
            "## How scripts become a module",
            "### File discovery and ordering",
            "### Side-specific compilation",
            "## Formatter contract",
            "### What the wrapper repairs",
            "### Mutable state and globals",
            "## Attributes and callback ownership",
            "### Direct-call blockers",
            "### Marker propagation",
            "## Generated script ownership",
            "## Refactoring classification",
            "## Safe batch workflow",
            "## Validation matrix",
            "## Failure diagnosis",
            "## Project policy boundary",
            "## Maintenance triggers",
            "## Source paths inspected",
        )
        for heading in headings:
            self.assertIn(heading, english)
            self.assertIn(heading, legacy)

        for contract in (
            "stable ascending sort by numeric value",
            "SERVER=1",
            "#ifdef SERVER",
            "clang-format 20",
            "FO_CLANG_FORMAT",
            "Script.MutableGlobalsAllowedNamespaces",
            "Script.AttributedFunctionDirectCallAllowedNamespaces",
            "generated `.fos` file needs an upstream owner",
            "warnings as failures",
            "Small batches",
            "not an Engine recommendation",
        ):
            self.assertIn(contract, english)

        self.assertIn('document_id: angelscript-style', english)
        self.assertIn('document_id: angelscript-style', russian)
        self.assertIn('source_path":"Docs/en/how-to/scripting/style-and-refactoring.md"', russian)
        self.assertEqual(self._fences(english), self._fences(russian))
        self.assertEqual(len(re.findall(r"^#{2,3} ", english, flags=re.MULTILINE)), 40)
        self.assertEqual(len(re.findall(r"^#{2,3} ", russian, flags=re.MULTILINE)), 40)
        self.assertIn("[English](en/how-to/scripting/style-and-refactoring.md)", legacy)
        self.assertIn("[Russian](ru/how-to/scripting/style-and-refactoring.md)", legacy)

    def test_backend_pins_file_order_side_modules_and_mutable_globals(self) -> None:
        backend = self._read("Source/Scripting/AngelScript/AngelScriptBackend.cpp")
        settings = self._read("Source/Common/Settings.inc")
        baker_tests = self._read("Source/Tests/Test_AngelScriptBaker.cpp")

        for contract in (
            'first_line.find("Sort ")',
            "std::ranges::stable_sort(final_script_files_order",
            'root_script.append("#include \\\""',
            'Preprocessor::Define(preprocessor_context.get(), "SERVER 1")',
            'Preprocessor::Define(preprocessor_context.get(), "CLIENT 1")',
            'Preprocessor::Define(preprocessor_context.get(), "MAPPER 1")',
            "IsScriptNamespaceAllowed(ns_view, _settings->MutableGlobalsAllowedNamespaces)",
        ):
            self.assertIn(contract, backend)

        self.assertIn("FIXED_SETTING(vector<string>, Script, MutableGlobalsAllowedNamespaces)", settings)
        self.assertIn("matched as prefixes", settings)
        self.assertIn("ScriptSettings::MutableGlobalsAllowedNamespaces", baker_tests)
        self.assertIn("ConstGlobalScripts", baker_tests)

    def test_attribute_contract_pins_blockers_markers_and_project_exceptions(self) -> None:
        attributes = self._read("Source/Scripting/AngelScript/AngelScriptAttributes.cpp")
        settings = self._read("Source/Common/Settings.inc")
        attribute_tests = self._read("Source/Tests/Test_AngelScriptAttributes.cpp")

        for name in (
            "Event",
            "TimeEvent",
            "AnimCallback",
            "PropertyGetter",
            "PropertySetter",
            "ServerRemoteCall",
            "ClientRemoteCall",
            "AdminRemoteCall",
            "ItemTrigger",
            "ItemStatic",
            "ModuleInit",
            "InvokeEntry",
        ):
            self.assertIn(f'string_view {{"{name}"}}', attributes)

        self.assertIn("caller must carry the same marker", attributes)
        self.assertIn("MakeMarkerPropagationError", attributes)
        self.assertIn("AttributedFunctionDirectCallAllowedNamespaces", settings)
        self.assertIn("ExtraDirectCallBlockingAttributes", settings)
        self.assertIn("RejectsDirectCallsToAttributedFunctions", attribute_tests)
        self.assertIn("viral marker", attribute_tests)

    def test_formatter_repairs_angelscript_without_touching_literals(self) -> None:
        formatted = (
            "Critter ? target;\n"
            "Critter ? Find(Critter ? fallback = null);\n"
            "auto@ value = cast < Critter ? > (target);\n"
            "Item ? [] items;\n"
            "Create(count : 2);\n"
            'string literal = "Create(count : 2) Critter ? target";\n'
            "// Create(count : 2) Critter ? target\n"
        )
        repaired = buildtools.fix_fos_nullable_suffix(formatted)

        self.assertIn("Critter? target;", repaired)
        self.assertIn("Critter? Find(Critter? fallback = null);", repaired)
        self.assertIn("cast<Critter?>(target)", repaired)
        self.assertIn("Item?[] items;", repaired)
        self.assertIn("Create(count: 2);", repaired)
        self.assertIn('"Create(count : 2) Critter ? target"', repaired)
        self.assertIn("// Create(count : 2) Critter ? target", repaired)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "Sample.fos"
            source.write_bytes(b"\xef\xbb\xbfnamespace Sample { }\r\n")
            with mock.patch.object(
                buildtools,
                "run_capture_text",
                return_value="namespace Sample\n{\n}\n",
            ):
                changed = buildtools.format_files("clang-format-20", root, ["**/*.fos"])

            self.assertEqual(changed, 1)
            self.assertEqual(source.read_bytes(), b"namespace Sample\r\n{\r\n}\r\n")

        formatter = self._read("BuildTools/buildtools.py")
        for contract in (
            "FO_CLANG_FORMAT",
            "('clang-format-20', 'clang-format')",
            "fix_fos_nullable_suffix",
            "int(match.group(1)) == 20",
        ):
            self.assertIn(contract, formatter)

    def test_core_scripts_follow_namespace_guard_and_file_contracts(self) -> None:
        core_root = ENGINE_ROOT / "Source/Scripting/AngelScript/CoreScripts"
        scripts = sorted(core_root.glob("*.fos"))
        self.assertGreaterEqual(len(scripts), 10)

        for script in scripts:
            with self.subTest(script=script.name):
                raw = script.read_bytes()
                self.assertFalse(raw.startswith(b"\xef\xbb\xbf"))
                normalized = raw.replace(b"\r\n", b"\n")
                self.assertTrue(normalized.endswith(b"\n"))
                self.assertFalse(normalized.endswith(b"\n\n"))

                text = raw.decode("utf-8")
                namespace = re.search(r"^namespace\s+([A-Za-z_]\w*)", text, re.MULTILINE)
                self.assertIsNotNone(namespace)
                self.assertEqual(namespace.group(1), script.stem)

                guard_depth = 0
                for line_number, line in enumerate(text.splitlines(), start=1):
                    directive = line.strip().split(maxsplit=1)[0] if line.strip() else ""
                    if directive in {"#if", "#ifdef", "#ifndef"}:
                        guard_depth += 1
                    elif directive == "#endif":
                        guard_depth -= 1
                    self.assertGreaterEqual(guard_depth, 0, f"unmatched #endif at {script}:{line_number}")
                self.assertEqual(guard_depth, 0, f"unclosed preprocessor guard in {script}")

        config = self._read("Source/Scripting/AngelScript/CoreScripts/.clang-format")
        workflow = self._read(".github/workflows/validate.yml")
        self.assertIn("ColumnLimit:     160", config)
        self.assertIn("InsertBraces:    true", config)
        self.assertIn("NamespaceIndentation: Inner", config)
        self.assertIn("SortIncludes:    Never", config)
        self.assertIn("python3 BuildTools/buildtools.py format-source", workflow)
        self.assertIn("git diff --exit-code", workflow)

    def test_guide_is_routed_localized_evaluated_and_promoted(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        document = manifest["documents"][EN_GUIDE]
        self.assertEqual(document["id"], "angelscript-style")
        self.assertEqual(document["classification"]["visibility"], "public")
        self.assertEqual(document["classification"]["translation"], "required")
        self.assertEqual(document["state"], "current")
        legacy = manifest["documents"][LEGACY_GUIDE]
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], "angelscript-style")
        self.assertIn("angelscript-style", manifest["ai_delivery"]["llms"]["start_document_ids"])
        script_group = next(
            group for group in manifest["site_delivery"]["navigation"] if group["id"] == "scripting"
        )
        self.assertIn("angelscript-style", script_group["document_ids"])

        evidence = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(
            record for record in evidence["records"] if record["id"] == "angelscript-style-and-refactoring"
        )
        self.assertEqual(record["disposition"], "promoted")
        self.assertIn(EN_GUIDE, record["engine_targets"])
        self.assertEqual(record["planned_targets"], [])
        sources = {(source["snapshot"], source["path"]) for source in record["sources"]}
        self.assertIn(("last-frontier", "LastFrontier.fomain"), sources)
        self.assertIn(("last-frontier", "Tools/Formatter/format_project.py"), sources)
        self.assertIn(("fonline-tla", "TLA.fomain"), sources)
        self.assertIn(("fonline-tla", "Tools/Formatter/format_project.py"), sources)
        self.assertIn("negative migration evidence", record["decision"])

        evaluation = json.loads(self._read("Docs/ai-evaluation.json"))
        task = next(task for task in evaluation["tasks"] if task["id"] == "scripting-style-refactoring")
        self.assertEqual(task["primary_document_id"], "angelscript-style")
        self.assertGreaterEqual(len(task["retrieval_checks"]), 2)
        self.assertGreaterEqual(len(task["answer_checks"]), 3)

    def test_maintenance_navigation_and_ci_route_to_canonical_guide(self) -> None:
        workflow = self._read(".github/workflows/validate.yml")
        validator = self._read("BuildTools/docs_validate.py")
        self.assertIn("python3 BuildTools/tests/test_docs_angelscript_style.py", workflow)
        self.assertIn("documentation AngelScript style workflow is missing", validator)

        for path in (
            "AGENTS.md",
            "Docs/en/index.md",
            "BuildTools/README.md",
            "Docs/en/contributing/documentation/index.md",
            "Docs/en/explanation/scripting-runtime/index.md",
            "Docs/en/how-to/build/generated-content.md",
        ):
            with self.subTest(path=path):
                self.assertIn("style-and-refactoring.md", self._read(path))

        self.assertIn("Триггеры сопровождения", self._read(RU_GUIDE))
        self.assertIn("Maintenance triggers", self._read(EN_GUIDE))


if __name__ == "__main__":
    unittest.main()
