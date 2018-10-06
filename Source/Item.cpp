#include "Common.h"
#include "Item.h"
#include "ProtoManager.h"

#ifdef FONLINE_SERVER
# include "Script.h"
# include "MapManager.h"
# include "CritterManager.h"
# include "ItemManager.h"
#endif

CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, hash, PicMap );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, hash, PicInv );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, Stackable );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, short, OffsetX );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, short, OffsetY );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, Slot );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, char, LightIntensity );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, LightDistance );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, LightFlags );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uint, LightColor );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uint, Count );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsFlat );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, char, DrawOrderOffsetHexY );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, int, Corner );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, DisableEgg );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsBadItem );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsColorize );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsShowAnim );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, bool, IsShowAnimExt );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, AnimStay0 );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, uchar, AnimStay1 );
CLASS_PROPERTY_ALIAS_IMPL( ProtoItem, Item, CScriptArray *, BlockLines );

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
CLASS_PROPERTY_IMPL( Item, LightIntensity );
CLASS_PROPERTY_IMPL( Item, LightDistance );
CLASS_PROPERTY_IMPL( Item, LightFlags );
CLASS_PROPERTY_IMPL( Item, LightColor );
CLASS_PROPERTY_IMPL( Item, Stackable );
CLASS_PROPERTY_IMPL( Item, Opened );
CLASS_PROPERTY_IMPL( Item, Corner );
CLASS_PROPERTY_IMPL( Item, Slot );
CLASS_PROPERTY_IMPL( Item, DisableEgg );
CLASS_PROPERTY_IMPL( Item, AnimWaitBase );
CLASS_PROPERTY_IMPL( Item, AnimWaitRndMin );
CLASS_PROPERTY_IMPL( Item, AnimWaitRndMax );
CLASS_PROPERTY_IMPL( Item, AnimStay0 );
CLASS_PROPERTY_IMPL( Item, AnimStay1 );
CLASS_PROPERTY_IMPL( Item, AnimShow0 );
CLASS_PROPERTY_IMPL( Item, AnimShow1 );
CLASS_PROPERTY_IMPL( Item, AnimHide0 );
CLASS_PROPERTY_IMPL( Item, AnimHide1 );
CLASS_PROPERTY_IMPL( Item, SpriteCut );
CLASS_PROPERTY_IMPL( Item, DrawOrderOffsetHexY );
CLASS_PROPERTY_IMPL( Item, BlockLines );
CLASS_PROPERTY_IMPL( Item, Weapon_Anim1 );
CLASS_PROPERTY_IMPL( Item, Grid_ToMap );
CLASS_PROPERTY_IMPL( Item, Grid_ToMapEntire );
CLASS_PROPERTY_IMPL( Item, Grid_ToMapDir );
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
CLASS_PROPERTY_IMPL( Item, IsAlwaysView );
CLASS_PROPERTY_IMPL( Item, IsBadItem );
CLASS_PROPERTY_IMPL( Item, IsNoHighlight );
CLASS_PROPERTY_IMPL( Item, IsShowAnim );
CLASS_PROPERTY_IMPL( Item, IsShowAnimExt );
CLASS_PROPERTY_IMPL( Item, IsLight );
CLASS_PROPERTY_IMPL( Item, IsGeck );
CLASS_PROPERTY_IMPL( Item, IsTrap );
CLASS_PROPERTY_IMPL( Item, IsNoLightInfluence );
CLASS_PROPERTY_IMPL( Item, IsGag );
CLASS_PROPERTY_IMPL( Item, IsColorize );
CLASS_PROPERTY_IMPL( Item, IsColorizeInv );
CLASS_PROPERTY_IMPL( Item, IsCanTalk );
CLASS_PROPERTY_IMPL( Item, IsRadio );
CLASS_PROPERTY_IMPL( Item, SortValue );
CLASS_PROPERTY_IMPL( Item, Mode );
CLASS_PROPERTY_IMPL( Item, Count );
CLASS_PROPERTY_IMPL( Item, TrapValue );
CLASS_PROPERTY_IMPL( Item, RadioChannel );
CLASS_PROPERTY_IMPL( Item, RadioFlags );
CLASS_PROPERTY_IMPL( Item, RadioBroadcastSend );
CLASS_PROPERTY_IMPL( Item, RadioBroadcastRecv );
CLASS_PROPERTY_IMPL( Item, FlyEffectSpeed );

Item::Item( uint id, ProtoItem* proto ): Entity( id, EntityType::Item, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );

    MEMORY_PROCESS( MEMORY_ITEM, sizeof( Item ) + PropertiesRegistrator->GetWholeDataSize() );

    ChildItems = nullptr;
    ViewPlaceOnMap = false;

    #ifdef FONLINE_SERVER
    SceneryScriptBindId = 0;
    ViewByCritter = nullptr;
    #endif

    if( GetCount() == 0 )
        SetCount( 1 );
}

Item::~Item()
{
    MEMORY_PROCESS( MEMORY_ITEM, -(int) ( sizeof( Item ) + PropertiesRegistrator->GetWholeDataSize() ) );
}

void Item::SetProto( ProtoItem* proto )
{
    RUNTIME_ASSERT( proto );
    proto->AddRef();
    Proto->Release();
    Proto = proto;
    Props = proto->Props;
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
bool Item::SetScript( asIScriptFunction* func, bool first_time )
{
    if( func )
    {
        hash func_num = Script::BindScriptFuncNumByFunc( func );
        if( !func_num )
        {
            WriteLog( "Script bind fail, item '{}'.\n", GetName() );
            return false;
        }
        SetScriptId( func_num );
    }

    if( GetScriptId() )
    {
        Script::PrepareScriptFuncContext( GetScriptId(), _str( "Item '{}' ({})", GetName(), GetId() ) );
        Script::SetArgEntity( this );
        Script::SetArgBool( first_time );
        Script::RunPrepared();
    }
    return true;
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
void Item::ContAddItem( Item*& item, uint stack_id )
{
    RUNTIME_ASSERT( item );

    if( !ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemMap ) );
        ChildItems = new ItemVec();
    }

    if( item->GetStackable() )
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
            return item;
        }
    }
    return nullptr;
}

void Item::ContGetAllItems( ItemVec& items, bool skip_hide )
{
    if( !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( !skip_hide || !item->GetIsHidden() )
            items.push_back( item );
    }
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
            return item;
    }
    return nullptr;
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
        beg = GetAnimStay0();
        end = GetAnimStay1();
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
        beg = GetAnimStay0();
        end = GetAnimStay1();
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
