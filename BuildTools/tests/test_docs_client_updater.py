from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/explanation/runtime/client-updater.md"
RUSSIAN_PATH = "Docs/ru/explanation/runtime/client-updater.md"
LEGACY_PATH = "Docs/ClientUpdater.md"


class ClientUpdaterDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_runtime_abi_and_updater_generation_match_source(self) -> None:
        runtime_api = self._read("Source/Client/ClientRuntimeApi.h")
        common = self._read("Source/Common/Common.h")
        guide = self._read(GUIDE_PATH)

        self.assertIn("FO_CLIENT_RUNTIME_HOST_ABI_VERSION = 3", runtime_api)
        self.assertIn("FO_UPDATER_VERSION = 2", common)
        self.assertIn("`FO_CLIENT_RUNTIME_HOST_ABI_VERSION = 3`", guide)
        self.assertIn("`FO_UPDATER_VERSION = 2`", guide)
        for result in ("Shutdown", "ReloadRequested", "FatalError"):
            self.assertIn(result, runtime_api)
            self.assertIn(f"`{result}`", guide)
        self.assertIn("does not load that module again in the running", guide)

    def test_host_selection_restart_and_installed_selector_are_covered(self) -> None:
        host = self._read("Source/Applications/ClientApp.cpp")
        guide = self._read(GUIDE_PATH)

        for switch in (
            "--ClientLibPath",
            "--ClientLibCompatibilityVersion",
            "--ForceEmbeddedRuntime",
        ):
            self.assertIn(switch, host)
            self.assertIn(f"`{switch}`", guide)

        for marker in (
            "PromoteStagedReloadForRestart",
            "ApplyStagedBinaryUpdate",
            "GetInstalledClientRuntimeBootstrapPath",
            "WriteClientRuntimeBootstrapTarget",
        ):
            self.assertIn(marker, host)
            self.assertIn(marker, guide)

        self.assertIn("promotes the staged file, exits", guide)
        self.assertIn("Missing, oversized, relative, newline-containing", guide)
        self.assertIn("writes it to the installed-client bootstrap selector", guide)

    def test_backend_signatures_and_stateless_offset_protocol_are_exact(self) -> None:
        header = self._read("Source/Server/UpdaterBackend.h")
        backend = self._read("Source/Server/UpdaterBackend.cpp")
        guide = self._read(GUIDE_PATH)

        signatures = (
            "void ProcessUpdateFile(ptr<Player> player, int32_t update_file_max_portion_size);",
            "auto GetUpdateDescriptor(string_view binary_target_name) const -> const_span<uint8_t>;",
        )
        for signature in signatures:
            self.assertIn(signature, header)
            self.assertIn(signature, guide)

        self.assertIn("auto start_offset = in_buf->Read<uint64_t>();", backend)
        self.assertIn("file.seekg(numeric_cast<std::streamoff>(start_offset)", backend)
        self.assertIn("start_offset: uint64", guide)
        self.assertIn("without server-side state", guide)
        self.assertIn("internal engine surface, not a stable public API", guide)

    def test_resume_hash_cache_and_package_payload_contract_are_current(self) -> None:
        updater = self._read("Source/Client/Updater.cpp")
        package = self._read("BuildTools/package.py")
        guide = self._read(GUIDE_PATH)

        cache_expression = (
            'strex("{}-{:016x}.hash", strex(file_path).extract_file_name(), '
            "hashing::hash<string_view> {}(file_path)).str()"
        )
        self.assertIn(cache_expression, updater)
        self.assertIn("`<basename>-<path-hash>.hash`", guide)
        self.assertIn("`hashing::hash<string_view>` over the full path string", guide)
        self.assertNotIn("The key is `<basename>.hash`", guide)

        for marker in (
            "package_all_client_runtime_update_payloads",
            "copy_runtime_pdb",
            "build_runtime_update_target_name",
            "extract_binary_entry_postfix",
        ):
            self.assertIn(marker, package)
            self.assertIn(marker, guide)
        self.assertIn("Sign the final patched host and runtime artifacts", guide)
        self.assertIn("byte-for-byte the artifact", guide)

    def test_project_evidence_supports_release_practices(self) -> None:
        evidence = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(
            item
            for item in evidence["records"]
            if item["id"] == "updater-secrets-security-and-recovery"
        )
        sources = {(item["snapshot"], item["path"]) for item in record["sources"]}
        for source in (
            ("last-frontier", "LastFrontier.fomain"),
            ("last-frontier", "Tools/CodeSign/README.md"),
            ("last-frontier", "Tools/PipelineTests/test_updater_smoke.py"),
            ("last-frontier", "Tools/PipelineTests/test_updater_installed_selector.py"),
            ("last-frontier", "Tools/PipelineTests/test_updater_postfix_variants.py"),
            ("last-frontier", "Tools/PipelineTests/test_server_stages_windows_pdb.py"),
            ("last-frontier", "Tools/PipelineTests/test_updater_restart_windows.py"),
            ("fonline-tla", "TLA.fomain"),
        ):
            self.assertIn(source, sources)
        self.assertIn(GUIDE_PATH, record["engine_targets"])

        guide = self._read(GUIDE_PATH)
        self.assertIn("## Embedding-project practices", guide)
        self.assertIn("Settings present in a project config", guide)
        self.assertIn("are not evidence that these paths work", guide)
        self.assertIn("Keep portable and installed clients as separate acceptance lanes", guide)
        self.assertIn("### Release acceptance matrix", guide)

    def test_russian_mirror_keeps_the_complete_updater_contract(self) -> None:
        guide = self._read(GUIDE_PATH)
        russian = self._read(RUSSIAN_PATH)

        self.assertGreaterEqual(len(russian.encode("utf-8")), len(guide.encode("utf-8")) * 0.75)
        self.assertEqual(
            len(re.findall(r"^#{2,3} ", russian, re.MULTILINE)),
            len(re.findall(r"^#{2,3} ", guide, re.MULTILINE)),
        )
        for contract in (
            "`FO_CLIENT_RUNTIME_HOST_ABI_VERSION = 3`",
            "`FO_UPDATER_VERSION = 2`",
            "`PromoteStagedReloadForRestart`",
            "`<basename>-<path-hash>.hash`",
            "`const_span<uint8_t>`",
            "`ServerNetwork.UpdateFilesInMemory`",
            "`PlatformBinaries/<target>/`",
            "`ApplicationShutdownHook()`",
        ):
            self.assertIn(contract, russian)

    def test_canonical_russian_and_legacy_routes_are_owned(self) -> None:
        documents = json.loads(self._read("Docs/documentation-manifest.json"))["documents"]
        canonical = documents[GUIDE_PATH]
        legacy = documents[LEGACY_PATH]

        self.assertEqual(
            (canonical["id"], canonical["state"], canonical["disposition"]),
            ("client-updater", "current", "retain"),
        )
        self.assertEqual(
            (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
            ("redirect", "replace", "client-updater"),
        )
        self.assertTrue((ENGINE_ROOT / RUSSIAN_PATH).is_file())

        canonical_text = self._read(GUIDE_PATH)
        legacy_text = self._read(LEGACY_PATH)
        for heading in re.findall(r"^#{2,3} .+$", canonical_text, re.MULTILINE):
            self.assertIn(heading, legacy_text)
        self.assertIn("en/explanation/runtime/client-updater.md#", legacy_text)
        self.assertIn("ru/explanation/runtime/client-updater.md", legacy_text)


if __name__ == "__main__":
    unittest.main()
