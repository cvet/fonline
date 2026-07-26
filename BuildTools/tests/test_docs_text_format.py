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

import docs_text_format  # noqa: E402


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
        self.assertIn("default is the first variant", current["behavior"])
        self.assertIn("negative index throws", current["missing_behavior"])
        self.assertEqual(current["sides"], ["client", "mapper"])
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
        guide = (ENGINE_ROOT / "Docs/TextAndLocalization.md").read_text(
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
        self.assertIn("`skipCount = 0` selects the first variant", guide)
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
        pages = docs_text_format.generate_reference_pages(self.model)

        self.assertEqual(set(pages), set(docs_text_format.OUTPUT_PATHS))
        self.assertIn(
            "first opening brace",
            pages["Docs/generated/text-format/syntax.md"],
        )
        self.assertIn(
            "Bake-time fallback",
            pages["Docs/generated/text-format/languages.md"],
        )
        self.assertIn(
            "StringEscaping",
            pages["Docs/generated/text-format/proto-text.md"],
        )
        self.assertIn(
            "no server script",
            pages["Docs/generated/text-format/runtime.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_text_format.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
