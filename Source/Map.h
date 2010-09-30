#ifndef __MAP__
#define __MAP__

#include "Common.h"
#include "ProtoMap.h"
#include "Item.h"
#include "Critter.h"

// Script events
#define MAP_EVENT_FINISH              (0)
#define MAP_EVENT_LOOP_0              (1)
#define MAP_EVENT_LOOP_1              (2)
#define MAP_EVENT_LOOP_2              (3)
#define MAP_EVENT_LOOP_3              (4)
#define MAP_EVENT_LOOP_4              (5)
#define MAP_EVENT_IN_CRITTER          (6)
#define MAP_EVENT_OUT_CRITTER         (7)
#define MAP_EVENT_CRITTER_DEAD        (8)
#define MAP_EVENT_TURN_BASED_BEGIN    (9)
#define MAP_EVENT_TURN_BASED_END      (10)
#define MAP_EVENT_TURN_BASED_PROCESS  (11)
#define MAP_EVENT_MAX                 (12)
extern const char* MapEventFuncName[MAP_EVENT_MAX];

// Loop times
#define MAP_LOOP_FUNC_MAX       (5)
#define MAP_LOOP_DEFAULT_TICK   (60*60000)
#define MAP_MAX_DATA            (100)

// Encaunters
#define ENCOUNTERS_TIME			(5000)

// Encaunters District
#define DISTRICT_WASTELAND      (0x01)
#define DISTRICT_MOUNTAINS      (0x02)
#define DISTRICT_FOREST         (0x04)
#define DISTRICT_CITY           (0x08)
#define DISTRICT_SHORELINE      (0x10)
#define DISTRICT_OCEAN          (0x20)

class Map;
class Location;

class Map
{
public:
	SyncObject Sync;

private:
	BYTE* hexFlags;
	CrVec mapCritters;
	ClVec mapPlayers;
	PcVec mapNpcs;
	ItemPtrVec hexItems;
	Location* mapLocation;

public:
	struct MapData
	{
		DWORD MapId;
		WORD MapPid;
		BYTE MapRain;
		bool IsTurnBasedAviable;
		int MapTime;
		DWORD ScriptId;
		int MapDayTime[4];
		BYTE MapDayColor[12];
		DWORD Reserved[20];
		int UserData[MAP_MAX_DATA];
	} Data;

	ProtoMap* Proto;

	bool NeedProcess;
	DWORD FuncId[MAP_EVENT_MAX];
	DWORD LoopEnabled[MAP_LOOP_FUNC_MAX];
	DWORD LoopLastTick[MAP_LOOP_FUNC_MAX];
	DWORD LoopWaitTick[MAP_LOOP_FUNC_MAX];

	bool Init(ProtoMap* proto, Location* location);
	bool Generate();
	void Clear(bool full);
	void Process();

	Location* GetLocation(bool lock);
	WORD GetMaxHexX(){return Proto->Header.MaxHexX;}
	WORD GetMaxHexY(){return Proto->Header.MaxHexY;}
	void SetLoopTime(DWORD loop_num, DWORD ms);
	BYTE GetRain();
	void SetRain(BYTE capacity);
	int GetTime();
	void SetTime(int time);
	DWORD GetDayTime(DWORD day_part);
	void SetDayTime(DWORD day_part, DWORD time);
	void GetDayColor(DWORD day_part, BYTE& r, BYTE& g, BYTE& b);
	void SetDayColor(DWORD day_part, BYTE r, BYTE g, BYTE b);
	int GetData(DWORD index);
	void SetData(DWORD index, int value);

	void SetText(WORD hx, WORD hy, DWORD color, const char* text);
	void SetTextMsg(WORD hx, WORD hy, DWORD color, WORD text_msg, DWORD num_str);
	void SetTextMsgLex(WORD hx, WORD hy, DWORD color, WORD text_msg, DWORD num_str, const char* lexems);

	bool GetStartCoord(WORD& hx, WORD& hy, BYTE& dir, DWORD entire);
	bool GetStartCoordCar(WORD& hx, WORD& hy, ProtoItem* proto_item);
	bool FindStartHex(WORD& hx, WORD& hy, DWORD multihex, DWORD seek_radius, bool skip_unsafe);

	void SetId(DWORD id, WORD pid){Data.MapId=id; Data.MapPid=pid;}
	DWORD GetId(){return Data.MapId;}
	WORD GetPid(){return Data.MapPid;}

	void AddCritter(Critter* cr);
	void AddCritterEvents(Critter* cr);
	void EraseCritter(Critter* cr);
	void KickPlayersToGlobalMap();

	bool AddItem(Item* item, WORD hx, WORD hy, bool send_all);
	void SetItem(Item* item, WORD hx, WORD hy);
	void EraseItem(DWORD item_id, bool send_all);
	void ChangeDataItem(Item* item);
	void ChangeViewItem(Item* item);
	void AnimateItem(Item* item, BYTE from_frm, BYTE to_frm);

	Item* GetItem(DWORD item_id);
	Item* GetItemHex(WORD hx, WORD hy, WORD item_pid, Critter* picker);
	Item* GetItemDoor(WORD hx, WORD hy);
	Item* GetItemCar(WORD hx, WORD hy);
	Item* GetItemContainer(WORD hx, WORD hy);
	Item* GetItemGag(WORD hx, WORD hy);

	ItemPtrVec& GetItemsNoLock(){return hexItems;}
	void GetItemsHex(WORD hx, WORD hy, ItemPtrVec& items, bool lock);
	void GetItemsHexEx(WORD hx, WORD hy, DWORD radius, WORD pid, ItemPtrVec& items, bool lock);
	void GetItemsPid(WORD pid, ItemPtrVec& items, bool lock);
	void GetItemsType(int type, ItemPtrVec& items, bool lock);
	void GetItemsTrap(WORD hx, WORD hy, ItemPtrVec& items, bool lock);
	void RecacheHexBlock(WORD hx, WORD hy);
	void RecacheHexShoot(WORD hx, WORD hy);
	void RecacheHexBlockShoot(WORD hx, WORD hy);

	WORD GetHexFlags(WORD hx, WORD hy);
	void SetHexFlag(WORD hx, WORD hy, BYTE flag);
	void UnsetHexFlag(WORD hx, WORD hy, BYTE flag);

	bool IsHexPassed(WORD hx, WORD hy);
	bool IsHexRaked(WORD hx, WORD hy);
	bool IsHexesPassed(WORD hx, WORD hy, DWORD radius);
	bool IsMovePassed(WORD hx, WORD hy, BYTE dir, DWORD multihex);
	bool IsHexItem(WORD hx, WORD hy){return FLAG(hexFlags[hy*GetMaxHexX()+hx],FH_ITEM);}

	bool IsHexTrigger(WORD hx, WORD hy){return FLAG(Proto->HexFlags[hy*GetMaxHexX()+hx],FH_TRIGGER);}
	bool IsHexTrap(WORD hx, WORD hy){return FLAG(hexFlags[hy*GetMaxHexX()+hx],FH_WALK_ITEM);}

	bool IsHexCritter(WORD hx, WORD hy){return FLAG(hexFlags[hy*GetMaxHexX()+hx],FH_CRITTER|FH_DEAD_CRITTER);}
	bool IsHexGag(WORD hx, WORD hy){return FLAG(hexFlags[hy*GetMaxHexX()+hx],FH_GAG_ITEM);}

	bool IsFlagCritter(WORD hx, WORD hy, bool dead);
	void SetFlagCritter(WORD hx, WORD hy, DWORD multihex, bool dead);
	void UnsetFlagCritter(WORD hx, WORD hy, DWORD multihex, bool dead);
	Critter* GetCritter(DWORD crid);
	DWORD GetNpcCount(int npc_role, int find_type);
	Critter* GetNpc(int npc_role, int find_type, DWORD skip_count);
	Critter* GetHexCritter(WORD hx, WORD hy, bool dead);
	void GetCrittersHex(WORD hx, WORD hy, DWORD radius, int find_type, CrVec& critters, bool lock);

	void GetCritters(CrVec& critters);
	void GetPlayers(ClVec& players);
	void GetNpcs(PcVec& npcs);
	CrVec& GetCrittersNoLock(){return mapCritters;}
	ClVec& GetPlayersNoLock(){return mapPlayers;}
	PcVec& GetNpcsNoLock(){return mapNpcs;}

	bool IsNoLogOut(){return Proto->Header.NoLogOut;}

	// Sends
	void SendEffect(WORD eff_pid, WORD hx, WORD hy, WORD radius);
	void SendFlyEffect(WORD eff_pid, DWORD from_crid, DWORD to_crid, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy);

	// Cars
public:
	void GetCritterCar(Critter* cr, Item* car);
	void SetCritterCar(WORD hx, WORD hy, Critter* cr, Item* car);
	bool IsPlaceForCar(WORD hx, WORD hy, ProtoItem* proto_item);
	void PlaceCarBlocks(WORD hx, WORD hy, ProtoItem* proto_item);
	void ReplaceCarBlocks(WORD hx, WORD hy, ProtoItem* proto_item);
	void GetCarBagPos(WORD& hx, WORD& hy, ProtoItem* pcar, int num_bag);
	Item* GetCarBag(WORD hx, WORD hy, ProtoItem* pcar, int num_bag);

	// Events
private:
	bool PrepareScriptFunc(int num_scr_func);

public:
	bool ParseScript(const char* script, bool first_time);
	void EventFinish(bool to_delete);
	void EventLoop(int loop_num);
	void EventInCritter(Critter* cr);
	void EventOutCritter(Critter* cr);
	void EventCritterDead(Critter* cr, Critter* killer);
	void EventTurnBasedBegin();
	void EventTurnBasedEnd();
	void EventTurnBasedProcess(Critter* cr, bool begin_turn);

	// Turn based game
public:
	bool IsTurnBasedOn;
	DWORD TurnBasedEndTick;
	int TurnSequenceCur;
	DwordVec TurnSequence;
	bool IsTurnBasedTimeout;
	DWORD TurnBasedBeginSecond;
	bool NeedEndTurnBased;
	DWORD TurnBasedRound;
	DWORD TurnBasedTurn;
	DWORD TurnBasedWholeTurn;

	void BeginTurnBased(Critter* first_cr);
	void EndTurnBased();
	bool TryEndTurnBased();
	void ProcessTurnBased();
	bool IsCritterTurn(Critter* cr);
	DWORD GetCritterTurnId();
	DWORD GetCritterTurnTime();
	void EndCritterTurn();
	void NextCritterTurn();
	void GenerateSequence(CrVec& critters, Critter* first_cr);

	// Constructor, destructor
public:
	Map();
	~Map();

	bool IsNotValid;
	short RefCounter;
	void AddRef(){RefCounter++;}
	void Release(){RefCounter--; if(RefCounter<=0) delete this;}
};
typedef map<DWORD,Map*,less<DWORD>> MapMap;
typedef map<DWORD,Map*,less<DWORD>>::iterator MapMapIt;
typedef map<DWORD,Map*,less<DWORD>>::value_type MapMapVal;
typedef vector<Map*> MapVec;
typedef vector<Map*>::iterator MapVecIt;

class ProtoLocation
{
public:
	bool IsInit;
	WORD LocPid;
	string Name;

	WORD MaxCopy;
	WordVec ProtoMapPids;
	WordVec AutomapsPids;
	DwordPairVec Entrance;
	int ScriptBindId;

	WORD Radius;
	bool Visible;
	bool AutoGarbage;
	bool GeckEnabled;

	ProtoLocation():IsInit(false){};
};
typedef vector<ProtoLocation> ProtoLocVec;

class Location
{
public:
	SyncObject Sync;

private:
	MapVec locMaps;

public:
	struct LocData
	{
		DWORD LocId;
		WORD LocPid;
		WORD WX;
		WORD WY;
		WORD Radius;
		bool Visible;
		bool GeckEnabled;
		bool AutoGarbage;
		bool ToGarbage;
		DWORD Color;
		DWORD Reserved3[59];
	} Data;

	ProtoLocation* Proto;
	volatile int GeckCount;

	bool Init(ProtoLocation* proto, WORD wx, WORD wy);
	void Clear(bool full);
	void Update();
	bool IsToGarbage(){return Data.AutoGarbage || Data.ToGarbage;}
	bool IsVisible(){return Data.Visible || IsGeckAviable();}
	bool IsGeckAviable(){return Data.GeckEnabled && GeckCount>0;}
	DWORD GetId(){return Data.LocId;}
	void SetId(DWORD _id){Data.LocId=_id;}
	WORD GetPid(){return Data.LocPid;}
	int GetRadius(){return Data.Radius;}
	MapVec& GetMapsNoLock(){return locMaps;};
	void GetMaps(MapVec& maps, bool lock);
	DWORD GetMapsCount(){return locMaps.size();}
	Map* GetMap(DWORD count);
	bool GetTransit(Map* from_map, DWORD& id_map, WORD& hx, WORD& hy, BYTE& ori);
	bool IsAutomaps(){return !Proto->AutomapsPids.empty();}
	bool IsAutomap(WORD map_pid){return std::find(Proto->AutomapsPids.begin(),Proto->AutomapsPids.end(),map_pid)!=Proto->AutomapsPids.end();}
	WordVec& GetAutomaps(){return Proto->AutomapsPids;}

	bool IsNoCrit();
	bool IsNoPlayer();
	bool IsNoNpc();
	bool IsCanDelete();

	bool IsNotValid;
	short RefCounter;
	void AddRef(){RefCounter++;}
	void Release(){RefCounter--; if(RefCounter<=0) delete this;}
	Location():RefCounter(1),Proto(NULL),IsNotValid(false){ZeroMemory((void*)&Data,sizeof(Data));}
};
typedef map<DWORD,Location*,less<DWORD>> LocMap;
typedef map<DWORD,Location*,less<DWORD>>::iterator LocMapIt;
typedef map<DWORD,Location*,less<DWORD>>::value_type LocMapVal;
typedef vector<Location*> LocVec;
typedef vector<Location*>::iterator LocVecIt;

#endif // __MAP__