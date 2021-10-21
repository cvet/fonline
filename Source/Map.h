#ifndef __MAP__
#define __MAP__

#include "Common.h"
#include "ProtoMap.h"
#include "Item.h"
#include "Critter.h"
#include "Entity.h"

class Map;
class Location;

using ItemVecMap = map< uint, ItemVec >;

class Map: public Entity
{
public:
	PROPERTIES_HEADER( );
	CLASS_PROPERTY( uint, LoopTime1 );
	CLASS_PROPERTY( uint, LoopTime2 );
	CLASS_PROPERTY( uint, LoopTime3 );
	CLASS_PROPERTY( uint, LoopTime4 );
	CLASS_PROPERTY( uint, LoopTime5 );
	CLASS_PROPERTY( string, FileDir );
	CLASS_PROPERTY( ushort, Width );
	CLASS_PROPERTY( ushort, Height );
	CLASS_PROPERTY( ushort, WorkHexX );
	CLASS_PROPERTY( ushort, WorkHexY );
	CLASS_PROPERTY( uint, LocId );
	CLASS_PROPERTY( uint, LocMapIndex );
	CLASS_PROPERTY( uchar, RainCapacity );
	CLASS_PROPERTY( int, CurDayTime );
	CLASS_PROPERTY( hash, ScriptId );
	CLASS_PROPERTY( CScriptArray *, DayTime );    // 4 int
	CLASS_PROPERTY( CScriptArray *, DayColor );   // 12 uchar
	CLASS_PROPERTY( bool, IsNoLogOut );

	Map( uint id, ProtoMap* proto, Location* location );
	~Map( );

private:
	HexData *HexFlags;

	int        hexFlagsSize;
	CrVec      mapCritters;
	ClVec      mapPlayers;
	PcVec      mapNpcs;
	ItemVec    mapItems;
	ItemMap    mapItemsById;
	ItemVecMap mapItemsByHex;
	ItemVecMap mapBlockLinesByHex;
	Location*  mapLocation;
	uint       loopLastTick[ 5 ];

	void PlaceItemBlocks( ushort hx, ushort hy, Item* item );
	void RemoveItemBlocks( ushort hx, ushort hy, Item* item );

public:
	bool Generate( );
	void DeleteContent( );
	void Process( );
	void ProcessLoop( int index, uint time, uint tick );

	ProtoMap* GetProtoMap( ) { return ( ProtoMap* )Proto; }
	Location* GetLocation( );
	void      SetLocation( Location* loc ) { mapLocation = loc; }

	void SetText( ushort hx, ushort hy, uint color, const string& text, bool unsafe_text );
	void SetTextMsg( ushort hx, ushort hy, uint color, ushort text_msg, uint num_str );
	void SetTextMsgLex( ushort hx, ushort hy, uint color, ushort text_msg, uint num_str, const char* lexems, ushort lexems_len );

	bool FindStartHex( ushort& hx, ushort& hy, uint multihex, uint seek_radius, bool skip_unsafe );

	void AddCritter( Critter* cr );
	void EraseCritter( Critter* cr );
	void KickPlayersToGlobalMap( );

	bool AddItem( Item* item, ushort hx, ushort hy );
	void SetItem( Item* item, ushort hx, ushort hy );
	void EraseItem( uint item_id );
	void SendProperty( NetProperty::Type type, Property* prop, Entity* entity );
	void ChangeViewItem( Item* item );
	void AnimateItem( Item* item, uchar from_frm, uchar to_frm );

	Item* GetItem( uint item_id );
	Item* GetItemHex( ushort hx, ushort hy, hash item_pid, Critter* picker );
	Item* GetItemGag( ushort hx, ushort hy );

	ItemVec GetItems( ) { return mapItems; } // Make copy
	void    GetItemsHex( ushort hx, ushort hy, ItemVec& items );
	void    GetItemsHexEx( ushort hx, ushort hy, uint radius, hash pid, ItemVec& items );
	void    GetItemsPid( hash pid, ItemVec& items );
	void    GetItemsTrigger( ushort hx, ushort hy, ItemVec& items );

	bool   IsPlaceForProtoItem( ushort hx, ushort hy, ProtoItem* proto_item );
	void   RecacheHexFlags( ushort hx, ushort hy );
	HexData GetHexFlags( ushort hx, ushort hy );
	void   SetHexFlag( ushort hx, ushort hy, uchar flag );
	void   UnsetHexFlag( ushort hx, ushort hy, uchar flag );

	bool IsHexPassed( ushort hx, ushort hy );
	bool IsHexRaked( ushort hx, ushort hy );
	bool IsHexesPassed( ushort hx, ushort hy, uint radius );
	bool IsMovePassed( ushort hx, ushort hy, uchar dir, uint multihex );
	bool IsHexTrigger( ushort hx, ushort hy ) { return HexFlags[ hy * GetWidth( ) + hx ].Trigger != 0; }
	bool IsHexCritter( ushort hx, ushort hy ) { return HexFlags[ hy * GetWidth( ) + hx ].Critter != 0 || HexFlags[ hy * GetWidth( ) + hx ].DeadCritter != 0; }
	bool IsHexGag( ushort hx, ushort hy ) { return HexFlags[ hy * GetWidth( ) + hx ].GagItem != 0; }
	bool IsHexStaticTrigger( ushort hx, ushort hy ) { return GetProtoMap( )->HexFlags[ hy * GetWidth( ) + hx ].StaticTrigger; }

	bool     IsFlagCritter( ushort hx, ushort hy, bool dead );
	void     SetFlagCritter( ushort hx, ushort hy, uint multihex, bool dead );
	void     UnsetFlagCritter( ushort hx, ushort hy, uint multihex, bool dead );
	uint     GetNpcCount( int npc_role, int find_type );
	Critter* GetCritter( uint crid );
	Critter* GetNpc( int npc_role, int find_type, uint skip_count );
	Critter* GetHexCritter( ushort hx, ushort hy, bool dead );
	void     GetCrittersHex( ushort hx, ushort hy, uint radius, int find_type, CrVec& critters ); // Critters append

	CrVec  GetCritters( );
	ClVec  GetPlayers( );
	PcVec  GetNpcs( );
	CrVec& GetCrittersRaw( ) { return mapCritters; }
	ClVec& GetPlayersRaw( ) { return mapPlayers; }
	PcVec& GetNpcsRaw( ) { return mapNpcs; }
	uint   GetCrittersCount( ) { return ( uint )mapCritters.size( ); }
	uint   GetPlayersCount( ) { return ( uint )mapPlayers.size( ); }
	uint   GetNpcsCount( ) { return ( uint )mapNpcs.size( ); }

	// Sends
	void SendEffect( hash eff_pid, ushort hx, ushort hy, ushort radius );
	void SendFlyEffect( hash eff_pid, uint from_crid, uint to_crid, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy );

	// Script
	bool SetScript( asIScriptFunction* func, bool first_time );
};
using MapMap = map< uint, Map* >;
using MapVec = vector< Map* >;

class Location: public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( CScriptArray *, MapProtos );    // hash[]
    CLASS_PROPERTY( CScriptArray *, MapEntrances ); // hash[]
    CLASS_PROPERTY( CScriptArray *, Automaps );     // hash[]
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

private:
    MapVec locMaps;

public:
    uint EntranceScriptBindId;
    int  GeckCount;

    void           BindScript();
    ProtoLocation* GetProtoLoc()  { return (ProtoLocation*) Proto; }
    bool           IsLocVisible() { return !GetHidden() || ( GetGeckVisible() && GeckCount > 0 ); }
    MapVec&        GetMapsRaw()   { return locMaps; };
    MapVec         GetMaps();
    uint           GetMapsCount() { return (uint) locMaps.size(); }
    Map*           GetMapByIndex( uint index );
    Map*           GetMapByPid( hash map_pid );
    uint           GetMapIndex( hash map_pid );
    bool           IsCanEnter( uint players_count );

    bool IsNoCrit();
    bool IsNoPlayer();
    bool IsNoNpc();
    bool IsCanDelete();
};
using LocMap = map< uint, Location* >;
using LocVec = vector< Location* >;

#endif // __MAP__
