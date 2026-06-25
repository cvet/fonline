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
#include "EntitySync.h"
#include "Managed/ManagedScripting.h"
#include "MetadataRegistration.h"
#include "Movement.h"
#include "PropertiesSerializator.h"

FO_BEGIN_NAMESPACE

extern void ServerInitHook(ServerEngine*);

// Per-thread SyncContext bound to the lifetime of an external `ServerEngine::Lock` call. Tests
// (and other off-thread callers) Lock to pause the main worker and then mutate engine state on
// their own thread; that thread needs a SyncContext active for the engine-wide invariant. Lock
// constructs+activates one, Unlock releases+deactivates it.
thread_local unique_ptr<SyncContext> ExternalLockSyncCtx {};

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
    WriteLog("Updater version: {}", FO_UPDATER_VERSION);
    WriteLog("Compatibility version: {}", settings.CompatibilityVersion);

    _starter.SetExceptionHandler([this](const std::exception& ex) FO_DEFERRED {
        ignore_unused(ex);

        _startingError = true;

        // Clear jobs
        return true;
    });

    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitHealthFileJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitScriptSystemJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitNetworkingJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitStorageJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitMetadataJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitLanguageJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitMapsJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitClientPacksJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitGameLogicJob(); }));
    _starter.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return InitDoneJob(); }));
}

ServerEngine::~ServerEngine()
{
    FO_STACK_TRACE_ENTRY();
}

auto ServerEngine::FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult
{
    FO_STACK_TRACE_ENTRY();

    if (callbacks.empty()) {
        return EventResult::ContinueChain;
    }

    // Engine-wide invariant: a primary SyncContext is always active when an event fires.
    FO_STRONG_ASSERT(GetCurrentSyncContext(), "Server event fired without active sync context");

    bool had_exception = false;

    // Iterate a copy — callbacks vector may be changed/invalidated during cycle work.
    for (const auto& cb : copy(callbacks)) {
        EventResult result = EventResult::ContinueChain;

        try {
            // Wrap each callback in its own nested SyncContext on top of the dispatcher's primary
            // cover. Inner `Sync::Lock(...)` only mutates the nested layer; the primary's locks
            // (the event's entity args) survive across the chain.
            ScopedSyncContext nested;

            result = cb.Callback(call);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            had_exception = true;

            if (cb.HasExplicitResult) {
                return EventResult::StopChain;
            }
        }

        if (result == EventResult::StopChain) {
            return EventResult::StopChain;
        }
    }

    return had_exception ? EventResult::StopChain : EventResult::ContinueChain;
}

auto ServerEngine::WrapJobWithSync(WorkThread::Job body) -> WorkThread::Job
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return [body_ = std::move(body)]() FO_DEFERRED -> std::optional<timespan> {
        ScopedSyncContext sync_ctx;

        return body_();
    };
}

void ServerEngine::CountServerStatsJob() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _completedServerStatsJobs.fetch_add(1, std::memory_order_relaxed);
}

void ServerEngine::LockForPropertyAccess() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _entityLock->Acquire(NextSyncTicket());
}

void ServerEngine::UnlockForPropertyAccess() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _entityLock->Release();
}

void ServerEngine::LockForPropertyAccessShared() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _entityLock->AcquireShared(NextSyncTicket());
}

void ServerEngine::UnlockForPropertyAccessShared() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _entityLock->ReleaseShared();
}

void ServerEngine::FlushExactSyncTime()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(GameTime.IsTimeSynchronized(), "Missing required game time is time synchronized");

    _persistedSyncTimeMark = GameTime.GetSynchronizedTime();
    const auto* prop = GetPropertySynchronizedTime();
    const auto value = AnyData::Value {numeric_cast<int64_t>(_persistedSyncTimeMark.milliseconds())};
    DbStorage.Update(GameCollectionName, ident_t {1}, prop->GetName(), value);
}

void ServerEngine::ScheduleDelayedCallback(timespan delay, function<void()> body)
{
    FO_STACK_TRACE_ENTRY();

    _workerPool->Submit(delay, [this, body = std::move(body)]() FO_DEFERRED -> std::optional<timespan> {
        auto complete_stats_job = scope_exit([this]() noexcept { CountServerStatsJob(); });

        body();
        return std::nullopt;
    });
}

auto ServerEngine::InitHealthFileJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    if (!Settings.WriteHealthFile) {
        return std::nullopt;
    }

    const auto exe_path = Platform::GetExePath();
    _healthFileName = strex("{}_Health.txt", exe_path ? strvex(exe_path.value()).extract_file_name().erase_file_extension() : string_view(FO_DEV_NAME));

    if (WriteHealthFile("Starting...")) {
        _mainWorker.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return HealthFileJob(); }));
    }
    else {
        WriteLog(LogType::Warning, "Can't write health file '{}'", _healthFileName);
    }

    return std::nullopt;
}

auto ServerEngine::HealthFileJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    if (_started && _healthWriter.GetJobsCount() == 0) {
        _healthWriter.AddJob([this, health_info = GetHealthInfo()]() FO_DEFERRED { return HealthFileWriteJob(health_info); });
    }

    return std::chrono::milliseconds {Settings.HealthFilePeriodMs};
}

auto ServerEngine::HealthFileWriteJob(const string& health_info) -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    string buf;
    buf.reserve(health_info.size() + 128);
    buf += strex("{} v{}\n\n", Settings.GameName, Settings.GameVersion);
    buf += health_info;
    WriteHealthFile(buf);

    return std::nullopt;
}

auto ServerEngine::WriteHealthFile(string_view text) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    std::ofstream health_file {std::filesystem::path {fs_make_path(_healthFileName)}, std::ios::binary | std::ios::trunc};

    if (!health_file) {
        return false;
    }

    if (!text.empty()) {
        health_file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    health_file.flush();
    return !!health_file;
}

auto ServerEngine::InitScriptSystemJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

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
#if FO_MANAGED_SCRIPTING
    InitManagedScripting(this, Resources);
#endif

    return std::nullopt;
}

auto ServerEngine::InitNetworkingJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Start networking");

    if (auto conn_server = NetworkServer::StartInterthreadServer(Settings, [this](shared_ptr<NetworkServerConnection> net_connection) FO_DEFERRED { OnNewConnection(std::move(net_connection)); })) {
        _connectionServers.emplace_back(std::move(conn_server));
    }

    if (Settings.DisableNetworking) {
        WriteLog("Skip remote networking startup");
        return std::nullopt;
    }

    if (Settings.EnableUdp) {
        if (auto conn_server = NetworkServer::StartUdpSocketsServer(Settings, [this](shared_ptr<NetworkServerConnection> net_connection) FO_DEFERRED { OnNewConnection(std::move(net_connection)); })) {
            _connectionServers.emplace_back(std::move(conn_server));
        }
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
}

auto ServerEngine::InitStorageJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    DataBaseCollectionSchemas collection_schemas;
    collection_schemas.reserve(3 + GetEntityTypes().size() + Settings.CustomCollections.size());

    unordered_map<hstring, DataBaseKeyType> registered_collection_types {};
    registered_collection_types.reserve(2 + GetEntityTypes().size() + Settings.CustomCollections.size());

    const auto register_collection = [&collection_schemas, &registered_collection_types](hstring collection_name, DataBaseKeyType key_type) {
        FO_VERIFY_AND_THROW(!collection_name.as_str().empty(), "Database collection registration received an empty collection name", key_type, registered_collection_types.size());

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
    register_collection(HashReportsCollectionName, DataBaseKeyType::String);

    for (const auto& type_desc : GetEntityTypes() | std::views::values) {
        register_collection(type_desc.PropRegistrator->GetTypeNamePlural(), DataBaseKeyType::IntId);
    }
    for (const auto& entry : Settings.CustomCollections) {
        register_custom_collection(entry);
    }

    DbStorage = ConnectToDataBase(Settings, Settings.DbStorage, collection_schemas, [] {
        FO_VERIFY_AND_THROW(App, "Missing required app");
        App->RequestQuit(false);
    });

    return std::nullopt;
}

auto ServerEngine::InitMetadataJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Setup engine");

    // Properties that saving to database
    const auto* sync_time_prop = GetPropertySynchronizedTime();
    sync_time_prop->AddPostSetter([this](Entity* entity, const Property* prop_) FO_DEFERRED { OnSaveSynchronizedTime(entity, prop_); });

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
            if (prop == sync_time_prop) {
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
        set_setter(GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME), Item::MultihexLines_RegIndex, [this](Entity* entity, const Property* prop, PropertyRawData& /*data*/) FO_DEFERRED { OnSetItemMultihexLines(entity, prop); });
    }

    return std::nullopt;
}

auto ServerEngine::InitLanguageJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Load language data");

    _defaultLang = TextPack {Hashes};
    _defaultLang.LoadFromResources(Resources, Settings.Language);

    return std::nullopt;
}

auto ServerEngine::InitMapsJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Load maps data");

    MapMngr.LoadFromResources();

    return std::nullopt;
}

auto ServerEngine::InitClientPacksJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    if (IsPackaged()) {
        WriteLog("Initialize updater backend with client resources using {} storage", Settings.UpdateFilesInMemory ? "memory" : "disk");

        auto updater_backend = SafeAlloc::MakeUnique<UpdaterBackend>();
        updater_backend->LoadFromClientResources(Settings);
        _updaterBackend = std::move(updater_backend);
    }
    else {
        WriteLog("Skip updater backend initialization in unpackaged mode");
    }

    return std::nullopt;
}

auto ServerEngine::InitGameLogicJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

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

        // Worker pool
        const auto worker_threads = Settings.WorkerThreads != 0 ? Settings.WorkerThreads : 0;
        _workerPool = SafeAlloc::MakeUnique<WorkerPool>("ServerPool", worker_threads, _shutdownInProgress, /*start_paused*/ true);

        TimeEventManager::DispatcherHooks hooks;
        hooks.Schedule = [this](refcount_ptr<Entity> entity, uint32_t event_id, timespan delay) { OnTimeEventSchedule(std::move(entity), event_id, delay); };
        hooks.Cancel = [this](uint32_t event_id) { OnTimeEventCancel(event_id); };
        TimeEventMngr.SetDispatcherHooks(std::move(hooks));

        // Scripting
        WriteLog("Init script modules");

        ServerInitHook(this);
        InitModules();

        if (OnInit.Fire() == EventResult::StopChain) {
            throw ServerInitException("Initialization script failed");
        }

        LoadReportedHashes();

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
}

auto ServerEngine::InitDoneJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_started, "Started is already set");
    FO_VERIFY_AND_THROW(_workerPool, "Missing required worker pool");

    WriteLog("Start server complete!");

    const nanotime stats_begin = nanotime::now();
    const uint64_t completed_jobs = GetCompletedServerJobsCount();

    _stats.ServerStartTime = stats_begin;
    _stats.JobCounterBegin = stats_begin;
    _stats.JobCounterBeginTotal = completed_jobs;
    _stats.JobsTotal = completed_jobs;
    _stats.JobTimeStamps.clear();
    _stats.JobTimeStamps.emplace_back(stats_begin, completed_jobs);

    _mainWorker.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return SyncPointJob(); }));
    _mainWorker.AddJob(WrapJobWithSync([this]() FO_DEFERRED { return FrameTimeJob(); }));

    _workerPool->Resume();

    // Set started flag AFTER workerPool is resumed and mainWorker has jobs queued so
    // external observers (tests, network OnNewConnection) only see a fully-running server.
    _started = true;

    return std::nullopt;
}

void ServerEngine::OnTimeEventSchedule(refcount_ptr<Entity> entity, uint32_t event_id, timespan delay)
{
    FO_STACK_TRACE_ENTRY();

    const auto key = WorkerJobKey {.Type = WorkerJobType::TimeEvent, .Id = static_cast<size_t>(event_id)};

    _workerPool->Submit(key, delay, [this, entity_hold = std::move(entity), event_id]() mutable -> std::optional<timespan> { return TimeEventJob(entity_hold.get(), event_id); });
}

auto ServerEngine::TimeEventJob(Entity* entity, uint32_t event_id) -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    if (!entity->IsGlobal()) {
        auto* ctx = GetCurrentSyncContext();
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
        ctx->SyncEntity(dynamic_cast<ServerEntity*>(entity));
    }

    if (entity->IsDestroyed()) {
        return std::nullopt;
    }

    return TimeEventMngr.FireAndAdvance(entity, event_id);
}

void ServerEngine::OnTimeEventCancel(uint32_t event_id)
{
    FO_STACK_TRACE_ENTRY();

    const auto key = WorkerJobKey {.Type = WorkerJobType::TimeEvent, .Id = static_cast<size_t>(event_id)};
    _workerPool->Cancel(key);
}

auto ServerEngine::SyncPointJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    SyncPoint();

    // Sample server stats at the sync tick: online counts, uptime, job throughput and CPU load.
    {
        scoped_lock locker {_unloginedPlayersLocker};

        _stats.CurOnline = _unloginedPlayers.size() + EntityMngr.GetPlayersCount();
        _stats.MaxOnline = std::max(_stats.MaxOnline, _stats.CurOnline);
    }

    _stats.RejectedConnections = _rejectedConnections.load(std::memory_order_relaxed);
    _stats.RejectedByRate = _rejectedByRate.load(std::memory_order_relaxed);

    const auto cur_time = nanotime::now();
    _stats.Uptime = cur_time - _stats.ServerStartTime;
    UpdateJobStats(cur_time);
    UpdateCpuStats(cur_time);

#if FO_TRACY
    TracyPlot("Server jobs per second", numeric_cast<int64_t>(_stats.JobsPerSecond));
#endif

    return std::chrono::milliseconds {Settings.SyncPeriodMs};
}

auto ServerEngine::FrameTimeJob() -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    FrameAdvance();

    return std::chrono::nanoseconds {Settings.FrameTimePeriodNs};
}

void ServerEngine::OnPlayerConnected(Player* unlogined_player)
{
    FO_STACK_TRACE_ENTRY();

    const auto key = WorkerJobKey {.Type = WorkerJobType::UnloginedPlayer, .Id = static_cast<size_t>(reinterpret_cast<uintptr_t>(unlogined_player))};

    {
        ScopedSyncContext ctx;

        ctx.Sync(unlogined_player);
        unlogined_player->GetConnection()->SetDataArrivedCallback([this, key]() { _workerPool->Wake(key); });
    }

    _workerPool->Submit(key, [this, unlogined_player_ = refcount_ptr<Player>(unlogined_player)]() mutable -> std::optional<timespan> { return UnloginedPlayerJob(unlogined_player_.get()); });
}

auto ServerEngine::UnloginedPlayerJob(Player* unlogined_player) -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    auto complete_stats_job = scope_exit([this]() noexcept { CountServerStatsJob(); });

    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->SyncEntity(unlogined_player);

    if (unlogined_player->IsDestroyed()) {
        return std::nullopt;
    }

    auto* connection = unlogined_player->GetConnection();

    try {
        ProcessConnection(connection);
        ProcessUnloginedPlayer(unlogined_player);
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

    if (unlogined_player->IsDestroyed()) {
        return std::nullopt;
    }

    return std::chrono::milliseconds {Settings.ConnectionProcessPeriodMs};
}

void ServerEngine::OnPlayerLogined(Player* player, Player* unlogined_player)
{
    FO_STACK_TRACE_ENTRY();

    if (unlogined_player != nullptr) {
        const auto unlogined_key = WorkerJobKey {.Type = WorkerJobType::UnloginedPlayer, .Id = static_cast<size_t>(reinterpret_cast<uintptr_t>(unlogined_player))};
        _workerPool->Cancel(unlogined_key);
    }

    if (player != unlogined_player) {
        const auto same_addr_key = WorkerJobKey {.Type = WorkerJobType::UnloginedPlayer, .Id = static_cast<size_t>(reinterpret_cast<uintptr_t>(player))};
        _workerPool->Cancel(same_addr_key);
    }

    const auto key = WorkerJobKey {.Type = WorkerJobType::Player, .Id = static_cast<size_t>(player->GetId().underlying_value())};

    player->GetConnection()->SetDataArrivedCallback([this, key]() { _workerPool->Wake(key); });

    _workerPool->Submit(key, [this, player_ = refcount_ptr<Player>(player)]() mutable -> std::optional<timespan> { return PlayerJob(player_.get()); });
}

auto ServerEngine::PlayerJob(Player* player) -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    auto complete_stats_job = scope_exit([this]() noexcept { CountServerStatsJob(); });

    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->SyncEntity(player);

    if (player->IsDestroyed()) {
        return std::nullopt;
    }

    auto* connection = player->GetConnection();

    try {
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

    if (player->IsDestroyed()) {
        return std::nullopt;
    }

    return std::chrono::milliseconds {Settings.ConnectionProcessPeriodMs};
}

void ServerEngine::UpdateJobStats(nanotime cur_time)
{
    FO_STACK_TRACE_ENTRY();

    const uint64_t completed_jobs = GetCompletedServerJobsCount();

    _stats.JobsTotal = completed_jobs;

    if (!_stats.JobCounterBegin || _stats.JobTimeStamps.empty()) {
        _stats.JobCounterBegin = cur_time;
        _stats.JobCounterBeginTotal = completed_jobs;
        _stats.JobTimeStamps.clear();
        _stats.JobTimeStamps.emplace_back(cur_time, completed_jobs);
        return;
    }

    if (cur_time - _stats.JobCounterBegin < std::chrono::seconds {1}) {
        return;
    }

    // Sample once per second so the rolling-minute window stays small (the counters are
    // monotonic except for a one-time drop when the worker pool is destroyed at shutdown,
    // which the underflow guards absorb).
    _stats.JobsPerSecond = completed_jobs >= _stats.JobCounterBeginTotal ? completed_jobs - _stats.JobCounterBeginTotal : 0;
    _stats.JobCounterBegin = cur_time;
    _stats.JobCounterBeginTotal = completed_jobs;

    _stats.JobTimeStamps.emplace_back(cur_time, completed_jobs);

    while (_stats.JobTimeStamps.size() > 1 && cur_time - _stats.JobTimeStamps.front().first > std::chrono::minutes {1}) {
        _stats.JobTimeStamps.pop_front();
    }

    const uint64_t minute_begin_jobs = _stats.JobTimeStamps.front().second;
    _stats.JobsPerMinute = completed_jobs >= minute_begin_jobs ? completed_jobs - minute_begin_jobs : 0;
}

void ServerEngine::UpdateCpuStats(nanotime cur_time)
{
    FO_STACK_TRACE_ENTRY();

    constexpr timespan CPU_SAMPLE_INTERVAL = std::chrono::seconds {1};

    if (_stats.LastCpuUsageSampleTime && cur_time - _stats.LastCpuUsageSampleTime < CPU_SAMPLE_INTERVAL) {
        return;
    }

    Platform::CpuUsageSnapshot current_snapshot = Platform::GetCpuUsageSnapshot();

    if (_stats.LastCpuUsageSnapshot.has_value()) {
        const Platform::CpuUsageSnapshot& previous_snapshot = *_stats.LastCpuUsageSnapshot;
        const size_t core_count = std::min(previous_snapshot.Cores.size(), current_snapshot.Cores.size());

        _stats.CpuCoreLoads.clear();
        _stats.CpuCoreLoads.reserve(core_count);
        _stats.CpuUsageAvailable = core_count != 0;

        uint64_t previous_idle = 0;
        uint64_t current_idle = 0;
        uint64_t previous_total = 0;
        uint64_t current_total = 0;

        for (size_t i = 0; i < core_count; i++) {
            const Platform::CpuUsageCoreSnapshot& previous_core = previous_snapshot.Cores[i];
            const Platform::CpuUsageCoreSnapshot& current_core = current_snapshot.Cores[i];

            _stats.CpuCoreLoads.emplace_back(CalculateBusyCpuLoad(previous_core.IdleTime, current_core.IdleTime, previous_core.TotalTime, current_core.TotalTime));

            previous_idle += previous_core.IdleTime;
            current_idle += current_core.IdleTime;
            previous_total += previous_core.TotalTime;
            current_total += current_core.TotalTime;
        }

        _stats.CpuSystemLoad = CalculateBusyCpuLoad(previous_idle, current_idle, previous_total, current_total);

        if (_stats.LastCpuUsageSampleTime && current_snapshot.ProcessTimeNs >= previous_snapshot.ProcessTimeNs) {
            const timespan sample_duration = cur_time - _stats.LastCpuUsageSampleTime;
            const int64_t sample_duration_ns = sample_duration.nanoseconds();

            if (sample_duration_ns > 0) {
                const uint64_t process_delta_ns = current_snapshot.ProcessTimeNs - previous_snapshot.ProcessTimeNs;
                const float64_t process_core_load = std::min(numeric_cast<float64_t>(process_delta_ns) * 100.0 / numeric_cast<float64_t>(sample_duration_ns), numeric_cast<float64_t>(std::max<size_t>(core_count, 1)) * 100.0);

                _stats.CpuProcessCoreLoad = numeric_cast<float32_t>(process_core_load);
                _stats.CpuProcessLoad = numeric_cast<float32_t>(process_core_load / numeric_cast<float64_t>(std::max<size_t>(core_count, 1)));
            }
        }
    }

    _stats.LastCpuUsageSnapshot = std::move(current_snapshot);
    _stats.LastCpuUsageSampleTime = cur_time;
}

auto ServerEngine::CalculateBusyCpuLoad(uint64_t previous_idle, uint64_t current_idle, uint64_t previous_total, uint64_t current_total) noexcept -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    if (current_total <= previous_total) {
        return 0.0f;
    }

    const uint64_t total_delta = current_total - previous_total;
    const uint64_t idle_delta = current_idle >= previous_idle ? current_idle - previous_idle : 0;
    const uint64_t capped_idle_delta = std::min(idle_delta, total_delta);
    const uint64_t busy_delta = total_delta - capped_idle_delta;

    return numeric_cast<float32_t>(numeric_cast<float64_t>(busy_delta) * 100.0 / numeric_cast<float64_t>(total_delta));
}

auto ServerEngine::GetCompletedServerJobsCount() const -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    return _completedServerStatsJobs.load(std::memory_order_relaxed);
}

void ServerEngine::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Stop server");

    _shutdownInProgress.store(true, std::memory_order_release);

    // Shutdown runs synchronously on the caller's thread (test thread or main app), which has
    // no SyncContext active. Stand one up here so DestroyAllEntities, MapMngr.DestroyLocation
    // and friends can satisfy the engine-wide invariant that any entity touch happens under a
    // primary SyncContext.
    ScopedSyncContext shutdown_ctx;

    WriteLog("Shutdown stage: willFinishDispatcher");
    _willFinishDispatcher();
    WriteLog("Shutdown stage: starter.Clear");
    _starter.Clear();

    // Stop + join the network IO threads BEFORE tearing down the worker pool. A connection's
    // DataArrivedCallback (`[this, key]{ _workerPool->Wake(key); }`) and `OnNewConnection` ->
    // `_workerPool->Submit` run on the network thread; if the pool is reset first they dereference a
    // freed pool. `conn_server->Shutdown()` joins the IO thread, so no such callback can fire after.
    WriteLog("Shutdown stage: connection servers (count={})", _connectionServers.size());

    for (auto& conn_server : _connectionServers) {
        conn_server->Shutdown();
    }

    _connectionServers.clear();

    // Cancel every pending entity TimeEvent BEFORE draining the worker pool. Each TimeEvent
    // schedule goes through `OnTimeEventSchedule` which Submits a `_workerPool` job. If the test
    // run leaked critters with periodic time events (Ai / Health / Modifiers / etc.) and we go
    // straight to `_workerPool->Clear/WaitIdle`, the in-flight jobs keep re-scheduling because
    // `TimeEventMngr.FireAndAdvance` returns a non-empty delay. `ClearTimeEvents` calls the
    // `Cancel` hook (`_workerPool->Cancel(event_id)`) for every event-id, which both removes
    // pending jobs from the pool's queue and marks any currently-running job as
    // `_cancelOnFinish` so it won't re-enqueue when it returns.
    WriteLog("Shutdown stage: TimeEventMngr.ClearTimeEvents (entityCount={})", EntityMngr.GetEntitiesCount());
    TimeEventMngr.ClearTimeEvents();

    // `_workerPool` is created at the end of InitMetadataJob — after InitStorageJob connected the
    // database and game time was synchronized. Its presence is therefore the marker that startup
    // reached a running state. If a mandatory startup job aborted earlier (e.g. the database was
    // unreachable and InitStorageJob threw), the pool is null, `DbStorage` is unconnected and game
    // time is unsynchronized; Shutdown must still be safe to call on that half-initialized engine —
    // both when an admin quits it and when the host fails fast on the start error itself. Without
    // this guard the drain below dereferenced a null pool (SIGSEGV in WorkerPool::Clear, locking the
    // pool mutex through a null `this`) and the database flushes further down tripped their
    // connected/synchronized invariants. Networking is already stopped above, and with no world or
    // players the remaining teardown is a no-op.
    const bool reached_running_state = _workerPool != nullptr;

    if (reached_running_state) {
        // Fully drain and reset the worker pool BEFORE clearing _mainWorker. Only _mainWorker drives
        // the whole-world sync handshake: its SyncPointJob calls SyncPoint(), which releases any thread
        // parked in Lock(). If a pool job is blocked on that handshake, stopping _mainWorker first would
        // strand it and hang WaitIdle, so the main worker is always torn down last.
        WriteLog("Shutdown stage: TimeEventMngr.ClearDispatcherHooks");
        TimeEventMngr.ClearDispatcherHooks();
        WriteLog("Shutdown stage: workerPool.Clear");
        _workerPool->Clear();

        // Graceful drain: give in-flight worker jobs a grace window to finish on their own. A job parked
        // inside `EntityLock::Acquire` still counts as active, so `WaitIdle` blocks on it. If the drain
        // does not complete within `Server.ShutdownGraceMs`, wake every parked waiter via
        // `AbortPendingWaiters` — each wakes with `STATE_ABORTED` and throws `EntityLockWaitAbortedException`,
        // which unwinds its job (the `WorkerPool` run loop swallows it because shutdown is in progress).
        // Nothing converts the abort into a return value; the exception alone stops the work.
        WriteLog("Shutdown stage: workerPool.WaitIdle (graceMs={})", Settings.ShutdownGraceMs);

        if (!_workerPool->WaitIdle(std::chrono::milliseconds {Settings.ShutdownGraceMs})) {
            WriteLog("Shutdown stage: drain exceeded grace, AbortPendingWaiters on entity locks");

            const auto entities = EntityMngr.GetEntities();
            size_t aborted_locks = 0;

            for (const auto& entity : entities) {
                if (auto* lock = entity->GetEntityLock(); lock != nullptr) {
                    lock->AbortPendingWaiters();
                    aborted_locks++;
                }
            }

            WriteLog("Shutdown stage: AbortPendingWaiters complete (locks={})", aborted_locks);

            WriteLog("Shutdown stage: workerPool.WaitIdle (post-abort)");
            _workerPool->WaitIdle();
        }

        WriteLog("Shutdown stage: workerPool.reset");
        _workerPool.reset();
    }

    WriteLog("Shutdown stage: mainWorker.Clear");
    _mainWorker.Clear();
    WriteLog("Shutdown stage: healthWriter.Clear");
    _healthWriter.Clear();

    WriteLog("Shutdown stage: OnFinish.Fire");
    OnFinish.Fire();

    WriteLog("Shutdown stage: UnsubscribeAllEvents");
    UnsubscribeAllEvents();
    WriteLog("Shutdown stage: ClearAllTimeEvents");
    ClearAllTimeEvents();

    WriteLog("Shutdown stage: per-entity unsubscribe (count={})", EntityMngr.GetEntitiesCount());

    for (auto& entity : EntityMngr.GetEntities()) {
        entity->UnsubscribeAllEvents();
        entity->ClearAllTimeEvents();

        if (auto* item = dynamic_cast<Item*>(entity.get()); item != nullptr) {
            item->StaticScriptFunc = {};
            item->TriggerScriptFunc = {};
        }
    }

    WriteLog("Shutdown stage: TimeEventMngr.ClearTimeEvents (late, expected no-op)");
    TimeEventMngr.ClearTimeEvents();

    WriteLog("Shutdown stage: DestroyInnerEntities");
    EntityMngr.DestroyInnerEntities(this);
    WriteLog("Shutdown stage: DestroyAllEntities (count={})", EntityMngr.GetEntitiesCount());
    EntityMngr.DestroyAllEntities();
    WriteLog("Shutdown stage: ShutdownBackends");
    ShutdownBackends();

    // Persisting the exact entity-id / synchronized-time marks and committing pending writes all
    // require a connected database and synchronized game time, which only exist once startup reached
    // the running state (see `reached_running_state` above). After a failed start there is nothing to
    // persist and `DbStorage` is unconnected, so skip them.
    if (reached_running_state) {
        WriteLog("Shutdown stage: FlushExactEntityId");
        EntityMngr.FlushExactEntityId();
        WriteLog("Shutdown stage: FlushExactSyncTime");
        FlushExactSyncTime();

        WriteLog("Shutdown stage: DbStorage.WaitCommitChanges");
        DbStorage.WaitCommitChanges();
    }

    // Logined players
    WriteLog("Shutdown stage: disconnect logined players (count={})", EntityMngr.GetPlayersCount());

    for (auto& player : EntityMngr.GetPlayers()) {
        player->GetConnection()->HardDisconnect();
    }

    // Unlogined players
    WriteLog("Shutdown stage: disconnect unlogined players");

    {
        scoped_lock locker {_unloginedPlayersLocker};

        for (auto& player : _unloginedPlayers) {
            player->GetConnection()->HardDisconnect();
            player->MarkAsDestroyed();
        }

        _unloginedPlayers.clear();
    }

    // Done
    WriteLog("Server stopped!");
    _started = false;
    _didFinishDispatcher();

    FO_VERIFY_AND_THROW(GetRefCount() == 1, "Server engine still has external references after shutdown", GetRefCount());
}

auto ServerEngine::Lock(optional<timespan> max_wait_time) -> bool
{
    FO_STACK_TRACE_ENTRY();

    while (!_started) {
        std::this_thread::yield();
    }

    if (std::this_thread::get_id() != _mainWorker.GetThreadId()) {
        unique_lock locker {_syncLocker};

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

    // Now this thread is allowed to touch engine state — stand up a SyncContext to satisfy the
    // engine-wide invariant for any RegisterX / DestroyX / event-fire that may follow.
    FO_VERIFY_AND_THROW(!ExternalLockSyncCtx, "External lock sync ctx is already set");
    ExternalLockSyncCtx = SafeAlloc::MakeUnique<SyncContext>();
    ExternalLockSyncCtx->Activate();
    return true;
}

void ServerEngine::Unlock()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ExternalLockSyncCtx, "Missing required external lock sync ctx");
    ExternalLockSyncCtx->Release();
    ExternalLockSyncCtx->Deactivate();
    ExternalLockSyncCtx.reset();

    if (std::this_thread::get_id() != _mainWorker.GetThreadId()) {
        unique_lock locker {_syncLocker};

        FO_VERIFY_AND_THROW(_syncRequest > 0, "Sync request must be positive");

        _syncRequest--;

        locker.unlock();
        _syncRunSignal.notify_one();
    }
}

void ServerEngine::SyncPoint()
{
    FO_STACK_TRACE_ENTRY();

    unique_lock locker {_syncLocker};

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

    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;

    const auto info_row = [](const char* key, string_view value) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(key);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(value.data(), value.data() + value.size());
    };

    const auto begin_info_table = [](const char* id) -> bool {
        if (ImGui::BeginTable(id, 2, table_flags)) {
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
            info_row("Jobs per second", strex("{}", _stats.JobsPerSecond).str());
            info_row("Jobs per minute", strex("{}", _stats.JobsPerMinute).str());
            info_row("Total jobs", strex("{}", _stats.JobsTotal).str());
            info_row("Rejected connections", strex("{}", _stats.RejectedConnections).str());
            info_row("Rejected by rate", strex("{}", _stats.RejectedByRate).str());
            info_row("CPU load", _stats.CpuUsageAvailable ? strex("{:.1f}% / {:.1f}%", numeric_cast<float64_t>(_stats.CpuProcessLoad), numeric_cast<float64_t>(_stats.CpuSystemLoad)).str() : string("n/a"));
            info_row("DB requests per minute", strex("{}", DbStorage.GetDbRequestsPerMinute()).str());

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Performance details")) {
        const WorkThread::Diagnostics starter_diagnostics = _starter.GetDiagnostics();
        const WorkThread::Diagnostics main_worker_diagnostics = _mainWorker.GetDiagnostics();
        const WorkThread::Diagnostics health_writer_diagnostics = _healthWriter.GetDiagnostics();
        const bool has_worker_pool = _workerPool != nullptr;
        const WorkerPool::Diagnostics worker_pool_diagnostics = has_worker_pool ? _workerPool->GetDiagnostics() : WorkerPool::Diagnostics {};

        if (begin_info_table("##PerformanceDetailsTable")) {
            info_row("Jobs per second", strex("{}", _stats.JobsPerSecond).str());
            info_row("Jobs per minute", strex("{}", _stats.JobsPerMinute).str());
            info_row("Total jobs", strex("{}", _stats.JobsTotal).str());
            info_row("Starter completed jobs", strex("{}", starter_diagnostics.CompletedJobs).str());
            info_row("Starter queued jobs", strex("{}", starter_diagnostics.QueuedJobs).str());
            info_row("Starter active job", strex("{}", starter_diagnostics.JobActive).str());
            info_row("Main worker completed jobs", strex("{}", main_worker_diagnostics.CompletedJobs).str());
            info_row("Main worker queued jobs", strex("{}", main_worker_diagnostics.QueuedJobs).str());
            info_row("Main worker active job", strex("{}", main_worker_diagnostics.JobActive).str());
            info_row("Health writer completed jobs", strex("{}", health_writer_diagnostics.CompletedJobs).str());
            info_row("Health writer queued jobs", strex("{}", health_writer_diagnostics.QueuedJobs).str());
            info_row("Health writer active job", strex("{}", health_writer_diagnostics.JobActive).str());
            info_row("Worker pool threads", has_worker_pool ? strex("{}", worker_pool_diagnostics.ThreadCount).str() : string("n/a"));
            info_row("Worker pool completed jobs", has_worker_pool ? strex("{}", worker_pool_diagnostics.CompletedJobs).str() : string("n/a"));
            info_row("Worker pool scheduled jobs", has_worker_pool ? strex("{}", worker_pool_diagnostics.ScheduledJobs).str() : string("n/a"));
            info_row("Worker pool running jobs", has_worker_pool ? strex("{}", worker_pool_diagnostics.RunningJobs).str() : string("n/a"));
            info_row("Worker pool pending reruns", has_worker_pool ? strex("{}", worker_pool_diagnostics.PendingReruns).str() : string("n/a"));
            info_row("Worker pool queued keys", has_worker_pool ? strex("{}", worker_pool_diagnostics.QueuedKeys).str() : string("n/a"));
            info_row("Worker pool active workers", has_worker_pool ? strex("{}", worker_pool_diagnostics.ActiveWorkers).str() : string("n/a"));
            info_row("Worker pool paused", has_worker_pool ? strex("{}", worker_pool_diagnostics.Paused).str() : string("n/a"));
            info_row("CPU system load", _stats.CpuUsageAvailable ? strex("{:.1f}%", numeric_cast<float64_t>(_stats.CpuSystemLoad)).str() : string("n/a"));
            info_row("CPU process load", _stats.CpuUsageAvailable ? strex("{:.1f}%", numeric_cast<float64_t>(_stats.CpuProcessLoad)).str() : string("n/a"));
            info_row("CPU process core load", _stats.CpuUsageAvailable ? strex("{:.1f}%", numeric_cast<float64_t>(_stats.CpuProcessCoreLoad)).str() : string("n/a"));
            info_row("CPU core count", strex("{}", _stats.CpuCoreLoads.size()).str());

            ImGui::EndTable();
        }

        if (!_stats.CpuCoreLoads.empty() && ImGui::TreeNode("CPU cores")) {
            if (ImGui::BeginTable("##CpuCoresTable", 2, table_flags)) {
                ImGui::TableSetupColumn("Core", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("System load", ImGuiTableColumnFlags_WidthStretch);

                for (size_t i = 0; i < _stats.CpuCoreLoads.size(); i++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(strex("{}", i).c_str());
                    ImGui::TableSetColumnIndex(1);
                    const string load = strex("{:.1f}%", numeric_cast<float64_t>(_stats.CpuCoreLoads[i])).str();
                    ImGui::TextUnformatted(load.c_str(), load.c_str() + load.size());
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
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

    function<void(const Item*)> draw_item;

    draw_item = [&](const Item* item) {
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
                    for (const auto* inner : inner_items) {
                        draw_item(inner);
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    };

    const auto draw_critter = [&](const Critter* cr) {
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
                info_row("Inventory items", strex("{}", cr->GetInvItems().size()).str());
                info_row("Visible items", strex("{}", cr->GetVisibleItems().size()).str());
                info_row("Attached critters", strex("{}", cr->GetAttachedCritters().size()).str());
                ImGui::EndTable();
            }

            if (ImGui::TreeNode("Properties")) {
                draw_properties_table(cr);
                ImGui::TreePop();
            }

            auto inv_items = cr->GetInvItems();

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

    const auto draw_map = [&](const Map* map) {
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
                    for (const auto& cr : critters) {
                        draw_critter(cr.get());
                    }

                    ImGui::TreePop();
                }
            }

            if (!items.empty()) {
                if (ImGui::TreeNode(strex("Items ({})", items.size()).c_str())) {
                    for (const auto& item : items) {
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
        for (const auto& player : EntityMngr.GetPlayers()) {
            ImGui::PushID(static_cast<const void*>(player.get()));

            const auto label = strex("{} ({})", player->GetName(), player->GetId()).str();

            if (ImGui::TreeNode(label.c_str())) {
                const auto* cr = player->GetControlledCritter();
                const auto* connection = player->GetConnection();

                if (begin_info_table("##PlayerSummary")) {
                    const auto connection_diagnostics = connection->GetDiagnostics();

                    info_row("Id", strex("{}", player->GetId()).str());
                    info_row("Name", player->GetName());
                    info_row("Logined", strex("{}", player->GetLogined()).str());
                    info_row("Host", connection->GetHost());
                    info_row("Port", strex("{}", connection->GetPort()).str());
                    info_row("Hard disconnected", strex("{}", connection->IsHardDisconnected()).str());
                    info_row("Graceful disconnected", strex("{}", connection->IsGracefulDisconnected()).str());
                    info_row("Was handshake", strex("{}", connection_diagnostics.HandshakeComplete).str());
                    info_row("Last activity", strex("{}", connection_diagnostics.LastActivityTime).str());
                    info_row("Ping ok", strex("{}", connection_diagnostics.PingAnswerReceived).str());
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
        scoped_lock locker {_unloginedPlayersLocker};

        if (ImGui::CollapsingHeader(strex("Unlogined players ({})###UnloginedPlayers", _unloginedPlayers.size()).c_str())) {
            int32_t index = 0;

            for (const auto& player : _unloginedPlayers) {
                ImGui::PushID(index++);

                const auto* connection = player->GetConnection();
                const auto label = strex("{}:{}", connection->GetHost(), connection->GetPort()).str();

                if (ImGui::TreeNode(label.c_str())) {
                    if (begin_info_table("##UnloginedSummary")) {
                        const auto connection_diagnostics = connection->GetDiagnostics();

                        info_row("Host", connection->GetHost());
                        info_row("Port", strex("{}", connection->GetPort()).str());
                        info_row("Was handshake", strex("{}", connection_diagnostics.HandshakeComplete).str());
                        info_row("Hard disconnected", strex("{}", connection->IsHardDisconnected()).str());
                        info_row("Graceful disconnected", strex("{}", connection->IsGracefulDisconnected()).str());
                        info_row("Last activity", strex("{}", connection_diagnostics.LastActivityTime).str());
                        info_row("Ping ok", strex("{}", connection_diagnostics.PingAnswerReceived).str());
                        ImGui::EndTable();
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }
    }

    if (ImGui::CollapsingHeader(strex("Locations ({})###Locations", EntityMngr.GetLocationsCount()).c_str())) {
        for (const auto& loc : EntityMngr.GetLocations()) {
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
                        for (const auto* map : loc->GetMaps()) {
                            draw_map(map);
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
    buf += strex("Jobs per second: {}\n", _stats.JobsPerSecond);
    buf += strex("Jobs per minute: {}\n", _stats.JobsPerMinute);
    buf += strex("Total jobs: {}\n", _stats.JobsTotal);
    buf += strex("Rejected connections: {}\n", _stats.RejectedConnections);
    buf += strex("Rejected by rate: {}\n", _stats.RejectedByRate);
    buf += strex("CPU load: {}\n", _stats.CpuUsageAvailable ? strex("system {:.1f}%, process {:.1f}%", numeric_cast<float64_t>(_stats.CpuSystemLoad), numeric_cast<float64_t>(_stats.CpuProcessLoad)).str() : string("n/a"));
    buf += strex("DB requests per minute: {}\n", DbStorage.GetDbRequestsPerMinute());

    return buf;
}

auto ServerEngine::ShouldAcceptConnection(size_t cur_connections, size_t cur_players, int32_t max_connections, int32_t max_players) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (max_connections > 0 && cur_connections >= numeric_cast<size_t>(max_connections)) {
        return false;
    }
    if (max_players > 0 && cur_players >= numeric_cast<size_t>(max_players)) {
        return false;
    }

    return true;
}

auto ServerEngine::EvaluateConnectionRate(ConnRateState& state, int64_t now_sec, int32_t rate_per_sec) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (rate_per_sec <= 0) {
        return true;
    }

    if (state.WindowSec != now_sec) {
        state.WindowSec = now_sec;
        state.Count = 1;
        return true;
    }

    state.Count++;
    return state.Count <= rate_per_sec;
}

void ServerEngine::OnNewConnection(shared_ptr<NetworkServerConnection> net_connection)
{
    FO_STACK_TRACE_ENTRY();

    if (!_started || _shutdownInProgress) {
        net_connection->Disconnect();
        return;
    }

    // Anti-flood: drop bursts from a single source before any per-connection allocation.
    if (Settings.NewConnectionRatePerSec > 0) {
        constexpr size_t MAX_CONN_RATE_ENTRIES = 50000;
        const int64_t now_sec = nanotime::now().seconds();
        const string source_key {net_connection->GetHost()};
        bool accept_rate;

        {
            scoped_lock locker {_connRateLocker};

            // Bound the per-source map: when it grows large, drop entries whose one-second window has rolled
            // over (so a spoofed-IP flood cannot turn the rate guard itself into an unbounded-memory vector).
            if (_connRates.size() > MAX_CONN_RATE_ENTRIES) {
                std::erase_if(_connRates, [now_sec](const auto& entry) { return entry.second.WindowSec != now_sec; });
            }

            accept_rate = EvaluateConnectionRate(_connRates[source_key], now_sec, Settings.NewConnectionRatePerSec);
        }

        if (!accept_rate) {
            _rejectedByRate.fetch_add(1, std::memory_order_relaxed);
            net_connection->Disconnect();
            return;
        }
    }

    // Population cap: reject when the connection or player ceiling is reached, before creating a player.
    if (Settings.MaxConnections > 0 || Settings.MaxPlayers > 0) {
        size_t cur_connections;
        size_t cur_players;

        {
            scoped_lock locker {_unloginedPlayersLocker};
            cur_players = EntityMngr.GetPlayersCount();
            cur_connections = _unloginedPlayers.size() + cur_players;
        }

        if (!ShouldAcceptConnection(cur_connections, cur_players, Settings.MaxConnections, Settings.MaxPlayers)) {
            _rejectedConnections.fetch_add(1, std::memory_order_relaxed);
            WriteLog("Rejected new connection from {}: population cap reached (connections={}, players={}, max_connections={}, max_players={})", net_connection->GetHost(), cur_connections, cur_players, Settings.MaxConnections, Settings.MaxPlayers);
            net_connection->Disconnect();
            return;
        }
    }

    CreateUnloginedPlayer(std::move(net_connection));
}

auto ServerEngine::CreateUnloginedPlayer(shared_ptr<NetworkServerConnection> net_connection) -> Player*
{
    FO_STACK_TRACE_ENTRY();

    Player* unlogined_player;

    {
        scoped_lock locker {_unloginedPlayersLocker};

        auto connection = SafeAlloc::MakeUnique<ServerConnection>(Settings, std::move(net_connection));
        auto new_player = SafeAlloc::MakeRefCounted<Player>(this, ident_t {}, std::move(connection));
        _unloginedPlayers.emplace_back(std::move(new_player));
        unlogined_player = _unloginedPlayers.back().get();
    }

    // Widen the outer SyncContext's cover to include the freshly-created unlogined player so
    // the caller (script via `Game.CreateUnloginedPlayer()` or any engine path running inside
    // a job's SyncContext) can immediately read/write its properties without a separate
    // `Sync::Lock(player)`. Network-thread path has no current context — the conditional skips
    // safely there; the player isn't reachable from script until it's placed into the unlogined
    // list anyway.
    if (auto* outermost = SyncContext::GetOutermostOnThisThread(); outermost != nullptr) {
        outermost->EnsureEntitySynced(unlogined_player);
    }

    OnPlayerConnected(unlogined_player);
    return unlogined_player;
}

void ServerEngine::ProcessUnloginedPlayer(Player* unlogined_player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = unlogined_player->GetConnection();

    if (connection->IsHardDisconnected()) {
        ProcessPendingUnresolvedHash(connection);

        scoped_lock locker {_unloginedPlayersLocker};

        const auto it = std::ranges::find(_unloginedPlayers, unlogined_player);

        if (it != _unloginedPlayers.end()) {
            _unloginedPlayers.erase(it);
            unlogined_player->MarkAsDestroyed();
        }

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

        if (!connection->IsHandshakeComplete()) {
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
                if (!_updaterBackend) {
                    WriteLog(LogType::Warning, "Wrong update file request, updater backend disabled, client host '{}'", connection->GetHost());
                    connection->HardDisconnect();
                    break;
                }

                _updaterBackend->ProcessUpdateFile(connection, Settings.UpdateFileMaxPortionSize);
                break;
            case NetMessage::RemoteCall:
                Process_RemoteCall(unlogined_player);
                break;
            case NetMessage::UnresolvedHash:
                Process_UnresolvedHash(connection);
                break;
            default:
                throw GenericException("Unexpected unlogined player message", msg);
            }
        }

        in_buf.Lock();
        in_buf->ShrinkReadBuf();

        connection->RegisterActivity(GameTime.GetFrameTime());
    }
}

void ServerEngine::ProcessPlayer(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();

    if (connection->IsHardDisconnected()) {
        ProcessPendingUnresolvedHash(connection);

        WriteLog("Disconnected player {}", player->GetName());

        ValidateEntityAccess(player);
        ValidateEntityAccess(player->GetControlledCritter());
        OnPlayerLogout.Fire(player);
        FO_VERIFY_AND_THROW(!player->IsDestroyed(), "Player is already destroyed during server operation");

        player->DetachCritter();
        player->ResetViewMap();
        player->MarkAsDestroyed();
        EntityMngr.UnregisterPlayer(player);
        return;
    }

    if (connection->IsGracefulDisconnected()) {
        auto in_buf = connection->ReadBuf();

        in_buf->ResetBuf();
        return;
    }

    // Bound the messages drained per job pass so one flooding connection cannot monopolize a worker
    // thread (the pool is shared with world jobs). PlayerJob reschedules periodically and wakes on
    // new data, so any leftover buffered messages are processed on the next pass.
    const auto max_per_pass = Settings.MaxMessagesPerProcessPass;
    int32_t processed_msgs = 0;

    while (!connection->IsHardDisconnected() && !connection->IsGracefulDisconnected()) {
        if (max_per_pass != 0 && processed_msgs >= max_per_pass) {
            break;
        }

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
        case NetMessage::UnresolvedHash:
            Process_UnresolvedHash(connection);
            break;
        default:
            throw GenericException("Unexpected player message", msg);
        }

        in_buf.Lock();
        in_buf->ShrinkReadBuf();

        connection->RegisterActivity(GameTime.GetFrameTime());

        processed_msgs++;
    }
}

void ServerEngine::ProcessConnection(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (connection->IsHardDisconnected()) {
        return;
    }

    const auto frame_time = GameTime.GetFrameTime();

    connection->EnsureActivityTime(frame_time);

    if (connection->IsInactive(frame_time)) {
        WriteLog("Connection activity timeout from host '{}'", connection->GetHost());
        connection->HardDisconnect();
        return;
    }

    if (connection->NeedPing(frame_time)) {
        if (connection->HasPendingPing() && !IsRunInDebugger()) {
            connection->HardDisconnect();
            return;
        }

        auto out_buf = connection->WriteMsg(NetMessage::Ping);

        out_buf->Write(false);

        connection->RegisterPingRequest(frame_time);
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
        FO_VERIFY_AND_THROW(player, "Missing player instance");
    }

    if (player == nullptr) {
        return;
    }

    auto out_buf = player->GetConnection()->WriteMsg(NetMessage::RemoteCall);

    out_buf->Write<hstring>(name);
    out_buf->Write<int32_t>(numeric_cast<int32_t>(data.size()));
    out_buf->Push(data);
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

    bool release_empty_sync_context = false;

    if (!cr->IsDestroyed()) {
        auto* ctx = GetCurrentSyncContext();
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

        if (ctx->IsEmpty()) {
            ctx->SyncEntity(cr.get());
            release_empty_sync_context = true;
        }
        else {
            ctx->EnsureEntitySynced(cr.get());
        }
    }

    auto release_empty_sync = scope_exit([this, release_empty_sync_context]() noexcept {
        if (release_empty_sync_context) {
            if (auto* ctx = GetCurrentSyncContext(); ctx != nullptr) {
                ctx->Release();
            }
        }
    });

    if (!cr->IsDestroyed()) {
        ValidateEntityAccess(cr.get());
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

    FO_VERIFY_AND_THROW(cr_id, "Missing required critter id");

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

    if (cr == nullptr) {
        throw GenericException("Critter proto removed by migration rule", cr_id);
    }

    if (for_player) {
        cr->MarkIsForPlayer();
    }

    EntityMngr.MakePersistent(cr, true, true);
    MapMngr.AddCritterToMap(cr, nullptr, {}, hdir {}, {});

    bool release_empty_sync_context = false;

    if (!cr->IsDestroyed()) {
        auto* ctx = GetCurrentSyncContext();
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

        if (ctx->IsEmpty()) {
            ctx->SyncEntity(cr);
            release_empty_sync_context = true;
        }
        else {
            ctx->EnsureEntitySynced(cr);
        }
    }

    auto release_empty_sync = scope_exit([this, release_empty_sync_context]() noexcept {
        if (release_empty_sync_context) {
            if (auto* ctx = GetCurrentSyncContext(); ctx != nullptr) {
                ctx->Release();
            }
        }
    });

    if (!cr->IsDestroyed()) {
        ValidateEntityAccess(cr);
        OnGlobalMapCritterIn.Fire(cr);
    }
    if (!cr->IsDestroyed()) {
        EntityMngr.CallInit(cr, false);
        MapMngr.ProcessVisibleCritters(cr);
        MapMngr.ProcessVisibleItems(cr);
    }
    if (!cr->IsDestroyed()) {
        ValidateEntityAccess(cr);
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

    FO_VERIFY_AND_THROW(cr, "Missing critter instance");
    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");

    WriteLog(LogType::Info, "Unload critter {}", cr->GetName());

    if (cr->IsDestroying()) {
        throw GenericException("Critter in destroying state");
    }
    if (cr->GetPlayer() != nullptr) {
        throw GenericException("Player critter not detached from player before unloading");
    }

    cr->MarkAsDestroying();

    ValidateEntityAccess(cr);
    OnCritterUnload.Fire(cr);
    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");

    cr->Broadcast_Action(CritterAction::Disconnect, 0, nullptr);

    auto map = cr->GetParent<Map>();
    FO_VERIFY_AND_THROW(!cr->GetMapId() || map, "Critter has map id but map lookup failed");

    MapMngr.RemoveCritterFromMap(cr, map.get());
    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");

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
                FO_VERIFY_AND_THROW(custom_entity, "Missing custom entity instance");

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

            for (auto& inner_item : inner_items) {
                unload_item(inner_item.get());
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

    FO_VERIFY_AND_THROW(player, "Missing player instance");
    FO_VERIFY_AND_THROW(!player->IsDestroyed(), "Player is already destroyed during server operation");

    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    bool release_empty_sync_context = false;

    if (ctx->IsEmpty()) {
        ServerEntity* sync_entities[] = {player, cr};
        ctx->SyncEntities(span<ServerEntity*> {sync_entities});
        release_empty_sync_context = true;
    }
    else {
        ctx->EnsureEntitySynced(player);
        ctx->EnsureEntitySynced(player->GetControlledCritter());
        ctx->EnsureEntitySynced(cr);
    }

    auto release_empty_sync = scope_exit([ctx, release_empty_sync_context]() noexcept {
        if (release_empty_sync_context) {
            ctx->Release();
        }
    });

    ValidateEntityAccess(player);
    ValidateEntityAccess(cr);

    Critter* prev_cr = player->GetControlledCritter();
    ValidateEntityAccess(prev_cr);

    if (cr == nullptr) {
        if (prev_cr == nullptr) {
            return;
        }

        WriteLog(LogType::Info, "Detach player {} from critter {}", player->GetName(), prev_cr->GetName());
        player->DetachCritter();
        return;
    }

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");

    if (prev_cr == cr) {
        throw GenericException("Player critter already selected");
    }
    if (!cr->GetControlledByPlayer()) {
        throw GenericException("Critter can't be controlled by player");
    }
    if (cr->GetPlayer() != nullptr) {
        throw GenericException("Critter already attached to player");
    }

    WriteLog(LogType::Info, "Switch player {} to critter {}", player->GetName(), cr->GetName());

    player->ResetViewMap();

    if (prev_cr != nullptr) {
        prev_cr->DetachPlayer();
    }

    cr->AttachPlayer(player);

    SendCritterInitialInfo(cr, prev_cr);

    if (cr->IsDestroyed() || player->GetControlledCritter() != cr || cr->GetPlayer() != player) {
        return;
    }

    if (prev_cr != nullptr) {
        ValidateEntityAccess(player);
        ValidateEntityAccess(cr);
        ValidateEntityAccess(prev_cr);
        OnPlayerCritterSwitched.Fire(player, cr, prev_cr);
    }
}

void ServerEngine::DestroyUnloadedCritter(ident_t cr_id)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cr_id, "Missing required critter id");

    WriteLog(LogType::Info, "Destroy unloaded critter {}", cr_id);

    if (EntityMngr.GetCritter(cr_id) != nullptr) {
        throw GenericException("Critter must be unloaded before destroying");
    }

    DbStorage.Delete(Hashes.ToHashedString("Critters"), cr_id);
}

void ServerEngine::SendCritterInitialInfo(Critter* cr, Critter* prev_cr)
{
    FO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(cr);
    ValidateEntityAccess(prev_cr);

    auto map = cr->GetParent<Map>();
    FO_VERIFY_AND_THROW(!!cr->GetMapId() == !!map, "Critter map id and parent map presence disagree before sending initial info", cr->GetId(), cr->GetMapId(), map ? map->GetId() : ident_t {}, prev_cr != nullptr ? prev_cr->GetId() : ident_t {});

    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    bool release_empty_sync_context = false;

    if (ctx->IsEmpty()) {
        if (map) {
            ServerEntity* sync_entities[] = {cr, map.get()};
            ctx->SyncEntities(span<ServerEntity*> {sync_entities});
        }
        else {
            ctx->SyncEntity(cr);
        }
        release_empty_sync_context = true;
    }
    else {
        ctx->EnsureEntitySynced(cr);
        ctx->EnsureEntitySynced(map.get());
    }

    auto release_empty_sync = scope_exit([ctx, release_empty_sync_context]() noexcept {
        if (release_empty_sync_context) {
            ctx->Release();
        }
    });

    ValidateEntityAccess(cr);
    ValidateEntityAccess(prev_cr);
    ValidateEntityAccess(map.get());

    cr->Send_TimeSync();

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
                if (const auto item = EntityMngr.GetItem(item_id); item != nullptr) {
                    cr->Send_RemoveItemFromMap(item.get());
                }
            }
        }
    }
    else {
        if (map) {
            auto* loc = map->GetLocation();
            FO_VERIFY_AND_THROW(loc, "Missing location instance");
            ctx->EnsureEntitySynced(loc);
        }

        cr->Send_LoadMap(map.get());
    }

    cr->Broadcast_Action(CritterAction::Connect, 0, nullptr);

    cr->Send_AddCritter(cr);

    if (!map) {
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

            if (const auto item = EntityMngr.GetItem(item_id); item != nullptr) {
                cr->Send_AddItemOnMap(item.get());
            }
        }
    }

    ValidateEntityAccess(cr);
    OnCritterSendInitialInfo.Fire(cr);

    if (cr->IsDestroyed()) {
        return;
    }

    if (map != nullptr) {
        if (map->IsDestroyed() || cr->GetMapId() != map->GetId() || map->GetCritter(cr->GetId()) != cr) {
            return;
        }
    }
    else {
        if (cr->GetMapId()) {
            return;
        }
    }

    cr->Send_PlaceToGameComplete();
    cr->Send_TimeSync();
}

void ServerEngine::Process_Handshake(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    auto in_buf = connection->ReadBuf();

    // Net protocol
    const auto comp_version = in_buf->Read<string>();
    const auto updater_version = in_buf->Read<uint32_t>();
    const auto requested_binary_target = in_buf->Read<string>();

    const auto compatibility_outdated = comp_version != Settings.CompatibilityVersion;
    const auto updater_outdated = updater_version != FO_UPDATER_VERSION;

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

        out_buf->Write(compatibility_outdated);
        out_buf->Write(updater_outdated);
        out_buf->Write(out_encrypt_key);
    }

    {
        auto out_buf = connection->WriteBuf();

        out_buf->SetEncryptKey(out_encrypt_key);
    }

    if (updater_outdated) {
        WriteLog("Connected client {} has outdated updater version {}", connection->GetHost(), updater_version);
        connection->GracefulDisconnect();
        return;
    }

    {
        LockForPropertyAccess();
        auto unlock_global_vars = scope_exit([this]() noexcept { UnlockForPropertyAccess(); });

        vector<const uint8_t*>* global_vars_data = nullptr;
        vector<uint32_t>* global_vars_data_sizes = nullptr;
        StoreData(false, &global_vars_data, &global_vars_data_sizes);

        static const vector<uint8_t> empty_update_desc;
        const auto& update_desc = _updaterBackend != nullptr ? _updaterBackend->GetUpdateDescriptor(requested_binary_target) : empty_update_desc;

        auto out_buf = connection->WriteMsg(NetMessage::InitData);

        out_buf->Write(numeric_cast<uint32_t>(update_desc.size()));
        out_buf->Push(update_desc);
        out_buf->WritePropsData(global_vars_data, global_vars_data_sizes);
        out_buf->Write(GameTime.GetSynchronizedTime());
    }

    connection->MarkHandshakeComplete();

    if (compatibility_outdated) {
        WriteLog("Connected client {} has outdated compatibility version {} for binary target {}", connection->GetHost(), comp_version, requested_binary_target);
    }
    else {
        WriteLog("Connected client {} for binary target {}", connection->GetHost(), requested_binary_target);
        SendAllReportedHashes(connection);
    }
}

void ServerEngine::LoadReportedHashes()
{
    FO_STACK_TRACE_ENTRY();

    size_t resolved_count = 0;
    vector<string> loaded;

    for (const auto& reported_string : DbStorage.GetAllStringIds(HashReportsCollectionName)) {
        // Now resolvable — developers added the missing string after the report, so stop tracking and broadcasting it
        if (Hashes.CheckHashedString(reported_string)) {
            DbStorage.Delete(HashReportsCollectionName, reported_string);
            resolved_count++;
            continue;
        }

        WriteLog(LogType::Warning, "Client-reported hash is still unresolvable on the server: '{}'", reported_string);
        loaded.emplace_back(reported_string);
    }

    size_t total;

    {
        scoped_lock locker {_reportedHashesLocker};
        _reportedStrings.insert(loaded.begin(), loaded.end());
        total = _reportedStrings.size();
    }

    if (total != 0 || resolved_count != 0) {
        WriteLog("Loaded {} unresolved client-reported hash(es), {} now resolved", total, resolved_count);
    }
}

void ServerEngine::Process_UnresolvedHash(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    auto in_buf = connection->ReadBuf();

    const auto hash = in_buf->Read<hstring::hash_t>();

    in_buf.Unlock();

    RegisterClientReportedHash(connection, hash);
}

void ServerEngine::ProcessPendingUnresolvedHash(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    auto in_buf = connection->ReadBuf();

    if (!in_buf->NeedProcess()) {
        return;
    }

    const auto msg = in_buf->ReadMsg();

    if (msg != NetMessage::UnresolvedHash) {
        return;
    }

    const auto hash = in_buf->Read<hstring::hash_t>();

    in_buf.Unlock();

    RegisterClientReportedHash(connection, hash);
}

void ServerEngine::RegisterClientReportedHash(ServerConnection* connection, hstring::hash_t hash)
{
    FO_STACK_TRACE_ENTRY();

    if (hash == 0) {
        return;
    }

    bool failed = false;
    const hstring hstr = Hashes.ResolveHash(hash, &failed);

    if (failed) {
        // The server can't resolve it either — a deeper content/version mismatch. Log each one once per session.
        bool first_report;
        {
            scoped_lock locker {_reportedHashesLocker};
            first_report = _unresolvableReportedHashes.emplace(hash).second;
        }

        if (first_report) {
            WriteLog(LogType::Warning, "Client {} reported hash {} that the server can't resolve either", connection->GetHost(), hash);
        }

        return;
    }

    const string& reported_string = hstr.as_str();

    {
        scoped_lock locker {_reportedHashesLocker};

        // emplace is the atomic check-and-insert; already-known strings (already broadcast and persisted)
        // return false and are skipped, so only the first reporter persists and broadcasts it.
        if (!_reportedStrings.emplace(reported_string).second) {
            return;
        }
    }

    WriteLog(LogType::Warning, "Client {} couldn't resolve hash {}: '{}'", connection->GetHost(), hash, reported_string);

    // Persist so the list survives a server restart and preloads new clients immediately
    AnyData::Document doc;
    doc.Emplace("_Name", reported_string);
    DbStorage.Insert(HashReportsCollectionName, reported_string, doc);

    // Teach the freshly learned string to clients that are already connected
    BroadcastReportedString(reported_string);
}

void ServerEngine::SendAllReportedHashes(ServerConnection* connection)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    vector<string> snapshot;

    {
        scoped_lock locker {_reportedHashesLocker};
        snapshot.assign(_reportedStrings.begin(), _reportedStrings.end());
    }

    if (snapshot.empty()) {
        return;
    }

    auto out_buf = connection->WriteMsg(NetMessage::HashList);

    out_buf->Write(numeric_cast<uint32_t>(snapshot.size()));

    for (const auto& reported_string : snapshot) {
        out_buf->Write(reported_string);
    }
}

void ServerEngine::BroadcastReportedString(string_view reported_string)
{
    FO_STACK_TRACE_ENTRY();

    const auto send_to_connection = [reported_string](ServerConnection* conn) {
        if (conn == nullptr || conn->IsHardDisconnected() || !conn->IsHandshakeComplete()) {
            return;
        }

        auto out_buf = conn->WriteMsg(NetMessage::HashList);

        out_buf->Write(numeric_cast<uint32_t>(1));
        out_buf->Write(reported_string);
    };

    for (auto* player : copy_hold_ref(EntityMngr.GetPlayers())) {
        send_to_connection(player->GetConnection());
    }

    {
        scoped_lock locker {_unloginedPlayersLocker};

        for (auto& player : _unloginedPlayers) {
            send_to_connection(player->GetConnection());
        }
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
        connection->RegisterPingAnswer(GameTime.GetFrameTime());
    }
    else {
        auto out_buf = connection->WriteMsg(NetMessage::Ping);

        out_buf->Write(true);
    }
}

auto ServerEngine::LoginPlayerToNewRecord(Player* unlogined_player) -> Player*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!unlogined_player->GetLogined(), "Unlogged player is already marked as logged in");

    auto player_holder = refcount_ptr<Player> {unlogined_player};

    {
        scoped_lock locker {_unloginedPlayersLocker};

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
            safe_call([&] { player->ResetViewMap(); });
            safe_call([&] { player->MarkAsDestroyed(); });
            safe_call([&] { EntityMngr.UnregisterPlayer(player); });
        }
        safe_call([&] { player->SetLogined(false); });
        safe_call([&] { player->GetConnection()->HardDisconnect(); });
    }};

    EntityMngr.RegisterPlayer(player, ident_t {});
    registered_player = true;

    const auto player_doc = PropertiesSerializator::SaveToDocument(&player->GetProperties(), nullptr, Hashes, *this);
    DbStorage.Insert(PlayersCollectionName, player->GetId(), player_doc);
    inserted_player_record = true;

    player->SetLogined(true);
    player->Send_LoginSuccess();

    ValidateEntityAccess(player);

    const EventResult login_result = OnPlayerLogin.Fire(player, nullptr);

    if (login_result == Entity::EventResult::StopChain) {
        DbStorage.Delete(PlayersCollectionName, player->GetId());
        inserted_player_record = false;
        player->DetachCritter();
        player->ResetViewMap();
        player->MarkAsDestroyed();
        EntityMngr.UnregisterPlayer(player);
        registered_player = false;
        player->SetLogined(false);
        player->GetConnection()->GracefulDisconnect();
        return nullptr;
    }

    OnPlayerLogined(player, unlogined_player);

    return player;
}

auto ServerEngine::LoginPlayerToExistentRecord(Player* unlogined_player, ident_t player_id) -> Player*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!unlogined_player->GetLogined(), "Unlogged player is already marked as logged in");
    FO_VERIFY_AND_THROW(player_id, "Missing required player id");

    auto player_holder = refcount_ptr<Player> {unlogined_player};

    {
        scoped_lock locker {_unloginedPlayersLocker};

        vec_remove_unique_value(_unloginedPlayers, unlogined_player);
    }

    refcount_ptr<Player> player_ref;
    Player* player = nullptr;
    bool registered_player = false;
    bool reconnect_swapped = false;

    scope_fail disconnect_on_error {[&]() noexcept {
        if (reconnect_swapped) {
            safe_call([&] { player->SwapConnection(unlogined_player); });
        }

        if (registered_player) {
            safe_call([&] { unlogined_player->DetachCritter(); });
            safe_call([&] { unlogined_player->ResetViewMap(); });
            safe_call([&] { unlogined_player->MarkAsDestroyed(); });
            safe_call([&] { EntityMngr.UnregisterPlayer(unlogined_player); });
        }
        else {
            safe_call([&] { unlogined_player->MarkAsDestroyed(); });
        }

        safe_call([&] { unlogined_player->SetLogined(false); });
        safe_call([&] { unlogined_player->GetConnection()->HardDisconnect(); });
    }};

    player_ref = EntityMngr.GetPlayer(player_id);
    player = player_ref.get();

    if (player == nullptr) {
        player = unlogined_player;
        const auto player_doc = DbStorage.Get(PlayersCollectionName, player_id);

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

        ValidateEntityAccess(player);

        const EventResult login_result = OnPlayerLogin.Fire(player, nullptr);

        if (login_result == Entity::EventResult::StopChain) {
            player->DetachCritter();
            player->ResetViewMap();
            player->MarkAsDestroyed();
            EntityMngr.UnregisterPlayer(player);
            registered_player = false;
            player->SetLogined(false);
            player->GetConnection()->GracefulDisconnect();
            return nullptr;
        }
    }
    else {
        auto* ctx = GetCurrentSyncContext();
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
        ServerEntity* sync_entities[] = {player, unlogined_player};
        ctx->SyncEntities(span<ServerEntity*> {sync_entities});

        if (player->IsDestroyed() || unlogined_player->IsDestroyed()) {
            return nullptr;
        }

        // Kick previous
        player->SwapConnection(unlogined_player);
        reconnect_swapped = true;
        unlogined_player->GetConnection()->HardDisconnect();
        player->Send_LoginSuccess();

        ValidateEntityAccess(player);
        ValidateEntityAccess(unlogined_player);

        const EventResult login_result = OnPlayerLogin.Fire(player, unlogined_player);

        if (login_result == Entity::EventResult::StopChain) {
            player->SetLogined(false);
            player->GetConnection()->GracefulDisconnect();
            unlogined_player->SetLogined(false);
            unlogined_player->MarkAsDestroyed();
            return nullptr;
        }

        unlogined_player->MarkAsDestroyed();

        auto* cr = player->GetControlledCritter();

        if (cr != nullptr) {
            SendCritterInitialInfo(cr, nullptr);
        }
    }

    OnPlayerLogined(player, unlogined_player);

    disconnect_on_error.release();
    return player;
}

auto ServerEngine::LoginPlayerToTempSession(Player* unlogined_player) -> Player*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!unlogined_player->GetLogined(), "Unlogged player is already marked as logged in");

    auto player_holder = refcount_ptr<Player> {unlogined_player};

    {
        scoped_lock locker {_unloginedPlayersLocker};

        vec_remove_unique_value(_unloginedPlayers, unlogined_player);
    }

    auto* player = unlogined_player;
    bool registered_player = false;

    scope_fail disconnect_on_error {[&]() noexcept {
        if (registered_player) {
            safe_call([&] { player->DetachCritter(); });
            safe_call([&] { player->MarkAsDestroyed(); });
            safe_call([&] { EntityMngr.UnregisterPlayer(player); });
        }
        safe_call([&] { player->SetLogined(false); });
        safe_call([&] { player->GetConnection()->HardDisconnect(); });
    }};

    EntityMngr.RegisterPlayer(player, ident_t {}, false);
    registered_player = true;

    player->SetLogined(true);
    player->Send_LoginSuccess();

    ValidateEntityAccess(player);

    const EventResult login_result = OnPlayerLogin.Fire(player, nullptr);

    if (login_result == Entity::EventResult::StopChain) {
        player->DetachCritter();
        player->MarkAsDestroyed();
        EntityMngr.UnregisterPlayer(player);
        registered_player = false;
        player->SetLogined(false);
        player->GetConnection()->GracefulDisconnect();
        return nullptr;
    }

    OnPlayerLogined(player, unlogined_player);

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

    auto map = EntityMngr.GetMap(map_id);

    if (map == nullptr) {
        WriteLog("Process_Move: map not found, player '{}', map_id {}, cr_id {}", player->GetName(), map_id, cr_id);
        return;
    }

    auto cr_ref = EntityMngr.GetCritter(cr_id);
    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    ServerEntity* sync_entities[] = {player, map.get(), cr_ref.get()};
    ctx->SyncEntities(span<ServerEntity*> {sync_entities});

    if (player->IsDestroyed() || map->IsDestroyed()) {
        return;
    }

    Critter* cr = cr_ref.get();

    if (cr == nullptr || cr->IsDestroyed() || map->GetCritter(cr_id) != cr) {
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

    ValidateEntityAccess(player);
    ValidateEntityAccess(cr);

    const EventResult move_result = OnPlayerMoveCritter.Fire(player, cr, corrected_speed);

    if (connection->IsHardDisconnected() || connection->IsGracefulDisconnected()) {
        return;
    }
    if (cr->IsDestroyed() || map->IsDestroyed() || player->GetControlledCritter() != cr) {
        return;
    }
    if (cr->GetMapId() != map->GetId() || map->GetCritter(cr_id) != cr) {
        return;
    }
    if (move_result == EventResult::StopChain) {
        WriteLog("Process_Move: move rejected by script, player '{}', critter '{}' ({}) on map '{}', speed {}", player->GetName(), cr->GetName(), cr_id, map->GetName(), speed);
        player->Send_Moving(cr);
        return;
    }

    // Fix async errors
    const auto cr_hex = cr->GetHex();

    if (cr_hex != start_hex) {
        const auto find_result = MapMngr.FindPath(map.get(), cr, cr_hex, start_hex, cr->GetMultihex(), 0);

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

        for (const auto& step : steps) {
            auto next_hex = validate_hex;

            if (!GeometryHelper::MoveHexByDir(next_hex, step, map->GetSize())) {
                break;
            }

            const auto block = PathFinding::CheckHexWithMultihex(next_hex, step, multihex, map->GetSize(), check_hex);

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

    const auto client_hex = in_buf->Read<mpos>();
    const auto client_hex_offset = in_buf->Read<ipos16>();
    const auto client_dir = in_buf->Read<mdir>();

    in_buf.Unlock();

    auto map = EntityMngr.GetMap(map_id);

    if (map == nullptr) {
        WriteLog("Process_StopMove: map not found, player '{}', map_id {}, cr_id {}", player->GetName(), map_id, cr_id);
        return;
    }

    auto cr_ref = EntityMngr.GetCritter(cr_id);
    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    ServerEntity* sync_entities[] = {player, map.get(), cr_ref.get()};
    ctx->SyncEntities(span<ServerEntity*> {sync_entities});

    if (player->IsDestroyed() || map->IsDestroyed()) {
        return;
    }

    Critter* cr = cr_ref.get();

    if (cr == nullptr || cr->IsDestroyed() || map->GetCritter(cr_id) != cr) {
        WriteLog("Process_StopMove: critter not found, player '{}', map '{}' ({}), cr_id {}", player->GetName(), map->GetName(), map_id, cr_id);
        return;
    }

    if (cr->GetIsAttached()) {
        WriteLog("Process_StopMove: critter is attached, player '{}', critter '{}' ({}) on map '{}'", player->GetName(), cr->GetName(), cr_id, map->GetName());
        player->Send_Attachments(cr);
        player->Send_Moving(cr);
        return;
    }

    if (!cr->IsMoving()) {
        player->Send_Moving(cr);
        return;
    }

    int32_t zero_speed = 0;

    ValidateEntityAccess(player);
    ValidateEntityAccess(cr);

    const EventResult move_result = OnPlayerMoveCritter.Fire(player, cr, zero_speed);

    if (connection->IsHardDisconnected() || connection->IsGracefulDisconnected()) {
        return;
    }
    if (cr->IsDestroyed() || map->IsDestroyed() || player->GetControlledCritter() != cr) {
        return;
    }
    if (cr->GetMapId() != map->GetId() || map->GetCritter(cr_id) != cr) {
        return;
    }
    if (move_result == EventResult::StopChain) {
        WriteLog("Process_StopMove: stop rejected by script, player '{}', critter '{}' ({}) on map '{}'", player->GetName(), cr->GetName(), cr_id, map->GetName());
        player->Send_Moving(cr);
        return;
    }

    const uint32_t stop_moving_uid = cr->GetMovingUid();

    ReconcileCritterStopPosition(player, cr, map.get(), client_hex, client_hex_offset, client_dir);

    if (connection->IsHardDisconnected() || connection->IsGracefulDisconnected()) {
        return;
    }
    if (cr->IsDestroyed() || map->IsDestroyed() || player->GetControlledCritter() != cr) {
        return;
    }
    if (cr->GetMapId() != map->GetId() || map->GetCritter(cr_id) != cr) {
        return;
    }
    if (!cr->IsMoving() || cr->GetMovingUid() != stop_moving_uid) {
        player->Send_Moving(cr);
        return;
    }

    StopCritterMoving(cr, MovingState::Stopped, [&] { cr->SendAndBroadcast(player, [cr](Player* p) { p->Send_Moving(cr); }); });
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

    auto map = EntityMngr.GetMap(map_id);

    if (map == nullptr) {
        WriteLog("Process_Dir: map not found, player '{}', map_id {}, cr_id {}", player->GetName(), map_id, cr_id);
        return;
    }

    auto cr_ref = EntityMngr.GetCritter(cr_id);
    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    ServerEntity* sync_entities[] = {player, map.get(), cr_ref.get()};
    ctx->SyncEntities(span<ServerEntity*> {sync_entities});

    if (player->IsDestroyed() || map->IsDestroyed()) {
        return;
    }

    auto* cr = cr_ref.get();

    if (cr == nullptr || cr->IsDestroyed() || map->GetCritter(cr_id) != cr) {
        WriteLog("Process_Dir: critter not found, player '{}', map '{}' ({}), cr_id {}", player->GetName(), map->GetName(), map_id, cr_id);
        return;
    }

    mdir checked_dir = dir;

    ValidateEntityAccess(player);
    ValidateEntityAccess(cr);

    const EventResult dir_result = OnPlayerDirCritter.Fire(player, cr, checked_dir);

    if (connection->IsHardDisconnected() || connection->IsGracefulDisconnected()) {
        return;
    }
    if (cr->IsDestroyed() || map->IsDestroyed() || player->GetControlledCritter() != cr) {
        return;
    }
    if (cr->GetMapId() != map->GetId() || map->GetCritter(cr_id) != cr) {
        return;
    }
    if (dir_result == EventResult::StopChain) {
        WriteLog("Process_Dir: dir rejected by script, player '{}', critter '{}' ({}) on map '{}', angle {}", player->GetName(), cr->GetName(), cr_id, map->GetName(), dir.angle());
        player->Send_Dir(cr);
        return;
    }

    cr->ChangeDir(checked_dir);
    cr->SendAndBroadcast(player, [cr](Player* p) { p->Send_Dir(cr); });
}

void ServerEngine::Process_Property(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    Critter* cr = player->GetControlledCritter();

    const auto data_size = in_buf->Read<uint32_t>();

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
    refcount_ptr<ServerEntity> entity_ref;
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
            entity_ref = EntityMngr.GetCritter(cr_id);
            entity = entity_ref.get();
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
            auto item = EntityMngr.GetItem(item_id);
            if (item != nullptr && !item->GetHidden()) {
                entity_ref = item;
                entity = item.get();
            }
        }
        break;
    case NetProperty::CritterItem:
        is_public = true;
        prop = GetPropertyRegistrator(ItemProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);

        if (prop != nullptr) {
            auto cr_ = EntityMngr.GetCritter(cr_id);

            if (cr_) {
                auto* item = cr_->GetInvItem(item_id);

                if (item != nullptr && !item->GetHidden()) {
                    entity_ref = cr_; // cr_ keeps the inventory item alive via cr_'s lock
                    entity = item;
                }
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
            entity_ref = cr->GetParent<Map>();
            entity = entity_ref.get();
        }
        break;
    case NetProperty::Location:
        is_public = true;
        prop = GetPropertyRegistrator(LocationProperties::ENTITY_TYPE_NAME)->GetPropertyByIndex(property_index);

        if (prop != nullptr) {
            auto map = cr->GetParent<Map>();

            if (map) {
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

        // Take the entity's per-property auto-lock so this network-driven write serializes
        // through the same primitive used by script-side `Game.X` / `entity.X` access.
        // For non-engine entities `LockForPropertyAccess` is the default no-op virtual.
        entity->LockForPropertyAccess();
        auto unlock = scope_exit([entity]() noexcept { entity->UnlockForPropertyAccess(); });

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

        if (server_entity->IsDestroying() || server_entity->IsDestroyed()) {
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

void ServerEngine::OnSaveSynchronizedTime(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    // Dedicated PostSetter for `Game.SynchronizedTime` — throttles DB writes to ~once per
    // `SyncTimePersistLead` of game time, persisting `live + lead` so the write provides
    // headroom and the next write doesn't fire until live crosses the mark. Init-phase
    // sets (before time is synchronized) and `FlushExactSyncTime` write through directly.
    FO_VERIFY_AND_THROW(entity == this, "Synchronized time post-setter received an entity different from the server engine", prop != nullptr ? string_view {prop->GetName()} : string_view {}, entity != nullptr ? entity->GetTypeName() : hstring {}, entity != nullptr ? entity->GetId() : ident_t {});
    FO_VERIFY_AND_THROW(prop == GetPropertySynchronizedTime(), "Synchronized time post-setter received a different property", prop != nullptr ? string_view {prop->GetName()} : string_view {}, GetPropertySynchronizedTime()->GetName());

    if (!GameTime.IsTimeSynchronized()) {
        // Init / external pin path: persist the exact property value.
        const auto value = PropertiesSerializator::SavePropertyToValue(&entity->GetProperties(), prop, Hashes, *this);
        DbStorage.Update(entity->GetTypeName(), ident_t {1}, prop->GetName(), value);
        return;
    }

    const auto time = GameTime.GetSynchronizedTime();

    if (time < _persistedSyncTimeMark) {
        return;
    }

    _persistedSyncTimeMark = time + SyncTimePersistLead;
    const auto ahead_value = AnyData::Value {numeric_cast<int64_t>(_persistedSyncTimeMark.milliseconds())};
    DbStorage.Update(entity->GetTypeName(), ident_t {1}, prop->GetName(), ahead_value);
}

void ServerEngine::OnSendGlobalValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(entity);

    if (prop->IsPublicSync()) {
        for (auto* player : copy_hold_ref(EntityMngr.GetPlayers())) {
            player->Send_Property(NetProperty::Game, prop, this);
        }
    }
}

void ServerEngine::OnSendPlayerValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* player = dynamic_cast<Player*>(entity);
    FO_VERIFY_AND_THROW(player, "Missing player instance");

    player->Send_Property(NetProperty::Player, prop, player);
}

void ServerEngine::OnSendCritterValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* cr = dynamic_cast<Critter*>(entity);
    FO_VERIFY_AND_THROW(cr, "Missing critter instance");

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

    FO_NON_CONST_METHOD_HINT();

    auto* item = dynamic_cast<Item*>(entity);
    FO_VERIFY_AND_THROW(item, "Missing item instance");

    if (!item->GetStatic() && item->GetId()) {
        switch (item->GetOwnership()) {
        case ItemOwnership::Nowhere: {
        } break;
        case ItemOwnership::CritterInventory: {
            if (prop->IsPublicSync() || prop->IsOwnerSync()) {
                auto cr = item->GetParent<Critter>();

                if (cr) {
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
                auto map = item->GetParent<Map>();

                if (map) {
                    if (item->CanSendItem(true)) {
                        map->SendProperty(NetProperty::MapItem, prop, item);
                    }
                }
            }
        } break;
        case ItemOwnership::ItemContainer: {
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
        FO_VERIFY_AND_THROW(map, "Missing map instance");

        map->SendProperty(NetProperty::Map, prop, map);
    }
}

void ServerEngine::OnSendLocationValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (prop->IsPublicSync()) {
        auto* loc = dynamic_cast<Location*>(entity);
        FO_VERIFY_AND_THROW(loc, "Missing location instance");

        for (auto* map : loc->GetMaps()) {
            map->SendProperty(NetProperty::Location, prop, loc);
        }
    }
}

void ServerEngine::OnSendCustomEntityValue(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    auto* custom_entity = dynamic_cast<CustomEntity*>(entity);
    FO_VERIFY_AND_THROW(custom_entity, "Missing custom entity instance");

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
    FO_VERIFY_AND_THROW(cr, "Missing critter instance");

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
    FO_VERIFY_AND_THROW(item, "Missing item instance");

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

    FO_NON_CONST_METHOD_HINT();
    ignore_unused(prop);

    auto* item = dynamic_cast<Item*>(entity);
    FO_VERIFY_AND_THROW(item, "Missing item instance");

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto map = item->GetParent<Map>();
        FO_VERIFY_AND_THROW(map, "Missing map instance");
        map->ChangeViewItem(item);
    }
    else if (item->GetOwnership() == ItemOwnership::CritterInventory) {
        auto cr = item->GetParent<Critter>();
        FO_VERIFY_AND_THROW(cr, "Missing critter instance");

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

    FO_NON_CONST_METHOD_HINT();
    ignore_unused(prop);

    // NoBlock, ShootThru, IsGag, IsTrigger
    auto* item = dynamic_cast<Item*>(entity);
    FO_VERIFY_AND_THROW(item, "Missing item instance");

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        auto map = item->GetParent<Map>();

        if (map != nullptr) {
            map->RecacheHexFlags(item->GetHex());
        }
    }
}

void ServerEngine::OnSetItemMultihexLines(Entity* entity, const Property* prop)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
    ignore_unused(prop);

    const auto* item = dynamic_cast<Item*>(entity);
    FO_VERIFY_AND_THROW(item, "Missing item instance");

    if (item->GetOwnership() == ItemOwnership::MapHex) {
        const auto map = item->GetParent<Map>();

        if (map != nullptr) {
            throw NotImplementedException(FO_LINE_STR);
        }
    }
}

void ServerEngine::ProcessCritterMovingBySteps(Critter* cr, Map* map)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cr->IsMoving(), "Critter is not moving");
    auto* moving = cr->GetMoving();
    FO_VERIFY_AND_THROW(moving, "Missing active movement state");
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

                FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");

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

            ValidateEntityAccess(cr);
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

        if (!cr->GetControlledByPlayer()) {
            cr->SetDir(progress.Dir);
        }

        if (cr->HasAttachedCritters()) {
            cr->MoveAttachedCritters();

            if (!validate_moving(cr_hex)) {
                return;
            }
        }

        if (progress.Completed) {
            const bool incorrect_final_position = cr->GetHex() != moving->GetEndHex();

            if (incorrect_final_position) {
                // Send final position update to client to correct it.
                StopCritterMoving(cr, MovingState::Success);
            }
            else {
                // Normal completion, skip sending of final position update.
                StopCritterMoving(cr, MovingState::Success, [] { });
            }

            return;
        }

        if (!moved || moving->GetElapsedTime() >= moving->GetRuntimeElapsedTime(current_time)) {
            break;
        }
    }
}

auto ServerEngine::ReconcileCritterStopPosition(Player* player, Critter* cr, Map* map, mpos client_hex, ipos16 client_hex_offset, mdir client_dir) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(cr->IsMoving(), "Critter is not moving");

    const auto* moving = cr->GetMoving();
    FO_VERIFY_AND_THROW(moving, "Missing active movement state");
    moving->ValidateRuntimeState();

    constexpr int32_t max_path_hex_distance = 2;
    constexpr int32_t max_stop_correction_hex_distance = 4;

    if (!map->GetSize().is_valid_pos(client_hex)) {
        WriteLog("Process_StopMove: client stop hex is invalid, player '{}', critter '{}' ({}) on map '{}', hex ({},{})", player->GetName(), cr->GetName(), cr->GetId(), map->GetName(), client_hex.x, client_hex.y);
        return false;
    }

    if (!GeometryHelper::NormalizeHexOffset(client_hex, client_hex_offset, map->GetSize())) {
        WriteLog("Process_StopMove: client stop position is outside map after normalization, player '{}', critter '{}' ({}) on map '{}', hex ({},{}), offset ({},{})", player->GetName(), cr->GetName(), cr->GetId(), map->GetName(), client_hex.x, client_hex.y, client_hex_offset.x, client_hex_offset.y);
        return false;
    }

    const vector<mpos> path_hexes = moving->EvaluatePathHexes(moving->GetStartHex());
    const auto client_hex_it = std::ranges::find(path_hexes, client_hex);
    size_t client_hex_index = 0;
    bool use_movement_path = true;

    if (client_hex_it == path_hexes.end()) {
        int32_t best_distance = std::numeric_limits<int32_t>::max();

        for (size_t i = 0; i < path_hexes.size(); i++) {
            const int32_t dist = GeometryHelper::GetDistance(path_hexes[i], client_hex);

            if (dist < best_distance) {
                best_distance = dist;
                client_hex_index = i;
            }
        }

        if (best_distance > max_path_hex_distance) {
            use_movement_path = false;
        }
    }
    else {
        client_hex_index = numeric_cast<size_t>(std::distance(path_hexes.begin(), client_hex_it));
    }

    if (use_movement_path) {
        const auto current_hex_it = std::find(path_hexes.begin(), path_hexes.end(), cr->GetHex());

        if (current_hex_it == path_hexes.end()) {
            use_movement_path = false;
        }
        else {
            const size_t current_hex_index = numeric_cast<size_t>(std::distance(path_hexes.begin(), current_hex_it));

            if (current_hex_index < client_hex_index) {
                for (size_t i = current_hex_index + 1; i <= client_hex_index; i++) {
                    if (!MoveCritterToStopHex(cr, map, path_hexes[i])) {
                        return false;
                    }
                    if (cr->IsDestroyed() || map->IsDestroyed()) {
                        return false;
                    }
                }
            }
            else {
                for (size_t i = current_hex_index; i > client_hex_index; i--) {
                    if (!MoveCritterToStopHex(cr, map, path_hexes[i - 1])) {
                        return false;
                    }
                    if (cr->IsDestroyed() || map->IsDestroyed()) {
                        return false;
                    }
                }
            }
        }
    }

    if (cr->GetHex() != client_hex && !MoveCritterAlongStopCorrectionPath(player, cr, map, client_hex, max_stop_correction_hex_distance)) {
        return false;
    }

    const int16_t clamped_ox = numeric_cast<int16_t>(std::clamp(client_hex_offset.x, numeric_cast<int16_t>(-GameSettings::MAP_HEX_WIDTH / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_WIDTH / 2)));
    const int16_t clamped_oy = numeric_cast<int16_t>(std::clamp(client_hex_offset.y, numeric_cast<int16_t>(-GameSettings::MAP_HEX_HEIGHT / 2), numeric_cast<int16_t>(GameSettings::MAP_HEX_HEIGHT / 2)));

    cr->SetHexOffset({clamped_ox, clamped_oy});

    mdir checked_dir = client_dir;

    ValidateEntityAccess(player);
    ValidateEntityAccess(cr);

    const EventResult dir_result = OnPlayerDirCritter.Fire(player, cr, checked_dir);

    if (player->GetConnection()->IsHardDisconnected() || player->GetConnection()->IsGracefulDisconnected()) {
        return false;
    }
    if (cr->IsDestroyed() || map->IsDestroyed() || player->GetControlledCritter() != cr) {
        return false;
    }
    if (cr->GetMapId() != map->GetId() || map->GetCritter(cr->GetId()) != cr) {
        return false;
    }

    if (dir_result != EventResult::StopChain) {
        cr->ChangeDir(checked_dir);
    }

    if (cr->IsDestroyed() || map->IsDestroyed()) {
        return false;
    }

    if (cr->HasAttachedCritters()) {
        cr->MoveAttachedCritters();
    }

    return !cr->IsDestroyed() && !map->IsDestroyed();
}

auto ServerEngine::MoveCritterAlongStopCorrectionPath(Player* player, Critter* cr, Map* map, mpos target_hex, int32_t max_hex_distance) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(player, "Missing player instance");
    FO_VERIFY_AND_THROW(cr, "Missing critter instance");
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    const int32_t direct_distance = GeometryHelper::GetDistance(cr->GetHex(), target_hex);

    if (direct_distance > max_hex_distance) {
        WriteLog("Process_StopMove: client stop hex is too far from server hex, player '{}', critter '{}' ({}) on map '{}', server_hex ({},{}), client_hex ({},{}), distance {}, limit {}", player->GetName(), cr->GetName(), cr->GetId(), map->GetName(), cr->GetHex().x, cr->GetHex().y, target_hex.x, target_hex.y, direct_distance, max_hex_distance);
        return false;
    }

    const auto find_result = MapMngr.FindPath(map, cr, cr->GetHex(), target_hex, cr->GetMultihex(), 0);

    if (find_result.Result == FindPathOutput::ResultType::AlreadyHere) {
        return true;
    }
    if (find_result.Result != FindPathOutput::ResultType::Ok) {
        WriteLog("Process_StopMove: stop correction pathfinding failed, player '{}', critter '{}' ({}) on map '{}', server_hex ({},{}), client_hex ({},{}), distance {}", player->GetName(), cr->GetName(), cr->GetId(), map->GetName(), cr->GetHex().x, cr->GetHex().y, target_hex.x, target_hex.y, direct_distance);
        return false;
    }
    if (find_result.Steps.size() > numeric_cast<size_t>(max_hex_distance)) {
        WriteLog("Process_StopMove: stop correction path is too long, player '{}', critter '{}' ({}) on map '{}', server_hex ({},{}), client_hex ({},{}), path {}, limit {}", player->GetName(), cr->GetName(), cr->GetId(), map->GetName(), cr->GetHex().x, cr->GetHex().y, target_hex.x, target_hex.y, find_result.Steps.size(), max_hex_distance);
        return false;
    }

    mpos step_hex = cr->GetHex();

    for (const mdir step : find_result.Steps) {
        if (!GeometryHelper::MoveHexByDir(step_hex, step, map->GetSize())) {
            return false;
        }
        if (!MoveCritterToStopHex(cr, map, step_hex)) {
            return false;
        }
        if (cr->IsDestroyed() || map->IsDestroyed()) {
            return false;
        }
    }

    return cr->GetHex() == target_hex;
}

auto ServerEngine::MoveCritterToStopHex(Critter* cr, Map* map, mpos target_hex) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const ident_t expected_map_id = map->GetId();
    const ident_t expected_cr_id = cr->GetId();
    const uint32_t expected_moving_uid = cr->GetMovingUid();

    const auto validate_moved_critter = [cr, map, expected_map_id, expected_cr_id, expected_moving_uid, target_hex] {
        if (cr->IsDestroyed() || map->IsDestroyed()) {
            return false;
        }
        if (cr->GetId() != expected_cr_id || cr->GetMapId() != expected_map_id || cr->GetMovingUid() != expected_moving_uid) {
            return false;
        }
        if (cr->GetHex() != target_hex) {
            return false;
        }
        return true;
    };

    const mpos old_hex = cr->GetHex();

    if (old_hex == target_hex) {
        return true;
    }

    const mdir dir = mdir(iround<int32_t>(GeometryHelper::GetDirAngle(old_hex, target_hex)));
    const int32_t multihex = cr->GetMultihex();
    const auto check_hex = [map](mpos h) -> HexBlockResult { return map->IsHexMovable(h) ? HexBlockResult::Passable : HexBlockResult::Blocked; };

    if (PathFinding::CheckHexWithMultihex(target_hex, dir, multihex, map->GetSize(), check_hex) == HexBlockResult::Blocked) {
        return false;
    }

    refcount_ptr cr_ref_holder = cr;
    refcount_ptr map_ref_holder = map;

    map->RemoveCritterFromField(cr);
    cr->SetHex(target_hex);
    map->AddCritterToField(cr);

    FO_VERIFY_AND_THROW(!cr->IsDestroyed(), "Critter is already destroyed");

    map->VerifyTrigger(cr, old_hex, target_hex, dir);

    if (!validate_moved_critter()) {
        return false;
    }

    MapMngr.ProcessVisibleCritters(cr);

    if (!validate_moved_critter()) {
        return false;
    }

    MapMngr.ProcessVisibleItems(cr);

    if (!validate_moved_critter()) {
        return false;
    }

    ValidateEntityAccess(cr);
    OnCritterMoved.Fire(cr, old_hex);

    if (!validate_moved_critter()) {
        return false;
    }

    MapMngr.ProcessVisibleCritters(cr);

    if (!validate_moved_critter()) {
        return false;
    }

    MapMngr.ProcessVisibleItems(cr);

    return validate_moved_critter();
}

void ServerEngine::StartCritterMoving(Critter* cr, refcount_ptr<MovingContext> moving, const Player* initiator)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (cr->GetIsAttached()) {
        cr->DetachFromCritter();
    }

    const bool was_moving = cr->IsMoving();
    const auto movement_key = WorkerJobKey {.Type = WorkerJobType::CritterMovement, .Id = static_cast<size_t>(cr->GetId().underlying_value())};

    if (was_moving) {
        _workerPool->Cancel(movement_key);
    }

    cr->StopMoving(MovingState::Stopped);
    cr->SetMoving(std::move(moving));
    cr->GetMoving()->ValidateRuntimeState();
    cr->SetMovingSpeed(cr->GetMoving()->GetSpeed());

    _workerPool->Submit(movement_key, [this, cr_ = refcount_ptr<Critter>(cr)]() mutable -> std::optional<timespan> { return CritterMovingJob(cr_.get()); });

    cr->SendAndBroadcast(initiator, [cr](Player* p) { p->Send_Moving(cr); });

    ValidateEntityAccess(cr);
    OnCritterStartMoving.Fire(cr, was_moving);
}

auto ServerEngine::CritterMovingJob(Critter* cr) -> std::optional<timespan>
{
    FO_STACK_TRACE_ENTRY();

    auto complete_stats_job = scope_exit([this]() noexcept { CountServerStatsJob(); });

    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->SyncEntity(cr);

    auto map = cr->GetParent<Map>();
    FO_VERIFY_AND_THROW(!!cr->GetMapId() == !!map, "Critter map id and parent map presence disagree during critter synchronization", cr->GetId(), cr->GetMapId(), map ? map->GetId() : ident_t {});

    if (map) {
        ServerEntity* sync_entities[] = {map.get(), cr};
        ctx->SyncEntities(span<ServerEntity*> {sync_entities});
    }

    if (cr->IsDestroyed() || !cr->IsMoving()) {
        return std::nullopt;
    }

    try {
        if (map && !cr->GetIsAttached()) {
            ProcessCritterMovingBySteps(cr, map.get());
        }
        else {
            const auto reason = cr->GetIsAttached() ? MovingState::Attached : MovingState::GenericError;
            StopCritterMoving(cr, reason);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }

    if (cr->IsDestroyed() || !cr->IsMoving()) {
        return std::nullopt;
    }

    return std::chrono::milliseconds {Settings.CritterMovingPeriodMs};
}

void ServerEngine::StartCritterMoving(Critter* cr, uint16_t speed, const vector<mdir>& steps, const vector<uint16_t>& control_steps, ipos16 end_hex_offset, const Player* initiator)
{
    FO_STACK_TRACE_ENTRY();

    const auto map = cr->GetParent<Map>();
    FO_VERIFY_AND_THROW(map, "Missing map instance");

    const auto start_hex = cr->GetHex();

    StartCritterMoving(cr, SafeAlloc::MakeRefCounted<MovingContext>(map->GetSize(), speed, steps, control_steps, GameTime.GetFrameTime(), timespan {}, start_hex, cr->GetHexOffset(), end_hex_offset), initiator);
}

void ServerEngine::StopCritterMoving(Critter* cr, MovingState reason, function<void()> customSend)
{
    FO_STACK_TRACE_ENTRY();

    if (!cr->IsMoving()) {
        return;
    }

    const auto key = WorkerJobKey {.Type = WorkerJobType::CritterMovement, .Id = static_cast<size_t>(cr->GetId().underlying_value())};
    _workerPool->Cancel(key);

    cr->StopMoving(reason);

    if (customSend) {
        customSend();
    }
    else {
        cr->SendAndBroadcast_Moving();
    }

    ValidateEntityAccess(cr);
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

    cr->SendAndBroadcast(nullptr, [cr](Player* p) { p->Send_MovingSpeed(cr); });

    ValidateEntityAccess(cr);
    OnCritterStartMoving.Fire(cr, true);
}

void ServerEngine::Process_RemoteCall(Player* player)
{
    FO_STACK_TRACE_ENTRY();

    auto* connection = player->GetConnection();
    auto in_buf = connection->ReadBuf();

    const auto remote_call_name = in_buf->Read<hstring>(Hashes);
    const auto remote_call_data_size = in_buf->Read<int32_t>();

    // The payload can never be larger than the bytes still buffered; reject before allocating
    if (remote_call_data_size < 0 || numeric_cast<size_t>(remote_call_data_size) > in_buf->GetUnreadSize()) {
        throw GenericException("Invalid remote call data size", remote_call_data_size);
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

    auto* ctx = GetCurrentSyncContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->SyncEntity(player);

    HandleInboundRemoteCall(remote_call_name, player, remote_call_data);
}

FO_END_NAMESPACE
