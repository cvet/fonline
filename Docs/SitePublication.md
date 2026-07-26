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
- `BuildTools/docs_site.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `_layouts/default.html`
- `assets/css/docs.css`
- `assets/js/docs.js`
- `assets/images/fonline-mark.png`
- `_data/docs-site.json`
- `assets/docs-search.json`
- `Docs/generated/document-routes.json`
- `llms.txt`
- `llms-full.txt`
- `docs-manifest.json`
- `Docs/Decisions/0001-github-pages-markdown-publication.md`
- `Docs/Decisions/0003-manifest-backed-ai-documentation-delivery.md`
- `Docs/Decisions/0004-manifest-backed-site-navigation-search.md`
- `Docs/Decisions/0006-documentation-version-locale-routing.md`

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
| Static search | Generated `assets/docs-search.json`, queried entirely in the browser |
| Version, locale, and route map | Generated `Docs/generated/document-routes.json`, derived from stable document IDs and manifest targets |
| Review output | Commit-addressable `_site` artifact from GitHub Actions |
| AI delivery | Root `llms.txt`, bounded `llms-full.txt`, and `docs-manifest.json`, generated from the source manifest and Markdown |

The publication route is intentionally independent from any embedding game project. Last Frontier, TLA, and public example games may link to this site, but they neither build nor define it.

## Reader navigation and static search

The public site wraps normal Jekyll-rendered Markdown in `_layouts/default.html`. The layout adds a persistent desktop sidebar, mobile navigation, page-local table of contents, source link, code-copy controls, light/dark preference, and a visible rolling `master` indicator. Markdown remains complete and readable when opened directly in GitHub; the layout owns no technical prose.

`Docs/documentation-manifest.json` owns the navigation groups by stable document ID. `BuildTools/docs_site.py` resolves those IDs to current titles and paths and writes:

| Artifact | Contents |
|---|---|
| `_data/docs-site.json` | Site identity, repository/source ref, ordered navigation groups, and resolved public Markdown routes |
| `assets/docs-search.json` | Compact weighted token postings and result metadata for every public current human document |
| `Docs/generated/document-routes.json` | Current public URLs, canonical future owners, planned English/Russian paths, route availability, and every required legacy redirect |

Every public current human top-level page must appear exactly once in navigation. Generated detail pages stay out of the sidebar but remain searchable behind their generated index pages. Internal plans, placeholders, and AI-only maintainer routes appear in neither reader surface.

Search uses repository-owned JavaScript and browser APIs only. There is no hosted index, account, analytics dependency, remote script, or server endpoint. Titles and headings receive more weight than body terms; complete technical identifiers and camel-case components remain searchable. The source manifest enforces a 1 MiB hard limit for the generated index.

Regenerate after changing public Markdown membership, titles, paths, lifecycle state, migration targets, version/localization policy, navigation groups, or search policy:

```bash
python BuildTools/docs_site.py --write
python BuildTools/docs_site.py --check
```

When both delivery layers change, generate site data first and AI delivery second: public `docs-manifest.json` records the site navigation, search, and route-catalog hashes. Never edit generated site data by hand. Presentation changes belong in the layout or local assets; navigation/search/routing ownership changes belong in the source manifest. The accepted boundaries are recorded in [ADR-0004](Decisions/0004-manifest-backed-site-navigation-search.md) and [ADR-0006](Decisions/0006-documentation-version-locale-routing.md).

## Version, locale, and route migration

The unversioned production site is the `current` channel. It follows rolling `master`, and the layout labels it `Current`; it does not claim that `master` is a stable release. Historical review uses repository revisions and commit-addressable `_site` artifacts.

Tagged release snapshots are deliberately deferred. The manifest reserves `/versions/{version}/`, but no release page is generated until the engine has immutable supported tags, a support matrix, and an approved follow-up decision. `VERSION`, a branch, or a reachable tag must not silently create a supported documentation line.

The localization policy is also source-owned:

- `en` is canonical and is still authored in the current flat paths;
- `ru` is planned and has one reserved mirror path for every translation-scoped page;
- paths below `Docs/en/` mirror directly below `Docs/ru/`;
- root and subsystem README pages use explicit pairs such as `README.md` and `README.ru.md`;
- normalized SHA-256 is the future translation-freshness input;
- translation parity and language switching remain pending until the Russian files exist.

`Docs/generated/document-routes.json` freezes the migration map before files move. Each public record carries its current route, planned canonical owner/path, locale paths, and redirect requirement. Multiple legacy pages may converge only when exactly one non-`replace` record owns the destination.

During migration, keep the old Markdown file as a durable pointer to the new canonical page. This preserves the route in both the GitHub repository UI and Jekyll without adding generated HTML or relying on an additional redirect plugin. Planned paths in the catalog are reservations, not claims that those pages are already published.

## AI and machine-readable routes

The static site publishes three generated root endpoints plus the generated route catalog alongside normal Jekyll-rendered Markdown:

| Route | Purpose |
|---|---|
| `https://fonline.ru/llms.txt` | Concise map of every public current page, grouped by Diataxis kind, plus canonical generated JSON models |
| `https://fonline.ru/llms-full.txt` | UTF-8/LF full-context bundle of public current authored pages and generated reference indexes, capped at 1.25 MiB |
| `https://fonline.ru/docs-manifest.json` | Public stable IDs, locale, owner, lifecycle state, canonical/source/raw URLs, source provenance, byte size, and SHA-256 content hashes |
| `https://fonline.ru/Docs/generated/document-routes.json` | Current/planned paths, version and locale policy, canonical target ownership, and legacy redirect requirements |

`Docs/documentation-manifest.json` owns membership and policy. `BuildTools/docs_ai_delivery.py` owns deterministic projection only; never edit the three outputs by hand. Generated reference detail pages remain available as normal Markdown and JSON, but the full-context bundle carries their index pages rather than duplicating large method/type/property inventories.

Regenerate after any inventoried Markdown or manifest change:

```bash
python BuildTools/docs_ai_delivery.py --write
python BuildTools/docs_ai_delivery.py --check
```

The generator fails instead of truncating when the context bundle exceeds its declared byte budget. Content hashes use normalized LF text, so the output is byte-identical on Windows and Linux. These routes aid discovery and transport; they do not override the source precedence in the source manifest or promote an internal engine symbol to a public API.

### Verified public state

The following was checked on 2026-07-10 and rechecked on 2026-07-15 before the navigation/search implementation:

- `https://fonline.ru` served the repository landing page through the Slate Jekyll theme.
- The repository `CNAME` contained `fonline.ru`.
- Public IPv4 DNS resolution returned the four GitHub Pages addresses `185.199.108.153` through `185.199.111.153`.
- Public evidence was consistent with publication from the repository root, but it did not prove the selected branch and folder in repository settings.
- The live page still used the basic Slate repository presentation and did not yet expose the generated navigation/search shell; production verification must wait for the implementation to reach the configured Pages source.

The exact **Settings > Pages > Build and deployment** source mode, branch, and folder are therefore recorded as `pending-admin-verification` in the documentation manifest. A repository administrator must update that record after inspecting the setting. Do not replace the existing production route based on inference from the live output.

DNS account names, registrar credentials, recovery codes, and GitHub credentials do not belong in this public repository. Operational ownership is shared by the repository administrators and the private domain-administration owner. Record the named account owner in the private operations inventory; this repository owns only the public domain contract and its validation.

## Compatible environment

The local environment is pinned to the GitHub Pages dependency set declared in:

- `.ruby-version` for the Ruby runtime;
- `Gemfile` for the `github-pages` bundle;
- `Docs/documentation-manifest.json` for the machine-readable publication contract.

The current pin is Ruby `3.3.4` and `github-pages` `232`. Before changing either value, compare it with the [published GitHub Pages dependency versions](https://pages.github.com/versions/), update all three declarations together, and run both the fast documentation gate and a rendered build.

`Gemfile.lock` is intentionally ignored, following GitHub's Pages guidance. The exact `github-pages` pin is the repository-level compatibility boundary; the official Pages build image is the CI authority.

## Local build and preview

Install the Ruby version from `.ruby-version` and Bundler, then run from the engine root:

On Windows, use the RubyInstaller package with Devkit/MSYS2 support. The GitHub Pages bundle contains native gems; a bare Ruby archive can resolve the bundle but cannot compile those dependencies.

```bash
python BuildTools/docs_site.py --check
bundle install
bundle exec jekyll build --trace
```

The rendered output is `_site/`. It is disposable and ignored by git. A successful build must finish without Jekyll or Liquid errors; `_config.yml` enables strict front-matter parsing.

For an interactive preview:

```bash
bundle exec jekyll serve --livereload --host 127.0.0.1 --port 4000
```

Open `http://127.0.0.1:4000/`. Stop the server before switching branches or changing the Ruby/Jekyll dependency pin. GitHub's maintained procedure is [Testing your GitHub Pages site locally with Jekyll](https://docs.github.com/en/pages/setting-up-a-github-pages-site-with-jekyll/testing-your-github-pages-site-locally-with-jekyll).

## Pull-request artifact

The `Build documentation site` job in `.github/workflows/validate.yml` runs after the fast standalone documentation checks. It:

1. checks out the exact pull-request or push revision;
2. renders the repository root with `actions/jekyll-build-pages@v1` into `_site/`;
3. uploads `documentation-site-<commit-sha>` with `actions/upload-artifact@v4` for 14 days.

This job validates and previews the production-compatible render. It does not deploy, alter Pages settings, write a branch, or change DNS. The existing GitHub Pages source remains the only production publication route.

Reviewers should inspect at least the landing page, changed pages, desktop and mobile navigation, search results for one prose term and one technical identifier, the rolling version indicator, page table of contents, code blocks, tables, and local assets in the artifact. A green source-level link check does not by itself prove that the rendered page is readable.

## Production verification

After a documentation change reaches the configured Pages source:

1. Confirm `Validate documentation` and `Build documentation site` are green for the published commit.
2. Inspect the commit-addressable artifact before attributing a production difference to Pages.
3. Open `https://fonline.ru/` and at least one changed Markdown route.
4. Confirm sidebar navigation highlights the current page and remains usable at a mobile width.
5. Search for `Game.Sync` and a prose topic such as `map baking`; confirm results stay on canonical Markdown routes.
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
4. `python BuildTools/tests/test_docs_ai_delivery.py` and `python BuildTools/docs_ai_delivery.py --check` pass.
5. `python BuildTools/tests/test_docs_site.py`, `python BuildTools/tests/test_docs_site_layout.py`, and `python BuildTools/docs_site.py --check` pass, including version/locale/route collision and redirect coverage.
6. `python BuildTools/docs_validate.py` passes.
7. A local or CI Jekyll build produces `_site/` without errors and contains the AI routes, generated navigation, search index, route catalog, layout, and local assets.
8. Desktop/mobile navigation, search, table of contents, source link, and theme preference are exercised in the rendered artifact.
9. The pull-request artifact is attached to the exact reviewed commit.
10. Production source settings and DNS were not changed unless the change explicitly required administrator review.
