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
#ifdef FO_SINGLEPLAYER
#include "Client.h"
#endif

#include "imgui.h"

// Check buffer for error
#define CHECK_IN_BUFF_ERROR(client) CHECK_IN_BUFF_ERROR_EXT(client, (void)0, return )
#define CHECK_IN_BUFF_ERROR_EXT(client, before_disconnect, after_disconnect) \
    if (client->Connection->Bin.IsError()) \
    { \
        WriteLog("Wrong network data from client '{}', line {}.\n", client->GetName(), __LINE__); \
        before_disconnect; \
        client->Disconnect(); \
        after_disconnect; \
    }

class FOServer : public NonCopyable // Todo: rename FOServer to just Server
{
public:
    FOServer(GlobalSettings& sett);

#ifdef FO_SINGLEPLAYER
    int RunAsSingleplayer(FOClient* client) { return 0; }
#endif

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
    GlobalVars* Globals {};

    void EntitySetValue(Entity* entity, Property* prop, void* cur_value, void* old_value);

    // Net process
    void Process_ParseToGame(Client* cl);
    void Process_Move(Client* cl);
    void Process_Update(Client* cl);
    void Process_UpdateFile(Client* cl);
    void Process_UpdateFileData(Client* cl);
    void Process_CreateClient(Client* cl);
    void Process_LogIn(Client*& cl);
    void Process_Dir(Client* cl);
    void Process_Text(Client* cl);
    void Process_Command(NetBuffer& buf, LogFunc logcb, Client* cl, const string& admin_panel);
    void Process_CommandReal(NetBuffer& buf, LogFunc logcb, Client* cl, const string& admin_panel);
    void Process_Dialog(Client* cl);
    void Process_GiveMap(Client* cl);
    void Process_Ping(Client* cl);
    void Process_Property(Client* cl, uint data_size);

    void Send_MapData(Client* cl, ProtoMap* pmap, StaticMap* static_map, bool send_tiles, bool send_scenery);

    // Update files
    struct UpdateFile
    {
        uint Size;
        uchar* Data;
    };
    using UpdateFileVec = vector<UpdateFile>;
    UpdateFileVec UpdateFiles;
    UCharVec UpdateFilesList;

    void GenerateUpdateFiles(bool first_generation = false, StrVec* resource_names = nullptr);

    // Actions
    bool Act_Move(Critter* cr, ushort hx, ushort hy, uint move_params);

    void VerifyTrigger(Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir);

    // Dialogs demand and result
    bool DialogScriptDemand(DemandResult& demand, Critter* master, Critter* slave);
    uint DialogScriptResult(DemandResult& result, Critter* master, Critter* slave);

    // Text listen
#define TEXT_LISTEN_FIRST_STR_MAX_LEN (63)
    struct TextListen
    {
        ScriptFunc<void, Critter*, string> Func {};
        int SayType {};
        string FirstStr {};
        uint64 Parameter {};
    };
    using TextListenVec = vector<TextListen>;
    TextListenVec TextListeners;
    std::mutex TextListenersLocker;

    void OnSendGlobalValue(Entity* entity, Property* prop);
    void OnSendCritterValue(Entity* entity, Property* prop);
    void OnSendMapValue(Entity* entity, Property* prop);
    void OnSendLocationValue(Entity* entity, Property* prop);

    // Items
    Item* CreateItemOnHex(Map* map, ushort hx, ushort hy, hash pid, uint count, Properties* props, bool check_blocks);
    void OnSendItemValue(Entity* entity, Property* prop);
    void OnSetItemCount(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemChangeView(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemRecacheHex(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemBlockLines(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemIsGeck(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemIsRadio(Entity* entity, Property* prop, void* cur_value, void* old_value);
    void OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value);

    // Npc
    void ProcessCritter(Critter* cr);
    bool Dialog_Compile(Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg);
    bool Dialog_CheckDemand(Npc* npc, Client* cl, DialogAnswer& answer, bool recheck);
    uint Dialog_UseResult(Npc* npc, Client* cl, DialogAnswer& answer);
    void Dialog_Begin(Client* cl, Npc* npc, hash dlg_pack_id, ushort hx, ushort hy, bool ignore_distance);

    // Main
    int UpdateIndex = -1;
    int UpdateLastIndex = -1;
    uint UpdateLastTick = 0;
    bool Active = false;
    bool ActiveInProcess = false;
    UIntMap RegIp;
    std::mutex RegIpLocker;

    void DisconnectClient(Client* cl);
    void RemoveClient(Client* cl);
    void Process(Client* cl);
    void ProcessMove(Critter* cr);

    // Log to client
    ClientVec LogClients;
    void LogToClients(const string& str);

    // Game time
    void SetGameTime(int multiplier, int year, int month, int day, int hour, int minute, int second);

    // Lang packs
    LangPackVec LangPacks; // Todo: synchronize LangPacks
    bool InitLangPacks(LangPackVec& lang_packs);
    bool InitLangPacksDialogs(LangPackVec& lang_packs);
    bool InitLangPacksLocations(LangPackVec& lang_packs);
    bool InitLangPacksItems(LangPackVec& lang_packs);

    // Init/Finish
    int Run();
    bool Init();
    bool InitReal();
    void Finish();
    bool Starting() { return Active && ActiveInProcess; }
    bool Started() { return Active && !ActiveInProcess; }
    bool Stopping() { return !Active && ActiveInProcess; }
    bool Stopped() { return !Active && !ActiveInProcess; }
    void Stop() { Settings.Quit = true; }
    void LogicTick();

    // Gui
    struct
    {
        ImVec2 DefaultSize = ImVec2(200, 200);
        ImVec2 MemoryPos = ImVec2(0, 0);
        ImVec2 PlayersPos = ImVec2(20, 20);
        ImVec2 LocMapsPos = ImVec2(40, 40);
        ImVec2 ItemsPos = ImVec2(60, 60);
        ImVec2 ProfilerPos = ImVec2(80, 80);
        ImVec2 InfoPos = ImVec2(100, 100);
        ImVec2 ControlPanelPos = ImVec2(120, 120);
        ImVec2 ButtonSize = ImVec2(200, 30);
        ImVec2 LogPos = ImVec2(140, 140);
        ImVec2 LogSize = ImVec2(800, 600);
        string CurLog;
        string WholeLog;
        string Stats;
    } Gui;

    void DrawGui();

    // Todo: run network listeners dynamically, without restriction, based on server settings
    NetServerBase* TcpServer {};
    NetServerBase* WebSocketsServer {};
    ClientVec ConnectedClients {};
    std::mutex ConnectedClientsLocker {};

    void OnNewConnection(NetConnection* connection);

    // Access
    void GetAccesses(StrVec& client, StrVec& tester, StrVec& moder, StrVec& admin, StrVec& admin_names);

// Banned
#define BANS_FNAME_ACTIVE "Save/Bans/Active.txt"
#define BANS_FNAME_EXPIRED "Save/Bans/Expired.txt"
    struct ClientBanned
    {
        DateTimeStamp BeginTime;
        DateTimeStamp EndTime;
        uint ClientIp;
        char ClientName[UTF8_BUF_SIZE(MAX_NAME)];
        char BannedBy[UTF8_BUF_SIZE(MAX_NAME)];
        char BanInfo[UTF8_BUF_SIZE(128)];
        bool operator==(const string& name) { return _str(name).compareIgnoreCaseUtf8(ClientName); }
        bool operator==(const uint ip) { return ClientIp == ip; }

        string GetBanLexems()
        {
            return _str("$banby{}$time{}$reason{}", BannedBy[0] ? BannedBy : "?",
                Timer::GetTimeDifference(EndTime, BeginTime) / 60 / 60, BanInfo[0] ? BanInfo : "just for fun");
        }
    };
    using ClientBannedVec = vector<ClientBanned>;
    ClientBannedVec Banned;
    std::mutex BannedLocker;

    ClientBanned* GetBanByName(const char* name);
    ClientBanned* GetBanByIp(uint ip);
    uint GetBanTime(ClientBanned& ban);
    void ProcessBans();
    void SaveBan(ClientBanned& ban, bool expired);
    void SaveBans();
    void LoadBans();

    // Statistics
    struct
    {
        uint ServerStartTick;
        uint Uptime;
        int64 BytesSend;
        int64 BytesRecv;
        int64 DataReal;
        int64 DataCompressed;
        float CompressRatio;
        uint MaxOnline;
        uint CurOnline;

        uint CycleTime;
        uint FPS;
        uint LoopTime;
        uint LoopCycles;
        uint LoopMin;
        uint LoopMax;
        uint LagsCount;
    } Statistics;

    string GetIngamePlayersStatistics();
};
