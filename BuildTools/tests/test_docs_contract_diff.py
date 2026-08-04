from __future__ import annotations

import copy
import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_api_diff  # noqa: E402
import docs_contract_diff  # noqa: E402


def _load_current_models() -> dict[str, dict[str, object]]:
    models: dict[str, dict[str, object]] = {}
    for domain in docs_contract_diff.DOMAIN_ORDER:
        model_path = ENGINE_ROOT / "Docs/generated" / docs_contract_diff.MODEL_FILES[domain]
        models[domain] = docs_contract_diff.load_model(domain, model_path, f"current {domain}")
    return models


def _empty_dispositions() -> dict[str, object]:
    return {"schema_version": docs_api_diff.DISPOSITIONS_SCHEMA_VERSION, "entries": []}


def _resolved_dispositions(report: dict[str, object]) -> dict[str, object]:
    entries = copy.deepcopy(report["missing_dispositions"])
    for entry in entries:
        entry.update(
            {
                "classification": "breaking",
                "rationale": "Intentional contract change.",
                "migration": "No migration is needed for this test fixture.",
                "release_note": "Covered by the test release note.",
                "compatibility": "Coordinated revision update.",
                "owner": "build-release",
            }
        )
    return {"schema_version": docs_api_diff.DISPOSITIONS_SCHEMA_VERSION, "entries": entries}


class DocumentationContractDiffTests(unittest.TestCase):
    def test_current_models_have_stable_ids_and_zero_aggregate_diff(self) -> None:
        models = _load_current_models()

        report = docs_contract_diff.generate_contract_diff(
            models, copy.deepcopy(models), _empty_dispositions()
        )

        self.assertEqual(report["status"], "pass")
        self.assertEqual(report["summary"]["domain_count"], 18)
        self.assertEqual(report["summary"]["change_count"], 0)
        self.assertEqual(
            set(report["summary"]["domains"]), set(docs_contract_diff.DOMAIN_ORDER)
        )
        for domain in (
            "cmake",
            "cli",
            "package",
            "helper-cli",
            "native-extension",
            "prototype-format",
            "map-format",
            "model-format",
            "text-format",
            "effect-format",
            "image-format",
            "particle-format",
            "font-format",
            "audio",
            "video",
            "gui-runtime",
            "ai-control-protocol",
        ):
            entries = docs_contract_diff._flatten_entries(domain, models[domain])
            self.assertEqual(len(entries), len({entry["id"] for entry in entries}))

    def test_experimental_cmake_break_blocks_but_internal_breaks_only_report(self) -> None:
        baseline = _load_current_models()
        current = copy.deepcopy(baseline)
        current["cmake"]["options"].pop()
        current["cli"]["commands"].pop()
        current["package"]["packs"][0]["targets"].pop()
        current["helper-cli"]["helpers"][0]["global_arguments"].pop()
        current["native-extension"]["binding_rules"].pop()
        current["prototype-format"]["rules"].pop()
        current["map-format"]["rules"].pop()
        current["model-format"]["rules"].pop()
        current["text-format"]["validation_rules"].pop()
        current["effect-format"]["validation_rules"].pop()
        current["image-format"]["validation_rules"].pop()
        current["particle-format"]["validation_rules"].pop()
        current["font-format"]["validation_rules"].pop()
        current["audio"]["validation_rules"].pop()
        current["video"]["validation_rules"].pop()
        current["gui-runtime"]["validation_rules"].pop()
        current["ai-control-protocol"]["validation_rules"].pop()

        report = docs_contract_diff.generate_contract_diff(
            baseline, current, _empty_dispositions()
        )

        self.assertEqual(report["status"], "blocked")
        self.assertEqual(report["domains"]["cmake"]["summary"]["missing_disposition_count"], 1)
        self.assertGreater(report["domains"]["cli"]["summary"]["change_count"], 0)
        self.assertEqual(report["domains"]["cli"]["summary"]["required_disposition_count"], 0)
        self.assertEqual(report["domains"]["package"]["summary"]["required_disposition_count"], 0)
        self.assertGreater(report["domains"]["helper-cli"]["summary"]["change_count"], 0)
        self.assertEqual(report["domains"]["helper-cli"]["summary"]["required_disposition_count"], 0)
        self.assertEqual(report["domains"]["native-extension"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["prototype-format"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["map-format"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["model-format"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["text-format"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["effect-format"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["image-format"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["particle-format"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["font-format"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["audio"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(report["domains"]["video"]["summary"]["required_disposition_count"], 1)
        self.assertEqual(
            report["domains"]["gui-runtime"]["summary"][
                "required_disposition_count"
            ],
            1,
        )
        self.assertEqual(
            report["domains"]["ai-control-protocol"]["summary"][
                "required_disposition_count"
            ],
            1,
        )
        self.assertEqual(
            {entry["domain"] for entry in report["missing_dispositions"]},
            {
                "cmake",
                "native-extension",
                "prototype-format",
                "map-format",
                "model-format",
                "text-format",
                "effect-format",
                "image-format",
                "particle-format",
                "font-format",
                "audio",
                "video",
                "gui-runtime",
                "ai-control-protocol",
            },
        )

        passed = docs_contract_diff.generate_contract_diff(
            baseline, current, _resolved_dispositions(report)
        )
        self.assertEqual(passed["status"], "pass")
        self.assertEqual(passed["summary"]["satisfied_disposition_count"], 14)

    def test_public_native_break_is_preserved_in_aggregate_report(self) -> None:
        baseline = _load_current_models()
        baseline["api"]["symbols"][0]["stability"] = "stable"
        current = copy.deepcopy(baseline)
        current["api"]["symbols"].pop(0)

        report = docs_contract_diff.generate_contract_diff(
            baseline, current, _empty_dispositions()
        )

        self.assertEqual(report["status"], "blocked")
        self.assertEqual(report["domains"]["api"]["summary"]["missing_disposition_count"], 1)
        self.assertEqual(report["missing_dispositions"][0]["domain"], "api")
        self.assertTrue(report["missing_dispositions"][0]["change_id"].startswith("api-change."))

    def test_nested_documentation_and_domain_policy_changes_are_non_breaking(self) -> None:
        baseline = _load_current_models()
        current = copy.deepcopy(baseline)
        current["package"]["declaration"]["clauses"][0]["arguments"][0]["description"] = (
            "Updated argument description."
        )
        current["cmake"]["scope"]["support_note"] = "Updated support note."
        added_option = copy.deepcopy(current["cmake"]["options"][0])
        added_option["id"] = "cmake.option.FO_TEST_ADDITIVE"
        added_option["name"] = "FO_TEST_ADDITIVE"
        current["cmake"]["options"].append(added_option)

        report = docs_contract_diff.generate_contract_diff(
            baseline, current, _empty_dispositions()
        )

        self.assertEqual(report["status"], "pass")
        classifications = {change["classification"] for change in report["changes"]}
        self.assertEqual(classifications, {"additive", "documentation", "policy"})
        package_change = report["domains"]["package"]["changes"][0]
        self.assertEqual(package_change["classification"], "documentation")
        self.assertEqual(
            docs_contract_diff.generic_contract_digest("package", baseline["package"]),
            docs_contract_diff.generic_contract_digest("package", current["package"]),
        )

    def test_map_enum_source_line_movement_is_provenance_only(self) -> None:
        baseline = _load_current_models()
        current = copy.deepcopy(baseline)
        current["map-format"]["ownerships"][0]["enum_source"]["line"] += 10

        report = docs_contract_diff.generate_contract_diff(
            baseline, current, _empty_dispositions()
        )

        self.assertEqual(report["status"], "pass")
        self.assertEqual(report["domains"]["map-format"]["summary"]["change_count"], 0)
        self.assertEqual(
            docs_contract_diff.generic_contract_digest(
                "map-format", baseline["map-format"]
            ),
            docs_contract_diff.generic_contract_digest(
                "map-format", current["map-format"]
            ),
        )

    def test_model_source_change_requires_exact_disposition(self) -> None:
        baseline = _load_current_models()
        current = copy.deepcopy(baseline)
        current["package"]["source_parser"] = "BuildTools/new_package_parser.py"

        blocked = docs_contract_diff.generate_contract_diff(
            baseline, current, _empty_dispositions()
        )
        self.assertEqual(blocked["status"], "blocked")
        self.assertEqual(blocked["missing_dispositions"][0]["domain"], "package")
        dispositions = _resolved_dispositions(blocked)

        passed = docs_contract_diff.generate_contract_diff(baseline, current, dispositions)
        self.assertEqual(passed["status"], "pass")
        self.assertEqual(passed["summary"]["satisfied_disposition_count"], 1)

        stale = copy.deepcopy(dispositions)
        stale["entries"][0]["current_contract_sha256"] = "0" * 64
        stale_report = docs_contract_diff.generate_contract_diff(baseline, current, stale)
        self.assertEqual(stale_report["status"], "blocked")

    def test_invalid_model_and_cross_domain_ledger_entry_are_rejected(self) -> None:
        models = _load_current_models()
        duplicate_cli = copy.deepcopy(models["cli"])
        duplicate_cli["commands"].append(copy.deepcopy(duplicate_cli["commands"][0]))
        with self.assertRaisesRegex(docs_contract_diff.ContractDiffError, "duplicate entry ID"):
            docs_contract_diff.load_model_text(
                "cli", json.dumps(duplicate_cli), "duplicate CLI model"
            )

        with self.assertRaisesRegex(docs_api_diff.ApiDiffError, "domain does not match"):
            docs_api_diff.validate_dispositions(
                {
                    "schema_version": docs_api_diff.DISPOSITIONS_SCHEMA_VERSION,
                    "entries": [
                        {
                            "domain": "cmake",
                            "change_id": "api-change.removed.0123456789abcdef",
                            "baseline_contract_sha256": "1" * 64,
                            "current_contract_sha256": "2" * 64,
                            "classification": "breaking",
                            "rationale": "x",
                            "migration": "x",
                            "release_note": "x",
                            "compatibility": "x",
                            "owner": "x",
                        }
                    ],
                }
            )

    def test_cli_writes_aggregate_reports_and_enforces_missing_dispositions(self) -> None:
        models = _load_current_models()
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            baseline_dir = root / "Workspace/baseline"
            current_dir = root / "Docs/generated"
            baseline_dir.mkdir(parents=True)
            current_dir.mkdir(parents=True)
            for domain, model in models.items():
                model_text = json.dumps(model, indent=2) + "\n"
                file_name = docs_contract_diff.MODEL_FILES[domain]
                (baseline_dir / file_name).write_text(model_text, encoding="utf-8")
                (current_dir / file_name).write_text(model_text, encoding="utf-8")
            (root / "Docs/contract-change-dispositions.json").write_text(
                json.dumps(_empty_dispositions()), encoding="utf-8"
            )

            current_cmake = copy.deepcopy(models["cmake"])
            current_cmake["options"].pop()
            (current_dir / docs_contract_diff.MODEL_FILES["cmake"]).write_text(
                json.dumps(current_cmake), encoding="utf-8"
            )
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = docs_contract_diff.main(
                    [
                        "--root",
                        str(root),
                        "--baseline-dir",
                        "Workspace/baseline",
                        "--current-dir",
                        "Docs/generated",
                        "--write",
                        "--enforce",
                    ]
                )

            self.assertEqual(result, 1)
            report = json.loads(
                (root / "Workspace/contract-diff.json").read_text(encoding="utf-8")
            )
            self.assertEqual(report["status"], "blocked")
            markdown = (root / "Workspace/contract-diff.md").read_text(encoding="utf-8")
            self.assertIn("Missing dispositions", markdown)
            self.assertIn("<code>cmake</code>", markdown)

    def test_git_baseline_bootstraps_only_models_missing_from_revision(self) -> None:
        models = _load_current_models()
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            generated_dir = root / "Docs/generated"
            generated_dir.mkdir(parents=True)
            api_path = generated_dir / docs_contract_diff.MODEL_FILES["api"]
            api_path.write_text(json.dumps(models["api"]), encoding="utf-8")
            subprocess.run(["git", "init"], cwd=root, check=True, capture_output=True)
            subprocess.run(
                ["git", "config", "user.email", "docs@example.invalid"],
                cwd=root,
                check=True,
            )
            subprocess.run(
                ["git", "config", "user.name", "Docs Test"], cwd=root, check=True
            )
            subprocess.run(["git", "add", "Docs/generated/api.json"], cwd=root, check=True)
            subprocess.run(
                ["git", "commit", "-m", "API baseline"],
                cwd=root,
                check=True,
                capture_output=True,
            )
            for domain in docs_contract_diff.DOMAIN_ORDER[1:]:
                (generated_dir / docs_contract_diff.MODEL_FILES[domain]).write_text(
                    json.dumps(models[domain]), encoding="utf-8"
                )
            (root / "Docs/contract-change-dispositions.json").write_text(
                json.dumps(_empty_dispositions()), encoding="utf-8"
            )

            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = docs_contract_diff.main(
                    [
                        "--root",
                        str(root),
                        "--baseline-git-ref",
                        "HEAD",
                        "--allow-missing-baseline",
                        "--write",
                        "--enforce",
                    ]
                )

            self.assertEqual(result, 0)
            report = json.loads(
                (root / "Workspace/contract-diff.json").read_text(encoding="utf-8")
            )
            self.assertEqual(report["status"], "bootstrap")
            self.assertEqual(report["domains"]["api"]["status"], "pass")
            for domain in docs_contract_diff.DOMAIN_ORDER[1:]:
                self.assertEqual(report["domains"][domain]["status"], "bootstrap")


if __name__ == "__main__":
    unittest.main()
