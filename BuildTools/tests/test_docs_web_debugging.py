from __future__ import annotations

import json
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/how-to/platforms/web-debugging.md"
RUSSIAN_PATH = "Docs/ru/how-to/platforms/web-debugging.md"
LEGACY_PATH = "Docs/WebDebugging.md"


class WebDebuggingDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_covers_build_runtime_hosting_and_release_boundaries(self) -> None:
        guide = self._read(GUIDE_PATH)

        for heading in (
            "## Contract status",
            "## Support and qualification matrix",
            "## Prepare the host and workspace",
            "## Build configurations and Web limits",
            "## Bake, build, and package",
            "## Browser package contract",
            "## Shell arguments and secret boundary",
            "## Serve a local package",
            "## Connect to a project server",
            "## Browser runtime behavior",
            "## Browser diagnostics",
            "## Native updater and redeployment",
            "## Production hosting and security",
            "## Browser and release acceptance matrix",
            "## Troubleshooting by layer",
            "## Project evidence and extraction rules",
            "## Maintenance triggers",
            "## Validation routes",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "Success at one layer does not qualify the next one.",
            "The `smoke_gated` label qualifies the checked fixture under pinned Chromium",
            "The helper does not build or bake.",
            "binds `('', port)`",
            "The audited generic path does not prove automatic write-back",
            "The Engine build lane intentionally does not supply this browser/release evidence.",
        ):
            self.assertIn(contract, guide)

    def test_toolchain_build_flags_and_support_match_source(self) -> None:
        buildtools = self._read("BuildTools/buildtools.py")
        init = self._read("BuildTools/cmake/stages/Init.cmake")
        support = json.loads(self._read("BuildTools/SupportMatrix.json"))
        workflow = self._read(".github/workflows/validate.yml")

        self.assertEqual(self._read("ThirdParty/emscripten").strip(), "6.0.3")
        self.assertIn("'web-packages': (\n\t\t'2',\n\t\t['nodejs', 'default-jre']", buildtools)
        self.assertIn("'web': ['emscripten']", buildtools)
        self.assertIn("return 'emsdk' if platform_name == 'web' else 'direct'", buildtools)
        self.assertIn("SetBuildPlatformInfo(\"Web-wasm\" \"browser\" \"wasm\")", init)
        for flag in (
            "-sSTACK_SIZE=16777216",
            "-sINITIAL_MEMORY=268435456",
            "-sMAXIMUM_MEMORY=4294967296",
            "-sALLOW_MEMORY_GROWTH=1",
            "-sMIN_WEBGL_VERSION=2",
            "-sMAX_WEBGL_VERSION=2",
            "-sFORCE_FILESYSTEM=1",
            "-sDYNAMIC_EXECUTION=0",
            "-sALLOW_UNIMPLEMENTED_SYSCALLS=0",
        ):
            self.assertIn(flag, init)

        platform = next(value for value in support["platforms"] if value["id"] == "web-client")
        self.assertEqual(platform["level"], "smoke_gated")
        self.assertEqual(platform["applications"], ["browser client"])
        self.assertIn("web-showcase-runtime", platform["ci_validation_targets"])
        self.assertIn("real WebGL 2 context", platform["runtime_evidence"])
        self.assertIn("web-client", workflow)
        self.assertIn("web-showcase-runtime", workflow)

    def test_content_showcase_owns_the_reusable_browser_fixture(self) -> None:
        runtime = json.loads(self._read("Examples/ContentShowcase/showcase-web-runtime.json"))
        browser = self._read("Examples/ContentShowcase/capture_showcase_web.mjs")
        runner = self._read("Examples/ContentShowcase/capture_showcase_web.py")

        self.assertEqual(runtime["capture"], {"width": 1280, "height": 800})
        self.assertIn("FOCS_Client.wasm", runtime["required_responses"])
        self.assertIn("showcase_client_resources_loaded", runtime["required_client_markers"])
        self.assertIn('getContext("webgl2")', browser)
        self.assertIn('failure.error !== "net::ERR_ABORTED"', browser)
        self.assertIn("verify_pixels", runner)

    def test_explicit_emsdk_overrides_the_default_workspace(self) -> None:
        sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))
        import buildtools

        with tempfile.TemporaryDirectory() as temporary_directory:
            explicit_emsdk = Path(temporary_directory) / "explicit-emsdk"
            with patch.dict(
                os.environ,
                {
                    "FO_ENGINE_ROOT": str(ENGINE_ROOT),
                    "FO_PROJECT_ROOT": str(ENGINE_ROOT),
                    "FO_WORKSPACE": str(Path(temporary_directory) / "workspace"),
                    "FO_EMSDK": str(explicit_emsdk),
                },
                clear=False,
            ):
                env = buildtools.resolve_env()

        self.assertEqual(env["FO_EMSDK"], str(explicit_emsdk.resolve()))

    def test_package_contract_matches_buildtools_and_packager(self) -> None:
        buildtools = self._read("BuildTools/buildtools.py")
        package = self._read("BuildTools/package.py")

        for contract in (
            "for config in configs:",
            "'Raw+WebServer'",
            "return resolve_web_debug_root(env) / f'{devname}-Client-{config}-Web'",
            "build_hash = resolve_build_hash(env)",
        ):
            self.assertIn(contract, buildtools)

        for contract in (
            "assert self.args.arch == 'wasm'",
            "assert not self.has_pack('NoRes'), 'Web package requires resources'",
            "self.patch_embedded(wasm_output_path)",
            "self.patch_config(wasm_output_path)",
            "'Resources.data'",
            "'--js-output=' + os.path.join(self.target_output_path, 'Resources.js')",
            "'--lz4'",
            "shutil.rmtree(os.path.join(self.target_output_path, self.client_res_dir), True)",
        ):
            self.assertIn(contract, package)

    def test_stock_shell_and_server_claims_match_source(self) -> None:
        shell = self._read("BuildTools/web/default-index.html")
        server = self._read("BuildTools/web/simple-web-server.py")

        for contract in (
            "var params = new URLSearchParams(window.location.search);",
            "args.push('--' + key);",
            "arguments: buildEngineArguments()",
            "FS.chdir('/')",
            "window.foShowError",
            "event.key === 'F8'",
            "window.addEventListener('unhandledrejection'",
            'src="$RESOURCESJS$"',
            'src="$MAINJS$"',
        ):
            self.assertIn(contract, shell)

        for contract in (
            "default=7000",
            "ReusableThreadingTCPServer(('', args.port), handler)",
            "'Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0'",
            "getattr(os, 'fork', None)",
        ):
            self.assertIn(contract, server)
        self.assertNotIn("application/wasm", server)
        self.assertNotIn("Cross-Origin-Opener-Policy", server)

    def test_runtime_transport_storage_and_updater_claims_match_source(self) -> None:
        web = self._read("Source/Common/WebRelated.cpp")
        settings = self._read("Source/Common/Settings.inc")
        sockets = self._read("Source/Client/NetworkClient-Sockets.cpp")
        updater = self._read("Source/Client/Updater.cpp")

        for contract in (
            "window.visualViewport.addEventListener('resize', notifyResize)",
            "document.addEventListener('fullscreenchange', notifyResize)",
            "FS.mount(IDBFS, {}, '/PersistentData')",
            "FS.syncfs(true, function(err) { Module.syncfsDone = 1; })",
            "emscripten_set_main_loop_arg(entry, data, 0, 1)",
            'SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0")',
        ):
            self.assertIn(contract, web)

        self.assertIn('FIXED_SETTING(string, ClientNetwork, WebSocketHost, "localhost")', settings)
        self.assertIn("FIXED_SETTING(int32_t, Network, WebSocketPort, 4001)", settings)
        self.assertIn("FIXED_SETTING(bool, Network, SecuredWebSockets, false)", settings)
        self.assertIn("const string_view host = _settings->WebSocketHost", sockets)
        self.assertIn("const uint16_t port = numeric_cast<uint16_t>(_settings->WebSocketPort)", sockets)
        self.assertIn("#if !FO_IOS && !FO_ANDROID && !FO_WEB", sockets)
        self.assertIn("return \"Web-wasm\"", updater)
        self.assertIn("case UpdatePlatform::Web:", updater)
        self.assertIn("return false;", updater)

    def test_project_evidence_is_exact_and_non_normative(self) -> None:
        model = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(
            value
            for value in model["records"]
            if value["id"] == "packaging-platform-and-release"
        )
        sources = {(source["snapshot"], source["path"]) for source in record["sources"]}

        for source in (
            ("last-frontier", ".github/workflows/cross-platform-tests.yml"),
            ("last-frontier", "Tools/PipelineTests/web_runner.py"),
            ("last-frontier", "Tools/PipelineTests/test_login_enter_game_web.py"),
            ("last-frontier", "Tools/PipelineTests/test_web_token_login_web.py"),
            ("last-frontier", "Tools/PipelineTests/test_web_combat_web.py"),
            ("last-frontier", "LastFrontier.fomain"),
            ("fonline-tla", "CMakePresets.json"),
            ("fonline-tla", "TLA.fomain"),
        ):
            self.assertIn(source, sources)

        guide = self._read(GUIDE_PATH)
        self.assertIn(GUIDE_PATH, record["engine_targets"])
        self.assertIn("required nightly/manual Linux-Web pipeline", guide)
        self.assertIn("no equivalent checked browser-package/Playwright qualification lane", guide)

    def test_translation_route_manifest_maintenance_and_ci_are_complete(self) -> None:
        english = self._read(GUIDE_PATH)
        russian = self._read(RUSSIAN_PATH)
        legacy_page = self._read(LEGACY_PATH)
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        workflow = self._read(".github/workflows/validate.yml")
        maintenance = self._read("Docs/en/contributing/documentation/index.md")

        self.assertIn("document_id: web-debugging", russian)
        for heading in (
            "## Статус контракта",
            "## Матрица поддержки и квалификации",
            "## Контракт браузерного пакета",
            "## Аргументы shell и граница секретов",
            "## Поведение runtime в браузере",
            "## Production-хостинг и безопасность",
            "## Матрица браузерной и release-приёмки",
            "## Project evidence и правила извлечения",
            "## Триггеры сопровождения",
            "## Маршруты проверки",
        ):
            self.assertIn(heading, russian)

        fenced = re.compile(r"```[^\n]*\n.*?```", re.DOTALL)
        self.assertEqual(fenced.findall(english), fenced.findall(russian))

        document = manifest["documents"][GUIDE_PATH]
        legacy = manifest["documents"][LEGACY_PATH]
        quality = next(
            group for group in manifest["site_delivery"]["navigation"] if group["id"] == "quality"
        )
        self.assertEqual(document["id"], "web-debugging")
        self.assertEqual(document["owner"], "platform")
        self.assertEqual(document["classification"]["translation"], "required")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], "web-debugging")
        self.assertIn("web-debugging", quality["document_ids"])
        self.assertIn("BuildTools/tests/test_docs_web_debugging.py", workflow)
        self.assertIn("default-index.html", maintenance)
        self.assertIn("platforms/web-debugging.md", maintenance)

        for heading in re.findall(r"^(##+ .+)$", english, re.MULTILINE):
            self.assertIn(heading, legacy_page)


if __name__ == "__main__":
    unittest.main()
