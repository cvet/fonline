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
            "## Authoring per-frame motion",
            "## Baked and runtime representation",
            "## When movement selects the frame",
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
            "Sprite frame offsets are presentation data",
            "T        = sum(delta[0..N-1])",
            "intentionally does not clamp the result to one hex",
            "cycle_proj      = rel_dot_total mod total_dot_total",
            "offs_anim    = (cycle_start - pos) + accum[i]",
            "3D model critter",
            "CritterAnimationFlags::Movement",
        ):
            self.assertIn(contract, guide)

    def test_baker_claims_match_offset_import_and_transport_source(self) -> None:
        header = self._read("Source/Tools/ImageImport.h")
        baker = self._read("Source/Tools/ImageBaker.cpp") + self._read("Source/Tools/ImageBaker-Fofrm.cpp")
        resource = self._read("Source/Common/SpriteResource.cpp")
        tests = self._read("Source/Tests/Test_ImageBaker.cpp")

        self.assertIn("isize32 Size {};", header)
        self.assertIn("ipos32 Motion {};", header)
        self.assertIn("writer.write<int16_t>(numeric_cast<int16_t>(shot.Motion.x));", baker)
        self.assertIn("writer.write<int16_t>(numeric_cast<int16_t>(shot.Motion.y));", baker)
        self.assertIn("frame.NextOffset = {reader.GetLEInt16(), reader.GetLEInt16()};", resource)
        self.assertNotIn('strex("NextY_{}", frm)', baker)
        self.assertNotIn('strex("NextX_{}", frm)', baker)
        self.assertIn("ipos32 Offset {};", header)
        self.assertIn("shot.Offset", baker)
        self.assertIn("auto shot = source.Frames[frame];", baker)
        self.assertIn("shot.Data = source.Frames[concrete].Data;", baker)
        self.assertIn("sequence.OffsX) + shot.Offset.x", baker)
        self.assertIn("sequence.OffsY) + shot.Offset.y", baker)
        self.assertIn("FrameOffsets", tests)

        guide = self._read(GUIDE_PATH)
        self.assertIn("Offset + FrameOffsets[i]", guide)
        self.assertIn("without accumulation or root motion", guide)
        self.assertNotIn("NextX_0 =", guide)
        self.assertNotIn("NextY_0 =", guide)

    def test_client_claims_match_sprite_loading_and_movement_source(self) -> None:
        sprites = self._read("Source/Client/DefaultSprites.cpp")
        movement = self._read("Source/Common/Movement.cpp")
        critter = self._read("Source/Client/CritterHexView.cpp")
        animation = self._read("Source/Client/AnimationMovement.cpp")

        self.assertIn("dir_anim->SetFrame(numeric_cast<int32_t>(j), std::move(spr), offset, frame.NextOffset);", sprites)
        self.assertIn("frame.Offset, frame.NextOffset", sprites)
        self.assertIn("_sprMotion[index] = motion;", sprites)
        self.assertIn("_sprOffset[index] = offset;", sprites)
        self.assertIn("Do not clamp HexOffset: prediction can leave current_hex a full step behind", movement)
        self.assertIn("progress.HexOffset = ipos16", movement)
        self.assertIn("is_enum_set(cur_anim.Flags, CritterAnimationFlags::Freeze)", critter)
        self.assertIn("!frozen && IsMoving() && is_enum_set(cur_anim.Flags, CritterAnimationFlags::Movement)", critter)
        self.assertIn("frame_index = i;", animation)
        self.assertIn("AnimationMovement::EvaluateWalkFrame(offsets, total, pos, _walkAnchorDisp, anim.FrameIndex, _offsAnim);", critter)
        self.assertIn("AnimationMovement::AccumulateFrameOffsets(offsets, anim.FrameIndex)", critter)
        self.assertIn("_map->MoveCritter(this, progress.Hex, false);", critter)

    def test_cycle_math_and_lifecycle_claims_match_critter_source(self) -> None:
        header = self._read("Source/Client/CritterHexView.h")
        critter = self._read("Source/Client/CritterHexView.cpp")
        animation = self._read("Source/Client/AnimationMovement.cpp")

        self.assertIn("nptr<const SpriteSheet> _walkAnchorAnim", header)
        self.assertIn("_walkAnchorDisp", header)
        self.assertIn("_walkAnchorAnim = nullptr;", critter)
        self.assertIn("_walkAnchorAnim.reset();", critter)
        self.assertIn("ScaleWalkPhase(old_phase, old_length, total.x)", animation)
        self.assertIn("anchor + cycle_offset + frame_offset - pos", animation)
        self.assertIn("_offsExt.round<int32_t>() + _offsAnim", critter)
        self.assertIn("projection % length < 0", animation)
        self.assertIn("cycle--;", animation)
        self.assertIn("WrapWalkPhase(projection, length)", animation)
        self.assertIn("frame_index = 0;", animation)
        self.assertIn("AnimationMovement::TransferWalkPhase(_walkAnchorAnim->GetSpriteMotion(), pos, _walkAnchorDisp, total)", critter)
        self.assertIn("if (diff < best_diff) {", animation)
        self.assertIn("numeric_cast<uint64_t>(phase) - static_cast<uint64_t>(frame_projection)", animation)
        self.assertNotRegex(header + critter + animation, r"\b(?:class|struct)\s+SpriteMotion\b")

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
