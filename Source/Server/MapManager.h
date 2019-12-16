#pragma once

#include "Common.h"
#include "Entity.h"

class ProtoManager;
class EntityManager;
class ItemManager;
class CritterManager;

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

struct TraceData
{
    // Input
    Map*        TraceMap = nullptr;
    ushort      BeginHx = 0;
    ushort      BeginHy = 0;
    ushort      EndHx = 0;
    ushort      EndHy = 0;
    uint        Dist = 0;
    float       Angle = 0.0f;
    Critter*    FindCr = nullptr;
    int         FindType = 0;
    bool        LastPassedSkipCritters = false;
    void        ( * HexCallback )( Map*, Critter*, ushort, ushort, ushort, ushort, uchar ) = nullptr;

    // Output
    CritterVec* Critters = nullptr;
    UShortPair* PreBlock = nullptr;
    UShortPair* Block = nullptr;
    UShortPair* LastPassed = nullptr;
    bool        IsFullTrace = false;
    bool        IsCritterFounded = false;
    bool        IsHaveLastPassed = false;
};

struct PathFindData
{
    uint     MapId = 0;
    ushort   MoveParams = 0;
    Critter* FromCritter = nullptr;
    ushort   FromX = 0;
    ushort   FromY = 0;
    ushort   ToX = 0;
    ushort   ToY = 0;
    ushort   NewToX = 0;
    ushort   NewToY = 0;
    uint     Multihex = 0;
    uint     Cut = 0;
    uint     PathNum = 0;
    uint     Trace = 0;
    bool     IsRun = false;
    bool     CheckCrit = false;
    bool     CheckGagItems = false;
    Critter* TraceCr = nullptr;
    Critter* GagCritter = nullptr;
    Item*    GagItem = nullptr;
};

struct PathStep
{
    ushort HexX = 0;
    ushort HexY = 0;
    uint   MoveParams = 0;
    uchar  Dir = 0;
};
using PathStepVec = vector< PathStep >;

class MapManager
{
public:
    MapManager( ProtoManager& proto_mngr, EntityManager& entity_mngr, CritterManager& cr_mngr, ItemManager& item_mngr );

    // Maps
    bool FindPlaceOnMap( Critter* cr, Map* map, ushort& hx, ushort& hy, uint radius );
    bool CanAddCrToMap( Critter* cr, Map* map, ushort hx, ushort hy, uint leader_id );
    void AddCrToMap( Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint leader_id );
    void EraseCrFromMap( Critter* cr, Map* map );
    bool TransitToGlobal( Critter* cr, uint leader_id, bool force );
    bool Transit( Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, uint leader_id, bool force );
    bool IsIntersectZone( int wx1, int wy1, int wx1_radius, int wx2, int wy2, int wx2_radius, int zones );
    void GetZoneLocations( int zx, int zy, int zone_radius, UIntVec& loc_ids );
    void KickPlayersToGlobalMap( Map* map );

    // Locations
    Location* CreateLocation( hash proto_id, ushort wx, ushort wy );
    bool      RestoreLocation( uint id, hash proto_id, const DataBase::Document& doc );
    Location* GetLocationByMap( uint map_id );
    Location* GetLocation( uint loc_id );
    Location* GetLocationByPid( hash loc_pid, uint skip_count );
    void      GetLocations( LocationVec& locs );
    uint      GetLocationsCount();
    void      LocationGarbager();
    void      DeleteLocation( Location* loc, ClientVec* gmap_players );

    // Maps
    Map*         CreateMap( hash proto_id, Location* loc );
    bool         RestoreMap( uint id, hash proto_id, const DataBase::Document& doc );
    void         RegenerateMap( Map* map );
    Map*         GetMap( uint map_id );
    Map*         GetMapByPid( hash map_pid, uint skip_count );
    void         GetMaps( MapVec& maps );
    uint         GetMapsCount();
    void         TraceBullet( TraceData& trace );
    int          FindPath( PathFindData& pfd );
    int          FindPathGrid( ushort& hx, ushort& hy, int index, bool smooth_switcher );
    PathStepVec& GetPath( uint num ) { return pathesPool[ num ]; }
    void         PathSetMoveParams( PathStepVec& path, bool is_run );

    void ProcessVisibleCritters( Critter* view_cr );
    void ProcessVisibleItems( Critter* view_cr );
    void ViewMap( Critter* view_cr, Map* map, int look, ushort hx, ushort hy, int dir );

    bool CheckKnownLocById( Critter* cr, uint loc_id );
    bool CheckKnownLocByPid( Critter* cr, hash loc_pid );
    void AddKnownLoc( Critter* cr, uint loc_id );
    void EraseKnownLoc( Critter* cr, uint loc_id );

    string GetLocationsMapsStatistics();

private:
    void GenerateMapContent( Map* map );
    void DeleteMapContent( Map* map );

    ProtoManager&   protoMngr;
    EntityManager&  entityMngr;
    CritterManager& crMngr;
    ItemManager&    itemMngr;

    bool            runGarbager = true;
    PathStepVec     pathesPool[ FPATH_DATA_SIZE ];
    uint            pathNumCur = 0;
    bool            smoothSwitcher = false;
};
