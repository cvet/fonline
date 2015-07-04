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

uint Entity::CurrentId;

Entity::Entity( uint id, EntityType type, PropertyRegistrator* registartor ): Type( type ), Props( registartor, &IsDestroyed )
{
    RUNTIME_ASSERT( id );
    RUNTIME_ASSERT( Type != EntityType::Invalid );

    if( id == GenerateId )
        id = ++CurrentId;
    else if( id == DeferredId )
        id = 0;
    Id = id;

    RefCounter = 1;
    IsDestroyed = false;
    IsDestroying = false;
}

uint Entity::GetId()
{
    return Id;
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

void Entity::SetDeferredId( uint id ) const
{
    RUNTIME_ASSERT( !id );
    Id = id;
}

CustomEntity::CustomEntity( uint id, uint sub_type, PropertyRegistrator* registrator ): Entity( id, EntityType::Custom, registrator ),
                                                                                        SubType( sub_type )
{
    //
}
