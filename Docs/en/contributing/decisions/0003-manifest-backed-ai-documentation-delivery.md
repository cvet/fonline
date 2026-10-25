---
layout: default
title: "ADR-0003: Manifest-Backed AI Documentation Delivery"
locale: en
document_id: adr-manifest-backed-ai-documentation-delivery
permalink: /Docs/en/contributing/decisions/0003-manifest-backed-ai-documentation-delivery.html
---

# ADR-0003: Manifest-Backed AI Documentation Delivery

- Status: Accepted
- Date: 2026-07-15
- Amended: 2026-08-01 for locale-aware delivery and deterministic evaluation ownership
- Owners: documentation, tooling

## Context

FOnline documentation must work as a standalone source for game developers and AI systems without creating separate, contradictory corpora. The repository already owns canonical Markdown, generated contract JSON, a document/source manifest, and a GitHub Pages/Jekyll route at `https://fonline.ru`.

AI clients still need three discovery surfaces that ordinary page navigation does not provide:

- a concise map of useful pages and machine-readable references;
- a bounded text bundle for systems that cannot crawl a site or repository;
- a public document catalog with stable IDs, canonical/source URLs, ownership, provenance, and content hashes.

Hand-maintaining those surfaces would duplicate the documentation index and drift as pages move, generated references expand, or the English/Russian mirror is introduced.

## Decision

1. `Docs/documentation-manifest.json` is the only source for AI-delivery membership, stable document IDs, audiences, state, ownership, locale policy, publication URL, and source provenance.
2. `BuildTools/docs_ai_delivery.py` deterministically generates three root-level static files:
   - `llms.txt`, a public current-page route grouped by Diataxis kind with an explicitly ordered start section and source-ref-pinned clean Markdown links;
   - `llms-full.txt`, a bounded context bundle of public current Markdown;
   - `docs-manifest.json`, a public machine-readable projection of the source manifest.
3. `llms-full.txt` includes complete authored public current documents and
   generated reference index pages. Generated detail pages stay out because
   their canonical JSON models are more precise. A reviewed
   `exclude_document_ids` list may omit a redundant routing/index page or a specialized migration record linked from an included owner while
   retaining it in `llms.txt`, search, the human site, and
   `docs-manifest.json`; start documents and unknown/non-current IDs cannot be
   excluded.
   Each complete document keeps its ID, source path and canonical `URL:` header; compact labels preserve the byte budget.
4. The reviewed size limit is owned by `ai_delivery.full_context.max_bytes` in the [documentation manifest](../../../documentation-manifest.json). The generator checks it before output. Oversize fails closed: no truncation or unreviewed increase. Whole-document membership, exclusions and generated indexes stay unchanged. Earlier reviewed growth:

   | Date / revision | Complete bundle bytes | Reviewed growth and reason |
   |---|---:|---|
   | Earlier | 1,571,968 | From 1.5 MiB: retain the required backup/recovery runbook and complete owners. |
   | Later | 2,097,153 | +64 KiB: synchronization, pathfinding and rendering guides. |
   | Later | 2,169,979 | +32 KiB: mandatory Engine-update and exhaustive migration guides. |
   | 2026-10-05 | 2,197,968 | +32 KiB: retained baking/testing explanations, migration notes and generated build references. |
   | 2026-10-09, published version 20 | 2,228,855; 112 complete documents | +32 KiB; 631 bytes over the prior limit. Retain current owners and required migration history. |
   | `e1a3815ff96c7d88f47314984e9d9242e37bc585` | 2,282,913 | +32 KiB: optional-component, baking-lock and migration guides; 21,921 bytes over the prior limit. Already in the published manifest. |
   | 2026-10-13, placement selection | 2,294,456 before this ADR row | +32 KiB: retain the effective-placement contract and required version-38 migration record; 696 bytes over the previous limit. No membership or exclusion changes. |
   | 2026-10-21, project-owned resource and animation policy | 2,367,685 before this ADR row | +32 KiB to 2,392,064: retain the complete ownership guides and version-58 migration record; 8,389 bytes over the prior limit. No membership or exclusion changes. |
   | 2026-10-23, player connections and restore | Combined with published project-owned presentation documentation | Retain the player admission/restore contracts and version-60 migration notes within the existing 2,392,064-byte limit; no additional increase, membership or exclusion changes. |
   | 2026-10-24, RPC runtime monitoring | 2,365,180 before this ADR row | +16 KiB: retain complete runtime settings, asynchronous ownership, queue age, enforcement limits and migration/testing guidance; 5,884 bytes over the prior limit. Reviewed growth preserves membership and exclusions. |

   The e1 measurement left 10,847 bytes before its documentation correction; remeasure generated output. Exclusions retain their owning replacements: `tools` after dedicated Mapper/viewer/particle manuals; `ScriptMethodsMap.md` after generated `PUBLIC_API.md`, `GeneratedApiAndMetadata.md` and native API index; `BuildTools/README.md` after generated CLI/helper/package references and `Docs/en/how-to/build/index.md`, `Docs/en/reference/cmake-and-buildtools/pipeline.md`, `Docs/en/how-to/release/packaging.md`. The specialized `script-owned-sprite-animations` migration remains complete and bilingual, linked from the included Engine-upgrade and changelog owners. Omitted pages remain in `llms.txt`, search and the public manifest. Budget review retains complete owning guides and required migration notes.
5. The public manifest includes every public document, including visible placeholder routes, so clients can distinguish a current contract from a migration route. Internal plans and verification records are excluded.
6. Document hashes use normalized UTF-8/LF content. Outputs contain no timestamp or checkout-specific commit hash, so Windows, Linux, local checks, and CI produce byte-identical files.
7. Every public manifest document exposes a canonical HTML URL and a source-ref-pinned `markdown_url`/`raw_url` at GitHub's static raw-content endpoint. `llms.txt` selects the clean Markdown URL as the machine route and keeps canonical HTML as a secondary human route.
8. Published HTML and generated endpoints use the existing GitHub Pages/Jekyll route. The artifacts are plain text or JSON copied by Jekyll; no client-side renderer, API service, or second documentation site is introduced.
9. `Docs/ai-evaluation.json` and its deterministic generated report measure retrieval and source-evidence freshness without claiming model answer correctness. Model-family runs follow the separately reviewed protocol in `Docs/en/contributing/documentation/ai-evaluation.md`.
10. English remains canonical while reviewed Russian records retain the same stable IDs with locale-qualified variants and translation-freshness metadata. AI delivery exposes only current locale records and never treats a missing or stale mirror as current.
11. The generated files are discovery and transport surfaces, not new normative owners. Engine source/tests, canonical Markdown, and generated contract models retain the precedence declared by the source manifest.
12. Focused tests, standalone validation, and GitHub Actions check schema, filters, URLs, hashes, deterministic output, byte budget, workflow wiring, and freshness.

## Consequences

### Positive

- Humans and AI systems route from the same reviewed ownership model.
- `fonline.ru/llms.txt`, `fonline.ru/llms-full.txt`, and `fonline.ru/docs-manifest.json` remain useful without JavaScript or an embedding game repository.
- Retrieval systems can reject stale cached pages by content hash and distinguish current pages, placeholders, generated references, and source provenance.
- Retrieval systems receive version-pinned Markdown without parsing rendered HTML, while people retain canonical `fonline.ru` routes.
- Large generated API inventories remain available as JSON without consuming the bounded prose context.
- A page omitted from the bounded bundle remains first-class human/search/AI
  discovery content and is named explicitly in the public manifest policy.

### Costs

- Every new public Markdown page must have complete manifest metadata before it appears in AI delivery.
- The bounded context budget may eventually require another reviewed increase, a new bundle policy, or multiple task-specific bundles.
- Rolling `master` URLs describe the current documentation revision, not a stable engine release; tagged/versioned documentation remains a separate release-policy task.

## Rejected alternatives

- **A hand-authored `llms.txt`:** rejected because it would duplicate navigation and ownership.
- **One unlimited concatenation of every generated page:** rejected because it would already exceed two megabytes and would mix readable guidance with machine-oriented inventories.
- **Runtime crawling or a server API:** rejected because it adds availability, deployment, and security boundaries to static documentation.
- **Generated HTML or a separate AI docs tree:** rejected because Markdown and source-backed JSON are already the canonical portable formats.
- **Silent byte truncation:** rejected because it can cut a contract mid-document and make content hashes or citations misleading.

## Verification

- `python BuildTools/tests/test_docs_ai_delivery.py`
- `python BuildTools/docs_ai_delivery.py --check`
- `python BuildTools/tests/test_docs_validate.py`
- `python BuildTools/docs_validate.py`
- GitHub Pages-compatible rendering through the existing `Build documentation site` job

## Related documents

- [ADR-0001](0001-github-pages-markdown-publication.md)
- [ADR-0006](0006-documentation-version-locale-routing.md)
- [Documentation site publication](../documentation/site-publication.md)
- [GeneratedApiAndMetadata.md](../../reference/metadata/index.md)
- [Documentation maintenance](../documentation/)
- [ProductionDocumentationPlan.md](https://github.com/cvet/fonline/blob/master/Docs/ProductionDocumentationPlan.md)
