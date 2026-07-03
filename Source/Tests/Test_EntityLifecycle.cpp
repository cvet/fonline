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

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "NetBuffer.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    class TestNetworkConnection final : public NetworkServerConnection
    {
    public:
        explicit TestNetworkConnection(ptr<ServerNetworkSettings> settings) :
            NetworkServerConnection(settings)
        {
            FO_STACK_TRACE_ENTRY();

            _host = "Test";
        }
        TestNetworkConnection(const TestNetworkConnection&) = delete;
        TestNetworkConnection(TestNetworkConnection&&) noexcept = delete;
        auto operator=(const TestNetworkConnection&) = delete;
        auto operator=(TestNetworkConnection&&) noexcept = delete;
        ~TestNetworkConnection() override = default;

        void Receive(const_span<uint8_t> buf)
        {
            FO_STACK_TRACE_ENTRY();

            ReceiveCallback(buf);
        }

    protected:
        void DispatchImpl() override { FO_NO_STACK_TRACE_ENTRY(); }

        void DisconnectImpl() override { FO_NO_STACK_TRACE_ENTRY(); }
    };

    static auto MakeSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "EntityLifecycleScripts",
            {
                {"Scripts/EntityLifecycle.fos", R"(
namespace EntityLifecycle
{
    int ItemInitCalls = 0;
    int CritterInitCalls = 0;
    int LocationInitCalls = 0;
    int ItemFinishCalls = 0;
    int CritterFinishCalls = 0;
    int LocationFinishCalls = 0;
    int CritterLoadCalls = 0;
    int CritterUnloadCalls = 0;
    int CritterUnloadSawOnMapCalls = 0;
    int DestroyOnInitMode = 0;
    int DestroyOnFinishMode = 0;
    int CritterLifecycleEventMode = 0;
    int PlayerEventMode = 0;
    int PlayerLoginCalls = 0;
    int PlayerLogoutCalls = 0;
    int PlayerDestroyedAfterLoginDisconnectCalls = 0;
    int PlayerDirCritterCalls = 0;
    int CritterSendInitialInfoCalls = 0;
    int PlayerCritterSwitchedCalls = 0;

    [[ModuleInit]]
    void InitEntityLifecycle()
    {
        Game.OnInit.Subscribe(OnInit);
        Game.OnItemInit.Subscribe(OnItemInit);
        Game.OnCritterInit.Subscribe(OnCritterInit);
        Game.OnLocationInit.Subscribe(OnLocationInit);
        Game.OnItemFinish.Subscribe(OnItemFinish);
        Game.OnCritterFinish.Subscribe(OnCritterFinish);
        Game.OnLocationFinish.Subscribe(OnLocationFinish);
        Game.OnCritterLoad.Subscribe(OnCritterLoad);
        Game.OnCritterUnload.Subscribe(OnCritterUnload);
        Game.OnPlayerLogin.Subscribe(OnPlayerLogin);
        Game.OnPlayerLogout.Subscribe(OnPlayerLogout);
        Game.OnPlayerDirCritter.Subscribe(OnPlayerDirCritter);
        Game.OnCritterSendInitialInfo.Subscribe(OnCritterSendInitialInfo);
        Game.OnPlayerCritterSwitched.Subscribe(OnPlayerCritterSwitched);
    }

    [[Event]]
    void OnInit()
    {
    }

    [[Event]]
    void OnItemInit(Item item, bool firstTime)
    {
        ItemInitCalls++;

        if (DestroyOnInitMode == 1) {
            Game.DestroyItem(item);
        }
    }

    [[Event]]
    void OnCritterInit(Critter cr, bool firstTime)
    {
        CritterInitCalls++;

        if (DestroyOnInitMode == 2) {
            Game.DestroyCritter(cr);
        }
    }

    [[Event]]
    void OnLocationInit(Location loc, bool firstTime)
    {
        LocationInitCalls++;

        if (DestroyOnInitMode == 3) {
            Game.DestroyLocation(loc);
        }
    }

    int GetItemInitCalls() { return ItemInitCalls; }
    int GetCritterInitCalls() { return CritterInitCalls; }
    int GetLocationInitCalls() { return LocationInitCalls; }
    int GetItemFinishCalls() { return ItemFinishCalls; }
    int GetCritterFinishCalls() { return CritterFinishCalls; }
    int GetLocationFinishCalls() { return LocationFinishCalls; }
    int GetCritterLoadCalls() { return CritterLoadCalls; }
    int GetCritterUnloadCalls() { return CritterUnloadCalls; }
    int GetCritterUnloadSawOnMapCalls() { return CritterUnloadSawOnMapCalls; }
    int GetPlayerLoginCalls() { return PlayerLoginCalls; }
    int GetPlayerLogoutCalls() { return PlayerLogoutCalls; }
    int GetPlayerDestroyedAfterLoginDisconnectCalls() { return PlayerDestroyedAfterLoginDisconnectCalls; }
    int GetPlayerDirCritterCalls() { return PlayerDirCritterCalls; }
    int GetCritterSendInitialInfoCalls() { return CritterSendInitialInfoCalls; }
    int GetPlayerCritterSwitchedCalls() { return PlayerCritterSwitchedCalls; }

    void SetDestroyOnInitMode(int mode)
    {
        DestroyOnInitMode = mode;
    }

    void SetDestroyOnFinishMode(int mode)
    {
        DestroyOnFinishMode = mode;
    }

    void SetCritterLifecycleEventMode(int mode)
    {
        CritterLifecycleEventMode = mode;
    }

    void SetPlayerEventMode(int mode)
    {
        PlayerEventMode = mode;
    }

    void ResetCounters()
    {
        ItemInitCalls = 0;
        CritterInitCalls = 0;
        LocationInitCalls = 0;
        ItemFinishCalls = 0;
        CritterFinishCalls = 0;
        LocationFinishCalls = 0;
        CritterLoadCalls = 0;
        CritterUnloadCalls = 0;
        CritterUnloadSawOnMapCalls = 0;
        DestroyOnInitMode = 0;
        DestroyOnFinishMode = 0;
        CritterLifecycleEventMode = 0;
        PlayerEventMode = 0;
        PlayerLoginCalls = 0;
        PlayerLogoutCalls = 0;
        PlayerDestroyedAfterLoginDisconnectCalls = 0;
        PlayerDirCritterCalls = 0;
        CritterSendInitialInfoCalls = 0;
        PlayerCritterSwitchedCalls = 0;
    }

    [[Event]]
    void OnItemFinish(Item item)
    {
        ItemFinishCalls++;

        if (DestroyOnFinishMode == 1) {
            Game.DestroyItem(item);
        }
    }

    [[Event]]
    void OnCritterFinish(Critter cr)
    {
        CritterFinishCalls++;

        if (DestroyOnFinishMode == 2) {
            Game.DestroyCritter(cr);
        }
    }

    [[Event]]
    void OnLocationFinish(Location loc)
    {
        LocationFinishCalls++;

        if (DestroyOnFinishMode == 3) {
            Game.DestroyLocation(loc);
        }
    }

    [[Event]]
    void OnCritterLoad(Critter cr)
    {
        CritterLoadCalls++;

        if (CritterLifecycleEventMode == 2) {
            cr.MakeControllable(false);
            Game.DestroyCritter(cr);
        }
    }

    [[Event]]
    void OnCritterUnload(Critter cr)
    {
        CritterUnloadCalls++;

        if (CritterLifecycleEventMode == 1) {
            if (cr.MapId != ZERO_IDENT) {
                CritterUnloadSawOnMapCalls++;
                cr.TransferToGlobal();
            }
        }
    }

    [[Event]]
    EventResult OnPlayerLogin(Player player, Player? unloginedPlayer)
    {
        PlayerLoginCalls++;

        if (PlayerEventMode == 1) {
            player.HardDisconnect();

            if (player.IsDestroyed) {
                PlayerDestroyedAfterLoginDisconnectCalls++;
            }
        }

        return EventResult::ContinueChain;
    }

    [[Event]]
    void OnPlayerLogout(Player player)
    {
        PlayerLogoutCalls++;
    }

    [[Event]]
    EventResult OnPlayerDirCritter(Player player, Critter cr, mdir& dir)
    {
        PlayerDirCritterCalls++;

        if (PlayerEventMode == 4) {
            player.SwitchCritter(null);
        }

        return EventResult::ContinueChain;
    }

    [[Event]]
    void OnCritterSendInitialInfo(Critter cr)
    {
        CritterSendInitialInfoCalls++;

        if (PlayerEventMode == 3) {
            Player? player = cr.GetPlayer();

            if (player !is null) {
                player.SwitchCritter(null);
            }
        }
    }

    [[Event]]
    void OnPlayerCritterSwitched(Player player, Critter cr, Critter prevCr)
    {
        PlayerCritterSwitchedCalls++;
    }
}
)"},
            },
            [](string_view message) {
                const auto message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    static auto MakeEmptyMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        auto writer = DataWriter(map_data);
        writer.Write<uint32_t>(uint32_t {0}); // hashes_count
        writer.Write<uint32_t>(uint32_t {0}); // cr_count
        writer.Write<uint32_t>(uint32_t {0}); // item_count
        return map_data;
    }

    static auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrator = proto_engine.GetPropertyRegistrator(type_name);
        REQUIRE(static_cast<bool>(registrator));

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), registrator.as_ptr()};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WriteStringBytes(type_name.as_str());
        writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.WriteStringBytes(proto_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        if (!props_data.empty()) {
            writer.WriteBytes({props_data.data(), props_data.size()});
        }

        return protos_data;
    }

    static auto MakeResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("EntityLifecycleCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto item_type = proto_engine.Hashes.ToHashedString("Item");
        const auto location_type = proto_engine.Hashes.ToHashedString("Location");
        const auto map_type = proto_engine.Hashes.ToHashedString("Map");
        const auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        const auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        const auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        const auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "TestMap", msize {200, 200});
        const auto fomap_blob = MakeEmptyMapBlob();
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("EntityLifecycleRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("EntityLifecycleCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("EntityLifecycleItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("EntityLifecycleLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("TestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("TestMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile("EntityLifecycle.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForStart(ptr<ServerEngine> server) -> string
    {
        for (int32_t i = 0; i < 6000; i++) {
            if (server->IsStarted()) {
                return {};
            }
            if (server->IsStartingError()) {
                return "ServerEngine startup failed";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }

        return "ServerEngine startup timed out";
    }

    static auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeResources());
    }

    static auto CreateLoggedPlayer(ptr<ServerEngine> server, shared_ptr<NetworkServerConnection> net_connection, string_view name) -> ptr<Player>;

    static auto CreateLoggedPlayer(ptr<ServerEngine> server, string_view name) -> ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        return CreateLoggedPlayer(server, std::move(net_connection), name);
    }

    static auto CreateLoggedPlayer(ptr<ServerEngine> server, shared_ptr<NetworkServerConnection> net_connection, string_view name) -> ptr<Player>
    {
        FO_VERIFY_AND_THROW(net_connection, "Missing required net connection");

        auto unlogined_player = server->CreateUnloginedPlayer(std::move(net_connection));

        unlogined_player->SetName(name);
        unlogined_player->SetLastControlledCritterId(ident_t {1});
        auto nullable_player = server->LoginPlayerToNewRecord(unlogined_player);
        FO_VERIFY_AND_THROW(!!nullable_player, "Player login to new record failed");

        return nullable_player.as_ptr();
    }

    static void SendStopCritterMove(ptr<TestNetworkConnection> connection, ptr<ServerEngine> server, ident_t map_id, ident_t cr_id, mpos client_hex, ipos16 client_hex_offset, mdir client_dir)
    {
        FO_STACK_TRACE_ENTRY();

        NetOutBuffer packet {numeric_cast<size_t>(server->Settings->NetBufferSize)};
        packet.StartMsg(NetMessage::SendStopCritterMove);
        packet.Write(map_id);
        packet.Write(cr_id);
        packet.Write(client_hex);
        packet.Write(client_hex_offset);
        packet.Write(client_dir);
        packet.EndMsg();

        connection->Receive(packet.GetData());
    }

    static auto WaitForUnlockedServerCondition(ptr<ServerEngine> server, bool& locked, const function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        FO_VERIFY_AND_THROW(locked, "Server must be locked before waiting on condition");

        server->Unlock();
        locked = false;

        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds {5});

            if (!server->Lock(timespan {std::chrono::seconds {10}})) {
                continue;
            }

            locked = true;

            if (condition()) {
                return true;
            }

            server->Unlock();
            locked = false;
        }

        if (!locked) {
            if (!server->Lock(timespan {std::chrono::seconds {10}})) {
                return false;
            }

            locked = true;
        }

        return condition();
    }
}

// ========== Entity Init Event Tests ==========

TEST_CASE("EntityInitEvents")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("CritterInitEventFires")
    {
        // Reset counters
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        // Create critter
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        // Check critter init event fired
        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterInitCalls"), calls));
        CHECK(calls >= 1);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("ItemInitEventFires")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemInitCalls"), calls));
        CHECK(calls >= 1);

        server->ItemMngr.DestroyItem(item);
    }

    SECTION("LocationInitEventFires")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"));

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetLocationInitCalls"), calls));
        CHECK(calls >= 1);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("ItemInitEventMayDestroyItem")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnInitMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(1));

        const auto initial_item_count = server->EntityMngr.GetItemsCount();
        REQUIRE_THROWS_AS(server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr), ItemManagerException);
        CHECK(server->EntityMngr.GetItemsCount() == initial_item_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemInitCalls"), calls));
        CHECK(calls >= 1);
    }

    SECTION("CritterInitEventMayDestroyCritter")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnInitMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(2));

        const auto initial_critter_count = server->EntityMngr.GetCrittersCount();
        REQUIRE_THROWS_AS(server->CreateCritter(fn("TestCritter"), false), GenericException);
        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterInitCalls"), calls));
        CHECK(calls >= 1);
    }

    SECTION("LocationInitEventMayDestroyLocation")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnInitMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(3));

        const auto initial_location_count = server->EntityMngr.GetLocationsCount();
        REQUIRE_THROWS_AS(server->MapMngr.CreateLocation(fn("TestLocation")), GenericException);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_location_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetLocationInitCalls"), calls));
        CHECK(calls >= 1);
    }

    SECTION("ItemFinishEventCannotTakeOverItemDestruction")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnFinishMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(1));

        const auto initial_item_count = server->EntityMngr.GetItemsCount();
        auto item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr);
        const ident_t item_id = item->GetId();

        server->ItemMngr.DestroyItem(item);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetItem(item_id)));
        CHECK(server->EntityMngr.GetItemsCount() == initial_item_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemFinishCalls"), calls));
        CHECK(calls == 1);
    }

    SECTION("CritterFinishEventCannotTakeOverCritterDestruction")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnFinishMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(2));

        const auto initial_critter_count = server->EntityMngr.GetCrittersCount();
        auto cr = server->CreateCritter(fn("TestCritter"), false);
        const ident_t cr_id = cr->GetId();

        server->CrMngr.DestroyCritter(cr);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterFinishCalls"), calls));
        CHECK(calls == 1);
    }

    SECTION("LocationFinishEventCannotTakeOverLocationDestruction")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetDestroyOnFinishMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(3));

        const auto initial_location_count = server->EntityMngr.GetLocationsCount();
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"));
        const ident_t loc_id = loc->GetId();

        server->MapMngr.DestroyLocation(loc);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetLocation(loc_id)));
        CHECK(server->EntityMngr.GetLocationsCount() == initial_location_count);

        int32_t calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetLocationFinishCalls"), calls));
        CHECK(calls == 1);
    }

    SECTION("CritterUnloadEventMayTransferCritterBeforeTeardown")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});

        auto nullable_map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(nullable_map));
        auto map = nullable_map.as_ptr();

        auto cr = server->CreateCritter(fn("TestCritter"), true);

        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        REQUIRE(cr->GetMapId() == map->GetId());
        REQUIRE(map->GetCritter(cr->GetId()) == cr.get());

        const ident_t cr_id = cr->GetId();

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetCritterLifecycleEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(1));

        server->UnloadCritter(cr);

        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));
        CHECK_FALSE(static_cast<bool>(map->GetCritter(cr_id)));

        int32_t unload_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterUnloadCalls"), unload_calls));
        CHECK(unload_calls == 1);

        int32_t saw_on_map_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterUnloadSawOnMapCalls"), saw_on_map_calls));
        CHECK(saw_on_map_calls == 1);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("CritterLoadEventMayDestroyLoadedCritterThrows")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto cr = server->CreateCritter(fn("TestCritter"), true);

        server->EntityMngr.MakePersistent(cr, true, true);
        const ident_t cr_id = cr->GetId();

        server->UnloadCritter(cr);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetCritterLifecycleEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(2));

        REQUIRE_THROWS_AS(server->LoadCritter(cr_id, true), GenericException);
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(cr_id)));

        int32_t load_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterLoadCalls"), load_calls));
        CHECK(load_calls == 1);
    }
}

// ========== Entity Manager C++ API Tests ==========

TEST_CASE("EntityManagerCppApi")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("GetEntitiesReturnsCorrectCollections")
    {
        auto initial_critter_count = server->EntityMngr.GetCrittersCount();

        auto cr = server->CreateCritter(fn("TestCritter"), false);

        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count + 1);

        auto after_critter_item_count = server->EntityMngr.GetItemsCount();

        auto item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr);

        CHECK(server->EntityMngr.GetItemsCount() == after_critter_item_count + 1);

        server->ItemMngr.DestroyItem(item);
        CHECK(server->EntityMngr.GetItemsCount() == after_critter_item_count);

        server->CrMngr.DestroyCritter(cr);
        CHECK(server->EntityMngr.GetCrittersCount() == initial_critter_count);
    }

    SECTION("GetEntityReturnsNullForInvalidId")
    {
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetEntity(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetCritter(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetItem(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetLocation(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetMap(ident_t {999999})));
        CHECK_FALSE(static_cast<bool>(server->EntityMngr.GetPlayer(ident_t {999999})));
    }

    SECTION("GetEntityFindsCreatedCritter")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        const auto cr_id = cr->GetId();
        auto nullable_found_cr = server->EntityMngr.GetCritter(cr_id);
        REQUIRE(nullable_found_cr);
        auto found_cr = nullable_found_cr.as_ptr();
        CHECK(found_cr == cr);

        auto nullable_found_entity = server->EntityMngr.GetEntity(cr_id);
        REQUIRE(nullable_found_entity);
        auto found_entity = nullable_found_entity.as_ptr();
        ptr<ServerEntity> cr_entity = cr;
        CHECK(found_entity == cr_entity);

        auto typed_found = server->EntityMngr.Get<Critter>(cr_id);
        REQUIRE(typed_found);
        CHECK(typed_found.as_ptr() == cr);

        const EntityManager& const_entity_mngr = server->EntityMngr;
        auto const_typed_found = const_entity_mngr.Get<Critter>(cr_id);
        REQUIRE(const_typed_found);
        ptr<const Critter> const_cr = cr;
        CHECK(const_typed_found.as_ptr() == const_cr);

        auto wrong_type = server->EntityMngr.Get<Item>(cr_id);
        CHECK_FALSE(static_cast<bool>(wrong_type));

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("GetEntityFindsCreatedItem")
    {
        auto item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr);

        const auto item_id = item->GetId();
        auto found = server->EntityMngr.GetItem(item_id);
        REQUIRE(found);
        CHECK(nptr<Item> {found} == item);

        server->ItemMngr.DestroyItem(item);
    }

    SECTION("GetEntityFindsCreatedLocation")
    {
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"));

        const auto loc_id = loc->GetId();
        auto found = server->EntityMngr.GetLocation(loc_id);
        REQUIRE(found);
        CHECK(nptr<Location> {found} == loc);

        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("LocationCount")
    {
        auto initial_count = server->EntityMngr.GetLocationsCount();

        auto loc1 = server->MapMngr.CreateLocation(fn("TestLocation"));
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 1);

        auto loc2 = server->MapMngr.CreateLocation(fn("TestLocation"));
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 2);

        server->MapMngr.DestroyLocation(loc1);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count + 1);

        server->MapMngr.DestroyLocation(loc2);
        CHECK(server->EntityMngr.GetLocationsCount() == initial_count);
    }

    SECTION("PlayersCountIsZero")
    {
        CHECK(server->EntityMngr.GetPlayersCount() == 0);
    }

    SECTION("MapsCountIsZero")
    {
        CHECK(server->EntityMngr.GetMapsCount() == 0);
    }
}

// ========== Critter C++ API Tests ==========

TEST_CASE("CritterCppApi")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("CritterStateChecks")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        CHECK(cr->IsAlive());
        CHECK_FALSE(cr->IsDead());
        CHECK_FALSE(cr->IsKnockout());
        CHECK_FALSE(cr->IsMoving());
        CHECK_FALSE(cr->HasPlayer());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterIdentity")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        CHECK(cr->GetId() != ident_t {});
        CHECK(cr->GetProtoId() == fn("TestCritter"));
        CHECK_FALSE(cr->IsDestroyed());
        CHECK_FALSE(cr->IsDestroying());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("CritterPlayerFlag")
    {
        auto npc = server->CreateCritter(fn("TestCritter"), false);

        CHECK_FALSE(npc->GetControlledByPlayer());

        server->CrMngr.DestroyCritter(npc);
    }

    SECTION("CritterInventoryOperations")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        CHECK_FALSE(cr->HasItems());

        auto item1 = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        REQUIRE(static_cast<bool>(item1));
        CHECK(cr->HasItems());

        auto item2 = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        REQUIRE(static_cast<bool>(item2));

        vector<ptr<Item>> inv_items = cr->GetInvItems();
        CHECK(inv_items.size() >= 2);

        // Find by pid
        auto found = cr->GetInvItemByPid(fn("TestItem"));
        CHECK(static_cast<bool>(found));

        // Count by pid
        CHECK(cr->CountInvItemByPid(fn("TestItem")) >= 2);

        // Find by id
        auto found_by_id = cr->GetInvItem(item1->GetId());
        CHECK(found_by_id == item1);

        // Destroy inventory
        server->CrMngr.DestroyInventory(cr);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("MultipleCritterCreation")
    {
        const size_t count = 5;
        vector<ptr<Critter>> critters;

        for (size_t i = 0; i < count; i++) {
            auto cr = server->CreateCritter(fn("TestCritter"), false);
            critters.push_back(cr);
        }

        // All should have unique IDs
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                CHECK(critters[i]->GetId() != critters[j]->GetId());
            }
        }

        // Clean up
        for (ptr<Critter> cr : critters) {
            server->CrMngr.DestroyCritter(cr);
        }
    }

    SECTION("CritterDestroyState")
    {
        // Hold a ref so the critter object survives DestroyCritter (which drops the manager's last
        // reference and frees it) and the post-destroy IsDestroyed() check reads a valid object.
        auto cr = server->CreateCritter(fn("TestCritter"), false).hold_ref();

        CHECK_FALSE(cr->IsDestroyed());
        server->CrMngr.DestroyCritter(cr.get());
        CHECK(cr->IsDestroyed());
    }
}

// ========== Item C++ API Tests ==========

TEST_CASE("ItemCppApi")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("ItemCreationAndDestruction")
    {
        // Hold a ref so the item survives DestroyItem (which drops the manager's last reference and
        // frees it) and the post-destroy IsDestroyed() check reads a valid object.
        auto item = server->ItemMngr.CreateItem(fn("TestItem"), 1, nullptr).hold_ref();

        CHECK(item->GetId() != ident_t {});
        CHECK(item->GetProtoId() == fn("TestItem"));
        CHECK_FALSE(item->IsDestroyed());

        server->ItemMngr.DestroyItem(item.get());
        CHECK(item->IsDestroyed());
    }

    SECTION("ItemAddToCritterAndRemove")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        auto item = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 5);
        REQUIRE(static_cast<bool>(item));
        CHECK(cr->HasItems());

        server->ItemMngr.SubItemCritter(cr, fn("TestItem"), 3);
        // Still has 2 items
        CHECK(cr->HasItems());

        server->ItemMngr.SubItemCritter(cr, fn("TestItem"), 2);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("ItemSetCount")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        server->ItemMngr.SetItemCritter(cr, fn("TestItem"), 10);
        CHECK(cr->CountInvItemByPid(fn("TestItem")) == 10);

        server->ItemMngr.SetItemCritter(cr, fn("TestItem"), 3);
        CHECK(cr->CountInvItemByPid(fn("TestItem")) == 3);

        server->ItemMngr.SetItemCritter(cr, fn("TestItem"), 0);
        CHECK(cr->CountInvItemByPid(fn("TestItem")) == 0);

        server->CrMngr.DestroyCritter(cr);
    }

    SECTION("MultipleItemTypes")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        auto item1 = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        auto item2 = server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);

        REQUIRE(static_cast<bool>(item1));
        REQUIRE(static_cast<bool>(item2));

        // Different item instances
        CHECK(item1->GetId() != item2->GetId());

        vector<ptr<Item>> inv = cr->GetInvItems();
        CHECK(inv.size() >= 2);

        server->CrMngr.DestroyInventory(cr);
        server->CrMngr.DestroyCritter(cr);
    }
}

// ========== Location C++ API Tests ==========

TEST_CASE("LocationCppApi")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("CreateAndDestroyLocation")
    {
        // Hold a ref so the location survives DestroyLocation (which drops the manager's last
        // reference and frees it) and the post-destroy IsDestroyed() check reads a valid object.
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation")).hold_ref();

        CHECK(loc->GetId() != ident_t {});
        CHECK(loc->GetProtoId() == fn("TestLocation"));
        CHECK_FALSE(loc->IsDestroyed());

        server->MapMngr.DestroyLocation(loc.get());
        CHECK(loc->IsDestroyed());
    }

    SECTION("MultipleLocations")
    {
        auto loc1 = server->MapMngr.CreateLocation(fn("TestLocation"));
        auto loc2 = server->MapMngr.CreateLocation(fn("TestLocation"));

        CHECK(loc1->GetId() != loc2->GetId());

        server->MapMngr.DestroyLocation(loc1);
        CHECK_FALSE(loc2->IsDestroyed());

        server->MapMngr.DestroyLocation(loc2);
    }

    SECTION("LocationMapsEmpty")
    {
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"));

        vector<ptr<Map>> maps = loc->GetMaps();
        CHECK(maps.empty());

        server->MapMngr.DestroyLocation(loc);
    }
}

// ========== Health Info C++ API Tests ==========

TEST_CASE("ServerHealthInfo")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    SECTION("HealthInfoNotEmpty")
    {
        const auto info = server->GetHealthInfo();
        CHECK_FALSE(info.empty());
    }

    SECTION("HealthInfoContainsUptime")
    {
        const auto info = server->GetHealthInfo();
        CHECK(info.find("Server uptime") != string::npos);
    }
}

// ========== Player Registration C++ API Tests ==========

TEST_CASE("PlayerRegistrationCppApi")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool server_locked = true;
    auto unlock = scope_exit([&server, &server_locked]() noexcept {
        if (server_locked) {
            safe_call([&server] { server->Unlock(); });
        }
    });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("LoginPlayerToNewRecordAllocatesNonZeroId")
    {
        auto player = CreateLoggedPlayer(server, "TestPlayer1");
        CHECK(player->GetId() != ident_t {});
    }

    SECTION("LoginPlayerToNewRecordRegistersPlayer")
    {
        auto player = CreateLoggedPlayer(server, "Player1");
        auto registered_player = server->EntityMngr.GetPlayer(player->GetId());
        REQUIRE(registered_player);
        CHECK(registered_player.as_ptr() == player);
    }

    SECTION("LoginPlayerToNewRecordProducesUniqueIds")
    {
        auto player1 = CreateLoggedPlayer(server, "Player1");
        auto player2 = CreateLoggedPlayer(server, "Player2");
        CHECK(player1->GetId() != player2->GetId());
    }

    SECTION("PlayerLoginHardDisconnectDefersDestructionToPlayerJob")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(1));

        const auto initial_player_count = server->EntityMngr.GetPlayersCount();
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto unlogined_player = server->CreateUnloginedPlayer(std::move(net_connection));
        unlogined_player->SetName("LoginDisconnect");
        unlogined_player->SetLastControlledCritterId(ident_t {1});

        (void)server->LoginPlayerToNewRecord(unlogined_player);

        int32_t login_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerLoginCalls"), login_calls));
        CHECK(login_calls == 1);

        int32_t destroyed_after_disconnect_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerDestroyedAfterLoginDisconnectCalls"), destroyed_after_disconnect_calls));
        CHECK(destroyed_after_disconnect_calls == 0);

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, initial_player_count] { return server->EntityMngr.GetPlayersCount() == initial_player_count; }, std::chrono::milliseconds {2000}));

        CHECK(server->EntityMngr.GetPlayersCount() == initial_player_count);

        int32_t logout_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerLogoutCalls"), logout_calls));
        CHECK(logout_calls == 1);
    }

    SECTION("TempSessionPlayerHardDisconnectDefersDestructionToPlayerJob")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        const auto initial_player_count = server->EntityMngr.GetPlayersCount();
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto unlogined_player = server->CreateUnloginedPlayer(std::move(net_connection));
        unlogined_player->SetName("TempSessionDisconnect");

        auto player = server->LoginPlayerToTempSession(unlogined_player);
        REQUIRE(static_cast<bool>(player));

        int32_t login_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerLoginCalls"), login_calls));
        CHECK(login_calls == 1);

        player->GetConnection()->HardDisconnect();

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, initial_player_count] { return server->EntityMngr.GetPlayersCount() == initial_player_count; }, std::chrono::milliseconds {2000}));

        CHECK(server->EntityMngr.GetPlayersCount() == initial_player_count);

        int32_t logout_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerLogoutCalls"), logout_calls));
        CHECK(logout_calls == 1);
    }

    SECTION("CritterInitialInfoMayDetachPlayerBeforeSwitchNotification")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto player = CreateLoggedPlayer(server, "InitialInfoDetach");

        auto prev_cr = server->CreateCritter(fn("TestCritter"), true);
        auto next_cr = server->CreateCritter(fn("TestCritter"), true);

        server->SwitchPlayerCritter(player, prev_cr);
        CHECK(player->GetControlledCritter() == prev_cr.get());

        REQUIRE(reset_func.Call());

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(3));

        server->SwitchPlayerCritter(player, next_cr);

        CHECK_FALSE(static_cast<bool>(player->GetControlledCritter()));
        CHECK_FALSE(static_cast<bool>(next_cr->GetPlayer()));
        CHECK_FALSE(static_cast<bool>(prev_cr->GetPlayer()));

        int32_t initial_info_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetCritterSendInitialInfoCalls"), initial_info_calls));
        CHECK(initial_info_calls == 1);

        int32_t switched_calls = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetPlayerCritterSwitchedCalls"), switched_calls));
        CHECK(switched_calls == 0);

        next_cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(next_cr);

        prev_cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(prev_cr);
    }

    SECTION("StopMoveDirEventMayDetachPlayerBeforeStoppingMovement")
    {
        auto reset_func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(reset_func);
        REQUIRE(reset_func.Call());

        auto test_connection = SafeAlloc::MakeShared<TestNetworkConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "StopMoveDetach");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (!loc->IsDestroyed()) {
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto nullable_map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(nullable_map));
        auto map = nullable_map.as_ptr();

        auto cr = server->CreateCritter(fn("TestCritter"), true);

        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());
        REQUIRE(cr->GetPlayer() == player);

        const vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        const vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr.get(), uint16_t {1}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());
        const uint32_t moving_uid = cr->GetMovingUid();

        auto set_mode_func = server->FindFunc<void, int32_t>(fn("EntityLifecycle::SetPlayerEventMode"));
        REQUIRE(set_mode_func);
        REQUIRE(set_mode_func.Call(4));

        auto test_connection_ptr = test_connection.as_ptr();
        SendStopCritterMove(test_connection_ptr, server, map->GetId(), cr->GetId(), cr->GetHex(), ipos16 {}, mdir {});

        int32_t dir_calls = 0;
        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &fn, &dir_calls] { return server->CallFunc(fn("EntityLifecycle::GetPlayerDirCritterCalls"), dir_calls) && dir_calls == 1; }));

        CHECK_FALSE(static_cast<bool>(player->GetControlledCritter()));
        CHECK_FALSE(static_cast<bool>(cr->GetPlayer()));
        CHECK(cr->GetMapId() == map->GetId());
        CHECK(map->GetCritter(cr->GetId()) == cr.get());
        CHECK(cr->IsMoving());
        CHECK(cr->GetMovingUid() == moving_uid);

        REQUIRE(set_mode_func.Call(0));
        server->StopCritterMoving(cr.get());
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
    }
}

// ========== NPC Manager C++ API Tests ==========

TEST_CASE("CritterManagerCppApi")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("GetNonPlayerCritters")
    {
        auto cr1 = server->CreateCritter(fn("TestCritter"), false);
        auto cr2 = server->CreateCritter(fn("TestCritter"), false);

        auto npcs = server->CrMngr.GetNonPlayerCritters();
        CHECK(npcs.size() >= 2);

        bool found1 = false;
        bool found2 = false;
        for (auto& npc : npcs) {
            if (npc == cr1.get()) {
                found1 = true;
            }
            if (npc == cr2.get()) {
                found2 = true;
            }
        }
        CHECK(found1);
        CHECK(found2);

        server->CrMngr.DestroyCritter(cr1);
        server->CrMngr.DestroyCritter(cr2);
    }

    SECTION("DestroyInventory")
    {
        auto cr = server->CreateCritter(fn("TestCritter"), false);

        server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        server->ItemMngr.AddItemCritter(cr, fn("TestItem"), 1);
        CHECK(cr->HasItems());

        server->CrMngr.DestroyInventory(cr);
        CHECK_FALSE(cr->HasItems());

        server->CrMngr.DestroyCritter(cr);
    }
}

// ========== Proto Access C++ API Tests ==========

TEST_CASE("ProtoAccessCppApi")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("GetProtoCritter")
    {
        auto proto = server->GetProtoCritter(fn("TestCritter"));
        CHECK(static_cast<bool>(proto));
    }

    SECTION("GetProtoItem")
    {
        auto proto = server->GetProtoItem(fn("TestItem"));
        CHECK(static_cast<bool>(proto));
    }

    SECTION("GetProtoLocation")
    {
        auto proto = server->GetProtoLocation(fn("TestLocation"));
        CHECK(static_cast<bool>(proto));
    }

    SECTION("GetProtoCritterReturnsNullForMissing")
    {
        auto proto = server->GetProtoCritter(fn("NonExistentProto"));
        CHECK_FALSE(static_cast<bool>(proto));
    }

    SECTION("GetProtoItemReturnsNullForMissing")
    {
        auto proto = server->GetProtoItem(fn("NonExistentProto"));
        CHECK_FALSE(static_cast<bool>(proto));
    }

    SECTION("GetProtoLocationReturnsNullForMissing")
    {
        auto proto = server->GetProtoLocation(fn("NonExistentProto"));
        CHECK_FALSE(static_cast<bool>(proto));
    }
}

// ========== Script Function Call Tests ==========

TEST_CASE("ScriptFunctionCalls")
{
    auto settings = MakeSettings();
    auto server = MakeServerEngine(settings);
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    SECTION("CallFuncWithReturnValue")
    {
        int32_t result = 0;
        REQUIRE(server->CallFunc(fn("EntityLifecycle::GetItemInitCalls"), result));
        // Result is valid (we just check the API works)
    }

    SECTION("FindFuncReturnsFalseForMissing")
    {
        auto func = server->FindFunc<int32_t>(fn("NonExistent::Function"));
        CHECK_FALSE(func);
    }

    SECTION("FindFuncWorksForExisting")
    {
        auto func = server->FindFunc<int32_t>(fn("EntityLifecycle::GetItemInitCalls"));
        CHECK(func);
        if (func) {
            REQUIRE(func.Call());
        }
    }

    SECTION("FindFuncVoid")
    {
        auto func = server->FindFunc<void>(fn("EntityLifecycle::ResetCounters"));
        REQUIRE(func);
        REQUIRE(func.Call());
    }
}

FO_END_NAMESPACE
