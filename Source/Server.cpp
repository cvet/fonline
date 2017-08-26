#include "Common.h"
#include "Server.h"
#include "AngelScript/sdk/add_on/scripthelper/scripthelper.h"
#include "minizip/zip.h"
#include "ResourceConverter.h"
#include "FileSystem.h"

#define MAX_CLIENTS_IN_GAME    ( 3000 )

int                         FOServer::UpdateIndex = -1;
int                         FOServer::UpdateLastIndex = -1;
uint                        FOServer::UpdateLastTick = 0;
ClVec                       FOServer::LogClients;
NetServerBase*              FOServer::TcpServer;
NetServerBase*              FOServer::WebSocketsServer;
ClVec                       FOServer::ConnectedClients;
Mutex                       FOServer::ConnectedClientsLocker;
FOServer::Statistics_       FOServer::Statistics;
uint                        FOServer::SaveWorldIndex = 0;
uint                        FOServer::SaveWorldTime = 0;
uint                        FOServer::SaveWorldNextTick = 0;
UIntVec                     FOServer::SaveWorldDeleteIndexes;
FOServer::EntityDumpVec     FOServer::DumpedEntities;
Thread                      FOServer::DumpThread;
bool                        FOServer::RequestReloadClientScripts = false;
LangPackVec                 FOServer::LangPacks;
Pragmas                     FOServer::ServerPropertyPragmas;
FOServer::TextListenVec     FOServer::TextListeners;
Mutex                       FOServer::TextListenersLocker;
bool                        FOServer::Active = false;
bool                        FOServer::ActiveInProcess = false;
bool                        FOServer::ActiveOnce = false;
ClVec                       FOServer::SaveClients;
Mutex                       FOServer::SaveClientsLocker;
UIntMap                     FOServer::RegIp;
Mutex                       FOServer::RegIpLocker;
FOServer::ClientBannedVec   FOServer::Banned;
Mutex                       FOServer::BannedLocker;
FOServer::ClientDataMap     FOServer::ClientsData;
Mutex                       FOServer::ClientsDataLocker;
FOServer::SingleplayerSave_ FOServer::SingleplayerSave;
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
    SingleplayerSave.Valid = false;
    RequestReloadClientScripts = false;
    MEMORY_PROCESS( MEMORY_STATIC, sizeof( FOServer ) );
}

FOServer::~FOServer()
{
    //
}

void FOServer::Finish()
{
    Active = false;
    ActiveInProcess = true;

    // World dumper
    DumpThread.Wait();
    SaveWorldIndex = 0;
    SaveWorldTime = 0;
    SaveWorldNextTick = 0;

    // Logging clients
    LogToFunc( FOServer::LogToClients, false );
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

    // Shutdown servers
    SAFEDEL( TcpServer );
    SAFEDEL( WebSocketsServer );

    // Managers
    EntityMngr.ClearEntities();
    DlgMngr.Finish();
    FinishScriptSystem();
    LangPacks.clear();
    FileManager::ClearDataFiles();

    // Statistics
    WriteLog( "Server stopped.\n" );
    WriteLog( "Statistics:\n" );
    WriteLog( "Traffic:\n" );
    WriteLog( "Bytes Send: {}\n", Statistics.BytesSend );
    WriteLog( "Bytes Recv: {}\n", Statistics.BytesRecv );
    WriteLog( "Cycles count: {}\n", Statistics.LoopCycles );
    WriteLog( "Approx cycle period: {}\n", Statistics.LoopTime / ( Statistics.LoopCycles ? Statistics.LoopCycles : 1 ) );
    WriteLog( "Min cycle period: {}\n", Statistics.LoopMin );
    WriteLog( "Max cycle period: {}\n", Statistics.LoopMax );
    WriteLog( "Count of lags (>100ms): {}\n", Statistics.LagsCount );

    ActiveInProcess = false;
}

string FOServer::GetIngamePlayersStatistics()
{
    const char* cond_states_str[] = { "None", "Life", "Knockout", "Dead" };

    ConnectedClientsLocker.Lock();
    uint conn_count = (uint) ConnectedClients.size();
    ConnectedClientsLocker.Unlock();

    ClVec players;
    CrMngr.GetClients( players );

    string result = _str( "Players in game: {}\nConnections: {}\n", players.size(), conn_count );
    result += "Name                 Id         Ip              Online  Cond     X     Y     Location and map\n";
    for( Client* cl : players )
    {
        Map*      map = MapMngr.GetMap( cl->GetMapId() );
        Location* loc = ( map ? map->GetLocation() : nullptr );

        string    str_loc = _str( "{} ({}) {} ({})",
                                  map ? loc->GetName() : "", map ? loc->GetId() : 0, map ? map->GetName() : "", map ? map->GetId() : 0 );
        result += _str( "{:<20} {:<10} {:<15} {:<7} {:<8} {:<5} {:<5} {}\n",
                        cl->Name.c_str(), cl->GetId(), cl->GetIpStr(), cl->IsOffline() ? "No" : "Yes", cond_states_str[ cl->GetCond() ],
                        map ? cl->GetHexX() : cl->GetWorldX(), map ? cl->GetHexY() : cl->GetWorldY(), map ? str_loc : "Global map" );
    }
    return result;
}

// Accesses
void FOServer::GetAccesses( StrVec& client, StrVec& tester, StrVec& moder, StrVec& admin, StrVec& admin_names )
{
    client = _str( MainConfig->GetStr( "", "Access_client" ) ).split( ' ' );
    tester = _str( MainConfig->GetStr( "", "Access_tester" ) ).split( ' ' );
    moder = _str( MainConfig->GetStr( "", "Access_moder" ) ).split( ' ' );
    admin = _str( MainConfig->GetStr( "", "Access_admin" ) ).split( ' ' );
    admin_names = _str( MainConfig->GetStr( "", "AccessNames_admin" ) ).split( ' ' );
}

void FOServer::DisconnectClient( Client* cl )
{
    // Manage saves, exit from game
    uint    id = cl->GetId();
    Client* cl_ = ( id ? CrMngr.GetPlayer( id ) : nullptr );
    if( cl_ && cl_ == cl )
    {
        EraseSaveClient( cl->GetId() );
        AddSaveClient( cl );

        SETFLAG( cl->Flags, FCRIT_DISCONNECT );
        if( cl->GetMapId() )
        {
            cl->SendA_Action( ACTION_DISCONNECT, 0, nullptr );
        }
        else
        {
            for( auto it = cl->GlobalMapGroup->begin(), end = cl->GlobalMapGroup->end(); it != end; ++it )
            {
                Critter* cr = *it;
                if( cr != cl )
                    cr->Send_CustomCommand( cl, OTHER_FLAGS, cl->Flags );
            }
        }
    }
}

void FOServer::RemoveClient( Client* cl )
{
    uint    id = cl->GetId();
    Client* cl_ = ( id ? CrMngr.GetPlayer( id ) : nullptr );
    if( cl_ && cl_ == cl )
    {
        Script::RaiseInternalEvent( ServerFunctions.PlayerLogout, cl );
        if( cl->GetClientToDelete() )
            Script::RaiseInternalEvent( ServerFunctions.CritterFinish, cl );

        MapMngr.EraseCrFromMap( cl, cl->GetMap() );

        // Deferred saving
        EraseSaveClient( cl->GetId() );
        if( !cl->GetClientToDelete() )
            AddSaveClient( cl );

        // Destroy
        bool full_delete = cl->GetClientToDelete();
        EntityMngr.UnregisterEntity( cl );
        cl->IsDestroyed = true;

        // Erase radios from collection
        ItemVec items = cl->GetItemsNoLock();
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
        {
            Item* item = *it;
            if( item->GetIsRadio() )
                ItemMngr.RadioRegister( item, false );
        }

        // Full delete
        if( full_delete )
        {
            SaveClientsLocker.Lock();
            auto it = ClientsData.find( id );
            delete it->second;
            ClientsData.erase( it );
            SaveClientsLocker.Unlock();

            cl->DeleteInventory();
            DeleteClientFile( cl->Name.c_str() );
        }

        cl->Release();
    }
    else
    {
        cl->IsDestroyed = true;
        Script::RemoveEventsEntity( cl );
    }
}

void FOServer::DeleteClientFile( const char* client_name )
{
    // Make old name
    string clients_path = FileManager::GetWritePath( "Save/Clients/" );
    string old_client_fname = _str( "{}{}.client", clients_path, client_name );

    // Make new name
    string new_client_fname;
    for( uint i = 0; ; i++ )
    {
        if( !i )
            new_client_fname = _str( "{}{}.foclient_deleted", clients_path, client_name, i );
        else
            new_client_fname = _str( "{}{}~{}.foclient_deleted", clients_path, client_name, i );
        if( FileExist( new_client_fname ) )
            continue;
        break;
    }

    // Rename
    if( !FileRename( old_client_fname, new_client_fname ) )
    {
        WriteLog( "Fail to rename from '{}' to '{}'.\n", old_client_fname, new_client_fname );
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
        {
            ++it;
        }
    }
}

void FOServer::MainLoop()
{
    if( !Started() )
        return;

    // Start logic threads
    WriteLog( "***   Starting game loop   ***\n" );

    while( !FOQuit )
        LogicTick();

    WriteLog( "***   Finishing game loop  ***\n" );

    UnloadWorld( true );
}

void FOServer::LogicTick()
{
    Timer::UpdateTick();

    // Cycle time
    double frame_begin = Timer::AccurateTick();

    // Process clients
    ConnectedClientsLocker.Lock();
    ClVec clients = ConnectedClients;
    for( Client* cl : clients )
        cl->AddRef();
    ConnectedClientsLocker.Unlock();
    for( Client* cl : clients )
    {
        // Check for removing
        if( cl->IsOffline() )
        {
            DisconnectClient( cl );

            ConnectedClientsLocker.Lock();
            auto it = std::find( ConnectedClients.begin(), ConnectedClients.end(), cl );
            RUNTIME_ASSERT( it != ConnectedClients.end() );
            ConnectedClients.erase( it );
            Statistics.CurOnline--;
            ConnectedClientsLocker.Unlock();

            cl->Release();
            continue;
        }

        // Process network messages
        Process( cl );
        cl->Release();
    }

    // Process critters
    CrVec critters;
    EntityMngr.GetCritters( critters );
    for( Critter* cr : critters )
    {
        // Player specific
        if( cr->CanBeRemoved )
            RemoveClient( (Client*) cr );

        // Check for removing
        if( cr->IsDestroyed )
            continue;

        // Process logic
        ProcessCritter( cr );
    }

    // Process maps
    MapVec maps;
    EntityMngr.GetMaps( maps );
    for( Map* map : maps )
    {
        // Check for removing
        if( map->IsDestroyed )
            continue;

        // Process logic
        map->Process();
    }

    // Locations and maps garbage
    MapMngr.LocationGarbager();

    // Game time
    Timer::ProcessGameTime();

    // Bans
    ProcessBans();

    // Process pending invocations
    Script::ProcessDeferredCalls();

    // Script game loop
    Script::RaiseInternalEvent( ServerFunctions.Loop );

    // Suspended contexts
    Script::RunSuspended();

    // Fill statistics
    double frame_time = Timer::AccurateTick() - frame_begin;
    uint   loop_tick = (uint) frame_time;
    Statistics.LoopTime += loop_tick;
    Statistics.LoopCycles++;
    if( loop_tick > Statistics.LoopMax )
        Statistics.LoopMax = loop_tick;
    if( loop_tick < Statistics.LoopMin )
        Statistics.LoopMin = loop_tick;
    Statistics.CycleTime = loop_tick;
    if( loop_tick > 100 )
        Statistics.LagsCount++;
    Statistics.Uptime = ( Timer::FastTick() - Statistics.ServerStartTick ) / 1000;

    // Calculate fps
    static uint   fps = 0;
    static double fps_tick = Timer::AccurateTick();
    if( fps_tick >= 1000.0 )
    {
        Statistics.FPS = fps;
        fps = 0;
    }
    else
    {
        fps++;
    }

    // Synchronize single player data
    #ifdef FO_WINDOWS
    if( Singleplayer )
    {
        // Check client process
        if( WaitForSingleObject( SingleplayerClientProcess, 0 ) == WAIT_OBJECT_0 )
        {
            FOQuit = true;
            return;
        }

        SingleplayerData.Refresh();

        if( SingleplayerData.Pause != Timer::IsGamePaused() )
            Timer::SetGamePause( SingleplayerData.Pause );
    }
    #endif

    // World saver
    if( Timer::FastTick() >= SaveWorldNextTick )
    {
        SaveWorld( nullptr );
        SaveWorldNextTick = Timer::FastTick() + SaveWorldTime;
    }

    // Client script
    if( RequestReloadClientScripts )
    {
        ReloadClientScripts();
        RequestReloadClientScripts = false;
    }

    // Sleep
    if( ServerGameSleep >= 0 )
        Thread_Sleep( ServerGameSleep );
}

void FOServer::OnNewConnection( NetConnection* connection )
{
    ConnectedClientsLocker.Lock();
    uint count = (uint) ConnectedClients.size();
    ConnectedClientsLocker.Unlock();
    if( count >= MAX_CLIENTS_IN_GAME )
    {
        connection->Disconnect();
        return;
    }

    // Allocate client
    ProtoCritter* cl_proto = ProtoMngr.GetProtoCritter( _str( "Player" ).toHash() );
    RUNTIME_ASSERT( cl_proto );
    Client*       cl = new Client( connection, cl_proto );

    // Add to connected collection
    ConnectedClientsLocker.Lock();
    cl->GameState = STATE_CONNECTED;
    ConnectedClients.push_back( cl );
    Statistics.CurOnline++;
    ConnectedClientsLocker.Unlock();
}

void FOServer::Process( Client* cl )
{
    if( cl->IsOffline() || cl->IsDestroyed )
    {
        cl->Connection->Bin.LockReset();
        return;
    }

    uint msg = 0;
    if( cl->GameState == STATE_CONNECTED )
    {
        BIN_BEGIN( cl );
        if( cl->Connection->Bin.IsEmpty() )
            cl->Connection->Bin.Reset();
        cl->Connection->Bin.Refresh();
        if( cl->Connection->Bin.NeedProcess() )
        {
            cl->Connection->Bin >> msg;

            uint tick = Timer::FastTick();
            switch( msg )
            {
            case 0xFFFFFFFF:
            {
                uint answer[ 4 ] = { CrMngr.PlayersInGame(), Statistics.Uptime, 0, 0 };
                BOUT_BEGIN( cl );
                cl->Connection->DisableCompression();
                cl->Connection->Bout.Push( answer, sizeof( answer ) );

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
            case NETMSG_RPC:
                Script::HandleRpc( cl );
                BIN_END( cl );
                break;
            default:
                cl->Connection->Bin.SkipMsg( msg );
                BIN_END( cl );
                break;
            }

            cl->LastActivityTime = Timer::FastTick();
        }
        else
        {
            CHECK_IN_BUFF_ERROR_EXT( cl, 0, 0 );
            BIN_END( cl );
        }

        if( cl->GameState == STATE_CONNECTED && cl->LastActivityTime && Timer::FastTick() - cl->LastActivityTime > PING_CLIENT_LIFE_TIME ) // Kick bot
        {
            WriteLog( "Connection timeout, client kicked, maybe bot. Ip '{}'.\n", cl->GetIpStr() );
            cl->Disconnect();
        }
    }
    else if( cl->GameState == STATE_TRANSFERRING )
    {
        #define CHECK_BUSY                                                                                                         \
            if( cl->IsBusy() && !Singleplayer ) { cl->Connection->Bin.MoveReadPos( -int( sizeof( msg ) ) ); BIN_END( cl ); return; \
            }

        BIN_BEGIN( cl );
        if( cl->Connection->Bin.IsEmpty() )
            cl->Connection->Bin.Reset();
        cl->Connection->Bin.Refresh();
        if( cl->Connection->Bin.NeedProcess() )
        {
            cl->Connection->Bin >> msg;

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
                cl->Connection->Bin.SkipMsg( msg );
                BIN_END( cl );
                break;
            }
        }
        else
        {
            CHECK_IN_BUFF_ERROR_EXT( cl, 0, 0 );
            BIN_END( cl );
        }
    }
    else if( cl->GameState == STATE_PLAYING )
    {
        #define MESSAGES_PER_CYCLE    ( 5 )
        #define CHECK_BUSY                                                \
            if( cl->IsBusy() && !Singleplayer )                           \
            {                                                             \
                cl->Connection->Bin.MoveReadPos( -int( sizeof( msg ) ) ); \
                BIN_END( cl );                                            \
                return;                                                   \
            }

        for( int i = 0; i < MESSAGES_PER_CYCLE; i++ )
        {
            BIN_BEGIN( cl );
            if( cl->Connection->Bin.IsEmpty() )
                cl->Connection->Bin.Reset();
            cl->Connection->Bin.Refresh();
            if( !cl->Connection->Bin.NeedProcess() )
            {
                CHECK_IN_BUFF_ERROR_EXT( cl, 0, 0 );
                BIN_END( cl );
                break;
            }
            cl->Connection->Bin >> msg;

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
                Process_Command( cl->Connection->Bin, [ cl ] ( auto s )
                                 {
                                     cl->Send_Text( cl, _str( s ).trim().c_str(), SAY_NETMSG );
                                 }, cl, "" );
                BIN_END( cl );
                continue;
            }
            case NETMSG_DIR:
            {
                CHECK_BUSY;
                Process_Dir( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_MOVE_WALK:
            {
                CHECK_BUSY;
                Process_Move( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_MOVE_RUN:
            {
                CHECK_BUSY;
                Process_Move( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_TALK_NPC:
            {
                CHECK_BUSY;
                Process_Dialog( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_REFRESH_ME:
            {
                cl->Send_LoadMap( nullptr );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_GET_INFO:
            {
                Map* map = MapMngr.GetMap( cl->GetMapId() );
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
            case NETMSG_RPC:
            {
                CHECK_BUSY;
                Script::HandleRpc( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SINGLEPLAYER_SAVE_LOAD:
            {
                Process_SingleplayerSaveLoad( cl );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_POD_PROPERTY( 1, 0 ):
            case NETMSG_SEND_POD_PROPERTY( 1, 1 ):
            case NETMSG_SEND_POD_PROPERTY( 1, 2 ):
            {
                Process_Property( cl, 1 );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_POD_PROPERTY( 2, 0 ):
            case NETMSG_SEND_POD_PROPERTY( 2, 1 ):
            case NETMSG_SEND_POD_PROPERTY( 2, 2 ):
            {
                Process_Property( cl, 2 );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_POD_PROPERTY( 4, 0 ):
            case NETMSG_SEND_POD_PROPERTY( 4, 1 ):
            case NETMSG_SEND_POD_PROPERTY( 4, 2 ):
            {
                Process_Property( cl, 4 );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_POD_PROPERTY( 8, 0 ):
            case NETMSG_SEND_POD_PROPERTY( 8, 1 ):
            case NETMSG_SEND_POD_PROPERTY( 8, 2 ):
            {
                Process_Property( cl, 8 );
                BIN_END( cl );
                continue;
            }
            case NETMSG_SEND_COMPLEX_PROPERTY:
            {
                Process_Property( cl, 0 );
                BIN_END( cl );
                continue;
            }
            default:
            {
                cl->Connection->Bin.SkipMsg( msg );
                BIN_END( cl );
                continue;
            }
            }

            cl->Connection->Bin.SkipMsg( msg );
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

    cl->Connection->Bin >> msg_len;
    cl->Connection->Bin >> how_say;
    cl->Connection->Bin >> len;
    CHECK_IN_BUFF_ERROR( cl );

    if( !len || len >= sizeof( str ) )
    {
        WriteLog( "Buffer zero sized or too large, length {}. Disconnect.\n", len );
        cl->Disconnect();
        return;
    }

    cl->Connection->Bin.Pop( str, len );
    str[ len ] = '\0';
    CHECK_IN_BUFF_ERROR( cl );

    if( !cl->IsLife() && how_say >= SAY_NORM && how_say <= SAY_RADIO )
        how_say = SAY_WHISP;

    if( Str::Compare( str, cl->LastSay ) )
    {
        cl->LastSayEqualCount++;
        if( cl->LastSayEqualCount >= 10 )
        {
            WriteLog( "Flood detected, client '{}'. Disconnect.\n", cl->GetName() );
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
        if( cl->GetMapId() )
            cl->SendAA_Text( cl->VisCr, str, SAY_NORM, true );
        else
            cl->SendAA_Text( *cl->GlobalMapGroup, str, SAY_NORM, true );
    }
    break;
    case SAY_SHOUT:
    {
        if( cl->GetMapId() )
        {
            Map* map = MapMngr.GetMap( cl->GetMapId() );
            if( !map )
                return;

            CrVec critters;
            map->GetCritters( critters );

            cl->SendAA_Text( critters, str, SAY_SHOUT, true );
        }
        else
        {
            cl->SendAA_Text( *cl->GlobalMapGroup, str, SAY_SHOUT, true );
        }
    }
    break;
    case SAY_EMOTE:
    {
        if( cl->GetMapId() )
            cl->SendAA_Text( cl->VisCr, str, SAY_EMOTE, true );
        else
            cl->SendAA_Text( *cl->GlobalMapGroup, str, SAY_EMOTE, true );
    }
    break;
    case SAY_WHISP:
    {
        if( cl->GetMapId() )
            cl->SendAA_Text( cl->VisCr, str, SAY_WHISP, true );
        else
            cl->Send_TextEx( cl->GetId(), str, len, SAY_WHISP, true );
    }
    break;
    case SAY_SOCIAL:
    {
        return;
    }
    break;
    case SAY_RADIO:
    {
        if( cl->GetMapId() )
            cl->SendAA_Text( cl->VisCr, str, SAY_WHISP, true );
        else
            cl->Send_TextEx( cl->GetId(), str, len, SAY_WHISP, true );

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

    // Text listen
    int    listen_count = 0;
    int    listen_func_id[ 100 ];         // 100 calls per one message is enough
    string listen_str[ 100 ];

    TextListenersLocker.Lock();

    if( how_say == SAY_RADIO )
    {
        for( uint i = 0; i < TextListeners.size(); i++ )
        {
            TextListen& tl = TextListeners[ i ];
            if( tl.SayType == SAY_RADIO && std::find( channels.begin(), channels.end(), tl.Parameter ) != channels.end() &&
                _str( string( str ).substr( 0, tl.FirstStr.length() ) ).compareIgnoreCaseUtf8( tl.FirstStr ) )
            {
                listen_func_id[ listen_count ] = tl.FuncId;
                listen_str[ listen_count ] = str;
                if( ++listen_count >= 100 )
                    break;
            }
        }
    }
    else
    {
        Map* map = cl->GetMap();
        hash pid = ( map ? map->GetProtoId() : 0 );
        for( uint i = 0; i < TextListeners.size(); i++ )
        {
            TextListen& tl = TextListeners[ i ];
            if( tl.SayType == how_say && tl.Parameter == pid &&
                _str( string( str ).substr( 0, tl.FirstStr.length() ) ).compareIgnoreCaseUtf8( tl.FirstStr ) )
            {
                listen_func_id[ listen_count ] = tl.FuncId;
                listen_str[ listen_count ] = str;
                if( ++listen_count >= 100 )
                    break;
            }
        }
    }

    TextListenersLocker.Unlock();

    for( int i = 0; i < listen_count; i++ )
    {
        Script::PrepareContext( listen_func_id[ i ], cl->GetName() );
        Script::SetArgEntity( cl );
        Script::SetArgObject( &listen_str[ i ] );
        Script::RunPrepared();
    }
}

void FOServer::Process_Command( BufferManager& buf, LogFunc logcb, Client* cl_, const string& admin_panel )
{
    LogToFunc( logcb, true );
    Process_CommandReal( buf, logcb, cl_, admin_panel );
    LogToFunc( logcb, false );
}

void FOServer::Process_CommandReal( BufferManager& buf, LogFunc logcb, Client* cl_, const string& admin_panel )
{
    uint  msg_len = 0;
    uchar cmd = 0;

    buf >> msg_len;
    buf >> cmd;

    string sstr = ( cl_ ? "" : admin_panel );
    bool   allow_command = Script::RaiseInternalEvent( ServerFunctions.PlayerAllowCommand, cl_, &sstr, cmd );

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

        string istr = _str( istr, "|0xFF00FF00 Name: |0xFFFF0000 {}|0xFF00FF00 , Id: |0xFFFF0000 {}|0xFF00FF00 , Access: ",
                            cl_->GetName(), cl_->GetId() );
        switch( cl_->Access )
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

        logcb( istr.c_str() );
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
            result = Script::GetDeferredCallsStatistics();
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

        ConnectedClientsLocker.Lock();
        string str = _str( "Connections: {}, Players: {}, Npc: {}. FOServer machine uptime: {} min., FOServer uptime: {} min.",
                           ConnectedClients.size(), CrMngr.PlayersInGame(), CrMngr.NpcInGame(),
                           Timer::FastTick() / 1000 / 60, ( Timer::FastTick() - Statistics.ServerStartTick ) / 1000 / 60 );
        ConnectedClientsLocker.Unlock();
        result += str;

        for( const auto& line : _str( result ).split( '\n' ) )
            logcb( line );
    }
    break;
    case CMD_CRITID:
    {
        string name;
        buf >> name;

        CHECK_ALLOW_COMMAND;

        SaveClientsLocker.Lock();

        uint        id = MAKE_CLIENT_ID( name );
        ClientData* cd = GetClientData( id );
        if( cd )
            logcb( _str( "Client id is {}.", id ).c_str() );
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

        Critter* cr = CrMngr.GetCritter( crid );
        if( !cr )
        {
            logcb( "Critter not found." );
            break;
        }

        Map* map = MapMngr.GetMap( cr->GetMapId() );
        if( !map )
        {
            logcb( "Critter is on global." );
            break;
        }

        if( hex_x >= map->GetWidth() || hex_y >= map->GetHeight() )
        {
            logcb( "Invalid hex position." );
            break;
        }

        if( MapMngr.Transit( cr, map, hex_x, hex_y, cr->GetDir(), 3, 0, true ) )
            logcb( "Critter move success." );
        else
            logcb( "Critter move fail." );
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

        Critter* cr = CrMngr.GetCritter( crid );
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

        if( MapMngr.TransitToGlobal( cl_, 0, false ) == true )
            logcb( "To global success." );
        else
            logcb( "To global fail." );
    }
    break;
    case CMD_PROPERTY:
    {
        uint   crid;
        string property_name;
        int    property_value;
        buf >> crid;
        buf >> property_name;
        buf >> property_value;

        CHECK_ALLOW_COMMAND;

        Critter* cr = ( !crid ? cl_ : CrMngr.GetCritter( crid ) );
        if( cr )
        {
            Property* prop = Critter::PropertiesRegistrator->Find( property_name );
            if( !prop )
            {
                logcb( "Property not found." );
                return;
            }
            if( !prop->IsPOD() || prop->GetBaseSize() != 4 )
            {
                logcb( "For now you can modify only int/uint properties." );
                return;
            }

            prop->SetValue< int >( cr, property_value );
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
        string name_access;
        string pasw_access;
        buf >> name_access;
        buf >> pasw_access;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        StrVec client, tester, moder, admin, admin_names;
        GetAccesses( client, tester, moder, admin, admin_names );

        int wanted_access = -1;
        if( name_access == "client" && std::find( client.begin(), client.end(), pasw_access ) != client.end() )
            wanted_access = ACCESS_CLIENT;
        else if( name_access == "tester" && std::find( tester.begin(), tester.end(), pasw_access ) != tester.end() )
            wanted_access = ACCESS_TESTER;
        else if( name_access == "moder" && std::find( moder.begin(), moder.end(), pasw_access ) != moder.end() )
            wanted_access = ACCESS_MODER;
        else if( name_access == "admin" && std::find( admin.begin(), admin.end(), pasw_access ) != admin.end() )
            wanted_access = ACCESS_ADMIN;

        bool allow = false;
        if( wanted_access != -1 )
        {
            string pass = pasw_access;
            allow = Script::RaiseInternalEvent( ServerFunctions.PlayerGetAccess, cl_, wanted_access, &pass );
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
        hash   pid;
        uint   count;
        buf >> hex_x;
        buf >> hex_y;
        buf >> pid;
        buf >> count;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        Map* map = MapMngr.GetMap( cl_->GetMapId() );
        if( !map || hex_x >= map->GetWidth() || hex_y >= map->GetHeight() )
        {
            logcb( "Wrong hexes or critter on global map." );
            return;
        }

        if( !CreateItemOnHex( map, hex_x, hex_y, pid, count, nullptr, true ) )
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
        hash pid;
        uint count;
        buf >> pid;
        buf >> count;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        if( ItemMngr.AddItemCritter( cl_, pid, count ) != nullptr )
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
        hash   pid;
        buf >> hex_x;
        buf >> hex_y;
        buf >> dir;
        buf >> pid;

        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        Map* map = MapMngr.GetMap( cl_->GetMapId() );
        Npc* npc = CrMngr.CreateNpc( pid, nullptr, map, hex_x, hex_y, dir, true );
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
        hash   pid;
        buf >> wx;
        buf >> wy;
        buf >> pid;

        CHECK_ALLOW_COMMAND;

        Location* loc = MapMngr.CreateLocation( pid, wx, wy );
        if( !loc )
            logcb( "Location not created." );
        else
            logcb( "Location created." );
    }
    break;
    case CMD_RELOADSCRIPTS:
    {
        CHECK_ALLOW_COMMAND;

        Script::Undef( nullptr );
        Script::Define( "__SERVER" );
        Script::Define( _str( "__VERSION {}", FONLINE_VERSION ) );
        if( Script::ReloadScripts( "Server" ) )
            logcb( "Success." );
        else
            logcb( "Fail." );
    }
    break;
    case CMD_LOADSCRIPT:
    {
        char module_name[ MAX_FOTEXT ];
        buf.Pop( module_name, MAX_FOTEXT );
        module_name[ MAX_FOTEXT - 1 ] = 0;

        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        FilesCollection scripts( "fos" );
        const char*     path;
        FileManager&    file = scripts.FindFile( module_name, &path );
        if( file.IsLoaded() )
        {
            char dir[ MAX_FOPATH ];
            FileManager::ExtractDir( path, dir );
            if( Script::LoadModuleFromFile( module_name, file, dir, nullptr ) )
            {
                if( Script::BindImportedFunctions() )
                    logcb( "Complete." );
                else
                    logcb( Str::FormatBuf( "Complete, with errors." ) );
            }
            else
            {
                logcb( "Unable to compile script." );
            }
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

        if( ReloadClientScripts() )
            logcb( "Reload client scripts success." );
        else
            logcb( "Reload client scripts fail." );
    }
    break;
    case CMD_RUNSCRIPT:
    {
        char module_name[ MAX_FOTEXT ];
        char func_name[ MAX_FOTEXT ];
        uint param0, param1, param2;
        buf.Pop( module_name, MAX_FOTEXT );
        module_name[ MAX_FOTEXT - 1 ] = 0;
        buf.Pop( func_name, MAX_FOTEXT );
        func_name[ MAX_FOTEXT - 1 ] = 0;
        buf >> param0;
        buf >> param1;
        buf >> param2;

        CHECK_ALLOW_COMMAND;

        if( func_name.empty() )
        {
            logcb( "Fail, length is zero." );
            break;
        }

        uint bind_id = Script::BindByFuncName( func_name, "void %s(Critter, int, int, int)", true );
        if( !bind_id )
        {
            logcb( "Fail, function not found." );
            break;
        }

        Script::PrepareContext( bind_id, cl_ ? cl_->GetName() : "AdminPanel" );
        Script::SetArgObject( cl_ );
        Script::SetArgUInt( param0 );
        Script::SetArgUInt( param1 );
        Script::SetArgUInt( param2 );

        if( Script::RunPrepared() )
            logcb( "Run script success." );
        else
            logcb( "Run script fail." );
    }
    break;
    case CMD_RELOAD_PROTOS:
    {
        CHECK_ALLOW_COMMAND;

        if( ProtoMngr.LoadProtosFromFiles() )
            logcb( "Reload protos success." );
        else
            logcb( "Reload protos fail." );

        InitLangPacksLocations( LangPacks );
        InitLangPacksItems( LangPacks );
        GenerateUpdateFiles();
    }
    break;
    case CMD_LOADLOCATION:
    {
        char loc_name[ MAX_FOPATH ];
        buf.Pop( loc_name, sizeof( loc_name ) );

        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        FilesCollection locs( "foloc" );
        FileManager&    file = locs.FindFile( loc_name );
        if( file.IsLoaded() )
        {
            if( MapMngr.LoadLocationProto( loc_name, file ) )
                logcb( "Load proto location success." );
            else
                logcb( "Load proto location fail." );
        }
        else
        {
            logcb( "File not found." );
        }

        InitLangPacksLocations( LangPacks );
        GenerateUpdateFiles();

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_RELOADMAPS:
    {
        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        if( MapMngr.ReloadMaps( nullptr ) == 0 )
            logcb( "Reload proto maps complete, without fails." );
        else
            logcb( "Reload proto maps complete, with fails." );

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_LOADMAP:
    {
        char map_name[ MAX_FOPATH ];
        buf.Pop( map_name, sizeof( map_name ) );

        CHECK_ALLOW_COMMAND;

        SynchronizeLogicThreads();

        if( MapMngr.ReloadMaps( map_name ) == 0 )
            logcb( "Load proto map success." );
        else
            logcb( "Load proto map success." );

        ResynchronizeLogicThreads();
    }
    break;
    case CMD_REGENMAP:
    {
        CHECK_ALLOW_COMMAND;
        CHECK_ADMIN_PANEL;

        // Check global
        if( !cl_->GetMapId() )
        {
            logcb( "Only on local map." );
            return;
        }

        // Find map
        Map* map = MapMngr.GetMap( cl_->GetMapId() );
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
            MapMngr.Transit( cl_, map, hx, hy, dir, 5, 0, true );
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

        bool error = DlgMngr.LoadDialogs();
        InitLangPacksDialogs( LangPacks );
        GenerateUpdateFiles();
        logcb( _str( "Dialogs reload done{}.", error ? ", with errors." : "" ).c_str() );
    }
    break;
    case CMD_LOADDIALOG:
    {
        string dlg_name;
        buf >> dlg_name;

        CHECK_ALLOW_COMMAND;

        FilesCollection dialogs( "fodlg" );
        FileManager&    fm = dialogs.FindFile( dlg_name );
        if( fm.IsLoaded() )
        {
            DialogPack* pack = DlgMngr.ParseDialog( dlg_name.c_str(), (char*) fm.GetBuf() );
            if( pack )
            {
                DlgMngr.EraseDialog( pack->PackId );

                if( DlgMngr.AddDialog( pack ) )
                {
                    InitLangPacksDialogs( LangPacks );
                    GenerateUpdateFiles();
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
    }
    break;
    case CMD_RELOADTEXTS:
    {
        CHECK_ALLOW_COMMAND;

        LangPackVec lang_packs;
        if( InitLangPacks( lang_packs ) && InitLangPacksDialogs( lang_packs ) && InitLangPacksLocations( lang_packs ) && InitLangPacksItems( lang_packs ) )
        {
            LangPacks = lang_packs;
            GenerateUpdateFiles();
            logcb( "Reload texts success." );
        }
        else
        {
            lang_packs.clear();
            logcb( "Reload texts fail." );
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
        string name;
        string params;
        uint   ban_hours;
        string info;
        buf >> name;
        buf >> params;
        buf >> ban_hours;
        buf >> info;

        CHECK_ALLOW_COMMAND;

        SCOPE_LOCK( BannedLocker );

        if( _str( params ).compareIgnoreCase( "list" ) )
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
                logcb( _str( "--- {:3} ---", index ) );
                if( ban.ClientName[ 0 ] )
                    logcb( _str( "User: {}", ban.ClientName ) );
                if( ban.ClientIp )
                    logcb( _str( "UserIp: {}", ban.ClientIp ) );
                logcb( _str( "BeginTime: {} {} {} {} {}", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute ) );
                logcb( _str( "EndTime: {} {} {} {} {}", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute ) );
                if( ban.BannedBy[ 0 ] )
                    logcb( _str( "BannedBy: {}", ban.BannedBy ) );
                if( ban.BanInfo[ 0 ] )
                    logcb( _str( "Comment: {}", ban.BanInfo ) );
                index++;
            }
        }
        else if( _str( params ).compareIgnoreCase( "add" ) || _str( params ).compareIgnoreCase( "add+" ) )
        {
            uint name_len = _str( name ).lengthUtf8();
            if( name_len < MIN_NAME || name_len < GameOpt.MinNameLength || name_len > MAX_NAME || name_len > GameOpt.MaxNameLength || !ban_hours )
            {
                logcb( "Invalid arguments." );
                return;
            }

            Client*      cl_banned = CrMngr.GetPlayer( name.c_str() );
            ClientBanned ban;
            memzero( &ban, sizeof( ban ) );
            Str::Copy( ban.ClientName, name.c_str() );
            ban.ClientIp = ( cl_banned && params.find( '+' ) != string::npos ? cl_banned->GetIp() : 0 );
            Timer::GetCurrentDateTime( ban.BeginTime );
            ban.EndTime = ban.BeginTime;
            Timer::ContinueTime( ban.EndTime, ban_hours * 60 * 60 );
            Str::Copy( ban.BannedBy, cl_ ? cl_->Name.c_str() : admin_panel.c_str() );
            Str::Copy( ban.BanInfo, info.c_str() );

            Banned.push_back( ban );
            SaveBan( ban, false );
            logcb( "User banned." );

            if( cl_banned )
            {
                cl_banned->Send_TextMsg( cl_banned, STR_NET_BAN, SAY_NETMSG, TEXTMSG_GAME );
                cl_banned->Send_TextMsgLex( cl_banned, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban.GetBanLexems().c_str() );
                cl_banned->Disconnect();
            }
        }
        else if( _str( params ).compareIgnoreCase( "delete" ) )
        {
            if( name.empty() )
            {
                logcb( "Invalid arguments." );
                return;
            }

            bool resave = false;
            if( name == "*" )
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
                    if( _str( ban.ClientName ).compareIgnoreCaseUtf8( name ) )
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

        if( memcmp( cl_->GetPassword().c_str(), pass_hash, PASS_HASH_SIZE ) )
        {
            logcb( "Invalid password." );
        }
        else
        {
            if( !cl_->GetClientToDelete() )
            {
                cl_->SetClientToDelete( true );
                logcb( "Your account will be deleted after character exit from game." );
            }
            else
            {
                cl_->SetClientToDelete( false );
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

        if( memcmp( cl_->GetPassword().c_str(), pass_hash, PASS_HASH_SIZE ) )
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
                cl_->SetPassword( new_pass_hash );
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

        LogToFunc( FOServer::LogToClients, false );
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
            LogToFunc( FOServer::LogToClients, true );
    }
    break;
    case CMD_DEV_EXEC:
    case CMD_DEV_FUNC:
    case CMD_DEV_GVAR:
    {
        ushort command_len = 0;
        char   command_raw[ MAX_FOTEXT ];
        buf >> command_len;
        buf.Pop( command_raw, command_len );
        command_raw[ command_len ] = 0;

        string command = _str( command_raw ).trim();
        string console_name = _str( "DevConsole ({})", cl_ ? cl_->GetName() : admin_panel );

        // Get module
        asIScriptEngine* engine = Script::GetEngine();
        asIScriptModule* mod = engine->GetModule( console_name.c_str() );
        if( !mod )
        {
            string      base_code;
            FileManager dev_file;
            if( dev_file.LoadFile( "Scripts/__dev.fos" ) )
                base_code = (char*) dev_file.GetBuf();
            else
                base_code = "void Dummy(){}";

            mod = engine->GetModule( console_name.c_str(), asGM_ALWAYS_CREATE );
            int r = mod->AddScriptSection( "DevConsole", base_code.c_str() );
            if( r < 0 )
            {
                mod->Discard();
                break;
            }

            r = mod->Build();
            if( r < 0 )
            {
                mod->Discard();
                break;
            }

            r = mod->BindAllImportedFunctions();
            if( r < 0 )
            {
                mod->Discard();
                break;
            }
        }

        // Execute command
        if( cmd == CMD_DEV_EXEC )
        {
            WriteLog( "{} : Execute '{}'.\n", console_name, command );

            string             func_code = _str( "void Execute(){\n{};\n}", command );
            asIScriptFunction* func = nullptr;
            int                r = mod->CompileFunction( "DevConsole", func_code.c_str(), -1, asCOMP_ADD_TO_MODULE, &func );
            if( r >= 0 )
            {
                asIScriptContext* ctx = engine->RequestContext();
                if( ctx->Prepare( func ) >= 0 )
                    ctx->Execute();
                mod->RemoveFunction( func );
                func->Release();
                engine->ReturnContext( ctx );
            }
        }
        else if( cmd == CMD_DEV_FUNC )
        {
            WriteLog( "{} : Set function '{}'.\n", console_name, command );

            mod->CompileFunction( "DevConsole", command.c_str(), 0, asCOMP_ADD_TO_MODULE, nullptr );
        }
        else if( cmd == CMD_DEV_GVAR )
        {
            WriteLog( "{} : Set global var '{}'.\n", console_name, command );

            mod->CompileGlobalVar( "DevConsole", command.c_str(), 0 );
        }
    }
    break;
    default:
        logcb( "Unknown command." );
        break;
    }
}

void FOServer::SaveGameInfoFile( IniParser& data )
{
    StrMap& kv = data.GetApp( "GeneralSettings" );

    // Singleplayer info
    kv[ "Singleplayer" ] = ( SingleplayerSave.Valid ? "true" : "false" );
    if( SingleplayerSave.Valid )
    {
        StrMap& client_kv = data.SetApp( "Client" );
        SingleplayerSave.CrProps->SaveToText( client_kv, nullptr );
        client_kv[ "$Id" ] = 1;
        client_kv[ "$Name" ] = SingleplayerSave.Name;

        string pic_data_str;
        for( uchar& x : SingleplayerSave.PicData )
            pic_data_str.append( _str( "{}", x ) ).append( " " );
        if( !SingleplayerSave.PicData.empty() )
            pic_data_str.pop_back();
        kv[ "SaveLoadPicture" ] = pic_data_str;
    }

    // Real time
    DateTimeStamp cur_time;
    uint64        ft;
    Timer::GetCurrentDateTime( cur_time );
    Timer::DateTimeToFullTime( cur_time, ft );
    kv[ "RealYear" ] = _str( "{}", cur_time.Year );
    kv[ "RealMonth" ] = _str( "{}", cur_time.Month );
    kv[ "RealDay" ] = _str( "{}", cur_time.Day );
    kv[ "RealHour" ] = _str( "{}", cur_time.Hour );
    kv[ "RealMinute" ] = _str( "{}", cur_time.Minute );
    kv[ "RealSecond" ] = _str( "{}", cur_time.Second );
    kv[ "SaveTimestamp" ] = _str( "{}", ft );

    // Game time
    kv[ "YearStart" ] = _str( "{}", GameOpt.YearStart );
    kv[ "Year" ] = _str( "{}", GameOpt.Year );
    kv[ "Month" ] = _str( "{}", GameOpt.Month );
    kv[ "Day" ] = _str( "{}", GameOpt.Day );
    kv[ "Hour" ] = _str( "{}", GameOpt.Hour );
    kv[ "Minute" ] = _str( "{}", GameOpt.Minute );
    kv[ "Second" ] = _str( "{}", GameOpt.Second );
    kv[ "TimeMultiplier" ] = _str( "{}", GameOpt.TimeMultiplier );
}

bool FOServer::LoadGameInfoFile( IniParser& data )
{
    WriteLog( "Load game info...\n" );

    StrMap& kv = data.GetApp( "GeneralSettings" );

    // Singleplayer info
    if( kv.count( "Singleplayer" ) && _str( kv[ "Singleplayer" ] ).compareIgnoreCase( "true" ) )
    {
        StrMap& client_kv = data.GetApp( "Client" );
        SingleplayerSave.CrProps->LoadFromText( client_kv );
        SingleplayerSave.Name = client_kv[ "$Name" ];
        SingleplayerSave.Valid = true;
    }

    // Time
    GameOpt.YearStart = _str( kv[ "YearStart" ] ).toInt();
    GameOpt.Year = _str( kv[ "Year" ] ).toInt();
    GameOpt.Month = _str( kv[ "Month" ] ).toInt();
    GameOpt.Day = _str( kv[ "Day" ] ).toInt();
    GameOpt.Hour = _str( kv[ "Hour" ] ).toInt();
    GameOpt.Minute = _str( kv[ "Minute" ] ).toInt();
    GameOpt.Second = _str( kv[ "Second" ] ).toInt();
    GameOpt.TimeMultiplier = _str( kv[ "TimeMultiplier" ] ).toInt();
    Timer::InitGameTime();

    WriteLog( "Load game info complete.\n" );
    return true;
}

void FOServer::SetGameTime( int multiplier, int year, int month, int day, int hour, int minute, int second )
{
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
            cl->Send_GameInfo( MapMngr.GetMap( cl->GetMapId() ) );
    }
    ConnectedClientsLocker.Unlock();
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
    WriteLog( "***   Starting initialization   ***\n" );

    FileManager::InitDataFiles( "./" );

    // Delete intermediate files if engine have been updated
    FileManager fm;
    if( !fm.LoadFile( FileManager::GetWritePath( "Version.txt" ) ) || _str( fm.GetCStr() ).toInt() != FONLINE_VERSION || GameOpt.ForceRebuildResources )
    {
        fm.SetStr( _str( "{}", FONLINE_VERSION ) );
        fm.SaveFile( "Version.txt" );
        FileManager::DeleteDir( "Update/" );
        FileManager::DeleteDir( "Binaries/" );
    }

    // Generic
    ConnectedClients.clear();
    SaveClients.clear();
    RegIp.clear();

    // Profiler
    uint sample_time = MainConfig->GetInt( "", "ProfilerSampleInterval", 0 );
    uint profiler_mode = MainConfig->GetInt( "", "ProfilerMode", 0 );
    if( !profiler_mode )
        sample_time = 0;

    if( !Singleplayer )
    {
        // Reserve memory
        ConnectedClients.reserve( MAX_CLIENTS_IN_GAME );
        SaveClients.reserve( MAX_CLIENTS_IN_GAME );
    }

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
    if( !DlgMngr.LoadDialogs() )
        return false;                    // Dialog manager
    if( !ProtoMngr.LoadProtosFromFiles() )
        return false;

    // Language packs
    if( !InitLangPacksDialogs( LangPacks ) )
        return false;                            // Create FODLG.MSG, need call after InitLangPacks and DlgMngr.LoadDialogs
    if( !InitLangPacksLocations( LangPacks ) )
        return false;                            // Create FOLOCATIONS.MSG, need call after InitLangPacks and MapMngr.LoadLocationsProtos
    if( !InitLangPacksItems( LangPacks ) )
        return false;                            // Create FOITEM.MSG, need call after InitLangPacks and ItemMngr.LoadProtos

    // Scripts post check
    if( !Script::PostInitScriptSystem() )
        return false;

    // Modules initialization
    if( !Script::RunModuleInitFunctions() )
        return false;

    // Update files
    StrVec resource_names;
    GenerateUpdateFiles( true, &resource_names );

    // Validate protos resources
    if( !ProtoMngr.ValidateProtoResources( resource_names ) )
        return false;

    // Initialization script
    if( !Script::RaiseInternalEvent( ServerFunctions.Init ) )
    {
        WriteLog( "Initialization script return false.\n" );
        return false;
    }

    // World loading
    if( GameOpt.GameServer && !Singleplayer )
    {
        // Copy of data
        if( !LoadWorld( nullptr ) )
            return false;

        // Try generate world if not exist
        if( !MapMngr.GetLocationsCount() && !NewWorld() )
            return false;
    }

    // End of initialization
    Statistics.BytesSend = 0;
    Statistics.BytesRecv = 0;
    Statistics.DataReal = 1;
    Statistics.DataCompressed = 1;
    Statistics.ServerStartTick = Timer::FastTick();

    // Net
    ushort port;
    if( !Singleplayer )
    {
        port = MainConfig->GetInt( "", "Port", 4000 );
        if( GameOpt.UpdateServer && !GameOpt.GameServer )
        {
            ushort update_port = (ushort) MainConfig->GetInt( "", "update" );
            if( update_port )
                port = update_port;
        }
        WriteLog( "Starting server on port {} and {}.\n", port, port + 1 );
    }
    else
    {
        port = 0;
        WriteLog( "Starting server on free port.\n", port );
    }

    if( !( TcpServer = NetServerBase::StartTcpServer( port, FOServer::OnNewConnection ) ) )
        return false;
    if( !( WebSocketsServer = NetServerBase::StartWebSocketsServer( port + 1, FOServer::OnNewConnection ) ) )
        return false;

    // Script timeouts
    Script::SetRunTimeout( GameOpt.ScriptRunSuspendTimeout, GameOpt.ScriptRunMessageTimeout );

    // World saver
    SaveWorldTime = MainConfig->GetInt( "", "WorldSaveTime", 60 ) * 60 * 1000;
    SaveWorldNextTick = Timer::FastTick() + SaveWorldTime;

    // Inform client about end of initialization
    #ifdef FO_WINDOWS
    if( Singleplayer )
    {
        if( !SingleplayerData.Lock() )
            return false;
        SingleplayerData.NetPort = port;
        SingleplayerData.Unlock();
    }
    #endif

    Active = true;
    return true;
}

bool FOServer::InitLangPacks( LangPackVec& lang_packs )
{
    WriteLog( "Load language packs...\n" );

    uint cur_lang = 0;
    while( true )
    {
        string cur_str_lang = _str( "Language_{}", cur_lang );
        string lang_name = MainConfig->GetStr( "", cur_str_lang );
        if( lang_name.empty() )
            break;

        if( lang_name.length() != 4 )
        {
            WriteLog( "Language name not equal to four letters.\n" );
            return false;
        }

        uint pack_id = *(uint*) &lang_name;
        if( std::find( lang_packs.begin(), lang_packs.end(), pack_id ) != lang_packs.end() )
        {
            WriteLog( "Language pack {} is already initialized.\n", cur_lang );
            return false;
        }

        LanguagePack lang;
        if( !lang.LoadFromFiles( lang_name.c_str() ) )
        {
            WriteLog( "Unable to init Language pack {}.\n", cur_lang );
            return false;
        }

        lang_packs.push_back( lang );
        cur_lang++;
    }

    WriteLog( "Load language packs complete, count {}.\n", cur_lang );
    return cur_lang > 0;
}

bool FOServer::InitLangPacksDialogs( LangPackVec& lang_packs )
{
    for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
        it->Msg[ TEXTMSG_DLG ].Clear();

    auto& all_protos = ProtoMngr.GetProtoCritters();
    for( auto& kv : all_protos )
    {
        ProtoCritter* proto = kv.second;
        for( uint i = 0, j = (uint) proto->TextsLang.size(); i < j; i++ )
        {
            for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
            {
                LanguagePack& lang = *it;
                if( proto->TextsLang[ i ] != lang.Name )
                    continue;

                if( lang.Msg[ TEXTMSG_DLG ].IsIntersects( *proto->Texts[ i ] ) )
                    WriteLog( "Warning! Proto item '{}' text intersection detected, send notification about this to developers.\n", proto->GetName() );

                lang.Msg[ TEXTMSG_DLG ] += *proto->Texts[ i ];
            }
        }
    }

    DialogPack* pack = nullptr;
    uint        index = 0;
    while( pack = DlgMngr.GetDialogByIndex( index++ ) )
    {
        for( uint i = 0, j = (uint) pack->TextsLang.size(); i < j; i++ )
        {
            for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
            {
                LanguagePack& lang = *it;
                if( pack->TextsLang[ i ] != lang.Name )
                    continue;

                if( lang.Msg[ TEXTMSG_DLG ].IsIntersects( *pack->Texts[ i ] ) )
                    WriteLog( "Warning! Dialog '{}' text intersection detected, send notification about this to developers.\n", pack->PackName.c_str() );

                lang.Msg[ TEXTMSG_DLG ] += *pack->Texts[ i ];
            }
        }
    }

    return true;
}

bool FOServer::InitLangPacksLocations( LangPackVec& lang_packs )
{
    for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
        it->Msg[ TEXTMSG_LOCATIONS ].Clear();

    auto& protos = ProtoMngr.GetProtoLocations();
    for( auto& kv : protos )
    {
        ProtoLocation* ploc = kv.second;
        for( uint i = 0, j = (uint) ploc->TextsLang.size(); i < j; i++ )
        {
            for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
            {
                LanguagePack& lang = *it;
                if( ploc->TextsLang[ i ] != lang.Name )
                    continue;

                if( lang.Msg[ TEXTMSG_LOCATIONS ].IsIntersects( *ploc->Texts[ i ] ) )
                    WriteLog( "Warning! Location '{}' text intersection detected, send notification about this to developers.\n", _str().parseHash( ploc->ProtoId ) );

                lang.Msg[ TEXTMSG_LOCATIONS ] += *ploc->Texts[ i ];
            }
        }
    }

    return true;
}

bool FOServer::InitLangPacksItems( LangPackVec& lang_packs )
{
    for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
        it->Msg[ TEXTMSG_ITEM ].Clear();

    auto& protos = ProtoMngr.GetProtoItems();
    for( auto& kv : protos )
    {
        ProtoItem* proto = kv.second;
        for( uint i = 0, j = (uint) proto->TextsLang.size(); i < j; i++ )
        {
            for( auto it = lang_packs.begin(), end = lang_packs.end(); it != end; ++it )
            {
                LanguagePack& lang = *it;
                if( proto->TextsLang[ i ] != lang.Name )
                    continue;

                if( lang.Msg[ TEXTMSG_ITEM ].IsIntersects( *proto->Texts[ i ] ) )
                    WriteLog( "Warning! Proto item '{}' text intersection detected, send notification about this to developers.\n", proto->GetName() );

                lang.Msg[ TEXTMSG_ITEM ] += *proto->Texts[ i ];
            }
        }
    }

    return true;
}

#pragma MESSAGE("Clients logging may be not thread safe.")
void FOServer::LogToClients( const string& str )
{
    ushort str_len = (ushort) str.length();
    if( str_len && str[ str_len - 1 ] == '\n' )
        str_len--;
    if( str_len )
    {
        for( auto it = LogClients.begin(); it < LogClients.end();)
        {
            Client* cl = *it;
            if( cl->IsOnline() )
            {
                cl->Send_TextEx( 0, str.c_str(), str_len, SAY_NETMSG, false );
                ++it;
            }
            else
            {
                cl->Release();
                it = LogClients.erase( it );
            }
        }
        if( LogClients.empty() )
            LogToFunc( FOServer::LogToClients, false );
    }
}

FOServer::ClientBanned* FOServer::GetBanByName( const char* name )
{
    auto it = std::find( Banned.begin(), Banned.end(), name );
    return it != Banned.end() ? &( *it ) : nullptr;
}

FOServer::ClientBanned* FOServer::GetBanByIp( uint ip )
{
    auto it = std::find( Banned.begin(), Banned.end(), ip );
    return it != Banned.end() ? &( *it ) : nullptr;
}

uint FOServer::GetBanTime( ClientBanned& ban )
{
    DateTimeStamp time;
    Timer::GetCurrentDateTime( time );
    int           diff = Timer::GetTimeDifference( ban.EndTime, time ) / 60 + 1;
    return diff > 0 ? diff : 1;
}

void FOServer::ProcessBans()
{
    SCOPE_LOCK( BannedLocker );

    bool          resave = false;
    DateTimeStamp time;
    Timer::GetCurrentDateTime( time );
    for( auto it = Banned.begin(); it != Banned.end();)
    {
        DateTimeStamp& ban_time = it->EndTime;
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
    if( !fm.LoadFile( fname ) )
    {
        WriteLog( "Can't open file '{}'.\n", fname );
        return;
    }
    fm.SwitchToWrite();

    fm.SetStr( "[Ban]\n" );
    if( ban.ClientName[ 0 ] )
        fm.SetStr( _str( "User = {}\n", ban.ClientName ) );
    if( ban.ClientIp )
        fm.SetStr( _str( "UserIp = {}\n", ban.ClientIp ) );
    fm.SetStr( _str( "BeginTime = {} {} {} {} {}\n", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute ) );
    fm.SetStr( _str( "EndTime = {} {} {} {} {}\n", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute ) );
    if( ban.BannedBy[ 0 ] )
        fm.SetStr( _str( "BannedBy = {}\n", ban.BannedBy ) );
    if( ban.BanInfo[ 0 ] )
        fm.SetStr( _str( "Comment = {}\n", ban.BanInfo ) );
    fm.SetStr( "\n" );

    if( !fm.SaveFile( fname ) )
        WriteLog( "Unable to save file '{}'.\n", fname );
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
            fm.SetStr( _str( "User = {}\n", ban.ClientName ) );
        if( ban.ClientIp )
            fm.SetStr( _str( "UserIp = {}\n", ban.ClientIp ) );
        fm.SetStr( _str( "BeginTime = {} {} {} {} {}\n", ban.BeginTime.Year, ban.BeginTime.Month, ban.BeginTime.Day, ban.BeginTime.Hour, ban.BeginTime.Minute ) );
        fm.SetStr( _str( "EndTime = {} {} {} {} {}\n", ban.EndTime.Year, ban.EndTime.Month, ban.EndTime.Day, ban.EndTime.Hour, ban.EndTime.Minute ) );
        if( ban.BannedBy[ 0 ] )
            fm.SetStr( _str( "BannedBy = {}\n", ban.BannedBy ) );
        if( ban.BanInfo[ 0 ] )
            fm.SetStr( _str( "Comment = {}\n", ban.BanInfo ) );
        fm.SetStr( "\n" );
    }

    if( !fm.SaveFile( BANS_FNAME_ACTIVE ) )
        WriteLog( "Unable to save file '{}'.\n", BANS_FNAME_ACTIVE );
}

void FOServer::LoadBans()
{
    SCOPE_LOCK( BannedLocker );

    Banned.clear();
    Banned.reserve( 1000 );
    IniParser bans_txt;
    if( !bans_txt.AppendFile( BANS_FNAME_ACTIVE ) )
        return;

    while( bans_txt.IsApp( "Ban" ) )
    {
        ClientBanned  ban;
        memzero( &ban, sizeof( ban ) );
        DateTimeStamp time;
        memzero( &time, sizeof( time ) );
        string        s;
        if( !( s = bans_txt.GetStr( "Ban", "User" ) ).empty() )
            Str::Copy( ban.ClientName, s.c_str() );
        ban.ClientIp = bans_txt.GetInt( "Ban", "UserIp", 0 );
        if( !( s = bans_txt.GetStr( "Ban", "BeginTime" ) ).empty() && sscanf( s.c_str(), "%hu%hu%hu%hu%hu", &time.Year, &time.Month, &time.Day, &time.Hour, &time.Minute ) )
            ban.BeginTime = time;
        if( !( s = bans_txt.GetStr( "Ban", "EndTime" ) ).empty() && sscanf( s.c_str(), "%hu%hu%hu%hu%hu", &time.Year, &time.Month, &time.Day, &time.Hour, &time.Minute ) )
            ban.EndTime = time;
        if( !( s = bans_txt.GetStr( "Ban", "BannedBy" ) ).empty() )
            Str::Copy( ban.BannedBy, s.c_str() );
        if( !( s = bans_txt.GetStr( "Ban", "Comment" ) ).empty() )
            Str::Copy( ban.BanInfo, s.c_str() );
        Banned.push_back( ban );

        bans_txt.GotoNextApp( "Ban" );
    }
    ProcessBans();
}

bool FOServer::LoadClientsData()
{
    WriteLog( "Indexing client data...\n" );

    // Path to client saves
    string clients_path = "Save/Clients/";

    // Index clients
    int    errors = 0;
    string file_find_fname;
    void*  file_find = nullptr;
    while( true )
    {
        if( !file_find )
        {
            file_find = FileFindFirst( clients_path, "foclient", &file_find_fname, nullptr, nullptr, nullptr );
            if( !file_find )
                break;
        }
        else
        {
            if( !FileFindNext( file_find, &file_find_fname, nullptr, nullptr, nullptr ) )
                break;
        }

        string    client_name = file_find_fname;
        string    name = _str( client_name ).eraseFileExtension();
        string    client_fname = clients_path + client_name;
        IniParser client_data;
        if( !client_data.AppendFile( client_fname ) )
        {
            WriteLog( "Unable to open client save file '{}'.\n", client_fname );
            errors++;
            continue;
        }

        // Generate user id
        uint id = MAKE_CLIENT_ID( name );
        RUNTIME_ASSERT( id != 0 );

        // Check uniqueness of id
        auto it = ClientsData.find( id );
        if( it != ClientsData.end() )
        {
            WriteLog( "Collisions of user id for client '{}' and client '{}'.\n", name, it->second->ClientName );
            errors++;
            continue;
        }

        // Add client information
        ClientData* data = new ClientData();
        memzero( data, sizeof( ClientData ) );
        Str::Copy( data->ClientName, name.c_str() );
        string pass_hash_str = client_data.GetStr( "Client", "Password" );
        memcpy( data->ClientPassHash, pass_hash_str.c_str(), pass_hash_str.length() );
        ClientsData.insert( std::make_pair( id, data ) );
    }

    // Clean up
    if( file_find )
        FileFindClose( file_find );

    // Fail
    if( errors )
        return false;

    WriteLog( "Indexing client data complete, count {}.\n", ClientsData.size() );
    return true;
}

FOServer::ClientData* FOServer::GetClientData( uint id )
{
    auto it = ClientsData.find( id );
    return it != ClientsData.end() ? it->second : nullptr;
}

bool FOServer::SaveClient( Client* cl )
{
    if( Singleplayer )
        return true;

    RUNTIME_ASSERT( cl->Name != "err" );
    RUNTIME_ASSERT( cl->GetId() );

    IniParser data;
    string    fname = "Save/Clients/" + cl->Name + ".foclient";
    cl->Props.SaveToText( data.SetApp( "Client" ), &cl->Proto->Props );
    if( !data.SaveFile( fname ) )
    {
        WriteLog( "Unable to save client '{}'.\n", fname );
        return false;
    }
    return true;
}

bool FOServer::LoadClient( Client* cl )
{
    if( Singleplayer )
        return true;

    string    fname = _str( "Save/Clients/{}.foclient", cl->Name );
    IniParser client_data;
    if( !client_data.AppendFile( fname ) )
    {
        WriteLog( "Unable to open client save file '{}'.\n", fname );
        return false;
    }
    if( !client_data.IsApp( "Client" ) )
    {
        WriteLog( "Client save file '{}' truncated.\n", fname );
        return false;
    }

    // Read data
    if( !cl->Props.LoadFromText( client_data.GetApp( "Client" ) ) )
    {
        WriteLog( "Client save file '{}' truncated.\n", fname );
        return false;
    }

    return true;
}

bool FOServer::NewWorld()
{
    UnloadWorld( false );

    Script::RaiseInternalEvent( ServerFunctions.GenerateWorld, &GameOpt.TimeMultiplier, &GameOpt.YearStart,
                                &GameOpt.Month, &GameOpt.Day, &GameOpt.Hour, &GameOpt.Minute );

    GameOpt.YearStart = CLAMP( GameOpt.YearStart, 1700, 30000 );
    GameOpt.Year = GameOpt.YearStart;
    GameOpt.Second = 0;
    Timer::InitGameTime();

    // Start script
    if( !Script::RaiseInternalEvent( ServerFunctions.Start ) )
    {
        WriteLog( "Start script fail.\n" );
        return false;
    }

    return true;
}

void FOServer::SaveWorld( const char* fname )
{
    if( !GameOpt.GameServer )
        return;
    if( !fname && Singleplayer )
        return;                           // Disable autosaving in singleplayer mode

    // Do nessesary stuff before save
    Script::RunMandatorySuspended();

    // Wait previous saving
    DumpThread.Wait();

    // Time counter
    double tick = Timer::AccurateTick();

    // Script callback
    SaveWorldDeleteIndexes.clear();
    CScriptArray* delete_indexes = Script::CreateArray( "uint[]" );
    if( Script::RaiseInternalEvent( ServerFunctions.WorldSave, SaveWorldIndex + 1, delete_indexes ) )
        Script::AssignScriptArrayInVector( SaveWorldDeleteIndexes, delete_indexes );
    delete_indexes->Release();

    // Make file name
    string final_name;
    if( !fname )
    {
        final_name = _str( "{}Auto{:04}.foworld", FileManager::GetWritePath( "Save/" ), SaveWorldIndex + 1 );
        if( ++SaveWorldIndex >= WORLD_SAVE_MAX_INDEX )
            SaveWorldIndex = 0;
    }
    else
    {
        final_name = fname;
    }

    // Collect data
    IniParser* data = new IniParser();
    data->SetApp( "GeneralSettings" );
    data->SetApp( "Globals" );
    SaveGameInfoFile( *data );
    Script::SaveDeferredCalls( *data );
    DumpEntity( Globals );
    EntityMngr.DumpEntities( DumpEntity, *data );
    ConnectedClientsLocker.Lock();
    for( auto& cl : ConnectedClients )
        if( cl->Id )
            DumpEntity( cl );
    ConnectedClientsLocker.Unlock();
    SaveClientsLocker.Lock();
    for( auto& cl : SaveClients )
    {
        DumpEntity( cl );
        cl->Release();
    }
    SaveClients.clear();
    SaveClientsLocker.Unlock();

    // Flush on disk
    void* args = new void*[ 2 ]
    {
        data,
        Str::Duplicate( final_name ),
    };
    if( !Singleplayer )
        DumpThread.Start( Dump_Work, "SaveWorld", args );
    else
        Dump_Work( args );

    // Report
    WriteLog( "World saved in {} ms.\n", Timer::AccurateTick() - tick );
}

bool FOServer::LoadWorld( const char* fname )
{
    UnloadWorld( false );

    IniParser data;
    if( !fname )
    {
        for( int i = WORLD_SAVE_MAX_INDEX; i >= 1; i-- )
        {
            string auto_fname = _str( auto_fname, "Save/Auto{:04}.foworld", i );
            if( data.AppendFile( auto_fname ) )
            {
                WriteLog( "Load world from dump file '{}'.\n", auto_fname );
                SaveWorldIndex = i;
                break;
            }
        }

        if( !data.IsLoaded() )
        {
            WriteLog( "World dump file not found.\n" );
            SaveWorldIndex = 0;
            return true;
        }
    }
    else
    {
        if( !data.AppendFile( fname ) )
        {
            WriteLog( "World dump file '{}' not found.\n", fname );
            return false;
        }

        WriteLog( "Load world from dump file '{}'.\n", fname );
    }

    // Main data
    _str::loadHashes( data.GetApp( "Hashes" ) );
    if( !LoadGameInfoFile( data ) )
        return false;
    if( !Script::LoadDeferredCalls( data ) )
        return false;
    if( !Globals->Props.LoadFromText( data.GetApp( "Global" ) ) )
        return false;
    if( !EntityMngr.LoadEntities( data ) )
        return false;

    // Start script
    if( !Script::RaiseInternalEvent( ServerFunctions.Start ) )
    {
        WriteLog( "Start script fail.\n" );
        return false;
    }

    return true;
}

void FOServer::UnloadWorld( bool save )
{
    // Last process
    Script::RaiseInternalEvent( ServerFunctions.Finish );

    // Save
    if( save )
        SaveWorld( nullptr );

    // Clean up
    ItemMngr.RadioClear();
    EntityMngr.ClearEntities();
    SingleplayerSave.Valid = false;
}

void FOServer::DumpEntity( Entity* entity )
{
    EntityDump* d = new EntityDump();

    const char* type_name;
    switch( entity->Type )
    {
    case EntityType::Location:
        type_name = "Location";
        break;
    case EntityType::Map:
        type_name = "Map";
        break;
    case EntityType::Npc:
        type_name = "Npc";
        break;
    case EntityType::Item:
        type_name = "Item";
        break;
    case EntityType::Custom:
        type_name = "Custom";
        break;
    case EntityType::Global:
        type_name = "Global";
        break;
    case EntityType::Client:
        type_name = "Client";
        break;
    default:
        RUNTIME_ASSERT( !"Unreachable place" );
        break;
    }

    d->IsClient = ( entity->Type == EntityType::Client );
    d->TypeName = type_name;
    d->Props = new Properties( entity->Props );
    d->ProtoProps = ( entity->Proto ? &entity->Proto->Props : nullptr );
    StrMap& kv = d->ExtraData;
    if( entity->Id )
        kv[ "$Id" ] = _str( "{}", entity->Id );
    if( entity->Proto )
        kv[ "$Proto" ] = _str().parseHash( entity->Proto->ProtoId );
    if( entity->Type == EntityType::Custom )
        kv[ "$ClassName" ] = ( (CustomEntity*) entity )->Props.GetClassName();
    if( entity->Type == EntityType::Client )
        d->TypeName = string( ( (Client*) entity )->Name ).append( ".foclient" );

    DumpedEntities.push_back( d );
}

void FOServer::Dump_Work( void* args )
{
    double      tick = Timer::AccurateTick();
    IniParser*  data = (IniParser*) ( (void**) args )[ 0 ];
    const char* fname = (const char*) ( (void**) args )[ 1 ];

    // Collect data
    uint index = 0;
    for( auto& d : DumpedEntities )
    {
        if( d->IsClient )
        {
            IniParser client_data;
            StrMap&   kv = client_data.SetApp( "Client" );
            kv.insert( d->ExtraData.begin(), d->ExtraData.end() );
            d->Props->SaveToText( kv, d->ProtoProps );
            RUNTIME_ASSERT( !Str::Compare( d->TypeName.c_str(), "err.foclient" ) );
            if( !client_data.SaveFile( "Save/Clients/" + d->TypeName ) )
                WriteLog( "Unable to save client to 'Save/Clients/{}'.\n", d->TypeName );

            // Sleep some time
            Thread_Sleep( 1 );
        }
        else
        {
            StrMap& kv = data->SetApp( d->TypeName );
            kv.insert( d->ExtraData.begin(), d->ExtraData.end() );
            d->Props->SaveToText( kv, d->ProtoProps );

            // Sleep some time
            if( index++ % 1000 == 0 )
                Thread_Sleep( 1 );
        }

        delete d->Props;
        delete d;
    }
    DumpedEntities.clear();

    // Save hashes
    _str::saveHashes( data->SetApp( "Hashes" ) );

    // Save world to file
    if( !data->SaveFile( fname ) )
        WriteLog( "Unable to save world to '{}'.\n", fname );

    // Delete old dump files
    for( uint index : SaveWorldDeleteIndexes )
    {
        string dir = _str( fname ).extractDir();
        string path = _str( "{}Auto{:04}.foworld", dir, index );
        void*  f = FileOpen( path, false );
        if( f )
        {
            FileClose( f );
            FileDelete( path );
        }
    }
    SaveWorldDeleteIndexes.clear();

    // Clean up
    delete data;
    delete[] fname;
    delete[] args;

    // Report
    WriteLog( "World flushed on disk in {} ms.\n", Timer::AccurateTick() - tick );
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

void FOServer::GenerateUpdateFiles( bool first_generation /* = false */, StrVec* resource_names /* = nullptr */ )
{
    if( !GameOpt.UpdateServer )
        return;
    if( !first_generation && UpdateFiles.empty() )
        return;

    WriteLog( "Generate update files...\n" );

    // Disconnect all connected clients to force data updating
    if( !first_generation )
    {
        ConnectedClientsLocker.Lock();
        for( auto it = ConnectedClients.begin(), end = ConnectedClients.end(); it != end; ++it )
            ( *it )->Disconnect();
    }

    // Generate resources
    bool changed = ResourceConverter::Generate( resource_names );

    // Clear collections
    for( auto it = UpdateFiles.begin(), end = UpdateFiles.end(); it != end; ++it )
        SAFEDELA( it->Data );
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

            string msg_cache_name = lang_pack.GetMsgCacheName( i );

            memzero( &update_file, sizeof( update_file ) );
            update_file.Size = (uint) msg_data.size();
            update_file.Data = new uchar[ update_file.Size ];
            memcpy( update_file.Data, &msg_data[ 0 ], update_file.Size );
            UpdateFiles.push_back( update_file );

            WriteData( UpdateFilesList, (short) msg_cache_name.length() );
            WriteDataArr( UpdateFilesList, msg_cache_name.c_str(), (uint) msg_cache_name.length() );
            WriteData( UpdateFilesList, update_file.Size );
            WriteData( UpdateFilesList, Crypt.MurmurHash2( update_file.Data, update_file.Size ) );
        }
    }

    // Fill prototypes
    UCharVec proto_items_data;
    ProtoMngr.GetBinaryData( proto_items_data );

    memzero( &update_file, sizeof( update_file ) );
    update_file.Size = (uint) proto_items_data.size();
    update_file.Data = new uchar[ update_file.Size ];
    memcpy( update_file.Data, &proto_items_data[ 0 ], update_file.Size );
    UpdateFiles.push_back( update_file );

    const string protos_cache_name = "$protos.cache";
    WriteData( UpdateFilesList, (short) protos_cache_name.length() );
    WriteDataArr( UpdateFilesList, protos_cache_name.c_str(), (uint) protos_cache_name.length() );
    WriteData( UpdateFilesList, update_file.Size );
    WriteData( UpdateFilesList, Crypt.MurmurHash2( update_file.Data, update_file.Size ) );

    // Fill files
    StrVec file_paths;
    FileManager::GetFolderFileNames( "Update/", true, "", file_paths );
    for( size_t i = 0; i < file_paths.size(); i++ )
    {
        string      file_path = file_paths[ i ];
        FileManager file;
        if( !file.LoadFile( ( "Update/" + file_path ).c_str() ) )
        {
            WriteLog( "Can't load file '{}'.\n", file_path.c_str() );
            continue;
        }

        memzero( &update_file, sizeof( update_file ) );
        update_file.Size = file.GetFsize();
        update_file.Data = file.ReleaseBuffer();
        UpdateFiles.push_back( update_file );

        WriteData( UpdateFilesList, (short) file_path.length() );
        WriteDataArr( UpdateFilesList, file_path.c_str(), (uint) file_path.length() );
        WriteData( UpdateFilesList, update_file.Size );
        WriteData( UpdateFilesList, Crypt.MurmurHash2( update_file.Data, update_file.Size ) );
    }

    WriteLog( "Generate update files complete.\n" );

    // Callback after generation
    if( first_generation && changed )
        Script::RaiseInternalEvent( ServerFunctions.ResourcesGenerated );

    // Append binaries
    StrVec binary_paths;
    FileManager::GetFolderFileNames( "Binaries/", true, "", binary_paths );
    for( size_t i = 0; i < binary_paths.size(); i++ )
    {
        string      file_path = binary_paths[ i ];
        FileManager file;
        if( !file.LoadFile( ( "Binaries/" + file_path ).c_str() ) )
        {
            WriteLog( "Can't load file '{}'.\n", file_path.c_str() );
            continue;
        }

        memzero( &update_file, sizeof( update_file ) );
        update_file.Size = file.GetFsize();
        update_file.Data = file.ReleaseBuffer();
        UpdateFiles.push_back( update_file );

        WriteData( UpdateFilesList, (short) file_path.length() );
        WriteDataArr( UpdateFilesList, file_path.c_str(), (uint) file_path.length() );
        WriteData( UpdateFilesList, update_file.Size );
        WriteData( UpdateFilesList, Crypt.MurmurHash2( update_file.Data, update_file.Size ) );
    }

    // Complete files list
    WriteData( UpdateFilesList, (short) -1 );

    // Allow clients to connect
    if( !first_generation )
        ConnectedClientsLocker.Unlock();
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
    auto it = BruteForceIps.insert( std::make_pair( ip, std::make_pair( tick, 0 ) ) );

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
    auto it = BruteForceNames.insert( std::make_pair( string( name ), tick ) );

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
