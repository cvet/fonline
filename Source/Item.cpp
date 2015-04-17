#include "Common.h"
#include "Item.h"
#include "ItemManager.h"

#ifdef FONLINE_SERVER
# include "MapManager.h"
# include "CritterManager.h"
# include "AI.h"
#endif

const char* ItemEventFuncName[ ITEM_EVENT_MAX ] =
{
    "void %s(Item&,bool)",                             // ITEM_EVENT_FINISH
    "bool %s(Item&,Critter&,Critter&)",                // ITEM_EVENT_ATTACK
    "bool %s(Item&,Critter&,Critter@,Item@,Scenery@)", // ITEM_EVENT_USE
    "bool %s(Item&,Critter&,Item@)",                   // ITEM_EVENT_USE_ON_ME
    "bool %s(Item&,Critter&,CritterProperty)",         // ITEM_EVENT_SKILL
    "void %s(Item&,Critter&)",                         // ITEM_EVENT_DROP
    "void %s(Item&,Critter&,uint8)",                   // ITEM_EVENT_MOVE
    "void %s(Item&,Critter&,bool,uint8)",              // ITEM_EVENT_WALK
};

PROPERTIES_IMPL( ProtoItem );
CLASS_PROPERTY_IMPL( ProtoItem, Type );
CLASS_PROPERTY_IMPL( ProtoItem, Flags );
CLASS_PROPERTY_IMPL( ProtoItem, Stackable );
CLASS_PROPERTY_IMPL( ProtoItem, Deteriorable );
CLASS_PROPERTY_IMPL( ProtoItem, PicMap );
CLASS_PROPERTY_IMPL( ProtoItem, PicInv );
CLASS_PROPERTY_IMPL( ProtoItem, GroundLevel );
CLASS_PROPERTY_IMPL( ProtoItem, Corner );
CLASS_PROPERTY_IMPL( ProtoItem, Slot );
CLASS_PROPERTY_IMPL( ProtoItem, Weight );
CLASS_PROPERTY_IMPL( ProtoItem, Volume );
CLASS_PROPERTY_IMPL( ProtoItem, Cost );
CLASS_PROPERTY_IMPL( ProtoItem, StartCount );
CLASS_PROPERTY_IMPL( ProtoItem, SoundId );
CLASS_PROPERTY_IMPL( ProtoItem, Material );
CLASS_PROPERTY_IMPL( ProtoItem, LightFlags );
CLASS_PROPERTY_IMPL( ProtoItem, LightDistance );
CLASS_PROPERTY_IMPL( ProtoItem, LightIntensity );
CLASS_PROPERTY_IMPL( ProtoItem, LightColor );
CLASS_PROPERTY_IMPL( ProtoItem, DisableEgg );
CLASS_PROPERTY_IMPL( ProtoItem, AnimWaitBase );
CLASS_PROPERTY_IMPL( ProtoItem, AnimWaitRndMin );
CLASS_PROPERTY_IMPL( ProtoItem, AnimWaitRndMax );
CLASS_PROPERTY_IMPL( ProtoItem, AnimStay_0 );
CLASS_PROPERTY_IMPL( ProtoItem, AnimStay_1 );
CLASS_PROPERTY_IMPL( ProtoItem, AnimShow_0 );
CLASS_PROPERTY_IMPL( ProtoItem, AnimShow_1 );
CLASS_PROPERTY_IMPL( ProtoItem, AnimHide_0 );
CLASS_PROPERTY_IMPL( ProtoItem, AnimHide_1 );
CLASS_PROPERTY_IMPL( ProtoItem, OffsetX );
CLASS_PROPERTY_IMPL( ProtoItem, OffsetY );
CLASS_PROPERTY_IMPL( ProtoItem, SpriteCut );
CLASS_PROPERTY_IMPL( ProtoItem, DrawOrderOffsetHexY );
CLASS_PROPERTY_IMPL( ProtoItem, RadioChannel );
CLASS_PROPERTY_IMPL( ProtoItem, RadioFlags );
CLASS_PROPERTY_IMPL( ProtoItem, RadioBroadcastSend );
CLASS_PROPERTY_IMPL( ProtoItem, RadioBroadcastRecv );
CLASS_PROPERTY_IMPL( ProtoItem, IndicatorStart );
CLASS_PROPERTY_IMPL( ProtoItem, IndicatorMax );
CLASS_PROPERTY_IMPL( ProtoItem, HolodiskNum );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_0 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_1 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_2 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_3 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_4 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_5 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_6 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_7 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_8 );
CLASS_PROPERTY_IMPL( ProtoItem, StartValue_9 );
CLASS_PROPERTY_IMPL( ProtoItem, BlockLines );
CLASS_PROPERTY_IMPL( ProtoItem, ChildPid_0 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildPid_1 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildPid_2 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildPid_3 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildPid_4 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildLines_0 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildLines_1 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildLines_2 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildLines_3 );
CLASS_PROPERTY_IMPL( ProtoItem, ChildLines_4 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_IsUnarmed );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_UnarmedTree );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_UnarmedPriority );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_UnarmedMinAgility );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_UnarmedMinUnarmed );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_UnarmedMinLevel );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Anim1 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_MaxAmmoCount );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Caliber );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_DefaultAmmoPid );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_MinStrength );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Perk );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_ActiveUses );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Skill_0 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Skill_1 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Skill_2 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_PicUse_0 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_PicUse_1 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_PicUse_2 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_MaxDist_0 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_MaxDist_1 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_MaxDist_2 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Round_0 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Round_1 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Round_2 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_ApCost_0 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_ApCost_1 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_ApCost_2 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Aim_0 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Aim_1 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_Aim_2 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_SoundId_0 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_SoundId_1 );
CLASS_PROPERTY_IMPL( ProtoItem, Weapon_SoundId_2 );
CLASS_PROPERTY_IMPL( ProtoItem, Ammo_Caliber );
CLASS_PROPERTY_IMPL( ProtoItem, Door_NoBlockMove );
CLASS_PROPERTY_IMPL( ProtoItem, Door_NoBlockShoot );
CLASS_PROPERTY_IMPL( ProtoItem, Door_NoBlockLight );
CLASS_PROPERTY_IMPL( ProtoItem, Container_Volume );
CLASS_PROPERTY_IMPL( ProtoItem, Container_Changeble );
CLASS_PROPERTY_IMPL( ProtoItem, Container_CannotPickUp );
CLASS_PROPERTY_IMPL( ProtoItem, Container_MagicHandsGrnd );
CLASS_PROPERTY_IMPL( ProtoItem, Locker_Condition );
CLASS_PROPERTY_IMPL( ProtoItem, Grid_Type );
CLASS_PROPERTY_IMPL( ProtoItem, Car_Speed );
CLASS_PROPERTY_IMPL( ProtoItem, Car_Passability );
CLASS_PROPERTY_IMPL( ProtoItem, Car_DeteriorationRate );
CLASS_PROPERTY_IMPL( ProtoItem, Car_CrittersCapacity );
CLASS_PROPERTY_IMPL( ProtoItem, Car_TankVolume );
CLASS_PROPERTY_IMPL( ProtoItem, Car_MaxDeterioration );
CLASS_PROPERTY_IMPL( ProtoItem, Car_FuelConsumption );
CLASS_PROPERTY_IMPL( ProtoItem, Car_Entrance );
CLASS_PROPERTY_IMPL( ProtoItem, Car_MovementType );

void ProtoItem::SetPropertyRegistrator( PropertyRegistrator* registrator )
{
    PROPERTIES_FIND();
    CLASS_PROPERTY_FIND( Type );
    CLASS_PROPERTY_FIND( Flags );
    CLASS_PROPERTY_FIND( Stackable );
    CLASS_PROPERTY_FIND( Deteriorable );
    CLASS_PROPERTY_FIND( PicMap );
    CLASS_PROPERTY_FIND( PicInv );
    CLASS_PROPERTY_FIND( GroundLevel );
    CLASS_PROPERTY_FIND( Corner );
    CLASS_PROPERTY_FIND( Slot );
    CLASS_PROPERTY_FIND( Weight );
    CLASS_PROPERTY_FIND( Volume );
    CLASS_PROPERTY_FIND( Cost );
    CLASS_PROPERTY_FIND( StartCount );
    CLASS_PROPERTY_FIND( SoundId );
    CLASS_PROPERTY_FIND( Material );
    CLASS_PROPERTY_FIND( LightFlags );
    CLASS_PROPERTY_FIND( LightDistance );
    CLASS_PROPERTY_FIND( LightIntensity );
    CLASS_PROPERTY_FIND( LightColor );
    CLASS_PROPERTY_FIND( DisableEgg );
    CLASS_PROPERTY_FIND( AnimWaitBase );
    CLASS_PROPERTY_FIND( AnimWaitRndMin );
    CLASS_PROPERTY_FIND( AnimWaitRndMax );
    CLASS_PROPERTY_FIND( AnimStay_0 );
    CLASS_PROPERTY_FIND( AnimStay_1 );
    CLASS_PROPERTY_FIND( AnimShow_0 );
    CLASS_PROPERTY_FIND( AnimShow_1 );
    CLASS_PROPERTY_FIND( AnimHide_0 );
    CLASS_PROPERTY_FIND( AnimHide_1 );
    CLASS_PROPERTY_FIND( OffsetX );
    CLASS_PROPERTY_FIND( OffsetY );
    CLASS_PROPERTY_FIND( SpriteCut );
    CLASS_PROPERTY_FIND( DrawOrderOffsetHexY );
    CLASS_PROPERTY_FIND( RadioChannel );
    CLASS_PROPERTY_FIND( RadioFlags );
    CLASS_PROPERTY_FIND( RadioBroadcastSend );
    CLASS_PROPERTY_FIND( RadioBroadcastRecv );
    CLASS_PROPERTY_FIND( IndicatorStart );
    CLASS_PROPERTY_FIND( IndicatorMax );
    CLASS_PROPERTY_FIND( HolodiskNum );
    CLASS_PROPERTY_FIND( StartValue_0 );
    CLASS_PROPERTY_FIND( StartValue_1 );
    CLASS_PROPERTY_FIND( StartValue_2 );
    CLASS_PROPERTY_FIND( StartValue_3 );
    CLASS_PROPERTY_FIND( StartValue_4 );
    CLASS_PROPERTY_FIND( StartValue_5 );
    CLASS_PROPERTY_FIND( StartValue_6 );
    CLASS_PROPERTY_FIND( StartValue_7 );
    CLASS_PROPERTY_FIND( StartValue_8 );
    CLASS_PROPERTY_FIND( StartValue_9 );
    CLASS_PROPERTY_FIND( BlockLines );
    CLASS_PROPERTY_FIND( ChildPid_0 );
    CLASS_PROPERTY_FIND( ChildPid_1 );
    CLASS_PROPERTY_FIND( ChildPid_2 );
    CLASS_PROPERTY_FIND( ChildPid_3 );
    CLASS_PROPERTY_FIND( ChildPid_4 );
    CLASS_PROPERTY_FIND( ChildLines_0 );
    CLASS_PROPERTY_FIND( ChildLines_1 );
    CLASS_PROPERTY_FIND( ChildLines_2 );
    CLASS_PROPERTY_FIND( ChildLines_3 );
    CLASS_PROPERTY_FIND( ChildLines_4 );
    CLASS_PROPERTY_FIND( Weapon_IsUnarmed );
    CLASS_PROPERTY_FIND( Weapon_UnarmedTree );
    CLASS_PROPERTY_FIND( Weapon_UnarmedPriority );
    CLASS_PROPERTY_FIND( Weapon_UnarmedMinAgility );
    CLASS_PROPERTY_FIND( Weapon_UnarmedMinUnarmed );
    CLASS_PROPERTY_FIND( Weapon_UnarmedMinLevel );
    CLASS_PROPERTY_FIND( Weapon_Anim1 );
    CLASS_PROPERTY_FIND( Weapon_MaxAmmoCount );
    CLASS_PROPERTY_FIND( Weapon_Caliber );
    CLASS_PROPERTY_FIND( Weapon_DefaultAmmoPid );
    CLASS_PROPERTY_FIND( Weapon_MinStrength );
    CLASS_PROPERTY_FIND( Weapon_Perk );
    CLASS_PROPERTY_FIND( Weapon_ActiveUses );
    CLASS_PROPERTY_FIND( Weapon_Skill_0 );
    CLASS_PROPERTY_FIND( Weapon_Skill_1 );
    CLASS_PROPERTY_FIND( Weapon_Skill_2 );
    CLASS_PROPERTY_FIND( Weapon_PicUse_0 );
    CLASS_PROPERTY_FIND( Weapon_PicUse_1 );
    CLASS_PROPERTY_FIND( Weapon_PicUse_2 );
    CLASS_PROPERTY_FIND( Weapon_MaxDist_0 );
    CLASS_PROPERTY_FIND( Weapon_MaxDist_1 );
    CLASS_PROPERTY_FIND( Weapon_MaxDist_2 );
    CLASS_PROPERTY_FIND( Weapon_Round_0 );
    CLASS_PROPERTY_FIND( Weapon_Round_1 );
    CLASS_PROPERTY_FIND( Weapon_Round_2 );
    CLASS_PROPERTY_FIND( Weapon_ApCost_0 );
    CLASS_PROPERTY_FIND( Weapon_ApCost_1 );
    CLASS_PROPERTY_FIND( Weapon_ApCost_2 );
    CLASS_PROPERTY_FIND( Weapon_Aim_0 );
    CLASS_PROPERTY_FIND( Weapon_Aim_1 );
    CLASS_PROPERTY_FIND( Weapon_Aim_2 );
    CLASS_PROPERTY_FIND( Weapon_SoundId_0 );
    CLASS_PROPERTY_FIND( Weapon_SoundId_1 );
    CLASS_PROPERTY_FIND( Weapon_SoundId_2 );
    CLASS_PROPERTY_FIND( Ammo_Caliber );
    CLASS_PROPERTY_FIND( Door_NoBlockMove );
    CLASS_PROPERTY_FIND( Door_NoBlockShoot );
    CLASS_PROPERTY_FIND( Door_NoBlockLight );
    CLASS_PROPERTY_FIND( Container_Volume );
    CLASS_PROPERTY_FIND( Container_Changeble );
    CLASS_PROPERTY_FIND( Container_CannotPickUp );
    CLASS_PROPERTY_FIND( Container_MagicHandsGrnd );
    CLASS_PROPERTY_FIND( Locker_Condition );
    CLASS_PROPERTY_FIND( Grid_Type );
    CLASS_PROPERTY_FIND( Car_Speed );
    CLASS_PROPERTY_FIND( Car_Passability );
    CLASS_PROPERTY_FIND( Car_DeteriorationRate );
    CLASS_PROPERTY_FIND( Car_CrittersCapacity );
    CLASS_PROPERTY_FIND( Car_TankVolume );
    CLASS_PROPERTY_FIND( Car_MaxDeterioration );
    CLASS_PROPERTY_FIND( Car_FuelConsumption );
    CLASS_PROPERTY_FIND( Car_Entrance );
    CLASS_PROPERTY_FIND( Car_MovementType );
}

ProtoItem::ProtoItem(): Props( PropertiesRegistrator )
{
    // Dummy
}

const char* ProtoItem::GetBlockLinesStr()
{
    uint   data_size;
    uchar* data = GetBlockLinesData( data_size );
    return data ? (char*) data : "";
}

hash ProtoItem::GetChildPid( uint index )
{
    if( index == 0 )
        return GetChildPid_0();
    else if( index == 0 )
        return GetChildPid_1();
    else if( index == 0 )
        return GetChildPid_2();
    else if( index == 0 )
        return GetChildPid_3();
    else if( index == 0 )
        return GetChildPid_4();
    return 0;
}

const char* ProtoItem::GetChildLinesStr( uint index )
{
    uint   data_size;
    uchar* data = NULL;
    if( index == 0 )
        data = GetChildLines_0Data( data_size );
    else if( index == 0 )
        data = GetChildLines_1Data( data_size );
    else if( index == 0 )
        data = GetChildLines_2Data( data_size );
    else if( index == 0 )
        data = GetChildLines_3Data( data_size );
    else if( index == 0 )
        data = GetChildLines_4Data( data_size );
    return data ? (char*) data : "";
}

PROPERTIES_IMPL( Item );
#ifdef FONLINE_SERVER
CLASS_PROPERTY_IMPL( Item, ScriptId );
CLASS_PROPERTY_IMPL( Item, LockerComplexity );
#endif
CLASS_PROPERTY_IMPL( Item, SortValue );
CLASS_PROPERTY_IMPL( Item, Indicator );
CLASS_PROPERTY_IMPL( Item, PicMap );
CLASS_PROPERTY_IMPL( Item, PicInv );
CLASS_PROPERTY_IMPL( Item, Flags );
CLASS_PROPERTY_IMPL( Item, Mode );
CLASS_PROPERTY_IMPL( Item, LightIntensity );
CLASS_PROPERTY_IMPL( Item, LightDistance );
CLASS_PROPERTY_IMPL( Item, LightFlags );
CLASS_PROPERTY_IMPL( Item, LightColor );
CLASS_PROPERTY_IMPL( Item, Count );
CLASS_PROPERTY_IMPL( Item, Cost );
CLASS_PROPERTY_IMPL( Item, Val0 );
CLASS_PROPERTY_IMPL( Item, Val1 );
CLASS_PROPERTY_IMPL( Item, Val2 );
CLASS_PROPERTY_IMPL( Item, Val3 );
CLASS_PROPERTY_IMPL( Item, Val4 );
CLASS_PROPERTY_IMPL( Item, Val5 );
CLASS_PROPERTY_IMPL( Item, Val6 );
CLASS_PROPERTY_IMPL( Item, Val7 );
CLASS_PROPERTY_IMPL( Item, Val8 );
CLASS_PROPERTY_IMPL( Item, Val9 );
CLASS_PROPERTY_IMPL( Item, BrokenFlags );
CLASS_PROPERTY_IMPL( Item, BrokenCount );
CLASS_PROPERTY_IMPL( Item, Deterioration );
CLASS_PROPERTY_IMPL( Item, AmmoPid );
CLASS_PROPERTY_IMPL( Item, AmmoCount );
CLASS_PROPERTY_IMPL( Item, TrapValue );
CLASS_PROPERTY_IMPL( Item, LockerId );
CLASS_PROPERTY_IMPL( Item, LockerCondition );
CLASS_PROPERTY_IMPL( Item, HolodiskNumber );
CLASS_PROPERTY_IMPL( Item, RadioChannel );
CLASS_PROPERTY_IMPL( Item, RadioFlags );
CLASS_PROPERTY_IMPL( Item, RadioBroadcastSend );
CLASS_PROPERTY_IMPL( Item, RadioBroadcastRecv );
CLASS_PROPERTY_IMPL( Item, OffsetX );
CLASS_PROPERTY_IMPL( Item, OffsetY );

void Item::SetPropertyRegistrator( PropertyRegistrator* registrator )
{
    PROPERTIES_FIND();
    #ifdef FONLINE_SERVER
    CLASS_PROPERTY_FIND( ScriptId );
    CLASS_PROPERTY_FIND( LockerComplexity );
    #endif
    CLASS_PROPERTY_FIND( SortValue );
    CLASS_PROPERTY_FIND( Indicator );
    CLASS_PROPERTY_FIND( PicMap );
    CLASS_PROPERTY_FIND( PicInv );
    CLASS_PROPERTY_FIND( Flags );
    CLASS_PROPERTY_FIND( Mode );
    CLASS_PROPERTY_FIND( LightIntensity );
    CLASS_PROPERTY_FIND( LightDistance );
    CLASS_PROPERTY_FIND( LightFlags );
    CLASS_PROPERTY_FIND( LightColor );
    CLASS_PROPERTY_FIND( Count );
    CLASS_PROPERTY_FIND( Cost );
    CLASS_PROPERTY_FIND( Val0 );
    CLASS_PROPERTY_FIND( Val1 );
    CLASS_PROPERTY_FIND( Val2 );
    CLASS_PROPERTY_FIND( Val3 );
    CLASS_PROPERTY_FIND( Val4 );
    CLASS_PROPERTY_FIND( Val5 );
    CLASS_PROPERTY_FIND( Val6 );
    CLASS_PROPERTY_FIND( Val7 );
    CLASS_PROPERTY_FIND( Val8 );
    CLASS_PROPERTY_FIND( Val9 );
    CLASS_PROPERTY_FIND( BrokenFlags );
    CLASS_PROPERTY_FIND( BrokenCount );
    CLASS_PROPERTY_FIND( Deterioration );
    CLASS_PROPERTY_FIND( AmmoPid );
    CLASS_PROPERTY_FIND( AmmoCount );
    CLASS_PROPERTY_FIND( TrapValue );
    CLASS_PROPERTY_FIND( LockerId );
    CLASS_PROPERTY_FIND( LockerCondition );
    CLASS_PROPERTY_FIND( HolodiskNumber );
    CLASS_PROPERTY_FIND( RadioChannel );
    CLASS_PROPERTY_FIND( RadioFlags );
    CLASS_PROPERTY_FIND( RadioBroadcastSend );
    CLASS_PROPERTY_FIND( RadioBroadcastRecv );
    CLASS_PROPERTY_FIND( OffsetX );
    CLASS_PROPERTY_FIND( OffsetY );
}

Item::Item( uint id, ProtoItem* proto ): Props( PropertiesRegistrator )
{
    RUNTIME_ASSERT( proto );

    MEMORY_PROCESS( MEMORY_ITEM, sizeof( Item ) + PropertiesRegistrator->GetWholeDataSize() );

    Id = 0;

    RefCounter = 1;
    IsNotValid = false;
    ViewPlaceOnMap = false;
    Accessory = 0;
    memzero( AccBuffer, sizeof( AccBuffer ) );

    #ifdef FONLINE_SERVER
    memzero( FuncId, sizeof( FuncId ) );
    ViewByCritter = NULL;
    ChildItems = NULL;
    #endif

    SetProto( proto );
    SetCount( 1 );

    Id = id;
}

Item::~Item()
{
    MEMORY_PROCESS( MEMORY_ITEM, -(int) ( sizeof( Item ) + PropertiesRegistrator->GetWholeDataSize() ) );
}

void Item::SetProto( ProtoItem* proto )
{
    Proto = proto;
    Accessory = ITEM_ACCESSORY_NONE;
    SetSortValue( 0x7FFF );
    SetFlags( Proto->GetFlags() );
    SetIndicator( Proto->GetIndicatorStart() );

    SetVal0( Proto->GetStartValue_0() );
    SetVal1( Proto->GetStartValue_1() );
    SetVal2( Proto->GetStartValue_2() );
    SetVal3( Proto->GetStartValue_3() );
    SetVal4( Proto->GetStartValue_4() );
    SetVal5( Proto->GetStartValue_5() );
    SetVal6( Proto->GetStartValue_6() );
    SetVal7( Proto->GetStartValue_7() );
    SetVal8( Proto->GetStartValue_8() );
    SetVal9( Proto->GetStartValue_9() );

    switch( GetType() )
    {
    case ITEM_TYPE_WEAPON:
        SetAmmoCount( Proto->GetWeapon_MaxAmmoCount() );
        SetAmmoPid( Proto->GetWeapon_DefaultAmmoPid() );
        break;
    case ITEM_TYPE_DOOR:
    {
        uint flags = GetFlags();
        SETFLAG( flags, ITEM_GAG );
        if( !Proto->GetDoor_NoBlockMove() )
            UNSETFLAG( flags, ITEM_NO_BLOCK );
        if( !Proto->GetDoor_NoBlockShoot() )
            UNSETFLAG( flags, ITEM_SHOOT_THRU );
        if( !Proto->GetDoor_NoBlockLight() )
            UNSETFLAG( flags, ITEM_LIGHT_THRU );
        SetFlags( flags );
    }
        SetLockerCondition( Proto->GetLocker_Condition() );
        break;
    case  ITEM_TYPE_CONTAINER:
        SetLockerCondition( Proto->GetLocker_Condition() );
        break;
    default:
        break;
    }

    if( IsRadio() )
    {
        SetRadioChannel( Proto->GetRadioChannel() );
        SetRadioFlags( Proto->GetRadioFlags() );
        SetRadioBroadcastSend( Proto->GetRadioBroadcastSend() );
        SetRadioBroadcastRecv( Proto->GetRadioBroadcastRecv() );
    }

    if( IsHolodisk() )
    {
        SetHolodiskNumber( Proto->GetHolodiskNum() );
        #ifdef FONLINE_SERVER
        if( !GetHolodiskNumber() )
            SetHolodiskNumber( Random( 1, 42 ) );
        #endif
    }
}

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
Item* Item::Clone()
{
    Item* clone = new Item( Id, Proto );
    clone->Accessory = Accessory;
    memcpy( clone->AccBuffer, AccBuffer, sizeof( AccBuffer ) );
    clone->Props = Props;
    return clone;
}
#endif

#ifdef FONLINE_SERVER
void Item::FullClear()
{
    IsNotValid = true;
    Accessory = 0x20 + GetType();

    if( IsContainer() && ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, -(int) sizeof( ItemMap ) );

        ItemVec del_items = *ChildItems;
        ChildItems->clear();
        SAFEDEL( ChildItems );

        for( auto it = del_items.begin(), end = del_items.end(); it != end; ++it )
        {
            Item* item = *it;
            SYNC_LOCK( item );
            item->Accessory = 0xB1;
            ItemMngr.ItemToGarbage( item );
        }
    }
}

bool Item::ParseScript( const char* script, bool first_time )
{
    if( script && script[ 0 ] )
    {
        hash func_num = Script::BindScriptFuncNum( script, "void %s(Item&,bool)" );
        if( !func_num )
        {
            WriteLogF( _FUNC_, " - Script<%s> bind fail, item pid<%u>.\n", script, GetProtoId() );
            return false;
        }
        SetScriptId( func_num );
    }

    if( GetScriptId() && Script::PrepareScriptFuncContext( GetScriptId(), _FUNC_, Str::FormatBuf( "Item id<%u>, pid<%u>", GetId(), GetProtoId() ) ) )
    {
        Script::SetArgObject( this );
        Script::SetArgBool( first_time );
        Script::RunPrepared();
    }
    return true;
}

bool Item::PrepareScriptFunc( int num_scr_func )
{
    if( num_scr_func >= ITEM_EVENT_MAX )
        return false;
    if( FuncId[ num_scr_func ] <= 0 )
        return false;
    return Script::PrepareContext( FuncId[ num_scr_func ], _FUNC_, Str::FormatBuf( "Item id<%u>, pid<%u>", GetId(), GetProtoId() ) );
}

void Item::EventFinish( bool deleted )
{
    if( PrepareScriptFunc( ITEM_EVENT_FINISH ) )
    {
        Script::SetArgObject( this );
        Script::SetArgBool( deleted );
        Script::RunPrepared();
    }
}

bool Item::EventAttack( Critter* cr, Critter* target )
{
    bool result = false;
    if( PrepareScriptFunc( ITEM_EVENT_ATTACK ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgObject( target );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Item::EventUse( Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    bool result = false;
    if( PrepareScriptFunc( ITEM_EVENT_USE ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgObject( on_critter );
        Script::SetArgObject( on_item );
        Script::SetArgObject( on_scenery );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Item::EventUseOnMe( Critter* cr, Item* used_item )
{
    bool result = false;
    if( PrepareScriptFunc( ITEM_EVENT_USE_ON_ME ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgObject( used_item );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Item::EventSkill( Critter* cr, int skill )
{
    bool result = false;
    if( PrepareScriptFunc( ITEM_EVENT_SKILL ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgUInt( skill );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

void Item::EventDrop( Critter* cr )
{
    if( !PrepareScriptFunc( ITEM_EVENT_DROP ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Item::EventMove( Critter* cr, uchar from_slot )
{
    if( !PrepareScriptFunc( ITEM_EVENT_MOVE ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::SetArgUChar( from_slot );
    Script::RunPrepared();
}

void Item::EventWalk( Critter* cr, bool entered, uchar dir )
{
    if( !PrepareScriptFunc( ITEM_EVENT_WALK ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );   // Saved in Act_Move
    Script::SetArgBool( entered );
    Script::SetArgUChar( dir );
    Script::RunPrepared();
}
#endif // FONLINE_SERVER

void Item::SetSortValue( ItemVec& items )
{
    ushort sort_value = 0x7FFF;
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item == this )
            continue;
        if( sort_value >= item->GetSortValue() )
            sort_value = item->GetSortValue() - 1;
    }
    SetSortValue( sort_value );
}

bool SortItemsFunc( Item* l, Item* r ) { return l->GetSortValue() < r->GetSortValue(); }
void Item::SortItems( ItemVec& items )
{
    std::sort( items.begin(), items.end(), SortItemsFunc );
}

void Item::ClearItems( ItemVec& items )
{
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
        ( *it )->Release();
    items.clear();
}

void Item::ChangeCount( int val )
{
    SetCount( GetCount() + val );
}

#ifdef FONLINE_SERVER
void Item::Repair()
{
    if( FLAG( GetBrokenFlags(), BI_BROKEN ) )
    {
        uchar flags = GetBrokenFlags();
        UNSETFLAG( flags, BI_BROKEN );
        SetBrokenFlags( flags );
        SetDeterioration( 0 );
    }
}
#endif

void Item::SetWeaponMode( uchar mode )
{
    if( !IsWeapon() )
    {
        mode &= 0xF;
        if( mode == USE_USE && !IsCanUse() && !IsCanUseOnSmth() )
            mode = USE_NONE;
        else if( IsCanUse() || IsCanUseOnSmth() )
            mode = USE_USE;
        else
            mode = USE_NONE;
    }
    else
    {
        uchar use = ( mode & 0xF );
        uchar aim = ( mode >> 4 );

        switch( use )
        {
        case USE_PRIMARY:
            if( Proto->GetWeapon_ActiveUses() & 0x1 )
                break;
            use = 0xF;
            aim = 0;
            break;
        case USE_SECONDARY:
            if( Proto->GetWeapon_ActiveUses() & 0x2 )
                break;
            use = USE_PRIMARY;
            aim = 0;
            break;
        case USE_THIRD:
            if( Proto->GetWeapon_ActiveUses() & 0x4 )
                break;
            use = USE_PRIMARY;
            aim = 0;
            break;
        case USE_RELOAD:
            aim = 0;
            if( Proto->GetWeapon_MaxAmmoCount() )
                break;
            use = USE_PRIMARY;
            break;
        case USE_USE:
            aim = 0;
            if( IsCanUseOnSmth() )
                break;
            use = USE_PRIMARY;
            break;
        default:
            use = USE_PRIMARY;
            aim = 0;
            break;
        }

        if( use < MAX_USES && aim && !( use == 0 ? Proto->GetWeapon_Aim_0() : ( use == 1 ? Proto->GetWeapon_Aim_1() : Proto->GetWeapon_Aim_2() ) ) )
            aim = 0;
        mode = ( aim << 4 ) | ( use & 0xF );
    }

    SetMode( mode );
}

uint Item::GetCost1st()
{
    uint cost = ( GetCost() ? GetCost() : Proto->GetCost() );
    if( IsWeapon() && GetAmmoCount() )
    {
        ProtoItem* ammo = ItemMngr.GetProtoItem( GetAmmoPid() );
        if( ammo )
            cost += ammo->GetCost() * GetAmmoCount();
    }
    if( !cost )
        cost = 1;
    return cost;
}

void Item::WeapLoadHolder()
{
    if( !GetAmmoPid() )
        SetAmmoPid( Proto->GetWeapon_DefaultAmmoPid() );
    SetAmmoCount( (ushort) Proto->GetWeapon_MaxAmmoCount() );
}

#ifdef FONLINE_SERVER
void Item::ContAddItem( Item*& item, uint stack_id )
{
    if( !IsContainer() || !item )
        return;

    if( !ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemMap ) );
        ChildItems = new ItemVec();
        if( !ChildItems )
            return;
    }

    if( item->IsStackable() )
    {
        Item* item_ = ContGetItemByPid( item->GetProtoId(), stack_id );
        if( item_ )
        {
            item_->ChangeCount( item->GetCount() );
            ItemMngr.ItemToGarbage( item );
            item = item_;
            return;
        }
    }

    item->AccContainer.StackId = stack_id;
    item->SetSortValue( *ChildItems );
    ContSetItem( item );
}

void Item::ContSetItem( Item* item )
{
    if( !ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemMap ) );
        ChildItems = new ItemVec();
        if( !ChildItems )
            return;
    }

    if( std::find( ChildItems->begin(), ChildItems->end(), item ) != ChildItems->end() )
    {
        WriteLogF( _FUNC_, " - Item already added!\n" );
        return;
    }

    ChildItems->push_back( item );
    item->Accessory = ITEM_ACCESSORY_CONTAINER;
    item->AccContainer.ContainerId = GetId();
}

void Item::ContEraseItem( Item* item )
{
    if( !IsContainer() )
    {
        WriteLogF( _FUNC_, " - Item is not container.\n" );
        return;
    }
    if( !ChildItems )
    {
        WriteLogF( _FUNC_, " - Container items null ptr.\n" );
        return;
    }
    if( !item )
    {
        WriteLogF( _FUNC_, " - Item null ptr.\n" );
        return;
    }

    auto it = std::find( ChildItems->begin(), ChildItems->end(), item );
    if( it != ChildItems->end() )
        ChildItems->erase( it );
    else
        WriteLogF( _FUNC_, " - Item not found, id<%u>, pid<%u>, container<%u>.\n", item->GetId(), item->GetProtoId(), GetId() );

    item->Accessory = 0xd3;

    if( ChildItems->empty() )
        SAFEDEL( ChildItems );
}

Item* Item::ContGetItem( uint item_id, bool skip_hide )
{
    if( !IsContainer() || !ChildItems || !item_id )
        return NULL;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == item_id )
        {
            if( skip_hide && item->IsHidden() )
                return NULL;
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

void Item::ContGetAllItems( ItemVec& items, bool skip_hide, bool sync_lock )
{
    if( !IsContainer() || !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( !skip_hide || !item->IsHidden() )
            items.push_back( item );
    }

    # pragma MESSAGE("Recheck after synchronization.")
    if( sync_lock && LogicMT )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

# pragma MESSAGE("Add explicit sync lock.")
Item* Item::ContGetItemByPid( hash pid, uint stack_id )
{
    if( !IsContainer() || !ChildItems )
        return NULL;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == pid && ( stack_id == uint( -1 ) || item->AccContainer.StackId == stack_id ) )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

void Item::ContGetItems( ItemVec& items, uint stack_id, bool sync_lock )
{
    if( !IsContainer() || !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( stack_id == uint( -1 ) || item->AccContainer.StackId == stack_id )
            items.push_back( item );
    }

    # pragma MESSAGE("Recheck after synchronization.")
    if( sync_lock && LogicMT )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

int Item::ContGetFreeVolume( uint stack_id )
{
    if( !IsContainer() )
        return 0;

    int cur_volume = 0;
    int max_volume = Proto->GetContainer_Volume();
    if( !ChildItems )
        return max_volume;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( stack_id == uint( -1 ) || item->AccContainer.StackId == stack_id )
            cur_volume += item->GetVolume();
    }
    return max_volume - cur_volume;
}

bool Item::ContIsItems()
{
    return ChildItems && ChildItems->size();
}

Item* Item::GetChild( uint child_index )
{
    hash pid = ( child_index == 0 ? Proto->GetChildPid_0() : ( child_index == 1 ? Proto->GetChildPid_1() : Proto->GetChildPid_2() ) );
    if( !pid )
        return NULL;

    if( Accessory == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( AccHex.MapId );
        if( !map )
            return NULL;
        return map->GetItemChild( AccHex.HexX, AccHex.HexY, Proto, child_index );
    }
    else if( Accessory == ITEM_ACCESSORY_CRITTER )
    {
        Critter* cr = CrMngr.GetCritter( AccCritter.Id, true );
        if( cr )
            return cr->GetItemByPid( pid );
    }
    else if( Accessory == ITEM_ACCESSORY_CONTAINER )
    {
        Item* cont = ItemMngr.GetItem( AccContainer.ContainerId, true );
        if( cont )
            return cont->ContGetItemByPid( pid, AccContainer.StackId );
    }
    return NULL;
}
#endif // FONLINE_SERVER

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
# include "ResourceManager.h"
# include "SpriteManager.h"

uint ProtoItem::GetCurSprId()
{
    AnyFrames* anim = ResMngr.GetItemAnim( GetPicMap() );
    if( !anim )
        return 0;

    uint beg = 0, end = 0;
    if( FLAG( GetFlags(), ITEM_SHOW_ANIM ) )
        end = anim->CntFrm - 1;
    if( FLAG( GetFlags(), ITEM_SHOW_ANIM_EXT ) )
    {
        beg = GetAnimStay_0();
        end = GetAnimStay_1();
    }

    if( beg >= anim->CntFrm )
        beg = anim->CntFrm - 1;
    if( end >= anim->CntFrm )
        end = anim->CntFrm - 1;
    if( beg > end )
        std::swap( beg, end );
    uint count = end - beg + 1;
    uint ticks = anim->Ticks / anim->CntFrm * count;
    return anim->Ind[ beg + ( ( Timer::GameTick() % ticks ) * 100 / ticks ) * count / 100 ];
}

uint Item::GetCurSprId()
{
    AnyFrames* anim = ResMngr.GetItemAnim( GetActualPicMap() );
    if( !anim )
        return 0;

    uint beg = 0, end = 0;
    if( IsShowAnim() )
        end = anim->CntFrm - 1;
    if( IsShowAnimExt() )
    {
        beg = Proto->GetAnimStay_0();
        end = Proto->GetAnimStay_1();
    }

    if( beg >= anim->CntFrm )
        beg = anim->CntFrm - 1;
    if( end >= anim->CntFrm )
        end = anim->CntFrm - 1;
    if( beg > end )
        std::swap( beg, end );
    uint count = end - beg + 1;
    uint ticks = anim->Ticks / anim->CntFrm * count;
    return anim->Ind[ beg + ( ( Timer::GameTick() % ticks ) * 100 / ticks ) * count / 100 ];
}
#endif
