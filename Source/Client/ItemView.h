#pragma once

#include "Common.h"
#include "FileManager.h"
#include "Text.h"
#include "Crypt.h"
#include "MsgFiles.h"
#include "Entity.h"

class ItemView: public Entity
{
public:
    // Properties
    PROPERTIES_HEADER();
    CLASS_PROPERTY( bool, Stackable );
    CLASS_PROPERTY( bool, Opened );
    CLASS_PROPERTY( int, Corner );
    CLASS_PROPERTY( uchar, Slot );
    CLASS_PROPERTY( bool, DisableEgg );
    CLASS_PROPERTY( ushort, AnimWaitBase );
    CLASS_PROPERTY( ushort, AnimWaitRndMin );
    CLASS_PROPERTY( ushort, AnimWaitRndMax );
    CLASS_PROPERTY( uchar, AnimStay0 );
    CLASS_PROPERTY( uchar, AnimStay1 );
    CLASS_PROPERTY( uchar, AnimShow0 );
    CLASS_PROPERTY( uchar, AnimShow1 );
    CLASS_PROPERTY( uchar, AnimHide0 );
    CLASS_PROPERTY( uchar, AnimHide1 );
    CLASS_PROPERTY( uchar, SpriteCut );
    CLASS_PROPERTY( char, DrawOrderOffsetHexY );
    CLASS_PROPERTY( CScriptArray *, BlockLines );
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( int, Accessory ); // enum ItemOwnership
    CLASS_PROPERTY( uint, MapId );
    CLASS_PROPERTY( ushort, HexX );
    CLASS_PROPERTY( ushort, HexY );
    CLASS_PROPERTY( uint, CritId );
    CLASS_PROPERTY( uchar, CritSlot );
    CLASS_PROPERTY( uint, ContainerId );
    CLASS_PROPERTY( uint, ContainerStack );
    CLASS_PROPERTY( bool, IsStatic );
    CLASS_PROPERTY( bool, IsScenery );
    CLASS_PROPERTY( bool, IsWall );
    CLASS_PROPERTY( bool, IsCanOpen );
    CLASS_PROPERTY( bool, IsScrollBlock );
    CLASS_PROPERTY( bool, IsHidden );
    CLASS_PROPERTY( bool, IsHiddenPicture );
    CLASS_PROPERTY( bool, IsHiddenInStatic );
    CLASS_PROPERTY( bool, IsFlat );
    CLASS_PROPERTY( bool, IsNoBlock );
    CLASS_PROPERTY( bool, IsShootThru );
    CLASS_PROPERTY( bool, IsLightThru );
    CLASS_PROPERTY( bool, IsAlwaysView );
    CLASS_PROPERTY( bool, IsBadItem );
    CLASS_PROPERTY( bool, IsNoHighlight );
    CLASS_PROPERTY( bool, IsShowAnim );
    CLASS_PROPERTY( bool, IsShowAnimExt );
    CLASS_PROPERTY( bool, IsLight );
    CLASS_PROPERTY( bool, IsGeck );
    CLASS_PROPERTY( bool, IsTrap );
    CLASS_PROPERTY( bool, IsTrigger );
    CLASS_PROPERTY( bool, IsNoLightInfluence );
    CLASS_PROPERTY( bool, IsGag );
    CLASS_PROPERTY( bool, IsColorize );
    CLASS_PROPERTY( bool, IsColorizeInv );
    CLASS_PROPERTY( bool, IsCanTalk );
    CLASS_PROPERTY( bool, IsRadio );
    CLASS_PROPERTY( short, SortValue );
    CLASS_PROPERTY( hash, PicMap );
    CLASS_PROPERTY( hash, PicInv );
    CLASS_PROPERTY( uchar, Mode );
    CLASS_PROPERTY( char, LightIntensity );
    CLASS_PROPERTY( uchar, LightDistance );
    CLASS_PROPERTY( uchar, LightFlags );
    CLASS_PROPERTY( uint, LightColor );
    CLASS_PROPERTY( uint, Count );
    CLASS_PROPERTY( short, TrapValue );
    CLASS_PROPERTY( ushort, RadioChannel );
    CLASS_PROPERTY( ushort, RadioFlags );
    CLASS_PROPERTY( uchar, RadioBroadcastSend );
    CLASS_PROPERTY( uchar, RadioBroadcastRecv );
    CLASS_PROPERTY( short, OffsetX );
    CLASS_PROPERTY( short, OffsetY );
    CLASS_PROPERTY( float, FlyEffectSpeed );

public:
    ItemView( uint id, ProtoItem* proto );
    ~ItemView();

    ItemViewVec* ChildItems;

    ProtoItem*  GetProtoItem() { return (ProtoItem*) Proto; }
    ItemView*   Clone();
    bool        operator==( const uint& id ) { return Id == id; }
    void        SetSortValue( ItemViewVec& items );
    static void SortItems( ItemViewVec& items );
    static void ClearItems( ItemViewVec& items );

    bool IsStatic()     { return GetIsStatic(); }
    bool IsAnyScenery() { return IsScenery() || IsWall(); }
    bool IsScenery()    { return GetIsScenery(); }
    bool IsWall()       { return GetIsWall(); }

    void ChangeCount( int val );

    uint GetCurSprId();

    void ContSetItem( ItemView* item );
    void ContEraseItem( ItemView* item );
    void ContGetItems( ItemViewVec& items, uint stack_id );

    // Colorize
    bool  IsColorize()  { return GetIsColorize(); }
    uint  GetColor()    { return ( GetLightColor() ? GetLightColor() : GetLightColor() ) & 0xFFFFFF; }
    uchar GetAlpha()    { return ( GetLightColor() ? GetLightColor() : GetLightColor() ) >> 24; }
    uint  GetInvColor() { return GetIsColorizeInv() ? ( GetLightColor() ? GetLightColor() : GetLightColor() ) : 0; }

    // Light
    uint LightGetHash()      { return GetIsLight() ? GetLightIntensity() + GetLightDistance() + GetLightFlags() + GetLightColor() : 0; }
    int  LightGetIntensity() { return GetLightIntensity() ? GetLightIntensity() : GetLightIntensity(); }
    int  LightGetDistance()  { return GetLightDistance() ? GetLightDistance() : GetLightDistance(); }
    int  LightGetFlags()     { return GetLightFlags() ? GetLightFlags() : GetLightFlags(); }
    uint LightGetColor()     { return ( GetLightColor() ? GetLightColor() : GetLightColor() ) & 0xFFFFFF; }

    // Radio
    bool RadioIsSendActive() { return !FLAG( GetRadioFlags(), RADIO_DISABLE_SEND ); }
    bool RadioIsRecvActive() { return !FLAG( GetRadioFlags(), RADIO_DISABLE_RECV ); }

    void SetProto( ProtoItem* proto );
};
