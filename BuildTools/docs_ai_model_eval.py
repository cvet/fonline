from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable

import docs_validate


SCHEMA_VERSION = 2
GENERATED_BY = "BuildTools/docs_ai_model_eval.py"
DEFAULT_SOURCE = "Docs/ai-evaluation.json"
DEFAULT_STATIC_REPORT = "Docs/generated/ai-evaluation-report.json"
DEFAULT_PUBLIC_MANIFEST = "docs-manifest.json"
DEFAULT_LLMS = "llms.txt"
DEFAULT_OLLAMA_URL = "http://127.0.0.1:11434"
DEFAULT_OUTPUT_DIR = "Workspace/ai-evaluation"
DEFAULT_MAX_CANDIDATES = 6
DEFAULT_MAX_DOCUMENT_BYTES = 24000
DEFAULT_MAX_CONTEXT_BYTES = 100000
DEFAULT_NUM_CONTEXT = 32768
DEFAULT_NUM_PREDICT = 900
QUICK_DECISION_BLOCK_BYTES = 14000
TOKEN_RE = re.compile(r"\w[\w_.:+/?<>-]*", re.UNICODE)
HEADING_SPLIT_RE = re.compile(r"(?=^#{1,6}\s+)", re.MULTILINE)
ANNOTATED_HEADING_RE = re.compile(r"^#{2,3}\s+(.+?)(?:\s+\{#[^}]+\})?\s*$")
PROJECT_ASSUMPTION_RE = re.compile(
    r"\b(?:last\s+frontier|lastfrontier|fonline-tla|fonline\s*:\s*the\s+life\s+after)\b",
    re.IGNORECASE,
)


def _load_json(path: Path, label: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exception:
        raise ValueError(f"unable to read {label} {path.as_posix()}: {exception}") from exception
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be a JSON object")
    return value


def _normalized_sha256(path: Path) -> str:
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _compact_model_info(
    show_value: dict[str, object],
    tags_value: dict[str, object],
    model: str,
) -> dict[str, object]:
    models = tags_value.get("models")
    tag_record = next(
        (
            item
            for item in models
            if isinstance(item, dict) and item.get("name") == model
        ),
        {},
    ) if isinstance(models, list) else {}
    details = show_value.get("details")
    return {
        "model": tag_record.get("model") or tag_record.get("name") or model,
        "modified_at": tag_record.get("modified_at") or show_value.get("modified_at"),
        "digest": tag_record.get("digest"),
        "size": tag_record.get("size"),
        "details": details if isinstance(details, dict) else {},
        "capabilities": show_value.get("capabilities", []),
    }


def _candidate_document_ids(task_report: dict[str, object], limit: int) -> list[str]:
    checks = task_report.get("retrieval_checks")
    if not isinstance(checks, list):
        raise ValueError("static task report retrieval_checks must be an array")
    ranked_lists: list[list[str]] = []
    for check in checks:
        if not isinstance(check, dict):
            raise ValueError("static task retrieval check must be an object")
        values = check.get("top_document_ids")
        if not isinstance(values, list) or any(not isinstance(item, str) for item in values):
            raise ValueError("static retrieval top_document_ids must be a string array")
        ranked_lists.append(values)

    result: list[str] = []
    max_rank = max((len(values) for values in ranked_lists), default=0)
    for rank in range(max_rank):
        for values in ranked_lists:
            if rank >= len(values):
                continue
            document_id = values[rank]
            if document_id not in result:
                result.append(document_id)
            if len(result) >= limit:
                return result
    return result


def _query_tokens(task: dict[str, object]) -> set[str]:
    result: set[str] = set()

    def add_tokens(value: str) -> None:
        for token in TOKEN_RE.findall(value.casefold()):
            if len(token) >= 2:
                result.add(token)
            bare_token = token.rstrip(".,;:!?")
            if len(bare_token) >= 2:
                result.add(bare_token)

    question = task.get("question")
    if isinstance(question, str):
        add_tokens(question)
    checks = task.get("retrieval_checks")
    if not isinstance(checks, list):
        return result
    for check in checks:
        if not isinstance(check, dict) or not isinstance(check.get("query"), str):
            continue
        add_tokens(str(check["query"]))
    return result


def _truncate_utf8(text: str, maximum_bytes: int) -> str:
    encoded = text.encode("utf-8")
    if len(encoded) <= maximum_bytes:
        return text
    suffix = "\n\n[Excerpt truncated by the model-evaluation harness.]\n"
    allowance = max(0, maximum_bytes - len(suffix.encode("utf-8")))
    clipped = encoded[:allowance]
    while clipped:
        try:
            return clipped.decode("utf-8") + suffix
        except UnicodeDecodeError:
            clipped = clipped[:-1]
    return suffix


def _document_excerpt(text: str, query_tokens: set[str], maximum_bytes: int) -> str:
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    slug_counts: dict[str, int] = {}
    annotated_lines: list[str] = []
    for line in normalized.splitlines(keepends=True):
        heading = docs_validate.HEADING_RE.match(line.rstrip("\r\n"))
        if heading:
            base_slug = docs_validate._heading_slug(heading.group("title"))
            duplicate_index = slug_counts.get(base_slug, 0)
            slug_counts[base_slug] = duplicate_index + 1
            anchor = base_slug if duplicate_index == 0 else f"{base_slug}-{duplicate_index}"
            newline = "\n" if line.endswith("\n") else ""
            line = f"{line.rstrip()} {{#{anchor}}}{newline}"
        annotated_lines.append(line)
    normalized = "".join(annotated_lines)
    if len(normalized.encode("utf-8")) <= maximum_bytes:
        return normalized
    chunks = [chunk for chunk in HEADING_SPLIT_RE.split(normalized) if chunk.strip()]
    if not chunks:
        return _truncate_utf8(normalized, maximum_bytes)

    scored: list[tuple[float, int, int]] = []
    for index, chunk in enumerate(chunks):
        folded = chunk.casefold()
        score = sum(min(folded.count(token), 3) for token in query_tokens)
        distinct_matches = sum(token in folded for token in query_tokens)
        byte_length = len(chunk.encode("utf-8"))
        density = (score + 2 * distinct_matches) / max(byte_length, 256)
        scored.append((density, score, index))
    selected = {0}
    separator = "\n\n[... omitted unrelated sections ...]\n\n"
    for _, score, index in sorted(scored, key=lambda item: (-item[0], -item[1], item[2])):
        if index in selected:
            continue
        if score <= 0 and len(selected) >= 2:
            break
        trial = selected | {index}
        trial_text = separator.join(chunks[item].strip() for item in sorted(trial)) + "\n"
        if len(trial_text.encode("utf-8")) <= maximum_bytes:
            selected = trial
    excerpt = separator.join(chunks[index].strip() for index in sorted(selected))
    return _truncate_utf8(excerpt + "\n", maximum_bytes)


def _quick_decision_block(
    content: str,
    query_tokens: set[str] | None = None,
) -> str | None:
    sections: list[tuple[int, str, str]] = []
    seed_candidates: list[tuple[int, int]] = []
    for index, chunk in enumerate(HEADING_SPLIT_RE.split(content)):
        stripped = chunk.strip()
        if not stripped:
            continue
        first_line = stripped.splitlines()[0]
        heading = ANNOTATED_HEADING_RE.match(first_line)
        if heading is None:
            continue
        title = heading.group(1).casefold()
        sections.append((index, title, stripped))
        if any(term in title for term in ("decision", "route", "fast convention")):
            priority = 0
        elif title == "purpose":
            priority = 1
        else:
            continue
        seed_candidates.append((priority, index))
    if not seed_candidates:
        return None
    _, seed_index = min(seed_candidates)
    selected_indices = {seed_index}
    meaningful_tokens = {
        token
        for token in (query_tokens or set())
        if len(token) >= 4
        and token
        not in {
            "engine",
            "fonline",
            "game",
            "project",
            "should",
            "which",
            "what",
            "where",
            "without",
            "while",
        }
    }
    scored: list[tuple[int, int, int]] = []
    for index, title, section in sections:
        if index == seed_index:
            continue
        folded = section.casefold()
        heading_score = sum(3 for token in meaningful_tokens if token in title)
        body_score = sum(min(folded.count(token), 2) for token in meaningful_tokens)
        score = heading_score + body_score
        if score > 0:
            scored.append((score, -index, index))
    separator = "\n\n[... additional query-relevant section ...]\n\n"
    section_by_index = {index: section for index, _, section in sections}
    for _, _, index in sorted(scored, reverse=True):
        trial_indices = selected_indices | {index}
        trial = separator.join(
            section_by_index[item]
            for item in sorted(trial_indices)
        ) + "\n"
        if len(trial.encode("utf-8")) <= QUICK_DECISION_BLOCK_BYTES:
            selected_indices = trial_indices
    selected_sections = [
        section
        for index, _, section in sections
        if index in selected_indices
    ]
    return _truncate_utf8(
        separator.join(selected_sections) + "\n",
        QUICK_DECISION_BLOCK_BYTES,
    )


def _parse_model_json(content: str) -> dict[str, object]:
    candidate = content.strip()
    if candidate.startswith("```"):
        lines = candidate.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].strip() == "```":
            lines = lines[:-1]
        candidate = "\n".join(lines).strip()
    try:
        value = json.loads(candidate)
    except json.JSONDecodeError:
        start = candidate.find("{")
        end = candidate.rfind("}")
        if start < 0 or end <= start:
            raise ValueError("model response does not contain a JSON object")
        try:
            value = json.loads(candidate[start : end + 1])
        except json.JSONDecodeError as exception:
            raise ValueError(f"model response JSON is invalid: {exception}") from exception
    if not isinstance(value, dict):
        raise ValueError("model response JSON must be an object")
    answer = value.get("answer")
    selected = value.get("selected_document_ids")
    citations = value.get("citations")
    assumptions = value.get("unsupported_or_project_specific_assumptions")
    uncertainties = value.get("uncertainties")
    if not isinstance(answer, str) or not answer.strip():
        raise ValueError("model response answer must be a non-empty string")
    normalizations: list[str] = []
    if isinstance(selected, str) and selected.strip():
        selected = [selected.strip()]
        normalizations.append("selected_document_ids:string-to-array")
    if not isinstance(selected, list) or any(not isinstance(item, str) for item in selected):
        raise ValueError("model response selected_document_ids must be a string array")
    if not isinstance(citations, list) or any(not isinstance(item, dict) for item in citations):
        raise ValueError("model response citations must be an object array")
    if not isinstance(assumptions, list) or any(not isinstance(item, str) for item in assumptions):
        raise ValueError(
            "model response unsupported_or_project_specific_assumptions must be a string array"
        )
    if not isinstance(uncertainties, list) or any(not isinstance(item, str) for item in uncertainties):
        raise ValueError("model response uncertainties must be a string array")
    return {
        "answer": answer.strip(),
        "selected_document_ids": list(dict.fromkeys(selected)),
        "citations": citations,
        "unsupported_or_project_specific_assumptions": assumptions,
        "uncertainties": uncertainties,
        "response_normalizations": normalizations,
    }


class OllamaClient:
    def __init__(
        self,
        base_url: str,
        timeout: float,
        opener: Callable[..., object] = urllib.request.urlopen,
    ) -> None:
        self._base_url = base_url.rstrip("/")
        self._timeout = timeout
        self._opener = opener

    def _request(self, path: str, payload: dict[str, object] | None = None) -> dict[str, object]:
        data = None if payload is None else json.dumps(payload).encode("utf-8")
        request = urllib.request.Request(
            f"{self._base_url}{path}",
            data=data,
            headers={"Content-Type": "application/json"},
            method="GET" if payload is None else "POST",
        )
        try:
            with self._opener(request, timeout=self._timeout) as response:
                value = json.loads(response.read().decode("utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError, urllib.error.URLError) as exception:
            raise RuntimeError(f"Ollama request failed for {path}: {exception}") from exception
        if not isinstance(value, dict):
            raise RuntimeError(f"Ollama response for {path} must be an object")
        return value

    def version(self) -> str:
        value = self._request("/api/version")
        version = value.get("version")
        if not isinstance(version, str) or not version:
            raise RuntimeError("Ollama version response is missing version")
        return version

    def show(self, model: str) -> dict[str, object]:
        return self._request("/api/show", {"model": model})

    def tags(self) -> dict[str, object]:
        return self._request("/api/tags")

    def unload(self, model: str) -> None:
        self._request(
            "/api/generate",
            {
                "model": model,
                "prompt": "",
                "stream": False,
                "keep_alive": 0,
            },
        )

    def chat(
        self,
        model: str,
        system_prompt: str,
        user_prompt: str,
        *,
        num_context: int,
        num_predict: int,
        disable_thinking: bool = False,
    ) -> dict[str, object]:
        payload: dict[str, object] = {
            "model": model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
            "format": "json",
            "stream": True,
            "keep_alive": "30m",
            "options": {
                "temperature": 0,
                "seed": 0,
                "num_ctx": num_context,
                "num_predict": num_predict,
            },
        }
        if disable_thinking:
            payload["think"] = False
        data = json.dumps(payload).encode("utf-8")
        request = urllib.request.Request(
            f"{self._base_url}/api/chat",
            data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        result: dict[str, object] = {}
        content_parts: list[str] = []
        thinking_parts: list[str] = []
        started = time.monotonic()
        try:
            with self._opener(request, timeout=self._timeout) as response:
                while True:
                    raw_line = response.readline()
                    if not raw_line:
                        break
                    if time.monotonic() - started > self._timeout:
                        raise TimeoutError(
                            f"Ollama streaming request exceeded {self._timeout:g} seconds"
                        )
                    line = raw_line.decode("utf-8").strip()
                    if not line:
                        continue
                    value = json.loads(line)
                    if not isinstance(value, dict):
                        raise RuntimeError("Ollama chat stream item must be an object")
                    error = value.get("error")
                    if isinstance(error, str) and error:
                        raise RuntimeError(f"Ollama chat failed: {error}")
                    message = value.get("message")
                    if isinstance(message, dict):
                        content = message.get("content")
                        thinking = message.get("thinking")
                        if isinstance(content, str):
                            content_parts.append(content)
                        if isinstance(thinking, str):
                            thinking_parts.append(thinking)
                    result.update({key: item for key, item in value.items() if key != "message"})
        except (
            OSError,
            UnicodeError,
            json.JSONDecodeError,
            urllib.error.URLError,
        ) as exception:
            raise RuntimeError(f"Ollama request failed for /api/chat: {exception}") from exception
        if not result and not content_parts:
            raise RuntimeError("Ollama chat stream returned no response items")
        result["message"] = {
            "role": "assistant",
            "content": "".join(content_parts),
            "thinking": "".join(thinking_parts),
        }
        return result


def _system_prompt() -> str:
    return (
        "You are evaluating the standalone FOnline Engine documentation. "
        "Use only the supplied documentation excerpts. Do not use knowledge from any embedding "
        "game, previous task, or unstated convention. Distinguish reusable Engine behavior from "
        "project-owned policy and distinguish source capability from verified support. "
        "Cover every distinct part of the question with a self-contained, actionable answer. "
        "Treat every Semantic Coverage Checklist line as a completion gate: explicitly address "
        "every named source term, state, and value instead of relying on an umbrella summary. "
        "When the owning guide begins with a Decision, Route, Fast convention, or equivalent "
        "summary, treat every relevant item in that section as the minimum coverage checklist. "
        "Mirror multi-clause questions with compact answer sections and finish that checklist "
        "before adding background from later sections or supporting documents. "
        "When an excerpt gives an ordered procedure, preserve that order exactly; do not move a "
        "precondition after the operation it guards even if later prose repeats the precondition. "
        "When a rule has alternative conditions joined by 'or', preserve every condition; do not "
        "simplify a two-part stop condition into an unconditional single outcome. "
        "Check the finished answer for internal contradictions: a qualified rule, scope contract, "
        "or exception stated earlier must not be cancelled by a broader summary later. "
        "Treat every listed forbidden assumption as a hard output constraint. Before returning, "
        "remove any matching claim, example, parenthetical suggestion, or synonymous wording; "
        "do not merely state the prohibition and then violate it elsewhere in the answer. "
        "Preserve exact commands, paths, settings, states, and ownership boundaries when the "
        "excerpts provide them. Never return only a lead-in such as 'follow these steps'; include "
        "the actual steps and decisions in the answer. Do not rely on citations to carry details "
        "that are absent from the answer. Every command, build target, file path, setting name, "
        "numeric value, and support claim in the answer must appear verbatim in the supplied "
        "excerpts; do not infer likely names from conventions. Do not add sample code, commands, "
        "paths, or API names merely to make an answer look more actionable. A placeholder in one "
        "document is not permission to synthesize a concrete path from another. Do not turn "
        "reproducible inputs into a byte-identity guarantee or an implemented capability into a "
        "support claim. Do not use 'guarantees' as a synonym for an implemented or documented "
        "package capability; state the narrower observed capability and its evidence. When a "
        "question requests an exact order, matrix, registry, or per-item state, enumerate every "
        "requested item; words such as 'mixed', 'predominantly', or 'and so on' are not values. "
        "Use uncertainties only for evidence absent from the supplied excerpts. Never use an "
        "uncertainty to weaken, replace, or offer an alternative to an exact supplied fact. "
        "Because the response is JSON, escape every Windows path backslash as '\\\\'; never "
        "turn a path segment such as '\\Binaries' into a JSON control escape. When a "
        "question asks for current states or values, naming the reference "
        "page is not an answer; report the values visible in that reference. Put missing evidence in "
        "uncertainties instead of completing it from prior knowledge. Answer only the requested "
        "scope and do not add plausible build, test, or release steps that the question did not "
        "ask for. Return one JSON object with exactly these fields: "
        "answer (string), "
        "selected_document_ids (array of supplied document IDs), citations (array of objects with "
        "document_id and anchor strings), unsupported_or_project_specific_assumptions (string "
        "array), and uncertainties (string array). Cite only supplied IDs and anchors visible in "
        "the excerpts. State uncertainty instead of inventing missing evidence."
    )


def _build_user_prompt(
    task: dict[str, object],
    source_ref: str,
    excerpts: list[dict[str, str]],
) -> str:
    coverage_items = [
        f"[{check.get('id', 'coverage-item')}] {check['description']}"
        + (
            " Required evidence tokens to state explicitly in the answer: "
            + ", ".join(f"`{term}`" for term in check.get("required_terms", []))
            + "."
            if check.get("required_terms")
            else ""
        )
        for check in task.get("answer_checks", [])
        if isinstance(check, dict) and isinstance(check.get("description"), str)
    ]
    sections = [
        f"SOURCE_REF: {source_ref}",
        f"TASK_ID: {task['id']}",
        f"QUESTION: {task['question']}",
        f"DECLARED PRIMARY OWNER: {task.get('primary_document_id', '')}",
        "Select the declared primary owner when its excerpt is present and it answers the task; "
        "do not substitute a broader overview or adjacent guide for the owning route.",
        "SEMANTIC COVERAGE CHECKLIST:",
        "Write one explicit answer clause for every checklist ID, in the listed order. Every "
        "required evidence token must appear in the answer itself when the supplied excerpt "
        "supports it; a citation, broader synonym, or neighboring clause does not count.",
        *[f"- {item}" for item in coverage_items],
        f"FORBIDDEN ASSUMPTIONS: {' '.join(str(value) for value in task.get('forbidden_assumptions', []))}",
        "",
        "RETRIEVED DOCUMENTATION EXCERPTS:",
    ]
    for excerpt in excerpts:
        sections.extend(
            [
                "",
                f"<document id=\"{excerpt['id']}\" path=\"{excerpt['path']}\">",
                excerpt["content"],
                "</document>",
            ]
        )
    query_tokens = _query_tokens(task)
    declared_owner_ids = {
        document_id
        for document_id in [
            task.get("primary_document_id"),
            *(task.get("supporting_document_ids", []) or []),
        ]
        if isinstance(document_id, str)
    }
    owner_excerpts = [
        excerpt for excerpt in excerpts if excerpt["id"] in declared_owner_ids
    ]
    quick_source_excerpts = owner_excerpts or excerpts
    quick_blocks = [
        (excerpt["id"], block)
        for excerpt in quick_source_excerpts
        if (block := _quick_decision_block(excerpt["content"], query_tokens)) is not None
    ]
    if quick_blocks:
        sections.extend(
            [
                "",
                "RETRIEVED QUICK EVIDENCE BLOCKS:",
                "These verbatim decision and query-relevant sections are repeated only from the "
                "task's declared primary and supporting owner documents so their minimum "
                "checklist and direct evidence remain visible after the longer excerpts.",
            ]
        )
        for document_id, block in quick_blocks:
            sections.extend(
                [
                    "",
                    f'<decision document_id="{document_id}">',
                    block.rstrip(),
                    "</decision>",
                ]
            )
    sections.extend(
        [
            "",
            "Answer the question for a game developer or Engine maintainer. Cover each distinct "
            "operation, boundary, or decision named in the question. Use exact identifiers from "
            "the excerpts where they make the answer actionable. Use the owning guide's opening "
            "decision or route summary as a minimum checklist when it addresses the question; do "
            "not compress several checklist items into a broader claim or spend the answer budget "
            "on secondary background until every relevant primary-owner item is stated. Before "
            "returning, scan the answer once for every checklist ID and every required evidence "
            "token; add any supported item that is still absent. Then remove every concrete "
            "identifier that is not copied exactly from the "
            "supplied excerpts. Return JSON only.",
        ]
    )
    return "\n".join(sections)


def _generate_parsed_response(
    client: OllamaClient,
    *,
    model: str,
    prompt: str,
    stage: str,
    num_context: int,
    num_predict: int,
) -> tuple[
    dict[str, object],
    dict[str, object],
    dict[str, object],
    list[dict[str, object]],
    float,
]:
    correction = (
        "\n\nRESPONSE CORRECTION: The previous generation did not satisfy the required "
        "JSON response schema or ended before the object was complete. Regenerate the complete "
        "answer from scratch. Return all five required fields; selected_document_ids must be an "
        "array of strings, citations must be an array of objects, and both assumption and "
        "uncertainty fields must be string arrays. Do not abbreviate or stop after an "
        "introductory clause."
    )
    attempts: list[dict[str, object]] = []
    elapsed = 0.0
    for attempt_index in range(2):
        attempt_prompt = prompt if attempt_index == 0 else prompt + correction
        started = time.monotonic()
        response = client.chat(
            model,
            _system_prompt(),
            attempt_prompt,
            num_context=num_context,
            num_predict=num_predict,
            disable_thinking=attempt_index > 0,
        )
        attempt_elapsed = time.monotonic() - started
        elapsed += attempt_elapsed
        message = response.get("message")
        if not isinstance(message, dict) or not isinstance(message.get("content"), str):
            raise ValueError("Ollama chat response is missing message.content")
        raw_response = str(message["content"])
        attempt_record: dict[str, object] = {
            "stage": stage,
            "attempt": attempt_index + 1,
            "prompt_sha256": hashlib.sha256(attempt_prompt.encode("utf-8")).hexdigest(),
            "elapsed_seconds": round(attempt_elapsed, 3),
            "raw_response": raw_response,
            "thinking_disabled": attempt_index > 0,
        }
        try:
            parsed = _parse_model_json(raw_response)
            attempt_record["status"] = "parsed"
            attempts.append(attempt_record)
            return parsed, response, message, attempts, elapsed
        except ValueError as parse_exception:
            attempt_record.update({"status": "invalid-response", "error": str(parse_exception)})
            attempts.append(attempt_record)
            if attempt_index == 1:
                raise
    raise ValueError("Ollama chat response could not be parsed")


def _answer_check_observations(
    task: dict[str, object],
    text: str,
    *,
    include_review_status: bool,
) -> list[dict[str, object]]:
    folded = " ".join(text.casefold().split())
    observations: list[dict[str, object]] = []
    for check in task.get("answer_checks", []):
        if not isinstance(check, dict):
            continue
        terms = check.get("required_terms", [])
        term_observations = {
            str(term): " ".join(str(term).casefold().split()) in folded
            for term in terms
            if isinstance(term, str)
        }
        observation: dict[str, object] = {
            "id": check.get("id"),
            "description": check.get("description"),
            "document_id": check.get("document_id"),
            "anchor": check.get("anchor"),
            "term_observations": term_observations,
            "all_terms_observed": bool(term_observations) and all(term_observations.values()),
        }
        if include_review_status:
            observation["review_status"] = "not-reviewed"
        observations.append(observation)
    return observations


def _task_observations(
    root: Path,
    task: dict[str, object],
    candidate_ids: list[str],
    document_map: dict[str, dict[str, object]],
    parsed: dict[str, object],
) -> dict[str, object]:
    selected_ids = list(parsed["selected_document_ids"])
    answer = str(parsed["answer"])
    answer_checks = _answer_check_observations(task, answer, include_review_status=True)

    citations: list[dict[str, object]] = []
    for citation in parsed["citations"]:
        document_id = citation.get("document_id")
        raw_anchor = citation.get("anchor")
        anchor = raw_anchor.lstrip("#") if isinstance(raw_anchor, str) else raw_anchor
        valid_document = isinstance(document_id, str) and document_id in candidate_ids
        valid_anchor = False
        if valid_document and isinstance(anchor, str):
            path_value = document_map[document_id].get("path")
            if isinstance(path_value, str):
                valid_anchor = anchor in docs_validate._markdown_anchors(root / path_value)
        citations.append(
            {
                "document_id": document_id,
                "anchor": anchor,
                "raw_anchor": raw_anchor,
                "valid_document": valid_document,
                "valid_anchor": valid_anchor,
            }
        )

    invalid_selected = [document_id for document_id in selected_ids if document_id not in candidate_ids]
    primary_document_id = str(task.get("primary_document_id", ""))
    supporting_ids = [
        str(value) for value in task.get("supporting_document_ids", []) if isinstance(value, str)
    ]
    return {
        "primary_document_selected": primary_document_id in selected_ids,
        "supporting_documents_selected": [
            document_id for document_id in supporting_ids if document_id in selected_ids
        ],
        "invalid_selected_document_ids": invalid_selected,
        "valid_citation_count": sum(
            1 for citation in citations if citation["valid_document"] and citation["valid_anchor"]
        ),
        "citations": citations,
        "answer_checks": answer_checks,
        "automatic_project_assumption_detected": bool(PROJECT_ASSUMPTION_RE.search(answer)),
        "model_reported_assumptions": parsed["unsupported_or_project_specific_assumptions"],
        "review_required": True,
        "final_task_success": None,
    }


def _semantic_repair_reasons(observations: dict[str, object]) -> list[str]:
    reasons: list[str] = []
    for check in observations.get("answer_checks", []):
        if not isinstance(check, dict):
            continue
        missing_terms = [
            str(term)
            for term, observed in check.get("term_observations", {}).items()
            if observed is not True
        ]
        if missing_terms:
            reasons.append(
                f"check {check.get('id')}: missing exact evidence tokens "
                + ", ".join(json.dumps(term, ensure_ascii=False) for term in missing_terms)
            )
    invalid_selected = observations.get("invalid_selected_document_ids", [])
    if isinstance(invalid_selected, list) and invalid_selected:
        reasons.append(
            "invalid selected document IDs: "
            + ", ".join(json.dumps(value, ensure_ascii=False) for value in invalid_selected)
        )
    if observations.get("primary_document_selected") is not True:
        reasons.append("the declared primary owner is absent from selected_document_ids")
    if observations.get("valid_citation_count") == 0:
        reasons.append("no citation names both a supplied document ID and a visible anchor")
    return reasons


def _write_report(path: Path, report: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    temporary.replace(path)


def _aggregate(report: dict[str, object]) -> None:
    tasks = [task for task in report.get("tasks", []) if isinstance(task, dict)]
    completed = [task for task in tasks if task.get("status") == "completed"]
    errors = [task for task in tasks if task.get("status") == "error"]
    owner_passes = sum(
        1
        for task in completed
        if isinstance(task.get("observations"), dict)
        and task["observations"].get("primary_document_selected") is True
    )
    checks = [
        check
        for task in completed
        if isinstance(task.get("observations"), dict)
        for check in task["observations"].get("answer_checks", [])
        if isinstance(check, dict)
    ]
    observed_checks = sum(1 for check in checks if check.get("all_terms_observed") is True)
    input_checks = [
        check
        for task in completed
        if isinstance(task.get("input_evidence"), dict)
        for check in task["input_evidence"].get("answer_checks", [])
        if isinstance(check, dict)
    ]
    input_observed_checks = sum(
        1 for check in input_checks if check.get("all_terms_observed") is True
    )
    project_assumptions = sum(
        1
        for task in completed
        if isinstance(task.get("observations"), dict)
        and task["observations"].get("automatic_project_assumption_detected") is True
    )
    report["summary"] = {
        "task_count": len(tasks),
        "completed_task_count": len(completed),
        "error_task_count": len(errors),
        "owning_document_selection_rate": owner_passes / len(completed) if completed else 0.0,
        "answer_check_term_observation_rate": observed_checks / len(checks) if checks else 0.0,
        "input_answer_check_evidence_rate": (
            input_observed_checks / len(input_checks) if input_checks else 0.0
        ),
        "automatic_project_assumption_count": project_assumptions,
        "reviewed_task_count": sum(
            1
            for task in completed
            if isinstance(task.get("observations"), dict)
            and task["observations"].get("final_task_success") is not None
        ),
        "final_task_success_rate": None,
    }


def _resume_existing_report(
    output: Path,
    fresh_report: dict[str, object],
) -> dict[str, object]:
    existing = _load_json(output, "partial model-family evaluation")
    comparable_keys = (
        "schema_version",
        "generated_by",
        "source_ref",
        "input_hashes",
        "provider",
        "model_family",
        "model",
        "model_info",
        "system_prompt",
        "parameters",
        "isolation",
        "requested_task_ids",
    )
    mismatches = [key for key in comparable_keys if existing.get(key) != fresh_report.get(key)]
    if mismatches:
        raise ValueError(
            "partial report is incompatible with this run: " + ", ".join(mismatches)
        )
    tasks = existing.get("tasks")
    if not isinstance(tasks, list) or any(not isinstance(task, dict) for task in tasks):
        raise ValueError("partial model-family evaluation tasks must be an object array")
    requested = set(str(value) for value in fresh_report["requested_task_ids"])
    task_ids = [str(task.get("id")) for task in tasks]
    if len(task_ids) != len(set(task_ids)):
        raise ValueError("partial model-family evaluation contains duplicate task ids")
    extra = sorted(set(task_ids) - requested)
    if extra:
        raise ValueError("partial report contains unrequested tasks: " + ", ".join(extra))
    existing["completed_at"] = None
    harness = fresh_report.get("harness")
    harness_history = existing.setdefault("resume_harnesses", [])
    if (
        isinstance(harness_history, list)
        and isinstance(harness, dict)
        and harness != existing.get("harness")
        and harness not in harness_history
    ):
        harness_history.append(harness)
    existing["tasks"] = [task for task in tasks if task.get("status") == "completed"]
    _aggregate(existing)
    return existing


def run_evaluation(
    root: Path,
    *,
    model: str,
    family: str,
    output: Path,
    client: OllamaClient,
    task_ids: set[str] | None = None,
    task_limit: int | None = None,
    max_candidates: int = DEFAULT_MAX_CANDIDATES,
    max_document_bytes: int = DEFAULT_MAX_DOCUMENT_BYTES,
    max_context_bytes: int = DEFAULT_MAX_CONTEXT_BYTES,
    num_context: int = DEFAULT_NUM_CONTEXT,
    num_predict: int = DEFAULT_NUM_PREDICT,
    resume: bool = False,
    self_review: bool = False,
) -> dict[str, object]:
    root = root.resolve()
    source_path = root / DEFAULT_SOURCE
    static_report_path = root / DEFAULT_STATIC_REPORT
    manifest_path = root / DEFAULT_PUBLIC_MANIFEST
    llms_path = root / DEFAULT_LLMS
    source = _load_json(source_path, "AI evaluation source")
    static_report = _load_json(static_report_path, "static AI evaluation report")
    public_manifest = _load_json(manifest_path, "public documentation manifest")
    source_tasks = source.get("tasks")
    static_tasks = static_report.get("tasks")
    documents = public_manifest.get("documents")
    if not isinstance(source_tasks, list) or not isinstance(static_tasks, list):
        raise ValueError("AI evaluation source and report must contain task arrays")
    if not isinstance(documents, list):
        raise ValueError("public documentation manifest must contain documents")
    source_by_id = {
        str(task["id"]): task
        for task in source_tasks
        if isinstance(task, dict) and isinstance(task.get("id"), str)
    }
    static_by_id = {
        str(task["id"]): task
        for task in static_tasks
        if isinstance(task, dict) and isinstance(task.get("id"), str)
    }
    document_map = {
        str(document["id"]): document
        for document in documents
        if isinstance(document, dict) and isinstance(document.get("id"), str)
    }
    selected_tasks = list(source_by_id.values())
    if task_ids is not None:
        unknown = sorted(task_ids - set(source_by_id))
        if unknown:
            raise ValueError(f"unknown task ids: {', '.join(unknown)}")
        selected_tasks = [task for task in selected_tasks if str(task["id"]) in task_ids]
    if task_limit is not None:
        selected_tasks = selected_tasks[:task_limit]
    if not selected_tasks:
        raise ValueError("model evaluation selected no tasks")

    fresh_report: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": GENERATED_BY,
        "started_at": _utc_now(),
        "completed_at": None,
        "source_ref": source.get("source_ref"),
        "input_hashes": {
            DEFAULT_SOURCE: _normalized_sha256(source_path),
            DEFAULT_STATIC_REPORT: _normalized_sha256(static_report_path),
            DEFAULT_PUBLIC_MANIFEST: _normalized_sha256(manifest_path),
            DEFAULT_LLMS: _normalized_sha256(llms_path),
        },
        "harness": {
            "path": GENERATED_BY,
            "sha256": _normalized_sha256(Path(__file__).resolve()),
        },
        "provider": {
            "id": "ollama",
            "base_url": client._base_url,
            "version": client.version(),
        },
        "model_family": family,
        "model": model,
        "model_info": _compact_model_info(client.show(model), client.tags(), model),
        "system_prompt": _system_prompt(),
        "parameters": {
            "temperature": 0,
            "seed": 0,
            "num_context": num_context,
            "num_predict": num_predict,
            "max_candidates": max_candidates,
            "max_document_bytes": max_document_bytes,
            "max_context_bytes": max_context_bytes,
            "self_review": self_review,
            "invalid_response_retry_count": 1,
            "disable_thinking_on_invalid_response_retry": True,
            "semantic_completion_retry_count": 1,
            "semantic_completion_triggers": [
                "missing-required-evidence-token",
                "invalid-selected-document-id",
                "missing-primary-owner",
                "no-valid-citation",
            ],
        },
        "isolation": {
            "conversation_per_task": True,
            "allowed_inputs": [
                DEFAULT_LLMS,
                DEFAULT_PUBLIC_MANIFEST,
                DEFAULT_STATIC_REPORT,
                "retrieved public current Markdown excerpts",
            ],
            "embedding_project_inputs": False,
            "answer_rubric_visible_to_model": False,
        },
        "requested_task_ids": [str(task["id"]) for task in selected_tasks],
        "tasks": [],
        "summary": {},
    }
    report = _resume_existing_report(output, fresh_report) if resume else fresh_report
    _write_report(output, report)

    completed_ids = {
        str(task.get("id"))
        for task in report["tasks"]
        if isinstance(task, dict) and task.get("status") == "completed"
    }
    for task in selected_tasks:
        task_id = str(task["id"])
        if task_id in completed_ids:
            sys.stdout.write(f"[{model}] resume: keeping completed task {task_id}\n")
            sys.stdout.flush()
            continue
        task_record: dict[str, object] = {
            "id": task_id,
            "category": task.get("category"),
            "question": task.get("question"),
            "primary_document_id": task.get("primary_document_id"),
            "supporting_document_ids": task.get("supporting_document_ids", []),
            "status": "running",
        }
        report["tasks"].append(task_record)
        _aggregate(report)
        _write_report(output, report)
        try:
            static_task = static_by_id.get(task_id)
            if static_task is None:
                raise ValueError(f"static report is missing task {task_id}")
            candidate_ids = _candidate_document_ids(static_task, max_candidates)
            query_tokens = _query_tokens(task)
            excerpts: list[dict[str, str]] = []
            remaining_context = max_context_bytes
            for document_id in candidate_ids:
                document = document_map.get(document_id)
                if document is None:
                    raise ValueError(f"candidate {document_id} is absent from public manifest")
                path_value = document.get("path")
                if not isinstance(path_value, str):
                    raise ValueError(f"candidate {document_id} has no source path")
                source_file = root / path_value
                excerpt_budget = min(max_document_bytes, remaining_context)
                if excerpt_budget < 512:
                    break
                content = _document_excerpt(
                    source_file.read_text(encoding="utf-8"),
                    query_tokens,
                    excerpt_budget,
                )
                excerpts.append({"id": document_id, "path": path_value, "content": content})
                remaining_context -= len(content.encode("utf-8"))
            prompt = _build_user_prompt(task, str(source.get("source_ref")), excerpts)
            input_checks = _answer_check_observations(
                task,
                prompt,
                include_review_status=False,
            )
            parsed, response, message, response_attempts, elapsed = _generate_parsed_response(
                client,
                model=model,
                prompt=prompt,
                stage="answer" if not self_review else "draft",
                num_context=num_context,
                num_predict=num_predict,
            )
            draft = None
            if self_review:
                draft = {
                    "answer": parsed["answer"],
                    "selected_document_ids": parsed["selected_document_ids"],
                    "citations": parsed["citations"],
                    "unsupported_or_project_specific_assumptions": parsed[
                        "unsupported_or_project_specific_assumptions"
                    ],
                    "uncertainties": parsed["uncertainties"],
                }
                review_prompt = (
                    prompt
                    + "\n\nDRAFT TO AUDIT (this draft is not documentation evidence):\n"
                    + json.dumps(draft, ensure_ascii=False, indent=2)
                    + "\n\nSELF-REVIEW: Re-read the supplied documentation excerpts. Decompose the "
                    "original question into every distinct operation, decision, ownership, "
                    "compatibility, safety, and validation clause it asks about. Rewrite the "
                    "answer so each Semantic Coverage Checklist ID has an explicit answer clause "
                    "and every supported required evidence token appears in the answer itself. "
                    "A citation or synonym does not satisfy an absent token. Remove every "
                    "command, target, path, setting, number, or support claim that is not present "
                    "verbatim in the excerpts. Keep project-owned policy separate from reusable "
                    "Engine behavior. Return the complete corrected JSON object only."
                )
                (
                    parsed,
                    response,
                    message,
                    review_attempts,
                    review_elapsed,
                ) = _generate_parsed_response(
                    client,
                    model=model,
                    prompt=review_prompt,
                    stage="self-review",
                    num_context=num_context,
                    num_predict=num_predict,
                )
                response_attempts.extend(review_attempts)
                elapsed += review_elapsed
            observations = _task_observations(
                root,
                task,
                candidate_ids,
                document_map,
                parsed,
            )
            semantic_repair_history: list[dict[str, object]] = []
            for semantic_repair_index in range(1):
                semantic_repair_reasons = _semantic_repair_reasons(observations)
                if not semantic_repair_reasons:
                    break
                semantic_repair_history.append(
                    {
                        "attempt": semantic_repair_index + 1,
                        "reasons": semantic_repair_reasons,
                    }
                )
                repair_prompt = (
                    prompt
                    + "\n\nANSWER TO REPAIR (this draft is not documentation evidence):\n"
                    + json.dumps(parsed, ensure_ascii=False, indent=2)
                    + "\n\nSEMANTIC COMPLETION REPAIR REQUIRED:\n- "
                    + "\n- ".join(semantic_repair_reasons)
                    + "\nRewrite the complete JSON object. Preserve correct clauses, but explicitly "
                    "add every listed evidence token from the supplied excerpts, select only "
                    "supplied document IDs including the declared primary owner, and cite only "
                    "visible anchors. Do not merely list a token: state the source-backed rule or "
                    "value it belongs to. Remove unsupported secondary details and return JSON only."
                )
                (
                    parsed,
                    response,
                    message,
                    semantic_repair_attempts,
                    semantic_repair_elapsed,
                ) = _generate_parsed_response(
                    client,
                    model=model,
                    prompt=repair_prompt,
                    stage="semantic-completion",
                    num_context=num_context,
                    num_predict=num_predict,
                )
                response_attempts.extend(semantic_repair_attempts)
                elapsed += semantic_repair_elapsed
                observations = _task_observations(
                    root,
                    task,
                    candidate_ids,
                    document_map,
                    parsed,
                )
            semantic_repair_unresolved_reasons = _semantic_repair_reasons(observations)
            task_record.update(
                {
                    "status": "completed",
                    "candidate_document_ids": candidate_ids,
                    "candidate_owner_available": task.get("primary_document_id") in candidate_ids,
                    "excerpt_document_ids": [excerpt["id"] for excerpt in excerpts],
                    "prompt_bytes": len(prompt.encode("utf-8")),
                    "prompt_sha256": hashlib.sha256(prompt.encode("utf-8")).hexdigest(),
                    "user_prompt": prompt,
                    "response_attempts": response_attempts,
                    "raw_response": response_attempts[-1]["raw_response"],
                    "draft": draft,
                    "semantic_repair_history": semantic_repair_history,
                    "semantic_repair_unresolved_reasons": semantic_repair_unresolved_reasons,
                    "input_evidence": {
                        "answer_checks": input_checks,
                        "all_answer_checks_observable": all(
                            check["all_terms_observed"] for check in input_checks
                        ),
                    },
                    "retrieval_checks": static_task.get("retrieval_checks", []),
                    "answer": parsed["answer"],
                    "selected_document_ids": parsed["selected_document_ids"],
                    "uncertainties": parsed["uncertainties"],
                    "response_normalizations": parsed["response_normalizations"],
                    "observations": observations,
                    "runtime": {
                        "elapsed_seconds": round(elapsed, 3),
                        "total_duration_ns": response.get("total_duration"),
                        "load_duration_ns": response.get("load_duration"),
                        "prompt_eval_count": response.get("prompt_eval_count"),
                        "prompt_eval_duration_ns": response.get("prompt_eval_duration"),
                        "eval_count": response.get("eval_count"),
                        "eval_duration_ns": response.get("eval_duration"),
                        "done_reason": response.get("done_reason"),
                        "thinking_redacted": bool(message.get("thinking")),
                    },
                }
            )
        except Exception as exception:  # Preserve partial evidence before returning failure
            cleanup_error = None
            try:
                client.unload(model)
            except Exception as cleanup_exception:
                cleanup_error = str(cleanup_exception)
            task_record.update(
                {
                    "status": "error",
                    "error": str(exception),
                    "model_unload_error": cleanup_error,
                }
            )
        _aggregate(report)
        _write_report(output, report)
        summary = report["summary"]
        sys.stdout.write(
            f"[{model}] {len(report['tasks'])}/{len(selected_tasks)} {task_id}: "
            f"{task_record['status']} (errors={summary['error_task_count']})\n"
        )
        sys.stdout.flush()

    report["completed_at"] = _utc_now()
    _aggregate(report)
    _write_report(output, report)
    return report


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run isolated FOnline documentation tasks against one Ollama model family."
    )
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--model", required=True)
    parser.add_argument("--family", required=True)
    parser.add_argument("--ollama-url", default=DEFAULT_OLLAMA_URL)
    parser.add_argument("--output")
    parser.add_argument("--task", action="append", default=[])
    parser.add_argument("--task-limit", type=int)
    parser.add_argument("--max-candidates", type=int, default=DEFAULT_MAX_CANDIDATES)
    parser.add_argument("--max-document-bytes", type=int, default=DEFAULT_MAX_DOCUMENT_BYTES)
    parser.add_argument("--max-context-bytes", type=int, default=DEFAULT_MAX_CONTEXT_BYTES)
    parser.add_argument("--num-context", type=int, default=DEFAULT_NUM_CONTEXT)
    parser.add_argument("--num-predict", type=int, default=DEFAULT_NUM_PREDICT)
    parser.add_argument("--timeout", type=float, default=900.0)
    parser.add_argument(
        "--resume",
        action="store_true",
        help="keep compatible completed tasks and retry unfinished tasks in --output",
    )
    parser.add_argument(
        "--self-review",
        action="store_true",
        help="run a second isolated grounding/completeness pass over each draft",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    for label, value in (
        ("task-limit", arguments.task_limit),
        ("max-candidates", arguments.max_candidates),
        ("max-document-bytes", arguments.max_document_bytes),
        ("max-context-bytes", arguments.max_context_bytes),
        ("num-context", arguments.num_context),
        ("num-predict", arguments.num_predict),
    ):
        if value is not None and value < 1:
            raise SystemExit(f"--{label} must be positive")
    root = Path(arguments.root).resolve()
    safe_model = re.sub(r"[^a-zA-Z0-9._-]+", "-", arguments.model).strip("-")
    output = Path(arguments.output) if arguments.output else (
        root / DEFAULT_OUTPUT_DIR / f"{safe_model}.json"
    )
    if not output.is_absolute():
        output = root / output
    client = OllamaClient(arguments.ollama_url, arguments.timeout)
    try:
        report = run_evaluation(
            root,
            model=arguments.model,
            family=arguments.family,
            output=output,
            client=client,
            task_ids=set(arguments.task) if arguments.task else None,
            task_limit=arguments.task_limit,
            max_candidates=arguments.max_candidates,
            max_document_bytes=arguments.max_document_bytes,
            max_context_bytes=arguments.max_context_bytes,
            num_context=arguments.num_context,
            num_predict=arguments.num_predict,
            resume=arguments.resume,
            self_review=arguments.self_review,
        )
    except (OSError, RuntimeError, ValueError) as exception:
        sys.stderr.write(f"AI model-family evaluation failed: {exception}\n")
        return 1
    summary = report["summary"]
    sys.stdout.write(
        "AI model-family evaluation completed: "
        f"{summary['completed_task_count']}/{summary['task_count']} tasks, "
        f"{summary['error_task_count']} errors; review remains required.\n"
    )
    return 0 if summary["error_task_count"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
