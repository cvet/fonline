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

#include "Baker.h"
#include "DataSerialization.h"
#include "NetBuffer.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

// An arrival report settles the server position before the action behind it is read. The rig is script-free: the
// only player fixture (Test_EntityLifecycle.cpp) compiles with AngelScript, which this project never enables
namespace
{
    class MoveReconciliationConnection final : public NetworkServerConnection
    {
    public:
        // Plays the client half of the secure channel: the offer waits until a ServerConnection takes it over, and
        // its answer, read back in DispatchImpl, opens the channel the reports are sealed into
        explicit MoveReconciliationConnection(ptr<ServerNetworkSettings> settings) :
            NetworkServerConnection(settings),
            _clientChannel {vector<crypto::key_bytes> {ParseSecureChannelKey(BakerTests::TEST_CHANNEL_PUBLIC_KEY, "Test")}}
        {
            _host = "Test";

            vector<uint8_t> offer;
            _clientChannel.TakeHandshakeOutput(offer);
            ReceiveCallback(offer);
        }
        MoveReconciliationConnection(const MoveReconciliationConnection&) = delete;
        MoveReconciliationConnection(MoveReconciliationConnection&&) noexcept = delete;
        auto operator=(const MoveReconciliationConnection&) = delete;
        auto operator=(MoveReconciliationConnection&&) noexcept = delete;
        ~MoveReconciliationConnection() override = default;

        void Receive(const_span<uint8_t> buf)
        {
            vector<uint8_t> sealed;

            {
                scoped_lock locker {_clientChannelLocker};

                _clientChannel.Seal(buf, sealed);
            }

            ReceiveCallback(sealed);
        }

    protected:
        void DispatchImpl() override
        {
            scoped_lock locker {_clientChannelLocker};

            vector<uint8_t> encoded_data = SendCallback();

            if (!encoded_data.empty()) {
                _clientChannel.Receive(encoded_data, _plainData);
                _plainData.clear();
            }
        }

        void DisconnectImpl() override { }

    private:
        mutex _clientChannelLocker {};
        SecureChannel _clientChannel;
        vector<uint8_t> _plainData {};
    };

    auto MakeSettings() -> GlobalSettings
    {
        GlobalSettings settings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);
        BakerTests::OverrideSetting(settings.Server.DbStorage, string {"Memory"});
        BakerTests::OverrideSetting(settings.Server.WriteHealthFile, false);
        BakerTests::OverrideSetting(settings.Common.Packaged, false);
        BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
        return settings;
    }

    auto MakeEmptyMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        data_writer writer(map_data);

        writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);
        writer.write<uint32_t>(uint32_t {0}); // hashes_count
        writer.write<uint32_t>(uint32_t {0}); // cr_count
        writer.write<uint32_t>(uint32_t {0}); // item_count
        return map_data;
    }

    auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrar = proto_engine.GetPropertyRegistrar(type_name);
        REQUIRE(static_cast<bool>(registrar));

        ProtoMap proto {proto_engine.Hashes.to_hashed_string(proto_name), registrar};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);
        ignore_unused(str_hashes);

        vector<uint8_t> protos_data;
        data_writer writer(protos_data);

        writer.write<uint32_t>(uint32_t {0});
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint32_t>(uint32_t {1});
        writer.write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.write_string_bytes(type_name.as_str());
        writer.write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.write_string_bytes(proto_name);
        writer.write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));

        if (!props_data.empty()) {
            writer.write_bytes({props_data.data(), props_data.size()});
        }

        return protos_data;
    }

    auto MakeResources() -> FileSystem
    {
        vector<uint8_t> metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MoveReconciliationCompiler");
        compiler_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        hstring location_type = proto_engine.Hashes.to_hashed_string("Location");
        hstring map_type = proto_engine.Hashes.to_hashed_string("Map");

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MoveReconciliationRuntime");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("MoveCritter.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter"));
        runtime_source->AddFile("MoveLocation.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation"));
        runtime_source->AddFile("TestMap.fopro-bin-server", MakeMapProtoBlob(proto_engine, map_type, "TestMap", msize {200, 200}));
        runtime_source->AddFile("TestMap.fomap-bin-server", MakeEmptyMapBlob());

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    auto WaitForStart(ptr<ServerEngine> server) -> string
    {
        for (int32_t i = 0; i < 6000; i++) {
            if (server->IsStarted()) {
                return {};
            }
            if (server->IsStartingError()) {
                return "ServerEngine startup failed";
            }

            coarse_sleep(std::chrono::milliseconds {10});
        }

        return "ServerEngine startup timed out";
    }

    auto CreateLoggedPlayer(ptr<ServerEngine> server, shared_ptr<NetworkServerConnection> net_connection, string_view name) -> ptr<Player>
    {
        auto not_logged_in_player = server->CreateNotLoggedInPlayer(std::move(net_connection));

        not_logged_in_player->SetName(name);
        not_logged_in_player->SetLastControlledCritterId(ident_t {1});

        return server->LoginPlayerToNewRecord(not_logged_in_player);
    }

    void SendCritterMoveFinished(ptr<MoveReconciliationConnection> connection, ptr<ServerEngine> server, ident_t map_id, ident_t cr_id, mpos reported_end_hex, mpos client_hex, ipos16 client_hex_offset, mdir client_dir)
    {
        NetOutBuffer packet {numeric_cast<size_t>(server->Settings->Network.NetBufferSize)};

        packet.StartMsg(NetMessage::SendCritterMoveFinished);
        packet.Write(map_id);
        packet.Write(cr_id);
        packet.Write(reported_end_hex);
        packet.Write(client_hex);
        packet.Write(client_hex_offset);
        packet.Write(client_dir);
        packet.EndMsg();

        connection->Receive(packet.GetData());
    }

    // Plants a measured round trip the way a ping exchange would, so a test exercises the real arrival
    // allowance instead of the bare movement period a never-pinged connection is limited to
    void PlantConnectionRoundTrip(ptr<Player> player, ptr<ServerEngine> server, timespan round_trip)
    {
        auto connection = player->GetConnection();
        nanotime request_time = server->GameTime.GetFrameTime();

        connection->RegisterPingRequest(request_time);
        connection->RegisterPingAnswer(request_time + round_trip);
    }

    auto WaitForUnlockedServerCondition(ptr<ServerEngine> server, bool& locked, const function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        FO_VERIFY_AND_THROW(locked, "Server must be locked before waiting on condition");

        server->Unlock();
        locked = false;

        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            coarse_sleep(std::chrono::milliseconds {5});

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

TEST_CASE("ServerCritterMovePositionReconciliation")
{
    auto settings = MakeSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeResources());
    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    bool server_locked = true;
    auto unlock = scope_exit([&server, &server_locked]() noexcept {
        safe_call([&server, &server_locked] {
            if (server_locked) {
                server->Unlock();
            }
        });
    });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    SECTION("TransferLandsOnTheHexWithoutTheInterruptedStep")
    {
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos landing_hex {40, 40};

        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);

        // A critter caught between two hexes carries that step as a sub-hex offset; a teleport must not keep it,
        // or every client normalizes it into a neighbouring hex and the next move starts one hex off
        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {1}, move_steps, control_steps, ipos16 {}, nullptr);
        cr->SetHexOffset(ipos16 {12, -6});
        REQUIRE(cr->IsMoving());

        server->MapMngr.TransferToMap(cr, map, landing_hex, mdir {}, std::nullopt);

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetHex() == landing_hex);
        CHECK(cr->GetHexOffset() == ipos16 {});

        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("RefusesArrivalBeyondCatchUpAllowance")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "MoveFinishedTooEarly");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos server_hex {20, 20};

        server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        // One pixel per second leaves the whole movement far outside any allowance this connection can earn,
        // so the reported arrival is a claim to skip travel the client cannot have performed
        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {1}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());

        auto moving = cr->GetMoving();
        REQUIRE(static_cast<bool>(moving));
        mpos end_hex = moving->GetEndHex();

        SendCritterMoveFinished(test_connection, server, map->GetId(), cr->GetId(), end_hex, end_hex, ipos16 {}, mdir {});

        // Nothing is expected to change, so this wait must run out instead of succeeding
        CHECK_FALSE(WaitForUnlockedServerCondition(
            server, server_locked,
            [&server, &cr] {
                auto ctx = server->RequireCurrentSyncContext();
                ctx->SyncEntity(cr);
                return !cr->IsMoving();
            },
            std::chrono::milliseconds {300}));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);

        CHECK(cr->IsMoving());
        CHECK(cr->GetHex() == server_hex);

        server->StopCritterMoving(cr.get());
        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("ReconcilesReportedPositionWithinAllowance")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "MoveFinishedAccepted");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos server_hex {20, 20};
        mpos first_step_hex = server_hex;
        REQUIRE(GeometryHelper::MoveHexByDir(first_step_hex, hdir::East, map->GetSize()));

        server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        PlantConnectionRoundTrip(player, server, timespan {std::chrono::milliseconds {800}});

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {400}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());

        auto moving = cr->GetMoving();
        REQUIRE(static_cast<bool>(moving));
        mpos end_hex = moving->GetEndHex();

        // The allowance is min(round trip / 2, MoveFinishCatchUpMaxMs) + CritterMovingPeriodMs. This movement
        // is authored to fit inside it, and the guard fails loudly if the hex metric or the defaults change
        int32_t allowed_catch_up_ms = std::min(400, server->Settings->Server.MoveFinishCatchUpMaxMs) + server->Settings->Server.CritterMovingPeriodMs;
        REQUIRE(moving->GetWholeTime() < numeric_cast<float32_t>(allowed_catch_up_ms));

        // Reporting arrival one hex in must settle the critter there, short of where the plan ends. Left to
        // itself the movement would carry it to end_hex, so this is what the reconciliation actually decides
        SendCritterMoveFinished(test_connection, server, map->GetId(), cr->GetId(), end_hex, first_step_hex, ipos16 {}, mdir {});

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetHex() == first_step_hex);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("IgnoresArrivalNamingAnotherPlan")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "MoveFinishedStalePlan");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos server_hex {20, 20};
        mpos first_step_hex = server_hex;
        REQUIRE(GeometryHelper::MoveHexByDir(first_step_hex, hdir::East, map->GetSize()));

        server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        PlantConnectionRoundTrip(player, server, timespan {std::chrono::milliseconds {800}});

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {400}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());

        auto moving = cr->GetMoving();
        REQUIRE(static_cast<bool>(moving));
        mpos end_hex = moving->GetEndHex();

        // The same report as the accepted case except for the plan it names, which this critter is not
        // walking. It must be dropped, leaving the movement to finish where its own plan ends
        SendCritterMoveFinished(test_connection, server, map->GetId(), cr->GetId(), server_hex, first_step_hex, ipos16 {}, mdir {});

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);

        CHECK(cr->GetHex() == end_hex);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }
}

FO_END_NAMESPACE
