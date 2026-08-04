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

import docs_model_format  # noqa: E402
import docs_localization  # noqa: E402


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
        particle_asset = next(
            asset
            for asset in self.model["assets"]
            if asset["id"] == "model-format.asset.particle"
        )
        self.assertEqual(particle_asset["extensions"], [".spk", ".efk"])
        particle_requirements = "\n".join(particle_asset["requirements"])
        self.assertIn("not its .spark or .efkproj authoring source", particle_requirements)
        self.assertIn("runtime particle creation requires it", particle_requirements)
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

    def test_model_info_baker_order_matches_the_live_header(self) -> None:
        header = (ENGINE_ROOT / "Source/Tools/ModelInfoBaker.h").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "GetOrder() const -> int32_t override { return 6; }",
            header,
        )
        rule = next(
            entry
            for entry in self.model["rules"]
            if entry["id"] == "model-format.rule.mesh-before-info"
        )
        self.assertIn("orders are 4 for ModelMesh and 6 for ModelInfo", rule["rationale"])
        guide = (ENGINE_ROOT / "Docs/en/how-to/content/model-format.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("`ModelInfoBaker` runs at order `6`", guide)

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
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/model-format.md"
        ).read_text(encoding="utf-8")

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
        self.assertIn("[Model Animation](model-animation.md)", guide)
        self.assertIn("[Sprite Root Motion](sprite-root-motion.md)", guide)
        self.assertIn("generated `.spk` or `.efk` resource", guide)

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
        pages = docs_model_format.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_model_format.OUTPUT_PATHS))
        self.assertIn("Subset", pages["Docs/en/reference/model-format/tokens.md"])
        self.assertIn(
            "AttachParticles",
            pages["Docs/en/reference/model-format/composition.md"],
        )
        self.assertIn(
            "FO_MODEL_MAX_BONES",
            pages["Docs/en/reference/model-format/assets.md"],
        )
        self.assertIn(
            "CalculateTangentSpace",
            pages["Docs/en/reference/model-format/validation.md"],
        )
        self.assertIn(
            "Справочник формата моделей",
            pages["Docs/ru/reference/model-format/index.md"],
        )
        self.assertIn(
            "одноимённые child/parent bones",
            pages["Docs/ru/reference/model-format/tokens.md"],
        )
        self.assertNotIn(
            "Every accepted current parser token.",
            pages["Docs/ru/reference/model-format/index.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_model_format.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)

    def test_russian_pages_pin_english_hashes_and_preserve_commands(self) -> None:
        pages = docs_model_format.render_reference_pages(ENGINE_ROOT)
        for (_, document_id, _), english_path, russian_path in zip(
            docs_model_format.PAGE_DEFINITIONS,
            docs_model_format.CANONICAL_OUTPUT_PATHS,
            docs_model_format.RUSSIAN_OUTPUT_PATHS,
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
        pages = docs_model_format.generate_reference_pages(self.model)

        for filename, _, _ in docs_model_format.PAGE_DEFINITIONS:
            canonical = pages[f"Docs/en/reference/model-format/{filename}"]
            legacy = pages[f"Docs/generated/model-format/{filename}"]
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)
                self.assertIn(
                    f"../../en/reference/model-format/{filename}#{anchor}",
                    legacy,
                )

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        canonical_guide = documents["Docs/en/how-to/content/model-format.md"]
        self.assertEqual(
            (
                canonical_guide["id"],
                canonical_guide["state"],
                canonical_guide["disposition"],
            ),
            ("model-format-guide", "current", "retain"),
        )
        legacy_guide = documents["Docs/ModelFormat.md"]
        self.assertEqual(
            (
                legacy_guide["state"],
                legacy_guide["disposition"],
                legacy_guide["redirect_to"],
            ),
            ("redirect", "replace", "model-format-guide"),
        )
        for filename, document_id, _ in docs_model_format.PAGE_DEFINITIONS:
            canonical = documents[f"Docs/en/reference/model-format/{filename}"]
            legacy = documents[f"Docs/generated/model-format/{filename}"]
            self.assertEqual(
                (canonical["id"], canonical["state"], canonical["disposition"]),
                (document_id, "current", "retain"),
            )
            self.assertEqual(
                (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
                ("redirect", "replace", document_id),
            )
        generated_paths = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["generated_artifacts"]["model_format_reference"]["paths"]
        self.assertTrue(
            set(docs_model_format.RUSSIAN_OUTPUT_PATHS).issubset(generated_paths)
        )


if __name__ == "__main__":
    unittest.main()
