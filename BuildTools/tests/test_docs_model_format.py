from __future__ import annotations

import contextlib
import copy
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_model_format  # noqa: E402


class ModelFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_model_format.generate_model_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_matches_the_live_parser(self) -> None:
        second = docs_model_format.generate_model_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_model_format.SCHEMA_VERSION)
        self.assertEqual(self.model["summary"]["token_group_count"], 32)
        self.assertEqual(self.model["summary"]["parser_token_count"], 59)
        self.assertEqual(
            set(self.model["parser_tokens"]),
            {
                name
                for entry in self.model["tokens"]
                for name in entry["names"]
            },
        )
        identities = [
            entry["id"]
            for key in ("compile_limits", "assets", "tokens", "rules")
            for entry in self.model[key]
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertTrue(
            all(
                docs_model_format.ENTRY_ID_PATTERN.fullmatch(identity)
                for identity in identities
            )
        )

    def test_token_contract_preserves_stateful_and_legacy_boundaries(self) -> None:
        tokens = {
            entry["id"]: entry for entry in self.model["tokens"]
        }

        self.assertEqual(tokens["model-format.token.subset"]["stability"], "deprecated")
        self.assertIn(
            "dummy link",
            tokens["model-format.token.layer"]["description"],
        )
        self.assertIn(
            "same-named child and parent bones",
            tokens["model-format.token.attach"]["runtime_effect"],
        )
        self.assertIn(
            "runtime particle creation requires it",
            next(
                asset["requirements"][1]
                for asset in self.model["assets"]
                if asset["id"] == "model-format.asset.particle"
            ),
        )
        self.assertIn(
            "not serialized",
            tokens["model-format.token.allow-animation-geometry"]["runtime_effect"],
        )
        self.assertEqual(
            {entry["name"] for entry in self.model["removed_legacy"]},
            {"AnimEqual", "CalculateTangentSpace", "DrawSize", "RenderFrame", "RenderFrames", "ViewSize"},
        )
        self.assertTrue(
            set(entry["name"] for entry in self.model["removed_legacy"]).isdisjoint(
                self.model["parser_tokens"]
            )
        )

    def test_assets_and_limits_follow_the_project_interface(self) -> None:
        limits = {
            entry["option"]: entry for entry in self.model["compile_limits"]
        }

        self.assertEqual(limits["FO_MODEL_LAYERS_COUNT"]["default"], "30")
        self.assertEqual(limits["FO_MODEL_MAX_TEXTURES"]["default"], "8")
        self.assertEqual(limits["FO_MODEL_MAX_BONES"]["default"], "54")
        self.assertEqual(limits["FO_MODEL_BONES_PER_VERTEX"]["default"], "4")
        self.assertEqual(
            self.model["outputs"]["mesh_extensions"],
            [".fbx", ".obj"],
        )
        self.assertEqual(self.model["outputs"]["template_prefix"], "TEMPLATE_")

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in ("compile_limits", "assets", "tokens", "rules"):
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_authoring_runtime_and_project_boundaries(self) -> None:
        guide = (ENGINE_ROOT / "Docs/ModelFormat.md").read_text(encoding="utf-8")

        for heading in (
            "## Source mesh contract",
            "## Parser state",
            "## Model attachments",
            "## Cut volumes",
            "## Legacy content warning",
            "## Validation workflow",
        ):
            self.assertIn(heading, guide)
        self.assertIn("There is no directive that restores `Layer`", guide)
        self.assertIn("exactly `36` vertices", guide)
        self.assertIn("Legacy project content is evidence", guide)
        self.assertIn("[ModelAnimation.md](ModelAnimation.md)", guide)
        self.assertIn("[SpriteRootMotion.md](SpriteRootMotion.md)", guide)

    def test_missing_parser_token_is_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_model_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        manifest["tokens"] = copy.deepcopy(manifest["tokens"])
        transform_entry = next(
            entry
            for entry in manifest["tokens"]
            if entry["id"] == "model-format.token.transform-set"
        )
        transform_entry["names"].remove("MoveZ")

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "ModelFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "documented .fo3d tokens diverge from ParseToken"
            ):
                docs_model_format.generate_model_format_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_model_format.generate_reference_pages(self.model)

        self.assertEqual(set(pages), set(docs_model_format.OUTPUT_PATHS))
        self.assertIn("Subset", pages["Docs/generated/model-format/tokens.md"])
        self.assertIn(
            "AttachParticles",
            pages["Docs/generated/model-format/composition.md"],
        )
        self.assertIn(
            "FO_MODEL_MAX_BONES",
            pages["Docs/generated/model-format/assets.md"],
        )
        self.assertIn(
            "CalculateTangentSpace",
            pages["Docs/generated/model-format/validation.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_model_format.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
