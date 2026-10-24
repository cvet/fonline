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

#include "Test_LocationAndEntityMgmtHelpers.h"

FO_BEGIN_NAMESPACE

using namespace LocationAndEntityMgmtTests;

TEST_CASE("LoadUnloadCritter")
{
    MAKE_LEM_SERVER();

    SECTION("UnloadAndLoadClearsMapId")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), true);
        vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));
        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        REQUIRE(cr->GetMapId() == map->GetId());

        server->EntityMngr.MakePersistent(cr, true, true);
        ident_t id = cr->GetId();
        server->UnloadCritter(cr);
        CHECK_FALSE(server->EntityMngr.GetCritter(id));

        auto loaded = server->LoadCritter(id, false);
        CHECK(loaded->GetId() == id);
        CHECK(loaded->GetMapId() == ident_t {});
        server->MapMngr.DestroyLocation(loc);
        server->CrMngr.DestroyCritter(loaded);
    }

    SECTION("DirectLoadReportsAndPrunesMissingInventoryItem")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), true);

        auto item = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(get_func("TestItem"), nullptr), true);

        server->EntityMngr.MakePersistent(cr, true, true);

        ident_t cr_id = cr->GetId();
        ident_t item_id = item->GetId();

        server->DbStorage.Delete(get_func("Items"), item_id);
        server->UnloadCritter(cr);

        CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
        CHECK_FALSE(server->EntityMngr.GetItem(item_id));

        bool is_error = false;
        auto loaded_holder = server->EntityMngr.LoadCritter(cr_id, false, nullptr, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK(is_error);
        CHECK(loaded->GetItemIds().empty());
        CHECK_FALSE(loaded->HasItems());
        CHECK(server->EntityMngr.GetCritter(cr_id) == loaded);

        loaded->MarkAsDestroying();
        server->UnloadCritterInnerEntities(loaded);
        loaded->MarkAsDestroyed();
        server->EntityMngr.UnregisterCritter(loaded, false);

        CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
    }

    SECTION("DirectCustomInnerEntityLifecycle")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        server->EntityMngr.MakePersistent(cr, true, true);

        hstring custom_type = get_func("CoverageTarget");
        hstring custom_entry = get_func("CoverageItems");
        auto custom = server->EntityMngr.CreateCustomInnerEntity(cr, custom_entry, {});
        auto custom_holder = custom.hold_ref();

        ident_t custom_id = custom->GetId();
        REQUIRE(custom_id);
        CHECK(custom->GetTypeName() == custom_type);
        CHECK(custom->GetCustomHolderId() == cr->GetId());
        CHECK(custom->GetCustomHolderEntry() == custom_entry);
        CHECK(custom->IsPersistent());

        auto parent = custom->GetParent();
        REQUIRE(parent);
        CHECK(parent == cr);

        auto holder_prop = server->GetEntityHolderIdsProp(cr, custom_entry);
        CHECK(cr->GetProperties()->GetValueFast<vector<ident_t>>(holder_prop.get()) == vector<ident_t> {custom_id});

        auto inner_entities = cr->GetInnerEntities(custom_entry);
        REQUIRE(inner_entities);
        REQUIRE(inner_entities->size() == 1);
        CHECK(inner_entities->front() == custom);
        CHECK(server->EntityMngr.GetCustomEntity(custom_type, custom_id) == custom);

        server->EntityMngr.DestroyInnerEntities(cr);

        CHECK_FALSE(cr->HasInnerEntities());
        CHECK(cr->GetProperties()->GetValueFast<vector<ident_t>>(holder_prop.get()).empty());
        CHECK(custom->IsDestroyed());
        CHECK_FALSE(server->EntityMngr.GetCustomEntity(custom_type, custom_id));

        server->EntityMngr.DestroyEntity(cr);
    }

    SECTION("CustomInnerEntityCreationRetainsCoveredHolderOwnLock")
    {
        vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);
        auto map = loc->GetMaps().front();
        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        server->MapMngr.AddCritterToMap(cr, map, mpos {20, 21}, mdir {0}, ident_t {});

        nptr<SyncContext> sync_ctx = SyncContext::GetCurrentOnThisThread();
        REQUIRE(sync_ctx);
        sync_ctx->SyncEntity(map);
        REQUIRE(map->GetEntityLock()->IsLockedByCurrentThread());
        CHECK_FALSE(cr->GetEntityLock()->IsLockedByCurrentThread());

        hstring custom_entry = get_func("CoverageItems");
        auto custom = server->EntityMngr.CreateCustomInnerEntity(cr, custom_entry, {});

        CHECK(cr->GetEntityLock()->IsLockedByCurrentThread());
        CHECK(custom->GetParent() == cr);
        CHECK(custom->GetCustomHolderEntry() == custom_entry);

        sync_ctx->SyncEntity(loc);
        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("DirectLoadRestoresContainerItemTree")
    {
        auto container = server->ItemMngr.CreateItem(get_func("TestItem"), nullptr);
        auto container_holder = container.hold_ref();

        auto inner = container->AddItemToContainer(server->ItemMngr.CreateItem(get_func("TestItem"), nullptr), {});
        auto inner_holder = inner.hold_ref();

        server->EntityMngr.MakePersistent(container, true, true);
        server->DbStorage.WaitCommitChanges();

        ident_t container_id = container->GetId();
        ident_t inner_id = inner->GetId();

        REQUIRE(container_id);
        REQUIRE(inner_id);
        CHECK(container->GetInnerItemIds() == vector<ident_t> {inner_id});
        CHECK(inner->GetOwnership() == ItemOwnership::ItemContainer);
        CHECK(inner->GetContainerId() == container_id);

        (void)container->TakeAllInnerItems();
        inner->MarkAsDestroyed();
        server->EntityMngr.UnregisterItem(inner, false);
        container->MarkAsDestroyed();
        server->EntityMngr.UnregisterItem(container, false);

        CHECK_FALSE(server->EntityMngr.GetItem(container_id));
        CHECK_FALSE(server->EntityMngr.GetItem(inner_id));

        bool is_error = false;
        refcount_nptr<Item> loaded_holder = server->EntityMngr.LoadItem(container_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK_FALSE(is_error);
        CHECK(loaded->GetId() == container_id);
        CHECK(loaded->GetProtoId() == get_func("TestItem"));
        CHECK(loaded->GetOwnership() == ItemOwnership::Nowhere);
        CHECK(loaded->GetInnerItemIds() == vector<ident_t> {inner_id});
        CHECK(server->EntityMngr.GetItem(container_id) == loaded);

        auto loaded_inner_items = loaded->GetAllInnerItems();
        REQUIRE(loaded_inner_items.size() == 1);

        auto loaded_inner = loaded_inner_items.front();
        CHECK(loaded_inner->GetId() == inner_id);
        CHECK(loaded_inner->GetProtoId() == get_func("TestItem"));
        CHECK(loaded_inner->GetOwnership() == ItemOwnership::ItemContainer);
        CHECK(loaded_inner->GetContainerId() == container_id);
        CHECK(loaded_inner->GetParent() == loaded);
        CHECK(server->EntityMngr.GetItem(inner_id) == loaded_inner);

        server->ItemMngr.DestroyItem(loaded);
        CHECK_FALSE(server->EntityMngr.GetItem(container_id));
        CHECK_FALSE(server->EntityMngr.GetItem(inner_id));
    }

    SECTION("ContainerRejectsItsOwnSubtree")
    {
        auto outer = server->ItemMngr.CreateItem(get_func("TestItem"), nullptr);
        auto outer_holder = outer.hold_ref();

        auto inner = outer->AddItemToContainer(server->ItemMngr.CreateItem(get_func("TestItem"), nullptr), {});
        auto inner_holder = inner.hold_ref();

        CHECK_THROWS(outer->AddItemToContainer(outer, {}));
        CHECK_THROWS(inner->AddItemToContainer(outer, {}));

        CHECK(inner->GetOwnership() == ItemOwnership::ItemContainer);
        CHECK(inner->GetContainerId() == outer->GetId());
        CHECK(outer->GetOwnership() == ItemOwnership::Nowhere);

        server->ItemMngr.DestroyItem(outer);
    }

    SECTION("DirectLoadMapRestoresCrittersAndItems")
    {
        vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        server->MapMngr.AddCritterToMap(cr, map, mpos {20, 21}, mdir {0}, ident_t {});

        auto item = server->ItemMngr.CreateItemOnHex(map, mpos {22, 23}, get_func("TestItem"), nullptr);

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        ident_t loc_id = loc->GetId();
        ident_t map_id = map->GetId();
        ident_t cr_id = cr->GetId();
        ident_t item_id = item->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        REQUIRE(cr_id);
        REQUIRE(item_id);
        CHECK(map->GetCritterIds() == vector<ident_t> {cr_id});
        CHECK(map->GetItemIds() == vector<ident_t> {item_id});

        hstring locations_collection = get_func("Locations");
        hstring maps_collection = get_func("Maps");
        hstring critters_collection = get_func("Critters");
        hstring items_collection = get_func("Items");

        auto loc_doc = server->DbStorage.Get(locations_collection, loc_id);
        auto map_doc = server->DbStorage.Get(maps_collection, map_id);
        auto cr_doc = server->DbStorage.Get(critters_collection, cr_id);
        auto item_doc = server->DbStorage.Get(items_collection, item_id);

        REQUIRE_FALSE(loc_doc.Empty());
        REQUIRE_FALSE(map_doc.Empty());
        REQUIRE_FALSE(cr_doc.Empty());
        REQUIRE_FALSE(item_doc.Empty());

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK_FALSE(server->EntityMngr.GetLocation(loc_id));
        CHECK_FALSE(server->EntityMngr.GetMap(map_id));
        CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
        CHECK_FALSE(server->EntityMngr.GetItem(item_id));

        server->DbStorage.Insert(locations_collection, loc_id, loc_doc);
        server->DbStorage.Insert(maps_collection, map_id, map_doc);
        server->DbStorage.Insert(critters_collection, cr_id, cr_doc);
        server->DbStorage.Insert(items_collection, item_id, item_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Map> loaded_holder = server->EntityMngr.LoadMap(map_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK_FALSE(is_error);
        CHECK(loaded->GetId() == map_id);
        CHECK(loaded->GetLocId() == loc_id);
        CHECK(loaded->GetCritterIds() == vector<ident_t> {cr_id});
        CHECK(loaded->GetItemIds() == vector<ident_t> {item_id});
        CHECK(server->EntityMngr.GetMap(map_id) == loaded);

        auto loaded_cr = server->EntityMngr.GetCritter(cr_id);
        CHECK(loaded_cr->GetMapId() == map_id);
        CHECK(loaded_cr->GetHex() == mpos {20, 21});
        CHECK(loaded->GetCritter(cr_id) == loaded_cr);

        auto loaded_item = server->EntityMngr.GetItem(item_id);
        CHECK(loaded_item->GetOwnership() == ItemOwnership::MapHex);
        CHECK(loaded_item->GetMapId() == map_id);
        CHECK(loaded_item->GetHex() == mpos {22, 23});
        CHECK(loaded->GetItem(item_id) == loaded_item);

        server->CrMngr.DestroyCritter(loaded_cr.get());
        loaded_cr.reset();

        server->ItemMngr.DestroyItem(loaded_item.get());
        loaded_item.reset();

        CHECK(loaded->GetCritters().empty());
        CHECK(loaded->GetItems().empty());

        loaded->MarkAsDestroyed();
        server->EntityMngr.UnregisterMap(loaded);
    }

    SECTION("DirectLoadMapClampsInvalidCritterAndItemHex")
    {
        vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        server->MapMngr.AddCritterToMap(cr, map, mpos {20, 21}, mdir {0}, ident_t {});

        auto item = server->ItemMngr.CreateItemOnHex(map, mpos {22, 23}, get_func("TestItem"), nullptr);

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        ident_t loc_id = loc->GetId();
        ident_t map_id = map->GetId();
        ident_t cr_id = cr->GetId();
        ident_t item_id = item->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        REQUIRE(cr_id);
        REQUIRE(item_id);

        hstring maps_collection = get_func("Maps");
        hstring critters_collection = get_func("Critters");
        hstring items_collection = get_func("Items");

        auto map_doc = server->DbStorage.Get(maps_collection, map_id);
        auto cr_doc = server->DbStorage.Get(critters_collection, cr_id);
        auto item_doc = server->DbStorage.Get(items_collection, item_id);

        REQUIRE_FALSE(map_doc.Empty());
        REQUIRE_FALSE(cr_doc.Empty());
        REQUIRE_FALSE(item_doc.Empty());

        cr_doc.Assign("Hex", AnyData::Value {string {"-5 300"}});
        item_doc.Assign("Hex", AnyData::Value {string {"300 -5"}});

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK_FALSE(server->EntityMngr.GetLocation(loc_id));
        CHECK_FALSE(server->EntityMngr.GetMap(map_id));
        CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
        CHECK_FALSE(server->EntityMngr.GetItem(item_id));

        server->DbStorage.Insert(maps_collection, map_id, map_doc);
        server->DbStorage.Insert(critters_collection, cr_id, cr_doc);
        server->DbStorage.Insert(items_collection, item_id, item_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Map> loaded_holder = server->EntityMngr.LoadMap(map_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK_FALSE(is_error);
        CHECK(loaded->GetId() == map_id);
        CHECK(loaded->GetCritterIds() == vector<ident_t> {cr_id});
        CHECK(loaded->GetItemIds() == vector<ident_t> {item_id});
        CHECK(server->EntityMngr.GetMap(map_id) == loaded);

        auto loaded_cr = server->EntityMngr.GetCritter(cr_id);
        CHECK(loaded_cr->GetMapId() == map_id);
        CHECK(loaded_cr->GetHex() == mpos {0, 199});
        CHECK(loaded->GetCritter(cr_id) == loaded_cr);

        auto loaded_item = server->EntityMngr.GetItem(item_id);
        CHECK(loaded_item->GetOwnership() == ItemOwnership::MapHex);
        CHECK(loaded_item->GetMapId() == map_id);
        CHECK(loaded_item->GetHex() == mpos {199, 0});
        CHECK(loaded->GetItem(item_id) == loaded_item);

        server->CrMngr.DestroyCritter(loaded_cr.get());
        loaded_cr.reset();

        server->ItemMngr.DestroyItem(loaded_item.get());
        loaded_item.reset();

        CHECK(loaded->GetCritters().empty());
        CHECK(loaded->GetItems().empty());

        loaded->MarkAsDestroyed();
        server->EntityMngr.UnregisterMap(loaded);
    }

    SECTION("DirectLoadMapPrunesMissingCritterAndItemRefs")
    {
        vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        server->MapMngr.AddCritterToMap(cr, map, mpos {20, 21}, mdir {0}, ident_t {});

        auto item = server->ItemMngr.CreateItemOnHex(map, mpos {22, 23}, get_func("TestItem"), nullptr);

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        ident_t loc_id = loc->GetId();
        ident_t map_id = map->GetId();
        ident_t cr_id = cr->GetId();
        ident_t item_id = item->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        REQUIRE(cr_id);
        REQUIRE(item_id);
        CHECK(map->GetCritterIds() == vector<ident_t> {cr_id});
        CHECK(map->GetItemIds() == vector<ident_t> {item_id});

        hstring maps_collection = get_func("Maps");
        auto map_doc = server->DbStorage.Get(maps_collection, map_id);
        REQUIRE_FALSE(map_doc.Empty());

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK_FALSE(server->EntityMngr.GetLocation(loc_id));
        CHECK_FALSE(server->EntityMngr.GetMap(map_id));
        CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
        CHECK_FALSE(server->EntityMngr.GetItem(item_id));

        server->DbStorage.Insert(maps_collection, map_id, map_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Map> loaded_holder = server->EntityMngr.LoadMap(map_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK(is_error);
        CHECK(loaded->GetId() == map_id);
        CHECK(loaded->GetLocId() == loc_id);
        CHECK(loaded->GetCritterIds().empty());
        CHECK(loaded->GetItemIds().empty());
        CHECK(loaded->GetCritters().empty());
        CHECK(loaded->GetItems().empty());
        CHECK(server->EntityMngr.GetMap(map_id) == loaded);
        CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
        CHECK_FALSE(server->EntityMngr.GetItem(item_id));

        loaded->MarkAsDestroyed();
        server->EntityMngr.UnregisterMap(loaded);
    }

    SECTION("DirectLoadLocationRestoresMapRefs")
    {
        vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        ident_t loc_id = loc->GetId();
        ident_t map_id = map->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        CHECK(loc->GetMapIds() == vector<ident_t> {map_id});
        CHECK(map->GetLocId() == loc_id);
        CHECK(map->GetLocMapIndex() == 0);
        CHECK(map->GetLocation() == loc);

        hstring locations_collection = get_func("Locations");
        hstring maps_collection = get_func("Maps");

        auto loc_doc = server->DbStorage.Get(locations_collection, loc_id);
        auto map_doc = server->DbStorage.Get(maps_collection, map_id);

        REQUIRE_FALSE(loc_doc.Empty());
        REQUIRE_FALSE(map_doc.Empty());

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK_FALSE(server->EntityMngr.GetLocation(loc_id));
        CHECK_FALSE(server->EntityMngr.GetMap(map_id));

        server->DbStorage.Insert(locations_collection, loc_id, loc_doc);
        server->DbStorage.Insert(maps_collection, map_id, map_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Location> loaded_holder = server->EntityMngr.LoadLocation(loc_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK_FALSE(is_error);
        CHECK(loaded->GetId() == loc_id);
        CHECK(loaded->GetProtoId() == get_func("TestLocation"));
        CHECK(loaded->GetMapIds() == vector<ident_t> {map_id});
        CHECK(server->EntityMngr.GetLocation(loc_id) == loaded);

        auto loaded_maps = loaded->GetMaps();
        REQUIRE(loaded_maps.size() == 1);

        auto loaded_map = loaded_maps.front();
        CHECK(loaded_map->GetId() == map_id);
        CHECK(loaded_map->GetProtoId() == get_func("TestMap"));
        CHECK(loaded_map->GetLocId() == loc_id);
        CHECK(loaded_map->GetLocMapIndex() == 0);
        CHECK(loaded_map->GetLocation() == loaded);
        CHECK(server->EntityMngr.GetMap(map_id) == loaded_map);

        server->MapMngr.DestroyLocation(loaded);
        loaded_holder.reset();
        server->DbStorage.WaitCommitChanges();

        CHECK_FALSE(server->EntityMngr.GetLocation(loc_id));
        CHECK_FALSE(server->EntityMngr.GetMap(map_id));
    }

    SECTION("DirectLoadLocationPrunesMissingMapRefs")
    {
        vector<hstring> map_pids {get_func("TestMap")};
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"), map_pids);

        auto loc_maps = loc->GetMaps();
        REQUIRE(loc_maps.size() == 1);

        auto map = loc_maps.front();

        server->EntityMngr.MakePersistent(loc, true, true);
        server->DbStorage.WaitCommitChanges();

        ident_t loc_id = loc->GetId();
        ident_t map_id = map->GetId();

        REQUIRE(loc_id);
        REQUIRE(map_id);
        CHECK(loc->GetMapIds() == vector<ident_t> {map_id});

        hstring locations_collection = get_func("Locations");
        auto loc_doc = server->DbStorage.Get(locations_collection, loc_id);
        REQUIRE_FALSE(loc_doc.Empty());

        server->MapMngr.DestroyLocation(loc);
        server->DbStorage.WaitCommitChanges();

        CHECK_FALSE(server->EntityMngr.GetLocation(loc_id));
        CHECK_FALSE(server->EntityMngr.GetMap(map_id));

        server->DbStorage.Insert(locations_collection, loc_id, loc_doc);
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        refcount_nptr<Location> loaded_holder = server->EntityMngr.LoadLocation(loc_id, is_error);
        REQUIRE(loaded_holder);
        auto loaded = loaded_holder;
        FO_VERIFY_AND_THROW(loaded, "Loaded entity is null");

        CHECK(is_error);
        CHECK(loaded->GetId() == loc_id);
        CHECK(loaded->GetMapIds().empty());
        CHECK_FALSE(loaded->HasMaps());
        CHECK(server->EntityMngr.GetLocation(loc_id) == loaded);
        CHECK_FALSE(server->EntityMngr.GetMap(map_id));

        server->MapMngr.DestroyLocation(loaded);
        loaded_holder.reset();
        server->DbStorage.WaitCommitChanges();

        CHECK_FALSE(server->EntityMngr.GetLocation(loc_id));
    }
}

#if FO_DATABASE_JSON
TEST_CASE("PersistedCustomInnerEntitiesAreReloadedFromDisk")
{
    // The inner-entity read path runs nowhere else in the suite, because every other test here keeps the world in
    // memory for one engine's lifetime
    auto storage_dir = std::filesystem::temp_directory_path() / std::format("fo_engine_custom_entity_reload_test_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code remove_error;
    std::filesystem::remove_all(storage_dir, remove_error);
    std::filesystem::create_directories(storage_dir);

    auto cleanup_storage = scope_exit([storage_dir]() noexcept {
        safe_call([storage_dir] {
            std::error_code ignored;
            std::filesystem::remove_all(storage_dir, ignored);
        });
    });

    string storage_option = strex("JSON {}", storage_dir.generic_string()).str();

    ident_t location_id;
    ident_t custom_id;

    {
        auto settings = MakeSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, storage_option);

        refcount_ptr<ServerEngine> server = MakeServerEngine(settings);
        string startup_error = WaitForStart(server.as_ptr());
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        {
            REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

            auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

            auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("TestLocation"));
            server->EntityMngr.MakePersistent(location, true, true);
            location_id = location->GetId();

            auto custom = server->EntityMngr.CreateCustomInnerEntity(location, server->Hashes.to_hashed_string("CoverageLocItems"), {});
            REQUIRE(custom->IsPersistent());
            custom_id = custom->GetId();
        }

        server->Shutdown();
    }

    {
        auto settings = MakeSettings();
        BakerTests::OverrideSetting(settings.Server.DbStorage, storage_option);

        refcount_ptr<ServerEngine> server = MakeServerEngine(settings);

        auto shutdown = scope_exit([&server]() noexcept {
            safe_call([&server] {
                if (server->IsStarted()) {
                    server->Shutdown();
                }
            });
        });

        string startup_error = WaitForStart(server.as_ptr());
        INFO(startup_error);
        REQUIRE(startup_error.empty());

        REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

        auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

        hstring custom_type = server->Hashes.to_hashed_string("CoverageTarget");

        // Only registry lookups are asserted, because reading properties needs the entity covered and a test
        // holding just the engine lock cannot take a pre-existing one into its context
        CHECK(server->EntityMngr.GetLocation(location_id));
        CHECK(server->EntityMngr.GetCustomEntity(custom_type, custom_id));
        CHECK_FALSE(server->EntityMngr.GetCustomEntity(custom_type, ident_t {}));
    }
}
#endif

TEST_CASE("LocationCppApiTests")
{
    MAKE_LEM_SERVER();

    SECTION("CreateMultipleLocations")
    {
        size_t initial_count = server->EntityMngr.GetLocationsCount();

        auto loc1 = server->MapMngr.CreateLocation(get_func("TestLocation"));
        auto loc2 = server->MapMngr.CreateLocation(get_func("TestLocation"));
        auto loc3 = server->MapMngr.CreateLocation(get_func("TestLocation"));

        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 3);
        CHECK(loc1->GetId() != loc2->GetId());
        CHECK(loc2->GetId() != loc3->GetId());
        CHECK(loc1->GetMaps().empty());

        server->MapMngr.DestroyLocation(loc3);
        server->MapMngr.DestroyLocation(loc2);
        server->MapMngr.DestroyLocation(loc1);

        CHECK(server->EntityMngr.GetLocationsCount() == initial_count);
    }

    SECTION("LocationById")
    {
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"));

        ident_t loc_id = loc->GetId();
        auto found_loc = server->EntityMngr.GetLocation(loc_id);
        REQUIRE(found_loc);
        CHECK(found_loc == loc);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("LocationProtoId")
    {
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"));

        CHECK(loc->GetProtoId() == get_func("TestLocation"));

        server->MapMngr.DestroyLocation(loc);
    }
}

TEST_CASE("CritterCppApiAdvanced")
{
    MAKE_LEM_SERVER();

    SECTION("CreateMultipleCritters")
    {
        size_t initial_count = server->EntityMngr.GetCrittersCount();

        auto cr1 = server->CreateCritter(get_func("TestCritter"), false);
        auto cr2 = server->CreateCritter(get_func("TestCritter"), false);
        auto cr3 = server->CreateCritter(get_func("TestCritter"), false);

        CHECK(server->EntityMngr.GetCrittersCount() >= initial_count + 3);
        CHECK(cr1->GetId() != cr2->GetId());
        CHECK(cr2->GetId() != cr3->GetId());

        server->CrMngr.DestroyCritter(cr3);
        server->CrMngr.DestroyCritter(cr2);
        server->CrMngr.DestroyCritter(cr1);
    }

    SECTION("CritterConditionCpp")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        CHECK(cr->IsAlive());
        CHECK_FALSE(cr->IsDead());
        CHECK_FALSE(cr->IsKnockout());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterItemsCpp")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        auto item = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(get_func("TestItem"), nullptr), true);

        auto inv_items = cr->GetInvItems();
        CHECK_FALSE(inv_items.empty());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterById")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        ident_t cr_id = cr->GetId();
        auto found_cr = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(found_cr);
        CHECK(found_cr == cr);

        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("ItemCppApiAdvanced")
{
    MAKE_LEM_SERVER();

    SECTION("CreateItemOnCritter")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        size_t initial_item_count = server->EntityMngr.GetItemsCount();

        auto item = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(get_func("TestItem"), nullptr), true);
        CHECK(server->EntityMngr.GetItemsCount() >= initial_item_count + 1);

        ident_t item_id = item->GetId();
        auto found = server->EntityMngr.GetItem(item_id);
        REQUIRE(found);
        CHECK(nptr<Item> {found} == item);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("DestroyItemCpp")
    {
        auto cr = server->CreateCritter(get_func("TestCritter"), false);

        auto item = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(get_func("TestItem"), nullptr), true);

        ident_t item_id = item->GetId();
        server->ItemMngr.DestroyItem(item);

        auto gone = server->EntityMngr.GetItem(item_id);
        CHECK_FALSE(static_cast<bool>(gone));

        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("ServerHealthInfoCpp")
{
    MAKE_LEM_SERVER();

    SECTION("HealthInfoContent")
    {
        string info = server->GetHealthInfo();
        CHECK_FALSE(info.empty());
        CHECK(info.find("Server uptime") != string::npos);
    }
}

TEST_CASE("EntityManagerCountsCpp")
{
    MAKE_LEM_SERVER();

    SECTION("InitialState")
    {
        CHECK(server->EntityMngr.GetCrittersCount() >= 0);
        CHECK(server->EntityMngr.GetItemsCount() >= 0);
        CHECK(server->EntityMngr.GetLocationsCount() >= 0);
        CHECK(server->EntityMngr.GetMapsCount() >= 0);
    }

    SECTION("CountsAfterCreation")
    {
        size_t initial_cr = server->EntityMngr.GetCrittersCount();
        size_t initial_loc = server->EntityMngr.GetLocationsCount();

        auto cr = server->CreateCritter(get_func("TestCritter"), false);
        auto loc = server->MapMngr.CreateLocation(get_func("TestLocation"));

        CHECK(server->EntityMngr.GetCrittersCount() >= initial_cr + 1);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_loc + 1);

        server->MapMngr.DestroyLocation(loc);
        server->CrMngr.DestroyCritter(cr);
    }
}

TEST_CASE("TimeEventCancellationContinuesAfterDispatcherFailure")
{
    ScriptFuncDesc timer_desc {};
    MAKE_LEM_SERVER();

    auto cr = server->CreateCritter(get_func("TestCritter"), false).hold_ref();
    timer_desc = BakerTests::MakeNoopCallbackDesc(get_func("LocEntity::OnUnloadTimer"));
    constexpr size_t event_count = 2;

    for (size_t i = 0; i < event_count; i++) {
        ScriptFunc<void> timer_func {&timer_desc};
        REQUIRE(timer_func);

        // The consuming call stays out of REQUIRE: the macro re-expands its argument in the
        // never-executed while clause, which reads as a second use of the moved-from function
        uint32_t event_id = server->TimeEventMngr.StartTimeEvent(cr, Entity::TimeEventData::FuncType {std::move(timer_func)}, timespan {std::chrono::seconds {60}}, {}, {});
        REQUIRE(event_id != 0);
    }

    size_t cancel_calls = 0;
    TimeEventManager::DispatcherHooks hooks;
    hooks.Cancel = [&](uint32_t) {
        cancel_calls++;
        throw GenericException("Injected time-event cancellation notification failure");
    };
    server->TimeEventMngr.SetDispatcherHooks(std::move(hooks));
    auto clear_dispatcher_hooks = scope_exit([&server]() noexcept { safe_call([&server] { server->TimeEventMngr.ClearDispatcherHooks(); }); });

    size_t cancellation_exception_reports = 0;
    auto previous_exception_callback = exceptions::get_callback();
    exceptions::set_callback([&cancellation_exception_reports](string_view message, const stack_trace::catched_data&, bool) {
        if (message.find("Injected time-event cancellation notification failure") != string_view::npos) {
            cancellation_exception_reports++;
        }
    });
    auto restore_exception_callback = scope_exit([previous = std::move(previous_exception_callback)]() mutable noexcept { exceptions::set_callback(std::move(previous)); });

    REQUIRE_NOTHROW(server->TimeEventMngr.CancelAllForEntity(cr));
    CHECK(cancel_calls == event_count);
    CHECK(cancellation_exception_reports == event_count);
    CHECK_FALSE(cr->HasTimeEvents());

    server->TimeEventMngr.ClearDispatcherHooks();
    clear_dispatcher_hooks.release();
    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("TimeEventManagerQueryModifyAndStop")
{
    ScriptFuncDesc timer_desc {};
    MAKE_LEM_SERVER();

    auto cr = server->CreateCritter(get_func("TestCritter"), false).hold_ref();
    hstring timer_name = get_func("LocEntity::OnUnloadTimer");
    timer_desc = BakerTests::MakeNoopCallbackDesc(timer_name);

    auto start_event = [&server, &cr, &timer_desc](timespan delay, timespan repeat) {
        ScriptFunc<void> timer_func {&timer_desc};
        REQUIRE(timer_func);
        return server->TimeEventMngr.StartTimeEvent(cr, Entity::TimeEventData::FuncType {std::move(timer_func)}, delay, repeat, {});
    };

    ScriptFuncName timer_func_name {timer_name, 0};
    ScriptFuncName unknown_func_name {get_func("LocEntity::NoSuchTimer"), 0};

    // Nothing is scheduled yet, so every query answers empty without touching the entity storage
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, timer_func_name, 0) == 0);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, ScriptFuncName(), 0) == 0);
    CHECK_NOTHROW(server->TimeEventMngr.StopTimeEvent(cr, timer_func_name, 0));
    CHECK_NOTHROW(server->TimeEventMngr.ModifyTimeEvent(cr, timer_func_name, 0, timespan {std::chrono::seconds {5}}, std::nullopt));
    CHECK_FALSE(server->TimeEventMngr.FireAndAdvance(cr, 1).has_value());

    uint32_t first_id = start_event(timespan {std::chrono::seconds {60}}, timespan {std::chrono::seconds {5}});
    uint32_t second_id = start_event(timespan {std::chrono::seconds {90}}, {});
    CHECK(first_id != 0);
    CHECK(second_id != 0);
    CHECK(first_id != second_id);
    CHECK(cr->HasTimeEvents());

    // A zero delay is clamped up to the minimum repeat time rather than firing in the past
    uint32_t immediate_id = start_event({}, {});
    CHECK(immediate_id != 0);

    CHECK(server->TimeEventMngr.CountTimeEvent(cr, timer_func_name, 0) == 3);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, unknown_func_name, 0) == 0);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, ScriptFuncName(), first_id) == 1);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, ScriptFuncName(), 999999) == 0);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, ScriptFuncName(), 0) == 0);
    CHECK_THROWS_AS(server->TimeEventMngr.CountTimeEvent(cr, timer_func_name, first_id), VerificationException);

    // Only the addressed event is retimed, and only the addressed data is replaced
    vector<any_t> new_data {any_t {"payload"}};
    server->TimeEventMngr.ModifyTimeEvent(cr, ScriptFuncName(), second_id, timespan {std::chrono::seconds {30}}, vector<any_t> {new_data});
    server->TimeEventMngr.ModifyTimeEvent(cr, ScriptFuncName(), 999999, timespan {std::chrono::seconds {30}}, std::nullopt);
    server->TimeEventMngr.ModifyTimeEvent(cr, unknown_func_name, 0, std::nullopt, vector<any_t> {new_data});

    {
        auto time_events = cr->GetTimeEvents();
        REQUIRE(time_events);
        REQUIRE(time_events->size() == 3);

        bool second_seen = false;

        for (const auto& te : *time_events) {
            if (te->Id == second_id) {
                second_seen = true;
                CHECK(te->RepeatDuration == timespan {std::chrono::seconds {30}});
                CHECK(te->Data.size() == 1);
            }
            else {
                CHECK(te->Data.empty());
            }
        }

        CHECK(second_seen);
    }

    // Modifying by function name with a zero repeat leaves the fire time alone and only swaps data
    server->TimeEventMngr.ModifyTimeEvent(cr, timer_func_name, 0, std::nullopt, vector<any_t> {new_data});
    {
        auto time_events = cr->GetTimeEvents();
        REQUIRE(time_events);

        for (const auto& te : *time_events) {
            CHECK(te->Data.size() == 1);
        }
    }

    server->TimeEventMngr.StopTimeEvent(cr, ScriptFuncName(), 999999);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, timer_func_name, 0) == 3);

    server->TimeEventMngr.StopTimeEvent(cr, ScriptFuncName(), immediate_id);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, timer_func_name, 0) == 2);

    server->TimeEventMngr.StopTimeEvent(cr, unknown_func_name, 0);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, timer_func_name, 0) == 2);

    server->TimeEventMngr.StopTimeEvent(cr, timer_func_name, 0);
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, timer_func_name, 0) == 0);
    CHECK_FALSE(cr->HasTimeEvents());

    server->CrMngr.DestroyCritter(cr);

    // A destroyed entity answers instead of failing, except where starting new work must be rejected
    CHECK(server->TimeEventMngr.CountTimeEvent(cr, timer_func_name, 0) == 0);
    CHECK_NOTHROW(server->TimeEventMngr.StopTimeEvent(cr, timer_func_name, 0));
    CHECK_FALSE(server->TimeEventMngr.FireAndAdvance(cr, first_id).has_value());
    CHECK_THROWS_AS(server->TimeEventMngr.ModifyTimeEvent(cr, timer_func_name, 0, std::nullopt, std::nullopt), VerificationException);
}

TEST_CASE("EntityManagerLoadDocumentDiagnostics")
{
    MAKE_LEM_SERVER();

    hstring locations_collection = get_func("Locations");
    hstring maps_collection = get_func("Maps");
    hstring items_collection = get_func("Items");

    auto make_doc = [](std::initializer_list<pair<string, AnyData::Value>> entries) {
        AnyData::Document doc;

        for (auto&& [key, value] : entries) {
            doc.Assign(key, value.Copy());
        }

        return doc;
    };

    SECTION("MissingDocumentsAreReportedAsErrors")
    {
        bool is_error = false;
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.LoadLocation(ident_t {900001}, is_error)));
        CHECK(is_error);

        is_error = false;
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.LoadMap(ident_t {900002}, is_error)));
        CHECK(is_error);

        is_error = false;
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.LoadItem(ident_t {900003}, is_error)));
        CHECK(is_error);
    }

    SECTION("MalformedProtoSectionsAreRejected")
    {
        ident_t no_proto_id {900011};
        ident_t wrong_type_id {900012};
        ident_t empty_proto_id {900013};
        ident_t unknown_proto_id {900014};

        server->DbStorage.StartCommitChanges();
        server->DbStorage.Insert(locations_collection, DataBaseKey {no_proto_id}, make_doc({{"Filler", AnyData::Value {int64_t {1}}}}));
        server->DbStorage.Insert(locations_collection, DataBaseKey {wrong_type_id}, make_doc({{"_Proto", AnyData::Value {int64_t {7}}}}));
        server->DbStorage.Insert(locations_collection, DataBaseKey {empty_proto_id}, make_doc({{"_Proto", AnyData::Value {string {}}}}));
        server->DbStorage.Insert(locations_collection, DataBaseKey {unknown_proto_id}, make_doc({{"_Proto", AnyData::Value {string {"NoSuchLocationProto"}}}}));
        server->DbStorage.WaitCommitChanges();

        // A document without a '_Proto' section fails because a location is expected to carry one
        bool is_error = false;
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.LoadLocation(no_proto_id, is_error)));
        CHECK(is_error);

        // A '_Proto' section of the wrong value type is rejected before any hashing happens
        is_error = false;
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.LoadLocation(wrong_type_id, is_error)));
        CHECK(is_error);

        is_error = false;
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.LoadLocation(empty_proto_id, is_error)));
        CHECK(is_error);

        // A well-formed name that no proto matches fails later, at proto resolution
        is_error = false;
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.LoadLocation(unknown_proto_id, is_error)));
        CHECK(is_error);
    }

    SECTION("LocationDropsMapIdsThatCannotBeLoaded")
    {
        ident_t loc_id {900021};
        ident_t missing_map_id {900022};

        AnyData::Array map_ids;
        map_ids.EmplaceBack(AnyData::Value {numeric_cast<int64_t>(missing_map_id.underlying_value())});

        AnyData::Document loc_doc;
        loc_doc.Assign("_Proto", AnyData::Value {string {"TestLocation"}});
        loc_doc.Assign("MapIds", AnyData::Value {std::move(map_ids)});

        server->DbStorage.StartCommitChanges();
        server->DbStorage.Insert(locations_collection, DataBaseKey {loc_id}, std::move(loc_doc));
        server->DbStorage.WaitCommitChanges();

        bool is_error = false;
        auto loc = server->EntityMngr.LoadLocation(loc_id, is_error);

        // The location itself restores, but the unloadable map is pruned from its id list
        REQUIRE(static_cast<bool>(loc));
        CHECK(is_error);
        CHECK(loc->GetMapsCount() == 0);
        CHECK(loc->GetMapIds().empty());

        server->MapMngr.DestroyLocation(loc.as_ptr());
    }

    ignore_unused(maps_collection, items_collection);
}

TEST_CASE("TimeEventManagerCollectsReadyEventsAndNotifiesDispatcher")
{
    ScriptFuncDesc timer_desc {};
    MAKE_LEM_SERVER();

    auto cr = server->CreateCritter(get_func("TestCritter"), false).hold_ref();
    hstring timer_name = get_func("LocEntity::OnUnloadTimer");
    timer_desc = BakerTests::MakeNoopCallbackDesc(timer_name);

    vector<uint32_t> scheduled_ids;
    vector<timespan> scheduled_delays;
    vector<uint32_t> cancelled_ids;

    TimeEventManager::DispatcherHooks hooks;
    hooks.Schedule = [&](refcount_ptr<Entity>, uint32_t event_id, timespan delay) {
        scheduled_ids.push_back(event_id);
        scheduled_delays.push_back(delay);
    };
    hooks.Cancel = [&](uint32_t event_id) { cancelled_ids.push_back(event_id); };
    server->TimeEventMngr.SetDispatcherHooks(std::move(hooks));
    auto clear_dispatcher_hooks = scope_exit([&server]() noexcept { safe_call([&server] { server->TimeEventMngr.ClearDispatcherHooks(); }); });

    auto start_event = [&server, &cr, &timer_desc](timespan delay, timespan repeat) {
        ScriptFunc<void> timer_func {&timer_desc};
        REQUIRE(timer_func);
        return server->TimeEventMngr.StartTimeEvent(cr, Entity::TimeEventData::FuncType {std::move(timer_func)}, delay, repeat, {});
    };

    uint32_t ready_id = start_event(TimeEventManager::MIN_REPEAT_TIME, {});
    uint32_t future_id = start_event(timespan {std::chrono::seconds {600}}, {});

    REQUIRE(scheduled_ids.size() == 2);
    CHECK(scheduled_ids[0] == ready_id);
    CHECK(scheduled_ids[1] == future_id);
    CHECK(scheduled_delays[0] == TimeEventManager::MIN_REPEAT_TIME);
    CHECK(scheduled_delays[1] == timespan {std::chrono::seconds {600}});

    // Retiming an event cancels the old dispatcher entry before scheduling the new one
    scheduled_ids.clear();
    scheduled_delays.clear();
    server->TimeEventMngr.ModifyTimeEvent(cr, ScriptFuncName(), future_id, timespan {std::chrono::seconds {300}}, std::nullopt);
    REQUIRE(cancelled_ids.size() == 1);
    CHECK(cancelled_ids[0] == future_id);
    REQUIRE(scheduled_ids.size() == 1);
    CHECK(scheduled_ids[0] == future_id);
    CHECK(scheduled_delays[0] == timespan {std::chrono::seconds {300}});

    // Nothing is due yet, so the collector only reports how long the dispatcher may sleep
    {
        optional<timespan> time_until_pending;
        CHECK(server->TimeEventMngr.CollectReadyTimeEvents(time_until_pending).empty());
        REQUIRE(time_until_pending.has_value());
        CHECK(time_until_pending.value() > timespan {});
    }

    // Backdate one event instead of waiting on the frame clock, so readiness is deterministic
    {
        auto time_events = cr->GetTimeEvents();
        REQUIRE(time_events);

        for (auto& te : *time_events) {
            if (te->Id == ready_id) {
                te->FireTime = nanotime {};
            }
        }
    }

    optional<timespan> time_until_next;
    auto ready = server->TimeEventMngr.CollectReadyTimeEvents(time_until_next);

    REQUIRE(ready.size() == 1);
    CHECK(ready.front().Event->Id == ready_id);
    CHECK(ready.front().OwnerEntity == cr);
    REQUIRE(time_until_next.has_value());
    CHECK(time_until_next.value() > timespan {});

    // A paused dispatcher stops delivering notifications without losing the hooks themselves
    cancelled_ids.clear();
    scheduled_ids.clear();
    server->TimeEventMngr.PauseDispatcherHooks();
    uint32_t paused_id = start_event(timespan {std::chrono::seconds {60}}, {});
    server->TimeEventMngr.StopTimeEvent(cr, ScriptFuncName(), paused_id);
    CHECK(scheduled_ids.empty());
    CHECK(cancelled_ids.empty());

    server->TimeEventMngr.ClearDispatcherHooks();
    clear_dispatcher_hooks.release();

    // Clearing drops every pending event across all polled entities
    server->TimeEventMngr.ClearTimeEvents();
    CHECK_FALSE(cr->HasTimeEvents());

    optional<timespan> time_until_next_after_clear;
    CHECK(server->TimeEventMngr.CollectReadyTimeEvents(time_until_next_after_clear).empty());
    CHECK_FALSE(time_until_next_after_clear.has_value());

    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("DestroyingEntityRejectsNewTimeEvents")
{
    ScriptFuncDesc timer_desc {};
    MAKE_LEM_SERVER();

    auto cr = server->CreateCritter(get_func("TestCritter"), false).hold_ref();
    timer_desc = BakerTests::MakeNoopCallbackDesc(get_func("LocEntity::OnUnloadTimer"));
    bool finish_called = false;
    bool timer_func_resolved = false;
    string rejection;
    Entity::EventCallbackData callback;
    callback.Callback = [&server, &cr, &timer_desc, &finish_called, &timer_func_resolved, &rejection](FuncCallData&) {
        finish_called = true;
        ScriptFunc<void> timer_func {&timer_desc};
        timer_func_resolved = static_cast<bool>(timer_func);

        if (timer_func) {
            try {
                (void)server->TimeEventMngr.StartTimeEvent(cr, Entity::TimeEventData::FuncType {std::move(timer_func)}, timespan {std::chrono::seconds {60}}, {}, {});
            }
            catch (const std::exception& ex) {
                rejection = ex.what();
            }
        }

        return Entity::EventResult::ContinueChain;
    };
    callback.SubscriptionPtr = reinterpret_cast<uintptr_t>(&finish_called);
    server->OnCritterFinish.Subscribe(std::move(callback));
    auto unsubscribe = scope_exit([&server, &finish_called]() noexcept { server->OnCritterFinish.Unsubscribe(reinterpret_cast<uintptr_t>(&finish_called)); });

    server->CrMngr.DestroyCritter(cr);

    CHECK(finish_called);
    CHECK(timer_func_resolved);
    CHECK(rejection.find("Cannot start a time event for an entity that is being destroyed") != string::npos);
    CHECK_FALSE(cr->HasTimeEvents());
}

FO_END_NAMESPACE
