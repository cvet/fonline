from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_inventory  # noqa: E402


class DocumentationInventoryTests(unittest.TestCase):
    def test_inventory_is_deterministic_and_source_backed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "Source/Scripting").mkdir(parents=True)
            (root / "Source/Tests").mkdir(parents=True)
            (root / "Source/Common").mkdir(parents=True)
            (root / "Source/Scripting/CommonGlobalScriptMethods.cpp").write_text(
                "///@ ExportMethod\nvoid First();\n///@ ExportMethod\nvoid Second();\n",
                encoding="utf-8",
            )
            (root / "Source/Scripting/ServerMapScriptMethods.cpp").write_text("// no exports\n", encoding="utf-8")
            (root / "Source/Tests/Test_Alpha.cpp").write_text("// test\n", encoding="utf-8")
            (root / "Source/Tests/Test_Beta.cpp").write_text("// test\n", encoding="utf-8")
            (root / "Source/Common/Settings.inc").write_text(
                "FIXED_SETTING(bool, Test, Fixed, false);\n"
                "VARIABLE_SETTING(string, Test, Variable, \"\");\n",
                encoding="utf-8",
            )

            inventory = docs_inventory.generate_inventory(root)

            self.assertEqual(inventory["script_api"]["export_method_declarations"], 2)
            self.assertEqual(inventory["script_api"]["method_file_count"], 2)
            self.assertEqual(inventory["engine_tests"]["test_file_count"], 2)
            self.assertEqual(inventory["settings"]["declaration_count"], 2)
            self.assertEqual(
                inventory["engine_tests"]["files"],
                ["Source/Tests/Test_Alpha.cpp", "Source/Tests/Test_Beta.cpp"],
            )

    def test_rendered_inventory_has_stable_json_format(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "Source/Scripting").mkdir(parents=True)
            (root / "Source/Tests").mkdir(parents=True)
            (root / "Source/Common").mkdir(parents=True)
            (root / "Source/Common/Settings.inc").write_text("", encoding="utf-8")

            rendered = docs_inventory.render_inventory(root)

            self.assertTrue(rendered.endswith("\n"))
            self.assertEqual(json.loads(rendered)["generated_by"], "BuildTools/docs_inventory.py")


if __name__ == "__main__":
    unittest.main()
