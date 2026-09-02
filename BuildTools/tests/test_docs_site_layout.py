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
        self.assertIn("site.data.docs-site.locale_pairs", layout)
        self.assertIn("aria-current=\"page\"", layout)
        self.assertIn("Primary documentation", layout)
        self.assertIn("Основная документация", layout)
        self.assertIn("Documentation language", layout)
        self.assertIn("Язык документации", layout)
        self.assertIn("href=\"#main-content\"", layout)

    def test_layout_uses_local_assets_and_generated_search_endpoint(self) -> None:
        layout = (ROOT / "_layouts/default.html").read_text(encoding="utf-8")

        self.assertIn("/assets/css/docs.css", layout)
        self.assertIn("/assets/js/docs.js", layout)
        self.assertIn("/assets/docs-search.json", layout)
        self.assertIn("/assets/docs-search.ru.json", layout)
        self.assertIn("/assets/images/fonline-mark.png", layout)
        self.assertNotRegex(layout, r"<(?:script|link)[^>]+(?:src|href)=\"https?://")

    def test_styles_and_script_cover_responsive_navigation_search_and_theme(self) -> None:
        stylesheet = (ROOT / "assets/css/docs.css").read_text(encoding="utf-8")
        script = (ROOT / "assets/js/docs.js").read_text(encoding="utf-8")

        for marker in (
            "@media (max-width: 900px)",
            "body.nav-open",
            ".search-dialog",
            ".page-toc",
            ".locale-switch",
        ):
            self.assertIn(marker, stylesheet)
        self.assertNotRegex(stylesheet, r"letter-spacing:\s*-|font-size:\s*[^;]*(?:vw|cqw)|gradient\(")
        for marker in (
            "data-nav-toggle",
            "data-search-dialog",
            "data-page-toc",
            "fonline-docs-theme",
            "navigator.clipboard.writeText",
            "navigationSidebar.inert = hidden",
            "firstSidebarControl.focus()",
            "focusables[nextIndex].focus()",
            "updateScrollableTables",
            "tokenScores",
            "effectiveTokenCount * 0.6",
            'pageLocale = document.documentElement.lang === "ru"',
            "searchDialog.dataset.searchUrlRu",
            "\\p{L}\\p{N}",
            "Поиск временно недоступен",
        ):
            self.assertIn(marker, script)

    def test_search_reads_the_term_index_only_through_the_own_property_guard(self) -> None:
        # The search index is parsed from JSON, so its object inherits Object.prototype. A raw
        # terms[token] lookup answers a query of "__proto__" with Object.prototype itself, which
        # is truthy and has no forEach, and the whole search UI reports itself unavailable
        script = (ROOT / "assets/js/docs.js").read_text(encoding="utf-8")

        self.assertIn(
            "Object.prototype.hasOwnProperty.call(terms, token) ? terms[token] : null",
            script,
        )
        unguarded = re.findall(r"(?<!Object\.keys\()index\.terms\[", script)
        self.assertEqual([], unguarded)

    def test_migrated_locale_pages_declare_stable_rendering_contract(self) -> None:
        for relative_path, locale, document_id, permalink in (
            (
                "README.md",
                "en",
                "repository-home",
                "/",
            ),
            (
                "README.ru.md",
                "ru",
                "repository-home",
                "/README.ru.html",
            ),
            (
                "BuildTools/README.md",
                "en",
                "buildtools-readme",
                "/BuildTools/README.html",
            ),
            (
                "BuildTools/README.ru.md",
                "ru",
                "buildtools-readme",
                "/BuildTools/README.ru.html",
            ),
            (
                "Docs/en/how-to/build/index.md",
                "en",
                "build-workflow",
                "/Docs/en/how-to/build/",
            ),
            (
                "Docs/ru/how-to/build/index.md",
                "ru",
                "build-workflow",
                "/Docs/ru/how-to/build/",
            ),
            (
                "Docs/en/how-to/build/embedding-project.md",
                "en",
                "embedding-project",
                "/Docs/en/how-to/build/embedding-project.html",
            ),
            (
                "Docs/ru/how-to/build/embedding-project.md",
                "ru",
                "embedding-project",
                "/Docs/ru/how-to/build/embedding-project.html",
            ),
            (
                "Docs/en/how-to/build/generated-content.md",
                "en",
                "generated-content-workflow",
                "/Docs/en/how-to/build/generated-content.html",
            ),
            (
                "Docs/ru/how-to/build/generated-content.md",
                "ru",
                "generated-content-workflow",
                "/Docs/ru/how-to/build/generated-content.html",
            ),
            (
                "Docs/en/how-to/build/project-configuration.md",
                "en",
                "project-configuration",
                "/Docs/en/how-to/build/project-configuration.html",
            ),
            (
                "Docs/ru/how-to/build/project-configuration.md",
                "ru",
                "project-configuration",
                "/Docs/ru/how-to/build/project-configuration.html",
            ),
            (
                "Docs/en/reference/cmake-and-buildtools/pipeline.md",
                "en",
                "buildtools-pipeline",
                "/Docs/en/reference/cmake-and-buildtools/pipeline.html",
            ),
            (
                "Docs/ru/reference/cmake-and-buildtools/pipeline.md",
                "ru",
                "buildtools-pipeline",
                "/Docs/ru/reference/cmake-and-buildtools/pipeline.html",
            ),
            (
                "Docs/en/how-to/migration/engine-upgrade.md",
                "en",
                "engine-upgrade-guide",
                "/Docs/en/how-to/migration/engine-upgrade.html",
            ),
            (
                "Docs/ru/how-to/migration/engine-upgrade.md",
                "ru",
                "engine-upgrade-guide",
                "/Docs/ru/how-to/migration/engine-upgrade.html",
            ),
            (
                "Docs/en/reference/platforms/support-matrix.md",
                "en",
                "support-matrix",
                "/Docs/en/reference/platforms/support-matrix.html",
            ),
            (
                "Docs/ru/reference/platforms/support-matrix.md",
                "ru",
                "support-matrix",
                "/Docs/ru/reference/platforms/support-matrix.html",
            ),
            (
                "Docs/en/how-to/release/packaging.md",
                "en",
                "packaging-and-release",
                "/Docs/en/how-to/release/packaging.html",
            ),
            (
                "Docs/ru/how-to/release/packaging.md",
                "ru",
                "packaging-and-release",
                "/Docs/ru/how-to/release/packaging.html",
            ),
            (
                "Docs/en/how-to/release/operations.md",
                "en",
                "release-operations",
                "/Docs/en/how-to/release/operations.html",
            ),
            (
                "Docs/ru/how-to/release/operations.md",
                "ru",
                "release-operations",
                "/Docs/ru/how-to/release/operations.html",
            ),
            (
                "Docs/en/how-to/release/backup-and-recovery.md",
                "en",
                "backup-and-recovery",
                "/Docs/en/how-to/release/backup-and-recovery.html",
            ),
            (
                "Docs/ru/how-to/release/backup-and-recovery.md",
                "ru",
                "backup-and-recovery",
                "/Docs/ru/how-to/release/backup-and-recovery.html",
            ),
            (
                "Docs/en/how-to/release/security-and-secrets.md",
                "en",
                "security-and-secrets",
                "/Docs/en/how-to/release/security-and-secrets.html",
            ),
            (
                "Docs/ru/how-to/release/security-and-secrets.md",
                "ru",
                "security-and-secrets",
                "/Docs/ru/how-to/release/security-and-secrets.html",
            ),
            (
                "Docs/en/contributing/documentation/index.md",
                "en",
                "documentation-maintenance",
                "/Docs/en/contributing/documentation/",
            ),
            (
                "Docs/ru/contributing/documentation/index.md",
                "ru",
                "documentation-maintenance",
                "/Docs/ru/contributing/documentation/",
            ),
            (
                "Docs/en/contributing/documentation/site-publication.md",
                "en",
                "documentation-site-publication",
                "/Docs/en/contributing/documentation/site-publication.html",
            ),
            (
                "Docs/ru/contributing/documentation/site-publication.md",
                "ru",
                "documentation-site-publication",
                "/Docs/ru/contributing/documentation/site-publication.html",
            ),
            (
                "Docs/en/contributing/documentation/translation.md",
                "en",
                "documentation-translation-workflow",
                "/Docs/en/contributing/documentation/translation.html",
            ),
            (
                "Docs/ru/contributing/documentation/translation.md",
                "ru",
                "documentation-translation-workflow",
                "/Docs/ru/contributing/documentation/translation.html",
            ),
            (
                "Docs/en/contributing/documentation/snippets.md",
                "en",
                "documentation-snippet-validation",
                "/Docs/en/contributing/documentation/snippets.html",
            ),
            (
                "Docs/ru/contributing/documentation/snippets.md",
                "ru",
                "documentation-snippet-validation",
                "/Docs/ru/contributing/documentation/snippets.html",
            ),
            (
                "Docs/en/contributing/documentation/ai-evaluation.md",
                "en",
                "ai-documentation-evaluation",
                "/Docs/en/contributing/documentation/ai-evaluation.html",
            ),
            (
                "Docs/ru/contributing/documentation/ai-evaluation.md",
                "ru",
                "ai-documentation-evaluation",
                "/Docs/ru/contributing/documentation/ai-evaluation.html",
            ),
            (
                "Docs/en/tutorials/getting-started.md",
                "en",
                "getting-started",
                "/Docs/en/tutorials/getting-started.html",
            ),
            (
                "Docs/en/tutorials/first-client.md",
                "en",
                "first-client-tutorial",
                "/Docs/en/tutorials/first-client.html",
            ),
            (
                "Docs/en/tutorials/first-project.md",
                "en",
                "legacy-tutorial-entry",
                "/Docs/en/tutorials/first-project.html",
            ),
            (
                "Docs/en/tutorials/first-content.md",
                "en",
                "first-content-tutorial",
                "/Docs/en/tutorials/first-content.html",
            ),
            (
                "Docs/en/tutorials/first-test.md",
                "en",
                "first-test-tutorial",
                "/Docs/en/tutorials/first-test.html",
            ),
            (
                "Docs/ru/tutorials/getting-started.md",
                "ru",
                "getting-started",
                "/Docs/ru/tutorials/getting-started.html",
            ),
            (
                "Docs/ru/tutorials/first-client.md",
                "ru",
                "first-client-tutorial",
                "/Docs/ru/tutorials/first-client.html",
            ),
            (
                "Docs/ru/tutorials/first-project.md",
                "ru",
                "legacy-tutorial-entry",
                "/Docs/ru/tutorials/first-project.html",
            ),
            (
                "Docs/ru/tutorials/first-content.md",
                "ru",
                "first-content-tutorial",
                "/Docs/ru/tutorials/first-content.html",
            ),
            (
                "Docs/ru/tutorials/first-test.md",
                "ru",
                "first-test-tutorial",
                "/Docs/ru/tutorials/first-test.html",
            ),
            (
                "Docs/en/explanation/architecture/index.md",
                "en",
                "engine-architecture",
                "/Docs/en/explanation/architecture/",
            ),
            (
                "Docs/ru/explanation/architecture/index.md",
                "ru",
                "engine-architecture",
                "/Docs/ru/explanation/architecture/",
            ),
            (
                "Docs/en/explanation/entity-and-property-model/index.md",
                "en",
                "entity-model",
                "/Docs/en/explanation/entity-and-property-model/",
            ),
            (
                "Docs/ru/explanation/entity-and-property-model/index.md",
                "ru",
                "entity-model",
                "/Docs/ru/explanation/entity-and-property-model/",
            ),
            (
                "Docs/en/explanation/runtime/client.md",
                "en",
                "client-runtime",
                "/Docs/en/explanation/runtime/client.html",
            ),
            (
                "Docs/ru/explanation/runtime/client.md",
                "ru",
                "client-runtime",
                "/Docs/ru/explanation/runtime/client.html",
            ),
            (
                "Docs/en/explanation/runtime/server.md",
                "en",
                "server-runtime",
                "/Docs/en/explanation/runtime/server.html",
            ),
            (
                "Docs/ru/explanation/runtime/server.md",
                "ru",
                "server-runtime",
                "/Docs/ru/explanation/runtime/server.html",
            ),
            (
                "Docs/en/explanation/maps-and-movement.md",
                "en",
                "maps-movement-geometry",
                "/Docs/en/explanation/maps-and-movement.html",
            ),
            (
                "Docs/ru/explanation/maps-and-movement.md",
                "ru",
                "maps-movement-geometry",
                "/Docs/ru/explanation/maps-and-movement.html",
            ),
            (
                "Docs/en/explanation/authority-and-networking/index.md",
                "en",
                "networking",
                "/Docs/en/explanation/authority-and-networking/",
            ),
            (
                "Docs/ru/explanation/authority-and-networking/index.md",
                "ru",
                "networking",
                "/Docs/ru/explanation/authority-and-networking/",
            ),
            (
                "Docs/en/explanation/persistence/index.md",
                "en",
                "persistence",
                "/Docs/en/explanation/persistence/",
            ),
            (
                "Docs/ru/explanation/persistence/index.md",
                "ru",
                "persistence",
                "/Docs/ru/explanation/persistence/",
            ),
            (
                "Docs/en/explanation/scripting-runtime/index.md",
                "en",
                "scripting-runtime",
                "/Docs/en/explanation/scripting-runtime/",
            ),
            (
                "Docs/ru/explanation/scripting-runtime/index.md",
                "ru",
                "scripting-runtime",
                "/Docs/ru/explanation/scripting-runtime/",
            ),
            (
                "Docs/en/how-to/scripting/lifecycle-and-concurrency.md",
                "en",
                "script-lifecycle-concurrency",
                "/Docs/en/how-to/scripting/lifecycle-and-concurrency.html",
            ),
            (
                "Docs/ru/how-to/scripting/lifecycle-and-concurrency.md",
                "ru",
                "script-lifecycle-concurrency",
                "/Docs/ru/how-to/scripting/lifecycle-and-concurrency.html",
            ),
            (
                "Docs/en/reference/scripting/remote-calls.md",
                "en",
                "remote-calls",
                "/Docs/en/reference/scripting/remote-calls.html",
            ),
            (
                "Docs/ru/reference/scripting/remote-calls.md",
                "ru",
                "remote-calls",
                "/Docs/ru/reference/scripting/remote-calls.html",
            ),
            (
                "Docs/en/reference/script-api/method-ownership.md",
                "en",
                "script-methods-map",
                "/Docs/en/reference/script-api/method-ownership.html",
            ),
            (
                "Docs/ru/reference/script-api/method-ownership.md",
                "ru",
                "script-methods-map",
                "/Docs/ru/reference/script-api/method-ownership.html",
            ),
            (
                "Docs/en/contributing/coding-contracts/exception-safety.md",
                "en",
                "exception-safety",
                "/Docs/en/contributing/coding-contracts/exception-safety.html",
            ),
            (
                "Docs/ru/contributing/coding-contracts/exception-safety.md",
                "ru",
                "exception-safety",
                "/Docs/ru/contributing/coding-contracts/exception-safety.html",
            ),
            (
                "Docs/en/contributing/coding-contracts/local-variables.md",
                "en",
                "local-variables",
                "/Docs/en/contributing/coding-contracts/local-variables.html",
            ),
            (
                "Docs/ru/contributing/coding-contracts/local-variables.md",
                "ru",
                "local-variables",
                "/Docs/ru/contributing/coding-contracts/local-variables.html",
            ),
            (
                "Docs/en/contributing/coding-contracts/nullability.md",
                "en",
                "nullability",
                "/Docs/en/contributing/coding-contracts/nullability.html",
            ),
            (
                "Docs/ru/contributing/coding-contracts/nullability.md",
                "ru",
                "nullability",
                "/Docs/ru/contributing/coding-contracts/nullability.html",
            ),
            (
                "Docs/en/contributing/coding-contracts/smart-pointers.md",
                "en",
                "smart-pointers",
                "/Docs/en/contributing/coding-contracts/smart-pointers.html",
            ),
            (
                "Docs/ru/contributing/coding-contracts/smart-pointers.md",
                "ru",
                "smart-pointers",
                "/Docs/ru/contributing/coding-contracts/smart-pointers.html",
            ),
            (
                "Docs/en/contributing/coding-contracts/thread-safety-analysis.md",
                "en",
                "thread-safety-analysis",
                "/Docs/en/contributing/coding-contracts/thread-safety-analysis.html",
            ),
            (
                "Docs/ru/contributing/coding-contracts/thread-safety-analysis.md",
                "ru",
                "thread-safety-analysis",
                "/Docs/ru/contributing/coding-contracts/thread-safety-analysis.html",
            ),
            (
                "Docs/en/contributing/testing/index.md",
                "en",
                "testing",
                "/Docs/en/contributing/testing/",
            ),
            (
                "Docs/ru/contributing/testing/index.md",
                "ru",
                "testing",
                "/Docs/ru/contributing/testing/",
            ),
            (
                "Docs/en/how-to/testing/gameplay-and-integration.md",
                "en",
                "gameplay-testing",
                "/Docs/en/how-to/testing/gameplay-and-integration.html",
            ),
            (
                "Docs/ru/how-to/testing/gameplay-and-integration.md",
                "ru",
                "gameplay-testing",
                "/Docs/ru/how-to/testing/gameplay-and-integration.html",
            ),
            (
                "Docs/en/how-to/quality/profiling.md",
                "en",
                "profiling",
                "/Docs/en/how-to/quality/profiling.html",
            ),
            (
                "Docs/ru/how-to/quality/profiling.md",
                "ru",
                "profiling",
                "/Docs/ru/how-to/quality/profiling.html",
            ),
            (
                "Docs/en/contributing/source-tree/index.md",
                "en",
                "source-tree",
                "/Docs/en/contributing/source-tree/",
            ),
            (
                "Docs/ru/contributing/source-tree/index.md",
                "ru",
                "source-tree",
                "/Docs/ru/contributing/source-tree/",
            ),
            (
                "Docs/en/reference/applications.md",
                "en",
                "applications-entry-points",
                "/Docs/en/reference/applications.html",
            ),
            (
                "Docs/ru/reference/applications.md",
                "ru",
                "applications-entry-points",
                "/Docs/ru/reference/applications.html",
            ),
            (
                "Examples/MinimalProject/README.md",
                "en",
                "minimal-project-readme",
                "/Examples/MinimalProject/README.html",
            ),
            (
                "Examples/MinimalProject/README.ru.md",
                "ru",
                "minimal-project-readme",
                "/Examples/MinimalProject/README.ru.html",
            ),
            (
                "Examples/MinimalMultiplayer/README.md",
                "en",
                "minimal-multiplayer-readme",
                "/Examples/MinimalMultiplayer/README.html",
            ),
            (
                "Examples/MinimalMultiplayer/README.ru.md",
                "ru",
                "minimal-multiplayer-readme",
                "/Examples/MinimalMultiplayer/README.ru.html",
            ),
            (
                "Examples/AiControlSample/README.md",
                "en",
                "ai-control-sample-readme",
                "/Examples/AiControlSample/README.html",
            ),
            (
                "Examples/AiControlSample/README.ru.md",
                "ru",
                "ai-control-sample-readme",
                "/Examples/AiControlSample/README.ru.html",
            ),
            (
                "Source/README.md",
                "en",
                "source-readme",
                "/Source/README.html",
            ),
            (
                "Source/README.ru.md",
                "ru",
                "source-readme",
                "/Source/README.ru.html",
            ),
            (
                "Source/Tests/README.md",
                "en",
                "unit-tests-readme",
                "/Source/Tests/README.html",
            ),
            (
                "Source/Tests/README.ru.md",
                "ru",
                "unit-tests-readme",
                "/Source/Tests/README.ru.html",
            ),
        ):
            source = (ROOT / relative_path).read_text(encoding="utf-8")
            front_matter = source.split("---", 2)[1]
            self.assertRegex(front_matter, rf"(?m)^locale:\s*{locale}\s*$")
            self.assertRegex(front_matter, rf"(?m)^document_id:\s*{document_id}\s*$")
            self.assertRegex(
                front_matter,
                rf"(?m)^permalink:\s*{re.escape(permalink)}\s*$",
            )

    def test_engine_owned_mark_is_published_byte_for_byte(self) -> None:
        source = (ROOT / "Resources/Radiation.png").read_bytes()
        published = (ROOT / "assets/images/fonline-mark.png").read_bytes()

        self.assertEqual(hashlib.sha256(published).digest(), hashlib.sha256(source).digest())
        self.assertGreater(len(published), 1024)

    def test_buildtools_entrypoint_uses_aggregate_current_docs_validation(self) -> None:
        readme = (ROOT / "BuildTools/README.md").read_text(encoding="utf-8")

        self.assertIn(
            'python -m unittest discover -s BuildTools/tests -p "test_docs*.py"',
            readme,
        )
        for command in (
            "python BuildTools/docs_ai_control_protocol.py --check",
            "python BuildTools/docs_inventory.py --check",
            "python BuildTools/docs_localization.py --check",
            "python BuildTools/docs_validate.py",
        ):
            self.assertIn(command, readme)
        self.assertNotIn("export FO_ENGINE_ROOT=/mnt/d/fonline-workspace", readme)
        self.assertNotIn("Docs/Plans/2026-06-27-client-av-heuristics-audit.md", readme)

    def test_jekyll_applies_the_custom_layout_to_pages(self) -> None:
        config = (ROOT / "_config.yml").read_text(encoding="utf-8")

        self.assertRegex(config, r"(?ms)^defaults:\s*\n\s*- scope:.*?type:\s*pages.*?layout:\s*default")
        self.assertIn("theme: jekyll-theme-slate", config)
        for example in (
            "ContentShowcase",
            "MinimalMultiplayer",
            "MinimalProject",
            "NativeExtensionSample",
        ):
            self.assertIn(f"Examples/{example}/Build/", config)
            self.assertIn(f"Examples/{example}/Engine/", config)
        self.assertIn("Examples/ContentShowcase/WebTests/node_modules/", config)
        for source_readme, permalink in (
            ("BuildTools/README.md", "/BuildTools/README.html"),
            ("Docs/README.md", "/Docs/README.html"),
            ("Docs/en/index.md", "/Docs/en/"),
            ("Docs/ru/index.md", "/Docs/ru/"),
            ("Examples/MinimalProject/README.md", "/Examples/MinimalProject/README.html"),
            (
                "Examples/MinimalMultiplayer/README.md",
                "/Examples/MinimalMultiplayer/README.html",
            ),
            ("Source/README.md", "/Source/README.html"),
            ("Source/Tests/README.md", "/Source/Tests/README.html"),
        ):
            readme = (ROOT / source_readme).read_text(encoding="utf-8-sig")
            self.assertRegex(
                readme,
                rf"(?ms)\A---\s*\n.*?^permalink:\s*{re.escape(permalink)}\s*$.*?^---\s*$",
            )


if __name__ == "__main__":
    unittest.main()
