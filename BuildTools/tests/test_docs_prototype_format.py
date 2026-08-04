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

import docs_prototype_format  # noqa: E402
import docs_localization  # noqa: E402


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
        self.assertIn(
            "prototype-format.rule.identifier-characters",
            {entry["id"] for entry in self.model["rules"]},
        )
        self.assertEqual(self.model["summary"]["rule_count"], 13)

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

    def test_human_guide_covers_authoring_and_project_boundaries(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/prototype-format.md"
        ).read_text(encoding="utf-8")

        for heading in (
            "## From source file to baked prototype",
            "## Configuration syntax",
            "## Inheritance",
            "## Property applicability",
            "## Migrations",
            "## Validation workflow",
        ):
            self.assertIn(heading, guide)
        self.assertIn("must not contain `/` or `$`", guide)
        self.assertIn("`ProtoBaker` interprets `$Name` and `$Parent`", guide)
        self.assertIn("section and metadata, not the extension or directory", guide)

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
        pages = docs_prototype_format.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_prototype_format.OUTPUT_PATHS))
        self.assertIn("Parent graphs must be acyclic", pages[
            "Docs/en/reference/prototype-format/validation.md"
        ])
        self.assertIn("[ProtoItem]", pages["Docs/en/reference/prototype-format/syntax.md"])
        self.assertIn("[ProtoMap]", pages["Docs/en/reference/prototype-format/syntax.md"])
        self.assertNotIn("[Header]", pages["Docs/en/reference/prototype-format/syntax.md"])
        self.assertIn("InitScript callback signatures", pages[
            "Docs/en/reference/prototype-format/validation.md"
        ])
        self.assertIn("`Critter` properties", pages[
            "Docs/en/reference/prototype-format/properties.md"
        ])
        russian_validation = pages[
            "Docs/ru/reference/prototype-format/validation.md"
        ]
        self.assertIn("Ациклическое наследование", russian_validation)
        self.assertIn("Граф родителей прототипов должен быть ациклическим.", russian_validation)
        self.assertNotIn("Acyclic inheritance", russian_validation)
        russian_properties = pages[
            "Docs/ru/reference/prototype-format/properties.md"
        ]
        self.assertIn("## Свойства `Critter`", russian_properties)
        self.assertIn(" | нет (temporary) | ", russian_properties)
        self.assertIn(" | да | ", russian_properties)
        canonical_index = pages[
            "Docs/en/reference/prototype-format/index.md"
        ]
        self.assertIn(
            docs_localization.translation_metadata_line(
                "generated-prototype-format-index",
                "Docs/en/reference/prototype-format/index.md",
                docs_localization.normalized_sha256(canonical_index),
            ),
            pages["Docs/ru/reference/prototype-format/index.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = docs_prototype_format.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)

    def test_legacy_generated_routes_preserve_headings_and_entry_anchors(self) -> None:
        pages = docs_prototype_format.generate_reference_pages(self.model)

        for filename, _, _ in docs_prototype_format.PAGE_DEFINITIONS:
            canonical = pages[f"Docs/en/reference/prototype-format/{filename}"]
            legacy = pages[f"Docs/generated/prototype-format/{filename}"]
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)
                self.assertIn(
                    f"../../en/reference/prototype-format/{filename}#{anchor}",
                    legacy,
                )

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        canonical_guide = documents[
            "Docs/en/how-to/content/prototype-format.md"
        ]
        self.assertEqual(
            (
                canonical_guide["id"],
                canonical_guide["state"],
                canonical_guide["disposition"],
            ),
            ("prototype-format-guide", "current", "retain"),
        )
        legacy_guide = documents["Docs/PrototypeFormat.md"]
        self.assertEqual(
            (
                legacy_guide["state"],
                legacy_guide["disposition"],
                legacy_guide["redirect_to"],
            ),
            ("redirect", "replace", "prototype-format-guide"),
        )
        for filename, document_id, _ in docs_prototype_format.PAGE_DEFINITIONS:
            canonical = documents[
                f"Docs/en/reference/prototype-format/{filename}"
            ]
            legacy = documents[f"Docs/generated/prototype-format/{filename}"]
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
