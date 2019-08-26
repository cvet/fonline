#pragma once

#include "Common.h"
#include "Properties.h"
#include "MsgFiles.h"

enum class EntityType
{
    None = 0,
    EntityProto,
    LocationProto,
    MapProto,
    CritterProto,
    ItemProto,
    Custom,
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    Location,
    Map,
    Client,
    Npc,
    Item,
    #endif
    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
    LocationCl,
    MapCl,
    CritterCl,
    ItemCl,
    ItemHex,
    #endif
    Global,
    Max,
};

class Entity;
using EntityVec = vector< Entity* >;
using EntityMap = map< uint, Entity* >;
#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
class Location;
using LocVec = vector< Location* >;
using LocMap = map< uint, Location* >;
class Map;
using MapVec = vector< Map* >;
using MapMap = map< uint, Map* >;
class Critter;
using CrMap = map< uint, Critter* >;
using CrVec = vector< Critter* >;
class Item;
using ItemVec = vector< Item* >;
using ItemMap = map< uint, Item* >;
#endif
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
class LocationCl;
using LocClVec = vector< LocationCl* >;
using LocClMap = map< uint, LocationCl* >;
class MapCl;
using MapClVec = vector< MapCl* >;
using MapClMap = map< uint, MapCl* >;
class CritterCl;
using CrClMap = map< uint, CritterCl* >;
using CrClVec = vector< CritterCl* >;
class ItemCl;
using ItemClVec = vector< ItemCl* >;
using ItemClMap = map< uint, ItemCl* >;
#endif
class ProtoEntity;
using ProtoEntityVec = vector< ProtoEntity* >;
using ProtoEntityMap = map< hash, ProtoEntity* >;
class ProtoLocation;
using ProtoLocVec = vector< ProtoLocation* >;
using ProtoLocMap = map< hash, ProtoLocation* >;
class ProtoMap;
using ProtoMapVec = vector< ProtoMap* >;
using ProtoMapMap = map< hash, ProtoMap* >;
class ProtoCritter;
using ProtoCritterMap = map< hash, ProtoCritter* >;
using ProtoCritterVec = vector< ProtoCritter* >;
class ProtoItem;
using ProtoItemVec = vector< ProtoItem* >;
using ProtoItemMap = map< hash, ProtoItem* >;

class Entity
{
protected:
    Entity( uint id, EntityType type, PropertyRegistrator* registartor, ProtoEntity* proto );
    ~Entity();

public:
    Properties       Props;
    const uint       Id;
    const EntityType Type;
    ProtoEntity*     Proto;
    mutable long     RefCounter;
    bool             IsDestroyed;
    bool             IsDestroying;

    uint      GetId() const;
    void      SetId( uint id );
    hash      GetProtoId() const;
    string    GetName() const;
    EntityVec GetChildren() const;
    void      AddRef() const;
    void      Release() const;
};

class ProtoEntity: public Entity
{
protected:
    ProtoEntity( hash proto_id, EntityType type, PropertyRegistrator* registrator );

public:
    const hash ProtoId;
    UIntVec    TextsLang;
    FOMsgVec   Texts;
    HashSet    Components;
    #ifdef FONLINE_EDITOR
    string     CollectionName;
    #endif

    bool HaveComponent( hash name ) const;
};

class CustomEntity: public Entity
{
public:
    CustomEntity( uint id, uint sub_type, PropertyRegistrator* registrator );
    const uint SubType;
};

class GlobalVars: public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( ushort, YearStart );
    CLASS_PROPERTY( ushort, Year );
    CLASS_PROPERTY( ushort, Month );
    CLASS_PROPERTY( ushort, Day );
    CLASS_PROPERTY( ushort, Hour );
    CLASS_PROPERTY( ushort, Minute );
    CLASS_PROPERTY( ushort, Second );
    CLASS_PROPERTY( ushort, TimeMultiplier );
    CLASS_PROPERTY( uint, LastEntityId );
    CLASS_PROPERTY( uint, LastDeferredCallId );
    CLASS_PROPERTY( uint, HistoryRecordsId );

    GlobalVars();
};
extern GlobalVars* Globals;

class ProtoLocation: public ProtoEntity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( CScriptArray *, MapProtos );

    ProtoLocation( hash pid );
};

class ProtoMap: public ProtoEntity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( string, FileDir );
    CLASS_PROPERTY( ushort, Width );
    CLASS_PROPERTY( ushort, Height );
    CLASS_PROPERTY( ushort, WorkHexX );
    CLASS_PROPERTY( ushort, WorkHexY );
    CLASS_PROPERTY( int, CurDayTime );
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( CScriptArray *, DayTime );      // 4 int
    CLASS_PROPERTY( CScriptArray *, DayColor );     // 12 uchar
    CLASS_PROPERTY( bool, IsNoLogOut );

    // Entities
    #ifdef FONLINE_EDITOR
    EntityVec AllEntities;
    uint      LastEntityId;
    #endif

    // Tiles
    struct Tile         // 16 bytes
    {
        hash   Name;
        ushort HexX, HexY;
        short  OffsX, OffsY;
        uchar  Layer;
        bool   IsRoof;
        #ifdef FONLINE_EDITOR
        bool   IsSelected;
        #endif

        Tile() { memzero( this, sizeof( Tile ) ); }
        Tile( hash name, ushort hx, ushort hy, char ox, char oy, uchar layer, bool is_roof ): Name( name ), HexX( hx ), HexY( hy ), OffsX( ox ), OffsY( oy ), Layer( layer ), IsRoof( is_roof ) {}
    };
    typedef vector< Tile > TileVec;
    TileVec Tiles;

private:
    #ifdef FONLINE_EDITOR
    void SaveTextFormat( IniParser& file );
    #endif
    bool LoadTextFormat( const char* buf );
    bool LoadOldTextFormat( const char* buf );
    bool OnAfterLoad( EntityVec& entities );

    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
public:
    UCharVec SceneryData;
    hash     HashTiles;
    hash     HashScen;

    CrVec    CrittersVec;
    ItemVec  HexItemsVec;
    ItemVec  ChildItemsVec;
    ItemVec  StaticItemsVec;
    ItemVec  TriggerItemsVec;
    uchar*   HexFlags;

private:
    bool BindScripts( EntityVec& entities );
    #endif

public:
    bool Load();

    #ifdef FONLINE_EDITOR
    void        GenNew();
    bool        Save( const string& custom_name );
    static bool IsMapFile( const string& fname );
    #endif

    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    void  GetStaticItemTriggers( ushort hx, ushort hy, ItemVec& triggers );
    Item* GetStaticItem( ushort hx, ushort hy, hash pid );
    void  GetStaticItemsHex( ushort hx, ushort hy, ItemVec& items );
    void  GetStaticItemsHexEx( ushort hx, ushort hy, uint radius, hash pid, ItemVec& items );
    void  GetStaticItemsByPid( hash pid, ItemVec& items );
    #endif

    ProtoMap( hash pid );
    ~ProtoMap();
};

class ProtoCritter: public ProtoEntity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( uint, Multihex );

    ProtoCritter( hash pid );
};

class ProtoItem: public ProtoEntity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( hash, PicMap );
    CLASS_PROPERTY( hash, PicInv );
    CLASS_PROPERTY( bool, Stackable );
    CLASS_PROPERTY( short, OffsetX );
    CLASS_PROPERTY( short, OffsetY );
    CLASS_PROPERTY( uchar, Slot );
    CLASS_PROPERTY( char, LightIntensity );
    CLASS_PROPERTY( uchar, LightDistance );
    CLASS_PROPERTY( uchar, LightFlags );
    CLASS_PROPERTY( uint, LightColor );
    CLASS_PROPERTY( uint, Count );
    CLASS_PROPERTY( bool, IsFlat );
    CLASS_PROPERTY( char, DrawOrderOffsetHexY );
    CLASS_PROPERTY( int, Corner );
    CLASS_PROPERTY( bool, DisableEgg );
    CLASS_PROPERTY( bool, IsStatic );
    CLASS_PROPERTY( bool, IsScenery );
    CLASS_PROPERTY( bool, IsWall );
    CLASS_PROPERTY( bool, IsBadItem );
    CLASS_PROPERTY( bool, IsColorize );
    CLASS_PROPERTY( bool, IsShowAnim );
    CLASS_PROPERTY( bool, IsShowAnimExt );
    CLASS_PROPERTY( uchar, AnimStay0 );
    CLASS_PROPERTY( uchar, AnimStay1 );
    CLASS_PROPERTY( CScriptArray *, BlockLines );

    ProtoItem( hash pid );

    int64 InstanceCount;

    bool IsStatic()     { return GetIsStatic(); }
    bool IsAnyScenery() { return IsScenery() || IsWall(); }
    bool IsScenery()    { return GetIsScenery(); }
    bool IsWall()       { return GetIsWall(); }

    #if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
    uint GetCurSprId();
    #endif
};
