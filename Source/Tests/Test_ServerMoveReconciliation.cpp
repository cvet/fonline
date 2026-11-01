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

#include "Baker.h"
#include "DataSerialization.h"
#include "LinkDelay.h"
#include "NetBuffer.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

// Move requests go through the export scripts call, because that is where a refused request decides what observers hear
ptr<MovingContext> Server_Critter_MoveToHex(ptr<Critter> self, mpos hex, int32_t cut, int32_t speed, ScriptFunc<bool, ptr<Critter>, ptr<Item>> gagCallabck);
FO_SCRIPT_API void Server_Player_ViewMapArea(ptr<Player> self, ptr<Map> map, mpos hex, int32_t radius, nptr<ServerEntity> context);
FO_SCRIPT_API void Server_Player_ReturnFromViewMap(ptr<Player> self);
FO_SCRIPT_API void Server_Critter_SendGlobalMapGroupInfo(ptr<Critter> self);

// An arrival report settles the server position before the action behind it is read. The rig runs no script; an
// AngelScript build still needs one bytecode file to start, so it gets an empty one
namespace
{
    // One decoded outgoing packet: fields actually written by the production sender, never reconstructed state
    struct TransportPacketSnapshot
    {
        NetMessage Message {};
        uint32_t FrameBytes {};
        bool PingAnswer {};
        ident_t CrId {};
        int32_t WholeMs {};
        int32_t ElapsedMs {};
        uint16_t Speed {};
        mpos StartHex {};
        vector<hdir> Steps {};
        vector<uint16_t> ControlSteps {};
        ipos16 EndHexOffset {};
        uint32_t MovingUid {};
        float32_t LeaseDistance {};
        int64_t SenderMs {};
        nanotime ReceiveTime {};
    };

    // Copied CritterAttachments wire payload, so recipient assertions never read production attachment state
    struct CritterAttachmentSnapshot
    {
        ident_t CrId {};
        bool IsAttached {};
        ident_t MasterId {};
        vector<ident_t> AttachedIds {};
    };

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

        [[nodiscard]] auto CountReceivedCritterMessage(NetMessage message, ident_t cr_id) -> size_t
        {
            scoped_lock locker {_clientChannelLocker};

            return numeric_cast<size_t>(std::ranges::count(_receivedCritterMessages, std::pair {message, cr_id}));
        }

        [[nodiscard]] auto ReadReceivedSenderTime(NetMessage message, ident_t cr_id) -> int64_t
        {
            scoped_lock locker {_clientChannelLocker};

            for (auto it = _receivedMovement.rbegin(); it != _receivedMovement.rend(); ++it) {
                const ReceivedMovement& received = *it;

                if (received.Message != message || received.CritterId != cr_id) {
                    continue;
                }

                NetInBuffer packet {numeric_cast<size_t>(_settings->Network.NetBufferSize)};
                packet.AddData(received.Data);
                REQUIRE(packet.ReadMsg() == message);
                REQUIRE(packet.Read<ident_t>() == cr_id);

                if (message == NetMessage::CritterMove) {
                    (void)packet.Read<uint32_t>();
                    (void)packet.Read<uint32_t>();
                    (void)packet.Read<uint16_t>();
                    (void)packet.Read<mpos>();

                    uint16_t steps_count = packet.Read<uint16_t>();

                    for (uint16_t i = 0; i < steps_count; i++) {
                        (void)packet.Read<hdir>();
                    }

                    uint16_t control_steps_count = packet.Read<uint16_t>();

                    for (uint16_t i = 0; i < control_steps_count; i++) {
                        (void)packet.Read<uint16_t>();
                    }

                    (void)packet.Read<ipos16>();
                    (void)packet.Read<uint32_t>();
                    (void)packet.Read<float32_t>();
                }
                else {
                    REQUIRE(message == NetMessage::CritterMoveLease);
                    (void)packet.Read<uint32_t>();
                    (void)packet.Read<float32_t>();
                }

                return packet.Read<int64_t>();
            }

            FO_VERIFY_AND_THROW(false, "Movement message was not received", message, cr_id);
        }

        void ClearReceivedMovement()
        {
            scoped_lock locker {_clientChannelLocker};

            _receivedMovement.clear();
        }

        [[nodiscard]] auto GetReceivedAttachmentSnapshots() -> vector<CritterAttachmentSnapshot>
        {
            scoped_lock locker {_clientChannelLocker};

            return _receivedAttachmentSnapshots;
        }

        [[nodiscard]] auto GetReceivedCritterMessages() -> vector<std::pair<NetMessage, ident_t>>
        {
            scoped_lock locker {_clientChannelLocker};

            return _receivedCritterMessages;
        }

        [[nodiscard]] auto GetReceivedTransportPackets() -> vector<TransportPacketSnapshot>
        {
            scoped_lock locker {_clientChannelLocker};

            return _transportPackets;
        }

        void ClearReceivedCritterMessages()
        {
            scoped_lock locker {_clientChannelLocker};

            _receivedCritterMessages.clear();
        }

        void ClearReceivedAttachmentSnapshots()
        {
            scoped_lock locker {_clientChannelLocker};

            _receivedAttachmentSnapshots.clear();
        }

        void ClearReceivedTransportPackets()
        {
            scoped_lock locker {_clientChannelLocker};

            _transportPackets.clear();
        }

        // Holding dispatch leaves framed output queued; the next dispatch compresses and seals it in order
        void SetDispatchHold(bool held)
        {
            scoped_lock locker {_clientChannelLocker};

            _dispatchHeld = held;
        }

    protected:
        void DispatchImpl() override
        {
            scoped_lock locker {_clientChannelLocker};

            if (_dispatchHeld) {
                return;
            }

            vector<uint8_t> encoded_data = SendCallback();

            if (!encoded_data.empty()) {
                _clientChannel.Receive(encoded_data, _plainData);
                RecordCritterMessages();
                _plainData.clear();
            }
        }

        void DisconnectImpl() override { }

    private:
        struct ReceivedMovement
        {
            NetMessage Message {};
            ident_t CritterId {};
            vector<uint8_t> Data {};
        };

        // The compressed stream is continuous, so every packet is unpacked in order even when nothing in it is recorded
        void RecordCritterMessages()
        {
            if (_plainData.empty()) {
                return;
            }

            const_span<uint8_t> data = _plainData;

            if (!_settings->Network.DisableZlibCompression) {
                _decompressor.decompress(_plainData, _unpackedData);
                data = _unpackedData;
            }

            constexpr size_t header_size = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(NetMessage);
            size_t offset = 0;

            while (offset < data.size()) {
                FO_VERIFY_AND_THROW(data.size() - offset >= header_size, "Truncated outgoing network message header", data.size(), offset);

                uint32_t message_signature {};
                uint32_t message_size {};
                NetMessage message {};
                memory::copy(&message_signature, data.data() + offset, sizeof(message_signature));
                memory::copy(&message_size, data.data() + offset + sizeof(uint32_t), sizeof(message_size));
                memory::copy(&message, data.data() + offset + sizeof(uint32_t) + sizeof(message_size), sizeof(message));

                FO_VERIFY_AND_THROW(message_signature == NetBuffer::NETMSG_SIGNATURE, "Invalid outgoing network message signature", message_signature, offset);
                FO_VERIFY_AND_THROW(message_size >= header_size && message_size <= data.size() - offset, "Invalid outgoing network message size", message_size, data.size(), offset);

                if (message == NetMessage::CritterMove || message == NetMessage::CritterPos || message == NetMessage::CritterMoveLease) {
                    FO_VERIFY_AND_THROW(message_size >= header_size + sizeof(ident_t), "Truncated outgoing movement message", message_size);

                    ident_t cr_id {};
                    memory::copy(&cr_id, data.data() + offset + header_size, sizeof(cr_id));
                    const_span<uint8_t> message_data = data.subspan(offset, message_size);
                    _receivedMovement.push_back(ReceivedMovement {.Message = message, .CritterId = cr_id, .Data = vector<uint8_t>(message_data.begin(), message_data.end())});
                }

                if (message == NetMessage::CritterMove || message == NetMessage::CritterPos || message == NetMessage::CritterAttachments || message == NetMessage::AddCritter) {
                    FO_VERIFY_AND_THROW(message_size >= header_size + sizeof(ident_t), "Truncated outgoing movement message", message_size);

                    ident_t cr_id {};
                    memory::copy(&cr_id, data.data() + offset + header_size, sizeof(cr_id));
                    _receivedCritterMessages.emplace_back(message, cr_id);
                }

                if (message == NetMessage::Ping || message == NetMessage::CritterMove || message == NetMessage::CritterMoveLease) {
                    const_span<uint8_t> payload = data.subspan(offset + header_size, message_size - header_size);
                    _transportPackets.emplace_back(DecodeTransportPacket(message, payload, nanotime::now(), message_size));
                }

                if (message == NetMessage::CritterAttachments) {
                    _receivedAttachmentSnapshots.emplace_back(DecodeAttachmentSnapshot(data.subspan(offset + header_size, message_size - header_size)));
                }

                offset += message_size;
            }
        }

        template<typename T>
        static void ReadTransportField(const_span<uint8_t> payload, size_t& pos, T& value)
        {
            FO_VERIFY_AND_THROW(pos <= payload.size() && sizeof(T) <= payload.size() - pos, "Truncated outgoing transport payload", payload.size(), pos, sizeof(T));
            memory::copy(&value, payload.data() + pos, sizeof(T));
            pos += sizeof(T);
        }

        // Payloads of Player::Send_Ping, Player::SendCritterMoving and Player::Send_MovingLease in their wire order
        [[nodiscard]] static auto DecodeTransportPacket(NetMessage message, const_span<uint8_t> payload, nanotime receive_time, uint32_t frame_bytes) -> TransportPacketSnapshot
        {
            TransportPacketSnapshot snapshot;
            snapshot.Message = message;
            snapshot.FrameBytes = frame_bytes;
            snapshot.ReceiveTime = receive_time;

            size_t pos = 0;

            switch (message) {
            case NetMessage::Ping:
                ReadTransportField(payload, pos, snapshot.PingAnswer);
                ReadTransportField(payload, pos, snapshot.SenderMs);
                break;
            case NetMessage::CritterMoveLease:
                ReadTransportField(payload, pos, snapshot.CrId);
                ReadTransportField(payload, pos, snapshot.MovingUid);
                ReadTransportField(payload, pos, snapshot.LeaseDistance);
                ReadTransportField(payload, pos, snapshot.SenderMs);
                break;
            case NetMessage::CritterMove: {
                ReadTransportField(payload, pos, snapshot.CrId);
                ReadTransportField(payload, pos, snapshot.WholeMs);
                ReadTransportField(payload, pos, snapshot.ElapsedMs);
                ReadTransportField(payload, pos, snapshot.Speed);
                ReadTransportField(payload, pos, snapshot.StartHex);

                uint16_t step_count = 0;
                ReadTransportField(payload, pos, step_count);
                FO_VERIFY_AND_THROW(step_count <= (payload.size() - pos) / sizeof(hdir), "Outgoing movement step count exceeds remaining bytes", step_count, payload.size(), pos);
                snapshot.Steps.resize(step_count);

                for (uint16_t i = 0; i < step_count; i++) {
                    ReadTransportField(payload, pos, snapshot.Steps[i]);
                }

                uint16_t control_count = 0;
                ReadTransportField(payload, pos, control_count);
                FO_VERIFY_AND_THROW(control_count <= (payload.size() - pos) / sizeof(uint16_t), "Outgoing movement control count exceeds remaining bytes", control_count, payload.size(), pos);
                snapshot.ControlSteps.resize(control_count);

                for (uint16_t i = 0; i < control_count; i++) {
                    ReadTransportField(payload, pos, snapshot.ControlSteps[i]);
                }

                ReadTransportField(payload, pos, snapshot.EndHexOffset);
                ReadTransportField(payload, pos, snapshot.MovingUid);
                ReadTransportField(payload, pos, snapshot.LeaseDistance);
                ReadTransportField(payload, pos, snapshot.SenderMs);
            } break;
            default:
                throw GenericException("Unsupported outgoing transport packet", message);
            }

            FO_VERIFY_AND_THROW(pos == payload.size(), "Outgoing transport payload has trailing bytes", message, payload.size(), pos);

            return snapshot;
        }

        // Payload of Player::Send_Attachments: id, attached flag, master id, attached count and each attached id
        [[nodiscard]] static auto DecodeAttachmentSnapshot(const_span<uint8_t> payload) -> CritterAttachmentSnapshot
        {
            constexpr size_t fixed_size = sizeof(ident_t) + sizeof(bool) + sizeof(ident_t) + sizeof(uint16_t);
            FO_VERIFY_AND_THROW(payload.size() >= fixed_size, "Truncated outgoing attachments payload", payload.size());

            size_t pos = 0;
            CritterAttachmentSnapshot snapshot;

            memory::copy(&snapshot.CrId, payload.data() + pos, sizeof(snapshot.CrId));
            pos += sizeof(snapshot.CrId);

            memory::copy(&snapshot.IsAttached, payload.data() + pos, sizeof(snapshot.IsAttached));
            pos += sizeof(snapshot.IsAttached);

            memory::copy(&snapshot.MasterId, payload.data() + pos, sizeof(snapshot.MasterId));
            pos += sizeof(snapshot.MasterId);

            uint16_t attached_count = 0;
            memory::copy(&attached_count, payload.data() + pos, sizeof(attached_count));
            pos += sizeof(attached_count);

            FO_VERIFY_AND_THROW(attached_count <= (payload.size() - pos) / sizeof(ident_t), "Outgoing attachments payload count exceeds remaining bytes", attached_count, payload.size(), pos);

            snapshot.AttachedIds.reserve(attached_count);

            for (uint16_t i = 0; i < attached_count; i++) {
                ident_t attached_id {};
                memory::copy(&attached_id, payload.data() + pos, sizeof(attached_id));
                pos += sizeof(attached_id);
                snapshot.AttachedIds.emplace_back(attached_id);
            }

            FO_VERIFY_AND_THROW(pos == payload.size(), "Outgoing attachments payload has trailing bytes", payload.size(), pos);

            return snapshot;
        }

        mutex _clientChannelLocker {};
        SecureChannel _clientChannel;
        vector<uint8_t> _plainData {};
        stream_decompressor _decompressor {};
        vector<uint8_t> _unpackedData {};
        vector<ReceivedMovement> _receivedMovement {};
        vector<pair<NetMessage, ident_t>> _receivedCritterMessages {};
        vector<CritterAttachmentSnapshot> _receivedAttachmentSnapshots {};
        vector<TransportPacketSnapshot> _transportPackets {};
        bool _dispatchHeld {};
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
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile("MoveReconciliation.fos-bin-server", BakerTests::CompileInlineScripts(&proto_engine, "MoveReconciliationScripts", {{"Scripts/MoveReconciliation.fos", "void MoveReconciliationFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
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

    void SendStopCritterMove(ptr<MoveReconciliationConnection> connection, ptr<ServerEngine> server, ident_t map_id, ident_t cr_id, mpos client_hex, ipos16 client_hex_offset, mdir client_dir)
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

    void SendCritterMove(ptr<MoveReconciliationConnection> connection, ptr<ServerEngine> server, ident_t map_id, ident_t cr_id, uint16_t speed, mpos start_hex, const vector<mdir>& steps, int64_t sender_ms, uint32_t plan_seq = 0, float32_t lease_time = 0.0f)
    {
        NetOutBuffer packet {numeric_cast<size_t>(server->Settings->Network.NetBufferSize)};

        packet.StartMsg(NetMessage::SendCritterMove);
        packet.Write(map_id);
        packet.Write(cr_id);
        packet.Write(speed);
        packet.Write(start_hex);
        packet.Write(numeric_cast<uint16_t>(steps.size()));

        for (mdir step : steps) {
            packet.Write(step.hex());
        }

        packet.Write(uint16_t {1});
        packet.Write(numeric_cast<uint16_t>(steps.size()));
        packet.Write(ipos16 {});
        packet.Write(plan_seq);
        packet.Write(lease_time * numeric_cast<float32_t>(speed) / 1000.0f);
        packet.Write(sender_ms);
        packet.EndMsg();

        connection->Receive(packet.GetData());
    }

    void SendCritterMoveLease(ptr<MoveReconciliationConnection> connection, ptr<ServerEngine> server, ident_t map_id, ident_t cr_id, uint32_t plan_seq, float32_t lease_time, uint16_t sender_speed = 300)
    {
        NetOutBuffer packet {numeric_cast<size_t>(server->Settings->Network.NetBufferSize)};

        packet.StartMsg(NetMessage::SendCritterMoveLease);
        packet.Write(map_id);
        packet.Write(cr_id);
        packet.Write(plan_seq);
        packet.Write(lease_time * numeric_cast<float32_t>(sender_speed) / 1000.0f);
        packet.Write(nanotime::now().milliseconds());
        packet.EndMsg();

        connection->Receive(packet.GetData());
    }

    // A ping stamped with the client's clock gives the link its usual transit before any move is measured against it
    void SendPing(ptr<MoveReconciliationConnection> connection, ptr<ServerEngine> server, int64_t sender_ms)
    {
        NetOutBuffer packet {numeric_cast<size_t>(server->Settings->Network.NetBufferSize)};

        packet.StartMsg(NetMessage::Ping);
        packet.Write(true);
        packet.Write(sender_ms);
        packet.EndMsg();

        connection->Receive(packet.GetData());
    }

    void SendCritterMove(ptr<MoveReconciliationConnection> connection, ptr<ServerEngine> server, ptr<Critter> cr, uint16_t speed)
    {
        SendCritterMove(connection, server, cr->GetMapId(), cr->GetId(), speed, cr->GetHex(), vector<mdir> {mdir {hdir::East}, mdir {hdir::East}, mdir {hdir::East}}, nanotime::now().milliseconds());
    }

    void SendCritterStop(ptr<MoveReconciliationConnection> connection, ptr<ServerEngine> server, ptr<Critter> cr)
    {
        SendStopCritterMove(connection, server, cr->GetMapId(), cr->GetId(), cr->GetHex(), cr->GetHexOffset(), cr->GetDir());
    }

    void SendCritterDir(ptr<MoveReconciliationConnection> connection, ptr<ServerEngine> server, ptr<Critter> cr, mdir dir)
    {
        NetOutBuffer packet {numeric_cast<size_t>(server->Settings->Network.NetBufferSize)};

        packet.StartMsg(NetMessage::SendCritterDir);
        packet.Write(cr->GetMapId());
        packet.Write(cr->GetId());
        packet.Write(dir);
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
        REQUIRE(moving->GetWholeTime().to_ms<float64_t>() < numeric_cast<float32_t>(allowed_catch_up_ms));

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

    SECTION("RefusedMoveRequestStopsTheCritterForItsObservers")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "RefusedMoveObserver");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto observer = server->CreateCritter(fn("TestCritter"), true);
        observer->SetLookDistance(40);
        server->MapMngr.TransferToMap(observer, map, mpos {20, 20}, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, observer);
        REQUIRE(player->GetControlledCritter() == observer.get());

        auto mover = server->CreateCritter(fn("TestCritter"), false);
        mpos mover_hex {24, 20};
        mpos far_hex {44, 20};
        server->MapMngr.TransferToMap(mover, map, mover_hex, mdir {}, std::nullopt);
        REQUIRE(observer->IsSeeCritter(mover->GetId()));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 5> sync_entities {player, observer, mover, map, loc};
        ctx->SyncEntities(sync_entities);

        auto request_move = [&mover](mpos hex, int32_t speed) { return refcount_ptr<MovingContext>::from_adopted_ref(Server_Critter_MoveToHex(mover, hex, 0, speed, {}).get()); };

        // The server keeps the critter where it is, so the observer must be told to stop playing the route it last
        // received; left alone, its client walks the critter to the end of that route and leaves it there
        auto refuse_mid_route = [&](mpos hex, int32_t speed, MovingState refusal) {
            auto route = request_move(far_hex, 1);
            REQUIRE(mover->IsMoving());

            test_connection->Dispatch();
            test_connection->ClearReceivedCritterMessages();

            auto refused = request_move(hex, speed);
            test_connection->Dispatch();

            CHECK(refused->IsCompleted());
            CHECK(refused->GetCompleteReason() == refusal);
            CHECK(route->GetCompleteReason() == MovingState::Stopped);
            CHECK_FALSE(mover->IsMoving());
            CHECK(mover->GetHex() == mover_hex);
            CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterPos, mover->GetId()) == 1);
            CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterMove, mover->GetId()) == 0);
        };

        refuse_mid_route(mover_hex, 1, MovingState::Success);
        refuse_mid_route(far_hex, 0, MovingState::CantMove);

        // A request that finds a route replaces the one in progress with a single CritterMove, not a stop and a start
        auto route = request_move(far_hex, 1);
        REQUIRE(mover->IsMoving());

        test_connection->Dispatch();
        test_connection->ClearReceivedCritterMessages();

        auto retarget = request_move(mpos {24, 40}, 1);
        test_connection->Dispatch();

        CHECK_FALSE(retarget->IsCompleted());
        CHECK(route->GetCompleteReason() == MovingState::Stopped);
        CHECK(mover->IsMoving());
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterMove, mover->GetId()) == 1);
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterPos, mover->GetId()) == 0);

        server->StopCritterMoving(mover);
        server->SwitchPlayerCritter(player, nullptr);
        observer->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(mover);
        server->CrMngr.DestroyCritter(observer);
        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("MovementMessagesAreStampedAtSendTime")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "MovementSendTime");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), false);
        server->MapMngr.TransferToMap(cr, map, mpos {20, 20}, mdir {}, std::nullopt);
        server->StartCritterMoving(cr, uint16_t {70}, vector<mdir> {hdir::East, hdir::East, hdir::East}, vector<uint16_t> {3}, ipos16 {}, nullptr);

        server->GameTime.Pause();
        auto resume = scope_exit([&server]() noexcept { safe_call([&server] { server->GameTime.Resume(); }); });

        coarse_sleep(std::chrono::milliseconds {250});
        test_connection->ClearReceivedMovement();

        // A frame snapshot can be old, but its link timestamp belongs to the actual send, not that frame
        int64_t before_send_ms = nanotime::now().milliseconds();
        player->Send_Moving(cr);
        player->Send_MovingLease(cr);
        int64_t after_send_ms = nanotime::now().milliseconds();
        test_connection->Dispatch();

        CHECK(test_connection->ReadReceivedSenderTime(NetMessage::CritterMove, cr->GetId()) >= before_send_ms);
        CHECK(test_connection->ReadReceivedSenderTime(NetMessage::CritterMove, cr->GetId()) <= after_send_ms);
        CHECK(test_connection->ReadReceivedSenderTime(NetMessage::CritterMoveLease, cr->GetId()) >= before_send_ms);
        CHECK(test_connection->ReadReceivedSenderTime(NetMessage::CritterMoveLease, cr->GetId()) <= after_send_ms);

        server->StopCritterMoving(cr);
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    // A stop reaching the server after the player's own plan ran out there still names a point on that plan: the server
    // walks back to it rather than correcting the player to the plan's end while its later messages move the server on
    SECTION("StopArrivingAfterTheOwnPlanEndedWalksBackAlongIt")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "LateStopOwnPlan");

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

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {400}, move_steps, control_steps, ipos16 {}, player);
        REQUIRE(cr->IsMoving());

        auto moving = cr->GetMoving();
        REQUIRE(static_cast<bool>(moving));
        mpos end_hex = moving->GetEndHex();

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        REQUIRE(cr->GetHex() == end_hex);

        SendStopCritterMove(test_connection, server, map->GetId(), cr->GetId(), first_step_hex, ipos16 {}, mdir {});

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr, first_step_hex] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return cr->GetHex() == first_step_hex;
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetHex() == first_step_hex);
        CHECK_FALSE(static_cast<bool>(cr->GetFinishedPlayerMoving()));

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    // A move a stall held back for three seconds is played from where the player has walked it by now, not from its
    // start, while a move on time starts at the beginning as before
    SECTION("LateMoveIsPlayedFromWhereThePlayerIsByNow")
    {
        for (int64_t delay_ms : {int64_t {3000}, int64_t {0}}) {
            auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
            auto player = CreateLoggedPlayer(server, test_connection, delay_ms != 0 ? "LateMoveCaughtUp" : "MoveOnTime");

            auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
            auto map = loc->GetMapByIndex(0);
            REQUIRE(static_cast<bool>(map));

            auto cr = server->CreateCritter(fn("TestCritter"), true);
            mpos server_hex {20, 20};

            server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
            server->SwitchPlayerCritter(player, cr);
            REQUIRE(player->GetControlledCritter() == cr.get());

            vector<mdir> move_steps(12, mdir {hdir::East});
            SendPing(test_connection, server, nanotime::now().milliseconds());
            SendCritterMove(test_connection, server, map->GetId(), cr->GetId(), uint16_t {20}, server_hex, move_steps, nanotime::now().milliseconds() - delay_ms);

            REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
                auto ctx = server->RequireCurrentSyncContext();
                ctx->SyncEntity(cr);
                return cr->IsMoving();
            }));

            auto ctx = server->RequireCurrentSyncContext();
            small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
            ctx->SyncEntities(sync_entities);

            auto moving = cr->GetMoving();
            REQUIRE(static_cast<bool>(moving));
            float64_t elapsed_ms = moving->GetRuntimeElapsedTime(server->GameTime.GetFrameTime()).to_ms<float64_t>();

            if (delay_ms != 0) {
                CHECK(elapsed_ms >= 2800.0f);
            }
            else {
                CHECK(elapsed_ms < 1000.0f);
            }

            server->StopCritterMoving(cr.get());
            server->SwitchPlayerCritter(player, nullptr);
            cr->UnmarkIsForPlayer();
            server->CrMngr.DestroyCritter(cr);
            server->MapMngr.DestroyLocation(loc);
        }
    }

    // A held direction is traced far ahead but the server walks it only as far as the player has confirmed holding it:
    // it waits at the lease and walks on once the player renews it
    SECTION("LeasedMoveWaitsAtTheLeaseUntilRenewed")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "LeasedMove");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos server_hex {20, 20};

        server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        // Two hundred milliseconds of a fast plan are about two hexes of twelve
        vector<mdir> move_steps(12, mdir {hdir::East});
        SendPing(test_connection, server, nanotime::now().milliseconds());
        SendCritterMove(test_connection, server, map->GetId(), cr->GetId(), uint16_t {300}, server_hex, move_steps, nanotime::now().milliseconds(), 7, 200.0f);

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return cr->IsMoving() && cr->GetMoving()->IsHeldByLease();
        }));

        mpos held_hex;
        {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            held_hex = cr->GetHex();
        }

        CHECK(GeometryHelper::GetDistance(server_hex, held_hex) <= 3);

        // A renewal under another plan's number is spent; the plan's own moves it on
        SendCritterMoveLease(test_connection, server, map->GetId(), cr->GetId(), 6, 5000.0f);
        SendCritterMoveLease(test_connection, server, map->GetId(), cr->GetId(), 7, 5000.0f);

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr, held_hex] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return GeometryHelper::GetDistance(held_hex, cr->GetHex()) >= 3;
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);

        if (cr->IsMoving()) {
            server->StopCritterMoving(cr.get());
        }

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("TruncatedDirectionBecomesAnAuthoritativeFinitePlan")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "TruncatedLease");
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos start_hex {20, 20};
        server->MapMngr.TransferToMap(cr, map, start_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);

        mpos blocker_hex = start_hex;

        for (int32_t i = 0; i < 6; i++) {
            REQUIRE(GeometryHelper::MoveHexByDir(blocker_hex, hdir::East, map->GetSize()));
        }

        map->SetHexManualBlock(blocker_hex, true, false);
        vector<mdir> move_steps(12, mdir {hdir::East});
        SendCritterMove(test_connection, server, map->GetId(), cr->GetId(), uint16_t {100}, start_hex, move_steps, nanotime::now().milliseconds(), 7, 200.0f);

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return cr->IsMoving();
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);
        REQUIRE(cr->GetMoving()->GetSteps().size() == 5);
        CHECK(cr->GetMoving()->GetLeaseTime().to_ms<float64_t>() == 0.0f);

        server->StopCritterMoving(cr);
        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    SECTION("LeaseRenewalPreservesItsPointAcrossASpeedChange")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "LeaseSpeedChange");
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos start_hex {20, 20};
        server->MapMngr.TransferToMap(cr, map, start_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);

        vector<mdir> steps(12, mdir {hdir::East});
        auto moving = safe_alloc::make_refcounted<MovingContext>(map->GetSize(), uint16_t {300}, steps, vector<uint16_t> {12}, server->GameTime.GetFrameTime(), timespan::zero, start_hex, ipos16 {}, ipos16 {});
        moving->SetLeaseTime(timespan {std::chrono::duration<float64_t, std::milli> {200.0f}}, server->GameTime.GetFrameTime());
        server->StartCritterMoving(cr, moving, player, 7);
        server->ChangeCritterMovingSpeed(cr, uint16_t {150});
        REQUIRE(moving->GetLeaseTime().to_ms<float64_t>() == 400.0f);

        // This renewal was sent before the player learned the new speed: its boundary is still ninety pixels in
        SendCritterMoveLease(test_connection, server, map->GetId(), cr->GetId(), 7, 300.0f, uint16_t {300});

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return cr->IsMoving() && cr->GetMoving()->GetLeaseTime().to_ms<float64_t>() >= 590.0f;
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);
        CHECK(moving->GetLeaseTime().to_ms<float64_t>() == 600.0f);

        server->StopCritterMoving(cr);
        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    // A held direction ends with the session that held it: a player who drops while the server waits at the lease comes
    // back to a critter standing there, not to a plan still under way
    SECTION("LeasedMoveStopsWhenThePlayerDisconnects")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "LeasedMoveDropped");

        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos server_hex {20, 20};

        server->MapMngr.TransferToMap(cr, map, server_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        vector<mdir> move_steps(12, mdir {hdir::East});
        SendPing(test_connection, server, nanotime::now().milliseconds());
        SendCritterMove(test_connection, server, map->GetId(), cr->GetId(), uint16_t {300}, server_hex, move_steps, nanotime::now().milliseconds(), 7, 200.0f);

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return cr->IsMoving() && cr->GetMoving()->IsHeldByLease();
        }));

        player->GetConnection()->HardDisconnect(DisconnectReason::ClientClosed);

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 3> sync_entities {cr, map, loc};
        ctx->SyncEntities(sync_entities);

        CHECK(GeometryHelper::GetDistance(server_hex, cr->GetHex()) <= 3);

        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }

    // A plan the player did not start - a script walked the critter - is not the player's to take back: a stop naming a
    // point on it after it has ended is answered with the server position, as before
    SECTION("StopArrivingAfterAScriptPlanEndedKeepsTheServerPosition")
    {
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "LateStopScriptPlan");

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

        vector<mdir> move_steps {hdir::East, hdir::East, hdir::East};
        vector<uint16_t> control_steps {3};
        server->StartCritterMoving(cr, uint16_t {400}, move_steps, control_steps, ipos16 {}, nullptr);
        REQUIRE(cr->IsMoving());

        auto moving = cr->GetMoving();
        REQUIRE(static_cast<bool>(moving));
        mpos end_hex = moving->GetEndHex();

        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&server, &cr] {
            auto ctx = server->RequireCurrentSyncContext();
            ctx->SyncEntity(cr);
            return !cr->IsMoving();
        }));

        SendStopCritterMove(test_connection, server, map->GetId(), cr->GetId(), first_step_hex, ipos16 {}, mdir {});

        bool walked_back = WaitForUnlockedServerCondition(
            server, server_locked,
            [&server, &cr, first_step_hex] {
                auto ctx = server->RequireCurrentSyncContext();
                ctx->SyncEntity(cr);
                return cr->GetHex() == first_step_hex;
            },
            std::chrono::milliseconds {300});

        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);

        CHECK_FALSE(walked_back);
        CHECK(cr->GetHex() == end_hex);

        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    }
}

TEST_CASE("ServerAttachedMasterMovementAuthority")
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
    auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
    auto player = CreateLoggedPlayer(server, test_connection, "AttachedMasterMovement");
    auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(static_cast<bool>(map));

    auto chosen = server->CreateCritter(fn("TestCritter"), true);
    auto master = server->CreateCritter(fn("TestCritter"), false);
    auto stranger = server->CreateCritter(fn("TestCritter"), true);
    server->MapMngr.TransferToMap(chosen, map, mpos {19, 20}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(master, map, mpos {20, 20}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(stranger, map, mpos {40, 40}, mdir {}, std::nullopt);
    server->SwitchPlayerCritter(player, chosen);
    chosen->AttachToCritter(master);
    REQUIRE(player->GetControlledCritter() == chosen.get());

    int32_t move_calls = 0;
    int32_t dir_calls = 0;
    function<Entity::EventResult()> on_move = [] { return Entity::EventResult::ContinueChain; };
    Entity::EventCallbackData move_callback;
    move_callback.Callback = [&](FuncCallData&) {
        move_calls++;
        return on_move();
    };
    move_callback.SubscriptionPtr = reinterpret_cast<uintptr_t>(&move_calls);
    server->OnPlayerMoveCritter.Subscribe(std::move(move_callback));
    Entity::EventCallbackData dir_callback;
    dir_callback.Callback = [&](FuncCallData&) {
        dir_calls++;
        return Entity::EventResult::ContinueChain;
    };
    dir_callback.SubscriptionPtr = reinterpret_cast<uintptr_t>(&dir_calls);
    server->OnPlayerDirCritter.Subscribe(std::move(dir_callback));
    auto unsubscribe = scope_exit([&]() noexcept {
        server->OnPlayerMoveCritter.Unsubscribe(reinterpret_cast<uintptr_t>(&move_calls));
        server->OnPlayerDirCritter.Unsubscribe(reinterpret_cast<uintptr_t>(&dir_calls));
    });

    auto sync_fixture = [&] {
        auto ctx = server->RequireCurrentSyncContext();
        small_vector<ptr<ServerEntity>, 6> entities {player, chosen, master, stranger, map, loc};
        ctx->SyncEntities(entities);
    };
    auto wait_move_calls = [&](int32_t count) {
        return WaitForUnlockedServerCondition(server, server_locked, [&] {
            sync_fixture();
            return move_calls >= count;
        });
    };
    auto start_master_move = [&](uint16_t speed) {
        server->StartCritterMoving(master, speed, vector<mdir> {hdir::East, hdir::East, hdir::East}, vector<uint16_t> {3}, ipos16 {}, player);
        REQUIRE(master->IsMoving());
    };
    auto start_master_lease = [&] {
        auto plan = safe_alloc::make_refcounted<MovingContext>(map->GetSize(), uint16_t {1}, vector<mdir> {hdir::East, hdir::East, hdir::East}, vector<uint16_t> {3}, server->GameTime.GetFrameTime(), timespan {}, master->GetHex(), ipos16 {}, ipos16 {});
        plan->SetLeaseTime(timespan {std::chrono::duration<float64_t, std::milli> {200.0f}}, server->GameTime.GetFrameTime());
        server->StartCritterMoving(master, std::move(plan), player, 7);
        REQUIRE(master->IsMoving());
    };

    SECTION("AttachedChosenReplacesItsMovingMastersPlan")
    {
        start_master_move(uint16_t {1});
        uint32_t previous_uid = master->GetMovingUid();
        SendCritterMove(test_connection, server, master, uint16_t {2});
        REQUIRE(wait_move_calls(1));
        REQUIRE(master->IsMoving());
        CHECK(master->GetMovingUid() != previous_uid);
        CHECK(master->GetMoving()->GetSpeed() == 2);
        CHECK(player->GetControlledCritter() == chosen.get());
    }

    SECTION("AttachedChosenRenewsItsMastersLease")
    {
        start_master_lease();
        SendCritterMoveLease(test_connection, server, map->GetId(), master->GetId(), 7, 1000.0f, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        CHECK(master->GetMoving()->GetLeaseTime() == timespan {std::chrono::duration<float64_t, std::milli> {1000.0f}});
        CHECK(player->GetControlledCritter() == chosen.get());
    }

    SECTION("ScriptCanRefuseMastersLease")
    {
        start_master_lease();
        on_move = [] { return Entity::EventResult::StopChain; };
        SendCritterMoveLease(test_connection, server, map->GetId(), master->GetId(), 7, 1000.0f, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        CHECK(master->GetMoving()->GetLeaseTime() == timespan {std::chrono::duration<float64_t, std::milli> {200.0f}});
    }

    SECTION("DetachmentDuringLeasePermissionCheckInvalidatesTheRenewal")
    {
        start_master_lease();
        on_move = [&] {
            chosen->DetachFromCritter();
            return Entity::EventResult::ContinueChain;
        };
        SendCritterMoveLease(test_connection, server, map->GetId(), master->GetId(), 7, 1000.0f, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        CHECK(master->GetMoving()->GetLeaseTime() == timespan {std::chrono::duration<float64_t, std::milli> {200.0f}});
        CHECK_FALSE(chosen->GetIsAttached());
    }

    SECTION("ControllerSwitchDuringLeasePermissionCheckInvalidatesTheRenewal")
    {
        start_master_lease();
        on_move = [&] {
            sync_fixture();
            server->SwitchPlayerCritter(player, stranger);
            return Entity::EventResult::ContinueChain;
        };
        SendCritterMoveLease(test_connection, server, map->GetId(), master->GetId(), 7, 1000.0f, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        CHECK(master->GetMoving()->GetLeaseTime() == timespan {std::chrono::duration<float64_t, std::milli> {200.0f}});
        CHECK(player->GetControlledCritter() == stranger.get());
    }

    SECTION("ReplacementPlanDuringLeasePermissionCheckIsNotRenewed")
    {
        start_master_lease();
        on_move = [&] {
            start_master_move(uint16_t {1});
            return Entity::EventResult::ContinueChain;
        };
        SendCritterMoveLease(test_connection, server, map->GetId(), master->GetId(), 7, 1000.0f, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        CHECK_FALSE(master->GetPlayerPlanMoving(7));
        CHECK(master->GetMoving()->GetLeaseTime() == timespan::zero);
    }

    SECTION("AttachedChosenStartsTurnsAndStopsItsMaster")
    {
        SendCritterMove(test_connection, server, master, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        REQUIRE(master->IsMoving());
        CHECK_FALSE(chosen->IsMoving());
        CHECK(player->GetControlledCritter() == chosen.get());

        mdir dir {hdir::NorthEast};
        SendCritterDir(test_connection, server, master, dir);
        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&] {
            sync_fixture();
            return dir_calls == 1;
        }));
        CHECK(master->GetDir() == dir);

        SendCritterStop(test_connection, server, master);
        REQUIRE(wait_move_calls(2));
        CHECK_FALSE(master->IsMoving());
        CHECK(chosen->GetIsAttached());
    }

    SECTION("AttachedChosenReconcilesItsMastersArrival")
    {
        PlantConnectionRoundTrip(player, server, timespan {std::chrono::milliseconds {800}});
        start_master_move(uint16_t {400});
        mpos end_hex = master->GetMoving()->GetEndHex();
        mpos first_hex = master->GetHex();
        REQUIRE(GeometryHelper::MoveHexByDir(first_hex, hdir::East, map->GetSize()));
        SendCritterMoveFinished(test_connection, server, map->GetId(), master->GetId(), end_hex, first_hex, ipos16 {}, mdir {});
        REQUIRE(wait_move_calls(1));
        CHECK_FALSE(master->IsMoving());
        CHECK(master->GetHex() == first_hex);
        CHECK(chosen->GetHex() == first_hex);
    }

    SECTION("ScriptCanRefuseMastersArrival")
    {
        on_move = [] { return Entity::EventResult::StopChain; };
        PlantConnectionRoundTrip(player, server, timespan {std::chrono::milliseconds {800}});
        start_master_move(uint16_t {400});
        mpos end_hex = master->GetMoving()->GetEndHex();
        auto start_hex = master->GetHex();
        SendCritterMoveFinished(test_connection, server, map->GetId(), master->GetId(), end_hex, start_hex, ipos16 {}, mdir {});
        REQUIRE(wait_move_calls(1));
        CHECK(master->IsMoving());
        CHECK(master->GetHex() == start_hex);
    }

    SECTION("ScriptCanRefuseMastersMoveAndStop")
    {
        on_move = [] { return Entity::EventResult::StopChain; };
        SendCritterMove(test_connection, server, master, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        CHECK_FALSE(master->IsMoving());

        start_master_move(uint16_t {1});
        SendCritterStop(test_connection, server, master);
        REQUIRE(wait_move_calls(2));
        CHECK(master->IsMoving());
    }

    SECTION("DetachmentDuringPermissionCheckInvalidatesTheMove")
    {
        on_move = [&] {
            chosen->DetachFromCritter();
            return Entity::EventResult::ContinueChain;
        };
        SendCritterMove(test_connection, server, master, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        CHECK_FALSE(master->IsMoving());
        CHECK_FALSE(chosen->GetIsAttached());
    }

    SECTION("ControllerSwitchDuringPermissionCheckInvalidatesTheMove")
    {
        on_move = [&] {
            sync_fixture();
            server->SwitchPlayerCritter(player, stranger);
            return Entity::EventResult::ContinueChain;
        };
        SendCritterMove(test_connection, server, master, uint16_t {1});
        REQUIRE(wait_move_calls(1));
        CHECK_FALSE(master->IsMoving());
        CHECK(player->GetControlledCritter() == stranger.get());
    }

    SECTION("AnUnrelatedCritterNeverReachesScriptPermissionChecks")
    {
        auto stranger_plan = safe_alloc::make_refcounted<MovingContext>(map->GetSize(), uint16_t {1}, vector<mdir> {hdir::East}, vector<uint16_t> {1}, server->GameTime.GetFrameTime(), timespan {}, stranger->GetHex(), ipos16 {}, ipos16 {});
        stranger_plan->SetLeaseTime(timespan {std::chrono::duration<float64_t, std::milli> {200.0f}}, server->GameTime.GetFrameTime());
        server->StartCritterMoving(stranger, std::move(stranger_plan), player, 7);
        SendCritterMoveLease(test_connection, server, map->GetId(), stranger->GetId(), 7, 1000.0f, uint16_t {1});
        mdir dir {hdir::NorthEast};
        SendCritterMove(test_connection, server, stranger, uint16_t {1});
        SendCritterDir(test_connection, server, stranger, dir);
        SendCritterDir(test_connection, server, chosen, dir);
        REQUIRE(WaitForUnlockedServerCondition(server, server_locked, [&] {
            sync_fixture();
            return dir_calls == 1;
        }));
        CHECK(move_calls == 0);
        CHECK(stranger->GetMoving()->GetLeaseTime() == timespan {std::chrono::duration<float64_t, std::milli> {200.0f}});
        CHECK(stranger->GetDir() != dir);
        CHECK(chosen->GetDir() == dir);
        server->StopCritterMoving(stranger);
    }

    sync_fixture();
    server->StopCritterMoving(master);
    if (chosen->GetIsAttached()) {
        chosen->DetachFromCritter();
    }
    server->SwitchPlayerCritter(player, nullptr);
    chosen->UnmarkIsForPlayer();
    stranger->UnmarkIsForPlayer();
    server->CrMngr.DestroyCritter(chosen);
    server->CrMngr.DestroyCritter(master);
    server->CrMngr.DestroyCritter(stranger);
    server->MapMngr.DestroyLocation(loc);
}

TEST_CASE("CritterAttachmentDestruction")
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
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };
    auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
    auto player = CreateLoggedPlayer(server, test_connection, "AttachmentDestruction");
    auto master = server->CreateCritter(fn("TestCritter"), false).hold_ref();
    auto passenger = server->CreateCritter(fn("TestCritter"), true).hold_ref();
    auto other_passenger = server->CreateCritter(fn("TestCritter"), false).hold_ref();
    auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(map);
    server->MapMngr.TransferToMap(master, map, mpos {20, 20}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(passenger, map, mpos {19, 20}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(other_passenger, map, mpos {21, 20}, mdir {}, std::nullopt);
    server->SwitchPlayerCritter(player, passenger);
    (void)master->AddVisibleCritter(passenger, CritterVisibilityMode::Full);
    (void)other_passenger->AddVisibleCritter(passenger, CritterVisibilityMode::Full);

    for (ptr<Critter> member : {passenger.as_ptr(), other_passenger.as_ptr()}) {
        member->AttachToCritter(master);
    }
    test_connection->Dispatch();
    test_connection->ClearReceivedCritterMessages();

    std::atomic<size_t> error_count {};
    logging::set_callback("critter-attachment-destruction-test", [&](logging::type type, string_view, nptr<const stack_trace::catched_data>) {
        if (type == logging::type::error) {
            error_count.fetch_add(1, std::memory_order_relaxed);
        }
    });
    auto remove_callback = scope_exit([]() noexcept { logging::set_callback("critter-attachment-destruction-test", {}); });

    SECTION("MasterDestroyedBeforePassengers")
    {
        CHECK_NOTHROW(server->CrMngr.DestroyCritter(master));
        CHECK(master->IsDestroyed());
        CHECK_FALSE(server->EntityMngr.GetCritter(master->GetId()));
        CHECK_FALSE(passenger->GetIsAttached());
        CHECK_FALSE(passenger->GetAttachMaster());
        CHECK_FALSE(other_passenger->GetIsAttached());
        CHECK_FALSE(other_passenger->GetAttachMaster());
        CHECK_FALSE(passenger->IsDestroyed());
        CHECK_FALSE(other_passenger->IsDestroyed());
        CHECK(player->GetControlledCritter() == passenger.get());
        test_connection->Dispatch();
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterAttachments, master->GetId()) == 0);
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterAttachments, passenger->GetId()) >= 1);
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterAttachments, other_passenger->GetId()) >= 1);
        CHECK(error_count.load(std::memory_order_relaxed) == 0);
        server->CrMngr.DestroyCritter(other_passenger);
    }

    SECTION("PassengerDestroyedBeforeMaster")
    {
        CHECK_NOTHROW(server->CrMngr.DestroyCritter(other_passenger));
        CHECK(other_passenger->IsDestroyed());
        REQUIRE(master->GetAttachedCritters().size() == 1);
        CHECK(master->GetAttachedCritters().front() == passenger.get());
        CHECK(passenger->GetAttachMaster() == master->GetId());
        test_connection->Dispatch();
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterAttachments, master->GetId()) >= 1);
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterAttachments, other_passenger->GetId()) == 0);
        CHECK(error_count.load(std::memory_order_relaxed) == 0);
        test_connection->ClearReceivedCritterMessages();
        CHECK_NOTHROW(server->CrMngr.DestroyCritter(master));
        CHECK(master->IsDestroyed());
        CHECK_FALSE(passenger->GetIsAttached());
        CHECK_FALSE(passenger->IsDestroyed());
        test_connection->Dispatch();
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterAttachments, master->GetId()) == 0);
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterAttachments, passenger->GetId()) >= 1);
        CHECK(error_count.load(std::memory_order_relaxed) == 0);
    }

    server->SwitchPlayerCritter(player, nullptr);
    passenger->UnmarkIsForPlayer();
    server->CrMngr.DestroyCritter(passenger);
    server->MapMngr.DestroyLocation(loc);
    CHECK(error_count.load(std::memory_order_relaxed) == 0);
}

// A rider's client rebuilds who sits with it from the master's CritterAttachments frames, so a real local-to-global
// transfer must leave both riders holding one listing exactly their two ids and leave other groups deaf to it
TEST_CASE("CritterGlobalAttachmentSnapshotsReachOnlyTheirGroupOnce")
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
    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };

    auto driver_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
    auto passenger_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
    auto outsider_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
    auto driver_player = CreateLoggedPlayer(server, driver_connection, "GlobalAttachmentDriver");
    auto passenger_player = CreateLoggedPlayer(server, passenger_connection, "GlobalAttachmentPassenger");
    auto outsider_player = CreateLoggedPlayer(server, outsider_connection, "GlobalAttachmentOutsider");

    auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
    auto map = loc->GetMapByIndex(0);
    REQUIRE(static_cast<bool>(map));

    auto master = server->CreateCritter(fn("TestCritter"), false).hold_ref();
    auto driver = server->CreateCritter(fn("TestCritter"), true).hold_ref();
    auto passenger = server->CreateCritter(fn("TestCritter"), true).hold_ref();
    auto outsider = server->CreateCritter(fn("TestCritter"), true).hold_ref();

    server->MapMngr.TransferToMap(master, map, mpos {20, 20}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(driver, map, mpos {19, 20}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(passenger, map, mpos {21, 20}, mdir {}, std::nullopt);
    server->MapMngr.TransferToMap(outsider, map, mpos {40, 40}, mdir {}, std::nullopt);

    server->SwitchPlayerCritter(driver_player, driver);
    server->SwitchPlayerCritter(passenger_player, passenger);
    server->SwitchPlayerCritter(outsider_player, outsider);

    auto sync_fixture = [&] {
        auto ctx = server->RequireCurrentSyncContext();
        ptr<Critter> master_ref = master;
        ptr<Critter> driver_ref = driver;
        ptr<Critter> passenger_ref = passenger;
        ptr<Critter> outsider_ref = outsider;
        small_vector<ptr<ServerEntity>, 10> entities {driver_player, passenger_player, outsider_player, master_ref, driver_ref, passenger_ref, outsider_ref, map, loc};
        ctx->SyncEntities(entities);
    };

    sync_fixture();
    driver->AttachToCritter(master);
    passenger->AttachToCritter(master);

    // Outsider leaves for a global group of its own first, so it keeps no map-visibility path into the tested
    // transfer and stays a real third player in a different group
    server->MapMngr.TransferToGlobal(outsider, ident_t {});

    auto flush_observations = [&] {
        driver_connection->Dispatch();
        passenger_connection->Dispatch();
        outsider_connection->Dispatch();
    };
    auto clear_observations = [&] {
        flush_observations();

        driver_connection->ClearReceivedCritterMessages();
        driver_connection->ClearReceivedAttachmentSnapshots();
        passenger_connection->ClearReceivedCritterMessages();
        passenger_connection->ClearReceivedAttachmentSnapshots();
        outsider_connection->ClearReceivedCritterMessages();
        outsider_connection->ClearReceivedAttachmentSnapshots();
    };
    auto latest_attachment = [](const vector<CritterAttachmentSnapshot>& snapshots, ident_t cr_id) -> optional<CritterAttachmentSnapshot> {
        for (size_t i = snapshots.size(); i > 0; i--) {
            const CritterAttachmentSnapshot& snapshot = snapshots[i - 1];

            if (snapshot.CrId == cr_id) {
                return snapshot;
            }
        }

        return std::nullopt;
    };
    auto attachment_ids_match = [](const vector<ident_t>& actual, const vector<ident_t>& expected) -> bool {
        if (actual.size() != expected.size()) {
            return false;
        }

        return std::ranges::all_of(expected, [&actual](ident_t id) { return std::ranges::find(actual, id) != actual.end(); });
    };
    auto count_attachments = [](const vector<CritterAttachmentSnapshot>& snapshots, ident_t cr_id) -> size_t { return numeric_cast<size_t>(std::ranges::count_if(snapshots, [cr_id](const CritterAttachmentSnapshot& snapshot) { return snapshot.CrId == cr_id; })); };

    // Oracles are the fixture's own actor ids, never the critters' current attachment state
    ident_t master_id = master->GetId();
    ident_t driver_id = driver->GetId();
    ident_t passenger_id = passenger->GetId();
    vector<ident_t> expected_full_master_ids {driver_id, passenger_id};
    vector<ident_t> expected_detached_master_ids {driver_id};

    // A known nonempty packet validates the independent decoder before missing broadcasts can satisfy the RED
    clear_observations();
    driver_player->Send_Attachments(master);
    flush_observations();
    auto known_snapshot = latest_attachment(driver_connection->GetReceivedAttachmentSnapshots(), master_id);
    REQUIRE(known_snapshot.has_value());
    REQUIRE(attachment_ids_match(known_snapshot->AttachedIds, expected_full_master_ids));

    std::atomic<size_t> error_count {};
    logging::set_callback("critter-global-attachment-test", [&](logging::type type, string_view, nptr<const stack_trace::catched_data>) {
        if (type == logging::type::error) {
            error_count.fetch_add(1, std::memory_order_relaxed);
        }
    });
    auto remove_callback = scope_exit([]() noexcept { logging::set_callback("critter-global-attachment-test", {}); });

    clear_observations();
    server->MapMngr.TransferToGlobal(master, ident_t {});
    flush_observations();

    // The master's own snapshot is the riders' whole model of who sits with them, so the transfer window must close
    // with one whose attached list is exactly their two ids
    SECTION("TransferLeavesBothRidersHoldingTheFullMasterList")
    {
        auto expect_full_master_list = [&](const vector<CritterAttachmentSnapshot>& snapshots) {
            optional<CritterAttachmentSnapshot> master_snapshot = latest_attachment(snapshots, master_id);

            CHECK(master_snapshot.has_value());

            if (master_snapshot.has_value()) {
                CHECK(attachment_ids_match(master_snapshot->AttachedIds, expected_full_master_ids));
            }
        };

        expect_full_master_list(driver_connection->GetReceivedAttachmentSnapshots());
        expect_full_master_list(passenger_connection->GetReceivedAttachmentSnapshots());
        CHECK(outsider_connection->GetReceivedAttachmentSnapshots().empty());
    }

    // A global detach publishes two states: the master's list narrowed to the rider still aboard and the child's
    // own cleared attachment, and both must reach the riders still sharing the group
    SECTION("GlobalDetachNarrowsTheMasterListAndClearsTheChild")
    {
        clear_observations();
        passenger->DetachFromCritter();
        flush_observations();

        auto expect_detach_state = [&](const vector<CritterAttachmentSnapshot>& snapshots) {
            optional<CritterAttachmentSnapshot> master_snapshot = latest_attachment(snapshots, master_id);
            optional<CritterAttachmentSnapshot> passenger_snapshot = latest_attachment(snapshots, passenger_id);

            CHECK(master_snapshot.has_value());

            if (master_snapshot.has_value()) {
                CHECK(attachment_ids_match(master_snapshot->AttachedIds, expected_detached_master_ids));
            }

            CHECK(passenger_snapshot.has_value());

            if (passenger_snapshot.has_value()) {
                CHECK_FALSE(passenger_snapshot->IsAttached);
                CHECK(passenger_snapshot->MasterId == ident_t {});
            }
        };

        expect_detach_state(driver_connection->GetReceivedAttachmentSnapshots());
        expect_detach_state(passenger_connection->GetReceivedAttachmentSnapshots());
        CHECK(outsider_connection->GetReceivedAttachmentSnapshots().empty());
    }

    // An explicit rider broadcast is one snapshot per real recipient: its own connection exactly once, its group peer
    // once and never a second copy back through the peer path
    SECTION("ExplicitRiderSendArrivesOnceAtItsOwnConnectionAndItsPeer")
    {
        clear_observations();
        driver->SendAndBroadcast_Attachments();
        flush_observations();

        vector<CritterAttachmentSnapshot> driver_frames = driver_connection->GetReceivedAttachmentSnapshots();
        vector<CritterAttachmentSnapshot> passenger_frames = passenger_connection->GetReceivedAttachmentSnapshots();

        CHECK(count_attachments(driver_frames, driver_id) == 1);
        CHECK(driver_frames.size() == 1);
        CHECK(count_attachments(passenger_frames, driver_id) == 1);
        CHECK(passenger_frames.size() == 1);
        CHECK(outsider_connection->GetReceivedAttachmentSnapshots().empty());
    }

    SECTION("GlobalSnapshotsRespectBoundedMapViewAndResumeAfterReturn")
    {
        sync_fixture();
        Server_Player_ViewMapArea(passenger_player, map, mpos {20, 20}, 5, nullptr);
        clear_observations();
        master->SendAndBroadcast_Attachments();
        flush_observations();
        CHECK(count_attachments(driver_connection->GetReceivedAttachmentSnapshots(), master_id) == 1);
        CHECK(passenger_connection->GetReceivedAttachmentSnapshots().empty());
        CHECK(outsider_connection->GetReceivedAttachmentSnapshots().empty());

        clear_observations();
        passenger->SendAndBroadcast_Attachments();
        flush_observations();
        CHECK(count_attachments(driver_connection->GetReceivedAttachmentSnapshots(), passenger_id) == 1);
        CHECK(passenger_connection->GetReceivedAttachmentSnapshots().empty());
        CHECK(outsider_connection->GetReceivedAttachmentSnapshots().empty());

        driver->DetachFromCritter();
        clear_observations();
        Server_Player_ReturnFromViewMap(passenger_player);
        Server_Critter_SendGlobalMapGroupInfo(passenger);
        flush_observations();

        for (ident_t id : vector<ident_t> {master_id, driver_id, passenger_id}) {
            CHECK(passenger_connection->CountReceivedCritterMessage(NetMessage::AddCritter, id) == 1);
        }

        auto resumed_master = latest_attachment(passenger_connection->GetReceivedAttachmentSnapshots(), master_id);
        CHECK(resumed_master.has_value());
        if (resumed_master) {
            CHECK(attachment_ids_match(resumed_master->AttachedIds, vector<ident_t> {passenger_id}));
        }

        auto resumed_driver = latest_attachment(passenger_connection->GetReceivedAttachmentSnapshots(), driver_id);
        CHECK(resumed_driver.has_value());
        if (resumed_driver) {
            CHECK_FALSE(resumed_driver->IsAttached);
            CHECK(resumed_driver->MasterId == ident_t {});
        }

        auto resumed_passenger = latest_attachment(passenger_connection->GetReceivedAttachmentSnapshots(), passenger_id);
        CHECK(resumed_passenger.has_value());
        if (resumed_passenger) {
            CHECK(resumed_passenger->IsAttached);
            CHECK(resumed_passenger->MasterId == master_id);
        }

        vector<ident_t> added_ids;
        for (const auto& [message, id] : passenger_connection->GetReceivedCritterMessages()) {
            if (message == NetMessage::AddCritter) {
                added_ids.emplace_back(id);
            }
            else if (message == NetMessage::CritterAttachments) {
                CHECK(std::ranges::find(added_ids, id) != added_ids.end());
            }
        }

        clear_observations();
        master->SendAndBroadcast_Attachments();
        flush_observations();
        CHECK(count_attachments(driver_connection->GetReceivedAttachmentSnapshots(), master_id) == 1);
        CHECK(count_attachments(passenger_connection->GetReceivedAttachmentSnapshots(), master_id) == 1);
        CHECK(outsider_connection->GetReceivedAttachmentSnapshots().empty());
    }

    SECTION("QueuedMapContextFollowsTheConnectionAcrossSwapAndRollback")
    {
        auto reconnect_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto reconnect_player = server->CreateNotLoggedInPlayer(reconnect_connection);
        clear_observations();
        driver_player->SwapConnection(reconnect_player);
        master->SendAndBroadcast_Attachments();
        reconnect_connection->Dispatch();
        CHECK(reconnect_connection->GetReceivedAttachmentSnapshots().empty());

        driver_player->Send_LoadMap(nullptr);
        reconnect_connection->ClearReceivedAttachmentSnapshots();
        master->SendAndBroadcast_Attachments();
        reconnect_connection->Dispatch();
        CHECK(count_attachments(reconnect_connection->GetReceivedAttachmentSnapshots(), master_id) == 1);

        driver_player->Send_LoadMap(map);
        driver_player->SwapConnection(reconnect_player);
        clear_observations();
        master->SendAndBroadcast_Attachments();
        flush_observations();
        CHECK(count_attachments(driver_connection->GetReceivedAttachmentSnapshots(), master_id) == 1);
        CHECK(outsider_connection->GetReceivedAttachmentSnapshots().empty());
        reconnect_player->MarkAsDestroyed();
    }

    SECTION("PinnedRecipientCannotReceiveTheOldGroupAfterSwitchingCritter")
    {
        auto replacement = server->CreateCritter(fn("TestCritter"), true).hold_ref();
        auto pending_player = passenger->GetPlayerForSend();
        REQUIRE(pending_player);
        server->SwitchPlayerCritter(passenger_player, replacement);
        clear_observations();

        // Models a recipient pinned before the switch but queued only after the new controller is installed
        pending_player->Send_Attachments(master, passenger);
        flush_observations();
        CHECK(passenger_connection->GetReceivedAttachmentSnapshots().empty());

        pending_player->Send_Attachments(replacement, replacement);
        flush_observations();
        CHECK(count_attachments(passenger_connection->GetReceivedAttachmentSnapshots(), replacement->GetId()) == 1);

        server->SwitchPlayerCritter(passenger_player, passenger);
        replacement->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(replacement);
    }

    sync_fixture();

    if (driver->GetIsAttached()) {
        driver->DetachFromCritter();
    }
    if (passenger->GetIsAttached()) {
        passenger->DetachFromCritter();
    }

    server->SwitchPlayerCritter(driver_player, nullptr);
    server->SwitchPlayerCritter(passenger_player, nullptr);
    server->SwitchPlayerCritter(outsider_player, nullptr);
    driver->UnmarkIsForPlayer();
    passenger->UnmarkIsForPlayer();
    outsider->UnmarkIsForPlayer();
    server->CrMngr.DestroyCritter(driver);
    server->CrMngr.DestroyCritter(passenger);
    server->CrMngr.DestroyCritter(outsider);
    server->CrMngr.DestroyCritter(master);
    server->MapMngr.DestroyLocation(loc);
    CHECK(error_count.load(std::memory_order_relaxed) == 0);
}

TEST_CASE("ServerMovementTransportClock")
{
    auto settings = MakeSettings();
    REQUIRE(settings.Network.NetBufferSize > 0);

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
    REQUIRE_FALSE(server->GameTime.IsPaused());
    REQUIRE(server->IsConnectionAdmissionOpen());
    REQUIRE_FALSE(server->GetCurrentSyncContext());

    bool callback_invoked = false;
    bool quiesced = server->RunInQuiescence(timespan {std::chrono::seconds {10}}, [&](const ServerQuiescenceState&) {
        callback_invoked = true;

        CHECK(server->GameTime.IsPaused());
        CHECK_FALSE(server->IsConnectionAdmissionOpen());

        auto ctx = server->RequireCurrentSyncContext();
        auto fn = [&server](string_view name) { return server->Hashes.to_hashed_string(name); };
        auto test_connection = safe_alloc::make_shared<MoveReconciliationConnection>(server->Settings);
        auto player = CreateLoggedPlayer(server, test_connection, "MovementTransportClock");
        auto loc = server->MapMngr.CreateLocation(fn("TestLocation"), vector<hstring> {fn("TestMap")});
        auto map = loc->GetMapByIndex(0);
        REQUIRE(static_cast<bool>(map));

        auto cr = server->CreateCritter(fn("TestCritter"), true);
        mpos start_hex {20, 20};
        server->MapMngr.TransferToMap(cr, map, start_hex, mdir {}, std::nullopt);
        server->SwitchPlayerCritter(player, cr);
        REQUIRE(player->GetControlledCritter() == cr.get());

        small_vector<ptr<ServerEntity>, 4> sync_entities {player, cr, map, loc};
        ctx->SyncEntities(sync_entities);

        vector<mdir> move_steps(12, mdir {hdir::East});
        vector<uint16_t> control_steps {12};
        auto moving = safe_alloc::make_refcounted<MovingContext>(map->GetSize(), uint16_t {300}, move_steps, control_steps, server->GameTime.GetFrameTime(), timespan {std::chrono::milliseconds {100}}, start_hex, ipos16 {}, ipos16 {});
        moving->SetLeaseTime(timespan {std::chrono::milliseconds {200}}, server->GameTime.GetFrameTime());
        server->StartCritterMoving(cr, moving, player, 7);
        REQUIRE(cr->IsMoving());

        uint32_t expected_moving_uid = cr->GetMovingUid();
        int32_t expected_whole_ms = iround<int32_t>(std::ceil(moving->GetWholeTime().to_ms<float64_t>()));

        test_connection->Dispatch();
        test_connection->ClearReceivedTransportPackets();
        test_connection->ClearReceivedCritterMessages();
        test_connection->ClearReceivedAttachmentSnapshots();

        nanotime frozen_frame = server->GameTime.GetFrameTime();
        nanotime sleep_started = nanotime::now();
        coarse_sleep(std::chrono::milliseconds {250});
        nanotime sleep_finished = nanotime::now();
        server->GameTime.FrameAdvance(true);

        REQUIRE(server->GameTime.GetFrameTime() == frozen_frame);
        REQUIRE(sleep_finished - sleep_started >= std::chrono::milliseconds {250});
        REQUIRE(sleep_finished - frozen_frame >= std::chrono::milliseconds {250});

        auto check_sender_stamp = [&](const TransportPacketSnapshot& packet, nanotime before, nanotime after) {
            CHECK(numeric_cast<size_t>(packet.FrameBytes) <= numeric_cast<size_t>(settings.Network.NetBufferSize));
            CHECK(packet.SenderMs >= before.milliseconds());
            CHECK(packet.SenderMs <= after.milliseconds());
            CHECK(packet.SenderMs > frozen_frame.milliseconds());
        };

        nanotime ping_before = nanotime::now();
        player->Send_Ping(false);
        nanotime ping_after = nanotime::now();
        nanotime move_before = nanotime::now();
        player->Send_Moving(cr);
        nanotime move_after = nanotime::now();
        nanotime lease_before = nanotime::now();
        player->Send_MovingLease(cr);
        nanotime lease_after = nanotime::now();
        test_connection->Dispatch();

        vector<TransportPacketSnapshot> packets = test_connection->GetReceivedTransportPackets();
        REQUIRE(packets.size() == 3);
        REQUIRE(packets[0].Message == NetMessage::Ping);
        REQUIRE(packets[1].Message == NetMessage::CritterMove);
        REQUIRE(packets[2].Message == NetMessage::CritterMoveLease);
        CHECK(test_connection->CountReceivedCritterMessage(NetMessage::CritterMove, cr->GetId()) == 1);

        CHECK_FALSE(packets[0].PingAnswer);
        check_sender_stamp(packets[0], ping_before, ping_after);

        CHECK(packets[1].CrId == cr->GetId());
        CHECK(packets[1].WholeMs == expected_whole_ms);
        CHECK(packets[1].ElapsedMs == 100);
        CHECK(packets[1].Speed == 300);
        CHECK(packets[1].StartHex == start_hex);
        REQUIRE(packets[1].Steps.size() == move_steps.size());
        CHECK(std::ranges::all_of(packets[1].Steps, [](hdir step) { return step == hdir::East; }));
        REQUIRE(packets[1].ControlSteps.size() == control_steps.size());
        CHECK(packets[1].ControlSteps == control_steps);
        CHECK(packets[1].EndHexOffset == ipos16 {});
        CHECK(packets[1].MovingUid == expected_moving_uid);
        CHECK(packets[1].LeaseDistance == 60.0f);
        check_sender_stamp(packets[1], move_before, move_after);

        CHECK(packets[2].CrId == cr->GetId());
        CHECK(packets[2].MovingUid == expected_moving_uid);
        CHECK(packets[2].LeaseDistance == 60.0f);
        check_sender_stamp(packets[2], lease_before, lease_after);

        TransportPacketSnapshot expected_move = packets[1];
        TransportPacketSnapshot expected_lease = packets[2];

        timespan catch_up_min = std::chrono::milliseconds {server->Settings->Network.MoveLateCatchUpMinMs};
        timespan catch_up_max = std::chrono::milliseconds {server->Settings->Network.MoveLateCatchUpMaxMs};
        auto make_estimator = [&server] {
            return LinkDelayEstimator {std::chrono::milliseconds {server->Settings->Network.LinkDelayWindowMs}, std::chrono::milliseconds {server->Settings->Network.MoveLateCatchUpMinMs}, std::chrono::milliseconds {server->Settings->Network.LinkDelayRebaseMs}};
        };
        auto evaluate_catch_up = [&](timespan late) { return EvaluateLateCatchUp(late, catch_up_min, catch_up_max); };
        auto check_move_payload = [&](const TransportPacketSnapshot& actual, const TransportPacketSnapshot& expected) {
            CHECK(actual.Message == NetMessage::CritterMove);
            CHECK(actual.CrId == expected.CrId);
            CHECK(actual.WholeMs == expected.WholeMs);
            CHECK(actual.ElapsedMs == expected.ElapsedMs);
            CHECK(actual.ElapsedMs == 100);
            CHECK(actual.Speed == expected.Speed);
            CHECK(actual.StartHex == expected.StartHex);
            CHECK(std::ranges::equal(actual.Steps, expected.Steps));
            CHECK(actual.ControlSteps == expected.ControlSteps);
            CHECK(actual.EndHexOffset == expected.EndHexOffset);
            CHECK(actual.MovingUid == expected.MovingUid);
            CHECK(actual.LeaseDistance == expected.LeaseDistance);
        };
        auto check_lease_payload = [&](const TransportPacketSnapshot& actual, const TransportPacketSnapshot& expected) {
            CHECK(actual.Message == NetMessage::CritterMoveLease);
            CHECK(actual.CrId == expected.CrId);
            CHECK(actual.MovingUid == expected.MovingUid);
            CHECK(actual.LeaseDistance == expected.LeaseDistance);
            CHECK(actual.LeaseDistance == 60.0f);
        };
        auto capture_fresh_ping = [&]() -> TransportPacketSnapshot {
            test_connection->Dispatch();
            test_connection->ClearReceivedTransportPackets();

            player->Send_Ping(false);
            test_connection->Dispatch();

            vector<TransportPacketSnapshot> ping_packets = test_connection->GetReceivedTransportPackets();
            REQUIRE(ping_packets.size() == 1);
            REQUIRE(ping_packets.front().Message == NetMessage::Ping);

            return ping_packets.front();
        };

        LinkDelayEstimator immediate_estimator = make_estimator();
        TransportPacketSnapshot baseline_ping = capture_fresh_ping();
        (void)immediate_estimator.AddSample(baseline_ping.SenderMs, baseline_ping.ReceiveTime);
        test_connection->ClearReceivedTransportPackets();

        move_before = nanotime::now();
        player->Send_Moving(cr);
        move_after = nanotime::now();
        lease_before = nanotime::now();
        player->Send_MovingLease(cr);
        lease_after = nanotime::now();
        test_connection->Dispatch();
        packets = test_connection->GetReceivedTransportPackets();
        REQUIRE(packets.size() == 2);
        REQUIRE(packets[0].Message == NetMessage::CritterMove);
        REQUIRE(packets[1].Message == NetMessage::CritterMoveLease);
        check_sender_stamp(packets[0], move_before, move_after);
        check_sender_stamp(packets[1], lease_before, lease_after);
        check_move_payload(packets[0], expected_move);
        check_lease_payload(packets[1], expected_lease);

        // Millisecond rounding can leave a timely sample nonzero; the production threshold decides lateness
        timespan immediate_move_late = immediate_estimator.AddSample(packets[0].SenderMs, packets[0].ReceiveTime);
        CHECK(evaluate_catch_up(immediate_move_late) == timespan::zero);
        timespan immediate_lease_late = immediate_estimator.AddSample(packets[1].SenderMs, packets[1].ReceiveTime);
        CHECK(evaluate_catch_up(immediate_lease_late) == timespan::zero);

        LinkDelayEstimator delayed_move_estimator = make_estimator();
        baseline_ping = capture_fresh_ping();
        (void)delayed_move_estimator.AddSample(baseline_ping.SenderMs, baseline_ping.ReceiveTime);
        test_connection->ClearReceivedTransportPackets();

        test_connection->SetDispatchHold(true);
        move_before = nanotime::now();
        player->Send_Moving(cr);
        move_after = nanotime::now();
        CHECK(test_connection->GetReceivedTransportPackets().empty());
        coarse_sleep(std::chrono::milliseconds {250});
        CHECK(test_connection->GetReceivedTransportPackets().empty());
        test_connection->SetDispatchHold(false);
        test_connection->Dispatch();

        packets = test_connection->GetReceivedTransportPackets();
        REQUIRE(packets.size() == 1);
        REQUIRE(packets.front().Message == NetMessage::CritterMove);
        check_sender_stamp(packets.front(), move_before, move_after);
        CHECK(packets.front().ReceiveTime - move_after >= std::chrono::milliseconds {250});
        check_move_payload(packets.front(), expected_move);

        timespan delayed_move_late = delayed_move_estimator.AddSample(packets.front().SenderMs, packets.front().ReceiveTime);
        CHECK(evaluate_catch_up(delayed_move_late) > timespan::zero);
        CHECK(server->GameTime.GetFrameTime() == frozen_frame);

        LinkDelayEstimator delayed_lease_estimator = make_estimator();
        baseline_ping = capture_fresh_ping();
        (void)delayed_lease_estimator.AddSample(baseline_ping.SenderMs, baseline_ping.ReceiveTime);
        test_connection->ClearReceivedTransportPackets();

        test_connection->SetDispatchHold(true);
        lease_before = nanotime::now();
        player->Send_MovingLease(cr);
        lease_after = nanotime::now();
        CHECK(test_connection->GetReceivedTransportPackets().empty());
        coarse_sleep(std::chrono::milliseconds {250});
        CHECK(test_connection->GetReceivedTransportPackets().empty());
        test_connection->SetDispatchHold(false);
        test_connection->Dispatch();

        packets = test_connection->GetReceivedTransportPackets();
        REQUIRE(packets.size() == 1);
        REQUIRE(packets.front().Message == NetMessage::CritterMoveLease);
        check_sender_stamp(packets.front(), lease_before, lease_after);
        CHECK(packets.front().ReceiveTime - lease_after >= std::chrono::milliseconds {250});
        check_lease_payload(packets.front(), expected_lease);

        timespan delayed_lease_late = delayed_lease_estimator.AddSample(packets.front().SenderMs, packets.front().ReceiveTime);
        CHECK(evaluate_catch_up(delayed_lease_late) > timespan::zero);
        CHECK(server->GameTime.GetFrameTime() == frozen_frame);

        server->StopCritterMoving(cr);
        server->SwitchPlayerCritter(player, nullptr);
        cr->UnmarkIsForPlayer();
        server->CrMngr.DestroyCritter(cr);
        server->MapMngr.DestroyLocation(loc);
    });

    CHECK(callback_invoked);
    REQUIRE(quiesced);
    CHECK_FALSE(server->GameTime.IsPaused());
    CHECK(server->IsConnectionAdmissionOpen());
    CHECK_FALSE(server->GetCurrentSyncContext());
}

FO_END_NAMESPACE
