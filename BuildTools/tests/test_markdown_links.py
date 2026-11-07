from __future__ import annotations

from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(BUILDTOOLS_DIR))
import markdown_links  # noqa: E402


def _write(root: Path, files: dict[str, str | bytes]) -> None:
    for relative_path, content in files.items():
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content, encoding="utf-8", newline="\n")


@pytest.fixture
def root(tmp_path: Path) -> Path:
    repository = (tmp_path / "repo").resolve()
    repository.mkdir()
    return repository


def _links(root: Path, files: dict[str, str | bytes], document: str = "Docs/A.md", **options: object) -> list[str]:
    _write(root, files)
    return markdown_links.check_links(root, [document], **options)


def test_accepts_resolving_links_and_rejects_a_missing_target(root: Path) -> None:
    assert _links(root, {"Docs/A.md": "[b](B.md)\n", "Docs/B.md": "# B\n"}) == []
    assert _links(root, {"Docs/A.md": "# A\n\n[gone](Gone.md)\n"}) == [
        "Docs/A.md:3: local link target does not exist: Gone.md",
    ]


def test_relative_links_resolve_from_the_document_directory(root: Path) -> None:
    files = {"Scripts/Foo.txt": "\n", "Docs/A.md": "[wrong](Scripts/Foo.txt)\n[right](../Scripts/Foo.txt)\n"}

    assert _links(root, files) == ["Docs/A.md:1: local link target does not exist: Scripts/Foo.txt"]


def test_slash_prefixed_links_resolve_from_the_root(root: Path) -> None:
    files = {"Scripts/Foo.txt": "\n", "Docs/A.md": "[root](/Scripts/Foo.txt)\n[gone](/Scripts/Bar.txt)\n"}

    assert _links(root, files) == ["Docs/A.md:2: local link target does not exist: /Scripts/Bar.txt"]


def test_rejects_a_link_that_escapes_the_root_even_when_the_target_exists(root: Path) -> None:
    _write(root.parent, {"Outside.md": "# Outside\n"})

    assert _links(root, {"Docs/A.md": "[out](../../Outside.md)\n"}) == [
        "Docs/A.md:1: local link escapes the root: ../../Outside.md",
    ]
    assert _links(root, {"Docs/A.md": "[out](../../Outside.md)\n"}, root_label="engine root") == [
        "Docs/A.md:1: local link escapes the engine root: ../../Outside.md",
    ]


def test_a_sibling_target_root_is_a_link_target_only(root: Path) -> None:
    sibling = root.parent / "Engine"
    _write(sibling, {"Docs/Guide.md": "# Guide\n\n## Setup\n", "Docs/Broken.md": "[gone](Gone.md)\n"})
    files = {"Docs/A.md": "[ok](../../Engine/Docs/Guide.md#setup)\n[bad](../../Engine/Docs/Guide.md#missing)\n"}

    assert _links(root, files, target_roots=[sibling]) == [
        "Docs/A.md:2: Markdown anchor does not exist: ../../Engine/Docs/Guide.md#missing",
    ]
    assert [error.split(": ")[1] for error in _links(root, files)] == ["local link escapes the root"] * 2


def test_an_embedded_tree_is_excluded_from_documents_but_checked_as_a_target(root: Path) -> None:
    _write(root, {
        "Engine/Docs/Guide.md": "# Guide\n\n## Setup\n\n[gone](Gone.md)\n",
        "Docs/A.md": "# A\n\n[ok](../Engine/Docs/Guide.md#setup)\n[bad](../Engine/Docs/Guide.md#nope)\n",
    })

    errors, count = markdown_links.check_documents(root, excludes=["Engine"])

    assert count == 1
    assert errors == ["Docs/A.md:4: Markdown anchor does not exist: ../Engine/Docs/Guide.md#nope"]


def test_skip_target_ignores_a_tree_that_is_not_checked_out(root: Path) -> None:
    files = {"Docs/A.md": "[art](../Art/Interface/Game.psd)\n[other](../Other/Missing.txt)\n"}

    assert _links(root, files, skip_targets=["Art"]) == [
        "Docs/A.md:2: local link target does not exist: ../Other/Missing.txt",
    ]


def test_heading_anchors_in_another_document_and_the_same_document(root: Path) -> None:
    files = {
        "Docs/A.md": "# A\n\n[here](#a)\n[gone](#b)\n[there](B.md#the-section)\n[missing](B.md#gone)\n",
        "Docs/B.md": "# B\n\n## The section\n",
    }

    assert _links(root, files) == [
        "Docs/A.md:4: Markdown anchor does not exist: #b",
        "Docs/A.md:6: Markdown anchor does not exist: B.md#gone",
    ]


@pytest.mark.parametrize(
    ("heading", "slug"),
    [
        ("The `verify` macro", "the-verify-macro"),
        ("NPC state (`state_dump`)", "npc-state-state_dump"),
        ("MCP / AiControl Validation", "mcp--aicontrol-validation"),
        ("Create / destroy", "create--destroy"),
        ("GuardSystem (охрана)", "guardsystem-охрана"),
        ("Café area", "café-area"),
        ("See [the guide](Guide.md) **now**", "see-the-guide-now"),
        ("<span>Tagged</span> title", "tagged-title"),
    ],
)
def test_heading_slug_matches_github(heading: str, slug: str) -> None:
    assert markdown_links.heading_slug(heading) == slug


def test_repeated_headings_receive_numbered_suffixes(root: Path) -> None:
    files = {
        "Docs/A.md": "[first](B.md#status)\n[second](B.md#status-1)\n[third](B.md#status-2)\n",
        "Docs/B.md": "## Status\n\n## Status\n",
    }

    assert _links(root, files) == ["Docs/A.md:3: Markdown anchor does not exist: B.md#status-2"]


def test_explicit_html_anchors_are_targets(root: Path) -> None:
    files = {
        "Docs/A.md": "[a](B.md#pinned)\n[b](B.md#entry-1)\n[c](B.md#Pinned)\n",
        "Docs/B.md": '<a name="pinned"></a>\n\n| <a id="entry-1"></a>`entry` | value |\n',
    }

    assert _links(root, files) == ["Docs/A.md:3: Markdown anchor does not exist: B.md#Pinned"]


def test_percent_encoded_paths_and_fragments_are_decoded(root: Path) -> None:
    files = {
        "Docs/A.md": "[guide](Guide%20Page.md#caf%C3%A9-area)\n[angle](<Guide Page.md#café-area>)\n",
        "Docs/Guide Page.md": "# Guide\n\n## Café area\n",
    }

    assert _links(root, files) == []


def test_link_titles_query_strings_and_directories_are_accepted(root: Path) -> None:
    files = {
        "Docs/A.md": '[t](B.md "Title")\n[q](B.md?plain=1)\n[d](Sub/)\n',
        "Docs/B.md": "# B\n",
        "Docs/Sub/C.txt": "\n",
    }

    assert _links(root, files) == []


def test_html_href_and_src_attributes_are_checked(root: Path) -> None:
    files = {"Docs/A.md": '<a href="B.md">b</a> <img src="missing.png">\n', "Docs/B.md": "# B\n"}

    assert _links(root, files) == ["Docs/A.md:1: local link target does not exist: missing.png"]


def test_external_targets_are_not_checked(root: Path) -> None:
    files = {"Docs/A.md": "[w](https://example.com/x.md#gone)\n[m](mailto:a@example.com)\n[n](//cdn.example/x)\n"}

    assert _links(root, files) == []


def test_line_fragments_need_the_option_on_markdown_targets(root: Path) -> None:
    files = {"Docs/A.md": "[range](B.md#L10-L20)\n[code](c.py#L3)\n", "Docs/B.md": "# B\n", "Docs/c.py": "\n"}

    assert _links(root, files) == ["Docs/A.md:1: Markdown anchor does not exist: B.md#L10-L20"]
    assert _links(root, files, allow_line_fragments=True) == []


def test_reference_definitions_are_checked_and_footnotes_are_not_links(root: Path) -> None:
    files = {"Docs/A.md": "See [b][ref] and the note[^1].\n\n[ref]: Gone.md\n[^1]: A footnote, not a path.\n"}

    assert _links(root, files) == ["Docs/A.md:3: local link target does not exist: Gone.md"]


def test_undefined_reference_labels_are_reported_on_request(root: Path) -> None:
    files = {
        "Docs/A.md": (
            "[defined][Ref] [collapsed][] [missing][nope]\n"
            "<code>Name$[0-3][T].art</code> and `x[1][2]` are code\n\n"
            "[ref]: B.md\n[collapsed]: B.md\n"
        ),
        "Docs/B.md": "# B\n",
    }

    assert _links(root, files) == []
    assert _links(root, files, require_reference_definitions=True) == [
        "Docs/A.md:1: reference link label is not defined: [nope]",
    ]


def test_code_fences_and_inline_code_hide_links_and_headings(root: Path) -> None:
    files = {
        "Docs/A.md": (
            "# A\n\n```text\n[b](Gone.md)\n```\n\n~~~markdown\n## Hidden heading\n[c](Gone.md)\n~~~\n\n"
            "A lambda `const auto f = [](int){}` is not a link.\n\n"
            "[target](B.md#hidden-example)\n[same file](#hidden-heading)\n"
        ),
        "Docs/B.md": "# B\n\n```markdown\n## Hidden example\n```\n",
    }

    assert _links(root, files) == [
        "Docs/A.md:14: Markdown anchor does not exist: B.md#hidden-example",
        "Docs/A.md:15: Markdown anchor does not exist: #hidden-heading",
    ]


def test_a_shorter_or_annotated_fence_does_not_close_a_longer_one() -> None:
    text = "# A\n\n````markdown\n```text\n[gone](Missing.md)\n```\n````\n\n~~~\n~~~ not a close\n~~~\n"

    assert list(markdown_links.iter_links(text)) == []
    assert markdown_links.unclosed_fence_line(text) is None


def test_inline_triple_backticks_do_not_open_a_fence() -> None:
    text = "```inline``` code\n[b](B.md)\n"

    assert list(markdown_links.iter_links(text)) == [(2, "B.md")]
    assert markdown_links.unclosed_fence_line(text) is None


def test_front_matter_is_not_prose() -> None:
    text = "---\ntitle: Page\n# a YAML comment\nlink: '[x](Gone.md)'\n---\n# Page\n"

    assert list(markdown_links.iter_links(text)) == []
    assert markdown_links.heading_anchors(text) == {"page"}


def test_structure_reports_unclosed_fences_utf8_and_replacement_characters(root: Path) -> None:
    _write(root, {
        "Docs/Open.md": "# Open\n\n```text\n[gone](Missing.md)\n",
        "Docs/Bad.md": b"# Bad\n\ntext \xff\n",
        "Docs/Replaced.md": "# Replaced\n\ntext \ufffd\n",
        "Docs/Bom.md": "\ufeff# Bom\n",
    })

    errors = markdown_links.check_structure(
        root, ["Docs/Open.md", "Docs/Bad.md", "Docs/Replaced.md", "Docs/Bom.md"], require_single_h1=True)

    assert errors == [
        "Docs/Bad.md: document is not valid UTF-8 (invalid start byte at byte 12)",
        "Docs/Open.md:3: fenced code block is never closed",
        "Docs/Replaced.md:3: document contains the U+FFFD replacement character",
    ]


def test_single_h1_is_optional_and_supports_exemptions(root: Path) -> None:
    _write(root, {
        "Docs/Two.md": "# One\n\n# Two\n\n```text\n# not a heading\n```\n",
        "Docs/None.md": "## Only a section\n",
        "CLAUDE.md": "@AGENTS.md\n",
    })
    documents = ["CLAUDE.md", "Docs/None.md", "Docs/Two.md"]

    assert markdown_links.check_structure(root, documents) == []
    assert markdown_links.check_structure(root, documents, require_single_h1=True, h1_exempt=["CLAUDE.md"]) == [
        "Docs/None.md: expected exactly one H1 heading, found 0",
        "Docs/Two.md: expected exactly one H1 heading, found 2",
    ]


def test_an_unreadable_anchor_target_is_reported(root: Path) -> None:
    files = {"Docs/A.md": "[b](B.md#x)\n", "Docs/B.md": b"# B\xff\n"}

    assert _links(root, files) == ["Docs/A.md:1: Markdown anchor target is not readable UTF-8: B.md#x"]


def test_collect_documents_applies_globs_and_directory_excludes(root: Path) -> None:
    _write(root, {
        "README.md": "# R\n",
        "Docs/A.md": "# A\n",
        "Docs/Deep/B.markdown": "# B\n",
        "Docs/notes.txt": "\n",
        "Workspace/C.md": "# C\n",
        "Engine/Docs/D.md": "# D\n",
        "SourceExt/vendor/E.md": "# E\n",
        "SourceExt/Own.md": "# Own\n",
        "pytest-cache-files-x1/README.md": "# cache\n",
        ".git/F.md": "# git\n",
    })

    assert markdown_links.collect_documents(root) == [
        "Docs/A.md", "Docs/Deep/B.markdown", "Engine/Docs/D.md", "README.md", "SourceExt/Own.md",
        "SourceExt/vendor/E.md", "Workspace/C.md", "pytest-cache-files-x1/README.md",
    ]
    assert markdown_links.collect_documents(
        root, excludes=["Workspace", "Engine/", "SourceExt/vendor", "pytest-cache-files-*"],
    ) == ["Docs/A.md", "Docs/Deep/B.markdown", "README.md", "SourceExt/Own.md"]
    assert markdown_links.collect_documents(root, includes=["Docs/*.md"]) == ["Docs/A.md"]
    assert markdown_links.collect_documents(root, includes=["Docs/**/*.md", "README.md"]) == [
        "Docs/A.md", "README.md",
    ]


def test_check_documents_reports_an_unreadable_document_once(root: Path) -> None:
    _write(root, {"Docs/Bad.md": b"\xff\n"})

    errors, count = markdown_links.check_documents(root)

    assert count == 1
    assert errors == ["Docs/Bad.md: document is not valid UTF-8 (invalid start byte at byte 0)"]


def test_main_exit_codes_and_deterministic_output(root: Path, capsys: pytest.CaptureFixture[str]) -> None:
    _write(root, {"Docs/B.md": "# B\n\n[gone](#x)\n", "Docs/A.md": "# A\n\n[gone](Gone.md)\n"})

    assert markdown_links.main(["--root", str(root)]) == 1
    assert capsys.readouterr().out.splitlines() == [
        "Docs/A.md:3: local link target does not exist: Gone.md",
        "Docs/B.md:3: Markdown anchor does not exist: #x",
        "markdown_links: checked 2 document(s), 2 finding(s)",
    ]

    _write(root, {"Docs/A.md": "# A\n", "Docs/B.md": "# B\n"})
    assert markdown_links.main(["--root", str(root), "--require-single-h1"]) == 0
    assert capsys.readouterr().out == "markdown_links: checked 2 document(s), 0 finding(s)\n"

    assert markdown_links.main(["--root", str(root), "--list", "--exclude", "Docs/B.md"]) == 0
    assert capsys.readouterr().out == "Docs/A.md\n"


def test_main_rejects_an_empty_selection_and_a_missing_target_root(
        root: Path, capsys: pytest.CaptureFixture[str]) -> None:
    _write(root, {"Docs/A.md": "# A\n"})

    assert markdown_links.main(["--root", str(root), "--include", "Missing/**/*.md"]) == 2
    assert "no documents match" in capsys.readouterr().err

    assert markdown_links.main(["--root", str(root), "--target-root", "../Absent"]) == 2
    assert "target root is not a directory" in capsys.readouterr().err

    assert markdown_links.main(["--root", str(root / "Absent")]) == 2
    assert "root is not a directory" in capsys.readouterr().err


def test_cli_output_is_utf8_when_redirected(root: Path) -> None:
    _write(root, {"Документы/Café.md": "# Café\n\n[нет](Нет-é.md)\n"})

    result = subprocess.run(
        [sys.executable, "-I", str(BUILDTOOLS_DIR / "markdown_links.py"), "--root", str(root)],
        capture_output=True,
        check=False,
    )

    assert result.returncode == 1
    assert result.stdout.decode("utf-8").splitlines() == [
        "Документы/Café.md:3: local link target does not exist: Нет-é.md",
        "markdown_links: checked 1 document(s), 1 finding(s)",
    ]
