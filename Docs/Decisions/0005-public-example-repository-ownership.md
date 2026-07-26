# ADR 0005: Public Example Repository Ownership

- Status: accepted
- Date: 2026-07-15
- Owners: documentation, build-release, runtime, content, repository administrators

## Context

FOnline needs runnable public examples that are smaller and more teachable than production games. A single large sample would mix bootstrap, gameplay, assets, native integration, releases, and project policy. Unpinned examples would drift with Engine `master`; copied Last Frontier or TLA material would make reusable documentation depend on another project's code and licenses.

The Engine already owns an executable headless scaffold in `Examples/MinimalProject`, generated contract references, a standalone documentation site, and CI smoke routes. It did not yet own a machine-readable external-repository portfolio, common governance files, exact pin validation, scheduled compatibility policy, asset provenance contract, or publication authority boundary.

## Decision

1. Maintain four separately scoped repositories: `fonline-project-template`, `fonline-minimal-multiplayer`, `fonline-content-showcase`, and `fonline-native-extension-sample`.
2. Keep the authoritative portfolio in `Examples/PublicRepositories.json`; generate the public JSON/Markdown projection with `BuildTools/docs_examples.py`.
3. Keep shared governance and workflow templates in `Examples/PublicRepositoryTemplate`. Apply and fully render that overlay before publishing any repository.
4. The Engine repository owns reusable policy, validation, and canonical source scaffolds. Each external repository owns its example code, assets, issues, tags, and artifacts. Engine source/tests/docs remain normative when an example disagrees.
5. Release and tutorial builds pin `Engine/` to an exact commit in both the gitlink and `example-repository.json`. Floating Engine refs are forbidden for release artifacts.
6. Every repository runs a protected pinned-Engine lane and a weekly current-Engine compatibility lane. Current-Engine results lead to reviewed update pull requests and never mutate released pins automatically.
7. Every redistributed asset has machine-readable source, license, path, and SHA-256 provenance. Asset-free examples still carry an empty valid provenance file.
8. Repository creation, access, security settings, visibility, Pages, and release publication are owner-gated administrator operations. Preparing local source does not authorize a push or publication.
9. Public documentation links only to repositories and tags whose registry status is `published` and whose exit gate has been verified.
10. Native client examples treat updater protocol generation and client host/runtime ABI as explicit release boundaries. An incompatible frozen host requires a full client package, not an in-process reload workaround.

## Consequences

- Developers and AI agents can distinguish normative Engine contracts from illustrative project composition.
- Tags, artifacts, logs, screenshots, and tutorials are reproducible from an exact Engine commit.
- Scheduled compatibility failures become visible before documentation or releases move their pin.
- Shared security, contribution, support, CI, and provenance files stay consistent without importing a production game's workflow.
- Publishing the first external repository requires administrator action and successful CI; this ADR deliberately does not create or push that repository.
- Four repositories create some maintenance overhead, but each remains small enough to have one responsibility and an explicit exit gate.

## Rejected alternatives

### One comprehensive sample game

Rejected because it would accumulate unrelated systems, obscure first-run paths, and become another production project.

### Build releases from Engine `master`

Rejected because generated contracts, native compatibility, artifacts, and tutorials would not be reproducible.

### Treat Last Frontier or TLA as the canonical example

Rejected because both contain project-specific policy, assets, services, and refactoring constraints that cannot define a standalone reusable Engine contract.

### Copy workflow and policy files independently into each repository

Rejected because silent drift would make security, compatibility, and support claims inconsistent. The Engine-owned overlay and validator are the shared source.

## Verification

- `python BuildTools/tests/test_docs_examples.py`
- `python BuildTools/docs_examples.py --check`
- `python BuildTools/docs_validate.py`
- both compatibility lanes in every repository whose status is `published`
