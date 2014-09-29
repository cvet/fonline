#include "StdAfx.h"
#include "Server.h"

void* zlib_alloc( void* opaque, unsigned int items, unsigned int size ) { return calloc( items, size ); }
void  zlib_free( void* opaque, void* address )                          { free( address ); }

#define MAX_CLIENTS_IN_GAME    ( 3000 )

uint                        FOServer::CpuCount = 1;
int                         FOServer::UpdateIndex = -1;
int                         FOServer::UpdateLastIndex = -1;
uint                        FOServer::UpdateLastTick = 0;
ClVec                       FOServer::LogClients;
Thread                      FOServer::ListenThread;
SOCKET                      FOServer::ListenSock = INVALID_SOCKET;
#if defined ( USE_LIBEVENT )
event_base*                 FOServer::NetIOEventHandler = NULL;
Thread                      FOServer::NetIOThread;
uint                        FOServer::NetIOThreadsCount = 0;
#else // IOCP
HANDLE                      FOServer::NetIOCompletionPort = NULL;
Thread*                     FOServer::NetIOThreads = NULL;
uint                        FOServer::NetIOThreadsCount = 0;
#endif
ClVec                       FOServer::ConnectedClients;
Mutex                       FOServer::ConnectedClientsLocker;
FOServer::Statistics_       FOServer::Statistics;
FOServer::ClientSaveDataVec FOServer::ClientsSaveData;
size_t                      FOServer::ClientsSaveDataCount = 0;
PUCharVec                   FOServer::WorldSaveData;
size_t                      FOServer::WorldSaveDataBufCount = 0;
size_t                      FOServer::WorldSaveDataBufFreeSize = 0;
void*                       FOServer::DumpFile = NULL;
MutexEvent                  FOServer::DumpBeginEvent;
MutexEvent                  FOServer::DumpEndEvent;
uint                        FOServer::SaveWorldIndex = 0;
uint                        FOServer::SaveWorldTime = 0;
uint                        FOServer::SaveWorldNextTick = 0;
UIntVec                     FOServer::SaveWorldDeleteIndexes;
Thread                      FOServer::DumpThread;
Thread*                     FOServer::LogicThreads;
uint                        FOServer::LogicThreadCount = 0;
bool                        FOServer::LogicThreadSetAffinity = false;
bool                        FOServer::RequestReloadClientScripts = false;
LangPackVec                 FOServer::LangPacks;
FOServer::HoloInfoMap       FOServer::HolodiskInfo;
Mutex                       FOServer::HolodiskLocker;
uint                        FOServer::LastHoloId = 0;
FOServer::TimeEventVec      FOServer::TimeEvents;
uint                        FOServer::TimeEventsLastNum = 0;
Mutex                       FOServer::TimeEventsLocker;
FOServer::AnyDataMap        FOServer::AnyData;
Mutex                       FOServer::AnyDataLocker;
StrVec                      FOServer::ServerWrongGlobalObjects;
FOServer::TextListenVec     FOServer::TextListeners;
Mutex                       FOServer::TextListenersLocker;
bool                        FOServer::Active = false;
bool                        FOServer::ActiveInProcess = false;
bool                        FOServer::ActiveOnce = false;
ClVec                       FOServer::SaveClients;
Mutex                       FOServer::SaveClientsLocker;
UIntMap                     FOServer::RegIp;
Mutex                       FOServer::RegIpLocker;
uint                        FOServer::VarsGarbageLastTick = 0;
FOServer::ClientBannedVec   FOServer::Banned;
Mutex                       FOServer::BannedLocker;
FOServer::ClientDataVec     FOServer::ClientsData;
Mutex                       FOServer::ClientsDataLocker;
volatile uint               FOServer::LastClientId = 0;
ScoreType                   FOServer::BestScores[ SCORES_MAX ];
Mutex                       FOServer::BestScoresLocker;
FOServer::SingleplayerSave_ FOServer::SingleplayerSave;
MutexSynchronizer           FOServer::LogicThreadSync;
FOServer::UpdateFileVec     FOServer::UpdateFiles;
UCharVec                    FOServer::UpdateFilesList;
UIntUIntPairMap             FOServer::BruteForceIps;
StrUIntMap                  FOServer::BruteForceNames;

FOServer::FOServer()
{
    Active = false;
    ActiveInProcess = false;
    ActiveOnce = false;
    memzero( &Statistics, sizeof( Statistics ) );
    memzero( &ServerFunctions, sizeof( ServerFunctions ) );
    LastClientId = 0;
    SingleplayerSave.Valid = false;
    VarsGarbageLastTick = 0;
    RequestReloadClientScripts = false;
    MEMORY_PROCESS( MEMORY_STATIC, sizeof( FOServer ) );
}

FOServer::~FOServer()
{}

void FOServer::Finish()
{
    Active = false;
    ActiveInProcess = true;

    // World dumper
    if( WorldSaveManager )
    {
        DumpEndEvent.Wait();
        MEMORY_PROCESS( MEMORY_SAVE_DATA, -(int) ClientsSaveData.size() * sizeof( ClientSaveData ) );
        ClientsSaveData.clear();
        ClientsSaveDataCount = 0;
        MEMORY_PROCESS( MEMORY_SAVE_DATA, -(int) WorldSaveData.size() * WORLD_SAVE_DATA_BUFFER_SIZE );
        for( uint i = 0; i < WorldSaveData.size(); i++ )
            delete[] WorldSaveData[ i ];
        WorldSaveData.clear();
        WorldSaveDataBufCount = 0;
        WorldSaveDataBufFreeSize = 0;
        DumpThread.Finish();
    }
    if( DumpFile )
        FileClose( DumpFile );
    DumpFile = NULL;
    SaveWorldIndex = 0;
    SaveWorldTime = 0;
    SaveWorldNextTick = 0;

    // End script
    if( Script::PrepareContext( ServerFunctions.Finish, _FUNC_, "Game" ) )
        Script::RunPrepared();

    // Logging clients
    LogToFunc( &FOServer::LogToClients, false );
    for( auto it = LogClients.begin(), end = LogClients.end(); it != end; ++it )
        ( *it )->Release();
    LogClients.clear();

    // Clients
    ConnectedClientsLocker.Lock();
    for( auto it = ConnectedClients.begin(), end = ConnectedClients.end(); it != end; ++it )
    {
        Client* cl = *it;
        cl->Disconnect();
    }
    ConnectedClientsLocker.Unlock();

    // Listen
    shutdown( ListenSock, SD_BOTH );
    closesocket( ListenSock );
    ListenSock = INVALID_SOCKET;
    ListenThread.Wait();

    #if defined ( USE_LIBEVENT )
    // Net IO events
    event_base* eb = NetIOEventHandler;
    NetIOEventHandler = NULL;
    if( eb )
        event_base_loopbreak( eb );
    NetIOThread.Wait();
    if( eb )
        event_base_free( eb );
    NetIOThreadsCount = 0;
    #else // IOCP
    for( uint i = 0; i < NetIOThreadsCount; i++ )
        PostQueuedCompletionStatus( NetIOCompletionPort, 0, 1, NULL );
    for( uint i = 0; i < NetIOThreadsCount; i++ )
        NetIOThreads[ i ].Wait();
    SAFEDELA( NetIOThreads );
    NetIOThreadsCount = 0;
    CloseHandle( NetIOCompletionPort );
    NetIOCompletionPort = NULL;
    #endif

    // Managers
    AIMngr.Finish();
    MapMngr.Finish();
    CrMngr.Finish();
    ItemMngr.Finish();
    DlgMngr.Finish();
    VarMngr.Finish();
    FinishScriptSystem();
    FinishLangPacks();
    FileManager::ClearDataFiles();

    // Statistics
    WriteLog( "Server stopped.\n" );
    WriteLog( "Statistics:\n" );
    WriteLog( "Traffic:\n" );
    WriteLog( "Bytes Send: %u\n", Statistics.BytesSend );
    WriteLog( "Bytes Recv: %u\n", Statistics.BytesRecv );
    WriteLog( "Cycles count: %u\n", Statistics.LoopCycles );
    WriteLog( "Approx cycle period: %u\n", Statistics.LoopTime / ( Statistics.LoopCycles ? Statistics.LoopCycles : 1 ) );
    WriteLog( "Min cycle period: %u\n", Statistics.LoopMin );
    WriteLog( "Max cycle period: %u\n", Statistics.LoopMax );
    WriteLog( "Count of lags (>100ms): %u\n", Statistics.LagsCount );

    ActiveInProcess = false;
}

string FOServer::GetIngamePlayersStatistics()
{
    static string result;
    const char*   cond_states_str[] = { "None", "Life", "Knockout", "Dead" };
    char          str[ MAX_FOTEXT ];
    char          str_loc[ MAX_FOTEXT ];
    char          str_map[ MAX_FOTEXT ];

    ConnectedClientsLocker.Lock();
    uint conn_count = (uint) ConnectedClients.size();
    ConnectedClientsLocker.Unlock();

    ClVec players;
    CrMngr.GetCopyPlayers( players, false );

    Str::Format( str, "Players in game: %u\nConnections: %u\n", players.size(), conn_count );
    result = str;
    result += "Name                 Id        Ip              Online  Cond     X     Y     Location (Id, Pid)             Map (Id, Pid)                  Level\n";
    for( uint i = 0, j = (uint) players.size(); i < j; i++ )
    {
        Client*     cl = players[ i ];
        const char* name = cl->GetName();
        Map*        map = MapMngr.GetMap( cl->GetMap(), false );
        Location*   loc = ( map ? map->GetLocation( false ) : NULL );

        Str::Format( str_loc, "%s (%u, %u)", map ? loc->Proto->Name.c_str() : "", map ? loc->GetId() : 0, map ? loc->GetPid() : 0 );
        Str::Format( str_map, "%s (%u, %u)", map ? map->Proto->GetName() : "", map ? map->GetId() : 0, map ? map->GetPid() : 0 );
        Str::Format( str, "%-20s %-9u %-15s %-7s %-8s %-5u %-5u %-30s %-30s %-4d\n",
                     cl->GetName(), cl->GetId(), cl->GetIpStr(), cl->IsOffline() ? "No" : "Yes", cond_states_str[ cl->Data.Cond ],
                     map ? cl->GetHexX() : cl->Data.WorldX, map ? cl->GetHexY() : cl->Data.WorldY, map ? str_loc : "Global map", map ? str_map : "", cl->Data.Params[ ST_LEVEL ] );
        result += str;
    }
    return result;
}

// Accesses
void FOServer::GetAccesses( StrVec& client, StrVec& tester, StrVec& moder, StrVec& admin, StrVec& admin_names )
{
    client.clear();
    tester.clear();
    moder.clear();
    admin.clear();
    admin_names.clear();
    IniParser& cfg = IniParser::GetServerConfig();
    if( cfg.IsLoaded() )
    {
        char buf[ MAX_FOTEXT ];
        if( cfg.GetStr( "Access_client", "", buf ) )
            Str::ParseLine( buf, ' ', client, Str::ParseLineDummy );
        if( cfg.GetStr( "Access_tester", "", buf ) )
            Str::ParseLine( buf, ' ', tester, Str::ParseLineDummy );
        if( cfg.GetStr( "Access_moder", "", buf ) )
            Str::ParseLine( buf, ' ', moder, Str::ParseLineDummy );
        if( cfg.GetStr( "Access_admin", "", buf ) )
            Str::ParseLine( buf, ' ', admin, Str::ParseLineDummy );
        if( cfg.GetStr( "AccessNames_admin", "", buf ) )
            Str::ParseLine( buf, ' ', admin_names, Str::ParseLineDummy );
    }
}

void FOServer::DisconnectClient( Client* cl )
{
    // Manage saves, exit from game
    uint    id = cl->GetId();
    Client* cl_ = ( id ? CrMngr.GetPlayer( id, false ) : NULL );
    if( cl_ && cl_ == cl )
    {
        EraseSaveClient( cl->GetId() );
        AddSaveClient( cl );

        SETFLAG( cl->Flags, FCRIT_DISCONNECT );
        if( cl->GetMap() )
        {
            cl->SendA_Action( ACTION_DISCONNECT, 0, NULL );
        }
        else if( cl->GroupMove )
        {
            for( auto it = cl->GroupMove->CritMove.begin(), end = cl->GroupMove->CritMove.end(); it != end; ++it )
            {
                Critter* cr = *it;
                if( cr != cl )
                    cr->Send_CritterParam( cl, OTHER_FLAGS, cl->Flags );
            }
        }
    }
}

void FOServer::RemoveClient( Client* cl )
{
    uint    id = cl->GetId();
    Client* cl_ = ( id ? CrMngr.GetPlayer( id, false ) : NULL );
    if( cl_ && cl_ == cl )
    {
        cl->EventFinish( cl->Data.ClientToDelete );
        if( Script::PrepareContext( ServerFunctions.CritterFinish, _FUNC_, cl->GetInfo() ) )
        {
            Script::SetArgObject( cl );
            Script::SetArgBool( cl->Data.ClientToDelete );
            Script::RunPrepared();
        }

        if( cl->GetMap() )
        {
            Map* map = MapMngr.GetMap( cl->GetMap(), true );
            if( map )
            {
                MapMngr.EraseCrFromMap( cl, map, cl->GetHexX(), cl->GetHexY() );
                map->EraseCritterEvents( cl );
            }
        }
        else if( cl->GroupMove )
        {
            GlobalMapGroup* group = cl->GroupMove;
            group->EraseCrit( cl );
            if( cl == group->Rule )
            {
                for( auto it = group->CritMove.begin(), end = group->CritMove.end(); it != end; ++it )
                {
                    Critter* cr = *it;
                    MapMngr.GM_GroupStartMove( cr );
                }
            }
            else
            {
                for( auto it = group->CritMove.begin(), end = group->CritMove.end(); it != end; ++it )
                {
                    Critter* cr = *it;
                    cr->Send_RemoveCritter( cl );
                }

                Item* car = cl->GetItemCar();
                if( car && car->GetId() == group->CarId )
                {
                    group->CarId = 0;
                    MapMngr.GM_GroupSetMove( group, group->ToX, group->ToY, 0.0f );                // Stop others
                }
            }
            cl->GroupMove = NULL;
        }

        // Deferred saving
        EraseSaveClient( cl->GetId() );
        if( !cl->Data.ClientToDelete )
            AddSaveClient( cl );

        CrMngr.EraseCritter( cl );
        cl->IsNotValid = true;

        // Erase radios from collection
        ItemPtrVec items = cl->GetItemsNoLock();
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->IsRadio() )
                ItemMngr.RadioRegister( item, false );
        }

        // Full delete
        if( cl->Data.ClientToDelete )
        {
            SaveClientsLocker.Lock();
            ClientData* data = GetClientData( id );
            if( data )
                data->Clear();
            SaveClientsLocker.Unlock();

            cl->FullClear();
            DeleteClientFile( cl->Name );
        }

        Job::DeferredRelease( cl );
    }
    else
    {
        cl->IsNotValid = true;
    }
}

void FOServer::DeleteClientFile( const char* client_name )
{
    // Make old name
    char clients_path[ MAX_FOPATH ];
    FileManager::GetWritePath( "", PT_SERVER_CLIENTS, clients_path );
    char old_client_fname[ MAX_FOPATH ];
    Str::Format( old_client_fname, "%s%s.client", clients_path, client_name );

    // Make new name
    char new_client_fname[ MAX_FOPATH ];
    for( uint i = 0; ; i++ )
    {
        if( !i )
            Str::Format( new_client_fname, "%s%s.client_deleted", clients_path, client_name, i );
        else
            Str::Format( new_client_fname, "%s%s~%u.client_deleted", clients_path, client_name, i );
        if( FileExist( new_client_fname ) )
            continue;
        break;
    }

    // Rename
    if( !FileRename( old_client_fname, new_client_fname ) )
    {
        WriteLogF( _FUNC_, " - Fail to rename from<%s> to<%s>.\n", old_client_fname, new_client_fname );
        FileClose( FileOpen( "cache_fail", true ) );
    }
}

void FOServer::AddSaveClient( Client* cl )
{
    if( Singleplayer )
        return;

    SCOPE_LOCK( SaveClientsLocker );

    cl->AddRef();
    SaveClients.push_back( cl );
}

void FOServer::EraseSaveClient( uint crid )
{
    SCOPE_LOCK( SaveClientsLocker );

    for( auto it = SaveClients.begin(); it != SaveClients.end();)
    {
        Client* cl = *it;
        if( cl->GetId() == crid )
        {
            cl->Release();
            it = SaveClients.erase( it );
        }
        else
            ++it;
    }
}

void FOServer::MainLoop()
{
    if( !Started() )
        return;

    // Add general jobs
    for( int i = 0; i < TIME_EVENTS_PER_CYCLE; i++ )
        Job::PushBack( JOB_TIME_EVENTS );
    Job::PushBack( JOB_GARBAGE_ITEMS );
    Job::PushBack( JOB_GARBAGE_CRITTERS );
    Job::PushBack( JOB_GARBAGE_LOCATIONS );
    Job::PushBack( JOB_GARBAGE_VARS );
    Job::PushBack( JOB_DEFERRED_RELEASE );
    Job::PushBack( JOB_GAME_TIME );
    Job::PushBack( JOB_BANS );
    Job::PushBack( JOB_LOOP_SCRIPT );

    // Start logic threads
    WriteLog( "Starting logic threads, count<%u>.\n", LogicThreadCount );
    WriteLog( "***   Starting game loop   ***\n" );

    LogicThreads = new Thread[ LogicThreadCount ];
    for( uint i = 0; i < LogicThreadCount; i++ )
    {
        Job::PushBack( JOB_THREAD_SYNCHRONIZE );

        char thread_name[ MAX_FOTEXT ];
        if( LogicThreadCount > 1 )
            Str::Format( thread_name, "Logic%u", i );
        else
            Str::Format( thread_name, "Logic" );

        LogicThreads[ i ].Start( Logic_Work, thread_name );

        if( LogicThreadSetAffinity )
        {
            #if defined ( FO_WINDOWS )
            SetThreadAffinityMask( LogicThreads[ i ].GetWindowsHandle(), 1 << ( i % CpuCount ) );
            #elif defined ( FO_LINUX )
            cpu_set_t mask;
            CPU_ZERO( &mask );
            CPU_SET( i % CpuCount, &mask );
            sched_setaffinity( LogicThreads[ i ].GetPid(), sizeof( mask ), &mask );
            #elif defined ( FO_OSX )
            // Todo: https://developer.apple.com/library/mac/#releasenotes/Performance/RN-AffinityAPI/index.html
            #endif
        }
    }

    SyncManager* sync_mngr = SyncManager::GetForCurThread();
    sync_mngr->UnlockAll();
    while( !FOQuit )
    {
        // Synchronize point
        LogicThreadSync.SynchronizePoint();

        // Synchronize single player data
        #ifdef FO_WINDOWS
        if( Singleplayer )
        {
            // Check client process
            if( WaitForSingleObject( SingleplayerClientProcess, 0 ) == WAIT_OBJECT_0 )
            {
                FOQuit = true;
                break;
            }

            SingleplayerData.Refresh();

            if( SingleplayerData.Pause != Timer::IsGamePaused() )
            {
                SynchronizeLogicThreads();
                Timer::SetGamePause( SingleplayerData.Pause );
                ResynchronizeLogicThreads();
            }
        }
        #endif

        // World saver
        if( Timer::FastTick() >= SaveWorldNextTick )
        {
            SynchronizeLogicThreads();
            SaveWorld( NULL );
            SaveWorldNextTick = Timer::FastTick() + SaveWorldTime;
            ResynchronizeLogicThreads();
        }

        // Client script
        if( RequestReloadClientScripts )
        {
            SynchronizeLogicThreads();
            ReloadClientScripts();
            RequestReloadClientScripts = false;
            ResynchronizeLogicThreads();
        }

        // Statistics
        Statistics.Uptime = ( Timer::FastTick() - Statistics.ServerStartTick ) / 1000;

//		sync_mngr->UnlockAll();
        Thread::Sleep( 100 );
    }

    WriteLog( "***   Finishing game loop  ***\n" );

    // Stop logic threads
    for( uint i = 0; i < LogicThreadCount; i++ )
        Job::PushBack( JOB_THREAD_FINISH );
    for( uint i = 0; i < LogicThreadCount; i++ )
        LogicThreads[ i ].Wait();
    SAFEDELA( LogicThreads );

    // Erase all jobs
    for( int i = 0; i < JOB_COUNT; i++ )
        Job::Erase( i );

    // Finish all critters
    CrMap& critters = CrMngr.GetCrittersNoLock();
    for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;
        bool     to_delete = ( cr->IsPlayer() && ( (Client*) cr )->Data.ClientToDelete );

        cr->EventFinish( to_delete );
        if( Script::PrepareContext( ServerFunctions.CritterFinish, _FUNC_, cr->GetInfo() ) )
        {
            Script::SetArgObject( cr );
            Script::SetArgBool( to_delete );
            Script::RunPrepared();
        }

        if( to_delete )
        {
            cr->FullClear();
            DeleteClientFile( ( (Client*) cr )->Name );
        }
    }

    // Last process
    ProcessBans();
    Timer::ProcessGameTime();
    ItemMngr.ItemGarbager();
    CrMngr.CritterGarbager();
    MapMngr.LocationGarbager();
    Job::SetDeferredReleaseCycle( 0xFFFFFFFF );
    Job::ProcessDeferredReleasing();

    // Save
    SaveWorld( NULL );

    // Last unlock
    sync_mngr->UnlockAll();
}

void FOServer::SynchronizeLogicThreads()
{
    SyncManager* sync_mngr = SyncManager::GetForCurThread();
    sync_mngr->Suspend();
    LogicThreadSync.Synchronize( LogicThreadCount );
    sync_mngr->Resume();
}

void FOServer::ResynchronizeLogicThreads()
{
    LogicThreadSync.Resynchronize();
}

void FOServer::Logic_Work( void* data )
{
    Thread::Sleep( 10 );

    // Init scripts
    if( !Script::InitThread() )
        return;

    // Add sleep job for current thread
    Job::PushBack( Job( JOB_THREAD_LOOP, NULL, true ) );

    // Get synchronize manager
    SyncManager* sync_mngr = SyncManager::GetForCurThread();

    // Wait next threads initialization
    Thread::Sleep( 10 );

    // Cycle time
    uint cycle_tick = Timer::FastTick();

    // Word loop
    while( true )
    {
        sync_mngr->UnlockAll();
        Job job = Job::PopFront();

        if( job.Type == JOB_CLIENT )
        {
            Client* cl = (Client*) job.Data;
            SYNC_LOCK( cl );

            // Disconnect
            if( cl->IsOffline() )
            {
                #if defined ( USE_LIBEVENT )
                cl->Bout.Lock();
                bool empty = cl->Bout.IsEmpty();
                cl->Bout.Unlock();
                if( empty )
                    cl->Shutdown();
                #else
                if( InterlockedCompareExchange( &cl->NetIOOut->Operation, 0, 0 ) == WSAOP_FREE )
                {
                    cl->Shutdown();
                }
                #endif

                if( cl->GetOfflineTime() > 60 * 60 * 1000 )         // 1 hour
                {
                    WriteLogF( _FUNC_, " - Offline connection timeout, force shutdown. Ip<%s>, name<%s>.\n", cl->GetIpStr(), cl->GetName() );
                    cl->Shutdown();
                }
            }

            // Check for removing
            #if defined ( USE_LIBEVENT )
            if( cl->Sock == INVALID_SOCKET )
            #else
            if( cl->Sock == INVALID_SOCKET &&
                InterlockedCompareExchange( &cl->NetIOIn->Operation, 0, 0 ) == WSAOP_FREE &&
                InterlockedCompareExchange( &cl->NetIOOut->Operation, 0, 0 ) == WSAOP_FREE )
            #endif
            {
                DisconnectClient( cl );

                ConnectedClientsLocker.Lock();
                auto it = std::find( ConnectedClients.begin(), ConnectedClients.end(), cl );
                if( it != ConnectedClients.end() )
                {
                    ConnectedClients.erase( it );
                    Statistics.CurOnline--;
                }
                ConnectedClientsLocker.Unlock();

                Job::DeferredRelease( cl );
                continue;
            }

            // Process net
            Process( cl );
            if( (Client*) job.Data != cl )
                job.Data = cl;
        }
        else if( job.Type == JOB_CRITTER )
        {
            Critter* cr = (Critter*) job.Data;
            SYNC_LOCK( cr );

            // Player specific
            if( cr->CanBeRemoved )
                RemoveClient( (Client*) cr );                           // Todo: rework, add to garbage collector

            // Check for removing
            if( cr->IsNotValid )
                continue;

            // Process logic
            ProcessCritter( cr );
        }
        else if( job.Type == JOB_MAP )
        {
            Map* map = (Map*) job.Data;
            SYNC_LOCK( map );

            // Check for removing
            if( map->IsNotValid )
                continue;

            // Process logic
            map->Process();
        }
        else if( job.Type == JOB_TIME_EVENTS )
        {
            // Time events
            ProcessTimeEvents();
        }
        else if( job.Type == JOB_GARBAGE_ITEMS )
        {
            // Items garbage
            sync_mngr->PushPriority( 2 );
            ItemMngr.ItemGarbager();
            sync_mngr->PopPriority();
        }
        else if( job.Type == JOB_GARBAGE_CRITTERS )
        {
            // Critters garbage
            sync_mngr->PushPriority( 2 );
            CrMngr.CritterGarbager();
            sync_mngr->PopPriority();
        }
        else if( job.Type == JOB_GARBAGE_LOCATIONS )
        {
            // Locations and maps garbage
            sync_mngr->PushPriority( 2 );
            MapMngr.LocationGarbager();
            sync_mngr->PopPriority();
        }
        else if( job.Type == JOB_GARBAGE_VARS )
        {
            // Game vars garbage
            VarsGarbarger( false );
        }
        else if( job.Type == JOB_DEFERRED_RELEASE )
        {
            // Release pointers
            Job::ProcessDeferredReleasing();
        }
        else if( job.Type == JOB_GAME_TIME )
        {
            // Game time
            Timer::ProcessGameTime();
        }
        else if( job.Type == JOB_BANS )
        {
            // Bans
            ProcessBans();
        }
        else if( job.Type == JOB_LOOP_SCRIPT )
        {
            // Script game loop
            static uint game_loop_tick = 1;
            if( game_loop_tick && Timer::FastTick() >= game_loop_tick )
            {
                uint wait = 3600000;               // 1hour
                if( Script::PrepareContext( ServerFunctions.Loop, _FUNC_, "Game" ) && Script::RunPrepared() )
                    wait = Script::GetReturnedUInt();
                if( !wait )
                    game_loop_tick = 0;                     // Disable
                else
                    game_loop_tick = Timer::FastTick() + wait;
            }
        }
        else if( job.Type == JOB_THREAD_LOOP )
        {
            // Sleep
            uint sleep_time = Timer::FastTick();
            if( ServerGameSleep >= 0 )
                Thread::Sleep( ServerGameSleep );
            sleep_time = Timer::FastTick() - sleep_time;

            // Thread statistics
            // Manage threads data
            static Mutex  stats_locker;
            static PtrVec stats_ptrs;
            stats_locker.Lock();
            struct StatisticsThread
            {
                uint CycleTime;
                uint FPS;
                uint LoopTime;
                uint LoopCycles;
                uint LoopMin;
                uint LoopMax;
                uint LagsCount;
            } static THREAD* stats = NULL;
            if( !stats )
            {
                stats = new StatisticsThread();
                memzero( stats, sizeof( StatisticsThread ) );
                stats->LoopMin = MAX_UINT;
                stats_ptrs.push_back( stats );
            }

            // Fill statistics
            uint loop_tick = Timer::FastTick() - cycle_tick - sleep_time;
            stats->LoopTime += loop_tick;
            stats->LoopCycles++;
            if( loop_tick > stats->LoopMax )
                stats->LoopMax = loop_tick;
            if( loop_tick < stats->LoopMin )
                stats->LoopMin = loop_tick;
            stats->CycleTime = loop_tick;

            // Calculate whole threads statistics
            uint real_min_cycle = MAX_UINT;           // Calculate real cycle count for deferred releasing
            uint cycle_time = 0, loop_time = 0, loop_cycles = 0, loop_min = 0, loop_max = 0, lags = 0;
            for( auto it = stats_ptrs.begin(), end = stats_ptrs.end(); it != end; ++it )
            {
                StatisticsThread* stats_thread = (StatisticsThread*) *it;
                cycle_time += stats_thread->CycleTime;
                loop_time += stats_thread->LoopTime;
                loop_cycles += stats_thread->LoopCycles;
                loop_min += stats_thread->LoopMin;
                loop_max += stats_thread->LoopMax;
                lags += stats_thread->LagsCount;
                real_min_cycle = MIN( real_min_cycle, stats->LoopCycles );
            }
            uint count = (uint) stats_ptrs.size();
            Statistics.CycleTime = cycle_time / count;
            Statistics.LoopTime = loop_time / count;
            Statistics.LoopCycles = loop_cycles / count;
            Statistics.LoopMin = loop_min / count;
            Statistics.LoopMax = loop_max / count;
            Statistics.LagsCount = lags / count;
            stats_locker.Unlock();

            // Set real cycle count for deferred releasing
            Job::SetDeferredReleaseCycle( real_min_cycle );

            // Start time of next cycle
            cycle_tick = Timer::FastTick();
        }
        else if( job.Type == JOB_THREAD_SYNCHRONIZE )
        {
            // Threads synchronization
            LogicThreadSync.SynchronizePoint();
        }
        else if( job.Type == JOB_THREAD_FINISH )
        {
            // Exit from thread
            break;
        }
        else         // JOB_NOP
        {
            Thread::Sleep( 100 );
            continue;
        }

        // Add job to back
        uint job_count = Job::PushBack( job );

        // Calculate fps
        static volatile uint job_cur = 0;
        static volatile uint fps = 0;
        static volatile uint job_tick = 0;
        if( ++job_cur >= job_count )
        {
            job_cur = 0;
            fps++;
            if( !job_tick || Timer::FastTick() - job_tick >= 1000 )
            {
                Statistics.FPS = fps;
                fps = 0;
                job_tick = Timer::FastTick();
            }
        }
    }

    sync_mngr->UnlockAll();
    Script::FinishThread();
}

void FOServer::Net_Listen( void* )
{
    while( true )
    {
        // Blocked
        sockaddr_in from;
        #ifdef FO_WINDOWS
        socklen_t   addrsize = sizeof( from );
        SOCKET      sock = WSAAccept( ListenSock, (sockaddr*) &from, &addrsize, NULL, NULL );
        #else
        socklen_t   addrsize = sizeof( from );
        SOCKET      sock = accept( ListenSock, (sockaddr*) &from, &addrsize );
        #endif
        if( sock == INVALID_SOCKET )
        {
            // End of work
            #ifdef FO_WINDOWS
            int error = WSAGetLastError();
            if( error == WSAEINTR || error == WSAENOTSOCK )
                break;
            #else
            if( errno == EINVAL )
                break;
            #endif

            WriteLogF( _FUNC_, " - Listen error<%s>. Continue listening.\n", GetLastSocketError() );
            continue;
        }

        ConnectedClientsLocker.Lock();
        uint count = (uint) ConnectedClients.size();
        ConnectedClientsLocker.Unlock();
        if( count >= MAX_CLIENTS_IN_GAME )
        {
            closesocket( sock );
            continue;
        }

        if( GameOpt.DisableTcpNagle )
        {
            #ifdef FO_WINDOWS
            int optval = 1;
            if( setsockopt( sock, IPPROTO_TCP, TCP_NODELAY, (char*) &optval, sizeof( optval ) ) )
                WriteLogF( _FUNC_, " - Can't set TCP_NODELAY (disable Nagle) to socket, error<%s>.\n", GetLastSocketError() );
            #else
            // socklen_t optval = 1;
            // if( setsockopt( sock, IPPROTO_TCP, 1, &optval, sizeof( optval ) ) )
            //    WriteLogF( _FUNC_, " - Can't set TCP_NODELAY (disable Nagle) to socket, error<%s>.\n", GetLastSocketError() );
            #endif
        }

        Client* cl = new Client();

        // Socket
        cl->Sock = sock;
        cl->From = from;

        // ZLib
        cl->Zstrm.zalloc = zlib_alloc;
        cl->Zstrm.zfree = zlib_free;
        cl->Zstrm.opaque = NULL;
        int result = deflateInit( &cl->Zstrm, Z_DEFAULT_COMPRESSION );
        if( result != Z_OK )
        {
            WriteLogF( _FUNC_, " - Client Zlib deflateInit fail, error<%d, %s>.\n", result, zError( result ) );
            closesocket( sock );
            delete cl;
            continue;
        }
        cl->ZstrmInit = true;

        #if defined ( USE_LIBEVENT )
        // Net IO events handling
        bufferevent* bev = bufferevent_socket_new( NetIOEventHandler, sock, BEV_OPT_THREADSAFE );   // BEV_OPT_DEFER_CALLBACKS
        if( !bev )
        {
            WriteLogF( _FUNC_, " - Create new buffer event fail.\n" );
            closesocket( sock );
            delete cl;
            continue;
        }
        bufferevent_lock( bev );
        #else // IOCP
        // CompletionPort
        if( !CreateIoCompletionPort( (HANDLE) sock, NetIOCompletionPort, 0, 0 ) )
        {
            WriteLogF( _FUNC_, " - CreateIoCompletionPort fail, error<%u>.\n", GetLastError() );
            closesocket( sock );
            delete cl;
            continue;
        }

        // First receive queue
        DWORD bytes;
        if( WSARecv( cl->Sock, &cl->NetIOIn->Buffer, 1, &bytes, &cl->NetIOIn->Flags, &cl->NetIOIn->OV, NULL ) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING )
        {
            WriteLogF( _FUNC_, " - First recv fail, error<%s>.\n", GetLastSocketError() );
            closesocket( sock );
            delete cl;
            continue;
        }
        #endif

        // Add to connected collection
        ConnectedClientsLocker.Lock();
        cl->GameState = STATE_CONNECTED;
        ConnectedClients.push_back( cl );
        Statistics.CurOnline++;
        ConnectedClientsLocker.Unlock();

        #if defined ( USE_LIBEVENT )
        // Setup timeouts
        # if !defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
        timeval tv = { 0, 5 * 1000 };  // Check output data every 5ms
        bufferevent_set_timeouts( bev, NULL, &tv );
        # endif

        // Setup bandwidth
        const uint                  rate = 100000; // 100kb per second
        static ev_token_bucket_cfg* rate_cfg = NULL;
        if( !rate_cfg )
            rate_cfg = ev_token_bucket_cfg_new( rate, rate, rate, rate, NULL );
        if( !Singleplayer )
            bufferevent_set_rate_limit( bev, rate_cfg );

        // Setup callbacks
        Client::NetIOArg* arg = new Client::NetIOArg();
        arg->PClient = cl;
        arg->BEV = bev;
        cl->NetIOArgPtr = arg;
        cl->AddRef();         // Released in Shutdown
        bufferevent_setcb( bev, NetIO_Input, NetIO_Output, NetIO_Event, cl->NetIOArgPtr );

        // Begin handle net events
        bufferevent_enable( bev, EV_WRITE | EV_READ );
        bufferevent_unlock( bev );
        #endif

        // Add job
        Job::PushBack( JOB_CLIENT, cl );
    }
}

#if defined ( USE_LIBEVENT )

void FOServer::NetIO_Loop( void* )
{
    while( true )
    {
        event_base* eb = NetIOEventHandler;
        if( !eb )
            break;

        int result = event_base_loop( eb, 0 );

        // Return 1 if no events, wait some time and run loop again
        if( result == 1 )
        {
            Thread::Sleep( 10 );
            continue;
        }

        // Error
        if( result == -1 )
            WriteLogF( _FUNC_, " - Error.\n" );

        // End of work
        // event_base_loopbreak called
        break;
    }
}

void CheckThreadName()
{
    static uint thread_count = 0;
    const char* cur_name = Thread::GetCurrentName();
    if( !cur_name[ 0 ] )
    {
        char thread_name[ MAX_FOTEXT ];
        Str::Format( thread_name, "NetWork%u", thread_count++ );
        Thread::SetCurrentName( thread_name );
    }
}

void FOServer::NetIO_Event( bufferevent* bev, short what, void* arg )
{
    CheckThreadName();

    Client::NetIOArg* arg_ = (Client::NetIOArg*) arg;
    SCOPE_LOCK( arg_->Locker );
    Client*           cl = arg_->PClient;

    # if !defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
    if( ( what & ( BEV_EVENT_TIMEOUT | BEV_EVENT_WRITING ) ) == ( BEV_EVENT_TIMEOUT | BEV_EVENT_WRITING ) )
    {
        // Process offline
        if( cl->IsOffline() )
        {
            // If no data to send than shutdown
            bool is_empty = false;
            cl->Bout.Lock();
            if( cl->Bout.IsEmpty() )
                is_empty = true;
            cl->Bout.Unlock();
            if( is_empty )
            {
                cl->Shutdown();
                return;
            }

            // Disable reading
            if( bufferevent_get_enabled() & EV_READ )
                bufferevent_disable( bev, EV_READ );

            // Disable timeout
            bufferevent_set_timeouts( bev, NULL, NULL );
        }

        // Try send again
        bufferevent_enable( bev, EV_WRITE );
        NetIO_Output( bev, arg );
    }
    else if( what & ( BEV_EVENT_ERROR | BEV_EVENT_EOF ) )
    # else
    if( what & ( BEV_EVENT_ERROR | BEV_EVENT_EOF ) )
    # endif
    {
        // Shutdown on errors
        cl->Shutdown();
    }
}

void FOServer::NetIO_Input( bufferevent* bev, void* arg )
{
    CheckThreadName();

    Client::NetIOArg* arg_ = (Client::NetIOArg*) arg;
    SCOPE_LOCK( arg_->Locker );
    Client*           cl = arg_->PClient;

    if( cl->IsOffline() )
    {
        evbuffer* input = bufferevent_get_input( bev );
        uint      read_len = (uint) evbuffer_get_length( input );
        if( read_len )
            evbuffer_drain( input, read_len );
        bufferevent_disable( bev, EV_READ );
        return;
    }

    evbuffer* input = bufferevent_get_input( bev );
    uint      read_len = (uint) evbuffer_get_length( input );
    if( read_len )
    {
        cl->Bin.Lock();
        if( cl->Bin.GetCurPos() + read_len >= GameOpt.FloodSize && !Singleplayer )
        {
            WriteLogF( _FUNC_, " - Flood.\n" );
            cl->Disconnect();
            cl->Bin.Reset();
            cl->Bin.Unlock();
            cl->Shutdown();
            return;
        }

        cl->Bin.GrowBuf( read_len );
        char* data = cl->Bin.GetData();
        uint  pos = cl->Bin.GetEndPos();
        cl->Bin.SetEndPos( pos + read_len );

        if( evbuffer_remove( input, data + pos, read_len ) != (int) read_len )
        {
            WriteLogF( _FUNC_, " - Receive fail.\n" );
            cl->Bin.Unlock();
            cl->Shutdown();
        }
        else
        {
            cl->Bin.Unlock();
            Statistics.BytesRecv += read_len;
        }
    }
}

void FOServer::NetIO_Output( bufferevent* bev, void* arg )
{
    CheckThreadName();

    Client::NetIOArg* arg_ = (Client::NetIOArg*) arg;
    SCOPE_LOCK( arg_->Locker );
    Client*           cl = arg_->PClient;

    cl->Bout.Lock();
    if( cl->Bout.IsEmpty() )   // Nothing to send
    {
        cl->Bout.Unlock();
        if( cl->IsOffline() )
            cl->Shutdown();
        return;
    }

    // Compress
    const uint         output_buffer_len = 100000; // 100kb
    static THREAD char output_buffer[ output_buffer_len ];
    uint               write_len = 0;
    if( !GameOpt.DisableZlibCompression && !cl->DisableZlib )
    {
        uint to_compr = cl->Bout.GetEndPos();
        if( to_compr > output_buffer_len )
            to_compr = output_buffer_len;

        cl->Zstrm.next_in = (uchar*) cl->Bout.GetCurData();
        cl->Zstrm.avail_in = to_compr;
        cl->Zstrm.next_out = (uchar*) output_buffer;
        cl->Zstrm.avail_out = output_buffer_len;

        if( deflate( &cl->Zstrm, Z_SYNC_FLUSH ) != Z_OK )
        {
            WriteLogF( _FUNC_, " - Deflate fail.\n" );
            cl->Disconnect();
            cl->Bout.Reset();
            cl->Bout.Unlock();
            cl->Shutdown();
            return;
        }

        uint compr = cl->Zstrm.next_out - (uchar*) output_buffer;
        uint real = cl->Zstrm.next_in - (uchar*) cl->Bout.GetCurData();
        write_len = compr;
        cl->Bout.Cut( real );
        Statistics.DataReal += real;
        Statistics.DataCompressed += compr;
    }
    // Without compressing
    else
    {
        uint len = cl->Bout.GetEndPos();
        if( len > output_buffer_len )
            len = output_buffer_len;
        memcpy( output_buffer, cl->Bout.GetCurData(), len );
        write_len = len;
        cl->Bout.Cut( len );
        Statistics.DataReal += len;
        Statistics.DataCompressed += len;
    }
    if( cl->Bout.IsEmpty() )
        cl->Bout.Reset();
    cl->Bout.Unlock();

    // Append to buffer
    if( bufferevent_write( bev, output_buffer, write_len ) )
    {
        WriteLogF( _FUNC_, " - Send fail.\n" );
        cl->Shutdown();
    }
    else
    {
        Statistics.BytesSend += write_len;
    }
}

#else // IOCP

void FOServer::NetIO_Work( void* )
{
    while( true )
    {
        DWORD             bytes;
        uint              key;
        Client::NetIOArg* io;
        BOOL              ok = GetQueuedCompletionStatus( NetIOCompletionPort, &bytes, (PULONG_PTR) &key, (LPOVERLAPPED*) &io, INFINITE );
        if( key == 1 )
            break;                // End of work

        io->Locker.Lock();
        Client* cl = (Client*) io->PClient;
        cl->AddRef();

        if( ok && bytes )
        {
            switch( InterlockedCompareExchange( &io->Operation, 0, 0 ) )
            {
            case WSAOP_SEND:
                Statistics.BytesSend += bytes;
                NetIO_Output( io );
                break;
            case WSAOP_RECV:
                Statistics.BytesRecv += bytes;
                io->Bytes = bytes;
                NetIO_Input( io );
                break;
            default:
                WriteLogF( _FUNC_, " - Unknown operation<%d>, is send<%d>.\n", io->Operation, io == cl->NetIOOut );
                InterlockedExchange( &io->Operation, WSAOP_FREE );
                cl->Disconnect();
                break;
            }
        }
        else
        {
            InterlockedExchange( &io->Operation, WSAOP_FREE );
            cl->Disconnect();
        }

        io->Locker.Unlock();
        cl->Release();
    }
}

void FOServer::NetIO_Input( Client::NetIOArg* io )
{
    Client* cl = (Client*) io->PClient;

    if( cl->IsOffline() )
    {
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        return;
    }

    cl->Bin.Lock();
    if( cl->Bin.GetCurPos() + io->Bytes >= GameOpt.FloodSize && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Flood.\n" );
        cl->Bin.Reset();
        cl->Bin.Unlock();
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        cl->Disconnect();
        return;
    }
    cl->Bin.Push( io->Buffer.buf, io->Bytes, true );
    cl->Bin.Unlock();

    io->Flags = 0;
    DWORD bytes;
    if( WSARecv( cl->Sock, &io->Buffer, 1, &bytes, &io->Flags, &io->OV, NULL ) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING )
    {
        WriteLogF( _FUNC_, " - Recv fail, error<%s>.\n", GetLastSocketError() );
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        cl->Disconnect();
    }
}

void FOServer::NetIO_Output( Client::NetIOArg* io )
{
    Client* cl = (Client*) io->PClient;

    // Nothing to send
    cl->Bout.Lock();
    if( cl->Bout.IsEmpty() )
    {
        cl->Bout.Unlock();
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        return;
    }

    // Compress
    if( !GameOpt.DisableZlibCompression && !cl->DisableZlib )
    {
        uint to_compr = cl->Bout.GetEndPos();
        if( to_compr > WSA_BUF_SIZE )
            to_compr = WSA_BUF_SIZE;

        cl->Zstrm.next_in = (Bytef*) cl->Bout.GetCurData();
        cl->Zstrm.avail_in = to_compr;
        cl->Zstrm.next_out = (Bytef*) io->Buffer.buf;
        cl->Zstrm.avail_out = WSA_BUF_SIZE;

        if( deflate( &cl->Zstrm, Z_SYNC_FLUSH ) != Z_OK )
        {
            WriteLogF( _FUNC_, " - Deflate fail.\n" );
            cl->Bout.Reset();
            cl->Bout.Unlock();
            InterlockedExchange( &io->Operation, WSAOP_FREE );
            cl->Disconnect();
            return;
        }

        uint compr = (uint) ( (size_t) cl->Zstrm.next_out - (size_t) io->Buffer.buf );
        uint real = (uint) ( (size_t) cl->Zstrm.next_in - (size_t) cl->Bout.GetCurData() );
        io->Buffer.len = compr;
        cl->Bout.Cut( real );
        Statistics.DataReal += real;
        Statistics.DataCompressed += compr;
    }
    // Without compressing
    else
    {
        uint len = cl->Bout.GetEndPos();
        if( len > WSA_BUF_SIZE )
            len = WSA_BUF_SIZE;
        memcpy( io->Buffer.buf, cl->Bout.GetCurData(), len );
        io->Buffer.len = len;
        cl->Bout.Cut( len );
        Statistics.DataReal += len;
        Statistics.DataCompressed += len;
    }
    if( cl->Bout.IsEmpty() )
        cl->Bout.Reset();
    cl->Bout.Unlock();

    // Send
    DWORD bytes;
    if( WSASend( cl->Sock, &io->Buffer, 1, &bytes, 0, (LPOVERLAPPED) io, NULL ) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING )
    {
        WriteLogF( _FUNC_, " - Send fail, error<%s>.\n", GetLastSocketError() );
        InterlockedExchange( &io->Operation, WSAOP_FREE );
        cl->Disconnect();
    }
}

#endif

void FOServer::Process( ClientPtr& cl )
{
    if( cl->IsOffline() || cl->IsNotValid )
    {
        cl->Bin.LockReset();
        return;
    }

    uint msg = 0;
    if( cl->GameState == STATE_CONNECTED )
    {
        BIN_BEGIN( cl );
        if( cl->Bin.IsEmpty() )
            cl->Bin.Reset();
        cl->Bin.Refresh();
        if( cl->Bin.NeedProcess() )
        {
            cl->Bin >> msg;

            uint tick = Timer::FastTick();
            switch( msg )
            {
            case 0xFFFFFFFF:
            {
                uint answer[ 4 ] = { CrMngr.PlayersInGame(), Statistics.Uptime, 0, 0 };
                BOUT_BEGIN( cl );
                cl->Bout.Push( (char*) answer, sizeof( answer ) );
                cl->DisableZlib = true;
                BOUT_END( cl );
                cl->Disconnect();
            }
                BIN_END( cl );
                break;
            case NETMSG_PING:
                Process_Ping( cl );
                BIN_END( cl );
                break;
            case NETMSG_LOGIN:
                if( GameOpt.GameServer )
                    Process_LogIn( cl );
                else
                    cl->Disconnect();
                BIN_END( cl );
                break;
            case NETMSG_CREATE_CLIENT:
                if( GameOpt.GameServer )
                    Process_CreateClient( cl );
                else
                    cl->Disconnect();
                BIN_END( cl );
                break;
            case NETMSG_SINGLEPLAYER_SAVE_LOAD:
                if( GameOpt.GameServer )
                    Process_SingleplayerSaveLoad( cl );
                else
                    cl->Disconnect();
                BIN_END( cl );
                break;
            case NETMSG_UPDATE:
                if( GameOpt.UpdateServer )
                    Process_Update( cl );
                else
                    cl->Disconnect();
                BIN_END( cl );
                break;
            case NETMSG_GET_UPDATE_FILE:
                if( GameOpt.UpdateServer )
                    Process_UpdateFile( cl );
                else
                    cl->Disconnect();
                BIN_END( cl );
                break;
            case NETMSG_GET_UPDATE_FILE_DATA:
                if( GameOpt.UpdateServer )
                    Process_UpdateFileData( cl );
                else
                    cl->Disconnect();
                BIN_END( cl );
                break;
            default:
                cl->Bin.SkipMsg( msg );
                BIN_END( cl );
                break;
            }

            cl->LastActivityTime = Timer::FastTick();
        }
        else
        {
            CHECK_IN_BUFF_ERROR( cl );
            BIN_END( cl );
        }

        if( cl->GameState == STATE_CONNECTED && cl->LastActivityTime && Timer::FastTick() - cl->LastActivityTime > PING_CLIENT_LIFE_TIME ) // Kick bot
        {
            WriteLogF( _FUNC_, " - Connection timeout, client kicked, maybe bot. Ip<%s>.\n", cl->GetIpStr() );
            cl->Disconnect();
        }
    }
    else if( cl->GameState == STATE_TRANSFERRING )
    {
        #define CHECK_BUSY                                                                                             \
            if( cl->IsBusy() && !Singleplayer ) { cl->Bin.MoveReadPos( -int( sizeof( msg ) ) ); BIN_END( cl ); return; \
            }

        BIN_BEGIN( cl );
        if( cl->Bin.IsEmpty() )
            cl->Bin.Reset();
        cl->Bin.Refresh();
        if( cl->Bin.NeedProcess() )
        {
            cl->Bin >> msg;

            uint tick = Timer::FastTick();
            switch( msg )
            {
            case NETMSG_PING:
                Process_Ping( cl );
                BIN_END( cl );
                break;
            case NETMSG_SEND_GIVE_MAP:
                CHECK_BUSY;
                Process_GiveMap( cl );
                BIN_END( cl );
                break;
            case NETMSG_SEND_LOAD_MAP_OK:
                CHECK_BUSY;
                Process_ParseToGame( cl );
                BIN_END( cl );
                break;
            default:
                cl->Bin.SkipMsg( msg );
                BIN_END( cl );
                break;
            }
        }
        else
        {
            CHECK_IN_BUFF_ERROR( cl );
            BIN_END( cl );
        }
    }
    else if( cl->GameState == STATE_PLAYING )
    {
        #define MESSAGES_PER_CYCLE    ( 5 )
        #define CHECK_BUSY_AND_LIFE                                                                                               \
            if( !cl->IsLife() )                                                                                                   \
                break; if( cl->IsBusy() && !Singleplayer ) { cl->Bin.MoveReadPos( -int( sizeof( msg ) ) ); BIN_END( cl ); return; \
            }
        #define CHECK_NO_GLOBAL \
            if( !cl->GetMap() ) \
                break;
            #define CHECK_IS_GLOBAL                  \
                if( cl->GetMap() || !cl->GroupMove ) \
                    break;
                #define CHECK_AP_MSG                                                                                                      \
                    uchar ap; cl->Bin >> ap; if( !Singleplayer ) { if( !cl->IsTurnBased() ) { if( ap > cl->GetParam( ST_ACTION_POINTS ) ) \
                                                                                                  break; if( (int) ap > cl->GetParam( ST_CURRENT_AP ) ) { cl->Bin.MoveReadPos( -int( sizeof( msg ) + sizeof( ap ) ) ); BIN_END( cl ); return; } } }
        #define CHECK_AP( ap )                                                                                     \
            if( !Singleplayer ) { if( !cl->IsTurnBased() ) { if( (int) ( ap ) > cl->GetParam( ST_ACTION_POINTS ) ) \
                                                                 break; if( (int) ( ap ) > cl->GetParam( ST_CURRENT_AP ) ) { cl->Bin.MoveReadPos( -int( sizeof( msg ) ) ); BIN_END( cl ); return; } } }
        #define CHECK_REAL_AP( ap )                                                                                             \
            if( !Singleplayer ) { if( !cl->IsTurnBased() ) { if( (int) ( ap ) > cl->GetParam( ST_ACTION_POINTS ) * AP_DIVIDER ) \
                                                                 break; if( (int) ( ap ) > cl->GetRealAp() ) { cl->Bin.MoveReadPos( -int( sizeof( msg ) ) ); BIN_END( cl ); return; } } }

        for( int i = 0; i < MESSAGES_PER_CYCLE; i++ )
        {
            BIN_BEGIN( cl );
            if( cl->Bin.IsEmpty() )
                cl->Bin.Reset();
            cl->Bin.Refresh();
            if( !cl->Bin.NeedProcess() )
            {
                CHECK_IN_BUFF_ERROR( cl );
                BIN_END( cl );
                break;
            }
            cl->Bin >> msg;

            uint tick = Timer::FastTick();
            switch( msg )
            {
            case NETMSG_PING:
            {
                Process_Ping( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_TEXT:
            {
                Process_Text( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_COMMAND:
            {
                static THREAD Client* cl_;
                struct LogCB
                {
                    static void Message( const char* str )
                    {
                        cl_->Send_Text( cl_, str, SAY_NETMSG );
                    }
                };
                cl_ = cl;
                Process_Command( cl->Bin, LogCB::Message, cl, NULL );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_LEVELUP:
            {
                Process_LevelUp( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_CRAFT_ASK:
            {
                Process_CraftAsk( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_CRAFT:
            {
                CHECK_BUSY_AND_LIFE;
                Process_Craft( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_DIR:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                Process_Dir( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_MOVE_WALK:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                CHECK_REAL_AP( cl->GetApCostCritterMove( false ) );
                Process_Move( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_MOVE_RUN:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                CHECK_REAL_AP( cl->GetApCostCritterMove( true ) );
                Process_Move( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_USE_ITEM:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_AP_MSG;
                Process_UseItem( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_PICK_ITEM:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                CHECK_AP( cl->GetApCostPickItem() );
                Process_PickItem( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_PICK_CRITTER:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                CHECK_AP( cl->GetApCostPickCritter() );
                Process_PickCritter( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_ITEM_CONT:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_AP( cl->GetApCostMoveItemContainer() );
                Process_ContainerItem( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_CHANGE_ITEM:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_AP_MSG;
                Process_ChangeItem( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_RATE_ITEM:
            {
                Process_RateItem( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_SORT_VALUE_ITEM:
            {
                Process_SortValueItem( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_USE_SKILL:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_AP( cl->GetApCostUseSkill() );
                Process_UseSkill( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_TALK_NPC:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                Process_Dialog( cl, false );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_SAY_NPC:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                Process_Dialog( cl, true );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_BARTER:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                Process_Barter( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_PLAYERS_BARTER:
            {
                CHECK_BUSY_AND_LIFE;
                Process_PlayersBarter( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_SCREEN_ANSWER:
            {
                Process_ScreenAnswer( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_REFRESH_ME:
            {
                cl->Send_LoadMap( NULL );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_COMBAT:
            {
                CHECK_BUSY_AND_LIFE;
                CHECK_NO_GLOBAL;
                Process_Combat( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_GET_INFO:
            {
                Map* map = MapMngr.GetMap( cl->GetMap(), false );
                cl->Send_GameInfo( map );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_GIVE_MAP:
            {
                CHECK_BUSY;
                Process_GiveMap( cl );
                BIN_END( cl );
                continue;
            }
            break;
            case NETMSG_SEND_GIVE_GLOBAL_INFO:
            {
                CHECK_IS_GLOBAL;
                Process_GiveGlobalInfo( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_RULE_GLOBAL:
            {
                CHECK_BUSY_AND_LIFE;
                Process_RuleGlobal( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_SET_USER_HOLO_STR:
            {
                CHECK_BUSY_AND_LIFE;
                Process_SetUserHoloStr( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_GET_USER_HOLO_STR:
            {
                CHECK_BUSY_AND_LIFE;
                Process_GetUserHoloStr( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_GET_SCORES:
            {
                CHECK_BUSY;
                Process_GetScores( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_RUN_SERVER_SCRIPT:
            {
                Process_RunServerScript( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_KARMA_VOTING:
            {
                Process_KarmaVoting( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SINGLEPLAYER_SAVE_LOAD:
            {
                Process_SingleplayerSaveLoad( cl );
                BIN_END( cl );
                continue;
            }
            default:
            {
                cl->Bin.SkipMsg( msg );
                BIN_END( cl );
                continue;
            }
            }

            cl->Bin.SkipMsg( msg );
            BIN_END( cl );
        }
    }
}

void FOServer::Process_Text( Client* cl )
{
    uint   msg_len = 0;
    uchar  how_say = 0;
    ushort len = 0;
    char   str[ UTF8_BUF_SIZE( MAX_CHAT_MESSAGE ) ];

    cl->Bin >> msg_len;
    cl->Bin >> how_say;
    cl->Bin >> len;
    CHECK_IN_BUFF_ERROR( cl );

    if( !len || len >= sizeof( str ) )
    {
        WriteLogF( _FUNC_, " - Buffer zero sized or too large, length<%u>. Disconnect.\n", len );
        cl->Disconnect();
        return;
    }

    cl->Bin.Pop( str, len );
    str[ len ] = '\0';
    CHECK_IN_BUFF_ERROR( cl );

    if( !cl->IsLife() && how_say >= SAY_NORM && how_say <= SAY_RADIO )
        how_say = SAY_WHISP;

    if( Str::Compare( str, cl->LastSay ) )
    {
        cl->LastSayEqualCount++;
        if( cl->LastSayEqualCount >= 10 )
        {
            WriteLogF( _FUNC_, " - Flood detected, client<%s>. Disconnect.\n", cl->GetInfo() );
            cl->Disconnect();
            return;
        }
        if( cl->LastSayEqualCount >= 3 )
            return;
    }
    else
    {
        Str::Copy( cl->LastSay, str );
        cl->LastSayEqualCount = 0;
    }

    UShortVec channels;

    switch( how_say )
    {
    case SAY_NORM:
    {
        if( cl->GetMap() )
            cl->SendAA_Text( cl->VisCr, str, SAY_NORM, true );
        else
            cl->SendAA_Text( cl->GroupMove->CritMove, str, SAY_NORM, true );
    }
    break;
    case SAY_SHOUT:
    {
        if( cl->GetMap() )
        {
            Map* map = MapMngr.GetMap( cl->GetMap() );
            if( !map )
                return;

            CrVec critters;
            map->GetCritters( critters, false );

            cl->SendAA_Text( critters, str, SAY_SHOUT, true );
        }
        else
        {
            cl->SendAA_Text( cl->GroupMove->CritMove, str, SAY_SHOUT, true );
        }
    }
    break;
    case SAY_EMOTE:
    {
        if( cl->GetMap() )
            cl->SendAA_Text( cl->VisCr, str, SAY_EMOTE, true );
        else
            cl->SendAA_Text( cl->GroupMove->CritMove, str, SAY_EMOTE, true );
    }
    break;
    case SAY_WHISP:
    {
        if( cl->GetMap() )
            cl->SendAA_Text( cl->VisCr, str, SAY_WHISP, true );
        else
            cl->Send_TextEx( cl->GetId(), str, len, SAY_WHISP, cl->IntellectCacheValue, true );
    }
    break;
    case SAY_SOCIAL:
    {
        return;
    }
    break;
    case SAY_RADIO:
    {
        if( cl->GetMap() )
            cl->SendAA_Text( cl->VisCr, str, SAY_WHISP, true );
        else
            cl->Send_TextEx( cl->GetId(), str, len, SAY_WHISP, cl->IntellectCacheValue, true );

        ItemMngr.RadioSendText( cl, str, len, true, 0, 0, channels );
        if( channels.empty() )
        {
            cl->Send_TextMsg( cl, STR_RADIO_CANT_SEND, SAY_NETMSG, TEXTMSG_GAME );
            return;
        }
    }
    break;
    default:
        return;
    }

    // Best score
    SetScore( SCORE_SPEAKER, cl, 1 );

    // Text listen
    int           listen_count = 0;
    int           licten_func_id[ 100 ];  // 100 calls per one message is enough
    ScriptString* licten_str[ 100 ];

    TextListenersLocker.Lock();

    if( how_say == SAY_RADIO )
    {
        for( uint i = 0; i < TextListeners.size(); i++ )
        {
            TextListen& tl = TextListeners[ i ];
            if( tl.SayType == SAY_RADIO && std::find( channels.begin(), channels.end(), tl.Parameter ) != channels.end() &&
                Str::CompareCaseCountUTF8( str, tl.FirstStr, tl.FirstStrLen ) )
            {
                licten_func_id[ listen_count ] = tl.FuncId;
                licten_str[ listen_count ] = ScriptString::Create( str );
                if( ++listen_count >= 100 )
                    break;
            }
        }
    }
    else
    {
        ushort pid = cl->GetProtoMap();
        for( uint i = 0; i < TextListeners.size(); i++ )
        {
            TextListen& tl = TextListeners[ i ];
            if( tl.SayType == how_say && tl.Parameter == pid && Str::CompareCaseCountUTF8( str, tl.FirstStr, tl.FirstStrLen ) )
            {
                licten_func_id[ listen_count ] = tl.FuncId;
                licten_str[ listen_count ] = ScriptString::Create( str );
                if( ++listen_count >= 100 )
                    break;
            }
        }
    }

    TextListenersLocker.Unlock();

    for( int i = 0; i < listen_count; i++ )
    {
        if( Script::PrepareContext( licten_func_id[ i ], _FUNC_, cl->GetInfo() ) )
        {
            Script::SetArgObject( cl );
            Script::SetArgObject( licten_str[ i ] );
            Script::RunPrepared();
        }
        licten_str[ i ]->Release();
    }
}

void FOServer::Process_Command( BufferManager& buf, void ( * logcb )( const char* ), Client* cl_, const char* admin_panel )
{
    uint  msg_len = 0;
    uchar cmd = 0;

    buf >> msg_len;
    buf >> cmd;

    bool allow_command = false;
    if( Script::PrepareContext( ServerFunctions.PlayerAllowCommand, _FUNC_, cl_ ? cl_->GetInfo() : "AdminPanel" ) )
    {
        ScriptString* sstr = ( cl_ ? NULL : ScriptString::Create( admin_panel ) );
        Script::SetArgObject( cl_ );
        Script::SetArgObject( sstr );
        Script::SetArgUChar( cmd );
        if( Script::RunPrepared() && Script::GetReturnedBool() )
            allow_command = true;
        if( sstr )
            sstr->Release();
    }

    if( !allow_command && !cl_ )
    {
        logcb( "Command refused by script." );
        return;
    }

    #define CHECK_ALLOW_COMMAND \
        if( !allow_command )    \
            return
    #define CHECK_ADMIN_PANEL    if( !cl_ ) { logcb( "Can't execute this command in admin panel." ); return; }
    switch( cmd )
    {
    case CMD_EXIT:
    {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        cl_->Disconnect();
    }
    break;
    case CMD_MYINFO:
    {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        char istr[ 1024 ];
        Str::Format( istr, "|0xFF00FF00 Name: |0xFFFF0000 %s"
                           "|0xFF00FF00 , Id: |0xFFFF0000 %u"
                           "|0xFF00FF00 , Access: ",
                     cl_->GetName(), cl_->GetId() );

        switch( cl_->Access )
        {
        case ACCESS_CLIENT:
            strcat( istr, "|0xFFFF0000 Client|0xFF00FF00 ." );
            break;
        case ACCESS_TESTER:
            strcat( istr, "|0xFFFF0000 Tester|0xFF00FF00 ." );
            break;
        case ACCESS_MODER:
            strcat( istr, "|0xFFFF0000 Moderator|0xFF00FF00 ." );
            break;
        case ACCESS_ADMIN:
            strcat( istr, "|0xFFFF0000 Administrator|0xFF00FF00 ." );
            break;
        default:
            break;
        }

        logcb( istr );
    }
    break;
    case CMD_GAMEINFO:
    {
        int info;
        buf >> info;

        CHECK_ALLOW_COMMAND;

        string result;
        switch( info )
        {
        case 0:
            result = Debugger::GetMemoryStatistics();
            break;
        case 1:
            result = GetIngamePlayersStatistics();
            break;
        case 2:
            result = MapMngr.GetLocationsMapsStatistics();
            break;
        case 3:
            result = GetTimeEventsStatistics();
            break;
        case 4:
            result = GetAnyDataStatistics();
            break;
        case 5:
            result = ItemMngr.GetItemsStatistics();
            break;
        default:
            break;
        }

        char str[ MAX_FOTEXT ];
        ConnectedClientsLocker.Lock();
        Str::Format( str, "Connections: %u, Players: %u, Npc: %u. FOServer machine uptime: %u min., FOServer uptime: %u min.",
                     ConnectedClients.size(), CrMngr.PlayersInGame(), CrMngr.NpcInGame(),
                     Timer::FastTick() / 1000 / 60, ( Timer::FastTick() - Statistics.ServerStartTick ) / 1000 / 60 );
        ConnectedClientsLocker.Unlock();
        result += str;

        const char* ptext = result.c_str();
        uint        text_begin = 0;
        char        buf[ MAX_FOTEXT ];
        for( uint i = 0, j = (uint) result.length(); i < j; i++ )
        {
            if( result[ i ] == '\n' )
            {
                uint len = i - text_begin;
                if( len )
                {
                    if( len >= MAX_FOTEXT )
                        len = MAX_FOTEXT - 1;
                    memcpy( buf, &ptext[ text_begin ], len );
                    buf[ len ] = 0;
                    logcb( buf );
                }
                text_begin = i + 1;
            }
        }
    }
    break;
    case CMD_CRITID:
    {
        char name[ UTF8_BUF_SIZE( MAX_NAME ) ];
        buf.Pop( name, sizeof( name ) );
        name[ sizeof( name ) - 1 ] = 0;

        CHECK_ALLOW_COMMAND;

        SaveClientsLocker.Lock();

        ClientData* cd = GetClientData( name );
        if( cd )
            logcb( Str::FormatBuf( "Client id is %u.", cd->ClientId ) );
        else
            logcb( "Client not found." );

        SaveClientsLocker.Unlock();
    }
    break;
    case CMD_MOVECRIT:
    {
        uint   crid;
        ushort hex_x;
        ushort hex_y;
        buf >> crid;
        buf >> hex_x;
        buf >> hex_y;

        CHECK_ALLOW_COMMAND;

        Critter* cr = CrMngr.GetCritter( crid, true );
        if( !cr )
        {
            logcb( "Critter not found." );
            break;
        }

        Map* map = MapMngr.GetMap( cr->GetMap(), true );
        if( !map )
        {
            logcb( "Critter is on global." );
            break;
        }

        if( hex_x >= map->GetMaxHexX() || hex_y >= map->GetMaxHexY() )
        {
            logcb( "Invalid hex position." );
            break;
        }

        if( MapMngr.Transit( cr, map, hex_x, hex_y, cr->GetDir(), 3, true ) )
            logcb( "Critter move success." );
        else
            logcb( "Critter move fail." );
    }
    break;
    case CMD_KILLCRIT:
    {
        uint crid;
        buf >> crid;

        CHECK_ALLOW_COMMAND;

        Critter* cr = CrMngr.GetCritter( crid, true );
        if( !cr )
        {
            logcb( "Critter not found." );
            break;
        }

        KillCritter( cr, ANIM2_DEAD_FRONT, NULL );
        logcb( "Critter is dead." );
    }
    break;
    case CMD_DISCONCRIT:
    {
        uint crid;
        buf >> crid;

        CHECK_ALLOW_COMMAND;

        if( cl_ && cl_->GetId() == crid )
        {
            logcb( "To kick yourself type <~exit>" );
            return;
        }

        Critter* cr = CrMngr.GetCritter( crid, true );
        if( !cr )
        {
            logcb( "Critter not found." );
            return;
        }

        if( !cr->IsPlayer() )
        {
            logcb( "Founded critter is not a player." );
            return;
        }

        Client* cl2 = (Client*) cr;
        if( cl2->GameState != STATE_PLAYING )
        {
            logcb( "Player is not in a game." );
            return;
        }

        cl2->Send_Text( cl2, "You are kicked from game.", SAY_NETMSG );
        cl2->Disconnect();

        logcb( "Player disconnected." );
    }
    break;
    case CMD_TOGLOBAL:
    {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if( !cl_->IsLife() )
        {
            logcb( "To global fail, only life none." );
            break;
        }

        if( MapMngr.TransitToGlobal( cl_, 0, 0, false ) == true )
            logcb( "To global success." );
        else
            logcb( "To global fail." );
    }
    break;
    case CMD_RESPAWN:
    {
        uint crid;
        buf >> crid;

        CHECK_ALLOW_COMMAND;

        Critter* cr = ( !crid ? cl_ : CrMngr.GetCritter( crid, true ) );
        if( !cr )
            logcb( "Critter not found." );
        else if( !cr->IsDead() )
            logcb( "Critter does not require respawn." );
        else
        {
            RespawnCritter( cr );
            logcb( "Respawn success." );
        }
    }
    break;
    case CMD_PARAM:
    {
        uint   crid;
        ushort param_num;
        int    param_val;
        buf >> crid;
        buf >> param_num;
        buf >> param_val;

        CHECK_ALLOW_COMMAND;

        Critter* cr = ( !crid ? cl_ : CrMngr.GetCritter( crid, true ) );
        if( cr )
        {
            if( param_num >= MAX_PARAMS )
            {
                logcb( "Wrong param number." );
                return;
            }

            cr->ChangeParam( param_num );
            cr->Data.Params[ param_num ] = param_val;
            logcb( "Done." );
        }
        else
        {
            logcb( "Critter not found." );
        }
    }
    break;
    case CMD_GETACCESS:
    {
        char name_access[ UTF8_BUF_SIZE( MAX_NAME ) ];
        char pasw_access[ UTF8_BUF_SIZE( 128 ) ];
        buf.Pop( name_access, sizeof( name_access ) );
        buf.Pop( pasw_access, sizeof( pasw_access ) );
        name_access[ sizeof( name_access ) - 1 ] = 0;
        pasw_access[ sizeof( pasw_access ) - 1 ] = 0;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        StrVec client, tester, moder, admin, admin_names;
        GetAccesses( client, tester, moder, admin, admin_names );

        int wanted_access = -1;
        if( Str::Compare( name_access, "client" ) && std::find( client.begin(), client.end(), pasw_access ) != client.end() )
            wanted_access = ACCESS_CLIENT;
        else if( Str::Compare( name_access, "tester" ) && std::find( tester.begin(), tester.end(), pasw_access ) != tester.end() )
            wanted_access = ACCESS_TESTER;
        else if( Str::Compare( name_access, "moder" ) && std::find( moder.begin(), moder.end(), pasw_access ) != moder.end() )
            wanted_access = ACCESS_MODER;
        else if( Str::Compare( name_access, "admin" ) && std::find( admin.begin(), admin.end(), pasw_access ) != admin.end() )
            wanted_access = ACCESS_ADMIN;

        bool allow = false;
        if( wanted_access != -1 && Script::PrepareContext( ServerFunctions.PlayerGetAccess, _FUNC_, cl_->GetInfo() ) )
        {
            ScriptString* pass = ScriptString::Create( pasw_access );
            Script::SetArgObject( cl_ );
            Script::SetArgUInt( wanted_access );
            Script::SetArgObject( pass );
            if( Script::RunPrepared() && Script::GetReturnedBool() )
                allow = true;
            pass->Release();
        }

        if( !allow )
        {
            logcb( "Access denied." );
            break;
        }

        cl_->Access = wanted_access;
        logcb( "Access changed." );
    }
    break;
    case CMD_ADDITEM:
    {
        ushort hex_x;
        ushort hex_y;
        ushort pid;
        uint   count;
        buf >> hex_x;
        buf >> hex_y;
        buf >> pid;
        buf >> count;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        Map* map = MapMngr.GetMap( cl_->GetMap() );
        if( !map || hex_x >= map->GetMaxHexX() || hex_y >= map->GetMaxHexY() )
        {
            logcb( "Wrong hexes or critter on global map." );
            return;
        }

        if( !CreateItemOnHex( map, hex_x, hex_y, pid, count ) )
        {
            logcb( "Item(s) not added." );
        }
        else
        {
            logcb( "Item(s) added." );
        }
    }
    break;
    case CMD_ADDITEM_SELF:
    {
        ushort pid;
        uint   count;
        buf >> pid;
        buf >> count;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if( ItemMngr.AddItemCritter( cl_, pid, count ) != NULL )
            logcb( "Item(s) added." );
        else
            logcb( "Item(s) added fail." );
    }
    break;
    case CMD_ADDNPC:
    {
        ushort hex_x;
        ushort hex_y;
        uchar  dir;
        ushort pid;
        buf >> hex_x;
        buf >> hex_y;
        buf >> dir;
        buf >> pid;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        Map* map = MapMngr.GetMap( cl_->GetMap() );
        Npc* npc = CrMngr.CreateNpc( pid, 0, NULL, 0, NULL, NULL, map, hex_x, hex_y, dir, true );
        if( !npc )
            logcb( "Npc not created." );
        else
            logcb( "Npc created." );
    }
    break;
    case CMD_ADDLOCATION:
    {
        ushort wx;
        ushort wy;
        ushort pid;
        buf >> wx;
        buf >> wy;
        buf >> pid;

        CHECK_ALLOW_COMMAND;

        Location* loc = MapMngr.CreateLocation( pid, wx, wy, 0 );
        if( !loc )
            logcb( "Location not created." );
        else
            logcb( "Location created." );
    }
    break;
    case CMD_RELOADSCRIPTS:
    {
        CHECK_ALLOW_COMMAND;
        if( Script::Profiler::IsActive() )
            return;

        SynchronizeLogicThreads();

        // Get config file
        FileManager scripts_cfg;
        if( scripts_cfg.LoadFile( SCRIPTS_LST, PT_SERVER_SCRIPTS ) )
        {
            // Reload script modules
            Script::Undef( NULL );
            Script::Define( "__SERVER" );
            Script::Define( "__VERSION %d", FONLINE_VERSION );
            if( Script::ReloadScripts( (char*) scripts_cfg.GetBuf(), "server", false ) )
                logcb( "Success." );
            else
                logcb( "Fail." );
        }
        else
        {
            logcb( "Scripts config file not found." );
        }

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_LOADSCRIPT:
    {
        char module_name[ MAX_SCRIPT_NAME + 1 ];
        buf.Pop( module_name, MAX_SCRIPT_NAME );
        module_name[ MAX_SCRIPT_NAME ] = 0;

        CHECK_ALLOW_COMMAND;
        if( Script::Profiler::IsActive() )
            return;

        if( !Str::Length( module_name ) )
        {
            logcb( "Fail, name length is zero." );
            break;
        }

        SynchronizeLogicThreads();

        if( Script::LoadScript( module_name, NULL, true ) )
        {
            int errors = Script::BindImportedFunctions();
            if( !errors )
                logcb( "Complete." );
            else
                logcb( Str::FormatBuf( "Complete, bind errors<%d>.", errors ) );
        }
        else
        {
            logcb( "Unable to load script." );
        }

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_RELOAD_CLIENT_SCRIPTS:
    {
        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        if( ReloadClientScripts() )
            logcb( "Reload client scripts success." );
        else
            logcb( "Reload client scripts fail." );

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_RUNSCRIPT:
    {
        char module_name[ MAX_SCRIPT_NAME + 1 ];
        char func_name[ MAX_SCRIPT_NAME + 1 ];
        uint param0, param1, param2;
        buf.Pop( module_name, MAX_SCRIPT_NAME );
        module_name[ MAX_SCRIPT_NAME ] = 0;
        buf.Pop( func_name, MAX_SCRIPT_NAME );
        func_name[ MAX_SCRIPT_NAME ] = 0;
        buf >> param0;
        buf >> param1;
        buf >> param2;

        CHECK_ALLOW_COMMAND;

        if( !Str::Length( module_name ) || !Str::Length( func_name ) )
        {
            logcb( "Fail, length is zero." );
            break;
        }

        if( !cl_ )
            SynchronizeLogicThreads();

        int bind_id = Script::Bind( module_name, func_name, "void %s(Critter&,int,int,int)", true );
        if( !bind_id )
        {
            if( !cl_ )
                ResynchronizeLogicThreads();
            logcb( "Fail, function not found." );
            break;
        }

        if( !Script::PrepareContext( bind_id, _FUNC_, cl_ ? cl_->GetInfo() : "AdminPanel" ) )
        {
            if( !cl_ )
                ResynchronizeLogicThreads();
            logcb( "Fail, prepare error." );
            break;
        }

        Script::SetArgObject( cl_ );
        Script::SetArgUInt( param0 );
        Script::SetArgUInt( param1 );
        Script::SetArgUInt( param2 );

        if( Script::RunPrepared() )
            logcb( "Run script success." );
        else
            logcb( "Run script fail." );

        if( !cl_ )
            ResynchronizeLogicThreads();
    }
    break;
    case CMD_RELOADLOCATIONS:
    {
        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        if( MapMngr.LoadLocationsProtos() )
            logcb( "Reload proto locations success." );
        else
            logcb( "Reload proto locations fail." );

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_LOADLOCATION:
    {
        ushort loc_pid;
        buf >> loc_pid;

        CHECK_ALLOW_COMMAND;

        if( !loc_pid || loc_pid >= MAX_PROTO_LOCATIONS )
        {
            logcb( "Invalid proto location pid." );
            break;
        }

        SynchronizeLogicThreads();

        IniParser city_txt;
        if( city_txt.LoadFile( "Locations.cfg", PT_SERVER_MAPS ) )
        {
            ProtoLocation ploc;
            if( !MapMngr.LoadLocationProto( city_txt, ploc, loc_pid ) )
                logcb( "Load proto location fail." );
            else
            {
                MapMngr.ProtoLoc[ loc_pid ] = ploc;
                logcb( "Load proto location success." );
            }
        }
        else
        {
            logcb( "Locations.cfg not found." );
        }

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_RELOADMAPS:
    {
        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        int fails = 0;
        for( int map_pid = 0; map_pid < MAX_PROTO_MAPS; map_pid++ )
        {
            if( MapMngr.ProtoMaps[ map_pid ].IsInit() )
            {
                ProtoMap    pmap;
                const char* map_name = MapMngr.ProtoMaps[ map_pid ].GetName();
                if( pmap.Init( map_pid, map_name, PT_SERVER_MAPS ) )
                {
                    MapMngr.ProtoMaps[ map_pid ].Clear();
                    MapMngr.ProtoMaps[ map_pid ] = pmap;
                }
                else
                {
                    fails++;
                }
            }
        }

        if( !fails )
            logcb( "Reload proto maps complete, without fails." );
        else
            logcb( "Reload proto maps complete, with fails." );

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_LOADMAP:
    {
        ushort map_pid;
        buf >> map_pid;

        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        if( map_pid > 0 && map_pid < MAX_PROTO_MAPS && MapMngr.ProtoMaps[ map_pid ].IsInit() )
        {
            ProtoMap    pmap;
            const char* map_name = MapMngr.ProtoMaps[ map_pid ].GetName();
            if( pmap.Init( map_pid, map_name, PT_SERVER_MAPS ) )
            {
                MapMngr.ProtoMaps[ map_pid ].Clear();
                MapMngr.ProtoMaps[ map_pid ] = pmap;

                logcb( "Load proto map success." );
            }
            else
            {
                logcb( "Load proto map fail." );
            }
        }
        else
        {
            logcb( "Invalid proto map pid." );
        }

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_REGENMAP:
    {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        // Check global
        if( !cl_->GetMap() )
        {
            logcb( "Only on local map." );
            return;
        }

        // Find map
        Map* map = MapMngr.GetMap( cl_->GetMap() );
        if( !map )
        {
            logcb( "Map not found." );
            return;
        }

        // Regenerate
        ushort hx = cl_->GetHexX();
        ushort hy = cl_->GetHexY();
        uchar  dir = cl_->GetDir();
        if( RegenerateMap( map ) )
        {
            // Transit to old position
            MapMngr.Transit( cl_, map, hx, hy, dir, 5, true );
            logcb( "Regenerate map success." );
        }
        else
        {
            logcb( "Regenerate map fail." );
        }
    }
    break;
    case CMD_RELOADDIALOGS:
    {
        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        DlgMngr.DialogsPacks.clear();
        DlgMngr.DlgPacksNames.clear();
        int errors = DlgMngr.LoadDialogs( DIALOGS_LST_NAME );

        InitLangPacks( LangPacks );
        InitLangPacksDialogs( LangPacks );
        logcb( Str::FormatBuf( "Dialogs reload done, errors<%d>.", errors ) );

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_LOADDIALOG:
    {
        char dlg_name[ 128 ];
        uint dlg_id;
        buf.Pop( dlg_name, 128 );
        buf >> dlg_id;
        dlg_name[ 127 ] = 0;

        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        FileManager fm;
        if( fm.LoadFile( Str::FormatBuf( "%s%s", dlg_name, DIALOG_FILE_EXT ), PT_SERVER_DIALOGS ) )
        {
            DialogPack* pack = DlgMngr.ParseDialog( dlg_name, dlg_id, (char*) fm.GetBuf() );
            if( pack )
            {
                DlgMngr.EraseDialogs( dlg_id );
                DlgMngr.EraseDialogs( string( dlg_name ) );

                if( DlgMngr.AddDialogs( pack ) )
                {
                    InitLangPacks( LangPacks );
                    InitLangPacksDialogs( LangPacks );
                    logcb( "Load dialog success." );
                }
                else
                {
                    logcb( "Unable to add dialog." );
                }
            }
            else
            {
                logcb( "Unable to parse dialog." );
            }
        }
        else
        {
            logcb( "File not found." );
        }

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_RELOADTEXTS:
    {
        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        LangPackVec lang_packs;
        if( InitLangPacks( lang_packs ) && InitLangPacksDialogs( lang_packs ) && InitCrafts( lang_packs ) )
        {
            LangPacks = lang_packs;
            logcb( "Reload texts success." );
        }
        else
        {
            lang_packs.clear();
            logcb( "Reload texts fail." );
        }

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_RELOADAI:
    {
        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        NpcAIMngr ai_mngr;
        if( ai_mngr.Init() )
        {
            AIMngr = ai_mngr;
            logcb( "Reload ai success." );
        }
        else
        {
            logcb( "Init AI manager fail." );
        }

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_CHECKVAR:
    {
        ushort tid_var;
        uchar  master_is_npc;
        uint   master_id;
        uint   slave_id;
        uchar  full_info;
        buf >> tid_var;
        buf >> master_is_npc;
        buf >> master_id;
        buf >> slave_id;
        buf >> full_info;

        CHECK_ALLOW_COMMAND;

        if( master_is_npc )
            master_id += NPC_START_ID - 1;
        GameVar* var = VarMngr.GetVar( tid_var, master_id, slave_id, true );
        if( !var )
        {
            logcb( "Var not found." );
            break;
        }

        if( !full_info )
        {
            logcb( Str::FormatBuf( "Value<%d>.", var->GetValue() ) );
        }
        else
        {
            TemplateVar* tvar = var->GetTemplateVar();
            logcb( Str::FormatBuf( "Value<%d>, Name<%s>, Start<%d>, MIN<%d>, Max<%d>.",
                                   var->GetValue(), tvar->Name.c_str(), tvar->StartVal, tvar->MinVal, tvar->MaxVal ) );
        }
    }
    break;
    case CMD_SETVAR:
    {
        ushort tid_var;
        uchar  master_is_npc;
        uint   master_id;
        uint   slave_id;
        int    value;
        buf >> tid_var;
        buf >> master_is_npc;
        buf >> master_id;
        buf >> slave_id;
        buf >> value;

        CHECK_ALLOW_COMMAND;

        if( master_is_npc )
            master_id += NPC_START_ID - 1;
        GameVar* var = VarMngr.GetVar( tid_var, master_id, slave_id, true );
        if( !var )
        {
            logcb( "Var not found." );
            break;
        }

        TemplateVar* tvar = var->GetTemplateVar();
        if( value < tvar->MinVal )
            logcb( "Incorrect new value. Less than minimum." );
        if( value > tvar->MaxVal )
            logcb( "Incorrect new value. Greater than maximum." );
        else
        {
            *var = value;
            logcb( "Var changed." );
        }
    }
    break;
    case CMD_SETTIME:
    {
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

        SetGameTime( multiplier, year, month, day, hour, minute, second );
        logcb( "Time changed." );
    }
    break;
    case CMD_BAN:
    {
        char name[ UTF8_BUF_SIZE( MAX_NAME ) ];
        char params[ UTF8_BUF_SIZE( MAX_NAME ) ];
        uint ban_hours;
        char info[ UTF8_BUF_SIZE( MAX_CHAT_MESSAGE ) ];
        buf.Pop( name, sizeof( name ) );
        name[ sizeof( name ) - 1 ] = 0;
        buf.Pop( params, sizeof( params ) );
        params[ sizeof( params ) - 1 ] = 0;
        buf >> ban_hours;
        buf.Pop( info, sizeof( info ) );
        info[ sizeof( info ) - 1 ] = 0;

        CHECK_ALLOW_COMMAND;

        SCOPE_LOCK( BannedLocker );

        if( Str::CompareCase( params, "list" ) )
        {
            if( Banned.empty() )
            {
                logcb( "Ban list empty." );
                return;
            }

            uint index = 1;
            for( auto it = Banned.begin(), end = Banned.end(); it != end; ++it )
            {
                ClientBanned& ban = *it;
                logcb( Str::FormatBuf( "--- %3u ---", index ) );
                if( ban.ClientName[ 0 ] )
                    logcb( Str::FormatBuf( "User: %s", ban.ClientName ) );
                if( ban.ClientIp )
                    logcb( Str::FormatBuf( "UserIp: %u", ban.ClientIp ) );
                logcb( Str::FormatBuf( "BeginTime: %u %u %u %u %u", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute ) );
                logcb( Str::FormatBuf( "EndTime: %u %u %u %u %u", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute ) );
                if( ban.BannedBy[ 0 ] )
                    logcb( Str::FormatBuf( "BannedBy: %s", ban.BannedBy ) );
                if( ban.BanInfo[ 0 ] )
                    logcb( Str::FormatBuf( "Comment: %s", ban.BanInfo ) );
                index++;
            }
        }
        else if( Str::CompareCase( params, "add" ) || Str::CompareCase( params, "add+" ) )
        {
            uint name_len = Str::LengthUTF8( name );
            if( name_len < MIN_NAME || name_len < GameOpt.MinNameLength || name_len > MAX_NAME || name_len > GameOpt.MaxNameLength || !ban_hours )
            {
                logcb( "Invalid arguments." );
                return;
            }

            Client*      cl_banned = CrMngr.GetPlayer( name, true );
            ClientBanned ban;
            memzero( &ban, sizeof( ban ) );
            Str::Copy( ban.ClientName, name );
            ban.ClientIp = ( cl_banned && strstr( params, "+" ) ? cl_banned->GetIp() : 0 );
            Timer::GetCurrentDateTime( ban.BeginTime );
            ban.EndTime = ban.BeginTime;
            Timer::ContinueTime( ban.EndTime, ban_hours * 60 * 60 );
            Str::Copy( ban.BannedBy, cl_ ? cl_->Name : admin_panel );
            Str::Copy( ban.BanInfo, info );

            Banned.push_back( ban );
            SaveBan( ban, false );
            logcb( "User banned." );

            if( cl_banned )
            {
                cl_banned->Send_TextMsg( cl_banned, STR_NET_BAN, SAY_NETMSG, TEXTMSG_GAME );
                cl_banned->Send_TextMsgLex( cl_banned, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban.GetBanLexems() );
                cl_banned->Disconnect();
            }
        }
        else if( Str::CompareCase( params, "delete" ) )
        {
            if( !Str::Length( name ) )
            {
                logcb( "Invalid arguments." );
                return;
            }

            bool resave = false;
            if( Str::CompareCase( name, "*" ) )
            {
                int index = (int) ban_hours - 1;
                if( index >= 0 && index < (int) Banned.size() )
                {
                    Banned.erase( Banned.begin() + index );
                    resave = true;
                }
            }
            else
            {
                for( auto it = Banned.begin(); it != Banned.end();)
                {
                    ClientBanned& ban = *it;
                    if( Str::CompareCaseUTF8( ban.ClientName, name ) )
                    {
                        SaveBan( ban, true );
                        it = Banned.erase( it );
                        resave = true;
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            if( resave )
            {
                SaveBans();
                logcb( "User unbanned." );
            }
            else
            {
                logcb( "User not found." );
            }
        }
        else
        {
            logcb( "Unknown option." );
        }
    }
    break;
    case CMD_DELETE_ACCOUNT:
    {
        char pass_hash[ PASS_HASH_SIZE ];
        buf.Pop( pass_hash, PASS_HASH_SIZE );

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if( memcmp( cl_->PassHash, pass_hash, PASS_HASH_SIZE ) )
        {
            logcb( "Invalid password." );
        }
        else
        {
            if( !cl_->Data.ClientToDelete )
            {
                cl_->Data.ClientToDelete = true;
                logcb( "Your account will be deleted after character exit from game." );
            }
            else
            {
                cl_->Data.ClientToDelete = false;
                logcb( "Deleting canceled." );
            }
        }
    }
    break;
    case CMD_CHANGE_PASSWORD:
    {
        char pass_hash[ PASS_HASH_SIZE ];
        char new_pass_hash[ PASS_HASH_SIZE ];
        buf.Pop( pass_hash, PASS_HASH_SIZE );
        buf.Pop( new_pass_hash, PASS_HASH_SIZE );

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if( memcmp( cl_->PassHash, pass_hash, PASS_HASH_SIZE ) )
        {
            logcb( "Invalid current password." );
        }
        else
        {
            SCOPE_LOCK( SaveClientsLocker );

            ClientData* data = GetClientData( cl_->GetId() );
            if( data )
            {
                memcpy( data->ClientPassHash, new_pass_hash, PASS_HASH_SIZE );
                memcpy( cl_->PassHash, new_pass_hash, PASS_HASH_SIZE );
                logcb( "Password changed." );
            }
        }
    }
    break;
    case CMD_DROP_UID:
    {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        ClientData* data = GetClientData( cl_->GetId() );
        if( data )
        {
            for( int i = 0; i < 5; i++ )
                data->UID[ i ] = 0;
            data->UIDEndTick = 0;
        }
        logcb( "UID dropped, you can relogin on another account without timeout." );
    }
    break;
    case CMD_LOG:
    {
        char flags[ 16 ];
        buf.Pop( flags, 16 );

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        int action = -1;
        if( flags[ 0 ] == '-' && flags[ 1 ] == '-' )
            action = 2;                                  // Detach all
        else if( flags[ 0 ] == '-' )
            action = 0;                                  // Detach current
        else if( flags[ 0 ] == '+' )
            action = 1;                                  // Attach current
        else
        {
            logcb( "Wrong flags. Valid is '+', '-', '--'." );
            return;
        }

        SynchronizeLogicThreads();

        LogToFunc( &FOServer::LogToClients, false );
        auto it = std::find( LogClients.begin(), LogClients.end(), cl_ );
        if( action == 0 && it != LogClients.end() )           // Detach current
        {
            logcb( "Detached." );
            cl_->Release();
            LogClients.erase( it );
        }
        else if( action == 1 && it == LogClients.end() )           // Attach current
        {
            logcb( "Attached." );
            cl_->AddRef();
            LogClients.push_back( cl_ );
        }
        else if( action == 2 )             // Detach all
        {
            logcb( "Detached all." );
            for( auto it_ = LogClients.begin(); it_ < LogClients.end(); ++it_ )
                ( *it_ )->Release();
            LogClients.clear();
        }
        if( !LogClients.empty() )
            LogToFunc( &FOServer::LogToClients, true );

        ResynchronizeLogicThreads();
    }
    break;
    default:
        logcb( "Unknown command." );
        break;
    }
}

void FOServer::SaveGameInfoFile()
{
    // Singleplayer info
    uint sp = ( SingleplayerSave.Valid ? SINGLEPLAYER_SAVE_LAST : 0 );
    AddWorldSaveData( &sp, sizeof( sp ) );
    if( SingleplayerSave.Valid )
    {
        // Critter data
        ClientSaveData& csd = SingleplayerSave.CrData;
        AddWorldSaveData( csd.Name, sizeof( csd.Name ) );
        AddWorldSaveData( &csd.Data, sizeof( csd.Data ) );
        AddWorldSaveData( &csd.DataExt, sizeof( csd.DataExt ) );
        uint te_count = (uint) csd.TimeEvents.size();
        AddWorldSaveData( &te_count, sizeof( te_count ) );
        if( te_count )
            AddWorldSaveData( &csd.TimeEvents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) );

        // Picture data
        uint pic_size = (uint) SingleplayerSave.PicData.size();
        AddWorldSaveData( &pic_size, sizeof( pic_size ) );
        if( pic_size )
            AddWorldSaveData( &SingleplayerSave.PicData[ 0 ], pic_size );
    }

    // Time
    AddWorldSaveData( &GameOpt.YearStart, sizeof( GameOpt.YearStart ) );
    AddWorldSaveData( &GameOpt.Year, sizeof( GameOpt.Year ) );
    AddWorldSaveData( &GameOpt.Month, sizeof( GameOpt.Month ) );
    AddWorldSaveData( &GameOpt.Day, sizeof( GameOpt.Day ) );
    AddWorldSaveData( &GameOpt.Hour, sizeof( GameOpt.Hour ) );
    AddWorldSaveData( &GameOpt.Minute, sizeof( GameOpt.Minute ) );
    AddWorldSaveData( &GameOpt.Second, sizeof( GameOpt.Second ) );
    AddWorldSaveData( &GameOpt.TimeMultiplier, sizeof( GameOpt.TimeMultiplier ) );

    // Scores
    AddWorldSaveData( &BestScores[ 0 ], sizeof( BestScores ) );
}

bool FOServer::LoadGameInfoFile( void* f )
{
    WriteLog( "Load game info..." );

    // Singleplayer info
    uint sp = 0;
    if( !FileRead( f, &sp, sizeof( sp ) ) )
        return false;
    if( sp != 0 )
    {
        WriteLog( "singleplayer..." );

        // Critter data
        ClientSaveData& csd = SingleplayerSave.CrData;
        csd.Clear();

        if( sp >= SINGLEPLAYER_SAVE_V2 )
        {
            if( !FileRead( f, csd.Name, sizeof( csd.Name ) ) )
                return false;
        }
        else
        {
            if( !FileRead( f, csd.Name, MAX_NAME + 1 ) )
                return false;
            for( char* name = csd.Name; *name; name++ )
                *name = ( ( ( *name >= 'A' && *name <= 'Z' ) || ( *name >= 'a' && *name <= 'z' ) ) ? *name : 'X' );
        }

        if( !FileRead( f, &csd.Data, sizeof( csd.Data ) ) )
            return false;
        if( !FileRead( f, &csd.DataExt, sizeof( csd.DataExt ) ) )
            return false;
        uint te_count = (uint) csd.TimeEvents.size();
        if( !FileRead( f, &te_count, sizeof( te_count ) ) )
            return false;
        if( te_count )
        {
            csd.TimeEvents.resize( te_count );
            if( !FileRead( f, &csd.TimeEvents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) ) )
                return false;
        }

        // Picture data
        uint pic_size = 0;
        if( !FileRead( f, &pic_size, sizeof( pic_size ) ) )
            return false;
        if( pic_size )
        {
            SingleplayerSave.PicData.resize( pic_size );
            if( !FileRead( f, &SingleplayerSave.PicData[ 0 ], pic_size ) )
                return false;
        }
    }
    SingleplayerSave.Valid = ( sp != 0 );

    // Time
    if( !FileRead( f, &GameOpt.YearStart, sizeof( GameOpt.YearStart ) ) )
        return false;
    if( !FileRead( f, &GameOpt.Year, sizeof( GameOpt.Year ) ) )
        return false;
    if( !FileRead( f, &GameOpt.Month, sizeof( GameOpt.Month ) ) )
        return false;
    if( !FileRead( f, &GameOpt.Day, sizeof( GameOpt.Day ) ) )
        return false;
    if( !FileRead( f, &GameOpt.Hour, sizeof( GameOpt.Hour ) ) )
        return false;
    if( !FileRead( f, &GameOpt.Minute, sizeof( GameOpt.Minute ) ) )
        return false;
    if( !FileRead( f, &GameOpt.Second, sizeof( GameOpt.Second ) ) )
        return false;
    if( !FileRead( f, &GameOpt.TimeMultiplier, sizeof( GameOpt.TimeMultiplier ) ) )
        return false;

    // Scores
    if( !FileRead( f, &BestScores[ 0 ], sizeof( BestScores ) ) )
        return false;

    WriteLog( "complete.\n" );
    return true;
}

void FOServer::InitGameTime()
{
    if( !GameOpt.TimeMultiplier )
    {
        if( Script::PrepareContext( ServerFunctions.GetStartTime, _FUNC_, "Game" ) )
        {
            Script::SetArgAddress( &GameOpt.TimeMultiplier );
            Script::SetArgAddress( &GameOpt.YearStart );
            Script::SetArgAddress( &GameOpt.Month );
            Script::SetArgAddress( &GameOpt.Day );
            Script::SetArgAddress( &GameOpt.Hour );
            Script::SetArgAddress( &GameOpt.Minute );
            Script::RunPrepared();
        }

        GameOpt.YearStart = CLAMP( GameOpt.YearStart, 1700, 30000 );
        GameOpt.Year = GameOpt.YearStart;
        GameOpt.Second = 0;
    }

    DateTime dt = { GameOpt.YearStart, 1, 0, 1, 0, 0, 0, 0 };
    uint64   start_ft;
    Timer::DateTimeToFullTime( dt, start_ft );
    GameOpt.YearStartFTHi = ( start_ft >> 32 ) & 0xFFFFFFFF;
    GameOpt.YearStartFTLo = start_ft & 0xFFFFFFFF;

    GameOpt.TimeMultiplier = CLAMP( GameOpt.TimeMultiplier, 1, 50000 );
    GameOpt.Year = CLAMP( GameOpt.Year, GameOpt.YearStart, GameOpt.YearStart + 130 );
    GameOpt.Month = CLAMP( GameOpt.Month, 1, 12 );
    GameOpt.Day = CLAMP( GameOpt.Day, 1, 31 );
    GameOpt.Hour = CLAMP( (short) GameOpt.Hour, 0, 23 );
    GameOpt.Minute = CLAMP( (short) GameOpt.Minute, 0, 59 );
    GameOpt.Second = CLAMP( (short) GameOpt.Second, 0, 59 );
    GameOpt.FullSecond = Timer::GetFullSecond( GameOpt.Year, GameOpt.Month, GameOpt.Day, GameOpt.Hour, GameOpt.Minute, GameOpt.Second );

    GameOpt.FullSecondStart = GameOpt.FullSecond;
    GameOpt.GameTimeTick = Timer::GameTick();
}

void FOServer::SetGameTime( int multiplier, int year, int month, int day, int hour, int minute, int second )
{
    SynchronizeLogicThreads();

    if( multiplier >= 1 && multiplier <= 50000 )
        GameOpt.TimeMultiplier = multiplier;
    if( year >= GameOpt.YearStart && year <= GameOpt.YearStart + 130 )
        GameOpt.Year = year;
    if( month >= 1 && month <= 12 )
        GameOpt.Month = month;
    if( day >= 1 && day <= 31 )
        GameOpt.Day = day;
    if( hour >= 0 && hour <= 23 )
        GameOpt.Hour = hour;
    if( minute >= 0 && minute <= 59 )
        GameOpt.Minute = minute;
    if( second >= 0 && second <= 59 )
        GameOpt.Second = second;
    GameOpt.FullSecond = Timer::GetFullSecond( GameOpt.Year, GameOpt.Month, GameOpt.Day, GameOpt.Hour, GameOpt.Minute, GameOpt.Second );
    GameOpt.FullSecondStart = GameOpt.FullSecond;
    GameOpt.GameTimeTick = Timer::GameTick();

    ConnectedClientsLocker.Lock();
    for( auto it = ConnectedClients.begin(), end = ConnectedClients.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( cl->IsOnline() )
            cl->Send_GameInfo( MapMngr.GetMap( cl->GetMap(), false ) );
    }
    ConnectedClientsLocker.Unlock();

    ResynchronizeLogicThreads();
}

bool FOServer::Init()
{
    ActiveOnce = true;
    Active = true;
    ActiveInProcess = true;
    Active = InitReal();
    ActiveInProcess = false;
    return Active;
}

bool FOServer::InitReal()
{
    WriteLog( "***   Starting initialization   ****\n" );

    FileManager::InitDataFiles( DIR_SLASH_SD );

    IniParser& cfg = IniParser::GetServerConfig();

    // Check the sizes of base types
    STATIC_ASSERT( sizeof( char ) == 1 );
    STATIC_ASSERT( sizeof( short ) == 2 );
    STATIC_ASSERT( sizeof( int ) == 4 );
    STATIC_ASSERT( sizeof( int64 ) == 8 );
    STATIC_ASSERT( sizeof( uchar ) == 1 );
    STATIC_ASSERT( sizeof( ushort ) == 2 );
    STATIC_ASSERT( sizeof( uint ) == 4 );
    STATIC_ASSERT( sizeof( uint64 ) == 8 );
    STATIC_ASSERT( sizeof( bool ) == 1 );
    #if defined ( FO_X86 )
    STATIC_ASSERT( sizeof( size_t ) == 4 );
    STATIC_ASSERT( sizeof( void* ) == 4 );
    #elif defined ( FO_X64 )
    STATIC_ASSERT( sizeof( size_t ) == 8 );
    STATIC_ASSERT( sizeof( void* ) == 8 );
    #endif

    // Critters parameters
    Critter::ParamsSendMsgLen = sizeof( Critter::ParamsSendCount );
    Critter::ParamsSendCount = 0;
    memzero( Critter::ParamsSendEnabled, sizeof( Critter::ParamsSendEnabled ) );
    memzero( Critter::ParamsChangeScript, sizeof( Critter::ParamsChangeScript ) );
    memzero( Critter::ParamsGetScript, sizeof( Critter::ParamsGetScript ) );
    memzero( Critter::SlotDataSendEnabled, sizeof( Critter::SlotDataSendEnabled ) );
    for( int i = 0; i < MAX_PARAMS; i++ )
        Critter::ParamsChosenSendMask[ i ] = uint( -1 );

    // Register dll script data
    struct CritterChangeParameter_
    {
        static void CritterChangeParameter( void* cr, uint index ) { ( (Critter*) cr )->ChangeParam( index ); } };
    GameOpt.CritterChangeParameter = &CritterChangeParameter_::CritterChangeParameter;
    GameOpt.CritterTypes = &CritType::GetRealCritType( 0 );

    // Cpu count
    #ifdef FO_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo( &si );
    CpuCount = si.dwNumberOfProcessors;
    #else
    CpuCount = sysconf( _SC_NPROCESSORS_ONLN );
    #endif

    // Generic
    ConnectedClients.clear();
    SaveClients.clear();
    RegIp.clear();
    LastHoloId = USER_HOLO_START_NUM;
    TimeEventsLastNum = 0;
    VarsGarbageLastTick = Timer::FastTick();

    // Profiler
    uint sample_time = cfg.GetInt( "ProfilerSampleInterval", 0 );
    uint profiler_mode = cfg.GetInt( "ProfilerMode", 0 );
    if( !profiler_mode )
        sample_time = 0;
    Script::Profiler::SetData( sample_time, ( ( profiler_mode & 1 ) != 0 ) ? 300000 : 0, ( ( profiler_mode & 2 ) != 0 ) );

    // Threading
    LogicThreadSetAffinity = cfg.GetInt( "LogicThreadSetAffinity", 0 ) != 0;
    LogicThreadCount = cfg.GetInt( "LogicThreadCount", 0 );
    if( sample_time )
        LogicThreadCount = 1;
    else if( !LogicThreadCount )
        LogicThreadCount = CpuCount;
    if( LogicThreadCount == 1 )
        Script::SetConcurrentExecution( false );
    LogicMT = ( LogicThreadCount != 1 );

    if( !Singleplayer )
    {
        // Reserve memory
        ConnectedClients.reserve( MAX_CLIENTS_IN_GAME );
        SaveClients.reserve( MAX_CLIENTS_IN_GAME );
    }

    ConstantsManager::Initialize( PT_SERVER_CONFIGS ); // Generate name of defines
    if( !InitScriptSystem() )
        return false;                                  // Script system
    if( !InitLangPacks( LangPacks ) )
        return false;                                  // Language packs
    if( !ReloadClientScripts() )
        return false;                                  // Client scripts, after language packs initialization
    if( GameOpt.GameServer && !Singleplayer && !LoadClientsData() )
        return false;
    if( !Singleplayer )
        LoadBans();

    // Managers
    if( !AIMngr.Init() )
        return false;                    // NpcAi manager
    if( !ItemMngr.Init() )
        return false;                    // Item manager
    if( !CrMngr.Init() )
        return false;                    // Critter manager
    if( !MapMngr.Init() )
        return false;                    // Map manager
    if( !VarMngr.Init() )
        return false;                    // Var Manager (only before dialog manager!)
    if( !DlgMngr.LoadDialogs( DIALOGS_LST_NAME ) )
        return false;                    // Dialog manager
    if( !InitLangPacksDialogs( LangPacks ) )
        return false;                    // Create FONPC.MSG, FODLG.MSG, need call after InitLangPacks and DlgMngr.LoadDialogs
    if( !InitCrafts( LangPacks ) )
        return false;                    // MrFixit
    if( !InitLangCrTypes( LangPacks ) )
        return false;                    // Critter types
    // if(!MapMngr.LoadRelief()) return false; // Global map relief

    // Prototypes
    if( !ItemMngr.LoadProtos() )
        return false;                          // Proto items
    if( !CrMngr.LoadProtos() )
        return false;                          // Proto critters
    if( !MapMngr.LoadLocationsProtos() )
        return false;                          // Proto locations and maps
    if( !ItemMngr.CheckProtoFunctions() )
        return false;                          // Check valid of proto functions

    // Initialization script
    Script::PrepareContext( ServerFunctions.Init, _FUNC_, "Game" );
    Script::RunPrepared();

    // Update files
    GenerateUpdateFiles( true );

    // World loading
    if( GameOpt.GameServer && !Singleplayer )
    {
        // Copy of data
        if( !LoadWorld( NULL ) )
            return false;

        // Try generate world if not exist
        if( !MapMngr.GetLocationsCount() && !NewWorld() )
            return false;

        // Clear unused variables
        VarsGarbarger( true );
    }

    // End of initialization
    Statistics.BytesSend = 0;
    Statistics.BytesRecv = 0;
    Statistics.DataReal = 1;
    Statistics.DataCompressed = 1;
    Statistics.ServerStartTick = Timer::FastTick();

    // Net
    #ifdef FO_WINDOWS
    WSADATA wsa;
    if( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) )
    {
        WriteLog( "WSAStartup error.\n" );
        return false;
    }
    #endif

    #ifdef FO_WINDOWS
    ListenSock = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
    #else
    ListenSock = socket( AF_INET, SOCK_STREAM, 0 );
    #endif

    ushort port;
    if( !Singleplayer )
    {
        port = cfg.GetInt( "Port", 4000 );
        if( GameOpt.UpdateServer && !GameOpt.GameServer )
        {
            ushort update_port = ( ushort ) Str::AtoI( Str::Substring( CommandLine, "-update" ) + Str::Length( "-update" ) );
            if( update_port )
                port = update_port;
        }
        WriteLog( "Starting server on port<%u>.\n", port );
    }
    else
    {
        port = 0;
        WriteLog( "Starting server on free port.\n", port );
    }

    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons( port );
    sin.sin_addr.s_addr = INADDR_ANY;

    if( bind( ListenSock, (sockaddr*) &sin, sizeof( sin ) ) == SOCKET_ERROR )
    {
        WriteLog( "Bind error.\n" );
        return false;
    }

    if( Singleplayer )
    {
        socklen_t namelen = sizeof( sin );
        if( getsockname( ListenSock, (sockaddr*) &sin, &namelen ) )
        {
            WriteLog( "Getsockname error.\n" );
            closesocket( ListenSock );
            return false;
        }
        WriteLog( "Taked port<%u>.\n", sin.sin_port );
    }

    if( listen( ListenSock, SOMAXCONN ) == SOCKET_ERROR )
    {
        WriteLog( "Listen error!" );
        closesocket( ListenSock );
        return false;
    }

    NetIOThreadsCount = cfg.GetInt( "NetWorkThread", 0 );
    if( !NetIOThreadsCount )
        NetIOThreadsCount = CpuCount;

    #if defined ( USE_LIBEVENT )
    // Net IO events initialization
    struct ELCB
    {
        static void Callback( int severity, const char* msg ) { WriteLog( "Libevent - severity<%d>, msg<%s>.\n", severity, msg ); }
    };
    struct EFCB
    {
        static void Callback( int err ) { WriteLog( "Libevent - error<%d>.\n", err ); }
    };
    event_set_log_callback( ELCB::Callback );
    event_set_fatal_callback( EFCB::Callback );

    // evthread_enable_lock_debuging();

    # ifdef FO_WINDOWS
    evthread_use_windows_threads();
    # else
    evthread_use_pthreads();
    # endif

    event_config* event_cfg = event_config_new();
    # ifdef FO_WINDOWS
    event_config_set_flag( event_cfg, EVENT_BASE_FLAG_STARTUP_IOCP );
    # endif
    event_config_set_num_cpus_hint( event_cfg, NetIOThreadsCount );
    NetIOEventHandler = event_base_new_with_config( event_cfg );
    event_config_free( event_cfg );
    if( !NetIOEventHandler )
    {
        WriteLog( "Can't create Net IO events handler.\n" );
        closesocket( ListenSock );
        return false;
    }
    NetIOThread.Start( NetIO_Loop, "NetLoop" );
    WriteLog( "Network IO threads started, count<%u>.\n", NetIOThreadsCount );

    // Listen
    ListenThread.Start( Net_Listen, "NetListen" );
    WriteLog( "Network listen thread started.\n" );

    # if defined ( LIBEVENT_TIMEOUTS_WORKAROUND )
    Client::SendData = &NetIO_Output;
    # endif
    #else // IOCP
    NetIOCompletionPort = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, NULL, NetIOThreadsCount );
    if( !NetIOCompletionPort )
    {
        WriteLogF( NULL, "Can't create IO Completion Port, error<%u>.\n", GetLastError() );
        shutdown( ListenSock, SD_BOTH );
        closesocket( ListenSock );
        return false;
    }

    WriteLogF( NULL, "Starting net listen thread.\n" );
    ListenThread.Start( Net_Listen, "NetListen" );

    WriteLogF( NULL, "Starting net work threads, count<%u>.\n", NetIOThreadsCount );
    NetIOThreads = new Thread[ NetIOThreadsCount ];
    for( uint i = 0; i < NetIOThreadsCount; i++ )
    {
        char thread_name[ MAX_FOTEXT ];
        if( NetIOThreadsCount > 1 )
            Str::Format( thread_name, "NetWork%u", i );
        else
            Str::Format( thread_name, "NetWork" );

        NetIOThreads[ i ].Start( NetIO_Work, thread_name );
    }

    Client::SendData = &NetIO_Output;
    #endif

    // Start script
    if( !Script::PrepareContext( ServerFunctions.Start, _FUNC_, "Game" ) || !Script::RunPrepared() || !Script::GetReturnedBool() )
    {
        WriteLogF( _FUNC_, " - Start script fail.\n" );
        shutdown( ListenSock, SD_BOTH );
        closesocket( ListenSock );
        return false;
    }

    // Process command line definitions
    const char*      cmd_line = CommandLine;
    asIScriptEngine* engine = Script::GetEngine();
    for( int i = 0, j = engine->GetGlobalPropertyCount(); i < j; i++ )
    {
        const char* name;
        int         type_id;
        bool        is_const;
        const char* config_group;
        void*       pointer;
        if( engine->GetGlobalPropertyByIndex( i, &name, NULL, &type_id, &is_const, &config_group, &pointer ) >= 0 )
        {
            const char* cmd_name = Str::Substring( cmd_line, name );
            if( cmd_name )
            {
                const char* type_decl = engine->GetTypeDeclaration( type_id );
                if( Str::Compare( type_decl, "bool" ) )
                {
                    *(bool*) pointer = atoi( cmd_name + Str::Length( name ) + 1 ) != 0;
                    WriteLog( "Global var<%s> changed to<%s>.\n", name, *(bool*) pointer ? "true" : "false" );
                }
                else if( Str::Compare( type_decl, "string" ) )
                {
                    *(string*) pointer = cmd_name + Str::Length( name ) + 1;
                    WriteLog( "Global var<%s> changed to<%s>.\n", name, ( *(string*) pointer ).c_str() );
                }
                else
                {
                    *(int*) pointer = atoi( cmd_name + Str::Length( name ) + 1 );
                    WriteLog( "Global var<%s> changed to<%d>.\n", name, *(int*) pointer );
                }
            }
        }
    }

    ScriptSystemUpdate();

    // World saver
    if( Singleplayer || !GameOpt.GameServer )
        WorldSaveManager = 0;                // Disable deferred saving
    if( WorldSaveManager )
    {
        DumpBeginEvent.Disallow();
        DumpEndEvent.Allow();
        DumpThread.Start( Dump_Work, "WorldSaveManager" );
    }
    SaveWorldTime = cfg.GetInt( "WorldSaveTime", 60 ) * 60 * 1000;
    SaveWorldNextTick = Timer::FastTick() + SaveWorldTime;

    Active = true;

    // Inform client about end of initialization
    #ifdef FO_WINDOWS
    if( Singleplayer )
    {
        if( !SingleplayerData.Lock() )
        {
            shutdown( ListenSock, SD_BOTH );
            closesocket( ListenSock );
            return false;
        }
        SingleplayerData.NetPort = sin.sin_port;
        SingleplayerData.Unlock();
    }
    #endif

    return true;
}

bool FOServer::InitCrafts( LangPackVec& lang_packs )
{
    WriteLog( "FixBoy load crafts...\n" );
    MrFixit.Finish();

    LanguagePack* main_lang = NULL;
    for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
    {
        LanguagePack& lang = *it;

        if( it == lang_packs.begin() )
        {
            if( !MrFixit.LoadCrafts( lang.Msg[ TEXTMSG_CRAFT ] ) )
            {
                WriteLogF( _FUNC_, " - Unable to load crafts from<%s>.\n", lang.NameStr );
                return false;
            }
            main_lang = &lang;
            continue;
        }

        CraftManager mr_fixit;
        if( !mr_fixit.LoadCrafts( lang.Msg[ TEXTMSG_CRAFT ] ) )
        {
            WriteLogF( _FUNC_, " - Unable to load crafts from<%s>.\n", lang.NameStr );
            return false;
        }

        if( !( MrFixit == mr_fixit ) )
        {
            WriteLogF( _FUNC_, " - Compare crafts fail. <%s>with<%s>.\n", main_lang->NameStr, lang.NameStr );
            return false;
        }
    }
    WriteLog( "FixBoy load crafts complete.\n" );
    return true;
}

bool FOServer::InitLangPacks( LangPackVec& lang_packs )
{
    WriteLog( "Loading language packs...\n" );

    IniParser& cfg = IniParser::GetServerConfig();
    uint       cur_lang = 0;

    while( true )
    {
        char cur_str_lang[ MAX_FOTEXT ];
        char lang_name[ MAX_FOTEXT ];
        Str::Format( cur_str_lang, "Language_%u", cur_lang );

        if( !cfg.GetStr( cur_str_lang, "", lang_name ) )
            break;

        if( Str::Length( lang_name ) != 4 )
        {
            WriteLog( "Language name not equal to four letters.\n" );
            return false;
        }

        uint pack_id = *(uint*) &lang_name;
        if( std::find( lang_packs.begin(), lang_packs.end(), pack_id ) != lang_packs.end() )
        {
            WriteLog( "Language pack<%u> is already initialized.\n", cur_lang );
            return false;
        }

        LanguagePack lang;
        if( !lang.LoadFromFiles( lang_name ) )
        {
            WriteLog( "Unable to init Language pack<%u>.\n", cur_lang );
            return false;
        }

        lang_packs.push_back( lang );
        cur_lang++;
    }

    WriteLog( "Loading language packs complete, loaded<%u> packs.\n", cur_lang );
    return cur_lang > 0;
}

bool FOServer::InitLangPacksDialogs( LangPackVec& lang_packs )
{
    // Parse dialog texts with one seed to prevent MSG numbers truncation
    Randomizer def_rnd = DefaultRandomizer;
    DefaultRandomizer.Generate( 666666 );

    for( auto it = DlgMngr.DialogsPacks.begin(), end = DlgMngr.DialogsPacks.end(); it != end; ++it )
    {
        DialogPack* pack = ( *it ).second;
        for( uint i = 0, j = (uint) pack->TextsLang.size(); i < j; i++ )
        {
            for( auto it_ = lang_packs.begin(), end_ = lang_packs.end(); it_ != end_; ++it_ )
            {
                LanguagePack& lang = *it_;
                if( pack->TextsLang[ i ] != lang.NameStr )
                    continue;

                FOMsg* msg = pack->Texts[ i ];
                FOMsg* msg_dlg = &lang.Msg[ TEXTMSG_DLG ];

                // Npc names, descriptions
                // 100000..10100000
                for( uint n = 100; n < 300; n += 10 )
                {
                    for( int l = 0, k = msg->Count( n ); l < k; l++ )
                        msg_dlg->AddStr( 100000 + pack->PackId * 1000 + n, msg->GetStr( n, l ) );
                }

                // Dialogs text
                // 100000000..999999999
                for( uint i_ = 0; i_ < pack->Dialogs.size(); i_++ )
                {
                    Dialog& dlg = pack->Dialogs[ i_ ];
                    if( dlg.TextId < 100000000 )
                        dlg.TextId = msg_dlg->AddStr( msg->GetStr( ( i_ + 1 ) * 1000 ) );
                    else
                        msg_dlg->AddStr( dlg.TextId, msg->GetStr( ( i_ + 1 ) * 1000 ) );
                    for( uint j_ = 0; j_ < dlg.Answers.size(); j_++ )
                    {
                        DialogAnswer& answ = dlg.Answers[ j_ ];
                        if( answ.TextId < 100000000 )
                            answ.TextId = msg_dlg->AddStr( msg->GetStr( ( i_ + 1 ) * 1000 + ( j_ + 1 ) * 10 ) );
                        else
                            msg_dlg->AddStr( answ.TextId, msg->GetStr( ( i_ + 1 ) * 1000 + ( j_ + 1 ) * 10 ) );
                    }
                }

                // Any texts
                // 1000000000..
                UIntStrMulMap& data = msg->GetData();
                auto           it__ = data.upper_bound( 99999999 );
                for( ; it__ != data.end(); ++it__ )
                    msg_dlg->AddStr( 1000000000 + pack->PackId * 100000 + ( ( *it__ ).first - 100000000 ), ( *it__ ).second );
            }
        }
    }

    // Restore default randomizer
    DefaultRandomizer = def_rnd;

    // Regenerate update files
    GenerateUpdateFiles();

    return true;
}

void FOServer::FinishLangPacks()
{
    WriteLog( "Finish lang packs...\n" );
    LangPacks.clear();
    WriteLog( "Finish lang packs complete.\n" );
}

bool FOServer::InitLangCrTypes( LangPackVec& lang_packs )
{
    FOMsg msg_crtypes;
    if( !CritType::InitFromFile( &msg_crtypes ) )
        return false;

    for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
    {
        LanguagePack& lang = *it;
        for( int i = 0; i < MAX_CRIT_TYPES; i++ )
            lang.Msg[ TEXTMSG_INTERNAL ].EraseStr( STR_INTERNAL_CRTYPE( i ) );
        lang.Msg[ TEXTMSG_INTERNAL ] += msg_crtypes;
    }

    // Regenerate update files
    GenerateUpdateFiles();

    return true;
}

#pragma MESSAGE("Clients logging may be not thread safe.")
void FOServer::LogToClients( const char* str )
{
    ushort str_len = Str::Length( str );
    if( str_len && str[ str_len - 1 ] == '\n' )
        str_len--;
    if( str_len )
    {
        for( auto it = LogClients.begin(); it < LogClients.end();)
        {
            Client* cl = *it;
            if( cl->IsOnline() )
            {
                cl->Send_TextEx( 0, str, str_len, SAY_NETMSG, 10, false );
                ++it;
            }
            else
            {
                cl->Release();
                it = LogClients.erase( it );
            }
        }
        if( LogClients.empty() )
            LogToFunc( &FOServer::LogToClients, false );
    }
}

uint FOServer::GetBanTime( ClientBanned& ban )
{
    DateTime time;
    Timer::GetCurrentDateTime( time );
    int      diff = Timer::GetTimeDifference( ban.EndTime, time ) / 60 + 1;
    return diff > 0 ? diff : 1;
}

void FOServer::ProcessBans()
{
    SCOPE_LOCK( BannedLocker );

    bool     resave = false;
    DateTime time;
    Timer::GetCurrentDateTime( time );
    for( auto it = Banned.begin(); it != Banned.end();)
    {
        DateTime& ban_time = ( *it ).EndTime;
        if( time.Year >= ban_time.Year && time.Month >= ban_time.Month && time.Day >= ban_time.Day &&
            time.Hour >= ban_time.Hour && time.Minute >= ban_time.Minute )
        {
            SaveBan( *it, true );
            it = Banned.erase( it );
            resave = true;
        }
        else
            ++it;
    }
    if( resave )
        SaveBans();
}

void FOServer::SaveBan( ClientBanned& ban, bool expired )
{
    SCOPE_LOCK( BannedLocker );

    FileManager fm;
    const char* fname = ( expired ? BANS_FNAME_EXPIRED : BANS_FNAME_ACTIVE );
    if( !fm.LoadFile( fname, PT_SERVER_BANS ) )
    {
        WriteLogF( _FUNC_, " - Can't open file<%s>.\n", FileManager::GetDataPath( fname, PT_SERVER_BANS ) );
        return;
    }
    fm.SwitchToWrite();

    fm.SetStr( "[Ban]\n" );
    if( ban.ClientName[ 0 ] )
        fm.SetStr( "User=%s\n", ban.ClientName );
    if( ban.ClientIp )
        fm.SetStr( "UserIp=%u\n", ban.ClientIp );
    fm.SetStr( "BeginTime=%u %u %u %u %u\n", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute );
    fm.SetStr( "EndTime=%u %u %u %u %u\n", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute );
    if( ban.BannedBy[ 0 ] )
        fm.SetStr( "BannedBy=%s\n", ban.BannedBy );
    if( ban.BanInfo[ 0 ] )
        fm.SetStr( "Comment=%s\n", ban.BanInfo );
    fm.SetStr( "\n" );

    if( !fm.SaveOutBufToFile( fname, PT_SERVER_BANS ) )
        WriteLogF( _FUNC_, " - Unable to save file<%s>.\n", FileManager::GetWritePath( fname, PT_SERVER_BANS ) );
}

void FOServer::SaveBans()
{
    SCOPE_LOCK( BannedLocker );

    FileManager fm;
    for( auto it = Banned.begin(), end = Banned.end(); it != end; ++it )
    {
        ClientBanned& ban = *it;
        fm.SetStr( "[Ban]\n" );
        if( ban.ClientName[ 0 ] )
            fm.SetStr( "User=%s\n", ban.ClientName );
        if( ban.ClientIp )
            fm.SetStr( "UserIp=%u\n", ban.ClientIp );
        fm.SetStr( "BeginTime=%u %u %u %u %u\n", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute );
        fm.SetStr( "EndTime=%u %u %u %u %u\n", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute );
        if( ban.BannedBy[ 0 ] )
            fm.SetStr( "BannedBy=%s\n", ban.BannedBy );
        if( ban.BanInfo[ 0 ] )
            fm.SetStr( "Comment=%s\n", ban.BanInfo );
        fm.SetStr( "\n" );
    }

    if( !fm.SaveOutBufToFile( BANS_FNAME_ACTIVE, PT_SERVER_BANS ) )
        WriteLogF( _FUNC_, " - Unable to save file<%s>.\n", FileManager::GetWritePath( BANS_FNAME_ACTIVE, PT_SERVER_BANS ) );
}

void FOServer::LoadBans()
{
    SCOPE_LOCK( BannedLocker );

    Banned.clear();
    Banned.reserve( 1000 );
    IniParser bans_txt;
    if( !bans_txt.LoadFile( BANS_FNAME_ACTIVE, PT_SERVER_BANS ) )
    {
        void* f = FileOpen( FileManager::GetWritePath( BANS_FNAME_ACTIVE, PT_SERVER_BANS ), true );
        if( f )
            FileClose( f );
        return;
    }

    char buf[ MAX_FOTEXT ];
    while( bans_txt.GotoNextApp( "Ban" ) )
    {
        ClientBanned ban;
        memzero( &ban, sizeof( ban ) );
        DateTime     time;
        memzero( &time, sizeof( time ) );
        if( bans_txt.GetStr( "Ban", "User", "", buf ) )
            Str::Copy( ban.ClientName, buf );
        ban.ClientIp = bans_txt.GetInt( "Ban", "UserIp", 0 );
        if( bans_txt.GetStr( "Ban", "BeginTime", "", buf ) && sscanf( buf, "%hu%hu%hu%hu%hu", &time.Year, &time.Month, &time.Day, &time.Hour, &time.Minute ) )
            ban.BeginTime = time;
        if( bans_txt.GetStr( "Ban", "EndTime", "", buf ) && sscanf( buf, "%hu%hu%hu%hu%hu", &time.Year, &time.Month, &time.Day, &time.Hour, &time.Minute ) )
            ban.EndTime = time;
        if( bans_txt.GetStr( "Ban", "BannedBy", "", buf ) )
            Str::Copy( ban.BannedBy, buf );
        if( bans_txt.GetStr( "Ban", "Comment", "", buf ) )
            Str::Copy( ban.BanInfo, buf );
        Banned.push_back( ban );
    }
    ProcessBans();
}

bool FOServer::LoadClientsData()
{
    WriteLog( "Indexing client data.\n" );
    LastClientId = 0;
    ClientsData.reserve( 10000 );

    bool success = false;
    char client_name[ MAX_FOPATH ];
    char clients_path[ MAX_FOPATH ];
    FileManager::GetReadPath( "", PT_SERVER_CLIENTS, clients_path );

    // Last id
    char  last_id_fname[ MAX_FOPATH ];
    Str::Format( last_id_fname, "%slast_id.txt", clients_path );
    void* last_id_file = FileOpen( last_id_fname, false );
    if( last_id_file )
    {
        uint  fsize = FileGetSize( last_id_file );
        char* buf = new char[ fsize + 1 ];
        if( FileRead( last_id_file, buf, fsize ) )
        {
            buf[ fsize ] = 0;
            sscanf( buf, "%u", &LastClientId );
        }
        FileClose( last_id_file );
        delete[] buf;
    }

    // Get cache for clients
    istrstream* cache_str = NULL;
    char*       cache_buf = NULL;
    char        cache_fname[ MAX_FOPATH ];
    Str::Format( cache_fname, "%sclients_list.txt", clients_path );

    if( FileExist( "cache_fail" ) )
    {
        FileDelete( "cache_fail" );
        FileDelete( cache_fname );
    }

    void* cache_file = FileOpen( cache_fname, false );
    if( cache_file )
    {
        uint  fsize = FileGetSize( cache_file );
        char* cache_buf = new char[ fsize + 1 ];
        if( fsize > 0 && FileRead( cache_file, cache_buf, fsize ) )
        {
            FileClose( cache_file );
            cache_buf[ fsize ] = 0;
            cache_str = new istrstream( cache_buf );
            cache_str->getline( client_name, sizeof( client_name ) );
            Str::EraseFrontBackSpecificChars( client_name );
            if( !client_name[ 0 ] )
            {
                SAFEDEL( cache_str );
                SAFEDELA( cache_buf );
            }
        }
        else
        {
            FileClose( cache_file );
            SAFEDELA( cache_buf );
        }
    }

    // Scan folder for names if no cache
    FIND_DATA   file_find_data;
    void*       file_find = NULL;
    FileManager file_cache_write;
    if( !cache_str )
    {
        // Find file
        file_find = FileFindFirst( clients_path, "client", file_find_data );
        if( !file_find )
        {
            WriteLog( "Clients data not found.\n" );
            return true;
        }
        Str::Copy( client_name, file_find_data.FileName );
    }

    // Index clients
    UIntSet id_already;
    bool    first_iteration = true;
    while( true )
    {
        // Next
        if( !first_iteration )
        {
            if( file_find )
            {
                file_cache_write.SetStr( client_name );
                file_cache_write.SetStr( "\n" );

                if( !FileFindNext( file_find, file_find_data ) )
                {
                    success = true;
                    break;
                }
                Str::Copy( client_name, file_find_data.FileName );
            }
            else
            {
                cache_str->getline( client_name, sizeof( client_name ) );
                Str::EraseFrontBackSpecificChars( client_name );
                if( cache_str->eof() || !client_name[ 0 ] )
                {
                    success = true;
                    break;
                }
            }
        }
        else
        {
            first_iteration = false;
        }

        // Take name from file title
        char        name[ MAX_FOPATH ];
        const char* ext = FileManager::GetExtension( client_name );
        if( !ext || !Str::CompareCase( ext, "client" ) )
        {
            WriteLog( "Wrong name<%s> of client file in cache. Cache file deleted, restart server.\n", client_name );
            break;
        }
        Str::Copy( name, client_name );
        name[ Str::Length( name ) - 7 ] = 0; // Erase extension, 7 == ".client"

        // Take id and password from file
        char  client_fname[ MAX_FOPATH ];
        Str::Format( client_fname, "%s%s", clients_path, client_name );
        void* f = FileOpen( client_fname, false );
        if( !f )
        {
            if( !file_find && !FileExist( client_fname ) ) // Client deleted
                continue;
            WriteLog( "Unable to open client save file<%s>.\n", client_fname );
            break;
        }

        // Header - signature, password and id
        char header[ 4 + PASS_HASH_SIZE + sizeof( uint ) ];
        if( !FileRead( f, header, sizeof( header ) ) )
        {
            WriteLog( "Unable to read header of client save file<%s>. Skipped.\n", client_fname );
            FileClose( f );
            continue;
        }
        FileClose( f );

        // Check client save version
        int version = header[ 3 ];
        if( !( header[ 0 ] == 'F' && header[ 1 ] == 'O' && header[ 2 ] == 0 && version >= CLIENT_SAVE_V2 ) )
        {
            WriteLog( "Save file<%s> outdated, please run DataPatcher and type 'patchSaves'.\n", client_fname );
            break;
        }

        // Get id and password hash
        uint id;
        char pass_hash[ PASS_HASH_SIZE ];
        memcpy( pass_hash, header + 4, PASS_HASH_SIZE );
        memcpy( &id, header + 4 + PASS_HASH_SIZE, sizeof( uint ) );

        // Check id
        if( !IS_USER_ID( id ) )
        {
            WriteLog( "Wrong id<%u> of client<%s>. Skipped.\n", id, name );
            continue;
        }

        // Check uniqueness of id
        if( id_already.count( id ) )
        {
            WriteLog( "Id<%u> of user<%s> already used by another client. Skipped.\n", id, name );
            continue;
        }
        id_already.insert( id );

        // Add client information, only for non marked as deleted
        ClientData data;
        data.Clear();
        Str::Copy( data.ClientName, name );
        memcpy( data.ClientPassHash, pass_hash, PASS_HASH_SIZE );
        data.ClientId = id;
        ClientsData.push_back( data );

        // Process ids
        if( id > LastClientId )
            LastClientId = id;
    }

    // Clean up
    if( file_find )
        FileFindClose( file_find );
    SAFEDEL( cache_str );
    SAFEDELA( cache_buf );

    // Fail
    if( !success )
    {
        FileDelete( cache_fname ); // Next try will be without cache
        return false;
    }

    // Refresh last id
    last_id_file = FileOpen( last_id_fname, true );
    if( last_id_file )
    {
        char last_id_str[ 128 ];
        Str::Format( last_id_str, "%u", LastClientId );
        FileWrite( last_id_file, last_id_str, Str::Length( last_id_str ) );
        FileClose( last_id_file );
    }

    // Save cache
    if( file_find )
        file_cache_write.SaveOutBufToFile( cache_fname, -1 );

    // Reserve memory for future clients data
    if( ClientsData.size() > 10000 )
        ClientsData.reserve( ClientsData.size() * 2 );

    WriteLog( "Indexing complete, clients found<%u>.\n", ClientsData.size() );
    return true;
}

bool FOServer::SaveClient( Client* cl, bool deferred )
{
    if( Singleplayer )
        return true;

    if( Str::Compare( cl->Name, "err" ) || !cl->GetId() )
    {
        WriteLogF( _FUNC_, " - Trying save not valid client.\n" );
        return false;
    }

    CritDataExt* data_ext = cl->GetDataExt();
    if( !data_ext )
    {
        WriteLogF( _FUNC_, " - Can't get extended critter data.\n" );
        return false;
    }

    if( deferred && WorldSaveManager )
    {
        AddClientSaveData( cl );
    }
    else
    {
        char fname[ MAX_FOPATH ];
        FileManager::GetWritePath( cl->Name, PT_SERVER_CLIENTS, fname );
        Str::Append( fname, ".client" );
        void* f = FileOpen( fname, true );
        if( !f )
        {
            WriteLogF( _FUNC_, " - Unable to open client save file<%s>.\n", fname );
            return false;
        }

        FileWrite( f, ClientSaveSignature, sizeof( ClientSaveSignature ) );
        FileWrite( f, cl->PassHash, sizeof( cl->PassHash ) );
        FileWrite( f, &cl->Data, sizeof( cl->Data ) );
        FileWrite( f, data_ext, sizeof( CritDataExt ) );
        uint te_count = (uint) cl->CrTimeEvents.size();
        FileWrite( f, &te_count, sizeof( te_count ) );
        if( te_count )
            FileWrite( f, &cl->CrTimeEvents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) );
        FileClose( f );

        cl->Data.Temp = 0;
    }
    return true;
}

bool FOServer::LoadClient( Client* cl )
{
    if( Singleplayer )
        return true;

    CritDataExt* data_ext = cl->GetDataExt();
    if( !data_ext )
    {
        WriteLogF( _FUNC_, " - Can't get extended critter data.\n" );
        return false;
    }

    char fname[ MAX_FOPATH ];
    FileManager::GetWritePath( cl->Name, PT_SERVER_CLIENTS, fname );
    Str::Append( fname, ".client" );
    void* f = FileOpen( fname, false );
    if( !f )
    {
        WriteLogF( _FUNC_, " - Unable to open client save file<%s>.\n", fname );
        return false;
    }

    // Read data
    char signature[ 4 ];
    if( !FileRead( f, signature, sizeof( signature ) ) )
        goto label_FileTruncated;
    if( !FileRead( f, cl->PassHash, sizeof( cl->PassHash ) ) )
        goto label_FileTruncated;
    if( !FileRead( f, &cl->Data, sizeof( cl->Data ) ) )
        goto label_FileTruncated;
    if( !FileRead( f, data_ext, sizeof( CritDataExt ) ) )
        goto label_FileTruncated;
    uint te_count;
    if( !FileRead( f, &te_count, sizeof( te_count ) ) )
        goto label_FileTruncated;
    if( te_count )
    {
        cl->CrTimeEvents.resize( te_count );
        if( !FileRead( f, &cl->CrTimeEvents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) ) )
            goto label_FileTruncated;
    }
    FileClose( f );

    return true;

label_FileTruncated:
    WriteLogF( _FUNC_, " - Client save file<%s> truncated.\n", fname );
    FileClose( f );
    return false;
}

bool FOServer::NewWorld()
{
    UnloadWorld();
    InitGameTime();
    if( !GameOpt.GenerateWorldDisabled && !MapMngr.GenerateWorld( "GenerateWorld.cfg", PT_SERVER_MAPS ) )
        return false;
    return true;
}

void FOServer::SaveWorld( const char* fname )
{
    if( !GameOpt.GameServer )
        return;
    if( !fname && Singleplayer )
        return;                           // Disable autosaving in singleplayer mode

    double tick;
    if( WorldSaveManager )
    {
        // Be sure what Dump_Work thread in wait state
        DumpEndEvent.Wait();
        DumpEndEvent.Disallow();
        tick = Timer::AccurateTick();
        WorldSaveDataBufCount = 0;
        WorldSaveDataBufFreeSize = 0;
        ClientsSaveDataCount = 0;
    }
    else
    {
        // Save directly to file
        tick = Timer::AccurateTick();
        char auto_fname[ MAX_FOPATH ];
        Str::Format( auto_fname, "%sworld%04d.fo", FileManager::GetWritePath( "", PT_SERVER_SAVE ), SaveWorldIndex + 1 );
        DumpFile = FileOpen( fname ? fname : auto_fname, true );
        if( !DumpFile )
        {
            WriteLog( "Can't create dump file<%s>.\n", fname ? fname : auto_fname );
            return;
        }
    }

    // ServerFunctions.SaveWorld
    SaveWorldDeleteIndexes.clear();
    if( Script::PrepareContext( ServerFunctions.WorldSave, _FUNC_, "Game" ) )
    {
        ScriptArray* delete_indexes = Script::CreateArray( "uint[]" );
        Script::SetArgUInt( SaveWorldIndex + 1 );
        Script::SetArgObject( delete_indexes );
        if( Script::RunPrepared() )
            Script::AssignScriptArrayInVector( SaveWorldDeleteIndexes, delete_indexes );
        delete_indexes->Release();
    }

    // Version
    uint version = WORLD_SAVE_LAST;
    AddWorldSaveData( &version, sizeof( version ) );

    // SaveGameInfoFile
    SaveGameInfoFile();

    // SaveAllLocationsAndMapsFile
    MapMngr.SaveAllLocationsAndMapsFile( AddWorldSaveData );

    // SaveCrittersFile
    CrMngr.SaveCrittersFile( AddWorldSaveData );

    // SaveAllItemsFile
    ItemMngr.SaveAllItemsFile( AddWorldSaveData );

    // SaveVarsDataFile
    VarMngr.SaveVarsDataFile( AddWorldSaveData );

    // SaveHoloInfoFile
    SaveHoloInfoFile();

    // SaveAnyDataFile
    SaveAnyDataFile();

    // SaveTimeEventsFile
    SaveTimeEventsFile();

    // SaveScriptFunctionsFile
    SaveScriptFunctionsFile();

    // End signature
    AddWorldSaveData( &version, sizeof( version ) );

    // SaveClient
    ConnectedClientsLocker.Lock();
    for( auto it = ConnectedClients.begin(), end = ConnectedClients.end(); it != end; ++it )
    {
        Client* cl = *it;
        if( cl->GetId() )
            SaveClient( cl, true );
    }
    ConnectedClientsLocker.Unlock();

    SaveClientsLocker.Lock();
    for( auto it = SaveClients.begin(), end = SaveClients.end(); it != end; ++it )
    {
        Client* cl = *it;
        SaveClient( cl, true );
        cl->Release();
    }
    SaveClients.clear();
    SaveClientsLocker.Unlock();

    // Finish collect data
    if( WorldSaveManager )
    {
        // Awake Dump_Work
        DumpBeginEvent.Allow();
    }
    else
    {
        FileClose( DumpFile );
        DumpFile = NULL;
        SaveWorldIndex++;
        if( SaveWorldIndex >= WORLD_SAVE_MAX_INDEX )
            SaveWorldIndex = 0;
    }

    WriteLog( "World saved in %g ms.\n", Timer::AccurateTick() - tick );
}

bool FOServer::LoadWorld( const char* fname )
{
    UnloadWorld();

    void* f = NULL;
    if( !fname )
    {
        for( int i = WORLD_SAVE_MAX_INDEX; i >= 1; i-- )
        {
            char auto_fname[ MAX_FOPATH ];
            Str::Format( auto_fname, "%sworld%04d.fo", FileManager::GetWritePath( "", PT_SERVER_SAVE ), i );
            f = FileOpen( auto_fname, false );
            if( f )
            {
                WriteLog( "Begin load world from dump file<%s>.\n", auto_fname );
                SaveWorldIndex = i;
                break;
            }
        }

        if( !f )
        {
            WriteLog( "World dump file not found.\n" );
            SaveWorldIndex = 0;
            return true;
        }
    }
    else
    {
        f = FileOpen( fname, false );
        if( !f )
        {
            WriteLog( "World dump file<%s> not found.\n", fname );
            return false;
        }

        WriteLog( "Begin load world from dump file<%s>.\n", fname );
    }

    // File begin
    uint version = 0;
    FileRead( f, &version, sizeof( version ) );
    if( version != WORLD_SAVE_V1 && version != WORLD_SAVE_V2 && version != WORLD_SAVE_V3 && version != WORLD_SAVE_V4 &&
        version != WORLD_SAVE_V5 && version != WORLD_SAVE_V6 && version != WORLD_SAVE_V7 && version != WORLD_SAVE_V8 &&
        version != WORLD_SAVE_V9 && version != WORLD_SAVE_V10 && version != WORLD_SAVE_V11 && version != WORLD_SAVE_V12 &&
        version != WORLD_SAVE_V13 && version != WORLD_SAVE_V14 )
    {
        WriteLog( "Unknown version<%u> of world dump file.\n", version );
        FileClose( f );
        return false;
    }
    if( version < WORLD_SAVE_V13 )
    {
        WriteLog( "Version of save file is not supported.\n" );
        FileClose( f );
        return false;
    }

    // Main data
    if( !LoadGameInfoFile( f ) )
        return false;
    if( !MapMngr.LoadAllLocationsAndMapsFile( f ) )
        return false;
    if( !CrMngr.LoadCrittersFile( f, version ) )
        return false;
    if( !ItemMngr.LoadAllItemsFile( f, version ) )
        return false;
    if( !VarMngr.LoadVarsDataFile( f, version ) )
        return false;
    if( !LoadHoloInfoFile( f ) )
        return false;
    if( !LoadAnyDataFile( f ) )
        return false;
    if( !LoadTimeEventsFile( f ) )
        return false;
    if( !LoadScriptFunctionsFile( f ) )
        return false;

    // File end
    uint version_ = 0;
    if( !FileRead( f, &version_, sizeof( version_ ) ) || version != version_ )
    {
        WriteLog( "World dump file truncated.\n" );
        FileClose( f );
        return false;
    }
    FileClose( f );

    // Initialize data
    InitGameTime();
    if( !TransferAllNpc() )
        return false;                // Transfer critter copies to maps
    if( !TransferAllItems() )
        return false;                // Transfer items copies to critters and maps
    MapMngr.RunInitScriptMaps();     // Init scripts for maps
    CrMngr.RunInitScriptCritters();  // Init scripts for critters
    ItemMngr.RunInitScriptItems();   // Init scripts for maps
    return true;
}

void FOServer::UnloadWorld()
{
    // Delete critter and map jobs
    Job::Erase( JOB_CRITTER );
    Job::Erase( JOB_MAP );

    // Locations / maps
    MapMngr.Clear();

    // Critters
    CrMngr.Clear();

    // Items
    ItemMngr.Clear();

    // Vars
    VarMngr.Clear();

    // Holo info
    HolodiskInfo.clear();
    LastHoloId = USER_HOLO_START_NUM;

    // Any data
    AnyData.clear();

    // Time events
    for( auto it = TimeEvents.begin(), end = TimeEvents.end(); it != end; ++it )
        delete *it;
    TimeEvents.clear();
    TimeEventsLastNum = 0;

    // Singleplayer header
    SingleplayerSave.Valid = false;
}

void FOServer::AddWorldSaveData( void* data, size_t size )
{
    if( !WorldSaveManager )
    {
        FileWrite( DumpFile, data, (uint) size );
        return;
    }

    if( !WorldSaveDataBufFreeSize )
    {
        WorldSaveDataBufCount++;
        WorldSaveDataBufFreeSize = WORLD_SAVE_DATA_BUFFER_SIZE;

        if( WorldSaveDataBufCount >= WorldSaveData.size() )
        {
            MEMORY_PROCESS( MEMORY_SAVE_DATA, WORLD_SAVE_DATA_BUFFER_SIZE );
            WorldSaveData.push_back( new uchar[ WORLD_SAVE_DATA_BUFFER_SIZE ] );
        }
    }

    size_t flush = ( size <= WorldSaveDataBufFreeSize ? size : WorldSaveDataBufFreeSize );
    if( flush )
    {
        uchar* ptr = WorldSaveData[ WorldSaveDataBufCount - 1 ];
        memcpy( &ptr[ WORLD_SAVE_DATA_BUFFER_SIZE - WorldSaveDataBufFreeSize ], data, flush );
        WorldSaveDataBufFreeSize -= flush;
        if( !WorldSaveDataBufFreeSize )
            AddWorldSaveData( ( (uchar*) data ) + flush, size - flush );
    }
}

void FOServer::AddClientSaveData( Client* cl )
{
    if( ClientsSaveDataCount >= ClientsSaveData.size() )
    {
        ClientsSaveData.push_back( ClientSaveData() );
        MEMORY_PROCESS( MEMORY_SAVE_DATA, sizeof( ClientSaveData ) );
    }

    ClientSaveData& csd = ClientsSaveData[ ClientsSaveDataCount ];
    memcpy( csd.Name, cl->Name, sizeof( csd.Name ) );
    memcpy( csd.PasswordHash, cl->PassHash, sizeof( csd.PasswordHash ) );
    memcpy( &csd.Data, &cl->Data, sizeof( cl->Data ) );
    memcpy( &csd.DataExt, cl->GetDataExt(), sizeof( CritDataExt ) );
    csd.TimeEvents = cl->CrTimeEvents;

    ClientsSaveDataCount++;
}

void FOServer::Dump_Work( void* data )
{
    char fname[ MAX_FOPATH ];

    while( true )
    {
        // Locks
        DumpBeginEvent.Wait();
        DumpBeginEvent.Disallow();

        // Paths
        char save_path[ MAX_FOPATH ];
        FileManager::GetWritePath( "", PT_SERVER_SAVE, save_path );
        char clients_path[ MAX_FOPATH ];
        FileManager::GetWritePath( "", PT_SERVER_CLIENTS, clients_path );

        // Save world data
        void* fworld = FileOpen( Str::Format( fname, "%sworld%04d.fo", save_path, SaveWorldIndex + 1 ), true );
        if( fworld )
        {
            for( uint i = 0; i < WorldSaveDataBufCount; i++ )
            {
                uchar* ptr = WorldSaveData[ i ];
                size_t flush = WORLD_SAVE_DATA_BUFFER_SIZE;
                if( i == WorldSaveDataBufCount - 1 )
                    flush -= WorldSaveDataBufFreeSize;
                FileWrite( fworld, ptr, (uint) flush );
                Thread::Sleep( 1 );
            }
            FileClose( fworld );
            SaveWorldIndex++;
            if( SaveWorldIndex >= WORLD_SAVE_MAX_INDEX )
                SaveWorldIndex = 0;
        }
        else
        {
            WriteLogF( _FUNC_, " - Can't create world dump file<%s>.\n", fname );
        }

        // Save clients data
        for( uint i = 0; i < ClientsSaveDataCount; i++ )
        {
            ClientSaveData& csd = ClientsSaveData[ i ];

            char            fname[ MAX_FOPATH ];
            Str::Format( fname, "%s%s.client", clients_path, csd.Name );
            void*           fc = FileOpen( fname, true );
            if( !fc )
            {
                WriteLogF( _FUNC_, " - Unable to open client save file<%s>.\n", fname );
                continue;
            }

            FileWrite( fc, ClientSaveSignature, sizeof( ClientSaveSignature ) );
            FileWrite( fc, csd.PasswordHash, sizeof( csd.PasswordHash ) );
            FileWrite( fc, &csd.Data, sizeof( csd.Data ) );
            FileWrite( fc, &csd.DataExt, sizeof( csd.DataExt ) );
            uint te_count = (uint) csd.TimeEvents.size();
            FileWrite( fc, &te_count, sizeof( te_count ) );
            if( te_count )
                FileWrite( fc, &csd.TimeEvents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) );
            FileClose( fc );
            Thread::Sleep( 1 );
        }

        // Clear old dump files
        for( auto it = SaveWorldDeleteIndexes.begin(), end = SaveWorldDeleteIndexes.end(); it != end; ++it )
        {
            void* fold = FileOpen( Str::Format( fname, "%sworld%04d.fo", save_path, *it ), true );
            if( fold )
            {
                FileClose( fold );
                FileDelete( fname );
            }
        }

        // Notify about end of processing
        DumpEndEvent.Allow();
    }
}

/************************************************************************/
/* Scores                                                               */
/************************************************************************/

void FOServer::SetScore( int score, Critter* cr, int val )
{
    SCOPE_LOCK( BestScoresLocker );

    cr->Data.Scores[ score ] += val;
    if( BestScores[ score ].ClientId == cr->GetId() )
        return;
    if( BestScores[ score ].Value >= cr->Data.Scores[ score ] )
        return;                                                     // TODO: less/greater
    BestScores[ score ].Value = cr->Data.Scores[ score ];
    BestScores[ score ].ClientId = cr->GetId();
    Str::Copy( BestScores[ score ].ClientName, cr->GetName() );
}

void FOServer::SetScore( int score, const char* name )
{
    SCOPE_LOCK( BestScoresLocker );

    BestScores[ score ].ClientId = 0;
    BestScores[ score ].Value = 0;
    Str::Copy( BestScores[ score ].ClientName, name );
}

const char* FOServer::GetScores()
{
    SCOPE_LOCK( BestScoresLocker );

    static THREAD char scores[ SCORE_NAME_LEN * SCORES_MAX ]; // Only names
    for( int i = 0; i < SCORES_MAX; i++ )
    {
        char* offs = &scores[ i * SCORE_NAME_LEN ];           // Name
        memcpy( offs, BestScores[ i ].ClientName, SCORE_NAME_LEN );
    }

    return scores;
}

void FOServer::ClearScore( int score )
{
    SCOPE_LOCK( BestScoresLocker );

    BestScores[ score ].ClientId = 0;
    BestScores[ score ].ClientName[ 0 ] = '\0';
    BestScores[ score ].Value = 0;
}

void FOServer::Process_GetScores( Client* cl )
{
    uint tick = Timer::FastTick();
    if( tick < cl->LastSendScoresTick + SCORES_SEND_TIME )
    {
        WriteLogF( _FUNC_, " - Client<%s> ignore send scores timeout.\n", cl->GetInfo() );
        return;
    }
    cl->LastSendScoresTick = tick;

    uint        msg = NETMSG_SCORES;
    const char* scores = GetScores();

    BOUT_BEGIN( cl );
    cl->Bout << msg;
    cl->Bout.Push( scores, SCORE_NAME_LEN * SCORES_MAX );
    BOUT_END( cl );
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

void FOServer::VarsGarbarger( bool force )
{
    if( !force )
    {
        uint tick = Timer::FastTick();
        if( tick - VarsGarbageLastTick < VarsGarbageTime )
            return;
    }

    // Get client and npc ids
    UIntSet ids_clients;
    if( !Singleplayer )
    {
        for( auto it = ClientsData.begin(), end = ClientsData.end(); it != end; ++it )
            ids_clients.insert( ( *it ).ClientId );
    }
    else
    {
        ids_clients.insert( 1 );
    }

    // Get npc ids
    UIntSet ids_npcs;
    CrMngr.GetNpcIds( ids_npcs );

    // Get location and map ids
    UIntSet ids_locs, ids_maps;
    MapMngr.GetLocationAndMapIds( ids_locs, ids_maps );

    // Get item ids
    UIntSet ids_items;
    ItemMngr.GetItemIds( ids_items );

    VarMngr.ClearUnusedVars( ids_npcs, ids_clients, ids_locs, ids_maps, ids_items );
    VarsGarbageLastTick = Timer::FastTick();
}

/************************************************************************/
/* Time events                                                          */
/************************************************************************/

void FOServer::SaveTimeEventsFile()
{
    uint count = 0;
    for( auto it = TimeEvents.begin(), end = TimeEvents.end(); it != end; ++it )
        if( ( *it )->IsSaved )
            count++;
    AddWorldSaveData( &count, sizeof( count ) );
    for( auto it = TimeEvents.begin(), end = TimeEvents.end(); it != end; ++it )
    {
        TimeEvent* te = *it;
        if( !te->IsSaved )
            continue;
        AddWorldSaveData( &te->Num, sizeof( te->Num ) );
        ushort script_name_len = (ushort) te->FuncName.length();
        AddWorldSaveData( &script_name_len, sizeof( script_name_len ) );
        AddWorldSaveData( (void*) te->FuncName.c_str(), script_name_len );
        AddWorldSaveData( &te->FullSecond, sizeof( te->FullSecond ) );
        AddWorldSaveData( &te->Rate, sizeof( te->Rate ) );
        uint values_size = (uint) te->Values.size();
        AddWorldSaveData( &values_size, sizeof( values_size ) );
        if( values_size )
            AddWorldSaveData( &te->Values[ 0 ], values_size * sizeof( uint ) );
    }
}

bool FOServer::LoadTimeEventsFile( void* f )
{
    WriteLog( "Load time events..." );

    TimeEventsLastNum = 0;

    uint count = 0;
    if( !FileRead( f, &count, sizeof( count ) ) )
        return false;
    for( uint i = 0; i < count; i++ )
    {
        uint num;
        if( !FileRead( f, &num, sizeof( num ) ) )
            return false;

        char   script_name[ MAX_SCRIPT_NAME * 2 + 2 ];
        ushort script_name_len;
        if( !FileRead( f, &script_name_len, sizeof( script_name_len ) ) )
            return false;
        if( !FileRead( f, script_name, script_name_len ) )
            return false;
        script_name[ script_name_len ] = 0;

        uint begin_second;
        if( !FileRead( f, &begin_second, sizeof( begin_second ) ) )
            return false;
        uint rate;
        if( !FileRead( f, &rate, sizeof( rate ) ) )
            return false;

        UIntVec values;
        uint    values_size;
        if( !FileRead( f, &values_size, sizeof( values_size ) ) )
            return false;
        if( values_size )
        {
            values.resize( values_size );
            if( !FileRead( f, &values[ 0 ], values_size * sizeof( uint ) ) )
                return false;
        }

        bool singed = false;
        int  bind_id = Script::Bind( script_name, "uint %s(int[]@)", false, true );
        if( bind_id <= 0 )
            bind_id = Script::Bind( script_name, "uint %s(uint[]@)", false );
        else
            singed = true;
        if( bind_id <= 0 )
        {
            WriteLog( "Unable to bind script function, event num<%u>, name<%s>.\n", num, script_name );
            continue;
        }

        TimeEvent* te = new TimeEvent();
        if( !te )
            return false;
        te->FullSecond = begin_second;
        te->Num = num;
        te->FuncName = script_name;
        te->BindId = bind_id;
        te->Rate = rate;
        te->SignedValues = singed;
        te->IsSaved = true;
        te->InProcess = 0;
        te->EraseMe = false;
        if( values_size )
            te->Values = values;

        TimeEvents.push_back( te );
        if( num > TimeEventsLastNum )
            TimeEventsLastNum = num;
    }

    WriteLog( "complete, count<%u>.\n", count );
    return true;
}

void FOServer::AddTimeEvent( TimeEvent* te )
{
    // Invoked in locked scope

    for( auto it = TimeEvents.begin(), end = TimeEvents.end(); it != end; ++it )
    {
        TimeEvent* te_ = *it;
        if( te->FullSecond < te_->FullSecond )
        {
            TimeEvents.insert( it, te );
            return;
        }
    }
    TimeEvents.push_back( te );
}

uint FOServer::CreateTimeEvent( uint begin_second, const char* script_name, int values, uint val1, ScriptArray* val2, bool save )
{
    char module_name[ MAX_FOTEXT ];
    char func_name[ MAX_FOTEXT ];
    if( !Script::ReparseScriptName( script_name, module_name, func_name ) )
        return 0;

    bool singed = false;
    int  bind_id = Script::Bind( module_name, func_name, "uint %s(int[]@)", false, true );
    if( bind_id <= 0 )
        bind_id = Script::Bind( module_name, func_name, "uint %s(uint[]@)", false );
    else
        singed = true;
    if( bind_id <= 0 )
        return 0;

    TimeEvent* te = new TimeEvent();
    if( !te )
        return 0;

    SCOPE_LOCK( TimeEventsLocker );

    te->FullSecond = begin_second;
    te->Num = TimeEventsLastNum + 1;
    te->FuncName = Str::FormatBuf( "%s@%s", module_name, func_name );
    te->BindId = bind_id;
    te->SignedValues = singed;
    te->IsSaved = save;
    te->InProcess = 0;
    te->EraseMe = false;
    te->Rate = 0;

    if( values == 1 )
    {
        // Single value
        te->Values.push_back( val1 );
    }
    else if( values == 2 )
    {
        // Array values
        uint count = val2->GetSize();
        if( count )
        {
            te->Values.resize( count );
            memcpy( &te->Values[ 0 ], val2->At( 0 ), count * sizeof( uint ) );
        }
    }

    AddTimeEvent( te );
    TimeEventsLastNum++;
    return TimeEventsLastNum;
}

void FOServer::TimeEventEndScriptCallback()
{
    SCOPE_LOCK( TimeEventsLocker );

    size_t tid = Thread::GetCurrentId();
    for( auto it = TimeEvents.begin(); it != TimeEvents.end();)
    {
        TimeEvent* te = *it;
        if( te->InProcess == tid )
        {
            te->InProcess = 0;

            if( te->EraseMe )
            {
                it = TimeEvents.erase( it );
                delete te;
            }
        }
        else
            ++it;
    }
}

bool FOServer::GetTimeEvent( uint num, uint& duration, ScriptArray* values )
{
    TimeEventsLocker.Lock();

    TimeEvent* te = NULL;
    size_t     tid = Thread::GetCurrentId();

    // Find event
    while( true )
    {
        for( auto it = TimeEvents.begin(), end = TimeEvents.end(); it != end; ++it )
        {
            TimeEvent* te_ = *it;
            if( te_->Num == num )
            {
                te = te_;
                break;
            }
        }

        // Event not found or erased
        if( !te || te->EraseMe )
        {
            TimeEventsLocker.Unlock();
            return false;
        }

        // Wait of other threads end work with event
        if( te->InProcess && te->InProcess != tid )
        {
            TimeEventsLocker.Unlock();
            Thread::Sleep( 0 );
            TimeEventsLocker.Lock();
        }
        else
        {
            // Begin work with event
            break;
        }
    }

    // Fill data
    if( values )
        Script::AppendVectorToArray( te->Values, values );
    duration = ( te->FullSecond > GameOpt.FullSecond ? te->FullSecond - GameOpt.FullSecond : 0 );

    // Lock for current thread
    if( LogicMT )
        te->InProcess = tid;

    // Add end of script execution callback to unlock event if SetTimeEvent not be called
    if( LogicMT )
        Script::AddEndExecutionCallback( TimeEventEndScriptCallback );

    TimeEventsLocker.Unlock();
    return true;
}

bool FOServer::SetTimeEvent( uint num, uint duration, ScriptArray* values )
{
    TimeEventsLocker.Lock();

    TimeEvent* te = NULL;
    size_t     tid = Thread::GetCurrentId();

    // Find event
    while( true )
    {
        for( auto it = TimeEvents.begin(), end = TimeEvents.end(); it != end; ++it )
        {
            TimeEvent* te_ = *it;
            if( te_->Num == num )
            {
                te = te_;
                break;
            }
        }

        // Event not found or erased
        if( !te || te->EraseMe )
        {
            TimeEventsLocker.Unlock();
            return false;
        }

        // Wait of other threads end work with event
        if( te->InProcess && te->InProcess != tid )
        {
            TimeEventsLocker.Unlock();
            Thread::Sleep( 0 );
            TimeEventsLocker.Lock();
        }
        else
        {
            // Begin work with event
            break;
        }
    }

    // Fill data, lock event to current thread
    if( values )
        Script::AssignScriptArrayInVector( te->Values, values );
    te->FullSecond = GameOpt.FullSecond + duration;

    // Unlock from current thread
    te->InProcess = 0;

    TimeEventsLocker.Unlock();
    return true;
}

bool FOServer::EraseTimeEvent( uint num )
{
    SCOPE_LOCK( TimeEventsLocker );

    for( auto it = TimeEvents.begin(), end = TimeEvents.end(); it != end; ++it )
    {
        TimeEvent* te = *it;
        if( te->Num == num )
        {
            if( te->InProcess )
            {
                te->EraseMe = true;
            }
            else
            {
                delete te;
                TimeEvents.erase( it );
            }
            return true;
        }
    }

    return false;
}

void FOServer::ProcessTimeEvents()
{
    TimeEventsLocker.Lock();

    TimeEvent* cur_event = NULL;
    for( auto it = TimeEvents.begin(), end = TimeEvents.end(); it != end; ++it )
    {
        TimeEvent* te = *it;
        if( !te->InProcess && te->FullSecond <= GameOpt.FullSecond )
        {
            te->InProcess = Thread::GetCurrentId();
            cur_event = te;
            break;
        }
    }

    TimeEventsLocker.Unlock();

    if( !cur_event )
        return;

    uint wait_time = 0;
    if( Script::PrepareContext( cur_event->BindId, _FUNC_, Str::FormatBuf( "Time event<%u>, name<%s>", cur_event->Num, cur_event->FuncName.c_str() ) ) )
    {
        ScriptArray* values = NULL;
        uint         size = (uint) cur_event->Values.size();

        if( size > 0 )
        {
            if( cur_event->SignedValues )
                values = Script::CreateArray( "int[]" );
            else
                values = Script::CreateArray( "uint[]" );

            if( !values )
            {
                WriteLogF( _FUNC_, " - Create values array fail. Wait 10 real minutes.\n" );
                wait_time = GameOpt.TimeMultiplier * 600;
            }
            else
            {
                values->Resize( size );
                memcpy( values->At( 0 ), (void*) &cur_event->Values[ 0 ], size * sizeof( uint ) );
            }
        }

        if( !size || values )
        {
            Script::SetArgObject( values );
            if( !Script::RunPrepared() )
            {
                WriteLogF( _FUNC_, " - RunPrepared fail. Wait 10 real minutes.\n" );
                wait_time = GameOpt.TimeMultiplier * 600;
            }
            else
            {
                wait_time = Script::GetReturnedUInt();
                if( wait_time && values )               // Refresh array
                {
                    uint arr_size = values->GetSize();
                    cur_event->Values.resize( arr_size );
                    if( arr_size )
                        memcpy( &cur_event->Values[ 0 ], values->At( 0 ), arr_size * sizeof( cur_event->Values[ 0 ] ) );
                }
            }

            if( values )
                values->Release();
        }
    }
    else
    {
        WriteLogF( _FUNC_, " - Game contexts prepare fail. Wait 10 real minutes.\n" );
        wait_time = GameOpt.TimeMultiplier * 600;
    }

    TimeEventsLocker.Lock();

    auto it = std::find( TimeEvents.begin(), TimeEvents.end(), cur_event );
    TimeEvents.erase( it );

    if( wait_time && !cur_event->EraseMe )
    {
        cur_event->FullSecond = GameOpt.FullSecond + wait_time;
        cur_event->Rate++;
        cur_event->InProcess = 0;
        AddTimeEvent( cur_event );
    }
    else
    {
        delete cur_event;
    }

    TimeEventsLocker.Unlock();
}

uint FOServer::GetTimeEventsCount()
{
    SCOPE_LOCK( TimeEventsLocker );

    uint count = (uint) TimeEvents.size();
    return count;
}

string FOServer::GetTimeEventsStatistics()
{
    SCOPE_LOCK( TimeEventsLocker );

    static string result;
    char          str[ 1024 ];
    Str::Format( str, "Time events: %u\n", TimeEvents.size() );
    result = str;
    DateTime st = Timer::GetGameTime( GameOpt.FullSecond );
    Str::Format( str, "Game time: %02u.%02u.%04u %02u:%02u:%02u\n", st.Day, st.Month, st.Year, st.Hour, st.Minute, st.Second );
    result += str;
    result += "Number    Date       Time     Rate Saved Function                            Values\n";
    for( uint i = 0, j = (uint) TimeEvents.size(); i < j; i++ )
    {
        TimeEvent* te = TimeEvents[ i ];
        st = Timer::GetGameTime( te->FullSecond );
        Str::Format( str, "%09u %02u.%02u.%04u %02u:%02u:%02u %04u %-5s %-35s", te->Num, st.Day, st.Month, st.Year, st.Hour, st.Minute, st.Second, te->Rate, te->IsSaved ? "true" : "false", te->FuncName.c_str() );
        result += str;
        for( uint k = 0, l = (uint) te->Values.size(); k < l; k++ )
        {
            Str::Format( str, " %-10u", te->Values[ k ] );
            result += str;
        }
        result += "\n";
    }
    return result;
}

void FOServer::SaveScriptFunctionsFile()
{
    StrVec func_names;
    Script::GetIndexedScriptFunc( func_names );
    uint   count = (uint) func_names.size();
    AddWorldSaveData( &count, sizeof( count ) );
    for( uint i = 0; i < count; i++ )
    {
        const string& func_name = func_names[ i ];
        uint          len = (uint) func_name.length();
        AddWorldSaveData( &len, sizeof( len ) );
        AddWorldSaveData( (void*) func_name.c_str(), len );
    }
}

bool FOServer::LoadScriptFunctionsFile( void* f )
{
    uint count = 0;
    if( !FileRead( f, &count, sizeof( count ) ) )
        return false;
    Script::AddIndexedScriptFunc( NULL, NULL );
    for( uint i = 0; i < count; i++ )
    {
        char script[ 1024 ];
        uint len = 0;
        if( !FileRead( f, &len, sizeof( len ) ) )
            return false;
        if( len && !FileRead( f, script, len ) )
            return false;
        script[ len ] = 0;

        // Cut off decl
        char* decl = strstr( script, "|" );
        if( !decl )
        {
            WriteLogF( _FUNC_, " - Function declaration not found, script<%s>.\n", script );
            continue;
        }
        *decl = 0;
        decl++;

        // Parse
        if( !Script::AddIndexedScriptFunc( script, decl ) )
        {
            WriteLogF( _FUNC_, " - Function<%s,%s> not found.\n", script, decl );
            continue;
        }
    }
    return true;
}

/************************************************************************/
/* Any data                                                             */
/************************************************************************/
void FOServer::SaveAnyDataFile()
{
    uint count = (uint) AnyData.size();
    AddWorldSaveData( &count, sizeof( count ) );
    for( auto it = AnyData.begin(), end = AnyData.end(); it != end; ++it )
    {
        const string& name = ( *it ).first;
        UCharVec&     data = ( *it ).second;
        uint          name_len = (uint) name.length();
        AddWorldSaveData( &name_len, sizeof( name_len ) );
        AddWorldSaveData( (void*) name.c_str(), name_len );
        uint data_len = (uint) data.size();
        AddWorldSaveData( &data_len, sizeof( data_len ) );
        if( data_len )
            AddWorldSaveData( &data[ 0 ], data_len );
    }
}

bool FOServer::LoadAnyDataFile( void* f )
{
    UCharVec data;
    uint     count = 0;
    if( !FileRead( f, &count, sizeof( count ) ) )
        return false;
    for( uint i = 0; i < count; i++ )
    {
        char name[ MAX_FOTEXT ];
        uint name_len;
        if( !FileRead( f, &name_len, sizeof( name_len ) ) )
            return false;
        if( !FileRead( f, name, name_len ) )
            return false;
        name[ name_len ] = 0;

        uint data_len;
        if( !FileRead( f, &data_len, sizeof( data_len ) ) )
            return false;
        data.resize( data_len );
        if( data_len && !FileRead( f, &data[ 0 ], data_len ) )
            return false;

        auto result = AnyData.insert( PAIR( string( name ), data ) );
        MEMORY_PROCESS( MEMORY_ANY_DATA, (int) ( *result.first ).second.capacity() );
    }
    return true;
}

bool FOServer::SetAnyData( const string& name, const uchar* data, uint data_size )
{
    SCOPE_LOCK( AnyDataLocker );

    auto      result = AnyData.insert( PAIR( name, UCharVec() ) );
    UCharVec& data_ = ( *result.first ).second;

    MEMORY_PROCESS( MEMORY_ANY_DATA, -(int) data_.capacity() );
    data_.resize( data_size );
    if( data_size )
        memcpy( &data_[ 0 ], data, data_size );
    MEMORY_PROCESS( MEMORY_ANY_DATA, (int) data_.capacity() );
    return true;
}

bool FOServer::GetAnyData( const string& name, ScriptArray& script_array )
{
    SCOPE_LOCK( AnyDataLocker );

    auto it = AnyData.find( name );
    if( it == AnyData.end() )
        return false;

    UCharVec& data = ( *it ).second;
    uint      length = (uint) data.size();

    if( !length )
    {
        script_array.Resize( 0 );
        return true;
    }

    uint element_size = script_array.GetElementSize();
    script_array.Resize( length / element_size + ( ( length % element_size ) ? 1 : 0 ) );
    memcpy( script_array.At( 0 ), &data[ 0 ], length );
    return true;
}

bool FOServer::IsAnyData( const string& name )
{
    SCOPE_LOCK( AnyDataLocker );

    auto it = AnyData.find( name );
    bool present = ( it != AnyData.end() );
    return present;
}

void FOServer::EraseAnyData( const string& name )
{
    SCOPE_LOCK( AnyDataLocker );

    AnyData.erase( name );
}

string FOServer::GetAnyDataStatistics()
{
    SCOPE_LOCK( AnyDataLocker );

    static string result;
    char          str[ MAX_FOTEXT ];
    result = "Any data count: ";
    result += Str::ItoA( (int) AnyData.size() );
    result += "\nName                          Length    Data\n";
    for( auto it = AnyData.begin(), end = AnyData.end(); it != end; ++it )
    {
        const string& name = ( *it ).first;
        UCharVec&     data = ( *it ).second;
        Str::Format( str, "%-30s", name.c_str() );
        result += str;
        Str::Format( str, "%-10u", data.size() );
        result += str;
        for( uint i = 0, j = (uint) data.size(); i < j; i++ )
        {
            Str::Format( str, "%02X", data[ i ] );
            result += str;
        }
        result += "\n";
    }
    return result;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

template< class T >
void ModifyVec( UCharVec& vec, T data )
{
    size_t cur = vec.size();
    vec.resize( cur + sizeof( data ) );
    memcpy( &vec[ cur ], &data, sizeof( data ) );
}

template< class T >
void ModifyVecArr( UCharVec& vec, T* data, size_t size )
{
    size_t cur = vec.size();
    vec.resize( cur + size );
    memcpy( &vec[ cur ], data, size );
}

void FOServer::GenerateUpdateFiles( bool first_generation /* = false */ )
{
    if( !GameOpt.UpdateServer )
        return;
    if( !first_generation && UpdateFiles.empty() )
        return;

    WriteLog( "Generate update files...\n" );

    // Clear collections
    for( auto it = UpdateFiles.begin(), end = UpdateFiles.end(); it != end; ++it )
        SAFEDELA( ( *it ).Data );
    UpdateFiles.clear();
    UpdateFilesList.clear();

    // Fill MSG
    UpdateFile update_file;
    for( auto it = LangPacks.begin(), end = LangPacks.end(); it != end; ++it )
    {
        LanguagePack& lang_pack = *it;
        for( int i = 0; i < TEXTMSG_COUNT; i++ )
        {
            UCharVec msg_data;
            lang_pack.Msg[ i ].GetBinaryData( msg_data );

            memzero( &update_file, sizeof( update_file ) );
            update_file.Size = (uint) msg_data.size();
            update_file.Data = new uchar[ update_file.Size ];
            memcpy( update_file.Data, &msg_data[ 0 ], update_file.Size );
            UpdateFiles.push_back( update_file );

            char msg_cache_name[ MAX_FOPATH ];
            lang_pack.GetMsgCacheName( i, msg_cache_name );

            ModifyVec( UpdateFilesList, (short) Str::Length( msg_cache_name ) );
            ModifyVecArr( UpdateFilesList, msg_cache_name, Str::Length( msg_cache_name ) );
            ModifyVec( UpdateFilesList, update_file.Size );
            ModifyVec( UpdateFilesList, Crypt.Crc32( update_file.Data, update_file.Size ) );
        }
    }

    // Fill prototypes
    UCharVec proto_items_data;
    ItemMngr.GetBinaryData( proto_items_data );

    memzero( &update_file, sizeof( update_file ) );
    update_file.Size = (uint) proto_items_data.size();
    update_file.Data = new uchar[ update_file.Size ];
    memcpy( update_file.Data, &proto_items_data[ 0 ], update_file.Size );
    UpdateFiles.push_back( update_file );

    char protos_cache_name[ MAX_FOPATH ];
    Str::Format( protos_cache_name, CACHE_ITEM_PROTOS );

    ModifyVec( UpdateFilesList, (short) Str::Length( protos_cache_name ) );
    ModifyVecArr( UpdateFilesList, protos_cache_name, Str::Length( protos_cache_name ) );
    ModifyVec( UpdateFilesList, update_file.Size );
    ModifyVec( UpdateFilesList, Crypt.Crc32( update_file.Data, update_file.Size ) );

    // Fill files
    StrVec file_names;
    FileManager::GetFolderFileNames( FileManager::GetDataPath( "", PT_SERVER_UPDATE ), true, NULL, file_names );
    for( size_t i = 0, j = file_names.size(); i < j; i++ )
    {
        string      file_name = file_names[ i ];
        string      file_name_target = file_name;

        const char* ext = FileManager::GetExtension( file_name.c_str() );
        if( ext && Str::CompareCase( ext, "link" ) )
        {
            FileManager link;
            if( !link.LoadFile( file_name.c_str(), PT_SERVER_UPDATE ) )
            {
                WriteLogF( _FUNC_, " - Can't load link file<%s>.\n", file_name.c_str() );
                continue;
            }

            file_name = file_name.substr( 0, file_name.length() - 5 );
            file_name.insert( 0, (char*) link.GetBuf() );
            file_name_target = file_name_target.substr( 0, file_name_target.length() - 5 );
        }

        FileManager file;
        if( !file.LoadFile( file_name.c_str(), PT_SERVER_UPDATE ) )
        {
            WriteLogF( _FUNC_, " - Can't load file<%s>.\n", file_name.c_str() );
            continue;
        }

        memzero( &update_file, sizeof( update_file ) );
        update_file.Size = file.GetFsize();
        update_file.Data = file.ReleaseBuffer();
        UpdateFiles.push_back( update_file );

        ModifyVec( UpdateFilesList, (short) file_name_target.length() );
        ModifyVecArr( UpdateFilesList, file_name_target.c_str(), (uint) file_name_target.length() );
        ModifyVec( UpdateFilesList, update_file.Size );
    }

    // Complete files list
    ModifyVec( UpdateFilesList, (short) -1 );

    // Disconnect all connected clients to force data updating
    if( !first_generation )
    {
        ConnectedClientsLocker.Lock();
        for( auto it = ConnectedClients.begin(), end = ConnectedClients.end(); it != end; ++it )
            ( *it )->Disconnect();
        ConnectedClientsLocker.Unlock();
    }

    WriteLog( "Generate update files complete.\n" );
}

/************************************************************************/
/* Brute force                                                          */
/************************************************************************/

bool FOServer::CheckBruteForceIp( uint ip )
{
    // Skip checking
    if( !GameOpt.BruteForceTick || Singleplayer )
        return false;

    // Get entire for this ip
    uint tick = Timer::FastTick();
    auto it = BruteForceIps.insert( PAIR( ip, PAIR( tick, 0 ) ) );

    // Newly inserted
    if( it.second )
        return false;

    // Blocked
    if( it.first->second.first > tick )
        return true;

    // Check timeout
    bool result = ( tick - it.first->second.first ) < GameOpt.BruteForceTick;
    it.first->second.first = tick;

    // After 10 fails block access for this ip on 10 minutes
    if( result && ++it.first->second.second >= 10 )
    {
        it.first->second.first = tick + 10 * 60 * 1000;
        it.first->second.second = 0;
        return true;
    }

    return result;
}

bool FOServer::CheckBruteForceName( const char* name )
{
    // Skip checking
    if( !GameOpt.BruteForceTick || Singleplayer )
        return false;

    // Get entire for this name
    uint tick = Timer::FastTick();
    auto it = BruteForceNames.insert( PAIR( string( name ), tick ) );

    // Newly inserted
    if( it.second )
        return false;

    // Check timeout
    bool result = ( tick - it.first->second ) < GameOpt.BruteForceTick;
    it.first->second = tick;
    return result;
}

void FOServer::ClearBruteForceEntire( uint ip, const char* name )
{
    BruteForceIps.erase( ip );
    BruteForceNames.erase( string( name ) );
}
