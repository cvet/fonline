#include "Entity.h"
#if defined ( FONLINE_SERVER )
# include "Item.h"
# include "Critter.h"
# include "Map.h"
#endif
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
# include "Item.h"
# include "ItemHex.h"
# include "CritterCl.h"
# include "MapCl.h"
#endif

ProtoEntity::ProtoEntity( hash proto_id, PropertyRegistrator* registartor ): Props( registartor ), ProtoId( proto_id ), RefCounter( 1 )
{
    //
}

ProtoEntity::~ProtoEntity()
{
    //
}

string ProtoEntity::GetName() const
{
    return _str().parseHash( ProtoId );
}

bool ProtoEntity::HaveComponent( hash name ) const
{
    return Components.count( name ) > 0;
}

void ProtoEntity::AddRef() const
{
    RefCounter++;
}

void ProtoEntity::Release() const
{
    if( --RefCounter == 0 )
        delete this;
}

Entity::Entity( uint id, EntityType type, PropertyRegistrator* registartor, ProtoEntity* proto ): Props( registartor ), Id( id ), Type( type ), Proto( proto ), RefCounter( 1 ), IsDestroyed( false ), IsDestroying( false )
{
    RUNTIME_ASSERT( Type != EntityType::None );

    if( Proto )
    {
        Proto->AddRef();
        Props = Proto->Props;
    }
}

Entity::~Entity()
{
    if( Proto )
        Proto->Release();
}

uint Entity::GetId() const
{
    return Id;
}

void Entity::SetId( uint id )
{
    const_cast< uint& >( Id ) = id;
}

hash Entity::GetProtoId() const
{
    return Proto ? Proto->ProtoId : 0;
}

string Entity::GetName() const
{
    return Proto ? Proto->GetName() : "Unnamed";
}

EntityVec Entity::GetChildren() const
{
    EntityVec children;
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
    # if defined ( FONLINE_SERVER )
    if( Type == EntityType::Npc || Type == EntityType::Client )
    {
        Critter* cr = (Critter*) this;
        for( auto& item : cr->GetInventory() )
            children.push_back( item );
    }
    # endif
    # if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
    if( Type == EntityType::CritterCl )
    {
        CritterCl* cr = (CritterCl*) this;
        for( auto& item : cr->InvItems )
            children.push_back( item );
    }
    # endif
    if( Type == EntityType::Item || Type == EntityType::ItemHex )
    {
        Item* cont = (Item*) this;
        if( cont->ChildItems )
        {
            for( auto& item :* cont->ChildItems )
                children.push_back( item );
        }
    }
    #endif
    return children;
}

void Entity::AddRef() const
{
    RefCounter++;
}

void Entity::Release() const
{
    if( --RefCounter == 0 )
    {
        if( Type == EntityType::Global )
            delete (GlobalVars*) this;
        else if( Type == EntityType::Custom )
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
        #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
        else if( Type == EntityType::Item )
            delete (Item*) this;
        else if( Type == EntityType::CritterCl )
            delete (CritterCl*) this;
        else if( Type == EntityType::ItemHex )
            delete (ItemHex*) this;
        else if( Type == EntityType::Location )
            delete (Location*) this;
        else if( Type == EntityType::Map )
            delete (Map*) this;
        #endif
        else
            RUNTIME_ASSERT( !"Unreachable place" );
    }
}

CustomEntity::CustomEntity( uint id, uint sub_type, PropertyRegistrator* registrator ): Entity( id, EntityType::Custom, registrator, nullptr ),
                                                                                        SubType( sub_type )
{
    //
}

GlobalVars::GlobalVars(): Entity( 0, EntityType::Global, PropertiesRegistrator, nullptr ) {}
GlobalVars* Globals;

PROPERTIES_IMPL( GlobalVars );
CLASS_PROPERTY_IMPL( GlobalVars, YearStart );
CLASS_PROPERTY_IMPL( GlobalVars, Year );
CLASS_PROPERTY_IMPL( GlobalVars, Month );
CLASS_PROPERTY_IMPL( GlobalVars, Day );
CLASS_PROPERTY_IMPL( GlobalVars, Hour );
CLASS_PROPERTY_IMPL( GlobalVars, Minute );
CLASS_PROPERTY_IMPL( GlobalVars, Second );
CLASS_PROPERTY_IMPL( GlobalVars, TimeMultiplier );
CLASS_PROPERTY_IMPL( GlobalVars, LastEntityId );
CLASS_PROPERTY_IMPL( GlobalVars, LastDeferredCallId );
CLASS_PROPERTY_IMPL( GlobalVars, HistoryRecordsId );
