from __future__ import annotations

import base64
import hashlib
import json
import posixpath
import shutil
import sys
import tempfile
import unittest
from pathlib import Path, PurePosixPath


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_validate  # noqa: E402
import docs_api  # noqa: E402
import docs_api_diff  # noqa: E402
import docs_ai_control_protocol  # noqa: E402
import docs_ai_delivery  # noqa: E402
import docs_ai_eval  # noqa: E402
import docs_audio  # noqa: E402
import docs_cli  # noqa: E402
import docs_cmake  # noqa: E402
import docs_contract_diff  # noqa: E402
import docs_description_translations  # noqa: E402
import docs_diagrams  # noqa: E402
import docs_screenshots  # noqa: E402
import docs_effect_format  # noqa: E402
import docs_examples  # noqa: E402
import docs_font_format  # noqa: E402
import docs_gui_runtime  # noqa: E402
import docs_helper_cli  # noqa: E402
import docs_image_format  # noqa: E402
import docs_inventory  # noqa: E402
import docs_localization  # noqa: E402
import docs_map_format  # noqa: E402
import docs_model_format  # noqa: E402
import docs_native_extension  # noqa: E402
import docs_package  # noqa: E402
import docs_particle_format  # noqa: E402
import docs_prototype_format  # noqa: E402
import docs_reference  # noqa: E402
import docs_site  # noqa: E402
import docs_snippets  # noqa: E402
import docs_support_matrix  # noqa: E402
import docs_text_format  # noqa: E402
import docs_video  # noqa: E402


class DocumentationValidatorTests(unittest.TestCase):
    def _create_tree(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary_directory = tempfile.TemporaryDirectory()
        root = Path(temporary_directory.name)
        (root / "Docs").mkdir()
        (root / "BuildTools").mkdir()
        (root / docs_description_translations.DEFAULT_CATALOG).write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "source_locale": "en",
                    "target_locale": "ru",
                    "enforcement": "registered-translations-current",
                    "domains": {},
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        (root / docs_snippets.DEFAULT_POLICY).write_text(
            (BUILDTOOLS_DIR.parent / docs_snippets.DEFAULT_POLICY).read_text(
                encoding="utf-8"
            ),
            encoding="utf-8",
        )
        (root / "Source/Scripting").mkdir(parents=True)
        (root / "Source/Tests").mkdir(parents=True)
        (root / "Source/Common").mkdir(parents=True)
        (root / "Source/example.txt").write_text("source\nfixture-anchor\n", encoding="utf-8")
        (root / "Source/Common/Settings.inc").write_text(
            'FIXED_SETTING(vector<string>, Baking, ProtoFileExtensions, "fopro");\n',
            encoding="utf-8",
        )
        (root / "CNAME").write_text("fonline.ru\n", encoding="utf-8")
        (root / ".github/workflows").mkdir(parents=True)
        (root / ".ruby-version").write_text("3.3.4\n", encoding="utf-8")
        (root / "Gemfile").write_text(
            'source "https://rubygems.org"\n\ngem "github-pages", "= 232", group: :jekyll_plugins\n',
            encoding="utf-8",
        )
        (root / "_config.yml").write_text(
            "url: https://fonline.ru\n"
            'baseurl: ""\n'
            "repository: cvet/fonline\n"
            "theme: jekyll-theme-slate\n"
            "strict_front_matter: true\n"
            "plugins:\n"
            "  - jekyll-relative-links\n"
            "defaults:\n"
            "  - scope:\n"
            "      path: \"\"\n"
            "      type: pages\n"
            "    values:\n"
            "      layout: default\n",
            encoding="utf-8",
        )
        fixture_assets = {
            "_layouts/default.html": "<!doctype html><main>{{ content }}</main>\n",
            "assets/css/docs.css": "body { color: black; }\n",
            "assets/js/docs.js": "'use strict';\n",
            "assets/images/fonline-mark.png": "fixture image\n",
        }
        for relative_path, content in fixture_assets.items():
            path = root / relative_path
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
        (root / ".github/workflows/validate.yml").write_text(
            "jobs:\n"
            "  documentation:\n"
            "    steps:\n"
            "    - uses: actions/checkout@v6\n"
            "      with:\n"
            "        fetch-depth: 0\n"
            "    - run: python3 BuildTools/tests/test_docs_api_diff.py\n"
            "    - run: python3 BuildTools/tests/test_docs_contract_diff.py\n"
            "    - run: python3 BuildTools/tests/test_docs_ai_delivery.py\n"
            "    - run: python3 BuildTools/tests/test_docs_ai_eval.py\n"
            "    - run: python3 BuildTools/tests/test_docs_ai_model_eval.py\n"
            "    - run: python3 BuildTools/tests/test_docs_ai_model_review.py\n"
            "    - run: python3 BuildTools/tests/test_docs_snippets.py\n"
            "    - run: python3 BuildTools/tests/test_docs_site.py\n"
            "    - run: python3 BuildTools/tests/test_docs_site_layout.py\n"
            "    - run: python3 BuildTools/tests/test_docs_site_artifact.py\n"
            "    - run: python3 BuildTools/tests/test_docs_browser.py\n"
            "    - run: python3 BuildTools/docs_contract_diff.py --baseline-git-ref HEAD^ "
            "--dispositions Docs/contract-change-dispositions.json --write --enforce\n"
            "    - run: python3 BuildTools/tests/test_docs_cmake.py\n"
            "    - run: python3 BuildTools/tests/test_docs_cli.py\n"
            "    - run: python3 BuildTools/tests/test_docs_helper_cli.py\n"
            "    - run: python3 BuildTools/tests/test_docs_native_extension.py\n"
            "    - run: python3 BuildTools/tests/test_docs_prototype_format.py\n"
            "    - run: python3 BuildTools/tests/test_docs_map_format.py\n"
            "    - run: python3 BuildTools/tests/test_docs_model_format.py\n"
            "    - run: python3 BuildTools/tests/test_docs_text_format.py\n"
            "    - run: python3 BuildTools/tests/test_docs_effect_format.py\n"
            "    - run: python3 BuildTools/tests/test_docs_image_format.py\n"
            "    - run: python3 BuildTools/tests/test_docs_particle_format.py\n"
            "    - run: python3 BuildTools/tests/test_docs_font_format.py\n"
            "    - run: python3 BuildTools/tests/test_docs_audio.py\n"
            "    - run: python3 BuildTools/tests/test_docs_video.py\n"
            "    - run: python3 BuildTools/tests/test_docs_gui_runtime.py\n"
            "    - run: python3 BuildTools/tests/test_ai_control_protocol.py\n"
            "    - run: python3 BuildTools/tests/test_docs_ai_control_protocol.py\n"
            "    - run: python3 BuildTools/tests/test_docs_package.py\n"
            "    - run: python3 BuildTools/tests/test_docs_examples.py\n"
            "    - run: python3 BuildTools/tests/test_docs_support_matrix.py\n"
            "    - run: python3 BuildTools/tests/test_docs_diagrams.py\n"
            "    - run: python3 BuildTools/tests/test_docs_screenshots.py\n"
            "    - run: python3 BuildTools/tests/test_docs_localization.py\n"
            "    - run: python3 BuildTools/tests/test_docs_description_translations.py\n"
            "    - run: python3 BuildTools/tests/test_docs_script_lifecycle.py\n"
            "    - run: python3 BuildTools/tests/test_docs_model_animation.py\n"
            "    - run: python3 BuildTools/tests/test_docs_sprite_root_motion.py\n"
            "    - run: python3 BuildTools/tests/test_docs_web_debugging.py\n"
            "    - run: python3 BuildTools/tests/test_docs_android_debugging.py\n"
            "    - run: python3 BuildTools/tests/test_docs_debugging.py\n"
            "    - run: python3 BuildTools/tests/test_docs_angelscript_style.py\n"
            "    - run: python3 BuildTools/tests/test_docs_viewer_tools.py\n"
            "    - run: python3 BuildTools/tests/test_docs_mapper_tools.py\n"
            "    - run: cmake -P BuildTools/tests/validate_project_interface.cmake\n"
            "    - run: cmake -P BuildTools/tests/validate_package_interface.cmake\n"
            "    - run: cmake -P BuildTools/tests/validate_native_extension_interface.cmake\n"
            "    - run: python3 BuildTools/docs_cmake.py --check\n"
            "    - run: python3 BuildTools/docs_cli.py --check\n"
            "    - run: python3 BuildTools/docs_helper_cli.py --check\n"
            "    - run: python3 BuildTools/docs_native_extension.py --check\n"
            "    - run: python3 BuildTools/docs_prototype_format.py --check\n"
            "    - run: python3 BuildTools/docs_map_format.py --check\n"
            "    - run: python3 BuildTools/docs_model_format.py --check\n"
            "    - run: python3 BuildTools/docs_text_format.py --check\n"
            "    - run: python3 BuildTools/docs_effect_format.py --check\n"
            "    - run: python3 BuildTools/docs_image_format.py --check\n"
            "    - run: python3 BuildTools/docs_particle_format.py --check\n"
            "    - run: python3 BuildTools/docs_font_format.py --check\n"
            "    - run: python3 BuildTools/docs_audio.py --check\n"
            "    - run: python3 BuildTools/docs_video.py --check\n"
            "    - run: python3 BuildTools/docs_gui_runtime.py --check\n"
            "    - run: python3 BuildTools/docs_ai_control_protocol.py --check\n"
            "    - run: python3 BuildTools/docs_package.py --check\n"
            "    - run: python3 BuildTools/docs_examples.py --check\n"
            "    - run: python3 BuildTools/docs_support_matrix.py --check\n"
            "    - run: python3 BuildTools/docs_diagrams.py --check\n"
            "    - run: python3 BuildTools/docs_screenshots.py --check\n"
            "    - run: python3 BuildTools/docs_localization.py --check\n"
            "    - run: python3 BuildTools/docs_description_translations.py --check\n"
            "    - run: python3 BuildTools/docs_snippets.py --check\n"
            "    - app: linux-client\n"
            "    - run: python3 BuildTools/docs_ai_delivery.py --check\n"
            "    - run: python3 BuildTools/docs_site.py --check\n"
            "    - run: python3 BuildTools/docs_ai_eval.py --check\n"
            "  documentation-snippets:\n"
            "    steps:\n"
            "    - run: python3 BuildTools/docs_snippets.py --check --external\n"
            "  documentation-site:\n"
            "    steps:\n"
            "    - uses: actions/jekyll-build-pages@v1\n"
            "      with:\n"
            "        destination: ./_site\n"
            "    - run: python3 BuildTools/docs_site_artifact.py --site-dir _site "
            "--json-output Workspace/docs-site-artifact-report.json\n"
            "    - run: npx playwright install --with-deps chromium\n"
            "    - run: npm run audit\n"
            "    - uses: actions/upload-artifact@v4\n"
            "      with:\n"
            "        path: _site/\n",
            encoding="utf-8",
        )
        return temporary_directory, root

    def _write_manifest(self, root: Path, documents: dict[str, dict[str, object]]) -> None:
        start_document_ids = [
            str(document["id"])
            for document in documents.values()
            if document.get("state") == "current"
            and isinstance(document.get("classification"), dict)
            and document["classification"].get("visibility") == "public"
        ][:1]
        site_document_ids = [
            str(document["id"])
            for path, document in documents.items()
            if document.get("state") == "current"
            and isinstance(document.get("classification"), dict)
            and document["classification"].get("visibility") == "public"
            and document["classification"].get("human") is True
            and not (path.startswith("Docs/generated/") and not path.endswith("/index.md"))
        ]
        manifest = {
            "schema_version": 1,
            "inventory": {
                "include": ["Docs/**/*.md"],
                "exclude": ["Docs/generated/api/*.md", "Docs/en/reference/script-api/*.md", "Docs/ru/reference/script-api/*.md", "Docs/en/reference/public-contract/*.md", "Docs/ru/reference/public-contract/*.md", "Docs/generated/cmake/*.md", "Docs/generated/cli/*.md", "Docs/generated/helper-cli/*.md", "Docs/en/reference/cmake/*.md", "Docs/en/reference/buildtools/*.md", "Docs/en/reference/helper-cli/*.md", "Docs/generated/native-extension/*.md", "Docs/en/reference/native-extension/*.md", "Docs/generated/prototype-format/*.md", "Docs/en/reference/prototype-format/*.md", "Docs/generated/map-format/*.md", "Docs/en/reference/map-format/*.md", "Docs/generated/model-format/*.md", "Docs/en/reference/model-format/*.md", "Docs/generated/text-format/*.md", "Docs/en/reference/text-format/*.md", "Docs/generated/effect-format/*.md", "Docs/en/reference/effect-format/*.md", "Docs/generated/image-format/*.md", "Docs/en/reference/image-format/*.md", "Docs/generated/particle-format/*.md", "Docs/en/reference/particle-format/*.md", "Docs/generated/font-format/*.md", "Docs/en/reference/font-format/*.md", "Docs/generated/audio/*.md", "Docs/generated/video/*.md", "Docs/en/reference/audio/*.md", "Docs/en/reference/video/*.md", "Docs/generated/gui-runtime/*.md", "Docs/en/reference/gui-runtime/*.md", "Docs/generated/ai-control-protocol/*.md", "Docs/en/reference/ai-control-protocol/*.md", "Docs/generated/package/*.md", "Docs/en/reference/packages/*.md", "Docs/ru/reference/packages/*.md", "Docs/generated/public-examples/*.md", "Docs/en/reference/public-examples/*.md", "Docs/ru/reference/public-examples/*.md", "Docs/generated/support-matrix/*.md", "Docs/en/reference/platforms/generated-matrix.md", "Docs/ru/reference/platforms/generated-matrix.md", "Docs/en/tutorials/first-project.md"],
            },
            "publishing": {
                "title": "FOnline Engine",
                "site_description": "Standalone documentation for building games with FOnline.",
                "provider": "github-pages",
                "generator": "jekyll",
                "content_format": "markdown",
                "domain": "fonline.ru",
                "production_url": "https://fonline.ru",
                "repository": "cvet/fonline",
                "theme": "jekyll-theme-slate",
                "cname": "CNAME",
                "config": "_config.yml",
                "gemfile": "Gemfile",
                "ruby_version_file": ".ruby-version",
                "ruby_version": "3.3.4",
                "pages_gem_version": "232",
                "workflow": ".github/workflows/validate.yml",
                "site_artifact": "_site",
                "source": {
                    "status": "pending-admin-verification",
                    "branch": None,
                    "folder": None,
                },
                "dns": {
                    "status": "public-resolution-confirmed",
                    "ownership_verification": "not-observed",
                    "challenge_record": "_github-pages-challenge-example.example.com",
                    "verified_on": "2026-08-02",
                    "owner": "repository-and-private-domain-administrators",
                },
            },
            "versioning": {
                "schema_version": docs_ai_delivery.VERSIONING_SCHEMA_VERSION,
                "current": {
                    "channel": "current",
                    "kind": "rolling-branch",
                    "label": "Current",
                    "source_ref": "master",
                    "path_prefix": "",
                    "support": "latest-development-revision",
                },
                "releases": {
                    "status": "deferred",
                    "source": "git-tag",
                    "path_template": "/versions/{version}/",
                    "requires_support_policy": True,
                },
                "history": {"mode": "commit-addressable-ci-artifacts"},
            },
            "localization": {
                "schema_version": docs_ai_delivery.LOCALIZATION_SCHEMA_VERSION,
                "canonical_locale": "en",
                "locales": [
                    {
                        "id": "en",
                        "label": "English",
                        "path_prefix": "Docs/en",
                        "status": "canonical",
                    },
                    {
                        "id": "ru",
                        "label": "Russian",
                        "path_prefix": "Docs/ru",
                        "status": "complete",
                    },
                ],
                "path_strategy": "mirrored-relative-path",
                "translation_hash": "normalized-sha256",
                "translation_pending": "pre-production-only",
                "glossary": docs_localization.DEFAULT_GLOSSARY,
                "status_output": docs_localization.DEFAULT_OUTPUT,
                "enforcement": "existing-translations-current",
                "entrypoint_targets": {},
            },
            "ai_delivery": {
                "schema_version": docs_ai_delivery.SCHEMA_VERSION,
                "canonical_locale": "en",
                "source_ref": "master",
                "llms": {
                    "path": docs_ai_delivery.DEFAULT_LLMS_OUTPUT,
                    "start_document_ids": start_document_ids,
                },
                "full_context": {
                    "path": docs_ai_delivery.DEFAULT_FULL_CONTEXT_OUTPUT,
                    "max_bytes": 65536,
                    "generated_pages": "indexes-only",
                },
                "public_manifest": {
                    "path": docs_ai_delivery.DEFAULT_PUBLIC_MANIFEST_OUTPUT,
                },
            },
            "site_delivery": {
                "schema_version": docs_site.SCHEMA_VERSION,
                "layout": "default",
                "navigation_data_path": docs_site.DEFAULT_NAVIGATION_OUTPUT,
                "search": {
                    "path": docs_site.DEFAULT_SEARCH_OUTPUT,
                    "locale_paths": {
                        "en": docs_site.DEFAULT_SEARCH_OUTPUT,
                        "ru": docs_site.DEFAULT_RUSSIAN_SEARCH_OUTPUT,
                    },
                    "max_bytes": 65536,
                    "minimum_query_length": 2,
                },
                "browser_validation": {
                    "schema_version": 1,
                    "node": "24.16.0",
                    "playwright": "1.62.0",
                    "axe_core": "4.12.1",
                    "wcag_target": "WCAG 2.2 Level AA automated axe-core subset",
                    "wcag_tags": [
                        "wcag2a",
                        "wcag2aa",
                        "wcag21a",
                        "wcag21aa",
                        "wcag22a",
                        "wcag22aa",
                    ],
                    "profiles": [
                        {"id": "desktop", "width": 1440, "height": 1000},
                        {"id": "mobile", "width": 390, "height": 844},
                        {
                            "id": "zoom-200",
                            "width": 640,
                            "height": 512,
                            "physical_width": 1280,
                            "physical_height": 1024,
                            "device_scale_factor": 2,
                            "zoom_percent": 200,
                            "compact_navigation": True,
                        },
                    ],
                },
                "routing": {
                    "path": docs_site.DEFAULT_ROUTES_OUTPUT,
                    "current_permalink_strategy": "source-markdown-path",
                    "planned_permalink_strategy": "manifest-target-path",
                    "legacy_route_strategy": "durable-markdown-pointer",
                    "migration_status": "planned",
                },
                "navigation": [
                    {
                        "id": "start",
                        "title": "Start",
                        "title_ru": "Начало",
                        "document_ids": site_document_ids,
                    }
                ],
            },
            "generated_artifacts": {
                "source_inventory": {
                    "path": docs_inventory.DEFAULT_OUTPUT,
                    "generator": "BuildTools/docs_inventory.py",
                },
                "api_model": {
                    "path": docs_api.DEFAULT_OUTPUT,
                    "generator": "BuildTools/docs_api.py",
                    "source_parser": "BuildTools/codegen.py",
                    "schema_version": docs_api.SCHEMA_VERSION,
                },
                "reference_pages": {
                    "directory": docs_reference.DEFAULT_OUTPUT_DIR,
                    "generator": "BuildTools/docs_reference.py",
                    "source_model": docs_reference.DEFAULT_MODEL,
                    "paths": list(docs_reference.OUTPUT_PATHS),
                },
                "cmake_reference": {
                    "source_manifest": docs_cmake.DEFAULT_MANIFEST,
                    "model": docs_cmake.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_cmake.py",
                    "schema_version": docs_cmake.SCHEMA_VERSION,
                    "directory": docs_cmake.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_cmake.OUTPUT_PATHS),
                    "structural_test": "BuildTools/tests/validate_project_interface.cmake",
                },
                "cli_reference": {
                    "source_parser": docs_cli.DEFAULT_SOURCE,
                    "model": docs_cli.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_cli.py",
                    "schema_version": docs_cli.SCHEMA_VERSION,
                    "directory": docs_cli.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_cli.OUTPUT_PATHS),
                },
                "helper_cli_reference": {
                    "source_manifest": docs_helper_cli.DEFAULT_MANIFEST,
                    "model": docs_helper_cli.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_helper_cli.py",
                    "schema_version": docs_helper_cli.SCHEMA_VERSION,
                    "directory": docs_helper_cli.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_helper_cli.OUTPUT_PATHS),
                },
                "native_extension_reference": {
                    "source_manifest": docs_native_extension.DEFAULT_MANIFEST,
                    "model": docs_native_extension.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_native_extension.py",
                    "schema_version": docs_native_extension.SCHEMA_VERSION,
                    "directory": docs_native_extension.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_native_extension.OUTPUT_PATHS),
                    "structural_test": "BuildTools/tests/validate_native_extension_interface.cmake",
                },
                "prototype_format_reference": {
                    "source_manifest": docs_prototype_format.DEFAULT_MANIFEST,
                    "model": docs_prototype_format.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_prototype_format.py",
                    "schema_version": docs_prototype_format.SCHEMA_VERSION,
                    "directory": docs_prototype_format.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_prototype_format.OUTPUT_PATHS),
                },
                "map_format_reference": {
                    "source_manifest": docs_map_format.DEFAULT_MANIFEST,
                    "model": docs_map_format.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_map_format.py",
                    "schema_version": docs_map_format.SCHEMA_VERSION,
                    "directory": docs_map_format.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_map_format.OUTPUT_PATHS),
                },
                "model_format_reference": {
                    "source_manifest": docs_model_format.DEFAULT_MANIFEST,
                    "model": docs_model_format.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_model_format.py",
                    "schema_version": docs_model_format.SCHEMA_VERSION,
                    "directory": docs_model_format.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_model_format.OUTPUT_PATHS),
                },
                "text_format_reference": {
                    "source_manifest": docs_text_format.DEFAULT_MANIFEST,
                    "model": docs_text_format.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_text_format.py",
                    "schema_version": docs_text_format.SCHEMA_VERSION,
                    "directory": docs_text_format.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_text_format.OUTPUT_PATHS),
                },
                "effect_format_reference": {
                    "source_manifest": docs_effect_format.DEFAULT_MANIFEST,
                    "model": docs_effect_format.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_effect_format.py",
                    "schema_version": docs_effect_format.SCHEMA_VERSION,
                    "directory": docs_effect_format.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_effect_format.OUTPUT_PATHS),
                },
                "image_format_reference": {
                    "source_manifest": docs_image_format.DEFAULT_MANIFEST,
                    "model": docs_image_format.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_image_format.py",
                    "schema_version": docs_image_format.SCHEMA_VERSION,
                    "directory": docs_image_format.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_image_format.OUTPUT_PATHS),
                },
                "particle_format_reference": {
                    "source_manifest": docs_particle_format.DEFAULT_MANIFEST,
                    "model": docs_particle_format.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_particle_format.py",
                    "schema_version": docs_particle_format.SCHEMA_VERSION,
                    "directory": docs_particle_format.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_particle_format.OUTPUT_PATHS),
                },
                "font_format_reference": {
                    "source_manifest": docs_font_format.DEFAULT_MANIFEST,
                    "model": docs_font_format.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_font_format.py",
                    "schema_version": docs_font_format.SCHEMA_VERSION,
                    "directory": docs_font_format.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_font_format.OUTPUT_PATHS),
                },
                "audio_reference": {
                    "source_manifest": docs_audio.DEFAULT_MANIFEST,
                    "model": docs_audio.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_audio.py",
                    "schema_version": docs_audio.SCHEMA_VERSION,
                    "directory": docs_audio.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_audio.OUTPUT_PATHS),
                },
                "video_reference": {
                    "source_manifest": docs_video.DEFAULT_MANIFEST,
                    "model": docs_video.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_video.py",
                    "schema_version": docs_video.SCHEMA_VERSION,
                    "directory": docs_video.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_video.OUTPUT_PATHS),
                },
                "gui_runtime_reference": {
                    "source_manifest": docs_gui_runtime.DEFAULT_MANIFEST,
                    "model": docs_gui_runtime.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_gui_runtime.py",
                    "schema_version": docs_gui_runtime.SCHEMA_VERSION,
                    "directory": docs_gui_runtime.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_gui_runtime.OUTPUT_PATHS),
                },
                "ai_control_protocol_reference": {
                    "source_manifest": docs_ai_control_protocol.DEFAULT_MANIFEST,
                    "model": docs_ai_control_protocol.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_ai_control_protocol.py",
                    "schema_version": docs_ai_control_protocol.SCHEMA_VERSION,
                    "directory": docs_ai_control_protocol.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_ai_control_protocol.OUTPUT_PATHS),
                    "reference_client": "BuildTools/ai_control_client.py",
                    "sample": "Examples/AiControlSample",
                    "smoke": "Examples/AiControlSample/run_protocol_smoke.py",
                },
                "package_reference": {
                    "source_manifest": docs_package.DEFAULT_MANIFEST,
                    "source_parser": docs_package.DEFAULT_SOURCE,
                    "model": docs_package.DEFAULT_MODEL,
                    "generator": "BuildTools/docs_package.py",
                    "schema_version": docs_package.SCHEMA_VERSION,
                    "directory": docs_package.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_package.OUTPUT_PATHS),
                    "structural_test": "BuildTools/tests/validate_package_interface.cmake",
                },
                "public_examples_reference": {
                    "source_manifest": docs_examples.DEFAULT_MANIFEST,
                    "model": docs_examples.DEFAULT_MODEL,
                    "generator": docs_examples.GENERATED_BY,
                    "schema_version": docs_examples.SCHEMA_VERSION,
                    "directory": str(Path(docs_examples.DEFAULT_INDEX).parent).replace("\\", "/"),
                    "paths": list(docs_examples.OUTPUT_PATHS),
                },
                "support_matrix": {
                    "source_manifest": docs_support_matrix.DEFAULT_MANIFEST,
                    "model": docs_support_matrix.DEFAULT_MODEL,
                    "generator": docs_support_matrix.GENERATED_BY,
                    "schema_version": docs_support_matrix.SCHEMA_VERSION,
                    "directory": str(
                        Path(docs_support_matrix.DEFAULT_INDEX).parent
                    ).replace("\\", "/"),
                    "paths": list(docs_support_matrix.OUTPUT_PATHS),
                },
                "documentation_diagrams": {
                    "source_manifest": docs_diagrams.DEFAULT_MANIFEST,
                    "model": docs_diagrams.DEFAULT_CATALOG,
                    "generator": docs_diagrams.GENERATED_BY,
                    "schema_version": docs_diagrams.SCHEMA_VERSION,
                    "directory": docs_diagrams.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_diagrams.OUTPUT_PATHS),
                },
                "documentation_screenshots": {
                    "source_manifest": docs_screenshots.DEFAULT_MANIFEST,
                    "model": docs_screenshots.DEFAULT_CATALOG,
                    "generator": docs_screenshots.GENERATED_BY,
                    "schema_version": docs_screenshots.SCHEMA_VERSION,
                    "directory": docs_screenshots.DEFAULT_OUTPUT_DIR,
                    "paths": list(docs_screenshots.MANIFEST_PATHS),
                },
                "localization_status": {
                    "source_manifest": docs_localization.DEFAULT_MANIFEST,
                    "glossary": docs_localization.DEFAULT_GLOSSARY,
                    "path": docs_localization.DEFAULT_OUTPUT,
                    "generator": docs_localization.GENERATED_BY,
                    "schema_version": docs_localization.SCHEMA_VERSION,
                },
                "description_translation_status": {
                    "source_catalog": docs_description_translations.DEFAULT_CATALOG,
                    "models": docs_description_translations.MODEL_PATHS,
                    "path": docs_description_translations.DEFAULT_OUTPUT,
                    "generator": docs_description_translations.GENERATED_BY,
                    "schema_version": docs_description_translations.SCHEMA_VERSION,
                },
                "ai_delivery": {
                    "source_manifest": docs_ai_delivery.DEFAULT_MANIFEST,
                    "generator": docs_ai_delivery.GENERATED_BY,
                    "schema_version": docs_ai_delivery.SCHEMA_VERSION,
                    "paths": list(docs_ai_delivery.OUTPUT_PATHS),
                },
                "ai_evaluation": {
                    "source": docs_ai_eval.DEFAULT_SOURCE,
                    "search": docs_ai_eval.DEFAULT_SEARCH,
                    "path": docs_ai_eval.DEFAULT_OUTPUT,
                    "generator": docs_ai_eval.GENERATED_BY,
                    "schema_version": docs_ai_eval.SCHEMA_VERSION,
                    "test": "BuildTools/tests/test_docs_ai_eval.py",
                },
                "snippet_validation": {
                    "source_policy": docs_snippets.DEFAULT_POLICY,
                    "path": docs_snippets.DEFAULT_OUTPUT,
                    "generator": docs_snippets.GENERATED_BY,
                    "schema_version": docs_snippets.SCHEMA_VERSION,
                    "test": "BuildTools/tests/test_docs_snippets.py",
                    "external_check": (
                        "python3 BuildTools/docs_snippets.py --check --external"
                    ),
                },
                "site_delivery": {
                    "source_manifest": docs_site.DEFAULT_MANIFEST,
                    "generator": docs_site.GENERATED_BY,
                    "schema_version": docs_site.SCHEMA_VERSION,
                    "paths": list(docs_site.OUTPUT_PATHS),
                    "layout": "_layouts/default.html",
                    "assets": [
                        "assets/css/docs.css",
                        "assets/js/docs.js",
                        "assets/images/fonline-mark.png",
                    ],
                    "artifact_validator": "BuildTools/docs_site_artifact.py",
                    "artifact_report": "Workspace/docs-site-artifact-report.json",
                    "browser_validator": "BuildTools/docs-browser/audit.mjs",
                    "browser_package_lock": (
                        "BuildTools/docs-browser/package-lock.json"
                    ),
                    "browser_report": (
                        "Workspace/docs-browser-audit-report.json"
                    ),
                    "browser_screenshots": (
                        "Workspace/docs-browser-screenshots"
                    ),
                },
                "contract_diff": {
                    "generator": "BuildTools/docs_contract_diff.py",
                    "domain_generators": {
                        "api": "BuildTools/docs_api_diff.py",
                        "cmake": "BuildTools/docs_contract_diff.py",
                        "cli": "BuildTools/docs_contract_diff.py",
                        "package": "BuildTools/docs_contract_diff.py",
                        "helper-cli": "BuildTools/docs_contract_diff.py",
                        "native-extension": "BuildTools/docs_contract_diff.py",
                        "prototype-format": "BuildTools/docs_contract_diff.py",
                        "map-format": "BuildTools/docs_contract_diff.py",
                        "model-format": "BuildTools/docs_contract_diff.py",
                        "text-format": "BuildTools/docs_contract_diff.py",
                        "effect-format": "BuildTools/docs_contract_diff.py",
                        "image-format": "BuildTools/docs_contract_diff.py",
                        "particle-format": "BuildTools/docs_contract_diff.py",
                        "font-format": "BuildTools/docs_contract_diff.py",
                        "audio": "BuildTools/docs_contract_diff.py",
                        "video": "BuildTools/docs_contract_diff.py",
                        "gui-runtime": "BuildTools/docs_contract_diff.py",
                        "ai-control-protocol": "BuildTools/docs_contract_diff.py",
                    },
                    "source_models": {
                        "api": docs_api.DEFAULT_OUTPUT,
                        "cmake": docs_cmake.DEFAULT_MODEL,
                        "cli": docs_cli.DEFAULT_MODEL,
                        "package": docs_package.DEFAULT_MODEL,
                        "helper-cli": docs_helper_cli.DEFAULT_MODEL,
                        "native-extension": docs_native_extension.DEFAULT_MODEL,
                        "prototype-format": docs_prototype_format.DEFAULT_MODEL,
                        "map-format": docs_map_format.DEFAULT_MODEL,
                        "model-format": docs_model_format.DEFAULT_MODEL,
                        "text-format": docs_text_format.DEFAULT_MODEL,
                        "effect-format": docs_effect_format.DEFAULT_MODEL,
                        "image-format": docs_image_format.DEFAULT_MODEL,
                        "particle-format": docs_particle_format.DEFAULT_MODEL,
                        "font-format": docs_font_format.DEFAULT_MODEL,
                        "audio": docs_audio.DEFAULT_MODEL,
                        "video": docs_video.DEFAULT_MODEL,
                        "gui-runtime": docs_gui_runtime.DEFAULT_MODEL,
                        "ai-control-protocol": docs_ai_control_protocol.DEFAULT_MODEL,
                    },
                    "dispositions": docs_contract_diff.DEFAULT_DISPOSITIONS,
                    "schema_version": docs_contract_diff.SCHEMA_VERSION,
                    "report_outputs": [
                        docs_contract_diff.DEFAULT_JSON_OUTPUT,
                        docs_contract_diff.DEFAULT_MARKDOWN_OUTPUT,
                    ],
                    "workflow": ".github/workflows/validate.yml",
                },
            },
            "owners": {"documentation": "Documentation owner."},
            "owner_review_requirements": {
                "documentation": {
                    "scope": "Documentation fixtures.",
                    "required_evidence": ["fixture validation"],
                    "co_review_when": ["fixture ownership changes"],
                }
            },
            "documents": documents,
        }
        generated_path = root / docs_inventory.DEFAULT_OUTPUT
        generated_path.parent.mkdir(parents=True, exist_ok=True)
        generated_path.write_text(docs_inventory.render_inventory(root), encoding="utf-8")
        api_path = root / docs_api.DEFAULT_OUTPUT
        api_path.write_text(docs_api.render_api_model(root), encoding="utf-8")
        for reference_path, content in docs_reference.render_reference_pages(root).items():
            output_path = root / reference_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        cmake_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_cmake.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        cmake_manifest_path = root / docs_cmake.DEFAULT_MANIFEST
        cmake_manifest_path.parent.mkdir(parents=True, exist_ok=True)
        cmake_manifest_path.write_text(json.dumps(cmake_manifest, indent=2) + "\n", encoding="utf-8")
        for key in ("stages", "helpers"):
            for entry in cmake_manifest[key]:
                source_path = root / entry["source"]
                source_path.parent.mkdir(parents=True, exist_ok=True)
                source_path.write_text("# fixture\n", encoding="utf-8")
        structural_test_path = root / "BuildTools/tests/validate_project_interface.cmake"
        structural_test_path.parent.mkdir(parents=True, exist_ok=True)
        structural_test_path.write_text("# fixture\n", encoding="utf-8")
        cmake_model_path = root / docs_cmake.DEFAULT_MODEL
        cmake_model_path.write_text(docs_cmake.render_cmake_model(root), encoding="utf-8")
        for cmake_page_path, content in docs_cmake.render_reference_pages(root).items():
            output_path = root / cmake_page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        cli_source_path = root / docs_cli.DEFAULT_SOURCE
        cli_source_path.parent.mkdir(parents=True, exist_ok=True)
        cli_source_path.write_text(
            "import argparse\n\n"
            "def create_parser():\n"
            "    parser = argparse.ArgumentParser(description='Fixture tools')\n"
            "    commands = parser.add_subparsers(dest='command', required=True)\n"
            "    inspect_parser = commands.add_parser('inspect', help='inspect a fixture')\n"
            "    inspect_parser.add_argument('path', help='fixture path')\n"
            "    return parser\n\n"
            "# The Effekseer Editor auxiliary build requires Windows\n"
            "# BuildTools' / 'EffekseerEditor' / 'build.ps1\n",
            encoding="utf-8",
        )
        cli_model_path = root / docs_cli.DEFAULT_MODEL
        cli_model_path.write_text(docs_cli.render_cli_model(root), encoding="utf-8")
        for cli_page_path, content in docs_cli.render_reference_pages(root).items():
            output_path = root / cli_page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        package_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_package.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        package_manifest_path = root / docs_package.DEFAULT_MANIFEST
        package_manifest_path.write_text(json.dumps(package_manifest, indent=2) + "\n", encoding="utf-8")
        declaration = package_manifest["declaration"]
        for source in (declaration["source"], declaration["consumer"]):
            source_path = root / source
            source_path.parent.mkdir(parents=True, exist_ok=True)
            source_path.write_text("# fixture\n", encoding="utf-8")
        package_source_path = root / docs_package.DEFAULT_SOURCE
        package_source_path.write_text(
            "import argparse\n\n"
            "def create_parser():\n"
            "    parser = argparse.ArgumentParser(description='Fixture packager')\n"
            f"    parser.add_argument('-target', choices={[entry['name'] for entry in package_manifest['targets']]!r}, required=True)\n"
            f"    parser.add_argument('-platform', choices={[entry['name'] for entry in package_manifest['platforms']]!r}, required=True)\n"
            "    parser.add_argument('-pack', required=True)\n"
            "    return parser\n",
            encoding="utf-8",
        )
        package_structural_test = root / "BuildTools/tests/validate_package_interface.cmake"
        package_structural_test.parent.mkdir(parents=True, exist_ok=True)
        package_structural_test.write_text("# fixture\n", encoding="utf-8")
        package_model_path = root / docs_package.DEFAULT_MODEL
        package_model_path.write_text(docs_package.render_package_model(root), encoding="utf-8")
        for package_page_path, content in docs_package.render_reference_pages(root).items():
            output_path = root / package_page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        shutil.copytree(
            BUILDTOOLS_DIR.parent / "Examples" / "MinimalProject",
            root / "Examples" / "MinimalProject",
        )
        shutil.copytree(
            BUILDTOOLS_DIR.parent / "Examples" / "MinimalMultiplayer",
            root / "Examples" / "MinimalMultiplayer",
            ignore=shutil.ignore_patterns("Build", "Engine"),
        )
        shutil.copytree(
            BUILDTOOLS_DIR.parent / "Examples" / "NativeExtensionSample",
            root / "Examples" / "NativeExtensionSample",
            ignore=shutil.ignore_patterns("Build", "Engine"),
        )
        shutil.copytree(
            BUILDTOOLS_DIR.parent / "Examples" / "ContentShowcase",
            root / "Examples" / "ContentShowcase",
            ignore=shutil.ignore_patterns("Build", "Engine", "Workspace"),
        )
        shutil.copytree(
            BUILDTOOLS_DIR.parent / "Examples" / "PublicRepositoryTemplate",
            root / "Examples" / "PublicRepositoryTemplate",
        )
        public_examples_manifest_path = root / docs_examples.DEFAULT_MANIFEST
        public_examples_manifest_path.parent.mkdir(parents=True, exist_ok=True)
        public_examples_manifest_path.write_text(
            (BUILDTOOLS_DIR.parent / docs_examples.DEFAULT_MANIFEST).read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        for public_examples_path, content in docs_examples.render_outputs(root).items():
            output_path = root / public_examples_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        support_manifest = {
            "schema_version": docs_support_matrix.SCHEMA_VERSION,
            "title": "Fixture support matrix",
            "channel": "current",
            "policy": {
                "build_gated": "Built by required CI.",
                "smoke_gated": "Built and run by required CI.",
                "source_capable": "Available without required CI.",
                "not_in_public_matrix": "No public validation profile.",
            },
            "sources": {
                "workflow": ".github/workflows/validate.yml",
                "validation_registry": "BuildTools/buildtools.py",
                "platform_configuration": "BuildTools/cmake/stages/Init.cmake",
                "application_targets": "BuildTools/cmake/stages/Applications.cmake",
            },
            "platforms": [
                {
                    "id": "linux-client",
                    "host": "Ubuntu",
                    "target": "Linux x64",
                    "compiler": "Clang",
                    "level": "source_capable",
                    "available_validation_targets": ["linux-client"],
                    "ci_validation_targets": [],
                    "applications": ["desktop client"],
                    "runtime_evidence": "No process smoke.",
                    "limitations": "Fixture profile.",
                }
            ],
            "renderer_policy": [
                {
                    "platforms": "Linux",
                    "compiled_backends": "OpenGL",
                    "qualification": "Visible acceptance remains project-owned.",
                }
            ],
        }
        support_manifest_path = root / docs_support_matrix.DEFAULT_MANIFEST
        support_manifest_path.parent.mkdir(parents=True, exist_ok=True)
        support_manifest_path.write_text(
            json.dumps(support_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        for source in (
            "BuildTools/cmake/stages/Init.cmake",
            "BuildTools/cmake/stages/Applications.cmake",
        ):
            source_path = root / source
            source_path.parent.mkdir(parents=True, exist_ok=True)
            source_path.write_text("# fixture\n", encoding="utf-8")
        for support_path, content in docs_support_matrix.render_outputs(root).items():
            output_path = root / support_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        glossary_path = root / docs_localization.DEFAULT_GLOSSARY
        glossary_path.write_text(
            json.dumps(
                {
                    "schema_version": docs_localization.SCHEMA_VERSION,
                    "source_locale": "en",
                    "target_locale": "ru",
                    "terms": [
                        {
                            "term": "guide",
                            "russian": "руководство",
                            "policy": "translate",
                            "note": "Fixture term.",
                        }
                    ],
                },
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        helper_source_path = root / "BuildTools/helper-tool.py"
        helper_source_path.write_text(
            "import argparse\n\n"
            "def create_parser():\n"
            "    parser = argparse.ArgumentParser(prog='helper-tool.py', description='Fixture helper')\n"
            "    parser.add_argument('--value', help='fixture value')\n"
            "    return parser\n",
            encoding="utf-8",
        )
        ai_control_client_path = root / "BuildTools/ai_control_client.py"
        ai_control_client_path.write_text(
            (BUILDTOOLS_DIR.parent / "BuildTools/ai_control_client.py").read_text(
                encoding="utf-8"
            ),
            encoding="utf-8",
        )
        helper_manifest = {
            "schema_version": 1,
            "description": "Fixture helper CLI manifest.",
            "scope": {
                "surface": "helper-cli",
                "stability": "internal",
                "since": None,
                "support_note": "Pin a fixture revision.",
                "included": ["fixture helper"],
                "excluded": ["dedicated CLIs"],
            },
            "discovery": {
                "root": "BuildTools",
                "excluded_directories": ["tests"],
                "excluded_name_prefixes": ["docs_"],
                "excluded_parser_sources": [
                    {"source": docs_cli.DEFAULT_SOURCE, "reason": "Dedicated CLI model."},
                    {"source": docs_package.DEFAULT_SOURCE, "reason": "Dedicated package model."},
                    {
                        "source": "BuildTools/ai_control_client.py",
                        "reason": "Dedicated AiControl protocol client.",
                    },
                ],
            },
            "helpers": [
                {
                    "id": "helper-cli.helper-tool",
                    "name": "Helper tool",
                    "source": "BuildTools/helper-tool.py",
                    "factory": "create_parser",
                    "program": "helper-tool.py",
                    "owner": "quality",
                    "audiences": ["engine-contributor"],
                    "invocation_owner": "fixture test",
                    "description": "Exercise helper CLI validation.",
                }
            ],
        }
        helper_manifest_path = root / docs_helper_cli.DEFAULT_MANIFEST
        helper_manifest_path.write_text(json.dumps(helper_manifest, indent=2) + "\n", encoding="utf-8")
        helper_model_path = root / docs_helper_cli.DEFAULT_MODEL
        helper_model_path.write_text(docs_helper_cli.render_helper_cli_model(root), encoding="utf-8")
        helper_model = json.loads(helper_model_path.read_text(encoding="utf-8"))
        for helper_page_path, content in docs_helper_cli.generate_reference_pages(helper_model).items():
            output_path = root / helper_page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        native_extension_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_native_extension.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        native_extension_manifest_path = root / docs_native_extension.DEFAULT_MANIFEST
        native_extension_manifest_path.write_text(
            json.dumps(native_extension_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        native_codegen_path = root / native_extension_manifest["registration"]["codegen_parser"]
        native_codegen_path.write_text("# fixture codegen\n", encoding="utf-8")
        call_site_hooks: dict[str, list[str]] = {}
        for hook in native_extension_manifest["hooks"]:
            for call_site in hook["call_sites"]:
                call_site_hooks.setdefault(call_site, []).append(hook["name"])
        for call_site, hook_names in call_site_hooks.items():
            call_site_path = root / call_site
            call_site_path.parent.mkdir(parents=True, exist_ok=True)
            call_site_path.write_text("\n".join(hook_names) + "\n", encoding="utf-8")
        native_extension_structural_test = root / "BuildTools/tests/validate_native_extension_interface.cmake"
        native_extension_structural_test.parent.mkdir(parents=True, exist_ok=True)
        native_extension_structural_test.write_text("# fixture\n", encoding="utf-8")
        native_extension_model_path = root / docs_native_extension.DEFAULT_MODEL
        native_extension_model_path.write_text(
            docs_native_extension.render_native_extension_model(root),
            encoding="utf-8",
        )
        native_extension_model = json.loads(native_extension_model_path.read_text(encoding="utf-8"))
        for page_path, content in docs_native_extension.generate_reference_pages(native_extension_model).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        prototype_format_manifest = {
            "schema_version": 1,
            "description": "Fixture prototype format manifest.",
            "scope": {
                "surface": "prototype-format",
                "stability": "experimental",
                "since": None,
                "support_note": "Pin a fixture revision.",
                "included": ["fixture syntax"],
                "excluded": ["fixture project semantics"],
            },
            "sources": {
                "source_parser": "Source/example.txt",
                "config_parser": "Source/example.txt",
                "property_parser": "Source/example.txt",
                "property_serializer": "Source/example.txt",
                "settings": "Source/Common/Settings.inc",
                "api_model_generator": "Source/example.txt",
                "api_model": docs_api.DEFAULT_OUTPUT,
                "tests": ["Source/example.txt"],
            },
            "file_selection": {
                "setting": "Baking.ProtoFileExtensions",
                "setting_anchor": "FIXED_SETTING(vector<string>, Baking, ProtoFileExtensions",
                "output_pattern": "<pack>.fopro-bin-<side>",
                "runtime_sides": ["server", "client", "mapper"],
                "nested_sections_skipped": True,
                "description": "Fixture selection.",
            },
            "section_forms": [
                {
                    "id": "prototype-format.section.fixture",
                    "name": "Fixture section",
                    "syntax": "[ProtoFixture]",
                    "resolves_to": "Fixture",
                    "stability": "experimental",
                    "description": "Fixture section form.",
                    "source": {
                        "path": "Source/example.txt",
                        "anchors": ["fixture-anchor"],
                    },
                }
            ],
            "directives": [
                {
                    "id": "prototype-format.directive.fixture",
                    "name": "$Fixture",
                    "syntax": "$Fixture = value",
                    "default": "none",
                    "inherited": False,
                    "stability": "experimental",
                    "description": "Fixture directive.",
                    "source": {
                        "path": "Source/example.txt",
                        "anchors": ["fixture-anchor"],
                    },
                }
            ],
            "rules": [
                {
                    "id": "prototype-format.rule.fixture",
                    "name": "Fixture rule",
                    "requirement": "Keep the fixture valid.",
                    "stability": "experimental",
                    "description": "Fixture validation rule.",
                    "source": {
                        "path": "Source/example.txt",
                        "anchors": ["fixture-anchor"],
                    },
                }
            ],
        }
        prototype_format_manifest_path = root / docs_prototype_format.DEFAULT_MANIFEST
        prototype_format_manifest_path.write_text(
            json.dumps(prototype_format_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        prototype_format_model_path = root / docs_prototype_format.DEFAULT_MODEL
        prototype_format_model_path.write_text(
            docs_prototype_format.render_prototype_format_model(root),
            encoding="utf-8",
        )
        prototype_format_model = json.loads(
            prototype_format_model_path.read_text(encoding="utf-8")
        )
        for page_path, content in docs_prototype_format.generate_reference_pages(
            prototype_format_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        map_format_manifest = {
            "schema_version": 1,
            "description": "Fixture map format manifest.",
            "scope": {
                "surface": "map-format",
                "stability": "experimental",
                "since": None,
                "support_note": "Pin a fixture revision.",
                "included": ["fixture map syntax"],
                "excluded": ["fixture project semantics"],
            },
            "sources": {
                "source_loader": "Source/example.txt",
                "config_parser": "Source/example.txt",
                "source_baker": "Source/example.txt",
                "prototype_baker": "Source/example.txt",
                "text_baker": "Source/example.txt",
                "source_mapper": "Source/example.txt",
                "map_serializer": "Source/example.txt",
                "server_loader": "Source/example.txt",
                "client_loader": "Source/example.txt",
                "api_model_generator": "Source/example.txt",
                "api_model": docs_api.DEFAULT_OUTPUT,
                "tests": ["Source/example.txt"],
            },
            "outputs": {
                "source_selection": "fixture extension selection plus ProtoMap anchor",
                "conventional_extension": ".fomap",
                "server_pattern": "<MapId>.fomap-bin-server",
                "client_pattern": "<MapId>.fomap-bin-client",
                "runtime_sides": ["server", "client", "mapper"],
                "description": "Fixture outputs.",
            },
            "sections": [
                {
                    "id": "map-format.section.fixture",
                    "name": "Fixture section",
                    "syntax": "[ProtoMap]",
                    "receiver": "Map",
                    "cardinality": "exactly one, first",
                    "stability": "experimental",
                    "description": "Fixture section form.",
                    "source": {"path": "Source/example.txt", "anchors": ["fixture-anchor"]},
                }
            ],
            "directives": [
                {
                    "id": "map-format.directive.fixture",
                    "name": "$Fixture",
                    "applies_to": "ProtoMap",
                    "syntax": "$Fixture = value",
                    "required": False,
                    "default": "none",
                    "stability": "experimental",
                    "description": "Fixture directive.",
                    "source": {"path": "Source/example.txt", "anchors": ["fixture-anchor"]},
                }
            ],
            "ownerships": [
                {
                    "id": f"map-format.ownership.{name.lower()}",
                    "name": name,
                    "value": value,
                    "supported": name != "Nowhere",
                    "owner_property": "none",
                    "stability": "experimental",
                    "description": "Fixture ownership.",
                    "source": {"path": "Source/example.txt", "anchors": ["fixture-anchor"]},
                }
                for value, name in enumerate(
                    ("MapHex", "CritterInventory", "ItemContainer", "Nowhere")
                )
            ],
            "rules": [
                {
                    "id": "map-format.rule.fixture",
                    "name": "Fixture rule",
                    "requirement": "Keep the fixture valid.",
                    "stability": "experimental",
                    "description": "Fixture validation rule.",
                    "source": {"path": "Source/example.txt", "anchors": ["fixture-anchor"]},
                }
            ],
        }
        map_format_manifest_path = root / docs_map_format.DEFAULT_MANIFEST
        map_format_manifest_path.write_text(
            json.dumps(map_format_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        map_format_model_path = root / docs_map_format.DEFAULT_MODEL
        map_format_model_path.write_text(
            docs_map_format.render_map_format_model(root),
            encoding="utf-8",
        )
        map_format_model = json.loads(map_format_model_path.read_text(encoding="utf-8"))
        for page_path, content in docs_map_format.generate_reference_pages(
            map_format_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        model_format_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_model_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        model_format_manifest_path = root / docs_model_format.DEFAULT_MANIFEST
        model_format_manifest_path.write_text(
            json.dumps(model_format_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        model_format_source_paths = {
            str(value)
            for key, value in model_format_manifest["sources"].items()
            if key != "tests"
        }
        model_format_source_paths.update(model_format_manifest["sources"]["tests"])
        for collection in ("compile_limits", "assets", "tokens", "rules"):
            for entry in model_format_manifest[collection]:
                model_format_source_paths.update(
                    source["path"] for source in entry["source"]
                )
        model_format_source_paths.discard(docs_cmake.DEFAULT_MANIFEST)
        for relative_path in sorted(model_format_source_paths):
            source_path = BUILDTOOLS_DIR.parent / relative_path
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            target_path.write_text(source_path.read_text(encoding="utf-8"), encoding="utf-8")
        model_format_model_path = root / docs_model_format.DEFAULT_MODEL
        model_format_model_path.write_text(
            docs_model_format.render_model_format_model(root),
            encoding="utf-8",
        )
        model_format_model = json.loads(
            model_format_model_path.read_text(encoding="utf-8")
        )
        for page_path, content in docs_model_format.generate_reference_pages(
            model_format_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        text_format_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_text_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        text_format_manifest_path = root / docs_text_format.DEFAULT_MANIFEST
        text_format_manifest_path.write_text(
            json.dumps(text_format_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        text_format_source_paths = {
            str(value)
            for key, value in text_format_manifest["sources"].items()
            if key != "tests"
        }
        text_format_source_paths.update(text_format_manifest["sources"]["tests"])
        text_format_anchors: dict[str, set[str]] = {}
        for collection in docs_text_format.COLLECTION_KINDS:
            for entry in text_format_manifest[collection]:
                for source in entry["source"]:
                    relative_path = source["path"]
                    text_format_source_paths.add(relative_path)
                    text_format_anchors.setdefault(relative_path, set()).update(
                        source["anchors"]
                    )
        for relative_path in sorted(text_format_source_paths):
            source_path = BUILDTOOLS_DIR.parent / relative_path
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            if relative_path.startswith("Source/Scripting/"):
                target_path.write_text(
                    "\n".join(sorted(text_format_anchors.get(relative_path, {"fixture"})))
                    + "\n",
                    encoding="utf-8",
                )
            else:
                target_path.write_text(
                    source_path.read_text(encoding="utf-8"), encoding="utf-8"
                )
        text_format_model_path = root / docs_text_format.DEFAULT_MODEL
        text_format_model_path.write_text(
            docs_text_format.render_text_format_model(root),
            encoding="utf-8",
        )
        text_format_model = json.loads(
            text_format_model_path.read_text(encoding="utf-8")
        )
        for page_path, content in docs_text_format.generate_reference_pages(
            text_format_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        effect_format_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_effect_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        effect_format_manifest_path = root / docs_effect_format.DEFAULT_MANIFEST
        effect_format_manifest_path.write_text(
            json.dumps(effect_format_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        effect_format_source_paths = {
            str(value)
            for key, value in effect_format_manifest["sources"].items()
            if key != "tests"
        }
        effect_format_source_paths.update(effect_format_manifest["sources"]["tests"])
        effect_format_anchors: dict[str, set[str]] = {}
        for collection in (
            "compile_limits",
            *docs_effect_format.COLLECTION_KINDS,
        ):
            for entry in effect_format_manifest[collection]:
                for source in entry["source"]:
                    relative_path = source["path"]
                    effect_format_source_paths.add(relative_path)
                    effect_format_anchors.setdefault(relative_path, set()).update(
                        source["anchors"]
                    )
        for relative_path in sorted(effect_format_source_paths):
            source_path = BUILDTOOLS_DIR.parent / relative_path
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            if relative_path.startswith("Source/Scripting/"):
                existing = (
                    target_path.read_text(encoding="utf-8")
                    if target_path.exists()
                    else ""
                )
                target_path.write_text(
                    existing
                    + "\n".join(
                        sorted(effect_format_anchors.get(relative_path, {"fixture"}))
                    )
                    + "\n",
                    encoding="utf-8",
                )
            else:
                target_path.write_text(
                    source_path.read_text(encoding="utf-8"), encoding="utf-8"
                )
        effect_format_model_path = root / docs_effect_format.DEFAULT_MODEL
        effect_format_model_path.write_text(
            docs_effect_format.render_effect_format_model(root),
            encoding="utf-8",
        )
        effect_format_model = json.loads(
            effect_format_model_path.read_text(encoding="utf-8")
        )
        for page_path, content in docs_effect_format.generate_reference_pages(
            effect_format_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        image_format_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_image_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        image_format_manifest_path = root / docs_image_format.DEFAULT_MANIFEST
        image_format_manifest_path.write_text(
            json.dumps(image_format_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        image_format_source_paths = {
            str(value)
            for key, value in image_format_manifest["sources"].items()
            if key != "tests"
        }
        image_format_source_paths.update(image_format_manifest["sources"]["tests"])
        for collection in docs_image_format.COLLECTION_KINDS:
            for entry in image_format_manifest[collection]:
                for source in entry["source"]:
                    image_format_source_paths.add(source["path"])
        for relative_path in sorted(image_format_source_paths):
            source_path = BUILDTOOLS_DIR.parent / relative_path
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            target_path.write_text(
                source_path.read_text(encoding="utf-8"), encoding="utf-8"
            )
        image_format_model_path = root / docs_image_format.DEFAULT_MODEL
        image_format_model_path.write_text(
            docs_image_format.render_image_format_model(root),
            encoding="utf-8",
        )
        image_format_model = json.loads(
            image_format_model_path.read_text(encoding="utf-8")
        )
        for page_path, content in docs_image_format.generate_reference_pages(
            image_format_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        particle_format_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_particle_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        particle_format_manifest_path = root / docs_particle_format.DEFAULT_MANIFEST
        particle_format_manifest_path.write_text(
            json.dumps(particle_format_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        particle_format_source_paths = set(particle_format_manifest["sources"])
        particle_format_anchors: dict[str, set[str]] = {}
        for collection in docs_particle_format.MANIFEST_COLLECTIONS:
            for entry in particle_format_manifest[collection]:
                for source in entry["source"]:
                    particle_format_source_paths.add(source["path"])
                    particle_format_anchors.setdefault(source["path"], set()).update(
                        source["anchors"]
                    )
        for relative_path in sorted(particle_format_source_paths):
            source_path = BUILDTOOLS_DIR.parent / relative_path
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            if relative_path == docs_cli.DEFAULT_SOURCE:
                continue
            if relative_path.startswith("Source/Scripting/"):
                existing = (
                    target_path.read_text(encoding="utf-8")
                    if target_path.exists()
                    else ""
                )
                target_path.write_text(
                    existing
                    + "\n".join(
                        sorted(particle_format_anchors.get(relative_path, {"fixture"}))
                    )
                    + "\n",
                    encoding="utf-8",
                )
            else:
                target_path.write_text(
                    source_path.read_text(encoding="utf-8"), encoding="utf-8"
                )
        particle_format_model_path = root / docs_particle_format.DEFAULT_MODEL
        particle_format_model_path.write_text(
            docs_particle_format.render_particle_format_model(root),
            encoding="utf-8",
        )
        particle_format_model = json.loads(
            particle_format_model_path.read_text(encoding="utf-8")
        )
        for page_path, content in docs_particle_format.generate_reference_pages(
            particle_format_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        font_format_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_font_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        font_format_manifest_path = root / docs_font_format.DEFAULT_MANIFEST
        font_format_manifest_path.write_text(
            json.dumps(font_format_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        font_format_source_paths = {
            str(value)
            for key, value in font_format_manifest["sources"].items()
            if key not in {"tests", "font_resources"}
        }
        font_format_source_paths.update(font_format_manifest["sources"]["tests"])
        for collection in docs_font_format.COLLECTION_KINDS:
            for entry in font_format_manifest[collection]:
                for source in entry["source"]:
                    font_format_source_paths.add(source["path"])
        for relative_path in sorted(font_format_source_paths):
            source_path = BUILDTOOLS_DIR.parent / relative_path
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            source_text = source_path.read_text(encoding="utf-8")
            if relative_path.startswith("Source/"):
                source_text = "\n".join(
                    line for line in source_text.splitlines() if "///@" not in line
                ) + "\n"
            if target_path.exists():
                existing = target_path.read_text(encoding="utf-8")
                if source_text not in existing:
                    target_path.write_text(existing + "\n" + source_text, encoding="utf-8")
            else:
                target_path.write_text(source_text, encoding="utf-8")
        font_resources = Path(font_format_manifest["sources"]["font_resources"])
        shutil.copytree(
            BUILDTOOLS_DIR.parent / font_resources,
            root / font_resources,
            dirs_exist_ok=True,
        )
        font_format_model_path = root / docs_font_format.DEFAULT_MODEL
        font_format_model_path.write_text(
            docs_font_format.render_font_format_model(root),
            encoding="utf-8",
        )
        font_format_model = json.loads(
            font_format_model_path.read_text(encoding="utf-8")
        )
        for page_path, content in docs_font_format.generate_reference_pages(
            font_format_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        audio_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_audio.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        audio_manifest_path = root / docs_audio.DEFAULT_MANIFEST
        audio_manifest_path.write_text(
            json.dumps(audio_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        audio_source_paths = {
            str(value)
            for key, value in audio_manifest["sources"].items()
            if key != "native_test_directory"
        }
        audio_anchors: dict[str, set[str]] = {}
        for collection in docs_audio.COLLECTION_KINDS:
            for entry in audio_manifest[collection]:
                for source in entry["source"]:
                    relative_path = source["path"]
                    audio_source_paths.add(relative_path)
                    audio_anchors.setdefault(relative_path, set()).update(
                        source["anchors"]
                    )
        audio_outputs = audio_manifest["outputs"]
        sound_fixture = (
            f'if (ext.empty()) {{\n    ext = "{audio_outputs["default_extension"]}";\n}}\n'
            + "\n".join(
                f'if (ext == "{extension}") {{}}'
                for extension in audio_outputs["decoder_extensions"]
            )
            + "\n#if FO_WEB\n"
            f'_streamingPortion = {hex(audio_outputs["ogg"]["web_stream_chunk_bytes"])};\n'
            "#else\n"
            f'_streamingPortion = {hex(audio_outputs["ogg"]["native_stream_chunk_bytes"])};\n'
            "#endif\n"
            f'if (WFormatTag != {audio_outputs["wav"]["format_tag"]}) {{}}\n'
            + "\n".join(
                f"case {bits}: sound->OriginalFormat = AppAudio::AUDIO_FORMAT_FIXTURE;"
                for bits in audio_outputs["wav"]["sample_bits"]
            )
            + f'\nsound->OriginalRate = {audio_outputs["acm"]["sample_rate"]};\n'
            + (
                'WriteLog("Unsupported sound format");\n'
                if audio_outputs["unsupported_extension_rejected"]
                else ""
            )
        )
        settings_fixture = (
            "FIXED_SETTING(vector<string>, Baking, RawCopyFileExtensions, "
            + ", ".join(
                f'"{extension}"'
                for extension in audio_outputs["raw_copy_extensions"]
            )
            + ");\n"
            f'VARIABLE_SETTING(bool, Audio, DisableAudio, {str(audio_outputs["audio_settings"]["DisableAudio"]).lower()})\n'
            f'VARIABLE_SETTING(int32_t, Audio, SoundVolume, {audio_outputs["audio_settings"]["SoundVolume"]})\n'
            f'VARIABLE_SETTING(int32_t, Audio, MusicVolume, {audio_outputs["audio_settings"]["MusicVolume"]})\n'
        )
        audio_fixtures = {
            audio_manifest["sources"]["sound_manager"]: sound_fixture,
            audio_manifest["sources"]["resource_manager"]: (
                "sound_extensions = {"
                + ", ".join(
                    f'"{extension}"'
                    for extension in audio_outputs["indexed_extensions"]
                )
                + "};\n"
            ),
            audio_manifest["sources"]["settings"]: settings_fixture,
            audio_manifest["sources"]["application"]: (
                f'std::clamp(volume, {audio_outputs["mix_volume_range"][0]}, '
                f'{audio_outputs["mix_volume_range"][1]});\n'
            ),
            audio_manifest["sources"]["application_headless"]: (
                "auto AppAudio::IsEnabled() const -> bool\n{\n"
                "    return false;\n"
                "}\n"
            ),
        }
        for relative_path in sorted(audio_source_paths):
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            source_text = audio_fixtures.get(relative_path, "")
            source_text += "\n".join(
                sorted(audio_anchors.get(relative_path, {"fixture"}))
            ) + "\n"
            if target_path.exists():
                existing = target_path.read_text(encoding="utf-8")
                if source_text not in existing:
                    target_path.write_text(
                        existing + "\n" + source_text,
                        encoding="utf-8",
                    )
            else:
                target_path.write_text(source_text, encoding="utf-8")
        (root / audio_manifest["sources"]["native_test_directory"]).mkdir(
            parents=True,
            exist_ok=True,
        )
        audio_model_path = root / docs_audio.DEFAULT_MODEL
        audio_model_path.write_text(
            docs_audio.render_audio_model(root),
            encoding="utf-8",
        )
        audio_model = json.loads(audio_model_path.read_text(encoding="utf-8"))
        for page_path, content in docs_audio.generate_reference_pages(
            audio_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        video_manifest = json.loads(
            (BUILDTOOLS_DIR.parent / docs_video.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        video_manifest_path = root / docs_video.DEFAULT_MANIFEST
        video_manifest_path.write_text(
            json.dumps(video_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        video_source_paths = {
            str(value)
            for key, value in video_manifest["sources"].items()
            if key != "native_test_directory"
        }
        video_anchors: dict[str, set[str]] = {}
        for collection in docs_video.COLLECTION_KINDS:
            for entry in video_manifest[collection]:
                for source in entry["source"]:
                    relative_path = source["path"]
                    video_source_paths.add(relative_path)
                    video_anchors.setdefault(relative_path, set()).update(
                        source["anchors"]
                    )
        video_fixtures = {
            video_manifest["sources"]["video_clip"]: (
                '#include "theora/theoradec.h"\n'
                "static constexpr size_t COUNT = 10;\n"
                "vector<uint8_t> RawVideoData;\n"
                "RawVideoData = std::move(video_data);\n"
                "vector<ucolor> RenderedTextureData;\n"
                "read_bytes = std::min(1024, read_bytes);\n"
                "ogg_sync_pageout();\n"
                "th_decode_headerin();\n"
                "th_decode_packetin();\n"
                "TH_PF_420 TH_PF_422 TH_PF_444\n"
                "pixel.comp.a = 0xFF;\n"
            ),
            video_manifest["sources"]["client"]: (
                "strex(video_name).split('|');\n"
                "Resources.ReadFile(names.front());\n"
                "SndMngr.PlayMusic(names[1]);\n"
                "SprMngr.DrawTexture(_video->Tex, false);\n"
                "OnRenderIface.Fire();\n"
                "ProcessVideo();\n"
            ),
            video_manifest["sources"]["client_global_scripts"]: (
                "Client_Game_CreateVideoPlayback\n"
                "VideoClip clip {file.GetData()};\n"
                "Client_Game_DrawVideoPlayback\n"
                "only in RenderIface event\n"
            ),
            video_manifest["sources"]["sprite_manager"]: (
                "if (!region_from && !region_to) {}\n"
                "width_to_i height_to_i\n"
            ),
            video_manifest["sources"]["third_party"]: (
                "# Theora\n"
                "AddStaticThirdPartyLibrary(Theora\n"
                "APPEND_TO FO_CLIENT_LIBS\n"
            ),
        }
        for relative_path in sorted(video_source_paths):
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            source_text = video_fixtures.get(relative_path, "")
            source_text += "\n".join(
                sorted(video_anchors.get(relative_path, {"fixture"}))
            ) + "\n"
            if target_path.exists():
                existing = target_path.read_text(encoding="utf-8")
                if source_text not in existing:
                    target_path.write_text(
                        existing + "\n" + source_text,
                        encoding="utf-8",
                    )
            else:
                target_path.write_text(source_text, encoding="utf-8")
        (root / video_manifest["sources"]["native_test_directory"]).mkdir(
            parents=True,
            exist_ok=True,
        )
        video_model_path = root / docs_video.DEFAULT_MODEL
        video_model_path.write_text(
            docs_video.render_video_model(root),
            encoding="utf-8",
        )
        video_model = json.loads(video_model_path.read_text(encoding="utf-8"))
        for page_path, content in docs_video.generate_reference_pages(
            video_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        gui_runtime_manifest = json.loads(
            (
                BUILDTOOLS_DIR.parent / docs_gui_runtime.DEFAULT_MANIFEST
            ).read_text(encoding="utf-8")
        )
        gui_runtime_manifest_path = root / docs_gui_runtime.DEFAULT_MANIFEST
        gui_runtime_manifest_path.write_text(
            json.dumps(gui_runtime_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        gui_runtime_sources = gui_runtime_manifest["sources"]
        for source_key in ("gui_script", "input_script"):
            relative_path = gui_runtime_sources[source_key]
            source_path = BUILDTOOLS_DIR.parent / relative_path
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            target_path.write_text(
                source_path.read_text(encoding="utf-8"),
                encoding="utf-8",
            )
        gui_runtime_anchors: dict[str, set[str]] = {}
        for collection in (
            "lifecycle_rules",
            "layout_rules",
            "input_rules",
            "integration_rules",
            "validation_rules",
        ):
            for entry in gui_runtime_manifest[collection]:
                for source in entry["source"]:
                    gui_runtime_anchors.setdefault(source["path"], set()).update(
                        source["anchors"]
                    )
        for relative_path in (
            gui_runtime_sources["client_runtime"],
            gui_runtime_sources["tutorial"],
            "Source/Tests/README.md",
        ):
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            anchor_text = "\n".join(
                sorted(gui_runtime_anchors.get(relative_path, {"fixture"}))
            ) + "\n"
            if target_path.exists():
                existing = target_path.read_text(encoding="utf-8")
                if anchor_text not in existing:
                    target_path.write_text(
                        existing + "\n" + anchor_text,
                        encoding="utf-8",
                    )
            else:
                target_path.write_text(anchor_text, encoding="utf-8")
        (root / gui_runtime_sources["native_test_directory"]).mkdir(
            parents=True,
            exist_ok=True,
        )
        gui_runtime_model_path = root / docs_gui_runtime.DEFAULT_MODEL
        gui_runtime_model_path.write_text(
            docs_gui_runtime.render_gui_runtime_model(root),
            encoding="utf-8",
        )
        gui_runtime_model = json.loads(
            gui_runtime_model_path.read_text(encoding="utf-8")
        )
        for page_path, content in docs_gui_runtime.generate_reference_pages(
            gui_runtime_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        ai_control_manifest_source = (
            BUILDTOOLS_DIR.parent / docs_ai_control_protocol.DEFAULT_MANIFEST
        )
        ai_control_manifest = json.loads(
            ai_control_manifest_source.read_text(encoding="utf-8")
        )
        ai_control_manifest_path = root / docs_ai_control_protocol.DEFAULT_MANIFEST
        ai_control_manifest_path.parent.mkdir(parents=True, exist_ok=True)
        ai_control_manifest_path.write_text(
            json.dumps(ai_control_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        for relative_path in ai_control_manifest["sources"].values():
            source_path = BUILDTOOLS_DIR.parent / relative_path
            target_path = root / relative_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            target_path.write_text(source_path.read_text(encoding="utf-8"), encoding="utf-8")
        ai_control_model_path = root / docs_ai_control_protocol.DEFAULT_MODEL
        ai_control_model_path.parent.mkdir(parents=True, exist_ok=True)
        ai_control_model_path.write_text(
            docs_ai_control_protocol.render_ai_control_protocol_model(root),
            encoding="utf-8",
        )
        ai_control_model = json.loads(ai_control_model_path.read_text(encoding="utf-8"))
        for page_path, content in docs_ai_control_protocol.generate_reference_pages(
            ai_control_model
        ).items():
            output_path = root / page_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        diagram_document = next(iter(documents), "Docs/Guide.md")
        diagram_alt = (
            "Fixture diagram showing two checked documentation states connected "
            "by one deterministic dependency for accessibility validation."
        )
        diagram_caption = (
            "Fixture caption explains that source-owned diagram metadata, rendered "
            "SVG bytes, and owning documentation must remain synchronized."
        )
        diagram_manifest = {
            "schema_version": docs_diagrams.SCHEMA_VERSION,
            "title": "Fixture documentation diagrams",
            "diagrams": [
                {
                    "id": diagram_id,
                    "title": f"Fixture {diagram_id}",
                    "description": (
                        "A deterministic fixture diagram for aggregate documentation "
                        "validation."
                    ),
                    "owning_document": diagram_document,
                    "alt": diagram_alt,
                    "caption": diagram_caption,
                    "width": 640,
                    "height": 360,
                    "source_paths": ["Source/example.txt", "CNAME"],
                    "bands": [
                        {
                            "x": 20,
                            "y": 100,
                            "width": 600,
                            "height": 200,
                            "title": "Fixture boundary",
                            "role": "neutral",
                        }
                    ],
                    "nodes": [
                        {
                            "id": "source",
                            "x": 60,
                            "y": 160,
                            "width": 200,
                            "height": 90,
                            "role": "project",
                            "title": "Source",
                            "lines": ["fixture input"],
                        },
                        {
                            "id": "output",
                            "x": 380,
                            "y": 160,
                            "width": 200,
                            "height": 90,
                            "role": "delivery",
                            "title": "Output",
                            "lines": ["fixture artifact"],
                        },
                    ],
                    "edges": [{"from": "source", "to": "output"}],
                }
                for diagram_id in docs_diagrams.DIAGRAM_IDS
            ],
        }
        diagram_manifest_path = root / docs_diagrams.DEFAULT_MANIFEST
        diagram_manifest_path.write_text(
            json.dumps(diagram_manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        diagram_document_path = root / diagram_document
        diagram_output_directory = PurePosixPath(
            posixpath.relpath(
                docs_diagrams.DEFAULT_OUTPUT_DIR,
                start=PurePosixPath(diagram_document).parent.as_posix(),
            )
        )
        diagram_markup = "".join(
            (
                "\n<figure class=\"docs-diagram\">\n"
                "<picture>\n"
                '<source media="(max-width: 700px)" '
                f'srcset="{diagram_output_directory}/{diagram_id}-mobile.svg">\n'
                f'<img src="{diagram_output_directory}/{diagram_id}.svg" '
                f'alt="{diagram_alt}" loading="lazy">\n'
                "</picture>\n"
                f"<figcaption>{diagram_caption}</figcaption>\n"
                "</figure>\n"
            )
            for diagram_id in docs_diagrams.DIAGRAM_IDS
        )
        diagram_document_path.write_text(
            diagram_document_path.read_text(encoding="utf-8") + diagram_markup,
            encoding="utf-8",
        )
        for relative_path, content in docs_diagrams.render_outputs(root).items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")

        screenshot_bytes = base64.b64decode(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk"
            "+A8AAQUBAScY42YAAAAASUVORK5CYII="
        )
        screenshot_sha = hashlib.sha256(screenshot_bytes).hexdigest()
        screenshot_records = []
        screenshot_markup = []
        screenshot_output_directory = PurePosixPath(
            posixpath.relpath(
                docs_screenshots.DEFAULT_OUTPUT_DIR,
                start=PurePosixPath(diagram_document).parent.as_posix(),
            )
        )
        for screenshot_id in docs_screenshots.SCREENSHOT_IDS:
            screenshot_path = (
                f"{docs_screenshots.DEFAULT_OUTPUT_DIR}/{screenshot_id}.png"
            )
            output_path = root / screenshot_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(screenshot_bytes)
            screenshot_alt = (
                f"Fixture {screenshot_id} screenshot showing the complete checked "
                "documentation tool state and its deterministic one-pixel image."
            )
            screenshot_caption = (
                f"Fixture {screenshot_id} caption binds the owning document, image "
                "bytes, capture metadata, source provenance, and recapture policy."
            )
            screenshot_records.append(
                {
                    "id": screenshot_id,
                    "title": f"Fixture {screenshot_id}",
                    "path": screenshot_path,
                    "owning_document": diagram_document,
                    "alt": screenshot_alt,
                    "caption": screenshot_caption,
                    "width": 1,
                    "height": 1,
                    "sha256": screenshot_sha,
                    "captured_on": "2026-07-31",
                    "license": "MIT",
                    "capture": {
                        "host": "Fixture host",
                        "renderer": "Fixture renderer",
                        "project": "Fixture project",
                        "viewport": "1x1",
                        "command": "fixture capture",
                        "interaction_steps": [
                            "Prepare the deterministic fixture.",
                            "Capture the declared tool state.",
                            "Retain the original PNG bytes.",
                        ],
                    },
                    "source_paths": [
                        "Source/example.txt",
                        "CNAME",
                        "_layouts/default.html",
                    ],
                    "recapture_triggers": [
                        "The fixture source changes.",
                        "The fixture capture contract changes.",
                        "The fixture owning markup changes.",
                    ],
                }
            )
            screenshot_markup.append(
                "\n<figure>\n"
                f'<img src="{screenshot_output_directory}/{screenshot_id}.png" '
                f'alt="{screenshot_alt}" loading="lazy">\n'
                f"<figcaption>{screenshot_caption}</figcaption>\n"
                "</figure>\n"
            )
        screenshot_manifest_path = root / docs_screenshots.DEFAULT_MANIFEST
        screenshot_manifest_path.write_text(
            json.dumps(
                {
                    "schema_version": docs_screenshots.SCHEMA_VERSION,
                    "title": "Fixture documentation screenshots",
                    "engine_base_revision": "0" * 40,
                    "revision_policy": (
                        "The fixture revision and source hashes identify the "
                        "deterministic validation capture."
                    ),
                    "screenshots": screenshot_records,
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        diagram_document_path.write_text(
            diagram_document_path.read_text(encoding="utf-8")
            + "".join(screenshot_markup),
            encoding="utf-8",
        )
        for relative_path, content in docs_screenshots.render_outputs(root).items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")

        generated_path.write_text(docs_inventory.render_inventory(root), encoding="utf-8")
        api_path.write_text(docs_api.render_api_model(root), encoding="utf-8")
        for reference_path, content in docs_reference.render_reference_pages(root).items():
            output_path = root / reference_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8")
        dispositions_path = root / docs_contract_diff.DEFAULT_DISPOSITIONS
        dispositions_path.write_text(
            json.dumps(
                {
                    "schema_version": docs_api_diff.DISPOSITIONS_SCHEMA_VERSION,
                    "entries": [],
                }
            )
            + "\n",
            encoding="utf-8",
        )
        generated_reference_modules = (
            docs_cmake,
            docs_cli,
            docs_helper_cli,
            docs_native_extension,
            docs_prototype_format,
            docs_map_format,
            docs_model_format,
            docs_text_format,
            docs_effect_format,
            docs_image_format,
            docs_particle_format,
            docs_font_format,
            docs_audio,
            docs_video,
            docs_gui_runtime,
            docs_ai_control_protocol,
            docs_package,
            docs_examples,
            docs_support_matrix,
        )
        inventory_excludes = manifest["inventory"]["exclude"]
        assert isinstance(inventory_excludes, list)
        generated_russian_paths = {
            path
            for module in generated_reference_modules
            for path in module.OUTPUT_PATHS
            if path.startswith("Docs/ru/")
        }
        inventory_excludes.extend(
            sorted(generated_russian_paths - set(inventory_excludes))
        )
        (root / docs_validate.DEFAULT_MANIFEST).write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        snippet_report = docs_snippets.evaluate(root)
        (root / docs_snippets.DEFAULT_OUTPUT).write_text(
            json.dumps(snippet_report, indent=2, ensure_ascii=True) + "\n",
            encoding="utf-8",
        )
        (root / docs_localization.DEFAULT_OUTPUT).write_text(
            docs_localization.render_localization_status(root),
            encoding="utf-8",
        )
        (root / docs_description_translations.DEFAULT_OUTPUT).write_text(
            docs_description_translations.render_status(root),
            encoding="utf-8",
        )
        for relative_path, content in docs_site.render_outputs(root).items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        primary_document_id = start_document_ids[0] if start_document_ids else "missing"
        primary_path = next(
            (
                path
                for path, document in documents.items()
                if document.get("id") == primary_document_id
            ),
            "Docs/Guide.md",
        )
        primary_file = root / primary_path
        primary_anchors = (
            sorted(docs_validate._markdown_anchors(primary_file))
            if primary_file.is_file()
            else ["missing"]
        )
        primary_anchor = primary_anchors[0] if primary_anchors else "missing"
        primary_term = (
            primary_file.read_text(encoding="utf-8").splitlines()[0].lstrip("# ").strip()
            if primary_file.is_file()
            else "missing"
        )
        evaluation_tasks = []
        for category in docs_ai_eval.REQUIRED_CATEGORIES:
            for task_number in range(2):
                evaluation_tasks.append(
                    {
                        "id": f"{category}-task-{task_number + 1}",
                        "category": category,
                        "question": f"How does the guide cover {category}?",
                        "primary_document_id": primary_document_id,
                        "supporting_document_ids": [],
                        "retrieval_checks": [
                            {
                                "query": primary_document_id,
                                "expected_document_ids": [primary_document_id],
                                "max_rank": 1,
                            },
                            {
                                "query": f"{primary_document_id} absent",
                                "expected_document_ids": [primary_document_id],
                                "max_rank": 1,
                            },
                        ],
                        "answer_checks": [
                            {
                                "id": "owning-evidence",
                                "description": "Use the owning document.",
                                "document_id": primary_document_id,
                                "anchor": primary_anchor,
                                "required_terms": [primary_term],
                            },
                            {
                                "id": "current-evidence",
                                "description": "Use current evidence.",
                                "document_id": primary_document_id,
                                "anchor": primary_anchor,
                                "required_terms": [primary_term],
                            },
                        ],
                        "forbidden_assumptions": [],
                    }
                )
        (root / docs_ai_eval.DEFAULT_SOURCE).write_text(
            json.dumps(
                {
                    "schema_version": docs_ai_eval.SCHEMA_VERSION,
                    "source_ref": "master",
                    "minimum_retrieval_success_rate": 1.0,
                    "categories": [
                        {"id": category, "title": category.title()}
                        for category in docs_ai_eval.REQUIRED_CATEGORIES
                    ],
                    "tasks": evaluation_tasks,
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        evaluation_report = docs_ai_eval.evaluate(root)
        (root / docs_ai_eval.DEFAULT_OUTPUT).write_text(
            json.dumps(evaluation_report, indent=2, ensure_ascii=True) + "\n",
            encoding="utf-8",
        )
        for relative_path, content in docs_ai_delivery.render_outputs(root).items():
            (root / relative_path).write_text(content, encoding="utf-8", newline="\n")

    def _document(self, document_id: str = "guide") -> dict[str, object]:
        return {
            "id": document_id,
            "title": "Guide",
            "audiences": ["game-developer", "ai-agent"],
            "classification": {
                "diataxis": "how-to",
                "visibility": "public",
                "human": True,
                "translation": "required",
            },
            "owner": "documentation",
            "state": "current",
            "disposition": "migrate",
            "target": "Docs/en/guide.md",
            "sources": ["Source/example.txt"],
        }

    def test_valid_manifest_links_and_inline_code_heading_pass(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text(
            "# Guide\n\nSee [the verify contract](#the-verify-macro) and [source](../Source/example.txt).\n\n"
            "## The `verify` macro\n",
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, count = docs_validate.validate_documentation(root)

        self.assertEqual(errors, [])
        self.assertEqual(count, 1)

    def test_link_outside_engine_root_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n\n[Project](../../Project.md)\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertTrue(any("escapes the engine root" in error for error in errors))

    def test_unclassified_markdown_file_fails_inventory(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        (root / "Docs/Extra.md").write_text("# Extra\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn("documentation inventory is missing a manifest entry: Docs/Extra.md", errors)

    def test_current_russian_variant_uses_canonical_manifest_entry(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        english_path = "Docs/en/guide.md"
        russian_path = "Docs/ru/guide.md"
        english_text = "# Guide\n"
        source = root / english_path
        source.parent.mkdir(parents=True)
        source.write_text(english_text, encoding="utf-8")
        document = self._document()
        document["target"] = english_path
        document["disposition"] = "retain"
        self._write_manifest(root, {english_path: document})
        translation = root / russian_path
        translation.parent.mkdir(parents=True, exist_ok=True)
        translation.write_text(
            docs_localization.translation_metadata_line(
                "guide",
                english_path,
                docs_localization.normalized_sha256(
                    source.read_text(encoding="utf-8")
                ),
            )
            + "\n\n# Руководство\n",
            encoding="utf-8",
        )
        (root / docs_localization.DEFAULT_OUTPUT).write_text(
            docs_localization.render_localization_status(root),
            encoding="utf-8",
        )
        for relative_path, content in docs_site.render_outputs(root).items():
            output_path = root / relative_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(content, encoding="utf-8", newline="\n")
        for relative_path, content in docs_ai_delivery.render_outputs(root).items():
            (root / relative_path).write_text(content, encoding="utf-8", newline="\n")

        errors, count = docs_validate.validate_documentation(root)

        self.assertNotIn(
            "documentation inventory is missing a manifest entry: Docs/ru/guide.md",
            errors,
        )
        self.assertEqual(count, 1)

    def test_missing_markdown_anchor_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n\n[Missing](#not-here)\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertTrue(any("Markdown anchor does not exist" in error for error in errors))

    def test_missing_manifest_source_path_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        document = self._document()
        document["sources"] = ["Source/missing.txt"]
        self._write_manifest(root, {"Docs/Guide.md": document})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn("manifest entry Docs/Guide.md source path does not exist: Source/missing.txt", errors)

    def test_cname_must_match_publishing_domain(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        (root / "CNAME").write_text("docs.example.com\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn("GitHub Pages CNAME does not match manifest domain: fonline.ru", errors)

    def test_pages_gem_pin_must_match_manifest(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        (root / "Gemfile").write_text('gem "github-pages", "= 231"\n', encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn("GitHub Pages Gemfile pin does not match the publishing manifest", errors)

    def test_version_source_ref_must_match_ai_delivery(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        manifest_path = root / docs_validate.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["versioning"]["current"]["source_ref"] = "stable"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertTrue(
            any(
                "ai_delivery source_ref must match documentation versioning current" in error
                for error in errors
            )
        )

    def test_translation_scoped_entrypoint_requires_locale_targets(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        manifest_path = root / docs_validate.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["documents"]["Docs/Guide.md"]["target"] = "Docs/Guide.md"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertTrue(
            any(
                "translation-required document guide must target Docs/en/ or declare "
                "explicit entrypoint targets" in error
                for error in errors
            )
        )

    def test_pages_workflow_must_build_and_upload_site(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        (root / ".github/workflows/validate.yml").write_text("jobs: {}\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "GitHub Pages validation workflow is missing: uses: actions/jekyll-build-pages@v1",
            errors,
        )

    def test_pages_public_readmes_must_pin_manifest_routes(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        readme_path = root / "Source/README.md"
        readme_path.write_text(
            "---\nlocale: en\n---\n\n# Source\n",
            encoding="utf-8",
        )
        source_document = self._document("source-readme")
        source_document["classification"]["translation"] = "not-required"
        source_document["target"] = "Source/README.md"
        self._write_manifest(
            root,
            {
                "Docs/Guide.md": self._document(),
                "Source/README.md": source_document,
            },
        )

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "GitHub Pages public README must pin permalink /Source/README.html: "
            "Source/README.md",
            errors,
        )

    def test_pages_workflow_must_validate_the_rendered_artifact(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/docs_site_artifact.py --site-dir _site "
                "--json-output Workspace/docs-site-artifact-report.json\n",
                "",
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "GitHub Pages validation workflow is missing: BuildTools/docs_site_artifact.py",
            errors,
        )

    def test_script_lifecycle_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_script_lifecycle.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation script lifecycle workflow is missing: "
            "BuildTools/tests/test_docs_script_lifecycle.py",
            errors,
        )

    def test_model_animation_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_model_animation.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation model animation workflow is missing: "
            "BuildTools/tests/test_docs_model_animation.py",
            errors,
        )

    def test_sprite_root_motion_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_sprite_root_motion.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation sprite root motion workflow is missing: "
            "BuildTools/tests/test_docs_sprite_root_motion.py",
            errors,
        )

    def test_android_debugging_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_android_debugging.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation Android debugging workflow is missing: "
            "BuildTools/tests/test_docs_android_debugging.py",
            errors,
        )

    def test_web_debugging_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_web_debugging.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation Web debugging workflow is missing: "
            "BuildTools/tests/test_docs_web_debugging.py",
            errors,
        )

    def test_native_and_script_debugging_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_debugging.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation native and AngelScript debugging workflow is missing: "
            "BuildTools/tests/test_docs_debugging.py",
            errors,
        )

    def test_angelscript_style_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_angelscript_style.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation AngelScript style workflow is missing: "
            "BuildTools/tests/test_docs_angelscript_style.py",
            errors,
        )

    def test_viewer_tools_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_viewer_tools.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation viewer tools workflow is missing: "
            "BuildTools/tests/test_docs_viewer_tools.py",
            errors,
        )

    def test_mapper_tools_source_check_must_run_in_workflow(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace(
                "    - run: python3 BuildTools/tests/test_docs_mapper_tools.py\n", ""
            ),
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "documentation Mapper tools workflow is missing: "
            "BuildTools/tests/test_docs_mapper_tools.py",
            errors,
        )

    def test_verified_pages_source_requires_branch_and_folder(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        manifest_path = root / docs_validate.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["publishing"]["source"]["status"] = "verified"
        manifest["publishing"]["source"]["build_type"] = "legacy"
        manifest["publishing"]["source"]["verified_on"] = "2026-08-02"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn("verified documentation publishing source must name a branch", errors)
        self.assertIn("verified documentation publishing source must name a folder", errors)

    def test_stale_generated_inventory_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / "Source/Tests/Test_New.cpp").write_text("// new test\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation inventory is stale; run python BuildTools/docs_inventory.py --write",
            errors,
        )

    def test_stale_ai_delivery_artifact_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / docs_ai_delivery.DEFAULT_LLMS_OUTPUT).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation AI artifact is stale: llms.txt; "
            "run python BuildTools/docs_ai_delivery.py --write",
            errors,
        )

    def test_stale_snippet_report_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text(
            "# Guide\n\n```json\n{\"ok\": true}\n```\n",
            encoding="utf-8",
        )
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / docs_snippets.DEFAULT_OUTPUT).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation snippet report is stale; "
            "run python BuildTools/docs_snippets.py --write",
            errors,
        )

    def test_stale_documentation_diagram_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = root / docs_diagrams.OUTPUT_PATHS[1]
        stale_path.write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation diagram artifact is stale: "
            f"{docs_diagrams.OUTPUT_PATHS[1]}; run python "
            "BuildTools/docs_diagrams.py --write",
            errors,
        )

    def test_stale_site_delivery_artifact_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / docs_site.DEFAULT_SEARCH_OUTPUT).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation site artifact is stale: assets/docs-search.json; "
            "run python BuildTools/docs_site.py --write",
            errors,
        )

    def test_stale_russian_search_artifact_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / docs_site.DEFAULT_RUSSIAN_SEARCH_OUTPUT).write_text(
            "stale\n",
            encoding="utf-8",
        )

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation site artifact is stale: "
            "assets/docs-search.ru.json; run python BuildTools/docs_site.py --write",
            errors,
        )

    def test_stale_route_catalog_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / docs_site.DEFAULT_ROUTES_OUTPUT).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation site artifact is stale: "
            "Docs/generated/document-routes.json; "
            "run python BuildTools/docs_site.py --write",
            errors,
        )

    def test_stale_generated_api_model_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / docs_api.DEFAULT_OUTPUT).write_text("{}\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation API model is stale; run python BuildTools/docs_api.py --write",
            errors,
        )

    def test_api_model_schema_version_must_match(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        manifest_path = root / docs_validate.DEFAULT_MANIFEST
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["generated_artifacts"]["api_model"]["schema_version"] = 1
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(f"documentation API model schema version must be {docs_api.SCHEMA_VERSION}", errors)

    def test_stale_generated_reference_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_reference.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation reference page is stale: {stale_path}; "
            "run python BuildTools/docs_reference.py --write",
            errors,
        )

    def test_stale_generated_cmake_model_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / docs_cmake.DEFAULT_MODEL).write_text("{}\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "generated documentation CMake model is stale; run python BuildTools/docs_cmake.py --write",
            errors,
        )

    def test_stale_generated_cmake_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_cmake.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation CMake page is stale: {stale_path}; "
            "run python BuildTools/docs_cmake.py --write",
            errors,
        )

    def test_stale_generated_cli_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_cli.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation BuildTools CLI page is stale: {stale_path}; "
            "run python BuildTools/docs_cli.py --write",
            errors,
        )

    def test_stale_generated_helper_cli_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_helper_cli.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation helper CLI page is stale: {stale_path}; "
            "run python BuildTools/docs_helper_cli.py --write",
            errors,
        )

    def test_stale_generated_native_extension_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_native_extension.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation native extension page is stale: {stale_path}; "
            "run python BuildTools/docs_native_extension.py --write",
            errors,
        )

    def test_stale_generated_prototype_format_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_prototype_format.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation prototype format page is stale: {stale_path}; "
            "run python BuildTools/docs_prototype_format.py --write",
            errors,
        )

    def test_stale_generated_map_format_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_map_format.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation map format page is stale: {stale_path}; "
            "run python BuildTools/docs_map_format.py --write",
            errors,
        )

    def test_stale_generated_model_format_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_model_format.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation model format page is stale: {stale_path}; "
            "run python BuildTools/docs_model_format.py --write",
            errors,
        )

    def test_stale_generated_text_format_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_text_format.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation text format page is stale: {stale_path}; "
            "run python BuildTools/docs_text_format.py --write",
            errors,
        )

    def test_stale_generated_effect_format_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_effect_format.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation effect format page is stale: {stale_path}; "
            "run python BuildTools/docs_effect_format.py --write",
            errors,
        )

    def test_stale_generated_package_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_package.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated documentation package page is stale: {stale_path}; "
            "run python BuildTools/docs_package.py --write",
            errors,
        )

    def test_stale_generated_public_examples_page_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        stale_path = docs_examples.OUTPUT_PATHS[0]
        (root / stale_path).write_text("stale\n", encoding="utf-8")

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            f"generated public example repository artifact is stale: {stale_path}; "
            "run python BuildTools/docs_examples.py --write",
            errors,
        )

    def test_invalid_contract_change_dispositions_fail(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        (root / docs_contract_diff.DEFAULT_DISPOSITIONS).write_text(
            '{"schema_version": 1, "entries": [{}]}\n', encoding="utf-8"
        )

        errors, _ = docs_validate.validate_documentation(root)

        self.assertTrue(
            any(error.startswith("invalid documentation contract change dispositions:") for error in errors)
        )

    def test_missing_contract_diff_workflow_gate_fails(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        self._write_manifest(root, {"Docs/Guide.md": self._document()})
        workflow_path = root / ".github/workflows/validate.yml"
        workflow_path.write_text(
            workflow_path.read_text(encoding="utf-8").replace("--enforce", ""),
            encoding="utf-8",
        )

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn("documentation contract diff workflow is missing: --enforce", errors)

    def test_placeholder_requires_visible_route_notice(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n\nWork in progress.\n", encoding="utf-8")
        document = self._document()
        document["state"] = "placeholder"
        self._write_manifest(root, {"Docs/Guide.md": document})

        errors, _ = docs_validate.validate_documentation(root)

        self.assertIn(
            "manifest entry Docs/Guide.md placeholder must declare a visible '> Placeholder route.' notice",
            errors,
        )

    def test_legacy_redirect_is_public_nonhuman_and_targets_canonical_owner(self) -> None:
        temporary_directory, root = self._create_tree()
        self.addCleanup(temporary_directory.cleanup)
        (root / "Docs/Guide.md").write_text("# Guide\n", encoding="utf-8")
        (root / "Docs/Legacy.md").write_text(
            "> Legacy route.\n\n# Legacy guide\n\nSee [Guide](Guide.md).\n",
            encoding="utf-8",
        )
        canonical = self._document("guide")
        redirect = self._document("legacy-guide")
        redirect["classification"] = {
            "diataxis": "none",
            "visibility": "public",
            "human": False,
            "translation": "not-required",
        }
        redirect["state"] = "redirect"
        redirect["disposition"] = "replace"
        redirect["target"] = canonical["target"]
        redirect["redirect_to"] = "guide"
        self._write_manifest(
            root,
            {
                "Docs/Guide.md": canonical,
                "Docs/Legacy.md": redirect,
            },
        )

        errors, _ = docs_validate.validate_documentation(root)
        self.assertEqual(errors, [])

        (root / "Docs/Legacy.md").write_text("# Legacy guide\n", encoding="utf-8")
        errors, _ = docs_validate.validate_documentation(root)
        self.assertIn(
            "manifest entry Docs/Legacy.md redirect must declare a visible '> Legacy route.' notice",
            errors,
        )

        (root / "Docs/Legacy.md").write_text(
            "> Legacy route.\n\n# Legacy guide\n\nSee [Legacy](Legacy.md).\n",
            encoding="utf-8",
        )
        errors, _ = docs_validate.validate_documentation(root)
        self.assertIn(
            "manifest entry Docs/Legacy.md redirect must link to canonical document Docs/Guide.md",
            errors,
        )


if __name__ == "__main__":
    unittest.main()
