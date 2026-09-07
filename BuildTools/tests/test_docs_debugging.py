from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/troubleshooting/debugging.md"
RUSSIAN_PATH = "Docs/ru/troubleshooting/debugging.md"
LEGACY_PATH = "Docs/Debugging.md"


class DebuggingDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_covers_native_script_crash_and_project_boundaries(self) -> None:
        guide = self._read(GUIDE_PATH)

        for heading in (
            "## Contract status",
            "## Evidence layers and support matrix",
            "## Build configurations and symbols",
            "## Native debugging",
            "## Debugger detection and debugger breaks",
            "## Visual Studio Visualizers",
            "## Stack Trace Architecture",
            "## AngelScript debugger",
            "## Debugger integration in an embedding project",
            "## Project launch-profile checklist",
            "## Client host and runtime validation",
            "## Project evidence and extraction rules",
            "## Troubleshooting by layer",
            "## Maintenance triggers",
            "## Validation checklist",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "Debug symbols are not debug semantics",
            "Late attach can observe process state, but it does not refresh",
            "The Engine writes crash diagnostics to its log.",
            "The AngelScript debugger is independent of `IsRunInDebugger`",
            "not a production-distributed editor product",
            "the compounds do not themselves enable `Script.DebuggerEnabled`",
        ):
            self.assertIn(contract, guide)

    def test_native_build_debugger_detection_and_visualizers_match_source(self) -> None:
        init = self._read("BuildTools/cmake/stages/Init.cmake")
        core = self._read("Source/Essentials/BasicCore.cpp")
        engine_sources = self._read("BuildTools/cmake/stages/EngineSources.cmake")
        third_party = self._read("BuildTools/cmake/stages/ThirdParty.cmake")

        for contract in (
            "AddConfiguration(Release_Debugging RelWithDebInfo)",
            "SetValue(expr_DebugInfo $<NOT:$<CONFIG:MinSizeRel>>)",
            "$<$<CONFIG:Release_Debugging>:/dynamicdeopt>",
            "$<${expr_DebugInfo}:/Zi>",
            "$<IF:${expr_DebugInfo},/DEBUG:FULL,/DEBUG:NONE>",
            "$<$<OR:${expr_DebugBuild},$<CONFIG:RelWithDebInfo>>:/JMC>",
        ):
            self.assertIn(contract, init)

        for contract in (
            "std::call_once(RunInDebuggerOnce",
            "::IsDebuggerPresent()",
            'line.starts_with("TracerPid:")',
            "P_TRACED",
            "::DebugBreak()",
            "__builtin_debugtrap()",
            "raise(SIGTRAP)",
        ):
            self.assertIn(contract, core)

        self.assertIn("BuildTools/natvis/essentials.natvis", engine_sources)
        self.assertIn("BuildTools/natvis/fonline.natjmc", engine_sources)
        for visualizer in ("glm.natvis", "imgui.natvis", "small_vector.natvis", "ufbx.natvis"):
            self.assertIn(visualizer, third_party)

    def test_stack_exception_and_crash_claims_match_source_and_tests(self) -> None:
        stack_h = self._read("Source/Essentials/StackTrace.h")
        stack_cpp = self._read("Source/Essentials/StackTrace.cpp")
        base_logging_h = self._read("Source/Essentials/BaseLogging.h")
        context = self._read("Source/Scripting/AngelScript/AngelScriptContext.cpp")
        self_test = self._read("Source/Common/DiagnosticSelfTest.cpp")
        exception_cpp = self._read("Source/Essentials/ExceptionHandling.cpp")
        stack_test = self._read("Source/Tests/Test_StackTrace.cpp")
        exception_test = self._read("Source/Tests/Test_ExceptionHandling.cpp")

        for contract in (
            "BirthNativeFrames",
            "ScriptLayers",
            "ClearResolvedStackTraceCache()",
            "GetResolvedStackTraceCacheSize()",
        ):
            self.assertIn(contract, stack_h)
        self.assertIn("SafeWriteStackTrace", base_logging_h)
        self.assertIn("FindLayerNativeAnchor", stack_cpp)
        self.assertIn('frame.Type == StackTraceFrame::FrameType::Script ? "Script" : "Native"', stack_cpp)
        self.assertIn("CaptureNativeStackFrames(ctx_ext->BirthNativeFrames", context)
        self.assertIn("SetScriptStackTraceProvider", context)
        self.assertIn("FO_MEMORY_SANITIZER", exception_cpp)
        self.assertIn("HAS_NATIVE_TRACE 0", exception_cpp)

        for mode in (
            "main_null_read",
            "main_null_write",
            "main_wild_write",
            "main_stack_overflow",
            "main_fpe",
            "main_abort",
            "main_noexcept_throw",
            "main_throw",
            "main_strong_assert",
            "thread_",
        ):
            self.assertIn(mode, self_test)

        self.assertIn("MultiLevelInterleavingSplicesNativeBetweenLayers", stack_test)
        self.assertIn("BaseEngineException", exception_test)

    def test_angelscript_endpoint_capabilities_and_limits_match_source(self) -> None:
        settings = self._read("Source/Common/Settings.inc")
        backend = self._read("Source/Scripting/AngelScript/AngelScriptBackend.cpp")
        endpoint = self._read("Source/Scripting/AngelScript/AngelScriptDebugger.cpp")

        self.assertIn("FIXED_SETTING(bool, Script, DebuggerEnabled, false)", settings)
        self.assertIn('FIXED_SETTING(string, Script, DebuggerBindHost, "127.0.0.1")', settings)
        self.assertIn("asEP_BUILD_WITHOUT_LINE_CUES, !_settings->DebuggerEnabled", backend)
        self.assertIn("asEP_OPTIMIZE_BYTECODE, !_settings->DebuggerEnabled", backend)

        for contract in (
            "ANGELSCRIPT_DEBUGGER_TCP_PORT_SPAN = 2000",
            "ANGELSCRIPT_DEBUGGER_DISCOVERY_PORT = 43001",
            'ANGELSCRIPT_DEBUGGER_DISCOVERY_PROBE = "fos-debug-discover-v1"',
            'command == "capabilities"',
            'command == "pause"',
            'command == "continue"',
            'command == "next" || command == "stepIn" || command == "stepOut"',
            'command == "setBreakpoints"',
            'command == "stackTrace"',
            'command == "variables"',
            'command == "disconnect"',
            "FrameType::Script",
            "extract_file_name()",
        ):
            self.assertIn(contract, endpoint)

        guide = self._read(GUIDE_PATH)
        for limitation in (
            "no authentication, authorization, confidentiality, or integrity protection",
            "one active TCP debug session at a time",
            "The Engine keys breakpoints by source file basename",
            "The adapter's Globals scope contains attach metadata",
            "Not a live Engine contract",
        ):
            self.assertIn(limitation, guide)

    def test_adapter_delivery_status_is_not_overclaimed(self) -> None:
        package = json.loads(self._read("BuildTools/angelscript-debugger/package.json"))
        guide = self._read(GUIDE_PATH)

        self.assertTrue(package["private"])
        self.assertEqual(package["version"], "0.1.0")
        self.assertEqual(package["scripts"]["test"], "npm run typecheck")
        self.assertFalse((ENGINE_ROOT / "BuildTools/angelscript-debugger/package-lock.json").exists())
        self.assertFalse(any((ENGINE_ROOT / "BuildTools/angelscript-debugger").glob("*.vsix")))
        self.assertIn("no adapter dependency lock file, checked VSIX", guide)
        self.assertIn("sample/mock runtime, not the live Engine endpoint", guide)

    def test_project_evidence_is_exact_positive_and_negative(self) -> None:
        model = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(value for value in model["records"] if value["id"] == "native-debugging-workflows")
        sources = {(source["snapshot"], source["path"]) for source in record["sources"]}

        for source in (
            ("last-frontier", ".vscode/launch.json"),
            ("last-frontier", ".vscode/tasks.json"),
            ("last-frontier", "LastFrontier.fomain"),
            ("last-frontier", "Tools/CiChecks/check_debug_workflows.py"),
            ("last-frontier", "Tools/CiChecks/test_check_debug_workflows.py"),
            ("last-frontier", "Tools/PipelineTests/test_crash_diagnostics_linux.py"),
            ("fonline-tla", ".vscode/launch.json"),
            ("fonline-tla", ".vscode/tasks.json"),
            ("fonline-tla", "TLA.fomain"),
        ):
            self.assertIn(source, sources)

        self.assertEqual(record["disposition"], "promoted")
        self.assertIn(GUIDE_PATH, record["engine_targets"])
        self.assertIn("loopback base bind", record["decision"])
        self.assertIn("wildcard bind", record["decision"])
        self.assertIn("negative evidence", record["decision"])

    def test_translation_route_manifest_maintenance_and_ci_are_complete(self) -> None:
        english = self._read(GUIDE_PATH)
        russian = self._read(RUSSIAN_PATH)
        legacy_page = self._read(LEGACY_PATH)
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        workflow = self._read(".github/workflows/validate.yml")
        maintenance = self._read("Docs/en/contributing/documentation/index.md")

        self.assertIn("document_id: debugging", russian)
        for heading in (
            "## Статус контракта",
            "## Слои evidence и матрица поддержки",
            "## Конфигурации сборки и символы",
            "## Нативная отладка",
            "## Архитектура стека",
            "## Отладчик AngelScript",
            "## Граница безопасности",
            "## Project evidence и правила извлечения",
            "## Триггеры сопровождения",
            "## Checklist проверки",
        ):
            self.assertIn(heading, russian)

        fenced = re.compile(r"```[^\n]*\n.*?```", re.DOTALL)
        self.assertEqual(fenced.findall(english), fenced.findall(russian))

        document = manifest["documents"][GUIDE_PATH]
        legacy = manifest["documents"][LEGACY_PATH]
        quality = next(
            group for group in manifest["site_delivery"]["navigation"] if group["id"] == "quality"
        )
        self.assertEqual(document["id"], "debugging")
        self.assertEqual(document["owner"], "quality")
        self.assertEqual(document["classification"]["translation"], "required")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], "debugging")
        self.assertIn("debugging", quality["document_ids"])
        self.assertIn("BuildTools/tests/test_docs_debugging.py", workflow)
        self.assertIn("AngelScript debugger endpoint/protocol", maintenance)
        self.assertIn("troubleshooting/debugging.md", maintenance)

        for heading in re.findall(r"^(##+ .+)$", english, re.MULTILINE):
            self.assertIn(heading, legacy_page)


if __name__ == "__main__":
    unittest.main()
