"""Helpers for advisory tools that rank suggested next actions for an agent.

Advisory payloads are suggestions, never commands: each option names the tool and arguments an agent
may call next, with a 0..100 score and a human-readable reason.
"""

from __future__ import annotations

from typing import Any


def clamp_score(score: Any) -> int:
    return max(0, min(int_or_default(score, 0), 100))


def int_or_default(value: Any, default: int = 0) -> int:
    """Integer value of an observation field that a project may omit or leave null."""
    if isinstance(value, bool):
        return default

    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def result_limit(arguments: dict[str, Any], default: int = 20, maximum: int = 100) -> int:
    return max(1, min(int_or_default(arguments.get("maxResults", default), default), maximum))


def task_option(
    kind: str,
    score: int,
    reason: str,
    tool: str,
    tool_arguments: dict[str, Any],
    extra: dict[str, Any] | None = None,
) -> dict[str, Any]:
    result: dict[str, Any] = {"kind": kind, "score": clamp_score(score), "reason": reason, "tool": tool, "arguments": tool_arguments}

    if extra:
        result.update(extra)

    return result


def ranked_options(options: list[dict[str, Any]], arguments: dict[str, Any], default_limit: int = 20) -> list[dict[str, Any]]:
    """Highest score first; ties keep their original order. Truncated to the caller's maxResults."""
    ordered = sorted(enumerate(options), key=lambda item: (-int(item[1].get("score", 0)), item[0]))
    return [option for _, option in ordered][: result_limit(arguments, default_limit)]
