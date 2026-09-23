---
layout: default
title: "ADR-0006: Documentation Version, Locale, And Stable Route Policy"
locale: en
document_id: adr-documentation-version-locale-routing
permalink: /Docs/en/contributing/decisions/0006-documentation-version-locale-routing.html
---

# ADR-0006: Documentation Version, Locale, And Stable Route Policy

- Status: Accepted
- Date: 2026-07-16
- Amended: 2026-08-01 after the reviewed physical EN/RU migration groups
- Owners: documentation, build-release

## Context

FOnline publishes repository Markdown through GitHub Pages/Jekyll at `https://fonline.ru`. ADR-0001 selected the future `Docs/en` and `Docs/ru` layout, ADR-0003 made the same corpus available to AI clients, and ADR-0004 added manifest-backed navigation and search. Those decisions did not yet define one executable contract for:

- the meaning of the visible documentation version;
- when release snapshots may be published;
- the stable URL of every current public page;
- the English destination and Russian mirror path of every human page;
- redirects when the flat English tree moves;
- multiple legacy pages that converge on one canonical replacement.

Moving files or beginning translation without that contract would make redirects, language switching, search, and AI retrieval depend on handwritten path knowledge.

## Source Paths Inspected

- `Docs/documentation-manifest.json`
- `BuildTools/docs_ai_delivery.py`
- `BuildTools/docs_site.py`
- `BuildTools/docs_validate.py`
- `BuildTools/tests/test_docs_ai_delivery.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `BuildTools/tests/test_docs_validate.py`
- `_config.yml`
- `_layouts/default.html`
- `Docs/en/contributing/documentation/site-publication.md`
- `Docs/ProductionDocumentationPlan.md`
- `Docs/en/contributing/decisions/0001-github-pages-markdown-publication.md`
- `Docs/en/contributing/decisions/0004-manifest-backed-site-navigation-search.md`

## Decision

### Current documentation

1. The unversioned site is the `current` documentation channel.
2. `current` follows the rolling `master` branch and is labeled `Current`, not `Stable`.
3. Current public URLs remain unversioned and stable while their content follows the latest published `master` revision.
4. Source links use the same `master` ref as the displayed documentation.
5. Historical review uses commit-addressable GitHub Actions `_site` artifacts and repository revisions.

### Release documentation

1. Tagged release snapshots remain deferred while the engine has no supported tag series and support matrix.
2. A later release channel must use immutable Git tags and the reserved `/versions/{version}/` path family.
3. Publishing the first release snapshot requires an explicit support policy and a follow-up ADR covering supported lines, retention, selectors, canonical URLs, and unmaintained-version banners.
4. Documentation tooling must not infer a stable release from `VERSION`, a branch name, or a reachable Git tag.

### Locale ownership

1. English (`en`) is canonical. Unmigrated flat English files remain the source until their reviewed group moves; migrated pages are canonical below `Docs/en`.
2. Russian (`ru`) is a whole-document mirror populated in reviewed groups. `Docs/generated/translation-status.json` is the authoritative coverage snapshot; no ADR paragraph owns a manually maintained translation count.
3. Human pages moving under `Docs/en` derive their Russian path by replacing `Docs/en/` with `Docs/ru/`.
4. Repository and subsystem README entry points use manifest-declared paired paths such as `README.md` and `README.ru.md`.
5. Stable document IDs, not translated titles, join English and Russian pages.
6. Translation freshness uses a normalized SHA-256 hash of canonical English content. Existing translations are checked for current hashes, identical fenced code, explicit locale metadata, and language-preserving links; complete parity becomes mandatory at production bilingual launch.
7. `translation-pending` is allowed only before production bilingual launch.

### Stable routes and migration

1. `Docs/documentation-manifest.json` owns versioning, localization, current source paths, stable document IDs, migration dispositions, and planned targets.
2. `BuildTools/docs_site.py` generates `Docs/generated/document-routes.json`.
3. The route catalog records every public page's current URL, canonical future owner, planned English URL, Russian mirror path, migration state, and required legacy redirect.
4. A future target shared by multiple legacy pages must have exactly one non-`replace` canonical owner. Other records are aliases that redirect to that owner.
5. A move is not allowed until the old route remains as a durable Markdown pointer page. This preserves both GitHub repository reading and GitHub Pages/Jekyll navigation without depending on an unsupported redirect plugin or checked-in HTML.
6. Current source-path URLs remain canonical until the corresponding move lands. Once it lands, the new `Docs/en` and `Docs/ru` routes are canonical and the old file becomes a manifest-owned pointer/redirect record. Unmigrated planned URLs remain reservations, not live-page claims.
7. Navigation, per-locale search, AI delivery, localization status, and the rendered version/language controls consume the same manifest policy. They may not maintain independent locale or version declarations.

## Consequences

### Positive

- Every planned English and Russian path is known before files move.
- The current public URL map is reviewable and generated from stable IDs.
- Legacy aliases can converge without making two pages canonical.
- Human and AI outputs identify `master` honestly as a rolling current revision.
- Future release snapshots have a reserved architecture but no unsupported stability promise.
- The solution stays inside repository Markdown and GitHub Pages/Jekyll.

### Costs

- Public page moves require both a new canonical file and an old Markdown pointer route.
- README-style entry points need explicit manifest-owned locale pairs because they do not live under `Docs/en`.
- Locale-aware navigation, hashes, per-locale search, language switching, and durable pointers are proven by reviewed migration groups; repeating the process for the remaining required pages is still substantial work.
- Release version selection remains unavailable until engine release governance exists.

## Rejected Alternatives

- **Treat `master` as stable:** rejected because current refactoring and the absent support matrix do not justify that promise.
- **Copy documentation for every tag now:** rejected because storage does not create a support policy.
- **Move files first and reconstruct redirects later:** rejected because old public URLs would be lost from the source of truth.
- **Use translated titles as locale keys:** rejected because titles change and are not machine-stable.
- **Add a separate documentation application:** rejected by ADR-0001; Markdown and GitHub Pages/Jekyll remain canonical.
- **Require an HTTP redirect plugin:** rejected because durable Markdown pointer pages work in both GitHub and Jekyll and keep the migration reviewable.

## Verification

- `python BuildTools/tests/test_docs_ai_delivery.py`
- `python BuildTools/tests/test_docs_site.py`
- `python BuildTools/tests/test_docs_site_layout.py`
- `python BuildTools/tests/test_docs_site_artifact.py`
- `python BuildTools/tests/test_docs_browser.py`
- `python BuildTools/tests/test_docs_validate.py`
- `python BuildTools/docs_site.py --check`
- `python BuildTools/docs_ai_delivery.py --check`
- `python BuildTools/docs_localization.py --check`
- `python BuildTools/docs_validate.py`

The tests reject version/source-ref drift, malformed locale policy, missing README locale pairs, route collisions, ambiguous canonical targets, stale route output, and layout drift.

## Related Documents

- [ADR-0001](0001-github-pages-markdown-publication.md)
- [ADR-0003](0003-manifest-backed-ai-documentation-delivery.md)
- [ADR-0004](0004-manifest-backed-site-navigation-search.md)
- [Documentation site publication](../documentation/site-publication.md)
- [Documentation maintenance](../documentation/)
- [ProductionDocumentationPlan.md](https://github.com/cvet/fonline/blob/master/Docs/ProductionDocumentationPlan.md)
- [documentation-manifest.json](../../../documentation-manifest.json)
