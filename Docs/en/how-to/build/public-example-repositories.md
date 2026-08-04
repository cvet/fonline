---
layout: default
title: Public Example Repositories
locale: en
document_id: public-example-repositories
permalink: /Docs/en/how-to/build/public-example-repositories.html
---

# Public Example Repositories

This guide defines how FOnline's example repositories are created, reviewed, staged, released, and kept compatible. The machine source is [Examples/PublicRepositories.json](../../../../Examples/PublicRepositories.json); the checked projection is [the generated registry](../../reference/public-examples/index.md).

## Contract status

This is the reusable Engine-owned publication procedure. It governs repository identity, exact Engine pins, common files, validation, compatibility lanes, provenance, support, and publication evidence. An individual example owns its code, assets, tutorial, releases, and issue tracker, but cannot weaken this contract.

The portfolio is still pre-publication. The four remote repositories exist as private staging repositories. Two Engine-owned source fixtures are ready, one of them has been staged remotely, and none has passed the complete public release gate. Repository names describe their intended public role, not current visibility.

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
| 2 | `cvet/fonline-minimal-multiplayer` | Small playable server/client/content/tutorial slice | Source-ready in `Examples/MinimalMultiplayer`, including a locally green Windows package lane; private repository reserved and not yet staged or published |
| 3 | `cvet/fonline-content-showcase` | Rendering, Mapper, asset, capture, and Web presentation | Source-ready in `Examples/ContentShowcase`; native Windows smoke, Direct3D 11 capture, complete Web package validation, and an isolated Chromium WebGL 2 runtime/capture are locally green, while Linux runtime/OpenGL and remote publication evidence remain unobserved |
| 4 | `cvet/fonline-native-extension-sample` | Advanced project-native C++ hook/export/test path | Source-ready in `Examples/NativeExtensionSample`; the focused native test and complete hook/export runtime route are locally Windows-green, while the private repository remains reserved and unpublished |

Remote existence, source readiness, and public release are separate states. `remote.visibility = private` records an administrator-created staging repository without exposing it as documentation; `remote.state = reserved` means only the repository shell exists, while `source-staged` means reviewed candidate source has been pushed. Do not publish links for a repository while its registry status is `planned`, `blocked`, or `source-ready`. Change both lifecycle and remote state to `published`, and visibility to `public`, only after its protected default branch, security configuration, required checks, first tag, and public artifact have been verified.

### Content Showcase source contract

[`Examples/ContentShowcase`](../../../../Examples/ContentShowcase/README.md) is the executable, Engine-owned gallery behind the third portfolio entry. It contains a one-map scene, an animated FOFRM beacon, a scripted sprite panel, SPARK particles rendered through a `.fofx` effect, a generated WAV cue, project-original TGA inputs, a prototype, and the smallest client/server script needed to prove loading and interaction. The authored inputs live under `ShowcaseAssets/`; generated `Resources/`, `ServerResources/`, `Baking/`, `Build/`, and `Workspace/` trees never belong to a staged repository.

Run `python validate.py` for the native source/bake/process gate and `python validate.py --web` for the fast Web source/compile gate. `python validate.py --web-package` adds native-host force-baking, exact raw/ZIP payload validation, WebAssembly and `Resources.data` checks, and archive parity. After `npm ci` and `npx playwright install chromium` in `WebTests/`, `python validate.py --web-runtime` adds a native server, packaged HTTP delivery, required response and lifecycle markers, a real WebGL 2 context, console/page/network failure checks, and compositor-pixel evidence. The same complete route is exposed as `web-showcase-runtime` in BuildTools and the required workflow. On Windows, `cmake --build --preset windows-capture` launches the real Direct3D 11 client. On Linux, `linux-showcase-capture` runs `linux-capture` under Xvfb with software Mesa and retains the PNG, process report, exact Engine SHA, and backend record as a commit-addressed workflow artifact. Both native routes record twelve warmed samples, validate independent header/runtime/gallery/footer regions, and keep the strongest complete frame. `assets/provenance.json`, `quality/performance-budget.json`, and `captures/capture-contract.json` are release inputs: update them in the same change as any asset, layout, renderer, budget, or supported-backend change. Checked Direct3D 11 and WebGL 2 captures are local backend-specific evidence; Linux OpenGL remains unobserved until the required remote job retains a successful artifact.

### Supporting in-repository fixtures

Not every focused proof should become another public game repository. `Examples/GameplayTestHarness` isolates process-runner semantics, and [`Examples/AiControlSample`](../../../../Examples/AiControlSample/README.md) isolates the AiControl transport, authorization, event cursor, and command lifecycle. The AiControl sample deliberately does not embed FOnline or define a game schema, so it is published with the Engine documentation rather than presented as a fifth minimal game.

If a separate AiControl repository is later justified, add it to `Examples/PublicRepositories.json` only after it embeds an exact Engine pin, contains a real project native listener and client-loop integration, preserves normal server authority, passes pinned/current Windows and Linux lanes, proves the shipping listener is absent, and carries the same governance/security files as the existing portfolio. The protocol-only Python smoke is insufficient for that publication claim.

## Remote staging audit

The authenticated GitHub audit on 2026-08-03 produced the snapshot recorded in `remote` fields of the machine registry:

| Repository | Visibility / branch | Observed head | Observed contents | Required checks |
| --- | --- | --- | --- | --- |
| `cvet/fonline-project-template` | private / `main` | `9946ca42c332a294f8fedd2732e7850a01c1ec27` | Candidate source with Engine pin `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42` | not observed |
| `cvet/fonline-minimal-multiplayer` | private / `main` | `97d232431488125b370be352fdcf28f66e6cbf4f` | Reservation README only | not observed |
| `cvet/fonline-content-showcase` | private / `main` | `011dab0d07eef6387609821206b8ee534ec51c3f` | Reservation README only | not observed |
| `cvet/fonline-native-extension-sample` | private / `main` | `97823816ab333a62aced43edd4daafa19c5fee22` | Reservation README only | not observed |

The template's Engine pin is a reachable ancestor of `origin/master`, but it predates `BuildTools/docs_examples.py`. Its workflows therefore skip the repository-contract command when that file is absent. The 2026-08-03 audit reconfirmed successful pinned run `29739863448` on Windows and Ubuntu and successful current-Engine run `29740066760` on Ubuntu; neither run retained an artifact, and no required commit status was observed. These runs prove the old primary smoke path, not the current repository contract or publication gate. Restage the candidate at a reviewed exact Engine revision containing the validator, then obtain complete pinned/current results and retained artifacts before publication.

Do not infer branch protection, Security Advisory availability, secrets policy, Pages configuration, release tags, or artifact retention from repository existence. Those are separate administrator-owned observations and remain unverified until recorded by the publication owner.

### Publication decision

A repository may move to public visibility only when one identified candidate commit satisfies all of these conditions:

1. the registry, `example-repository.json`, Engine gitlink, generated README, and remote head identify the same repository and exact Engine revision;
2. a clean clone passes the repository validator, primary check, and every required pinned/current platform lane without fallback skips;
3. branch protection requires the declared checks and CODEOWNERS review, direct release pushes are blocked, and Security Advisories are enabled;
4. every redistributed asset passes byte-level provenance validation;
5. the first immutable tag and commit-addressed artifact are built from that same commit and include the required evidence;
6. support, security, contribution, and known-limit documentation have been reviewed from the public reader's perspective;
7. an administrator changes visibility, after which maintainers update the registry to `published` and regenerate the site and AI artifacts.

If any condition is unknown, keep the repository private and its check state `not-observed`. A repository shell, local green run, or source-ready fixture is not a substitute for observed remote gates.

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
- `.gitmodules` with the canonical HTTPS Engine URL and `.gitattributes` with the shared line-ending policy;
- `Engine/` committed as a git submodule at that exact revision;
- the common `LICENSE`, `CONTRIBUTING.md`, `SECURITY.md`, `SUPPORT.md`, `THIRD_PARTY_NOTICES.md`, CODEOWNERS, pull-request template, and two compatibility workflows;
- `assets/provenance.json`, including an entry for every redistributed visual, audio, model, font, or other non-code asset;
- a short task-oriented README that identifies the Engine revision and links deep reusable contracts to `fonline.ru`;
- one repository-specific command that proves its primary behavior without administrator services or private credentials.

Validate a candidate checkout from its root:

```bash
python Engine/BuildTools/docs_examples.py --verify-repository . --engine-mode pinned
```

The validator rejects unresolved publication placeholders, unknown repository IDs, non-exact revisions, metadata/registry drift, a wrong `.gitmodules` path or Engine URL, a non-submodule `Engine/`, gitlink/checkout mismatch, missing governance files, missing provenance assets, and asset bytes whose SHA-256 differs from the recorded digest.

## Creating a repository

The publication owner follows this sequence:

1. Re-audit the remote and update `verified_on`, default branch, head commit, observed Engine pin, and required-check state in the registry. Confirm the entry's dependencies and exit gate and record the candidate owner.
2. Confirm that the Engine working tree is clean and that the chosen commit is fetchable from `program.engine_clone_url`. The source may consume no uncommitted Engine behavior.
3. Materialize the approved source plus governance overlay into a new directory:

   ```bash
   engine_revision="$(git rev-parse HEAD)"
   candidate="Workspace/fonline-minimal-multiplayer"
   python BuildTools/docs_examples.py --stage-repository minimal-multiplayer --engine-revision "$engine_revision" --output "$candidate"
   ```

   The command requires that the requested revision equal the clean Engine checkout and be contained by a fetched remote-tracking branch. It refuses an existing output directory, excludes the registry's generated/local paths, preserves the source walkthrough as `TUTORIAL.md`, renders every template placeholder, records the exact pin in metadata, and aligns Engine-owned provenance URLs with that pin. It does not commit, push, or change a remote repository.
4. Initialize the candidate and create the exact Engine gitlink:

   ```bash
   git -C "$candidate" init -b main
   git -C "$candidate" submodule add --force https://github.com/cvet/fonline.git Engine
   git -C "$candidate/Engine" checkout "$engine_revision"
   git -C "$candidate" add .
   ```

5. Review the complete staged tree. Add no asset whose source, license, digest, and redistribution right cannot be verified. The generated short README is the public front door; `TUTORIAL.md` owns the complete project-specific walkthrough.
6. Commit the candidate, run the repository validator and the primary check from a clean clone, then push to the private staging repository for the first pinned/current CI runs.
7. Enable GitHub Security Advisories, require CODEOWNERS review, protect `main`, require the registry's checks, and prevent direct release pushes.
8. Review the README and generated documentation links from a clean clone. Create the first immutable candidate tag and artifact, verify them, and only then change repository visibility.
9. Change the Engine registry status and remote state to `published`, record public visibility and passing checks, add only the verified public/tag links, regenerate documentation, and rerun site/AI validation.

Creating the remote repository and changing its settings are owner-gated operations. A local documentation change never implies authorization to create, push, publish, or transfer a GitHub repository.

## Engine revisions and compatibility

Release artifacts and tutorial tags build against the exact Engine gitlink recorded in `example-repository.json`. They never fetch a floating default branch. This makes logs, generated metadata, screenshots, package contents, and support claims reproducible.

Each repository also runs a weekly `current-engine` lane against Engine `master`. That lane answers whether an update is ready; it does not mutate the release pin. A green result may produce a reviewed Engine-update pull request. A failure remains visible with the tested Engine commit and is assigned to the owner of the changed boundary.

Review all three compatibility boundaries during an update:

- gameplay compatibility version, including network and serialized contract changes;
- updater protocol generation;
- native client host/runtime ABI.

Updater protocol generation 2 and client host/runtime ABI 3 reject older unsafe native clients before they can transfer or load a new runtime. An example distributing a native client across this boundary must publish a new full client package and state that generation-1/ABI-2 installations require one manual reinstall. Do not present an in-process runtime reload or a floating client package as a compatibility path. See [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md).

## CI lanes

`pinned-engine` runs on every pull request and protected-branch update. It checks repository governance, gitlink/metadata identity, the primary example behavior, and all repository-specific platform gates. The project template and minimal multiplayer source both require Windows and Linux smoke jobs; Minimal Multiplayer additionally requires Windows/Linux package acceptance and retained commit-addressed package evidence.

Linux jobs prepare host dependencies through the checked-out revision's `Engine/BuildTools/prepare-workspace.sh linux-packages linux` command. Do not copy an apt package list into example workflows: the Engine-owned command is the versioned platform contract and keeps pinned and compatibility lanes on the same prerequisites.

Content Showcase adds `showcase-display-packages web-packages web` to that command. Because the helper runs from the example repository root, it installs the pinned SDK under `Workspace/emsdk`; set `FO_EMSDK` to that root-owned path, not `Engine/Workspace/emsdk`. The Linux OpenGL PNG, capture contract, and process report are uploaded immediately after the successful Xvfb/Mesa capture and before the longer WebGL 2 runtime lane. A later Web failure therefore cannot erase already valid backend evidence, while the overall required job still remains failed until every lane passes.

`current-engine` runs weekly and on demand. It temporarily checks out Engine `master`, records the tested commit, and runs the same primary behavior. It must not rewrite `example-repository.json`, the gitlink, generated files, tags, or release artifacts.

Additional gates come from the registry entry:

- tutorial repositories replay every lesson tag or fixture;
- the content showcase validates provenance, performance budgets, and capture reproduction;
- the native extension sample runs focused native tests and the generated extension contract checks.

An example that publishes native client/server archives must add a project-owned package lane modeled on `Examples/PackagingMatrix`: force-baked embedded config, archive/payload inventory parity, packaged updater handshake, exact Engine revision, SHA-256 manifest, and commit-addressed CI artifact. `Examples/MinimalMultiplayer` contains this source lane and has local Windows proof; its Linux job, immutable external artifact, and public tag still require observation. The shared Engine fixture proves package mechanics only; it does not silently qualify an example repository's release artifact.

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

Every redistributed asset entry records a stable ID, repository-relative path, SPDX-style allowed license identifier, source URL or `project-original`, and lowercase SHA-256 digest. The validator rejects missing files, duplicate identities, unknown licenses, non-HTTPS external sources, malformed digests, and byte/digest mismatches.

The template and native-extension sample should remain asset-free. The multiplayer example should prefer project-original or permissively licensed assets. The showcase accepts only audited public-domain, CC0, CC-BY, MIT-compatible, or project-original material whose redistribution and modification rights are explicit.

Screenshots and generated captures are release artifacts derived from a tagged build. Record the source tag, Engine revision, backend, and capture command; do not treat screenshots as the source asset.

## Project evidence and extraction rules

Last Frontier and `fonline-tla` are evidence that real games need clean Engine/project ownership, reproducible builds, native extensions, package acceptance, multilingual content, asset catalogs, and long-lived upgrade paths. They are not publication source trees. Their project services, private workflows, game schemas, assets, balance, credentials, and refactoring constraints must not leak into an Engine example.

Promote a practice from a production project only through this sequence:

1. identify the reusable problem in both Engine and project terms;
2. verify the behavior against current Engine source and tests;
3. reduce it to an Engine-owned fixture, validator, or documented contract without project-only dependencies;
4. prove the reduced route on an exact Engine revision and in the current-Engine lane;
5. add it to one example only when it supports that repository's single responsibility and exit gate.

The external evidence record `asset-provenance-and-public-examples` specifically permits the conclusion that public examples need redistributable minimal assets, exact Engine pins, byte-verified provenance, immutable lesson tags, and pinned/current compatibility evidence. It does not grant redistribution rights for either project's assets. Maintainers can inspect the [internal generated evidence index](https://github.com/cvet/fonline/blob/master/Docs/generated/external-project-evidence/index.md); it is source-controlled but excluded from the public site and AI delivery.

Treat TLA's ongoing refactoring as compatibility input, not a best-practice template. Treat Last Frontier's production checks as candidate practices that still need an Engine-owned reduction. This keeps examples useful to both human developers and AI agents without making them dependent on either game.

## Support and security

Support covers the latest tagged example revision, its pinned Engine commit, and combinations exercised by required CI. A scheduled current-Engine success is forward-compatibility evidence, not support for every Engine `master` commit.

Public issues must include the example revision, Engine revision, host, target, exact command, and complete relevant log. Vulnerabilities use GitHub Security Advisories and are never posted with exploit details or credentials in public issues.

## Maintenance triggers

Reconcile the registry, overlay, generated outputs, remote audit, and affected repositories when any of these changes:

- public CMake, CLI, package, native-extension, prototype, map, scripting, updater, or compatibility contracts;
- `Examples/MinimalProject`, `Examples/MinimalMultiplayer`, `Examples/ContentShowcase`, `Examples/NativeExtensionSample`, their smoke markers, prerequisites, capture contracts, or supported platforms;
- `Examples/AiControlSample`, `BuildTools/AiControlProtocol.json`, the reference client, protocol smoke, or the decision to promote a native public example;
- source-staging exclusions, display names, primary checks, the generated `TUTORIAL.md`, or the candidate materialization command;
- required repository files, branch protections, GitHub Actions versions, security policy, or asset licenses;
- repository lifecycle status, remote visibility/state, observed branch/head/check state, owner, dependency, source path, exit gate, tag, artifact, or public URL;
- Engine revision updates in a staged or published example;
- Last Frontier or TLA evidence used to justify a promoted practice.

Run:

```bash
python BuildTools/docs_examples.py --write
python BuildTools/docs_examples.py --check
python BuildTools/docs_external_evidence.py --check
python BuildTools/docs_site.py --write
python BuildTools/docs_ai_delivery.py --write
python BuildTools/docs_validate.py
```

The generated model is the compact AI-readable portfolio. This page owns rationale and operation. External READMEs remain concise and link here instead of duplicating the policy. Record each remote audit date and evidence in the active documentation plan; never update `required_checks_state` from an assumption.

## See also

- [ADR-0005: Public Example Repository Ownership](../../contributing/decisions/0005-public-example-repository-ownership.md)
- [Packaging and Release](../release/packaging.md)
- [Engine Upgrade Guide](../migration/engine-upgrade.md)
- [Support Matrix](../../reference/platforms/support-matrix.md)
