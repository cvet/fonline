from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path


SCHEMA_VERSION = 1
GENERATED_BY = "BuildTools/docs_ai_model_review.py"
DEFAULT_SOURCE = "Docs/ai-evaluation.json"
REVIEW_STATUSES = {"pass", "fail", "not-observable", "not-reviewed"}


def _load_json(path: Path, label: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exception:
        raise ValueError(f"unable to read {label} {path.as_posix()}: {exception}") from exception
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be a JSON object")
    return value


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _write_json(path: Path, value: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    temporary.replace(path)


def build_template(
    source: dict[str, object],
    run: dict[str, object],
    *,
    run_path: str,
    run_sha256: str,
    reviewer: str,
) -> dict[str, object]:
    source_tasks = source.get("tasks")
    run_tasks = run.get("tasks")
    if not isinstance(source_tasks, list) or not isinstance(run_tasks, list):
        raise ValueError("evaluation source and model run must contain task arrays")
    source_by_id = {
        str(task["id"]): task
        for task in source_tasks
        if isinstance(task, dict) and isinstance(task.get("id"), str)
    }
    run_by_id = {
        str(task["id"]): task
        for task in run_tasks
        if isinstance(task, dict) and isinstance(task.get("id"), str)
    }
    if set(source_by_id) != set(run_by_id):
        missing = sorted(set(source_by_id) - set(run_by_id))
        extra = sorted(set(run_by_id) - set(source_by_id))
        raise ValueError(f"run task set mismatch; missing={missing}, extra={extra}")

    tasks: list[dict[str, object]] = []
    for task_id, task in source_by_id.items():
        run_task = run_by_id[task_id]
        observations = run_task.get("observations")
        tasks.append(
            {
                "id": task_id,
                "category": task.get("category"),
                "run_status": run_task.get("status"),
                "primary_document_id": task.get("primary_document_id"),
                "primary_document_selected": (
                    observations.get("primary_document_selected")
                    if isinstance(observations, dict)
                    else None
                ),
                "source_selection": {"status": "not-reviewed", "notes": ""},
                "answer_checks": [
                    {
                        "id": check.get("id"),
                        "description": check.get("description"),
                        "status": "not-reviewed",
                        "notes": "",
                    }
                    for check in task.get("answer_checks", [])
                    if isinstance(check, dict)
                ],
                "forbidden_assumptions": [
                    {
                        "description": description,
                        "status": "not-reviewed",
                        "notes": "",
                    }
                    for description in task.get("forbidden_assumptions", [])
                    if isinstance(description, str)
                ],
                "grounding": {"status": "not-reviewed", "notes": ""},
                "final_task_success": None,
                "notes": "",
            }
        )

    model_info = run.get("model_info")
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "reviewed_at": None,
        "reviewer": {
            "id": reviewer,
            "method": "answer-by-answer semantic review against hidden source rubrics",
            "independent_from_evaluated_models": True,
        },
        "run": {
            "path": run_path,
            "sha256": run_sha256,
            "schema_version": run.get("schema_version"),
            "source_ref": run.get("source_ref"),
            "input_hashes": run.get("input_hashes"),
            "model_family": run.get("model_family"),
            "model": run.get("model"),
            "model_digest": (
                model_info.get("digest") if isinstance(model_info, dict) else None
            ),
            "provider": run.get("provider"),
            "parameters": run.get("parameters"),
            "completed_at": run.get("completed_at"),
        },
        "status_meaning": {
            "answer_check_pass": "the answer semantically satisfies the check",
            "forbidden_assumption_pass": "the answer does not violate the forbidden assumption",
            "not_observable": "the supplied model input did not make the criterion reviewable",
        },
        "tasks": tasks,
        "summary": {
            "task_count": len(tasks),
            "reviewed_task_count": 0,
            "successful_task_count": 0,
            "final_task_success_rate": None,
        },
    }


def _status(value: object, label: str, *, complete: bool) -> str:
    if value not in REVIEW_STATUSES:
        raise ValueError(f"{label} has invalid review status: {value}")
    if complete and value == "not-reviewed":
        raise ValueError(f"{label} is not reviewed")
    return str(value)


def validate_run_provenance(
    root: Path,
    review: dict[str, object],
    *,
    require_run: bool,
) -> None:
    run_record = review.get("run")
    if not isinstance(run_record, dict):
        raise ValueError("review.run must be an object")

    for field in ("source_ref", "model_family", "model", "completed_at"):
        if not isinstance(run_record.get(field), str) or not run_record[field]:
            raise ValueError(f"review.run.{field} must be present")
    model_digest = run_record.get("model_digest")
    if (
        not isinstance(model_digest, str)
        or len(model_digest) != 64
        or any(character not in "0123456789abcdef" for character in model_digest)
    ):
        raise ValueError("review.run.model_digest must be a lowercase SHA-256 digest")
    provider = run_record.get("provider")
    if (
        not isinstance(provider, dict)
        or not isinstance(provider.get("id"), str)
        or not provider["id"]
        or not isinstance(provider.get("version"), str)
        or not provider["version"]
    ):
        raise ValueError("review.run.provider must identify an exact provider version")
    if not isinstance(run_record.get("parameters"), dict):
        raise ValueError("review.run.parameters must be an object")
    input_hashes = run_record.get("input_hashes")
    if not isinstance(input_hashes, dict) or not input_hashes:
        raise ValueError("review.run.input_hashes must be a non-empty object")
    for input_path, digest in input_hashes.items():
        if (
            not isinstance(input_path, str)
            or not input_path
            or not isinstance(digest, str)
            or len(digest) != 64
            or any(character not in "0123456789abcdef" for character in digest)
        ):
            raise ValueError("review.run.input_hashes must contain SHA-256 digests")
    run_path_value = run_record.get("path")
    run_sha256 = run_record.get("sha256")
    if not isinstance(run_path_value, str) or not run_path_value:
        raise ValueError("review.run.path must be present")
    if (
        not isinstance(run_sha256, str)
        or len(run_sha256) != 64
        or any(character not in "0123456789abcdef" for character in run_sha256)
    ):
        raise ValueError("review.run.sha256 must be a lowercase SHA-256 digest")

    relative_path = Path(run_path_value)
    if relative_path.is_absolute():
        raise ValueError("review.run.path must be relative to the Engine root")
    run_path = (root / relative_path).resolve()
    try:
        run_path.relative_to(root)
    except ValueError as exception:
        raise ValueError("review.run.path escapes the Engine root") from exception
    if not run_path.exists():
        if require_run:
            raise ValueError(f"model-family run is unavailable: {run_path_value}")
        return
    if _sha256(run_path) != run_sha256:
        raise ValueError(f"model-family run SHA-256 mismatch: {run_path_value}")

    raw_run = _load_json(run_path, "model-family run")
    model_info = raw_run.get("model_info")
    expected = {
        "schema_version": raw_run.get("schema_version"),
        "source_ref": raw_run.get("source_ref"),
        "input_hashes": raw_run.get("input_hashes"),
        "model_family": raw_run.get("model_family"),
        "model": raw_run.get("model"),
        "model_digest": model_info.get("digest") if isinstance(model_info, dict) else None,
        "provider": raw_run.get("provider"),
        "parameters": raw_run.get("parameters"),
        "completed_at": raw_run.get("completed_at"),
    }
    for field, expected_value in expected.items():
        if run_record.get(field) != expected_value:
            raise ValueError(f"review.run.{field} does not match the raw run")


def validate_and_finalize(
    source: dict[str, object],
    review: dict[str, object],
    *,
    complete: bool,
    update_timestamp: bool = True,
) -> dict[str, object]:
    if review.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"review schema_version must be {SCHEMA_VERSION}")
    reviewer = review.get("reviewer")
    if not isinstance(reviewer, dict) or not isinstance(reviewer.get("id"), str):
        raise ValueError("reviewer.id must be present")
    source_tasks = source.get("tasks")
    review_tasks = review.get("tasks")
    if not isinstance(source_tasks, list) or not isinstance(review_tasks, list):
        raise ValueError("source and review must contain task arrays")
    source_by_id = {
        str(task["id"]): task
        for task in source_tasks
        if isinstance(task, dict) and isinstance(task.get("id"), str)
    }
    review_by_id = {
        str(task["id"]): task
        for task in review_tasks
        if isinstance(task, dict) and isinstance(task.get("id"), str)
    }
    if set(source_by_id) != set(review_by_id):
        raise ValueError("review task ids do not match the evaluation source")

    successful = 0
    reviewed = 0
    for task_id, source_task in source_by_id.items():
        task = review_by_id[task_id]
        source_selection = task.get("source_selection")
        grounding = task.get("grounding")
        if not isinstance(source_selection, dict) or not isinstance(grounding, dict):
            raise ValueError(f"{task_id} source_selection and grounding must be objects")
        statuses = [
            _status(source_selection.get("status"), f"{task_id}.source_selection", complete=complete),
            _status(grounding.get("status"), f"{task_id}.grounding", complete=complete),
        ]

        expected_checks = {
            str(check["id"]): check
            for check in source_task.get("answer_checks", [])
            if isinstance(check, dict) and isinstance(check.get("id"), str)
        }
        checks = task.get("answer_checks")
        if not isinstance(checks, list) or any(not isinstance(check, dict) for check in checks):
            raise ValueError(f"{task_id}.answer_checks must be an object array")
        check_by_id = {str(check.get("id")): check for check in checks}
        if set(check_by_id) != set(expected_checks):
            raise ValueError(f"{task_id} answer check ids do not match the source")
        statuses.extend(
            _status(check["status"], f"{task_id}.{check_id}", complete=complete)
            for check_id, check in check_by_id.items()
        )

        expected_forbidden = [
            value for value in source_task.get("forbidden_assumptions", []) if isinstance(value, str)
        ]
        forbidden = task.get("forbidden_assumptions")
        if not isinstance(forbidden, list) or any(not isinstance(item, dict) for item in forbidden):
            raise ValueError(f"{task_id}.forbidden_assumptions must be an object array")
        descriptions = [str(item.get("description")) for item in forbidden]
        if descriptions != expected_forbidden:
            raise ValueError(f"{task_id} forbidden assumptions do not match the source")
        statuses.extend(
            _status(item.get("status"), f"{task_id}.forbidden[{index}]", complete=complete)
            for index, item in enumerate(forbidden)
        )

        is_reviewed = all(status != "not-reviewed" for status in statuses)
        success = task.get("run_status") == "completed" and all(
            status == "pass" for status in statuses
        )
        task["final_task_success"] = success if is_reviewed else None
        if is_reviewed:
            reviewed += 1
        if success:
            successful += 1

    review["summary"] = {
        "task_count": len(review_tasks),
        "reviewed_task_count": reviewed,
        "successful_task_count": successful,
        "final_task_success_rate": successful / len(review_tasks) if complete else None,
    }
    if complete and update_timestamp:
        review["reviewed_at"] = _utc_now()
    elif complete and not isinstance(review.get("reviewed_at"), str):
        raise ValueError("reviewed_at must be present in a completed review")
    return review


def apply_suggestion(
    review: dict[str, object],
    suggestion: dict[str, object],
) -> dict[str, object]:
    review_tasks = review.get("tasks")
    suggestion_tasks = suggestion.get("tasks")
    if not isinstance(review_tasks, list) or not isinstance(suggestion_tasks, list):
        raise ValueError("review and suggestion must contain task arrays")
    review_by_id = {
        str(task["id"]): task
        for task in review_tasks
        if isinstance(task, dict) and isinstance(task.get("id"), str)
    }
    suggestion_by_id = {
        str(task["id"]): task
        for task in suggestion_tasks
        if isinstance(task, dict) and isinstance(task.get("id"), str)
    }
    if set(review_by_id) != set(suggestion_by_id):
        raise ValueError("suggestion task ids do not match the review")
    for task_id, task in review_by_id.items():
        proposed = suggestion_by_id[task_id]
        source_selection = task.get("source_selection")
        grounding = task.get("grounding")
        if not isinstance(source_selection, dict) or not isinstance(grounding, dict):
            raise ValueError(f"{task_id} review routing fields must be objects")
        source_selection["status"] = _status(
            proposed.get("source_selection"), f"{task_id}.source_selection", complete=True
        )
        grounding["status"] = _status(
            proposed.get("grounding"), f"{task_id}.grounding", complete=True
        )

        proposed_checks = proposed.get("answer_checks")
        review_checks = task.get("answer_checks")
        if not isinstance(proposed_checks, list) or not isinstance(review_checks, list):
            raise ValueError(f"{task_id} answer checks must be arrays")
        proposed_by_id = {
            str(check["id"]): check
            for check in proposed_checks
            if isinstance(check, dict) and isinstance(check.get("id"), str)
        }
        review_check_by_id = {
            str(check["id"]): check
            for check in review_checks
            if isinstance(check, dict) and isinstance(check.get("id"), str)
        }
        if set(proposed_by_id) != set(review_check_by_id):
            raise ValueError(f"{task_id} suggestion answer checks do not match the review")
        for check_id, check in review_check_by_id.items():
            check["status"] = _status(
                proposed_by_id[check_id].get("status"),
                f"{task_id}.{check_id}",
                complete=True,
            )

        proposed_forbidden = proposed.get("forbidden_assumptions")
        review_forbidden = task.get("forbidden_assumptions")
        if not isinstance(proposed_forbidden, list) or not isinstance(review_forbidden, list):
            raise ValueError(f"{task_id} forbidden assumptions must be arrays")
        if len(proposed_forbidden) != len(review_forbidden):
            raise ValueError(f"{task_id} forbidden assumption counts do not match")
        for index, item in enumerate(review_forbidden):
            if not isinstance(item, dict):
                raise ValueError(f"{task_id} forbidden assumption review must be an object")
            item["status"] = _status(
                proposed_forbidden[index],
                f"{task_id}.forbidden[{index}]",
                complete=True,
            )
        notes = proposed.get("notes")
        if not isinstance(notes, str):
            raise ValueError(f"{task_id} suggestion notes must be a string")
        task["notes"] = notes
    return review


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Create or validate a semantic AI-doc review.")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--source", default=DEFAULT_SOURCE)
    parser.add_argument("--run")
    parser.add_argument("--output", required=True)
    parser.add_argument("--reviewer")
    parser.add_argument("--suggestion")
    parser.add_argument("--write-template", action="store_true")
    parser.add_argument("--apply-suggestion", action="store_true")
    parser.add_argument("--finalize", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument(
        "--require-run",
        action="store_true",
        help="Require and hash-check the raw Workspace run instead of accepting a compact review alone.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    if sum(
        (
            arguments.write_template,
            arguments.apply_suggestion,
            arguments.finalize,
            arguments.check,
        )
    ) != 1:
        raise SystemExit(
            "select exactly one of --write-template, --apply-suggestion, --finalize, or --check"
        )
    root = Path(arguments.root).resolve()
    output = Path(arguments.output)
    if not output.is_absolute():
        output = root / output
    try:
        source = _load_json(root / arguments.source, "AI evaluation source")
        if arguments.write_template:
            if not arguments.run or not arguments.reviewer:
                raise ValueError("--write-template requires --run and --reviewer")
            run_path = Path(arguments.run)
            if not run_path.is_absolute():
                run_path = root / run_path
            review = build_template(
                source,
                _load_json(run_path, "model-family run"),
                run_path=run_path.relative_to(root).as_posix(),
                run_sha256=_sha256(run_path),
                reviewer=arguments.reviewer,
            )
            _write_json(output, review)
        elif arguments.apply_suggestion:
            if not arguments.suggestion:
                raise ValueError("--apply-suggestion requires --suggestion")
            suggestion_path = Path(arguments.suggestion)
            if not suggestion_path.is_absolute():
                suggestion_path = root / suggestion_path
            review = apply_suggestion(
                _load_json(output, "model-family review"),
                _load_json(suggestion_path, "semantic review suggestion"),
            )
            _write_json(output, review)
        else:
            original_review = _load_json(output, "model-family review")
            validate_run_provenance(
                root,
                original_review,
                require_run=arguments.require_run,
            )
            review = validate_and_finalize(
                source,
                original_review,
                complete=True,
                update_timestamp=arguments.finalize,
            )
            if arguments.finalize:
                _write_json(output, review)
            elif review != _load_json(output, "model-family review"):
                raise ValueError("model-family review aggregates are stale; run --finalize")
        summary = review["summary"]
        sys.stdout.write(
            f"AI model review: {summary['reviewed_task_count']}/{summary['task_count']} reviewed, "
            f"{summary['successful_task_count']} successful.\n"
        )
        return 0
    except (OSError, ValueError) as exception:
        sys.stderr.write(f"AI model review failed: {exception}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
