#ifndef __ITEM__
#define __ITEM__

#include "Common.h"
#include "FileManager.h"
#include "Text.h"
#include "Crypt.h"
#include "Properties.h"

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

// Prototypes
#define ITEM_MAX_SCRIPT_VALUES        ( 10 )
#define ITEM_MAX_CHILDS               ( 5 )

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

// Flags
#define ITEM_HIDDEN                   ( 0x00000001 )
#define ITEM_FLAT                     ( 0x00000002 )
#define ITEM_NO_BLOCK                 ( 0x00000004 )
#define ITEM_SHOOT_THRU               ( 0x00000008 )
#define ITEM_LIGHT_THRU               ( 0x00000010 )
#define ITEM_MULTI_HEX                ( 0x00000020 ) // Not used
#define ITEM_WALL_TRANS_END           ( 0x00000040 ) // Not used
#define ITEM_TWO_HANDS                ( 0x00000080 )
#define ITEM_BIG_GUN                  ( 0x00000100 )
#define ITEM_ALWAYS_VIEW              ( 0x00000200 )
#define ITEM_HAS_TIMER                ( 0x00000400 )
#define ITEM_BAD_ITEM                 ( 0x00000800 )
#define ITEM_NO_HIGHLIGHT             ( 0x00001000 )
#define ITEM_SHOW_ANIM                ( 0x00002000 )
#define ITEM_SHOW_ANIM_EXT            ( 0x00004000 )
#define ITEM_LIGHT                    ( 0x00008000 )
#define ITEM_GECK                     ( 0x00010000 )
#define ITEM_TRAP                     ( 0x00020000 )
#define ITEM_NO_LIGHT_INFLUENCE       ( 0x00040000 )
#define ITEM_NO_LOOT                  ( 0x00080000 )
#define ITEM_NO_STEAL                 ( 0x00100000 )
#define ITEM_GAG                      ( 0x00200000 )
#define ITEM_COLORIZE                 ( 0x00400000 )
#define ITEM_COLORIZE_INV             ( 0x00800000 )
#define ITEM_CAN_USE_ON_SMTH          ( 0x01000000 )
#define ITEM_CAN_LOOK                 ( 0x02000000 )
#define ITEM_CAN_TALK                 ( 0x04000000 )
#define ITEM_CAN_PICKUP               ( 0x08000000 )
#define ITEM_CAN_USE                  ( 0x10000000 )
#define ITEM_HOLODISK                 ( 0x20000000 )
#define ITEM_RADIO                    ( 0x40000000 )
#define ITEM_CACHED                   ( 0x80000000 ) // Not used

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
#define SP_SCEN_IBLOCK                ( 2344 )         // Block Hex Auto Inviso
#define SP_SCEN_TRIGGER               ( 3852 )
#define SP_WALL_BLOCK_LIGHT           ( 5621 )         // Wall s.t. with light
#define SP_WALL_BLOCK                 ( 5622 )         // Wall s.t.
#define SP_GRID_EXITGRID              ( 2049 )         // Exit Grid Map Marker
#define SP_GRID_ENTIRE                ( 3853 )
#define SP_MISC_SCRBLOCK              ( 4012 )         // Scroll block

// Slot protos offsets
// 1000 - 1100 protos reserved
#define SLOT_MAIN_PROTO_OFFSET        ( 1000 )
#define SLOT_EXT_PROTO_OFFSET         ( 1030 )
#define SLOT_ARMOR_PROTO_OFFSET       ( 1060 )

/************************************************************************/
/* ProtoItem                                                            */
/************************************************************************/

class ProtoItem
{
public:
    // Properties
    PROPERTIES_HEADER();
    CLASS_PROPERTY( int, Type );
    CLASS_PROPERTY( uint, Flags );
    CLASS_PROPERTY( bool, Stackable );
    CLASS_PROPERTY( bool, Deteriorable );
    CLASS_PROPERTY( hash, PicMap );
    CLASS_PROPERTY( hash, PicInv );
    CLASS_PROPERTY( bool, GroundLevel );
    CLASS_PROPERTY( int, Corner );
    CLASS_PROPERTY( uchar, Slot );
    CLASS_PROPERTY( uint, Weight );
    CLASS_PROPERTY( uint, Volume );
    CLASS_PROPERTY( uint, Cost );
    CLASS_PROPERTY( uint, StartCount );
    CLASS_PROPERTY( uchar, SoundId );
    CLASS_PROPERTY( uchar, Material );
    CLASS_PROPERTY( uchar, LightFlags );
    CLASS_PROPERTY( uchar, LightDistance );
    CLASS_PROPERTY( char, LightIntensity );
    CLASS_PROPERTY( uint, LightColor );
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
    CLASS_PROPERTY( short, OffsetX );
    CLASS_PROPERTY( short, OffsetY );
    CLASS_PROPERTY( uchar, SpriteCut );
    CLASS_PROPERTY( char, DrawOrderOffsetHexY );
    CLASS_PROPERTY( ushort, RadioChannel );
    CLASS_PROPERTY( ushort, RadioFlags );
    CLASS_PROPERTY( uchar, RadioBroadcastSend );
    CLASS_PROPERTY( uchar, RadioBroadcastRecv );
    CLASS_PROPERTY( uchar, IndicatorStart );
    CLASS_PROPERTY( uchar, IndicatorMax );
    CLASS_PROPERTY( uint, HolodiskNum );
    CLASS_PROPERTY( int, StartValue_0 );
    CLASS_PROPERTY( int, StartValue_1 );
    CLASS_PROPERTY( int, StartValue_2 );
    CLASS_PROPERTY( int, StartValue_3 );
    CLASS_PROPERTY( int, StartValue_4 );
    CLASS_PROPERTY( int, StartValue_5 );
    CLASS_PROPERTY( int, StartValue_6 );
    CLASS_PROPERTY( int, StartValue_7 );
    CLASS_PROPERTY( int, StartValue_8 );
    CLASS_PROPERTY( int, StartValue_9 );
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
    ProtoItem();
    bool operator==( const hash& pid ) { return ( ProtoId == pid ); }

    hash  ProtoId;
    int64 InstanceCount;

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

    bool IsItem()      { return !IsScen() && !IsWall() && !IsGrid(); }
    bool IsScen()      { return GetType() == ITEM_TYPE_GENERIC; }
    bool IsWall()      { return GetType() == ITEM_TYPE_WALL; }
    bool IsGrid()      { return GetType() == ITEM_TYPE_GRID; }
    bool IsArmor()     { return GetType() == ITEM_TYPE_ARMOR; }
    bool IsDrug()      { return GetType() == ITEM_TYPE_DRUG; }
    bool IsWeapon()    { return GetType() == ITEM_TYPE_WEAPON; }
    bool IsAmmo()      { return GetType() == ITEM_TYPE_AMMO; }
    bool IsMisc()      { return GetType() == ITEM_TYPE_MISC; }
    bool IsKey()       { return GetType() == ITEM_TYPE_KEY; }
    bool IsContainer() { return GetType() == ITEM_TYPE_CONTAINER; }
    bool IsDoor()      { return GetType() == ITEM_TYPE_DOOR; }
    bool IsGeneric()   { return GetType() == ITEM_TYPE_GENERIC; }
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
    CLASS_PROPERTY( ushort, SortValue );
    CLASS_PROPERTY( uchar, Indicator );
    CLASS_PROPERTY( hash, PicMap );
    CLASS_PROPERTY( hash, PicInv );
    CLASS_PROPERTY( uint, Flags );
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
    CLASS_PROPERTY( uint, HolodiskNumber );
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

    bool IsPassed()           { return FLAG( GetFlags(), ITEM_NO_BLOCK ) && FLAG( GetFlags(), ITEM_SHOOT_THRU ); }
    bool IsRaked()            { return FLAG( GetFlags(), ITEM_SHOOT_THRU ); }
    bool IsFlat()             { return FLAG( GetFlags(), ITEM_FLAT ); }
    bool IsHidden()           { return FLAG( GetFlags(), ITEM_HIDDEN ); }
    bool IsCanPickUp()        { return FLAG( GetFlags(), ITEM_CAN_PICKUP ); }
    bool IsCanTalk()          { return FLAG( GetFlags(), ITEM_CAN_TALK ); }
    bool IsCanUse()           { return FLAG( GetFlags(), ITEM_CAN_USE ); }
    bool IsCanUseOnSmth()     { return FLAG( GetFlags(), ITEM_CAN_USE_ON_SMTH ); }
    bool IsHasTimer()         { return FLAG( GetFlags(), ITEM_HAS_TIMER ); }
    bool IsBadItem()          { return FLAG( GetFlags(), ITEM_BAD_ITEM ); }
    bool IsTwoHands()         { return FLAG( GetFlags(), ITEM_TWO_HANDS ); }
    bool IsBigGun()           { return FLAG( GetFlags(), ITEM_BIG_GUN ); }
    bool IsNoHighlight()      { return FLAG( GetFlags(), ITEM_NO_HIGHLIGHT ); }
    bool IsShowAnim()         { return FLAG( GetFlags(), ITEM_SHOW_ANIM ); }
    bool IsShowAnimExt()      { return FLAG( GetFlags(), ITEM_SHOW_ANIM_EXT ); }
    bool IsLightThru()        { return FLAG( GetFlags(), ITEM_LIGHT_THRU ); }
    bool IsAlwaysView()       { return FLAG( GetFlags(), ITEM_ALWAYS_VIEW ); }
    bool IsGeck()             { return FLAG( GetFlags(), ITEM_GECK ); }
    bool IsNoLightInfluence() { return FLAG( GetFlags(), ITEM_NO_LIGHT_INFLUENCE ); }
    bool IsNoLoot()           { return FLAG( GetFlags(), ITEM_NO_LOOT ); }
    bool IsNoSteal()          { return FLAG( GetFlags(), ITEM_NO_STEAL ); }
    bool IsGag()              { return FLAG( GetFlags(), ITEM_GAG ); }
    bool IsHolodisk()         { return FLAG( GetFlags(), ITEM_HOLODISK ); }
    bool IsTrap()             { return FLAG( GetFlags(), ITEM_TRAP ); }

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

    // Key
    bool IsKey()     { return Proto->IsKey(); }
    uint KeyDoorId() { return GetLockerId(); }

    // Drug
    bool IsDrug() { return Proto->IsDrug(); }

    // Misc
    bool IsMisc() { return Proto->IsMisc(); }

    // Colorize
    bool  IsColorize()  { return FLAG( GetFlags(), ITEM_COLORIZE ); }
    uint  GetColor()    { return ( GetLightColor() ? GetLightColor() : Proto->GetLightColor() ) & 0xFFFFFF; }
    uchar GetAlpha()    { return ( GetLightColor() ? GetLightColor() : Proto->GetLightColor() ) >> 24; }
    uint  GetInvColor() { return FLAG( GetFlags(), ITEM_COLORIZE_INV ) ? ( GetLightColor() ? GetLightColor() : Proto->GetLightColor() ) : 0; }

    // Light
    bool IsLight()           { return FLAG( GetFlags(), ITEM_LIGHT ); }
    uint LightGetHash()      { return IsLight() ? GetLightIntensity() + GetLightDistance() + GetLightFlags() + GetLightColor() : 0; }
    int  LightGetIntensity() { return GetLightIntensity() ? GetLightIntensity() : Proto->GetLightIntensity(); }
    int  LightGetDistance()  { return GetLightDistance() ? GetLightDistance() : Proto->GetLightDistance(); }
    int  LightGetFlags()     { return GetLightFlags() ? GetLightFlags() : Proto->GetLightFlags(); }
    uint LightGetColor()     { return ( GetLightColor() ? GetLightColor() : Proto->GetLightColor() ) & 0xFFFFFF; }

    // Radio
    bool IsRadio()           { return FLAG( GetFlags(), ITEM_RADIO ); }
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
