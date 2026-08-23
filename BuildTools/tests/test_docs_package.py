from __future__ import annotations

import copy
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
BUILDTOOLS_DIR = ENGINE_ROOT / "BuildTools"
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_package  # noqa: E402
import docs_localization  # noqa: E402
import package as package_tool  # noqa: E402


def _manifest() -> dict[str, object]:
    return json.loads((ENGINE_ROOT / docs_package.DEFAULT_MANIFEST).read_text(encoding="utf-8"))


def _write_fixture(root: Path, manifest: dict[str, object]) -> None:
    manifest_path = root / docs_package.DEFAULT_MANIFEST
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    translation_catalog = root / "Docs/description-translations.ru.json"
    translation_catalog.parent.mkdir(parents=True, exist_ok=True)
    translation_catalog.write_text(
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

    declaration = manifest["declaration"]
    for source in (declaration["source"], declaration["consumer"]):
        source_path = root / source
        source_path.parent.mkdir(parents=True, exist_ok=True)
        source_path.write_text("# fixture\n", encoding="utf-8")

    targets = [entry["name"] for entry in manifest["targets"]]
    platforms = [entry["name"] for entry in manifest["platforms"]]
    parser_source = root / manifest["packager"]["source"]
    parser_source.parent.mkdir(parents=True, exist_ok=True)
    parser_source.write_text(
        "import argparse\n\n"
        "def create_parser():\n"
        "    parser = argparse.ArgumentParser(description='Fixture packager')\n"
        f"    parser.add_argument('-target', choices={targets!r}, required=True)\n"
        f"    parser.add_argument('-platform', choices={platforms!r}, required=True)\n"
        "    parser.add_argument('-pack', required=True)\n"
        "    return parser\n",
        encoding="utf-8",
    )


class DocumentationPackageTests(unittest.TestCase):
    def test_runtime_payload_postfix_accepts_package_and_cmake_variants(self) -> None:
        extract = package_tool.Packager.extract_binary_entry_postfix

        self.assertEqual(extract("Client-Linux-x64"), "")
        self.assertEqual(extract("Client-Linux-x64-Profiling_OnDemand"), "")
        self.assertEqual(extract("Client-Linux-x64-Profiling_OnDemand-Debug-Steam"), "Steam")

    def test_runtime_payload_postfix_rejects_unrelated_or_malformed_entries(self) -> None:
        extract = package_tool.Packager.extract_binary_entry_postfix

        self.assertIsNone(extract("Server-Linux-x64"))
        self.assertIsNone(extract("Client-Unknown-x64"))
        with self.assertRaisesRegex(AssertionError, "Unexpected binary entry layout"):
            extract("Client-Linux-x64-Debug_Profiling_OnDemand_bad")

    def test_current_model_has_stable_shape_ids_and_exact_help(self) -> None:
        model = docs_package.generate_package_model(ENGINE_ROOT)

        self.assertEqual(model["schema_version"], 1)
        self.assertEqual(model["generated_by"], "BuildTools/docs_package.py")
        self.assertEqual(model["summary"]["clause_count"], 2)
        self.assertEqual(model["summary"]["option_count"], 1)
        self.assertEqual(model["summary"]["target_count"], 6)
        self.assertEqual(model["summary"]["platform_count"], 6)
        self.assertEqual(model["summary"]["implemented_platform_count"], 4)
        self.assertEqual(model["summary"]["pack_count"], 19)
        self.assertEqual(model["summary"]["implemented_pack_count"], 18)
        self.assertEqual(model["summary"]["artifact_pack_count"], 8)
        self.assertEqual(model["summary"]["cli_argument_count"], 13)
        self.assertEqual(
            [entry["name"] for entry in model["targets"]],
            ["Server", "Client", "Mapper", "Baker", "AnimationViewer", "ParticleViewer"],
        )
        self.assertEqual([entry["name"] for entry in model["declaration"]["options"]], ["POSTFIX"])
        self.assertIn("win32-win7", model["platforms"][0]["architectures"])
        self.assertIn("win64-win7", model["platforms"][0]["architectures"])

        identities = [model["declaration"]["id"]]
        identities.extend(entry["id"] for entry in model["declaration"]["clauses"])
        identities.extend(entry["id"] for entry in model["declaration"]["options"])
        identities.extend(
            entry["id"]
            for key in ("targets", "platforms", "packs", "payloads")
            for entry in model[key]
        )
        identities.extend(entry["id"] for entry in model["cli"]["arguments"])
        self.assertEqual(len(identities), len(set(identities)))

        environment = dict(os.environ)
        environment["COLUMNS"] = str(docs_package.HELP_COLUMNS)
        result = subprocess.run(
            [sys.executable, str(ENGINE_ROOT / docs_package.DEFAULT_SOURCE), "--help"],
            cwd=ENGINE_ROOT,
            env=environment,
            capture_output=True,
            check=True,
            text=True,
        )
        self.assertEqual(result.stdout.replace("\r\n", "\n"), model["cli"]["help_output"])

    def test_manifest_validation_rejects_contract_drift(self) -> None:
        cases: list[tuple[dict[str, object], str]] = []

        duplicate_pack = _manifest()
        duplicate_pack["packs"][1]["name"] = duplicate_pack["packs"][0]["name"]
        cases.append((duplicate_pack, "duplicate packs name"))

        unknown_target = _manifest()
        unknown_target["platforms"][0]["targets"].append("Missing")
        cases.append((unknown_target, "unknown target"))

        missing_clause = _manifest()
        missing_clause["declaration"]["clauses"].pop()
        cases.append((missing_clause, "must be CONFIG and BINARY"))

        for manifest, expected in cases:
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as temp_dir:
                root = Path(temp_dir)
                _write_fixture(root, manifest)
                with self.assertRaisesRegex(ValueError, expected):
                    docs_package.generate_package_model(root)

    def test_reference_pages_are_deterministic_escaped_and_cover_ids(self) -> None:
        model = docs_package.generate_package_model(ENGINE_ROOT)
        pages = docs_package.generate_reference_pages(model)

        self.assertEqual(tuple(sorted(pages)), tuple(sorted(docs_package.OUTPUT_PATHS)))
        self.assertEqual(pages, docs_package.generate_reference_pages(copy.deepcopy(model)))
        combined = "\n".join(pages.values())
        for entry in (*model["declaration"]["clauses"], *model["declaration"]["options"]):
            self.assertIn(entry["id"], combined)
            self.assertIn(docs_package._anchor(entry["id"]), combined)
        for key in ("targets", "platforms", "packs", "payloads"):
            for entry in model[key]:
                self.assertIn(entry["id"], combined)
                self.assertIn(docs_package._anchor(entry["id"]), combined)

        escaped = copy.deepcopy(model)
        escaped["packs"][0]["description"] = "A | B {shape} <unsafe>"
        matrix = docs_package.generate_reference_pages(escaped)[f"{docs_package.DEFAULT_OUTPUT_DIR}/matrix.md"]
        self.assertIn("A &#124; B &#123;shape&#125; &lt;unsafe&gt;", matrix)

    def test_reference_pages_have_canonical_russian_and_legacy_routes(self) -> None:
        pages = docs_package.generate_reference_pages(
            docs_package.generate_package_model(ENGINE_ROOT)
        )
        self.assertEqual(
            set(pages),
            set(docs_package.CANONICAL_OUTPUT_PATHS)
            | set(docs_package.RUSSIAN_OUTPUT_PATHS)
            | set(docs_package.LEGACY_OUTPUT_PATHS),
        )
        for filename, document_id, _ in docs_package.PAGE_DEFINITIONS:
            canonical_path = f"{docs_package.DEFAULT_OUTPUT_DIR}/{filename}"
            russian_path = f"{docs_package.RUSSIAN_OUTPUT_DIR}/{filename}"
            legacy_path = f"{docs_package.LEGACY_OUTPUT_DIR}/{filename}"
            english = pages[canonical_path]
            russian = pages[russian_path]
            legacy = pages[legacy_path]
            marker = docs_localization.translation_metadata_line(
                document_id,
                canonical_path,
                docs_localization.normalized_sha256(english),
            )
            self.assertIn("locale: en", english)
            self.assertIn("locale: ru", russian)
            self.assertIn(marker, russian)
            self.assertEqual(
                re.findall(r"^#{2,3} ", english, re.MULTILINE),
                re.findall(r"^#{2,3} ", russian, re.MULTILINE),
            )
            self.assertIn(f"../../en/reference/packages/{filename}", legacy)
            self.assertIn(f"../../ru/reference/packages/{filename}", legacy)

    def test_manifest_assigns_stable_ids_to_canonical_package_pages(self) -> None:
        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        for filename, document_id, _ in docs_package.PAGE_DEFINITIONS:
            canonical = documents[f"{docs_package.DEFAULT_OUTPUT_DIR}/{filename}"]
            legacy = documents[f"{docs_package.LEGACY_OUTPUT_DIR}/{filename}"]
            self.assertEqual(canonical["id"], document_id)
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertNotEqual(legacy["id"], document_id)

    def test_release_guide_preserves_capability_and_project_boundaries(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/how-to/release/packaging.md").read_text(encoding="utf-8")

        for heading in (
            "## Know what the Engine proves",
            "## Build, bake, then package",
            "## Reproducibility and provenance",
            "## Signing and secret boundaries",
            "## Acceptance matrix",
            "## Release checklist",
        ):
            self.assertIn(heading, guide)
        for required_contract in (
            "ForceBakeResources",
            "MakePackage-<package-id>",
            "Packaging.CodeSigningHook",
            "full SHAs",
            "SHA-256",
            "project-qualified",
            "`package.py` currently aborts for both `macOS` and `iOS`",
            "`Service` and `Daemon` are binary variants, not deployment systems",
        ):
            self.assertIn(required_contract, guide)

    def test_cli_write_check_and_stale_detection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            _write_fixture(root, _manifest())
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                self.assertEqual(docs_package.main(["--root", str(root), "--write"]), 0)
                self.assertEqual(docs_package.main(["--root", str(root), "--check"]), 0)
                stale_page = root / docs_package.OUTPUT_PATHS[0]
                stale_page.write_text(stale_page.read_text(encoding="utf-8") + "stale\n", encoding="utf-8")
                self.assertEqual(docs_package.main(["--root", str(root), "--check"]), 1)
            self.assertIn("missing or stale", output.getvalue())


if __name__ == "__main__":
    unittest.main()
