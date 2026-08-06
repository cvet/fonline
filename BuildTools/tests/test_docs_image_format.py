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

import docs_image_format  # noqa: E402
import docs_localization  # noqa: E402


class ImageFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_image_format.generate_image_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_extension_boundaries(self) -> None:
        second = docs_image_format.generate_image_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_image_format.SCHEMA_VERSION)
        outputs = self.model["outputs"]
        self.assertEqual(outputs["baker_name"], "Image")
        self.assertEqual(outputs["baker_order"], 4)
        self.assertEqual(len(outputs["accepted_extensions"]), 12)
        self.assertEqual(len(outputs["default_runtime_extensions"]), 11)
        self.assertEqual(outputs["default_runtime_unsupported"], ["spr"])
        self.assertEqual(outputs["container_magic"], 43)
        self.assertEqual(outputs["container_version"], 2)
        self.assertNotIn("spr", outputs["default_runtime_extensions"])

        identities = [
            entry["id"]
            for collection in docs_image_format.COLLECTION_KINDS
            for entry in self.model[collection]
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertTrue(
            all(docs_image_format.ENTRY_ID_PATTERN.fullmatch(identity) for identity in identities)
        )

    def test_fofrm_contract_records_flattening_timing_and_ignored_effect(self) -> None:
        fields = {entry["id"]: entry for entry in self.model["descriptor_fields"]}

        self.assertIn("carry forward", fields["image-format.field.sequence-offset"]["requirement"])
        self.assertIn("Main sequence", fields["image-format.field.flattening"]["requirement"])
        self.assertIn("not from the flattened", fields["image-format.field.timing"]["requirement"])
        self.assertEqual(fields["image-format.field.effect"]["stability"], "deprecated")
        self.assertIn("does not serialize", fields["image-format.field.effect"]["requirement"])
        self.assertFalse(self.model["outputs"]["fofrm_effect_serialized"])

    def test_runtime_contract_distinguishes_bake_load_atlas_and_cache(self) -> None:
        runtime = {entry["id"]: entry for entry in self.model["runtime_rules"]}
        formats = {entry["id"]: entry for entry in self.model["formats"]}

        self.assertIn("does not register", formats["image-format.format.spr"]["requirement"])
        self.assertIn("indexed silhouette", runtime["image-format.runtime.atlas"]["requirement"])
        baking = {entry["id"]: entry for entry in self.model["baking_rules"]}
        self.assertIn("SpriteMesh.Enabled", baking["image-format.baking.sprite-mesh"]["requirement"])
        self.assertIn("including when SpriteMesh.Enabled is false", baking["image-format.baking.sprite-mesh"]["requirement"])
        self.assertIn("SpriteInfo/", baking["image-format.baking.sprite-info-index"]["requirement"])
        self.assertIn("path plus AtlasType", runtime["image-format.runtime.cache"]["requirement"])
        self.assertIn("does not clear", runtime["image-format.runtime.missing-cache"]["requirement"])

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in docs_image_format.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(encoding="utf-8")
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_authoring_runtime_and_project_boundaries(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/image-format.md"
        ).read_text(encoding="utf-8")

        for heading in (
            "## Choose a source format",
            "## FOFRM grammar",
            "## Legacy filename options",
            "## Baked container boundary",
            "## Runtime loading, atlas, and caches",
            "## Authoring practices",
            "## Validation workflow",
        ):
            self.assertIn(heading, guide)
        self.assertIn("does not serialize or apply it", guide)
        self.assertIn("does not register `.spr`", guide)
        self.assertIn("JPEG, BMP, GIF, DDS", guide)
        self.assertIn("SpriteMesh.Enabled", guide)
        self.assertIn("validates the complete `SpriteMesh.*` group even when", guide)
        self.assertIn("SpriteInfo/", guide)

    def test_changed_extension_manifest_is_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_image_format.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        manifest["outputs"] = copy.deepcopy(manifest["outputs"])
        manifest["outputs"]["accepted_extensions"].remove("tga")

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "ImageFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "outputs.accepted_extensions must be"):
                docs_image_format.generate_image_format_model(ENGINE_ROOT, str(manifest_path))

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_image_format.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_image_format.OUTPUT_PATHS))
        self.assertIn("baker only by default", pages["Docs/en/reference/image-format/formats.md"])
        self.assertIn("Nested sequence flattening", pages["Docs/en/reference/image-format/fofrm.md"])
        self.assertIn("Name$cycle[-frame].bam", pages["Docs/en/reference/image-format/options.md"])
        self.assertIn("Private container header", pages["Docs/en/reference/image-format/baking.md"])
        self.assertIn("Missing-path memoization", pages["Docs/en/reference/image-format/runtime.md"])
        self.assertIn(
            "Сгенерированный справочник форматов изображений",
            pages["Docs/ru/reference/image-format/index.md"],
        )
        self.assertIn(
            "Мемоизация отсутствующего пути",
            pages["Docs/ru/reference/image-format/runtime.md"],
        )
        self.assertNotIn(
            "Accepted source formats and their current import behavior.",
            pages["Docs/ru/reference/image-format/index.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = docs_image_format.main(["--root", str(ENGINE_ROOT), "--check"])
        self.assertEqual(result, 0)

    def test_russian_pages_pin_english_hashes_and_preserve_commands(self) -> None:
        pages = docs_image_format.render_reference_pages(ENGINE_ROOT)
        for (_, document_id, _), english_path, russian_path in zip(
            docs_image_format.PAGE_DEFINITIONS,
            docs_image_format.CANONICAL_OUTPUT_PATHS,
            docs_image_format.RUSSIAN_OUTPUT_PATHS,
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
        pages = docs_image_format.generate_reference_pages(self.model)

        for filename, _, _ in docs_image_format.PAGE_DEFINITIONS:
            canonical = pages[f"Docs/en/reference/image-format/{filename}"]
            legacy = pages[f"Docs/generated/image-format/{filename}"]
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)
                self.assertIn(
                    f"../../en/reference/image-format/{filename}#{anchor}",
                    legacy,
                )

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        guide = documents["Docs/en/how-to/content/image-format.md"]
        legacy_guide = documents["Docs/ImageFormat.md"]
        self.assertEqual(
            (guide["id"], guide["state"], guide["disposition"]),
            ("image-format-guide", "current", "retain"),
        )
        self.assertEqual(
            (
                legacy_guide["id"],
                legacy_guide["state"],
                legacy_guide["disposition"],
                legacy_guide["redirect_to"],
            ),
            (
                "legacy-image-format-guide-route",
                "redirect",
                "replace",
                "image-format-guide",
            ),
        )
        for filename, document_id, _ in docs_image_format.PAGE_DEFINITIONS:
            canonical = documents[f"Docs/en/reference/image-format/{filename}"]
            legacy = documents[f"Docs/generated/image-format/{filename}"]
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
        )["generated_artifacts"]["image_format_reference"]["paths"]
        self.assertTrue(
            set(docs_image_format.RUSSIAN_OUTPUT_PATHS).issubset(generated_paths)
        )


if __name__ == "__main__":
    unittest.main()
