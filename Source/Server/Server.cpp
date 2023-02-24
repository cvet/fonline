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

#include "Server.h"
#include "AdminPanel.h"
#include "AnyData.h"
#include "Application.h"
#include "GenericUtils.h"
#include "Networking.h"
#include "PropertiesSerializator.h"
#include "ServerScripting.h"
#include "StringUtils.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#include "imgui.h"

FOServer::FOServer(GlobalSettings& settings) :
#if !FO_SINGLEPLAYER
    FOEngineBase(settings, PropertiesRelationType::ServerRelative),
#else
    FOEngineBase(settings, PropertiesRelationType::BothRelative),
#endif
    ProtoMngr(this),
    ServerDeferredCalls(this),
    EntityMngr(this),
    MapMngr(this),
    CrMngr(this),
    ItemMngr(this),
    DlgMngr(this)
{
    STACK_TRACE_ENTRY();

    WriteLog("Start server");

    WriteLog("Mount data packs");

    Resources.AddDataSource(Settings.EmbeddedResources);
    Resources.AddDataSource(Settings.ResourcesDir, DataSourceType::DirRoot);

    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("Maps"));
    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("ServerProtos"));
    Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("Dialogs"));
    if constexpr (FO_ANGELSCRIPT_SCRIPTING) {
        Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath("ServerAngelScript"));
    }

    for (const auto& entry : Settings.ServerResourceEntries) {
        Resources.AddDataSource(_str(Settings.ResourcesDir).combinePath(entry));
    }

#if !FO_SINGLEPLAYER
    WriteLog("Initialize script system");

    extern void Server_RegisterData(FOEngineBase*);
    Server_RegisterData(this);

    ScriptSys = new ServerScriptSystem(this);
    ScriptSys->InitSubsystems();
#endif

    // Network
#if !FO_SINGLEPLAYER
    WriteLog("Start networking");

    if (auto* server = NetServerBase::StartInterthreadServer(Settings, [this](NetConnection* net_connection) { OnNewConnection(net_connection); }); server != nullptr) {
        _connectionServers.emplace_back(server);
    }

    if (auto* server = NetServerBase::StartTcpServer(Settings, [this](NetConnection* net_connection) { OnNewConnection(net_connection); }); server != nullptr) {
        _connectionServers.emplace_back(server);
    }
    if (auto* server = NetServerBase::StartWebSocketsServer(Settings, [this](NetConnection* net_connection) { OnNewConnection(net_connection); }); server != nullptr) {
        _connectionServers.emplace_back(server);
    }
#endif

    // Data base
    DbStorage = ConnectToDataBase(Settings.DbStorage);
    if (!DbStorage) {
        throw ServerInitException("Can't init storage data base", Settings.DbStorage);
    }

    WriteLog("Setup engine data");

    // Properties that saving to data base
    for (auto&& [name, registrator] : GetAllPropertyRegistrators()) {
        const auto count = static_cast<int>(registrator->GetCount());
        for (auto i = 0; i < count; i++) {
            const auto* prop = registrator->GetByIndex(i);

            if (prop->IsDisabled()) {
                continue;
            }
            if (prop->IsTemporary()) {
                continue;
            }

            switch (prop->GetAccess()) {
            case Property::AccessType::PrivateCommon:
                [[fallthrough]];
            case Property::AccessType::PrivateServer:
                [[fallthrough]];
            case Property::AccessType::Public:
                [[fallthrough]];
            case Property::AccessType::PublicModifiable:
                [[fallthrough]];
            case Property::AccessType::PublicFullModifiable:
                [[fallthrough]];
            case Property::AccessType::Protected:
                [[fallthrough]];
            case Property::AccessType::ProtectedModifiable:
                break;
            default:
                continue;
            }

            prop->AddPostSetter([this](Entity* entity, const Property* prop_) { OnSaveEntityValue(entity, prop_); });
        }
    }

    // Properties that sending to clients
    {
        const auto set_send_callbacks = [](const auto* registrator, const PropertyPostSetCallback& callback) {
            const auto count = static_cast<int>(registrator->GetCount());
            for (auto i = 0; i < count; i++) {
                const auto* prop = registrator->GetByIndex(i);

                if (prop->IsDisabled()) {
                    continue;
                }

                switch (prop->GetAccess()) {
                case Property::AccessType::Public:
                    [[fallthrough]];
                case Property::AccessType::PublicModifiable:
                    [[fallthrough]];
                case Property::AccessType::PublicFullModifiable:
                    [[fallthrough]];
                case Property::AccessType::Protected:
                    [[fallthrough]];
                case Property::AccessType::ProtectedModifiable:
                    break;
                default:
                    continue;
                }

                prop->AddPostSetter(callback);
            }
        };

        set_send_callbacks(GetPropertyRegistrator(GameProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendGlobalValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(PlayerProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendPlayerValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendItemValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendCritterValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendMapValue(entity, prop); });
        set_send_callbacks(GetPropertyRegistrator(LocationProperties::ENTITY_CLASS_NAME), [this](Entity* entity, const Property* prop) { OnSendLocationValue(entity, prop); });
    }

    // Properties with custom behaviours
    {
        const auto set_setter = [](const auto* registrator, int prop_index, PropertySetCallback callback) {
            const auto* prop = registrator->GetByIndex(prop_index);
            prop->AddSetter(std::move(callback));
        };
        const auto set_post_setter = [](const auto* registrator, int prop_index, PropertyPostSetCallback callback) {
            const auto* prop = registrator->GetByIndex(prop_index);
            prop->AddPostSetter(std::move(callback));
        };

        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::Count_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemCount(entity, prop, data.GetPtrAs<void>()); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsHidden_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemChangeView(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsAlwaysView_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemChangeView(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsTrap_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemChangeView(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::TrapValue_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemChangeView(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsNoBlock_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemRecacheHex(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsShootThru_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemRecacheHex(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsGag_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemRecacheHex(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsTrigger_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemRecacheHex(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::BlockLines_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemBlockLines(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsGeck_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemIsGeck(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsRadio_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemIsRadio(entity, prop); });
        set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::Opened_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemOpened(entity, prop); });
    }

    WriteLog("Load dialogs data");
    DlgMngr.LoadFromResources();

    WriteLog("Load protos data");
    ProtoMngr.LoadFromResources();

    WriteLog("Load maps data");
    MapMngr.LoadFromResources();

    // Resource packs for client
    if (Settings.DataSynchronization) {
        WriteLog("Load client data packs for synchronization");

        FileSystem client_resources;
        client_resources.AddDataSource(_str("Client{}", Settings.ResourcesDir), DataSourceType::DirRoot);

        auto writer = DataWriter(_updateFilesDesc);

        const auto add_sync_file = [&client_resources, &writer, this](string_view path) {
            const auto file = client_resources.ReadFile(path);
            if (!file) {
                throw ServerInitException("Resource pack for client not found", path);
            }

            const auto data = file.GetData();
            _updateFilesData.push_back(data);

            writer.Write<int16>(static_cast<int16>(file.GetPath().length()));
            writer.WritePtr(file.GetPath().data(), file.GetPath().length());
            writer.Write<uint>(static_cast<uint>(data.size()));
            writer.Write<uint>(Hashing::MurmurHash2(data.data(), data.size()));
        };

        add_sync_file("EngineData.zip");
        add_sync_file("Core.zip");
        add_sync_file("Texts.zip");
        add_sync_file("ClientProtos.zip");
        add_sync_file("StaticMaps.zip");
        if constexpr (FO_ANGELSCRIPT_SCRIPTING) {
            add_sync_file("ClientAngelScript.zip");
        }

        for (const auto& resource_entry : Settings.ClientResourceEntries) {
            add_sync_file(_str("{}.zip", resource_entry));
        }

        // Complete files list
        writer.Write<int16>(-1);
    }

    // Admin manager
    if (Settings.AdminPanelPort != 0u) {
        WriteLog("Run admin panel at port {}", Settings.AdminPanelPort);

        InitAdminManager(this, static_cast<uint16>(Settings.AdminPanelPort));
    }
}

void FOServer::Start()
{
    STACK_TRACE_ENTRY();

    WriteLog("Start game logic");

    DbStorage.StartChanges();

    // Globals
    const auto globals_doc = DbStorage.Get("Game", ident_t {1});
    if (globals_doc.empty()) {
        DbStorage.Insert("Game", ident_t {1}, {});
    }
    else {
        if (!PropertiesSerializator::LoadFromDocument(&GetPropertiesForEdit(), globals_doc, *this)) {
            throw ServerInitException("Failed to load globals document");
        }

        GameTime.Reset(GetYear(), GetMonth(), GetDay(), GetHour(), GetMinute(), GetSecond(), GetTimeMultiplier());
    }

    GameTime.FrameAdvance();

    // Scripting
    WriteLog("Init script modules");

    ScriptSys->InitModules();

    if (!OnInit.Fire()) {
        throw ServerInitException("Initialization script failed");
    }

    // Init world
    if (globals_doc.empty()) {
        WriteLog("Generate world");

        if (!OnGenerateWorld.Fire()) {
            throw ServerInitException("Generate world script failed");
        }
    }
    else {
        WriteLog("Restore world");

        int errors = 0;

        try {
            ServerDeferredCalls.LoadDeferredCalls();
        }
        catch (std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }

        try {
            EntityMngr.LoadEntities();
        }
        catch (std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }

        if (errors != 0) {
            throw ServerInitException("Something went wrong during world restoring");
        }
    }

    WriteLog("Start world");

    // Start script
    if (!OnStart.Fire()) {
        throw ServerInitException("Start script failed");
    }

    // Commit initial data base changes
    DbStorage.CommitChanges();

    GameTime.FrameAdvance();
    _stats.ServerStartTick = GameTime.FrameTick();
    _fpsTick = GameTime.FrameTick();
    _started = true;

    WriteLog("Start server complete!");
}

FOServer::~FOServer()
{
    STACK_TRACE_ENTRY();

    for (const auto* server : _connectionServers) {
        delete server;
    }
    _connectionServers.clear();

    delete ScriptSys;
}

void FOServer::Shutdown()
{
    STACK_TRACE_ENTRY();

    _willFinishDispatcher();

    // Finish logic
    DbStorage.StartChanges();

    OnFinish.Fire();
    EntityMngr.FinalizeEntities();

    DbStorage.CommitChanges();

    // Shutdown servers
    for (auto* server : _connectionServers) {
        server->Shutdown();
        delete server;
    }
    _connectionServers.clear();

    // Logging clients
    SetLogCallback("LogToClients", nullptr);
    for (const auto* cl : _logClients) {
        cl->Release();
    }
    _logClients.clear();

    // Logined players
    for (auto&& [id, player] : copy(EntityMngr.GetPlayers())) {
        player->Connection->HardDisconnect();
    }

    // Unlogined players
    for (auto* player : _unloginedPlayers) {
        player->Connection->HardDisconnect();
        player->MarkAsDestroyed();
        player->Release();
    }
    _unloginedPlayers.clear();

    // New connections
    {
        std::lock_guard locker(_newConnectionsLocker);

        for (auto* conn : _newConnections) {
            conn->HardDisconnect();
            delete conn;
        }
        _newConnections.clear();
    }

    // Stats
    WriteLog("Server stopped");
    WriteLog("Stats:");
    WriteLog("Traffic:");
    WriteLog("Bytes Send: {}", _stats.BytesSend);
    WriteLog("Bytes Recv: {}", _stats.BytesRecv);
    WriteLog("Cycles count: {}", _stats.LoopCycles);
    WriteLog("Approx cycle period: {}", _stats.LoopTime / (_stats.LoopCycles != 0u ? _stats.LoopCycles : 1));
    WriteLog("Min cycle period: {}", _stats.LoopMin);
    WriteLog("Max cycle period: {}", _stats.LoopMax);
    WriteLog("Count of lags (>100ms): {}", _stats.LagsCount);

    _didFinishDispatcher();
}

void FOServer::MainLoop()
{
    STACK_TRACE_ENTRY();

    // Cycle time
    const auto frame_begin = Timer::RealtimeTick();

    // Begin data base changes
    DbStorage.StartChanges();

    // Advance time
    if (GameTime.FrameAdvance()) {
        const auto st = GameTime.GetGameTime(GameTime.GetFullSecond());
        SetYear(st.Year);
        SetMonth(st.Month);
        SetDay(st.Day);
        SetHour(st.Hour);
        SetMinute(st.Minute);
        SetSecond(st.Second);
    }

    // Script subsystems update
    ScriptSys->Process();

    // Process unlogined players
    {
        {
            std::lock_guard locker(_newConnectionsLocker);

            if (!_newConnections.empty()) {
                for (auto* conn : _newConnections) {
                    _unloginedPlayers.emplace_back(new Player(this, ident_t {}, conn));
                }
                _newConnections.clear();
            }
        }

        for (auto* player : copy_hold_ref(_unloginedPlayers)) {
            try {
                ProcessConnection(player->Connection);
                ProcessUnloginedPlayer(player);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }

        _stats.CurOnline = _unloginedPlayers.size() + CrMngr.PlayersInGame();
        _stats.MaxOnline = std::max(_stats.MaxOnline, _stats.CurOnline);
    }

    // Process players
    {
        for (auto* player : copy_hold_ref(EntityMngr.GetPlayers())) {
            try {
                RUNTIME_ASSERT(!player->IsDestroyed());

                ProcessConnection(player->Connection);
                ProcessPlayer(player);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }
    }

    // Process critters
    {
        for (auto* cr : copy_hold_ref(EntityMngr.GetCritters())) {
            if (cr->IsDestroyed()) {
                continue;
            }

            try {
                ProcessCritter(cr);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }
    }

    // Process maps
    {
        for (auto* map : copy_hold_ref(EntityMngr.GetMaps())) {
            if (map->IsDestroyed()) {
                continue;
            }

            try {
                map->Process();
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }
    }

    try {
        MapMngr.LocationGarbager();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }

    ServerDeferredCalls.Process();

    // Script game loop
    OnLoop.Fire();

    // Commit changed to data base
    DbStorage.CommitChanges();

    // Clients log
    DispatchLogToClients();

    // Fill statistics
    const uint loop_tick = iround(Timer::RealtimeTick() - frame_begin);
    _stats.LoopTime += loop_tick;
    _stats.LoopCycles++;
    _stats.LoopMax = std::max(loop_tick, _stats.LoopMax);
    _stats.LoopMax = std::min(loop_tick, _stats.LoopMin);
    _stats.CycleTime = loop_tick;
    if (loop_tick > 100) {
        _stats.LagsCount++;
    }
    _stats.Uptime = (GameTime.FrameTick() - _stats.ServerStartTick) / 1000;

    // Calculate fps
    if (GameTime.FrameTick() - _fpsTick >= 1000) {
        _stats.Fps = _fpsCounter;
        _fpsCounter = 0;
        _fpsTick = GameTime.FrameTick();
    }
    else {
        _fpsCounter++;
    }

    // Sleep
    if (Settings.ServerSleep >= 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(Settings.ServerSleep));
    }
}

void FOServer::DrawGui(string_view server_name)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    constexpr auto default_buf_size = 1000000; // ~1mb
    string buf;
    buf.reserve(default_buf_size);

    if (ImGui::Begin(string(server_name).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Info
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Info")) {
            buf = "";
            const auto st = GameTime.GetGameTime(GameTime.GetFullSecond());
            buf += _str("Time: {:02}.{:02}.{:04} {:02}:{:02}:{:02} x{}\n", st.Day, st.Month, st.Year, st.Hour, st.Minute, st.Second, "WIP" /*GetTimeMultiplier()*/);
            buf += _str("Connections: {}\n", _stats.CurOnline);
            buf += _str("Players in game: {}\n", CrMngr.PlayersInGame());
            buf += _str("Critters in game: {}\n", CrMngr.CrittersInGame());
            buf += _str("Locations: {} ({})\n", MapMngr.GetLocationsCount(), MapMngr.GetMapsCount());
            buf += _str("Items: {}\n", ItemMngr.GetItemsCount());
            buf += _str("Cycles per second: {}\n", _stats.Fps);
            buf += _str("Cycle time: {}\n", _stats.CycleTime);
            const auto seconds = _stats.Uptime;
            buf += _str("Uptime: {:02}:{:02}:{:02}\n", seconds / 60 / 60, seconds / 60 % 60, seconds % 60);
            buf += _str("KBytes Send: {}\n", _stats.BytesSend / 1024);
            buf += _str("KBytes Recv: {}\n", _stats.BytesRecv / 1024);
            buf += _str("Compress ratio: {}", static_cast<double>(_stats.DataReal) / (_stats.DataCompressed != 0 ? _stats.DataCompressed : 1));
            ImGui::TextUnformatted(buf.c_str(), buf.c_str() + buf.size());
            ImGui::TreePop();
        }

        // Players
        if (ImGui::TreeNode("Players")) {
            buf = GetIngamePlayersStatistics();
            ImGui::TextUnformatted(buf.c_str(), buf.c_str() + buf.size());
            ImGui::TreePop();
        }

        // Locations and maps
        if (ImGui::TreeNode("Locations and maps")) {
            buf = MapMngr.GetLocationAndMapsStatistics();
            ImGui::TextUnformatted(buf.c_str(), buf.c_str() + buf.size());
            ImGui::TreePop();
        }

        // Items count
        if (ImGui::TreeNode("Items count")) {
            buf = ItemMngr.GetItemsStatistics();
            ImGui::TextUnformatted(buf.c_str(), buf.c_str() + buf.size());
            ImGui::TreePop();
        }

        // Profiler
        if (ImGui::TreeNode("Profiler")) {
            buf = "WIP..........................."; // ScriptSys.GetProfilerStatistics();
            ImGui::TextUnformatted(buf.c_str(), buf.c_str() + buf.size());
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

auto FOServer::GetIngamePlayersStatistics() -> string
{
    STACK_TRACE_ENTRY();

    const auto& players = EntityMngr.GetPlayers();
    const auto conn_count = _unloginedPlayers.size() + players.size();

    string result = _str("Players in game: {}\nConnections: {}\n", players.size(), conn_count);
    result += "Name                 Id         Ip              X     Y     Location and map\n";
    for (auto&& [id, player] : players) {
        const auto* cr = player->GetOwnedCritter();
        const auto* map = MapMngr.GetMap(cr->GetMapId());
        const auto* loc = (map != nullptr ? map->GetLocation() : nullptr);

        const string str_loc = _str("{} ({}) {} ({})", map != nullptr ? loc->GetName() : "", map != nullptr ? loc->GetId() : ident_t {}, map != nullptr ? map->GetName() : "", map != nullptr ? map->GetId() : ident_t {});
        result += _str("{:<20} {:<10} {:<15} {:<5} {:<5} {}\n", player->GetName(), player->GetId(), player->GetHost(), map != nullptr ? cr->GetHexX() : cr->GetWorldX(), map != nullptr ? cr->GetHexY() : cr->GetWorldY(), map != nullptr ? str_loc : "Global map");
    }
    return result;
}

void FOServer::OnNewConnection(NetConnection* net_connection)
{
    STACK_TRACE_ENTRY();

    if (!_started) {
        net_connection->Disconnect();
        return;
    }

    std::lock_guard locker(_newConnectionsLocker);

    _newConnections.emplace_back(new ClientConnection(net_connection));
}

void FOServer::ProcessUnloginedPlayer(Player* unlogined_player)
{
    STACK_TRACE_ENTRY();

    auto* connection = unlogined_player->Connection;

    if (connection->IsHardDisconnected()) {
        const auto it = std::find(_unloginedPlayers.begin(), _unloginedPlayers.end(), unlogined_player);
        RUNTIME_ASSERT(it != _unloginedPlayers.end());
        _unloginedPlayers.erase(it);
        unlogined_player->MarkAsDestroyed();
        unlogined_player->Release();
        return;
    }

    std::lock_guard locker(connection->InBufLocker);

    if (connection->IsGracefulDisconnected()) {
        connection->InBuf.ResetBuf();
        return;
    }

    connection->InBuf.ShrinkReadBuf();

    if (connection->InBuf.NeedProcess()) {
        connection->LastActivityTime = GameTime.FrameTick();

        uint msg = 0;
        connection->InBuf >> msg;

        if (!connection->WasHandshake) {
            switch (msg) {
            case 0xFFFFFFFF: {
                // At least 16 bytes should be sent for backward compatibility,
                // even if answer data will change its meaning
                CONNECTION_OUTPUT_BEGIN(connection);
                connection->DisableCompression();
                connection->OutBuf << _stats.CurOnline;
                connection->OutBuf << _stats.Uptime;
                connection->OutBuf << static_cast<uint>(0);
                connection->OutBuf << static_cast<uint8>(0);
                connection->OutBuf << static_cast<uint8>(0xF0);
                connection->OutBuf << static_cast<uint16>(0xFFFF);
                CONNECTION_OUTPUT_END(connection);

                connection->HardDisconnect();
                break;
            }

            case NETMSG_HANDSHAKE:
                Process_Handshake(connection);
                break;

            default:
                connection->InBuf.SkipMsg(msg);
                break;
            }
        }
        else {
            switch (msg) {
            case NETMSG_PING:
                Process_Ping(connection);
                break;
            case NETMSG_GET_UPDATE_FILE:
                Process_UpdateFile(connection);
                break;
            case NETMSG_GET_UPDATE_FILE_DATA:
                Process_UpdateFileData(connection);
                break;

            case NETMSG_REGISTER:
                Process_Register(unlogined_player);
                break;
            case NETMSG_RPC:
                Process_RemoteCall(unlogined_player);
                break;

            case NETMSG_LOGIN:
                Process_Login(unlogined_player);
                break;

            default:
                connection->InBuf.SkipMsg(msg);
                break;
            }
        }

        if (IsRunInDebugger() && connection->IsInterthreadConnection() && !connection->InBuf.NeedProcess()) {
            RUNTIME_ASSERT(connection->InBuf.GetReadPos() == connection->InBuf.GetEndPos());
        }
    }
}

void FOServer::ProcessPlayer(Player* player)
{
    STACK_TRACE_ENTRY();

    if (player->Connection->IsHardDisconnected()) {
        WriteLog("Disconnected player {}", player->GetName());

        if (auto* cr = player->GetOwnedCritter(); cr != nullptr) {
            cr->DetachPlayer();
        }

        OnPlayerLogout.Fire(player);
        EntityMngr.UnregisterEntity(player);

        player->Release();
        return;
    }

    if (player->Connection->IsGracefulDisconnected()) {
        std::lock_guard locker(player->Connection->InBufLocker);
        player->Connection->InBuf.ResetBuf();
        return;
    }

    uint msg = 0;

    while (!player->Connection->IsHardDisconnected() && !player->Connection->IsGracefulDisconnected()) {
        std::lock_guard locker(player->Connection->InBufLocker);

        player->Connection->InBuf.ShrinkReadBuf();

        if (!player->Connection->InBuf.NeedProcess()) {
            CHECK_CLIENT_IN_BUF_ERROR(player->Connection);
            if (IsRunInDebugger() && player->Connection->IsInterthreadConnection()) {
                RUNTIME_ASSERT(player->Connection->InBuf.GetReadPos() == player->Connection->InBuf.GetEndPos());
            }
            break;
        }

        player->Connection->InBuf >> msg;

        switch (msg) {
        case NETMSG_PING:
            Process_Ping(player->Connection);
            break;
        case NETMSG_SEND_TEXT:
            Process_Text(player);
            break;
        case NETMSG_SEND_COMMAND:
            Process_Command(
                player->Connection->InBuf, [player](auto s) { player->Send_Text(nullptr, _str(s).trim(), SAY_NETMSG); }, player, "");
            break;
        case NETMSG_DIR:
            Process_Dir(player);
            break;
        case NETMSG_SEND_MOVE:
            Process_Move(player);
            break;
        case NETMSG_SEND_STOP_MOVE:
            Process_StopMove(player);
            break;
        case NETMSG_SEND_TALK_NPC:
            Process_Dialog(player);
            break;
        case NETMSG_RPC:
            Process_RemoteCall(player);
            break;
        case NETMSG_SEND_POD_PROPERTY(1, 0):
        case NETMSG_SEND_POD_PROPERTY(1, 1):
        case NETMSG_SEND_POD_PROPERTY(1, 2):
            Process_Property(player, 1);
            break;
        case NETMSG_SEND_POD_PROPERTY(2, 0):
        case NETMSG_SEND_POD_PROPERTY(2, 1):
        case NETMSG_SEND_POD_PROPERTY(2, 2):
            Process_Property(player, 2);
            break;
        case NETMSG_SEND_POD_PROPERTY(4, 0):
        case NETMSG_SEND_POD_PROPERTY(4, 1):
        case NETMSG_SEND_POD_PROPERTY(4, 2):
            Process_Property(player, 4);
            break;
        case NETMSG_SEND_POD_PROPERTY(8, 0):
        case NETMSG_SEND_POD_PROPERTY(8, 1):
        case NETMSG_SEND_POD_PROPERTY(8, 2):
            Process_Property(player, 8);
            break;
        case NETMSG_SEND_COMPLEX_PROPERTY:
            Process_Property(player, 0);
            break;
        default:
            player->Connection->InBuf.SkipMsg(msg);
            break;
        }

        player->Connection->LastActivityTime = GameTime.FrameTick();

        CHECK_CLIENT_IN_BUF_ERROR(player->Connection);
    }
}

void FOServer::ProcessConnection(ClientConnection* connection)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (connection->IsHardDisconnected()) {
        return;
    }

    if (connection->InBuf.IsError()) {
        WriteLog("Wrong network data from connection '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    if (connection->LastActivityTime == 0u) {
        connection->LastActivityTime = GameTime.FrameTick();
    }

    if (Settings.InactivityDisconnectTime != 0u && GameTime.FrameTick() - connection->LastActivityTime >= Settings.InactivityDisconnectTime) {
        WriteLog("Connection activity timeout from ip '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    if (connection->WasHandshake && (connection->PingNextTick == 0u || GameTime.FrameTick() >= connection->PingNextTick)) {
        if (!connection->PingOk && !IsRunInDebugger()) {
            connection->HardDisconnect();
            return;
        }

        CONNECTION_OUTPUT_BEGIN(connection);
        connection->OutBuf.StartMsg(NETMSG_PING);
        connection->OutBuf << PING_CLIENT;
        connection->OutBuf.EndMsg();
        CONNECTION_OUTPUT_END(connection);

        connection->PingNextTick = GameTime.FrameTick() + PING_CLIENT_LIFE_TIME;
        connection->PingOk = false;
    }
}

void FOServer::Process_Text(Player* player)
{
    STACK_TRACE_ENTRY();

    Critter* cr = player->GetOwnedCritter();

    uint msg_len = 0;
    uint8 how_say = 0;
    string str;

    player->Connection->InBuf >> msg_len;
    player->Connection->InBuf >> how_say;
    player->Connection->InBuf >> str;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    if (!cr->IsAlive() && how_say >= SAY_NORM && how_say <= SAY_RADIO) {
        how_say = SAY_WHISP;
    }

    if (player->LastSay == str) {
        player->LastSayEqualCount++;

        if (player->LastSayEqualCount >= 10) {
            WriteLog("Flood detected, client '{}'. Disconnect", cr->GetName());
            player->Connection->HardDisconnect();
            return;
        }

        if (player->LastSayEqualCount >= 3) {
            return;
        }
    }
    else {
        player->LastSay = str;
        player->LastSayEqualCount = 0;
    }

    vector<uint16> channels;

    switch (how_say) {
    case SAY_NORM: {
        if (cr->GetMapId()) {
            cr->SendAndBroadcast_Text(cr->VisCr, str, SAY_NORM, true);
        }
        else {
            cr->SendAndBroadcast_Text(*cr->GlobalMapGroup, str, SAY_NORM, true);
        }
    } break;
    case SAY_SHOUT: {
        if (cr->GetMapId()) {
            auto* map = MapMngr.GetMap(cr->GetMapId());
            if (map == nullptr) {
                return;
            }

            cr->SendAndBroadcast_Text(map->GetCritters(), str, SAY_SHOUT, true);
        }
        else {
            cr->SendAndBroadcast_Text(*cr->GlobalMapGroup, str, SAY_SHOUT, true);
        }
    } break;
    case SAY_EMOTE: {
        if (cr->GetMapId()) {
            cr->SendAndBroadcast_Text(cr->VisCr, str, SAY_EMOTE, true);
        }
        else {
            cr->SendAndBroadcast_Text(*cr->GlobalMapGroup, str, SAY_EMOTE, true);
        }
    } break;
    case SAY_WHISP: {
        if (cr->GetMapId()) {
            cr->SendAndBroadcast_Text(cr->VisCr, str, SAY_WHISP, true);
        }
        else {
            cr->Send_TextEx(cr->GetId(), str, SAY_WHISP, true);
        }
    } break;
    case SAY_SOCIAL: {
        return;
    } break;
    case SAY_RADIO: {
        if (cr->GetMapId()) {
            cr->SendAndBroadcast_Text(cr->VisCr, str, SAY_WHISP, true);
        }
        else {
            cr->Send_TextEx(cr->GetId(), str, SAY_WHISP, true);
        }

        ItemMngr.RadioSendText(cr, str, true, 0, 0, channels);
        if (channels.empty()) {
            cr->Send_TextMsg(cr, STR_RADIO_CANT_SEND, SAY_NETMSG, TEXTMSG_GAME);
            return;
        }
    } break;
    default:
        return;
    }

    // Text listen
    vector<pair<ScriptFunc<void, Critter*, string>, string>> listen_callbacks;
    listen_callbacks.reserve(100u);

    if (how_say == SAY_RADIO) {
        for (auto& tl : _textListeners) {
            if (tl.SayType == SAY_RADIO && std::find(channels.begin(), channels.end(), tl.Parameter) != channels.end() && _str(string(str).substr(0, tl.FirstStr.length())).compareIgnoreCaseUtf8(tl.FirstStr)) {
                listen_callbacks.emplace_back(tl.Func, str);
            }
        }
    }
    else {
        const auto* map = MapMngr.GetMap(cr->GetMapId());
        const auto pid = (map != nullptr ? map->GetProtoId() : hstring());
        for (auto& tl : _textListeners) {
            if (tl.SayType == how_say && tl.Parameter == pid.as_uint() && _str(string(str).substr(0, tl.FirstStr.length())).compareIgnoreCaseUtf8(tl.FirstStr)) {
                listen_callbacks.emplace_back(tl.Func, str);
            }
        }
    }

    for (auto& cb : listen_callbacks) {
        if (!cb.first(cr, cb.second)) {
            // Nop
        }
    }
}

void FOServer::Process_Command(NetInBuffer& buf, const LogFunc& logcb, Player* player, string_view admin_panel)
{
    STACK_TRACE_ENTRY();

    SetLogCallback("Process_Command", logcb);
    Process_CommandReal(buf, logcb, player, admin_panel);
    SetLogCallback("Process_Command", nullptr);
}

void FOServer::Process_CommandReal(NetInBuffer& buf, const LogFunc& logcb, Player* player, string_view admin_panel)
{
    STACK_TRACE_ENTRY();

    auto* cl_ = player->GetOwnedCritter();

    uint msg_len = 0;
    uint8 cmd = 0;

    buf >> msg_len;
    buf >> cmd;

    auto sstr = string(cl_ != nullptr ? "" : admin_panel);
    auto allow_command = OnPlayerAllowCommand.Fire(player, sstr, cmd);

    if (!allow_command && (cl_ == nullptr)) {
        logcb("Command refused by script.");
        return;
    }

#define CHECK_ALLOW_COMMAND() \
    do { \
        if (!allow_command) { \
            return; \
        } \
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

        player->Connection->HardDisconnect();
    } break;
    case CMD_MYINFO: {
        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        string istr = _str("|0xFF00FF00 Name: |0xFFFF0000 {}|0xFF00FF00 , Id: |0xFFFF0000 {}|0xFF00FF00 , Access: ", cl_->GetName(), cl_->GetId());
        switch (player->Access) {
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

        string str = _str("Unlogined players: {}, Logined players: {}, Critters: {}. FOServer machine uptime: {} min., FOServer uptime: {} min.", //
            _unloginedPlayers.size(), CrMngr.PlayersInGame(), CrMngr.CrittersInGame(), GameTime.FrameTick() / 1000 / 60, (GameTime.FrameTick() - _stats.ServerStartTick) / 1000 / 60);

        result += str;

        for (const auto& line : _str(result).split('\n')) {
            logcb(line);
        }
    } break;
    case CMD_CRITID: {
        string name;
        buf >> name;

        CHECK_ALLOW_COMMAND();

        const auto player_id = MakePlayerId(name);
        if (DbStorage.Valid("Players", player_id)) {
            logcb(_str("Player id is {}.", player_id));
        }
        else {
            logcb("Client not found.");
        }
    } break;
    case CMD_MOVECRIT: {
        ident_t cr_id;
        uint16 hex_x = 0;
        uint16 hex_y = 0;
        buf >> cr_id;
        buf >> hex_x;
        buf >> hex_y;

        CHECK_ALLOW_COMMAND();

        auto* cr = CrMngr.GetCritter(cr_id);
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

        if (MapMngr.Transit(cr, map, hex_x, hex_y, cr->GetDir(), 3, ident_t {})) {
            logcb("Critter move success.");
        }
        else {
            logcb("Critter move fail.");
        }
    } break;
    case CMD_DISCONCRIT: {
        ident_t cr_id;
        buf >> cr_id;

        CHECK_ALLOW_COMMAND();

        if (cl_ != nullptr && cl_->GetId() == cr_id) {
            logcb("To kick yourself type <~exit>");
            return;
        }

        auto* cr = CrMngr.GetCritter(cr_id);
        if (cr == nullptr) {
            logcb("Critter not found.");
            return;
        }

        if (!cr->IsOwnedByPlayer()) {
            logcb("Founded critter is not owned by player.");
            return;
        }

        if (auto* owner = cr->GetOwner()) {
            owner->Send_Text(nullptr, "You are kicked from game.", SAY_NETMSG);
            owner->Connection->GracefulDisconnect();
            logcb("Player disconnected.");
        }
        else {
            logcb("Player is not in a game.");
        }
    } break;
    case CMD_TOGLOBAL: {
        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        if (!cl_->IsAlive()) {
            logcb("To global fail, only life none.");
            break;
        }

        if (MapMngr.TransitToGlobal(cl_, ident_t {})) {
            logcb("To global success.");
        }
        else {
            logcb("To global fail.");
        }
    } break;
    case CMD_PROPERTY: {
        ident_t cr_id;
        string property_name;
        auto property_value = 0;
        buf >> cr_id;
        buf >> property_name;
        buf >> property_value;

        CHECK_ALLOW_COMMAND();

        auto* cr = !cr_id ? cl_ : CrMngr.GetCritter(cr_id);
        if (cr != nullptr) {
            const auto* prop = GetPropertyRegistrator("Critter")->Find(property_name);
            if (prop == nullptr) {
                logcb("Property not found.");
                return;
            }
            if (!prop->IsPlainData()) {
                logcb("Property is not plain data type.");
                return;
            }

            cr->SetValueAsInt(prop, property_value);
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

        auto wanted_access = -1;
        if (name_access == "client" && std::find(Settings.AccessClient.begin(), Settings.AccessClient.end(), pasw_access) != Settings.AccessClient.end()) {
            wanted_access = ACCESS_CLIENT;
        }
        else if (name_access == "tester" && std::find(Settings.AccessTester.begin(), Settings.AccessTester.end(), pasw_access) != Settings.AccessTester.end()) {
            wanted_access = ACCESS_TESTER;
        }
        else if (name_access == "moder" && std::find(Settings.AccessModer.begin(), Settings.AccessModer.end(), pasw_access) != Settings.AccessModer.end()) {
            wanted_access = ACCESS_MODER;
        }
        else if (name_access == "admin" && std::find(Settings.AccessAdmin.begin(), Settings.AccessAdmin.end(), pasw_access) != Settings.AccessAdmin.end()) {
            wanted_access = ACCESS_ADMIN;
        }

        auto allow = false;
        if (wanted_access != -1) {
            auto& pass = pasw_access;
            allow = OnPlayerGetAccess.Fire(player, wanted_access, pass);
        }

        if (!allow) {
            logcb("Access denied.");
            break;
        }

        player->Access = static_cast<uint8>(wanted_access);
        logcb("Access changed.");
    } break;
    case CMD_ADDITEM: {
        uint16 hex_x = 0;
        uint16 hex_y = 0;
        hstring pid;
        uint count = 0;
        buf >> hex_x;
        buf >> hex_y;
        pid = buf.Read<hstring>(*this);
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
        hstring pid;
        uint count = 0;
        pid = buf.Read<hstring>(*this);
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
        uint16 hex_x = 0;
        uint16 hex_y = 0;
        uint8 dir = 0;
        hstring pid;
        buf >> hex_x;
        buf >> hex_y;
        buf >> dir;
        pid = buf.Read<hstring>(*this);

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        auto* map = MapMngr.GetMap(cl_->GetMapId());
        auto* npc = CrMngr.CreateCritter(pid, nullptr, map, hex_x, hex_y, dir, true);
        if (npc == nullptr) {
            logcb("Npc not created.");
        }
        else {
            logcb("Npc created.");
        }
    } break;
    case CMD_ADDLOCATION: {
        uint16 wx = 0;
        uint16 wy = 0;
        hstring pid;
        buf >> wx;
        buf >> wy;
        pid = buf.Read<hstring>(*this);

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
        int param0 = 0;
        int param1 = 0;
        int param2 = 0;
        buf >> func_name;
        buf >> param0;
        buf >> param1;
        buf >> param2;

        CHECK_ALLOW_COMMAND();

        if (func_name.empty()) {
            logcb("Fail, length is zero.");
            break;
        }

        if (ScriptSys->CallFunc<void, Critter*, int, int, int>(ToHashedString(func_name), static_cast<Critter*>(cl_), param0, param1, param2) || //
            ScriptSys->CallFunc<void, Player*, int, int, int>(ToHashedString(func_name), player, param0, param1, param2)) {
            logcb("Run script success.");
        }
        else {
            logcb("Run script failed.");
        }
    } break;
    case CMD_REGENMAP: {
        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        // Check global
        if (!cl_->GetMapId()) {
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
        MapMngr.Transit(cl_, map, hx, hy, dir, 5, ident_t {});
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
    } break;
    case CMD_LOG: {
        char flags[16];
        buf.Pop(flags, 16);

        CHECK_ALLOW_COMMAND();
        CHECK_ADMIN_PANEL();

        SetLogCallback("LogToClients", nullptr);

        auto it = std::find(_logClients.begin(), _logClients.end(), player);
        if (flags[0] == '-' && flags[1] == '\0' && it != _logClients.end()) // Detach current
        {
            logcb("Detached.");
            player->Release();
            _logClients.erase(it);
        }
        else if (flags[0] == '+' && flags[1] == '\0' && it == _logClients.end()) // Attach current
        {
            logcb("Attached.");
            player->AddRef();
            _logClients.push_back(player);
        }
        else if (flags[0] == '-' && flags[1] == '-' && flags[2] == '\0') // Detach all
        {
            logcb("Detached all.");
            for (auto* acc : _logClients) {
                acc->Release();
            }
            _logClients.clear();
        }

        if (!_logClients.empty()) {
            SetLogCallback("LogToClients", [this](string_view str) { LogToClients(str); });
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
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(multiplier >= 1 && multiplier <= 50000);
    RUNTIME_ASSERT(year >= Settings.StartYear);
    RUNTIME_ASSERT(year < Settings.StartYear + 100);
    RUNTIME_ASSERT(month >= 1 && month <= 12);
    RUNTIME_ASSERT(day >= 1 && day <= 31);
    RUNTIME_ASSERT(hour >= 0 && hour <= 23);
    RUNTIME_ASSERT(minute >= 0 && minute <= 59);
    RUNTIME_ASSERT(second >= 0 && second <= 59);

    SetTimeMultiplier(multiplier);
    SetYear(year);
    SetMonth(month);
    SetDay(day);
    SetHour(hour);
    SetMinute(minute);
    SetSecond(second);

    GameTime.Reset(year, month, day, hour, minute, second, multiplier);

    for (auto&& [id, player] : EntityMngr.GetPlayers()) {
        player->Send_TimeSync();
    }
}

void FOServer::LogToClients(string_view str)
{
    STACK_TRACE_ENTRY();

    if (!str.empty() && str.back() == '\n') {
        _logLines.emplace_back(str, 0, str.length() - 1);
    }
    else {
        _logLines.emplace_back(str);
    }
}

void FOServer::DispatchLogToClients()
{
    STACK_TRACE_ENTRY();

    if (_logLines.empty()) {
        return;
    }

    for (const auto& str : _logLines) {
        for (auto it = _logClients.begin(); it < _logClients.end();) {
            if (auto* player = *it; !player->IsDestroyed()) {
                player->Send_TextEx(ident_t {}, str, SAY_NETMSG, false);
                ++it;
            }
            else {
                player->Release();
                it = _logClients.erase(it);
            }
        }
    }

    if (_logClients.empty()) {
        SetLogCallback("LogToClients", nullptr);
    }

    _logLines.clear();
}

void FOServer::ProcessCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

#if FO_SINGLEPLAYER
    if (GameTime.IsGamePaused()) {
        return;
    }
#endif

    if (cr->IsDestroyed()) {
        return;
    }

    // Moving
    ProcessMove(cr);

    if (cr->IsDestroyed()) {
        return;
    }

    // Idle functions
    OnCritterIdle.Fire(cr);

    if (cr->IsDestroyed()) {
        return;
    }

    CrMngr.ProcessTalk(cr, false);

    if (cr->IsDestroyed()) {
        return;
    }

    // Cache look distance
    // Todo: disable look distance caching
    if (GameTime.GameTick() >= cr->CacheValuesNextTick) {
        cr->LookCacheValue = cr->GetLookDistance();
        cr->CacheValuesNextTick = GameTime.GameTick() + 3000;
    }

    // Time events
    cr->ProcessTimeEvents();

    if (cr->IsDestroyed()) {
        return;
    }

    // Remove player critter from game
    if (cr->IsOwnedByPlayer() && cr->GetOwner() == nullptr && cr->IsAlive() && cr->GetTimeoutRemoveFromGame() == 0 && cr->GetOfflineTime() >= Settings.MinimumOfflineTime) {
        LogoutCritter(cr);
    }
}

void FOServer::LogoutCritter(Critter* cr)
{
    STACK_TRACE_ENTRY();

    WriteLog("Logout critter {}", cr->GetName());

    cr->Broadcast_Action(ACTION_DISCONNECT, 0, nullptr);

    auto* map = MapMngr.GetMap(cr->GetMapId());
    MapMngr.EraseCrFromMap(cr, map);

    auto& inv_items = cr->GetRawItems();

    const auto remove_item = [this](Item* item) {
        if (item->GetIsRadio()) {
            ItemMngr.UnregisterRadio(item);
        }

        EntityMngr.UnregisterEntity(item, false);
        item->MarkAsDestroyed();
        item->Release();
    };

    std::function<void(Item*)> remove_sub_items;
    remove_sub_items = [&remove_sub_items, &remove_item](Item* cont) {
        auto& sub_items = cont->ContGetRawItems();

        for (auto* sub_item : sub_items) {
            if (sub_item->ContIsItems()) {
                remove_sub_items(sub_item);
            }
        }

        for (auto* sub_item : sub_items) {
            remove_item(sub_item);
        }

        sub_items.clear();
    };

    for (auto* item : inv_items) {
        if (item->ContIsItems()) {
            remove_sub_items(item);
        }

        remove_item(item);
    }

    inv_items.clear();

    EntityMngr.UnregisterEntity(cr);
    cr->MarkAsDestroyed();
    cr->Release();
}

void FOServer::VerifyTrigger(Map* map, Critter* cr, uint16 from_hx, uint16 from_hy, uint16 to_hx, uint16 to_hy, uint8 dir)
{
    STACK_TRACE_ENTRY();

    if (map->IsHexStaticTrigger(from_hx, from_hy)) {
        for (auto* item : map->GetStaticItemsTrigger(from_hx, from_hy)) {
            if (item->TriggerScriptFunc) {
                if (!item->TriggerScriptFunc(cr, item, false, dir)) {
                    // Nop
                }

                if (cr->IsDestroyed()) {
                    return;
                }
            }

            OnStaticItemWalk.Fire(item, cr, false, dir);

            if (cr->IsDestroyed()) {
                return;
            }
        }
    }

    if (map->IsHexStaticTrigger(to_hx, to_hy)) {
        for (auto* item : map->GetStaticItemsTrigger(to_hx, to_hy)) {
            if (item->TriggerScriptFunc) {
                if (!item->TriggerScriptFunc(cr, item, true, dir)) {
                    // Nop
                }

                if (cr->IsDestroyed()) {
                    return;
                }
            }

            OnStaticItemWalk.Fire(item, cr, true, dir);

            if (cr->IsDestroyed()) {
                return;
            }
        }
    }

    if (map->IsHexTrigger(from_hx, from_hy)) {
        for (auto* item : map->GetItemsTrigger(from_hx, from_hy)) {
            if (item->IsDestroyed()) {
                continue;
            }

            item->OnCritterWalk.Fire(cr, false, dir);

            if (cr->IsDestroyed()) {
                return;
            }
        }
    }

    if (map->IsHexTrigger(to_hx, to_hy)) {
        for (auto* item : map->GetItemsTrigger(to_hx, to_hy)) {
            if (item->IsDestroyed()) {
                continue;
            }

            item->OnCritterWalk.Fire(cr, true, dir);

            if (cr->IsDestroyed()) {
                return;
            }
        }
    }
}

void FOServer::Process_Handshake(ClientConnection* connection)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    // Net protocol
    uint proto_ver = 0;
    connection->InBuf >> proto_ver;
    const auto outdated = (proto_ver != static_cast<uint>(FO_COMPATIBILITY_VERSION));

    // Begin data encrypting
    uint encrypt_key = 0;
    connection->InBuf >> encrypt_key;

    uint8 padding[28] = {};
    connection->InBuf.Pop(padding, sizeof(padding));
    UNUSED_VARIABLE(padding);

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    connection->InBuf.SetEncryptKey(encrypt_key);
    connection->OutBuf.SetEncryptKey(encrypt_key);

    vector<uint8*>* global_vars_data = nullptr;
    vector<uint>* global_vars_data_sizes = nullptr;
    if (!outdated) {
        StoreData(false, &global_vars_data, &global_vars_data_sizes);
    }

    CONNECTION_OUTPUT_BEGIN(connection);
    connection->OutBuf.StartMsg(NETMSG_UPDATE_FILES_LIST);
    connection->OutBuf << outdated;
    connection->OutBuf << static_cast<uint>(_updateFilesDesc.size());
    connection->OutBuf.Push(_updateFilesDesc.data(), _updateFilesDesc.size());
    if (!outdated) {
        NET_WRITE_PROPERTIES(connection->OutBuf, global_vars_data, global_vars_data_sizes);
    }
    connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(connection);

    connection->WasHandshake = true;
}

void FOServer::Process_Ping(ClientConnection* connection)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    uint8 ping = 0;

    connection->InBuf >> ping;

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    if (ping == PING_CLIENT) {
        connection->PingOk = true;
        connection->PingNextTick = GameTime.FrameTick() + PING_CLIENT_LIFE_TIME;
        return;
    }

    if (ping != PING_SERVER) {
        WriteLog("Unknown ping {}, client host '{}'", ping, connection->GetHost());
        return;
    }

    CONNECTION_OUTPUT_BEGIN(connection);
    connection->OutBuf.StartMsg(NETMSG_PING);
    connection->OutBuf << ping;
    connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(connection);
}

void FOServer::Process_UpdateFile(ClientConnection* connection)
{
    STACK_TRACE_ENTRY();

    uint file_index = 0;
    connection->InBuf >> file_index;

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    if (file_index >= static_cast<uint>(_updateFilesData.size())) {
        WriteLog("Wrong file index {}, from host '{}'", file_index, connection->GetHost());
        connection->GracefulDisconnect();
        return;
    }

    connection->UpdateFileIndex = static_cast<int>(file_index);
    connection->UpdateFilePortion = 0;

    Process_UpdateFileData(connection);
}

void FOServer::Process_UpdateFileData(ClientConnection* connection)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    if (connection->UpdateFileIndex == -1) {
        WriteLog("Wrong update call, client host '{}'", connection->GetHost());
        connection->GracefulDisconnect();
        return;
    }

    const auto& update_file_data = _updateFilesData[connection->UpdateFileIndex];
    uint update_portion = Settings.UpdateFileSendSize;
    const auto offset = connection->UpdateFilePortion * update_portion;

    if (offset + update_portion < update_file_data.size()) {
        connection->UpdateFilePortion++;
    }
    else {
        update_portion = update_file_data.size() % update_portion;
        connection->UpdateFileIndex = -1;
    }

    CONNECTION_OUTPUT_BEGIN(connection);
    connection->OutBuf.StartMsg(NETMSG_UPDATE_FILE_DATA);
    connection->OutBuf << update_portion;
    connection->OutBuf.Push(&update_file_data[offset], update_portion);
    connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(connection);
}

void FOServer::Process_Register(Player* unlogined_player)
{
    STACK_TRACE_ENTRY();

    uint msg_len = 0;
    unlogined_player->Connection->InBuf >> msg_len;
    string name;
    unlogined_player->Connection->InBuf >> name;
    string password;
    unlogined_player->Connection->InBuf >> password;

    CHECK_CLIENT_IN_BUF_ERROR(unlogined_player->Connection);

    // Check data
    if (!_str(name).isValidUtf8() || name.find('*') != string::npos) {
        unlogined_player->Send_TextMsg(nullptr, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }

    // Check name length
    const auto name_len_utf8 = _str(name).lengthUtf8();
    if (name_len_utf8 < Settings.MinNameLength || name_len_utf8 > Settings.MaxNameLength) {
        unlogined_player->Send_TextMsg(nullptr, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }

    // Check for exist
    const auto player_id = MakePlayerId(name);
    if (DbStorage.Valid("Players", player_id)) {
        unlogined_player->Send_TextMsg(nullptr, STR_NET_PLAYER_ALREADY, SAY_NETMSG, TEXTMSG_GAME);
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }

    // Check brute force registration
    if (Settings.RegistrationTimeout != 0u) {
        auto ip = unlogined_player->Connection->GetIp();
        const auto reg_tick = Settings.RegistrationTimeout * 1000;
        if (const auto it = _regIp.find(ip); it != _regIp.end()) {
            auto& last_reg = it->second;
            const auto tick = GameTime.FrameTick();
            if (tick - last_reg < reg_tick) {
                unlogined_player->Send_TextMsg(nullptr, STR_NET_REGISTRATION_IP_WAIT, SAY_NETMSG, TEXTMSG_GAME);
                unlogined_player->Send_TextMsgLex(nullptr, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, _str("$time{}", (reg_tick - (tick - last_reg)) / 60000 + 1));
                unlogined_player->Connection->GracefulDisconnect();
                return;
            }
            last_reg = tick;
        }
        else {
            _regIp.insert(std::make_pair(ip, GameTime.FrameTick()));
        }
    }

#if !FO_SINGLEPLAYER
    uint disallow_msg_num = 0;
    uint disallow_str_num = 0;
    string lexems;
    const auto allow = OnPlayerRegistration.Fire(unlogined_player->Connection->GetIp(), name, disallow_msg_num, disallow_str_num, lexems);
    if (!allow) {
        if (disallow_msg_num < TEXTMSG_COUNT && (disallow_str_num != 0u)) {
            unlogined_player->Send_TextMsgLex(nullptr, disallow_str_num, SAY_NETMSG, TEXTMSG_GAME, lexems);
        }
        else {
            unlogined_player->Send_TextMsg(nullptr, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        }
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }
#endif

    // Register
    auto reg_ip = AnyData::Array();
    reg_ip.emplace_back(static_cast<int>(unlogined_player->Connection->GetIp()));
    auto reg_port = AnyData::Array();
    reg_port.emplace_back(static_cast<int>(unlogined_player->Connection->GetPort()));

    DbStorage.Insert("Players", player_id, {{"_Name", name}, {"Password", password}, {"ConnectionIp", reg_ip}, {"ConnectionPort", reg_port}});

    WriteLog("Registered player {} with id {}", name, player_id);

    // Notify
    unlogined_player->Send_TextMsg(nullptr, STR_NET_REG_SUCCESS, SAY_NETMSG, TEXTMSG_GAME);

    CONNECTION_OUTPUT_BEGIN(unlogined_player->Connection);
    unlogined_player->Connection->OutBuf.StartMsg(NETMSG_REGISTER_SUCCESS);
    unlogined_player->Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(unlogined_player->Connection);
}

void FOServer::Process_Login(Player* unlogined_player)
{
    STACK_TRACE_ENTRY();

    uint msg_len = 0;
    unlogined_player->Connection->InBuf >> msg_len;
    string name;
    unlogined_player->Connection->InBuf >> name;
    string password;
    unlogined_player->Connection->InBuf >> password;

    CHECK_CLIENT_IN_BUF_ERROR(unlogined_player->Connection);

    // If only cache checking than disconnect
    if (name.empty()) {
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }

    // Check for null in login/password
    if (name.find('\0') != string::npos || password.find('\0') != string::npos) {
        unlogined_player->Send_TextMsg(nullptr, STR_NET_WRONG_LOGIN, SAY_NETMSG, TEXTMSG_GAME);
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }

    // Check valid symbols in name
    if (!_str(name).isValidUtf8() || name.find('*') != string::npos) {
        unlogined_player->Send_TextMsg(nullptr, STR_NET_WRONG_DATA, SAY_NETMSG, TEXTMSG_GAME);
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }

    // Check for name length
    const auto name_len_utf8 = _str(name).lengthUtf8();
    if (name_len_utf8 < Settings.MinNameLength || name_len_utf8 > Settings.MaxNameLength) {
        unlogined_player->Send_TextMsg(nullptr, STR_NET_WRONG_LOGIN, SAY_NETMSG, TEXTMSG_GAME);
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }

    // Check password
    const auto player_id = MakePlayerId(name);
    auto player_doc = DbStorage.Get("Players", player_id);
    if (player_doc.count("Password") == 0u || player_doc["Password"].index() != AnyData::STRING_VALUE || std::get<string>(player_doc["Password"]).length() != password.length() || std::get<string>(player_doc["Password"]) != password) {
        unlogined_player->Send_TextMsg(nullptr, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME);
        unlogined_player->Connection->GracefulDisconnect();
        return;
    }

#if !FO_SINGLEPLAYER
    // Request script
    {
        uint disallow_msg_num = 0;
        uint disallow_str_num = 0;
        string lexems;
        if (const auto allow = OnPlayerLogin.Fire(unlogined_player->Connection->GetIp(), name, player_id, disallow_msg_num, disallow_str_num, lexems); !allow) {
            if (disallow_msg_num < TEXTMSG_COUNT && (disallow_str_num != 0u)) {
                unlogined_player->Send_TextMsgLex(nullptr, disallow_str_num, SAY_NETMSG, TEXTMSG_GAME, lexems);
            }
            else {
                unlogined_player->Send_TextMsg(nullptr, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME);
            }
            unlogined_player->Connection->GracefulDisconnect();
            return;
        }
    }
#endif

    // Switch from unlogined to logined
    Player* player = EntityMngr.GetPlayer(player_id);
    bool player_reconnected;

    if (player == nullptr) {
        player_reconnected = false;

        if (!PropertiesSerializator::LoadFromDocument(&unlogined_player->GetPropertiesForEdit(), player_doc, *this)) {
            unlogined_player->Send_TextMsg(nullptr, STR_NET_WRONG_DATA, SAY_NETMSG, TEXTMSG_GAME);
            unlogined_player->Connection->GracefulDisconnect();
            return;
        }

        const auto it = std::find(_unloginedPlayers.begin(), _unloginedPlayers.end(), unlogined_player);
        RUNTIME_ASSERT(it != _unloginedPlayers.end());
        _unloginedPlayers.erase(it);

        player = unlogined_player;
        EntityMngr.RegisterEntity(player, player_id);
        player->SetName(name);

        WriteLog("Connected player {}", name);

        OnPlayerInit.Fire(player);
    }
    else {
        player_reconnected = true;

        // Kick previous
        std::swap(player->Connection, unlogined_player->Connection);

        const auto it = std::find(_unloginedPlayers.begin(), _unloginedPlayers.end(), unlogined_player);
        RUNTIME_ASSERT(it != _unloginedPlayers.end());
        _unloginedPlayers.erase(it);

        unlogined_player->Connection->HardDisconnect();
        unlogined_player->MarkAsDestroyed();
        unlogined_player->Release();

        WriteLog("Reconnected player {}", name);
    }

    // Connection info
    {
        const auto ip = player->Connection->GetIp();
        const auto port = player->Connection->GetPort();

        auto conn_ip = player->GetConnectionIp();
        auto conn_port = player->GetConnectionPort();
        RUNTIME_ASSERT(conn_ip.size() == conn_port.size());

        auto ip_found = false;
        for (uint i = 0; i < conn_ip.size(); i++) {
            if (conn_ip[i] == ip) {
                if (i < conn_ip.size() - 1) {
                    conn_ip.erase(conn_ip.begin() + i);
                    conn_ip.push_back(ip);
                    player->SetConnectionIp(conn_ip);
                    conn_port.erase(conn_port.begin() + i);
                    conn_port.push_back(port);
                    player->SetConnectionPort(conn_port);
                }
                else if (conn_port.back() != port) {
                    conn_port.back() = port;
                    player->SetConnectionPort(conn_port);
                }
                ip_found = true;
                break;
            }
        }

        if (!ip_found) {
            conn_ip.push_back(ip);
            player->SetConnectionIp(conn_ip);
            conn_port.push_back(port);
            player->SetConnectionPort(conn_port);
        }
    }

    // Login ok
    const auto encrypt_key = NetBuffer::GenerateEncryptKey();

    vector<uint8*>* global_vars_data = nullptr;
    vector<uint>* global_vars_data_sizes = nullptr;
    StoreData(false, &global_vars_data, &global_vars_data_sizes);

    vector<uint8*>* player_data = nullptr;
    vector<uint>* player_data_sizes = nullptr;
    player->StoreData(true, &player_data, &player_data_sizes);

    CONNECTION_OUTPUT_BEGIN(player->Connection);
    player->Connection->OutBuf.StartMsg(NETMSG_LOGIN_SUCCESS);
    player->Connection->OutBuf << encrypt_key;
    player->Connection->OutBuf << player_id;
    NET_WRITE_PROPERTIES(player->Connection->OutBuf, global_vars_data, global_vars_data_sizes);
    NET_WRITE_PROPERTIES(player->Connection->OutBuf, player_data, player_data_sizes);
    player->Connection->OutBuf.EndMsg();
    CONNECTION_OUTPUT_END(player->Connection);

    player->Connection->OutBuf.SetEncryptKey(encrypt_key);

    // Attach critter
    bool critter_reconnected = false;

    if (!player_reconnected) {
        Critter* cr = nullptr;

        const auto owned_critter_ids = player->GetOwnedCritterIds();
        const auto cr_id = !owned_critter_ids.empty() ? owned_critter_ids.front() : ident_t {};

        // Try find in game
        if (cr_id) {
            cr = CrMngr.GetCritter(cr_id);
            if (cr != nullptr) {
                RUNTIME_ASSERT(cr->IsOwnedByPlayer());
                cr->AttachPlayer(player);
                player->SetOwnedCritter(cr);
                critter_reconnected = true;

                WriteLog("Critter for player found in game");
            }
        }

        // Try load critter from data base
        if (cr == nullptr && cr_id) {
            bool is_error = false;
            cr = EntityMngr.LoadCritter(cr_id, player, is_error);
            if (is_error) {
                if (cr != nullptr) {
                    cr->Release();
                }
                player->Send_TextMsg(nullptr, STR_NET_WRONG_DATA, SAY_NETMSG, TEXTMSG_GAME);
                player->Connection->GracefulDisconnect();
                return;
            }

            cr->SetMapId(ident_t {});
            cr->SetGlobalMapLeaderId(ident_t {});

            player->SetOwnedCritter(cr);

            WriteLog("Critter for player loaded from data base");

            const auto can_add_to_global_map = MapMngr.CanAddCrToMap(cr, nullptr, 0, 0, ident_t {});
            RUNTIME_ASSERT(can_add_to_global_map);
            MapMngr.AddCrToMap(cr, nullptr, 0, 0, 0, ident_t {});

            cr->Send_TimeSync();
            cr->Send_LoadMap(nullptr);

            EntityMngr.CallInit(cr, false);
        }

        // Create new critter
        if (cr == nullptr) {
            const auto* proto = ProtoMngr.GetProtoCritter(ToHashedString("Player"));
            RUNTIME_ASSERT(proto);

            cr = new Critter(this, ident_t {}, player, proto);

            EntityMngr.RegisterEntity(cr);
            player->SetOwnedCritterIds({cr->GetId()});
            player->SetOwnedCritter(cr);

            WriteLog("Critter for player created from scratch");

            const auto can_add_to_global_map = MapMngr.CanAddCrToMap(cr, nullptr, 0, 0, ident_t {});
            RUNTIME_ASSERT(can_add_to_global_map);
            MapMngr.AddCrToMap(cr, nullptr, 0, 0, 0, ident_t {});

            cr->Send_TimeSync();
            cr->Send_LoadMap(nullptr);

            EntityMngr.CallInit(cr, true);
        }
    }
    else {
        critter_reconnected = true;
    }

    if (critter_reconnected) {
        Critter* cr = player->GetOwnedCritter();

        cr->Send_TimeSync();

        if (cr->ViewMapId) {
            auto* map = MapMngr.GetMap(cr->ViewMapId);
            cr->ViewMapId = ident_t {};
            if (map != nullptr) {
                MapMngr.ViewMap(cr, map, cr->ViewMapLook, cr->ViewMapHx, cr->ViewMapHy, cr->ViewMapDir);
                cr->Send_ViewMap();
                return;
            }
        }

        auto* map = MapMngr.GetMap(cr->GetMapId());
        cr->Send_LoadMap(map);
        cr->Broadcast_Action(ACTION_CONNECT, 0, nullptr);

        if (!cr->GetMapId()) {
            RUNTIME_ASSERT(cr->GlobalMapGroup);

            cr->Send_GlobalInfo(GM_INFO_ALL);
            cr->Send_AllAutomapsInfo();
        }
        else {
            RUNTIME_ASSERT(map);

            // Send chosen
            cr->Send_AddCritter(cr);
            cr->Send_AddAllItems();
            cr->Send_AllAutomapsInfo();

            // Send current critters
            for (auto* visible_cr : cr->VisCrSelf) {
                cr->Send_AddCritter(visible_cr);
            }

            // Send current items on map
            for (const auto item_id : cr->VisItem) {
                if (auto* item = ItemMngr.GetItem(item_id); item != nullptr) {
                    cr->Send_AddItemOnMap(item);
                }
            }
        }

        // Check active talk
        if (cr->Talk.Type != TalkType::None) {
            CrMngr.ProcessTalk(cr, true);
            cr->Send_Talk();
        }

        // Notify about end of placing
        cr->Send_PlaceToGameComplete();
    }
}

void FOServer::Process_Move(Player* player)
{
    STACK_TRACE_ENTRY();

    uint msg_len;
    ident_t map_id;
    ident_t cr_id;
    uint16 speed;
    uint16 start_hx;
    uint16 start_hy;
    uint16 steps_count;
    vector<uint8> steps;
    uint16 control_steps_count;
    vector<uint16> control_steps;
    int16 end_hex_ox;
    int16 end_hex_oy;

    player->Connection->InBuf >> msg_len;
    player->Connection->InBuf >> map_id;
    player->Connection->InBuf >> cr_id;
    player->Connection->InBuf >> speed;
    player->Connection->InBuf >> start_hx;
    player->Connection->InBuf >> start_hy;
    player->Connection->InBuf >> steps_count;
    steps.resize(steps_count);
    for (auto i = 0; i < steps_count; i++) {
        player->Connection->InBuf >> steps[i];
    }
    player->Connection->InBuf >> control_steps_count;
    control_steps.resize(control_steps_count);
    for (auto i = 0; i < control_steps_count; i++) {
        player->Connection->InBuf >> control_steps[i];
    }
    player->Connection->InBuf >> end_hex_ox;
    player->Connection->InBuf >> end_hex_oy;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    auto* map = MapMngr.GetMap(map_id);
    if (map == nullptr) {
        BreakIntoDebugger();
        return;
    }

    Critter* cr = map->GetCritter(cr_id);
    if (cr == nullptr) {
        BreakIntoDebugger();
        return;
    }

    if (speed == 0) {
        BreakIntoDebugger();
        player->Send_Position(cr);
        return;
    }

    // Todo: validate moving steps programmaticaly

    uint checked_speed = speed;
    if (!OnPlayerCheckMove.Fire(player, cr, checked_speed)) {
        BreakIntoDebugger();
        player->Send_Position(cr);
        return;
    }

    MoveCritter(cr, static_cast<uint16>(checked_speed), start_hx, start_hy, steps, control_steps, end_hex_ox, end_hex_oy, false);
}

void FOServer::Process_StopMove(Player* player)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    ident_t map_id;
    ident_t cr_id;
    uint16 start_hx;
    uint16 start_hy;
    int16 hex_ox;
    int16 hex_oy;
    int16 dir_angle;

    player->Connection->InBuf >> map_id;
    player->Connection->InBuf >> cr_id;
    player->Connection->InBuf >> start_hx;
    player->Connection->InBuf >> start_hy;
    player->Connection->InBuf >> hex_ox;
    player->Connection->InBuf >> hex_oy;
    player->Connection->InBuf >> dir_angle;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    auto* map = MapMngr.GetMap(map_id);
    if (map == nullptr) {
        BreakIntoDebugger();
        return;
    }

    Critter* cr = map->GetCritter(cr_id);
    if (cr == nullptr) {
        BreakIntoDebugger();
        return;
    }

    uint zero_speed = 0;
    if (!OnPlayerCheckMove.Fire(player, cr, zero_speed)) {
        BreakIntoDebugger();
        player->Send_Position(cr);
        return;
    }

    cr->ClearMove();
    cr->Broadcast_Move();
}

void FOServer::Process_Dir(Player* player)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    ident_t map_id;
    ident_t cr_id;
    int16 dir_angle;

    player->Connection->InBuf >> map_id;
    player->Connection->InBuf >> cr_id;
    player->Connection->InBuf >> dir_angle;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    auto* map = MapMngr.GetMap(map_id);
    if (map == nullptr) {
        BreakIntoDebugger();
        return;
    }

    Critter* cr = map->GetCritter(cr_id);
    if (cr == nullptr) {
        BreakIntoDebugger();
        return;
    }

    int16 checked_dir_angle = dir_angle;
    if (!OnPlayerCheckDir.Fire(player, cr, checked_dir_angle)) {
        BreakIntoDebugger();
        player->Send_Dir(cr);
        return;
    }

    cr->ChangeDirAngle(checked_dir_angle);
    cr->Broadcast_Dir();
}

void FOServer::Process_Property(Player* player, uint data_size)
{
    STACK_TRACE_ENTRY();

    Critter* cr = player->GetOwnedCritter();

    uint msg_len;
    ident_t cr_id;
    ident_t item_id;
    uint16 property_index;

    if (data_size == 0) {
        player->Connection->InBuf >> msg_len;
    }

    char type_ = 0;
    player->Connection->InBuf >> type_;
    const auto type = static_cast<NetProperty>(type_);

    uint additional_args = 0;
    switch (type) {
    case NetProperty::CritterItem:
        additional_args = 2;
        player->Connection->InBuf >> cr_id;
        player->Connection->InBuf >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        player->Connection->InBuf >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        player->Connection->InBuf >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        player->Connection->InBuf >> item_id;
        break;
    default:
        break;
    }

    player->Connection->InBuf >> property_index;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    PropertyRawData prop_data;

    if (data_size != 0) {
        player->Connection->InBuf.Pop(prop_data.Alloc(data_size), data_size);
    }
    else {
        const uint len = msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(uint16);
        // Todo: control max size explicitly, add option to property registration
        if (len > 0xFFFF) {
            // For now 64Kb for all
            BreakIntoDebugger("len > 0xFFFF");
            return;
        }

        if (len != 0u) {
            player->Connection->InBuf.Pop(prop_data.Alloc(len), len);
        }
    }

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    auto is_public = false;
    const Property* prop = nullptr;
    Entity* entity = nullptr;
    switch (type) {
    case NetProperty::Game:
        is_public = true;
        prop = GetPropertyRegistrator(GameProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            entity = this;
        }
        break;
    case NetProperty::Player:
        prop = GetPropertyRegistrator(PlayerProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            entity = player;
        }
        break;
    case NetProperty::Critter:
        is_public = true;
        prop = GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            entity = CrMngr.GetCritter(cr_id);
        }
        break;
    case NetProperty::Chosen:
        prop = GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            entity = cr;
        }
        break;
    case NetProperty::MapItem:
        is_public = true;
        prop = GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            entity = ItemMngr.GetItem(item_id);
        }
        break;
    case NetProperty::CritterItem:
        is_public = true;
        prop = GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            auto* cr = CrMngr.GetCritter(cr_id);
            if (cr != nullptr) {
                entity = cr->GetItem(item_id, true);
            }
        }
        break;
    case NetProperty::ChosenItem:
        prop = GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            entity = cr->GetItem(item_id, true);
        }
        break;
    case NetProperty::Map:
        is_public = true;
        prop = GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            entity = MapMngr.GetMap(cr->GetMapId());
        }
        break;
    case NetProperty::Location:
        is_public = true;
        prop = GetPropertyRegistrator(LocationProperties::ENTITY_CLASS_NAME)->GetByIndex(property_index);
        if (prop != nullptr) {
            auto* map = MapMngr.GetMap(cr->GetMapId());
            if (map != nullptr) {
                entity = map->GetLocation();
            }
        }
        break;
    default:
        break;
    }
    if (prop == nullptr || entity == nullptr) {
        return;
    }

    if (prop->IsDisabled()) {
        BreakIntoDebugger();
        return;
    }
    if (prop->IsVirtual()) {
        BreakIntoDebugger();
        return;
    }

    const auto access = prop->GetAccess();
    if (is_public && !IsEnumSet(access, Property::AccessType::PublicMask)) {
        BreakIntoDebugger();
        return;
    }
    if (!is_public && !IsEnumSet(access, Property::AccessType::ProtectedMask) && !IsEnumSet(access, Property::AccessType::PublicMask)) {
        BreakIntoDebugger();
        return;
    }
    if (!IsEnumSet(access, Property::AccessType::ModifiableMask)) {
        BreakIntoDebugger();
        return;
    }
    if (is_public && access != Property::AccessType::PublicFullModifiable) {
        BreakIntoDebugger();
        return;
    }

    if (prop->IsPlainData() && data_size != prop->GetBaseSize()) {
        BreakIntoDebugger();
        return;
    }
    if (!prop->IsPlainData() && data_size != 0) {
        BreakIntoDebugger();
        return;
    }

    {
        player->SendIgnoreEntity = entity;
        player->SendIgnoreProperty = prop;

        auto revert_send_ignore = ScopeCallback([player]() noexcept {
            player->SendIgnoreEntity = nullptr;
            player->SendIgnoreProperty = nullptr;
        });

        // Todo: verify property data from client
        entity->SetValueFromData(prop, prop_data);
    }
}

void FOServer::OnSaveEntityValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    const auto* server_entity = dynamic_cast<ServerEntity*>(entity);

    ident_t entry_id;

    if (server_entity != nullptr) {
        entry_id = server_entity->GetId();

        if (!entry_id) {
            return;
        }
    }
    else {
        entry_id = ident_t {1};
    }

    auto&& value = PropertiesSerializator::SavePropertyToValue(&entity->GetProperties(), prop, *this);

    if (server_entity != nullptr) {
        DbStorage.Update(_str("{}s", server_entity->GetClassName()), entry_id, prop->GetName(), value);
    }
    else {
        DbStorage.Update(entity->GetClassName(), entry_id, prop->GetName(), value);
    }

    if (prop->IsHistorical()) {
        const auto history_id_num = GetHistoryRecordsId().underlying_value() + 1;
        const auto history_id = ident_t {history_id_num};

        SetHistoryRecordsId(history_id);

        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

        AnyData::Document doc;
        doc["Time"] = static_cast<int64>(time.count());
        doc["EntityClass"] = string(entity->GetClassName());
        doc["EntityId"] = static_cast<int64>(entry_id.underlying_value());
        doc["Property"] = prop->GetName();
        doc["Value"] = value;

        DbStorage.Insert("History", history_id, doc);
    }
}

void FOServer::OnSendGlobalValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    if (IsEnumSet(prop->GetAccess(), Property::AccessType::PublicMask)) {
        for (auto&& [id, player] : EntityMngr.GetPlayers()) {
            player->Send_Property(NetProperty::Game, prop, this);
        }
    }
}

void FOServer::OnSendPlayerValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    auto* player = dynamic_cast<Player*>(entity);

    player->Send_Property(NetProperty::Player, prop, player);
}

void FOServer::OnSendCritterValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    auto* cr = dynamic_cast<Critter*>(entity);

    const auto is_public = IsEnumSet(prop->GetAccess(), Property::AccessType::PublicMask);
    const auto is_protected = IsEnumSet(prop->GetAccess(), Property::AccessType::ProtectedMask);

    if (is_public || is_protected) {
        cr->Send_Property(NetProperty::Chosen, prop, cr);
    }
    if (is_public) {
        cr->Broadcast_Property(NetProperty::Critter, prop, cr);
    }
}

void FOServer::OnSendItemValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    if (auto* item = dynamic_cast<Item*>(entity); item != nullptr && !item->IsStatic() && item->GetId()) {
        const auto is_public = IsEnumSet(prop->GetAccess(), Property::AccessType::PublicMask);
        const auto is_protected = IsEnumSet(prop->GetAccess(), Property::AccessType::ProtectedMask);

        switch (item->GetOwnership()) {
        case ItemOwnership::Nowhere: {
        } break;
        case ItemOwnership::CritterInventory: {
            if (is_public || is_protected) {
                auto* cr = CrMngr.GetCritter(item->GetCritterId());
                if (cr != nullptr) {
                    if (is_public || is_protected) {
                        cr->Send_Property(NetProperty::ChosenItem, prop, item);
                    }
                    if (is_public) {
                        cr->Broadcast_Property(NetProperty::CritterItem, prop, item);
                    }
                }
            }
        } break;
        case ItemOwnership::MapHex: {
            if (is_public) {
                auto* map = MapMngr.GetMap(item->GetMapId());
                if (map != nullptr) {
                    map->SendProperty(NetProperty::MapItem, prop, item);
                }
            }
        } break;
        case ItemOwnership::ItemContainer: {
            // Todo: add container properties changing notifications
            // Item* cont = ItemMngr.GetItem( item->GetContainerId() );
        } break;
        }
    }
}

void FOServer::OnSendMapValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    if (IsEnumSet(prop->GetAccess(), Property::AccessType::PublicMask)) {
        auto* map = dynamic_cast<Map*>(entity);
        map->SendProperty(NetProperty::Map, prop, map);
    }
}

void FOServer::OnSendLocationValue(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    if (IsEnumSet(prop->GetAccess(), Property::AccessType::PublicMask)) {
        auto* loc = dynamic_cast<Location*>(entity);
        for (auto* map : loc->GetMaps()) {
            map->SendProperty(NetProperty::Location, prop, loc);
        }
    }
}

void FOServer::OnSetItemCount(Entity* entity, const Property* prop, const void* new_value)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();
    UNUSED_VARIABLE(prop);

    const auto* item = dynamic_cast<Item*>(entity);
    const auto new_count = *static_cast<const uint*>(new_value);
    const auto old_count = entity->GetProperties().GetValue<uint>(prop);
    if (static_cast<int>(new_count) > 0 && (item->GetStackable() || new_count == 1)) {
        const auto diff = static_cast<int>(item->GetCount()) - static_cast<int>(old_count);
        ItemMngr.ChangeItemStatistics(item->GetProtoId(), diff);
    }
    else {
        if (!item->GetStackable()) {
            throw GenericException("Trying to change count of not stackable item");
        }

        throw GenericException("Item count can't be zero or negative", new_count);
    }
}

void FOServer::OnSetItemChangeView(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    // IsHidden, IsAlwaysView, IsTrap, TrapValue
    auto* item = dynamic_cast<Item*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            map->ChangeViewItem(item);
            if (prop == item->GetPropertyIsTrap()) {
                map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
            }
        }
    }
    else if (item->GetOwnership() == ItemOwnership::CritterInventory) {
        auto* cr = CrMngr.GetCritter(item->GetCritterId());
        if (cr != nullptr) {
            if (item->GetIsHidden()) {
                cr->Send_EraseItem(item);
            }
            else {
                cr->Send_AddItem(item);
            }
            cr->SendAndBroadcast_MoveItem(item, ACTION_REFRESH, 0);
        }
    }
}

void FOServer::OnSetItemRecacheHex(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(prop);

    // IsNoBlock, IsShootThru, IsGag, IsTrigger
    const auto* item = dynamic_cast<Item*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
        }
    }
}

void FOServer::OnSetItemBlockLines(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    // BlockLines
    const auto* item = dynamic_cast<Item*>(entity);
    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            // Todo: make BlockLines changable in runtime
            throw NotImplementedException(LINE_STR);
        }
    }
}

void FOServer::OnSetItemIsGeck(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    const auto* item = dynamic_cast<Item*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            map->GetLocation()->GeckCount += item->GetIsGeck() ? 1 : -1;
        }
    }
}

void FOServer::OnSetItemIsRadio(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    auto* item = dynamic_cast<Item*>(entity);

    if (item->GetIsRadio()) {
        ItemMngr.RegisterRadio(item);
    }
    else {
        ItemMngr.UnregisterRadio(item);
    }
}

void FOServer::OnSetItemOpened(Entity* entity, const Property* prop)
{
    STACK_TRACE_ENTRY();

    auto* item = dynamic_cast<Item*>(entity);

    if (item->GetIsCanOpen()) {
        if (item->GetOpened()) {
            item->SetIsLightThru(true);

            if (item->GetOwnership() == ItemOwnership::MapHex) {
                auto* map = MapMngr.GetMap(item->GetMapId());
                if (map != nullptr) {
                    map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
                }
            }
        }
        else {
            item->SetIsLightThru(false);

            if (item->GetOwnership() == ItemOwnership::MapHex) {
                auto* map = MapMngr.GetMap(item->GetMapId());
                if (map != nullptr) {
                    map->SetHexFlag(item->GetHexX(), item->GetHexY(), FH_BLOCK_ITEM);
                    map->SetHexFlag(item->GetHexX(), item->GetHexY(), FH_NRAKE_ITEM);
                }
            }
        }
    }
}

void FOServer::ProcessMove(Critter* cr)
{
    STACK_TRACE_ENTRY();

    // Moving
    if (cr->IsMoving()) {
        auto* map = MapMngr.GetMap(cr->GetMapId());
        if (map != nullptr && cr->IsAlive()) {
            ProcessMoveBySteps(cr, map);

            if (cr->IsDestroyed()) {
                return;
            }
        }
        else {
            cr->ClearMove();
            cr->Send_Move(cr);
            cr->Broadcast_Move();
            cr->Send_Position(cr);
            cr->Broadcast_Position();
        }
    }

    // Check for path recalculation
    auto recalculate = false;

    if (cr->IsMoving() && cr->TargetMoving.TargId && (cr->TargetMoving.HexX != 0u || cr->TargetMoving.HexY != 0u)) {
        const auto* targ = cr->GetCrSelf(cr->TargetMoving.TargId);
        if (targ != nullptr && !Geometry.CheckDist(targ->GetHexX(), targ->GetHexY(), cr->TargetMoving.HexX, cr->TargetMoving.HexY, 0)) {
            recalculate = true;
        }
    }

    if (cr->TargetMoving.State == MovingState::InProgress || recalculate) {
        // Find path if not exist
        if (!cr->IsMoving() || recalculate) {
            uint16 hx;
            uint16 hy;
            uint cut;
            uint trace_dist;
            Critter* trace_cr;

            if (cr->TargetMoving.TargId) {
                Critter* targ = cr->GetCrSelf(cr->TargetMoving.TargId);
                if (targ == nullptr) {
                    cr->TargetMoving.State = MovingState::TargetNotFound;
                    cr->Broadcast_Position();
                    return;
                }

                hx = targ->GetHexX();
                hy = targ->GetHexY();
                cut = cr->TargetMoving.Cut;
                trace_dist = cr->TargetMoving.TraceDist;
                trace_cr = targ;

                cr->TargetMoving.HexX = hx;
                cr->TargetMoving.HexY = hy;
            }
            else {
                hx = cr->TargetMoving.HexX;
                hy = cr->TargetMoving.HexY;
                cut = cr->TargetMoving.Cut;
                trace_dist = 0;
                trace_cr = nullptr;
            }

            FindPathInput find_input;
            find_input.MapId = cr->GetMapId();
            find_input.FromCritter = cr;
            find_input.FromHexX = cr->GetHexX();
            find_input.FromHexY = cr->GetHexY();
            find_input.ToHexX = hx;
            find_input.ToHexY = hy;
            find_input.Multihex = cr->GetMultihex();
            find_input.Cut = cut;
            find_input.TraceDist = trace_dist;
            find_input.TraceCr = trace_cr;
            find_input.CheckCritter = true;
            find_input.CheckGagItems = true;

            if (cr->TargetMoving.Speed == 0) {
                cr->TargetMoving.State = MovingState::CantMove;
                return;
            }

            const auto find_path = MapMngr.FindPath(find_input);

            if (find_path.GagCritter != nullptr) {
                cr->TargetMoving.State = MovingState::GagCritter;
                cr->TargetMoving.GagEntityId = find_path.GagCritter->GetId();
                return;
            }

            if (find_path.GagItem != nullptr) {
                cr->TargetMoving.State = MovingState::GagItem;
                cr->TargetMoving.GagEntityId = find_path.GagItem->GetId();
                return;
            }

            // Failed
            if (find_path.Result != FindPathOutput::ResultType::Ok) {
                if (find_path.Result == FindPathOutput::ResultType::AlreadyHere) {
                    cr->TargetMoving.State = MovingState::Success;
                    return;
                }

                MovingState reason = {};
                switch (find_path.Result) {
                case FindPathOutput::ResultType::MapNotFound:
                    reason = MovingState::InternalError;
                    break;
                case FindPathOutput::ResultType::TooFar:
                    reason = MovingState::HexTooFar;
                    break;
                case FindPathOutput::ResultType::InternalError:
                    reason = MovingState::InternalError;
                    break;
                case FindPathOutput::ResultType::InvalidHexes:
                    reason = MovingState::InternalError;
                    break;
                case FindPathOutput::ResultType::TraceTargetNullptr:
                    reason = MovingState::InternalError;
                    break;
                case FindPathOutput::ResultType::HexBusy:
                    reason = MovingState::HexBusy;
                    break;
                case FindPathOutput::ResultType::HexBusyRing:
                    reason = MovingState::HexBusyRing;
                    break;
                case FindPathOutput::ResultType::Deadlock:
                    reason = MovingState::Deadlock;
                    break;
                case FindPathOutput::ResultType::TraceFailed:
                    reason = MovingState::TraceFailed;
                    break;
                case FindPathOutput::ResultType::Unknown:
                    reason = MovingState::InternalError;
                    break;
                case FindPathOutput::ResultType::Ok:
                    reason = MovingState::InternalError;
                    break;
                case FindPathOutput::ResultType::AlreadyHere:
                    reason = MovingState::InternalError;
                    break;
                }

                cr->TargetMoving.State = reason;
                return;
            }

            // Success
            cr->TargetMoving.State = MovingState::Success;
            MoveCritter(cr, cr->TargetMoving.Speed, cr->GetHexX(), cr->GetHexY(), find_path.Steps, find_path.ControlSteps, 0, 0, true);
        }
    }
}

void FOServer::ProcessMoveBySteps(Critter* cr, Map* map)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!cr->Moving.Steps.empty());
    RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
    RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
    RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

    const auto move_uid = cr->Moving.Uid;
    auto normalized_time = static_cast<float>(GameTime.FrameTick() - cr->Moving.StartTick) / cr->Moving.WholeTime;
    normalized_time = std::clamp(normalized_time, 0.0f, 1.0f);

    const auto dist_pos = cr->Moving.WholeDist * normalized_time;

    auto start_hx = cr->Moving.StartHexX;
    auto start_hy = cr->Moving.StartHexY;
    auto cur_dist = 0.0f;

    auto control_step_begin = 0;
    for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
        auto hx = start_hx;
        auto hy = start_hy;

        RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
        RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
        for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
            const auto move_ok = Geometry.MoveHexByDir(hx, hy, cr->Moving.Steps[j], map->GetWidth(), map->GetHeight());
            RUNTIME_ASSERT(move_ok);
        }

        auto&& [ox, oy] = Geometry.GetHexInterval(start_hx, start_hy, hx, hy);

        if (i == 0) {
            ox -= cr->Moving.StartOx;
            oy -= cr->Moving.StartOy;
        }
        if (i == cr->Moving.ControlSteps.size() - 1) {
            ox += cr->Moving.EndOx;
            oy += cr->Moving.EndOy;
        }

        const auto proj_oy = static_cast<float>(oy) * Geometry.GetYProj();
        auto dist = std::sqrt(static_cast<float>(ox * ox) + proj_oy * proj_oy);
        if (dist < 0.0001f) {
            dist = 0.0001f;
        }

        if ((normalized_time < 1.0f && dist_pos >= cur_dist && dist_pos <= cur_dist + dist) || (normalized_time == 1.0f && i == cr->Moving.ControlSteps.size() - 1)) {
            auto normalized_dist = (dist_pos - cur_dist) / dist;
            normalized_dist = std::clamp(normalized_dist, 0.0f, 1.0f);
            if (normalized_time == 1.0f) {
                normalized_dist = 1.0f;
            }

            // Evaluate current hex
            const auto step_index_f = std::round(normalized_dist * static_cast<float>(cr->Moving.ControlSteps[i] - control_step_begin));
            const auto step_index = control_step_begin + static_cast<int>(step_index_f);
            RUNTIME_ASSERT(step_index >= control_step_begin);
            RUNTIME_ASSERT(step_index <= cr->Moving.ControlSteps[i]);

            auto hx2 = start_hx;
            auto hy2 = start_hy;

            for (auto j2 = control_step_begin; j2 < step_index; j2++) {
                const auto move_ok = Geometry.MoveHexByDir(hx2, hy2, cr->Moving.Steps[j2], map->GetWidth(), map->GetHeight());
                RUNTIME_ASSERT(move_ok);
            }

            const auto old_hx = cr->GetHexX();
            const auto old_hy = cr->GetHexY();

            if (old_hx != hx2 || old_hy != hy2) {
                const uint8 dir = Geometry.GetFarDir(old_hx, old_hy, hx2, hy2);
                const uint multihex = cr->GetMultihex();

                if (map->IsMovePassed(cr, hx2, hy2, multihex)) {
                    const auto is_dead = cr->IsDead();
                    map->UnsetFlagCritter(old_hx, old_hy, multihex, is_dead);
                    cr->SetHexX(hx2);
                    cr->SetHexY(hy2);
                    map->SetFlagCritter(hx2, hy2, multihex, is_dead);

                    RUNTIME_ASSERT(!cr->IsDestroyed());
                    MapMngr.ProcessVisibleCritters(cr);
                    if (cr->Moving.Uid != move_uid || cr->IsDestroyed()) {
                        return;
                    }

                    MapMngr.ProcessVisibleItems(cr);
                    if (cr->Moving.Uid != move_uid || cr->IsDestroyed()) {
                        return;
                    }

                    VerifyTrigger(map, cr, old_hx, old_hy, hx2, hy2, dir);
                    if (cr->Moving.Uid != move_uid || cr->IsDestroyed()) {
                        return;
                    }
                }
                else if (map->IsHexBlockItem(hx2, hy2)) {
                    cr->ClearMove();
                    cr->Send_Position(cr);
                    cr->Broadcast_Position();
                    return;
                }
            }

            // Evaluate current position
            const auto cr_hx = cr->GetHexX();
            const auto cr_hy = cr->GetHexY();
            const auto moved = (cr_hx != old_hx || cr_hy != old_hy);

            auto&& [cr_ox, cr_oy] = Geometry.GetHexInterval(start_hx, start_hy, cr_hx, cr_hy);
            if (i == 0) {
                cr_ox -= cr->Moving.StartOx;
                cr_oy -= cr->Moving.StartOy;
            }

            const auto lerp = [](int a, int b, float t) { return static_cast<float>(a) * (1.0f - t) + static_cast<float>(b) * t; };

            auto mx = lerp(0, ox, normalized_dist);
            auto my = lerp(0, oy, normalized_dist);
            mx -= static_cast<float>(cr_ox);
            my -= static_cast<float>(cr_oy);

            const auto mxi = static_cast<int16>(std::round(mx));
            const auto myi = static_cast<int16>(std::round(my));
            if (moved || cr->GetHexOffsX() != mxi || cr->GetHexOffsY() != myi) {
                cr->SetHexOffsX(mxi);
                cr->SetHexOffsY(myi);
            }

            // Evaluate dir angle
            const auto dir_angle = Geometry.GetLineDirAngle(0, 0, ox, oy);
            cr->SetDirAngle(static_cast<int16>(dir_angle));

            break;
        }

        RUNTIME_ASSERT(i < cr->Moving.ControlSteps.size() - 1);

        control_step_begin = cr->Moving.ControlSteps[i];
        start_hx = hx;
        start_hy = hy;
        cur_dist += dist;
    }

    if (normalized_time == 1.0f) {
        if (cr->GetHexX() != cr->Moving.EndHexX || cr->GetHexY() != cr->Moving.EndHexY) {
            cr->Send_Position(cr);
            cr->Broadcast_Position();
        }

        cr->ClearMove();
    }
}

void FOServer::MoveCritter(Critter* cr, uint16 speed, uint16 start_hx, uint16 start_hy, const vector<uint8>& steps, const vector<uint16>& control_steps, int16 end_hex_ox, int16 end_hex_oy, bool send_self)
{
    STACK_TRACE_ENTRY();

    cr->ClearMove();

    const auto* map = MapMngr.GetMap(cr->GetMapId());
    if (map == nullptr) {
        return;
    }

    cr->Moving.Speed = speed;
    cr->Moving.StartTick = GameTime.FrameTick();
    cr->Moving.Steps = steps;
    cr->Moving.ControlSteps = control_steps;
    cr->Moving.StartHexX = start_hx;
    cr->Moving.StartHexY = start_hy;
    cr->Moving.StartOx = cr->GetHexOffsX();
    cr->Moving.StartOy = cr->GetHexOffsY();
    cr->Moving.EndOx = end_hex_ox;
    cr->Moving.EndOy = end_hex_oy;

    cr->Moving.WholeTime = 0.0f;
    cr->Moving.WholeDist = 0.0f;

    RUNTIME_ASSERT(cr->Moving.Speed > 0);
    const auto base_move_speed = static_cast<float>(cr->Moving.Speed);

    auto control_step_begin = 0;
    for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
        auto hx = start_hx;
        auto hy = start_hy;

        RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
        RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
        for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
            const auto move_ok = Geometry.MoveHexByDir(hx, hy, cr->Moving.Steps[j], map->GetWidth(), map->GetHeight());
            RUNTIME_ASSERT(move_ok);
        }

        auto&& [ox, oy] = Geometry.GetHexInterval(start_hx, start_hy, hx, hy);

        if (i == 0) {
            ox -= cr->Moving.StartOx;
            oy -= cr->Moving.StartOy;
        }
        if (i == cr->Moving.ControlSteps.size() - 1) {
            ox += cr->Moving.EndOx;
            oy += cr->Moving.EndOy;
        }

        const auto proj_oy = static_cast<float>(oy) * Geometry.GetYProj();
        const auto dist = std::sqrt(static_cast<float>(ox * ox) + proj_oy * proj_oy);

        cr->Moving.WholeTime += dist / base_move_speed * 1000.0f;
        cr->Moving.WholeDist += dist;

        control_step_begin = cr->Moving.ControlSteps[i];
        start_hx = hx;
        start_hy = hy;

        cr->Moving.EndHexX = hx;
        cr->Moving.EndHexY = hy;
    }

    if (cr->Moving.WholeTime < 0.0001f) {
        cr->Moving.WholeTime = 0.0001f;
    }
    if (cr->Moving.WholeDist < 0.0001f) {
        cr->Moving.WholeDist = 0.0001f;
    }

    RUNTIME_ASSERT(!cr->Moving.Steps.empty());
    RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
    RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
    RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

    if (send_self) {
        cr->Send_Move(cr);
    }

    cr->Broadcast_Move();
}

auto FOServer::DialogCompile(Critter* npc, Critter* cl, const Dialog& base_dlg, Dialog& compiled_dlg) -> bool
{
    STACK_TRACE_ENTRY();

    if (base_dlg.Id < 2) {
        WriteLog("Wrong dialog id {}", base_dlg.Id);
        return false;
    }

    compiled_dlg = base_dlg;

    for (auto it_a = compiled_dlg.Answers.begin(); it_a != compiled_dlg.Answers.end();) {
        if (!DialogCheckDemand(npc, cl, *it_a, false)) {
            it_a = compiled_dlg.Answers.erase(it_a);
        }
        else {
            ++it_a;
        }
    }

    if (!Settings.NoAnswerShuffle && !compiled_dlg.NoShuffle) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 rnd(rd());
        std::shuffle(compiled_dlg.Answers.begin(), compiled_dlg.Answers.end(), rnd);
    }

    return true;
}

auto FOServer::DialogCheckDemand(Critter* npc, Critter* cl, const DialogAnswer& answer, bool recheck) -> bool
{
    STACK_TRACE_ENTRY();

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

        switch (demand.Type) {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP: {
            const Entity* entity = nullptr;

            if (demand.Type == DR_PROP_GLOBAL) {
                entity = master;
            }
            else if (demand.Type == DR_PROP_CRITTER) {
                entity = master;
            }
            else if (demand.Type == DR_PROP_ITEM) {
                entity = master->GetItemSlot(1);
            }
            else if (demand.Type == DR_PROP_LOCATION) {
                auto* map = MapMngr.GetMap(master->GetMapId());
                entity = (map != nullptr ? map->GetLocation() : nullptr);
            }
            else if (demand.Type == DR_PROP_MAP) {
                entity = MapMngr.GetMap(master->GetMapId());
            }

            if (entity == nullptr) {
                break;
            }

            const auto val = entity->GetProperties().GetValueAsInt(static_cast<int>(demand.ParamIndex));

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
            const auto pid = demand.ParamHash;
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

auto FOServer::DialogUseResult(Critter* npc, Critter* cl, const DialogAnswer& answer) -> uint
{
    STACK_TRACE_ENTRY();

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

        switch (result.Type) {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP: {
            Entity* entity = nullptr;

            if (result.Type == DR_PROP_GLOBAL) {
                entity = master;
            }
            else if (result.Type == DR_PROP_CRITTER) {
                entity = master;
            }
            else if (result.Type == DR_PROP_ITEM) {
                entity = master->GetItemSlot(1);
            }
            else if (result.Type == DR_PROP_LOCATION) {
                auto* map = MapMngr.GetMap(master->GetMapId());
                entity = (map != nullptr ? map->GetLocation() : nullptr);
            }
            else if (result.Type == DR_PROP_MAP) {
                entity = MapMngr.GetMap(master->GetMapId());
            }

            if (entity == nullptr) {
                break;
            }

            auto val = entity->GetProperties().GetValueAsInt(static_cast<int>(result.ParamIndex));

            switch (result.Op) {
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

            entity->GetPropertiesForEdit().SetValueAsInt(static_cast<int>(result.ParamIndex), val);
        }
            continue;
        case DR_ITEM: {
            const auto pid = result.ParamHash;
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

void FOServer::BeginDialog(Critter* cl, Critter* npc, hstring dlg_pack_id, uint16 hx, uint16 hy, bool ignore_distance)
{
    STACK_TRACE_ENTRY();

    if (cl->Talk.Locked) {
        WriteLog("Dialog locked, client '{}'", cl->GetName());
        return;
    }
    if (cl->Talk.Type != TalkType::None) {
        CrMngr.CloseTalk(cl);
    }

    hstring selected_dlg_pack_id = dlg_pack_id;
    DialogPack* dialog_pack;
    vector<Dialog>* dialogs;

    // Talk with npc
    if (npc != nullptr) {
        if (npc->GetIsNoTalk()) {
            return;
        }

        if (!selected_dlg_pack_id) {
            const auto npc_dlg_id = npc->GetDialogId();
            if (!npc_dlg_id) {
                return;
            }

            selected_dlg_pack_id = npc_dlg_id;
        }

        if (!ignore_distance) {
            if (cl->GetMapId() != npc->GetMapId()) {
                WriteLog("Different maps, npc '{}' {}, client '{}' {}", npc->GetName(), npc->GetMapId(), cl->GetName(), cl->GetMapId());
                return;
            }

            auto talk_distance = npc->GetTalkDistance();
            talk_distance = (talk_distance != 0u ? talk_distance : Settings.TalkDistance) + cl->GetMultihex();
            if (!Geometry.CheckDist(cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY(), talk_distance)) {
                cl->Send_Position(cl);
                cl->Send_Position(npc);
                cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
                WriteLog("Wrong distance to npc '{}', client '{}'", npc->GetName(), cl->GetName());
                return;
            }

            auto* map = MapMngr.GetMap(cl->GetMapId());
            if (map == nullptr) {
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
                cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
                return;
            }
        }

        if (!npc->IsAlive()) {
            cl->Send_TextMsg(cl, STR_DIALOG_NPC_NOT_LIFE, SAY_NETMSG, TEXTMSG_GAME);
            WriteLog("Npc '{}' bad condition, client '{}'", npc->GetName(), cl->GetName());
            return;
        }

        if (!npc->IsFreeToTalk()) {
            cl->Send_TextMsg(cl, STR_DIALOG_MANY_TALKERS, SAY_NETMSG, TEXTMSG_GAME);
            return;
        }

        // Todo: don't remeber but need check (IsPlaneNoTalk)

        if (!npc->OnTalk.Fire(cl, true, npc->GetTalkedPlayers() + 1) || !OnCritterTalk.Fire(npc, cl, true, npc->GetTalkedPlayers() + 1)) {
            return;
        }

        dialog_pack = DlgMngr.GetDialog(selected_dlg_pack_id);
        dialogs = (dialog_pack != nullptr ? &dialog_pack->Dialogs : nullptr);
        if (dialogs == nullptr || dialogs->empty()) {
            return;
        }

        if (!ignore_distance) {
            const auto dir = Geometry.GetFarDir(cl->GetHexX(), cl->GetHexY(), npc->GetHexX(), npc->GetHexY());
            npc->ChangeDir(Geometry.ReverseDir(dir));
            npc->Broadcast_Dir();
            cl->ChangeDir(dir);
            cl->Broadcast_Dir();
            cl->Send_Dir(cl);
        }
    }
    // Talk with hex
    else {
        if (!ignore_distance && !Geometry.CheckDist(cl->GetHexX(), cl->GetHexY(), hx, hy, Settings.TalkDistance + cl->GetMultihex())) {
            cl->Send_Position(cl);
            cl->Send_TextMsg(cl, STR_DIALOG_DIST_TOO_LONG, SAY_NETMSG, TEXTMSG_GAME);
            WriteLog("Wrong distance to hexes, hx {}, hy {}, client '{}'", hx, hy, cl->GetName());
            return;
        }

        dialog_pack = DlgMngr.GetDialog(selected_dlg_pack_id);
        dialogs = (dialog_pack != nullptr ? &dialog_pack->Dialogs : nullptr);
        if (dialogs == nullptr || dialogs->empty()) {
            WriteLog("No dialogs, hx {}, hy {}, client '{}'", hx, hy, cl->GetName());
            return;
        }
    }

    // Predialogue installations
    auto it_d = dialogs->begin();
    auto go_dialog = static_cast<uint>(-1);
    auto it_a = (*it_d).Answers.begin();
    for (; it_a != (*it_d).Answers.end(); ++it_a) {
        if (DialogCheckDemand(npc, cl, *it_a, false)) {
            go_dialog = (*it_a).Link;
        }
        if (go_dialog != static_cast<uint>(-1)) {
            break;
        }
    }

    if (go_dialog == static_cast<uint>(-1)) {
        return;
    }

    // Use result
    const auto force_dialog = DialogUseResult(npc, cl, (*it_a));
    if (force_dialog != 0u) {
        if (force_dialog == static_cast<uint>(-1)) {
            return;
        }
        go_dialog = force_dialog;
    }

    // Find dialog
    it_d = std::find_if(dialogs->begin(), dialogs->end(), [go_dialog](const Dialog& dlg) { return dlg.Id == go_dialog; });
    if (it_d == dialogs->end()) {
        cl->Send_TextMsg(cl, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog from link {} not found, client '{}', dialog pack {}", go_dialog, cl->GetName(), dialog_pack->PackId);
        return;
    }

    // Compile
    if (!DialogCompile(npc, cl, *it_d, cl->Talk.CurDialog)) {
        cl->Send_TextMsg(cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog compile fail, client '{}', dialog pack {}", cl->GetName(), dialog_pack->PackId);
        return;
    }

    if (npc != nullptr) {
        cl->Talk.Type = TalkType::Critter;
        cl->Talk.CritterId = npc->GetId();
    }
    else {
        cl->Talk.Type = TalkType::Hex;
        cl->Talk.TalkHexMap = cl->GetMapId();
        cl->Talk.TalkHexX = hx;
        cl->Talk.TalkHexY = hy;
    }

    cl->Talk.DialogPackId = selected_dlg_pack_id;
    cl->Talk.LastDialogId = go_dialog;
    cl->Talk.StartTick = GameTime.GameTick();
    cl->Talk.TalkTime = Settings.DlgTalkMaxTime;
    cl->Talk.Barter = false;
    cl->Talk.IgnoreDistance = ignore_distance;

    // Get lexems
    cl->Talk.Lexems.clear();

    if (cl->Talk.CurDialog.DlgScriptFuncName) {
        auto failed = false;

        cl->Talk.Locked = true;
        if (auto func = ScriptSys->FindFunc<void, Critter*, Critter*, string*>(cl->Talk.CurDialog.DlgScriptFuncName); func && !func(cl, npc, &cl->Talk.Lexems)) {
            failed = true;
        }
        if (auto func = ScriptSys->FindFunc<uint, Critter*, Critter*, string*>(cl->Talk.CurDialog.DlgScriptFuncName); func && !func(cl, npc, &cl->Talk.Lexems)) {
            failed = true;
        }
        cl->Talk.Locked = false;

        if (failed) {
            CrMngr.CloseTalk(cl);
            cl->Send_TextMsg(cl, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
            WriteLog("Dialog generation failed, client '{}', dialog pack {}", cl->GetName(), dialog_pack->PackId);
            return;
        }
    }

    // On head text
    if (cl->Talk.CurDialog.Answers.empty()) {
        if (npc != nullptr) {
            npc->SendAndBroadcast_MsgLex(npc->VisCr, cl->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cl->Talk.Lexems);
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

void FOServer::Process_Dialog(Player* player)
{
    STACK_TRACE_ENTRY();

    Critter* cr = player->GetOwnedCritter();

    auto&& bin = player->Connection->InBuf;

    const auto cr_id = bin.Read<ident_t>();
    const auto dlg_pack_id = bin.Read<hstring>(*this);
    const auto num_answer = bin.Read<uint8>();

    if (cr->Talk.Type == TalkType::None) {
        BreakIntoDebugger();
        CrMngr.CloseTalk(cr);
        return;
    }

    if ((cr->Talk.Type == TalkType::Critter && cr->Talk.CritterId != cr_id) || (cr->Talk.Type == TalkType::Hex && cr->Talk.DialogPackId != dlg_pack_id)) {
        WriteLog("Invalid talk data '{}' {} {} {}", cr->GetName(), cr->Talk.Type, cr_id, dlg_pack_id);
        BreakIntoDebugger();
        CrMngr.CloseTalk(cr);
        return;
    }

    CrMngr.ProcessTalk(cr, true);

    if (cr->Talk.Type == TalkType::None) {
        BreakIntoDebugger();
        return;
    }

    Critter* talker = nullptr;

    if (cr->Talk.Type == TalkType::Critter) {
        talker = CrMngr.GetCritter(cr_id);

        if (talker == nullptr) {
            WriteLog("Critter with id {} not found, client '{}'", cr_id, cr->GetName());
            BreakIntoDebugger();
            cr->Send_TextMsg(cr, STR_DIALOG_NPC_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
            CrMngr.CloseTalk(cr);
            return;
        }
    }

    // Set dialogs
    auto* dialog_pack = DlgMngr.GetDialog(cr->Talk.DialogPackId);
    auto* dialogs = (dialog_pack != nullptr ? &dialog_pack->Dialogs : nullptr);
    if (dialogs == nullptr || dialogs->empty()) {
        WriteLog("No dialogs, npc '{}', client '{}'", talker->GetName(), cr->GetName());
        CrMngr.CloseTalk(cr);
        return;
    }

    // Continue dialog
    const auto* cur_dialog = &cr->Talk.CurDialog;
    const auto last_dialog = cur_dialog->Id;

    uint next_dlg_id;

    if (!cr->Talk.Barter) {
        // Barter
        const auto do_barter = [&] {
            if (cur_dialog->DlgScriptFuncName) {
                cr->Send_TextMsg(talker, STR_BARTER_NO_BARTER_NOW, SAY_DIALOG, TEXTMSG_GAME);
                return;
            }

            if (!talker->OnBarter.Fire(cr, true, talker->GetBarterPlayers() + 1) || !OnCritterBarter.Fire(talker, cr, true, talker->GetBarterPlayers() + 1)) {
                cr->Talk.Barter = true;
                cr->Talk.StartTick = GameTime.GameTick();
                cr->Talk.TalkTime = Settings.DlgBarterMaxTime;
            }
        };

        if (num_answer == ANSWER_BARTER) {
            do_barter();
            return;
        }

        // Refresh
        if (num_answer == ANSWER_BEGIN) {
            cr->Send_Talk();
            return;
        }

        // End
        if (num_answer == ANSWER_END) {
            CrMngr.CloseTalk(cr);
            return;
        }

        // Invalid answer
        if (num_answer >= cur_dialog->Answers.size()) {
            WriteLog("Wrong number of answer {}, client '{}'", num_answer, cr->GetName());
            cr->Send_Talk(); // Refresh
            return;
        }

        // Find answer
        const auto* answer = &cur_dialog->Answers[num_answer];

        // Check demand again
        if (!DialogCheckDemand(talker, cr, *answer, true)) {
            WriteLog("Secondary check of dialog demands fail, client '{}'", cr->GetName());
            CrMngr.CloseTalk(cr); // End
            return;
        }

        // Use result
        const uint force_dlg_id = DialogUseResult(talker, cr, *answer);
        if (force_dlg_id != 0) {
            next_dlg_id = force_dlg_id;
        }
        else {
            next_dlg_id = answer->Link;
        }

        // Special links
        switch (next_dlg_id) {
        case static_cast<uint>(-3):
        case DIALOG_BARTER:
            do_barter();
            return;
        case static_cast<uint>(-2):
        case DIALOG_BACK:
            if (cr->Talk.LastDialogId != 0u) {
                next_dlg_id = cr->Talk.LastDialogId;
                break;
            }
            [[fallthrough]];
        case static_cast<uint>(-1):
        case DIALOG_END:
            CrMngr.CloseTalk(cr);
            return;
        default:
            break;
        }
    }
    else {
        cr->Talk.Barter = false;
        next_dlg_id = cur_dialog->Id;
        if (talker != nullptr) {
            talker->OnBarter.Fire(cr, false, talker->GetBarterPlayers() + 1);
            OnCritterBarter.Fire(talker, cr, false, talker->GetBarterPlayers() + 1);
        }
    }

    // Find dialog
    const auto it_d = std::find_if(dialogs->begin(), dialogs->end(), [next_dlg_id](const Dialog& dlg) { return dlg.Id == next_dlg_id; });
    if (it_d == dialogs->end()) {
        CrMngr.CloseTalk(cr);
        cr->Send_TextMsg(cr, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog from link {} not found, client '{}', dialog pack {}", next_dlg_id, cr->GetName(), dialog_pack->PackId);
        return;
    }

    // Compile
    if (!DialogCompile(talker, cr, *it_d, cr->Talk.CurDialog)) {
        CrMngr.CloseTalk(cr);
        cr->Send_TextMsg(cr, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog compile fail, client '{}', dialog pack {}", cr->GetName(), dialog_pack->PackId);
        return;
    }

    if (next_dlg_id != last_dialog) {
        cr->Talk.LastDialogId = last_dialog;
    }

    // Get lexems
    cr->Talk.Lexems.clear();
    if (cr->Talk.CurDialog.DlgScriptFuncName) {
        auto failed = false;

        cr->Talk.Locked = true;
        if (auto func = ScriptSys->FindFunc<void, Critter*, Critter*, string*>(cr->Talk.CurDialog.DlgScriptFuncName); func && !func(cr, talker, &cr->Talk.Lexems)) {
            failed = true;
        }
        if (auto func = ScriptSys->FindFunc<uint, Critter*, Critter*, string*>(cr->Talk.CurDialog.DlgScriptFuncName); func && !func(cr, talker, &cr->Talk.Lexems)) {
            failed = true;
        }
        cr->Talk.Locked = false;

        if (failed) {
            CrMngr.CloseTalk(cr);
            cr->Send_TextMsg(cr, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
            WriteLog("Dialog generation failed, client '{}', dialog pack {}", cr->GetName(), dialog_pack->PackId);
            return;
        }
    }

    // On head text
    if (cr->Talk.CurDialog.Answers.empty()) {
        if (talker != nullptr) {
            talker->SendAndBroadcast_MsgLex(talker->VisCr, cr->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cr->Talk.Lexems);
        }
        else {
            auto* map = MapMngr.GetMap(cr->GetMapId());
            if (map != nullptr) {
                map->SetTextMsg(cr->Talk.TalkHexX, cr->Talk.TalkHexY, 0, TEXTMSG_DLG, cr->Talk.CurDialog.TextId);
            }
        }

        CrMngr.CloseTalk(cr);
        return;
    }

    cr->Talk.StartTick = GameTime.GameTick();
    cr->Talk.TalkTime = Settings.DlgTalkMaxTime;
    cr->Send_Talk();
}

void FOServer::Process_RemoteCall(Player* player)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    uint msg_len;
    player->Connection->InBuf >> msg_len;
    uint rpc_num;
    player->Connection->InBuf >> rpc_num;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    ScriptSys->HandleRemoteCall(rpc_num, player);
}

auto FOServer::CreateItemOnHex(Map* map, uint16 hx, uint16 hy, hstring pid, uint count, Properties* props, bool check_blocks) -> Item*
{
    STACK_TRACE_ENTRY();

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

auto FOServer::DialogScriptDemand(const DialogAnswerReq& demand, Critter* master, Critter* slave) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    bool result;

    switch (demand.ValuesCount) {
    case 0:
        return ScriptSys->CallFunc<bool, Critter*, Critter*>(demand.AnswerScriptFuncName, master, slave, result) && result;
    case 1:
        return ScriptSys->CallFunc<bool, Critter*, Critter*, int>(demand.AnswerScriptFuncName, master, slave, demand.ValueExt[0], result) && result;
    case 2:
        return ScriptSys->CallFunc<bool, Critter*, Critter*, int, int>(demand.AnswerScriptFuncName, master, slave, demand.ValueExt[0], demand.ValueExt[1], result) && result;
    case 3:
        return ScriptSys->CallFunc<bool, Critter*, Critter*, int, int, int>(demand.AnswerScriptFuncName, master, slave, demand.ValueExt[0], demand.ValueExt[1], demand.ValueExt[2], result) && result;
    case 4:
        return ScriptSys->CallFunc<bool, Critter*, Critter*, int, int, int, int>(demand.AnswerScriptFuncName, master, slave, demand.ValueExt[0], demand.ValueExt[1], demand.ValueExt[2], demand.ValueExt[3], result) && result;
    case 5:
        return ScriptSys->CallFunc<bool, Critter*, Critter*, int, int, int, int, int>(demand.AnswerScriptFuncName, master, slave, demand.ValueExt[0], demand.ValueExt[1], demand.ValueExt[2], demand.ValueExt[3], demand.ValueExt[4], result) && result;
    default:
        throw UnreachablePlaceException(LINE_STR);
    }
}

auto FOServer::DialogScriptResult(const DialogAnswerReq& result, Critter* master, Critter* slave) -> uint
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    switch (result.ValuesCount) {
    case 0:
        if (auto&& func = ScriptSys->FindFunc<uint, Critter*, Critter*>(result.AnswerScriptFuncName)) {
            return func(master, slave) ? func.GetResult() : 0u;
        }
        break;
    case 1:
        if (auto&& func = ScriptSys->FindFunc<uint, Critter*, Critter*, int>(result.AnswerScriptFuncName)) {
            return func(master, slave, result.ValueExt[0]) ? func.GetResult() : 0u;
        }
        break;
    case 2:
        if (auto&& func = ScriptSys->FindFunc<uint, Critter*, Critter*, int, int>(result.AnswerScriptFuncName)) {
            return func(master, slave, result.ValueExt[0], result.ValueExt[1]) ? func.GetResult() : 0u;
        }
        break;
    case 3:
        if (auto&& func = ScriptSys->FindFunc<uint, Critter*, Critter*, int, int, int>(result.AnswerScriptFuncName)) {
            return func(master, slave, result.ValueExt[0], result.ValueExt[1], result.ValueExt[2]) ? func.GetResult() : 0u;
        }
        break;
    case 4:
        if (auto&& func = ScriptSys->FindFunc<uint, Critter*, Critter*, int, int, int, int>(result.AnswerScriptFuncName)) {
            return func(master, slave, result.ValueExt[0], result.ValueExt[1], result.ValueExt[2], result.ValueExt[3]) ? func.GetResult() : 0u;
        }
        break;
    case 5:
        if (auto&& func = ScriptSys->FindFunc<uint, Critter*, Critter*, int, int, int, int, int>(result.AnswerScriptFuncName)) {
            return func(master, slave, result.ValueExt[0], result.ValueExt[1], result.ValueExt[2], result.ValueExt[3], result.ValueExt[4]) ? func.GetResult() : 0u;
        }
        break;
    default:
        throw UnreachablePlaceException(LINE_STR);
    }

    switch (result.ValuesCount) {
    case 0:
        if (!ScriptSys->CallFunc<void, Critter*, Critter*>(result.AnswerScriptFuncName, master, slave)) {
            return 0u;
        }
        break;
    case 1:
        if (!ScriptSys->CallFunc<void, Critter*, Critter*, int>(result.AnswerScriptFuncName, master, slave, result.ValueExt[0])) {
            return 0u;
        }
        break;
    case 2:
        if (!ScriptSys->CallFunc<void, Critter*, Critter*, int, int>(result.AnswerScriptFuncName, master, slave, result.ValueExt[0], result.ValueExt[1])) {
            return 0u;
        }
        break;
    case 3:
        if (!ScriptSys->CallFunc<void, Critter*, Critter*, int, int, int>(result.AnswerScriptFuncName, master, slave, result.ValueExt[0], result.ValueExt[1], result.ValueExt[2])) {
            return 0u;
        }
        break;
    case 4:
        if (!ScriptSys->CallFunc<void, Critter*, Critter*, int, int, int, int>(result.AnswerScriptFuncName, master, slave, result.ValueExt[0], result.ValueExt[1], result.ValueExt[2], result.ValueExt[3])) {
            return 0u;
        }
        break;
    case 5:
        if (!ScriptSys->CallFunc<void, Critter*, Critter*, int, int, int, int, int>(result.AnswerScriptFuncName, master, slave, result.ValueExt[0], result.ValueExt[1], result.ValueExt[2], result.ValueExt[3], result.ValueExt[4])) {
            return 0u;
        }
        break;
    default:
        throw UnreachablePlaceException(LINE_STR);
    }

    return 0u;
}

auto FOServer::MakePlayerId(string_view player_name) const -> ident_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!player_name.empty());
    const auto hash_value = Hashing::MurmurHash2(reinterpret_cast<const uint8*>(player_name.data()), static_cast<uint>(player_name.length()));
    RUNTIME_ASSERT(hash_value);
    return ident_t {(1u << 31u) | hash_value};
}
