#ifndef __MAP_MANAGER__
#define __MAP_MANAGER__

#include "Defines.h"
#include "Map.h"

class GlobalMapGroup
{
public:
	CrVec CritMove;
	Critter* Rule;
	uint CarId;
	int WXi,WYi;
	float WXf,WYf;
	int MoveX,MoveY;
	float SpeedX,SpeedY;
	bool IsSetMove;
	uint TimeCanFollow;
	uint NextEncaunter;
	bool IsMultiply;
	uint MoveLastTick;
	uint ProcessLastTick;
	uint EncounterDescriptor;
	uint EncounterTick;
	bool EncounterForce;

	bool IsValid();
	bool IsMoving();
	uint GetSize();
	void SyncLockGroup();
	Critter* GetCritter(uint crid);
	Item* GetCar();
	bool CheckForFollow(Critter* cr);
	void AddCrit(Critter* cr);
	void EraseCrit(Critter* cr);
	void StartEncaunterTime(int time);
	bool IsEncaunterTime();
	void Clear();
	GlobalMapGroup(){Clear();}
};
typedef vector<GlobalMapGroup*> GMapGroupVec;
typedef vector<GlobalMapGroup*>::iterator GMapGroupVecIt;

struct TraceData
{
	// Input
	Map* TraceMap;
	ushort BeginHx,BeginHy;
	ushort EndHx,EndHy;
	uint Dist;
	float Angle;
	Critter* FindCr;
	int FindType;
	bool IsCheckTeam;
	uint BaseCrTeamId;
	bool LastPassedSkipCritters;
	bool (*HexCallback)(Map*, Critter*, ushort, ushort, ushort, ushort, uchar);
	// Output
	CrVec* Critters;
	UShortPair* PreBlock;
	UShortPair* Block;
	UShortPair* LastPassed;
	bool IsFullTrace;
	bool IsCritterFounded;
	bool IsHaveLastPassed;
	bool IsTeammateFounded;

	TraceData(){ZeroMemory(this,sizeof(TraceData));}
};

// Path
#define FPATH_DATA_SIZE                 (10000)
#define FPATH_MAX_PATH                  (400)
#define FPATH_OK                        (0)
#define FPATH_ALREADY_HERE              (2)
#define FPATH_MAP_NOT_FOUND             (5)
#define FPATH_HEX_BUSY                  (6)
#define FPATH_HEX_BUSY_RING             (7)
#define FPATH_TOOFAR                    (8)
#define FPATH_DEADLOCK                  (9)
#define FPATH_ERROR                     (10)
#define FPATH_INVALID_HEXES             (11)
#define FPATH_TRACE_FAIL                (12)
#define FPATH_TRACE_TARG_NULL_PTR       (13)
#define FPATH_ALLOC_FAIL                (14)

struct PathFindData
{
	uint MapId;
	ushort MoveParams;
	Critter* FromCritter;
	ushort FromX,FromY;
	ushort ToX,ToY;
	ushort NewToX,NewToY;
	uint Multihex;
	uint Cut;
	uint PathNum;
	uint Trace;
	bool IsRun;
	bool CheckCrit;
	bool CheckGagItems;
	Critter* TraceCr;
	Critter* GagCritter;
	Item* GagItem;

	void Clear(){ZeroMemory(this,sizeof(PathFindData));MoveParams=0xFFFF;}
};

struct PathStep
{
	ushort HexX;
	ushort HexY;
	uint MoveParams;
	uchar Dir;
};
typedef vector<PathStep> PathStepVec;

class MapManager
{
private:
	uint lastMapId;
	uint lastLocId;
	Mutex mapLocker;

public:
	ProtoMap ProtoMaps[MAX_PROTO_MAPS];
	ProtoLocation ProtoLoc[MAX_PROTO_LOCATIONS];

	MapManager();
	bool Init();
	void Finish();
	void Clear();

	bool LoadLocationsProtos();
	bool LoadLocationProto(IniParser& city_txt, ProtoLocation& ploc, ushort pid);
	void SaveAllLocationsAndMapsFile(void(*save_func)(void*,size_t));
	bool LoadAllLocationsAndMapsFile(FILE* f);
	string GetLocationsMapsStatistics();
	void RunInitScriptMaps();
	bool GenerateWorld(const char* fname, int path_type);
	void GetLocationAndMapIds(UIntSet& loc_ids, UIntSet& map_ids);

	// Maps stuff
public:
	bool AddCrToMap(Critter* cr, Map* map, ushort tx, ushort ty, uint radius);
	void EraseCrFromMap(Critter* cr, Map* map, ushort hex_x, ushort hex_y);
	bool TryTransitCrGrid(Critter* cr, Map* map, ushort hx, ushort hy, bool force);
	bool TransitToGlobal(Critter* cr, uint rule, uchar follow_type, bool force);
	bool Transit(Critter* cr, Map* map, ushort hx, ushort hy, uchar dir, uint radius, bool force);

	// Global map
private:
	CByteMask* gmMask;

public:
	bool IsIntersectZone(int wx1, int wy1, int wx1_radius, int wx2, int wy2, int wx2_radius, int zones);
	void GetZoneLocations(int zx, int zy, int zone_radius, UIntVec& loc_ids);

	bool RefreshGmMask(const char* mask_path);
	int GetGmRelief(uint x, uint y){return gmMask->GetByte(x,y)&0xF;}

	void GM_GroupStartMove(Critter* cr, bool send);
	void GM_AddCritToGroup(Critter* cr, uint rule_id);
	void GM_LeaveGroup(Critter* cr);
	void GM_GiveRule(Critter* cr, Critter* new_rule);
	void GM_StopGroup(Critter* cr);
	bool GM_GroupToMap(GlobalMapGroup* group, Map* map, uint entire, ushort mx, ushort my, uchar mdir);
	bool GM_GroupToLoc(Critter* rule, uint loc_id, uchar entrance, bool force = false);
	void GM_GroupSetMove(GlobalMapGroup* group, int gx, int gy, uint speed);
	void GM_GroupMove(GlobalMapGroup* group);
	void GM_GlobalProcess(Critter* cr, GlobalMapGroup* group, int type);
	void GM_GlobalInvite(GlobalMapGroup* group, int combat_mode);
	void GM_GroupScanZone(GlobalMapGroup* group, int zx, int zy);
	bool GM_CheckEntrance(Location* loc, CScriptArray* arr, uchar entrance);
	CScriptArray* GM_CreateGroupArray(GlobalMapGroup* group);

	// Locations
private:
	LocMap allLocations;
	volatile bool runGarbager;

public:
	bool IsInitProtoLocation(ushort pid_loc);
	ProtoLocation* GetProtoLocation(ushort loc_pid);
	Location* CreateLocation(ushort pid_loc, ushort wx, ushort wy, uint loc_id);
	Location* GetLocationByMap(uint map_id);
	Location* GetLocation(uint loc_id);
	Location* GetLocationByPid(ushort loc_pid, uint skip_count);
	bool IsLocationOnCoord(int wx, int wy);
	void GetLocations(LocVec& locs, bool lock);
	uint GetLocationsCount();
	void LocationGarbager();
	void RunGarbager(){runGarbager=true;}

	// Maps
private:
	MapMap allMaps;
	PathStepVec pathesPool[FPATH_DATA_SIZE];
	uint pathNumCur;

public:
	bool IsInitProtoMap(ushort pid_map);
	Map* CreateMap(ushort pid_map, Location* loc_map, uint map_id);
	Map* GetMap(uint map_id, bool sync_lock = true);
	Map* GetMapByPid(ushort map_pid, uint skip_count);
	void GetMaps(MapVec& maps, bool lock);
	uint GetMapsCount();
	ProtoMap* GetProtoMap(ushort pid_map);
	bool IsProtoMapNoLogOut(ushort pid_map);
	void TraceBullet(TraceData& trace);
	int FindPath(PathFindData& pfd);
	int FindPathGrid(ushort& hx, ushort& hy, int index, bool smooth_switcher);
	PathStepVec& GetPath(uint num){return pathesPool[num];}
	void PathSetMoveParams(PathStepVec& path, bool is_run);
};

extern MapManager MapMngr;

#endif // __MAP_MANAGER__