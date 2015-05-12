#ifndef __ITEM__
#define __ITEM__

#include "Common.h"
#include "FileManager.h"
#include "Text.h"
#include "Crypt.h"
#include "Properties.h"
#include "MsgFiles.h"

class Critter;
class MapObject;

#define ITEM_EVENT_FINISH             ( 0 )
#define ITEM_EVENT_ATTACK             ( 1 )
#define ITEM_EVENT_USE                ( 2 )
#define ITEM_EVENT_USE_ON_ME          ( 3 )
#define ITEM_EVENT_SKILL              ( 4 )
#define ITEM_EVENT_DROP               ( 5 )
#define ITEM_EVENT_MOVE               ( 6 )
#define ITEM_EVENT_WALK               ( 7 )
#define ITEM_EVENT_MAX                ( 8 )
extern const char* ItemEventFuncName[ ITEM_EVENT_MAX ];

// Generic
#define ITEM_MAX_SCRIPT_VALUES        ( 10 )
#define ITEM_MAX_CHILDS               ( 5 )
#define MAX_ADDED_NOGROUP_ITEMS       ( 30 )

// Types
#define ITEM_TYPE_OTHER               ( 0 )
#define ITEM_TYPE_ARMOR               ( 1 )
#define ITEM_TYPE_DRUG                ( 2 ) // stacked
#define ITEM_TYPE_WEAPON              ( 3 ) // combined
#define ITEM_TYPE_AMMO                ( 4 ) // stacked
#define ITEM_TYPE_MISC                ( 5 ) // combined
#define ITEM_TYPE_KEY                 ( 7 )
#define ITEM_TYPE_CONTAINER           ( 8 )
#define ITEM_TYPE_DOOR                ( 9 )
#define ITEM_TYPE_GRID                ( 10 )
#define ITEM_TYPE_GENERIC             ( 11 )
#define ITEM_TYPE_WALL                ( 12 )
#define ITEM_TYPE_CAR                 ( 13 )
#define ITEM_MAX_TYPES                ( 14 )

// Grid Types
#define GRID_EXITGRID                 ( 1 )
#define GRID_STAIRS                   ( 2 )
#define GRID_LADDERBOT                ( 3 )
#define GRID_LADDERTOP                ( 4 )
#define GRID_ELEVATOR                 ( 5 )

// Accessory
#define ITEM_ACCESSORY_NONE           ( 0 )
#define ITEM_ACCESSORY_CRITTER        ( 1 )
#define ITEM_ACCESSORY_HEX            ( 2 )
#define ITEM_ACCESSORY_CONTAINER      ( 3 )

// Uses
#define USE_PRIMARY                   ( 0 )
#define USE_SECONDARY                 ( 1 )
#define USE_THIRD                     ( 2 )
#define USE_RELOAD                    ( 3 )
#define USE_USE                       ( 4 )
#define MAX_USES                      ( 3 )
#define USE_NONE                      ( 15 )
#define MAKE_ITEM_MODE( use, aim )    ( ( ( ( aim ) << 4 ) | ( ( use ) & 0xF ) ) & 0xFF )

// Corner type
#define CORNER_NORTH_SOUTH            ( 0 )
#define CORNER_WEST                   ( 1 )
#define CORNER_EAST                   ( 2 )
#define CORNER_SOUTH                  ( 3 )
#define CORNER_NORTH                  ( 4 )
#define CORNER_EAST_WEST              ( 5 )

// Item deterioration info
#define MAX_DETERIORATION             ( 10000 )
#define BI_BROKEN                     ( 0x0F )

// Radio
// Flags
#define RADIO_DISABLE_SEND            ( 0x01 )
#define RADIO_DISABLE_RECV            ( 0x02 )
// Broadcast
#define RADIO_BROADCAST_WORLD         ( 0 )
#define RADIO_BROADCAST_MAP           ( 20 )
#define RADIO_BROADCAST_LOCATION      ( 40 )
#define RADIO_BROADCAST_ZONE( x )     ( 100 + CLAMP( x, 1, 100 ) ) // 1..100
#define RADIO_BROADCAST_FORCE_ALL     ( 250 )

// Light flags
#define LIGHT_DISABLE_DIR( dir )      ( 1 << CLAMP( dir, 0, 5 ) )
#define LIGHT_GLOBAL                  ( 0x40 )
#define LIGHT_INVERSE                 ( 0x80 )

// Item data masks
#define ITEM_DATA_MASK_CHOSEN         ( 0 )
#define ITEM_DATA_MASK_CRITTER        ( 1 )
#define ITEM_DATA_MASK_CRITTER_EXT    ( 2 )
#define ITEM_DATA_MASK_CONTAINER      ( 3 )
#define ITEM_DATA_MASK_MAP            ( 4 )
#define ITEM_DATA_MASK_MAX            ( 5 )

// Special item pids
#define HASH_DECL( var, name )        extern hash var
#define HASH_IMPL( var, name )        hash var = Str::GetHash( name )
HASH_DECL( ITEM_DEF_SLOT, "internal_0" );
HASH_DECL( ITEM_DEF_ARMOR, "internal_100" );
HASH_DECL( SP_SCEN_IBLOCK, "minimap_invisible_block" );
HASH_DECL( SP_SCEN_TRIGGER, "trigger" );
HASH_DECL( SP_WALL_BLOCK_LIGHT, "block_light" );
HASH_DECL( SP_WALL_BLOCK, "block" );
HASH_DECL( SP_GRID_EXITGRID, "exit_grid" );
HASH_DECL( SP_GRID_ENTIRE, "entrance" );
HASH_DECL( SP_MISC_SCRBLOCK, "scroll_block" );

/************************************************************************/
/* ProtoItem                                                            */
/************************************************************************/

class ProtoItem
{
public:
    // Properties
    PROPERTIES_HEADER();
    CLASS_PROPERTY( hash, PicMap );
    CLASS_PROPERTY( hash, PicInv );
    CLASS_PROPERTY( short, OffsetX );
    CLASS_PROPERTY( short, OffsetY );
    CLASS_PROPERTY( uint, Cost );
    CLASS_PROPERTY( char, LightIntensity );
    CLASS_PROPERTY( uchar, LightDistance );
    CLASS_PROPERTY( uchar, LightFlags );
    CLASS_PROPERTY( uint, LightColor );
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
    CLASS_PROPERTY( uint, HolodiskNum );
    CLASS_PROPERTY_DATA( BlockLines );
    CLASS_PROPERTY( hash, ChildPid_0 );
    CLASS_PROPERTY( hash, ChildPid_1 );
    CLASS_PROPERTY( hash, ChildPid_2 );
    CLASS_PROPERTY( hash, ChildPid_3 );
    CLASS_PROPERTY( hash, ChildPid_4 );
    CLASS_PROPERTY_DATA( ChildLines_0 );
    CLASS_PROPERTY_DATA( ChildLines_1 );
    CLASS_PROPERTY_DATA( ChildLines_2 );
    CLASS_PROPERTY_DATA( ChildLines_3 );
    CLASS_PROPERTY_DATA( ChildLines_4 );
    CLASS_PROPERTY( bool, Weapon_IsUnarmed );
    CLASS_PROPERTY( int, Weapon_UnarmedTree );
    CLASS_PROPERTY( int, Weapon_UnarmedPriority );
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
    CLASS_PROPERTY( uint, Car_Speed );
    CLASS_PROPERTY( uint, Car_Passability );
    CLASS_PROPERTY( uint, Car_DeteriorationRate );
    CLASS_PROPERTY( uint, Car_CrittersCapacity );
    CLASS_PROPERTY( uint, Car_TankVolume );
    CLASS_PROPERTY( uint, Car_MaxDeterioration );
    CLASS_PROPERTY( uint, Car_FuelConsumption );
    CLASS_PROPERTY( uint, Car_Entrance );
    CLASS_PROPERTY( uint, Car_MovementType );

    // Internal data
public:
    ProtoItem( hash pid );
    bool operator==( hash pid ) { return ProtoId == pid; }

    hash       ProtoId;
    Properties ItemProps;
    int64      InstanceCount;

    UIntVec    TextsLang;
    FOMsgVec   Texts;

    #ifdef FONLINE_SERVER
    string ScriptName;
    #endif

    #ifdef FONLINE_MAPPER
    string CollectionName;
    #endif

    #ifdef FONLINE_OBJECT_EDITOR
    string ScriptModule;
    string ScriptFunc;
    string PicMapStr;
    string PicInvStr;
    string WeaponPicStr[ MAX_USES ];
    string Weapon_Anim2[ MAX_USES ];
    #endif

    void AddRef()  {}
    void Release() {}

    const char* GetName() { return HASH_STR( ProtoId ); }

    bool IsItem()      { return !IsScen() && !IsWall() && !IsGrid(); }
    bool IsScen()      { return GetType() == ITEM_TYPE_GENERIC; }
    bool IsWall()      { return GetType() == ITEM_TYPE_WALL; }
    bool IsGrid()      { return GetType() == ITEM_TYPE_GRID; }
    bool IsArmor()     { return GetType() == ITEM_TYPE_ARMOR; }
    bool IsWeapon()    { return GetType() == ITEM_TYPE_WEAPON; }
    bool IsAmmo()      { return GetType() == ITEM_TYPE_AMMO; }
    bool IsContainer() { return GetType() == ITEM_TYPE_CONTAINER; }
    bool IsDoor()      { return GetType() == ITEM_TYPE_DOOR; }
    bool IsCar()       { return GetType() == ITEM_TYPE_CAR; }

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    uint GetCurSprId();
    #endif

    const char* GetBlockLinesStr();
    hash        GetChildPid( uint index );
    const char* GetChildLinesStr( uint index );
};

typedef vector< ProtoItem* >    ProtoItemVec;
typedef map< hash, ProtoItem* > ProtoItemMap;

class Item;
typedef map< uint, Item* >      ItemMap;
typedef vector< Item* >         ItemVec;

/************************************************************************/
/* Item                                                                 */
/************************************************************************/

class Item
{
public:
    // Properties
    PROPERTIES_HEADER();
    #ifdef FONLINE_SERVER
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( ushort, LockerComplexity );
    #endif
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
    CLASS_PROPERTY( ushort, SortValue );
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
    CLASS_PROPERTY( ushort, AmmoCount );
    CLASS_PROPERTY( short, TrapValue );
    CLASS_PROPERTY( uint, LockerId );
    CLASS_PROPERTY( ushort, LockerCondition );
    CLASS_PROPERTY( uint, HolodiskNum );
    CLASS_PROPERTY( ushort, RadioChannel );
    CLASS_PROPERTY( ushort, RadioFlags );
    CLASS_PROPERTY( uchar, RadioBroadcastSend );
    CLASS_PROPERTY( uchar, RadioBroadcastRecv );
    CLASS_PROPERTY( short, OffsetX );
    CLASS_PROPERTY( short, OffsetY );

    // Internal fields
    uint       Id;
    ProtoItem* Proto;
    uchar      Accessory;
    bool       ViewPlaceOnMap;
    bool       IsNotValid;
    int        RefCounter;

    union     // 8
    {
        struct
        {
            uint   MapId;
            ushort HexX;
            ushort HexY;
        } AccHex;

        struct
        {
            uint  Id;
            uchar Slot;
        } AccCritter;

        struct
        {
            uint ContainerId;
            uint StackId;
        } AccContainer;

        char AccBuffer[ 8 ];
    };

    #ifdef FONLINE_SERVER
    int        FuncId[ ITEM_EVENT_MAX ];
    Critter*   ViewByCritter;
    ItemVec*   ChildItems;
    SyncObject Sync;
    #endif

    Item( uint id, ProtoItem* proto );
    ~Item();
    void SetProto( ProtoItem* proto );

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    Item* Clone();
    #endif

    bool operator==( const uint& id ) { return ( Id == id ); }

    void AddRef()  { RefCounter++; }
    void Release() { if( --RefCounter <= 0 ) delete this; }

    #ifdef FONLINE_SERVER
    void FullClear();

    bool ParseScript( const char* script, bool first_time );
    bool PrepareScriptFunc( int num_scr_func );
    void EventFinish( bool deleted );
    bool EventAttack( Critter* cr, Critter* target );
    bool EventUse( Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery );
    bool EventUseOnMe( Critter* cr, Item* used_item );
    bool EventSkill( Critter* cr, int skill );
    void EventDrop( Critter* cr );
    void EventMove( Critter* cr, uchar from_slot );
    void EventWalk( Critter* cr, bool entered, uchar dir );
    #endif // FONLINE_SERVER

    void        SetSortValue( ItemVec& items );
    static void SortItems( ItemVec& items );
    static void ClearItems( ItemVec& items );

    // All
    uint GetId()            { return Id; }
    hash GetProtoId()       { return Proto->ProtoId; }
    hash GetActualPicMap()  { return GetPicMap() ? GetPicMap() : Proto->GetPicMap(); }
    hash GetActualPicInv()  { return GetPicInv() ? GetPicInv() : Proto->GetPicInv(); }
    bool IsValidAccessory() { return Accessory == ITEM_ACCESSORY_CRITTER || Accessory == ITEM_ACCESSORY_HEX || Accessory == ITEM_ACCESSORY_CONTAINER; }
    void ChangeCount( int val );
    int  GetType() { return Proto->GetType(); }
    void SetWeaponMode( uchar mode );
    bool IsStackable() { return Proto->GetStackable(); }

    uint GetVolume()    { return GetCount() * Proto->GetVolume(); }
    uint GetVolume1st() { return Proto->GetVolume(); }
    uint GetWeight()    { return GetCount() * Proto->GetWeight(); }
    uint GetWeight1st() { return Proto->GetWeight(); }
    uint GetWholeCost() { return GetCount() * GetCost1st(); }
    uint GetCost1st();

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    uint GetCurSprId();
    #endif

    // Deterioration
    #ifdef FONLINE_SERVER
    void Repair();
    #endif

    bool IsDeteriorable()       { return Proto->GetDeteriorable(); }
    bool IsBroken()             { return FLAG( GetBrokenFlags(), BI_BROKEN ); }
    int  GetDeteriorationProc() { return CLAMP( GetDeterioration() * 100 / MAX_DETERIORATION, 0, 100 ); }

    // Armor
    bool IsArmor() { return GetType() == ITEM_TYPE_ARMOR; }

    // Weapon
    bool IsWeapon()                  { return GetType() == ITEM_TYPE_WEAPON; }
    bool WeapIsEmpty()               { return !GetAmmoCount(); }
    bool WeapIsFull()                { return GetAmmoCount() >= Proto->GetWeapon_MaxAmmoCount(); }
    uint WeapGetAmmoCount()          { return GetAmmoCount(); }
    hash WeapGetAmmoPid()            { return GetAmmoPid(); }
    uint WeapGetMaxAmmoCount()       { return Proto->GetWeapon_MaxAmmoCount(); }
    int  WeapGetAmmoCaliber()        { return Proto->GetWeapon_Caliber(); }
    bool WeapIsUseAviable( int use ) { return use >= USE_PRIMARY && use <= USE_THIRD ? ( ( ( Proto->GetWeapon_ActiveUses() >> use ) & 1 ) != 0 ) : false; }
    bool WeapIsCanAim( int use )     { return use >= 0 && use < MAX_USES && ( use == 0 ? Proto->GetWeapon_Aim_0() : ( use == 1 ? Proto->GetWeapon_Aim_1() : Proto->GetWeapon_Aim_2() ) ); }
    void WeapLoadHolder();

    // Container
    bool IsContainer()          { return Proto->IsContainer(); }
    bool ContIsCannotPickUp()   { return Proto->GetContainer_CannotPickUp(); }
    bool ContIsMagicHandsGrnd() { return Proto->GetContainer_MagicHandsGrnd(); }
    bool ContIsChangeble()      { return Proto->GetContainer_Changeble(); }
    #ifdef FONLINE_SERVER
    void  ContAddItem( Item*& item, uint stack_id );
    void  ContSetItem( Item* item );
    void  ContEraseItem( Item* item );
    Item* ContGetItem( uint item_id, bool skip_hide );
    void  ContGetAllItems( ItemVec& items, bool skip_hide, bool sync_lock );
    Item* ContGetItemByPid( hash pid, uint stack_id );
    void  ContGetItems( ItemVec& items, uint stack_id, bool sync_lock );
    int   ContGetFreeVolume( uint stack_id );
    bool  ContIsItems();
    #endif

    // Door
    bool IsDoor() { return GetType() == ITEM_TYPE_DOOR; }

    // Locker
    bool IsHasLocker()       { return IsDoor() || IsContainer(); }
    bool LockerIsOpen()      { return FLAG( GetLockerCondition(), LOCKER_ISOPEN ); }
    bool LockerIsClose()     { return !LockerIsOpen(); }
    bool LockerIsChangeble() { return Proto->IsDoor() ? true : ( IsContainer() ? Proto->GetContainer_Changeble() : false ); }

    // Ammo
    bool IsAmmo()         { return Proto->IsAmmo(); }
    int  AmmoGetCaliber() { return Proto->GetAmmo_Caliber(); }

    // Colorize
    bool  IsColorize()  { return GetIsColorize(); }
    uint  GetColor()    { return ( GetLightColor() ? GetLightColor() : Proto->GetLightColor() ) & 0xFFFFFF; }
    uchar GetAlpha()    { return ( GetLightColor() ? GetLightColor() : Proto->GetLightColor() ) >> 24; }
    uint  GetInvColor() { return GetIsColorizeInv() ? ( GetLightColor() ? GetLightColor() : Proto->GetLightColor() ) : 0; }

    // Light
    uint LightGetHash()      { return GetIsLight() ? GetLightIntensity() + GetLightDistance() + GetLightFlags() + GetLightColor() : 0; }
    int  LightGetIntensity() { return GetLightIntensity() ? GetLightIntensity() : Proto->GetLightIntensity(); }
    int  LightGetDistance()  { return GetLightDistance() ? GetLightDistance() : Proto->GetLightDistance(); }
    int  LightGetFlags()     { return GetLightFlags() ? GetLightFlags() : Proto->GetLightFlags(); }
    uint LightGetColor()     { return ( GetLightColor() ? GetLightColor() : Proto->GetLightColor() ) & 0xFFFFFF; }

    // Radio
    bool RadioIsSendActive() { return !FLAG( GetRadioFlags(), RADIO_DISABLE_SEND ); }
    bool RadioIsRecvActive() { return !FLAG( GetRadioFlags(), RADIO_DISABLE_RECV ); }

    // Car
    bool IsCar() { return Proto->IsCar(); }

    #ifdef FONLINE_SERVER
    Item* GetChild( uint child_index );
    #endif
};

/************************************************************************/
/*                                                                      */
/************************************************************************/

#define FOREACH_PROTO_ITEM_LINES( lines, hx, hy, maxhx, maxhy ) \
    FOREACH_PROTO_ITEM_LINES_WORK( lines, hx, hy, maxhx, maxhy, {} )
#define FOREACH_PROTO_ITEM_LINES_WORK( lines, hx, hy, maxhx, maxhy, work )   \
    int hx__ = hx, hy__ = hy;                                                \
    int maxhx__ = maxhx, maxhy__ = maxhy;                                    \
    for( uint i__ = 0, j__ = Str::Length( lines ) / 2; i__ < j__; i__++ )    \
    {                                                                        \
        uchar steps__ = lines[ i__ * 2 ] - '0';                              \
        uchar dir__ = lines[ i__ * 2 + 1 ] - '0';                            \
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

/************************************************************************/
/*                                                                      */
/************************************************************************/

#endif // __ITEM__
