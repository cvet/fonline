from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class ModelAnimationDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_covers_the_owned_animation_duration_boundaries(self) -> None:
        guide = self._read("Docs/ModelAnimation.md")

        for heading in (
            "## Authoring animation tuples",
            "## One-step aliases",
            "## Bake output and distribution",
            "## Runtime and script lookup",
            "## Failure behavior",
            "## Authoring practices",
            "## Project boundary",
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
            "private baker/runtime contract",
            "AllowAnimationGeometry",
            "ModelSourceAssetCache",
            "LFOZZRIG",
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
            'must be positive',
            "Animation for state/action pair not found in animation file",
            'else if (token == "AllowAnimationGeometry")',
            "External animation model contains drawable mesh nodes",
        ):
            self.assertIn(marker, baker)

    def test_duration_and_alias_claims_match_source_and_native_tests(self) -> None:
        baker = self._read("Source/Tools/ModelInfoBaker.cpp")
        tests = self._read("Source/Tests/Test_ModelBaker.cpp")

        self.assertIn("double duration_milliseconds = static_cast<double>(clip_duration) / static_cast<double>(speed) * 1000.0;", baker)
        self.assertIn("int32_t duration_ms = iround<int32_t>(duration_milliseconds);", baker)
        self.assertIn("both alias maps are applied once", baker)
        self.assertIn("an alias has priority over an exact entry", baker)
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

    def test_guide_is_routed_and_source_checks_run_in_ci(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        document = manifest["documents"]["Docs/ModelAnimation.md"]
        content_group = next(group for group in manifest["site_delivery"]["navigation"] if group["id"] == "content")
        workflow = self._read(".github/workflows/validate.yml")
        maintenance = self._read("Docs/DocumentationMaintenance.md")

        self.assertEqual(document["id"], "model-animation")
        self.assertEqual(document["owner"], "content-data")
        self.assertEqual(document["classification"]["translation"], "required")
        self.assertIn("model-animation", content_group["document_ids"])
        self.assertIn("BuildTools/tests/test_docs_model_animation.py", workflow)
        self.assertIn("ModelInfoBaker", maintenance)
        self.assertIn("ModelAnimation.md", maintenance)


if __name__ == "__main__":
    unittest.main()
