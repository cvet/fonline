---
layout: default
title: Sprite Frame Offsets and Walk Cycles
locale: en
document_id: sprite-root-motion
permalink: /Docs/en/how-to/content/sprite-root-motion.html
---

# Sprite Frame Offsets and Walk Cycles

FOnline transports baked 2D frame deltas and aligns walk/run poses with authoritative
hex movement. Projects own art, animation mapping, tuning and visual acceptance.

## Contract status

Engine source/tests define this independently usable 2D contract: offset transport,
movement-driven frame selection, phase transfer and stop normalization. Last
Frontier/TLA are discovery and compatibility evidence, pinned in
`BuildTools/ExternalProjectEvidence.json`; their art, policy and historical names
do not extend it. Re-derive reusable claims from Engine source/tests.

[Image And Sprite Formats](image-format.md) owns import, FOFRM, containers,
loading, atlases and caches. `.fo3d` animation/durations belong to
[Model Animation](model-animation.md).

## Scope and authority

MovingContext route/elapsed/lease intervals and look-ahead use timespan; wire ms
and distance units stay unchanged. Speed rebasing retains whole-ms offset rounding.

The owning sources are:

- `Source/Tools/ImageImport.*` for `FrameShot::Motion` and the shared loader context;
- `Source/Tools/ImageBaker.*` for coordination, transformations, and baked sprite serialization; adjacent `ImageBaker-*.h/.cpp` pairs own source-format import;
- `Source/Common/SpriteResource.*` for the shared SpriteResource v3 frame decoder and `NextOffset` transport;
- `Source/Client/DefaultSprites.*` for `SpriteSheet::_sprOffset` / `_sprMotion`, baked loading, shared frames, and per-direction sheets;
- `Source/Common/Movement.*` for authoritative time/path progress and integer-pixel `HexOffset` construction;
- `Source/Common/Geometry.*` for lossless hex/offset re-splitting and passability-aware normalization;
- `Source/Client/AnimationMovement.*` for pure frame-delta accumulation, cycle projection, phase transfer, frame selection and visual compensation;
- `Source/Client/CritterHexView.*` for the path-relative anchor, movement displacement, application of frame-offset compensation and stop-time normalization;
- `Source/Client/CritterFramesCache.cpp` for caching project-resolved directional sheets and animation tags; projects select, extract and splice frames through the generic sprite API;
- `Source/Tests/Test_Mapper.cpp`, `Source/Tests/Test_ImageBaker.cpp` and `Source/Tests/Test_Geometry.cpp` for actual critter cycle/alignment behavior, format transport and normalization coverage.

Project documents and content repositories are useful integration evidence, but they are not the authority for reusable Engine behavior.

## Three independent positions

A moving 2D critter involves three related values:

1. **Logical movement**: `MovingContext` owns path progress, current hex, direction, completion, and movement speed.
2. **Sub-hex interpolation**: `MovingContext::BuildProgress` converts the smooth segment position to integer pixel `HexOffset` values. `CritterHexView::ProcessMoving` applies the logical hex and offset.
3. **Authored sprite displacement**: each animation frame carries an integer `(Motion.x, Motion.y)` delta. `CritterHexView` chooses a frame and computes `_offsAnim` so the rendered sprite follows the authored walk cycle while logical movement remains authoritative.

Sprite frame offsets are presentation data. They do not move the entity, choose a path, change movement speed, authorize a hex transition, or replicate movement over the network.

<a id="authoring-per-frame-offsets"></a>

## Authoring per-frame motion

`ImageImport::FrameShot` stores:

```cpp
isize32 Size;
ipos32 Motion;
ipos32 Offset;
```

`Size` is the bitmap size, `Offset` is independent draw placement, and
`Motion` is a per-frame animation delta in pixels, accumulated during playback. Each pair contributes pixel displacement in one direction's `N`-frame cycle:

```text
delta[i] = Motion[i]
accum[i] = sum(delta[0..i])
T        = sum(delta[0..N-1])
```

`T` is the cycle displacement. Its length and angle need not match a grid step;
runtime projects movement onto it without assuming a hex pitch or cycle length.

FRM/FRx frame pairs and ART DeltaX/DeltaY supply motion. BAM centers and SPR
bounding-box/center calculations supply `Offset`, with zero imported motion.
PNG, TGA, MOS, RIX, ZAR and TIL do not import movement. FOFRM preserves them unless
Motion/FrameMotions explicitly replaces them: signed int16 X Y, one pair per
source reference before flattening. Bitmap transforms never rotate/scale movement.
Its optional FrameOffsets pairs adjust draw placement (`Offset + FrameOffsets[i]`)
without accumulation or root motion; NextX_N/NextY_N are refused. See
[FOFRM grammar](image-format.md#fofrm-grammar) for pair counts, section scope and nesting.

ART preserves DeltaX/DeltaY as Motion, with mirrored signs. Hot spots align
the canvas and sequence placement. Source retains both; ordinary composition
discards child sequence placement and retains frame placement/deltas.
See [ART](image-format.md#art).

Offsets are direction-specific. Projects select frames and splice sequences through
the [generic sprite API](image-format.md#script-owned-2d-critter-animations). The Engine
does not supply Fallout filename, trim or splice policy.

## Baked and runtime representation

`ImageBaker::BakeCollection` writes each logical frame's checked signed16 Motion components,
int32 placement and positive uint32 duration into SpriteResource v3. Shared slots
reference earlier pixels/geometry, retaining their own movement, placement and time.
`ReadSpriteResource` decodes movement into `SpriteResourceFrameData::NextOffset`.

`DefaultSpriteFactory::LoadSprite` converts placement to logical canvas coordinates
and stores it independently from each record's `NextOffset` motion. A shared record
uses the referenced sprite's logical placement plus the difference between its own
and the referenced record's baked `Offset`; chains preserve that anchor. Each
direction owns its count and timeline.

`SpriteSheet::GetSpriteOffset(frame)` reads the ordinary sprite offset to the root
from `_sprOffset`. `GetSpriteMotion()[frame]` reads the per-frame visual displacement
delta from `_sprMotion`. `SetFrame(index, sprite, offset, motion)` assigns each
input to its matching field. Sheet copies retain independent values.
`CritterHexView::RefreshAnimOffset` delegates motion accumulation and pose alignment
to `AnimationMovement`, retaining the active sheet and path-relative anchor. There is no separate motion object or
animation-driven movement system.

`Game.GetSpriteFrameMotion` exposes a frame delta; `Game.CreateSpriteSequence` stores
the supplied `motions` separately from absolute draw offsets. Projects own frame
selection, cumulative last-frame displacement and sequence splices.

Created sheets share original `AtlasSprite` frames. Independent absolute draw anchors
live in `SpriteSheet::_sprOffset` / `_sprMotion`. `SetAnimSpr` selects only the image and draw
anchor. `RefreshAnimOffset` selects a movement-driven pose and its presentation correction
together before image application; frozen/timed frame indices come from playback.
`RefreshOffs` combines `HexOffset + extra displacement + _offsAnim` once.
MapSprite applies the draw anchor only to sprite placement, keeping it out of the
motion cycle and logical world root. None of these values changes path progress.

<a id="when-root-motion-drives-a-critter"></a>

## When movement selects the frame

The movement-driven branch in `CritterHexView::Process` is active only when all of these are true:

- the critter uses a 2D sprite rather than a loaded 3D model;
- the critter has an active `MovingContext`;
- the project returned `CritterAnimationFlags::Movement` and did not return `Freeze` from `OnCritterAnimationInit`.

In that branch, movement position selects the frame. Elapsed animation time does not advance the walk/run cycle independently.

Without the Movement flag, `RefreshAnimOffset` accumulates motion through the selected frame into `_offsAnim`. This lets an authored action visually shift while the logical critter remains anchored. A movement-driven sheet with no active movement is not treated as an entity-movement command.

## Continuous movement displacement

`EvaluateMovementDisplacement` measures the critter's current integer-pixel position relative to the start of the active `MovingContext`:

```text
pos = GetHexOffset(start_hex, current_hex)
    + current_hex_offset
    - start_hex_offset
```

The logical hex and `HexOffset` change in opposite directions when movement crosses a hex boundary, so their sum remains spatially continuous. This lets the animation phase survive logical hex snaps.

`MovingContext::BuildProgress` computes the smooth segment interpolation and rounds each `HexOffset` component to an integer pixel. It intentionally does not clamp the result to one hex. Rounded path progress can lag the smooth position, and client reconciliation can fold an inter-hex delta into rapid stop/start input, so a valid moving offset may span more than one hex. Rendering remains correct because the sprite position is the sum of `current_hex + offset`; clamping would break that invariant.

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

`AnimationMovement::EvaluateWalkFrame` projects each cumulative authored displacement onto `T`:

```text
accum_dot_total[i] = dot(accum[i], T)
```

It chooses the first frame with the smallest absolute distance:

```text
abs(accum_dot_total[i] - cycle_proj)
```

Select the closest pose along the cycle axis using integer `int32_t`/`int64_t` displacement and dot products.

If `T` is zero, `dot(T, T)` is zero and frame `0` is selected. The walk/run offset branch also leaves `_offsAnim` at zero. A zero-total cycle is valid fallback data, but it cannot spatially drive a walk cycle.

## Rendered offset

For non-zero `T`, `AnimationMovement::EvaluateWalkFrame` computes the integer cycle number with floor division, including negative projected displacement:

```text
cycle_number = floor(rel_dot_total / total_dot_total)
cycle_start  = anchor + cycle_number * T
offs_anim    = (cycle_start - pos) + accum[i]
```

Map placement includes logical hex position and `HexOffset`. `offs_anim` cancels
within-frame linear movement, placing the sprite at the cumulative authored offset.
Frame changes advance the visual by the next delta while logical movement continues.

Frame-offset pixels affect visual stepping/phase, never server travel distance or speed.

## Direction and sheet changes

Turning during one `MovingContext` usually resolves a different direction-specific `SpriteSheet` with a different `T` and possibly a different frame count. Restarting at phase zero would visibly reset the gait on each turn.

When the resolved sheet changes, `RefreshAnimOffset` calls `AnimationMovement::TransferWalkPhase` before either frame selection or offset calculation, including `RefreshView` before the next `Process`. It retains ownership of the new anchor, shifting it so projection onto the new cycle vector begins at the same fraction:

```text
old_phase      = old_cycle_proj / dot(T_old, T_old)
new_anchor     = pos - round(old_phase * T_new)
```

The module-local `ScaleWalkPhase` helper performs exact component scaling with signed nearest-integer rounding (ties away from zero), without overflowing an intermediate phase numerator. If the old total vector is zero, there is no phase to preserve and the new sheet anchors at the current position.

This logic applies to any resolved walk/run sheet change, not only a direction change. Content substitution or resource selection that returns a different sheet uses the same re-anchoring path.

## Failure and fallback behavior

| Condition | Current result |
|---|---|
| Missing FOFRM FrameOffsets | Zero draw-placement adjustments; imported root-motion deltas are unchanged. |
| Motion exceeds signed 16-bit range during serialization | Numeric conversion fails instead of silently wrapping. |
| Baked sprite has zero frames or directions | Client loading fails validation. |
| Baked frame is a sprite reference | The referenced frame's sprite and offset are copied. |
| Movement-driven cycle total `T` is zero | Frame `0` is selected and movement frame-offset offset remains zero. |
| No active movement with Movement and no Freeze | The alignment branch applies no animation correction. |
| No Movement flag | Offsets accumulate visually through the selected frame. |
| 3D model critter | The 2D sprite frame-offset branch is skipped. |
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
| Zero-total fallback | Stable frame `0` with no frame-offset offset | Divide-by-zero, unstable frame choice, or drift |
| Supported zoom/renderers | Foot placement remains acceptable in the project scene | Zoom-dependent jitter or unacceptable cadence |

Automated baker and geometry tests establish data and normalization behavior. They do not replace visible client acceptance for gait quality.

## Authoring practices

1. `Motion` are per-frame pixel deltas, not absolute coordinates.
2. Inspect every direction; keep cycle vectors directionally consistent for turns.
3. Keep `T` nonzero for locomotion sheets that spatially drive gait.
4. Match deltas to poses; a large single-frame correction jumps even with a correct total.
5. Check mirrors, extraction, merges and first/last substitutions for offset changes.
6. Tune speed through authoritative project movement, then match the visual cycle.
7. Test rapid turns and stop/start for anchor transfer and normalization.
8. Test supported zoom, geometry and renderers; cadence/foot-sliding acceptance is project-owned.

## Project evidence and extraction rules

The pinned Last Frontier snapshot routes 2D work through `Docs/ContentWorkflow.md`
and Engine reconciliation through `Docs/DocumentationMaintenance.md`. Its mainly
`.fo3d` pipeline skips `*_RM` translation in `Docs/CharacterGenerator.md` because
Engine owns world movement; this does not prove 2D `Motion` values.

Audited LF/TLA trees contain no current authored `.fofrm` or legacy FRM-family sprite assets
with a verified nonzero production cycle. TLA `Docs/Animation.md` explains projection historically;
its `raw_ptr<const SpriteSheet>`/`DefaultSpriteFactory::LoadAnimation` names are
stale. Current names are `nptr<const SpriteSheet>`/`DefaultSpriteFactory::LoadSprite`.

Extract concepts only; record exact paths/commits in the evidence model, re-derive
them from Engine and keep project speeds, offsets and visual acceptance project-owned.

## Project boundary

The Engine owns source-format import, baked offset transport, direction-specific sprite sheets, authoritative movement interpolation, anchor/phase math, frame selection, final client-side alignment, and stop-time hex/offset normalization.

The embedding project owns sprite assets, FOFRM composition, animation substitutions, state/action mapping, movement speeds, geometry choice, visual quality bars, and scene-level regression coverage. A project guide may document its concrete art pipeline and tuning values, but should link here rather than restating the runtime algorithm.

## Maintenance triggers

Re-audit this page and its focused tests when a change touches any of these surfaces:

- `FrameShot::Motion`, SpriteResource versioning, `NextOffset`, shared-frame behavior, or `SpriteSheet::_sprOffset` / `_sprMotion`;
- FOFRM/legacy image import, mirroring, cropping, frame extraction, animation merge, cloning, or substitution;
- `MovingContext::BuildProgress`, hex/offset interpolation, prediction reconciliation, or `GeometryHelper::NormalizeHexOffset`;
- `AnimationMovement` accumulation, projection, phase transfer, frame selection and visual compensation;
- `CritterHexView` movement lifecycle, project-controlled activation, anchor ownership, `_offsAnim`, map passability;
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

For offset import and baked transport, run the `ImageBaker` unit-test cases in the embedding project's Engine test binary. `Test_ImageBaker.cpp` covers signed FRM offsets, PascalCase FOFRM keys and sections, rejection of obsolete names, nested composition, mirroring, frame selection, and baked round trips. `Test_Geometry.cpp` covers successful re-splitting, blocked-target refusal, same-hex behavior, unrestricted normalization, and outside-map refusal.

`CritterAnimationPreservesProjectSelectionWhileMoving` exercises the actual critter consumer with arbitrary identifiers, explicit movement flags and frozen frames. After changing `MovingContext`, `CritterHexView`, `SpriteSheet`, `CritterFramesCache` or project animation selection, or authored locomotion offsets, run the [visual acceptance matrix](#visual-acceptance-matrix) in a visible client scene. Baking proves the data path, geometry tests prove re-splitting, and the visible scene proves gait alignment.
