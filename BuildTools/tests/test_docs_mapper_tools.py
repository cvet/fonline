from __future__ import annotations

import hashlib
import json
import re
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
EN_MANUAL = "Docs/en/how-to/tools/mapper-interactive.md"
RU_MANUAL = "Docs/ru/how-to/tools/mapper-interactive.md"
LEGACY_MANUAL = "Docs/MapperManual.md"
EN_TOOLS = "Docs/en/how-to/tools/mapper.md"
RU_TOOLS = "Docs/ru/how-to/tools/mapper.md"
LEGACY_TOOLS = "Docs/MapperTools.md"


class MapperToolsDocumentationTests(unittest.TestCase):
    @staticmethod
    def _read(relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    @staticmethod
    def _fences(markdown: str) -> list[str]:
        return re.findall(
            r"^```[^\n]*\n.*?^```$",
            markdown,
            flags=re.MULTILINE | re.DOTALL,
        )

    @staticmethod
    def _section_headings(markdown: str) -> list[str]:
        return re.findall(r"^#{2,3} .+$", markdown, flags=re.MULTILINE)

    def test_manual_matches_current_menu_and_hotkey_source(self) -> None:
        source = self._read("Source/Tools/Mapper.cpp")
        manual = self._read(EN_MANUAL)

        for marker in (
            'ImGui::BeginMenu("File")',
            'ImGui::BeginMenu("Windows")',
            'ImGui::BeginMenu("Edit")',
            'ImGui::BeginMenu("View")',
            'ImGui::BeginMenu("Tools")',
            'ImGui::BeginMenu("System")',
            'ImGui::MenuItem("Save current", "Ctrl+S"',
            'ImGui::MenuItem("Mark blocked hexes"',
            'dirty_label = "*** Save ***"',
            'ImGui::Button("Reset layout")',
        ):
            self.assertIn(marker, source)

        for marker in (
            "case KeyCode::D:",
            "cur_map->SetScrollCheck(!cur_map->IsScrollCheck());",
            "case KeyCode::B:",
            "MarkBlockedHexes();",
        ):
            self.assertIn(marker, source)

        for marker in (
            "`Ctrl+D`",
            "Toggle camera scroll checking",
            "`Ctrl+B`",
            "Tools -> Mark blocked hexes",
            "Hotkeys are suppressed while an ImGui text field is active.",
        ):
            self.assertIn(marker, manual)

    def test_layout_storage_and_reset_contract_match_source(self) -> None:
        source = self._read("Source/Tools/Mapper.cpp")
        header = self._read("Source/Tools/Mapper.h")
        manual = self._read(EN_MANUAL)

        for marker in (
            'MAPPER_IMGUI_SETTINGS_KEY = "ImGuiLayout"',
            "_uiSettings.Remove(MAPPER_IMGUI_SETTINGS_KEY)",
            'ImGui::LoadIniSettingsFromMemory("", 0)',
            "MapListWindowVisible = true",
            "MapWindowVisible = true",
            "_uiSettings.SetString(MAPPER_IMGUI_SETTINGS_KEY",
        ):
            self.assertIn(marker, source)
        self.assertIn('SettingsStorage _uiSettings {"Mapper"}', header)

        for marker in (
            "HKCU\\Software\\FOnline\\Mapper",
            "per-application user-data store",
            "Settings -> Reset layout",
        ):
            self.assertIn(marker, manual)

    def test_map_and_entity_exports_match_the_guide(self) -> None:
        bindings = self._read(
            "Source/Scripting/MapperGlobalScriptMethods.cpp"
        )
        guide = self._read(EN_TOOLS)

        methods = (
            "NewMap",
            "NewMapFromText",
            "LoadMap",
            "UnloadMap",
            "SaveMap",
            "SaveMapToPath",
            "ShowMap",
            "GetLoadedMaps",
            "GetMapFileNames",
            "ResizeMap",
            "AddItem",
            "AddCritter",
            "AddTile",
            "MoveEntity",
            "DeleteEntity",
            "SelectEntity",
            "FindEntityById",
            "SetEntityProperty",
        )
        for method in methods:
            self.assertIn(f"Mapper_Game_{method}", bindings, method)
            self.assertIn(f"Game.{method}", guide, method)

        for marker in (
            "Map name must not contain path separators",
            "Path traversal is not allowed",
            "mapper->SaveMapToDir(map, subDir, name)",
        ):
            self.assertIn(marker, bindings)
        self.assertIn("preserves sibling map blocks", guide)

    def test_view_capture_and_atlas_exports_match_the_guide(self) -> None:
        bindings = self._read(
            "Source/Scripting/MapperGlobalScriptMethods.cpp"
        )
        guide = self._read(EN_TOOLS)

        methods = (
            "GetCurMapHexSize",
            "GetCurMapPixelSize",
            "SetMapperViewSize",
            "CenterMapperOnPlayableArea",
            "CenterMapperOnHex",
            "CenterMapperOnRawHex",
            "SetMapperZoom",
            "CalcMapperFitZoom",
            "SetMapperOverlayVisible",
            "SetMapperHexOverlayVisible",
            "SetMapperHiddenSpritesVisible",
            "AddMapperIgnoredItemPids",
            "SetMapperScrollCheckEnabled",
            "SaveMapperScreenshot",
        )
        for method in methods:
            self.assertIn(f"Mapper_Game_{method}", bindings, method)
            self.assertIn(f"Game.{method}", guide, method)

        client_bindings = self._read(
            "Source/Scripting/ClientGlobalScriptMethods.cpp"
        )
        self.assertIn("Client_Game_DumpAtlases", client_bindings)
        self.assertIn("Game.DumpAtlases()", guide)

    def test_screenshot_lifecycle_and_tga_contract_match_source(self) -> None:
        source = self._read("Source/Scripting/MapperGlobalScriptMethods.cpp")
        manual = self._read(EN_MANUAL)
        guide = self._read(EN_TOOLS)

        for marker in (
            "Mapper_Game_SaveMapperScreenshot",
            "mapper->DrawMapperFrame();",
            "main_rt->GetTexture()",
            "texture->GetTextureRegion",
            "ImageWriter::WriteSimpleTga(filePath, size, std::move(pixels))",
        ):
            self.assertIn(marker, source)

        normalized_manual = " ".join(manual.split())
        for marker in (
            "Synchronous TGA write",
            "platform screenshot tool",
            "application-level ImGui composition",
        ):
            self.assertIn(marker, normalized_manual)
        for marker in (
            "map-only path",
            "platform screenshot tool",
            "non-uniform test map and pixel inspection",
            "Render.HeadlessWindow=True",
        ):
            self.assertIn(marker, guide)

    def test_map_item_animation_freeze_is_documented(self) -> None:
        source = self._read("Source/Client/ItemHexView.cpp")
        guide = self._read(EN_TOOLS)

        self.assertIn("void ItemHexView::RefreshAnim()", source)
        self.assertIn("if (_map->IsMapperMode())", source)
        self.assertIn("SetTime(0.0f)", source)
        self.assertIn("at time `0.0`", guide)
        self.assertIn("representative client scene", guide)

    def test_locale_guides_and_legacy_routes_preserve_contracts(self) -> None:
        pairs = (
            (EN_MANUAL, RU_MANUAL, LEGACY_MANUAL),
            (EN_TOOLS, RU_TOOLS, LEGACY_TOOLS),
        )
        for english_path, russian_path, legacy_path in pairs:
            english = self._read(english_path)
            russian = self._read(russian_path)
            legacy = self._read(legacy_path)

            self.assertIn(f"document_id: ", english)
            self.assertIn("locale: en", english)
            self.assertIn("locale: ru", russian)
            self.assertIn("docs-translation:", russian)
            self.assertEqual(self._fences(english), self._fences(russian))
            for heading in self._section_headings(english):
                self.assertIn(heading, legacy)
            self.assertIn(english_path.removeprefix("Docs/"), legacy)
            self.assertIn(russian_path.removeprefix("Docs/"), legacy)

            digest = hashlib.sha256(
                english.replace("\r\n", "\n")
                .replace("\r", "\n")
                .encode("utf-8")
            ).hexdigest()
            self.assertIn(f'"source_sha256":"{digest}"', russian)

        for heading in (
            "## Ориентация в интерфейсе",
            "## Справочник клавиш",
            "## Контракт снимков и автоматизации",
        ):
            self.assertIn(heading, self._read(RU_MANUAL))
        for heading in (
            "## Граница владения",
            "## Интеграция headless-захвата",
            "## Контракт снимка",
        ):
            self.assertIn(heading, self._read(RU_TOOLS))

    def test_minimal_fixture_and_screenshot_provenance_are_pinned(self) -> None:
        config = self._read(
            "Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain"
        )
        screenshots = json.loads(
            self._read("BuildTools/DocumentationScreenshots.json")
        )
        entry = next(
            item
            for item in screenshots["screenshots"]
            if item["id"] == "mapper-particle-preview"
        )

        for marker in (
            "Name = MapperDocumentationCapture",
            "Mapper.StartMap = TutorialMap",
            "Mapper.ParticlePreviewEffect = Documentation.spk",
            "Mapper.ParticlePreviewSeed",
        ):
            self.assertIn(marker, config)

        self.assertEqual(entry["owning_document"], EN_MANUAL)
        self.assertEqual(entry["width"], 1280)
        self.assertEqual(entry["height"], 800)
        self.assertIn("platform screenshot tool", " ".join(entry["capture"]["interaction_steps"]))
        self.assertTrue((ENGINE_ROOT / entry["path"]).is_file())
        for path in entry["source_paths"]:
            self.assertTrue((ENGINE_ROOT / path).exists(), path)

    def test_manifest_navigation_ai_and_evidence_use_canonical_routes(
        self,
    ) -> None:
        manifest = json.loads(
            self._read("Docs/documentation-manifest.json")
        )
        for path, document_id in (
            (EN_MANUAL, "mapper-interactive-manual"),
            (EN_TOOLS, "mapper-tools"),
        ):
            document = manifest["documents"][path]
            self.assertEqual(document["id"], document_id)
            self.assertEqual(document["state"], "current")
            self.assertEqual(document["disposition"], "retain")
            self.assertEqual(document["target"], path)
            self.assertIn("ai-agent", document["audiences"])
            for source in document["sources"]:
                self.assertTrue((ENGINE_ROOT / source).exists(), source)

        for path, redirect_to, target in (
            (LEGACY_MANUAL, "mapper-interactive-manual", EN_MANUAL),
            (LEGACY_TOOLS, "mapper-tools", EN_TOOLS),
        ):
            legacy = manifest["documents"][path]
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["disposition"], "replace")
            self.assertEqual(legacy["redirect_to"], redirect_to)
            self.assertEqual(legacy["target"], target)

        content = next(
            group
            for group in manifest["site_delivery"]["navigation"]
            if group["id"] == "content"
        )
        self.assertIn("mapper-interactive-manual", content["document_ids"])
        self.assertIn("mapper-tools", content["document_ids"])
        self.assertIn(
            "mapper-interactive-manual",
            manifest["ai_delivery"]["llms"]["start_document_ids"],
        )

        evaluation = json.loads(self._read("Docs/ai-evaluation.json"))
        task = next(
            task
            for task in evaluation["tasks"]
            if task["id"] == "automate-mapper-map-capture"
        )
        self.assertEqual(task["primary_document_id"], "mapper-tools")
        self.assertGreaterEqual(len(task["retrieval_checks"]), 2)
        self.assertGreaterEqual(len(task["answer_checks"]), 4)

        evidence = json.loads(
            self._read("BuildTools/ExternalProjectEvidence.json")
        )
        record = next(
            record
            for record in evidence["records"]
            if record["id"] == "mapper-and-focused-viewers"
        )
        self.assertEqual(record["disposition"], "promoted")
        self.assertIn(EN_MANUAL, record["engine_targets"])
        self.assertIn(EN_TOOLS, record["engine_targets"])
        sources = {
            (source["snapshot"], source["path"])
            for source in record["sources"]
        }
        self.assertIn(
            ("last-frontier", "Scripts/MapperRender.fos"), sources
        )
        self.assertIn(
            ("last-frontier", "Tools/MapPreview/generate_map_preview.py"),
            sources,
        )
        self.assertIn(("fonline-tla", "README.md"), sources)

    def test_current_entry_points_and_ci_use_canonical_routes(self) -> None:
        expected = {
            "AGENTS.md": (EN_MANUAL, EN_TOOLS),
            "README.md": (EN_MANUAL, EN_TOOLS),
            "README.ru.md": (RU_MANUAL, RU_TOOLS),
            "Docs/en/index.md": (
                "how-to/tools/mapper-interactive.md",
                "how-to/tools/mapper.md",
            ),
        }
        for path, markers in expected.items():
            text = self._read(path)
            for marker in markers:
                self.assertIn(marker, text, path)

        workflow = self._read(".github/workflows/validate.yml")
        validator = self._read("BuildTools/docs_validate.py")
        command = "python3 BuildTools/tests/test_docs_mapper_tools.py"
        self.assertIn(command, workflow)
        self.assertIn("test_docs_mapper_tools.py", validator)


if __name__ == "__main__":
    unittest.main()
