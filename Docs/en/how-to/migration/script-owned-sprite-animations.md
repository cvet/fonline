---
layout: default
title: Migrate Script-Owned Sprite Animations
locale: en
document_id: script-owned-sprite-animations
permalink: /Docs/en/how-to/migration/script-owned-sprite-animations.html
---

# Migrate Script-Owned Sprite Animations

### Script-owned 2D critter animations

Remove OnCritterAnimationFrames(modelName,stateAnim,actionAnim,pass,flags,ox,oy,animName) and
OnCritterAnimationFallout(modelName,stateAnim,actionAnim,fStateAnim,fActionAnim,fStateAnimEx,fActionAnimEx,flags).
Projects now own paths, suffixes, retries, anchors, extraction and splices; CritterFramesCache
only caches sheets/tags. Optional FRM decoding and OnCritterAnimationSubstitute are unchanged.
Subscribe to Game.OnCritterAnimationSprite(hstring,CritterStateAnim,CritterActionAnim,uint32&).
Zero permits substitution; a nonzero SpriteSheet handle is copied and consumed, even on
StopChain/exception cleanup. Wrap static images in a sequence; do not reuse transferred handles.

GetSpriteFrameCount/GetSpriteDirectionCount/GetSpriteDuration/GetSpriteFrameOffset/GetSpriteFrameMotion inspect loaded 2D handles.
CreateSpriteSequence(sources,frames,offsets,motions,framesPerDirection,duration) takes equal
nonempty arrays ordered by direction then frame. Sources are handles, frames are zero-based
indices; a source supplies that direction or its only direction. Offsets are absolute pixel
anchors; motions are explicit deltas, including script-owned splice compensation. Length
divides by positive framesPerDirection; direction count is one or the configured full count.
Invalid arrays/handles/indices/kinds and nonpositive/oversized durations throw.
Duration is a timespan with at least one nanosecond per frame and a total no greater
than uint32 milliseconds; native playback retains fractional milliseconds. Results are stopped,
independent sheets sharing atlas pixels/geometry. Free inputs; FreeSprite releases constructed
sheets and caches named resources by name/atlas. Client map-unload cleanup drops unused
named handles before collecting sprite/atlas caches; active handles remain valid.
Missing named resources return zero.
FreeSprite consumes the handle before Stop. A stop failure releases the instance and
propagates to direct callers. Resolver cleanup reports that failure while preserving
the resolver result or its original exception.
Replace the old filename/flags handler with explicit frame selection:

```angelscript
namespace Animations
{
[[ModuleInit]]
void Init()
{
    Game.OnCritterAnimationSprite.Subscribe(Resolve);
}

[[Event]]
void Resolve(hstring model, CritterStateAnim state, CritterActionAnim action, uint& sprId)
{
    uint source = Game.LoadMapSprite("Sprites/Actor.fofrm");

    if (source == 0) {
        return;
    }

    sprId = Game.CreateSpriteSequence(uint[] = {source}, int[] = {0},
        ipos[] = {Game.GetSpriteFrameOffset(source, 0, mdir(0))},
        ipos[] = {Game.GetSpriteFrameMotion(source, 0, mdir(0))},
        1, Game.GetSpriteDuration(source));

    Game.FreeSprite(source);
}
}
```

Managed: regenerate SDK; subscribe `[Event] void Resolve(hstring model,CritterStateAnim state,CritterActionAnim action,ref uint sprId)` from `[ModuleInit]` with `Game.OnCritterAnimationSprite.Subscribe(Resolve)`. Use the same calls with uint[], int[], ipos[], ipos[] and timespan; free inputs, transfer the result.
Native: replace both old resolvers with `ResolveCritterAnimationSprite(hstring,CritterStateAnim,CritterActionAnim) -> shared_ptr<SpriteSheet>`, returning an independent sheet or null when unresolved; substitutions retain their signature. `SpriteSheet::SetFrame(index, sprite, offset, motion)` retains the original shared sprite with independent ordinary offset and per-frame motion metadata; `GetSpriteOffset(index)` reads the directional sheet's anchor. Remove native calls to `AtlasSprite::CopyWithOffset` and its wrapper constructor; pass the original shared sprite and draw offset separately. Script signatures, source offsets, atlas pixels and geometry remain unchanged.
Detect old callers with `rg -n 'OnCritterAnimationFrames|OnCritterAnimationFallout|ResolveCritterAnimationFrames|ResolveCritterAnimationFallout' <source-roots>`; removed members fail compile. Regenerate metadata/bindings/SDK, rebuild client/Mapper/baker/compiler/server, rebake scripts/metadata, restart and package matching outputs. Check directions, poses, two-axis splices, weapon/death/reload timing, source immutability and visible walking. Test_SpriteSheet/Test_ClientEngine own native checks; projects own resource/visual acceptance.
### Project-owned animation playback and preview

Native `CritterStateAnim` and `CritterActionAnim` now declare only `None=0`. Declare the project's other values with their existing numeric identifiers in script metadata; stored animation identifiers need no renumbering.

`OnCritterAnimationInit` now receives `(Critter cr, CritterStateAnim& stateAnim, CritterActionAnim& actionAnim, AbstractItem? contextItem, CritterAnimationContext context, int32 movingSpeed, CritterAnimationFlags& flags, float32& normalizedTime, float32& speed)`. Context describes default, forward/backward movement, left/right turns, queued actions, or preview. The project selects the pair, `Freeze`, normalized freeze position, playback speed, and the `Movement` flag for movement-driven 2D frames. Engine does not infer any of these choices from condition, weapon posture, or action numbers. Zero/None is unresolved; use project substitution or an explicit project fallback.

Move the former `Render.WalkAnimBaseSpeed`, `Render.RunAnimStartSpeed`, and `Render.RunAnimBaseSpeed` to project settings if preserving that policy. Set `Animation.PreviewStateAnim` and `Animation.PreviewActionAnim` explicitly for Mapper, AnimationViewer and canonical model bounds. Both engine defaults are zero. Bounds select the configured pair, falling back to available bounds when that clip is absent; no unarmed/idle heuristic remains. Rebake model metadata after changing the pair. Native ModelInstance animation callbacks now receive context, moving speed and all playback outputs; `GetMovingContext()` replaces the former `GetMovingAnim()`.

The current idle/movement selection and a queued action retain independent playback
controls. A `Freeze` on either relevant selection disables the 3D locomotion layer;
an advancing queued action still uses its own flags, normalized time and speed.
Logical movement is unaffected by disabling that visual layer.

### Project-owned path and draw policy

Set `Critter.PathfindingObstacle` to classify a character as a pathfinding detour independently of life condition. It adds the configured detour cost, not a hard movement block. Set `Critter.DrawFlatten` to select `DrawOrderType.FlatCritter` (numeric value 13); neither property defaults to a game rule. Install project lifecycle/property handlers for new and restored entities and for condition transitions. Projects retaining `DeadDrawNoFlatten` declare that persistent PublicSync bool themselves and carry the former `IsNoFlatten -> DeadDrawNoFlatten` migration rule.

Apply this policy in Mapper as well: initialize loaded maps through `OnEditMapLoad`, cover
new views through the project's animation-init handler, and derive state again from property
setters. Mapper text edits now execute ordinary setters/post-setters; `DrawFlatten` updates
the drawn layer without a manual view rebuild.

### Historical messages and authored resources

Declare `EngineInfoMessage.NetWrongTagSkill=1029` and `NetPermanentDeath=1049` in project metadata when retaining those messages. Info-message transport remains opaque. `View.GlobalDayColorTime` and `View.GlobalDayColor` are removed from native settings. Remove those config keys and store the curve in project code/content, calculate colors in scripts and submit them through `Map.SetDayColors`. `Map.GlobalDayColor` is the current rendered color output and remains a separate generic map property. Fallout font files and radiation launcher assets are no longer delivered from Engine resources; copy required files into project input packs and preserve resource lookup paths. `Android.Icon` is optional and defaults empty: omitted/empty preserves the platform launcher, an explicit PNG retains validation and copying to all launcher densities.

Base aafc7fe74b (2026.1.57-dev), target 2026.1.58-dev: runtime 0.0.76 forbids mixed outputs. Sprite container v2 and Managed ABI 9 remain unchanged. Existing numeric animation/message values and retained property names preserve stored meaning; projects must initialize the new obstacle/draw properties on restored entities. Rollback restores the complete prior binaries/scripts/metadata/pin and project policy together. No engine game-name aliases remain.

### Resource ownership

`ResourceManager` owns sprite factory registration, extension dispatch, copyable templates,
failed-path memoization, invalidation and retries. `SpriteManager` owns rendering, atlases,
batching and live sprite updates. `FileSystem` owns mounted sources and raw file reads.
Factories still retain model/particle textures for borrowed renderer callbacks; this change
does not introduce a shared texture cache or alter those lifetimes.

Construct `SpriteManager` before `ResourceManager`, then `CritterFramesCache` and
`FontManager`; destroy them in reverse order. Factories receive both managers. ResourceManager
subscribes to begin-scene updates at the former factory-update position, before live sprite
updates, and to low-memory cleanup. It detaches both callbacks before destroying its caches
and factories; `ClientEngine::Shutdown` also detaches them explicitly.

`CritterFramesCache` caches project-resolved directional sheets and animation tags.
Mutable critter preview models belong to `MapperEngine`. Neither owner chooses project
filenames, animation identifiers or frame-splice rules.


Native migration: include ResourceManager.h for SpriteFactory and resource APIs; remove the FileSystem argument from SpriteManager, construct `ResourceManager(ptr<FileSystem>, ptr<SpriteManager>)`, and pass it after `ptr<SpriteManager>` to Default/Model/ParticleSpriteFactory and FontManager. Replace ResourceManager::IndexFiles() with LoadDefaultSprites(settings). Route GetCritterAnimFrames/CleanupCritterFrames to `CritterFramesCache(ptr<AnimationResolver>)`; Mapper owns preview methods. Standalone updaters/viewers need their own explicit resource owner. Script signatures, baked sprite version and saved data are unchanged by this ownership refactor; rebuild native consumers together.

### Sprite offsets and frame motion

The terms are distinct: offset is the ordinary sprite offset to its root; motion
is an authored per-frame delta of visual displacement. SpriteSheet stores these
values in `_sprOffset` and `_sprMotion`. `SetFrame(index, sprite, offset, motion)`
assigns each input to its matching field. `GetSpriteOffset(frame)` reads the offset;
`GetSpriteMotion()[frame]` reads motion. Replace the former native
`GetSpriteOffset()` delta span with `GetSpriteMotion()`. The renderer's borrowed optional frame override uses
`SetSpriteOffsetOverride`; its owner and fallback to Sprite::GetOffset are unchanged.

Public script names retain these meanings: `GetSpriteFrameOffset` reads offset,
`GetSpriteFrameMotion` reads a motion delta, and `CreateSpriteSequence` accepts
`offsets, motions` in that order.
Static images keep their sprite offset and have zero motion. Copies retain
independent values and share original atlas sprites.

`CritterHexView::RefreshAnimOffset` derives cumulative visual displacement,
distance-based frame selection, cycle phase and `_offsAnim` compensation from
frame motion deltas. `SetAnimSpr` selects the image and its ordinary offset;
`RefreshOffs` combines compensation, logical interpolation and extra displacement
once. MovingContext remains authoritative. NextX/NextY and SpriteResource v2
retain their meanings. Rebuild native consumers together with the matching
project scripts and metadata for 2026.1.58-dev/runtime 0.0.76.
