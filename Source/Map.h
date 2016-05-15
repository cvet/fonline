#ifndef __MAP__
#define __MAP__

#include "Common.h"
#include "ProtoMap.h"
#include "Item.h"
#include "Critter.h"
#include "Entity.h"

// Script events
#define MAP_EVENT_FINISH                ( 0 )
#define MAP_EVENT_LOOP_0                ( 1 )
#define MAP_EVENT_LOOP_1                ( 2 )
#define MAP_EVENT_LOOP_2                ( 3 )
#define MAP_EVENT_LOOP_3                ( 4 )
#define MAP_EVENT_LOOP_4                ( 5 )
#define MAP_EVENT_IN_CRITTER            ( 6 )
#define MAP_EVENT_OUT_CRITTER           ( 7 )
#define MAP_EVENT_CRITTER_DEAD          ( 8 )
#define MAP_EVENT_TURN_BASED_BEGIN      ( 9 )
#define MAP_EVENT_TURN_BASED_END        ( 10 )
#define MAP_EVENT_TURN_BASED_PROCESS    ( 11 )
#define MAP_EVENT_MAX                   ( 12 )
extern const char* MapEventFuncName[ MAP_EVENT_MAX ];

// Loop times
#define MAP_LOOP_FUNC_MAX               ( 5 )
#define MAP_LOOP_DEFAULT_TICK           ( 60 * 60000 )
#define MAP_MAX_DATA                    ( 100 )

class Map;
class Location;

class Map: public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( ushort, Width );
    CLASS_PROPERTY( ushort, Height );
    CLASS_PROPERTY( ushort, WorkHexX );
    CLASS_PROPERTY( ushort, WorkHexY );
    CLASS_PROPERTY( uint, LocId );
    CLASS_PROPERTY( uint, LocMapIndex );
    CLASS_PROPERTY( uchar, RainCapacity );
    CLASS_PROPERTY( bool, IsTurnBasedAviable );
    CLASS_PROPERTY( int, CurDayTime );
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( ScriptArray *, DayTime );    // 4 int
    CLASS_PROPERTY( ScriptArray *, DayColor );   // 12 uchar
    CLASS_PROPERTY( bool, IsNoLogOut );

    Map( uint id, ProtoMap* proto, Location* location );
    ~Map();

    SyncObject Sync;

private:
    Mutex     dataLocker;
    uchar*    hexFlags;
    int       hexFlagsSize;
    CrVec     mapCritters;
    ClVec     mapPlayers;
    PcVec     mapNpcs;
    ItemVec   hexItems;
    Location* mapLocation;

public:
    bool NeedProcess;
    uint FuncId[ MAP_EVENT_MAX ];
    uint LoopEnabled[ MAP_LOOP_FUNC_MAX ];
    uint LoopLastTick[ MAP_LOOP_FUNC_MAX ];
    uint LoopWaitTick[ MAP_LOOP_FUNC_MAX ];

    bool Generate();
    void DeleteContent();
    void Process();
    void Lock()   { dataLocker.Lock(); }
    void Unlock() { dataLocker.Unlock(); }

    ProtoMap* GetProtoMap() { return (ProtoMap*) Proto; }
    Location* GetLocation( bool lock );
    void      SetLocation( Location* loc ) { mapLocation = loc; }
    void      SetLoopTime( uint loop_num, uint ms );

    void SetText( ushort hx, ushort hy, uint color, const char* text, ushort text_len, bool unsafe_text );
    void SetTextMsg( ushort hx, ushort hy, uint color, ushort text_msg, uint num_str );
    void SetTextMsgLex( ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, const char* lexems, ushort lexems_len );

    bool GetStartCoord( ushort& hx, ushort& hy, uchar& dir, uint entire );
    bool GetStartCoordCar( ushort& hx, ushort& hy, Item* item );
    bool FindStartHex( ushort& hx, ushort& hy, uint multihex, uint seek_radius, bool skip_unsafe );

    void AddCritter( Critter* cr );
    void AddCritterEvents( Critter* cr );
    void EraseCritter( Critter* cr );
    void EraseCritterEvents( Critter* cr );
    void KickPlayersToGlobalMap();

    bool AddItem( Item* item, ushort hx, ushort hy );
    void SetItem( Item* item, ushort hx, ushort hy );
    void EraseItem( uint item_id );
    void SendProperty( NetProperty::Type type, Property* prop, Entity* entity );
    void ChangeViewItem( Item* item );
    void AnimateItem( Item* item, uchar from_frm, uchar to_frm );

    Item* GetItem( uint item_id );
    Item* GetItemHex( ushort hx, ushort hy, hash item_pid, Critter* picker );
    Item* GetItemDoor( ushort hx, ushort hy );
    Item* GetItemCar( ushort hx, ushort hy );
    Item* GetItemContainer( ushort hx, ushort hy );
    Item* GetItemGag( ushort hx, ushort hy );

    ItemVec& GetItemsNoLock() { return hexItems; }
    void     GetItemsHex( ushort hx, ushort hy, ItemVec& items, bool lock );
    void     GetItemsHexEx( ushort hx, ushort hy, uint radius, hash pid, ItemVec& items, bool lock );
    void     GetItemsPid( hash pid, ItemVec& items, bool lock );
    void     GetItemsType( int type, ItemVec& items, bool lock );
    void     GetItemsTrap( ushort hx, ushort hy, ItemVec& items, bool lock );
    void     RecacheHexBlock( ushort hx, ushort hy );
    void     RecacheHexShoot( ushort hx, ushort hy );
    void     RecacheHexBlockShoot( ushort hx, ushort hy );

    ushort GetHexFlags( ushort hx, ushort hy );
    void   SetHexFlag( ushort hx, ushort hy, uchar flag );
    void   UnsetHexFlag( ushort hx, ushort hy, uchar flag );

    bool IsHexPassed( ushort hx, ushort hy );
    bool IsHexRaked( ushort hx, ushort hy );
    bool IsHexesPassed( ushort hx, ushort hy, uint radius );
    bool IsMovePassed( ushort hx, ushort hy, uchar dir, uint multihex );
    bool IsHexItem( ushort hx, ushort hy )    { return FLAG( hexFlags[ hy * GetWidth() + hx ], FH_ITEM ); }
    bool IsHexTrap( ushort hx, ushort hy )    { return FLAG( hexFlags[ hy * GetWidth() + hx ], FH_WALK_ITEM ); }
    bool IsHexCritter( ushort hx, ushort hy ) { return FLAG( hexFlags[ hy * GetWidth() + hx ], FH_CRITTER | FH_DEAD_CRITTER ); }
    bool IsHexGag( ushort hx, ushort hy )     { return FLAG( hexFlags[ hy * GetWidth() + hx ], FH_GAG_ITEM ); }
    bool IsHexTrigger( ushort hx, ushort hy ) { return FLAG( GetProtoMap()->HexFlags[ hy * GetWidth() + hx ], FH_TRIGGER ); }

    bool     IsFlagCritter( ushort hx, ushort hy, bool dead );
    void     SetFlagCritter( ushort hx, ushort hy, uint multihex, bool dead );
    void     UnsetFlagCritter( ushort hx, ushort hy, uint multihex, bool dead );
    uint     GetNpcCount( int npc_role, int find_type );
    Critter* GetCritter( uint crid, bool sync_lock );
    Critter* GetNpc( int npc_role, int find_type, uint skip_count, bool sync_lock );
    Critter* GetHexCritter( ushort hx, ushort hy, bool dead, bool sync_lock );
    void     GetCrittersHex( ushort hx, ushort hy, uint radius, int find_type, CrVec& critters, bool sync_lock ); // Critters append

    void   GetCritters( CrVec& critters, bool sync_lock );
    void   GetPlayers( ClVec& players, bool sync_lock );
    void   GetNpcs( PcVec& npcs, bool sync_lock );
    CrVec& GetCrittersNoLock() { return mapCritters; }
    ClVec& GetPlayersNoLock()  { return mapPlayers; }
    PcVec& GetNpcsNoLock()     { return mapNpcs; }
    uint   GetCrittersCount();
    uint   GetPlayersCount();
    uint   GetNpcsCount();

    // Sends
    void SendEffect( hash eff_pid, ushort hx, ushort hy, ushort radius );
    void SendFlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );

    // Cars
public:
    void  GetCritterCar( Critter* cr, Item* car );
    void  SetCritterCar( ushort hx, ushort hy, Critter* cr, Item* car );
    bool  IsPlaceForItem( ushort hx, ushort hy, Item* item );
    bool  IsPlaceForProtoItem( ushort hx, ushort hy, ProtoItem* proto_item );
    void  PlaceItemBlocks( ushort hx, ushort hy, Item* item );
    void  ReplaceItemBlocks( ushort hx, ushort hy, Item* item );
    Item* GetItemChild( ushort hx, ushort hy, Item* item, uint child_index );

    // Events
private:
    bool PrepareScriptFunc( int num_scr_func );

public:
    bool SetScript( const char* script_name, bool first_time );
    void EventFinish( bool to_delete );
    void EventLoop( int loop_num );
    void EventInCritter( Critter* cr );
    void EventOutCritter( Critter* cr );
    void EventCritterDead( Critter* cr, Critter* killer );
    void EventTurnBasedBegin();
    void EventTurnBasedEnd();
    void EventTurnBasedProcess( Critter* cr, bool begin_turn );

    // Turn based game
public:
    bool    IsTurnBasedOn;
    uint    TurnBasedEndTick;
    int     TurnSequenceCur;
    UIntVec TurnSequence;
    bool    IsTurnBasedTimeout;
    uint    TurnBasedBeginSecond;
    bool    NeedEndTurnBased;
    uint    TurnBasedRound;
    uint    TurnBasedTurn;
    uint    TurnBasedWholeTurn;

    void BeginTurnBased( Critter* first_cr );
    void EndTurnBased();
    bool TryEndTurnBased();
    void ProcessTurnBased();
    bool IsCritterTurn( Critter* cr );
    uint GetCritterTurnId();
    uint GetCritterTurnTime();
    void EndCritterTurn();
    void NextCritterTurn();
    void GenerateSequence( Critter* first_cr );
};
typedef map< uint, Map* > MapMap;
typedef vector< Map* >    MapVec;

// Script events
#define LOCATION_EVENT_FINISH    ( 0 )
#define LOCATION_EVENT_ENTER     ( 1 )
#define LOCATION_EVENT_MAX       ( 2 )
extern const char* LocationEventFuncName[ LOCATION_EVENT_MAX ];

class Location: public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( ScriptArray *, MapProtos );    // hash[]
    CLASS_PROPERTY( ScriptArray *, MapEntrances ); // hash[]
    CLASS_PROPERTY( ScriptArray *, Automaps );     // hash[]
    CLASS_PROPERTY( uint, MaxPlayers );
    CLASS_PROPERTY( bool, AutoGarbage );
    CLASS_PROPERTY( bool, GeckVisible );
    CLASS_PROPERTY( hash, EntranceScript );
    CLASS_PROPERTY( ushort, WorldX );
    CLASS_PROPERTY( ushort, WorldY );
    CLASS_PROPERTY( ushort, Radius );
    CLASS_PROPERTY( bool, Hidden );
    CLASS_PROPERTY( bool, ToGarbage );
    CLASS_PROPERTY( uint, Color );

    Location( uint id, ProtoLocation* proto );
    ~Location();

    SyncObject Sync;

private:
    MapVec locMaps;

public:
    uint         EntranceScriptBindId;

    volatile int GeckCount;
    uint         FuncId[ LOCATION_EVENT_MAX ];

    void           BindScript();
    ProtoLocation* GetProtoLoc()   { return (ProtoLocation*) Proto; }
    bool           IsLocVisible()  { return !GetHidden() || ( GetGeckVisible() && GeckCount > 0 ); }
    MapVec&        GetMapsNoLock() { return locMaps; };
    void           GetMaps( MapVec& maps, bool lock );
    uint           GetMapsCount() { return (uint) locMaps.size(); }
    Map*           GetMapByIndex( uint index );
    Map*           GetMapByPid( hash map_pid );
    uint           GetMapIndex( hash map_pid );
    bool           GetTransit( Map* from_map, uint& id_map, ushort& hx, ushort& hy, uchar& dir );
    bool           IsCanEnter( uint players_count );

    bool IsNoCrit();
    bool IsNoPlayer();
    bool IsNoNpc();
    bool IsCanDelete();

// Events
private:
    bool PrepareScriptFunc( int num_scr_func );

public:
    void EventFinish( bool to_delete );
    bool EventEnter( ScriptArray* group, uchar entrance );
};
typedef map< uint, Location* > LocMap;
typedef vector< Location* >    LocVec;

#endif // __MAP__
