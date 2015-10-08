#include "EntityManager.h"
#include "ItemManager.h"
#include "CritterManager.h"
#include "MapManager.h"
#include "Script.h"

EntityManager EntityMngr;

EntityManager::EntityManager()
{
    currentId = 0;
    memzero( entitiesCount, sizeof( entitiesCount ) );
}

void EntityManager::RegisterEntity( Entity* entity )
{
    SCOPE_LOCK( entitiesLocker );

    if( !entity->GetId() )
        entity->SetId( ++currentId );

    auto it = allEntities.insert( PAIR( entity->GetId(), entity ) );
    RUNTIME_ASSERT( it.second );
    entitiesCount[ (int) entity->Type ]++;
}

void EntityManager::UnregisterEntity( Entity* entity )
{
    SCOPE_LOCK( entitiesLocker );

    auto it = allEntities.find( entity->GetId() );
    RUNTIME_ASSERT( it != allEntities.end() );
    allEntities.erase( it );
    entitiesCount[ (int) entity->Type ]--;
}

Entity* EntityManager::GetEntity( uint id, EntityType type )
{
    SCOPE_LOCK( entitiesLocker );

    auto it = allEntities.find( id );
    if( it != allEntities.end() && it->second->Type == type )
        return it->second;
    return NULL;
}

void EntityManager::GetEntities( EntityType type, EntityVec& entities )
{
    SCOPE_LOCK( entitiesLocker );

    entities.reserve( entities.size() + entitiesCount[ (int) type ] );
    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        if( it->second->Type == type )
            entities.push_back( it->second );
    }
}

uint EntityManager::GetEntitiesCount( EntityType type )
{
    SCOPE_LOCK( entitiesLocker );

    return entitiesCount[ (int) type ];
}

void EntityManager::GetItems( ItemVec& items )
{
    SCOPE_LOCK( entitiesLocker );

    items.reserve( items.size() + entitiesCount[ (int) EntityType::Item ] );
    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        if( it->second->Type == EntityType::Item )
            items.push_back( (Item*) it->second );
    }
}

void EntityManager::GetCritterItems( uint crid, ItemVec& items )
{
    SCOPE_LOCK( entitiesLocker );

    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        if( it->second->Type == EntityType::Item )
        {
            Item* item = (Item*) it->second;
            if( item->Accessory == ITEM_ACCESSORY_CRITTER && item->AccCritter.Id == crid )
                items.push_back( item );
        }
    }
}

Critter* EntityManager::GetCritter( uint id )
{
    SCOPE_LOCK( entitiesLocker );

    auto it = allEntities.find( id );
    if( it != allEntities.end() && ( it->second->Type == EntityType::Npc || it->second->Type == EntityType::Client ) )
        return (Critter*) it->second;
    return NULL;
}

void EntityManager::GetCritters( CrVec& critters )
{
    SCOPE_LOCK( entitiesLocker );

    critters.reserve( critters.size() + entitiesCount[ (int) EntityType::Npc ] + entitiesCount[ (int) EntityType::Client ] );
    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        if( it->second->Type == EntityType::Npc || it->second->Type == EntityType::Client )
            critters.push_back( (Critter*) it->second );
    }
}

Map* EntityManager::GetMapByPid( hash pid, uint skip_count )
{
    SCOPE_LOCK( entitiesLocker );

    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        if( it->second->Type == EntityType::Map )
        {
            Map* map = (Map*) it->second;
            if( map->Data.MapPid == pid )
            {
                if( !skip_count )
                    return map;
                else
                    skip_count--;
            }
        }
    }
    return NULL;
}

void EntityManager::GetMaps( MapVec& maps )
{
    SCOPE_LOCK( entitiesLocker );

    maps.reserve( maps.size() + entitiesCount[ (int) EntityType::Map ] );
    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        if( it->second->Type == EntityType::Map )
            maps.push_back( (Map*) it->second );
    }
}

Location* EntityManager::GetLocationByPid( hash pid, uint skip_count )
{
    SCOPE_LOCK( entitiesLocker );

    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        if( it->second->Type == EntityType::Location )
        {
            Location* loc = (Location*) it->second;
            if( loc->Data.LocPid == pid )
            {
                if( !skip_count )
                    return loc;
                else
                    skip_count--;
            }
        }
    }
    return NULL;
}

void EntityManager::GetLocations( LocVec& locs )
{
    SCOPE_LOCK( entitiesLocker );

    locs.reserve( locs.size() + entitiesCount[ (int) EntityType::Location ] );
    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        if( it->second->Type == EntityType::Location )
            locs.push_back( (Location*) it->second );
    }
}

void EntityManager::SaveEntities( void ( * save_func )( void*, size_t ) )
{
    save_func( &currentId, sizeof( currentId ) );

    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        Entity*    entity = it->second;
        EntityType type = entity->Type;
        if( type != EntityType::Item && type != EntityType::Npc && type != EntityType::Location && type != EntityType::Custom )
            continue;

        uint id = entity->GetId();
        RUNTIME_ASSERT( id );
        save_func( &id, sizeof( id ) );
        char type_c = (char) type;
        save_func( &type_c, sizeof( type_c ) );

        if( type == EntityType::Item )
        {
            Item* item = (Item*) entity;
            entity->Props.Save( save_func );
            save_func( &item->Proto->ProtoId, sizeof( item->Proto->ProtoId ) );
            save_func( &item->Accessory, sizeof( item->Accessory ) );
            save_func( &item->AccBuffer[ 0 ], sizeof( item->AccBuffer ) );
        }
        else if( type == EntityType::Npc )
        {
            Npc* npc = (Npc*) entity;
            entity->Props.Save( save_func );
            save_func( &npc->Data, sizeof( npc->Data ) );
            uint te_count = (uint) npc->CrTimeEvents.size();
            save_func( &te_count, sizeof( te_count ) );
            if( te_count )
                save_func( &npc->CrTimeEvents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) );
        }
        else if( type == EntityType::Location )
        {
            Location* loc = (Location*) entity;
            entity->Props.Save( save_func );
            save_func( &loc->Data, sizeof( loc->Data ) );
            MapVec& maps = loc->GetMapsNoLock();
            uint    map_count = (uint) maps.size();
            save_func( &map_count, sizeof( map_count ) );
            for( auto it_ = maps.begin(), end_ = maps.end(); it_ != end_; ++it_ )
            {
                Map* map = *it_;
                uint map_id = map->GetId();
                save_func( &map_id, sizeof( map_id ) );
                save_func( &map->Data, sizeof( map->Data ) );
                map->Props.Save( save_func );
            }
        }
        else if( type == EntityType::Custom )
        {
            CustomEntity* custom_entity = (CustomEntity*) entity;
            string        class_name = custom_entity->Props.GetClassName();
            ushort        class_name_len = (ushort) class_name.length();
            RUNTIME_ASSERT( class_name_len );
            save_func( &class_name_len, sizeof( class_name_len ) );
            save_func( (void*) class_name.c_str(), class_name_len );
            entity->Props.Save( save_func );
        }
        else
        {
            RUNTIME_ASSERT( !"Unreachable place" );
        }
    }
    uint zero = 0;
    save_func( &zero, sizeof( zero ) );
}

bool EntityManager::LoadEntities( void* file, uint version )
{
    WriteLog( "Load entities...\n" );

    PropertyRegistrator dummy_registrator( false, "Dummy" );
    dummy_registrator.FinishRegistration();

    if( !FileRead( file, &currentId, sizeof( currentId ) ) )
        return false;

    uint count = 0;
    while( true )
    {
        uint id;
        if( !FileRead( file, &id, sizeof( id ) ) )
            return false;
        if( !id )
            break;

        char type_c;
        if( !FileRead( file, &type_c, sizeof( type_c ) ) )
            return false;
        EntityType type = (EntityType) type_c;

        if( type == EntityType::Item )
        {
            Properties props( Item::PropertiesRegistrator );
            if( !props.Load( file, version ) )
                return false;

            hash pid;
            if( !FileRead( file, &pid, sizeof( pid ) ) )
                return false;
            uchar acc;
            if( !FileRead( file, &acc, sizeof( acc ) ) )
                return false;
            char acc_buf[ 8 ];
            if( !FileRead( file, &acc_buf[ 0 ], sizeof( acc_buf ) ) )
                return false;

            if( !ItemMngr.RestoreItem( id, pid, props, acc, acc_buf ) )
                WriteLog( "Fail to restore item '%s' with id %u.\n", Str::GetName( pid ), id );
        }
        else if( type == EntityType::Npc )
        {
            Properties props( Critter::PropertiesRegistrator );
            if( !props.Load( file, version ) )
                return false;

            CritData data;
            if( !FileRead( file, &data, sizeof( data ) ) )
                return false;
            Critter::CrTimeEventVec tevents;
            uint                    te_count;
            if( !FileRead( file, &te_count, sizeof( te_count ) ) )
                return false;
            if( te_count )
            {
                tevents.resize( te_count );
                if( !FileRead( file, &tevents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) ) )
                    return false;
            }

            if( !CrMngr.RestoreNpc( id, data, props, tevents ) )
                WriteLog( "Fail to restore npc '%s' with id %u on map '%s'.\n", Str::GetName( data.ProtoId ), id, Str::GetName( data.MapPid ) );
        }
        else if( type == EntityType::Location )
        {
            Properties loc_props( Location::PropertiesRegistrator );
            if( !loc_props.Load( file, version ) )
                return false;
            Location::LocData data;
            if( !FileRead( file, &data, sizeof( data ) ) )
                return false;

            uint map_count;
            if( !FileRead( file, &map_count, sizeof( map_count ) ) )
                return false;
            vector< uint >         map_ids( map_count );
            vector< Map::MapData > map_datas( map_count );
            vector< Properties* >  map_props( map_count );
            for( uint j = 0; j < map_count; j++ )
            {
                if( !FileRead( file, &map_ids[ j ], sizeof( map_ids[ j ] ) ) )
                    return false;
                if( !FileRead( file, &map_datas[ j ], sizeof( map_datas[ j ] ) ) )
                    return false;
                map_props[ j ] = new Properties( Map::PropertiesRegistrator );
                if( !map_props[ j ]->Load( file, version ) )
                    return false;
            }

            if( !MapMngr.RestoreLocation( id, data, loc_props, map_ids, map_datas, map_props ) )
                WriteLog( "Fail to restore location '%s' with id %u.\n", Str::GetName( data.LocPid ), id );
        }
        else if( type == EntityType::Custom )
        {
            char   class_name[ MAX_FOTEXT ];
            ushort class_name_len;
            if( !FileRead( file, &class_name_len, sizeof( class_name_len ) ) )
                return false;
            if( !FileRead( file, class_name, class_name_len ) )
                return false;
            class_name[ class_name_len ] = 0;

            PropertyRegistrator* registrator = Script::FindEntityRegistrator( class_name );
            if( registrator )
            {
                Properties props( registrator );
                if( !props.Load( file, version ) )
                    return false;

                Script::RestoreEntity( class_name, id, props );
            }
            else
            {
                Properties props( &dummy_registrator );
                if( !props.Load( file, version ) )
                    return false;

                WriteLog( "Fail to restore entity '%s' with id %u.\n", class_name, id );
            }
        }
        else
        {
            RUNTIME_ASSERT( !"Unreachable place" );
        }

        count++;
    }

    WriteLog( "Load entities complete, count %u.\n", count );
    return true;
}

void EntityManager::InitEntities()
{
    EntityMap entities = allEntities;
    for( auto it = entities.begin(); it != entities.end(); ++it )
    {
        Entity* entity = it->second;
        if( entity->IsDestroyed )
            continue;

        if( entity->Type == EntityType::Item )
        {
            Item* item = (Item*) entity;
            if( item->GetScriptId() )
                item->SetScript( NULL, false );
            if( !item->IsDestroyed && item->GetIsRadio() )
                ItemMngr.RadioRegister( item, true );
        }
        else if( entity->Type == EntityType::Npc )
        {
            Npc* npc = (Npc*) entity;
            npc->NextRefreshBagTick = Timer::GameTick() + GameOpt.BagRefreshTime * 60 * 1000;
            npc->RefreshName();
            if( Script::PrepareContext( ServerFunctions.CritterInit, _FUNC_, npc->GetInfo() ) )
            {
                Script::SetArgObject( npc );
                Script::SetArgBool( false );
                Script::RunPrepared();
            }
            if( !npc->IsDestroyed && npc->GetScriptId() )
                npc->SetScript( NULL, false );
        }
        else if( entity->Type == EntityType::Map )
        {
            Map* map = (Map*) entity;
            if( map->Data.ScriptId )
                map->SetScript( NULL, false );
        }
    }
}

void EntityManager::FinishEntities()
{
    WriteLog( "Finish entities...\n" );

    EntityMap entities = allEntities;
    for( auto it = entities.begin(); it != entities.end(); ++it )
    {
        Entity* entity = it->second;
        RUNTIME_ASSERT( !entity->IsDestroyed );

        if( entity->Type == EntityType::Item )
        {
            Item* item = (Item*) entity;

            item->EventFinish( false );
        }
        else if( entity->Type == EntityType::Npc || entity->Type == EntityType::Client )
        {
            Critter* cr = (Critter*) entity;

            cr->EventFinish( false );
            if( !cr->IsDestroyed && Script::PrepareContext( ServerFunctions.CritterFinish, _FUNC_, cr->GetInfo() ) )
            {
                Script::SetArgObject( cr );
                Script::SetArgBool( false );
                Script::RunPrepared();
            }

//                      if( entity->Type == EntityType::Client )
//                      {
//                              Client* cl = (Client*) cr;
//                              bool    to_delete = cl->Data.ClientToDelete;
//
//                              cr->EventFinish( to_delete );
//                              if( Script::PrepareContext( ServerFunctions.CritterFinish, _FUNC_, cr->GetInfo() ) )
//                              {
//                                      Script::SetArgObject( cr );
//                                      Script::SetArgBool( to_delete );
//                                      Script::RunPrepared();
//                              }
//
//                              if( to_delete )
//                              {
//                                      cl->DeleteInventory();
//                                      DeleteClientFile( cl->Name );
//                              }
//                      }
        }
        else if( entity->Type == EntityType::Location )
        {
            Location* loc = (Location*) entity;

            MapVec    maps;
            loc->GetMaps( maps, false );

            loc->EventFinish( false );
            for( auto it_ = maps.begin(); it_ != maps.end() && !loc->IsDestroyed; ++it_ )
                ( *it_ )->EventFinish( false );
        }
    }

    WriteLog( "Finish entities complete.\n" );
}

void EntityManager::ClearEntities()
{
    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        it->second->IsDestroyed = true;
        entitiesCount[ (int) it->second->Type ]--;
        SAFEREL( it->second );
    }
    allEntities.clear();

    for( int i = 0; i < (int) EntityType::Max; i++ )
        RUNTIME_ASSERT( entitiesCount[ i ] == 0 );
}
