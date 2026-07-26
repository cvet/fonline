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

import docs_map_format  # noqa: E402


class MapFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_map_format.generate_map_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_source_backed(self) -> None:
        second = docs_map_format.generate_map_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_map_format.SCHEMA_VERSION)
        self.assertEqual(
            [entry["name"] for entry in self.model["sections"]],
            ["Map prototype", "Critter placement", "Item placement"],
        )
        self.assertEqual(
            {entry["name"]: entry["value"] for entry in self.model["ownerships"]},
            {"MapHex": 0, "CritterInventory": 1, "ItemContainer": 2, "Nowhere": 3},
        )
        self.assertGreater(self.model["summary"]["authorable_property_count"], 0)
        self.assertGreater(self.model["summary"]["excluded_property_count"], 0)

        identities = [
            entry["id"]
            for key in ("sections", "directives", "ownerships", "rules", "properties")
            for entry in self.model[key]
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertTrue(
            all(docs_map_format.ENTRY_ID_PATTERN.fullmatch(identity) for identity in identities)
        )

    def test_property_catalog_matches_receiver_and_side_rules(self) -> None:
        properties = {
            (entry["receiver"], entry["name"]): entry for entry in self.model["properties"]
        }

        self.assertTrue(properties[("Item", "Count")]["authorable"])
        self.assertFalse(properties[("Critter", "AttachMaster")]["authorable"])
        self.assertEqual(
            properties[("Critter", "AttachMaster")]["excluded_reason"],
            "temporary",
        )
        self.assertEqual(properties[("Critter", "InitScript")]["runtime_sides"], ["server"])
        self.assertEqual(
            properties[("Critter", "InitScript")]["skipped_sides"],
            ["client", "mapper"],
        )
        self.assertTrue(properties[("Map", "Size")]["authorable"])

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in ("sections", "directives", "ownerships", "rules"):
            for entry in self.model[collection]:
                source = entry["source"]
                source_text = (ENGINE_ROOT / source["path"]).read_text(encoding="utf-8")
                for anchor in source["anchors"]:
                    self.assertIn(anchor, source_text)

    def test_duplicate_manifest_identity_is_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_map_format.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        manifest["rules"].append(copy.deepcopy(manifest["rules"][0]))
        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "MapFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate rules id"):
                docs_map_format.generate_map_format_model(ENGINE_ROOT, str(manifest_path))

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_map_format.generate_reference_pages(self.model)

        self.assertEqual(set(pages), set(docs_map_format.OUTPUT_PATHS))
        syntax = pages["Docs/generated/map-format/syntax.md"]
        baking = pages["Docs/generated/map-format/baking.md"]
        validation = pages["Docs/generated/map-format/validation.md"]
        self.assertIn("[ProtoMap]", syntax)
        self.assertIn("[$Name/Critter]", syntax)
        self.assertIn("[$Name/Item]", syntax)
        self.assertIn("one or more", syntax)
        self.assertNotIn("[Header]", syntax)
        self.assertIn("<code>Nowhere</code>", baking)
        self.assertIn("Map-supported", baking)
        self.assertIn("map-format.rule.mapper-round-trip", validation)
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = docs_map_format.main(["--root", str(ENGINE_ROOT), "--check"])
        self.assertEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
