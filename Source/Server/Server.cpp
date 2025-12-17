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

#include "Server.h"
#include "AnyData.h"
#include "Application.h"
#include "ImGuiStuff.h"
#include "NetCommand.h"
#include "PropertiesSerializator.h"
#include "Version-Include.h"

FO_BEGIN_NAMESPACE();

extern void Server_RegisterData(EngineData*);
extern void InitServerEngine(FOServer*);

#if FO_ANGELSCRIPT_SCRIPTING
extern void Init_AngelScript_ServerScriptSystem(BaseEngine*);
#endif

static auto GetServerResources(GlobalSettings& settings) -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    FileSystem resources;
    resources.AddPacksSource(IsPackaged() ? settings.ServerResources : settings.BakeOutput, settings.ServerResourceEntries);
    return resources;
}

FOServer::FOServer(GlobalSettings& settings) :
    BaseEngine(settings, GetServerResources(settings), PropertiesRelationType::ServerRelative, [this] { Server_RegisterData(this); }),
    EntityMngr(this),
    MapMngr(this),
    CrMngr(this),
    ItemMngr(this)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Start server");

    _starter.SetExceptionHandler([this](const std::exception& ex) {
        ignore_unused(ex);

        _startingError = true;

        // Clear jobs
        return true;
    });

    // Health file
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitHealthFileJob");

        if (Settings.WriteHealthFile) {
            const auto exe_path = Platform::GetExePath();
            const string health_file_name = strex("{}_Health.txt", exe_path ? strex(exe_path.value()).extract_file_name().erase_file_extension().str() : FO_DEV_NAME);

            const auto write_health_file = [health_file_name](string_view text) {
                if (auto health_file = DiskFileSystem::OpenFile(health_file_name, true, true)) {
                    return health_file.Write(text);
                }
                else {
                    return false;
                }
            };

            if (write_health_file("Starting...")) {
                _mainWorker.AddJob([this, write_health_file] {
                    FO_STACK_TRACE_ENTRY_NAMED("HealthFileJob");

                    if (_started && _healthWriter.GetJobsCount() == 0) {
                        _healthWriter.AddJob([health_info = GetHealthInfo(), write_health_file, &settings_ = Settings] {
                            FO_STACK_TRACE_ENTRY_NAMED("HealthFileWriteJob");

                            string buf;
                            buf.reserve(health_info.size() + 128);
                            buf += strex("{} v{}\n\n", settings_.GameName, settings_.GameVersion);
                            buf += health_info;
                            write_health_file(buf);

                            return std::nullopt;
                        });
                    }

                    return std::chrono::milliseconds {300};
                });
            }
            else {
                WriteLog("Can't write health file '{}'", health_file_name);
            }
        }

        return std::nullopt;
    });

    // Script system
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitScriptSystemJob");

        WriteLog("Initialize script system");

        ScriptSys.MapEngineEntityType<Player>(Player::ENTITY_TYPE_NAME);
        ScriptSys.MapEngineEntityType<Item>(Item::ENTITY_TYPE_NAME);
        ScriptSys.MapEngineEntityType<Critter>(Critter::ENTITY_TYPE_NAME);
        ScriptSys.MapEngineEntityType<Map>(Map::ENTITY_TYPE_NAME);
        ScriptSys.MapEngineEntityType<Location>(Location::ENTITY_TYPE_NAME);

#if FO_ANGELSCRIPT_SCRIPTING
        Init_AngelScript_ServerScriptSystem(this);
#endif

        return std::nullopt;
    });

    // Network
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitNetworkingJob");

        WriteLog("Start networking");

        if (auto conn_server = NetworkServer::StartInterthreadServer(Settings, [this](shared_ptr<NetworkServerConnection> net_connection) { OnNewConnection(std::move(net_connection)); })) {
            _connectionServers.emplace_back(std::move(conn_server));
        }

#if FO_HAVE_ASIO
        if (auto conn_server = NetworkServer::StartAsioServer(Settings, [this](shared_ptr<NetworkServerConnection> net_connection) { OnNewConnection(std::move(net_connection)); })) {
            _connectionServers.emplace_back(std::move(conn_server));
        }
#endif
#if FO_HAVE_WEB_SOCKETS
        if (auto conn_server = NetworkServer::StartWebSocketsServer(Settings, [this](shared_ptr<NetworkServerConnection> net_connection) { OnNewConnection(std::move(net_connection)); })) {
            _connectionServers.emplace_back(std::move(conn_server));
        }
#endif

        return std::nullopt;
    });

    // Data base
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitStorageJob");

        DbStorage = ConnectToDataBase(Settings, Settings.DbStorage);
        if (!DbStorage) {
            throw ServerInitException("Can't init storage data base", Settings.DbStorage);
        }

        return std::nullopt;
    });

    // Engine data
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitEngineDataJob");

        WriteLog("Setup engine data");

        // Properties that saving to database
        for (const auto& entity_info : GetEntityTypesInfo() | std::views::values) {
            const auto& registrator = entity_info.PropRegistrator;

            for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
                const auto* prop = registrator->GetPropertyByIndex(numeric_cast<int32>(i));

                if (prop->IsDisabled()) {
                    continue;
                }
                if (!prop->IsPersistent()) {
                    continue;
                }

                prop->AddPostSetter([this](Entity* entity, const Property* prop_) { OnSaveEntityValue(entity, prop_); });
            }
        }

        // Properties that sending to clients
        {
            const auto set_send_callbacks = [](const auto* registrator, const PropertyPostSetCallback& callback) {
                for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
                    const auto* prop = registrator->GetPropertyByIndex(numeric_cast<int32>(i));

                    if (prop->IsDisabled()) {
                        continue;
                    }
                    if (!prop->IsSynced()) {
                        continue;
                    }

                    prop->AddPostSetter(callback);
                }
            };

            set_send_callbacks(GetPropertyRegistrator(GameProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendGlobalValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(PlayerProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendPlayerValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendItemValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendCritterValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendMapValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(LocationProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) { OnSendLocationValue(entity, prop); });

            for (const auto& entity_info : GetEntityTypesInfo() | std::views::values) {
                if (entity_info.Exported) {
                    continue;
                }

                set_send_callbacks(entity_info.PropRegistrator.get(), [this](Entity* entity, const Property* prop) { OnSendCustomEntityValue(entity, prop); });
            }
        }

        // Properties with custom behaviours
        {
            const auto set_setter = [](const auto* registrator, int32 prop_index, PropertySetCallback callback) {
                const auto* prop = registrator->GetPropertyByIndex(prop_index);
                prop->AddSetter(std::move(callback));
            };
            const auto set_post_setter = [](const auto* registrator, int32 prop_index, PropertyPostSetCallback callback) {
                const auto* prop = registrator->GetPropertyByIndex(prop_index);
                prop->AddPostSetter(std::move(callback));
            };

            set_post_setter(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), Critter::LookDistance_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterLook(entity, prop); });
            set_post_setter(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), Critter::InSneakMode_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterLook(entity, prop); });
            set_post_setter(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), Critter::SneakCoefficient_RegIndex, [this](Entity* entity, const Property* prop) { OnSetCritterLook(entity, prop); });
            set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::Count_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) { OnSetItemCount(entity, prop, data.GetPtrAs<void>()); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::Hidden_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemChangeView(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::AlwaysView_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemChangeView(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::IsTrap_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemChangeView(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::TrapValue_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemChangeView(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::NoBlock_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemRecacheHex(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::ShootThru_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemRecacheHex(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::IsGag_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemRecacheHex(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::IsTrigger_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemRecacheHex(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::MultihexLines_RegIndex, [this](Entity* entity, const Property* prop) { OnSetItemMultihexLines(entity, prop); });
        }

        return std::nullopt;
    });

    // Protos
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitProtosJob");

        WriteLog("Load protos data");

        ProtoMngr.LoadFromResources(Resources);

        _defaultLang = LanguagePack {Settings.Language, *this};
        _defaultLang.LoadFromResources(Resources);

        return std::nullopt;
    });

    // Maps
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitMapsJob");

        WriteLog("Load maps data");

        MapMngr.LoadFromResources();

        return std::nullopt;
    });

    // Resource packs for client
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitClientPacksJob");

        if (IsPackaged()) {
            WriteLog("Load client data packs for synchronization");

            FileSystem client_resources;
            client_resources.AddDirSource(Settings.ClientResources, false, true, true);

            auto writer = DataWriter(_updateFilesDesc);

            const auto add_sync_file = [&client_resources, &writer, this](string_view path) {
                const auto file = client_resources.ReadFile(path);

                if (!file) {
                    throw ServerInitException("Resource pack for client not found", path);
                }

                const auto data = file.GetData();
                _updateFilesData.push_back(data);

                writer.Write<int16>(numeric_cast<int16>(file.GetPath().length()));
                writer.WritePtr(file.GetPath().data(), file.GetPath().length());
                writer.Write<uint32>(numeric_cast<uint32>(data.size()));
                writer.Write<uint32>(Hashing::MurmurHash2(data.data(), data.size()));
            };

            for (const auto& resource_entry : Settings.ClientResourceEntries) {
                if (resource_entry != "Embedded") {
                    add_sync_file(strex("{}.zip", resource_entry));
                }
            }

            // Complete files list
            writer.Write<int16>(const_numeric_cast<int16>(-1));
        }

        return std::nullopt;
    });

    // Game logic
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitGameLogicJob");

        WriteLog("Start game logic");

        try {
            // Globals
            const auto globals_doc = DbStorage.Get(GameCollectionName, ident_t {1});

            if (globals_doc.Empty()) {
                DbStorage.Insert(GameCollectionName, ident_t {1}, {});
                SetSynchronizedTime(synctime(std::chrono::milliseconds {1}));
            }
            else {
                if (!PropertiesSerializator::LoadFromDocument(&GetPropertiesForEdit(), globals_doc, Hashes, *this)) {
                    throw ServerInitException("Failed to load globals document");
                }
            }

            GameTime.SetSynchronizedTime(GetSynchronizedTime());
            FrameAdvance();

            // Scripting
            WriteLog("Init script modules");

            InitServerEngine(this);

            ScriptSys.InitModules();

            if (!OnInit.Fire()) {
                throw ServerInitException("Initialization script failed");
            }

            // Init world
            if (globals_doc.Empty()) {
                WriteLog("Generate world");

                if (!OnGenerateWorld.Fire()) {
                    throw ServerInitException("Generate world script failed");
                }
            }
            else {
                WriteLog("Restore world");

                size_t errors = 0;

                try {
                    EntityMngr.LoadEntities();
                }
                catch (const std::exception& ex) {
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
        }
        catch (...) {
            // Don't change database
            DbStorage.ClearChanges();

            throw;
        }

        // Commit initial database changes
        DbStorage.CommitChanges(true);

        // Advance time after initialization
        FrameAdvance();

        return std::nullopt;
    });

    // Done, fill regular jobs
    _starter.AddJob([this] {
        FO_STACK_TRACE_ENTRY_NAMED("InitDoneJob");

        WriteLog("Start server complete!");

        _started = true;

        _loopBalancer = FrameBalancer {true, Settings.ServerSleep, Settings.LoopsPerSecondCap};
        _loopBalancer.StartLoop();

        _stats.LoopCounterBegin = nanotime::now();
        _stats.ServerStartTime = nanotime::now();

        // Sync point
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("SyncPointJob");

            SyncPoint();

            return std::chrono::milliseconds {0};
        });

        // Advance time
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("FrameTimeJob");

            FrameAdvance();

            return std::chrono::milliseconds {0};
        });

        // Script subsystems update
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("ScriptSystemJob");

            ScriptSys.Process();

            return std::chrono::milliseconds {0};
        });

        // Process unlogined players
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("UnloginedPlayersJob");

            {
                std::scoped_lock locker(_newConnectionsLocker);

                if (!_newConnections.empty()) {
                    while (!_newConnections.empty()) {
                        auto conn = std::move(_newConnections.back());
                        _newConnections.pop_back();
                        _unloginedPlayers.emplace_back(SafeAlloc::MakeRefCounted<Player>(this, ident_t {}, SafeAlloc::MakeUnique<ServerConnection>(Settings, conn)));
                    }
                }
            }

            for (auto& player : copy_hold_ref(_unloginedPlayers)) {
                auto* connection = player->GetConnection();

                try {
                    ProcessConnection(connection);
                    ProcessUnloginedPlayer(player.get());
                }
                catch (const UnknownMessageException&) {
                    WriteLog("Invalid network data from host {}:{}", connection->GetHost(), connection->GetPort());
                    connection->HardDisconnect();
                }
                catch (const NetBufferException& ex) {
                    ReportExceptionAndContinue(ex);
                    connection->HardDisconnect();
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }
            }

            _stats.CurOnline = _unloginedPlayers.size() + EntityMngr.GetPlayers().size();
            _stats.MaxOnline = std::max(_stats.MaxOnline, _stats.CurOnline);

            return std::chrono::milliseconds {0};
        });

        // Process players
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("PlayersJob");

            for (auto* player : copy_hold_ref(EntityMngr.GetPlayers())) {
                auto* connection = player->GetConnection();

                try {
                    FO_RUNTIME_ASSERT(!player->IsDestroyed());

                    ProcessConnection(connection);
                    ProcessPlayer(player);
                }
                catch (const NetBufferException& ex) {
                    ReportExceptionAndContinue(ex);
                    connection->HardDisconnect();
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }
            }

            return std::chrono::milliseconds {0};
        });

        // Process critters
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("CrittersJob");

            for (auto* cr : copy_hold_ref(EntityMngr.GetCritters())) {
                if (cr->IsDestroyed()) {
                    continue;
                }

                try {
                    ProcessCritterMoving(cr);
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }
            }

            return std::chrono::milliseconds {0};
        });

        // Time events
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("TimeEventsJob");

            TimeEventMngr.ProcessTimeEvents();

            return std::chrono::milliseconds {0};
        });

        // Commit data to storage
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("StorageCommitJob");

            DbStorage.CommitChanges(false);

            return std::chrono::milliseconds {Settings.DataBaseCommitPeriod};
        });

        // Clients log
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("LogDispatchJob");

            DispatchLogToClients();

            return std::chrono::milliseconds {0};
        });

        // Loop stats
        _mainWorker.AddJob([this] {
            FO_STACK_TRACE_ENTRY_NAMED("LoopJob");

            _loopBalancer.EndLoop();
            _loopBalancer.StartLoop();

            const auto cur_time = nanotime::now();
            const auto loop_duration = _loopBalancer.GetLoopDuration();

            // Calculate loop average time
            _stats.LoopTimeStamps.emplace_back(cur_time, loop_duration);
            _stats.LoopWholeAvgTime += loop_duration;

            while (cur_time - _stats.LoopTimeStamps.front().first > std::chrono::milliseconds {Settings.LoopAverageTimeInterval}) {
                _stats.LoopWholeAvgTime -= _stats.LoopTimeStamps.front().second;
                _stats.LoopTimeStamps.pop_front();
            }

            _stats.LoopAvgTime = _stats.LoopWholeAvgTime.value() / _stats.LoopTimeStamps.size();

            // Calculate loops per second
            if (cur_time - _stats.LoopCounterBegin >= std::chrono::seconds(1)) {
                _stats.LoopsPerSecond = _stats.LoopCounter;
                _stats.LoopCounter = 0;
                _stats.LoopCounterBegin = cur_time;
            }
            else {
                _stats.LoopCounter++;
            }

            // Fill statistics
            _stats.LoopsCount++;
            _stats.LoopMaxTime = _stats.LoopMaxTime ? std::max(loop_duration, _stats.LoopMaxTime) : loop_duration;
            _stats.LoopMinTime = _stats.LoopMinTime ? std::min(loop_duration, _stats.LoopMinTime) : loop_duration;
            _stats.LoopLastTime = loop_duration;
            _stats.Uptime = cur_time - _stats.ServerStartTime;

#if FO_TRACY
            TracyPlot("Server loops per second", numeric_cast<int64>(_stats.LoopsPerSecond));
#endif

            return std::chrono::milliseconds {0};
        });

        return std::nullopt;
    });
}

FOServer::~FOServer()
{
    FO_STACK_TRACE_ENTRY();

    try {
        WriteLog("Stop server");

        _willFinishDispatcher();

        // Finish logic
        _starter.Clear();
        _mainWorker.Clear();
        _healthWriter.Clear();

        if (_started) {
            _mainWorker.AddJob([this] {
                OnFinish.Fire();
                EntityMngr.DestroyInnerEntities(this);
                EntityMngr.DestroyAllEntities();
                DbStorage.CommitChanges(true);

                return std::nullopt;
            });

            _mainWorker.Wait();
        }

        _started = false;

        // Shutdown servers
        for (auto& conn_server : _connectionServers) {
            conn_server->Shutdown();
        }
        _connectionServers.clear();

        // Logging clients
        SetLogCallback("LogToClients", nullptr);
        _logClients.clear();

        // Logined players
        for (auto& player : copy(EntityMngr.GetPlayers()) | std::views::values) {
            player->GetConnection()->HardDisconnect();
        }

        // Unlogined players
        for (auto& player : _unloginedPlayers) {
            player->GetConnection()->HardDisconnect();
            player->MarkAsDestroyed();
        }
        _unloginedPlayers.clear();

        // New connections
        {
            std::scoped_lock locker(_newConnectionsLocker);

            for (auto& connection : _newConnections) {
                connection->Disconnect();
            }
            _newConnections.clear();
        }

        // Done
        WriteLog("Server stopped!");

        _didFinishDispatcher();
    }
    catch (const std::exception& ex) {
        WriteLog("Server stopped with error!");

        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

auto FOServer::Lock(optional<timespan> max_wait_time) -> bool
{
    FO_STACK_TRACE_ENTRY();

    while (!_started) {
        std::this_thread::yield();
    }

    if (std::this_thread::get_id() != _mainWorker.GetThreadId()) {
        std::unique_lock locker {_syncLocker};

        _syncRequest++;

        while (!_syncPointReady) {
            if (max_wait_time.has_value()) {
                if (_syncWaitSignal.wait_for(locker, max_wait_time.value().value()) == std::cv_status::timeout) {
                    _syncRequest--;
                    return false;
                }
            }
            else {
                _syncWaitSignal.wait(locker);
            }
        }
    }

    return true;
}

void FOServer::Unlock()
{
    FO_STACK_TRACE_ENTRY();

    if (std::this_thread::get_id() != _mainWorker.GetThreadId()) {
        std::unique_lock locker {_syncLocker};

        FO_RUNTIME_ASSERT(_syncRequest > 0);

        _syncRequest--;

        locker.unlock();
        _syncRunSignal.notify_one();
    }
}

void FOServer::SyncPoint()
{
    FO_STACK_TRACE_ENTRY();

    std::unique_lock locker {_syncLocker};

    if (_syncRequest > 0) {
        _syncPointReady = true;

        locker.unlock();
        _syncWaitSignal.notify_all();
        locker.lock();

        while (_syncRequest > 0) {
            _syncRunSignal.wait(locker);
        }

        _syncPointReady = false;
    }
}

void FOServer::DrawGui(string_view server_name)
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto default_buf_size = 1000000; // ~1mb
    string buf;
    buf.reserve(default_buf_size);

    if (ImGui::Begin(string(server_name).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!_started) {
            if (!_startingError) {
                ImGui::TextUnformatted("Server is starting...");
            }
            else {
                ImGui::TextUnformatted("Server starting error, see log");
            }
        }
        else {
            if (Settings.LockMaxWaitTime != 0) {
                const auto max_wait_time = timespan {std::chrono::milliseconds {Settings.LockMaxWaitTime}};
                if (!Lock(max_wait_time)) {
                    ImGui::TextUnformatted(strex("Server hanged (no response more than {})", max_wait_time).c_str());
                    WriteLog("Server hanged (no response more than {})", max_wait_time);
                    ImGui::End();
                    return;
                }
            }
            else {
                Lock(std::nullopt);
            }

            auto unlocker = ScopeCallback([this]() noexcept { safe_call([this] { Unlock(); }); });

            // Info
            ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
            if (ImGui::TreeNode("Info")) {
                buf = GetHealthInfo();
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
                buf = GetLocationAndMapsStatistics();
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
    }

    ImGui::End();
}

auto FOServer::GetHealthInfo() const -> string
{
    string buf;
    buf.reserve(2048);

    buf += strex("System time: {}\n", nanotime::now());
    buf += strex("Synchronized time: {}\n", GetSynchronizedTime());
    buf += strex("Server uptime: {}\n", _stats.Uptime);
    buf += strex("Connections: {}\n", _stats.CurOnline);
    buf += strex("Players: {}\n", EntityMngr.GetPlayersCount());
    buf += strex("Critters: {}\n", EntityMngr.GetCrittersCount());
    buf += strex("Locations: {}\n", EntityMngr.GetLocationsCount());
    buf += strex("Maps: {}\n", EntityMngr.GetMapsCount());
    buf += strex("Items: {}\n", EntityMngr.GetItemsCount());
    buf += strex("Loops per second: {}\n", _stats.LoopsPerSecond);
    buf += strex("Average loop time: {}\n", _stats.LoopAvgTime);
    buf += strex("Min loop time: {}\n", _stats.LoopMinTime);
    buf += strex("Max loop time: {}\n", _stats.LoopMaxTime);
    buf += strex("KBytes Send: {}\n", _stats.BytesSend / 1024);
    buf += strex("KBytes Recv: {}\n", _stats.BytesRecv / 1024);
    buf += strex("Compress ratio: {}\n", numeric_cast<float64>(_stats.DataReal) / numeric_cast<float64>(_stats.DataCompressed != 0 ? _stats.DataCompressed : 1));
    buf += strex("DB commit jobs: {}\n", DbStorage.GetCommitJobsCount());

    return buf;
}

auto FOServer::GetIngamePlayersStatistics() -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto& players = EntityMngr.GetPlayers();
    const auto conn_count = _unloginedPlayers.size() + players.size();

    string result = strex("Players: {}\nConnections: {}\n", players.size(), conn_count);
    result += "Name                 Id         Ip              X     Y     Location and map\n";

    for (const auto& player : players | std::views::values) {
        const auto* cr = player->GetControlledCritter();
        const auto* map = EntityMngr.GetMap(cr->GetMapId());
        const auto* loc = map != nullptr ? map->GetLocation() : nullptr;

        const string str_loc = strex("{} ({}) {} ({})", map != nullptr ? loc->GetName() : "", map != nullptr ? loc->GetId() : ident_t {}, map != nullptr ? map->GetName() : "", map != nullptr ? map->GetId() : ident_t {});
        result += strex("{:<20} {:<10} {:<15} {:<5} {}\n", player->GetName(), player->GetId(), player->GetConnection()->GetHost(), cr->GetHex(), map != nullptr ? str_loc : "Global map");
    }

    return result;
}

auto FOServer::GetLocationAndMapsStatistics() -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto& locations = EntityMngr.GetLocations();
    const auto& maps = EntityMngr.GetMaps();

    string result = strex("Locations count: {}\n", locations.size());
    result += strex("Maps count: {}\n", maps.size());
    result += "Location             Id\n";
    result += "          Map                 Id          Time Script\n";

    for (const auto& loc : locations | std::views::values) {
        result += strex("{:<20} {:<10}\n", loc->GetName(), loc->GetId());

        int32 map_index = 0;

        for (const auto& map : loc->GetMaps()) {
            result += strex("     {:02}) {:<20} {:<9}   {:<4} {}\n", map_index, map->GetName(), map->GetId(), map->GetFixedDayTime(), map->GetInitScript());
            map_index++;
        }
    }

    return result;
}

void FOServer::OnNewConnection(shared_ptr<NetworkServerConnection> net_connection)
{
    FO_STACK_TRACE_ENTRY();

    if (!_started) {
        net_connection->Disconnect();
        return;
    }

    std::scoped_lock locker(_newConnectionsLocker);

    _newConnections.emplace_back(net_connection);
}

void FOServer::ProcessUnloginedPlayer(Player* unlogined_player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = unlogined_player->GetConnection();

    if (connection->IsHardDisconnected()) {
        const auto it = std::ranges::find(_unloginedPlayers, unlogined_player);
        FO_RUNTIME_ASSERT(it != _unloginedPlayers.end());
        _unloginedPlayers.erase(it);
        unlogined_player->MarkAsDestroyed();
        return;
    }

    auto in_buf = connection->ReadBuf();

    if (connection->IsGracefulDisconnected()) {
        in_buf->ResetBuf();
        return;
    }

    if (in_buf->NeedProcess()) {
        const auto msg = in_buf->ReadMsg();

        in_buf.Unlock();

        if (!connection->WasHandshake) {
            if (msg == NetMessage::Handshake) {
                Process_Handshake(connection);
            }
            else {
                throw GenericException("Expected handshake message", msg);
            }
        }
        else {
            switch (msg) {
            case NetMessage::Ping:
                Process_Ping(connection);
                break;
            case NetMessage::GetUpdateFile:
                Process_UpdateFile(connection);
                break;
            case NetMessage::GetUpdateFileData:
                Process_UpdateFileData(connection);
                break;

            case NetMessage::RemoteCall:
                Process_RemoteCall(unlogined_player);
                break;
            case NetMessage::Register:
                Process_Register(unlogined_player);
                break;
            case NetMessage::Login:
                Process_Login(unlogined_player); // Maybe invalidated in method call
                break;

            default:
                throw GenericException("Unexpected unlogined player message", msg);
            }
        }

        in_buf.Lock();
        in_buf->ShrinkReadBuf();

        connection->LastActivityTime = GameTime.GetFrameTime();
    }
}

void FOServer::ProcessPlayer(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();

    if (connection->IsHardDisconnected()) {
        WriteLog("Disconnected player {}", player->GetName());

        OnPlayerLogout.Fire(player);

        if (auto* cr = player->GetControlledCritter(); cr != nullptr) {
            cr->DetachPlayer();
        }

        player->MarkAsDestroyed();
        EntityMngr.UnregisterPlayer(player);
        return;
    }

    if (connection->IsGracefulDisconnected()) {
        auto in_buf = connection->ReadBuf();

        in_buf->ResetBuf();
        return;
    }

    while (!connection->IsHardDisconnected() && !connection->IsGracefulDisconnected()) {
        auto in_buf = connection->ReadBuf();

        if (!in_buf->NeedProcess()) {
            break;
        }

        const auto msg = in_buf->ReadMsg();

        in_buf.Unlock();

        switch (msg) {
        case NetMessage::Ping:
            Process_Ping(connection);
            break;

        case NetMessage::SendCommand:
            in_buf.Lock();
            Process_Command(*in_buf, [player](auto s) { player->Send_InfoMessage(EngineInfoMessage::ServerLog, strex(s).trim()); }, player);
            in_buf.Unlock();
            break;
        case NetMessage::SendCritterDir:
            Process_Dir(player);
            break;
        case NetMessage::SendCritterMove:
            Process_Move(player);
            break;
        case NetMessage::SendStopCritterMove:
            Process_StopMove(player);
            break;
        case NetMessage::RemoteCall:
            Process_RemoteCall(player);
            break;
        case NetMessage::SendProperty:
            Process_Property(player);
            break;
        default:
            throw GenericException("Unexpected player message", msg);
        }

        in_buf.Lock();
        in_buf->ShrinkReadBuf();

        connection->LastActivityTime = GameTime.GetFrameTime();
    }
}

void FOServer::ProcessConnection(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (connection->IsHardDisconnected()) {
        return;
    }

    if (!connection->LastActivityTime) {
        connection->LastActivityTime = GameTime.GetFrameTime();
    }

    if (Settings.InactivityDisconnectTime != 0 && GameTime.GetFrameTime() - connection->LastActivityTime >= std::chrono::milliseconds {Settings.InactivityDisconnectTime}) {
        WriteLog("Connection activity timeout from host '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    if (connection->WasHandshake && (!connection->PingNextTime || GameTime.GetFrameTime() >= connection->PingNextTime)) {
        if (!connection->PingOk && !IsRunInDebugger()) {
            connection->HardDisconnect();
            return;
        }

        auto out_buf = connection->WriteMsg(NetMessage::Ping);

        out_buf->Write(false);

        connection->PingNextTime = GameTime.GetFrameTime() + std::chrono::milliseconds {Settings.ClientPingTime};
        connection->PingOk = false;
    }
}

void FOServer::Process_Command(NetInBuffer& buf, const LogFunc& logcb, Player* player)
{
    FO_STACK_TRACE_ENTRY();

    SetLogCallback("Process_Command", logcb);
    auto remove_log_callback = ScopeCallback([]() noexcept { safe_call([] { SetLogCallback("Process_Command", nullptr); }); });

    const auto cmd = buf.Read<uint8>();
    auto* player_cr = player->GetControlledCritter();
    auto allow_command = OnPlayerAllowCommand.Fire(player, cmd);

    if (!allow_command) {
        logcb("Command refused");
        return;
    }

    switch (cmd) {
    case CMD_EXIT: {
        player->GetConnection()->GracefulDisconnect();
        logcb("Done");
    } break;
    case CMD_MYINFO: {
        string istr = strex("|0xFF00FF00 Name: |0xFFFF0000 {}|0xFF00FF00 , Id: |0xFFFF0000 {}", player_cr->GetName(), player_cr->GetId());
        logcb(istr);
    } break;
    case CMD_GAMEINFO: {
        const auto info = buf.Read<int32>();

        string result;
        switch (info) {
        case 1:
            result = GetIngamePlayersStatistics();
            break;
        case 2:
            result = GetLocationAndMapsStatistics();
            break;
        case 3:
            // result = ScriptSys.GetDeferredCallsStatistics();
            break;
        case 4:
            result = "WIP";
            break;
        default:
            break;
        }

        string str = strex("Unlogined players: {}, Logined players: {}, Critters: {}, Frame time: {}, Server uptime: {}", //
            _unloginedPlayers.size(), EntityMngr.GetPlayersCount(), EntityMngr.GetCrittersCount(), GameTime.GetFrameTime(), GameTime.GetFrameTime() - _stats.ServerStartTime);

        result += str;

        for (const auto& line : strex(result).split('\n')) {
            logcb(line);
        }
    } break;
    case CMD_CRITID: {
        const auto name = buf.Read<string>();
        const auto player_id = MakePlayerId(name);

        if (DbStorage.Valid(PlayersCollectionName, player_id)) {
            logcb(strex("Player id is {}", player_id));
        }
        else {
            logcb("Client not found");
        }
    } break;
    case CMD_MOVECRIT: {
        const auto cr_id = buf.Read<ident_t>();
        const auto cr_hex = buf.Read<mpos>();

        auto* cr = EntityMngr.GetCritter(cr_id);

        if (cr == nullptr) {
            logcb("Critter not found");
            break;
        }

        auto* map = EntityMngr.GetMap(cr->GetMapId());

        if (map == nullptr) {
            logcb("Critter is on global");
            break;
        }

        if (!map->GetSize().is_valid_pos(cr_hex)) {
            logcb("Invalid hex position");
            break;
        }

        MapMngr.TransferToMap(cr, map, cr_hex, cr->GetDir(), std::nullopt);
    } break;
    case CMD_DISCONCRIT: {
        const auto cr_id = buf.Read<ident_t>();

        if (player_cr != nullptr && player_cr->GetId() == cr_id) {
            logcb("To kick yourself type <~exit>");
            return;
        }

        auto* cr = EntityMngr.GetCritter(cr_id);

        if (cr == nullptr) {
            logcb("Critter not found");
            return;
        }

        if (!cr->GetControlledByPlayer()) {
            logcb("Founded critter is not controlled by player");
            return;
        }

        if (auto* player_ = cr->GetPlayer()) {
            player_->Send_InfoMessage(EngineInfoMessage::KickedFromGame);
            player_->GetConnection()->GracefulDisconnect();
            logcb("Player disconnected");
        }
        else {
            logcb("Player is not in a game");
        }
    } break;
    case CMD_TOGLOBAL: {
        MapMngr.TransferToGlobal(player_cr, {});
        logcb("Done");
    } break;
    case CMD_PROPERTY: {
        const auto cr_id = buf.Read<ident_t>();
        const auto property_name = buf.Read<string>();
        const auto is_set = buf.Read<bool>();
        const auto property_value = buf.Read<string>();

        auto* cr = !cr_id ? player_cr : EntityMngr.GetCritter(cr_id);

        if (cr != nullptr) {
            const auto* prop = GetPropertyRegistrator("Critter")->FindProperty(property_name);

            if (prop == nullptr) {
                logcb("Property not found");
                return;
            }

            if (is_set) {
                if (!prop->IsPlainData()) {
                    logcb("Property is not plain data type");
                    return;
                }

                cr->SetValueAsAny(prop, any_t {property_value});
                logcb("Done");
            }
            else {
                logcb(cr->GetValueAsAny(prop));
            }
        }
        else {
            logcb("Critter not found");
        }
    } break;
    case CMD_ADDITEM: {
        const auto hex = buf.Read<mpos>();
        const auto pid = buf.Read<hstring>(Hashes);
        const auto count = buf.Read<int32>();

        auto* map = EntityMngr.GetMap(player_cr->GetMapId());

        if (map == nullptr || !map->GetSize().is_valid_pos(hex)) {
            logcb("Wrong hexes or critter on global map");
            return;
        }

        CreateItemOnHex(map, hex, pid, count, nullptr);
        logcb("Item(s) added");
    } break;
    case CMD_ADDITEM_SELF: {
        const auto pid = buf.Read<hstring>(Hashes);
        const auto count = buf.Read<int32>();

        if (count > 0) {
            ItemMngr.AddItemCritter(player_cr, pid, count);
            logcb("Item(s) added");
        }
        else {
            logcb("No item(s) added");
        }
    } break;
    case CMD_ADDNPC: {
        const auto hex = buf.Read<mpos>();
        const auto dir = buf.Read<uint8>();
        const auto pid = buf.Read<hstring>(Hashes);

        auto* map = EntityMngr.GetMap(player_cr->GetMapId());
        CrMngr.CreateCritterOnMap(pid, nullptr, map, hex, dir);

        logcb("Npc created");
    } break;
    case CMD_ADDLOCATION: {
        const auto pid = buf.Read<hstring>(Hashes);
        auto* loc = MapMngr.CreateLocation(pid);

        if (loc == nullptr) {
            logcb("Location not created");
        }
        else {
            logcb("Location created");
        }
    } break;
    case CMD_RUNSCRIPT: {
        const auto func_name = any_t {buf.Read<string>()};
        const auto param0_str = any_t {buf.Read<string>()};
        const auto param1_str = any_t {buf.Read<string>()};
        const auto param2_str = any_t {buf.Read<string>()};

        if (func_name.empty()) {
            logcb("Fail, length is zero");
            break;
        }

        bool failed = false;
        const auto param0 = ResolveGenericValue(param0_str, &failed);
        const auto param1 = ResolveGenericValue(param1_str, &failed);
        const auto param2 = ResolveGenericValue(param2_str, &failed);

        if (ScriptSys.CallFunc<void, Player*>(Hashes.ToHashedString(func_name), player) || //
            ScriptSys.CallFunc<void, Player*, int32>(Hashes.ToHashedString(func_name), player, param0) || //
            ScriptSys.CallFunc<void, Player*, any_t>(Hashes.ToHashedString(func_name), player, param0_str) || //
            ScriptSys.CallFunc<void, Player*, int32, int32>(Hashes.ToHashedString(func_name), player, param0, param1) || //
            ScriptSys.CallFunc<void, Player*, any_t, any_t>(Hashes.ToHashedString(func_name), player, param0_str, param1_str) || //
            ScriptSys.CallFunc<void, Player*, int32, int32, int32>(Hashes.ToHashedString(func_name), player, param0, param1, param2) || //
            ScriptSys.CallFunc<void, Player*, any_t, any_t, any_t>(Hashes.ToHashedString(func_name), player, param0_str, param1_str, param2_str) || //
            ScriptSys.CallFunc<void, Critter*>(Hashes.ToHashedString(func_name), player_cr) || //
            ScriptSys.CallFunc<void, Critter*, int32>(Hashes.ToHashedString(func_name), player_cr, param0) || //
            ScriptSys.CallFunc<void, Critter*, any_t>(Hashes.ToHashedString(func_name), player_cr, param0_str) || //
            ScriptSys.CallFunc<void, Critter*, int32, int32>(Hashes.ToHashedString(func_name), player_cr, param0, param1) || //
            ScriptSys.CallFunc<void, Critter*, any_t, any_t>(Hashes.ToHashedString(func_name), player_cr, param0_str, param1_str) || //
            ScriptSys.CallFunc<void, Critter*, int32, int32, int32>(Hashes.ToHashedString(func_name), player_cr, param0, param1, param2) || //
            ScriptSys.CallFunc<void, Critter*, any_t, any_t, any_t>(Hashes.ToHashedString(func_name), player_cr, param0_str, param1_str, param2_str) || //
            ScriptSys.CallFunc<void>(Hashes.ToHashedString(func_name)) || //
            ScriptSys.CallFunc<void, int32>(Hashes.ToHashedString(func_name), param0) || //
            ScriptSys.CallFunc<void, any_t>(Hashes.ToHashedString(func_name), param0_str) || //
            ScriptSys.CallFunc<void, int32, int32>(Hashes.ToHashedString(func_name), param0, param1) || //
            ScriptSys.CallFunc<void, any_t, any_t>(Hashes.ToHashedString(func_name), param0_str, param1_str) || //
            ScriptSys.CallFunc<void, int32, int32, int32>(Hashes.ToHashedString(func_name), param0, param1, param2) || //
            ScriptSys.CallFunc<void, any_t, any_t, any_t>(Hashes.ToHashedString(func_name), param0_str, param1_str, param2_str)) {
            logcb("Run script success");
        }
        else {
            logcb("Run script failed");
        }
    } break;
    case CMD_REGENMAP: {
        // Check global
        if (!player_cr->GetMapId()) {
            logcb("Only on local map");
            return;
        }

        // Find map
        auto* map = EntityMngr.GetMap(player_cr->GetMapId());

        if (map == nullptr) {
            logcb("Map not found");
            return;
        }

        // Regenerate
        auto hex = player_cr->GetHex();
        auto dir = player_cr->GetDir();
        MapMngr.RegenerateMap(map);
        MapMngr.TransferToMap(player_cr, map, hex, dir, 5);
        logcb("Regenerate map complete");
    } break;
    case CMD_LOG: {
        char flags[16];
        buf.Pop(flags, 16);

        SetLogCallback("LogToClients", nullptr);

        auto it = std::ranges::find(_logClients, player);
        if (flags[0] == '-' && flags[1] == '\0' && it != _logClients.end()) // Detach current
        {
            logcb("Detached");
            _logClients.erase(it);
        }
        else if (flags[0] == '+' && flags[1] == '\0' && it == _logClients.end()) // Attach current
        {
            logcb("Attached");
            _logClients.emplace_back(player);
        }
        else if (flags[0] == '-' && flags[1] == '-' && flags[2] == '\0') // Detach all
        {
            logcb("Detached all");
            _logClients.clear();
        }

        if (!_logClients.empty()) {
            SetLogCallback("LogToClients", [this](string_view str) { LogToClients(str); });
        }
    } break;
    default:
        logcb("Unknown command");
        break;
    }
}

void FOServer::LogToClients(string_view str)
{
    FO_STACK_TRACE_ENTRY();

    if (!str.empty() && str.back() == '\n') {
        _logLines.emplace_back(str, 0, str.length() - 1);
    }
    else {
        _logLines.emplace_back(str);
    }
}

void FOServer::DispatchLogToClients()
{
    FO_STACK_TRACE_ENTRY();

    if (_logLines.empty()) {
        return;
    }

    for (const auto& str : _logLines) {
        for (auto it = _logClients.begin(); it < _logClients.end();) {
            if (auto& player = *it; !player->IsDestroyed()) {
                player->Send_InfoMessage(EngineInfoMessage::ServerLog, str);
                ++it;
            }
            else {
                it = _logClients.erase(it);
            }
        }
    }

    if (_logClients.empty()) {
        SetLogCallback("LogToClients", nullptr);
    }

    _logLines.clear();
}

auto FOServer::CreateCritter(hstring pid, bool for_player) -> Critter*
{
    FO_STACK_TRACE_ENTRY();

    WriteLog(LogType::Info, "Create critter {}", pid);

    const auto* proto = ProtoMngr.GetProtoCritter(pid);
    auto cr = SafeAlloc::MakeRefCounted<Critter>(this, ident_t {}, proto);

    EntityMngr.RegisterCritter(cr.get());

    if (for_player) {
        cr->MarkIsForPlayer();
    }

    MapMngr.AddCritterToMap(cr.get(), nullptr, {}, 0, {});

    if (!cr->IsDestroyed()) {
        EntityMngr.CallInit(cr.get(), true);
    }

    if (cr->IsDestroyed()) {
        throw GenericException("Critter destroyed during init");
    }

    return cr.get();
}

auto FOServer::LoadCritter(ident_t cr_id, bool for_player) -> Critter*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr_id);

    WriteLog(LogType::Info, "Load critter {}", cr_id);

    if (EntityMngr.GetCritter(cr_id) != nullptr) {
        throw GenericException("Critter already in game");
    }

    bool is_error = false;
    Critter* cr = EntityMngr.LoadCritter(cr_id, is_error);

    if (is_error) {
        if (cr != nullptr) {
            cr->MarkAsDestroying();
            UnloadCritterInnerEntities(cr);
            cr->MarkAsDestroyed();
            EntityMngr.UnregisterCritter(cr);
        }

        throw GenericException("Critter data base loading error");
    }

    FO_RUNTIME_ASSERT(cr);

    if (for_player) {
        cr->MarkIsForPlayer();
    }

    MapMngr.AddCritterToMap(cr, nullptr, {}, 0, {});

    if (!cr->IsDestroyed()) {
        OnCritterLoad.Fire(cr);
    }

    if (!cr->IsDestroyed()) {
        EntityMngr.CallInit(cr, false);
    }

    if (cr->IsDestroyed()) {
        throw GenericException("Player critter destroyed during loading");
    }

    return cr;
}

void FOServer::UnloadCritter(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr);
    FO_RUNTIME_ASSERT(!cr->IsDestroyed());

    WriteLog(LogType::Info, "Unload critter {}", cr->GetName());

    if (cr->IsDestroying()) {
        throw GenericException("Critter in destroying state");
    }
    if (cr->GetPlayer() != nullptr) {
        throw GenericException("Player critter not detached from player before unloading");
    }

    cr->MarkAsDestroying();

    OnCritterUnload.Fire(cr);
    FO_RUNTIME_ASSERT(!cr->IsDestroyed());

    cr->Broadcast_Action(CritterAction::Disconnect, 0, nullptr);

    auto* map = EntityMngr.GetMap(cr->GetMapId());
    FO_RUNTIME_ASSERT(!cr->GetMapId() || map);

    MapMngr.RemoveCritterFromMap(cr, map);
    FO_RUNTIME_ASSERT(!cr->IsDestroyed());

    if (cr->GetIsAttached()) {
        cr->DetachFromCritter();
    }

    if (!cr->AttachedCritters.empty()) {
        for (auto& attached_cr : copy_hold_ref(cr->AttachedCritters)) {
            attached_cr->DetachFromCritter();
        }
    }

    UnloadCritterInnerEntities(cr);
    cr->MarkAsDestroyed();
    EntityMngr.UnregisterCritter(cr);
}

void FOServer::UnloadCritterInnerEntities(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    function<void(Entity*)> unload_inner_entities;

    unload_inner_entities = [this, &unload_inner_entities](Entity* holder) {
        for (auto& entities : holder->GetInnerEntities() | std::views::values) {
            for (auto& entity : entities) {
                if (entity->HasInnerEntities()) {
                    unload_inner_entities(entity.get());
                }

                auto* custom_entity = dynamic_cast<CustomEntity*>(entity.get());
                FO_RUNTIME_ASSERT(custom_entity);

                custom_entity->MarkAsDestroyed();
                EntityMngr.UnregisterCustomEntity(custom_entity, false);
            }
        }

        holder->ClearInnerEntities();
    };

    if (cr->HasInnerEntities()) {
        unload_inner_entities(cr);
    }

    function<void(Item*)> unload_item;

    unload_item = [&](Item* item) {
        if (item->HasInnerItems()) {
            auto& inner_items = item->GetRawInnerItems();

            for (auto* inner_item : inner_items) {
                unload_item(inner_item);
            }

            inner_items.clear();
        }

        if (item->HasInnerEntities()) {
            unload_inner_entities(item);
        }

        item->MarkAsDestroyed();
        EntityMngr.UnregisterItem(item, false);

        cr->RemoveItem(item);
    };

    for (auto* item : copy_hold_ref(cr->GetInvItems())) {
        unload_item(item);
    }
}

void FOServer::SwitchPlayerCritter(Player* player, Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(player);
    FO_RUNTIME_ASSERT(!player->IsDestroyed());
    FO_RUNTIME_ASSERT(cr);
    FO_RUNTIME_ASSERT(!cr->IsDestroyed());

    WriteLog(LogType::Info, "Switch player {} to critter {}", player->GetName(), cr->GetName());

    Critter* prev_cr = player->GetControlledCritter();

    if (prev_cr == cr) {
        throw GenericException("Player critter already selected");
    }
    if (!cr->GetControlledByPlayer()) {
        throw GenericException("Critter can't be controlled by player");
    }

    if (prev_cr != nullptr) {
        prev_cr->DetachPlayer();
    }

    cr->AttachPlayer(player);

    SendCritterInitialInfo(cr, prev_cr);

    OnPlayerCritterSwitched.Fire(player, cr, prev_cr);
}

void FOServer::DestroyUnloadedCritter(ident_t cr_id)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr_id);

    WriteLog(LogType::Info, "Destroy unloaded critter {}", cr_id);

    if (EntityMngr.GetCritter(cr_id) != nullptr) {
        throw GenericException("Critter must be unloaded before destroying");
    }

    DbStorage.Delete(Hashes.ToHashedString("Critters"), cr_id);
}

void FOServer::SendCritterInitialInfo(Critter* cr, Critter* prev_cr)
{
    cr->Send_TimeSync();

    if (cr->ViewMapId) {
        auto* map = EntityMngr.GetMap(cr->ViewMapId);
        cr->ViewMapId = ident_t {};

        if (map != nullptr) {
            MapMngr.ViewMap(cr, map, cr->ViewMapLook, cr->ViewMapHex, cr->ViewMapDir);
            cr->Send_ViewMap();
            return;
        }
    }

    const auto* map = EntityMngr.GetMap(cr->GetMapId());
    const bool same_map = prev_cr != nullptr && prev_cr->GetMapId() == cr->GetMapId();

    if (same_map) {
        // Remove entities diff
        cr->Send_RemoveCritter(prev_cr);

        for (const auto* visible_cr : prev_cr->GetCritters(CritterSeeType::WhoISee, CritterFindType::Any)) {
            if (!cr->IsSeeCritter(visible_cr->GetId())) {
                cr->Send_RemoveCritter(visible_cr);
            }
        }

        for (const auto item_id : prev_cr->GetVisibleItems()) {
            if (!cr->IsSeeItem(item_id)) {
                if (const auto* item = EntityMngr.GetItem(item_id); item != nullptr) {
                    cr->Send_RemoveItemFromMap(item);
                }
            }
        }
    }
    else {
        cr->Send_LoadMap(map);
    }

    cr->Broadcast_Action(CritterAction::Connect, 0, nullptr);

    cr->Send_AddCritter(cr);

    if (map == nullptr) {
        for (const auto& group_cr : cr->GetGlobalMapGroup()) {
            if (group_cr != cr) {
                cr->Send_AddCritter(group_cr.get());
            }
        }
    }
    else {
        // Send current critters
        for (const auto* visible_cr : cr->GetCritters(CritterSeeType::WhoISee, CritterFindType::Any)) {
            if (same_map && prev_cr->IsSeeCritter(visible_cr->GetId())) {
                continue;
            }

            cr->Send_AddCritter(visible_cr);
        }

        // Send current items on map
        for (const auto item_id : cr->GetVisibleItems()) {
            if (same_map && prev_cr->IsSeeItem(item_id)) {
                continue;
            }

            if (const auto* item = EntityMngr.GetItem(item_id); item != nullptr) {
                cr->Send_AddItemOnMap(item);
            }
        }
    }

    OnCritterSendInitialInfo.Fire(cr);

    cr->Send_PlaceToGameComplete();
}

void FOServer::VerifyTrigger(Map* map, Critter* cr, mpos from_hex, mpos to_hex, uint8 dir)
{
    FO_STACK_TRACE_ENTRY();

    if (map->IsTriggerStaticItemOnHex(from_hex)) {
        for (auto& item : map->GetTriggerStaticItemsOnHex(from_hex)) {
            if (item->TriggerScriptFunc) {
                if (!item->TriggerScriptFunc(cr, item.get(), false, dir)) {
                    // Nop
                }

                if (cr->IsDestroyed()) {
                    return;
                }
            }

            OnStaticItemWalk.Fire(item.get(), cr, false, dir);

            if (cr->IsDestroyed()) {
                return;
            }
        }
    }

    if (map->IsTriggerStaticItemOnHex(to_hex)) {
        for (auto& item : map->GetTriggerStaticItemsOnHex(to_hex)) {
            if (item->TriggerScriptFunc) {
                if (!item->TriggerScriptFunc(cr, item.get(), true, dir)) {
                    // Nop
                }

                if (cr->IsDestroyed()) {
                    return;
                }
            }

            OnStaticItemWalk.Fire(item.get(), cr, true, dir);

            if (cr->IsDestroyed()) {
                return;
            }
        }
    }

    if (map->IsTriggerItemOnHex(from_hex)) {
        for (auto* item : map->GetTriggerItemsOnHex(from_hex)) {
            if (item->IsDestroyed()) {
                continue;
            }

            item->OnCritterWalk.Fire(cr, false, dir);

            if (cr->IsDestroyed()) {
                return;
            }
        }
    }

    if (map->IsTriggerItemOnHex(to_hex)) {
        for (auto* item : map->GetTriggerItemsOnHex(to_hex)) {
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

void FOServer::Process_Handshake(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    auto in_buf = connection->ReadBuf();

    // Net protocol
    const auto comp_version = in_buf->Read<uint32>();
    const auto outdated = comp_version != 0 && comp_version != numeric_cast<uint32>(FO_COMPATIBILITY_VERSION);

    // Begin data encrypting
    const auto in_encrypt_key = in_buf->Read<uint32>();
    FO_RUNTIME_ASSERT(in_encrypt_key != 0);
    in_buf->SetEncryptKey(in_encrypt_key);

    in_buf.Unlock();

    const auto out_encrypt_key = NetBuffer::GenerateEncryptKey();

    {
        auto out_buf = connection->WriteMsg(NetMessage::HandshakeAnswer);

        out_buf->Write(outdated);
        out_buf->Write(out_encrypt_key);
    }

    {
        auto out_buf = connection->WriteBuf();

        out_buf->SetEncryptKey(out_encrypt_key);
    }

    if (!outdated) {
        vector<const uint8*>* global_vars_data = nullptr;
        vector<uint32>* global_vars_data_sizes = nullptr;
        StoreData(false, &global_vars_data, &global_vars_data_sizes);

        auto out_buf = connection->WriteMsg(NetMessage::InitData);

        out_buf->Write(numeric_cast<uint32>(_updateFilesDesc.size()));
        out_buf->Push(_updateFilesDesc);
        out_buf->WritePropsData(global_vars_data, global_vars_data_sizes);
        out_buf->Write(GameTime.GetSynchronizedTime());
    }

    connection->WasHandshake = true;
}

void FOServer::Process_Ping(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto in_buf = connection->ReadBuf();

    const auto answer = in_buf->Read<bool>();

    in_buf.Unlock();

    if (answer) {
        connection->PingOk = true;
        connection->PingNextTime = GameTime.GetFrameTime() + std::chrono::milliseconds {Settings.ClientPingTime};
    }
    else {
        auto out_buf = connection->WriteMsg(NetMessage::Ping);

        out_buf->Write(true);
    }
}

void FOServer::Process_UpdateFile(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    auto in_buf = connection->ReadBuf();

    const auto file_index = in_buf->Read<uint32>();

    in_buf.Unlock();

    if (file_index >= _updateFilesData.size()) {
        WriteLog("Wrong file index {}, from host '{}'", file_index, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    connection->UpdateFileIndex = numeric_cast<int32>(file_index);
    connection->UpdateFilePortion = 0;

    Process_UpdateFileData(connection);
}

void FOServer::Process_UpdateFileData(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (connection->UpdateFileIndex == -1) {
        WriteLog("Wrong update call, client host '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    const auto& update_file_data = _updateFilesData[connection->UpdateFileIndex];
    auto update_portion = Settings.UpdateFileSendSize;
    const auto offset = connection->UpdateFilePortion * update_portion;

    if (offset + update_portion < numeric_cast<int32>(update_file_data.size())) {
        connection->UpdateFilePortion++;
    }
    else {
        update_portion = numeric_cast<int32>(update_file_data.size()) % update_portion;
        connection->UpdateFileIndex = -1;
    }

    auto out_buf = connection->WriteMsg(NetMessage::UpdateFileData);

    out_buf->Write(update_portion);
    out_buf->Push(&update_file_data[offset], update_portion);
}

void FOServer::Process_Register(Player* unlogined_player)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Register player");

    auto* connection = unlogined_player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto name = in_buf->Read<string>();
    const auto password = in_buf->Read<string>();

    in_buf.Unlock();

    // Check data
    if (!strex(name).is_valid_utf8() || name.find('*') != string::npos) {
        unlogined_player->Send_InfoMessage(EngineInfoMessage::NetLoginPassWrong);
        connection->GracefulDisconnect();
        return;
    }

    // Check name length
    const auto name_len_utf8 = numeric_cast<int32>(strex(name).length_utf8());

    if (name_len_utf8 < Settings.MinNameLength || name_len_utf8 > Settings.MaxNameLength) {
        unlogined_player->Send_InfoMessage(EngineInfoMessage::NetLoginPassWrong);
        connection->GracefulDisconnect();
        return;
    }

    // Check for exist
    const auto player_id = MakePlayerId(name);

    if (DbStorage.Valid(PlayersCollectionName, player_id)) {
        unlogined_player->Send_InfoMessage(EngineInfoMessage::NetPlayerAlready);
        connection->GracefulDisconnect();
        return;
    }

    // Check brute force registration
    if (Settings.RegistrationTimeout != 0) {
        const auto host = connection->GetHost();
        const auto reg_tick = std::chrono::milliseconds {Settings.RegistrationTimeout * 1000};

        if (const auto it = _registrationHistory.find(host); it != _registrationHistory.end()) {
            auto& last_reg = it->second;
            const auto tick = GameTime.GetFrameTime();

            if (tick - last_reg < reg_tick) {
                unlogined_player->Send_InfoMessage(EngineInfoMessage::NetRegistrationIpWait);
                connection->GracefulDisconnect();
                return;
            }

            last_reg = tick;
        }
        else {
            _registrationHistory.emplace(host, GameTime.GetFrameTime());
        }
    }

    const auto allow = OnPlayerRegistration.Fire(unlogined_player, name);

    if (!allow) {
        connection->GracefulDisconnect();
        return;
    }

    // Register
    auto reg_host = AnyData::Array();
    reg_host.EmplaceBack(string(connection->GetHost()));
    auto reg_port = AnyData::Array();
    reg_port.EmplaceBack(numeric_cast<int64>(connection->GetPort()));

    AnyData::Document player_data;
    player_data.Emplace("_Name", string(name));
    player_data.Emplace("Password", string(password));
    player_data.Emplace("ConnectionHost", std::move(reg_host));
    player_data.Emplace("ConnectionPort", std::move(reg_port));

    DbStorage.Insert(PlayersCollectionName, player_id, player_data);

    WriteLog("Registered player {} with id {}", name, player_id);

    // Notify
    unlogined_player->Send_InfoMessage(EngineInfoMessage::NetRegSuccess);

    connection->WriteMsg(NetMessage::RegisterSuccess);
}

void FOServer::Process_Login(Player* unlogined_player)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Login player");

    auto* connection = unlogined_player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto name = in_buf->Read<string>();
    const auto password = in_buf->Read<string>();

    in_buf.Unlock();

    // If only cache checking than disconnect
    if (name.empty()) {
        connection->GracefulDisconnect();
        return;
    }

    // Check for null in login/password
    if (name.find('\0') != string::npos || password.find('\0') != string::npos) {
        unlogined_player->Send_InfoMessage(EngineInfoMessage::NetWrongLogin);
        connection->GracefulDisconnect();
        return;
    }

    // Check valid symbols in name
    if (!strex(name).is_valid_utf8() || name.find('*') != string::npos) {
        unlogined_player->Send_InfoMessage(EngineInfoMessage::NetWrongData);
        connection->GracefulDisconnect();
        return;
    }

    // Check for name length
    const auto name_len_utf8 = numeric_cast<int32>(strex(name).length_utf8());

    if (name_len_utf8 < Settings.MinNameLength || name_len_utf8 > Settings.MaxNameLength) {
        unlogined_player->Send_InfoMessage(EngineInfoMessage::NetWrongLogin);
        connection->GracefulDisconnect();
        return;
    }

    // Check password
    const auto player_id = MakePlayerId(name);
    const auto player_doc = DbStorage.Get(PlayersCollectionName, player_id);

    if (!player_doc.Contains("Password") || player_doc["Password"].Type() != AnyData::ValueType::String || player_doc["Password"].AsString().length() != password.length() || player_doc["Password"].AsString() != password) {
        unlogined_player->Send_InfoMessage(EngineInfoMessage::NetLoginPassWrong);
        connection->GracefulDisconnect();
        return;
    }

    // Request script
    {
        const auto allow = OnPlayerLogin.Fire(unlogined_player, name, player_id);

        if (!allow) {
            connection->GracefulDisconnect();
            return;
        }
    }

    // Switch from unlogined to logined
    Player* player = EntityMngr.GetPlayer(player_id);
    bool player_reconnected;

    if (player == nullptr) {
        player_reconnected = false;

        if (!PropertiesSerializator::LoadFromDocument(&unlogined_player->GetPropertiesForEdit(), player_doc, Hashes, *this)) {
            unlogined_player->Send_InfoMessage(EngineInfoMessage::NetWrongData);
            connection->GracefulDisconnect();
            return;
        }

        const auto it = std::ranges::find(_unloginedPlayers, unlogined_player);
        FO_RUNTIME_ASSERT(it != _unloginedPlayers.end());
        _unloginedPlayers.erase(it);

        player = unlogined_player;
        EntityMngr.RegisterPlayer(player, player_id);
        player->SetName(name);

        WriteLog("Connected player {}", name);

        OnPlayerInit.Fire(player);
    }
    else {
        player_reconnected = true;

        // Kick previous
        const auto it = std::ranges::find(_unloginedPlayers, unlogined_player);
        FO_RUNTIME_ASSERT(it != _unloginedPlayers.end());
        _unloginedPlayers.erase(it);

        player->SwapConnection(unlogined_player);

        unlogined_player->GetConnection()->HardDisconnect();
        unlogined_player->MarkAsDestroyed();

        WriteLog("Reconnected player {}", name);
    }

    // Connection info
    {
        const auto host = connection->GetHost();
        const auto port = connection->GetPort();

        auto conn_host = player->GetConnectionHost();
        auto conn_port = player->GetConnectionPort();
        FO_RUNTIME_ASSERT(conn_host.size() == conn_port.size());

        bool ip_found = false;

        for (size_t i = 0; i < conn_host.size(); i++) {
            if (conn_host[i] == host) {
                if (i < conn_host.size() - 1) {
                    conn_host.erase(conn_host.begin() + numeric_cast<ptrdiff_t>(i));
                    conn_host.emplace_back(host);
                    player->SetConnectionHost(conn_host);
                    conn_port.erase(conn_port.begin() + numeric_cast<ptrdiff_t>(i));
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
            conn_host.emplace_back(host);
            player->SetConnectionHost(conn_host);
            conn_port.push_back(port);
            player->SetConnectionPort(conn_port);
        }
    }

    // Login success
    player->Send_LoginSuccess();

    // Attach critter
    if (!player_reconnected) {
        const auto cr_id = player->GetLastControlledCritterId();

        // Try find in game
        if (cr_id) {
            auto* cr = EntityMngr.GetCritter(cr_id);

            if (cr != nullptr) {
                FO_RUNTIME_ASSERT(cr->GetControlledByPlayer());

                // Already attached to another player
                if (cr->GetPlayer() != nullptr) {
                    player->Send_InfoMessage(EngineInfoMessage::NetPlayerInGame);
                    connection->GracefulDisconnect();
                    return;
                }

                cr->AttachPlayer(player);

                SendCritterInitialInfo(cr, nullptr);

                WriteLog(LogType::Info, "Critter for player found in game");
            }
        }
    }
    else {
        auto* cr = player->GetControlledCritter();

        if (cr != nullptr) {
            SendCritterInitialInfo(cr, nullptr);
        }
    }

    if (OnPlayerEnter.Fire(player)) {
        player->Send_InfoMessage(EngineInfoMessage::NetLoginOk);
    }
    else {
        player->Send_InfoMessage(EngineInfoMessage::NetWrongData);
        connection->GracefulDisconnect();
    }
}

void FOServer::Process_Move(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto map_id = in_buf->Read<ident_t>();
    const auto cr_id = in_buf->Read<ident_t>();
    const auto speed = in_buf->Read<uint16>();
    const auto start_hex = in_buf->Read<mpos>();

    const auto steps_count = in_buf->Read<uint16>();
    vector<uint8> steps;
    steps.resize(steps_count);

    for (uint16 i = 0; i < steps_count; i++) {
        steps[i] = in_buf->Read<uint8>();
    }

    const auto control_steps_count = in_buf->Read<uint16>();
    vector<uint16> control_steps;
    control_steps.resize(control_steps_count);

    for (uint16 i = 0; i < control_steps_count; i++) {
        control_steps[i] = in_buf->Read<uint16>();
    }

    const auto end_hex_offset = in_buf->Read<ipos16>();

    in_buf.Unlock();

    auto* map = EntityMngr.GetMap(map_id);

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
        player->Send_Moving(cr);
        return;
    }

    if (cr->GetIsAttached()) {
        BreakIntoDebugger();
        player->Send_Attachments(cr);
        player->Send_Moving(cr);
        return;
    }

    // Validate path
    // Todo: validate player moving path
    /*auto next_start_hx = start_hx;
    auto next_start_hy = start_hy;
    uint16 control_step_begin = 0;

    for (size_t i = 0; i < control_steps.size(); i++) {
        auto hx = next_start_hx;
        auto hy = next_start_hy;

        FO_RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
        FO_RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
        for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
            const auto move_ok = Geometry.MoveHexByDir(hx, hy, cr->Moving.Steps[j], map->GetWidth(), map->GetHeight());
            if (!move_ok || !map->IsHexMovable(hx, hy)) {
            }
        }

        control_step_begin = cr->Moving.ControlSteps[i];
        next_start_hx = hx;
        next_start_hy = hy;
    }*/

    int32 corrected_speed = speed;

    if (!OnPlayerMoveCritter.Fire(player, cr, corrected_speed)) {
        BreakIntoDebugger();
        player->Send_Moving(cr);
        return;
    }

    // Fix async errors
    const auto cr_hex = cr->GetHex();

    if (cr_hex != start_hex) {
        FindPathInput find_input;
        find_input.TargetMap = map;
        find_input.FromCritter = cr;
        find_input.FromHex = cr_hex;
        find_input.ToHex = start_hex;
        find_input.Multihex = cr->GetMultihex();

        const auto find_result = MapMngr.FindPath(find_input);

        if (find_result.Result != FindPathOutput::ResultType::Ok) {
            BreakIntoDebugger();
            player->Send_Moving(cr);
            return;
        }

        // Insert part of path to beginning of whole path
        for (auto& control_step : control_steps) {
            control_step += numeric_cast<uint16>(find_result.Steps.size());
        }

        control_steps.insert(control_steps.begin(), find_result.ControlSteps.begin(), find_result.ControlSteps.end());
        steps.insert(steps.begin(), find_result.Steps.begin(), find_result.Steps.end());
    }

    if (end_hex_offset.x < -Settings.MapHexWidth / 2 || end_hex_offset.x > Settings.MapHexWidth / 2) {
        BreakIntoDebugger();
    }
    if (end_hex_offset.y < -Settings.MapHexHeight / 2 || end_hex_offset.y > Settings.MapHexHeight / 2) {
        BreakIntoDebugger();
    }

    const auto clamped_end_hex_ox = std::clamp(end_hex_offset.x, numeric_cast<int16>(-Settings.MapHexWidth / 2), numeric_cast<int16>(Settings.MapHexWidth / 2));
    const auto clamped_end_hex_oy = std::clamp(end_hex_offset.y, numeric_cast<int16>(-Settings.MapHexHeight / 2), numeric_cast<int16>(Settings.MapHexHeight / 2));

    StartCritterMoving(cr, numeric_cast<uint16>(corrected_speed), steps, control_steps, {clamped_end_hex_ox, clamped_end_hex_oy}, player);

    if (corrected_speed != numeric_cast<int32>(speed)) {
        player->Send_MovingSpeed(cr);
    }
}

void FOServer::Process_StopMove(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto map_id = in_buf->Read<ident_t>();
    const auto cr_id = in_buf->Read<ident_t>();

    // Todo: validate stop position and place critter in it
    [[maybe_unused]] const auto start_hex = in_buf->Read<mpos>();
    [[maybe_unused]] const auto hex_offset = in_buf->Read<ipos16>();
    [[maybe_unused]] const auto dir_angle = in_buf->Read<int16>();

    in_buf.Unlock();

    auto* map = EntityMngr.GetMap(map_id);

    if (map == nullptr) {
        BreakIntoDebugger();
        return;
    }

    Critter* cr = map->GetCritter(cr_id);

    if (cr == nullptr) {
        BreakIntoDebugger();
        return;
    }

    if (cr->GetIsAttached()) {
        BreakIntoDebugger();
        player->Send_Attachments(cr);
        player->Send_Moving(cr);
        return;
    }

    int32 zero_speed = 0;

    if (!OnPlayerMoveCritter.Fire(player, cr, zero_speed)) {
        BreakIntoDebugger();
        player->Send_Moving(cr);
        return;
    }

    cr->ClearMove();
    cr->SendAndBroadcast(player, [cr](Critter* cr2) { cr2->Send_Moving(cr); });
}

void FOServer::Process_Dir(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto map_id = in_buf->Read<ident_t>();
    const auto cr_id = in_buf->Read<ident_t>();
    const auto dir_angle = in_buf->Read<int16>();

    in_buf.Unlock();

    auto* map = EntityMngr.GetMap(map_id);

    if (map == nullptr) {
        return;
    }

    auto* cr = map->GetCritter(cr_id);

    if (cr == nullptr) {
        return;
    }

    int16 checked_dir_angle = dir_angle;

    if (!OnPlayerDirCritter.Fire(player, cr, checked_dir_angle)) {
        BreakIntoDebugger();
        player->Send_Dir(cr);
        return;
    }

    cr->ChangeDirAngle(checked_dir_angle);
    cr->SendAndBroadcast(player, [cr](Critter* cr2) { cr2->Send_Dir(cr); });
}

void FOServer::Process_Property(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    Critter* cr = player->GetControlledCritter();

    const auto data_size = in_buf->Read<uint32>();

    // Todo: control max size explicitly, add option to property registration
    if (data_size > 0xFFFF) {
        // For now 64Kb for all
        throw GenericException("Property len > 0xFFFF");
    }

    const auto type = in_buf->Read<NetProperty>();

    ident_t cr_id;
    ident_t item_id;

    switch (type) {
    case NetProperty::CritterItem:
        cr_id = in_buf->Read<ident_t>();
        item_id = in_buf->Read<ident_t>();
        break;
    case NetProperty::Critter:
        cr_id = in_buf->Read<ident_t>();
        break;
    case NetProperty::MapItem:
        item_id = in_buf->Read<ident_t>();
        break;
    case NetProperty::ChosenItem:
        item_id = in_buf->Read<ident_t>();
        break;
    default:
        break;
    }

    const auto property_index = in_buf->Read<uint16>();

    PropertyRawData prop_data;
    in_buf->Pop(prop_data.Alloc(data_size), data_size);

    in_buf.Unlock();

    bool is_public = false;
    const Property* prop = nullptr;
    Entity* entity = nullptr;

    switch (type) {
    case NetProperty::Game:
        is_public = true;
        prop = GetPropertyRegistrator(GameProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            entity = this;
        }
        break;
    case NetProperty::Player:
        prop = GetPropertyRegistrator(PlayerProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            entity = player;
        }
        break;
    case NetProperty::Critter:
        is_public = true;
        prop = GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            entity = EntityMngr.GetCritter(cr_id);
        }
        break;
    case NetProperty::Chosen:
        prop = GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            entity = cr;
        }
        break;
    case NetProperty::MapItem:
        is_public = true;
        prop = GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            auto* item = EntityMngr.GetItem(item_id);
            entity = item != nullptr && !item->GetHidden() ? item : nullptr;
        }
        break;
    case NetProperty::CritterItem:
        is_public = true;
        prop = GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            auto* cr_ = EntityMngr.GetCritter(cr_id);
            if (cr_ != nullptr) {
                auto* item = cr_->GetInvItem(item_id);
                entity = item != nullptr && !item->GetHidden() ? item : nullptr;
            }
        }
        break;
    case NetProperty::ChosenItem:
        prop = GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            auto* item = cr->GetInvItem(item_id);
            entity = item != nullptr && !item->GetHidden() ? item : nullptr;
        }
        break;
    case NetProperty::Map:
        is_public = true;
        prop = GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            entity = EntityMngr.GetMap(cr->GetMapId());
        }
        break;
    case NetProperty::Location:
        is_public = true;
        prop = GetPropertyRegistrator(LocationProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);
        if (prop != nullptr) {
            auto* map = EntityMngr.GetMap(cr->GetMapId());
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

    if (is_public && !prop->IsPublicSync()) {
        BreakIntoDebugger();
        return;
    }
    if (!is_public && !prop->IsSynced()) {
        BreakIntoDebugger();
        return;
    }
    if (!prop->IsModifiableByClient() && !prop->IsModifiableByAnyClient()) {
        BreakIntoDebugger();
        return;
    }
    if (is_public && !prop->IsModifiableByAnyClient()) {
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
        player->SetIgnoreSendEntityProperty(entity, prop);
        auto revert_send_ignore = ScopeCallback([player]() noexcept { player->SetIgnoreSendEntityProperty(nullptr, nullptr); });

        // Todo: verify property data from client
        entity->SetValueFromData(prop, prop_data);
    }
}

void FOServer::OnSaveEntityValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

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

    auto value = PropertiesSerializator::SavePropertyToValue(&entity->GetProperties(), prop, Hashes, *this);

    hstring collection_name;

    if (server_entity != nullptr) {
        collection_name = server_entity->GetTypeNamePlural();
    }
    else {
        collection_name = entity->GetTypeName();
    }

    DbStorage.Update(collection_name, entry_id, prop->GetName(), value);

    if (prop->IsHistorical()) {
        const auto history_id_num = GetHistoryRecordsId().underlying_value() + 1;
        const auto history_id = ident_t {history_id_num};

        SetHistoryRecordsId(history_id);

        const auto time = GameTime.GetSynchronizedTime();

        AnyData::Document doc;
        doc.Emplace("Time", numeric_cast<int64>(time.milliseconds()));
        doc.Emplace("EntityType", string(entity->GetTypeName()));
        doc.Emplace("EntityId", numeric_cast<int64>(entry_id.underlying_value()));
        doc.Emplace("Property", prop->GetName());
        doc.Emplace("Value", std::move(value));

        DbStorage.Insert(HistoryCollectionName, history_id, doc);
    }
}

void FOServer::OnSendGlobalValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(entity);

    if (prop->IsPublicSync()) {
        for (auto& player : EntityMngr.GetPlayers() | std::views::values) {
            player->Send_Property(NetProperty::Game, prop, this);
        }
    }
}

void FOServer::OnSendPlayerValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* player = dynamic_cast<Player*>(entity);
    FO_RUNTIME_ASSERT(player);

    player->Send_Property(NetProperty::Player, prop, player);
}

void FOServer::OnSendCritterValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* cr = dynamic_cast<Critter*>(entity);
    FO_RUNTIME_ASSERT(cr);

    if (prop->IsPublicSync() || prop->IsOwnerSync()) {
        cr->Send_Property(NetProperty::Chosen, prop, cr);
    }
    if (prop->IsPublicSync()) {
        cr->Broadcast_Property(NetProperty::Critter, prop, cr);
    }
}

void FOServer::OnSendItemValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    auto* item = dynamic_cast<Item*>(entity);
    FO_RUNTIME_ASSERT(item);

    if (!item->GetStatic() && item->GetId()) {
        switch (item->GetOwnership()) {
        case ItemOwnership::Nowhere: {
        } break;
        case ItemOwnership::CritterInventory: {
            if (prop->IsPublicSync() || prop->IsOwnerSync()) {
                auto* cr = EntityMngr.GetCritter(item->GetCritterId());

                if (cr != nullptr) {
                    if (item->CanSendItem(false)) {
                        cr->Send_Property(NetProperty::ChosenItem, prop, item);
                    }

                    if (prop->IsPublicSync()) {
                        if (item->CanSendItem(true)) {
                            cr->Broadcast_Property(NetProperty::CritterItem, prop, item);
                        }
                    }
                }
            }
        } break;
        case ItemOwnership::MapHex: {
            if (prop->IsPublicSync()) {
                auto* map = EntityMngr.GetMap(item->GetMapId());

                if (map != nullptr) {
                    if (item->CanSendItem(true)) {
                        map->SendProperty(NetProperty::MapItem, prop, item);
                    }
                }
            }
        } break;
        case ItemOwnership::ItemContainer: {
            // Todo: add container properties changing notifications
            // Item* cont = EntityMngr.GetItem( item->GetContainerId() );
        } break;
        }
    }
}

void FOServer::OnSendMapValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (prop->IsPublicSync()) {
        auto* map = dynamic_cast<Map*>(entity);
        FO_RUNTIME_ASSERT(map);

        map->SendProperty(NetProperty::Map, prop, map);
    }
}

void FOServer::OnSendLocationValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (prop->IsPublicSync()) {
        auto* loc = dynamic_cast<Location*>(entity);
        FO_RUNTIME_ASSERT(loc);

        for (auto& map : loc->GetMaps()) {
            map->SendProperty(NetProperty::Location, prop, loc);
        }
    }
}

void FOServer::OnSendCustomEntityValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    auto* custom_entity = dynamic_cast<CustomEntity*>(entity);
    FO_RUNTIME_ASSERT(custom_entity);

    EntityMngr.ForEachCustomEntityView(custom_entity, [&](Player* player, bool owner) {
        if (owner || prop->IsPublicSync()) {
            player->Send_Property(NetProperty::CustomEntity, prop, custom_entity);
        }
    });
}

void FOServer::OnSetCritterLook(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    // LookDistance, InSneakMode, SneakCoefficient
    auto* cr = dynamic_cast<Critter*>(entity);
    FO_RUNTIME_ASSERT(cr);

    MapMngr.ProcessVisibleCritters(cr);

    if (prop == cr->GetPropertyLookDistance()) {
        MapMngr.ProcessVisibleItems(cr);
    }
}

void FOServer::OnSetItemCount(Entity* entity, const Property* prop, const void* new_value)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
    ignore_unused(prop);

    const auto* item = dynamic_cast<Item*>(entity);
    const auto new_count = *static_cast<const uint32*>(new_value);
    FO_RUNTIME_ASSERT(item);

    if (!item->GetStackable() && new_count != 1) {
        throw GenericException("Trying to change count of not stackable item");
    }
    else if (new_count <= 0) {
        throw GenericException("Item count can't be zero or negative", new_count);
    }
}

void FOServer::OnSetItemChangeView(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    // Hidden, AlwaysView, IsTrap, TrapValue
    auto* item = dynamic_cast<Item*>(entity);
    FO_RUNTIME_ASSERT(item);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = EntityMngr.GetMap(item->GetMapId());

        if (map != nullptr) {
            map->ChangeViewItem(item);

            if (prop == item->GetPropertyIsTrap()) {
                map->RecacheHexFlags(item->GetHex());
            }
        }
    }
    else if (item->GetOwnership() == ItemOwnership::CritterInventory) {
        auto* cr = EntityMngr.GetCritter(item->GetCritterId());

        if (cr != nullptr) {
            if (item->GetHidden()) {
                cr->Send_ChosenRemoveItem(item);
            }
            else {
                cr->Send_ChosenAddItem(item);
            }

            cr->SendAndBroadcast_MoveItem(item, CritterAction::Refresh, CritterItemSlot::Inventory);
        }
    }
}

void FOServer::OnSetItemRecacheHex(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    // NoBlock, ShootThru, IsGag, IsTrigger
    const auto* item = dynamic_cast<Item*>(entity);
    FO_RUNTIME_ASSERT(item);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = EntityMngr.GetMap(item->GetMapId());

        if (map != nullptr) {
            map->RecacheHexFlags(item->GetHex());
        }
    }
}

void FOServer::OnSetItemMultihexLines(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    // BlockLines
    const auto* item = dynamic_cast<Item*>(entity);
    FO_RUNTIME_ASSERT(item);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        const auto* map = EntityMngr.GetMap(item->GetMapId());

        if (map != nullptr) {
            // Todo: make BlockLines changable in runtime
            throw NotImplementedException(FO_LINE_STR);
        }
    }
}

void FOServer::ProcessCritterMoving(Critter* cr)
{
    FO_STACK_TRACE_ENTRY();

    // Moving
    if (cr->IsMoving()) {
        auto* map = EntityMngr.GetMap(cr->GetMapId());

        if (map != nullptr && cr->IsAlive() && !cr->GetIsAttached()) {
            ProcessCritterMovingBySteps(cr, map);

            if (cr->IsDestroyed()) {
                return;
            }
        }
        else {
            cr->ClearMove();
            cr->SendAndBroadcast_Moving();
        }
    }

    if (cr->TargetMoving.State == MovingState::InProgress) {
        if (cr->GetIsAttached()) {
            cr->TargetMoving.State = MovingState::Attached;
        }
        else if (!cr->IsAlive()) {
            cr->TargetMoving.State = MovingState::NotAlive;
        }

        if (cr->TargetMoving.State != MovingState::InProgress && cr->IsMoving()) {
            cr->ClearMove();
            cr->SendAndBroadcast_Moving();
        }
    }

    // Path find
    if (cr->TargetMoving.State == MovingState::InProgress) {
        bool need_find_path = !cr->IsMoving();

        if (!need_find_path && cr->TargetMoving.TargId) {
            const auto* target = cr->GetCritter(cr->TargetMoving.TargId, CritterSeeType::WhoISee);

            if (target != nullptr && !GeometryHelper::CheckDist(target->GetHex(), cr->TargetMoving.TargHex, 0)) {
                need_find_path = true;
            }
        }

        if (need_find_path) {
            mpos hex;
            int32 cut;
            int32 trace_dist;
            Critter* trace_cr;

            if (cr->TargetMoving.TargId) {
                Critter* target = cr->GetCritter(cr->TargetMoving.TargId, CritterSeeType::WhoISee);

                if (target == nullptr) {
                    cr->TargetMoving.State = MovingState::TargetNotFound;
                    return;
                }

                hex = target->GetHex();
                cut = cr->TargetMoving.Cut;
                trace_dist = cr->TargetMoving.TraceDist;
                trace_cr = target;

                cr->TargetMoving.TargHex = hex;
            }
            else {
                hex = cr->TargetMoving.TargHex;
                cut = cr->TargetMoving.Cut;
                trace_dist = 0;
                trace_cr = nullptr;
            }

            FindPathInput find_input;
            find_input.TargetMap = EntityMngr.GetMap(cr->GetMapId());
            find_input.FromCritter = cr;
            find_input.FromHex = cr->GetHex();
            find_input.ToHex = hex;
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

            if (find_path.GagCritterId) {
                cr->TargetMoving.State = MovingState::GagCritter;
                cr->TargetMoving.GagEntityId = find_path.GagCritterId;
                return;
            }

            if (find_path.GagItemId) {
                cr->TargetMoving.State = MovingState::GagItem;
                cr->TargetMoving.GagEntityId = find_path.GagItemId;
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
                case FindPathOutput::ResultType::NoWay:
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
            StartCritterMoving(cr, cr->TargetMoving.Speed, find_path.Steps, find_path.ControlSteps, {0, 0}, nullptr);
        }
    }
}

void FOServer::ProcessCritterMovingBySteps(Critter* cr, Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!cr->Moving.Steps.empty());
    FO_RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
    FO_RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
    FO_RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

    const auto validate_moving = [cr, expected_uid = cr->Moving.Uid, expected_map_id = cr->GetMapId(), map](mpos expected_hex) -> bool {
        const auto validate_moving_inner = [&]() -> bool {
            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return false;
            }
            if (cr->Moving.Uid != expected_uid) {
                return false;
            }
            if (cr->GetMapId() != expected_map_id) {
                return false;
            }
            if (cr->GetHex() != expected_hex) {
                return false;
            }
            return true;
        };

        if (!validate_moving_inner()) {
            if (!cr->IsDestroyed() && cr->Moving.Uid == expected_uid) {
                cr->ClearMove();
                cr->SendAndBroadcast_Moving();
            }

            return false;
        }
        else {
            return true;
        }
    };

    auto normalized_time = (GameTime.GetFrameTime() - cr->Moving.StartTime + cr->Moving.OffsetTime).to_ms<float32>() / cr->Moving.WholeTime;
    normalized_time = std::clamp(normalized_time, 0.0f, 1.0f);

    const auto dist_pos = cr->Moving.WholeDist * normalized_time;
    auto next_start_hex = cr->Moving.StartHex;
    auto cur_dist = 0.0f;

    uint16 control_step_begin = 0;

    for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
        auto hex = next_start_hex;

        FO_RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
        FO_RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
        for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
            const auto move_ok = GeometryHelper::MoveHexByDir(hex, cr->Moving.Steps[j], map->GetSize());
            FO_RUNTIME_ASSERT(move_ok);
        }

        auto&& [ox, oy] = Geometry.GetHexOffset(next_start_hex, hex);

        if (i == 0) {
            ox -= cr->Moving.StartHexOffset.x;
            oy -= cr->Moving.StartHexOffset.y;
        }
        if (i == cr->Moving.ControlSteps.size() - 1) {
            ox += cr->Moving.EndHexOffset.x;
            oy += cr->Moving.EndHexOffset.y;
        }

        const auto proj_oy = numeric_cast<float32>(oy) * Geometry.GetYProj();
        auto dist = std::sqrt(numeric_cast<float32>(ox * ox) + proj_oy * proj_oy);
        dist = std::max(dist, 0.0001f);

        if ((normalized_time < 1.0f && dist_pos >= cur_dist && dist_pos <= cur_dist + dist) || (normalized_time == 1.0f && i == cr->Moving.ControlSteps.size() - 1)) {
            float32 normalized_dist = (dist_pos - cur_dist) / dist;
            normalized_dist = std::clamp(normalized_dist, 0.0f, 1.0f);
            if (normalized_time == 1.0f) {
                normalized_dist = 1.0f;
            }

            // Evaluate current hex
            const auto step_index = control_step_begin + iround<int32>(normalized_dist * numeric_cast<float32>(cr->Moving.ControlSteps[i] - control_step_begin));
            FO_RUNTIME_ASSERT(step_index >= numeric_cast<int32>(control_step_begin));
            FO_RUNTIME_ASSERT(step_index <= numeric_cast<int32>(cr->Moving.ControlSteps[i]));

            auto hex2 = next_start_hex;

            for (int32 j2 = control_step_begin; j2 < step_index; j2++) {
                const auto move_ok = GeometryHelper::MoveHexByDir(hex2, cr->Moving.Steps[j2], map->GetSize());
                FO_RUNTIME_ASSERT(move_ok);
            }

            const auto old_hex = cr->GetHex();
            const uint8 dir = GeometryHelper::GetDir(old_hex, hex2);

            if (old_hex != hex2) {
                const auto multihex = cr->GetMultihex();

                if (map->IsHexesMovable(hex2, multihex, cr)) {
                    map->RemoveCritterFromField(cr);
                    cr->SetHex(hex2);
                    map->AddCritterToField(cr);

                    FO_RUNTIME_ASSERT(!cr->IsDestroyed());

                    VerifyTrigger(map, cr, old_hex, hex2, dir);
                    if (!validate_moving(hex2)) {
                        return;
                    }

                    MapMngr.ProcessVisibleCritters(cr);
                    if (!validate_moving(hex2)) {
                        return;
                    }

                    MapMngr.ProcessVisibleItems(cr);
                    if (!validate_moving(hex2)) {
                        return;
                    }
                }
                else if (map->IsBlockItemOnHex(hex2)) {
                    cr->ClearMove();
                    cr->SendAndBroadcast_Moving();
                    return;
                }
            }

            // Evaluate current position
            const auto cr_hex = cr->GetHex();
            const auto moved = cr_hex != old_hex;

            auto&& [cr_ox, cr_oy] = Geometry.GetHexOffset(next_start_hex, cr_hex);

            if (i == 0) {
                cr_ox -= cr->Moving.StartHexOffset.x;
                cr_oy -= cr->Moving.StartHexOffset.y;
            }

            const auto lerp = [](int32 a, int32 b, float32 t) { return numeric_cast<float32>(a) * (1.0f - t) + numeric_cast<float32>(b) * t; };

            auto mx = lerp(0, ox, normalized_dist);
            auto my = lerp(0, oy, normalized_dist);

            mx -= numeric_cast<float32>(cr_ox);
            my -= numeric_cast<float32>(cr_oy);

            const auto mxi = iround<int16>(mx);
            const auto myi = iround<int16>(my);

            if (moved || cr->GetHexOffset() != ipos16 {mxi, myi}) {
                cr->SetHexOffset({mxi, myi});
            }

            // Evaluate dir angle
            const auto dir_angle = iround<int16>(Geometry.GetLineDirAngle(0, 0, ox, oy));

            cr->SetDirAngle(dir_angle);

            // Move attached critters
            if (!cr->AttachedCritters.empty()) {
                cr->MoveAttachedCritters();

                if (!validate_moving(cr_hex)) {
                    return;
                }
            }

            break;
        }

        FO_RUNTIME_ASSERT(i < cr->Moving.ControlSteps.size() - 1);

        control_step_begin = cr->Moving.ControlSteps[i];
        next_start_hex = hex;
        cur_dist += dist;
    }

    if (normalized_time == 1.0f) {
        const bool incorrect_final_position = cr->GetHex() != cr->Moving.EndHex;

        cr->ClearMove();
        cr->TargetMoving.State = MovingState::Success;

        if (incorrect_final_position) {
            cr->SendAndBroadcast_Moving();
        }
    }
}

void FOServer::StartCritterMoving(Critter* cr, uint16 speed, const vector<uint8>& steps, const vector<uint16>& control_steps, ipos16 end_hex_offset, const Player* initiator)
{
    FO_STACK_TRACE_ENTRY();

    if (cr->GetIsAttached()) {
        cr->DetachFromCritter();
    }

    cr->ClearMove();

    const auto* map = EntityMngr.GetMap(cr->GetMapId());
    FO_RUNTIME_ASSERT(map);

    const auto start_hex = cr->GetHex();

    cr->Moving.Speed = speed;
    cr->Moving.StartTime = GameTime.GetFrameTime();
    cr->Moving.OffsetTime = {};
    cr->Moving.Steps = steps;
    cr->Moving.ControlSteps = control_steps;
    cr->Moving.StartHex = start_hex;
    cr->Moving.StartHexOffset = cr->GetHexOffset();
    cr->Moving.EndHexOffset = end_hex_offset;

    cr->Moving.WholeTime = 0.0f;
    cr->Moving.WholeDist = 0.0f;

    FO_RUNTIME_ASSERT(cr->Moving.Speed > 0);
    const auto base_move_speed = numeric_cast<float32>(cr->Moving.Speed);

    auto next_start_hex = start_hex;
    uint16 control_step_begin = 0;

    for (size_t i = 0; i < cr->Moving.ControlSteps.size(); i++) {
        auto hex = next_start_hex;

        FO_RUNTIME_ASSERT(control_step_begin <= cr->Moving.ControlSteps[i]);
        FO_RUNTIME_ASSERT(cr->Moving.ControlSteps[i] <= cr->Moving.Steps.size());
        for (auto j = control_step_begin; j < cr->Moving.ControlSteps[i]; j++) {
            const auto move_ok = GeometryHelper::MoveHexByDir(hex, cr->Moving.Steps[j], map->GetSize());
            FO_RUNTIME_ASSERT(move_ok);
        }

        auto&& [ox, oy] = Geometry.GetHexOffset(next_start_hex, hex);

        if (i == 0) {
            ox -= cr->Moving.StartHexOffset.x;
            oy -= cr->Moving.StartHexOffset.y;
        }
        if (i == cr->Moving.ControlSteps.size() - 1) {
            ox += cr->Moving.EndHexOffset.x;
            oy += cr->Moving.EndHexOffset.y;
        }

        const auto proj_oy = numeric_cast<float32>(oy) * Geometry.GetYProj();
        const auto dist = std::sqrt(numeric_cast<float32>(ox * ox) + proj_oy * proj_oy);

        cr->Moving.WholeTime += dist / base_move_speed * 1000.0f;
        cr->Moving.WholeDist += dist;

        control_step_begin = cr->Moving.ControlSteps[i];
        next_start_hex = hex;

        cr->Moving.EndHex = hex;
    }

    cr->Moving.WholeTime = std::max(cr->Moving.WholeTime, 0.0001f);
    cr->Moving.WholeDist = std::max(cr->Moving.WholeDist, 0.0001f);

    FO_RUNTIME_ASSERT(!cr->Moving.Steps.empty());
    FO_RUNTIME_ASSERT(!cr->Moving.ControlSteps.empty());
    FO_RUNTIME_ASSERT(cr->Moving.WholeTime > 0.0f);
    FO_RUNTIME_ASSERT(cr->Moving.WholeDist > 0.0f);

    cr->SetMovingSpeed(speed);

    cr->SendAndBroadcast(initiator, [cr](Critter* cr2) { cr2->Send_Moving(cr); });
}

void FOServer::ChangeCritterMovingSpeed(Critter* cr, uint16 speed)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (!cr->IsMoving()) {
        return;
    }
    if (cr->Moving.Speed == speed) {
        return;
    }

    if (speed == 0) {
        cr->ClearMove();
        cr->SendAndBroadcast_Moving();
        return;
    }

    const auto diff = numeric_cast<float32>(speed) / numeric_cast<float32>(cr->Moving.Speed);
    const auto cur_time = GameTime.GetFrameTime();
    const auto elapsed_time = (cur_time - cr->Moving.StartTime + cr->Moving.OffsetTime).to_ms<float32>();

    cr->Moving.WholeTime /= diff;
    cr->Moving.WholeTime = std::max(cr->Moving.WholeTime, 0.0001f);
    cr->Moving.StartTime = cur_time;
    cr->Moving.OffsetTime = std::chrono::milliseconds(iround<int32>(elapsed_time / diff));
    cr->Moving.Speed = speed;

    cr->SetMovingSpeed(speed);

    cr->SendAndBroadcast(nullptr, [cr](Critter* cr2) { cr2->Send_MovingSpeed(cr); });
}

void FOServer::Process_RemoteCall(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto rpc_num = in_buf->Read<uint32>();

    in_buf.Unlock();

    ScriptSys.HandleRemoteCall(rpc_num, player);
}

auto FOServer::CreateItemOnHex(Map* map, mpos hex, hstring pid, int32 count, Properties* props) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    if (count <= 0) {
        throw GenericException("Invalid items cound");
    }

    const auto* proto = ProtoMngr.GetProtoItem(pid);

    const auto add_item = [&]() -> Item* {
        auto* item = ItemMngr.CreateItem(pid, proto->GetStackable() ? count : 1, props);
        map->AddItem(item, hex, nullptr);
        return item;
    };

    auto* item = add_item();

    // Non-stacked items
    if (item != nullptr && !proto->GetStackable() && count > 1) {
        const auto fixed_count = std::min(count, Settings.MaxAddUnstackableItems);

        for (int32 i = 0; i < fixed_count; i++) {
            if (add_item() == nullptr) {
                break;
            }
        }
    }

    return item;
}

auto FOServer::MakePlayerId(string_view player_name) const -> ident_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!player_name.empty());
    const auto hash_value = Hashing::MurmurHash2(reinterpret_cast<const uint8*>(player_name.data()), numeric_cast<uint32>(player_name.length()));
    FO_RUNTIME_ASSERT(hash_value);
    return ident_t {(1u << 31u) | hash_value};
}

FO_END_NAMESPACE();
