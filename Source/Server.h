#ifndef __SERVER__
#define __SERVER__

#include "StdAfx.h"
#include "Common.h"
#include "Item.h"
#include "Critter.h"
#include "Map.h"
#include "MapManager.h"
#include "CritterManager.h"
#include "ItemManager.h"
#include "Dialogs.h"
#include "Vars.h"
#include "CraftManager.h"
#include "Names.h"
#include "CritterType.h"
#include "NetProtocol.h"
#include "Access.h"
#include <process.h>

// #ifdef _DEBUG
// #define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
// #endif

// Check buffer for error
#define CHECK_IN_BUFF_ERROR(client) CHECK_IN_BUFF_ERROR_EX(client,0)
#define CHECK_IN_BUFF_ERROR_EX(client,ext) \
	if(client->Bin.IsError())\
	{\
		WriteLog(__FUNCTION__" - Wrong MSG data from client<%s>.\n",client->GetInfo());\
		ext;\
		client->Disconnect();\
		client->Bin.LockReset();\
		return;\
	}

class FOServer
{
public:
	FOServer();
	~FOServer();

	// Net proccess
	void Process_ParseToGame(Client* cl);
	void Process_Move(Client* cl);
	void Process_CreateClient(Client* cl);
	void Process_LogIn(ClientPtr& cl);
	void Process_Dir(Client* cl);
	void Process_ChangeItem(Client* cl);
	void Process_RateItem(Client* cl);
	void Process_SortValueItem(Client* cl);
	void Process_UseItem(Client* cl); //заявка на использование объекта
	void Process_PickItem(Client* cl);
	void Process_PickCritter(Client* cl);
	void Process_ContainerItem(Client* cl);
	void Process_UseSkill(Client* cl); //заявка на использование скилла
	void Process_GiveGlobalInfo(Client* cl);
	void Process_RuleGlobal(Client* cl);
	void Process_Text(Client* cl);
	void Process_Command(Client* cl);
	void Process_Dialog(Client* cl, bool is_say);
	void Process_Barter(Client* cl);
	void Process_GiveMap(Client* cl);
	void Process_Radio(Client* cl); //with send
	void Process_SetUserHoloStr(Client* cl);
	void Process_GetUserHoloStr(Client* cl); //with send
	void Process_LevelUp(Client* cl);
	void Process_CraftAsk(Client* cl);
	void Process_Craft(Client* cl);
	void Process_Ping(Client* cl);
	void Process_PlayersBarter(Client* cl);
	void Process_ScreenAnswer(Client* cl);
	void Process_GetScores(Client* cl);
	void Process_Combat(Client* cl);
	void Process_RunServerScript(Client* cl);
	void Process_KarmaVoting(Client* cl);

	void Send_MapData(Client* cl, ProtoMap* pmap, BYTE send_info);
	void Send_MsgData(Client* cl, DWORD lang, WORD num_msg, FOMsg& data_msg);
	void Send_ProtoItemData(Client* cl, BYTE type, ProtoItemVec& data, DWORD data_hash);

	// Data
	int UpdateVarsTemplate();

	// Holodisks
	struct HoloInfo
	{
		bool CanRewrite;
		string Title;
		string Text;
		HoloInfo(bool can_rw, const char* title, const char* text):CanRewrite(can_rw),Title(title),Text(text){}
	};
typedef map<DWORD,HoloInfo*,less<DWORD>> HoloInfoMap;
typedef map<DWORD,HoloInfo*,less<DWORD>>::iterator HoloInfoMapIt;
typedef map<DWORD,HoloInfo*,less<DWORD>>::value_type HoloInfoMapVal;
	HoloInfoMap HolodiskInfo;
	DWORD LastHoloId;
	void SaveHoloInfoFile();
	void LoadHoloInfoFile(FILE* f);
	HoloInfo* GetHoloInfo(DWORD id){HoloInfoMapIt it=HolodiskInfo.find(id); return it!=HolodiskInfo.end()?(*it).second:NULL;}
	void AddPlayerHoloInfo(Critter* cr, DWORD holo_num, bool send);
	void ErasePlayerHoloInfo(Critter* cr, DWORD index, bool send);
	void Send_PlayerHoloInfo(Critter* cr, DWORD holo_num, bool send_text);

	// Actions
	bool Act_Move(Critter* cr, WORD hx, WORD hy, WORD move_params);
	bool Act_Attack(Critter* cr, BYTE rate_weap, DWORD target_id);
	bool Act_Reload(Critter* cr, DWORD weap_id, DWORD ammo_id);
	bool Act_Use(Critter* cr, DWORD item_id, int skill, int target_type, DWORD target_id, WORD target_pid, DWORD param);
	bool Act_PickItem(Critter* cr, WORD hx, WORD hy, WORD pid);

	static void KillCritter(Critter* cr, BYTE dead_type, Critter* attacker);
	static void RespawnCritter(Critter* cr);
	static void KnockoutCritter(Critter* cr, bool face_up, DWORD lose_ap, WORD knock_hx, WORD knock_hy);
	static bool MoveRandom(Critter* cr);
	static bool RegenerateMap(Map* map);
	static bool VerifyTrigger(Map* map, Critter* cr, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, BYTE dir);

	// Time events
#define TIME_EVENT_MAX_SIZE       (0xFFFF)
	void SaveTimeEventsFile();
	void LoadTimeEventsFile(FILE* f);
	DWORD CreateScriptEvent(DWORD begin_minute, const char* script_name, DwordVec& values, bool save);
	void EraseTimeEvent(DWORD num);
	void ProcessTimeEvents();
	DWORD GetTimeEventsCount();
	string GetTimeEventsStatistics();

	void SaveScriptFunctionsFile();
	void LoadScriptFunctionsFile(FILE* f);

	// Any data
typedef map<string,ByteVec,less<string>> AnyDataMap;
typedef map<string,ByteVec,less<string>>::iterator AnyDataMapIt;
typedef map<string,ByteVec,less<string>>::value_type AnyDataMapVal;
#define ANY_DATA_MAX_NAME         (64)
#define ANY_DATA_MAX_SIZE         (0xFFFF)

	AnyDataMap AnyData;
	void SaveAnyDataFile();
	void LoadAnyDataFile(FILE* f);
	bool SetAnyData(const string& name, const BYTE* data, DWORD data_size);
	BYTE* GetAnyData(const string& name, DWORD& length);
	bool IsAnyData(const string& name);
	void EraseAnyData(const string& name);
	string GetAnyDataStatistics();

	// Scripting
	// Init/Finish system
	bool InitScriptSystem();
	void FinishScriptSystem();
	void ScriptSystemUpdate();

	// Dialogs demand and result
	bool DialogScriptDemand(DemandResult& demand, Critter* master, Critter* slave);
	void DialogScriptResult(DemandResult& result, Critter* master, Critter* slave);

	// Pragma callbacks
	static bool PragmaCallbackCrData(const char* text);

	// Text listen
#define TEXT_LISTEN_FIRST_STR_MIN_LEN     (5)
#define TEXT_LISTEN_FIRST_STR_MAX_LEN     (63)
	struct TextListen
	{
		int FuncId;
		int SayType;
		char FirstStr[TEXT_LISTEN_FIRST_STR_MAX_LEN+1];
		DWORD FirstStrLen;
		WORD Parameter;
	};
typedef vector<TextListen> TextListenVec;
typedef vector<TextListen>::iterator TextListenVecIt;
	TextListenVec TextListeners;

//	void GlobalEventCritterUseItem(Critter* cr);
//	void GlobalEventCritterUseSkill(Critter* cr);
//	void GlobalEventCritterInit(Critter* cr, bool first_time);
//	void GlobalEventCritterFinish(Critter* cr, bool to_delete);
//	void GlobalEventCritterIdle(Critter* cr);
//	void GlobalEventCritterDead(Critter* cr);

	struct SScriptFunc
	{
#define SCRIPT_ERROR(error) do{SScriptFunc::ScriptLastError=error; ServerScript.LogError(__FUNCTION__", "error);}while(0)
#define SCRIPT_ERROR_RX(error,x) do{SScriptFunc::ScriptLastError=error; ServerScript.LogError(__FUNCTION__", "error); return x;}while(0)
#define SCRIPT_ERROR_R(error) do{SScriptFunc::ScriptLastError=error; ServerScript.LogError(__FUNCTION__", "error); return;}while(0)
#define SCRIPT_ERROR_R0(error) do{SScriptFunc::ScriptLastError=error; ServerScript.LogError(__FUNCTION__", "error); return 0;}while(0)
		static string ScriptLastError;
		static CScriptString* Global_GetLastError();

		static int* DataRef_Index(CritterPtr& cr, DWORD index);
		static int DataVal_Index(CritterPtr& cr, DWORD index);

		static AIDataPlane* NpcPlane_GetCopy(AIDataPlane* plane);
		static AIDataPlane* NpcPlane_SetChild(AIDataPlane* plane, AIDataPlane* child_plane);
		static AIDataPlane* NpcPlane_GetChild(AIDataPlane* plane, DWORD index);
		static bool NpcPlane_Misc_SetScript(AIDataPlane* plane, CScriptString& func_name);

		static Item* Container_AddItem(Item* cont, WORD pid, DWORD count, DWORD special_id);
		static DWORD Container_GetItems(Item* cont, DWORD special_id, asIScriptArray* items);
		static Item* Container_GetItem(Item* cont, WORD pid, DWORD special_id);

		static bool Item_IsGrouped(Item* item);
		static bool Item_IsWeared(Item* item);
		static bool Item_SetScript(Item* item, CScriptString* script);
		static DWORD Item_GetScriptId(Item* item);
		static bool Item_SetEvent(Item* item, int event_type, CScriptString* func_name);
		static BYTE Item_GetType(Item* item);
		static WORD Item_GetProtoId(Item* item);
		static DWORD Item_GetCount(Item* item);
		static void Item_SetCount(Item* item, DWORD count);
		static DWORD Item_GetCost(Item* item);
		static Map* Item_GetMapPosition(Item* item, WORD& hx, WORD& hy);
		static bool Item_ChangeProto(Item* item, WORD pid);
		static void Item_Update(Item* item);
		static void Item_Animate(Item* item, BYTE from_frm, BYTE to_frm);
		static void Item_SetLexems(Item* item, CScriptString* lexems);
		static bool Item_LockerOpen(Item* item);
		static bool Item_LockerClose(Item* item);
		static bool Item_IsCar(Item* item);
		static Item* Item_CarGetBag(Item* item, int num_bag);

		static void Item_EventFinish(Item* item, bool deleted);
		static bool Item_EventAttack(Item* item, Critter* attacker, Critter* target);
		static bool Item_EventUse(Item* item, Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery);
		static bool Item_EventUseOnMe(Item* item, Critter* cr, Item* used_item);
		static bool Item_EventSkill(Item* item, Critter* cr, int skill);
		static void Item_EventDrop(Item* item, Critter* cr);
		static void Item_EventMove(Item* item, Critter* cr, BYTE from_slot);
		static void Item_EventWalk(Item* item, Critter* cr, bool entered, BYTE dir);

		static void Item_set_Flags(Item* item, DWORD value);
		static DWORD Item_get_Flags(Item* item);
		static void Item_set_TrapValue(Item* item, short value);
		static short Item_get_TrapValue(Item* item);
		
/*#define ITEM_CHANGED_FUNC(prop,data_prop,type) \
	static type Item_get_##prop(Item* item)\
	{\
		return item->Data.data_prop;\
	}\
	static void Item_set_##prop(Item* item, type val)\
	{\
		item->Data.data_prop=val;\
		ItemMngr.AddChangedItem(item);\
	}
		ITEM_CHANGED_FUNC(SortValue,SortValue,WORD);
		ITEM_CHANGED_FUNC(Info,Info,BYTE);
		ITEM_CHANGED_FUNC(PicMap,PicMap,WORD);
		ITEM_CHANGED_FUNC(PicInv,PicInv,WORD);
		ITEM_CHANGED_FUNC(AnimWaitBase,AnimWaitBase,WORD);
		ITEM_CHANGED_FUNC(AnimStayBegin,AnimStay[0],BYTE);
		ITEM_CHANGED_FUNC(AnimStayEnd,AnimStay[1],BYTE);
		ITEM_CHANGED_FUNC(AnimShowBegin,AnimShow[0],BYTE);
		ITEM_CHANGED_FUNC(AnimShowEnd,AnimShow[1],BYTE);
		ITEM_CHANGED_FUNC(AnimHideBegin,AnimHide[0],BYTE);
		ITEM_CHANGED_FUNC(AnimHideEnd,AnimHide[1],BYTE);
		ITEM_CHANGED_FUNC(Cost,Cost,DWORD);
		ITEM_CHANGED_FUNC(Val0,ScriptValues[0],int);
		ITEM_CHANGED_FUNC(Val1,ScriptValues[1],int);
		ITEM_CHANGED_FUNC(Val2,ScriptValues[2],int);
		ITEM_CHANGED_FUNC(Val3,ScriptValues[3],int);
		ITEM_CHANGED_FUNC(Val4,ScriptValues[4],int);
		ITEM_CHANGED_FUNC(Val5,ScriptValues[5],int);
		ITEM_CHANGED_FUNC(Val6,ScriptValues[6],int);
		ITEM_CHANGED_FUNC(Val7,ScriptValues[7],int);
		ITEM_CHANGED_FUNC(Val8,ScriptValues[8],int);
		ITEM_CHANGED_FUNC(Val9,ScriptValues[9],int);
		ITEM_CHANGED_FUNC(LightIntensity,LightIntensity,char);
		ITEM_CHANGED_FUNC(LightRadius,LightRadius,BYTE);
		ITEM_CHANGED_FUNC(LightFlags,LightFlags,BYTE);
		ITEM_CHANGED_FUNC(LightColor,LightColor,DWORD);
		ITEM_CHANGED_FUNC(TrapValue,TrapValue,short);
		ITEM_CHANGED_FUNC(BrokenFlags,TechInfo.DeteorationFlags,BYTE);
		ITEM_CHANGED_FUNC(BrokenCount,TechInfo.DeteorationCount,BYTE);
		ITEM_CHANGED_FUNC(BrokenWear,TechInfo.DeteorationValue,WORD);
		ITEM_CHANGED_FUNC(WeaponAmmoPid,TechInfo.AmmoPid,WORD);
		ITEM_CHANGED_FUNC(WeaponAmmoCount,TechInfo.AmmoCount,WORD);
		ITEM_CHANGED_FUNC(LockerId,Locker.DoorId,DWORD);
		ITEM_CHANGED_FUNC(LockerCondition,Locker.Condition,WORD);
		ITEM_CHANGED_FUNC(LockerComplexity,Locker.Complexity,WORD);
		ITEM_CHANGED_FUNC(CarFuel,Car.Fuel,WORD);
		ITEM_CHANGED_FUNC(CarDeteoration,Car.Deteoration,WORD);
		ITEM_CHANGED_FUNC(RadioChannel,Radio.Channel,WORD);
		ITEM_CHANGED_FUNC(HolodiskNumber,Holodisk.Number,DWORD);*/

		static bool Crit_IsPlayer(Critter* cr);
		static bool Crit_IsNpc(Critter* cr);
		static bool Crit_IsCanWalk(Critter* cr);
		static bool Crit_IsCanRun(Critter* cr);
		static bool Crit_IsCanRotate(Critter* cr);
		static bool Crit_IsCanAim(Critter* cr);
		static bool Crit_IsAnim1(Critter* cr, DWORD index);
		static bool Crit_IsAnim3d(Critter* cr);
		static int Cl_GetAccess(Critter* cl);
		static bool Crit_SetEvent(Critter* cr, int event_type, CScriptString* func_name);
		static void Crit_SetLexems(Critter* cr, CScriptString* lexems);
		static Map* Crit_GetMap(Critter* cr);
		static DWORD Crit_GetMapId(Critter* cr);
		static WORD Crit_GetMapProtoId(Critter* cr);
		static void Crit_SetHomePos(Critter* cr, WORD hx, WORD hy, BYTE dir);
		static void Crit_GetHomePos(Critter* cr, DWORD& map_id, WORD& hx, WORD& hy, BYTE& dir);
		static bool Crit_ChangeCrType(Critter* cr, DWORD new_type);
		static void Cl_DropTimers(Critter* cl);
		static bool Crit_MoveRandom(Critter* cr);
		static bool Crit_MoveToDir(Critter* cr, BYTE direction);
		static bool Crit_TransitToHex(Critter* cr, WORD hx, WORD hy, BYTE dir);
		static bool Crit_TransitToMapHex(Critter* cr, DWORD map_id, WORD hx, WORD hy, BYTE dir);
		static bool Crit_TransitToMapEntire(Critter* cr, DWORD map_id, BYTE entire);
		static bool Crit_TransitToGlobal(Critter* cr, bool request_group);
		static bool Crit_TransitToGlobalWithGroup(Critter* cr, asIScriptArray& group);
		static bool Crit_TransitToGlobalGroup(Critter* cr, DWORD critter_id);
		static bool Crit_IsLife(Critter* cr);
		static bool Crit_IsKnockout(Critter* cr);
		static bool Crit_IsDead(Critter* cr);
		static bool Crit_IsFree(Critter* cr);
		static bool Crit_IsBusy(Critter* cr);
		static void Crit_Wait(Critter* cr, DWORD ms);
		static void Crit_ToDead(Critter* cr, BYTE dead_type, Critter* killer);
		static bool Crit_ToLife(Critter* cr);
		static bool Crit_ToKnockout(Critter* cr, bool face_up, DWORD lose_ap, WORD knock_hx, WORD knock_hy);
		static void Crit_RefreshVisible(Critter* cr);
		static void Crit_ViewMap(Critter* cr, Map* map, DWORD look, WORD hx, WORD hy, BYTE dir);
		static void Crit_AddScore(Critter* cr, int score, int val);
		static void Crit_AddHolodiskInfo(Critter* cr, DWORD holodisk_num);
		static void Crit_EraseHolodiskInfo(Critter* cr, DWORD holodisk_num);
		static bool Crit_IsHolodiskInfo(Critter* cr, DWORD holodisk_num);
		static void Crit_Say(Critter* cr, BYTE how_say, CScriptString& text);
		static void Crit_SayMsg(Critter* cr, BYTE how_say, WORD text_msg, DWORD num_str);
		static void Crit_SayMsgLex(Critter* cr, BYTE how_say, WORD text_msg, DWORD num_str, CScriptString& lexems);
		static void Crit_SetDir(Critter* cr, BYTE dir);
		static bool Crit_PickItem(Critter* cr, WORD hx, WORD hy, WORD pid);
		static void Crit_SetFavoriteItem(Critter* cr, int slot, WORD pid);
		static WORD Crit_GetFavoriteItem(Critter* cr, int slot);
		static DWORD Crit_GetCritters(Critter* cr, bool look_on_me, int find_type, asIScriptArray* critters);
		static DWORD Crit_GetFollowGroup(Critter* cr, int find_type, asIScriptArray* critters);
		static Critter* Crit_GetFollowLeader(Critter* cr);
		static asIScriptArray* Crit_GetGlobalGroup(Critter* cr);
		static bool Crit_IsGlobalGroupLeader(Critter* cr);
		static void Crit_LeaveGlobalGroup(Critter* cr);
		static void Crit_GiveGlobalGroupLead(Critter* cr, Critter* to_cr);
		static DWORD Npc_GetTalkedPlayers(Critter* cr, asIScriptArray* players);
		static bool Crit_IsSeeCr(Critter* cr, Critter* cr_);
		static bool Crit_IsSeenByCr(Critter* cr, Critter* cr_);
		static bool Crit_IsSeeItem(Critter* cr, Item* item);
		static Item* Crit_AddItem(Critter* cr, WORD pid, DWORD count);
		static bool Crit_DeleteItem(Critter* cr, WORD pid, DWORD count);
		static DWORD Crit_ItemsCount(Critter* cr);
		static DWORD Crit_ItemsWeight(Critter* cr);
		static DWORD Crit_ItemsVolume(Critter* cr);
		static DWORD Crit_CountItem(Critter* cr, WORD proto_id);
		static Item* Crit_GetItem(Critter* cr, WORD proto_id, int slot);
		static Item* Crit_GetItemById(Critter* cr, DWORD item_id);
		static DWORD Crit_GetItems(Critter* cr, int slot, asIScriptArray* items);
		static DWORD Crit_GetItemsByType(Critter* cr, int type, asIScriptArray* items);
		static ProtoItem* Crit_GetSlotProto(Critter* cr, int slot);
		static bool Crit_MoveItem(Critter* cr, DWORD item_id, DWORD count, BYTE to_slot);

		static DWORD Npc_ErasePlane(Critter* npc, int plane_type, bool all);
		static bool Npc_ErasePlaneIndex(Critter* npc, DWORD index);
		static void Npc_DropPlanes(Critter* npc);
		static bool Npc_IsNoPlanes(Critter* npc);
		static bool Npc_IsCurPlane(Critter* npc, int plane_type);
		static AIDataPlane* Npc_GetCurPlane(Critter* npc);
		static DWORD Npc_GetPlanes(Critter* npc, asIScriptArray* arr);
		static DWORD Npc_GetPlanesIdentifier(Critter* npc, int identifier, asIScriptArray* arr);
		static DWORD Npc_GetPlanesIdentifier2(Critter* npc, int identifier, DWORD identifier_ext, asIScriptArray* arr);
		static bool Npc_AddPlane(Critter* npc, AIDataPlane& plane);

		static void Crit_SendMessage(Critter* cr, int num, int val, int to);
		static void Crit_SendCombatResult(Critter* cr, asIScriptArray& arr);
		static void Crit_Action(Critter* cr, int action, int action_ext, Item* item);
		static void Crit_Animate(Critter* cr, DWORD anim1, DWORD anim2, Item* item, bool clear_sequence, bool delay_play);
		static void Crit_PlaySound(Critter* cr, CScriptString& sound_name, bool send_self);
		static void Crit_PlaySoundType(Critter* cr, BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext, bool send_self);

		static bool Cl_IsKnownLoc(Critter* cl, bool by_id, DWORD loc_num);
		static bool Cl_SetKnownLoc(Critter* cl, bool by_id, DWORD loc_num);
		static bool Cl_UnsetKnownLoc(Critter* cl, bool by_id, DWORD loc_num);
		static void Cl_SetFog(Critter* cl, WORD zone_x, WORD zone_y, int fog);
		static int Cl_GetFog(Critter* cl, WORD zone_x, WORD zone_y);
		static void Crit_SetTimeout(Critter* cr, int num_timeout, DWORD time);
		static DWORD Crit_GetTimeout(Critter* cr, DWORD num_timeout);
		static void Cl_ShowContainer(Critter* cl, Critter* cr_cont, Item* item_cont, BYTE transfer_type);
		static void Cl_ShowScreen(Critter* cl, int screen_type, DWORD param, CScriptString& func_name);
		static void Cl_RunClientScript(Critter* cl, CScriptString& func_name, int p0, int p1, int p2, CScriptString* p3, asIScriptArray* p4);
		static void Cl_Disconnect(Critter* cl);

		static bool Crit_SetScript(Critter* cr, CScriptString* script);
		static DWORD Crit_GetScriptId(Critter* cr);
		static DWORD Crit_GetBagId(Critter* cr);
		static void Crit_SetBagRefreshTime(Critter* cr, DWORD real_minutes);
		static DWORD Crit_GetBagRefreshTime(Critter* cr);
		static void Crit_SetInternalBag(Critter* cr, asIScriptArray& pids, asIScriptArray* min_counts, asIScriptArray* max_counts, asIScriptArray* slots);
		static DWORD Crit_GetInternalBag(Critter* cr, asIScriptArray* pids, asIScriptArray* min_counts, asIScriptArray* max_counts, asIScriptArray* slots);
		static void Crit_SetDialogId(Critter* cr, DWORD dialog_id);
		static DWORD Crit_GetDialogId(Critter* cr);
		static WORD Crit_GetProtoId(Critter* cr);

		static void Crit_AddEnemyInStack(Critter* cr, DWORD critter_id);
		static bool Crit_CheckEnemyInStack(Critter* cr, DWORD critter_id);
		static void Crit_EraseEnemyFromStack(Critter* cr, DWORD critter_id);
		static void Crit_ChangeEnemyStackSize(Critter* cr, DWORD new_size);
		static void Crit_GetEnemyStack(Critter* cr, asIScriptArray& arr);
		static void Crit_ClearEnemyStack(Critter* cr);
		static void Crit_ClearEnemyStackNpc(Critter* cr);

		static bool Crit_AddTimeEvent(Critter* cr, CScriptString& func_name, DWORD duration, int identifier);
		static bool Crit_AddTimeEventRate(Critter* cr, CScriptString& func_name, DWORD duration, int identifier, DWORD rate);
		static DWORD Crit_GetTimeEvents(Critter* cr, int identifier, asIScriptArray* indexes, asIScriptArray* durations, asIScriptArray* rates);
		static DWORD Crit_GetTimeEventsArr(Critter* cr, asIScriptArray& find_identifiers, asIScriptArray* identifiers, asIScriptArray* indexes, asIScriptArray* durations, asIScriptArray* rates);
		static void Crit_ChangeTimeEvent(Critter* cr, DWORD index, DWORD new_duration, DWORD new_rate);
		static void Crit_EraseTimeEvent(Critter* cr, DWORD index);
		static DWORD Crit_EraseTimeEvents(Critter* cr, int identifier);
		static DWORD Crit_EraseTimeEventsArr(Critter* cr, asIScriptArray& identifiers);

		static void Crit_EventIdle(Critter* cr);
		static void Crit_EventFinish(Critter* cr, bool deleted);
		static void Crit_EventDead(Critter* cr, Critter* killer);
		static void Crit_EventRespawn(Critter* cr);
		static void Crit_EventShowCritter(Critter* cr, Critter* show_cr);
		static void Crit_EventShowCritter1(Critter* cr, Critter* show_cr);
		static void Crit_EventShowCritter2(Critter* cr, Critter* show_cr);
		static void Crit_EventShowCritter3(Critter* cr, Critter* show_cr);
		static void Crit_EventHideCritter(Critter* cr, Critter* hide_cr);
		static void Crit_EventHideCritter1(Critter* cr, Critter* hide_cr);
		static void Crit_EventHideCritter2(Critter* cr, Critter* hide_cr);
		static void Crit_EventHideCritter3(Critter* cr, Critter* hide_cr);
		static void Crit_EventShowItemOnMap(Critter* cr, Item* show_item, bool added, Critter* dropper);
		static void Crit_EventChangeItemOnMap(Critter* cr, Item* item);
		static void Crit_EventHideItemOnMap(Critter* cr, Item* hide_item, bool removed, Critter* picker);
		static bool Crit_EventAttack(Critter* cr, Critter* target);
		static bool Crit_EventAttacked(Critter* cr, Critter* attacker);
		static bool Crit_EventStealing(Critter* cr, Critter* thief, Item* item, DWORD count);
		static void Crit_EventMessage(Critter* cr, Critter* from_cr, int message, int value);
		static bool Crit_EventUseItem(Critter* cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery);
		static bool Crit_EventUseSkill(Critter* cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery);
		static void Crit_EventDropItem(Critter* cr, Item* item);
		static void Crit_EventMoveItem(Critter* cr, Item* item, BYTE from_slot);
		static void Crit_EventKnockout(Critter* cr, bool face_up, DWORD lost_ap, DWORD knock_dist);
		static void Crit_EventSmthDead(Critter* cr, Critter* from_cr, Critter* killer);
		static void Crit_EventSmthStealing(Critter* cr, Critter* from_cr, Critter* thief, bool success, Item* item, DWORD count);
		static void Crit_EventSmthAttack(Critter* cr, Critter* from_cr, Critter* target);
		static void Crit_EventSmthAttacked(Critter* cr, Critter* from_cr, Critter* attacker);
		static void Crit_EventSmthUseItem(Critter* cr, Critter* from_cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery);
		static void Crit_EventSmthUseSkill(Critter* cr, Critter* from_cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery);
		static void Crit_EventSmthDropItem(Critter* cr, Critter* from_cr, Item* item);
		static void Crit_EventSmthMoveItem(Critter* cr, Critter* from_cr, Item* item, BYTE from_slot);
		static void Crit_EventSmthKnockout(Critter* cr, Critter* from_cr, bool face_up, DWORD lost_ap, DWORD knock_dist);
		static int Crit_EventPlaneBegin(Critter* cr, AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item);
		static int Crit_EventPlaneEnd(Critter* cr, AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item);
		static int Crit_EventPlaneRun(Critter* cr, AIDataPlane* plane, int reason, DWORD& p0, DWORD& p1, DWORD& p2);
		static bool Crit_EventBarter(Critter* cr, Critter* cr_barter, bool attach, DWORD barter_count);
		static bool Crit_EventTalk(Critter* cr, Critter* cr_talk, bool attach, DWORD talk_count);
		static bool Crit_EventGlobalProcess(Critter* cr, int type, asIScriptArray* group, Item* car, DWORD& x, DWORD& y, DWORD& to_x, DWORD& to_y, DWORD& speed, DWORD& encounter_descriptor, bool& wait_for_answer);
		static bool Crit_EventGlobalInvite(Critter* cr, asIScriptArray* group, Item* car, DWORD encounter_descriptor, int combat_mode, DWORD& map_id, WORD& hx, WORD& hy, BYTE& dir);
		static void Crit_EventTurnBasedProcess(Critter* cr, Map* map, bool begin_turn);
		static void Crit_EventSmthTurnBasedProcess(Critter* cr, Critter* from_cr, Map* map, bool begin_turn);

		static void Cl_SendQuestVar(Critter* cl, GameVar* var);
		static GameVar* Global_GetGlobalVar(WORD tvar_id);
		static GameVar* Global_GetLocalVar(WORD tvar_id, DWORD master_id);
		static GameVar* Global_GetUnicumVar(WORD tvar_id, DWORD master_id, DWORD slave_id);
		static DWORD Global_DeleteVars(DWORD id);

		static DWORD Map_GetId(Map* map);
		static WORD Map_GetProtoId(Map* map);
		static Location* Map_GetLocation(Map* map);
		static bool Map_SetScript(Map* map, CScriptString* script);
		static DWORD Map_GetScriptId(Map* map);
		static bool Map_SetEvent(Map* map, int event_type, CScriptString* func_name);
		static void Map_SetLoopTime(Map* map, DWORD loop_num, DWORD ms);
		static BYTE Map_GetRain(Map* map);
		static void Map_SetRain(Map* map, BYTE capacity);
		static int Map_GetTime(Map* map);
		static void Map_SetTime(Map* map, int time);
		static DWORD Map_GetDayTime(Map* map, DWORD day_part);
		static void Map_SetDayTime(Map* map, DWORD day_part, DWORD time);
		static void Map_GetDayColor(Map* map, DWORD day_part, BYTE& r, BYTE& g, BYTE& b);
		static void Map_SetDayColor(Map* map, DWORD day_part, BYTE r, BYTE g, BYTE b);
		static void Map_SetTurnBasedAvailability(Map* map, bool value);
		static bool Map_IsTurnBasedAvailability(Map* map);
		static void Map_BeginTurnBased(Map* map, Critter* first_turn_crit);
		static bool Map_IsTurnBased(Map* map);
		static void Map_EndTurnBased(Map* map);
		static int Map_GetTurnBasedSequence(Map* map, asIScriptArray& critters_ids);
		static void Map_SetData(Map* map, DWORD index, int value);
		static int Map_GetData(Map* map, DWORD index);
		static Item* Map_AddItem(Map* map, WORD hx, WORD hy, WORD proto_id, DWORD count);
		static DWORD Map_GetItemsHex(Map* map, WORD hx, WORD hy, asIScriptArray* items);
		static DWORD Map_GetItemsHexEx(Map* map, WORD hx, WORD hy, DWORD radius, WORD pid, asIScriptArray* items);
		static DWORD Map_GetItemsByPid(Map* map, WORD pid, asIScriptArray* items);
		static DWORD Map_GetItemsByType(Map* map, int type, asIScriptArray* items);
		static Item* Map_GetItem(Map* map, DWORD item_id);
		static Item* Map_GetItemHex(Map* map, WORD hx, WORD hy, WORD pid);
		static Item* Map_GetDoor(Map* map, WORD hx, WORD hy);
		static Item* Map_GetCar(Map* map, WORD hx, WORD hy);
		static MapObject* Map_GetSceneryHex(Map* map, WORD hx, WORD hy, WORD pid);
		static DWORD Map_GetSceneriesHex(Map* map, WORD hx, WORD hy, asIScriptArray* sceneries);
		static DWORD Map_GetSceneriesHexEx(Map* map, WORD hx, WORD hy, DWORD radius, WORD pid, asIScriptArray* sceneries);
		static DWORD Map_GetSceneriesByPid(Map* map, WORD pid, asIScriptArray* sceneries);
		static Critter* Map_GetCritterHex(Map* map, WORD hx, WORD hy);
		static Critter* Map_GetCritterById(Map* map, DWORD crid);
		static DWORD Map_GetCritters(Map* map, WORD hx, WORD hy, DWORD radius, int find_type, asIScriptArray* critters);
		static DWORD Map_GetCrittersByPids(Map* map, WORD pid, int find_type, asIScriptArray* critters);
		static DWORD Map_GetCrittersInPath(Map* map, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float angle, DWORD dist, int find_type, asIScriptArray* critters);
		static DWORD Map_GetCrittersInPathBlock(Map* map, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float angle, DWORD dist, int find_type, asIScriptArray* critters, WORD& pre_block_hx, WORD& pre_block_hy, WORD& block_hx, WORD& block_hy);
		static DWORD Map_GetCrittersWhoViewPath(Map* map, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, int find_type, asIScriptArray* critters);
		static DWORD Map_GetCrittersSeeing(Map* map, asIScriptArray& critters, bool look_on_them, int find_type, asIScriptArray* result_critters);
		static void Map_GetHexInPath(Map* map, WORD from_hx, WORD from_hy, WORD& to_hx, WORD& to_hy, float angle, DWORD dist);
		static void Map_GetHexInPathWall(Map* map, WORD from_hx, WORD from_hy, WORD& to_hx, WORD& to_hy, float angle, DWORD dist);
		static DWORD Map_GetPathLength(Map* map, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, DWORD cut);
		static Critter* Map_AddNpc(Map* map, WORD proto_id, WORD hx, WORD hy, BYTE dir, asIScriptArray* params, asIScriptArray* items, CScriptString* script);
		static DWORD Map_GetNpcCount(Map* map, int npc_role, int find_type);
		static Critter* Map_GetNpc(Map* map, int npc_role, int find_type, DWORD skip_count);
		static DWORD Map_CountEntire(Map* map, int entire_num);
		static DWORD Map_GetEntires(Map* map, int entire, asIScriptArray* entires, asIScriptArray* hx, asIScriptArray* hy);
		static bool Map_GetEntireCoords(Map* map, int entire_num, DWORD skip, WORD& hx, WORD& hy);
		static bool Map_GetNearEntireCoords(Map* map, int& entire, WORD& hx, WORD& hy);
		static bool Map_IsHexPassed(Map* map, WORD hex_x, WORD hex_y);
		static bool Map_IsHexRaked(Map* map, WORD hex_x, WORD hex_y);
		static void Map_SetText(Map* map, WORD hex_x, WORD hex_y, DWORD color, CScriptString& text);
		static void Map_SetTextMsg(Map* map, WORD hex_x, WORD hex_y, DWORD color, WORD text_msg, DWORD str_num);
		static void Map_SetTextMsgLex(Map* map, WORD hex_x, WORD hex_y, DWORD color, WORD text_msg, DWORD str_num, CScriptString& lexems);
		static void Map_RunEffect(Map* map, WORD eff_pid, WORD hx, WORD hy, DWORD radius);
		static void Map_RunFlyEffect(Map* map, WORD eff_pid, Critter* from_cr, Critter* to_cr, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy);
		static bool Map_CheckPlaceForCar(Map* map, WORD hx, WORD hy, WORD pid);
		static void Map_BlockHex(Map* map, WORD hx, WORD hy, bool full);
		static void Map_UnblockHex(Map* map, WORD hx, WORD hy);
		static void Map_PlaySound(Map* map, CScriptString& sound_name);
		static void Map_PlaySoundRadius(Map* map, CScriptString& sound_name, WORD hx, WORD hy, DWORD radius);
		static bool Map_Reload(Map* map);
		static WORD Map_GetWidth(Map* map);
		static WORD Map_GetHeight(Map* map);
		static void Map_MoveHexByDir(Map* map, WORD& hx, WORD& hy, BYTE dir, DWORD steps);
		static bool Map_VerifyTrigger(Map* map, Critter* cr, WORD hx, WORD hy, BYTE dir);

		static void Map_EventFinish(Map* map, bool deleted);
		static void Map_EventLoop0(Map* map);
		static void Map_EventLoop1(Map* map);
		static void Map_EventLoop2(Map* map);
		static void Map_EventLoop3(Map* map);
		static void Map_EventLoop4(Map* map);
		static void Map_EventInCritter(Map* map, Critter* cr);
		static void Map_EventOutCritter(Map* map, Critter* cr);
		static void Map_EventCritterDead(Map* map, Critter* cr, Critter* killer);
		static void Map_EventTurnBasedBegin(Map* map);
		static void Map_EventTurnBasedEnd(Map* map);
		static void Map_EventTurnBasedProcess(Map* map, Critter* cr, bool begin_turn);

		static DWORD Location_GetId(Location* loc);
		static WORD Location_GetProtoId(Location* loc);
		static DWORD Location_GetMapCount(Location* loc);
		static Map* Location_GetMap(Location* loc, WORD map_pid);
		static Map* Location_GetMapByIndex(Location* loc, DWORD index);
		static DWORD Location_GetMaps(Location* loc, asIScriptArray* arr);
		static bool Location_Reload(Location* loc);
		static void Location_Update(Location* loc);

		static void Global_Log(CScriptString& text);
		static ProtoItem* Global_GetProtoItem(WORD pid);
		static Item* Global_GetItem(DWORD item_id);
		static DWORD Global_GetCrittersDistantion(Critter* cr1, Critter* cr2);
		static DWORD Global_GetDistantion(WORD hx1, WORD hy1, WORD hx2, WORD hy2);
		static BYTE Global_GetDirection(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy);
		static BYTE Global_GetOffsetDir(WORD hx, WORD hy, WORD tx, WORD ty, float offset);
		static void Global_MoveItemCr(Item* item, DWORD count, Critter* to_cr);
		static void Global_MoveItemMap(Item* item, DWORD count, Map* to_map, WORD to_hx, WORD to_hy);
		static void Global_MoveItemCont(Item* item, DWORD count, Item* to_cont, DWORD special_id);
		static void Global_MoveItemsCr(asIScriptArray& items, Critter* to_cr);
		static void Global_MoveItemsMap(asIScriptArray& items, Map* to_map, WORD to_hx, WORD to_hy);
		static void Global_MoveItemsCont(asIScriptArray& items, Item* to_cont, DWORD stack_id);
		static void Global_DeleteItem(Item* item);
		static void Global_DeleteItems(asIScriptArray& items);
		static void Global_DeleteNpc(Critter* npc);
		static void Global_DeleteNpcForce(Critter* npc);
		static void Global_RadioMessage(WORD channel, CScriptString& text);
		static void Global_RadioMessageMsg(WORD channel, WORD text_msg, DWORD num_str);
		static DWORD Global_GetFullMinute(WORD year, WORD month, WORD day, WORD hour, WORD minute);
		static void Global_GetGameTime(DWORD full_minute, WORD& year, WORD& month, WORD& day, WORD& day_of_week, WORD& hour, WORD& minute);
		static DWORD Global_CreateLocation(WORD loc_pid, WORD wx, WORD wy, asIScriptArray* critters);
		static void Global_DeleteLocation(DWORD loc_id);
		static void Global_GetProtoCritter(WORD proto_id, asIScriptArray& data);
		static Critter* Global_GetCritter(DWORD crid);
		static Critter* Global_GetPlayer(CScriptString& name);
		static DWORD Global_GetPlayerId(CScriptString& name);
		static CScriptString* Global_GetPlayerName(DWORD id);
		static DWORD Global_CreateTimeEventEmpty(DWORD begin_minute, CScriptString& script_name, bool save);
		static DWORD Global_CreateTimeEventDw(DWORD begin_minute, CScriptString& script_name, DWORD dw, bool save);
		static DWORD Global_CreateTimeEventDws(DWORD begin_minute, CScriptString& script_name, asIScriptArray& dw, bool save);
		static DWORD Global_CreateTimeEventCr(DWORD begin_minute, CScriptString& script_name, Critter* cr, bool save);
		static DWORD Global_CreateTimeEventItem(DWORD begin_minute, CScriptString& script_name, Item* item, bool save);
		static DWORD Global_CreateTimeEventArr(DWORD begin_minute, CScriptString& script_name, asIScriptArray* critters, asIScriptArray* items, bool save);
		static void Global_EraseTimeEvent(DWORD num);
		static bool Global_SetAnyData(CScriptString& name, asIScriptArray& data);
		static bool Global_SetAnyDataSize(CScriptString& name, asIScriptArray& data, DWORD data_size_bytes);
		static bool Global_GetAnyData(CScriptString& name, asIScriptArray& data);
		static bool Global_IsAnyData(CScriptString& name);
		static void Global_EraseAnyData(CScriptString& name);
		static void Global_ArrayPushBackInteger(asIScriptArray* arr, void* value);
		static void Global_ArrayPushBackCritter(asIScriptArray* arr, Critter* cr);
		//static void Global_ArrayErase(asIScriptArray* arr, DWORD index);
		static Map* Global_GetMap(DWORD map_id);
		static Map* Global_GetMapByPid(WORD map_pid, DWORD skip_count);
		static Location* Global_GetLocation(DWORD loc_id);
		static Location* Global_GetLocationByPid(WORD loc_pid, DWORD skip_count);
		static DWORD Global_GetLocations(WORD wx, WORD wy, DWORD radius, asIScriptArray* locations);
		static bool Global_StrToInt(CScriptString& text, int& result);
		static bool Global_RunDialogNpc(Critter* player, Critter* npc, bool ignore_distance);
		static bool Global_RunDialogNpcDlgPack(Critter* player, Critter* npc, DWORD dlg_pack, bool ignore_distance);
		static bool Global_RunDialogHex(Critter* player, DWORD dlg_pack, WORD hx, WORD hy, bool ignore_distance);
		static __int64 Global_WorldItemCount(WORD pid);
		static void Global_SetBestScore(int score, Critter* cl, CScriptString& name);
		static bool Global_AddTextListener(int say_type, CScriptString& first_str, WORD parameter, CScriptString& script_name);
		static void Global_EraseTextListener(int say_type, CScriptString& first_str, WORD parameter);
		static AIDataPlane* Global_CreatePlane();
		static DWORD Global_GetBagItems(DWORD bag_id, asIScriptArray* pids, asIScriptArray* min_counts, asIScriptArray* max_counts, asIScriptArray* slots);
		static void Global_SetSendParameter(int index, bool enabled);
		static void Global_SetSendParameterCond(int index, bool enabled, int condition_index, int condition_mask);
		static bool Global_SwapCritters(Critter* cr1, Critter* cr2, bool with_inventory, bool with_vars);
		static DWORD Global_GetAllItems(WORD pid, asIScriptArray* items);
		static DWORD Global_GetAllNpc(WORD pid, asIScriptArray* npc);
		static DWORD Global_GetAllMaps(WORD pid, asIScriptArray* maps);
		static DWORD Global_GetAllLocations(WORD pid, asIScriptArray* locations);
		static DWORD Global_GetScriptId(CScriptString& script_name, CScriptString& func_decl);
		static CScriptString* Global_GetScriptName(DWORD script_id);
		static asIScriptArray* Global_GetItemDataMask(int mask_type);
		static bool Global_SetItemDataMask(int mask_type, asIScriptArray& mask);
		static DWORD Global_GetTick(){return Timer::FastTick();}
		static void Global_GetTime(WORD& year, WORD& month, WORD& day, WORD& day_of_week, WORD& hour, WORD& minute, WORD& second, WORD& milliseconds);
		static bool Global_SetParameterGetBehaviour(DWORD index, CScriptString& func_name);
		static bool Global_SetParameterChangeBehaviour(DWORD index, CScriptString& func_name);
		static void Global_AllowSlot(BYTE index, CScriptString& ini_option);
		static void Global_SetRegistrationParam(DWORD index, bool enabled);
		static DWORD Global_GetAngelScriptProperty(int property);
		static bool Global_SetAngelScriptProperty(int property, DWORD value);
		static DWORD Global_GetStrHash(CScriptString* str);
		//static DWORD Global_GetVersion();
		static bool Global_IsCritterCanWalk(DWORD cr_type);
		static bool Global_IsCritterCanRun(DWORD cr_type);
		static bool Global_IsCritterCanRotate(DWORD cr_type);
		static bool Global_IsCritterCanAim(DWORD cr_type);
		static bool Global_IsCritterAnim1(DWORD cr_type, DWORD index);
		static bool Global_IsCritterAnim3d(DWORD cr_type);
		static int Global_GetGlobalMapRelief(DWORD x, DWORD y);
	} ScriptFunc;

	// Items
	static Item* CreateItemOnHex(Map* map, WORD hx, WORD hy, WORD pid, DWORD count);
	static Item* CreateItemToHexCr(Critter* cr, WORD hx, WORD hy, WORD pid, DWORD count);

	void FindCritterItems(Critter* cr);
	bool TransferAllItems();

	// Npc
	void ProcessAI(Npc* npc);
	bool AI_Stay(Npc* npc, DWORD ms);
	bool AI_Move(Npc* npc, WORD hx, WORD hy, bool is_run, BYTE cut, BYTE trace);
	bool AI_MoveToCrit(Npc* npc, DWORD targ_id, DWORD cut, DWORD trace, bool is_run);
	bool AI_MoveItem(Npc* npc, Map* map, BYTE from_slot, BYTE to_slot, DWORD item_id, DWORD count);
	bool AI_Attack(Npc* npc, Map* map, BYTE use, BYTE aim, DWORD targ_id);
	bool AI_PickItem(Npc* npc, Map* map, WORD hx, WORD hy, WORD pid, DWORD use_item_id);
	bool AI_ReloadWeapon(Npc* npc, Map* map, Item* weap, DWORD ammo_id);
	void TraceFireLine(Map* map, WORD hx, WORD hy, float sx, float sy, DWORD dist, Npc* npc, WordPairVec& positions, DWORD step);
	//DWORD AIGetAttackPosition(Npc* npc, Map* map, WORD& hx, WORD& hy);
	bool TransferAllNpc();
	void ProcessCritter(Critter* cr);
	bool Dialog_Compile(Npc* npc, Client* cl, const Dialog& base_dlg, Dialog& compiled_dlg);
	bool Dialog_CheckDemand(Npc* npc, Client* cl, DialogAnswer& answer, bool recheck);
	void Dialog_UseResult(Npc* npc, Client* cl, DialogAnswer& answer);
	void Dialog_Begin(Client* cl, Npc* npc, DWORD dlg_pack_id, WORD hx, WORD hy, bool ignore_distance);

	// Radio
#ifdef RADIO_SAFE
	DwordVec* RadioChannels[0x10000];
#else
	CrVec* RadioChannels[0x10000];
#endif

	void RadioClearChannels();
	void RadioAddPlayer(Client* cl, WORD channel);
	void RadioErsPlayer(Client* cl, WORD channel);
	void RadioSendText(Critter* cr, WORD channel, const char* text, bool unsafe_text);
	void RadioSendMsg(Critter* cr, WORD channel, WORD text_msg, DWORD num_str);

	// Main
	static FOServer* Self;
	static int UpdateIndex,UpdateLastIndex;
	static DWORD UpdateLastTick;
	HWND ServerWindow;
	volatile bool Active;
	FileManager FileMngr;
	ClVec ProcessClients;
	ClVec SaveClients;
	DwordMap RegIp;

	void DisconnectClient(Client* cl);
	void RemoveClient(Client* cl);
	void EraseSaveClient(DWORD crid);
	void Process(ClientPtr& acl);

	// Log to client
	static ClVec LogClients;
	static CRITICAL_SECTION LogClientsCS;
	static void LogToClients(char* str);

	// Game time
	DWORD GameTimeStartTick,GameTimeStartMinute;
	void SaveGameInfoFile();
	void LoadGameInfoFile(FILE* f, DWORD version);
	void InitGameTime();
	void ProcessGameTime();

	// Lang packs
	LangPackVec LangPacks;
	bool InitCrafts(LangPackVec& lang_packs);
	bool InitLangPacks(LangPackVec& lang_packs);
	bool InitLangPacksDialogs(LangPackVec& lang_packs);
	void FinishLangPacks();
	bool InitLangScript(LangPackVec& lang_packs);
	bool ReloadLangScript();
	bool InitLangCrTypes(LangPackVec& lang_packs);

	// Init/Finish
	bool Init();
	void Finish();
	bool IsInit(){return Active;}
	void RunGameLoop();

	// Net
	static HANDLE IOCompletionPort;
	static HANDLE* IOThreadHandles;
	static DWORD WorkThreadCount;
	static SOCKET ListenSock;
	static ClVec ConnectedClients;
	static CRITICAL_SECTION CSConnectedClients;

	static unsigned int __stdcall Net_Listen(HANDLE iocp); // Thread
	static unsigned int __stdcall Net_Work(HANDLE iocp); // Thread
	static void Net_Input(WSAOVERLAPPED_EX* io);
	static void Net_Output(WSAOVERLAPPED_EX* io);

	// Service
	DWORD VarsGarbageLastTick;
	void ClearUnusedVars();
	void VarsGarbarger(DWORD cycle_tick);

	// Dump save/load
	struct ClientSaveData
	{
		DWORD Id;
		char Name[MAX_NAME+1];
		char Password[MAX_NAME+1];
		CritData Data;
		CritDataExt DataExt;
		Critter::CrTimeEventVec TimeEvents;
	};
typedef vector<ClientSaveData> ClientSaveDataVec;
	static ClientSaveDataVec ClientsSaveData;
	static size_t ClientsSaveDataCount;

#define WORLD_SAVE_MAX_INDEX                  (9999)
#define WORLD_SAVE_DATA_BUFFER_SIZE           (10000000) // 10mb
	static PByteVec WorldSaveData;
	static size_t WorldSaveDataBufCount,WorldSaveDataBufFreeSize;

	static DWORD SaveWorldIndex,SaveWorldTime,SaveWorldNextTick;
	static DwordVec SaveWorldDeleteIndexes;
	static FILE* DumpFile;
	static HANDLE DumpBeginEvent,DumpEndEvent;
	static HANDLE DumpThreadHandle;

	void SaveWorld();
	bool LoadWorld();
	static void AddWorldSaveData(void* data, size_t size);
	static void AddClientSaveData(Client* cl);
	static unsigned int __stdcall Dump_Work(void* data); // Thread

	// Access
	StrVec AccessClient,AccessTester,AccessModer,AccessAdmin;

	// Banned
#define BANS_FNAME_ACTIVE         "active.txt"
#define BANS_FNAME_EXPIRED        "expired.txt"
	struct ClientBanned
	{
		SYSTEMTIME BeginTime;
		SYSTEMTIME EndTime;
		DWORD ClientIp;
		char ClientName[MAX_NAME+1];
		char BannedBy[MAX_NAME+1];
		char BanInfo[128];
		bool operator==(const char* name){return !_stricmp(name,ClientName);}
		bool operator==(const DWORD ip){return ClientIp==ip;}

		const char* GetBanLexems(){return Str::Format("$banby%s$time%d$reason%s",BannedBy[0]?BannedBy:"?",Timer::GetTimeDifference(EndTime,BeginTime)/60,BanInfo[0]?BanInfo:"just for fun");}
	};
typedef vector<ClientBanned> ClientBannedVec;
typedef vector<ClientBanned>::iterator ClientBannedVecIt;
	ClientBannedVec Banned;

	ClientBanned* GetBanByName(const char* name){ClientBannedVecIt it=std::find(Banned.begin(),Banned.end(),name); return it!=Banned.end()?&(*it):NULL;}
	ClientBanned* GetBanByIp(DWORD ip){ClientBannedVecIt it=std::find(Banned.begin(),Banned.end(),ip); return it!=Banned.end()?&(*it):NULL;}
	DWORD GetBanTime(ClientBanned& ban);
	void ProcessBans();
	void SaveBan(ClientBanned& ban, bool expired);
	void SaveBans();
	void LoadBans();

	// Clients data
	struct ClientData
	{
		char ClientName[MAX_NAME+1];
		char ClientPass[MAX_NAME+1];
		DWORD ClientId;
		DWORD SaveIndex;
		DWORD UID[5];
		DWORD UIDEndTick;
		void Clear(){ZeroMemory(this,sizeof(ClientData));}
		bool operator==(const char* name){return !_stricmp(name,ClientName);}
		bool operator==(const DWORD id){return ClientId==id;}
		ClientData(){Clear();}
	};
typedef vector<ClientData> ClientDataVec;
typedef vector<ClientData>::iterator ClientDataVecIt;
	ClientDataVec ClientsData;
	bool ShowUIDError;
	DWORD LastClientId;

	bool LoadClientsData();
	ClientData* GetClientData(const char* name){ClientDataVecIt it=std::find(ClientsData.begin(),ClientsData.end(),name); return it!=ClientsData.end()?&(*it):NULL;}
	ClientData* GetClientData(DWORD id){ClientDataVecIt it=std::find(ClientsData.begin(),ClientsData.end(),id); return it!=ClientsData.end()?&(*it):NULL;}
	bool SaveClient(Client* cl, bool deferred);
	bool LoadClient(Client* cl);

	// Lags
	DWORD CycleBeginTick;

	// Statistics
	struct Statistics_
	{
		DWORD ServerStartTick;
		DWORD Uptime;
		int FPS;
		int CycleTime;
		__int64 BytesSend;
		__int64 BytesRecv;
		__int64 DataReal;
		__int64 DataCompressed;
		float CompressRatio;
		DWORD MaxOnline;
		DWORD CurOnline;
		int LoopTime;
		int LoopCycles;
		int LoopMin;
		int LoopMax;
		int LagsCount;
	} static Statistics;

	DWORD PlayersInGame(){return CrMngr.PlayersInGame();}
	DWORD NpcInGame(){return CrMngr.NpcInGame();}
	string GetIngamePlayersStatistics();

	// Scores
	ScoreType BestScores[SCORES_MAX];

	void SetScore(int score, Critter* cr, int val);
	void SetScore(int score, const char* name);
	const char* GetScores(); // size == MAX_NAME*SCORES_MAX
	void ClearScore(int score);
};

#endif // __SERVER__