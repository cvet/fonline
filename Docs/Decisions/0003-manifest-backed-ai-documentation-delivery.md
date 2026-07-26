# ADR-0003: Manifest-Backed AI Documentation Delivery

- Status: Accepted
- Date: 2026-07-15
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
   - `llms.txt`, a public current-page route grouped by Diataxis kind with an explicitly ordered start section;
   - `llms-full.txt`, a bounded context bundle of public current Markdown;
   - `docs-manifest.json`, a public machine-readable projection of the source manifest.
3. `llms-full.txt` includes complete authored public current documents and generated reference index pages. Generated detail pages stay out of the bundle because their canonical JSON models are more precise and several individual pages are too large for a general routing context.
4. The full-context byte budget is declared in the source manifest and enforced before writing output. Growth beyond the budget fails validation; it is never handled by silently truncating a document. The reviewed current budget is 1.25 MiB (1,310,720 bytes); it was raised from 1 MiB when the complete image-format contract brought the source-backed bundle to 1,064,128 bytes.
5. The public manifest includes every public document, including visible placeholder routes, so clients can distinguish a current contract from a migration route. Internal plans and verification records are excluded.
6. Document hashes use normalized UTF-8/LF content. Outputs contain no timestamp or checkout-specific commit hash, so Windows, Linux, local checks, and CI produce byte-identical files.
7. Published URLs use the existing GitHub Pages/Jekyll route. The artifacts are plain text or JSON copied by Jekyll; no client-side renderer, API service, or second documentation site is introduced.
8. English remains the canonical locale until the accepted locale-tree migration. Future Russian records retain the same stable IDs with locale-qualified variants and translation-freshness metadata; this ADR does not start that migration.
9. The generated files are discovery and transport surfaces, not new normative owners. Engine source/tests, canonical Markdown, and generated contract models retain the precedence declared by the source manifest.
10. Focused tests, standalone validation, and GitHub Actions check schema, filters, URLs, hashes, deterministic output, byte budget, workflow wiring, and freshness.

## Consequences

### Positive

- Humans and AI systems route from the same reviewed ownership model.
- `fonline.ru/llms.txt`, `fonline.ru/llms-full.txt`, and `fonline.ru/docs-manifest.json` remain useful without JavaScript or an embedding game repository.
- Retrieval systems can reject stale cached pages by content hash and distinguish current pages, placeholders, generated references, and source provenance.
- Large generated API inventories remain available as JSON without consuming the bounded prose context.

### Costs

- Every new public Markdown page must have complete manifest metadata before it appears in AI delivery.
- The 1.25 MiB context budget may eventually require another reviewed increase, a new bundle policy, or multiple task-specific bundles.
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
- [SitePublication.md](../SitePublication.md)
- [GeneratedApiAndMetadata.md](../GeneratedApiAndMetadata.md)
- [DocumentationMaintenance.md](../DocumentationMaintenance.md)
- [ProductionDocumentationPlan.md](../ProductionDocumentationPlan.md)
