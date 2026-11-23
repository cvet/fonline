// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "Test_ServerItemsHelpers.h"

FO_BEGIN_NAMESPACE

using namespace ServerItemsTests;

TEST_CASE("AngelScriptServerItemCreationAndDestruction")
{
    auto settings {ServerItemsTests::MakeSettings()};
    auto server {ServerItemsTests::MakeServerEngine(settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {ServerItemsTests::WaitForStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

    auto fn {[&server](string_view name) { return server->Hashes.to_hashed_string(name); }};

    hstring item_pid {fn("TestItem")};
    REQUIRE(static_cast<bool>(server->GetProtoItem(item_pid)));

    size_t initial_item_count {server->EntityMngr.GetItemsCount()};
    size_t initial_entity_count {server->EntityMngr.GetEntitiesCount()};

    // Create item
    auto item {server->ItemMngr.CreateItem(item_pid, nullptr)};

    ident_t item_id {item->GetId()};
    CHECK(item->GetProtoId() == item_pid);
    auto found_item {server->EntityMngr.GetItem(item_id)};
    REQUIRE(found_item);
    CHECK(found_item == item);
    CHECK(server->EntityMngr.GetItemsCount() == initial_item_count + 1);
    CHECK(server->EntityMngr.GetEntitiesCount() > initial_entity_count);

    // OnItemInit should have been called
    int init_calls {0};
    REQUIRE(server->CallFunc(fn("ServerItemsTest::GetItemInitCalls"), init_calls));
    CHECK(init_calls >= 1);

    int64_t last_item_id {0};
    REQUIRE(server->CallFunc(fn("ServerItemsTest::GetLastItemId"), last_item_id));
    CHECK(last_item_id == item_id.underlying_value());

    // Create second item
    auto item2 {server->ItemMngr.CreateItem(item_pid, nullptr)};
    CHECK(item2->GetId() != item_id);
    CHECK(server->EntityMngr.GetItemsCount() == initial_item_count + 2);

    // Destroy items
    ident_t item2_id {item2->GetId()};
    server->ItemMngr.DestroyItem(item2);
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetItem(item2_id)));
    CHECK(server->EntityMngr.GetItemsCount() == initial_item_count + 1);

    server->ItemMngr.DestroyItem(item);
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetItem(item_id)));
    CHECK(server->EntityMngr.GetItemsCount() == initial_item_count);
}

TEST_CASE("AngelScriptServerItemAddedToCritterInventory")
{
    auto settings {ServerItemsTests::MakeSettings()};
    auto server {ServerItemsTests::MakeServerEngine(settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {ServerItemsTests::WaitForStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

    auto fn {[&server](string_view name) { return server->Hashes.to_hashed_string(name); }};

    hstring critter_pid {fn("TestCritter")};
    hstring item_pid {fn("TestItem")};

    // Create critter
    auto cr {server->CreateCritter(critter_pid, false)};
    CHECK_FALSE(cr->HasItems());

    // Add item to critter inventory
    auto item {server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(item_pid, nullptr), true)};
    CHECK(cr->HasItems());

    vector<ptr<Item>> inv_items {cr->GetInvItems()};
    CHECK_FALSE(inv_items.empty());

    ident_t item_id {item->GetId()};
    CHECK(cr->GetInvItem(item_id) == item);

    // A second item of the same prototype is a separate instance
    auto item2 {server->CrMngr.AddItemToCritter(cr, server->ItemMngr.CreateItem(item_pid, nullptr), true)};
    CHECK(item2->GetId() != item->GetId());

    vector<ptr<Item>> inv_items2 {cr->GetInvItems()};
    CHECK(inv_items2.size() == 2);

    // Check via script
    auto cr_item_count_func {server->FindFunc<int64_t, ptr<Critter>>(fn("ServerItemsTest::GetCritterItemCount"))};
    REQUIRE(cr_item_count_func);
    REQUIRE(cr_item_count_func.Call(cr));
    CHECK(cr_item_count_func.GetResult() == 2);

    // Destroy inventory
    server->CrMngr.DestroyInventory(cr);
    CHECK_FALSE(cr->HasItems());
    CHECK(cr->GetInvItems().empty());

    // Cleanup critter
    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("AngelScriptServerCritterLifecycleOperations")
{
    auto settings {ServerItemsTests::MakeSettings()};
    auto server {ServerItemsTests::MakeServerEngine(settings)};

    auto shutdown {scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    })};

    string startup_error {ServerItemsTests::WaitForStart(server)};
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock {scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); })};

    auto fn {[&server](string_view name) { return server->Hashes.to_hashed_string(name); }};

    hstring critter_pid {fn("TestCritter")};

    // Create multiple critters
    size_t cr_count_before {server->EntityMngr.GetCrittersCount()};

    auto cr1 {server->CreateCritter(critter_pid, false)};
    auto cr2 {server->CreateCritter(critter_pid, false)};
    auto cr3 {server->CreateCritter(critter_pid, true)};

    CHECK(server->EntityMngr.GetCrittersCount() == cr_count_before + 3);

    // All IDs should be unique
    CHECK(cr1->GetId() != cr2->GetId());
    CHECK(cr2->GetId() != cr3->GetId());
    CHECK(cr1->GetId() != cr3->GetId());

    // Check player status
    CHECK_FALSE(cr1->GetControlledByPlayer());
    CHECK_FALSE(cr2->GetControlledByPlayer());
    CHECK(cr3->GetControlledByPlayer());

    // Entity lookup
    CHECK(static_cast<bool>(server->EntityMngr.GetEntity(cr1->GetId())));
    auto found_cr1 {server->EntityMngr.GetCritter(cr1->GetId())};
    auto found_cr2 {server->EntityMngr.GetCritter(cr2->GetId())};
    auto found_cr3 {server->EntityMngr.GetCritter(cr3->GetId())};
    REQUIRE(found_cr1);
    REQUIRE(found_cr2);
    REQUIRE(found_cr3);
    CHECK(found_cr1 == cr1);
    CHECK(found_cr2 == cr2);
    CHECK(found_cr3 == cr3);

    // Script alive/dead check
    auto is_alive_func {server->FindFunc<bool, ptr<Critter>>(fn("ServerItemsTest::TestCritterIsAlive"))};
    REQUIRE(is_alive_func);
    REQUIRE(is_alive_func.Call(cr1));
    CHECK(is_alive_func.GetResult() == true);

    // Player login allocates persistent ids through the entity manager
    auto player1 {ServerItemsTests::CreateLoggedPlayer(server, "TestPlayer1")};
    auto player2 {ServerItemsTests::CreateLoggedPlayer(server, "TestPlayer2")};
    CHECK(player1->GetId() != ident_t {});
    CHECK(player2->GetId() != ident_t {});
    CHECK(player1->GetId() != player2->GetId());

    // Unload player critter
    ident_t cr3_id {cr3->GetId()};
    server->UnloadCritter(cr3);
    CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr3_id)));

    // Health info contains something
    string health {server->GetHealthInfo()};
    CHECK_FALSE(health.empty());

    // Destroy remaining critters
    server->CrMngr.DestroyCritter(cr2);
    server->CrMngr.DestroyCritter(cr1);

    CHECK(server->EntityMngr.GetCrittersCount() == cr_count_before);
}

FO_END_NAMESPACE

#endif
