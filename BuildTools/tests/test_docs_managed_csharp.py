from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
EN_GUIDE = "Docs/en/how-to/scripting/managed-csharp.md"
RU_GUIDE = "Docs/ru/how-to/scripting/managed-csharp.md"


class ManagedCSharpDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guides_cover_the_complete_backend_contract_in_both_locales(self) -> None:
        english = self._read(EN_GUIDE)
        russian = self._read(RU_GUIDE)

        english_headings = (
            "## Contract status",
            "## Ownership and source layout",
            "## Configure the backend",
            "## Generated project and assemblies",
            "## Authoring shape",
            "## Initialization and attributes",
            "## Events, callbacks, timers, and named calls",
            "## Async and continuation scheduling",
            "## Server entity synchronization",
            "## Values, collections, properties, and lifetime",
            "## Runtime loading, isolation, and shutdown",
            "## Build and bake workflow",
            "## Packaging and updating",
            "## Platforms and sanitizers",
            "## Diagnostics and debugging",
            "## Validation matrix",
            "## Migration from AngelScript",
            "## Project documentation boundary",
            "## Maintenance triggers",
            "## Source paths inspected",
        )
        russian_headings = (
            "## Статус контракта",
            "## Владение и layout исходников",
            "## Настройка backend",
            "## Generated project и assemblies",
            "## Форма авторского кода",
            "## Инициализация и attributes",
            "## Events, callbacks, timers и named calls",
            "## Async и планирование continuations",
            "## Серверная синхронизация сущностей",
            "## Значения, коллекции, properties и lifetime",
            "## Runtime loading, изоляция и shutdown",
            "## Сборка и baking",
            "## Packaging и updating",
            "## Платформы и sanitizers",
            "## Диагностика и debugging",
            "## Матрица проверки",
            "## Миграция с AngelScript",
            "## Граница документации проекта",
            "## Триггеры сопровождения",
            "## Проверенные пути исходников",
        )
        for heading in english_headings:
            self.assertIn(heading, english)
        for heading in russian_headings:
            self.assertIn(heading, russian)

        for contract in (
            "FO_MANAGED_SCRIPTING",
            "CompileManagedScripts",
            "ManagedScriptBaker",
            "ManagedScriptTargetFramework",
            "[ModuleInit]",
            "Game.YieldAsync",
            "ScriptSynchronizationContext",
            "[RequiresCover]",
            "[ProvidesCover]",
            "[PreservesCover]",
            "FOSYNC009",
            "[CallableByName]",
            "ManagedRuntime",
            "ManagedLoadContextHost",
            "PrepareManagedRuntimePayload",
            "Test_ManagedScriptBaker",
        ):
            self.assertIn(contract, english)
            self.assertIn(contract, russian)

        self.assertNotIn("FO_MONO_SCRIPTING", english)
        self.assertNotIn("FO_MONO_SCRIPTING", russian)
        self.assertEqual(
            len(re.findall(r"^## ", english, flags=re.MULTILINE)),
            len(re.findall(r"^## ", russian, flags=re.MULTILINE)),
        )

    def test_runtime_lifecycle_and_debugging_route_both_backends(self) -> None:
        runtime = self._read("Docs/en/explanation/scripting-runtime/index.md")
        lifecycle = self._read("Docs/en/how-to/scripting/lifecycle-and-concurrency.md")
        debugging = self._read("Docs/en/troubleshooting/debugging.md")

        for page in (runtime, lifecycle, debugging):
            self.assertIn("Managed C#", page)
            self.assertIn("AngelScript", page)
            self.assertIn("managed-csharp.md", page)
        self.assertIn("Native scripting remains a placeholder", runtime)
        self.assertIn("## Managed C# equivalents", lifecycle)
        self.assertIn("## Managed C# diagnostics and debugging", debugging)
        self.assertRegex(debugging, r"The AngelScript `fos`\s+adapter does not debug C#")

    def test_manifest_navigation_and_ci_make_managed_docs_first_class(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        document = manifest["documents"][EN_GUIDE]
        self.assertEqual(document["id"], "managed-csharp-scripting")
        self.assertEqual(document["state"], "current")
        self.assertEqual(document["classification"]["translation"], "required")
        scripting = next(
            group
            for group in manifest["site_delivery"]["navigation"]
            if group["id"] == "scripting"
        )
        self.assertIn("managed-csharp-scripting", scripting["document_ids"])
        self.assertIn(
            "managed-csharp-scripting",
            manifest["ai_delivery"]["llms"]["start_document_ids"],
        )

        workflow = self._read(".github/workflows/validate.yml")
        self.assertIn("python3 BuildTools/tests/test_docs_managed_csharp.py", workflow)

    def test_documented_source_roots_exist(self) -> None:
        for relative_path in (
            "Source/Scripting/Managed/CoreScripts",
            "Source/Scripting/Managed/ManagedScriptBackend.cpp",
            "Source/Scripting/Managed/ManagedRuntime.cpp",
            "Source/Scripting/Managed/ManagedHost/ManagedLoadContextHost.cs",
            "Source/Scripting/Managed/Analyzers/SyncCoverAnalyzer.cs",
            "Source/Tools/ManagedScriptBaker.cpp",
            "Source/Applications/ManagedScriptBakerApp.cpp",
            "BuildTools/managed_runtime_payload.py",
            "Source/Tests/Test_ManagedScriptBaker.cpp",
        ):
            self.assertTrue((ENGINE_ROOT / relative_path).exists(), relative_path)


if __name__ == "__main__":
    unittest.main()
