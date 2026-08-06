---
layout: default
title: Sprite Root Motion and Walk Cycles
locale: en
document_id: sprite-root-motion
permalink: /Docs/en/how-to/content/sprite-root-motion.html
---

# Sprite Root Motion and Walk Cycles

This guide documents how FOnline carries per-frame 2D sprite displacement through image baking and uses it to align a moving critter's rendered walk or run cycle with authoritative hex movement. It follows the current baker, resource decoder, sprite loader, movement interpolation, client presentation code, and Engine tests. An embedding project owns its source art, animation mapping, movement tuning, and visual acceptance tests.

## Contract status

This is the Engine-owned production contract for 2D per-frame displacement, baked offset transport, movement-driven frame selection, cycle-phase transfer, and stop-time offset normalization. Engine source and tests are normative. Last Frontier and FOnline TLA are discovery and compatibility evidence only; their assets, visual policy, and historical implementation names do not extend this contract.

The contract is independently usable without either project checkout. Project evidence is pinned in `BuildTools/ExternalProjectEvidence.json`, while every reusable claim on this page is re-derived from current Engine source or tests.

This is a 2D movement-presentation contract. General PNG/TGA and legacy import, FOFRM composition, baked records, stock runtime loading, atlases, and caches are owned by [Image And Sprite Formats](image-format.md). It is independent of `.fo3d` skeletal animation and the duration metadata documented in [Model Animation](model-animation.md).

## Scope and authority

The owning sources are:

- `Source/Tools/ImageBaker.*` for `FrameShot::NextX` / `NextY`, source-format import, transformations, and baked sprite serialization;
- `Source/Common/SpriteResource.*` for the shared SpriteResource v2 frame decoder and `NextOffset` transport;
- `Source/Client/DefaultSprites.*` for `SpriteSheet::_sprOffset`, baked loading, shared frames, and per-direction sheets;
- `Source/Common/Movement.*` for authoritative time/path progress and integer-pixel `HexOffset` construction;
- `Source/Common/Geometry.*` for lossless hex/offset re-splitting and passability-aware normalization;
- `Source/Client/CritterHexView.*` for cycle anchoring, movement displacement, frame selection, rendered animation offsets, and stop-time normalization;
- `Source/Client/ResourceManager.cpp` for animation selection, frame extraction, merges, cloning, and offset preservation;
- `Source/Tests/Test_ImageBaker.cpp` and `Source/Tests/Test_Geometry.cpp` for format-level offset and geometry-normalization coverage.

Project documents and content repositories are useful integration evidence, but they are not the authority for reusable Engine behavior.

## Three independent positions

A moving 2D critter involves three related values:

1. **Logical movement**: `MovingContext` owns path progress, current hex, direction, completion, and movement speed.
2. **Sub-hex interpolation**: `MovingContext::BuildProgress` converts the smooth segment position to integer pixel `HexOffset` values. `CritterHexView::ProcessMoving` applies the logical hex and offset.
3. **Authored sprite displacement**: each animation frame carries an integer `(NextX, NextY)` delta. `CritterHexView` chooses a frame and computes `_offsAnim` so the rendered sprite follows the authored walk cycle while logical movement remains authoritative.

Sprite root motion is presentation data. It does not move the entity, choose a path, change movement speed, authorize a hex transition, or replicate movement over the network.

## Authoring per-frame offsets

`ImageBaker::FrameShot` stores:

```cpp
int16_t NextX;
int16_t NextY;
```

Each pair is the rendered displacement contributed by one frame, in pixels. For a direction-specific animation with `N` frames:

```text
delta[i] = (NextX[i], NextY[i])
accum[i] = sum(delta[0..i])
T        = sum(delta[0..N-1])
```

`T` is the authored displacement of one complete cycle. It may cover less or more than one hex and need not be parallel to one grid-step vector. The runtime projects movement onto `T`; it does not assume a particular hex pitch or cycle length.

Format-specific image loaders derive these values from their source records. Use [FOFRM grammar](image-format.md#fofrm-grammar) for the complete descriptor contract. For root-motion authoring, the current parser accepts both lower-case and camel-case frame keys:

```ini
[dir_0]
next_x_0 = 4
next_y_0 = -2
NextX_1 = 3
NextY_1 = -1
```

Nested FOFRM references add the referencing frame's offset to the selected nested frame. Other supported formats carry or derive offsets according to their own importer. Missing values normally become zero; inspect the format-specific `ImageBaker` loader instead of assuming that every source format exposes the same authored fields.

Offsets are direction-specific. Image baking may mirror, crop, or compose frames. Client resource selection may extract the first or last frame, clone a sheet, or merge a base and extra Fallout animation. A merge copies both sheets and rebases the first extra frame by subtracting the base cycle total independently on `x` and `y`. These transformations are private Engine behavior, so validate the selected baked sheet rather than editing the baked stream.

## Baked and runtime representation

`ImageBaker::BakeCollection` writes each concrete post-transformation frame's `NextX` and `NextY` as signed 16-bit values in the SpriteResource v2 frame record. A deduplicated shared-frame record points to an earlier frame instead. `ReadSpriteResource` decodes the pair into `SpriteResourceFrameData::NextOffset` together with the frame's draw offset, dimensions, pixels, and optional mesh.

`DefaultSpriteFactory::LoadSprite` creates a direction-specific `SpriteSheet` and copies each concrete frame's decoded `NextOffset` into `_sprOffset`. A shared frame copies the referenced sprite and its already-decoded offset. `SpriteSheet::GetSprOffset()` exposes a read-only span to client runtime code.

This binary layout and `_sprOffset` storage are private Engine contracts. Game scripts and tools should author supported source formats and consume rendered behavior, not parse or patch the baked stream.

## When root motion drives a critter

The movement-driven branch in `CritterHexView::Process` is active only when all of these are true:

- the critter uses a 2D sprite rather than a loaded 3D model;
- no explicit queued action animation is currently active;
- the critter has an active `MovingContext`;
- the resolved locomotion sheet reports `CritterActionAnim::Walk` or `CritterActionAnim::Run`.

In that branch, movement position selects the frame. Elapsed animation time does not advance the walk/run cycle independently.

For non-walk/run animations, `SetAnimSpr` accumulates offsets through the selected frame into `_offsAnim`. This lets an authored action visually shift while the logical critter remains anchored. A walk/run sheet with no active movement is not treated as a free-standing root-motion command.

## Continuous movement displacement

`EvaluateMovementDisplacement` measures the critter's current integer-pixel position relative to the start of the active `MovingContext`:

```text
pos = GetHexOffset(start_hex, current_hex)
    + current_hex_offset
    - start_hex_offset
```

The logical hex and `HexOffset` change in opposite directions when movement crosses a hex boundary, so their sum remains spatially continuous. This lets the animation phase survive logical hex snaps.

`MovingContext::BuildProgress` computes the smooth segment interpolation and rounds each `HexOffset` component to an integer pixel. It intentionally does not clamp the result to one hex. Rounded path progress can lag the smooth position, and client reconciliation can fold an inter-hex delta into rapid stop/start input, so a valid moving offset may span more than one hex. Rendering remains correct because the sprite position is the sum of `current_hex + offset`; clamping would break that invariant. The light fan bounds its own copy in `CritterHexView::RefreshOffs` instead.

## Movement stop and offset normalization

`SetMoving` and `StopMoving` clear the sheet anchor and phase displacement. `StopMoving` then calls `CritterHexView::NormalizeHexOffset` to cash whole-hex displacement out of a potentially large `HexOffset` while preserving the rendered world position.

`GeometryHelper::NormalizeHexOffset` re-splits the same pixel position into the nearest logical hex plus a sub-hex remainder. `CritterHexView` supplies a predicate that rejects map fields whose `MoveBlocked` flag is set. The client therefore follows these rules:

- if normalization stays on the current hex, the remainder may be updated without consulting passability;
- if the nearest target hex is inside the map and movable, the logical hex and remainder are updated, then sprite offsets are refreshed;
- if the target is outside the map or blocked, normalization is refused and the existing hex/offset pair remains unchanged;
- both successful representations draw at the same world position, so a normal stop produces no visual jump.

Do not clamp a live movement offset to emulate this lifecycle step. Multi-hex offsets are valid during interpolation; normalization is a stop-time re-partition with map-aware acceptance.

## Anchor and cycle phase

`CritterHexView` stores a sheet-specific anchor:

```cpp
nptr<const SpriteSheet> _walkAnchorAnim;
ipos32 _walkAnchorDisp;
```

For the active sheet:

```text
rel             = pos - anchor
rel_dot_total   = dot(rel, T)
total_dot_total = dot(T, T)
cycle_proj      = rel_dot_total mod total_dot_total
```

Negative modulo results are wrapped into `[0, total_dot_total)`. Therefore `cycle_proj / total_dot_total` is the current phase in `[0, 1)` without floating-point projection.

The first sheet in a new movement anchors at the current `pos`, giving phase zero. A later movement receives a fresh anchor because both lifecycle methods clear the previous sheet and displacement.

## Frame selection

`EvaluateMovementFrameIndex` projects each cumulative authored displacement onto `T`:

```text
accum_dot_total[i] = dot(accum[i], T)
```

It chooses the first frame with the smallest absolute distance:

```text
abs(accum_dot_total[i] - cycle_proj)
```

The selected frame is therefore the authored pose closest to the critter's current position along the cycle axis. The algorithm uses integer `int32_t` / `int64_t` arithmetic for displacement and dot products.

If `T` is zero, `dot(T, T)` is zero and the function returns frame `0`. The walk/run offset branch also leaves `_offsAnim` at zero. A zero-total cycle is valid fallback data, but it cannot spatially drive a walk cycle.

## Rendered offset

For non-zero `T`, `SetAnimSpr` computes the integer cycle number with floor division, including negative projected displacement:

```text
cycle_number = floor(rel_dot_total / total_dot_total)
cycle_start  = anchor + cycle_number * T
offs_anim    = (cycle_start - pos) + accum[i]
```

The normal map sprite position already includes logical hex position and `HexOffset`. Adding `offs_anim` cancels linear movement inside the selected frame and places the sprite at the frame's authored cumulative position. The visual advances when the selected frame changes, by the next authored delta, while logical movement continues underneath.

This is alignment, not animation-authored locomotion. Changing `NextX` / `NextY` changes visual stepping and phase selection but cannot make the server-side entity travel farther or faster.

## Direction and sheet changes

Turning during one `MovingContext` usually resolves a different direction-specific `SpriteSheet` with a different `T` and possibly a different frame count. Restarting at phase zero would visibly reset the gait on each turn.

When the resolved sheet pointer changes, `Process` preserves the old wrapped phase. It shifts the new anchor so projection onto the new cycle vector begins at the same fraction:

```text
old_phase      = old_cycle_proj / dot(T_old, T_old)
new_anchor     = pos - round(old_phase * T_new)
```

The implementation performs the component calculations with signed integer rounding. If the old total vector is zero, there is no phase to preserve and the new sheet anchors at the current position.

This logic applies to any resolved walk/run sheet change, not only a direction change. Content substitution or resource selection that returns a different sheet uses the same re-anchoring path.

## Failure and fallback behavior

| Condition | Current result |
|---|---|
| Missing FOFRM frame offset | The parser uses zero for that component. |
| Offset exceeds signed 16-bit range during baking | Numeric conversion fails instead of silently wrapping. |
| Baked sprite has zero frames or directions | Client loading fails validation. |
| Baked frame is a sprite reference | The referenced frame's sprite and offset are copied. |
| Walk/run cycle total `T` is zero | Frame `0` is selected and movement root-motion offset remains zero. |
| No active movement | Walk/run root motion does not move the logical or rendered critter through this branch. |
| Non-walk/run animation | Offsets accumulate visually through the selected frame. |
| 3D model critter | The 2D sprite root-motion branch is skipped. |
| Stop-time target hex is outside the map or blocked | Normalization is refused; the current logical hex and offset remain unchanged. |

## Visual acceptance matrix

| Route | Evidence to collect | Failure signal |
|---|---|---|
| Straight walk and run | Stable cadence over several complete cycles at each supported speed | Foot sliding, backward pose progression, or periodic jump |
| Every authored direction | Directionally correct displacement and comparable cycle length | Mirrored axis, diagonal drift, or inconsistent pace |
| Turn during movement | Phase continues across sheet replacement | Gait restarts or sprite snaps on each turn |
| Rapid stop/start | Logical hex catches up without a rendered jump | Growing offset, stranded idle sprite, or blocked-path resync loop |
| Hex boundary and reconciliation | Sprite world position stays continuous while hex/offset repartition | Cell-edge sticking or one-hex jump |
| Extraction, merge, or substitution | Selected sheet retains intended per-axis cumulative offsets | First appended frame jumps or one axis is rebased incorrectly |
| Zero-total fallback | Stable frame `0` with no root-motion offset | Divide-by-zero, unstable frame choice, or drift |
| Supported zoom/renderers | Foot placement remains acceptable in the project scene | Zoom-dependent jitter or unacceptable cadence |

Automated baker and geometry tests establish data and normalization behavior. They do not replace visible client acceptance for gait quality.

## Authoring practices

1. Treat `NextX` / `NextY` as per-frame visual displacement, not as absolute frame coordinates.
2. Author and inspect every movement direction. Keep cycle vectors directionally consistent so turns preserve a believable phase.
3. Keep the sum `T` non-zero for locomotion sheets that should spatially drive a gait.
4. Match offset order to pose order. A large correction concentrated in one frame produces a visible jump even when the total cycle is correct.
5. Validate mirroring, extraction, merging, and first/last-frame substitutions because those operations can alter accumulated offsets.
6. Do not tune path speed by changing root-motion pixels. Tune authoritative movement through project movement rules and then author the visual cycle to match.
7. Test rapid direction changes and movement stop/start. Those paths exercise anchor transfer and offset normalization, not only steady straight movement.
8. Test at the project's supported zoom, geometry, and renderer configurations. The algorithm is reusable, but acceptable visual cadence and foot sliding are content decisions.

## Project evidence and extraction rules

The pinned Last Frontier snapshot routes 2D locomotion through `Docs/ContentWorkflow.md` and requires Engine changes to be reconciled through `Docs/DocumentationMaintenance.md`. Its current character pipeline is predominantly `.fo3d`; `Docs/CharacterGenerator.md` skips imported 3D `*_RM` translation clips because Engine movement owns world translation. That 3D rule is a useful ownership boundary, not evidence for concrete 2D `NextX` / `NextY` values.

The audited Last Frontier and FOnline TLA resource trees contain no current authored `.fofrm` or legacy FRM-family sprite assets from which a non-zero production cycle can be verified. TLA's `Docs/Animation.md` preserves useful historical reasoning about walk-cycle projection, but its `raw_ptr<const SpriteSheet>` and `DefaultSpriteFactory::LoadAnimation` names are stale. Current Engine uses `nptr<const SpriteSheet>` and `DefaultSpriteFactory::LoadSprite`.

Therefore no external offset value, gait quality claim, or project-specific speed is promoted into the reusable contract. Extract only cross-project concepts, record exact snapshot paths and commits in the evidence model, re-derive the result from current Engine source, and keep project visual acceptance in the owning project.

## Project boundary

The Engine owns source-format import, baked offset transport, direction-specific sprite sheets, authoritative movement interpolation, anchor/phase math, frame selection, final client-side alignment, and stop-time hex/offset normalization.

The embedding project owns sprite assets, FOFRM composition, animation substitutions, state/action mapping, movement speeds, geometry choice, visual quality bars, and scene-level regression coverage. A project guide may document its concrete art pipeline and tuning values, but should link here rather than restating the runtime algorithm.

## Maintenance triggers

Re-audit this page and its focused tests when a change touches any of these surfaces:

- `FrameShot::NextX` / `NextY`, SpriteResource versioning, `NextOffset`, shared-frame behavior, or `SpriteSheet::_sprOffset`;
- FOFRM/legacy image import, mirroring, cropping, frame extraction, animation merge, cloning, or substitution;
- `MovingContext::BuildProgress`, hex/offset interpolation, prediction reconciliation, or `GeometryHelper::NormalizeHexOffset`;
- `CritterHexView` movement lifecycle, walk/run activation, anchor transfer, frame selection, `_offsAnim`, map passability, or light-offset bounding;
- a pinned Last Frontier or TLA update that changes relevant assets, project policy, or historical evidence.

For an external project or Engine update, fetch first, record the old/new commit range, audit the complete incoming range, update `BuildTools/ExternalProjectEvidence.json`, regenerate the site/AI/localization artifacts in dependency order, and keep this page independent of project-only helpers.

## Validation routes

For documentation changes:

```bash
python BuildTools/tests/test_docs_sprite_root_motion.py
python BuildTools/docs_external_evidence.py --check
python BuildTools/docs_localization.py --check
python BuildTools/docs_site.py --check
python BuildTools/docs_ai_delivery.py --check
python BuildTools/docs_validate.py
```

For offset import and baked transport, run the `ImageBaker` unit-test cases in the embedding project's Engine test binary. `Test_ImageBaker.cpp` covers signed FRM offsets, FOFRM lower/camel-case keys, nested composition, mirroring, frame selection, and baked round trips. `Test_Geometry.cpp` covers successful re-splitting, blocked-target refusal, same-hex behavior, unrestricted normalization, and outside-map refusal.

The current native suite does not expose a focused `CritterHexView` root-motion fixture. After changing `MovingContext`, `CritterHexView`, `SpriteSheet`, `ResourceManager` animation selection, or authored locomotion offsets, run the [visual acceptance matrix](#visual-acceptance-matrix) in a visible client scene. Baking proves the data path, geometry tests prove re-splitting, and the visible scene proves gait alignment.
