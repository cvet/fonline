from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA_VERSION = 1
DEFAULT_MANIFEST = "BuildTools/ExternalProjectEvidence.json"
DEFAULT_MODEL = "Docs/generated/external-project-evidence.json"
DEFAULT_INDEX = "Docs/generated/external-project-evidence/index.md"
DOCUMENTATION_MANIFEST = "Docs/documentation-manifest.json"
GENERATED_BY = "BuildTools/docs_external_evidence.py"
OUTPUT_PATHS = (DEFAULT_MODEL, DEFAULT_INDEX)
VALID_DISPOSITIONS = {
    "promoted",
    "boundary-owned",
    "promotion-candidate",
    "project-owned",
}
VALID_PRIORITIES = {"P0", "P1", "P2", "P3"}
REVIEW_FIELDS = ("scope", "required_evidence", "co_review_when")


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _string_list(
    value: object,
    label: str,
    *,
    allow_empty: bool = False,
) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        qualifier = "an" if allow_empty else "a non-empty"
        raise ValueError(f"{label} must be {qualifier} array of strings")
    if any(not isinstance(item, str) or not item.strip() for item in value):
        raise ValueError(f"{label} must contain only non-empty strings")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates")
    return list(value)


def _relative_path(value: object, label: str) -> str:
    path = _required_string(value, label)
    pure = PurePosixPath(path)
    if pure.is_absolute() or ".." in pure.parts or "\\" in path:
        raise ValueError(f"{label} must be a normalized repository-relative path")
    return path


def _load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exception:
        raise ValueError(f"Unable to read {label} {path}: {exception}") from exception
    except json.JSONDecodeError as exception:
        raise ValueError(f"Unable to parse {label} {path}: {exception}") from exception
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be a JSON object")
    return value


def _validate_owner_policy(
    documentation_manifest: dict[str, Any],
) -> tuple[dict[str, str], dict[str, dict[str, Any]]]:
    owners = documentation_manifest.get("owners")
    reviews = documentation_manifest.get("owner_review_requirements")
    if not isinstance(owners, dict) or not owners:
        raise ValueError("documentation manifest owners must be a non-empty object")
    if not isinstance(reviews, dict) or set(reviews) != set(owners):
        raise ValueError(
            "documentation manifest owner_review_requirements must match owners exactly"
        )

    normalized_owners: dict[str, str] = {}
    normalized_reviews: dict[str, dict[str, Any]] = {}
    for owner, description in owners.items():
        if not isinstance(owner, str) or not re.fullmatch(r"[a-z][a-z0-9-]*", owner):
            raise ValueError(f"invalid documentation owner id: {owner}")
        normalized_owners[owner] = _required_string(description, f"owners.{owner}")
        review = reviews[owner]
        if not isinstance(review, dict) or set(review) != set(REVIEW_FIELDS):
            raise ValueError(
                f"owner_review_requirements.{owner} must define exactly "
                f"{list(REVIEW_FIELDS)}"
            )
        normalized_reviews[owner] = {
            "scope": _required_string(
                review.get("scope"),
                f"owner_review_requirements.{owner}.scope",
            ),
            "required_evidence": _string_list(
                review.get("required_evidence"),
                f"owner_review_requirements.{owner}.required_evidence",
            ),
            "co_review_when": _string_list(
                review.get("co_review_when"),
                f"owner_review_requirements.{owner}.co_review_when",
            ),
        }
    return normalized_owners, normalized_reviews


def generate_model(
    root: Path,
    manifest_path: str = DEFAULT_MANIFEST,
) -> dict[str, Any]:
    source_path = root / manifest_path
    raw = _load_json(source_path, "external project evidence manifest")
    if raw.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"external evidence schema_version must be {SCHEMA_VERSION}"
        )
    title = _required_string(raw.get("title"), "title")

    policy = raw.get("policy")
    if not isinstance(policy, dict):
        raise ValueError("policy must be an object")
    normative_boundary = _required_string(
        policy.get("normative_boundary"),
        "policy.normative_boundary",
    )
    dispositions = policy.get("dispositions")
    priorities = policy.get("priorities")
    if not isinstance(dispositions, dict) or set(dispositions) != VALID_DISPOSITIONS:
        raise ValueError(
            f"policy.dispositions must define exactly {sorted(VALID_DISPOSITIONS)}"
        )
    if not isinstance(priorities, dict) or set(priorities) != VALID_PRIORITIES:
        raise ValueError(
            f"policy.priorities must define exactly {sorted(VALID_PRIORITIES)}"
        )
    for key, value in dispositions.items():
        _required_string(value, f"policy.dispositions.{key}")
    for key, value in priorities.items():
        _required_string(value, f"policy.priorities.{key}")

    snapshots = raw.get("snapshots")
    if not isinstance(snapshots, dict) or len(snapshots) < 2:
        raise ValueError("snapshots must define at least two external projects")
    normalized_snapshots: dict[str, dict[str, str]] = {}
    for snapshot_id, value in snapshots.items():
        if not isinstance(snapshot_id, str) or not re.fullmatch(
            r"[a-z][a-z0-9-]*", snapshot_id
        ):
            raise ValueError(f"invalid snapshot id: {snapshot_id}")
        if not isinstance(value, dict):
            raise ValueError(f"snapshots.{snapshot_id} must be an object")
        revision = _required_string(
            value.get("revision"), f"snapshots.{snapshot_id}.revision"
        )
        if not re.fullmatch(r"[0-9a-f]{40}", revision):
            raise ValueError(
                f"snapshots.{snapshot_id}.revision must be a lowercase full SHA"
            )
        normalized_snapshots[snapshot_id] = {
            field: _required_string(
                value.get(field), f"snapshots.{snapshot_id}.{field}"
            )
            for field in ("repository", "url", "revision", "availability", "role")
        }

    documentation_manifest = _load_json(
        root / DOCUMENTATION_MANIFEST,
        "documentation manifest",
    )
    owners, owner_reviews = _validate_owner_policy(documentation_manifest)

    records = raw.get("records")
    if not isinstance(records, list) or not records:
        raise ValueError("records must be a non-empty array")
    normalized_records: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    represented_snapshots: set[str] = set()
    for index, value in enumerate(records):
        label = f"records[{index}]"
        if not isinstance(value, dict):
            raise ValueError(f"{label} must be an object")
        record_id = _required_string(value.get("id"), f"{label}.id")
        if not re.fullmatch(r"[a-z][a-z0-9-]*", record_id):
            raise ValueError(f"{label}.id is invalid: {record_id}")
        if record_id in seen_ids:
            raise ValueError(f"duplicate record id: {record_id}")
        seen_ids.add(record_id)

        sources = value.get("sources")
        if not isinstance(sources, list) or not sources:
            raise ValueError(f"{label}.sources must be a non-empty array")
        normalized_sources: list[dict[str, str]] = []
        source_keys: set[tuple[str, str]] = set()
        for source_index, source in enumerate(sources):
            source_label = f"{label}.sources[{source_index}]"
            if not isinstance(source, dict) or set(source) != {"snapshot", "path"}:
                raise ValueError(
                    f"{source_label} must define exactly snapshot and path"
                )
            snapshot = _required_string(
                source.get("snapshot"), f"{source_label}.snapshot"
            )
            if snapshot not in normalized_snapshots:
                raise ValueError(
                    f"{source_label} references unknown snapshot: {snapshot}"
                )
            path = _relative_path(source.get("path"), f"{source_label}.path")
            key = (snapshot, path)
            if key in source_keys:
                raise ValueError(f"{label}.sources repeats {snapshot}:{path}")
            source_keys.add(key)
            represented_snapshots.add(snapshot)
            normalized_sources.append({"snapshot": snapshot, "path": path})

        disposition = _required_string(
            value.get("disposition"), f"{label}.disposition"
        )
        if disposition not in VALID_DISPOSITIONS:
            raise ValueError(f"{label}.disposition is unsupported: {disposition}")
        priority = _required_string(value.get("priority"), f"{label}.priority")
        if priority not in VALID_PRIORITIES:
            raise ValueError(f"{label}.priority is unsupported: {priority}")
        owner = _required_string(value.get("owner"), f"{label}.owner")
        if owner not in owners:
            raise ValueError(f"{label}.owner is unknown: {owner}")
        required_reviews = _string_list(
            value.get("required_reviews"),
            f"{label}.required_reviews",
        )
        unknown_reviews = sorted(set(required_reviews) - set(owners))
        if unknown_reviews:
            raise ValueError(
                f"{label}.required_reviews names unknown owners: "
                + ", ".join(unknown_reviews)
            )
        if owner not in required_reviews:
            raise ValueError(f"{label}.required_reviews must include owner {owner}")

        engine_targets = [
            _relative_path(item, f"{label}.engine_targets")
            for item in _string_list(
                value.get("engine_targets"),
                f"{label}.engine_targets",
                allow_empty=True,
            )
        ]
        planned_targets = [
            _relative_path(item, f"{label}.planned_targets")
            for item in _string_list(
                value.get("planned_targets"),
                f"{label}.planned_targets",
                allow_empty=True,
            )
        ]
        external_targets = _string_list(
            value.get("external_targets"),
            f"{label}.external_targets",
            allow_empty=True,
        )
        for target in engine_targets:
            if not (root / target).exists():
                raise ValueError(f"{label}.engine_targets does not exist: {target}")
        for target in external_targets:
            snapshot, separator, path = target.partition(":")
            if not separator or snapshot not in normalized_snapshots:
                raise ValueError(f"{label}.external_targets is invalid: {target}")
            _relative_path(path, f"{label}.external_targets")

        if disposition == "promoted" and not engine_targets:
            raise ValueError(f"{label} promoted record must name engine_targets")
        if disposition == "boundary-owned" and (
            not engine_targets or not external_targets
        ):
            raise ValueError(
                f"{label} boundary-owned record needs engine and external targets"
            )
        if disposition == "promotion-candidate" and not planned_targets:
            raise ValueError(
                f"{label} promotion-candidate record must name planned_targets"
            )
        if disposition == "project-owned" and (
            engine_targets or planned_targets or not external_targets
        ):
            raise ValueError(
                f"{label} project-owned record needs only external_targets"
            )

        normalized_records.append(
            {
                "id": record_id,
                "title": _required_string(value.get("title"), f"{label}.title"),
                "sources": normalized_sources,
                "reusable_claim": _required_string(
                    value.get("reusable_claim"), f"{label}.reusable_claim"
                ),
                "disposition": disposition,
                "owner": owner,
                "priority": priority,
                "engine_targets": engine_targets,
                "planned_targets": planned_targets,
                "external_targets": external_targets,
                "required_reviews": required_reviews,
                "promotion_gate": _required_string(
                    value.get("promotion_gate"), f"{label}.promotion_gate"
                ),
                "decision": _required_string(
                    value.get("decision"), f"{label}.decision"
                ),
            }
        )

    if represented_snapshots != set(normalized_snapshots):
        missing = sorted(set(normalized_snapshots) - represented_snapshots)
        raise ValueError("unrepresented snapshots: " + ", ".join(missing))

    disposition_counts = Counter(
        record["disposition"] for record in normalized_records
    )
    priority_counts = Counter(record["priority"] for record in normalized_records)
    owner_counts = Counter(record["owner"] for record in normalized_records)
    source_count = sum(len(record["sources"]) for record in normalized_records)
    source_sha256 = hashlib.sha256(source_path.read_bytes()).hexdigest()
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_manifest": manifest_path,
        "source_sha256": source_sha256,
        "title": title,
        "policy": {
            "normative_boundary": normative_boundary,
            "dispositions": copy.deepcopy(dispositions),
            "priorities": copy.deepcopy(priorities),
        },
        "snapshots": normalized_snapshots,
        "owners": owners,
        "owner_review_requirements": owner_reviews,
        "records": normalized_records,
        "summary": {
            "snapshot_count": len(normalized_snapshots),
            "record_count": len(normalized_records),
            "source_reference_count": source_count,
            "records_by_disposition": dict(sorted(disposition_counts.items())),
            "records_by_priority": dict(sorted(priority_counts.items())),
            "records_by_owner": dict(sorted(owner_counts.items())),
        },
    }


def render_index(model: dict[str, Any]) -> str:
    summary = model["summary"]
    disposition_summary = ", ".join(
        f"`{key}` {value}"
        for key, value in summary["records_by_disposition"].items()
    )
    lines = [
        "<!-- Generated by BuildTools/docs_external_evidence.py. Do not edit by hand. -->",
        "",
        "# External Project Evidence And Promotion Inventory",
        "",
        model["policy"]["normative_boundary"],
        "",
        "This internal audit records discovery material, ownership decisions, and promotion gates. It is not a substitute for the Engine sources and tests named by the resulting Engine-owned guides.",
        "",
        "## Coverage",
        "",
        f"- Snapshots: **{summary['snapshot_count']}**",
        f"- Classified concerns: **{summary['record_count']}**",
        f"- Source references: **{summary['source_reference_count']}**",
        f"- Dispositions: {disposition_summary}",
        "",
        "## Audited snapshots",
        "",
        "| Snapshot | Repository | Exact revision | Availability | Role |",
        "| --- | --- | --- | --- | --- |",
    ]
    for snapshot_id, snapshot in model["snapshots"].items():
        lines.append(
            f"| `{snapshot_id}` | `{snapshot['repository']}` | "
            f"`{snapshot['revision']}` | `{snapshot['availability']}` | "
            f"{snapshot['role']} |"
        )

    lines.extend(
        [
            "",
            "## Classified concerns",
            "",
            "| Concern | Disposition | Priority | Owner | Engine or planned targets | Required reviews |",
            "| --- | --- | --- | --- | --- | --- |",
        ]
    )
    for record in model["records"]:
        targets = record["engine_targets"] + record["planned_targets"]
        if not targets:
            targets = record["external_targets"]
        lines.append(
            f"| `{record['id']}` {record['title']} | `{record['disposition']}` | "
            f"`{record['priority']}` | `{record['owner']}` | "
            f"{'<br>'.join(f'`{target}`' for target in targets)} | "
            f"{'<br>'.join(f'`{owner}`' for owner in record['required_reviews'])} |"
        )

    lines.extend(["", "## Decisions and gates", ""])
    for record in model["records"]:
        source_text = ", ".join(
            f"`{source['snapshot']}:{source['path']}`"
            for source in record["sources"]
        )
        lines.extend(
            [
                f"### {record['title']}",
                "",
                f"- Sources: {source_text}",
                f"- Reusable claim: {record['reusable_claim']}",
                f"- Decision: {record['decision']}",
                f"- Promotion gate: {record['promotion_gate']}",
                "",
            ]
        )

    lines.extend(
        [
            "## Owner review requirements",
            "",
            "| Owner | Scope | Required evidence | Co-review when |",
            "| --- | --- | --- | --- |",
        ]
    )
    for owner, review in model["owner_review_requirements"].items():
        lines.append(
            f"| `{owner}` | {review['scope']} | "
            f"{'<br>'.join(review['required_evidence'])} | "
            f"{'<br>'.join(review['co_review_when'])} |"
        )
    lines.extend(
        [
            "",
            "## Verification",
            "",
            "The normal generator/check validates schema, ownership, disposition rules, Engine targets, and deterministic output. A maintainer with both exact external checkouts also runs source verification; it proves every recorded path exists in the pinned commit rather than trusting the current working tree.",
            "",
            "```bash",
            "python BuildTools/docs_external_evidence.py --check",
            "python BuildTools/docs_external_evidence.py --check --verify-sources --last-frontier-root ../ --tla-root Workspace/fonline-tla-audit",
            "```",
            "",
        ]
    )
    return "\n".join(lines)


def render_outputs(root: Path) -> dict[str, str]:
    model = generate_model(root)
    return {
        DEFAULT_MODEL: json.dumps(model, ensure_ascii=False, indent=2) + "\n",
        DEFAULT_INDEX: render_index(model),
    }


def _git_output(root: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise ValueError(f"git {' '.join(args)} failed for {root}: {detail}")
    return result.stdout.strip()


def verify_external_sources(
    model: dict[str, Any],
    roots: dict[str, Path],
) -> None:
    if set(roots) != set(model["snapshots"]):
        raise ValueError(
            "external source roots must match snapshots exactly: "
            + ", ".join(sorted(model["snapshots"]))
        )
    sources_by_snapshot: dict[str, set[str]] = {
        snapshot: set() for snapshot in model["snapshots"]
    }
    for record in model["records"]:
        for source in record["sources"]:
            sources_by_snapshot[source["snapshot"]].add(source["path"])

    for snapshot_id, snapshot in model["snapshots"].items():
        checkout = roots[snapshot_id].resolve()
        if not checkout.is_dir():
            raise ValueError(
                f"external checkout does not exist for {snapshot_id}: {checkout}"
            )
        head = _git_output(checkout, "rev-parse", "HEAD")
        if head != snapshot["revision"]:
            raise ValueError(
                f"{snapshot_id} checkout HEAD must be {snapshot['revision']}, got {head}"
            )
        for source_path in sorted(sources_by_snapshot[snapshot_id]):
            result = subprocess.run(
                [
                    "git",
                    "-C",
                    str(checkout),
                    "cat-file",
                    "-e",
                    f"{snapshot['revision']}:{source_path}",
                ],
                check=False,
                capture_output=True,
                text=True,
                encoding="utf-8",
            )
            if result.returncode != 0:
                raise ValueError(
                    f"{snapshot_id} pinned source is missing: {source_path}"
                )


def _write_or_check(root: Path, *, check: bool) -> int:
    stale: list[str] = []
    for relative_path, content in render_outputs(root).items():
        output = root / relative_path
        if check:
            if not output.is_file() or output.read_text(encoding="utf-8") != content:
                stale.append(relative_path)
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(content, encoding="utf-8")
    if stale:
        print(
            "External project evidence documentation is stale: "
            + ", ".join(stale),
            file=sys.stderr,
        )
        return 1
    if check:
        print("External project evidence documentation is current")
    return 0


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate and verify the external project evidence inventory."
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--last-frontier-root", type=Path)
    parser.add_argument("--tla-root", type=Path)
    parser.add_argument("--verify-sources", action="store_true")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    root = args.root.resolve()
    try:
        result = _write_or_check(root, check=args.check)
        if result != 0:
            return result
        if args.verify_sources:
            if args.last_frontier_root is None or args.tla_root is None:
                raise ValueError(
                    "--verify-sources requires --last-frontier-root and --tla-root"
                )
            model = generate_model(root)
            verify_external_sources(
                model,
                {
                    "last-frontier": args.last_frontier_root,
                    "fonline-tla": args.tla_root,
                },
            )
            print("External project source snapshots are verified")
        return 0
    except ValueError as exception:
        print(f"External project evidence generation failed: {exception}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
