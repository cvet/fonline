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

#include "Server.h"
#include "AdminPanel.h"
#include "GenericUtils.h"
#include "PropertiesSerializator.h"
#include "Testing.h"
#include "Version_Include.h"
#include "WinApi_Include.h"

FOServer::FOServer(GlobalSettings& sett) :
    Settings {sett},
    GeomHelper(Settings),
    FileMngr(),
    ScriptSys(this, sett, FileMngr),
    GameTime(Settings, Globals),
    ProtoMngr(),
    EntityMngr(MapMngr, CrMngr, ItemMngr, ScriptSys),
    MapMngr(Settings, ProtoMngr, EntityMngr, CrMngr, ItemMngr, ScriptSys),
    CrMngr(Settings, ProtoMngr, EntityMngr, MapMngr, ItemMngr, ScriptSys),
    ItemMngr(ProtoMngr, EntityMngr, MapMngr, CrMngr, ScriptSys),
    DlgMngr(FileMngr, ScriptSys)
{
    Active = true;
    ActiveInProcess = true;
}

int FOServer::Run()
{
    WriteLog("FOnline Server ({:#x}).\n", FO_VERSION);
    if (!Settings.CommandLine.empty())
        WriteLog("Command line '{}'.\n", Settings.CommandLine);

    if (Settings.AdminPanelPort)
        InitAdminManager(this, Settings.AdminPanelPort);

    if (!Init())
    {
        WriteLog("Initialization failed!\n");
        return 1;
    }

    WriteLog("***   Starting game loop   ***\n");

    while (!Settings.Quit)
        LogicTick();

    WriteLog("***   Finishing game loop  ***\n");

    Finish();
    return 0;
}

void FOServer::Finish()
{
    Active = false;
    ActiveInProcess = true;

    // Finish logic
    DbStorage->StartChanges();
    if (DbHistory)
        DbHistory->StartChanges();

    ScriptSys.FinishEvent();
    ItemMngr.RadioClear();
    EntityMngr.ClearEntities();

    DbStorage->CommitChanges();
    if (DbHistory)
        DbHistory->CommitChanges();

    // Logging clients
    LogToFunc("LogToClients", std::bind(&FOServer::LogToClients, this, std::placeholders::_1), false);
    for (auto it = LogClients.begin(), end = LogClients.end(); it != end; ++it)
        (*it)->Release();
    LogClients.clear();

    // Clients
    {
        SCOPE_LOCK(ConnectedClientsLocker);

        for (auto it = ConnectedClients.begin(), end = ConnectedClients.end(); it != end; ++it)
        {
            Client* cl = *it;
            cl->Disconnect();
        }
    }

    // Shutdown servers
    SAFEDEL(TcpServer);
    SAFEDEL(WebSocketsServer);

    // Statistics
    WriteLog("Server stopped.\n");
    WriteLog("Statistics:\n");
    WriteLog("Traffic:\n");
    WriteLog("Bytes Send: {}\n", Statistics.BytesSend);
    WriteLog("Bytes Recv: {}\n", Statistics.BytesRecv);
    WriteLog("Cycles count: {}\n", Statistics.LoopCycles);
    WriteLog("Approx cycle period: {}\n", Statistics.LoopTime / (Statistics.LoopCycles ? Statistics.LoopCycles : 1));
    WriteLog("Min cycle period: {}\n", Statistics.LoopMin);
    WriteLog("Max cycle period: {}\n", Statistics.LoopMax);
    WriteLog("Count of lags (>100ms): {}\n", Statistics.LagsCount);

    ActiveInProcess = false;
}

string FOServer::GetIngamePlayersStatistics()
{
    const char* cond_states_str[] = {"None", "Life", "Knockout", "Dead"};

    uint conn_count;
    {
        SCOPE_LOCK(ConnectedClientsLocker);
        conn_count = (uint)ConnectedClients.size();
    }

    ClientVec players;
    CrMngr.GetClients(players);

    string result = _str("Players in game: {}\nConnections: {}\n", players.size(), conn_count);
    result += "Name                 Id         Ip              Online  Cond     X     Y     Location and map\n";
    for (Client* cl : players)
    {
        Map* map = MapMngr.GetMap(cl->GetMapId());
        Location* loc = (map ? map->GetLocation() : nullptr);

        string str_loc = _str("{} ({}) {} ({})", map ? loc->GetName() : "", map ? loc->GetId() : 0,
            map ? map->GetName() : "", map ? map->GetId() : 0);
        result += _str("{:<20} {:<10} {:<15} {:<7} {:<8} {:<5} {:<5} {}\n", cl->Name, cl->GetId(), cl->GetIpStr(),
            cl->IsOffline() ? "No" : "Yes", cond_states_str[cl->GetCond()], map ? cl->GetHexX() : cl->GetWorldX(),
            map ? cl->GetHexY() : cl->GetWorldY(), map ? str_loc : "Global map");
    }
    return result;
}

// Accesses
void FOServer::GetAccesses(StrVec& client, StrVec& tester, StrVec& moder, StrVec& admin, StrVec& admin_names)
{
    // Todo: restore settings
    // client = _str(MainConfig->GetStr("", "Access_client")).split(' ');
    // tester = _str(MainConfig->GetStr("", "Access_tester")).split(' ');
    // moder = _str(MainConfig->GetStr("", "Access_moder")).split(' ');
    // admin = _str(MainConfig->GetStr("", "Access_admin")).split(' ');
    // admin_names = _str(MainConfig->GetStr("", "AccessNames_admin")).split(' ');
}

void FOServer::DisconnectClient(Client* cl)
{
    // Manage saves, exit from game
    uint id = cl->GetId();
    Client* cl_ = (id ? CrMngr.GetPlayer(id) : nullptr);
    if (cl_ && cl_ == cl)
    {
        SETFLAG(cl->Flags, FCRIT_DISCONNECT);
        if (cl->GetMapId())
        {
            cl->SendA_Action(ACTION_DISCONNECT, 0, nullptr);
        }
        else
        {
            for (auto it = cl->GlobalMapGroup->begin(), end = cl->GlobalMapGroup->end(); it != end; ++it)
            {
                Critter* cr = *it;
                if (cr != cl)
                    cr->Send_CustomCommand(cl, OTHER_FLAGS, cl->Flags);
            }
        }
    }
}

void FOServer::RemoveClient(Client* cl)
{
    uint id = cl->GetId();
    Client* cl_ = (id ? CrMngr.GetPlayer(id) : nullptr);
    if (cl_ && cl_ == cl)
    {
        ScriptSys.PlayerLogoutEvent(cl);
        if (cl->GetClientToDelete())
            ScriptSys.CritterFinishEvent(cl);

        Map* map = MapMngr.GetMap(cl->GetMapId());
        MapMngr.EraseCrFromMap(cl, map);

        // Destroy
        bool full_delete = cl->GetClientToDelete();
        EntityMngr.UnregisterEntity(cl);
        cl->IsDestroyed = true;

        // Erase radios from collection
        ItemVec items = cl->GetItemsNoLock();
        for (auto it = items.begin(), end = items.end(); it != end; ++it)
        {
            Item* item = *it;
            if (item->GetIsRadio())
                ItemMngr.RadioRegister(item, false);
        }

        // Full delete
        if (full_delete)
        {
            CrMngr.DeleteInventory(cl);
            DbStorage->Delete("Players", cl->Id);
        }

        cl->Release();
    }
    else
    {
        cl->IsDestroyed = true;
        ScriptSys.RemoveEntity(cl);
    }
}

void FOServer::LogicTick()
{
    Timer::UpdateTick();

    // Cycle time
    double frame_begin = Timer::AccurateTick();

    // Begin data base changes
    DbStorage->StartChanges();
    if (DbHistory)
        DbHistory->StartChanges();

    // Process clients
    ClientVec clients;
    {
        SCOPE_LOCK(ConnectedClientsLocker);

        clients = ConnectedClients;
        for (Client* cl : clients)
            cl->AddRef();
    }

    for (Client* cl : clients)
    {
        // Check for removing
        if (cl->IsOffline())
        {
            DisconnectClient(cl);

            {
                SCOPE_LOCK(ConnectedClientsLocker);

                auto it = std::find(ConnectedClients.begin(), ConnectedClients.end(), cl);
                RUNTIME_ASSERT(it != ConnectedClients.end());
                ConnectedClients.erase(it);
                Statistics.CurOnline--;
            }

            cl->Release();
            continue;
        }

        // Process network messages
        Process(cl);
        cl->Release();
    }

    // Process critters
    CritterVec critters;
    EntityMngr.GetCritters(critters);
    for (Critter* cr : critters)
    {
        // Player specific
        if (cr->CanBeRemoved)
            RemoveClient((Client*)cr);

        // Check for removing
        if (cr->IsDestroyed)
            continue;

        // Process logic
        ProcessCritter(cr);
    }

    // Process maps
    MapVec maps;
    EntityMngr.GetMaps(maps);
    for (Map* map : maps)
    {
        // Check for removing
        if (map->IsDestroyed)
            continue;

        // Process logic
        map->Process();
    }

    // Locations and maps garbage
    MapMngr.LocationGarbager();

    // Game time
    GameTime.ProcessGameTime();

    // Bans
    ProcessBans();

    // Script game loop
    ScriptSys.LoopEvent();

    // Commit changed to data base
    DbStorage->CommitChanges();
    if (DbHistory)
        DbHistory->CommitChanges();

    // Fill statistics
    double frame_time = Timer::AccurateTick() - frame_begin;
    uint loop_tick = (uint)frame_time;
    Statistics.LoopTime += loop_tick;
    Statistics.LoopCycles++;
    if (loop_tick > Statistics.LoopMax)
        Statistics.LoopMax = loop_tick;
    if (loop_tick < Statistics.LoopMin)
        Statistics.LoopMin = loop_tick;
    Statistics.CycleTime = loop_tick;
    if (loop_tick > 100)
        Statistics.LagsCount++;
    Statistics.Uptime = (Timer::FastTick() - Statistics.ServerStartTick) / 1000;

    // Calculate fps
    static uint fps = 0;
    static double fps_tick = Timer::AccurateTick();
    if (fps_tick >= 1000.0)
    {
        Statistics.FPS = fps;
        fps = 0;
    }
    else
    {
        fps++;
    }

    // Sleep
    if (Settings.GameSleep >= 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(Settings.GameSleep));
}

void FOServer::DrawGui()
{
    // Players
    ImGui::SetNextWindowPos(Gui.PlayersPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Players", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        Gui.Stats =
            "WIP..........................."; // ( Server && Server->Started() ? Server->GetIngamePlayersStatistics() :
                                              // "Waiting for server start..." );
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Locations and maps
    ImGui::SetNextWindowPos(Gui.LocMapsPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Locations and maps", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        Gui.Stats =
            "WIP..........................."; // ( Server && Server->Started() ? MapMngr.GetLocationsMapsStatistics() :
                                              // "Waiting for server start..." );
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Items count
    ImGui::SetNextWindowPos(Gui.ItemsPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Items count", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        Gui.Stats = (Started() ? ItemMngr.GetItemsStatistics() : "Waiting for server start...");
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Profiler
    ImGui::SetNextWindowPos(Gui.ProfilerPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        Gui.Stats = "WIP..........................."; // ScriptSys.GetProfilerStatistics();
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Info
    ImGui::SetNextWindowPos(Gui.InfoPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        Gui.Stats = "";
        DateTimeStamp st = GameTime.GetGameTime(Settings.FullSecond);
        Gui.Stats += _str("Time: {:02}.{:02}.{:04} {:02}:{:02}:{:02} x{}\n", st.Day, st.Month, st.Year, st.Hour,
            st.Minute, st.Second, "WIP" /*Globals->GetTimeMultiplier()*/);
        Gui.Stats += _str("Connections: {}\n", Statistics.CurOnline);
        Gui.Stats += _str("Players in game: {}\n", CrMngr.PlayersInGame());
        Gui.Stats += _str("NPC in game: {}\n", CrMngr.NpcInGame());
        Gui.Stats += _str("Locations: {} ({})\n", MapMngr.GetLocationsCount(), MapMngr.GetMapsCount());
        Gui.Stats += _str("Items: {}\n", ItemMngr.GetItemsCount());
        Gui.Stats += _str("Cycles per second: {}\n", Statistics.FPS);
        Gui.Stats += _str("Cycle time: {}\n", Statistics.CycleTime);
        uint seconds = Statistics.Uptime;
        Gui.Stats += _str("Uptime: {:02}:{:02}:{:02}\n", seconds / 60 / 60, seconds / 60 % 60, seconds % 60);
        Gui.Stats += _str("KBytes Send: {}\n", Statistics.BytesSend / 1024);
        Gui.Stats += _str("KBytes Recv: {}\n", Statistics.BytesRecv / 1024);
        Gui.Stats += _str("Compress ratio: {}",
            (double)Statistics.DataReal / (Statistics.DataCompressed ? Statistics.DataCompressed : 1));
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Control panel
    ImGui::SetNextWindowPos(Gui.ControlPanelPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Control panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (Started() && !Settings.Quit && ImGui::Button("Stop & Quit", Gui.ButtonSize))
            Settings.Quit = true;
        if (!Started() && !Stopping() && ImGui::Button("Quit", Gui.ButtonSize))
            exit(0);
        if (ImGui::Button("Create dump", Gui.ButtonSize))
            CreateDump("ManualDump", "Manual");
        if (ImGui::Button("Save log", Gui.ButtonSize))
        {
            DateTimeStamp dt;
            Timer::GetCurrentDateTime(dt);
            string log_name = _str("Logs/FOnlineServer_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.log", "Log", dt.Year,
                dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
            OutputFile log_file = FileMngr.WriteFile(log_name);
            log_file.SetStr(Gui.WholeLog);
            log_file.Save();
        }
    }
    ImGui::End();

    // Log
    ImGui::SetNextWindowPos(Gui.LogPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.LogSize, ImGuiCond_Once);
    if (ImGui::Begin(
            "Log", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar))
    {
        LogGetBuffer(Gui.CurLog);
        if (!Gui.CurLog.empty())
        {
            Gui.WholeLog += Gui.CurLog;
            Gui.CurLog.clear();
            if (Gui.WholeLog.size() > 100000)
                Gui.WholeLog = Gui.WholeLog.substr(Gui.WholeLog.size() - 100000);
        }

        if (!Gui.WholeLog.empty())
            ImGui::TextUnformatted(Gui.WholeLog.c_str(), Gui.WholeLog.c_str() + Gui.WholeLog.size());
    }
    ImGui::End();
}

void FOServer::OnNewConnection(NetConnection* connection)
{
    // Allocate client
    ProtoCritter* cl_proto = ProtoMngr.GetProtoCritter(_str("Player").toHash());
    RUNTIME_ASSERT(cl_proto);
    Client* cl = new Client(connection, cl_proto, Settings, ScriptSys);

    // Add to connected collection
    {
        SCOPE_LOCK(ConnectedClientsLocker);

        cl->GameState = STATE_CONNECTED;
        ConnectedClients.push_back(cl);
        Statistics.CurOnline++;
    }
}

#define CHECK_BUSY \
    if (cl->IsBusy()) \
    { \
        cl->Connection->Bin.MoveReadPos(-int(sizeof(msg))); \
        return; \
    }

void FOServer::Process(Client* cl)
{
    if (cl->IsOffline() || cl->IsDestroyed)
    {
        SCOPE_LOCK(cl->Connection->BinLocker);
        cl->Connection->Bin.Reset();
        return;
    }

    if (cl->GameState == STATE_CONNECTED)
    {
        SCOPE_LOCK(cl->Connection->BinLocker);

        if (cl->Connection->Bin.IsEmpty())
            cl->Connection->Bin.Reset();

        cl->Connection->Bin.Refresh();

        if (cl->Connection->Bin.NeedProcess())
        {
            uint msg = 0;
            cl->Connection->Bin >> msg;

            uint tick = Timer::FastTick();
            switch (msg)
            {
            case 0xFFFFFFFF: {
                // At least 16 bytes should be sent for backward compatibility,
                // even if answer data will change its meaning
                CLIENT_OUTPUT_BEGIN(cl);
                cl->Connection->DisableCompression();
                cl->Connection->Bout << (uint)Statistics.CurOnline - 1;
                cl->Connection->Bout << (uint)Statistics.Uptime;
                cl->Connection->Bout << (uint)0;
                cl->Connection->Bout << (uchar)0;
                cl->Connection->Bout << (uchar)0xF0;
                cl->Connection->Bout << (ushort)FO_VERSION;
                CLIENT_OUTPUT_END(cl);

                cl->Disconnect();
                break;
            }
            case NETMSG_PING:
                Process_Ping(cl);
                break;
            case NETMSG_LOGIN:
                Process_LogIn(cl);
                break;
            case NETMSG_CREATE_CLIENT:
                Process_CreateClient(cl);
                break;
            case NETMSG_UPDATE:
                Process_Update(cl);
                break;
            case NETMSG_GET_UPDATE_FILE:
                Process_UpdateFile(cl);
                break;
            case NETMSG_GET_UPDATE_FILE_DATA:
                Process_UpdateFileData(cl);
                break;
            case NETMSG_RPC:
                // ScriptSys.HandleRpc(cl);
                break;
            default:
                cl->Connection->Bin.SkipMsg(msg);
                break;
            }

            cl->LastActivityTime = Timer::FastTick();
        }
        else
        {
            CHECK_IN_BUFF_ERROR_EXT(cl, (void)0, (void)0);
        }

        if (cl->GameState == STATE_CONNECTED && cl->LastActivityTime &&
            Timer::FastTick() - cl->LastActivityTime > PING_CLIENT_LIFE_TIME) // Kick bot
        {
            WriteLog("Connection timeout, client kicked, maybe bot. Ip '{}'.\n", cl->GetIpStr());
            cl->Disconnect();
        }
    }
    else if (cl->GameState == STATE_TRANSFERRING)
    {
        SCOPE_LOCK(cl->Connection->BinLocker);

        if (cl->Connection->Bin.IsEmpty())
            cl->Connection->Bin.Reset();

        cl->Connection->Bin.Refresh();

        if (cl->Connection->Bin.NeedProcess())
        {
            uint msg = 0;
            cl->Connection->Bin >> msg;

            uint tick = Timer::FastTick();
            switch (msg)
            {
            case NETMSG_PING:
                Process_Ping(cl);
                break;
            case NETMSG_SEND_GIVE_MAP:
                CHECK_BUSY;
                Process_GiveMap(cl);
                break;
            case NETMSG_SEND_LOAD_MAP_OK:
                CHECK_BUSY;
                Process_ParseToGame(cl);
                break;
            default:
                cl->Connection->Bin.SkipMsg(msg);
                break;
            }
        }
        else
        {
            CHECK_IN_BUFF_ERROR_EXT(cl, (void)0, (void)0);
        }
    }
    else if (cl->GameState == STATE_PLAYING)
    {
        static const int messages_per_cycle = 5;
        for (int i = 0; i < messages_per_cycle; i++)
        {
            SCOPE_LOCK(cl->Connection->BinLocker);

            if (cl->Connection->Bin.IsEmpty())
                cl->Connection->Bin.Reset();

            cl->Connection->Bin.Refresh();

            if (!cl->Connection->Bin.NeedProcess())
            {
                CHECK_IN_BUFF_ERROR_EXT(cl, (void)0, (void)0);
                break;
            }

            uint msg = 0;
            cl->Connection->Bin >> msg;

            uint tick = Timer::FastTick();
            switch (msg)
            {
            case NETMSG_PING:
                Process_Ping(cl);
                continue;
            case NETMSG_SEND_TEXT:
                Process_Text(cl);
                continue;
            case NETMSG_SEND_COMMAND:
                Process_Command(
                    cl->Connection->Bin, [cl](auto s) { cl->Send_Text(cl, _str(s).trim(), SAY_NETMSG); }, cl, "");
                continue;
            case NETMSG_DIR:
                CHECK_BUSY;
                Process_Dir(cl);
                continue;
            case NETMSG_SEND_MOVE_WALK:
                CHECK_BUSY;
                Process_Move(cl);
                continue;
            case NETMSG_SEND_MOVE_RUN:
                CHECK_BUSY;
                Process_Move(cl);
                continue;
            case NETMSG_SEND_TALK_NPC:
                CHECK_BUSY;
                Process_Dialog(cl);
                continue;
            case NETMSG_SEND_REFRESH_ME:
                cl->Send_LoadMap(nullptr, MapMngr);
                continue;
            case NETMSG_SEND_GET_INFO:
                cl->Send_GameInfo(MapMngr.GetMap(cl->GetMapId()));
                continue;
            case NETMSG_SEND_GIVE_MAP:
                CHECK_BUSY;
                Process_GiveMap(cl);
                continue;
            case NETMSG_RPC:
                CHECK_BUSY;
                // ScriptSys.HandleRpc(cl);
                continue;
            case NETMSG_SEND_POD_PROPERTY(1, 0):
            case NETMSG_SEND_POD_PROPERTY(1, 1):
            case NETMSG_SEND_POD_PROPERTY(1, 2):
                Process_Property(cl, 1);
                continue;
            case NETMSG_SEND_POD_PROPERTY(2, 0):
            case NETMSG_SEND_POD_PROPERTY(2, 1):
            case NETMSG_SEND_POD_PROPERTY(2, 2):
                Process_Property(cl, 2);
                continue;
            case NETMSG_SEND_POD_PROPERTY(4, 0):
            case NETMSG_SEND_POD_PROPERTY(4, 1):
            case NETMSG_SEND_POD_PROPERTY(4, 2):
                Process_Property(cl, 4);
                continue;
            case NETMSG_SEND_POD_PROPERTY(8, 0):
            case NETMSG_SEND_POD_PROPERTY(8, 1):
            case NETMSG_SEND_POD_PROPERTY(8, 2):
                Process_Property(cl, 8);
                continue;
            case NETMSG_SEND_COMPLEX_PROPERTY:
                Process_Property(cl, 0);
                continue;
            default:
                cl->Connection->Bin.SkipMsg(msg);
                continue;
            }

            cl->Connection->Bin.SkipMsg(msg);
        }
    }
}

void FOServer::Process_Text(Client* cl)
{
    uint msg_len = 0;
    uchar how_say = 0;
    ushort len = 0;
    char str[UTF8_BUF_SIZE(MAX_CHAT_MESSAGE)];

    cl->Connection->Bin >> msg_len;
    cl->Connection->Bin >> how_say;
    cl->Connection->Bin >> len;
    CHECK_IN_BUFF_ERROR(cl);

    if (!len || len >= sizeof(str))
    {
        WriteLog("Buffer zero sized or too large, length {}. Disconnect.\n", len);
        cl->Disconnect();
        return;
    }

    cl->Connection->Bin.Pop(str, len);
    str[len] = 0;
    CHECK_IN_BUFF_ERROR(cl);

    if (!cl->IsLife() && how_say >= SAY_NORM && how_say <= SAY_RADIO)
        how_say = SAY_WHISP;

    if (Str::Compare(str, cl->LastSay))
    {
        cl->LastSayEqualCount++;
        if (cl->LastSayEqualCount >= 10)
        {
            WriteLog("Flood detected, client '{}'. Disconnect.\n", cl->GetName());
            cl->Disconnect();
            return;
        }
        if (cl->LastSayEqualCount >= 3)
            return;
    }
    else
    {
        Str::Copy(cl->LastSay, str);
        cl->LastSayEqualCount = 0;
    }

    UShortVec channels;

    switch (how_say)
    {
    case SAY_NORM: {
        if (cl->GetMapId())
            cl->SendAA_Text(cl->VisCr, str, SAY_NORM, true);
        else
            cl->SendAA_Text(*cl->GlobalMapGroup, str, SAY_NORM, true);
    }
    break;
    case SAY_SHOUT: {
        if (cl->GetMapId())
        {
            Map* map = MapMngr.GetMap(cl->GetMapId());
            if (!map)
                return;

            cl->SendAA_Text(map->GetCritters(), str, SAY_SHOUT, true);
        }
        else
        {
            cl->SendAA_Text(*cl->GlobalMapGroup, str, SAY_SHOUT, true);
        }
    }
    break;
    case SAY_EMOTE: {
        if (cl->GetMapId())
            cl->SendAA_Text(cl->VisCr, str, SAY_EMOTE, true);
        else
            cl->SendAA_Text(*cl->GlobalMapGroup, str, SAY_EMOTE, true);
    }
    break;
    case SAY_WHISP: {
        if (cl->GetMapId())
            cl->SendAA_Text(cl->VisCr, str, SAY_WHISP, true);
        else
            cl->Send_TextEx(cl->GetId(), str, SAY_WHISP, true);
    }
    break;
    case SAY_SOCIAL: {
        return;
    }
    break;
    case SAY_RADIO: {
        if (cl->GetMapId())
            cl->SendAA_Text(cl->VisCr, str, SAY_WHISP, true);
        else
            cl->Send_TextEx(cl->GetId(), str, SAY_WHISP, true);

        ItemMngr.RadioSendText(cl, str, true, 0, 0, channels);
        if (channels.empty())
        {
            cl->Send_TextMsg(cl, STR_RADIO_CANT_SEND, SAY_NETMSG, TEXTMSG_GAME);
            return;
        }
    }
    break;
    default:
        return;
    }

    // Text listen
    int listen_count = 0;
    ScriptFunc<void, Critter*, string> listen_func[100]; // 100 calls per one message is enough
    string listen_str[100];

    {
        SCOPE_LOCK(TextListenersLocker);

        if (how_say == SAY_RADIO)
        {
            for (uint i = 0; i < TextListeners.size(); i++)
            {
                TextListen& tl = TextListeners[i];
                if (tl.SayType == SAY_RADIO &&
                    std::find(channels.begin(), channels.end(), tl.Parameter) != channels.end() &&
                    _str(string(str).substr(0, tl.FirstStr.length())).compareIgnoreCaseUtf8(tl.FirstStr))
                {
                    listen_func[listen_count] = tl.Func;
                    listen_str[listen_count] = str;
                    if (++listen_count >= 100)
                        break;
                }
            }
        }
        else
        {
            Map* map = MapMngr.GetMap(cl->GetMapId());
            hash pid = (map ? map->GetProtoId() : 0);
            for (uint i = 0; i < TextListeners.size(); i++)
            {
                TextListen& tl = TextListeners[i];
                if (tl.SayType == how_say && tl.Parameter == pid &&
                    _str(string(str).substr(0, tl.FirstStr.length())).compareIgnoreCaseUtf8(tl.FirstStr))
                {
                    listen_func[listen_count] = tl.Func;
                    listen_str[listen_count] = str;
                    if (++listen_count >= 100)
                        break;
                }
            }
        }
    }

    for (int i = 0; i < listen_count; i++)
        listen_func[i](cl, listen_str[i]);
}

void FOServer::Process_Command(NetBuffer& buf, LogFunc logcb, Client* cl_, const string& admin_panel)
{
    LogToFunc("Process_Command", logcb, true);
    Process_CommandReal(buf, logcb, cl_, admin_panel);
    LogToFunc("Process_Command", logcb, false);
}

void FOServer::Process_CommandReal(NetBuffer& buf, LogFunc logcb, Client* cl_, const string& admin_panel)
{
    uint msg_len = 0;
    uchar cmd = 0;

    buf >> msg_len;
    buf >> cmd;

    string sstr = (cl_ ? "" : admin_panel);
    bool allow_command = ScriptSys.PlayerAllowCommandEvent(cl_, sstr, cmd);

    if (!allow_command && !cl_)
    {
        logcb("Command refused by script.");
        return;
    }

#define CHECK_ALLOW_COMMAND \
    if (!allow_command) \
    return
#define CHECK_ADMIN_PANEL \
    if (!cl_) \
    { \
        logcb("Can't execute this command in admin panel."); \
        return; \
    }
    switch (cmd)
    {
    case CMD_EXIT: {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        cl_->Disconnect();
    }
    break;
    case CMD_MYINFO: {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        string istr = _str("|0xFF00FF00 Name: |0xFFFF0000 {}|0xFF00FF00 , Id: |0xFFFF0000 {}|0xFF00FF00 , Access: ",
            cl_->GetName(), cl_->GetId());
        switch (cl_->Access)
        {
        case ACCESS_CLIENT:
            istr += "|0xFFFF0000 Client|0xFF00FF00 .";
            break;
        case ACCESS_TESTER:
            istr += "|0xFFFF0000 Tester|0xFF00FF00 .";
            break;
        case ACCESS_MODER:
            istr += "|0xFFFF0000 Moderator|0xFF00FF00 .";
            break;
        case ACCESS_ADMIN:
            istr += "|0xFFFF0000 Administrator|0xFF00FF00 .";
            break;
        default:
            break;
        }

        logcb(istr);
    }
    break;
    case CMD_GAMEINFO: {
        int info;
        buf >> info;

        CHECK_ALLOW_COMMAND;

        string result;
        switch (info)
        {
        case 1:
            result = GetIngamePlayersStatistics();
            break;
        case 2:
            result = MapMngr.GetLocationsMapsStatistics();
            break;
        case 3:
            // result = ScriptSys.GetDeferredCallsStatistics();
            break;
        case 4:
            result = "WIP";
            break;
        case 5:
            result = ItemMngr.GetItemsStatistics();
            break;
        default:
            break;
        }

        string str;
        {
            SCOPE_LOCK(ConnectedClientsLocker);

            str = _str(
                "Connections: {}, Players: {}, Npc: {}. FOServer machine uptime: {} min., FOServer uptime: {} min.",
                ConnectedClients.size(), CrMngr.PlayersInGame(), CrMngr.NpcInGame(), Timer::FastTick() / 1000 / 60,
                (Timer::FastTick() - Statistics.ServerStartTick) / 1000 / 60);
        }

        result += str;

        for (const auto& line : _str(result).split('\n'))
            logcb(line);
    }
    break;
    case CMD_CRITID: {
        string name;
        buf >> name;

        CHECK_ALLOW_COMMAND;

        uint id = MAKE_CLIENT_ID(name);
        if (!DbStorage->Get("Players", id).empty())
            logcb(_str("Client id is {}.", id));
        else
            logcb("Client not found.");
    }
    break;
    case CMD_MOVECRIT: {
        uint crid;
        ushort hex_x;
        ushort hex_y;
        buf >> crid;
        buf >> hex_x;
        buf >> hex_y;

        CHECK_ALLOW_COMMAND;

        Critter* cr = CrMngr.GetCritter(crid);
        if (!cr)
        {
            logcb("Critter not found.");
            break;
        }

        Map* map = MapMngr.GetMap(cr->GetMapId());
        if (!map)
        {
            logcb("Critter is on global.");
            break;
        }

        if (hex_x >= map->GetWidth() || hex_y >= map->GetHeight())
        {
            logcb("Invalid hex position.");
            break;
        }

        if (MapMngr.Transit(cr, map, hex_x, hex_y, cr->GetDir(), 3, 0, true))
            logcb("Critter move success.");
        else
            logcb("Critter move fail.");
    }
    break;
    case CMD_DISCONCRIT: {
        uint crid;
        buf >> crid;

        CHECK_ALLOW_COMMAND;

        if (cl_ && cl_->GetId() == crid)
        {
            logcb("To kick yourself type <~exit>");
            return;
        }

        Critter* cr = CrMngr.GetCritter(crid);
        if (!cr)
        {
            logcb("Critter not found.");
            return;
        }

        if (!cr->IsPlayer())
        {
            logcb("Founded critter is not a player.");
            return;
        }

        Client* cl2 = (Client*)cr;
        if (cl2->GameState != STATE_PLAYING)
        {
            logcb("Player is not in a game.");
            return;
        }

        cl2->Send_Text(cl2, "You are kicked from game.", SAY_NETMSG);
        cl2->Disconnect();

        logcb("Player disconnected.");
    }
    break;
    case CMD_TOGLOBAL: {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if (!cl_->IsLife())
        {
            logcb("To global fail, only life none.");
            break;
        }

        if (MapMngr.TransitToGlobal(cl_, 0, false) == true)
            logcb("To global success.");
        else
            logcb("To global fail.");
    }
    break;
    case CMD_PROPERTY: {
        uint crid;
        string property_name;
        int property_value;
        buf >> crid;
        buf >> property_name;
        buf >> property_value;

        CHECK_ALLOW_COMMAND;

        Critter* cr = (!crid ? cl_ : CrMngr.GetCritter(crid));
        if (cr)
        {
            Property* prop = Critter::PropertiesRegistrator->Find(property_name);
            if (!prop)
            {
                logcb("Property not found.");
                return;
            }
            if (!prop->IsPOD() || prop->GetBaseSize() != 4)
            {
                logcb("For now you can modify only int/uint properties.");
                return;
            }

            cr->Props.SetValue<int>(prop, property_value);
            logcb("Done.");
        }
        else
        {
            logcb("Critter not found.");
        }
    }
    break;
    case CMD_GETACCESS: {
        string name_access;
        string pasw_access;
        buf >> name_access;
        buf >> pasw_access;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        StrVec client, tester, moder, admin, admin_names;
        GetAccesses(client, tester, moder, admin, admin_names);

        int wanted_access = -1;
        if (name_access == "client" && std::find(client.begin(), client.end(), pasw_access) != client.end())
            wanted_access = ACCESS_CLIENT;
        else if (name_access == "tester" && std::find(tester.begin(), tester.end(), pasw_access) != tester.end())
            wanted_access = ACCESS_TESTER;
        else if (name_access == "moder" && std::find(moder.begin(), moder.end(), pasw_access) != moder.end())
            wanted_access = ACCESS_MODER;
        else if (name_access == "admin" && std::find(admin.begin(), admin.end(), pasw_access) != admin.end())
            wanted_access = ACCESS_ADMIN;

        bool allow = false;
        if (wanted_access != -1)
        {
            string pass = pasw_access;
            allow = ScriptSys.PlayerGetAccessEvent(cl_, wanted_access, pass);
        }

        if (!allow)
        {
            logcb("Access denied.");
            break;
        }

        cl_->Access = wanted_access;
        logcb("Access changed.");
    }
    break;
    case CMD_ADDITEM: {
        ushort hex_x;
        ushort hex_y;
        hash pid;
        uint count;
        buf >> hex_x;
        buf >> hex_y;
        buf >> pid;
        buf >> count;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        Map* map = MapMngr.GetMap(cl_->GetMapId());
        if (!map || hex_x >= map->GetWidth() || hex_y >= map->GetHeight())
        {
            logcb("Wrong hexes or critter on global map.");
            return;
        }

        if (!CreateItemOnHex(map, hex_x, hex_y, pid, count, nullptr, true))
        {
            logcb("Item(s) not added.");
        }
        else
        {
            logcb("Item(s) added.");
        }
    }
    break;
    case CMD_ADDITEM_SELF: {
        hash pid;
        uint count;
        buf >> pid;
        buf >> count;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if (ItemMngr.AddItemCritter(cl_, pid, count) != nullptr)
            logcb("Item(s) added.");
        else
            logcb("Item(s) added fail.");
    }
    break;
    case CMD_ADDNPC: {
        ushort hex_x;
        ushort hex_y;
        uchar dir;
        hash pid;
        buf >> hex_x;
        buf >> hex_y;
        buf >> dir;
        buf >> pid;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        Map* map = MapMngr.GetMap(cl_->GetMapId());
        Npc* npc = CrMngr.CreateNpc(pid, nullptr, map, hex_x, hex_y, dir, true);
        if (!npc)
            logcb("Npc not created.");
        else
            logcb("Npc created.");
    }
    break;
    case CMD_ADDLOCATION: {
        ushort wx;
        ushort wy;
        hash pid;
        buf >> wx;
        buf >> wy;
        buf >> pid;

        CHECK_ALLOW_COMMAND;

        Location* loc = MapMngr.CreateLocation(pid, wx, wy);
        if (!loc)
            logcb("Location not created.");
        else
            logcb("Location created.");
    }
    break;
    case CMD_RUNSCRIPT: {
        string func_name;
        uint param0, param1, param2;
        buf >> func_name;
        buf >> param0;
        buf >> param1;
        buf >> param2;

        CHECK_ALLOW_COMMAND;

        if (func_name.empty())
        {
            logcb("Fail, length is zero.");
            break;
        }

        if (ScriptSys.CallFunc<void, Critter*, int, int, int>(func_name, cl_, param0, param1, param2))
            logcb("Run script success.");
        else
            logcb("Run script fail.");
    }
    break;
    case CMD_REGENMAP: {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        // Check global
        if (!cl_->GetMapId())
        {
            logcb("Only on local map.");
            return;
        }

        // Find map
        Map* map = MapMngr.GetMap(cl_->GetMapId());
        if (!map)
        {
            logcb("Map not found.");
            return;
        }

        // Regenerate
        ushort hx = cl_->GetHexX();
        ushort hy = cl_->GetHexY();
        uchar dir = cl_->GetDir();
        MapMngr.RegenerateMap(map);
        MapMngr.Transit(cl_, map, hx, hy, dir, 5, 0, true);
        logcb("Regenerate map complete.");
    }
    break;
    case CMD_SETTIME: {
        int multiplier;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        buf >> multiplier;
        buf >> year;
        buf >> month;
        buf >> day;
        buf >> hour;
        buf >> minute;
        buf >> second;

        CHECK_ALLOW_COMMAND;

        SetGameTime(multiplier, year, month, day, hour, minute, second);
        logcb("Time changed.");
    }
    break;
    case CMD_BAN: {
        string name;
        string params;
        uint ban_hours;
        string info;
        buf >> name;
        buf >> params;
        buf >> ban_hours;
        buf >> info;

        CHECK_ALLOW_COMMAND;

        SCOPE_LOCK(BannedLocker);

        if (_str(params).compareIgnoreCase("list"))
        {
            if (Banned.empty())
            {
                logcb("Ban list empty.");
                return;
            }

            uint index = 1;
            for (auto it = Banned.begin(), end = Banned.end(); it != end; ++it)
            {
                ClientBanned& ban = *it;
                logcb(_str("--- {:3} ---", index));
                if (ban.ClientName[0])
                    logcb(_str("User: {}", ban.ClientName));
                if (ban.ClientIp)
                    logcb(_str("UserIp: {}", ban.ClientIp));
                logcb(_str("BeginTime: {} {} {} {} {}", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day,
                    ban.BeginTime.Hour, ban.BeginTime.Minute));
                logcb(_str("EndTime: {} {} {} {} {}", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day,
                    ban.EndTime.Hour, ban.EndTime.Minute));
                if (ban.BannedBy[0])
                    logcb(_str("BannedBy: {}", ban.BannedBy));
                if (ban.BanInfo[0])
                    logcb(_str("Comment: {}", ban.BanInfo));
                index++;
            }
        }
        else if (_str(params).compareIgnoreCase("add") || _str(params).compareIgnoreCase("add+"))
        {
            uint name_len = _str(name).lengthUtf8();
            if (name_len < MIN_NAME || name_len < Settings.MinNameLength || name_len > MAX_NAME ||
                name_len > Settings.MaxNameLength || !ban_hours)
            {
                logcb("Invalid arguments.");
                return;
            }

            uint id = MAKE_CLIENT_ID(name);
            Client* cl_banned = CrMngr.GetPlayer(id);
            ClientBanned ban;
            memzero(&ban, sizeof(ban));
            Str::Copy(ban.ClientName, name.c_str());
            ban.ClientIp = (cl_banned && params.find('+') != string::npos ? cl_banned->GetIp() : 0);
            Timer::GetCurrentDateTime(ban.BeginTime);
            ban.EndTime = ban.BeginTime;
            Timer::ContinueTime(ban.EndTime, ban_hours * 60 * 60);
            Str::Copy(ban.BannedBy, cl_ ? cl_->Name.c_str() : admin_panel.c_str());
            Str::Copy(ban.BanInfo, info.c_str());

            Banned.push_back(ban);
            SaveBan(ban, false);
            logcb("User banned.");

            if (cl_banned)
            {
                cl_banned->Send_TextMsg(cl_banned, STR_NET_BAN, SAY_NETMSG, TEXTMSG_GAME);
                cl_banned->Send_TextMsgLex(
                    cl_banned, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban.GetBanLexems().c_str());
                cl_banned->Disconnect();
            }
        }
        else if (_str(params).compareIgnoreCase("delete"))
        {
            if (name.empty())
            {
                logcb("Invalid arguments.");
                return;
            }

            bool resave = false;
            if (name == "*")
            {
                int index = (int)ban_hours - 1;
                if (index >= 0 && index < (int)Banned.size())
                {
                    Banned.erase(Banned.begin() + index);
                    resave = true;
                }
            }
            else
            {
                for (auto it = Banned.begin(); it != Banned.end();)
                {
                    ClientBanned& ban = *it;
                    if (_str(ban.ClientName).compareIgnoreCaseUtf8(name))
                    {
                        SaveBan(ban, true);
                        it = Banned.erase(it);
                        resave = true;
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            if (resave)
            {
                SaveBans();
                logcb("User unbanned.");
            }
            else
            {
                logcb("User not found.");
            }
        }
        else
        {
            logcb("Unknown option.");
        }
    }
    break;
    case CMD_DELETE_ACCOUNT: {
        char pass_hash[PASS_HASH_SIZE];
        buf.Pop(pass_hash, PASS_HASH_SIZE);

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if (memcmp(cl_->GetPassword().c_str(), pass_hash, PASS_HASH_SIZE))
        {
            logcb("Invalid password.");
        }
        else
        {
            if (!cl_->GetClientToDelete())
            {
                cl_->SetClientToDelete(true);
                logcb("Your account will be deleted after character exit from game.");
            }
            else
            {
                cl_->SetClientToDelete(false);
                logcb("Deleting canceled.");
            }
        }
    }
    break;
    case CMD_CHANGE_PASSWORD: {
        char pass_hash[PASS_HASH_SIZE];
        char new_pass_hash[PASS_HASH_SIZE];
        buf.Pop(pass_hash, PASS_HASH_SIZE);
        buf.Pop(new_pass_hash, PASS_HASH_SIZE);

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if (memcmp(cl_->GetPassword().c_str(), pass_hash, PASS_HASH_SIZE))
        {
            logcb("Invalid current password.");
        }
        else
        {
            cl_->SetPassword(new_pass_hash);
            logcb("Password changed.");
        }
    }
    break;
    case CMD_LOG: {
        char flags[16];
        buf.Pop(flags, 16);

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        int action = -1;
        if (flags[0] == '-' && flags[1] == '-')
            action = 2; // Detach all
        else if (flags[0] == '-')
            action = 0; // Detach current
        else if (flags[0] == '+')
            action = 1; // Attach current
        else
        {
            logcb("Wrong flags. Valid is '+', '-', '--'.");
            return;
        }

        LogToFunc("LogToClients", std::bind(&FOServer::LogToClients, this, std::placeholders::_1), false);
        auto it = std::find(LogClients.begin(), LogClients.end(), cl_);
        if (action == 0 && it != LogClients.end()) // Detach current
        {
            logcb("Detached.");
            cl_->Release();
            LogClients.erase(it);
        }
        else if (action == 1 && it == LogClients.end()) // Attach current
        {
            logcb("Attached.");
            cl_->AddRef();
            LogClients.push_back(cl_);
        }
        else if (action == 2) // Detach all
        {
            logcb("Detached all.");
            for (auto it_ = LogClients.begin(); it_ < LogClients.end(); ++it_)
                (*it_)->Release();
            LogClients.clear();
        }
        if (!LogClients.empty())
            LogToFunc("LogToClients", std::bind(&FOServer::LogToClients, this, std::placeholders::_1), true);
    }
    break;
    case CMD_DEV_EXEC:
    case CMD_DEV_FUNC:
    case CMD_DEV_GVAR: {
        /*ushort command_len = 0;
        char command_raw[MAX_FOTEXT];
        buf >> command_len;
        buf.Pop(command_raw, command_len);
        command_raw[command_len] = 0;

        string command = _str(command_raw).trim();
        string console_name = _str("DevConsole ({})", cl_ ? cl_->GetName() : admin_panel);

        // Get module
        asIScriptEngine* engine = ScriptSys.GetEngine();
        asIScriptModule* mod = engine->GetModule(console_name.c_str());
        if (!mod)
        {
            string base_code;
            File dev_file = FileMngr.ReadFile("Scripts/__dev.fos");
            if (dev_file)
                base_code = dev_file.GetCStr();
            else
                base_code = "void Dummy(){}";

            mod = engine->GetModule(console_name.c_str(), asGM_ALWAYS_CREATE);
            int r = mod->AddScriptSection("DevConsole", base_code.c_str());
            if (r < 0)
            {
                mod->Discard();
                break;
            }

            r = mod->Build();
            if (r < 0)
            {
                mod->Discard();
                break;
            }

            r = mod->BindAllImportedFunctions();
            if (r < 0)
            {
                mod->Discard();
                break;
            }
        }

        // Execute command
        if (cmd == CMD_DEV_EXEC)
        {
            WriteLog("{} : Execute '{}'.\n", console_name, command);

            string func_code = _str("void Execute(){\n{};\n}", command);
            asIScriptFunction* func = nullptr;
            int r = mod->CompileFunction("DevConsole", func_code.c_str(), -1, asCOMP_ADD_TO_MODULE, &func);
            if (r >= 0)
            {
                asIScriptContext* ctx = engine->RequestContext();
                if (ctx->Prepare(func) >= 0)
                    ctx->Execute();
                mod->RemoveFunction(func);
                func->Release();
                engine->ReturnContext(ctx);
            }
        }
        else if (cmd == CMD_DEV_FUNC)
        {
            WriteLog("{} : Set function '{}'.\n", console_name, command);

            mod->CompileFunction("DevConsole", command.c_str(), 0, asCOMP_ADD_TO_MODULE, nullptr);
        }
        else if (cmd == CMD_DEV_GVAR)
        {
            WriteLog("{} : Set global var '{}'.\n", console_name, command);

            mod->CompileGlobalVar("DevConsole", command.c_str(), 0);
        }*/
    }
    break;
    default:
        logcb("Unknown command.");
        break;
    }
}

void FOServer::SetGameTime(int multiplier, int year, int month, int day, int hour, int minute, int second)
{
    RUNTIME_ASSERT((multiplier >= 1 && multiplier <= 50000));
    RUNTIME_ASSERT(
        (Globals->GetYearStart() == 0 || (year >= Globals->GetYearStart() && year <= Globals->GetYearStart() + 130)));
    RUNTIME_ASSERT((month >= 1 && month <= 12));
    RUNTIME_ASSERT((day >= 1 && day <= 31));
    RUNTIME_ASSERT((hour >= 0 && hour <= 23));
    RUNTIME_ASSERT((minute >= 0 && minute <= 59));
    RUNTIME_ASSERT((second >= 0 && second <= 59));

    Globals->SetTimeMultiplier(multiplier);
    Globals->SetYear(year);
    Globals->SetMonth(month);
    Globals->SetDay(day);
    Globals->SetHour(hour);
    Globals->SetMinute(minute);
    Globals->SetSecond(second);

    if (Globals->GetYearStart() == 0)
        Globals->SetYearStart(year);

    GameTime.Reset();

    {
        SCOPE_LOCK(ConnectedClientsLocker);

        for (auto it = ConnectedClients.begin(), end = ConnectedClients.end(); it != end; ++it)
        {
            Client* cl = *it;
            if (cl->IsOnline())
                cl->Send_GameInfo(MapMngr.GetMap(cl->GetMapId()));
        }
    }
}

bool FOServer::Init()
{
    Active = InitReal();
    ActiveInProcess = false;
    return Active;
}

bool FOServer::InitReal()
{
    WriteLog("***   Starting initialization   ***\n");

    // Root data file
    FileMngr.AddDataSource("./", false);

    // Delete intermediate files if engine have been updated
    File version_file = FileMngr.ReadFile("Version.txt");
    if (!version_file || _str("0x{}", version_file.GetCStr()).toInt() != FO_VERSION)
    {
        OutputFile out_version_file = FileMngr.WriteFile("Version.txt");
        out_version_file.SetStr(_str("{:#x}", FO_VERSION));
        out_version_file.Save();
        FileMngr.DeleteDir("Update/");
        FileMngr.DeleteDir("Binaries/");
    }

    // Generic
    ConnectedClients.clear();
    RegIp.clear();

    // Reserve memory
    ConnectedClients.reserve(10000);

    // Data base
    WriteLog("Initialize storage data base at '{}'.\n", Settings.DbStorage);
    DbStorage = GetDataBase(Settings.DbStorage);
    if (!DbStorage)
        return false;

    if (Settings.DbHistory != "None")
    {
        WriteLog("Initialize history data base at '{}'.\n", Settings.DbHistory);
        DbHistory = GetDataBase(Settings.DbHistory);
        if (!DbHistory)
            return false;
    }

    // Start data base changes
    DbStorage->StartChanges();
    if (DbHistory)
        DbHistory->StartChanges();

    // PropertyRegistrator::GlobalSetCallbacks.push_back(EntitySetValue);

    if (!InitLangPacks(LangPacks))
        return false; // Language packs

    LoadBans();

    // Managers
    if (!DlgMngr.LoadDialogs())
        return false; // Dialog manager
    // if (!ProtoMngr.LoadProtosFromFiles(FileMngr))
    //    return false;

    // Language packs
    if (!InitLangPacksDialogs(LangPacks))
        return false; // Create FODLG.MSG, need call after InitLangPacks and DlgMngr.LoadDialogs
    if (!InitLangPacksLocations(LangPacks))
        return false; // Create FOLOCATIONS.MSG, need call after InitLangPacks and MapMngr.LoadLocationsProtos
    if (!InitLangPacksItems(LangPacks))
        return false; // Create FOITEM.MSG, need call after InitLangPacks and ItemMngr.LoadProtos

    // Load globals
    Globals->SetId(1);
    DataBase::Document globals_doc = DbStorage->Get("Globals", 1);
    if (globals_doc.empty())
    {
        DbStorage->Insert("Globals", Globals->Id, {{"_Proto", string("")}});
    }
    else
    {
        if (!PropertiesSerializator::LoadFromDbDocument(&Globals->Props, globals_doc, ScriptSys))
        {
            WriteLog("Failed to load globals document.\n");
            return false;
        }

        GameTime.Reset();
    }

    // Todo: restore hashes loading
    // _str::loadHashes();

    // Deferred calls
    // if (!ScriptSys.LoadDeferredCalls())
    //    return false;

    // Modules initialization
    Timer::UpdateTick();

    // Update files
    StrVec resource_names;
    GenerateUpdateFiles(true, &resource_names);

    // Validate protos resources
    if (!ProtoMngr.ValidateProtoResources(resource_names))
        return false;

    // Initialization script
    Timer::UpdateTick();
    if (ScriptSys.InitEvent())
    {
        WriteLog("Initialization script failed.\n");
        return false;
    }

    // Init world
    if (globals_doc.empty())
    {
        // Generate world
        if (ScriptSys.GenerateWorldEvent())
        {
            WriteLog("Generate world script failed.\n");
            return false;
        }
    }
    else
    {
        // World loading
        if (!EntityMngr.LoadEntities())
            return false;
    }

    // Start script
    if (ScriptSys.StartEvent())
    {
        WriteLog("Start script fail.\n");
        return false;
    }

    // Commit data base changes
    DbStorage->CommitChanges();
    if (DbHistory)
        DbHistory->CommitChanges();

    // End of initialization
    Statistics.BytesSend = 0;
    Statistics.BytesRecv = 0;
    Statistics.DataReal = 1;
    Statistics.DataCompressed = 1;
    Statistics.ServerStartTick = Timer::FastTick();

    // Net
    WriteLog("Starting server on ports {} and {}.\n", Settings.Port, Settings.Port + 1);
    if (!(TcpServer = NetServerBase::StartTcpServer(
              Settings, std::bind(&FOServer::OnNewConnection, this, std::placeholders::_1))))
        return false;
    if (!(WebSocketsServer = NetServerBase::StartWebSocketsServer(
              Settings, std::bind(&FOServer::OnNewConnection, this, std::placeholders::_1))))
        return false;

    Active = true;
    return true;
}

bool FOServer::InitLangPacks(LangPackVec& lang_packs)
{
    WriteLog("Load language packs...\n");

    uint cur_lang = 0;
    while (true)
    {
        // Todo: restore settings
        string cur_str_lang; // = _str("Language_{}", cur_lang);
        string lang_name; // = MainConfig->GetStr("", cur_str_lang);
        if (lang_name.empty())
            break;

        if (lang_name.length() != 4)
        {
            WriteLog("Language name not equal to four letters.\n");
            return false;
        }

        uint pack_id = *(uint*)&lang_name;
        if (std::find(lang_packs.begin(), lang_packs.end(), pack_id) != lang_packs.end())
        {
            WriteLog("Language pack {} is already initialized.\n", cur_lang);
            return false;
        }

        LanguagePack lang;
        lang.LoadFromFiles(FileMngr, lang_name);

        lang_packs.push_back(lang);
        cur_lang++;
    }

    WriteLog("Load language packs complete, count {}.\n", cur_lang);
    return cur_lang > 0;
}

bool FOServer::InitLangPacksDialogs(LangPackVec& lang_packs)
{
    for (auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it)
        it->Msg[TEXTMSG_DLG].Clear();

    auto& all_protos = ProtoMngr.GetProtoCritters();
    for (auto& kv : all_protos)
    {
        ProtoCritter* proto = kv.second;
        for (uint i = 0, j = (uint)proto->TextsLang.size(); i < j; i++)
        {
            for (auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it)
            {
                LanguagePack& lang = *it;
                if (proto->TextsLang[i] != lang.Name)
                    continue;

                if (lang.Msg[TEXTMSG_DLG].IsIntersects(*proto->Texts[i]))
                    WriteLog(
                        "Warning! Proto item '{}' text intersection detected, send notification about this to developers.\n",
                        proto->GetName());

                lang.Msg[TEXTMSG_DLG] += *proto->Texts[i];
            }
        }
    }

    DialogPack* pack = nullptr;
    uint index = 0;
    while ((pack = DlgMngr.GetDialogByIndex(index++)))
    {
        for (uint i = 0, j = (uint)pack->TextsLang.size(); i < j; i++)
        {
            for (auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it)
            {
                LanguagePack& lang = *it;
                if (pack->TextsLang[i] != lang.Name)
                    continue;

                if (lang.Msg[TEXTMSG_DLG].IsIntersects(*pack->Texts[i]))
                    WriteLog(
                        "Warning! Dialog '{}' text intersection detected, send notification about this to developers.\n",
                        pack->PackName);

                lang.Msg[TEXTMSG_DLG] += *pack->Texts[i];
            }
        }
    }

    return true;
}

bool FOServer::InitLangPacksLocations(LangPackVec& lang_packs)
{
    for (auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it)
        it->Msg[TEXTMSG_LOCATIONS].Clear();

    auto& protos = ProtoMngr.GetProtoLocations();
    for (auto& kv : protos)
    {
        ProtoLocation* ploc = kv.second;
        for (uint i = 0, j = (uint)ploc->TextsLang.size(); i < j; i++)
        {
            for (auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it)
            {
                LanguagePack& lang = *it;
                if (ploc->TextsLang[i] != lang.Name)
                    continue;

                if (lang.Msg[TEXTMSG_LOCATIONS].IsIntersects(*ploc->Texts[i]))
                    WriteLog(
                        "Warning! Location '{}' text intersection detected, send notification about this to developers.\n",
                        _str().parseHash(ploc->ProtoId));

                lang.Msg[TEXTMSG_LOCATIONS] += *ploc->Texts[i];
            }
        }
    }

    return true;
}

bool FOServer::InitLangPacksItems(LangPackVec& lang_packs)
{
    for (auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it)
        it->Msg[TEXTMSG_ITEM].Clear();

    auto& protos = ProtoMngr.GetProtoItems();
    for (auto& kv : protos)
    {
        ProtoItem* proto = kv.second;
        for (uint i = 0, j = (uint)proto->TextsLang.size(); i < j; i++)
        {
            for (auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it)
            {
                LanguagePack& lang = *it;
                if (proto->TextsLang[i] != lang.Name)
                    continue;

                if (lang.Msg[TEXTMSG_ITEM].IsIntersects(*proto->Texts[i]))
                    WriteLog(
                        "Warning! Proto item '{}' text intersection detected, send notification about this to developers.\n",
                        proto->GetName());

                lang.Msg[TEXTMSG_ITEM] += *proto->Texts[i];
            }
        }
    }

    return true;
}

// Todo: clients logging may be not thread safe
void FOServer::LogToClients(const string& str)
{
    string str_fixed = str;
    if (!str_fixed.empty() && str.back() == '\n')
        str_fixed.erase(str.length() - 1);
    if (!str_fixed.empty())
    {
        for (auto it = LogClients.begin(); it < LogClients.end();)
        {
            Client* cl = *it;
            if (cl->IsOnline())
            {
                cl->Send_TextEx(0, str_fixed, SAY_NETMSG, false);
                ++it;
            }
            else
            {
                cl->Release();
                it = LogClients.erase(it);
            }
        }
        if (LogClients.empty())
            LogToFunc("LogToClients", std::bind(&FOServer::LogToClients, this, std::placeholders::_1), false);
    }
}

FOServer::ClientBanned* FOServer::GetBanByName(const char* name)
{
    auto it = std::find(Banned.begin(), Banned.end(), name);
    return it != Banned.end() ? &(*it) : nullptr;
}

FOServer::ClientBanned* FOServer::GetBanByIp(uint ip)
{
    auto it = std::find(Banned.begin(), Banned.end(), ip);
    return it != Banned.end() ? &(*it) : nullptr;
}

uint FOServer::GetBanTime(ClientBanned& ban)
{
    DateTimeStamp time;
    Timer::GetCurrentDateTime(time);
    int diff = Timer::GetTimeDifference(ban.EndTime, time) / 60 + 1;
    return diff > 0 ? diff : 1;
}

void FOServer::ProcessBans()
{
    SCOPE_LOCK(BannedLocker);

    bool resave = false;
    DateTimeStamp time;
    Timer::GetCurrentDateTime(time);
    for (auto it = Banned.begin(); it != Banned.end();)
    {
        DateTimeStamp& ban_time = it->EndTime;
        if (time.Year >= ban_time.Year && time.Month >= ban_time.Month && time.Day >= ban_time.Day &&
            time.Hour >= ban_time.Hour && time.Minute >= ban_time.Minute)
        {
            SaveBan(*it, true);
            it = Banned.erase(it);
            resave = true;
        }
        else
            ++it;
    }
    if (resave)
        SaveBans();
}

void FOServer::SaveBan(ClientBanned& ban, bool expired)
{
    SCOPE_LOCK(BannedLocker);

    const string ban_file_name = (expired ? BANS_FNAME_EXPIRED : BANS_FNAME_ACTIVE);
    OutputFile out_ban_file = FileMngr.WriteFile(ban_file_name, true);

    out_ban_file.SetStr("[Ban]\n");
    if (ban.ClientName[0])
        out_ban_file.SetStr(_str("User = {}\n", ban.ClientName));
    if (ban.ClientIp)
        out_ban_file.SetStr(_str("UserIp = {}\n", ban.ClientIp));
    out_ban_file.SetStr(_str("BeginTime = {} {} {} {} {}\n", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day,
        ban.BeginTime.Hour, ban.BeginTime.Minute));
    out_ban_file.SetStr(_str("EndTime = {} {} {} {} {}\n", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day,
        ban.EndTime.Hour, ban.EndTime.Minute));
    if (ban.BannedBy[0])
        out_ban_file.SetStr(_str("BannedBy = {}\n", ban.BannedBy));
    if (ban.BanInfo[0])
        out_ban_file.SetStr(_str("Comment = {}\n", ban.BanInfo));
    out_ban_file.SetStr("\n");

    out_ban_file.Save();
}

void FOServer::SaveBans()
{
    SCOPE_LOCK(BannedLocker);

    OutputFile out_ban_file = FileMngr.WriteFile(BANS_FNAME_ACTIVE);
    for (auto it = Banned.begin(), end = Banned.end(); it != end; ++it)
    {
        ClientBanned& ban = *it;
        out_ban_file.SetStr("[Ban]\n");
        if (ban.ClientName[0])
            out_ban_file.SetStr(_str("User = {}\n", ban.ClientName));
        if (ban.ClientIp)
            out_ban_file.SetStr(_str("UserIp = {}\n", ban.ClientIp));
        out_ban_file.SetStr(_str("BeginTime = {} {} {} {} {}\n", ban.BeginTime.Year, ban.BeginTime.Month,
            ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute));
        out_ban_file.SetStr(_str("EndTime = {} {} {} {} {}\n", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day,
            ban.EndTime.Hour, ban.EndTime.Minute));
        if (ban.BannedBy[0])
            out_ban_file.SetStr(_str("BannedBy = {}\n", ban.BannedBy));
        if (ban.BanInfo[0])
            out_ban_file.SetStr(_str("Comment = {}\n", ban.BanInfo));
        out_ban_file.SetStr("\n");
    }

    out_ban_file.Save();
}

void FOServer::LoadBans()
{
    SCOPE_LOCK(BannedLocker);

    Banned.clear();

    ConfigFile bans = FileMngr.ReadConfigFile(BANS_FNAME_ACTIVE);
    if (!bans)
        return;

    while (bans.IsApp("Ban"))
    {
        ClientBanned ban;
        memzero(&ban, sizeof(ban));
        DateTimeStamp time;
        memzero(&time, sizeof(time));
        string s;
        if (!(s = bans.GetStr("Ban", "User")).empty())
            Str::Copy(ban.ClientName, s.c_str());
        ban.ClientIp = bans.GetInt("Ban", "UserIp", 0);
        if (!(s = bans.GetStr("Ban", "BeginTime")).empty() &&
            sscanf(s.c_str(), "%hu%hu%hu%hu%hu", &time.Year, &time.Month, &time.Day, &time.Hour, &time.Minute))
            ban.BeginTime = time;
        if (!(s = bans.GetStr("Ban", "EndTime")).empty() &&
            sscanf(s.c_str(), "%hu%hu%hu%hu%hu", &time.Year, &time.Month, &time.Day, &time.Hour, &time.Minute))
            ban.EndTime = time;
        if (!(s = bans.GetStr("Ban", "BannedBy")).empty())
            Str::Copy(ban.BannedBy, s.c_str());
        if (!(s = bans.GetStr("Ban", "Comment")).empty())
            Str::Copy(ban.BanInfo, s.c_str());
        Banned.push_back(ban);

        bans.GotoNextApp("Ban");
    }
    ProcessBans();
}

void FOServer::GenerateUpdateFiles(bool first_generation /* = false */, StrVec* resource_names /* = nullptr */)
{
    if (!first_generation && UpdateFiles.empty())
        return;

    WriteLog("Generate update files...\n");

    // Disconnect all connected clients to force data updating
    if (!first_generation)
    {
        SCOPE_LOCK(ConnectedClientsLocker);

        for (Client* cl : ConnectedClients)
            cl->Disconnect();
    }

    // Clear collections
    for (auto it = UpdateFiles.begin(), end = UpdateFiles.end(); it != end; ++it)
        SAFEDELA(it->Data);
    UpdateFiles.clear();
    UpdateFilesList.clear();

    // Fill MSG
    UpdateFile update_file;
    for (LanguagePack& lang_pack : LangPacks)
    {
        for (int i = 0; i < TEXTMSG_COUNT; i++)
        {
            UCharVec msg_data;
            lang_pack.Msg[i].GetBinaryData(msg_data);

            string msg_cache_name = lang_pack.GetMsgCacheName(i);

            memzero(&update_file, sizeof(update_file));
            update_file.Size = (uint)msg_data.size();
            update_file.Data = new uchar[update_file.Size];
            memcpy(update_file.Data, &msg_data[0], update_file.Size);
            UpdateFiles.push_back(update_file);

            WriteData(UpdateFilesList, (short)msg_cache_name.length());
            WriteDataArr(UpdateFilesList, msg_cache_name.c_str(), (uint)msg_cache_name.length());
            WriteData(UpdateFilesList, update_file.Size);
            WriteData(UpdateFilesList, Hashing::MurmurHash2(update_file.Data, update_file.Size));
        }
    }

    // Fill prototypes
    UCharVec proto_items_data = ProtoMngr.GetProtosBinaryData();

    memzero(&update_file, sizeof(update_file));
    update_file.Size = (uint)proto_items_data.size();
    update_file.Data = new uchar[update_file.Size];
    memcpy(update_file.Data, &proto_items_data[0], update_file.Size);
    UpdateFiles.push_back(update_file);

    const string protos_cache_name = "$protos.cache";
    WriteData(UpdateFilesList, (short)protos_cache_name.length());
    WriteDataArr(UpdateFilesList, protos_cache_name.c_str(), (uint)protos_cache_name.length());
    WriteData(UpdateFilesList, update_file.Size);
    WriteData(UpdateFilesList, Hashing::MurmurHash2(update_file.Data, update_file.Size));

    // Fill files
    FileCollection files = FileMngr.FilterFiles("", "Update/");
    while (files.MoveNext())
    {
        File file = files.GetCurFile();

        memzero(&update_file, sizeof(update_file));
        update_file.Size = file.GetFsize();
        update_file.Data = file.ReleaseBuffer();
        UpdateFiles.push_back(update_file);

        string file_path = file.GetName().substr("Update/"_len);
        WriteData(UpdateFilesList, (short)file_path.length());
        WriteDataArr(UpdateFilesList, file_path.c_str(), (uint)file_path.length());
        WriteData(UpdateFilesList, update_file.Size);
        WriteData(UpdateFilesList, Hashing::MurmurHash2(update_file.Data, update_file.Size));
    }

    WriteLog("Generate update files complete.\n");

    // Append binaries
    FileCollection binaries = FileMngr.FilterFiles("", "Binaries/");
    while (binaries.MoveNext())
    {
        File file = binaries.GetCurFile();

        memzero(&update_file, sizeof(update_file));
        update_file.Size = file.GetFsize();
        update_file.Data = file.ReleaseBuffer();
        UpdateFiles.push_back(update_file);

        string file_path = file.GetName().substr("Binaries/"_len);
        WriteData(UpdateFilesList, (short)file_path.length());
        WriteDataArr(UpdateFilesList, file_path.c_str(), (uint)file_path.length());
        WriteData(UpdateFilesList, update_file.Size);
        WriteData(UpdateFilesList, Hashing::MurmurHash2(update_file.Data, update_file.Size));
    }

    // Complete files list
    WriteData(UpdateFilesList, (short)-1);
}

void FOServer::EntitySetValue(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    if (!entity->Id || prop->IsTemporary())
        return;

    DataBase::Value value = PropertiesSerializator::SavePropertyToDbValue(&entity->Props, prop, ScriptSys);

    if (entity->Type == EntityType::Location)
        DbStorage->Update("Locations", entity->Id, prop->GetName(), value);
    else if (entity->Type == EntityType::Map)
        DbStorage->Update("Maps", entity->Id, prop->GetName(), value);
    else if (entity->Type == EntityType::Npc)
        DbStorage->Update("Critters", entity->Id, prop->GetName(), value);
    else if (entity->Type == EntityType::Item)
        DbStorage->Update("Items", entity->Id, prop->GetName(), value);
    else if (entity->Type == EntityType::Custom)
        DbStorage->Update(entity->Props.GetRegistrator()->GetClassName() + "s", entity->Id, prop->GetName(), value);
    else if (entity->Type == EntityType::Client)
        DbStorage->Update("Players", entity->Id, prop->GetName(), value);
    else if (entity->Type == EntityType::Global)
        DbStorage->Update("Globals", entity->Id, prop->GetName(), value);
    else
        throw UnreachablePlaceException(LINE_STR);

    if (DbHistory && !prop->IsNoHistory())
    {
        uint id = Globals->GetHistoryRecordsId();
        Globals->SetHistoryRecordsId(id + 1);

        std::chrono::milliseconds time =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

        DataBase::Document doc;
        doc["Time"] = (int64)time.count();
        doc["EntityId"] = (int)entity->Id;
        doc["Property"] = prop->GetName();
        doc["Value"] = value;

        if (entity->Type == EntityType::Location)
            DbHistory->Insert("LocationsHistory", id, doc);
        else if (entity->Type == EntityType::Map)
            DbHistory->Insert("MapsHistory", id, doc);
        else if (entity->Type == EntityType::Npc)
            DbHistory->Insert("CrittersHistory", id, doc);
        else if (entity->Type == EntityType::Item)
            DbHistory->Insert("ItemsHistory", id, doc);
        else if (entity->Type == EntityType::Custom)
            DbHistory->Insert(entity->Props.GetRegistrator()->GetClassName() + "sHistory", id, doc);
        else if (entity->Type == EntityType::Client)
            DbHistory->Insert("PlayersHistory", id, doc);
        else if (entity->Type == EntityType::Global)
            DbHistory->Insert("GlobalsHistory", id, doc);
        else
            throw UnreachablePlaceException(LINE_STR);
    }
}

void FOServer::ProcessCritter(Critter* cr)
{
    if (cr->CanBeRemoved || cr->IsDestroyed)
        return;
    if (Timer::IsGamePaused())
        return;

    // Moving
    ProcessMove(cr);

    // Idle functions
    ScriptSys.CritterIdleEvent(cr);
    if (!cr->GetMapId())
        ScriptSys.CritterGlobalMapIdleEvent(cr);

    // Internal misc/drugs time events
    // One event per cycle
    /*if (cr->IsNonEmptyTE_FuncNum())
    {
        CScriptArray* te_next_time = cr->GetTE_NextTime();
        uint next_time = *(uint*)te_next_time->At(0);
        if (!next_time || Settings.FullSecond >= next_time)
        {
            CScriptArray* te_func_num = cr->GetTE_FuncNum();
            CScriptArray* te_rate = cr->GetTE_Rate();
            CScriptArray* te_identifier = cr->GetTE_Identifier();
            RUNTIME_ASSERT(te_next_time->GetSize() == te_func_num->GetSize());
            RUNTIME_ASSERT(te_func_num->GetSize() == te_rate->GetSize());
            RUNTIME_ASSERT(te_rate->GetSize() == te_identifier->GetSize());
            hash func_num = *(hash*)te_func_num->At(0);
            uint rate = *(hash*)te_rate->At(0);
            int identifier = *(hash*)te_identifier->At(0);
            te_func_num->Release();
            te_rate->Release();
            te_identifier->Release();

            cr->EraseCrTimeEvent(0);

            uint time = Globals->GetTimeMultiplier() * 1800; // 30 minutes on error
            ScriptSys.PrepareScriptFuncContext(func_num, cr->GetName());
            ScriptSys.SetArgEntity(cr);
            ScriptSys.SetArgUInt(identifier);
            ScriptSys.SetArgAddress(&rate);
            if (ScriptSys.RunPrepared())
                time = ScriptSys.GetReturnedUInt();
            if (time)
                cr->AddCrTimeEvent(func_num, rate, time, identifier);
        }
        te_next_time->Release();
    }*/

    // Client
    if (cr->IsPlayer())
    {
        // Cast
        Client* cl = (Client*)cr;

        // Talk
        CrMngr.ProcessTalk(cl, false);

        // Ping client
        if (cl->IsToPing())
            cl->PingClient();

        // Kick from game
        if (cl->IsOffline() && cl->IsLife() && !(cl->GetTimeoutBattle() > Settings.FullSecond) &&
            !cl->GetTimeoutRemoveFromGame() && cl->GetOfflineTime() >= Settings.MinimumOfflineTime)
        {
            cl->RemoveFromGame();
        }

        // Cache look distance
        // Todo: disable look distance caching
        if (Timer::GameTick() >= cl->CacheValuesNextTick)
        {
            cl->LookCacheValue = cl->GetLookDistance();
            cl->CacheValuesNextTick = Timer::GameTick() + 3000;
        }
    }
}

bool FOServer::Act_Move(Critter* cr, ushort hx, ushort hy, uint move_params)
{
    uint map_id = cr->GetMapId();
    if (!map_id)
        return false;

    // Check run/walk
    bool is_run = FLAG(move_params, MOVE_PARAM_RUN);
    if (is_run && cr->GetIsNoRun())
    {
        // Switch to walk
        move_params ^= MOVE_PARAM_RUN;
        is_run = false;
    }
    if (!is_run && cr->GetIsNoWalk())
        return false;

    // Check
    Map* map = MapMngr.GetMap(map_id);
    if (!map || map_id != cr->GetMapId() || hx >= map->GetWidth() || hy >= map->GetHeight())
        return false;

    // Check passed
    ushort fx = cr->GetHexX();
    ushort fy = cr->GetHexY();
    uchar dir = GeomHelper.GetNearDir(fx, fy, hx, hy);
    uint multihex = cr->GetMultihex();

    if (!map->IsMovePassed(hx, hy, dir, multihex))
    {
        if (cr->IsPlayer())
        {
            cr->Send_XY(cr);
            Critter* cr_hex = map->GetHexCritter(hx, hy, false);
            if (cr_hex)
                cr->Send_XY(cr_hex);
        }
        return false;
    }

    // Set last move type
    cr->IsRunning = is_run;

    // Process step
    bool is_dead = cr->IsDead();
    map->UnsetFlagCritter(fx, fy, multihex, is_dead);
    cr->SetHexX(hx);
    cr->SetHexY(hy);
    map->SetFlagCritter(hx, hy, multihex, is_dead);

    // Set dir
    cr->SetDir(dir);

    if (is_run)
    {
        if (cr->GetIsHide())
            cr->SetIsHide(false);
        cr->SetBreakTimeDelta(cr->GetRunTime());
    }
    else
    {
        cr->SetBreakTimeDelta(cr->GetWalkTime());
    }

    cr->SendA_Move(move_params);
    MapMngr.ProcessVisibleCritters(cr);
    MapMngr.ProcessVisibleItems(cr);

    // Triggers
    if (cr->GetMapId() == map->GetId())
        VerifyTrigger(map, cr, fx, fy, hx, hy, dir);

    return true;
}

void FOServer::VerifyTrigger(
    Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir)
{
    if (map->IsHexStaticTrigger(from_hx, from_hy))
    {
        ItemVec triggers;
        map->GetStaticItemTriggers(from_hx, from_hy, triggers);
        for (Item* item : triggers)
        {
            if (item->TriggerScriptFunc)
                item->TriggerScriptFunc(cr, item, false, dir);

            ScriptSys.StaticItemWalkEvent(item, cr, false, dir);
        }
    }

    if (map->IsHexStaticTrigger(to_hx, to_hy))
    {
        ItemVec triggers;
        map->GetStaticItemTriggers(to_hx, to_hy, triggers);
        for (Item* item : triggers)
        {
            if (item->TriggerScriptFunc)
                item->TriggerScriptFunc(cr, item, true, dir);

            ScriptSys.StaticItemWalkEvent(item, cr, true, dir);
        }
    }

    if (map->IsHexTrigger(from_hx, from_hy))
    {
        ItemVec triggers;
        map->GetItemsTrigger(from_hx, from_hy, triggers);
        for (Item* item : triggers)
            ScriptSys.ItemWalkEvent(item, cr, false, dir);
    }

    if (map->IsHexTrigger(to_hx, to_hy))
    {
        ItemVec triggers;
        map->GetItemsTrigger(to_hx, to_hy, triggers);
        for (Item* item : triggers)
            ScriptSys.ItemWalkEvent(item, cr, true, dir);
    }
}

void FOServer::Process_Update(Client* cl)
{
    // Net protocol
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;
    bool outdated = (proto_ver != (ushort)FO_VERSION);

    // Begin data encrypting
    uint encrypt_key;
    cl->Connection->Bin >> encrypt_key;
    cl->Connection->Bin.SetEncryptKey(encrypt_key + 521);
    cl->Connection->Bout.SetEncryptKey(encrypt_key + 3491);

    // Send update files list
    uint msg_len =
        sizeof(NETMSG_UPDATE_FILES_LIST) + sizeof(msg_len) + sizeof(bool) + sizeof(uint) + (uint)UpdateFilesList.size();

    // With global properties
    PUCharVec* global_vars_data;
    UIntVec* global_vars_data_sizes;
    if (!outdated)
        msg_len += sizeof(ushort) + Globals->Props.StoreData(false, &global_vars_data, &global_vars_data_sizes);

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_UPDATE_FILES_LIST;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << outdated;
    cl->Connection->Bout << (uint)UpdateFilesList.size();
    if (!UpdateFilesList.empty())
        cl->Connection->Bout.Push(&UpdateFilesList[0], (uint)UpdateFilesList.size());
    if (!outdated)
        NET_WRITE_PROPERTIES(cl->Connection->Bout, global_vars_data, global_vars_data_sizes);
    CLIENT_OUTPUT_END(cl);
}

void FOServer::Process_UpdateFile(Client* cl)
{
    uint file_index;
    cl->Connection->Bin >> file_index;

    if (file_index >= (uint)UpdateFiles.size())
    {
        WriteLog("Wrong file index {}, client ip '{}'.\n", file_index, cl->GetIpStr());
        cl->Disconnect();
        return;
    }

    cl->UpdateFileIndex = file_index;
    cl->UpdateFilePortion = 0;
    Process_UpdateFileData(cl);
}

void FOServer::Process_UpdateFileData(Client* cl)
{
    if (cl->UpdateFileIndex == -1)
    {
        WriteLog("Wrong update call, client ip '{}'.\n", cl->GetIpStr());
        cl->Disconnect();
        return;
    }

    UpdateFile& update_file = UpdateFiles[cl->UpdateFileIndex];
    uint offset = cl->UpdateFilePortion * FILE_UPDATE_PORTION;

    if (offset + FILE_UPDATE_PORTION < update_file.Size)
        cl->UpdateFilePortion++;
    else
        cl->UpdateFileIndex = -1;

    if (cl->IsSendDisabled() || cl->IsOffline())
        return;

    uchar data[FILE_UPDATE_PORTION];
    uint remaining_size = update_file.Size - offset;
    if (remaining_size >= sizeof(data))
    {
        memcpy(data, &update_file.Data[offset], sizeof(data));
    }
    else
    {
        memcpy(data, &update_file.Data[offset], remaining_size);
        memzero(&data[remaining_size], sizeof(data) - remaining_size);
    }

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_UPDATE_FILE_DATA;
    cl->Connection->Bout.Push(data, sizeof(data));
    CLIENT_OUTPUT_END(cl);
}

void FOServer::Process_CreateClient(Client* cl)
{
    // Read message
    uint msg_len;
    cl->Connection->Bin >> msg_len;

    // Protocol version
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;

    // Begin data encrypting
    cl->Connection->Bin.SetEncryptKey(1234567890);
    cl->Connection->Bout.SetEncryptKey(1234567890);

    // Check protocol
    if (proto_ver != (ushort)FO_VERSION)
    {
        cl->Send_CustomMessage(NETMSG_WRONG_NET_PROTO);
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK(BannedLocker);

        uint ip = cl->GetIp();
        ClientBanned* ban = GetBanByIp(ip);
        if (ban)
        {
            cl->Send_TextMsg(cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME);
            // cl->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,ban->GetBanLexems());
            cl->Send_TextMsgLex(
                cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
            cl->Disconnect();
            return;
        }
    }

    // Name
    char name[UTF8_BUF_SIZE(MAX_NAME)];
    cl->Connection->Bin.Pop(name, sizeof(name));
    name[sizeof(name) - 1] = 0;
    cl->Name = name;

    // Password hash
    char password[UTF8_BUF_SIZE(MAX_NAME)];
    cl->Connection->Bin.Pop(password, sizeof(password));
    password[sizeof(password) - 1] = 0;

    // Check net
    CHECK_IN_BUFF_ERROR_EXT(cl, cl->Send_TextMsg(cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME), return );

    // Check data
    if (!_str(cl->Name).isValidUtf8() || cl->Name.find('*') != string::npos)
    {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    uint name_len_utf8 = _str(cl->Name).lengthUtf8();
    if (name_len_utf8 < MIN_NAME || name_len_utf8 < Settings.MinNameLength || name_len_utf8 > MAX_NAME ||
        name_len_utf8 > Settings.MaxNameLength)
    {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check for exist
    uint id = MAKE_CLIENT_ID(name);
    RUNTIME_ASSERT(IS_CLIENT_ID(id));
    if (!DbStorage->Get("Players", id).empty())
    {
        cl->Send_TextMsg(cl, STR_NET_ACCOUNT_ALREADY, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check brute force registration
    if (Settings.RegistrationTimeout)
    {
        SCOPE_LOCK(RegIpLocker);

        uint ip = cl->GetIp();
        uint reg_tick = Settings.RegistrationTimeout * 1000;
        auto it = RegIp.find(ip);
        if (it != RegIp.end())
        {
            uint& last_reg = it->second;
            uint tick = Timer::FastTick();
            if (tick - last_reg < reg_tick)
            {
                cl->Send_TextMsg(cl, STR_NET_REGISTRATION_IP_WAIT, SAY_NETMSG, TEXTMSG_GAME);
                cl->Send_TextMsgLex(cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME,
                    _str("$time{}", (reg_tick - (tick - last_reg)) / 60000 + 1).c_str());
                cl->Disconnect();
                return;
            }
            last_reg = tick;
        }
        else
        {
            RegIp.insert(std::make_pair(ip, Timer::FastTick()));
        }
    }

    uint disallow_msg_num = 0, disallow_str_num = 0;
    string lexems;
    bool allow = ScriptSys.PlayerRegistrationEvent(cl->GetIp(), cl->Name, disallow_msg_num, disallow_str_num, lexems);
    if (!allow)
    {
        if (disallow_msg_num < TEXTMSG_COUNT && disallow_str_num)
            cl->Send_TextMsgLex(cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, lexems.c_str());
        else
            cl->Send_TextMsg(cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Register
    cl->SetId(id);
    DbStorage->Insert("Players", id, {{"_Proto", string("Player")}});
    cl->SetPassword(password);
    cl->SetWorldX((Settings.GlobalMapWidth * Settings.GlobalMapZoneLength) / 2);
    cl->SetWorldY((Settings.GlobalMapHeight * Settings.GlobalMapZoneLength) / 2);
    cl->SetCond(COND_LIFE);

    uint reg_ip = cl->GetIp();
    cl->SetConnectionIp({reg_ip});
    ushort reg_port = cl->GetPort();
    cl->SetConnectionPort({reg_port});

    // Assign base access
    cl->Access = ACCESS_DEFAULT;

    // Notify
    cl->Send_TextMsg(cl, STR_NET_REG_SUCCESS, SAY_NETMSG, TEXTMSG_GAME);

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << (uint)NETMSG_REGISTER_SUCCESS;
    CLIENT_OUTPUT_END(cl);

    cl->Disconnect();

    cl->AddRef();
    EntityMngr.RegisterEntity(cl);
    bool can = MapMngr.CanAddCrToMap(cl, nullptr, 0, 0, 0);
    RUNTIME_ASSERT(can);
    MapMngr.AddCrToMap(cl, nullptr, 0, 0, 0, 0);

    if (ScriptSys.CritterInitEvent(cl, true))
    {
        // Todo: remove from game
    }
}

void FOServer::Process_LogIn(Client*& cl)
{
    // Net protocol
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;

    // Begin data encrypting
    cl->Connection->Bin.SetEncryptKey(12345);
    cl->Connection->Bout.SetEncryptKey(12345);

    // Check protocol
    if (proto_ver != (ushort)FO_VERSION)
    {
        cl->Send_CustomMessage(NETMSG_WRONG_NET_PROTO);
        cl->Disconnect();
        return;
    }

    // Login, password hash
    char name[UTF8_BUF_SIZE(MAX_NAME)];
    cl->Connection->Bin.Pop(name, sizeof(name));
    name[sizeof(name) - 1] = 0;
    cl->Name = name;
    char password[UTF8_BUF_SIZE(MAX_NAME)];
    cl->Connection->Bin.Pop(password, sizeof(password));

    // Bin hashes
    uint msg_language;
    cl->Connection->Bin >> msg_language;

    CHECK_IN_BUFF_ERROR_EXT(cl, cl->Send_TextMsg(cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME), return );

    // Language
    auto it_l = std::find(LangPacks.begin(), LangPacks.end(), msg_language);
    if (it_l != LangPacks.end())
        cl->LanguageMsg = msg_language;
    else
        cl->LanguageMsg = (*LangPacks.begin()).Name;

    // If only cache checking than disconnect
    if (!name[0])
    {
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK(BannedLocker);

        uint ip = cl->GetIp();
        ClientBanned* ban = GetBanByIp(ip);
        if (ban)
        {
            cl->Send_TextMsg(cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME);
            if (_str(ban->ClientName).compareIgnoreCaseUtf8(cl->Name))
                cl->Send_TextMsgLex(cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban->GetBanLexems().c_str());
            cl->Send_TextMsgLex(
                cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
            cl->Disconnect();
            return;
        }
    }

    // Check login/password
    uint name_len_utf8 = _str(cl->Name).lengthUtf8();
    if (name_len_utf8 < MIN_NAME || name_len_utf8 < Settings.MinNameLength || name_len_utf8 > MAX_NAME ||
        name_len_utf8 > Settings.MaxNameLength)
    {
        cl->Send_TextMsg(cl, STR_NET_WRONG_LOGIN, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    if (!_str(cl->Name).isValidUtf8() || cl->Name.find('*') != string::npos)
    {
        cl->Send_TextMsg(cl, STR_NET_WRONG_DATA, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check password
    uint id = MAKE_CLIENT_ID(cl->Name);
    DataBase::Document doc = DbStorage->Get("Players", id);
    if (!doc.count("Password") || doc["Password"].index() != DataBase::StringValue ||
        !Str::Compare(password, std::get<string>(doc["Password"]).c_str()))
    {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check for ban by name
    {
        SCOPE_LOCK(BannedLocker);

        ClientBanned* ban = GetBanByName(cl->Name.c_str());
        if (ban)
        {
            cl->Send_TextMsg(cl, STR_NET_BANNED, SAY_NETMSG, TEXTMSG_GAME);
            cl->Send_TextMsgLex(cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban->GetBanLexems().c_str());
            cl->Send_TextMsgLex(
                cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
            cl->Disconnect();
            return;
        }
    }

    // Request script
    uint disallow_msg_num = 0, disallow_str_num = 0;
    string lexems;
    bool allow = !ScriptSys.PlayerLoginEvent(cl->GetIp(), cl->Name, id, disallow_msg_num, disallow_str_num, lexems);
    if (!allow)
    {
        if (disallow_msg_num < TEXTMSG_COUNT && disallow_str_num)
            cl->Send_TextMsgLex(cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, lexems.c_str());
        else
            cl->Send_TextMsg(cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Find in players in game
    Client* cl_old = CrMngr.GetPlayer(id);
    RUNTIME_ASSERT(cl != cl_old);

    // Avatar in game
    if (cl_old)
    {
        {
            SCOPE_LOCK(ConnectedClientsLocker);

            // Disconnect current connection
            if (!cl_old->Connection->IsDisconnected)
                cl_old->Disconnect();

            // Swap data
            std::swap(cl_old->Connection, cl->Connection);
            cl_old->GameState = STATE_CONNECTED;
            cl->GameState = STATE_NONE;
            cl_old->LastActivityTime = 0;
            UNSETFLAG(cl_old->Flags, FCRIT_DISCONNECT);
            SETFLAG(cl->Flags, FCRIT_DISCONNECT);
            cl->IsDestroyed = true;
            cl_old->IsDestroyed = false;
            ScriptSys.RemoveEntity(cl);

            // Change in list
            cl_old->AddRef();
            auto it = std::find(ConnectedClients.begin(), ConnectedClients.end(), cl);
            RUNTIME_ASSERT(it != ConnectedClients.end());
            *it = cl_old;
            cl->Release();
            cl = cl_old;
        }

        cl->SendA_Action(ACTION_CONNECT, 0, nullptr);
    }
    // Avatar not in game
    else
    {
        cl->SetId(id);

        // Data
        if (!PropertiesSerializator::LoadFromDbDocument(&cl->Props, doc, ScriptSys))
        {
            WriteLog("Player '{}' data truncated.\n", cl->Name);
            cl->Send_TextMsg(cl, STR_NET_BD_ERROR, SAY_NETMSG, TEXTMSG_GAME);
            cl->Disconnect();
            return;
        }

        // Add to collection
        cl->AddRef();
        EntityMngr.RegisterEntity(cl);

        // Disable network data sending, because we resend all data later
        cl->DisableSend++;

        // Find items
        ItemMngr.SetCritterItems(cl);

        // Initially add on global map
        cl->SetMapId(0);
        uint ref_gm_leader_id = cl->GetRefGlobalMapLeaderId();
        uint ref_gm_trip_id = cl->GetRefGlobalMapTripId();
        bool can = MapMngr.CanAddCrToMap(cl, nullptr, 0, 0, 0);
        RUNTIME_ASSERT(can);
        MapMngr.AddCrToMap(cl, nullptr, 0, 0, 0, 0);
        cl->SetRefGlobalMapLeaderId(ref_gm_leader_id);
        cl->SetRefGlobalMapLeaderId(ref_gm_trip_id);

        // Initial scripts
        if (ScriptSys.CritterInitEvent(cl, false))
        {
            // Todo: remove from game
        }
        cl->SetScript(nullptr, false);

        // Restore data sending
        cl->DisableSend--;
    }

    // Connection info
    uint ip = cl->GetIp();
    ushort port = cl->GetPort();
    vector<uint> conn_ip = cl->GetConnectionIp();
    vector<ushort> conn_port = cl->GetConnectionPort();
    RUNTIME_ASSERT(conn_ip.size() == conn_port.size());
    bool ip_found = false;
    for (uint i = 0; i < conn_ip.size(); i++)
    {
        if (conn_ip[i] == ip)
        {
            if (i < conn_ip.size() - 1)
            {
                conn_ip.erase(conn_ip.begin() + i);
                conn_ip.push_back(ip);
                cl->SetConnectionIp(conn_ip);
                conn_port.erase(conn_port.begin() + i);
                conn_port.push_back(port);
                cl->SetConnectionPort(conn_port);
            }
            else if (conn_port.back() != port)
            {
                conn_port.back() = port;
                cl->SetConnectionPort(conn_port);
            }
            ip_found = true;
            break;
        }
    }
    if (!ip_found)
    {
        conn_ip.push_back(ip);
        cl->SetConnectionIp(conn_ip);
        conn_port.push_back(port);
        cl->SetConnectionPort(conn_port);
    }

    // Login ok
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) * 2;
    uint bin_seed = GenericUtils::Random(100000, 2000000000);
    uint bout_seed = GenericUtils::Random(100000, 2000000000);
    PUCharVec* global_vars_data;
    UIntVec* global_vars_data_sizes;
    uint whole_data_size = Globals->Props.StoreData(false, &global_vars_data, &global_vars_data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_LOGIN_SUCCESS;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << bin_seed;
    cl->Connection->Bout << bout_seed;
    NET_WRITE_PROPERTIES(cl->Connection->Bout, global_vars_data, global_vars_data_sizes);
    CLIENT_OUTPUT_END(cl);

    cl->Connection->Bin.SetEncryptKey(bin_seed);
    cl->Connection->Bout.SetEncryptKey(bout_seed);

    cl->Send_LoadMap(nullptr, MapMngr);
}

void FOServer::Process_ParseToGame(Client* cl)
{
    if (!cl->GetId() || !CrMngr.GetPlayer(cl->GetId()))
        return;
    cl->SetBreakTime(Settings.Breaktime);

    cl->GameState = STATE_PLAYING;
    cl->PingOk(PING_CLIENT_LIFE_TIME * 5);

    if (cl->ViewMapId)
    {
        Map* map = MapMngr.GetMap(cl->ViewMapId);
        cl->ViewMapId = 0;
        if (map)
        {
            MapMngr.ViewMap(cl, map, cl->ViewMapLook, cl->ViewMapHx, cl->ViewMapHy, cl->ViewMapDir);
            cl->Send_ViewMap();
            return;
        }
    }

    // Parse
    Map* map = MapMngr.GetMap(cl->GetMapId());
    cl->Send_GameInfo(map);

    // Parse to global
    if (!cl->GetMapId())
    {
        RUNTIME_ASSERT(cl->GlobalMapGroup);

        cl->Send_GlobalInfo(GM_INFO_ALL, MapMngr);
        for (auto it = cl->GlobalMapGroup->begin(), end = cl->GlobalMapGroup->end(); it != end; ++it)
        {
            Critter* cr = *it;
            if (cr != cl)
                cr->Send_CustomCommand(cl, OTHER_FLAGS, cl->Flags);
        }
        cl->Send_AllAutomapsInfo(MapMngr);
        if (cl->Talk.TalkType != TALK_NONE)
        {
            CrMngr.ProcessTalk(cl, true);
            cl->Send_Talk();
        }
    }
    else
    {
        if (!map)
        {
            WriteLog("Map not found, client '{}'.\n", cl->GetName());
            cl->Disconnect();
            return;
        }

        // Parse to local
        SETFLAG(cl->Flags, FCRIT_CHOSEN);
        cl->Send_AddCritter(cl);
        UNSETFLAG(cl->Flags, FCRIT_CHOSEN);

        // Send all data
        cl->Send_AddAllItems();
        cl->Send_AllAutomapsInfo(MapMngr);

        // Send current critters
        CritterVec critters = cl->VisCrSelf;
        for (auto it = critters.begin(), end = critters.end(); it != end; ++it)
            cl->Send_AddCritter(*it);

        // Send current items on map
        UIntSet items = cl->VisItem;
        for (auto it = items.begin(), end = items.end(); it != end; ++it)
        {
            Item* item = ItemMngr.GetItem(*it);
            if (item)
                cl->Send_AddItemOnMap(item);
        }

        // Check active talk
        if (cl->Talk.TalkType != TALK_NONE)
        {
            CrMngr.ProcessTalk(cl, true);
            cl->Send_Talk();
        }
    }

    // Notify about end of parsing
    cl->Send_CustomMessage(NETMSG_END_PARSE_TO_GAME);
}

void FOServer::Process_GiveMap(Client* cl)
{
    bool automap;
    hash map_pid;
    uint loc_id;
    hash hash_tiles;
    hash hash_scen;

    cl->Connection->Bin >> automap;
    cl->Connection->Bin >> map_pid;
    cl->Connection->Bin >> loc_id;
    cl->Connection->Bin >> hash_tiles;
    cl->Connection->Bin >> hash_scen;
    CHECK_IN_BUFF_ERROR(cl);

    uint tick = Timer::FastTick();
    if (tick - cl->LastSendedMapTick < Settings.Breaktime * 3)
        cl->SetBreakTime(Settings.Breaktime * 3);
    else
        cl->SetBreakTime(Settings.Breaktime);
    cl->LastSendedMapTick = tick;

    ProtoMap* pmap = ProtoMngr.GetProtoMap(map_pid);
    if (!pmap)
    {
        WriteLog("Map prototype not found, client '{}'.\n", cl->GetName());
        cl->Disconnect();
        return;
    }

    if (automap)
    {
        if (!MapMngr.CheckKnownLocById(cl, loc_id))
        {
            WriteLog("Request for loading unknown automap, client '{}'.\n", cl->GetName());
            return;
        }

        Location* loc = MapMngr.GetLocation(loc_id);
        if (!loc)
        {
            WriteLog("Request for loading incorrect automap, client '{}'.\n", cl->GetName());
            return;
        }

        bool found = false;
        HashVec automaps = (loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : HashVec());
        if (!automaps.empty())
        {
            for (size_t i = 0; i < automaps.size() && !found; i++)
                if (automaps[i] == map_pid)
                    found = true;
        }
        if (!found)
        {
            WriteLog("Request for loading incorrect automap, client '{}'.\n", cl->GetName());
            return;
        }
    }
    else
    {
        Map* map = MapMngr.GetMap(cl->GetMapId());
        if ((!map || map_pid != map->GetProtoId()) && map_pid != cl->ViewMapPid)
        {
            WriteLog("Request for loading incorrect map, client '{}'.\n", cl->GetName());
            return;
        }
    }

    StaticMap* static_map = MapMngr.FindStaticMap(pmap);
    RUNTIME_ASSERT(static_map);

    Send_MapData(cl, pmap, static_map, static_map->HashTiles != hash_tiles, static_map->HashScen != hash_scen);

    if (!automap)
    {
        Map* map = nullptr;
        if (cl->ViewMapId)
            map = MapMngr.GetMap(cl->ViewMapId);
        cl->Send_LoadMap(map, MapMngr);
    }
}

void FOServer::Send_MapData(Client* cl, ProtoMap* pmap, StaticMap* static_map, bool send_tiles, bool send_scenery)
{
    uint msg = NETMSG_MAP;
    hash map_pid = pmap->ProtoId;
    ushort maxhx = pmap->GetWidth();
    ushort maxhy = pmap->GetHeight();
    uint msg_len = sizeof(msg) + sizeof(msg_len) + sizeof(map_pid) + sizeof(maxhx) + sizeof(maxhy) + sizeof(bool) * 2;

    if (send_tiles)
        msg_len += sizeof(uint) + (uint)static_map->Tiles.size() * sizeof(MapTile);
    if (send_scenery)
        msg_len += sizeof(uint) + (uint)static_map->SceneryData.size();

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << msg;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << map_pid;
    cl->Connection->Bout << maxhx;
    cl->Connection->Bout << maxhy;
    cl->Connection->Bout << send_tiles;
    cl->Connection->Bout << send_scenery;
    if (send_tiles)
    {
        cl->Connection->Bout << (uint)(static_map->Tiles.size() * sizeof(MapTile));
        if (static_map->Tiles.size())
            cl->Connection->Bout.Push(&static_map->Tiles[0], (uint)static_map->Tiles.size() * sizeof(MapTile));
    }
    if (send_scenery)
    {
        cl->Connection->Bout << (uint)static_map->SceneryData.size();
        if (static_map->SceneryData.size())
            cl->Connection->Bout.Push(&static_map->SceneryData[0], (uint)static_map->SceneryData.size());
    }
    CLIENT_OUTPUT_END(cl);
}

void FOServer::Process_Move(Client* cl)
{
    uint move_params;
    ushort hx;
    ushort hy;

    cl->Connection->Bin >> move_params;
    cl->Connection->Bin >> hx;
    cl->Connection->Bin >> hy;
    CHECK_IN_BUFF_ERROR(cl);

    if (!cl->GetMapId())
        return;

    // The player informs that has stopped
    if (FLAG(move_params, MOVE_PARAM_STEP_DISALLOW))
    {
        // cl->Send_XY(cl);
        cl->SendA_XY();
        return;
    }

    // Check running availability
    bool is_run = FLAG(move_params, MOVE_PARAM_RUN);
    if (is_run)
    {
        if (cl->GetIsNoRun() || (Settings.RunOnCombat ? false : cl->GetTimeoutBattle() > Settings.FullSecond) ||
            (Settings.RunOnTransfer ? false : cl->GetTimeoutTransfer() > Settings.FullSecond))
        {
            // Switch to walk
            move_params ^= MOVE_PARAM_RUN;
            is_run = false;
        }
    }

    // Check walking availability
    if (!is_run)
    {
        if (cl->GetIsNoWalk())
        {
            cl->Send_XY(cl);
            return;
        }
    }

    // Check dist
    if (!GeomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, 1))
    {
        cl->Send_XY(cl);
        return;
    }

    // Try move
    Act_Move(cl, hx, hy, move_params);
}

void FOServer::Process_Dir(Client* cl)
{
    uchar dir;
    cl->Connection->Bin >> dir;
    CHECK_IN_BUFF_ERROR(cl);

    if (!cl->GetMapId() || dir >= Settings.MapDirCount || cl->GetDir() == dir || cl->IsTalking())
    {
        if (cl->GetDir() != dir)
            cl->Send_Dir(cl);
        return;
    }

    cl->SetDir(dir);
    cl->SendA_Dir();
}

void FOServer::Process_Ping(Client* cl)
{
    uchar ping;

    cl->Connection->Bin >> ping;
    CHECK_IN_BUFF_ERROR(cl);

    if (ping == PING_CLIENT)
    {
        cl->PingOk(PING_CLIENT_LIFE_TIME);
        return;
    }
    else if (ping == PING_PING || ping == PING_WAIT)
    {
        // Valid pings
    }
    else
    {
        WriteLog("Unknown ping {}, client '{}'.\n", ping, cl->GetName());
        return;
    }

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_PING;
    cl->Connection->Bout << ping;
    CLIENT_OUTPUT_END(cl);
}

void FOServer::Process_Property(Client* cl, uint data_size)
{
    uint msg_len = 0;
    NetProperty::Type type = NetProperty::None;
    uint cr_id = 0;
    uint item_id = 0;
    ushort property_index = 0;

    if (data_size == 0)
        cl->Connection->Bin >> msg_len;

    char type_ = 0;
    cl->Connection->Bin >> type_;
    type = (NetProperty::Type)type_;

    uint additional_args = 0;
    switch (type)
    {
    case NetProperty::CritterItem:
        additional_args = 2;
        cl->Connection->Bin >> cr_id;
        cl->Connection->Bin >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        cl->Connection->Bin >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        cl->Connection->Bin >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        cl->Connection->Bin >> item_id;
        break;
    default:
        break;
    }

    cl->Connection->Bin >> property_index;

    CHECK_IN_BUFF_ERROR(cl);

    UCharVec data;
    if (data_size != 0)
    {
        data.resize(data_size);
        cl->Connection->Bin.Pop(&data[0], data_size);
    }
    else
    {
        uint len =
            msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(ushort);
        // Todo: control max size explicitly, add option to property registration
        if (len > 0xFFFF) // For now 64Kb for all
            return;
        data.resize(len);
        cl->Connection->Bin.Pop(&data[0], len);
    }

    CHECK_IN_BUFF_ERROR(cl);

    bool is_public = false;
    Property* prop = nullptr;
    Entity* entity = nullptr;
    switch (type)
    {
    case NetProperty::Global:
        is_public = true;
        prop = GlobalVars::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = Globals;
        break;
    case NetProperty::Critter:
        is_public = true;
        prop = Critter::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = CrMngr.GetCritter(cr_id);
        break;
    case NetProperty::Chosen:
        prop = Critter::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = cl;
        break;
    case NetProperty::MapItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = ItemMngr.GetItem(item_id);
        break;
    case NetProperty::CritterItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop)
        {
            Critter* cr = CrMngr.GetCritter(cr_id);
            if (cr)
                entity = cr->GetItem(item_id, true);
        }
        break;
    case NetProperty::ChosenItem:
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = cl->GetItem(item_id, true);
        break;
    case NetProperty::Map:
        is_public = true;
        prop = Map::PropertiesRegistrator->Get(property_index);
        if (prop)
            entity = MapMngr.GetMap(cl->GetMapId());
        break;
    case NetProperty::Location:
        is_public = true;
        prop = Location::PropertiesRegistrator->Get(property_index);
        if (prop)
        {
            Map* map = MapMngr.GetMap(cl->GetMapId());
            if (map)
                entity = map->GetLocation();
        }
        break;
    default:
        break;
    }
    if (!prop || !entity)
        return;

    Property::AccessType access = prop->GetAccess();
    if (is_public && !(access & Property::PublicMask))
        return;
    if (!is_public && !(access & (Property::ProtectedMask | Property::PublicMask)))
        return;
    if (!(access & Property::ModifiableMask))
        return;
    if (is_public && access != Property::PublicFullModifiable)
        return;
    if (prop->IsPOD() && data_size != prop->GetBaseSize())
        return;
    if (!prop->IsPOD() && data_size != 0)
        return;

    // Todo: disable send changing field by client to this client
    entity->Props.SetValueFromData(prop, data.data(), (uint)data.size());
}

void FOServer::OnSendGlobalValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0)
    {
        ClientVec players;
        CrMngr.GetClients(players);
        for (auto it = players.begin(); it != players.end(); ++it)
        {
            Client* cl = *it;
            cl->Send_Property(NetProperty::Global, prop, Globals);
        }
    }
}

void FOServer::OnSendCritterValue(Entity* entity, Property* prop)
{
    Critter* cr = (Critter*)entity;

    bool is_public = (prop->GetAccess() & Property::PublicMask) != 0;
    bool is_protected = (prop->GetAccess() & Property::ProtectedMask) != 0;
    if (is_public || is_protected)
        cr->Send_Property(NetProperty::Chosen, prop, cr);
    if (is_public)
        cr->SendA_Property(NetProperty::Critter, prop, cr);
}

void FOServer::OnSendMapValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0)
    {
        Map* map = (Map*)entity;
        map->SendProperty(NetProperty::Map, prop, map);
    }
}

void FOServer::OnSendLocationValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0)
    {
        Location* loc = (Location*)entity;
        for (Map* map : loc->GetMaps())
            map->SendProperty(NetProperty::Location, prop, loc);
    }
}

#define MOVING_SUCCESS (1)
#define MOVING_TARGET_NOT_FOUND (2)
#define MOVING_CANT_MOVE (3)
#define MOVING_GAG_CRITTER (4)
#define MOVING_GAG_ITEM (5)
#define MOVING_FIND_PATH_ERROR (6)
#define MOVING_HEX_TOO_FAR (7)
#define MOVING_HEX_BUSY (8)
#define MOVING_HEX_BUSY_RING (9)
#define MOVING_DEADLOCK (10)
#define MOVING_TRACE_FAIL (11)

void FOServer::ProcessMove(Critter* cr)
{
    if (cr->Moving.State)
        return;
    if (cr->IsBusy() || cr->IsWait())
        return;

    // Check for path recalculation
    if (cr->Moving.PathNum && cr->Moving.TargId)
    {
        Critter* targ = cr->GetCritSelf(cr->Moving.TargId);
        if (!targ ||
            ((cr->Moving.HexX || cr->Moving.HexY) &&
                !GeomHelper.CheckDist(targ->GetHexX(), targ->GetHexY(), cr->Moving.HexX, cr->Moving.HexY, 0)))
            cr->Moving.PathNum = 0;
    }

    // Find path if not exist
    if (!cr->Moving.PathNum)
    {
        ushort hx;
        ushort hy;
        uint cut;
        uint trace;
        Critter* trace_cr;
        bool check_cr = false;

        if (cr->Moving.TargId)
        {
            Critter* targ = cr->GetCritSelf(cr->Moving.TargId);
            if (!targ)
            {
                cr->Moving.State = MOVING_TARGET_NOT_FOUND;
                cr->SendA_XY();
                return;
            }

            hx = targ->GetHexX();
            hy = targ->GetHexY();
            cut = cr->Moving.Cut;
            trace = cr->Moving.Trace;
            trace_cr = targ;
            check_cr = true;
        }
        else
        {
            hx = cr->Moving.HexX;
            hy = cr->Moving.HexY;
            cut = cr->Moving.Cut;
            trace = 0;
            trace_cr = nullptr;
        }

        PathFindData pfd;
        pfd.MapId = cr->GetMapId();
        pfd.FromCritter = cr;
        pfd.FromX = cr->GetHexX();
        pfd.FromY = cr->GetHexY();
        pfd.ToX = hx;
        pfd.ToY = hy;
        pfd.IsRun = cr->Moving.IsRun;
        pfd.Multihex = cr->GetMultihex();
        pfd.Cut = cut;
        pfd.Trace = trace;
        pfd.TraceCr = trace_cr;
        pfd.CheckCrit = true;
        pfd.CheckGagItems = true;

        if (pfd.IsRun && cr->GetIsNoRun())
            pfd.IsRun = false;
        if (!pfd.IsRun && cr->GetIsNoWalk())
        {
            cr->Moving.State = MOVING_CANT_MOVE;
            return;
        }

        int result = MapMngr.FindPath(pfd);
        if (pfd.GagCritter)
        {
            cr->Moving.State = MOVING_GAG_CRITTER;
            cr->Moving.GagEntityId = pfd.GagCritter->GetId();
            return;
        }

        if (pfd.GagItem)
        {
            cr->Moving.State = MOVING_GAG_ITEM;
            cr->Moving.GagEntityId = pfd.GagItem->GetId();
            return;
        }

        // Failed
        if (result != FPATH_OK)
        {
            if (result == FPATH_ALREADY_HERE)
            {
                cr->Moving.State = MOVING_SUCCESS;
                return;
            }

            int reason = 0;
            switch (result)
            {
            case FPATH_MAP_NOT_FOUND:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FPATH_TOOFAR:
                reason = MOVING_HEX_TOO_FAR;
                break;
            case FPATH_ERROR:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FPATH_INVALID_HEXES:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FPATH_TRACE_TARG_NULL_PTR:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FPATH_HEX_BUSY:
                reason = MOVING_HEX_BUSY;
                break;
            case FPATH_HEX_BUSY_RING:
                reason = MOVING_HEX_BUSY_RING;
                break;
            case FPATH_DEADLOCK:
                reason = MOVING_DEADLOCK;
                break;
            case FPATH_TRACE_FAIL:
                reason = MOVING_TRACE_FAIL;
                break;
            default:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            }

            cr->Moving.State = reason;
            cr->SendA_XY();
            return;
        }

        // Success
        cr->Moving.PathNum = pfd.PathNum;
        cr->Moving.Iter = 0;
        cr->SetWait(0);
    }

    // Get path and move
    PathStepVec& path = MapMngr.GetPath(cr->Moving.PathNum);
    if (cr->Moving.PathNum && cr->Moving.Iter < path.size())
    {
        PathStep& ps = path[cr->Moving.Iter];
        if (!GeomHelper.CheckDist(cr->GetHexX(), cr->GetHexY(), ps.HexX, ps.HexY, 1) ||
            !Act_Move(cr, ps.HexX, ps.HexY, ps.MoveParams))
        {
            // Error
            cr->Moving.PathNum = 0;
            cr->SendA_XY();
        }
        else
        {
            // Next
            cr->Moving.Iter++;
        }
    }
    else
    {
        // Done
        cr->Moving.State = MOVING_SUCCESS;
    }
}

bool FOServer::Dialog_Compile(Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg)
{
    if (base_dlg.Id < 2)
    {
        WriteLog("Wrong dialog id {}.\n", base_dlg.Id);
        return false;
    }
    compiled_dlg = base_dlg;

    for (auto it_a = compiled_dlg.Answers.begin(); it_a != compiled_dlg.Answers.end();)
    {
        if (!Dialog_CheckDemand(npc, cl, *it_a, false))
            it_a = compiled_dlg.Answers.erase(it_a);
        else
            it_a++;
    }

    if (!Settings.NoAnswerShuffle && !compiled_dlg.IsNoShuffle())
    {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 rnd(rd());
        std::shuffle(compiled_dlg.Answers.begin(), compiled_dlg.Answers.end(), rnd);
    }
    return true;
}

bool FOServer::Dialog_CheckDemand(Npc* npc, Client* cl, DialogAnswer& answer, bool recheck)
{
    if (!answer.Demands.size())
        return true;

    Critter* master;
    Critter* slave;

    for (auto it = answer.Demands.begin(), end = answer.Demands.end(); it != end; ++it)
    {
        DemandResult& demand = *it;
        if (recheck && demand.NoRecheck)
            continue;

        switch (demand.Who)
        {
        case DR_WHO_PLAYER:
            master = cl;
            slave = npc;
            break;
        case DR_WHO_NPC:
            master = npc;
            slave = cl;
            break;
        default:
            continue;
        }

        if (!master)
            continue;

        max_t index = demand.ParamId;
        switch (demand.Type)
        {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_CRITTER_DICT:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP: {
            Entity* entity = nullptr;
            PropertyRegistrator* prop_registrator = nullptr;
            if (demand.Type == DR_PROP_GLOBAL)
            {
                entity = master;
                prop_registrator = GlobalVars::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_CRITTER)
            {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_CRITTER_DICT)
            {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_ITEM)
            {
                entity = master->GetItemSlot(1);
                prop_registrator = Item::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_LOCATION)
            {
                Map* map = MapMngr.GetMap(master->GetMapId());
                entity = (map ? map->GetLocation() : nullptr);
                prop_registrator = Location::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_MAP)
            {
                entity = MapMngr.GetMap(master->GetMapId());
                prop_registrator = Map::PropertiesRegistrator;
            }
            if (!entity)
                break;

            uint prop_index = (uint)index;
            Property* prop = prop_registrator->Get(prop_index);
            int val = 0;
            if (demand.Type == DR_PROP_CRITTER_DICT)
            {
                if (!slave)
                    break;

                /*CScriptDict* dict = (CScriptDict*)prop->GetValue<void*>(entity);
                uint slave_id = slave->GetId();
                void* pvalue = dict->GetDefault(&slave_id, nullptr);
                dict->Release();
                if (pvalue)
                {
                    int value_type_id = prop->GetASObjectType()->GetSubTypeId(1);
                    if (value_type_id == asTYPEID_BOOL)
                        val = (int)*(bool*)pvalue ? 1 : 0;
                    else if (value_type_id == asTYPEID_INT8)
                        val = (int)*(char*)pvalue;
                    else if (value_type_id == asTYPEID_INT16)
                        val = (int)*(short*)pvalue;
                    else if (value_type_id == asTYPEID_INT32)
                        val = (int)*(char*)pvalue;
                    else if (value_type_id == asTYPEID_INT64)
                        val = (int)*(int64*)pvalue;
                    else if (value_type_id == asTYPEID_UINT8)
                        val = (int)*(uchar*)pvalue;
                    else if (value_type_id == asTYPEID_UINT16)
                        val = (int)*(ushort*)pvalue;
                    else if (value_type_id == asTYPEID_UINT32)
                        val = (int)*(uint*)pvalue;
                    else if (value_type_id == asTYPEID_UINT64)
                        val = (int)*(uint64*)pvalue;
                    else if (value_type_id == asTYPEID_FLOAT)
                        val = (int)*(float*)pvalue;
                    else if (value_type_id == asTYPEID_DOUBLE)
                        val = (int)*(double*)pvalue;
                    else
                        RUNTIME_ASSERT(false);
                }*/
            }
            else
            {
                val = entity->Props.GetPODValueAsInt(prop);
            }

            switch (demand.Op)
            {
            case '>':
                if (val > demand.Value)
                    continue;
                break;
            case '<':
                if (val < demand.Value)
                    continue;
                break;
            case '=':
                if (val == demand.Value)
                    continue;
                break;
            case '!':
                if (val != demand.Value)
                    continue;
                break;
            case '}':
                if (val >= demand.Value)
                    continue;
                break;
            case '{':
                if (val <= demand.Value)
                    continue;
                break;
            default:
                break;
            }
        }
        break;
        case DR_ITEM: {
            hash pid = (hash)index;
            switch (demand.Op)
            {
            case '>':
                if ((int)master->CountItemPid(pid) > demand.Value)
                    continue;
                break;
            case '<':
                if ((int)master->CountItemPid(pid) < demand.Value)
                    continue;
                break;
            case '=':
                if ((int)master->CountItemPid(pid) == demand.Value)
                    continue;
                break;
            case '!':
                if ((int)master->CountItemPid(pid) != demand.Value)
                    continue;
                break;
            case '}':
                if ((int)master->CountItemPid(pid) >= demand.Value)
                    continue;
                break;
            case '{':
                if ((int)master->CountItemPid(pid) <= demand.Value)
                    continue;
                break;
            default:
                break;
            }
        }
        break;
        case DR_SCRIPT: {
            cl->Talk.Locked = true;
            if (DialogScriptDemand(demand, master, slave))
            {
                cl->Talk.Locked = false;
                continue;
            }
            cl->Talk.Locked = false;
        }
        break;
        case DR_OR:
            return true;
        default:
            continue;
        }

        bool or_mod = false;
        for (; it != end; ++it)
        {
            if (it->Type == DR_OR)
            {
                or_mod = true;
                break;
            }
        }
        if (!or_mod)
            return false;
    }

    return true;
}

uint FOServer::Dialog_UseResult(Npc* npc, Client* cl, DialogAnswer& answer)
{
    if (!answer.Results.size())
        return 0;

    uint force_dialog = 0;
    Critter* master;
    Critter* slave;

    for (auto it = answer.Results.begin(), end = answer.Results.end(); it != end; ++it)
    {
        DemandResult& result = *it;

        switch (result.Who)
        {
        case DR_WHO_PLAYER:
            master = cl;
            slave = npc;
            break;
        case DR_WHO_NPC:
            master = npc;
            slave = cl;
            break;
        default:
            continue;
        }

        if (!master)
            continue;

        max_t index = result.ParamId;
        switch (result.Type)
        {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_CRITTER_DICT:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP: {
            Entity* entity = nullptr;
            PropertyRegistrator* prop_registrator = nullptr;
            if (result.Type == DR_PROP_GLOBAL)
            {
                entity = master;
                prop_registrator = GlobalVars::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_CRITTER)
            {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_CRITTER_DICT)
            {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_ITEM)
            {
                entity = master->GetItemSlot(1);
                prop_registrator = Item::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_LOCATION)
            {
                Map* map = MapMngr.GetMap(master->GetMapId());
                entity = (map ? map->GetLocation() : nullptr);
                prop_registrator = Location::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_MAP)
            {
                entity = MapMngr.GetMap(master->GetMapId());
                prop_registrator = Map::PropertiesRegistrator;
            }
            if (!entity)
                break;

            /*uint prop_index = (uint)index;
            Property* prop = prop_registrator->Get(prop_index);
            int val = 0;
            CScriptDict* dict = nullptr;
            if (result.Type == DR_PROP_CRITTER_DICT)
            {
                if (!slave)
                    continue;

                dict = (CScriptDict*)prop->GetValue<void*>(master);
                uint slave_id = slave->GetId();
                void* pvalue = dict->GetDefault(&slave_id, nullptr);
                if (pvalue)
                {
                    int value_type_id = prop->GetASObjectType()->GetSubTypeId(1);
                    if (value_type_id == asTYPEID_BOOL)
                        val = (int)*(bool*)pvalue ? 1 : 0;
                    else if (value_type_id == asTYPEID_INT8)
                        val = (int)*(char*)pvalue;
                    else if (value_type_id == asTYPEID_INT16)
                        val = (int)*(short*)pvalue;
                    else if (value_type_id == asTYPEID_INT32)
                        val = (int)*(char*)pvalue;
                    else if (value_type_id == asTYPEID_INT64)
                        val = (int)*(int64*)pvalue;
                    else if (value_type_id == asTYPEID_UINT8)
                        val = (int)*(uchar*)pvalue;
                    else if (value_type_id == asTYPEID_UINT16)
                        val = (int)*(ushort*)pvalue;
                    else if (value_type_id == asTYPEID_UINT32)
                        val = (int)*(uint*)pvalue;
                    else if (value_type_id == asTYPEID_UINT64)
                        val = (int)*(uint64*)pvalue;
                    else if (value_type_id == asTYPEID_FLOAT)
                        val = (int)*(float*)pvalue;
                    else if (value_type_id == asTYPEID_DOUBLE)
                        val = (int)*(double*)pvalue;
                    else
                        RUNTIME_ASSERT(false);
                }
            }
            else
            {
                val = prop->GetPODValueAsInt(entity);
            }

            switch (result.Op)
            {
            case '+':
                val += result.Value;
                break;
            case '-':
                val -= result.Value;
                break;
            case '*':
                val *= result.Value;
                break;
            case '/':
                val /= result.Value;
                break;
            case '=':
                val = result.Value;
                break;
            default:
                break;
            }

            if (result.Type == DR_PROP_CRITTER_DICT)
            {
                uint64 buf = 0;
                void* pvalue = &buf;
                int value_type_id = prop->GetASObjectType()->GetSubTypeId(1);
                if (value_type_id == asTYPEID_BOOL)
                    *(bool*)pvalue = val != 0;
                else if (value_type_id == asTYPEID_INT8)
                    *(char*)pvalue = (char)val;
                else if (value_type_id == asTYPEID_INT16)
                    *(short*)pvalue = (short)val;
                else if (value_type_id == asTYPEID_INT32)
                    *(int*)pvalue = (int)val;
                else if (value_type_id == asTYPEID_INT64)
                    *(int64*)pvalue = (int64)val;
                else if (value_type_id == asTYPEID_UINT8)
                    *(uchar*)pvalue = (uchar)val;
                else if (value_type_id == asTYPEID_UINT16)
                    *(ushort*)pvalue = (ushort)val;
                else if (value_type_id == asTYPEID_UINT32)
                    *(uint*)pvalue = (uint)val;
                else if (value_type_id == asTYPEID_UINT64)
                    *(uint64*)pvalue = (uint64)val;
                else if (value_type_id == asTYPEID_FLOAT)
                    *(float*)pvalue = (float)val;
                else if (value_type_id == asTYPEID_DOUBLE)
                    *(double*)pvalue = (double)val;
                else
                    RUNTIME_ASSERT(false);

                uint slave_id = slave->GetId();
                dict->Set(&slave_id, pvalue);
                prop->SetValue<void*>(entity, dict);
                dict->Release();
            }
            else
            {
                prop->SetPODValueAsInt(entity, val);
            }*/
        }
            continue;
        case DR_ITEM: {
            hash pid = (hash)index;
            int cur_count = master->CountItemPid(pid);
            int need_count = cur_count;

            switch (result.Op)
            {
            case '+':
                need_count += result.Value;
                break;
            case '-':
                need_count -= result.Value;
                break;
            case '*':
                need_count *= result.Value;
                break;
            case '/':
                need_count /= result.Value;
                break;
            case '=':
                need_count = result.Value;
                break;
            default:
                continue;
            }

            if (need_count < 0)
                need_count = 0;
            if (cur_count == need_count)
                continue;
            ItemMngr.SetItemCritter(master, pid, need_count);
        }
            continue;
        case DR_SCRIPT: {
            cl->Talk.Locked = true;
            force_dialog = DialogScriptResult(result, master, slave);
            cl->Talk.Locked = false;
        }
            continue;
        default:
            continue;
        }
    }

    return force_dialog;
}

void FOServer::Dialog_Begin(Client* cl, Npc* npc, hash dlg_pack_id, ushort hx, ushort hy, bool ignore_distance)
{
    if (cl->Talk.Locked)
    {
        WriteLog("Dialog locked, client '{}'.\n", cl->GetName());
        return;
    }
    if (cl->Talk.TalkType != TALK_NONE)
        CrMngr.CloseTalk(cl);

    DialogPack* dialog_pack = nullptr;
    DialogsVec* dialogs = nullptr;

    // Talk with npc
    if (npc)
    {
        if (npc->GetIsNoTalk())
            return;

        if (!dlg_pack_id)
        {
            hash npc_dlg_id = npc->GetDialogId();
            if (!npc_dlg_id)
                return;
            dlg_pack_id = npc_dlg_id;
        }

        if (!ignore_distance)
        {
            if (cl->GetMapId() != npc->GetMapId())
            {
                WriteLog("Different maps, npc '{}' {}, client '{}' {}.\n", npc->GetName(), npc->GetMapId(),
                    cl->GetName(), cl->GetMapId());
                return;
            }

            uint talk_distance = npc->GetTalkDistance();
            talk_distance = (talk_distance ? talk_distance : Settings.TalkDistance) + cl->GetMultihex();
            if (!GeomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY(), talk_distance))
            {
                cl->Send_XY(cl);
                cl->Send_XY(npc);
                cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
                WriteLog("Wrong distance to npc '{}', client '{}'.\n", npc->GetName(), cl->GetName());
                return;
            }

            Map* map = MapMngr.GetMap(cl->GetMapId());
            if (!map)
            {
                cl->Send_TextMsg(cl, STR_FINDPATH_AIMBLOCK, SAY_NETMSG, TEXTMSG_GAME);
                return;
            }

            TraceData trace;
            trace.TraceMap = map;
            trace.BeginHx = cl->GetHexX();
            trace.BeginHy = cl->GetHexY();
            trace.EndHx = npc->GetHexX();
            trace.EndHy = npc->GetHexY();
            trace.Dist = talk_distance;
            trace.FindCr = npc;
            MapMngr.TraceBullet(trace);
            if (!trace.IsCritterFounded)
            {
                cl->Send_TextMsg(cl, STR_FINDPATH_AIMBLOCK, SAY_NETMSG, TEXTMSG_GAME);
                return;
            }
        }

        if (!npc->IsLife())
        {
            cl->Send_TextMsg(cl, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME);
            WriteLog("Npc '{}' bad condition, client '{}'.\n", npc->GetName(), cl->GetName());
            return;
        }

        if (!npc->IsFreeToTalk())
        {
            cl->Send_TextMsg(cl, STR_DIALOG_MANY_TALKERS, SAY_NETMSG, TEXTMSG_GAME);
            return;
        }

        // Todo: don't remeber but need check (IsPlaneNoTalk)

        ScriptSys.CritterTalkEvent(cl, npc, true, npc->GetTalkedPlayers() + 1);

        dialog_pack = DlgMngr.GetDialog(dlg_pack_id);
        dialogs = (dialog_pack ? &dialog_pack->Dialogs : nullptr);
        if (!dialogs || !dialogs->size())
            return;

        if (!ignore_distance)
        {
            int dir = GeomHelper.GetFarDir(cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY());
            npc->SetDir(GeomHelper.ReverseDir(dir));
            npc->SendA_Dir();
            cl->SetDir(dir);
            cl->SendA_Dir();
            cl->Send_Dir(cl);
        }
    }
    // Talk with hex
    else
    {
        if (!ignore_distance &&
            !GeomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, Settings.TalkDistance + cl->GetMultihex()))
        {
            cl->Send_XY(cl);
            cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
            WriteLog("Wrong distance to hexes, hx {}, hy {}, client '{}'.\n", hx, hy, cl->GetName());
            return;
        }

        dialog_pack = DlgMngr.GetDialog(dlg_pack_id);
        dialogs = (dialog_pack ? &dialog_pack->Dialogs : nullptr);
        if (!dialogs || !dialogs->size())
        {
            WriteLog("No dialogs, hx {}, hy {}, client '{}'.\n", hx, hy, cl->GetName());
            return;
        }
    }

    // Predialogue installations
    auto it_d = dialogs->begin();
    uint go_dialog = uint(-1);
    auto it_a = (*it_d).Answers.begin();
    for (; it_a != (*it_d).Answers.end(); ++it_a)
    {
        if (Dialog_CheckDemand(npc, cl, *it_a, false))
            go_dialog = (*it_a).Link;
        if (go_dialog != uint(-1))
            break;
    }

    if (go_dialog == uint(-1))
        return;

    // Use result
    uint force_dialog = Dialog_UseResult(npc, cl, (*it_a));
    if (force_dialog)
    {
        if (force_dialog == uint(-1))
            return;
        go_dialog = force_dialog;
    }

    // Find dialog
    it_d = std::find(dialogs->begin(), dialogs->end(), go_dialog);
    if (it_d == dialogs->end())
    {
        cl->Send_TextMsg(cl, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog from link {} not found, client '{}', dialog pack {}.\n", go_dialog, cl->GetName(),
            dialog_pack->PackId);
        return;
    }

    // Compile
    if (!Dialog_Compile(npc, cl, *it_d, cl->Talk.CurDialog))
    {
        cl->Send_TextMsg(cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog compile fail, client '{}', dialog pack {}.\n", cl->GetName(), dialog_pack->PackId);
        return;
    }

    if (npc)
    {
        cl->Talk.TalkType = TALK_WITH_NPC;
        cl->Talk.TalkNpc = npc->GetId();
    }
    else
    {
        cl->Talk.TalkType = TALK_WITH_HEX;
        cl->Talk.TalkHexMap = cl->GetMapId();
        cl->Talk.TalkHexX = hx;
        cl->Talk.TalkHexY = hy;
    }

    cl->Talk.DialogPackId = dlg_pack_id;
    cl->Talk.LastDialogId = go_dialog;
    cl->Talk.StartTick = Timer::GameTick();
    cl->Talk.TalkTime = Settings.DlgTalkMinTime;
    cl->Talk.Barter = false;
    cl->Talk.IgnoreDistance = ignore_distance;

    // Get lexems
    cl->Talk.Lexems.clear();
    if (cl->Talk.CurDialog.DlgScriptFunc)
    {
        cl->Talk.Locked = true;
        if (cl->Talk.CurDialog.DlgScriptFunc(cl, npc))
            cl->Talk.Lexems = cl->Talk.CurDialog.DlgScriptFunc.GetResult();
        cl->Talk.Locked = false;
    }

    // On head text
    if (!cl->Talk.CurDialog.Answers.size())
    {
        if (npc)
        {
            npc->SendAA_MsgLex(
                npc->VisCr, cl->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cl->Talk.Lexems.c_str());
        }
        else
        {
            Map* map = MapMngr.GetMap(cl->GetMapId());
            if (map)
                map->SetTextMsg(hx, hy, 0, TEXTMSG_DLG, cl->Talk.CurDialog.TextId);
        }

        CrMngr.CloseTalk(cl);
        return;
    }

    // Open dialog window
    cl->Send_Talk();
}

void FOServer::Process_Dialog(Client* cl)
{
    uchar is_npc;
    uint talk_id;
    uchar num_answer;

    cl->Connection->Bin >> is_npc;
    cl->Connection->Bin >> talk_id;
    cl->Connection->Bin >> num_answer;

    if ((is_npc && (cl->Talk.TalkType != TALK_WITH_NPC || cl->Talk.TalkNpc != talk_id)) ||
        (!is_npc && (cl->Talk.TalkType != TALK_WITH_HEX || cl->Talk.DialogPackId != talk_id)))
    {
        CrMngr.CloseTalk(cl);
        WriteLog("Invalid talk id {} {}, client '{}'.\n", is_npc, talk_id, cl->GetName());
        return;
    }

    if (cl->GetIsHide())
        cl->SetIsHide(false);
    CrMngr.ProcessTalk(cl, true);

    Npc* npc = nullptr;
    DialogPack* dialog_pack = nullptr;
    DialogsVec* dialogs = nullptr;

    // Find npc
    if (is_npc)
    {
        npc = CrMngr.GetNpc(talk_id);
        if (!npc)
        {
            cl->Send_TextMsg(cl, STR_DIALOG_NPC_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
            CrMngr.CloseTalk(cl);
            WriteLog("Npc with id {} not found, client '{}'.\n", talk_id, cl->GetName());
            return;
        }
    }

    // Set dialogs
    dialog_pack = DlgMngr.GetDialog(cl->Talk.DialogPackId);
    dialogs = (dialog_pack ? &dialog_pack->Dialogs : nullptr);
    if (!dialogs || !dialogs->size())
    {
        CrMngr.CloseTalk(cl);
        WriteLog("No dialogs, npc '{}', client '{}'.\n", npc->GetName(), cl->GetName());
        return;
    }

    // Continue dialog
    Dialog* cur_dialog = &cl->Talk.CurDialog;
    uint last_dialog = cur_dialog->Id;
    uint dlg_id;
    uint force_dialog;
    DialogAnswer* answer;

    if (!cl->Talk.Barter)
    {
        // Barter
        if (num_answer == ANSWER_BARTER)
            goto label_Barter;

        // Refresh
        if (num_answer == ANSWER_BEGIN)
        {
            cl->Send_Talk();
            return;
        }

        // End
        if (num_answer == ANSWER_END)
        {
            CrMngr.CloseTalk(cl);
            return;
        }

        // Invalid answer
        if (num_answer >= cur_dialog->Answers.size())
        {
            WriteLog("Wrong number of answer {}, client '{}'.\n", num_answer, cl->GetName());
            cl->Send_Talk(); // Refresh
            return;
        }

        // Find answer
        answer = &(*(cur_dialog->Answers.begin() + num_answer));

        // Check demand again
        if (!Dialog_CheckDemand(npc, cl, *answer, true))
        {
            WriteLog("Secondary check of dialog demands fail, client '{}'.\n", cl->GetName());
            CrMngr.CloseTalk(cl); // End
            return;
        }

        // Use result
        force_dialog = Dialog_UseResult(npc, cl, *answer);
        if (force_dialog)
            dlg_id = force_dialog;
        else
            dlg_id = answer->Link;

        // Special links
        switch (dlg_id)
        {
        case uint(-3):
        case DIALOG_BARTER:
        label_Barter:
            if (cur_dialog->DlgScriptFunc)
            {
                cl->Send_TextMsg(npc, STR_BARTER_NO_BARTER_NOW, SAY_DIALOG, TEXTMSG_GAME);
                return;
            }

            if (!ScriptSys.CritterBarterEvent(cl, npc, true, npc->GetBarterPlayers() + 1))
            {
                cl->Talk.Barter = true;
                cl->Talk.StartTick = Timer::GameTick();
                cl->Talk.TalkTime = Settings.DlgBarterMinTime;
            }
            return;
        case uint(-2):
        case DIALOG_BACK:
            if (cl->Talk.LastDialogId)
            {
                dlg_id = cl->Talk.LastDialogId;
                break;
            }
        case uint(-1):
        case DIALOG_END:
            CrMngr.CloseTalk(cl);
            return;
        default:
            break;
        }
    }
    else
    {
        cl->Talk.Barter = false;
        dlg_id = cur_dialog->Id;
        if (npc)
            ScriptSys.CritterBarterEvent(cl, npc, false, npc->GetBarterPlayers() + 1);
    }

    // Find dialog
    auto it_d = std::find(dialogs->begin(), dialogs->end(), dlg_id);
    if (it_d == dialogs->end())
    {
        CrMngr.CloseTalk(cl);
        cl->Send_TextMsg(cl, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog from link {} not found, client '{}', dialog pack {}.\n", dlg_id, cl->GetName(),
            dialog_pack->PackId);
        return;
    }

    // Compile
    if (!Dialog_Compile(npc, cl, *it_d, cl->Talk.CurDialog))
    {
        CrMngr.CloseTalk(cl);
        cl->Send_TextMsg(cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog compile fail, client '{}', dialog pack {}.\n", cl->GetName(), dialog_pack->PackId);
        return;
    }

    if (dlg_id != last_dialog)
        cl->Talk.LastDialogId = last_dialog;

    // Get lexems
    cl->Talk.Lexems.clear();
    if (cl->Talk.CurDialog.DlgScriptFunc)
    {
        cl->Talk.Locked = true;
        if (cl->Talk.CurDialog.DlgScriptFunc(cl, npc))
            cl->Talk.Lexems = cl->Talk.CurDialog.DlgScriptFunc.GetResult();
        cl->Talk.Locked = false;
    }

    // On head text
    if (!cl->Talk.CurDialog.Answers.size())
    {
        if (npc)
        {
            npc->SendAA_MsgLex(
                npc->VisCr, cl->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cl->Talk.Lexems.c_str());
        }
        else
        {
            Map* map = MapMngr.GetMap(cl->GetMapId());
            if (map)
                map->SetTextMsg(cl->Talk.TalkHexX, cl->Talk.TalkHexY, 0, TEXTMSG_DLG, cl->Talk.CurDialog.TextId);
        }

        CrMngr.CloseTalk(cl);
        return;
    }

    cl->Talk.StartTick = Timer::GameTick();
    cl->Talk.TalkTime = Settings.DlgTalkMinTime;
    cl->Send_Talk();
}

Item* FOServer::CreateItemOnHex(
    Map* map, ushort hx, ushort hy, hash pid, uint count, Properties* props, bool check_blocks)
{
    // Checks
    ProtoItem* proto_item = ProtoMngr.GetProtoItem(pid);
    if (!proto_item || !count)
        return nullptr;

    // Check blockers
    if (check_blocks && !map->IsPlaceForProtoItem(hx, hy, proto_item))
        return nullptr;

    // Create instance
    Item* item = ItemMngr.CreateItem(pid, count, props);
    if (!item)
        return nullptr;

    // Add on map
    if (!map->AddItem(item, hx, hy))
    {
        ItemMngr.DeleteItem(item);
        return nullptr;
    }

    // Recursive non-stacked items
    if (!proto_item->GetStackable() && count > 1)
        return CreateItemOnHex(map, hx, hy, pid, count - 1, props, true);

    return item;
}

void FOServer::OnSendItemValue(Entity* entity, Property* prop)
{
    Item* item = (Item*)entity;
    if (item->Id && item->Id != uint(-1))
    {
        bool is_public = (prop->GetAccess() & Property::PublicMask) != 0;
        bool is_protected = (prop->GetAccess() & Property::ProtectedMask) != 0;
        if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER)
        {
            if (is_public || is_protected)
            {
                Critter* cr = CrMngr.GetCritter(item->GetCritId());
                if (cr)
                {
                    if (is_public || is_protected)
                        cr->Send_Property(NetProperty::ChosenItem, prop, item);
                    if (is_public)
                        cr->SendA_Property(NetProperty::CritterItem, prop, item);
                }
            }
        }
        else if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
        {
            if (is_public)
            {
                Map* map = MapMngr.GetMap(item->GetMapId());
                if (map)
                    map->SendProperty(NetProperty::MapItem, prop, item);
            }
        }
        else if (item->GetAccessory() == ITEM_ACCESSORY_CONTAINER)
        {
            // Todo: add container properties changing notifications
            // Item* cont = ItemMngr.GetItem( item->GetContainerId() );
        }
    }
}

void FOServer::OnSetItemCount(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    Item* item = (Item*)entity;
    uint cur = *(uint*)cur_value;
    uint old = *(uint*)old_value;
    if ((int)cur > 0 && (item->GetStackable() || cur == 1))
    {
        int diff = (int)item->GetCount() - (int)old;
        ItemMngr.ChangeItemStatistics(item->GetProtoId(), diff);
    }
    else
    {
        item->SetCount(old);
        if (!item->GetStackable())
            throw GenericException("Trying to change count of not stackable item");
        else
            throw GenericException("Item count can't be zero or negative", cur);
    }
}

void FOServer::OnSetItemChangeView(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    // IsHidden, IsAlwaysView, IsTrap, TrapValue
    Item* item = (Item*)entity;

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = MapMngr.GetMap(item->GetMapId());
        if (map)
        {
            map->ChangeViewItem(item);
            if (prop == Item::PropertyIsTrap)
                map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
        }
    }
    else if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER)
    {
        Critter* cr = CrMngr.GetCritter(item->GetCritId());
        if (cr)
        {
            bool value = *(bool*)cur_value;
            if (value)
                cr->Send_EraseItem(item);
            else
                cr->Send_AddItem(item);
            cr->SendAA_MoveItem(item, ACTION_REFRESH, 0);
        }
    }
}

void FOServer::OnSetItemRecacheHex(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    // IsNoBlock, IsShootThru, IsGag, IsTrigger
    Item* item = (Item*)entity;
    bool value = *(bool*)cur_value;

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = MapMngr.GetMap(item->GetMapId());
        if (map)
            map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
    }
}

void FOServer::OnSetItemBlockLines(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    // BlockLines
    Item* item = (Item*)entity;
    if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = MapMngr.GetMap(item->GetMapId());
        if (map)
        {
            // Todo: make BlockLines changable in runtime
        }
    }
}

void FOServer::OnSetItemIsGeck(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    Item* item = (Item*)entity;
    bool value = *(bool*)cur_value;

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
    {
        Map* map = MapMngr.GetMap(item->GetMapId());
        if (map)
            map->GetLocation()->GeckCount += (value ? 1 : -1);
    }
}

void FOServer::OnSetItemIsRadio(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    Item* item = (Item*)entity;
    bool value = *(bool*)cur_value;

    ItemMngr.RadioRegister(item, value);
}

void FOServer::OnSetItemOpened(Entity* entity, Property* prop, void* cur_value, void* old_value)
{
    Item* item = (Item*)entity;
    bool cur = *(bool*)cur_value;
    bool old = *(bool*)old_value;

    if (item->GetIsCanOpen())
    {
        if (!old && cur)
        {
            item->SetIsLightThru(true);

            if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
            {
                Map* map = MapMngr.GetMap(item->GetMapId());
                if (map)
                    map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
            }
        }
        if (old && !cur)
        {
            item->SetIsLightThru(false);

            if (item->GetAccessory() == ITEM_ACCESSORY_HEX)
            {
                Map* map = MapMngr.GetMap(item->GetMapId());
                if (map)
                {
                    map->SetHexFlag(item->GetHexX(), item->GetHexY(), FH_BLOCK_ITEM);
                    map->SetHexFlag(item->GetHexX(), item->GetHexY(), FH_NRAKE_ITEM);
                }
            }
        }
    }
}

bool FOServer::DialogScriptDemand(DemandResult& demand, Critter* master, Critter* slave)
{
    /*int bind_id = (int)demand.ParamId;
    ScriptSys.PrepareContext(bind_id, master->GetName());
    ScriptSys.SetArgEntity(master);
    ScriptSys.SetArgEntity(slave);
    for (int i = 0; i < demand.ValuesCount; i++)
        ScriptSys.SetArgUInt(demand.ValueExt[i]);
    if (ScriptSys.RunPrepared())
        return ScriptSys.GetReturnedBool();*/
    return false;
}

uint FOServer::DialogScriptResult(DemandResult& result, Critter* master, Critter* slave)
{
    /*int bind_id = (int)result.ParamId;
    ScriptSys.PrepareContext(
        bind_id, _str("Critter '{}', func '{}'", master->GetName(), ScriptSys.GetBindFuncName(bind_id)));
    ScriptSys.SetArgEntity(master);
    ScriptSys.SetArgEntity(slave);
    for (int i = 0; i < result.ValuesCount; i++)
        ScriptSys.SetArgUInt(result.ValueExt[i]);
    if (ScriptSys.RunPrepared() && result.RetValue)
        return ScriptSys.GetReturnedUInt();*/
    return 0;
}
