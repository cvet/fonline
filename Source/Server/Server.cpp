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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "AngelScriptScripting.h"
#include "AnyData.h"
#include "Application.h"
#include "ClientDataValidation.h"
#include "MetadataRegistration.h"
#include "Movement.h"
#include "NetCommand.h"
#include "PropertiesSerializator.h"

FO_BEGIN_NAMESPACE

extern void ServerInitHook(ServerEngine*);

auto GetServerResources(GlobalSettings& settings) -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    FileSystem resources;
    resources.AddPacksSource(IsPackaged() ? settings.ServerResources : settings.BakeOutput, settings.ServerResourceEntries);
    return resources;
}

ServerEngine::ServerEngine(GlobalSettings& settings, FileSystem&& resources) :
    BaseEngine(settings, std::move(resources), [&] { RegisterServerMetadata(this, &resources); }),
    EntityMngr(this),
    MapMngr(this),
    CrMngr(this),
    ItemMngr(this)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Start server");
    WriteLog("Compatibility version: {}", settings.CompatibilityVersion);

    _starter.SetExceptionHandler([this](const std::exception& ex) FO_DEFERRED {
        ignore_unused(ex);

        _startingError = true;

        // Clear jobs
        return true;
    });

    // Health file
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitHealthFileJob");

        if (Settings.WriteHealthFile) {
            const auto exe_path = Platform::GetExePath();
            const string health_file_name = strex("{}_Health.txt", exe_path ? strvex(exe_path.value()).extract_file_name().erase_file_extension() : string_view(FO_DEV_NAME));

            const auto write_health_file = [health_file_name](string_view text) FO_DEFERRED {
                std::ofstream health_file {std::filesystem::path {fs_make_path(health_file_name)}, std::ios::binary | std::ios::trunc};

                if (!health_file) {
                    return false;
                }

                if (!text.empty()) {
                    health_file.write(text.data(), static_cast<std::streamsize>(text.size()));
                }

                health_file.flush();
                return !!health_file;
            };

            if (write_health_file("Starting...")) {
                _mainWorker.AddJob([this, write_health_file]() FO_DEFERRED {
                    FO_STACK_TRACE_ENTRY_NAMED("HealthFileJob");

                    if (_started && _healthWriter.GetJobsCount() == 0) {
                        _healthWriter.AddJob([health_info = GetHealthInfo(), write_health_file, &settings_ = Settings]() FO_DEFERRED {
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
                WriteLog(LogType::Warning, "Can't write health file '{}'", health_file_name);
            }
        }

        return std::nullopt;
    });

    // Script system
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitScriptSystemJob");

        WriteLog("Initialize script system");

        MapScriptTypes(this);
        MapEngineType<Player>(GetBaseType(Player::ENTITY_TYPE_NAME));
        MapEngineType<Item>(GetBaseType(Item::ENTITY_TYPE_NAME));
        MapEngineType<StaticItem>(GetBaseType("StaticItem"));
        MapEngineType<Critter>(GetBaseType(Critter::ENTITY_TYPE_NAME));
        MapEngineType<Map>(GetBaseType(Map::ENTITY_TYPE_NAME));
        MapEngineType<Location>(GetBaseType(Location::ENTITY_TYPE_NAME));

#if FO_ANGELSCRIPT_SCRIPTING
        InitAngelScriptScripting(this, Settings, Resources);
#endif

        return std::nullopt;
    });

    // Network
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitNetworkingJob");

        WriteLog("Start networking");

        if (auto conn_server = NetworkServer::StartInterthreadServer(Settings, [this](shared_ptr<NetworkServerConnection> net_connection) FO_DEFERRED { OnNewConnection(std::move(net_connection)); })) {
            _connectionServers.emplace_back(std::move(conn_server));
        }

        if (Settings.DisableNetworking) {
            WriteLog("Skip remote networking startup");
            return std::nullopt;
        }

#if FO_HAVE_ASIO
        if (auto conn_server = NetworkServer::StartAsioServer(Settings, [this](shared_ptr<NetworkServerConnection> net_connection) FO_DEFERRED { OnNewConnection(std::move(net_connection)); })) {
            _connectionServers.emplace_back(std::move(conn_server));
        }
#endif
#if FO_HAVE_WEB_SOCKETS
        if (auto conn_server = NetworkServer::StartWebSocketsServer(Settings, [this](shared_ptr<NetworkServerConnection> net_connection) FO_DEFERRED { OnNewConnection(std::move(net_connection)); })) {
            _connectionServers.emplace_back(std::move(conn_server));
        }
#endif

        return std::nullopt;
    });

    // Data base
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitStorageJob");

        DataBaseCollectionSchemas collection_schemas;
        collection_schemas.reserve(3 + GetEntityTypes().size() + Settings.CustomCollections.size());

        unordered_map<hstring, DataBaseKeyType> registered_collection_types {};
        registered_collection_types.reserve(2 + GetEntityTypes().size() + Settings.CustomCollections.size());

        const auto register_collection = [&collection_schemas, &registered_collection_types](hstring collection_name, DataBaseKeyType key_type) {
            FO_RUNTIME_ASSERT(!collection_name.as_str().empty());

            if (registered_collection_types.contains(collection_name)) {
                throw DataBaseException("Duplicate database collection name", collection_name.as_str());
            }

            registered_collection_types.emplace(collection_name, key_type);
            collection_schemas.emplace_back(collection_name, key_type);
            return true;
        };

        const auto register_custom_collection = [&](string_view entry) {
            const auto separator = entry.find(':');

            if (separator == string_view::npos || separator == 0 || separator + 1 >= entry.size() || entry.find(':', separator + 1) != string_view::npos) {
                throw DataBaseException("Invalid database collection setting", entry);
            }

            const auto collection_name = strex(entry.substr(0, separator)).trim().str();
            const auto key_type_name = strex(entry.substr(separator + 1)).trim().str();

            if (collection_name.empty() || key_type_name.empty()) {
                throw DataBaseException("Invalid database collection setting", entry);
            }

            DataBaseKeyType key_type;

            if (strex(key_type_name).lower().trim().str() == "int") {
                key_type = DataBaseKeyType::IntId;
            }
            else if (strex(key_type_name).lower().trim().str() == "str") {
                key_type = DataBaseKeyType::String;
            }
            else {
                throw DataBaseException("Unknown database key type", key_type_name);
            }

            register_collection(Hashes.ToHashedString(collection_name), key_type);
        };

        register_collection(GameCollectionName, DataBaseKeyType::IntId);
        register_collection(HistoryCollectionName, DataBaseKeyType::IntId);

        for (const auto& type_desc : GetEntityTypes() | std::views::values) {
            register_collection(type_desc.PropRegistrator->GetTypeNamePlural(), DataBaseKeyType::IntId);
        }
        for (const auto& entry : Settings.CustomCollections) {
            register_custom_collection(entry);
        }

        DbStorage = ConnectToDataBase(Settings, Settings.DbStorage, collection_schemas, [] {
            FO_RUNTIME_ASSERT(App);
            App->RequestQuit(false);
        });

        return std::nullopt;
    });

    // Engine data
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitMetadataJob");

        WriteLog("Setup engine");

        // Properties that saving to database
        for (const auto& type_desc : GetEntityTypes() | std::views::values) {
            const auto& registrator = type_desc.PropRegistrator;

            for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
                const auto* prop = registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));

                if (prop->IsDisabled()) {
                    continue;
                }
                if (!prop->IsPersistent()) {
                    continue;
                }

                prop->AddPostSetter([this](Entity* entity, const Property* prop_) FO_DEFERRED { OnSaveEntityValue(entity, prop_); });
            }
        }

        // Properties that sending to clients
        {
            const auto set_send_callbacks = [](const auto* registrator, const PropertyPostSetCallback& callback) {
                for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
                    const auto* prop = registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));

                    if (prop->IsDisabled()) {
                        continue;
                    }
                    if (!prop->IsSynced()) {
                        continue;
                    }

                    prop->AddPostSetter(callback);
                }
            };

            set_send_callbacks(GetPropertyRegistrator(GameProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSendGlobalValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(PlayerProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSendPlayerValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSendItemValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSendCritterValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(MapProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSendMapValue(entity, prop); });
            set_send_callbacks(GetPropertyRegistrator(LocationProperties::ENTITY_TYPE_NAME), [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSendLocationValue(entity, prop); });

            for (const auto& type_desc : GetEntityTypes() | std::views::values) {
                if (type_desc.Exported) {
                    continue;
                }

                set_send_callbacks(type_desc.PropRegistrator.get(), [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSendCustomEntityValue(entity, prop); });
            }
        }

        // Properties with custom behaviours
        {
            const auto set_setter = [](const auto* registrator, int32_t prop_index, PropertySetCallback callback) {
                const auto* prop = registrator->GetPropertyByIndex(prop_index);
                prop->AddSetter(std::move(callback));
            };
            const auto set_post_setter = [](const auto* registrator, int32_t prop_index, PropertyPostSetCallback callback) {
                const auto* prop = registrator->GetPropertyByIndex(prop_index);
                prop->AddPostSetter(std::move(callback));
            };

            set_post_setter(GetPropertyRegistrator(CritterProperties::ENTITY_TYPE_NAME), Critter::LookDistance_RegIndex, [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSetCritterLookDistance(entity, prop); });
            set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::Count_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& data) FO_DEFERRED { OnSetItemCount(entity, prop, data.GetPtrAs<void>()); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::Hidden_RegIndex, [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSetItemHidden(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::NoBlock_RegIndex, [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSetItemRecacheHex(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::ShootThru_RegIndex, [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSetItemRecacheHex(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::IsGag_RegIndex, [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSetItemRecacheHex(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::IsTrigger_RegIndex, [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSetItemRecacheHex(entity, prop); });
            set_post_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::MultihexLines_RegIndex, [this](Entity* entity, const Property* prop) FO_DEFERRED { OnSetItemMultihexLines(entity, prop); });
        }

        return std::nullopt;
    });

    // Language
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitLanguageJob");

        WriteLog("Load language data");

        _defaultLang = TextPack {Hashes};
        _defaultLang.LoadFromResources(Resources, Settings.Language);

        return std::nullopt;
    });

    // Maps
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitMapsJob");

        WriteLog("Load maps data");

        MapMngr.LoadFromResources();

        return std::nullopt;
    });

    // Resource packs for client
    _starter.AddJob([this]() FO_DEFERRED {
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

                writer.Write<int16_t>(numeric_cast<int16_t>(file.GetPath().length()));
                writer.WritePtr(file.GetPath().data(), file.GetPath().length());
                writer.Write<uint32_t>(numeric_cast<uint32_t>(data.size()));
                writer.Write<uint64_t>(hashing_ex::hash(data.data(), data.size()));
            };

            for (const auto& resource_entry : Settings.ClientResourceEntries) {
                if (resource_entry != "Embedded") {
                    add_sync_file(strex("{}.zip", resource_entry));
                }
            }

            // Complete files list
            writer.Write<int16_t>(const_numeric_cast<int16_t>(-1));
        }

        return std::nullopt;
    });

    // Game logic
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitGameLogicJob");

        WriteLog("Start game logic");

        try {
            // Globals
            const auto globals_doc = DbStorage.Get(GameCollectionName, ident_t {1});

            if (globals_doc.Empty()) {
                AnyData::Document doc;
                doc.Emplace("_Name", string("Globals"));
                DbStorage.Insert(GameCollectionName, ident_t {1}, doc);
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

            ServerInitHook(this);

            InitModules();

            if (OnInit.Fire() == EventResult::StopChain) {
                throw ServerInitException("Initialization script failed");
            }

            // Init world
            if (globals_doc.Empty()) {
                WriteLog("Generate world");

                if (OnGenerateWorld.Fire() == EventResult::StopChain) {
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
            if (OnStart.Fire() == EventResult::StopChain) {
                throw ServerInitException("Start script failed");
            }
        }
        catch (...) {
            // Don't change database
            DbStorage.ClearChanges();

            throw;
        }

        // Start automatic committing only after successful initialization
        DbStorage.StartCommitChanges();
        DbStorage.WaitCommitChanges();

        // Advance time after initialization
        FrameAdvance();

        return std::nullopt;
    });

    // Done, fill regular jobs
    _starter.AddJob([this]() FO_DEFERRED {
        FO_STACK_TRACE_ENTRY_NAMED("InitDoneJob");

        WriteLog("Start server complete!");

        _started = true;

        _loopBalancer = FrameBalancer {true, Settings.ServerSleep, Settings.LoopsPerSecondCap};
        _loopBalancer.StartLoop();

        _stats.LoopCounterBegin = nanotime::now();
        _stats.ServerStartTime = nanotime::now();

        // Sync point
        _mainWorker.AddJob([this]() FO_DEFERRED {
            FO_STACK_TRACE_ENTRY_NAMED("SyncPointJob");

            SyncPoint();

            return std::chrono::milliseconds {0};
        });

        // Advance time
        _mainWorker.AddJob([this]() FO_DEFERRED {
            FO_STACK_TRACE_ENTRY_NAMED("FrameTimeJob");

            FrameAdvance();

            return std::chrono::milliseconds {0};
        });

        // Script subsystems update
        _mainWorker.AddJob([this]() FO_DEFERRED {
            FO_STACK_TRACE_ENTRY_NAMED("ScriptSystemJob");

            ProcessScriptEvents();

            return std::chrono::milliseconds {0};
        });

        // Process unlogined players
        _mainWorker.AddJob([this]() FO_DEFERRED {
            FO_STACK_TRACE_ENTRY_NAMED("UnloginedPlayersJob");

            vector<refcount_ptr<Player>> unlogined_players;

            {
                std::scoped_lock locker {_unloginedPlayersLocker};

                unlogined_players = _unloginedPlayers;
            }

            for (auto& player : unlogined_players) {
                if (player->IsDestroyed()) {
                    continue;
                }

                auto* connection = player->GetConnection();

                try {
                    ProcessConnection(connection);
                    ProcessUnloginedPlayer(player.get());
                }
                catch (const UnknownMessageException&) {
                    WriteLog(LogType::Warning, "Invalid network data from host {}:{}", connection->GetHost(), connection->GetPort());
                    connection->HardDisconnect();
                }
                catch (const NetBufferException& ex) {
                    ReportExceptionAndContinue(ex);
                    connection->HardDisconnect();
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
            }

            _stats.CurOnline = unlogined_players.size() + EntityMngr.GetPlayers().size();
            _stats.MaxOnline = std::max(_stats.MaxOnline, _stats.CurOnline);

            return std::chrono::milliseconds {0};
        });

        // Process players
        _mainWorker.AddJob([this]() FO_DEFERRED {
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
            }

            return std::chrono::milliseconds {0};
        });

        // Process critters
        _mainWorker.AddJob([this]() FO_DEFERRED {
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
            }

            return std::chrono::milliseconds {0};
        });

        // Time events
        _mainWorker.AddJob([this]() FO_DEFERRED {
            FO_STACK_TRACE_ENTRY_NAMED("TimeEventsJob");

            TimeEventMngr.ProcessTimeEvents();

            return std::chrono::milliseconds {0};
        });

        // Clients log
        _mainWorker.AddJob([this]() FO_DEFERRED {
            FO_STACK_TRACE_ENTRY_NAMED("LogDispatchJob");

            DispatchLogToClients();

            return std::chrono::milliseconds {0};
        });

        // Loop stats
        _mainWorker.AddJob([this]() FO_DEFERRED {
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
            TracyPlot("Server loops per second", numeric_cast<int64_t>(_stats.LoopsPerSecond));
#endif

            return std::chrono::milliseconds {0};
        });

        return std::nullopt;
    });
}

ServerEngine::~ServerEngine()
{
    FO_STACK_TRACE_ENTRY();
}

void ServerEngine::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Stop server");

    _willFinishDispatcher();
    _starter.Clear();
    _mainWorker.Clear();
    _healthWriter.Clear();

    OnFinish.Fire();

    UnsubscribeAllEvents();
    ClearAllTimeEvents();

    for (auto& entity : EntityMngr.GetEntities() | std::views::values) {
        entity->UnsubscribeAllEvents();
        entity->ClearAllTimeEvents();

        if (auto* item = dynamic_cast<Item*>(entity.get()); item != nullptr) {
            item->StaticScriptFunc = {};
            item->TriggerScriptFunc = {};
        }
    }

    TimeEventMngr.ClearTimeEvents();
    EntityMngr.DestroyInnerEntities(this);
    EntityMngr.DestroyAllEntities();
    ShutdownBackends();
    DbStorage.WaitCommitChanges();

    // Logging clients
    SetLogCallback("LogToClients", nullptr);
    _logClients.clear();

    // Logined players
    for (auto& player : copy(EntityMngr.GetPlayers()) | std::views::values) {
        player->GetConnection()->HardDisconnect();
    }

    // Unlogined players
    {
        std::scoped_lock locker {_unloginedPlayersLocker};

        for (auto& player : _unloginedPlayers) {
            player->GetConnection()->HardDisconnect();
            player->MarkAsDestroyed();
        }

        _unloginedPlayers.clear();
    }

    // Shutdown servers
    for (auto& conn_server : _connectionServers) {
        conn_server->Shutdown();
    }

    _connectionServers.clear();

    // Done
    WriteLog("Server stopped!");
    _started = false;
    _didFinishDispatcher();

    FO_RUNTIME_ASSERT(GetRefCount() == 1);
}

auto ServerEngine::Lock(optional<timespan> max_wait_time) -> bool
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

void ServerEngine::Unlock()
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

void ServerEngine::SyncPoint()
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

void ServerEngine::DrawGui()
{
    FO_STACK_TRACE_ENTRY();

    if (!_started) {
        if (!_startingError) {
            ImGui::TextUnformatted("Server is starting...");
        }
        else {
            ImGui::TextUnformatted("Server starting error, see log");
        }

        return;
    }

    if (Settings.LockMaxWaitTime != 0) {
        const auto max_wait_time = timespan {std::chrono::milliseconds {Settings.LockMaxWaitTime}};

        if (!Lock(max_wait_time)) {
            ImGui::TextUnformatted(strex("Server hanged (no response more than {})", max_wait_time).c_str());
            WriteLog(LogType::Warning, "Server hanged (no response more than {})", max_wait_time);
            return;
        }
    }
    else {
        Lock(std::nullopt);
    }

    auto unlocker = scope_exit([this]() noexcept { safe_call([this] { Unlock(); }); });

    constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;

    const auto info_row = [](const char* key, string_view value) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(key);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    };

    const auto begin_info_table = [](const char* id) -> bool {
        if (ImGui::BeginTable(id, 2, TABLE_FLAGS)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            return true;
        }
        return false;
    };

    const auto draw_properties_table = [&info_row, &begin_info_table](const Entity* entity) {
        if (begin_info_table("##PropsTable")) {
            const auto& props = entity->GetProperties();
            const auto* registrator = props.GetRegistrator();
            const auto props_count = registrator->GetPropertiesCount();

            for (size_t i = 1; i < props_count; ++i) {
                const auto* prop = registrator->GetPropertyByIndexUnsafe(i);

                if (prop->IsDisabled()) {
                    continue;
                }
                if (prop->IsClientOnly()) {
                    continue;
                }
                if (prop->IsComponentItself()) {
                    continue;
                }
                if (prop->IsVirtual()) {
                    continue;
                }

                string value_str;

                try {
                    value_str = props.SavePropertyToText(prop);
                }
                catch (const std::exception& ex) {
                    value_str = strex("<error: {}>", ex.what()).str();
                }
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }

                info_row(prop->GetName().c_str(), value_str);
            }

            ImGui::EndTable();
        }
    };

    if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (begin_info_table("##InfoTable")) {
            info_row("Version", strex("{}", Settings.GameVersion).str());
            info_row("Compatibility version", strex("{}", Settings.CompatibilityVersion).str());
            info_row("System time", strex("{}", nanotime::now()).str());
            info_row("Synchronized time", strex("{}", GetSynchronizedTime()).str());
            info_row("Server uptime", strex("{}", _stats.Uptime).str());
            info_row("Connections", strex("{}", _stats.CurOnline).str());
            info_row("Players", strex("{}", EntityMngr.GetPlayersCount()).str());
            info_row("Critters", strex("{}", EntityMngr.GetCrittersCount()).str());
            info_row("Locations", strex("{}", EntityMngr.GetLocationsCount()).str());
            info_row("Maps", strex("{}", EntityMngr.GetMapsCount()).str());
            info_row("Items", strex("{}", EntityMngr.GetItemsCount()).str());
            info_row("Total entities", strex("{}", EntityMngr.GetEntitiesCount()).str());
            info_row("Loops per second", strex("{}", _stats.LoopsPerSecond).str());
            info_row("Average loop time", strex("{}", _stats.LoopAvgTime).str());
            info_row("Min loop time", strex("{}", _stats.LoopMinTime).str());
            info_row("Max loop time", strex("{}", _stats.LoopMaxTime).str());
            info_row("DB requests per minute", strex("{}", DbStorage.GetDbRequestsPerMinute()).str());

            ImGui::EndTable();
        }
    }

    const auto cond_to_str = [](CritterCondition cond) -> const char* {
        switch (cond) {
        case CritterCondition::Alive:
            return "Alive";
        case CritterCondition::Knockout:
            return "Knockout";
        case CritterCondition::Dead:
            return "Dead";
        }
        return "?";
    };

    function<void(Item*)> draw_item;
    draw_item = [&](Item* item) {
        ImGui::PushID(static_cast<const void*>(item));

        const auto label = strex("{} ({}) x{}", item->GetName(), item->GetId(), item->GetCount()).str();

        if (ImGui::TreeNode(label.c_str())) {
            if (begin_info_table("##ItemSummary")) {
                info_row("Id", strex("{}", item->GetId()).str());
                info_row("Proto", strex("{}", item->GetProtoId()).str());
                info_row("Count", strex("{}", item->GetCount()).str());
                info_row("Stackable", strex("{}", item->GetStackable()).str());
                info_row("Ownership", strex("{}", item->GetOwnership()).str());
                info_row("Critter slot", strex("{}", item->GetCritterSlot()).str());
                info_row("Critter id", strex("{}", item->GetCritterId()).str());
                info_row("Map id", strex("{}", item->GetMapId()).str());
                info_row("Hex", strex("{}", item->GetHex()).str());
                info_row("Container id", strex("{}", item->GetContainerId()).str());
                info_row("Container stack", strex("{}", item->GetContainerStack()).str());
                info_row("Has inner items", strex("{}", item->HasInnerItems()).str());
                ImGui::EndTable();
            }

            if (ImGui::TreeNode("Properties")) {
                draw_properties_table(item);
                ImGui::TreePop();
            }

            if (item->HasInnerItems()) {
                const auto inner_items = item->GetAllInnerItems();

                if (ImGui::TreeNode(strex("Inner items ({})", inner_items.size()).c_str())) {
                    for (auto* inner : inner_items) {
                        draw_item(inner);
                    }
                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    };

    const auto draw_critter = [&](Critter* cr) {
        ImGui::PushID(static_cast<const void*>(cr));

        const char* cond_str = cond_to_str(cr->GetCondition());
        const auto label = strex("{} ({}) [{}]", cr->GetName(), cr->GetId(), cond_str).str();

        if (ImGui::TreeNode(label.c_str())) {
            if (begin_info_table("##CritterSummary")) {
                info_row("Id", strex("{}", cr->GetId()).str());
                info_row("Name", cr->GetName());
                info_row("Proto", strex("{}", cr->GetProtoId()).str());
                info_row("Map id", strex("{}", cr->GetMapId()).str());
                info_row("Hex", strex("{}", cr->GetHex()).str());
                info_row("Direction", strex("{}", cr->GetDir()).str());
                info_row("Condition", cond_str);
                info_row("Controlled by player", strex("{}", cr->GetControlledByPlayer()).str());
                info_row("Has player", strex("{}", cr->HasPlayer()).str());
                info_row("Offline time", cr->HasPlayer() ? string("0") : strex("{}", cr->GetOfflineTime()).str());
                info_row("Is moving", strex("{}", cr->IsMoving()).str());
                info_row("Is attached", strex("{}", cr->GetIsAttached()).str());
                info_row("Attach master", strex("{}", cr->GetAttachMaster()).str());
                info_row("Look distance", strex("{}", cr->GetLookDistance()).str());
                info_row("Multihex", strex("{}", cr->GetMultihex()).str());
                info_row("Lexems", cr->GetLexems());
                info_row("Inventory items", strex("{}", cr->GetInvItems().size()).str());
                info_row("Visible items", strex("{}", cr->GetVisibleItems().size()).str());
                info_row("Attached critters", strex("{}", cr->GetAttachedCritters().size()).str());
                ImGui::EndTable();
            }

            if (ImGui::TreeNode("Properties")) {
                draw_properties_table(cr);
                ImGui::TreePop();
            }

            const auto inv_items = cr->GetInvItems();

            if (!inv_items.empty()) {
                if (ImGui::TreeNode(strex("Inventory ({})", inv_items.size()).c_str())) {
                    for (auto& item : inv_items) {
                        draw_item(item.get());
                    }
                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    };

    const auto draw_map = [&](Map* map) {
        ImGui::PushID(static_cast<const void*>(map));

        const auto label = strex("{} ({})", map->GetProtoId(), map->GetId()).str();

        if (ImGui::TreeNode(label.c_str())) {
            const auto critters = map->GetCritters();
            const auto items = map->GetItems();
            const auto static_items = map->GetStaticItems();
            const auto map_size = map->GetSize();

            if (begin_info_table("##MapSummary")) {
                info_row("Id", strex("{}", map->GetId()).str());
                info_row("Proto", strex("{}", map->GetProtoId()).str());
                info_row("Location id", strex("{}", map->GetLocId()).str());
                info_row("Size", strex("{}x{}", map_size.width, map_size.height).str());
                info_row("Work hex", strex("{}", map->GetWorkHex()).str());
                info_row("Critters", strex("{}", critters.size()).str());
                info_row("Player critters", strex("{}", map->GetPlayerCritters().size()).str());
                info_row("Non-player critters", strex("{}", map->GetNonPlayerCritters().size()).str());
                info_row("Items", strex("{}", items.size()).str());
                info_row("Static items", strex("{}", static_items.size()).str());
                ImGui::EndTable();
            }

            if (ImGui::TreeNode("Properties")) {
                draw_properties_table(map);
                ImGui::TreePop();
            }

            if (!critters.empty()) {
                if (ImGui::TreeNode(strex("Critters ({})", critters.size()).c_str())) {
                    for (auto& cr : critters) {
                        draw_critter(cr.get());
                    }
                    ImGui::TreePop();
                }
            }

            if (!items.empty()) {
                if (ImGui::TreeNode(strex("Items ({})", items.size()).c_str())) {
                    for (auto& item : items) {
                        draw_item(item.get());
                    }
                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    };

    if (ImGui::CollapsingHeader(strex("Players ({})###Players", EntityMngr.GetPlayersCount()).c_str())) {
        for (auto& player : EntityMngr.GetPlayers() | std::views::values) {
            ImGui::PushID(static_cast<const void*>(player.get()));

            const auto label = strex("{} ({})", player->GetName(), player->GetId()).str();

            if (ImGui::TreeNode(label.c_str())) {
                auto* cr = player->GetControlledCritter();
                const auto* connection = player->GetConnection();

                if (begin_info_table("##PlayerSummary")) {
                    info_row("Id", strex("{}", player->GetId()).str());
                    info_row("Name", player->GetName());
                    info_row("Logined", strex("{}", player->GetLogined()).str());
                    info_row("Host", connection->GetHost());
                    info_row("Port", strex("{}", connection->GetPort()).str());
                    info_row("Hard disconnected", strex("{}", connection->IsHardDisconnected()).str());
                    info_row("Graceful disconnected", strex("{}", connection->IsGracefulDisconnected()).str());
                    info_row("Was handshake", strex("{}", connection->WasHandshake).str());
                    info_row("Last activity", strex("{}", connection->LastActivityTime).str());
                    info_row("Ping ok", strex("{}", connection->PingOk).str());
                    info_row("Controlled critter id", strex("{}", player->GetControlledCritterId()).str());
                    info_row("Last controlled critter id", strex("{}", player->GetLastControlledCritterId()).str());
                    ImGui::EndTable();
                }

                if (ImGui::TreeNode("Properties")) {
                    draw_properties_table(player.get());
                    ImGui::TreePop();
                }

                if (cr != nullptr) {
                    if (ImGui::TreeNode("Controlled critter")) {
                        draw_critter(cr);
                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    {
        std::scoped_lock locker {_unloginedPlayersLocker};

        if (ImGui::CollapsingHeader(strex("Unlogined players ({})###UnloginedPlayers", _unloginedPlayers.size()).c_str())) {
            int32_t index = 0;

            for (auto& player : _unloginedPlayers) {
                ImGui::PushID(index++);

                const auto* connection = player->GetConnection();
                const auto label = strex("{}:{}", connection->GetHost(), connection->GetPort()).str();

                if (ImGui::TreeNode(label.c_str())) {
                    if (begin_info_table("##UnloginedSummary")) {
                        info_row("Host", connection->GetHost());
                        info_row("Port", strex("{}", connection->GetPort()).str());
                        info_row("Was handshake", strex("{}", connection->WasHandshake).str());
                        info_row("Hard disconnected", strex("{}", connection->IsHardDisconnected()).str());
                        info_row("Graceful disconnected", strex("{}", connection->IsGracefulDisconnected()).str());
                        info_row("Last activity", strex("{}", connection->LastActivityTime).str());
                        info_row("Ping ok", strex("{}", connection->PingOk).str());
                        info_row("Update file index", strex("{}", connection->UpdateFileIndex).str());
                        info_row("Update file portion", strex("{}", connection->UpdateFilePortion).str());
                        ImGui::EndTable();
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }
    }

    if (ImGui::CollapsingHeader(strex("Locations ({})###Locations", EntityMngr.GetLocationsCount()).c_str())) {
        for (auto& loc : EntityMngr.GetLocations() | std::views::values) {
            ImGui::PushID(static_cast<const void*>(loc.get()));

            const auto label = strex("{} ({})", loc->GetProtoId(), loc->GetId()).str();

            if (ImGui::TreeNode(label.c_str())) {
                if (begin_info_table("##LocSummary")) {
                    info_row("Id", strex("{}", loc->GetId()).str());
                    info_row("Proto", strex("{}", loc->GetProtoId()).str());
                    info_row("Name", loc->GetName());
                    info_row("Maps count", strex("{}", loc->GetMapsCount()).str());
                    ImGui::EndTable();
                }

                if (ImGui::TreeNode("Properties")) {
                    draw_properties_table(loc.get());
                    ImGui::TreePop();
                }

                if (loc->HasMaps()) {
                    if (ImGui::TreeNode(strex("Maps ({})", loc->GetMapsCount()).c_str())) {
                        for (auto& map : loc->GetMaps()) {
                            draw_map(map.get());
                        }
                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Data base")) {
        DbStorage.DrawGui();
    }
}

auto ServerEngine::GetHealthInfo() const -> string
{
    FO_STACK_TRACE_ENTRY();

    string buf;
    buf.reserve(2048);

    buf += strex("Version: {}\n", Settings.GameVersion);
    buf += strex("Compatibility version: {}\n", Settings.CompatibilityVersion);
    buf += strex("System time: {}\n", nanotime::now());
    buf += strex("Synchronized time: {}\n", GetSynchronizedTime());
    buf += strex("Server uptime: {}\n", _stats.Uptime);
    buf += strex("Connections: {}\n", _stats.CurOnline);
    buf += strex("Players: {}\n", EntityMngr.GetPlayersCount());
    buf += strex("Critters: {}\n", EntityMngr.GetCrittersCount());
    buf += strex("Locations: {}\n", EntityMngr.GetLocationsCount());
    buf += strex("Maps: {}\n", EntityMngr.GetMapsCount());
    buf += strex("Items: {}\n", EntityMngr.GetItemsCount());
    buf += strex("Total entities: {}\n", EntityMngr.GetEntitiesCount());
    buf += strex("Loops per second: {}\n", _stats.LoopsPerSecond);
    buf += strex("Average loop time: {}\n", _stats.LoopAvgTime);
    buf += strex("Min loop time: {}\n", _stats.LoopMinTime);
    buf += strex("Max loop time: {}\n", _stats.LoopMaxTime);
    buf += strex("DB requests per minute: {}\n", DbStorage.GetDbRequestsPerMinute());

    return buf;
}

void ServerEngine::OnNewConnection(shared_ptr<NetworkServerConnection> net_connection)
{
    FO_STACK_TRACE_ENTRY();

    if (!_started) {
        net_connection->Disconnect();
        return;
    }

    CreateUnloginedPlayer(std::move(net_connection));
}

auto ServerEngine::CreateUnloginedPlayer(shared_ptr<NetworkServerConnection> net_connection) -> Player*
{
    FO_STACK_TRACE_ENTRY();

    std::scoped_lock locker {_unloginedPlayersLocker};

    auto connection = SafeAlloc::MakeUnique<ServerConnection>(Settings, std::move(net_connection));
    auto new_player = SafeAlloc::MakeRefCounted<Player>(this, ident_t {}, std::move(connection));
    _unloginedPlayers.emplace_back(std::move(new_player));
    return _unloginedPlayers.back().get();
}

void ServerEngine::ProcessUnloginedPlayer(Player* unlogined_player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = unlogined_player->GetConnection();

    if (connection->IsHardDisconnected()) {
        std::scoped_lock locker {_unloginedPlayersLocker};

        vec_remove_unique_value(_unloginedPlayers, unlogined_player);
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
                unlogined_player->Send_TimeSync();
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
            default:
                throw GenericException("Unexpected unlogined player message", msg);
            }
        }

        in_buf.Lock();
        in_buf->ShrinkReadBuf();

        connection->LastActivityTime = GameTime.GetFrameTime();
    }
}

void ServerEngine::ProcessPlayer(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();

    if (connection->IsHardDisconnected()) {
        WriteLog("Disconnected player {}", player->GetName());

        OnPlayerLogout.Fire(player);

        player->DetachCritter();
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
            player->Send_TimeSync();
            break;

        case NetMessage::SendCommand:
            in_buf.Lock();
            Process_Command(*in_buf, [player](LogType, string_view s) { player->Send_InfoMessage(EngineInfoMessage::ServerLog, strvex(s).trim()); }, player);
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

void ServerEngine::ProcessConnection(ServerConnection* connection)
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

void ServerEngine::HandleOutboundRemoteCall(hstring name, Entity* caller, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    Player* player = nullptr;
    auto* cr = dynamic_cast<Critter*>(caller);

    if (cr != nullptr) {
        player = cr->GetPlayer();
    }
    else {
        player = dynamic_cast<Player*>(caller);
        FO_RUNTIME_ASSERT(player);
    }

    if (player == nullptr) {
        return;
    }

    auto out_buf = player->GetConnection()->WriteMsg(NetMessage::RemoteCall);

    out_buf->Write<hstring>(name);
    out_buf->Write<int32_t>(numeric_cast<int32_t>(data.size()));
    out_buf->Push(data);
}

void ServerEngine::Process_Command(NetInBuffer& buf, const LogFunc& logcb_typed, Player* player)
{
    FO_STACK_TRACE_ENTRY();

    SetLogCallback("Process_Command", logcb_typed);
    auto remove_log_callback = scope_exit([]() noexcept { safe_call([] { SetLogCallback("Process_Command", nullptr); }); });

    // Local untyped helper so the existing command-output sites stay readable; all command
    // responses are user-facing info, not warnings/errors.
    const auto logcb = [&logcb_typed](string_view s) { logcb_typed(LogType::Info, s); };

    const auto cmd = buf.Read<uint8_t>();
    auto* player_cr = player->GetControlledCritter();
    const auto allow_command = OnPlayerAllowCommand.Fire(player, cmd) == EventResult::ContinueChain;

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
        std::scoped_lock locker {_unloginedPlayersLocker};

        string str = strex("Unlogined players: {}, Logined players: {}, Critters: {}, Frame time: {}, Server uptime: {}", //
            _unloginedPlayers.size(), EntityMngr.GetPlayersCount(), EntityMngr.GetCrittersCount(), GameTime.GetFrameTime(), GameTime.GetFrameTime() - _stats.ServerStartTime);
        logcb(str);
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
        const auto count = buf.Read<int32_t>();

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
        const auto count = buf.Read<int32_t>();

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
        const auto dir = mdir(buf.Read<hdir>());
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
        const auto func_name = Hashes.ToHashedString(buf.Read<string>());
        const auto param0_str = any_t {buf.Read<string>()};
        const auto param1_str = any_t {buf.Read<string>()};
        const auto param2_str = any_t {buf.Read<string>()};

        if (!func_name) {
            logcb("Fail, length is zero");
            break;
        }

        if (CallAdminFunc<void, Player*>(func_name, player) || //
            CallAdminFunc<void, Player*, any_t>(func_name, player, param0_str) || //
            CallAdminFunc<void, Player*, any_t, any_t>(func_name, player, param0_str, param1_str) || //
            CallAdminFunc<void, Player*, any_t, any_t, any_t>(func_name, player, param0_str, param1_str, param2_str) || //
            CallAdminFunc<void, Critter*>(func_name, player_cr) || //
            CallAdminFunc<void, Critter*, any_t>(func_name, player_cr, param0_str) || //
            CallAdminFunc<void, Critter*, any_t, any_t>(func_name, player_cr, param0_str, param1_str) || //
            CallAdminFunc<void, Critter*, any_t, any_t, any_t>(func_name, player_cr, param0_str, param1_str, param2_str) || //
            CallAdminFunc<void>(func_name) || //
            CallAdminFunc<void, any_t>(func_name, param0_str) || //
            CallAdminFunc<void, any_t, any_t>(func_name, param0_str, param1_str) || //
            CallAdminFunc<void, any_t, any_t, any_t>(func_name, param0_str, param1_str, param2_str)) {
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
            SetLogCallback("LogToClients", [this](LogType, string_view str) FO_DEFERRED { LogToClients(str); });
        }
    } break;
    default:
        logcb("Unknown command");
        break;
    }
}

void ServerEngine::LogToClients(string_view str)
{
    FO_STACK_TRACE_ENTRY();

    if (!str.empty() && str.back() == '\n') {
        _logLines.emplace_back(str, 0, str.length() - 1);
    }
    else {
        _logLines.emplace_back(str);
    }
}

void ServerEngine::DispatchLogToClients()
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

auto ServerEngine::CreateCritter(hstring pid, bool for_player, const Properties* props) -> Critter*
{
    FO_STACK_TRACE_ENTRY();

    WriteLog(LogType::Info, "Create critter {}", pid);

    const auto* proto = GetProtoCritter(pid);

    if (proto == nullptr) {
        throw GenericException("Critter proto not found", pid);
    }

    auto cr = SafeAlloc::MakeRefCounted<Critter>(this, ident_t {}, proto, props);

    EntityMngr.RegisterCritter(cr.get());

    if (for_player) {
        cr->MarkIsForPlayer();
    }

    MapMngr.AddCritterToMap(cr.get(), nullptr, {}, hdir {}, {});

    if (!cr->IsDestroyed()) {
        OnGlobalMapCritterIn.Fire(cr.get());
    }
    if (!cr->IsDestroyed()) {
        EntityMngr.CallInit(cr.get(), true);
        MapMngr.ProcessVisibleCritters(cr.get());
        MapMngr.ProcessVisibleItems(cr.get());
    }

    if (cr->IsDestroyed()) {
        throw GenericException("Critter destroyed during init");
    }

    return cr.get();
}

auto ServerEngine::LoadCritter(ident_t cr_id, bool for_player) -> Critter*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr_id);

    WriteLog(LogType::Info, "Load critter {}", cr_id);

    if (EntityMngr.GetCritter(cr_id) != nullptr) {
        throw GenericException("Critter already in game");
    }

    bool is_error = false;
    Critter* cr = EntityMngr.LoadCritter(cr_id, is_error);
    refcount_ptr cr_holder = cr;

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

    EntityMngr.MakePersistent(cr, true, true);
    MapMngr.AddCritterToMap(cr, nullptr, {}, hdir {}, {});

    if (!cr->IsDestroyed()) {
        OnGlobalMapCritterIn.Fire(cr);
    }
    if (!cr->IsDestroyed()) {
        EntityMngr.CallInit(cr, false);
        MapMngr.ProcessVisibleCritters(cr);
        MapMngr.ProcessVisibleItems(cr);
    }
    if (!cr->IsDestroyed()) {
        OnCritterLoad.Fire(cr);
    }

    if (cr->IsDestroyed()) {
        throw GenericException("Player critter destroyed during loading");
    }

    return cr;
}

void ServerEngine::UnloadCritter(Critter* cr)
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

    if (cr->HasAttachedCritters()) {
        for (auto* attached_cr : copy_hold_ref(cr->GetAttachedCritters())) {
            attached_cr->DetachFromCritter();
        }
    }

    UnloadCritterInnerEntities(cr);
    cr->MarkAsDestroyed();
    EntityMngr.UnregisterCritter(cr);
}

void ServerEngine::UnloadCritterInnerEntities(Critter* cr)
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
    };

    for (auto* item : copy_hold_ref(cr->GetInvItems())) {
        unload_item(item);
        cr->RemoveItem(item);
    }
}

void ServerEngine::SwitchPlayerCritter(Player* player, Critter* cr)
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

    if (prev_cr != nullptr) {
        OnPlayerCritterSwitched.Fire(player, cr, prev_cr);
    }
}

void ServerEngine::DestroyUnloadedCritter(ident_t cr_id)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr_id);

    WriteLog(LogType::Info, "Destroy unloaded critter {}", cr_id);

    if (EntityMngr.GetCritter(cr_id) != nullptr) {
        throw GenericException("Critter must be unloaded before destroying");
    }

    DbStorage.Delete(Hashes.ToHashedString("Critters"), cr_id);
}

void ServerEngine::SendCritterInitialInfo(Critter* cr, Critter* prev_cr)
{
    cr->Send_TimeSync();

    if (const auto* view_map = cr->GetViewMap(); view_map != nullptr) {
        auto* map = EntityMngr.GetMap(view_map->MapId);

        if (map != nullptr) {
            MapMngr.ViewMap(cr, map, view_map->Look, view_map->Hex, view_map->Dir);
            cr->Send_ViewMap();
            cr->Send_TimeSync();
            cr->ResetViewMap();
            return;
        }

        cr->ResetViewMap();
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
    cr->Send_TimeSync();
}

void ServerEngine::Process_Handshake(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    auto in_buf = connection->ReadBuf();

    // Net protocol
    const auto comp_version = in_buf->Read<string>();
    const auto outdated = comp_version != Settings.CompatibilityVersion;

    // Begin data encrypting
    const auto in_encrypt_key = in_buf->Read<uint32_t>();

    if (in_encrypt_key == 0) {
        WriteLog("Process_Handshake: zero encrypt key from host '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    in_buf->SetEncryptKey(in_encrypt_key);

    in_buf.Unlock();

    const uint32_t out_encrypt_key = //
        (numeric_cast<uint32_t>(Random(1, 255)) << 24) | //
        (numeric_cast<uint32_t>(Random(1, 255)) << 16) | //
        (numeric_cast<uint32_t>(Random(1, 255)) << 8) | //
        (numeric_cast<uint32_t>(Random(1, 255)) << 0);

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
        vector<const uint8_t*>* global_vars_data = nullptr;
        vector<uint32_t>* global_vars_data_sizes = nullptr;
        StoreData(false, &global_vars_data, &global_vars_data_sizes);

        auto out_buf = connection->WriteMsg(NetMessage::InitData);

        out_buf->Write(numeric_cast<uint32_t>(_updateFilesDesc.size()));
        out_buf->Push(_updateFilesDesc);
        out_buf->WritePropsData(global_vars_data, global_vars_data_sizes);
        out_buf->Write(GameTime.GetSynchronizedTime());
    }

    connection->WasHandshake = true;

    if (outdated) {
        WriteLog("Connected client {} has outdated compatibility version {}", connection->GetHost(), comp_version);
    }
    else {
        WriteLog("Connected client {}", connection->GetHost());
    }
}

void ServerEngine::Process_Ping(ServerConnection* connection)
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

void ServerEngine::Process_UpdateFile(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    auto in_buf = connection->ReadBuf();

    const auto file_index = in_buf->Read<uint32_t>();

    in_buf.Unlock();

    if (file_index >= _updateFilesData.size()) {
        WriteLog(LogType::Warning, "Wrong file index {}, from host '{}'", file_index, connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    connection->UpdateFileIndex = numeric_cast<int32_t>(file_index);
    connection->UpdateFilePortion = 0;

    Process_UpdateFileData(connection);
}

void ServerEngine::Process_UpdateFileData(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (connection->UpdateFileIndex == -1) {
        WriteLog(LogType::Warning, "Wrong update call, client host '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    const auto& update_file_data = _updateFilesData[connection->UpdateFileIndex];
    auto update_portion = Settings.UpdateFileSendSize;
    const auto offset = connection->UpdateFilePortion * update_portion;

    if (offset + update_portion < numeric_cast<int32_t>(update_file_data.size())) {
        connection->UpdateFilePortion++;
    }
    else {
        update_portion = numeric_cast<int32_t>(update_file_data.size()) % update_portion;
        connection->UpdateFileIndex = -1;
    }

    auto out_buf = connection->WriteMsg(NetMessage::UpdateFileData);

    out_buf->Write(update_portion);
    out_buf->Push(&update_file_data[offset], update_portion);
}

auto ServerEngine::LoginPlayerToNewRecord(Player* unlogined_player) -> Player*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!unlogined_player->GetLogined());

    auto player_holder = refcount_ptr<Player> {unlogined_player};

    {
        std::scoped_lock locker {_unloginedPlayersLocker};

        vec_remove_unique_value(_unloginedPlayers, unlogined_player);
    }

    auto* player = unlogined_player;
    bool registered_player = false;
    bool inserted_player_record = false;

    scope_fail disconnect_on_error {[&]() noexcept {
        if (inserted_player_record) {
            safe_call([&] { DbStorage.Delete(PlayersCollectionName, player->GetId()); });
        }
        if (registered_player) {
            safe_call([&] { player->DetachCritter(); });
            safe_call([&] { player->MarkAsDestroyed(); });
            safe_call([&] { EntityMngr.UnregisterPlayer(player); });
        }
        safe_call([&] { player->SetLogined(false); });
        safe_call([&] { player->GetConnection()->HardDisconnect(); });
    }};

    EntityMngr.RegisterPlayer(player, ident_t {});
    registered_player = true;

    auto player_doc = PropertiesSerializator::SaveToDocument(&player->GetProperties(), nullptr, Hashes, *this);
    DbStorage.Insert(PlayersCollectionName, player->GetId(), player_doc);
    inserted_player_record = true;

    player->SetLogined(true);
    player->Send_LoginSuccess();

    if (OnPlayerLogin.Fire(player, nullptr) == Entity::EventResult::StopChain) {
        DbStorage.Delete(PlayersCollectionName, player->GetId());
        inserted_player_record = false;
        player->DetachCritter();
        player->MarkAsDestroyed();
        EntityMngr.UnregisterPlayer(player);
        registered_player = false;
        player->SetLogined(false);
        player->GetConnection()->GracefulDisconnect();
        return nullptr;
    }

    return player;
}

auto ServerEngine::LoginPlayerToExistentRecord(Player* unlogined_player, ident_t player_id) -> Player*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!unlogined_player->GetLogined());
    FO_RUNTIME_ASSERT(player_id);

    auto player_holder = refcount_ptr<Player> {unlogined_player};

    {
        std::scoped_lock locker {_unloginedPlayersLocker};

        vec_remove_unique_value(_unloginedPlayers, unlogined_player);
    }

    bool registered_player = false;

    scope_fail disconnect_on_error {[&]() noexcept {
        if (registered_player) {
            safe_call([&] { unlogined_player->DetachCritter(); });
            safe_call([&] { unlogined_player->MarkAsDestroyed(); });
            safe_call([&] { EntityMngr.UnregisterPlayer(unlogined_player); });
        }
        safe_call([&] { unlogined_player->SetLogined(false); });
        safe_call([&] { unlogined_player->GetConnection()->HardDisconnect(); });
    }};

    Player* player = EntityMngr.GetPlayer(player_id);

    if (player == nullptr) {
        player = unlogined_player;
        auto player_doc = DbStorage.Get(PlayersCollectionName, player_id);

        if (player_doc.Empty()) {
            throw GenericException("Player data not found");
        }
        if (!PropertiesSerializator::LoadFromDocument(&player->GetPropertiesForEdit(), player_doc, Hashes, *this)) {
            throw GenericException("Invalid player data");
        }

        EntityMngr.RegisterPlayer(player, player_id);
        registered_player = true;

        if (player_doc.Contains("_Name")) {
            player->SetName(AnyData::ValueToString(player_doc["_Name"]));
        }

        player->SetLogined(true);
        player->Send_LoginSuccess();

        if (OnPlayerLogin.Fire(player, nullptr) == Entity::EventResult::StopChain) {
            player->DetachCritter();
            player->MarkAsDestroyed();
            EntityMngr.UnregisterPlayer(player);
            registered_player = false;
            player->SetLogined(false);
            player->GetConnection()->GracefulDisconnect();
            return nullptr;
        }
    }
    else {
        // Kick previous
        player->SwapConnection(unlogined_player);
        unlogined_player->GetConnection()->HardDisconnect();
        player->Send_LoginSuccess();

        if (OnPlayerLogin.Fire(player, unlogined_player) == Entity::EventResult::StopChain) {
            player->SetLogined(false);
            player->GetConnection()->GracefulDisconnect();
            return nullptr;
        }

        unlogined_player->MarkAsDestroyed();

        auto* cr = player->GetControlledCritter();

        if (cr != nullptr) {
            SendCritterInitialInfo(cr, nullptr);
        }
    }

    return player;
}

void ServerEngine::Process_Move(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto map_id = in_buf->Read<ident_t>();
    const auto cr_id = in_buf->Read<ident_t>();
    const auto speed = in_buf->Read<uint16_t>();
    const auto start_hex = in_buf->Read<mpos>();

    const auto steps_count = in_buf->Read<uint16_t>();
    vector<mdir> steps;
    steps.resize(steps_count);

    for (uint16_t i = 0; i < steps_count; i++) {
        steps[i] = mdir(in_buf->Read<hdir>());
    }

    const auto control_steps_count = in_buf->Read<uint16_t>();
    vector<uint16_t> control_steps;
    control_steps.resize(control_steps_count);

    for (uint16_t i = 0; i < control_steps_count; i++) {
        control_steps[i] = in_buf->Read<uint16_t>();
    }

    const auto end_hex_offset = in_buf->Read<ipos16>();

    in_buf.Unlock();

    auto* map = EntityMngr.GetMap(map_id);

    if (map == nullptr) {
        WriteLog("Process_Move: map not found, player '{}', map_id {}, cr_id {}", player->GetName(), map_id, cr_id);
        return;
    }

    Critter* cr = map->GetCritter(cr_id);

    if (cr == nullptr) {
        WriteLog("Process_Move: critter not found, player '{}', map '{}' ({}), cr_id {}", player->GetName(), map->GetName(), map_id, cr_id);
        return;
    }

    if (speed == 0) {
        WriteLog("Process_Move: zero speed, player '{}', critter '{}' ({}) on map '{}'", player->GetName(), cr->GetName(), cr_id, map->GetName());
        player->Send_Moving(cr);
        return;
    }

    if (cr->GetIsAttached()) {
        WriteLog("Process_Move: critter is attached, player '{}', critter '{}' ({}) on map '{}'", player->GetName(), cr->GetName(), cr_id, map->GetName());
        player->Send_Attachments(cr);
        player->Send_Moving(cr);
        return;
    }

    int32_t corrected_speed = speed;

    if (OnPlayerMoveCritter.Fire(player, cr, corrected_speed) == EventResult::StopChain) {
        WriteLog("Process_Move: move rejected by script, player '{}', critter '{}' ({}) on map '{}', speed {}", player->GetName(), cr->GetName(), cr_id, map->GetName(), speed);
        player->Send_Moving(cr);
        return;
    }

    // Fix async errors
    const auto cr_hex = cr->GetHex();

    if (cr_hex != start_hex) {
        const auto find_result = MapMngr.FindPath(map, cr, cr_hex, start_hex, cr->GetMultihex(), 0);

        if (find_result.Result != FindPathOutput::ResultType::Ok) {
            WriteLog("Process_Move: async fix pathfinding failed, player '{}', critter '{}' ({}) on map '{}', server_hex ({},{}), client_hex ({},{})", player->GetName(), cr->GetName(), cr_id, map->GetName(), cr_hex.x, cr_hex.y, start_hex.x, start_hex.y);
            player->Send_Moving(cr);
            return;
        }

        // Insert part of path to beginning of whole path
        for (auto& control_step : control_steps) {
            control_step += numeric_cast<uint16_t>(find_result.Steps.size());
        }

        control_steps.insert(control_steps.begin(), find_result.ControlSteps.begin(), find_result.ControlSteps.end());
        steps.insert(steps.begin(), find_result.Steps.begin(), find_result.Steps.end());
    }

    // Validate path: walk each step and check hex movability
    bool path_truncated = false;

    {
        const auto multihex = cr->GetMultihex();
        auto validate_hex = cr_hex;
        size_t valid_step_count = 0;

        const auto check_hex = [map](mpos h) -> HexBlockResult { return map->IsHexMovable(h) ? HexBlockResult::Passable : HexBlockResult::Blocked; };

        for (size_t i = 0; i < steps.size(); i++) {
            auto next_hex = validate_hex;

            if (!GeometryHelper::MoveHexByDir(next_hex, steps[i], map->GetSize())) {
                break;
            }

            const auto block = PathFinding::CheckHexWithMultihex(next_hex, steps[i], multihex, map->GetSize(), check_hex);

            if (block == HexBlockResult::Blocked) {
                break;
            }

            validate_hex = next_hex;
            valid_step_count++;
        }

        if (valid_step_count == 0) {
            WriteLog("Process_Move: all steps blocked, player '{}', critter '{}' ({}) on map '{}', hex ({},{}), multihex {}, total_steps {}", player->GetName(), cr->GetName(), cr_id, map->GetName(), cr_hex.x, cr_hex.y, multihex, steps.size());
            player->Send_Moving(cr);
            return;
        }

        if (valid_step_count < steps.size()) {
            steps.resize(valid_step_count);

            // Truncate control_steps to match shortened path
            while (!control_steps.empty() && control_steps.back() > valid_step_count) {
                control_steps.pop_back();
            }

            if (control_steps.empty() || control_steps.back() != numeric_cast<uint16_t>(valid_step_count)) {
                control_steps.push_back(numeric_cast<uint16_t>(valid_step_count));
            }

            path_truncated = true;
        }
    }

    if (end_hex_offset.x < -GameSettings::MAP_HEX_WIDTH / 2 || end_hex_offset.x > GameSettings::MAP_HEX_WIDTH / 2) {
        WriteLog("Process_Move: end_hex_offset.x out of range, player '{}', critter '{}' ({}) on map '{}', offset ({},{})", player->GetName(), cr->GetName(), cr_id, map->GetName(), end_hex_offset.x, end_hex_offset.y);
    }
    if (end_hex_offset.y < -GameSettings::MAP_HEX_HEIGHT / 2 || end_hex_offset.y > GameSettings::MAP_HEX_HEIGHT / 2) {
        WriteLog("Process_Move: end_hex_offset.y out of range, player '{}', critter '{}' ({}) on map '{}', offset ({},{})", player->GetName(), cr->GetName(), cr_id, map->GetName(), end_hex_offset.x, end_hex_offset.y);
    }

    const auto clamped_end_hex_ox = std::clamp(end_hex_offset.x, numeric_cast<int16_t>(-GameSettings::MAP_HEX_WIDTH / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_WIDTH / 2));
    const auto clamped_end_hex_oy = std::clamp(end_hex_offset.y, numeric_cast<int16_t>(-GameSettings::MAP_HEX_HEIGHT / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_HEIGHT / 2));

    StartCritterMoving(cr, numeric_cast<uint16_t>(corrected_speed), steps, control_steps, {clamped_end_hex_ox, clamped_end_hex_oy}, player);

    if (path_truncated) {
        player->Send_Moving(cr);
    }
    if (corrected_speed != numeric_cast<int32_t>(speed)) {
        player->Send_MovingSpeed(cr);
    }
}

void ServerEngine::Process_StopMove(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto map_id = in_buf->Read<ident_t>();
    const auto cr_id = in_buf->Read<ident_t>();

    // Todo: validate stop position and place critter in it
    [[maybe_unused]] const auto start_hex = in_buf->Read<mpos>();
    [[maybe_unused]] const auto hex_offset = in_buf->Read<ipos16>();
    [[maybe_unused]] const auto dir_angle = in_buf->Read<mdir>();

    in_buf.Unlock();

    auto* map = EntityMngr.GetMap(map_id);

    if (map == nullptr) {
        WriteLog("Process_StopMove: map not found, player '{}', map_id {}, cr_id {}", player->GetName(), map_id, cr_id);
        return;
    }

    Critter* cr = map->GetCritter(cr_id);

    if (cr == nullptr) {
        WriteLog("Process_StopMove: critter not found, player '{}', map '{}' ({}), cr_id {}", player->GetName(), map->GetName(), map_id, cr_id);
        return;
    }

    if (cr->GetIsAttached()) {
        WriteLog("Process_StopMove: critter is attached, player '{}', critter '{}' ({}) on map '{}'", player->GetName(), cr->GetName(), cr_id, map->GetName());
        player->Send_Attachments(cr);
        player->Send_Moving(cr);
        return;
    }

    int32_t zero_speed = 0;

    if (OnPlayerMoveCritter.Fire(player, cr, zero_speed) == EventResult::StopChain) {
        WriteLog("Process_StopMove: stop rejected by script, player '{}', critter '{}' ({}) on map '{}'", player->GetName(), cr->GetName(), cr_id, map->GetName());
        player->Send_Moving(cr);
        return;
    }

    StopCritterMoving(cr, MovingState::Stopped, [&] { cr->SendAndBroadcast(player, [&](Critter* cr2) { cr2->Send_Moving(cr); }); });
}

void ServerEngine::Process_Dir(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto map_id = in_buf->Read<ident_t>();
    const auto cr_id = in_buf->Read<ident_t>();
    const auto dir = in_buf->Read<mdir>();

    in_buf.Unlock();

    auto* map = EntityMngr.GetMap(map_id);

    if (map == nullptr) {
        WriteLog("Process_Dir: map not found, player '{}', map_id {}, cr_id {}", player->GetName(), map_id, cr_id);
        return;
    }

    auto* cr = map->GetCritter(cr_id);

    if (cr == nullptr) {
        WriteLog("Process_Dir: critter not found, player '{}', map '{}' ({}), cr_id {}", player->GetName(), map->GetName(), map_id, cr_id);
        return;
    }

    mdir checked_dir = dir;

    if (OnPlayerDirCritter.Fire(player, cr, checked_dir) == EventResult::StopChain) {
        WriteLog("Process_Dir: dir rejected by script, player '{}', critter '{}' ({}) on map '{}', angle {}", player->GetName(), cr->GetName(), cr_id, map->GetName(), dir.angle());
        player->Send_Dir(cr);
        return;
    }

    cr->ChangeDir(checked_dir);
    cr->SendAndBroadcast(player, [cr](Critter* cr2) { cr2->Send_Dir(cr); });
}

void ServerEngine::Process_Property(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    Critter* cr = player->GetControlledCritter();

    const auto data_size = in_buf->Read<uint32_t>();

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

    const auto property_index = in_buf->Read<uint16_t>();

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

    if (prop == nullptr) {
        throw GenericException("Unknown property index", player->GetName(), type, property_index);
    }
    if (entity == nullptr) {
        throw GenericException("Entity not found for property", player->GetName(), type, property_index, cr_id, item_id);
    }

    if (prop->IsDisabled()) {
        throw GenericException("Property is disabled", prop->GetName(), player->GetName(), type, entity->GetName());
    }
    if (prop->IsVirtual()) {
        throw GenericException("Property is virtual", prop->GetName(), player->GetName(), type, entity->GetName());
    }

    if (is_public && !prop->IsPublicSync()) {
        throw GenericException("Property is not public sync", prop->GetName(), player->GetName(), type, entity->GetName());
    }
    if (!is_public && !prop->IsSynced()) {
        throw GenericException("Property is not synced", prop->GetName(), player->GetName(), type, entity->GetName());
    }
    if (!prop->IsModifiableByClient() && !prop->IsModifiableByAnyClient()) {
        throw GenericException("Property is not modifiable by client", prop->GetName(), player->GetName(), type, entity->GetName());
    }
    if (is_public && !prop->IsModifiableByAnyClient()) {
        throw GenericException("Property is public but not modifiable by any client", prop->GetName(), player->GetName(), type, entity->GetName());
    }

    try {
        ValidateInboundPropertyData(prop, {static_cast<const uint8_t*>(prop_data.GetPtr()), prop_data.GetSize()}, *this);
    }
    catch (const ClientDataValidationException& ex) {
        WriteLog("Process_Property: property '{}' validation failed ({}), player '{}', type {}, entity '{}'", prop->GetName(), ex.what(), player->GetName(), type, entity->GetName());
        throw;
    }

    {
        player->SetIgnoreSendEntityProperty(entity, prop);
        auto revert_send_ignore = scope_exit([player]() noexcept { player->SetIgnoreSendEntityProperty(nullptr, nullptr); });

        entity->SetValueFromData(prop, prop_data);
    }
}

void ServerEngine::OnSaveEntityValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    const auto* server_entity = dynamic_cast<ServerEntity*>(entity);

    ident_t entry_id;

    if (server_entity != nullptr) {
        if (!server_entity->IsPersistent()) {
            return;
        }

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
        doc.Emplace("Time", numeric_cast<int64_t>(time.milliseconds()));
        doc.Emplace("EntityType", string(entity->GetTypeName()));
        doc.Emplace("EntityId", numeric_cast<int64_t>(entry_id.underlying_value()));
        doc.Emplace("Property", prop->GetName());
        doc.Emplace("Value", std::move(value));

        DbStorage.Insert(HistoryCollectionName, history_id, doc);
    }
}

void ServerEngine::OnSendGlobalValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(entity);

    if (prop->IsPublicSync()) {
        for (auto& player : EntityMngr.GetPlayers() | std::views::values) {
            player->Send_Property(NetProperty::Game, prop, this);
        }
    }
}

void ServerEngine::OnSendPlayerValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* player = dynamic_cast<Player*>(entity);
    FO_RUNTIME_ASSERT(player);

    player->Send_Property(NetProperty::Player, prop, player);
}

void ServerEngine::OnSendCritterValue(Entity* entity, const Property* prop)
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

void ServerEngine::OnSendItemValue(Entity* entity, const Property* prop)
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

void ServerEngine::OnSendMapValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (prop->IsPublicSync()) {
        auto* map = dynamic_cast<Map*>(entity);
        FO_RUNTIME_ASSERT(map);

        map->SendProperty(NetProperty::Map, prop, map);
    }
}

void ServerEngine::OnSendLocationValue(Entity* entity, const Property* prop)
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

void ServerEngine::OnSendCustomEntityValue(Entity* entity, const Property* prop)
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

void ServerEngine::OnSetCritterLookDistance(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    auto* cr = dynamic_cast<Critter*>(entity);
    FO_RUNTIME_ASSERT(cr);

    MapMngr.ProcessVisibleCritters(cr);

    if (prop == cr->GetPropertyLookDistance()) {
        MapMngr.ProcessVisibleItems(cr);
    }
}

void ServerEngine::OnSetItemCount(Entity* entity, const Property* prop, const void* new_value)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
    ignore_unused(prop);

    const auto* item = dynamic_cast<Item*>(entity);
    const auto new_count = *cast_from_void<const uint32_t*>(new_value);
    FO_RUNTIME_ASSERT(item);

    if (!item->GetStackable() && new_count != 1) {
        throw GenericException("Trying to change count of not stackable item");
    }
    else if (new_count <= 0) {
        throw GenericException("Item count can't be zero or negative", new_count);
    }
}

void ServerEngine::OnSetItemHidden(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(prop);

    auto* item = dynamic_cast<Item*>(entity);
    FO_RUNTIME_ASSERT(item);

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto* map = EntityMngr.GetMap(item->GetMapId());
        FO_RUNTIME_ASSERT(map);
        map->ChangeViewItem(item);
    }
    else if (item->GetOwnership() == ItemOwnership::CritterInventory) {
        auto* cr = EntityMngr.GetCritter(item->GetCritterId());
        FO_RUNTIME_ASSERT(cr);

        if (item->GetHidden()) {
            cr->Send_ChosenRemoveItem(item);
        }
        else {
            cr->Send_ChosenAddItem(item);
        }

        cr->SendAndBroadcast_MoveItem(item, CritterAction::Refresh, CritterItemSlot::Inventory);
    }
}

void ServerEngine::OnSetItemRecacheHex(Entity* entity, const Property* prop)
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

void ServerEngine::OnSetItemMultihexLines(Entity* entity, const Property* prop)
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

void ServerEngine::ProcessCritterMoving(Critter* cr)
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
            const auto reason = cr->GetIsAttached() ? MovingState::Attached : (cr->IsAlive() ? MovingState::GenericError : MovingState::NotAlive);
            StopCritterMoving(cr, reason);
        }
    }
}

void ServerEngine::ProcessCritterMovingBySteps(Critter* cr, Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(cr->IsMoving());
    auto* moving = cr->GetMoving();
    FO_RUNTIME_ASSERT(moving);
    moving->ValidateRuntimeState();

    const auto validate_moving = [this, cr, expected_uid = cr->GetMovingUid(), expected_map_id = cr->GetMapId(), map](mpos expected_hex) -> bool {
        const auto validate_moving_inner = [&]() -> bool {
            if (cr->IsDestroyed() || map->IsDestroyed()) {
                return false;
            }
            if (cr->GetMovingUid() != expected_uid) {
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
            if (!cr->IsDestroyed() && cr->GetMovingUid() == expected_uid) {
                StopCritterMoving(cr, MovingState::Stopped);
            }

            return false;
        }
        else {
            return true;
        }
    };

    const auto current_time = GameTime.GetFrameTime();
    const auto max_hex_updates = moving->GetSteps().size() + 1;

    for (size_t i = 0; i < max_hex_updates; i++) {
        const auto old_hex = cr->GetHex();

        moving->UpdateCurrentTimeToNextHex(current_time, old_hex);

        auto progress = moving->EvaluateProgress();
        const auto target_hex = progress.Hex;

        if (old_hex != target_hex) {
            const auto dir = mdir(iround<int32_t>(GeometryHelper::GetDirAngle(old_hex, target_hex)));
            const auto multihex = cr->GetMultihex();

            const auto check_hex = [map](mpos h) -> HexBlockResult { return map->IsHexMovable(h) ? HexBlockResult::Passable : HexBlockResult::Blocked; };

            if (PathFinding::CheckHexWithMultihex(target_hex, dir, multihex, map->GetSize(), check_hex) != HexBlockResult::Blocked) {
                map->RemoveCritterFromField(cr);
                cr->SetHex(target_hex);
                map->AddCritterToField(cr);

                FO_RUNTIME_ASSERT(!cr->IsDestroyed());

                map->VerifyTrigger(cr, old_hex, target_hex, dir);

                if (!validate_moving(target_hex)) {
                    return;
                }

                MapMngr.ProcessVisibleCritters(cr);

                if (!validate_moving(target_hex)) {
                    return;
                }

                MapMngr.ProcessVisibleItems(cr);

                if (!validate_moving(target_hex)) {
                    return;
                }
            }
            else {
                moving->SetBlockHexes(old_hex, target_hex);
                StopCritterMoving(cr, MovingState::HexBusy);
                return;
            }

            OnCritterMoved.Fire(cr, old_hex);

            if (!validate_moving(target_hex)) {
                return;
            }

            MapMngr.ProcessVisibleCritters(cr);

            if (!validate_moving(target_hex)) {
                return;
            }

            MapMngr.ProcessVisibleItems(cr);

            if (!validate_moving(target_hex)) {
                return;
            }
        }

        const auto cr_hex = cr->GetHex();
        const auto moved = cr_hex != old_hex;

        if (cr_hex != progress.Hex) {
            progress = moving->EvaluateProgress(cr_hex);
        }

        if (moved || cr->GetHexOffset() != progress.HexOffset) {
            cr->SetHexOffset(progress.HexOffset);
        }

        cr->SetDir(progress.Dir);

        if (cr->HasAttachedCritters()) {
            cr->MoveAttachedCritters();

            if (!validate_moving(cr_hex)) {
                return;
            }
        }

        if (progress.Completed) {
            const bool incorrect_final_position = cr->GetHex() != moving->GetEndHex();
            StopCritterMoving(cr, MovingState::Success, incorrect_final_position ? nullptr : [] { });
            return;
        }

        if (!moved || moving->GetElapsedTime() >= moving->GetRuntimeElapsedTime(current_time)) {
            break;
        }
    }
}

void ServerEngine::StartCritterMoving(Critter* cr, refcount_ptr<MovingContext> moving, const Player* initiator)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (cr->GetIsAttached()) {
        cr->DetachFromCritter();
    }

    const bool was_moving = cr->IsMoving();

    cr->StopMoving(MovingState::Stopped);
    cr->SetMoving(std::move(moving));
    cr->GetMoving()->ValidateRuntimeState();
    cr->SetMovingSpeed(cr->GetMoving()->GetSpeed());

    cr->SendAndBroadcast(initiator, [cr](Critter* cr2) { cr2->Send_Moving(cr); });

    OnCritterStartMoving.Fire(cr, was_moving);
}

void ServerEngine::StartCritterMoving(Critter* cr, uint16_t speed, const vector<mdir>& steps, const vector<uint16_t>& control_steps, ipos16 end_hex_offset, const Player* initiator)
{
    FO_STACK_TRACE_ENTRY();

    const auto* map = EntityMngr.GetMap(cr->GetMapId());
    FO_RUNTIME_ASSERT(map);

    const auto start_hex = cr->GetHex();

    StartCritterMoving(cr, SafeAlloc::MakeRefCounted<MovingContext>(map->GetSize(), speed, steps, control_steps, GameTime.GetFrameTime(), timespan {}, start_hex, cr->GetHexOffset(), end_hex_offset), initiator);
}

void ServerEngine::StopCritterMoving(Critter* cr, MovingState reason, function<void()> customSend)
{
    FO_STACK_TRACE_ENTRY();

    if (!cr->IsMoving()) {
        return;
    }

    cr->StopMoving(reason);

    if (customSend) {
        customSend();
    }
    else {
        cr->SendAndBroadcast_Moving();
    }

    OnCritterStopMoving.Fire(cr);
}

void ServerEngine::ChangeCritterMovingSpeed(Critter* cr, uint16_t speed)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (!cr->IsMoving()) {
        return;
    }
    if (cr->GetMoving()->GetSpeed() == speed) {
        return;
    }

    if (speed == 0) {
        StopCritterMoving(cr, MovingState::Stopped);
        return;
    }

    const auto cur_time = GameTime.GetFrameTime();
    cr->GetMoving()->ChangeSpeed(speed, cur_time);
    cr->SetMovingSpeed(speed);

    cr->SendAndBroadcast(nullptr, [cr](Critter* cr2) { cr2->Send_MovingSpeed(cr); });

    OnCritterStartMoving.Fire(cr, true);
}

void ServerEngine::Process_RemoteCall(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto remote_call_name = in_buf->Read<hstring>(Hashes);
    const auto remote_call_data_size = in_buf->Read<int32_t>();

    if (remote_call_data_size < 0) {
        throw GenericException("Invalid data size", remote_call_data_size);
    }

    vector<uint8_t> remote_call_data;
    remote_call_data.resize(remote_call_data_size);
    in_buf->Pop(remote_call_data.data(), remote_call_data_size);

    in_buf.Unlock();

    const auto& remote_calls = GetInboundRemoteCalls();
    const auto remote_call_it = remote_calls.find(remote_call_name);

    if (remote_call_it == remote_calls.end()) {
        throw GenericException("Invalid remote call", remote_call_name);
    }

    ValidateInboundRemoteCallData(remote_call_it->second, remote_call_data, *this);
    HandleInboundRemoteCall(remote_call_name, player, remote_call_data);
}

auto ServerEngine::CreateItemOnHex(Map* map, mpos hex, hstring pid, int32_t count, Properties* props) -> Item*
{
    FO_STACK_TRACE_ENTRY();

    if (count <= 0) {
        throw GenericException("Invalid items cound");
    }

    const auto* proto = GetProtoItem(pid);

    if (proto == nullptr) {
        throw GenericException("Item proto not found", pid);
    }

    const auto add_item = [&]() -> Item* {
        auto* item = ItemMngr.CreateItem(pid, proto->GetStackable() ? count : 1, props);
        map->AddItem(item, hex, nullptr);
        return item;
    };

    auto* item = add_item();

    // Non-stacked items
    if (item != nullptr && !proto->GetStackable() && count > 1) {
        const auto fixed_count = std::min(count, Settings.MaxAddUnstackableItems);

        for (int32_t i = 0; i < fixed_count; i++) {
            if (add_item() == nullptr) {
                break;
            }
        }
    }

    return item;
}

auto ServerEngine::MakePlayerId(string_view player_name) const -> ident_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!player_name.empty());
    const auto hash_value = static_cast<uint32_t>(hashing_ex::hash(reinterpret_cast<const uint8_t*>(player_name.data()), player_name.length()));
    FO_RUNTIME_ASSERT(hash_value);
    return ident_t {(1u << 31u) | hash_value};
}
FO_END_NAMESPACE
