from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/how-to/content/model-animation.md"
RUSSIAN_PATH = "Docs/ru/how-to/content/model-animation.md"
LEGACY_PATH = "Docs/ModelAnimation.md"


class ModelAnimationDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_covers_the_owned_animation_duration_boundaries(self) -> None:
        guide = self._read(GUIDE_PATH)

        for heading in (
            "## Contract status",
            "## Authoring animation tuples",
            "## One-step aliases",
            "## Bake output and distribution",
            "## Runtime and script lookup",
            "## Timing acceptance matrix",
            "## Failure behavior",
            "## Authoring practices",
            "## Project evidence and extraction rules",
            "## Project boundary",
            "## Maintenance triggers",
            "## Validation routes",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "effective_duration_ms = round((clip_duration_seconds / AnimSpeed) * 1000)",
            "aliases are not followed recursively",
            "alias has priority over an exact tuple",
            "ModelAnimationInfo.foinfo",
            "Game.GetModelAnimDuration",
            "Critter.GetModelAnimDuration",
            "private baker/runtime contracts",
            "AllowAnimationGeometry",
            "ModelSourceAssetCache",
            "LFOZZRIG",
            "signed 32-bit millisecond maximum",
            "positive sub-millisecond result that rounds to zero is rejected",
        ):
            self.assertIn(contract, guide)

    def test_authored_tokens_and_validation_claims_match_the_baker(self) -> None:
        baker = self._read("Source/Tools/ModelInfoBaker.cpp")

        for marker in (
            'else if (token == "Anim")',
            'else if (token == "AnimSpeed")',
            'else if (token == "StateAnimEqual")',
            'else if (token == "ActionAnimEqual")',
            'starts_with("TEMPLATE_")',
            'ValidateModelDescriptionEnumValue(name_resolver, "CritterStateAnim"',
            'ValidateModelDescriptionEnumValue(name_resolver, "CritterActionAnim"',
            "Animation speed must be positive with a finite reciprocal",
            "Animation duration is outside the millisecond output range",
            "Animation duration rounds to a non-positive millisecond value",
            "Animation for state/action pair not found in animation file",
            'else if (token == "AllowAnimationGeometry")',
            "External animation model contains drawable mesh nodes",
        ):
            self.assertIn(marker, baker)

    def test_duration_and_alias_claims_match_source_and_native_tests(self) -> None:
        baker = self._read("Source/Tools/ModelInfoBaker.cpp")
        tests = self._read("Source/Tests/Test_ModelBaker.cpp")

        self.assertIn("!std::isfinite(1.0f / speed)", baker)
        self.assertIn("duration_milliseconds > static_cast<double>(std::numeric_limits<int32_t>::max())", baker)
        self.assertIn("int32_t duration_ms = iround<int32_t>(duration_milliseconds);", baker)
        self.assertIn("materializes every resolvable pair", baker)
        self.assertIn("where an alias outranks an exact entry", baker)
        self.assertIn('SECTION("Materializes model animation aliases with client lookup semantics")', tests)
        self.assertIn('CHECK(config.find("StateAnimations = 1 0 1 0\\n") != string::npos);', tests)
        self.assertIn('CHECK(config.find("ActionAnimations = 5 5 3 3\\n") != string::npos);', tests)
        self.assertIn('CHECK(config.find("DurationsMs = 500 500 200 200\\n") != string::npos);', tests)

    def test_runtime_lookup_claims_match_metadata_and_script_sources(self) -> None:
        animation_info = self._read("Source/Common/AnimationInfo.cpp")
        engine = self._read("Source/Common/EngineBase.cpp")
        common_methods = self._read("Source/Scripting/CommonGlobalScriptMethods.cpp")
        client_methods = self._read("Source/Scripting/ClientCritterScriptMethods.cpp")
        common_tests = self._read("Source/Tests/Test_CommonScriptMethods.cpp")

        self.assertIn('MODEL_ANIMATION_INFO_FILE_NAME = "ModelAnimationInfo.foinfo"', animation_info)
        self.assertIn("ReadAnimationInfo(resources, Hashes)", engine)
        self.assertIn("engine->GetAnimationInfo(modelName)", common_methods)
        self.assertIn("return model->GetAnimDuration(stateAnim, actionAnim);", client_methods)
        self.assertIn('TEST_CASE("ModelAnimationInfoLookup")', common_tests)
        self.assertIn('CHECK(server->Hashes.CheckHashedString("Critters/Test.fo3d"));', common_tests)
        self.assertIn("if (missingAction.milliseconds != 0)", common_tests)
        self.assertIn("if (missingModel.milliseconds != 0)", common_tests)

    def test_project_evidence_is_explicit_and_legacy_tokens_stay_non_normative(self) -> None:
        model = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(
            value
            for value in model["records"]
            if value["id"] == "model-animation-and-root-motion"
        )
        sources = {
            (source["snapshot"], source["path"])
            for source in record["sources"]
        }

        for source in (
            ("last-frontier", "Resources/CrittersArt/Critters/TEMPLATE_HumanAnimations.fo3d"),
            ("last-frontier", "Resources/CrittersArt/Critters/CR_HumanMaleNormal.fo3d"),
            ("last-frontier", "Resources/CrittersArt/Critters/VH_Jagger.fo3d"),
            ("last-frontier", "Resources/CrittersArt/Critters/VH_Snowmobile.fo3d"),
            ("fonline-tla", "Resources/VanBuren/art/critters/_VBMob.fo3d"),
            ("fonline-tla", "Resources/VanBuren/art/critters/_VBWeapon.fo3d"),
            ("fonline-tla", "Resources/VanBuren/art/critters/_VBHuman.fo3d"),
            ("fonline-tla", "Resources/VanBuren/art/critters/VbDog.fo3d"),
        ):
            self.assertIn(source, sources)

        guide = self._read(GUIDE_PATH)
        self.assertIn("TEMPLATE_HumanAnimations.fo3d", guide)
        self.assertIn("historical bare `AnimEqual` declarations", guide)
        self.assertIn("model-level `Speed` declarations are not tuple `AnimSpeed`", guide)
        self.assertIn(GUIDE_PATH, record["engine_targets"])
        self.assertIn("explicit legacy migration evidence", record["decision"])

    def test_russian_translation_is_complete_and_preserves_code(self) -> None:
        english = self._read(GUIDE_PATH)
        russian = self._read(RUSSIAN_PATH)

        self.assertIn('document_id: model-animation', russian)
        self.assertIn('"document_id":"model-animation"', russian)
        for heading in (
            "## Статус контракта",
            "## Создание animation tuples",
            "## Одношаговые aliases",
            "## Матрица приёмки времени",
            "## Project evidence и правила извлечения",
            "## Триггеры сопровождения",
            "## Маршруты проверки",
        ):
            self.assertIn(heading, russian)

        fenced = re.compile(r"```[^\n]*\n.*?```", re.DOTALL)
        self.assertEqual(fenced.findall(english), fenced.findall(russian))

    def test_guide_is_routed_and_source_checks_run_in_ci(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        document = manifest["documents"][GUIDE_PATH]
        legacy = manifest["documents"][LEGACY_PATH]
        content_group = next(
            group
            for group in manifest["site_delivery"]["navigation"]
            if group["id"] == "content"
        )
        workflow = self._read(".github/workflows/validate.yml")
        maintenance = self._read("Docs/en/contributing/documentation/index.md")
        guide = self._read(GUIDE_PATH)
        legacy_page = self._read(LEGACY_PATH)

        self.assertEqual(document["id"], "model-animation")
        self.assertEqual(document["owner"], "content-data")
        self.assertEqual(document["classification"]["translation"], "required")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], "model-animation")
        self.assertIn("Source/Common/AnimationInfo.cpp", document["sources"])
        self.assertIn("model-animation", content_group["document_ids"])
        self.assertIn("BuildTools/tests/test_docs_model_animation.py", workflow)
        self.assertIn("ModelInfoBaker", maintenance)
        self.assertIn("model-animation.md", maintenance)

        for heading in re.findall(r"^(##+ .+)$", guide, re.MULTILINE):
            self.assertIn(heading, legacy_page)


if __name__ == "__main__":
    unittest.main()
