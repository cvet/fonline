from __future__ import annotations

import contextlib
import copy
import hashlib
import io
import json
import posixpath
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_screenshots  # noqa: E402


class DocumentationScreenshotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = docs_screenshots.load_screenshots(ENGINE_ROOT)
        cls.outputs = docs_screenshots.render_outputs(ENGINE_ROOT)
        cls.catalog = json.loads(
            cls.outputs[docs_screenshots.DEFAULT_CATALOG]
        )

    def test_manifest_owns_two_versioned_mapper_screenshots(self) -> None:
        self.assertEqual(
            [entry["id"] for entry in self.manifest["screenshots"]],
            ["mapper-particle-preview", "mapper-spark-editor"],
        )
        self.assertRegex(
            self.manifest["engine_base_revision"], r"^[0-9a-f]{40}$"
        )
        for entry in self.manifest["screenshots"]:
            self.assertEqual((entry["width"], entry["height"]), (1280, 800))
            self.assertEqual(entry["license"], "MIT")
            self.assertGreaterEqual(len(entry["source_paths"]), 9)
            self.assertGreaterEqual(len(entry["recapture_triggers"]), 3)
            self.assertGreaterEqual(
                len(entry["capture"]["interaction_steps"]), 3
            )

    def test_png_bytes_dimensions_and_hashes_match_manifest(self) -> None:
        for entry in self.manifest["screenshots"]:
            path = ENGINE_ROOT / entry["path"]
            self.assertEqual(
                docs_screenshots._png_size(path),
                (entry["width"], entry["height"]),
            )
            self.assertEqual(
                hashlib.sha256(path.read_bytes()).hexdigest(),
                entry["sha256"],
            )

    def test_catalog_hashes_every_declared_source(self) -> None:
        self.assertEqual(self.catalog["screenshot_count"], 2)
        self.assertEqual(
            self.catalog["engine_base_revision"],
            self.manifest["engine_base_revision"],
        )
        for record in self.catalog["screenshots"]:
            self.assertEqual(
                list(record["source_sha256"]), record["source_paths"]
            )
            for path, digest in record["source_sha256"].items():
                self.assertEqual(
                    hashlib.sha256(
                        docs_screenshots._normalized_text_bytes(ENGINE_ROOT / path)
                    ).hexdigest(),
                    digest,
                )

    def test_text_hashes_are_independent_of_checkout_line_endings(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            lf_path = root / "lf.txt"
            crlf_path = root / "crlf.txt"
            lf_path.write_bytes(b"first\nsecond\n")
            crlf_path.write_bytes(b"first\r\nsecond\r\n")
            self.assertEqual(
                docs_screenshots._normalized_text_bytes(lf_path),
                docs_screenshots._normalized_text_bytes(crlf_path),
            )

    def test_owning_manuals_embed_exact_accessible_metadata(self) -> None:
        owning_documents = {
            entry["owning_document"] for entry in self.manifest["screenshots"]
        }
        self.assertEqual(
            owning_documents,
            {
                "Docs/en/how-to/tools/mapper-interactive.md",
                "Docs/en/how-to/tools/particle-authoring.md",
            },
        )
        for entry in self.manifest["screenshots"]:
            text = (ENGINE_ROOT / entry["owning_document"]).read_text(
                encoding="utf-8"
            )
            image_path = posixpath.relpath(
                entry["path"],
                start=Path(entry["owning_document"]).parent.as_posix(),
            )
            self.assertIn(f'src="{image_path}"', text)
            self.assertIn(entry["alt"], text)
            self.assertIn(entry["caption"], text)

    def test_mapper_full_window_capture_uses_visible_application_profile(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_screenshots.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        entry = next(
            value
            for value in manifest["screenshots"]
            if value["id"] == "mapper-particle-preview"
        )
        config = (
            ENGINE_ROOT / "Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain"
        ).read_text(encoding="utf-8")
        mapper_methods = (
            ENGINE_ROOT / "Source/Scripting/MapperGlobalScriptMethods.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn("Name = MapperDocumentationCapture", config)
        self.assertIn("Render.HeadlessWindow = False", config)
        self.assertIn("Mapper_Game_SaveMapperScreenshot", mapper_methods)
        self.assertNotIn("RequestMapperWindowScreenshot", mapper_methods)
        self.assertIn(
            "platform screenshot tool",
            " ".join(entry["capture"]["interaction_steps"]),
        )

    def test_spark_editor_uses_the_canonical_baked_sprite_parser(self) -> None:
        editor = (
            ENGINE_ROOT / "Source/Tools/SparkParticleEditor.cpp"
        ).read_text(encoding="utf-8")
        preview = (ENGINE_ROOT / "Source/Tools/ParticleEditor.cpp").read_text(
            encoding="utf-8"
        )
        loader = (ENGINE_ROOT / "Source/Client/DefaultSprites.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("InvalidateSpriteResource", editor)
        self.assertIn('change_file_extension("spk")', editor)
        self.assertIn("SprMngr.LoadSprite", preview)
        self.assertIn("ReadSpriteResource(file.GetDataSpan())", loader)
        self.assertIn("ExtractSpriteResourceFrameImage", loader)
        self.assertNotIn("check_number == 42", loader)

    def test_invalid_hash_dimensions_and_source_are_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_screenshots.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        candidates = []
        stale_hash = copy.deepcopy(manifest)
        stale_hash["screenshots"][0]["sha256"] = "0" * 64
        candidates.append(("hash.json", stale_hash, "sha256 is stale"))
        bad_size = copy.deepcopy(manifest)
        bad_size["screenshots"][0]["width"] = 1279
        candidates.append(("size.json", bad_size, "dimensions are"))
        missing_source = copy.deepcopy(manifest)
        missing_source["screenshots"][0]["source_paths"][0] = "Source/Missing.cpp"
        candidates.append(("source.json", missing_source, "does not exist"))

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            for name, candidate, message in candidates:
                path = root / name
                path.write_text(json.dumps(candidate), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    docs_screenshots.load_screenshots(
                        ENGINE_ROOT,
                        str(path),
                        validate_embeddings=False,
                    )

    def test_output_is_current_and_ci_routes_the_validator(self) -> None:
        self.assertEqual(
            tuple(self.outputs), docs_screenshots.OUTPUT_PATHS
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_screenshots.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)
        workflow = (
            ENGINE_ROOT / ".github/workflows/validate.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("BuildTools/tests/test_docs_screenshots.py", workflow)
        self.assertIn("BuildTools/docs_screenshots.py --check", workflow)


if __name__ == "__main__":
    unittest.main()
