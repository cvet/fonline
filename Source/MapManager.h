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
	DWORD EncounterDescriptor;
	DWORD EncounterTick;
	bool EncounterForce;

	bool IsValid();
	bool IsMoving();
	DWORD GetSize();
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
	int Dist;
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
#define FPATH_ERROR1                    (10)
#define FPATH_INVALID_HEXES             (11)
#define FPATH_TRACE_FAIL                (12)
#define FPATH_TRACE_TARG_NULL_PTR       (13)

struct PathFindData 
{
	DWORD MapId;
	WORD MoveParams;
	WORD FromX,FromY;
	WORD ToX,ToY;
	WORD NewToX,NewToY;
	DWORD Cut;
	bool IsRun;
	DWORD PathNum;
	DWORD Trace;
	bool IsAround;
	bool CheckCrit;
	bool CheckGagItems;
	Critter* TraceCr;
	Critter* GagCritter;
	Item* GagItem;

	void Clear(){ZeroMemory(this,sizeof(PathFindData));MoveParams=0xFFFF;}
};

struct PathStep // 8 bytes
{
	WORD HexX;
	WORD HexY;
	WORD MoveParams;
	BYTE Dir;
	BYTE Reserved;
};
typedef vector<PathStep> PathStepVec;

class MapManager
{
private:
	bool active;
	FileManager fm;
	FileManager fmMsk;
	DWORD lastMapId;
	DWORD lastLocId;

public:
	ProtoMap ProtoMaps[MAX_PROTO_MAPS];
	ProtoLocation ProtoLoc[MAX_PROTO_LOCATIONS];

	MapManager();
	bool Init();
	void Finish();
	bool LoadLocationsProtos();
	bool LoadLocationProto(IniParser& city_txt, ProtoLocation& ploc, WORD pid);
	bool LoadMapsProtos();
	bool LoadMapProto(IniParser& maps_txt, ProtoMap& pmap, WORD pid);
	void SaveAllLocationsAndMapsFile(void(*save_func)(void*,size_t));
	bool LoadAllLocationsAndMapsFile(FILE* f);
	string GetLocationsMapsStatistics();
	void RunInitScriptMaps();
	void ProcessMaps();
	bool GenerateWorld(const char* fname, int path_type);

	// Maps stuff
public:
	bool AddCrToMap(Critter* cr, Map* map, WORD tx, WORD ty, DWORD radius);
	void EraseCrFromMap(Critter* cr, Map* map, WORD hex_x, WORD hex_y);
	bool TryTransitCrGrid(Critter* cr, Map* map, WORD hx, WORD hy, bool force);
	bool TransitToGlobal(Critter* cr, DWORD rule, BYTE follow_type, bool force);
	bool Transit(Critter* cr, Map* map, WORD hx, WORD hy, BYTE dir, DWORD radius, bool force);

	// Global map
private:
	GlobalMapZone gmZone[GM__MAXZONEX][GM__MAXZONEY];
	CByteMask* gmMask;

public:
	GlobalMapZone& GetZoneByCoord(WORD coord_x, WORD coord_y);
	GlobalMapZone& GetZone(WORD zone_x, WORD zone_y);

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
	bool GM_CheckEntrance(int bind_id, asIScriptArray* arr, BYTE entrance);
	asIScriptArray* GM_CreateGroupArray(GlobalMapGroup* group);

	// Locations
private:
	LocMap gameLoc;
	bool runGarbager;
	void DeleteLocation(DWORD loc_id);

public:
	bool IsInitProtoLocation(WORD pid_loc);
	ProtoLocation* GetProtoLoc(WORD loc_pid);
	Location* CreateLocation(WORD pid_loc, WORD wx, WORD wy, DWORD loc_id);
	Location* GetLocationByMap(DWORD map_id);
	Location* GetLocation(DWORD loc_id);
	Location* GetLocationByPid(WORD loc_pid, DWORD skip_count);
	Location* GetLocationByCoord(int wx, int wy);
	LocMap& GetLocations(){return gameLoc;}
	void RunLocGarbager(){runGarbager=true;}
	void LocationGarbager(DWORD cycle_tick);

	// Maps
private:
	MapMap allMaps;
	StrWordMap pmapsLoaded;
	PathStepVec pathesPool[FPATH_DATA_SIZE];
	DWORD pathNumCur;

public:
	bool IsInitProtoMap(WORD pid_map);
	Map* CreateMap(WORD pid_map, Location* loc_map, DWORD map_id);
	Map* GetMap(DWORD map_id);
	Map* GetMapByPid(WORD map_pid, DWORD skip_count);
	MapMap& GetAllMaps(){return allMaps;}
	ProtoMap* GetProtoMap(WORD pid_map);
	bool IsProtoMapNoLogOut(WORD pid_map);
	void TraceBullet(TraceData& trace);
	int FindPath(PathFindData& pfd);
	int FindPathGrid(WORD& hx, WORD& hy, int index, bool switcher);
	PathStepVec& GetPath(DWORD num){return pathesPool[num];}
	void PathSetMoveParams(PathStepVec& path, bool is_run);
};

extern MapManager MapMngr;

#endif // __MAP_MANAGER__