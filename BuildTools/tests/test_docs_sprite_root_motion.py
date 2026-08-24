from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/how-to/content/sprite-root-motion.md"
RUSSIAN_PATH = "Docs/ru/how-to/content/sprite-root-motion.md"
LEGACY_PATH = "Docs/SpriteRootMotion.md"


class SpriteRootMotionDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_covers_the_owned_sprite_root_motion_boundaries(self) -> None:
        guide = self._read(GUIDE_PATH)

        for heading in (
            "## Contract status",
            "## Authoring per-frame offsets",
            "## Baked and runtime representation",
            "## When root motion drives a critter",
            "## Continuous movement displacement",
            "## Movement stop and offset normalization",
            "## Anchor and cycle phase",
            "## Frame selection",
            "## Rendered offset",
            "## Direction and sheet changes",
            "## Failure and fallback behavior",
            "## Visual acceptance matrix",
            "## Authoring practices",
            "## Project evidence and extraction rules",
            "## Project boundary",
            "## Maintenance triggers",
            "## Validation routes",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "Sprite root motion is presentation data",
            "T        = sum(delta[0..N-1])",
            "intentionally does not clamp the result to one hex",
            "cycle_proj      = rel_dot_total mod total_dot_total",
            "offs_anim    = (cycle_start - pos) + accum[i]",
            "3D model critter",
            "current native suite does not expose a focused `CritterHexView` root-motion fixture",
        ):
            self.assertIn(contract, guide)

    def test_baker_claims_match_offset_import_and_transport_source(self) -> None:
        header = self._read("Source/Tools/ImageBaker.h")
        baker = self._read("Source/Tools/ImageBaker.cpp")
        resource = self._read("Source/Common/SpriteResource.cpp")
        tests = self._read("Source/Tests/Test_ImageBaker.cpp")

        self.assertIn("int16_t NextX {};", header)
        self.assertIn("int16_t NextY {};", header)
        self.assertIn("writer.Write<int16_t>(bake_shot->NextX);", baker)
        self.assertIn("writer.Write<int16_t>(bake_shot->NextY);", baker)
        self.assertIn("frame.NextOffset.x = reader.GetLEInt16();", resource)
        self.assertIn("frame.NextOffset.y = reader.GetLEInt16();", resource)
        self.assertIn('strex("next_x_{}", frm)', baker)
        self.assertIn('strex("NextX_{}", frm)', baker)
        self.assertIn("NextX_0=5", tests)
        self.assertIn("next_y_0=6", tests)

    def test_client_claims_match_sprite_loading_and_movement_source(self) -> None:
        sprites = self._read("Source/Client/DefaultSprites.cpp")
        movement = self._read("Source/Common/Movement.cpp")
        critter = self._read("Source/Client/CritterHexView.cpp")

        self.assertIn("dir_anim->_sprOffset[j] = frame.NextOffset;", sprites)
        self.assertIn("dir_anim->_sprOffset[j] = dir_anim->_sprOffset[index];", sprites)
        self.assertIn("Do not clamp HexOffset: prediction can leave current_hex a full step behind", movement)
        self.assertIn("progress.HexOffset = ipos16", movement)
        self.assertIn("bool root_motion_drive = !_curAnim.has_value() && IsMoving()", critter)
        self.assertIn("EvaluateMovementFrameIndex(cur_anim.Frames)", critter)
        self.assertIn("_map->MoveCritter(this, progress.Hex, false);", critter)

    def test_cycle_math_and_lifecycle_claims_match_critter_source(self) -> None:
        header = self._read("Source/Client/CritterHexView.h")
        critter = self._read("Source/Client/CritterHexView.cpp")

        self.assertIn("_walkAnchorAnim", header)
        self.assertIn("_walkAnchorDisp", header)
        self.assertIn("_walkAnchorAnim = nullptr;", critter)
        self.assertIn("_walkAnchorAnim.reset();", critter)
        self.assertIn("old_cycle_proj += old_total_dot_total;", critter)
        self.assertIn("cycle_number = -((-rel_dot_total + total_dot_total - 1) / total_dot_total);", critter)
        self.assertIn("_offsAnim.x = cycle_start_x - pos.x + accum.x;", critter)
        self.assertIn("int64_t diff = std::abs(accum_dot_total - cycle_proj);", critter)
        self.assertIn("if (total_dot_total <= 0) {\n        return 0;", critter)

    def test_stop_normalization_claims_match_geometry_source_and_tests(self) -> None:
        critter = self._read("Source/Client/CritterHexView.cpp")
        geometry = self._read("Source/Common/Geometry.h")
        tests = self._read("Source/Tests/Test_Geometry.cpp")

        self.assertIn("NormalizeHexOffset();", critter)
        self.assertIn("!GetMap()->GetField(check_hex).MoveBlocked", critter)
        self.assertIn("GeometryHelper::NormalizeHexOffset(hex, hex_offset, GetMap()->GetSize(), is_movable)", critter)
        self.assertIn("NormalizeHexOffset(mpos& hex, ipos16& hex_offset, msize map_size, const function<bool(mpos)>& is_movable)", geometry)
        self.assertIn('TEST_CASE("NormalizeHexOffset")', tests)
        self.assertIn('SECTION("A blocked target hex leaves the position untouched")', tests)
        self.assertIn('SECTION("Positions outside the map are refused before the predicate runs")', tests)

    def test_project_evidence_is_explicit_and_historical_names_stay_non_normative(self) -> None:
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
            ("last-frontier", "Docs/ContentWorkflow.md"),
            ("last-frontier", "Docs/CharacterGenerator.md"),
            ("last-frontier", "Docs/DocumentationMaintenance.md"),
            ("fonline-tla", "Docs/Animation.md"),
        ):
            self.assertIn(source, sources)

        guide = self._read(GUIDE_PATH)
        self.assertIn("no current authored `.fofrm` or legacy FRM-family sprite assets", guide)
        self.assertIn("`raw_ptr<const SpriteSheet>`", guide)
        self.assertIn("`DefaultSpriteFactory::LoadAnimation`", guide)
        self.assertIn("`DefaultSpriteFactory::LoadSprite`", guide)
        self.assertIn(GUIDE_PATH, record["engine_targets"])
        self.assertIn("No current authored 2D root-motion asset was observed", record["decision"])

    def test_russian_translation_is_complete_and_preserves_code(self) -> None:
        english = self._read(GUIDE_PATH)
        russian = self._read(RUSSIAN_PATH)

        self.assertIn("document_id: sprite-root-motion", russian)
        for heading in (
            "## Статус контракта",
            "## Создание покадровых offsets",
            "## Остановка и нормализация offset",
            "## Матрица визуальной приёмки",
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

        self.assertEqual(document["id"], "sprite-root-motion")
        self.assertEqual(document["owner"], "content-data")
        self.assertEqual(document["classification"]["translation"], "required")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], "sprite-root-motion")
        self.assertIn("Source/Common/Geometry.cpp", document["sources"])
        self.assertIn("Source/Tests/Test_Geometry.cpp", document["sources"])
        self.assertIn("sprite-root-motion", content_group["document_ids"])
        self.assertIn("BuildTools/tests/test_docs_sprite_root_motion.py", workflow)
        self.assertIn("CritterHexView", maintenance)
        self.assertIn("sprite-root-motion.md", maintenance)

        for heading in re.findall(r"^(##+ .+)$", guide, re.MULTILINE):
            self.assertIn(heading, legacy_page)


if __name__ == "__main__":
    unittest.main()
