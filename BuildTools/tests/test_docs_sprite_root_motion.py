from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class SpriteRootMotionDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_covers_the_owned_sprite_root_motion_boundaries(self) -> None:
        guide = self._read("Docs/SpriteRootMotion.md")

        for heading in (
            "## Authoring per-frame offsets",
            "## Baked and runtime representation",
            "## When root motion drives a critter",
            "## Continuous movement displacement",
            "## Anchor and cycle phase",
            "## Frame selection",
            "## Rendered offset",
            "## Direction and sheet changes",
            "## Failure and fallback behavior",
            "## Authoring practices",
            "## Project boundary",
            "## Validation routes",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "Sprite root motion is presentation data",
            "T        = sum(delta[0..N-1])",
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

    def test_guide_is_routed_and_source_checks_run_in_ci(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        document = manifest["documents"]["Docs/SpriteRootMotion.md"]
        content_group = next(group for group in manifest["site_delivery"]["navigation"] if group["id"] == "content")
        workflow = self._read(".github/workflows/validate.yml")
        maintenance = self._read("Docs/DocumentationMaintenance.md")

        self.assertEqual(document["id"], "sprite-root-motion")
        self.assertEqual(document["owner"], "content-data")
        self.assertEqual(document["classification"]["translation"], "required")
        self.assertIn("sprite-root-motion", content_group["document_ids"])
        self.assertIn("BuildTools/tests/test_docs_sprite_root_motion.py", workflow)
        self.assertIn("CritterHexView", maintenance)
        self.assertIn("SpriteRootMotion.md", maintenance)


if __name__ == "__main__":
    unittest.main()

