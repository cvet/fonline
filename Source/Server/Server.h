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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: improve ban system

#pragma once

#include "Common.h"

#include "ClientConnection.h"
#include "Critter.h"
#include "CritterManager.h"
#include "DataBase.h"
#include "Dialogs.h"
#include "DiskFileSystem.h"
#include "EngineBase.h"
#include "EntityManager.h"
#include "Item.h"
#include "ItemManager.h"
#include "Location.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "ProtoManager.h"
#include "ScriptSystem.h"
#include "ServerDeferredCalls.h"
#include "Settings.h"

DECLARE_EXCEPTION(ServerInitException);

class NetServerBase;

class FOServer : virtual public FOEngineBase
{
    friend class ServerScriptSystem;

public:
    explicit FOServer(GlobalSettings& settings);

    FOServer(const FOServer&) = delete;
    FOServer(FOServer&&) noexcept = delete;
    auto operator=(const FOServer&) = delete;
    auto operator=(FOServer&&) noexcept = delete;
    ~FOServer() override;

    [[nodiscard]] auto GetEngine() -> FOServer* { return this; }

    [[nodiscard]] auto IsStarted() const -> bool { return _started; }
    [[nodiscard]] auto IsStartingError() const -> bool { return _startingError; }
    [[nodiscard]] auto GetHealthInfo() const -> string;
    [[nodiscard]] auto GetIngamePlayersStatistics() -> string;
    [[nodiscard]] auto MakePlayerId(string_view player_name) const -> ident_t;

    auto Lock(optional<time_duration> max_wait_time) -> bool;
    void Unlock();
    void DrawGui(string_view server_name);

    void SetGameTime(int multiplier, int year, int month, int day, int hour, int minute, int second);
    auto CreateItemOnHex(Map* map, uint16 hx, uint16 hy, hstring pid, uint count, Properties* props, bool check_blocks) -> Item*;
    void VerifyTrigger(Map* map, Critter* cr, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint8 dir);
    void BeginDialog(Critter* cl, Critter* npc, hstring dlg_pack_id, uint16 hx, uint16 hy, bool ignore_distance);

    auto CreateCritter(hstring pid, bool for_player) -> Critter*;
    auto LoadCritter(ident_t cr_id, bool for_player) -> Critter*;
    void UnloadCritter(Critter* cr);
    void SwitchPlayerCritter(Player* player, Critter* cr);
    void DestroyUnloadedCritter(ident_t cr_id);

    ///@ ExportEvent
    ENTITY_EVENT(OnInit);
    ///@ ExportEvent
    ENTITY_EVENT(OnGenerateWorld);
    ///@ ExportEvent
    ENTITY_EVENT(OnStart);
    ///@ ExportEvent
    ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    ENTITY_EVENT(OnLoop);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerRegistration, Player* /*player*/, string /*name*/, TextPackName& /*disallowTextPack*/, uint& /*disallowStrNum*/, string& /*disallowLex*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerLogin, Player* /*player*/, string /*name*/, ident_t /*id*/, TextPackName& /*disallowTextPack*/, uint& /*disallowStrNum*/, string& /*disallowLex*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerGetAccess, Player* /*player*/, int /*arg1*/, string& /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerAllowCommand, Player* /*player*/, string /*arg1*/, uint8 /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerLogout, Player* /*player*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerInit, Player* /*player*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerEnter, Player* /*player*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerCritterSwitched, Player* /*player*/, Critter* /*cr*/, Critter* /*prevCr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerCheckMove, Player* /*player*/, Critter* /*cr*/, uint& /*speed*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerCheckDir, Player* /*player*/, Critter* /*cr*/, int16& /*dirAngle*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnGlobalMapCritterIn, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnGlobalMapCritterOut, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnLocationInit, Location* /*location*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnLocationFinish, Location* /*location*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapInit, Map* /*map*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapFinish, Map* /*map*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapLoop, Map* /*map*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapLoopEx, Map* /*map*/, uint /*loopIndex*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapCritterIn, Map* /*map*/, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapCritterOut, Map* /*map*/, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapCheckLook, Map* /*map*/, Critter* /*cr*/, Critter* /*target*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapCheckTrapLook, Map* /*map*/, Critter* /*cr*/, Item* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterInit, Critter* /*cr*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterFinish, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterLoad, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterUnload, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterIdle, Critter* /*cr*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterCheckMoveItem, Critter* /*cr*/, Item* /*item*/, CritterItemSlot /*toSlot*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterMoveItem, Critter* /*cr*/, Item* /*item*/, CritterItemSlot /*fromSlot*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterTalk, Critter* /*cr*/, Critter* /*talker*/, bool /*begin*/, uint /*talkers*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterBarter, Critter* /*cr*/, Critter* /*trader*/, bool /*begin*/, uint /*barterCount*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterGetAttackDistantion, Critter* /*cr*/, AbstractItem* /*item*/, uint8 /*itemMode*/, uint& /*dist*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemInit, Item* /*item*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemFinish, Item* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemCheckMove, Item* /*item*/, uint /*count*/, Entity* /*from*/, Entity* /*to*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnStaticItemWalk, StaticItem* /*item*/, Critter* /*cr*/, bool /*isIn*/, uint8 /*dir*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemStackChanged, Item* /*item*/, int /*countDiff*/);

    ServerDeferredCallManager ServerDeferredCalls;

    EntityManager EntityMngr;
    MapManager MapMngr;
    CritterManager CrMngr;
    ItemManager ItemMngr;
    DialogManager DlgMngr;

    DataBase DbStorage {};
    const hstring GameCollectionName = ToHashedString("Game");
    const hstring PlayersCollectionName = ToHashedString("Players");
    const hstring LocationsCollectionName = ToHashedString("Locations");
    const hstring MapsCollectionName = ToHashedString("Maps");
    const hstring CrittersCollectionName = ToHashedString("Critters");
    const hstring ItemsCollectionName = ToHashedString("Items");
    const hstring DeferredCallsCollectionName = ToHashedString("DeferredCalls");
    const hstring HistoryCollectionName = ToHashedString("History");

    EventObserver<> OnWillFinish {};
    EventObserver<> OnDidFinish {};

private:
    struct ServerStats
    {
        time_point ServerStartTime {};
        time_duration Uptime {};

        int64 BytesSend {};
        int64 BytesRecv {};
        int64 DataReal {1};
        int64 DataCompressed {1};
        float CompressRatio {};

        size_t MaxOnline {};
        size_t CurOnline {};

        size_t LoopsCount {};
        time_duration LoopLastTime {};
        time_duration LoopMinTime {};
        time_duration LoopMaxTime {};

        deque<pair<time_point, time_duration>> LoopTimeStamps {};
        time_duration LoopWholeAvgTime {};
        time_duration LoopAvgTime {};

        time_point LoopCounterBegin {};
        size_t LoopCounter {};
        size_t LoopsPerSecond {};
    };

    struct TextListener
    {
        ScriptFunc<void, Critter*, string> Func {};
        int SayType {};
        string FirstStr {};
        uint64 Parameter {};
    };

    void SyncPoint();

    void OnNewConnection(NetConnection* net_connection);

    void ProcessUnloginedPlayer(Player* unlogined_player);
    void ProcessPlayer(Player* player);
    void ProcessConnection(ClientConnection* connection);

    void Process_Handshake(ClientConnection* connection);
    void Process_Ping(ClientConnection* connection);
    void Process_UpdateFile(ClientConnection* connection);
    void Process_UpdateFileData(ClientConnection* connection);
    void Process_Register(Player* unlogined_player);
    void Process_Login(Player* unlogined_player);
    void Process_Move(Player* player);
    void Process_StopMove(Player* player);
    void Process_Dir(Player* player);
    void Process_Text(Player* player);
    void Process_Command(NetInBuffer& buf, const LogFunc& logcb, Player* player, string_view admin_panel);
    void Process_CommandReal(NetInBuffer& buf, const LogFunc& logcb, Player* player, string_view admin_panel);
    void Process_Dialog(Player* player);
    void Process_Property(Player* player, uint data_size);
    void Process_RemoteCall(Player* player);

    void OnSaveEntityValue(Entity* entity, const Property* prop);

    void OnSendGlobalValue(Entity* entity, const Property* prop);
    void OnSendPlayerValue(Entity* entity, const Property* prop);
    void OnSendItemValue(Entity* entity, const Property* prop);
    void OnSendCritterValue(Entity* entity, const Property* prop);
    void OnSendMapValue(Entity* entity, const Property* prop);
    void OnSendLocationValue(Entity* entity, const Property* prop);

    void OnSetItemCount(Entity* entity, const Property* prop, const void* new_value);
    void OnSetItemChangeView(Entity* entity, const Property* prop);
    void OnSetItemRecacheHex(Entity* entity, const Property* prop);
    void OnSetItemBlockLines(Entity* entity, const Property* prop);
    void OnSetItemIsGeck(Entity* entity, const Property* prop);
    void OnSetItemIsRadio(Entity* entity, const Property* prop);

    void ProcessCritter(Critter* cr);
    void ProcessCritterMoving(Critter* cr);
    void ProcessCritterMovingBySteps(Critter* cr, Map* map);
    void StartCritterMoving(Critter* cr, uint16 speed, const vector<uint8>& steps, const vector<uint16>& control_steps, int16 end_hex_ox, int16 end_hex_oy, const Player* initiator);
    void SendCritterInitialInfo(Critter* cr, Critter* prev_cr);

    auto DialogScriptDemand(const DialogAnswerReq& demand, Critter* master, Critter* slave) -> bool;
    auto DialogScriptResult(const DialogAnswerReq& result, Critter* master, Critter* slave) -> uint;
    auto DialogCompile(Critter* npc, Critter* cl, const Dialog& base_dlg, Dialog& compiled_dlg) -> bool;
    auto DialogCheckDemand(Critter* npc, Critter* cl, const DialogAnswer& answer, bool recheck) -> bool;
    auto DialogUseResult(Critter* npc, Critter* cl, const DialogAnswer& answer) -> uint;

    void LogToClients(string_view str);
    void DispatchLogToClients();

    WorkThread _starter {"ServerStarter"};
    WorkThread _mainWorker {"ServerWorker"};
    WorkThread _healthWriter {"ServerHealthWriter"};

    std::mutex _syncLocker {};
    std::condition_variable _syncWaitSignal {};
    std::condition_variable _syncRunSignal {};
    int _syncRequest {};
    bool _syncPointReady {};

    std::atomic_bool _started {};
    std::atomic_bool _startingError {};
    FrameBalancer _loopBalancer {};
    ServerStats _stats {};
    unique_ptr<DiskFile> _healthFile {};
    map<uint, time_point> _regIp {};
    vector<vector<uint8>> _updateFilesData {};
    vector<uint8> _updateFilesDesc {};
    vector<TextListener> _textListeners {};
    vector<Player*> _logClients {};
    vector<string> _logLines {};
    vector<NetServerBase*> _connectionServers {}; // Todo: run network listeners dynamically, without restriction, based on server settings
    vector<ClientConnection*> _newConnections {};
    mutable std::mutex _newConnectionsLocker {};
    vector<Player*> _unloginedPlayers {};
    EventDispatcher<> _willFinishDispatcher {OnWillFinish};
    EventDispatcher<> _didFinishDispatcher {OnDidFinish};
};
