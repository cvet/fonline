#ifndef __MAP_MANAGER__
#define __MAP_MANAGER__

#include "Common.h"
#include "Map.h"
#include "Critter.h"
#include "Item.h"

struct TraceData
{
    // Input
    Map*        TraceMap;
    ushort      BeginHx, BeginHy;
    ushort      EndHx, EndHy;
    uint        Dist;
    float       Angle;
    Critter*    FindCr;
    int         FindType;
    bool        LastPassedSkipCritters;
    void        ( * HexCallback )( Map*, Critter*, ushort, ushort, ushort, ushort, uchar );
    // Output
    CrVec*      Critters;
    UShortPair* PreBlock;
    UShortPair* Block;
    UShortPair* LastPassed;
    bool        IsFullTrace;
    bool        IsCritterFounded;
    bool        IsHaveLastPassed;

    TraceData() { memzero( this, sizeof( TraceData ) ); }
};

// Path
#define FPATH_DATA_SIZE              ( 10000 )
#define FPATH_MAX_PATH               ( 400 )
#define FPATH_OK                     ( 0 )
#define FPATH_ALREADY_HERE           ( 2 )
#define FPATH_MAP_NOT_FOUND          ( 5 )
#define FPATH_HEX_BUSY               ( 6 )
#define FPATH_HEX_BUSY_RING          ( 7 )
#define FPATH_TOOFAR                 ( 8 )
#define FPATH_DEADLOCK               ( 9 )
#define FPATH_ERROR                  ( 10 )
#define FPATH_INVALID_HEXES          ( 11 )
#define FPATH_TRACE_FAIL             ( 12 )
#define FPATH_TRACE_TARG_NULL_PTR    ( 13 )

struct PathFindData
{
    uint     MapId;
    ushort   MoveParams;
    Critter* FromCritter;
    ushort   FromX, FromY;
    ushort   ToX, ToY;
    ushort   NewToX, NewToY;
    uint     Multihex;
    uint     Cut;
    uint     PathNum;
    uint     Trace;
    bool     IsRun;
    bool     CheckCrit;
    bool     CheckGagItems;
    Critter* TraceCr;
    Critter* GagCritter;
    Item*    GagItem;

    void     Clear()
    {
        memzero( this, sizeof( PathFindData ) );
        MoveParams = 0xFFFF;
    }
};

struct PathStep
{
    ushort HexX;
    ushort HexY;
    uint   MoveParams;
    uchar  Dir;
};
typedef vector< PathStep > PathStepVec;

class MapManager
{
public:
    MapManager();

    string GetLocationsMapsStatistics();

    // Maps stuff
public:
    bool FindPlaceOnMap( Critter* cr, Map* map, ushort& hx, ushort& hy, uint radius );
    bool CanAddCrToMap( Critter* cr, Map* map, ushort hx, ushort hy, uint leader_id );
    void AddCrToMap( Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint leader_id );
    void EraseCrFromMap( Critter* cr, Map* map );
    bool TransitToMapHex( Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, bool force );
    bool TransitToGlobal( Critter* cr, uint leader_id, bool force );
    bool Transit( Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, uint leader_id, bool force );
    bool IsIntersectZone( int wx1, int wy1, int wx1_radius, int wx2, int wy2, int wx2_radius, int zones );
    void GetZoneLocations( int zx, int zy, int zone_radius, UIntVec& loc_ids );

    // Locations
private:
    volatile bool runGarbager;

public:
    Location* CreateLocation( hash proto_id, ushort wx, ushort wy );
    bool      RestoreLocation( uint id, hash proto_id, const StrMap& props_data );
    Location* GetLocationByMap( uint map_id );
    Location* GetLocation( uint loc_id );
    Location* GetLocationByPid( hash loc_pid, uint skip_count );
    void      GetLocations( LocVec& locs );
    uint      GetLocationsCount();
    void      LocationGarbager();
    void      DeleteLocation( Location* loc, ClVec* gmap_players );
    void      RunGarbager() { runGarbager = true; }

    // Maps
private:
    PathStepVec pathesPool[ FPATH_DATA_SIZE ];
    uint        pathNumCur;

public:
    Map*         CreateMap( hash proto_id, Location* loc );
    bool         RestoreMap( uint id, hash proto_id, const StrMap& props_data );
    Map*         GetMap( uint map_id );
    Map*         GetMapByPid( hash map_pid, uint skip_count );
    void         GetMaps( MapVec& maps );
    uint         GetMapsCount();
    void         TraceBullet( TraceData& trace );
    int          FindPath( PathFindData& pfd );
    int          FindPathGrid( ushort& hx, ushort& hy, int index, bool smooth_switcher );
    PathStepVec& GetPath( uint num ) { return pathesPool[ num ]; }
    void         PathSetMoveParams( PathStepVec& path, bool is_run );
};

extern MapManager MapMngr;

#endif // __MAP_MANAGER__
