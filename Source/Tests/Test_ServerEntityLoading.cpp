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

#include "catch_amalgamated.hpp"

#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakeEntityLoadingSettings() -> GlobalSettings
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    // A persistent holder entry on the critter, so custom inner entities travel with it to the database and back
    vector<uint8_t> metadata = BakerTests::MakeMetadataBlob({
        {"Entity", {{"LoadingRecord"}}},
        {"EntityHolder", {{"Server", "Critter", "LoadingRecord", "HeldRecords", "Persistent"}}},
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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    return vec_transform(items, [](ptr<Item> item) -> ident_t { return item->GetId(); });
}

static auto CollectCustomEntityIds(ptr<Entity> holder, hstring entry) -> vector<ident_t>
{
    FO_STACK_TRACE_ENTRY();

    auto inner_entities = holder->GetInnerEntities(entry);

    if (!inner_entities) {
        return {};
    }

    return vec_transform(*inner_entities, [](auto&& entity) -> ident_t { return require_refcount_ptr(entity.template dyn_cast<const CustomEntity>())->GetId(); });
}

static void DiscardLoadedCritter(ptr<ServerEngine> server, ptr<Critter> cr)
{
    FO_STACK_TRACE_ENTRY();

    cr->MarkAsDestroying();
    server->UnloadCritterInnerEntities(cr);
    cr->MarkAsDestroyed();
    server->EntityMngr.UnregisterCritter(cr);
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
    auto loaded = server->EntityMngr.LoadCritter(cr_id, false, is_error);
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

// One lost record must cost its container only that entry: the siblings read in the same batch still come back
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
    auto loaded = server->EntityMngr.LoadCritter(cr_id, false, is_error);
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
    auto loaded = server->EntityMngr.LoadCritter(cr_id, false, is_error);
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

// One lost record costs the holder only that entry: the siblings read in the same batch still come back
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
    auto loaded = server->EntityMngr.LoadCritter(cr_id, false, is_error);
    REQUIRE(loaded);

    CHECK(is_error);
    CHECK_FALSE(server->EntityMngr.GetCustomEntity(record_type, missing_id));

    auto holder_prop = server->GetEntityHolderIdsProp(loaded, record_entry);
    CHECK(loaded->GetProperties()->GetValueFast<vector<ident_t>>(holder_prop.get()) == vector<ident_t> {kept_first_id, kept_last_id});
    CHECK(CollectCustomEntityIds(loaded, record_entry) == vector<ident_t> {kept_first_id, kept_last_id});

    DiscardLoadedCritter(server, loaded);
}

FO_END_NAMESPACE
