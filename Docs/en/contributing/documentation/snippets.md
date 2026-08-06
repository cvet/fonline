---
layout: default
title: Documentation Snippet Validation
locale: en
document_id: documentation-snippet-validation
permalink: /Docs/en/contributing/documentation/snippets.html
---

# Documentation Snippet Validation

This guide defines the checked contract for fenced examples in the standalone
FOnline Engine documentation. The reviewed policy is
[`BuildTools/SnippetPolicy.json`](../../../../BuildTools/SnippetPolicy.json); the
generated current inventory and result is
[`generated/snippets.json`](../../../generated/snippets.json).

## Contract status

Every fenced block in a public, current, human document from
[`documentation-manifest.json`](../../../documentation-manifest.json) belongs to the
snippet corpus. A fence without a language or with an unsupported language is
an error. Generated reference pages use the same gate as authored pages.

Code, command, configuration, and data languages are normative. Every
normative block must pass its declared parser harness. Plain `text` blocks are
evidence such as expected output, logs, file trees, diagrams, or wire shapes;
they remain inventoried and structurally validated but are not reported as
executable code.

The checked report records the owning stable document ID, heading, source
lines, language, contract, harness, normalized hash, template status, and
result for every block. Normative coverage must be exactly 100 percent.

## Harnesses

| Language | Harness | Checked boundary |
|---|---|---|
| `bash` | `bash-parse` | Static checks plus the real Bash parser in `bash -n` mode |
| `powershell` | `powershell-parse` | Static checks plus the PowerShell language parser |
| `cmake` | `cmake-parse` | Complete command invocations, comments, strings, and balanced arguments |
| `cpp`, `angelscript`, `glsl` | `c-family-parse` | Strings, comments, and balanced parentheses, brackets, and braces |
| `ini` | `ini-parse` | Sections, assignments, continuations, and embedded vertex/fragment shader structure |
| `json` | `json-parse` | Python's strict JSON parser |
| `python` | `python-parse` | Python AST parser |
| `text` | `text-contract` | Non-empty UTF-8 text without forbidden control characters |
| `xml` | `xml-parse` | Python `ElementTree` parse of one complete XML document |

Bash and PowerShell commands are parsed, never executed. Before parsing,
documented angle-bracket placeholders and ellipses are replaced with inert
tokens. The replacement is only a parser accommodation; the generated report
retains `template: true`, so a grammar template cannot be mistaken for a
tested concrete command.

The C-family harness proves lexical structure, not C++ or AngelScript type
correctness. A snippet that claims to compile, bake, launch, or produce a
runtime result still needs the owning source/example test named by its guide.
The snippet gate prevents malformed documentation from landing; it does not
turn an isolated fragment into semantic build evidence.

## Run the gate

From the Engine root, run `python BuildTools/docs_snippets.py --write
--external` after changing fenced content or policy. Run `python
BuildTools/tests/test_docs_snippets.py` for focused regressions and `python
BuildTools/docs_snippets.py --check --external` for the exact CI gate.

`--write` updates only the deterministic report. `--check` rejects missing or
stale output. `--external` requires both real shell parsers; use `DOCS_BASH`
or `DOCS_POWERSHELL` only to select an installed parser executable, never to
replace the harness with a wrapper that executes commands.

The main documentation job runs the focused tests and static freshness check.
The separate `documentation-snippets` job runs the external parsers without a
native Engine build. This keeps quick source validation portable while making
the production parser evidence mandatory in CI.

## Author or change a snippet

1. Put the example in the owning public document and declare one supported
   language on the opening fence.
2. Prefer a complete command or a source-backed example. Use placeholders only
   for genuine grammar/reference parameters, not to omit the core steps of a
   tutorial.
3. Keep expected output in a separate `text` block. Do not label output as a
   shell or source language merely to obtain a stronger-looking result.
4. Regenerate the snippet report and inspect the entry's document ID, heading,
   harness, template flag, and status.
5. Run the semantic owner: compile C++/AngelScript examples, configure CMake,
   bake authored data, or execute the narrow example smoke when the prose
   claims those outcomes.
6. Regenerate localization, site/search, AI evaluation, and AI delivery after
   the snippet report, because those artifacts hash or mirror the changed
   documentation corpus.

Moving a fence changes its recorded source line. Changing its content changes
its normalized hash and generated ID. Both are deliberate freshness signals;
never hand-edit the report to preserve an obsolete identity.

## Templates and expected output

The report marks a block as a template when it contains an angle placeholder
in a command/CMake/text block or an ASCII/Unicode ellipsis. A template can
still be normative and parser-valid, but it is not proof of a runnable task.
Tutorials and release procedures should resolve templates to exact paths,
targets, revisions, and expected success signals through an Engine-owned
example or a tagged public repository.

`text` evidence is checked for transport safety only. When prose claims exact
output, the owning smoke/unit/integration test remains the evidence source and
must be updated in the same change if that output changes.

## Failure handling

- **Missing language:** choose the actual syntax; use `text` only for output
  or diagrams.
- **Unknown language:** add a reviewed harness and policy entry before using
  the language.
- **Static parser failure:** repair the example or mark genuine grammar
  parameters explicitly; do not weaken delimiter/section checks.
- **External shell parser failure:** reproduce with the reported block and
  parser in no-execution mode, then fix shell syntax or placeholder shape.
- **Stale report:** regenerate after all source edits, then review the diff.
- **Semantic build failure despite a green snippet gate:** fix the example and
  its owning compile/bake/smoke test. Lexical validity is not semantic proof.

Do not add an ignore list for inconvenient examples. If a fenced block is not
code or evidence worth maintaining, remove the fence or remove the stale
content.
