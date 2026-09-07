---
layout: default
title: AI Documentation Evaluation
locale: en
document_id: ai-documentation-evaluation
permalink: /Docs/en/contributing/documentation/ai-evaluation.html
---

# AI Documentation Evaluation

This guide defines the versioned evaluation contract for using the standalone
FOnline Engine documentation with retrieval systems and AI assistants. The
source set is [ai-evaluation.json](../../../ai-evaluation.json); the generated current
result is
[ai-evaluation-report.json](../../../generated/ai-evaluation-report.json).

## Contract status

The checked gate proves deterministic retrieval and current answer evidence; it
does not prove model task success by itself. Two isolated model families were
rerun and independently reviewed on 2026-08-04 after source and harness
remediation. Both selected every owning document, met the per-family production
task-success target, and produced no unsupported safety, migration,
compatibility, or release claim. The AI quality exit gate is complete.

The current source contains 27 tasks, 65 retrieval checks, and 92 answer
checks. Its static retrieval threshold is 100 percent: every checked query must
find an expected owner within the declared maximum rank.

The evaluation uses only files in a standalone Engine checkout. Last Frontier,
TLA, private repositories, chat history, and unstated project conventions are
not permitted as answer authority.

## Evaluation categories

The source contains at least two tasks in each required category:

| Category | Scope |
| --- | --- |
| `architecture` | Engine/project ownership, source-layer routing, and project-local dependency integration |
| `scripting` | Module lifecycle, async execution, synchronization, and teardown |
| `content` | Prototype, map, and authored-data contracts |
| `debugging` | Test-boundary selection and native/script/platform diagnosis |
| `migration` | Complete Engine-range adoption, generated-contract disposition, public-contract selection, and revision-pinned build-interface selection |
| `release` | Support evidence, packaging, lifecycle, secret, backup/recovery, and project-owned release gates |

Keep the categories stable so results remain comparable. Add a new category
only with a schema revision, an owning documentation page, and an explicit
reason why the existing categories cannot represent the task.

## Task schema

Every task records:

- a stable task ID, category, and user-style question;
- one primary owning document and optional supporting documents;
- at least two retrieval queries with expected document IDs and maximum rank;
- at least two answer checks with a current document ID, heading anchor, and
  source terms that must still exist;
- forbidden assumptions that a reviewer must reject even when the rest of an
  answer is plausible.

Primary documents must be public, current, and routed to the `ai-agent`
audience. Supporting documents must also be public and current. Questions and
answer-check descriptions cannot depend on a named embedding project.

Answer terms are evidence sentinels, not a substitute for semantic review.
They prove that the recorded source and section still contain the concepts
which the rubric expects; they do not prove that a generated answer used them
correctly.

## Run the deterministic gate

Generate site search first because the evaluation exercises the same compact
index used by the browser:

```bash
python BuildTools/docs_site.py --write
python BuildTools/docs_ai_eval.py --write
python BuildTools/docs_ai_eval.py --check
python BuildTools/tests/test_docs_ai_eval.py
```

`--write` always writes the diagnostic report before returning failure. This
keeps failed ranks, top document IDs, stale anchors, and missing terms
inspectable. `--check` then requires byte-identical committed output and a
retrieval success rate at or above the source-owned threshold.

The current browser/Python ranking contract:

1. tokenizes technical identifiers without translating or stemming them;
2. uses exact terms first and bounded prefix matches second;
3. counts each query token at most once per document;
4. ignores query tokens absent from the compact index;
5. requires at least 60 percent of the effective query tokens;
6. ranks by matched-token count, weighted document score, and title.

The Python implementation in `docs_site.search_documents` and the browser
implementation in `assets/js/docs.js` must change together. Focused tests pin
long-query, absent-token, prefix, and static-layout markers.

## Run model-family evaluations

Use the generated report only after the deterministic gate is green. For each
model family:

1. start with a clean standalone Engine checkout at the report's `source_ref`;
2. provide only `llms.txt`, `docs-manifest.json`, the clean Markdown endpoints,
   and machine-readable models selected through those entry points;
3. run every task without project repositories or previous-task conversation;
4. retain the exact model/provider/version, system prompt, retrieval results,
   selected document IDs, answer, latency, and token usage;
5. score every answer check as `pass`, `fail`, or `not-observable`;
6. score every forbidden assumption independently;
7. record reviewer identity and notes for safety, migration, compatibility, and
   support claims;
8. preserve the raw run under `Workspace/ai-evaluation/<run-id>.json` and add a
   dated aggregate result to the verification report.

Run at least two materially different model families. A second alias, size, or
deployment of the same underlying family is not independent evidence. Do not
place provider credentials or private prompts in the repository or report.

The optional local reference harness talks to Ollama and is not a CI or network
dependency of the public documentation. It streams responses under one
deadline, fixes temperature and seed to zero, records the exact model digest,
provider version, prompts, source/input hashes, candidate documents, raw
response attempts, latency, and token counts, and unloads the model after an
error. It keeps completed compatible tasks when `--resume` retries an
interrupted run. One invalid-response retry records the failed response and
disables model thinking for the retry. After valid JSON is available, one
semantic-completion repair may run only for an objective defect: a missing
required evidence token, invalid selected document ID, missing primary owner,
or zero valid citations. Every attempt is retained, hidden reasoning is
redacted, and semantic review remains independent. Run one family with an
explicit, retained profile:

```bash
python BuildTools/docs_ai_model_eval.py --model gpt-oss:20b --family gpt-oss --max-candidates 6 --max-document-bytes 24000 --max-context-bytes 60000 --num-context 32768 --num-predict 6000 --timeout 300 --output Workspace/ai-evaluation/2026-08-04-gpt-oss-20b-v2.json
python BuildTools/docs_ai_model_eval.py --model gpt-oss:20b --family gpt-oss --max-candidates 6 --max-document-bytes 24000 --max-context-bytes 60000 --num-context 32768 --num-predict 6000 --timeout 300 --resume --output Workspace/ai-evaluation/2026-08-04-gpt-oss-20b-v2.json
```

The harness supplies the question and retrieved corpus but keeps the answer
rubric hidden. Its input observations must show that every hidden criterion was
actually present before an omission can count against the model. For each
candidate that exposes an opening decision, route, fast-convention, or purpose
summary, the harness repeats that verbatim block plus as many query-relevant
sections as fit in a 14,000-byte per-document quick-evidence budget. This is a
retrieval presentation layer, not answer evidence invented by the harness: the
raw prompt retains document IDs, paths, exact text, and hashes, and the hidden
answer checks never enter the prompt.

`--self-review` is only a diagnostic second pass. A two-task smoke run did not
materially close the omissions in the 2026-08-04 baseline. Owner-only excerpts
also shortened answers without closing the composite criteria. The retained
quick-evidence strategy improved the five-task maintainer smoke materially,
especially for AngelScript and layered release evidence, but smoke estimates do
not replace a full run and independent review.

Create a compact review, fill it directly or apply an independently produced
structured suggestion, then finalize and verify it:

```bash
python BuildTools/docs_ai_model_review.py --write-template --run Workspace/ai-evaluation/2026-08-04-gpt-oss-20b-v2.json --reviewer REVIEWER_ID --output Docs/_meta/ai-evaluation/2026-08-04-gpt-oss-20b.review.json
python BuildTools/docs_ai_model_review.py --apply-suggestion --suggestion Workspace/ai-evaluation/2026-08-04-gpt-oss-20b.review-suggestion.json --output Docs/_meta/ai-evaluation/2026-08-04-gpt-oss-20b.review.json
python BuildTools/docs_ai_model_review.py --finalize --output Docs/_meta/ai-evaluation/2026-08-04-gpt-oss-20b.review.json
python BuildTools/docs_ai_model_review.py --check --require-run --output Docs/_meta/ai-evaluation/2026-08-04-gpt-oss-20b.review.json
```

Raw reports stay in ignored `Workspace/`; version only the compact internal
review. Plain `--check` validates that compact evidence in a checkout without
the raw run. `--require-run` additionally requires the raw file, checks its
SHA-256 digest, and compares the embedded model, provider, parameters, source,
input hashes, and completion time.

## Current reviewed baseline

Both final 2026-08-04 runs used Ollama 0.32.5, the explicit profile above,
identical documentation inputs, harness SHA-256
`5fdd971f46898f789817c1f87531af862ec4ccd3b38e30f6a9ce987915f3aa09`,
and an independent answer-by-answer semantic review.

| Family and exact model | Model digest | Owner selected | Input rubric evidence | Reviewed task success |
| --- | --- | ---: | ---: | ---: |
| Qwen 3.5, `huihui_ai/qwen3.5-abliterated:9b` | `92a443adb124f5e805bbdee23fdb38fcd22a7bf00a1016b53f764e741369c600` | 27/27 | 92/92 | 27/27 (100%) |
| GPT-OSS, `gpt-oss:20b` | `17052f91a42e97930aa6e28a6c6c06a983e6a58dbb00434885a0cf5313e376f7` | 27/27 | 92/92 | 25/27 (92.6%) |

The retained Qwen raw run is
`Workspace/ai-evaluation/2026-08-04-qwen3.5-9b-v12-final.json` with SHA-256
`30acb42ee573e85176cbf3f58cbd34d922b21b4742b984713fa04f58e07ff466`;
its compact review is
`Docs/_meta/ai-evaluation/2026-08-04-qwen3.5-9b-v12-final.review.json` with
SHA-256
`c579f6d400f575b191c2a8f3c2ffad97848527046010465ba16a04f09fcc814d`.
The retained GPT-OSS raw run is
`Workspace/ai-evaluation/2026-08-04-gpt-oss-20b-v13-final.json` with SHA-256
`49b2706895a13c477e04669ea89df0dd82a3407ba3ab4af49c3d8de985611291`;
its compact review is
`Docs/_meta/ai-evaluation/2026-08-04-gpt-oss-20b-v13-final.review.json` with
SHA-256
`e58b5cad481fa0d9854d8f561fc30d7dce949945b3bb8c405fd9a8137983d8ec`.

The shared input hashes are
`dd3e5f83d17b2b5aafcf787f374446929f8d412db68ed6a193aaed0479d2e310`
for `Docs/ai-evaluation.json`,
`b264cb916ab4690829502de18a9ca7328e252fd3adc587dec3362e1403bd4499`
for `Docs/generated/ai-evaluation-report.json`,
`f14577a60f6d7e074bdbe24b890b927de8ed9797ddcc5f6e621353cc19865d30`
for `docs-manifest.json`, and
`471a86233febdf79dd931d16ef077d854839266852682f500a2a0ee533c4407f`
for `llms.txt`.

Qwen passed every task. GPT-OSS failed the focused-viewer task by prescribing
Mapper screenshot APIs that the supplied evidence boundary explicitly forbade,
and failed the complete Engine-adoption workflow by repeating stage names
without an actionable persisted-state and validation procedure. Neither failure
is an unsupported safety, migration, compatibility, or release claim. The
active two-family qualification therefore exceeds the unchanged 90-percent
target. Earlier compact reviews remain immutable historical evidence rather
than being rewritten as current results.

## Scoring

Report these dimensions separately:

- owning-document selection;
- retrieval success at the declared rank;
- answer-check coverage;
- unsupported or project-specific assumptions;
- version/source-ref selection;
- final task success.

For an answer check, `pass` means the answer semantically satisfies the check.
For a forbidden assumption, `pass` means the answer did not make that forbidden
claim. `not-observable` means the isolated input did not expose enough evidence
to score the criterion and therefore cannot be counted as task success.

The production target is at least 90 percent final task success for each tested
family, with zero unsupported safety, migration, compatibility, or release
claims. A high aggregate score cannot waive a release-blocking error in one of
those areas.

Static retrieval is intentionally stricter than the model-family production
target: every checked query must remain within its declared rank. Lowering the
100-percent source threshold requires a reviewed explanation in the plan and
verification report, not merely a regenerated result.

## Respond to a regression

Inspect the task's `top_document_ids` before changing weights:

- If another result is the true owner, correct task ownership and navigation.
- If the owner lacks the user's vocabulary, improve its title, headings,
  prerequisites, or terminology without duplicating another page.
- If several pages claim the same behavior, choose one owner and turn the
  others into scoped routes.
- If a query contains only unsupported assumptions, change the task rather
  than teaching the search index a private project term.
- If evidence moved, update the stable heading deliberately and preserve any
  public route that already depends on it.

Never add irrelevant keyword lists, hidden text, or artificial repetitions to
make a rank green. Retrieval improvements must also improve a human reader's
ability to select the correct page.

## Maintenance triggers

Regenerate and review the evaluation when:

- a task-owning document ID, title, path, audience, or heading changes;
- navigation/search tokenization, document-frequency filtering, or ranking
  changes;
- a source-ref or documentation version policy changes;
- a new public workflow displaces an existing owning page;
- a model run finds ambiguity, a missing prerequisite, or an unsupported
  assumption.

Run `docs_site.py`, then `docs_ai_eval.py`, then `docs_ai_delivery.py`.
AI delivery publishes the evaluation report as a machine-readable reference;
the rendered-site validator checks that the byte-identical JSON endpoint
survives Jekyll.

## Limitations

The deterministic gate does not evaluate prose quality, reasoning, command
execution, code compilation, visual understanding, or provider behavior. It
also cannot prove that a model ignored information outside the supplied
standalone corpus unless the run harness records and isolates its inputs.

Human usability studies, semantic compile/bake/execute evidence for examples,
browser accessibility, Russian retrieval parity, and live production endpoint
monitoring remain separate requirements. Fenced syntax and template coverage
are owned by [Documentation Snippet Validation](snippets.md).
