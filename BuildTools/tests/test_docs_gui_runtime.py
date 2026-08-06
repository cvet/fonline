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
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_gui_runtime  # noqa: E402
import docs_localization  # noqa: E402


class GuiRuntimeDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_gui_runtime.generate_gui_runtime_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_contract(self) -> None:
        second = docs_gui_runtime.generate_gui_runtime_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(
            self.model["schema_version"], docs_gui_runtime.SCHEMA_VERSION
        )
        outputs = self.model["outputs"]
        self.assertEqual(outputs["type_count"], 12)
        self.assertEqual(outputs["api_member_count"], 159)
        self.assertEqual(outputs["callback_signature_count"], 39)
        self.assertEqual(outputs["screen_api_overload_count"], 31)
        self.assertEqual(outputs["annotation_count"], 8)
        self.assertEqual(outputs["panel_scroll_animation_ms"], 120)
        self.assertEqual(outputs["password_reveal_ms"], 1000)
        self.assertEqual(outputs["press_repeat_initial_ms"], 500)
        self.assertEqual(outputs["press_repeat_interval_ms"], 40)
        self.assertEqual(outputs["engine_authored_file_formats"], [])
        self.assertEqual(outputs["focused_native_test_files"], [])
        self.assertEqual(self.model["summary"]["entry_count"], 82)

    def test_type_hierarchy_members_and_callbacks_are_complete(self) -> None:
        types = {
            entry["name"]: entry
            for entry in self.model["types"]
        }

        self.assertEqual(
            list(types),
            [
                "Object",
                "Panel",
                "Text",
                "TextInput",
                "Button",
                "CheckBox",
                "RadioButton",
                "Screen",
                "Grid",
                "MessageBox",
                "Console",
                "ItemView",
            ],
        )
        self.assertIsNone(types["Object"]["base"])
        self.assertEqual(types["Panel"]["base"], "Object")
        self.assertEqual(types["ItemView"]["base"], "Grid")
        self.assertIn(
            "MessageBoxType[] MessageTypes",
            types["MessageBox"]["members"],
        )
        self.assertIn(
            "void SetHasOnDraw(bool enabled)",
            types["Object"]["members"],
        )
        self.assertIn(
            "void OnMove(ipos deltaPos)",
            types["Object"]["callbacks"],
        )
        self.assertIn(
            "Item[] OnGetItems() - return all items for display",
            types["ItemView"]["callbacks"],
        )

    def test_screen_api_and_annotations_are_source_derived(self) -> None:
        signatures = {
            entry["signature"] for entry in self.model["screen_api"]
        }
        self.assertIn(
            "void RegisterScreen(GuiScreen screenNum, "
            "CreateScreenFunc screenFunc)",
            signatures,
        )
        self.assertIn(
            "void ShowScreen(GuiScreen screenNum, dict<string, any> params)",
            signatures,
        )
        self.assertIn("Screen? GetActiveScreen()", signatures)
        annotations = {
            entry["name"]: entry for entry in self.model["annotations"]
        }
        self.assertEqual(
            annotations["AnchorStyle"]["values"],
            {"None": 0, "Left": 1, "Right": 2, "Top": 4, "Bottom": 8},
        )
        self.assertEqual(
            annotations["DockStyle"]["values"],
            {
                "None": 0,
                "Left": 1,
                "Right": 2,
                "Top": 3,
                "Bottom": 4,
                "Fill": 5,
            },
        )
        self.assertEqual(
            annotations["Game.OnScreenChange"]["kind"], "event"
        )

    def test_input_and_project_hook_boundary_is_pinned(self) -> None:
        outputs = self.model["outputs"]
        self.assertEqual(
            outputs["input_subscriptions"],
            [
                "Game.OnMouseDown",
                "Game.OnMouseUp",
                "Game.OnMouseMove",
                "Game.OnKeyDown",
                "Game.OnKeyUp",
                "Game.OnInputLost",
            ],
        )
        self.assertEqual(
            outputs["repeatable_keys"],
            ["Text", "Space", "Back", "Delete", "Left", "Right"],
        )
        self.assertEqual(outputs["key_state_size"], 256)
        self.assertEqual(
            outputs["mouse_state_size_expression"],
            "MouseButton::Ext4 + 1",
        )
        self.assertEqual(
            outputs["external_screen_initializer"],
            "GuiScreens::InitializeScreens",
        )
        self.assertIn(
            "Callback_OnResolutionChanged", outputs["integration_hooks"]
        )
        self.assertIn(
            "Callback_OnLanguageChanged", outputs["integration_hooks"]
        )

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in docs_gui_runtime.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (
                        ENGINE_ROOT / source["path"]
                    ).read_text(encoding="utf-8", errors="replace")
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_runtime_and_project_boundary(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/how-to/runtime/gui.md").read_text(
            encoding="utf-8"
        )

        for heading in (
            "## What Engine provides",
            "## Required project contract",
            "## Client event bridge",
            "## Object model",
            "## Layout contract",
            "## Screen registration and lifetime",
            "## Drawing order",
            "## Mouse input",
            "## Keyboard and focus",
            "## Touch boundary",
            "## Authoring approaches",
            "## Diagnostics",
            "## Validation workflow",
            "## Project boundary",
            "## Maintenance",
        ):
            self.assertIn(heading, guide)
        self.assertIn("does **not** parse `.fogui`", guide)
        self.assertIn("GuiScreens::InitializeScreens", guide)
        self.assertIn("no self-contained", guide)
        self.assertIn("native fixture in Engine", guide)
        self.assertIn("Headless success is not visual", guide)

    def test_changed_derived_manifest_values_are_rejected(self) -> None:
        manifest = json.loads(
            (
                ENGINE_ROOT / docs_gui_runtime.DEFAULT_MANIFEST
            ).read_text(encoding="utf-8")
        )
        manifest["outputs"] = copy.deepcopy(manifest["outputs"])
        manifest["outputs"]["press_repeat_interval_ms"] = 50

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = (
                Path(temporary_directory) / "GuiRuntimeInterface.json"
            )
            manifest_path.write_text(
                json.dumps(manifest), encoding="utf-8"
            )
            with self.assertRaisesRegex(
                ValueError,
                "outputs.press_repeat_interval_ms must match the live source",
            ):
                docs_gui_runtime.generate_gui_runtime_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_gui_runtime.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_gui_runtime.OUTPUT_PATHS))
        self.assertIn(
            "Runtime types | 12",
            pages["Docs/en/reference/gui-runtime/index.md"],
        )
        self.assertIn(
            "MessageBoxType[] MessageTypes",
            pages["Docs/en/reference/gui-runtime/types.md"],
        )
        self.assertIn(
            "gui-runtime.screen-api.register-screen",
            pages["Docs/en/reference/gui-runtime/screen-api.md"],
        )
        self.assertIn(
            "Show and hide callback order",
            pages["Docs/en/reference/gui-runtime/lifecycle.md"],
        )
        self.assertIn(
            "9-slice frame",
            pages["Docs/en/reference/gui-runtime/layout-rendering.md"],
        )
        self.assertIn(
            "Pressed-object repeat",
            pages["Docs/en/reference/gui-runtime/input.md"],
        )
        self.assertIn(
            "No Engine declarative GUI format",
            pages[
                "Docs/en/reference/gui-runtime/integration-validation.md"
            ],
        )
        self.assertIn(
            "Сгенерированный справочник GUI Runtime",
            pages["Docs/ru/reference/gui-runtime/index.md"],
        )
        self.assertIn(
            "Порядок callback-функций show и hide",
            pages["Docs/ru/reference/gui-runtime/lifecycle.md"],
        )
        self.assertNotIn(
            "Registration, stack, focus, lookup, and drag/drop callables.",
            pages["Docs/ru/reference/gui-runtime/index.md"],
        )
        with contextlib.redirect_stdout(
            io.StringIO()
        ), contextlib.redirect_stderr(io.StringIO()):
            result = docs_gui_runtime.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)

    def test_russian_pages_pin_english_hashes_and_preserve_commands(self) -> None:
        pages = docs_gui_runtime.render_reference_pages(ENGINE_ROOT)
        for (_, document_id, _), english_path, russian_path in zip(
            docs_gui_runtime.PAGE_DEFINITIONS,
            docs_gui_runtime.CANONICAL_OUTPUT_PATHS,
            docs_gui_runtime.RUSSIAN_OUTPUT_PATHS,
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

    def test_legacy_generated_routes_preserve_headings_and_entry_anchors(self) -> None:
        pages = docs_gui_runtime.generate_reference_pages(self.model)
        for canonical_path, legacy_path in zip(
            docs_gui_runtime.CANONICAL_OUTPUT_PATHS,
            docs_gui_runtime.LEGACY_OUTPUT_PATHS,
            strict=True,
        ):
            canonical = pages[canonical_path]
            legacy = pages[legacy_path]
            self.assertEqual(
                re.findall(r'<a id="([^"]+)"></a>', canonical),
                re.findall(r'<a id="([^"]+)"></a>', legacy),
            )
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            self.assertIn("../../en/reference/gui-runtime/", legacy)
            self.assertIn("../../ru/reference/gui-runtime/", legacy)

    def test_ci_manifest_and_contract_diff_route_the_domain(self) -> None:
        workflow = (
            ENGINE_ROOT / ".github/workflows/validate.yml"
        ).read_text(encoding="utf-8")
        manifest = json.loads(
            (
                ENGINE_ROOT / "Docs/documentation-manifest.json"
            ).read_text(encoding="utf-8")
        )
        contract_diff = (
            ENGINE_ROOT / "BuildTools/docs_contract_diff.py"
        ).read_text(encoding="utf-8")
        validate = (
            ENGINE_ROOT / "BuildTools/docs_validate.py"
        ).read_text(encoding="utf-8")

        self.assertIn(
            "BuildTools/tests/test_docs_gui_runtime.py", workflow
        )
        self.assertIn(
            "BuildTools/docs_gui_runtime.py --check", workflow
        )
        document_ids = {
            document["id"] for document in manifest["documents"].values()
        }
        self.assertIn("gui-runtime-guide", document_ids)
        self.assertIn("generated-gui-runtime-index", document_ids)
        guide = manifest["documents"]["Docs/en/how-to/runtime/gui.md"]
        self.assertEqual(guide["disposition"], "retain")
        legacy_guide = manifest["documents"]["Docs/GuiRuntime.md"]
        self.assertEqual(legacy_guide["state"], "redirect")
        self.assertEqual(legacy_guide["redirect_to"], "gui-runtime-guide")
        for canonical_path, legacy_path in zip(
            docs_gui_runtime.CANONICAL_OUTPUT_PATHS,
            docs_gui_runtime.LEGACY_OUTPUT_PATHS,
            strict=True,
        ):
            canonical = manifest["documents"][canonical_path]
            legacy = manifest["documents"][legacy_path]
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["redirect_to"], canonical["id"])
        generated_paths = manifest["generated_artifacts"]["gui_runtime_reference"][
            "paths"
        ]
        self.assertTrue(
            set(docs_gui_runtime.RUSSIAN_OUTPUT_PATHS).issubset(generated_paths)
        )
        self.assertIn('"gui-runtime"', contract_diff)
        self.assertIn("docs_gui_runtime", validate)


if __name__ == "__main__":
    unittest.main()
