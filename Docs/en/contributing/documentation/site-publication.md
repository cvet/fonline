---
layout: default
title: Documentation Site Publication
locale: en
document_id: documentation-site-publication
permalink: /Docs/en/contributing/documentation/site-publication.html
---

# Documentation Site Publication

> Engine-owned documentation. This page defines how the FOnline Markdown corpus is previewed, validated, and published through the existing GitHub Pages route.

## Purpose

Use this page when changing `_config.yml`, the documentation rendering layer, the custom domain, or the documentation jobs in GitHub Actions. It does not authorize a separate documentation application or a second content tree: repository Markdown remains canonical.

## Source paths inspected

- `_config.yml`
- `CNAME`
- `.ruby-version`
- `Gemfile`
- `.gitignore`
- `.github/workflows/validate.yml`
- `Docs/documentation-manifest.json`
- `BuildTools/docs_ai_delivery.py`
- `BuildTools/tests/test_docs_ai_delivery.py`
- `BuildTools/docs_ai_eval.py`
- `BuildTools/tests/test_docs_ai_eval.py`
- `BuildTools/SnippetPolicy.json`
- `BuildTools/docs_snippets.py`
- `BuildTools/tests/test_docs_snippets.py`
- `BuildTools/DocumentationDiagrams.json`
- `BuildTools/docs_diagrams.py`
- `BuildTools/tests/test_docs_diagrams.py`
- `BuildTools/DocumentationScreenshots.json`
- `BuildTools/docs_screenshots.py`
- `BuildTools/tests/test_docs_screenshots.py`
- `BuildTools/docs_site.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `BuildTools/docs_site_artifact.py`
- `BuildTools/tests/test_docs_site_artifact.py`
- `_layouts/default.html`
- `assets/css/docs.css`
- `assets/js/docs.js`
- `assets/images/fonline-mark.png`
- `_data/docs-site.json`
- `assets/docs-search.json`
- `assets/docs-search.ru.json`
- `Docs/generated/document-routes.json`
- `Docs/ai-evaluation.json`
- `Docs/generated/ai-evaluation-report.json`
- `Docs/generated/snippets.json`
- `Docs/generated/diagrams.json`
- `Docs/assets/diagrams/`
- `Docs/generated/screenshots.json`
- `Docs/assets/screenshots/`
- `llms.txt`
- `llms-full.txt`
- `docs-manifest.json`
- `Docs/en/contributing/decisions/0001-github-pages-markdown-publication.md`
- `Docs/en/contributing/decisions/0003-manifest-backed-ai-documentation-delivery.md`
- `Docs/en/contributing/decisions/0004-manifest-backed-site-navigation-search.md`
- `Docs/en/contributing/decisions/0006-documentation-version-locale-routing.md`

## Production contract

| Concern | Contract |
|---|---|
| Canonical content | Versioned Markdown in this repository |
| Production provider | GitHub Pages |
| Renderer | GitHub Pages-compatible Jekyll |
| Production URL | `https://fonline.ru` |
| Custom-domain source | Root `CNAME`, containing only `fonline.ru` |
| Site configuration | Root `_config.yml` |
| Rendering layer | GitHub Pages-supported themes, plugins, layouts, includes, data, and static assets only |
| Reader navigation | Generated `_data/docs-site.json`, consumed by the repository-owned default layout |
| Static search | Generated locale-scoped `assets/docs-search.json` and `assets/docs-search.ru.json`, queried entirely in the browser |
| Teaching diagrams | Source-owned local SVG under `Docs/assets/diagrams/`, with exact provenance and hashes in `Docs/generated/diagrams.json` |
| Tool screenshots | Source-owned local PNG under `Docs/assets/screenshots/`, with capture environment, interactions, source/image hashes, and recapture triggers in `Docs/generated/screenshots.json` |
| Version, locale, and route map | Generated `Docs/generated/document-routes.json`, derived from stable document IDs and manifest targets |
| Review output | Commit-addressable `_site` artifact plus rendered-site validation report from GitHub Actions |
| AI delivery | Root `llms.txt`, bounded `llms-full.txt`, public `docs-manifest.json`, deterministic AI evaluation, and complete snippet coverage reports |

The publication route is intentionally independent from any embedding game project. Last Frontier, TLA, and public example games may link to this site, but they neither build nor define it.

<figure class="docs-diagram">
<picture>
<source media="(max-width: 700px)" srcset="../../../assets/diagrams/documentation-delivery-mobile.svg">
<img src="../../../assets/diagrams/documentation-delivery.svg" alt="Documentation delivery diagram. Canonical Markdown and the documentation manifest feed deterministic generators. Generated navigation, search, diagrams, route catalogs, AI bundles, and evaluation reports are checked in. Jekyll builds the human site while AI clients consume llms.txt, llms-full.txt, docs-manifest.json, and generated JSON. Static, browser, accessibility, hash, and freshness gates validate the same revision." loading="lazy">
</picture>
<figcaption>Human and AI routes are projections of one versioned Markdown corpus. GitHub Pages, machine-readable endpoints, and CI evidence all use the same manifest, generated artifacts, source revision, and content hashes.</figcaption>
</figure>

GitHub Pages uses `jekyll-readme-index`, which normally changes a nested `README.md` route to the directory index. The seven public root/subsystem/example READMEs therefore pin their manifest-owned `.html` routes in short YAML front matter blocks. `_config.yml` excludes local build trees, third-party inputs, private example governance templates, and BuildTools subtrees that do not own public pages while retaining the source files referenced by the docs. Removing a pinned permalink creates a route that the catalog promises but Jekyll does not render and fails source-level plus post-build validation.

Ordinary `index.md` pages use Jekyll's canonical directory URL (`/path/`), while
their artifact remains `/path/index.html`. `BuildTools/docs_site.py` records the
directory form in navigation, search, locale reservations, and the route
catalog so canonical tags and published links agree with GitHub Pages.

Manifest document records with `visibility: internal` and generated artifacts
with the same visibility are source-only maintainer evidence. `_config.yml`
must exclude their canonical paths and any repository pointer paths from
GitHub Pages, while `BuildTools/docs_ai_delivery.py` must omit internal models
from the public AI bundle. `BuildTools/docs_site_artifact.py` fails the rendered
artifact when any internal Markdown, JSON, index, or rendered HTML variant is
present. Public documentation may point maintainers to these files through a
GitHub source URL, but it must not promise a same-domain route for them.

## Reader navigation and static search

The public site wraps normal Jekyll-rendered Markdown in `_layouts/default.html`. The layout adds a persistent desktop sidebar, mobile navigation, page-local table of contents, source link, code-copy controls, light/dark preference, a visible rolling `master` indicator, and an EN/RU switch for current locale pairs. Markdown remains complete and readable when opened directly in GitHub; the layout owns no technical prose.

`Docs/documentation-manifest.json` owns the navigation groups by stable document ID. `BuildTools/docs_site.py` resolves those IDs to current titles and paths and writes:

| Artifact | Contents |
|---|---|
| `_data/docs-site.json` | Site identity, repository/source ref, localized navigation groups, resolved public Markdown routes, and current stable-ID locale pairs |
| `assets/docs-search.json` | Compact weighted English token postings and result metadata for every public current English human document |
| `assets/docs-search.ru.json` | Compact weighted Russian token postings and result metadata for every current translated human document |
| `Docs/generated/document-routes.json` | Current public URLs, canonical future owners, planned English/Russian paths, route availability, and every required legacy redirect |

Every public current human top-level page must appear exactly once in navigation. Generated detail pages stay out of the sidebar but remain searchable behind their generated index pages. Internal plans, placeholders, and AI-only maintainer routes appear in neither reader surface.

Search uses repository-owned JavaScript and browser APIs only. There is no hosted index, account, analytics dependency, remote script, or server endpoint. The rendered page loads only its active locale index, and result routes remain in that locale. Titles and headings receive more weight than body terms; complete technical identifiers and camel-case components remain searchable. Pure numeric components and terms present in more than 60% of a locale corpus are omitted because they cannot usefully distinguish results. Compact JSON is emitted as UTF-8 instead of expanding non-ASCII text into `\\uXXXX` escapes, so the Russian budget measures actual text bytes. The source manifest enforces the reviewed 1.75 MiB (1,835,008-byte) hard limit independently for each generated index. The limit is capacity for the complete bilingual corpus, not permission to omit documents; generation still fails closed when either locale exceeds it.

Regenerate after changing public Markdown membership, titles, paths, lifecycle state, migration targets, version/localization policy, navigation groups, or search policy:

```bash
python BuildTools/docs_site.py --write
python BuildTools/docs_site.py --check
```

When visual, fenced, or delivery layers change, generate diagrams and the
screenshot catalog first, the snippet report second, translation status third,
site data fourth, the AI evaluation report fifth, and AI delivery last. The
evaluation consumes the site search model, while public `docs-manifest.json`
records diagram, screenshot, snippet, machine-model, evaluation, navigation,
search, and route-catalog hashes. Never edit generated delivery data by hand.
Presentation changes belong in the layout or local assets;
navigation/search/routing ownership changes belong in the source manifest.
The accepted boundaries are recorded in
[Documentation Snippet Validation](snippets.md), [ADR-0004](../decisions/0004-manifest-backed-site-navigation-search.md),
[ADR-0006](../decisions/0006-documentation-version-locale-routing.md),
[AI Documentation Evaluation](ai-evaluation.md), and
[Translation Workflow](translation.md).

## Version, locale, and route migration

The unversioned production site is the `current` channel. It follows rolling `master`, and the layout labels it `Current`; it does not claim that `master` is a stable release. Historical review uses repository revisions and commit-addressable `_site` artifacts.

Tagged release snapshots are deliberately deferred. The manifest reserves `/versions/{version}/`, but no release page is generated until the engine has immutable supported tags, a support matrix, and an approved follow-up decision. `VERSION`, a branch, or a reachable tag must not silently create a supported documentation line.

The localization policy is also source-owned:

- `en` is canonical; manifest-owned human pages use their current paths below `Docs/en/` or an explicit repository/subsystem README entrypoint;
- `ru` is a complete mirror; this revision has all 197 required counterparts under fail-closed parity enforcement;
- paths below `Docs/en/` mirror directly below `Docs/ru/`;
- root and subsystem README pages use explicit pairs such as `README.md` and `README.ru.md`;
- `BuildTools/docs_localization.py` computes normalized SHA-256 for every canonical English source and rejects stale or mismatched existing Russian pages;
- `Docs/generated/translation-status.json` currently reports exact required/current/missing coverage;
- stable-ID language switching and locale-scoped search are active for every required pair; manifest enforcement is `complete`.

`Docs/generated/document-routes.json` freezes the migration map before files move. Each public record carries its current route, planned canonical owner/path, locale paths, and redirect requirement. Multiple legacy pages may converge only when exactly one non-`replace` record owns the destination.

Former flat Markdown files remain durable pointers to their canonical pages, including links for old heading anchors. This preserves routes in both the GitHub repository UI and Jekyll without generated HTML or an additional redirect plugin. The generated route inventory rejects missing owners, route collisions, and stale pointer records.

## AI and machine-readable routes

The static site publishes three generated root endpoints, the route catalog, and the current AI evaluation report alongside normal Jekyll-rendered Markdown:

| Route | Purpose |
|---|---|
| `https://fonline.ru/llms.txt` | Concise map of every public current page, grouped by Diataxis kind, plus canonical generated JSON models |
| `https://fonline.ru/llms-full.txt` | UTF-8/LF full-context bundle of public current authored pages and generated reference indexes, capped at 2 MiB |
| `https://fonline.ru/docs-manifest.json` | Public stable IDs, locale, owner, lifecycle state, canonical/source/raw URLs, source provenance, byte size, and SHA-256 content hashes |
| `https://fonline.ru/Docs/generated/document-routes.json` | Current/planned paths, version and locale policy, canonical target ownership, and legacy redirect requirements |
| `https://fonline.ru/Docs/generated/ai-evaluation-report.json` | Deterministic task-set identity, retrieval ranks, current evidence checks, success rate, MRR, and failures |
| `https://fonline.ru/Docs/generated/snippets.json` | Every public/current/human fenced block, owning document/heading, parser harness, hash, template status, result, and normative coverage |
| `https://fonline.ru/Docs/generated/diagrams.json` | Owned diagram IDs, documents, dimensions, alt/caption text, source provenance, published SVG paths, and exact hashes |
| `https://fonline.ru/Docs/generated/screenshots.json` | Owned screenshot IDs, documents, dimensions, alt/caption text, capture environment/interactions, source provenance, recapture triggers, and exact hashes |

`Docs/documentation-manifest.json` owns membership and policy. `BuildTools/docs_ai_delivery.py` owns deterministic projection only; never edit the three outputs by hand. Every public document record includes a canonical human HTML URL at `fonline.ru` and a `source_ref`-pinned clean Markdown URL at GitHub's raw-content endpoint; `llms.txt` uses the Markdown route and retains an HTML link. The current GitHub Pages/Jekyll 3.10 pipeline cannot publish same-domain raw aliases without duplicating content or adding a custom plugin, so same-domain Markdown aliases remain a publication-platform task rather than a false route claim. Generated reference detail pages remain available as normal Markdown and JSON, but the full-context bundle carries their index pages rather than duplicating large method/type/property inventories.

Regenerate after any inventoried Markdown or manifest change:

```bash
python BuildTools/docs_ai_delivery.py --write
python BuildTools/docs_ai_delivery.py --check
```

When the search model or AI task set changes, run the complete dependency chain:

```bash
python BuildTools/docs_diagrams.py --write
python BuildTools/docs_screenshots.py --write
python BuildTools/docs_snippets.py --write --external
python BuildTools/docs_localization.py --write
python BuildTools/docs_site.py --write
python BuildTools/docs_ai_eval.py --write
python BuildTools/docs_ai_eval.py --check
python BuildTools/docs_ai_delivery.py --write
python BuildTools/docs_ai_delivery.py --check
```

The generator fails instead of truncating when the context bundle exceeds its declared byte budget. Content hashes use normalized LF text, so the output is byte-identical on Windows and Linux. These routes aid discovery and transport; they do not override the source precedence in the source manifest or promote an internal engine symbol to a public API.

### Verified public state

The following was checked on 2026-07-10, rechecked on 2026-07-15 and 2026-08-01 from public state, and verified through the authenticated GitHub Pages API on 2026-08-02:

- The Pages API reports status `built`, source mode `legacy`, branch `master`, and folder `/`. It reports `fonline.ru` as the custom domain and HTTPS enforcement as enabled.
- `https://fonline.ru/` and the legacy `/Docs/` index return HTTP 200. Representative generated `/Docs/en/` and `/Docs/ru/` routes return 404 because the documentation branch has not reached `master`.
- The repository `CNAME` contains `fonline.ru`. Public IPv4 DNS returns the four GitHub Pages addresses `185.199.108.153` through `185.199.111.153`, and `www.fonline.ru` is a CNAME for `fonline.ru`.
- The latest Pages build for remote `master` commit `fee50fb636b5bd1e30509aded929df1fc0e95db5` completed successfully. The accompanying legacy `validate` workflow failed in non-documentation build lanes and does not contain the pending documentation artifact/browser jobs.
- `_github-pages-challenge-cvet.fonline.ru` has no public TXT record. DNS routing is confirmed, but GitHub domain-ownership verification is recorded as `not-observed` rather than inferred from a working custom domain.

`Docs/documentation-manifest.json` records the verified Pages source and the separately unresolved ownership-verification state. Update its audit date and values only from authenticated settings/API evidence plus public DNS, never from the rendered page alone. Do not replace the existing production route or weaken HTTPS while landing the documentation corpus.

DNS account names, registrar credentials, recovery codes, and GitHub credentials do not belong in this public repository. Operational ownership is shared by the repository administrators and the private domain-administration owner. Record the named account owner in the private operations inventory; this repository owns only the public domain contract and its validation.

## Compatible environment

The local environment is pinned to the GitHub Pages dependency set declared in:

- `.ruby-version` for the Ruby runtime;
- `Gemfile` for the `github-pages` bundle;
- `BuildTools/docs-browser/package.json` and `package-lock.json` for Node, Playwright, Chromium revision, and axe-core;
- `Docs/documentation-manifest.json` for the machine-readable publication contract.

The current render pin is Ruby `3.3.4` and `github-pages` `232`. Browser validation uses Node `24.16.0`, Playwright `1.62.0` with its Chromium `151.0.7922.34` revision, and axe-core `4.12.1`. Before changing a render value, compare it with the [published GitHub Pages dependency versions](https://pages.github.com/versions/) and update the manifest declarations together. Browser dependency updates require an exact lock-file change, both focused tests, and a complete route audit; never use `latest` in CI.

`Gemfile.lock` is intentionally ignored, following GitHub's Pages guidance. The exact `github-pages` pin is the repository-level compatibility boundary; the official Pages build image is the CI authority.

## Local build and preview

Install the Ruby version from `.ruby-version` and Bundler, then run from the engine root:

On Windows, use the RubyInstaller package with Devkit/MSYS2 support. The GitHub Pages bundle contains native gems; a bare Ruby archive can resolve the bundle but cannot compile those dependencies.

```bash
python BuildTools/docs_diagrams.py --check
python BuildTools/docs_screenshots.py --check
python BuildTools/docs_site.py --check
bundle install
bundle exec jekyll build --trace
python BuildTools/docs_site_artifact.py --site-dir _site
npm ci --prefix BuildTools/docs-browser
npx --prefix BuildTools/docs-browser playwright install chromium
npm --prefix BuildTools/docs-browser run audit
```

The rendered output is `_site/`. It is disposable and ignored by git. A successful build must finish without Jekyll or Liquid errors; `_config.yml` enables strict front-matter parsing. Browser validation serves `_site/` only on an ephemeral `127.0.0.1` port and writes `Workspace/docs-browser-audit-report.json` plus the retained screenshot set under `Workspace/docs-browser-screenshots/`. It never contacts production or executes documentation snippets.

For an interactive preview:

```bash
bundle exec jekyll serve --livereload --host 127.0.0.1 --port 4000
```

Open `http://127.0.0.1:4000/`. Stop the server before switching branches or changing the Ruby/Jekyll dependency pin. GitHub's maintained procedure is [Testing your GitHub Pages site locally with Jekyll](https://docs.github.com/en/pages/setting-up-a-github-pages-site-with-jekyll/testing-your-github-pages-site-locally-with-jekyll).

## Pull-request artifact

The `Build documentation site` job in `.github/workflows/validate.yml` runs after the fast standalone documentation checks. It:

1. checks out the exact pull-request or push revision;
2. renders the repository root with `actions/jekyll-build-pages@v1` into `_site/`;
3. runs `BuildTools/docs_site_artifact.py` against the completed tree;
4. installs the manifest-pinned Node, npm lock, Chromium revision, and required Linux browser libraries;
5. runs `BuildTools/docs-browser/audit.mjs` against the same completed tree;
6. uploads separate static-validation and browser-validation artifacts, including the browser JSON and screenshots;
7. uploads `documentation-site-<commit-sha>` with `actions/upload-artifact@v4` for 14 days.

The static post-build gate requires every current route and every available locale route from `Docs/generated/document-routes.json`, verifies that static AI/search/generated-model endpoints are copied byte-for-byte, and checks canonical URLs, HTML language, one `h1`, skip/main landmarks, accessible image/button names, unique IDs, search targets, and links to publishable local resources.

The browser gate visits every catalog route in three manifest-owned profiles: desktop at 1440 x 1000 CSS pixels, mobile at 390 x 844 CSS pixels, and `zoom-200` at 640 x 512 CSS pixels with device scale factor 2. The last profile models a 1280 x 1024 physical viewport at 200 percent browser zoom and requires the compact navigation/reflow contract without a mobile user agent. Each rendered page must have no selected WCAG 2.2 A/AA axe violation, page/console/request error, undecoded image, clipped reading column, fixed-header/sidebar overlap, or reachable page-level horizontal scroll. Interaction scenarios additionally prove keyboard skip navigation, English technical/prose search, native-dialog Escape, theme persistence, copy feedback, compact/mobile drawer semantics, focus containment and restoration, Russian search, `html lang`, active-locale state, all explicit README language pairs, exact paired-route switching, architecture/content-showcase rendering, and Russian 200-percent reflow. The retained screenshots include responsive document/navigation states, every Russian README entry point, translated documents, architecture and Content Showcase media for each profile, and `zoom-200-russian-documentation.png` at an exact 1280 x 1024 physical size without changing the Pages deployment path.

Automated axe results cover only machine-detectable criteria. Raw incomplete nodes remain rule-level records with node/route counts and bounded example paths/targets instead of being mislabeled as passes. Axe can return `color-contrast` as incomplete when text is clipped inside a scroll container; for those exact nodes, the harness computes the effective foreground/background with alpha composition, applies the WCAG relative-luminance formula, and requires 4.5:1 or 3:1 for large text. Any failed or unresolvable fallback fails its route; both raw and resolved counts stay in the report. The `zoom-200` profile is deterministic reflow, accessibility, and screenshot evidence for one viewport; it is not proof for every browser, operating-system magnifier, font override, or production rendering difference. A green job still does not prove screen-reader behavior, cognitive accessibility, content clarity, or production equivalence. Release review needs keyboard-only and representative assistive-technology checks on the landed artifact and production domain.

This job validates and previews the production-compatible render. It does not deploy, alter Pages settings, write a branch, or change DNS. The existing GitHub Pages source remains the only production publication route.

Reviewers should inspect every retained screenshot, including the dedicated Russian 200-percent reflow image, then inspect at least the landing page and changed pages interactively. Check the rolling version indicator, locale switch, page table of contents, code blocks, tables, local assets, keyboard order, and one screen-reader landmark/headings pass. Repeat 200-percent zoom on the landed artifact or production domain before release; the local profile does not replace that environment check. A green source, artifact, or axe check does not by itself prove that the rendered page is readable by every user.

## Production verification

After a documentation change reaches the configured Pages source:

1. Confirm `Validate documentation` and `Build documentation site`, including the browser audit, are green for the published commit.
2. Inspect both validation reports, every browser screenshot, and the commit-addressable site artifact before attributing a production difference to Pages.
3. Open `https://fonline.ru/` and the English, Russian, and legacy routes for at least one migrated document.
4. Confirm sidebar navigation highlights the current page and remains usable at a mobile width.
5. Search for `Game.Sync` and a prose topic such as `map baking` in English, then `игровой клиент` in Russian; confirm results stay on canonical routes in the active locale.
6. Confirm HTTPS is valid and the browser remains on `fonline.ru`.
7. Confirm `CNAME` still contains exactly `fonline.ru`.
8. Resolve the domain and confirm it still targets GitHub Pages.

PowerShell DNS check:

```powershell
Resolve-DnsName fonline.ru -Type A
```

POSIX DNS check:

```bash
dig +short fonline.ru A
```

Escalate a mismatch to both repository and domain administrators. Do not work around a Pages or DNS problem by committing generated HTML, changing the canonical domain, or adding an alternate deployment workflow.

## Validation checklist

1. Markdown remains the canonical authored content.
2. `CNAME`, `_config.yml`, `Gemfile`, `.ruby-version`, the manifest, and CI agree on the publication contract.
3. `python BuildTools/tests/test_docs_validate.py` passes.
4. `python BuildTools/tests/test_docs_snippets.py` and `python BuildTools/docs_snippets.py --check --external` pass without executing commands.
5. `python BuildTools/tests/test_docs_diagrams.py`,
   `python BuildTools/docs_diagrams.py --check`,
   `python BuildTools/tests/test_docs_screenshots.py`,
   `python BuildTools/docs_screenshots.py --check`,
   `python BuildTools/tests/test_docs_site.py`,
   `python BuildTools/tests/test_docs_site_layout.py`, and
   `python BuildTools/docs_site.py --check` pass, including visual
   ownership/freshness and version/locale/route collision coverage.
6. `python BuildTools/tests/test_docs_ai_eval.py` and `python BuildTools/docs_ai_eval.py --check` pass against that search model.
7. `python BuildTools/tests/test_docs_ai_delivery.py` and `python BuildTools/docs_ai_delivery.py --check` pass.
8. `python BuildTools/tests/test_docs_localization.py` and `python BuildTools/docs_localization.py --check` pass; production bilingual launch also uses `--enforce-complete`.
9. `python BuildTools/docs_validate.py` passes.
10. A local or CI Jekyll build produces `_site/` without errors and contains the AI routes, evaluation/snippet reports, generated navigation, both locale search indexes, route catalog, layout, and local assets.
11. `python BuildTools/tests/test_docs_site_artifact.py` and `python BuildTools/docs_site_artifact.py --site-dir _site` pass against that rendered tree.
12. `python BuildTools/tests/test_docs_browser.py` passes, the pinned npm dependencies are installed from the lock file, and `npm --prefix BuildTools/docs-browser run audit` passes every route in all three manifest profiles plus every interaction scenario.
13. Desktop, mobile, and 200-percent reflow navigation, search, table of contents, source link, theme preference, page overflow, focus containment, and screenshots are exercised in the rendered artifact.
14. The dedicated Russian 200-percent screenshot is reviewed; manual keyboard, production-domain 200-percent zoom, and representative screen-reader checks are recorded for a release candidate; axe incomplete nodes are reviewed rather than silently ignored.
15. The site, static report, browser report, and screenshot artifacts are attached to the exact reviewed commit.
16. Production source settings and DNS were not changed unless the change explicitly required administrator review.
