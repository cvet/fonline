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
#include "Baker.h"
#include "DataSerialization.h"
#include "NetBuffer.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

// The global-map group fan-out is a script-driven follow-up to initial info, so its cover contract is pinned
// through the script export itself
void Server_Critter_SendGlobalMapGroupInfo(ptr<Critter> self);

namespace EntityLifecycleTests
{
    class TestNetworkConnection final : public NetworkServerConnection
    {
    public:
        // Plays the client half of the secure channel; the offer waits in the connection until a ServerConnection
        // takes it over, whose answer then opens the channel before any message is written
        explicit TestNetworkConnection(ptr<ServerNetworkSettings> settings) :
            NetworkServerConnection(settings),
            _clientChannel {vector<crypto::key_bytes> {ParseSecureChannelKey(BakerTests::TEST_CHANNEL_PUBLIC_KEY, "Test")}}
        {
            _host = "Test";

            vector<uint8_t> offer;
            _clientChannel.TakeHandshakeOutput(offer);
            ReceiveCallback(offer);
        }
        TestNetworkConnection(const TestNetworkConnection&) = delete;
        TestNetworkConnection(TestNetworkConnection&&) noexcept = delete;
        auto operator=(const TestNetworkConnection&) = delete;
        auto operator=(TestNetworkConnection&&) noexcept = delete;
        ~TestNetworkConnection() override = default;

        void Receive(const_span<uint8_t> buf)
        {
            vector<uint8_t> sealed;

            {
                scoped_lock locker {_clientChannelLocker};

                _clientChannel.Seal(buf, sealed);
            }

            ReceiveCallback(sealed);
        }

        void ResetSentPacketCount() noexcept
        {
            _sentPacketCount.store(0, std::memory_order_relaxed);
            _sentAddCritterCount.store(0, std::memory_order_relaxed);
            _sentRemoveCritterCount.store(0, std::memory_order_relaxed);
            _sentInfoMessageCount.store(0, std::memory_order_relaxed);
            _sentDisconnectCount.store(0, std::memory_order_relaxed);
            _lastSentInfoMessage.store(EngineInfoMessage::None, std::memory_order_relaxed);
            _sentTrackedMessageCount.store(0, std::memory_order_relaxed);
        }

        [[nodiscard]] auto GetSentPacketCount() const noexcept -> size_t { return _sentPacketCount.load(std::memory_order_relaxed); }

        [[nodiscard]] auto GetSentMessageCount(NetMessage msg) const noexcept -> size_t
        {
            switch (msg) {
            case NetMessage::AddCritter:
                return _sentAddCritterCount.load(std::memory_order_relaxed);
            case NetMessage::RemoveCritter:
                return _sentRemoveCritterCount.load(std::memory_order_relaxed);
            case NetMessage::Disconnect:
                return _sentDisconnectCount.load(std::memory_order_relaxed);
            default:
                return 0;
            }
        }

        [[nodiscard]] auto GetSentTrackedMessageCount() const noexcept -> size_t { return _sentTrackedMessageCount.load(std::memory_order_relaxed); }

        [[nodiscard]] auto GetSentInfoMessageCount() const noexcept -> size_t { return _sentInfoMessageCount.load(std::memory_order_relaxed); }

        [[nodiscard]] auto GetLastSentInfoMessage() const noexcept -> EngineInfoMessage { return _lastSentInfoMessage.load(std::memory_order_relaxed); }

        [[nodiscard]] auto GetSentTrackedMessage(size_t index) const -> NetMessage
        {
            FO_VERIFY_AND_THROW(index < 2, "Invalid tracked outgoing message index", index);
            return index == 0 ? _firstSentTrackedMessage.load(std::memory_order_relaxed) : _secondSentTrackedMessage.load(std::memory_order_relaxed);
        }

    protected:
        void DispatchImpl() override
        {
            // Held across the pull and the decode, so frames are opened in the order the server sealed them
            scoped_lock locker {_clientChannelLocker};

            vector<uint8_t> encoded_data = SendCallback();
            _plainData.clear();

            if (!encoded_data.empty()) {
                _clientChannel.Receive(encoded_data, _plainData);
            }

            if (!_plainData.empty()) {
                _sentPacketCount.fetch_add(1, std::memory_order_relaxed);

                const_span<uint8_t> data = _plainData;
                if (!_settings->Network.DisableZlibCompression) {
                    _decompressor.decompress(_plainData, _unpackedData);
                    data = _unpackedData;
                }

                constexpr size_t header_size = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(NetMessage);
                size_t offset = 0;

                while (offset < data.size()) {
                    FO_VERIFY_AND_THROW(data.size() - offset >= header_size, "Truncated outgoing network message header", data.size(), offset);

                    uint32_t signature {};
                    uint32_t message_size {};
                    NetMessage message {};
                    memory::copy(&signature, data.data() + offset, sizeof(signature));
                    memory::copy(&message_size, data.data() + offset + sizeof(signature), sizeof(message_size));
                    memory::copy(&message, data.data() + offset + sizeof(signature) + sizeof(message_size), sizeof(message));

                    FO_VERIFY_AND_THROW(signature == NetBuffer::NETMSG_SIGNATURE, "Invalid outgoing network message signature", signature);
                    FO_VERIFY_AND_THROW(message_size >= header_size && message_size <= data.size() - offset, "Invalid outgoing network message size", message_size, data.size(), offset);

                    if (message == NetMessage::AddCritter) {
                        _sentAddCritterCount.fetch_add(1, std::memory_order_relaxed);
                    }
                    else if (message == NetMessage::RemoveCritter) {
                        _sentRemoveCritterCount.fetch_add(1, std::memory_order_relaxed);
                    }
                    else if (message == NetMessage::InfoMessage) {
                        FO_VERIFY_AND_THROW(message_size >= header_size + sizeof(EngineInfoMessage), "Truncated outgoing info message", message_size);

                        EngineInfoMessage info_message {};
                        memory::copy(&info_message, data.data() + offset + header_size, sizeof(info_message));
                        _lastSentInfoMessage.store(info_message, std::memory_order_relaxed);
                        _sentInfoMessageCount.fetch_add(1, std::memory_order_relaxed);
                    }
                    else if (message == NetMessage::Disconnect) {
                        _sentDisconnectCount.fetch_add(1, std::memory_order_relaxed);
                    }

                    if (message == NetMessage::AddCritter || message == NetMessage::RemoveCritter) {
                        size_t tracked_index = _sentTrackedMessageCount.fetch_add(1, std::memory_order_relaxed);
                        if (tracked_index == 0) {
                            _firstSentTrackedMessage.store(message, std::memory_order_relaxed);
                        }
                        else if (tracked_index == 1) {
                            _secondSentTrackedMessage.store(message, std::memory_order_relaxed);
                        }
                    }

                    offset += message_size;
                }
            }
        }

        void DisconnectImpl() override { }

    private:
        std::atomic<size_t> _sentPacketCount {};
        std::atomic<size_t> _sentAddCritterCount {};
        std::atomic<size_t> _sentRemoveCritterCount {};
        std::atomic<size_t> _sentInfoMessageCount {};
        std::atomic<size_t> _sentDisconnectCount {};
        std::atomic<EngineInfoMessage> _lastSentInfoMessage {};
        mutex _clientChannelLocker {};
        SecureChannel _clientChannel;
        vector<uint8_t> _plainData {};
        stream_decompressor _decompressor {};
        vector<uint8_t> _unpackedData {};
        std::atomic<size_t> _sentTrackedMessageCount {};
        std::atomic<NetMessage> _firstSentTrackedMessage {};
        std::atomic<NetMessage> _secondSentTrackedMessage {};
    };

    inline auto MakeSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);
        return settings;
    }

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
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
    int CritterPreLoadCalls = 0;
    int CritterLoadCalls = 0;
    int CritterUnloadCalls = 0;
    int CritterUnloadSawOnMapCalls = 0;
    int CritterLoadOrder = 0;
    bool CritterPreLoadTransferBlocked = false;
    bool CritterPreLoadSawControllable = false;
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
        Game.OnCritterPreLoad.Subscribe(OnCritterPreLoad);
        Game.OnCritterInit.Subscribe(OnCritterInit);
        Game.OnLocationInit.Subscribe(OnLocationInit);
        Game.OnItemFinish.Subscribe(OnItemFinish);
        Game.OnCritterFinish.Subscribe(OnCritterFinish);
        Game.OnLocationFinish.Subscribe(OnLocationFinish);
        Game.OnCritterLoad.Subscribe(OnCritterLoad);
        Game.OnCritterUnload.Subscribe(OnCritterUnload);
        Game.OnGlobalMapCritterIn.Subscribe(OnGlobalMapCritterIn);
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
    void OnCritterPreLoad(Critter cr)
    {
        CritterPreLoadCalls++;
        CritterPreLoadSawControllable = cr.ControlledByPlayer;

        if (CritterLifecycleEventMode == 3) {
            CritterLoadOrder = cr.MapId == ZERO_IDENT && CritterLoadOrder == 0 ? 1 : -1;

            try {
                cr.TransferToGlobal();
            }
            catch {
                CritterPreLoadTransferBlocked = true;
            }
        }
        else if (CritterLifecycleEventMode == 4) {
            cr.MakeControllable(false);
            Game.DestroyCritter(cr);
        }
        else if (CritterLifecycleEventMode == 5) {
            cr.TransferToGlobal();
        }
    }

    [[Event]]
    void OnCritterInit(Critter cr, bool firstTime)
    {
        CritterInitCalls++;

        if (CritterLifecycleEventMode == 3 && !firstTime) {
            CritterLoadOrder = CritterLoadOrder == 2 ? 3 : -3;
        }

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
    int GetCritterPreLoadCalls() { return CritterPreLoadCalls; }
    int GetCritterLoadCalls() { return CritterLoadCalls; }
    int GetCritterUnloadCalls() { return CritterUnloadCalls; }
    int GetCritterUnloadSawOnMapCalls() { return CritterUnloadSawOnMapCalls; }
    int GetCritterLoadOrder() { return CritterLoadOrder; }
    bool GetCritterPreLoadTransferBlocked() { return CritterPreLoadTransferBlocked; }
    bool GetCritterPreLoadSawControllable() { return CritterPreLoadSawControllable; }
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
        CritterPreLoadCalls = 0;
        CritterLoadCalls = 0;
        CritterUnloadCalls = 0;
        CritterUnloadSawOnMapCalls = 0;
        CritterLoadOrder = 0;
        CritterPreLoadTransferBlocked = false;
        CritterPreLoadSawControllable = false;
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

        if (CritterLifecycleEventMode == 3) {
            CritterLoadOrder = CritterLoadOrder == 3 ? 4 : -4;
        }

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
    void OnGlobalMapCritterIn(Critter cr)
    {
        if (CritterLifecycleEventMode == 3) {
            CritterLoadOrder = CritterLoadOrder == 1 ? 2 : -2;
        }
    }

    [[Event]]
    EventResult OnPlayerLogin(Player player, Player? notLoggedInPlayer)
    {
        PlayerLoginCalls++;

        if (PlayerEventMode == 1) {
            player.HardDisconnect();

            if (player.IsDestroyed) {
                PlayerDestroyedAfterLoginDisconnectCalls++;
            }
        }
        if (PlayerEventMode == 5) {
            return EventResult::StopChain;
        }
        if (PlayerEventMode == 6) {
            throw("Deliberate login event failure");
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
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }
#endif

    inline auto MakeEmptyMapBlob() -> vector<uint8_t>
    {
        vector<uint8_t> map_data;
        auto writer = data_writer(map_data);
        writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);
        writer.write<uint32_t>(uint32_t {0}); // hashes_count
        writer.write<uint32_t>(uint32_t {0}); // cr_count
        writer.write<uint32_t>(uint32_t {0}); // item_count
        return map_data;
    }

    inline auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrar = proto_engine.GetPropertyRegistrar(type_name);
        REQUIRE(static_cast<bool>(registrar));

        ProtoMap proto {proto_engine.Hashes.to_hashed_string(proto_name), registrar};
        proto.SetSize(map_size);
        proto.GetProperties()->StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = data_writer(protos_data);

        writer.write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
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

    inline auto MakeResources() -> FileSystem
    {
        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("EntityLifecycleCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        hstring item_type = proto_engine.Hashes.to_hashed_string("Item");
        hstring location_type = proto_engine.Hashes.to_hashed_string("Location");
        hstring map_type = proto_engine.Hashes.to_hashed_string("Map");
        auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
        auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "TestLocation");
        auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "TestMap", msize {200, 200});
        auto fomap_blob = MakeEmptyMapBlob();
#if FO_ANGELSCRIPT_SCRIPTING
        auto script_blob = MakeScriptBinary(compiler_resources);
#endif

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("EntityLifecycleRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("EntityLifecycleCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("EntityLifecycleItem.fopro-bin-server", item_blob);
        runtime_source->AddFile("EntityLifecycleLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("TestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("TestMap.fomap-bin-server", fomap_blob);
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile("EntityLifecycle.fos-bin-server", script_blob);
#endif

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    inline auto WaitForStart(ptr<ServerEngine> server) -> string
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

    inline auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return safe_alloc::make_refcounted<ServerEngine>(&settings, MakeResources());
    }

    inline auto CreatePreparedNotLoggedInPlayer(ptr<ServerEngine> server, shared_ptr<NetworkServerConnection> net_connection, string_view name) -> ptr<Player>
    {
        auto not_logged_in_player = server->CreateNotLoggedInPlayer(std::move(net_connection));

        not_logged_in_player->SetName(name);
        not_logged_in_player->SetLastControlledCritterId(ident_t {1});

        return not_logged_in_player;
    }

    inline auto CreatePreparedNotLoggedInPlayer(ptr<ServerEngine> server, string_view name) -> ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        return CreatePreparedNotLoggedInPlayer(server, std::move(net_connection), name);
    }

    // A map spectator is an ordinary Player entity, so the fixture only needs the connection shell — not a
    // login. The caller covers the returned player explicitly through its own SyncEntities call
    inline auto MakeSpectatorPlayer(ptr<ServerEngine> server) -> refcount_ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto connection = safe_alloc::make_unique<ServerConnection>(server->Settings, std::move(net_connection), BakerTests::MakeTestChannelIdentity());

        return safe_alloc::make_refcounted<Player>(server, ident_t {}, std::move(connection));
    }

    inline auto CreateLoggedPlayer(ptr<ServerEngine> server, shared_ptr<NetworkServerConnection> net_connection, string_view name) -> ptr<Player>;

    inline auto CreateLoggedPlayer(ptr<ServerEngine> server, string_view name) -> ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        return CreateLoggedPlayer(server, std::move(net_connection), name);
    }

    inline auto CreateLoggedPlayer(ptr<ServerEngine> server, shared_ptr<NetworkServerConnection> net_connection, string_view name) -> ptr<Player>
    {
        FO_VERIFY_AND_THROW(net_connection, "Missing required net connection");

        auto not_logged_in_player = server->CreateNotLoggedInPlayer(std::move(net_connection));
        not_logged_in_player->SetName(name);
        not_logged_in_player->SetLastControlledCritterId(ident_t {1});
        auto player = server->LoginPlayerToNewRecord(not_logged_in_player);

        return player;
    }

    inline void SendStopCritterMove(ptr<TestNetworkConnection> connection, ptr<ServerEngine> server, ident_t map_id, ident_t cr_id, mpos client_hex, ipos16 client_hex_offset, mdir client_dir)
    {
        NetOutBuffer packet {numeric_cast<size_t>(server->Settings->Network.NetBufferSize)};
        packet.StartMsg(NetMessage::SendStopCritterMove);
        packet.Write(map_id);
        packet.Write(cr_id);
        packet.Write(client_hex);
        packet.Write(client_hex_offset);
        packet.Write(client_dir);
        packet.Write(nanotime::now().milliseconds());
        packet.EndMsg();

        connection->Receive(packet.GetData());
    }

    inline auto WaitForUnlockedServerCondition(ptr<ServerEngine> server, bool& locked, const function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        FO_VERIFY_AND_THROW(locked, "Server must be locked before waiting on condition");

        server->Unlock();
        locked = false;

        auto deadline = std::chrono::steady_clock::now() + timeout;

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

FO_END_NAMESPACE
