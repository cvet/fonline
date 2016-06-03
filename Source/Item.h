#ifndef __ITEM__
#define __ITEM__

#include "Common.h"
#include "FileManager.h"
#include "Text.h"
#include "Crypt.h"
#include "MsgFiles.h"
#include "Entity.h"

// Items accessory
#define ITEM_ACCESSORY_NONE          ( 0 )
#define ITEM_ACCESSORY_CRITTER       ( 1 )
#define ITEM_ACCESSORY_HEX           ( 2 )
#define ITEM_ACCESSORY_CONTAINER     ( 3 )

// Generic
#define ITEM_MAX_CHILDS              ( 5 )
#define MAX_ADDED_NOGROUP_ITEMS      ( 30 )

// Types
#define ITEM_TYPE_OTHER              ( 0 )
#define ITEM_TYPE_ARMOR              ( 1 )
#define ITEM_TYPE_DRUG               ( 2 )
#define ITEM_TYPE_WEAPON             ( 3 )
#define ITEM_TYPE_AMMO               ( 4 )
#define ITEM_TYPE_MISC               ( 5 )
#define ITEM_TYPE_KEY                ( 7 )
#define ITEM_TYPE_CONTAINER          ( 8 )
#define ITEM_TYPE_DOOR               ( 9 )
#define ITEM_TYPE_GRID               ( 10 )
#define ITEM_TYPE_GENERIC            ( 11 )
#define ITEM_TYPE_WALL               ( 12 )
#define ITEM_TYPE_CAR                ( 13 )
#define ITEM_MAX_TYPES               ( 14 )

// Grid Types
#define GRID_EXITGRID                ( 1 )
#define GRID_STAIRS                  ( 2 )
#define GRID_LADDERBOT               ( 3 )
#define GRID_LADDERTOP               ( 4 )
#define GRID_ELEVATOR                ( 5 )

// Uses
#define USE_PRIMARY                  ( 0 )
#define USE_SECONDARY                ( 1 )
#define USE_THIRD                    ( 2 )
#define USE_RELOAD                   ( 3 )
#define USE_USE                      ( 4 )
#define MAX_USES                     ( 3 )
#define USE_NONE                     ( 15 )
#define MAKE_ITEM_MODE( use, aim )    ( ( ( ( aim ) << 4 ) | ( ( use ) & 0xF ) ) & 0xFF )

// Item deterioration info
#define MAX_DETERIORATION            ( 10000 )
#define BI_BROKEN                    ( 0x0F )

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

// Special item pids
#define HASH_DECL( var )              extern hash var
HASH_DECL( ITEM_DEF_SLOT );
HASH_DECL( ITEM_DEF_ARMOR );
HASH_DECL( SP_SCEN_IBLOCK );
HASH_DECL( SP_SCEN_TRIGGER );
HASH_DECL( SP_WALL_BLOCK_LIGHT );
HASH_DECL( SP_WALL_BLOCK );
HASH_DECL( SP_GRID_EXITGRID );
HASH_DECL( SP_GRID_ENTIRE );
HASH_DECL( SP_MISC_SCRBLOCK );

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

    CLASS_PROPERTY_ALIAS( int, Type );
    CLASS_PROPERTY_ALIAS( int, Grid_Type );
    CLASS_PROPERTY_ALIAS( hash, PicMap );
    CLASS_PROPERTY_ALIAS( hash, PicInv );
    CLASS_PROPERTY_ALIAS( bool, Stackable );
    CLASS_PROPERTY_ALIAS( bool, Deteriorable );
    CLASS_PROPERTY_ALIAS( bool, Weapon_IsUnarmed );
    CLASS_PROPERTY_ALIAS( uint, Weapon_Anim1 );
    CLASS_PROPERTY_ALIAS( short, OffsetX );
    CLASS_PROPERTY_ALIAS( short, OffsetY );
    CLASS_PROPERTY_ALIAS( uchar, Slot );
    CLASS_PROPERTY_ALIAS( char, LightIntensity );
    CLASS_PROPERTY_ALIAS( uchar, LightDistance );
    CLASS_PROPERTY_ALIAS( uchar, LightFlags );
    CLASS_PROPERTY_ALIAS( uint, LightColor );
    CLASS_PROPERTY_ALIAS( bool, IsCanPickUp );
    CLASS_PROPERTY_ALIAS( uint, Cost );
    CLASS_PROPERTY_ALIAS( uint, Count );
    CLASS_PROPERTY_ALIAS( bool, IsFlat );
    CLASS_PROPERTY_ALIAS( char, DrawOrderOffsetHexY );
    CLASS_PROPERTY_ALIAS( int, Corner );
    CLASS_PROPERTY_ALIAS( bool, DisableEgg );
    CLASS_PROPERTY_ALIAS( bool, IsBadItem );
    CLASS_PROPERTY_ALIAS( bool, IsColorize );
    CLASS_PROPERTY_ALIAS( bool, IsShowAnim );
    CLASS_PROPERTY_ALIAS( bool, IsShowAnimExt );
    CLASS_PROPERTY_ALIAS( uchar, AnimStay_0 );
    CLASS_PROPERTY_ALIAS( uchar, AnimStay_1 );
    CLASS_PROPERTY_ALIAS( ScriptString *, BlockLines );

    bool IsScenery() { return IsGeneric() || IsWall() || IsGrid(); }
    bool IsGeneric() { return GetType() == ITEM_TYPE_GENERIC; }
    bool IsWall()    { return GetType() == ITEM_TYPE_WALL; }
    bool IsGrid()    { return GetType() == ITEM_TYPE_GRID; }

    bool IsWeapon()    { return GetType() == ITEM_TYPE_WEAPON; }
    bool IsArmor()     { return GetType() == ITEM_TYPE_ARMOR; }
    bool IsContainer() { return GetType() == ITEM_TYPE_CONTAINER; }

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
    CLASS_PROPERTY( int, Type );
    CLASS_PROPERTY( bool, Stackable );
    CLASS_PROPERTY( bool, Deteriorable );
    CLASS_PROPERTY( bool, GroundLevel );
    CLASS_PROPERTY( int, Corner );
    CLASS_PROPERTY( uchar, Slot );
    CLASS_PROPERTY( uint, Weight );
    CLASS_PROPERTY( uint, Volume );
    CLASS_PROPERTY( uchar, SoundId );
    CLASS_PROPERTY( uchar, Material );
    CLASS_PROPERTY( bool, DisableEgg );
    CLASS_PROPERTY( ushort, AnimWaitBase );
    CLASS_PROPERTY( ushort, AnimWaitRndMin );
    CLASS_PROPERTY( ushort, AnimWaitRndMax );
    CLASS_PROPERTY( uchar, AnimStay_0 );
    CLASS_PROPERTY( uchar, AnimStay_1 );
    CLASS_PROPERTY( uchar, AnimShow_0 );
    CLASS_PROPERTY( uchar, AnimShow_1 );
    CLASS_PROPERTY( uchar, AnimHide_0 );
    CLASS_PROPERTY( uchar, AnimHide_1 );
    CLASS_PROPERTY( uchar, SpriteCut );
    CLASS_PROPERTY( char, DrawOrderOffsetHexY );
    CLASS_PROPERTY( uchar, IndicatorMax );
    CLASS_PROPERTY( ScriptString *, BlockLines );
    CLASS_PROPERTY( hash, ChildPid_0 );
    CLASS_PROPERTY( hash, ChildPid_1 );
    CLASS_PROPERTY( hash, ChildPid_2 );
    CLASS_PROPERTY( hash, ChildPid_3 );
    CLASS_PROPERTY( hash, ChildPid_4 );
    CLASS_PROPERTY( ScriptString *, ChildLines_0 );
    CLASS_PROPERTY( ScriptString *, ChildLines_1 );
    CLASS_PROPERTY( ScriptString *, ChildLines_2 );
    CLASS_PROPERTY( ScriptString *, ChildLines_3 );
    CLASS_PROPERTY( ScriptString *, ChildLines_4 );
    CLASS_PROPERTY( bool, Weapon_IsUnarmed );
    CLASS_PROPERTY( int, Weapon_UnarmedMinAgility );
    CLASS_PROPERTY( int, Weapon_UnarmedMinUnarmed );
    CLASS_PROPERTY( int, Weapon_UnarmedMinLevel );
    CLASS_PROPERTY( uint, Weapon_Anim1 );
    CLASS_PROPERTY( uint, Weapon_MaxAmmoCount );
    CLASS_PROPERTY( int, Weapon_Caliber );
    CLASS_PROPERTY( hash, Weapon_DefaultAmmoPid );
    CLASS_PROPERTY( int, Weapon_MinStrength );
    CLASS_PROPERTY( int, Weapon_Perk );
    CLASS_PROPERTY( bool, Weapon_IsTwoHanded );
    CLASS_PROPERTY( uint, Weapon_ActiveUses );
    CLASS_PROPERTY( int, Weapon_Skill_0 );
    CLASS_PROPERTY( int, Weapon_Skill_1 );
    CLASS_PROPERTY( int, Weapon_Skill_2 );
    CLASS_PROPERTY( hash, Weapon_PicUse_0 );
    CLASS_PROPERTY( hash, Weapon_PicUse_1 );
    CLASS_PROPERTY( hash, Weapon_PicUse_2 );
    CLASS_PROPERTY( uint, Weapon_MaxDist_0 );
    CLASS_PROPERTY( uint, Weapon_MaxDist_1 );
    CLASS_PROPERTY( uint, Weapon_MaxDist_2 );
    CLASS_PROPERTY( uint, Weapon_Round_0 );
    CLASS_PROPERTY( uint, Weapon_Round_1 );
    CLASS_PROPERTY( uint, Weapon_Round_2 );
    CLASS_PROPERTY( uint, Weapon_ApCost_0 );
    CLASS_PROPERTY( uint, Weapon_ApCost_1 );
    CLASS_PROPERTY( uint, Weapon_ApCost_2 );
    CLASS_PROPERTY( bool, Weapon_Aim_0 );
    CLASS_PROPERTY( bool, Weapon_Aim_1 );
    CLASS_PROPERTY( bool, Weapon_Aim_2 );
    CLASS_PROPERTY( uchar, Weapon_SoundId_0 );
    CLASS_PROPERTY( uchar, Weapon_SoundId_1 );
    CLASS_PROPERTY( uchar, Weapon_SoundId_2 );
    CLASS_PROPERTY( int, Ammo_Caliber );
    CLASS_PROPERTY( bool, Door_NoBlockMove );
    CLASS_PROPERTY( bool, Door_NoBlockShoot );
    CLASS_PROPERTY( bool, Door_NoBlockLight );
    CLASS_PROPERTY( uint, Container_Volume );
    CLASS_PROPERTY( bool, Container_Changeble );
    CLASS_PROPERTY( bool, Container_CannotPickUp );
    CLASS_PROPERTY( bool, Container_MagicHandsGrnd );
    CLASS_PROPERTY( ushort, Locker_Condition );
    CLASS_PROPERTY( int, Grid_Type );
    CLASS_PROPERTY( hash, Grid_ToMap );
    CLASS_PROPERTY( int, Grid_ToMapEntire );
    CLASS_PROPERTY( uchar, Grid_ToMapDir );
    CLASS_PROPERTY( uint, Car_Speed );
    CLASS_PROPERTY( uint, Car_Passability );
    CLASS_PROPERTY( uint, Car_DeteriorationRate );
    CLASS_PROPERTY( uint, Car_CrittersCapacity );
    CLASS_PROPERTY( uint, Car_TankVolume );
    CLASS_PROPERTY( uint, Car_MaxDeterioration );
    CLASS_PROPERTY( uint, Car_FuelConsumption );
    CLASS_PROPERTY( uint, Car_Entrance );
    CLASS_PROPERTY( uint, Car_MovementType );
    CLASS_PROPERTY( ScriptArray *, SceneryParams );
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( int, Accessory ); // enum ItemOwnership
    CLASS_PROPERTY( uint, MapId );
    CLASS_PROPERTY( ushort, HexX );
    CLASS_PROPERTY( ushort, HexY );
    CLASS_PROPERTY( uint, CritId );
    CLASS_PROPERTY( uchar, CritSlot );
    CLASS_PROPERTY( uint, ContainerId );
    CLASS_PROPERTY( uint, ContainerStack );
    CLASS_PROPERTY( bool, IsHidden );
    CLASS_PROPERTY( bool, IsFlat );
    CLASS_PROPERTY( bool, IsNoBlock );
    CLASS_PROPERTY( bool, IsShootThru );
    CLASS_PROPERTY( bool, IsLightThru );
    CLASS_PROPERTY( bool, IsMultiHex );
    CLASS_PROPERTY( bool, IsWallTransEnd );
    CLASS_PROPERTY( bool, IsBigGun );
    CLASS_PROPERTY( bool, IsAlwaysView );
    CLASS_PROPERTY( bool, IsHasTimer );
    CLASS_PROPERTY( bool, IsBadItem );
    CLASS_PROPERTY( bool, IsNoHighlight );
    CLASS_PROPERTY( bool, IsShowAnim );
    CLASS_PROPERTY( bool, IsShowAnimExt );
    CLASS_PROPERTY( bool, IsLight );
    CLASS_PROPERTY( bool, IsGeck );
    CLASS_PROPERTY( bool, IsTrap );
    CLASS_PROPERTY( bool, IsNoLightInfluence );
    CLASS_PROPERTY( bool, IsNoLoot );
    CLASS_PROPERTY( bool, IsNoSteal );
    CLASS_PROPERTY( bool, IsGag );
    CLASS_PROPERTY( bool, IsColorize );
    CLASS_PROPERTY( bool, IsColorizeInv );
    CLASS_PROPERTY( bool, IsCanUseOnSmth );
    CLASS_PROPERTY( bool, IsCanLook );
    CLASS_PROPERTY( bool, IsCanTalk );
    CLASS_PROPERTY( bool, IsCanPickUp );
    CLASS_PROPERTY( bool, IsCanUse );
    CLASS_PROPERTY( bool, IsHolodisk );
    CLASS_PROPERTY( bool, IsRadio );
    CLASS_PROPERTY( short, SortValue );
    CLASS_PROPERTY( uchar, Indicator );
    CLASS_PROPERTY( hash, PicMap );
    CLASS_PROPERTY( hash, PicInv );
    CLASS_PROPERTY( uchar, Mode );
    CLASS_PROPERTY( char, LightIntensity );
    CLASS_PROPERTY( uchar, LightDistance );
    CLASS_PROPERTY( uchar, LightFlags );
    CLASS_PROPERTY( uint, LightColor );
    CLASS_PROPERTY( uint, Count );
    CLASS_PROPERTY( uint, Cost );
    CLASS_PROPERTY( int, Val0 );
    CLASS_PROPERTY( int, Val1 );
    CLASS_PROPERTY( int, Val2 );
    CLASS_PROPERTY( int, Val3 );
    CLASS_PROPERTY( int, Val4 );
    CLASS_PROPERTY( int, Val5 );
    CLASS_PROPERTY( int, Val6 );
    CLASS_PROPERTY( int, Val7 );
    CLASS_PROPERTY( int, Val8 );
    CLASS_PROPERTY( int, Val9 );
    CLASS_PROPERTY( uchar, BrokenFlags );
    CLASS_PROPERTY( uchar, BrokenCount );
    CLASS_PROPERTY( ushort, Deterioration );
    CLASS_PROPERTY( hash, AmmoPid );
    CLASS_PROPERTY( uint, AmmoCount );
    CLASS_PROPERTY( short, TrapValue );
    CLASS_PROPERTY( uint, LockerId );
    CLASS_PROPERTY( ushort, LockerCondition );
    CLASS_PROPERTY( ushort, LockerComplexity );
    CLASS_PROPERTY( uint, HolodiskNum );
    CLASS_PROPERTY( ushort, RadioChannel );
    CLASS_PROPERTY( ushort, RadioFlags );
    CLASS_PROPERTY( uchar, RadioBroadcastSend );
    CLASS_PROPERTY( uchar, RadioBroadcastRecv );
    CLASS_PROPERTY( short, OffsetX );
    CLASS_PROPERTY( short, OffsetY );
    CLASS_PROPERTY( int, TriggerNum );

public:
    Item( uint id, ProtoItem* proto );
    ~Item();

    // Internal fields
    ItemVec* ChildItems;
    bool     ViewPlaceOnMap;

    #ifdef FONLINE_SERVER
    uint       SceneryScriptBindId;
    Critter*   ViewByCritter;
    SyncObject Sync;
    #endif

    ProtoItem*    GetProtoItem() { return (ProtoItem*) Proto; }
    hash          GetChildPid( uint index );
    ScriptString* GetChildLinesStr( uint index );

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    Item* Clone();
    #endif

    bool operator==( const uint& id ) { return Id == id; }

    #ifdef FONLINE_SERVER
    bool SetScript( const char* script_name, bool first_time );
    #endif // FONLINE_SERVER

    void        SetSortValue( ItemVec& items );
    static void SortItems( ItemVec& items );
    static void ClearItems( ItemVec& items );

    // All
    bool IsScenery() { return IsGeneric() || IsWall() || IsGrid(); }
    bool IsGeneric() { return GetType() == ITEM_TYPE_GENERIC; }
    bool IsWall()    { return GetType() == ITEM_TYPE_WALL; }
    bool IsGrid()    { return GetType() == ITEM_TYPE_GRID; }

    void ChangeCount( int val );
    void SetWeaponMode( uchar mode );

    uint GetWholeVolume() { return GetCount() * GetVolume(); }
    uint GetWholeWeight() { return GetCount() * GetWeight(); }
    uint GetWholeCost()   { return GetCount() * GetCost1st(); }
    uint GetCost1st();

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    uint GetCurSprId();
    #endif

    // Deterioration
    #ifdef FONLINE_SERVER
    void Repair();
    #endif
    bool IsBroken()             { return FLAG( GetBrokenFlags(), BI_BROKEN ); }
    int  GetDeteriorationProc() { return CLAMP( GetDeterioration() * 100 / MAX_DETERIORATION, 0, 100 ); }

    // Armor
    bool IsArmor() { return GetType() == ITEM_TYPE_ARMOR; }

    // Weapon
    bool IsWeapon()                  { return GetType() == ITEM_TYPE_WEAPON; }
    bool WeapIsEmpty()               { return !GetAmmoCount(); }
    bool WeapIsFull()                { return GetAmmoCount() >= GetWeapon_MaxAmmoCount(); }
    uint WeapGetAmmoCount()          { return GetAmmoCount(); }
    hash WeapGetAmmoPid()            { return GetAmmoPid(); }
    uint WeapGetMaxAmmoCount()       { return GetWeapon_MaxAmmoCount(); }
    int  WeapGetAmmoCaliber()        { return GetWeapon_Caliber(); }
    bool WeapIsUseAviable( int use ) { return use >= USE_PRIMARY && use <= USE_THIRD ? ( ( ( GetWeapon_ActiveUses() >> use ) & 1 ) != 0 ) : false; }
    bool WeapIsCanAim( int use )     { return use >= 0 && use < MAX_USES && ( use == 0 ? GetWeapon_Aim_0() : ( use == 1 ? GetWeapon_Aim_1() : GetWeapon_Aim_2() ) ); }
    void WeapLoadHolder();

    // Container
    bool IsContainer() { return GetType() == ITEM_TYPE_CONTAINER; }

    // Children
    #ifdef FONLINE_SERVER
    void  ContAddItem( Item*& item, uint stack_id );
    void  ContSetItem( Item* item );
    void  ContEraseItem( Item* item );
    Item* ContGetItem( uint item_id, bool skip_hide );
    void  ContGetAllItems( ItemVec& items, bool skip_hide, bool sync_lock );
    Item* ContGetItemByPid( hash pid, uint stack_id );
    void  ContGetItems( ItemVec& items, uint stack_id, bool sync_lock );
    bool  ContHaveFreeVolume( uint stack_id, uint volume );
    bool  ContIsItems();
    void  ContDeleteItems();
    #else
    void ContSetItem( Item* item );
    void ContEraseItem( Item* item );
    void ContGetItems( ItemVec& items, uint stack_id );
    #endif

    // Door
    bool IsDoor() { return GetType() == ITEM_TYPE_DOOR; }

    // Locker
    bool IsHasLocker()       { return IsDoor() || IsContainer(); }
    bool LockerIsOpen()      { return FLAG( GetLockerCondition(), LOCKER_ISOPEN ); }
    bool LockerIsClose()     { return !LockerIsOpen(); }
    bool LockerIsChangeble() { return IsDoor() ? true : ( IsContainer() ? GetContainer_Changeble() : false ); }

    // Ammo
    bool IsAmmo()         { return GetType() == ITEM_TYPE_AMMO; }
    int  AmmoGetCaliber() { return GetAmmo_Caliber(); }

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

    // Car
    bool IsCar() { return GetType() == ITEM_TYPE_CAR; }

    #ifdef FONLINE_SERVER
    Item* GetChild( uint child_index );
    #endif

    void SetProto( ProtoItem* proto );
};

// Lines foreach helper
#define FOREACH_PROTO_ITEM_LINES( lines, hx, hy, maxhx, maxhy ) \
    FOREACH_PROTO_ITEM_LINES_WORK( lines, hx, hy, maxhx, maxhy, {} )
#define FOREACH_PROTO_ITEM_LINES_WORK( lines, hx, hy, maxhx, maxhy, work )   \
    int hx__ = hx, hy__ = hy;                                                \
    int           maxhx__ = maxhx, maxhy__ = maxhy;                          \
    ScriptString* lines__ = lines;                                           \
    const char*   lines___ = lines__->c_str();                               \
    for( uint i__ = 0, j__ = Str::Length( lines___ ) / 2; i__ < j__; i__++ ) \
    {                                                                        \
        uchar dir__ = lines___[ i__ * 2 ] - '0';                             \
        uchar steps__ = lines___[ i__ * 2 + 1 ] - '0';                       \
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
    }                                                                        \
    SAFEREL( lines__ )

#endif // __ITEM__
