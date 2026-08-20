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

import docs_text_format  # noqa: E402
import docs_localization  # noqa: E402


class TextFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_text_format.generate_text_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_defaults(self) -> None:
        second = docs_text_format.generate_text_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_text_format.SCHEMA_VERSION)
        self.assertEqual(
            self.model["setting_defaults"]["Baking.BakeLanguages"], ["engl"]
        )
        self.assertEqual(self.model["setting_defaults"]["Client.Language"], "engl")
        self.assertEqual(
            self.model["outputs"]["prototype_packs"],
            ["Items", "Critters", "Maps", "Locations", "Protos"],
        )
        identities = [
            entry["id"]
            for collection in docs_text_format.COLLECTION_KINDS
            for entry in self.model[collection]
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertTrue(
            all(
                docs_text_format.ENTRY_ID_PATTERN.fullmatch(identity)
                for identity in identities
            )
        )

    def test_raw_syntax_and_language_contract_preserve_edge_cases(self) -> None:
        rules = {
            entry["id"]: entry
            for collection in ("syntax_rules", "language_rules")
            for entry in self.model[collection]
        }

        self.assertIn(
            "no fixed character count",
            rules["text-format.language.identifier"]["requirement"],
        )
        self.assertIn(
            "first opening brace",
            rules["text-format.syntax.comment-boundary"]["requirement"],
        )
        self.assertIn(
            "not the number or ordering",
            rules["text-format.language.variant-cardinality"]["requirement"],
        )
        self.assertEqual(self.model["outputs"]["raw_key3"], "empty")

    def test_runtime_contract_is_indexed_and_side_specific(self) -> None:
        methods = {
            entry["id"]: entry for entry in self.model["runtime_methods"]
        }

        current = methods["text-format.runtime.get-current-text"]
        self.assertIn("first variant", current["behavior"])
        self.assertIn("missing key returns an empty string", current["missing_behavior"].lower())
        self.assertEqual(current["sides"], ["client", "mapper"])
        indexed = methods["text-format.runtime.get-indexed-text"]
        self.assertIn("zero-based variant", indexed["behavior"])
        self.assertIn("negative index throws", indexed["missing_behavior"])
        self.assertEqual(indexed["sides"], ["client", "mapper"])
        self.assertIn(
            "no automatic fallback",
            methods["text-format.runtime.get-language-text"]["missing_behavior"],
        )
        self.assertIn(
            "empty langName",
            methods["text-format.runtime.get-language-text"]["behavior"],
        )
        self.assertIn(
            "no server script Game.GetText",
            methods["text-format.runtime.server-boundary"]["missing_behavior"],
        )

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in docs_text_format.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_engine_and_project_boundaries(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/text-and-localization.md"
        ).read_text(
            encoding="utf-8"
        )

        for heading in (
            "## Raw `.fotxt` files",
            "## Language normalization",
            "## Prototype `$Text` fields",
            "## Runtime script API",
            "## Engine and project formatting boundary",
            "## Validation workflow",
        ):
            self.assertIn(heading, guide)
        self.assertIn("fallback is completed during baking", guide)
        self.assertIn("`Game.GetText(key)` selects", guide)
        self.assertIn("does not define `@pname@`", guide)

    def test_source_derived_proto_pack_drift_is_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_text_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        manifest["outputs"] = copy.deepcopy(manifest["outputs"])
        manifest["outputs"]["prototype_packs"].pop()

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "TextFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "prototype_packs diverges from ProtoTextBaker"
            ):
                docs_text_format.generate_text_format_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_text_format.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_text_format.OUTPUT_PATHS))
        self.assertIn(
            "first opening brace",
            pages["Docs/en/reference/text-format/syntax.md"],
        )
        self.assertIn(
            "Bake-time fallback",
            pages["Docs/en/reference/text-format/languages.md"],
        )
        self.assertIn(
            "StringEscaping",
            pages["Docs/en/reference/text-format/proto-text.md"],
        )
        self.assertIn(
            "no server script",
            pages["Docs/en/reference/text-format/runtime.md"],
        )
        russian_languages = pages["Docs/ru/reference/text-format/languages.md"]
        russian_runtime = pages["Docs/ru/reference/text-format/runtime.md"]
        self.assertIn("Fallback при запекании", russian_languages)
        self.assertNotIn("Bake-time fallback", russian_languages)
        self.assertIn("Возвращает число вариантов, сохранённых под полным ключом.", russian_runtime)
        self.assertNotIn("Returns the number of variants", russian_runtime)
        for filename, document_id, _ in docs_text_format.PAGE_DEFINITIONS:
            english_page = pages[f"{docs_text_format.DEFAULT_OUTPUT_DIR}/{filename}"]
            russian_page = pages[f"{docs_text_format.RUSSIAN_OUTPUT_DIR}/{filename}"]
            self.assertEqual(
                re.findall(r"```[^\n]*\n(.*?)```", english_page, flags=re.DOTALL),
                re.findall(r"```[^\n]*\n(.*?)```", russian_page, flags=re.DOTALL),
            )
            self.assertIn(
                docs_localization.translation_metadata_line(
                    document_id,
                    f"{docs_text_format.DEFAULT_OUTPUT_DIR}/{filename}",
                    docs_localization.normalized_sha256(english_page),
                ),
                russian_page,
            )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_text_format.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)

    def test_legacy_generated_routes_preserve_headings_and_entry_anchors(self) -> None:
        pages = docs_text_format.generate_reference_pages(self.model)

        for filename, _, _ in docs_text_format.PAGE_DEFINITIONS:
            canonical = pages[f"Docs/en/reference/text-format/{filename}"]
            legacy = pages[f"Docs/generated/text-format/{filename}"]
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)
                self.assertIn(
                    f"../../en/reference/text-format/{filename}#{anchor}",
                    legacy,
                )

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        guide = documents["Docs/en/how-to/content/text-and-localization.md"]
        legacy_guide = documents["Docs/TextAndLocalization.md"]
        self.assertEqual(
            (guide["id"], guide["state"], guide["disposition"]),
            ("text-and-localization-guide", "current", "retain"),
        )
        self.assertEqual(
            (
                legacy_guide["id"],
                legacy_guide["state"],
                legacy_guide["disposition"],
                legacy_guide["redirect_to"],
            ),
            (
                "legacy-text-and-localization-guide-route",
                "redirect",
                "replace",
                "text-and-localization-guide",
            ),
        )
        for filename, document_id, _ in docs_text_format.PAGE_DEFINITIONS:
            canonical = documents[f"Docs/en/reference/text-format/{filename}"]
            legacy = documents[f"Docs/generated/text-format/{filename}"]
            self.assertEqual(
                (canonical["id"], canonical["state"], canonical["disposition"]),
                (document_id, "current", "retain"),
            )
            self.assertEqual(
                (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
                ("redirect", "replace", document_id),
            )


if __name__ == "__main__":
    unittest.main()
