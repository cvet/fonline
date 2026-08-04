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

import docs_support_matrix  # noqa: E402
import docs_localization  # noqa: E402


class SupportMatrixDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_support_matrix.generate_support_matrix(ENGINE_ROOT)

    def test_model_is_deterministic_and_uses_live_validation_targets(self) -> None:
        self.assertEqual(
            self.model,
            docs_support_matrix.generate_support_matrix(ENGINE_ROOT),
        )
        self.assertEqual(self.model["summary"]["platform_profile_count"], 10)
        self.assertEqual(self.model["summary"]["build_gated_profile_count"], 8)
        self.assertEqual(self.model["summary"]["smoke_gated_profile_count"], 3)
        self.assertIn(
            "linux-tutorial-smoke",
            {
                target
                for platform in self.model["platforms"]
                for target in platform["ci_validation_targets"]
            },
        )

    def test_build_and_smoke_claims_are_present_in_required_workflow(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        for platform in self.model["platforms"]:
            for target in platform["ci_validation_targets"]:
                self.assertIn(target, workflow)
            if platform["level"] == "smoke_gated":
                self.assertTrue(
                    any(
                        target.endswith(("-starter-smoke", "-native-extension-smoke", "-tutorial-smoke", "-showcase-smoke", "-showcase-runtime"))
                        for target in platform["ci_validation_targets"]
                    )
                )

    def test_source_capable_profile_cannot_claim_a_ci_target(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_support_matrix.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        manifest = copy.deepcopy(manifest)
        entry = next(
            platform
            for platform in manifest["platforms"]
            if platform["id"] == "android-x86-client"
        )
        entry["ci_validation_targets"] = ["android-x86-client"]

        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "SupportMatrix.json"
            path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "source_capable entry must not declare CI targets"
            ):
                docs_support_matrix.generate_support_matrix(
                    ENGINE_ROOT, str(path)
                )

    def test_human_guide_preserves_evidence_boundaries(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/reference/platforms/support-matrix.md").read_text(encoding="utf-8")
        for heading in (
            "## Support vocabulary",
            "## Current qualified baseline",
            "## Application boundaries",
            "## Renderer boundaries",
            "## Project release matrix",
            "## Adding or changing a profile",
            "## Maintenance",
        ):
            self.assertIn(heading, guide)
        self.assertIn("compile-time capability, not visual qualification", guide)
        self.assertIn("Headless smoke tests deliberately prove no pixels", guide)
        self.assertIn("When Engine or an embedding project is updated", guide)

    def test_outputs_are_current(self) -> None:
        outputs = docs_support_matrix.render_outputs(ENGINE_ROOT)
        self.assertEqual(
            set(outputs),
            {
                docs_support_matrix.DEFAULT_MODEL,
                docs_support_matrix.DEFAULT_INDEX,
                docs_support_matrix.RUSSIAN_INDEX,
                docs_support_matrix.LEGACY_INDEX,
            },
        )
        self.assertIn("Generated Support Matrix", outputs[docs_support_matrix.DEFAULT_INDEX])
        english = outputs[docs_support_matrix.DEFAULT_INDEX]
        russian = outputs[docs_support_matrix.RUSSIAN_INDEX]
        self.assertIn(
            docs_localization.translation_metadata_line(
                "generated-support-matrix-index",
                docs_support_matrix.DEFAULT_INDEX,
                docs_localization.normalized_sha256(english),
            ),
            russian,
        )
        self.assertEqual(
            re.findall(r"^#{2,3} ", english, re.MULTILINE),
            re.findall(r"^#{2,3} ", russian, re.MULTILINE),
        )
        self.assertIn(
            "../../en/reference/platforms/generated-matrix.md",
            outputs[docs_support_matrix.LEGACY_INDEX],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_support_matrix.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)

    def test_ci_and_manifest_route_the_matrix(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        manifest = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertIn("BuildTools/tests/test_docs_support_matrix.py", workflow)
        self.assertIn("BuildTools/docs_support_matrix.py --check", workflow)
        ids = {document["id"] for document in manifest["documents"].values()}
        self.assertIn("support-matrix", ids)
        self.assertIn("generated-support-matrix-index", ids)
        canonical = manifest["documents"][docs_support_matrix.DEFAULT_INDEX]
        legacy = manifest["documents"][docs_support_matrix.LEGACY_INDEX]
        self.assertEqual(canonical["state"], "current")
        self.assertEqual(canonical["disposition"], "retain")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], canonical["id"])


if __name__ == "__main__":
    unittest.main()
