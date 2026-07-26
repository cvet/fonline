# Generated Contract Change Management

> Engine-owned maintainer guide. Use this page to compare the generated native API, CMake, main/helper BuildTools CLI, package, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, and font-format contracts across revisions and to dispose compatibility-sensitive changes before merge.

## Purpose

FOnline publishes fourteen deterministic machine-readable models under `Docs/generated/`. `BuildTools/docs_contract_diff.py` compares all fourteen with the same models at a base revision and produces one JSON report for automation plus one Markdown report for review.

The gate answers four separate questions:

1. Which domain and stable entry changed?
2. Is the change additive, documentation-only, policy-only, or structurally breaking?
3. Did the baseline revision promise compatibility for that entry or domain?
4. If review is required, where are the owner decision, migration, release-note, and compatibility dispositions?

The comparator reports internal churn but does not promote an internal surface to public API. Stability remains source-owned: native symbols use `///@ ApiContract`; CMake, main CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, and font-format models use their declared domain or entry stability.

## Covered domains

| Domain | Canonical model | Entry matching | Current enforcement |
| --- | --- | --- | --- |
| Native API | [generated/api.json](generated/api.json) | Native symbol `id`; overloads retain their signature-hashed IDs | Baseline `stable`, `experimental`, and `deprecated` breaks require disposition; internal breaks remain visible |
| CMake | [generated/cmake.json](generated/cmake.json) | `cmake.option.*`, `cmake.stage.*`, and `cmake.helper.*` IDs | The domain is `experimental`, so removals and shape changes require disposition |
| BuildTools CLI | [generated/cli.json](generated/cli.json) | Command and argument `cli.*` IDs | The domain is `internal`; breaking changes are reported but do not create a compatibility promise |
| Package | [generated/package.json](generated/package.json) | Declaration, option, target, platform, pack, payload, and argument `package.*` IDs | The domain is `internal`; breaking changes are reported but do not create a compatibility promise |
| Helper CLI | [generated/helper-cli.json](generated/helper-cli.json) | Helper, subcommand, and argument `helper-cli.*` IDs | The domain is `internal`; parser/ownership changes are reported but do not create a compatibility promise |
| Native extension | [generated/native-extension.json](generated/native-extension.json) | Role, hook, and binding-rule `native-extension.*` IDs | The domain is `experimental`; removals and structural changes require disposition, while binary compatibility between independently built revisions is not promised |
| Prototype format | [generated/prototype-format.json](generated/prototype-format.json) | Section, directive, rule, built-in entity, and property `prototype-format.*` IDs | Grammar/rules are `experimental` and require disposition when broken; the derived property catalog is `internal` and remains visible without an added compatibility promise |
| Map format | [generated/map-format.json](generated/map-format.json) | Section, directive, ownership, rule, and property `map-format.*` IDs | Grammar/ownership/rules are `experimental` and require disposition when broken; the revision-derived property catalog is `internal` |
| Model format | [generated/model-format.json](generated/model-format.json) | Compile limit, asset, token, and rule `model-format.*` IDs | Grammar/composition rules are `experimental` and require disposition when broken; compile limits and derived asset facts remain visible under their declared stability |
| Text format | [generated/text-format.json](generated/text-format.json) | Syntax, language, prototype-text, runtime, rendering, and validation `text-format.*` IDs | The parser/baker/runtime contract is `experimental`; removals and structural changes require disposition while project language and formatter policy remain outside the model |
| Effect format | [generated/effect-format.json](generated/effect-format.json) | Compile-limit, section, option, resource, baking, runtime, script-method, and validation `effect-format.*` IDs | The baker/renderer/runtime contract is `experimental`; removals and structural changes require disposition while project shader catalogs, visual policy, and ScriptValue meanings remain outside the model |
| Image format | [generated/image-format.json](generated/image-format.json) | Source-format, FOFRM field, filename-option, baking, runtime, and validation `image-format.*` IDs | The baker/default-client contract is `experimental`; private container entries are `internal`, while project asset catalogs, licenses, pack precedence, visual policy, and acceptance remain outside the model |
| Particle format | [generated/particle-format.json](generated/particle-format.json) | Registered object/family, XML, renderer, tooling, runtime, integration, and validation `particle-format.*` IDs | The `.fopts`/SPARK/Engine integration contract is `experimental`; the current native-coverage record is `internal`, while project catalogs, settings, effects, textures, budgets, and visual acceptance remain outside the model |
| Font format | [generated/font-format.json](generated/font-format.json) | Descriptor format/field, binding, layout, rendering, and validation `font-format.*` IDs | The FOFNT/BMFont and client text pipeline contract is `experimental`; cache internals remain `internal`, while project slot assignment, glyph coverage, typography, and visual acceptance remain outside the model |

Model source, repository/scope, or model-level contract changes are conservative domain breaks and always require disposition. This prevents a comparator or ownership boundary change from silently redefining what the gate covers.

Project-authored remote calls remain a separate baked project catalog. Remaining authored file formats, updater behavior, and project configuration keys require their owning model before they can join this comparator.

## Source paths inspected

- `BuildTools/docs_api.py`
- `BuildTools/docs_api_diff.py`
- `BuildTools/docs_cmake.py`
- `BuildTools/docs_cli.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/docs_helper_cli.py`
- `BuildTools/NativeExtensionInterface.json`
- `BuildTools/docs_native_extension.py`
- `BuildTools/PrototypeFormatInterface.json`
- `BuildTools/docs_prototype_format.py`
- `BuildTools/MapFormatInterface.json`
- `BuildTools/docs_map_format.py`
- `BuildTools/ModelFormatInterface.json`
- `BuildTools/docs_model_format.py`
- `BuildTools/TextFormatInterface.json`
- `BuildTools/docs_text_format.py`
- `BuildTools/EffectFormatInterface.json`
- `BuildTools/docs_effect_format.py`
- `BuildTools/ImageFormatInterface.json`
- `BuildTools/docs_image_format.py`
- `BuildTools/ParticleFormatInterface.json`
- `BuildTools/docs_particle_format.py`
- `BuildTools/FontFormatInterface.json`
- `BuildTools/docs_font_format.py`
- `BuildTools/docs_package.py`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_validate.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
- `BuildTools/tests/test_docs_helper_cli.py`
- `BuildTools/tests/test_docs_native_extension.py`
- `BuildTools/tests/test_docs_prototype_format.py`
- `BuildTools/tests/test_docs_map_format.py`
- `BuildTools/tests/test_docs_model_format.py`
- `BuildTools/tests/test_docs_text_format.py`
- `BuildTools/tests/test_docs_effect_format.py`
- `BuildTools/tests/test_docs_image_format.py`
- `BuildTools/tests/test_docs_particle_format.py`
- `BuildTools/tests/test_docs_font_format.py`
- `Docs/generated/api.json`
- `Docs/generated/cmake.json`
- `Docs/generated/cli.json`
- `Docs/generated/helper-cli.json`
- `Docs/generated/native-extension.json`
- `Docs/generated/prototype-format.json`
- `Docs/generated/map-format.json`
- `Docs/generated/model-format.json`
- `Docs/generated/text-format.json`
- `Docs/generated/effect-format.json`
- `Docs/generated/image-format.json`
- `Docs/generated/particle-format.json`
- `Docs/generated/font-format.json`
- `Docs/generated/package.json`
- `Docs/contract-change-dispositions.json`
- `Docs/Decisions/0002-public-api-stability-contract.md`
- `.github/workflows/validate.yml`

## Inputs and outputs

The aggregate diff consumes:

- one baseline directory or Git revision containing all available canonical models;
- the current `Docs/generated/` model directory;
- the cumulative `Docs/contract-change-dispositions.json` ledger.

It writes revision-pair artifacts under ignored `Workspace/`:

- `contract-diff.json` for automation and AI review;
- `contract-diff.md` for maintainers.

GitHub Actions uploads them as `contract-diff-<commit-sha>` for 14 days. They are diagnostic artifacts, not current reference pages, so the GitHub Pages site continues to render only the current Markdown and generated references.

## Stable matching and normalization

Each domain compares stable IDs, not display names or JSON array positions. Additions and removals never pair by a similar label.

The native comparator retains its symbol-specific behavior:

- a non-overloaded signature change normally modifies one symbol;
- an overload signature change appears as one removed ID and one added ID under the same `family_id`;
- source path and line movement cannot become a false breaking change.

The CMake, main CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, and font-format comparators flatten their model-owned entry collections by stable ID. Source provenance, including derived enum source paths/line numbers, generated summaries, derived usage strings, and model digests do not create duplicate changes. Nested description/help edits remain documentation changes; defaults, choices, cardinality, required flags, ownership/invocation metadata, platform/target matrices, signatures, stage order, hook fallbacks/call sites, role routing, prototype/map/model/text/effect/image/particle/font grammar and resource applicability, compile limits, language normalization, runtime lookup, shader/image/particle/font cache behavior, and payload semantics are structural contract data.

Each domain records two hashes:

- `model_sha256` covers the exact canonical model input;
- `contract_sha256` excludes source provenance and descriptive prose while retaining shape and policy.

A disposition is bound to the affected domain's baseline and current contract hashes. Unrelated prose or source-line movement cannot invalidate it, while a further contract change does.

## Automatic classifications

| Classification | Examples | Merge effect |
| --- | --- | --- |
| `additive` | New symbol, option, command, argument, platform, pack, or payload entry | Reported; no breaking disposition |
| `documentation` | Description, example, summary, notes, or help prose only | Reported; no breaking disposition |
| `policy` | Non-withdrawing stability/since/support metadata | Reported; no breaking disposition |
| `breaking` | Removal; signature/type/default/required/choice/order/matrix/payload shape change; stability withdrawal | Disposition depends on baseline stability, except model-source/scope changes which always require one |

The old revision controls enforcement. A change cannot relabel a baseline-public surface as `internal` while deleting or reshaping it to bypass review.

| Baseline stability | Breaking entry change |
| --- | --- |
| `stable` | Blocked until migration, release, compatibility, and owner handling are recorded |
| `experimental` | Blocked until the owner records the change and release/migration disposition |
| `deprecated` | Blocked until replacement/removal timing is explicitly disposed |
| `internal` | Visible in the report; no compatibility disposition required |

## Local workflow

Regenerate and verify every current model first:

```bash
python BuildTools/docs_api.py --write
python BuildTools/docs_reference.py --write
python BuildTools/docs_cmake.py --write
python BuildTools/docs_cli.py --write
python BuildTools/docs_helper_cli.py --write
python BuildTools/docs_native_extension.py --write
python BuildTools/docs_prototype_format.py --write
python BuildTools/docs_map_format.py --write
python BuildTools/docs_model_format.py --write
python BuildTools/docs_text_format.py --write
python BuildTools/docs_effect_format.py --write
python BuildTools/docs_image_format.py --write
python BuildTools/docs_particle_format.py --write
python BuildTools/docs_font_format.py --write
python BuildTools/docs_package.py --write
```

Compare with the intended integration base:

```bash
python BuildTools/docs_contract_diff.py \
  --baseline-git-ref origin/master \
  --current-dir Docs/generated \
  --dispositions Docs/contract-change-dispositions.json \
  --json-output Workspace/contract-diff.json \
  --markdown-output Workspace/contract-diff.md \
  --write \
  --enforce
```

For an explicitly saved local baseline containing all fourteen model files:

```bash
python BuildTools/docs_contract_diff.py \
  --baseline-dir Workspace/contract-baseline \
  --current-dir Docs/generated \
  --check \
  --enforce
```

`--check` computes and enforces without writing report files. Prefer `--write` while resolving a failure because the Markdown report includes ready-to-fill ledger templates.

`BuildTools/docs_api_diff.py` remains available for native-symbol investigation and regression tests. CI enforcement uses the aggregate command so a green API-only report cannot hide CMake, CLI, package, helper-CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, or font-format drift.

## Bootstrap behavior

`--allow-missing-baseline` is reserved for the first revision that introduces a canonical model to an existing branch. With a Git baseline, only missing domains enter visible `bootstrap` status; present domains are still compared and enforced. An unknown or unfetched Git revision remains an error.

Directory baselines must contain all fourteen models. This avoids accidental partial local comparisons that look complete.

After all models have landed, missing baseline files are not normal. Do not add bootstrap mode to local commands or CI changes merely to bypass a failing comparison.

## Disposition ledger

`Docs/contract-change-dispositions.json` is a cumulative schema-v2 ledger. Every entry contains:

- `domain`: `api`, `cmake`, `cli`, `package`, `helper-cli`, `native-extension`, `prototype-format`, `map-format`, `model-format`, `text-format`, `effect-format`, `image-format`, `particle-format`, or `font-format`;
- deterministic domain-prefixed `change_id`;
- the baseline and current domain `contract_sha256` values;
- owner classification: `breaking` or `compatible`;
- non-empty rationale, migration, release-note, compatibility, and owner fields.

Example:

```json
{
  "domain": "cmake",
  "change_id": "cmake-change.modified.0123456789abcdef",
  "baseline_contract_sha256": "<64 lowercase hex characters>",
  "current_contract_sha256": "<64 lowercase hex characters>",
  "classification": "breaking",
  "rationale": "Why the change is intentional and what embedding projects observe.",
  "migration": "Docs/Migrations/Next.md#changed-option",
  "release_note": "Docs/ReleaseNotes/Next.md#changed-option",
  "compatibility": "Pinned projects must update the option and engine revision together.",
  "owner": "build-release"
}
```

Historical entries may remain; unmatched entries are inert. Domain, change ID, and both hashes must match. Marking a detected change `compatible` does not waive the other fields. If migration or a release note is unnecessary, record the reviewed reason rather than an empty value.

The validator proves ledger shape and exact binding, not approval quality. Review must reject placeholder text, incorrect compatibility claims, missing migrations, or an owner outside the affected domain.

## CI behavior

The `Validate documentation` job checks out full history and selects:

- `github.event.pull_request.base.sha` for pull requests;
- `github.event.before` for pushes to `master`.

After model/reference freshness checks, CI runs `docs_contract_diff.py --write --enforce`. A missing required disposition fails the job. The `if: always()` upload step preserves both reports for diagnosis.

A multi-commit pull request is compared with its base branch commit, not the previous feature-branch commit. A multi-commit push uses the complete pushed range. The standalone validator rejects removal of the shared ledger, fourteen-model manifest contract, full-history checkout, base-ref argument, aggregate test, or enforcement switch.

## What requires human review

For a public breaking change, review all of these:

1. The source declaration or manifest change is intentional.
2. A replacement or migration path exists, or the ledger explains why none is possible.
3. Release notes identify affected developers and supported revision lines.
4. Network/serialization changes include required compatibility or migration metadata.
5. Generated references, examples, and focused tests change together.
6. The owning runtime, scripting, build, or release domain accepts the support timeline.

Internal changes need no compatibility promise, but surprising removals and shape churn still deserve review because they may reveal a missing stability classification.

## Limits of static diffing

The aggregate report cannot detect:

- behavior changes behind unchanged declarations;
- updater protocol, authored formats, or project settings without canonical models;
- incorrect prose claiming compatibility;
- whether a migration guide or package path was exercised successfully;
- support across release lines that have not been declared and tagged.

Runtime, structural CMake, native-extension, prototype/map/model/text/effect/image/particle/font bake and visible-render, package, starter, and embedding-project tests remain required. A green fourteen-domain report proves only that the modeled declarative surfaces have an accepted revision transition.

## Troubleshooting

- `Contract baseline git revision is unavailable`: fetch the exact revision; do not use bootstrap to hide a shallow checkout.
- `does not exist at baseline revision`: expected only for a model's first landed revision.
- `missing dispositions`: inspect `Workspace/contract-diff.md`, add reviewed entries, and rerun against the same revision pair.
- disposition remains missing: copy the exact domain, change ID, and both contract hashes from the current report.
- a description edit is classified as breaking: add a focused nested-field regression before changing policy or the ledger.
- an internal change unexpectedly blocks: inspect model-source/scope drift first; ordinary internal entry changes are report-only.
- many entries change together: verify stable IDs and the owning generator before accepting the report.

## Validation checklist

1. Regenerate and check all fourteen canonical models plus generated Markdown.
2. Run `test_docs_api_diff.py` and `test_docs_contract_diff.py`.
3. Compare against the intended base with `--write --enforce`.
4. Review every required disposition and replace every generated placeholder.
5. Run the affected native, script, CMake, packaging, starter, or project test.
6. Run `test_docs_validate.py` and `docs_validate.py`.
7. Inspect the CI contract-diff artifact and confirm each domain status.
8. Keep staging empty unless staging or commit was explicitly requested.

## See also

- [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md) - canonical models and generated-reference ownership.
- [Decisions/0002-public-api-stability-contract.md](Decisions/0002-public-api-stability-contract.md) - accepted stability and change policy.
- [RemoteCalls.md](RemoteCalls.md) - project-owned remote-call catalog and compatibility boundary.
- [DocumentationMaintenance.md](DocumentationMaintenance.md) - documentation change and revision-reconciliation workflow.
- [../PUBLIC_API.md](../PUBLIC_API.md) - transitional public API route.
