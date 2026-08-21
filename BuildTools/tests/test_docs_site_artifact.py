from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_site_artifact  # noqa: E402


class DocumentationSiteArtifactTests(unittest.TestCase):
    def _page(
        self,
        canonical: str,
        *,
        body: str,
        locale: str = "en",
    ) -> str:
        return (
            "<!doctype html>\n"
            f'<html lang="{locale}">\n'
            "<head>\n"
            '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            f"<title>{canonical}</title>\n"
            f'<link rel="canonical" href="{canonical}">\n'
            '<link rel="stylesheet" href="/assets/css/docs.css">\n'
            "</head>\n"
            "<body>\n"
            '<a class="skip-link" href="#main-content">Skip</a>\n'
            '<button type="button" aria-label="Search"></button>\n'
            '<img src="/assets/images/fonline-mark.png" alt="">\n'
            '<main id="main-content"><h1 id="heading">Heading</h1>\n'
            f"{body}</main>\n"
            '<script src="/assets/js/docs.js"></script>\n'
            "</body>\n"
            "</html>\n"
        )

    def _create_fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary_directory = tempfile.TemporaryDirectory()
        root = Path(temporary_directory.name)
        site = root / "_site"

        manifest = {
            "ai_delivery": {
                "llms": {"path": "llms.txt"},
                "full_context": {"path": "llms-full.txt"},
                "public_manifest": {"path": "docs-manifest.json"},
            },
            "site_delivery": {
                "search": {
                    "path": "assets/docs-search.json",
                    "locale_paths": {
                        "en": "assets/docs-search.json",
                        "ru": "assets/docs-search.ru.json",
                    },
                },
                "routing": {"path": "Docs/generated/document-routes.json"},
            },
            "generated_artifacts": {
                "support_matrix": {"model": "Docs/generated/support-matrix.json"},
                "localization_status": {"path": "Docs/generated/translation-status.json"},
            },
        }
        routes = {
            "canonical_base_url": "https://fonline.ru",
            "routes": [
                {
                    "id": "home",
                    "current_path": "/",
                    "current_url": "https://fonline.ru/",
                },
                {
                    "id": "guide",
                    "current_path": "/Docs/Guide.html",
                    "current_url": "https://fonline.ru/Docs/Guide.html",
                },
            ],
        }
        search = {
            "locale": "en",
            "documents": [
                {"id": "home", "locale": "en", "url": "/"},
                {"id": "guide", "locale": "en", "url": "/Docs/Guide.html"},
            ]
        }
        russian_search = {"locale": "ru", "documents": []}
        source_files: dict[str, str] = {
            "CNAME": "fonline.ru\n",
            "llms.txt": "# Docs\n",
            "llms-full.txt": "# Full docs\n",
            "docs-manifest.json": json.dumps({"documents": []}) + "\n",
            "assets/docs-search.json": json.dumps(search) + "\n",
            "assets/docs-search.ru.json": json.dumps(russian_search) + "\n",
            "assets/css/docs.css": "body { color: black; }\n",
            "assets/js/docs.js": "'use strict';\n",
            "assets/images/fonline-mark.png": "image\n",
            "Docs/generated/document-routes.json": json.dumps(routes) + "\n",
            "Docs/generated/support-matrix.json": json.dumps({"profiles": []}) + "\n",
            "Docs/generated/translation-status.json": json.dumps({"documents": []}) + "\n",
        }
        for relative_path, content in source_files.items():
            source_path = root / relative_path
            rendered_path = site / relative_path
            source_path.parent.mkdir(parents=True, exist_ok=True)
            rendered_path.parent.mkdir(parents=True, exist_ok=True)
            source_path.write_text(content, encoding="utf-8")
            rendered_path.write_text(content, encoding="utf-8")

        manifest_path = root / docs_site_artifact.DEFAULT_MANIFEST
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(json.dumps(manifest) + "\n", encoding="utf-8")
        (site / "index.html").write_text(
            self._page(
                "https://fonline.ru/",
                body='<a href="/Docs/Guide.html#heading">Guide</a>',
            ),
            encoding="utf-8",
        )
        (site / "Docs/Guide.html").write_text(
            self._page(
                "https://fonline.ru/Docs/Guide.html",
                body='<a href="/">Home</a>',
            ),
            encoding="utf-8",
        )
        return temporary_directory, root

    def test_valid_rendered_artifact_passes(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)

        report = docs_site_artifact.audit_site(root, root / "_site")

        self.assertEqual(report["error_count"], 0)
        self.assertEqual(report["route_count"], 2)
        self.assertEqual(report["rendered_route_count"], 2)
        self.assertEqual(report["static_endpoint_count"], 12)
        self.assertGreater(report["checked_local_reference_count"], 0)

    def test_missing_route_and_static_endpoint_fail(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        (root / "_site/Docs/Guide.html").unlink()
        (root / "_site/llms.txt").unlink()

        report = docs_site_artifact.audit_site(root, root / "_site")

        self.assertIn(
            "rendered route is missing: guide -> /Docs/Guide.html",
            report["errors"],
        )
        self.assertIn("rendered endpoint is missing: /llms.txt", report["errors"])
        self.assertTrue(
            any(
                "rendered en search document target is missing: guide" in error
                for error in report["errors"]
            )
        )

    def test_internal_documents_and_generated_artifacts_must_not_be_published(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest_path = root / docs_site_artifact.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["documents"] = {
            "Docs/Internal.md": {
                "classification": {"visibility": "internal"},
            }
        }
        manifest["generated_artifacts"]["internal_model"] = {
            "visibility": "internal",
            "model": "Docs/generated/internal.json",
            "paths": ["Docs/generated/internal/index.md"],
        }
        manifest_path.write_text(json.dumps(manifest) + "\n", encoding="utf-8")

        for relative_path in (
            "Docs/Internal.md",
            "Docs/generated/internal.json",
            "Docs/generated/internal/index.md",
        ):
            for base in (root, root / "_site"):
                path = base / relative_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("internal\n", encoding="utf-8")

        report = docs_site_artifact.audit_site(root, root / "_site")

        self.assertEqual(report["internal_path_count"], 3)
        self.assertEqual(report["published_internal_path_count"], 3)
        self.assertIn(
            "internal documentation artifact is published: /Docs/Internal.md",
            report["errors"],
        )
        self.assertNotIn("Docs/generated/internal.json", docs_site_artifact._expected_static_paths(manifest))

    def test_accessibility_and_canonical_failures_are_reported(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        broken = (
            "<html><head><title></title></head><body>"
            '<main id="main-content"><h1 id="same">One</h1><h1 id="same">Two</h1>'
            '<img src="/assets/images/fonline-mark.png"><button></button></main>'
            "</body></html>"
        )
        (root / "_site/Docs/Guide.html").write_text(broken, encoding="utf-8")

        report = docs_site_artifact.audit_site(root, root / "_site")
        joined = "\n".join(report["errors"])

        self.assertIn("guide: rendered page must start with an HTML5 doctype", joined)
        self.assertIn("guide: rendered html lang must be en", joined)
        self.assertIn("guide: rendered page must contain exactly one h1", joined)
        self.assertIn("guide: duplicate HTML ids: same", joined)
        self.assertIn("guide: 1 image(s) lack alt attributes", joined)
        self.assertIn("guide: 1 button(s) lack accessible names", joined)
        self.assertIn("guide: canonical URL must be", joined)

    def test_broken_published_link_and_fragment_fail(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        (root / "_site/index.html").write_text(
            self._page(
                "https://fonline.ru/",
                body=(
                    '<a href="/Docs/Missing.html">Missing</a>'
                    '<a href="/Docs/Guide.html#missing">Missing anchor</a>'
                ),
            ),
            encoding="utf-8",
        )

        report = docs_site_artifact.audit_site(root, root / "_site")

        self.assertIn(
            "home: local published target is missing: /Docs/Missing.html",
            report["errors"],
        )
        self.assertIn(
            "home: local fragment is missing: /Docs/Guide.html#missing",
            report["errors"],
        )

    def test_available_locale_route_is_rendered_with_matching_language(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        route_paths = (
            root / "Docs/generated/document-routes.json",
            root / "_site/Docs/generated/document-routes.json",
        )
        routes = json.loads(route_paths[0].read_text(encoding="utf-8"))
        routes["routes"][1]["locale_routes"] = [
            {
                "locale": "ru",
                "path": "/Docs/ru/Guide.html",
                "url": "https://fonline.ru/Docs/ru/Guide.html",
                "availability": "available",
            }
        ]
        rendered_routes = json.dumps(routes) + "\n"
        for route_path in route_paths:
            route_path.write_text(rendered_routes, encoding="utf-8")
        russian_page = root / "_site/Docs/ru/Guide.html"
        russian_page.parent.mkdir(parents=True)
        russian_page.write_text(
            self._page(
                "https://fonline.ru/Docs/ru/Guide.html",
                body='<a href="/Docs/Guide.html">English</a>',
                locale="ru",
            ),
            encoding="utf-8",
        )
        russian_search = {
            "locale": "ru",
            "documents": [
                {
                    "id": "guide",
                    "locale": "ru",
                    "url": "/Docs/ru/Guide.html",
                }
            ],
        }
        rendered_search = json.dumps(russian_search) + "\n"
        for search_path in (
            root / "assets/docs-search.ru.json",
            root / "_site/assets/docs-search.ru.json",
        ):
            search_path.write_text(rendered_search, encoding="utf-8")

        report = docs_site_artifact.audit_site(root, root / "_site")

        self.assertEqual(report["error_count"], 0)
        self.assertEqual(report["rendered_route_count"], 3)

    def test_cli_writes_report_on_validation_failure(self) -> None:
        temporary_directory, root = self._create_fixture()
        self.addCleanup(temporary_directory.cleanup)
        (root / "_site/index.html").unlink()

        result = docs_site_artifact.main(
            [
                "--root",
                str(root),
                "--site-dir",
                "_site",
                "--json-output",
                "Workspace/report.json",
            ]
        )

        self.assertEqual(result, 1)
        report = json.loads((root / "Workspace/report.json").read_text(encoding="utf-8"))
        self.assertGreater(report["error_count"], 0)


if __name__ == "__main__":
    unittest.main()
