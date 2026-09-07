from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class DocumentationScriptingFoundationsTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_scripting_guide_lists_the_complete_core_library(self) -> None:
        guide = self._read("Docs/en/explanation/scripting-runtime/index.md")
        section = guide.split("## Core scripts", 1)[1].split("\n## ", 1)[0]
        documented = set(re.findall(r"`([A-Za-z][A-Za-z0-9]*\.fos)`", section))
        actual = {
            path.name
            for path in (
                ENGINE_ROOT / "Source/Scripting/AngelScript/CoreScripts"
            ).glob("*.fos")
        }
        self.assertEqual(documented, actual)
        for marker in (
            "Source/Tests/Test_AngelScriptCall.cpp",
            "Source/Tests/Test_ClientDataValidation.cpp",
            "Source/Tests/Test_EntityLifecycle.cpp",
            "Source/Tests/Test_EntitySync.cpp",
            "Source/Tests/Test_MetadataBaker.cpp",
            "Source/Tests/Test_NetBuffer.cpp",
        ):
            self.assertIn(marker, guide)

    def test_method_ownership_map_matches_every_export_file(self) -> None:
        guide = self._read("Docs/en/reference/script-api/method-ownership.md")
        documented = set(
            re.findall(
                r"^### `Source/Scripting/([^`]+ScriptMethods\.cpp)`$",
                guide,
                flags=re.MULTILINE,
            )
        )
        actual = {
            path.name
            for path in (ENGINE_ROOT / "Source/Scripting").glob("*ScriptMethods.cpp")
        }
        self.assertEqual(documented, actual)
        mapper = self._read("Source/Scripting/MapperGlobalScriptMethods.cpp")
        self.assertIn("SaveMapperScreenshot", guide)
        self.assertIn("Mapper_Game_SaveMapperScreenshot", mapper)
        self.assertNotIn("RequestMapperWindowScreenshot", guide)

    def test_remote_call_validation_precedes_cover_and_dispatch(self) -> None:
        guide = self._read("Docs/en/reference/scripting/remote-calls.md")
        server = self._read("Source/Server/Server.cpp")
        validator = self._read("Source/Server/ClientDataValidation.cpp")
        process = server[server.index("void ServerEngine::Process_RemoteCall") :]

        self.assertLess(
            process.index("ValidateInboundRemoteCallData"),
            process.index("ctx->SyncEntity(player)"),
        )
        self.assertLess(
            process.index("ctx->SyncEntity(player)"),
            process.index("HandleInboundRemoteCall"),
        )
        for marker in (
            "ValidateInboundRemoteCallData()",
            "requires complete payload consumption",
            "Source/Tests/Test_ClientDataValidation.cpp",
            "Source/Tests/Test_NetBuffer.cpp",
        ):
            self.assertIn(marker, guide)
        for marker in (
            "reader.VerifyEnd();",
            "reader.VerifyPayloadCount",
            "Negative array size",
            "Negative dict size",
        ):
            self.assertIn(marker, validator)

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        manifest = json.loads(
            self._read("Docs/documentation-manifest.json")
        )["documents"]
        pairs = (
            (
                "Docs/en/explanation/scripting-runtime/index.md",
                "Docs/Scripting.md",
                "scripting-runtime",
            ),
            (
                "Docs/en/how-to/scripting/lifecycle-and-concurrency.md",
                "Docs/ScriptLifecycleAndConcurrency.md",
                "script-lifecycle-concurrency",
            ),
            (
                "Docs/en/reference/scripting/remote-calls.md",
                "Docs/RemoteCalls.md",
                "remote-calls",
            ),
            (
                "Docs/en/reference/script-api/method-ownership.md",
                "Docs/ScriptMethodsMap.md",
                "script-methods-map",
            ),
        )

        for canonical_path, legacy_path, document_id in pairs:
            canonical = manifest[canonical_path]
            legacy = manifest[legacy_path]
            self.assertEqual(canonical["id"], document_id)
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertEqual(legacy["target"], canonical_path)


if __name__ == "__main__":
    unittest.main()
