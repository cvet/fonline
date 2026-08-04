from __future__ import annotations

import contextlib
import copy
import hashlib
import io
import json
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ElementTree
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_diagrams  # noqa: E402


class DocumentationDiagramTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = docs_diagrams.load_diagrams(ENGINE_ROOT)
        cls.outputs = docs_diagrams.render_outputs(ENGINE_ROOT)

    def test_manifest_has_three_owned_teaching_diagrams(self) -> None:
        self.assertEqual(
            tuple(diagram["id"] for diagram in self.manifest["diagrams"]),
            docs_diagrams.DIAGRAM_IDS,
        )
        self.assertEqual(
            {diagram["owning_document"] for diagram in self.manifest["diagrams"]},
            {
                "Docs/en/explanation/architecture/index.md",
                "Docs/en/how-to/build/generated-content.md",
                "Docs/en/contributing/documentation/site-publication.md",
            },
        )
        for diagram in self.manifest["diagrams"]:
            self.assertGreaterEqual(len(diagram["source_paths"]), 2)
            self.assertGreaterEqual(len(diagram["alt"]), 80)
            self.assertGreaterEqual(len(diagram["caption"]), 80)

    def test_svg_outputs_are_accessible_local_and_safe(self) -> None:
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        for path in docs_diagrams.OUTPUT_PATHS[1:]:
            svg = self.outputs[path]
            root = ElementTree.fromstring(svg)
            self.assertEqual(root.attrib["role"], "img")
            labelled_by = root.attrib["aria-labelledby"].split()
            self.assertEqual(len(labelled_by), 2)
            title = root.find("svg:title", namespace)
            description = root.find("svg:desc", namespace)
            self.assertIsNotNone(title)
            self.assertIsNotNone(description)
            self.assertEqual(title.attrib["id"], labelled_by[0])
            self.assertEqual(description.attrib["id"], labelled_by[1])
            self.assertNotIn("<script", svg.casefold())
            self.assertNotIn("<foreignObject", svg)
            self.assertNotIn("http://", svg.replace("http://www.w3.org/2000/svg", ""))
            self.assertNotIn("https://", svg)
            self.assertNotIn("href=", svg)

    def test_catalog_hashes_match_exact_svg_bytes(self) -> None:
        catalog = json.loads(self.outputs[docs_diagrams.DEFAULT_CATALOG])
        self.assertEqual(catalog["diagram_count"], len(docs_diagrams.DIAGRAM_IDS))
        for record in catalog["diagrams"]:
            self.assertEqual(
                [variant["id"] for variant in record["variants"]],
                ["desktop", "mobile"],
            )
            for variant in record["variants"]:
                svg = self.outputs[variant["path"]].encode("utf-8")
                self.assertEqual(
                    hashlib.sha256(svg).hexdigest(), variant["sha256"]
                )
                self.assertGreater(variant["width"], 0)
                self.assertGreater(variant["height"], 0)

    def test_palette_text_contrast_exceeds_wcag_normal_text_ratio(self) -> None:
        def luminance(color: str) -> float:
            channels = [
                int(color[index : index + 2], 16) / 255
                for index in (1, 3, 5)
            ]
            linear = [
                channel / 12.92
                if channel <= 0.04045
                else ((channel + 0.055) / 1.055) ** 2.4
                for channel in channels
            ]
            return (
                linear[0] * 0.2126
                + linear[1] * 0.7152
                + linear[2] * 0.0722
            )

        def contrast(first: str, second: str) -> float:
            first_luminance = luminance(first)
            second_luminance = luminance(second)
            lighter = max(first_luminance, second_luminance)
            darker = min(first_luminance, second_luminance)
            return (lighter + 0.05) / (darker + 0.05)

        for fill, _ in docs_diagrams.ROLE_PALETTE.values():
            self.assertGreaterEqual(contrast("#35433b", fill), 4.5)

    def test_nodes_do_not_overlap_and_stay_inside_canvas(self) -> None:
        for diagram in self.manifest["diagrams"]:
            for index, node in enumerate(diagram["nodes"]):
                self.assertGreaterEqual(node["x"], 0)
                self.assertGreaterEqual(node["y"], 0)
                self.assertLessEqual(
                    node["x"] + node["width"], diagram["width"]
                )
                self.assertLessEqual(
                    node["y"] + node["height"], diagram["height"]
                )
                for other in diagram["nodes"][index + 1 :]:
                    self.assertFalse(docs_diagrams._boxes_overlap(node, other))

    def test_invalid_source_and_overlap_are_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_diagrams.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        missing_source = copy.deepcopy(manifest)
        missing_source["diagrams"][0]["source_paths"][0] = "Source/Missing.cpp"
        overlapping = copy.deepcopy(manifest)
        overlapping["diagrams"][0]["nodes"][1]["x"] = overlapping["diagrams"][0][
            "nodes"
        ][0]["x"]
        overlapping["diagrams"][0]["nodes"][1]["y"] = overlapping["diagrams"][0][
            "nodes"
        ][0]["y"]
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            for name, candidate, message in (
                ("missing.json", missing_source, "does not exist"),
                ("overlap.json", overlapping, "nodes overlap"),
            ):
                path = root / name
                path.write_text(json.dumps(candidate), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    docs_diagrams.load_diagrams(
                        ENGINE_ROOT,
                        str(path),
                        validate_embeddings=False,
                    )

    def test_outputs_are_current_and_ci_routes_generator(self) -> None:
        self.assertEqual(tuple(self.outputs), docs_diagrams.OUTPUT_PATHS)
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_diagrams.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("BuildTools/tests/test_docs_diagrams.py", workflow)
        self.assertIn("BuildTools/docs_diagrams.py --check", workflow)


if __name__ == "__main__":
    unittest.main()
