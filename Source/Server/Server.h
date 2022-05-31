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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "DeferredCalls.h"
#include "Dialogs.h"
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
#include "Settings.h"
#include "Timer.h"

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
    [[nodiscard]] auto GetIngamePlayersStatistics() const -> string;
    [[nodiscard]] auto MakePlayerId(string_view player_name) const -> uint;

    void Shutdown();
    void MainLoop();
    void DrawGui(string_view server_name);

    void SetGameTime(int multiplier, int year, int month, int day, int hour, int minute, int second);
    auto CreateItemOnHex(Map* map, ushort hx, ushort hy, hstring pid, uint count, Properties* props, bool check_blocks) -> Item*;
    void VerifyTrigger(Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir);
    void BeginDialog(Critter* cl, Critter* npc, hstring dlg_pack_id, ushort hx, ushort hy, bool ignore_distance);
    void GetAccesses(vector<string>& client, vector<string>& tester, vector<string>& moder, vector<string>& admin, vector<string>& admin_names);

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
    ENTITY_EVENT(OnPlayerRegistration, uint /*ip*/, string /*name*/, uint& /*disallowMsgNum*/, uint& /*disallowStrNum*/, string& /*disallowLex*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerLogin, uint /*ip*/, string /*name*/, uint /*id*/, uint& /*disallowMsgNum*/, uint& /*disallowStrNum*/, string& /*disallowLex*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerGetAccess, Player* /*player*/, int /*arg1*/, string& /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerAllowCommand, Player* /*player*/, string /*arg1*/, uchar /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnPlayerLogout, Player* /*player*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnGlobalMapCritterIn, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnGlobalMapCritterOut, Critter* /*critter*/);
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
    ENTITY_EVENT(OnMapCritterIn, Map* /*map*/, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapCritterOut, Map* /*map*/, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapCheckLook, Map* /*map*/, Critter* /*critter*/, Critter* /*target*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnMapCheckTrapLook, Map* /*map*/, Critter* /*critter*/, Item* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterInit, Critter* /*critter*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterFinish, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterIdle, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterGlobalMapIdle, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterCheckMoveItem, Critter* /*critter*/, Item* /*item*/, uchar /*toSlot*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterMoveItem, Critter* /*critter*/, Item* /*item*/, uchar /*fromSlot*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterTalk, Critter* /*critter*/, Critter* /*playerCr*/, bool /*begin*/, uint /*talkers*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterBarter, Critter* /*critter*/, Critter* /*playerCr*/, bool /*begin*/, uint /*barterCount*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnCritterGetAttackDistantion, Critter* /*critter*/, AbstractItem* /*item*/, uchar /*itemMode*/, uint& /*dist*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemInit, Item* /*item*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemFinish, Item* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemWalk, Item* /*item*/, Critter* /*critter*/, bool /*isIn*/, uchar /*dir*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnItemCheckMove, Item* /*item*/, uint /*count*/, Entity* /*from*/, Entity* /*to*/);
    ///@ ExportEvent
    ENTITY_EVENT(OnStaticItemWalk, StaticItem* /*item*/, Critter* /*critter*/, bool /*isIn*/, uchar /*dir*/);

    GameTimer GameTime;
    ProtoManager ProtoMngr;
    DeferredCallManager DeferredCallMngr;

    EntityManager EntityMngr;
    MapManager MapMngr;
    CritterManager CrMngr;
    ItemManager ItemMngr;
    DialogManager DlgMngr;

    DataBase DbStorage {};
    DataBase DbHistory {}; // Todo: remove history DB system?

    EventObserver<> OnWillFinish {};
    EventObserver<> OnDidFinish {};

private:
    struct ServerStats
    {
        uint ServerStartTick {};
        uint Uptime {};
        int64 BytesSend {};
        int64 BytesRecv {};
        int64 DataReal {1};
        int64 DataCompressed {1};
        float CompressRatio {};
        uint MaxOnline {};
        uint CurOnline {};
        uint CycleTime {};
        uint Fps {};
        uint LoopTime {};
        uint LoopCycles {};
        uint LoopMin {};
        uint LoopMax {};
        uint LagsCount {};
    };

    struct TextListener
    {
        ScriptFunc<void, Critter*, string> Func {};
        int SayType {};
        string FirstStr {};
        uint64 Parameter {};
    };

    void OnNewConnection(NetConnection* net_connection);
    void ProcessFreeConnection(ClientConnection* connection);
    void ProcessPlayerConnection(Player* player);
    void ProcessConnection(ClientConnection* connection);

    void Process_Ping(ClientConnection* connection);
    void Process_Update(ClientConnection* connection);
    void Process_UpdateFile(ClientConnection* connection);
    void Process_UpdateFileData(ClientConnection* connection);
    void Process_Register(ClientConnection* connection);
    void Process_LogIn(ClientConnection* connection);

    void Process_PlaceToGame(Player* player);
    void Process_Move(Player* player);
    void Process_Dir(Player* player);
    void Process_Text(Player* player);
    void Process_Command(NetInBuffer& buf, const LogFunc& logcb, Player* player, string_view admin_panel);
    void Process_CommandReal(NetInBuffer& buf, const LogFunc& logcb, Player* player, string_view admin_panel);
    void Process_Dialog(Player* player);
    void Process_GiveMap(Player* player);
    void Process_Property(Player* player, uint data_size);

    void OnSaveEntityValue(Entity* entity, const Property* prop, const void* new_value, const void* old_value);

    void OnSendGlobalValue(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSendPlayerValue(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSendItemValue(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSendCritterValue(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSendMapValue(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSendLocationValue(Entity* entity, const Property* prop, const void* new_value, const void* old_value);

    void OnSetItemCount(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSetItemChangeView(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSetItemRecacheHex(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSetItemBlockLines(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSetItemIsGeck(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSetItemIsRadio(Entity* entity, const Property* prop, const void* new_value, const void* old_value);
    void OnSetItemOpened(Entity* entity, const Property* prop, const void* new_value, const void* old_value);

    void ProcessCritter(Critter* cr);
    void ProcessCritterMoving(Critter* cr);
    auto MoveCritter(Critter* cr, ushort hx, ushort hy, uint move_params) -> bool;

    auto DialogScriptDemand(DemandResult& demand, Critter* master, Critter* slave) -> bool;
    auto DialogScriptResult(DemandResult& result, Critter* master, Critter* slave) -> uint;
    auto DialogCompile(Critter* npc, Critter* cl, const Dialog& base_dlg, Dialog& compiled_dlg) -> bool;
    auto DialogCheckDemand(Critter* npc, Critter* cl, DialogAnswer& answer, bool recheck) -> bool;
    auto DialogUseResult(Critter* npc, Critter* cl, DialogAnswer& answer) -> uint;

    void LogToClients(string_view str);
    void DispatchLogToClients();

    std::atomic_bool _started {};
    ServerStats _stats {};
    map<uint, uint> _regIp {};
    uint _fpsTick {};
    uint _fpsCounter {};
    vector<vector<uchar>> _updateFilesData {};
    vector<uchar> _updateFilesDesc {};
    vector<TextListener> _textListeners {};
    vector<Player*> _logClients {};
    vector<string> _logLines {};
    // Todo: run network listeners dynamically, without restriction, based on server settings
    NetServerBase* _tcpServer {};
    NetServerBase* _webSocketsServer {};
    vector<ClientConnection*> _freeConnections {};
    mutable std::mutex _freeConnectionsLocker {};
    EventDispatcher<> _willFinishDispatcher {OnWillFinish};
    EventDispatcher<> _didFinishDispatcher {OnDidFinish};
};
