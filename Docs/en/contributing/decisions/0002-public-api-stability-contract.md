---
layout: default
title: "ADR-0002: Public API Stability Contract"
locale: en
document_id: adr-public-api-stability-contract
permalink: /Docs/en/contributing/decisions/0002-public-api-stability-contract.html
---

# ADR-0002: Public API Stability Contract

- Status: Accepted
- Date: 2026-07-10
- Amended: 2026-08-02 after the native-codegen inventory received an owner-reviewed experimental scope contract
- Owners: scripting, runtime, build-release, documentation

## Context

FOnline exposes many surfaces to game projects: CMake helpers, BuildTools commands, settings, package layouts, native hooks, AngelScript methods/types/events/properties, metadata, serialized entities, network messages, file formats, and application/runtime ABIs.

Reachability is not the same as a compatibility promise. Before this decision, `PUBLIC_API.md` mixed current, planned, and obsolete statements and could not be regenerated or checked against source. The locale-specific public contract index is now generated, while the owning model retains the actual stability classification and the root path remains a durable legacy route. Freezing every reachable symbol would also prevent necessary engine refactoring.

Developers and AI agents need to know which surface is stable, which is experimental, where the authoritative declaration lives, and what work is required when a contract changes.

## Decision

### Stability labels

Every generated public contract uses one of these labels:

| Label | Meaning |
|---|---|
| `stable` | Supported for documented release lines; incompatible changes require migration and release disposition. |
| `experimental` | Publicly usable for evaluation, but signature/behavior may change with explicit release notes. |
| `internal` | Reachable implementation detail with no compatibility promise. |
| `deprecated` | Previously supported surface scheduled for removal, with replacement and removal window documented. |

Until a symbol or surface is explicitly classified, it is `internal`. Existing reachability, an example in a project, or inclusion in generated bindings does not implicitly make it stable.

### Source of truth

1. Source declarations, metadata annotations, parsers, settings declarations, CMake helpers, and tests are authoritative for current behavior.
2. Generated canonical contract models normalize those sources into stable IDs and machine-readable JSON.
3. Human reference pages, the locale-specific public contract indexes, and the root `PUBLIC_API.md` legacy route are generated from those models and supplemented with task-oriented examples and explanations.
4. Manual prose must not maintain declaration counts, signatures, or cross-domain inventories that can be generated. `Docs/generated/source-inventory.json` remains the independent source inventory; the eighteen contract models own their respective reusable surfaces.
5. Project documentation may describe how it consumes an engine API but cannot upgrade that API's stability label.

### Source-owned classification

Native-codegen symbols use a separate `///@ ApiContract <selector> <label> ...` metadata tag. A selector resolves against the canonical generated `id` first, then against `family_id` so one reviewed declaration can cover an overload family. The reserved `scope:native-codegen` selector classifies the complete current model and requires both `SymbolCount` and `InventorySha256` pins over the sorted stable-ID inventory. The tag is parsed and validated by `BuildTools/codegen.py` but is documentation-only and excluded from the runtime compatibility hash.

Without a valid scope contract, unannotated symbols remain `internal (default)`. A valid scope contract applies its reviewed label only while both inventory pins match; any symbol addition, removal, or stable-ID change fails generation until an owner updates the pins. An exact symbol declaration may override the scope, while overlapping exact/family declarations remain invalid. An explicit `internal` tag records a reviewed decision without promoting the symbol. `stable`/`experimental` require `Since`; `deprecated` requires deprecation version, live replacement, and removal target.

Contract tags are not a shortcut around owner review. The current scope declaration classifies all 2,472 generated symbols as revision-pinned `experimental` since `2022.1.0.wip`, then keeps the development-only `Game.BreakIntoDebugger` helper explicitly `internal`. This makes the integration surface usable and change-tracked without claiming any broad `stable` compatibility promise.

### Contract domains

The stability model applies separately to:

- build helpers, options, stages, and BuildTools CLI;
- settings and configuration keys;
- script methods, global functions, types, enums, events, remote calls, and properties;
- native hooks and extension ABI;
- client host/runtime ABI and updater protocol;
- serialized data, database entities, and migration metadata;
- network protocol and compatibility version;
- authored and generated file formats;
- package layout and supported platform matrix.

A release can support one domain without promising stability for every domain.

### Change policy

For a `stable` surface, an incompatible change requires:

1. an API diff or equivalent source-backed detection;
2. an explicit breaking-change classification;
3. migration instructions and replacement where applicable;
4. release-note and support-policy disposition;
5. compatibility-version or migration metadata updates when the network/serialized contract requires them;
6. updates to generated reference, examples, and tests in the same change.

Silent fallback aliases, undocumented compatibility shims, and stale duplicate overloads are not the default migration strategy. Add a compatibility layer only when the support policy requires it and give it an owner/removal condition.

For an `experimental` surface, breaking changes still require an explicit changelog/release note so users and AI agents can select the correct revision.

### Automated generated-contract enforcement

`BuildTools/docs_contract_diff.py` compares all eighteen modeled domains with the same paths at the pull request base SHA or pre-push revision: native API, CMake, main BuildTools CLI, package, helper CLI, native extension, prototype, map, model, text, effect, image, particle, font, audio, video, GUI runtime, and AiControl protocol. The specialized native layer in `BuildTools/docs_api_diff.py` retains symbol/overload semantics; the other models match their source-owned stable IDs. All domains classify additions, documentation, policy, and breaking changes against baseline stability so a simultaneous downgrade to `internal` cannot bypass review.

For helper scripts, each executable `create_parser()` remains the syntax source of truth while `BuildTools/HelperCliInterface.json` owns stable helper identity, purpose, audience, invocation owner, and explicit exclusions. AST inventory validation rejects a newly exposed helper parser that is neither modeled nor assigned to another canonical domain. The helper CLI domain is `internal` until an owner approves a versioned support policy.

For project-native C++, `BuildTools/NativeExtensionInterface.json` models the source roles consumed by current targets, supported hook signatures/fallbacks/call sites, and binding rules. Structural and generator tests compare it with current CMake/codegen behavior; it is not a runtime input. The domain is `experimental` and source-compatible only at a pinned engine revision; it does not promise a cross-revision binary ABI. Project implementations and external SDKs remain outside the engine contract.

A breaking `stable`, `experimental`, or `deprecated` declaration change requires an exact schema-v2 cumulative-ledger entry in `Docs/contract-change-dispositions.json`. The entry records the domain, owner classification, rationale, migration, release-note, and compatibility handling and is bound to the change ID plus that domain's baseline/current contract digests. Model-source, model-scope, and model-level contract changes also require disposition. Internal entry changes remain reportable but do not become compatibility promises.

CI writes JSON/Markdown revision-pair reports under `Workspace/`, enforces missing dispositions, and uploads the report even on failure. The current site does not publish historical snapshots merely to support this gate.

### Versioning

Documentation describes the current branch until the engine has meaningful release tags and a support matrix. Historical API snapshots are not created merely because the publication layer can host them. When supported release lines exist, generated models and pages are pinned to immutable tags/revisions.

## Consequences

### Positive

- Developers can distinguish contract from implementation and examples.
- Engine refactoring remains possible because unclassified internals are not accidentally frozen.
- Breaking changes become visible, reviewable, and migration-backed.
- Generated JSON gives documentation tools and AI systems stable symbol identities.
- Project examples cannot silently redefine engine guarantees.

### Costs

- Existing surfaces must be inventoried and classified before broad `stable` promises are made.
- Source annotations/descriptions need improvement where generated reference lacks meaning.
- Multi-domain diff automation and its cumulative decision ledger require ongoing maintenance as contract domains and release policy expand.
- Maintainers must write migration guidance when a stable contract changes.

## Rejected alternatives

- **Everything reachable is public/stable:** rejected because it freezes internals and contradicts current refactoring reality.
- **No stability promises at all:** rejected because game developers need a dependable integration surface.
- **Manual `PUBLIC_API.md` lists:** rejected because signatures, counts, and ownership drift from source.
- **Project usage defines stability:** rejected because one project's dependency is evidence, not an engine-wide support decision.

## Verification

- `BuildTools/docs_inventory.py --check` pins the independent source-backed export/test/settings inventory.
- `BuildTools/docs_api.py --check` pins the canonical native-codegen model, including family/symbol IDs, descriptions, explicit/default contract provenance, lifecycle metadata, signatures, and declaration provenance.
- `BuildTools/docs_reference.py --check` pins the generated human pages for the native-codegen model.
- Focused tests prove that `ApiContract` family selectors cover overloads, stale scope counts and inventory hashes fail closed, exact scope overrides work, deprecated replacements resolve, invalid selectors fail, and docs-only annotations do not change the compatibility hash.
- `BuildTools/docs_contract_diff.py --enforce`, the specialized API comparator, and their focused tests reject undisposed baseline-public removals, shape changes, stability withdrawals, stale digests, and model/parser contract changes across all eighteen generated domains.
- `BuildTools/docs_public_api.py --check` regenerates the root contract index from those eighteen models and rejects stale domain links, labels, or native summary counts.
- Broad stable classifications remain owner and release-policy work; the inventory-pinned `experimental` scope does not itself claim cross-revision stable compatibility.
- Code review must reject prose that presents an unclassified reachable symbol as stable.

## Related documents

- [GeneratedApiAndMetadata.md](../../reference/metadata/index.md)
- [Generated Contract Change Management](../contract-change-management.md)
- [ScriptMethodsMap.md](../../../ScriptMethodsMap.md)
- [ProductionDocumentationPlan.md](https://github.com/cvet/fonline/blob/master/Docs/ProductionDocumentationPlan.md)
- [Public Contract Index](../../reference/public-contract/index.md)
