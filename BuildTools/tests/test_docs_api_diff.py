from __future__ import annotations

import contextlib
import copy
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))
import docs_api  # noqa: E402
import docs_api_diff  # noqa: E402


def _symbol(
    symbol_id: str,
    *,
    family_id: str | None = None,
    stability: str = "internal",
    signature: str | None = None,
    description: str = "",
) -> dict[str, object]:
    name = symbol_id.rsplit(".", 1)[-1].split("#", 1)[0]
    return {
        "id": symbol_id,
        "family_id": family_id or symbol_id.split("#", 1)[0],
        "kind": "method",
        "name": name,
        "receiver": "Game",
        "runtime_sides": ["server", "client"],
        "signature": signature or f"void Game.{name}()",
        "flags": [],
        "description": description,
        "stability": stability,
        "since": "1.0.0" if stability in {"stable", "experimental"} else None,
        "deprecated": (
            {
                "since": "1.5.0",
                "replacement": "script.method.common.Game.Current",
                "removal": "2.0.0",
            }
            if stability == "deprecated"
            else None
        ),
        "examples": [],
        "source": {"path": "Source/Scripting/Test.cpp", "line": 1},
        "contract": {
            "explicit": stability != "internal",
            "selector": symbol_id if stability != "internal" else None,
            "source": {"path": "Source/Scripting/Test.cpp", "line": 1}
            if stability != "internal"
            else None,
            "notes": "",
        },
        "declared_target": "common",
        "receivers": ["Game"],
        "return": {"type": "void", "nullable": False},
        "arguments": [],
    }


def _model(*symbols: dict[str, object]) -> dict[str, object]:
    return {
        "schema_version": docs_api.SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_api.py",
        "source_parser": "BuildTools/codegen.py",
        "scope": {
            "repository": "cvet/fonline",
            "surface": "engine-native-codegen",
            "default_stability": "internal",
            "included": ["native script API"],
            "excluded": ["project metadata"],
        },
        "parser_contract": {
            "export_targets": ["common", "server", "client"],
            "registration_targets": ["server", "client", "mapper"],
            "engine_hook_names": ["OnStart"],
            "migration_rule_kinds": ["Version"],
            "api_stability_labels": ["stable", "experimental", "deprecated", "internal"],
        },
        "summary": {"symbol_count": len(symbols)},
        "metadata_source_files": ["Source/Scripting/Test.cpp"],
        "symbols": list(symbols),
    }


def _empty_dispositions() -> dict[str, object]:
    return {"schema_version": docs_api_diff.DISPOSITIONS_SCHEMA_VERSION, "entries": []}


def _resolved_dispositions(report: dict[str, object]) -> dict[str, object]:
    entries = copy.deepcopy(report["missing_dispositions"])
    for entry in entries:
        entry.update(
            {
                "classification": "breaking",
                "rationale": "Intentional contract change approved by the scripting owner.",
                "migration": "Docs/Migrations/Next.md",
                "release_note": "Docs/ReleaseNotes/Next.md",
                "compatibility": "Coordinated client/server release.",
                "owner": "scripting",
            }
        )
    return {"schema_version": docs_api_diff.DISPOSITIONS_SCHEMA_VERSION, "entries": entries}


class DocumentationApiDiffTests(unittest.TestCase):
    def test_additions_and_documentation_changes_are_non_blocking(self) -> None:
        baseline = _model(
            _symbol("script.method.common.Game.Existing", description="Old description")
        )
        current = _model(
            _symbol("script.method.common.Game.Existing", description="New description"),
            _symbol("script.method.common.Game.NewStable", stability="stable"),
        )

        report = docs_api_diff.generate_api_diff(baseline, current, _empty_dispositions())

        self.assertEqual(report["status"], "pass")
        self.assertEqual(report["summary"]["required_disposition_count"], 0)
        self.assertEqual(
            {change["classification"] for change in report["changes"]},
            {"additive", "documentation"},
        )

    def test_public_breaks_and_stability_withdrawal_require_dispositions(self) -> None:
        baseline = _model(
            _symbol("script.method.common.Game.StableRemoved", stability="stable"),
            _symbol("script.method.common.Game.ExperimentalChanged", stability="experimental"),
            _symbol("script.method.common.Game.DeprecatedHidden", stability="deprecated"),
            _symbol("script.method.common.Game.InternalChanged"),
        )
        current = _model(
            _symbol(
                "script.method.common.Game.ExperimentalChanged",
                stability="experimental",
                signature="void Game.ExperimentalChanged(int32 value)",
            ),
            _symbol("script.method.common.Game.DeprecatedHidden"),
            _symbol(
                "script.method.common.Game.InternalChanged",
                signature="void Game.InternalChanged(bool value)",
            ),
        )

        report = docs_api_diff.generate_api_diff(baseline, current, _empty_dispositions())

        self.assertEqual(report["status"], "blocked")
        self.assertEqual(report["summary"]["required_disposition_count"], 3)
        self.assertEqual(report["summary"]["missing_disposition_count"], 3)
        changes = {change["symbol_id"]: change for change in report["changes"]}
        self.assertTrue(changes["script.method.common.Game.StableRemoved"]["disposition_required"])
        self.assertTrue(
            changes["script.method.common.Game.ExperimentalChanged"]["disposition_required"]
        )
        self.assertIn(
            "stability withdrawn",
            " ".join(changes["script.method.common.Game.DeprecatedHidden"]["reasons"]),
        )
        self.assertFalse(
            changes["script.method.common.Game.InternalChanged"]["disposition_required"]
        )

    def test_exact_dispositions_satisfy_gate_and_stale_digests_do_not(self) -> None:
        baseline = _model(_symbol("script.method.common.Game.Public", stability="stable"))
        current = _model()
        blocked = docs_api_diff.generate_api_diff(baseline, current, _empty_dispositions())
        dispositions = _resolved_dispositions(blocked)

        passed = docs_api_diff.generate_api_diff(baseline, current, dispositions)

        self.assertEqual(passed["status"], "pass")
        self.assertEqual(passed["summary"]["satisfied_disposition_count"], 1)

        stale = copy.deepcopy(dispositions)
        stale_entry = stale["entries"][0]
        stale_entry["current_contract_sha256"] = "0" * 64
        stale_report = docs_api_diff.generate_api_diff(baseline, current, stale)
        self.assertEqual(stale_report["status"], "blocked")

    def test_overload_signature_replacement_is_remove_plus_add(self) -> None:
        family_id = "script.method.common.Game.Overloaded"
        baseline = _model(
            _symbol(
                family_id + "#old00000000",
                family_id=family_id,
                stability="stable",
                signature="void Game.Overloaded(int32 value)",
            )
        )
        current = _model(
            _symbol(
                family_id + "#new00000000",
                family_id=family_id,
                stability="stable",
                signature="void Game.Overloaded(string value)",
            )
        )

        report = docs_api_diff.generate_api_diff(baseline, current, _empty_dispositions())

        self.assertEqual(report["summary"]["changes_by_type"], {"added": 1, "removed": 1})
        removed = next(change for change in report["changes"] if change["change_type"] == "removed")
        added = next(change for change in report["changes"] if change["change_type"] == "added")
        self.assertTrue(removed["disposition_required"])
        self.assertFalse(added["disposition_required"])
        self.assertEqual(removed["family_id"], added["family_id"])

    def test_parser_contract_change_requires_disposition(self) -> None:
        baseline = _model()
        current = copy.deepcopy(baseline)
        current["parser_contract"]["engine_hook_names"].append("OnFinish")

        report = docs_api_diff.generate_api_diff(baseline, current, _empty_dispositions())

        self.assertEqual(report["status"], "blocked")
        self.assertEqual(report["changes"][0]["change_type"], "parser-contract")
        self.assertTrue(report["changes"][0]["disposition_required"])

    def test_invalid_models_and_disposition_ledgers_are_rejected(self) -> None:
        duplicate = _model(
            _symbol("script.method.common.Game.Duplicate"),
            _symbol("script.method.common.Game.Duplicate"),
        )
        with self.assertRaisesRegex(docs_api_diff.ApiDiffError, "duplicate symbol ID"):
            docs_api_diff.load_api_model_text(json.dumps(duplicate), "duplicate")

        with self.assertRaisesRegex(docs_api_diff.ApiDiffError, "classification"):
            docs_api_diff.validate_dispositions(
                {
                    "schema_version": docs_api_diff.DISPOSITIONS_SCHEMA_VERSION,
                    "entries": [
                        {
                            "domain": "api",
                            "change_id": "api-change.removed.0123456789abcdef",
                            "baseline_contract_sha256": "1" * 64,
                            "current_contract_sha256": "2" * 64,
                            "classification": "ignored",
                            "rationale": "x",
                            "migration": "x",
                            "release_note": "x",
                            "compatibility": "x",
                            "owner": "x",
                        }
                    ],
                }
            )

    def test_cli_writes_reports_and_enforces_missing_dispositions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "Docs").mkdir()
            baseline = _model(_symbol("script.method.common.Game.Public", stability="stable"))
            current = _model()
            (root / "Docs/baseline.json").write_text(
                json.dumps(baseline), encoding="utf-8"
            )
            (root / "Docs/current.json").write_text(
                json.dumps(current), encoding="utf-8"
            )
            (root / "Docs/dispositions.json").write_text(
                json.dumps(_empty_dispositions()), encoding="utf-8"
            )
            arguments = [
                "--root",
                str(root),
                "--baseline",
                "Docs/baseline.json",
                "--current",
                "Docs/current.json",
                "--dispositions",
                "Docs/dispositions.json",
                "--json-output",
                "Workspace/diff.json",
                "--markdown-output",
                "Workspace/diff.md",
                "--write",
                "--enforce",
            ]

            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = docs_api_diff.main(arguments)

            self.assertEqual(result, 1)
            report = json.loads((root / "Workspace/diff.json").read_text(encoding="utf-8"))
            self.assertEqual(report["status"], "blocked")
            markdown = (root / "Workspace/diff.md").read_text(encoding="utf-8")
            self.assertIn("Missing dispositions", markdown)
            self.assertIn(report["changes"][0]["change_id"], markdown)


if __name__ == "__main__":
    unittest.main()
