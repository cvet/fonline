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

import docs_font_format  # noqa: E402
import docs_localization  # noqa: E402


class FontFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_font_format.generate_font_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_contract(self) -> None:
        second = docs_font_format.generate_font_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_font_format.SCHEMA_VERSION)
        outputs = self.model["outputs"]
        self.assertEqual(outputs["runtime_extensions"], ["ttf"])
        self.assertEqual(outputs["raw_copy_extensions"], ["ttf", "ogv", "json", "ini"])
        self.assertTrue(outputs["raw_copy_passthrough"])
        self.assertTrue(outputs["bitmap_loaders_removed"])
        self.assertEqual(
            outputs["script_bind_overloads"],
            [
                "FontType font, readonly_vector<string> faceFnames, int32_t pixelSize, "
                "FontOptions options, readonly_vector<int32_t> preloadRanges"
            ],
        )
        self.assertEqual(outputs["atlas_type"], "IfaceSprites")
        self.assertEqual(outputs["cache_invalidation_frames"], 3)
        self.assertEqual(self.model["summary"]["entry_count"], 45)
        self.assertNotIn("fofnt_fields", self.model)
        self.assertNotIn("bmfont_rules", self.model)

    def test_default_font_and_embedded_capacity_are_source_derived(self) -> None:
        outputs = self.model["outputs"]

        self.assertEqual(
            outputs["default_font"],
            {
                "path": "Fonts/FOnline-Regular.ttf",
                "pixel_size": 10,
                "pack": "Embedded",
                "hinting": "Normal",
                "outline_width": 1,
                "preload_ranges": [32, 126, 160, 255, 1024, 1279],
                "atlas_type": "IfaceSprites",
                "bound_by": ["Client", "Mapper", "Updater"],
            },
        )
        self.assertEqual(outputs["embedded_data_capacity_default"], 400000)
        self.assertEqual(outputs["shipped_faces"]["Embedded"], ["FOnline-Regular.ttf"])
        for pack, names in outputs["shipped_faces"].items():
            for name in names:
                self.assertTrue(
                    (ENGINE_ROOT / "Resources" / pack / "Fonts" / name).is_file()
                )

    def test_colored_faces_and_converter_facts_are_source_derived(self) -> None:
        outputs = self.model["outputs"]

        self.assertEqual(
            outputs["colored_faces"],
            {
                "freetype_color_layers": True,
                "load_flags": "FT_LOAD_COLOR | FT_LOAD_NO_HINTING",
                "render_mode": "FT_RENDER_MODE_NORMAL",
                "neutral_glyph_grey": 128,
            },
        )
        converter = outputs["converter"]
        self.assertEqual(converter["units_per_pixel"], 100)
        self.assertEqual(converter["native_size_range"], [1, 255])
        self.assertEqual(
            converter["neutral_grey"], outputs["colored_faces"]["neutral_glyph_grey"]
        )
        self.assertEqual(converter["input_extensions"], ["fofnt", "fnt"])
        self.assertEqual(converter["modes"], ["--check"])
        self.assertEqual(
            converter["python_packages"], ["fontTools", "numpy", "PIL", "potrace"]
        )
        self.assertEqual(
            converter["colored_faces"],
            ["FOnline-BigNumbers.ttf", "FOnline-Numbers.ttf", "FOnline-SandNumbers.ttf"],
        )
        shipped = sorted(
            path.name
            for directory in ("Resources/Embedded/Fonts", "Resources/Core/Fonts")
            for path in (ENGINE_ROOT / directory).glob("*.ttf")
        )
        self.assertEqual(converter["shipped_face_count"], len(shipped))
        self.assertEqual(
            sorted(name for names in outputs["shipped_faces"].values() for name in names),
            shipped,
        )

    def test_slots_flags_and_limits_are_source_derived(self) -> None:
        outputs = self.model["outputs"]

        self.assertEqual(
            [(entry["name"], entry["value"]) for entry in outputs["font_slots"]],
            [("Default", 0)],
        )
        self.assertEqual(
            [(entry["name"], entry["value"]) for entry in outputs["font_flags"]],
            [
                ("None", 0),
                ("NoWrap", 1),
                ("TruncateLine", 2),
                ("CenterX", 4),
                ("CenterY", 8),
                ("AlignRight", 16),
                ("AlignBottom", 32),
                ("KeepTail", 64),
                ("NoColorize", 128),
                ("Justify", 256),
                ("Bordered", 512),
            ],
        )
        self.assertEqual(
            outputs["truetype_limits"],
            {
                "max_pixel_size": 256,
                "max_outline_width": 8,
                "max_preload_letters": 65536,
                "sheet_max_side": 2048,
            },
        )

    def test_runtime_has_no_bitmap_font_loader(self) -> None:
        manager = (ENGINE_ROOT / "Source/Client/FontManager.cpp").read_text(encoding="utf-8")
        header = (ENGINE_ROOT / "Source/Client/FontManager.h").read_text(encoding="utf-8")
        scripts = (
            ENGINE_ROOT / "Source/Scripting/ClientGlobalScriptMethods.cpp"
        ).read_text(encoding="utf-8")

        for marker in ("BindFoFont", "BindBmfFont", "BuildFont", "BakeFontScale", "fofnt"):
            self.assertNotIn(marker, manager)
            self.assertNotIn(marker, header)
            self.assertNotIn(marker, scripts)
        self.assertEqual(scripts.count("FO_SCRIPT_API void Client_Game_BindFont("), 1)
        self.assertEqual(
            [
                path.name
                for path in (ENGINE_ROOT / "Resources").rglob("*")
                if path.suffix in {".fofnt", ".fnt", ".bmfc"}
            ],
            [],
        )
        self.assertFalse((ENGINE_ROOT / "Resources/FontSources").exists())

    def test_default_font_is_a_truetype_face(self) -> None:
        path = (
            ENGINE_ROOT / "Resources/Embedded" / self.model["outputs"]["default_font"]["path"]
        )

        self.assertEqual(path.read_bytes()[:4], b"\x00\x01\x00\x00")

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in docs_font_format.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8", errors="replace"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_truetype_default_font_converter_and_boundary(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/font-format.md"
        ).read_text(encoding="utf-8")

        for heading in (
            "## Supported resources",
            "## Default font",
            "## Binding font slots",
            "## TrueType fonts",
            "## Colored fonts",
            "## Converting bitmap fonts",
            "## TextFormat and layout",
            "## Measurement and drawing",
            "## Color and effects",
            "## Recommended project practice",
            "## Validation workflow",
        ):
            self.assertIn(heading, guide)
        for removed in (
            "## Minimal FOFNT",
            "## FOFNT metrics",
            "## Binary BMFont",
            "## Bind-time scale",
        ):
            self.assertNotIn(removed, guide)
        self.assertIn("FOnline-Regular.ttf", guide)
        self.assertIn("FO_EMBEDDED_DATA_CAPACITY", guide)
        self.assertIn("bitmap_font_to_ttf.py", guide)
        self.assertIn("NoWrap` truncates drawing", guide)
        self.assertIn("licensing", guide)
        self.assertIn("embedding project owns", guide)

    def test_changed_derived_manifest_values_are_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_font_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        manifest["outputs"] = copy.deepcopy(manifest["outputs"])
        manifest["outputs"]["default_font"]["pixel_size"] = 12

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "FontFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "outputs.default_font must match the live source"
            ):
                docs_font_format.generate_font_format_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_font_format.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_font_format.OUTPUT_PATHS))
        self.assertIn(
            "Runtime faces | <code>.ttf</code>",
            pages["Docs/en/reference/font-format/index.md"],
        )
        self.assertIn(
            "COLR version 0 color layers over a CPAL palette",
            pages["Docs/en/reference/font-format/formats.md"],
        )
        self.assertIn(
            "Engine default font", pages["Docs/en/reference/font-format/binding.md"]
        )
        self.assertIn(
            "bitmap_font_to_ttf.py SOURCE.fofnt OUTPUT.ttf",
            pages["Docs/en/reference/font-format/authoring.md"],
        )
        self.assertIn("<code>NoWrap</code>", pages["Docs/en/reference/font-format/layout.md"])
        self.assertIn(
            "Colored glyph rendering", pages["Docs/en/reference/font-format/rendering.md"]
        )
        self.assertIn(
            "Converted faces are current",
            pages["Docs/en/reference/font-format/validation.md"],
        )
        self.assertIn(
            "Сгенерированный справочник форматов шрифтов",
            pages["Docs/ru/reference/font-format/index.md"],
        )
        self.assertIn(
            "Шрифт движка по умолчанию",
            pages["Docs/ru/reference/font-format/binding.md"],
        )
        self.assertNotIn(
            "Runtime face formats, converter sources, and shipped faces.",
            pages["Docs/ru/reference/font-format/index.md"],
        )
        self.assertFalse([path for path in pages if path.endswith(("fofnt.md", "bmfont.md"))])
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = docs_font_format.main(["--root", str(ENGINE_ROOT), "--check"])
        self.assertEqual(result, 0)

    def test_russian_pages_pin_english_hashes_and_preserve_commands(self) -> None:
        pages = docs_font_format.render_reference_pages(ENGINE_ROOT)
        for (_, document_id, _), english_path, russian_path in zip(
            docs_font_format.PAGE_DEFINITIONS,
            docs_font_format.CANONICAL_OUTPUT_PATHS,
            docs_font_format.RUSSIAN_OUTPUT_PATHS,
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
        pages = docs_font_format.generate_reference_pages(self.model)

        for filename, _, _ in docs_font_format.PAGE_DEFINITIONS:
            canonical = pages[f"Docs/en/reference/font-format/{filename}"]
            legacy = pages[f"Docs/generated/font-format/{filename}"]
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)
                self.assertIn(
                    f"../../en/reference/font-format/{filename}#{anchor}",
                    legacy,
                )

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        canonical_guide = documents["Docs/en/how-to/content/font-format.md"]
        self.assertEqual(
            (
                canonical_guide["id"],
                canonical_guide["state"],
                canonical_guide["disposition"],
            ),
            ("font-format-guide", "current", "retain"),
        )
        legacy_guide = documents["Docs/FontFormat.md"]
        self.assertEqual(
            (
                legacy_guide["state"],
                legacy_guide["disposition"],
                legacy_guide["redirect_to"],
            ),
            ("redirect", "replace", "font-format-guide"),
        )
        for filename, document_id, _ in docs_font_format.PAGE_DEFINITIONS:
            canonical = documents[f"Docs/en/reference/font-format/{filename}"]
            legacy = documents[f"Docs/generated/font-format/{filename}"]
            self.assertEqual(
                (canonical["id"], canonical["state"], canonical["disposition"]),
                (document_id, "current", "retain"),
            )
            self.assertEqual(
                (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
                ("redirect", "replace", document_id),
            )
        for removed in ("fofnt.md", "bmfont.md"):
            self.assertNotIn(f"Docs/en/reference/font-format/{removed}", documents)
            self.assertNotIn(f"Docs/generated/font-format/{removed}", documents)
        generated_paths = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["generated_artifacts"]["font_format_reference"]["paths"]
        self.assertTrue(
            set(docs_font_format.RUSSIAN_OUTPUT_PATHS).issubset(generated_paths)
        )

    def test_ci_manifest_and_contract_diff_route_the_domain(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        manifest = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        contract_diff = (ENGINE_ROOT / "BuildTools/docs_contract_diff.py").read_text(
            encoding="utf-8"
        )
        validate = (ENGINE_ROOT / "BuildTools/docs_validate.py").read_text(
            encoding="utf-8"
        )

        self.assertIn("BuildTools/docs_font_format.py --check", workflow)
        self.assertIn("BuildTools/tests/test_bitmap_font_to_ttf.py", workflow)
        document_ids = {document["id"] for document in manifest["documents"].values()}
        self.assertIn("font-format-guide", document_ids)
        self.assertIn("generated-font-format-index", document_ids)
        self.assertIn('"font-format"', contract_diff)
        self.assertIn("docs_font_format", validate)


if __name__ == "__main__":
    unittest.main()
