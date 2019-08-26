#include "ItemCl.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "SpriteManager.h"

PROPERTIES_IMPL( ItemCl );
CLASS_PROPERTY_IMPL( ItemCl, PicMap );
CLASS_PROPERTY_IMPL( ItemCl, PicInv );
CLASS_PROPERTY_IMPL( ItemCl, OffsetX );
CLASS_PROPERTY_IMPL( ItemCl, OffsetY );
CLASS_PROPERTY_IMPL( ItemCl, LightIntensity );
CLASS_PROPERTY_IMPL( ItemCl, LightDistance );
CLASS_PROPERTY_IMPL( ItemCl, LightFlags );
CLASS_PROPERTY_IMPL( ItemCl, LightColor );
CLASS_PROPERTY_IMPL( ItemCl, Stackable );
CLASS_PROPERTY_IMPL( ItemCl, Opened );
CLASS_PROPERTY_IMPL( ItemCl, Corner );
CLASS_PROPERTY_IMPL( ItemCl, Slot );
CLASS_PROPERTY_IMPL( ItemCl, DisableEgg );
CLASS_PROPERTY_IMPL( ItemCl, AnimWaitBase );
CLASS_PROPERTY_IMPL( ItemCl, AnimWaitRndMin );
CLASS_PROPERTY_IMPL( ItemCl, AnimWaitRndMax );
CLASS_PROPERTY_IMPL( ItemCl, AnimStay0 );
CLASS_PROPERTY_IMPL( ItemCl, AnimStay1 );
CLASS_PROPERTY_IMPL( ItemCl, AnimShow0 );
CLASS_PROPERTY_IMPL( ItemCl, AnimShow1 );
CLASS_PROPERTY_IMPL( ItemCl, AnimHide0 );
CLASS_PROPERTY_IMPL( ItemCl, AnimHide1 );
CLASS_PROPERTY_IMPL( ItemCl, SpriteCut );
CLASS_PROPERTY_IMPL( ItemCl, DrawOrderOffsetHexY );
CLASS_PROPERTY_IMPL( ItemCl, BlockLines );
CLASS_PROPERTY_IMPL( ItemCl, ScriptId );
CLASS_PROPERTY_IMPL( ItemCl, Accessory );
CLASS_PROPERTY_IMPL( ItemCl, MapId );
CLASS_PROPERTY_IMPL( ItemCl, HexX );
CLASS_PROPERTY_IMPL( ItemCl, HexY );
CLASS_PROPERTY_IMPL( ItemCl, CritId );
CLASS_PROPERTY_IMPL( ItemCl, CritSlot );
CLASS_PROPERTY_IMPL( ItemCl, ContainerId );
CLASS_PROPERTY_IMPL( ItemCl, ContainerStack );
CLASS_PROPERTY_IMPL( ItemCl, IsStatic );
CLASS_PROPERTY_IMPL( ItemCl, IsScenery );
CLASS_PROPERTY_IMPL( ItemCl, IsWall );
CLASS_PROPERTY_IMPL( ItemCl, IsCanOpen );
CLASS_PROPERTY_IMPL( ItemCl, IsScrollBlock );
CLASS_PROPERTY_IMPL( ItemCl, IsHidden );
CLASS_PROPERTY_IMPL( ItemCl, IsHiddenPicture );
CLASS_PROPERTY_IMPL( ItemCl, IsHiddenInStatic );
CLASS_PROPERTY_IMPL( ItemCl, IsFlat );
CLASS_PROPERTY_IMPL( ItemCl, IsNoBlock );
CLASS_PROPERTY_IMPL( ItemCl, IsShootThru );
CLASS_PROPERTY_IMPL( ItemCl, IsLightThru );
CLASS_PROPERTY_IMPL( ItemCl, IsAlwaysView );
CLASS_PROPERTY_IMPL( ItemCl, IsBadItem );
CLASS_PROPERTY_IMPL( ItemCl, IsNoHighlight );
CLASS_PROPERTY_IMPL( ItemCl, IsShowAnim );
CLASS_PROPERTY_IMPL( ItemCl, IsShowAnimExt );
CLASS_PROPERTY_IMPL( ItemCl, IsLight );
CLASS_PROPERTY_IMPL( ItemCl, IsGeck );
CLASS_PROPERTY_IMPL( ItemCl, IsTrap );
CLASS_PROPERTY_IMPL( ItemCl, IsTrigger );
CLASS_PROPERTY_IMPL( ItemCl, IsNoLightInfluence );
CLASS_PROPERTY_IMPL( ItemCl, IsGag );
CLASS_PROPERTY_IMPL( ItemCl, IsColorize );
CLASS_PROPERTY_IMPL( ItemCl, IsColorizeInv );
CLASS_PROPERTY_IMPL( ItemCl, IsCanTalk );
CLASS_PROPERTY_IMPL( ItemCl, IsRadio );
CLASS_PROPERTY_IMPL( ItemCl, SortValue );
CLASS_PROPERTY_IMPL( ItemCl, Mode );
CLASS_PROPERTY_IMPL( ItemCl, Count );
CLASS_PROPERTY_IMPL( ItemCl, TrapValue );
CLASS_PROPERTY_IMPL( ItemCl, RadioChannel );
CLASS_PROPERTY_IMPL( ItemCl, RadioFlags );
CLASS_PROPERTY_IMPL( ItemCl, RadioBroadcastSend );
CLASS_PROPERTY_IMPL( ItemCl, RadioBroadcastRecv );
CLASS_PROPERTY_IMPL( ItemCl, FlyEffectSpeed );

ItemCl::ItemCl( uint id, ProtoItem* proto ): Entity( id, EntityType::ItemCl, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );

    MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemCl ) + PropertiesRegistrator->GetWholeDataSize() );

    ChildItems = nullptr;

    RUNTIME_ASSERT( GetCount() > 0 );
}

ItemCl::~ItemCl()
{
    MEMORY_PROCESS( MEMORY_ITEM, -(int) ( sizeof( ItemCl ) + PropertiesRegistrator->GetWholeDataSize() ) );
}

void ItemCl::SetProto( ProtoItem* proto )
{
    RUNTIME_ASSERT( proto );
    proto->AddRef();
    Proto->Release();
    Proto = proto;
    Props = proto->Props;
}

ItemCl* ItemCl::Clone()
{
    RUNTIME_ASSERT( Type == EntityType::ItemCl );
    ItemCl* clone = new ItemCl( Id, (ProtoItem*) Proto );
    clone->Props = Props;
    return clone;
}

void ItemCl::SetSortValue( ItemClVec& items )
{
    short sort_value = 0;
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        ItemCl* item = *it;
        if( item == this )
            continue;
        if( sort_value >= item->GetSortValue() )
            sort_value = item->GetSortValue() - 1;
    }
    SetSortValue( sort_value );
}

static bool SortItemsFunc( ItemCl* l, ItemCl* r ) { return l->GetSortValue() < r->GetSortValue(); }
void        ItemCl::SortItems( ItemClVec& items )
{
    std::sort( items.begin(), items.end(), SortItemsFunc );
}

void ItemCl::ClearItems( ItemClVec& items )
{
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
        ( *it )->Release();
    items.clear();
}

void ItemCl::ChangeCount( int val )
{
    SetCount( GetCount() + val );
}

void ItemCl::ContSetItem( ItemCl* item )
{
    if( !ChildItems )
        ChildItems = new ItemClVec();

    RUNTIME_ASSERT( std::find( ChildItems->begin(), ChildItems->end(), item ) == ChildItems->end() );

    ChildItems->push_back( item );
    item->SetAccessory( ITEM_ACCESSORY_CONTAINER );
    item->SetContainerId( Id );
}

void ItemCl::ContEraseItem( ItemCl* item )
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

void ItemCl::ContGetItems( ItemClVec& items, uint stack_id )
{
    if( !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        ItemCl* item = *it;
        if( stack_id == uint( -1 ) || item->GetContainerStack() == stack_id )
            items.push_back( item );
    }
}

uint ItemCl::GetCurSprId()
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
