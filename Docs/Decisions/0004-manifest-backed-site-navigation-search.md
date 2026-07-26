# ADR-0004: Manifest-Backed Site Navigation And Search

- Status: Accepted
- Date: 2026-07-15
- Owners: documentation, tooling

## Context

FOnline documentation is published from repository Markdown through GitHub Pages/Jekyll at `https://fonline.ru`. The publication route and AI delivery are already source-owned, but the rendered site still presents the basic Slate repository page: readers have no persistent documentation map, integrated search, current-version identity, mobile documentation navigation, or page-local table of contents.

Hand-authored Jekyll menus would duplicate `Docs/documentation-manifest.json`. Hosted search or a separate site application would add another deployment and availability boundary. Moving all pages or adding front matter only to satisfy a theme would also mix presentation metadata into the canonical technical corpus before the accepted locale migration.

## Decision

1. `Docs/documentation-manifest.json` owns site navigation groups and search policy in `site_delivery`. Navigation entries refer to stable document IDs, never copied titles or handwritten URLs.
2. `BuildTools/docs_site.py` deterministically generates:
   - `_data/docs-site.json`, consumed by Jekyll/Liquid for navigation, site identity, repository identity, and the rolling source-ref indicator;
   - `assets/docs-search.json`, consumed by the local browser script for static search.
3. Main navigation contains every public, current, human-facing top-level document exactly once. Generated reference detail pages stay behind their generated index pages so the sidebar remains scannable.
4. Search includes every public, current, human-facing document, including generated detail pages. Internal records, placeholders, and AI-only maintainer routes are excluded.
5. The search artifact stores compact weighted token postings and result metadata, not full document bodies. Titles and headings receive higher weight than body terms; technical identifiers and their camel-case components remain searchable. The manifest declares a hard byte limit and generation fails instead of silently dropping documents.
6. `_layouts/default.html`, `assets/css/docs.css`, and `assets/js/docs.js` form a thin rendering layer over `{{ content }}`. They may provide responsive navigation, search, page table of contents, copy controls, source links, and a persisted light/dark preference, but they do not own technical prose.
7. The interface uses only repository-owned static assets and browser APIs. It has no hosted search, remote JavaScript/CSS dependency, application build, server API, or generated HTML checked into source.
8. The visible version indicator is `master`, explicitly a rolling branch rather than a stable engine release. Versioned documentation remains blocked on the release/tag support decision.
9. The published FOnline mark is a byte-for-byte copy of the engine-owned `Resources/Radiation.png`. It is presentation-only and may be replaced later through a reviewed branding change without affecting document identity.
10. Markdown must remain readable in the GitHub repository without Jekyll. Navigation/search generation, layout/static contracts, standalone validation, and the GitHub Pages build are required gates in the same change as manifest or rendering updates.

## Consequences

### Positive

- Readers get a stable, responsive documentation shell without losing clean Markdown or GitHub readability.
- Navigation cannot silently omit a new top-level public document or point at a renamed path.
- Search covers generated technical reference without shipping the whole Markdown corpus to the browser.
- The same stable IDs and source ref now drive human navigation, static search, and AI delivery.
- The site remains deployable by the existing GitHub Pages action and custom domain.

### Costs

- Adding or reclassifying public documentation may require assigning its stable ID to a navigation group and regenerating both site and AI artifacts.
- Client-side search is current-revision document search, not a versioned symbol service or semantic retrieval system.
- Layout changes require both source-level interaction tests and an inspected Jekyll artifact because local static tests cannot prove Liquid rendering.
- The current flat English paths remain visible until the locale-tree migration. ADR-0006 and the generated route catalog now reserve the `Docs/en` / `Docs/ru` paths and every required legacy redirect before that move.

## Rejected alternatives

- **Theme-owned front matter on every page:** rejected because it duplicates titles/order and couples technical Markdown to one theme.
- **Hosted search:** rejected because it adds credentials, indexing delay, privacy, and service availability to static documentation.
- **A hand-maintained JavaScript document list:** rejected because it would drift from the manifest and AI catalog.
- **Full Markdown bodies in the search JSON:** rejected because it duplicates the corpus in a browser payload and grows poorly with generated reference.
- **A Node documentation application:** rejected by ADR-0001; GitHub Pages/Jekyll remains the publisher.

## Verification

- `python BuildTools/tests/test_docs_site.py`
- `python BuildTools/tests/test_docs_site_layout.py`
- `python BuildTools/docs_site.py --check`
- `python BuildTools/tests/test_docs_validate.py`
- `python BuildTools/docs_validate.py`
- GitHub Actions `Build documentation site` artifact inspection at desktop and mobile widths

## Related documents

- [ADR-0001](0001-github-pages-markdown-publication.md)
- [ADR-0003](0003-manifest-backed-ai-documentation-delivery.md)
- [ADR-0006](0006-documentation-version-locale-routing.md)
- [SitePublication.md](../SitePublication.md)
- [DocumentationMaintenance.md](../DocumentationMaintenance.md)
- [documentation-manifest.json](../documentation-manifest.json)
