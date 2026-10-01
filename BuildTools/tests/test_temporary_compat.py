from __future__ import annotations

import datetime
import importlib.util
import os
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
        _, issues = temporary_compat.collect([temporary_compat.ENGINE_SOURCE])

        self.assertEqual(issues, [])


if __name__ == "__main__":
    unittest.main()
