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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Critter.h"
#include "CritterManager.h"
#include "DataBase.h"
#include "EngineBase.h"
#include "EntityManager.h"
#include "Geometry.h"
#include "Item.h"
#include "ItemManager.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "NetworkServer.h"
#include "Player.h"
#include "ProtoManager.h"
#include "ScriptSystem.h"
#include "ServerConnection.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(ServerInitException);

class FOServer final : public BaseEngine
{
    friend class ServerScriptSystem;

public:
    explicit FOServer(GlobalSettings& settings);

    FOServer(const FOServer&) = delete;
    FOServer(FOServer&&) noexcept = delete;
    auto operator=(const FOServer&) = delete;
    auto operator=(FOServer&&) noexcept = delete;
    ~FOServer() override;

    [[nodiscard]] auto GetEngine() noexcept -> FOServer* { return this; }

    [[nodiscard]] auto IsStarted() const noexcept -> bool { return _started; }
    [[nodiscard]] auto IsStartingError() const noexcept -> bool { return _startingError; }
    [[nodiscard]] auto GetHealthInfo() const -> string;
    [[nodiscard]] auto GetIngamePlayersStatistics() -> string;
    [[nodiscard]] auto GetLocationAndMapsStatistics() -> string;
    [[nodiscard]] auto MakePlayerId(string_view player_name) const -> ident_t;
    [[nodiscard]] auto GetLangPack() const -> const LanguagePack& { return _defaultLang; }

    auto Lock(optional<timespan> max_wait_time) -> bool;
    void Unlock();
    void DrawGui(string_view server_name);

    auto CreateItemOnHex(Map* map, mpos hex, hstring pid, int32 count, Properties* props) -> FO_NON_NULL Item*;
    void VerifyTrigger(Map* map, Critter* cr, mpos from_hex, mpos to_hex, uint8 dir);

    auto CreateCritter(hstring pid, bool for_player) -> Critter*;
    auto LoadCritter(ident_t cr_id, bool for_player) -> Critter*;
    void UnloadCritter(Critter* cr);
    void UnloadCritterInnerEntities(Critter* cr);
    void SwitchPlayerCritter(Player* player, Critter* cr);
    void DestroyUnloadedCritter(ident_t cr_id);

    void StartCritterMoving(Critter* cr, uint16 speed, const vector<uint8>& steps, const vector<uint16>& control_steps, ipos16 end_hex_offset, const Player* initiator);
    void ChangeCritterMovingSpeed(Critter* cr, uint16 speed);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnInit);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGenerateWorld);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnStart);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerRegistration, Player* /*player*/, string /*name*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerLogin, Player* /*player*/, string /*name*/, ident_t /*id*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerAllowCommand, Player* /*player*/, uint8 /*arg2*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerLogout, Player* /*player*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerInit, Player* /*player*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerEnter, Player* /*player*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerCritterSwitched, Player* /*player*/, Critter* /*cr*/, Critter* /*prevCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerMoveCritter, Player* /*player*/, Critter* /*cr*/, int32& /*speed*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerDirCritter, Player* /*player*/, Critter* /*cr*/, int16& /*dirAngle*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterTransfer, Critter* /*cr*/, Map* /*prevMap*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGlobalMapCritterIn, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGlobalMapCritterOut, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLocationInit, Location* /*location*/, bool /*firstTime*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLocationFinish, Location* /*location*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapInit, Map* /*map*/, bool /*firstTime*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapFinish, Map* /*map*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapCritterIn, Map* /*map*/, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapCritterOut, Map* /*map*/, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapCheckLook, Map* /*map*/, Critter* /*cr*/, Critter* /*target*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapCheckTrapLook, Map* /*map*/, Critter* /*cr*/, Item* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterInit, Critter* /*cr*/, bool /*firstTime*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterFinish, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterLoad, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterUnload, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterSendInitialInfo, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterItemMoved, Critter* /*cr*/, Item* /*item*/, CritterItemSlot /*fromSlot*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemInit, Item* /*item*/, bool /*firstTime*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemFinish, Item* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnStaticItemWalk, StaticItem* /*item*/, Critter* /*cr*/, bool /*isIn*/, uint8 /*dir*/);

    EntityManager EntityMngr;
    MapManager MapMngr;
    CritterManager CrMngr;
    ItemManager ItemMngr;

    DataBase DbStorage {};
    const hstring GameCollectionName = Hashes.ToHashedString("Game");
    const hstring HistoryCollectionName = Hashes.ToHashedString("History");
    const hstring PlayersCollectionName = Hashes.ToHashedString("Players");

    EventObserver<> OnWillFinish {};
    EventObserver<> OnDidFinish {};

private:
    struct ServerStats
    {
        nanotime ServerStartTime {};
        timespan Uptime {};

        int64 BytesSend {};
        int64 BytesRecv {};
        int64 DataReal {1};
        int64 DataCompressed {1};
        float32 CompressRatio {};

        size_t MaxOnline {};
        size_t CurOnline {};

        size_t LoopsCount {};
        timespan LoopLastTime {};
        timespan LoopMinTime {};
        timespan LoopMaxTime {};

        deque<pair<nanotime, timespan>> LoopTimeStamps {};
        timespan LoopWholeAvgTime {};
        timespan LoopAvgTime {};

        nanotime LoopCounterBegin {};
        size_t LoopCounter {};
        size_t LoopsPerSecond {};
    };

    void SyncPoint();

    void OnNewConnection(shared_ptr<NetworkServerConnection> net_connection);

    void ProcessUnloginedPlayer(Player* unlogined_player);
    void ProcessPlayer(Player* player);
    void ProcessConnection(ServerConnection* connection);

    void Process_Handshake(ServerConnection* connection);
    void Process_Ping(ServerConnection* connection);
    void Process_UpdateFile(ServerConnection* connection);
    void Process_UpdateFileData(ServerConnection* connection);
    void Process_Register(Player* unlogined_player);
    void Process_Login(Player* unlogined_player);
    void Process_Move(Player* player);
    void Process_StopMove(Player* player);
    void Process_Dir(Player* player);
    void Process_Command(NetInBuffer& buf, const LogFunc& logcb, Player* player);
    void Process_Property(Player* player);
    void Process_RemoteCall(Player* player);

    void OnSaveEntityValue(Entity* entity, const Property* prop);

    void OnSendGlobalValue(Entity* entity, const Property* prop);
    void OnSendPlayerValue(Entity* entity, const Property* prop);
    void OnSendItemValue(Entity* entity, const Property* prop);
    void OnSendCritterValue(Entity* entity, const Property* prop);
    void OnSendMapValue(Entity* entity, const Property* prop);
    void OnSendLocationValue(Entity* entity, const Property* prop);
    void OnSendCustomEntityValue(Entity* entity, const Property* prop);

    void OnSetCritterLook(Entity* entity, const Property* prop);
    void OnSetItemCount(Entity* entity, const Property* prop, const void* new_value);
    void OnSetItemChangeView(Entity* entity, const Property* prop);
    void OnSetItemRecacheHex(Entity* entity, const Property* prop);
    void OnSetItemMultihexLines(Entity* entity, const Property* prop);

    void ProcessCritterMoving(Critter* cr);
    void ProcessCritterMovingBySteps(Critter* cr, Map* map);
    void SendCritterInitialInfo(Critter* cr, Critter* prev_cr);

    void LogToClients(string_view str);
    void DispatchLogToClients();

    WorkThread _starter {"ServerStarter"};
    WorkThread _mainWorker {"ServerWorker"};
    WorkThread _healthWriter {"ServerHealthWriter"};

    std::mutex _syncLocker {};
    std::condition_variable _syncWaitSignal {};
    std::condition_variable _syncRunSignal {};
    int32 _syncRequest {};
    bool _syncPointReady {};

    std::atomic_bool _started {};
    std::atomic_bool _startingError {};
    FrameBalancer _loopBalancer {};
    ServerStats _stats {};
    unordered_map<string, nanotime> _registrationHistory {};
    vector<vector<uint8>> _updateFilesData {};
    vector<uint8> _updateFilesDesc {};
    vector<refcount_ptr<Player>> _logClients {};
    vector<string> _logLines {};
    LanguagePack _defaultLang {};
    vector<unique_ptr<NetworkServer>> _connectionServers {}; // Todo: run network listeners dynamically, without restriction, based on server settings
    vector<shared_ptr<NetworkServerConnection>> _newConnections {};
    mutable std::mutex _newConnectionsLocker {};
    vector<refcount_ptr<Player>> _unloginedPlayers {};
    EventDispatcher<> _willFinishDispatcher {OnWillFinish};
    EventDispatcher<> _didFinishDispatcher {OnDidFinish};
};

FO_END_NAMESPACE();
