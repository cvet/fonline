#include "Entity.h"
#if defined ( FONLINE_SERVER )
# include "Item.h"
# include "Critter.h"
# include "Map.h"
#endif
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
# include "Item.h"
# include "ItemHex.h"
# include "CritterCl.h"
#endif

Entity::Entity( uint id, EntityType type, PropertyRegistrator* registartor ): Id( id ), Type( type ), Props( registartor )
{
    RUNTIME_ASSERT( Type != EntityType::Invalid );

    RefCounter = 1;
    IsDestroyed = false;
    IsDestroying = false;
}

uint Entity::GetId()
{
    return Id;
}

void Entity::SetId( uint id )
{
    RUNTIME_ASSERT( !Id );
    const_cast< uint& >( Id ) = id;
}

void Entity::AddRef() const
{
    InterlockedIncrement( &RefCounter );
}

void Entity::Release() const
{
    if( !InterlockedDecrement( &RefCounter ) )
    {
        if( Type == EntityType::Custom )
            delete (CustomEntity*) this;
        #if defined ( FONLINE_SERVER )
        else if( Type == EntityType::Item )
            delete (Item*) this;
        else if( Type == EntityType::Client )
            delete (Client*) this;
        else if( Type == EntityType::Npc )
            delete (Npc*) this;
        else if( Type == EntityType::Location )
            delete (Location*) this;
        else if( Type == EntityType::Map )
            delete (Map*) this;
        #endif
        #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
        else if( Type == EntityType::Item )
            delete (Item*) this;
        else if( Type == EntityType::CritterCl )
            delete (CritterCl*) this;
        else if( Type == EntityType::ItemHex )
            delete (ItemHex*) this;
        #endif
        else
            RUNTIME_ASSERT( !"Unreachable place" );
    }
}

CustomEntity::CustomEntity( uint id, uint sub_type, PropertyRegistrator* registrator ): Entity( id, EntityType::Custom, registrator ),
                                                                                        SubType( sub_type )
{
    //
}

PROPERTIES_IMPL( GlobalVars );
CLASS_PROPERTY_IMPL( GlobalVars, BestScores );
GlobalVars::GlobalVars(): Entity( 0, EntityType::Global, PropertiesRegistrator ) {}
GlobalVars* Globals;

PROPERTIES_IMPL( ClientMap );
ClientMap::ClientMap(): Entity( 0, EntityType::ClientMap, PropertiesRegistrator ) {}
ClientMap* ClientCurMap;

PROPERTIES_IMPL( ClientLocation );
ClientLocation::ClientLocation(): Entity( 0, EntityType::ClientLocation, PropertiesRegistrator ) {}
ClientLocation* ClientCurLocation;
