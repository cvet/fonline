from __future__ import annotations

import copy
import io
import json
import re
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path, PurePosixPath


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

import docs_cmake  # noqa: E402
import docs_description_translations  # noqa: E402
import docs_localization  # noqa: E402


def _manifest() -> dict[str, object]:
    return json.loads((ENGINE_ROOT / docs_cmake.DEFAULT_MANIFEST).read_text(encoding="utf-8"))


def _write_fixture(root: Path, manifest: dict[str, object]) -> None:
    manifest_path = root / docs_cmake.DEFAULT_MANIFEST
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    sources = [entry["source"] for key in ("stages", "helpers") for entry in manifest[key]]
    for source in sources:
        relative = PurePosixPath(source)
        source_path = root.joinpath(*relative.parts)
        source_path.parent.mkdir(parents=True, exist_ok=True)
        source_path.write_text("# fixture\n", encoding="utf-8")

    catalog_path = root / docs_description_translations.DEFAULT_CATALOG
    catalog_path.parent.mkdir(parents=True, exist_ok=True)
    catalog_path.write_text(
        json.dumps(
            {
                "schema_version": docs_description_translations.SCHEMA_VERSION,
                "source_locale": "en",
                "target_locale": "ru",
                "enforcement": "registered-translations-current",
                "domains": {},
            }
        )
        + "\n",
        encoding="utf-8",
    )


class DocumentationCMakeTests(unittest.TestCase):
    def test_current_interface_model_has_stable_shape_and_ids(self) -> None:
        model = docs_cmake.generate_cmake_model(ENGINE_ROOT)

        self.assertEqual(model["schema_version"], 1)
        self.assertEqual(model["generated_by"], "BuildTools/docs_cmake.py")
        self.assertEqual(model["summary"]["option_count"], 44)
        self.assertEqual(model["summary"]["required_option_count"], 9)
        self.assertEqual(model["summary"]["stage_count"], 10)
        self.assertEqual(model["summary"]["helper_count"], 7)
        self.assertEqual(model["stages"][0]["id"], "cmake.stage.Init")
        self.assertEqual(model["stages"][-1]["entrypoint"], "FinalizeProjectGeneration")
        self.assertEqual(model["options"][0]["id"], "cmake.option.FO_MAIN_CONFIG")
        self.assertEqual(model["helpers"][0]["id"], "cmake.helper.SetOption")
        options = {entry["name"]: entry for entry in model["options"]}
        self.assertEqual(options["FO_SPARK_PARTICLES"]["default"], "OFF")
        self.assertEqual(options["FO_EFFEKSEER_PARTICLES"]["default"], "OFF")
        helpers = {entry["name"]: entry for entry in model["helpers"]}
        self.assertEqual(
            helpers["AddBakingTarget"]["signature"],
            "AddBakingTarget(<target> [SUB_CONFIG <name>] [FORCE] [COMMENT <text>])",
        )
        self.assertEqual(
            helpers["AddBakingTarget"]["source"],
            "BuildTools/cmake/helpers/Build.cmake",
        )

        identities = [
            entry["id"]
            for key in ("options", "stages", "helpers")
            for entry in model[key]
        ]
        self.assertEqual(len(identities), len(set(identities)))

    def test_add_baking_target_implementation_matches_public_contract(self) -> None:
        helper_source = (
            ENGINE_ROOT / "BuildTools/cmake/helpers/Build.cmake"
        ).read_text(encoding="utf-8")

        for expected in (
            "function(AddBakingTarget target)",
            "set(options FORCE)",
            "set(oneValueArgs SUB_CONFIG COMMENT)",
            'set(BAKING_TARGET_SUB_CONFIG "NONE")',
            'set(BAKING_TARGET_COMMENT "Bake resources")',
            "COMMAND ${bakeResources} -ForceBaking ${forceBaking}",
            "DEPENDS ForceCodeGeneration",
            "WORKING_DIRECTORY ${FO_OUTPUT_PATH}",
        ):
            with self.subTest(expected=expected):
                self.assertIn(expected, helper_source)

        stage_source = (
            ENGINE_ROOT / "BuildTools/cmake/stages/ScriptsAndBaking.cmake"
        ).read_text(encoding="utf-8")
        self.assertIn("AddBakingTarget(BakeResources)", stage_source)
        self.assertIn("AddBakingTarget(ForceBakeResources FORCE)", stage_source)

    def test_manifest_validation_rejects_contract_drift(self) -> None:
        cases = []

        duplicate_option = _manifest()
        duplicate_option["options"][1]["name"] = duplicate_option["options"][0]["name"]
        cases.append((duplicate_option, "duplicate CMake project option"))

        invalid_default = _manifest()
        invalid_default["options"][0]["default"] = "project.fomain"
        cases.append((invalid_default, "must be STRING with a null default"))

        invalid_boolean = _manifest()
        bool_option = next(option for option in invalid_boolean["options"] if option["cache_type"] == "BOOL")
        bool_option["default"] = "MAYBE"
        cases.append((invalid_boolean, "must be optional with an ON or OFF default"))

        invalid_order = _manifest()
        invalid_order["stages"][1]["order"] = 7
        cases.append((invalid_order, "must be the contiguous value 2"))

        duplicate_helper = _manifest()
        duplicate_helper["helpers"][1]["name"] = duplicate_helper["helpers"][0]["name"]
        duplicate_helper["helpers"][1]["signature"] = duplicate_helper["helpers"][0]["signature"]
        cases.append((duplicate_helper, "duplicate CMake project helper"))

        for manifest, expected in cases:
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as temp_dir:
                root = Path(temp_dir)
                _write_fixture(root, manifest)
                with self.assertRaisesRegex(ValueError, expected):
                    docs_cmake.generate_cmake_model(root)

    def test_manifest_validation_rejects_missing_or_unsafe_sources(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            manifest = _manifest()
            _write_fixture(root, manifest)

            manifest["stages"][0]["source"] = "BuildTools/cmake/stages/Missing.cmake"
            (root / docs_cmake.DEFAULT_MANIFEST).write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "does not exist"):
                docs_cmake.generate_cmake_model(root)

            manifest["stages"][0]["source"] = "../outside.cmake"
            (root / docs_cmake.DEFAULT_MANIFEST).write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "repository-relative"):
                docs_cmake.generate_cmake_model(root)

    def test_reference_pages_are_deterministic_escaped_and_cover_every_id(self) -> None:
        model = docs_cmake.generate_cmake_model(ENGINE_ROOT)
        pages = docs_cmake.generate_reference_pages(model)

        self.assertEqual(tuple(sorted(pages)), tuple(sorted(docs_cmake.OUTPUT_PATHS)))
        self.assertEqual(pages, docs_cmake.generate_reference_pages(copy.deepcopy(model)))
        page_for_key = {
            "options": pages[f"{docs_cmake.DEFAULT_OUTPUT_DIR}/options.md"],
            "stages": pages[f"{docs_cmake.DEFAULT_OUTPUT_DIR}/stages.md"],
            "helpers": pages[f"{docs_cmake.DEFAULT_OUTPUT_DIR}/helpers.md"],
        }
        self.assertIn(
            "[package interface reference](../packages/index.md)",
            pages[f"{docs_cmake.DEFAULT_OUTPUT_DIR}/index.md"],
        )
        for key, page in page_for_key.items():
            for entry in model[key]:
                self.assertIn(str(entry["id"]), page)
                self.assertIn(docs_cmake._anchor("entry", str(entry["id"])), page)

        escaped_model = copy.deepcopy(model)
        escaped_model["options"][0]["description"] = "A | B {shape} <unsafe>"
        escaped_page = docs_cmake.generate_reference_pages(escaped_model)[
            f"{docs_cmake.DEFAULT_OUTPUT_DIR}/options.md"
        ]
        self.assertIn("A &#124; B &#123;shape&#125; &lt;unsafe&gt;", escaped_page)

        for filename, _, _ in docs_cmake.PAGE_DEFINITIONS:
            canonical = pages[f"{docs_cmake.DEFAULT_OUTPUT_DIR}/{filename}"]
            legacy = pages[f"{docs_cmake.LEGACY_OUTPUT_DIR}/{filename}"]
            self.assertIn(f"../../en/reference/cmake/{filename}", legacy)
            self.assertIn(f"../../ru/reference/cmake/{filename}", legacy)
            for heading in re.findall(r"^(#{2,3} .+)$", canonical, flags=re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)

        localized_pages = docs_cmake.render_reference_pages(ENGINE_ROOT)
        english_options = localized_pages[f"{docs_cmake.DEFAULT_OUTPUT_DIR}/options.md"]
        russian_options = localized_pages[f"{docs_cmake.RUSSIAN_OUTPUT_DIR}/options.md"]
        russian_stages = localized_pages[f"{docs_cmake.RUSSIAN_OUTPUT_DIR}/stages.md"]
        self.assertIn("Компилирует backend скриптов AngelScript.", russian_options)
        self.assertNotIn("Compile the AngelScript scripting backend.", russian_options)
        self.assertIn("Создаёт цели компиляции скриптов и запекания ресурсов.", russian_stages)
        self.assertIn(
            docs_localization.translation_metadata_line(
                "generated-cmake-options",
                f"{docs_cmake.DEFAULT_OUTPUT_DIR}/options.md",
                docs_localization.normalized_sha256(english_options),
            ),
            russian_options,
        )

    def test_cli_write_check_and_stale_detection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            _write_fixture(root, _manifest())
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                self.assertEqual(docs_cmake.main(["--root", str(root), "--write"]), 0)
                self.assertEqual(docs_cmake.main(["--root", str(root), "--check"]), 0)

                stale_page = root / docs_cmake.OUTPUT_PATHS[0]
                stale_page.write_text(stale_page.read_text(encoding="utf-8") + "stale\n", encoding="utf-8")
                self.assertEqual(docs_cmake.main(["--root", str(root), "--check"]), 1)
            self.assertIn("missing or stale", output.getvalue())


if __name__ == "__main__":
    unittest.main()
