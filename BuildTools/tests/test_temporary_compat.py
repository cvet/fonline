from __future__ import annotations

import datetime
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "temporary_compat.py"
SPEC = importlib.util.spec_from_file_location("temporary_compat", MODULE_PATH)
temporary_compat = importlib.util.module_from_spec(SPEC)
sys.modules["temporary_compat"] = temporary_compat
SPEC.loader.exec_module(temporary_compat)

TODAY = datetime.date(2026, 9, 26)


class TemporaryCompatTests(unittest.TestCase):
    def check(self, files: dict[str, str], today: datetime.date = TODAY, dirs: tuple[str, ...] = ("Source", "Project")):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)

            for rel_path, text in files.items():
                path = root / rel_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")

            previous_dir = os.getcwd()
            os.chdir(root)

            try:
                markers, issues = temporary_compat.collect([Path(scan_dir) for scan_dir in dirs])
            finally:
                os.chdir(previous_dir)

            return markers, issues + temporary_compat.check(markers, today)

    def test_live_markers_report_nothing(self):
        markers, issues = self.check({
            "Source/Server/Connection.cpp": 'FO_TEMPORARY_COMPAT(OldClient, "2026-12-31");\nint x;\n',
            "Source/Server/Connection.h": 'class C\n{\n    FO_TEMPORARY_COMPAT(OldClient, "2026-12-31");\n};\n',
            "Project/Old.cs": '[Event, TemporaryCompat("OldSave", "2026-10-01")]\ninternal static void Run() { }\n',
        })

        self.assertEqual(issues, [])
        self.assertEqual(sorted((m.id, m.path, m.line) for m in markers), [
            ("OldClient", "Source/Server/Connection.cpp", 1),
            ("OldClient", "Source/Server/Connection.h", 3),
            ("OldSave", "Project/Old.cs", 1),
        ])

    def test_an_expired_compatibility_names_every_place(self):
        _, issues = self.check({
            "Source/A.cpp": 'FO_TEMPORARY_COMPAT(OldClient, "2026-09-25");\n',
            "Project/B.cpp": '\nFO_TEMPORARY_COMPAT(OldClient, "2026-09-25");\n',
        })

        self.assertEqual(len(issues), 1)
        self.assertIn("OldClient expired after 2026-09-25", issues[0].message)
        self.assertIn("Source/A.cpp:1", issues[0].message)
        self.assertIn("Project/B.cpp:2", issues[0].message)

    def test_the_date_itself_is_still_live(self):
        _, issues = self.check({"Source/A.cpp": 'FO_TEMPORARY_COMPAT(OldClient, "2026-09-26");\n'})

        self.assertEqual(issues, [])

    def test_places_of_one_compatibility_must_agree_on_the_date(self):
        _, issues = self.check({
            "Source/A.cpp": 'FO_TEMPORARY_COMPAT(OldClient, "2026-12-31");\n',
            "Source/B.cpp": 'FO_TEMPORARY_COMPAT(OldClient, "2026-11-30");\n',
        })

        self.assertEqual(len(issues), 1)
        self.assertIn("carries different dates", issues[0].message)

    def test_a_date_more_than_a_year_ahead_is_refused(self):
        _, issues = self.check({"Project/A.cs": '[TemporaryCompat("Forever", "2099-01-01")]\nclass A { }\n'})

        self.assertEqual(len(issues), 1)
        self.assertIn("more than 366 days ahead", issues[0].message)

    def test_malformed_markers_are_refused(self):
        _, issues = self.check({
            "Source/A.cpp": 'FO_TEMPORARY_COMPAT(OldClient, "31.12.2026");\nFO_TEMPORARY_COMPAT("OldClient", "2026-12-31");\nFO_TEMPORARY_COMPAT(OldClient, "2026-02-30");\n',
            "Project/A.cs": '[TemporaryCompat(id: "OldSave", until: "2026-12-31")]\nclass A { }\n',
        })

        self.assertEqual(sorted((issue.path, issue.line) for issue in issues), [
            ("Project/A.cs", 1),
            ("Source/A.cpp", 1),
            ("Source/A.cpp", 2),
            ("Source/A.cpp", 3),
        ])

    def test_definitions_and_comments_are_not_markers(self):
        markers, issues = self.check({
            "Source/Essentials/BasicCore.h": '#define FO_TEMPORARY_COMPAT(id, until) static_assert(sizeof(until) == 11, "FO_TEMPORARY_COMPAT(Id, date)")\n',
            "Source/Scripting/Attributes.cs": 'public sealed class TemporaryCompatAttribute : Attribute\n{\n    public TemporaryCompatAttribute(string id, string until) { }\n}\n',
            "Source/A.cpp": '// FO_TEMPORARY_COMPAT(OldClient, "2020-01-01");\n/* [TemporaryCompat("Old", "2020-01-01")] */\nauto s = "// not a comment"; FO_TEMPORARY_COMPAT(Live, "2026-12-31");\n',
        })

        self.assertEqual(issues, [])
        self.assertEqual([(m.id, m.line) for m in markers], [("Live", 3)])

    def test_only_the_given_directories_are_scanned(self):
        markers, issues = self.check({"Other/A.cpp": 'FO_TEMPORARY_COMPAT(OldClient, "2020-01-01");\n'})

        self.assertEqual((markers, issues), ([], []))

    def test_the_engine_tree_is_read_without_malformed_markers(self):
        _, issues = temporary_compat.collect(temporary_compat.DEFAULT_INPUTS)

        self.assertEqual(issues, [])

    def test_cmake_markers_include_inactive_branches_and_cmakelists(self):
        markers, issues = self.check({
            "Source/CMakeLists.txt": 'if(FALSE)\n    TemporaryCompat(WebDatabase "2026-12-31")\nendif()\n',
            "Project/Providers.cmake": 'temporarycompat (\n    WebDatabase # Same restriction\n    "2026-12-31"\n)\n',
        })

        self.assertEqual(issues, [])
        self.assertEqual(sorted((m.path, m.line, m.id) for m in markers), [
            ("Project/Providers.cmake", 1, "WebDatabase"),
            ("Source/CMakeLists.txt", 2, "WebDatabase"),
        ])

    def test_cmake_comments_strings_and_definitions_are_not_markers(self):
        markers, issues = self.check({
            "Project/Markers.cmake": '''# TemporaryCompat(Comment "2020-01-01")
#[=[ TemporaryCompat(Block "2020-01-01")
#[[ nested text ]] ]=]
set(example "TemporaryCompat(Quoted \\"2020-01-01\\") # text")
set(example [==[ TemporaryCompat(Bracket "2020-01-01") ]==])
function(TemporaryCompat id until)
endfunction()
TemporaryCompat(Live "2026-12-31") # TemporaryCompat(Comment "2020-01-01")
''',
        })

        self.assertEqual(issues, [])
        self.assertEqual([(m.id, m.line) for m in markers], [("Live", 8)])

    def test_cmake_malformed_markers_are_refused(self):
        _, issues = self.check({
            "Project/Bad.cmake": '''TemporaryCompat("QuotedId" "2026-12-31")
TemporaryCompat(Comma, "2026-12-31")
TemporaryCompat(InvalidDate "2026-02-30")
TemporaryCompat(VariableDate "${until}")
TemporaryCompat(Extra "2026-12-31" extra)
TemporaryCompat(Missing)
''',
        })

        self.assertEqual([issue.line for issue in issues], list(range(1, 7)))

    def test_cmake_and_native_markers_share_expiry_and_date_consistency(self):
        files = {
            "Source/A.cpp": 'FO_TEMPORARY_COMPAT(Shared, "2026-09-25");\n',
            "Project/A.cmake": 'TemporaryCompat(Shared "2026-09-25")\n',
        }
        _, issues = self.check(files)
        self.assertEqual(len(issues), 1)
        self.assertIn("Shared expired after 2026-09-25", issues[0].message)
        self.assertIn("Source/A.cpp:1", issues[0].message)
        self.assertIn("Project/A.cmake:1", issues[0].message)

        files["Project/A.cmake"] = 'TemporaryCompat(Shared "2026-09-26")\n'
        _, issues = self.check(files)
        self.assertEqual(len(issues), 1)
        self.assertIn("carries different dates", issues[0].message)

    def test_input_files_and_overlapping_directories_are_scanned_once(self):
        markers, issues = self.check({
            "Project/CMakeLists.txt": 'TemporaryCompat(ProjectGuard "2026-12-31")\n',
        }, dirs=("Project/CMakeLists.txt", "Project", "Project/CMakeLists.txt"))

        self.assertEqual(issues, [])
        self.assertEqual(len(markers), 1)

    def test_cmake_last_valid_day_and_horizon(self):
        _, issues = self.check({"Project/A.cmake": 'TemporaryCompat(Live "2026-09-26")\n'})
        self.assertEqual(issues, [])
        _, issues = self.check({"Project/A.cmake": 'TemporaryCompat(Forever "2099-01-01")\n'})
        self.assertEqual(len(issues), 1)
        self.assertIn("more than 366 days ahead", issues[0].message)

    def run_cmake(self, source: str) -> subprocess.CompletedProcess[str]:
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake, "CMake is required to validate the temporary compatibility helper")

        with tempfile.TemporaryDirectory() as temp_dir:
            script = Path(temp_dir) / "Check.cmake"
            script.write_text("cmake_minimum_required(VERSION 3.22)\n" + source, encoding="utf-8")
            return subprocess.run([cmake, "-P", str(script)], capture_output=True, text=True, timeout=30)

    def test_cmake_helper_checks_shape_without_calendar_expiry(self):
        commands = (temporary_compat.ENGINE_CMAKE / "helpers/Commands.cmake").as_posix()
        include = f'include("{commands}")\n'
        result = self.run_cmake(include + 'TemporaryCompat(Expired "2000-01-01")\n')
        self.assertEqual(result.returncode, 0, result.stderr)

        for arguments in ('"Bad Id" "2026-12-31"', 'Id "31.12.2026"', 'Id "2026-12-31" extra', 'Id'):
            with self.subTest(arguments=arguments):
                result = self.run_cmake(include + f"TemporaryCompat({arguments})\n")
                self.assertNotEqual(result.returncode, 0)

    def test_web_database_options_have_no_platform_veto(self):
        commands = (temporary_compat.ENGINE_CMAKE / "helpers/Commands.cmake").as_posix()
        stage = (temporary_compat.ENGINE_CMAKE / "stages/ProjectOptions.cmake").as_posix()
        prefix = f'''include("{commands}")
function(AbortMessage text)
    message(FATAL_ERROR "${{text}}")
endfunction()
set(FO_FORCE_ENABLE_3D "")
'''

        for web, server, mongo, sqlite in ((0, 1, 1, 1), (1, 0, 1, 1), (1, 1, 0, 0), (1, 1, 1, 0), (1, 1, 0, 1), (1, 1, 1, 1)):
            with self.subTest(web=web, server=server, mongo=mongo, sqlite=sqlite):
                source = prefix + f'''set(FO_WEB {web})
set(FO_BUILD_SERVER_LIB {server})
set(FO_DATABASE_MONGO {mongo})
set(FO_DATABASE_SQLITE {sqlite})
include("{stage}")
'''
                result = self.run_cmake(source)

                self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
