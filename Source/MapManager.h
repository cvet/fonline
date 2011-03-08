#ifndef __MAP_MANAGER__
#define __MAP_MANAGER__

#include "Defines.h"
#include "Map.h"

class GlobalMapGroup
{
public:
	CrVec CritMove;
	Critter* Rule;
	DWORD CarId;
	int WXi,WYi;
	float WXf,WYf;
	int MoveX,MoveY;
	float SpeedX,SpeedY;
	bool IsSetMove;
	DWORD TimeCanFollow;
	DWORD NextEncaunter;
	bool IsMultiply;
	DWORD MoveLastTick;
	DWORD ProcessLastTick;
	DWORD EncounterDescriptor;
	DWORD EncounterTick;
	bool EncounterForce;

	bool IsValid();
	bool IsMoving();
	DWORD GetSize();
	void SyncLockGroup();
	Critter* GetCritter(DWORD crid);
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
	WORD BeginHx,BeginHy;
	WORD EndHx,EndHy;
	DWORD Dist;
	float Angle;
	Critter* FindCr;
	int FindType;
	bool IsCheckTeam;
	DWORD BaseCrTeamId;
	bool LastPassedSkipCritters;
	bool (*HexCallback)(Map*, Critter*, WORD, WORD, WORD, WORD, BYTE);
	// Output
	CrVec* Critters;
	WordPair* PreBlock;
	WordPair* Block;
	WordPair* LastPassed;
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
	DWORD MapId;
	WORD MoveParams;
	Critter* FromCritter;
	WORD FromX,FromY;
	WORD ToX,ToY;
	WORD NewToX,NewToY;
	DWORD Multihex;
	DWORD Cut;
	DWORD PathNum;
	DWORD Trace;
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
	WORD HexX;
	WORD HexY;
	DWORD MoveParams;
	BYTE Dir;
};
typedef vector<PathStep> PathStepVec;

class MapManager
{
private:
	DWORD lastMapId;
	DWORD lastLocId;
	Mutex mapLocker;

public:
	ProtoMap ProtoMaps[MAX_PROTO_MAPS];
	ProtoLocation ProtoLoc[MAX_PROTO_LOCATIONS];

	MapManager();
	bool Init();
	void Finish();
	void Clear();

	bool LoadLocationsProtos();
	bool LoadLocationProto(IniParser& city_txt, ProtoLocation& ploc, WORD pid);
	void SaveAllLocationsAndMapsFile(void(*save_func)(void*,size_t));
	bool LoadAllLocationsAndMapsFile(FILE* f);
	string GetLocationsMapsStatistics();
	void RunInitScriptMaps();
	bool GenerateWorld(const char* fname, int path_type);
	void GetLocationAndMapIds(DwordSet& loc_ids, DwordSet& map_ids);

	// Maps stuff
public:
	bool AddCrToMap(Critter* cr, Map* map, WORD tx, WORD ty, DWORD radius);
	void EraseCrFromMap(Critter* cr, Map* map, WORD hex_x, WORD hex_y);
	bool TryTransitCrGrid(Critter* cr, Map* map, WORD hx, WORD hy, bool force);
	bool TransitToGlobal(Critter* cr, DWORD rule, BYTE follow_type, bool force);
	bool Transit(Critter* cr, Map* map, WORD hx, WORD hy, BYTE dir, DWORD radius, bool force);

	// Global map
private:
	CByteMask* gmMask;

public:
	bool IsIntersectZone(int wx1, int wy1, int wx1_radius, int wx2, int wy2, int wx2_radius, int zones);
	void GetZoneLocations(int zx, int zy, int zone_radius, DwordVec& loc_ids);

	bool RefreshGmMask(const char* mask_path);
	int GetGmRelief(DWORD x, DWORD y){return gmMask->GetByte(x,y)&0xF;}

	void GM_GroupStartMove(Critter* cr, bool send);
	void GM_AddCritToGroup(Critter* cr, DWORD rule_id);
	void GM_LeaveGroup(Critter* cr);
	void GM_GiveRule(Critter* cr, Critter* new_rule);
	void GM_StopGroup(Critter* cr);
	bool GM_GroupToMap(GlobalMapGroup* group, Map* map, DWORD entire, WORD mx, WORD my, BYTE mdir);
	bool GM_GroupToLoc(Critter* rule, DWORD loc_id, BYTE entrance, bool force = false);
	void GM_GroupSetMove(GlobalMapGroup* group, int gx, int gy, DWORD speed);
	void GM_GroupMove(GlobalMapGroup* group);
	void GM_GlobalProcess(Critter* cr, GlobalMapGroup* group, int type);
	void GM_GlobalInvite(GlobalMapGroup* group, int combat_mode);
	void GM_GroupScanZone(GlobalMapGroup* group, int zx, int zy);
	bool GM_CheckEntrance(Location* loc, CScriptArray* arr, BYTE entrance);
	CScriptArray* GM_CreateGroupArray(GlobalMapGroup* group);

	// Locations
private:
	LocMap allLocations;
	volatile bool runGarbager;

public:
	bool IsInitProtoLocation(WORD pid_loc);
	ProtoLocation* GetProtoLocation(WORD loc_pid);
	Location* CreateLocation(WORD pid_loc, WORD wx, WORD wy, DWORD loc_id);
	Location* GetLocationByMap(DWORD map_id);
	Location* GetLocation(DWORD loc_id);
	Location* GetLocationByPid(WORD loc_pid, DWORD skip_count);
	bool IsLocationOnCoord(int wx, int wy);
	void GetLocations(LocVec& locs, bool lock);
	DWORD GetLocationsCount();
	void LocationGarbager();
	void RunGarbager(){runGarbager=true;}

	// Maps
private:
	MapMap allMaps;
	PathStepVec pathesPool[FPATH_DATA_SIZE];
	DWORD pathNumCur;

public:
	bool IsInitProtoMap(WORD pid_map);
	Map* CreateMap(WORD pid_map, Location* loc_map, DWORD map_id);
	Map* GetMap(DWORD map_id, bool sync_lock = true);
	Map* GetMapByPid(WORD map_pid, DWORD skip_count);
	void GetMaps(MapVec& maps, bool lock);
	DWORD GetMapsCount();
	ProtoMap* GetProtoMap(WORD pid_map);
	bool IsProtoMapNoLogOut(WORD pid_map);
	void TraceBullet(TraceData& trace);
	int FindPath(PathFindData& pfd);
	int FindPathGrid(WORD& hx, WORD& hy, int index, bool smooth_switcher);
	PathStepVec& GetPath(DWORD num){return pathesPool[num];}
	void PathSetMoveParams(PathStepVec& path, bool is_run);
};

extern MapManager MapMngr;

#endif // __MAP_MANAGER__