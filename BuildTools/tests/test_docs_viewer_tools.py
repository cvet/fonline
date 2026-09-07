from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
EN_GUIDE = "Docs/en/how-to/tools/animation-particle-viewers.md"
RU_GUIDE = "Docs/ru/how-to/tools/animation-particle-viewers.md"
LEGACY_GUIDE = "Docs/ViewerTools.md"


class ViewerToolsDocumentationTests(unittest.TestCase):
    @staticmethod
    def _read(relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    @staticmethod
    def _fences(markdown: str) -> list[str]:
        return re.findall(
            r"^```[^\n]*\n.*?^```$",
            markdown,
            flags=re.MULTILINE | re.DOTALL,
        )

    def test_cmake_creates_both_viewers_with_mapper(self) -> None:
        applications = (
            ENGINE_ROOT / "BuildTools/cmake/stages/Applications.cmake"
        ).read_text(encoding="utf-8")
        sources = (
            ENGINE_ROOT / "BuildTools/cmake/stages/EngineSources.cmake"
        ).read_text(encoding="utf-8")
        libraries = (
            ENGINE_ROOT / "BuildTools/cmake/stages/CoreLibs.cmake"
        ).read_text(encoding="utf-8")
        init = (
            ENGINE_ROOT / "BuildTools/cmake/stages/Init.cmake"
        ).read_text(encoding="utf-8")

        self.assertIn("if(FO_BUILD_MAPPER)", applications)
        for viewer in ("AnimationViewer", "ParticleViewer"):
            self.assertIn(
                f"AddExecutableApplication(${{FO_DEV_NAME}}_{viewer}",
                applications,
            )
            self.assertIn(
                f'Source/Applications/{viewer}App.cpp"', applications
            )
            self.assertIn(f"{viewer}Lib", applications)
            self.assertIn(f"Source/Tools/{viewer}.cpp", sources)
            self.assertIn(
                f"AddCoreStaticLibrary({viewer}Lib", libraries
            )
            self.assertIn(
                f"SetBinaryOutputPath(FO_{viewer.upper().replace('VIEWER', '_VIEWER')}_OUTPUT "
                f"{viewer})",
                init,
            )

    def test_standalone_hosts_use_viewer_only_client_services(self) -> None:
        for viewer in ("AnimationViewer", "ParticleViewer"):
            source = (
                ENGINE_ROOT / f"Source/Applications/{viewer}App.cpp"
            ).read_text(encoding="utf-8")
            for marker in (
                "SafeAlloc::MakeRefCounted<ClientEngine>",
                "BakerDataSource",
                "AddPacksSource(settings.ClientResources",
                "AddPacksSource(settings.ClientResources, "
                "settings.MapperResourceEntries)",
                "FrameAdvance()",
                "SetFillViewport(true)",
                "SaveSettings()",
            ):
                self.assertIn(marker, source, f"{viewer}: {marker}")
            for absent in (
                "ClientStartupSettingsHook",
                "MainLoop()",
                "Connect(",
            ):
                self.assertNotIn(absent, source, f"{viewer}: {absent}")

    def test_animation_viewer_controls_match_source(self) -> None:
        source = (
            ENGINE_ROOT / "Source/Tools/AnimationViewer.cpp"
        ).read_text(encoding="utf-8")

        for marker in (
            'ImGui::InputTextWithHint("##Filter", "Filter"',
            'ImGui::SliderFloat("Angle"',
            'ImGui::SliderFloat("Zoom"',
            'ImGui::Checkbox("Direct draw"',
            'ImGui::Checkbox("Root"',
            'ImGui::Checkbox("Name level"',
            'ImGui::Checkbox("Draw rect"',
            'ImGui::Checkbox("View rect"',
            'ImGui::Checkbox("Loop"',
            'ImGui::Button("Idle")',
            "GetAvailableAnimations()",
            "GetCritterAnimFrames(",
            "Render.ModelLayerProperties",
            "PlayIdle();",
        ):
            self.assertIn(marker, source)

        header = (
            ENGINE_ROOT / "Source/Tools/AnimationViewer.h"
        ).read_text(encoding="utf-8")
        self.assertIn('SettingsStorage _settings {"AnimationViewer"}', header)

    def test_particle_viewer_controls_match_source(self) -> None:
        source = (
            ENGINE_ROOT / "Source/Tools/ParticleViewer.cpp"
        ).read_text(encoding="utf-8")

        for marker in (
            "factory->GetExtensions()",
            "FilterFiles(particle_extension)",
            'ImGui::InputInt("Seed"',
            'ImGui::Button("New seed")',
            'ImGui::Button("Replay")',
            'ImGui::Checkbox("Loop"',
            'ImGui::Checkbox("Prewarm"',
            'ImGui::Checkbox("Draw in scene"',
            'ImGui::SliderFloat("Direction"',
            'ImGui::Checkbox("Root"',
            'ImGui::Checkbox("Draw rect"',
            'ImGui::Checkbox("Show wireframe"',
            "PlayWithSeed(_seed)",
            "RebaseWorldParticles(",
        ):
            self.assertIn(marker, source)

        header = (
            ENGINE_ROOT / "Source/Tools/ParticleViewer.h"
        ).read_text(encoding="utf-8")
        self.assertIn('SettingsStorage _settings {"ParticleViewer"}', header)

    def test_settings_and_package_boundaries_are_current(self) -> None:
        settings = (
            ENGINE_ROOT / "Source/Common/SettingsStorage.cpp"
        ).read_text(encoding="utf-8")
        package = json.loads(
            (ENGINE_ROOT / "BuildTools/PackageInterface.json").read_text(
                encoding="utf-8"
            )
        )

        self.assertIn('strex("Software\\\\FOnline\\\\{}"', settings)
        self.assertIn("Platform::GetUserDataBase()", settings)

        targets = {
            target["name"]: target for target in package["targets"]
        }
        for viewer in ("AnimationViewer", "ParticleViewer"):
            self.assertEqual(targets[viewer]["resource_mode"], "none")
            self.assertEqual(targets[viewer]["required_packs"], ["NoRes"])

        for platform in package["platforms"]:
            if platform["name"] in ("Windows", "Linux"):
                self.assertIn("AnimationViewer", platform["targets"])
                self.assertIn("ParticleViewer", platform["targets"])
            else:
                self.assertNotIn(
                    "AnimationViewer", platform.get("targets", [])
                )
                self.assertNotIn(
                    "ParticleViewer", platform.get("targets", [])
                )

    def test_guides_cover_build_review_evidence_and_locale_parity(self) -> None:
        guide = self._read(EN_GUIDE)
        russian = self._read(RU_GUIDE)
        legacy = self._read(LEGACY_GUIDE)
        normalized = " ".join(guide.split())

        for heading in (
            "## Purpose",
            "## Production review contract",
            "## Source paths inspected",
            "## Application and ownership boundary",
            "## Build and launch",
            "## Animation review workflow",
            "### Animation discovery",
            "## Particle review workflow",
            "### Atlas and direct-scene interpretation",
            "## Persisted settings",
            "## Failure diagnosis",
            "## Review evidence and screenshots",
            "## Maintenance",
            "## See also",
        ):
            self.assertIn(heading, guide)
            self.assertIn(heading, legacy)

        for heading in (
            "## Назначение",
            "## Контракт производственной проверки",
            "## Проверенные пути исходников",
            "## Граница приложения и владения",
            "## Сборка и запуск",
            "## Процесс проверки анимации",
            "### Обнаружение анимаций",
            "## Процесс проверки частиц",
            "### Интерпретация атласа и прямого рисования в сцене",
            "## Сохраняемые настройки",
            "## Диагностика отказов",
            "## Материалы ревью и снимки",
            "## Сопровождение",
            "## См. также",
        ):
            self.assertIn(heading, russian)

        self.assertEqual(self._fences(guide), self._fences(russian))

        for marker in (
            "FO_BUILD_MAPPER=ON",
            "FOMM_AnimationViewer",
            "FOMM_ParticleViewer",
            "AnimationViewer-Windows-win64",
            "ParticleViewer-Windows-win64",
            "do not call the gameplay `ClientStartupSettingsHook`",
            "viewers are inspection tools, not authoring editors",
            "no generic Editor application",
            "no AssetExplorer application",
            "no script, command-line selection, screenshot, or headless "
            "inspection API",
            "process-start smoke proves only",
            "developer package that already supplies compatible resources",
            "FOnline TLA currently supplies no equivalent focused viewer workflow",
        ):
            self.assertIn(marker, normalized)

    def test_manifest_navigation_and_entry_points_route_viewers(self) -> None:
        manifest = json.loads(
            (
                ENGINE_ROOT / "Docs/documentation-manifest.json"
            ).read_text(encoding="utf-8")
        )
        document = manifest["documents"][EN_GUIDE]

        self.assertEqual(document["id"], "viewer-tools")
        self.assertEqual(document["owner"], "tooling")
        self.assertEqual(document["state"], "current")
        self.assertEqual(document["disposition"], "retain")
        self.assertEqual(
            document["target"],
            EN_GUIDE,
        )
        self.assertIn("content-author", document["audiences"])
        self.assertIn("ai-agent", document["audiences"])
        for path in document["sources"]:
            self.assertTrue((ENGINE_ROOT / path).exists(), path)

        legacy = manifest["documents"][LEGACY_GUIDE]
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["disposition"], "replace")
        self.assertEqual(legacy["redirect_to"], "viewer-tools")
        self.assertEqual(legacy["target"], EN_GUIDE)

        content = next(
            group
            for group in manifest["site_delivery"]["navigation"]
            if group["id"] == "content"
        )
        self.assertIn("viewer-tools", content["document_ids"])

        expected_links = {
            "AGENTS.md": EN_GUIDE,
            "README.md": "[Animation and Particle Viewers]"
            "(Docs/en/how-to/tools/animation-particle-viewers.md)",
            "README.ru.md": "Docs/ru/how-to/tools/animation-particle-viewers.md",
            "Docs/en/index.md": "[Animation and Particle Viewers]"
            "(how-to/tools/animation-particle-viewers.md)",
            "Docs/en/reference/tools/index.md": "[Viewer Tools]"
            "(../../how-to/tools/animation-particle-viewers.md)",
            "Docs/en/reference/applications.md": "[Viewer Tools]"
            "(../how-to/tools/animation-particle-viewers.md)",
            "Docs/en/how-to/tools/mapper.md": "[Animation and Particle Viewers]"
            "(animation-particle-viewers.md)",
        }
        for path, marker in expected_links.items():
            text = (ENGINE_ROOT / path).read_text(encoding="utf-8")
            self.assertIn(marker, text, path)

    def test_stale_asset_explorer_route_is_removed_and_ci_is_wired(self) -> None:
        for path in (
            "README.md",
            "Docs/en/index.md",
            "Docs/en/contributing/source-tree/index.md",
            "Docs/en/reference/tools/index.md",
        ):
            text = (ENGINE_ROOT / path).read_text(encoding="utf-8")
            self.assertNotIn("asset explorer", text.casefold(), path)

        workflow = (
            ENGINE_ROOT / ".github/workflows/validate.yml"
        ).read_text(encoding="utf-8")
        maintenance = (
            ENGINE_ROOT / "Docs/en/contributing/documentation/index.md"
        ).read_text(encoding="utf-8")
        ai_source = (
            ENGINE_ROOT / "Docs/ai-evaluation.json"
        ).read_text(encoding="utf-8")

        self.assertIn(
            "python3 BuildTools/tests/test_docs_viewer_tools.py", workflow
        )
        for marker in (
            "`AnimationViewer`",
            "`ParticleViewer`",
            "[Animation and Particle Viewers]"
            "(../../how-to/tools/animation-particle-viewers.md)",
            "`test_docs_viewer_tools.py`",
        ):
            self.assertIn(marker, maintenance)
        self.assertNotIn("asset explorer", ai_source.casefold())
        self.assertIn(
            '"id": "inspect-animation-particle-assets"', ai_source
        )
        evaluation = json.loads(ai_source)
        task = next(
            task
            for task in evaluation["tasks"]
            if task["id"] == "inspect-animation-particle-assets"
        )
        self.assertEqual(task["primary_document_id"], "viewer-tools")
        self.assertGreaterEqual(len(task["retrieval_checks"]), 2)
        self.assertGreaterEqual(len(task["answer_checks"]), 4)
        self.assertIn(
            "production-review-contract",
            {check["anchor"] for check in task["answer_checks"]},
        )

        evidence = json.loads(
            self._read("BuildTools/ExternalProjectEvidence.json")
        )
        record = next(
            record
            for record in evidence["records"]
            if record["id"] == "mapper-and-focused-viewers"
        )
        self.assertEqual(record["disposition"], "promoted")
        self.assertIn(EN_GUIDE, record["engine_targets"])
        self.assertEqual(record["planned_targets"], [])
        sources = {
            (source["snapshot"], source["path"])
            for source in record["sources"]
        }
        self.assertIn(("last-frontier", "Docs/CharacterGenerator.md"), sources)
        self.assertIn(("last-frontier", "Docs/Particles.md"), sources)
        self.assertIn(("last-frontier", ".vscode/tasks.json"), sources)
        self.assertIn(("last-frontier", "CMakeLists.txt"), sources)
        self.assertIn(("fonline-tla", "README.md"), sources)
        self.assertIn("negative evidence", record["decision"])


if __name__ == "__main__":
    unittest.main()
