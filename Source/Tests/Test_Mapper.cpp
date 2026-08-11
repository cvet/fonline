//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include <chrono>
#include <filesystem>
#include <thread>

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
#include "Test_ImGuiHarness.h"

FO_BEGIN_NAMESPACE

// These tests lock the CURRENT behavior of MapperEngine::MergeItemsToMultihexMeshes (the multihex-mesh
// coalescence run at map load) so that a later O(N) optimization is guarded against regressions. They
// construct a real, headless MapperEngine over self-contained synthetic resources: NullRenderer stubs
// the GPU, minimal baked font sprites satisfy the mapper interface init, and two item protos carry
// MultihexGeneration = SameSibling so their clean tiles coalesce.

namespace
{
    constexpr auto TILE_PICTURE = "MapperTile.png";
    constexpr auto CRITTER_A = "MapperCoverageCritter";
    constexpr auto SCENERY_A = "MapperCoverageScenery";
    constexpr auto WALL_A = "MapperCoverageWall";
    constexpr auto TILE_A = "MapperMergeTileA"; // SameSibling (spatial, adjacency-based merge)
    constexpr auto TILE_B = "MapperMergeTileB"; // SameSibling
    constexpr auto TILE_U = "MapperMergeTileU"; // AnyUnique (position-independent same-proto+same-data merge)

    static auto MakeMapperTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);

        // The MapperEngine ctor reads GetResourcePacks() to seed the map file system. The maps in these
        // tests are supplied directly via LoadMapFromText, so a single named pack with no input dirs is
        // enough to keep construction from throwing "No information about resource packs found".
        auto pack_config = ConfigFile("[ResourcePack]\nName = MapperMergeTestPack\n");
        settings.ApplyConfigFile(pack_config, "");

        return settings;
    }

    static auto MakeMapperScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerMapperEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "MapperMergeScripts",
            {
                {"Scripts/MapperMergeTest.fos", R"(
namespace MapperMergeTest
{
    [[ModuleInit]]
    void ModuleInit()
    {
        Game.OnInspectorProperties.Subscribe(OnInspectorProperties);
    }

    // The inspector renders one row per index this event hands back, so with no subscriber it draws an
    // empty panel. Listing the first properties of the entity registrator is enough to exercise the rows.
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
        Game.SetEntityProperty(added, "Count", "3");
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

        // The sandboxed save rejects an empty name, path separators and traversal before touching anything.
        // The two well-formed saves are rejected too, because this fixture serves maps from memory and has no
        // maps root on disk to write into - which is itself the check that the root is resolved, not assumed.
        int saveRejections = 0;
        try { Game.SaveMapToPath(map, "Generated", ""); } catch { saveRejections++; }
        try { Game.SaveMapToPath(map, "Generated", "with/separator"); } catch { saveRejections++; }
        try { Game.SaveMapToPath(map, "..", "traversal"); } catch { saveRejections++; }
        if (saveRejections != 3) return -5;

        // Only the sandboxed writer is driven here. Game.SaveMap resolves against the maps root, which in a
        // memory-only fixture lands in the working directory and would litter the repository.
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

        map.SetTransparentEgg(TransparentEggSlot::Primary, mpos(5, 5), ipos(0, 0), isize(10, 10));
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
        Item countedClone = item.Clone(2);
        countedClone.Finish();
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
        if (cr.CountItem("MapperMergeTileA".hstr()) != 0) return -1;
        if (!cr.GetItems().isEmpty()) return -2;
        if (cr.GetItem("MapperMergeTileA".hstr()) !is null) return -3;

        ipos textPos;
        cr.GetTextPos(textPos);

        // Animation and bone queries answer for a 2D critter without a model
        cr.IsAnimAvailable(CritterStateAnim(1), CritterActionAnim(1));
        cr.Animate(CritterStateAnim(1), CritterActionAnim(1), null, false);
        cr.StopAnim();

        ipos boneOffset;
        cr.GetBonePos("Head".hstr(), boneOffset);

        ProtoItem tileProto = Game.GetProtoItem("MapperMergeTileA".hstr());
        if (cr.CountItem(tileProto) != 0) return -11;
        if (cr.GetItem(tileProto) !is null) return -12;
        if (cr.GetItem(ItemProperty::Count, 1) !is null) return -13;
        if (!cr.GetItems(ItemProperty::Count, 1).isEmpty()) return -14;

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

    static void AddMinimalFont(BakerTests::MemoryDataSource& source, string_view font_name)
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
    // serve real particle resources without a build step.
    static auto MakeBakedParticleResources(string_view asset_path, string_view asset_text, string_view effect_text, string_view texture_name) -> vector<pair<string, vector<uint8_t>>>
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

    static auto MakeMapperTestResources() -> FileSystem
    {
        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapperMergeCompilerResources");
        compiler_source->AddFile("Metadata.fometa-mapper", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerMapperEngine proto_engine {compiler_resources};
        hstring item_type = proto_engine.Hashes.ToHashedString("Item");

        // A real map picture gives placed items sprite geometry, without which the editor's screen hit tests
        // can never find anything under the cursor
        hstring tile_pic = proto_engine.Hashes.ToHashedString(TILE_PICTURE);

        auto configure_tile = [tile_pic](ProtoItem& proto) {
            proto.SetMultihexGeneration(MultihexGenerationType::SameSibling);
            proto.SetPicMap(tile_pic);
        };
        auto configure_unique = [tile_pic](ProtoItem& proto) {
            proto.SetMultihexGeneration(MultihexGenerationType::AnyUnique);
            proto.SetPicMap(tile_pic);
        };

        // Selection, drawing and the workspace lists all branch on what kind of item is placed, so the
        // fixture carries one proto of each kind rather than tiles alone
        auto configure_scenery = [tile_pic](ProtoItem& proto) {
            proto.SetIsScenery(true);
            proto.SetPicMap(tile_pic);
        };
        auto configure_wall = [tile_pic](ProtoItem& proto) {
            proto.SetIsWall(true);
            proto.SetPicMap(tile_pic);
        };

        vector<pair<string, function<void(ProtoItem&)>>> tile_protos {
            {string(TILE_A), configure_tile},
            {string(TILE_B), configure_tile},
            {string(TILE_U), configure_unique},
            {string(SCENERY_A), configure_scenery},
            {string(WALL_A), configure_wall},
        };

        auto proto_blob = BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(proto_engine, item_type, tile_protos);

        // A critter proto makes the client-side critter view surface reachable from mapper scripts
        hstring critter_type = proto_engine.Hashes.ToHashedString("Critter");
        vector<pair<string, function<void(ProtoCritter&)>>> critter_protos {
            {string(CRITTER_A), [](ProtoCritter&) { }},
        };
        auto critter_proto_blob = BakerTests::MakeMultiProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, critter_protos);

        auto script_blob = MakeMapperScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapperMergeRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-mapper", metadata_blob);
        runtime_source->AddFile("MapperMergeTiles.fopro-bin-mapper", proto_blob);
        runtime_source->AddFile("MapperMergeCritters.fopro-bin-mapper", critter_proto_blob);
        runtime_source->AddFile("MapperMergeTest.fos-bin-mapper", script_blob);

        runtime_source->AddFile(TILE_PICTURE, BakerTests::MakeMinimalBakedSprite(32, 16));

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

    static auto MakeMapText(string_view body, int32_t size = 32) -> string
    {
        return strex("[ProtoMap]\nSize = {} {}\nWorkHex = 0 0\n\n{}", size, size, body).str();
    }

    static auto MakeItemBlock(int32_t id, string_view proto, int32_t hx, int32_t hy, string_view extra = "") -> string
    {
        return strex("[$Name/Item]\n$Id = {}\n$Proto = {}\nHex = {} {}\n{}\n", id, proto, hx, hy, extra).str();
    }

    static auto MakeCritterBlock(int32_t id, string_view proto, int32_t hx, int32_t hy, string_view extra = "") -> string
    {
        return strex("[$Name/Critter]\n$Id = {}\n$Proto = {}\nDir = 0\nHex = {} {}\n{}\n", id, proto, hx, hy, extra).str();
    }

    static auto HexLess(mpos hex1, mpos hex2) -> bool
    {
        return hex1.y == hex2.y ? hex1.x < hex2.x : hex1.y < hex2.y;
    }

    static auto CollectMeshHexes(ptr<const ItemHexView> item) -> vector<mpos>
    {
        vector<mpos> hexes;
        hexes.emplace_back(item->GetHex());

        for (auto hex : item->GetMultihexMesh()) {
            hexes.emplace_back(hex);
        }

        std::ranges::sort(hexes, HexLess);
        return hexes;
    }

    static auto CountItemsOfProto(ptr<MapView> map, hstring proto_id) -> int32_t
    {
        int32_t count = 0;

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == proto_id) {
                count++;
            }
        }

        return count;
    }

    // A surviving item described independently of authoring/merge order: its serialized id, normalized origin,
    // the full hex_less-sorted set of hexes it covers, and the per-item Count value that survived the merge (the
    // merge survivor keeps ITS OWN data, so Count is a fingerprint of which item won a path-dependent race; the
    // id pins WHICH item became the survivor, which matters for AnyUnique where the survivor is the lowest id).
    struct SurvivorDesc
    {
        int64_t Id;
        mpos Origin;
        vector<mpos> Covered;
        int32_t Count;

        auto operator==(const SurvivorDesc& other) const -> bool { return Id == other.Id && Origin == other.Origin && Covered == other.Covered && Count == other.Count; }
    };

    static auto CollectSurvivors(ptr<MapView> map, hstring proto_id) -> vector<SurvivorDesc>
    {
        vector<SurvivorDesc> survivors;

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == proto_id) {
                survivors.emplace_back(SurvivorDesc {item->GetId().underlying_value(), item->GetHex(), CollectMeshHexes(item), item->GetCount()});
            }
        }

        std::ranges::sort(survivors, [](const SurvivorDesc& a, const SurvivorDesc& b) { return HexLess(a.Origin, b.Origin); });
        return survivors;
    }
}

TEST_CASE("MapperMultihexMeshMerge")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    hstring tile_a = mapper->Hashes.ToHashedString(TILE_A);
    hstring tile_b = mapper->Hashes.ToHashedString(TILE_B);

    REQUIRE(mapper->GetProtoItem(tile_a) != nullptr);
    REQUIRE(mapper->GetProtoItem(tile_b) != nullptr);
    REQUIRE(mapper->GetProtoItem(tile_a)->GetMultihexGeneration() == MultihexGenerationType::SameSibling);

    SECTION("Coalesces an adjacent block into one multihex-mesh item")
    {
        // A horizontal run of four clean TileA tiles.
        string body;
        for (int32_t i = 0; i < 4; i++) {
            body += MakeItemBlock(10 + i, TILE_A, 5 + i, 5);
        }

        auto map = mapper->LoadMapFromText("CoalesceMap", "CoalesceMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);
        CHECK(survivor->IsNonEmptyMultihexMesh());

        auto covered = CollectMeshHexes(survivor);
        REQUIRE(covered.size() == 4);
        CHECK(covered[0] == mpos {5, 5});
        CHECK(covered[1] == mpos {6, 5});
        CHECK(covered[2] == mpos {7, 5});
        CHECK(covered[3] == mpos {8, 5});
    }

    SECTION("Origin is normalized to the hex_less-smallest covered hex and mesh is sorted")
    {
        // Authoring order intentionally does not start at the smallest hex.
        string body;
        body += MakeItemBlock(30, TILE_A, 8, 5);
        body += MakeItemBlock(31, TILE_A, 6, 5);
        body += MakeItemBlock(32, TILE_A, 7, 5);
        body += MakeItemBlock(33, TILE_A, 5, 5);

        auto map = mapper->LoadMapFromText("NormalizeMap", "NormalizeMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        auto covered = CollectMeshHexes(survivor);
        REQUIRE(covered.size() == 4);

        // Origin is the smallest covered hex.
        CHECK(survivor->GetHex() == covered.front());
        CHECK(survivor->GetHex() == mpos {5, 5});

        // The stored mesh (origin + remaining covered hexes) is sorted by hex_less.
        const auto& mesh = survivor->GetMultihexMesh();
        REQUIRE(!mesh.empty());
        CHECK(std::ranges::is_sorted(mesh, HexLess));
        CHECK(HexLess(survivor->GetHex(), mesh.front()));
    }

    SECTION("Idempotent: re-running the merge changes nothing")
    {
        string body;
        for (int32_t i = 0; i < 4; i++) {
            body += MakeItemBlock(40 + i, TILE_A, 5 + i, 5);
        }

        auto map = mapper->LoadMapFromText("IdempotentMap", "IdempotentMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        auto origin_before = survivor->GetHex();
        auto covered_before = CollectMeshHexes(survivor);

        // LoadMapFromText already runs the merge twice (the second call asserts idempotency); run it once
        // more directly to lock that the public entry point is a fixed point on an already-merged map.
        size_t extra_merges = mapper->MergeItemsToMultihexMeshes(map);
        CHECK(extra_merges == 0);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);
        CHECK(survivor->GetHex() == origin_before);
        CHECK(CollectMeshHexes(survivor) == covered_before);
    }

    SECTION("Different protos do not merge")
    {
        string body;
        body += MakeItemBlock(50, TILE_A, 5, 5);
        body += MakeItemBlock(51, TILE_B, 6, 5);

        auto map = mapper->LoadMapFromText("CrossProtoMap", "CrossProtoMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        CHECK(CountItemsOfProto(map, tile_a) == 1);
        CHECK(CountItemsOfProto(map, tile_b) == 1);

        for (const auto& item : map->GetItems()) {
            CHECK_FALSE(item->IsNonEmptyMultihexMesh());
        }
    }

    SECTION("Tiles with differing modified data are not merged together")
    {
        // Two adjacent TileA tiles carrying DIFFERENT authored Count values. Neither is clean (equal to
        // the proto) and their per-item data differs, so CompareMultihexItemForMerge keeps them apart:
        // a merge between two non-clean items requires identical data.
        string body;
        body += MakeItemBlock(60, TILE_A, 5, 5, "Count = 7");
        body += MakeItemBlock(61, TILE_A, 6, 5, "Count = 9");

        auto map = mapper->LoadMapFromText("ModifiedPairMap", "ModifiedPairMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        REQUIRE(CountItemsOfProto(map, tile_a) == 2);

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                CHECK_FALSE(item->IsNonEmptyMultihexMesh());
            }
        }
    }

    SECTION("Current behavior: a clean tile next to a modified tile still coalesces")
    {
        // Locks the surprising current rule: the merge has a dedicated "first merge to modified items"
        // pass and CompareMultihexItemForMerge allows a clean source to merge into any same-proto target
        // (allow_clean_merge). So one modified tile adjacent to clean tiles is NOT kept separate - the
        // whole run collapses into a single multihex-mesh item and the modified per-item data is dropped.
        string body;
        body += MakeItemBlock(70, TILE_A, 5, 5);
        body += MakeItemBlock(71, TILE_A, 6, 5);
        body += MakeItemBlock(72, TILE_A, 7, 5);
        body += MakeItemBlock(73, TILE_A, 8, 5, "Count = 7");

        auto map = mapper->LoadMapFromText("CleanPlusModifiedMap", "CleanPlusModifiedMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);
        CHECK(survivor->IsNonEmptyMultihexMesh());
        CHECK(CollectMeshHexes(survivor).size() == 4);
    }

    SECTION("Two disjoint clusters stay separate")
    {
        string body;
        // Cluster 1 near the top-left.
        body += MakeItemBlock(70, TILE_A, 3, 3);
        body += MakeItemBlock(71, TILE_A, 4, 3);
        // Cluster 2 far away so the two never touch as neighbors.
        body += MakeItemBlock(72, TILE_A, 20, 20);
        body += MakeItemBlock(73, TILE_A, 21, 20);

        auto map = mapper->LoadMapFromText("DisjointMap", "DisjointMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        REQUIRE(CountItemsOfProto(map, tile_a) == 2);

        int32_t mesh_items = 0;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                CHECK(item->IsNonEmptyMultihexMesh());
                CHECK(CollectMeshHexes(item).size() == 2);
                mesh_items++;
            }
        }
        CHECK(mesh_items == 2);
    }

    SECTION("Path-dependent: a clean bridge collapses a modified..modified chain into one clean mesh")
    {
        // ADVERSARIAL guard for the O(N) optimization. A naive static connected-component flood-fill of
        // "~-connected same-proto tiles" would also merge this whole line, but it would NOT reproduce the
        // exact PATH-DEPENDENT survivor: because the merge runs a dedicated "modified items first" pass and
        // CompareMultihexItemForMerge(allow_clean_merge) lets a CLEAN source merge into ANY same-proto
        // target, the two modified end tiles (Count 3 and Count 9) are bridged by the clean middle tiles and
        // the WHOLE run collapses into a single multihex mesh whose surviving data is the CLEAN proto data
        // (Count == 0, the proto default) - both authored Count values are dropped. This pins that exact
        // result so the incremental candidate-collection optimization cannot quietly change the survivor.
        string body;
        body += MakeItemBlock(200, TILE_A, 5, 5, "Count = 3");
        body += MakeItemBlock(201, TILE_A, 6, 5);
        body += MakeItemBlock(202, TILE_A, 7, 5);
        body += MakeItemBlock(203, TILE_A, 8, 5, "Count = 9");

        auto map = mapper->LoadMapFromText("ModChainAsc", "ModChainAsc.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}, {8, 5}});
    }

    SECTION("Path-dependent: a modified tile bridged by clean neighbors is absorbed (clean - modified - clean)")
    {
        string body;
        body += MakeItemBlock(210, TILE_A, 5, 5);
        body += MakeItemBlock(211, TILE_A, 6, 5, "Count = 4");
        body += MakeItemBlock(212, TILE_A, 7, 5);

        auto map = mapper->LoadMapFromText("CleanModClean", "CleanModClean.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}});
    }

    SECTION("Path-dependent: modified..clean..modified collapse is id-order independent")
    {
        // Same chain as above but the modified end tiles own the LOWEST ids and the clean bridge owns the
        // HIGHEST ids, so the per-step best-by-id merge direction differs from the ascending-id case. The
        // collapse and the clean survivor data must match regardless: a single mesh of all four hexes with
        // Count == 0. This catches an optimization that accidentally became sensitive to id authoring order.
        string body;
        body += MakeItemBlock(220, TILE_A, 5, 5, "Count = 3");
        body += MakeItemBlock(223, TILE_A, 6, 5);
        body += MakeItemBlock(222, TILE_A, 7, 5);
        body += MakeItemBlock(221, TILE_A, 8, 5, "Count = 9");

        auto map = mapper->LoadMapFromText("ModChainModLowIds", "ModChainModLowIds.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}, {8, 5}});
    }

    SECTION("Modified data partitions a run that no clean tile bridges (modified X - X - Y)")
    {
        // modified(X) - modified(X) - modified(Y): the first two share authored data and merge; the third
        // carries different data with no clean tile to bridge it, so it stays separate. A pure
        // proto-adjacency flood-fill would over-merge all three into one mesh - this pins the data-aware
        // partition: two survivors, a mesh of {(5,5),(6,5)} keeping Count 5 and a single {(7,5)} keeping
        // Count 8.
        string body;
        body += MakeItemBlock(230, TILE_A, 5, 5, "Count = 5");
        body += MakeItemBlock(231, TILE_A, 6, 5, "Count = 5");
        body += MakeItemBlock(232, TILE_A, 7, 5, "Count = 8");

        auto map = mapper->LoadMapFromText("ModXModXModY", "ModXModXModY.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 2);

        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].Count == 5);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}});

        CHECK(survivors[1].Origin == mpos {7, 5});
        CHECK(survivors[1].Count == 8);
        CHECK(survivors[1].Covered == vector<mpos> {{7, 5}});
    }

    SECTION("Large solid block coalesces and loads")
    {
        // Solid 24x24 block of clean TileA tiles; pins the result shape for the perf optimization.
        constexpr int32_t block = 24;
        constexpr int32_t origin = 3;

        string body;
        int32_t id = 100;
        for (int32_t y = 0; y < block; y++) {
            for (int32_t x = 0; x < block; x++) {
                body += MakeItemBlock(id++, TILE_A, origin + x, origin + y);
            }
        }

        auto map = mapper->LoadMapFromText("LargeBlockMap", "LargeBlockMap.fomap", MakeMapText(body, 64));
        REQUIRE(map != nullptr);

        // Current behavior: an adjacency-connected block collapses into a single multihex-mesh item.
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        auto covered = CollectMeshHexes(survivor);
        CHECK(covered.size() == numeric_cast<size_t>(block) * block);
        CHECK(survivor->GetHex() == mpos {origin, origin});
    }
}

// AnyUnique is the second multihex-mesh strategy (used by floor tiles/walls in real maps, and the dominant cost
// of the 1000x1000 map load). Unlike SameSibling it is NOT spatial: it merges EVERY same-proto item that has the
// same per-item data (ignoring Hex and MultihexMesh) into one mesh regardless of position, collapsing each
// (proto, data) group into its LOWEST-id member. These sections lock that behavior so the O(N) optimization of
// the AnyUnique coalescence stays behavior-identical.
TEST_CASE("MapperAnyUniqueMeshMerge")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    hstring tile_u = mapper->Hashes.ToHashedString(TILE_U);

    REQUIRE(mapper->GetProtoItem(tile_u) != nullptr);
    REQUIRE(mapper->GetProtoItem(tile_u)->GetMultihexGeneration() == MultihexGenerationType::AnyUnique);

    SECTION("Non-adjacent clean tiles merge into one mesh despite no adjacency")
    {
        string body;
        body += MakeItemBlock(300, TILE_U, 3, 3);
        body += MakeItemBlock(301, TILE_U, 20, 20);
        body += MakeItemBlock(302, TILE_U, 5, 25);

        auto map = mapper->LoadMapFromText("U_NonAdjacentClean", "U_NonAdjacentClean.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Id == 300); // lowest id wins
        CHECK(survivors[0].Origin == mpos {3, 3});
        CHECK(survivors[0].Count == 0);
        // hex_less is y-major: (3,3) then (20,20) then (5,25).
        CHECK(survivors[0].Covered == vector<mpos> {{3, 3}, {20, 20}, {5, 25}});
    }

    SECTION("Scattered tiles partition by per-item data, each group collapsing into its lowest id")
    {
        string body;
        body += MakeItemBlock(310, TILE_U, 2, 2);
        body += MakeItemBlock(311, TILE_U, 10, 2, "Count = 7");
        body += MakeItemBlock(312, TILE_U, 2, 10);
        body += MakeItemBlock(313, TILE_U, 25, 25, "Count = 7");
        body += MakeItemBlock(314, TILE_U, 18, 4, "Count = 9");

        auto map = mapper->LoadMapFromText("U_MixedScatter", "U_MixedScatter.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 3);

        // Clean group (310 + 312).
        CHECK(survivors[0].Id == 310);
        CHECK(survivors[0].Origin == mpos {2, 2});
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{2, 2}, {2, 10}});

        // Count == 7 group (311 + 313).
        CHECK(survivors[1].Id == 311);
        CHECK(survivors[1].Origin == mpos {10, 2});
        CHECK(survivors[1].Count == 7);
        CHECK(survivors[1].Covered == vector<mpos> {{10, 2}, {25, 25}});

        // Lone Count == 9 tile (314).
        CHECK(survivors[2].Id == 314);
        CHECK(survivors[2].Origin == mpos {18, 4});
        CHECK(survivors[2].Count == 9);
        CHECK(survivors[2].Covered == vector<mpos> {{18, 4}});
    }

    SECTION("Survivor is the lowest-id group member regardless of authoring/position order")
    {
        string body;
        body += MakeItemBlock(330, TILE_U, 5, 5);
        body += MakeItemBlock(320, TILE_U, 6, 5);
        body += MakeItemBlock(325, TILE_U, 7, 5);

        auto map = mapper->LoadMapFromText("U_ShuffledIds", "U_ShuffledIds.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Id == 320); // lowest id, even though it was authored second and sits at (6,5)
        CHECK(survivors[0].Origin == mpos {5, 5}); // origin normalized to the hex_less-smallest covered hex
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}});
    }

    SECTION("Different protos and a SameSibling tile never merge into an AnyUnique mesh")
    {
        hstring tile_a = mapper->Hashes.ToHashedString(TILE_A);

        string body;
        body += MakeItemBlock(340, TILE_U, 3, 3);
        body += MakeItemBlock(341, TILE_A, 4, 3); // SameSibling, adjacent - must not join the AnyUnique mesh
        body += MakeItemBlock(342, TILE_U, 6, 3);

        auto map = mapper->LoadMapFromText("U_CrossStrategy", "U_CrossStrategy.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto u_survivors = CollectSurvivors(map, tile_u);
        REQUIRE(u_survivors.size() == 1);
        CHECK(u_survivors[0].Id == 340);
        CHECK(u_survivors[0].Covered == vector<mpos> {{3, 3}, {6, 3}});

        CHECK(CountItemsOfProto(map, tile_a) == 1);
    }

    SECTION("Idempotent: re-running the merge changes nothing")
    {
        string body;
        body += MakeItemBlock(350, TILE_U, 2, 2);
        body += MakeItemBlock(351, TILE_U, 9, 9);
        body += MakeItemBlock(352, TILE_U, 4, 12, "Count = 4");
        body += MakeItemBlock(353, TILE_U, 20, 1, "Count = 4");

        auto map = mapper->LoadMapFromText("U_Idempotent", "U_Idempotent.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto before = CollectSurvivors(map, tile_u);
        size_t extra_merges = mapper->MergeItemsToMultihexMeshes(map);
        CHECK(extra_merges == 0);
        CHECK(CollectSurvivors(map, tile_u) == before);
    }
}

// Regression: MapperEngine::LoadMap resolves a map file located by a directory-qualified path (the
// form the render/preview tooling passes, e.g. "Gambell/NewGambell_Center") and never lets a
// same-stem sibling of another type (the map's NewGambell_Center.foloc location file) shadow the
// .fomap during file discovery. Nearly every location map in the project ships such a .foloc sibling,
// so the shadowing broke headless map loading for most maps.
TEST_CASE("MapperLoadMapResolvesNameAndPath")
{
    auto settings = MakeMapperTestSettings();

    // Mirror the project's proto-extension order (LastFrontier.fomain), where .foloc precedes .fomap.
    // That ordering is what let a same-stem location file shadow the map file during discovery.
    BakerTests::OverrideSetting(settings.ProtoFileExtensions, vector<string> {"foinfo", "fopro", "foloc", "fomap", "focr", "foitem"});

    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    // A single-map .fomap and a same-stem .foloc live side by side under a subdirectory, mirroring the
    // real content layout. The .foloc must not shadow the .fomap for either lookup form.
    auto maps_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapperLoadMapTestMaps");
    maps_source->AddFile("Gambell/ShadowedMap.foloc", "[ProtoLocation]\n$Name = ShadowedMap\nMapProtos = ShadowedMap\n");
    maps_source->AddFile("Gambell/ShadowedMap.fomap", MakeMapText(MakeItemBlock(10, TILE_A, 5, 5)));
    mapper->MapsFileSys.AddCustomSource(std::move(maps_source));

    hstring expected_proto = mapper->Hashes.ToHashedString("ShadowedMap");

    SECTION("Loads by directory-qualified path despite a same-stem location sibling")
    {
        auto map = mapper->LoadMap("Gambell/ShadowedMap");
        REQUIRE(map != nullptr);
        CHECK(map->GetProtoId() == expected_proto);
    }

    SECTION("Loads by bare declared map name")
    {
        auto map = mapper->LoadMap("ShadowedMap");
        REQUIRE(map != nullptr);
        CHECK(map->GetProtoId() == expected_proto);
    }
}

TEST_CASE("MapperDrawsEditorPanelsHeadlessly")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // Every editor panel is hidden by default, so opt them all in before drawing
    mapper->WorkspaceWindowVisible = true;
    mapper->ContentWindowVisible = true;
    mapper->CritterAnimationsWindowVisible = true;
    mapper->ScriptCallWindowVisible = true;
    mapper->MapListWindowVisible = true;
    mapper->MapWindowVisible = true;
    mapper->HistoryWindowVisible = true;
    mapper->SettingsWindowVisible = true;
    mapper->InspectorVisible = true;

    // A shown map with everything selected gives the map, content and inspector panels real rows to render:
    // the inspector only lists property lines for a selected entity, so without this it draws an empty frame
    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7);
    auto map = mapper->LoadMapFromText("PanelMap", "PanelMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);

    mapper->ShowMap(map.as_ptr());
    mapper->SelectAll();
    REQUIRE_FALSE(mapper->SelectedEntities.empty());
    REQUIRE(mapper->GetInspectorEntity() != nullptr);

    // Walk the inspector's property lines and commit an edit so the parse/apply path runs too
    for (int32_t line = 0; line < 4; line++) {
        REQUIRE_NOTHROW(mapper->SelectInspectorPropertyLine(line));
    }

    // Committing an edit runs the text parser and the undo op behind it; the first property rows start at
    // line 3, and a malformed value has to be rejected and rolled back to the initial one
    for (int32_t line = 3; line < 8; line++) {
        mapper->SelectInspectorPropertyLine(line);
        mapper->InspectorSelectedLineValue = "1";
        REQUIRE_NOTHROW(mapper->ApplyInspectorPropertyEdit(mapper->GetInspectorEntity().as_ptr()));
    }

    mapper->SelectInspectorPropertyLine(3);
    mapper->InspectorSelectedLineInitialValue = "0";
    mapper->InspectorSelectedLineValue = "not a value";
    REQUIRE_NOTHROW(mapper->ApplyInspectorPropertyEdit(mapper->GetInspectorEntity().as_ptr()));

    // Structured values go through the token reader, which handles quoting, escapes and nesting
    for (string_view structured_value : {"{ 1 2 3 }", R"("quoted value")", R"({ "a" 1 "b" 2 })", R"("with \"escape\"")", "{ unterminated"}) {
        mapper->SelectInspectorPropertyLine(4);
        mapper->InspectorSelectedLineValue = string {structured_value};
        REQUIRE_NOTHROW(mapper->ApplyInspectorPropertyEdit(mapper->GetInspectorEntity().as_ptr()));
    }

    mapper->SelectInspectorPropertyLine(0);

    // Logging auto-expands tree nodes, so the panel bodies run instead of collapsing to headers
    constexpr int32_t PANEL_FRAMES = 8;
    string drawn_text;

    // Each panel mode shows a different content-window body, so the frames walk through them
    vector<int32_t> panel_modes {MapperEngine::INT_MODE_ITEM, MapperEngine::INT_MODE_TILE, MapperEngine::INT_MODE_CRIT, MapperEngine::INT_MODE_INCONT, MapperEngine::INT_MODE_FAST, MapperEngine::INT_MODE_IGNORE, MapperEngine::INT_MODE_MESS, MapperEngine::INT_MODE_LIST};

    // Collapsing headers opt out of the log auto-expansion, so their bodies only render once their stored
    // state is seeded open; these are the ones the map window carries
    constexpr std::array MAP_WINDOW_HEADER_IDS = {"Visibility", "Selection"};

    for (int32_t frame = 0; frame < PANEL_FRAMES; frame++) {
        mapper->SetActivePanelMode(panel_modes[numeric_cast<size_t>(frame) % panel_modes.size()]);

        ImGui::NewFrame();

        // State storage is per window, so the Controls window has to be found by name once it exists
        if (auto controls_window = make_nptr(ImGui::FindWindowByName("Controls"))) {
            for (string_view header_id : MAP_WINDOW_HEADER_IDS) {
                controls_window->StateStorage.SetInt(controls_window->GetID(header_id.data(), header_id.data() + header_id.size()), 1);
            }
        }

        ImGui::LogToBuffer(12);

        REQUIRE_NOTHROW(mapper->DrawMainPanelImGui());
        REQUIRE_NOTHROW(mapper->DrawWorkspaceWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawContentWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawCritterAnimationsWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawScriptCallWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawMapListWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawMapWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawHistoryWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawSettingsWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawInspectorImGui());
        REQUIRE_NOTHROW(mapper->DrawConsoleImGui());

        drawn_text.assign(ImGui::GetCurrentContext()->LogBuffer.c_str());
        ImGui::LogFinish();
        ImGui::Render();
    }

    CHECK(ImGui::GetFrameCount() == PANEL_FRAMES);
    INFO(drawn_text);
    CHECK_FALSE(drawn_text.empty());

    // The inspector draws its editor widgets only for the line that is currently being edited, so every
    // property type's editor - text, bool, array, struct fields - is unreachable until each line in turn
    // becomes the edit target. Walking the lines is what covers that tree.
    REQUIRE_FALSE(mapper->ShowProps.empty());

    for (size_t prop_index = 0; prop_index < mapper->ShowProps.size(); prop_index++) {
        int32_t line = 3 + numeric_cast<int32_t>(prop_index);
        INFO(line);

        mapper->SelectInspectorPropertyLine(line);
        mapper->InspectorEditLine = line;
        mapper->InspectorEditBuf = mapper->InspectorSelectedLineValue;

        // Two frames per line: the second one meets the pending focus/caret state the first one queued
        for (int32_t frame = 0; frame < 2; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(mapper->DrawInspectorImGui());
            ImGui::LogFinish();
            ImGui::Render();
        }
    }

    // Applying an edit to every compatible selected entity is a separate arm of the same walk
    mapper->InspectorApplyToAll = true;
    mapper->SelectInspectorPropertyLine(3);
    mapper->InspectorEditLine = 3;
    mapper->InspectorEditBuf = mapper->InspectorSelectedLineValue;

    ImGui::NewFrame();
    REQUIRE_NOTHROW(mapper->DrawInspectorImGui());
    ImGui::Render();

    mapper->InspectorEditLine = -1;
    mapper->InspectorApplyToAll = false;
}

TEST_CASE("MapperSelectionFollowsLayerVisibility")
{
    // Select-all walks the placed items once and admits each by its own kind, so a map of plain items only
    // ever reaches one arm of that test. This map carries an item, a scenery piece, a wall, a floor tile,
    // a roof tile and a critter, and the layers are switched off one at a time.
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5);
    body += MakeItemBlock(11, SCENERY_A, 7, 5);
    body += MakeItemBlock(12, WALL_A, 9, 5);
    body += MakeCritterBlock(13, CRITTER_A, 11, 5);

    auto map = mapper->LoadMapFromText("SelectionMap", "SelectionMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);
    mapper->ShowMap(map.as_ptr());

    // Tiles are not authored in map text - the mapper places them, which is also what marks them as roof
    ptr<MapView> map_ptr = map.as_ptr();
    REQUIRE_NOTHROW(ignore_unused(map_ptr->AddMapperTile(mapper->Hashes.ToHashedString(TILE_A), mpos {6, 8}, 0, false).get()));
    REQUIRE_NOTHROW(ignore_unused(map_ptr->AddMapperTile(mapper->Hashes.ToHashedString(TILE_B), mpos {8, 8}, 0, true).get()));

    SECTION("EveryLayerContributesItsOwnEntities")
    {
        mapper->SelectAll();
        size_t all_layers = mapper->SelectedEntities.size();
        CHECK(all_layers >= 6);

        // Each layer switched off must cost exactly the entities that belong to it
        settings.ShowCrit = false;
        mapper->SelectAll();
        CHECK(mapper->SelectedEntities.size() < all_layers);

        settings.ShowScen = false;
        settings.ShowWall = false;
        settings.ShowTile = false;
        settings.ShowRoof = false;
        mapper->SelectAll();
        size_t items_only = mapper->SelectedEntities.size();
        CHECK(items_only < all_layers);

        settings.ShowItem = false;
        mapper->SelectAll();
        CHECK(mapper->SelectedEntities.empty());

        settings.ShowItem = true;
        settings.ShowScen = true;
        settings.ShowWall = true;
        settings.ShowTile = true;
        settings.ShowRoof = true;
        settings.ShowCrit = true;
    }

    SECTION("PerKindSelectionSwitchesGateTheSameWalk")
    {
        mapper->SelectCrittersEnabled = false;
        mapper->SelectSceneryEnabled = false;
        mapper->SelectWallsEnabled = false;
        mapper->SelectTilesEnabled = false;
        mapper->SelectRoofTilesEnabled = false;
        mapper->SelectAll();
        size_t items_only = mapper->SelectedEntities.size();

        mapper->SelectItemsEnabled = false;
        mapper->SelectAll();
        CHECK(mapper->SelectedEntities.empty());

        mapper->SelectItemsEnabled = true;
        mapper->SelectCrittersEnabled = true;
        mapper->SelectSceneryEnabled = true;
        mapper->SelectWallsEnabled = true;
        mapper->SelectTilesEnabled = true;
        mapper->SelectRoofTilesEnabled = true;
        mapper->SelectAll();
        CHECK(mapper->SelectedEntities.size() > items_only);
    }

    SECTION("DeselectingAnAnyUniqueItemRunsTheIncrementalMerge")
    {
        // Dropping an item out of the selection re-merges it into its multihex mesh, and for the AnyUnique
        // strategy that goes through the per-step incremental driver rather than the whole-map coalescer -
        // a path no other test reaches, because every other fixture item is SameSibling or plain.
        ptr<MapView> selection_map = mapper->GetCurMap().as_ptr();
        hstring unique_pid = mapper->Hashes.ToHashedString(TILE_U);

        auto first_unique = selection_map->AddMapperItem(unique_pid, mpos {4, 12}, nullptr);
        auto second_unique = selection_map->AddMapperItem(unique_pid, mpos {16, 18}, nullptr);
        REQUIRE_NOTHROW(ignore_unused(first_unique.get()));
        REQUIRE_NOTHROW(ignore_unused(second_unique.get()));

        mapper->SelectClear();
        mapper->SelectAdd(first_unique);
        mapper->SelectAdd(second_unique);
        REQUIRE(mapper->SelectedEntities.size() == 2);

        // The second one leaving the selection is what offers it to the first one's mesh
        REQUIRE_NOTHROW(mapper->SelectRemove(second_unique, false));
        REQUIRE_NOTHROW(mapper->SelectRemove(first_unique, false));
        CHECK(mapper->SelectedEntities.empty());

        // Same-proto AnyUnique items with identical data collapse into one mesh regardless of distance
        size_t unique_left = 0;

        for (ptr<const ItemHexView> item : selection_map->GetItems()) {
            if (!item->IsDestroyed() && item->GetProtoId() == unique_pid) {
                unique_left++;
            }
        }

        CHECK(unique_left == 1);
    }

    SECTION("RemovingAndReAddingASelectionKeepsBothStoresInStep")
    {
        mapper->SelectAll();
        REQUIRE_FALSE(mapper->SelectedEntities.empty());

        size_t before = mapper->SelectedEntities.size();
        ptr<ClientEntity> first = mapper->SelectedEntities.front().as_ptr();

        mapper->SelectRemove(first, false);
        CHECK(mapper->SelectedEntities.size() == before - 1);

        // Removing an entity that is not selected is ordinary editing, not an error
        REQUIRE_NOTHROW(mapper->SelectRemove(first, false));
        CHECK(mapper->SelectedEntities.size() == before - 1);

        mapper->SelectAdd(first);
        CHECK(mapper->SelectedEntities.size() == before);

        mapper->SelectClear();
        CHECK(mapper->SelectedEntities.empty());
    }
}

TEST_CASE("MapperPanelControlsRunTheirActions")
{
    // The panels draw their controls in every headless frame but nothing is ever pressed, so the code
    // behind each button stays unreachable. Saving writes real files, so the fixture keeps a private
    // Maps root the same way the save test does.
    auto maps_dir = std::filesystem::temp_directory_path() / std::format("fo_engine_mapper_controls_test_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code remove_error;
    std::filesystem::remove_all(maps_dir, remove_error);
    std::filesystem::create_directories(maps_dir);

    auto cleanup_maps_dir = scope_exit([maps_dir]() noexcept {
        safe_call([maps_dir] {
            std::error_code ignored;
            std::filesystem::remove_all(maps_dir, ignored);
        });
    });

    REQUIRE(fs_write_file((maps_dir / "ReferenceMap.fomap").generic_string(), MakeMapText(MakeItemBlock(10, TILE_A, 5, 5))));

    auto settings = MakeMapperTestSettings();
    BakerTests::OverrideSetting(settings.ProtoFileExtensions, vector<string> {"fopro", "fomap"});

    // The direct-draw render path the headless fixture uses turns map zoom off, and with it the zoom
    // buttons become no-ops that cannot be told apart from a press that never landed
    BakerTests::OverrideSetting(settings.MapZoomEnabled, true);
    auto pack_config = ConfigFile(strex("[ResourcePack]\nName = MapperControlsTestPack\nInputDirs = {}\n", maps_dir.generic_string()).str());
    settings.ApplyConfigFile(pack_config, "");

    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1920.0f, 1080.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // Pressing the fullscreen control writes through to the process-global app window
    bool saved_fullscreen = GetApp()->MainWindow.IsFullscreen();
    auto restore_fullscreen = scope_exit([saved_fullscreen]() noexcept { safe_call([saved_fullscreen] { (void)GetApp()->MainWindow.ToggleFullscreen(saved_fullscreen); }); });

    mapper->WorkspaceWindowVisible = true;
    mapper->ContentWindowVisible = true;
    mapper->CritterAnimationsWindowVisible = true;
    mapper->ScriptCallWindowVisible = true;
    mapper->MapListWindowVisible = true;
    mapper->MapWindowVisible = true;
    mapper->HistoryWindowVisible = true;
    mapper->SettingsWindowVisible = true;

    auto map = mapper->LoadMapFromText("ControlsMap", "ControlsMap.fomap", MakeMapText(MakeItemBlock(11, TILE_A, 5, 5) + MakeCritterBlock(12, CRITTER_A, 6, 6)));
    REQUIRE(map != nullptr);
    mapper->ShowMap(map.as_ptr());
    mapper->SelectAll();

    // The two folded sections of the Controls window hold most of its checkboxes
    constexpr std::array CONTROLS_HEADER_IDS = {"Visibility", "Selection"};

    auto seed_controls_headers = [&] {
        if (auto controls_window = make_nptr(ImGui::FindWindowByName("Controls"))) {
            for (string_view header_id : CONTROLS_HEADER_IDS) {
                controls_window->StateStorage.SetInt(controls_window->GetID(header_id.data(), header_id.data() + header_id.size()), 1);
            }
        }
    };

    // A press is addressed to one panel, so only that panel is drawn around it: a neighbouring window that
    // asks for keyboard focus while it appears would otherwise take over the queued activation
    auto press = [&seed_controls_headers](string_view window_name, string_view control_label, auto&& draw_panel) {
        INFO(window_name);
        INFO(control_label);

        auto draw_frame = [&seed_controls_headers, &draw_panel] {
            ImGui::NewFrame();
            seed_controls_headers();
            REQUIRE_NOTHROW(draw_panel());
            ImGui::Render();
        };

        // ImGui consumes a queued activation when it meets the widget again, so the first frame submits
        // the widget and the second one runs the branch behind it
        draw_frame();
        REQUIRE(ImGuiTestHarness::ActivateItem(window_name, control_label));
        draw_frame();
    };

    // Controls laid out inside a child window belong to the child's id stack, not the panel's
    auto press_child = [&seed_controls_headers](string_view window_name, std::initializer_list<string_view> child_labels, string_view control_label, auto&& draw_panel) {
        INFO(window_name);
        INFO(control_label);

        auto draw_frame = [&seed_controls_headers, &draw_panel] {
            ImGui::NewFrame();
            seed_controls_headers();
            REQUIRE_NOTHROW(draw_panel());
            ImGui::Render();
        };

        draw_frame();
        REQUIRE(ImGuiTestHarness::ActivateChildItem(window_name, child_labels, control_label));
        draw_frame();
    };

    auto draw_controls = [&mapper] { mapper->DrawMapWindowImGui(); };
    auto draw_content = [&mapper] { mapper->DrawContentWindowImGui(); };

    SECTION("TheMapControlsChangeTheShownMap")
    {
        float32_t zoom_before = map->GetSpritesZoomTarget();
        press("Controls", "Zoom in", draw_controls);
        CHECK(map->GetSpritesZoomTarget() > zoom_before);

        press("Controls", "Zoom out", draw_controls);
        press("Controls", "Zoom reset", draw_controls);
        CHECK(map->GetSpritesZoomTarget() == 1.0f);

        bool overlay_before = mapper->MapperHexOverlayVisible;
        press("Controls", "Toggle hex", draw_controls);
        CHECK(mapper->MapperHexOverlayVisible != overlay_before);

        press("Controls", "Time +1h", draw_controls);
        press("Controls", "Time -1h", draw_controls);

        mdir dir_before = mapper->CritterDir;
        press("Controls", strex("Rotate preview dir ({})", dir_before).str(), draw_controls);
        CHECK(mapper->CritterDir != dir_before);

        press("Controls", "Rotate selected critters", draw_controls);

        bool roof_preview_before = mapper->PreviewRoofTiles;
        press("Controls", "Roof preview", draw_controls);
        CHECK(mapper->PreviewRoofTiles != roof_preview_before);

        bool axial_before = mapper->SelectAxialGrid;
        press("Controls", "Axial grid selection", draw_controls);
        CHECK(mapper->SelectAxialGrid != axial_before);

        bool entire_before = mapper->SelectEntireEntity;
        press("Controls", "Select entire entity", draw_controls);
        CHECK(mapper->SelectEntireEntity != entire_before);

        press("Controls", "Scroll check", draw_controls);

        // The folded groups carry the layer toggles the renderer reads
        bool show_items_before = settings.ShowItem;
        press("Controls", "Items", draw_controls);
        CHECK(settings.ShowItem != show_items_before);

        for (string_view layer_label : {"Scenery", "Walls", "Critters", "Tiles", "Roof", "Fast"}) {
            press("Controls", layer_label, draw_controls);
        }
    }

    SECTION("TheToolWindowsRunTheirCommands")
    {
        // The workspace layer buttons rebuild the map, and its tab list is what switches the panel mode
        auto draw_workspace = [&mapper] { mapper->DrawWorkspaceWindowImGui(); };

        bool workspace_items_before = settings.ShowItem;
        press("Workspace", "Items", draw_workspace);
        CHECK(settings.ShowItem != workspace_items_before);

        for (string_view layer_button : {"Scenery", "Walls", "Critters", "Tiles", "Roof", "Fast"}) {
            INFO(layer_button);
            press("Workspace", layer_button, draw_workspace);
        }

        press("Critter Animations", "Play pair", [&mapper] { mapper->DrawCritterAnimationsWindowImGui(); });
        press("Critter Animations", "Play sequence", [&mapper] { mapper->DrawCritterAnimationsWindowImGui(); });
        press("Script Call", "Run script", [&mapper] { mapper->DrawScriptCallWindowImGui(); });
        press("Workspace", "Open Content", [&mapper] { mapper->DrawWorkspaceWindowImGui(); });

        press("Settings", "Reset layout", [&mapper] { mapper->DrawSettingsWindowImGui(); });
        CHECK(mapper->ResetImGuiSettingsRequested);

        press("Settings", "Fullscreen", [&mapper] { mapper->DrawSettingsWindowImGui(); });
        press("Settings", "Fullscreen", [&mapper] { mapper->DrawSettingsWindowImGui(); });
    }

    SECTION("TheMapCommandsLoadSaveAndUnload")
    {
        mapper->SetActivePanelMode(MapperEngine::INT_MODE_LIST);

        // Picking another loaded map out of the browser list is what switches the shown map
        auto other_map = mapper->LoadMapFromText("OtherControlsMap", "OtherControlsMap.fomap", MakeMapText(MakeItemBlock(13, TILE_A, 4, 4)));
        REQUIRE(other_map != nullptr);
        // The list labels a map by its own name and marks the current one with a leading asterisk
        string other_map_label = string {other_map->GetName()};
        press_child("Content", {"##LoadedMaps"}, other_map_label, draw_content);
        CHECK(mapper->GetCurMap() == other_map.as_ptr());

        // Picking the already-current map is the branch that must not reload it
        press_child("Content", {"##LoadedMaps"}, strex("* {}", other_map_label).str(), draw_content);
        CHECK(mapper->GetCurMap() == other_map.as_ptr());

        // An empty name input makes load and save-as no-ops, which is the branch a user hits first
        press("Content", "Load", draw_content);
        press("Content", "Save As", draw_content);

        // Saving resolves both the file name and the container from the loaded map files, so the result
        // may be a new .fomap or a splice into an existing one - either way the root grows
        auto maps_root_bytes = [&maps_dir] {
            uintmax_t total = 0;

            for (const auto& entry : std::filesystem::directory_iterator {maps_dir}) {
                if (entry.is_regular_file()) {
                    total += entry.file_size();
                }
            }

            return total;
        };

        uintmax_t bytes_before_save = maps_root_bytes();
        press("Content", "Save current", draw_content);
        CHECK(maps_root_bytes() > bytes_before_save);

        press("Content", "Resave all", draw_content);
        press("Content", "New map", draw_content);
        press("Content", "Unload current", draw_content);
    }

    SECTION("TheMessageLogCanBeClearedAndCopied")
    {
        mapper->SetActivePanelMode(MapperEngine::INT_MODE_MESS);
        mapper->AddMess("Control sweep message");
        REQUIRE_FALSE(mapper->MessBox.empty());

        press("Content", "Copy all", draw_content);
        CHECK_FALSE(mapper->MessBox.empty());

        press("Content", "Clear", draw_content);
        CHECK(mapper->MessBox.empty());
    }
}

TEST_CASE("MapperEditorOperations")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7) + MakeItemBlock(12, TILE_A, 9, 9);
    auto map = mapper->LoadMapFromText("EditorMap", "EditorMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);

    mapper->ShowMap(map.as_ptr());
    REQUIRE(mapper->GetCurMap() != nullptr);

    SECTION("PanelModesAndCursorModesSwitch")
    {
        for (int32_t mode = MapperEngine::INT_MODE_CUSTOM0; mode < MapperEngine::INT_MODE_COUNT; mode++) {
            REQUIRE_NOTHROW(mapper->SetActivePanelMode(mode));
        }

        REQUIRE_NOTHROW(mapper->SetCurMode(MapperEngine::CUR_MODE_DEFAULT));
        REQUIRE_NOTHROW(mapper->SetCurMode(MapperEngine::CUR_MODE_MOVE_SELECTION));
        REQUIRE_NOTHROW(mapper->SetCurMode(MapperEngine::CUR_MODE_PLACE_OBJECT));
        REQUIRE_NOTHROW(mapper->SetCurMode(MapperEngine::CUR_MODE_DEFAULT));

        REQUIRE_NOTHROW(mapper->ChangeZoom(1.5f));
        REQUIRE_NOTHROW(mapper->ChangeZoom(0.5f));
        REQUIRE_NOTHROW(mapper->ChangeZoom(1.0f));
    }

    SECTION("SelectionBufferRoundTripAndUndo")
    {
        size_t initial_items = map->GetItems().size();
        REQUIRE(initial_items >= 3);

        mapper->SelectAll();
        REQUIRE_NOTHROW(mapper->BufferCopy());
        REQUIRE_NOTHROW(mapper->BufferPaste());

        // Pasting the whole selection at least preserves what was there
        CHECK(map->GetItems().size() >= initial_items);

        mapper->SelectAll();
        REQUIRE_NOTHROW(mapper->BufferCut());

        if (mapper->CanUndo()) {
            CHECK_FALSE(mapper->GetUndoLabel().empty());
            CHECK(mapper->ExecuteUndo());
            CHECK(mapper->CanRedo());
            CHECK_FALSE(mapper->GetRedoLabel().empty());

            // Redo used to escape with "Lookup failed in vec" here, because undo clears the selection and
            // the replayed delete then failed to find its entity in the selection vector
            CHECK(mapper->ExecuteRedo());
            CHECK(mapper->CanUndo());
        }
    }

    SECTION("ConsoleAcceptsCommands")
    {
        // The console is the mapper's scripted command surface; an unknown command must not throw
        mapper->ConsoleStr = "~help";
        REQUIRE_NOTHROW(mapper->ConsoleSubmitCommand());

        mapper->ConsoleStr = "";
        REQUIRE_NOTHROW(mapper->ConsoleSubmitCommand());

        mapper->ConsoleStr = "totally unknown mapper command";
        REQUIRE_NOTHROW(mapper->ConsoleSubmitCommand());
    }

    SECTION("MapLifecycleUnloadsCleanly")
    {
        REQUIRE_NOTHROW(mapper->UnloadMap(map.as_ptr()));
        CHECK_FALSE(mapper->CanUndo());
    }
}

TEST_CASE("MapperViewerAndParticleEditorPanelsDrawHeadlessly")
{
    auto settings = MakeMapperTestSettings();

    // The particle preview sub-editor initialises only when a preview effect is configured and a map is
    // shown; without both it returns immediately and none of its windows ever draw
    BakerTests::OverrideSetting(settings.ParticlePreviewEffect, string {"Particles/MapperEditorTest.spk"});

    // Every particle renderer draws a wireframe overlay on top of its geometry when this is on
    BakerTests::OverrideSetting(settings.DrawWireframe, true);
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // The viewers and the particle editor are mapper-hosted tool windows, so they take their
    // dependencies straight off the running mapper engine
    // The viewer only selects a critter through a list click or through its persisted "last critter"
    // setting, so the setting is seeded here and restored afterwards to keep the user store untouched
    string saved_selected_proto;

    string saved_selected_particle;

    {
        SettingsStorage viewer_settings {"AnimationViewer"};
        saved_selected_proto = viewer_settings.GetString("SelectedProto");
        viewer_settings.SetString("SelectedProto", CRITTER_A);
    }

    {
        SettingsStorage particle_settings {"ParticleViewer"};
        saved_selected_particle = particle_settings.GetString("SelectedPath");
        particle_settings.SetString("SelectedPath", "Particles/MapperEditorTest.spk");
    }

    auto restore_viewer_settings = scope_exit([&saved_selected_proto, &saved_selected_particle]() noexcept {
        safe_call([&saved_selected_proto] {
            SettingsStorage viewer_settings {"AnimationViewer"};
            viewer_settings.SetString("SelectedProto", saved_selected_proto);
        });

        safe_call([&saved_selected_particle] {
            SettingsStorage particle_settings {"ParticleViewer"};
            particle_settings.SetString("SelectedPath", saved_selected_particle);
        });
    });

    AnimationViewer animation_viewer {mapper.as_ptr(), &mapper->SprMngr, &mapper->ResMngr, &mapper->GameTime};
    ParticleViewer particle_viewer {mapper.as_ptr(), &mapper->SprMngr};
    ParticleEditorManager particle_editor {mapper.as_ptr()};

    auto preview_map = mapper->LoadMapFromText("PreviewMap", "PreviewMap.fomap", MakeMapText(MakeItemBlock(30, TILE_A, 5, 5)));
    REQUIRE(preview_map != nullptr);
    mapper->ShowMap(preview_map.as_ptr());

    REQUIRE_NOTHROW(particle_editor.Initialize());

    auto editor_shutdown = scope_exit([&particle_editor]() noexcept { safe_call([&particle_editor] { particle_editor.Shutdown(); }); });

    CHECK_FALSE(animation_viewer.IsVisible());
    animation_viewer.SetVisible(true);
    animation_viewer.SetFillViewport(true);
    CHECK(animation_viewer.IsVisible());

    particle_viewer.SetVisible(true);
    particle_viewer.SetFillViewport(false);
    CHECK(particle_viewer.IsVisible());

    // Initialize opened the preview window and placed the configured effect, so the frames below draw the
    // populated panel; resetting the layout first would close it and leave only the early return covered
    REQUIRE_NOTHROW(particle_editor.OnFocusGained());

    constexpr int32_t VIEWER_FRAMES = 3;

    for (int32_t frame = 0; frame < VIEWER_FRAMES; frame++) {
        ImGui::NewFrame();
        ImGui::LogToBuffer(12);

        REQUIRE_NOTHROW(animation_viewer.Draw());
        REQUIRE_NOTHROW(particle_viewer.Draw());
        REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
        REQUIRE_NOTHROW(particle_editor.DrawWindows());

        ImGui::LogFinish();
        ImGui::Render();
    }

    // Nothing below the resource list runs until a control is pressed, so each press is queued against
    // the drawn window and consumed by the frame that follows it
    constexpr std::array PRESSED_CONTROLS = {"Refresh", "Mouse position", "View center", "Play", "Restart", "Remove"};

    for (string_view control : PRESSED_CONTROLS) {
        INFO(control);
        REQUIRE(ImGuiTestHarness::ActivateItem("Particle Preview", control));

        ImGui::NewFrame();
        REQUIRE_NOTHROW(particle_editor.DrawWindows());
        ImGui::Render();
    }

    // The viewer lays its controls out in child windows, so each press is addressed to the owning child
    for (string_view toggle : {"Direct draw", "Root", "Name level", "Draw rect", "View rect"}) {
        INFO(toggle);
        REQUIRE(ImGuiTestHarness::ActivateChildItem("Animation Viewer", {"Preview"}, toggle));

        ImGui::NewFrame();
        REQUIRE_NOTHROW(animation_viewer.Draw());
        ImGui::Render();
    }

    for (string_view control : {"Loop", "Idle"}) {
        INFO(control);
        REQUIRE(ImGuiTestHarness::ActivateChildItem("Animation Viewer", {"RightColumn", "Animations"}, control));

        ImGui::NewFrame();
        REQUIRE_NOTHROW(animation_viewer.Draw());
        ImGui::Render();
    }

    // Picking a critter from the list is what loads a preview sprite in the first place
    REQUIRE(ImGuiTestHarness::ActivateChildItem("Animation Viewer", {"Critters"}, CRITTER_A));

    ImGui::NewFrame();
    REQUIRE_NOTHROW(animation_viewer.Draw());
    ImGui::Render();

    // The SPARK browser sits behind a menu item, and DrawMenuItems draws into ImGui's implicit window
    // when no real menu bar hosts it - so the item is addressable there
    REQUIRE(ImGuiTestHarness::ActivateItem("Debug##Default", "SPARK particle editor"));

    {
        for (int32_t frame = 0; frame < 3; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
            REQUIRE_NOTHROW(particle_editor.DrawWindows());
            ImGui::LogFinish();
            ImGui::Render();
        }

        // Refreshing the list and picking the one asset in it is what opens an editor window
        for (string_view browser_control : {"Refresh", "Particles/MapperEditorTest.spark"}) {
            INFO(browser_control);
            REQUIRE(ImGuiTestHarness::ActivateItem("SPARK Particle Editor", browser_control));

            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
            REQUIRE_NOTHROW(particle_editor.DrawWindows());
            ImGui::LogFinish();
            ImGui::Render();
        }

        for (int32_t frame = 0; frame < 2; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
            REQUIRE_NOTHROW(particle_editor.DrawWindows());
            ImGui::LogFinish();
            ImGui::Render();
        }
    }

    // Switching away from the previewed map and then unloading it must take the placed sprite with it
    auto second_map = mapper->LoadMapFromText("PreviewMapB", "PreviewMapB.fomap", MakeMapText(MakeItemBlock(31, TILE_A, 6, 6)));
    REQUIRE(second_map != nullptr);
    REQUIRE_NOTHROW(particle_editor.OnCurrentMapChanging(second_map.as_ptr()));
    mapper->ShowMap(second_map.as_ptr());
    REQUIRE_NOTHROW(particle_editor.OnMapUnloading(preview_map.as_ptr()));
    mapper->UnloadMap(preview_map.as_ptr());

    // Closing the panel through the layout reset must tear the preview down instead of leaving it placed
    REQUIRE_NOTHROW(particle_editor.ResetLayout());

    // Hidden windows must be just as safe to drive as visible ones
    animation_viewer.SetVisible(false);
    particle_viewer.SetVisible(false);

    ImGui::NewFrame();
    REQUIRE_NOTHROW(animation_viewer.Draw());
    REQUIRE_NOTHROW(particle_viewer.Draw());
    REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
    REQUIRE_NOTHROW(particle_editor.DrawWindows());
    ImGui::Render();

    REQUIRE_NOTHROW(animation_viewer.SaveSettings());
    REQUIRE_NOTHROW(particle_viewer.SaveSettings());

    CHECK(ImGui::GetFrameCount() >= VIEWER_FRAMES + numeric_cast<int32_t>(PRESSED_CONTROLS.size()) + 9);
}

#if FO_SPARK_PARTICLES

TEST_CASE("SparkParticleEditorDrawsHeadlessly")
{
    auto settings = MakeMapperTestSettings();

    string asset_path = "Particles/MapperEditorTest.spark";

    auto raw_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("SparkEditorRawResources");
    raw_source->AddFile(asset_path, vector<uint8_t>(MAPPER_TEST_SPARK_ASSET.begin(), MAPPER_TEST_SPARK_ASSET.end()));

    FileSystem raw_resources;
    raw_resources.AddCustomSource(std::move(raw_source));

    // The editor previews the *baked* particle, so the raw asset is run through the real baker first
    BakerTests::TestRig rig;
    rig.AddSourceFile(asset_path, MAPPER_TEST_SPARK_ASSET, 10);

    ParticleBaker baker(rig.MakeContext());
    baker.BakeFiles(rig.GetAllSourceFiles(), "");

    string baked_path = strex(asset_path).change_file_extension("spk").str();
    REQUIRE(rig.Outputs.contains(baked_path));

    // The SPARK renderer resolves the effect named in the asset plus the whole atlas-variant family the
    // particle runtime registers up front, so all of them are baked into the fixture
    BakerTests::TestRig effect_rig;

    for (string_view effect_name : {"Effects/Particles_ColorAdd.fofx", "Effects/Particles_ColorAddAtlas.fofx", "Effects/Particles_ColorMulAtlas.fofx", "Effects/Particles_ColorSubAtlas.fofx", "Effects/Particles_DistortionAtlas.fofx", "Effects/Particles_DistortionAddAtlas.fofx"}) {
        effect_rig.AddSourceFile(effect_name, MAPPER_TEST_PARTICLE_EFFECT, 10);
    }

    EffectBaker effect_baker(effect_rig.MakeContext());
    effect_baker.BakeFiles(effect_rig.GetAllSourceFiles(), "");
    REQUIRE(effect_rig.Outputs.contains("Effects/Particles_ColorAdd.fofx"));

    auto baked_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("SparkEditorBakedResources");
    baked_source->AddFile(baked_path, rig.Outputs.at(baked_path));

    for (const auto& [output_path, output_data] : effect_rig.Outputs) {
        baked_source->AddFile(output_path, output_data);
    }

    // The renderer resolves its texture relative to the particle asset directory
    baked_source->AddFile("Particles/TestParticle.png", BakerTests::MakeMinimalBakedSprite(4, 4));

    FileSystem baked_resources;
    baked_resources.AddCustomSource(std::move(baked_source));

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    int32_t saved_calls = 0;
    SparkParticleEditor editor {asset_path, &settings, &raw_resources, &baked_resources, [&saved_calls](string_view) { saved_calls++; }};

    CHECK(editor.GetAssetPath() == asset_path);
    CHECK_FALSE(editor.IsChanged());

    editor.BringToFront();

    // The window carries a stable "###" id suffix, which is what ImGui matches a window by
    string editor_window_title = strex("###SparkParticleEditor_{}", asset_path).str();

    // Logging auto-expands the object tree so the per-node inspectors run instead of staying collapsed
    constexpr int32_t EDITOR_FRAMES = 4;

    for (int32_t frame = 0; frame < EDITOR_FRAMES; frame++) {
        ImGui::NewFrame();
        ImGui::LogToBuffer(12);

        bool keep_open = true;
        REQUIRE_NOTHROW(keep_open = editor.Draw());
        CHECK(keep_open);

        ImGui::LogFinish();
        ImGui::Render();
    }

    // The editor's own controls live in a child window and only run when something is pressed
    constexpr std::array INFO_CONTROLS = {"Adding mode", "Removing mode", "Naming mode", "Auto replay", "Respawn"};

    for (string_view control : INFO_CONTROLS) {
        INFO(control);
        REQUIRE(ImGuiTestHarness::ActivateChildItem(editor_window_title, {"Info"}, control));

        ImGui::NewFrame();
        ImGui::LogToBuffer(12);
        REQUIRE_NOTHROW((void)editor.Draw());
        ImGui::LogFinish();
        ImGui::Render();
    }

    editor.Hide();

    ImGui::NewFrame();
    REQUIRE_NOTHROW((void)editor.Draw());
    ImGui::Render();

    // Nothing above edits the asset, so the editor must not have written anything back
    CHECK(saved_calls == 0);
}

#endif

TEST_CASE("MapperProcessesInputEventsAndDrawsFrame")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7);
    auto map = mapper->LoadMapFromText("InputMap", "InputMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);
    mapper->ShowMap(map.as_ptr());

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    auto make_key_event = [](InputEvent::EventType type, KeyCode code, string_view text = {}) {
        InputEvent ev;
        ev.Type = type;

        if (type == InputEvent::EventType::KeyDownEvent) {
            ev.KeyDown.Code = code;
            ev.KeyDown.Text = string {text};
        }
        else {
            ev.KeyUp.Code = code;
        }

        return ev;
    };

    SECTION("MouseAndWheelEventsAreProcessed")
    {
        ImGui::NewFrame();

        InputEvent move;
        move.Type = InputEvent::EventType::MouseMoveEvent;
        move.MouseMove.MouseX = 100;
        move.MouseMove.MouseY = 100;
        move.MouseMove.DeltaX = 4;
        move.MouseMove.DeltaY = 4;
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(move));

        for (MouseButton button : {MouseButton::Left, MouseButton::Right, MouseButton::Middle}) {
            InputEvent down;
            down.Type = InputEvent::EventType::MouseDownEvent;
            down.MouseDown.Button = button;
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(down));

            InputEvent up;
            up.Type = InputEvent::EventType::MouseUpEvent;
            up.MouseUp.Button = button;
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(up));
        }

        InputEvent wheel;
        wheel.Type = InputEvent::EventType::MouseWheelEvent;
        wheel.MouseWheel.Delta = 3;
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(wheel));
        wheel.MouseWheel.Delta = -3;
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(wheel));

        REQUIRE_NOTHROW(mapper->ProcessRightMouseInertia());

        ImGui::Render();
    }

    SECTION("SelectingAndDraggingOverTheMapDrivesTheEditorCursor")
    {
        auto mouse_move = [](int32_t x, int32_t y, int32_t dx, int32_t dy) {
            InputEvent ev;
            ev.Type = InputEvent::EventType::MouseMoveEvent;
            ev.MouseMove.MouseX = x;
            ev.MouseMove.MouseY = y;
            ev.MouseMove.DeltaX = dx;
            ev.MouseMove.DeltaY = dy;
            return ev;
        };

        auto mouse_button = [](InputEvent::EventType type, MouseButton button) {
            InputEvent ev;
            ev.Type = type;

            if (type == InputEvent::EventType::MouseDownEvent) {
                ev.MouseDown.Button = button;
            }
            else {
                ev.MouseUp.Button = button;
            }

            return ev;
        };

        // Press over the map, drag across it and release: this is the rubber-band selection path, which is
        // where the cursor modes, the hit tests and the selection move live
        for (int32_t mode : {MapperEngine::INT_MODE_ITEM, MapperEngine::INT_MODE_TILE, MapperEngine::INT_MODE_CRIT}) {
            mapper->SetActivePanelMode(mode);

            ImGui::NewFrame();
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(300, 300, 0, 0)));
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseDownEvent, MouseButton::Left)));

            for (int32_t step = 1; step <= 4; step++) {
                REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(300 + step * 20, 300 + step * 15, 20, 15)));
                REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            }

            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseUpEvent, MouseButton::Left)));
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            ImGui::Render();
        }

        // With everything selected, pressing on a selected entity switches the drag into selection-move
        // instead of rubber-banding, so the press has to land on a hex that actually holds one
        mapper->SelectAll();
        REQUIRE_FALSE(mapper->SelectedEntities.empty());

        // Rather than deriving the screen point from a hex, ask the map itself where something is: the
        // press has to land on a hit-testable entity for the drag to become a selection move
        ptr<MapView> cur_map = mapper->GetCurMap().as_ptr();
        cur_map->InstantScrollTo(mpos {5, 5});
        cur_map->RebuildMap();

        ipos32 hex_screen_pos {};
        bool found_entity = false;

        for (int32_t y = 0; y < 720 && !found_entity; y += 8) {
            for (int32_t x = 0; x < 1280 && !found_entity; x += 8) {
                if (cur_map->GetEntityAtScreen(ipos32 {x, y}, 0, false).first) {
                    hex_screen_pos = ipos32 {x, y};
                    found_entity = true;
                }
            }
        }

        CHECK(found_entity);

        ImGui::NewFrame();
        mapper->SetCurMode(MapperEngine::CUR_MODE_MOVE_SELECTION);
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(hex_screen_pos.x, hex_screen_pos.y, 0, 0)));
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseDownEvent, MouseButton::Left)));

        for (int32_t step = 1; step <= 3; step++) {
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(hex_screen_pos.x + step * 24, hex_screen_pos.y + step * 18, 24, 18)));
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
        }

        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseUpEvent, MouseButton::Left)));
        REQUIRE_NOTHROW(mapper->DrawMapperFrame());
        ImGui::Render();

        // Placing mode drops a new object under the cursor
        ImGui::NewFrame();
        mapper->SetCurMode(MapperEngine::CUR_MODE_PLACE_OBJECT);
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(360, 340, 0, 0)));
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseDownEvent, MouseButton::Left)));
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseUpEvent, MouseButton::Left)));
        REQUIRE_NOTHROW(mapper->DrawMapperFrame());
        ImGui::Render();

        mapper->SetCurMode(MapperEngine::CUR_MODE_DEFAULT);
    }

    SECTION("FullEditorFrameRenders")
    {
        // DrawMapperFrame drives the whole editor frame: map draw, interface events and every panel
        for (int32_t frame = 0; frame < 2; frame++) {
            ImGui::NewFrame();
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            ImGui::Render();
        }
    }

    SECTION("FrameRendersAcrossZoomAndScrollStates")
    {
        // A denser map plus zoom/scroll changes walks far more of the map-view draw path than a
        // single static frame does
        string dense;
        for (int32_t i = 0; i < 12; i++) {
            dense += MakeItemBlock(100 + i, TILE_A, 4 + i, 4 + (i % 3));
            dense += MakeItemBlock(200 + i, TILE_B, 6 + i, 8 + (i % 4));
        }

        auto dense_map = mapper->LoadMapFromText("DenseMap", "DenseMap.fomap", MakeMapText(dense));
        REQUIRE(dense_map != nullptr);
        mapper->ShowMap(dense_map.as_ptr());

        vector<float32_t> zooms {1.0f, 2.0f, 0.5f, 1.0f};

        for (float32_t zoom : zooms) {
            REQUIRE_NOTHROW(mapper->ChangeZoom(zoom));

            InputEvent scroll;
            scroll.Type = InputEvent::EventType::MouseWheelEvent;
            scroll.MouseWheel.Delta = zoom > 1.0f ? 2 : -2;
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(scroll));

            ImGui::NewFrame();
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            ImGui::Render();
        }

        // Selecting everything makes the frame draw selection decoration on every item too
        mapper->SelectAll();

        ImGui::NewFrame();
        REQUIRE_NOTHROW(mapper->DrawMapperFrame());
        ImGui::Render();

        mapper->UnloadMap(dense_map.as_ptr());
    }

    SECTION("KeyboardHotkeysAreProcessed")
    {
        ImGui::NewFrame();

        // The editor's hotkey tables are wide switch statements, so the whole common key range is walked
        vector<KeyCode> keys {
            KeyCode::Escape,
            KeyCode::Delete,
            KeyCode::Tab,
            KeyCode::Space,
            KeyCode::Up,
            KeyCode::Down,
            KeyCode::Left,
            KeyCode::Right,
            KeyCode::A,
            KeyCode::B,
            KeyCode::C,
            KeyCode::D,
            KeyCode::E,
            KeyCode::F,
            KeyCode::G,
            KeyCode::H,
            KeyCode::L,
            KeyCode::M,
            KeyCode::S,
            KeyCode::V,
            KeyCode::X,
            KeyCode::Z,
            KeyCode::C1,
            KeyCode::C2,
            KeyCode::C3,
            KeyCode::C0,
        };

        for (KeyCode key : keys) {
            REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->HandleShiftMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->HandleCtrlMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->UpdateArrowScrollKeys(key, KeyCode::None));
            REQUIRE_NOTHROW(mapper->UpdateArrowScrollKeys(KeyCode::None, key));
        }

        // The function keys drive the panel toggles, the view modes and the window state, and F8/F11/F12
        // reach through to the process-global app window, so the original state is put back afterwards
        bool saved_fullscreen = GetApp()->MainWindow.IsFullscreen();

        auto restore_fullscreen = scope_exit([saved_fullscreen]() noexcept { safe_call([saved_fullscreen] { (void)GetApp()->MainWindow.ToggleFullscreen(saved_fullscreen); }); });

        // With an entity selected and the inspector up, F9, Delete and Escape take their other branches.
        // This runs before the sweep below, which toggles the layer visibility a selection depends on.
        mapper->SelectAll();
        REQUIRE_FALSE(mapper->SelectedEntities.empty());
        mapper->InspectorVisible = true;
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::F9, false));
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::F9, false));
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::Escape, false));
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::Delete, false));

        for (KeyCode function_key : {KeyCode::F1, KeyCode::F2, KeyCode::F3, KeyCode::F4, KeyCode::F5, KeyCode::F6, KeyCode::F7, KeyCode::F8, KeyCode::F9, KeyCode::F10, KeyCode::F11, KeyCode::F12, KeyCode::Add, KeyCode::Subtract}) {
            REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(function_key, false));
        }

        // The shift and ctrl tables both early-out on GetApp()->Input.IsShiftDown()/IsCtrlDown(), which the
        // real InputSystem only ever sets from an SDL modifier state. A pushed or simulated key event does
        // not update it, so their bodies stay unreachable from a test - see the plan note on simulated
        // modifier state.
        for (KeyCode shift_key : {KeyCode::F7, KeyCode::F11, KeyCode::C0, KeyCode::Numpad0, KeyCode::Tab}) {
            REQUIRE_NOTHROW(mapper->HandleShiftMapperHotkeys(shift_key, false));
        }

        for (KeyCode ctrl_key : {KeyCode::A, KeyCode::C, KeyCode::X, KeyCode::V, KeyCode::Z, KeyCode::Y, KeyCode::D, KeyCode::B}) {
            REQUIRE_NOTHROW(mapper->HandleCtrlMapperHotkeys(ctrl_key, false));
        }

        // Blocked hotkeys must take the early-out path instead of acting
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::Delete, true));
        REQUIRE_NOTHROW(mapper->HandleShiftMapperHotkeys(KeyCode::Delete, true));
        REQUIRE_NOTHROW(mapper->HandleCtrlMapperHotkeys(KeyCode::Z, true));

        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(make_key_event(InputEvent::EventType::KeyDownEvent, KeyCode::A, "a")));
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(make_key_event(InputEvent::EventType::KeyUpEvent, KeyCode::A)));

        REQUIRE_NOTHROW(mapper->HandleMapperConsoleKeyDown(KeyCode::A, "a"));
        REQUIRE_NOTHROW(mapper->HandleMapperConsoleKeyDown(KeyCode::Back, {}));
        REQUIRE_NOTHROW(mapper->HandleMapperConsoleKeyDown(KeyCode::Return, {}));

        ImGui::Render();
    }
}

TEST_CASE("MapViewLightingAndViewportOperations")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    // A light-emitting item drives the light fan machinery: tracing, marking the fan ends and cleaning it up
    // on every rebuild. Without a light source those paths never run, whatever else the map contains.
    string light_props = "LightSource = true\nLightIntensity = 50\nLightDistance = 6\nLightFlags = 0\nLightColor = 0xFFFFFFFF";
    string body = MakeItemBlock(10, TILE_A, 8, 8, light_props) + MakeItemBlock(11, TILE_B, 12, 12, light_props) + MakeItemBlock(12, TILE_A, 5, 5);

    // A map critter exercises the critter map view alongside the item paths. Its light properties are
    // client-scoped, so they cannot be authored in the map text and are switched on at runtime below.
    body += MakeCritterBlock(20, CRITTER_A, 6, 6);
    body += MakeCritterBlock(21, CRITTER_A, 10, 10);

    auto map = mapper->LoadMapFromText("LightMap", "LightMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);
    mapper->ShowMap(map.as_ptr());

    ptr<MapView> map_ptr = map.as_ptr();

    for (auto& map_critter : map_ptr->GetCritters()) {
        map_critter->SetLightSource(true);
        map_critter->SetLightIntensity(int8_t {50});
        map_critter->SetLightDistance(int16_t {6});
    }

    map_ptr->RebuildMap();

    SECTION("CritterViewsAnimateAndMoveOverFrames")
    {
        // A critter view only advances its animation while the map is processed, so nothing under the
        // per-frame animation walk runs in a test that just builds the map and looks at it
        auto critters = map_ptr->GetCritters();
        REQUIRE_FALSE(critters.empty());

        ptr<CritterHexView> critter = critters.front().as_ptr();

        auto process_frames = [&map_ptr](int32_t count) {
            for (int32_t frame = 0; frame < count; frame++) {
                REQUIRE_NOTHROW(map_ptr->Process());
            }
        };

        // Queued animations play one after another, and appending adds to the queue instead of replacing
        REQUIRE_NOTHROW(critter->AppendAnim(CritterStateAnim::Unarmed, CritterActionAnim::Idle, nullptr));
        REQUIRE_NOTHROW(critter->AppendAnim(CritterStateAnim::Unarmed, CritterActionAnim::Walk, nullptr));
        process_frames(12);

        // The action path picks its own animation for each kind of action
        for (CritterAction action : {CritterAction::Refresh, CritterAction::MoveItem, CritterAction::Knockout, CritterAction::StandUp, CritterAction::Dead, CritterAction::Respawn}) {
            REQUIRE_NOTHROW(critter->Action(action, 0, nullptr, true));
            process_frames(3);
        }

        // Direction changes and the view refresh rebuild the sprite the animation walks over
        for (int32_t dir = 0; dir < 6; dir++) {
            REQUIRE_NOTHROW(critter->ChangeLookDir(mdir {numeric_cast<uint8_t>(dir)}));
            REQUIRE_NOTHROW(critter->ChangeMoveDir(mdir {numeric_cast<uint8_t>(dir)}));
            process_frames(2);
        }

        REQUIRE_NOTHROW(critter->RefreshView(false));
        REQUIRE_NOTHROW(critter->RefreshView(true));
        REQUIRE_NOTHROW(critter->RefreshModel());
        process_frames(4);

        ignore_unused(critter->IsAnimPlaying());
        ignore_unused(critter->IsMoving());

        ipos32 name_pos {};
        ignore_unused(critter->GetNameTextPos(name_pos));
        ignore_unused(critter->GetViewRect());
    }

    SECTION("LightSourcesSurviveRebuildsAndMoves")
    {
        REQUIRE_NOTHROW(map_ptr->RebuildMap());

        auto items = map_ptr->GetItems();
        REQUIRE_FALSE(items.empty());

        // Moving a light source retraces its fan from the new hex
        for (auto& item : items) {
            if (item->GetLightSource()) {
                REQUIRE_NOTHROW(map_ptr->MoveItem(item.as_ptr(), mpos {9, 9}));
                break;
            }
        }

        REQUIRE_NOTHROW(map_ptr->RebuildMap());
    }

    SECTION("CritterViewsProcessAnimationAndVisibility")
    {
        auto critters = map_ptr->GetCritters();
        REQUIRE_FALSE(critters.empty());

        for (auto& cr : critters) {
            ptr<CritterHexView> cr_ptr = cr.as_ptr();

            REQUIRE_NOTHROW(cr_ptr->RefreshView());
            REQUIRE_NOTHROW(cr_ptr->ChangeDir(mdir {2}));
            REQUIRE_NOTHROW(cr_ptr->StopAnim());
            REQUIRE_NOTHROW(cr_ptr->Process());
        }

        REQUIRE_NOTHROW(map_ptr->Process());
        REQUIRE_NOTHROW(map_ptr->RebuildMap());
    }

    SECTION("ScrollingAndZoomingRetraceTheVisibleArea")
    {
        REQUIRE_NOTHROW(map_ptr->ChangeZoom(1.5f, fpos32 {}));
        REQUIRE_NOTHROW(map_ptr->ChangeZoom(0.5f, fpos32 {}));
        REQUIRE_NOTHROW(map_ptr->ChangeZoom(1.0f, fpos32 {}));

        REQUIRE_NOTHROW(map_ptr->InstantScrollTo(mpos {12, 12}));
        REQUIRE_NOTHROW(map_ptr->InstantScrollTo(mpos {2, 2}));

        // Auto-scroll and auto-zoom advance from the frame delta, so the pumping has to let real time pass
        // between frames or the animation steps are all zero-length and never run
        REQUIRE_NOTHROW(map_ptr->ScrollToHex(mpos {8, 8}, ipos16 {}, 5, false));

        for (int32_t i = 0; i < 8; i++) {
            mapper->GameTime.FrameAdvance(true);
            REQUIRE_NOTHROW(map_ptr->Process());
            std::this_thread::sleep_for(std::chrono::milliseconds {12});
        }

        REQUIRE_NOTHROW(map_ptr->ApplyScrollOffset(ipos32 {20, 20}, 5, false));

        for (int32_t i = 0; i < 8; i++) {
            mapper->GameTime.FrameAdvance(true);
            REQUIRE_NOTHROW(map_ptr->Process());
            std::this_thread::sleep_for(std::chrono::milliseconds {12});
        }

        REQUIRE_NOTHROW(map_ptr->SetScreenSize(isize32 {800, 600}));
        REQUIRE_NOTHROW(map_ptr->SetScreenSize(isize32 {1024, 768}));
    }

    SECTION("TheMapDrawsWithLightsAndFog")
    {
        REQUIRE(ImGui::GetCurrentContext() == nullptr);
        ImGuiExt::Init();

        auto destroy_context = scope_exit([]() noexcept {
            safe_call([] {
                if (ImGui::GetCurrentContext() != nullptr) {
                    ImGui::DestroyContext();
                }
            });
        });

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2 {1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        map_ptr->AddFog(mpos {8, 8}, DrawOrderType::Light, nullptr);

        for (int32_t frame = 0; frame < 3; frame++) {
            ImGui::NewFrame();
            REQUIRE_NOTHROW(map_ptr->Process());
            REQUIRE_NOTHROW(map_ptr->DrawMap());
            ImGui::Render();
        }
    }
}

TEST_CASE("MapperSavesMapsToADiskMapsRoot")
{
    // Saving resolves the on-disk Maps root from an existing map container, so the fixture needs a real
    // directory with a reference .fomap in it - a memory-only resource set can never reach this path.
    auto maps_dir = std::filesystem::temp_directory_path() / std::format("fo_engine_mapper_save_test_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code remove_error;
    std::filesystem::remove_all(maps_dir, remove_error);
    std::filesystem::create_directories(maps_dir);

    auto cleanup_maps_dir = scope_exit([maps_dir]() noexcept {
        safe_call([maps_dir] {
            std::error_code ignored;
            std::filesystem::remove_all(maps_dir, ignored);
        });
    });

    string reference_map = MakeMapText(MakeItemBlock(10, TILE_A, 5, 5));
    REQUIRE(fs_write_file((maps_dir / "ReferenceMap.fomap").generic_string(), reference_map));

    auto settings = MakeMapperTestSettings();
    BakerTests::OverrideSetting(settings.ProtoFileExtensions, vector<string> {"fopro", "fomap"});
    auto pack_config = ConfigFile(strex("[ResourcePack]\nName = MapperSaveTestPack\nInputDirs = {}\n", maps_dir.generic_string()).str());
    settings.ApplyConfigFile(pack_config, "");

    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    auto map = mapper->LoadMapFromText("SaveMap", "SaveMap.fomap", MakeMapText(MakeItemBlock(11, TILE_B, 6, 6)));
    REQUIRE(map != nullptr);
    mapper->ShowMap(map.as_ptr());

    SECTION("SavingIntoASubDirectoryWritesTheFile")
    {
        REQUIRE_NOTHROW(mapper->SaveMapToDir(map.as_ptr(), "Generated", "SavedByTest"));
        CHECK(std::filesystem::exists(maps_dir / "Generated" / "SavedByTest.fomap"));

        // Re-saving the same name overwrites in place instead of appending a second container
        REQUIRE_NOTHROW(mapper->SaveMapToDir(map.as_ptr(), "Generated", "SavedByTest"));
        CHECK(std::filesystem::exists(maps_dir / "Generated" / "SavedByTest.fomap"));
    }

    SECTION("SavingUnderTheReferenceContainerSplicesIntoIt")
    {
        // SaveMap falls back to the first source file's directory when the map has no container of its
        // own, which in this fixture is the working directory - so the sandboxed writer is used instead
        REQUIRE_NOTHROW(mapper->SaveMapToDir(map.as_ptr(), "", "SaveMap"));
        CHECK(std::filesystem::exists(maps_dir / "SaveMap.fomap"));
    }

    SECTION("TheMapListWindowEnumeratesTheDiskMapsRoot")
    {
        REQUIRE(ImGui::GetCurrentContext() == nullptr);
        ImGuiExt::Init();

        auto destroy_context = scope_exit([]() noexcept {
            safe_call([] {
                if (ImGui::GetCurrentContext() != nullptr) {
                    ImGui::DestroyContext();
                }
            });
        });

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2 {1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        mapper->MapListWindowVisible = true;

        for (int32_t frame = 0; frame < 3; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(mapper->DrawMapListWindowImGui());
            ImGui::LogFinish();
            ImGui::Render();
        }
    }

    SECTION("SavingAnUnloadedMapIsRejected")
    {
        auto other = mapper->LoadMapFromText("OtherMap", "OtherMap.fomap", MakeMapText(MakeItemBlock(12, TILE_A, 7, 7)));
        REQUIRE(other != nullptr);

        // LoadMapFromText hands back a borrow, and the engine's only owning reference lives in LoadedMaps,
        // so unloading destroys the view outright. Hold an own reference across the unload - otherwise the
        // rejection below reads freed memory instead of exercising the destroyed-map guard.
        refcount_ptr<MapView> unloaded = refcount_ptr<MapView>::from_add_ref(other.as_ptr().get());

        mapper->UnloadMap(other.as_ptr());

        CHECK_THROWS(mapper->SaveMapToDir(unloaded.as_ptr(), "Generated", "NotLoaded"));
    }
}

TEST_CASE("MapperConsoleCommands")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    SECTION("EmptyAndUnknownInputIsIgnored")
    {
        REQUIRE_NOTHROW(mapper->ParseCommand(""));
        REQUIRE_NOTHROW(mapper->ParseCommand("no leading sigil"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*unknown-subcommand"));
    }

    SECTION("MapLifecycleCommandsRunAgainstAFreshMap")
    {
        REQUIRE_NOTHROW(mapper->ParseCommand("*new"));
        REQUIRE(mapper->GetCurMap() != nullptr);

        REQUIRE_NOTHROW(mapper->ParseCommand("*size 40 40"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*reverse-light"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*merge-items"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*break-items"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*unload"));
    }

    SECTION("LoadAndSaveCommandsReportTheirOutcome")
    {
        // Both sigils validate the name before touching the filesystem
        REQUIRE_NOTHROW(mapper->ParseCommand("~"));
        REQUIRE_NOTHROW(mapper->ParseCommand("~NoSuchMapName"));
        REQUIRE_NOTHROW(mapper->ParseCommand("^"));
        REQUIRE_NOTHROW(mapper->ParseCommand("^SomeName"));
    }

    SECTION("ScriptAndAnimationCommandsHandleAMissingTarget")
    {
        REQUIRE_NOTHROW(mapper->ParseCommand("#"));
        REQUIRE_NOTHROW(mapper->ParseCommand("#NoSuchFunction argument"));
        REQUIRE_NOTHROW(mapper->ParseCommand("@"));
        REQUIRE_NOTHROW(mapper->ParseCommand("@1 2"));
    }

    SECTION("AnimationCommandsWalkTheCrittersOfALoadedMap")
    {
        auto map = mapper->LoadMapFromText("ConsoleMap", "ConsoleMap.fomap", MakeMapText(MakeItemBlock(10, TILE_A, 5, 5)));
        REQUIRE(map != nullptr);
        mapper->ShowMap(map.as_ptr());

        REQUIRE_NOTHROW(mapper->ParseCommand("@1 2"));
        REQUIRE_NOTHROW(mapper->ParseCommand("@"));
    }

    SECTION("ConsoleTypingReachesTheParser")
    {
        // The console edit buffer accumulates characters and submits on Return
        mapper->HandleMapperConsoleKeyDown(KeyCode::Return, {});

        for (char ch : string {"*new"}) {
            mapper->HandleMapperConsoleKeyDown(KeyCode::A, string {ch});
        }

        REQUIRE_NOTHROW(mapper->HandleMapperConsoleKeyDown(KeyCode::Return, {}));
    }
}

TEST_CASE("MapperScriptApiCoverage")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7);
    auto map = mapper->LoadMapFromText("ScriptApiMap", "ScriptApiMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);
    mapper->ShowMap(map.as_ptr());

    auto run_script = [&mapper](string_view name) {
        int32_t result = -1;
        INFO(name);
        REQUIRE(mapper->CallFunc(mapper->Hashes.ToHashedString(name), result));
        CHECK(result == 0);
    };

    run_script("MapperMergeTest::UnitTestMapperViewApi");
    run_script("MapperMergeTest::UnitTestMapperEntityApi");
    run_script("MapperMergeTest::UnitTestMapperTabApi");
#if FO_SPARK_PARTICLES
    // The particle sprite factory only has a baked particle to serve when the SPARK fixture is compiled in
    run_script("MapperMergeTest::UnitTestParticleSpriteApi");
#endif
    run_script("MapperMergeTest::UnitTestClientMapApi");
    run_script("MapperMergeTest::UnitTestClientItemApi");
    run_script("MapperMergeTest::UnitTestClientEntityTimeEvents");
    run_script("MapperMergeTest::UnitTestClientCritterApi");
    run_script("MapperMergeTest::UnitTestMapperEditingApi");

    // Last: it creates and unloads maps, so it changes which map is current
    run_script("MapperMergeTest::UnitTestMapperMapLifecycleApi");
}

FO_END_NAMESPACE
