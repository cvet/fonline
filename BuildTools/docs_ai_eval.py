from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path

import docs_ai_delivery
import docs_site
import docs_validate


SCHEMA_VERSION = 1
GENERATED_BY = "BuildTools/docs_ai_eval.py"
DEFAULT_MANIFEST = "Docs/documentation-manifest.json"
DEFAULT_SOURCE = "Docs/ai-evaluation.json"
DEFAULT_SEARCH = "assets/docs-search.json"
DEFAULT_OUTPUT = "Docs/generated/ai-evaluation-report.json"
REQUIRED_CATEGORIES = (
    "architecture",
    "scripting",
    "content",
    "debugging",
    "migration",
    "release",
)
VALID_ID_RE = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
PROJECT_ASSUMPTION_RE = re.compile(
    r"\b(?:last\s+frontier|lastfrontier|fonline-tla|fonline\s*:\s*the\s+life\s+after)\b",
    re.IGNORECASE,
)


def _required_object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    return value


def _required_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _required_string_array(value: object, label: str, *, allow_empty: bool = False) -> list[str]:
    if (
        not isinstance(value, list)
        or any(not isinstance(item, str) or not item.strip() for item in value)
        or (not allow_empty and not value)
    ):
        suffix = " (empty allowed)" if allow_empty else ""
        raise ValueError(f"{label} must be a non-empty string array{suffix}")
    return list(value)


def _load_json(path: Path, label: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exception:
        raise ValueError(f"unable to read {label} {path.as_posix()}: {exception}") from exception
    return _required_object(value, label)


def _normalized_sha256(path: Path) -> str:
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _ranked_document_ids(search: dict[str, object], query: str) -> list[str]:
    return [
        str(result["document"]["id"])
        for result in docs_site.search_documents(search, query)
    ]


def evaluate(
    root: Path,
    source_path: str = DEFAULT_SOURCE,
    manifest_path: str = DEFAULT_MANIFEST,
    search_path: str = DEFAULT_SEARCH,
) -> dict[str, object]:
    root = root.resolve()
    source_file = root / source_path
    source = _load_json(source_file, "AI evaluation source")
    manifest = _load_json(root / manifest_path, "documentation manifest")
    search = _load_json(root / search_path, "documentation search index")
    records = docs_ai_delivery._document_records(root, manifest)
    public_current = {
        str(record["id"]): record
        for record in records
        if record["visibility"] == "public" and record["state"] == "current"
    }
    versioning = docs_ai_delivery._versioning_config(manifest)
    current = _required_object(versioning.get("current"), "documentation current version")

    errors: list[str] = []
    if source.get("schema_version") != SCHEMA_VERSION:
        errors.append(f"AI evaluation schema_version must be {SCHEMA_VERSION}")
    source_ref = source.get("source_ref")
    if source_ref != current.get("source_ref"):
        errors.append(
            "AI evaluation source_ref must match documentation current source_ref: "
            f"{current.get('source_ref')}"
        )
    minimum_rate = source.get("minimum_retrieval_success_rate")
    if not isinstance(minimum_rate, (int, float)) or not 0 < minimum_rate <= 1:
        errors.append("AI evaluation minimum_retrieval_success_rate must be in (0, 1]")
        minimum_rate = 1.0

    categories = source.get("categories")
    category_ids: list[str] = []
    if not isinstance(categories, list):
        errors.append("AI evaluation categories must be an array")
    else:
        for index, value in enumerate(categories):
            if not isinstance(value, dict):
                errors.append(f"AI evaluation category[{index}] must be an object")
                continue
            try:
                category_id = _required_string(value.get("id"), f"category[{index}].id")
                _required_string(value.get("title"), f"category[{index}].title")
            except ValueError as exception:
                errors.append(str(exception))
                continue
            category_ids.append(category_id)
        if tuple(category_ids) != REQUIRED_CATEGORIES:
            errors.append(
                "AI evaluation categories must be ordered exactly: "
                + ", ".join(REQUIRED_CATEGORIES)
            )

    tasks = source.get("tasks")
    if not isinstance(tasks, list):
        errors.append("AI evaluation tasks must be an array")
        tasks = []

    seen_task_ids: set[str] = set()
    category_counts: Counter[str] = Counter()
    task_reports: list[dict[str, object]] = []
    query_count = 0
    retrieval_pass_count = 0
    reciprocal_rank_total = 0.0
    top_one_count = 0
    top_three_count = 0
    anchor_cache: dict[Path, set[str]] = {}

    for task_index, task_value in enumerate(tasks):
        label = f"AI evaluation task[{task_index}]"
        task_errors: list[str] = []
        if not isinstance(task_value, dict):
            errors.append(f"{label} must be an object")
            continue
        try:
            task_id = _required_string(task_value.get("id"), f"{label}.id")
            category = _required_string(task_value.get("category"), f"{label}.category")
            question = _required_string(task_value.get("question"), f"{label}.question")
            primary_document_id = _required_string(
                task_value.get("primary_document_id"),
                f"{label}.primary_document_id",
            )
            supporting_document_ids = _required_string_array(
                task_value.get("supporting_document_ids", []),
                f"{label}.supporting_document_ids",
                allow_empty=True,
            )
        except ValueError as exception:
            errors.append(str(exception))
            continue

        if not VALID_ID_RE.fullmatch(task_id):
            task_errors.append(f"{label}.id is invalid: {task_id}")
        if task_id in seen_task_ids:
            task_errors.append(f"AI evaluation repeats task id: {task_id}")
        seen_task_ids.add(task_id)
        if category not in REQUIRED_CATEGORIES:
            task_errors.append(f"{label} has invalid category: {category}")
        category_counts[category] += 1
        if PROJECT_ASSUMPTION_RE.search(question):
            task_errors.append(f"{label} question contains an embedding-project assumption")

        owner_ids = [primary_document_id, *supporting_document_ids]
        if len(owner_ids) != len(set(owner_ids)):
            task_errors.append(f"{label} repeats an owning document id")
        for document_id in owner_ids:
            if document_id not in public_current:
                task_errors.append(
                    f"{label} references a non-public/non-current document: {document_id}"
                )
        primary = public_current.get(primary_document_id)
        if primary is not None and "ai-agent" not in primary["audiences"]:
            task_errors.append(
                f"{label} primary document is not routed to ai-agent: {primary_document_id}"
            )

        retrieval_checks = task_value.get("retrieval_checks")
        if not isinstance(retrieval_checks, list) or len(retrieval_checks) < 2:
            task_errors.append(f"{label}.retrieval_checks must contain at least two checks")
            retrieval_checks = []
        retrieval_reports: list[dict[str, object]] = []
        for query_index, query_value in enumerate(retrieval_checks):
            query_label = f"{label}.retrieval_checks[{query_index}]"
            if not isinstance(query_value, dict):
                task_errors.append(f"{query_label} must be an object")
                continue
            try:
                query = _required_string(query_value.get("query"), f"{query_label}.query")
                expected_ids = _required_string_array(
                    query_value.get("expected_document_ids"),
                    f"{query_label}.expected_document_ids",
                )
            except ValueError as exception:
                task_errors.append(str(exception))
                continue
            max_rank = query_value.get("max_rank")
            if not isinstance(max_rank, int) or not 1 <= max_rank <= 12:
                task_errors.append(f"{query_label}.max_rank must be an integer in [1, 12]")
                continue
            if any(document_id not in owner_ids for document_id in expected_ids):
                task_errors.append(
                    f"{query_label} expected IDs must be task owning documents"
                )
            ranked_ids = _ranked_document_ids(search, query)
            matching_ranks = [
                ranked_ids.index(document_id) + 1
                for document_id in expected_ids
                if document_id in ranked_ids
            ]
            rank = min(matching_ranks) if matching_ranks else None
            passed = rank is not None and rank <= max_rank
            query_count += 1
            if passed:
                retrieval_pass_count += 1
            if rank is not None:
                reciprocal_rank_total += 1.0 / rank
                if rank == 1:
                    top_one_count += 1
                if rank <= 3:
                    top_three_count += 1
            retrieval_reports.append(
                {
                    "query": query,
                    "expected_document_ids": expected_ids,
                    "max_rank": max_rank,
                    "rank": rank,
                    "passed": passed,
                    "top_document_ids": ranked_ids[:5],
                }
            )
            if not passed:
                task_errors.append(
                    f"{query_label} did not retrieve {', '.join(expected_ids)} "
                    f"within rank {max_rank}"
                )

        answer_checks = task_value.get("answer_checks")
        if not isinstance(answer_checks, list) or len(answer_checks) < 2:
            task_errors.append(f"{label}.answer_checks must contain at least two checks")
            answer_checks = []
        answer_check_ids: set[str] = set()
        answer_reports: list[dict[str, object]] = []
        for check_index, check_value in enumerate(answer_checks):
            check_label = f"{label}.answer_checks[{check_index}]"
            if not isinstance(check_value, dict):
                task_errors.append(f"{check_label} must be an object")
                continue
            try:
                check_id = _required_string(check_value.get("id"), f"{check_label}.id")
                description = _required_string(
                    check_value.get("description"),
                    f"{check_label}.description",
                )
                document_id = _required_string(
                    check_value.get("document_id"),
                    f"{check_label}.document_id",
                )
                anchor = _required_string(check_value.get("anchor"), f"{check_label}.anchor")
                required_terms = _required_string_array(
                    check_value.get("required_terms"),
                    f"{check_label}.required_terms",
                )
            except ValueError as exception:
                task_errors.append(str(exception))
                continue
            if check_id in answer_check_ids:
                task_errors.append(f"{label} repeats answer check id: {check_id}")
            answer_check_ids.add(check_id)
            if not VALID_ID_RE.fullmatch(check_id):
                task_errors.append(f"{check_label}.id is invalid: {check_id}")
            if document_id not in owner_ids:
                task_errors.append(f"{check_label} document must be a task owning document")
            record = public_current.get(document_id)
            evidence_ok = record is not None
            if record is not None:
                document_path = root / str(record["path"])
                anchors = anchor_cache.setdefault(
                    document_path,
                    docs_validate._markdown_anchors(document_path),
                )
                if anchor not in anchors:
                    task_errors.append(
                        f"{check_label} anchor does not exist in {record['path']}: #{anchor}"
                    )
                    evidence_ok = False
                content = str(record["content"]).casefold()
                missing_terms = [
                    term for term in required_terms if term.casefold() not in content
                ]
                if missing_terms:
                    task_errors.append(
                        f"{check_label} terms are missing from {record['path']}: "
                        + ", ".join(missing_terms)
                    )
                    evidence_ok = False
            if PROJECT_ASSUMPTION_RE.search(description):
                task_errors.append(f"{check_label} contains an embedding-project assumption")
            answer_reports.append(
                {
                    "id": check_id,
                    "description": description,
                    "document_id": document_id,
                    "anchor": anchor,
                    "required_terms": required_terms,
                    "evidence_current": evidence_ok,
                }
            )

        forbidden_assumptions = task_value.get("forbidden_assumptions")
        if not isinstance(forbidden_assumptions, list) or any(
            not isinstance(value, str) or not value.strip()
            for value in forbidden_assumptions
        ):
            task_errors.append(f"{label}.forbidden_assumptions must be a string array")
            forbidden_assumptions = []

        errors.extend(task_errors)
        task_reports.append(
            {
                "id": task_id,
                "category": category,
                "question": question,
                "primary_document_id": primary_document_id,
                "supporting_document_ids": supporting_document_ids,
                "retrieval_checks": retrieval_reports,
                "answer_checks": answer_reports,
                "forbidden_assumptions": list(forbidden_assumptions),
                "passed": not task_errors,
            }
        )

    for category in REQUIRED_CATEGORIES:
        if category_counts[category] < 2:
            errors.append(f"AI evaluation category needs at least two tasks: {category}")

    retrieval_success_rate = (
        retrieval_pass_count / query_count if query_count else 0.0
    )
    if retrieval_success_rate < float(minimum_rate):
        errors.append(
            "AI evaluation retrieval success rate "
            f"{retrieval_success_rate:.3f} is below {float(minimum_rate):.3f}"
        )

    unique_errors = sorted(set(errors))
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "source_path": source_path,
        "source_sha256": _normalized_sha256(source_file),
        "source_ref": current.get("source_ref"),
        "task_count": len(task_reports),
        "category_counts": {
            category: category_counts[category] for category in REQUIRED_CATEGORIES
        },
        "retrieval": {
            "query_count": query_count,
            "pass_count": retrieval_pass_count,
            "success_rate": round(retrieval_success_rate, 6),
            "top_1_rate": round(top_one_count / query_count, 6) if query_count else 0.0,
            "top_3_rate": round(top_three_count / query_count, 6) if query_count else 0.0,
            "mean_reciprocal_rank": (
                round(reciprocal_rank_total / query_count, 6) if query_count else 0.0
            ),
            "minimum_success_rate": minimum_rate,
        },
        "passed_task_count": sum(1 for task in task_reports if task["passed"]),
        "error_count": len(unique_errors),
        "errors": unique_errors,
        "tasks": task_reports,
    }


def _render(report: dict[str, object]) -> str:
    return json.dumps(report, indent=2, ensure_ascii=True) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate FOnline documentation retrieval and answer-evidence tasks."
    )
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--source", default=DEFAULT_SOURCE)
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST)
    parser.add_argument("--search", default=DEFAULT_SEARCH)
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    try:
        report = evaluate(root, args.source, args.manifest, args.search)
    except ValueError as exception:
        print(f"Unable to evaluate AI documentation tasks: {exception}", file=sys.stderr)
        return 1

    output_path = root / args.output
    rendered = _render(report)
    stale = not output_path.is_file() or output_path.read_text(encoding="utf-8") != rendered
    if args.write:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(rendered, encoding="utf-8", newline="\n")
    elif stale:
        print(
            f"AI documentation evaluation report is missing or stale: {args.output}; "
            f"run python {GENERATED_BY} --write",
            file=sys.stderr,
        )
        return 1

    if report["errors"]:
        for error in report["errors"][:50]:
            print(f"ERROR: {error}", file=sys.stderr)
        print(
            f"AI documentation evaluation failed with {report['error_count']} error(s)",
            file=sys.stderr,
        )
        return 1

    retrieval = report["retrieval"]
    print(
        "AI documentation evaluation passed: "
        f"{report['task_count']} tasks, {retrieval['query_count']} retrieval checks, "
        f"{retrieval['success_rate']:.1%} success, "
        f"{retrieval['mean_reciprocal_rank']:.3f} MRR"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
