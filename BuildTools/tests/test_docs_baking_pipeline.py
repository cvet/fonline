from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/explanation/content-pipeline/baking.md"
RUSSIAN_PATH = "Docs/ru/explanation/content-pipeline/baking.md"
LEGACY_PATH = "Docs/BakingPipeline.md"

BAKERS = (
    ("Metadata", "MetadataBaker", 1),
    ("Config", "ConfigBaker", 2),
    ("RawCopy", "RawCopyBaker", 4),
    ("Image", "ImageBaker", 4),
    ("Effect", "EffectBaker", 4),
    ("Text", "TextBaker", 4),
    ("ModelMesh", "ModelMeshBaker", 4),
    ("AngelScript", "AngelScriptBaker", 4),
    ("Particle", "ParticleBaker", 5),
    ("ProtoText", "ProtoTextBaker", 6),
    ("ModelInfo", "ModelInfoBaker", 6),
    ("Proto", "ProtoBaker", 7),
    ("Map", "MapBaker", 8),
)


class BakingPipelineDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_built_in_baker_names_and_orders_match_headers_and_guide(self) -> None:
        guide = self._read(GUIDE_PATH)
        setup = self._read("Source/Tools/Baker.cpp")

        for name, class_name, order in BAKERS:
            header = self._read(f"Source/Tools/{class_name}.h")
            self.assertIn(f'NAME = "{name}"', header, class_name)
            self.assertIn(
                f"GetOrder() const -> int32_t override {{ return {order}; }}",
                header,
                class_name,
            )
            self.assertIn(f"vec_exists(request_bakers, {class_name}::NAME)", setup)
            self.assertRegex(
                guide,
                rf"\| `{re.escape(name)}` \| `{class_name}` \| {order} \|",
                class_name,
            )

        self.assertIn("#if FO_ENABLE_3D", setup)
        self.assertIn("#if FO_ANGELSCRIPT_SCRIPTING", setup)

    def test_cmake_targets_use_project_config_and_fresh_codegen(self) -> None:
        cmake = self._read("BuildTools/cmake/stages/ScriptsAndBaking.cmake")
        helper = self._read("BuildTools/cmake/helpers/Build.cmake")
        self.assertIn(
            'SetValue(foMainConfigArgs -ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}" -ApplySubConfig "NONE")',
            cmake,
        )
        compile_block = re.search(
            r"AddCommandTarget\(CompileAngelScript\b(?P<body>.*?)(?=\n\s*endif\(\))",
            cmake,
            re.DOTALL,
        )
        self.assertIsNotNone(compile_block)
        self.assertIn("DEPENDS ForceCodeGeneration", compile_block.group("body"))
        self.assertIn("AddBakingTarget(BakeResources)", cmake)
        self.assertIn("AddBakingTarget(ForceBakeResources FORCE)", cmake)
        for marker in (
            '-ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}"',
            '-ApplySubConfig "${BAKING_TARGET_SUB_CONFIG}"',
            'set(BAKING_TARGET_SUB_CONFIG "NONE")',
            "DEPENDS ForceCodeGeneration",
        ):
            self.assertIn(marker, helper)

        guide = self._read(GUIDE_PATH)
        self.assertIn("`-ApplyConfig <FO_MAIN_CONFIG>`", guide)
        self.assertIn("default subconfig `NONE`", guide)
        self.assertIn("`ForceCodeGeneration`", guide)

    def test_resource_pack_contract_and_practices_are_source_backed(self) -> None:
        settings_header = self._read("Source/Common/Settings.h")
        settings_source = self._read("Source/Common/Settings.cpp")
        guide = self._read(GUIDE_PATH)

        for field in (
            "Name",
            "InputDirs",
            "InputFiles",
            "IncludePatterns",
            "ExcludePatterns",
            "ServerOnly",
            "ClientOnly",
            "MapperOnly",
            "Bakers",
        ):
            self.assertIn(field, settings_header)
            self.assertIn(f"`{field}`", guide)
        self.assertIn("Resource pack name not specifed", settings_source)
        self.assertIn("Resource pack can be common or server, client or mapper only", settings_source)
        self.assertIn("bakers at the same order may run concurrently", guide)
        self.assertIn("do not edit baked output as source", guide)

    def test_canonical_russian_and_legacy_routes_are_owned(self) -> None:
        documents = json.loads(self._read("Docs/documentation-manifest.json"))["documents"]
        canonical = documents[GUIDE_PATH]
        legacy = documents[LEGACY_PATH]

        self.assertEqual(
            (canonical["id"], canonical["state"], canonical["disposition"]),
            ("baking-pipeline", "current", "retain"),
        )
        self.assertEqual(
            (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
            ("redirect", "replace", "baking-pipeline"),
        )
        self.assertTrue((ENGINE_ROOT / RUSSIAN_PATH).is_file())

        canonical_text = self._read(GUIDE_PATH)
        legacy_text = self._read(LEGACY_PATH)
        for heading in re.findall(r"^#{2,3} .+$", canonical_text, re.MULTILINE):
            self.assertIn(heading, legacy_text)
        self.assertIn("en/explanation/content-pipeline/baking.md", legacy_text)
        self.assertIn("ru/explanation/content-pipeline/baking.md", legacy_text)

    def test_external_evidence_uses_pinned_project_pack_definitions(self) -> None:
        evidence = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(
            item
            for item in evidence["records"]
            if item["id"] == "generated-content-ownership"
        )
        sources = {
            (item["snapshot"], item["path"])
            for item in record["sources"]
        }
        self.assertIn(("last-frontier", "LastFrontier.fomain"), sources)
        self.assertIn(("fonline-tla", "TLA.fomain"), sources)
        self.assertIn(GUIDE_PATH, record["engine_targets"])
        self.assertNotIn(LEGACY_PATH, record["engine_targets"])


if __name__ == "__main__":
    unittest.main()
