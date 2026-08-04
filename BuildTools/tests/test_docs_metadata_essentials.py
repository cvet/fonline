from __future__ import annotations

import json
import re
import sys
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_contract_diff  # noqa: E402
import docs_localization  # noqa: E402


METADATA_PATH = "Docs/en/reference/metadata/index.md"
METADATA_RUSSIAN_PATH = "Docs/ru/reference/metadata/index.md"
METADATA_LEGACY_PATH = "Docs/GeneratedApiAndMetadata.md"
ESSENTIALS_PATH = "Docs/en/reference/native/essentials.md"
ESSENTIALS_RUSSIAN_PATH = "Docs/ru/reference/native/essentials.md"
ESSENTIALS_LEGACY_PATH = "Docs/Essentials.md"


class MetadataAndEssentialsDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def _manifest_documents(self) -> dict[str, object]:
        return json.loads(self._read("Docs/documentation-manifest.json"))["documents"]

    def _assert_owned_route(
        self,
        canonical_path: str,
        canonical_id: str,
        russian_path: str,
        legacy_path: str,
    ) -> None:
        documents = self._manifest_documents()
        canonical = documents[canonical_path]
        legacy = documents[legacy_path]

        self.assertEqual(
            (canonical["id"], canonical["state"], canonical["disposition"]),
            (canonical_id, "current", "retain"),
        )
        self.assertEqual(canonical["target"], canonical_path)
        self.assertEqual(canonical["classification"]["translation"], "required")
        self.assertIn("ai-agent", canonical["audiences"])
        self.assertEqual(
            (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
            ("redirect", "replace", canonical_id),
        )
        self.assertEqual(legacy["target"], canonical_path)
        self.assertTrue((ENGINE_ROOT / russian_path).is_file())

        canonical_text = self._read(canonical_path)
        russian_text = self._read(russian_path)
        legacy_text = self._read(legacy_path)
        canonical_permalink = (
            f"/{canonical_path.removesuffix('index.md')}"
            if canonical_path.endswith("/index.md")
            else f"/{canonical_path.removesuffix('.md')}.html"
        )
        russian_permalink = (
            f"/{russian_path.removesuffix('index.md')}"
            if russian_path.endswith("/index.md")
            else f"/{russian_path.removesuffix('.md')}.html"
        )
        self.assertIn(f"permalink: {canonical_permalink}", canonical_text)
        self.assertIn(f"permalink: {russian_permalink}", russian_text)
        for heading in re.findall(r"^#{2,3} .+$", canonical_text, re.MULTILINE):
            self.assertIn(heading, legacy_text)
        canonical_route = canonical_path.removeprefix("Docs/").removesuffix("index.md")
        russian_route = russian_path.removeprefix("Docs/").removesuffix("index.md")
        self.assertIn(canonical_route, legacy_text)
        self.assertIn(russian_route, legacy_text)

    def _assert_translation_current(self, english_path: str, russian_path: str) -> None:
        english = self._read(english_path)
        russian = self._read(russian_path)
        marker_match = re.search(
            r'<!-- docs-translation: (?P<marker>\{.*?\}) -->',
            russian,
        )
        self.assertIsNotNone(marker_match)
        marker = json.loads(marker_match.group("marker"))
        self.assertEqual(marker["source_path"], english_path)
        self.assertEqual(
            marker["source_sha256"],
            docs_localization.normalized_sha256(english),
        )
        self.assertEqual(
            len(re.findall(r"^#{2,3} ", russian, re.MULTILINE)),
            len(re.findall(r"^#{2,3} ", english, re.MULTILINE)),
        )

    def test_manifest_and_legacy_routes_are_complete(self) -> None:
        self._assert_owned_route(
            METADATA_PATH,
            "generated-api-metadata",
            METADATA_RUSSIAN_PATH,
            METADATA_LEGACY_PATH,
        )
        self._assert_owned_route(
            ESSENTIALS_PATH,
            "native-essentials",
            ESSENTIALS_RUSSIAN_PATH,
            ESSENTIALS_LEGACY_PATH,
        )

    def test_russian_mirrors_are_current_and_preserve_code(self) -> None:
        self._assert_translation_current(METADATA_PATH, METADATA_RUSSIAN_PATH)
        self._assert_translation_current(ESSENTIALS_PATH, ESSENTIALS_RUSSIAN_PATH)

        fence_pattern = re.compile(r"^```[^\n]*\n(?P<body>.*?)^```\s*$", re.MULTILINE | re.DOTALL)
        english_fences = [match.group("body") for match in fence_pattern.finditer(self._read(METADATA_PATH))]
        russian_fences = [match.group("body") for match in fence_pattern.finditer(self._read(METADATA_RUSSIAN_PATH))]
        self.assertTrue(english_fences)
        self.assertEqual(russian_fences, english_fences)

        essentials_russian = self._read(ESSENTIALS_RUSSIAN_PATH)
        for anchor in (
            "memory-pointers-and-lifetime-utilities",
            "third-party-allocators",
            "vector-containers-and-inline-storage",
        ):
            self.assertIn(f'<a id="{anchor}"></a>', essentials_russian)

    def test_essentials_dependency_order_matches_the_umbrella_header(self) -> None:
        umbrella = self._read("Source/Essentials/Essentials.h")
        guide = self._read(ESSENTIALS_PATH)
        include_order = re.findall(r'^#include "([A-Za-z0-9_.]+)"$', umbrella, re.MULTILINE)
        self.assertEqual(len(include_order), 23)

        section = guide.split("## Include and dependency model", 1)[1].split("\n## ", 1)[0]
        documented_order: list[str] = []
        for match in re.finditer(r"^\d+\. (?P<label>.*?) \u2014 ", section, re.MULTILINE):
            documented_order.extend(re.findall(r"`([A-Za-z0-9_.]+\.h)`", match.group("label")))
        self.assertEqual(documented_order, include_order)

    def test_essentials_source_inventory_matches_cmake_and_the_guide(self) -> None:
        cmake = self._read("BuildTools/cmake/stages/EngineSources.cmake")
        core_libs = self._read("BuildTools/cmake/stages/CoreLibs.cmake")
        guide = self._read(ESSENTIALS_PATH)
        authored_files = sorted(
            path.name
            for path in (ENGINE_ROOT / "Source/Essentials").iterdir()
            if path.is_file() and path.suffix in {".h", ".cpp", ".inc"}
        )
        self.assertTrue(authored_files)
        for name in authored_files:
            relative_path = f"Source/Essentials/{name}"
            self.assertIn(relative_path, cmake)
            self.assertIn(f"`{relative_path}`", guide)
        self.assertIn("AddCoreStaticLibrary(EssentialsLib FO_ESSENTIALS_SOURCE", core_libs)

    def test_metadata_guide_covers_every_contract_domain_and_generator(self) -> None:
        guide = self._read(METADATA_PATH)
        self.assertEqual(set(docs_contract_diff.MODEL_FILES), set(docs_contract_diff.DOMAIN_ORDER))
        for model_name in docs_contract_diff.MODEL_FILES.values():
            self.assertIn(f"generated/{model_name}", guide)

        for generator in (
            "docs_audio.py",
            "docs_video.py",
            "docs_gui_runtime.py",
            "docs_ai_control_protocol.py",
            "docs_public_api.py",
            "docs_support_matrix.py",
            "docs_localization.py",
            "docs_external_evidence.py",
            "docs_diagrams.py",
            "docs_screenshots.py",
        ):
            self.assertIn(f"`BuildTools/{generator}`", guide)

    def test_active_links_do_not_target_flat_legacy_routes(self) -> None:
        legacy_target = re.compile(
            r"\]\([^)]*(?:GeneratedApiAndMetadata|Essentials)\.md(?:#[^)]*)?\)"
        )
        paths = [ENGINE_ROOT / "README.md", ENGINE_ROOT / "README.ru.md", ENGINE_ROOT / "AGENTS.md"]
        documents = self._manifest_documents()
        paths.extend(
            ENGINE_ROOT / relative_path
            for relative_path, record in documents.items()
            if record.get("state") == "current"
            and record.get("classification", {}).get("human") is True
            and (ENGINE_ROOT / relative_path).is_file()
        )
        offenders = [
            str(path.relative_to(ENGINE_ROOT))
            for path in paths
            if legacy_target.search(path.read_text(encoding="utf-8"))
        ]
        self.assertEqual(offenders, [])

    def test_reference_generators_use_the_canonical_metadata_route(self) -> None:
        for path in (
            "BuildTools/docs_reference.py",
            "BuildTools/docs_cmake.py",
            "BuildTools/docs_cli.py",
            "BuildTools/docs_helper_cli.py",
        ):
            source = self._read(path)
            self.assertNotIn("GeneratedApiAndMetadata.md", source)
            self.assertIn("../metadata/", source)


if __name__ == "__main__":
    unittest.main()
