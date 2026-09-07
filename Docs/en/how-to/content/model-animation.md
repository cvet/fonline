---
layout: default
title: Model Animation Metadata and Duration
locale: en
document_id: model-animation
permalink: /Docs/en/how-to/content/model-animation.html
---

# Model Animation Metadata and Duration

This guide documents the reusable FOnline contract that maps authored model-animation sources and `(state, action)` pairs into the client runtime rig and common effective-cycle metadata. It follows the current source loader, converter, `ModelInfoBaker`, client lookup, common metadata registration, script exports, and engine tests. An embedding project may define its own model names, animation enum usage, gameplay timing, and fallback policy, but it should not reimplement the engine lookup or parse private baked payloads.

## Contract status

This is the Engine-owned production contract for `.fo3d` animation tuples, authored playback speed, one-step aliases, canonical rig conversion, effective-duration metadata, and typed duration lookup. Engine source and tests are normative. Last Frontier and FOnline TLA are discovery and compatibility evidence only; their assets, enum policy, timing policy, and historical tokens do not extend the grammar.

The contract is independently usable without either project checkout. Project evidence is recorded in `BuildTools/ExternalProjectEvidence.json`, while this page re-derives every promoted claim from current Engine source and tests.

## Scope and authority

The owning sources are:

- `Source/Tools/ModelSourceLoader.*` for source skeleton/TRS/clip extraction, validation, and per-bake single-flight caching;
- `Source/Tools/ModelAnimationConverter.*` and `Source/Common/ModelAnimationData.*` for canonical compatibility analysis, Ozz conversion, and the versioned runtime-rig wire contract;
- `Source/Tools/ModelInfoBaker.cpp` for `.fo3d` animation tokens, dependency and geometry validation, effective-duration calculation, alias materialization, `LFMODINF`/`LFOZZRIG`, and `ModelAnimationInfo.foinfo` output;
- `Source/Client/ModelInformation.*`, `ModelAnimation.*`, and `ModelInstance.*` for strict rig loading, one-step alias lookup, timeline/sampling/pose ownership, and loaded-instance duration queries;
- `Source/Common/AnimationInfo.cpp` for duration/bounds decoding and `Source/Common/EngineBase.cpp` for common metadata registration and lookup;
- `Source/Scripting/CommonGlobalScriptMethods.cpp` and `Source/Scripting/ClientCritterScriptMethods.cpp` for script access;
- the model baker, source-loader, animation-data/converter/runtime, skeleton-compatibility, Ozz, client-engine, and common script-method tests for executable examples and failure behavior.

Use [Model Format](model-format.md) for the complete `.fo3d` language, FBX/OBJ inputs, layers, attachments, transforms, materials, cuts, rendering controls, and runtime composition. This page owns the narrower tuple, alias, speed, duration, and script-lookup contract; project gameplay policy remains project-owned.

## Authoring animation tuples

`ModelInfoBaker` bakes non-template `.fo3d` model descriptions. A basename beginning with `TEMPLATE_` is an include-only template: its declarations affect concrete model descriptions after `Include` expansion, but it does not become an independent baked model or metadata section.

The duration path uses these declarations:

```text
Anim <state> <action> <model-file> <animation-name>
AnimSpeed <state> <action> <positive-playback-factor>
AllowAnimationGeometry <external-animation-file>
StateAnimEqual <input-state> <resolved-state>
ActionAnimEqual <input-action> <resolved-action>
```

`<state>` and `<action>` must resolve to current `CritterStateAnim` and `CritterActionAnim` enum values. Prefer named enum entries in authored files because they expose intent and are checked by the same name resolver as numeric values.

An `Anim` declaration identifies one clip by a `(state, action)` tuple:

```text
Anim CritterStateAnim::Unarmed CritterActionAnim::Walk ModelFile Walk
Anim CritterStateAnim::Unarmed CritterActionAnim::Run ANIM_Human.fbx Run
```

- `ModelFile` selects the source named by the model description's `Model` declaration.
- Another filename is resolved relative to the `.fo3d` file.
- `Base` selects the first animation clip in that validated source asset.
- A leading `~` is removed before animation-name validation and duration lookup.
- The referenced baked mesh, current source file, and animation clip must exist. Duplicate tuple declarations are not a useful override mechanism; the first tuple is selected for conversion and duration metadata, so keep each canonical tuple unique.

`AnimSpeed` applies to the same tuple and must be positive with a finite reciprocal. The baker records the effective authored cycle, not the raw clip length:

```text
effective_duration_ms = round((clip_duration_seconds / AnimSpeed) * 1000)
```

For example, a 1-second clip with `AnimSpeed ... 2` produces `500 ms`. The unrounded result must be finite, greater than zero, and no larger than the signed 32-bit millisecond maximum. A positive sub-millisecond result that rounds to zero is rejected. Runtime movement-speed scaling is applied separately while the client plays an animation and is not baked into this value.

### Source conversion and geometry exceptions

`ModelInfoBaker` loads selected source assets through one per-bake `ModelSourceAssetCache`, validates source and baked-mesh freshness, analyzes skeleton compatibility, and converts the selected clips into the canonical runtime rig. Animation-only joints may contribute compatible canonical joints without becoming physical mesh bones. Incompatible roots, parents, transforms, names, limits, or clip data fail before output.

An external `Anim` source should contain transform hierarchy and animation only. Drawable geometry is rejected because the client rig does not consume that duplicate mesh. `AllowAnimationGeometry <file>` is an exact, temporary validation exception for repairing existing exports:

- it resolves from the final concrete `.fo3d`, like an external `Anim` path;
- it must name an exact external source selected by the first effective `Anim` tuple;
- duplicate lines, duplicate resolved paths, non-selected files, and exceptions left after geometry removal fail the bake;
- it is not serialized and has no runtime effect.

Preserve helper/bone names, parents, and tracks while removing geometry from an animation export. Remove the exception with the repaired source instead of treating it as a permanent allowlist.

## One-step aliases

`StateAnimEqual A B` means that an input state `A` is replaced with `B` once before tuple lookup. `ActionAnimEqual` does the same for the action. State and action mappings are independent and both are applied before the lookup.

The rules intentionally match `ModelInformation::GetAnimationIndexEx`:

- each map is consulted once; aliases are not followed recursively;
- an alias has priority over an exact tuple whose state or action is the alias source;
- cycles therefore describe one-step swaps, not recursive chains;
- an alias input is emitted into common metadata only when its one-step result reaches a real positive-duration tuple;
- aliases that do not resolve to such a tuple are omitted.

The native regression uses this compact case:

```text
Anim 1 3 ModelFile Base
Anim 0 3 ModelFile Base
Anim 1 5 ModelFile Base
AnimSpeed 1 3 2
AnimSpeed 0 3 4
AnimSpeed 1 5 5
StateAnimEqual 0 1
ActionAnimEqual 3 5
ActionAnimEqual 5 3
ActionAnimEqual 4 6
```

With a 1-second base clip, input `(0, 3)` resolves once to `(1, 5)` and therefore returns `200 ms`. The exact authored `(0, 3)` value of `250 ms` is not reachable because both input components are alias sources. Input action `4` resolves to `6`, for which no tuple exists, so it has no metadata entry.

## Bake output and distribution

`ModelInfoBaker::BakeFiles` produces two related outputs: each concrete `.fo3d` becomes versioned `LFMODINF` with a required `LFOZZRIG` runtime payload, and the selected model set also produces `ModelAnimationInfo.foinfo` for common duration and bounds lookup. The metadata pass:

1. collects every non-template `.fo3d` selected by the baker;
2. expands includes, verifies source/baked-mesh dependency freshness, and reads clip durations from validated source assets;
3. computes positive effective durations;
4. materializes input tuples with the one-step alias rules above;
5. writes model sections in deterministic source-path order.

The generated metadata file uses one section per concrete model path. Parallel `StateAnimations`, `ActionAnimations`, and `DurationsMs` arrays carry the effective durations. Bounds schema version 2 also carries aggregate model bounds, idle-priority view bounds, and parallel per-animation bounds arrays. Both that representation and the model-info/rig byte streams are private baker/runtime contracts. Game scripts must use typed engine methods instead of reading them.

The behavior does not branch on resource `PackName`: any resource pack that selects `ModelInfo` can produce the table. Put the resulting pack on every runtime side that calls the common duration API. A common setup ships model metadata to server and client while keeping client-only mesh, texture, and rendering assets in client packs.

If the selected source set contains no concrete `.fo3d` files, no duration metadata file is written. A concrete model with no positive-duration animation tuples has no duration section, although its model-info output still contains the required validated rig payload for the current schema.

## Runtime and script lookup

`ModelInformation` strictly loads one immutable canonical skeleton, clip set, binding table, remaps, presence/nearest data, and rest-pose contract from the required rig payload. It rejects old unversioned model-info data, missing/partial rigs, identity mismatches, malformed counts, and trailing data; there is no fallback to a legacy pose evaluator. `ModelAnimation` keeps Ozz objects behind the Engine-owned runtime interface, while each `ModelInstance` owns mutable controller timelines, sampled pose buffers, world matrices, linked children, and procedural overrides. Shared `ModelHierarchy` data remains physical mesh topology and never receives mutable pose output.

`BaseEngine` registers `ModelAnimationInfo.foinfo` during startup after prototypes and before metadata registration is finalized. Registration validates the versioned duration and bounds payload, unique model sections, and unique tuples. Each section path is registered in the engine hash storage and becomes the model key used by scripts.

The common API is available on server, client, and mapper runtimes:

```angelscript
timespan cycle = Game.GetModelAnimDuration(
    modelName,
    CritterStateAnim::Unarmed,
    CritterActionAnim::Walk);
```

It returns the baked effective duration for the exact input tuple after aliases were materialized. It returns a zero `timespan` when the metadata file, model, or tuple is absent. A game that uses the value for authoritative timing should define and test its own explicit zero-duration fallback.

The client/mapper-only `Critter.GetModelAnimDuration(state, action)` is a different boundary. It asks the currently loaded 3D model instance, uses the converted runtime clip/controller metadata, and may continue through the client's configured cross-model animation-substitute path. It returns zero for a non-model critter or unresolved animation and throws when 3D support is not built. Use the common `Game` method when server and client need the same baked local-model duration; use the `Critter` method when code specifically needs the rendered instance's client result.

## Timing acceptance matrix

| Layer | What to verify | Required evidence |
|---|---|---|
| Authored tuple | Named state/action, selected source and clip, speed, and intentional aliases | Focused source test plus model bake |
| Baked metadata | Exact effective milliseconds, deterministic aliases, bounds, and pack distribution | Model-baker tests and inspection through the typed common API |
| Client pose | Converted rig, sampled pose, attachments, substitutions, and visual cycle | Visible representative client or viewer scene |
| Gameplay timing | Attack windows, movement cadence, effects, and zero-duration fallback | Project-owned gameplay or integration test on every authoritative side |

A successful bake proves grammar, source compatibility, conversion, and metadata shape. It does not prove that the visible pose is correct or that gameplay timing policy is fair and synchronized. Keep those acceptance claims separate.

## Failure behavior

| Condition | Result |
|---|---|
| Unknown state/action enum value | Model bake fails. |
| `AnimSpeed <= 0` or its reciprocal is not finite | Model bake fails. |
| Effective milliseconds are non-finite, non-positive, above `int32_t` maximum, or round to zero | Model bake fails. |
| Referenced baked mesh, source file, or animation name is absent or stale | Model bake fails. |
| Source skeleton/clip data is malformed or incompatible with the canonical rig | Model bake fails with source/conversion context. |
| Direct attached FBX contains clips | Model bake fails; use a child `.fo3d` with explicit mappings. |
| External animation source contains drawable geometry | Model bake fails unless that exact selected source has a temporary `AllowAnimationGeometry` line. |
| Geometry exception is duplicate, non-selected, duplicate-resolved, or stale | Model bake fails; narrow or remove the exception. |
| Source clip duration is not positive or finite | Source/model validation fails before output. |
| Alias expansion would produce a duplicate output tuple | Model bake fails. |
| No concrete model or no positive tuple | File or model section is omitted. |
| `ModelAnimationInfo.foinfo` is absent at startup | Engine logs an informational message; common lookups return zero. |
| Metadata arrays are malformed, non-positive, or duplicated | Runtime startup registration fails. |
| Model-info or rig payload is missing, old, truncated, inconsistent, or has trailing data | Client loading fails; no legacy runtime fallback is attempted. |
| Model or tuple is absent at query time | The typed common lookup returns zero. |

## Authoring practices

1. Keep one canonical `Anim` tuple for each semantic state/action pair and use aliases only for intentional reuse.
2. Prefer named `CritterStateAnim` and `CritterActionAnim` values; do not encode project meaning in unexplained integers.
3. Put repeated families in include-only templates and keep concrete models small enough to review.
4. Treat `AnimSpeed` as authored playback speed. Do not confuse a model `Speed` property or runtime movement scaling with tuple duration.
5. Keep the pack containing `ModelAnimationInfo.foinfo` available to every side that calls `Game.GetModelAnimDuration`.
6. Query the typed API. Never bind game scripts to the private duration or bounds-array layout.
7. Make zero handling a visible project policy. Missing metadata must not silently become an arbitrary timing constant.
8. Validate both the model bake and the gameplay path that consumes the duration, especially for attacks, movement cadence, or effects.
9. Keep external animation sources geometry-free. Use `AllowAnimationGeometry` only as temporary, exact migration debt and remove it with the repaired export.
10. Preserve helper/bone hierarchy and animation tracks when stripping duplicate geometry; animation-only canonical joints are supported, silent branch pruning is not.
11. Force-bake after source-loader, converter, wire, skeleton, or source-animation changes, then run an incremental bake to prove dependency timestamps settle cleanly.

## Project evidence and extraction rules

Last Frontier currently centralizes named animation tuples, all tuple `AnimSpeed` declarations, five state aliases, and exact temporary geometry exceptions in `Resources/CrittersArt/Critters/TEMPLATE_HumanAnimations.fo3d`. Concrete models such as `CR_HumanMaleNormal.fo3d` include that template; vehicles such as `VH_Jagger.fo3d` and `VH_Snowmobile.fo3d` use current `ActionAnimEqual` declarations. The reusable lessons are named enums, reviewable include templates, deliberate one-step aliases, exact speed tuning, and temporary geometry debt. The project-specific clip catalog, values, combat semantics, and asset layout remain outside this contract.

FOnline TLA provides useful compatibility evidence through `_VBMob.fo3d`, `_VBWeapon.fo3d`, `_VBHuman.fo3d`, and concrete includes such as `VbDog.fo3d`. Its named `Anim` tuples and template composition remain useful observations. However, `_VBHuman.fo3d` also contains historical bare `AnimEqual` declarations that the current Engine parser does not accept; current aliases are `StateAnimEqual` and `ActionAnimEqual`. Its model-level `Speed` declarations are not tuple `AnimSpeed`. Do not copy either historical form into new content.

When an embedding project or TLA revision changes, update the evidence snapshot and re-audit the complete affected files. Promote only behavior that current Engine source and tests confirm. Record useful project policy in that project's documentation, and record rejected legacy patterns as migration evidence rather than silently normalizing them into the Engine contract.

## Project boundary

The engine owns token parsing, source/dependency and compatibility validation, canonical rig conversion, Ozz-backed runtime sampling, effective-duration calculation, one-step local aliases, baked metadata, startup registration, and typed lookups.

The embedding project owns concrete model paths and assets, state/action selection, cross-model substitute configuration, resource-pack composition, combat or movement timing policy, fallback behavior, and semantic tests. Project documentation should link here for the reusable mechanism and document only those project decisions beside it.

## Maintenance triggers

Update this guide, its focused test, the documentation manifest, and external evidence in the same change when any of these surfaces change:

- `.fo3d` animation token parsing, enum resolution, duplicate handling, source selection, or `AnimSpeed` validation;
- source loading, skeleton compatibility, Ozz conversion, rig or model-info wire contracts;
- one-step alias lookup, duration materialization, bounds output, pack distribution, or startup registration;
- `Game.GetModelAnimDuration`, `Critter.GetModelAnimDuration`, substitutions, or zero-result behavior;
- a pinned Last Frontier or TLA revision that changes the cited evidence files.

Re-run the source-derived checks before editing prose. An incoming project commit may be evidence of a practice or migration need, but it is never sufficient proof of Engine behavior by itself.

## Validation routes

For documentation changes:

```bash
python BuildTools/tests/test_docs_model_animation.py
python BuildTools/docs_external_evidence.py --check --verify-sources --last-frontier-root ../ --tla-root ../Workspace/fonline-tla
python BuildTools/docs_api.py --check
python BuildTools/docs_reference.py --check
python BuildTools/docs_site.py --check
python BuildTools/docs_ai_delivery.py --check
python BuildTools/docs_validate.py
```

For engine behavior, build the embedding project's engine unit-test target. Run the focused model baker, mesh-data, source-loader, animation-data/converter/runtime, procedural-pose, skeleton-compatibility, Ozz, client-engine, and `ModelAnimationInfoLookup` cases, then run the complete unit-test target before integrating an engine revision.

After changing `.fo3d` animation declarations, enum metadata, model sources, mesh/rig wire contracts, source loading/conversion, `ModelInfoBaker`, metadata registration, or either script method, force-rebake an affected embedding project and then rerun its incremental bake. Exercise the project path that consumes the animation or duration; a successful resource bake proves syntax/conversion/assets, while visible gameplay or integration testing proves pose, attachments, timing, and presentation.
