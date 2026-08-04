from __future__ import annotations

import json
import re
import sys
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_localization  # noqa: E402


CONFIG_ID = "configuration-data-sources"
CONFIG_EN = "Docs/en/reference/settings/configuration-and-data-sources.md"
CONFIG_RU = "Docs/ru/reference/settings/configuration-and-data-sources.md"
CONFIG_LEGACY = "Docs/ConfigurationAndDataSources.md"
TOOLS_ID = "tools"
TOOLS_EN = "Docs/en/reference/tools/index.md"
TOOLS_RU = "Docs/ru/reference/tools/index.md"
TOOLS_LEGACY = "Docs/Tools.md"


class ConfigurationAndToolsDocumentationTests(unittest.TestCase):
    def _text(self, path: str) -> str:
        return (ENGINE_ROOT / path).read_text(encoding="utf-8")

    def test_manifest_owns_canonical_locale_pairs_and_legacy_routes(self) -> None:
        manifest = json.loads(self._text("Docs/documentation-manifest.json"))
        documents = manifest["documents"]

        for document_id, english_path, russian_path, legacy_path in (
            (CONFIG_ID, CONFIG_EN, CONFIG_RU, CONFIG_LEGACY),
            (TOOLS_ID, TOOLS_EN, TOOLS_RU, TOOLS_LEGACY),
        ):
            canonical = documents[english_path]
            self.assertEqual(canonical["id"], document_id)
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(canonical["target"], english_path)
            self.assertEqual(canonical["classification"]["translation"], "required")

            legacy = documents[legacy_path]
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["disposition"], "replace")
            self.assertEqual(legacy["target"], english_path)
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertIn(english_path, legacy["sources"])
            self.assertIn(russian_path, legacy["sources"])

        self.assertEqual(manifest["localization"]["enforcement"], "complete")
        self.assertIn("permalink: /Docs/en/reference/tools/", self._text(TOOLS_EN))
        self.assertIn("permalink: /Docs/ru/reference/tools/", self._text(TOOLS_RU))

    def test_configuration_reference_matches_current_source_contract(self) -> None:
        config_header = self._text("Source/Common/ConfigFile.h")
        settings_header = self._text("Source/Common/Settings.h")
        data_source = self._text("Source/Common/DataSource.h")
        file_system = self._text("Source/Common/FileSystem.h")
        startup = self._text("Source/Frontend/ApplicationInit.cpp")
        english = self._text(CONFIG_EN)
        russian = self._text(CONFIG_RU)

        for marker in ("CollectContent", "SkipNestedSections"):
            self.assertIn(marker, config_header)
        for marker in (
            "ResourcePackInfo",
            "SubConfigInfo",
            "ApplyConfigAtPath",
            "ApplyCommandLine",
            "ApplySubConfigSection",
            "ApplyDefaultSettings",
            "ApplyAutoSettings",
            "FindCustomSetting",
            "GetCustomSetting",
        ):
            self.assertIn(marker, settings_header)
            self.assertIn(f"`{marker}", english)
            self.assertIn(f"`{marker}", russian)
        for marker in ("MountDir", "MountPack", "Reindex"):
            self.assertIn(marker, data_source)
            self.assertIn(marker, english)
            self.assertIn(marker, russian)
        for marker in ("AddDirSource", "AddPackSource", "AddCustomSource", "ReindexDataSources"):
            self.assertIn(marker, file_system)
            self.assertIn(marker, english)
            self.assertIn(marker, russian)

        load_settings = startup[startup.index("auto LoadAppSettings") :]
        self.assertLess(
            load_settings.index("settings.ApplyDefaultSettings();"),
            load_settings.index("if (!IsPackaged())"),
        )
        self.assertIn("defaults before reading project input", english)
        self.assertIn("defaults движка применяются до чтения входов проекта", russian)

    def test_tools_reference_covers_every_registered_builtin_baker(self) -> None:
        baker_source = self._text("Source/Tools/Baker.cpp")
        registered = set(re.findall(r"MakeUnique<([A-Za-z]+Baker)>", baker_source))
        expected = {
            "AngelScriptBaker",
            "ConfigBaker",
            "EffectBaker",
            "ImageBaker",
            "MapBaker",
            "MetadataBaker",
            "ModelInfoBaker",
            "ModelMeshBaker",
            "ParticleBaker",
            "ProtoBaker",
            "ProtoTextBaker",
            "RawCopyBaker",
            "TextBaker",
        }
        self.assertEqual(registered, expected)

        english = self._text(TOOLS_EN)
        russian = self._text(TOOLS_RU)
        for baker in sorted(expected):
            self.assertIn(f"`Source/Tools/{baker}.*`", english)
            self.assertIn(f"`Source/Tools/{baker}.*`", russian)

        mapper_header = self._text("Source/Tools/Mapper.h")
        for marker in ("SaveMapperScreenshot", "RequestMapperWindowScreenshot"):
            self.assertIn(marker, mapper_header)
            self.assertIn(f"`{marker}()`", english)
            self.assertIn(f"`{marker}()`", russian)

    def test_legacy_pointers_preserve_heading_routes(self) -> None:
        for canonical_path, legacy_path in (
            (CONFIG_EN, CONFIG_LEGACY),
            (TOOLS_EN, TOOLS_LEGACY),
        ):
            canonical_headings = re.findall(r"^(#{2,3}) (.+)$", self._text(canonical_path), re.MULTILINE)
            legacy = self._text(legacy_path)
            for level, heading in canonical_headings:
                self.assertIn(f"{level} {heading}", legacy)
            self.assertIn("[English]", legacy)
            self.assertIn("[Russian]", legacy)

    def test_complete_localization_and_ci_gate_are_current(self) -> None:
        model = docs_localization.generate_localization_status(ENGINE_ROOT)
        self.assertEqual(model["summary"]["required_document_count"], 197)
        self.assertEqual(model["summary"]["current_translation_count"], 197)
        self.assertEqual(model["summary"]["missing_translation_count"], 0)
        self.assertTrue(model["summary"]["complete"])

        current = {
            document["id"]: document
            for document in model["documents"]
            if document["status"] == "current"
        }
        self.assertEqual(current[CONFIG_ID]["russian_path"], CONFIG_RU)
        self.assertEqual(current[TOOLS_ID]["russian_path"], TOOLS_RU)

        workflow = self._text(".github/workflows/validate.yml")
        self.assertIn("BuildTools/tests/test_docs_configuration_tools.py", workflow)
        self.assertIn(
            "BuildTools/docs_localization.py --check --enforce-complete",
            workflow,
        )


if __name__ == "__main__":
    unittest.main()
