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
#include "Version-Include.h"
#include "WinApi-Include.h"

FOServer::FOServer(GlobalSettings& settings) : Settings {settings}, GeomHelper(Settings), ScriptSys(this, settings, FileMngr), GameTime(Settings), ProtoMngr(FileMngr), EntityMngr(ProtoMngr, ScriptSys, DbStorage, Globals), MapMngr(Settings, ProtoMngr, EntityMngr, CrMngr, ItemMngr, ScriptSys, GameTime), CrMngr(Settings, ProtoMngr, EntityMngr, MapMngr, ItemMngr, ScriptSys, GameTime), ItemMngr(ProtoMngr, EntityMngr, MapMngr, CrMngr, ScriptSys), DlgMngr(FileMngr, ScriptSys)
{
    WriteLog("***   Starting initialization   ***\n");

    GameTime.FrameAdvance();

    // Reserve memory
    ConnectedClients.reserve(10000);

    // Data base
    DbStorage = ConnectToDataBase(Settings.DbStorage);
    if (!DbStorage) {
        throw ServerInitException("Can't init storage data base", Settings.DbStorage);
    }

    if (Settings.DbHistory != "None") {
        DbHistory = ConnectToDataBase(Settings.DbHistory);
        if (!DbHistory) {
            throw ServerInitException("Can't init histrory data base", Settings.DbHistory);
        }
    }

    // Start data base changes
    DbStorage.StartChanges();
    if (DbHistory) {
        DbHistory.StartChanges();
    }

    // PropertyRegistrator::GlobalSetCallbacks.push_back(EntitySetValue);

    if (!InitLangPacks(LangPacks)) {
        throw ServerInitException("Can't init lang packs");
    }

    LoadBans();

    // Managers
    if (!DlgMngr.LoadDialogs()) {
        throw ServerInitException("Can't load dialogs");
    }
    // if (!ProtoMngr.LoadProtosFromFiles(FileMngr))
    //    return false;

    // Language packs
    if (!InitLangPacksDialogs(LangPacks)) {
        throw ServerInitException("Can't init dialogs lang packs");
    }
    if (!InitLangPacksLocations(LangPacks)) {
        throw ServerInitException("Can't init locations lang packs");
    }
    if (!InitLangPacksItems(LangPacks)) {
        throw ServerInitException("Can't init items lang packs");
    }

    // Load globals
    Globals = new GlobalVars();
    Globals->SetId(1);
    const auto globals_doc = DbStorage.Get("Globals", 1);
    if (globals_doc.empty()) {
        DbStorage.Insert("Globals", Globals->Id, {{"_Proto", string("")}});
    }
    else {
        if (!PropertiesSerializator::LoadFromDbDocument(&Globals->Props, globals_doc, ScriptSys)) {
            throw ServerInitException("Failed to load globals document");
        }

        GameTime.Reset(Globals->GetYear(), Globals->GetMonth(), Globals->GetDay(), Globals->GetHour(), Globals->GetMinute(), Globals->GetSecond(), Globals->GetTimeMultiplier());
    }

    // Todo: restore hashes loading
    // _str::loadHashes();

    // Deferred calls
    // if (!ScriptSys.LoadDeferredCalls())
    //    return false;

    // Update files
    vector<string> resource_names;
    GenerateUpdateFiles(true, &resource_names);

    // Validate protos resources
    if (!ProtoMngr.ValidateProtoResources(resource_names)) {
        throw ServerInitException("Failed to validate resource names");
    }

    // Initialization script
    if (ScriptSys.InitEvent()) {
        throw ServerInitException("Initialization script failed");
    }

    // Init world
    if (globals_doc.empty()) {
        if (ScriptSys.GenerateWorldEvent()) {
            throw ServerInitException("Generate world script failed");
        }
    }
    else {
        const auto loc_fabric = [this](uint id, const ProtoLocation* proto) { return new Location(id, proto, ScriptSys); };
        const auto map_fabric = [this](uint id, const ProtoMap* proto) {
            const auto* static_map = MapMngr.FindStaticMap(proto);
            if (static_map == nullptr) {
                throw EntitiesLoadException("Static map not found", proto->GetName());
            }
            return new Map(id, proto, nullptr, static_map, Settings, ScriptSys, GameTime);
        };
        const auto cr_fabric = [this](uint id, const ProtoCritter* proto) { return new Npc(id, proto, Settings, ScriptSys, GameTime); };
        const auto item_fabric = [this](uint id, const ProtoItem* proto) { return new Item(id, proto, ScriptSys); };
        EntityMngr.LoadEntities(loc_fabric, map_fabric, cr_fabric, item_fabric);

        MapMngr.LinkMaps();
        CrMngr.LinkCritters();
        ItemMngr.LinkItems();

        EntityMngr.InitAfterLoad();

        for (auto* item : EntityMngr.GetItems()) {
            if (!item->IsDestroyed && item->GetIsRadio()) {
                ItemMngr.RegisterRadio(item);
            }
        }

        WriteLog("Process critters visibility...\n");
        for (auto* cr : EntityMngr.GetCritters()) {
            MapMngr.ProcessVisibleCritters(cr);
            MapMngr.ProcessVisibleItems(cr);
        }
        WriteLog("Process critters visibility complete.\n");
    }

    // Start script
    if (ScriptSys.StartEvent()) {
        throw ServerInitException("Start script failed");
    }

    // Commit data base changes
    DbStorage.CommitChanges();
    if (DbHistory) {
        DbHistory.CommitChanges();
    }

    Stats.ServerStartTick = GameTime.FrameTick();

    // Network
    WriteLog("Starting server on ports {} and {}.\n", Settings.Port, Settings.Port + 1);
    if ((TcpServer = NetServerBase::StartTcpServer(Settings, std::bind(&FOServer::OnNewConnection, this, std::placeholders::_1))) == nullptr) {
        throw ServerInitException("Can't listen TCP server ports", Settings.Port);
    }
    if ((WebSocketsServer = NetServerBase::StartWebSocketsServer(Settings, std::bind(&FOServer::OnNewConnection, this, std::placeholders::_1))) == nullptr) {
        throw ServerInitException("Can't listen TCP server ports", Settings.Port + 1);
    }

    // Admin manager
    if (Settings.AdminPanelPort != 0u) {
        InitAdminManager(this, Settings.AdminPanelPort);
    }

    GameTime.FrameAdvance();
    FpsTick = GameTime.FrameTick();
    Started = true;
}

FOServer::~FOServer()
{
    delete TcpServer;
    delete WebSocketsServer;
}

void FOServer::Shutdown()
{
    WillFinishDispatcher();

    // Finish logic
    DbStorage.StartChanges();
    if (DbHistory) {
        DbHistory.StartChanges();
    }

    ScriptSys.FinishEvent();
    EntityMngr.FinalizeEntities();

    DbStorage.CommitChanges();
    if (DbHistory) {
        DbHistory.CommitChanges();
    }

    // Logging clients
    LogToFunc("LogToClients", std::bind(&FOServer::LogToClients, this, std::placeholders::_1), false);
    for (auto* cl : LogClients) {
        cl->Release();
    }
    LogClients.clear();

    // Clients
    {
        SCOPE_LOCK(ConnectedClientsLocker);

        for (auto* cl : ConnectedClients) {
            cl->Disconnect();
        }
    }

    // Shutdown servers
    if (TcpServer != nullptr) {
        TcpServer->Shutdown();
        delete TcpServer;
        TcpServer = nullptr;
    }
    if (WebSocketsServer != nullptr) {
        WebSocketsServer->Shutdown();
        delete WebSocketsServer;
        WebSocketsServer = nullptr;
    }

    // Stats
    WriteLog("Server stopped.\n");
    WriteLog("Stats:\n");
    WriteLog("Traffic:\n");
    WriteLog("Bytes Send: {}\n", Stats.BytesSend);
    WriteLog("Bytes Recv: {}\n", Stats.BytesRecv);
    WriteLog("Cycles count: {}\n", Stats.LoopCycles);
    WriteLog("Approx cycle period: {}\n", Stats.LoopTime / (Stats.LoopCycles != 0u ? Stats.LoopCycles : 1));
    WriteLog("Min cycle period: {}\n", Stats.LoopMin);
    WriteLog("Max cycle period: {}\n", Stats.LoopMax);
    WriteLog("Count of lags (>100ms): {}\n", Stats.LagsCount);

    DidFinishDispatcher();
}

void FOServer::MainLoop()
{
    if (GameTime.FrameAdvance()) {
        const auto st = GameTime.GetGameTime(GameTime.GetFullSecond());
        Globals->SetYear(st.Year);
        Globals->SetMonth(st.Month);
        Globals->SetDay(st.Day);
        Globals->SetHour(st.Hour);
        Globals->SetMinute(st.Minute);
        Globals->SetSecond(st.Second);
    }

    // Cycle time
    const auto frame_begin = Timer::RealtimeTick();

    // Begin data base changes
    DbStorage.StartChanges();
    if (DbHistory) {
        DbHistory.StartChanges();
    }

    // Process clients
    vector<Client*> clients;
    {
        SCOPE_LOCK(ConnectedClientsLocker);

        clients = ConnectedClients;
        for (auto* cl : clients) {
            cl->AddRef();
        }
    }

    for (auto* cl : clients) {
        // Check for removing
        if (cl->IsOffline()) {
            DisconnectClient(cl);

            {
                SCOPE_LOCK(ConnectedClientsLocker);

                auto it = std::find(ConnectedClients.begin(), ConnectedClients.end(), cl);
                RUNTIME_ASSERT(it != ConnectedClients.end());
                ConnectedClients.erase(it);
                Stats.CurOnline--;
            }

            cl->Release();
            continue;
        }

        // Process network messages
        Process(cl);
        cl->Release();
    }

    // Process critters
    for (auto* cr : EntityMngr.GetCritters()) {
        // Player specific
        if (cr->CanBeRemoved) {
            RemoveClient(dynamic_cast<Client*>(cr));
        }

        // Check for removing
        if (cr->IsDestroyed) {
            continue;
        }

        // Process logic
        ProcessCritter(cr);
    }

    // Process maps
    for (auto* map : EntityMngr.GetMaps()) {
        // Check for removing
        if (map->IsDestroyed) {
            continue;
        }

        // Process logic
        map->Process();
    }

    // Locations and maps garbage
    MapMngr.LocationGarbager();

    // Bans
    ProcessBans();

    // Script game loop
    ScriptSys.LoopEvent();

    // Commit changed to data base
    DbStorage.CommitChanges();
    if (DbHistory) {
        DbHistory.CommitChanges();
    }

    // Clients log
    DispatchLogToClients();

    // Fill statistics
    const auto frame_time = Timer::RealtimeTick() - frame_begin;
    const auto loop_tick = static_cast<uint>(frame_time);
    Stats.LoopTime += loop_tick;
    Stats.LoopCycles++;
    if (loop_tick > Stats.LoopMax) {
        Stats.LoopMax = loop_tick;
    }
    if (loop_tick < Stats.LoopMin) {
        Stats.LoopMin = loop_tick;
    }
    Stats.CycleTime = loop_tick;
    if (loop_tick > 100) {
        Stats.LagsCount++;
    }
    Stats.Uptime = (GameTime.FrameTick() - Stats.ServerStartTick) / 1000;

    // Calculate fps
    if (GameTime.FrameTick() - FpsTick >= 1000) {
        Stats.Fps = FpsCounter;
        FpsCounter = 0;
        FpsTick = GameTime.FrameTick();
    }
    else {
        FpsCounter++;
    }

    // Sleep
    if (Settings.GameSleep >= 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(Settings.GameSleep));
    }
}

void FOServer::DrawGui()
{
    // Players
    ImGui::SetNextWindowPos(Gui.PlayersPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Players", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Gui.Stats = "WIP..........................."; // ( Server && Server->Started ? Server->GetIngamePlayersStatistics() :
                                                      // "Waiting for server start..." );
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Locations and maps
    ImGui::SetNextWindowPos(Gui.LocMapsPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Locations and maps", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Gui.Stats = "WIP..........................."; // ( Server && Server->Started ? MapMngr.GetLocationAndMapsStatistics() :
                                                      // "Waiting for server start..." );
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Items count
    ImGui::SetNextWindowPos(Gui.ItemsPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Items count", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Gui.Stats = (Started ? ItemMngr.GetItemsStatistics() : "Waiting for server start...");
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Profiler
    ImGui::SetNextWindowPos(Gui.ProfilerPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Gui.Stats = "WIP..........................."; // ScriptSys.GetProfilerStatistics();
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Info
    ImGui::SetNextWindowPos(Gui.InfoPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Gui.Stats = "";
        const auto st = GameTime.GetGameTime(GameTime.GetFullSecond());
        Gui.Stats += _str("Time: {:02}.{:02}.{:04} {:02}:{:02}:{:02} x{}\n", st.Day, st.Month, st.Year, st.Hour, st.Minute, st.Second, "WIP" /*Globals->GetTimeMultiplier()*/);
        Gui.Stats += _str("Connections: {}\n", Stats.CurOnline);
        Gui.Stats += _str("Players in game: {}\n", CrMngr.PlayersInGame());
        Gui.Stats += _str("NPC in game: {}\n", CrMngr.NpcInGame());
        Gui.Stats += _str("Locations: {} ({})\n", MapMngr.GetLocationsCount(), MapMngr.GetMapsCount());
        Gui.Stats += _str("Items: {}\n", ItemMngr.GetItemsCount());
        Gui.Stats += _str("Cycles per second: {}\n", Stats.Fps);
        Gui.Stats += _str("Cycle time: {}\n", Stats.CycleTime);
        const auto seconds = Stats.Uptime;
        Gui.Stats += _str("Uptime: {:02}:{:02}:{:02}\n", seconds / 60 / 60, seconds / 60 % 60, seconds % 60);
        Gui.Stats += _str("KBytes Send: {}\n", Stats.BytesSend / 1024);
        Gui.Stats += _str("KBytes Recv: {}\n", Stats.BytesRecv / 1024);
        Gui.Stats += _str("Compress ratio: {}", static_cast<double>(Stats.DataReal) / (Stats.DataCompressed != 0 ? Stats.DataCompressed : 1));
        ImGui::TextUnformatted(Gui.Stats.c_str(), Gui.Stats.c_str() + Gui.Stats.size());
    }
    ImGui::End();

    // Control panel
    ImGui::SetNextWindowPos(Gui.ControlPanelPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.DefaultSize, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    if (ImGui::Begin("Control panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (Started && !Settings.Quit && ImGui::Button("Stop & Quit", Gui.ButtonSize)) {
            Settings.Quit = true;
        }
        if (ImGui::Button("Quit", Gui.ButtonSize)) {
            exit(0);
        }
        if (ImGui::Button("Create dump", Gui.ButtonSize)) {
            CreateDump("ManualDump", "Manual");
        }
        if (ImGui::Button("Save log", Gui.ButtonSize)) {
            const auto dt = Timer::GetCurrentDateTime();
            const string log_name = _str("Logs/FOnlineServer_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.log", "Log", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
            auto log_file = FileMngr.WriteFile(log_name, false);
            log_file.SetStr(Gui.WholeLog);
            log_file.Save();
        }
    }
    ImGui::End();

    // Log
    ImGui::SetNextWindowPos(Gui.LogPos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(Gui.LogSize, ImGuiCond_Once);
    if (ImGui::Begin("Log", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
        const auto log = LogGetBuffer();
        if (!log.empty()) {
            Gui.WholeLog += log;
            if (Gui.WholeLog.size() > 100000) {
                Gui.WholeLog = Gui.WholeLog.substr(Gui.WholeLog.size() - 100000);
            }
        }

        if (!Gui.WholeLog.empty()) {
            ImGui::TextUnformatted(Gui.WholeLog.c_str(), Gui.WholeLog.c_str() + Gui.WholeLog.size());
        }
    }
    ImGui::End();
}

auto FOServer::GetIngamePlayersStatistics() -> string
{
    const char* cond_states_str[] = {"None", "Life", "Knockout", "Dead"};

    uint conn_count;
    {
        SCOPE_LOCK(ConnectedClientsLocker);
        conn_count = static_cast<uint>(ConnectedClients.size());
    }

    auto clients = CrMngr.GetClients(false);

    string result = _str("Players in game: {}\nConnections: {}\n", clients.size(), conn_count);
    result += "Name                 Id         Ip              Online  Cond     X     Y     Location and map\n";
    for (auto* cl : clients) {
        auto* map = MapMngr.GetMap(cl->GetMapId());
        auto* loc = (map != nullptr ? map->GetLocation() : nullptr);

        const string str_loc = _str("{} ({}) {} ({})", map != nullptr ? loc->GetName() : "", map != nullptr ? loc->GetId() : 0, map != nullptr ? map->GetName() : "", map != nullptr ? map->GetId() : 0);
        result += _str("{:<20} {:<10} {:<15} {:<7} {:<8} {:<5} {:<5} {}\n", cl->Name, cl->GetId(), cl->GetIpStr(), cl->IsOffline() ? "No" : "Yes", cond_states_str[cl->GetCond()], map != nullptr ? cl->GetHexX() : cl->GetWorldX(), map != nullptr ? cl->GetHexY() : cl->GetWorldY(), map != nullptr ? str_loc : "Global map");
    }
    return result;
}

// Accesses
void FOServer::GetAccesses(vector<string>& client, vector<string>& tester, vector<string>& moder, vector<string>& admin, vector<string>& admin_names)
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
    const auto id = cl->GetId();
    auto* cl_ = (id != 0u ? CrMngr.GetPlayer(id) : nullptr);
    if (cl_ != nullptr && cl_ == cl) {
        SetBit(cl->Flags, FCRIT_DISCONNECT);
        if (cl->GetMapId() != 0u) {
            cl->SendA_Action(ACTION_DISCONNECT, 0, nullptr);
        }
        else {
            for (auto it = cl->GlobalMapGroup->begin(), end = cl->GlobalMapGroup->end(); it != end; ++it) {
                auto* cr = *it;
                if (cr != cl) {
                    cr->Send_CustomCommand(cl, OTHER_FLAGS, cl->Flags);
                }
            }
        }
    }
}

void FOServer::RemoveClient(Client* cl)
{
    const auto id = cl->GetId();
    auto* cl_ = (id != 0u ? CrMngr.GetPlayer(id) : nullptr);
    if (cl_ != nullptr && cl_ == cl) {
        RUNTIME_ASSERT(cl->Connection->IsDisconnected);

#ifndef FO_SINGLEPLAYER
        ScriptSys.PlayerLogoutEvent(cl);
#endif

        if (cl->GetClientToDelete()) {
            ScriptSys.CritterFinishEvent(cl);
        }

        auto* map = MapMngr.GetMap(cl->GetMapId());
        MapMngr.EraseCrFromMap(cl, map);

        // Destroy
        const auto full_delete = cl->GetClientToDelete();
        EntityMngr.UnregisterEntity(cl);
        cl->IsDestroyed = true;

        // Erase radios from collection
        auto items = cl->GetItemsNoLock();
        for (auto* item : items) {
            if (item->GetIsRadio()) {
                ItemMngr.UnregisterRadio(item);
            }
        }

        // Full delete
        if (full_delete) {
            CrMngr.DeleteInventory(cl);
            DbStorage.Delete("Players", cl->Id);
        }

        cl->Release();
    }
    else {
        cl->IsDestroyed = true;
        ScriptSys.RemoveEntity(cl);
    }
}

void FOServer::OnNewConnection(NetConnection* connection)
{
    // Allocate client
    const auto* cl_proto = ProtoMngr.GetProtoCritter(_str("Player").toHash());
    RUNTIME_ASSERT(cl_proto);

    auto* cl = new Client(connection, cl_proto, Settings, ScriptSys, GameTime);

    // Add to connected collection
    {
        SCOPE_LOCK(ConnectedClientsLocker);

        cl->State = ClientState::Connected;
        ConnectedClients.push_back(cl);
        Stats.CurOnline++;
    }
}

#define CHECK_BUSY \
    if (cl->IsBusy()) { \
        cl->Connection->Bin.MoveReadPos(-int(sizeof(msg))); \
        return; \
    }

void FOServer::Process(Client* cl)
{
    if (cl->IsOffline() || cl->IsDestroyed) {
        SCOPE_LOCK(cl->Connection->BinLocker);
        cl->Connection->Bin.Reset();
        return;
    }

    if (cl->State == ClientState::Connected) {
        SCOPE_LOCK(cl->Connection->BinLocker);

        if (cl->Connection->Bin.IsEmpty()) {
            cl->Connection->Bin.Reset();
        }

        cl->Connection->Bin.Refresh();

        if (cl->Connection->Bin.NeedProcess()) {
            uint msg = 0;
            cl->Connection->Bin >> msg;

            auto tick = GameTime.FrameTick();
            switch (msg) {
            case 0xFFFFFFFF: {
                // At least 16 bytes should be sent for backward compatibility,
                // even if answer data will change its meaning
                CLIENT_OUTPUT_BEGIN(cl);
                cl->Connection->DisableCompression();
                cl->Connection->Bout << Stats.CurOnline - 1;
                cl->Connection->Bout << Stats.Uptime;
                cl->Connection->Bout << static_cast<uint>(0);
                cl->Connection->Bout << static_cast<uchar>(0);
                cl->Connection->Bout << static_cast<uchar>(0xF0);
                cl->Connection->Bout << static_cast<ushort>(FO_VERSION);
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

            cl->LastActivityTime = GameTime.FrameTick();
        }
        else {
            CHECK_IN_BUFF_ERROR_EXT(cl, (void)0, (void)0)
        }

        if (cl->State == ClientState::Connected && (cl->LastActivityTime != 0u) && GameTime.FrameTick() - cl->LastActivityTime > PING_CLIENT_LIFE_TIME) // Kick bot
        {
            WriteLog("Connection timeout, client kicked, maybe bot. Ip '{}'.\n", cl->GetIpStr());
            cl->Disconnect();
        }
    }
    else if (cl->State == ClientState::Transferring) {
        SCOPE_LOCK(cl->Connection->BinLocker);

        if (cl->Connection->Bin.IsEmpty()) {
            cl->Connection->Bin.Reset();
        }

        cl->Connection->Bin.Refresh();

        if (cl->Connection->Bin.NeedProcess()) {
            uint msg = 0;
            cl->Connection->Bin >> msg;

            auto tick = GameTime.FrameTick();
            switch (msg) {
            case NETMSG_PING:
                Process_Ping(cl);
                break;
            case NETMSG_SEND_GIVE_MAP:
                CHECK_BUSY
                Process_GiveMap(cl);
                break;
            case NETMSG_SEND_LOAD_MAP_OK:
                CHECK_BUSY
                Process_ParseToGame(cl);
                break;
            default:
                cl->Connection->Bin.SkipMsg(msg);
                break;
            }
        }
        else {
            CHECK_IN_BUFF_ERROR_EXT(cl, (void)0, (void)0)
        }
    }
    else if (cl->State == ClientState::Playing) {
        static const auto messages_per_cycle = 5;
        for (auto i = 0; i < messages_per_cycle; i++) {
            SCOPE_LOCK(cl->Connection->BinLocker);

            if (cl->Connection->Bin.IsEmpty()) {
                cl->Connection->Bin.Reset();
            }

            cl->Connection->Bin.Refresh();

            if (!cl->Connection->Bin.NeedProcess()) {
                CHECK_IN_BUFF_ERROR_EXT(cl, (void)0, (void)0)
                break;
            }

            uint msg = 0;
            cl->Connection->Bin >> msg;

            auto tick = GameTime.FrameTick();
            switch (msg) {
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
                CHECK_BUSY
                Process_Dir(cl);
                continue;
            case NETMSG_SEND_MOVE_WALK:
                CHECK_BUSY
                Process_Move(cl);
                continue;
            case NETMSG_SEND_MOVE_RUN:
                CHECK_BUSY
                Process_Move(cl);
                continue;
            case NETMSG_SEND_TALK_NPC:
                CHECK_BUSY
                Process_Dialog(cl);
                continue;
            case NETMSG_SEND_REFRESH_ME:
                cl->Send_LoadMap(nullptr, MapMngr);
                continue;
            case NETMSG_SEND_GET_INFO:
                cl->Send_GameInfo(MapMngr.GetMap(cl->GetMapId()));
                continue;
            case NETMSG_SEND_GIVE_MAP:
                CHECK_BUSY
                Process_GiveMap(cl);
                continue;
            case NETMSG_RPC:
                CHECK_BUSY
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
    CHECK_IN_BUFF_ERROR(cl)

    if ((len == 0u) || len >= sizeof(str)) {
        WriteLog("Buffer zero sized or too large, length {}. Disconnect.\n", len);
        cl->Disconnect();
        return;
    }

    cl->Connection->Bin.Pop(str, len);
    str[len] = 0;
    CHECK_IN_BUFF_ERROR(cl)

    if (!cl->IsAlive() && how_say >= SAY_NORM && how_say <= SAY_RADIO) {
        how_say = SAY_WHISP;
    }

    if (cl->LastSay == str) {
        cl->LastSayEqualCount++;

        if (cl->LastSayEqualCount >= 10) {
            WriteLog("Flood detected, client '{}'. Disconnect.\n", cl->GetName());
            cl->Disconnect();
            return;
        }

        if (cl->LastSayEqualCount >= 3) {
            return;
        }
    }
    else {
        cl->LastSay = str;
        cl->LastSayEqualCount = 0;
    }

    vector<ushort> channels;

    switch (how_say) {
    case SAY_NORM: {
        if (cl->GetMapId() != 0u) {
            cl->SendAA_Text(cl->VisCr, str, SAY_NORM, true);
        }
        else {
            cl->SendAA_Text(*cl->GlobalMapGroup, str, SAY_NORM, true);
        }
    } break;
    case SAY_SHOUT: {
        if (cl->GetMapId() != 0u) {
            auto* map = MapMngr.GetMap(cl->GetMapId());
            if (map == nullptr) {
                return;
            }

            cl->SendAA_Text(map->GetCritters(), str, SAY_SHOUT, true);
        }
        else {
            cl->SendAA_Text(*cl->GlobalMapGroup, str, SAY_SHOUT, true);
        }
    } break;
    case SAY_EMOTE: {
        if (cl->GetMapId() != 0u) {
            cl->SendAA_Text(cl->VisCr, str, SAY_EMOTE, true);
        }
        else {
            cl->SendAA_Text(*cl->GlobalMapGroup, str, SAY_EMOTE, true);
        }
    } break;
    case SAY_WHISP: {
        if (cl->GetMapId() != 0u) {
            cl->SendAA_Text(cl->VisCr, str, SAY_WHISP, true);
        }
        else {
            cl->Send_TextEx(cl->GetId(), str, SAY_WHISP, true);
        }
    } break;
    case SAY_SOCIAL: {
        return;
    } break;
    case SAY_RADIO: {
        if (cl->GetMapId() != 0u) {
            cl->SendAA_Text(cl->VisCr, str, SAY_WHISP, true);
        }
        else {
            cl->Send_TextEx(cl->GetId(), str, SAY_WHISP, true);
        }

        ItemMngr.RadioSendText(cl, str, true, 0, 0, channels);
        if (channels.empty()) {
            cl->Send_TextMsg(cl, STR_RADIO_CANT_SEND, SAY_NETMSG, TEXTMSG_GAME);
            return;
        }
    } break;
    default:
        return;
    }

    // Text listen
    auto listen_count = 0;
    ScriptFunc<void, Critter*, string> listen_func[100]; // 100 calls per one message is enough
    string listen_str[100];

    {
        SCOPE_LOCK(TextListenersLocker);

        if (how_say == SAY_RADIO) {
            for (auto& tl : TextListeners) {
                if (tl.SayType == SAY_RADIO && std::find(channels.begin(), channels.end(), tl.Parameter) != channels.end() && _str(string(str).substr(0, tl.FirstStr.length())).compareIgnoreCaseUtf8(tl.FirstStr)) {
                    listen_func[listen_count] = tl.Func;
                    listen_str[listen_count] = str;
                    if (++listen_count >= 100) {
                        break;
                    }
                }
            }
        }
        else {
            auto* map = MapMngr.GetMap(cl->GetMapId());
            const auto pid = (map != nullptr ? map->GetProtoId() : 0);
            for (auto& tl : TextListeners) {
                if (tl.SayType == how_say && tl.Parameter == pid && _str(string(str).substr(0, tl.FirstStr.length())).compareIgnoreCaseUtf8(tl.FirstStr)) {
                    listen_func[listen_count] = tl.Func;
                    listen_str[listen_count] = str;
                    if (++listen_count >= 100) {
                        break;
                    }
                }
            }
        }
    }

    for (auto i = 0; i < listen_count; i++) {
        listen_func[i](cl, listen_str[i]);
    }
}

void FOServer::Process_Command(NetBuffer& buf, const LogFunc& logcb, Client* cl_, const string& admin_panel)
{
    LogToFunc("Process_Command", logcb, true);
    Process_CommandReal(buf, logcb, cl_, admin_panel);
    LogToFunc("Process_Command", logcb, false);
}

void FOServer::Process_CommandReal(NetBuffer& buf, const LogFunc& logcb, Client* cl_, const string& admin_panel)
{
    uint msg_len = 0;
    uchar cmd = 0;

    buf >> msg_len;
    buf >> cmd;

    auto sstr = (cl_ != nullptr ? "" : admin_panel);
    auto allow_command = ScriptSys.PlayerAllowCommandEvent(cl_, sstr, cmd);

    if (!allow_command && (cl_ == nullptr)) {
        logcb("Command refused by script.");
        return;
    }

#define CHECK_ALLOW_COMMAND() \
    do { \
        if (!allow_command) \
            return; \
    } while (0)

#define CHECK_ADMIN_PANEL() \
    do { \
        if (!cl_) { \
            logcb("Can't execute this command in admin panel."); \
            return; \
        } \
    } while (0)

    switch (cmd) {
    case CMD_EXIT: {
        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        cl_->Disconnect();
    } break;
    case CMD_MYINFO: {
        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        string istr = _str("|0xFF00FF00 Name: |0xFFFF0000 {}|0xFF00FF00 , Id: |0xFFFF0000 {}|0xFF00FF00 , Access: ", cl_->GetName(), cl_->GetId());
        switch (cl_->Access) {
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
    } break;
    case CMD_GAMEINFO: {
        auto info = 0;
        buf >> info;

        CHECK_ALLOW_COMMAND();

        string result;
        switch (info) {
        case 1:
            result = GetIngamePlayersStatistics();
            break;
        case 2:
            result = MapMngr.GetLocationAndMapsStatistics();
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

            str = _str("Connections: {}, Players: {}, Npc: {}. FOServer machine uptime: {} min., FOServer uptime: {} min.", ConnectedClients.size(), CrMngr.PlayersInGame(), CrMngr.NpcInGame(), GameTime.FrameTick() / 1000 / 60, (GameTime.FrameTick() - Stats.ServerStartTick) / 1000 / 60);
        }

        result += str;

        for (const auto& line : _str(result).split('\n')) {
            logcb(line);
        }
    } break;
    case CMD_CRITID: {
        string name;
        buf >> name;

        CHECK_ALLOW_COMMAND();

        auto id = MAKE_CLIENT_ID(name);
        if (!DbStorage.Get("Players", id).empty()) {
            logcb(_str("Client id is {}.", id));
        }
        else {
            logcb("Client not found.");
        }
    } break;
    case CMD_MOVECRIT: {
        uint crid = 0;
        ushort hex_x = 0;
        ushort hex_y = 0;
        buf >> crid;
        buf >> hex_x;
        buf >> hex_y;

        CHECK_ALLOW_COMMAND();

        auto* cr = CrMngr.GetCritter(crid);
        if (cr == nullptr) {
            logcb("Critter not found.");
            break;
        }

        auto* map = MapMngr.GetMap(cr->GetMapId());
        if (map == nullptr) {
            logcb("Critter is on global.");
            break;
        }

        if (hex_x >= map->GetWidth() || hex_y >= map->GetHeight()) {
            logcb("Invalid hex position.");
            break;
        }

        if (MapMngr.Transit(cr, map, hex_x, hex_y, cr->GetDir(), 3, 0, true)) {
            logcb("Critter move success.");
        }
        else {
            logcb("Critter move fail.");
        }
    } break;
    case CMD_DISCONCRIT: {
        uint crid = 0;
        buf >> crid;

        CHECK_ALLOW_COMMAND();

        if ((cl_ != nullptr) && cl_->GetId() == crid) {
            logcb("To kick yourself type <~exit>");
            return;
        }

        auto* cr = CrMngr.GetCritter(crid);
        if (cr == nullptr) {
            logcb("Critter not found.");
            return;
        }

        if (!cr->IsPlayer()) {
            logcb("Founded critter is not a player.");
            return;
        }

        auto* cl2 = dynamic_cast<Client*>(cr);
        if (cl2->State != ClientState::Playing) {
            logcb("Player is not in a game.");
            return;
        }

        cl2->Send_Text(cl2, "You are kicked from game.", SAY_NETMSG);
        cl2->Disconnect();

        logcb("Player disconnected.");
    } break;
    case CMD_TOGLOBAL: {
        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        if (!cl_->IsAlive()) {
            logcb("To global fail, only life none.");
            break;
        }

        if (MapMngr.TransitToGlobal(cl_, 0, false)) {
            logcb("To global success.");
        }
        else {
            logcb("To global fail.");
        }
    } break;
    case CMD_PROPERTY: {
        uint crid = 0;
        string property_name;
        auto property_value = 0;
        buf >> crid;
        buf >> property_name;
        buf >> property_value;

        CHECK_ALLOW_COMMAND();

        auto* cr = (crid == 0u ? cl_ : CrMngr.GetCritter(crid));
        if (cr != nullptr) {
            auto* prop = Critter::PropertiesRegistrator->Find(property_name);
            if (prop == nullptr) {
                logcb("Property not found.");
                return;
            }
            if (!prop->IsPOD() || prop->GetBaseSize() != 4) {
                logcb("For now you can modify only int/uint properties.");
                return;
            }

            cr->Props.SetValue<int>(prop, property_value);
            logcb("Done.");
        }
        else {
            logcb("Critter not found.");
        }
    } break;
    case CMD_GETACCESS: {
        string name_access;
        string pasw_access;
        buf >> name_access;
        buf >> pasw_access;

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        vector<string> client;
        vector<string> tester;
        vector<string> moder;
        vector<string> admin;
        vector<string> admin_names;
        GetAccesses(client, tester, moder, admin, admin_names);

        auto wanted_access = -1;
        if (name_access == "client" && std::find(client.begin(), client.end(), pasw_access) != client.end()) {
            wanted_access = ACCESS_CLIENT;
        }
        else if (name_access == "tester" && std::find(tester.begin(), tester.end(), pasw_access) != tester.end()) {
            wanted_access = ACCESS_TESTER;
        }
        else if (name_access == "moder" && std::find(moder.begin(), moder.end(), pasw_access) != moder.end()) {
            wanted_access = ACCESS_MODER;
        }
        else if (name_access == "admin" && std::find(admin.begin(), admin.end(), pasw_access) != admin.end()) {
            wanted_access = ACCESS_ADMIN;
        }

        auto allow = false;
        if (wanted_access != -1) {
            auto& pass = pasw_access;
            allow = ScriptSys.PlayerGetAccessEvent(cl_, wanted_access, pass);
        }

        if (!allow) {
            logcb("Access denied.");
            break;
        }

        cl_->Access = wanted_access;
        logcb("Access changed.");
    } break;
    case CMD_ADDITEM: {
        ushort hex_x = 0;
        ushort hex_y = 0;
        hash pid = 0;
        uint count = 0;
        buf >> hex_x;
        buf >> hex_y;
        buf >> pid;
        buf >> count;

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        auto* map = MapMngr.GetMap(cl_->GetMapId());
        if ((map == nullptr) || hex_x >= map->GetWidth() || hex_y >= map->GetHeight()) {
            logcb("Wrong hexes or critter on global map.");
            return;
        }

        if (CreateItemOnHex(map, hex_x, hex_y, pid, count, nullptr, true) == nullptr) {
            logcb("Item(s) not added.");
        }
        else {
            logcb("Item(s) added.");
        }
    } break;
    case CMD_ADDITEM_SELF: {
        hash pid = 0;
        uint count = 0;
        buf >> pid;
        buf >> count;

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        if (ItemMngr.AddItemCritter(cl_, pid, count) != nullptr) {
            logcb("Item(s) added.");
        }
        else {
            logcb("Item(s) added fail.");
        }
    } break;
    case CMD_ADDNPC: {
        ushort hex_x = 0;
        ushort hex_y = 0;
        uchar dir = 0;
        hash pid = 0;
        buf >> hex_x;
        buf >> hex_y;
        buf >> dir;
        buf >> pid;

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        auto* map = MapMngr.GetMap(cl_->GetMapId());
        auto* npc = CrMngr.CreateNpc(pid, nullptr, map, hex_x, hex_y, dir, true);
        if (npc == nullptr) {
            logcb("Npc not created.");
        }
        else {
            logcb("Npc created.");
        }
    } break;
    case CMD_ADDLOCATION: {
        ushort wx = 0;
        ushort wy = 0;
        hash pid = 0;
        buf >> wx;
        buf >> wy;
        buf >> pid;

        CHECK_ALLOW_COMMAND();

        auto* loc = MapMngr.CreateLocation(pid, wx, wy);
        if (loc == nullptr) {
            logcb("Location not created.");
        }
        else {
            logcb("Location created.");
        }
    } break;
    case CMD_RUNSCRIPT: {
        string func_name;
        uint param0 = 0;
        uint param1 = 0;
        uint param2 = 0;
        buf >> func_name;
        buf >> param0;
        buf >> param1;
        buf >> param2;

        CHECK_ALLOW_COMMAND();

        if (func_name.empty()) {
            logcb("Fail, length is zero.");
            break;
        }

        if (ScriptSys.CallFunc<void, Critter*, int, int, int>(func_name, cl_, param0, param1, param2)) {
            logcb("Run script success.");
        }
        else {
            logcb("Run script fail.");
        }
    } break;
    case CMD_REGENMAP: {
        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        // Check global
        if (cl_->GetMapId() == 0u) {
            logcb("Only on local map.");
            return;
        }

        // Find map
        auto* map = MapMngr.GetMap(cl_->GetMapId());
        if (map == nullptr) {
            logcb("Map not found.");
            return;
        }

        // Regenerate
        auto hx = cl_->GetHexX();
        auto hy = cl_->GetHexY();
        auto dir = cl_->GetDir();
        MapMngr.RegenerateMap(map);
        MapMngr.Transit(cl_, map, hx, hy, dir, 5, 0, true);
        logcb("Regenerate map complete.");
    } break;
    case CMD_SETTIME: {
        auto multiplier = 0;
        auto year = 0;
        auto month = 0;
        auto day = 0;
        auto hour = 0;
        auto minute = 0;
        auto second = 0;
        buf >> multiplier;
        buf >> year;
        buf >> month;
        buf >> day;
        buf >> hour;
        buf >> minute;
        buf >> second;

        CHECK_ALLOW_COMMAND();

        SetGameTime(multiplier, year, month, day, hour, minute, second);
        logcb("Time changed.");
    } break;
    case CMD_BAN: {
        string name;
        string params;
        uint ban_hours = 0;
        string info;
        buf >> name;
        buf >> params;
        buf >> ban_hours;
        buf >> info;

        CHECK_ALLOW_COMMAND();

        SCOPE_LOCK(BannedLocker);

        if (_str(params).compareIgnoreCase("list")) {
            if (Banned.empty()) {
                logcb("Ban list empty.");
                return;
            }

            uint index = 1;
            for (auto& ban : Banned) {
                logcb(_str("--- {:3} ---", index));
                if (ban.ClientName[0] != 0) {
                    logcb(_str("User: {}", ban.ClientName));
                }
                if (ban.ClientIp != 0u) {
                    logcb(_str("UserIp: {}", ban.ClientIp));
                }
                logcb(_str("BeginTime: {} {} {}", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute));
                logcb(_str("EndTime: {} {} {}", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute));
                if (ban.BannedBy[0] != 0) {
                    logcb(_str("BannedBy: {}", ban.BannedBy));
                }
                if (ban.BanInfo[0] != 0) {
                    logcb(_str("Comment: {}", ban.BanInfo));
                }
                index++;
            }
        }
        else if (_str(params).compareIgnoreCase("add") || _str(params).compareIgnoreCase("add+")) {
            auto name_len = _str(name).lengthUtf8();
            if (name_len < MIN_NAME || name_len < Settings.MinNameLength || name_len > MAX_NAME || name_len > Settings.MaxNameLength || (ban_hours == 0u)) {
                logcb("Invalid arguments.");
                return;
            }

            auto id = MAKE_CLIENT_ID(name);
            auto* cl_banned = CrMngr.GetPlayer(id);
            ClientBanned ban;
            std::memset(&ban, 0, sizeof(ban));
            Str::Copy(ban.ClientName, name.c_str());
            ban.ClientIp = ((cl_banned != nullptr) && params.find('+') != string::npos ? cl_banned->GetIp() : 0);
            ban.BeginTime = Timer::GetCurrentDateTime();
            ban.EndTime = ban.BeginTime;
            ban.EndTime = Timer::AdvanceTime(ban.EndTime, static_cast<int>(ban_hours) * 60 * 60);
            Str::Copy(ban.BannedBy, cl_ != nullptr ? cl_->Name.c_str() : admin_panel.c_str());
            Str::Copy(ban.BanInfo, info.c_str());

            Banned.push_back(ban);
            SaveBan(ban, false);
            logcb("User banned.");

            if (cl_banned != nullptr) {
                cl_banned->Send_TextMsg(cl_banned, STR_NET_BAN, SAY_NETMSG, TEXTMSG_GAME);
                cl_banned->Send_TextMsgLex(cl_banned, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, GetBanLexems(ban).c_str());
                cl_banned->Disconnect();
            }
        }
        else if (_str(params).compareIgnoreCase("delete")) {
            if (name.empty()) {
                logcb("Invalid arguments.");
                return;
            }

            auto resave = false;
            if (name == "*") {
                auto index = static_cast<int>(ban_hours) - 1;
                if (index >= 0 && index < static_cast<int>(Banned.size())) {
                    Banned.erase(Banned.begin() + index);
                    resave = true;
                }
            }
            else {
                for (auto it = Banned.begin(); it != Banned.end();) {
                    auto& ban = *it;
                    if (_str(ban.ClientName).compareIgnoreCaseUtf8(name)) {
                        SaveBan(ban, true);
                        it = Banned.erase(it);
                        resave = true;
                    }
                    else {
                        ++it;
                    }
                }
            }

            if (resave) {
                SaveBans();
                logcb("User unbanned.");
            }
            else {
                logcb("User not found.");
            }
        }
        else {
            logcb("Unknown option.");
        }
    } break;
    case CMD_DELETE_ACCOUNT: {
        char pass_hash[PASS_HASH_SIZE];
        buf.Pop(pass_hash, PASS_HASH_SIZE);

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        if (std::memcmp(cl_->GetPassword().c_str(), pass_hash, PASS_HASH_SIZE) != 0) {
            logcb("Invalid password.");
        }
        else {
            if (!cl_->GetClientToDelete()) {
                cl_->SetClientToDelete(true);
                logcb("Your account will be deleted after character exit from game.");
            }
            else {
                cl_->SetClientToDelete(false);
                logcb("Deleting canceled.");
            }
        }
    } break;
    case CMD_CHANGE_PASSWORD: {
        char pass_hash[PASS_HASH_SIZE];
        char new_pass_hash[PASS_HASH_SIZE];
        buf.Pop(pass_hash, PASS_HASH_SIZE);
        buf.Pop(new_pass_hash, PASS_HASH_SIZE);

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        if (std::memcmp(cl_->GetPassword().c_str(), pass_hash, PASS_HASH_SIZE) != 0) {
            logcb("Invalid current password.");
        }
        else {
            cl_->SetPassword(new_pass_hash);
            logcb("Password changed.");
        }
    } break;
    case CMD_LOG: {
        char flags[16];
        buf.Pop(flags, 16);

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        LogToFunc("LogToClients", std::bind(&FOServer::LogToClients, this, std::placeholders::_1), false);

        auto it = std::find(LogClients.begin(), LogClients.end(), cl_);
        if (flags[0] == '-' && flags[1] == '\0' && it != LogClients.end()) // Detach current
        {
            logcb("Detached.");
            cl_->Release();
            LogClients.erase(it);
        }
        else if (flags[0] == '+' && flags[1] == '\0' && it == LogClients.end()) // Attach current
        {
            logcb("Attached.");
            cl_->AddRef();
            LogClients.push_back(cl_);
        }
        else if (flags[0] == '-' && flags[1] == '-' && flags[2] == '\0') // Detach all
        {
            logcb("Detached all.");
            for (auto* cl : LogClients) {
                cl->Release();
            }
            LogClients.clear();
        }

        if (!LogClients.empty()) {
            LogToFunc("LogToClients", std::bind(&FOServer::LogToClients, this, std::placeholders::_1), true);
        }
    } break;
    default:
        logcb("Unknown command.");
        break;
    }

#undef CHECK_ALLOW_COMMAND
#undef CHECK_ADMIN_PANEL
}

void FOServer::SetGameTime(int multiplier, int year, int month, int day, int hour, int minute, int second)
{
    RUNTIME_ASSERT(multiplier >= 1 && multiplier <= 50000);
    RUNTIME_ASSERT(year >= Settings.StartYear);
    RUNTIME_ASSERT(year < Settings.StartYear + 100);
    RUNTIME_ASSERT(month >= 1 && month <= 12);
    RUNTIME_ASSERT(day >= 1 && day <= 31);
    RUNTIME_ASSERT(hour >= 0 && hour <= 23);
    RUNTIME_ASSERT(minute >= 0 && minute <= 59);
    RUNTIME_ASSERT(second >= 0 && second <= 59);

    Globals->SetTimeMultiplier(multiplier);
    Globals->SetYear(year);
    Globals->SetMonth(month);
    Globals->SetDay(day);
    Globals->SetHour(hour);
    Globals->SetMinute(minute);
    Globals->SetSecond(second);

    GameTime.Reset(year, month, day, hour, minute, second, multiplier);

    {
        SCOPE_LOCK(ConnectedClientsLocker);

        for (auto* cl : ConnectedClients) {
            if (cl->IsOnline()) {
                cl->Send_GameInfo(MapMngr.GetMap(cl->GetMapId()));
            }
        }
    }
}

auto FOServer::InitLangPacks(vector<LanguagePack>& lang_packs) -> bool
{
    WriteLog("Load language packs...\n");

    uint cur_lang = 0;
    while (true) {
        // Todo: restore settings
        string cur_str_lang; // = _str("Language_{}", cur_lang);
        string lang_name; // = MainConfig->GetStr("", cur_str_lang);
        if (lang_name.empty()) {
            break;
        }

        if (lang_name.length() != 4) {
            WriteLog("Language name not equal to four letters.\n");
            return false;
        }

        auto pack_id = *reinterpret_cast<uint*>(&lang_name);
        if (std::find(lang_packs.begin(), lang_packs.end(), pack_id) != lang_packs.end()) {
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

auto FOServer::InitLangPacksDialogs(vector<LanguagePack>& lang_packs) -> bool
{
    for (auto& lang_pack : lang_packs) {
        lang_pack.Msg[TEXTMSG_DLG].Clear();
    }

    const auto& all_protos = ProtoMngr.GetProtoCritters();
    for (const auto& kv : all_protos) {
        auto* proto = kv.second;
        for (uint i = 0, j = static_cast<uint>(proto->TextsLang.size()); i < j; i++) {
            for (auto& lang : lang_packs) {
                if (proto->TextsLang[i] != lang.NameCode) {
                    continue;
                }

                if (lang.Msg[TEXTMSG_DLG].IsIntersects(*proto->Texts[i])) {
                    WriteLog("Warning! Proto item '{}' text intersection detected, send notification about this to developers.\n", proto->GetName());
                }

                lang.Msg[TEXTMSG_DLG] += *proto->Texts[i];
            }
        }
    }

    DialogPack* pack = nullptr;
    uint index = 0;
    while ((pack = DlgMngr.GetDialogByIndex(index++)) != nullptr) {
        for (uint i = 0, j = static_cast<uint>(pack->TextsLang.size()); i < j; i++) {
            for (auto& lang : lang_packs) {
                if (pack->TextsLang[i] != lang.NameCode) {
                    continue;
                }

                if (lang.Msg[TEXTMSG_DLG].IsIntersects(*pack->Texts[i])) {
                    WriteLog("Warning! Dialog '{}' text intersection detected, send notification about this to developers.\n", pack->PackName);
                }

                lang.Msg[TEXTMSG_DLG] += *pack->Texts[i];
            }
        }
    }

    return true;
}

auto FOServer::InitLangPacksLocations(vector<LanguagePack>& lang_packs) -> bool
{
    for (auto& lang_pack : lang_packs) {
        lang_pack.Msg[TEXTMSG_LOCATIONS].Clear();
    }

    const auto& protos = ProtoMngr.GetProtoLocations();
    for (const auto& kv : protos) {
        auto* ploc = kv.second;
        for (uint i = 0, j = static_cast<uint>(ploc->TextsLang.size()); i < j; i++) {
            for (auto& lang : lang_packs) {
                if (ploc->TextsLang[i] != lang.NameCode) {
                    continue;
                }

                if (lang.Msg[TEXTMSG_LOCATIONS].IsIntersects(*ploc->Texts[i])) {
                    WriteLog("Warning! Location '{}' text intersection detected, send notification about this to developers.\n", _str().parseHash(ploc->ProtoId));
                }

                lang.Msg[TEXTMSG_LOCATIONS] += *ploc->Texts[i];
            }
        }
    }

    return true;
}

auto FOServer::InitLangPacksItems(vector<LanguagePack>& lang_packs) -> bool
{
    for (auto& lang_pack : lang_packs) {
        lang_pack.Msg[TEXTMSG_ITEM].Clear();
    }

    const auto& protos = ProtoMngr.GetProtoItems();
    for (const auto& kv : protos) {
        auto* proto = kv.second;
        for (uint i = 0, j = static_cast<uint>(proto->TextsLang.size()); i < j; i++) {
            for (auto& lang : lang_packs) {
                if (proto->TextsLang[i] != lang.NameCode) {
                    continue;
                }

                if (lang.Msg[TEXTMSG_ITEM].IsIntersects(*proto->Texts[i])) {
                    WriteLog("Warning! Proto item '{}' text intersection detected, send notification about this to developers.\n", proto->GetName());
                }

                lang.Msg[TEXTMSG_ITEM] += *proto->Texts[i];
            }
        }
    }

    return true;
}

void FOServer::LogToClients(const string& str)
{
    if (!str.empty() && str.back() == '\n') {
        LogLines.emplace_back(str, 0, str.length() - 1);
    }
    else {
        LogLines.emplace_back(str);
    }
}

void FOServer::DispatchLogToClients()
{
    if (LogLines.empty()) {
        return;
    }

    for (const auto& str : LogLines) {
        for (auto it = LogClients.begin(); it < LogClients.end();) {
            auto* cl = *it;
            if (!cl->IsDestroyed && cl->IsOnline()) {
                cl->Send_TextEx(0, str, SAY_NETMSG, false);
                ++it;
            }
            else {
                cl->Release();
                it = LogClients.erase(it);
            }
        }
    }

    if (LogClients.empty()) {
        LogToFunc("LogToClients", std::bind(&FOServer::LogToClients, this, std::placeholders::_1), false);
    }

    LogLines.clear();
}

auto FOServer::GetBanByName(const char* name) -> FOServer::ClientBanned*
{
    const auto it = std::find_if(Banned.begin(), Banned.end(), [name](const ClientBanned& ban) { return _str(name).compareIgnoreCaseUtf8(ban.ClientName); });
    return it != Banned.end() ? &(*it) : nullptr;
}

auto FOServer::GetBanByIp(uint ip) -> FOServer::ClientBanned*
{
    const auto it = std::find_if(Banned.begin(), Banned.end(), [ip](const ClientBanned& ban) { return ban.ClientIp == ip; });
    return it != Banned.end() ? &(*it) : nullptr;
}

auto FOServer::GetBanTime(ClientBanned& ban) -> uint
{
    const auto time = Timer::GetCurrentDateTime();
    const auto diff = Timer::GetTimeDifference(ban.EndTime, time) / 60 + 1;
    return diff > 0 ? diff : 1;
}

auto FOServer::GetBanLexems(ClientBanned& ban) -> string
{
    return _str("$banby{}$time{}$reason{}", ban.BannedBy[0] != 0 ? ban.BannedBy : "?", Timer::GetTimeDifference(ban.EndTime, ban.BeginTime) / 60 / 60, ban.BanInfo[0] != 0 ? ban.BanInfo : "just for fun");
}

void FOServer::ProcessBans()
{
    SCOPE_LOCK(BannedLocker);

    auto resave = false;
    const auto time = Timer::GetCurrentDateTime();
    for (auto it = Banned.begin(); it != Banned.end();) {
        auto& ban_time = it->EndTime;
        if (time.Year >= ban_time.Year && time.Month >= ban_time.Month && time.Day >= ban_time.Day && time.Hour >= ban_time.Hour && time.Minute >= ban_time.Minute) {
            SaveBan(*it, true);
            it = Banned.erase(it);
            resave = true;
        }
        else {
            ++it;
        }
    }
    if (resave) {
        SaveBans();
    }
}

void FOServer::SaveBan(ClientBanned& ban, bool expired)
{
    SCOPE_LOCK(BannedLocker);

    const string ban_file_name = (expired ? BANS_FNAME_EXPIRED : BANS_FNAME_ACTIVE);
    auto out_ban_file = FileMngr.WriteFile(ban_file_name, true);

    out_ban_file.SetStr("[Ban]\n");
    if (ban.ClientName[0] != 0) {
        out_ban_file.SetStr(_str("User = {}\n", ban.ClientName));
    }
    if (ban.ClientIp != 0u) {
        out_ban_file.SetStr(_str("UserIp = {}\n", ban.ClientIp));
    }
    out_ban_file.SetStr(_str("BeginTime = {} {} {}\n", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute));
    out_ban_file.SetStr(_str("EndTime = {} {} {}\n", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute));
    if (ban.BannedBy[0] != 0) {
        out_ban_file.SetStr(_str("BannedBy = {}\n", ban.BannedBy));
    }
    if (ban.BanInfo[0] != 0) {
        out_ban_file.SetStr(_str("Comment = {}\n", ban.BanInfo));
    }
    out_ban_file.SetStr("\n");

    out_ban_file.Save();
}

void FOServer::SaveBans()
{
    SCOPE_LOCK(BannedLocker);

    auto out_ban_file = FileMngr.WriteFile(BANS_FNAME_ACTIVE, false);
    for (auto& ban : Banned) {
        out_ban_file.SetStr("[Ban]\n");
        if (ban.ClientName[0] != 0) {
            out_ban_file.SetStr(_str("User = {}\n", ban.ClientName));
        }
        if (ban.ClientIp != 0u) {
            out_ban_file.SetStr(_str("UserIp = {}\n", ban.ClientIp));
        }
        out_ban_file.SetStr(_str("BeginTime = {} {} {}\n", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute));
        out_ban_file.SetStr(_str("EndTime = {} {} {}\n", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute));
        if (ban.BannedBy[0] != 0) {
            out_ban_file.SetStr(_str("BannedBy = {}\n", ban.BannedBy));
        }
        if (ban.BanInfo[0] != 0) {
            out_ban_file.SetStr(_str("Comment = {}\n", ban.BanInfo));
        }
        out_ban_file.SetStr("\n");
    }

    out_ban_file.Save();
}

void FOServer::LoadBans()
{
    SCOPE_LOCK(BannedLocker);

    Banned.clear();

    auto bans = FileMngr.ReadConfigFile(BANS_FNAME_ACTIVE);
    if (!bans) {
        return;
    }

    while (bans.IsApp("Ban")) {
        ClientBanned ban;
        std::memset(&ban, 0, sizeof(ban));

        DateTimeStamp time;
        std::memset(&time, 0, sizeof(time));

        string s;
        if (!(s = bans.GetStr("Ban", "User")).empty()) {
            Str::Copy(ban.ClientName, s.c_str());
        }

        ban.ClientIp = bans.GetInt("Ban", "UserIp", 0);

        if (!(s = bans.GetStr("Ban", "BeginTime")).empty() && (sscanf(s.c_str(), "%hu%hu%hu%hu%hu", &time.Year, &time.Month, &time.Day, &time.Hour, &time.Minute) != 0)) {
            ban.BeginTime = time;
        }

        if (!(s = bans.GetStr("Ban", "EndTime")).empty() && (sscanf(s.c_str(), "%hu%hu%hu%hu%hu", &time.Year, &time.Month, &time.Day, &time.Hour, &time.Minute) != 0)) {
            ban.EndTime = time;
        }

        if (!(s = bans.GetStr("Ban", "BannedBy")).empty()) {
            Str::Copy(ban.BannedBy, s.c_str());
        }

        if (!(s = bans.GetStr("Ban", "Comment")).empty()) {
            Str::Copy(ban.BanInfo, s.c_str());
        }

        Banned.push_back(ban);

        bans.GotoNextApp("Ban");
    }
    ProcessBans();
}

void FOServer::GenerateUpdateFiles(bool first_generation, vector<string>* resource_names)
{
    if (!first_generation && UpdateFiles.empty()) {
        return;
    }

    WriteLog("Generate update files...\n");

    // Disconnect all connected clients to force data updating
    if (!first_generation) {
        SCOPE_LOCK(ConnectedClientsLocker);

        for (auto* cl : ConnectedClients) {
            cl->Disconnect();
        }
    }

    // Clear collections
    for (auto& update_file : UpdateFiles) {
        delete[] update_file.Data;
    }
    UpdateFiles.clear();
    UpdateFilesList.clear();

    // Fill MSG
    UpdateFile update_file {};
    for (auto& lang_pack : LangPacks) {
        for (auto i = 0; i < TEXTMSG_COUNT; i++) {
            const auto msg_data = lang_pack.Msg[i].GetBinaryData();
            auto msg_cache_name = lang_pack.GetMsgCacheName(i);

            std::memset(&update_file, 0, sizeof(update_file));
            update_file.Size = static_cast<uint>(msg_data.size());
            update_file.Data = new uchar[update_file.Size];
            std::memcpy(update_file.Data, &msg_data[0], update_file.Size);
            UpdateFiles.push_back(update_file);

            WriteData(UpdateFilesList, static_cast<short>(msg_cache_name.length()));
            WriteDataArr(UpdateFilesList, msg_cache_name.c_str(), static_cast<uint>(msg_cache_name.length()));
            WriteData(UpdateFilesList, update_file.Size);
            WriteData(UpdateFilesList, Hashing::MurmurHash2(update_file.Data, update_file.Size));
        }
    }

    // Fill prototypes
    auto proto_items_data = ProtoMngr.GetProtosBinaryData();

    std::memset(&update_file, 0, sizeof(update_file));
    update_file.Size = static_cast<uint>(proto_items_data.size());
    update_file.Data = new uchar[update_file.Size];
    std::memcpy(update_file.Data, &proto_items_data[0], update_file.Size);
    UpdateFiles.push_back(update_file);

    const string protos_cache_name = "$protos.cache";
    WriteData(UpdateFilesList, static_cast<short>(protos_cache_name.length()));
    WriteDataArr(UpdateFilesList, protos_cache_name.c_str(), static_cast<uint>(protos_cache_name.length()));
    WriteData(UpdateFilesList, update_file.Size);
    WriteData(UpdateFilesList, Hashing::MurmurHash2(update_file.Data, update_file.Size));

    // Fill files
    auto files = FileMngr.FilterFiles("", "Update/", true);
    while (files.MoveNext()) {
        auto file = files.GetCurFile();

        std::memset(&update_file, 0, sizeof(update_file));
        update_file.Size = file.GetFsize();
        update_file.Data = file.ReleaseBuffer();
        UpdateFiles.push_back(update_file);

        auto file_path = file.GetName().substr("Update/"_len);
        WriteData(UpdateFilesList, static_cast<short>(file_path.length()));
        WriteDataArr(UpdateFilesList, file_path.c_str(), static_cast<uint>(file_path.length()));
        WriteData(UpdateFilesList, update_file.Size);
        WriteData(UpdateFilesList, Hashing::MurmurHash2(update_file.Data, update_file.Size));
    }

    WriteLog("Generate update files complete.\n");

    // Append binaries
    auto binaries = FileMngr.FilterFiles("", "Binaries/", true);
    while (binaries.MoveNext()) {
        auto file = binaries.GetCurFile();

        std::memset(&update_file, 0, sizeof(update_file));
        update_file.Size = file.GetFsize();
        update_file.Data = file.ReleaseBuffer();
        UpdateFiles.push_back(update_file);

        auto file_path = file.GetName().substr("Binaries/"_len);
        WriteData(UpdateFilesList, static_cast<short>(file_path.length()));
        WriteDataArr(UpdateFilesList, file_path.c_str(), static_cast<uint>(file_path.length()));
        WriteData(UpdateFilesList, update_file.Size);
        WriteData(UpdateFilesList, Hashing::MurmurHash2(update_file.Data, update_file.Size));
    }

    // Complete files list
    WriteData(UpdateFilesList, static_cast<short>(-1));
}

void FOServer::EntitySetValue(Entity* entity, Property* prop, void* /*cur_value*/, void* /*old_value*/)
{
    NON_CONST_METHOD_HINT(FpsTick);

    if ((entity->Id == 0u) || prop->IsTemporary()) {
        return;
    }

    const auto value = PropertiesSerializator::SavePropertyToDbValue(&entity->Props, prop, ScriptSys);

    if (entity->Type == EntityType::Location) {
        DbStorage.Update("Locations", entity->Id, prop->GetName(), value);
    }
    else if (entity->Type == EntityType::Map) {
        DbStorage.Update("Maps", entity->Id, prop->GetName(), value);
    }
    else if (entity->Type == EntityType::Npc) {
        DbStorage.Update("Critters", entity->Id, prop->GetName(), value);
    }
    else if (entity->Type == EntityType::Item) {
        DbStorage.Update("Items", entity->Id, prop->GetName(), value);
    }
    else if (entity->Type == EntityType::Custom) {
        DbStorage.Update(entity->Props.GetRegistrator()->GetClassName() + "s", entity->Id, prop->GetName(), value);
    }
    else if (entity->Type == EntityType::Client) {
        DbStorage.Update("Players", entity->Id, prop->GetName(), value);
    }
    else if (entity->Type == EntityType::Global) {
        DbStorage.Update("Globals", entity->Id, prop->GetName(), value);
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }

    if (DbHistory && !prop->IsNoHistory()) {
        const auto id = Globals->GetHistoryRecordsId();
        Globals->SetHistoryRecordsId(id + 1);

        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

        DataBase::Document doc;
        doc["Time"] = static_cast<int64>(time.count());
        doc["EntityId"] = static_cast<int>(entity->Id);
        doc["Property"] = prop->GetName();
        doc["Value"] = value;

        if (entity->Type == EntityType::Location) {
            DbHistory.Insert("LocationsHistory", id, doc);
        }
        else if (entity->Type == EntityType::Map) {
            DbHistory.Insert("MapsHistory", id, doc);
        }
        else if (entity->Type == EntityType::Npc) {
            DbHistory.Insert("CrittersHistory", id, doc);
        }
        else if (entity->Type == EntityType::Item) {
            DbHistory.Insert("ItemsHistory", id, doc);
        }
        else if (entity->Type == EntityType::Custom) {
            DbHistory.Insert(entity->Props.GetRegistrator()->GetClassName() + "sHistory", id, doc);
        }
        else if (entity->Type == EntityType::Client) {
            DbHistory.Insert("PlayersHistory", id, doc);
        }
        else if (entity->Type == EntityType::Global) {
            DbHistory.Insert("GlobalsHistory", id, doc);
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    }
}

void FOServer::ProcessCritter(Critter* cr)
{
    if (cr->CanBeRemoved || cr->IsDestroyed) {
        return;
    }
    if (GameTime.IsGamePaused()) {
        return;
    }

    // Moving
    ProcessMove(cr);

    // Idle functions
    ScriptSys.CritterIdleEvent(cr);
    if (cr->GetMapId() == 0u) {
        ScriptSys.CritterGlobalMapIdleEvent(cr);
    }

    // Internal misc/drugs time events
    // One event per cycle
    /*if (cr->IsNonEmptyTE_FuncNum())
    {
        CScriptArray* te_next_time = cr->GetTE_NextTime();
        uint next_time = *(uint*)te_next_time->At(0);
        if (!next_time || GameTime.GetFullSecond() >= next_time)
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
    if (cr->IsPlayer()) {
        // Cast
        auto* cl = dynamic_cast<Client*>(cr);

        // Talk
        CrMngr.ProcessTalk(cl, false);

        // Ping client
        if (cl->IsToPing()) {
            cl->PingClient();
        }

        // Kick from game
        if (cl->IsOffline() && cl->IsAlive() && !(cl->GetTimeoutBattle() > GameTime.GetFullSecond()) && (cl->GetTimeoutRemoveFromGame() == 0u) && cl->GetOfflineTime() >= Settings.MinimumOfflineTime) {
            cl->RemoveFromGame();
        }

        // Cache look distance
        // Todo: disable look distance caching
        if (GameTime.GameTick() >= cl->CacheValuesNextTick) {
            cl->LookCacheValue = cl->GetLookDistance();
            cl->CacheValuesNextTick = GameTime.GameTick() + 3000;
        }
    }
}

auto FOServer::Act_Move(Critter* cr, ushort hx, ushort hy, uint move_params) -> bool
{
    const auto map_id = cr->GetMapId();
    if (map_id == 0u) {
        return false;
    }

    // Check run/walk
    auto is_run = IsBitSet(move_params, MOVE_PARAM_RUN);
    if (is_run && cr->GetIsNoRun()) {
        // Switch to walk
        move_params ^= MOVE_PARAM_RUN;
        is_run = false;
    }
    if (!is_run && cr->GetIsNoWalk()) {
        return false;
    }

    // Check
    auto* map = MapMngr.GetMap(map_id);
    if ((map == nullptr) || map_id != cr->GetMapId() || hx >= map->GetWidth() || hy >= map->GetHeight()) {
        return false;
    }

    // Check passed
    const auto fx = cr->GetHexX();
    const auto fy = cr->GetHexY();
    const auto dir = GeomHelper.GetNearDir(fx, fy, hx, hy);
    const auto multihex = cr->GetMultihex();

    if (!map->IsMovePassed(hx, hy, dir, multihex)) {
        if (cr->IsPlayer()) {
            cr->Send_XY(cr);
            auto* cr_hex = map->GetHexCritter(hx, hy, false);
            if (cr_hex != nullptr) {
                cr->Send_XY(cr_hex);
            }
        }
        return false;
    }

    // Set last move type
    cr->IsRunning = is_run;

    // Process step
    const auto is_dead = cr->IsDead();
    map->UnsetFlagCritter(fx, fy, multihex, is_dead);
    cr->SetHexX(hx);
    cr->SetHexY(hy);
    map->SetFlagCritter(hx, hy, multihex, is_dead);

    // Set dir
    cr->SetDir(dir);

    if (is_run) {
        if (cr->GetIsHide()) {
            cr->SetIsHide(false);
        }
        cr->SetBreakTimeDelta(cr->GetRunTime());
    }
    else {
        cr->SetBreakTimeDelta(cr->GetWalkTime());
    }

    cr->SendA_Move(move_params);
    MapMngr.ProcessVisibleCritters(cr);
    MapMngr.ProcessVisibleItems(cr);

    // Triggers
    if (cr->GetMapId() == map->GetId()) {
        VerifyTrigger(map, cr, fx, fy, hx, hy, dir);
    }

    return true;
}

void FOServer::VerifyTrigger(Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir)
{
    if (map->IsHexStaticTrigger(from_hx, from_hy)) {
        for (auto* item : map->GetStaticItemTriggers(from_hx, from_hy)) {
            if (item->TriggerScriptFunc) {
                item->TriggerScriptFunc(cr, item, false, dir);
            }

            ScriptSys.StaticItemWalkEvent(item, cr, false, dir);
        }
    }

    if (map->IsHexStaticTrigger(to_hx, to_hy)) {
        for (auto* item : map->GetStaticItemTriggers(to_hx, to_hy)) {
            if (item->TriggerScriptFunc) {
                item->TriggerScriptFunc(cr, item, true, dir);
            }

            ScriptSys.StaticItemWalkEvent(item, cr, true, dir);
        }
    }

    if (map->IsHexTrigger(from_hx, from_hy)) {
        for (auto* item : map->GetItemsTrigger(from_hx, from_hy)) {
            ScriptSys.ItemWalkEvent(item, cr, false, dir);
        }
    }

    if (map->IsHexTrigger(to_hx, to_hy)) {
        for (auto* item : map->GetItemsTrigger(to_hx, to_hy)) {
            ScriptSys.ItemWalkEvent(item, cr, true, dir);
        }
    }
}

void FOServer::Process_Update(Client* cl)
{
    // Net protocol
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;
    const auto outdated = (proto_ver != static_cast<ushort>(FO_VERSION));

    // Begin data encrypting
    uint encrypt_key = 0;
    cl->Connection->Bin >> encrypt_key;
    cl->Connection->Bin.SetEncryptKey(encrypt_key + 521);
    cl->Connection->Bout.SetEncryptKey(encrypt_key + 3491);

    // Send update files list
    uint msg_len = sizeof(NETMSG_UPDATE_FILES_LIST) + sizeof(msg_len) + sizeof(bool) + sizeof(uint) + static_cast<uint>(UpdateFilesList.size());

    // With global properties
    vector<uchar*>* global_vars_data = nullptr;
    vector<uint>* global_vars_data_sizes = nullptr;
    if (!outdated) {
        msg_len += sizeof(ushort) + Globals->Props.StoreData(false, &global_vars_data, &global_vars_data_sizes);
    }

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_UPDATE_FILES_LIST;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << outdated;
    cl->Connection->Bout << static_cast<uint>(UpdateFilesList.size());
    if (!UpdateFilesList.empty()) {
        cl->Connection->Bout.Push(&UpdateFilesList[0], static_cast<uint>(UpdateFilesList.size()));
    }
    if (!outdated)
        NET_WRITE_PROPERTIES(cl->Connection->Bout, global_vars_data, global_vars_data_sizes)
    CLIENT_OUTPUT_END(cl);
}

void FOServer::Process_UpdateFile(Client* cl)
{
    uint file_index = 0;
    cl->Connection->Bin >> file_index;

    if (file_index >= static_cast<uint>(UpdateFiles.size())) {
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
    if (cl->UpdateFileIndex == -1) {
        WriteLog("Wrong update call, client ip '{}'.\n", cl->GetIpStr());
        cl->Disconnect();
        return;
    }

    auto& update_file = UpdateFiles[cl->UpdateFileIndex];
    const auto offset = cl->UpdateFilePortion * FILE_UPDATE_PORTION;

    if (offset + FILE_UPDATE_PORTION < update_file.Size) {
        cl->UpdateFilePortion++;
    }
    else {
        cl->UpdateFileIndex = -1;
    }

    if (cl->IsSendDisabled() || cl->IsOffline()) {
        return;
    }

    uchar data[FILE_UPDATE_PORTION];
    const auto remaining_size = update_file.Size - offset;
    if (remaining_size >= sizeof(data)) {
        std::memcpy(data, &update_file.Data[offset], sizeof(data));
    }
    else {
        std::memcpy(data, &update_file.Data[offset], remaining_size);
        std::memset(&data[remaining_size], 0, sizeof(data) - remaining_size);
    }

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_UPDATE_FILE_DATA;
    cl->Connection->Bout.Push(data, sizeof(data));
    CLIENT_OUTPUT_END(cl);
}

void FOServer::Process_CreateClient(Client* cl)
{
    // Read message
    uint msg_len = 0;
    cl->Connection->Bin >> msg_len;

    // Protocol version
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;

    // Begin data encrypting
    cl->Connection->Bin.SetEncryptKey(1234567890);
    cl->Connection->Bout.SetEncryptKey(1234567890);

    // Check protocol
    if (proto_ver != static_cast<ushort>(FO_VERSION)) {
        cl->Send_CustomMessage(NETMSG_WRONG_NET_PROTO);
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK(BannedLocker);

        const auto ip = cl->GetIp();
        auto* ban = GetBanByIp(ip);
        if (ban != nullptr) {
            cl->Send_TextMsg(cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME);
            // cl->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,GetBanLexems(*ban));
            cl->Send_TextMsgLex(cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
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
    CHECK_IN_BUFF_ERROR_EXT(cl, cl->Send_TextMsg(cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME), return )

    // Check data
    if (!_str(cl->Name).isValidUtf8() || cl->Name.find('*') != string::npos) {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    const auto name_len_utf8 = _str(cl->Name).lengthUtf8();
    if (name_len_utf8 < MIN_NAME || name_len_utf8 < Settings.MinNameLength || name_len_utf8 > MAX_NAME || name_len_utf8 > Settings.MaxNameLength) {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check for exist
    const auto id = MAKE_CLIENT_ID(name);
    RUNTIME_ASSERT(IS_CLIENT_ID(id));
    if (!DbStorage.Get("Players", id).empty()) {
        cl->Send_TextMsg(cl, STR_NET_ACCOUNT_ALREADY, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check brute force registration
    if (Settings.RegistrationTimeout != 0u) {
        SCOPE_LOCK(RegIpLocker);

        auto ip = cl->GetIp();
        const auto reg_tick = Settings.RegistrationTimeout * 1000;
        auto it = RegIp.find(ip);
        if (it != RegIp.end()) {
            auto& last_reg = it->second;
            const auto tick = GameTime.FrameTick();
            if (tick - last_reg < reg_tick) {
                cl->Send_TextMsg(cl, STR_NET_REGISTRATION_IP_WAIT, SAY_NETMSG, TEXTMSG_GAME);
                cl->Send_TextMsgLex(cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", (reg_tick - (tick - last_reg)) / 60000 + 1).c_str());
                cl->Disconnect();
                return;
            }
            last_reg = tick;
        }
        else {
            RegIp.insert(std::make_pair(ip, GameTime.FrameTick()));
        }
    }

#ifndef FO_SINGLEPLAYER
    uint disallow_msg_num = 0;
    uint disallow_str_num = 0;
    string lexems;
    const auto allow = ScriptSys.PlayerRegistrationEvent(cl->GetIp(), cl->Name, disallow_msg_num, disallow_str_num, lexems);
    if (!allow) {
        if (disallow_msg_num < TEXTMSG_COUNT && (disallow_str_num != 0u)) {
            cl->Send_TextMsgLex(cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, lexems.c_str());
        }
        else {
            cl->Send_TextMsg(cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        }
        cl->Disconnect();
        return;
    }
#endif

    // Register
    cl->SetId(id);
    DbStorage.Insert("Players", id, {{"_Proto", string("Player")}});
    cl->SetPassword(password);
    cl->SetWorldX((Settings.GlobalMapWidth * Settings.GlobalMapZoneLength) / 2);
    cl->SetWorldY((Settings.GlobalMapHeight * Settings.GlobalMapZoneLength) / 2);
    cl->SetCond(COND_ALIVE);

    const auto reg_ip = cl->GetIp();
    cl->SetConnectionIp({reg_ip});
    const auto reg_port = cl->GetPort();
    cl->SetConnectionPort({reg_port});

    // Assign base access
    cl->Access = ACCESS_DEFAULT;

    // Notify
    cl->Send_TextMsg(cl, STR_NET_REG_SUCCESS, SAY_NETMSG, TEXTMSG_GAME);

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_REGISTER_SUCCESS;
    CLIENT_OUTPUT_END(cl);

    cl->Disconnect();

    cl->AddRef();
    EntityMngr.RegisterEntity(cl);
    const auto can = MapMngr.CanAddCrToMap(cl, nullptr, 0, 0, 0);
    RUNTIME_ASSERT(can);
    MapMngr.AddCrToMap(cl, nullptr, 0, 0, 0, 0);

    if (ScriptSys.CritterInitEvent(cl, true)) {
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
    if (proto_ver != static_cast<ushort>(FO_VERSION)) {
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
    uint msg_language = 0;
    cl->Connection->Bin >> msg_language;

    CHECK_IN_BUFF_ERROR_EXT(cl, cl->Send_TextMsg(cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME), return )

    // Language
    const auto it_l = std::find(LangPacks.begin(), LangPacks.end(), msg_language);
    if (it_l != LangPacks.end()) {
        cl->LanguageMsg = msg_language;
    }
    else {
        cl->LanguageMsg = (*LangPacks.begin()).NameCode;
    }

    // If only cache checking than disconnect
    if (name[0] == 0) {
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK(BannedLocker);

        const auto ip = cl->GetIp();
        auto* ban = GetBanByIp(ip);
        if (ban != nullptr) {
            cl->Send_TextMsg(cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME);
            if (_str(ban->ClientName).compareIgnoreCaseUtf8(cl->Name)) {
                cl->Send_TextMsgLex(cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, GetBanLexems(*ban).c_str());
            }
            cl->Send_TextMsgLex(cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
            cl->Disconnect();
            return;
        }
    }

    // Check login/password
    const auto name_len_utf8 = _str(cl->Name).lengthUtf8();
    if (name_len_utf8 < MIN_NAME || name_len_utf8 < Settings.MinNameLength || name_len_utf8 > MAX_NAME || name_len_utf8 > Settings.MaxNameLength) {
        cl->Send_TextMsg(cl, STR_NET_WRONG_LOGIN, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    if (!_str(cl->Name).isValidUtf8() || cl->Name.find('*') != string::npos) {
        cl->Send_TextMsg(cl, STR_NET_WRONG_DATA, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check password
    const auto id = MAKE_CLIENT_ID(cl->Name);
    auto doc = DbStorage.Get("Players", id);
    if ((doc.count("Password") == 0u) || doc["Password"].index() != DataBase::STRING_VALUE || !Str::Compare(password, std::get<string>(doc["Password"]).c_str())) {
        cl->Send_TextMsg(cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        cl->Disconnect();
        return;
    }

    // Check for ban by name
    {
        SCOPE_LOCK(BannedLocker);

        auto* ban = GetBanByName(cl->Name.c_str());
        if (ban != nullptr) {
            cl->Send_TextMsg(cl, STR_NET_BANNED, SAY_NETMSG, TEXTMSG_GAME);
            cl->Send_TextMsgLex(cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, GetBanLexems(*ban).c_str());
            cl->Send_TextMsgLex(cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", GetBanTime(*ban)).c_str());
            cl->Disconnect();
            return;
        }
    }

#ifndef FO_SINGLEPLAYER
    // Request script
    uint disallow_msg_num = 0;
    uint disallow_str_num = 0;
    string lexems;
    const auto allow = !ScriptSys.PlayerLoginEvent(cl->GetIp(), cl->Name, id, disallow_msg_num, disallow_str_num, lexems);
    if (!allow) {
        if (disallow_msg_num < TEXTMSG_COUNT && (disallow_str_num != 0u)) {
            cl->Send_TextMsgLex(cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, lexems.c_str());
        }
        else {
            cl->Send_TextMsg(cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        }
        cl->Disconnect();
        return;
    }
#endif

    // Find in players in game
    auto* cl_old = CrMngr.GetPlayer(id);
    RUNTIME_ASSERT(cl != cl_old);

    // Avatar in game
    if (cl_old != nullptr) {
        {
            SCOPE_LOCK(ConnectedClientsLocker);

            // Disconnect current connection
            if (!cl_old->Connection->IsDisconnected) {
                cl_old->Disconnect();
            }

            // Swap data
            std::swap(cl_old->Connection, cl->Connection);
            cl_old->State = ClientState::Connected;
            cl->State = ClientState::None;
            cl_old->LastActivityTime = 0;
            UnsetBit(cl_old->Flags, FCRIT_DISCONNECT);
            SetBit(cl->Flags, FCRIT_DISCONNECT);
            cl->IsDestroyed = true;
            cl_old->IsDestroyed = false;
            ScriptSys.RemoveEntity(cl);

            // Change in list
            cl_old->AddRef();
            const auto it = std::find(ConnectedClients.begin(), ConnectedClients.end(), cl);
            RUNTIME_ASSERT(it != ConnectedClients.end());
            *it = cl_old;
            cl->Release();
            cl = cl_old;
        }

        cl->SendA_Action(ACTION_CONNECT, 0, nullptr);
    }
    // Avatar not in game
    else {
        cl->SetId(id);

        // Data
        if (!PropertiesSerializator::LoadFromDbDocument(&cl->Props, doc, ScriptSys)) {
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

        // Find and set items
        for (auto* item : EntityMngr.FindCritterItems(cl->GetId())) {
            cl->SetItem(item);

            if (item->GetIsRadio()) {
                ItemMngr.RegisterRadio(item);
            }
        }

        // Initially add on global map
        cl->SetMapId(0);
        const auto ref_gm_leader_id = cl->GetRefGlobalMapLeaderId();
        const auto ref_gm_trip_id = cl->GetRefGlobalMapTripId();
        const auto can = MapMngr.CanAddCrToMap(cl, nullptr, 0, 0, 0);
        RUNTIME_ASSERT(can);
        MapMngr.AddCrToMap(cl, nullptr, 0, 0, 0, 0);
        cl->SetRefGlobalMapLeaderId(ref_gm_leader_id);
        cl->SetRefGlobalMapLeaderId(ref_gm_trip_id);

        // Initial scripts
        if (ScriptSys.CritterInitEvent(cl, false)) {
            // Todo: remove from game
        }

        cl->SetScript(nullptr, false);

        // Restore data sending
        cl->DisableSend--;
    }

    // Connection info
    const auto ip = cl->GetIp();
    const auto port = cl->GetPort();
    auto conn_ip = cl->GetConnectionIp();
    auto conn_port = cl->GetConnectionPort();
    RUNTIME_ASSERT(conn_ip.size() == conn_port.size());
    auto ip_found = false;
    for (uint i = 0; i < conn_ip.size(); i++) {
        if (conn_ip[i] == ip) {
            if (i < conn_ip.size() - 1) {
                conn_ip.erase(conn_ip.begin() + i);
                conn_ip.push_back(ip);
                cl->SetConnectionIp(conn_ip);
                conn_port.erase(conn_port.begin() + i);
                conn_port.push_back(port);
                cl->SetConnectionPort(conn_port);
            }
            else if (conn_port.back() != port) {
                conn_port.back() = port;
                cl->SetConnectionPort(conn_port);
            }
            ip_found = true;
            break;
        }
    }
    if (!ip_found) {
        conn_ip.push_back(ip);
        cl->SetConnectionIp(conn_ip);
        conn_port.push_back(port);
        cl->SetConnectionPort(conn_port);
    }

    // Login ok
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) * 2;
    const uint bin_seed = GenericUtils::Random(100000, 2000000000);
    const uint bout_seed = GenericUtils::Random(100000, 2000000000);
    vector<uchar*>* global_vars_data = nullptr;
    vector<uint>* global_vars_data_sizes = nullptr;
    const auto whole_data_size = Globals->Props.StoreData(false, &global_vars_data, &global_vars_data_sizes);
    msg_len += sizeof(ushort) + whole_data_size;

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << NETMSG_LOGIN_SUCCESS;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << bin_seed;
    cl->Connection->Bout << bout_seed;
    NET_WRITE_PROPERTIES(cl->Connection->Bout, global_vars_data, global_vars_data_sizes)
    CLIENT_OUTPUT_END(cl);

    cl->Connection->Bin.SetEncryptKey(bin_seed);
    cl->Connection->Bout.SetEncryptKey(bout_seed);

    cl->Send_LoadMap(nullptr, MapMngr);
}

void FOServer::Process_ParseToGame(Client* cl)
{
    if ((cl->GetId() == 0u) || (CrMngr.GetPlayer(cl->GetId()) == nullptr)) {
        return;
    }
    cl->SetBreakTime(Settings.Breaktime);

    cl->State = ClientState::Playing;
    cl->PingOk(PING_CLIENT_LIFE_TIME * 5);

    if (cl->ViewMapId != 0u) {
        auto* map = MapMngr.GetMap(cl->ViewMapId);
        cl->ViewMapId = 0;
        if (map != nullptr) {
            MapMngr.ViewMap(cl, map, cl->ViewMapLook, cl->ViewMapHx, cl->ViewMapHy, cl->ViewMapDir);
            cl->Send_ViewMap();
            return;
        }
    }

    // Parse
    auto* map = MapMngr.GetMap(cl->GetMapId());
    cl->Send_GameInfo(map);

    // Parse to global
    if (cl->GetMapId() == 0u) {
        RUNTIME_ASSERT(cl->GlobalMapGroup);

        cl->Send_GlobalInfo(GM_INFO_ALL, MapMngr);
        for (auto* cr : *cl->GlobalMapGroup) {
            if (cr != cl) {
                cr->Send_CustomCommand(cl, OTHER_FLAGS, cl->Flags);
            }
        }

        cl->Send_AllAutomapsInfo(MapMngr);

        if (cl->Talk.Type != TalkType::None) {
            CrMngr.ProcessTalk(cl, true);
            cl->Send_Talk();
        }
    }
    else {
        if (map == nullptr) {
            WriteLog("Map not found, client '{}'.\n", cl->GetName());
            cl->Disconnect();
            return;
        }

        // Parse to local
        SetBit(cl->Flags, FCRIT_CHOSEN);
        cl->Send_AddCritter(cl);
        UnsetBit(cl->Flags, FCRIT_CHOSEN);

        // Send all data
        cl->Send_AddAllItems();
        cl->Send_AllAutomapsInfo(MapMngr);

        // Send current critters
        auto critters = cl->VisCrSelf;
        for (auto& critter : critters) {
            cl->Send_AddCritter(critter);
        }

        // Send current items on map
        auto items = cl->VisItem;
        for (auto item_id : items) {
            auto* item = ItemMngr.GetItem(item_id);
            if (item != nullptr) {
                cl->Send_AddItemOnMap(item);
            }
        }

        // Check active talk
        if (cl->Talk.Type != TalkType::None) {
            CrMngr.ProcessTalk(cl, true);
            cl->Send_Talk();
        }
    }

    // Notify about end of parsing
    cl->Send_CustomMessage(NETMSG_END_PARSE_TO_GAME);
}

void FOServer::Process_GiveMap(Client* cl)
{
    bool automap = 0;
    hash map_pid = 0;
    uint loc_id = 0;
    hash hash_tiles = 0;
    hash hash_scen = 0;

    cl->Connection->Bin >> automap;
    cl->Connection->Bin >> map_pid;
    cl->Connection->Bin >> loc_id;
    cl->Connection->Bin >> hash_tiles;
    cl->Connection->Bin >> hash_scen;
    CHECK_IN_BUFF_ERROR(cl)

    const auto tick = GameTime.FrameTick();
    if (tick - cl->LastSendedMapTick < Settings.Breaktime * 3) {
        cl->SetBreakTime(Settings.Breaktime * 3);
    }
    else {
        cl->SetBreakTime(Settings.Breaktime);
    }
    cl->LastSendedMapTick = tick;

    const auto* proto_map = ProtoMngr.GetProtoMap(map_pid);
    if (proto_map == nullptr) {
        WriteLog("Map prototype not found, client '{}'.\n", cl->GetName());
        cl->Disconnect();
        return;
    }

    if (automap) {
        if (!MapMngr.CheckKnownLocById(cl, loc_id)) {
            WriteLog("Request for loading unknown automap, client '{}'.\n", cl->GetName());
            return;
        }

        auto* loc = MapMngr.GetLocation(loc_id);
        if (loc == nullptr) {
            WriteLog("Request for loading incorrect automap, client '{}'.\n", cl->GetName());
            return;
        }

        auto found = false;
        auto automaps = (loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : vector<hash>());
        if (!automaps.empty()) {
            for (size_t i = 0; i < automaps.size() && !found; i++) {
                if (automaps[i] == map_pid) {
                    found = true;
                }
            }
        }
        if (!found) {
            WriteLog("Request for loading incorrect automap, client '{}'.\n", cl->GetName());
            return;
        }
    }
    else {
        auto* map = MapMngr.GetMap(cl->GetMapId());
        if (((map == nullptr) || map_pid != map->GetProtoId()) && map_pid != cl->ViewMapPid) {
            WriteLog("Request for loading incorrect map, client '{}'.\n", cl->GetName());
            return;
        }
    }

    const auto* static_map = MapMngr.FindStaticMap(proto_map);
    RUNTIME_ASSERT(static_map);

    Send_MapData(cl, proto_map, static_map, static_map->HashTiles != hash_tiles, static_map->HashScen != hash_scen);

    if (!automap) {
        Map* map = nullptr;
        if (cl->ViewMapId != 0u) {
            map = MapMngr.GetMap(cl->ViewMapId);
        }
        cl->Send_LoadMap(map, MapMngr);
    }
}

void FOServer::Send_MapData(Client* cl, const ProtoMap* pmap, const StaticMap* static_map, bool send_tiles, bool send_scenery)
{
    auto msg = NETMSG_MAP;
    auto map_pid = pmap->ProtoId;
    auto maxhx = pmap->GetWidth();
    auto maxhy = pmap->GetHeight();
    uint msg_len = sizeof(msg) + sizeof(msg_len) + sizeof(map_pid) + sizeof(maxhx) + sizeof(maxhy) + sizeof(bool) * 2;

    if (send_tiles) {
        msg_len += sizeof(uint) + static_cast<uint>(static_map->Tiles.size()) * sizeof(MapTile);
    }
    if (send_scenery) {
        msg_len += sizeof(uint) + static_cast<uint>(static_map->SceneryData.size());
    }

    CLIENT_OUTPUT_BEGIN(cl);
    cl->Connection->Bout << msg;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << map_pid;
    cl->Connection->Bout << maxhx;
    cl->Connection->Bout << maxhy;
    cl->Connection->Bout << send_tiles;
    cl->Connection->Bout << send_scenery;
    if (send_tiles) {
        cl->Connection->Bout << static_cast<uint>(static_map->Tiles.size() * sizeof(MapTile));
        if (!static_map->Tiles.empty()) {
            cl->Connection->Bout.Push(&static_map->Tiles[0], static_cast<uint>(static_map->Tiles.size()) * sizeof(MapTile));
        }
    }
    if (send_scenery) {
        cl->Connection->Bout << static_cast<uint>(static_map->SceneryData.size());
        if (!static_map->SceneryData.empty()) {
            cl->Connection->Bout.Push(&static_map->SceneryData[0], static_cast<uint>(static_map->SceneryData.size()));
        }
    }
    CLIENT_OUTPUT_END(cl);
}

void FOServer::Process_Move(Client* cl)
{
    uint move_params = 0;
    ushort hx = 0;
    ushort hy = 0;

    cl->Connection->Bin >> move_params;
    cl->Connection->Bin >> hx;
    cl->Connection->Bin >> hy;
    CHECK_IN_BUFF_ERROR(cl)

    if (cl->GetMapId() == 0u) {
        return;
    }

    // The player informs that has stopped
    if (IsBitSet(move_params, MOVE_PARAM_STEP_DISALLOW)) {
        // cl->Send_XY(cl);
        cl->SendA_XY();
        return;
    }

    // Check running availability
    auto is_run = IsBitSet(move_params, MOVE_PARAM_RUN);
    if (is_run) {
        if (cl->GetIsNoRun() || (Settings.RunOnCombat ? false : cl->GetTimeoutBattle() > GameTime.GetFullSecond()) || (Settings.RunOnTransfer ? false : cl->GetTimeoutTransfer() > GameTime.GetFullSecond())) {
            // Switch to walk
            move_params ^= MOVE_PARAM_RUN;
            is_run = false;
        }
    }

    // Check walking availability
    if (!is_run) {
        if (cl->GetIsNoWalk()) {
            cl->Send_XY(cl);
            return;
        }
    }

    // Check dist
    if (!GeomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, 1)) {
        cl->Send_XY(cl);
        return;
    }

    // Try move
    Act_Move(cl, hx, hy, move_params);
}

void FOServer::Process_Dir(Client* cl)
{
    uchar dir = 0;
    cl->Connection->Bin >> dir;
    CHECK_IN_BUFF_ERROR(cl)

    if ((cl->GetMapId() == 0u) || dir >= Settings.MapDirCount || cl->GetDir() == dir || cl->IsTalking()) {
        if (cl->GetDir() != dir) {
            cl->Send_Dir(cl);
        }
        return;
    }

    cl->SetDir(dir);
    cl->SendA_Dir();
}

void FOServer::Process_Ping(Client* cl)
{
    uchar ping = 0;

    cl->Connection->Bin >> ping;
    CHECK_IN_BUFF_ERROR(cl)

    if (ping == PING_CLIENT) {
        cl->PingOk(PING_CLIENT_LIFE_TIME);
        return;
    }
    if (ping == PING_PING || ping == PING_WAIT) {
        // Valid pings
    }
    else {
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
    auto type = NetProperty::None;
    uint cr_id = 0;
    uint item_id = 0;
    ushort property_index = 0;

    if (data_size == 0) {
        cl->Connection->Bin >> msg_len;
    }

    char type_ = 0;
    cl->Connection->Bin >> type_;
    type = static_cast<NetProperty::Type>(type_);

    uint additional_args = 0;
    switch (type) {
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

    CHECK_IN_BUFF_ERROR(cl)

    vector<uchar> data;
    if (data_size != 0) {
        data.resize(data_size);
        cl->Connection->Bin.Pop(&data[0], data_size);
    }
    else {
        const uint len = msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(ushort);
        // Todo: control max size explicitly, add option to property registration
        if (len > 0xFFFF) {
            // For now 64Kb for all
            return;
        }
        data.resize(len);
        cl->Connection->Bin.Pop(&data[0], len);
    }

    CHECK_IN_BUFF_ERROR(cl)

    auto is_public = false;
    Property* prop = nullptr;
    Entity* entity = nullptr;
    switch (type) {
    case NetProperty::Global:
        is_public = true;
        prop = GlobalVars::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = Globals;
        }
        break;
    case NetProperty::Critter:
        is_public = true;
        prop = Critter::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = CrMngr.GetCritter(cr_id);
        }
        break;
    case NetProperty::Chosen:
        prop = Critter::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = cl;
        }
        break;
    case NetProperty::MapItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = ItemMngr.GetItem(item_id);
        }
        break;
    case NetProperty::CritterItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            auto* cr = CrMngr.GetCritter(cr_id);
            if (cr != nullptr) {
                entity = cr->GetItem(item_id, true);
            }
        }
        break;
    case NetProperty::ChosenItem:
        prop = Item::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = cl->GetItem(item_id, true);
        }
        break;
    case NetProperty::Map:
        is_public = true;
        prop = Map::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            entity = MapMngr.GetMap(cl->GetMapId());
        }
        break;
    case NetProperty::Location:
        is_public = true;
        prop = Location::PropertiesRegistrator->Get(property_index);
        if (prop != nullptr) {
            auto* map = MapMngr.GetMap(cl->GetMapId());
            if (map != nullptr) {
                entity = map->GetLocation();
            }
        }
        break;
    default:
        break;
    }
    if ((prop == nullptr) || (entity == nullptr)) {
        return;
    }

    const auto access = prop->GetAccess();
    if (is_public && ((access & Property::PublicMask) == 0)) {
        return;
    }
    if (!is_public && ((access & (Property::ProtectedMask | Property::PublicMask)) == 0)) {
        return;
    }
    if ((access & Property::ModifiableMask) == 0) {
        return;
    }
    if (is_public && access != Property::PublicFullModifiable) {
        return;
    }
    if (prop->IsPOD() && data_size != prop->GetBaseSize()) {
        return;
    }
    if (!prop->IsPOD() && data_size != 0) {
        return;
    }

    // Todo: disable send changing field by client to this client
    entity->Props.SetValueFromData(prop, data.data(), static_cast<uint>(data.size()));
}

void FOServer::OnSendGlobalValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0) {
        for (auto* cl : CrMngr.GetClients(false)) {
            cl->Send_Property(NetProperty::Global, prop, Globals);
        }
    }
}

void FOServer::OnSendCritterValue(Entity* entity, Property* prop)
{
    auto* cr = dynamic_cast<Critter*>(entity);

    const auto is_public = (prop->GetAccess() & Property::PublicMask) != 0;
    const auto is_protected = (prop->GetAccess() & Property::ProtectedMask) != 0;

    if (is_public || is_protected) {
        cr->Send_Property(NetProperty::Chosen, prop, cr);
    }
    if (is_public) {
        cr->SendA_Property(NetProperty::Critter, prop, cr);
    }
}

void FOServer::OnSendMapValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0) {
        auto* map = dynamic_cast<Map*>(entity);
        map->SendProperty(NetProperty::Map, prop, map);
    }
}

void FOServer::OnSendLocationValue(Entity* entity, Property* prop)
{
    if ((prop->GetAccess() & Property::PublicMask) != 0) {
        auto* loc = dynamic_cast<Location*>(entity);
        for (auto* map : loc->GetMaps()) {
            map->SendProperty(NetProperty::Location, prop, loc);
        }
    }
}

static constexpr auto MOVING_SUCCESS = 1;
static constexpr auto MOVING_TARGET_NOT_FOUND = 2;
static constexpr auto MOVING_CANT_MOVE = 3;
static constexpr auto MOVING_GAG_CRITTER = 4;
static constexpr auto MOVING_GAG_ITEM = 5;
static constexpr auto MOVING_FIND_PATH_ERROR = 6;
static constexpr auto MOVING_HEX_TOO_FAR = 7;
static constexpr auto MOVING_HEX_BUSY = 8;
static constexpr auto MOVING_HEX_BUSY_RING = 9;
static constexpr auto MOVING_DEADLOCK = 10;
static constexpr auto MOVING_TRACE_FAIL = 11;

void FOServer::ProcessMove(Critter* cr)
{
    if (cr->Moving.State != 0) {
        return;
    }
    if (cr->IsBusy() || cr->IsWait()) {
        return;
    }

    // Check for path recalculation
    if (!cr->Moving.Steps.empty() && cr->Moving.TargId != 0u) {
        auto* targ = cr->GetCrSelf(cr->Moving.TargId);
        if (targ == nullptr || ((cr->Moving.HexX != 0u || cr->Moving.HexY != 0u) && !GeomHelper.CheckDist(targ->GetHexX(), targ->GetHexY(), cr->Moving.HexX, cr->Moving.HexY, 0))) {
            cr->Moving.Steps.clear();
        }
    }

    // Find path if not exist
    if (cr->Moving.Steps.empty()) {
        ushort hx = 0;
        ushort hy = 0;
        uint cut = 0;
        uint trace = 0;
        Critter* trace_cr = nullptr;
        auto check_cr = false;

        if (cr->Moving.TargId != 0u) {
            auto* targ = cr->GetCrSelf(cr->Moving.TargId);
            if (targ == nullptr) {
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
        else {
            hx = cr->Moving.HexX;
            hy = cr->Moving.HexY;
            cut = cr->Moving.Cut;
            trace = 0;
            trace_cr = nullptr;
        }

        FindPathInput input;
        input.MapId = cr->GetMapId();
        input.FromCritter = cr;
        input.FromX = cr->GetHexX();
        input.FromY = cr->GetHexY();
        input.ToX = hx;
        input.ToY = hy;
        input.IsRun = cr->Moving.IsRun;
        input.Multihex = cr->GetMultihex();
        input.Cut = cut;
        input.Trace = trace;
        input.TraceCr = trace_cr;
        input.CheckCrit = true;
        input.CheckGagItems = true;

        if (input.IsRun && cr->GetIsNoRun()) {
            input.IsRun = false;
        }
        if (!input.IsRun && cr->GetIsNoWalk()) {
            cr->Moving.State = MOVING_CANT_MOVE;
            return;
        }

        auto output = MapMngr.FindPath(input);
        if (output.GagCritter != nullptr) {
            cr->Moving.State = MOVING_GAG_CRITTER;
            cr->Moving.GagEntityId = output.GagCritter->GetId();
            return;
        }

        if (output.GagItem != nullptr) {
            cr->Moving.State = MOVING_GAG_ITEM;
            cr->Moving.GagEntityId = output.GagItem->GetId();
            return;
        }

        // Failed
        if (output.Result != FindPathResult::Ok) {
            if (output.Result == FindPathResult::AlreadyHere) {
                cr->Moving.State = MOVING_SUCCESS;
                return;
            }

            int reason;
            switch (output.Result) {
            case FindPathResult::MapNotFound:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FindPathResult::TooFar:
                reason = MOVING_HEX_TOO_FAR;
                break;
            case FindPathResult::InternalError:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FindPathResult::InvalidHexes:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FindPathResult::TraceTargetNullptr:
                reason = MOVING_FIND_PATH_ERROR;
                break;
            case FindPathResult::HexBusy:
                reason = MOVING_HEX_BUSY;
                break;
            case FindPathResult::HexBusyRing:
                reason = MOVING_HEX_BUSY_RING;
                break;
            case FindPathResult::DeadLock:
                reason = MOVING_DEADLOCK;
                break;
            case FindPathResult::TraceFailed:
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
        cr->Moving.Steps = std::move(output.Steps);
        cr->Moving.Iter = 0;
        cr->SetWait(0);
    }

    // Get path and move
    const auto& steps = cr->Moving.Steps;
    if (!cr->Moving.Steps.empty() && cr->Moving.Iter < steps.size()) {
        const auto& ps = steps[cr->Moving.Iter];
        if (!GeomHelper.CheckDist(cr->GetHexX(), cr->GetHexY(), ps.HexX, ps.HexY, 1) || !Act_Move(cr, ps.HexX, ps.HexY, ps.MoveParams)) {
            // Error
            cr->Moving.Steps.clear();
            cr->SendA_XY();
        }
        else {
            // Next
            cr->Moving.Iter++;
        }
    }
    else {
        // Done
        cr->Moving.State = MOVING_SUCCESS;
    }
}

auto FOServer::Dialog_Compile(Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg) -> bool
{
    if (base_dlg.Id < 2) {
        WriteLog("Wrong dialog id {}.\n", base_dlg.Id);
        return false;
    }

    compiled_dlg = base_dlg;

    for (auto it_a = compiled_dlg.Answers.begin(); it_a != compiled_dlg.Answers.end();) {
        if (!Dialog_CheckDemand(npc, cl, *it_a, false)) {
            it_a = compiled_dlg.Answers.erase(it_a);
        }
        else {
            it_a++;
        }
    }

    if (!Settings.NoAnswerShuffle && !compiled_dlg.NoShuffle) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 rnd(rd());
        std::shuffle(compiled_dlg.Answers.begin(), compiled_dlg.Answers.end(), rnd);
    }

    return true;
}

auto FOServer::Dialog_CheckDemand(Npc* npc, Client* cl, DialogAnswer& answer, bool recheck) -> bool
{
    if (answer.Demands.empty()) {
        return true;
    }

    Critter* master = nullptr;
    Critter* slave = nullptr;

    for (auto it = answer.Demands.begin(), end = answer.Demands.end(); it != end; ++it) {
        auto& demand = *it;
        if (recheck && demand.NoRecheck) {
            continue;
        }

        switch (demand.Who) {
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

        if (master == nullptr) {
            continue;
        }

        const auto index = demand.ParamId;
        switch (demand.Type) {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_CRITTER_DICT:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP: {
            Entity* entity = nullptr;
            PropertyRegistrator* prop_registrator = nullptr;
            if (demand.Type == DR_PROP_GLOBAL) {
                entity = master;
                prop_registrator = GlobalVars::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_CRITTER) {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_CRITTER_DICT) {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_ITEM) {
                entity = master->GetItemSlot(1);
                prop_registrator = Item::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_LOCATION) {
                auto* map = MapMngr.GetMap(master->GetMapId());
                entity = (map != nullptr ? map->GetLocation() : nullptr);
                prop_registrator = Location::PropertiesRegistrator;
            }
            else if (demand.Type == DR_PROP_MAP) {
                entity = MapMngr.GetMap(master->GetMapId());
                prop_registrator = Map::PropertiesRegistrator;
            }
            if (entity == nullptr) {
                break;
            }

            const auto prop_index = static_cast<uint>(index);
            auto* prop = prop_registrator->Get(prop_index);
            auto val = 0;
            if (demand.Type == DR_PROP_CRITTER_DICT) {
                if (slave == nullptr) {
                    break;
                }

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
            else {
                val = entity->Props.GetPODValueAsInt(prop);
            }

            switch (demand.Op) {
            case '>':
                if (val > demand.Value) {
                    continue;
                }
                break;
            case '<':
                if (val < demand.Value) {
                    continue;
                }
                break;
            case '=':
                if (val == demand.Value) {
                    continue;
                }
                break;
            case '!':
                if (val != demand.Value) {
                    continue;
                }
                break;
            case '}':
                if (val >= demand.Value) {
                    continue;
                }
                break;
            case '{':
                if (val <= demand.Value) {
                    continue;
                }
                break;
            default:
                break;
            }
        } break;
        case DR_ITEM: {
            const auto pid = static_cast<hash>(index);
            switch (demand.Op) {
            case '>':
                if (static_cast<int>(master->CountItemPid(pid)) > demand.Value) {
                    continue;
                }
                break;
            case '<':
                if (static_cast<int>(master->CountItemPid(pid)) < demand.Value) {
                    continue;
                }
                break;
            case '=':
                if (static_cast<int>(master->CountItemPid(pid)) == demand.Value) {
                    continue;
                }
                break;
            case '!':
                if (static_cast<int>(master->CountItemPid(pid)) != demand.Value) {
                    continue;
                }
                break;
            case '}':
                if (static_cast<int>(master->CountItemPid(pid)) >= demand.Value) {
                    continue;
                }
                break;
            case '{':
                if (static_cast<int>(master->CountItemPid(pid)) <= demand.Value) {
                    continue;
                }
                break;
            default:
                break;
            }
        } break;
        case DR_SCRIPT: {
            cl->Talk.Locked = true;
            if (DialogScriptDemand(demand, master, slave)) {
                cl->Talk.Locked = false;
                continue;
            }
            cl->Talk.Locked = false;
        } break;
        case DR_OR:
            return true;
        default:
            continue;
        }

        auto or_mod = false;
        for (; it != end; ++it) {
            if (it->Type == DR_OR) {
                or_mod = true;
                break;
            }
        }
        if (!or_mod) {
            return false;
        }
    }

    return true;
}

auto FOServer::Dialog_UseResult(Npc* npc, Client* cl, DialogAnswer& answer) -> uint
{
    if (answer.Results.empty()) {
        return 0;
    }

    uint force_dialog = 0;
    Critter* master = nullptr;
    Critter* slave = nullptr;

    for (auto& result : answer.Results) {
        switch (result.Who) {
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

        if (master == nullptr) {
            continue;
        }

        const auto index = result.ParamId;
        switch (result.Type) {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_CRITTER_DICT:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP: {
            Entity* entity = nullptr;
            PropertyRegistrator* prop_registrator = nullptr;
            if (result.Type == DR_PROP_GLOBAL) {
                entity = master;
                prop_registrator = GlobalVars::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_CRITTER) {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_CRITTER_DICT) {
                entity = master;
                prop_registrator = Critter::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_ITEM) {
                entity = master->GetItemSlot(1);
                prop_registrator = Item::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_LOCATION) {
                auto* map = MapMngr.GetMap(master->GetMapId());
                entity = (map != nullptr ? map->GetLocation() : nullptr);
                prop_registrator = Location::PropertiesRegistrator;
            }
            else if (result.Type == DR_PROP_MAP) {
                entity = MapMngr.GetMap(master->GetMapId());
                prop_registrator = Map::PropertiesRegistrator;
            }
            if (entity == nullptr) {
                break;
            }

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
            const auto pid = static_cast<hash>(index);
            const int cur_count = master->CountItemPid(pid);
            auto need_count = cur_count;

            switch (result.Op) {
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

            if (need_count < 0) {
                need_count = 0;
            }
            if (cur_count == need_count) {
                continue;
            }
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
    if (cl->Talk.Locked) {
        WriteLog("Dialog locked, client '{}'.\n", cl->GetName());
        return;
    }
    if (cl->Talk.Type != TalkType::None) {
        CrMngr.CloseTalk(cl);
    }

    DialogPack* dialog_pack = nullptr;
    DialogsVec* dialogs = nullptr;

    // Talk with npc
    if (npc != nullptr) {
        if (npc->GetIsNoTalk()) {
            return;
        }

        if (dlg_pack_id == 0u) {
            const auto npc_dlg_id = npc->GetDialogId();
            if (npc_dlg_id == 0u) {
                return;
            }
            dlg_pack_id = npc_dlg_id;
        }

        if (!ignore_distance) {
            if (cl->GetMapId() != npc->GetMapId()) {
                WriteLog("Different maps, npc '{}' {}, client '{}' {}.\n", npc->GetName(), npc->GetMapId(), cl->GetName(), cl->GetMapId());
                return;
            }

            auto talk_distance = npc->GetTalkDistance();
            talk_distance = (talk_distance != 0u ? talk_distance : Settings.TalkDistance) + cl->GetMultihex();
            if (!GeomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY(), talk_distance)) {
                cl->Send_XY(cl);
                cl->Send_XY(npc);
                cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
                WriteLog("Wrong distance to npc '{}', client '{}'.\n", npc->GetName(), cl->GetName());
                return;
            }

            auto* map = MapMngr.GetMap(cl->GetMapId());
            if (map == nullptr) {
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
            if (!trace.IsCritterFounded) {
                cl->Send_TextMsg(cl, STR_FINDPATH_AIMBLOCK, SAY_NETMSG, TEXTMSG_GAME);
                return;
            }
        }

        if (!npc->IsAlive()) {
            cl->Send_TextMsg(cl, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME);
            WriteLog("Npc '{}' bad condition, client '{}'.\n", npc->GetName(), cl->GetName());
            return;
        }

        if (!npc->IsFreeToTalk()) {
            cl->Send_TextMsg(cl, STR_DIALOG_MANY_TALKERS, SAY_NETMSG, TEXTMSG_GAME);
            return;
        }

        // Todo: don't remeber but need check (IsPlaneNoTalk)

        ScriptSys.CritterTalkEvent(cl, npc, true, npc->GetTalkedPlayers() + 1);

        dialog_pack = DlgMngr.GetDialog(dlg_pack_id);
        dialogs = (dialog_pack != nullptr ? &dialog_pack->Dialogs : nullptr);
        if ((dialogs == nullptr) || dialogs->empty()) {
            return;
        }

        if (!ignore_distance) {
            const int dir = GeomHelper.GetFarDir(cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY());
            npc->SetDir(GeomHelper.ReverseDir(dir));
            npc->SendA_Dir();
            cl->SetDir(dir);
            cl->SendA_Dir();
            cl->Send_Dir(cl);
        }
    }
    // Talk with hex
    else {
        if (!ignore_distance && !GeomHelper.CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, Settings.TalkDistance + cl->GetMultihex())) {
            cl->Send_XY(cl);
            cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
            WriteLog("Wrong distance to hexes, hx {}, hy {}, client '{}'.\n", hx, hy, cl->GetName());
            return;
        }

        dialog_pack = DlgMngr.GetDialog(dlg_pack_id);
        dialogs = (dialog_pack != nullptr ? &dialog_pack->Dialogs : nullptr);
        if ((dialogs == nullptr) || dialogs->empty()) {
            WriteLog("No dialogs, hx {}, hy {}, client '{}'.\n", hx, hy, cl->GetName());
            return;
        }
    }

    // Predialogue installations
    auto it_d = dialogs->begin();
    auto go_dialog = uint(-1);
    auto it_a = (*it_d).Answers.begin();
    for (; it_a != (*it_d).Answers.end(); ++it_a) {
        if (Dialog_CheckDemand(npc, cl, *it_a, false)) {
            go_dialog = (*it_a).Link;
        }
        if (go_dialog != uint(-1)) {
            break;
        }
    }

    if (go_dialog == uint(-1)) {
        return;
    }

    // Use result
    const auto force_dialog = Dialog_UseResult(npc, cl, (*it_a));
    if (force_dialog != 0u) {
        if (force_dialog == uint(-1)) {
            return;
        }
        go_dialog = force_dialog;
    }

    // Find dialog
    it_d = std::find_if(dialogs->begin(), dialogs->end(), [go_dialog](const Dialog& dlg) { return dlg.Id == go_dialog; });
    if (it_d == dialogs->end()) {
        cl->Send_TextMsg(cl, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog from link {} not found, client '{}', dialog pack {}.\n", go_dialog, cl->GetName(), dialog_pack->PackId);
        return;
    }

    // Compile
    if (!Dialog_Compile(npc, cl, *it_d, cl->Talk.CurDialog)) {
        cl->Send_TextMsg(cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog compile fail, client '{}', dialog pack {}.\n", cl->GetName(), dialog_pack->PackId);
        return;
    }

    if (npc != nullptr) {
        cl->Talk.Type = TalkType::Npc;
        cl->Talk.TalkNpc = npc->GetId();
    }
    else {
        cl->Talk.Type = TalkType::Hex;
        cl->Talk.TalkHexMap = cl->GetMapId();
        cl->Talk.TalkHexX = hx;
        cl->Talk.TalkHexY = hy;
    }

    cl->Talk.DialogPackId = dlg_pack_id;
    cl->Talk.LastDialogId = go_dialog;
    cl->Talk.StartTick = GameTime.GameTick();
    cl->Talk.TalkTime = Settings.DlgTalkMinTime;
    cl->Talk.Barter = false;
    cl->Talk.IgnoreDistance = ignore_distance;

    // Get lexems
    cl->Talk.Lexems.clear();
    if (cl->Talk.CurDialog.DlgScriptFunc) {
        cl->Talk.Locked = true;
        if (cl->Talk.CurDialog.DlgScriptFunc(cl, npc)) {
            cl->Talk.Lexems = cl->Talk.CurDialog.DlgScriptFunc.GetResult();
        }
        cl->Talk.Locked = false;
    }

    // On head text
    if (cl->Talk.CurDialog.Answers.empty()) {
        if (npc != nullptr) {
            npc->SendAA_MsgLex(npc->VisCr, cl->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cl->Talk.Lexems.c_str());
        }
        else {
            auto* map = MapMngr.GetMap(cl->GetMapId());
            if (map != nullptr) {
                map->SetTextMsg(hx, hy, 0, TEXTMSG_DLG, cl->Talk.CurDialog.TextId);
            }
        }

        CrMngr.CloseTalk(cl);
        return;
    }

    // Open dialog window
    cl->Send_Talk();
}

void FOServer::Process_Dialog(Client* cl)
{
    uchar is_npc = 0;
    uint talk_id = 0;
    uchar num_answer = 0;

    cl->Connection->Bin >> is_npc;
    cl->Connection->Bin >> talk_id;
    cl->Connection->Bin >> num_answer;

    if (((is_npc != 0u) && (cl->Talk.Type != TalkType::Npc || cl->Talk.TalkNpc != talk_id)) || ((is_npc == 0u) && (cl->Talk.Type != TalkType::Hex || cl->Talk.DialogPackId != talk_id))) {
        CrMngr.CloseTalk(cl);
        WriteLog("Invalid talk id {}, client '{}'.\n", is_npc, talk_id, cl->GetName());
        return;
    }

    if (cl->GetIsHide()) {
        cl->SetIsHide(false);
    }
    CrMngr.ProcessTalk(cl, true);

    Npc* npc = nullptr;
    DialogPack* dialog_pack = nullptr;
    DialogsVec* dialogs = nullptr;

    // Find npc
    if (is_npc != 0u) {
        npc = CrMngr.GetNpc(talk_id);
        if (npc == nullptr) {
            cl->Send_TextMsg(cl, STR_DIALOG_NPC_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
            CrMngr.CloseTalk(cl);
            WriteLog("Npc with id {} not found, client '{}'.\n", talk_id, cl->GetName());
            return;
        }
    }

    // Set dialogs
    dialog_pack = DlgMngr.GetDialog(cl->Talk.DialogPackId);
    dialogs = (dialog_pack != nullptr ? &dialog_pack->Dialogs : nullptr);
    if ((dialogs == nullptr) || dialogs->empty()) {
        CrMngr.CloseTalk(cl);
        WriteLog("No dialogs, npc '{}', client '{}'.\n", npc->GetName(), cl->GetName());
        return;
    }

    // Continue dialog
    auto* cur_dialog = &cl->Talk.CurDialog;
    const auto last_dialog = cur_dialog->Id;
    uint dlg_id = 0;
    uint force_dialog = 0;
    DialogAnswer* answer = nullptr;

    if (!cl->Talk.Barter) {
        // Barter
        if (num_answer == ANSWER_BARTER) {
            goto label_Barter;
        }

        // Refresh
        if (num_answer == ANSWER_BEGIN) {
            cl->Send_Talk();
            return;
        }

        // End
        if (num_answer == ANSWER_END) {
            CrMngr.CloseTalk(cl);
            return;
        }

        // Invalid answer
        if (num_answer >= cur_dialog->Answers.size()) {
            WriteLog("Wrong number of answer {}, client '{}'.\n", num_answer, cl->GetName());
            cl->Send_Talk(); // Refresh
            return;
        }

        // Find answer
        answer = &(*(cur_dialog->Answers.begin() + num_answer));

        // Check demand again
        if (!Dialog_CheckDemand(npc, cl, *answer, true)) {
            WriteLog("Secondary check of dialog demands fail, client '{}'.\n", cl->GetName());
            CrMngr.CloseTalk(cl); // End
            return;
        }

        // Use result
        force_dialog = Dialog_UseResult(npc, cl, *answer);
        if (force_dialog != 0u) {
            dlg_id = force_dialog;
        }
        else {
            dlg_id = answer->Link;
        }

        // Special links
        switch (dlg_id) {
        case static_cast<uint>(-3):
            [[fallthrough]];
        case DIALOG_BARTER:
        label_Barter:
            if (cur_dialog->DlgScriptFunc) {
                cl->Send_TextMsg(npc, STR_BARTER_NO_BARTER_NOW, SAY_DIALOG, TEXTMSG_GAME);
                return;
            }

            if (!ScriptSys.CritterBarterEvent(cl, npc, true, npc->GetBarterPlayers() + 1)) {
                cl->Talk.Barter = true;
                cl->Talk.StartTick = GameTime.GameTick();
                cl->Talk.TalkTime = Settings.DlgBarterMinTime;
            }
            return;
        case static_cast<uint>(-2):
            [[fallthrough]];
        case DIALOG_BACK:
            if (cl->Talk.LastDialogId != 0u) {
                dlg_id = cl->Talk.LastDialogId;
                break;
            }
            [[fallthrough]];
        case static_cast<uint>(-1):
            [[fallthrough]];
        case DIALOG_END:
            CrMngr.CloseTalk(cl);
            return;
        default:
            break;
        }
    }
    else {
        cl->Talk.Barter = false;
        dlg_id = cur_dialog->Id;
        if (npc != nullptr) {
            ScriptSys.CritterBarterEvent(cl, npc, false, npc->GetBarterPlayers() + 1);
        }
    }

    // Find dialog
    const auto it_d = std::find_if(dialogs->begin(), dialogs->end(), [dlg_id](const Dialog& dlg) { return dlg.Id == dlg_id; });
    if (it_d == dialogs->end()) {
        CrMngr.CloseTalk(cl);
        cl->Send_TextMsg(cl, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog from link {} not found, client '{}', dialog pack {}.\n", dlg_id, cl->GetName(), dialog_pack->PackId);
        return;
    }

    // Compile
    if (!Dialog_Compile(npc, cl, *it_d, cl->Talk.CurDialog)) {
        CrMngr.CloseTalk(cl);
        cl->Send_TextMsg(cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog compile fail, client '{}', dialog pack {}.\n", cl->GetName(), dialog_pack->PackId);
        return;
    }

    if (dlg_id != last_dialog) {
        cl->Talk.LastDialogId = last_dialog;
    }

    // Get lexems
    cl->Talk.Lexems.clear();
    if (cl->Talk.CurDialog.DlgScriptFunc) {
        cl->Talk.Locked = true;
        if (cl->Talk.CurDialog.DlgScriptFunc(cl, npc)) {
            cl->Talk.Lexems = cl->Talk.CurDialog.DlgScriptFunc.GetResult();
        }
        cl->Talk.Locked = false;
    }

    // On head text
    if (cl->Talk.CurDialog.Answers.empty()) {
        if (npc != nullptr) {
            npc->SendAA_MsgLex(npc->VisCr, cl->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cl->Talk.Lexems.c_str());
        }
        else {
            auto* map = MapMngr.GetMap(cl->GetMapId());
            if (map != nullptr) {
                map->SetTextMsg(cl->Talk.TalkHexX, cl->Talk.TalkHexY, 0, TEXTMSG_DLG, cl->Talk.CurDialog.TextId);
            }
        }

        CrMngr.CloseTalk(cl);
        return;
    }

    cl->Talk.StartTick = GameTime.GameTick();
    cl->Talk.TalkTime = Settings.DlgTalkMinTime;
    cl->Send_Talk();
}

auto FOServer::CreateItemOnHex(Map* map, ushort hx, ushort hy, hash pid, uint count, Properties* props, bool check_blocks) -> Item*
{
    // Checks
    const auto* proto_item = ProtoMngr.GetProtoItem(pid);
    if (proto_item == nullptr || count == 0u) {
        return nullptr;
    }

    // Check blockers
    if (check_blocks && !map->IsPlaceForProtoItem(hx, hy, proto_item)) {
        return nullptr;
    }

    // Create instance
    auto* item = ItemMngr.CreateItem(pid, count, props);
    if (item == nullptr) {
        return nullptr;
    }

    // Add on map
    if (!map->AddItem(item, hx, hy)) {
        ItemMngr.DeleteItem(item);
        return nullptr;
    }

    // Recursive non-stacked items
    if (!proto_item->GetStackable() && count > 1) {
        return CreateItemOnHex(map, hx, hy, pid, count - 1, props, true);
    }

    return item;
}

void FOServer::OnSendItemValue(Entity* entity, Property* prop)
{
    auto* item = dynamic_cast<Item*>(entity);
    if (item->Id != 0u && item->Id != static_cast<uint>(-1)) {
        const auto is_public = (prop->GetAccess() & Property::PublicMask) != 0;
        const auto is_protected = (prop->GetAccess() & Property::ProtectedMask) != 0;
        if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER) {
            if (is_public || is_protected) {
                auto* cr = CrMngr.GetCritter(item->GetCritId());
                if (cr != nullptr) {
                    if (is_public || is_protected) {
                        cr->Send_Property(NetProperty::ChosenItem, prop, item);
                    }
                    if (is_public) {
                        cr->SendA_Property(NetProperty::CritterItem, prop, item);
                    }
                }
            }
        }
        else if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
            if (is_public) {
                auto* map = MapMngr.GetMap(item->GetMapId());
                if (map != nullptr) {
                    map->SendProperty(NetProperty::MapItem, prop, item);
                }
            }
        }
        else if (item->GetAccessory() == ITEM_ACCESSORY_CONTAINER) {
            // Todo: add container properties changing notifications
            // Item* cont = ItemMngr.GetItem( item->GetContainerId() );
        }
    }
}

void FOServer::OnSetItemCount(Entity* entity, Property* /*prop*/, void* cur_value, void* old_value)
{
    NON_CONST_METHOD_HINT(FpsTick);

    auto* item = dynamic_cast<Item*>(entity);
    const auto cur = *static_cast<uint*>(cur_value);
    const auto old = *static_cast<uint*>(old_value);
    if (static_cast<int>(cur) > 0 && (item->GetStackable() || cur == 1)) {
        const auto diff = static_cast<int>(item->GetCount()) - static_cast<int>(old);
        ItemMngr.ChangeItemStatistics(item->GetProtoId(), diff);
    }
    else {
        item->SetCount(old);
        if (!item->GetStackable()) {
            throw GenericException("Trying to change count of not stackable item");
        }

        throw GenericException("Item count can't be zero or negative", cur);
    }
}

void FOServer::OnSetItemChangeView(Entity* entity, Property* prop, void* cur_value, void* /*old_value*/)
{
    // IsHidden, IsAlwaysView, IsTrap, TrapValue
    auto* item = dynamic_cast<Item*>(entity);

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            map->ChangeViewItem(item);
            if (prop == Item::PropertyIsTrap) {
                map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
            }
        }
    }
    else if (item->GetAccessory() == ITEM_ACCESSORY_CRITTER) {
        auto* cr = CrMngr.GetCritter(item->GetCritId());
        if (cr != nullptr) {
            const auto value = *static_cast<bool*>(cur_value);
            if (value) {
                cr->Send_EraseItem(item);
            }
            else {
                cr->Send_AddItem(item);
            }
            cr->SendAA_MoveItem(item, ACTION_REFRESH, 0);
        }
    }
}

void FOServer::OnSetItemRecacheHex(Entity* entity, Property* /*prop*/, void* cur_value, void* /*old_value*/)
{
    // IsNoBlock, IsShootThru, IsGag, IsTrigger
    auto* item = dynamic_cast<Item*>(entity);
    auto value = *static_cast<bool*>(cur_value);

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
        }
    }
}

void FOServer::OnSetItemBlockLines(Entity* entity, Property* /*prop*/, void* /*cur_value*/, void* /*old_value*/)
{
    // BlockLines
    auto* item = dynamic_cast<Item*>(entity);
    if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            // Todo: make BlockLines changable in runtime
        }
    }
}

void FOServer::OnSetItemIsGeck(Entity* entity, Property* /*prop*/, void* cur_value, void* /*old_value*/)
{
    auto* item = dynamic_cast<Item*>(entity);
    const auto value = *static_cast<bool*>(cur_value);

    if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            map->GetLocation()->GeckCount += (value ? 1 : -1);
        }
    }
}

void FOServer::OnSetItemIsRadio(Entity* entity, Property* /*prop*/, void* cur_value, void* /*old_value*/)
{
    auto* item = dynamic_cast<Item*>(entity);
    const auto value = *static_cast<bool*>(cur_value);

    if (value) {
        ItemMngr.RegisterRadio(item);
    }
    else {
        ItemMngr.UnregisterRadio(item);
    }
}

void FOServer::OnSetItemOpened(Entity* entity, Property* /*prop*/, void* cur_value, void* old_value)
{
    auto* item = dynamic_cast<Item*>(entity);
    const auto cur = *static_cast<bool*>(cur_value);
    const auto old = *static_cast<bool*>(old_value);

    if (item->GetIsCanOpen()) {
        if (!old && cur) {
            item->SetIsLightThru(true);

            if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
                auto* map = MapMngr.GetMap(item->GetMapId());
                if (map != nullptr) {
                    map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
                }
            }
        }
        if (old && !cur) {
            item->SetIsLightThru(false);

            if (item->GetAccessory() == ITEM_ACCESSORY_HEX) {
                auto* map = MapMngr.GetMap(item->GetMapId());
                if (map != nullptr) {
                    map->SetHexFlag(item->GetHexX(), item->GetHexY(), FH_BLOCK_ITEM);
                    map->SetHexFlag(item->GetHexX(), item->GetHexY(), FH_NRAKE_ITEM);
                }
            }
        }
    }
}

auto FOServer::DialogScriptDemand(DemandResult& /*demand*/, Critter* /*master*/, Critter * /*slave*/) -> bool
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

auto FOServer::DialogScriptResult(DemandResult& /*result*/, Critter* /*master*/, Critter * /*slave*/) -> uint
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
