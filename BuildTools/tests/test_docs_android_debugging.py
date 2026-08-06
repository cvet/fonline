from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/how-to/platforms/android-debugging.md"
RUSSIAN_PATH = "Docs/ru/how-to/platforms/android-debugging.md"
LEGACY_PATH = "Docs/AndroidDebugging.md"


class AndroidDebuggingDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_covers_build_package_device_and_release_boundaries(self) -> None:
        guide = self._read(GUIDE_PATH)

        for heading in (
            "## Contract status",
            "## Support and qualification matrix",
            "## Prepare the host and workspace",
            "## Build and stage a local debug package",
            "## Assemble and inspect the debug APK",
            "## Android package configuration",
            "## Runtime bootstrap and resource staging",
            "## Discover and select a Wi-Fi ADB device",
            "## Install, launch, stop, and collect logs",
            "## Connect to a development server",
            "## Integrate release packaging",
            "## Device and release acceptance matrix",
            "## Troubleshooting by layer",
            "## Project evidence and extraction rules",
            "## Maintenance triggers",
            "## Validation routes",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "Success at one layer does not qualify the next one.",
            "source-capable",
            "It does not accept a generic command-line extra.",
            "A debug-key APK is always a development artifact.",
            "`Updater::CanSelfUpdateNativeModules()` returns false for Android",
            "The Engine CI matrix intentionally does not supply this device evidence.",
        ):
            self.assertIn(contract, guide)

    def test_platform_support_and_pins_match_buildtools_and_checked_matrix(self) -> None:
        buildtools = self._read("BuildTools/buildtools.py")
        support = json.loads(self._read("BuildTools/SupportMatrix.json"))
        workflow = self._read(".github/workflows/validate.yml")

        self.assertIn("ANDROID_PLATFORMS = ('android-arm32', 'android-arm64', 'android-x86')", buildtools)
        self.assertIn("'android-arm32': 'armeabi-v7a'", buildtools)
        self.assertIn("'android-arm64': 'arm64-v8a'", buildtools)
        self.assertIn("'android-x86': 'x86'", buildtools)
        self.assertIn("'build-tools;34.0.0'", buildtools)
        self.assertIn("'platforms;android-35'", buildtools)
        self.assertEqual(self._read("ThirdParty/android-sdk").strip(), "14742923")
        self.assertEqual(self._read("ThirdParty/android-ndk").strip(), "android-ndk-r29")
        self.assertEqual(self._read("ThirdParty/android-api").strip(), "23")

        platforms = {entry["id"]: entry for entry in support["platforms"]}
        self.assertEqual(platforms["android-arm-clients"]["level"], "build_gated")
        self.assertEqual(platforms["android-x86-client"]["level"], "source_capable")
        self.assertEqual(platforms["android-x86-client"]["ci_validation_targets"], [])
        self.assertIn("android-arm32-client", workflow)
        self.assertIn("android-arm64-client", workflow)

    def test_local_package_flow_matches_buildtools_and_packager(self) -> None:
        buildtools = self._read("BuildTools/buildtools.py")
        package = self._read("BuildTools/package.py")

        for contract in (
            "return resolve_android_debug_root(env) / f'{devname}-Client-{config}-Android'",
            "'-platform',\n\t\t'Android'",
            "'-pack',\n\t\t'Raw'",
            "for config in configs:",
        ):
            self.assertIn(contract, buildtools)

        for contract in (
            "dst_so = os.path.join(jni_libs_dir, 'libmain.so')",
            "shutil.move(client_res_source, assets_res_dir)",
            "version_name = self.args.buildhash[:8] if self.args.buildhash else '1.0'",
            "build_task = 'assembleDebug' if self.has_pack('Debug') else 'assembleRelease'",
            "assert self.has_pack('Debug') or not selected_apk_file.endswith('-unsigned.apk')",
            "if not self.has_pack('Raw'):",
        ):
            self.assertIn(contract, package)

    def test_config_gradle_and_manifest_claims_match_templates(self) -> None:
        settings = self._read("Source/Common/Settings.inc")
        package = self._read("BuildTools/package.py")
        gradle = self._read("BuildTools/android-project/app/build.gradle")
        manifest = self._read("BuildTools/android-project/app/src/main/AndroidManifest.xml")

        for setting in (
            'FIXED_SETTING(string, Android, PackageName, "com.fonline.app")',
            "FIXED_SETTING(int32_t, Android, VersionCode, 1)",
            "FIXED_SETTING(int32_t, Android, MinSdk, 23)",
            "FIXED_SETTING(int32_t, Android, TargetSdk, 35)",
            "FIXED_SETTING(int32_t, Android, CompileSdk, 35)",
            'FIXED_SETTING(string, Android, ScreenOrientation, "landscape")',
        ):
            self.assertIn(setting, settings)

        self.assertIn("ANDROID_MANIFEST_METADATA_CONFIG_PREFIX", package)
        self.assertIn("ANDROID_GRADLE_MAVEN_REPOSITORY_CONFIG_PREFIX", package)
        self.assertIn("ANDROID_GRADLE_DEPENDENCY_CONFIG_PREFIX", package)
        self.assertIn("ANDROID_JAVA_SOURCE_CONFIG_PREFIX", package)
        self.assertIn("Android.JavaSource.* must not override", package)
        self.assertIn("Android.Icon must point to a PNG file", package)

        self.assertIn("signingConfig = hasReleaseSigning ? signingConfigs.release : signingConfigs.debug", gradle)
        self.assertIn("abortOnError = false", gradle)
        self.assertIn("noCompress += ['zip']", gradle)
        self.assertIn('android:glEsVersion="0x00030000"', manifest)
        self.assertIn('android.permission.INTERNET', manifest)
        self.assertIn('android:name=".FOnlineActivity"', manifest)

    def test_activity_claims_match_runtime_bootstrap_and_resource_copy(self) -> None:
        activity = self._read(
            "BuildTools/android-project/app/src/main/java-template/FOnlineActivity.java"
        )

        for contract in (
            'return new String[]{\n            "main"',
            'args.add("--ApplySubConfig")',
            'args.add("--Baking.ClientResources")',
            'args.add("--Baking.CacheResources")',
            'getIntent().getStringExtra("server_host")',
            'args.add("--ClientNetwork.ServerHost")',
            'final File revisionFile = new File(runtimeRoot, ".asset_revision")',
            'return Long.toString(packageInfo.lastUpdateTime)',
            '!new File(resourcesDir, "Metadata.zip").isFile()',
            'deleteRecursively(resourcesDir)',
            'copyAssetTree("Resources", resourcesDir)',
            'writeSmallTextFile(revisionFile, assetRevision)',
        ):
            self.assertIn(contract, activity)

        updater = self._read("Source/Client/Updater.cpp")
        self.assertIn("case UpdatePlatform::Android:", updater)
        self.assertIn("return false;", updater)

    def test_device_helper_claims_match_endpoint_and_adb_commands(self) -> None:
        helper = self._read("BuildTools/android_device.py")

        for contract in (
            "MDNS_CONNECT_MARKERS = ('_adb-tls-connect._tcp', '_adb._tcp')",
            "return workspace_root / 'android-debug' / 'device-endpoint.txt'",
            "return endpoint + ':5555'",
            "if not sys.stdin.isatty():",
            "[adb_path, '-s', endpoint, 'install', '-r', apk_path]",
            "'am', 'force-stop', package_name",
            "'--es',\n\t\t'server_host'",
            "DEFAULT_LOGCAT_FILTERS = ['-s', 'SDL:V', 'FOnline:V', 'LF:V', '*:E']",
        ):
            self.assertIn(contract, helper)

        self.assertIn("discover_parser = subparsers.add_parser('discover'", helper)
        self.assertNotIn("pair_parser =", helper)
        self.assertNotIn("logcat_parser.add_argument('--filter'", helper)

    def test_project_evidence_is_exact_and_non_normative(self) -> None:
        model = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(
            value
            for value in model["records"]
            if value["id"] == "packaging-platform-and-release"
        )
        sources = {
            (source["snapshot"], source["path"])
            for source in record["sources"]
        }

        for source in (
            ("last-frontier", ".vscode/tasks.json"),
            ("last-frontier", "CMakeLists.txt"),
            ("last-frontier", "LastFrontier.fomain"),
            ("last-frontier", ".github/workflows/cross-platform-tests.yml"),
            ("fonline-tla", "TLA.fomain"),
            ("fonline-tla", "CMakePresets.json"),
            ("fonline-tla", ".github/workflows/build.yml"),
        ):
            self.assertIn(source, sources)

        guide = self._read(GUIDE_PATH)
        self.assertIn(GUIDE_PATH, record["engine_targets"])
        self.assertIn("no official APK artifact", record["decision"])
        self.assertIn("official production package declarations currently do not emit an Android APK", guide)
        self.assertIn("no Android APK/device qualification observed by this audit", guide)

    def test_translation_route_maintenance_and_ci_are_complete(self) -> None:
        english = self._read(GUIDE_PATH)
        russian = self._read(RUSSIAN_PATH)
        legacy_page = self._read(LEGACY_PATH)
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        workflow = self._read(".github/workflows/validate.yml")
        maintenance = self._read("Docs/en/contributing/documentation/index.md")

        self.assertIn("document_id: android-debugging", russian)
        for heading in (
            "## Статус контракта",
            "## Матрица поддержки и квалификации",
            "## Runtime bootstrap и staging ресурсов",
            "## Матрица device- и release-приёмки",
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
            group
            for group in manifest["site_delivery"]["navigation"]
            if group["id"] == "quality"
        )
        self.assertEqual(document["id"], "android-debugging")
        self.assertEqual(document["owner"], "platform")
        self.assertEqual(document["classification"]["translation"], "required")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["redirect_to"], "android-debugging")
        self.assertIn("android-debugging", quality["document_ids"])
        self.assertIn("BuildTools/tests/test_docs_android_debugging.py", workflow)
        self.assertIn("FOnlineActivity", maintenance)
        self.assertIn("platforms/android-debugging.md", maintenance)

        for heading in re.findall(r"^(##+ .+)$", english, re.MULTILINE):
            self.assertIn(heading, legacy_page)


if __name__ == "__main__":
    unittest.main()
