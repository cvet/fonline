from __future__ import annotations

import json
import re
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ENGINE_ROOT = Path(__file__).resolve().parents[2]
BUILD_TOOLS = ENGINE_ROOT / "BuildTools"
GUIDE_PATH = "Docs/en/how-to/build/public-example-repositories.md"
RUSSIAN_PATH = "Docs/ru/how-to/build/public-example-repositories.md"
LEGACY_PATH = "Docs/PublicExampleRepositories.md"
import sys

sys.path.insert(0, str(BUILD_TOOLS))

import docs_examples
import docs_localization


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
            ENGINE_ROOT / "Examples" / "MinimalMultiplayer",
            root / "Examples" / "MinimalMultiplayer",
            ignore=shutil.ignore_patterns("Build", "Engine"),
        )
        shutil.copytree(
            ENGINE_ROOT / "Examples" / "NativeExtensionSample",
            root / "Examples" / "NativeExtensionSample",
            ignore=shutil.ignore_patterns("Build", "Engine"),
        )
        shutil.copytree(
            ENGINE_ROOT / "Examples" / "ContentShowcase",
            root / "Examples" / "ContentShowcase",
            ignore=shutil.ignore_patterns("Build", "Engine", "Workspace"),
        )
        shutil.copytree(
            ENGINE_ROOT / "Examples" / "PublicRepositoryTemplate",
            root / "Examples" / "PublicRepositoryTemplate",
        )
        shutil.copyfile(
            ENGINE_ROOT / docs_examples.DEFAULT_MANIFEST,
            root / docs_examples.DEFAULT_MANIFEST,
        )
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

    def test_stage_repository_materializes_clean_reviewable_source(self) -> None:
        root = self.make_fixture()
        source_root = root / "Examples" / "MinimalMultiplayer"
        (source_root / "Build").mkdir()
        (source_root / "Build" / "generated.txt").write_text("generated\n", encoding="utf-8")
        (source_root / "local.log").write_text("local\n", encoding="utf-8")
        output_root = root / "candidate"
        revision = "a" * 40

        result = docs_examples.stage_repository(
            root,
            "minimal-multiplayer",
            output_root,
            revision,
            check_engine_git=False,
        )

        self.assertEqual(result["repository"], "cvet/fonline-minimal-multiplayer")
        self.assertTrue((output_root / "Scripts" / "Tutorial.fos").is_file())
        self.assertTrue((output_root / "TUTORIAL.md").is_file())
        self.assertIn("FOnline Minimal Multiplayer", (output_root / "README.md").read_text(encoding="utf-8"))
        self.assertIn("python validate.py", (output_root / "README.md").read_text(encoding="utf-8"))
        self.assertIn("url = https://github.com/cvet/fonline.git", (output_root / ".gitmodules").read_text())
        self.assertFalse((output_root / "Build").exists())
        self.assertFalse((output_root / "Engine").exists())
        self.assertFalse((output_root / "local.log").exists())
        provenance = json.loads((output_root / "assets" / "provenance.json").read_text(encoding="utf-8"))
        self.assertIn(f"/blob/{revision}/", provenance["assets"][0]["source"])
        metadata = json.loads((output_root / "example-repository.json").read_text(encoding="utf-8"))
        self.assertEqual(metadata["engine"]["revision"], revision)
        self.assertEqual(metadata["primary_check"], "python validate.py")
        self.assertNotRegex(
            "\n".join(
                path.read_text(encoding="utf-8")
                for path in output_root.rglob("*")
                if path.is_file()
            ),
            docs_examples.PLACEHOLDER_PATTERN,
        )

        asset_path = output_root / "Engine" / "Resources" / "Radiation.png"
        asset_path.parent.mkdir(parents=True)
        asset_path.write_bytes(b"not the recorded asset")
        with self.assertRaisesRegex(ValueError, "digest does not match"):
            docs_examples.verify_repository(root, output_root, "pinned", check_git=False)

        with self.assertRaisesRegex(ValueError, "output already exists"):
            docs_examples.stage_repository(
                root,
                "minimal-multiplayer",
                output_root,
                revision,
                check_engine_git=False,
            )

    def test_stage_repository_rejects_non_exact_revision(self) -> None:
        root = self.make_fixture()
        with self.assertRaisesRegex(ValueError, "exact lowercase"):
            docs_examples.stage_repository(root, "minimal-multiplayer", root / "candidate", "master")

    def test_stage_content_showcase_keeps_authored_assets_and_capture_contract(self) -> None:
        root = self.make_fixture()
        source_root = root / "Examples" / "ContentShowcase"
        (source_root / "Build").mkdir()
        (source_root / "Build" / "generated.txt").write_text("generated\n", encoding="utf-8")
        (source_root / "Workspace").mkdir()
        (source_root / "Workspace" / "local-report.json").write_text("{}\n", encoding="utf-8")
        output_root = root / "showcase-candidate"

        result = docs_examples.stage_repository(
            root,
            "content-showcase",
            output_root,
            "a" * 40,
            check_engine_git=False,
        )

        self.assertEqual(result["repository"], "cvet/fonline-content-showcase")
        self.assertTrue((output_root / "TUTORIAL.md").is_file())
        self.assertTrue((output_root / "README.ru.md").is_file())
        self.assertTrue((output_root / "ShowcaseAssets/Showcase/Sprites/Beacon_0.tga").is_file())
        self.assertTrue((output_root / "captures/windows-direct3d11.png").is_file())
        self.assertTrue((output_root / "captures/web-webgl2.png").is_file())
        self.assertTrue((output_root / "captures/capture-contract.json").is_file())
        self.assertTrue((output_root / "WebTests/package-lock.json").is_file())
        self.assertFalse((output_root / "WebTests/node_modules").exists())
        self.assertTrue((output_root / "capture_showcase_web.mjs").is_file())
        self.assertFalse((output_root / "Build").exists())
        self.assertFalse((output_root / "Workspace").exists())
        provenance = json.loads((output_root / "assets/provenance.json").read_text(encoding="utf-8"))
        self.assertEqual(len(provenance["assets"]), 13)
        self.assertTrue(all(asset["source"] == "project-original" for asset in provenance["assets"]))

    def test_stage_native_extension_repository_keeps_source_and_empty_provenance(self) -> None:
        root = self.make_fixture()
        output_root = root / "native-candidate"

        result = docs_examples.stage_repository(
            root,
            "native-extension-sample",
            output_root,
            "a" * 40,
            check_engine_git=False,
        )

        self.assertEqual(result["repository"], "cvet/fonline-native-extension-sample")
        self.assertTrue((output_root / "SourceExt" / "ServerExtension.cpp").is_file())
        self.assertTrue((output_root / "Tests" / "NativeExtensionCoreTest.cpp").is_file())
        self.assertTrue((output_root / "TUTORIAL.md").is_file())
        self.assertFalse((output_root / "Engine").exists())
        provenance = json.loads((output_root / "assets" / "provenance.json").read_text(encoding="utf-8"))
        self.assertEqual(provenance["assets"], [])

    def test_stage_repository_requires_current_clean_remote_engine_revision(self) -> None:
        root = self.make_fixture()
        revision = "a" * 40
        cases = [
            (["b" * 40], "must match the current Engine checkout"),
            ([revision, " M Source/Example.cpp"], "requires a clean Engine working tree"),
            ([revision, "", ""], "not contained by a fetched remote-tracking branch"),
        ]

        for index, (git_results, message) in enumerate(cases):
            with self.subTest(message=message):
                with mock.patch.object(docs_examples, "_run_git", side_effect=git_results):
                    with self.assertRaisesRegex(ValueError, message):
                        docs_examples.stage_repository(
                            root,
                            "minimal-multiplayer",
                            root / f"candidate-{index}",
                            revision,
                        )

    def test_real_registry_has_ordered_owned_portfolio(self) -> None:
        model = docs_examples.generate_model(ENGINE_ROOT)

        self.assertEqual(model["summary"]["repository_count"], 4)
        self.assertEqual(model["summary"]["source_ready_count"], 4)
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
        self.assertEqual(
            [
                (
                    repository["remote"]["verified_on"],
                    repository["remote"]["default_branch"],
                    repository["remote"]["head_commit"],
                    repository["remote"]["required_checks_state"],
                )
                for repository in model["repositories"]
            ],
            [
                ("2026-08-03", "main", "9946ca42c332a294f8fedd2732e7850a01c1ec27", "not-observed"),
                ("2026-08-03", "main", "97d232431488125b370be352fdcf28f66e6cbf4f", "not-observed"),
                ("2026-08-03", "main", "011dab0d07eef6387609821206b8ee534ec51c3f", "not-observed"),
                ("2026-08-03", "main", "97823816ab333a62aced43edd4daafa19c5fee22", "not-observed"),
            ],
        )
        self.assertEqual(
            model["repositories"][0]["remote"]["engine_revision"],
            "9d74c751f5684f80aef3b35a0eb16a8fabf9fa42",
        )
        rendered = docs_examples.render_index(model)
        self.assertIn("release Engine ref `exact-commit`", rendered)
        self.assertIn("update delivery `reviewed-pull-request`", rendered)
        self.assertIn("`minimal-multiplayer`=not observed", rendered)
        for repository_id in (
            "project-template",
            "minimal-multiplayer",
            "content-showcase",
            "native-extension-sample",
        ):
            self.assertIn(f"- `{repository_id}`: source `source-ready`;", rendered)
            self.assertIn("required checks `not-observed`.", rendered)
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

        multiplayer = model["repositories"][1]
        self.assertEqual(multiplayer["status"], "source-ready")
        self.assertEqual(multiplayer["remote"]["state"], "reserved")
        self.assertEqual(multiplayer["source_path"], "Examples/MinimalMultiplayer")
        tutorial_presets = json.loads(
            (ENGINE_ROOT / "Examples/MinimalMultiplayer/CMakePresets.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            {preset["name"] for preset in tutorial_presets["configurePresets"]},
            {"tutorial-base", "windows", "linux"},
        )
        self.assertEqual(
            {preset["name"]: preset["targets"] for preset in tutorial_presets["buildPresets"]},
            {
                "windows-check": ["RunTutorialChecks"],
                "linux-check": ["RunTutorialChecks"],
                "windows-package": ["RunTutorialPackageChecks"],
                "linux-package": ["RunTutorialPackageChecks"],
            },
        )
        tutorial_validator = (ENGINE_ROOT / "Examples/MinimalMultiplayer/validate.py").read_text(encoding="utf-8")
        self.assertIn('["cmake", "--preset", preset]', tutorial_validator)
        self.assertIn('["cmake", "--build", "--preset", f"{preset}-check"]', tutorial_validator)
        tutorial_runner = (ENGINE_ROOT / "Examples/MinimalMultiplayer/run_tutorial_smoke.py").read_text(
            encoding="utf-8"
        )
        tutorial_smoke = (ENGINE_ROOT / "Examples/MinimalMultiplayer/tutorial-smoke.json").read_text(
            encoding="utf-8"
        )
        for marker in (
            "tutorial_content_test_passed",
            "tutorial_client_map_loaded",
            "tutorial_server_supply_collected=1",
            "tutorial_client_smoke_passed",
        ):
            self.assertIn(marker, tutorial_smoke)
        self.assertIn("gameplay_test_runner.main", tutorial_runner)
        tutorial_package = (ENGINE_ROOT / "Examples/MinimalMultiplayer/package-smoke.json").read_text(
            encoding="utf-8"
        )
        package_verifier = (ENGINE_ROOT / "Examples/MinimalMultiplayer/verify_tutorial_package.py").read_text(
            encoding="utf-8"
        )
        self.assertIn("packaged-server-client-interaction", tutorial_package)
        self.assertIn("verify_archive", package_verifier)
        self.assertIn("tutorial-packaging-manifest.json", package_verifier)
        self.assertIn("gameplay_test_runner.run_manifest", package_verifier)
        tutorial_config = (
            ENGINE_ROOT / "Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain"
        ).read_text(encoding="utf-8")
        tutorial_config_generator = (
            ENGINE_ROOT / "Examples/MinimalMultiplayer/generate_config.py"
        ).read_text(encoding="utf-8")
        for setting in (
            "Baking.BakeLanguages = engl russ",
            "Render.ImGuiDefaultEffect = Effects/ImGui_Default.fofx",
            "Tutorial.Automation = False",
        ):
            self.assertIn(setting, tutorial_config)
        self.assertIn("def render_config", tutorial_config_generator)
        self.assertIn("Settings.inc", tutorial_config_generator)

        showcase = model["repositories"][2]
        self.assertEqual(showcase["status"], "source-ready")
        self.assertEqual(showcase["remote"]["state"], "reserved")
        self.assertEqual(showcase["source_path"], "Examples/ContentShowcase")
        self.assertEqual(showcase["primary_check"], "python validate.py")
        showcase_presets = json.loads(
            (ENGINE_ROOT / "Examples/ContentShowcase/CMakePresets.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            {preset["name"] for preset in showcase_presets["configurePresets"]},
            {
                "showcase-base",
                "windows",
                "linux",
                "web",
                "web-package-host-base",
                "web-package-host-windows",
                "web-package-host-linux",
                "web-package",
            },
        )
        self.assertEqual(
            {preset["name"]: preset["targets"] for preset in showcase_presets["buildPresets"]},
            {
                "windows-check": ["RunShowcaseChecks"],
                "linux-check": ["RunShowcaseChecks"],
                "windows-capture": ["RunShowcaseCapture"],
                "linux-capture": ["RunShowcaseCapture"],
                "web-check": ["RunShowcaseWebChecks"],
                "web-package-host-windows-bake": ["ForceBakeResources", "FOCS_ServerHeadless"],
                "web-package-host-linux-bake": ["ForceBakeResources", "FOCS_ServerHeadless"],
                "web-package-check": ["RunShowcaseWebPackageChecks"],
            },
        )
        web_package_contract = json.loads(
            (ENGINE_ROOT / "Examples/ContentShowcase/showcase-web-package.json").read_text(encoding="utf-8")
        )
        self.assertEqual(web_package_contract["payload_directory"], "FOCS-Client-ShowcaseRelease-Web")
        self.assertIn("Resources.data", web_package_contract["required_files"])
        showcase_verifier = (ENGINE_ROOT / "Examples/ContentShowcase/verify_showcase.py").read_text(
            encoding="utf-8"
        )
        self.assertIn("verify_web_package", showcase_verifier)
        self.assertIn("package_zip.testzip()", showcase_verifier)
        web_runtime_contract = json.loads(
            (ENGINE_ROOT / "Examples/ContentShowcase/showcase-web-runtime.json").read_text(encoding="utf-8")
        )
        self.assertEqual(web_runtime_contract["capture"], {"width": 1280, "height": 800})
        self.assertIn("showcase_client_world_ready", web_runtime_contract["required_client_markers"])
        web_test_package = json.loads(
            (ENGINE_ROOT / "Examples/ContentShowcase/WebTests/package.json").read_text(encoding="utf-8")
        )
        self.assertEqual(web_test_package["dependencies"]["playwright"], "1.62.0")
        capture_contract = json.loads(
            (ENGINE_ROOT / "Examples/ContentShowcase/captures/capture-contract.json").read_text(encoding="utf-8")
        )
        self.assertEqual(capture_contract["schema_version"], 1)
        capture_profiles = {profile["id"]: profile for profile in capture_contract["profiles"]}
        self.assertIn("windows-direct3d11", capture_profiles)
        self.assertEqual(capture_profiles["linux-opengl"]["status"], "required-unobserved")
        web_profile = next(profile for profile in capture_contract["profiles"] if profile["id"] == "web-webgl2")
        self.assertEqual(web_profile["status"], "observed-local")
        self.assertEqual(web_profile["width"], 1280)
        self.assertEqual(web_profile["height"], 800)
        self.assertEqual(web_profile["command"], "python validate.py --web-runtime")
        self.assertEqual(len(web_profile["package_sha256"]), 64)
        self.assertEqual(web_profile["pixel_evidence"]["visible_pixels"], 1280 * 800)
        sys.path.insert(0, str(ENGINE_ROOT / "Examples/ContentShowcase"))
        import capture_showcase
        import capture_showcase_web

        with mock.patch.object(capture_showcase.platform, "system", return_value="Windows"):
            self.assertEqual(
                capture_showcase.capture_profile(),
                (
                    "windows-direct3d11",
                    "ShowcaseCapture",
                    "windows-direct3d11.png",
                    "cmake --build --preset windows-capture",
                ),
            )
        with mock.patch.object(capture_showcase.platform, "system", return_value="Linux"):
            self.assertEqual(
                capture_showcase.capture_profile(),
                (
                    "linux-opengl",
                    "ShowcaseCaptureOpenGL",
                    "linux-opengl.png",
                    "cmake --build --preset linux-capture",
                ),
            )
        capture_manifest = json.loads(
            (ENGINE_ROOT / "Examples/ContentShowcase/showcase-capture.json").read_text(encoding="utf-8")
        )
        commands = [
            process["command"]
            for scenario in capture_manifest["scenarios"]
            for process in scenario["processes"]
        ]
        self.assertTrue(all("{capture_subconfig}" in command for command in commands))

        width, height, rgba = capture_showcase_web.read_png(
            ENGINE_ROOT / "Examples/ContentShowcase/captures/web-webgl2.png"
        )
        self.assertEqual((width, height), (1280, 800))
        self.assertEqual(capture_showcase.verify_pixels(width, height, rgba), web_profile["pixel_evidence"])
        provenance = json.loads(
            (ENGINE_ROOT / "Examples/ContentShowcase/assets/provenance.json").read_text(encoding="utf-8")
        )
        self.assertEqual(len(provenance["assets"]), 13)
        self.assertTrue(all(asset["source"] == "project-original" for asset in provenance["assets"]))
        attributes = (ENGINE_ROOT / ".gitattributes").read_text(encoding="utf-8")
        for source_path in (
            "Examples/ContentShowcase/ShowcaseAssets/Showcase/Effects/ShowcaseParticle.fofx",
            "Examples/ContentShowcase/ShowcaseAssets/Showcase/Particles/Showcase.spark",
            "Examples/ContentShowcase/ShowcaseAssets/Showcase/Sprites/Beacon.fofrm",
        ):
            self.assertIn(f"{source_path} text eol=lf", attributes)

        native_sample = model["repositories"][3]
        self.assertEqual(native_sample["status"], "source-ready")
        self.assertEqual(native_sample["remote"]["state"], "reserved")
        self.assertEqual(native_sample["source_path"], "Examples/NativeExtensionSample")
        native_presets = json.loads(
            (ENGINE_ROOT / "Examples/NativeExtensionSample/CMakePresets.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            {preset["name"] for preset in native_presets["configurePresets"]},
            {"native-extension-base", "windows", "linux"},
        )
        self.assertTrue(
            all(preset["targets"] == ["RunNativeExtensionChecks"] for preset in native_presets["buildPresets"])
        )
        native_cmake = (ENGINE_ROOT / "Examples/NativeExtensionSample/CMakeLists.txt").read_text(encoding="utf-8")
        for marker in (
            "AddProjectLibraries(",
            "ROLES SERVER",
            "AddEngineSources(SERVER SourceExt/ServerExtension.cpp)",
            "FONATIVE_NativeExtensionCoreTest",
            "RunNativeExtensionChecks",
        ):
            self.assertIn(marker, native_cmake)
        native_extension = (
            ENGINE_ROOT / "Examples/NativeExtensionSample/SourceExt/ServerExtension.cpp"
        ).read_text(encoding="utf-8")
        for marker in (
            "///@ EngineHook",
            "ServerInitHook",
            "///@ ExportMethod",
            "Server_Game_NativeExtensionValue",
            "server->UserData",
        ):
            self.assertIn(marker, native_extension)
        sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))
        import buildtools

        for target_name, platform_name in (
            ("win64-native-extension-smoke", "win64"),
            ("linux-native-extension-smoke", "linux"),
        ):
            target = buildtools.VALIDATION_TARGETS[target_name]
            self.assertEqual(target["platform"], platform_name)
            self.assertEqual(target["target"], "native-extension-smoke")
            self.assertEqual(target["project"], "NativeExtensionSample")
            self.assertEqual(target["run_target"], "RunNativeExtensionChecks")

        for target_name, platform_name, build_target, run_target in (
            ("win64-showcase-smoke", "win64", "showcase-smoke", "RunShowcaseChecks"),
            ("linux-showcase-smoke", "linux", "showcase-smoke", "RunShowcaseChecks"),
            ("linux-showcase-capture", "linux", "showcase-smoke", "RunShowcaseCapture"),
            ("web-showcase-build", "web", "showcase-web", "RunShowcaseWebChecks"),
            (
                "web-showcase-package",
                "web",
                "showcase-web-package",
                "RunShowcaseWebPackageChecks",
            ),
            (
                "web-showcase-runtime",
                "web",
                "showcase-web-package",
                "RunShowcaseWebPackageChecks",
            ),
        ):
            target = buildtools.VALIDATION_TARGETS[target_name]
            self.assertEqual(target["platform"], platform_name)
            self.assertEqual(target["target"], build_target)
            self.assertEqual(target["project"], "ContentShowcase")
            self.assertEqual(target["run_target"], run_target)
        web_package_target = buildtools.VALIDATION_TARGETS["web-showcase-package"]
        self.assertEqual(web_package_target["host_target"], "showcase-web-package-host")
        self.assertEqual(web_package_target["host_run_target"], "ForceBakeResources")
        web_runtime_target = buildtools.VALIDATION_TARGETS["web-showcase-runtime"]
        self.assertEqual(web_runtime_target["host_target"], "showcase-web-package-host")
        self.assertEqual(web_runtime_target["host_run_target"], "ForceBakeResources")
        self.assertTrue(web_runtime_target["showcase_web_runtime"])

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
        staged_planned["repositories"][2]["status"] = "planned"
        staged_planned["repositories"][2]["remote"]["state"] = "source-staged"
        cases.append((staged_planned, "source-staged state requires source-ready status"))

        unchecked_published = self.load_fixture_manifest(root)
        unchecked_published["repositories"][0]["status"] = "published"
        unchecked_published["repositories"][0]["remote"].update(
            {"visibility": "public", "state": "published"}
        )
        cases.append((unchecked_published, "published status requires passing required checks"))

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

        gitmodules = repository_root / ".gitmodules"
        original_gitmodules = gitmodules.read_text(encoding="utf-8")
        gitmodules.write_text(
            original_gitmodules.replace("https://github.com/cvet/fonline.git", "https://example.com/fonline.git"),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, r"\.gitmodules Engine URL"):
            docs_examples.verify_repository(root, repository_root, "pinned", check_git=False)
        gitmodules.write_text(original_gitmodules, encoding="utf-8")

        readme = repository_root / "README.md"
        readme.write_text(readme.read_text(encoding="utf-8") + "\n{{SUMMARY}}\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "unresolved publication placeholders"):
            docs_examples.verify_repository(root, repository_root, "pinned", check_git=False)

    def test_outputs_are_deterministic_and_check_detects_stale_files(self) -> None:
        root = self.make_fixture()
        first = docs_examples.render_outputs(root)
        second = docs_examples.render_outputs(root)
        self.assertEqual(first, second)
        self.assertEqual(
            set(first),
            {
                docs_examples.DEFAULT_MODEL,
                docs_examples.DEFAULT_INDEX,
                docs_examples.RUSSIAN_INDEX,
                docs_examples.LEGACY_INDEX,
            },
        )
        self.assertIn("cvet/fonline-project-template", first[docs_examples.DEFAULT_INDEX])
        self.assertIn(
            docs_localization.translation_metadata_line(
                "generated-public-examples-index",
                docs_examples.DEFAULT_INDEX,
                docs_localization.normalized_sha256(first[docs_examples.DEFAULT_INDEX]),
            ),
            first[docs_examples.RUSSIAN_INDEX],
        )
        self.assertIn(
            "../../en/reference/public-examples/index.md",
            first[docs_examples.LEGACY_INDEX],
        )

        self.assertEqual(docs_examples.main(["--root", str(root), "--write"]), 0)
        self.assertEqual(docs_examples.main(["--root", str(root), "--check"]), 0)
        (root / docs_examples.DEFAULT_INDEX).write_text("stale\n", encoding="utf-8")
        self.assertEqual(docs_examples.main(["--root", str(root), "--check"]), 1)

    def test_human_guide_owns_remote_gates_project_evidence_and_locale_routes(self) -> None:
        guide = (ENGINE_ROOT / GUIDE_PATH).read_text(encoding="utf-8")
        russian = (ENGINE_ROOT / RUSSIAN_PATH).read_text(encoding="utf-8")
        legacy = (ENGINE_ROOT / LEGACY_PATH).read_text(encoding="utf-8")

        for marker in (
            "## Contract status",
            "## Remote staging audit",
            "### Publication decision",
            "## Project evidence and extraction rules",
            "`required_checks_state`",
            "`asset-provenance-and-public-examples`",
            "no required commit status was observed",
        ):
            self.assertIn(marker, guide)

        self.assertGreaterEqual(len(russian.encode("utf-8")), len(guide.encode("utf-8")) * 0.75)
        self.assertEqual(
            len(re.findall(r"^#{2,3} ", russian, re.MULTILINE)),
            len(re.findall(r"^#{2,3} ", guide, re.MULTILINE)),
        )
        fenced = re.compile(r"```[^\n]*\n(.*?)```", re.DOTALL)
        self.assertEqual(fenced.findall(russian), fenced.findall(guide))

        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(encoding="utf-8")
        )["documents"]
        self.assertEqual(documents[GUIDE_PATH]["id"], "public-example-repositories")
        self.assertEqual(documents[GUIDE_PATH]["disposition"], "retain")
        self.assertEqual(documents[LEGACY_PATH]["state"], "redirect")
        self.assertEqual(documents[LEGACY_PATH]["redirect_to"], "public-example-repositories")
        self.assertIn("BuildTools/ExternalProjectEvidence.json", documents[GUIDE_PATH]["sources"])
        generated = documents[docs_examples.DEFAULT_INDEX]
        generated_legacy = documents[docs_examples.LEGACY_INDEX]
        self.assertEqual(generated["id"], "generated-public-examples-index")
        self.assertEqual(generated["state"], "current")
        self.assertEqual(generated["disposition"], "retain")
        self.assertEqual(generated_legacy["state"], "redirect")
        self.assertEqual(generated_legacy["redirect_to"], generated["id"])
        for heading in re.findall(r"^#{2,3} .+$", guide, re.MULTILINE):
            self.assertIn(heading, legacy)

        evidence = json.loads(
            (ENGINE_ROOT / "BuildTools/ExternalProjectEvidence.json").read_text(encoding="utf-8")
        )
        record = next(
            item for item in evidence["records"] if item["id"] == "asset-provenance-and-public-examples"
        )
        self.assertIn(GUIDE_PATH, record["engine_targets"])


if __name__ == "__main__":
    unittest.main()
