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

import docs_prototype_format  # noqa: E402


class PrototypeFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_prototype_format.generate_prototype_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_source_backed(self) -> None:
        second = docs_prototype_format.generate_prototype_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_prototype_format.SCHEMA_VERSION)
        self.assertEqual(
            self.model["file_selection"]["engine_defaults"],
            ["fopro"],
        )
        self.assertEqual(
            {entry["name"] for entry in self.model["entity_types"]},
            {"Critter", "Item", "Location", "Map"},
        )
        self.assertGreater(self.model["summary"]["authorable_property_count"], 0)
        self.assertGreater(self.model["summary"]["excluded_property_count"], 0)
        self.assertIn(
            "prototype-format.rule.init-script-signature",
            {entry["id"] for entry in self.model["rules"]},
        )

        identities = [
            entry["id"]
            for key in ("section_forms", "directives", "rules", "entity_types", "properties")
            for entry in self.model[key]
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertTrue(
            all(docs_prototype_format.ENTRY_ID_PATTERN.fullmatch(identity) for identity in identities)
        )

    def test_property_catalog_matches_temporary_and_side_rules(self) -> None:
        properties = {
            (entry["entity"], entry["name"]): entry for entry in self.model["properties"]
        }

        self.assertTrue(properties[("Item", "Count")]["authorable"])
        self.assertFalse(properties[("Critter", "AttachMaster")]["authorable"])
        self.assertEqual(
            properties[("Critter", "AttachMaster")]["excluded_reason"],
            "temporary",
        )
        self.assertEqual(
            properties[("Critter", "InitScript")]["runtime_sides"],
            ["server"],
        )
        self.assertEqual(
            properties[("Critter", "InitScript")]["skipped_sides"],
            ["client", "mapper"],
        )
        self.assertTrue(properties[("Critter", "InitScript")]["authorable"])

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in ("section_forms", "directives", "rules"):
            for entry in self.model[collection]:
                source = entry["source"]
                source_text = (ENGINE_ROOT / source["path"]).read_text(encoding="utf-8")
                for anchor in source["anchors"]:
                    self.assertIn(anchor, source_text)

    def test_duplicate_manifest_identity_is_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_prototype_format.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        duplicate = copy.deepcopy(manifest["rules"][0])
        manifest["rules"].append(duplicate)
        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "PrototypeFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate rules id"):
                docs_prototype_format.generate_prototype_format_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_prototype_format.generate_reference_pages(self.model)

        self.assertEqual(set(pages), set(docs_prototype_format.OUTPUT_PATHS))
        self.assertIn("Parent graphs must be acyclic", pages[
            "Docs/generated/prototype-format/validation.md"
        ])
        self.assertIn("[ProtoItem]", pages["Docs/generated/prototype-format/syntax.md"])
        self.assertIn("[ProtoMap]", pages["Docs/generated/prototype-format/syntax.md"])
        self.assertNotIn("[Header]", pages["Docs/generated/prototype-format/syntax.md"])
        self.assertIn("InitScript callback signatures", pages[
            "Docs/generated/prototype-format/validation.md"
        ])
        self.assertIn("`Critter` properties", pages[
            "Docs/generated/prototype-format/properties.md"
        ])
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = docs_prototype_format.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
