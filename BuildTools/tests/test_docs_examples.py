from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
BUILD_TOOLS = ENGINE_ROOT / "BuildTools"
import sys

sys.path.insert(0, str(BUILD_TOOLS))

import docs_examples


class PublicExampleDocumentationTests(unittest.TestCase):
    def make_fixture(self) -> Path:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        shutil.copytree(
            ENGINE_ROOT / "Examples" / "MinimalProject",
            root / "Examples" / "MinimalProject",
        )
        shutil.copytree(
            ENGINE_ROOT / "Examples" / "PublicRepositoryTemplate",
            root / "Examples" / "PublicRepositoryTemplate",
        )
        shutil.copyfile(
            ENGINE_ROOT / docs_examples.DEFAULT_MANIFEST,
            root / docs_examples.DEFAULT_MANIFEST,
        )
        return root

    def load_fixture_manifest(self, root: Path) -> dict[str, object]:
        return json.loads((root / docs_examples.DEFAULT_MANIFEST).read_text(encoding="utf-8"))

    def write_fixture_manifest(self, root: Path, manifest: dict[str, object]) -> None:
        (root / docs_examples.DEFAULT_MANIFEST).write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    def materialize_repository(self, root: Path) -> Path:
        model = docs_examples.generate_model(root)
        publication = model["program"]["publication"]
        template_root = root / publication["template_path"]
        repository_root = root / "published"
        replacements = {
            "{{CODEOWNER}}": "@cvet",
            "{{ENGINE_REVISION}}": "a" * 40,
            "{{PRIMARY_CHECK_COMMAND}}": "python run_starter_smoke.py",
            "{{REPOSITORY_ID}}": "project-template",
            "{{REPOSITORY_NAME}}": "FOnline Project Template",
            "{{REPOSITORY_SLUG}}": "fonline-project-template",
            "{{SUMMARY}}": "Minimal tested FOnline project.",
        }
        for source, destination in publication["copy_files"].items():
            source_path = template_root.joinpath(*Path(source).parts)
            destination_path = repository_root.joinpath(*Path(destination).parts)
            destination_path.parent.mkdir(parents=True, exist_ok=True)
            text = source_path.read_text(encoding="utf-8")
            for placeholder, value in replacements.items():
                text = text.replace(placeholder, value)
            destination_path.write_text(text, encoding="utf-8")
        return repository_root

    def test_real_registry_has_ordered_owned_portfolio(self) -> None:
        model = docs_examples.generate_model(ENGINE_ROOT)

        self.assertEqual(model["summary"]["repository_count"], 4)
        self.assertEqual(model["summary"]["source_ready_count"], 1)
        self.assertEqual(model["summary"]["published_count"], 0)
        self.assertEqual(model["summary"]["private_count"], 4)
        self.assertEqual(model["summary"]["source_staged_count"], 1)
        self.assertEqual(
            [repository["id"] for repository in model["repositories"]],
            ["project-template", "minimal-multiplayer", "content-showcase", "native-extension-sample"],
        )
        self.assertEqual(model["program"]["compatibility"]["release_engine_ref"], "exact-commit")
        self.assertEqual(
            set(model["program"]["compatibility"]["required_lanes"]),
            {"pinned-engine", "current-engine"},
        )
        self.assertRegex(model["contract_digest"], r"^[0-9a-f]{64}$")
        self.assertTrue(all("url" not in repository for repository in model["repositories"]))
        self.assertEqual(model["repositories"][0]["remote"]["state"], "source-staged")
        presets = json.loads((ENGINE_ROOT / "Examples/MinimalProject/CMakePresets.json").read_text(encoding="utf-8"))
        self.assertEqual({preset["name"] for preset in presets["configurePresets"]}, {"starter-base", "windows", "linux"})
        self.assertTrue(all(preset["targets"] == ["RunStarterSmoke"] for preset in presets["buildPresets"]))
        linux_preset = next(preset for preset in presets["configurePresets"] if preset["name"] == "linux")
        self.assertEqual(linux_preset["cacheVariables"]["CMAKE_C_COMPILER"], "gcc")
        self.assertEqual(linux_preset["cacheVariables"]["CMAKE_CXX_COMPILER"], "g++")
        validator = (ENGINE_ROOT / "Examples/MinimalProject/validate.py").read_text(encoding="utf-8")
        self.assertIn('["cmake", "--preset", preset]', validator)
        self.assertIn('["cmake", "--build", "--preset", f"{preset}-smoke"]', validator)
        smoke_runner = (ENGINE_ROOT / "Examples/MinimalProject/run_starter_smoke.py").read_text(encoding="utf-8")
        self.assertIn("def decode_metadata", smoke_runner)
        self.assertNotIn("from docs_metadata import", smoke_runner)
        starter_config = (ENGINE_ROOT / "Examples/MinimalProject/FOnlineStarter.fomain").read_text(encoding="utf-8")
        for setting in (
            "SpriteMesh.Enabled = False",
            "SpriteMesh.AlphaThreshold = 0",
            "SpriteMesh.MaxTriangles = 4096",
            "SpriteMesh.AreaSavingsWeight = 32.0",
        ):
            self.assertIn(setting, starter_config)
        pinned_workflow = (
            ENGINE_ROOT / "Examples/PublicRepositoryTemplate/.github/workflows/pinned-engine.template.yml"
        ).read_text(encoding="utf-8")
        current_workflow = (
            ENGINE_ROOT / "Examples/PublicRepositoryTemplate/.github/workflows/current-engine.template.yml"
        ).read_text(encoding="utf-8")
        for marker in ("actions/checkout@v6", "submodules: recursive", "--engine-mode pinned", "{{PRIMARY_CHECK_COMMAND}}"):
            self.assertIn(marker, pinned_workflow)
        for workflow in (pinned_workflow, current_workflow):
            self.assertIn("Engine/BuildTools/prepare-workspace.sh linux-packages linux", workflow)
        for marker in ("schedule:", "git -C Engine fetch --depth=1 origin master", "--engine-mode current"):
            self.assertIn(marker, current_workflow)

    def test_registry_rejects_floating_release_duplicate_and_bad_dependency(self) -> None:
        root = self.make_fixture()
        cases = []

        floating = self.load_fixture_manifest(root)
        floating["program"]["compatibility"]["release_engine_ref"] = "master"
        cases.append((floating, "release_engine_ref must be exact-commit"))

        duplicate = self.load_fixture_manifest(root)
        duplicate["repositories"][1]["repository"] = duplicate["repositories"][0]["repository"]
        cases.append((duplicate, "duplicate public example repository identity"))

        dependency = self.load_fixture_manifest(root)
        dependency["repositories"][0]["depends_on"] = ["minimal-multiplayer"]
        cases.append((dependency, "must have an earlier sequence"))

        private_published = self.load_fixture_manifest(root)
        private_published["repositories"][0]["status"] = "published"
        cases.append((private_published, "published status requires a public, published remote"))

        staged_planned = self.load_fixture_manifest(root)
        staged_planned["repositories"][1]["remote"]["state"] = "source-staged"
        cases.append((staged_planned, "source-staged state requires source-ready status"))

        for manifest, message in cases:
            with self.subTest(message=message):
                self.write_fixture_manifest(root, manifest)
                with self.assertRaisesRegex(ValueError, message):
                    docs_examples.generate_model(root)

    def test_template_requires_complete_files_and_exact_placeholders(self) -> None:
        root = self.make_fixture()
        missing = root / "Examples" / "PublicRepositoryTemplate" / "SECURITY.md"
        missing.unlink()
        with self.assertRaisesRegex(ValueError, "template file does not exist"):
            docs_examples.generate_model(root)

        shutil.copyfile(ENGINE_ROOT / "Examples" / "PublicRepositoryTemplate" / "SECURITY.md", missing)
        manifest = self.load_fixture_manifest(root)
        manifest["program"]["publication"]["placeholders"].append("{{UNUSED_PLACEHOLDER}}")
        self.write_fixture_manifest(root, manifest)
        with self.assertRaisesRegex(ValueError, "placeholder mismatch"):
            docs_examples.generate_model(root)

    def test_published_repository_validation_checks_metadata_and_placeholders(self) -> None:
        root = self.make_fixture()
        repository_root = self.materialize_repository(root)

        result = docs_examples.verify_repository(root, repository_root, "pinned", check_git=False)
        self.assertEqual(result["repository_id"], "project-template")
        self.assertEqual(result["pinned_engine_revision"], "a" * 40)

        readme = repository_root / "README.md"
        readme.write_text(readme.read_text(encoding="utf-8") + "\n{{SUMMARY}}\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "unresolved publication placeholders"):
            docs_examples.verify_repository(root, repository_root, "pinned", check_git=False)

    def test_outputs_are_deterministic_and_check_detects_stale_files(self) -> None:
        root = self.make_fixture()
        first = docs_examples.render_outputs(root)
        second = docs_examples.render_outputs(root)
        self.assertEqual(first, second)
        self.assertIn("cvet/fonline-project-template", first[docs_examples.DEFAULT_INDEX])

        self.assertEqual(docs_examples.main(["--root", str(root), "--write"]), 0)
        self.assertEqual(docs_examples.main(["--root", str(root), "--check"]), 0)
        (root / docs_examples.DEFAULT_INDEX).write_text("stale\n", encoding="utf-8")
        self.assertEqual(docs_examples.main(["--root", str(root), "--check"]), 1)


if __name__ == "__main__":
    unittest.main()
