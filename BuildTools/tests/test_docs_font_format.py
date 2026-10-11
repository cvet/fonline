from __future__ import annotations

import contextlib
import copy
import io
import json
import re
import struct
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_font_format  # noqa: E402
import docs_localization  # noqa: E402


def _read_bmfont(path: Path) -> tuple[dict[int, bytes], list[tuple[int, int, int, int]]]:
    data = path.read_bytes()
    if data[:4] != b"BMF\x03":
        raise AssertionError(f"unexpected BMFont header in {path}")
    blocks: dict[int, bytes] = {}
    offset = 4
    while offset < len(data):
        block_type = data[offset]
        block_size = struct.unpack_from("<I", data, offset + 1)[0]
        payload_begin = offset + 5
        payload_end = payload_begin + block_size
        if payload_end > len(data):
            raise AssertionError(f"truncated BMFont block in {path}")
        blocks[block_type] = data[payload_begin:payload_end]
        offset = payload_end
    chars = blocks[4]
    if len(chars) % 20 != 0:
        raise AssertionError(f"invalid BMFont chars block in {path}")
    metrics = [
        (codepoint, xoffset, yoffset, xadvance)
        for codepoint, _x, _y, _width, _height, xoffset, yoffset, xadvance, _page, _channel in struct.iter_unpack(
            "<IHHHHhhhBB", chars
        )
    ]
    return blocks, metrics


class FontFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_font_format.generate_font_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_contract(self) -> None:
        second = docs_font_format.generate_font_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_font_format.SCHEMA_VERSION)
        outputs = self.model["outputs"]
        self.assertEqual(outputs["runtime_extensions"], ["fofnt", "fnt"])
        self.assertEqual(outputs["authoring_sidecar_extensions"], ["bmfc"])
        self.assertTrue(outputs["raw_copy_passthrough"])
        self.assertEqual(outputs["fofnt_max_version"], 2)
        self.assertEqual(outputs["bmfont"]["version"], 3)
        self.assertEqual(outputs["bmfont"]["char_record_size"], 20)
        self.assertEqual(outputs["atlas_type"], "IfaceSprites")
        self.assertEqual(outputs["cache_invalidation_frames"], 3)
        self.assertEqual(self.model["summary"]["entry_count"], 57)

    def test_fofnt_keys_slots_flags_and_scale_are_source_derived(self) -> None:
        outputs = self.model["outputs"]

        self.assertEqual(
            outputs["fofnt_keys"],
            [
                "Version",
                "Image",
                "LineHeight",
                "YAdvance",
                "End",
                "Letter",
                "PositionX",
                "PositionY",
                "Width",
                "Height",
                "OffsetX",
                "OffsetY",
                "XAdvance",
            ],
        )
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
        self.assertEqual(outputs["default_scale"], 1.0)
        self.assertEqual(
            outputs["scale_range"],
            {"minimum_exclusive": 0.0, "maximum_inclusive": 1.0},
        )

    def test_bundled_binary_fonts_expose_unsigned_loader_limitation(self) -> None:
        source = (ENGINE_ROOT / "Source/Client/FontManager.cpp").read_text(
            encoding="utf-8"
        )

        for variable in ("ox", "oy", "xa"):
            self.assertIn(
                f"uint16_t {variable} = reader.GetLEUInt16();", source
            )
            self.assertNotIn(
                f"int16_t {variable} = reader.GetLEInt16();", source
            )

        for filename in ("CourierNewSmall.fnt", "DefaultExt.fnt"):
            blocks, metrics = _read_bmfont(
                ENGINE_ROOT / "Resources/Core/Fonts" / filename
            )
            self.assertEqual(list(blocks)[:4], [1, 2, 3, 4])
            self.assertEqual(blocks[1][7:11], b"\x01\x01\x01\x01")
            self.assertEqual(struct.unpack_from("<H", blocks[2], 8)[0], 1)
            self.assertTrue(metrics)
            self.assertTrue(
                any(xoffset < 0 or yoffset < 0 for _, xoffset, yoffset, _ in metrics),
                f"{filename} must retain a negative-bearing regression fixture",
            )

    def test_fofnt_missing_image_guard_is_a_documented_limitation(self) -> None:
        source = (ENGINE_ROOT / "Source/Client/FontManager.cpp").read_text(
            encoding="utf-8"
        )

        back_access = source.index("if (image_name.back() == '*')")
        self.assertGreater(back_access, 0)
        self.assertNotIn("if (image_name.empty())", source)
        self.assertNotIn('"Font image is not specified"', source)

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in docs_font_format.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8", errors="replace"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_formats_layout_and_project_boundary(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/font-format.md"
        ).read_text(encoding="utf-8")

        for heading in (
            "## Supported resources",
            "## Minimal FOFNT",
            "## FOFNT metrics",
            "## Binary BMFont",
            "## Binding font slots",
            "## Bind-time scale",
            "## TextFormat and layout",
            "## Measurement and drawing",
            "## Color and effects",
            "## Recommended project practice",
            "## Validation workflow",
        ):
            self.assertIn(heading, guide)
        self.assertIn("does not load BMFont text/XML descriptors", guide)
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
        manifest["outputs"]["bmfont"]["unsigned_fields"] = ["xoffset", "yoffset"]

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "FontFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "outputs.bmfont must match the live source"
            ):
                docs_font_format.generate_font_format_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_font_format.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_font_format.OUTPUT_PATHS))
        self.assertIn(
            "Runtime descriptors | <code>.fofnt</code>, <code>.fnt</code>",
            pages["Docs/en/reference/font-format/index.md"],
        )
        self.assertIn("Raw-copied authoring sidecar", pages["Docs/en/reference/font-format/formats.md"])
        self.assertIn("OffsetX", pages["Docs/en/reference/font-format/fofnt.md"])
        self.assertIn("Unsigned BMFont metric limitation", pages["Docs/en/reference/font-format/validation.md"])
        self.assertIn("<code>NoWrap</code>", pages["Docs/en/reference/font-format/layout.md"])
        self.assertIn(
            "Сгенерированный справочник форматов шрифтов",
            pages["Docs/ru/reference/font-format/index.md"],
        )
        self.assertIn(
            "Уменьшение при привязке",
            pages["Docs/ru/reference/font-format/binding.md"],
        )
        self.assertNotIn(
            "Descriptor roles and supported resource suffixes.",
            pages["Docs/ru/reference/font-format/index.md"],
        )
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
        document_ids = {document["id"] for document in manifest["documents"].values()}
        self.assertIn("font-format-guide", document_ids)
        self.assertIn("generated-font-format-index", document_ids)
        self.assertIn('"font-format"', contract_diff)
        self.assertIn("docs_font_format", validate)


if __name__ == "__main__":
    unittest.main()
