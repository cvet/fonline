#include "ItemView.h"
#include "Timer.h"
#include "ProtoManager.h"
#include "ResourceManager.h"
#include "SpriteManager.h"
#include "Testing.h"
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
    RUNTIME_ASSERT( GetCount() > 0 );
}

ItemView::~ItemView()
{
    MEMORY_PROCESS( MEMORY_ITEM, -(int) ( sizeof( ItemView ) + PropertiesRegistrator->GetWholeDataSize() ) );
}

ItemView* ItemView::Clone()
{
    ItemView* clone = new ItemView( Id, (ProtoItem*) Proto );
    clone->Props = Props;
    return clone;
}

#ifdef FONLINE_EDITOR
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
#endif
