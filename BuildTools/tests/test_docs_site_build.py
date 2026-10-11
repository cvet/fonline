from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_site_build  # noqa: E402
import documentation_metadata  # noqa: E402


class DocumentationSiteBuildTests(unittest.TestCase):
    def test_build_uses_workspace_dependencies_and_exports_public_root_endpoints(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            destination = docs_site_build.site_directory(root, documentation_metadata.SITE_OUTPUT_DIR)
            prepared = root / documentation_metadata.BUILD_OUTPUT_DIR
            prepared.mkdir(parents=True)
            for name in documentation_metadata.ROOT_ENDPOINTS:
                (prepared / name).write_text(f"current {name}\n", encoding="utf-8")
            (root / "llms.txt").write_text("obsolete root copy\n", encoding="utf-8")

            def render(command, **kwargs):
                self.assertEqual(command[0], str(root / "Ruby Tools/bundle.bat"))
                self.assertEqual(command[command.index("--config") + 1], str(root / documentation_metadata.SITE_CONFIG))
                self.assertEqual(command[command.index("--source") + 1], str(root))
                self.assertEqual(kwargs["cwd"], prepared)
                self.assertEqual(kwargs["env"]["BUNDLE_GEMFILE"], str(prepared / "Gemfile"))
                self.assertEqual(kwargs["env"]["JEKYLL_ENV"], "production")
                destination.mkdir(parents=True)
                (destination / "index.html").write_text("rendered page\n", encoding="utf-8")
                (destination / "llms.txt").write_text("obsolete rendered copy\n", encoding="utf-8")

            with patch.object(docs_site_build.docs_prepare, "prepare") as prepare, \
                 patch.object(docs_site_build.shutil, "which", return_value=str(root / "Ruby Tools/bundle.bat")), \
                 patch.object(docs_site_build.subprocess, "run", side_effect=render):
                docs_site_build.build(root, destination)
            prepare.assert_called_once_with(root, external=False)
            for name in documentation_metadata.ROOT_ENDPOINTS:
                self.assertEqual((destination / name).read_bytes(), (prepared / name).read_bytes())
            self.assertEqual((destination / "index.html").read_text(encoding="utf-8"), "rendered page\n")
            self.assertEqual((root / "llms.txt").read_text(encoding="utf-8"), "obsolete root copy\n")

    def test_failed_render_does_not_export_or_claim_a_successful_site(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            with patch.object(docs_site_build.docs_prepare, "prepare"), \
                 patch.object(docs_site_build.subprocess, "run", side_effect=subprocess.CalledProcessError(7, "bundle")), \
                 patch.object(docs_site_build, "export_endpoints") as export:
                self.assertEqual(docs_site_build.main(["--root", str(root)]), 1)
            export.assert_not_called()

    def test_output_must_stay_below_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            for target in (".", "Docs", "Workspace", "Workspace/../../other"):
                with self.subTest(target=target), self.assertRaises(ValueError):
                    docs_site_build.site_directory(root, target)


if __name__ == "__main__":
    unittest.main()
