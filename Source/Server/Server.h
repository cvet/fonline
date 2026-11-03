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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
#include "EngineBase.h"
#include "EntityManager.h"
#include "Geometry.h"
#include "ImGuiStuff.h"
#include "Item.h"
#include "ItemManager.h"
#include "Location.h"
#include "Map.h"
#include "MapManager.h"
#include "NetworkServer.h"
#include "Player.h"
#include "ProtoManager.h"
#include "ScriptSystem.h"
#include "ServerConnection.h"
#include "Settings.h"
#include "UpdaterBackend.h"
#include "WorkerPool.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ServerInitException);
FO_DECLARE_EXCEPTION(ServerQuiescenceException);
FO_DECLARE_EXCEPTION(ServerSnapshotException);

auto GetServerResources(GlobalSettings& settings) -> FileSystem;

enum class ServerSnapshotBlockerKind : uint8_t
{
    SuspendedScriptContexts,
    ActiveScriptContexts,
    RetainedScriptContexts,
    DelayedCallbacks,
    TimeEvents,
    CritterMovements,
    DelegateBoundTimeEvents,
    EmbeddingState,
    ManagedScriptWork,
};

struct ServerSnapshotBlocker
{
    ServerSnapshotBlockerKind Kind {};
    size_t Count {};
    string Identifier {};
};

struct ServerSnapshotEligibilityIssue
{
    string Identifier {};
    size_t Count {};
};

// Exhaustive public lifecycle state for an authoritative server session
enum class ServerSessionState : uint8_t
{
    Starting = 0, // Constructed, startup jobs not finished
    Running = 1, // Started, simulation advancing
    Paused = 2, // Started, simulation held by PauseSimulation
    Stopping = 3, // Shutdown requested, world not yet torn down
    Stopped = 4, // Shutdown finished
    Failed = 5, // Startup did not complete
};

[[nodiscard]] auto ServerSessionStateName(ServerSessionState state) noexcept -> string_view;

// Process-independent movement state; elapsed time replaces the monotonic start timestamp
struct SerializableCritterMovement
{
    ident_t CritterId {};
    ident_t MapId {};
    msize MapSize {};
    uint16_t Speed {};
    vector<mdir> Steps {};
    vector<uint16_t> ControlSteps {};
    mpos StartHex {};
    ipos16 StartHexOffset {};
    ipos16 EndHexOffset {};
    float32_t ElapsedTimeMs {};
    float32_t WholeTimeMs {};
    float32_t LeaseTimeMs {};
};

struct ServerSnapshotState
{
    static constexpr uint32_t FORMAT_VERSION = 4;

    uint32_t FormatVersion {FORMAT_VERSION};
    string CompatibilityVersion {};
    string MetadataVersion {};
    synctime SynchronizedTime {};
    ident_t LastEntityId {};
    random_generator::state_data RandomState {};
    uint32_t LastTimeEventId {};
    vector<TimeEventManager::SerializableTimeEvent> TimeEvents {};
    vector<SerializableCritterMovement> CritterMovements {};
};

// The captured world as plain bytes plus the state that describes them. Storing the pair, naming it,
// versioning the container and publishing it atomically are the embedder's business, not the Engine's
struct ServerSnapshotCaptureResult
{
    bool ReachedQuiescence {};
    vector<ServerSnapshotBlocker> Blockers {};
    optional<ServerSnapshotState> State {};
    vector<uint8_t> Payload {};
};

struct ServerSnapshotRestore
{
    ServerSnapshotState State {};
    vector<uint8_t> Payload {};
};

struct ServerQuiescenceState
{
    synctime SynchronizedTime {};
    random_generator::state_data RandomState {};
};

class ServerEngine final : public BaseEngine, public EntityManagerApi
{
    friend class ServerScriptSystem;
    friend class ServerEntity;

public:
    using SnapshotEligibilityCheck = copyable_function<vector<ServerSnapshotEligibilityIssue>(ptr<const ServerEngine>)>;
    using QuiescenceCallback = function<void(const ServerQuiescenceState&)>;

    explicit ServerEngine(ptr<GlobalSettings> settings, FileSystem&& resources, optional<ServerSnapshotRestore> restore_snapshot = std::nullopt);

    ServerEngine(const ServerEngine&) = delete;
    ServerEngine(ServerEngine&&) noexcept = delete;
    auto operator=(const ServerEngine&) = delete;
    auto operator=(ServerEngine&&) noexcept = delete;
    ~ServerEngine() override;

    [[nodiscard]] auto GetEngine() noexcept -> ptr<ServerEngine> { return this; }
    [[nodiscard]] auto IsStarted() const noexcept -> bool { return _started; }
    [[nodiscard]] auto IsStartingError() const noexcept -> bool { return _startingError; }
    [[nodiscard]] auto IsShutdownInProgress() const noexcept -> bool { return _shutdownInProgress.load(); }
    [[nodiscard]] auto IsRestoredFromSnapshot() const noexcept -> bool { return _restoreSnapshot.has_value(); }
    [[nodiscard]] auto IsSimulationPaused() const noexcept -> bool { return _simulationPaused.load(std::memory_order_acquire); }
    [[nodiscard]] auto GetSessionState() const noexcept -> ServerSessionState;
    [[nodiscard]] auto GetHealthInfo() const -> string;
    [[nodiscard]] auto GetLangPack() const -> const TextPack& { return _defaultLang; }
    [[nodiscard]] auto GetCurrentSyncContext() const noexcept -> nptr<SyncContext> { return SyncContext::GetCurrentOnThisThread(); }
    [[nodiscard]] auto RequireCurrentSyncContext() const -> ptr<SyncContext>;
    [[nodiscard]] auto GetEntityLock() const noexcept -> ptr<EntityLock> { return _entityLock; }
    [[nodiscard]] auto GetCompletedServerJobsCount() const -> uint64_t;
    [[nodiscard]] auto IsConnectionAdmissionOpen() const -> bool;
    [[nodiscard]] auto GetWorkerThreadCount() const -> int32_t;

    void Shutdown() override;
    void FlushExactSyncTime();

    void LockForPropertyAccess() noexcept override;
    void UnlockForPropertyAccess() noexcept override;
    void LockForPropertyAccessShared() noexcept override;
    void UnlockForPropertyAccessShared() noexcept override;

    void ScheduleDelayedCallback(timespan delay, function<void()> body) override;
    auto RunScriptContext(const function<void()>& callback) -> timespan override;

    auto CreateCustomInnerEntity(ptr<Entity> holder, hstring entry, hstring pid) -> nptr<Entity> override { return EntityMngr.CreateCustomInnerEntity(holder, entry, pid); }
    auto CreateCustomEntity(hstring type_name, hstring pid) -> nptr<Entity> override { return EntityMngr.CreateCustomEntity(type_name, pid); }
    auto GetCustomEntity(hstring type_name, ident_t id) -> refcount_nptr<Entity> override
    {
        auto custom_entity = EntityMngr.GetCustomEntity(type_name, id);

        if (!custom_entity) {
            return nullptr;
        }

        auto custom_entity_holder = std::move(custom_entity).take_not_null();
        refcount_ptr<Entity> entity = std::move(custom_entity_holder);
        return std::move(entity);
    }
    void DestroyEntity(ptr<Entity> entity) override { EntityMngr.DestroyEntity(entity); }

    auto Lock(optional<timespan> max_wait_time) -> bool;
    void Unlock();
    auto RunInQuiescence(optional<timespan> max_wait_time, const QuiescenceCallback& callback) -> bool;
    auto CreateSnapshot(optional<timespan> max_wait_time) -> ServerSnapshotCaptureResult;
    // Structural restore preflight without a world; compatibility and content identity remain the caller's checks
    static void ValidateServerSnapshotState(const ServerSnapshotState& state);
    void RegisterSnapshotEligibilityCheck(SnapshotEligibilityCheck check);
    void PauseSimulation();
    void ResumeSimulation();
    [[nodiscard]] auto HoldSimulation(string_view reason) -> bool;
    [[nodiscard]] auto ReleaseSimulationHold(string_view reason) -> bool;
    void DrawGui();

    auto CreateNotLoggedInPlayer(shared_ptr<NetworkServerConnection> net_connection) -> ptr<Player>;
    auto LoginPlayerToNewRecord(ptr<Player> not_logged_in_player) -> ptr<Player>;
    auto LoginPlayerToExistentRecord(ptr<Player> not_logged_in_player, ident_t player_id) -> ptr<Player>;
    auto LoginPlayerToTempSession(ptr<Player> not_logged_in_player) -> ptr<Player>;

    auto CreateCritter(hstring pid, bool for_player, nptr<const Properties> props = nullptr) -> ptr<Critter>;
    auto LoadCritter(ident_t cr_id, bool for_player, nptr<Map> map = nullptr) -> ptr<Critter>;
    void UnloadCritter(ptr<Critter> cr);
    void UnloadCritterInnerEntities(ptr<Critter> cr);
    void SwitchPlayerCritter(ptr<Player> player, nptr<Critter> cr);
    void ReturnPlayerFromViewMap(ptr<Player> player);
    void DestroyUnloadedCritter(ident_t cr_id);

    void StartCritterMoving(ptr<Critter> cr, refcount_ptr<MovingContext> moving, nptr<const Player> initiator, uint32_t plan_seq = 0, float32_t plan_distance_shift = 0.0f);
    void StartCritterMoving(ptr<Critter> cr, uint16_t speed, const vector<mdir>& steps, const vector<uint16_t>& control_steps, ipos16 end_hex_offset, nptr<const Player> initiator, timespan offset_time = {});
    void StopCritterMoving(ptr<Critter> cr, MovingState reason = MovingState::Stopped, function<void()> customSend = nullptr);
    void ChangeCritterMovingSpeed(ptr<Critter> cr, uint16_t speed);
    // Writes one server-side MOVESYNC line. Call sites test Network.MoveSyncTrace first, so a disabled trace
    // formats nothing on the movement hot path
    void TraceMoveSync(string_view event, string_view details);

    // Runs after script modules initialize and before the world is generated or restored; `StopChain` aborts server initialization
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnInit);
    // Runs only for an empty database to create the initial world; `StopChain` aborts server initialization
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGenerateWorld);
    // Runs after world generation or restoration and before normal server work; `StopChain` aborts startup
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnStart);
    // Runs during server shutdown before script modules are finalized
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    // Native admission gate before registration, storage or reconnect replacement; StopChain rejects the candidate
    FO_ENTITY_EVENT(OnPlayerPreLogin, ptr<Player> /*notLoggedInPlayer*/);
    // Native gate before each complete logged-in message; StopChain disconnects and stops dispatch
    FO_ENTITY_EVENT(OnPlayerPreProcess, ptr<Player> /*player*/);
    // Runs after login success is sent and before post-login setup; `notLoggedInPlayer` is present only during reconnect, and `StopChain` rejects the login
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerLogin, ptr<Player> /*player*/, nptr<Player> /*notLoggedInPlayer*/);
    // Runs after a hard disconnect is observed and before the player is detached and unregistered
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerLogout, ptr<Player> /*player*/);
    // Runs after a player switches from `prevCr` to `cr` and initial critter information is sent
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerCritterSwitched, ptr<Player> /*player*/, ptr<Critter> /*cr*/, ptr<Critter> /*prevCr*/);
    // Validates a player movement request; handlers may replace `speed`, and `StopChain` rejects the request
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerMoveCritter, ptr<Player> /*player*/, ptr<Critter> /*cr*/, int32_t& /*speed*/);
    // Validates a player direction request; handlers may replace `dir`, and `StopChain` rejects the request
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerDirCritter, ptr<Player> /*player*/, ptr<Critter> /*cr*/, mdir& /*dir*/);
    // Runs after a critter changes map hex and reports the previous hex in `oldHex`
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterMoved, ptr<Critter> /*cr*/, mpos /*oldHex*/);
    // Runs after a movement context is activated and broadcast; `wasMoving` reports whether it replaced active movement
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterStartMoving, ptr<Critter> /*cr*/, bool /*wasMoving*/);
    // Runs after active critter movement is stopped and the resulting state is broadcast
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterStopMoving, ptr<Critter> /*cr*/);
    // Runs after a critter transfer completes; `prevMap` is absent when the critter did not come from a local map
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterTransfer, ptr<Critter> /*cr*/, nptr<Map> /*prevMap*/);
    // Runs when a critter enters the global-map container
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGlobalMapCritterIn, ptr<Critter> /*cr*/);
    // Runs when a critter leaves the global-map container
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGlobalMapCritterOut, ptr<Critter> /*cr*/);
    // Runs once before a location's own initialization script; `firstTime` distinguishes creation from restore
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLocationInit, ptr<Location> /*location*/, bool /*firstTime*/);
    // Runs while a location is being finished, before its maps are destroyed
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLocationFinish, ptr<Location> /*location*/);
    // Runs once before a map's own initialization script; `firstTime` distinguishes creation from restore
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapInit, ptr<Map> /*map*/, bool /*firstTime*/);
    // Runs while a map is being finished and before it is unregistered
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapFinish, ptr<Map> /*map*/);
    // Runs after a critter is attached to a local map
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapCritterIn, ptr<Map> /*map*/, ptr<Critter> /*cr*/);
    // Runs before a critter is detached from a local map
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapCritterOut, ptr<Map> /*map*/, ptr<Critter> /*cr*/);
    // Runs after a persistent critter and its owned entities are restored but before world entry; `StopChain` rejects the load
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterPreLoad, ptr<Critter> /*cr*/);
    // Runs once before a critter's own initialization script; `firstTime` distinguishes creation from restore
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterInit, ptr<Critter> /*cr*/, bool /*firstTime*/);
    // Runs while a critter is being finished and before it is unregistered
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterFinish, ptr<Critter> /*cr*/);
    // Runs after a persistent critter is loaded and enters server ownership
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterLoad, ptr<Critter> /*cr*/);
    // Runs before a persistent critter leaves server ownership without being destroyed
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterUnload, ptr<Critter> /*cr*/);
    // Runs while the server prepares initial controlled-critter state for its player
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterSendInitialInfo, ptr<Critter> /*cr*/);
    // Runs after an item enters, leaves, or changes slot in a critter inventory; `fromSlot` reports its previous slot
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterItemMoved, ptr<Critter> /*cr*/, ptr<Item> /*item*/, CritterItemSlot /*fromSlot*/);
    // Runs once before an item's own initialization script; `firstTime` distinguishes creation from restore
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemInit, ptr<Item> /*item*/, bool /*firstTime*/);
    // Runs while an item is being finished and before it is unregistered
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemFinish, ptr<Item> /*item*/);
    // Runs when a critter crosses a static item's trigger footprint; `isIn` distinguishes entry from exit
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnStaticItemWalk, ptr<StaticItem> /*item*/, ptr<Critter> /*cr*/, bool /*isIn*/, mdir /*dir*/);

private:
    std::atomic_bool _started {false};
    std::atomic_bool _startingError {false};
    std::atomic_bool _shutdownInProgress {false};
    std::atomic_bool _shutdownCompleted {false};
    std::atomic<int32_t> _liveEntityCount {};

public:
    EntityManager EntityMngr;
    MapManager MapMngr;
    CritterManager CrMngr;
    ItemManager ItemMngr;

    DataBase DbStorage {};
    const hstring GameCollectionName = Hashes.to_hashed_string("Game");
    const hstring HistoryCollectionName = Hashes.to_hashed_string("History");
    const hstring PlayersCollectionName = Hashes.to_hashed_string("Players");
    const hstring CrittersCollectionName = Hashes.to_hashed_string("Critters");
    const hstring HashReportsCollectionName = Hashes.to_hashed_string("HashReports");

    EventObserver<> OnWillFinish {};
    EventObserver<> OnDidFinish {};

    struct ConnRateState
    {
        int64_t WindowSec {};
        int32_t Count {};
    };

    static auto ShouldAcceptConnection(size_t cur_connections, size_t cur_players, int32_t max_connections, int32_t max_players) noexcept -> bool;
    static auto EvaluateConnectionRate(ConnRateState& state, int64_t now_sec, int32_t rate_per_sec) noexcept -> bool;

private:
    struct ServerStats
    {
        nanotime ServerStartTime {};
        timespan Uptime {};

        size_t MaxOnline {};
        size_t CurOnline {};
        size_t RejectedConnections {};
        size_t RejectedByRate {};

        uint64_t JobsTotal {};
        uint64_t JobsPerSecond {};
        uint64_t JobsPerMinute {};
        nanotime JobCounterBegin {};
        uint64_t JobCounterBeginTotal {};
        deque<pair<nanotime, uint64_t>> JobTimeStamps {};

        optional<platform::cpu_usage_snapshot> LastCpuUsageSnapshot {};
        nanotime LastCpuUsageSampleTime {};
        bool CpuUsageAvailable {};
        float32_t CpuSystemLoad {};
        float32_t CpuProcessLoad {};
        float32_t CpuProcessCoreLoad {};
        vector<float32_t> CpuCoreLoads {};
    };

    void SyncPoint();
    void SyncWholeWorld(SyncContext& ctx, span<const refcount_ptr<Player>> additional_players = {});
    void SuspendSimulationWork();
    void RestoreSimulationWork();
    void RestoreSnapshotTimeEvents();
    void RestoreSnapshotCritterMovements();
    auto CaptureCritterMovements() -> vector<SerializableCritterMovement>;
    auto CanRestoreSnapshotEntity(ptr<const ServerEntity> entity) const -> bool;
    auto CollectSnapshotBlockers() -> vector<ServerSnapshotBlocker>;

    void StartConnectionServer(string_view what, const function<unique_ptr<NetworkServer>()>& start);
    void OnNewConnection(shared_ptr<NetworkServerConnection> net_connection);
    void ProcessNotLoggedInPlayer(ptr<Player> not_logged_in_player);
    void ProcessPlayer(ptr<Player> player);
    void ProcessConnection(ptr<Player> player);
    void HandleOutboundRemoteCall(hstring name, ptr<Entity> caller, const_span<uint8_t> data) override;

    void LoadReportedHashes();
    void RegisterClientReportedHash(ptr<ServerConnection> connection, hstring::hash_t hash);
    void ProcessPendingUnresolvedHash(ptr<ServerConnection> connection);
    void SendAllReportedHashes(ptr<Player> player);
    void BroadcastReportedString(string_view reported_string);

    auto FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult override;

    void Process_Handshake(ptr<Player> player);
    void Process_Ping(ptr<Player> player);
    void Process_UnresolvedHash(ptr<ServerConnection> connection);
    void Process_Move(ptr<Player> player);
    void Process_MoveLease(ptr<Player> player);
    void Process_StopMove(ptr<Player> player);
    void Process_MoveFinished(ptr<Player> player);
    void Process_Dir(ptr<Player> player);
    void Process_Property(ptr<Player> player);
    void Process_RemoteCall(ptr<Player> player);

    void OnSaveEntityValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSaveSynchronizedTime(ptr<Entity> entity, ptr<const Property> prop);

    void OnSendGlobalValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendPlayerValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendItemValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendCritterValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendMapValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendLocationValue(ptr<Entity> entity, ptr<const Property> prop);
    void OnSendCustomEntityValue(ptr<Entity> entity, ptr<const Property> prop);

    void OnSetCritterLookDistance(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetMapRemovedStaticItems(ptr<Entity> entity, ptr<const Property> prop, PropertyRawData& data);
    void OnPostSetMapRemovedStaticItems(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemHidden(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemRecacheHex(ptr<Entity> entity, ptr<const Property> prop);
    void OnSetItemMultihexLines(ptr<Entity> entity, ptr<const Property> prop);

    void ProcessCritterMovingBySteps(ptr<Critter> cr, ptr<Map> map);
    auto ReconcileCritterStopPosition(string_view request_name, ptr<Player> player, ptr<Critter> cr, ptr<Map> map, ptr<MovingContext> moving, mpos client_hex, ipos16 client_hex_offset, mdir client_dir) -> bool;
    auto MoveCritterAlongStopCorrectionPath(string_view request_name, ptr<Player> player, ptr<Critter> cr, ptr<Map> map, mpos target_hex, int32_t max_hex_distance) -> bool;
    auto MoveCritterToStopHex(ptr<Critter> cr, ptr<Map> map, mpos target_hex) -> bool;
    void SendCritterInitialInfo(ptr<Critter> cr, nptr<Critter> prev_cr);

    auto InitHealthFileJob() -> std::optional<timespan>;
    auto HealthFileJob() -> std::optional<timespan>;
    auto RpcWatchdogJob() -> std::optional<timespan>;
    auto HealthFileWriteJob(const string& health_info) -> std::optional<timespan>;
    auto WriteHealthFile(string_view text) -> bool;
    auto InitScriptSystemJob() -> std::optional<timespan>;
    auto InitNetworkingJob() -> std::optional<timespan>;
    auto InitStorageJob() -> std::optional<timespan>;
    auto InitMetadataJob() -> std::optional<timespan>;
    auto InitLanguageJob() -> std::optional<timespan>;
    auto InitMapsJob() -> std::optional<timespan>;
    auto InitClientPacksJob() -> std::optional<timespan>;
    auto InitGameLogicJob() -> std::optional<timespan>;
    auto InitDoneJob() -> std::optional<timespan>;
    auto SyncPointJob() -> std::optional<timespan>;
    auto FrameTimeJob() -> std::optional<timespan>;
    void UpdateJobStats(nanotime cur_time);
    void UpdateCpuStats(nanotime cur_time);
    auto CalculateBusyCpuLoad(uint64_t previous_idle, uint64_t current_idle, uint64_t previous_total, uint64_t current_total) noexcept -> float32_t;

    void OnTimeEventSchedule(refcount_ptr<Entity> entity, uint32_t event_id, timespan delay);
    auto TimeEventJob(ptr<Entity> entity, uint32_t event_id) -> std::optional<timespan>;
    void OnTimeEventCancel(uint32_t event_id);
    void OnPlayerConnected(ptr<Player> not_logged_in_player);
    void RequirePlayerPreLogin(ptr<Player> not_logged_in_player);
    auto NotLoggedInPlayerJob(ptr<Player> not_logged_in_player) -> std::optional<timespan>;
    void OnPlayerLoggedIn(ptr<Player> player, nptr<Player> not_logged_in_player);
    auto PlayerJob(ptr<Player> player) -> std::optional<timespan>;
    void RejectMalformedInput(ptr<ServerConnection> connection, const NetBufferException& ex);
    auto CritterMovingJob(ptr<Critter> cr) -> std::optional<timespan>;
    auto WrapJobWithSync(work_thread::job body) -> work_thread::job;
    void CountServerStatsJob() noexcept;

    work_thread _starter {"ServerStarter"};
    work_thread _mainWorker {"ServerWorker"};
    work_thread _healthWriter {"ServerHealthWriter"};
    work_thread _rpcWatchdog {"ServerRpcWatchdog"};
    mutex _rpcStatesLocker {};
    vector<weak_ptr<ServerConnection::RuntimeState>> _rpcStates FO_TSA_GUARDED_BY(_rpcStatesLocker) {};
    string _healthFileName {};
    optional<WorkerPool> _workerPool {};
    std::atomic<uint64_t> _completedServerStatsJobs {};

    synctime _persistedSyncTimeMark {};
    static constexpr auto SyncTimePersistLead = std::chrono::seconds {10};

    EntityLock _ownedLock {};
    mutable ptr<EntityLock> _entityLock {&_ownedLock};

    mutex _syncLocker {};
    std::condition_variable_any _syncWaitSignal {};
    std::condition_variable_any _syncRunSignal {};
    int32_t _syncRequest FO_TSA_GUARDED_BY(_syncLocker) {};
    bool _syncPointReady FO_TSA_GUARDED_BY(_syncLocker) {};

    std::atomic<size_t> _rejectedConnections {};
    std::atomic<size_t> _rejectedByRate {};
    ServerStats _stats {};
    optional<UpdaterBackend> _updaterBackend {};
    TextPack _defaultLang {make_ptr(&Hashes)};
    vector<unique_ptr<NetworkServer>> _connectionServers {};
    // Loaded before any listener starts and unchanged after, which is what lets their threads read it unlocked
    optional<SecureChannelIdentity> _channelIdentity {};
    mutable mutex _notLoggedInPlayersLocker {};
    vector<refcount_ptr<Player>> _notLoggedInPlayers FO_TSA_GUARDED_BY(_notLoggedInPlayersLocker) {};
    mutable mutex _connectionAdmissionLocker {};
    bool _connectionAdmissionOpen FO_TSA_GUARDED_BY(_connectionAdmissionLocker) {true};
    mutex _quiescenceLocker {};
    optional<ServerSnapshotRestore> _restoreSnapshot {};
    // Startup owns these on the starter thread; after _started, control is serialized by _quiescenceLocker
    std::atomic<bool> _simulationPaused {};
    bool _manualSimulationPause {};
    bool _startupSimulationPause {};
    unordered_set<string> _simulationHolds {};
    mutable mutex _snapshotEligibilityLocker {};
    vector<SnapshotEligibilityCheck> _snapshotEligibilityChecks FO_TSA_GUARDED_BY(_snapshotEligibilityLocker) {};
    mutable mutex _connRateLocker {};
    unordered_map<string, ConnRateState> _connRates FO_TSA_GUARDED_BY(_connRateLocker) {};

    mutable mutex _reportedHashesLocker {};
    unordered_set<string> _reportedStrings FO_TSA_GUARDED_BY(_reportedHashesLocker) {};
    unordered_set<hstring::hash_t> _unresolvableReportedHashes FO_TSA_GUARDED_BY(_reportedHashesLocker) {};

    EventDispatcher<> _willFinishDispatcher {&OnWillFinish};
    EventDispatcher<> _didFinishDispatcher {&OnDidFinish};
};

FO_END_NAMESPACE
