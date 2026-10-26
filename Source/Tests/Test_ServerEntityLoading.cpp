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

#include "catch_amalgamated.hpp"

#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakeEntityLoadingSettings() -> GlobalSettings
{
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedServerSettings(settings);
    BakerTests::OverrideSetting(settings.Server.DbStorage, string {"Memory"});
    BakerTests::OverrideSetting(settings.Server.WriteHealthFile, false);
    BakerTests::OverrideSetting(settings.Server.WorkerThreads, int32_t {1});
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    return settings;
}

static auto MakeEntityLoadingResources() -> FileSystem
{
    // Exercise critter, singleton and nested holder persistence with the same custom entity types
    vector<uint8_t> metadata = BakerTests::MakeMetadataBlob({
        {"Entity", {{"LoadingRecord"}, {"LoadingChild"}}},
        {"EntityHolder",
            {
                {"Server", "Critter", "LoadingRecord", "HeldRecords", "Persistent"},
                {"Server", "Game", "LoadingRecord", "PersistentRecords", "Persistent"},
                {"Server", "Game", "LoadingRecord", "TemporaryRecords"},
                {"Server", "LoadingRecord", "LoadingChild", "NestedRecords", "Persistent"},
            }},
    });
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("EntityLoadingCompiler");
    compiler_source->AddFile("Metadata.fometa-server", metadata);

    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerServerEngine proto_engine {compiler_resources};

    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("EntityLoadingRuntime");
    source->AddFile("Metadata.fometa-server", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
    source->AddFile("EntityLoading.fos-bin-server", BakerTests::CompileInlineScripts(&proto_engine, "EntityLoadingScripts", {{"Scripts/EntityLoading.fos", "void EntityLoadingFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    source->AddFile("LoadingCritter.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, proto_engine.Hashes.to_hashed_string("Critter"), "LoadingCritter"));
    source->AddFile("LoadingItem.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, proto_engine.Hashes.to_hashed_string("Item"), "LoadingItem"));

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

static auto WaitForEntityLoadingStartup(ptr<ServerEngine> server) -> bool
{
    nanotime deadline = nanotime::now() + std::chrono::seconds {30};

    while (nanotime::now() < deadline) {
        if (server->IsStartingError()) {
            return false;
        }
        if (server->IsStarted()) {
            return true;
        }

        coarse_sleep(std::chrono::milliseconds {10});
    }

    return false;
}

static auto CollectItemIds(const vector<ptr<Item>>& items) -> vector<ident_t>
{
    return vec_transform(items, [](ptr<Item> item) -> ident_t { return item->GetId(); });
}

static auto CollectCustomEntityIds(ptr<Entity> holder, hstring entry) -> vector<ident_t>
{
    auto inner_entities = holder->GetInnerEntities(entry);

    if (!inner_entities) {
        return {};
    }

    return vec_transform(*inner_entities, [](auto&& entity) -> ident_t { return require_refcount_ptr(entity.template dyn_cast<const CustomEntity>())->GetId(); });
}

static void DiscardLoadedCritter(ptr<ServerEngine> server, ptr<Critter> cr)
{
    cr->MarkAsDestroying();

    if (cr->GetRawGlobalMapGroup()) {
        server->MapMngr.RemoveCritterFromMap(cr, nullptr);
    }

    server->UnloadCritterInnerEntities(cr);
    cr->MarkAsDestroyed();
    server->EntityMngr.UnregisterCritter(cr, false);
}

#define MAKE_ENTITY_LOADING_SERVER() \
    GlobalSettings settings = MakeEntityLoadingSettings(); \
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeEntityLoadingResources()); \
    auto shutdown = scope_exit([&server]() noexcept { \
        safe_call([&server] { \
            if (server->IsStarted()) { \
                server->Shutdown(); \
            } \
        }); \
    }); \
    REQUIRE(WaitForEntityLoadingStartup(server)); \
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}})); \
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); }); \
    hstring critter_pid = server->Hashes.to_hashed_string("LoadingCritter"); \
    hstring item_pid = server->Hashes.to_hashed_string("LoadingItem"); \
    hstring record_type = server->Hashes.to_hashed_string("LoadingRecord"); \
    hstring record_entry = server->Hashes.to_hashed_string("HeldRecords"); \
    ignore_unused(item_pid, record_type, record_entry)

// A login restores the whole inventory tree, so the reads it costs must follow the depth of the tree, not its size
TEST_CASE("ServerLoadCritterReadsInventoryTreeOneRequestPerLevel", "[server][entity][database]")
{
    MAKE_ENTITY_LOADING_SERVER();

    constexpr size_t top_level_items = 20;
    constexpr size_t second_level_items = 10;
    constexpr size_t third_level_items = 5;

    ptr<Critter> cr = server->CreateCritter(critter_pid, true);
    vector<ident_t> top_level_ids;

    for (size_t i = 0; i < top_level_items; i++) {
        ptr<Item> item = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(item_pid, nullptr), true);
        top_level_ids.emplace_back(item->GetId());
    }

    auto outer = server->EntityMngr.GetItem(top_level_ids.front());
    REQUIRE(outer);
    vector<ident_t> second_level_ids;

    for (size_t i = 0; i < second_level_items; i++) {
        ptr<Item> inner = outer->AddItemToContainer(server->ItemMngr.CreateItem(item_pid, nullptr), {});
        second_level_ids.emplace_back(inner->GetId());
    }

    auto middle = server->EntityMngr.GetItem(second_level_ids.back());
    REQUIRE(middle);
    vector<ident_t> third_level_ids;

    for (size_t i = 0; i < third_level_items; i++) {
        ptr<Item> inner = middle->AddItemToContainer(server->ItemMngr.CreateItem(item_pid, nullptr), {});
        third_level_ids.emplace_back(inner->GetId());
    }

    server->EntityMngr.MakePersistent(cr, true, true);
    server->DbStorage.WaitCommitChanges();

    ident_t cr_id = cr->GetId();
    server->UnloadCritter(cr);
    server->DbStorage.WaitCommitChanges();
    REQUIRE_FALSE(server->EntityMngr.GetItem(third_level_ids.front()));

    size_t requests_before = server->DbStorage.GetDbRequestsPerMinute();
    bool is_error = false;
    auto loaded = server->EntityMngr.LoadCritter(cr_id, false, nullptr, is_error);
    size_t requests_after = server->DbStorage.GetDbRequestsPerMinute();

    REQUIRE(loaded);
    CHECK_FALSE(is_error);

    // The critter, then one read per nesting level; a read per item would cost 36 here
    REQUIRE(requests_after >= requests_before);
    CHECK(requests_after - requests_before <= 8);

    CHECK(loaded->GetItemIds() == top_level_ids);
    CHECK(CollectItemIds(loaded->GetInvItems()) == top_level_ids);

    auto loaded_outer = server->EntityMngr.GetItem(top_level_ids.front());
    REQUIRE(loaded_outer);
    CHECK(loaded_outer->GetInnerItemIds() == second_level_ids);
    CHECK(CollectItemIds(loaded_outer->GetAllInnerItems()) == second_level_ids);

    auto loaded_middle = server->EntityMngr.GetItem(second_level_ids.back());
    REQUIRE(loaded_middle);
    CHECK(loaded_middle->GetContainerId() == top_level_ids.front());
    CHECK(loaded_middle->GetInnerItemIds() == third_level_ids);
    CHECK(CollectItemIds(loaded_middle->GetAllInnerItems()) == third_level_ids);

    for (ident_t third_level_id : third_level_ids) {
        auto loaded_inner = server->EntityMngr.GetItem(third_level_id);
        REQUIRE(loaded_inner);
        CHECK(loaded_inner->GetOwnership() == ItemOwnership::ItemContainer);
        CHECK(loaded_inner->GetContainerId() == second_level_ids.back());
    }

    DiscardLoadedCritter(server, loaded);
}

// A low-level partial load prunes its in-memory container, while the failed top-level load must keep durable IDs
TEST_CASE("ServerLoadCritterPrunesMissingNestedItemAndKeepsItsSiblings", "[server][entity][database]")
{
    MAKE_ENTITY_LOADING_SERVER();

    ptr<Critter> cr = server->CreateCritter(critter_pid, true);
    ptr<Item> outer = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(item_pid, nullptr), true);
    ptr<Item> kept_first = outer->AddItemToContainer(server->ItemMngr.CreateItem(item_pid, nullptr), {});
    ptr<Item> missing = outer->AddItemToContainer(server->ItemMngr.CreateItem(item_pid, nullptr), {});
    ptr<Item> kept_last = outer->AddItemToContainer(server->ItemMngr.CreateItem(item_pid, nullptr), {});

    server->EntityMngr.MakePersistent(cr, true, true);
    server->DbStorage.WaitCommitChanges();

    ident_t cr_id = cr->GetId();
    ident_t outer_id = outer->GetId();
    ident_t kept_first_id = kept_first->GetId();
    ident_t missing_id = missing->GetId();
    ident_t kept_last_id = kept_last->GetId();

    server->UnloadCritter(cr);
    server->DbStorage.WaitCommitChanges();
    server->DbStorage.Delete(server->Hashes.to_hashed_string("Items"), missing_id);
    server->DbStorage.WaitCommitChanges();

    bool is_error = false;
    auto loaded = server->EntityMngr.LoadCritter(cr_id, false, nullptr, is_error);
    REQUIRE(loaded);

    CHECK(is_error);
    CHECK_FALSE(server->EntityMngr.GetItem(missing_id));

    auto loaded_outer = server->EntityMngr.GetItem(outer_id);
    REQUIRE(loaded_outer);
    CHECK(loaded_outer->GetInnerItemIds() == vector<ident_t> {kept_first_id, kept_last_id});
    CHECK(CollectItemIds(loaded_outer->GetAllInnerItems()) == vector<ident_t> {kept_first_id, kept_last_id});

    DiscardLoadedCritter(server, loaded);
}

// Custom inner entities of one holder entry come back with one request as well, whatever their number
TEST_CASE("ServerLoadCritterReadsCustomInnerEntitiesInOneRequest", "[server][entity][database]")
{
    MAKE_ENTITY_LOADING_SERVER();

    constexpr size_t record_count = 12;

    ptr<Critter> cr = server->CreateCritter(critter_pid, true);
    vector<ident_t> record_ids;

    for (size_t i = 0; i < record_count; i++) {
        record_ids.emplace_back(server->EntityMngr.CreateCustomInnerEntity(cr, record_entry, {})->GetId());
    }

    server->EntityMngr.MakePersistent(cr, true, true);
    server->DbStorage.WaitCommitChanges();

    ident_t cr_id = cr->GetId();
    server->UnloadCritter(cr);
    server->DbStorage.WaitCommitChanges();
    REQUIRE_FALSE(server->EntityMngr.GetCustomEntity(record_type, record_ids.front()));

    size_t requests_before = server->DbStorage.GetDbRequestsPerMinute();
    bool is_error = false;
    auto loaded = server->EntityMngr.LoadCritter(cr_id, false, nullptr, is_error);
    size_t requests_after = server->DbStorage.GetDbRequestsPerMinute();

    REQUIRE(loaded);
    CHECK_FALSE(is_error);

    // The critter and one read for the whole entry; a read per record would cost 13 here
    REQUIRE(requests_after >= requests_before);
    CHECK(requests_after - requests_before <= 3);

    auto holder_prop = server->GetEntityHolderIdsProp(loaded, record_entry);
    CHECK(loaded->GetProperties()->GetValueFast<vector<ident_t>>(holder_prop.get()) == record_ids);
    CHECK(CollectCustomEntityIds(loaded, record_entry) == record_ids);

    for (ident_t record_id : record_ids) {
        auto record = server->EntityMngr.GetCustomEntity(record_type, record_id);
        REQUIRE(record);
        CHECK(record->GetCustomHolderId() == cr_id);
        CHECK(record->GetCustomHolderEntry() == record_entry);
    }

    DiscardLoadedCritter(server, loaded);
}

// A low-level partial load prunes its in-memory holder, while the failed top-level load must keep durable IDs
TEST_CASE("ServerLoadCritterPrunesMissingCustomInnerEntityAndKeepsItsSiblings", "[server][entity][database]")
{
    MAKE_ENTITY_LOADING_SERVER();

    ptr<Critter> cr = server->CreateCritter(critter_pid, true);
    ident_t kept_first_id = server->EntityMngr.CreateCustomInnerEntity(cr, record_entry, {})->GetId();
    ident_t missing_id = server->EntityMngr.CreateCustomInnerEntity(cr, record_entry, {})->GetId();
    ident_t kept_last_id = server->EntityMngr.CreateCustomInnerEntity(cr, record_entry, {})->GetId();

    server->EntityMngr.MakePersistent(cr, true, true);
    server->DbStorage.WaitCommitChanges();

    ident_t cr_id = cr->GetId();
    server->UnloadCritter(cr);
    server->DbStorage.WaitCommitChanges();
    server->DbStorage.Delete(server->Hashes.to_hashed_string("LoadingRecords"), missing_id);
    server->DbStorage.WaitCommitChanges();

    bool is_error = false;
    auto loaded = server->EntityMngr.LoadCritter(cr_id, false, nullptr, is_error);
    REQUIRE(loaded);

    CHECK(is_error);
    CHECK_FALSE(server->EntityMngr.GetCustomEntity(record_type, missing_id));

    auto holder_prop = server->GetEntityHolderIdsProp(loaded, record_entry);
    CHECK(loaded->GetProperties()->GetValueFast<vector<ident_t>>(holder_prop.get()) == vector<ident_t> {kept_first_id, kept_last_id});
    CHECK(CollectCustomEntityIds(loaded, record_entry) == vector<ident_t> {kept_first_id, kept_last_id});

    DiscardLoadedCritter(server, loaded);
}

TEST_CASE("ServerFailedCritterLoadKeepsCustomEntityReferencesForRetry", "[server][entity][database]")
{
    MAKE_ENTITY_LOADING_SERVER();

    ptr<Critter> cr = server->CreateCritter(critter_pid, false);
    ident_t kept_first_id = server->EntityMngr.CreateCustomInnerEntity(cr, record_entry, {})->GetId();
    ident_t missing_id = server->EntityMngr.CreateCustomInnerEntity(cr, record_entry, {})->GetId();
    ptr<CustomEntity> kept_last = server->EntityMngr.CreateCustomInnerEntity(cr, record_entry, {});
    hstring nested_entry = server->Hashes.to_hashed_string("NestedRecords");
    ident_t nested_id = server->EntityMngr.CreateCustomInnerEntity(kept_last, nested_entry, {})->GetId();
    vector<ident_t> record_ids {kept_first_id, missing_id, kept_last->GetId()};

    server->EntityMngr.MakePersistent(cr, true, true);
    server->DbStorage.WaitCommitChanges();

    ident_t cr_id = cr->GetId();
    ident_t kept_last_id = kept_last->GetId();
    hstring critters_collection = server->Hashes.to_hashed_string("Critters");
    hstring records_collection = server->Hashes.to_hashed_string("LoadingRecords");
    hstring children_collection = server->Hashes.to_hashed_string("LoadingChilds");
    auto original_critter_doc = server->DbStorage.Get(critters_collection, cr_id);
    auto original_kept_first_doc = server->DbStorage.Get(records_collection, kept_first_id);
    auto missing_doc = server->DbStorage.Get(records_collection, missing_id);
    auto original_kept_last_doc = server->DbStorage.Get(records_collection, kept_last_id);
    auto original_nested_doc = server->DbStorage.Get(children_collection, nested_id);

    server->UnloadCritter(cr);
    server->DbStorage.WaitCommitChanges();
    server->DbStorage.Delete(records_collection, missing_id);
    server->DbStorage.WaitCommitChanges();

    CHECK_THROWS_AS(server->LoadCritter(cr_id, false), GenericException);
    server->DbStorage.WaitCommitChanges();

    CHECK(server->DbStorage.Get(critters_collection, cr_id) == original_critter_doc);
    CHECK(server->DbStorage.Get(records_collection, kept_first_id) == original_kept_first_doc);
    CHECK(server->DbStorage.Get(records_collection, kept_last_id) == original_kept_last_doc);
    CHECK(server->DbStorage.Get(children_collection, nested_id) == original_nested_doc);
    CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));

    CHECK_THROWS_AS(server->LoadCritter(cr_id, false), GenericException);
    server->DbStorage.WaitCommitChanges();
    CHECK(server->DbStorage.Get(critters_collection, cr_id) == original_critter_doc);

    server->DbStorage.Insert(records_collection, missing_id, std::move(missing_doc));
    server->DbStorage.WaitCommitChanges();

    ptr<Critter> loaded = server->LoadCritter(cr_id, false);
    auto holder_prop = server->GetEntityHolderIdsProp(loaded, record_entry);
    CHECK(loaded->GetProperties()->GetValueFast<vector<ident_t>>(holder_prop.get()) == record_ids);
    CHECK(CollectCustomEntityIds(loaded, record_entry) == record_ids);

    auto loaded_last = server->EntityMngr.GetCustomEntity(record_type, kept_last_id);
    REQUIRE(loaded_last);
    hstring child_type = server->Hashes.to_hashed_string("LoadingChild");
    auto loaded_nested = server->EntityMngr.GetCustomEntity(child_type, nested_id);
    REQUIRE(loaded_nested);
    CHECK(loaded_nested->GetCustomHolderId() == kept_last_id);
    CHECK(loaded_nested->GetCustomHolderEntry() == nested_entry);

    DiscardLoadedCritter(server, loaded);
}

TEST_CASE("ServerGameHolderPersistsDeclaredChildren", "[server][entity][database]")
{
    MAKE_ENTITY_LOADING_SERVER();
    server->LockForPropertyAccess();
    auto unlock_singleton = scope_exit([&server]() noexcept { server->UnlockForPropertyAccess(); });

    hstring persistent_entry = server->Hashes.to_hashed_string("PersistentRecords");
    hstring temporary_entry = server->Hashes.to_hashed_string("TemporaryRecords");
    hstring nested_entry = server->Hashes.to_hashed_string("NestedRecords");
    auto persistent = server->EntityMngr.CreateCustomInnerEntity(server, persistent_entry, {});
    auto nested = server->EntityMngr.CreateCustomInnerEntity(persistent, nested_entry, {});
    auto temporary = server->EntityMngr.CreateCustomInnerEntity(server, temporary_entry, {});

    CHECK(persistent->IsPersistent());
    CHECK(nested->IsPersistent());
    CHECK_FALSE(temporary->IsPersistent());
    CHECK(persistent->GetEntityLock() == server->GetEntityLock());
    CHECK(nested->GetEntityLock() == server->GetEntityLock());
    CHECK(nested->GetCustomHolderId() == persistent->GetId());
    CHECK(persistent->GetCustomHolderEntry() == persistent_entry);

    server->DbStorage.WaitCommitChanges();
    hstring records_collection = server->Hashes.to_hashed_string("LoadingRecords");
    hstring children_collection = server->Hashes.to_hashed_string("LoadingChilds");
    auto persistent_doc = server->DbStorage.Get(records_collection, persistent->GetId());
    auto nested_doc = server->DbStorage.Get(children_collection, nested->GetId());
    auto temporary_doc = server->DbStorage.Get(records_collection, temporary->GetId());
    CHECK_FALSE(persistent_doc.Empty());
    CHECK_FALSE(nested_doc.Empty());
    CHECK(temporary_doc.Empty());
}

// Unloading a persistent NPC keeps its critter and carried item records for the next load; destroy is what deletes them
TEST_CASE("ServerUnloadLoadKeepsPersistentNpcAndItemRecordsUntilDestroy", "[server][entity][database]")
{
    MAKE_ENTITY_LOADING_SERVER();

    ptr<Critter> cr = server->CreateCritter(critter_pid, false);
    REQUIRE_FALSE(cr->GetControlledByPlayer());

    ptr<Item> item = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(item_pid, nullptr), true);

    server->EntityMngr.MakePersistent(cr, true, true);
    server->DbStorage.WaitCommitChanges();

    ident_t cr_id = cr->GetId();
    ident_t item_id = item->GetId();
    hstring critters_collection = server->Hashes.to_hashed_string("Critters");
    hstring items_collection = server->Hashes.to_hashed_string("Items");

    REQUIRE(server->DbStorage.Valid(critters_collection, cr_id));
    REQUIRE(server->DbStorage.Valid(items_collection, item_id));

    server->UnloadCritter(cr);
    server->DbStorage.WaitCommitChanges();

    REQUIRE_FALSE(server->EntityMngr.GetCritter(cr_id));
    REQUIRE_FALSE(server->EntityMngr.GetItem(item_id));

    REQUIRE(server->DbStorage.Valid(critters_collection, cr_id));
    REQUIRE(server->DbStorage.Valid(items_collection, item_id));

    ptr<Critter> loaded = server->LoadCritter(cr_id, false);
    REQUIRE_FALSE(loaded->GetControlledByPlayer());
    CHECK(loaded->GetId() == cr_id);
    CHECK(loaded->GetItemIds() == vector<ident_t> {item_id});
    CHECK(CollectItemIds(loaded->GetInvItems()) == vector<ident_t> {item_id});

    auto loaded_item = server->EntityMngr.GetItem(item_id);
    REQUIRE(loaded_item);
    CHECK(loaded_item->GetCritterId() == cr_id);
    CHECK(loaded_item->GetOwnership() == ItemOwnership::CritterInventory);

    server->CrMngr.DestroyCritter(loaded);
    server->DbStorage.WaitCommitChanges();

    CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
    CHECK_FALSE(server->EntityMngr.GetItem(item_id));
    CHECK_FALSE(server->DbStorage.Valid(critters_collection, cr_id));
    CHECK_FALSE(server->DbStorage.Valid(items_collection, item_id));
}

TEST_CASE("ServerFailedNpcLoadKeepsRecoverableRecords", "[server][entity][database]")
{
    MAKE_ENTITY_LOADING_SERVER();

    ptr<Critter> cr = server->CreateCritter(critter_pid, false);
    REQUIRE_FALSE(cr->GetControlledByPlayer());
    ptr<Item> kept = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(item_pid, nullptr), true);
    ptr<Item> missing = server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(item_pid, nullptr), true);
    REQUIRE(cr->GetInvItems().size() == 2);

    server->EntityMngr.MakePersistent(cr, true, true);
    server->DbStorage.WaitCommitChanges();

    ident_t cr_id = cr->GetId();
    ident_t kept_id = kept->GetId();
    ident_t missing_id = missing->GetId();
    hstring critters_collection = server->Hashes.to_hashed_string("Critters");
    hstring items_collection = server->Hashes.to_hashed_string("Items");
    auto original_critter_doc = server->DbStorage.Get(critters_collection, cr_id);
    auto original_kept_doc = server->DbStorage.Get(items_collection, kept_id);
    auto missing_doc = server->DbStorage.Get(items_collection, missing_id);

    server->UnloadCritter(cr);
    server->DbStorage.Delete(items_collection, missing_id);
    server->DbStorage.WaitCommitChanges();
    REQUIRE_FALSE(server->DbStorage.Valid(items_collection, missing_id));

    CHECK_THROWS_AS(server->LoadCritter(cr_id, false), GenericException);
    server->DbStorage.WaitCommitChanges();
    CHECK_FALSE(server->EntityMngr.GetCritter(cr_id));
    CHECK_FALSE(server->EntityMngr.GetItem(kept_id));
    CHECK(server->DbStorage.Get(critters_collection, cr_id) == original_critter_doc);
    CHECK(server->DbStorage.Get(items_collection, kept_id) == original_kept_doc);

    CHECK_THROWS_AS(server->LoadCritter(cr_id, false), GenericException);
    server->DbStorage.WaitCommitChanges();
    CHECK(server->DbStorage.Get(critters_collection, cr_id) == original_critter_doc);

    server->DbStorage.Insert(items_collection, missing_id, std::move(missing_doc));
    server->DbStorage.WaitCommitChanges();

    ptr<Critter> loaded = server->LoadCritter(cr_id, false);
    CHECK_FALSE(loaded->GetControlledByPlayer());
    CHECK(loaded->GetItemIds() == vector<ident_t> {kept_id, missing_id});
    auto loaded_item = server->EntityMngr.GetItem(kept_id);
    REQUIRE(loaded_item);
    CHECK(loaded_item->GetCritterId() == cr_id);
    REQUIRE(server->EntityMngr.GetItem(missing_id));
    server->CrMngr.DestroyCritter(loaded);
    server->DbStorage.WaitCommitChanges();
    CHECK_FALSE(server->DbStorage.Valid(critters_collection, cr_id));
    CHECK_FALSE(server->DbStorage.Valid(items_collection, kept_id));
}

FO_END_NAMESPACE
