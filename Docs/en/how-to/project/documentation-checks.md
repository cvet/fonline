---
layout: default
title: Check Documentation Links in a Game Project
locale: en
document_id: project-documentation-checks
permalink: /Docs/en/how-to/project/documentation-checks.html
---

# Check Documentation Links in a Game Project

This guide shows how a game project checks its own Markdown with the Engine's `BuildTools/markdown_links.py`: local links must reach existing files, `#fragments` must match a real heading or HTML anchor, files must be valid UTF-8 with balanced code fences, and each page can be required to carry one H1. It covers the commands, exit codes, every option, a thin project wrapper, local and CI wiring, and the migration of a project that kept its own copy of such a checker.

The Engine owns the checker and runs the same link logic on its own documentation through `BuildTools/docs_validate.py`. The project owns its scope: which files are documentation, which trees are excluded and why, and any extra rules.

## Source paths inspected

- `BuildTools/markdown_links.py`
- `BuildTools/tests/test_markdown_links.py`
- `BuildTools/docs_validate.py`
- `BuildTools/docs_ai_eval.py`
- `BuildTools/docs_ai_model_eval.py`
- `BuildTools/HelperCliInterface.json`
- `.github/workflows/validate.yml`
- `Docs/en/how-to/project/quality-gates.md`
- `Docs/en/how-to/project/continuous-integration.md`
- `Docs/en/how-to/project/vscode-workspace.md`

## What it checks

| Check | Rule | Default |
|---|---|---|
| UTF-8 | Strict decoding; a leading byte order mark is accepted; a U+FFFD replacement character is reported with its line | Always |
| Fences | A fence opened with three or more backticks or tildes must close with the same character, at least as long, and nothing after it | Always |
| Local targets | Inline links and images, HTML `href`/`src` attributes and reference definitions (`[label]: path`) must resolve to an existing file or directory | Always |
| Root boundary | A resolved target must stay inside `--root` or a `--target-root`; symbolic links are resolved first | Always |
| Anchors | A `#fragment` on a `.md` or `.markdown` target must equal a heading slug or an explicit `id`/`name` attribute in that file | Always |
| One H1 | Exactly one `#` heading outside front matter and fences | `--require-single-h1` |
| Reference labels | `[text][label]` and `[text][]` need a `[label]:` definition in the same file | `--require-reference-definitions` |

How targets are read:

- A target is relative to the document's directory. A target starting with `/` is relative to `--root`, the way GitHub renders repository links.
- Percent-encoding is decoded, `?query` is ignored, `"titles"` are stripped and `<angle brackets>` allow spaces in a path.
- Any target with a URL scheme (`https:`, `mailto:`) or starting with `//` is external and is never fetched.
- Fragments on non-Markdown targets (for example `Source/Game.cpp#L10`) are not checked; a fragment on a directory is not checked.
- Front matter (a block between `---` lines at the top of the file), fenced code and inline code spans are not prose: links and headings inside them are ignored. Footnote definitions (`[^1]: text`) are not links.

### Heading slugs

The slug follows GitHub and the kramdown GFM parser used by GitHub Pages: link markup keeps only its text, HTML tags, backticks and asterisks are removed, the text is lower-cased, every character other than a letter, digit, underscore, hyphen or space is removed, and then every space becomes a hyphen. Spaces are not collapsed, so a dropped separator leaves a double hyphen.

| Heading | Fragment |
|---|---|
| `` ## The `verify` macro `` | `#the-verify-macro` |
| `## MCP / AiControl Validation` | `#mcp--aicontrol-validation` |
| `## GuardSystem (охрана)` | `#guardsystem-охрана` |
| second `## Status` in a file | `#status-1` |

Fragments are compared case-sensitively. When a heading must keep a stable fragment across rewording, add an explicit anchor before it, such as `<a id="inbound-hardening"></a>`.

### Limits

The checker understands ATX headings and single-line links. It does not render Markdown: setext headings (`===` underlines) create no anchors, link text that spans lines is not seen, and fences inside block quotes are not recognized. It does not check heading-level order or prose. Use the rendered-site checks of the publishing tool for rendering questions.

## Run it

From the root of a game project that embeds the Engine at `Engine/`, list the selected documents first, then check them.

```powershell
py -3 Engine\BuildTools\markdown_links.py --root . --exclude Engine --exclude Build --list
py -3 Engine\BuildTools\markdown_links.py --root . --exclude Engine --exclude Build
```

```bash
python3 Engine/BuildTools/markdown_links.py --root . --exclude Engine --exclude Build --list
python3 Engine/BuildTools/markdown_links.py --root . --exclude Engine --exclude Build
```

Excluding `Engine` removes the Engine's own pages from the document set, but links from project pages into `Engine/` are still resolved and their anchors are still checked. If the submodule is not checked out, those links fail as missing targets, which is the correct signal for a documentation job.

Each finding is one line, sorted by file and line, followed by a summary:

```text
Docs/Combat.md:14: local link target does not exist: Weapons.md
Docs/Combat.md:31: Markdown anchor does not exist: ../Engine/Docs/en/index.md#start-here
Docs/Quests.md:5: fenced code block is never closed
Docs/World.md: expected exactly one H1 heading, found 2
markdown_links: checked 64 document(s), 4 finding(s)
```

Output is UTF-8 even when redirected on Windows, so non-Latin file names survive in CI logs.

### Exit codes

| Code | Meaning |
|---|---|
| `0` | No findings, or `--list` completed |
| `1` | At least one finding was printed |
| `2` | Invalid invocation: unknown option, `--root` or a `--target-root` is not a directory, or no document matches the include and exclude patterns |

An empty selection fails on purpose: a mistyped include must not turn the gate green.

## Options

| Option | Purpose |
|---|---|
| `--root DIR` | Repository root that bounds link targets and anchors the patterns; default is the current directory |
| `--include GLOB` | Document pattern, repeatable; default `**/*.md` and `**/*.markdown` |
| `--exclude GLOB` | File or directory pattern removed from the document set, repeatable; a directory match removes everything below it |
| `--target-root DIR` | Extra directory, relative to `--root`, that links may enter although it is outside the root; its files are not checked as documents |
| `--skip-target GLOB` | Root-relative pattern of targets that are not verified, for a tree that is legitimately absent, such as an optional asset submodule |
| `--allow-line-fragments` | Accept `#L10` and `#L10-L20` on Markdown targets |
| `--require-single-h1` | Report files without exactly one H1 |
| `--h1-exempt GLOB` | Files exempt from `--require-single-h1`, such as one-line instruction import files |
| `--require-reference-definitions` | Report reference-style links whose label is undefined |
| `--list` | Print the selected documents and exit |

Patterns are root-relative and use `/` on every host: `*` and `?` stay inside one path segment, `**` crosses segments. `.git` is never walked and directory symbolic links are not followed. The generated [helper CLI reference](../../reference/helper-cli/commands.md) records the exact help text of the current revision.

Use `--target-root` only for a tree outside the root, for example an Engine checked out beside the project as `../Engine`. An Engine embedded at `Engine/` as a submodule is already inside the root and needs only `--exclude Engine`. When `Engine/` is a symbolic link or a Windows junction, its targets resolve outside the root and are reported as escaping it; add `--target-root Engine` in that layout.

## Wrap it in the project

Keep the scope in one project-owned script so local tasks, hooks and CI run the same check. Every exclusion and skipped target carries its reason on the same line, so a reviewer sees why a tree is out of scope.

```python
"""Check the project's Markdown links with the Engine checker."""

from __future__ import annotations

from pathlib import Path
import sys

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "Engine" / "BuildTools"))

import markdown_links  # noqa: E402

EXCLUDES = [
    "Engine",  # Engine pages are checked by the Engine; links into it stay checked
    "Build",  # generated build output
    "Workspace",  # task scratch files are not maintained documentation
    "**/node_modules",  # vendored packages with upstream documentation
    "ThirdParty",  # upstream documentation keeps its own link conventions
]
SKIP_TARGETS = [
    "Art",  # optional asset submodule that documentation jobs do not check out
]
H1_EXEMPT = [
    "CLAUDE.md",  # one-line import of AGENTS.md without a heading
]


def main() -> int:
    arguments = ["--root", str(PROJECT_ROOT), "--require-single-h1"]
    for pattern in EXCLUDES:
        arguments += ["--exclude", pattern]
    for pattern in SKIP_TARGETS:
        arguments += ["--skip-target", pattern]
    for pattern in H1_EXEMPT:
        arguments += ["--h1-exempt", pattern]
    return markdown_links.main(arguments + sys.argv[1:])


if __name__ == "__main__":
    raise SystemExit(main())
```

Save it as `tools/check_docs.py`. Extra arguments pass through, so `py -3 tools\check_docs.py --list` (Windows) and `python3 tools/check_docs.py --list` (Linux) show the selection.

### Add a local task

A VS Code task that runs the wrapper, in the style of the [recommended task graph](vscode-workspace.md#recommended-task-graph):

```json
{
  "label": "Check :: Docs Links",
  "type": "shell",
  "windows": { "command": "py", "args": ["-3", "tools/check_docs.py"] },
  "linux": { "command": "python3", "args": ["tools/check_docs.py"] },
  "problemMatcher": []
}
```

### Run it in CI

Run the wrapper in the documentation job on every pull request, including documentation-only changes, after the Engine submodule is checked out; see [skipping runtime jobs](continuous-integration.md#skip-runtime-jobs-for-documentation-only-changes). A step for a GitHub Actions Ubuntu runner:

```text
- name: Check documentation links
  run: python3 tools/check_docs.py
```

The step needs only Python 3.12 or newer and no network access. Fail the job on any non-zero exit code; do not add `continue-on-error`.

## Extend it in a project

- **Scope and exemptions.** Change only the wrapper's lists. Add an exclusion when a tree is not maintained documentation, never to hide a broken link in a maintained page. A skipped target is for content that is legitimately absent from the checkout; links to anything else must resolve.
- **Project rules.** Put extra rules in a separate function of the wrapper, or a separate script, that uses the library functions `collect_documents`, `iter_links`, `resolve_local_target`, `heading_anchors` and `check_links`. For example, a project can forbid links into its scratch directory:

```python
def forbid_scratch_links(root: Path, documents: list[str]) -> list[str]:
    findings: list[str] = []
    for document in documents:
        path = root / document
        for line, target in markdown_links.iter_links(path.read_text(encoding="utf-8")):
            if markdown_links.is_external_target(target) or target.startswith("#"):
                continue
            resolved, _ = markdown_links.resolve_local_target(root, path, target)
            if resolved.is_relative_to(root / "Workspace"):
                findings.append(f"{document}:{line}: link into Workspace/: {target}")
    return findings
```

- **Special corpora.** Documents with their own verifier, such as evidence copies that must be checked against a pinned revision, stay with that project verifier. Exclude them from the generic run so each file has one owner.
- **Stricter structure.** Heading-level order, title style or language rules are project checks beside the wrapper; the Engine checker does not enforce them.

### Migrate a project-owned copy

A project that carried its own link or anchor checker moves to the Engine checker in one change:

1. Record the old tool's scope: its document discovery, excluded directories, skipped targets, H1 exemptions and accepted fragment forms. Run it once and keep its document count and findings.
2. Translate the scope into the wrapper: excluded directories become `--exclude`, a never-checked-out submodule becomes `--skip-target`, an embedded Engine becomes `--exclude Engine`, a sibling Engine becomes `--target-root`, accepted `#L10` fragments become `--allow-line-fragments`, a one-H1 rule becomes `--require-single-h1` with `--h1-exempt` entries. A tool that read `git ls-files` now walks the tree, so exclude ignored output and cache directories explicitly.
3. Compare `tools/check_docs.py --list` with the old document set until they match.
4. Run the wrapper and fix every new finding in the documents. Expect findings the old copy missed: HTML `href`/`src` attributes and `<angle-bracket>` destinations are now checked, `/`-prefixed targets resolve from the root, fragments are case-sensitive, and slugs keep one hyphen per space (`a--b` for `A / B`).
5. Keep only project-specific verifiers, delete the old checker and its tests, and point the local task and CI step at the wrapper.

## Validation checklist

1. `tools/check_docs.py --list` prints the maintained documents and nothing from excluded trees.
2. A deliberately broken link and a deliberately wrong `#fragment` in a scratch copy each produce one finding and exit code `1`.
3. A clean run prints `0 finding(s)` and exits `0` on Windows and Linux.
4. Every exclusion and skipped target in the wrapper has a reason on its line.
5. The CI documentation job runs the wrapper on documentation-only changes and fails on a non-zero exit.
6. No project copy of an older checker remains in the repository or in task definitions.
