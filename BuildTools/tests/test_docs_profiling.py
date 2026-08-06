from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent


class ProfilingDocumentationTests(unittest.TestCase):
    def test_build_configurations_pin_tracy_modes(self) -> None:
        source = (
            ENGINE_ROOT / "BuildTools/cmake/stages/Init.cmake"
        ).read_text(encoding="utf-8")

        for declaration in (
            "AddConfiguration(Profiling_Total RelWithDebInfo)",
            "AddConfiguration(Profiling_OnDemand RelWithDebInfo)",
            "AddConfiguration(Debug_Profiling_Total Debug)",
            "AddConfiguration(Debug_Profiling_OnDemand Debug)",
        ):
            self.assertIn(declaration, source)
        self.assertIn(
            "SetValue(expr_TracyEnabled "
            "$<CONFIG:Profiling_Total,Profiling_OnDemand,"
            "Debug_Profiling_Total,Debug_Profiling_OnDemand>)",
            source,
        )
        self.assertIn(
            "SetValue(expr_TracyOnDemand "
            "$<CONFIG:Profiling_OnDemand,Debug_Profiling_OnDemand>)",
            source,
        )

    def test_tracy_client_wiring_is_pinned(self) -> None:
        source = (
            ENGINE_ROOT / "BuildTools/cmake/stages/ThirdParty.cmake"
        ).read_text(encoding="utf-8")

        for marker in (
            "$<${expr_TracyEnabled}:TRACY_ENABLE>",
            "$<${expr_TracyOnDemand}:TRACY_ON_DEMAND>",
            "FO_TRACY=${expr_TracyEnabled}",
            "SetCacheValues(TRACY_STATIC ON)",
            "AppendList(FO_ESSENTIALS_LIBS TracyClient)",
        ):
            self.assertIn(marker, source)

    def test_vendored_version_and_pruned_tool_boundary_are_current(self) -> None:
        version = (
            ENGINE_ROOT
            / "ThirdParty/tracy/public/common/TracyVersion.hpp"
        ).read_text(encoding="utf-8")
        values = {
            name: int(value)
            for name, value in re.findall(
                r"enum \{ (Major|Minor|Patch) = (\d+) \};", version
            )
        }

        self.assertEqual(values, {"Major": 0, "Minor": 13, "Patch": 1})
        self.assertFalse((ENGINE_ROOT / "ThirdParty/tracy/capture").exists())
        self.assertFalse((ENGINE_ROOT / "ThirdParty/tracy/csvexport").exists())

        guide = (
            ENGINE_ROOT / "Docs/en/how-to/quality/profiling.md"
        ).read_text(encoding="utf-8")
        normalized_guide = " ".join(guide.split())
        self.assertIn("vendors Tracy `0.13.1`", guide)
        self.assertIn("--branch v0.13.1", guide)
        self.assertIn("intentionally pruned", normalized_guide)
        self.assertIn("capture protocol must match", normalized_guide)
        self.assertIn(
            "https://github.com/wolfpld/tracy/blob/v0.13.1/capture/src/capture.cpp",
            guide,
        )
        self.assertIn(
            "https://github.com/wolfpld/tracy/blob/v0.13.1/csvexport/src/csvexport.cpp",
            guide,
        )

    def test_instrumentation_surfaces_are_current(self) -> None:
        expected_markers = {
            "Source/Frontend/ApplicationInit.cpp": "TracySetProgramName("
            "FO_NICE_NAME)",
            "Source/Frontend/Application.cpp": "FrameMark;",
            "Source/Frontend/ApplicationHeadless.cpp": "FrameMark;",
            "Source/Client/Client.cpp": 'TracyPlot("Client FPS"',
            "Source/Server/Server.cpp": 'TracyPlot("Server jobs per second"',
            "Source/Essentials/Logging.cpp": "TracyMessage(",
            "Source/Essentials/MemorySystem.cpp": "TracyAlloc(",
            "Source/Scripting/AngelScript/AngelScriptContext.cpp": (
                "___tracy_emit_zone_begin_alloc("
            ),
        }

        for path, marker in expected_markers.items():
            source = (ENGINE_ROOT / path).read_text(
                encoding="utf-8", errors="replace"
            )
            self.assertIn(marker, source, path)

        stack_trace = (
            ENGINE_ROOT / "Source/Essentials/StackTrace.h"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "#define FO_STACK_TRACE_ENTRY() ZoneScoped", stack_trace
        )
        self.assertIn(
            "#define FO_STACK_TRACE_ENTRY_NAMED(name) ZoneScopedN(name)",
            stack_trace,
        )

    def test_guide_covers_reproducible_client_and_server_workflows(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/quality/profiling.md"
        ).read_text(encoding="utf-8")
        normalized_guide = " ".join(guide.split())

        for heading in (
            "## What the Engine instruments",
            "## Build configurations",
            "## Prepare matching Tracy tools",
            "## Choose one measurement boundary",
            "## Build a profiled sample",
            "## Capture a client",
            "## Capture a server",
            "## Design a reproducible workload",
            "## Analyze a capture",
            "## Add focused instrumentation",
            "## Common failure modes",
            "## Project-owned automation",
            "## Validation workflow",
            "## Maintenance",
        ):
            self.assertIn(heading, guide)

        for required in (
            "instrument only the process being measured",
            "profiled client should connect to a regular server",
            "profiled server should be driven by a regular client",
            "does not add renderer GPU zones or Tracy lock wrappers",
            "at least three comparable attempts",
            "Examples\\MinimalMultiplayer\\Build\\windows",
            "-ApplyConfig ..\\..\\FOnlineMinimalMultiplayer.fomain",
            "--truncated_mean=95",
            "Scene IDs, config names, executable names",
        ):
            self.assertIn(required, normalized_guide)

        for project_only in (
            "Last Frontier",
            "LF_Client",
            "LF_Server",
            "Tools/ClientProfiling",
            "ProfilingSceneLaunch",
            "PerfClientCrowd",
        ):
            self.assertNotIn(project_only, guide)

        sample_readme = (
            ENGINE_ROOT / "Examples/MinimalMultiplayer/README.md"
        ).read_text(encoding="utf-8")
        self.assertIn(r"Set-Location Build\windows", sample_readme)
        self.assertIn(
            r"-ApplyConfig ..\..\FOnlineMinimalMultiplayer.fomain",
            sample_readme,
        )

    def test_manifest_navigation_and_entry_points_route_profiling(self) -> None:
        manifest = json.loads(
            (
                ENGINE_ROOT / "Docs/documentation-manifest.json"
            ).read_text(encoding="utf-8")
        )
        document = manifest["documents"]["Docs/en/how-to/quality/profiling.md"]

        self.assertEqual(document["id"], "profiling")
        self.assertEqual(document["owner"], "quality")
        self.assertEqual(document["state"], "current")
        self.assertEqual(document["disposition"], "retain")
        self.assertEqual(
            document["target"], "Docs/en/how-to/quality/profiling.md"
        )
        self.assertIn("game-developer", document["audiences"])
        self.assertIn("ai-agent", document["audiences"])
        for path in document["sources"]:
            self.assertTrue((ENGINE_ROOT / path).exists(), path)

        legacy = manifest["documents"]["Docs/Profiling.md"]
        self.assertEqual(legacy["id"], "legacy-profiling-route")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], "profiling")

        quality = next(
            group
            for group in manifest["site_delivery"]["navigation"]
            if group["id"] == "quality"
        )
        self.assertIn("profiling", quality["document_ids"])

        expected_links = {
            "AGENTS.md": "Docs/en/how-to/quality/profiling.md",
            "README.md": "[Profiling](Docs/en/how-to/quality/profiling.md)",
            "Docs/en/index.md": "[Profiling](how-to/quality/profiling.md)",
            "Docs/en/contributing/testing/index.md": "[Profiling](../../how-to/quality/profiling.md)",
            "Docs/en/troubleshooting/debugging.md": "[Profiling](../how-to/quality/profiling.md)",
        }
        for path, marker in expected_links.items():
            text = (ENGINE_ROOT / path).read_text(encoding="utf-8")
            self.assertIn(marker, text, path)

    def test_ci_and_maintenance_route_profiling_changes(self) -> None:
        workflow = (
            ENGINE_ROOT / ".github/workflows/validate.yml"
        ).read_text(encoding="utf-8")
        maintenance = (
            ENGINE_ROOT / "Docs/en/contributing/documentation/index.md"
        ).read_text(encoding="utf-8")

        self.assertIn(
            "python3 BuildTools/tests/test_docs_profiling.py", workflow
        )
        for marker in (
            "`Profiling_*`",
            "`FO_TRACY`",
            "`TracyVersion.hpp`",
            "[Profiling](../../how-to/quality/profiling.md)",
            "`test_docs_profiling.py`",
        ):
            self.assertIn(marker, maintenance)


if __name__ == "__main__":
    unittest.main()
