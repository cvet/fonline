#include "Common.h"
#include "Item.h"
#include "ProtoManager.h"

#ifdef FONLINE_SERVER
# include "Script.h"
# include "MapManager.h"
# include "CritterManager.h"
# include "ItemManager.h"
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

CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, int, Type );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, int, Grid_Type );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, hash, PicMap );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, hash, PicInv );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, Stackable );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, Deteriorable );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, Weapon_IsUnarmed );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uint, Weapon_Anim1 );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, short, OffsetX );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, short, OffsetY );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, Slot );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, char, LightIntensity );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, LightDistance );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, LightFlags );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uint, LightColor );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsCanPickUp );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uint, Count );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsFlat );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, char, DrawOrderOffsetHexY );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, int, Corner );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, DisableEgg );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsBadItem );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsColorize );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsShowAnim );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsShowAnimExt );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, AnimStay_0 );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, AnimStay_1 );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, ScriptString *, BlockLines );

ProtoItem::ProtoItem( hash pid ): ProtoEntity( pid, Item::PropertiesRegistrator )
{
    InstanceCount = 0;
}

PROPERTIES_IMPL( Item );
CLASS_PROPERTY_IMPL( Item, TriggerNum );
CLASS_PROPERTY_IMPL( Item, PicMap );
CLASS_PROPERTY_IMPL( Item, PicInv );
CLASS_PROPERTY_IMPL( Item, OffsetX );
CLASS_PROPERTY_IMPL( Item, OffsetY );
CLASS_PROPERTY_IMPL( Item, Cost );
CLASS_PROPERTY_IMPL( Item, LightIntensity );
CLASS_PROPERTY_IMPL( Item, LightDistance );
CLASS_PROPERTY_IMPL( Item, LightFlags );
CLASS_PROPERTY_IMPL( Item, LightColor );
CLASS_PROPERTY_IMPL( Item, Type );
CLASS_PROPERTY_IMPL( Item, Stackable );
CLASS_PROPERTY_IMPL( Item, Deteriorable );
CLASS_PROPERTY_IMPL( Item, GroundLevel );
CLASS_PROPERTY_IMPL( Item, Corner );
CLASS_PROPERTY_IMPL( Item, Slot );
CLASS_PROPERTY_IMPL( Item, Weight );
CLASS_PROPERTY_IMPL( Item, Volume );
CLASS_PROPERTY_IMPL( Item, SoundId );
CLASS_PROPERTY_IMPL( Item, Material );
CLASS_PROPERTY_IMPL( Item, DisableEgg );
CLASS_PROPERTY_IMPL( Item, AnimWaitBase );
CLASS_PROPERTY_IMPL( Item, AnimWaitRndMin );
CLASS_PROPERTY_IMPL( Item, AnimWaitRndMax );
CLASS_PROPERTY_IMPL( Item, AnimStay_0 );
CLASS_PROPERTY_IMPL( Item, AnimStay_1 );
CLASS_PROPERTY_IMPL( Item, AnimShow_0 );
CLASS_PROPERTY_IMPL( Item, AnimShow_1 );
CLASS_PROPERTY_IMPL( Item, AnimHide_0 );
CLASS_PROPERTY_IMPL( Item, AnimHide_1 );
CLASS_PROPERTY_IMPL( Item, SpriteCut );
CLASS_PROPERTY_IMPL( Item, DrawOrderOffsetHexY );
CLASS_PROPERTY_IMPL( Item, IndicatorMax );
CLASS_PROPERTY_IMPL( Item, BlockLines );
CLASS_PROPERTY_IMPL( Item, ChildPid_0 );
CLASS_PROPERTY_IMPL( Item, ChildPid_1 );
CLASS_PROPERTY_IMPL( Item, ChildPid_2 );
CLASS_PROPERTY_IMPL( Item, ChildPid_3 );
CLASS_PROPERTY_IMPL( Item, ChildPid_4 );
CLASS_PROPERTY_IMPL( Item, ChildLines_0 );
CLASS_PROPERTY_IMPL( Item, ChildLines_1 );
CLASS_PROPERTY_IMPL( Item, ChildLines_2 );
CLASS_PROPERTY_IMPL( Item, ChildLines_3 );
CLASS_PROPERTY_IMPL( Item, ChildLines_4 );
CLASS_PROPERTY_IMPL( Item, Weapon_IsUnarmed );
CLASS_PROPERTY_IMPL( Item, Weapon_UnarmedMinAgility );
CLASS_PROPERTY_IMPL( Item, Weapon_UnarmedMinUnarmed );
CLASS_PROPERTY_IMPL( Item, Weapon_UnarmedMinLevel );
CLASS_PROPERTY_IMPL( Item, Weapon_Anim1 );
CLASS_PROPERTY_IMPL( Item, Weapon_MaxAmmoCount );
CLASS_PROPERTY_IMPL( Item, Weapon_Caliber );
CLASS_PROPERTY_IMPL( Item, Weapon_DefaultAmmoPid );
CLASS_PROPERTY_IMPL( Item, Weapon_MinStrength );
CLASS_PROPERTY_IMPL( Item, Weapon_Perk );
CLASS_PROPERTY_IMPL( Item, Weapon_IsTwoHanded );
CLASS_PROPERTY_IMPL( Item, Weapon_ActiveUses );
CLASS_PROPERTY_IMPL( Item, Weapon_Skill_0 );
CLASS_PROPERTY_IMPL( Item, Weapon_Skill_1 );
CLASS_PROPERTY_IMPL( Item, Weapon_Skill_2 );
CLASS_PROPERTY_IMPL( Item, Weapon_PicUse_0 );
CLASS_PROPERTY_IMPL( Item, Weapon_PicUse_1 );
CLASS_PROPERTY_IMPL( Item, Weapon_PicUse_2 );
CLASS_PROPERTY_IMPL( Item, Weapon_MaxDist_0 );
CLASS_PROPERTY_IMPL( Item, Weapon_MaxDist_1 );
CLASS_PROPERTY_IMPL( Item, Weapon_MaxDist_2 );
CLASS_PROPERTY_IMPL( Item, Weapon_Round_0 );
CLASS_PROPERTY_IMPL( Item, Weapon_Round_1 );
CLASS_PROPERTY_IMPL( Item, Weapon_Round_2 );
CLASS_PROPERTY_IMPL( Item, Weapon_ApCost_0 );
CLASS_PROPERTY_IMPL( Item, Weapon_ApCost_1 );
CLASS_PROPERTY_IMPL( Item, Weapon_ApCost_2 );
CLASS_PROPERTY_IMPL( Item, Weapon_Aim_0 );
CLASS_PROPERTY_IMPL( Item, Weapon_Aim_1 );
CLASS_PROPERTY_IMPL( Item, Weapon_Aim_2 );
CLASS_PROPERTY_IMPL( Item, Weapon_SoundId_0 );
CLASS_PROPERTY_IMPL( Item, Weapon_SoundId_1 );
CLASS_PROPERTY_IMPL( Item, Weapon_SoundId_2 );
CLASS_PROPERTY_IMPL( Item, Ammo_Caliber );
CLASS_PROPERTY_IMPL( Item, Door_NoBlockMove );
CLASS_PROPERTY_IMPL( Item, Door_NoBlockShoot );
CLASS_PROPERTY_IMPL( Item, Door_NoBlockLight );
CLASS_PROPERTY_IMPL( Item, Container_Volume );
CLASS_PROPERTY_IMPL( Item, Container_Changeble );
CLASS_PROPERTY_IMPL( Item, Container_CannotPickUp );
CLASS_PROPERTY_IMPL( Item, Container_MagicHandsGrnd );
CLASS_PROPERTY_IMPL( Item, Locker_Condition );
CLASS_PROPERTY_IMPL( Item, Grid_Type );
CLASS_PROPERTY_IMPL( Item, Grid_ToMap );
CLASS_PROPERTY_IMPL( Item, Grid_ToMapEntire );
CLASS_PROPERTY_IMPL( Item, Grid_ToMapDir );
CLASS_PROPERTY_IMPL( Item, Car_Speed );
CLASS_PROPERTY_IMPL( Item, Car_Passability );
CLASS_PROPERTY_IMPL( Item, Car_DeteriorationRate );
CLASS_PROPERTY_IMPL( Item, Car_CrittersCapacity );
CLASS_PROPERTY_IMPL( Item, Car_TankVolume );
CLASS_PROPERTY_IMPL( Item, Car_MaxDeterioration );
CLASS_PROPERTY_IMPL( Item, Car_FuelConsumption );
CLASS_PROPERTY_IMPL( Item, Car_Entrance );
CLASS_PROPERTY_IMPL( Item, Car_MovementType );
CLASS_PROPERTY_IMPL( Item, SceneryParams );
CLASS_PROPERTY_IMPL( Item, ScriptId );
CLASS_PROPERTY_IMPL( Item, Accessory );
CLASS_PROPERTY_IMPL( Item, MapId );
CLASS_PROPERTY_IMPL( Item, HexX );
CLASS_PROPERTY_IMPL( Item, HexY );
CLASS_PROPERTY_IMPL( Item, CritId );
CLASS_PROPERTY_IMPL( Item, CritSlot );
CLASS_PROPERTY_IMPL( Item, ContainerId );
CLASS_PROPERTY_IMPL( Item, ContainerStack );
CLASS_PROPERTY_IMPL( Item, IsHidden );
CLASS_PROPERTY_IMPL( Item, IsFlat );
CLASS_PROPERTY_IMPL( Item, IsNoBlock );
CLASS_PROPERTY_IMPL( Item, IsShootThru );
CLASS_PROPERTY_IMPL( Item, IsLightThru );
CLASS_PROPERTY_IMPL( Item, IsMultiHex );
CLASS_PROPERTY_IMPL( Item, IsWallTransEnd );
CLASS_PROPERTY_IMPL( Item, IsBigGun );
CLASS_PROPERTY_IMPL( Item, IsAlwaysView );
CLASS_PROPERTY_IMPL( Item, IsHasTimer );
CLASS_PROPERTY_IMPL( Item, IsBadItem );
CLASS_PROPERTY_IMPL( Item, IsNoHighlight );
CLASS_PROPERTY_IMPL( Item, IsShowAnim );
CLASS_PROPERTY_IMPL( Item, IsShowAnimExt );
CLASS_PROPERTY_IMPL( Item, IsLight );
CLASS_PROPERTY_IMPL( Item, IsGeck );
CLASS_PROPERTY_IMPL( Item, IsTrap );
CLASS_PROPERTY_IMPL( Item, IsNoLightInfluence );
CLASS_PROPERTY_IMPL( Item, IsNoLoot );
CLASS_PROPERTY_IMPL( Item, IsNoSteal );
CLASS_PROPERTY_IMPL( Item, IsGag );
CLASS_PROPERTY_IMPL( Item, IsColorize );
CLASS_PROPERTY_IMPL( Item, IsColorizeInv );
CLASS_PROPERTY_IMPL( Item, IsCanUseOnSmth );
CLASS_PROPERTY_IMPL( Item, IsCanLook );
CLASS_PROPERTY_IMPL( Item, IsCanTalk );
CLASS_PROPERTY_IMPL( Item, IsCanPickUp );
CLASS_PROPERTY_IMPL( Item, IsCanUse );
CLASS_PROPERTY_IMPL( Item, IsHolodisk );
CLASS_PROPERTY_IMPL( Item, IsRadio );
CLASS_PROPERTY_IMPL( Item, SortValue );
CLASS_PROPERTY_IMPL( Item, Indicator );
CLASS_PROPERTY_IMPL( Item, Mode );
CLASS_PROPERTY_IMPL( Item, Count );
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
CLASS_PROPERTY_IMPL( Item, LockerComplexity );
CLASS_PROPERTY_IMPL( Item, HolodiskNum );
CLASS_PROPERTY_IMPL( Item, RadioChannel );
CLASS_PROPERTY_IMPL( Item, RadioFlags );
CLASS_PROPERTY_IMPL( Item, RadioBroadcastSend );
CLASS_PROPERTY_IMPL( Item, RadioBroadcastRecv );

Item::Item( uint id, ProtoItem* proto ): Entity( id, EntityType::Item, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );

    MEMORY_PROCESS( MEMORY_ITEM, sizeof( Item ) + PropertiesRegistrator->GetWholeDataSize() );

    ChildItems = nullptr;
    ViewPlaceOnMap = false;

    #ifdef FONLINE_SERVER
    memzero( FuncId, sizeof( FuncId ) );
    SceneryScriptBindId = 0;
    ViewByCritter = nullptr;
    #endif

    if( GetCount() == 0 )
        SetCount( 1 );

    switch( GetType() )
    {
    case ITEM_TYPE_WEAPON:
        SetAmmoCount( GetWeapon_MaxAmmoCount() );
        SetAmmoPid( Str::GetHash( ConvertProtoIdByInt( GetWeapon_DefaultAmmoPid() ) ) );
        break;
    case ITEM_TYPE_DOOR:
        SetIsGag( true );
        if( !GetDoor_NoBlockMove() )
            SetIsNoBlock( false );
        if( !GetDoor_NoBlockShoot() )
            SetIsShootThru( false );
        if( !GetDoor_NoBlockLight() )
            SetIsLightThru( false );
        SetLockerCondition( GetLocker_Condition() );
        break;
    case  ITEM_TYPE_CONTAINER:
        SetLockerCondition( GetLocker_Condition() );
        break;
    default:
        break;
    }
}

Item::~Item()
{
    MEMORY_PROCESS( MEMORY_ITEM, -(int) ( sizeof( Item ) + PropertiesRegistrator->GetWholeDataSize() ) );
}

void Item::SetProto( ProtoItem* proto )
{
    RUNTIME_ASSERT( proto );
    Proto->Release();
    Proto = proto;
    Props = proto->Props;
}

hash Item::GetChildPid( uint index )
{
    if( index == 0 )
        return GetChildPid_0();
    else if( index == 1 )
        return GetChildPid_1();
    else if( index == 2 )
        return GetChildPid_2();
    else if( index == 3 )
        return GetChildPid_3();
    else if( index == 4 )
        return GetChildPid_4();
    return 0;
}

ScriptString* Item::GetChildLinesStr( uint index )
{
    ScriptString* result = nullptr;
    if( index == 0 )
        result = GetChildLines_0();
    else if( index == 1 )
        result = GetChildLines_1();
    else if( index == 2 )
        result = GetChildLines_2();
    else if( index == 3 )
        result = GetChildLines_3();
    else if( index == 4 )
        result = GetChildLines_4();
    return result;
}

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
Item* Item::Clone()
{
    RUNTIME_ASSERT( Type == EntityType::Item );
    Item* clone = new Item( Id, (ProtoItem*) Proto );
    clone->Props = Props;
    return clone;
}
#endif

#ifdef FONLINE_SERVER
bool Item::SetScript( const char* script_name, bool first_time )
{
    if( script_name && script_name[ 0 ] )
    {
        hash func_num = Script::BindScriptFuncNumByScriptName( script_name, "void %s(Item&,bool)" );
        if( !func_num )
        {
            WriteLogF( _FUNC_, " - Script '%s' bind fail, item '%s'.\n", script_name, GetName() );
            return false;
        }
        SetScriptId( func_num );
    }

    if( GetScriptId() && Script::PrepareScriptFuncContext( GetScriptId(), _FUNC_, Str::FormatBuf( "Item '%s' (%u)", GetName(), GetId() ) ) )
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
    if( !FuncId[ num_scr_func ] )
        return false;
    return Script::PrepareContext( FuncId[ num_scr_func ], _FUNC_, Str::FormatBuf( "Item '%s' (%u)", GetName(), GetId() ) );
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

bool Item::EventUse( Critter* cr, Critter* on_critter, Item* on_item, Item* on_scenery )
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
    short sort_value = 0;
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
        if( mode == USE_USE && !GetIsCanUse() && !GetIsCanUseOnSmth() )
            mode = USE_NONE;
        else if( GetIsCanUse() || GetIsCanUseOnSmth() )
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
            if( GetWeapon_ActiveUses() & 0x1 )
                break;
            use = 0xF;
            aim = 0;
            break;
        case USE_SECONDARY:
            if( GetWeapon_ActiveUses() & 0x2 )
                break;
            use = USE_PRIMARY;
            aim = 0;
            break;
        case USE_THIRD:
            if( GetWeapon_ActiveUses() & 0x4 )
                break;
            use = USE_PRIMARY;
            aim = 0;
            break;
        case USE_RELOAD:
            aim = 0;
            if( GetWeapon_MaxAmmoCount() )
                break;
            use = USE_PRIMARY;
            break;
        case USE_USE:
            aim = 0;
            if( GetIsCanUseOnSmth() )
                break;
            use = USE_PRIMARY;
            break;
        default:
            use = USE_PRIMARY;
            aim = 0;
            break;
        }

        if( use < MAX_USES && aim && !( use == 0 ? GetWeapon_Aim_0() : ( use == 1 ? GetWeapon_Aim_1() : GetWeapon_Aim_2() ) ) )
            aim = 0;
        mode = ( aim << 4 ) | ( use & 0xF );
    }

    SetMode( mode );
}

uint Item::GetCost1st()
{
    uint cost = ( GetCost() ? GetCost() : GetCost() );
    if( IsWeapon() && GetAmmoCount() )
    {
        ProtoItem* ammo = ProtoMngr.GetProtoItem( GetAmmoPid() );
        if( ammo )
            cost += ammo->Props.GetPropValue< uint >( Item::PropertyCost ) * GetAmmoCount();
    }
    if( !cost )
        cost = 1;
    return cost;
}

void Item::WeapLoadHolder()
{
    if( !GetAmmoPid() )
        SetAmmoPid( GetWeapon_DefaultAmmoPid() );
    SetAmmoCount( (ushort) GetWeapon_MaxAmmoCount() );
}

#ifdef FONLINE_SERVER
void Item::ContAddItem( Item*& item, uint stack_id )
{
    RUNTIME_ASSERT( item );

    if( !ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemMap ) );
        ChildItems = new ItemVec();
    }

    if( item->IsStackable() )
    {
        Item* item_ = ContGetItemByPid( item->GetProtoId(), stack_id );
        if( item_ )
        {
            item_->ChangeCount( item->GetCount() );
            ItemMngr.DeleteItem( item );
            item = item_;
            return;
        }
    }

    item->SetContainerStack( stack_id );
    item->SetSortValue( *ChildItems );
    ContSetItem( item );
}

void Item::ContSetItem( Item* item )
{
    if( !ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemMap ) );
        ChildItems = new ItemVec();
    }

    RUNTIME_ASSERT( std::find( ChildItems->begin(), ChildItems->end(), item ) == ChildItems->end() );

    ChildItems->push_back( item );
    item->SetAccessory( ITEM_ACCESSORY_CONTAINER );
    item->SetContainerId( Id );
}

void Item::ContEraseItem( Item* item )
{
    RUNTIME_ASSERT( ChildItems );
    RUNTIME_ASSERT( item );

    auto it = std::find( ChildItems->begin(), ChildItems->end(), item );
    RUNTIME_ASSERT( it != ChildItems->end() );
    ChildItems->erase( it );

    item->SetAccessory( ITEM_ACCESSORY_NONE );
    item->SetContainerId( 0 );
    item->SetContainerStack( 0 );

    if( ChildItems->empty() )
        SAFEDEL( ChildItems );
}

Item* Item::ContGetItem( uint item_id, bool skip_hide )
{
    RUNTIME_ASSERT( item_id );

    if( !ChildItems )
        return nullptr;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == item_id )
        {
            if( skip_hide && item->GetIsHidden() )
                return nullptr;
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

void Item::ContGetAllItems( ItemVec& items, bool skip_hide, bool sync_lock )
{
    if( !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( !skip_hide || !item->GetIsHidden() )
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
    if( !ChildItems )
        return nullptr;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == pid && ( stack_id == uint( -1 ) || item->GetContainerStack() == stack_id ) )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return nullptr;
}

void Item::ContGetItems( ItemVec& items, uint stack_id, bool sync_lock )
{
    if( !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( stack_id == uint( -1 ) || item->GetContainerStack() == stack_id )
            items.push_back( item );
    }

    # pragma MESSAGE("Recheck after synchronization.")
    if( sync_lock && LogicMT )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

bool Item::ContHaveFreeVolume( uint stack_id, uint volume )
{
    uint max_volume = GetContainer_Volume();
    if( max_volume == 0 )
        return true;

    if( volume > max_volume )
        return false;

    uint cur_volume = 0;
    if( ChildItems )
    {
        for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
        {
            Item* item = *it;
            if( stack_id == uint( -1 ) || item->GetContainerStack() == stack_id )
                cur_volume += item->GetWholeVolume();
        }
    }
    return max_volume >= cur_volume + volume;
}

bool Item::ContIsItems()
{
    return ChildItems && ChildItems->size();
}

void Item::ContDeleteItems()
{
    while( ChildItems )
    {
        RUNTIME_ASSERT( !ChildItems->empty() );
        ItemMngr.DeleteItem( *ChildItems->begin() );
    }
}

Item* Item::GetChild( uint child_index )
{
    hash pid = ( child_index == 0 ? GetChildPid_0() : ( child_index == 1 ? GetChildPid_1() : GetChildPid_2() ) );
    if( !pid )
        return nullptr;

    if( GetAccessory() == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( GetMapId() );
        if( !map )
            return nullptr;
        return map->GetItemChild( GetHexX(), GetHexY(), this, child_index );
    }
    else if( GetAccessory() == ITEM_ACCESSORY_CRITTER )
    {
        Critter* cr = CrMngr.GetCritter( GetCritId(), true );
        if( cr )
            return cr->GetItemByPid( pid );
    }
    else if( GetAccessory() == ITEM_ACCESSORY_CONTAINER )
    {
        Item* cont = ItemMngr.GetItem( GetContainerId(), true );
        if( cont )
            return cont->ContGetItemByPid( pid, GetContainerStack() );
    }
    return nullptr;
}

#else

void Item::ContSetItem( Item* item )
{
    if( !ChildItems )
        ChildItems = new ItemVec();

    RUNTIME_ASSERT( std::find( ChildItems->begin(), ChildItems->end(), item ) == ChildItems->end() );

    ChildItems->push_back( item );
    item->SetAccessory( ITEM_ACCESSORY_CONTAINER );
    item->SetContainerId( Id );
}

void Item::ContEraseItem( Item* item )
{
    RUNTIME_ASSERT( ChildItems );
    RUNTIME_ASSERT( item );

    auto it = std::find( ChildItems->begin(), ChildItems->end(), item );
    RUNTIME_ASSERT( it != ChildItems->end() );
    ChildItems->erase( it );

    item->SetAccessory( ITEM_ACCESSORY_NONE );
    item->SetContainerId( 0 );
    item->SetContainerStack( 0 );

    if( ChildItems->empty() )
        SAFEDEL( ChildItems );
}

void Item::ContGetItems( ItemVec& items, uint stack_id )
{
    if( !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( stack_id == uint( -1 ) || item->GetContainerStack() == stack_id )
            items.push_back( item );
    }
}
#endif // FONLINE_SERVER

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
# include "ResourceManager.h"
# include "SpriteManager.h"

uint Item::GetCurSprId()
{
    AnyFrames* anim = ResMngr.GetItemAnim( GetPicMap() );
    if( !anim )
        return 0;

    uint beg = 0, end = 0;
    if( GetIsShowAnim() )
    {
        beg = 0;
        end = anim->CntFrm - 1;
    }
    if( GetIsShowAnimExt() )
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

uint ProtoItem::GetCurSprId()
{
    AnyFrames* anim = ResMngr.GetItemAnim( GetPicMap() );
    if( !anim )
        return 0;

    uint beg = 0, end = 0;
    if( GetIsShowAnim() )
    {
        beg = 0;
        end = anim->CntFrm - 1;
    }
    if( GetIsShowAnimExt() )
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
#endif
