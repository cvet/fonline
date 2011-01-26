#ifndef __CRITTER__
#define __CRITTER__

#include "Common.h"
#include "zlib/zlib.h"
#include "BufferManager.h"
#include "Item.h"
#include "Dialogs.h"
#include "AI.h"
#include "Vars.h"
#include "CritterData.h"
#include "DataMask.h"
#include "NetProtocol.h"

#define CRITTER_EVENT_IDLE                    (0)
#define CRITTER_EVENT_FINISH                  (1)
#define CRITTER_EVENT_DEAD                    (2)
#define CRITTER_EVENT_RESPAWN                 (3)
#define CRITTER_EVENT_SHOW_CRITTER            (4)
#define CRITTER_EVENT_SHOW_CRITTER_1          (5)
#define CRITTER_EVENT_SHOW_CRITTER_2          (6)
#define CRITTER_EVENT_SHOW_CRITTER_3          (7)
#define CRITTER_EVENT_HIDE_CRITTER            (8)
#define CRITTER_EVENT_HIDE_CRITTER_1          (9)
#define CRITTER_EVENT_HIDE_CRITTER_2          (10)
#define CRITTER_EVENT_HIDE_CRITTER_3          (11)
#define CRITTER_EVENT_SHOW_ITEM_ON_MAP        (12)
#define CRITTER_EVENT_CHANGE_ITEM_ON_MAP      (13)
#define CRITTER_EVENT_HIDE_ITEM_ON_MAP        (14)
#define CRITTER_EVENT_ATTACK                  (15)
#define CRITTER_EVENT_ATTACKED                (16)
#define CRITTER_EVENT_STEALING                (17)
#define CRITTER_EVENT_MESSAGE                 (18)
#define CRITTER_EVENT_USE_ITEM                (19)
#define CRITTER_EVENT_USE_ITEM_ON_ME          (20)
#define CRITTER_EVENT_USE_SKILL               (21)
#define CRITTER_EVENT_USE_SKILL_ON_ME         (22)
#define CRITTER_EVENT_DROP_ITEM               (23)
#define CRITTER_EVENT_MOVE_ITEM               (24)
#define CRITTER_EVENT_KNOCKOUT                (25)
#define CRITTER_EVENT_SMTH_DEAD               (26)
#define CRITTER_EVENT_SMTH_STEALING           (27)
#define CRITTER_EVENT_SMTH_ATTACK             (28)
#define CRITTER_EVENT_SMTH_ATTACKED           (29)
#define CRITTER_EVENT_SMTH_USE_ITEM           (30)
#define CRITTER_EVENT_SMTH_USE_SKILL          (31)
#define CRITTER_EVENT_SMTH_DROP_ITEM          (32)
#define CRITTER_EVENT_SMTH_MOVE_ITEM          (33)
#define CRITTER_EVENT_SMTH_KNOCKOUT           (34)
#define CRITTER_EVENT_PLANE_BEGIN             (35)
#define CRITTER_EVENT_PLANE_END               (36)
#define CRITTER_EVENT_PLANE_RUN               (37)
#define CRITTER_EVENT_BARTER                  (38)
#define CRITTER_EVENT_TALK                    (39)
#define CRITTER_EVENT_GLOBAL_PROCESS          (40)
#define CRITTER_EVENT_GLOBAL_INVITE           (41)
#define CRITTER_EVENT_TURN_BASED_PROCESS      (42)
#define CRITTER_EVENT_SMTH_TURN_BASED_PROCESS (43)
#define CRITTER_EVENT_MAX                     (44)
extern const char* CritterEventFuncName[CRITTER_EVENT_MAX];

#define PLANE_RUN_GLOBAL                      (0)
#define PLANE_KEEP                            (1)
#define PLANE_DISCARD                         (2)

class Critter;
class Client;
class Npc;
class Map;
class Location;
class MapObject;
class GlobalMapGroup;

typedef Critter* CritterPtr;
typedef Client* ClientPtr;
typedef Npc* NpcPtr;

typedef map<DWORD,Critter*,less<DWORD>> CrMap;
typedef CrMap::iterator CrMapIt;
typedef CrMap::value_type CrMapVal;
typedef map<DWORD,Client*,less<DWORD>> ClMap;
typedef ClMap::iterator ClMapIt;
typedef ClMap::value_type ClMapVal;
typedef map<DWORD,Npc*,less<DWORD>> PcMap;
typedef PcMap::iterator PcMapIt;
typedef PcMap::value_type PcMapVal;

typedef vector<Critter*> CrVec;
typedef vector<Critter*>::iterator CrVecIt;
typedef vector<Client*> ClVec;
typedef vector<Client*>::iterator ClVecIt;
typedef vector<Npc*> PcVec;
typedef vector<Npc*>::iterator PcVecIt;

class Critter
{
private:
	Critter(const Critter&){}
	Critter& operator=(const Critter&){}

public:
	Critter();
	~Critter();

	CritData Data; // Saved
	CritDataExt* DataExt; // Saved
	SyncObject Sync;
	bool CritterIsNpc;
	DWORD Flags;
	string NameStr;
	int NameStrRefCounter;
	C2BitMask GMapFog;
	bool IsRuning;
	DWORD PrevHexTick;
	WORD PrevHexX,PrevHexY;
	int LockMapTransfers;
	Critter* ThisPtr[MAX_PARAMETERS_ARRAYS];
	DWORD AllowedToDownloadMap;

	static WSASendCallback SendDataCallback;
	static bool ParamsRegEnabled[MAX_PARAMS];
	static DWORD ParamsSendMsgLen;
	static WORD ParamsSendCount;
	static WordVec ParamsSend;
	static bool ParamsSendEnabled[MAX_PARAMS];
	static int ParamsSendScript[MAX_PARAMS];
	static int ParamsChangeScript[MAX_PARAMS];
	static int ParamsGetScript[MAX_PARAMS];
	static bool SlotDataSendEnabled[0x100];
	static int SlotDataSendScript[0x100];
	static DWORD ParamsChosenSendMask[MAX_PARAMS];
	static DWORD ParametersMin[MAX_PARAMETERS_ARRAYS];
	static DWORD ParametersMax[MAX_PARAMETERS_ARRAYS];
	static bool ParametersOffset[MAX_PARAMETERS_ARRAYS];
	bool ParamsIsChanged[MAX_PARAMS];
	IntVec ParamsChanged;
	int ParamLocked;
	static bool SlotEnabled[0x100];
	static Item* SlotEnabledCacheData[0x100];
	static Item* SlotEnabledCacheDataExt[0x100];

	CritDataExt* GetDataExt();
	void SetMaps(DWORD map_id, WORD map_pid){Data.MapId=map_id;Data.MapPid=map_pid;}
	void SetLexems(const char* lexems);
	bool IsLexems(){return Data.Lexems[0]!=0;}

	int RunParamsSendScript(int bind_id, DWORD param_index, Critter* from_cr, Critter* to_cr);
	bool RunSlotDataSendScript(int bind_id, BYTE slot, Item* item, Critter* from_cr, Critter* to_cr);

	// Visible critters and items
	CrVec VisCr;
	CrVec VisCrSelf;
	DwordSet VisCr1,VisCr2,VisCr3;
	DwordSet VisItem;
	MutexSpinlock VisItemLocker;
	DWORD ViewMapId;
	WORD ViewMapPid,ViewMapLook,ViewMapHx,ViewMapHy;
	BYTE ViewMapDir;
	DWORD ViewMapLocId,ViewMapLocEnt;

	void SyncLockCritters(bool self_critters, bool only_players);
	void ProcessVisibleCritters();
	void ProcessVisibleItems();
	void ViewMap(Map* map, int look, WORD hx, WORD hy, int dir);
	void ClearVisible();

	Critter* GetCritSelf(DWORD crid, bool sync_lock);
	void GetCrFromVisCr(CrVec& critters, int find_type, bool vis_cr_self, bool sync_lock);

	bool AddCrIntoVisVec(Critter* add_cr);
	bool DelCrFromVisVec(Critter* del_cr);

	bool AddCrIntoVisSet1(DWORD crid);
	bool AddCrIntoVisSet2(DWORD crid);
	bool AddCrIntoVisSet3(DWORD crid);
	bool DelCrFromVisSet1(DWORD crid);
	bool DelCrFromVisSet2(DWORD crid);
	bool DelCrFromVisSet3(DWORD crid);

	bool AddIdVisItem(DWORD item_id);
	bool DelIdVisItem(DWORD item_id);
	bool CountIdVisItem(DWORD item_id);

	// Global map group
public:
	GlobalMapGroup* GroupSelf;
	GlobalMapGroup* GroupMove;

	// Items
protected:
	ItemPtrVec invItems;
	Item* defItemSlotHand;
	Item* defItemSlotArmor;

public:	
	Item* ItemSlotMain;
	Item* ItemSlotExt;
	Item* ItemSlotArmor;

	void SyncLockItems();
	bool SetDefaultItems(ProtoItem* proto_hand, ProtoItem* proto_armor);
	Item* GetDefaultItemSlotMain(){return defItemSlotHand;}
	void AddItem(Item*& item, bool send);
	void SetItem(Item* item);
	void EraseItem(Item* item, bool send);
	Item* GetItem(DWORD item_id, bool skip_hide);
	Item* GetInvItem(DWORD item_id, int transfer_type);
	ItemPtrVec& GetItemsNoLock(){return invItems;}
	void GetInvItems(ItemPtrVec& items, int transfer_type, bool lock);
	Item* GetItemByPid(WORD item_pid);
	Item* GetItemByPidInvPriority(WORD item_pid);
	Item* GetAmmoForWeapon(Item* weap);
	Item* GetAmmo(DWORD caliber);
	Item* GetItemCar();
	Item* GetItemSlot(int slot);
	void GetItemsSlot(int slot, ItemPtrVec& items, bool lock);
	void GetItemsType(int type, ItemPtrVec& items, bool lock);
	DWORD CountItemPid(WORD item_pid);
	void TakeDefaultItem(BYTE slot);
	bool MoveItem(BYTE from_slot, BYTE to_slot, DWORD item_id, DWORD count);
	DWORD RealCountItems(){return invItems.size();}
	DWORD CountItems();
	ItemPtrVec& GetInventory(){SyncLockItems(); return invItems;}
	bool IsHaveGeckItem();

	// Scripts
protected:
	bool PrepareScriptFunc(int num_scr_func);

public:
	int FuncId[CRITTER_EVENT_MAX];
	bool ParseScript(const char* script, bool first_time);
	DWORD GetScriptId(){return Data.ScriptId;}

	void EventIdle();
	void EventFinish(bool deleted);
	void EventDead(Critter* killer);
	void EventRespawn();
	void EventShowCritter(Critter* cr);
	void EventShowCritter1(Critter* cr);
	void EventShowCritter2(Critter* cr);
	void EventShowCritter3(Critter* cr);
	void EventHideCritter(Critter* cr);
	void EventHideCritter1(Critter* cr);
	void EventHideCritter2(Critter* cr);
	void EventHideCritter3(Critter* cr);
	void EventShowItemOnMap(Item* item, bool added, Critter* dropper);
	void EventChangeItemOnMap(Item* item);
	void EventHideItemOnMap(Item* item, bool removed, Critter* picker);
	bool EventAttack(Critter* target);
	bool EventAttacked(Critter* attacker);
	bool EventStealing(Critter* thief, Item* item, DWORD count);
	void EventMessage(Critter* from_cr, int num, int val);
	bool EventUseItem(Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery);
	bool EventUseItemOnMe(Critter* who_use, Item* item);
	bool EventUseSkill(int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery);
	bool EventUseSkillOnMe(Critter* who_use, int skill);
	void EventDropItem(Item* item);
	void EventMoveItem(Item* item, BYTE from_slot);
	void EventKnockout(bool face_up, DWORD lost_ap, DWORD knock_dist);
	void EventSmthDead(Critter* from_cr, Critter* killer);
	void EventSmthStealing(Critter* from_cr, Critter* thief, bool success, Item* item, DWORD count);
	void EventSmthAttack(Critter* from_cr, Critter* target);
	void EventSmthAttacked(Critter* from_cr,Critter* attacker);
	void EventSmthUseItem(Critter* from_cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery);
	void EventSmthUseSkill(Critter* from_cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery);
	void EventSmthDropItem(Critter* from_cr, Item* item);
	void EventSmthMoveItem(Critter* from_cr, Item* item, BYTE from_slot);
	void EventSmthKnockout(Critter* from_cr, bool face_up, DWORD lost_ap, DWORD knock_dist);
	int EventPlaneBegin(AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item);
	int EventPlaneEnd(AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item);
	int EventPlaneRun(AIDataPlane* plane, int reason, DWORD& p0, DWORD& p1, DWORD& p2);
	bool EventBarter(Critter* cr_barter, bool attach, DWORD barter_count);
	bool EventTalk(Critter* cr_talk, bool attach, DWORD talk_count);
	bool EventGlobalProcess(int type, Item* car, DWORD& x, DWORD& y, DWORD& to_x, DWORD& to_y, DWORD& speed, DWORD& encounter_descriptor, bool& wait_for_answer);
	bool EventGlobalInvite(Item* car, DWORD encounter_descriptor, int combat_mode, DWORD& map_id, WORD& hx, WORD& hy, BYTE& dir);
	void EventTurnBasedProcess(Map* map, bool begin_turn);
	void EventSmthTurnBasedProcess(Critter* from_cr, Map* map, bool begin_turn);

	// Knockout
	DWORD KnockoutAp;

	void ToKnockout(bool face_up, DWORD lose_ap, WORD knock_hx, WORD knock_hy);
	void TryUpOnKnockout();

	// Respawn, Dead
	void ToDead(BYTE dead_type, bool send_all);

	// Heal
	DWORD LastHealTick;
	void Heal();

	// Cached values to avoid synchronization
	DWORD CacheValuesNextTick;
	WORD IntellectCacheValue; // Players talking
	DWORD LookCacheValue; // Critter::GetLook()

	// Break time
private:
	DWORD startBreakTime;
	int	breakTime;
	DWORD waitEndTick;

public:
	bool IsFree(){return (Timer::GameTick()-startBreakTime>=breakTime);}
	bool IsBusy(){return !IsFree();}
	void SetBreakTime(DWORD ms){breakTime=ms; startBreakTime=Timer::GameTick(); ApRegenerationTick=0;}
	void SetBreakTimeDelta(DWORD ms){int dt=(Timer::GameTick()-startBreakTime)-breakTime; if(dt>ms) dt=0; SetBreakTime(ms-dt);}// if(dt>0) {dt=(int)ms-dt; if(dt>0) breakTime=dt; else breakTime=0;}

	void SetWait(DWORD ms){waitEndTick=Timer::GameTick()+ms;}
	bool IsWait(){return Timer::GameTick()<waitEndTick;}

	void FullClear();

	// Send
	volatile int DisableSend;
	bool IsSendDisabled(){return DisableSend>0;}
	void Send_Move(Critter* from_cr, WORD move_params);
	void Send_Dir(Critter* from_cr);
	void Send_AddCritter(Critter* cr);
	void Send_RemoveCritter(Critter* cr);
	void Send_LoadMap(Map* map);
	void Send_XY(Critter* cr);
	void Send_AddItemOnMap(Item* item);
	void Send_ChangeItemOnMap(Item* item);
	void Send_EraseItemFromMap(Item* item);
	void Send_AnimateItem(Item* item, BYTE from_frm, BYTE to_frm);
	void Send_AddItem(Item* item);
	void Send_EraseItem(Item* item);
	void Send_ContainerInfo();
	void Send_ContainerInfo(Item* item_cont, BYTE transfer_type, bool open_screen);
	void Send_ContainerInfo(Critter* cr_cont, BYTE transfer_type, bool open_screen);
	void Send_GlobalInfo(BYTE flags);
	void Send_GlobalLocation(Location* loc, bool add);
	void Send_GlobalMapFog(WORD zx, WORD zy, BYTE fog);
	void Send_AllParams();
	void Send_Param(WORD num_param);
	void Send_ParamOther(WORD num_param, int val);
	void Send_CritterParam(Critter* cr, WORD num_param, int val);
	void Send_Talk();
	void Send_GameInfo(Map* map);
	void Send_Text(Critter* from_cr, const char* s_str, BYTE how_say);
	void Send_TextEx(DWORD from_id, const char* s_str, WORD str_len, BYTE how_say, WORD intellect, bool unsafe_text);
	void Send_TextMsg(Critter* from_cr, DWORD str_num, BYTE how_say, WORD num_msg);
	void Send_TextMsg(DWORD from_id, DWORD str_num, BYTE how_say, WORD num_msg);
	void Send_TextMsgLex(Critter* from_cr, DWORD num_str, BYTE how_say, WORD num_msg, const char* lexems);
	void Send_TextMsgLex(DWORD from_id, DWORD num_str, BYTE how_say, WORD num_msg, const char* lexems);
	void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
	void Send_Knockout(Critter* from_cr, bool face_up, WORD knock_hx, WORD knock_hy);
	void Send_MoveItem(Critter* from_cr, Item* item, BYTE action, BYTE prev_slot);
	void Send_ItemData(Critter* from_cr, BYTE slot, Item* item, bool ext_data);
	void Send_Animate(Critter* from_cr, DWORD anim1, DWORD anim2, Item* item, bool clear_sequence, bool delay_play);
	void Send_CombatResult(DWORD* combat_res, DWORD len);
	void Send_Quest(DWORD num);
	void Send_Quests(DwordVec& nums);
	void Send_HoloInfo(bool clear, WORD offset, WORD count);
	void Send_AutomapsInfo(void* locs_vec, Location* loc);
	void Send_Follow(DWORD rule, BYTE follow_type, WORD map_pid, DWORD follow_wait);
	void Send_Effect(WORD eff_pid, WORD hx, WORD hy, WORD radius);
	void Send_FlyEffect(WORD eff_pid, DWORD from_crid, DWORD to_crid, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy);
	void Send_PlaySound(DWORD crid_synchronize, const char* sound_name);
	void Send_PlaySoundType(DWORD crid_synchronize, BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext);
	void Send_CritterLexems(Critter* cr);

	// Send all
	void SendA_Move(WORD move_params);
	void SendA_XY();
	void SendA_Action(int action, int action_ext, Item* item);
	void SendAA_Action(int action, int action_ext, Item* item);
	void SendA_Knockout(bool face_up, WORD knock_hx, WORD knock_hy);
	void SendAA_MoveItem(Item* item, BYTE action, BYTE prev_slot);
	void SendAA_ItemData(Item* item);
	void SendAA_Animate(DWORD anim1, DWORD anim2, Item* item, bool clear_sequence, bool delay_play);
	void SendA_GlobalInfo(GlobalMapGroup* group, BYTE info_flags);
	void SendAA_Text(CrVec& to_cr, const char* str, BYTE how_say, bool unsafe_text);
	void SendAA_Msg(CrVec& to_cr, DWORD num_str, BYTE how_say, WORD num_msg);
	void SendAA_MsgLex(CrVec& to_cr, DWORD num_str, BYTE how_say, WORD num_msg, const char* lexems);
	void SendA_Dir();
	void SendA_Follow(BYTE follow_type, WORD map_pid, DWORD follow_wait);
	void SendA_ParamOther(WORD num_param, int val);
	void SendA_ParamCheck(WORD num_param);

	// Chosen data
	void Send_AddAllItems();
	void Send_AllQuests();
	void Send_AllAutomapsInfo();

	bool IsPlayer(){return !CritterIsNpc;}
	bool IsNpc(){return CritterIsNpc;}
	DWORD GetId(){return Data.Id;}
	DWORD GetMap(){return Data.MapId;}
	WORD GetProtoMap(){return Data.MapPid;}
	const char* GetName(){return NameStr.c_str();}
	const char* GetInfo();
	DWORD GetCrType(){return Data.BaseType;}
	WORD GetHexX(){return Data.HexX;}
	WORD GetHexY(){return Data.HexY;}
	BYTE GetDir(){return Data.Dir;}
	DWORD GetTimeWalk();
	DWORD GetTimeRun();
	DWORD GetItemsWeight();
	DWORD GetItemsVolume();
	bool IsOverweight(){return GetItemsWeight()>GetParam(ST_CARRY_WEIGHT);}
	int GetFreeWeight();
	int GetFreeVolume();
	int GetParam(DWORD index);
	void ChangeParam(DWORD index);
	void ProcessChangedParams();
	DWORD GetFollowCrId(){return Data.Params[ST_FOLLOW_CRIT];}
	void SetFollowCrId(DWORD crid){ChangeParam(ST_FOLLOW_CRIT); Data.Params[ST_FOLLOW_CRIT]=crid;}
	bool IsRawParam(DWORD index){return Data.Params[index]!=0;}
	int GetRawParam(DWORD index){return Data.Params[index];}
	bool IsDmgLeg(){return Data.Params[DAMAGE_RIGHT_LEG]!=0 || Data.Params[DAMAGE_LEFT_LEG]!=0;}
	bool IsDmgTwoLeg(){return Data.Params[DAMAGE_RIGHT_LEG]!=0 && Data.Params[DAMAGE_LEFT_LEG]!=0;}
	bool IsDmgArm(){return Data.Params[DAMAGE_RIGHT_ARM]!=0 || Data.Params[DAMAGE_LEFT_ARM]!=0;}
	bool IsDmgTwoArm(){return Data.Params[DAMAGE_RIGHT_ARM]!=0 && Data.Params[DAMAGE_LEFT_ARM]!=0;}
	void SendMessage(int num, int val, int to);
	int GetLook();
	DWORD GetTalkDistance(Critter* talker);
	DWORD GetAttackDist(Item* weap, int use);
	DWORD GetUseDist();
	DWORD GetMultihex();
	bool IsLife(){return Data.Cond==COND_LIFE;}
	bool IsDead(){return Data.Cond==COND_DEAD;}
	bool IsKnockout(){return Data.Cond==COND_KNOCKOUT;}
	bool CheckFind(int find_type);
	int GetRealAp(){return Data.Params[ST_CURRENT_AP];}
	int GetAllAp(){return GetParam(ST_CURRENT_AP)+GetParam(ST_MOVE_AP);}
	void SubAp(int val){ChangeParam(ST_CURRENT_AP);Data.Params[ST_CURRENT_AP]-=val*AP_DIVIDER;ApRegenerationTick=0;}
	void SubMoveAp(int val){ChangeParam(ST_CURRENT_AP);Data.Params[ST_MOVE_AP]-=val;}

	// Turn based
	bool IsTurnBased(){return TB_BATTLE_TIMEOUT_CHECK(GetParam(TO_BATTLE));}
	bool CheckMyTurn(Map* map);
	int GetApCostCritterMove(bool is_run){return IsTurnBased()?GameOpt.TbApCostCritterMove*AP_DIVIDER*(IsDmgTwoLeg()?4:(IsDmgLeg()?2:1)):(GetParam(TO_BATTLE)?(is_run?GameOpt.RtApCostCritterRun:GameOpt.RtApCostCritterWalk):0);}
	int GetApCostMoveItemContainer(){return IsTurnBased()?GameOpt.TbApCostMoveItemContainer:GameOpt.RtApCostMoveItemContainer;}
	int GetApCostMoveItemInventory(){int val=IsTurnBased()?GameOpt.TbApCostMoveItemInventory:GameOpt.RtApCostMoveItemInventory; if(IsRawParam(PE_QUICK_POCKETS)) val/=2; return val;}
	int GetApCostPickItem(){return IsTurnBased()?GameOpt.TbApCostPickItem:GameOpt.RtApCostPickItem;}
	int GetApCostDropItem(){return IsTurnBased()?GameOpt.TbApCostDropItem:GameOpt.RtApCostDropItem;}
	int GetApCostPickCritter(){return IsTurnBased()?GameOpt.TbApCostPickCritter:GameOpt.RtApCostPickCritter;}
	int GetApCostUseSkill(){return IsTurnBased()?GameOpt.TbApCostUseSkill:GameOpt.RtApCostUseSkill;}

	// Timeouts
	void SetTimeout(int timeout, DWORD game_seconds);
	bool IsTransferTimeouts(bool send);

	// Current container
	DWORD AccessContainerId;
	DWORD ItemTransferCount;

	// Home
	DWORD TryingGoHomeTick;

	void SetHome(DWORD map_id, WORD hx, WORD hy, BYTE dir){Data.HomeMap=map_id; Data.HomeX=hx; Data.HomeY=hy; Data.HomeOri=dir;}
	DWORD GetHomeMap(){return Data.HomeMap;}
	WORD GetHomeX(){return Data.HomeX;}
	WORD GetHomeY(){return Data.HomeY;}
	BYTE GetHomeOri(){return Data.HomeOri;}
	bool IsInHome(){return !GetMap() || (Data.HomeOri==GetDir() && Data.HomeX==GetHexX() && Data.HomeY==GetHexY() && Data.HomeMap==GetMap());}

	// Enemy stack
	void AddEnemyInStack(DWORD crid);
	bool CheckEnemyInStack(DWORD crid);
	void EraseEnemyInStack(DWORD crid);
	Critter* ScanEnemyStack();

	// Time events
	struct CrTimeEvent
	{
		DWORD FuncNum;
		DWORD Rate;
		DWORD NextTime;
		int Identifier;
		void operator=(const CrTimeEvent& r){FuncNum=r.FuncNum;Rate=r.Rate;NextTime=r.NextTime;Identifier=r.Identifier;}
	};
typedef vector<CrTimeEvent> CrTimeEventVec;
typedef vector<CrTimeEvent>::iterator CrTimeEventVecIt;
	CrTimeEventVec CrTimeEvents;

	void AddCrTimeEvent(DWORD func_num, DWORD rate, DWORD duration, int identifier);
	void EraseCrTimeEvent(int index);
	void ContinueTimeEvents(int offs_time);

	// Other
	DWORD GlobalIdleNextTick;
	DWORD ApRegenerationTick;

	// Reference counter
	bool IsNotValid;
	bool CanBeRemoved;
	long RefCounter;
	void AddRef(){InterlockedIncrement(&RefCounter);}
	void Release(){if(!InterlockedDecrement(&RefCounter)) Delete();}
	void Delete();
};

class Client : public Critter
{
public:
	char Name[MAX_NAME+1]; // Saved
	char Pass[MAX_NAME+1]; // Saved
	BYTE Access;
	DWORD LanguageMsg;
	DWORD UID[5];
	volatile SOCKET Sock;
	SOCKADDR_IN From;
	BufferManager Bin,Bout;
	WSAOVERLAPPED_EX* WSAIn;
	WSAOVERLAPPED_EX* WSAOut;
	volatile long NetState;
	Mutex ShutdownLocker;
	bool IsDisconnected;
	bool DisableZlib;
	z_stream Zstrm;
	bool ZstrmInit;
	DWORD ConnectTime;
	DWORD LastSendedMapTick;
	char LastSay[MAX_NET_TEXT+1];
	DWORD LastSayEqualCount;
	DWORD RadioMessageSended;

public:
	DWORD GetIp(){return From.sin_addr.s_addr;}
	const char* GetIpStr(){return inet_ntoa(From.sin_addr);}

	// Net
#define BIN_BEGIN(cl_) cl_->Bin.Lock()
#define BIN_END(cl_) cl_->Bin.Unlock()
#define BOUT_BEGIN(cl_) cl_->Bout.Lock()
#define BOUT_END(cl_) cl_->Bout.Unlock(); if(InterlockedCompareExchange(&cl_->WSAOut->Operation,0,0)==WSAOP_FREE) (*Critter::SendDataCallback)(cl_->WSAOut);
private:
	DWORD disconnectTick;

public:
	void Shutdown();
	bool IsOnline(){return InterlockedCompareExchange(&NetState,0,0)==STATE_GAME || InterlockedCompareExchange(&NetState,0,0)==STATE_LOGINOK || InterlockedCompareExchange(&NetState,0,0)==STATE_CONN;}
	bool IsOffline(){return InterlockedCompareExchange(&NetState,0,0)==STATE_DISCONNECT;}
	void Disconnect(){InterlockedExchange(&NetState,STATE_DISCONNECT); disconnectTick=Timer::FastTick();}
	void RemoveFromGame(){CanBeRemoved=true;}
	DWORD GetOfflineTime(){return Timer::FastTick()-disconnectTick;}

	// Ping
private:
	DWORD pingNextTick;
	bool pingOk;

public:
	bool IsToPing(){return InterlockedCompareExchange(&NetState,0,0)==STATE_GAME && Timer::FastTick()>=pingNextTick && !GetParam(TO_TRANSFER) && !Singleplayer;}
	void PingClient();
	void PingOk(DWORD next_ping){pingOk=true; pingNextTick=Timer::FastTick()+next_ping;}

	// Sends
	void Send_Move(Critter* from_cr, WORD move_params);
	void Send_Dir(Critter* from_cr);
	void Send_AddCritter(Critter* cr);
	void Send_RemoveCritter(Critter* cr);
	void Send_LoadMap(Map* map);
	void Send_XY(Critter* cr);
	void Send_AddItemOnMap(Item* item);
	void Send_ChangeItemOnMap(Item* item);
	void Send_EraseItemFromMap(Item* item);
	void Send_AnimateItem(Item* item, BYTE from_frm, BYTE to_frm);
	void Send_AddItem(Item* item);
	void Send_EraseItem(Item* item);
	void Send_ContainerInfo();
	void Send_ContainerInfo(Item* item_cont, BYTE transfer_type, bool open_screen);
	void Send_ContainerInfo(Critter* cr_cont, BYTE transfer_type, bool open_screen);
	void Send_GlobalInfo(BYTE flags);
	void Send_GlobalLocation(Location* loc, bool add);
	void Send_GlobalMapFog(WORD zx, WORD zy, BYTE fog);
	void Send_AllParams();
	void Send_Param(WORD num_param);
	void Send_ParamOther(WORD num_param, int val);
	void Send_CritterParam(Critter* cr, WORD num_param, int val);
	void Send_Talk();
	void Send_GameInfo(Map* map);
	void Send_Text(Critter* from_cr, const char* s_str, BYTE how_say);
	void Send_TextEx(DWORD from_id, const char* s_str, WORD str_len, BYTE how_say, WORD intellect, bool unsafe_text);
	void Send_TextMsg(Critter* from_cr, DWORD str_num, BYTE how_say, WORD num_msg);
	void Send_TextMsg(DWORD from_id, DWORD str_num, BYTE how_say, WORD num_msg);
	void Send_TextMsgLex(Critter* from_cr, DWORD num_str, BYTE how_say, WORD num_msg, const char* lexems);
	void Send_TextMsgLex(DWORD from_id, DWORD num_str, BYTE how_say, WORD num_msg, const char* lexems);
	void Send_Action(Critter* from_cr, int action, int action_ext, Item* item);
	void Send_Knockout(Critter* from_cr, bool face_up, WORD knock_hx, WORD knock_hy);
	void Send_MoveItem(Critter* from_cr, Item* item, BYTE action, BYTE prev_slot);
	void Send_ItemData(Critter* from_cr, BYTE slot, Item* item, bool ext_data);
	void Send_Animate(Critter* from_cr, DWORD anim1, DWORD anim2, Item* item, bool clear_sequence, bool delay_play);
	void Send_CombatResult(DWORD* combat_res, DWORD len);
	void Send_Quest(DWORD num);
	void Send_Quests(DwordVec& nums);
	void Send_HoloInfo(bool clear, WORD offset, WORD count);
	void Send_AutomapsInfo(void* locs_vec, Location* loc);
	void Send_Follow(DWORD rule, BYTE follow_type, WORD map_pid, DWORD follow_wait);
	void Send_Effect(WORD eff_pid, WORD hx, WORD hy, WORD radius);
	void Send_FlyEffect(WORD eff_pid, DWORD from_crid, DWORD to_crid, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy);
	void Send_PlaySound(DWORD crid_synchronize, const char* sound_name);
	void Send_PlaySoundType(DWORD crid_synchronize, BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext);
	void Send_CritterLexems(Critter* cr);
	void Send_MapText(WORD hx, WORD hy, DWORD color, const char* text, WORD text_len, WORD intellect, bool unsafe_text);
	void Send_MapTextMsg(WORD hx, WORD hy, DWORD color, WORD num_msg, DWORD num_str);
	void Send_MapTextMsgLex(WORD hx, WORD hy, DWORD color, WORD num_msg, DWORD num_str, const char* lexems, WORD lexems_len);
	void Send_UserHoloStr(DWORD str_num, const char* text, WORD text_len);
	void Send_PlayersBarter(BYTE barter, DWORD param, DWORD param_ext);
	void Send_PlayersBarterSetHide(Item* item, DWORD count);
	void Send_ShowScreen(int screen_type, DWORD param, bool need_answer);
	void Send_RunClientScript(const char* func_name, int p0, int p1, int p2, const char* p3, DwordVec& p4);
	void Send_DropTimers();
	void Send_ViewMap();
	void Send_ItemLexems(Item* item); // Without checks!
	void Send_ItemLexemsNull(Item* item); // Without checks!
	void Send_CheckUIDS();
	void Send_SomeItem(Item* item); // Without checks!

	// Locations
	bool CheckKnownLocById(DWORD loc_id);
	bool CheckKnownLocByPid(WORD loc_pid);
	void AddKnownLoc(DWORD loc_id);
	void EraseKnownLoc(DWORD loc_id);

	// Players barter
	bool BarterOffer;
	bool BarterHide;
	DWORD BarterOpponent;
	struct BarterItem 
	{
		DWORD Id;
		DWORD Pid;
		DWORD Count;
		bool operator==(DWORD id){return Id==id;}
		BarterItem(DWORD id, DWORD pid, DWORD count):Id(id),Pid(pid),Count(count){}
	};
typedef vector<BarterItem> BarterItemVec;
typedef vector<BarterItem>::iterator BarterItemVecIt;
	BarterItemVec BarterItems;

	Client* BarterGetOpponent(DWORD opponent_id);
	void BarterEnd();
	void BarterRefresh(Client* opponent);
	BarterItem* BarterGetItem(DWORD item_id);
	void BarterEraseItem(DWORD item_id);

	// Timers
	DWORD LastSendScoresTick,LastSendCraftTick,LastSendEntrancesTick,LastSendEntrancesLocId;
	void DropTimers(bool send);

	// Dialogs
private:
	DWORD talkNextTick;

public:
	Talking Talk;
	bool IsTalking(){return Talk.TalkType!=TALK_NONE;}
	void ProcessTalk(bool force);
	void CloseTalk();

	// Screen callback
	int ScreenCallbackBindId;

	Client();
	~Client();
};

class Npc : public Critter
{
	// Bag
public:
	DWORD NextRefreshBagTick;
	bool IsNeedRefreshBag(){return IsLife() && Timer::GameTick()>NextRefreshBagTick && IsNoPlanes();}
	void RefreshBag();

	// AI plane
private:
	AIDataPlaneVec aiPlanes;

public:
	bool AddPlane(int reason, AIDataPlane* plane, bool is_child, Critter* some_cr = NULL, Item* some_item = NULL);
	void NextPlane(int reason, Critter* some_cr = NULL, Item* some_item = NULL);
	bool RunPlane(int reason, DWORD& r0, DWORD& r1, DWORD& r2);
	bool IsPlaneAviable(int plane_type);
	bool IsCurPlane(int plane_type){AIDataPlane* p=GetCurPlane(); return p?p->Type==plane_type:false;}
	AIDataPlane* GetCurPlane(){return aiPlanes.size()?aiPlanes[0]->GetCurPlane():NULL;}
	AIDataPlaneVec& GetPlanes(){return aiPlanes;}
	void DropPlanes();
	void SetBestCurPlane();
	bool IsNoPlanes(){return aiPlanes.empty();}

	// Dialogs
public:
	DWORD GetTalkedPlayers();
	bool IsTalkedPlayers();
	DWORD GetBarterPlayers();
	bool IsFreeToTalk();
	bool IsPlaneNoTalk();

	// Target
public:
	void MoveToHex(int reason, WORD hx, WORD hy, BYTE ori, bool is_run, BYTE cut);
	void SetTarget(int reason, Critter* target, int min_hp, bool is_gag);

	// Params
	WORD GetProtoId(void) const {return Data.ProtoId;}

	DWORD Reserved;

	Npc();
	~Npc();
};

#endif // __CRITTER__