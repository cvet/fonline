#include "Entity.h"
#include "StringUtils.h"
#include "Testing.h"

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
# include "Map.h"
# include "Critter.h"
# include "Item.h"
#endif
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
# include "MapView.h"
# include "CritterView.h"
# include "ItemView.h"
# include "ItemHexView.h"
#endif

ProtoEntity::ProtoEntity( hash proto_id, EntityType type, PropertyRegistrator* registrator ): Entity( 0, type, registrator, nullptr ), ProtoId( proto_id )
{
    RUNTIME_ASSERT( ProtoId );
}

bool ProtoEntity::HaveComponent( hash name ) const
{
    return Components.count( name ) > 0;
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
    switch( Type )
    {
    case EntityType::EntityProto:
    case EntityType::ItemProto:
    case EntityType::CritterProto:
    case EntityType::MapProto:
    case EntityType::LocationProto:
        return _str().parseHash( ( (ProtoEntity*) this )->ProtoId );
    default:
        break;
    }
    return Proto ? _str().parseHash( Proto->ProtoId ) : "Unnamed";
}

EntityVec Entity::GetChildren() const
{
    EntityVec children;
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    if( Type == EntityType::Npc || Type == EntityType::Client )
    {
        Critter* cr = (Critter*) this;
        for( auto& item : cr->GetInventory() )
            children.push_back( item );
    }
    else if( Type == EntityType::Item )
    {
        Item* cont = (Item*) this;
        if( cont->ChildItems )
        {
            for( auto& item :* cont->ChildItems )
                children.push_back( item );
        }
    }
    #endif
    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
    if( Type == EntityType::CritterView )
    {
        CritterView* cr = (CritterView*) this;
        for( auto& item : cr->InvItems )
            children.push_back( item );
    }
    else if( Type == EntityType::ItemView || Type == EntityType::ItemHexView )
    {
        ItemView* cont = (ItemView*) this;
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
        else if( Type == EntityType::EntityProto )
            delete (ProtoEntity*) this;
        else if( Type == EntityType::LocationProto )
            delete (ProtoLocation*) this;
        else if( Type == EntityType::MapProto )
            delete (ProtoMap*) this;
        else if( Type == EntityType::CritterProto )
            delete (ProtoCritter*) this;
        else if( Type == EntityType::ItemProto )
            delete (ProtoItem*) this;
        else if( Type == EntityType::Custom )
            delete (CustomEntity*) this;
        #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
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
        else if( Type == EntityType::ItemView )
            delete (ItemView*) this;
        else if( Type == EntityType::ItemHexView )
            delete (ItemHexView*) this;
        else if( Type == EntityType::CritterView )
            delete (CritterView*) this;
        else if( Type == EntityType::MapView )
            delete (MapView*) this;
        else if( Type == EntityType::LocationView )
            delete (LocationView*) this;
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

ProtoLocation::ProtoLocation( hash pid ): ProtoEntity( pid, EntityType::LocationProto, ProtoLocation::PropertiesRegistrator )
{
    // ...
}

PROPERTIES_IMPL( ProtoLocation );
CLASS_PROPERTY_IMPL( ProtoLocation, MapProtos );

PROPERTIES_IMPL( ProtoMap );
CLASS_PROPERTY_IMPL( ProtoMap, FileDir );
CLASS_PROPERTY_IMPL( ProtoMap, Width );
CLASS_PROPERTY_IMPL( ProtoMap, Height );
CLASS_PROPERTY_IMPL( ProtoMap, WorkHexX );
CLASS_PROPERTY_IMPL( ProtoMap, WorkHexY );
CLASS_PROPERTY_IMPL( ProtoMap, CurDayTime );
CLASS_PROPERTY_IMPL( ProtoMap, ScriptId );
CLASS_PROPERTY_IMPL( ProtoMap, DayTime );    // 4 int
CLASS_PROPERTY_IMPL( ProtoMap, DayColor );   // 12 uchar
CLASS_PROPERTY_IMPL( ProtoMap, IsNoLogOut );

ProtoCritter::ProtoCritter( hash pid ): ProtoEntity( pid, EntityType::CritterProto, ProtoCritter::PropertiesRegistrator )
{
    // ...
}

PROPERTIES_IMPL( ProtoCritter );
CLASS_PROPERTY_IMPL( ProtoCritter, Multihex );

ProtoItem::ProtoItem( hash pid ): ProtoEntity( pid, EntityType::ItemProto, ProtoItem::PropertiesRegistrator )
{
    InstanceCount = 0;
}

PROPERTIES_IMPL( ProtoItem );
CLASS_PROPERTY_IMPL( ProtoItem, PicMap );
CLASS_PROPERTY_IMPL( ProtoItem, PicInv );
CLASS_PROPERTY_IMPL( ProtoItem, Stackable );
CLASS_PROPERTY_IMPL( ProtoItem, OffsetX );
CLASS_PROPERTY_IMPL( ProtoItem, OffsetY );
CLASS_PROPERTY_IMPL( ProtoItem, Slot );
CLASS_PROPERTY_IMPL( ProtoItem, LightIntensity );
CLASS_PROPERTY_IMPL( ProtoItem, LightDistance );
CLASS_PROPERTY_IMPL( ProtoItem, LightFlags );
CLASS_PROPERTY_IMPL( ProtoItem, LightColor );
CLASS_PROPERTY_IMPL( ProtoItem, Count );
CLASS_PROPERTY_IMPL( ProtoItem, IsFlat );
CLASS_PROPERTY_IMPL( ProtoItem, DrawOrderOffsetHexY );
CLASS_PROPERTY_IMPL( ProtoItem, Corner );
CLASS_PROPERTY_IMPL( ProtoItem, DisableEgg );
CLASS_PROPERTY_IMPL( ProtoItem, IsStatic );
CLASS_PROPERTY_IMPL( ProtoItem, IsScenery );
CLASS_PROPERTY_IMPL( ProtoItem, IsWall );
CLASS_PROPERTY_IMPL( ProtoItem, IsBadItem );
CLASS_PROPERTY_IMPL( ProtoItem, IsColorize );
CLASS_PROPERTY_IMPL( ProtoItem, IsShowAnim );
CLASS_PROPERTY_IMPL( ProtoItem, IsShowAnimExt );
CLASS_PROPERTY_IMPL( ProtoItem, AnimStay0 );
CLASS_PROPERTY_IMPL( ProtoItem, AnimStay1 );
CLASS_PROPERTY_IMPL( ProtoItem, BlockLines );
