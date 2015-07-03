#include "EntityManager.h"
#include "ItemManager.h"
#include "CritterManager.h"
#include "Script.h"

EntityManager EntityMngr;

EntityManager::EntityManager()
{
    memzero( entitiesCount, sizeof( entitiesCount ) );
}

void EntityManager::RegisterEntity( Entity* entity )
{
    SCOPE_LOCK( entitiesLocker );

    RUNTIME_ASSERT( entity->GetId() );
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

Critter* EntityManager::GetCritter( uint id )
{
    SCOPE_LOCK( entitiesLocker );

    auto it = allEntities.find( id );
    if( it != allEntities.end() && ( it->second->Type == EntityType::Npc || it->second->Type == EntityType::Client ) )
        return (Critter*) it->second;
    return NULL;
}

void EntityManager::SaveEntities( void ( * save_func )( void*, size_t ) )
{
    save_func( &Entity::CurrentId, sizeof( Entity::CurrentId ) );

    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        Entity*    entity = it->second;
        EntityType type = entity->Type;
        if( type != EntityType::Item && type != EntityType::Npc )
            continue;

        uint id = entity->GetId();
        RUNTIME_ASSERT( id );
        save_func( &id, sizeof( id ) );
        entity->Props.Save( save_func );
        char type_c = (char) type;
        save_func( &type_c, sizeof( type_c ) );
        entity->Props.Save( save_func );

        if( type == EntityType::Item )
        {
            Item* item = (Item*) entity;
            save_func( &item->Proto->ProtoId, sizeof( item->Proto->ProtoId ) );
            save_func( &item->Accessory, sizeof( item->Accessory ) );
            save_func( &item->AccBuffer[ 0 ], sizeof( item->AccBuffer ) );
        }
        else if( type == EntityType::Npc )
        {
            Npc* npc = (Npc*) entity;
            save_func( &npc->Data, sizeof( npc->Data ) );
            npc->Props.Save( save_func );
            uint te_count = (uint) npc->CrTimeEvents.size();
            save_func( &te_count, sizeof( te_count ) );
            if( te_count )
                save_func( &npc->CrTimeEvents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) );
        }
    }
    uint zero = 0;
    save_func( &zero, sizeof( zero ) );
}

bool EntityManager::LoadEntities( void* file, uint version )
{
    WriteLog( "Load entities...\n" );

    PropertyRegistrator dummy_fields_registrator( false, "Dummy" );
    dummy_fields_registrator.FinishRegistration();

    if( !FileRead( file, &Entity::CurrentId, sizeof( Entity::CurrentId ) ) )
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
            Properties props( Item::PropertiesRegistrator, NULL );
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
                WriteLog( "Fail to restore item '%s' with id %u.\n", HASH_STR( pid ), id );
        }
        else if( type == EntityType::Npc )
        {
            CritData                data;
            FileRead( file, &data, sizeof( data ) );
            Properties              props( Critter::PropertiesRegistrator, NULL );
            props.Load( file, version );
            Critter::CrTimeEventVec tevents;
            uint                    te_count;
            FileRead( file, &te_count, sizeof( te_count ) );
            if( te_count )
            {
                tevents.resize( te_count );
                FileRead( file, &tevents[ 0 ], te_count * sizeof( Critter::CrTimeEvent ) );
            }

            if( !CrMngr.RestoreNpc( id, data, props, tevents ) )
                WriteLog( "Fail to create npc '%s' with id %u on map '%s'.\n", HASH_STR( data.ProtoId ), id, HASH_STR( data.MapPid ) );
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
                item->ParseScript( NULL, false );
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
                npc->ParseScript( NULL, false );
        }
        else if( entity->Type == EntityType::Location )
        {
            Location* loc = (Location*) entity;
        }
    }
}

void EntityManager::FinishEntities()
{
    while( !allEntities.empty() )
    {
        Entity* entity = allEntities.begin()->second;
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
        }

        if( !entity->IsDestroyed )
        {
            auto it_ = allEntities.find( entity->GetId() );
            RUNTIME_ASSERT( it_ != allEntities.end() );
            allEntities.erase( it_ );
            entity->IsDestroyed = true;
            entitiesCount[ (int) entity->Type ]--;
            entity->Release();
        }
    }
}

void EntityManager::ClearEntities()
{
    for( auto it = allEntities.begin(); it != allEntities.end(); ++it )
    {
        it->second->IsDestroyed = true;
        SAFEREL( it->second );
    }
}
