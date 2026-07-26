# ADR-0001: GitHub Pages Markdown Publication and Locale Layout

- Status: Accepted
- Date: 2026-07-10
- Owners: documentation, build-release

## Context

FOnline already publishes its repository site through GitHub Pages, uses Jekyll configuration from `_config.yml`, and binds the production domain through root `CNAME = fonline.ru`. The documentation source is Markdown in the engine repository and must remain readable both in the GitHub repository UI and on the public site.

The production documentation program also requires:

- standalone engine documentation with no embedding-project filesystem dependency;
- English canonical content and a complete Russian mirror for human-facing pages;
- stable URLs, redirects, navigation, search, and machine-readable indexes;
- pull-request validation that exercises the same constraints as production;
- no second content tree whose generated output can drift from repository Markdown.

Introducing a separate site application would duplicate ownership and move the publication contract away from the system already serving `fonline.ru`.

## Decision

1. GitHub Pages remains the production publisher and Jekyll remains the renderer.
2. Markdown committed to this repository is the canonical human-documentation source.
3. Root `_config.yml` and `CNAME` remain part of the tested publication contract. `Docs/documentation-manifest.json` records provider, generator, source format, domain, and owning paths.
4. Do not introduce Docusaurus, a parallel `website/` content tree, or checked-in generated HTML.
5. Jekyll layouts, includes, data files, supported plugins, theme overrides, and static assets may provide presentation and navigation, but must remain a thin rendering layer over Markdown.
6. The target public locale layout is:

   ```text
   Docs/
     en/       # canonical English human docs
     ru/       # Russian mirror with identical relative paths and stable document IDs
     assets/   # shared published media, styles, and search assets
     _meta/    # internal plans/reports, excluded from public navigation
   ```

7. Public pages use stable IDs and matching relative paths across locales. A language switch resolves by document ID/path rather than title text.
8. English is canonical for source synchronization because engine identifiers, source comments, symbols, and upstream collaboration are English. Russian pages are whole-document mirrors, not mixed-language fragments.
9. Translation freshness is tracked from a canonical-content hash. Production publication must not present a stale Russian page as current.
10. Existing public URLs receive GitHub Pages-compatible redirects or durable Markdown route pages before source files move.
11. The existing Pages source branch/folder remains unchanged by documentation restructuring. Repository administrators must verify and record that setting plus DNS ownership before a production migration.
12. Pull requests run fast Markdown/manifest/link checks and, in the site phase, a GitHub Pages-compatible Jekyll build that uploads `_site` as a review artifact. Production still deploys through the existing Pages route.

## Consequences

### Positive

- GitHub and `fonline.ru` render the same authored files.
- Documentation remains portable and useful without Node or a client-side application.
- AI systems can consume clean Markdown and generated JSON directly.
- The custom domain and publishing stack are reviewable in normal repository diffs.
- Locale parity can be enforced by stable paths/IDs without a framework-specific translation registry.

### Costs

- Navigation, static search, language switching, and version indicators must be implemented within GitHub Pages-supported Jekyll capabilities.
- Moving the current flat English tree requires redirect planning before `Docs/en/` becomes canonical.
- GitHub Pages provides one production site; pull-request previews are build artifacts unless a separate approved preview environment is added later.
- Translation parity adds release work after the English information architecture freezes.

## Rejected alternatives

- **Docusaurus or another separate site application:** rejected because it creates a second framework/content contract and is not the existing production route.
- **Checked-in generated HTML:** rejected because generated output would compete with Markdown as source of truth.
- **Sibling `Docs.EN` / `Docs.RU` roots:** rejected in favor of conventional lowercase locale directories under one documentation root.
- **Mixed English/Russian pages:** rejected because they weaken routing, search, translation freshness, and machine retrieval.
- **Immediate version snapshots:** deferred until the engine has release tags and an explicit support policy.

## Verification

- `BuildTools/docs_validate.py` checks the manifest publication values, `_config.yml`, and `CNAME`/domain agreement.
- The `Validate documentation` GitHub Actions job runs without an embedding project or native build.
- Future Jekyll work must keep standalone GitHub Markdown rendering functional.

## Related documents

- [ADR-0006](0006-documentation-version-locale-routing.md)
- [ProductionDocumentationPlan.md](../ProductionDocumentationPlan.md)
- [DocumentationMaintenance.md](../DocumentationMaintenance.md)
- [documentation-manifest.json](../documentation-manifest.json)

