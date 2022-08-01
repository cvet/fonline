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
    WriteLog("Start server");

    FileSys.AddDataSource(Settings.EmbeddedResources);
    FileSys.AddDataSource(Settings.ResourcesDir, DataSourceType::DirRoot);

    FileSys.AddDataSource(_str(Settings.ResourcesDir).combinePath("Maps"));
    FileSys.AddDataSource(_str(Settings.ResourcesDir).combinePath("Protos"));
    FileSys.AddDataSource(_str(Settings.ResourcesDir).combinePath("Dialogs"));
    FileSys.AddDataSource(_str(Settings.ResourcesDir).combinePath("AngelScript"));

    for (const auto& entry : Settings.ServerResourceEntries) {
        FileSys.AddDataSource(_str(Settings.ResourcesDir).combinePath(entry));
    }

#if !FO_SINGLEPLAYER
    extern auto Server_RegisterData(FOEngineBase*)->vector<uchar>;
    RestoreInfoBin = Server_RegisterData(this);

    ScriptSys = new ServerScriptSystem(this);
    ScriptSys->InitSubsystems();
#endif

    GameTime.FrameAdvance();

    // Network
    WriteLog("Starting server on ports {} and {}", Settings.ServerPort, Settings.ServerPort + 1);
    if ((_tcpServer = NetServerBase::StartTcpServer(Settings, [this](NetConnection* net_connection) { OnNewConnection(net_connection); })) == nullptr) {
        throw ServerInitException("Can't listen TCP server ports", Settings.ServerPort);
    }
    if ((_webSocketsServer = NetServerBase::StartWebSocketsServer(Settings, [this](NetConnection* net_connection) { OnNewConnection(net_connection); })) == nullptr) {
        throw ServerInitException("Can't listen TCP server ports", Settings.ServerPort + 1);
    }

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

    DbStorage.StartChanges();
    if (DbHistory) {
        DbHistory.StartChanges();
    }

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

        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::Count_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemCount(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsHidden_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemChangeView(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsAlwaysView_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemChangeView(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsTrap_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemChangeView(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::TrapValue_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemChangeView(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsNoBlock_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemRecacheHex(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsShootThru_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemRecacheHex(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsGag_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemRecacheHex(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsTrigger_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemRecacheHex(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::BlockLines_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemBlockLines(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsGeck_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemIsGeck(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::IsRadio_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemIsRadio(entity, prop, data.GetPtrAs<void>()); });
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), Item::Opened_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemOpened(entity, prop, data.GetPtrAs<void>()); });
    }

    DlgMngr.LoadFromResources();
    ProtoMngr.LoadFromResources();
    MapMngr.LoadFromResources();

    // Globals
    const auto globals_doc = DbStorage.Get("Game", 1);
    if (globals_doc.empty()) {
        DbStorage.Insert("Game", 1, {});
    }
    else {
        if (!PropertiesSerializator::LoadFromDocument(&GetPropertiesForEdit(), globals_doc, *this)) {
            throw ServerInitException("Failed to load globals document");
        }

        GameTime.Reset(GetYear(), GetMonth(), GetDay(), GetHour(), GetMinute(), GetSecond(), GetTimeMultiplier());
    }

    ServerDeferredCalls.LoadDeferredCalls();

    // Resource packs for client
    if (Settings.DataSynchronization) {
        auto writer = DataWriter(_updateFilesDesc);

        const auto add_sync_file = [&writer, this](string_view path) {
            const auto file = FileSys.ReadFile(path);
            if (!file) {
                throw ServerInitException("Resource pack for client not found", path);
            }

            const auto data = file.GetData();
            _updateFilesData.push_back(data);

            writer.Write<short>(static_cast<short>(file.GetPath().length()));
            writer.WritePtr(file.GetPath().data(), file.GetPath().length());
            writer.Write<uint>(static_cast<uint>(data.size()));
            writer.Write<uint>(Hashing::MurmurHash2(data.data(), data.size()));
        };

        add_sync_file("Core.zip");
        add_sync_file("Protos.zip");
        add_sync_file("Texts.zip");
        add_sync_file("AngelScript.zip");

        for (const auto& resource_entry : Settings.ClientResourceEntries) {
            add_sync_file(_str("{}.zip", resource_entry));
        }

        add_sync_file("Texts.zip");
        add_sync_file("Protos.zip");
        if constexpr (FO_ANGELSCRIPT_SCRIPTING) {
            add_sync_file("AngelScript.zip");
        }

        // Complete files list
        writer.Write<short>(-1);
    }

    // Scripting
    ScriptSys->InitModules();

    if (!OnInit.Fire()) {
        throw ServerInitException("Initialization script failed");
    }

    // Init world
    if (globals_doc.empty()) {
        if (!OnGenerateWorld.Fire()) {
            throw ServerInitException("Generate world script failed");
        }
    }
    else {
        const auto loc_fabric = [this](uint id, const ProtoLocation* proto) { return new Location(this, id, proto); };
        const auto map_fabric = [this](uint id, const ProtoMap* proto) {
            const auto* static_map = MapMngr.GetStaticMap(proto);
            return new Map(this, id, proto, nullptr, static_map);
        };
        const auto cr_fabric = [this](uint id, const ProtoCritter* proto) { return new Critter(this, id, nullptr, proto); };
        const auto item_fabric = [this](uint id, const ProtoItem* proto) { return new Item(this, id, proto); };
        EntityMngr.LoadEntities(loc_fabric, map_fabric, cr_fabric, item_fabric);

        MapMngr.LinkMaps();
        CrMngr.LinkCritters();
        ItemMngr.LinkItems();

        EntityMngr.InitAfterLoad();

        for (auto&& [id, item] : EntityMngr.GetItems()) {
            if (item->GetIsRadio()) {
                ItemMngr.RegisterRadio(item);
            }
        }

        WriteLog("Process critters visibility..");
        for (auto&& [id, cr] : copy(EntityMngr.GetCritters())) {
            if (!cr->IsDestroyed()) {
                MapMngr.ProcessVisibleCritters(cr);
                MapMngr.ProcessVisibleItems(cr);
            }
        }
        WriteLog("Process critters visibility complete");
    }

    // Start script
    if (!OnStart.Fire()) {
        throw ServerInitException("Start script failed");
    }

    // Admin manager
    if (Settings.AdminPanelPort != 0u) {
        InitAdminManager(this, static_cast<ushort>(Settings.AdminPanelPort));
    }

    // Commit initial data base changes
    DbStorage.CommitChanges();
    if (DbHistory) {
        DbHistory.CommitChanges();
    }

    GameTime.FrameAdvance();
    _stats.ServerStartTick = GameTime.FrameTick();
    _fpsTick = GameTime.FrameTick();
    _started = true;

    WriteLog("Start server complete!");
}

FOServer::~FOServer()
{
    delete _tcpServer;
    delete _webSocketsServer;
    delete ScriptSys;
}

void FOServer::Shutdown()
{
    _willFinishDispatcher();

    // Finish logic
    DbStorage.StartChanges();
    if (DbHistory) {
        DbHistory.StartChanges();
    }

    OnFinish.Fire();
    EntityMngr.FinalizeEntities();

    DbStorage.CommitChanges();
    if (DbHistory) {
        DbHistory.CommitChanges();
    }

    // Logging clients
    SetLogCallback("LogToClients", nullptr);
    for (const auto* cl : _logClients) {
        cl->Release();
    }
    _logClients.clear();

    // Connected players
    for (auto&& [id, player] : copy(EntityMngr.GetPlayers())) {
        player->Connection->HardDisconnect();
    }

    // Free connections
    {
        std::lock_guard locker(_freeConnectionsLocker);

        for (auto* free_connection : _freeConnections) {
            free_connection->HardDisconnect();
            delete free_connection;
        }

        _freeConnections.clear();
    }

    // Shutdown servers
    if (_tcpServer != nullptr) {
        _tcpServer->Shutdown();
        delete _tcpServer;
        _tcpServer = nullptr;
    }
    if (_webSocketsServer != nullptr) {
        _webSocketsServer->Shutdown();
        delete _webSocketsServer;
        _webSocketsServer = nullptr;
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
    // Todo: move server loop to async processing

    // Cycle time
    const auto frame_begin = Timer::RealtimeTick();

    // Begin data base changes
    DbStorage.StartChanges();
    if (DbHistory) {
        DbHistory.StartChanges();
    }

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

    // Process free connections
    {
        vector<ClientConnection*> free_connections;

        {
            std::lock_guard locker(_freeConnectionsLocker);
            free_connections = copy(_freeConnections);
        }

        for (auto* free_connection : free_connections) {
            ProcessConnection(free_connection);
            ProcessFreeConnection(free_connection);
        }
    }

    // Process players
    {
        const auto players = copy(EntityMngr.GetPlayers());

        for (auto&& [id, player] : players) {
            player->AddRef();
        }

        for (auto&& [id, player] : players) {
            try {
                RUNTIME_ASSERT(!player->IsDestroyed());

                ProcessConnection(player->Connection);
                ProcessPlayerConnection(player);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }

        for (auto&& [id, player] : players) {
            player->Release();
        }
    }

    // Process critters
    for (auto&& [id, cr] : copy(EntityMngr.GetCritters())) {
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

    // Process maps
    for (auto&& [id, map] : copy(EntityMngr.GetMaps())) {
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
    if (DbHistory) {
        DbHistory.CommitChanges();
    }

    // Clients log
    DispatchLogToClients();

    // Fill statistics
    const auto frame_time = Timer::RealtimeTick() - frame_begin;
    const auto loop_tick = static_cast<uint>(frame_time);
    _stats.LoopTime += loop_tick;
    _stats.LoopCycles++;
    if (loop_tick > _stats.LoopMax) {
        _stats.LoopMax = loop_tick;
    }
    if (loop_tick < _stats.LoopMin) {
        _stats.LoopMin = loop_tick;
    }
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
            buf += _str("NPC in game: {}\n", CrMngr.NpcInGame());
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
    uint conn_count;

    {
        std::lock_guard locker(_freeConnectionsLocker);
        conn_count = static_cast<uint>(_freeConnections.size());
    }

    const auto& players = EntityMngr.GetPlayers();

    string result = _str("Players in game: {}\nConnections: {}\n", players.size(), conn_count);
    result += "Name                 Id         Ip              X     Y     Location and map\n";
    for (auto&& [id, player] : players) {
        const auto* cr = player->GetOwnedCritter();
        const auto* map = MapMngr.GetMap(cr->GetMapId());
        const auto* loc = (map != nullptr ? map->GetLocation() : nullptr);

        const string str_loc = _str("{} ({}) {} ({})", map != nullptr ? loc->GetName() : "", map != nullptr ? loc->GetId() : 0, map != nullptr ? map->GetName() : "", map != nullptr ? map->GetId() : 0);
        result += _str("{:<20} {:<10} {:<15} {:<5} {:<5} {}\n", player->GetName(), player->GetId(), player->GetHost(), map != nullptr ? cr->GetHexX() : cr->GetWorldX(), map != nullptr ? cr->GetHexY() : cr->GetWorldY(), map != nullptr ? str_loc : "Global map");
    }
    return result;
}

void FOServer::OnNewConnection(NetConnection* net_connection)
{
    if (!_started) {
        return;
    }

    auto* connection = new ClientConnection(net_connection);

    // Add to free connections
    {
        std::lock_guard locker(_freeConnectionsLocker);
        _freeConnections.push_back(connection);
    }
}

void FOServer::ProcessFreeConnection(ClientConnection* connection)
{
    if (connection->IsHardDisconnected()) {
        std::lock_guard locker(_freeConnectionsLocker);
        const auto it = std::find(_freeConnections.begin(), _freeConnections.end(), connection);
        RUNTIME_ASSERT(it != _freeConnections.end());
        _freeConnections.erase(it);
        delete connection;
        return;
    }

    std::lock_guard locker(connection->BinLocker);

    if (connection->IsGracefulDisconnected()) {
        connection->Bin.ResetBuf();
        return;
    }

    connection->Bin.ShrinkReadBuf();

    if (connection->Bin.NeedProcess()) {
        uint msg = 0;
        connection->Bin >> msg;

        switch (msg) {
        case 0xFFFFFFFF: {
            // At least 16 bytes should be sent for backward compatibility,
            // even if answer data will change its meaning
            CONNECTION_OUTPUT_BEGIN(connection);
            connection->DisableCompression();
            connection->Bout << _stats.CurOnline;
            connection->Bout << _stats.Uptime;
            connection->Bout << static_cast<uint>(0);
            connection->Bout << static_cast<uchar>(0);
            connection->Bout << static_cast<uchar>(0xF0);
            connection->Bout << static_cast<ushort>(FO_COMPATIBILITY_VERSION);
            CONNECTION_OUTPUT_END(connection);

            connection->HardDisconnect();
            break;
        }
        case NETMSG_PING:
            Process_Ping(connection);
            break;
        case NETMSG_LOGIN:
            Process_LogIn(connection);
            break;
        case NETMSG_REGISTER:
            Process_Register(connection);
            break;
        case NETMSG_UPDATE:
            Process_Update(connection);
            break;
        case NETMSG_GET_UPDATE_FILE:
            Process_UpdateFile(connection);
            break;
        case NETMSG_GET_UPDATE_FILE_DATA:
            Process_UpdateFileData(connection);
            break;
        default:
            connection->Bin.SkipMsg(msg);
            break;
        }

        connection->LastActivityTime = GameTime.FrameTick();
    }
}

void FOServer::ProcessPlayerConnection(Player* player)
{
    if (player->Connection->IsHardDisconnected()) {
        // Manage saves, exit from game
        /*
         #if !FO_SINGLEPLAYER
        PlayerLogout.Fire(cr);
#endif
         * const auto id = cl->GetId();
        auto* cl_ = (id != 0u ? CrMngr.GetPlayer(id) : nullptr);
        if (cl_ != nullptr && cl_ == cl) {
            SetBit(cl->Flags, FCRIT_DISCONNECT);
            if (cl->GetMapId() != 0u) {
                cl->Broadcast_Action(ACTION_DISCONNECT, 0, nullptr);
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

        {
            std::lock_guard locker(ConnectedClientsLocker);

            auto it = std::find(ConnectedClients.begin(), ConnectedClients.end(), cl);
            RUNTIME_ASSERT(it != ConnectedClients.end());
            ConnectedClients.erase(it);
        }

        cl->Release();*/
        return;
    }

    if (player->Connection->IsGracefulDisconnected()) {
        std::lock_guard locker(player->Connection->BinLocker);
        player->Connection->Bin.ResetBuf();
        return;
    }

    if (player->IsTransferring) {
        while (!player->Connection->IsHardDisconnected() && !player->Connection->IsGracefulDisconnected()) {
            std::lock_guard locker(player->Connection->BinLocker);

            player->Connection->Bin.ShrinkReadBuf();

            if (!player->Connection->Bin.NeedProcess()) {
                CHECK_CLIENT_IN_BUF_ERROR(player->Connection);
                break;
            }

            uint msg = 0;
            player->Connection->Bin >> msg;

            switch (msg) {
            case NETMSG_PING:
                Process_Ping(player->Connection);
                break;
            case NETMSG_SEND_GIVE_MAP:
                Process_GiveMap(player);
                break;
            case NETMSG_SEND_LOAD_MAP_OK:
                Process_PlaceToGame(player);
                break;
            default:
                player->Connection->Bin.SkipMsg(msg);
                break;
            }

            player->Connection->LastActivityTime = GameTime.FrameTick();

            CHECK_CLIENT_IN_BUF_ERROR(player->Connection);
        }
    }
    else {
        while (!player->Connection->IsHardDisconnected() && !player->Connection->IsGracefulDisconnected()) {
            std::lock_guard locker(player->Connection->BinLocker);

            player->Connection->Bin.ShrinkReadBuf();

            if (!player->Connection->Bin.NeedProcess()) {
                CHECK_CLIENT_IN_BUF_ERROR(player->Connection);
                break;
            }

            uint msg = 0;
            player->Connection->Bin >> msg;

            switch (msg) {
            case NETMSG_PING:
                Process_Ping(player->Connection);
                break;
            case NETMSG_SEND_TEXT:
                Process_Text(player);
                break;
            case NETMSG_SEND_COMMAND:
                Process_Command(
                    player->Connection->Bin, [player](auto s) { player->Send_Text(nullptr, _str(s).trim(), SAY_NETMSG); }, player, "");
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
            case NETMSG_SEND_REFRESH_ME:
                player->Send_LoadMap(nullptr, MapMngr);
                break;
            case NETMSG_SEND_GET_INFO:
                player->Send_GameInfo(MapMngr.GetMap(player->GetOwnedCritter()->GetMapId()));
                break;
            case NETMSG_SEND_GIVE_MAP:
                Process_GiveMap(player);
                break;
            case NETMSG_RPC:
                // ScriptSys.HandleRpc(player);
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
                player->Connection->Bin.SkipMsg(msg);
                break;
            }

            player->Connection->LastActivityTime = GameTime.FrameTick();

            CHECK_CLIENT_IN_BUF_ERROR(player->Connection);
        }
    }
}

void FOServer::ProcessConnection(ClientConnection* connection)
{
    NON_CONST_METHOD_HINT();

    if (connection->IsHardDisconnected()) {
        return;
    }

    if (connection->Bin.IsError()) {
        WriteLog("Wrong network data from connection ip '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    if (connection->LastActivityTime == 0u) {
        connection->LastActivityTime = GameTime.FrameTick();
    }

    if (GameTime.FrameTick() - connection->LastActivityTime >= PING_CLIENT_LIFE_TIME) {
        WriteLog("Connection activity timeout from ip '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    if (connection->PingNextTick == 0u || GameTime.FrameTick() >= connection->PingNextTick) {
        if (!connection->PingOk) {
            connection->HardDisconnect();
            return;
        }

        CONNECTION_OUTPUT_BEGIN(connection);
        connection->Bout << NETMSG_PING;
        connection->Bout << PING_CLIENT;
        CONNECTION_OUTPUT_END(connection);

        connection->PingNextTick = GameTime.FrameTick() + PING_CLIENT_LIFE_TIME;
        connection->PingOk = false;
    }
}

void FOServer::Process_Text(Player* player)
{
    Critter* cr = player->GetOwnedCritter();

    uint msg_len = 0;
    uchar how_say = 0;
    string str;

    player->Connection->Bin >> msg_len;
    player->Connection->Bin >> how_say;
    player->Connection->Bin >> str;

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

    vector<ushort> channels;

    switch (how_say) {
    case SAY_NORM: {
        if (cr->GetMapId() != 0u) {
            cr->SendAndBroadcast_Text(cr->VisCr, str, SAY_NORM, true);
        }
        else {
            cr->SendAndBroadcast_Text(*cr->GlobalMapGroup, str, SAY_NORM, true);
        }
    } break;
    case SAY_SHOUT: {
        if (cr->GetMapId() != 0u) {
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
        if (cr->GetMapId() != 0u) {
            cr->SendAndBroadcast_Text(cr->VisCr, str, SAY_EMOTE, true);
        }
        else {
            cr->SendAndBroadcast_Text(*cr->GlobalMapGroup, str, SAY_EMOTE, true);
        }
    } break;
    case SAY_WHISP: {
        if (cr->GetMapId() != 0u) {
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
        if (cr->GetMapId() != 0u) {
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
    SetLogCallback("Process_Command", logcb);
    Process_CommandReal(buf, logcb, player, admin_panel);
    SetLogCallback("Process_Command", nullptr);
}

void FOServer::Process_CommandReal(NetInBuffer& buf, const LogFunc& logcb, Player* player, string_view admin_panel)
{
    auto* cl_ = player->GetOwnedCritter();

    uint msg_len = 0;
    uchar cmd = 0;

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

        string str;
        {
            std::lock_guard locker(_freeConnectionsLocker);
            str = _str("Free connections: {}, Players: {}, Npc: {}. FOServer machine uptime: {} min., FOServer uptime: {} min.", _freeConnections.size(), CrMngr.PlayersInGame(), CrMngr.NpcInGame(), GameTime.FrameTick() / 1000 / 60, (GameTime.FrameTick() - _stats.ServerStartTick) / 1000 / 60);
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

        const auto player_id = MakePlayerId(name);
        if (DbStorage.Valid("Players", player_id)) {
            logcb(_str("Player id is {}.", player_id));
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

        if (cl_ != nullptr && cl_->GetId() == crid) {
            logcb("To kick yourself type <~exit>");
            return;
        }

        auto* cr = CrMngr.GetCritter(crid);
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

        player->Access = static_cast<uchar>(wanted_access);
        logcb("Access changed.");
    } break;
    case CMD_ADDITEM: {
        ushort hex_x = 0;
        ushort hex_y = 0;
        hstring pid;
        uint count = 0;
        buf >> hex_x;
        buf >> hex_y;
        pid = buf.ReadHashedString(*this);
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
        pid = buf.ReadHashedString(*this);
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
        hstring pid;
        buf >> hex_x;
        buf >> hex_y;
        buf >> dir;
        pid = buf.ReadHashedString(*this);

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
        hstring pid;
        buf >> wx;
        buf >> wy;
        pid = buf.ReadHashedString(*this);

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

        if (ScriptSys->CallFunc<void, Critter*, int, int, int>(ToHashedString(func_name), static_cast<Critter*>(cl_), param0, param1, param2)) {
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
        player->Send_GameInfo(MapMngr.GetMap(player->GetOwnedCritter()->GetMapId()));
    }
}

void FOServer::LogToClients(string_view str)
{
    if (!str.empty() && str.back() == '\n') {
        _logLines.emplace_back(str, 0, str.length() - 1);
    }
    else {
        _logLines.emplace_back(str);
    }
}

void FOServer::DispatchLogToClients()
{
    if (_logLines.empty()) {
        return;
    }

    for (const auto& str : _logLines) {
        for (auto it = _logClients.begin(); it < _logClients.end();) {
            if (auto* player = *it; !player->IsDestroyed()) {
                player->Send_TextEx(0, str, SAY_NETMSG, false);
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
#if FO_SINGLEPLAYER
    if (GameTime.IsGamePaused()) {
        return;
    }
#endif

    // Moving
    ProcessMove(cr);

    // Idle functions
    OnCritterIdle.Fire(cr);
    if (cr->GetMapId() == 0u) {
        OnCritterGlobalMapIdle.Fire(cr);
    }

    CrMngr.ProcessTalk(cr, false);

    // Cache look distance
    // Todo: disable look distance caching
    if (GameTime.GameTick() >= cr->CacheValuesNextTick) {
        cr->LookCacheValue = cr->GetLookDistance();
        cr->CacheValuesNextTick = GameTime.GameTick() + 3000;
    }

    // Remove player critter from game
    if (cr->IsOwnedByPlayer() && cr->GetOwner() == nullptr && cr->IsAlive() && cr->GetTimeoutRemoveFromGame() == 0u && cr->GetOfflineTime() >= Settings.MinimumOfflineTime) {
        if (cr->GetClientToDelete()) {
            OnCritterFinish.Fire(cr);
        }

        auto* map = MapMngr.GetMap(cr->GetMapId());
        MapMngr.EraseCrFromMap(cr, map);

        // Destroy
        const auto full_delete = cr->GetClientToDelete();
        EntityMngr.UnregisterEntity(cr);
        cr->MarkAsDestroyed();

        // Erase radios from collection
        for (auto* item : cr->GetRawItems()) {
            if (item->GetIsRadio()) {
                ItemMngr.UnregisterRadio(item);
            }
        }

        // Full delete
        if (full_delete) {
            CrMngr.DeleteInventory(cr);
            DbStorage.Delete("Players", cr->GetId());
        }

        cr->Release();
    }
}

void FOServer::VerifyTrigger(Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir)
{
    if (map->IsHexStaticTrigger(from_hx, from_hy)) {
        for (auto* item : map->GetStaticItemTriggers(from_hx, from_hy)) {
            if (item->TriggerScriptFunc) {
                if (!item->TriggerScriptFunc(cr, item, false, dir)) {
                    // Nop
                }
            }

            OnStaticItemWalk.Fire(item, cr, false, dir);
        }
    }

    if (map->IsHexStaticTrigger(to_hx, to_hy)) {
        for (auto* item : map->GetStaticItemTriggers(to_hx, to_hy)) {
            if (item->TriggerScriptFunc) {
                if (!item->TriggerScriptFunc(cr, item, true, dir)) {
                    // Nop
                }
            }

            OnStaticItemWalk.Fire(item, cr, true, dir);
        }
    }

    if (map->IsHexTrigger(from_hx, from_hy)) {
        for (auto* item : map->GetItemsTrigger(from_hx, from_hy)) {
            OnItemWalk.Fire(item, cr, false, dir);
        }
    }

    if (map->IsHexTrigger(to_hx, to_hy)) {
        for (auto* item : map->GetItemsTrigger(to_hx, to_hy)) {
            OnItemWalk.Fire(item, cr, true, dir);
        }
    }
}

void FOServer::Process_Ping(ClientConnection* connection)
{
    NON_CONST_METHOD_HINT();

    uchar ping = 0;

    connection->Bin >> ping;

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    if (ping == PING_CLIENT) {
        connection->PingOk = true;
        connection->PingNextTick = GameTime.FrameTick() + PING_CLIENT_LIFE_TIME;
        return;
    }

    if (ping == PING_PING) {
        // Valid pings
    }
    else {
        WriteLog("Unknown ping {}, client host '{}'", ping, connection->GetHost());
        return;
    }

    CONNECTION_OUTPUT_BEGIN(connection);
    connection->Bout << NETMSG_PING;
    connection->Bout << ping;
    CONNECTION_OUTPUT_END(connection);
}

void FOServer::Process_Update(ClientConnection* connection)
{
    // Net protocol
    ushort proto_ver = 0;
    connection->Bin >> proto_ver;
    const auto outdated = (proto_ver != static_cast<ushort>(FO_COMPATIBILITY_VERSION));

    // Begin data encrypting
    uint encrypt_key = 0;
    connection->Bin >> encrypt_key;
    connection->Bin.SetEncryptKey(encrypt_key);
    connection->Bout.SetEncryptKey(encrypt_key);

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    // Send update files list
    uint msg_len = sizeof(NETMSG_UPDATE_FILES_LIST) + sizeof(msg_len) + sizeof(bool) + sizeof(uint) + static_cast<uint>(_updateFilesDesc.size());

    // With global properties
    vector<uchar*>* global_vars_data = nullptr;
    vector<uint>* global_vars_data_sizes = nullptr;
    if (!outdated) {
        msg_len += sizeof(ushort) + StoreData(false, &global_vars_data, &global_vars_data_sizes);
    }

    CONNECTION_OUTPUT_BEGIN(connection);
    connection->Bout << NETMSG_UPDATE_FILES_LIST;
    connection->Bout << msg_len;
    connection->Bout << outdated;
    connection->Bout << static_cast<uint>(_updateFilesDesc.size());
    connection->Bout.Push(_updateFilesDesc.data(), static_cast<uint>(_updateFilesDesc.size()));
    if (!outdated) {
        NET_WRITE_PROPERTIES(connection->Bout, global_vars_data, global_vars_data_sizes);
    }
    CONNECTION_OUTPUT_END(connection);
}

void FOServer::Process_UpdateFile(ClientConnection* connection)
{
    uint file_index = 0;
    connection->Bin >> file_index;

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
    if (connection->UpdateFileIndex == -1) {
        WriteLog("Wrong update call, client host '{}'", connection->GetHost());
        connection->GracefulDisconnect();
        return;
    }

    const auto& update_file_data = _updateFilesData[connection->UpdateFileIndex];
    const auto offset = connection->UpdateFilePortion * FILE_UPDATE_PORTION;

    if (offset + FILE_UPDATE_PORTION < update_file_data.size()) {
        connection->UpdateFilePortion++;
    }
    else {
        connection->UpdateFileIndex = -1;
    }

    uchar data[FILE_UPDATE_PORTION];
    const auto remaining_size = update_file_data.size() - offset;
    if (remaining_size >= sizeof(data)) {
        std::memcpy(data, &update_file_data[offset], sizeof(data));
    }
    else {
        std::memcpy(data, &update_file_data[offset], remaining_size);
        std::memset(&data[remaining_size], 0, sizeof(data) - remaining_size);
    }

    CONNECTION_OUTPUT_BEGIN(connection);
    connection->Bout << NETMSG_UPDATE_FILE_DATA;
    connection->Bout.Push(data, sizeof(data));
    CONNECTION_OUTPUT_END(connection);
}

void FOServer::Process_Register(ClientConnection* connection)
{
    // Read message
    uint msg_len = 0;
    connection->Bin >> msg_len;

    // Protocol version
    ushort proto_ver = 0;
    connection->Bin >> proto_ver;

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    // Begin data encrypting
    connection->Bin.SetEncryptKey(1234567890);
    connection->Bout.SetEncryptKey(1234567890);

    // Check protocol
    if (proto_ver != static_cast<ushort>(FO_COMPATIBILITY_VERSION)) {
        connection->Send_CustomMessage(NETMSG_WRONG_NET_PROTO);
        connection->GracefulDisconnect();
        return;
    }

    // Name
    string name;
    connection->Bin >> name;

    // Password
    string password;
    connection->Bin >> password;

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    // Check data
    if (!_str(name).isValidUtf8() || name.find('*') != string::npos) {
        connection->Send_TextMsg(STR_NET_LOGINPASS_WRONG);
        connection->GracefulDisconnect();
        return;
    }

    // Check name length
    const auto name_len_utf8 = _str(name).lengthUtf8();
    if (name_len_utf8 < Settings.MinNameLength || name_len_utf8 > Settings.MaxNameLength) {
        connection->Send_TextMsg(STR_NET_LOGINPASS_WRONG);
        connection->GracefulDisconnect();
        return;
    }

    // Check for exist
    const auto player_id = MakePlayerId(name);
    if (DbStorage.Valid("Players", player_id)) {
        connection->Send_TextMsg(STR_NET_PLAYER_ALREADY);
        connection->GracefulDisconnect();
        return;
    }

    // Check brute force registration
    if (Settings.RegistrationTimeout != 0u) {
        auto ip = connection->GetIp();
        const auto reg_tick = Settings.RegistrationTimeout * 1000;
        if (auto it = _regIp.find(ip); it != _regIp.end()) {
            auto& last_reg = it->second;
            const auto tick = GameTime.FrameTick();
            if (tick - last_reg < reg_tick) {
                connection->Send_TextMsg(STR_NET_REGISTRATION_IP_WAIT);
                connection->Send_TextMsgLex(STR_NET_TIME_LEFT, _str("$time{}", (reg_tick - (tick - last_reg)) / 60000 + 1));
                connection->GracefulDisconnect();
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
    const auto allow = OnPlayerRegistration.Fire(connection->GetIp(), name, disallow_msg_num, disallow_str_num, lexems);
    if (!allow) {
        if (disallow_msg_num < TEXTMSG_COUNT && (disallow_str_num != 0u)) {
            connection->Send_TextMsgLex(disallow_str_num, lexems);
        }
        else {
            connection->Send_TextMsg(STR_NET_LOGIN_SCRIPT_FAIL);
        }
        connection->GracefulDisconnect();
        return;
    }
#endif

    // Register
    auto reg_ip = AnyData::Array();
    reg_ip.emplace_back(static_cast<int>(connection->GetIp()));
    auto reg_port = AnyData::Array();
    reg_port.emplace_back(static_cast<int>(connection->GetPort()));

    DbStorage.Insert("Players", player_id, {{"Name", name}, {"Password", password}, {"ConnectionIp", reg_ip}, {"ConnectionPort", reg_port}});

    // Notify
    connection->Send_TextMsg(STR_NET_REG_SUCCESS);

    CONNECTION_OUTPUT_BEGIN(connection);
    connection->Bout << NETMSG_REGISTER_SUCCESS;
    CONNECTION_OUTPUT_END(connection);

    connection->GracefulDisconnect();
}

void FOServer::Process_LogIn(ClientConnection* connection)
{
    // Net protocol
    ushort proto_ver = 0;
    connection->Bin >> proto_ver;

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    // Begin data encrypting
    connection->Bin.SetEncryptKey(12345);
    connection->Bout.SetEncryptKey(12345);

    // Check protocol
    if (proto_ver != static_cast<ushort>(FO_COMPATIBILITY_VERSION)) {
        connection->Send_CustomMessage(NETMSG_WRONG_NET_PROTO);
        connection->GracefulDisconnect();
        return;
    }

    // Login, password
    string name;
    string password;
    connection->Bin >> name;
    connection->Bin >> password;

    CHECK_CLIENT_IN_BUF_ERROR(connection);

    // If only cache checking than disconnect
    if (name.empty()) {
        connection->GracefulDisconnect();
        return;
    }

    // Check for null in login/password
    if (name.find('\0') != string::npos || password.find('\0') != string::npos) {
        connection->Send_TextMsg(STR_NET_WRONG_LOGIN);
        connection->GracefulDisconnect();
        return;
    }

    // Check valid symbols in name
    if (!_str(name).isValidUtf8() || name.find('*') != string::npos) {
        connection->Send_TextMsg(STR_NET_WRONG_DATA);
        connection->GracefulDisconnect();
        return;
    }

    // Check for name length
    const auto name_len_utf8 = _str(name).lengthUtf8();
    if (name_len_utf8 < Settings.MinNameLength || name_len_utf8 > Settings.MaxNameLength) {
        connection->Send_TextMsg(STR_NET_WRONG_LOGIN);
        connection->GracefulDisconnect();
        return;
    }

    // Check password
    const auto player_id = MakePlayerId(name);
    auto doc = DbStorage.Get("Players", player_id);
    if (doc.count("Password") == 0u || doc["Password"].index() != AnyData::STRING_VALUE || std::get<string>(doc["Password"]).length() != password.length() || std::get<string>(doc["Password"]) != password) {
        connection->Send_TextMsg(STR_NET_LOGINPASS_WRONG);
        connection->GracefulDisconnect();
        return;
    }

#if !FO_SINGLEPLAYER
    // Request script
    {
        uint disallow_msg_num = 0;
        uint disallow_str_num = 0;
        string lexems;
        if (const auto allow = !OnPlayerLogin.Fire(connection->GetIp(), name, player_id, disallow_msg_num, disallow_str_num, lexems); !allow) {
            if (disallow_msg_num < TEXTMSG_COUNT && (disallow_str_num != 0u)) {
                connection->Send_TextMsgLex(disallow_str_num, lexems);
            }
            else {
                connection->Send_TextMsg(STR_NET_LOGIN_SCRIPT_FAIL);
            }
            connection->GracefulDisconnect();
            return;
        }
    }
#endif

    // Move from free connections to player
    {
        std::lock_guard locker(_freeConnectionsLocker);

        const auto it = std::find(_freeConnections.begin(), _freeConnections.end(), connection);
        RUNTIME_ASSERT(it != _freeConnections.end());
        _freeConnections.erase(it);
    }

    // Kick previous
    if (const auto* player = EntityMngr.GetPlayer(player_id); player != nullptr) {
        connection->HardDisconnect();
    }

    // Create new
    auto* player = new Player(this, player_id, name, connection);

    if (!PropertiesSerializator::LoadFromDocument(&player->GetPropertiesForEdit(), doc, *this)) {
        player->Release();
        connection->Send_TextMsg(STR_NET_WRONG_DATA);
        connection->GracefulDisconnect();
        return;
    }

    // Attach critter
    // Todo: attach critter to player
    throw NotImplementedException(LINE_STR);
    /*EntityMngr.RegisterEntity(cl);
    const auto can = MapMngr.CanAddCrToMap(cl, nullptr, 0, 0, 0);
    RUNTIME_ASSERT(can);
    MapMngr.AddCrToMap(cl, nullptr, 0, 0, 0, 0);

    if (CritterInit.Fire(cl, true)) {
    }*/

    // Connection info
    {
        const auto ip = connection->GetIp();
        const auto port = connection->GetPort();

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
    uint msg_len = sizeof(uint) + sizeof(msg_len) + sizeof(uint) * 2;
    const uint bin_seed = GenericUtils::Random(100000, 2000000000);
    const uint bout_seed = GenericUtils::Random(100000, 2000000000);

    vector<uchar*>* global_vars_data = nullptr;
    vector<uint>* global_vars_data_sizes = nullptr;
    const auto whole_global_vars_data_size = StoreData(false, &global_vars_data, &global_vars_data_sizes);
    msg_len += sizeof(ushort) + whole_global_vars_data_size;

    vector<uchar*>* player_data = nullptr;
    vector<uint>* player_data_sizes = nullptr;
    const auto whole_player_data_size = StoreData(false, &player_data, &player_data_sizes);
    msg_len += sizeof(ushort) + whole_player_data_size;

    CONNECTION_OUTPUT_BEGIN(connection);
    connection->Bout << NETMSG_LOGIN_SUCCESS;
    connection->Bout << msg_len;
    connection->Bout << bin_seed;
    connection->Bout << bout_seed;
    NET_WRITE_PROPERTIES(connection->Bout, global_vars_data, global_vars_data_sizes);
    NET_WRITE_PROPERTIES(connection->Bout, player_data, player_data_sizes);
    CONNECTION_OUTPUT_END(connection);

    connection->Bin.SetEncryptKey(bin_seed);
    connection->Bout.SetEncryptKey(bout_seed);

    player->Send_LoadMap(nullptr, MapMngr);
}

void FOServer::Process_PlaceToGame(Player* player)
{
    Critter* cr = player->GetOwnedCritter();

    player->IsTransferring = false;

    if (cr->ViewMapId != 0u) {
        auto* map = MapMngr.GetMap(cr->ViewMapId);
        cr->ViewMapId = 0;
        if (map != nullptr) {
            MapMngr.ViewMap(cr, map, cr->ViewMapLook, cr->ViewMapHx, cr->ViewMapHy, cr->ViewMapDir);
            cr->Send_ViewMap();
            return;
        }
    }

    // Parse
    auto* map = MapMngr.GetMap(cr->GetMapId());
    cr->Send_GameInfo(map);

    // Parse to global
    if (cr->GetMapId() == 0u) {
        RUNTIME_ASSERT(cr->GlobalMapGroup);

        cr->Send_GlobalInfo(GM_INFO_ALL, MapMngr);
        cr->Send_AllAutomapsInfo(MapMngr);

        if (cr->Talk.Type != TalkType::None) {
            CrMngr.ProcessTalk(cr, true);
            cr->Send_Talk();
        }
    }
    else {
        if (map == nullptr) {
            WriteLog("Map not found, client '{}'", cr->GetName());
            player->Connection->HardDisconnect();
            return;
        }

        // Parse to local
        cr->Send_AddCritter(cr);

        // Send all data
        cr->Send_AddAllItems();
        cr->Send_AllAutomapsInfo(MapMngr);

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

        // Check active talk
        if (cr->Talk.Type != TalkType::None) {
            CrMngr.ProcessTalk(cr, true);
            cr->Send_Talk();
        }
    }

    // Notify about end of parsing
    cr->Send_CustomMessage(NETMSG_END_PARSE_TO_GAME);
}

void FOServer::Process_GiveMap(Player* player)
{
    Critter* cr = player->GetOwnedCritter();

    bool automap = 0;
    uint loc_id = 0;
    uint hash_tiles = 0;
    uint hash_scen = 0;

    player->Connection->Bin >> automap;
    const auto map_pid = player->Connection->Bin.ReadHashedString(*this);
    player->Connection->Bin >> loc_id;
    player->Connection->Bin >> hash_tiles;
    player->Connection->Bin >> hash_scen;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    const auto* proto_map = ProtoMngr.GetProtoMap(map_pid);
    if (proto_map == nullptr) {
        WriteLog("Map prototype not found, client '{}'", cr->GetName());
        player->Connection->HardDisconnect();
        return;
    }

    if (automap) {
        if (!MapMngr.CheckKnownLoc(cr, loc_id)) {
            WriteLog("Request for loading unknown automap, client '{}'", cr->GetName());
            return;
        }

        const auto* loc = MapMngr.GetLocation(loc_id);
        if (loc == nullptr) {
            WriteLog("Request for loading incorrect automap, client '{}'", cr->GetName());
            return;
        }

        auto found = false;
        const auto automaps = (loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : vector<hstring>());
        if (!automaps.empty()) {
            for (size_t i = 0; i < automaps.size() && !found; i++) {
                if (automaps[i] == map_pid) {
                    found = true;
                }
            }
        }
        if (!found) {
            WriteLog("Request for loading incorrect automap, client '{}'", cr->GetName());
            return;
        }
    }
    else {
        const auto* map = MapMngr.GetMap(cr->GetMapId());
        if ((map == nullptr || map_pid != map->GetProtoId()) && map_pid != cr->ViewMapPid) {
            WriteLog("Request for loading incorrect map, client '{}'", cr->GetName());
            return;
        }
    }

    {
        const auto* static_map = MapMngr.GetStaticMap(proto_map);
        const auto send_tiles = static_map->HashTiles != hash_tiles;
        const auto send_scenery = static_map->HashScen != hash_scen;

        constexpr auto msg = NETMSG_MAP;
        const auto maxhx = proto_map->GetWidth();
        const auto maxhy = proto_map->GetHeight();
        uint msg_len = sizeof(msg) + sizeof(msg_len) + sizeof(hstring::hash_t) + sizeof(maxhx) + sizeof(maxhy) + sizeof(bool) * 2;

        if (send_tiles) {
            msg_len += sizeof(uint) + static_cast<uint>(static_map->Tiles.size()) * sizeof(MapTile);
        }
        if (send_scenery) {
            msg_len += sizeof(uint) + static_cast<uint>(static_map->SceneryData.size());
        }

        CONNECTION_OUTPUT_BEGIN(player->Connection);
        player->Connection->Bout << msg;
        player->Connection->Bout << msg_len;
        player->Connection->Bout << map_pid;
        player->Connection->Bout << maxhx;
        player->Connection->Bout << maxhy;
        player->Connection->Bout << send_tiles;
        player->Connection->Bout << send_scenery;
        if (send_tiles) {
            player->Connection->Bout << static_cast<uint>(static_map->Tiles.size() * sizeof(MapTile));
            if (!static_map->Tiles.empty()) {
                player->Connection->Bout.Push(&static_map->Tiles[0], static_cast<uint>(static_map->Tiles.size()) * sizeof(MapTile));
            }
        }
        if (send_scenery) {
            player->Connection->Bout << static_cast<uint>(static_map->SceneryData.size());
            if (!static_map->SceneryData.empty()) {
                player->Connection->Bout.Push(&static_map->SceneryData[0], static_cast<uint>(static_map->SceneryData.size()));
            }
        }
        CONNECTION_OUTPUT_END(player->Connection);
    }

    if (!automap) {
        Map* map = nullptr;
        if (cr->ViewMapId != 0u) {
            map = MapMngr.GetMap(cr->ViewMapId);
        }
        cr->Send_LoadMap(map, MapMngr);
    }
}

void FOServer::Process_Move(Player* player)
{
}

void FOServer::Process_StopMove(Player* player)
{
}

void FOServer::Process_Dir(Player* player)
{
    NON_CONST_METHOD_HINT();

    Critter* cr = player->GetOwnedCritter();

    uchar dir = 0;
    player->Connection->Bin >> dir;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    if (cr->GetMapId() == 0u || dir >= Settings.MapDirCount || cr->GetDir() == dir || cr->IsTalking()) {
        if (cr->GetDir() != dir) {
            cr->Send_Dir(cr);
        }
        return;
    }

    cr->ChangeDir(dir);
    cr->Broadcast_Dir();
}

void FOServer::Process_Property(Player* player, uint data_size)
{
    Critter* cr = player->GetOwnedCritter();

    uint msg_len;
    uint cr_id;
    uint item_id;
    ushort property_index;

    if (data_size == 0) {
        player->Connection->Bin >> msg_len;
    }

    char type_ = 0;
    player->Connection->Bin >> type_;
    const auto type = static_cast<NetProperty>(type_);

    uint additional_args = 0;
    switch (type) {
    case NetProperty::CritterItem:
        additional_args = 2;
        player->Connection->Bin >> cr_id;
        player->Connection->Bin >> item_id;
        break;
    case NetProperty::Critter:
        additional_args = 1;
        player->Connection->Bin >> cr_id;
        break;
    case NetProperty::MapItem:
        additional_args = 1;
        player->Connection->Bin >> item_id;
        break;
    case NetProperty::ChosenItem:
        additional_args = 1;
        player->Connection->Bin >> item_id;
        break;
    default:
        break;
    }

    player->Connection->Bin >> property_index;

    CHECK_CLIENT_IN_BUF_ERROR(player->Connection);

    vector<uchar> data;
    if (data_size != 0) {
        data.resize(data_size);
        player->Connection->Bin.Pop(&data[0], data_size);
    }
    else {
        const uint len = msg_len - sizeof(uint) - sizeof(msg_len) - sizeof(char) - additional_args * sizeof(uint) - sizeof(ushort);
        // Todo: control max size explicitly, add option to property registration
        if (len > 0xFFFF) {
            // For now 64Kb for all
            return;
        }
        data.resize(len);
        player->Connection->Bin.Pop(&data[0], len);
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

    const auto access = prop->GetAccess();
    if (is_public && !IsEnumSet(access, Property::AccessType::PublicMask)) {
        return;
    }
    if (!is_public && (!IsEnumSet(access, Property::AccessType::ProtectedMask) || !IsEnumSet(access, Property::AccessType::PublicMask))) {
        return;
    }
    if (!IsEnumSet(access, Property::AccessType::ModifiableMask)) {
        return;
    }
    if (is_public && access != Property::AccessType::PublicFullModifiable) {
        return;
    }
    if (prop->IsPlainData() && data_size != prop->GetBaseSize()) {
        return;
    }
    if (!prop->IsPlainData() && data_size != 0) {
        return;
    }

    {
        player->SendIgnoreEntity = entity;
        player->SendIgnoreProperty = prop;

        auto revert_send_ignore = ScopeCallback([player]() noexcept {
            player->SendIgnoreEntity = nullptr;
            player->SendIgnoreProperty = nullptr;
        });

        entity->SetValueFromData(prop, data);
    }
}

void FOServer::OnSaveEntityValue(Entity* entity, const Property* prop)
{
    NON_CONST_METHOD_HINT();

    const auto value = PropertiesSerializator::SavePropertyToValue(&entity->GetProperties(), prop, *this);

    uint entry_id;

    if (const auto* server_entity = dynamic_cast<ServerEntity*>(entity); server_entity != nullptr) {
        if (server_entity->GetId() == 0u) {
            return;
        }

        entry_id = server_entity->GetId();

        DbStorage.Update(_str("{}s", entity->GetClassName()), entry_id, prop->GetName(), value);
    }
    else {
        entry_id = 1u;

        DbStorage.Update(entity->GetClassName(), entry_id, prop->GetName(), value);
    }

    if (DbHistory && prop->IsHistorical()) {
        const auto history_id = GetHistoryRecordsId();
        SetHistoryRecordsId(history_id + 1u);

        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

        AnyData::Document doc;
        doc["Time"] = static_cast<int64>(time.count());
        doc["EntityId"] = static_cast<int>(entry_id);
        doc["Property"] = prop->GetName();
        doc["Value"] = value;

        DbHistory.Insert(_str("{}sHistory", entity->GetClassName()), history_id, doc);
    }
}

void FOServer::OnSendGlobalValue(Entity* entity, const Property* prop)
{
    if (IsEnumSet(prop->GetAccess(), Property::AccessType::PublicMask)) {
        for (auto&& [id, player] : EntityMngr.GetPlayers()) {
            player->Send_Property(NetProperty::Game, prop, this);
        }
    }
}

void FOServer::OnSendPlayerValue(Entity* entity, const Property* prop)
{
    auto* player = dynamic_cast<Player*>(entity);

    player->Send_Property(NetProperty::Player, prop, player);
}

void FOServer::OnSendCritterValue(Entity* entity, const Property* prop)
{
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
    if (auto* item = dynamic_cast<Item*>(entity); item != nullptr && item->GetId() != 0u) {
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
    if (IsEnumSet(prop->GetAccess(), Property::AccessType::PublicMask)) {
        auto* map = dynamic_cast<Map*>(entity);
        map->SendProperty(NetProperty::Map, prop, map);
    }
}

void FOServer::OnSendLocationValue(Entity* entity, const Property* prop)
{
    if (IsEnumSet(prop->GetAccess(), Property::AccessType::PublicMask)) {
        auto* loc = dynamic_cast<Location*>(entity);
        for (auto* map : loc->GetMaps()) {
            map->SendProperty(NetProperty::Location, prop, loc);
        }
    }
}

void FOServer::OnSetItemCount(Entity* entity, const Property* prop, const void* new_value)
{
    NON_CONST_METHOD_HINT();
    UNUSED_VARIABLE(prop);

    auto* item = dynamic_cast<Item*>(entity);
    const auto new_count = *static_cast<const uint*>(new_value);
    const auto old_count = entity->GetProperties().GetValue<uint>(prop);
    if (static_cast<int>(new_count) > 0 && (item->GetStackable() || new_count == 1)) {
        const auto diff = static_cast<int>(item->GetCount()) - static_cast<int>(old_count);
        ItemMngr.ChangeItemStatistics(item->GetProtoId(), diff);
    }
    else {
        item->SetCount(old_count);
        if (!item->GetStackable()) {
            throw GenericException("Trying to change count of not stackable item");
        }

        throw GenericException("Item count can't be zero or negative", new_count);
    }
}

void FOServer::OnSetItemChangeView(Entity* entity, const Property* prop, const void* new_value)
{
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
            const auto value = *static_cast<const bool*>(new_value);
            if (value) {
                cr->Send_EraseItem(item);
            }
            else {
                cr->Send_AddItem(item);
            }
            cr->SendAndBroadcast_MoveItem(item, ACTION_REFRESH, 0);
        }
    }
}

void FOServer::OnSetItemRecacheHex(Entity* entity, const Property* prop, const void* new_value)
{
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(new_value);

    // IsNoBlock, IsShootThru, IsGag, IsTrigger
    const auto* item = dynamic_cast<Item*>(entity);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            map->RecacheHexFlags(item->GetHexX(), item->GetHexY());
        }
    }
}

void FOServer::OnSetItemBlockLines(Entity* entity, const Property* prop, const void* new_value)
{
    UNUSED_VARIABLE(new_value);

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

void FOServer::OnSetItemIsGeck(Entity* entity, const Property* prop, const void* new_value)
{
    auto* item = dynamic_cast<Item*>(entity);
    const auto value = *static_cast<const bool*>(new_value);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = MapMngr.GetMap(item->GetMapId());
        if (map != nullptr) {
            map->GetLocation()->GeckCount += (value ? 1 : -1);
        }
    }
}

void FOServer::OnSetItemIsRadio(Entity* entity, const Property* prop, const void* new_value)
{
    auto* item = dynamic_cast<Item*>(entity);
    const auto value = *static_cast<const bool*>(new_value);

    if (value) {
        ItemMngr.RegisterRadio(item);
    }
    else {
        ItemMngr.UnregisterRadio(item);
    }
}

void FOServer::OnSetItemOpened(Entity* entity, const Property* prop, const void* new_value)
{
    auto* item = dynamic_cast<Item*>(entity);
    const auto new_opened = *static_cast<const bool*>(new_value);

    if (item->GetIsCanOpen()) {
        if (new_opened) {
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
    // Moving
    if (cr->IsMoving()) {
        if (cr->Moving.IsRunning && cr->GetIsHide()) {
            cr->SetIsHide(false);
        }

        // bool is_run = cr->IsRunning;
        // if (is_run && cr->GetIsNoRun())
        //    return false;
        // if (!is_run && cr->GetIsNoWalk())
        //    return false;

        auto* map = MapMngr.GetMap(cr->GetMapId());
        if (map != nullptr && cr->IsAlive()) {
            ProcessMoveBySteps(cr, map);
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

    if (cr->IsMoving() && cr->TargetMoving.TargId != 0u && (cr->TargetMoving.HexX != 0u || cr->TargetMoving.HexY != 0u)) {
        const auto* targ = cr->GetCrSelf(cr->TargetMoving.TargId);
        if (targ != nullptr && !Geometry.CheckDist(targ->GetHexX(), targ->GetHexY(), cr->TargetMoving.HexX, cr->TargetMoving.HexY, 0)) {
            recalculate = true;
        }
    }

    if (cr->TargetMoving.State == MovingState::InProgress || recalculate) {
        // Find path if not exist
        if (!cr->IsMoving() || recalculate) {
            ushort hx;
            ushort hy;
            uint cut;
            uint trace_dist;
            Critter* trace_cr;

            if (cr->TargetMoving.TargId != 0u) {
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
            find_input.IsRun = cr->TargetMoving.IsRun;
            find_input.Multihex = cr->GetMultihex();
            find_input.Cut = cut;
            find_input.TraceDist = trace_dist;
            find_input.TraceCr = trace_cr;
            find_input.CheckCritter = true;
            find_input.CheckGagItems = true;

            if (find_input.IsRun && cr->GetIsNoRun()) {
                find_input.IsRun = false;
            }

            if (cr->GetIsNoMove()) {
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
            MoveCritter(cr, !cr->GetIsNoRun(), cr->GetHexX(), cr->GetHexY(), find_path.Steps, find_path.ControlSteps, 0, 0, true);
        }
    }
}

void FOServer::ProcessMoveBySteps(Critter* cr, Map* map)
{
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
            if (normalized_time == 1.0f)
                normalized_dist = 1.0f;

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
                const uchar dir = Geometry.GetFarDir(old_hx, old_hy, hx2, hy2);
                const uint multihex = cr->GetMultihex();

                if (map->IsMovePassed(cr, hx2, hy2, multihex)) {
                    const auto is_dead = cr->IsDead();
                    map->UnsetFlagCritter(old_hx, old_hy, multihex, is_dead);
                    cr->SetHexX(hx2);
                    cr->SetHexY(hy2);
                    map->SetFlagCritter(hx2, hy2, multihex, is_dead);

                    MapMngr.ProcessVisibleCritters(cr);
                    if (cr->Moving.Uid != move_uid) {
                        return;
                    }

                    MapMngr.ProcessVisibleItems(cr);
                    if (cr->Moving.Uid != move_uid) {
                        return;
                    }

                    VerifyTrigger(map, cr, old_hx, old_hy, hx2, hy2, dir);
                    if (cr->Moving.Uid != move_uid) {
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

            const auto mxi = static_cast<short>(std::round(mx));
            const auto myi = static_cast<short>(std::round(my));
            if (moved || cr->GetHexOffsX() != mxi || cr->GetHexOffsY() != myi) {
                cr->SetHexOffsX(mxi);
                cr->SetHexOffsY(myi);
            }

            // Evaluate dir angle
            const auto dir_angle = Geometry.GetLineDirAngle(0, 0, ox, oy);
            cr->SetDirAngle(static_cast<short>(dir_angle));

            goto label_Done;
        }

        RUNTIME_ASSERT(i < cr->Moving.ControlSteps.size() - 1);

        control_step_begin = cr->Moving.ControlSteps[i];
        start_hx = hx;
        start_hy = hy;
        cur_dist += dist;
    }

label_Done:
    if (normalized_time == 1.0f) {
        if (cr->GetHexX() != cr->Moving.EndHexX || cr->GetHexY() != cr->Moving.EndHexY) {
            cr->Send_Position(cr);
            cr->Broadcast_Position();
        }

        cr->ClearMove();
    }
}

void FOServer::MoveCritter(Critter* cr, bool is_run, ushort start_hx, ushort start_hy, const vector<uchar>& steps, const vector<ushort>& control_steps, short end_hex_ox, short end_hex_oy, bool send_self)
{
    cr->ClearMove();

    const auto* map = MapMngr.GetMap(cr->GetMapId());
    if (map == nullptr) {
        return;
    }

    cr->Moving.IsRunning = is_run;
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

    const auto base_move_speed = static_cast<float>(cr->Moving.IsRunning ? cr->GetRunSpeed() : cr->GetWalkSpeed());

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

auto FOServer::DialogCheckDemand(Critter* npc, Critter* cl, DialogAnswer& answer, bool recheck) -> bool
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

        switch (demand.Type) {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_CRITTER_DICT:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP: {
            Entity* entity = nullptr;
            const PropertyRegistrator* prop_registrator = nullptr;
            if (demand.Type == DR_PROP_GLOBAL) {
                entity = master;
                prop_registrator = GetPropertyRegistrator(GameProperties::ENTITY_CLASS_NAME);
            }
            else if (demand.Type == DR_PROP_CRITTER) {
                entity = master;
                prop_registrator = GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME);
            }
            else if (demand.Type == DR_PROP_CRITTER_DICT) {
                entity = master;
                prop_registrator = GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME);
            }
            else if (demand.Type == DR_PROP_ITEM) {
                entity = master->GetItemSlot(1);
                prop_registrator = GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME);
            }
            else if (demand.Type == DR_PROP_LOCATION) {
                auto* map = MapMngr.GetMap(master->GetMapId());
                entity = (map != nullptr ? map->GetLocation() : nullptr);
                prop_registrator = GetPropertyRegistrator(LocationProperties::ENTITY_CLASS_NAME);
            }
            else if (demand.Type == DR_PROP_MAP) {
                entity = MapMngr.GetMap(master->GetMapId());
                prop_registrator = GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME);
            }
            if (entity == nullptr) {
                break;
            }

            const auto* prop = prop_registrator->GetByIndex(demand.ParamIndex);
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
                val = entity->GetProperties().GetPlainDataValueAsInt(prop);
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

auto FOServer::DialogUseResult(Critter* npc, Critter* cl, DialogAnswer& answer) -> uint
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

        switch (result.Type) {
        case DR_PROP_GLOBAL:
        case DR_PROP_CRITTER:
        case DR_PROP_CRITTER_DICT:
        case DR_PROP_ITEM:
        case DR_PROP_LOCATION:
        case DR_PROP_MAP: {
            Entity* entity = nullptr;
            const PropertyRegistrator* prop_registrator = nullptr;
            if (result.Type == DR_PROP_GLOBAL) {
                entity = master;
                prop_registrator = GetPropertyRegistrator(GameProperties::ENTITY_CLASS_NAME);
            }
            else if (result.Type == DR_PROP_CRITTER) {
                entity = master;
                prop_registrator = GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME);
            }
            else if (result.Type == DR_PROP_CRITTER_DICT) {
                entity = master;
                prop_registrator = GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME);
            }
            else if (result.Type == DR_PROP_ITEM) {
                entity = master->GetItemSlot(1);
                prop_registrator = GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME);
            }
            else if (result.Type == DR_PROP_LOCATION) {
                auto* map = MapMngr.GetMap(master->GetMapId());
                entity = (map != nullptr ? map->GetLocation() : nullptr);
                prop_registrator = GetPropertyRegistrator(LocationProperties::ENTITY_CLASS_NAME);
            }
            else if (result.Type == DR_PROP_MAP) {
                entity = MapMngr.GetMap(master->GetMapId());
                prop_registrator = GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME);
            }
            if (entity == nullptr) {
                break;
            }

            // Todo: restore DialogUseResult
            UNUSED_VARIABLE(prop_registrator);
            throw NotImplementedException(LINE_STR);
            /*const auto* prop = prop_registrator->GetByIndex(index);
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

void FOServer::BeginDialog(Critter* cl, Critter* npc, hstring dlg_pack_id, ushort hx, ushort hy, bool ignore_distance)
{
    if (cl->Talk.Locked) {
        WriteLog("Dialog locked, client '{}'", cl->GetName());
        return;
    }
    if (cl->Talk.Type != TalkType::None) {
        CrMngr.CloseTalk(cl);
    }

    DialogPack* dialog_pack;
    vector<Dialog>* dialogs;

    // Talk with npc
    if (npc != nullptr) {
        if (npc->GetIsNoTalk()) {
            return;
        }

        if (dlg_pack_id) {
            const auto npc_dlg_id = npc->GetDialogId();
            if (npc_dlg_id) {
                return;
            }

            dlg_pack_id = npc_dlg_id;
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

        dialog_pack = DlgMngr.GetDialog(dlg_pack_id);
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

        dialog_pack = DlgMngr.GetDialog(dlg_pack_id);
        dialogs = (dialog_pack != nullptr ? &dialog_pack->Dialogs : nullptr);
        if (dialogs == nullptr || dialogs->empty()) {
            WriteLog("No dialogs, hx {}, hy {}, client '{}'", hx, hy, cl->GetName());
            return;
        }
    }

    // Predialogue installations
    auto it_d = dialogs->begin();
    auto go_dialog = uint(-1);
    auto it_a = (*it_d).Answers.begin();
    for (; it_a != (*it_d).Answers.end(); ++it_a) {
        if (DialogCheckDemand(npc, cl, *it_a, false)) {
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
    const auto force_dialog = DialogUseResult(npc, cl, (*it_a));
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

    cl->Talk.DialogPackId = dlg_pack_id;
    cl->Talk.LastDialogId = go_dialog;
    cl->Talk.StartTick = GameTime.GameTick();
    cl->Talk.TalkTime = Settings.DlgTalkMaxTime;
    cl->Talk.Barter = false;
    cl->Talk.IgnoreDistance = ignore_distance;

    // Get lexems
    cl->Talk.Lexems.clear();
    if (cl->Talk.CurDialog.DlgScriptFunc) {
        cl->Talk.Locked = true;
        if (!ScriptSys->CallFunc<string, Critter*, Critter*>(cl->Talk.CurDialog.DlgScriptFunc, cl, npc, cl->Talk.Lexems)) {
            // Nop
        }
        cl->Talk.Locked = false;
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
    Critter* cr = player->GetOwnedCritter();

    uchar is_npc = 0;
    uint talk_id = 0;
    uchar num_answer = 0;

    player->Connection->Bin >> is_npc;
    player->Connection->Bin >> talk_id;
    player->Connection->Bin >> num_answer;

    if ((is_npc != 0u && (cr->Talk.Type != TalkType::Critter || cr->Talk.CritterId != talk_id)) || (is_npc == 0u && (cr->Talk.Type != TalkType::Hex || cr->Talk.DialogPackId.as_uint() != talk_id))) {
        CrMngr.CloseTalk(cr);
        WriteLog("Invalid talk id {}, client '{}'", is_npc, talk_id, cr->GetName());
        return;
    }

    if (cr->GetIsHide()) {
        cr->SetIsHide(false);
    }
    CrMngr.ProcessTalk(cr, true);

    Critter* npc = nullptr;

    // Find npc
    if (is_npc != 0u) {
        npc = CrMngr.GetCritter(talk_id);
        if (npc == nullptr) {
            cr->Send_TextMsg(cr, STR_DIALOG_NPC_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
            CrMngr.CloseTalk(cr);
            WriteLog("Npc with id {} not found, client '{}'", talk_id, cr->GetName());
            return;
        }
    }

    // Set dialogs
    auto* dialog_pack = DlgMngr.GetDialog(cr->Talk.DialogPackId);
    auto* dialogs = (dialog_pack != nullptr ? &dialog_pack->Dialogs : nullptr);
    if ((dialogs == nullptr) || dialogs->empty()) {
        CrMngr.CloseTalk(cr);
        WriteLog("No dialogs, npc '{}', client '{}'", npc->GetName(), cr->GetName());
        return;
    }

    // Continue dialog
    auto* cur_dialog = &cr->Talk.CurDialog;
    const auto last_dialog = cur_dialog->Id;
    uint dlg_id;
    uint force_dialog = 0;
    DialogAnswer* answer = nullptr;

    if (!cr->Talk.Barter) {
        // Barter
        if (num_answer == ANSWER_BARTER) {
            goto label_Barter;
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
        answer = &(*(cur_dialog->Answers.begin() + num_answer));

        // Check demand again
        if (!DialogCheckDemand(npc, cr, *answer, true)) {
            WriteLog("Secondary check of dialog demands fail, client '{}'", cr->GetName());
            CrMngr.CloseTalk(cr); // End
            return;
        }

        // Use result
        force_dialog = DialogUseResult(npc, cr, *answer);
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
                cr->Send_TextMsg(npc, STR_BARTER_NO_BARTER_NOW, SAY_DIALOG, TEXTMSG_GAME);
                return;
            }

            if (!npc->OnBarter.Fire(cr, true, npc->GetBarterPlayers() + 1) || !OnCritterBarter.Fire(npc, cr, true, npc->GetBarterPlayers() + 1)) {
                cr->Talk.Barter = true;
                cr->Talk.StartTick = GameTime.GameTick();
                cr->Talk.TalkTime = Settings.DlgBarterMaxTime;
            }
            return;
        case static_cast<uint>(-2):
            [[fallthrough]];
        case DIALOG_BACK:
            if (cr->Talk.LastDialogId != 0u) {
                dlg_id = cr->Talk.LastDialogId;
                break;
            }
            [[fallthrough]];
        case static_cast<uint>(-1):
            [[fallthrough]];
        case DIALOG_END:
            CrMngr.CloseTalk(cr);
            return;
        default:
            break;
        }
    }
    else {
        cr->Talk.Barter = false;
        dlg_id = cur_dialog->Id;
        if (npc != nullptr) {
            npc->OnBarter.Fire(cr, false, npc->GetBarterPlayers() + 1);
            OnCritterBarter.Fire(npc, cr, false, npc->GetBarterPlayers() + 1);
        }
    }

    // Find dialog
    const auto it_d = std::find_if(dialogs->begin(), dialogs->end(), [dlg_id](const Dialog& dlg) { return dlg.Id == dlg_id; });
    if (it_d == dialogs->end()) {
        CrMngr.CloseTalk(cr);
        cr->Send_TextMsg(cr, STR_DIALOG_FROM_LINK_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog from link {} not found, client '{}', dialog pack {}", dlg_id, cr->GetName(), dialog_pack->PackId);
        return;
    }

    // Compile
    if (!DialogCompile(npc, cr, *it_d, cr->Talk.CurDialog)) {
        CrMngr.CloseTalk(cr);
        cr->Send_TextMsg(cr, STR_DIALOG_COMPILE_FAIL, SAY_NETMSG, TEXTMSG_GAME);
        WriteLog("Dialog compile fail, client '{}', dialog pack {}", cr->GetName(), dialog_pack->PackId);
        return;
    }

    if (dlg_id != last_dialog) {
        cr->Talk.LastDialogId = last_dialog;
    }

    // Get lexems
    cr->Talk.Lexems.clear();
    if (cr->Talk.CurDialog.DlgScriptFunc) {
        cr->Talk.Locked = true;
        if (!ScriptSys->CallFunc<string, Critter*, Critter*>(cr->Talk.CurDialog.DlgScriptFunc, cr, npc, cr->Talk.Lexems)) {
            // Nop
        }
        cr->Talk.Locked = false;
    }

    // On head text
    if (cr->Talk.CurDialog.Answers.empty()) {
        if (npc != nullptr) {
            npc->SendAndBroadcast_MsgLex(npc->VisCr, cr->Talk.CurDialog.TextId, SAY_NORM_ON_HEAD, TEXTMSG_DLG, cr->Talk.Lexems);
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

auto FOServer::CreateItemOnHex(Map* map, ushort hx, ushort hy, hstring pid, uint count, Properties* props, bool check_blocks) -> Item*
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

auto FOServer::DialogScriptDemand(DemandResult& /*demand*/, Critter* /*master*/, Critter* /*slave*/) -> bool
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

auto FOServer::DialogScriptResult(DemandResult& /*result*/, Critter* /*master*/, Critter* /*slave*/) -> uint
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

auto FOServer::MakePlayerId(string_view player_name) const -> uint
{
    RUNTIME_ASSERT(!player_name.empty());
    const auto hash_value = Hashing::MurmurHash2(reinterpret_cast<const uchar*>(player_name.data()), static_cast<uint>(player_name.length()));
    RUNTIME_ASSERT(hash_value);
    return (1u << 31u) | hash_value;
}
