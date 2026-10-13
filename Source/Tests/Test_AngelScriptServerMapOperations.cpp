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

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "Test_ServerMapOperationsHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ServerMapOperationsTests;

TEST_CASE("AngelScriptMapItemOperations")
{
    MAKE_SERVER;

    SECTION("AddItemByPid")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemByPid");
    }

    SECTION("AddItemInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddItemMultiple")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemMultiple");
    }

    SECTION("GameDistanceOverloads")
    {
        RUN_FUNC("MapOpsTest::TestGameDistanceOverloads");
    }

    SECTION("GameDistanceExceptionBranches")
    {
        RUN_FUNC("MapOpsTest::TestGameDistanceExceptionBranches");
    }

    SECTION("GameMoveItemMapAndContainerOverloads")
    {
        RUN_FUNC("MapOpsTest::TestGameMoveItemMapAndContainerOverloads");
    }

    SECTION("GameMoveItemsOverloads")
    {
        RUN_FUNC("MapOpsTest::TestGameMoveItemsOverloads");
    }

    SECTION("GetItemOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemOnHex");
    }

    SECTION("GetItemOnHexInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemOnHexInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetItemOnHexByProtoInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemOnHexByProtoInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetItemOnHexByPropertyInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemOnHexByPropertyInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetItemInRadius")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemInRadius");
    }

    SECTION("GetItemsInRadiusNegativeRadiusThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemsInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("GetItemsInRadiusInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetItemsInRadiusInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetItemsByNullProtoThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemsByNullProtoThrows");
    }
}

TEST_CASE("AngelScriptMapCritterOperations")
{
    MAKE_SERVER;

    SECTION("AddCritter")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritter");
    }

    SECTION("AddCritterInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddCritterByProtoInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterByProtoInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetCritterOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCritterOnHex");
    }

    SECTION("GetCritterOnHexInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetCritterOnHexInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetCrittersByFindType")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersByFindType");
    }

    SECTION("GetCrittersOnHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersOnHex");
    }

    SECTION("GetCrittersInLargeRadius")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersInLargeRadius");
    }

    SECTION("GetCrittersInRadiusHexWalkArm")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersInRadiusHexWalkArm");
    }

    SECTION("GetCrittersInRadiusIncludesMultihexOverlap")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersInRadiusIncludesMultihexOverlap");
    }

    SECTION("GetCrittersInRadiusNegativeRadiusThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetCrittersInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("GetCrittersInRadiusInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetCrittersInRadiusInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetCrittersWhoSeeHex")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersWhoSeeHex");
    }

    SECTION("GetCrittersWhoSeePath")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersWhoSeePath");
    }
}

TEST_CASE("AngelScriptMapHexOperations")
{
    MAKE_SERVER;

    SECTION("MultihexItemFlagChanges")
    {
        RUN_FUNC("MapOpsTest::TestMapMultihexItemFlagChanges");
    }

    SECTION("HexMovable")
    {
        RUN_FUNC("MapOpsTest::TestMapHexMovable");
    }

    SECTION("HexMovableInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapIsHexMovableInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("HexesMovableNegativeRadiusThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapIsHexesMovableNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("HexShootableInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapIsHexShootableInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("OutsideAreaInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapIsOutsideAreaInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("BlockUnblock")
    {
        RUN_FUNC("MapOpsTest::TestMapBlockUnblock");
    }

    SECTION("MoveHexByDir")
    {
        RUN_FUNC("MapOpsTest::TestMapMoveHexByDir");
    }

    SECTION("CheckPlaceForItem")
    {
        RUN_FUNC("MapOpsTest::TestMapCheckPlaceForItem");
    }

    SECTION("CheckPlaceForItemInvalidPidThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapCheckPlaceForItemInvalidPidThrows", "Invalid item proto id arg");
    }
}

TEST_CASE("AngelScriptMapPathOperations")
{
    MAKE_SERVER;

    SECTION("PathLength")
    {
        RUN_FUNC("MapOpsTest::TestMapPathLength");
    }

    SECTION("PathLengthInvalidFromHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapPathLengthInvalidFromHexThrows", "Invalid from hex args");
    }

    SECTION("PathLengthInvalidToHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapPathLengthInvalidToHexThrows", "Invalid to hex args");
    }

    SECTION("CritterPathLengthInvalidToHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapCritterPathLengthInvalidToHexThrows", "Invalid to hex args");
    }

    SECTION("FindReachableHexes")
    {
        RUN_FUNC("MapOpsTest::TestMapFindReachableHexes");
    }

    SECTION("FindReachableHexesInvalidTargetThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapFindReachableHexesInvalidTargetThrows", "Invalid target hex arg");
    }

    SECTION("GetHexInPath")
    {
        RUN_FUNC("MapOpsTest::TestMapGetHexInPath");
    }

    SECTION("GetCrittersInPath")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCrittersInPath");
    }
}

TEST_CASE("AngelScriptMapStaticItems")
{
    MAKE_SERVER;

    SECTION("GetStaticItems")
    {
        RUN_FUNC("MapOpsTest::TestMapGetStaticItems");
    }

    SECTION("GameStaticMapQueries")
    {
        RUN_FUNC("MapOpsTest::TestGameStaticMapQueries");
    }

    SECTION("StaticItemRemoval")
    {
        RUN_FUNC("MapOpsTest::TestMapStaticItemRemoval");
    }

    SECTION("RemoveUnknownStaticItemThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapRemoveUnknownStaticItemThrows", "Static item not found on map");
    }

    SECTION("GetStaticItemsOnHexInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsOnHexInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetStaticItemsInRadiusNegativeRadiusThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("GetStaticItemsInRadiusInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsInRadiusInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetStaticItemsInRadiusByProtoInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsInRadiusByProtoInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetStaticItemsOnHexByPropertyInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsOnHexByPropertyInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("GetStaticItemsInRadiusByPropertyInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapGetStaticItemsInRadiusByPropertyInvalidHexThrows", "Invalid hex arg");
    }
}

TEST_CASE("AngelScriptMapStaticItemRemoval")
{
    MAKE_SERVER;

    ident_t visible_id {21};
    ident_t hidden_id {22};
    mpos visible_hex {12, 13};
    mpos hidden_hex {14, 15};
    hstring visible_pid = get_func("TestStaticItem");

    auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), vector<hstring> {get_func("StaticMap")});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(static_cast<bool>(map));

    SECTION("StaticItemsCarryTheirMapFileId")
    {
        auto visible_item = map->GetStaticItem(visible_id);
        REQUIRE(visible_item);
        CHECK(visible_item->GetId() == visible_id);
        CHECK(visible_item->GetProtoId() == visible_pid);
    }

    SECTION("RemovalHidesTheItemFromEveryQuery")
    {
        REQUIRE(map->GetStaticItems().size() == 2);
        REQUIRE(map->IsHexMovable(visible_hex) == false);

        CHECK(map->RemoveStaticItem(visible_id));

        CHECK(map->HasRemovedStaticItems());
        CHECK(map->GetRemovedStaticItemIds() == vector<ident_t> {visible_id});
        CHECK_FALSE(static_cast<bool>(map->GetStaticItem(visible_id)));
        CHECK(map->GetStaticItems().size() == 1);
        CHECK(map->GetStaticItems(visible_pid).empty());
        CHECK(map->GetStaticItemsOnHex(visible_hex).empty());
        CHECK_FALSE(static_cast<bool>(map->GetStaticItemOnHex(visible_hex, visible_pid)));
        CHECK(map->GetStaticItemsInRadius(visible_hex, 5, visible_pid).empty());

        // The item was the only thing blocking its hex, so passability must follow the removal
        CHECK(map->IsHexMovable(visible_hex));
        CHECK(map->IsHexShootable(visible_hex));

        // HasStaticItem asks the baked map, not the instance
        CHECK(map->HasStaticItem(visible_id));

        // Untouched neighbours keep both their identity and their blocking
        CHECK(static_cast<bool>(map->GetStaticItem(hidden_id)));
        CHECK_FALSE(map->IsHexMovable(hidden_hex));

        // Removing twice is a no-op rather than a duplicated id in the stored list
        CHECK_FALSE(map->RemoveStaticItem(visible_id));
        CHECK(map->GetRemovedStaticItemIds().size() == 1);
    }

    SECTION("RemovingEveryStaticItemLeavesAnEmptyMap")
    {
        REQUIRE(map->RemoveStaticItem(visible_id));
        REQUIRE(map->RemoveStaticItem(hidden_id));

        CHECK(map->GetStaticItems().empty());
        CHECK(map->IsHexMovable(visible_hex));
        CHECK(map->IsHexMovable(hidden_hex));
    }

    SECTION("RemovalIsOneWayForTheLifeOfTheMap")
    {
        REQUIRE(map->RemoveStaticItem(visible_id));

        // Taking an id back out of the list is refused rather than quietly failing to bring the item back:
        // clients already on the map are only ever told to drop a static item
        CHECK_THROWS_AS(map->SetRemovedStaticItemIds(vector<ident_t> {}), VerificationException);

        // The refusal precedes the store, so list and overlay both stand. Rejecting after the write would
        // persist the shrunk list and silently undo the removal on the next server start
        CHECK(map->GetRemovedStaticItemIds() == vector<ident_t> {visible_id});
        CHECK_FALSE(static_cast<bool>(map->GetStaticItem(visible_id)));
        CHECK(map->IsHexMovable(visible_hex));

        // Growing the list is the only accepted direction
        CHECK_NOTHROW(map->SetRemovedStaticItemIds(vector<ident_t> {visible_id, hidden_id}));
        CHECK(map->GetStaticItems().empty());
    }

    SECTION("RegenerateKeepsRemovals")
    {
        REQUIRE(map->RemoveStaticItem(visible_id));

        // Regeneration rebuilds map content, not the baked static layer
        server->MapMngr.RegenerateMap(map);

        CHECK(map->GetRemovedStaticItemIds() == vector<ident_t> {visible_id});
        CHECK_FALSE(static_cast<bool>(map->GetStaticItem(visible_id)));
        CHECK(map->GetStaticItems().size() == 1);
    }

    SECTION("RemovalIsPerMapInstance")
    {
        auto other_loc = server->MapMngr.CreateLocation(get_func("TestLocation"), vector<hstring> {get_func("StaticMap")});
        auto other_map = other_loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(other_map));

        REQUIRE(map->RemoveStaticItem(visible_id));

        // The StaticMap is shared by every instance of the proto, so a removal must never reach it
        CHECK(static_cast<bool>(other_map->GetStaticItem(visible_id)));
        CHECK(other_map->GetStaticItems().size() == 2);
        CHECK_FALSE(other_map->IsHexMovable(visible_hex));

        auto static_map = server->MapMngr.GetStaticMap(map->GetProtoMap());
        CHECK(static_map->GetStaticItems().size() == 2);
        CHECK(static_map->GetField(visible_hex).MoveBlocked);

        server->MapMngr.DestroyLocation(other_loc);
    }

    SECTION("RebuiltFieldKeepsTheBakedScrollBlock")
    {
        // A scroll block is baked into the field with no owning item. Simulate one on the item hex the way
        // the map loader writes it, so removing the item must not open the hex
        auto static_map = server->MapMngr.GetStaticMap(map->GetProtoMap());
        static_map->MarkScrollBlocked(visible_hex);

        REQUIRE(map->RemoveStaticItem(visible_id));

        CHECK(map->GetStaticItemsOnHex(visible_hex).empty());
        CHECK(map->IsHexShootable(visible_hex));
        CHECK_FALSE(map->IsHexMovable(visible_hex));
    }

    server->MapMngr.DestroyLocation(loc);
}

TEST_CASE("AngelScriptMapLocationRelationship")
{
    MAKE_SERVER;

    SECTION("LocationRelationship")
    {
        RUN_FUNC("MapOpsTest::TestMapLocationRelationship");
    }

    SECTION("Regenerate")
    {
        RUN_FUNC("MapOpsTest::TestMapRegenerate");
    }

    SECTION("VerifyTrigger")
    {
        RUN_FUNC("MapOpsTest::TestMapVerifyTrigger");
    }

    SECTION("LocationCreateWithMaps")
    {
        RUN_FUNC("MapOpsTest::TestLocationCreateWithMaps");
    }

    SECTION("LocationMethodBindings")
    {
        RUN_FUNC("MapOpsTest::TestLocationMethodBindings");
    }

    SECTION("GameLocationAndMapOverloads")
    {
        RUN_FUNC("MapOpsTest::TestGameLocationAndMapOverloads");
    }

    SECTION("CreateLocationInvalidProtoPropsThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestCreateLocationInvalidProtoPropsThrows", "Invalid location proto id arg");
    }

    SECTION("CreateLocationInvalidProtoMapPropsThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestCreateLocationInvalidProtoMapPropsThrows", "Invalid location proto id arg");
    }

    SECTION("CreateLocationMissingInitScriptThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestCreateLocationMissingInitScriptThrows", "Init function not found or has a mismatched signature");
    }

    SECTION("CreateLocationMismatchedInitScriptThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestCreateLocationMismatchedInitScriptThrows", "Init function not found or has a mismatched signature");
    }

    SECTION("GameGetLocations")
    {
        RUN_FUNC("MapOpsTest::TestGameGetLocations");
    }
}

TEST_CASE("AngelScriptMapProtoOverloads")
{
    MAKE_SERVER;

    SECTION("AddItemByProto")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemByProto");
    }

    SECTION("AddItemByProtoInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemByProtoInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddCritterByProto")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterByProto");
    }
}

TEST_CASE("AngelScriptMapPropertyFilterOverloads")
{
    MAKE_SERVER;

    SECTION("GetItemByProperty")
    {
        RUN_FUNC("MapOpsTest::TestMapGetItemByProperty");
    }

    SECTION("GetCritterByProperty")
    {
        RUN_FUNC("MapOpsTest::TestMapGetCritterByProperty");
    }

    SECTION("GetStaticItemOverloads")
    {
        RUN_FUNC("MapOpsTest::TestMapGetStaticItemOverloads");
    }
}

TEST_CASE("AngelScriptMapSetupScript")
{
    MAKE_SERVER;

    SECTION("SetupScriptEx")
    {
        RUN_FUNC("MapOpsTest::TestMapSetupScriptEx");
    }

    SECTION("SetupScript")
    {
        RUN_FUNC("MapOpsTest::TestMapSetupScript");
    }

    SECTION("SetupScriptExMissingThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapSetupScriptExMissingThrows");
    }

    SECTION("SetupScriptDelegateThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapSetupScriptDelegateThrows");
    }
}

TEST_CASE("AngelScriptMapAddWithProperties")
{
    MAKE_SERVER;

    SECTION("AddItemWithProperties")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemWithProperties");
    }

    SECTION("AddItemWithPropertiesInvalidProtoThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemWithPropertiesInvalidProtoThrows", "Invalid item proto id arg");
    }

    SECTION("AddItemWithPropertiesInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemWithPropertiesInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddItemWithProtoPropertiesInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddItemWithProtoPropertiesInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddCritterWithProperties")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterWithProperties");
    }

    SECTION("AddCritterWithIntPropertiesInvalidProtoThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterWithIntPropertiesInvalidProtoThrows", "Invalid critter proto id arg");
    }

    SECTION("AddCritterWithAnyPropertiesInvalidProtoThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterWithAnyPropertiesInvalidProtoThrows", "Invalid critter proto id arg");
    }

    SECTION("AddCritterWithIntPropertiesInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterWithIntPropertiesInvalidHexThrows", "Invalid hex arg");
    }

    SECTION("AddCritterWithAnyPropertiesInvalidHexThrows")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestMapAddCritterWithAnyPropertiesInvalidHexThrows", "Invalid hex arg");
    }
}

TEST_CASE("AngelScriptMapReentrantEvents")
{
    MAKE_SERVER;

    SECTION("MapCritterOutEventRunsBeforeDetach")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterOutEventRunsBeforeDetach");
    }

    SECTION("MapCritterOutEventMayDestroyCritter")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterOutEventMayDestroyCritter");
    }

    SECTION("MapCritterOutEventBlocksNestedTransfer")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterOutEventBlocksNestedTransfer");
    }

    SECTION("CritterDisappearedEventRunsBeforeDetach")
    {
        RUN_FUNC("MapOpsTest::TestCritterDisappearedEventRunsBeforeDetach");
    }

    SECTION("CritterDisappearedEventMayDestroyCritter")
    {
        RUN_FUNC("MapOpsTest::TestCritterDisappearedEventMayDestroyCritter");
    }

    SECTION("MapCritterInEventMayDestroyEnteringCritter")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterInEventMayDestroyEnteringCritter");
    }

    SECTION("MapCritterInEventMayDestroyBeforeInitialInfo")
    {
        RUN_FUNC("MapOpsTest::TestMapCritterInEventMayDestroyBeforeInitialInfo");
    }

    SECTION("MapAddCritterEventMayDestroyCritterThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterEventMayDestroyCritterThrows");
    }

    SECTION("MapAddCritterEventMayDestroyMapThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterEventMayDestroyMapThrows");
    }

    SECTION("MapAddCritterEventMayMoveCritterAwayThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterEventMayMoveCritterAwayThrows");
    }

    SECTION("MapAddCritterInitEventMayDestroyCritterThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterInitEventMayDestroyCritterThrows");
    }

    SECTION("MapAddCritterInitEventMayDestroyMapThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterInitEventMayDestroyMapThrows");
    }

    SECTION("MapAddCritterInitEventMayMoveCritterAwayThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddCritterInitEventMayMoveCritterAwayThrows");
    }

    SECTION("GlobalMapCritterOutEventKeepsTrip")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapCritterOutEventKeepsTrip");
    }

    SECTION("GlobalMapCritterOutEventMayDestroyCritter")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapCritterOutEventMayDestroyCritter");
    }

    SECTION("GlobalMapCritterInEventMayDestroyBeforeInitialInfo")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapCritterInEventMayDestroyBeforeInitialInfo");
    }

    SECTION("GlobalMapAddCritterEventMayDestroyCritterThrows")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapAddCritterEventMayDestroyCritterThrows");
    }

    SECTION("GlobalMapLoadCritterEventMayDestroyBeforeCritterLoad")
    {
        RUN_FUNC("MapOpsTest::TestGlobalMapLoadCritterEventMayDestroyBeforeCritterLoad");
    }

    SECTION("AttachedCritterTransferEventMayDestroyLeaderBeforeFinalTransfer")
    {
        RUN_FUNC("MapOpsTest::TestAttachedCritterTransferEventMayDestroyLeaderBeforeFinalTransfer");
    }

    SECTION("CritterAppearedEventMayDestroyTarget")
    {
        RUN_FUNC("MapOpsTest::TestCritterAppearedEventMayDestroyTarget");
    }

    SECTION("CritterAppearedEventStopsDistanceGroupsAfterTransfer")
    {
        RUN_FUNC("MapOpsTest::TestCritterAppearedEventStopsDistanceGroupsAfterTransfer");
    }

    SECTION("CritterAppearedDistEventStopsNextDistanceGroupsAfterTransfer")
    {
        RUN_FUNC("MapOpsTest::TestCritterAppearedDistEventStopsNextDistanceGroupsAfterTransfer");
    }

    SECTION("CritterDisappearedEventStopsDistanceGroupsAfterTransfer")
    {
        RUN_FUNC("MapOpsTest::TestCritterDisappearedEventStopsDistanceGroupsAfterTransfer");
    }

    SECTION("ItemOnMapAppearedEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapAppearedEventMayDestroyItem");
    }

    SECTION("ItemOnMapAppearedEventMayMoveItemAway")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapAppearedEventMayMoveItemAway");
    }

    SECTION("MapAddItemEventMayDestroyItemThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemEventMayDestroyItemThrows");
    }

    SECTION("MapAddItemEventMayDestroyMapThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemEventMayDestroyMapThrows");
    }

    SECTION("MapAddItemEventMayMoveItemAwayThrows")
    {
        RUN_FUNC("MapOpsTest::TestMapAddItemEventMayMoveItemAwayThrows");
    }

    SECTION("ItemOnMapDisappearedEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapDisappearedEventMayDestroyItem");
    }

    SECTION("ItemOnMapChangedEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapChangedEventMayDestroyItem");
    }

    SECTION("ItemOnMapChangedEventMayMoveItemAwayBeforeNextObserver")
    {
        RUN_FUNC("MapOpsTest::TestItemOnMapChangedEventMayMoveItemAwayBeforeNextObserver");
    }

    SECTION("CritterItemMovedEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestCritterItemMovedEventMayDestroyItem");
    }

    SECTION("CritterItemMovedAddEventMayMoveItemAway")
    {
        RUN_FUNC("MapOpsTest::TestCritterItemMovedAddEventMayMoveItemAway");
    }

    SECTION("CritterItemMovedSwapEventMayMoveOriginalItem")
    {
        RUN_FUNC("MapOpsTest::TestCritterItemMovedSwapEventMayMoveOriginalItem");
    }

    SECTION("CritterItemMovedSwapEventMayDestroyOriginalItem")
    {
        RUN_FUNC("MapOpsTest::TestCritterItemMovedSwapEventMayDestroyOriginalItem");
    }

    SECTION("CritterWalkEventMayDestroyCritter")
    {
        RUN_FUNC("MapOpsTest::TestCritterWalkEventMayDestroyCritter");
    }

    SECTION("CritterWalkEventMayDestroyItem")
    {
        RUN_FUNC("MapOpsTest::TestCritterWalkEventMayDestroyItem");
    }

    SECTION("CritterWalkEventMayTransferCritterBeforeNextTrigger")
    {
        RUN_FUNC("MapOpsTest::TestCritterWalkEventMayTransferCritterBeforeNextTrigger");
    }

    SECTION("CritterWalkEventMayDestroyMapBeforeNextTrigger")
    {
        RUN_FUNC("MapOpsTest::TestCritterWalkEventMayDestroyMapBeforeNextTrigger");
    }

    SECTION("MapFinishEventCannotDestroyOwningLocation")
    {
        RUN_FUNC("MapOpsTest::TestMapFinishEventCannotDestroyOwningLocation");
    }

    SECTION("MapRemovedEventCannotDestroyOwningLocation")
    {
        RUN_FUNC("MapOpsTest::TestMapRemovedEventCannotDestroyOwningLocation");
    }

    SECTION("MapInitEventMayDestroyMap")
    {
        RUN_FUNC("MapOpsTest::TestMapInitEventMayDestroyMap");
    }

    SECTION("MapAddedEventMayDestroyLocation")
    {
        RUN_FUNC("MapOpsTest::TestMapAddedEventMayDestroyLocation");
    }

    SECTION("MapAddedEventMayDestroyMap")
    {
        RUN_FUNC("MapOpsTest::TestMapAddedEventMayDestroyMap");
    }
}

TEST_CASE("AngelScriptScriptArgValidation")
{
    MAKE_SERVER;

    SECTION("ItemsInRadiusNegativeRadius")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgItemsInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("CrittersInRadiusNegativeRadius")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgCrittersInRadiusNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("IsHexesMovableNegativeRadius")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgIsHexesMovableNegativeRadiusThrows", "Radius arg must not be negative");
    }

    SECTION("TransferToMapOutOfBoundsHex")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgTransferToMapOutOfBoundsHexThrows", "Invalid target hex arg");
    }

    SECTION("MoveToHexNegativeSpeed")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgMoveToHexNegativeSpeedThrows", "Speed arg out of range");
    }

    SECTION("ChangeMovingSpeedNegative")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgChangeMovingSpeedNegativeThrows", "Speed arg out of range");
    }

    SECTION("TraceHexLineOffsetOutOfRange")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgTraceHexLineOffsetOutOfRangeThrows", "Hex offset arg out of range");
    }

    SECTION("DestroyEntityPlayerCritter")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgDestroyEntityPlayerCritterThrows", "Cannot destroy a player-controlled critter");
    }

    SECTION("StringRawResizeNegative")
    {
        RUN_FUNC_THROWS("MapOpsTest::TestArgStringRawResizeNegativeThrows", "String resize length must not be negative");
    }
}

FO_END_NAMESPACE

#endif

