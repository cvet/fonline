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

auto GetServerResources(GlobalSettings& settings) -> FileSystem;

class ServerEngine final : public BaseEngine, public EntityManagerApi
{
    friend class ServerScriptSystem;

public:
    explicit ServerEngine(GlobalSettings& settings, FileSystem&& resources);

    ServerEngine(const ServerEngine&) = delete;
    ServerEngine(ServerEngine&&) noexcept = delete;
    auto operator=(const ServerEngine&) = delete;
    auto operator=(ServerEngine&&) noexcept = delete;
    ~ServerEngine() override;

    [[nodiscard]] auto GetEngine() noexcept -> ServerEngine* { return this; }
    [[nodiscard]] auto IsStarted() const noexcept -> bool { return _started; }
    [[nodiscard]] auto IsStartingError() const noexcept -> bool { return _startingError; }
    [[nodiscard]] auto IsShutdownInProgress() const noexcept -> bool { return _shutdownInProgress; }
    [[nodiscard]] auto GetHealthInfo() const -> string;
    [[nodiscard]] auto GetLangPack() const -> const TextPack& { return _defaultLang; }
    [[nodiscard]] auto GetCurrentSyncContext() const noexcept -> SyncContext* { return SyncContext::GetCurrentOnThisThread(); }
    [[nodiscard]] auto GetEntityLock() const noexcept -> EntityLock* { return _entityLock.get(); }
    [[nodiscard]] auto GetCompletedServerJobsCount() const -> uint64_t;

    void Shutdown() override;
    void FlushExactSyncTime();

    void LockForPropertyAccess() noexcept override;
    void UnlockForPropertyAccess() noexcept override;
    void LockForPropertyAccessShared() noexcept override;
    void UnlockForPropertyAccessShared() noexcept override;

    void ScheduleDelayedCallback(timespan delay, function<void()> body) override;

    auto CreateCustomInnerEntity(Entity* holder, hstring entry, hstring pid) -> Entity* override { return EntityMngr.CreateCustomInnerEntity(holder, entry, pid); }
    auto CreateCustomEntity(hstring type_name, hstring pid) -> Entity* override { return EntityMngr.CreateCustomEntity(type_name, pid); }
    auto GetCustomEntity(hstring type_name, ident_t id) -> refcount_ptr<Entity> override { return EntityMngr.GetCustomEntity(type_name, id); }
    void DestroyEntity(Entity* entity) override { EntityMngr.DestroyEntity(entity); }

    auto Lock(optional<timespan> max_wait_time) -> bool;
    void Unlock();
    void DrawGui();

    auto CreateUnloginedPlayer(shared_ptr<NetworkServerConnection> net_connection) -> Player*;
    auto LoginPlayerToNewRecord(Player* unlogined_player) -> Player*;
    auto LoginPlayerToExistentRecord(Player* unlogined_player, ident_t player_id) -> Player*;
    auto LoginPlayerToTempSession(Player* unlogined_player) -> Player*;

    auto CreateCritter(hstring pid, bool for_player, const Properties* props = nullptr) -> Critter*;
    auto LoadCritter(ident_t cr_id, bool for_player) -> Critter*;
    void UnloadCritter(Critter* cr);
    void UnloadCritterInnerEntities(Critter* cr);
    void SwitchPlayerCritter(Player* player, Critter* cr);
    void DestroyUnloadedCritter(ident_t cr_id);

    void StartCritterMoving(Critter* cr, refcount_ptr<MovingContext> moving, const Player* initiator);
    void StartCritterMoving(Critter* cr, uint16_t speed, const vector<mdir>& steps, const vector<uint16_t>& control_steps, ipos16 end_hex_offset, const Player* initiator);
    void StopCritterMoving(Critter* cr, MovingState reason = MovingState::Stopped, function<void()> customSend = nullptr);
    void ChangeCritterMovingSpeed(Critter* cr, uint16_t speed);

    ///@ ExportEvent
    FO_ENTITY_EVENT(OnInit);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGenerateWorld);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnStart);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnFinish);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerLogin, Player* /*player*/, FO_NULLABLE Player* /*unloginedPlayer*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerLogout, Player* /*player*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerCritterSwitched, Player* /*player*/, Critter* /*cr*/, Critter* /*prevCr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerMoveCritter, Player* /*player*/, Critter* /*cr*/, int32_t& /*speed*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnPlayerDirCritter, Player* /*player*/, Critter* /*cr*/, mdir& /*dir*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterMoved, Critter* /*cr*/, mpos /*oldHex*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterStartMoving, Critter* /*cr*/, bool /*wasMoving*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterStopMoving, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterTransfer, Critter* /*cr*/, FO_NULLABLE Map* /*prevMap*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGlobalMapCritterIn, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnGlobalMapCritterOut, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLocationInit, Location* /*location*/, bool /*firstTime*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnLocationFinish, Location* /*location*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapInit, Map* /*map*/, bool /*firstTime*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapFinish, Map* /*map*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapCritterIn, Map* /*map*/, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnMapCritterOut, Map* /*map*/, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterInit, Critter* /*cr*/, bool /*firstTime*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterFinish, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterLoad, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterUnload, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterSendInitialInfo, Critter* /*cr*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnCritterItemMoved, Critter* /*cr*/, Item* /*item*/, CritterItemSlot /*fromSlot*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemInit, Item* /*item*/, bool /*firstTime*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnItemFinish, Item* /*item*/);
    ///@ ExportEvent
    FO_ENTITY_EVENT(OnStaticItemWalk, StaticItem* /*item*/, Critter* /*cr*/, bool /*isIn*/, mdir /*dir*/);

    EntityManager EntityMngr;
    MapManager MapMngr;
    CritterManager CrMngr;
    ItemManager ItemMngr;

    DataBase DbStorage {};
    const hstring GameCollectionName = Hashes.ToHashedString("Game");
    const hstring HistoryCollectionName = Hashes.ToHashedString("History");
    const hstring PlayersCollectionName = Hashes.ToHashedString("Players");
    const hstring HashReportsCollectionName = Hashes.ToHashedString("HashReports");

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

        optional<Platform::CpuUsageSnapshot> LastCpuUsageSnapshot {};
        nanotime LastCpuUsageSampleTime {};
        bool CpuUsageAvailable {};
        float32_t CpuSystemLoad {};
        float32_t CpuProcessLoad {};
        float32_t CpuProcessCoreLoad {};
        vector<float32_t> CpuCoreLoads {};
    };

    void SyncPoint();

    void OnNewConnection(shared_ptr<NetworkServerConnection> net_connection);
    void ProcessUnloginedPlayer(Player* unlogined_player);
    void ProcessPlayer(Player* player);
    void ProcessConnection(ServerConnection* connection);
    void HandleOutboundRemoteCall(hstring name, Entity* caller, const_span<uint8_t> data) override;

    void LoadReportedHashes();
    void RegisterClientReportedHash(ServerConnection* connection, hstring::hash_t hash);
    void ProcessPendingUnresolvedHash(ServerConnection* connection);
    void SendAllReportedHashes(ServerConnection* connection);
    void BroadcastReportedString(string_view reported_string);

    auto FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult override;

    void Process_Handshake(ServerConnection* connection);
    void Process_Ping(ServerConnection* connection);
    void Process_UnresolvedHash(ServerConnection* connection);
    void Process_Move(Player* player);
    void Process_StopMove(Player* player);
    void Process_Dir(Player* player);
    void Process_Property(Player* player);
    void Process_RemoteCall(Player* player);

    void OnSaveEntityValue(Entity* entity, const Property* prop);
    void OnSaveSynchronizedTime(Entity* entity, const Property* prop);

    void OnSendGlobalValue(Entity* entity, const Property* prop);
    void OnSendPlayerValue(Entity* entity, const Property* prop);
    void OnSendItemValue(Entity* entity, const Property* prop);
    void OnSendCritterValue(Entity* entity, const Property* prop);
    void OnSendMapValue(Entity* entity, const Property* prop);
    void OnSendLocationValue(Entity* entity, const Property* prop);
    void OnSendCustomEntityValue(Entity* entity, const Property* prop);

    void OnSetCritterLookDistance(Entity* entity, const Property* prop);
    void OnSetItemCount(Entity* entity, const Property* prop, const void* new_value);
    void OnSetItemHidden(Entity* entity, const Property* prop);
    void OnSetItemRecacheHex(Entity* entity, const Property* prop);
    void OnSetItemMultihexLines(Entity* entity, const Property* prop);

    void ProcessCritterMovingBySteps(Critter* cr, Map* map);
    auto ReconcileCritterStopPosition(Player* player, Critter* cr, Map* map, mpos client_hex, ipos16 client_hex_offset, mdir client_dir) -> bool;
    auto MoveCritterAlongStopCorrectionPath(Player* player, Critter* cr, Map* map, mpos target_hex, int32_t max_hex_distance) -> bool;
    auto MoveCritterToStopHex(Critter* cr, Map* map, mpos target_hex) -> bool;
    void SendCritterInitialInfo(Critter* cr, Critter* prev_cr);

    auto InitHealthFileJob() -> std::optional<timespan>;
    auto HealthFileJob() -> std::optional<timespan>;
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
    auto TimeEventJob(Entity* entity, uint32_t event_id) -> std::optional<timespan>;
    void OnTimeEventCancel(uint32_t event_id);
    void OnPlayerConnected(Player* unlogined_player);
    auto UnloginedPlayerJob(Player* unlogined_player) -> std::optional<timespan>;
    void OnPlayerLogined(Player* player, Player* unlogined_player);
    auto PlayerJob(Player* player) -> std::optional<timespan>;
    auto CritterMovingJob(Critter* cr) -> std::optional<timespan>;
    auto WrapJobWithSync(WorkThread::Job body) -> WorkThread::Job;
    void CountServerStatsJob() noexcept;

    WorkThread _starter {"ServerStarter"};
    WorkThread _mainWorker {"ServerWorker"};
    WorkThread _healthWriter {"ServerHealthWriter"};
    string _healthFileName {};
    unique_ptr<WorkerPool> _workerPool {};
    std::atomic<uint64_t> _completedServerStatsJobs {};

    synctime _persistedSyncTimeMark {};
    static constexpr auto SyncTimePersistLead = std::chrono::seconds {10};

    EntityLock _ownedLock {};
    mutable raw_ptr<EntityLock> _entityLock {&_ownedLock};

    mutex _syncLocker {};
    std::condition_variable_any _syncWaitSignal {};
    std::condition_variable_any _syncRunSignal {};
    int32_t _syncRequest FO_TSA_GUARDED_BY(_syncLocker) {};
    bool _syncPointReady FO_TSA_GUARDED_BY(_syncLocker) {};

    std::atomic_bool _started {};
    std::atomic_bool _startingError {};
    std::atomic_bool _shutdownInProgress {};
    std::atomic<size_t> _rejectedConnections {};
    std::atomic<size_t> _rejectedByRate {};
    ServerStats _stats {};
    unique_ptr<UpdaterBackend> _updaterBackend {};
    TextPack _defaultLang {Hashes};
    vector<unique_ptr<NetworkServer>> _connectionServers {};
    mutable mutex _unloginedPlayersLocker {};
    vector<refcount_ptr<Player>> _unloginedPlayers FO_TSA_GUARDED_BY(_unloginedPlayersLocker) {};
    mutable mutex _connRateLocker {};
    unordered_map<string, ConnRateState> _connRates FO_TSA_GUARDED_BY(_connRateLocker) {};

    mutable mutex _reportedHashesLocker {};
    unordered_set<string> _reportedStrings FO_TSA_GUARDED_BY(_reportedHashesLocker) {};
    unordered_set<hstring::hash_t> _unresolvableReportedHashes FO_TSA_GUARDED_BY(_reportedHashesLocker) {};

    EventDispatcher<> _willFinishDispatcher {OnWillFinish};
    EventDispatcher<> _didFinishDispatcher {OnDidFinish};
};

FO_END_NAMESPACE
