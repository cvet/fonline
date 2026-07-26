from __future__ import annotations

import hashlib
import re
import unittest
from html.parser import HTMLParser
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class LandmarkParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.tags: list[tuple[str, dict[str, str | None]]] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self.tags.append((tag, dict(attrs)))


class DocumentationSiteLayoutTests(unittest.TestCase):
    def test_layout_has_accessible_documentation_landmarks_and_manifest_data(self) -> None:
        layout = (ROOT / "_layouts/default.html").read_text(encoding="utf-8")
        parser = LandmarkParser()
        parser.feed(layout)

        tags = [tag for tag, _ in parser.tags]
        self.assertIn("header", tags)
        self.assertIn("main", tags)
        self.assertIn("article", tags)
        self.assertIn("nav", tags)
        self.assertIn("dialog", tags)
        self.assertIn("site.data.docs-site.navigation", layout)
        self.assertIn("site.data.docs-site.canonical_locale", layout)
        self.assertIn("site.data.docs-site.version.channel", layout)
        self.assertIn("site.data.docs-site.version.label", layout)
        self.assertIn("site.data.docs-site.version.value", layout)
        self.assertIn("site.data.docs-site.source_ref", layout)
        self.assertIn("aria-current=\"page\"", layout)
        self.assertIn("href=\"#main-content\"", layout)

    def test_layout_uses_local_assets_and_generated_search_endpoint(self) -> None:
        layout = (ROOT / "_layouts/default.html").read_text(encoding="utf-8")

        self.assertIn("/assets/css/docs.css", layout)
        self.assertIn("/assets/js/docs.js", layout)
        self.assertIn("/assets/docs-search.json", layout)
        self.assertIn("/assets/images/fonline-mark.png", layout)
        self.assertNotRegex(layout, r"<(?:script|link)[^>]+(?:src|href)=\"https?://")

    def test_styles_and_script_cover_responsive_navigation_search_and_theme(self) -> None:
        stylesheet = (ROOT / "assets/css/docs.css").read_text(encoding="utf-8")
        script = (ROOT / "assets/js/docs.js").read_text(encoding="utf-8")

        for marker in ("@media (max-width: 900px)", "body.nav-open", ".search-dialog", ".page-toc"):
            self.assertIn(marker, stylesheet)
        self.assertNotRegex(stylesheet, r"letter-spacing:\s*-|font-size:\s*[^;]*(?:vw|cqw)|gradient\(")
        for marker in (
            "data-nav-toggle",
            "data-search-dialog",
            "data-page-toc",
            "fonline-docs-theme",
            "navigator.clipboard.writeText",
        ):
            self.assertIn(marker, script)

    def test_engine_owned_mark_is_published_byte_for_byte(self) -> None:
        source = (ROOT / "Resources/Radiation.png").read_bytes()
        published = (ROOT / "assets/images/fonline-mark.png").read_bytes()

        self.assertEqual(hashlib.sha256(published).digest(), hashlib.sha256(source).digest())
        self.assertGreater(len(published), 1024)

    def test_jekyll_applies_the_custom_layout_to_pages(self) -> None:
        config = (ROOT / "_config.yml").read_text(encoding="utf-8")

        self.assertRegex(config, r"(?ms)^defaults:\s*\n\s*- scope:.*?type:\s*pages.*?layout:\s*default")
        self.assertIn("theme: jekyll-theme-slate", config)


if __name__ == "__main__":
    unittest.main()
