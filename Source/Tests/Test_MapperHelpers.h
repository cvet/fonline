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

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "AnimationViewer.h"
#include "Application.h"
#include "Baker.h"
#include "ConfigFile.h"
#include "EffectBaker.h"
#include "ImGuiStuff.h"
#include "MapView.h"
#include "Mapper.h"
#include "ParticleBaker.h"
#include "ParticleEditor.h"
#include "ParticleViewer.h"
#include "SettingsStorage.h"
#include "SparkParticleEditor.h"
#include "Test_BakerHelpers.h"
#include "Test_DumpArtifacts.h"
#include "Test_ImGuiHarness.h"

FO_BEGIN_NAMESPACE

// Lock the current multihex-mesh coalescence behavior so a later optimization cannot change it silently,
// over a real headless MapperEngine built from self-contained synthetic resources

namespace MapperTests
{
    constexpr auto TILE_PICTURE = "MapperTile.png";
    constexpr auto CRITTER_A = "MapperCoverageCritter";
    constexpr auto SCENERY_A = "MapperCoverageScenery";
    constexpr auto WALL_A = "MapperCoverageWall";
    constexpr auto TILE_A = "MapperMergeTileA"; // SameSibling (spatial, adjacency-based merge)
    constexpr auto TILE_B = "MapperMergeTileB"; // SameSibling
    constexpr auto TILE_U = "MapperMergeTileU"; // AnyUnique (position-independent same-proto+same-data merge)

    inline auto MakeMapperTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);

        // The maps come from LoadMapFromText, so one named pack with no input dirs is all the constructor needs
        // to seed its file system
        auto pack_config = ConfigFile("[ResourcePack]\nName = MapperMergeTestPack\n");
        settings.ApplyConfigFile(pack_config, "");

        return settings;
    }

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeMapperScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerMapperEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "MapperMergeScripts",
            {
                {"Scripts/MapperMergeTest.fos",
                    R"(
namespace MapperMergeTest
{
    [[ModuleInit]]
    void ModuleInit()
    {
        Game.OnInspectorProperties.Subscribe(OnInspectorProperties);
    }

    // The inspector renders one row per index this event hands back, so with no subscriber it draws an
    // empty panel. Listing the first properties of the entity registrar is enough to exercise the rows
    [[Event]]
    void OnInspectorProperties(Entity entity, int[]& properties)
    {
        for (int i = 0; i < 12; i++) {
            properties.insertLast(i);
        }

        properties.insertLast(-1);
    }

    [[ModuleInit]]
    void InitMapperMergeTest()
    {
    }

    // Exercises the mapper's scripted editor API against the currently shown map
    int UnitTestMapperViewApi()
    {
        Game.AddMessage("mapper script coverage");

        msize hexSize = Game.GetCurMapHexSize();
        if (hexSize.width <= 0 || hexSize.height <= 0) return -1;

        isize pixelSize = Game.GetCurMapPixelSize();
        if (pixelSize.width <= 0) return -2;

        Game.SetMapperViewSize(isize(800, 600));
        Game.CenterMapperOnPlayableArea();
        Game.CenterMapperOnHex(mpos(5, 5));
        Game.CenterMapperOnRawHex(ipos(5, 5));

        Game.SetMapperOverlayVisible(true);
        if (!Game.IsMapperOverlayVisible()) return -3;
        Game.SetMapperOverlayVisible(false);

        Game.SetMapperHexOverlayVisible(true);
        if (!Game.IsMapperHexOverlayVisible()) return -4;
        Game.SetMapperHexOverlayVisible(false);

        Game.GetMapperTrackOverlayHexes();
        Game.GetMapperTrackOverlayKinds();
        Game.GetMapperScrollBorderHexes();
        Game.SetMapperHiddenSpritesVisible(true);
        Game.SetMapperHiddenSpritesVisible(false);
        Game.SetMapperScrollCheckEnabled(false);
        Game.SetMapperScrollCheckEnabled(true);

        float fitZoom = Game.CalcMapperFitZoom(isize(800, 600));
        if (fitZoom <= 0.0f) return -5;
        Game.SetMapperZoom(1.0f);

        return 0;
    }

    int UnitTestMapperEntityApi()
    {
        int currentIndex = 0;
        if (Game.GetLoadedMaps(currentIndex).isEmpty()) return -1;

        // Selection round-trip over whatever the current map holds
        Game.SelectEntities({}, true);
        if (!Game.GetSelectedEntities().isEmpty()) return -2;
        if (Game.GetSelectedEntity() !is null) return -3;

        Game.GetItemsOnHex(mpos(5, 5));
        Game.GetCrittersOnHex(mpos(5, 5), CritterFindType::Any);

        // An in-bounds hex that holds nothing proves the miss path without tripping the bounds guard
        if (Game.GetItemOnHex(mpos(2, 2)) !is null) return -4;
        if (Game.GetCritterOnHex(mpos(2, 2), CritterFindType::Any) !is null) return -5;

        Game.GetMapFileNames("", true);

        return 0;
    }

    int UnitTestMapperEditingApi()
    {
        int currentIndex = 0;
        Map[] maps = Game.GetLoadedMaps(currentIndex);
        if (maps.isEmpty()) return -1;

        Map map = maps[currentIndex];

        // Authoring: add, address by id, move, edit a property, then delete
        Item added = Game.AddItem("MapperMergeTileA".hstr(), mpos(12, 12));
        if (Game.FindEntityById(added.Id) is null) return -2;

        // Moving clears the selection, so the delete right after it operates on an unselected entity
        Game.MoveEntity(added, mpos(13, 13));
        Game.SetEntityProperty(added, "LightDistance", "3");
        Game.SelectEntity(added, true);
        if (Game.GetSelectedEntity() is null) return -3;

        Game.SelectEntity(added, false);
        Game.DeleteEntity(added);
        if (Game.FindEntityById(added.Id) !is null) return -4;

        Item tile = Game.AddTile("MapperMergeTileA".hstr(), mpos(14, 14), 0, false);

        // Placing a same-proto tile next to an existing one takes the incremental multihex-merge path
        Item neighbour = Game.AddItem("MapperMergeTileA".hstr(), mpos(15, 14));
        Item farther = Game.AddItem("MapperMergeTileA".hstr(), mpos(16, 14));
        Game.DeleteEntities({tile, neighbour, farther});

        // The well-formed saves are rejected too, because this fixture serves maps from memory: that rejection is
        // itself the proof that the maps root is resolved rather than assumed
        int saveRejections = 0;
        try { Game.SaveMapToPath(map, "Generated", ""); } catch { saveRejections++; }
        try { Game.SaveMapToPath(map, "Generated", "with/separator"); } catch { saveRejections++; }
        try { Game.SaveMapToPath(map, "..", "traversal"); } catch { saveRejections++; }
        if (saveRejections != 3) return -5;

        // Only the sandboxed writer is driven here. Game.SaveMap resolves against the maps root, which in a
        // memory-only fixture lands in the working directory and would litter the repository
        int writeRejections = 0;
        try { Game.SaveMapToPath(map, "Generated", "MapperCoverageSaved"); } catch { writeRejections++; }
        if (writeRejections == 0) return -6;

        Game.ResizeMap(24, 24);

        return 0;
    }

    int UnitTestClientMapApi()
    {
        int currentIndex = 0;
        Map[] maps = Game.GetLoadedMaps(currentIndex);
        if (maps.isEmpty()) return -1;

        Map map = maps[currentIndex];

        // Screen and scroll state round-trips without a session
        isize screen = map.GetScreenSize();
        map.SetScreenSize(screen);
        map.SetScrollCheck(map.IsScrollCheck());
        map.SetExtraScrollOffset(fpos(0.0f, 0.0f));
        map.ApplyScreenScroll(ipos(4, 4), 0, false);
        map.IsAutoScrolling();
        map.MoveScreenToHex(mpos(5, 5), ipos16(0, 0), 0, false);
        map.ChangeZoom(1.0f);
        map.RedrawMap();
        map.RebuildFog();
        map.SetDayColors(ucolor(20, 20, 30, 255), 100, ucolor(20, 20, 30, 255), 100);

        // Content queries: the map holds tiles at (5,5) and (7,7) and no critters at all
        if (map.GetItems().isEmpty()) return -2;
        if (!map.GetCritters(CritterFindType::Any).isEmpty()) return -3;
        if (!map.GetCrittersOnHex(mpos(5, 5), CritterFindType::Any).isEmpty()) return -4;
        if (!map.GetCrittersInRadius(mpos(5, 5), 3, CritterFindType::Any).isEmpty()) return -5;
        if (map.GetCritterInRadius(mpos(5, 5), 3, CritterFindType::Any) !is null) return -6;
        if (map.GetCritterOnHex(mpos(5, 5), CritterFindType::Any) !is null) return -7;

        map.GetItemsOnHex(mpos(5, 5));
        map.GetItemOnHex(mpos(5, 5));
        if (map.GetItemOnHex(mpos(2, 2)) !is null) return -8;

        // Hex geometry and coordinate translation
        if (!map.IsHexValid(mpos(5, 5))) return -9;
        map.GetHexScreenPos(mpos(5, 5));
        map.GetHexMapPos(mpos(5, 5));
        map.GetHexScreenPosF(mpos(5, 5));
        map.IsHexVisible(mpos(5, 5));
        map.GetVisibleHexes();

        mpos hitHex;
        map.GetHexAtScreenPos(ipos(0, 0), hitHex);

        ipos hitOffset;
        map.GetHexAtScreenPos(ipos(0, 0), hitHex, hitOffset);

        mpos stepped = mpos(5, 5);
        map.MoveHexByDir(stepped, mdir(0));
        map.MoveHexByDir(stepped, mdir(0), 2);

        // Pathing over an empty map answers a straight run
        if (map.GetPath(mpos(1, 1), mpos(4, 4), 0).isEmpty()) return -10;
        if (map.GetPathLength(mpos(1, 1), mpos(4, 4), 0) <= 0) return -11;

        mpos pathTarget = mpos(4, 4);
        map.GetHexInPath(mpos(1, 1), pathTarget, 0.0f, 4);
        map.GetCrittersInPath(mpos(1, 1), mpos(4, 4), 0.0f, 4, CritterFindType::Any);

        mpos preBlock;
        mpos block;
        map.GetCrittersWithBlockInPath(mpos(1, 1), mpos(4, 4), 0.0f, 4, CritterFindType::Any, preBlock, block);

        // Screen picking against an empty spot
        map.GetItemAtScreenPos(ipos(0, 0));
        map.GetCritterAtScreenPos(ipos(0, 0), 0);
        map.GetEntityAtScreenPos(ipos(0, 0));

        map.SetTransparentEgg(TransparentEggSlot::Primary, mpos(5, 5), ipos(0, 0), isize(10, 10), TransparentEggTarget::AnyOccluder);
        map.ClearTransparentEgg(TransparentEggSlot::Primary);

        // Hex classification and fog
        map.IsHexMovable(mpos(5, 5));
        map.IsHexShootable(mpos(5, 5));
        map.IsOutsideArea(mpos(5, 5));
        map.AddFog(mpos(5, 5), DrawOrderType::Light, -1);
        map.SetHiddenRoof(mpos(5, 5));
        map.GetHexContentSize(mpos(5, 5));

        // Lookups by identity: the map's own tiles resolve, and a zero id is rejected as an authoring mistake
        Item[] mapItems = map.GetItems();
        if (map.GetItem(mapItems[0].Id) is null) return -12;

        // The zero-id contract differs between the two lookups: the item one throws, the critter one answers null
        int idRejections = 0;
        try { map.GetItem(ident()); } catch { idRejections++; }
        if (idRejections != 1) return -13;
        if (map.GetCritter(ident()) !is null) return -14;

        // A locally created item lives only in this view and never reaches the server
        Item local = map.CreateLocalItem("MapperMergeTileA".hstr(), mpos(3, 3));
        local.Finish();

        return 0;
    }

    [[TimeEvent]]
    void EntityTimeEventPlain(Item self) {}

    [[TimeEvent]]
    void EntityTimeEventWithData(Item self, any data) {}

    [[TimeEvent]]
    void EntityTimeEventWithArray(Item self, any[] data) {}

    [[TimeEvent]]
    void EntityTimeEventWithContext(Item self, TimeEventContext context) {}

    int UnitTestClientEntityTimeEvents()
    {
        int currentIndex = 0;
        Map[] maps = Game.GetLoadedMaps(currentIndex);
        if (maps.isEmpty()) return -1;

        Item[] items = maps[currentIndex].GetItems();
        if (items.isEmpty()) return -2;

        Item item = items[0];

        any payload = 1;
        any[] payloads = {1, 2};

        // Every registration shape of the entity time-event family, in both the one-shot and repeating form
        uint plainId = item.StartTimeEvent(timespan(1, 3), EntityTimeEventPlain);
        item.StartTimeEvent(timespan(1, 3), EntityTimeEventWithData, payload);
        item.StartTimeEvent(timespan(1, 3), EntityTimeEventWithArray, payloads);
        item.StartTimeEvent(timespan(1, 3), EntityTimeEventWithContext);
        item.StartTimeEvent(timespan(1, 3), EntityTimeEventWithContext, payload);
        item.StartTimeEvent(timespan(1, 3), EntityTimeEventWithContext, payloads);
        item.StartTimeEvent(timespan(1, 3), timespan(2, 3), EntityTimeEventPlain);
        item.StartTimeEvent(timespan(1, 3), timespan(2, 3), EntityTimeEventWithData, payload);
        item.StartTimeEvent(timespan(1, 3), timespan(2, 3), EntityTimeEventWithArray, payloads);
        item.StartTimeEvent(timespan(1, 3), timespan(2, 3), EntityTimeEventWithContext);
        item.StartTimeEvent(timespan(1, 3), timespan(2, 3), EntityTimeEventWithContext, payload);
        item.StartTimeEvent(timespan(1, 3), timespan(2, 3), EntityTimeEventWithContext, payloads);

        if (plainId == 0) return -3;
        if (item.CountTimeEvent(EntityTimeEventPlain) <= 0) return -4;
        if (item.CountTimeEvent(plainId) <= 0) return -5;

        item.CountTimeEvent(EntityTimeEventWithData);
        item.CountTimeEvent(EntityTimeEventWithArray);
        item.CountTimeEvent(EntityTimeEventWithContext);

        item.SetTimeEventData(EntityTimeEventWithData, payload);
        item.SetTimeEventData(EntityTimeEventWithArray, payloads);
        item.SetTimeEventData(EntityTimeEventWithContext, payload);
        item.SetTimeEventData(EntityTimeEventWithContext, payloads);
        item.SetTimeEventData(plainId, payload);
        item.SetTimeEventData(plainId, payloads);

        item.RepeatTimeEvent(EntityTimeEventPlain, timespan(3, 3));
        item.RepeatTimeEvent(EntityTimeEventWithData, timespan(3, 3));
        item.RepeatTimeEvent(EntityTimeEventWithArray, timespan(3, 3));
        item.RepeatTimeEvent(EntityTimeEventWithContext, timespan(3, 3));
        item.RepeatTimeEvent(plainId, timespan(3, 3));

        item.StopTimeEvent(EntityTimeEventWithData);
        item.StopTimeEvent(EntityTimeEventWithArray);
        item.StopTimeEvent(EntityTimeEventWithContext);
        item.StopTimeEvent(plainId);
        item.StopTimeEvent(EntityTimeEventPlain);

        if (item.CountTimeEvent(EntityTimeEventPlain) != 0) return -6;

        return 0;
    }

    int UnitTestClientItemApi()
    {
        int currentIndex = 0;
        Map[] maps = Game.GetLoadedMaps(currentIndex);
        if (maps.isEmpty()) return -1;

        Item[] items = maps[currentIndex].GetItems();
        if (items.isEmpty()) return -2;

        Item item = items[0];

        item.IsVisible();
        item.GetSpriteOffset();
        item.IsAnimPlaying();
        item.PlayAnim("".hstr(), true, false);
        item.SetAnimTime(0.5f);
        item.SetAnimDir(mdir(0));
        item.StopAnim();
        item.IsMoving();
        item.GetInnerItems();
        item.SetAlpha(item.GetAlpha());

        mpos itemHex;
        item.GetMapPos(itemHex);

        // A clone is a detached copy, so finishing it must not disturb the map content
        int before = maps[currentIndex].GetItems().length();
        Item clone = item.Clone();
        clone.Finish();
        if (maps[currentIndex].GetItems().length() != before) return -3;

        return 0;
    }

    int UnitTestClientCritterApi()
    {
        Critter cr = Game.AddCritter("MapperCoverageCritter".hstr(), mpos(9, 9));

        cr.SetName("coverage critter");
        cr.IsOffline();
        cr.IsAlive();
        cr.IsKnockout();
        cr.IsDead();
        cr.IsOnMap();
        cr.IsMoving();
        cr.GetMovingContext();
        cr.IsModel();
        cr.IsVisible();
        cr.GetSpriteOffset();
        cr.IsAnimPlaying();
        cr.StopAnim();
        cr.RefreshView();
        cr.SetAlpha(cr.GetAlpha());
)"
                    R"(        cr.GetBodyAngle();
        cr.ChangeDir(mdir(1));
        cr.StopMove();

        // The critter carries nothing, so every inventory query must answer empty rather than fail
        if (!cr.GetItems().isEmpty()) return -2;
        if (cr.GetItem("MapperMergeTileA".hstr()) !is null) return -3;

        ipos textPos;
        cr.GetTextPos(textPos);

        // Animation and bone queries answer for a 2D critter without a model
        cr.IsAnimAvailable(CritterStateAnim(1), CritterActionAnim(1));
        cr.Animate(CritterStateAnim(1), CritterActionAnim(1), null, false);
        cr.StopAnim();

        if (cr.GetModelAnimDuration(CritterStateAnim(1), CritterActionAnim(1)).milliseconds != 0) return -20;

        ipos boneOffset;
        if (cr.GetBonePos("Head".hstr(), boneOffset)) return -21;

        ProtoItem tileProto = Game.GetProtoItem("MapperMergeTileA".hstr());
        if (cr.GetItem(tileProto) !is null) return -12;
        if (cr.GetItem(ItemProperty::LightDistance, 1) !is null) return -13;
        if (!cr.GetItems(ItemProperty::LightDistance, 1).isEmpty()) return -14;

        cr.MoveToHex(mpos(8, 8), ipos(0, 0), 10);
        cr.MoveToHex(mpos(8, 8), 0, ipos(0, 0), 10);
        cr.MoveToDir(mdir(2), 10);
        cr.StopMove();

        // A newly added critter must be discoverable through the map view it was placed on
        int currentIndex = 0;
        Map[] maps = Game.GetLoadedMaps(currentIndex);
        if (maps.isEmpty()) return -4;
        if (maps[currentIndex].GetCritters(CritterFindType::Any).isEmpty()) return -5;
        if (maps[currentIndex].GetCritterOnHex(mpos(9, 9), CritterFindType::Any) is null) return -6;
        if (maps[currentIndex].GetCritterInRadius(mpos(9, 9), 2, CritterFindType::Any) is null) return -7;
        if (maps[currentIndex].GetCritters("MapperCoverageCritter".hstr(), CritterFindType::Any).isEmpty()) return -8;
        if (maps[currentIndex].GetPath(cr, mpos(4, 4), 0).isEmpty()) return -9;
        if (maps[currentIndex].GetPathLength(cr, mpos(4, 4), 0) <= 0) return -10;

        // The client global lookups resolve against the current map, which the mapper provides without a session
        if (Game.GetCritter(cr.Id) is null) return -15;
        if (Game.GetCritters("MapperCoverageCritter".hstr(), CritterFindType::Any).isEmpty()) return -16;
        if (Game.GetCritters(Game.GetProtoCritter("MapperCoverageCritter".hstr()), CritterFindType::Any).isEmpty()) return -17;

        Item[] mapItems = maps[currentIndex].GetItems();
        if (Game.GetItem(mapItems[0].Id) is null) return -18;

        // Distance answers for every entity pairing
        Game.GetDistance(cr, cr);
        Game.GetDistance(mapItems[0], mapItems[0]);
        Game.GetDistance(cr, mapItems[0]);
        Game.GetDistance(mapItems[0], cr);
        Game.GetDistance(cr, mpos(1, 1));
        Game.GetDistance(mpos(1, 1), cr);
        Game.GetDistance(mapItems[0], mpos(1, 1));
        Game.GetDistance(mpos(1, 1), mapItems[0]);

        Critter[] sorted = Game.SortCrittersByDeep({cr});
        if (sorted.length() != 1) return -19;

        Game.SimulateKeyboardPress(KeyCode::A, KeyCode::None, "a", "");

        Game.DeleteEntity(cr);

        return 0;
    }

    int UnitTestParticleSpriteApi()
    {
        // The fixture serves a baked particle, so the sprite factory has to build a real particle sprite
        uint particle = Game.LoadSprite("Particles/MapperEditorTest.spk");
        if (particle == 0) return -1;

        isize particleSize = Game.GetSpriteSize(particle);
        if (particleSize.width <= 0 || particleSize.height <= 0) return -2;

        Game.SetParticleScale(particle, 2.0f);
        Game.PrewarmParticle(particle);
        Game.PlaySprite(particle, "".hstr(), true, false);
        Game.SetSpriteTime(particle, 0.25f);
        Game.StopSprite(particle);
        Game.IsSpriteHit(particle, ipos(0, 0));
        Game.FreeSprite(particle);

        return 0;
    }

    int UnitTestMapperTabApi()
    {
        // Tab state is pure editor bookkeeping and answers without a loaded map
        Game.TabSetName(0, "coverage tab");
        Game.TabSelect(0, "coverage tab", false);

        hstring[] itemPids = {"MapperMergeTileA".hstr(), "MapperMergeTileB".hstr()};
        Game.TabSetItemPids(0, "coverage tab", itemPids);
        if (Game.TabGetItemPids(0, "coverage tab").length() != 2) return -1;

        hstring[] critterPids = {"MapperCoverageCritter".hstr()};
        Game.TabSetCritterPids(0, "coverage tab", critterPids);
        if (Game.TabGetCritterPids(0, "coverage tab").length() != 1) return -2;

        // Clearing a tab with an empty list is the removal path
        hstring[] noPids;
        Game.TabSetItemPids(0, "coverage tab", noPids);
        Game.TabSetCritterPids(0, "coverage tab", noPids);

        Game.TabDelete(0);

        return 0;
    }

    int UnitTestMapperMapLifecycleApi()
    {
        // Creating, showing and unloading maps from script is a separate path from LoadMapFromText
        Map? fresh = Game.NewMap("ScriptNewMap", 24, 24);
        if (fresh is null) return -1;

        Game.ShowMap(fresh);

        Map? fromText = Game.NewMapFromText("ScriptTextMap", "[ProtoMap]\nSize = 24 24\nWorkHex = 0 0\n");
        if (fromText is null) return -2;

        Game.ShowMap(fromText);

        // Proto-object overloads of the authoring helpers
        ProtoItem itemProto = Game.GetProtoItem("MapperMergeTileA".hstr());
        Item added = Game.AddItem(itemProto, mpos(3, 3));

        ProtoCritter critterProto = Game.GetProtoCritter("MapperCoverageCritter".hstr());
        Critter cr = Game.AddCritter(critterProto, mpos(4, 4));

        Game.DeleteEntity(added);
        Game.DeleteEntity(cr);

        hstring[] ignored = {"MapperMergeTileB".hstr()};
        Game.AddMapperIgnoredItemPids(ignored);

        Game.UnloadMap(fromText);
        Game.UnloadMap(fresh);

        // Put the fixture map back in front so nothing after this sees a stale current map
        int restoreIndex = 0;
        Map[] remaining = Game.GetLoadedMaps(restoreIndex);

        if (!remaining.isEmpty()) {
            Game.ShowMap(remaining[0]);
        }

        return 0;
    }
}
)"},
            },
            [](string_view message) {
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }
#endif

    inline void AddMinimalFont(BakerTests::MemoryDataSource& source, string_view font_name)
    {
        string fofnt_path = strex("Fonts/{}.fofnt", font_name).str();
        string image_name = strex("{}.png", font_name).str();
        string image_path = strex("Fonts/{}", image_name).str();

        string fofnt_text = strex("Version 2\nImage {}\nYAdvance 1\n\nLetter ' '\n  PositionX 1\n  PositionY 1\n  XAdvance 5\n\nEnd\n", image_name).str();

        source.AddFile(fofnt_path, fofnt_text);
        source.AddFile(image_path, BakerTests::MakeMinimalBakedSprite());
    }

#if FO_SPARK_PARTICLES

    // The SPARK asset below names this effect on its renderer, and the runtime refuses to build the system
    // unless the effect resolves, so the editor fixture bakes it alongside the particle
    static constexpr string_view MAPPER_TEST_PARTICLE_EFFECT = R"EFFECT(
    [Effect]

    [VertexShader]
    layout(binding = 0, std140) uniform ProjBuf { mat4 ProjMatrix; };

    layout(location = 0) in vec3 InPosition;
    layout(location = 1) in vec4 InColor;
    layout(location = 2) in vec2 InTexCoord;

    layout(location = 0) out vec2 TexCoord;

    void main(void)
    {
        gl_Position = ProjMatrix * vec4(InPosition.xy, 0.0, 1.0);
        TexCoord = InTexCoord;
    }

    [FragmentShader]
    layout(binding = 0) uniform sampler2D MainTex;

    layout(location = 0) in vec2 TexCoord;
    layout(location = 0) out vec4 FragColor;

    void main(void)
    {
        FragColor = texture(MainTex, TexCoord);
    }
    )EFFECT";

    static constexpr string_view MAPPER_TEST_SPARK_ASSET = R"PARTICLE(
    <SPARK>
      <System name="MapperEditorParticle">
        <attrib id="groups">
          <Group name="MapperEditorGroup">
            <attrib id="capacity" value="4" />
            <attrib id="life time" value="1;1" />
            <attrib id="color interpolator">
              <ColorGraphInterpolator>
                <attrib id="graph keys" value="0;0.5;1" />
                <attrib id="graph values" value="0xFF000000;0xFFFFFF80;0xFFFFFF00" />
                <attrib id="graph values 2" value="0xFF000000;0xFFFFFFFF;0xFFFFFF00" />
                <attrib id="looping enabled" value="false" />
              </ColorGraphInterpolator>
            </attrib>
            <attrib id="scale interpolator">
              <FloatRandomInterpolator>
                <attrib id="values" value="0.3;0.4;0.5;0.7" />
              </FloatRandomInterpolator>
            </attrib>
            <attrib id="angle interpolator">
              <FloatSimpleInterpolator>
                <attrib id="values" value="0;1.5708" />
              </FloatSimpleInterpolator>
            </attrib>
            <attrib id="texture index interpolator">
              <FloatRandomInitializer>
                <attrib id="values" value="0;4" />
              </FloatRandomInitializer>
            </attrib>
            <attrib id="mass interpolator">
              <FloatGraphInterpolator>
                <attrib id="graph keys" value="0;0.5;1" />
                <attrib id="graph values" value="1;1.5;2" />
                <attrib id="graph values 2" value="1;1.8;2.5" />
                <attrib id="looping enabled" value="false" />
              </FloatGraphInterpolator>
            </attrib>
            <attrib id="modifiers">
              <Gravity>
                <attrib id="local" value="true" />
                <attrib id="value" value="(0,0.05,0)" />
              </Gravity>
              <Friction>
                <attrib id="value" value="0.5" />
              </Friction>
              <LinearForce>
                <attrib id="value" value="(0.1,0,0)" />
              </LinearForce>
              <Vortex>
                <attrib id="position" value="(0,0,0)" />
                <attrib id="direction" value="(0,1,0)" />
                <attrib id="rotation speed" value="1" />
                <attrib id="attraction speed" value="0.5" />
              </Vortex>
            </attrib>
            <attrib id="emitters">
              <RandomEmitter>
                <attrib id="tank" value="4" />
                <attrib id="flow" value="-1" />
                <attrib id="force" value="0.02;0.04" />
                <attrib id="zone">
                  <Sphere>
                    <attrib id="position" value="(0,0,0)" />
                    <attrib id="radius" value="0.5" />
                  </Sphere>
                </attrib>
                <attrib id="full" value="false" />
              </RandomEmitter>
              <StaticEmitter>
                <attrib id="tank" value="1" />
                <attrib id="flow" value="-1" />
                <attrib id="force" value="0" />
                <attrib id="zone">
                  <Point>
                    <attrib id="position" value="(0,0,0)" />
                  </Point>
                </attrib>
                <attrib id="full" value="false" />
              </StaticEmitter>
            </attrib>
            <attrib id="renderer">
              <SparkQuadRenderer>
                <attrib id="draw in scene" value="true" />
                <attrib id="active" value="true" />
                <attrib id="effect" value="Effects/Particles_ColorAdd.fofx" />
                <attrib id="texture" value="TestParticle.png" />
                <attrib id="scale" value="1.5;2" />
                <attrib id="atlas dimensions" value="2;3" />
              </SparkQuadRenderer>
            </attrib>
          </Group>
        </attrib>
      </System>
    </SPARK>
    )PARTICLE";

    // Bakes the test particle plus the effect family and texture the SPARK runtime resolves, so a fixture can
    // serve real particle resources without a build step
    inline auto MakeBakedParticleResources(string_view asset_path, string_view asset_text, string_view effect_text, string_view texture_name) -> vector<pair<string, vector<uint8_t>>>
    {
        BakerTests::TestRig particle_rig;
        particle_rig.AddSourceFile(asset_path, asset_text, 10);

        ParticleBaker particle_baker(particle_rig.MakeContext());
        particle_baker.BakeFiles(particle_rig.GetAllSourceFiles(), "");

        string baked_path = strex(asset_path).change_file_extension("spk").str();
        REQUIRE(particle_rig.Outputs.contains(baked_path));

        BakerTests::TestRig effect_rig;

        for (string_view effect_name : {"Effects/Particles_ColorAdd.fofx", "Effects/Particles_ColorAddAtlas.fofx", "Effects/Particles_ColorMulAtlas.fofx", "Effects/Particles_ColorSubAtlas.fofx", "Effects/Particles_DistortionAtlas.fofx", "Effects/Particles_DistortionAddAtlas.fofx"}) {
            effect_rig.AddSourceFile(effect_name, effect_text, 10);
        }

        EffectBaker effect_baker(effect_rig.MakeContext());
        effect_baker.BakeFiles(effect_rig.GetAllSourceFiles(), "");

        vector<pair<string, vector<uint8_t>>> resources;
        resources.emplace_back(baked_path, particle_rig.Outputs.at(baked_path));

        for (const auto& [output_path, output_data] : effect_rig.Outputs) {
            resources.emplace_back(output_path, output_data);
        }

        resources.emplace_back(string(strex(asset_path).extract_dir().combine_path(texture_name)), BakerTests::MakeMinimalBakedSprite(4, 4));
        return resources;
    }

#endif

    inline auto MakeMapperTestResources(string_view critter_model = {}) -> FileSystem
    {
        auto metadata_blob = BakerTests::MakeMetadataBlob({{"Property",
            {
                {"Item", "Common", "int32", "AuthoringNumber"},
                {"Item", "Common", "bool", "AuthoringFlag"},
                {"Item", "Common", "string", "AuthoringText"},
                {"Item", "Common", "int32[]", "AuthoringList"},
                {"Item", "Client", "int32", "AuthoringClient"},
                {"Item", "Common", "int32", "AuthoringOwner", "Mutable", "OwnerSync", "Persistent"},
                {"Item", "Server", "int32", "AuthoringServer", "Mutable", "Persistent"},
                {"Item", "Common", "int32", "AuthoringPrivate", "Mutable", "NoSync", "Persistent"},
            }}});

        auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MapperMergeCompilerResources");
        compiler_source->AddFile("Metadata.fometa-mapper", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerMapperEngine proto_engine {compiler_resources};
        hstring item_type = proto_engine.Hashes.to_hashed_string("Item");

        // A real map picture gives placed items sprite geometry, without which the editor's screen hit tests
        // can never find anything under the cursor
        hstring tile_pic = proto_engine.Hashes.to_hashed_string(TILE_PICTURE);

        auto configure_tile = [tile_pic](ProtoItem& proto) {
            proto.SetStatic(true);
            proto.SetMultihexGeneration(MultihexGenerationType::SameSibling);
            proto.SetDrawMultihexMesh(true);
            proto.SetPicMap(tile_pic);
        };
        auto configure_unique = [tile_pic](ProtoItem& proto) {
            proto.SetStatic(true);
            proto.SetMultihexGeneration(MultihexGenerationType::AnyUnique);
            proto.SetDrawMultihexMesh(true);
            proto.SetPicMap(tile_pic);
        };

        // Selection, drawing and the workspace lists all branch on what kind of item is placed, so the
        // fixture carries one proto of each kind rather than tiles alone
        auto configure_scenery = [tile_pic](ProtoItem& proto) {
            proto.SetIsScenery(true);
            proto.SetPicMap(tile_pic);
            proto.GetPropertiesForEdit()->ApplyFromText(map<string, string> {{"AuthoringNumber", "20"}, {"AuthoringFlag", "True"}, {"AuthoringText", "base"}, {"AuthoringList", "7 9"}, {"AuthoringClient", "20"}});
        };
        auto configure_wall = [tile_pic](ProtoItem& proto) {
            proto.SetIsWall(true);
            proto.SetPicMap(tile_pic);
        };

        vector<pair<string, function<void(ProtoItem&)>>> tile_protos;
        tile_protos.emplace_back(string(TILE_A), configure_tile);
        tile_protos.emplace_back(string(TILE_B), configure_tile);
        tile_protos.emplace_back(string(TILE_U), configure_unique);
        tile_protos.emplace_back(string(SCENERY_A), configure_scenery);
        tile_protos.emplace_back(string(WALL_A), configure_wall);

        auto proto_blob = BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(proto_engine, item_type, tile_protos);

        // A critter proto makes the client-side critter view surface reachable from mapper scripts
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        vector<pair<string, function<void(ProtoCritter&)>>> critter_protos;
        critter_protos.emplace_back(string(CRITTER_A), [&](ProtoCritter& proto) { proto.SetModelName(proto_engine.Hashes.to_hashed_string(critter_model)); });
        auto critter_proto_blob = BakerTests::MakeMultiProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, critter_protos);

#if FO_ANGELSCRIPT_SCRIPTING
        auto script_blob = MakeMapperScriptBinary(compiler_resources);
#endif

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MapperMergeRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-mapper", metadata_blob);
        runtime_source->AddFile("MapperMergeTiles.fopro-bin-mapper", proto_blob);
        runtime_source->AddFile("MapperMergeCritters.fopro-bin-mapper", critter_proto_blob);
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile("MapperMergeTest.fos-bin-mapper", script_blob);
#endif

        for (string_view name : {"ImGui_Default", "2D_Default", "2D_NoDepth", "Primitive_Default", "Primitive_Light", "Primitive_Fog", "Flush_RenderTarget", "Flush_Primitive", "Flush_Map", "Flush_Light", "Flush_Fog", "3D_Skinned", "Particles_ColorMulAtlas", "Particles_ColorAddAtlas", "Particles_ColorSubAtlas", "Particles_DistortionAtlas", "Particles_DistortionAddAtlas"}) {
            runtime_source->AddFile(strex("Effects/{}.fofx", name).str(), "[Effect]\nPasses = 1\n");
            runtime_source->AddFile(strex("Effects/{}.fofx-1-info", name).str(), "[EffectInfo]\n");
        }

        runtime_source->AddFile(TILE_PICTURE, BakerTests::MakeMinimalBakedSprite(32, 16));
        runtime_source->AddFile("CurDefault.png", BakerTests::MakeMinimalBakedSprite());
        runtime_source->AddFile("CurHand.png", BakerTests::MakeMinimalBakedSprite());

        for (string_view font_name : {"OldDefault", "Numbers", "BigNumbers", "SandNumbers", "Special", "Default", "Thin", "Fat", "Big"}) {
            AddMinimalFont(*runtime_source, font_name);
        }

#if FO_SPARK_PARTICLES

        // Real particle resources make the particle editor, the particle viewer and the particle sprite
        // factory reachable from the mapper instead of stopping at an empty resource list
        for (auto& [particle_path, particle_data] : MakeBakedParticleResources("Particles/MapperEditorTest.spark", MAPPER_TEST_SPARK_ASSET, MAPPER_TEST_PARTICLE_EFFECT, "TestParticle.png")) {
            runtime_source->AddFile(particle_path, particle_data);
        }

#endif

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    inline auto MakeMapText(string_view body, int32_t size = 32) -> string
    {
        return strex("[ProtoMap]\nSize = {} {}\nWorkHex = 0 0\n\n{}", size, size, body).str();
    }

    inline auto MakeItemBlock(int32_t id, string_view proto, int32_t hx, int32_t hy, string_view extra = "") -> string
    {
        return strex("[$Name/Item]\n$Id = {}\n$Proto = {}\nHex = {} {}\n{}\n", id, proto, hx, hy, extra).str();
    }

    inline auto MakeCritterBlock(int32_t id, string_view proto, int32_t hx, int32_t hy, string_view extra = "") -> string
    {
        return strex("[$Name/Critter]\n$Id = {}\n$Proto = {}\nDir = 0\nHex = {} {}\n{}\n", id, proto, hx, hy, extra).str();
    }

    inline auto HexLess(mpos hex1, mpos hex2) -> bool
    {
        return hex1.y == hex2.y ? hex1.x < hex2.x : hex1.y < hex2.y;
    }

    inline auto CollectMeshHexes(ptr<const ItemHexView> item) -> vector<mpos>
    {
        vector<mpos> hexes;
        hexes.emplace_back(item->GetHex());

        for (auto hex : item->GetMultihexMesh()) {
            hexes.emplace_back(hex);
        }

        std::ranges::sort(hexes, HexLess);
        return hexes;
    }

    inline auto CountItemsOfProto(ptr<MapView> map, hstring proto_id) -> int32_t
    {
        int32_t count = 0;

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == proto_id) {
                count++;
            }
        }

        return count;
    }

    // Describes a survivor independently of authoring order; the survivor keeps its own data, so LightDistance
    // fingerprints which item won the path-dependent race and the id pins which one survived
    struct SurvivorDesc
    {
        int64_t Id;
        mpos Origin;
        vector<mpos> Covered;
        int32_t LightDistance;

        auto operator==(const SurvivorDesc& other) const -> bool { return Id == other.Id && Origin == other.Origin && Covered == other.Covered && LightDistance == other.LightDistance; }
    };

    inline auto CollectSurvivors(ptr<MapView> map, hstring proto_id) -> vector<SurvivorDesc>
    {
        vector<SurvivorDesc> survivors;

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == proto_id) {
                survivors.emplace_back(SurvivorDesc {item->GetId().underlying_value(), item->GetHex(), CollectMeshHexes(item), item->GetLightDistance()});
            }
        }

        std::ranges::sort(survivors, [](const SurvivorDesc& a, const SurvivorDesc& b) { return HexLess(a.Origin, b.Origin); });
        return survivors;
    }
}

FO_END_NAMESPACE
