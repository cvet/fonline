# Sprite Root Motion and Walk Cycles

This guide documents how FOnline carries per-frame 2D sprite displacement through image baking and uses it to align a moving critter's rendered walk or run cycle with authoritative hex movement. It describes current Engine behavior. An embedding project owns its source art, animation mapping, movement tuning, and visual acceptance tests.

This is a 2D movement-presentation contract. General PNG/TGA and legacy import, FOFRM composition, baked records, stock runtime loading, atlases, and caches are owned by [ImageFormat.md](ImageFormat.md). It is independent of `.fo3d` skeletal model descriptions and the model-duration metadata documented in [ModelAnimation.md](ModelAnimation.md).

## Scope and authority

The owning sources are:

- `Source/Tools/ImageBaker.h` and `ImageBaker.cpp` for `FrameShot::NextX` / `NextY`, source-format import, and baked sprite serialization;
- `Source/Common/SpriteResource.h` and `SpriteResource.cpp` for the shared SpriteResource v2 frame decoder and `NextOffset` transport;
- `Source/Client/DefaultSprites.h` and `DefaultSprites.cpp` for `SpriteSheet::_sprOffset`, baked loading, and per-direction sheets;
- `Source/Common/Movement.cpp` for authoritative time/path progress and integer-pixel `HexOffset` construction;
- `Source/Client/CritterHexView.h` and `CritterHexView.cpp` for cycle anchoring, movement displacement, frame selection, and rendered animation offsets;
- `Source/Client/ResourceManager.cpp` for animation selection, frame transformations, merges, and preservation of sprite offsets;
- `Source/Tests/Test_ImageBaker.cpp` for format-level per-frame offset coverage.

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

Format-specific image loaders derive these values from their source records. Use [ImageFormat.md#fofrm-grammar](ImageFormat.md#fofrm-grammar) for the complete descriptor contract. For root-motion authoring, the current parser accepts both lower-case and camel-case frame keys:

```ini
[dir_0]
next_x_0 = 4
next_y_0 = -2
NextX_1 = 3
NextY_1 = -1
```

Nested FOFRM references add the referencing frame's offset to the selected nested frame. Other supported formats carry or derive offsets according to their own importer. Missing values normally become zero; inspect the format-specific `ImageBaker` loader instead of assuming that every source format exposes the same authored fields.

Offsets are direction-specific. Mirroring, frame extraction, first/last-frame selection, animation merging, and cloning can transform or accumulate them in `ImageBaker` or `ResourceManager`. Validate the baked result after using those operations.

## Baked and runtime representation

`ImageBaker::BakeCollection` writes each concrete post-padding/cropping frame's `NextX` and `NextY` as signed 16-bit values in the SpriteResource v2 frame record. A deduplicated shared-frame record points to an earlier frame instead. `ReadSpriteResource` decodes the pair into `SpriteResourceFrameData::NextOffset` together with the frame's draw offset, dimensions, pixels, and optional mesh.

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

`MovingContext::BuildProgress` computes the underlying smooth segment interpolation and rounds each `HexOffset` component to an integer pixel. Root-motion selection deliberately consumes that integer position, keeping frame choice and final sprite placement in the same coordinate domain.

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

The first sheet in a new movement anchors at the current `pos`, giving phase zero. `SetMoving` and `StopMoving` clear both anchor fields so a later movement starts a fresh cycle.

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

The normal map sprite position already includes logical hex position and `HexOffset`. Adding `offs_anim` cancels linear movement inside the selected frame and places the sprite at the frame's authored cumulative position. The visual advances when the selected frame changes, by the next authored delta, while the logical movement continues underneath.

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

## Authoring practices

1. Treat `NextX` / `NextY` as per-frame visual displacement, not as absolute frame coordinates.
2. Author and inspect every movement direction. Keep the cycle vectors directionally consistent so turns preserve a believable phase.
3. Keep the sum `T` non-zero for locomotion sheets that should spatially drive a gait.
4. Match offset order to pose order. A large correction concentrated in one frame produces a visible jump even when the total cycle is correct.
5. Validate mirroring, extraction, merging, and first/last-frame substitutions because those operations can alter accumulated offsets.
6. Do not tune path speed by changing root-motion pixels. Tune authoritative movement through project movement rules and then author the visual cycle to match.
7. Test rapid direction changes and movement stop/start. Those paths exercise anchor transfer and lifecycle reset rather than only steady straight movement.
8. Test at the project's supported zoom, geometry, and renderer configurations. The algorithm is reusable, but acceptable visual cadence and foot sliding are content decisions.

## Project boundary

The Engine owns source-format import, baked offset transport, direction-specific sprite sheets, authoritative movement interpolation, anchor/phase math, frame selection, and final client-side alignment.

The embedding project owns sprite assets, FOFRM composition, animation substitutions, state/action mapping, movement speeds, geometry choice, visual quality bars, and scene-level regression coverage. A project guide may document its concrete art pipeline and tuning values, but should link here rather than restating the runtime algorithm.

## Validation routes

For documentation changes:

```bash
python BuildTools/tests/test_docs_sprite_root_motion.py
python BuildTools/docs_site.py --check
python BuildTools/docs_ai_delivery.py --check
python BuildTools/docs_validate.py
```

For offset import and baked transport, run the `ImageBaker` unit-test cases in the embedding project's Engine test binary. `Test_ImageBaker.cpp` covers signed FRM offsets, FOFRM lower/camel-case keys, nested composition, mirroring, frame selection, and baked round trips.

The current native suite does not expose a focused `CritterHexView` root-motion fixture. After changing `MovingContext`, `CritterHexView`, `SpriteSheet`, animation selection, or authored locomotion offsets, run a visible client scene with straight movement, turns, stop/start, and both walk/run speeds. Baking proves the data path; the visible scene proves gait alignment.

