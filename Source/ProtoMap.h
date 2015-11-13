#ifndef __PROTO_MAP__
#define __PROTO_MAP__

#include "Common.h"
#include "FileManager.h"

typedef vector< ScriptString* > ScriptStringVec;

// Generic
#define MAPOBJ_SCRIPT_NAME    ( 128 )

// Map object types
#define MAP_OBJECT_CRITTER    ( 0 )
#define MAP_OBJECT_ITEM       ( 1 )
#define MAP_OBJECT_SCENERY    ( 2 )

class ProtoMap;
class MapObject // Available in fonline.h
{
public:
    uchar         MapObjType;
    hash          ProtoId;
    #ifdef FONLINE_MAPPER
    ScriptString* ProtoName;
    #endif
    ushort        MapX;
    ushort        MapY;

    uint          UID;
    uint          ContainerUID;
    uint          ParentUID;
    uint          ParentChildIndex;

    uint          LightColor;
    uchar         LightDay;
    uchar         LightDirOff;
    uchar         LightDistance;
    char          LightIntensity;

    char          ScriptName[ MAPOBJ_SCRIPT_NAME ];
    char          FuncName[ MAPOBJ_SCRIPT_NAME ];

    int           UserData[ 10 ];

    #ifdef FONLINE_MAPPER
    ScriptStringVec* Props;
    #else
    IntVec*          Props;
    #endif

    union
    {
        struct
        {
            uchar Dir;
            uchar Cond;
            uint  Anim1;
            uint  Anim2;
        } MCritter;

        struct
        {
            short         OffsetX;
            short         OffsetY;
            hash          PicMap;
            hash          PicInv;

            uint          Count;
            uchar         ItemSlot;

            uchar         BrokenFlags;
            uchar         BrokenCount;
            ushort        Deterioration;

            #ifdef FONLINE_MAPPER
            ScriptString* AmmoPid;
            #else
            hash          AmmoPid;
            #endif
            uint          AmmoCount;

            uint          LockerDoorId;
            ushort        LockerCondition;
            ushort        LockerComplexity;

            short         TrapValue;

            int           Val[ 10 ];
        } MItem;

        struct
        {
            short         OffsetX;
            short         OffsetY;
            hash          PicMap;
            hash          PicInv;

            bool          CanUse;
            bool          CanTalk;
            uint          TriggerNum;

            uchar         ParamsCount;
            int           Param[ 5 ];

            #ifdef FONLINE_MAPPER
            ScriptString* ToMap;
            #else
            hash          ToMap;
            #endif
            uint          ToEntire;
            uchar         ToDir;

            uchar         SpriteCut;
        } MScenery;
    };

    struct _RunTime
    {
        #ifdef FONLINE_MAPPER
        ProtoMap* FromMap;
        uint      MapObjId;
        char      PicMapName[ 64 ];
        char      PicInvName[ 64 ];
        #else
        uint      BindScriptId;
        #endif
        long      RefCounter;
    } RunTime;

    MapObject()
    {
        memzero( this, sizeof( MapObject ) );
        RunTime.RefCounter = 1;
    }

    ~MapObject()
    {
        if( Props )
        {
            #ifdef FONLINE_MAPPER
            for( size_t i = 0; i < Props->size(); i++ )
                Props->at( i )->Release();
            #endif
            SAFEDEL( Props );
        }
    }

    void AddRef()  { ++RunTime.RefCounter; }
    void Release() { if( !--RunTime.RefCounter ) delete this; }

    void AllocateProps()
    {
        #ifdef FONLINE_MAPPER
        Props = new ScriptStringVec();
        #else
        Props = new IntVec();
        #endif
    }

    #ifdef FONLINE_MAPPER
    MapObject( const MapObject& r )
    {
        memzero( this, sizeof( MapObject ) );
        CopyObject( r );
        RunTime.RefCounter = 1;
    }

    MapObject& operator=( const MapObject& r )
    {
        CopyObject( r );
        return *this;
    }

    void CopyObject( const MapObject& other )
    {
        SAFEDEL( Props );
        SAFEREL( ProtoName );
        if( MapObjType == MAP_OBJECT_ITEM )
            SAFEREL( MItem.AmmoPid );
        if( MapObjType == MAP_OBJECT_SCENERY )
            SAFEREL( MScenery.ToMap );

        int ref_count = RunTime.RefCounter;
        memcpy( this, &other, sizeof( MapObject ) );
        RunTime.RefCounter = ref_count;

        if( Props )
        {
            AllocateProps();
            for( size_t i = 0; i < Props->size(); i++ )
                *Props->at( i ) = *other.Props->at( i );
        }
        if( MapObjType == MAP_OBJECT_ITEM && MItem.AmmoPid )
            MItem.AmmoPid = ScriptString::Create( *MItem.AmmoPid );
        if( MapObjType == MAP_OBJECT_SCENERY && MScenery.ToMap )
            MScenery.ToMap = ScriptString::Create( *MScenery.ToMap );

        RunTime.RefCounter = 1;
    }

    ScriptString* GetPropValue( const char* prop )
    {
        if( !Props )
            AllocateProps();
        for( size_t i = 0; i < Props->size(); i += 2 )
            if( Str::Compare( Props->at( i )->c_str(), prop ) )
                return Props->at( i + 1 );
        Props->push_back( ScriptString::Create( prop ) );
        Props->push_back( ScriptString::Create() );
        return Props->back();
    }
    #else
private:
    MapObject( const MapObject& r ) {}
    MapObject& operator=( const MapObject& r ) { return *this; }
    #endif
};
typedef vector< MapObject* > MapObjectPtrVec;

struct SceneryCl
{
    hash   ProtoId;
    hash   PicMap;
    ushort MapX;
    ushort MapY;
    short  OffsetX;
    short  OffsetY;
    uint   LightColor;
    uchar  LightDistance;
    uchar  LightFlags;
    char   LightIntensity;
    uchar  Flags;
    uchar  SpriteCut;
};
typedef vector< SceneryCl > SceneryClVec;

class ProtoMap
{
public:
    // Header
    struct
    {
        uint   Version;
        ushort MaxHexX, MaxHexY;
        int    WorkHexX, WorkHexY;
        char   ScriptModule[ 65 ];
        char   ScriptFunc[ 65 ];
        int    Time;
        bool   NoLogOut;
        int    DayTime[ 4 ];
        uchar  DayColor[ 12 ];

        // Deprecated
        ushort HeaderSize;
        bool   Packed;
        uint   UnpackedDataLen;
    } Header;

    // Objects
    #ifdef FONLINE_MAPPER
    MapObjectPtrVec MObjects;
    uint            LastObjectUID;
    #endif

    // Tiles
    struct Tile     // 16 bytes
    {
        hash   Name;
        ushort HexX, HexY;
        short  OffsX, OffsY;
        uchar  Layer;
        bool   IsRoof;
        #ifdef FONLINE_MAPPER
        bool   IsSelected;
        #endif

        Tile() { memzero( this, sizeof( Tile ) ); }
        Tile( hash name, ushort hx, ushort hy, char ox, char oy, uchar layer, bool is_roof ): Name( name ), HexX( hx ), HexY( hy ), OffsX( ox ), OffsY( oy ), Layer( layer ), IsRoof( is_roof ) {}
    };
    typedef vector< Tile >    TileVec;
    TileVec Tiles;
    #ifdef FONLINE_MAPPER
    // For fast access
    typedef vector< TileVec > TileVecVec;
    TileVecVec TilesField;
    TileVecVec RoofsField;
    TileVec&   GetTiles( ushort hx, ushort hy, bool is_roof ) { return is_roof ? RoofsField[ hy * Header.MaxHexX + hx ] : TilesField[ hy * Header.MaxHexX + hx ]; }
    #endif

private:
    bool LoadTextFormat( const char* buf );
    #ifdef FONLINE_MAPPER
    void SaveTextFormat( FileManager& fm );
    #endif

    #ifdef FONLINE_SERVER
public:
    // To Client
    SceneryClVec    WallsToSend;
    SceneryClVec    SceneriesToSend;
    uint            HashTiles;
    uint            HashWalls;
    uint            HashScen;

    MapObjectPtrVec CrittersVec;
    MapObjectPtrVec ItemsVec;
    MapObjectPtrVec SceneryVec;
    MapObjectPtrVec GridsVec;
    uchar*          HexFlags;

private:
    bool LoadCache( FileManager& fm );
    void SaveCache( FileManager& fm );
    bool BindScripts( MapObjectPtrVec& mobjs );
    #endif

public:
    // Entires
    struct MapEntire
    {
        uint   Number;
        ushort HexX;
        ushort HexY;
        uchar  Dir;

        MapEntire() { memzero( this, sizeof( MapEntire ) ); }
        MapEntire( ushort hx, ushort hy, uchar dir, uint type ): Number( type ), HexX( hx ), HexY( hy ), Dir( dir ) {}
    };
    typedef vector< MapEntire > EntiresVec;

private:
    EntiresVec mapEntires;

public:
    uint       CountEntire( uint num );
    MapEntire* GetEntire( uint num, uint skip );
    MapEntire* GetEntireRandom( uint num );
    MapEntire* GetEntireNear( uint num, ushort hx, ushort hy );
    MapEntire* GetEntireNear( uint num, uint num_ext, ushort hx, ushort hy );
    void       GetEntires( uint num, EntiresVec& entires );

private:
    string pmapName;
    string pmapDir;
    hash   pmapPid;

public:
    bool Init( const char* name );
    void Clear();
    bool Refresh();

    #ifdef FONLINE_MAPPER
    void        GenNew();
    bool        Save( const char* custom_name = nullptr );
    static bool IsMapFile( const char* fname );
    #endif

    const char* GetName()    { return pmapName.c_str(); }
    hash        GetProtoId() { return pmapPid; }

    long RefCounter;
    void AddRef()  { ++RefCounter; }
    void Release() { if( !--RefCounter ) delete this; }

    #ifdef FONLINE_SERVER
    MapObject* GetMapScenery( ushort hx, ushort hy, hash pid );
    void       GetMapSceneriesHex( ushort hx, ushort hy, MapObjectPtrVec& mobjs );
    void       GetMapSceneriesHexEx( ushort hx, ushort hy, uint radius, hash pid, MapObjectPtrVec& mobjs );
    void       GetMapSceneriesByPid( hash pid, MapObjectPtrVec& mobjs );
    MapObject* GetMapGrid( ushort hx, ushort hy );
    ProtoMap(): HexFlags( nullptr ) { MEMORY_PROCESS( MEMORY_PROTO_MAP, sizeof( ProtoMap ) ); }
    ProtoMap( const ProtoMap& r )
    {
        *this = r;
        MEMORY_PROCESS( MEMORY_PROTO_MAP, sizeof( ProtoMap ) );
    }
    ~ProtoMap()
    {
        HexFlags = nullptr;
        MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) sizeof( ProtoMap ) );
    }
    #else
    ProtoMap(): RefCounter( 1 ) {}
    ~ProtoMap() {}
    #endif
};
typedef vector< ProtoMap* >    ProtoMapVec;
typedef map< hash, ProtoMap* > ProtoMapMap;

#endif // __PROTO_MAP__
