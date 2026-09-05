from __future__ import annotations

import json
import re
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_inventory  # noqa: E402

ENGINE_ROOT = BUILDTOOLS_DIR.parent


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

    def test_test_file_order_uses_portable_posix_path_sorting(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "Source/Scripting").mkdir(parents=True)
            (root / "Source/Tests").mkdir(parents=True)
            (root / "Source/Common").mkdir(parents=True)
            (root / "Source/Common/Settings.inc").write_text("", encoding="utf-8")
            for name in ("Test_ImageBaker.cpp", "Test_ImGui.cpp"):
                (root / "Source/Tests" / name).write_text("// test\n", encoding="utf-8")

            inventory = docs_inventory.generate_inventory(root)

            self.assertEqual(
                inventory["engine_tests"]["files"],
                [
                    "Source/Tests/Test_ImGui.cpp",
                    "Source/Tests/Test_ImageBaker.cpp",
                ],
            )

    def test_unit_test_readmes_match_the_source_inventory(self) -> None:
        inventory = docs_inventory.generate_inventory(ENGINE_ROOT)
        expected = set(inventory["engine_tests"]["files"])

        for relative_path in ("Source/Tests/README.md", "Source/Tests/README.ru.md"):
            text = (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")
            listed = {
                match.group(0)
                for match in re.finditer(r"Source/Tests/Test_[A-Za-z0-9_]+\.cpp", text)
            }
            self.assertEqual(listed, expected, relative_path)


if __name__ == "__main__":
    unittest.main()
