from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/contributing/testing/index.md"


class TestingFoundationsDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_test_source_inventory_matches_cmake_and_checkout(self) -> None:
        inventory = json.loads(self._read("Docs/generated/source-inventory.json"))
        generated_files = inventory["engine_tests"]["files"]
        actual_files = sorted(
            path.relative_to(ENGINE_ROOT).as_posix()
            for path in (ENGINE_ROOT / "Source/Tests").glob("Test_*.cpp")
        )
        cmake = self._read("BuildTools/cmake/stages/EngineSources.cmake")
        cmake_files = sorted(
            set(
                re.findall(
                    r'\$\{FO_ENGINE_ROOT\}/(Source/Tests/Test_[^"\n]+\.cpp)',
                    cmake,
                )
            )
        )

        self.assertEqual(inventory["engine_tests"]["test_file_count"], len(actual_files))
        self.assertCountEqual(generated_files, actual_files)
        self.assertCountEqual(cmake_files, actual_files)

    def test_runner_targets_coverage_and_sanitizers_are_documented(self) -> None:
        guide = self._read(GUIDE_PATH)
        testing_app = self._read("Source/Applications/TestingApp.cpp")
        applications = self._read("BuildTools/cmake/stages/Applications.cmake")
        init = self._read("BuildTools/cmake/stages/Init.cmake")
        validate = self._read("BuildTools/validate.sh")
        buildtools = self._read("BuildTools/buildtools.py")
        workflow = self._read(".github/workflows/validate.yml")

        for marker in (
            "FO_TESTING_APP",
            "InitAppForTesting()",
            "Catch::Session().run(argc, argv)",
        ):
            self.assertIn(marker, testing_app)
            self.assertIn(marker, guide)

        for marker in (
            "SetupTestBuild(UnitTests)",
            "SetupTestBuild(CodeCoverage)",
            "GenerateCodeCoverageReport",
            "AnalyzeCodeCoverage",
        ):
            self.assertIn(marker, applications)
            self.assertIn(marker.replace("SetupTestBuild(", "").rstrip(")"), guide)

        for sanitizer in ("address", "memory", "undefined", "thread"):
            command = f"unit-tests-san-{sanitizer}"
            self.assertIn(command, buildtools)
            self.assertIn(command, guide)
        self.assertIn('exec "$PYTHON_BIN" "$CUR_DIR/buildtools.py" validate "$@"', validate)
        self.assertIn("unit-tests-sanitizers", workflow)
        self.assertIn("FO_MSAN_LIBCXX_ROOT", guide)

        portable_calls_start = init.index("SetValue(expr_PortableScriptCalls")
        portable_calls_end = init.index("if(MSVC", portable_calls_start)
        portable_calls = init[portable_calls_start:portable_calls_end]
        self.assertIn("$<BOOL:${FO_CODE_COVERAGE}>", portable_calls)
        self.assertIn("AS_MAX_PORTABILITY", portable_calls)

    def test_canonical_and_legacy_routes_are_explicit(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        canonical = manifest["documents"][GUIDE_PATH]
        legacy = manifest["documents"]["Docs/Testing.md"]

        self.assertEqual(canonical["id"], "testing")
        self.assertEqual(canonical["state"], "current")
        self.assertEqual(canonical["disposition"], "retain")
        self.assertEqual(canonical["classification"]["translation"], "required")
        self.assertEqual(legacy["id"], "legacy-testing-route")
        self.assertEqual(legacy["state"], "redirect")
        self.assertEqual(legacy["disposition"], "replace")
        self.assertEqual(legacy["redirect_to"], "testing")
        self.assertFalse(legacy["classification"]["human"])

        legacy_text = self._read("Docs/Testing.md")
        self.assertIn("> Legacy route.", legacy_text)
        self.assertIn("[English](en/contributing/testing/index.md)", legacy_text)
        self.assertIn("[Русский](ru/contributing/testing/)", legacy_text)


if __name__ == "__main__":
    unittest.main()
