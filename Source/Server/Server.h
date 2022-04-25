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
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "ClientConnection.h"
#include "Critter.h"
#include "CritterManager.h"
#include "DataBase.h"
#include "DeferredCalls.h"
#include "Dialogs.h"
#include "DiskFileSystem.h"
#include "EngineBase.h"
#include "EntityManager.h"
#include "FileSystem.h"
#include "GeometryHelper.h"
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
#include "StringUtils.h"
#include "Timer.h"
#if FO_SINGLEPLAYER
#include "Client.h"
#endif

#include "imgui.h"

DECLARE_EXCEPTION(ServerInitException);

class NetServerBase;

class FOServer final : public FOEngineBase
{
    friend class ServerScriptSystem;

public:
    FOServer() = delete;
    explicit FOServer(GlobalSettings& settings, ScriptSystem* script_sys = nullptr);
    FOServer(const FOServer&) = delete;
    FOServer(FOServer&&) noexcept = delete;
    auto operator=(const FOServer&) = delete;
    auto operator=(FOServer&&) noexcept = delete;
    ~FOServer() override;

#if FO_SINGLEPLAYER
    void ConnectClient(FOClient* client) { }
#endif

    [[nodiscard]] auto GetEngine() -> FOServer* { return this; }

    [[nodiscard]] auto IsStarted() const -> bool { return _started; }
    [[nodiscard]] auto GetIngamePlayersStatistics() const -> string;
    [[nodiscard]] auto MakePlayerId(string_view player_name) const -> uint;

    void Shutdown();
    void MainLoop();
    void DrawGui();

    void SetGameTime(int multiplier, int year, int month, int day, int hour, int minute, int second);
    auto CreateItemOnHex(Map* map, ushort hx, ushort hy, hstring pid, uint count, Properties* props, bool check_blocks) -> Item*;
    void VerifyTrigger(Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir);
    void BeginDialog(Critter* cl, Critter* npc, hstring dlg_pack_id, ushort hx, ushort hy, bool ignore_distance);
    void GetAccesses(vector<string>& client, vector<string>& tester, vector<string>& moder, vector<string>& admin, vector<string>& admin_names);

    ///@ ExportEvent
    ENTITY_EVENT(Init);
    ///@ ExportEvent
    ENTITY_EVENT(GenerateWorld);
    ///@ ExportEvent
    ENTITY_EVENT(Start);
    ///@ ExportEvent
    ENTITY_EVENT(Finish);
    ///@ ExportEvent
    ENTITY_EVENT(Loop);
    ///@ ExportEvent
    ENTITY_EVENT(PlayerRegistration, uint /*ip*/, string /*name*/, uint& /*disallowMsgNum*/, uint& /*disallowStrNum*/, string& /*disallowLex*/);
    ///@ ExportEvent
    ENTITY_EVENT(PlayerLogin, uint /*ip*/, string /*name*/, uint /*id*/, uint& /*disallowMsgNum*/, uint& /*disallowStrNum*/, string& /*disallowLex*/);
    ///@ ExportEvent
    ENTITY_EVENT(PlayerGetAccess, Player* /*player*/, int /*arg1*/, string& /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(PlayerAllowCommand, Player* /*player*/, string /*arg1*/, uchar /*arg2*/);
    ///@ ExportEvent
    ENTITY_EVENT(PlayerLogout, Player* /*player*/);
    ///@ ExportEvent
    ENTITY_EVENT(GlobalMapCritterIn, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(GlobalMapCritterOut, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(LocationInit, Location* /*location*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(LocationFinish, Location* /*location*/);
    ///@ ExportEvent
    ENTITY_EVENT(MapInit, Map* /*map*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(MapFinish, Map* /*map*/);
    ///@ ExportEvent
    ENTITY_EVENT(MapLoop, Map* /*map*/);
    ///@ ExportEvent
    ENTITY_EVENT(MapLoopEx, Map* /*map*/, uint /*loopIndex*/);
    ///@ ExportEvent
    ENTITY_EVENT(MapCritterIn, Map* /*map*/, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(MapCritterOut, Map* /*map*/, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(MapCheckLook, Map* /*map*/, Critter* /*critter*/, Critter* /*target*/);
    ///@ ExportEvent
    ENTITY_EVENT(MapCheckTrapLook, Map* /*map*/, Critter* /*critter*/, Item* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterInit, Critter* /*critter*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterFinish, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterIdle, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterGlobalMapIdle, Critter* /*critter*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterCheckMoveItem, Critter* /*critter*/, Item* /*item*/, uchar /*toSlot*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterMoveItem, Critter* /*critter*/, Item* /*item*/, uchar /*fromSlot*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterTalk, Critter* /*critter*/, Critter* /*playerCr*/, bool /*begin*/, uint /*talkers*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterBarter, Critter* /*critter*/, Critter* /*playerCr*/, bool /*begin*/, uint /*barterCount*/);
    ///@ ExportEvent
    ENTITY_EVENT(CritterGetAttackDistantion, Critter* /*critter*/, AbstractItem* /*item*/, uchar /*itemMode*/, uint& /*dist*/);
    ///@ ExportEvent
    ENTITY_EVENT(ItemInit, Item* /*item*/, bool /*firstTime*/);
    ///@ ExportEvent
    ENTITY_EVENT(ItemFinish, Item* /*item*/);
    ///@ ExportEvent
    ENTITY_EVENT(ItemWalk, Item* /*item*/, Critter* /*critter*/, bool /*isIn*/, uchar /*dir*/);
    ///@ ExportEvent
    ENTITY_EVENT(ItemCheckMove, Item* /*item*/, uint /*count*/, Entity* /*from*/, Entity* /*to*/);
    ///@ ExportEvent
    ENTITY_EVENT(StaticItemWalk, StaticItem* /*item*/, Critter* /*critter*/, bool /*isIn*/, uchar /*dir*/);

    EventObserver<> OnWillFinish {};
    EventObserver<> OnDidFinish {};

    ServerSettings& Settings;
    GeometryHelper GeomHelper;
    ScriptSystem* ScriptSys;
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

    struct ServerGui
    {
        ImVec2 DefaultSize {ImVec2(200, 200)};
        ImVec2 MemoryPos {ImVec2(0, 0)};
        ImVec2 PlayersPos {ImVec2(20, 20)};
        ImVec2 LocMapsPos {ImVec2(40, 40)};
        ImVec2 ItemsPos {ImVec2(60, 60)};
        ImVec2 ProfilerPos {ImVec2(80, 80)};
        ImVec2 InfoPos {ImVec2(100, 100)};
        ImVec2 ControlPanelPos {ImVec2(120, 120)};
        ImVec2 ButtonSize {ImVec2(200, 30)};
        ImVec2 LogPos {ImVec2(140, 140)};
        ImVec2 LogSize {ImVec2(800, 600)};
        string WholeLog {};
        string Stats {};
    };

    struct UpdateFile
    {
        vector<uchar> Data {};
    };

    struct TextListener
    {
        ScriptFunc<void, Critter*, string> Func {};
        int SayType {};
        string FirstStr {};
        uint64 Parameter {};
    };

    struct ClientBanned
    {
        DateTimeStamp BeginTime {};
        DateTimeStamp EndTime {};
        uint ClientIp {};
        string ClientName {};
        string BannedBy {};
        string BanInfo {};
    };

    static constexpr auto BANS_FNAME_ACTIVE = "Save/Bans/Active.txt";
    static constexpr auto BANS_FNAME_EXPIRED = "Save/Bans/Expired.txt";

    void RegisterData();

    auto InitLangPacks(vector<LanguagePack>& lang_packs) -> bool;
    auto InitLangPacksDialogs(vector<LanguagePack>& lang_packs) -> bool;
    auto InitLangPacksLocations(vector<LanguagePack>& lang_packs) -> bool;
    auto InitLangPacksItems(vector<LanguagePack>& lang_packs) -> bool;
    void GenerateUpdateFiles(vector<string>* resource_names);

    void EntitySetValue(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSendGlobalValue(Entity* entity, const Property* prop);
    void OnSendPlayerValue(Entity* entity, const Property* prop);
    void OnSendCritterValue(Entity* entity, const Property* prop);
    void OnSendMapValue(Entity* entity, const Property* prop);
    void OnSendLocationValue(Entity* entity, const Property* prop);

    void Send_MapData(Player* player, const ProtoMap* pmap, const StaticMap* static_map, bool send_tiles, bool send_scenery);
    auto Act_Move(Critter* cr, ushort hx, ushort hy, uint move_params) -> bool;
    auto DialogScriptDemand(DemandResult& demand, Critter* master, Critter* slave) -> bool;
    auto DialogScriptResult(DemandResult& result, Critter* master, Critter* slave) -> uint;

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

    void OnSendItemValue(Entity* entity, const Property* prop);
    void OnSetItemCount(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemChangeView(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemRecacheHex(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemBlockLines(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemIsGeck(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemIsRadio(Entity* entity, const Property* prop, void* cur_value, void* old_value);
    void OnSetItemOpened(Entity* entity, const Property* prop, void* cur_value, void* old_value);

    void ProcessCritter(Critter* cr);

    auto Dialog_Compile(Critter* npc, Critter* cl, const Dialog& base_dlg, Dialog& compiled_dlg) -> bool;
    auto Dialog_CheckDemand(Critter* npc, Critter* cl, DialogAnswer& answer, bool recheck) -> bool;
    auto Dialog_UseResult(Critter* npc, Critter* cl, DialogAnswer& answer) -> uint;

    void OnNewConnection(NetConnection* net_connection);

    void ProcessFreeConnection(ClientConnection* connection);
    void ProcessPlayerConnection(Player* player);
    void ProcessConnection(ClientConnection* connection);

    void ProcessMove(Critter* cr);

    void LogToClients(string_view str);
    void DispatchLogToClients();

    auto GetBanByName(string_view name) -> ClientBanned*;
    auto GetBanByIp(uint ip) -> ClientBanned*;
    auto GetBanTime(ClientBanned& ban) -> uint;
    auto GetBanLexems(ClientBanned& ban) -> string;
    void ProcessBans();
    void SaveBan(ClientBanned& ban, bool expired);
    void SaveBans();
    void LoadBans();

    std::atomic_bool _started {};
    vector<uchar> _restoreInfoBin {};
    ServerStats _stats {};
    ServerGui _gui {};
    map<uint, uint> _regIp {};
    std::mutex _regIpLocker {};
    vector<LanguagePack> _langPacks {};
    uint _fpsTick {};
    uint _fpsCounter {};
    vector<UpdateFile> _updateFiles {};
    vector<uchar> _updateFilesList {};
    vector<TextListener> _textListeners {};
    vector<Player*> _logClients {};
    vector<string> _logLines {};
    // Todo: run network listeners dynamically, without restriction, based on server settings
    NetServerBase* _tcpServer {};
    NetServerBase* _webSocketsServer {};
    vector<ClientConnection*> _freeConnections {};
    mutable std::mutex _freeConnectionsLocker {};
    vector<ClientBanned> _banned {};
    std::mutex _bannedLocker {};
    EventDispatcher<> _willFinishDispatcher {OnWillFinish};
    EventDispatcher<> _didFinishDispatcher {OnDidFinish};
    bool _nonConstHelper {};
};
