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

// Blocks, childs
#define ITEM_MAX_BLOCK_LINES          ( 50 )
#define ITEM_MAX_CHILDS               ( 5 )
#define ITEM_MAX_CHILD_LINES          ( 6 )

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
    // Internal data
    hash   ProtoId;
    int    Type;
    hash   PicMap;
    hash   PicInv;
    uint   Flags;
    bool   Stackable;
    bool   Deteriorable;
    bool   GroundLevel;
    int    Corner;
    uchar  Slot;
    uint   Weight;
    uint   Volume;
    uint   Cost;
    uint   StartCount;
    uchar  SoundId;
    uchar  Material;
    uchar  LightFlags;
    uchar  LightDistance;
    char   LightIntensity;
    uint   LightColor;
    bool   DisableEgg;
    ushort AnimWaitBase;
    ushort AnimWaitRndMin;
    ushort AnimWaitRndMax;
    uchar  AnimStay[ 2 ];
    uchar  AnimShow[ 2 ];
    uchar  AnimHide[ 2 ];
    short  OffsetX;
    short  OffsetY;
    uchar  SpriteCut;
    char   DrawOrderOffsetHexY;
    ushort RadioChannel;
    ushort RadioFlags;
    uchar  RadioBroadcastSend;
    uchar  RadioBroadcastRecv;
    uchar  IndicatorStart;
    uchar  IndicatorMax;
    uint   HolodiskNum;
    int    StartValue[ ITEM_MAX_SCRIPT_VALUES ];
    uchar  BlockLines[ ITEM_MAX_BLOCK_LINES ];
    hash   ChildPid[ ITEM_MAX_CHILDS ];
    uchar  ChildLines[ ITEM_MAX_CHILDS ][ ITEM_MAX_CHILD_LINES ];

    // User data, binded with 'bindfield' pragma
    int UserData[ PROTO_ITEM_USER_DATA_SIZE / sizeof( int ) ];

    // Type specific data
    bool   Weapon_IsUnarmed;
    int    Weapon_UnarmedTree;
    int    Weapon_UnarmedPriority;
    int    Weapon_UnarmedMinAgility;
    int    Weapon_UnarmedMinUnarmed;
    int    Weapon_UnarmedMinLevel;
    uint   Weapon_Anim1;
    uint   Weapon_MaxAmmoCount;
    int    Weapon_Caliber;
    hash   Weapon_DefaultAmmoPid;
    int    Weapon_MinStrength;
    int    Weapon_Perk;
    uint   Weapon_ActiveUses;
    int    Weapon_Skill[ MAX_USES ];
    hash   Weapon_PicUse[ MAX_USES ];
    uint   Weapon_MaxDist[ MAX_USES ];
    uint   Weapon_Round[ MAX_USES ];
    uint   Weapon_ApCost[ MAX_USES ];
    bool   Weapon_Aim[ MAX_USES ];
    uchar  Weapon_SoundId[ MAX_USES ];
    int    Ammo_Caliber;
    bool   Door_NoBlockMove;
    bool   Door_NoBlockShoot;
    bool   Door_NoBlockLight;
    uint   Container_Volume;
    bool   Container_CannotPickUp;
    bool   Container_MagicHandsGrnd;
    bool   Container_Changeble;
    ushort Locker_Condition;
    int    Grid_Type;
    uint   Car_Speed;
    uint   Car_Passability;
    uint   Car_DeteriorationRate;
    uint   Car_CrittersCapacity;
    uint   Car_TankVolume;
    uint   Car_MaxDeterioration;
    uint   Car_FuelConsumption;
    uint   Car_Entrance;
    uint   Car_MovementType;

    void AddRef()  {}
    void Release() {}

    bool IsItem() { return !IsScen() && !IsWall() && !IsGrid(); }
    bool IsScen() { return Type == ITEM_TYPE_GENERIC; }
    bool IsWall() { return Type == ITEM_TYPE_WALL; }
    bool IsGrid() { return Type == ITEM_TYPE_GRID; }

    bool IsArmor()     { return Type == ITEM_TYPE_ARMOR; }
    bool IsDrug()      { return Type == ITEM_TYPE_DRUG; }
    bool IsWeapon()    { return Type == ITEM_TYPE_WEAPON; }
    bool IsAmmo()      { return Type == ITEM_TYPE_AMMO; }
    bool IsMisc()      { return Type == ITEM_TYPE_MISC; }
    bool IsKey()       { return Type == ITEM_TYPE_KEY; }
    bool IsContainer() { return Type == ITEM_TYPE_CONTAINER; }
    bool IsDoor()      { return Type == ITEM_TYPE_DOOR; }
    bool IsGeneric()   { return Type == ITEM_TYPE_GENERIC; }
    bool IsCar()       { return Type == ITEM_TYPE_CAR; }

    bool IsBlocks() { return BlockLines[ 0 ] != 0; }
    bool LockerIsChangeble()
    {
        if( IsDoor() ) return true;
        if( IsContainer() ) return Container_Changeble;
        return false;
    }
    bool IsCanPickUp() { return FLAG( Flags, ITEM_CAN_PICKUP ); }

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    uint GetCurSprId();
    #endif

    ProtoItem() { memzero( this, OFFSETOF( ProtoItem, InstanceCount ) + sizeof( InstanceCount ) ); }
    bool operator==( const hash& pid ) { return ( ProtoId == pid ); }

    int64 InstanceCount;

    #ifdef FONLINE_OBJECT_EDITOR
    string ScriptModule;
    string ScriptFunc;
    string PicMapStr;
    string PicInvStr;
    string WeaponPicStr[ MAX_USES ];
    string Weapon_Anim2[ MAX_USES ];
    #endif

    #ifdef FONLINE_MAPPER
    string CollectionName;
    #endif

    #ifdef FONLINE_SERVER
    string ScriptName;
    #endif
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
    // Script fields
    static PropertyRegistrator* PropertiesRegistrator;
    static void SetPropertyRegistrator( PropertyRegistrator* registrator );
    static Property*            PropertyFlags;
    static Property*            PropertyCount;
    static Property*            PropertyHolodiskNumber;
    static Property*            PropertySortValue;
    static Property*            PropertyBrokenFlags;
    static Property*            PropertyDeterioration;
    static Property*            PropertyLockerCondition;
    static Property*            PropertyMode;
    static Property*            PropertyAmmoPid;
    static Property*            PropertyAmmoCount;
    Properties                  Props;

    // Internal fields
    uint       Id;
    ProtoItem* Proto;
    int        From;
    uchar      Accessory;
    bool       ViewPlaceOnMap;

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

    short RefCounter;
    bool  IsNotValid;

    ushort& SortValue;
    uchar&  Indicator;
    hash&   PicMap;
    hash&   PicInv;
    uint&   Flags;
    uchar&  Mode;
    char&   LightIntensity;
    uchar&  LightDistance;
    uchar&  LightFlags;
    uint&   LightColor;
    hash&   ScriptId;
    uint&   Count;
    uint&   Cost;
    int&    Val0;
    int&    Val1;
    int&    Val2;
    int&    Val3;
    int&    Val4;
    int&    Val5;
    int&    Val6;
    int&    Val7;
    int&    Val8;
    int&    Val9;
    uchar&  BrokenFlags;
    uchar&  BrokenCount;
    ushort& Deterioration;
    hash&   AmmoPid;
    ushort& AmmoCount;
    short&  TrapValue;
    uint&   LockerId;
    ushort& LockerCondition;
    ushort& LockerComplexity;
    uint&   HolodiskNumber;
    ushort& RadioChannel;
    ushort& RadioFlags;
    uchar&  RadioBroadcastSend;
    uchar&  RadioBroadcastRecv;
    short&  OffsetX;
    short&  OffsetY;

    #ifdef FONLINE_SERVER
    int        FuncId[ ITEM_EVENT_MAX ];
    Critter*   ViewByCritter;
    ItemVec*   ChildItems;
    char*      PLexems;
    SyncObject Sync;
    #endif
    #ifdef FONLINE_CLIENT
    ScriptString* Lexems;
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

    template< typename T >
    void SetPropertyValue( Property* prop, T value )
    {
        RUNTIME_ASSERT( sizeof( T ) == prop->Size );
        prop->Accessor->GenericSet( this, &value );
    }

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
    hash GetPicMap()        { return PicMap ? PicMap : Proto->PicMap; }
    hash GetPicInv()        { return PicInv ? PicInv : Proto->PicInv; }
    bool IsValidAccessory() { return Accessory == ITEM_ACCESSORY_CRITTER || Accessory == ITEM_ACCESSORY_HEX || Accessory == ITEM_ACCESSORY_CONTAINER; }

    void SetCount( int val );
    void ChangeCount( int val );

    int  GetType() { return Proto->Type; }
    void SetMode( uchar mode );

    bool IsStackable() { return Proto->Stackable; }
    bool IsBlocks()    { return Proto->IsBlocks(); }

    bool IsPassed()           { return FLAG( Flags, ITEM_NO_BLOCK ) && FLAG( Flags, ITEM_SHOOT_THRU ); }
    bool IsRaked()            { return FLAG( Flags, ITEM_SHOOT_THRU ); }
    bool IsFlat()             { return FLAG( Flags, ITEM_FLAT ); }
    bool IsHidden()           { return FLAG( Flags, ITEM_HIDDEN ); }
    bool IsCanPickUp()        { return FLAG( Flags, ITEM_CAN_PICKUP ); }
    bool IsCanTalk()          { return FLAG( Flags, ITEM_CAN_TALK ); }
    bool IsCanUse()           { return FLAG( Flags, ITEM_CAN_USE ); }
    bool IsCanUseOnSmth()     { return FLAG( Flags, ITEM_CAN_USE_ON_SMTH ); }
    bool IsHasTimer()         { return FLAG( Flags, ITEM_HAS_TIMER ); }
    bool IsBadItem()          { return FLAG( Flags, ITEM_BAD_ITEM ); }
    bool IsTwoHands()         { return FLAG( Flags, ITEM_TWO_HANDS ); }
    bool IsBigGun()           { return FLAG( Flags, ITEM_BIG_GUN ); }
    bool IsNoHighlight()      { return FLAG( Flags, ITEM_NO_HIGHLIGHT ); }
    bool IsShowAnim()         { return FLAG( Flags, ITEM_SHOW_ANIM ); }
    bool IsShowAnimExt()      { return FLAG( Flags, ITEM_SHOW_ANIM_EXT ); }
    bool IsLightThru()        { return FLAG( Flags, ITEM_LIGHT_THRU ); }
    bool IsAlwaysView()       { return FLAG( Flags, ITEM_ALWAYS_VIEW ); }
    bool IsGeck()             { return FLAG( Flags, ITEM_GECK ); }
    bool IsNoLightInfluence() { return FLAG( Flags, ITEM_NO_LIGHT_INFLUENCE ); }
    bool IsNoLoot()           { return FLAG( Flags, ITEM_NO_LOOT ); }
    bool IsNoSteal()          { return FLAG( Flags, ITEM_NO_STEAL ); }
    bool IsGag()              { return FLAG( Flags, ITEM_GAG ); }
    bool IsHolodisk()         { return FLAG( Flags, ITEM_HOLODISK ); }
    bool IsTrap()             { return FLAG( Flags, ITEM_TRAP ); }

    uint GetVolume()    { return Count * Proto->Volume; }
    uint GetVolume1st() { return Proto->Volume; }
    uint GetWeight()    { return Count * Proto->Weight; }
    uint GetWeight1st() { return Proto->Weight; }
    uint GetCost()      { return Count * GetCost1st(); }
    uint GetCost1st();

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    uint GetCurSprId();
    #endif

    #ifdef FONLINE_SERVER
    void SetLexems( const char* lexems );
    #endif

    // Deterioration
    #ifdef FONLINE_SERVER
    void Repair();
    #endif

    bool IsDeteriorable() { return Proto->Deteriorable; }
    bool IsBroken()       { return FLAG( BrokenFlags, BI_BROKEN ); }
    int  GetDeteriorationProc()
    {
        int val = Deterioration * 100 / MAX_DETERIORATION;
        return CLAMP( val, 0, 100 );
    }

    // Armor
    bool IsArmor() { return GetType() == ITEM_TYPE_ARMOR; }

    // Weapon
    bool IsWeapon()                  { return GetType() == ITEM_TYPE_WEAPON; }
    bool WeapIsEmpty()               { return !AmmoCount; }
    bool WeapIsFull()                { return AmmoCount >= Proto->Weapon_MaxAmmoCount; }
    uint WeapGetAmmoCount()          { return AmmoCount; }
    hash WeapGetAmmoPid()            { return AmmoPid; }
    uint WeapGetMaxAmmoCount()       { return Proto->Weapon_MaxAmmoCount; }
    int  WeapGetAmmoCaliber()        { return Proto->Weapon_Caliber; }
    bool WeapIsUseAviable( int use ) { return use >= USE_PRIMARY && use <= USE_THIRD ? ( ( ( Proto->Weapon_ActiveUses >> use ) & 1 ) != 0 ) : false; }
    bool WeapIsCanAim( int use )     { return use >= 0 && use < MAX_USES && Proto->Weapon_Aim[ use ]; }
    void WeapLoadHolder();

    // Container
    bool IsContainer()          { return Proto->IsContainer(); }
    bool ContIsCannotPickUp()   { return Proto->Container_CannotPickUp; }
    bool ContIsMagicHandsGrnd() { return Proto->Container_MagicHandsGrnd; }
    bool ContIsChangeble()      { return Proto->Container_Changeble; }
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
    bool LockerIsOpen()      { return FLAG( LockerCondition, LOCKER_ISOPEN ); }
    bool LockerIsClose()     { return !LockerIsOpen(); }
    bool LockerIsChangeble() { return Proto->LockerIsChangeble(); }

    // Ammo
    bool IsAmmo()         { return Proto->IsAmmo(); }
    int  AmmoGetCaliber() { return Proto->Ammo_Caliber; }

    // Key
    bool IsKey()     { return Proto->IsKey(); }
    uint KeyDoorId() { return LockerId; }

    // Drug
    bool IsDrug() { return Proto->IsDrug(); }

    // Misc
    bool IsMisc() { return Proto->IsMisc(); }

    // Colorize
    bool  IsColorize()  { return FLAG( Flags, ITEM_COLORIZE ); }
    uint  GetColor()    { return ( LightColor ? LightColor : Proto->LightColor ) & 0xFFFFFF; }
    uchar GetAlpha()    { return ( LightColor ? LightColor : Proto->LightColor ) >> 24; }
    uint  GetInvColor() { return FLAG( Flags, ITEM_COLORIZE_INV ) ? ( LightColor ? LightColor : Proto->LightColor ) : 0; }

    // Light
    bool IsLight() { return FLAG( Flags, ITEM_LIGHT ); }
    uint LightGetHash()
    {
        if( !IsLight() ) return 0;
        if( LightIntensity ) return Crypt.Crc32( (uchar*) &LightIntensity, 7 ) + FLAG( Flags, ITEM_LIGHT );
        return (uint) (uint64) Proto;
    }
    int  LightGetIntensity() { return LightIntensity ? LightIntensity : Proto->LightIntensity; }
    int  LightGetDistance()  { return LightDistance ? LightDistance : Proto->LightDistance; }
    int  LightGetFlags()     { return LightFlags ? LightFlags : Proto->LightFlags; }
    uint LightGetColor()     { return ( LightColor ? LightColor : Proto->LightColor ) & 0xFFFFFF; }

    // Radio
    bool IsRadio()           { return FLAG( Flags, ITEM_RADIO ); }
    bool RadioIsSendActive() { return !FLAG( RadioFlags, RADIO_DISABLE_SEND ); }
    bool RadioIsRecvActive() { return !FLAG( RadioFlags, RADIO_DISABLE_RECV ); }

    // Car
    bool IsCar() { return Proto->IsCar(); }

    #ifdef FONLINE_SERVER
    Item* GetChild( uint child_index );
    #endif
};

/************************************************************************/
/*                                                                      */
/************************************************************************/

#define FOREACH_PROTO_ITEM_LINES( lines, hx, hy, maxhx, maxhy, work )        \
    int hx__ = hx, hy__ = hy;                                                \
    int maxhx__ = maxhx, maxhy__ = maxhy;                                    \
    for( uint i__ = 0; i__ < sizeof( lines ); i__++ )                        \
    {                                                                        \
        uchar block__ = lines[ i__ ];                                        \
        uchar dir__ = ( block__ >> 4 );                                      \
        if( dir__ >= DIRS_COUNT )                                            \
            break;                                                           \
        uchar steps__ = ( block__ & 0xF );                                   \
        if( !steps__ )                                                       \
            break;                                                           \
        for( uchar j__ = 0; j__ < steps__; j__++ )                           \
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
