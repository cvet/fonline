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

import docs_particle_format  # noqa: E402


class ParticleFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_particle_format.generate_particle_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_boundaries(self) -> None:
        second = docs_particle_format.generate_particle_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_particle_format.SCHEMA_VERSION)
        outputs = self.model["outputs"]
        self.assertEqual(outputs["authored_extensions"], ["spark", "efkproj"])
        self.assertEqual(outputs["runtime_extensions"], ["spk", "efk"])
        self.assertEqual(
            outputs["backend_options"],
            {
                "spark": "FO_SPARK_PARTICLES",
                "effekseer": "FO_EFFEKSEER_PARTICLES",
            },
        )
        self.assertEqual(
            outputs["bake_transforms"], {"spark": "spk", "efkproj": "efk"}
        )
        self.assertTrue(outputs["authored_binaries_rejected"])
        self.assertEqual(self.model["summary"]["entry_count"], 106)
        self.assertEqual(self.model["summary"]["format_count"], 4)
        self.assertEqual(self.model["summary"]["graph_object_count"], 37)

    def test_formats_distinguish_authored_and_generated_resources(self) -> None:
        formats = {
            entry["format"]: entry
            for entry in self.model["objects"]
            if "format" in entry
        }
        graph_objects = {
            entry["xml_tag"]: entry
            for entry in self.model["objects"]
            if "xml_tag" in entry
        }

        self.assertEqual(formats[".spark"]["role"], "authored")
        self.assertEqual(formats[".spk"]["role"], "generated")
        self.assertEqual(formats[".efkproj"]["role"], "authored")
        self.assertEqual(formats[".efk"]["role"], "generated")
        self.assertIn(
            "never check it into an authored resource source",
            formats[".efk"]["requirement"],
        )
        self.assertIn("System", graph_objects)
        self.assertIn("SparkQuadRenderer", graph_objects)

    def test_runtime_contract_is_backend_neutral_and_seedable(self) -> None:
        runtime = {entry["id"]: entry for entry in self.model["runtime_rules"]}

        self.assertIn(
            "single feature-aware composition point",
            runtime["particle-format.runtime.backend-composition"]["rationale"],
        )
        self.assertIn(
            "explicit seed",
            runtime["particle-format.runtime.seed"]["requirement"],
        )
        self.assertIn(
            "reported elapsed time",
            runtime["particle-format.runtime.prewarm"]["rationale"],
        )

    def test_tooling_contract_separates_preview_and_authoring(self) -> None:
        tooling = {entry["id"]: entry for entry in self.model["tooling_rules"]}

        self.assertIn(
            "baked .spk and .efk",
            tooling["particle-format.tooling.mapper-preview"]["requirement"],
        )
        self.assertIn(
            "raw .spark assets",
            tooling["particle-format.tooling.spark-editor"]["requirement"],
        )
        self.assertIn(
            "build-auxiliary",
            tooling["particle-format.tooling.effekseer-editor"]["requirement"],
        )

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in docs_particle_format.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_both_backends_and_project_boundaries(self) -> None:
        guide = (ENGINE_ROOT / "Docs/ParticleFormat.md").read_text(encoding="utf-8")

        for heading in (
            "## Build-time selection",
            "## Resource pipeline",
            "## SPARK authoring",
            "## Effekseer authoring",
            "## Mapper workflow",
            "## Runtime contract",
            "## Integration",
            "## Production practices",
            "## Validation",
        ):
            self.assertIn(heading, guide)
        self.assertIn("`FO_SPARK_PARTICLES`", guide)
        self.assertIn("`FO_EFFEKSEER_PARTICLES`", guide)
        self.assertIn("`.spark` -> `.spk`", guide)
        self.assertIn("`.efkproj` -> `.efk`", guide)
        self.assertNotIn("There is no `ParticleBaker`", guide)

    def test_changed_derived_manifest_values_are_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_particle_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        manifest["outputs"] = copy.deepcopy(manifest["outputs"])
        manifest["outputs"]["runtime_extensions"] = ["spk"]

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "ParticleFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, r"outputs.runtime_extensions must be \['spk', 'efk'\]"
            ):
                docs_particle_format.generate_particle_format_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_particle_format.generate_reference_pages(self.model)

        self.assertEqual(set(pages), set(docs_particle_format.OUTPUT_PATHS))
        self.assertIn(
            "Authored extensions", pages["Docs/generated/particle-format/index.md"]
        )
        self.assertIn(
            "Effekseer project XML", pages["Docs/generated/particle-format/xml.md"]
        )
        self.assertIn(
            ".efkproj", pages["Docs/generated/particle-format/objects.md"]
        )
        self.assertIn(
            "particle-format.renderer.effekseer-direct-scene",
            pages["Docs/generated/particle-format/renderer.md"],
        )
        self.assertIn(
            "Standalone Effekseer Editor",
            pages["Docs/generated/particle-format/tooling.md"],
        )
        self.assertIn(
            "Seeded respawn", pages["Docs/generated/particle-format/runtime.md"]
        )
        self.assertIn(
            "AttachParticles",
            pages["Docs/generated/particle-format/integration.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_particle_format.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
