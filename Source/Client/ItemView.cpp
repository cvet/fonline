#include "ItemView.h"
#include "Timer.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "SpriteManager.h"
#include "Exception.h"
#include "Debugger.h"

PROPERTIES_IMPL( ItemView );
CLASS_PROPERTY_IMPL( ItemView, PicMap );
CLASS_PROPERTY_IMPL( ItemView, PicInv );
CLASS_PROPERTY_IMPL( ItemView, OffsetX );
CLASS_PROPERTY_IMPL( ItemView, OffsetY );
CLASS_PROPERTY_IMPL( ItemView, LightIntensity );
CLASS_PROPERTY_IMPL( ItemView, LightDistance );
CLASS_PROPERTY_IMPL( ItemView, LightFlags );
CLASS_PROPERTY_IMPL( ItemView, LightColor );
CLASS_PROPERTY_IMPL( ItemView, Stackable );
CLASS_PROPERTY_IMPL( ItemView, Opened );
CLASS_PROPERTY_IMPL( ItemView, Corner );
CLASS_PROPERTY_IMPL( ItemView, Slot );
CLASS_PROPERTY_IMPL( ItemView, DisableEgg );
CLASS_PROPERTY_IMPL( ItemView, AnimWaitBase );
CLASS_PROPERTY_IMPL( ItemView, AnimWaitRndMin );
CLASS_PROPERTY_IMPL( ItemView, AnimWaitRndMax );
CLASS_PROPERTY_IMPL( ItemView, AnimStay0 );
CLASS_PROPERTY_IMPL( ItemView, AnimStay1 );
CLASS_PROPERTY_IMPL( ItemView, AnimShow0 );
CLASS_PROPERTY_IMPL( ItemView, AnimShow1 );
CLASS_PROPERTY_IMPL( ItemView, AnimHide0 );
CLASS_PROPERTY_IMPL( ItemView, AnimHide1 );
CLASS_PROPERTY_IMPL( ItemView, SpriteCut );
CLASS_PROPERTY_IMPL( ItemView, DrawOrderOffsetHexY );
CLASS_PROPERTY_IMPL( ItemView, BlockLines );
CLASS_PROPERTY_IMPL( ItemView, ScriptId );
CLASS_PROPERTY_IMPL( ItemView, Accessory );
CLASS_PROPERTY_IMPL( ItemView, MapId );
CLASS_PROPERTY_IMPL( ItemView, HexX );
CLASS_PROPERTY_IMPL( ItemView, HexY );
CLASS_PROPERTY_IMPL( ItemView, CritId );
CLASS_PROPERTY_IMPL( ItemView, CritSlot );
CLASS_PROPERTY_IMPL( ItemView, ContainerId );
CLASS_PROPERTY_IMPL( ItemView, ContainerStack );
CLASS_PROPERTY_IMPL( ItemView, IsStatic );
CLASS_PROPERTY_IMPL( ItemView, IsScenery );
CLASS_PROPERTY_IMPL( ItemView, IsWall );
CLASS_PROPERTY_IMPL( ItemView, IsCanOpen );
CLASS_PROPERTY_IMPL( ItemView, IsScrollBlock );
CLASS_PROPERTY_IMPL( ItemView, IsHidden );
CLASS_PROPERTY_IMPL( ItemView, IsHiddenPicture );
CLASS_PROPERTY_IMPL( ItemView, IsHiddenInStatic );
CLASS_PROPERTY_IMPL( ItemView, IsFlat );
CLASS_PROPERTY_IMPL( ItemView, IsNoBlock );
CLASS_PROPERTY_IMPL( ItemView, IsShootThru );
CLASS_PROPERTY_IMPL( ItemView, IsLightThru );
CLASS_PROPERTY_IMPL( ItemView, IsAlwaysView );
CLASS_PROPERTY_IMPL( ItemView, IsBadItem );
CLASS_PROPERTY_IMPL( ItemView, IsNoHighlight );
CLASS_PROPERTY_IMPL( ItemView, IsShowAnim );
CLASS_PROPERTY_IMPL( ItemView, IsShowAnimExt );
CLASS_PROPERTY_IMPL( ItemView, IsLight );
CLASS_PROPERTY_IMPL( ItemView, IsGeck );
CLASS_PROPERTY_IMPL( ItemView, IsTrap );
CLASS_PROPERTY_IMPL( ItemView, IsTrigger );
CLASS_PROPERTY_IMPL( ItemView, IsNoLightInfluence );
CLASS_PROPERTY_IMPL( ItemView, IsGag );
CLASS_PROPERTY_IMPL( ItemView, IsColorize );
CLASS_PROPERTY_IMPL( ItemView, IsColorizeInv );
CLASS_PROPERTY_IMPL( ItemView, IsCanTalk );
CLASS_PROPERTY_IMPL( ItemView, IsRadio );
CLASS_PROPERTY_IMPL( ItemView, SortValue );
CLASS_PROPERTY_IMPL( ItemView, Mode );
CLASS_PROPERTY_IMPL( ItemView, Count );
CLASS_PROPERTY_IMPL( ItemView, TrapValue );
CLASS_PROPERTY_IMPL( ItemView, RadioChannel );
CLASS_PROPERTY_IMPL( ItemView, RadioFlags );
CLASS_PROPERTY_IMPL( ItemView, RadioBroadcastSend );
CLASS_PROPERTY_IMPL( ItemView, RadioBroadcastRecv );
CLASS_PROPERTY_IMPL( ItemView, FlyEffectSpeed );

ItemView::ItemView( uint id, ProtoItem* proto ): Entity( id, EntityType::ItemView, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );

    MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemView ) + PropertiesRegistrator->GetWholeDataSize() );

    ChildItems = nullptr;

    RUNTIME_ASSERT( GetCount() > 0 );
}

ItemView::~ItemView()
{
    MEMORY_PROCESS( MEMORY_ITEM, -(int) ( sizeof( ItemView ) + PropertiesRegistrator->GetWholeDataSize() ) );
}

void ItemView::SetProto( ProtoItem* proto )
{
    RUNTIME_ASSERT( proto );
    proto->AddRef();
    Proto->Release();
    Proto = proto;
    Props = proto->Props;
}

ItemView* ItemView::Clone()
{
    RUNTIME_ASSERT( Type == EntityType::ItemView );
    ItemView* clone = new ItemView( Id, (ProtoItem*) Proto );
    clone->Props = Props;
    return clone;
}

void ItemView::SetSortValue( ItemViewVec& items )
{
    short sort_value = 0;
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        ItemView* item = *it;
        if( item == this )
            continue;
        if( sort_value >= item->GetSortValue() )
            sort_value = item->GetSortValue() - 1;
    }
    SetSortValue( sort_value );
}

static bool SortItemsFunc( ItemView* l, ItemView* r ) { return l->GetSortValue() < r->GetSortValue(); }
void        ItemView::SortItems( ItemViewVec& items )
{
    std::sort( items.begin(), items.end(), SortItemsFunc );
}

void ItemView::ClearItems( ItemViewVec& items )
{
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
        ( *it )->Release();
    items.clear();
}

void ItemView::ChangeCount( int val )
{
    SetCount( GetCount() + val );
}

void ItemView::ContSetItem( ItemView* item )
{
    if( !ChildItems )
        ChildItems = new ItemViewVec();

    RUNTIME_ASSERT( std::find( ChildItems->begin(), ChildItems->end(), item ) == ChildItems->end() );

    ChildItems->push_back( item );
    item->SetAccessory( ITEM_ACCESSORY_CONTAINER );
    item->SetContainerId( Id );
}

void ItemView::ContEraseItem( ItemView* item )
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

void ItemView::ContGetItems( ItemViewVec& items, uint stack_id )
{
    if( !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        ItemView* item = *it;
        if( stack_id == uint( -1 ) || item->GetContainerStack() == stack_id )
            items.push_back( item );
    }
}

uint ItemView::GetCurSprId()
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
