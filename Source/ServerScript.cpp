#include "StdAfx.h"
#include "Server.h"
#include "AngelScript/Preprocessor/preprocess.h"

void* dbg_malloc(size_t size)
{
	size+=sizeof(size_t);
	MEMORY_PROCESS(MEMORY_ANGEL_SCRIPT,size);
	size_t* ptr=(size_t*)malloc(size);
	*ptr=size;
	return ++ptr;
}

void dbg_free(void* ptr)
{
	size_t* ptr_=(size_t*)ptr;
	size_t size=*(--ptr_);
	MEMORY_PROCESS(MEMORY_ANGEL_SCRIPT,-(int)size);
	free(ptr_);
}

bool ASDbgMemoryCanWork=false;
map<void*,string> ASDbgMemoryPtr;
char ASDbgMemoryBuf[1024];
void* dbg_malloc2(size_t size)
{
	size+=sizeof(size_t);
	size_t* ptr=(size_t*)malloc(size);
	*ptr=size;

	if(ASDbgMemoryCanWork)
	{
		const char* module=Script::GetActiveModuleName();
		const char* func=Script::GetActiveFuncName();
		sprintf(ASDbgMemoryBuf,"AS : %s : %s",module?module:"<nullptr>",func?func:"<nullptr>");
		MEMORY_PROCESS_STR(ASDbgMemoryBuf,size);
		ASDbgMemoryPtr.insert(map<void*,string>::value_type(ptr,string(ASDbgMemoryBuf)));
	}
	MEMORY_PROCESS(MEMORY_ANGEL_SCRIPT,size);

	return ++ptr;
}

void dbg_free2(void* ptr)
{
	size_t* ptr_=(size_t*)ptr;
	size_t size=*(--ptr_);

	if(ASDbgMemoryCanWork)
	{
		map<void*,string>::iterator it=ASDbgMemoryPtr.find(ptr_);
		if(it!=ASDbgMemoryPtr.end())
		{
			MEMORY_PROCESS_STR((*it).second.c_str(),-(int)size);
			ASDbgMemoryPtr.erase(it);
		}
	}
	MEMORY_PROCESS(MEMORY_ANGEL_SCRIPT,-(int)size);

	free(ptr_);
}

StrSet ParametersAlready;
DWORD ParametersIndex=1; // 0 is ParamBase
StrSet ParametersClAlready;
DWORD ParametersClIndex=1; // 0 is ParamBase

bool FOServer::InitScriptSystem()
{
	WriteLog("Script system initialization...\n");

	// Memory debugging
#ifdef MEMORY_DEBUG
	if(MemoryDebugLevel>=2) asSetGlobalMemoryFunctions(dbg_malloc2,dbg_free2);
	else if(MemoryDebugLevel>=1) asSetGlobalMemoryFunctions(dbg_malloc,dbg_free);
	else asSetGlobalMemoryFunctions(malloc,free);
#endif

	// Init
	if(!Script::Init(false,PragmaCallbackCrData))
	{
		WriteLog("Script System Init fail.\n");
		return false;
	}
	Script::SetScriptsPath(PT_SERVER_SCRIPTS);

	// Bind vars and functions, look bind.h
	asIScriptEngine* engine=Script::GetEngine();
#define BIND_SERVER
#define BIND_CLASS FOServer::SScriptFunc::
#define BIND_ERROR do{WriteLog(__FUNCTION__" - Bind error, line<%d>.\n",__LINE__); return false;}while(0)
#include "ScriptBind.h"

	// Get config file
	FileManager scripts_cfg;
	scripts_cfg.LoadFile(SCRIPTS_LST,PT_SERVER_SCRIPTS);
	if(!scripts_cfg.IsLoaded())
	{
		WriteLog("Config file<%s> not found.\n",SCRIPTS_LST);
		return false;
	}

	// Load script modules
	Script::Undefine(NULL);
	Script::Define("__SERVER");
	if(!Script::ReloadScripts((char*)scripts_cfg.GetBuf(),"server",false))
	{
		Script::Finish();
		WriteLog("Reload scripts fail.\n");
		return false;
	}

	// Bind game functions
	ReservedScriptFunction BindGameFunc[]={
		{&ServerFunctions.Start,"start","bool %s()"},
		{&ServerFunctions.GetStartTime,"get_start_time","void %s(uint16&,uint16&,uint16&,uint16&,uint16&,uint16&)"},
		{&ServerFunctions.Finish,"finish","void %s()"},
		{&ServerFunctions.Loop,"loop","uint %s()"},
		{&ServerFunctions.GlobalProcess,"global_process","void %s(int,Critter&,Critter@[]&,Item@,uint&,uint&,uint&,uint&,uint&,uint&,bool&)"},
		{&ServerFunctions.GlobalInvite,"global_invite","void %s(Critter@[]&,Item@,uint,int,uint&,uint16&,uint16&,uint8&)"},
		{&ServerFunctions.CritterAttack,"critter_attack","void %s(Critter&,Critter&,ProtoItem&,ProtoItem@,uint8)"},
		{&ServerFunctions.CritterAttacked,"critter_attacked","void %s(Critter&,Critter&)"},
		{&ServerFunctions.CritterStealing,"critter_stealing","bool %s(Critter&,Critter&,Item&,uint)"},
		{&ServerFunctions.CritterUseItem,"critter_use_item","bool %s(Critter&,Item&,Critter@,Item@,Scenery@,uint)"},
		{&ServerFunctions.CritterUseSkill,"critter_use_skill","bool %s(Critter&,int,Critter@,Item@,Scenery@)"},
		{&ServerFunctions.CritterReloadWeapon,"critter_reload_weapon","void %s(Critter&,Item&,Item@)"},
		{&ServerFunctions.CritterInit,"critter_init","void %s(Critter&,bool)"},
		{&ServerFunctions.CritterFinish,"critter_finish","void %s(Critter&,bool)"},
		{&ServerFunctions.CritterIdle,"critter_idle","void %s(Critter&)"},
		{&ServerFunctions.CritterDead,"critter_dead","void %s(Critter&,Critter@)"},
		{&ServerFunctions.CritterRespawn,"critter_respawn","void %s(Critter&)"},
		{&ServerFunctions.CritterChangeItem,"critter_change_item","void %s(Critter&,Item&,uint8)"},
		{&ServerFunctions.MapCritterIn,"map_critter_in","void %s(Map&,Critter&)"},
		{&ServerFunctions.MapCritterOut,"map_critter_out","void %s(Map&,Critter&)"},
		{&ServerFunctions.NpcPlaneBegin,"npc_plane_begin","bool %s(Critter&,NpcPlane&,int,Critter@,Item@)"},
		{&ServerFunctions.NpcPlaneEnd,"npc_plane_end","bool %s(Critter&,NpcPlane&,int,Critter@,Item@)"},
		{&ServerFunctions.NpcPlaneRun,"npc_plane_run","bool %s(Critter&,NpcPlane&,int,uint&,uint&,uint&)"},
		{&ServerFunctions.KarmaVoting,"karma_voting","void %s(Critter&,Critter&,bool)"},
		{&ServerFunctions.CheckLook,"check_look","bool %s(Map&,Critter&,Critter&)"},
		{&ServerFunctions.ItemCost,"item_cost","uint %s(Item&,Critter&,Critter&,bool)"},
		{&ServerFunctions.ItemsBarter,"items_barter","bool %s(Item@[]&,uint[]&,Item@[]&,uint[]&,Critter&,Critter&)"},
		{&ServerFunctions.ItemsCrafted,"items_crafted","void %s(Item@[]&,uint[]&,Item@[]&,Critter&)"},
		{&ServerFunctions.PlayerLevelUp,"player_levelup","void %s(Critter&,uint,uint,uint)"},
		{&ServerFunctions.TurnBasedBegin,"turn_based_begin","void %s(Map&)"},
		{&ServerFunctions.TurnBasedEnd,"turn_based_end","void %s(Map&)"},
		{&ServerFunctions.TurnBasedProcess,"turn_based_process","void %s(Map&,Critter&,bool)"},
		{&ServerFunctions.WorldSave,"world_save","void %s(uint,uint[]&)"},
		{&ServerFunctions.PlayerRegistration,"player_registration","bool %s(uint,string&,string&,uint&,uint&)"},
		{&ServerFunctions.PlayerLogin,"player_login","bool %s(uint,string&,string&,uint,uint&,uint&)"},
		{&ServerFunctions.PlayerGetAccess,"player_getaccess","bool %s(Critter&,int,string&)"},
	};
	if(!Script::BindReservedFunctions((char*)scripts_cfg.GetBuf(),"server",BindGameFunc,sizeof(BindGameFunc)/sizeof(BindGameFunc[0])))
	{
		Script::Finish();
		WriteLog("Bind game functions fail.\n");
		return false;
	}

	ASDbgMemoryCanWork=true;
	WriteLog("Script system initialization complete.\n");
	return true;
}

void FOServer::FinishScriptSystem()
{
	WriteLog("Script system finish...\n");
	Script::Finish();
	WriteLog("Script system finish complete.\n");
}

void FOServer::ScriptSystemUpdate()
{
	Script::SetRunTimeout(GameOpt.ScriptRunSuspendTimeout,GameOpt.ScriptRunMessageTimeout);
}

bool FOServer::DialogScriptDemand(DemandResult& demand, Critter* master, Critter* slave)
{
	if(!Script::PrepareContext(demand.ParamId,CALL_FUNC_STR,master->GetInfo())) return 0;
	Script::SetArgObject(master);
	Script::SetArgObject(slave);
	for(int i=0;i<demand.ValuesCount;i++) Script::SetArgDword(demand.ValueExt[i]);
	if(Script::RunPrepared()) return Script::GetReturnedBool();
	return false;
}

void FOServer::DialogScriptResult(DemandResult& result, Critter* master, Critter* slave)
{
	if(!Script::PrepareContext(result.ParamId,CALL_FUNC_STR,master->GetInfo())) return;
	Script::SetArgObject(master);
	Script::SetArgObject(slave);
	for(int i=0;i<result.ValuesCount;i++) Script::SetArgDword(result.ValueExt[i]);
	Script::RunPrepared();
}

/************************************************************************/
/* Client script processing                                             */
/************************************************************************/

#undef BIND_SERVER
#undef BIND_CLASS
#undef BIND_ERROR
#define BIND_CLIENT
#define BIND_CLASS BindClass::
#define BIND_ERROR do{WriteLog(__FUNCTION__" - Bind error, line<%d>.\n",__LINE__); bind_errors++;}while(0)

namespace ClientBind
{
#include "DummyData.h"

	static int Bind(asIScriptEngine* engine)
	{
		int bind_errors=0;
#include "ScriptBind.h"
		return bind_errors;
	}
}

bool FOServer::ReloadClientScripts()
{
	WriteLog("Reload client scripts...\n");

	// Get config file
	FileManager scripts_cfg;
	scripts_cfg.LoadFile(SCRIPTS_LST,PT_SERVER_SCRIPTS);
	if(!scripts_cfg.IsLoaded())
	{
		WriteLog("Config file<%s> not found.\n",SCRIPTS_LST);
		return false;
	}

	// Disable debug allocators
#ifdef MEMORY_DEBUG
	asSetGlobalMemoryFunctions(malloc,free);
#endif

	asIScriptEngine* old_engine=Script::GetEngine();
	asIScriptEngine* engine=Script::CreateEngine(PragmaCallbackCrClData);
	if(engine) Script::SetEngine(engine);

	// Bind vars and functions
	int bind_errors=0;
	if(engine) bind_errors=ClientBind::Bind(engine);

	// Check errors
	if(!engine || bind_errors)
	{
		if(!engine) WriteLog(__FUNCTION__" - asCreateScriptEngine fail.\n");
		else WriteLog("Bind fail, errors<%d>.\n",bind_errors);
		Script::FinishEngine(engine);

#ifdef MEMORY_DEBUG
		if(MemoryDebugLevel>=2) asSetGlobalMemoryFunctions(dbg_malloc2,dbg_free2);
		else if(MemoryDebugLevel>=1) asSetGlobalMemoryFunctions(dbg_malloc,dbg_free);
		else asSetGlobalMemoryFunctions(malloc,free);
#endif
		return false;
	}

	// Load script modules
	Script::Undefine("__SERVER");
	Script::Define("__CLIENT");
	ParametersClAlready.clear();
	ParametersClIndex=1;

	int num=STR_INTERNAL_SCRIPT_MODULES;
	int errors=0;
	char buf[MAX_FOTEXT];
	string value,config;
	StrVec pragmas;
	while(scripts_cfg.GetLine(buf,MAX_FOTEXT))
	{
		if(buf[0]!='@') continue;
		istrstream str(&buf[1]);
		str >> value;
		if(str.fail() || value!="client") continue;
		str >> value;
		if(str.fail() || (value!="module" && value!="bind")) continue;

		if(value=="module")
		{
			str >> value;
			if(str.fail()) continue;

			if(!Script::LoadScript(value.c_str(),NULL,false,"CLIENT_"))
			{
				WriteLog(__FUNCTION__" - Unable to load client script<%s>.\n",value.c_str());
				errors++;
				continue;
			}

			asIScriptModule* module=engine->GetModule(value.c_str(),asGM_ONLY_IF_EXISTS);
			CBytecodeStream binary;
			if(!module || module->SaveByteCode(&binary)<0)
			{
				WriteLog(__FUNCTION__" - Unable to save bytecode of client script<%s>.\n",value.c_str());
				errors++;
				continue;
			}
			std::vector<asBYTE>& buf=binary.GetBuf();

			StrVec& pr=Preprocessor::GetParsedPragmas();
			pragmas.insert(pragmas.end(),pr.begin(),pr.end());

			// Add module name and bytecode
			for(LangPackVecIt it=LangPacks.begin(),end=LangPacks.end();it!=end;++it)
			{
				LanguagePack& lang=*it;
				FOMsg& msg_script=lang.Msg[TEXTMSG_INTERNAL];

				for(int i=0;i<10;i++) msg_script.EraseStr(num+i);
				msg_script.AddStr(num,value.c_str());
				msg_script.AddBinary(num+1,(BYTE*)&buf[0],buf.size());
			}
			num+=2;
		}
		else
		{
			// Make bind line
			string config_="@ client bind ";
			str >> value;
			if(str.fail()) continue;
			config_+=value+" ";
			str >> value;
			if(str.fail()) continue;
			config_+=value;
			config+=config_+"\n";
		}
	}

	Script::FinishEngine(engine);
	Script::Undefine("__CLIENT");
	Script::Define("__SERVER");

#ifdef MEMORY_DEBUG
	if(MemoryDebugLevel>=2) asSetGlobalMemoryFunctions(dbg_malloc2,dbg_free2);
	else if(MemoryDebugLevel>=1) asSetGlobalMemoryFunctions(dbg_malloc,dbg_free);
	else asSetGlobalMemoryFunctions(malloc,free);
#endif
	Script::SetEngine(old_engine);

	// Add config text and pragmas, calculate hash
	for(LangPackVecIt it=LangPacks.begin(),end=LangPacks.end();it!=end;++it)
	{
		LanguagePack& lang=*it;
		FOMsg& msg_script=lang.Msg[TEXTMSG_INTERNAL];

		msg_script.EraseStr(STR_INTERNAL_SCRIPT_CONFIG);
		msg_script.AddStr(STR_INTERNAL_SCRIPT_CONFIG,config.c_str());

		for(size_t i=0,j=pragmas.size();i<j;i++)
		{
			msg_script.EraseStr(STR_INTERNAL_SCRIPT_PRAGMAS+i);
			msg_script.AddStr(STR_INTERNAL_SCRIPT_PRAGMAS+i,pragmas[i].c_str());
		}
		for(size_t i=0,j=10;i<j;i++) msg_script.EraseStr(STR_INTERNAL_SCRIPT_PRAGMAS+pragmas.size()+i);

		msg_script.CalculateHash();
	}

	// Send to all connected clients
	EnterCriticalSection(&CSConnectedClients);
	for(ClVecIt it=ConnectedClients.begin(),end=ConnectedClients.end();it!=end;++it)
	{
		Client* cl=*it;
		LangPackVecIt it_l=std::find(LangPacks.begin(),LangPacks.end(),cl->LanguageMsg);
		if(it_l!=LangPacks.end()) Send_MsgData(cl,cl->LanguageMsg,TEXTMSG_INTERNAL,(*it_l).Msg[TEXTMSG_INTERNAL]);
		cl->Send_LoadMap(NULL);
	}
	LeaveCriticalSection(&CSConnectedClients);

	WriteLog("Reload client scripts complete.\n");
	return true;
}

/************************************************************************/
/* Pragma callbacks                                                     */
/************************************************************************/

bool FOServer::PragmaCallbackCrData(const char* text)
{
	string name;
	DWORD min,max;
	istrstream str(text);
	str >> name >> min >> max;
	if(str.fail()) return false;
	if(min>max || max>=MAX_PARAMS) return false;
	if(ParametersAlready.count(name)) return true;
	if(ParametersIndex>=MAX_PARAMETERS_ARRAYS) return false;

	asIScriptEngine* engine=Script::GetEngine();
	char decl[128];
	sprintf_s(decl,"DataVal %s",name.c_str());
	if(engine->RegisterObjectProperty("Critter",decl,offsetof(Critter,ThisPtr[ParametersIndex]))<0) return false;
	sprintf_s(decl,"DataRef %sBase",name.c_str());
	if(engine->RegisterObjectProperty("Critter",decl,offsetof(Critter,ThisPtr[ParametersIndex]))<0) return false;
	Critter::ParametersMin[ParametersIndex]=min;
	Critter::ParametersMax[ParametersIndex]=max;
	Critter::ParametersOffset[ParametersIndex]=(strstr(text,"+")!=NULL);
	ParametersIndex++;
	ParametersAlready.insert(name);
	return true;
}

bool FOServer::PragmaCallbackCrClData(const char* text)
{
	string name;
	DWORD min,max;
	istrstream str(text);
	str >> name >> min >> max;
	if(str.fail()) return false;
	if(min>max || max>=MAX_PARAMS) return false;
	if(ParametersClAlready.count(name)) return true;
	if(ParametersClIndex>=MAX_PARAMETERS_ARRAYS) return false;

	asIScriptEngine* engine=Script::GetEngine();
	char decl[128];
	sprintf_s(decl,"DataVal %s",name.c_str());
	if(engine->RegisterObjectProperty("CritterCl",decl,10000+ParametersClIndex*4)<0) return false;
	sprintf_s(decl,"DataRef %sBase",name.c_str());
	if(engine->RegisterObjectProperty("CritterCl",decl,10000+ParametersClIndex*4)<0) return false;
	ParametersClIndex++;
	ParametersClAlready.insert(name);
	return true;
}

/************************************************************************/
/* Wrapper functions                                                    */
/************************************************************************/

string FOServer::SScriptFunc::ScriptLastError="No errors.";
CScriptString* FOServer::SScriptFunc::Global_GetLastError()
{
	return new CScriptString(ScriptLastError);
}

int SortCritterHx=0,SortCritterHy=0;
bool SortCritterByDistPred(Critter* cr1, Critter* cr2)
{
	return DistGame(SortCritterHx,SortCritterHy,cr1->GetHexX(),cr1->GetHexY())<DistGame(SortCritterHx,SortCritterHy,cr2->GetHexX(),cr2->GetHexY());
}
void SortCritterByDist(Critter* cr, CrVec& critters)
{
	SortCritterHx=cr->GetHexX();
	SortCritterHy=cr->GetHexY();
	std::sort(critters.begin(),critters.end(),SortCritterByDistPred);
}
void SortCritterByDist(int hx, int hy, CrVec& critters)
{
	SortCritterHx=hx;
	SortCritterHy=hy;
	std::sort(critters.begin(),critters.end(),SortCritterByDistPred);
}

int* FOServer::SScriptFunc::DataRef_Index(CritterPtr& cr, DWORD index)
{
	static int dummy=0;
	if(cr->IsNotValid) SCRIPT_ERROR_RX("This nulltptr.",&dummy);
	if(index>=MAX_PARAMS) SCRIPT_ERROR_RX("Invalid index arg.",&dummy);
	DWORD data_index=((DWORD)&cr-(DWORD)&cr->ThisPtr[0])/sizeof(cr->ThisPtr[0]);
	if(Critter::ParametersOffset[data_index]) index+=Critter::ParametersMin[data_index];
	if(index<Critter::ParametersMin[data_index]) SCRIPT_ERROR_RX("Index is less than minimum.",&dummy);
	if(index>Critter::ParametersMax[data_index]) SCRIPT_ERROR_RX("Index is greather than maximum.",&dummy);
	cr->ChangeParam(index);
	return &cr->Data.Params[index];
}

int FOServer::SScriptFunc::DataVal_Index(CritterPtr& cr, DWORD index)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nulltptr.");
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R0("Invalid index arg.");
	DWORD data_index=((DWORD)&cr-(DWORD)&cr->ThisPtr[0])/sizeof(cr->ThisPtr[0]);
	if(Critter::ParametersOffset[data_index]) index+=Critter::ParametersMin[data_index];
	if(index<Critter::ParametersMin[data_index]) SCRIPT_ERROR_R0("Index is less than minimum.");
	if(index>Critter::ParametersMax[data_index]) SCRIPT_ERROR_R0("Index is greather than maximum.");
	return cr->GetParam(index);
}

AIDataPlane* FOServer::SScriptFunc::NpcPlane_GetCopy(AIDataPlane* plane)
{
	return plane->GetCopy();
}

AIDataPlane* FOServer::SScriptFunc::NpcPlane_SetChild(AIDataPlane* plane, AIDataPlane* child_plane)
{
	if(child_plane->Assigned) child_plane=child_plane->GetCopy();
	else child_plane->AddRef();
	plane->ChildPlane=child_plane;
	return child_plane;
}

AIDataPlane* FOServer::SScriptFunc::NpcPlane_GetChild(AIDataPlane* plane, DWORD index)
{
	AIDataPlane* result=plane->ChildPlane;
	for(DWORD i=0;i<index && result;i++) result=result->ChildPlane;
	return result;
}

bool FOServer::SScriptFunc::NpcPlane_Misc_SetScript(AIDataPlane* plane, CScriptString& func_name)
{
	int bind_id=Script::Bind(func_name.c_str(),"void %s(Critter&)",false);
	if(bind_id<=0) SCRIPT_ERROR_R0("Script not found.");
	plane->Misc.ScriptBindId=bind_id;
	return true;
}

Item* FOServer::SScriptFunc::Container_AddItem(Item* cont, WORD pid, DWORD count, DWORD special_id)
{
	if(cont->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cont->IsContainer()) SCRIPT_ERROR_R0("Container item is not container type.");
	if(!ItemMngr.GetProtoItem(pid)) SCRIPT_ERROR_R0("Invalid proto id arg.");
	return ItemMngr.AddItemContainer(cont,pid,count,special_id);
}

DWORD FOServer::SScriptFunc::Container_GetItems(Item* cont, DWORD special_id, asIScriptArray* items)
{
	if(!items) SCRIPT_ERROR_R0("Items array arg nullptr.");
	if(cont->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cont->IsContainer()) SCRIPT_ERROR_R0("Container item is not container type.");
	ItemPtrVec items_;
	cont->ContGetItems(items_,special_id);
	if(items) Script::AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

Item* FOServer::SScriptFunc::Container_GetItem(Item* cont, WORD pid, DWORD special_id)
{
	if(cont->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cont->IsContainer()) SCRIPT_ERROR_R0("Container item is not container type.");
	return cont->ContGetItemByPid(pid,special_id);
}

bool FOServer::SScriptFunc::Item_IsGrouped(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->IsGrouped();
}

bool FOServer::SScriptFunc::Item_IsWeared(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->IsWeared();
}

bool FOServer::SScriptFunc::Item_SetScript(Item* item, CScriptString* script)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!script || !script->length())
	{
		item->Data.ScriptId=0;
	}
	else
	{
		if(!item->ParseScript(script->c_str(),true)) SCRIPT_ERROR_R0("Script function not found.");
	}
	return true;
}

DWORD FOServer::SScriptFunc::Item_GetScriptId(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->Data.ScriptId;
}

bool FOServer::SScriptFunc::Item_SetEvent(Item* item, int event_type, CScriptString* func_name)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(event_type<0 || event_type>=ITEM_EVENT_MAX) SCRIPT_ERROR_R0("Invalid event type arg.");
	if(!func_name || !func_name->length()) item->FuncId[event_type]=0;
	else
	{
		item->FuncId[event_type]=Script::Bind(func_name->c_str(),ItemEventFuncName[event_type],false);
		if(item->FuncId[event_type]<=0) SCRIPT_ERROR_R0("Function not found.");

		if(event_type==ITEM_EVENT_WALK && item->Accessory==ITEM_ACCESSORY_HEX)
		{
			Map* map=MapMngr.GetMap(item->ACC_HEX.MapId);
			if(map) map->SetHexFlag(item->ACC_HEX.HexX,item->ACC_HEX.HexY,FH_WALK_ITEM);
		}
	}
	return true;
}

BYTE FOServer::SScriptFunc::Item_GetType(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->GetType();
}

WORD FOServer::SScriptFunc::Item_GetProtoId(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->GetProtoId();
}

DWORD FOServer::SScriptFunc::Item_GetCount(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->GetCount();
}

void FOServer::SScriptFunc::Item_SetCount(Item* item, DWORD count)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!item->IsGrouped()) SCRIPT_ERROR_R("Item is not grouped.");
	if(count==0) SCRIPT_ERROR_R("Count arg is zero.");
	if(item->GetCount()==count) return;
	item->Count_Set(count);
	ItemMngr.NotifyChangeItem(item);
}

DWORD FOServer::SScriptFunc::Item_GetCost(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->GetCost();
}

Map* FOServer::SScriptFunc::Item_GetMapPosition(Item* item, WORD& hx, WORD& hy)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	Map* map=NULL;
	switch(item->Accessory)
	{
	case ITEM_ACCESSORY_CRITTER:
		{
			Critter* cr=CrMngr.GetCritter(item->ACC_CRITTER.Id);
			if(!cr) SCRIPT_ERROR_R0("Critter accessory, Critter not found.");
			if(!cr->GetMap()) return NULL;
			map=MapMngr.GetMap(cr->GetMap());
			if(!map) SCRIPT_ERROR_R0("Critter accessory, Map not found.");
			hx=cr->GetHexX();
			hy=cr->GetHexY();
		}
		break;
	case ITEM_ACCESSORY_HEX:
		{
			map=MapMngr.GetMap(item->ACC_HEX.MapId);
			if(!map) SCRIPT_ERROR_R0("Hex accessory, Map not found.");
			hx=item->ACC_HEX.HexX;
			hy=item->ACC_HEX.HexY;
		}
		break;
	case ITEM_ACCESSORY_CONTAINER:
		{
			if(item->GetId()==item->ACC_CONTAINER.ContainerId) SCRIPT_ERROR_R0("Container accessory, Crosslinks.");
			Item* cont=ItemMngr.GetItem(item->ACC_CONTAINER.ContainerId);
			if(!cont) SCRIPT_ERROR_R0("Container accessory, Container not found.");
			return Item_GetMapPosition(cont,hx,hy); // Recursion
		}
		break;
	default:
		SCRIPT_ERROR_R0("Unknown accessory.");
		break;
	}
	return map;
}

bool FOServer::SScriptFunc::Item_ChangeProto(Item* item, WORD pid)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
	if(!pid) SCRIPT_ERROR_R0("Proto item not found.");
	if(item->GetType()!=proto_item->GetType()) SCRIPT_ERROR_R0("Different types.");
	ProtoItem* old_proto_item=item->Proto;
	item->Proto=proto_item;
	// Send
	if(item->Accessory==ITEM_ACCESSORY_CRITTER)
	{
		Critter* cr=CrMngr.GetCritter(item->ACC_CRITTER.Id);
		if(!cr) return true;
		item->Proto=old_proto_item;
		cr->Send_EraseItem(item);
		item->Proto=proto_item;
		cr->Send_AddItem(item);
		cr->SendAA_MoveItem(item,ACTION_REFRESH,0);
	}
	else if(item->Accessory==ITEM_ACCESSORY_HEX)
	{
		Map* map=MapMngr.GetMap(item->ACC_HEX.MapId);
		if(!map) return true;
		WORD hx=item->ACC_HEX.HexX;
		WORD hy=item->ACC_HEX.HexY;
		item->Proto=old_proto_item;
		map->EraseItem(item->GetId(),true);
		item->Proto=proto_item;
		map->AddItem(item,hx,hy,true);
	}
	return true;
}

void FOServer::SScriptFunc::Item_Update(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	ItemMngr.NotifyChangeItem(item);
}

void FOServer::SScriptFunc::Item_Animate(Item* item, BYTE from_frm, BYTE to_frm)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	switch(item->Accessory)
	{
	case ITEM_ACCESSORY_CRITTER:
		{
		//	Critter* cr=CrMngr.GetCrit(item->ACC_CRITTER.Id);
		//	if(cr) cr->Send_AnimateItem(item,from_frm,to_frm);
		//	else SCRIPT_ERROR("Critter not found, maybe client in offline.");
		}
		break;
	case ITEM_ACCESSORY_HEX:
		{
			Map* map=MapMngr.GetMap(item->ACC_HEX.MapId);
			if(!map)
			{
				SCRIPT_ERROR("Map not found.");
				break;
			}
			map->AnimateItem(item,from_frm,to_frm);
		}
		break;
	case ITEM_ACCESSORY_CONTAINER:
		break;
	default:
		WriteLog(__FUNCTION__" - Unknown accessory<%u>!",item->Accessory);
		SCRIPT_ERROR("Unknown accessory.");
		return;
	}
}

void FOServer::SScriptFunc::Item_SetLexems(Item* item, CScriptString* lexems)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!lexems) item->SetLexems(NULL);
	else
	{
		const char* str=lexems->c_str();
		DWORD len=strlen(str);
		if(!len) SCRIPT_ERROR_R("Lexems arg length is zero.");
		if(len+1>=LEXEMS_SIZE) SCRIPT_ERROR_R("Lexems arg length is greater than maximum.");
		item->SetLexems(str);
	}

	// Update
	switch(item->Accessory)
	{
	case ITEM_ACCESSORY_CRITTER:
		{
			Client* cl=CrMngr.GetPlayer(item->ACC_CRITTER.Id);
			if(cl && cl->IsOnline())
			{
				if(item->PLexems) cl->Send_ItemLexems(item);
				else cl->Send_ItemLexemsNull(item);
			}
		}
		break;
	case ITEM_ACCESSORY_HEX:
		{
			Map* map=MapMngr.GetMap(item->ACC_HEX.MapId);
			if(!map)
			{
				SCRIPT_ERROR("Map not found.");
				break;
			}

			ClVec& clients=map->GetPlayers();
			for(ClVecIt it=clients.begin(),end=clients.end();it!=end;++it)
			{
				Client* cl=*it;
				if(cl->IsOnline() && cl->CountIdVisItem(item->GetId()))
				{
					if(item->PLexems) cl->Send_ItemLexems(item);
					else cl->Send_ItemLexemsNull(item);
				}
			}
		}
		break;
	case ITEM_ACCESSORY_CONTAINER:
		break;
	default:
		WriteLog(__FUNCTION__" - Unknown accessory<%u>!",item->Accessory);
		SCRIPT_ERROR("Unknown accessory.");
		return;
	}
}

bool FOServer::SScriptFunc::Item_LockerOpen(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("Door arg nullptr.");
	if(!item->IsHasLocker()) SCRIPT_ERROR_R0("Door item is no have locker.");
	if(!item->LockerIsChangeble()) SCRIPT_ERROR_R0("Door is not changeble.");
	if(item->LockerIsOpen()) return true;
	SETFLAG(item->Data.Locker.Condition,LOCKER_ISOPEN);
	if(item->IsDoor())
	{
		bool recache_block=false;
		bool recache_shoot=false;
		if(!item->Proto->Door.NoBlockMove)
		{
			SETFLAG(item->Data.Flags,ITEM_NO_BLOCK);
			recache_block=true;
		}
		if(!item->Proto->Door.NoBlockShoot)
		{
			SETFLAG(item->Data.Flags,ITEM_SHOOT_THRU);
			recache_shoot=true;
		}
		if(!item->Proto->Door.NoBlockLight) SETFLAG(item->Data.Flags,ITEM_LIGHT_THRU);

		if(item->Accessory==ITEM_ACCESSORY_HEX && (recache_block || recache_shoot))
		{
			Map* map=MapMngr.GetMap(item->ACC_HEX.MapId);
			if(map)
			{
				if(recache_block && recache_shoot) map->RecacheHexBlockShoot(item->ACC_HEX.HexX,item->ACC_HEX.HexY);
				else if(recache_block) map->RecacheHexBlock(item->ACC_HEX.HexX,item->ACC_HEX.HexY);
				else if(recache_shoot) map->RecacheHexShoot(item->ACC_HEX.HexX,item->ACC_HEX.HexY);
			}
		}
	}
	ItemMngr.NotifyChangeItem(item);
	return true;
}

bool FOServer::SScriptFunc::Item_LockerClose(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("Door arg nullptr.");
	if(!item->IsHasLocker()) SCRIPT_ERROR_R0("Door item is no have locker.");
	if(!item->LockerIsChangeble()) SCRIPT_ERROR_R0("Door is not changeble.");
	if(item->LockerIsClose()) return true;
	UNSETFLAG(item->Data.Locker.Condition,LOCKER_ISOPEN);
	if(item->IsDoor())
	{
		bool recache_block=false;
		bool recache_shoot=false;
		if(!item->Proto->Door.NoBlockMove)
		{
			UNSETFLAG(item->Data.Flags,ITEM_NO_BLOCK);
			recache_block=true;
		}
		if(!item->Proto->Door.NoBlockShoot)
		{
			UNSETFLAG(item->Data.Flags,ITEM_SHOOT_THRU);
			recache_shoot=true;
		}
		if(!item->Proto->Door.NoBlockLight) UNSETFLAG(item->Data.Flags,ITEM_LIGHT_THRU);

		if(item->Accessory==ITEM_ACCESSORY_HEX && (recache_block || recache_shoot))
		{
			Map* map=MapMngr.GetMap(item->ACC_HEX.MapId);
			if(map)
			{
				if(recache_block) map->SetHexFlag(item->ACC_HEX.HexX,item->ACC_HEX.HexY,FH_BLOCK_ITEM);
				if(recache_shoot) map->SetHexFlag(item->ACC_HEX.HexX,item->ACC_HEX.HexY,FH_NRAKE_ITEM);
			}
		}
	}
	ItemMngr.NotifyChangeItem(item);
	return true;
}

bool FOServer::SScriptFunc::Item_IsCar(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->IsCar();
}

Item* FOServer::SScriptFunc::Item_CarGetBag(Item* item, int num_bag)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return item->CarGetBag(num_bag);
}

void FOServer::SScriptFunc::Item_EventFinish(Item* item, bool deleted)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	item->EventFinish(deleted);
}

bool FOServer::SScriptFunc::Item_EventAttack(Item* item, Critter* attacker, Critter* target)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(attacker->IsNotValid) SCRIPT_ERROR_R0("Attacker critter arg nullptr.");
	if(target->IsNotValid) SCRIPT_ERROR_R0("Target critter arg nullptr.");
	return item->EventAttack(attacker,target);
}

bool FOServer::SScriptFunc::Item_EventUse(Item* item, Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R0("Critter arg nullptr.");
	if(on_critter && on_critter->IsNotValid) SCRIPT_ERROR_R0("On critter arg nullptr.");
	if(on_item && on_item->IsNotValid) SCRIPT_ERROR_R0("On item arg nullptr.");
	return item->EventUse(cr,on_critter,on_item,on_scenery);
}

bool FOServer::SScriptFunc::Item_EventUseOnMe(Item* item, Critter* cr, Item* used_item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R0("Critter arg nullptr.");
	if(used_item && used_item->IsNotValid) SCRIPT_ERROR_R0("Used item arg nullptr.");
	return item->EventUseOnMe(cr,used_item);
}

bool FOServer::SScriptFunc::Item_EventSkill(Item* item, Critter* cr, int skill)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R0("Critter arg nullptr.");
	return item->EventSkill(cr,skill);
}

void FOServer::SScriptFunc::Item_EventDrop(Item* item, Critter* cr)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	item->EventDrop(cr);
}

void FOServer::SScriptFunc::Item_EventMove(Item* item, Critter* cr, BYTE from_slot)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	item->EventMove(cr,from_slot);
}

void FOServer::SScriptFunc::Item_EventWalk(Item* item, Critter* cr, bool entered, BYTE dir)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	item->EventWalk(cr,entered,dir);
}

void FOServer::SScriptFunc::Item_set_Flags(Item* item, DWORD value)
{
	if(item->IsNotValid) return;

	DWORD old=item->Data.Flags;
	item->Data.Flags=value;
	Map* map=NULL;

	// Recalculate view for this item
	if((old&(ITEM_HIDDEN|ITEM_ALWAYS_VIEW|ITEM_TRAP))!=(value&(ITEM_HIDDEN|ITEM_ALWAYS_VIEW|ITEM_TRAP)))
	{
		if(item->Accessory==ITEM_ACCESSORY_HEX)
		{
			if(!map) map=MapMngr.GetMap(item->ACC_HEX.MapId);
			if(map) map->ChangeViewItem(item);
		}
		else if(item->Accessory==ITEM_ACCESSORY_CRITTER && (old&ITEM_HIDDEN)!=(value&ITEM_HIDDEN))
		{
			Critter* cr=CrMngr.GetCritter(item->ACC_CRITTER.Id);
			if(cr)
			{
				if(FLAG(value,ITEM_HIDDEN)) cr->Send_EraseItem(item);
				else cr->Send_AddItem(item);
				cr->SendAA_MoveItem(item,ACTION_REFRESH,0);
			}
		}
	}

	// Recache move and shoot blockers
	if((old&(ITEM_NO_BLOCK|ITEM_SHOOT_THRU|ITEM_GAG))!=(value&(ITEM_NO_BLOCK|ITEM_SHOOT_THRU|ITEM_GAG)) && item->Accessory==ITEM_ACCESSORY_HEX)
	{
		if(!map) map=MapMngr.GetMap(item->ACC_HEX.MapId);
		if(map)
		{
			bool recache_block=false;
			bool recache_shoot=false;

			if(FLAG(value,ITEM_NO_BLOCK))
				recache_block=true;
			else
				map->SetHexFlag(item->ACC_HEX.HexX,item->ACC_HEX.HexY,FH_BLOCK_ITEM);
			if(FLAG(value,ITEM_SHOOT_THRU))
				recache_shoot=true;
			else
				map->SetHexFlag(item->ACC_HEX.HexX,item->ACC_HEX.HexY,FH_NRAKE_ITEM);
			if(!FLAG(value,ITEM_GAG))
				recache_block=true;
			else
				map->SetHexFlag(item->ACC_HEX.HexX,item->ACC_HEX.HexY,FH_GAG_ITEM);

			if(recache_block && recache_shoot) map->RecacheHexBlockShoot(item->ACC_HEX.HexX,item->ACC_HEX.HexY);
			else if(recache_block) map->RecacheHexBlock(item->ACC_HEX.HexX,item->ACC_HEX.HexY);
			else if(recache_shoot) map->RecacheHexShoot(item->ACC_HEX.HexX,item->ACC_HEX.HexY);
		}
	}

	// Recache geck value
	if((old&ITEM_GECK)!=(value&ITEM_GECK) && item->Accessory==ITEM_ACCESSORY_HEX)
	{
		if(!map) map=MapMngr.GetMap(item->ACC_HEX.MapId);
		if(map) map->MapLocation->GeckCount+=(FLAG(value,ITEM_GECK)?1:-1);
	}

	// Update data
	if(old!=value) ItemMngr.NotifyChangeItem(item);
}

DWORD FOServer::SScriptFunc::Item_get_Flags(Item* item)
{
	return item->Data.Flags;
}

void FOServer::SScriptFunc::Item_set_TrapValue(Item* item, short value)
{
	if(item->IsNotValid) return;
	item->Data.TrapValue=value;
	if(item->Accessory==ITEM_ACCESSORY_HEX)
	{
		Map* map=MapMngr.GetMap(item->ACC_HEX.MapId);
		if(map) map->ChangeViewItem(item);
	}
}

short FOServer::SScriptFunc::Item_get_TrapValue(Item* item)
{
	return item->Data.TrapValue;
}

bool FOServer::SScriptFunc::Crit_IsPlayer(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsPlayer();
}

bool FOServer::SScriptFunc::Crit_IsNpc(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsNpc();
}

bool FOServer::SScriptFunc::Crit_IsCanWalk(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return CritType::IsCanWalk(cr->GetCrType());
}

bool FOServer::SScriptFunc::Crit_IsCanRun(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return CritType::IsCanRun(cr->GetCrType());
}

bool FOServer::SScriptFunc::Crit_IsCanRotate(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return CritType::IsCanRotate(cr->GetCrType());
}

bool FOServer::SScriptFunc::Crit_IsCanAim(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return CritType::IsCanAim(cr->GetCrType()) && !cr->IsPerk(TRAIT_FAST_SHOT);
}

bool FOServer::SScriptFunc::Crit_IsAnim1(Critter* cr, DWORD index)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return CritType::IsAnim1(cr->GetCrType(),index);
}

bool FOServer::SScriptFunc::Crit_IsAnim3d(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return CritType::IsAnim3d(cr->GetCrType());
}

int FOServer::SScriptFunc::Cl_GetAccess(Critter* cl)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cl->IsPlayer()) SCRIPT_ERROR_R0("Critter is not player.");
	Client* cl_=(Client*)cl;
	if(FLAG(cl_->Access,ACCESS_IMPLEMENTOR)) return 3;
	if(FLAG(cl_->Access,ACCESS_ADMIN)) return 3;
	if(FLAG(cl_->Access,ACCESS_MODER)) return 2;
	if(FLAG(cl_->Access,ACCESS_TESTER)) return 1;
	return 0;
}

bool FOServer::SScriptFunc::Crit_SetEvent(Critter* cr, int event_type, CScriptString* func_name)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(event_type<0 || event_type>=CRITTER_EVENT_MAX) SCRIPT_ERROR_R0("Invalid event type arg.");
	if(!func_name || !func_name->length()) cr->FuncId[event_type]=0;
	else
	{
		cr->FuncId[event_type]=Script::Bind(func_name->c_str(),CritterEventFuncName[event_type],false);
		if(cr->FuncId[event_type]<=0) SCRIPT_ERROR_R0("Function not found.");
	}
	return true;
}

void FOServer::SScriptFunc::Crit_SetLexems(Critter* cr, CScriptString* lexems)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->SetLexems(lexems?lexems->c_str():NULL);

	if(cr->GetMap())
	{
		cr->Send_CritterLexems(cr);
		for(CrVecIt it=cr->VisCr.begin(),end=cr->VisCr.end();it!=end;++it)
		{
			Critter* cr_=*it;
			cr_->Send_CritterLexems(cr);
		}
	}
	else if(cr->GroupMove)
	{
		for(CrVecIt it=cr->GroupMove->CritMove.begin(),end=cr->GroupMove->CritMove.end();it!=end;++it)
		{
			Critter* cr_=*it;
			cr_->Send_CritterLexems(cr);
		}
	}
}

Map* FOServer::SScriptFunc::Crit_GetMap(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return MapMngr.GetMap(cr->GetMap());
}

DWORD FOServer::SScriptFunc::Crit_GetMapId(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetMap();
}

WORD FOServer::SScriptFunc::Crit_GetMapProtoId(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetProtoMap();
}

void FOServer::SScriptFunc::Crit_SetHomePos(Critter* cr, WORD hx, WORD hy, BYTE dir)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map || hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");
	if(dir>5) SCRIPT_ERROR_R("Invalid dir arg.");
	cr->SetHome(cr->GetMap(),hx,hy,dir);
}

void FOServer::SScriptFunc::Crit_GetHomePos(Critter* cr, DWORD& map_id, WORD& hx, WORD& hy, BYTE& dir)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map_id=cr->Data.HomeMap;
	hx=cr->Data.HomeX;
	hy=cr->Data.HomeY;
	dir=cr->Data.HomeOri;
}

bool FOServer::SScriptFunc::Crit_ChangeCrType(Critter* cr, DWORD new_type)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!new_type) SCRIPT_ERROR_R0("New type arg is zero.");
	if(!CritType::IsEnabled(new_type)) SCRIPT_ERROR_R0("New type arg is not enabled.");

	if(cr->Data.BaseType!=new_type)
	{
		if(cr->Data.Multihex<0 && cr->GetMap() && !cr->IsDead())
		{
			DWORD old_mh=CritType::GetMultihex(cr->Data.BaseType);
			DWORD new_mh=CritType::GetMultihex(new_type);
			if(new_mh!=old_mh)
			{
				Map* map=MapMngr.GetMap(cr->GetMap());
				if(map)
				{
					map->UnsetFlagCritter(cr->GetHexX(),cr->GetHexY(),old_mh,false);
					map->SetFlagCritter(cr->GetHexX(),cr->GetHexY(),new_mh,false);
				}
			}
		}

		cr->Data.BaseType=new_type;
		cr->Send_ParamOther(OTHER_BASE_TYPE,new_type);
		cr->SendA_ParamOther(OTHER_BASE_TYPE,new_type);
	}
	return true;
}

void FOServer::SScriptFunc::Cl_DropTimers(Critter* cl)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!cl->IsPlayer()) return;
	Client* cl_=(Client*)cl;
	cl_->DropTimers(true);
}

bool FOServer::SScriptFunc::Crit_MoveRandom(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cr->GetMap()) SCRIPT_ERROR_R0("Critter is on global.");
	return MoveRandom(cr);
}

bool FOServer::SScriptFunc::Crit_MoveToDir(Critter* cr, BYTE direction)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map) SCRIPT_ERROR_R0("Critter is on global.");
	if(direction>5) SCRIPT_ERROR_R0("Invalid direction arg.");

	WORD hx=cr->GetHexX();
	WORD hy=cr->GetHexY();
	MoveHexByDir(hx,hy,direction,map->GetMaxHexX(),map->GetMaxHexY());
	WORD move_flags=direction|BIN16(00000000,00111000);
 	bool move=Self->Act_Move(cr,hx,hy,move_flags);
	if(!move) SCRIPT_ERROR_R0("Move fail.");
	cr->Send_Move(cr,move_flags);
	return true;
}

bool FOServer::SScriptFunc::Crit_TransitToHex(Critter* cr, WORD hx, WORD hy, BYTE dir)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->LockMapTransfers) SCRIPT_ERROR_R0("Transfers locked.");
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map) SCRIPT_ERROR_R0("Critter is on global.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	if(hx!=cr->GetHexX() || hy!=cr->GetHexY())
	{
		if(dir<6 && cr->Data.Dir!=dir) cr->Data.Dir=dir;
		if(!MapMngr.Transit(cr,map,hx,hy,cr->GetDir(),2,true)) SCRIPT_ERROR_R0("Transit fail.");
	}
	else if(dir<6 && cr->Data.Dir!=dir)
	{
		cr->Data.Dir=dir;
		cr->Send_Dir(cr);
		cr->SendA_Dir();
	}
	return true;
}

bool FOServer::SScriptFunc::Crit_TransitToMapHex(Critter* cr, DWORD map_id, WORD hx, WORD hy, BYTE dir)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->LockMapTransfers) SCRIPT_ERROR_R0("Transfers locked.");
	if(!map_id) SCRIPT_ERROR_R0("Map id arg is zero.");
	Map* map=MapMngr.GetMap(map_id);
	if(!map) SCRIPT_ERROR_R0("Map not found.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");

	if(!cr->GetMap())
	{
		if(dir<6 && cr->Data.Dir!=dir) cr->Data.Dir=dir;
		if(!MapMngr.GM_GroupToMap(cr->GroupMove,map,0,hx,hy,cr->GetDir())) SCRIPT_ERROR_R0("Transit from global to map fail.");
	}
	else
	{
		if(cr->GetMap()!=map_id || cr->GetHexX()!=hx || cr->GetHexY()!=hy)
		{
			if(dir<6 && cr->Data.Dir!=dir) cr->Data.Dir=dir;
			if(!MapMngr.Transit(cr,map,hx,hy,cr->GetDir(),2,true)) SCRIPT_ERROR_R0("Transit from map to map fail.");
		}
		else if(dir<6 && cr->Data.Dir!=dir)
		{
			cr->Data.Dir=dir;
			cr->Send_Dir(cr);
			cr->SendA_Dir();
		}
		else return true;
	}

	Location* loc=map->MapLocation;
	if(loc && DistSqrt(cr->Data.WorldX,cr->Data.WorldY,loc->Data.WX,loc->Data.WY)>loc->GetRadius())
	{
		cr->Data.WorldX=loc->Data.WX;
		cr->Data.WorldY=loc->Data.WY;
	}
	return true;
}

bool FOServer::SScriptFunc::Crit_TransitToMapEntire(Critter* cr, DWORD map_id, BYTE entire)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->LockMapTransfers) SCRIPT_ERROR_R0("Transfers locked.");
	if(!map_id) SCRIPT_ERROR_R0("Map id arg is zero.");
	//if(cr->GetMap()==map_id) SCRIPT_ERROR_R0("Critter already on need map.");
	Map* map=MapMngr.GetMap(map_id);
	if(!map) SCRIPT_ERROR_R0("Map not found.");

	WORD hx,hy;
	BYTE dir;
	if(!map->GetStartCoord(hx,hy,dir,entire)) SCRIPT_ERROR_R0("Entire not found.");
	if(!cr->GetMap())
	{
		if(!MapMngr.GM_GroupToMap(cr->GroupMove,map,0,hx,hy,dir)) SCRIPT_ERROR_R0("Transit from global to map fail.");
	}
	else
	{
		if(!MapMngr.Transit(cr,map,hx,hy,dir,2,true)) SCRIPT_ERROR_R0("Transit from map to map fail.");
	}

	Location* loc=map->MapLocation;
	if(loc && DistSqrt(cr->Data.WorldX,cr->Data.WorldY,loc->Data.WX,loc->Data.WY)>loc->GetRadius())
	{
		cr->Data.WorldX=loc->Data.WX;
		cr->Data.WorldY=loc->Data.WY;
	}

	return true;
}

bool FOServer::SScriptFunc::Crit_TransitToGlobal(Critter* cr, bool request_group)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->LockMapTransfers) SCRIPT_ERROR_R0("Transfers locked.");
	if(!cr->GetMap()) return true; //SCRIPT_ERROR_R0("Critter already on global.");
	if(!MapMngr.TransitToGlobal(cr,0,request_group?FOLLOW_PREP:FOLLOW_FORCE,true)) SCRIPT_ERROR_R0("Transit fail.");
	return true;
}

bool FOServer::SScriptFunc::Crit_TransitToGlobalWithGroup(Critter* cr, asIScriptArray& group)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->LockMapTransfers) SCRIPT_ERROR_R0("Transfers locked.");
	if(!cr->GetMap()) SCRIPT_ERROR_R0("Critter already on global.");
	if(!MapMngr.TransitToGlobal(cr,0,FOLLOW_FORCE,true)) SCRIPT_ERROR_R0("Transit fail.");
	for(int i=0,j=group.GetElementCount();i<j;i++)
	{
		Critter* cr_=*(Critter**)group.GetElementPointer(i);
		if(!cr_ || cr_->IsNotValid || !cr_->GetMap()) continue;
		MapMngr.TransitToGlobal(cr_,cr->GetId(),FOLLOW_FORCE,true);
	}
	return true;
}

bool FOServer::SScriptFunc::Crit_TransitToGlobalGroup(Critter* cr, DWORD critter_id)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->LockMapTransfers) SCRIPT_ERROR_R0("Transfers locked.");
	if(!cr->GetMap()) SCRIPT_ERROR_R0("Critter already on global.");
	Critter* cr_global=CrMngr.GetCritter(critter_id);
	if(!cr_global) SCRIPT_ERROR_R0("Critter on global not found.");
	if(cr_global->GetMap() || !cr_global->GroupMove) SCRIPT_ERROR_R0("Founded critter is not on global.");
	if(!MapMngr.TransitToGlobal(cr,cr_global->GroupMove->Rule->GetId(),FOLLOW_FORCE,true)) SCRIPT_ERROR_R0("Transit fail.");
	return true;
}

bool FOServer::SScriptFunc::Crit_IsLife(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsLife();
}

bool FOServer::SScriptFunc::Crit_IsKnockout(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsKnockout();
}

bool FOServer::SScriptFunc::Crit_IsDead(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsDead();
}

bool FOServer::SScriptFunc::Crit_IsFree(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsFree() && !cr->IsWait();
}

bool FOServer::SScriptFunc::Crit_IsBusy(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->IsBusy() || cr->IsWait();
}

void FOServer::SScriptFunc::Crit_Wait(Critter* cr, DWORD ms)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->SetWait(ms);
	if(cr->IsPlayer())
	{
		Client* cl=(Client*)cr;
		cl->SetBreakTime(ms);
		cl->Send_ParamOther(OTHER_BREAK_TIME,ms);
	}
}

/*DWORD FOServer::SScriptFunc::Crit_GetWait(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->IsPlayer())
	{
		cr->SetBreakTime(ms);
		cr->Send_ParamOther(OTHER_BREAK_TIME,cr->GetBreakTime());
	}
	else if(cr->IsNpc())
	{
		((Npc*)cr)->SetWait(ms);
	}
}*/

void FOServer::SScriptFunc::Crit_ToDead(Critter* cr, BYTE dead_type, Critter* killer)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsDead()) SCRIPT_ERROR_R("Critter already dead.");
	KillCritter(cr,dead_type,killer);
}

bool FOServer::SScriptFunc::Crit_ToLife(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cr->IsDead()) SCRIPT_ERROR_R0("Critter not dead.");
	if(!cr->GetMap()) SCRIPT_ERROR_R0("Critter on global map.");
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map) SCRIPT_ERROR_R0("Map not found.");
	if(!map->IsHexesPassed(cr->GetHexX(),cr->GetHexY(),cr->GetMultihex())) SCRIPT_ERROR_R0("Position busy.");
	RespawnCritter(cr);
	if(!cr->IsLife()) SCRIPT_ERROR_R0("Respawn critter fail.");
	return true;
}

bool FOServer::SScriptFunc::Crit_ToKnockout(Critter* cr, bool face_up, DWORD lose_ap, WORD knock_hx, WORD knock_hy)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cr->IsLife())
	{
		if(cr->IsKnockout())
		{
			cr->KnockoutAp+=lose_ap;
			return true;
		}
		SCRIPT_ERROR_R0("Critter is dead.");
	}

	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map) SCRIPT_ERROR_R0("Critter map not found.");
	if(knock_hx>=map->GetMaxHexX() || knock_hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");

	if(cr->GetHexX()!=knock_hx || cr->GetHexY()!=knock_hy)
	{
		bool passed=false;
		DWORD multihex=cr->GetMultihex();
		if(multihex)
		{
			map->UnsetFlagCritter(cr->GetHexX(),cr->GetHexY(),multihex,false);
			passed=map->IsHexesPassed(knock_hx,knock_hy,multihex);
			map->SetFlagCritter(cr->GetHexX(),cr->GetHexY(),multihex,false);
		}
		else
		{
			passed=map->IsHexPassed(knock_hx,knock_hy);
		}
		if(!passed) SCRIPT_ERROR_R0("Knock hexes is busy.");
	}

	KnockoutCritter(cr,face_up,lose_ap,knock_hx,knock_hy);
	return true;
}

void FOServer::SScriptFunc::Crit_RefreshVisible(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->ProcessVisibleCritters();
	cr->ProcessVisibleItems();
}

void FOServer::SScriptFunc::Crit_ViewMap(Critter* cr, Map* map, DWORD look, WORD hx, WORD hy, BYTE dir)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(map->IsNotValid) SCRIPT_ERROR_R("Map arg nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");
	if(!cr->IsPlayer()) return;
	if(dir>5) dir=cr->GetDir();
	if(!look) look=cr->GetLook();

	cr->ViewMapId=map->GetId();
	cr->ViewMapPid=map->GetPid();
	cr->ViewMapLook=look;
	cr->ViewMapHx=hx;
	cr->ViewMapHy=hy;
	cr->ViewMapDir=dir;
	cr->ViewMapLocId=0;
	cr->ViewMapLocEnt=0;
	cr->Send_LoadMap(map);
}

void FOServer::SScriptFunc::Crit_AddScore(Critter* cr, int score, int val)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(score<0) SCRIPT_ERROR_R("Score arg is negative.");
	if(score>=SCORES_MAX) SCRIPT_ERROR_R("Score arg is greater than max scores.");
	Self->SetScore(score,cr,val);
}

void FOServer::SScriptFunc::Crit_AddHolodiskInfo(Critter* cr, DWORD holodisk_num)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	Self->AddPlayerHoloInfo(cr,holodisk_num,true);
}

void FOServer::SScriptFunc::Crit_EraseHolodiskInfo(Critter* cr, DWORD holodisk_num)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	Self->ErasePlayerHoloInfo(cr,holodisk_num,true);
}

bool FOServer::SScriptFunc::Crit_IsHolodiskInfo(Critter* cr, DWORD holodisk_num)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	for(int i=0;i<cr->Data.HoloInfoCount;i++)
		if(cr->Data.HoloInfo[i]==holodisk_num) return true;
	return false;
}

void FOServer::SScriptFunc::Crit_Say(Critter* cr, BYTE how_say, CScriptString& text)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(how_say==SAY_FLASH_WINDOW) text=" ";
	if(!text.length()) SCRIPT_ERROR_R("Text empty.");
	if(cr->IsNpc() && !cr->IsLife()) return; //SCRIPT_ERROR_R("Npc is not life.");

	if(how_say>=SAY_NETMSG) cr->Send_Text(cr,text.c_str(),how_say);
	else if(cr->GetMap()) cr->SendAA_Text(cr->VisCr,text.c_str(),how_say,false);
}

void FOServer::SScriptFunc::Crit_SayMsg(Critter* cr, BYTE how_say, WORD text_msg, DWORD num_str)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNpc() && !cr->IsLife()) return; //SCRIPT_ERROR_R("Npc is not life.");

	if(how_say>=SAY_NETMSG) cr->Send_TextMsg(cr,num_str,how_say,text_msg);
	else if(cr->GetMap()) cr->SendAA_Msg(cr->VisCr,num_str,how_say,text_msg);
}

void FOServer::SScriptFunc::Crit_SayMsgLex(Critter* cr, BYTE how_say, WORD text_msg, DWORD num_str, CScriptString& lexems)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNpc() && !cr->IsLife()) return; //SCRIPT_ERROR_R("Npc is not life.");
	if(!lexems.length()) SCRIPT_ERROR_R("Lexems arg is empty.");

	if(how_say>=SAY_NETMSG) cr->Send_TextMsgLex(cr,num_str,how_say,text_msg,lexems.c_str());
	else if(cr->GetMap()) cr->SendAA_MsgLex(cr->VisCr,num_str,how_say,text_msg,lexems.c_str());
}

void FOServer::SScriptFunc::Crit_SetDir(Critter* cr, BYTE dir)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(dir>5) SCRIPT_ERROR_R("Invalid direction arg.");
	if(cr->Data.Dir==dir) return;//SCRIPT_ERROR_R("Direction already set.");

	cr->Data.Dir=dir;
	if(cr->GetMap())
	{
		cr->Send_Dir(cr);
		cr->SendA_Dir();
	}
}

bool FOServer::SScriptFunc::Crit_PickItem(Critter* cr, WORD hx, WORD hy, WORD pid)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	Map* map=MapMngr.GetMap(cr->GetMap());
	if(!map) SCRIPT_ERROR_R0("Map not found.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	bool pick=Self->Act_PickItem(cr,hx,hy,pid);
	if(!pick) SCRIPT_ERROR_R0("Pick fail.");
	return true;
}

void FOServer::SScriptFunc::Crit_SetFavoriteItem(Critter* cr, int slot, WORD pid)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	switch(slot)
	{
	case SLOT_HAND1: cr->Data.FavoriteItemPid[SLOT_HAND1]=pid; break;
	case SLOT_HAND2: cr->Data.FavoriteItemPid[SLOT_HAND2]=pid; break;
	case SLOT_ARMOR: cr->Data.FavoriteItemPid[SLOT_ARMOR]=pid; break;
	default: SCRIPT_ERROR("Invalid slot arg."); break;
	}
}

WORD FOServer::SScriptFunc::Crit_GetFavoriteItem(Critter* cr, int slot)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	switch(slot)
	{
	case SLOT_HAND1: return cr->Data.FavoriteItemPid[SLOT_HAND1];
	case SLOT_HAND2: return cr->Data.FavoriteItemPid[SLOT_HAND2];
	case SLOT_ARMOR: return cr->Data.FavoriteItemPid[SLOT_ARMOR];
	default: SCRIPT_ERROR("Invalid slot arg."); break;
	}
	return 0;
}

DWORD FOServer::SScriptFunc::Crit_GetCritters(Critter* cr, bool look_on_me, int find_type, asIScriptArray* critters)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	CrVec cr_vec;
	for(CrVecIt it=(look_on_me?cr->VisCr.begin():cr->VisCrSelf.begin()),end=(look_on_me?cr->VisCr.end():cr->VisCrSelf.end());it!=end;++it)
	{
		Critter* cr_=*it;
		if(cr_->CheckFind(find_type)) cr_vec.push_back(cr_);
	}
	if(cr_vec.empty()) return 0;
	if(critters)
	{
		SortCritterByDist(cr,cr_vec);
		Script::AppendVectorToArrayRef<Critter*>(cr_vec,critters);
	}
	return cr_vec.size();
}

DWORD FOServer::SScriptFunc::Crit_GetFollowGroup(Critter* cr, int find_type, asIScriptArray* critters)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");

	CrVec cr_vec;
	for(CrVecIt it=cr->VisCrSelf.begin(),end=cr->VisCrSelf.end();it!=end;++it)
	{
		Critter* cr_=*it;
		if(cr_->GetFollowCrId()==cr->GetId() && cr_->CheckFind(find_type)) cr_vec.push_back(cr_);
	}
	if(cr_vec.empty()) return 0;
	if(critters)
	{
		SortCritterByDist(cr,cr_vec);
		Script::AppendVectorToArrayRef<Critter*>(cr_vec,critters);
	}
	return cr_vec.size();
}

Critter* FOServer::SScriptFunc::Crit_GetFollowLeader(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	DWORD leader_id=cr->GetFollowCrId();
	if(!leader_id) return NULL;
	return cr->GetCritSelf(leader_id);
}

asIScriptArray* FOServer::SScriptFunc::Crit_GetGlobalGroup(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->GetMap() || !cr->GroupMove) return NULL;
	asIScriptArray* result=MapMngr.GM_CreateGroupArray(cr->GroupMove);
	if(!result) SCRIPT_ERROR_R0("Fail to create group.");
	return result;
}

bool FOServer::SScriptFunc::Crit_IsGlobalGroupLeader(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return !cr->GetMap() && cr->GroupMove && cr->GroupMove->Rule==cr;
}

void FOServer::SScriptFunc::Crit_LeaveGlobalGroup(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	//if(cr->GetMap() || !cr->GroupMove || cr->GroupMove->GetSize()<2) return;
	MapMngr.GM_LeaveGroup(cr);
}

void FOServer::SScriptFunc::Crit_GiveGlobalGroupLead(Critter* cr, Critter* to_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(to_cr->IsNotValid) SCRIPT_ERROR_R("To critter this nullptr.");
	//if(cr->GetMap() || !cr->GroupMove || cr->GroupMove->GetSize()<2 || cr==to_cr) return;
	MapMngr.GM_GiveRule(cr,to_cr);
}

DWORD FOServer::SScriptFunc::Npc_GetTalkedPlayers(Critter* cr, asIScriptArray* players)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cr->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");

	DWORD talk=0;
	CrVec players_;
	for(CrVecIt it=cr->VisCr.begin(),end=cr->VisCr.end();it!=end;++it)
	{
		if(!(*it)->IsPlayer()) continue;
		Client* cl=(Client*)(*it);
		if(cl->Talk.TalkType==TALK_WITH_NPC && cl->Talk.TalkNpc==cr->GetId())
		{
			talk++;
			if(players) players_.push_back(cl);
		}
	}

	if(players)
	{
		SortCritterByDist(cr,players_);
		Script::AppendVectorToArrayRef<Critter*>(players_,players);
	}
	return talk;
}

bool FOServer::SScriptFunc::Crit_IsSeeCr(Critter* cr, Critter* cr_)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr_->IsNotValid) return false;
	if(cr==cr_) return true;
	if(!cr->GetMap()) return cr->GroupMove->GetCritter(cr_->GetId())!=NULL;
	return std::find(cr_->VisCr.begin(),cr_->VisCr.end(),cr)!=cr_->VisCr.end();
}

bool FOServer::SScriptFunc::Crit_IsSeenByCr(Critter* cr, Critter* cr_)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr_->IsNotValid) return false;
	if(cr==cr_) return true;
	if(!cr->GetMap()) return cr->GroupMove->GetCritter(cr_->GetId())!=NULL;
	return std::find(cr->VisCr.begin(),cr->VisCr.end(),cr_)!=cr->VisCr.end();
}

bool FOServer::SScriptFunc::Crit_IsSeeItem(Critter* cr, Item* item)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(item->IsNotValid) return false;
	return cr->CountIdVisItem(item->GetId());
}

Item* FOServer::SScriptFunc::Crit_AddItem(Critter* cr, WORD pid, DWORD count)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!pid) SCRIPT_ERROR_R0("Proto id arg is zero.");
	if(!count) SCRIPT_ERROR_R0("Count arg is zero.");
	if(!ItemMngr.GetProtoItem(pid)) SCRIPT_ERROR_R0("Invalid pid.");
	return ItemMngr.AddItemCritter(cr,pid,count);
}

bool FOServer::SScriptFunc::Crit_DeleteItem(Critter* cr, WORD pid, DWORD count)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!pid) SCRIPT_ERROR_R0("Proto id arg is zero.");
	if(!count) count=cr->CountItemPid(pid);
	return ItemMngr.SubItemCritter(cr,pid,count);
}

DWORD FOServer::SScriptFunc::Crit_ItemsCount(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->CountItems();
}

DWORD FOServer::SScriptFunc::Crit_ItemsWeight(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetItemsWeight();
}

DWORD FOServer::SScriptFunc::Crit_ItemsVolume(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetItemsVolume();
}

DWORD FOServer::SScriptFunc::Crit_CountItem(Critter* cr, WORD proto_id)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->CountItemPid(proto_id);
}

Item* FOServer::SScriptFunc::Crit_GetItem(Critter* cr, WORD proto_id, int slot)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(proto_id) return cr->GetItemByPidInvPriority(proto_id);
	return cr->GetItemSlot(slot);
}

Item* FOServer::SScriptFunc::Crit_GetItemById(Critter* cr, DWORD item_id)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetItem(item_id,false);
}

DWORD FOServer::SScriptFunc::Crit_GetItems(Critter* cr, int slot, asIScriptArray* items)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ItemPtrVec items_;
	cr->GetItemsSlot(slot,items_);
	if(items) Script::AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

DWORD FOServer::SScriptFunc::Crit_GetItemsByType(Critter* cr, int type, asIScriptArray* items)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ItemPtrVec items_;
	cr->GetItemsType(type,items_);
	if(items) Script::AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

ProtoItem* FOServer::SScriptFunc::Crit_GetSlotProto(Critter* cr, int slot)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ProtoItem* proto_item;
	switch(slot)
	{
	case SLOT_HAND1: proto_item=cr->ItemSlotMain->Proto; break;
	case SLOT_HAND2: proto_item=(cr->ItemSlotExt->GetId()?cr->ItemSlotExt->Proto:cr->GetDefaultItemSlotMain().Proto); break;
	case SLOT_ARMOR: proto_item=cr->ItemSlotArmor->Proto; break;
	default:
		{
			Item* item=cr->GetItemSlot(slot);
			if(item) proto_item=item->Proto;
		}
		break;
	}
	return proto_item;
}

bool FOServer::SScriptFunc::Crit_MoveItem(Critter* cr, DWORD item_id, DWORD count, BYTE to_slot)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!item_id) SCRIPT_ERROR_R0("Item id arg is zero.");
	Item* item=cr->GetItem(item_id,cr->IsPlayer());
	if(!item) SCRIPT_ERROR_R0("Item not found.");
	if(!count) count=item->GetCount();
	if(item->ACC_CRITTER.Slot==to_slot) return true;//SCRIPT_ERROR_R0("To slot arg is equal of current item slot.");
	if(count>item->GetCount()) SCRIPT_ERROR_R0("Item count arg is greater than items count.");
	bool result=cr->MoveItem(item->ACC_CRITTER.Slot,to_slot,item_id,count);
	if(!result) return false; //SCRIPT_ERROR_R0("Fail to move item.");
	return true;
}

DWORD FOServer::SScriptFunc::Npc_ErasePlane(Critter* npc, int plane_type, bool all)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");

	Npc* npc_=(Npc*)npc;
	AIDataPlaneVec& planes=npc_->GetPlanes();
	DWORD result=0;
	for(AIDataPlaneVecIt it=planes.begin();it!=planes.end();)
	{
		AIDataPlane* p=*it;
		if(p->Type==plane_type || plane_type==-1)
		{
			if(!result && it==planes.begin()) npc->SendA_XY();

			p->Assigned=false;
			p->Release();
			it=planes.erase(it);

			result++;
			if(!all) break;
		}
		else ++it;
	}

	return result;
}

bool FOServer::SScriptFunc::Npc_ErasePlaneIndex(Critter* npc, DWORD index)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");

	Npc* npc_=(Npc*)npc;
	AIDataPlaneVec& planes=npc_->GetPlanes();
	if(index>=planes.size()) SCRIPT_ERROR_R0("Invalid index arg.");

	AIDataPlane* p=planes[index];
	if(!index) npc->SendA_XY();
	p->Assigned=false;
	p->Release();
	planes.erase(planes.begin()+index);
	return true;
}

void FOServer::SScriptFunc::Npc_DropPlanes(Critter* npc)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R("Critter is not npc.");

	Npc* npc_=(Npc*)npc;
	AIDataPlaneVec& planes=npc_->GetPlanes();
	npc_->DropPlanes();
}

bool FOServer::SScriptFunc::Npc_IsNoPlanes(Critter* npc)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");
	return ((Npc*)npc)->IsNoPlanes();
}

bool FOServer::SScriptFunc::Npc_IsCurPlane(Critter* npc, int plane_type)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");
	Npc* npc_=(Npc*)npc;
	return npc_->IsCurPlane(plane_type);
}

AIDataPlane* FOServer::SScriptFunc::Npc_GetCurPlane(Critter* npc)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");
	Npc* npc_=(Npc*)npc;
	if(npc_->IsNoPlanes()) return NULL;
	return npc_->GetPlanes()[0];
}

DWORD FOServer::SScriptFunc::Npc_GetPlanes(Critter* npc, asIScriptArray* arr)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");
	Npc* npc_=(Npc*)npc;
	if(npc_->IsNoPlanes()) return 0;
	if(arr) Script::AppendVectorToArrayRef<AIDataPlane*>(npc_->GetPlanes(),arr);
	return npc_->GetPlanes().size();
}

DWORD FOServer::SScriptFunc::Npc_GetPlanesIdentifier(Critter* npc, int identifier, asIScriptArray* arr)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");
	Npc* npc_=(Npc*)npc;
	if(npc_->IsNoPlanes()) return 0;
	AIDataPlaneVec planes=npc_->GetPlanes(); // Copy
	for(AIDataPlaneVecIt it=planes.begin();it!=planes.end();) if((*it)->Identifier!=identifier) it=planes.erase(it); else ++it;
	if(!arr) return planes.size();
	Script::AppendVectorToArrayRef<AIDataPlane*>(planes,arr);
	return planes.size();
}

DWORD FOServer::SScriptFunc::Npc_GetPlanesIdentifier2(Critter* npc, int identifier, DWORD identifier_ext, asIScriptArray* arr)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");
	Npc* npc_=(Npc*)npc;
	if(npc_->IsNoPlanes()) return 0;
	AIDataPlaneVec planes=npc_->GetPlanes(); // Copy
	for(AIDataPlaneVecIt it=planes.begin();it!=planes.end();) if((*it)->Identifier!=identifier || (*it)->IdentifierExt!=identifier_ext) it=planes.erase(it); else ++it;
	if(!arr) return planes.size();
	Script::AppendVectorToArrayRef<AIDataPlane*>(planes,arr);
	return planes.size();
}

bool FOServer::SScriptFunc::Npc_AddPlane(Critter* npc, AIDataPlane& plane)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Critter is not npc.");
	Npc* npc_=(Npc*)npc;
	if(npc_->IsNoPlanes()) npc_->SetWait(0);
	if(plane.Assigned)
	{
		npc_->AddPlane(REASON_FROM_SCRIPT,plane.GetCopy(),false);
	}
	else
	{
		plane.AddRef();
		npc_->AddPlane(REASON_FROM_SCRIPT,&plane,false);
	}
	return true;
}

void FOServer::SScriptFunc::Crit_SendMessage(Critter* cr, int num, int val, int to)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->SendMessage(num,val,to);
}

void FOServer::SScriptFunc::Crit_SendCombatResult(Critter* cr, asIScriptArray& arr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(arr.GetElementSize()!=sizeof(DWORD)) SCRIPT_ERROR_R("Element size is not equal to 4.");
	if(arr.GetElementCount()>GameOpt.FloodSize/sizeof(DWORD)) SCRIPT_ERROR_R("Elements count is greater than maximum.");
	cr->Send_CombatResult((DWORD*)arr.GetElementPointer(0),arr.GetElementCount());
}

void FOServer::SScriptFunc::Crit_Action(Critter* cr, int action, int action_ext, Item* item)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->SendAA_Action(action,action_ext,item);
}

void FOServer::SScriptFunc::Crit_Animate(Critter* cr, DWORD anim1, DWORD anim2, Item* item, bool clear_sequence, bool delay_play)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->SendAA_Animate(anim1,anim2,item,clear_sequence,delay_play);
}

void FOServer::SScriptFunc::Crit_PlaySound(Critter* cr, CScriptString& sound_name, bool send_self)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");

	char sound_name_[16];
	strncpy(sound_name_,sound_name.c_str(),16);
	DWORD crid=cr->GetId();

	if(send_self) cr->Send_PlaySound(crid,sound_name_);
	for(CrVecIt it=cr->VisCr.begin(),end=cr->VisCr.end();it!=end;++it)
	{
		Critter* cr_=*it;
		cr_->Send_PlaySound(crid,sound_name_);
	}
}

void FOServer::SScriptFunc::Crit_PlaySoundType(Critter* cr, BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext, bool send_self)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");

	DWORD crid=cr->GetId();
	if(send_self) cr->Send_PlaySoundType(crid,sound_type,sound_type_ext,sound_id,sound_id_ext);
	for(CrVecIt it=cr->VisCr.begin(),end=cr->VisCr.end();it!=end;++it)
	{
		Critter* cr_=*it;
		cr_->Send_PlaySoundType(crid,sound_type,sound_type_ext,sound_id,sound_id_ext);
	}
}

bool FOServer::SScriptFunc::Cl_IsKnownLoc(Critter* cl, bool by_id, DWORD loc_num)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cl->IsPlayer()) SCRIPT_ERROR_R0("Critter is not player.");
	Client* cl_=(Client*)cl;
	if(by_id) return cl_->CheckKnownLocById(loc_num);
	return cl_->CheckKnownLocByPid((WORD)loc_num);
}

bool FOServer::SScriptFunc::Cl_SetKnownLoc(Critter* cl, bool by_id, DWORD loc_num)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cl->IsPlayer()) SCRIPT_ERROR_R0("Critter is not player.");
	Client* cl_=(Client*)cl;
	Location* loc=(by_id?MapMngr.GetLocation(loc_num):MapMngr.GetLocationByPid((WORD)loc_num,0));
	if(!loc) SCRIPT_ERROR_R0("Location not found.");

	cl_->AddKnownLoc(loc->GetId());
	if(loc->IsAutomaps()) cl_->Send_AutomapsInfo(NULL,loc);
	if(!cl_->GetMap()) cl_->Send_GlobalLocation(loc,true);

	int zx=GM_ZONE(loc->Data.WX);
	int zy=GM_ZONE(loc->Data.WY);
	if(cl_->GMapFog.Get2Bit(zx,zy)==GM_FOG_FULL)
	{
		cl_->GMapFog.Set2Bit(zx,zy,GM_FOG_SELF);
		if(!cl_->GetMap()) cl_->Send_GlobalMapFog(zx,zy,cl_->GMapFog.Get2Bit(zx,zy));
	}
	return true;
}

bool FOServer::SScriptFunc::Cl_UnsetKnownLoc(Critter* cl, bool by_id, DWORD loc_num)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cl->IsPlayer()) SCRIPT_ERROR_R0("Critter is not player.");

	Client* cl_=(Client*)cl;
	Location* loc=(by_id?MapMngr.GetLocation(loc_num):MapMngr.GetLocationByPid((WORD)loc_num,0));
	if(!loc) SCRIPT_ERROR_R0("Location not found.");
	if(!cl_->CheckKnownLocById(loc->GetId())) SCRIPT_ERROR_R0("Player is not know this location.");

	cl_->EraseKnownLoc(loc->GetId());
	if(!cl_->GetMap()) cl_->Send_GlobalLocation(loc,false);
	return true;
}

void FOServer::SScriptFunc::Cl_SetFog(Critter* cl, WORD zone_x, WORD zone_y, int fog)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!cl->IsPlayer()) SCRIPT_ERROR_R("Critter is not player.");
	if(zone_x>=GameOpt.GlobalMapWidth || zone_y>=GameOpt.GlobalMapHeight) SCRIPT_ERROR_R("Invalid world map pos arg.");
	if(fog<GM_FOG_FULL || fog>GM_FOG_NONE) SCRIPT_ERROR_R("Invalid fog arg.");

	Client* cl_=(Client*)cl;
	cl_->GetDataExt(); // Generate ext data
	cl_->GMapFog.Set2Bit(zone_x,zone_y,fog);
	if(!cl_->GetMap()) cl_->Send_GlobalMapFog(zone_x,zone_y,fog);
}

int FOServer::SScriptFunc::Cl_GetFog(Critter* cl, WORD zone_x, WORD zone_y)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cl->IsPlayer()) SCRIPT_ERROR_R0("Critter is not player.");
	if(zone_x>=GameOpt.GlobalMapWidth || zone_y>=GameOpt.GlobalMapHeight) SCRIPT_ERROR_R0("Invalid world map pos arg.");

	Client* cl_=(Client*)cl;
	cl_->GetDataExt(); // Generate ext data
	return cl_->GMapFog.Get2Bit(zone_x,zone_y);
}

void FOServer::SScriptFunc::Cl_ShowContainer(Critter* cl, Critter* cr_cont, Item* item_cont, BYTE transfer_type)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!cl->IsPlayer()) return; //SCRIPT_ERROR_R("Critter is not player.");
	if(cr_cont)
	{
		if(cr_cont->IsNotValid) SCRIPT_ERROR_R("Critter container nullptr.");
		((Client*)cl)->Send_ContainerInfo(cr_cont,transfer_type,true);
	}
	else if(item_cont)
	{
		if(item_cont->IsNotValid) SCRIPT_ERROR_R("Item container nullptr.");
		((Client*)cl)->Send_ContainerInfo(item_cont,transfer_type,true);
	}
	else
	{
		((Client*)cl)->Send_ContainerInfo();
	}
}

void FOServer::SScriptFunc::Cl_ShowScreen(Critter* cl, int screen_type, DWORD param, CScriptString& func_name)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!cl->IsPlayer()) return; //SCRIPT_ERROR_R("Critter is not player.");

	int bind_id=0;
	if(func_name.length())
	{
		bind_id=Script::Bind(func_name.c_str(),"void %s(Critter&,uint,string&)",true);
		if(bind_id<=0) SCRIPT_ERROR_R("Function not found.");
	}

	Client* cl_=(Client*)cl;
	cl_->ScreenCallbackBindId=bind_id;
	cl_->Send_ShowScreen(screen_type,param,bind_id!=0);
}

void FOServer::SScriptFunc::Cl_RunClientScript(Critter* cl, CScriptString& func_name, int p0, int p1, int p2, CScriptString* p3, asIScriptArray* p4)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!cl->IsPlayer()) SCRIPT_ERROR_R("Critter is not player.");

	DwordVec dw;
	if(p4) Script::AssignScriptArrayInVector<DWORD>(dw,p4);

	Client* cl_=(Client*)cl;
	cl_->Send_RunClientScript(func_name.c_str(),p0,p1,p2,p3?p3->c_str():NULL,dw);
}

void FOServer::SScriptFunc::Cl_Disconnect(Critter* cl)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!cl->IsPlayer()) SCRIPT_ERROR_R("Critter is not player.");
	Client* cl_=(Client*)cl;
	if(cl_->IsOnline()) cl_->Disconnect();
}

bool FOServer::SScriptFunc::Crit_SetScript(Critter* cr, CScriptString* script)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!script || !script->length())
	{
		cr->Data.ScriptId=0;
	}
	else
	{
		if(!cr->ParseScript(script->c_str(),true)) SCRIPT_ERROR_R0("Script function not found.");
	}
	return true;
}

DWORD FOServer::SScriptFunc::Crit_GetScriptId(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->Data.ScriptId;
}

WORD FOServer::SScriptFunc::Crit_GetProtoId(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->Data.ProtoId;
}

DWORD FOServer::SScriptFunc::Crit_GetMultihex(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->GetMultihex();
}

void FOServer::SScriptFunc::Crit_SetMultihex(Critter* cr, int value)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(value<-1 || value>MAX_HEX_OFFSET) SCRIPT_ERROR_R("Invalid multihex arg value.");
	if(cr->Data.Multihex==value) return;

	DWORD old_mh=cr->GetMultihex();
	cr->Data.Multihex=value;
	DWORD new_mh=cr->GetMultihex();

	if(old_mh!=new_mh && cr->GetMap() && !cr->IsDead())
	{
		Map* map=MapMngr.GetMap(cr->GetMap());
		if(map)
		{
			map->UnsetFlagCritter(cr->GetHexX(),cr->GetHexY(),old_mh,false);
			map->SetFlagCritter(cr->GetHexX(),cr->GetHexY(),new_mh,false);
		}
	}

	cr->Send_ParamOther(OTHER_MULTIHEX,value);
	cr->SendA_ParamOther(OTHER_MULTIHEX,value);
}

void FOServer::SScriptFunc::Crit_AddEnemyInStack(Critter* cr, DWORD critter_id)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->AddEnemyInStack(critter_id);
}

bool FOServer::SScriptFunc::Crit_CheckEnemyInStack(Critter* cr, DWORD critter_id)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->CheckEnemyInStack(critter_id);
}

void FOServer::SScriptFunc::Crit_EraseEnemyFromStack(Critter* cr, DWORD critter_id)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->EraseEnemyInStack(critter_id);
}

void FOServer::SScriptFunc::Crit_ChangeEnemyStackSize(Critter* cr, DWORD new_size)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(new_size>MAX_ENEMY_STACK) SCRIPT_ERROR_R("New size arg is greater than max enemy stack size.");
	for(int i=cr->Data.EnemyStackCount;i<MAX_ENEMY_STACK;i++) cr->Data.EnemyStack[i]=0;
	cr->Data.EnemyStackCount=new_size;
}

void FOServer::SScriptFunc::Crit_GetEnemyStack(Critter* cr, asIScriptArray& arr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	int stack_count=min(cr->Data.EnemyStackCount,MAX_ENEMY_STACK);
	DwordVec dw;
	dw.resize(stack_count);
	for(int i=0;i<stack_count;i++) dw[i]=cr->Data.EnemyStack[i];
	Script::AppendVectorToArray(dw,&arr);
}

void FOServer::SScriptFunc::Crit_ClearEnemyStack(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	for(int i=0;i<MAX_ENEMY_STACK;i++) cr->Data.EnemyStack[i]=0;
}

void FOServer::SScriptFunc::Crit_ClearEnemyStackNpc(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	for(int i=0;i<MAX_ENEMY_STACK;i++)
	{
		if(IS_NPC_ID(cr->Data.EnemyStack[i]))
		{
			cr->Data.EnemyStack[i]=0;
			for(int j=i;j<MAX_ENEMY_STACK-1;j++) cr->Data.EnemyStack[i]=cr->Data.EnemyStack[i+1];
		}
	}
}

bool FOServer::SScriptFunc::Crit_AddTimeEvent(Critter* cr, CScriptString& func_name, DWORD duration, int identifier)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	DWORD func_num=Script::GetScriptFuncNum(func_name.c_str(),"uint %s(Critter&,int,uint&)");
	if(!func_num) SCRIPT_ERROR_R0("Function not found.");
	cr->AddCrTimeEvent(func_num,0,duration,identifier);
	return true;
}

bool FOServer::SScriptFunc::Crit_AddTimeEventRate(Critter* cr, CScriptString& func_name, DWORD duration, int identifier, DWORD rate)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	DWORD func_num=Script::GetScriptFuncNum(func_name.c_str(),"uint %s(Critter&,int,uint&)");
	if(!func_num) SCRIPT_ERROR_R0("Function not found.");
	cr->AddCrTimeEvent(func_num,rate,duration,identifier);
	return true;
}

DWORD FOServer::SScriptFunc::Crit_GetTimeEvents(Critter* cr, int identifier, asIScriptArray* indexes, asIScriptArray* durations, asIScriptArray* rates)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	DWORD index=0;
	DwordVec te_vec;
	for(Critter::CrTimeEventVecIt it=cr->CrTimeEvents.begin(),end=cr->CrTimeEvents.end();it!=end;++it)
	{
		Critter::CrTimeEvent& cte=*it;
		if(cte.Identifier==identifier) te_vec.push_back(index);
		index++;
	}

	int size=te_vec.size();
	if(!size || (!indexes && !durations && !rates)) return size;

	int indexes_size,durations_size,rates_size;
	if(indexes)
	{
		indexes_size=indexes->GetElementCount();
		indexes->Resize(indexes_size+size);
	}
	if(durations)
	{
		durations_size=durations->GetElementCount();
		durations->Resize(durations_size+size);
	}
	if(rates)
	{
		rates_size=rates->GetElementCount();
		rates->Resize(rates_size+size);
	}

	for(int i=0;i<size;i++)
	{
		Critter::CrTimeEvent& cte=cr->CrTimeEvents[te_vec[i]];
		if(indexes) *(DWORD*)indexes->GetElementPointer(indexes_size+i)=te_vec[i];
		if(durations) *(DWORD*)durations->GetElementPointer(durations_size+i)=(cte.NextTime>GameOpt.FullSecond?cte.NextTime-GameOpt.FullSecond:0);
		if(rates) *(DWORD*)rates->GetElementPointer(rates_size+i)=cte.Rate;
	}
	return size;
}

DWORD FOServer::SScriptFunc::Crit_GetTimeEventsArr(Critter* cr, asIScriptArray& find_identifiers, asIScriptArray* identifiers, asIScriptArray* indexes, asIScriptArray* durations, asIScriptArray* rates)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");

	IntVec find_vec;
	Script::AssignScriptArrayInVector(find_vec,&find_identifiers);

	DWORD index=0;
	DwordVec te_vec;
	for(Critter::CrTimeEventVecIt it=cr->CrTimeEvents.begin(),end=cr->CrTimeEvents.end();it!=end;++it)
	{
		Critter::CrTimeEvent& cte=*it;
		if(std::find(find_vec.begin(),find_vec.end(),cte.Identifier)!=find_vec.end()) te_vec.push_back(index);
		index++;
	}

	int size=te_vec.size();
	if(!size || (!identifiers && !indexes && !durations && !rates)) return size;

	int identifiers_size,indexes_size,durations_size,rates_size;
	if(identifiers)
	{
		identifiers_size=identifiers->GetElementCount();
		identifiers->Resize(identifiers_size+size);
	}
	if(indexes)
	{
		indexes_size=indexes->GetElementCount();
		indexes->Resize(indexes_size+size);
	}
	if(durations)
	{
		durations_size=durations->GetElementCount();
		durations->Resize(durations_size+size);
	}
	if(rates)
	{
		rates_size=rates->GetElementCount();
		rates->Resize(rates_size+size);
	}

	for(int i=0;i<size;i++)
	{
		Critter::CrTimeEvent& cte=cr->CrTimeEvents[te_vec[i]];
		if(identifiers) *(int*)identifiers->GetElementPointer(identifiers_size+i)=cte.Identifier;
		if(indexes) *(DWORD*)indexes->GetElementPointer(indexes_size+i)=te_vec[i];
		if(durations) *(DWORD*)durations->GetElementPointer(durations_size+i)=(cte.NextTime>GameOpt.FullSecond?cte.NextTime-GameOpt.FullSecond:0);
		if(rates) *(DWORD*)rates->GetElementPointer(rates_size+i)=cte.Rate;
	}
	return size;
}

void FOServer::SScriptFunc::Crit_ChangeTimeEvent(Critter* cr, DWORD index, DWORD new_duration, DWORD new_rate)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(index>=cr->CrTimeEvents.size()) SCRIPT_ERROR_R("Index arg is greater than maximum time events.");
	Critter::CrTimeEvent cte=cr->CrTimeEvents[index];
	cr->EraseCrTimeEvent(index);
	cr->AddCrTimeEvent(cte.FuncNum,new_rate,new_duration,cte.Identifier);
}

void FOServer::SScriptFunc::Crit_EraseTimeEvent(Critter* cr, DWORD index)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(index>=cr->CrTimeEvents.size()) SCRIPT_ERROR_R("Index arg is greater than maximum time events.");
	cr->EraseCrTimeEvent(index);
}

DWORD FOServer::SScriptFunc::Crit_EraseTimeEvents(Critter* cr, int identifier)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	DWORD result=0;
	for(Critter::CrTimeEventVecIt it=cr->CrTimeEvents.begin();it!=cr->CrTimeEvents.end();)
	{
		Critter::CrTimeEvent& cte=*it;
		if(cte.Identifier==identifier)
		{
			it=cr->CrTimeEvents.erase(it);
			result++;
		}
		else ++it;
	}
	return result;
}

DWORD FOServer::SScriptFunc::Crit_EraseTimeEventsArr(Critter* cr, asIScriptArray& identifiers)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	DWORD result=0;
	for(int i=0,j=identifiers.GetElementCount();i<j;i++)
	{
		int identifier=*(int*)identifiers.GetElementPointer(i);
		for(Critter::CrTimeEventVecIt it=cr->CrTimeEvents.begin();it!=cr->CrTimeEvents.end();)
		{
			Critter::CrTimeEvent& cte=*it;
			if(cte.Identifier==identifier)
			{
				it=cr->CrTimeEvents.erase(it);
				result++;
			}
			else ++it;
		}
	}
	return result;
}

void FOServer::SScriptFunc::Crit_SetBagRefreshTime(Critter* cr, DWORD real_minutes)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->Data.BagRefreshTime=real_minutes;
}

DWORD FOServer::SScriptFunc::Crit_GetBagRefreshTime(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return (DWORD)cr->Data.BagRefreshTime;
}

void FOServer::SScriptFunc::Crit_SetInternalBag(Critter* cr, asIScriptArray& pids, asIScriptArray* min_counts, asIScriptArray* max_counts, asIScriptArray* slots)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(pids.GetElementCount()>=MAX_NPC_BAGS) SCRIPT_ERROR_R("Pids count arg is greater than maximum.");
	if(min_counts && min_counts->GetElementCount()!=pids.GetElementCount()) SCRIPT_ERROR_R("Different sizes in pids and minCounts args.");
	if(max_counts && max_counts->GetElementCount()!=pids.GetElementCount()) SCRIPT_ERROR_R("Different sizes in pids and maxCounts args.");
	if(slots && slots->GetElementCount()!=pids.GetElementCount()) SCRIPT_ERROR_R("Different sizes in pids and slots args.");

	DWORD count=pids.GetElementCount();
	DWORD end_count=0;
	for(int i=0;i<count;i++)
	{
		WORD pid=*(WORD*)pids.GetElementPointer(i);
		ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
		if(!proto_item)
		{
			SCRIPT_ERROR("Proto item not found, skip.");
			continue;
		}
		DWORD min_count=(min_counts?*(DWORD*)min_counts->GetElementPointer(i):1);
		DWORD max_count=(max_counts?*(DWORD*)max_counts->GetElementPointer(i):1);
		DWORD slot=(slots?*(int*)slots->GetElementPointer(i):SLOT_INV);
		if(min_count>max_count)
		{
			SCRIPT_ERROR("Min count is greater than max count, skip.");
			continue;
		}

		NpcBagItem& item=cr->Data.Bag[i];
		item.ItemPid=pid;
		item.MinCnt=min_count;
		item.MaxCnt=max_count;
		item.ItemSlot=slot;
		end_count++;
	}
	cr->Data.BagSize=end_count;
}

DWORD FOServer::SScriptFunc::Crit_GetInternalBag(Critter* cr, asIScriptArray* pids, asIScriptArray* min_counts, asIScriptArray* max_counts, asIScriptArray* slots)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!cr->Data.BagSize) return 0;

	DWORD count=cr->Data.BagSize;
	if(pids || min_counts || max_counts || slots)
	{
		if(pids) pids->Resize(pids->GetElementCount()+count);
		if(min_counts) min_counts->Resize(min_counts->GetElementCount()+count);
		if(max_counts) max_counts->Resize(max_counts->GetElementCount()+count);
		if(slots) slots->Resize(slots->GetElementCount()+count);
		for(int i=0;i<count;i++)
		{
			NpcBagItem& item=cr->Data.Bag[i];
			if(pids) *(WORD*)pids->GetElementPointer(pids->GetElementCount()-count+i)=item.ItemPid;
			if(min_counts) *(DWORD*)min_counts->GetElementPointer(min_counts->GetElementCount()-count+i)=item.MinCnt;
			if(max_counts) *(DWORD*)max_counts->GetElementPointer(max_counts->GetElementCount()-count+i)=item.MaxCnt;
			if(slots) *(int*)slots->GetElementPointer(slots->GetElementCount()-count+i)=item.ItemSlot;
		}
	}
	return count;
}

void FOServer::SScriptFunc::Crit_EventIdle(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->EventIdle();
}

void FOServer::SScriptFunc::Crit_EventFinish(Critter* cr, bool deleted)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->EventFinish(deleted);
}

void FOServer::SScriptFunc::Crit_EventDead(Critter* cr, Critter* killer)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(killer && killer->IsNotValid) SCRIPT_ERROR_R("Killer critter arg nullptr.");
	cr->EventDead(killer);
}

void FOServer::SScriptFunc::Crit_EventRespawn(Critter* cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->EventRespawn();
}

void FOServer::SScriptFunc::Crit_EventShowCritter(Critter* cr, Critter* show_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(show_cr->IsNotValid) SCRIPT_ERROR_R("Show critter arg nullptr.");
	cr->EventShowCritter(show_cr);
}

void FOServer::SScriptFunc::Crit_EventShowCritter1(Critter* cr, Critter* show_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(show_cr->IsNotValid) SCRIPT_ERROR_R("Show critter arg nullptr.");
	cr->EventShowCritter1(show_cr);
}

void FOServer::SScriptFunc::Crit_EventShowCritter2(Critter* cr, Critter* show_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(show_cr->IsNotValid) SCRIPT_ERROR_R("Show critter arg nullptr.");
	cr->EventShowCritter2(show_cr);
}

void FOServer::SScriptFunc::Crit_EventShowCritter3(Critter* cr, Critter* show_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(show_cr->IsNotValid) SCRIPT_ERROR_R("Show critter arg nullptr.");
	cr->EventShowCritter3(show_cr);
}

void FOServer::SScriptFunc::Crit_EventHideCritter(Critter* cr, Critter* hide_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hide_cr->IsNotValid) SCRIPT_ERROR_R("Hide critter arg nullptr.");
	cr->EventHideCritter(hide_cr);
}

void FOServer::SScriptFunc::Crit_EventHideCritter1(Critter* cr, Critter* hide_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hide_cr->IsNotValid) SCRIPT_ERROR_R("Hide critter arg nullptr.");
	cr->EventHideCritter1(hide_cr);
}

void FOServer::SScriptFunc::Crit_EventHideCritter2(Critter* cr, Critter* hide_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hide_cr->IsNotValid) SCRIPT_ERROR_R("Hide critter arg nullptr.");
	cr->EventHideCritter2(hide_cr);
}

void FOServer::SScriptFunc::Crit_EventHideCritter3(Critter* cr, Critter* hide_cr)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hide_cr->IsNotValid) SCRIPT_ERROR_R("Hide critter arg nullptr.");
	cr->EventHideCritter3(hide_cr);
}

void FOServer::SScriptFunc::Crit_EventShowItemOnMap(Critter* cr, Item* show_item, bool added, Critter* dropper)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(show_item->IsNotValid) SCRIPT_ERROR_R("Show item arg nullptr.");
	if(dropper && dropper->IsNotValid) SCRIPT_ERROR_R("Dropper critter arg nullptr.");
	cr->EventShowItemOnMap(show_item,added,dropper);
}

void FOServer::SScriptFunc::Crit_EventChangeItemOnMap(Critter* cr, Item* item)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	cr->EventChangeItemOnMap(item);
}

void FOServer::SScriptFunc::Crit_EventHideItemOnMap(Critter* cr, Item* hide_item, bool removed, Critter* picker)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hide_item->IsNotValid) SCRIPT_ERROR_R("Hide item arg nullptr.");
	if(picker && picker->IsNotValid) SCRIPT_ERROR_R("Picker critter arg nullptr.");
	cr->EventHideItemOnMap(hide_item,removed,picker);
}

bool FOServer::SScriptFunc::Crit_EventAttack(Critter* cr, Critter* target)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(target->IsNotValid) SCRIPT_ERROR_R0("Target critter arg nullptr.");
	return cr->EventAttack(target);
}

bool FOServer::SScriptFunc::Crit_EventAttacked(Critter* cr, Critter* attacker)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(attacker->IsNotValid) SCRIPT_ERROR_R0("Attacker critter arg nullptr.");
	return cr->EventAttacked(attacker);
}

bool FOServer::SScriptFunc::Crit_EventStealing(Critter* cr, Critter* thief, Item* item, DWORD count)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(thief->IsNotValid) SCRIPT_ERROR_R0("Thief critter arg nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R0("Item arg nullptr.");
	return cr->EventStealing(thief,item,count);
}

void FOServer::SScriptFunc::Crit_EventMessage(Critter* cr, Critter* from_cr, int message, int value)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	cr->EventMessage(from_cr,message,value);
}

bool FOServer::SScriptFunc::Crit_EventUseItem(Critter* cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R0("Item arg nullptr.");
	if(on_critter && on_critter->IsNotValid) SCRIPT_ERROR_R0("On critter arg nullptr.");
	if(on_item && on_item->IsNotValid) SCRIPT_ERROR_R0("On item arg nullptr.");
	return cr->EventUseItem(item,on_critter,on_item,on_scenery);
}

bool FOServer::SScriptFunc::Crit_EventUseSkill(Critter* cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(on_critter && on_critter->IsNotValid) SCRIPT_ERROR_R0("On critter arg nullptr.");
	if(on_item && on_item->IsNotValid) SCRIPT_ERROR_R0("On item arg nullptr.");
	return cr->EventUseSkill(skill,on_critter,on_item,on_scenery);
}

void FOServer::SScriptFunc::Crit_EventDropItem(Critter* cr, Item* item)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	cr->EventDropItem(item);
}

void FOServer::SScriptFunc::Crit_EventMoveItem(Critter* cr, Item* item, BYTE from_slot)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	cr->EventMoveItem(item,from_slot);
}

void FOServer::SScriptFunc::Crit_EventKnockout(Critter* cr, bool face_up, DWORD lost_ap, DWORD knock_dist)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	cr->EventKnockout(face_up,lost_ap,knock_dist);
}

void FOServer::SScriptFunc::Crit_EventSmthDead(Critter* cr, Critter* from_cr, Critter* killer)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	if(killer && killer->IsNotValid) SCRIPT_ERROR_R("Killer critter arg nullptr.");
	cr->EventSmthDead(from_cr,killer);
}

void FOServer::SScriptFunc::Crit_EventSmthStealing(Critter* cr, Critter* from_cr, Critter* thief, bool success, Item* item, DWORD count)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	if(thief->IsNotValid) SCRIPT_ERROR_R("Thief critter arg nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	cr->EventSmthStealing(from_cr,thief,success,item,count);
}

void FOServer::SScriptFunc::Crit_EventSmthAttack(Critter* cr, Critter* from_cr, Critter* target)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	if(target->IsNotValid) SCRIPT_ERROR_R("Target critter arg nullptr.");
	cr->EventSmthAttack(from_cr,target);
}

void FOServer::SScriptFunc::Crit_EventSmthAttacked(Critter* cr, Critter* from_cr, Critter* attacker)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	if(attacker->IsNotValid) SCRIPT_ERROR_R("Attacker critter arg nullptr.");
	cr->EventSmthAttacked(from_cr,attacker);
}

void FOServer::SScriptFunc::Crit_EventSmthUseItem(Critter* cr, Critter* from_cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	if(on_critter && on_critter->IsNotValid) SCRIPT_ERROR_R("On critter arg nullptr.");
	if(on_item && on_item->IsNotValid) SCRIPT_ERROR_R("On item arg nullptr.");
	cr->EventSmthUseItem(from_cr,item,on_critter,on_item,on_scenery);
}

void FOServer::SScriptFunc::Crit_EventSmthUseSkill(Critter* cr, Critter* from_cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	if(on_critter && on_critter->IsNotValid) SCRIPT_ERROR_R("On critter arg nullptr.");
	if(on_item && on_item->IsNotValid) SCRIPT_ERROR_R("On item arg nullptr.");
	cr->EventSmthUseSkill(from_cr,skill,on_critter,on_item,on_scenery);
}

void FOServer::SScriptFunc::Crit_EventSmthDropItem(Critter* cr, Critter* from_cr, Item* item)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	cr->EventSmthDropItem(from_cr,item);
}

void FOServer::SScriptFunc::Crit_EventSmthMoveItem(Critter* cr, Critter* from_cr, Item* item, BYTE from_slot)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	cr->EventSmthMoveItem(from_cr,item,from_slot);
}

void FOServer::SScriptFunc::Crit_EventSmthKnockout(Critter* cr, Critter* from_cr, bool face_up, DWORD lost_ap, DWORD knock_dist)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter arg nullptr.");
	cr->EventSmthKnockout(from_cr,face_up,lost_ap,knock_dist);
}

int FOServer::SScriptFunc::Crit_EventPlaneBegin(Critter* cr, AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->EventPlaneBegin(plane,reason,some_cr,some_item);
}

int FOServer::SScriptFunc::Crit_EventPlaneEnd(Critter* cr, AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->EventPlaneEnd(plane,reason,some_cr,some_item);
}

int FOServer::SScriptFunc::Crit_EventPlaneRun(Critter* cr, AIDataPlane* plane, int reason, DWORD& p0, DWORD& p1, DWORD& p2)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->EventPlaneRun(plane,reason,p0,p1,p2);
}

bool FOServer::SScriptFunc::Crit_EventBarter(Critter* cr, Critter* cr_barter, bool attach, DWORD barter_count)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr_barter->IsNotValid) SCRIPT_ERROR_R0("Barter critter nullptr.");
	return cr->EventBarter(cr_barter,attach,barter_count);
}

bool FOServer::SScriptFunc::Crit_EventTalk(Critter* cr, Critter* cr_talk, bool attach, DWORD talk_count)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr_talk->IsNotValid) SCRIPT_ERROR_R0("Barter critter nullptr.");
	return cr->EventTalk(cr_talk,attach,talk_count);
}

bool FOServer::SScriptFunc::Crit_EventGlobalProcess(Critter* cr, int type, asIScriptArray* group, Item* car, DWORD& x, DWORD& y, DWORD& to_x, DWORD& to_y, DWORD& speed, DWORD& encounter_descriptor, bool& wait_for_answer)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->EventGlobalProcess(type,group,car,x,y,to_x,to_y,speed,encounter_descriptor,wait_for_answer);
}

bool FOServer::SScriptFunc::Crit_EventGlobalInvite(Critter* cr, asIScriptArray* group, Item* car, DWORD encounter_descriptor, int combat_mode, DWORD& map_id, WORD& hx, WORD& hy, BYTE& dir)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return cr->EventGlobalInvite(group,car,encounter_descriptor,combat_mode,map_id,hx,hy,dir);
}

void FOServer::SScriptFunc::Crit_EventTurnBasedProcess(Critter* cr, Map* map, bool begin_turn)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(map->IsNotValid) SCRIPT_ERROR_R("Map nullptr.");
	cr->EventTurnBasedProcess(map,begin_turn);
}

void FOServer::SScriptFunc::Crit_EventSmthTurnBasedProcess(Critter* cr, Critter* from_cr, Map* map, bool begin_turn)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(from_cr->IsNotValid) SCRIPT_ERROR_R("From critter nullptr.");
	if(map->IsNotValid) SCRIPT_ERROR_R("Map nullptr.");
	cr->EventSmthTurnBasedProcess(from_cr,map,begin_turn);
}

GameVar* FOServer::SScriptFunc::Global_GetGlobalVar(WORD tvar_id)
{
	GameVar* gvar=VarMngr.GetVar(tvar_id,0,0,true);
	if(!gvar) SCRIPT_ERROR_R0("Global var not found.");
	return gvar;
}

GameVar* FOServer::SScriptFunc::Global_GetLocalVar(WORD tvar_id, DWORD master_id)
{
	if(!master_id) SCRIPT_ERROR_R0("Master id is zero.");
	GameVar* lvar=VarMngr.GetVar(tvar_id,master_id,0,true);
	if(!lvar) SCRIPT_ERROR_R0("Local var not found.");
	return lvar;
}

GameVar* FOServer::SScriptFunc::Global_GetUnicumVar(WORD tvar_id, DWORD master_id, DWORD slave_id)
{
	if(!master_id) SCRIPT_ERROR_R0("Master id is zero.");
	if(!slave_id) SCRIPT_ERROR_R0("Slave id is zero.");
	GameVar* uvar=VarMngr.GetVar(tvar_id,master_id,slave_id,true);
	if(!uvar) SCRIPT_ERROR_R0("Unicum var not found.");
	return uvar;
}

DWORD FOServer::SScriptFunc::Global_DeleteVars(DWORD id)
{
	return VarMngr.DeleteVars(id);
}

void FOServer::SScriptFunc::Cl_SendQuestVar(Critter* cl, GameVar* var)
{
	if(cl->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!cl->IsPlayer()) return; //SCRIPT_ERROR_R("Critter is not player.");
	if(!var->IsQuest()) SCRIPT_ERROR_R("GameVar is not quest var.");
	cl->Send_Quest(var->GetQuestStr());
}

DWORD FOServer::SScriptFunc::Map_GetId(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->GetId();
}

WORD FOServer::SScriptFunc::Map_GetProtoId(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->GetPid();
}

Location* FOServer::SScriptFunc::Map_GetLocation(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->MapLocation;
}

bool FOServer::SScriptFunc::Map_SetScript(Map* map, CScriptString* script)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!script || !script->length())
	{
		map->Data.ScriptId=0;
	}
	else
	{
		if(!map->ParseScript(script->c_str(),true)) SCRIPT_ERROR_R0("Script function not found.");
	}
	return true;
}

DWORD FOServer::SScriptFunc::Map_GetScriptId(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->Data.ScriptId;
}

bool FOServer::SScriptFunc::Map_SetEvent(Map* map, int event_type, CScriptString* func_name)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(event_type<0 || event_type>=MAP_EVENT_MAX) SCRIPT_ERROR_R0("Invalid event type arg.");
	if(!func_name || !func_name->length()) map->FuncId[event_type]=0;
	else map->FuncId[event_type]=Script::Bind(func_name->c_str(),MapEventFuncName[event_type],false);

	if(event_type>=MAP_EVENT_LOOP_0 && event_type<=MAP_EVENT_LOOP_4)
	{
		map->NeedProcess=false;
		for(int i=0;i<MAP_LOOP_FUNC_MAX;i++)
		{
			if(map->FuncId[MAP_EVENT_LOOP_0+i]<=0)
			{
				map->LoopEnabled[i]=false;
			}
			else
			{
				if(event_type==MAP_EVENT_LOOP_0+i) map->LoopLastTick[i]=Timer::GameTick();
				map->LoopEnabled[i]=true;
				map->NeedProcess=true;
			}
		}
	}

	if(func_name && func_name->length() && map->FuncId[event_type]<=0) SCRIPT_ERROR_R0("Function not found.");
	return true;
}

void FOServer::SScriptFunc::Map_SetLoopTime(Map* map, DWORD loop_num, DWORD ms)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(loop_num>=MAP_LOOP_FUNC_MAX) SCRIPT_ERROR_R("Invalid loop number arg.");
	map->SetLoopTime(loop_num,ms);
}

BYTE FOServer::SScriptFunc::Map_GetRain(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->GetRain();
}

void FOServer::SScriptFunc::Map_SetRain(Map* map, BYTE capacity)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->SetRain(capacity);
}

int FOServer::SScriptFunc::Map_GetTime(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->GetTime();
}

void FOServer::SScriptFunc::Map_SetTime(Map* map, int time)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->SetTime(time);
}

DWORD FOServer::SScriptFunc::Map_GetDayTime(Map* map, DWORD day_part)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(day_part>=4) SCRIPT_ERROR_R0("Invalid day part arg.");
	return map->GetDayTime(day_part);
}

void FOServer::SScriptFunc::Map_SetDayTime(Map* map, DWORD day_part, DWORD time)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(day_part>=4) SCRIPT_ERROR_R("Invalid day part arg.");
	if(time>=1440) SCRIPT_ERROR_R("Invalid time arg.");
	map->SetDayTime(day_part,time);
}

void FOServer::SScriptFunc::Map_GetDayColor(Map* map, DWORD day_part, BYTE& r, BYTE& g, BYTE& b)
{
	r=g=b=0;
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(day_part>=4) SCRIPT_ERROR_R("Invalid day part arg.");
	map->GetDayColor(day_part,r,g,b);
}

void FOServer::SScriptFunc::Map_SetDayColor(Map* map, DWORD day_part, BYTE r, BYTE g, BYTE b)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(day_part>=4) SCRIPT_ERROR_R("Invalid day part arg.");
	map->SetDayColor(day_part,r,g,b);
}

void FOServer::SScriptFunc::Map_SetTurnBasedAvailability(Map* map, bool value)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->Data.IsTurnBasedAviable=value;
}

bool FOServer::SScriptFunc::Map_IsTurnBasedAvailability(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->Data.IsTurnBasedAviable;
}

void FOServer::SScriptFunc::Map_BeginTurnBased(Map* map, Critter* first_turn_crit)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(first_turn_crit && first_turn_crit->IsNotValid) SCRIPT_ERROR_R("Critter arg is not valid.");
	map->BeginTurnBased(first_turn_crit);
}

bool FOServer::SScriptFunc::Map_IsTurnBased(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->IsTurnBasedOn;
}

void FOServer::SScriptFunc::Map_EndTurnBased(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->NeedEndTurnBased=true;
}

int FOServer::SScriptFunc::Map_GetTurnBasedSequence(Map* map, asIScriptArray& critters_ids)
{
	if(map->IsNotValid) SCRIPT_ERROR_RX("This nullptr.",-1);
	if(!map->IsTurnBasedOn) SCRIPT_ERROR_RX("Map is not in turn based state.",-1);
	Script::AppendVectorToArray(map->TurnSequence,&critters_ids);
	return map->TurnSequenceCur>=0 && map->TurnSequenceCur<map->TurnSequence.size()?map->TurnSequenceCur:-1;
}

void FOServer::SScriptFunc::Map_SetData(Map* map, DWORD index, int value)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(index>=MAP_MAX_DATA) SCRIPT_ERROR_R("Index arg invalid value.");
	map->SetData(index,value);
}

int FOServer::SScriptFunc::Map_GetData(Map* map, DWORD index)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(index>=MAP_MAX_DATA) SCRIPT_ERROR_R0("Index arg invalid value.");
	return map->GetData(index);
}

Item* FOServer::SScriptFunc::Map_AddItem(Map* map, WORD hx, WORD hy, WORD proto_id, DWORD count)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	ProtoItem* proto_item=ItemMngr.GetProtoItem(proto_id);
	if(!proto_item) SCRIPT_ERROR_R0("Invalid proto id arg.");
	if(proto_item->IsCar() && !map->IsPlaceForCar(hx,hy,proto_item)) SCRIPT_ERROR_R0("No place for car.");
	if(!count) count=1;
	return Self->CreateItemOnHex(map,hx,hy,proto_id,count);
}

DWORD FOServer::SScriptFunc::Map_GetItemsHex(Map* map, WORD hx, WORD hy, asIScriptArray* items)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	ItemPtrVec items_;
	map->GetItemsHex(hx,hy,items_);
	if(items_.empty()) return 0;
	if(items) Script::AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

DWORD FOServer::SScriptFunc::Map_GetItemsHexEx(Map* map, WORD hx, WORD hy, DWORD radius, WORD pid, asIScriptArray* items)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	ItemPtrVec items_;
	map->GetItemsHexEx(hx,hy,radius,pid,items_);
	if(items_.empty()) return 0;
	if(items) Script::AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

DWORD FOServer::SScriptFunc::Map_GetItemsByPid(Map* map, WORD pid, asIScriptArray* items)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ItemPtrVec items_;
	map->GetItemsPid(pid,items_);
	if(items_.empty()) return 0;
	if(items) Script::AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

DWORD FOServer::SScriptFunc::Map_GetItemsByType(Map* map, int type, asIScriptArray* items)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ItemPtrVec items_;
	map->GetItemsType(type,items_);
	if(items_.empty()) return 0;
	if(items) Script::AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

Item* FOServer::SScriptFunc::Map_GetItem(Map* map, DWORD item_id)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!item_id) SCRIPT_ERROR_R0("Item id arg is zero.");
	return map->GetItem(item_id);
}

Item* FOServer::SScriptFunc::Map_GetItemHex(Map* map, WORD hx, WORD hy, WORD pid)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	return map->GetItemHex(hx,hy,pid,NULL);
}

Critter* FOServer::SScriptFunc::Map_GetCritterHex(Map* map, WORD hx, WORD hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	Critter* cr=map->GetHexCritter(hx,hy,false);
	if(!cr) cr=map->GetHexCritter(hx,hy,true);
	return cr;
}

Item* FOServer::SScriptFunc::Map_GetDoor(Map* map, WORD hx, WORD hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	return map->GetItemDoor(hx,hy);
}

Item* FOServer::SScriptFunc::Map_GetCar(Map* map, WORD hx, WORD hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	return map->GetItemCar(hx,hy);
}

MapObject* FOServer::SScriptFunc::Map_GetSceneryHex(Map* map, WORD hx, WORD hy, WORD pid)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	return map->Proto->GetMapScenery(hx,hy,pid);
}

DWORD FOServer::SScriptFunc::Map_GetSceneriesHex(Map* map, WORD hx, WORD hy, asIScriptArray* sceneries)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	MapObjectPtrVec mobjs;
	map->Proto->GetMapSceneriesHex(hx,hy,mobjs);
	if(!mobjs.size()) return 0;
	if(sceneries) Script::AppendVectorToArrayRef(mobjs,sceneries);
	return mobjs.size();
}

DWORD FOServer::SScriptFunc::Map_GetSceneriesHexEx(Map* map, WORD hx, WORD hy, DWORD radius, WORD pid, asIScriptArray* sceneries)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	MapObjectPtrVec mobjs;
	map->Proto->GetMapSceneriesHexEx(hx,hy,radius,pid,mobjs);
	if(!mobjs.size()) return 0;
	if(sceneries) Script::AppendVectorToArrayRef(mobjs,sceneries);
	return mobjs.size();
}

DWORD FOServer::SScriptFunc::Map_GetSceneriesByPid(Map* map, WORD pid, asIScriptArray* sceneries)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	MapObjectPtrVec mobjs;
	map->Proto->GetMapSceneriesByPid(pid,mobjs);
	if(!mobjs.size()) return 0;
	if(sceneries) Script::AppendVectorToArrayRef(mobjs,sceneries);
	return mobjs.size();
}

Critter* FOServer::SScriptFunc::Map_GetCritterById(Map* map, DWORD crid)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	Critter* cr=map->GetCritter(crid);
	if(!cr) SCRIPT_ERROR_R0("Critter not found.");
	return cr;
}

DWORD FOServer::SScriptFunc::Map_GetCritters(Map* map, WORD hx, WORD hy, DWORD radius, int find_type, asIScriptArray* critters)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	CrVec cr_vec;
	map->GetCrittersHex(hx,hy,radius,find_type,cr_vec);
	if(critters)
	{
		SortCritterByDist(hx,hy,cr_vec);
		Script::AppendVectorToArrayRef<Critter*>(cr_vec,critters);
	}
	return cr_vec.size();
}

DWORD FOServer::SScriptFunc::Map_GetCrittersByPids(Map* map, WORD pid, int find_type, asIScriptArray* critters)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	CrVec cr_vec;
	if(!pid)
	{
		for(CrVecIt it=map->GetCritters().begin(),end=map->GetCritters().end();it!=end;++it)
		{
			Critter* cr=*it;
			if(cr->CheckFind(find_type)) cr_vec.push_back(cr);
		}
	}
	else
	{
		for(PcVecIt it=map->GetNpcs().begin(),end=map->GetNpcs().end();it!=end;++it)
		{
			Npc* npc=*it;
			if(npc->GetProtoId()==pid && npc->CheckFind(find_type)) cr_vec.push_back(npc);
		}
	}
	if(cr_vec.empty()) return 0;
	if(critters) Script::AppendVectorToArrayRef<Critter*>(cr_vec,critters);
	return cr_vec.size();
}

DWORD FOServer::SScriptFunc::Map_GetCrittersInPath(Map* map, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float angle, DWORD dist, int find_type, asIScriptArray* critters)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	CrVec cr_vec;
	TraceData trace;
	trace.TraceMap=map;
	trace.BeginHx=from_hx;
	trace.BeginHy=from_hy;
	trace.EndHx=to_hx;
	trace.EndHy=to_hy;
	trace.Dist=dist;
	trace.Angle=angle;
	trace.Critters=&cr_vec;
	trace.FindType=find_type;
	MapMngr.TraceBullet(trace);
	if(critters) Script::AppendVectorToArrayRef<Critter*>(cr_vec,critters);
	return cr_vec.size();
}

DWORD FOServer::SScriptFunc::Map_GetCrittersInPathBlock(Map* map, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, float angle, DWORD dist, int find_type, asIScriptArray* critters, WORD& pre_block_hx, WORD& pre_block_hy, WORD& block_hx, WORD& block_hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	CrVec cr_vec;
	WordPair block,pre_block;
	TraceData trace;
	trace.TraceMap=map;
	trace.BeginHx=from_hx;
	trace.BeginHy=from_hy;
	trace.EndHx=to_hx;
	trace.EndHy=to_hy;
	trace.Dist=dist;
	trace.Angle=angle;
	trace.Critters=&cr_vec;
	trace.FindType=find_type;
	trace.PreBlock=&pre_block;
	trace.Block=&block;
	MapMngr.TraceBullet(trace);
	if(critters) Script::AppendVectorToArrayRef<Critter*>(cr_vec,critters);
	pre_block_hx=pre_block.first;
	pre_block_hy=pre_block.second;
	block_hx=block.first;
	block_hy=block.second;
	return cr_vec.size();
}

DWORD FOServer::SScriptFunc::Map_GetCrittersWhoViewPath(Map* map, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, int find_type, asIScriptArray* critters)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	CrVec cr_vec;
	if(critters) Script::AssignScriptArrayInVector<Critter*>(cr_vec,critters);
	CrVec& crits=map->GetCritters();
	for(CrVecIt it=crits.begin(),end=crits.end();it!=end;++it)
	{
		Critter* cr=*it;
		if( cr->CheckFind(find_type)
			&& std::find(cr_vec.begin(),cr_vec.end(),cr)==cr_vec.end()
			&& IntersectCircleLine(cr->GetHexX(),cr->GetHexY(),cr->GetLook(),from_hx,from_hy,to_hx,to_hy))
			cr_vec.push_back(cr);
	}
	if(cr_vec.empty()) return 0;
	if(critters) Script::AppendVectorToArrayRef<Critter*>(cr_vec,critters);
	return cr_vec.size();
}

DWORD FOServer::SScriptFunc::Map_GetCrittersSeeing(Map* map, asIScriptArray& critters, bool look_on_them, int find_type, asIScriptArray* result_critters)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	CrVec cr_vec;
	Script::AssignScriptArrayInVector<Critter*>(cr_vec,&critters);
	for(int i=0,j=critters.GetElementCount();i<j;i++)
	{
		Critter* cr=*(Critter**)critters.GetElementPointer(i);
		if(look_on_them) cr->GetCrFromVisCr(cr_vec,find_type);
		else cr->GetCrFromVisCrSelf(cr_vec,find_type);
	}
	if(result_critters) Script::AppendVectorToArrayRef<Critter*>(cr_vec,result_critters);
	return cr_vec.size();
}

void FOServer::SScriptFunc::Map_GetHexInPath(Map* map, WORD from_hx, WORD from_hy, WORD& to_hx, WORD& to_hy, float angle, DWORD dist)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	WordPair pre_block,block;
	TraceData trace;
	trace.TraceMap=map;
	trace.BeginHx=from_hx;
	trace.BeginHy=from_hy;
	trace.EndHx=to_hx;
	trace.EndHy=to_hy;
	trace.Dist=dist;
	trace.Angle=angle;
	trace.PreBlock=&pre_block;
	trace.Block=&block;
	MapMngr.TraceBullet(trace);
	to_hx=pre_block.first;
	to_hy=pre_block.second;
}

void FOServer::SScriptFunc::Map_GetHexInPathWall(Map* map, WORD from_hx, WORD from_hy, WORD& to_hx, WORD& to_hy, float angle, DWORD dist)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	WordPair last_passed;
	TraceData trace;
	trace.TraceMap=map;
	trace.BeginHx=from_hx;
	trace.BeginHy=from_hy;
	trace.EndHx=to_hx;
	trace.EndHy=to_hy;
	trace.Dist=dist;
	trace.Angle=angle;
	trace.LastPassed=&last_passed;
	MapMngr.TraceBullet(trace);
	if(trace.IsHaveLastPassed)
	{
		to_hx=last_passed.first;
		to_hy=last_passed.second;
	}
	else
	{
		to_hx=from_hx;
		to_hy=from_hy;
	}
}

DWORD FOServer::SScriptFunc::Map_GetPathLengthHex(Map* map, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy, DWORD cut)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(from_hx>=map->GetMaxHexX() || from_hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid from hexes args.");
	if(to_hx>=map->GetMaxHexX() || to_hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid to hexes args.");

	PathFindData pfd;
	pfd.Clear();
	pfd.MapId=map->GetId();
	pfd.FromX=from_hx;
	pfd.FromY=from_hy;
	pfd.ToX=to_hx;
	pfd.ToY=to_hy;
	pfd.Cut=cut;
	DWORD result=MapMngr.FindPath(pfd);
	if(result!=FPATH_OK) return 0;
	PathStepVec& path=MapMngr.GetPath(pfd.PathNum);
	return path.size();
}

DWORD FOServer::SScriptFunc::Map_GetPathLengthCr(Map* map, Critter* cr, WORD to_hx, WORD to_hy, DWORD cut)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R0("Critter arg nullptr.");
	if(to_hx>=map->GetMaxHexX() || to_hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid to hexes args.");

	PathFindData pfd;
	pfd.Clear();
	pfd.MapId=map->GetId();
	pfd.FromCritter=cr;
	pfd.FromX=cr->GetHexX();
	pfd.FromY=cr->GetHexY();
	pfd.ToX=to_hx;
	pfd.ToY=to_hy;
	pfd.Multihex=cr->GetMultihex();
	pfd.Cut=cut;
	DWORD result=MapMngr.FindPath(pfd);
	if(result!=FPATH_OK) return 0;
	PathStepVec& path=MapMngr.GetPath(pfd.PathNum);
	return path.size();
}

Critter* FOServer::SScriptFunc::Map_AddNpc(Map* map, WORD proto_id, WORD hx, WORD hy, BYTE dir, asIScriptArray* params, asIScriptArray* items, CScriptString* script)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(params && params->GetElementCount()&1) SCRIPT_ERROR_R0("Invalid params array size.");
	if(items && items->GetElementCount()%3) SCRIPT_ERROR_R0("Invalid items array size.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	if(!CrMngr.GetProto(proto_id)) SCRIPT_ERROR_R0("Proto not found.");

	IntVec params_;
	if(params)
	{
		Script::AssignScriptArrayInVector(params_,params);
		for(size_t i=0,j=params_.size();i<j;i+=2)
		{
			int index=params_[i];
			if(index<0 || index>=MAX_PARAMS) SCRIPT_ERROR_R0("Invalid params value.");
		}
	}

	IntVec items_;
	if(items)
	{
		Script::AssignScriptArrayInVector(items_,items);
		for(size_t i=0,j=items_.size();i<j;i+=3)
		{
			int pid=items_[i];
			int count=items_[i+1];
			int slot=items_[i+2];
			if(pid && !ItemMngr.IsInitProto(pid)) SCRIPT_ERROR_R0("Invalid item pid value.");
			if(count<0) SCRIPT_ERROR_R0("Invalid items count value.");
			if(slot<0 || slot>=0xFF || !Critter::SlotEnabled[slot]) SCRIPT_ERROR_R0("Invalid items slot value.");
		}
	}

	Critter* npc=CrMngr.CreateNpc(proto_id,
		params_.size()/2,params_.size()?&params_[0]:NULL,items_.size()/3,items_.size()?&items_[0]:NULL,
		script && script->length()?script->c_str():NULL,map,hx,hy,dir,false);
	if(!npc) SCRIPT_ERROR_R0("Create npc fail.");
	return npc;
}

DWORD FOServer::SScriptFunc::Map_GetNpcCount(Map* map, int npc_role, int find_type)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->GetNpcCount(npc_role,find_type);
}

Critter* FOServer::SScriptFunc::Map_GetNpc(Map* map, int npc_role, int find_type, DWORD skip_count)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->GetNpc(npc_role,find_type,skip_count);
}

DWORD FOServer::SScriptFunc::Map_CountEntire(Map* map, int entire_num)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->Proto->CountEntire(entire_num);
}

DWORD FOServer::SScriptFunc::Map_GetEntires(Map* map, int entire, asIScriptArray* entires, asIScriptArray* hx, asIScriptArray* hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ProtoMap::EntiresVec entires_;
	map->Proto->GetEntires(entire,entires_);
	if(entires_.empty()) return 0;
	if(entires)
	{
		size_t size=entires->GetElementCount();
		entires->Resize(size+entires_.size());
		for(size_t i=0,j=entires_.size();i<j;i++) *(int*)entires->GetElementPointer(size+i)=entires_[i].Number;
	}
	if(hx)
	{
		size_t size=hx->GetElementCount();
		hx->Resize(size+entires_.size());
		for(size_t i=0,j=entires_.size();i<j;i++) *(WORD*)hx->GetElementPointer(size+i)=entires_[i].HexX;
	}
	if(hy)
	{
		size_t size=hy->GetElementCount();
		hy->Resize(size+entires_.size());
		for(size_t i=0,j=entires_.size();i<j;i++) *(WORD*)hy->GetElementPointer(size+i)=entires_[i].HexY;
	}
	return entires_.size();
}

bool FOServer::SScriptFunc::Map_GetEntireCoords(Map* map, int entire, DWORD skip, WORD& hx, WORD& hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ProtoMap::MapEntire* e=map->Proto->GetEntire(entire,skip);
	if(!e) return false; //SCRIPT_ERROR_R0("Entire not found.");
	hx=e->HexX;
	hy=e->HexY;
	return true;
}

bool FOServer::SScriptFunc::Map_GetNearEntireCoords(Map* map, int& entire, WORD& hx, WORD& hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ProtoMap::MapEntire* near_entire=map->Proto->GetEntireNear(entire,hx,hy);
	if(near_entire)
	{
		entire=near_entire->Number;
		hx=near_entire->HexX;
		hy=near_entire->HexY;
		return true;
	}
	return false;
}

bool FOServer::SScriptFunc::Map_IsHexPassed(Map* map, WORD hex_x, WORD hex_y)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hex_x>=map->GetMaxHexX() || hex_y>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	return map->IsHexPassed(hex_x,hex_y);
}

bool FOServer::SScriptFunc::Map_IsHexRaked(Map* map, WORD hex_x, WORD hex_y)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(hex_x>=map->GetMaxHexX() || hex_y>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	return map->IsHexRaked(hex_x,hex_y);
}

void FOServer::SScriptFunc::Map_SetText(Map* map, WORD hex_x, WORD hex_y, DWORD color, CScriptString& text)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hex_x>=map->GetMaxHexX() || hex_y>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");
	map->SetText(hex_x,hex_y,color,text.c_str());
}

void FOServer::SScriptFunc::Map_SetTextMsg(Map* map, WORD hex_x, WORD hex_y, DWORD color, WORD text_msg, DWORD str_num)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hex_x>=map->GetMaxHexX() || hex_y>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");
	map->SetTextMsg(hex_x,hex_y,color,text_msg,str_num);
}

void FOServer::SScriptFunc::Map_SetTextMsgLex(Map* map, WORD hex_x, WORD hex_y, DWORD color, WORD text_msg, DWORD str_num, CScriptString& lexems)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hex_x>=map->GetMaxHexX() || hex_y>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");
	map->SetTextMsgLex(hex_x,hex_y,color,text_msg,str_num,lexems.c_str());
}

void FOServer::SScriptFunc::Map_RunEffect(Map* map, WORD eff_pid, WORD hx, WORD hy, DWORD radius)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!eff_pid || eff_pid>=MAX_ITEM_PROTOTYPES) SCRIPT_ERROR_R("Effect pid invalid arg.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");
	map->SendEffect(eff_pid,hx,hy,radius);
}

void FOServer::SScriptFunc::Map_RunFlyEffect(Map* map, WORD eff_pid, Critter* from_cr, Critter* to_cr, WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(!eff_pid || eff_pid>=MAX_ITEM_PROTOTYPES) SCRIPT_ERROR_R("Effect pid invalid arg.");
	if(from_hx>=map->GetMaxHexX() || from_hy>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid from hexes args.");
	if(to_hx>=map->GetMaxHexX() || to_hy>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid to hexes args.");
	if(from_cr && from_cr->IsNotValid) SCRIPT_ERROR_R("From critter nullptr.");
	if(to_cr && to_cr->IsNotValid) SCRIPT_ERROR_R("To critter nullptr.");
	DWORD from_crid=(from_cr?from_cr->GetId():0);
	DWORD to_crid=(to_cr?to_cr->GetId():0);
	map->SendFlyEffect(eff_pid,from_crid,to_crid,from_hx,from_hy,to_hx,to_hy);
}

bool FOServer::SScriptFunc::Map_CheckPlaceForCar(Map* map, WORD hx, WORD hy, WORD pid)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
	if(!proto_item) SCRIPT_ERROR_R0("Proto item not found.");
	if(!proto_item->IsCar()) SCRIPT_ERROR_R0("Proto item is not car.");
	return map->IsPlaceForCar(hx,hy,proto_item);
}

void FOServer::SScriptFunc::Map_BlockHex(Map* map, WORD hx, WORD hy, bool full)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");
	map->SetHexFlag(hx,hy,FH_BLOCK_ITEM);
	if(full) map->SetHexFlag(hx,hy,FH_NRAKE_ITEM);
}

void FOServer::SScriptFunc::Map_UnblockHex(Map* map, WORD hx, WORD hy)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");
	map->UnsetHexFlag(hx,hy,FH_BLOCK_ITEM);
	map->UnsetHexFlag(hx,hy,FH_NRAKE_ITEM);
}

void FOServer::SScriptFunc::Map_PlaySound(Map* map, CScriptString& sound_name)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");

	char sound_name_[16];
	strncpy(sound_name_,sound_name.c_str(),16);

	for(ClVecIt it=map->GetPlayers().begin(),end=map->GetPlayers().end();it!=end;++it)
	{
		Critter* cr=*it;
		cr->Send_PlaySound(0,sound_name_);
	}
}

void FOServer::SScriptFunc::Map_PlaySoundRadius(Map* map, CScriptString& sound_name, WORD hx, WORD hy, DWORD radius)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexes args.");

	char sound_name_[16];
	strncpy(sound_name_,sound_name.c_str(),16);

	for(ClVecIt it=map->GetPlayers().begin(),end=map->GetPlayers().end();it!=end;++it)
	{
		Critter* cr=*it;
		if(CheckDist(hx,hy,cr->GetHexX(),cr->GetHexY(),radius==0?cr->GetLook():radius)) cr->Send_PlaySound(0,sound_name_);
	}
}

bool FOServer::SScriptFunc::Map_Reload(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(!RegenerateMap(map)) SCRIPT_ERROR_R0("Reload map fail.");
	return true;
}

WORD FOServer::SScriptFunc::Map_GetWidth(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->GetMaxHexX();
}

WORD FOServer::SScriptFunc::Map_GetHeight(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return map->GetMaxHexY();
}

void FOServer::SScriptFunc::Map_MoveHexByDir(Map* map, WORD& hx, WORD& hy, BYTE dir, DWORD steps)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(dir>5) SCRIPT_ERROR_R("Invalid dir arg.");
	if(!steps) SCRIPT_ERROR_R("Steps arg is zero.");
	WORD maxhx=map->GetMaxHexX();
	WORD maxhy=map->GetMaxHexY();
	if(steps>1)
	{
		for(int i=0;i<steps;i++) MoveHexByDir(hx,hy,dir,maxhx,maxhy);
	}
	else
	{
		MoveHexByDir(hx,hy,dir,maxhx,maxhy);
	}
}

bool FOServer::SScriptFunc::Map_VerifyTrigger(Map* map, Critter* cr, WORD hx, WORD hy, BYTE dir)
{
	if(map->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R0("Critter arg nullptr.");
	if(hx>=map->GetMaxHexX() || hy>=map->GetMaxHexY()) SCRIPT_ERROR_R0("Invalid hexes args.");
	if(dir>5) SCRIPT_ERROR_R0("Invalid dir arg.");
	WORD from_hx=hx,from_hy=hy;
	MoveHexByDir(from_hx,from_hy,ReverseDir(dir),map->GetMaxHexX(),map->GetMaxHexY());
	return Self->VerifyTrigger(map,cr,from_hx,from_hy,hx,hy,dir);
}

void FOServer::SScriptFunc::Map_EventFinish(Map* map, bool deleted)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->EventFinish(deleted);
}

void FOServer::SScriptFunc::Map_EventLoop0(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->EventLoop(0);
}

void FOServer::SScriptFunc::Map_EventLoop1(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->EventLoop(1);
}

void FOServer::SScriptFunc::Map_EventLoop2(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->EventLoop(2);
}

void FOServer::SScriptFunc::Map_EventLoop3(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->EventLoop(3);
}

void FOServer::SScriptFunc::Map_EventLoop4(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->EventLoop(4);
}

void FOServer::SScriptFunc::Map_EventInCritter(Map* map, Critter* cr)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	map->EventInCritter(cr);
}

void FOServer::SScriptFunc::Map_EventOutCritter(Map* map, Critter* cr)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	map->EventOutCritter(cr);
}

void FOServer::SScriptFunc::Map_EventCritterDead(Map* map, Critter* cr, Critter* killer)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	if(killer && killer->IsNotValid) SCRIPT_ERROR_R("Killer arg nullptr.");
	map->EventCritterDead(cr,killer);
}

void FOServer::SScriptFunc::Map_EventTurnBasedBegin(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->EventTurnBasedBegin();
}

void FOServer::SScriptFunc::Map_EventTurnBasedEnd(Map* map)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	map->EventTurnBasedEnd();
}

void FOServer::SScriptFunc::Map_EventTurnBasedProcess(Map* map, Critter* cr, bool begin_turn)
{
	if(map->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	if(cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	map->EventTurnBasedProcess(cr,begin_turn);
}

DWORD FOServer::SScriptFunc::Location_GetId(Location* loc)
{
	if(loc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return loc->GetId();
}

WORD FOServer::SScriptFunc::Location_GetProtoId(Location* loc)
{
	if(loc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return loc->GetPid();
}

DWORD FOServer::SScriptFunc::Location_GetMapCount(Location* loc)
{
	if(loc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	return loc->GetMaps().size();
}

Map* FOServer::SScriptFunc::Location_GetMap(Location* loc, WORD map_pid)
{
	if(loc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	MapVec& maps=loc->GetMaps();
	for(MapVecIt it=maps.begin(),end=maps.end();it!=end;++it)
	{
		Map* map=*it;
		if(map->GetPid()==map_pid) return map;
	}
	return NULL;
}

Map* FOServer::SScriptFunc::Location_GetMapByIndex(Location* loc, DWORD index)
{
	if(loc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	MapVec& maps=loc->GetMaps();
	if(index>=maps.size()) SCRIPT_ERROR_R0("Invalid index arg.");
	return maps[index];
}

DWORD FOServer::SScriptFunc::Location_GetMaps(Location* loc, asIScriptArray* arr)
{
	if(loc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	MapVec& maps=loc->GetMaps();
	Script::AppendVectorToArrayRef<Map*>(maps,arr);
	return maps.size();
}

bool FOServer::SScriptFunc::Location_Reload(Location* loc)
{
	if(loc->IsNotValid) SCRIPT_ERROR_R0("This nullptr.");
	MapVec& maps=loc->GetMaps();
	for(MapVecIt it=maps.begin(),end=maps.end();it!=end;++it)
	{
		if(!RegenerateMap(*it)) SCRIPT_ERROR_R0("Reload map in location fail.");
	}
	return true;
}

void FOServer::SScriptFunc::Location_Update(Location* loc)
{
	if(loc->IsNotValid) SCRIPT_ERROR_R("This nullptr.");
	loc->Update();
}

DWORD FOServer::SScriptFunc::Global_GetCrittersDistantion(Critter* cr1, Critter* cr2)
{
	if(cr1->IsNotValid) SCRIPT_ERROR_RX("Critter1 arg nullptr.",-1);
	if(cr2->IsNotValid) SCRIPT_ERROR_RX("Critter2 arg nullptr.",-1);
	if(cr1->GetMap()!=cr2->GetMap()) SCRIPT_ERROR_RX("Differernt maps.",-1);
	return DistGame(cr1->GetHexX(),cr1->GetHexY(),cr2->GetHexX(),cr2->GetHexY());
}

DWORD FOServer::SScriptFunc::Global_GetDistantion(WORD hx1, WORD hy1, WORD hx2, WORD hy2)
{
	return DistGame(hx1,hy1,hx2,hy2);
}

BYTE FOServer::SScriptFunc::Global_GetDirection(WORD from_hx, WORD from_hy, WORD to_hx, WORD to_hy)
{
	return GetFarDir(from_hx,from_hy,to_hx,to_hy);
}

BYTE FOServer::SScriptFunc::Global_GetOffsetDir(WORD hx, WORD hy, WORD tx, WORD ty, float offset)
{
	float nx=3.0f*(float(tx)-float(hx));
	float ny=SQRT3T2_FLOAT*(float(ty)-float(hy))-SQRT3_FLOAT*(float(tx&1)-float(hx&1));
	float dir=180.0f+RAD2DEG*atan2(ny,nx);
	dir+=offset;
	if(dir>360.0f) dir-=360.0f;
	else if(dir<0.0f) dir+=360.0f;
	if(dir>=60.0f  && dir<120.0f) return 5;
	if(dir>=120.0f && dir<180.0f) return 4;
	if(dir>=180.0f && dir<240.0f) return 3;
	if(dir>=240.0f && dir<300.0f) return 2;
	if(dir>=300.0f && dir<360.0f) return 1;
	return 0;
}

void FOServer::SScriptFunc::Global_Log(CScriptString& text)
{
	Script::Log(text.c_str());
}

ProtoItem* FOServer::SScriptFunc::Global_GetProtoItem(WORD pid)
{
	ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
	//if(!proto_item) SCRIPT_ERROR_R0("Proto item not found.");
	return proto_item;
}

Item* FOServer::SScriptFunc::Global_GetItem(DWORD item_id)
{
	if(!item_id) SCRIPT_ERROR_R0("Item id arg is zero.");
	Item* item=ItemMngr.GetItem(item_id);
	if(!item || item->IsNotValid) return NULL;
	return item;
}

void FOServer::SScriptFunc::Global_MoveItemCr(Item* item, DWORD count, Critter* to_cr)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	if(to_cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	if(!count) count=item->GetCount();
	if(count>item->GetCount()) SCRIPT_ERROR_R("Count arg is greather than maximum.");
	ItemMngr.MoveItem(item,count,to_cr);
}

void FOServer::SScriptFunc::Global_MoveItemMap(Item* item, DWORD count, Map* to_map, WORD to_hx, WORD to_hy)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	if(to_map->IsNotValid) SCRIPT_ERROR_R("Container arg nullptr.");
	if(to_hx>=to_map->GetMaxHexX() || to_hy>=to_map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexex args.");
	if(!count) count=item->GetCount();
	if(count>item->GetCount()) SCRIPT_ERROR_R("Count arg is greather than maximum.");
	ItemMngr.MoveItem(item,count,to_map,to_hx,to_hy);
}

void FOServer::SScriptFunc::Global_MoveItemCont(Item* item, DWORD count, Item* to_cont, DWORD stack_id)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	if(to_cont->IsNotValid) SCRIPT_ERROR_R("Container arg nullptr.");
	if(!to_cont->IsContainer()) SCRIPT_ERROR_R("Container arg is not container type.");
	if(!count) count=item->GetCount();
	if(count>item->GetCount()) SCRIPT_ERROR_R("Count arg is greather than maximum.");
	ItemMngr.MoveItem(item,count,to_cont,stack_id);
}

void FOServer::SScriptFunc::Global_MoveItemsCr(asIScriptArray& items, Critter* to_cr)
{
	if(to_cr->IsNotValid) SCRIPT_ERROR_R("Critter arg nullptr.");
	for(int i=0,j=items.GetElementCount();i<j;i++)
	{
		Item* item=*(Item**)items.GetElementPointer(i);
		if(!item || item->IsNotValid) continue;
		ItemMngr.MoveItem(item,item->GetCount(),to_cr);
	}
}

void FOServer::SScriptFunc::Global_MoveItemsMap(asIScriptArray& items, Map* to_map, WORD to_hx, WORD to_hy)
{
	if(to_map->IsNotValid) SCRIPT_ERROR_R("Container arg nullptr.");
	if(to_hx>=to_map->GetMaxHexX() || to_hy>=to_map->GetMaxHexY()) SCRIPT_ERROR_R("Invalid hexex args.");
	for(int i=0,j=items.GetElementCount();i<j;i++)
	{
		Item* item=*(Item**)items.GetElementPointer(i);
		if(!item || item->IsNotValid) continue;
		ItemMngr.MoveItem(item,item->GetCount(),to_map,to_hx,to_hy);
	}
}

void FOServer::SScriptFunc::Global_MoveItemsCont(asIScriptArray& items, Item* to_cont, DWORD stack_id)
{
	if(to_cont->IsNotValid) SCRIPT_ERROR_R("Container arg nullptr.");
	if(!to_cont->IsContainer()) SCRIPT_ERROR_R("Container arg is not container type.");
	for(int i=0,j=items.GetElementCount();i<j;i++)
	{
		Item* item=*(Item**)items.GetElementPointer(i);
		if(!item || item->IsNotValid) continue;
		ItemMngr.MoveItem(item,item->GetCount(),to_cont,stack_id);
	}
}

void FOServer::SScriptFunc::Global_DeleteItem(Item* item)
{
	if(item->IsNotValid) SCRIPT_ERROR_R("Item arg nullptr.");
	// Delete car bags
	if(item->IsCar() && item->Accessory==ITEM_ACCESSORY_HEX)
	{
		Map* map=MapMngr.GetMap(item->ACC_HEX.MapId);
		if(map)
		{
			for(int i=0;i<CAR_MAX_BAGS;i++)
			{
				Item* bag=map->GetCarBag(item->ACC_HEX.HexX,item->ACC_HEX.HexY,item->Proto,i);
				if(bag && !bag->IsNotValid) ItemMngr.DeleteItem(bag);
			}
		}
	}
	// Delete item
	ItemMngr.DeleteItem(item);
}

void FOServer::SScriptFunc::Global_DeleteItems(asIScriptArray& items)
{
	for(int i=0,j=items.GetElementCount();i<j;i++)
	{
		Item* item=*(Item**)items.GetElementPointer(i);
		if(item && !item->IsNotValid) ItemMngr.DeleteItem(item);
	}
}

void FOServer::SScriptFunc::Global_DeleteNpc(Critter* npc)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R("Npc arg nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R("Critter is not npc.");
	CrMngr.CritterToGarbage(npc);
	npc->IsNotValid=false;
}

void FOServer::SScriptFunc::Global_DeleteNpcForce(Critter* npc)
{
	if(npc->IsNotValid) SCRIPT_ERROR_R("Npc arg nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R("Critter is not npc.");
	CrMngr.CritterToGarbage(npc);
}

void FOServer::SScriptFunc::Global_RadioMessage(WORD channel, CScriptString& text)
{
	Self->RadioSendText(NULL,channel,text.c_str(),false);
}

void FOServer::SScriptFunc::Global_RadioMessageMsg(WORD channel, WORD text_msg, DWORD num_str)
{
	Self->RadioSendMsg(NULL,channel,text_msg,num_str);
}

DWORD FOServer::SScriptFunc::Global_GetFullSecond(WORD year, WORD month, WORD day, WORD hour, WORD minute, WORD second)
{
	if(!year) year=GameOpt.Year;
	else year=CLAMP(year,GameOpt.YearStart,GameOpt.YearStart+130);
	if(!month) month=GameOpt.Month;
	else month=CLAMP(month,1,12);
	if(!day) day=GameOpt.Day;
	else
	{
		DWORD month_day=GameTimeMonthDay(year,month);
		day=CLAMP(day,1,month_day);
	}
	hour=CLAMP(hour,0,23);
	minute=CLAMP(minute,0,59);
	second=CLAMP(second,0,59);
	return GetFullSecond(year,month,day,hour,minute,second);
}

void FOServer::SScriptFunc::Global_GetGameTime(DWORD full_second, WORD& year, WORD& month, WORD& day, WORD& day_of_week, WORD& hour, WORD& minute, WORD& second)
{
	SYSTEMTIME st=GetGameTime(full_second);
	year=st.wYear;
	month=st.wMonth;
	day_of_week=st.wDayOfWeek;
	day=st.wDay;
	hour=st.wHour;
	minute=st.wMinute;
	second=st.wSecond;
}

DWORD FOServer::SScriptFunc::Global_CreateLocation(WORD loc_pid, WORD wx, WORD wy, asIScriptArray* critters)
{
	if(!MapMngr.IsInitProtoLocation(loc_pid)) SCRIPT_ERROR_R0("Proto location is not init.");

	// Create and generate location
	Location* loc=MapMngr.CreateLocation(loc_pid,wx,wy,0);
	if(!loc)
	{
		WriteLog(__FUNCTION__" - Unable to create location, pid<%u>.\n",loc_pid);
		SCRIPT_ERROR_R0("Unable to create location.");
	}

	// Add Known locations to critters
	if(!critters) return loc->GetId();
	for(DWORD i=0,j=critters->GetElementCount();i<j;i++)
	{
		Critter* cr=*(Critter**)critters->GetElementPointer(i);
		if(cr->IsPlayer())
		{
			Client* cl=(Client*)cr;
			cl->AddKnownLoc(loc->GetId());
			if(!cl->GetMap()) cl->Send_GlobalLocation(loc,true);
			if(loc->IsAutomaps()) cl->Send_AutomapsInfo(NULL,loc);

			WORD zx=GM_ZONE(loc->Data.WX);
			WORD zy=GM_ZONE(loc->Data.WY);
			if(cl->GMapFog.Get2Bit(zx,zy)==GM_FOG_FULL)
			{
				cl->GMapFog.Set2Bit(zx,zy,GM_FOG_SELF);
				if(!cl->GetMap()) cl->Send_GlobalMapFog(zx,zy,GM_FOG_SELF);
			}
		}
	}
	return loc->GetId();
}

void FOServer::SScriptFunc::Global_DeleteLocation(DWORD loc_id)
{
	Location* loc=MapMngr.GetLocation(loc_id);
	if(!loc) return;

	/*MapVec& maps=loc->GetMaps();
	for(MapVecIt it=maps.begin(),end=maps.end();it!=end;++it)
	{
		Map* map=*it;
		for(ClMapIt it_=crits.begin(),end_=crits.end();it_!=end_;++it_)
		{
			Critter* cr=(*it_).second;
			Self->TransitToGlobal(cr,0,FOLLOW_FORCE,true);
		}
	}*/

	loc->Data.ToGarbage=true;
	MapMngr.RunLocGarbager();
}

void FOServer::SScriptFunc::Global_GetProtoCritter(WORD proto_id, asIScriptArray& data)
{
	CritData* data_=CrMngr.GetProto(proto_id);
	if(!data_) SCRIPT_ERROR_R("Proto critter not found.");
	IntVec data__;
	data__.resize(MAX_PARAMS);
	memcpy(&data__[0],data_->Params,sizeof(data_->Params));
	Script::AppendVectorToArray(data__,&data);
}

Critter* FOServer::SScriptFunc::Global_GetCritter(DWORD crid)
{
	if(!crid) return NULL;//SCRIPT_ERROR_R0("Critter id arg is zero.");
	Critter* cr=CrMngr.GetCritter(crid);
	if(!cr || cr->IsNotValid) return NULL;
	return cr;
}

Critter* FOServer::SScriptFunc::Global_GetPlayer(CScriptString& name)
{
	DWORD len=name.length();
	if(len<MIN_NAME || len<GameOpt.MinNameLength) return NULL;
	if(len>MAX_NAME || len>GameOpt.MaxNameLength) return NULL;
	return CrMngr.GetPlayer(name.c_str());
}

DWORD FOServer::SScriptFunc::Global_GetPlayerId(CScriptString& name)
{
	DWORD len=name.length();
	if(len<MIN_NAME || len<GameOpt.MinNameLength) SCRIPT_ERROR_R0("Name length is less than minimum.");
	if(len>MAX_NAME || len>GameOpt.MaxNameLength) SCRIPT_ERROR_R0("Name length is greater than maximum.");
	ClientData* data=Self->GetClientData(name.c_str());
	if(!data) SCRIPT_ERROR_R0("Player not found.");
	return data->ClientId;
}

CScriptString* FOServer::SScriptFunc::Global_GetPlayerName(DWORD id)
{
	if(!id) SCRIPT_ERROR_RX("Id arg is zero.",new CScriptString(""));
	ClientData* data=Self->GetClientData(id);
	if(!data) SCRIPT_ERROR_RX("Player not found.",new CScriptString(""));
	return new CScriptString(data->ClientName);
}

DWORD FOServer::SScriptFunc::Global_CreateTimeEventEmpty(DWORD begin_second, CScriptString& script_name, bool save)
{
	DwordVec values;
	return Self->CreateScriptEvent(begin_second,script_name.c_str(),values,save);
}

DWORD FOServer::SScriptFunc::Global_CreateTimeEventDw(DWORD begin_second, CScriptString& script_name, DWORD dw, bool save)
{
	DwordVec values;
	values.push_back(dw);
	return Self->CreateScriptEvent(begin_second,script_name.c_str(),values,save);
}

DWORD FOServer::SScriptFunc::Global_CreateTimeEventDws(DWORD begin_second, CScriptString& script_name, asIScriptArray& dw, bool save)
{
	DwordVec values;
	DWORD dw_size=dw.GetElementCount();
	values.resize(dw_size);
	if(dw_size) memcpy((void*)&values[0],dw.GetElementPointer(0),dw_size*dw.GetElementSize());
	return Self->CreateScriptEvent(begin_second,script_name.c_str(),values,save);
}

DWORD FOServer::SScriptFunc::Global_CreateTimeEventCr(DWORD begin_second, CScriptString& script_name, Critter* cr, bool save)
{
	if(cr->IsNotValid) SCRIPT_ERROR_R0("Critter arg nullptr.");
	DwordVec values;
	values.push_back(cr->GetId());
	return Self->CreateScriptEvent(begin_second,script_name.c_str(),values,save);
}

DWORD FOServer::SScriptFunc::Global_CreateTimeEventItem(DWORD begin_second, CScriptString& script_name, Item* item, bool save)
{
	if(item->IsNotValid) SCRIPT_ERROR_R0("Item arg nullptr.");
	DwordVec values;
	values.push_back(item->GetId());
	return Self->CreateScriptEvent(begin_second,script_name.c_str(),values,save);
}

DWORD FOServer::SScriptFunc::Global_CreateTimeEventArr(DWORD begin_second, CScriptString& script_name, asIScriptArray* critters, asIScriptArray* items, bool save)
{
	DwordVec values;
	DWORD cr_size=(critters?critters->GetElementCount():0);
	DWORD items_size=(items?items->GetElementCount():0);
	values.resize(cr_size+items_size+2);
	if(cr_size) memcpy((void*)&values[0],critters->GetElementPointer(0),cr_size);
	if(items_size) memcpy((void*)&values[cr_size],items->GetElementPointer(0),items_size);
	values[values.size()-2]=cr_size;
	values[values.size()-1]=items_size;
	return Self->CreateScriptEvent(begin_second,script_name.c_str(),values,save);
}

void FOServer::SScriptFunc::Global_EraseTimeEvent(DWORD num)
{
	Self->EraseTimeEvent(num);
}

bool FOServer::SScriptFunc::Global_SetAnyData(CScriptString& name, asIScriptArray& data)
{
	if(!name.length()) SCRIPT_ERROR_R0("Name arg length is zero.");
	DWORD data_size_bytes=data.GetElementCount()*data.GetElementSize();
	return Self->SetAnyData(name.c_std_str(),(BYTE*)data.GetElementPointer(0),data_size_bytes);
}

bool FOServer::SScriptFunc::Global_SetAnyDataSize(CScriptString& name, asIScriptArray& data, DWORD data_size)
{
	if(!name.length()) SCRIPT_ERROR_R0("Name arg length is zero.");
	DWORD element_size=data.GetElementSize();
	DWORD data_size_bytes=data_size*element_size;
	DWORD real_data_size_bytes=data.GetElementCount()*element_size;
	if(real_data_size_bytes<data_size_bytes) SCRIPT_ERROR_R0("Array length is less than data size in bytes arg.");
	return Self->SetAnyData(name.c_std_str(),(BYTE*)data.GetElementPointer(0),data_size_bytes);
}

bool FOServer::SScriptFunc::Global_GetAnyData(CScriptString& name, asIScriptArray& data)
{
	if(!name.length()) SCRIPT_ERROR_R0("Name arg length is zero.");
	DWORD length;
	BYTE* buf=Self->GetAnyData(name.c_std_str(),length);
	if(!buf) return false; // SCRIPT_ERROR_R0("Data not found.");
	if(!length)
	{
		data.Resize(0);
		return true;
	}
	DWORD element_size=data.GetElementSize();
	data.Resize(length/element_size+((length%element_size)?1:0));
	CopyMemory(data.GetElementPointer(0),buf,length);
	return true;
}

bool FOServer::SScriptFunc::Global_IsAnyData(CScriptString& name)
{
	if(!name.length()) SCRIPT_ERROR_R0("Name arg length is zero.");
	return Self->IsAnyData(name.c_std_str());
}

void FOServer::SScriptFunc::Global_EraseAnyData(CScriptString& name)
{
	if(!name.length()) SCRIPT_ERROR_R("Name arg length is zero.");
	Self->EraseAnyData(name.c_std_str());
}

void FOServer::SScriptFunc::Global_ArrayPushBackInteger(asIScriptArray* arr, void* value)
{
	DWORD count=arr->GetElementCount();
	DWORD element_size=arr->GetElementSize();
	arr->Resize(count+1);
	CopyMemory(arr->GetElementPointer(count),value,element_size);
}

void FOServer::SScriptFunc::Global_ArrayPushBackCritter(asIScriptArray* arr, Critter* cr)
{
	DWORD count=arr->GetElementCount();
	arr->Resize(count+1);
	void* p=arr->GetElementPointer(count);
	*(Critter**)p=cr;
	if(cr) cr->AddRef();
}

/*void FOServer::SScriptFunc::Global_ArrayErase(asIScriptArray* arr, DWORD index)
{
	DWORD count=arr->GetElementCount();
	if(index>=count) return;
	if(index<count-1)
	{
		int esize=4;
		int copy_cnt=count-index-1;
		BYTE* data=(BYTE*)arr->GetElementPointer(index);
		for(int i=0;i<copy_cnt;i++)
		{
			memcpy(data,data+esize,esize);
			data+=esize;
		}
	}
	arr->Resize(count-1);
}*/

Map* FOServer::SScriptFunc::Global_GetMap(DWORD map_id)
{
	if(!map_id) SCRIPT_ERROR_R0("Map id arg is zero.");
	return MapMngr.GetMap(map_id);
}

Map* FOServer::SScriptFunc::Global_GetMapByPid(WORD map_pid, DWORD skip_count)
{
	if(!map_pid || map_pid>=MAX_PROTO_MAPS) SCRIPT_ERROR_R0("Invalid map proto id arg.");
	return MapMngr.GetMapByPid(map_pid,skip_count);
}

Location* FOServer::SScriptFunc::Global_GetLocation(DWORD loc_id)
{
	if(!loc_id) SCRIPT_ERROR_R0("Location id arg is zero.");
	return MapMngr.GetLocation(loc_id);
}

Location* FOServer::SScriptFunc::Global_GetLocationByPid(WORD loc_pid, DWORD skip_count)
{
	if(!loc_pid || loc_pid>=MAX_PROTO_LOCATIONS) SCRIPT_ERROR_R0("Invalid location proto id arg.");
	return MapMngr.GetLocationByPid(loc_pid,skip_count);
}

DWORD FOServer::SScriptFunc::Global_GetLocations(WORD wx, WORD wy, DWORD radius, asIScriptArray* locations)
{
	LocVec locs_;
	LocMap& locs=MapMngr.GetLocations();
	for(LocMapIt it=locs.begin(),end=locs.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		if(DistSqrt(wx,wy,loc->Data.WX,loc->Data.WY)<=radius+loc->Data.Radius) locs_.push_back(loc);
	}
	if(locations) Script::AppendVectorToArrayRef<Location*>(locs_,locations);
	return locs_.size();
}

bool FOServer::SScriptFunc::Global_StrToInt(CScriptString& text, int& result)
{
	if(!text.length()) return false;
	return Str::StrToInt(text.c_str(),result);
}

bool FOServer::SScriptFunc::Global_RunDialogNpc(Critter* player, Critter* npc, bool ignore_distance)
{
	if(player->IsNotValid) SCRIPT_ERROR_R0("Player arg nullptr.");
	if(!player->IsPlayer()) SCRIPT_ERROR_R0("Player arg is not player.");
	if(npc->IsNotValid) SCRIPT_ERROR_R0("Npc arg nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Npc arg is not npc.");
	Client* cl=(Client*)player;
	if(cl->Talk.Locked) SCRIPT_ERROR_R0("Can't open new dialog from demand, result or dialog functions."); //if(DlgCtx->GetState()==asEXECUTION_ACTIVE)
	Self->Dialog_Begin(cl,(Npc*)npc,0,0,0,ignore_distance);
	return cl->Talk.TalkType==TALK_WITH_NPC && cl->Talk.TalkNpc==npc->GetId();
}

bool FOServer::SScriptFunc::Global_RunDialogNpcDlgPack(Critter* player, Critter* npc, DWORD dlg_pack, bool ignore_distance)
{
	if(player->IsNotValid) SCRIPT_ERROR_R0("Player arg nullptr.");
	if(!player->IsPlayer()) SCRIPT_ERROR_R0("Player arg is not player.");
	if(npc->IsNotValid) SCRIPT_ERROR_R0("Npc arg nullptr.");
	if(!npc->IsNpc()) SCRIPT_ERROR_R0("Npc arg is not npc.");
	Client* cl=(Client*)player;
	if(cl->Talk.Locked) SCRIPT_ERROR_R0("Can't open new dialog from demand, result or dialog functions."); //if(DlgCtx->GetState()==asEXECUTION_ACTIVE)
	Self->Dialog_Begin(cl,(Npc*)npc,dlg_pack,0,0,ignore_distance);
	return cl->Talk.TalkType==TALK_WITH_NPC && cl->Talk.TalkNpc==npc->GetId();
}

bool FOServer::SScriptFunc::Global_RunDialogHex(Critter* player, DWORD dlg_pack, WORD hx, WORD hy, bool ignore_distance)
{
	if(player->IsNotValid) SCRIPT_ERROR_R0("Player arg nullptr.");
	if(!player->IsPlayer()) SCRIPT_ERROR_R0("Player arg is not player.");
	if(!DlgMngr.GetDialogPack(dlg_pack)) SCRIPT_ERROR_R0("Dialog not found.");
	Client* cl=(Client*)player;
	if(cl->Talk.Locked) SCRIPT_ERROR_R0("Can't open new dialog from demand, result or dialog functions."); //if(DlgCtx->GetState()==asEXECUTION_ACTIVE)
	Self->Dialog_Begin(cl,NULL,dlg_pack,hx,hy,ignore_distance);
	return cl->Talk.TalkType==TALK_WITH_HEX && cl->Talk.TalkHexX==hx && cl->Talk.TalkHexY==hy;
}

__int64 FOServer::SScriptFunc::Global_WorldItemCount(WORD pid)
{
	if(!ItemMngr.IsInitProto(pid)) SCRIPT_ERROR_R0("Invalid protoId arg.");
	return ItemMngr.GetItemStatistics(pid);
}

void FOServer::SScriptFunc::Global_SetBestScore(int score, Critter* cl, CScriptString& name)
{
	if(score<0) SCRIPT_ERROR_R("Score arg is negative.");
	if(score>=SCORES_MAX) SCRIPT_ERROR_R("Score arg is greater than max scores.");
	if(cl && cl->IsNotValid) SCRIPT_ERROR_R("Player arg nullptr.");
	if(cl && !cl->IsPlayer()) return; //SCRIPT_ERROR_R("Critter arg is not player.");
	if(cl) name=cl->GetName();
	if(!name.length() || name.length()>=SCORE_NAME_LEN-1) SCRIPT_ERROR_R("Invalid length of name.");
	Self->SetScore(score,name.c_str());
}

bool FOServer::SScriptFunc::Global_AddTextListener(int say_type, CScriptString& first_str, WORD parameter, CScriptString& script_name)
{
	if(first_str.length()<TEXT_LISTEN_FIRST_STR_MIN_LEN) SCRIPT_ERROR_R0("First string arg length less than minimum.");
	if(first_str.length()>=TEXT_LISTEN_FIRST_STR_MAX_LEN) SCRIPT_ERROR_R0("First string arg length greater than maximum.");

	int func_id=Script::Bind(script_name.c_str(),"void %s(Critter&,string&)",false);
	if(func_id<=0) SCRIPT_ERROR_R0("Unable to bind script function.");

	TextListen tl;
	tl.FuncId=func_id;
	tl.SayType=say_type;
	StringCopy(tl.FirstStr,first_str.c_str());
	tl.FirstStrLen=strlen(tl.FirstStr);
	tl.Parameter=parameter;
	Self->TextListeners.push_back(tl);
	return true;
}

void FOServer::SScriptFunc::Global_EraseTextListener(int say_type, CScriptString& first_str, WORD parameter)
{
	for(TextListenVecIt it=Self->TextListeners.begin(),end=Self->TextListeners.end();it!=end;++it)
	{
		TextListen& tl=*it;
		if(say_type==tl.SayType && !_stricmp(first_str.c_str(),tl.FirstStr) && tl.Parameter==parameter)
		{
			Self->TextListeners.erase(it);
			return;
		}
	}
}

AIDataPlane* FOServer::SScriptFunc::Global_CreatePlane()
{
	return new AIDataPlane(0,0);
}

DWORD FOServer::SScriptFunc::Global_GetBagItems(DWORD bag_id, asIScriptArray* pids, asIScriptArray* min_counts, asIScriptArray* max_counts, asIScriptArray* slots)
{
	NpcBag& bag=AIMngr.GetBag(bag_id);
	if(bag.empty()) return 0;

	DwordVec bag_data;
	bag_data.reserve(100);
	for(int i=0,j=bag.size();i<j;i++)
	{
		NpcBagCombination& bc=bag[i];
		for(int k=0,l=bc.size();k<l;k++)
		{
			NpcBagItems& bci=bc[k];
			for(int m=0,n=bci.size();m<n;m++)
			{
				NpcBagItem& item=bci[m];
				bag_data.push_back(item.ItemPid);
				bag_data.push_back(item.MinCnt);
				bag_data.push_back(item.MaxCnt);
				bag_data.push_back(item.ItemSlot);
			}
		}
	}

	DWORD count=bag_data.size()/4;
	if(pids || min_counts || max_counts || slots)
	{
		if(pids) pids->Resize(pids->GetElementCount()+count);
		if(min_counts) min_counts->Resize(min_counts->GetElementCount()+count);
		if(max_counts) max_counts->Resize(max_counts->GetElementCount()+count);
		if(slots) slots->Resize(slots->GetElementCount()+count);
		for(int i=0;i<count;i++)
		{
			if(pids) *(WORD*)pids->GetElementPointer(pids->GetElementCount()-count+i)=bag_data[i*4];
			if(min_counts) *(DWORD*)min_counts->GetElementPointer(min_counts->GetElementCount()-count+i)=bag_data[i*4+1];
			if(max_counts) *(DWORD*)max_counts->GetElementPointer(max_counts->GetElementCount()-count+i)=bag_data[i*4+2];
			if(slots) *(int*)slots->GetElementPointer(slots->GetElementCount()-count+i)=bag_data[i*4+3];
		}
	}
	return count;
}

void FOServer::SScriptFunc::Global_SetSendParameter(int index, bool enabled)
{
	Global_SetSendParameterFunc(index,enabled,NULL);
}

void FOServer::SScriptFunc::Global_SetSendParameterFunc(int index, bool enabled, CScriptString* allow_func)
{
	if(index<0)
	{
		index=-index;
		if(index>=SLOT_GROUND) SCRIPT_ERROR_R("Invalid index arg.");

		if(allow_func && allow_func->length())
		{
			int bind_id=Script::Bind(allow_func->c_str(),"bool %s(uint,Critter&,Critter&)",false);
			if(bind_id<=0) SCRIPT_ERROR_R("Function not found.");
			Critter::SlotDataSendScript[index]=bind_id;
		}
		else
		{
			Critter::SlotDataSendScript[index]=0;
		}

		Critter::SlotDataSendEnabled[index]=enabled;
		return;
	}

	if(index>=MAX_PARAMS)
	{
		SCRIPT_ERROR_R("Invalid index arg.");
	}

	if(allow_func && allow_func->length())
	{
		int bind_id=Script::Bind(allow_func->c_str(),"int %s(uint,Critter&,Critter&)",false);
		if(bind_id<=0) SCRIPT_ERROR_R("Function not found.");
		Critter::ParamsSendScript[index]=bind_id;
	}
	else
	{
		Critter::ParamsSendScript[index]=0;
	}

	WordVec& vec=Critter::ParamsSend;
	WORD& count=Critter::ParamsSendCount;
	WordVecIt it=std::find(vec.begin(),vec.end(),index);

	if(enabled)
	{
		if(it!=vec.end()) SCRIPT_ERROR_R("Index already enabled.");
		vec.push_back(index);
		std::sort(vec.begin(),vec.end());
		Critter::ParamsSendMsgLen+=sizeof(WORD)+sizeof(int);
	}
	else
	{
		if(it==vec.end()) SCRIPT_ERROR_R("Index already disabled.");
		vec.erase(it);
		Critter::ParamsSendMsgLen-=sizeof(WORD)+sizeof(int);
	}

	count=vec.size();
	Critter::ParamsSendEnabled[index]=enabled;
}

template<typename Ty>
void SwapArray(Ty& arr1, Ty& arr2)
{
	Ty tmp;
	memcpy(tmp,arr1,sizeof(tmp));
	memcpy(arr1,arr2,sizeof(tmp));
	memcpy(arr2,tmp,sizeof(tmp));
}

void SwapCrittersRefreshNpc(Npc* npc)
{
	UNSETFLAG(npc->Flags,FCRIT_PLAYER);
	SETFLAG(npc->Flags,FCRIT_NPC);
	AIDataPlaneVec& planes=npc->GetPlanes();
	for(AIDataPlaneVecIt it=planes.begin(),end=planes.end();it!=end;++it) delete *it;
	planes.clear();
	npc->NextRefreshBagTick=Timer::GameTick()+(npc->Data.BagRefreshTime?npc->Data.BagRefreshTime:GameOpt.BagRefreshTime)*60*1000;
}

void SwapCrittersRefreshClient(Client* cl, Map* map, Map* prev_map)
{
	UNSETFLAG(cl->Flags,FCRIT_NPC);
	SETFLAG(cl->Flags,FCRIT_PLAYER);

	if(cl->Talk.TalkType!=TALK_NONE) cl->CloseTalk();

	if(map!=prev_map)
	{
		cl->Send_LoadMap(NULL);
	}
	else
	{
		cl->Send_AllParams();
		cl->Send_AddAllItems();
		cl->Send_AllQuests();
		cl->Send_HoloInfo(true,0,cl->Data.HoloInfoCount);
		cl->Send_AllAutomapsInfo();

		if(map->IsTurnBasedOn)
		{
			if(map->IsCritterTurn(cl)) cl->Send_ParamOther(OTHER_YOU_TURN,map->GetCritterTurnTime());
			else
			{
				Critter* cr=cl->GetCritSelf(map->GetCritterTurnId());
				if(cr) cl->Send_CritterParam(cr,OTHER_YOU_TURN,map->GetCritterTurnTime());
			}
		}
		else if(TB_BATTLE_TIMEOUT_CHECK(cl->GetTimeout(TO_BATTLE))) cl->SetTimeout(TO_BATTLE,0);
	}
}

bool FOServer::SScriptFunc::Global_SwapCritters(Critter* cr1, Critter* cr2, bool with_inventory, bool with_vars)
{
	// Check
	if(cr1->IsNotValid) SCRIPT_ERROR_R0("Critter1 nullptr.");
	if(cr2->IsNotValid) SCRIPT_ERROR_R0("Critter2 nullptr.");
	if(cr1==cr2) SCRIPT_ERROR_R0("Critter1 is equal to Critter2.");
	if(!cr1->GetMap()) SCRIPT_ERROR_R0("Critter1 is on global map.");
	if(!cr2->GetMap()) SCRIPT_ERROR_R0("Critter2 is on global map.");

	// Swap positions
	Map* map1=MapMngr.GetMap(cr1->GetMap());
	if(!map1) SCRIPT_ERROR_R0("Map of Critter1 not found.");
	Map* map2=MapMngr.GetMap(cr2->GetMap());
	if(!map2) SCRIPT_ERROR_R0("Map of Critter2 not found.");

	CrVec& cr_map1=map1->GetCritters();
	ClVec& cl_map1=map1->GetPlayers();
	PcVec& npc_map1=map1->GetNpcs();
	CrVecIt it_cr=std::find(cr_map1.begin(),cr_map1.end(),cr1);
	if(it_cr!=cr_map1.end()) cr_map1.erase(it_cr);
	ClVecIt it_cl=std::find(cl_map1.begin(),cl_map1.end(),(Client*)cr1);
	if(it_cl!=cl_map1.end()) cl_map1.erase(it_cl);
	PcVecIt it_pc=std::find(npc_map1.begin(),npc_map1.end(),(Npc*)cr1);
	if(it_pc!=npc_map1.end()) npc_map1.erase(it_pc);

	CrVec& cr_map2=map2->GetCritters();
	ClVec& cl_map2=map2->GetPlayers();
	PcVec& npc_map2=map2->GetNpcs();
	it_cr=std::find(cr_map2.begin(),cr_map2.end(),cr1);
	if(it_cr!=cr_map2.end()) cr_map2.erase(it_cr);
	it_cl=std::find(cl_map2.begin(),cl_map2.end(),(Client*)cr1);
	if(it_cl!=cl_map2.end()) cl_map2.erase(it_cl);
	it_pc=std::find(npc_map2.begin(),npc_map2.end(),(Npc*)cr1);
	if(it_pc!=npc_map2.end()) npc_map2.erase(it_pc);

	cr_map2.push_back(cr1);
	if(cr1->IsNpc()) npc_map2.push_back((Npc*)cr1);
	else cl_map2.push_back((Client*)cr1);
	cr_map1.push_back(cr2);
	if(cr2->IsNpc()) npc_map1.push_back((Npc*)cr2);
	else cl_map1.push_back((Client*)cr2);

	cr1->SetMaps(map2->GetId(),map2->GetPid());
	cr2->SetMaps(map1->GetId(),map1->GetPid());

	// Swap data
	if(cr1->DataExt || cr2->DataExt)
	{
		CritDataExt* data_ext1=cr1->GetDataExt();
		CritDataExt* data_ext2=cr2->GetDataExt();
		if(data_ext1 && data_ext2) std::swap(*data_ext1,*data_ext2);
	}
	std::swap(cr1->Data,cr2->Data);
	std::swap(cr1->KnockoutAp,cr2->KnockoutAp);
	std::swap(cr1->Flags,cr2->Flags);
	SwapArray(cr1->FuncId,cr2->FuncId);
	std::swap(cr1->LastHealTick,cr2->LastHealTick);
	cr1->SetBreakTime(0);
	cr2->SetBreakTime(0);
	std::swap(cr1->AccessContainerId,cr2->AccessContainerId);
	std::swap(cr1->ApRegenerationTick,cr2->ApRegenerationTick);
	std::swap(cr1->CrTimeEvents,cr2->CrTimeEvents);

	// Swap inventory
	if(with_inventory)
	{
		ItemPtrVec items1=cr1->GetInventory();
		ItemPtrVec items2=cr2->GetInventory();
		for(ItemPtrVecIt it=items1.begin(),end=items1.end();it!=end;++it) cr1->EraseItem(*it,false);
		for(ItemPtrVecIt it=items2.begin(),end=items2.end();it!=end;++it) cr2->EraseItem(*it,false);
		for(ItemPtrVecIt it=items1.begin(),end=items1.end();it!=end;++it) cr2->AddItem(*it,false);
		for(ItemPtrVecIt it=items2.begin(),end=items2.end();it!=end;++it) cr1->AddItem(*it,false);
	}

	// Swap vars
	if(with_vars) VarMngr.SwapVars(cr1->GetId(),cr2->GetId());

	// Refresh
	cr1->ClearVisible();
	cr2->ClearVisible();

	if(cr1->IsNpc()) SwapCrittersRefreshNpc((Npc*)cr1);
	else SwapCrittersRefreshClient((Client*)cr1,map2,map1);
	if(cr2->IsNpc()) SwapCrittersRefreshNpc((Npc*)cr2);
	else SwapCrittersRefreshClient((Client*)cr2,map1,map2);
	if(map1==map2)
	{
		cr1->Send_ParamOther(OTHER_CLEAR_MAP,0);
		cr2->Send_ParamOther(OTHER_CLEAR_MAP,0);
		cr1->Send_Dir(cr1);
		cr2->Send_Dir(cr2);
		cr1->Send_ParamOther(OTHER_TELEPORT,(cr1->GetHexX()<<16)|(cr1->GetHexY())); // cr1->Send_XY(cr1);
		cr2->Send_ParamOther(OTHER_TELEPORT,(cr2->GetHexX()<<16)|(cr2->GetHexY())); // cr2->Send_XY(cr2);
		cr1->Send_ParamOther(OTHER_BASE_TYPE,cr1->Data.BaseType);
		cr2->Send_ParamOther(OTHER_BASE_TYPE,cr2->Data.BaseType);
		cr1->ProcessVisibleCritters();
		cr2->ProcessVisibleCritters();
		cr1->ProcessVisibleItems();
		cr2->ProcessVisibleItems();
	}
	return true;
}

DWORD FOServer::SScriptFunc::Global_GetAllItems(WORD pid, asIScriptArray* items)
{
	ItemPtrMap& game_items=ItemMngr.GetGameItems();
	ItemPtrVec items_;
	items_.reserve(game_items.size());
	for(ItemPtrMapIt it=game_items.begin(),end=game_items.end();it!=end;++it)
	{
		Item* item=(*it).second;
		if(!item->IsNotValid && (!pid || pid==item->GetProtoId())) items_.push_back(item);
	}
	if(!items_.size()) return 0;
	if(items) Script::AppendVectorToArrayRef<Item*>(items_,items);
	return items_.size();
}

DWORD FOServer::SScriptFunc::Global_GetAllNpc(WORD pid, asIScriptArray* npc)
{
	PcVec npcs;
	CrVec npcs_;
	CrMngr.GetCopyNpcs(npcs);
	npcs_.reserve(npcs.size());
	for(PcVecIt it=npcs.begin(),end=npcs.end();it!=end;++it)
	{
		Npc* npc_=*it;
		if(!npc_->IsNotValid && (!pid || pid==npc_->GetProtoId())) npcs_.push_back(npc_);
	}
	if(!npcs_.size()) return 0;
	if(npc) Script::AppendVectorToArrayRef<Critter*>(npcs_,npc);
	return npcs_.size();
}

DWORD FOServer::SScriptFunc::Global_GetAllMaps(WORD pid, asIScriptArray* maps)
{
	MapMap& maps_=MapMngr.GetAllMaps();
	MapVec maps__;
	maps__.reserve(maps_.size());
	for(MapMapIt it=maps_.begin(),end=maps_.end();it!=end;++it)
	{
		Map* map=(*it).second;
		if(!map->IsNotValid && (!pid || pid==map->GetPid())) maps__.push_back(map);
	}
	if(!maps__.size()) return 0;
	if(maps) Script::AppendVectorToArrayRef<Map*>(maps__,maps);
	return maps__.size();
}

DWORD FOServer::SScriptFunc::Global_GetAllLocations(WORD pid, asIScriptArray* locations)
{
	LocMap& locs=MapMngr.GetLocations();
	LocVec locs_;
	locs_.reserve(locs.size());
	for(LocMapIt it=locs.begin(),end=locs.end();it!=end;++it)
	{
		Location* loc=(*it).second;
		if(!loc->IsNotValid && (!pid || pid==loc->GetPid())) locs_.push_back(loc);
	}
	if(!locs_.size()) return 0;
	if(locations) Script::AppendVectorToArrayRef<Location*>(locs_,locations);
	return locs_.size();
}

CScriptString* FOServer::SScriptFunc::Global_GetScriptName(DWORD script_id)
{
	return new CScriptString(Script::GetScriptFuncName(script_id));
}

asIScriptArray* FOServer::SScriptFunc::Global_GetItemDataMask(int mask_type)
{
	if(mask_type<0 || mask_type>=ITEM_DATA_MASK_MAX) SCRIPT_ERROR_R0("Invalid mask type arg.");
	asIScriptArray* result=Script::CreateArray("int8[]");
	if(!result) return NULL;
	CharVec mask;
	for(size_t i=0;i<sizeof(Item::ItemData);i++) mask.push_back(Item::ItemData::SendMask[mask_type][i]);
	Script::AppendVectorToArray(mask,result);
	return result;
}

bool FOServer::SScriptFunc::Global_SetItemDataMask(int mask_type, asIScriptArray& mask)
{
	if(mask_type<0 || mask_type>=ITEM_DATA_MASK_MAX) SCRIPT_ERROR_R0("Invalid mask type arg.");
	if(mask.GetElementCount()!=sizeof(Item::ItemData)) SCRIPT_ERROR_R0("Invalid mask size.");
	for(size_t i=0;i<sizeof(Item::ItemData);i++) Item::ItemData::SendMask[mask_type][i]=*(char*)mask.GetElementPointer(i);
	return true;
}

void FOServer::SScriptFunc::Global_GetTime(WORD& year, WORD& month, WORD& day, WORD& day_of_week, WORD& hour, WORD& minute, WORD& second, WORD& milliseconds)
{
	SYSTEMTIME cur_time;
	GetLocalTime(&cur_time);
	year=cur_time.wYear;
	month=cur_time.wMonth;
	day_of_week=cur_time.wDayOfWeek;
	day=cur_time.wDay;
	hour=cur_time.wHour;
	minute=cur_time.wMinute;
	second=cur_time.wSecond;
	milliseconds=cur_time.wMilliseconds;
}

bool FOServer::SScriptFunc::Global_SetParameterGetBehaviour(DWORD index, CScriptString& func_name)
{
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R0("Invalid index arg.");
	Critter::ParamsGetScript[index]=0;
	if(func_name.length()>0)
	{
		int bind_id=Script::Bind(func_name.c_str(),"int %s(Critter&,uint)",false);
		if(bind_id<=0) SCRIPT_ERROR_R0("Function not found.");
		Critter::ParamsGetScript[index]=bind_id;
	}
	return true;
}

bool FOServer::SScriptFunc::Global_SetParameterChangeBehaviour(DWORD index, CScriptString& func_name)
{
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R0("Invalid index arg.");
	Critter::ParamsChangeScript[index]=0;
	if(func_name.length()>0)
	{
		int bind_id=Script::Bind(func_name.c_str(),"void %s(Critter&,uint,int)",false);
		if(bind_id<=0) SCRIPT_ERROR_R0("Function not found.");
		Critter::ParamsChangeScript[index]=bind_id;
	}
	return true;
}

void FOServer::SScriptFunc::Global_AllowSlot(BYTE index, CScriptString& ini_option)
{
	if(index<=SLOT_ARMOR || index==SLOT_GROUND) SCRIPT_ERROR_R("Invalid index arg.");
	if(!ini_option.length()) SCRIPT_ERROR_R("Ini string is empty.");
	Critter::SlotEnabled[index]=true;
}

void FOServer::SScriptFunc::Global_SetRegistrationParam(DWORD index, bool enabled)
{
	if(index>=MAX_PARAMS) SCRIPT_ERROR_R("Invalid index arg.");
	Critter::ParamsRegEnabled[index]=enabled;
}

DWORD FOServer::SScriptFunc::Global_GetAngelScriptProperty(int property)
{
	asIScriptEngine* engine=Script::GetEngine();
	if(!engine) SCRIPT_ERROR_R0("Can't get engine.");
	return engine->GetEngineProperty((asEEngineProp)property);
}

bool FOServer::SScriptFunc::Global_SetAngelScriptProperty(int property, DWORD value)
{
	asIScriptEngine* engine=Script::GetEngine();
	if(!engine) SCRIPT_ERROR_R0("Can't get engine.");
	int result=engine->SetEngineProperty((asEEngineProp)property,value);
	if(result<0) SCRIPT_ERROR_R0("Invalid data. Property not setted.");
	return true;
}

DWORD FOServer::SScriptFunc::Global_GetStrHash(CScriptString* str)
{
	if(str) return Str::GetHash(str->c_str());
	return 0;
}

bool FOServer::SScriptFunc::Global_IsCritterCanWalk(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsCanWalk(cr_type);
}

bool FOServer::SScriptFunc::Global_IsCritterCanRun(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsCanRun(cr_type);
}

bool FOServer::SScriptFunc::Global_IsCritterCanRotate(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsCanRotate(cr_type);
}

bool FOServer::SScriptFunc::Global_IsCritterCanAim(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsCanAim(cr_type);
}

bool FOServer::SScriptFunc::Global_IsCritterAnim1(DWORD cr_type, DWORD index)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsAnim1(cr_type,index);
}

bool FOServer::SScriptFunc::Global_IsCritterAnim3d(DWORD cr_type)
{
	if(!CritType::IsEnabled(cr_type)) SCRIPT_ERROR_R0("Invalid critter type arg.");
	return CritType::IsAnim3d(cr_type);
}

int FOServer::SScriptFunc::Global_GetGlobalMapRelief(DWORD x, DWORD y)
{
	if(x>=GM_MAXX || y>=GM_MAXY) SCRIPT_ERROR_R0("Invalid coord args.");
	return MapMngr.GetGmRelief(x,y);
}

DWORD FOServer::SScriptFunc::Global_GetScriptId(CScriptString& script_name, CScriptString& func_decl)
{
	return Script::GetScriptFuncNum(script_name.c_str(),func_decl.c_str());
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
