from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_localization  # noqa: E402


class LocalizationDocumentationTests(unittest.TestCase):
    def _fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        (root / "Docs").mkdir()
        (root / "Docs/Guide.md").write_text(
            "# Guide\n\nRun:\n\n```bash\ncmake --build Build\n```\n",
            encoding="utf-8",
        )
        glossary = {
            "schema_version": 1,
            "source_locale": "en",
            "target_locale": "ru",
            "terms": [
                {
                    "term": "build",
                    "russian": "сборка",
                    "policy": "translate",
                    "note": "Fixture term.",
                }
            ],
        }
        (root / docs_localization.DEFAULT_GLOSSARY).write_text(
            json.dumps(glossary, ensure_ascii=False),
            encoding="utf-8",
        )
        manifest = {
            "localization": {
                "schema_version": 1,
                "canonical_locale": "en",
                "path_strategy": "mirrored-relative-path",
                "translation_hash": "normalized-sha256",
                "translation_pending": "pre-production-only",
                "glossary": docs_localization.DEFAULT_GLOSSARY,
                "status_output": docs_localization.DEFAULT_OUTPUT,
                "enforcement": "existing-translations-current",
                "entrypoint_targets": {},
            },
            "documents": {
                "Docs/Guide.md": {
                    "id": "guide",
                    "title": "Guide",
                    "owner": "documentation",
                    "state": "current",
                    "target": "Docs/en/how-to/guide.md",
                    "classification": {
                        "diataxis": "how-to",
                        "visibility": "public",
                        "human": True,
                        "translation": "required",
                    },
                }
            },
        }
        (root / docs_localization.DEFAULT_MANIFEST).write_text(
            json.dumps(manifest),
            encoding="utf-8",
        )
        return temporary, root

    def test_repository_status_is_deterministic_and_honest(self) -> None:
        first = docs_localization.generate_localization_status(ENGINE_ROOT)
        second = docs_localization.generate_localization_status(ENGINE_ROOT)
        self.assertEqual(first, second)
        self.assertGreater(first["summary"]["required_document_count"], 100)
        self.assertEqual(first["summary"]["required_document_count"], 197)
        self.assertEqual(first["summary"]["current_translation_count"], 197)
        self.assertEqual(
            first["summary"]["missing_translation_count"],
            0,
        )
        self.assertTrue(first["summary"]["complete"])
        current = {
            str(document["id"]): document
            for document in first["documents"]
            if document["status"] == "current"
        }
        self.assertEqual(
            set(current),
            {
                "adr-documentation-version-locale-routing",
                "adr-github-pages-markdown-publication",
                "adr-manifest-backed-ai-documentation-delivery",
                "adr-manifest-backed-site-navigation-search",
                "adr-public-api-stability-contract",
                "adr-public-example-repository-ownership",
                "ai-control-protocol-guide",
                "ai-control-sample-readme",
                "ai-documentation-evaluation",
                "applications-entry-points",
                "api-change-management",
                "android-debugging",
                "angelscript-style",
                "audio-guide",
                "backup-and-recovery",
                "baking-pipeline",
                "build-workflow",
                "buildtools-pipeline",
                "buildtools-readme",
                "client-runtime",
                "client-updater",
                "configuration-data-sources",
                "documentation-snippet-validation",
                "documentation-maintenance",
                "documentation-home",
                "documentation-site-publication",
                "documentation-translation-workflow",
                "debugging",
                "engine-architecture",
                "engine-upgrade-guide",
                "entity-model",
                "effect-format-guide",
                "font-format-guide",
                "frontend-rendering",
                "map-format-guide",
                "model-format-guide",
                "model-animation",
                "sprite-root-motion",
                "image-format-guide",
                "particle-format-guide",
                "public-example-repositories",
                "particle-authoring-tools",
                "embedding-project",
                "exception-safety",
                "getting-started",
                "generated-content-workflow",
                "generated-api-events",
                "generated-api-index",
                "generated-api-metadata",
                "generated-api-methods",
                "generated-api-migrations",
                "generated-api-properties",
                "generated-api-settings",
                "generated-api-types",
                "generated-cli-commands",
                "generated-cli-index",
                "generated-cmake-helpers",
                "generated-cmake-index",
                "generated-cmake-options",
                "generated-cmake-stages",
                "generated-helper-cli-commands",
                "generated-helper-cli-index",
                "generated-audio-decoding",
                "generated-audio-delivery",
                "generated-audio-formats",
                "generated-audio-index",
                "generated-audio-playback",
                "generated-audio-validation",
                "generated-ai-control-protocol-commands-events",
                "generated-ai-control-protocol-index",
                "generated-ai-control-protocol-integration-validation",
                "generated-ai-control-protocol-methods",
                "generated-ai-control-protocol-security",
                "generated-ai-control-protocol-wire",
                "generated-gui-runtime-index",
                "generated-gui-runtime-input",
                "generated-gui-runtime-integration-validation",
                "generated-gui-runtime-layout-rendering",
                "generated-gui-runtime-lifecycle",
                "generated-gui-runtime-screen-api",
                "generated-gui-runtime-types",
                "generated-effect-format-baking",
                "generated-effect-format-index",
                "generated-effect-format-render-state",
                "generated-effect-format-resources",
                "generated-effect-format-runtime",
                "generated-effect-format-syntax",
                "generated-effect-format-validation",
                "generated-font-format-binding",
                "generated-font-format-bmfont",
                "generated-font-format-fofnt",
                "generated-font-format-formats",
                "generated-font-format-index",
                "generated-font-format-layout",
                "generated-font-format-rendering",
                "generated-font-format-validation",
                "generated-image-format-baking",
                "generated-image-format-fofrm",
                "generated-image-format-formats",
                "generated-image-format-index",
                "generated-image-format-options",
                "generated-image-format-runtime",
                "generated-image-format-validation",
                "generated-map-format-baking",
                "generated-map-format-index",
                "generated-map-format-properties",
                "generated-map-format-syntax",
                "generated-map-format-validation",
                "generated-package-cli",
                "generated-package-declaration",
                "generated-package-index",
                "generated-package-matrix",
                "generated-package-payloads",
                "generated-model-format-animation",
                "generated-model-format-assets",
                "generated-model-format-composition",
                "generated-model-format-index",
                "generated-model-format-syntax",
                "generated-model-format-tokens",
                "generated-model-format-validation",
                "generated-particle-format-index",
                "generated-particle-format-xml",
                "generated-particle-format-objects",
                "generated-particle-format-renderer",
                "generated-particle-format-tooling",
                "generated-particle-format-runtime",
                "generated-particle-format-integration",
                "generated-particle-format-validation",
                "generated-public-examples-index",
                "generated-prototype-format-index",
                "generated-prototype-format-properties",
                "generated-prototype-format-syntax",
                "generated-prototype-format-validation",
                "generated-native-extension-bindings",
                "generated-native-extension-hooks",
                "generated-native-extension-index",
                "generated-native-extension-roles",
                "generated-text-format-index",
                "generated-text-format-languages",
                "generated-text-format-proto-text",
                "generated-text-format-runtime",
                "generated-text-format-syntax",
                "generated-text-format-validation",
                "generated-video-decoding",
                "generated-video-delivery",
                "generated-video-embedded",
                "generated-video-formats",
                "generated-video-fullscreen",
                "generated-video-index",
                "generated-video-validation",
                "generated-support-matrix-index",
                "gameplay-testing",
                "gui-runtime-guide",
                "first-client-tutorial",
                "first-content-tutorial",
                "first-test-tutorial",
                "legacy-tutorial-entry",
                "legacy-public-api-entry",
                "local-variables",
                "mapper-interactive-manual",
                "mapper-tools",
                "maps-movement-geometry",
                "content-showcase-readme",
                "minimal-project-readme",
                "minimal-multiplayer-readme",
                "native-extension-sample-readme",
                "native-extensions-guide",
                "native-essentials",
                "networking",
                "nullability",
                "persistence",
                "packaging-and-release",
                "repository-home",
                "remote-calls",
                "script-lifecycle-concurrency",
                "script-methods-map",
                "scripting-runtime",
                "server-runtime",
                "smart-pointers",
                "support-matrix",
                "source-readme",
                "source-tree",
                "testing",
                "thread-safety-analysis",
                "text-and-localization-guide",
                "tools",
                "unit-tests-readme",
                "video-guide",
                "viewer-tools",
                "web-debugging",
                "profiling",
                "project-configuration",
                "project-local-dependencies",
                "prototype-format-guide",
                "release-operations",
                "security-and-secrets",
                "third-party-maintenance",
            },
        )
        for document in current.values():
            self.assertEqual(document["source_path"], document["english_path"])
            self.assertTrue((ENGINE_ROOT / document["russian_path"]).is_file())

    def test_current_translation_requires_exact_hash_and_code(self) -> None:
        temporary, root = self._fixture()
        self.addCleanup(temporary.cleanup)
        source = (root / "Docs/Guide.md").read_text(encoding="utf-8")
        source_hash = docs_localization.normalized_sha256(source)
        russian = root / "Docs/ru/how-to/guide.md"
        russian.parent.mkdir(parents=True)
        russian.write_text(
            docs_localization.translation_metadata_line(
                "guide", "Docs/Guide.md", source_hash
            )
            + "\n\n# Руководство\n\nЗапустите:\n\n"
            + "```bash\ncmake --build Build\n```\n",
            encoding="utf-8",
        )
        model = docs_localization.generate_localization_status(root)
        self.assertEqual(model["summary"]["current_translation_count"], 1)
        self.assertTrue(model["summary"]["complete"])

        (root / "Docs/Guide.md").write_text(source + "\nChanged.\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "metadata is stale"):
            docs_localization.generate_localization_status(root)

    def test_translation_cannot_change_fenced_code(self) -> None:
        temporary, root = self._fixture()
        self.addCleanup(temporary.cleanup)
        source = (root / "Docs/Guide.md").read_text(encoding="utf-8")
        russian = root / "Docs/ru/how-to/guide.md"
        russian.parent.mkdir(parents=True)
        russian.write_text(
            docs_localization.translation_metadata_line(
                "guide",
                "Docs/Guide.md",
                docs_localization.normalized_sha256(source),
            )
            + "\n\n# Руководство\n\n```bash\ncmake --build Other\n```\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "fenced code blocks"):
            docs_localization.generate_localization_status(root)

    def test_translation_cannot_change_indented_fenced_code(self) -> None:
        temporary, root = self._fixture()
        self.addCleanup(temporary.cleanup)
        source = "# Guide\n\n1. Run:\n\n   ```bash\n   cmake --build Build\n   ```\n"
        (root / "Docs/Guide.md").write_text(source, encoding="utf-8")
        russian = root / "Docs/ru/how-to/guide.md"
        russian.parent.mkdir(parents=True)
        russian.write_text(
            docs_localization.translation_metadata_line(
                "guide",
                "Docs/Guide.md",
                docs_localization.normalized_sha256(source),
            )
            + "\n\n# Руководство\n\n1. Запустите:\n\n"
            + "   ```bash\n   cmake --build Other\n   ```\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "fenced code blocks"):
            docs_localization.generate_localization_status(root)

    def test_complete_gate_rejects_missing_pair(self) -> None:
        temporary, root = self._fixture()
        self.addCleanup(temporary.cleanup)
        with self.assertRaisesRegex(ValueError, "coverage is incomplete"):
            docs_localization.generate_localization_status(
                root, enforce_complete=True
            )

    def test_glossary_rejects_duplicate_terms(self) -> None:
        temporary, root = self._fixture()
        self.addCleanup(temporary.cleanup)
        glossary_path = root / docs_localization.DEFAULT_GLOSSARY
        glossary = json.loads(glossary_path.read_text(encoding="utf-8"))
        glossary["terms"].append(dict(glossary["terms"][0]))
        glossary_path.write_text(
            json.dumps(glossary, ensure_ascii=False),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "duplicate translation glossary term"):
            docs_localization.generate_localization_status(root)

    def test_outputs_ci_and_manifest_are_current(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        manifest = json.loads(
            (ENGINE_ROOT / docs_localization.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        self.assertIn("BuildTools/tests/test_docs_localization.py", workflow)
        self.assertIn("BuildTools/docs_localization.py --check", workflow)
        self.assertEqual(
            manifest["localization"]["glossary"],
            docs_localization.DEFAULT_GLOSSARY,
        )
        self.assertEqual(
            manifest["localization"]["status_output"],
            docs_localization.DEFAULT_OUTPUT,
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_localization.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)


if __name__ == "__main__":
    unittest.main()
