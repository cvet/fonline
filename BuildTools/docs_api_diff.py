from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path
from typing import Any

import docs_api


SCHEMA_VERSION = 1
DISPOSITIONS_SCHEMA_VERSION = 2
DEFAULT_CURRENT = docs_api.DEFAULT_OUTPUT
DEFAULT_DISPOSITIONS = "Docs/contract-change-dispositions.json"
DEFAULT_JSON_OUTPUT = "Workspace/api-diff.json"
DEFAULT_MARKDOWN_OUTPUT = "Workspace/api-diff.md"

VALID_STABILITIES = {"stable", "experimental", "deprecated", "internal"}
PUBLIC_STABILITIES = {"stable", "experimental", "deprecated"}
VALID_DOMAINS = {
    "api",
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
}
VALID_CLASSIFICATIONS = {"breaking", "compatible"}
DOC_FIELDS = {"description", "examples"}
POLICY_FIELDS = {"stability", "since", "deprecated", "contract"}
IGNORED_SYMBOL_FIELDS = {"source"}
CHANGE_ID_PATTERN = re.compile(
    r"^(api|cmake|cli|package|helper-cli|native-extension|prototype-format|map-format|model-format|text-format|effect-format|image-format|particle-format|font-format)-change\.[a-z-]+\.[0-9a-f]{16}$"
)
DIGEST_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class ApiDiffError(ValueError):
    pass


class BaselineUnavailable(ApiDiffError):
    pass


def _canonical_bytes(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("ascii")


def _digest(value: object) -> str:
    return hashlib.sha256(_canonical_bytes(value)).hexdigest()


def _read_json_text(text: str, label: str) -> dict[str, Any]:
    try:
        value = json.loads(text)
    except json.JSONDecodeError as exception:
        raise ApiDiffError(f"Invalid JSON in {label}: {exception}") from exception
    if not isinstance(value, dict):
        raise ApiDiffError(f"{label} must contain a JSON object")
    return value


def load_api_model_text(text: str, label: str) -> dict[str, Any]:
    model = _read_json_text(text, label)
    if model.get("schema_version") != docs_api.SCHEMA_VERSION:
        raise ApiDiffError(
            f"{label} API schema version must be {docs_api.SCHEMA_VERSION}, "
            f"got {model.get('schema_version')}"
        )

    scope = model.get("scope")
    if not isinstance(scope, dict) or not isinstance(scope.get("repository"), str) or not isinstance(
        scope.get("surface"), str
    ):
        raise ApiDiffError(f"{label} has no valid scope.repository/surface")

    parser_contract = model.get("parser_contract")
    if not isinstance(parser_contract, dict):
        raise ApiDiffError(f"{label} has no parser_contract object")

    symbols = model.get("symbols")
    if not isinstance(symbols, list):
        raise ApiDiffError(f"{label} symbols must be an array")

    symbol_ids: set[str] = set()
    for index, symbol in enumerate(symbols):
        if not isinstance(symbol, dict):
            raise ApiDiffError(f"{label} symbol {index} must be an object")
        for field in ("id", "family_id", "kind", "signature", "stability"):
            if not isinstance(symbol.get(field), str) or not symbol[field]:
                raise ApiDiffError(f"{label} symbol {index} has no valid {field}")
        symbol_id = str(symbol["id"])
        if symbol_id in symbol_ids:
            raise ApiDiffError(f"{label} contains duplicate symbol ID: {symbol_id}")
        symbol_ids.add(symbol_id)
        if symbol["stability"] not in VALID_STABILITIES:
            raise ApiDiffError(
                f"{label} symbol {symbol_id} has invalid stability: {symbol['stability']}"
            )

    return model


def load_api_model(path: Path, label: str) -> dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exception:
        raise ApiDiffError(f"Unable to read {label} API model {path}: {exception}") from exception
    return load_api_model_text(text, label)


def _normalized_contract(contract: object, *, include_notes: bool) -> object:
    if not isinstance(contract, dict):
        return contract
    result = {key: value for key, value in contract.items() if key != "source"}
    if not include_notes:
        result.pop("notes", None)
    return result


def _symbol_diff_view(symbol: dict[str, Any]) -> dict[str, Any]:
    result = {
        key: value
        for key, value in symbol.items()
        if key not in IGNORED_SYMBOL_FIELDS
    }
    if "contract" in result:
        result["contract"] = _normalized_contract(result["contract"], include_notes=True)
    return result


def _symbol_contract_view(symbol: dict[str, Any]) -> dict[str, Any]:
    result = {
        key: value
        for key, value in symbol.items()
        if key not in IGNORED_SYMBOL_FIELDS | DOC_FIELDS
    }
    if "contract" in result:
        result["contract"] = _normalized_contract(result["contract"], include_notes=False)
    return result


def _model_contract_view(model: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema_version": model["schema_version"],
        "source_parser": model.get("source_parser"),
        "scope": model["scope"],
        "parser_contract": model["parser_contract"],
        "symbols": [
            _symbol_contract_view(symbol)
            for symbol in sorted(model["symbols"], key=lambda item: str(item["id"]))
        ],
    }


def model_digest(model: dict[str, Any]) -> str:
    return _digest(model)


def contract_digest(model: dict[str, Any]) -> str:
    return _digest(_model_contract_view(model))


def validate_dispositions(value: object, label: str = "Contract change dispositions") -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ApiDiffError(f"{label} must contain a JSON object")
    if value.get("schema_version") != DISPOSITIONS_SCHEMA_VERSION:
        raise ApiDiffError(
            f"{label} schema_version must be {DISPOSITIONS_SCHEMA_VERSION}"
        )
    entries = value.get("entries")
    if not isinstance(entries, list):
        raise ApiDiffError(f"{label} entries must be an array")

    seen: set[tuple[str, str, str, str]] = set()
    required_text_fields = (
        "rationale",
        "migration",
        "release_note",
        "compatibility",
        "owner",
    )
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise ApiDiffError(f"{label} entry {index} must be an object")
        change_id = entry.get("change_id")
        domain = entry.get("domain")
        baseline_digest = entry.get("baseline_contract_sha256")
        current_digest = entry.get("current_contract_sha256")
        classification = entry.get("classification")
        if not isinstance(change_id, str) or not CHANGE_ID_PATTERN.fullmatch(change_id):
            raise ApiDiffError(f"{label} entry {index} has an invalid change_id")
        if domain not in VALID_DOMAINS:
            raise ApiDiffError(f"{label} entry {index} has an invalid domain")
        if not change_id.startswith(f"{domain}-change."):
            raise ApiDiffError(
                f"{label} entry {index} domain does not match change_id"
            )
        if not isinstance(baseline_digest, str) or not DIGEST_PATTERN.fullmatch(baseline_digest):
            raise ApiDiffError(
                f"{label} entry {index} has an invalid baseline_contract_sha256"
            )
        if not isinstance(current_digest, str) or not DIGEST_PATTERN.fullmatch(current_digest):
            raise ApiDiffError(
                f"{label} entry {index} has an invalid current_contract_sha256"
            )
        if classification not in VALID_CLASSIFICATIONS:
            raise ApiDiffError(
                f"{label} entry {index} classification must be breaking or compatible"
            )
        for field in required_text_fields:
            if not isinstance(entry.get(field), str) or not entry[field].strip():
                raise ApiDiffError(f"{label} entry {index} has no non-empty {field}")

        key = (domain, change_id, baseline_digest, current_digest)
        if key in seen:
            raise ApiDiffError(f"{label} contains a duplicate entry for {change_id}")
        seen.add(key)

    return value


def load_dispositions(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exception:
        raise ApiDiffError(f"Unable to read contract change dispositions {path}: {exception}") from exception
    except json.JSONDecodeError as exception:
        raise ApiDiffError(f"Invalid JSON in contract change dispositions {path}: {exception}") from exception
    return validate_dispositions(value, str(path))


def _change_id(change_type: str, identity: object) -> str:
    return f"api-change.{change_type}.{_digest(identity)[:16]}"


def stability_withdrawal(before: str, after: str) -> bool:
    if before == "stable" and after in {"experimental", "internal"}:
        return True
    return before in {"experimental", "deprecated"} and after == "internal"


def _changed_fields(before: dict[str, Any], after: dict[str, Any]) -> list[str]:
    return sorted(
        key
        for key in set(before) | set(after)
        if before.get(key) != after.get(key)
    )


def _make_symbol_change(
    change_type: str,
    before: dict[str, Any] | None,
    after: dict[str, Any] | None,
) -> dict[str, Any]:
    symbol = before if before is not None else after
    if symbol is None:
        raise ApiDiffError("Symbol change has no before or after value")
    symbol_id = str(symbol["id"])
    baseline_stability = str(before["stability"]) if before is not None else None
    current_stability = str(after["stability"]) if after is not None else None

    if change_type == "added":
        classification = "additive"
        changed_fields = []
        reasons = ["new symbol"]
        disposition_required = False
    elif change_type == "removed":
        classification = "breaking"
        changed_fields = []
        reasons = ["symbol removed"]
        disposition_required = baseline_stability in PUBLIC_STABILITIES
    else:
        if before is None or after is None:
            raise ApiDiffError("Modified symbol change must have before and after values")
        changed_fields = _changed_fields(before, after)
        shape_fields = [
            field
            for field in changed_fields
            if field not in DOC_FIELDS | POLICY_FIELDS
        ]
        withdrawal = stability_withdrawal(
            str(before["stability"]), str(after["stability"])
        )
        if shape_fields or withdrawal:
            classification = "breaking"
            reasons = []
            if shape_fields:
                reasons.append("API shape changed: " + ", ".join(shape_fields))
            if withdrawal:
                reasons.append(
                    f"stability withdrawn: {before['stability']} -> {after['stability']}"
                )
            disposition_required = str(before["stability"]) in PUBLIC_STABILITIES
        elif any(field in POLICY_FIELDS for field in changed_fields):
            classification = "policy"
            reasons = ["API policy metadata changed"]
            disposition_required = False
        else:
            classification = "documentation"
            reasons = ["documentation metadata changed"]
            disposition_required = False

    identity = {
        "change_type": change_type,
        "symbol_id": symbol_id,
        "before": before,
        "after": after,
    }
    return {
        "change_id": _change_id(change_type, identity),
        "domain": "api",
        "change_type": change_type,
        "classification": classification,
        "symbol_id": symbol_id,
        "family_id": symbol.get("family_id"),
        "symbol_kind": symbol.get("kind"),
        "baseline_stability": baseline_stability,
        "current_stability": current_stability,
        "changed_fields": changed_fields,
        "reasons": reasons,
        "disposition_required": disposition_required,
        "before": before,
        "after": after,
    }


def _make_model_change(
    change_type: str,
    before: object,
    after: object,
    reason: str,
) -> dict[str, Any]:
    identity = {
        "change_type": change_type,
        "before": before,
        "after": after,
    }
    return {
        "change_id": _change_id(change_type, identity),
        "domain": "api",
        "change_type": change_type,
        "classification": "breaking",
        "symbol_id": None,
        "family_id": None,
        "symbol_kind": "api-model",
        "baseline_stability": "domain-contract",
        "current_stability": "domain-contract",
        "changed_fields": [change_type.replace("-", "_")],
        "reasons": [reason],
        "disposition_required": True,
        "before": before,
        "after": after,
    }


def _disposition_index(dispositions: dict[str, Any]) -> dict[tuple[str, str, str], dict[str, Any]]:
    return {
        (
            str(entry["change_id"]),
            str(entry["baseline_contract_sha256"]),
            str(entry["current_contract_sha256"]),
        ): entry
        for entry in dispositions["entries"]
    }


def _missing_disposition_template(
    change: dict[str, Any], baseline_contract_sha256: str, current_contract_sha256: str
) -> dict[str, str]:
    return {
        "domain": "api",
        "change_id": str(change["change_id"]),
        "baseline_contract_sha256": baseline_contract_sha256,
        "current_contract_sha256": current_contract_sha256,
        "classification": "breaking",
        "rationale": "Describe why this change is intentional.",
        "migration": "Link migration guidance or explain why it is not required.",
        "release_note": "Link the release note or record its planned owner.",
        "compatibility": "Record migration/version/coordinated-release impact.",
        "owner": "Owning domain or maintainer.",
    }


def generate_api_diff(
    baseline: dict[str, Any],
    current: dict[str, Any],
    dispositions: dict[str, Any],
    *,
    baseline_label: str = "baseline",
    current_label: str = "current",
) -> dict[str, Any]:
    validate_dispositions(dispositions)
    if baseline["scope"].get("repository") != current["scope"].get("repository"):
        raise ApiDiffError("Baseline and current API models describe different repositories")
    if baseline["scope"].get("surface") != current["scope"].get("surface"):
        raise ApiDiffError("Baseline and current API models describe different surfaces")

    baseline_model_sha256 = model_digest(baseline)
    current_model_sha256 = model_digest(current)
    baseline_contract_sha256 = contract_digest(baseline)
    current_contract_sha256 = contract_digest(current)
    changes: list[dict[str, Any]] = []

    if baseline.get("source_parser") != current.get("source_parser"):
        changes.append(
            _make_model_change(
                "source-parser",
                baseline.get("source_parser"),
                current.get("source_parser"),
                "canonical API source parser changed",
            )
        )
    if baseline["scope"] != current["scope"]:
        changes.append(
            _make_model_change(
                "model-scope",
                baseline["scope"],
                current["scope"],
                "canonical API model scope changed",
            )
        )
    if baseline["parser_contract"] != current["parser_contract"]:
        changes.append(
            _make_model_change(
                "parser-contract",
                baseline["parser_contract"],
                current["parser_contract"],
                "parser-exposed targets, hooks, migrations, or stability labels changed",
            )
        )

    baseline_symbols = {
        str(symbol["id"]): _symbol_diff_view(symbol)
        for symbol in baseline["symbols"]
    }
    current_symbols = {
        str(symbol["id"]): _symbol_diff_view(symbol)
        for symbol in current["symbols"]
    }
    for symbol_id in sorted(set(baseline_symbols) | set(current_symbols)):
        before = baseline_symbols.get(symbol_id)
        after = current_symbols.get(symbol_id)
        if before is None:
            changes.append(_make_symbol_change("added", None, after))
        elif after is None:
            changes.append(_make_symbol_change("removed", before, None))
        elif before != after:
            changes.append(_make_symbol_change("modified", before, after))

    changes.sort(
        key=lambda change: (
            str(change["symbol_id"] or ""),
            str(change["change_type"]),
            str(change["change_id"]),
        )
    )
    disposition_index = _disposition_index(dispositions)
    missing_dispositions: list[dict[str, str]] = []
    satisfied_disposition_count = 0
    for change in changes:
        if not change["disposition_required"]:
            change["disposition"] = None
            change["disposition_status"] = "not-required"
            continue
        key = (
            str(change["change_id"]),
            baseline_contract_sha256,
            current_contract_sha256,
        )
        disposition = disposition_index.get(key)
        if disposition is None:
            change["disposition"] = None
            change["disposition_status"] = "missing"
            missing_dispositions.append(
                _missing_disposition_template(
                    change, baseline_contract_sha256, current_contract_sha256
                )
            )
        else:
            change["disposition"] = disposition
            change["disposition_status"] = "satisfied"
            satisfied_disposition_count += 1

    classifications = Counter(str(change["classification"]) for change in changes)
    change_types = Counter(str(change["change_type"]) for change in changes)
    required_disposition_count = sum(bool(change["disposition_required"]) for change in changes)
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_api_diff.py",
        "domain": "api",
        "status": "blocked" if missing_dispositions else "pass",
        "baseline": {
            "label": baseline_label,
            "model_sha256": baseline_model_sha256,
            "contract_sha256": baseline_contract_sha256,
            "symbol_count": len(baseline_symbols),
        },
        "current": {
            "label": current_label,
            "model_sha256": current_model_sha256,
            "contract_sha256": current_contract_sha256,
            "symbol_count": len(current_symbols),
        },
        "summary": {
            "change_count": len(changes),
            "changes_by_type": dict(sorted(change_types.items())),
            "changes_by_classification": dict(sorted(classifications.items())),
            "required_disposition_count": required_disposition_count,
            "satisfied_disposition_count": satisfied_disposition_count,
            "missing_disposition_count": len(missing_dispositions),
        },
        "changes": changes,
        "missing_dispositions": missing_dispositions,
    }


def generate_bootstrap_report(
    current: dict[str, Any], current_label: str, baseline_label: str, reason: str
) -> dict[str, Any]:
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": "BuildTools/docs_api_diff.py",
        "domain": "api",
        "status": "bootstrap",
        "baseline": {"label": baseline_label, "unavailable_reason": reason},
        "current": {
            "label": current_label,
            "model_sha256": model_digest(current),
            "contract_sha256": contract_digest(current),
            "symbol_count": len(current["symbols"]),
        },
        "summary": {
            "change_count": 0,
            "changes_by_type": {},
            "changes_by_classification": {},
            "required_disposition_count": 0,
            "satisfied_disposition_count": 0,
            "missing_disposition_count": 0,
        },
        "changes": [],
        "missing_dispositions": [],
    }


def render_api_diff(report: dict[str, Any]) -> str:
    return json.dumps(report, indent=2, ensure_ascii=True) + "\n"


def _text(value: object) -> str:
    result = html.escape(str(value), quote=True)
    result = result.replace("|", "&#124;").replace("{", "&#123;").replace("}", "&#125;")
    return result.replace("\r\n", "<br>").replace("\r", "<br>").replace("\n", "<br>")


def _code(value: object) -> str:
    return f"<code>{_text(value)}</code>"


def render_api_diff_markdown(report: dict[str, Any]) -> str:
    summary = report["summary"]
    lines = [
        "# FOnline API Diff",
        "",
        f"Status: **{_text(report['status'])}**",
        "",
        f"Baseline: {_code(report['baseline']['label'])}",
        "",
        f"Current: {_code(report['current']['label'])}",
        "",
        f"Changes: **{summary['change_count']}**; required dispositions: "
        f"**{summary['required_disposition_count']}**; missing: "
        f"**{summary['missing_disposition_count']}**.",
        "",
    ]
    if report["status"] == "bootstrap":
        lines.extend(
            [
                "The baseline model is unavailable, so this run establishes the first comparable snapshot.",
                "",
                f"Reason: {_text(report['baseline']['unavailable_reason'])}",
                "",
            ]
        )

    lines.extend(
        [
            "| Classification | Change | Symbol | Stability | Fields | Disposition | Change ID |",
            "| --- | --- | --- | --- | --- | --- | --- |",
        ]
    )
    for change in report["changes"]:
        symbol = change["symbol_id"] or "API model"
        stability = f"{change['baseline_stability']} -> {change['current_stability']}"
        fields = ", ".join(change["changed_fields"]) or "-"
        lines.append(
            f"| {_text(change['classification'])} | {_text(change['change_type'])} | "
            f"{_code(symbol)} | {_text(stability)} | {_text(fields)} | "
            f"{_text(change['disposition_status'])} | {_code(change['change_id'])} |"
        )
    if not report["changes"]:
        lines.append("| - | - | - | - | - | - | - |")
    lines.append("")

    if report["missing_dispositions"]:
        lines.extend(
            [
                "## Missing dispositions",
                "",
                "Add these entries to `Docs/contract-change-dispositions.json`, replacing every placeholder "
                "with an owner-reviewed disposition:",
                "",
                "```json",
                json.dumps(report["missing_dispositions"], indent=2, ensure_ascii=True),
                "```",
                "",
            ]
        )
    return "\n".join(lines).rstrip() + "\n"


def _resolve_inside_root(root: Path, value: str, label: str) -> Path:
    path = (root / value).resolve()
    try:
        path.relative_to(root)
    except ValueError as exception:
        raise ApiDiffError(f"{label} escapes the engine root: {value}") from exception
    return path


def load_git_baseline(root: Path, ref: str, model_path: str) -> dict[str, Any]:
    if not ref or set(ref) == {"0"}:
        raise BaselineUnavailable("event has no previous revision")
    commit_check = subprocess.run(
        ["git", "cat-file", "-e", f"{ref}^{{commit}}"],
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if commit_check.returncode != 0:
        detail = commit_check.stderr.strip() or "unknown revision"
        raise ApiDiffError(f"API baseline git revision is unavailable: {ref}: {detail}")

    result = subprocess.run(
        ["git", "show", f"{ref}:{model_path}"],
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="strict",
        check=False,
    )
    if result.returncode != 0:
        raise BaselineUnavailable(
            f"{model_path} does not exist at baseline revision {ref}"
        )
    return load_api_model_text(result.stdout, f"git {ref}:{model_path}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Compare canonical FOnline API models and enforce public breaking-change dispositions"
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    baseline = parser.add_mutually_exclusive_group(required=True)
    baseline.add_argument("--baseline")
    baseline.add_argument("--baseline-git-ref")
    parser.add_argument("--current", default=DEFAULT_CURRENT)
    parser.add_argument("--dispositions", default=DEFAULT_DISPOSITIONS)
    parser.add_argument("--json-output", default=DEFAULT_JSON_OUTPUT)
    parser.add_argument("--markdown-output", default=DEFAULT_MARKDOWN_OUTPUT)
    parser.add_argument("--allow-missing-baseline", action="store_true")
    parser.add_argument("--enforce", action="store_true")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        current_path = _resolve_inside_root(root, args.current, "Current API model")
        dispositions_path = _resolve_inside_root(
            root, args.dispositions, "Contract change dispositions"
        )
        current = load_api_model(current_path, "current")
        dispositions = load_dispositions(dispositions_path)
        current_label = current_path.relative_to(root).as_posix()
        baseline_label: str
        try:
            if args.baseline_git_ref:
                baseline_label = f"git:{args.baseline_git_ref}:{args.current}"
                baseline_model = load_git_baseline(root, args.baseline_git_ref, args.current)
            else:
                baseline_path = _resolve_inside_root(root, args.baseline, "Baseline API model")
                baseline_label = baseline_path.relative_to(root).as_posix()
                baseline_model = load_api_model(baseline_path, "baseline")
        except BaselineUnavailable as exception:
            if not args.allow_missing_baseline:
                raise
            baseline_label = (
                f"git:{args.baseline_git_ref}:{args.current}"
                if args.baseline_git_ref
                else str(args.baseline)
            )
            report = generate_bootstrap_report(
                current, current_label, baseline_label, str(exception)
            )
        else:
            report = generate_api_diff(
                baseline_model,
                current,
                dispositions,
                baseline_label=baseline_label,
                current_label=current_label,
            )

        if args.write:
            json_output = _resolve_inside_root(root, args.json_output, "API diff JSON output")
            markdown_output = _resolve_inside_root(
                root, args.markdown_output, "API diff Markdown output"
            )
            json_output.parent.mkdir(parents=True, exist_ok=True)
            markdown_output.parent.mkdir(parents=True, exist_ok=True)
            json_output.write_text(render_api_diff(report), encoding="utf-8", newline="\n")
            markdown_output.write_text(
                render_api_diff_markdown(report), encoding="utf-8", newline="\n"
            )
    except (ApiDiffError, OSError, UnicodeError) as exception:
        print(f"Unable to evaluate API diff: {exception}", file=sys.stderr)
        return 1

    summary = report["summary"]
    print(
        f"API diff {report['status']}: {summary['change_count']} changes, "
        f"{summary['required_disposition_count']} required dispositions, "
        f"{summary['missing_disposition_count']} missing"
    )
    if args.enforce and report["status"] == "blocked":
        report_hint = (
            "inspect Workspace/api-diff.md"
            if args.write
            else "rerun with --write to produce Workspace/api-diff.md"
        )
        print(
            "Public API breaking changes require entries in Docs/contract-change-dispositions.json; "
            + report_hint,
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
