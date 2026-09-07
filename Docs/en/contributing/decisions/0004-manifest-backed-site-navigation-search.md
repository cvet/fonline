---
layout: default
title: "ADR-0004: Manifest-Backed Site Navigation And Search"
locale: en
document_id: adr-manifest-backed-site-navigation-search
permalink: /Docs/en/contributing/decisions/0004-manifest-backed-site-navigation-search.html
---

# ADR-0004: Manifest-Backed Site Navigation And Search

- Status: Accepted
- Date: 2026-07-15
- Amended: 2026-08-01 for locale-aware navigation, bounded per-locale search, and rendered-browser validation
- Owners: documentation, tooling

## Context

FOnline documentation is published from repository Markdown through GitHub Pages/Jekyll at `https://fonline.ru`. At adoption, the publication route and AI delivery were source-owned, but the rendered site still presented the basic Slate repository page: readers had no persistent documentation map, integrated search, current-version identity, mobile documentation navigation, or page-local table of contents.

Hand-authored Jekyll menus would duplicate `Docs/documentation-manifest.json`. Hosted search or a separate site application would add another deployment and availability boundary. Moving all pages or adding front matter only to satisfy a theme would also mix presentation metadata into the canonical technical corpus before the reviewed locale migration began.

## Decision

1. `Docs/documentation-manifest.json` owns site navigation groups and search policy in `site_delivery`. Navigation entries refer to stable document IDs, never copied titles or handwritten URLs.
2. `BuildTools/docs_site.py` deterministically generates:
   - `_data/docs-site.json`, consumed by Jekyll/Liquid for navigation, site identity, repository identity, and the rolling source-ref indicator;
   - `assets/docs-search.json` for English and `assets/docs-search.ru.json` for Russian, consumed by the local browser script for locale-scoped static search.
3. Main navigation contains every public, current, human-facing top-level document exactly once. Generated reference detail pages stay behind their generated index pages so the sidebar remains scannable.
4. Each locale index includes every public, current, human-facing document available in that locale, including generated detail pages. Internal records, placeholders, AI-only maintainer routes, and missing translations are excluded. Russian navigation falls back to the English route only where no current Russian counterpart exists; search results never cross locales.
5. Each search artifact stores compact weighted token postings and result metadata, not full document bodies. Titles and headings receive higher weight than body terms; technical identifiers and their camel-case components remain searchable. Pure numeric tokens and terms present in more than 60% of that locale's documents are omitted because they cannot distinguish results. The manifest declares a 1.75 MiB (1,835,008 byte) hard limit for each locale index and generation fails instead of silently dropping documents. Separate limits keep a complete Russian mirror from competing with English for one shared payload budget. The budget was raised from 1.25 MiB when the reviewed Russian mirror reached 163 of 195 documents and the complete current corpus no longer fit; no document or token class was silently removed to make the index pass.
6. `_layouts/default.html`, `assets/css/docs.css`, and `assets/js/docs.js` form a thin rendering layer over <code>&#123;&#123; content &#125;&#125;</code>. They may provide responsive navigation, search, page table of contents, copy controls, source links, and a persisted light/dark preference, but they do not own technical prose.
7. The interface uses only repository-owned static assets and browser APIs. It has no hosted search, remote JavaScript/CSS dependency, application build, server API, or generated HTML checked into source.
8. The visible version indicator is `master`, explicitly a rolling branch rather than a stable engine release. Versioned documentation remains blocked on the release/tag support decision.
9. The published FOnline mark is a byte-for-byte copy of the engine-owned `Resources/Radiation.png`. It is presentation-only and may be replaced later through a reviewed branding change without affecting document identity.
10. Markdown must remain readable in the GitHub repository without Jekyll. Navigation/search generation, layout/static contracts, standalone validation, and the GitHub Pages build are required gates in the same change as manifest or rendering updates.
11. `BuildTools/docs_site_artifact.py` validates the completed `_site` tree rather than inferring rendered correctness from source. Every current and available locale route, copied static endpoint, canonical URL, language, accessibility landmark/name, search result, and publishable local link is checked before the artifact is retained.
12. The layout resolves locale pairs by stable document ID, sets the rendered HTML language, labels navigation in the active locale, and exposes an EN/RU switch only when both current routes exist. The browser gate exercises Russian search and both directions of the paired route.

## Consequences

### Positive

- Readers get a stable, responsive documentation shell without losing clean Markdown or GitHub readability.
- Navigation cannot silently omit a new top-level public document or point at a renamed path.
- Search covers generated technical reference without shipping the whole Markdown corpus to the browser, and each reader downloads only the active locale index.
- The same stable IDs and source ref now drive human navigation, static search, and AI delivery.
- The site remains deployable by the existing GitHub Pages action and custom domain.

### Costs

- Adding, translating, or reclassifying public documentation may require assigning its stable ID to a navigation group and regenerating both locale indexes, routes, navigation, localization status, and AI artifacts.
- Client-side search is current-revision document search, not a versioned symbol service or semantic retrieval system.
- Layout changes require both source-level interaction tests and an inspected Jekyll artifact because local static tests cannot prove Liquid rendering.
- Unmigrated flat English paths remain visible during the locale-tree migration. Reviewed migration groups prove canonical EN/RU pages, stable-ID switching, Russian search, and durable legacy pointer routes; ADR-0006, localization status, and the generated route catalog own the current coverage and remaining paths.

## Rejected alternatives

- **Theme-owned front matter on every page:** rejected because it duplicates titles/order and couples technical Markdown to one theme.
- **Hosted search:** rejected because it adds credentials, indexing delay, privacy, and service availability to static documentation.
- **A hand-maintained JavaScript document list:** rejected because it would drift from the manifest and AI catalog.
- **Full Markdown bodies in the search JSON:** rejected because it duplicates the corpus in a browser payload and grows poorly with generated reference.
- **A Node documentation application:** rejected by ADR-0001; GitHub Pages/Jekyll remains the publisher.

## Verification

- `python BuildTools/tests/test_docs_site.py`
- `python BuildTools/tests/test_docs_site_layout.py`
- `python BuildTools/tests/test_docs_site_artifact.py`
- `python BuildTools/tests/test_docs_browser.py`
- `python BuildTools/docs_site.py --check`
- `python BuildTools/docs_site_artifact.py --site-dir _site`
- `npm --prefix BuildTools/docs-browser run audit`
- `python BuildTools/tests/test_docs_validate.py`
- `python BuildTools/docs_validate.py`
- GitHub Actions `Build documentation site` artifact inspection at desktop and mobile widths

## Related documents

- [ADR-0001](0001-github-pages-markdown-publication.md)
- [ADR-0003](0003-manifest-backed-ai-documentation-delivery.md)
- [ADR-0006](0006-documentation-version-locale-routing.md)
- [Documentation site publication](../documentation/site-publication.md)
- [Documentation maintenance](../documentation/)
- [documentation-manifest.json](../../../documentation-manifest.json)
