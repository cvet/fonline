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

#include "Test_ClientServerIntegrationHelpers.h"

FO_BEGIN_NAMESPACE

using namespace TestClientServerIntegration;

TEST_CASE("AngelScriptClientAndServerHandshakeOverInterthreadTransport")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    CHECK(GetServerConnectionCount(server) == 0);
    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));

    auto get_client_func_name = [&client](string_view name) { return client->Hashes.to_hashed_string(name); };

    int connecting_calls = 0;
    int connected_calls = 0;
    int login_success_calls = 0;
    int disconnected_calls = 0;

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectingCalls"), connecting_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectedCalls"), connected_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetLoginSuccessCalls"), login_success_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));

    CHECK(connecting_calls == 0);
    CHECK(connected_calls == 0);
    CHECK(login_success_calls == 0);
    CHECK(disconnected_calls == 0);

    client->Connect();

    REQUIRE(WaitForConnected(client, server));

    CHECK(client->IsConnected());
    CHECK_FALSE(client->IsConnecting());
    REQUIRE(static_cast<bool>(client->GetCurPlayer()));
    CHECK(client->GetConnection()->GetBytesSend() > 0);
    CHECK(client->GetConnection()->GetBytesReceived() > 0);
    CHECK(GetServerConnectionCount(server) == 1);

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectingCalls"), connecting_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetConnectedCalls"), connected_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetLoginSuccessCalls"), login_success_calls));
    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));

    CHECK(connecting_calls >= 1);
    CHECK(connected_calls >= 1);
    CHECK(login_success_calls == 0);
    CHECK(disconnected_calls == 0);

    client->Disconnect();

    REQUIRE(WaitForDisconnected(client, server));

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK_FALSE(static_cast<bool>(client->GetCurPlayer()));
    CHECK(GetServerConnectionCount(server) == 0);

    REQUIRE(client->CallFunc(get_client_func_name("ClientServerIntegrationClient::UnitTestGetDisconnectedCalls"), disconnected_calls));
    CHECK(disconnected_calls >= 1);
}

TEST_CASE("AngelScriptClientLogsInThroughARemoteCall")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    // The connected-but-not-logged-in session only accepts a remote call, which is how a real client logs in
    REQUIRE(client->CallFunc<void>(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestSendLogin")));

    int32_t login_success_calls = 0;
    bool logged_in = false;

    for (int32_t i = 0; i < 2000 && !logged_in; i++) {
        client->MainLoop();

        REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestGetLoginSuccessCalls"), login_success_calls));
        logged_in = login_success_calls >= 1;

        if (!logged_in) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(logged_in);
    CHECK(login_success_calls >= 1);

    int32_t server_login_calls = 0;
    REQUIRE(server->CallFunc(server->Hashes.to_hashed_string("ClientServerIntegrationServer::UnitTestGetLoginCalls"), server_login_calls));
    CHECK(server_login_calls == 1);

    CHECK(client->IsConnected());
    REQUIRE(static_cast<bool>(client->GetCurPlayer()));

    // The login reply leaves the server before the remote call reaches SwitchCritter, so the client can see success first
    int32_t switched_critters = 0;

    for (int32_t i = 0; i < 2000 && switched_critters == 0; i++) {
        REQUIRE(server->CallFunc(server->Hashes.to_hashed_string("ClientServerIntegrationServer::UnitTestGetSwitchedCritters"), switched_critters));

        if (switched_critters == 0) {
            client->MainLoop();
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(switched_critters == 1);

    // The controlled critter arrives over the wire, so the client ends up with a chosen critter of its own
    bool has_chosen = false;

    for (int32_t i = 0; i < 2000 && !has_chosen; i++) {
        client->MainLoop();
        has_chosen = static_cast<bool>(client->GetChosen());

        if (!has_chosen) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(has_chosen);

    // Player commands round-trip: the client sends move and direction, the server validates and applies them
    int32_t drive_result = -1;

    for (int32_t i = 0; i < 2000 && drive_result != 0; i++) {
        client->MainLoop();
        REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestDriveChosen"), drive_result));

        if (drive_result != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(drive_result == 0);

    for (int32_t i = 0; i < 200; i++) {
        client->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    int32_t npc_critters = 0;
    REQUIRE(server->CallFunc(server->Hashes.to_hashed_string("ClientServerIntegrationServer::UnitTestGetNpcCritters"), npc_critters));
    CHECK(npc_critters == 1);

    int32_t write_result = -1;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestWriteChosenProperty"), write_result));
    CHECK(write_result == 0);

    for (int32_t i = 0; i < 100; i++) {
        client->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    // Drive the server-side world steps through the second remote call, so each one runs with proper cover
    for (int32_t step = 0; step < 6; step++) {
        REQUIRE(client->CallFunc<void, int32_t>(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestSendWorldStep"), step));

        for (int32_t i = 0; i < 100; i++) {
            client->MainLoop();
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    int32_t client_pings = 0;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestGetClientPings"), client_pings));
    CHECK(client_pings == 42);

    // The login handler pushes the controlled critter's inventory down as a separate item payload, and
    // those views must not present themselves as items lying on a map hex
    int32_t received_items = 0;
    int32_t received_map_owned_items = -1;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestGetReceivedItemCount"), received_items));
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestGetReceivedMapOwnedItemCount"), received_map_owned_items));
    CHECK(received_items > 0);
    CHECK(received_map_owned_items == 0);

    int32_t action_context_items = 0;
    int32_t action_map_owned_context_items = -1;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestGetActionContextItemCount"), action_context_items));
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestGetActionMapOwnedContextItemCount"), action_map_owned_context_items));
    CHECK(action_context_items > 0);
    CHECK(action_map_owned_context_items == 0);

    // Movement and property writes from the client side enter the server through their own message
    // handlers, which nothing else in the suite reaches
    int32_t movement_result = -1;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestDriveChosenMovement"), movement_result));
    CHECK(movement_result == 0);

    for (int32_t i = 0; i < 200; i++) {
        client->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    // Every wire-representable argument shape, sent to the server and echoed back
    int32_t every_arg_result = -1;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestSendEveryArg"), every_arg_result));
    CHECK(every_arg_result == 0);

    int32_t every_arg_echoes = 0;

    for (int32_t i = 0; i < 500 && every_arg_echoes == 0; i++) {
        client->MainLoop();
        REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestGetEveryArgEchoes"), every_arg_echoes));

        if (every_arg_echoes == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(every_arg_echoes == 1);

    int32_t every_arg_echo_mismatch = -1;
    REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestGetEveryArgEchoMismatch"), every_arg_echo_mismatch));
    CHECK(every_arg_echo_mismatch == 0);

    int32_t every_arg_calls = 0;
    REQUIRE(server->CallFunc(server->Hashes.to_hashed_string("ClientServerIntegrationServer::UnitTestGetEveryArgCalls"), every_arg_calls));
    CHECK(every_arg_calls == 1);

    int32_t every_arg_mismatch = -1;
    REQUIRE(server->CallFunc(server->Hashes.to_hashed_string("ClientServerIntegrationServer::UnitTestGetEveryArgMismatch"), every_arg_mismatch));
    CHECK(every_arg_mismatch == 0);

    int32_t world_steps = 0;
    REQUIRE(server->CallFunc(server->Hashes.to_hashed_string("ClientServerIntegrationServer::UnitTestGetWorldSteps"), world_steps));
    CHECK(world_steps == 6);

    // The world arrives asynchronously and the instrumented build is much slower, so the inspection polls
    // instead of asserting on the first attempt
    int32_t inspect_result = -1;

    for (int32_t i = 0; i < 1000 && inspect_result != 0; i++) {
        client->MainLoop();
        REQUIRE(client->CallFunc(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestInspectChosenWorld"), inspect_result));

        if (inspect_result != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(inspect_result == 0);

    // Fog of war is a client-only path: the mapper's map view returns from PrepareFogToDraw before it
    // starts, so the shapes and their draw slots are only reachable from a session map
    {
        auto session_map = client->GetCurMap();
        REQUIRE(session_map);
        ptr<MapView> map_view = session_map.as_ptr();

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

        // A hex-anchored shape and one that follows the chosen critter are the two ways a fog layer is made
        REQUIRE_NOTHROW(ignore_unused(map_view->AddFog(mpos {8, 8}, DrawOrderType::Light, nullptr).get()));
        REQUIRE_NOTHROW(ignore_unused(map_view->AddFog(client->GetChosen(), DrawOrderType::Light, nullptr).get()));
        REQUIRE_NOTHROW(map_view->RebuildFog());

        for (int32_t frame = 0; frame < 4; frame++) {
            ImGui::NewFrame();
            REQUIRE_NOTHROW(map_view->Process());
            REQUIRE_NOTHROW(map_view->DrawMap());
            ImGui::Render();
        }

        // Scrolling walks hexes in and out of the view field, which is what runs the per-hex show/hide
        // bookkeeping over real content instead of an empty viewport
        for (const mpos& center : {mpos {4, 4}, mpos {12, 12}, mpos {8, 4}, mpos {10, 10}}) {
            REQUIRE_NOTHROW(map_view->InstantScrollTo(center));

            ImGui::NewFrame();
            REQUIRE_NOTHROW(map_view->Process());
            REQUIRE_NOTHROW(map_view->DrawMap());
            ImGui::Render();
        }

        // A smooth scroll advances over frames rather than snapping, and zoom rebuilds the view field
        REQUIRE_NOTHROW(map_view->ScrollToHex(mpos {6, 6}, ipos16 {0, 0}, 20, true));

        for (float32_t zoom : {1.5f, 0.75f, 1.0f}) {
            REQUIRE_NOTHROW(map_view->ChangeZoom(zoom, fpos32 {0.5f, 0.5f}));

            ImGui::NewFrame();
            REQUIRE_NOTHROW(map_view->Process());
            REQUIRE_NOTHROW(map_view->DrawMap());
            ImGui::Render();
        }

        // A sub-layer set on a server item reaches the client as its offset does, and a value that arrives after
        // the item is drawn moves its sprite: the item world step 0 drops beside the login's one comes out after it
        nptr<ItemHexView> login_item;
        nptr<ItemHexView> dropped_item;

        for (ptr<ItemHexView> hex_item : map_view->GetItemsOnHex(mpos {10, 10})) {
            if (hex_item->GetDrawOrderSubLayer() == 3) {
                login_item = hex_item;
            }
            else if (hex_item->GetDrawOrderSubLayer() == 5) {
                dropped_item = hex_item;
            }
        }

        REQUIRE(login_item);
        REQUIRE(dropped_item);
        REQUIRE_NOTHROW(map_view->InstantScrollTo(mpos {10, 10}));

        ImGui::NewFrame();
        REQUIRE_NOTHROW(map_view->Process());
        REQUIRE_NOTHROW(map_view->DrawMap());
        ImGui::Render();

        REQUIRE(login_item->IsMapSpriteVisible());
        REQUIRE(dropped_item->IsMapSpriteVisible());
        CHECK(dropped_item->GetMapSprite()->GetSortValue() > login_item->GetMapSprite()->GetSortValue());

        // A scroll rebuilds every sprite from current values, so the new value is also written where no rebuild
        // follows: the sprite has to move by itself
        dropped_item->SetDrawOrderSubLayer(1);

        ImGui::NewFrame();
        REQUIRE_NOTHROW(map_view->Process());
        REQUIRE_NOTHROW(map_view->DrawMap());
        ImGui::Render();

        REQUIRE(dropped_item->IsMapSpriteVisible());
        CHECK(dropped_item->GetMapSprite()->GetSortValue() < login_item->GetMapSprite()->GetSortValue());
    }

    // A static item the server drops from this map instance has to leave the client's map view without a reload
    {
        auto session_map = client->GetCurMap();
        REQUIRE(session_map);
        ptr<MapView> map_view = session_map.as_ptr();

        REQUIRE(static_cast<bool>(map_view->GetItem(STATIC_ITEM_ID)));

        auto drive_world_step = [&client](int32_t step) {
            REQUIRE(client->CallFunc<void, int32_t>(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestSendWorldStep"), step));

            for (int32_t i = 0; i < 200; i++) {
                client->MainLoop();
                std::this_thread::sleep_for(std::chrono::milliseconds {2});
            }
        };

        drive_world_step(6);
        CHECK_FALSE(static_cast<bool>(map_view->GetItem(STATIC_ITEM_ID)));
        CHECK(map_view->GetItemsOnHex(STATIC_ITEM_HEX).empty());

        // Removal is one-way: nothing the server can say afterwards puts it back on this loaded map
        drive_world_step(6);
        CHECK_FALSE(static_cast<bool>(map_view->GetItem(STATIC_ITEM_ID)));
        CHECK(map_view->GetItemsOnHex(STATIC_ITEM_HEX).empty());
    }

    // The other half of the contract: a client that loads the map while the item is already removed must
    // never build it into the view, rather than building it and taking it out again
    {
        auto map_proto = client->GetProtoMap(client->Hashes.to_hashed_string("UnitTestSharedMap"));
        REQUIRE(map_proto);

        isize32 screen_size = client->SprMngr.GetScreenSize();

        auto load_view = [&](bool with_removal) {
            auto map_view = safe_alloc::make_refcounted<MapView>(client.as_ptr(), ident_t {9001}, map_proto.as_ptr(), screen_size);
            auto destroy_view = scope_exit([&map_view]() noexcept { safe_call([&map_view] { map_view->DestroySelf(); }); });

            if (with_removal) {
                map_view->SetRemovedStaticItemIds(vector<ident_t> {STATIC_ITEM_ID});
            }

            REQUIRE_NOTHROW(map_view->LoadStaticData());
            return static_cast<bool>(map_view->GetItem(STATIC_ITEM_ID));
        };

        // Without the removal the same load does produce the view, so the check above is not vacuous
        CHECK(load_view(false));
        CHECK_FALSE(load_view(true));
    }

    // The server diagnostic panels only have real rows to render once a world exists, which is exactly the
    // state this session leaves behind: a logged-in player, a location with a map, critters and items
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

        // Collapsing headers opt out of the log auto-expansion, so the root panels are seeded open by hand;
        // otherwise the per-map, per-critter and per-item rows never render even with a populated world
        constexpr std::array ROOT_PANEL_IDS = {"Info", "Performance details", "###Players", "###NotLoggedInPlayers", "###Locations", "Data base"};

        for (int32_t frame = 0; frame < 3; frame++) {
            ImGui::NewFrame();

            if (ImGui::Begin("ServerDiagnostics")) {
                ptr<ImGuiWindow> window = ImGui::GetCurrentWindow();

                for (string_view panel_id : ROOT_PANEL_IDS) {
                    window->StateStorage.SetInt(ImGui::GetID(panel_id.data(), panel_id.data() + panel_id.size()), 1);
                }

                ImGui::LogToBuffer(12);
                REQUIRE_NOTHROW(server->DrawGui());
                ImGui::LogFinish();
            }

            ImGui::End();
            ImGui::Render();
        }
    }

    client->Disconnect();
    REQUIRE(WaitForDisconnected(client, server));
}

TEST_CASE("AngelScriptTwoClientsShareOneMapSession")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    auto first_settings = MakeClientTestSettings(port);
    auto second_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto first = MakeClientEngine(first_settings);
    auto second = MakeClientEngine(second_settings);

    auto shutdown = scope_exit([&server, &first, &second]() noexcept {
        safe_call([&second] { second->Shutdown(); });
        safe_call([&first] { first->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    auto login = [&server](ptr<ClientEngine> client, size_t expected_connections) {
        client->Connect();
        REQUIRE(WaitForConnected(client, server, expected_connections));
        REQUIRE(client->CallFunc<void>(client->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestSendLogin")));

        for (int32_t i = 0; i < 2000; i++) {
            client->MainLoop();

            if (client->GetChosen()) {
                return true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }

        return false;
    };

    REQUIRE(login(first.as_ptr(), 1));
    REQUIRE(login(second.as_ptr(), 2));

    // Both sessions land on their own map instance, so each still sees exactly one player critter of its
    // own; what this drives is the server fanning world state out to two independent player views at once
    for (int32_t i = 0; i < 300; i++) {
        first->MainLoop();
        second->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    int32_t first_result = -1;
    int32_t second_result = -1;

    for (int32_t i = 0; i < 1000 && (first_result != 0 || second_result != 0); i++) {
        first->MainLoop();
        second->MainLoop();
        REQUIRE(first->CallFunc(first->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestInspectChosenWorld"), first_result));
        REQUIRE(second->CallFunc(second->Hashes.to_hashed_string("ClientServerIntegrationClient::UnitTestInspectChosenWorld"), second_result));

        if (first_result != 0 || second_result != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {2});
        }
    }

    CHECK(first_result == 0);
    CHECK(second_result == 0);

    // Dropping one session must not disturb the other
    first->Disconnect();
    REQUIRE(WaitForServerConnectionCount(server, 1));

    for (int32_t i = 0; i < 100; i++) {
        second->MainLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds {2});
    }

    CHECK(second->IsConnected());
    CHECK(static_cast<bool>(second->GetChosen()));

    second->Disconnect();
    REQUIRE(WaitForServerConnectionCount(server, 0));
}

TEST_CASE("AngelScriptClientReportsLazyUnresolvedHashAndLearnsWithoutDisconnect")
{
    using namespace TestClientServerIntegration;

    auto port = IntegrationTestPort.fetch_add(1);

    auto server_settings = MakeServerTestSettings(port);
    // Linux debug stack traces for the expected script exception below can outlive the default ping window
    BakerTests::OverrideSetting(server_settings.ServerNetwork.ClientPingTime, 120000);
    auto client_settings = MakeClientTestSettings(port);

    auto server = MakeServerEngine(server_settings);
    auto client = MakeClientEngine(client_settings);

    auto shutdown = scope_exit([&server, &client]() noexcept {
        safe_call([&client] { client->Shutdown(); });

        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForServerStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    client->Connect();
    REQUIRE(WaitForConnected(client, server));

    // A server-only runtime hstring that is not read through NetInBuffer, matching lazy property/script resolves
    hstring reported = server->Hashes.to_hashed_string("integration_test_lazy_hash");

    auto critter_registrar = client->GetPropertyRegistrar(CritterView::ENTITY_TYPE_NAME);
    REQUIRE(static_cast<bool>(critter_registrar));

    auto critter_props = Properties(critter_registrar);
    auto model_name_prop = critter_registrar->GetPropertyByIndex(CritterView::ModelName_RegIndex);
    REQUIRE(static_cast<bool>(model_name_prop));

    hstring::hash_t unresolved_hash = reported.as_hash();
    critter_props.SetRawData(model_name_prop, {reinterpret_cast<const uint8_t*>(&unresolved_hash), sizeof(unresolved_hash)});

    auto proto = client->GetProtoCritter(client->Hashes.to_hashed_string("UnitTestSharedCritter"));
    REQUIRE(static_cast<bool>(proto));

    auto critter_props_ptr = make_nptr(&critter_props);
    auto critter = safe_alloc::make_refcounted<CritterView>(client, ident_t {}, proto, critter_props_ptr);
    auto get_client_func_name = [&client](string_view name) { return client->Hashes.to_hashed_string(name); };

    // Trigger the same client unresolved-hash reporter without forcing a slow script exception
    bool failed = false;
    hstring unresolved = client->Hashes.resolve_hash(reported.as_hash(), &failed);
    CHECK(failed);
    CHECK_FALSE(static_cast<bool>(unresolved));

    REQUIRE(WaitForLearnedHash(client, reported.as_hash(), "integration_test_lazy_hash"));
    CHECK(client->IsConnected());
    CHECK(GetServerConnectionCount(server) == 1);

    string model_name;
    REQUIRE(client->CallFunc<string, ptr<CritterView>>(get_client_func_name("ClientServerIntegrationClient::UnitTestReadCritterModelName"), critter, model_name));
    CHECK(model_name == "integration_test_lazy_hash");
}

FO_END_NAMESPACE

#endif

