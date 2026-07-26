from __future__ import annotations

import copy
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from types import SimpleNamespace


ENGINE_ROOT = Path(__file__).resolve().parents[2]
BUILDTOOLS_DIR = ENGINE_ROOT / "BuildTools"
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_package  # noqa: E402
import package as package_tool  # noqa: E402


def _manifest() -> dict[str, object]:
    return json.loads((ENGINE_ROOT / docs_package.DEFAULT_MANIFEST).read_text(encoding="utf-8"))


def _write_fixture(root: Path, manifest: dict[str, object]) -> None:
    manifest_path = root / docs_package.DEFAULT_MANIFEST
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

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


def _contract_packager(target: str, platform: str, arch: str, packs: set[str]) -> package_tool.Packager:
    packager = package_tool.Packager.__new__(package_tool.Packager)
    packager.args = SimpleNamespace(target=target, platform=platform, arch=arch)
    packager.pack_args = packs
    return packager


class DocumentationPackageTests(unittest.TestCase):
    def test_current_model_has_stable_shape_ids_and_exact_help(self) -> None:
        model = docs_package.generate_package_model(ENGINE_ROOT)

        self.assertEqual(model["schema_version"], 1)
        self.assertEqual(model["generated_by"], "BuildTools/docs_package.py")
        self.assertEqual(model["summary"]["clause_count"], 2)
        self.assertEqual(model["summary"]["option_count"], 1)
        self.assertEqual(model["summary"]["target_count"], 5)
        self.assertEqual(model["summary"]["platform_count"], 6)
        self.assertEqual(model["summary"]["implemented_platform_count"], 4)
        self.assertEqual(model["summary"]["pack_count"], 19)
        self.assertEqual(model["summary"]["implemented_pack_count"], 18)
        self.assertEqual(model["summary"]["artifact_pack_count"], 8)
        self.assertEqual(model["summary"]["cli_argument_count"], 13)
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
        environment["COLUMNS"] = "80"
        result = subprocess.run(
            [sys.executable, str(ENGINE_ROOT / docs_package.DEFAULT_SOURCE), "--help"],
            cwd=ENGINE_ROOT,
            env=environment,
            capture_output=True,
            check=True,
            text=True,
        )
        self.assertEqual(result.stdout.replace("\r\n", "\n"), model["cli"]["help_output"])

    def test_runtime_contract_accepts_implemented_packs_and_rejects_invalid_combinations(self) -> None:
        for pack_name, pack in package_tool.PACKAGE_PACKS.items():
            target = pack["targets"][0]
            platform = pack["platforms"][0]
            arch = package_tool.PACKAGE_PLATFORMS[platform]["architectures"][0]
            packs = {pack_name}
            if not pack["artifact"]:
                packs.add("Raw")
            if "NoRes" in package_tool.PACKAGE_TARGETS[target]["required_packs"]:
                packs.add("NoRes")
            packager = _contract_packager(target, platform, arch, packs)
            if pack["status"] == "implemented":
                with self.subTest(pack=pack_name):
                    packager.validate_package_contract()
            else:
                with self.subTest(pack=pack_name), self.assertRaisesRegex(AssertionError, "not implemented"):
                    packager.validate_package_contract()

        with self.assertRaisesRegex(AssertionError, "does not support target"):
            _contract_packager("Server", "Web", "wasm", {"Raw"}).validate_package_contract()
        with self.assertRaisesRegex(AssertionError, "requires pack token"):
            _contract_packager("Mapper", "Windows", "win64", {"Raw"}).validate_package_contract()
        with self.assertRaisesRegex(AssertionError, "at least one output artifact"):
            _contract_packager("Client", "Windows", "win64", {"OGL"}).validate_package_contract()
        _contract_packager("Client", "Windows", "win32-win7", {"Raw"}).validate_package_contract()
        _contract_packager("Client", "Windows", "win64-win7", {"Raw"}).validate_package_contract()

        args = SimpleNamespace(pack="Raw+Unknown", target="Client", platform="Windows", arch="win64")
        with self.assertRaisesRegex(AssertionError, "Unknown package pack token"):
            package_tool.Packager(args, None)

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
