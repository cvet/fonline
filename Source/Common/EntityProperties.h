//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "Common.h"

#include "Entity.h"
#include "Geometry.h"

FO_BEGIN_NAMESPACE

class GameProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Game";

    explicit GameProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    // Current synchronized game time shared by the server and clients and persisted by the server.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(synctime, SynchronizedTime);
    // Persistent high-water mark used to allocate durable entity identifiers.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(ident_t, LastEntityId);
    // Persistent high-water mark used to allocate server history-record identifiers.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(ident_t, HistoryRecordsId);
    // Monotonic engine time captured for the current frame.
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(nanotime, FrameTime);
    // Elapsed monotonic time between the current and previous frame advances.
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(timespan, FrameDeltaTime);
    // Frame count measured during the most recently completed one-second sampling interval.
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(int32_t, FramesPerSecond);
    // Last server-assigned identifier for a global-map trip.
    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(uint32_t, LastGlobalMapTripId);
    // Current position of the global day-light cycle, measured in minutes and controlled by game scripts.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int32_t, GlobalDayTime);
};

class PlayerProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Player";

    explicit PlayerProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    // Reports whether this player connection has completed server login.
    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(bool, LoggedIn);
    // Identifier of the critter currently controlled by this player; zero when none is attached.
    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(ident_t, ControlledCritterId);
    // Persistent identifier of the most recently controlled critter, retained after detachment for restoration.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(ident_t, LastControlledCritterId);
};

class ItemProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Item";

    explicit ItemProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    // Maps the legacy Item property name ScriptId to InitScript during Engine migration lookup.
    ///@ MigrationRule Property Item ScriptId InitScript
    // Server initialization function called after the item-init event for a newly created or restored item.
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = ItemInit
    FO_ENTITY_PROPERTY(hstring, InitScript);
    // Maps the legacy Item property name IsStatic to Static during Engine migration lookup.
    ///@ MigrationRule Property Item IsStatic Static
    // Marks map-authored item data as static scenery rather than a dynamic item entity.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, Static);
    // Maps the legacy Item property name Accessory to Ownership during Engine migration lookup.
    ///@ MigrationRule Property Item Accessory Ownership
    // Current placement category: map hex, critter inventory, item container, or nowhere.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ItemOwnership, Ownership);
    // Identifier of the owning map while Ownership is MapHex; zero otherwise.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, MapId);
    // Origin map hex used while the item is placed on a map.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(mpos, Hex);
    // Vertical rendering elevation of the map item.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int16_t, Elevation);
    // Maps the legacy Item property name CritId to CritterId during Engine migration lookup.
    ///@ MigrationRule Property Item CritId CritterId
    // Identifier of the owning critter while Ownership is CritterInventory; zero otherwise.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, CritterId);
    // Maps the legacy Item property name CritSlot to CritterSlot during Engine migration lookup.
    ///@ MigrationRule Property Item CritSlot CritterSlot
    // Maps the legacy Item property name Slot to CritterSlot during Engine migration lookup.
    ///@ MigrationRule Property Item Slot CritterSlot
    // Inventory or equipment slot occupied while the item belongs to a critter.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(CritterItemSlot, CritterSlot);
    // Identifier of the parent item while Ownership is ItemContainer; zero otherwise.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, ContainerId);
    // Project-defined grouping key used to select and merge items inside a container.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(any_t, ContainerStack);
    // Maps the legacy Item property name SubItemIds to InnerItemIds during Engine migration lookup.
    ///@ MigrationRule Property Item SubItemIds InnerItemIds
    // Persistent identifiers of the items directly contained by this item.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, InnerItemIds);
    // Allows equal items to merge into one entity whose Count is greater than one.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, Stackable);
    // Positive quantity represented by this item; non-stackable items must have a value of one.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int32_t, Count);
    // Sprite resource used to render the item on a map.
    ///@ ExportProperty Common Mutable PublicSync Persistent Resource
    FO_ENTITY_PROPERTY(hstring, PicMap);
    // Pixel offset applied to the item's map sprite from its origin hex.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(ipos16, Offset);
    // Wall-corner orientation used by map lighting and transparent-foreground behavior.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(CornerType, Corner);
    // Disables transparent-foreground egg processing for the item's map sprite.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, DisableEgg);
    // Maps the legacy Item property name BlockLines to MultihexLines during Engine migration lookup.
    ///@ MigrationRule Property Item BlockLines MultihexLines
    // Direction and step-count pairs that trace additional footprint hexes from each mesh origin.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(vector<uint8_t>, MultihexLines);
    // Explicit map hexes that share this item's footprint and optional repeated sprite.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(vector<mpos>, MultihexMesh);
    // Mapper coalescing strategy used to build MultihexMesh from compatible item placements.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(MultihexGenerationType, MultihexGeneration);
    // Draws repeated item sprites on footprint hexes generated by MultihexLines.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, DrawMultihexLines);
    // Draws repeated item sprites on the explicit MultihexMesh hexes.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, DrawMultihexMesh);
    // Maps the legacy Item property name IsHidden to Hidden during Engine migration lookup.
    ///@ MigrationRule Property Item IsHidden Hidden
    // Server visibility flag that suppresses a dynamic item from client views while set.
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(bool, Hidden);
    // Hides the current client map sprite without removing the item's map presence.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, HideSprite);
    // Maps the legacy Item property name IsHiddenPicture to AlwaysHideSprite during Engine migration lookup.
    ///@ MigrationRule Property Item IsHiddenPicture AlwaysHideSprite
    // Prevents the item sprite from being drawn at runtime and hides it by default in Mapper.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, AlwaysHideSprite);
    // Maps the legacy Item property name IsNoBlock to NoBlock during Engine migration lookup.
    ///@ MigrationRule Property Item IsNoBlock NoBlock
    // Allows movement through every hex occupied by the item.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, NoBlock);
    // Maps the legacy Item property name IsShootThru to ShootThru during Engine migration lookup.
    ///@ MigrationRule Property Item IsShootThru ShootThru
    // Allows projectile traces through every hex occupied by the item.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, ShootThru);
    // Maps the legacy Item property name IsLightThru to LightThru during Engine migration lookup.
    ///@ MigrationRule Property Item IsLightThru LightThru
    // Allows map light propagation through every hex occupied by the item.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, LightThru);
    // Maps the legacy Item property name IsLight to LightSource during Engine migration lookup.
    ///@ MigrationRule Property Item IsLight LightSource
    // Enables a map light source centered on this item.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, LightSource);
    // Signed percentage intensity of the item's light source.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int8_t, LightIntensity);
    // Radius in hexes reached by the item's light source.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int16_t, LightDistance);
    // Bit mask controlling global, inverse, and direction-blocking behavior of the light source.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(uint16_t, LightFlags);
    // RGBA color emitted by the item's light source.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(ucolor, LightColor);
    // RGBA tint applied when Colorize is enabled; alpha also supplies the default sprite opacity.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(ucolor, ColorizeColor);
    // Maps the legacy Item property name SceneryScript to StaticScript during Engine migration lookup.
    ///@ MigrationRule Property Item SceneryScript StaticScript
    // Server callback invoked when a critter interacts with this static item.
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = ItemStatic
    FO_ENTITY_PROPERTY(hstring, StaticScript);
    // Server callback invoked when a critter enters or leaves this trigger item.
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = ItemTrigger
    FO_ENTITY_PROPERTY(hstring, TriggerScript);
    // Includes the item in map trigger lookup and movement-trigger processing.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, IsTrigger);
    // Sprite resource used to present the item in inventory interfaces.
    ///@ ExportProperty Common Mutable PublicSync Persistent Resource
    FO_ENTITY_PROPERTY(hstring, PicInv);
    // Classifies the item as scenery for Mapper filtering and flat-render lighting rules.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, IsScenery);
    // Classifies the item as a wall for Mapper filtering and map presentation.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, IsWall);
    // Classifies the item as a ground or roof tile rendered in a tile layer.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, IsTile);
    // Selects roof rendering instead of ground-tile rendering when IsTile is enabled.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, IsRoofTile);
    // Zero-based ground- or roof-tile draw layer selected for a tile item.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(uint8_t, TileLayer);
    // Maps the legacy Item property name IsFlat to DrawFlatten during Engine migration lookup.
    ///@ MigrationRule Property Item IsFlat DrawFlatten
    // Renders the item in a flat map pass instead of the normal depth-sorted item pass.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, DrawFlatten);
    // Signed Y-hex offset used only when calculating the item's sprite draw order.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(int8_t, DrawOrderOffsetHexY);
    // Maps the legacy Item property name IsNoHighlight to NoHighlight during Engine migration lookup.
    ///@ MigrationRule Property Item IsNoHighlight NoHighlight
    // Prevents the client from applying normal item highlight presentation.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, NoHighlight);
    // Maps the legacy Item property name IsNoLightInfluence to NoLightInfluence during Engine migration lookup.
    ///@ MigrationRule Property Item IsNoLightInfluence NoLightInfluence
    // Prevents ambient map lighting from tinting the item's sprite.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, NoLightInfluence);
    // Marks a blocking item as conditionally passable through the map gag callback.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, IsGag);
    // Maps the legacy Item property name IsColorize to Colorize during Engine migration lookup.
    ///@ MigrationRule Property Item IsColorize Colorize
    // Enables ColorizeColor tinting for the item's map sprite.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, Colorize);
};

class CritterProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Critter";

    explicit CritterProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    // Maps the legacy Critter property name ScriptId to InitScript during Engine migration lookup.
    ///@ MigrationRule Property Critter ScriptId InitScript
    // Server initialization function called after the critter-init event for a newly created or restored critter.
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = CritterInit
    FO_ENTITY_PROPERTY(hstring, InitScript);
    // Identifier of the map currently containing the critter; zero while it is on the global map.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, MapId);
    // Server-assigned identifier shared by members of the same current global-map trip.
    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(uint32_t, GlobalMapTripId);
    // Origin hex currently occupied by the critter on its map.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(mpos, Hex);
    // Sub-hex map-space offset used by movement interpolation and rendering.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ipos16, HexOffset);
    // Vertical rendering elevation of the critter.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int16_t, Elevation);
    // Current facing direction on the map grid.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(mdir, Dir);
    // Persistent identifiers of the items directly held in the critter's inventory.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, ItemIds);
    // Sprite or 3D model resource used to present the critter.
    ///@ ExportProperty Common Mutable PublicSync Persistent Resource
    FO_ENTITY_PROPERTY(hstring, ModelName);
    // Radius in hexes occupied by the critter in movement, pathfinding, and distance checks.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(int32_t, Multihex);
    // 3D model scale in thousandths; zero selects the default scale of 1000.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int32_t, ScaleFactor);
    // Optional first visibility-event distance in hexes; zero disables this distance group.
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(int32_t, ShowCritterDist1);
    // Optional second visibility-event distance in hexes; zero disables this distance group.
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(int32_t, ShowCritterDist2);
    // Optional third visibility-event distance in hexes; zero disables this distance group.
    ///@ ExportProperty Server Mutable Persistent
    FO_ENTITY_PROPERTY(int32_t, ShowCritterDist3);
    // Fixed-size client values passed to the 3D model layer and animation resolver.
    ///@ ExportProperty Client Mutable
    FO_ENTITY_PROPERTY(vector<int32_t>, ModelLayers);
    // Maps the legacy Critter property name IsControlledByPlayer to ControlledByPlayer during Engine migration lookup.
    ///@ MigrationRule Property Critter IsControlledByPlayer ControlledByPlayer
    // Classifies the critter as player-controllable independently of its current online state.
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(bool, ControlledByPlayer);
    // Reports that this critter is the locally chosen critter on the current client.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(bool, IsChosen);
    // Client-side connection state reported for a player-controlled critter.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(bool, IsPlayerOffline);
    // Reports that this critter currently follows another critter as an attachment.
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(bool, IsAttached);
    // Identifier of the attachment master; zero when the critter is not attached.
    ///@ ExportProperty Common
    FO_ENTITY_PROPERTY(ident_t, AttachMaster);
    // Hides the current client map sprite without removing the critter's map presence.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, HideSprite);
    // Current server movement speed published for active movement.
    ///@ ExportProperty Server
    FO_ENTITY_PROPERTY(int32_t, MovingSpeed);
    // Visibility detail mode assigned to this critter from the current client's viewpoint.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(CritterVisibilityMode, VisibilityMode);
    // Maps the legacy Critter property name Cond to Condition during Engine migration lookup.
    ///@ MigrationRule Property Critter Cond Condition
    // Current life-state condition used by animation, filtering, and gameplay queries.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(CritterCondition, Condition);
    // Vertical pixel adjustment applied to the critter name label.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int16_t, NameOffset);
    // Maximum map-view distance in hexes used by server visibility processing.
    ///@ ExportProperty Common Mutable OwnerSync Persistent
    FO_ENTITY_PROPERTY(int32_t, LookDistance);
    // Maps the legacy Critter property name IsNoFlatten to DeadDrawNoFlatten during Engine migration lookup.
    ///@ MigrationRule Property Critter IsNoFlatten DeadDrawNoFlatten
    // Keeps a dead critter in the normal critter draw order instead of the flattened dead-critter order.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(bool, DeadDrawNoFlatten);
    // Enables a client-side map light source centered on this critter.
    ///@ ExportProperty Client Mutable
    FO_ENTITY_PROPERTY(bool, LightSource);
    // Signed percentage intensity of the critter's client-side light source.
    ///@ ExportProperty Client Mutable
    FO_ENTITY_PROPERTY(int8_t, LightIntensity);
    // Radius in hexes reached by the critter's client-side light source.
    ///@ ExportProperty Client Mutable
    FO_ENTITY_PROPERTY(int16_t, LightDistance);
    // Bit mask controlling global, inverse, and direction-blocking behavior of the light source.
    ///@ ExportProperty Client Mutable
    FO_ENTITY_PROPERTY(uint16_t, LightFlags);
    // RGBA color emitted by the critter's client-side light source.
    ///@ ExportProperty Client Mutable
    FO_ENTITY_PROPERTY(ucolor, LightColor);
};

class MapProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Map";

    explicit MapProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    // Maps the legacy Map property name ScriptId to InitScript during Engine migration lookup.
    ///@ MigrationRule Property Map ScriptId InitScript
    // Server initialization function called after the map-init event for a newly created or restored map.
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = MapInit
    FO_ENTITY_PROPERTY(hstring, InitScript);
    // Identifier of the location that owns this map.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(ident_t, LocId);
    // Zero-based position of this map in its owning location's map list.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(int32_t, LocMapIndex);
    // Persistent identifiers of the non-player critters owned by this map.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, CritterIds);
    // Persistent identifiers of the dynamic items placed directly on this map.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, ItemIds);
    // Identifiers of the baked static items this map instance has dropped; removal is one-way for the life of the instance.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, RemovedStaticItemIds);
    // Width and height of the map grid in hexes.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(msize, Size);
    // Authoring work hex used as the initial Mapper view center.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(mpos, WorkHex);
    // Rounded client viewport offset in map-space pixels.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(ipos32, ScrollOffset);
    // Axial-coordinate rectangle that bounds map scrolling; a zero rectangle leaves the full map available.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(irect32, ScrollAxialArea);
    // Current client map-sprite zoom factor.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(float32_t, SpritesZoom);
    // Target zoom factor toward which the client map view is interpolating.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(float32_t, SpritesZoomTarget);
    // Maps the legacy Map property name CurDayTime to FixedDayTime during Engine migration lookup.
    ///@ MigrationRule Property Map CurDayTime FixedDayTime
    // Map-local day-time override value, conventionally expressed in minutes and interpreted by game scripts.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(int32_t, FixedDayTime);
    // Maps the legacy Map property name DayTime to DayColorTime during Engine migration lookup.
    ///@ MigrationRule Property Map DayTime DayColorTime
    // Map-specific daylight color keyframe times expressed in minutes of the day cycle.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(vector<int32_t>, DayColorTime);
    // Channel-major RGB values for the map's corresponding daylight color keyframes.
    ///@ ExportProperty Common Mutable PublicSync Persistent
    FO_ENTITY_PROPERTY(vector<uint8_t>, DayColor);
    // Current client-side map tint supplied through SetDayColors and consumed by map rendering.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(ucolor, MapDayColor);
    // Current client-side global daylight color supplied through SetDayColors for scripts and effects.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(ucolor, GlobalDayColor);
    // Current daylight capacity percentage for ordinary map light sources, clamped to 0 through 100.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(int32_t, MapDayLightCapacity);
    // Current daylight capacity percentage for light sources flagged as global, clamped to 0 through 100.
    ///@ ExportProperty Client
    FO_ENTITY_PROPERTY(int32_t, GlobalDayLightCapacity);
};

class LocationProperties : public EntityProperties
{
public:
    static constexpr string_view ENTITY_TYPE_NAME = "Location";

    explicit LocationProperties(Properties& props) noexcept :
        EntityProperties(props)
    {
    }

    // Maps the legacy Location property name ScriptId to InitScript during Engine migration lookup.
    ///@ MigrationRule Property Location ScriptId InitScript
    // Server initialization function called after the location-init event for a newly created or restored location.
    ///@ ExportProperty Server Mutable Persistent ScriptFuncType = LocationInit
    FO_ENTITY_PROPERTY(hstring, InitScript);
    // Persistent identifiers of the maps owned by this location, in location-map order.
    ///@ ExportProperty Server Persistent
    FO_ENTITY_PROPERTY(vector<ident_t>, MapIds);
};

FO_END_NAMESPACE
