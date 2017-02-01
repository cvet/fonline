#include "Common.h"
#include "Server.h"

void FOServer::ProcessCritter( Critter* cr )
{
    if( cr->CanBeRemoved || cr->IsDestroyed )
        return;
    if( Timer::IsGamePaused() )
        return;

    uint tick = Timer::GameTick();

    // Idle functions
    if( tick >= cr->IdleNextTick )
    {
        Script::RaiseInternalEvent( ServerFunctions.CritterIdle, cr );
        if( !cr->GetMapId() )
            Script::RaiseInternalEvent( ServerFunctions.CritterGlobalMapIdle, cr );
        cr->IdleNextTick = tick + GameOpt.CritterIdleTick;
    }

    // Internal misc/drugs time events
    // One event per cycle
    if( cr->IsNonEmptyTE_FuncNum() )
    {
        CScriptArray* te_next_time = cr->GetTE_NextTime();
        uint          next_time = *(uint*) te_next_time->At( 0 );
        if( !next_time || GameOpt.FullSecond >= next_time )
        {
            CScriptArray* te_func_num = cr->GetTE_FuncNum();
            CScriptArray* te_rate = cr->GetTE_Rate();
            CScriptArray* te_identifier = cr->GetTE_Identifier();
            RUNTIME_ASSERT( te_next_time->GetSize() == te_func_num->GetSize() );
            RUNTIME_ASSERT( te_func_num->GetSize() == te_rate->GetSize() );
            RUNTIME_ASSERT( te_rate->GetSize() == te_identifier->GetSize() );
            hash func_num = *(hash*) te_func_num->At( 0 );
            uint rate = *(hash*) te_rate->At( 0 );
            int  identifier = *(hash*) te_identifier->At( 0 );
            te_func_num->Release();
            te_rate->Release();
            te_identifier->Release();

            cr->EraseCrTimeEvent( 0 );

            uint time = GameOpt.TimeMultiplier * 1800;             // 30 minutes on error
            Script::PrepareScriptFuncContext( func_num, cr->GetInfo() );
            Script::SetArgEntity( cr );
            Script::SetArgUInt( identifier );
            Script::SetArgAddress( &rate );
            if( Script::RunPrepared() )
                time = Script::GetReturnedUInt();
            if( time )
                cr->AddCrTimeEvent( func_num, rate, time, identifier );
        }
        te_next_time->Release();
    }

    // Client
    if( cr->IsPlayer() )
    {
        // Cast
        Client* cl = (Client*) cr;

        // Talk
        cl->ProcessTalk( false );

        // Ping client
        if( cl->IsToPing() )
        {
            #ifndef DEV_VERSION
            if( GameOpt.AccountPlayTime && Random( 0, 3 ) == 0 )
                cl->Send_CheckUIDS();
            #endif
            cl->PingClient();
            if( cl->GetMapId() )
                MapMngr.TransitToMapHex( cr, MapMngr.GetMap( cr->GetMapId() ), cr->GetHexX(), cr->GetHexY(), cr->GetDir(), false );
        }

        // Kick from game
        if( cl->IsOffline() && cl->IsLife() && !IS_TIMEOUT( cl->GetTimeoutBattle() ) &&
            !cl->GetTimeoutRemoveFromGame() && cl->GetOfflineTime() >= GameOpt.MinimumOfflineTime )
        {
            cl->RemoveFromGame();
        }

        // Cache look distance
        #pragma MESSAGE("Disable look distance caching")
        if( tick >= cl->CacheValuesNextTick )
        {
            cl->LookCacheValue = cl->GetLookDistance();
            cl->CacheValuesNextTick = tick + 3000;
        }
    }
    // Npc
    else
    {
        // Cast
        Npc* npc = (Npc*) cr;

        // Process
        if( npc->IsLife() )
            ProcessAI( npc );
    }
}

bool FOServer::Act_Move( Critter* cr, ushort hx, ushort hy, uint move_params )
{
    uint map_id = cr->GetMapId();
    if( !map_id )
        return false;

    // Check run/walk
    bool is_run = FLAG( move_params, MOVE_PARAM_RUN );
    if( is_run && cr->GetIsNoRun() )
    {
        // Switch to walk
        move_params ^= MOVE_PARAM_RUN;
        is_run = false;
    }
    if( !is_run && cr->GetIsNoWalk() )
        return false;

    // Check
    Map* map = MapMngr.GetMap( map_id );
    if( !map || map_id != cr->GetMapId() || hx >= map->GetWidth() || hy >= map->GetHeight() )
        return false;

    // Check passed
    ushort fx = cr->GetHexX();
    ushort fy = cr->GetHexY();
    uchar  dir = GetNearDir( fx, fy, hx, hy );
    uint   multihex = cr->GetMultihex();

    if( !map->IsMovePassed( hx, hy, dir, multihex ) )
    {
        if( cr->IsPlayer() )
        {
            cr->Send_XY( cr );
            Critter* cr_hex = map->GetHexCritter( hx, hy, false );
            if( cr_hex )
                cr->Send_XY( cr_hex );
        }
        return false;
    }

    // Set last move type
    cr->IsRunning = is_run;

    // Process step
    bool is_dead = cr->IsDead();
    map->UnsetFlagCritter( fx, fy, multihex, is_dead );
    cr->SetHexX( hx );
    cr->SetHexY( hy );
    map->SetFlagCritter( hx, hy, multihex, is_dead );

    // Set dir
    cr->SetDir( dir );

    if( is_run )
    {
        if( cr->GetIsHide() )
            cr->SetIsHide( false );
        cr->SetBreakTimeDelta( cr->GetRunTime() );
    }
    else
    {
        cr->SetBreakTimeDelta( cr->GetWalkTime() );
    }

    cr->SendA_Move( move_params );
    cr->ProcessVisibleCritters();
    cr->ProcessVisibleItems();

    if( cr->GetMapId() == map->GetId() )
    {
        // Triggers
        VerifyTrigger( map, cr, fx, fy, hx, hy, dir );

        // Out trap
        if( map->IsHexTrap( fx, fy ) )
        {
            ItemVec traps;
            map->GetItemsTrap( fx, fy, traps );
            for( auto it = traps.begin(), end = traps.end(); it != end; ++it )
                Script::RaiseInternalEvent( ServerFunctions.ItemWalk, *it, cr, false, dir );
        }

        // In trap
        if( map->IsHexTrap( hx, hy ) )
        {
            ItemVec traps;
            map->GetItemsTrap( hx, hy, traps );
            for( auto it = traps.begin(), end = traps.end(); it != end; ++it )
                Script::RaiseInternalEvent( ServerFunctions.ItemWalk, *it, cr, true, dir );
        }

        // Try transit
        MapMngr.TransitToMapHex( cr, map, cr->GetHexX(), cr->GetHexY(), cr->GetDir(), false );
    }

    return true;
}

bool FOServer::MoveRandom( Critter* cr )
{
    UCharVec dirs( 6 );
    for( int i = 0; i < 6; i++ )
        dirs[ i ] = i;
    std::random_shuffle( dirs.begin(), dirs.end() );

    Map* map = cr->GetMap();
    if( !map )
        return false;

    uint   multihex = cr->GetMultihex();
    ushort maxhx = map->GetWidth();
    ushort maxhy = map->GetHeight();

    for( int i = 0; i < 6; i++ )
    {
        uchar  dir = dirs[ i ];
        ushort hx = cr->GetHexX();
        ushort hy = cr->GetHexY();
        if( MoveHexByDir( hx, hy, dir, maxhx, maxhy ) && map->IsMovePassed( hx, hy, dir, multihex ) )
        {
            if( Act_Move( cr, hx, hy, 0 ) )
            {
                cr->Send_Move( cr, 0 );
                return true;
            }
            return false;
        }
    }
    return false;
}

bool FOServer::RegenerateMap( Map* map )
{
    Script::RaiseInternalEvent( ServerFunctions.MapFinish, map );
    map->DeleteContent();
    map->Generate();
    for( auto item : map->GetItemsNoLock() )
        Script::RaiseInternalEvent( ServerFunctions.ItemInit, item, true );
    Script::RaiseInternalEvent( ServerFunctions.MapInit, map, true );
    return true;
}

void FOServer::VerifyTrigger( Map* map, Critter* cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uchar dir )
{
    if( map->IsHexTrigger( from_hx, from_hy ) || map->IsHexTrigger( to_hx, to_hy ) )
    {
        Item* out_trigger = map->GetProtoMap()->GetMapScenery( from_hx, from_hy, SP_SCEN_TRIGGER );
        Item* in_trigger = map->GetProtoMap()->GetMapScenery( to_hx, to_hy, SP_SCEN_TRIGGER );
        if( !( out_trigger && in_trigger && out_trigger->GetTriggerNum() == in_trigger->GetTriggerNum() ) )
        {
            if( out_trigger && out_trigger->SceneryScriptBindId )
            {
                Script::PrepareContext( out_trigger->SceneryScriptBindId, cr->GetInfo() );
                Script::SetArgEntity( cr );
                Script::SetArgEntity( out_trigger );
                Script::SetArgBool( false );
                Script::SetArgUChar( dir );
                Script::RunPreparedSuspend();
            }
            if( in_trigger && in_trigger->SceneryScriptBindId )
            {
                Script::PrepareContext( in_trigger->SceneryScriptBindId, cr->GetInfo() );
                Script::SetArgEntity( cr );
                Script::SetArgEntity( in_trigger );
                Script::SetArgBool( true );
                Script::SetArgUChar( dir );
                Script::RunPreparedSuspend();
            }
        }
    }
}

void FOServer::Process_Update( Client* cl )
{
    // Net protocol
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;
    bool   outdated = ( proto_ver != FONLINE_VERSION );

    // Begin data encrypting
    uint encrypt_key;
    cl->Connection->Bin >> encrypt_key;
    cl->Connection->Bin.SetEncryptKey( encrypt_key + 521 );
    cl->Connection->Bout.SetEncryptKey( encrypt_key + 3491 );

    // Send update files list
    uint msg_len = sizeof( NETMSG_UPDATE_FILES_LIST ) + sizeof( msg_len ) + sizeof( bool ) +
                   sizeof( uint ) + (uint) UpdateFilesList.size();

    // With global properties
    PUCharVec* global_vars_data;
    UIntVec*   global_vars_data_sizes;
    if( !outdated )
        msg_len += sizeof( ushort ) + Globals->Props.StoreData( false, &global_vars_data, &global_vars_data_sizes );

    BOUT_BEGIN( cl );
    cl->Connection->Bout << NETMSG_UPDATE_FILES_LIST;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << outdated;
    cl->Connection->Bout << (uint) UpdateFilesList.size();
    if( !UpdateFilesList.empty() )
        cl->Connection->Bout.Push( &UpdateFilesList[ 0 ], (uint) UpdateFilesList.size() );
    if( !outdated )
        NET_WRITE_PROPERTIES( cl->Connection->Bout, global_vars_data, global_vars_data_sizes );
    BOUT_END( cl );
}

void FOServer::Process_UpdateFile( Client* cl )
{
    uint file_index;
    cl->Connection->Bin >> file_index;

    if( file_index >= (uint) UpdateFiles.size() )
    {
        WriteLog( "Wrong file index {}, client ip '{}'.\n", file_index, cl->GetIpStr() );
        cl->Disconnect();
        return;
    }

    cl->UpdateFileIndex = file_index;
    cl->UpdateFilePortion = 0;
    Process_UpdateFileData( cl );
}

void FOServer::Process_UpdateFileData( Client* cl )
{
    if( cl->UpdateFileIndex == -1 )
    {
        WriteLog( "Wrong update call, client ip '{}'.\n", cl->GetIpStr() );
        cl->Disconnect();
        return;
    }

    UpdateFile& update_file = UpdateFiles[ cl->UpdateFileIndex ];
    uint        offset = cl->UpdateFilePortion * FILE_UPDATE_PORTION;

    if( offset + FILE_UPDATE_PORTION < update_file.Size )
        cl->UpdateFilePortion++;
    else
        cl->UpdateFileIndex = -1;

    if( cl->IsSendDisabled() || cl->IsOffline() )
        return;

    uchar data[ FILE_UPDATE_PORTION ];
    uint  remaining_size = update_file.Size - offset;
    if( remaining_size >= sizeof( data ) )
    {
        memcpy( data, &update_file.Data[ offset ], sizeof( data ) );
    }
    else
    {
        memcpy( data, &update_file.Data[ offset ], remaining_size );
        memzero( &data[ remaining_size ], sizeof( data ) - remaining_size );
    }

    BOUT_BEGIN( cl );
    cl->Connection->Bout << NETMSG_UPDATE_FILE_DATA;
    cl->Connection->Bout.Push( data, sizeof( data ) );
    BOUT_END( cl );
}

void FOServer::Process_CreateClient( Client* cl )
{
    // Prevent brute force by ip
    if( CheckBruteForceIp( cl->GetIp() ) )
    {
        cl->Disconnect();
        return;
    }

    // Read message
    uint msg_len;
    cl->Connection->Bin >> msg_len;

    // Protocol version
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;

    // Begin data encrypting
    cl->Connection->Bin.SetEncryptKey( 1234567890 );
    cl->Connection->Bout.SetEncryptKey( 1234567890 );

    // Check protocol
    if( proto_ver != FONLINE_VERSION )
    {
        cl->Send_CustomMessage( NETMSG_WRONG_NET_PROTO );
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK( BannedLocker );

        uint          ip = cl->GetIp();
        ClientBanned* ban = GetBanByIp( ip );
        if( ban && !Singleplayer )
        {
            cl->Send_TextMsg( cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME );
            // cl->Send_TextMsgLex(cl,STR_NET_BAN_REASON,SAY_NETMSG,TEXTMSG_GAME,ban->GetBanLexems());
            cl->Send_TextMsgLex( cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, Str::FormatBuf( "$time%u", GetBanTime( *ban ) ) );
            cl->Disconnect();
            return;
        }
    }

    // Name
    char name[ UTF8_BUF_SIZE( MAX_NAME ) ];
    cl->Connection->Bin.Pop( name, sizeof( name ) );
    name[ sizeof( name ) - 1 ] = 0;
    cl->Name = name;

    // Password hash
    char password[ UTF8_BUF_SIZE( MAX_NAME ) ];
    cl->Connection->Bin.Pop( password, sizeof( password ) );
    password[ sizeof( password ) - 1 ] = 0;
    cl->SetPassword( password );

    // Check net
    CHECK_IN_BUFF_ERROR_EXT( cl, cl->Send_TextMsg( cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME ), return );

    // Check data
    if( !Str::IsValidUTF8( cl->Name.c_str() ) || Str::Substring( cl->Name.c_str(), "*" ) )
    {
        cl->Send_TextMsg( cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    uint name_len_utf8 = Str::LengthUTF8( cl->Name.c_str() );
    if( name_len_utf8 < MIN_NAME || name_len_utf8 < GameOpt.MinNameLength ||
        name_len_utf8 > MAX_NAME || name_len_utf8 > GameOpt.MaxNameLength )
    {
        cl->Send_TextMsg( cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Check for exist
    uint id = MAKE_CLIENT_ID( name );
    RUNTIME_ASSERT( IS_CLIENT_ID( id ) );
    if( !Singleplayer )
    {
        SaveClientsLocker.Lock();
        bool exist = ( GetClientData( id ) != nullptr );
        SaveClientsLocker.Unlock();

        if( !exist )
        {
            // Avoid created files rewriting
            char fname[ MAX_FOPATH ];
            FileManager::GetWritePath( "Save/Clients/", fname );
            Str::Append( fname, ".foclient" );
            exist = FileManager::IsFileExists( fname );
        }

        if( exist )
        {
            cl->Send_TextMsg( cl, STR_NET_ACCOUNT_ALREADY, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }
    }

    // Check brute force registration
    #ifndef DEV_VERSION
    if( GameOpt.RegistrationTimeout && !Singleplayer )
    {
        SCOPE_LOCK( RegIpLocker );

        uint ip = cl->GetIp();
        uint reg_tick = GameOpt.RegistrationTimeout * 1000;
        auto it = RegIp.find( ip );
        if( it != RegIp.end() )
        {
            uint& last_reg = it->second;
            uint  tick = Timer::FastTick();
            if( tick - last_reg < reg_tick )
            {
                cl->Send_TextMsg( cl, STR_NET_REGISTRATION_IP_WAIT, SAY_NETMSG, TEXTMSG_GAME );
                cl->Send_TextMsgLex( cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, Str::FormatBuf( "$time%u", ( reg_tick - ( tick - last_reg ) ) / 60000 + 1 ) );
                cl->Disconnect();
                return;
            }
            last_reg = tick;
        }
        else
        {
            RegIp.insert( PAIR( ip, Timer::FastTick() ) );
        }
    }
    #endif

    uint   disallow_msg_num = 0, disallow_str_num = 0;
    string lexems;
    bool   allow = Script::RaiseInternalEvent( ServerFunctions.PlayerRegistration, cl->GetIp(), &cl->Name, &disallow_msg_num, &disallow_str_num, &lexems );
    if( !allow )
    {
        if( disallow_msg_num < TEXTMSG_COUNT && disallow_str_num )
            cl->Send_TextMsgLex( cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, lexems.c_str() );
        else
            cl->Send_TextMsg( cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Register
    cl->SetId( id );
    cl->RefreshName();
    cl->SetWorldX( GM_MAXX / 2 );
    cl->SetWorldY( GM_MAXY / 2 );
    cl->SetCond( COND_LIFE );

    CScriptArray* arr_reg_ip = Script::CreateArray( "uint[]" );
    uint          reg_ip = cl->GetIp();
    arr_reg_ip->InsertLast( &reg_ip );
    cl->SetConnectionIp( arr_reg_ip );
    SAFEREL( arr_reg_ip );
    CScriptArray* arr_reg_port = Script::CreateArray( "uint16[]" );
    ushort        reg_port = cl->GetPort();
    arr_reg_port->InsertLast( &reg_port );
    cl->SetConnectionPort( arr_reg_port );
    SAFEREL( arr_reg_port );

    // Assign base access
    cl->Access = ACCESS_DEFAULT;

    // First save
    if( !SaveClient( cl ) )
    {
        WriteLog( "First save fail.\n" );
        cl->Send_TextMsg( cl, STR_NET_BD_ERROR, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Clear brute force ip and name, because client enter to game immediately after registration
    ClearBruteForceEntire( cl->GetIp(), cl->Name.c_str() );

    // Load world
    if( Singleplayer )
    {
        if( !NewWorld() )
        {
            WriteLog( "Generate new world fail.\n" );
            cl->Send_TextMsg( cl, STR_SP_NEW_GAME_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }
    }

    // Notify
    if( !Singleplayer )
        cl->Send_TextMsg( cl, STR_NET_REG_SUCCESS, SAY_NETMSG, TEXTMSG_GAME );
    else
        cl->Send_TextMsg( cl, STR_SP_NEW_GAME_SUCCESS, SAY_NETMSG, TEXTMSG_GAME );

    BOUT_BEGIN( cl );
    cl->Connection->Bout << (uint) NETMSG_REGISTER_SUCCESS;
    BOUT_END( cl );

    cl->Disconnect();

    cl->AddRef();
    EntityMngr.RegisterEntity( cl );
    bool can = MapMngr.CanAddCrToMap( cl, nullptr, 0, 0, 0 );
    RUNTIME_ASSERT( can );
    MapMngr.AddCrToMap( cl, nullptr, 0, 0, 0, 0 );

    if( !Script::RaiseInternalEvent( ServerFunctions.CritterInit, cl, true ) )
    {
        // Todo: remove from game
    }
    SaveClient( cl );

    if( !Singleplayer )
    {
        ClientData* data = new ClientData();
        memzero( data, sizeof( ClientData ) );
        Str::Copy( data->ClientName, cl->Name.c_str() );
        memcpy( data->ClientPassHash, password, Str::Length( password ) );

        SCOPE_LOCK( ClientsDataLocker );
        ClientsData.insert( PAIR( cl->GetId(), data ) );
    }
    else
    {
        SingleplayerSave.Valid = true;
        SingleplayerSave.Name = cl->Name;
        SingleplayerSave.CrProps = &cl->Props;
        SingleplayerSave.PicData.clear();
    }
}

void FOServer::Process_LogIn( Client*& cl )
{
    // Prevent brute force by ip
    if( CheckBruteForceIp( cl->GetIp() ) )
    {
        cl->Disconnect();
        return;
    }

    // Net protocol
    ushort proto_ver = 0;
    cl->Connection->Bin >> proto_ver;

    // UIDs
    uint uidxor, uidor, uidcalc;
    uint uid[ 5 ];
    cl->Connection->Bin >> uid[ 4 ];

    // Begin data encrypting
    cl->Connection->Bin.SetEncryptKey( uid[ 4 ] + 12345 );
    cl->Connection->Bout.SetEncryptKey( uid[ 4 ] + 12345 );

    // Check protocol
    if( proto_ver != FONLINE_VERSION )
    {
        cl->Send_CustomMessage( NETMSG_WRONG_NET_PROTO );
        cl->Disconnect();
        return;
    }

    // Login, password hash
    char name[ UTF8_BUF_SIZE( MAX_NAME ) ];
    cl->Connection->Bin.Pop( name, sizeof( name ) );
    name[ sizeof( name ) - 1 ] = 0;
    cl->Name = name;
    cl->Connection->Bin >> uid[ 1 ];
    char password[ UTF8_BUF_SIZE( MAX_NAME ) ];
    cl->Connection->Bin.Pop( password, sizeof( password ) );

    if( Singleplayer )
    {
        cl->Name = "";
        memzero( password, sizeof( password ) );
    }

    // Prevent brute force by name
    if( CheckBruteForceName( cl->Name.c_str() ) )
    {
        cl->Disconnect();
        return;
    }

    // Bin hashes
    uint msg_language;
    uint textmsg_hash[ TEXTMSG_COUNT ];
    uint item_hash[ ITEM_MAX_TYPES ];

    cl->Connection->Bin >> msg_language;
    for( int i = 0; i < TEXTMSG_COUNT; i++ )
        cl->Connection->Bin >> textmsg_hash[ i ];
    cl->Connection->Bin >> uidxor;
    cl->Connection->Bin >> uid[ 3 ];
    cl->Connection->Bin >> uid[ 2 ];
    cl->Connection->Bin >> uidor;
    for( int i = 0; i < ITEM_MAX_TYPES; i++ )
        cl->Connection->Bin >> item_hash[ i ];
    cl->Connection->Bin >> uidcalc;
    cl->Connection->Bin >> uid[ 0 ];
    char dummy[ 100 ];
    cl->Connection->Bin.Pop( dummy, 100 );
    CHECK_IN_BUFF_ERROR_EXT( cl, cl->Send_TextMsg( cl, STR_NET_DATATRANS_ERR, SAY_NETMSG, TEXTMSG_GAME ), return );

    // Language
    auto it_l = std::find( LangPacks.begin(), LangPacks.end(), msg_language );
    if( it_l != LangPacks.end() )
        cl->LanguageMsg = msg_language;
    else
        cl->LanguageMsg = ( *LangPacks.begin() ).Name;

    // If only cache checking than disconnect
    if( !Singleplayer && !name[ 0 ] )
    {
        cl->Disconnect();
        return;
    }

    // Singleplayer
    if( Singleplayer && !SingleplayerSave.Valid )
    {
        WriteLog( "World not contain singleplayer data.\n" );
        cl->Send_TextMsg( cl, STR_SP_SAVE_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Check for ban by ip
    {
        SCOPE_LOCK( BannedLocker );

        uint          ip = cl->GetIp();
        ClientBanned* ban = GetBanByIp( ip );
        if( ban && !Singleplayer )
        {
            cl->Send_TextMsg( cl, STR_NET_BANNED_IP, SAY_NETMSG, TEXTMSG_GAME );
            if( Str::CompareCaseUTF8( ban->ClientName, cl->Name.c_str() ) )
                cl->Send_TextMsgLex( cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban->GetBanLexems() );
            cl->Send_TextMsgLex( cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, Str::FormatBuf( "$time%u", GetBanTime( *ban ) ) );
            cl->Disconnect();
            return;
        }
    }

    // Check login/password
    if( !Singleplayer )
    {
        uint name_len_utf8 = Str::LengthUTF8( cl->Name.c_str() );
        if( name_len_utf8 < MIN_NAME || name_len_utf8 < GameOpt.MinNameLength || name_len_utf8 > MAX_NAME || name_len_utf8 > GameOpt.MaxNameLength )
        {
            cl->Send_TextMsg( cl, STR_NET_WRONG_LOGIN, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        if( !Str::IsValidUTF8( cl->Name.c_str() ) || Str::Substring( cl->Name.c_str(), "*" ) )
        {
            cl->Send_TextMsg( cl, STR_NET_WRONG_DATA, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }
    }

    // Get client account data
    uint       id = MAKE_CLIENT_ID( cl->Name.c_str() );
    ClientData data;
    if( !Singleplayer )
    {
        SCOPE_LOCK( ClientsDataLocker );

        ClientData* data_ = GetClientData( id );
        if( !data_ || !Str::Compare( password, data_->ClientPassHash ) )
        {
            cl->Send_TextMsg( cl, STR_NET_LOGINPASS_WRONG, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        data = *data_;
    }
    else
    {
        Str::Copy( data.ClientName, SingleplayerSave.Name.c_str() );
        id = MAKE_CLIENT_ID( data.ClientName );
    }

    // Check for ban by name
    {
        SCOPE_LOCK( BannedLocker );

        ClientBanned* ban = GetBanByName( data.ClientName );
        if( ban && !Singleplayer )
        {
            cl->Send_TextMsg( cl, STR_NET_BANNED, SAY_NETMSG, TEXTMSG_GAME );
            cl->Send_TextMsgLex( cl, STR_NET_BAN_REASON, SAY_NETMSG, TEXTMSG_GAME, ban->GetBanLexems() );
            cl->Send_TextMsgLex( cl, STR_NET_TIME_LEFT, SAY_NETMSG, TEXTMSG_GAME, Str::FormatBuf( "$time%u", GetBanTime( *ban ) ) );
            cl->Disconnect();
            return;
        }
    }

    // Request script
    uint   disallow_msg_num = 0, disallow_str_num = 0;
    string name_str = data.ClientName;
    string lexems;
    bool   allow = Script::RaiseInternalEvent( ServerFunctions.PlayerLogin, cl->GetIp(), &name_str, id, &disallow_msg_num, &disallow_str_num, &lexems );
    if( !allow )
    {
        if( disallow_msg_num < TEXTMSG_COUNT && disallow_str_num )
            cl->Send_TextMsgLex( cl, disallow_str_num, SAY_NETMSG, disallow_msg_num, lexems.c_str() );
        else
            cl->Send_TextMsg( cl, STR_NET_LOGIN_SCRIPT_FAIL, SAY_NETMSG, TEXTMSG_GAME );
        cl->Disconnect();
        return;
    }

    // Copy data
    cl->Name = data.ClientName;
    cl->RefreshName();

    // Check UIDS
    #ifndef DEV_VERSION
    if( GameOpt.AccountPlayTime && !Singleplayer )
    {
        int uid_zero = 0;
        for( int i = 0; i < 5; i++ )
            if( !uid[ i ] )
                uid_zero++;
        if( uid_zero > 2 )
        {
            WriteLog( "Received more than two zero UIDs, client '{}'.\n", cl->Name );
            cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        if( ( uid[ 0 ] && ( !FLAG( uid[ 0 ], 0x00800000 ) || FLAG( uid[ 0 ], 0x00000400 ) ) ) ||
            ( uid[ 1 ] && ( !FLAG( uid[ 1 ], 0x04000000 ) || FLAG( uid[ 1 ], 0x00200000 ) ) ) ||
            ( uid[ 2 ] && ( !FLAG( uid[ 2 ], 0x00000020 ) || FLAG( uid[ 2 ], 0x00000800 ) ) ) ||
            ( uid[ 3 ] && ( !FLAG( uid[ 3 ], 0x80000000 ) || FLAG( uid[ 3 ], 0x40000000 ) ) ) ||
            ( uid[ 4 ] && ( !FLAG( uid[ 4 ], 0x00000800 ) || FLAG( uid[ 4 ], 0x00004000 ) ) ) )
        {
            WriteLog( "Invalid UIDs, client '{}'.\n", cl->Name );
            cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        uint uidxor_ = 0xF145668A, uidor_ = 0, uidcalc_ = 0x45012345;
        for( int i = 0; i < 5; i++ )
        {
            uidxor_ ^= uid[ i ];
            uidor_ |= uid[ i ];
            uidcalc_ += uid[ i ];
        }

        if( uidxor != uidxor_ || uidor != uidor_ || uidcalc != uidcalc_ )
        {
            WriteLog( "Invalid UIDs hash, client '{}'.\n", cl->Name );
            cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        SCOPE_LOCK( ClientsDataLocker );

        uint tick = Timer::FastTick();
        for( auto it = ClientsData.begin(), end = ClientsData.end(); it != end; ++it )
        {
            ClientData* cd = it->second;
            if( it->first == id )
                continue;

            int matches = 0;
            if( !uid[ 0 ] || uid[ 0 ] == cd->UID[ 0 ] )
                matches++;
            if( !uid[ 1 ] || uid[ 1 ] == cd->UID[ 1 ] )
                matches++;
            if( !uid[ 2 ] || uid[ 2 ] == cd->UID[ 2 ] )
                matches++;
            if( !uid[ 3 ] || uid[ 3 ] == cd->UID[ 3 ] )
                matches++;
            if( !uid[ 4 ] || uid[ 4 ] == cd->UID[ 4 ] )
                matches++;

            if( matches >= 5 )
            {
                if( !cd->UIDEndTick || tick >= cd->UIDEndTick )
                {
                    for( int i = 0; i < 5; i++ )
                        cd->UID[ i ] = 0;
                    cd->UIDEndTick = 0;
                }
                else
                {
                    WriteLog( "UID already used by critter '{}', client '{}'.\n", cd->ClientName, cl->Name );
                    cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
                    cl->Disconnect();
                    return;
                }
            }
        }

        for( int i = 0; i < 5; i++ )
        {
            if( data.UID[ i ] != uid[ i ] )
            {
                if( !data.UIDEndTick || tick >= data.UIDEndTick )
                {
                    // Set new uids on this account and start play timeout
                    ClientData* data_ = GetClientData( id );
                    for( int j = 0; j < 5; j++ )
                        data_->UID[ j ] = uid[ j ];
                    data_->UIDEndTick = tick + GameOpt.AccountPlayTime * 1000;
                }
                else
                {
                    WriteLog( "Different UID {}, client '{}'.\n", i, cl->Name );
                    cl->Send_TextMsg( cl, STR_NET_UID_FAIL, SAY_NETMSG, TEXTMSG_GAME );
                    cl->Disconnect();
                    return;
                }
                break;
            }
        }
    }
    #endif

    // Find in players in game
    Client* cl_old = CrMngr.GetPlayer( id );
    RUNTIME_ASSERT( cl != cl_old );

    // Avatar in game
    if( cl_old )
    {
        ConnectedClientsLocker.Lock();

        // Disconnect current connection
        if( !cl_old->Connection->IsDisconnected )
            cl_old->Disconnect();

        // Swap data
        std::swap( cl_old->Connection, cl->Connection );
        cl_old->GameState = STATE_CONNECTED;
        cl->GameState = STATE_NONE;
        cl_old->LastActivityTime = 0;
        UNSETFLAG( cl_old->Flags, FCRIT_DISCONNECT );
        SETFLAG( cl->Flags, FCRIT_DISCONNECT );
        cl->IsDestroyed = true;
        cl_old->IsDestroyed = false;
        Script::RemoveEventsEntity( cl );

        // Change in list
        cl_old->AddRef();
        auto it = std::find( ConnectedClients.begin(), ConnectedClients.end(), cl );
        RUNTIME_ASSERT( it != ConnectedClients.end() );
        *it = cl_old;
        cl->Release();
        cl = cl_old;

        ConnectedClientsLocker.Unlock();

        // Erase from save
        EraseSaveClient( cl->GetId() );

        cl->SendA_Action( ACTION_CONNECT, 0, nullptr );
    }
    // Avatar not in game
    else
    {
        cl->SetId( id );

        // Singleplayer data
        if( Singleplayer )
        {
            cl->Name = SingleplayerSave.Name;
            cl->Props = *SingleplayerSave.CrProps;
        }

        // Find in saved array
        SaveClientsLocker.Lock();
        Client* cl_saved = nullptr;
        auto    it = std::find_if( SaveClients.begin(), SaveClients.end(), [ id ] ( Client * cl )
                                   {
                                       return cl->GetId() == id;
                                   } );
        if( it != SaveClients.end() )
            cl_saved = *it;
        SaveClientsLocker.Unlock();

        if( cl_saved )
        {
            cl->Props = cl_saved->Props;
            EraseSaveClient( id );
        }
        else
        {
            if( !LoadClient( cl ) )
            {
                WriteLog( "Error load from data base, client '{}'.\n", cl->GetInfo() );
                cl->Send_TextMsg( cl, STR_NET_BD_ERROR, SAY_NETMSG, TEXTMSG_GAME );
                cl->Disconnect();
                return;
            }
        }

        // Add to collection
        cl->AddRef();
        EntityMngr.RegisterEntity( cl );

        // Disable network data sending, because we resend all data later
        cl->DisableSend++;

        // Find items
        ItemMngr.SetCritterItems( cl );

        // Initially add on global map
        cl->SetMapId( 0 );
        uint ref_gm_leader_id = cl->GetRefGlobalMapLeaderId();
        uint ref_gm_trip_id = cl->GetRefGlobalMapTripId();
        bool can = MapMngr.CanAddCrToMap( cl, nullptr, 0, 0, 0 );
        RUNTIME_ASSERT( can );
        MapMngr.AddCrToMap( cl, nullptr, 0, 0, 0, 0 );
        cl->SetRefGlobalMapLeaderId( ref_gm_leader_id );
        cl->SetRefGlobalMapLeaderId( ref_gm_trip_id );

        // Initial scripts
        if( !Script::RaiseInternalEvent( ServerFunctions.CritterInit, cl, false ) )
        {
            // Todo: remove from game
        }
        cl->SetScript( nullptr, false );

        // Restore data sending
        cl->DisableSend--;
    }

    for( int i = 0; i < 5; i++ )
        cl->UID[ i ] = uid[ i ];

    // Connection info
    uint          ip = cl->GetIp();
    ushort        port = cl->GetPort();
    CScriptArray* conn_ip = cl->GetConnectionIp();
    CScriptArray* conn_port = cl->GetConnectionPort();
    RUNTIME_ASSERT( conn_ip->GetSize() == conn_port->GetSize() );
    bool          ip_found = false;
    for( uint i = 0, j = conn_ip->GetSize(); i < j; i++ )
    {
        if( *(uint*) conn_ip->At( i ) == ip )
        {
            if( i < j - 1 )
            {
                conn_ip->RemoveAt( i );
                conn_ip->InsertLast( &ip );
                cl->SetConnectionIp( conn_ip );
                conn_port->RemoveAt( i );
                conn_port->InsertLast( &port );
                cl->SetConnectionPort( conn_port );
            }
            else if( *(ushort*) conn_port->At( j - 1 ) != port )
            {
                conn_port->SetValue( i, &port );
                cl->SetConnectionPort( conn_port );
            }
            ip_found = true;
            break;
        }
    }
    if( !ip_found )
    {
        conn_ip->InsertLast( &ip );
        cl->SetConnectionIp( conn_ip );
        conn_port->InsertLast( &port );
        cl->SetConnectionPort( conn_port );
    }
    SAFEREL( conn_ip );
    SAFEREL( conn_port );

    // Login ok
    uint       msg_len = sizeof( uint ) + sizeof( msg_len ) + sizeof( uint ) * 2;
    uint       bin_seed = Random( 100000, 2000000000 );
    uint       bout_seed = Random( 100000, 2000000000 );
    PUCharVec* global_vars_data;
    UIntVec*   global_vars_data_sizes;
    uint       whole_data_size = Globals->Props.StoreData( false, &global_vars_data, &global_vars_data_sizes );
    msg_len += sizeof( ushort ) + whole_data_size;
    BOUT_BEGIN( cl );
    cl->Connection->Bout << NETMSG_LOGIN_SUCCESS;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << bin_seed;
    cl->Connection->Bout << bout_seed;
    NET_WRITE_PROPERTIES( cl->Connection->Bout, global_vars_data, global_vars_data_sizes );
    BOUT_END( cl );

    cl->Connection->Bin.SetEncryptKey( bin_seed );
    cl->Connection->Bout.SetEncryptKey( bout_seed );

    cl->Send_LoadMap( nullptr );
}

void FOServer::Process_SingleplayerSaveLoad( Client* cl )
{
    if( !Singleplayer )
    {
        cl->Disconnect();
        return;
    }

    uint     msg_len;
    bool     save;
    ushort   fname_len;
    char     fname[ MAX_FOTEXT ];
    UCharVec pic_data;
    cl->Connection->Bin >> msg_len;
    cl->Connection->Bin >> save;
    cl->Connection->Bin >> fname_len;
    cl->Connection->Bin.Pop( fname, fname_len );
    fname[ fname_len ] = 0;
    if( save )
    {
        uint pic_data_len;
        cl->Connection->Bin >> pic_data_len;
        pic_data.resize( pic_data_len );
        if( pic_data_len )
            cl->Connection->Bin.Pop( &pic_data[ 0 ], pic_data_len );
    }

    CHECK_IN_BUFF_ERROR( cl );

    if( save )
    {
        SingleplayerSave.PicData = pic_data;

        SaveWorld( fname );
        cl->Send_TextMsg( cl, STR_SP_SAVE_SUCCESS, SAY_NETMSG, TEXTMSG_GAME );
    }
    else
    {
        if( !LoadWorld( fname ) )
        {
            WriteLog( "Unable load world from file '{}'.\n", fname );
            cl->Send_TextMsg( cl, STR_SP_LOAD_FAIL, SAY_NETMSG, TEXTMSG_GAME );
            cl->Disconnect();
            return;
        }

        cl->Send_TextMsg( cl, STR_SP_LOAD_SUCCESS, SAY_NETMSG, TEXTMSG_GAME );

        BOUT_BEGIN( cl );
        cl->Connection->Bout << (uint) NETMSG_REGISTER_SUCCESS;
        BOUT_END( cl );
        cl->Disconnect();
    }
}

void FOServer::Process_ParseToGame( Client* cl )
{
    if( !cl->GetId() || !CrMngr.GetPlayer( cl->GetId() ) )
        return;
    cl->SetBreakTime( GameOpt.Breaktime );

    #ifdef DEV_VERSION
    cl->Access = ACCESS_ADMIN;
    #endif

    cl->GameState = STATE_PLAYING;
    cl->PingOk( PING_CLIENT_LIFE_TIME * 5 );

    if( cl->ViewMapId )
    {
        Map* map = MapMngr.GetMap( cl->ViewMapId );
        cl->ViewMapId = 0;
        if( map )
        {
            cl->ViewMap( map, cl->ViewMapLook, cl->ViewMapHx, cl->ViewMapHy, cl->ViewMapDir );
            cl->Send_ViewMap();
            return;
        }
    }

    // Parse
    Map* map = MapMngr.GetMap( cl->GetMapId() );
    cl->Send_GameInfo( map );

    // Parse to global
    if( !cl->GetMapId() )
    {
        RUNTIME_ASSERT( cl->GlobalMapGroup );

        cl->Send_GlobalInfo( GM_INFO_ALL );
        for( auto it = cl->GlobalMapGroup->begin(), end = cl->GlobalMapGroup->end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr != cl )
                cr->Send_CustomCommand( cl, OTHER_FLAGS, cl->Flags );
        }
        cl->Send_AllAutomapsInfo();
        if( cl->Talk.TalkType != TALK_NONE )
        {
            cl->ProcessTalk( true );
            cl->Send_Talk();
        }
    }
    else
    {
        if( !map )
        {
            WriteLog( "Map not found, client '{}'.\n", cl->GetInfo() );
            cl->Disconnect();
            return;
        }

        // Parse to local
        SETFLAG( cl->Flags, FCRIT_CHOSEN );
        cl->Send_AddCritter( cl );
        UNSETFLAG( cl->Flags, FCRIT_CHOSEN );

        // Send all data
        cl->Send_AddAllItems();
        cl->Send_AllAutomapsInfo();

        // Send current critters
        CrVec critters = cl->VisCrSelf;
        for( auto it = critters.begin(), end = critters.end(); it != end; ++it )
            cl->Send_AddCritter( *it );

        // Send current items on map
        cl->VisItemLocker.Lock();
        UIntSet items = cl->VisItem;
        cl->VisItemLocker.Unlock();
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
        {
            Item* item = ItemMngr.GetItem( *it );
            if( item )
                cl->Send_AddItemOnMap( item );
        }

        // Check active talk
        if( cl->Talk.TalkType != TALK_NONE )
        {
            cl->ProcessTalk( true );
            cl->Send_Talk();
        }
    }

    // Notify about end of parsing
    cl->Send_CustomMessage( NETMSG_END_PARSE_TO_GAME );
}

void FOServer::Process_GiveMap( Client* cl )
{
    bool automap;
    hash map_pid;
    uint loc_id;
    hash hash_tiles;
    hash hash_scen;

    cl->Connection->Bin >> automap;
    cl->Connection->Bin >> map_pid;
    cl->Connection->Bin >> loc_id;
    cl->Connection->Bin >> hash_tiles;
    cl->Connection->Bin >> hash_scen;
    CHECK_IN_BUFF_ERROR( cl );

    if( !Singleplayer )
    {
        uint tick = Timer::FastTick();
        if( tick - cl->LastSendedMapTick < GameOpt.Breaktime * 3 )
            cl->SetBreakTime( GameOpt.Breaktime * 3 );
        else
            cl->SetBreakTime( GameOpt.Breaktime );
        cl->LastSendedMapTick = tick;
    }

    ProtoMap* pmap = ProtoMngr.GetProtoMap( map_pid );
    if( !pmap )
    {
        WriteLog( "Map prototype not found, client '{}'.\n", cl->GetInfo() );
        cl->Disconnect();
        return;
    }

    if( automap )
    {
        if( !cl->CheckKnownLocById( loc_id ) )
        {
            WriteLog( "Request for loading unknown automap, client '{}'.\n", cl->GetInfo() );
            return;
        }

        Location* loc = MapMngr.GetLocation( loc_id );
        if( !loc )
        {
            WriteLog( "Request for loading incorrect automap, client '{}'.\n", cl->GetInfo() );
            return;
        }

        bool          found = false;
        CScriptArray* automaps = ( loc->IsNonEmptyAutomaps() ? loc->GetAutomaps() : nullptr );
        if( automaps )
        {
            for( uint i = 0, j = automaps->GetSize(); i < j && !found; i++ )
                if( *(hash*) automaps->At( i ) == map_pid )
                    found = true;
        }
        if( !found )
        {
            WriteLog( "Request for loading incorrect automap, client '{}'.\n", cl->GetInfo() );
            return;
        }
    }
    else
    {
        Map* map = cl->GetMap();
        if( !map || ( map_pid != map->GetProtoId() && cl->ViewMapPid != map_pid ) )
        {
            WriteLog( "Request for loading incorrect map, client '{}'.\n", cl->GetInfo() );
            return;
        }
    }

    Send_MapData( cl, pmap, pmap->HashTiles != hash_tiles, pmap->HashScen != hash_scen );

    if( !automap )
    {
        Map* map = nullptr;
        if( cl->ViewMapId )
            map = MapMngr.GetMap( cl->ViewMapId );
        cl->Send_LoadMap( map );
    }
}

void FOServer::Send_MapData( Client* cl, ProtoMap* pmap, bool send_tiles, bool send_scenery )
{
    uint   msg = NETMSG_MAP;
    hash   map_pid = pmap->ProtoId;
    ushort maxhx = pmap->GetWidth();
    ushort maxhy = pmap->GetHeight();
    uint   msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( map_pid ) + sizeof( maxhx ) + sizeof( maxhy ) + sizeof( bool ) * 2;

    if( send_tiles )
        msg_len += sizeof( uint ) + (uint) pmap->Tiles.size() * sizeof( ProtoMap::Tile );
    if( send_scenery )
        msg_len += sizeof( uint ) + (uint) pmap->SceneryData.size();

    // Header
    BOUT_BEGIN( cl );
    cl->Connection->Bout << msg;
    cl->Connection->Bout << msg_len;
    cl->Connection->Bout << map_pid;
    cl->Connection->Bout << maxhx;
    cl->Connection->Bout << maxhy;
    cl->Connection->Bout << send_tiles;
    cl->Connection->Bout << send_scenery;
    if( send_tiles )
    {
        cl->Connection->Bout << (uint) ( pmap->Tiles.size() * sizeof( ProtoMap::Tile ) );
        if( pmap->Tiles.size() )
            cl->Connection->Bout.Push( &pmap->Tiles[ 0 ], (uint) pmap->Tiles.size() * sizeof( ProtoMap::Tile ) );
    }
    if( send_scenery )
    {
        cl->Connection->Bout << (uint) pmap->SceneryData.size();
        if( pmap->SceneryData.size() )
            cl->Connection->Bout.Push( &pmap->SceneryData[ 0 ], (uint) pmap->SceneryData.size() );
    }
    BOUT_END( cl );
}

void FOServer::Process_Move( Client* cl )
{
    uint   move_params;
    ushort hx;
    ushort hy;

    cl->Connection->Bin >> move_params;
    cl->Connection->Bin >> hx;
    cl->Connection->Bin >> hy;
    CHECK_IN_BUFF_ERROR( cl );

    if( !cl->GetMapId() )
        return;

    // The player informs that has stopped
    if( FLAG( move_params, MOVE_PARAM_STEP_DISALLOW ) )
    {
        // cl->Send_XY(cl);
        cl->SendA_XY();
        return;
    }

    // Check running availability
    bool is_run = FLAG( move_params, MOVE_PARAM_RUN );
    if( is_run )
    {
        if( cl->GetIsNoRun() ||
            ( GameOpt.RunOnCombat ? false : IS_TIMEOUT( cl->GetTimeoutBattle() ) ) ||
            ( GameOpt.RunOnTransfer ? false : IS_TIMEOUT( cl->GetTimeoutTransfer() ) ) )
        {
            // Switch to walk
            move_params ^= MOVE_PARAM_RUN;
            is_run = false;
        }
    }

    // Check walking availability
    if( !is_run )
    {
        if( cl->GetIsNoWalk() )
        {
            cl->Send_XY( cl );
            return;
        }
    }

    // Check dist
    if( !CheckDist( cl->GetHexX(), cl->GetHexY(), hx, hy, 1 ) )
    {
        cl->Send_XY( cl );
        return;
    }

    // Try move
    Act_Move( cl, hx, hy, move_params );
}

void FOServer::Process_ContainerItem( Client* cl )
{
    uchar transfer_type;
    uint  cont_id;
    uint  item_id;
    uint  item_count;
    uchar take_flags;

    cl->Bin >> transfer_type;
    cl->Bin >> cont_id;
    cl->Bin >> item_id;
    cl->Bin >> item_count;
    cl->Bin >> take_flags;
    CHECK_IN_BUFF_ERROR( cl );

    cl->SetBreakTime( GameOpt.Breaktime );

    if( cl->AccessContainerId != cont_id )
    {
        WriteLogF( _FUNC_, " - Try work with not accessed container, client '%s'.\n", cl->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }
    cl->ItemTransferCount = item_count;

    int ap_cost = cl->GetApCostMoveItemContainer();
    if( cl->GetCurrentAp() / AP_DIVIDER < ap_cost && !Singleplayer )
    {
        WriteLogF( _FUNC_, " - Not enough AP, client '%s'.\n", cl->GetInfo() );
        cl->Send_ContainerInfo();
        return;
    }
    cl->SubAp( ap_cost );

    if( !cl->GetMapId() && ( transfer_type != TRANSFER_CRIT_STEAL && transfer_type != TRANSFER_FAR_CONT && transfer_type != TRANSFER_FAR_CRIT &&
                             transfer_type != TRANSFER_CRIT_BARTER && transfer_type != TRANSFER_SELF_CONT ) )
        return;

/************************************************************************/
/* Item container                                                       */
/************************************************************************/
    if( transfer_type == TRANSFER_HEX_CONT_UP || transfer_type == TRANSFER_HEX_CONT_DOWN || transfer_type == TRANSFER_SELF_CONT || transfer_type == TRANSFER_FAR_CONT )
    {
        Item* cont;

        // Check and get hex cont
        if( transfer_type == TRANSFER_HEX_CONT_UP || transfer_type == TRANSFER_HEX_CONT_DOWN )
        {
            // Get item
            cont = ItemMngr.GetItem( cont_id, true );
            if( !cont || cont->GetAccessory() != ITEM_ACCESSORY_HEX || !cont->IsContainer() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - TRANSFER_HEX_CONT error.\n" );
                return;
            }

            // Check map
            if( cont->GetMapId() != cl->GetMapId() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - Attempt to take a subject from the container on other map.\n" );
                return;
            }

            // Check dist
            if( !CheckDist( cl->GetHexX(), cl->GetHexY(), cont->GetHexX(), cont->GetHexY(), cl->GetUseDist() ) )
            {
                cl->Send_XY( cl );
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - Transfer item container. Client '%s' distance more than allowed.\n", cl->GetInfo() );
                return;
            }

            // Check for close
            if( cont->GetContainer_Changeble() && !cont->GetOpened() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - Container is not open.\n" );
                return;
            }
        }
        // Get far container without checks
        else if( transfer_type == TRANSFER_FAR_CONT )
        {
            cont = ItemMngr.GetItem( cont_id, true );
            if( !cont || !cont->IsContainer() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - TRANSFER_FAR_CONT error.\n" );
                return;
            }
        }
        // Check and get self cont
        else
        {
            // Get item
            cont = cl->GetItem( cont_id, true );
            if( !cont || !cont->IsContainer() )
            {
                cl->Send_ContainerInfo();
                WriteLogF( _FUNC_, " - TRANSFER_SELF_CONT error2.\n" );
                return;
            }
        }

        // Process
        switch( take_flags )
        {
        case CONT_GET:
        {
            // Get item
            Item* item = cont->ContGetItem( item_id, true );
            if( !item )
            {
                cl->Send_ContainerInfo( cont, transfer_type, false );
                cl->Send_TextMsg( cl, STR_ITEM_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 0, item );

            // Check count
            if( !item_count || item->GetCount() < item_count )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Error count.", SAY_NETMSG );
                return;
            }

            // Script events
            if( !Script::RaiseInternalEvent( ServerFunctions.CritterUseSkill, cl, SKILL_TAKE_CONT, nullptr, item, nullptr ) )
                return;

            // Transfer
            if( !ItemMngr.MoveItemCritterFromCont( cont, cl, item, item_count ) )
                WriteLogF( _FUNC_, " - Transfer item, from container to player (get), fail.\n" );
        }
        break;
        case CONT_GETALL:
        {
            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 1, nullptr );

            // Get items
            ItemVec items;
            cont->ContGetAllItems( items, true, true );
            ItemMngr.FilterMoveItems( items, cont, cl );
            if( items.empty() )
            {
                cl->Send_ContainerInfo();
                return;
            }

            // Script events
            if( !Script::RaiseInternalEvent( ServerFunctions.CritterUseSkill, cl, SKILL_TAKE_ALL_CONT, nullptr, cont, nullptr ) )
                return;

            // Transfer
            for( auto it = items.begin(), end = items.end(); it != end; ++it )
            {
                Item* item = *it;
                if( Script::RaiseInternalEvent( ServerFunctions.CritterUseSkill, cl, SKILL_TAKE_CONT, nullptr, item, nullptr ) )
                {
                    cont->ContEraseItem( item );
                    cl->AddItem( item, true );
                }
            }
        }
        break;
        case CONT_PUT:
        {
            // Get item
            Item* item = cl->GetItem( item_id, true );
            if( !item || item->GetCritSlot() != SLOT_INV )
            {
                cl->Send_ContainerInfo();
                cl->Send_TextMsg( cl, STR_ITEM_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 2, item );

            // Check count
            if( !item_count || item->GetCount() < item_count )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Error count.", SAY_NETMSG );
                return;
            }

            // Check slot
            if( item->GetCritSlot() != SLOT_INV )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Cheat detected.", SAY_NETMSG );
                WriteLogF( _FUNC_, " - Attempting to put in a container not from the inventory, client '%s'.", cl->GetInfo() );
                return;
            }

            // Script events
            if( !Script::RaiseInternalEvent( ServerFunctions.CritterUseSkill, cl, SKILL_PUT_CONT, nullptr, item, nullptr ) )
                return;

            // Transfer
            if( !ItemMngr.MoveItemCritterToCont( cl, cont, item, item_count, 0 ) )
                WriteLogF( _FUNC_, " - Transfer item, from player to container (put), fail.\n" );
        }
        break;
        case CONT_PUTALL:
        {
            cl->Send_ContainerInfo();
        }
        break;
        default:
            break;
        }

        cl->Send_ContainerInfo( cont, transfer_type, false );

    }
/************************************************************************/
/* Critter container                                                    */
/************************************************************************/
    else if( transfer_type == TRANSFER_CRIT_LOOT || transfer_type == TRANSFER_CRIT_STEAL || transfer_type == TRANSFER_FAR_CRIT )
    {
        bool is_far = ( transfer_type == TRANSFER_FAR_CRIT );
        bool is_steal = ( transfer_type == TRANSFER_CRIT_STEAL );
        bool is_loot = ( transfer_type == TRANSFER_CRIT_LOOT );

        // Get critter target
        Critter* cr = ( is_far ? CrMngr.GetCritter( cont_id, true ) : ( cl->GetMapId() ? cl->GetCritSelf( cont_id, true ) : cl->GetGlobalMapCritter( cont_id ) ) );
        if( !cr )
        {
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - Critter not found.\n" );
            return;
        }
        SYNC_LOCK( cr );

        // Check for self
        if( cl == cr )
        {
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - Critter '%s' self pick.\n", cl->GetInfo() );
            return;
        }

        // Check NoSteal flag
        if( is_steal && cr->GetIsNoSteal() )
        {
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - Critter has NoSteal flag, critter '%s'.\n", cl->GetInfo() );
            return;
        }

        // Check dist
        if( !is_far && !CheckDist( cl->GetHexX(), cl->GetHexY(), cr->GetHexX(), cr->GetHexY(), cl->GetUseDist() + cr->GetMultihex() ) )
        {
            cl->Send_XY( cl );
            cl->Send_XY( cr );
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - Transfer critter container. Client '%s' distance more than allowed.\n", cl->GetInfo() );
            return;
        }

        // Check loot valid
        if( is_loot && !cr->IsDead() )
        {
            cl->Send_ContainerInfo();
            WriteLogF( _FUNC_, " - TRANSFER_CRIT_LOOT - Critter not dead.\n" );
            return;
        }

        // Check steal valid
        if( is_steal )
        {
            // Player is offline
            /*if(cr->IsPlayer() && cr->State!=STATE_PLAYING)
               {
                    cr->SendA_Text(&cr->VisCr,"Please, don't kill me",SAY_WHISP_ON_HEAD);
                    cl->Send_ContainerInfo();
                    WriteLog("Process_ContainerItem - TRANSFER_CRIT_STEAL - Critter not in game\n");
                    return;
               }*/

            // Npc in battle
            if( cr->IsNpc() && ( (Npc*) cr )->IsCurPlane( AI_PLANE_ATTACK ) )
            {
                cl->Send_ContainerInfo();
                return;
            }
        }

        // Process
        switch( take_flags )
        {
        case CONT_GET:
        {
            // Get item
            Item* item = cr->GetInvItem( item_id, transfer_type );
            if( !item )
            {
                cl->Send_ContainerInfo();
                cl->Send_TextMsg( cl, STR_ITEM_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 0, item );

            // Check count
            if( !item_count || item->GetCount() < item_count )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Incorrect count.", SAY_NETMSG );
                return;
            }

            // Script events
            if( !Script::RaiseInternalEvent( ServerFunctions.CritterUseSkill, cl, SKILL_TAKE_CONT, nullptr, item, nullptr ) )
                return;

            // Transfer
            if( !ItemMngr.MoveItemCritters( cr, cl, item, item_count ) )
                WriteLogF( _FUNC_, " - Transfer item, from player to player (get), fail.\n" );
        }
        break;
        case CONT_GETALL:
        {
            // Check for steal
            if( is_steal )
            {
                cl->Send_ContainerInfo();
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 1, nullptr );

            // Get items
            ItemVec items;
            cr->GetInvItems( items, transfer_type, true );
            ItemMngr.FilterMoveItems( items, cr, cl );
            if( items.empty() )
                return;

            // Script events
            if( !Script::RaiseInternalEvent( ServerFunctions.CritterUseSkill, cl, SKILL_TAKE_ALL_CONT, cr, nullptr, nullptr ) )
                return;

            // Transfer
            for( uint i = 0, j = (uint) items.size(); i < j; ++i )
            {
                if( Script::RaiseInternalEvent( ServerFunctions.CritterUseSkill, cl, SKILL_TAKE_CONT, nullptr, items[ i ], nullptr ) )
                {
                    cr->EraseItem( items[ i ], true );
                    cl->AddItem( items[ i ], true );
                }
            }
        }
        break;
        case CONT_PUT:
        {
            // Get item
            Item* item = cl->GetItem( item_id, true );
            if( !item || item->GetCritSlot() != SLOT_INV )
            {
                cl->Send_ContainerInfo();
                cl->Send_TextMsg( cl, STR_ITEM_NOT_FOUND, SAY_NETMSG, TEXTMSG_GAME );
                return;
            }

            // Send
            cl->SendAA_Action( ACTION_OPERATE_CONTAINER, transfer_type * 10 + 2, item );

            // Check count
            if( !item_count || item->GetCount() < item_count )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Incorrect count.", SAY_NETMSG );
                return;
            }

            // Check slot
            if( item->GetCritSlot() != SLOT_INV )
            {
                cl->Send_ContainerInfo();
                cl->Send_Text( cl, "Cheat detected.", SAY_NETMSG );
                WriteLogF( _FUNC_, " - Attempting to put in a container not from the inventory2, client '%s'.", cl->GetInfo() );
                return;
            }

            // Script events
            if( !Script::RaiseInternalEvent( ServerFunctions.CritterUseSkill, cl, SKILL_PUT_CONT, nullptr, item, nullptr ) )
                return;

            // Transfer
            if( !ItemMngr.MoveItemCritters( cl, cr, item, item_count ) )
                WriteLogF( _FUNC_, " - transfer item, from player to player (put), fail.\n" );
        }
        break;
        case CONT_PUTALL:
        {
            cl->Send_ContainerInfo();
        }
        break;
        default:
            break;
        }

        cl->Send_ContainerInfo( cr, transfer_type, false );
    }
}

void FOServer::Process_Dir( Client* cl )
{
    uchar dir;
    cl->Connection->Bin >> dir;
    CHECK_IN_BUFF_ERROR( cl );

    if( !cl->GetMapId() || dir >= DIRS_COUNT || cl->GetDir() == dir || cl->IsTalking() )
    {
        if( cl->GetDir() != dir )
            cl->Send_Dir( cl );
        return;
    }

    cl->SetDir( dir );
    cl->SendA_Dir();
}

void FOServer::Process_Ping( Client* cl )
{
    uchar ping;

    cl->Connection->Bin >> ping;
    CHECK_IN_BUFF_ERROR( cl );

    if( ping == PING_CLIENT )
    {
        cl->PingOk( PING_CLIENT_LIFE_TIME );
        return;
    }
    else if( ping == PING_UID_FAIL )
    {
        #ifndef DEV_VERSION
        SCOPE_LOCK( ClientsDataLocker );

        ClientData* data = GetClientData( cl->GetId() );
        if( data )
        {
            WriteLog( "Wrong UID, client '{}'. Disconnect.\n", cl->GetInfo() );
            for( int i = 0; i < 5; i++ )
                data->UID[ i ] = Random( 0, 10000 );
            data->UIDEndTick = Timer::FastTick() + GameOpt.AccountPlayTime * 1000;
        }
        cl->Disconnect();
        return;
        #endif
    }
    else if( ping == PING_PING || ping == PING_WAIT )
    {
        // Valid pings
    }
    else
    {
        WriteLog( "Unknown ping {}, client '{}'.\n", ping, cl->GetInfo() );
        return;
    }

    BOUT_BEGIN( cl );
    cl->Connection->Bout << NETMSG_PING;
    cl->Connection->Bout << ping;
    BOUT_END( cl );
}

void FOServer::Process_Property( Client* cl, uint data_size )
{
    uint              msg_len = 0;
    NetProperty::Type type = NetProperty::None;
    uint              cr_id = 0;
    uint              item_id = 0;
    ushort            property_index = 0;

    if( data_size == 0 )
        cl->Connection->Bin >> msg_len;

    char type_ = 0;
    cl->Connection->Bin >> type_;
    type = (NetProperty::Type) type_;

    uint additional_args = 0;
    switch( type )
    {
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

    CHECK_IN_BUFF_ERROR( cl );

    UCharVec data;
    if( data_size != 0 )
    {
        data.resize( data_size );
        cl->Connection->Bin.Pop( &data[ 0 ], data_size );
    }
    else
    {
        uint len = msg_len - sizeof( uint ) - sizeof( msg_len ) - sizeof( char ) - additional_args * sizeof( uint ) - sizeof( ushort );
        #pragma MESSAGE( "Control max size explicitly, add option to property registration" )
        if( len > 0xFFFF )         // For now 64Kb for all
            return;
        data.resize( len );
        cl->Connection->Bin.Pop( &data[ 0 ], len );
    }

    CHECK_IN_BUFF_ERROR( cl );

    bool      is_public = false;
    Property* prop = nullptr;
    Entity*   entity = nullptr;
    switch( type )
    {
    case NetProperty::Global:
        is_public = true;
        prop = GlobalVars::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = Globals;
        break;
    case NetProperty::Critter:
        is_public = true;
        prop = Critter::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = CrMngr.GetCritter( cr_id );
        break;
    case NetProperty::Chosen:
        prop = Critter::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = cl;
        break;
    case NetProperty::MapItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = ItemMngr.GetItem( item_id );
        break;
    case NetProperty::CritterItem:
        is_public = true;
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
        {
            Critter* cr = CrMngr.GetCritter( cr_id );
            if( cr )
                entity = cr->GetItem( item_id, true );
        }
        break;
    case NetProperty::ChosenItem:
        prop = Item::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = cl->GetItem( item_id, true );
        break;
    case NetProperty::Map:
        is_public = true;
        prop = Map::PropertiesRegistrator->Get( property_index );
        if( prop )
            entity = MapMngr.GetMap( cl->GetMapId() );
        break;
    case NetProperty::Location:
        is_public = true;
        prop = Location::PropertiesRegistrator->Get( property_index );
        if( prop )
        {
            Map* map = cl->GetMap();
            if( map )
                entity = map->GetLocation();
        }
        break;
    default:
        break;
    }
    if( !prop || !entity )
        return;

    Property::AccessType access = prop->GetAccess();
    if( is_public && !( access & Property::PublicMask ) )
        return;
    if( !is_public && !( access & ( Property::ProtectedMask | Property::PublicMask ) ) )
        return;
    if( !( access & Property::ModifiableMask ) )
        return;
    if( is_public && access != Property::PublicFullModifiable )
        return;
    if( prop->IsPOD() && data_size != prop->GetBaseSize() )
        return;
    if( !prop->IsPOD() && data_size != 0 )
        return;

    #pragma MESSAGE( "Disable send changing field by client to this client" )
    prop->SetData( entity, !data.empty() ? &data[ 0 ] : nullptr, (uint) data.size() );
}

void FOServer::OnSendGlobalValue( Entity* entity, Property* prop )
{
    if( ( prop->GetAccess() & Property::PublicMask ) != 0 )
    {
        ClVec players;
        CrMngr.GetClients( players );
        for( auto it = players.begin(); it != players.end(); ++it )
        {
            Client* cl = *it;
            cl->Send_Property( NetProperty::Global, prop, Globals );
        }
    }
}

void FOServer::OnSendCritterValue( Entity* entity, Property* prop )
{
    Critter* cr = (Critter*) entity;

    bool     is_public = ( prop->GetAccess() & Property::PublicMask ) != 0;
    bool     is_protected = ( prop->GetAccess() & Property::ProtectedMask ) != 0;
    if( is_public || is_protected )
        cr->Send_Property( NetProperty::Chosen, prop, cr );
    if( is_public )
        cr->SendA_Property( NetProperty::Critter, prop, cr );
}

void FOServer::OnSendMapValue( Entity* entity, Property* prop )
{
    if( ( prop->GetAccess() & Property::PublicMask ) != 0 )
    {
        Map* map = (Map*) entity;
        map->SendProperty( NetProperty::Map, prop, map );
    }
}

void FOServer::OnSendLocationValue( Entity* entity, Property* prop )
{
    if( ( prop->GetAccess() & Property::PublicMask ) != 0 )
    {
        Location* loc = (Location*) entity;
        MapVec    maps;
        loc->GetMaps( maps );
        for( auto it = maps.begin(); it != maps.end(); ++it )
        {
            Map* map = *it;
            map->SendProperty( NetProperty::Location, prop, loc );
        }
    }
}
