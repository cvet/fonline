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

#include "Critter.h"
#include "CritterManager.h"
#include "DataBase.h"
#include "Dialogs.h"
#include "EntityManager.h"
#include "FileSystem.h"
#include "GeometryHelper.h"
#include "Item.h"
#include "ItemManager.h"
#include "Location.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "Networking.h"
#include "ProtoManager.h"
#include "ServerScripting.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Timer.h"
#if FO_SINGLEPLAYER
#include "Client.h"
#endif

#include "imgui.h"

DECLARE_EXCEPTION(ServerInitException);

// Check buffer for error
#define CHECK_IN_BUFF_ERROR(client) CHECK_IN_BUFF_ERROR_EXT(client, (void)0, return )
#define CHECK_IN_BUFF_ERROR_EXT(client, before_disconnect, after_disconnect) \
    do { \
        if ((client)->Connection->Bin.IsError()) { \
            WriteLog("Wrong network data from client '{}', line {}.\n", (client)->GetName(), __LINE__); \
            before_disconnect; \
            (client)->Disconnect(); \
            after_disconnect; \
        } \
    } while (0)

class FOServer final // Todo: rename FOServer to just Server
{
public:
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
        uint Size {};
        uchar* Data {};
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

    static constexpr auto TEXT_LISTEN_FIRST_STR_MAX_LEN = 63;
    static constexpr auto BANS_FNAME_ACTIVE = "Save/Bans/Active.txt";
    static constexpr auto BANS_FNAME_EXPIRED = "Save/Bans/Expired.txt";

    FOServer() = delete;
    explicit FOServer(GlobalSettings& settings);
    FOServer(const FOServer&) = delete;
    FOServer(FOServer&&) noexcept = delete;
    auto operator=(const FOServer&) = delete;
    auto operator=(FOServer&&) noexcept = delete;
    ~FOServer();

#if FO_SINGLEPLAYER
    void ConnectClient(FOClient* client) { }
#endif

    void Shutdown();
    void MainLoop();
    void DrawGui();
    auto GetIngamePlayersStatistics() -> string;

    auto InitLangPacks(vector<LanguagePack>& lang_packs) -> bool;
    auto InitLangPacksDialogs(vector<LanguagePack>& lang_packs) -> bool;
    auto InitLangPacksLocations(vector<LanguagePack>& lang_packs) -> bool;
    auto InitLangPacksItems(vector<LanguagePack>& lang_packs) -> bool;
    void GenerateUpdateFiles(bool first_generation, vector<string>* resource_names);

    void EntitySetValue(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSendGlobalValue(Entity* entity, Property* prop);
    void OnSendCritterValue(Entity* entity, Property* prop);
    void OnSendMapValue(Entity* entity, Property* prop);
    void OnSendLocationValue(Entity* entity, Property* prop);

    void Send_MapData(Client* cl, const ProtoMap* pmap, const StaticMap* static_map, bool send_tiles, bool send_scenery);
    auto Act_Move(Critter* cr, ushort hx, ushort hy, uint move_params) -> bool;
    void VerifyTrigger(Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir);
    auto DialogScriptDemand(DemandResult& demand, Critter* master, Critter* slave) -> bool;
    auto DialogScriptResult(DemandResult& result, Critter* master, Critter* slave) -> uint;

    void Process_ParseToGame(Client* cl);
    void Process_Move(Client* cl);
    void Process_Update(Client* cl);
    void Process_UpdateFile(Client* cl);
    void Process_UpdateFileData(Client* cl);
    void Process_CreateClient(Client* cl);
    void Process_LogIn(Client*& cl);
    void Process_Dir(Client* cl);
    void Process_Text(Client* cl);
    void Process_Command(NetBuffer& buf, const LogFunc& logcb, Client* cl, const string& admin_panel);
    void Process_CommandReal(NetBuffer& buf, const LogFunc& logcb, Client* cl, const string& admin_panel);
    void Process_Dialog(Client* cl);
    void Process_GiveMap(Client* cl);
    void Process_Ping(Client* cl);
    void Process_Property(Client* cl, uint data_size);

    auto CreateItemOnHex(Map* map, ushort hx, ushort hy, hash pid, uint count, Properties* props, bool check_blocks) -> Item*;
    void OnSendItemValue(Entity* entity, Property* prop);
    void OnSetItemCount(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemChangeView(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemRecacheHex(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemBlockLines(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemIsGeck(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemIsRadio(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value);

    void ProcessCritter(Critter* cr);
    auto Dialog_Compile(Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg) -> bool;
    auto Dialog_CheckDemand(Npc* npc, Client* cl, DialogAnswer& answer, bool recheck) -> bool;
    auto Dialog_UseResult(Npc* npc, Client* cl, DialogAnswer& answer) -> uint;
    void Dialog_Begin(Client* cl, Npc* npc, hash dlg_pack_id, ushort hx, ushort hy, bool ignore_distance);

    void OnNewConnection(NetConnection* connection);
    void DisconnectClient(Client* cl);
    void RemoveClient(Client* cl);
    void Process(Client* cl);
    void ProcessMove(Critter* cr);

    void LogToClients(const string& str);
    void DispatchLogToClients();
    void SetGameTime(int multiplier, int year, int month, int day, int hour, int minute, int second);
    void GetAccesses(vector<string>& client, vector<string>& tester, vector<string>& moder, vector<string>& admin, vector<string>& admin_names);

    auto GetBanByName(const char* name) -> ClientBanned*;
    auto GetBanByIp(uint ip) -> ClientBanned*;
    auto GetBanTime(ClientBanned& ban) -> uint;
    auto GetBanLexems(ClientBanned& ban) -> string;
    void ProcessBans();
    void SaveBan(ClientBanned& ban, bool expired);
    void SaveBans();
    void LoadBans();

    EventObserver<> OnWillFinish {};
    EventObserver<> OnDidFinish {};
    EventDispatcher<> WillFinishDispatcher {OnWillFinish};
    EventDispatcher<> DidFinishDispatcher {OnDidFinish};

    ServerSettings& Settings;
    GeometryHelper GeomHelper;
    FileManager FileMngr;
    ServerScriptSystem ScriptSys;
    ProtoManager ProtoMngr;
    EntityManager EntityMngr;
    MapManager MapMngr;
    CritterManager CrMngr;
    ItemManager ItemMngr;
    DialogManager DlgMngr;
    GameTimer GameTime;
    ServerStats Stats {};
    ServerGui Gui {};
    GlobalVars* Globals {};
    DataBase DbStorage {};
    DataBase DbHistory {};
    std::atomic_bool Started {};
    map<uint, uint> RegIp {};
    std::mutex RegIpLocker {};
    vector<LanguagePack> LangPacks {};
    uint FpsTick {};
    uint FpsCounter {};
    vector<UpdateFile> UpdateFiles {};
    vector<uchar> UpdateFilesList {};
    vector<TextListener> TextListeners {};
    std::mutex TextListenersLocker {};
    vector<Client*> LogClients {};
    vector<string> LogLines {};
    // Todo: run network listeners dynamically, without restriction, based on server settings
    NetServerBase* TcpServer {};
    NetServerBase* WebSocketsServer {};
    vector<Client*> ConnectedClients {};
    std::mutex ConnectedClientsLocker {};
    vector<ClientBanned> Banned {};
    std::mutex BannedLocker {};
    bool _nonConstHelper {};
};
