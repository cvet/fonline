# Public Example Repositories

This page defines how FOnline's public example repositories are created, reviewed, released, and kept compatible. The machine source is [Examples/PublicRepositories.json](../Examples/PublicRepositories.json); the checked projection is [the generated registry](generated/public-examples/index.md).

## Authority boundary

Public examples are executable teaching material. They demonstrate one supported composition at one exact Engine revision, but they do not define Engine behavior.

Use this source order when an example and the Engine disagree:

1. Engine source and tests;
2. the owning Engine documentation page and generated contract model;
3. the example repository at its recorded tag and Engine gitlink;
4. embedding-project documentation and historical examples.

Fix a reusable defect in `cvet/fonline` first. Fix example-only code, presentation, tutorials, and assets in the example repository. Do not make Engine documentation depend on unpublished branches, Last Frontier, TLA, or an example's project policy.

## Portfolio

The program intentionally uses several small repositories instead of one sample game.

| Order | Repository | Responsibility | Current state |
| ---: | --- | --- | --- |
| 1 | `cvet/fonline-project-template` | Canonical project bootstrap and first successful headless smoke | Source-ready and pushed to a private staging repository; not published |
| 2 | `cvet/fonline-minimal-multiplayer` | Small playable server/client/content/tutorial slice | Private repository reserved; source remains planned after the project template is tagged |
| 3 | `cvet/fonline-content-showcase` | Rendering, mapper, asset, capture, and Web presentation | Private repository reserved; source remains planned after the minimal multiplayer source and asset policies are proven |
| 4 | `cvet/fonline-native-extension-sample` | Advanced project-native C++ hook/export/test path | Private repository reserved; source remains planned after the template and public extension contracts are stable |

Remote existence, source readiness, and public release are separate states. `remote.visibility = private` records an administrator-created staging repository without exposing it as documentation; `remote.state = reserved` means only the repository shell exists, while `source-staged` means reviewed candidate source has been pushed. Do not publish links for a repository while its registry status is `planned`, `blocked`, or `source-ready`. Change both lifecycle and remote state to `published`, and visibility to `public`, only after its protected default branch, security configuration, required checks, first tag, and public artifact have been verified.

## Ownership

| Area | Accountable owner | Required review |
| --- | --- | --- |
| Portfolio, tutorials, links, and generated registry | Documentation maintainers | Owning technical maintainer |
| Template, CI, pins, artifacts, and tags | Build and release maintainers | Documentation plus affected platform owner |
| Reusable runtime and native behavior | Engine runtime maintainers | Contract owner named by the Engine source/docs |
| Showcase assets, captures, and provenance | Content and asset maintainers | License/provenance review |
| Advisories, branch protection, and repository access | Repository administrators | Security owner |

An external repository owns its code, releases, and issue tracker. The Engine repository owns the shared policy, validator, governance overlay, and canonical source scaffold. Repository administrators remain the only owners authorized to create repositories, change visibility, grant access, configure secrets, enable Pages, or publish releases.

## Required repository contract

Every public example root contains:

- `example-repository.json` with its stable program ID, canonical repository name, Engine clone URL, exact 40-character Engine commit, `Engine/` submodule path, primary check, and provenance path;
- `Engine/` committed as a git submodule at that exact revision;
- the common `LICENSE`, `CONTRIBUTING.md`, `SECURITY.md`, `SUPPORT.md`, `THIRD_PARTY_NOTICES.md`, CODEOWNERS, pull-request template, and two compatibility workflows;
- `assets/provenance.json`, including an entry for every redistributed visual, audio, model, font, or other non-code asset;
- a short task-oriented README that identifies the Engine revision and links deep reusable contracts to `fonline.ru`;
- one repository-specific command that proves its primary behavior without administrator services or private credentials.

Validate a candidate checkout from its root:

```bash
python Engine/BuildTools/docs_examples.py --verify-repository . --engine-mode pinned
```

The validator rejects unresolved publication placeholders, unknown repository IDs, non-exact revisions, metadata/registry drift, a non-submodule `Engine/`, gitlink/checkout mismatch, missing governance files, and incomplete asset provenance.

## Creating a repository

The publication owner follows this sequence:

1. Confirm the registry entry's dependencies and exit gate. Record the source Engine commit and candidate repository owner.
2. Start from the approved source tree. For `fonline-project-template`, use `Examples/MinimalProject`; do not copy a production game.
3. Apply the file mapping from `program.publication.copy_files` in `Examples/PublicRepositories.json` using `Examples/PublicRepositoryTemplate` as the source.
4. Replace every recorded `{{PLACEHOLDER}}`. The published tree must contain no template token.
5. Add `Engine/` from `https://github.com/cvet/fonline.git`, checkout the exact reviewed commit, and commit the gitlink. Record the same lowercase commit in `example-repository.json`.
6. Fill `assets/provenance.json`, even when its initial asset list is empty. Add no asset whose source, license, and digest cannot be verified.
7. Run the repository validator and the primary check locally. Then push to a private or draft repository for the first pinned/current CI runs.
8. Enable GitHub Security Advisories, require CODEOWNERS review, protect `main`, require the registry's checks, and prevent direct release pushes.
9. Review the README and generated documentation links from a clean clone. Publish the repository, then create the first immutable tag and artifact from the same commit.
10. Change the Engine registry status and remote state to `published`, record public visibility, add only the verified public/tag links, regenerate documentation, and rerun site/AI validation.

Creating the remote repository and changing its settings are owner-gated operations. A local documentation change never implies authorization to create, push, publish, or transfer a GitHub repository.

## Engine revisions and compatibility

Release artifacts and tutorial tags build against the exact Engine gitlink recorded in `example-repository.json`. They never fetch a floating default branch. This makes logs, generated metadata, screenshots, package contents, and support claims reproducible.

Each repository also runs a weekly `current-engine` lane against Engine `master`. That lane answers whether an update is ready; it does not mutate the release pin. A green result may produce a reviewed Engine-update pull request. A failure remains visible with the tested Engine commit and is assigned to the owner of the changed boundary.

Review all three compatibility boundaries during an update:

- gameplay compatibility version, including network and serialized contract changes;
- updater protocol generation;
- native client host/runtime ABI.

Updater protocol generation 2 and client host/runtime ABI 3 reject older unsafe native clients before they can transfer or load a new runtime. An example distributing a native client across this boundary must publish a new full client package and state that generation-1/ABI-2 installations require one manual reinstall. Do not present an in-process runtime reload or a floating client package as a compatibility path. See [Client Updater](ClientUpdater.md).

## CI lanes

`pinned-engine` runs on every pull request and protected-branch update. It checks repository governance, gitlink/metadata identity, the primary example behavior, and all repository-specific platform gates. The project template requires Windows and Linux smoke jobs.

Linux jobs prepare host dependencies through the checked-out revision's `Engine/BuildTools/prepare-workspace.sh linux-packages linux` command. Do not copy an apt package list into example workflows: the Engine-owned command is the versioned platform contract and keeps pinned and compatibility lanes on the same prerequisites.

`current-engine` runs weekly and on demand. It temporarily checks out Engine `master`, records the tested commit, and runs the same primary behavior. It must not rewrite `example-repository.json`, the gitlink, generated files, tags, or release artifacts.

Additional gates come from the registry entry:

- tutorial repositories replay every lesson tag or fixture;
- the content showcase validates provenance, performance budgets, and capture reproduction;
- the native extension sample runs focused native tests and the generated extension contract checks.

Shared workflow changes are made in the Engine overlay first, validated there, then rolled out through reviewed pull requests. Do not copy an embedding project's CI names, credentials, deployments, or private services into the shared template.

## Releases and tutorials

Use immutable tags for lesson checkpoints and published examples. Do not maintain divergent tutorial branches. A tutorial names the repository, tag, file, and Engine revision it was tested against.

A release artifact includes or exposes:

- example repository commit and tag;
- exact Engine revision;
- host/target and build configuration;
- primary-check result;
- asset provenance digest when assets are present;
- known limitations and support status.

If a security or compatibility correction invalidates an old tutorial tag, keep the tag immutable and publish a replacement notice or corrected tag. Do not silently rewrite teaching history.

## Asset provenance

Every redistributed asset entry records a stable ID, repository-relative path, SPDX-style allowed license identifier, source URL or `project-original`, and lowercase SHA-256 digest. The validator rejects missing files, duplicate identities, unknown licenses, non-HTTPS external sources, and malformed digests.

The template and native-extension sample should remain asset-free. The multiplayer example should prefer project-original or permissively licensed assets. The showcase accepts only audited public-domain, CC0, CC-BY, MIT-compatible, or project-original material whose redistribution and modification rights are explicit.

Screenshots and generated captures are release artifacts derived from a tagged build. Record the source tag, Engine revision, backend, and capture command; do not treat screenshots as the source asset.

## Support and security

Support covers the latest tagged example revision, its pinned Engine commit, and combinations exercised by required CI. A scheduled current-Engine success is forward-compatibility evidence, not support for every Engine `master` commit.

Public issues must include the example revision, Engine revision, host, target, exact command, and complete relevant log. Vulnerabilities use GitHub Security Advisories and are never posted with exploit details or credentials in public issues.

## Maintenance triggers

Reconcile the registry, overlay, generated outputs, and affected repositories when any of these changes:

- public CMake, CLI, package, native-extension, prototype, map, scripting, updater, or compatibility contracts;
- `Examples/MinimalProject`, starter smoke markers, prerequisites, or supported platforms;
- required repository files, branch protections, GitHub Actions versions, security policy, or asset licenses;
- repository lifecycle status, remote visibility/state, owner, dependency, source path, exit gate, tag, artifact, or public URL;
- Engine revision updates in a published example.

Run:

```bash
python BuildTools/docs_examples.py --write
python BuildTools/docs_examples.py --check
python BuildTools/docs_site.py --write
python BuildTools/docs_ai_delivery.py --write
python BuildTools/docs_validate.py
```

The generated model is the compact AI-readable portfolio. This page owns rationale and operation. External READMEs remain concise and link here instead of duplicating the policy.
