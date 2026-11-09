"""Tests for the shared count-ratchet baseline format."""

from __future__ import annotations

from pathlib import Path
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import finding_baseline as baseline  # noqa: E402


HEADER = "rule\tpath\tcount\treason\n"


def finding(rule: str = "rule-a", path: str = "Source/A.h", line: int = 1) -> baseline.Finding:
    return baseline.Finding(rule, path, line, "message")


def write(tmp_path: Path, name: str, body: str) -> Path:
    path = tmp_path.resolve() / name
    path.write_text(body, encoding="utf-8")
    return path


def test_rows_accept_up_to_their_count(tmp_path: Path) -> None:
    rows = baseline.load_baselines([write(tmp_path, "b.tsv", HEADER + "rule-a\tSource/A.h\t2\tKept for a reason\n")])

    remaining, stale = baseline.apply_baseline([finding(line=1), finding(line=2)], rows)

    assert remaining == []
    assert stale == []


def test_findings_above_the_count_are_all_reported(tmp_path: Path) -> None:
    rows = baseline.load_baselines([write(tmp_path, "b.tsv", HEADER + "rule-a\tSource/A.h\t1\tKept for a reason\n")])

    remaining, stale = baseline.apply_baseline([finding(line=1), finding(line=2)], rows)

    assert [item.line for item in remaining] == [1, 2]
    assert stale == []


def test_unlisted_findings_fail_and_other_rules_do_not_share_counts(tmp_path: Path) -> None:
    rows = baseline.load_baselines([write(tmp_path, "b.tsv", HEADER + "rule-a\tSource/A.h\t5\tKept for a reason\n")])

    remaining, _ = baseline.apply_baseline([finding(rule="rule-b"), finding(path="Source/B.h")], rows)

    assert len(remaining) == 2


def test_counts_may_only_go_down(tmp_path: Path) -> None:
    rows = baseline.load_baselines([write(tmp_path, "b.tsv",
                                          HEADER + "rule-a\tSource/A.h\t3\tKept\nrule-b\tSource/B.h\t1\tKept\n")])

    remaining, stale = baseline.apply_baseline([finding()], rows)

    assert remaining == []
    assert len(stale) == 2
    assert "lower the count to 1" in stale[0]
    assert "matches no finding" in stale[1]


def test_several_files_combine_but_a_key_belongs_to_one_file(tmp_path: Path) -> None:
    engine = write(tmp_path, "engine.tsv", HEADER + "rule-a\tSource/A.h\t1\tEngine reason\n")
    project = write(tmp_path, "project.tsv", HEADER + "rule-a\tSourceExt/B.h\t1\tProject reason\n")
    duplicate = write(tmp_path, "duplicate.tsv", HEADER + "rule-a\tSource/A.h\t1\tAgain\n")

    rows = baseline.load_baselines([engine, project])

    assert set(rows) == {("rule-a", "Source/A.h"), ("rule-a", "SourceExt/B.h")}

    with pytest.raises(baseline.BaselineError, match="duplicate baseline key"):
        baseline.load_baselines([engine, duplicate])


@pytest.mark.parametrize(("body", "message"), [
    ("rule\tpath\tcount\n", "header"),
    (HEADER + "rule-a\tSource/A.h\t1\t\n", "written reason"),
    (HEADER + "rule-a\tSource/A.h\t0\tReason\n", "positive integer"),
    (HEADER + "rule-a\tSource/*.h\t1\tReason\n", "exact root-relative"),
    (HEADER + "rule-a\t../A.h\t1\tReason\n", "exact root-relative"),
    (HEADER + "rule-a\tSource/A.h\t1\n", "4 tab-separated columns"),
    ("# only a comment\n", "header is missing"),
])
def test_malformed_baselines_are_rejected(tmp_path: Path, body: str, message: str) -> None:
    with pytest.raises(baseline.BaselineError, match=message):
        baseline.load_baselines([write(tmp_path, "b.tsv", body)])


def test_written_draft_needs_reasons_before_it_loads(tmp_path: Path) -> None:
    target = tmp_path.resolve() / "draft" / "b.tsv"

    rows = baseline.write_baseline(target, [finding(line=2), finding(line=1), finding(rule="rule-b")])

    assert rows == 2
    assert target.read_text(encoding="utf-8") == (
        HEADER + "rule-a\tSource/A.h\t2\t\nrule-b\tSource/A.h\t1\t\n")

    with pytest.raises(baseline.BaselineError, match="written reason"):
        baseline.load_baselines([target])


def test_comments_blank_lines_and_bom_are_ignored(tmp_path: Path) -> None:
    path = tmp_path.resolve() / "b.tsv"
    path.write_bytes(("\ufeff# note\n\n" + HEADER + "rule-a\tSource/A.h\t1\tReason\r\n").encode("utf-8"))

    rows = baseline.load_baselines([path])

    assert rows[("rule-a", "Source/A.h")].reason == "Reason"
    assert rows[("rule-a", "Source/A.h")].source.endswith("b.tsv:4")
