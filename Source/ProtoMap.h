#ifndef __PROTO_MAP__
#define __PROTO_MAP__

#include "Common.h"
#include "FileManager.h"

// Generic
#define MAPOBJ_SCRIPT_NAME       ( 25 )
#define MAPOBJ_CRITTER_PARAMS    ( 40 )

// Map object types
#define MAP_OBJECT_CRITTER       ( 0 )
#define MAP_OBJECT_ITEM          ( 1 )
#define MAP_OBJECT_SCENERY       ( 2 )

class ProtoMap;
class MapObject // Available in fonline.h
{
public:
    uchar  MapObjType;
    ushort ProtoId;
    ushort MapX;
    ushort MapY;
    short  Dir;

    uint   UID;
    uint   ContainerUID;
    uint   ParentUID;
    uint   ParentChildIndex;

    uint   LightColor;
    uchar  LightDay;
    uchar  LightDirOff;
    uchar  LightDistance;
    char   LightIntensity;

    char   ScriptName[ MAPOBJ_SCRIPT_NAME + 1 ];
    char   FuncName[ MAPOBJ_SCRIPT_NAME + 1 ];

    uint   Reserved[ 7 ];
    int    UserData[ 10 ];

    union
    {
        struct
        {
            uchar Cond;
            uint  Anim1;
            uint  Anim2;
            short ParamIndex[ MAPOBJ_CRITTER_PARAMS ];
            int   ParamValue[ MAPOBJ_CRITTER_PARAMS ];
        } MCritter;

        struct
        {
            short  OffsetX;
            short  OffsetY;
            uchar  AnimStayBegin;
            uchar  AnimStayEnd;
            ushort AnimWait;
            uchar  InfoOffset;
            uint   PicMapHash;
            uint   PicInvHash;

            uint   Count;
            uchar  ItemSlot;

            uchar  BrokenFlags;
            uchar  BrokenCount;
            ushort Deterioration;

            ushort AmmoPid;
            uint   AmmoCount;

            uint   LockerDoorId;
            ushort LockerCondition;
            ushort LockerComplexity;

            short  TrapValue;

            int    Val[ 10 ];
        } MItem;

        struct
        {
            short  OffsetX;
            short  OffsetY;
            uchar  AnimStayBegin;
            uchar  AnimStayEnd;
            ushort AnimWait;
            uchar  InfoOffset;
            uint   PicMapHash;
            uint   PicInvHash;

            bool   CanUse;
            bool   CanTalk;
            uint   TriggerNum;

            uchar  ParamsCount;
            int    Param[ 5 ];

            ushort ToMapPid;
            uint   ToEntire;
            uchar  ToDir;

            uchar  SpriteCut;
        } MScenery;
    };

    struct _RunTime
    {
        #ifdef FONLINE_MAPPER
        ProtoMap* FromMap;
        uint      MapObjId;
        char      PicMapName[ 64 ];
        char      PicInvName[ 64 ];
        #endif
        #ifdef FONLINE_SERVER
        int       BindScriptId;
        #endif
        long      RefCounter;
    } RunTime;

    MapObject()
    {
        memzero( this, sizeof( MapObject ) );
        RunTime.RefCounter = 1;
    }
    MapObject( const MapObject& r )
    {
        memcpy( this, &r, sizeof( MapObject ) );
        RunTime.RefCounter = 1;
    }
    MapObject& operator=( const MapObject& r )
    {
        memcpy( this, &r, sizeof( MapObject ) );
        RunTime.RefCounter = 1;
        return *this;
    }

    void AddRef()  { ++RunTime.RefCounter; }
    void Release() { if( !--RunTime.RefCounter ) delete this; }
};
typedef vector< MapObject* > MapObjectPtrVec;
typedef vector< MapObject >  MapObjectVec;

struct SceneryCl
{
    ushort ProtoId;
    uchar  Flags;
    uchar  SpriteCut;
    ushort MapX;
    ushort MapY;
    short  OffsetX;
    short  OffsetY;
    uint   LightColor;
    uchar  LightDistance;
    uchar  LightFlags;
    char   LightIntensity;
    uchar  InfoOffset;
    uchar  AnimStayBegin;
    uchar  AnimStayEnd;
    ushort AnimWait;
    uint   PicMapHash;
    short  Dir;
    ushort Reserved1;

	void AddRef( ) { }
	void Release( ) { }
};
typedef vector< SceneryCl > SceneryClVec;
typedef vector< SceneryCl* > SceneryClRefVec;

class ProtoMap
{
public:
    // Header
    struct
    {
        uint   Version;
        ushort MaxHexX, MaxHexY;
        int    WorkHexX, WorkHexY;
        char   ScriptModule[ MAX_SCRIPT_NAME + 1 ];
        char   ScriptFunc[ MAX_SCRIPT_NAME + 1 ];
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
    MapObjectPtrVec MObjects;
    uint            LastObjectUID;

    // Tiles
    struct Tile     // 12 bytes
    {
        uint   NameHash;
        ushort HexX, HexY;
        char   OffsX, OffsY;
        uchar  Layer;
        bool   IsRoof;
        #ifdef FONLINE_MAPPER
        bool   IsSelected;
        #endif

        Tile() { memzero( this, sizeof( Tile ) ); }
        Tile( uint name, ushort hx, ushort hy, char ox, char oy, uchar layer, bool is_roof ): NameHash( name ), HexX( hx ), HexY( hy ), OffsX( ox ), OffsY( oy ), Layer( layer ), IsRoof( is_roof ) {}
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
    bool ReadHeader( FileManager& fm, int version );
    bool ReadTiles( FileManager& fm, int version );
    bool ReadObjects( FileManager& fm, int version );
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
    void BindSceneryScript( MapObject* mobj );
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
        MapEntire( ushort hx, ushort hy, uchar dir, uint type ): HexX( hx ), HexY( hy ), Dir( dir ), Number( type ) {}
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
    int    pathType;
    string pmapName;
    ushort pmapPid;
    bool   isInit;

public:
    bool Init( ushort pid, const char* name, int path_type );
    void Clear();
    bool Refresh();

    #ifdef FONLINE_MAPPER
    void        GenNew();
    bool        Save( const char* fname, int path_type );
    static bool IsMapFile( const char* fname );
    #endif

    bool        IsInit()  { return isInit; }
    ushort      GetPid()  { return isInit ? pmapPid : 0; }
    const char* GetName() { return pmapName.c_str(); }

    long RefCounter;
    void AddRef()  { ++RefCounter; }
    void Release() { if( !--RefCounter ) delete this; }

    #ifdef FONLINE_SERVER
    MapObject* GetMapScenery( ushort hx, ushort hy, ushort pid );
    void       GetMapSceneriesHex( ushort hx, ushort hy, MapObjectPtrVec& mobjs );
    void       GetMapSceneriesHexEx( ushort hx, ushort hy, uint radius, ushort pid, MapObjectPtrVec& mobjs );
    void       GetMObjectsHexEx( ushort hx, ushort hy, uint radius, ushort pid, MapObjectPtrVec& mobjs );
    void       GetMapSceneriesByPid( ushort pid, MapObjectPtrVec& mobjs );

	void GetWalls( ushort hexX, ushort hexY, SceneryClRefVec& sceneries );
	void GetSceneryClients( ushort hexX, ushort hexY, SceneryClRefVec& sceneries );

    MapObject* GetMapGrid( ushort hx, ushort hy );
    ProtoMap(): isInit( false ), pathType( 0 ), HexFlags( NULL ) { MEMORY_PROCESS( MEMORY_PROTO_MAP, sizeof( ProtoMap ) ); }
    ProtoMap( const ProtoMap& r )
    {
        *this = r;
        MEMORY_PROCESS( MEMORY_PROTO_MAP, sizeof( ProtoMap ) );
    }
    ~ProtoMap()
    {
        isInit = false;
        HexFlags = NULL;
        MEMORY_PROCESS( MEMORY_PROTO_MAP, -(int) sizeof( ProtoMap ) );
    }
    #else
    ProtoMap(): isInit( false ), pathType( 0 ), RefCounter( 1 ) {}
    ~ProtoMap() { isInit = false; }
    #endif
};
typedef vector< ProtoMap >  ProtoMapVec;
typedef vector< ProtoMap* > ProtoMapPtrVec;

#endif // __PROTO_MAP__
