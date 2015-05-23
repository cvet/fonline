#include "Common.h"
#include "CritterManager.h"
#include "ConstantsManager.h"
#include "ItemManager.h"
#include "IniParser.h"

#ifdef FONLINE_SERVER
# include "MapManager.h"
#endif

CritterManager CrMngr;

#define CRPROTO_APP    "Critter proto"

bool CritterManager::Init()
{
    WriteLog( "Critter manager initialization...\n" );

    allProtos.clear();
    Clear();

    WriteLog( "Critter manager initialization complete.\n" );
    return true;
}

void CritterManager::Finish()
{
    WriteLog( "Critter manager finish...\n" );

    #ifdef FONLINE_SERVER
    CritterGarbager();
    #endif

    allProtos.clear();
    Clear();

    WriteLog( "Critter manager finish complete.\n" );
}

void CritterManager::Clear()
{
    #ifdef FONLINE_SERVER
    for( auto it = allCritters.begin(), end = allCritters.end(); it != end; ++it )
        SAFEREL( ( *it ).second );
    allCritters.clear();
    crToDelete.clear();
    playersCount = 0;
    npcCount = 0;
    lastNpcId = NPC_START_ID;
    #endif
}

bool CritterManager::LoadProtos()
{
    WriteLog( "Load critter prototypes...\n" );

    FilesCollection files = FilesCollection( PT_SERVER_PRO_CRITTERS, "focr", true );
    uint            files_loaded = 0;
    int             errors = 0;
    while( files.IsNextFile() )
    {
        const char*  file_name;
        FileManager& file = files.GetNextFile( &file_name );
        if( !file.IsLoaded() )
        {
            WriteLog( "Unable to open file<%s>.\n", file_name );
            errors++;
            continue;
        }

        uint pid = Str::GetHash( file_name );
        if( allProtos.count( pid ) )
        {
            WriteLog( "Proto critter<%s> already loaded.\n", file_name );
            errors++;
            continue;
        }

        ProtoCritter* proto = new ProtoCritter();
        proto->ProtoId = pid;
        #ifdef FONLINE_SERVER
        proto->Props = new Properties( Critter::PropertiesRegistrator );
        #else
        proto->Props = new Properties( CritterCl::PropertiesRegistrator );
        #endif

        IniParser focr;
        focr.LoadFilePtr( (char*) file.GetBuf(), file.GetFsize() );
        if( focr.GotoNextApp( "Critter" ) )
        {
            if( !proto->Props->LoadFromText( focr.GetApp( "Critter" ) ) )
                errors++;
        }

        // Texts
        focr.CacheApps();
        StrSet& apps = focr.GetCachedApps();
        for( auto it = apps.begin(), end = apps.end(); it != end; ++it )
        {
            const string& app_name = *it;
            if( !( app_name.size() == 9 && app_name.find( "Text_" ) == 0 ) )
                continue;

            char* app_content = focr.GetApp( app_name.c_str() );
            FOMsg temp_msg;
            temp_msg.LoadFromString( app_content, Str::Length( app_content ) );
            SAFEDELA( app_content );

            FOMsg* msg = new FOMsg();
            uint   str_num = 0;
            while( str_num = temp_msg.GetStrNumUpper( str_num ) )
            {
                uint count = temp_msg.Count( str_num );
                uint new_str_num = CR_STR_ID( proto->ProtoId, str_num );
                for( uint n = 0; n < count; n++ )
                    msg->AddStr( new_str_num, temp_msg.GetStr( str_num, n ) );
            }

            proto->TextsLang.push_back( *(uint*) app_name.substr( 5 ).c_str() );
            proto->Texts.push_back( msg );
        }

        #ifdef FONLINE_MAPPER
        proto->CollectionName = "all";
        #endif

        allProtos.insert( PAIR( proto->ProtoId, proto ) );
        files_loaded++;
    }

    WriteLog( "Load critter prototypes complete, loaded<%u/%u>.\n", files_loaded, files.GetFilesCount() );
    return errors == 0;
}

ProtoCritter* CritterManager::GetProto( hash proto_id )
{
    auto it = allProtos.find( proto_id );
    return it != allProtos.end() ? ( *it ).second : NULL;
}

ProtoCritterMap& CritterManager::GetAllProtos()
{
    return allProtos;
}

#ifdef FONLINE_SERVER
void CritterManager::SaveCrittersFile( void ( * save_func )( void*, size_t ) )
{
    CrVec npcs;
    for( auto it = allCritters.begin(), end = allCritters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;
        if( cr->IsNpc() )
            npcs.push_back( cr );
    }

    uint count = (uint) npcs.size();
    save_func( &count, sizeof( count ) );
    for( auto it = npcs.begin(), end = npcs.end(); it != end; ++it )
    {
        Critter* npc = *it;
        npc->Data.IsDataExt = ( npc->DataExt ? true : false );
        save_func( &npc->Data, sizeof( npc->Data ) );
        if( npc->Data.IsDataExt )
            save_func( npc->DataExt, sizeof( CritDataExt ) );
        npc->Props.Save( save_func );
        uint te_count = (uint) npc->CrTimeEvents.size();
        save_func( &te_count, sizeof( te_count ) );
        if( te_count )
            save_func( &npc->CrTimeEvents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) );
    }
}

bool CritterManager::LoadCrittersFile( void* f, uint version )
{
    WriteLog( "Load npc...\n" );

    lastNpcId = 0;

    uint count;
    FileRead( f, &count, sizeof( count ) );
    if( !count )
        return true;

    uint errors = 0;
    for( uint i = 0; i < count; i++ )
    {
        CritData data;
        FileRead( f, &data, sizeof( data ) );

        CritDataExt data_ext;
        if( data.IsDataExt )
            FileRead( f, &data_ext, sizeof( data_ext ) );

        Properties props( Critter::PropertiesRegistrator );
        props.Load( f, version );

        Critter::CrTimeEventVec tevents;
        uint                    te_count;
        FileRead( f, &te_count, sizeof( te_count ) );
        if( te_count )
        {
            tevents.resize( te_count );
            FileRead( f, &tevents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) );
        }

        if( data.Id > lastNpcId )
            lastNpcId = data.Id;

        Npc* npc = CreateNpc( data.ProtoId, false );
        if( !npc )
        {
            WriteLog( "Unable to create npc with id<%u>, pid<%u> on map with id<%u>, pid<%u>.\n", data.Id, data.ProtoId, data.MapId, data.MapPid );
            errors++;
            continue;
        }

        npc->Data = data;
        if( data.IsDataExt )
        {
            CritDataExt* pdata_ext = npc->GetDataExt();
            if( pdata_ext )
                *pdata_ext = data_ext;
        }

        npc->Props = props;

        if( te_count )
            npc->CrTimeEvents = tevents;

        npc->NextRefreshBagTick = Timer::GameTick() + ( npc->Data.BagRefreshTime ? npc->Data.BagRefreshTime : GameOpt.BagRefreshTime ) * 60 * 1000;
        npc->RefreshName();

        AddCritter( npc );
    }

    if( errors )
        WriteLog( "Load npc complete, count<%u>, errors<%u>.\n", count - errors, errors );
    else
        WriteLog( "Load npc complete, count<%u>.\n", count );
    return true;
}

void CritterManager::RunInitScriptCritters()
{
    for( auto it = allCritters.begin(), end = allCritters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;
        if( Script::PrepareContext( ServerFunctions.CritterInit, _FUNC_, cr->GetInfo() ) )
        {
            Script::SetArgObject( cr );
            Script::SetArgBool( false );
            Script::RunPrepared();
        }
        if( cr->Data.ScriptId )
            cr->ParseScript( NULL, false );
    }
}

void CritterManager::CritterToGarbage( Critter* cr )
{
    SCOPE_LOCK( crLocker );
    crToDelete.push_back( cr->GetId() );
}

void CritterManager::CritterGarbager()
{
    if( !crToDelete.empty() )
    {
        crLocker.Lock();
        UIntVec to_del = crToDelete;
        crToDelete.clear();
        crLocker.Unlock();

        for( auto it = to_del.begin(), end = to_del.end(); it != end; ++it )
        {
            // Find and erase
            Critter* cr = NULL;

            crLocker.Lock();
            auto it_cr = allCritters.find( *it );
            if( it_cr != allCritters.end() )
                cr = ( *it_cr ).second;
            if( !cr || !cr->IsNpc() )
            {
                crLocker.Unlock();
                continue;
            }
            allCritters.erase( it_cr );
            npcCount--;
            crLocker.Unlock();

            SYNC_LOCK( cr );

            // Finish critter
            cr->LockMapTransfers++;

            Map* map = MapMngr.GetMap( cr->GetMap() );
            if( map )
            {
                cr->ClearVisible();
                map->EraseCritter( cr );
                map->EraseCritterEvents( cr );
                map->UnsetFlagCritter( cr->GetHexX(), cr->GetHexY(), cr->GetMultihex(), cr->IsDead() );
            }

            cr->EventFinish( true );
            if( Script::PrepareContext( ServerFunctions.CritterFinish, _FUNC_, cr->GetInfo() ) )
            {
                Script::SetArgObject( cr );
                Script::SetArgBool( true );
                Script::RunPrepared();
            }

            cr->LockMapTransfers--;

            // Erase from global
            if( cr->GroupMove )
            {
                GlobalMapGroup* group = cr->GroupMove;
                group->EraseCrit( cr );
                if( cr == group->Rule )
                {
                    for( auto it_ = group->CritMove.begin(), end_ = group->CritMove.end(); it_ != end_; ++it_ )
                    {
                        Critter* cr_ = *it_;
                        MapMngr.GM_GroupStartMove( cr_ );
                    }
                }
                else
                {
                    for( auto it_ = group->CritMove.begin(), end_ = group->CritMove.end(); it_ != end_; ++it_ )
                    {
                        Critter* cr_ = *it_;
                        cr_->Send_RemoveCritter( cr );
                    }

                    Item* car = cr->GetItemCar();
                    if( car && car->GetId() == group->CarId )
                    {
                        group->CarId = 0;
                        MapMngr.GM_GroupSetMove( group, group->ToX, group->ToY, 0.0f );                    // Stop others
                    }
                }
                cr->GroupMove = NULL;
            }

            cr->IsNotValid = true;
            cr->FullClear();
            Job::DeferredRelease( cr );
        }
    }
}

Npc* CritterManager::CreateNpc( hash proto_id, IntVec* props_data, IntVec* items_data, const char* script, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy )
{
    if( !map || hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
    {
        WriteLogF( _FUNC_, " - Wrong map values, hx<%u>, hy<%u>, map is nullptr<%s>.\n", hx, hy, !map ? "true" : "false" );
        return NULL;
    }

    if( !map->IsHexPassed( hx, hy ) )
    {
        if( accuracy )
        {
            WriteLogF( _FUNC_, " - Accuracy position busy, map pid<%u>, hx<%u>, hy<%u>.\n", map->GetPid(), hx, hy );
            return NULL;
        }

        static THREAD int cur_step = 0;
        short             hx_ = hx;
        short             hy_ = hy;
        short*            sx, * sy;
        GetHexOffsets( hx & 1, sx, sy );

        // Find in 2 hex radius
        for( int i = 0; ; i++ )
        {
            if( i >= 18 )
            {
                WriteLogF( _FUNC_, " - All positions busy, map pid<%u>, hx<%u>, hy<%u>.\n", map->GetPid(), hx, hy );
                return NULL;
            }
            cur_step++;
            if( cur_step >= 18 )
                cur_step = 0;
            if( hx_ + sx[ cur_step ] < 0 || hx_ + sx[ cur_step ] >= map->GetMaxHexX() )
                continue;
            if( hy_ + sy[ cur_step ] < 0 || hy_ + sy[ cur_step ] >= map->GetMaxHexY() )
                continue;
            if( !map->IsHexPassed( hx_ + sx[ cur_step ], hy_ + sy[ cur_step ] ) )
                continue;
            break;
        }

        hx_ += sx[ cur_step ];
        hy_ += sy[ cur_step ];
        hx = hx_;
        hy = hy_;
    }

    Npc* npc = CreateNpc( proto_id, true );
    if( !npc )
    {
        WriteLogF( _FUNC_, " - Create npc with proto<%s> fail.\n", HASH_STR( proto_id ) );
        return NULL;
    }

    crLocker.Lock();
    npc->Data.Id = lastNpcId + 1;
    lastNpcId++;
    crLocker.Unlock();

    // Flags and coords
    Location* loc = map->GetLocation( true );

    if( dir >= DIRS_COUNT )
        dir = Random( 0, DIRS_COUNT - 1 );
    npc->Data.Dir = dir;
    npc->Data.WorldX = ( loc ? loc->Data.WX : 100 );
    npc->Data.WorldY = ( loc ? loc->Data.WY : 100 );
    npc->SetHome( map->GetId(), hx, hy, dir );
    npc->SetMaps( map->GetId(), map->GetPid() );
    npc->Data.HexX = hx;
    npc->Data.HexY = hy;
    npc->RefreshName();

    if( props_data )
    {
        for( size_t i = 0, j = props_data->size() / 2; i < j; i++ )
            npc->Props.SetValueAsInt( props_data->at( i * 2 ), props_data->at( i * 2 + 1 ) );
    }

    map->AddCritter( npc );
    AddCritter( npc );

    if( items_data )
    {
        for( size_t i = 0, j = items_data->size() / 3; i < j; i++ )
        {
            hash pid = items_data->at( i * 3 );
            int  count = items_data->at( i * 3 + 1 );
            int  slot = items_data->at( i * 3 + 2 );
            if( pid != 0 && count > 0 && slot >= 0 && slot < 255 && Critter::SlotEnabled[ slot ] )
            {
                Item* item = ItemMngr.AddItemCritter( npc, pid, count );
                if( item && slot != SLOT_INV )
                    npc->MoveItem( SLOT_INV, slot, item->GetId(), item->GetCount() );
            }
        }
    }

    if( Script::PrepareContext( ServerFunctions.CritterInit, _FUNC_, npc->GetInfo() ) )
    {
        Script::SetArgObject( npc );
        Script::SetArgBool( true );
        Script::RunPrepared();
    }
    if( script )
        npc->ParseScript( script, true );
    map->AddCritterEvents( npc );

    npc->RefreshBag();
    npc->ProcessVisibleCritters();
    npc->ProcessVisibleItems();
    return npc;
}

Npc* CritterManager::CreateNpc( hash proto_id, bool copy_data )
{
    ProtoCritter* proto = GetProto( proto_id );
    if( !proto )
    {
        WriteLogF( _FUNC_, " - Critter proto<%s> not found.\n", HASH_STR( proto_id ) );
        return NULL;
    }

    Npc* npc = new Npc();
    if( !npc->SetDefaultItems( ItemMngr.GetProtoItem( ITEM_DEF_SLOT ), ItemMngr.GetProtoItem( ITEM_DEF_ARMOR ) ) )
    {
        delete npc;
        WriteLogF( _FUNC_, " - Can't set default items, critter proto<%s>.\n", HASH_STR( proto_id ) );
        return NULL;
    }

    if( copy_data )
        npc->Props = *proto->Props;
    npc->Data.EnemyStackCount = MAX_ENEMY_STACK;
    npc->Data.ProtoId = proto_id;
    npc->Data.Cond = COND_LIFE;
    npc->Data.Multihex = -1;

    SYNC_LOCK( npc );
    Job::PushBack( JOB_CRITTER, npc );
    return npc;
}

void CritterManager::AddCritter( Critter* cr )
{
    SCOPE_LOCK( crLocker );

    allCritters.insert( PAIR( cr->GetId(), cr ) );
    if( cr->IsPlayer() )
        playersCount++;
    else
        npcCount++;
}

void CritterManager::GetCopyCritters( CrVec& critters, bool sync_lock )
{
    crLocker.Lock();
    CrMap all_critters = allCritters;
    crLocker.Unlock();

    CrVec find_critters;
    find_critters.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        find_critters.push_back( ( *it ).second );

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        crLocker.Lock();
        all_critters = allCritters;
        crLocker.Unlock();

        CrVec find_critters2;
        find_critters2.reserve( find_critters.size() );
        for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
            find_critters2.push_back( ( *it ).second );

        // Search again, if different
        if( !CompareContainers( find_critters, find_critters2 ) )
        {
            GetCopyCritters( critters, sync_lock );
            return;
        }
    }

    critters = find_critters;
}

void CritterManager::GetCopyNpcs( PcVec& npcs, bool sync_lock )
{
    crLocker.Lock();
    CrMap all_critters = allCritters;
    crLocker.Unlock();

    PcVec find_npcs;
    find_npcs.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;
        if( cr->IsNpc() )
            find_npcs.push_back( (Npc*) cr );
    }

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_npcs.begin(), end = find_npcs.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        crLocker.Lock();
        all_critters = allCritters;
        crLocker.Unlock();

        PcVec find_npcs2;
        find_npcs2.reserve( find_npcs.size() );
        for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        {
            Critter* cr = ( *it ).second;
            if( cr->IsNpc() )
                find_npcs2.push_back( (Npc*) cr );
        }

        // Search again, if different
        if( !CompareContainers( find_npcs, find_npcs2 ) )
        {
            GetCopyNpcs( npcs, sync_lock );
            return;
        }
    }

    npcs = find_npcs;
}

void CritterManager::GetCopyPlayers( ClVec& players, bool sync_lock )
{
    crLocker.Lock();
    CrMap all_critters = allCritters;
    crLocker.Unlock();

    ClVec find_players;
    find_players.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;
        if( cr->IsPlayer() )
            find_players.push_back( (Client*) cr );
    }

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_players.begin(), end = find_players.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        crLocker.Lock();
        all_critters = allCritters;
        crLocker.Unlock();

        ClVec find_players2;
        find_players2.reserve( find_players.size() );
        for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        {
            Critter* cr = ( *it ).second;
            if( cr->IsPlayer() )
                find_players2.push_back( (Client*) cr );
        }

        // Search again, if different
        if( !CompareContainers( find_players, find_players2 ) )
        {
            GetCopyPlayers( players, sync_lock );
            return;
        }
    }

    players = find_players;
}

void CritterManager::GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type, CrVec& critters, bool sync_lock )
{
    crLocker.Lock();
    CrMap all_critters = allCritters;
    crLocker.Unlock();

    CrVec find_critters;
    find_critters.reserve( all_critters.size() );
    for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;
        if( !cr->GetMap() && cr->GroupMove && DistSqrt( (int) cr->GroupMove->CurX, (int) cr->GroupMove->CurY, wx, wy ) <= radius &&
            cr->CheckFind( find_type ) )
            find_critters.push_back( cr );
    }

    if( sync_lock && LogicMT )
    {
        // Synchronize
        for( auto it = find_critters.begin(), end = find_critters.end(); it != end; ++it )
            SYNC_LOCK( *it );

        // Recheck
        crLocker.Lock();
        all_critters = allCritters;
        crLocker.Unlock();

        CrVec find_critters2;
        find_critters2.reserve( find_critters.size() );
        for( auto it = all_critters.begin(), end = all_critters.end(); it != end; ++it )
        {
            Critter* cr = ( *it ).second;
            if( !cr->GetMap() && cr->GroupMove && DistSqrt( (int) cr->GroupMove->CurX, (int) cr->GroupMove->CurY, wx, wy ) <= radius &&
                cr->CheckFind( find_type ) )
                find_critters2.push_back( cr );
        }

        // Search again, if different
        if( !CompareContainers( find_critters, find_critters2 ) )
        {
            GetGlobalMapCritters( wx, wy, radius, find_type, critters, sync_lock );
            return;
        }
    }

    critters = find_critters;
}

Critter* CritterManager::GetCritter( uint crid, bool sync_lock )
{
    Critter* cr = NULL;

    crLocker.Lock();
    auto it = allCritters.find( crid );
    if( it != allCritters.end() )
        cr = ( *it ).second;
    crLocker.Unlock();

    if( cr && sync_lock )
        SYNC_LOCK( cr );
    return cr;
}

Client* CritterManager::GetPlayer( uint crid, bool sync_lock )
{
    if( !IS_USER_ID( crid ) )
        return NULL;

    Critter* cr = NULL;

    crLocker.Lock();
    auto it = allCritters.find( crid );
    if( it != allCritters.end() )
        cr = ( *it ).second;
    crLocker.Unlock();

    if( !cr || !cr->IsPlayer() )
        return NULL;
    if( sync_lock )
        SYNC_LOCK( cr );
    return (Client*) cr;
}

Client* CritterManager::GetPlayer( const char* name, bool sync_lock )
{
    Client* cl = NULL;

    crLocker.Lock();
    for( auto it = allCritters.begin(), end = allCritters.end(); it != end; ++it )
    {
        Critter* cr = ( *it ).second;
        if( cr->IsPlayer() && Str::CompareCaseUTF8( name, cr->GetName() ) )
        {
            cl = (Client*) cr;
            break;
        }
    }
    crLocker.Unlock();

    if( cl && sync_lock )
        SYNC_LOCK( cl );
    return cl;
}

Npc* CritterManager::GetNpc( uint crid, bool sync_lock )
{
    if( !IS_NPC_ID( crid ) )
        return NULL;

    Critter* cr = NULL;

    crLocker.Lock();
    auto it = allCritters.find( crid );
    if( it != allCritters.end() )
        cr = ( *it ).second;
    crLocker.Unlock();

    if( !cr || !cr->IsNpc() )
        return NULL;
    if( sync_lock )
        SYNC_LOCK( cr );
    return (Npc*) cr;
}

void CritterManager::EraseCritter( Critter* cr )
{
    SCOPE_LOCK( crLocker );

    auto it = allCritters.find( cr->GetId() );
    if( it != allCritters.end() )
    {
        if( cr->IsPlayer() )
            playersCount--;
        else
            npcCount--;
        allCritters.erase( it );
    }
}

uint CritterManager::PlayersInGame()
{
    SCOPE_LOCK( crLocker );

    uint count = playersCount;
    return count;
}

uint CritterManager::NpcInGame()
{
    SCOPE_LOCK( crLocker );

    uint count = npcCount;
    return count;
}

uint CritterManager::CrittersInGame()
{
    SCOPE_LOCK( crLocker );

    uint count = (uint) allCritters.size();
    return count;
}

#endif
