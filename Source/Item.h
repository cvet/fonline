#ifndef __ITEM__
#define __ITEM__

#include "Common.h"
#include "FileManager.h"
#include "Text.h"
#include "Crypt.h"
#include "MsgFiles.h"
#include "Entity.h"

#ifdef FONLINE_SERVER
class Critter;
#endif

// Items accessory
#define ITEM_ACCESSORY_NONE          ( 0 )
#define ITEM_ACCESSORY_CRITTER       ( 1 )
#define ITEM_ACCESSORY_HEX           ( 2 )
#define ITEM_ACCESSORY_CONTAINER     ( 3 )

// Generic
#define MAX_ADDED_NOGROUP_ITEMS      ( 30 )

// Uses
#define USE_PRIMARY                  ( 0 )
#define USE_SECONDARY                ( 1 )
#define USE_THIRD                    ( 2 )
#define USE_RELOAD                   ( 3 )
#define USE_USE                      ( 4 )
#define MAX_USES                     ( 3 )
#define USE_NONE                     ( 15 )
#define MAKE_ITEM_MODE( use, aim )    ( ( ( ( aim ) << 4 ) | ( ( use ) & 0xF ) ) & 0xFF )

// Radio
// Flags
#define RADIO_DISABLE_SEND           ( 0x01 )
#define RADIO_DISABLE_RECV           ( 0x02 )
// Broadcast
#define RADIO_BROADCAST_WORLD        ( 0 )
#define RADIO_BROADCAST_MAP          ( 20 )
#define RADIO_BROADCAST_LOCATION     ( 40 )
#define RADIO_BROADCAST_ZONE( x )     ( 100 + CLAMP( x, 1, 100 ) ) // 1..100
#define RADIO_BROADCAST_FORCE_ALL    ( 250 )

// Light flags
#define LIGHT_DISABLE_DIR( dir )      ( 1 << CLAMP( dir, 0, 5 ) )
#define LIGHT_GLOBAL                 ( 0x40 )
#define LIGHT_INVERSE                ( 0x80 )

// Item prototype
class ProtoItem: public ProtoEntity
{
public:
    ProtoItem( hash pid );

    int64    InstanceCount;

    UIntVec  TextsLang;
    FOMsgVec Texts;

    #ifdef FONLINE_MAPPER
    string CollectionName;
    #endif

    CLASS_PROPERTY_ALIAS( hash, PicMap );
    CLASS_PROPERTY_ALIAS( hash, PicInv );
    CLASS_PROPERTY_ALIAS( bool, Stackable );
    CLASS_PROPERTY_ALIAS( short, OffsetX );
    CLASS_PROPERTY_ALIAS( short, OffsetY );
    CLASS_PROPERTY_ALIAS( uchar, Slot );
    CLASS_PROPERTY_ALIAS( char, LightIntensity );
    CLASS_PROPERTY_ALIAS( uchar, LightDistance );
    CLASS_PROPERTY_ALIAS( uchar, LightFlags );
    CLASS_PROPERTY_ALIAS( uint, LightColor );
    CLASS_PROPERTY_ALIAS( uint, Count );
    CLASS_PROPERTY_ALIAS( bool, IsFlat );
    CLASS_PROPERTY_ALIAS( char, DrawOrderOffsetHexY );
    CLASS_PROPERTY_ALIAS( int, Corner );
    CLASS_PROPERTY_ALIAS( bool, DisableEgg );
    CLASS_PROPERTY_ALIAS( bool, IsStatic );
    CLASS_PROPERTY_ALIAS( bool, IsScenery );
    CLASS_PROPERTY_ALIAS( bool, IsWall );
    CLASS_PROPERTY_ALIAS( bool, IsBadItem );
    CLASS_PROPERTY_ALIAS( bool, IsColorize );
    CLASS_PROPERTY_ALIAS( bool, IsShowAnim );
    CLASS_PROPERTY_ALIAS( bool, IsShowAnimExt );
    CLASS_PROPERTY_ALIAS( uchar, AnimStay0 );
    CLASS_PROPERTY_ALIAS( uchar, AnimStay1 );
    CLASS_PROPERTY_ALIAS( CScriptArray *, BlockLines );

    bool IsStatic()     { return GetIsStatic(); }
    bool IsAnyScenery() { return IsScenery() || IsWall(); }
    bool IsScenery()    { return GetIsScenery(); }
    bool IsWall()       { return GetIsWall(); }

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    uint GetCurSprId();
    #endif
};
typedef vector< ProtoItem* >    ProtoItemVec;
typedef map< hash, ProtoItem* > ProtoItemMap;

// Item
class Item;
typedef map< uint, Item* >      ItemMap;
typedef vector< Item* >         ItemVec;
class Item: public Entity
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
    Item( uint id, ProtoItem* proto );
    ~Item();

    // Internal fields
    ItemVec* ChildItems;
    bool     ViewPlaceOnMap;

    #ifdef FONLINE_SERVER
    uint     SceneryScriptBindId;
    Critter* ViewByCritter;
    #endif

    ProtoItem* GetProtoItem() { return (ProtoItem*) Proto; }

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    Item* Clone();
    #endif

    bool operator==( const uint& id ) { return Id == id; }

    #ifdef FONLINE_SERVER
    bool SetScript( asIScriptFunction* func, bool first_time );
    #endif // FONLINE_SERVER

    void        SetSortValue( ItemVec& items );
    static void SortItems( ItemVec& items );
    static void ClearItems( ItemVec& items );

    // All
    bool IsStatic()     { return GetIsStatic(); }
    bool IsAnyScenery() { return IsScenery() || IsWall(); }
    bool IsScenery()    { return GetIsScenery(); }
    bool IsWall()       { return GetIsWall(); }

    void ChangeCount( int val );

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    uint GetCurSprId();
    #endif

    // Children
    #ifdef FONLINE_SERVER
    void  ContAddItem( Item*& item, uint stack_id );
    void  ContSetItem( Item* item );
    void  ContEraseItem( Item* item );
    Item* ContGetItem( uint item_id, bool skip_hide );
    void  ContGetAllItems( ItemVec& items, bool skip_hide );
    Item* ContGetItemByPid( hash pid, uint stack_id );
    void  ContGetItems( ItemVec& items, uint stack_id );
    bool  ContIsItems();
    void  ContDeleteItems();
    #else
    void ContSetItem( Item* item );
    void ContEraseItem( Item* item );
    void ContGetItems( ItemVec& items, uint stack_id );
    #endif

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

// Lines foreach helper
#define FOREACH_PROTO_ITEM_LINES( lines, hx, hy, maxhx, maxhy, work )        \
    int hx__ = hx, hy__ = hy;                                                \
    int maxhx__ = maxhx, maxhy__ = maxhy;                                    \
    for( uint i__ = 0, j__ = ( lines )->GetSize() / 2; i__ < j__; i__++ )    \
    {                                                                        \
        uchar dir__ = *(uchar*) ( lines )->At( i__ * 2 );                    \
        uchar steps__ = *(uchar*) ( lines )->At( i__ * 2 + 1 );              \
        if( dir__ >= DIRS_COUNT || !steps__ || steps__ > 9 )                 \
            break;                                                           \
        for( uchar k__ = 0; k__ < steps__; k__++ )                           \
        {                                                                    \
            MoveHexByDirUnsafe( hx__, hy__, dir__ );                         \
            if( hx__ < 0 || hy__ < 0 || hx__ >= maxhx__ || hy__ >= maxhy__ ) \
                continue;                                                    \
            hx = hx__, hy = hy__;                                            \
            work                                                             \
        }                                                                    \
    }

#endif // __ITEM__
