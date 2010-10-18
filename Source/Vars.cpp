#include "StdAfx.h"
#include "Vars.h"
#include "Text.h"

#ifdef FONLINE_SERVER
#include "Critter.h"
#endif

VarManager VarMngr;
FileLogger* DbgLog=NULL;

bool VarManager::Init(const char* fpath)
{
	WriteLog("Var Manager initialization.\n");

	if(!fpath) varsPath=".\\";
	else varsPath=string(fpath);

	// Load vars templates
	if(!UpdateVarsTemplate()) return false;

	if(GameOpt.LoggingVars) DbgLog=new FileLogger("vars.log");

	isInit=true;
	WriteLog("Var Manager initialization complete.\n");
	return true;
}

#ifdef FONLINE_SERVER
void VarManager::SaveVarsDataFile(void(*save_func)(void*,size_t))
{
	save_func(&varsCount,sizeof(varsCount));
	for(TempVarVecIt it=tempVars.begin(),end=tempVars.end();it!=end;++it)
	{
		TemplateVar* tvar=*it;
		if(tvar)
		{
			for(VarsMap32It it_=tvar->Vars.begin(),end_=tvar->Vars.end();it_!=end_;++it_)
			{
				GameVar* var=(*it_).second;
				save_func(&var->VarTemplate->TempId,sizeof(var->VarTemplate->TempId));
				save_func(&var->MasterId,sizeof(var->MasterId));
				save_func(&var->SlaveId,sizeof(var->SlaveId));
				save_func(&var->VarValue,sizeof(var->VarValue));
			}
			for(VarsMap64It it_=tvar->VarsUnicum.begin(),end_=tvar->VarsUnicum.end();it_!=end_;++it_)
			{
				GameVar* var=(*it_).second;
				save_func(&var->VarTemplate->TempId,sizeof(var->VarTemplate->TempId));
				save_func(&var->MasterId,sizeof(var->MasterId));
				save_func(&var->SlaveId,sizeof(var->SlaveId));
				save_func(&var->VarValue,sizeof(var->VarValue));
			}
		}
	}
}

bool VarManager::LoadVarsDataFile(FILE* f, int version)
{
	WriteLog("Load vars...");
	allQuestVars.reserve(10000); // 40kb

	DWORD count=0;
	fread(&count,sizeof(count),1,f);
	for(DWORD i=0;i<count;i++)
	{
		WORD temp_id;
		DWORD master_id,slave_id;
		int val;
		if(version<WORLD_SAVE_V10)
		{
			ULONGLONG id;
			fread(&id,sizeof(id),1,f);
			fread(&temp_id,sizeof(temp_id),1,f);
			fread(&val,sizeof(val),1,f);
			master_id=(id>>24)&0xFFFFFF;
			slave_id=id&0xFFFFFF;
		}
		else
		{
			fread(&temp_id,sizeof(temp_id),1,f);
			fread(&master_id,sizeof(master_id),1,f);
			fread(&slave_id,sizeof(slave_id),1,f);
			fread(&val,sizeof(val),1,f);
		}

		TemplateVar* tvar=GetTemplateVar(temp_id);
		if(!tvar)
		{
			WriteLog("Template var not found, tid<%u>.\n",temp_id);
			continue;
		}

		if(tvar->IsError())
		{
			WriteLog("Template var have invalid data, tid<%u>.\n",temp_id);
			continue;
		}

		GameVar* var;
		if(tvar->IsNotUnicum()) var=CreateVar(master_id,tvar);
		else var=CreateVarUnicum((((ULONGLONG)slave_id)<<32)|((ULONGLONG)master_id),master_id,slave_id,tvar);

		if(!var)
		{
			WriteLog("Can't create var, tid<%u>.\n",temp_id);
			continue;
		}

		var->VarValue=val;
	}

	WriteLog("complete, count<%u>.\n",count);
	return true;
}
#endif // FONLINE_SERVER

void VarManager::Finish()
{
	WriteLog("Var manager finish...\n");

	Clear();

	for(TempVarVecIt it=tempVars.begin(),end=tempVars.end();it!=end;++it) SAFEDEL(*it);
	tempVars.clear();

	varsPath="";
	SAFEDEL(DbgLog);
	isInit=false;
	WriteLog("Var manager finish complete.\n");
}

void VarManager::Clear()
{
#ifdef FONLINE_SERVER
	for(TempVarVecIt it=tempVars.begin(),end=tempVars.end();it!=end;++it)
	{
		TemplateVar* tvar=*it;
		if(tvar)
		{
			for(VarsMap32It it_=tvar->Vars.begin(),end_=tvar->Vars.end();it_!=end_;++it_)
				(*it_).second->Release();
			tvar->Vars.clear();
			for(VarsMap64It it_=tvar->VarsUnicum.begin(),end_=tvar->VarsUnicum.end();it_!=end_;++it_)
				(*it_).second->Release();
			tvar->VarsUnicum.clear();
		}
	}
	varsCount=0;
#endif
}

bool VarManager::UpdateVarsTemplate()
{
	WriteLog("Update template vars...");

	string full_path=varsPath+string(VAR_FNAME_VARS);
	FILE* f=fopen(full_path.c_str(),"rb");
	if(!f)
	{
		WriteLog("file<%s> not found.\n",full_path.c_str());
		return false;
	}

	TempVarVec load_vars;
	if(!LoadTemplateVars(f,load_vars)) return false;
	fclose(f);

	for(TempVarVecIt it=load_vars.begin(),it_end=load_vars.end();it!=it_end;++it)
	{
		if(!AddTemplateVar(*it)) return false;
	}

	WriteLog("complete.\n");
	return true;
}

bool VarManager::LoadTemplateVars(FILE* f, TempVarVec& vars)
{
	if(!f) return false;
	
	WORD var_id;
	int var_type;
	char var_name[VAR_NAME_LEN];
	char var_desc[VAR_DESC_LEN];
	int var_start;
	int var_min;
	int var_max;
	DWORD var_flags;

	fseek(f,0,SEEK_END);
	long fsize=ftell(f);
	fseek(f,0,SEEK_SET);

	char* buf_begin=new char[fsize];
	fread(buf_begin,1,fsize-1,f);
	buf_begin[fsize-1]=0;
	Str::Replacement(buf_begin,'\r','\n','\n');
	char* buf=buf_begin;

	for(;*buf;buf++)
	{
		if(*buf!='$') continue;
		buf++;

		if(sscanf(buf,"%u%d%s%d%d%d%u",&var_id,&var_type,var_name,&var_start,&var_min,&var_max,&var_flags)!=7)
		{
			WriteLog("Fail to scan var.\n");
			return false;
		}

		// Description
		StringCopy(var_desc,"error");

		do
		{
			char* mark=strstr(buf,VAR_DESC_MARK);
			if(!mark) break;
			Str::SkipLine(mark);
			char* mark2=strstr(mark,VAR_DESC_MARK);
			if(!mark2) break;
			*(mark2-1)=0;
			StringCopy(var_desc,mark);

			buf=mark2+sizeof(VAR_DESC_MARK);
		}
		while(false);

		// Parse var
		TemplateVar* var=new TemplateVar();
		var->TempId=var_id;
		var->Type=var_type;
		var->Name=string(var_name);
		var->Desc=string(var_desc);
		var->StartVal=var_start;
		var->MinVal=var_min;
		var->MaxVal=var_max;
		var->Flags=var_flags;
		vars.push_back(var);
	}

	delete[] buf_begin;
	return true;
}

bool VarManager::AddTemplateVar(TemplateVar* var)
{
	if(!var)
	{
		WriteLog(__FUNCTION__" - Nullptr.\n");
		return false;
	}

	if(var->IsError())
	{
		WriteLog(__FUNCTION__" - IsError, name<%s>, id<%u>.\n",var->Name.c_str(),var->TempId);
		return false;
	}

	if(IsTemplateVarAviable(var->Name.c_str()))
	{
		WriteLog(__FUNCTION__" - Name already used, name<%s>, id<%u>.\n",var->Name.c_str(),var->TempId);
		return false;
	}

	if(var->TempId<tempVars.size() && tempVars[var->TempId])
	{
		WriteLog(__FUNCTION__" - Id already used, name<%s>, id<%u>.\n",var->Name.c_str(),var->TempId);
		return false;
	}

	if(var->TempId>=tempVars.size()) tempVars.resize(var->TempId+1);
	tempVars[var->TempId]=var;
	return true;
}

void VarManager::EraseTemplateVar(WORD temp_id)
{
	TemplateVar* var=GetTemplateVar(temp_id);
	if(!var) return;

	delete var; // If delete on server runtime than possible memory leaks
	tempVars[temp_id]=NULL;
}

WORD VarManager::GetTemplateVarId(const char* var_name)
{
	for(TempVarVecIt it=tempVars.begin(),end=tempVars.end();it!=end;++it)
	{
		TemplateVar* tvar=*it;
		if(tvar && !_stricmp(tvar->Name.c_str(),var_name)) return tvar->TempId;
	}
	return false;
}

TemplateVar* VarManager::GetTemplateVar(WORD temp_id)
{
	if(temp_id<tempVars.size()) return tempVars[temp_id];
	return NULL;
}

bool VarManager::IsTemplateVarAviable(const char* var_name)
{
	for(TempVarVecIt it=tempVars.begin(),end=tempVars.end();it!=end;++it)
	{
		TemplateVar* tvar=*it;
		if(tvar && !_stricmp(tvar->Name.c_str(),var_name)) return true;
	}
	return false;
}

void VarManager::SaveTemplateVars()
{
	WriteLog("Save vars...");

	string full_path=varsPath+string(VAR_FNAME_VARS);
	FILE* f=fopen(full_path.c_str(),"wt");
	if(f)
	{
		DWORD count=0;
		for(TempVarVecIt it=tempVars.begin();it!=tempVars.end();++it) if(*it) count++;

		fprintf(f,"#ifndef __VARS__\n");
		fprintf(f,"#define __VARS__\n");
		fprintf(f,"/*************************************************************************************\n");
		fprintf(f,"***  VARS  *****  COUNT: %08d  ***************************************************\n",count);
		fprintf(f,"*************************************************************************************/\n");
		fprintf(f,"\n\n");

		for(TempVarVecIt it=tempVars.begin();it!=tempVars.end();++it)
		{
			TemplateVar* var=*it;
			if(!var) continue;

			fprintf(f,"#define ");
			if(var->Type==VAR_GLOBAL) fprintf(f,"GVAR_");
			else if(var->Type==VAR_LOCAL) fprintf(f,"LVAR_");
			else if(var->Type==VAR_UNICUM) fprintf(f,"UVAR_");
			else if(var->Type==VAR_LOCAL_LOCATION) fprintf(f,"LLVAR_");
			else if(var->Type==VAR_LOCAL_MAP) fprintf(f,"LMVAR_");
			else if(var->Type==VAR_LOCAL_ITEM) fprintf(f,"LIVAR_");
			else fprintf(f,"?VAR_");
			fprintf(f,"%s",var->Name.c_str());
			int spaces=var->Name.length()+(var->Type==VAR_LOCAL_LOCATION || var->Type==VAR_LOCAL_MAP || var->Type==VAR_LOCAL_ITEM?1:0);
			for(int i=0,j=max(1,40-spaces);i<j;i++) fprintf(f," ");
			fprintf(f,"(%u)\n",var->TempId);
		}

		fprintf(f,"\n\n");
		fprintf(f,"/*************************************************************************************\n");
		fprintf(f,"\tId\tType\tName\t\tStart\tMin\tMax\tFlags\n");
		fprintf(f,"**************************************************************************************\n");

		for(TempVarVecIt it=tempVars.begin();it!=tempVars.end();++it)
		{
			TemplateVar* var=*it;
			if(!var) continue;

			fprintf(f,"$\t%u\t%d\t%s\t%d\t%d\t%d\t%u\n",
				var->TempId,var->Type,var->Name.c_str(),var->StartVal,var->MinVal,var->MaxVal,var->Flags);

			fprintf(f,"%s\n",VAR_DESC_MARK);
			fprintf(f,"%s\n",var->Desc.c_str());
			fprintf(f,"%s\n",VAR_DESC_MARK);
			fprintf(f,"\n");
		}

		fprintf(f,"**************************************************************************************\n");
		fprintf(f,"**************************************************************************************\n");
		fprintf(f,"*************************************************************************************/\n");
		fprintf(f,"#endif\n");
		fclose(f);
	}
	else
	{
		WriteLog("unable to create file<%s>.\n",full_path);
	}
	WriteLog("complete.\n");
}

/**************************************************************************************************
***************************************************************************************************
**************************************************************************************************/
#ifdef FONLINE_SERVER

bool VarManager::CheckVar(const char* var_name, DWORD master_id, DWORD slave_id, char oper, int val)
{
	WORD temp_id=GetTemplateVarId(var_name);
	if(!temp_id) return false;
	GameVar* uvar=GetVar(temp_id,master_id,slave_id,true);
	if(!uvar) return false;
	return CheckVar(uvar,oper,val);
}

bool VarManager::CheckVar(WORD temp_id, DWORD master_id, DWORD slave_id, char oper, int val)
{
	GameVar* var=GetVar(temp_id,master_id,slave_id,true);
	if(!var) return false;
	return CheckVar(var,oper,val);
}

GameVar* VarManager::ChangeVar(const char* var_name, DWORD master_id, DWORD slave_id, char oper, int val)
{
	WORD temp_id=GetTemplateVarId(var_name);
	if(!temp_id) return NULL;
	GameVar* var=GetVar(temp_id,master_id,slave_id,true);
	if(!var) return NULL;
	ChangeVar(var,oper,val);
	return var;
}

GameVar* VarManager::ChangeVar(WORD temp_id, DWORD master_id, DWORD slave_id, char oper, int val)
{
	GameVar* var=GetVar(temp_id,master_id,slave_id,true);
	if(!var) return NULL;
	ChangeVar(var,oper,val);
	return var;
}

/**************************************************************************************************
***************************************************************************************************
**************************************************************************************************/

bool VarManager::CheckVar(GameVar* var, char oper, int val)
{
	switch(oper)
	{
	case '>': return *var>val;
	case '<': return *var<val;
	case '=': return *var==val;
	case '!': return *var!=val;
	case '}': return *var>=val;
	case '{': return *var<=val;
	default: return false;
	}
}

void VarManager::ChangeVar(GameVar* var, char oper, int val)
{
	switch(oper)
	{
	case '+': *var+=val; break;
	case '-': *var-=val; break;
	case '*': *var*=val; break;
	case '/': *var/=val; break;
	case '=': *var=val; break;
	default: break;
	}
}

/**************************************************************************************************
***************************************************************************************************
**************************************************************************************************/

GameVar* VarManager::GetVar(const char* name, DWORD master_id, DWORD slave_id,  bool create)
{
	WORD temp_id=GetTemplateVarId(name);
	if(!temp_id) return NULL;
	return GetVar(temp_id,master_id,slave_id,create);
}

GameVar* VarManager::GetVar(WORD temp_id, DWORD master_id, DWORD slave_id,  bool create)
{
	TemplateVar* tvar=GetTemplateVar(temp_id);
	if(!tvar) return NULL;

	switch(tvar->Type)
	{
	case VAR_GLOBAL:
		if(master_id || slave_id) return NULL;
		master_id=temp_id;
		break;
	case VAR_LOCAL:
		if(!master_id || slave_id) return NULL;
		break;
	case VAR_UNICUM:
		if(!master_id || !slave_id) return NULL;
		break;
	case VAR_LOCAL_LOCATION:
		if(!master_id || slave_id) return NULL;
		break;
	case VAR_LOCAL_MAP:
		if(!master_id || slave_id) return NULL;
		break;
	case VAR_LOCAL_ITEM:
		if(!master_id || slave_id) return NULL;
		break;
	default:
		return NULL;
	}

	GameVar* var;
	bool allocated=false; // For DbgLog

	if(tvar->IsNotUnicum())
	{
		SCOPE_LOCK(varsLocker);

		VarsMap32It it=tvar->Vars.find(master_id);
		if(it==tvar->Vars.end())
		{
			if(!create) return NULL;
			var=CreateVar(master_id,tvar);
			if(!var) return NULL;
			allocated=true;
		}
		else
		{
			var=(*it).second;
		}
	}
	else
	{
		SCOPE_LOCK(varsLocker);

		ULONGLONG id=(((ULONGLONG)slave_id)<<32)|((ULONGLONG)master_id);
		VarsMap64It it=tvar->VarsUnicum.find(id);
		if(it==tvar->VarsUnicum.end())
		{
			if(!create) return NULL;
			var=CreateVarUnicum(id,master_id,slave_id,tvar);
			if(!var) return NULL;
			allocated=true;
		}
		else
		{
			var=(*it).second;
		}
	}

	SYNC_LOCK(var);

	if(DbgLog)
	{
		if(tvar->Type==VAR_GLOBAL) DbgLog->Write("Reading gvar<%s> value<%d>.%s\n",tvar->Name.c_str(),var->GetValue(),allocated?" Allocated.":"");
		else if(tvar->Type==VAR_LOCAL) DbgLog->Write("Reading lvar<%s> masterId<%u> value<%d>.%s\n",tvar->Name.c_str(),master_id,var->GetValue(),allocated?" Allocated.":"");
		else if(tvar->Type==VAR_UNICUM) DbgLog->Write("Reading uvar<%s> masterId<%u> slaveId<%u> value<%d>.%s\n",tvar->Name.c_str(),master_id,slave_id,var->GetValue(),allocated?" Allocated.":"");
		else if(tvar->Type==VAR_LOCAL_LOCATION) DbgLog->Write("Reading llvar<%s> locId<%u> value<%d>.%s\n",tvar->Name.c_str(),master_id,var->GetValue(),allocated?" Allocated.":"");
		else if(tvar->Type==VAR_LOCAL_MAP) DbgLog->Write("Reading lmvar<%s> mapId<%u> value<%d>.%s\n",tvar->Name.c_str(),master_id,var->GetValue(),allocated?" Allocated.":"");
		else if(tvar->Type==VAR_LOCAL_ITEM) DbgLog->Write("Reading livar<%s> itemId<%u> value<%d>.%s\n",tvar->Name.c_str(),master_id,var->GetValue(),allocated?" Allocated.":"");
	}

	return var;
}

GameVar* VarManager::CreateVar(DWORD master_id, TemplateVar* tvar)
{
	GameVar* var=new(nothrow) GameVar(master_id,0,tvar,tvar->IsRandom()?Random(tvar->MinVal,tvar->MaxVal):tvar->StartVal);
	if(!var) return NULL;

	tvar->Vars.insert(VarsMap32Val(master_id,var));

	if(tvar->IsQuest())
	{
		bool founded=false;
		for(size_t i=0,j=allQuestVars.size();i<j;i++)
		{
			if(!allQuestVars[i])
			{
				var->QuestVarIndex=i;
				allQuestVars[i]=var;
				founded=true;
				break;
			}
		}

		if(!founded)
		{
			var->QuestVarIndex=allQuestVars.size();
			allQuestVars.push_back(var);
		}
	}

	varsCount++;
	return var;
}

GameVar* VarManager::CreateVarUnicum(ULONGLONG id, DWORD master_id, DWORD slave_id, TemplateVar* tvar)
{
	GameVar* var=new(nothrow) GameVar(master_id,slave_id,tvar,tvar->IsRandom()?Random(tvar->MinVal,tvar->MaxVal):tvar->StartVal);
	if(!var) return NULL;

	tvar->VarsUnicum.insert(VarsMap64Val(id,var));

	varsCount++;
	return var;
}

void VarManager::SwapVars(DWORD id1, DWORD id2)
{
	if(!id1 || !id2 || id1==id2) return;

	// Collect vars
	VarsVec swap_vars1;
	VarsVec swap_vars2;
	VarsVec swap_vars_share;
	varsLocker.Lock();
	for(TempVarVecIt it=tempVars.begin(),end=tempVars.end();it!=end;++it)
	{
		TemplateVar* tvar=*it;
		if(tvar && (tvar->Type==VAR_LOCAL || tvar->Type==VAR_UNICUM))
		{
			if(tvar->IsNotUnicum())
			{
				for(VarsMap32It it_=tvar->Vars.begin(),end_=tvar->Vars.end();it_!=end_;++it_)
				{
					GameVar* var=(*it_).second;
					if(var->MasterId==id1) swap_vars1.push_back(var);
					else if(var->MasterId==id2) swap_vars2.push_back(var);
				}
			}
			else
			{
				for(VarsMap64It it_=tvar->VarsUnicum.begin(),end_=tvar->VarsUnicum.end();it_!=end_;++it_)
				{
					GameVar* var=(*it_).second;
					if((var->MasterId==id1 && var->SlaveId==id2) ||
						(var->MasterId==id2 && var->SlaveId==id1)) swap_vars_share.push_back(var);
					else if(var->MasterId==id1 || var->SlaveId==id1) swap_vars1.push_back(var);
					else if(var->MasterId==id2 || var->SlaveId==id2) swap_vars2.push_back(var);
				}
			}
		}
	}
	varsLocker.Unlock();

	// Synchronize
	for(VarsVecIt it=swap_vars1.begin(),end=swap_vars1.end();it!=end;++it) SYNC_LOCK(*it);
	for(VarsVecIt it=swap_vars2.begin(),end=swap_vars2.end();it!=end;++it) SYNC_LOCK(*it);
	for(VarsVecIt it=swap_vars_share.begin(),end=swap_vars_share.end();it!=end;++it) SYNC_LOCK(*it);

	// Swap shared
	varsLocker.Lock();
	for(VarsVecIt it=swap_vars_share.begin(),end=swap_vars_share.end();it!=end;)
	{
		GameVar* var=*it;
		TemplateVar* tvar=var->VarTemplate;

		tvar->VarsUnicum.erase(var->GetUid());
		std::swap(var->MasterId,var->SlaveId);
		tvar->VarsUnicum.insert(VarsMap64Val(var->GetUid(),var));
	}

	// Erase vars
	for(VarsVecIt it=swap_vars1.begin(),end=swap_vars1.end();it!=end;++it)
	{
		GameVar* var=*it;
		TemplateVar* tvar=var->VarTemplate;
		if(tvar->IsNotUnicum()) tvar->Vars.erase(id1);
		else tvar->VarsUnicum.erase(var->GetUid());
	}
	for(VarsVecIt it=swap_vars2.begin(),end=swap_vars2.end();it!=end;++it)
	{
		GameVar* var=*it;
		TemplateVar* tvar=var->VarTemplate;
		if(tvar->IsNotUnicum()) tvar->Vars.erase(id2);
		else tvar->VarsUnicum.erase(var->GetUid());
	}

	// Change owner, place
	for(VarsVecIt it=swap_vars1.begin(),end=swap_vars1.end();it!=end;++it)
	{
		GameVar* var=*it;
		TemplateVar* tvar=var->VarTemplate;

		if(tvar->IsNotUnicum())
		{
			var->MasterId=id2;
			tvar->Vars.insert(VarsMap32Val(id2,var));
		}
		else
		{
			if(var->MasterId==id1) var->MasterId=id2;
			else var->SlaveId=id2;
			tvar->VarsUnicum.insert(VarsMap64Val(var->GetUid(),var));
		}
	}
	for(VarsVecIt it=swap_vars2.begin(),end=swap_vars2.end();it!=end;++it)
	{
		GameVar* var=*it;
		TemplateVar* tvar=var->VarTemplate;

		if(tvar->IsNotUnicum())
		{
			var->MasterId=id1;
			tvar->Vars.insert(VarsMap32Val(id1,var));
		}
		else
		{
			if(var->MasterId==id2) var->MasterId=id1;
			else var->SlaveId=id1;
			tvar->VarsUnicum.insert(VarsMap64Val(var->GetUid(),var));
		}
	}
	varsLocker.Unlock();
}

DWORD VarManager::ClearUnusedVars(DwordSet& ids1, DwordSet& ids2, DwordSet& ids_locs, DwordSet& ids_maps, DwordSet& ids_items)
{
	// Collect all vars
	varsLocker.Lock();
	VarsVec all_vars;
	for(TempVarVecIt it=tempVars.begin(),end=tempVars.end();it!=end;++it)
	{
		TemplateVar* tvar=*it;
		if(tvar && tvar->Type!=VAR_GLOBAL)
		{
			for(VarsMap32It it_=tvar->Vars.begin(),end_=tvar->Vars.end();it_!=end_;++it_)
				all_vars.push_back((*it_).second);
			for(VarsMap64It it_=tvar->VarsUnicum.begin(),end_=tvar->VarsUnicum.end();it_!=end_;++it_)
				all_vars.push_back((*it_).second);
		}
	}
	varsLocker.Unlock();

	// Collect non used vars, synchronize it
	VarsVec del_vars;
	for(VarsVecIt it=all_vars.begin();it!=all_vars.end();++it)
	{
		GameVar* var=*it;
		TemplateVar* tvar=var->VarTemplate;

		if(var->VarValue!=var->VarTemplate->StartVal || tvar->IsRandom() || tvar->IsQuest())
		{
			switch(var->Type)
			{
			case VAR_LOCAL:
				if(ids1.count(var->MasterId) || ids2.count(var->MasterId)) continue;
				break;
			case VAR_UNICUM:
				if(ids1.count(var->MasterId) || ids2.count(var->MasterId)) continue;
				if(ids1.count(var->SlaveId) || ids2.count(var->SlaveId)) continue;
				break;
			case VAR_LOCAL_LOCATION:
				if(ids_locs.count(var->MasterId)) continue;
				break;
			case VAR_LOCAL_MAP:
				if(ids_maps.count(var->MasterId)) continue;
				break;
			case VAR_LOCAL_ITEM:
				if(ids_items.count(var->MasterId)) continue;
				break;
			default:
				break;
			}
		}

		del_vars.push_back(var);
		SYNC_LOCK(var);
	}

	// Delete vars
	size_t del_count=0;
	varsLocker.Lock();
	for(VarsVecIt it=del_vars.begin();it!=del_vars.end();++it)
	{
		GameVar* var=*it;
		TemplateVar* tvar=var->VarTemplate;

		// Be sure what var not changed between collection
		if(var->VarValue!=var->VarTemplate->StartVal || tvar->IsRandom() || tvar->IsQuest())
		{
			switch(var->Type)
			{
			case VAR_LOCAL:
				if(ids1.count(var->MasterId) || ids2.count(var->MasterId)) continue;
				break;
			case VAR_UNICUM:
				if(ids1.count(var->MasterId) || ids2.count(var->MasterId)) continue;
				if(ids1.count(var->SlaveId) || ids2.count(var->SlaveId)) continue;
				break;
			case VAR_LOCAL_LOCATION:
				if(ids_locs.count(var->MasterId)) continue;
				break;
			case VAR_LOCAL_MAP:
				if(ids_maps.count(var->MasterId)) continue;
				break;
			case VAR_LOCAL_ITEM:
				if(ids_items.count(var->MasterId)) continue;
				break;
			default:
				break;
			}
		}

		// Delete it
		if(tvar->IsQuest()) allQuestVars[var->QuestVarIndex]=NULL;

		if(tvar->IsNotUnicum()) tvar->Vars.erase(var->MasterId);
		else tvar->VarsUnicum.erase(var->GetUid());

		Job::DeferredRelease(var);

		del_count++;
		varsCount--;
	}
	varsLocker.Unlock();

	return del_count;
}

void VarManager::GetQuestVars(DWORD master_id, DwordVec& vars)
{
	SCOPE_LOCK(varsLocker);

	for(VarsVecIt it=allQuestVars.begin(),end=allQuestVars.end();it!=end;++it)
	{
		GameVar* var=*it;
		if(var && var->MasterId==master_id) vars.push_back(VAR_CALC_QUEST(var->VarTemplate->TempId,var->VarValue));
	}
}

/**************************************************************************************************
***************************************************************************************************
**************************************************************************************************/

void DebugLog(GameVar* var, const char* op, int value)
{
	DWORD master_id=var->MasterId;
	DWORD slave_id=var->SlaveId;
	TemplateVar* tvar=var->GetTemplateVar();
	if(tvar->Type==VAR_GLOBAL) DbgLog->Write("Changing gvar<%s> op<%s> value<%d> result<%d>.\n",tvar->Name.c_str(),op,value,var->GetValue());
	else if(tvar->Type==VAR_LOCAL) DbgLog->Write("Changing lvar<%s> masterId<%u> op<%s> value<%d> result<%d>.\n",tvar->Name.c_str(),master_id,op,value,var->GetValue());
	else if(tvar->Type==VAR_UNICUM) DbgLog->Write("Changing uvar<%s> masterId<%u> slaveId<%u> op<%s> value<%d> result<%d>.\n",tvar->Name.c_str(),master_id,slave_id,op,value,var->GetValue());
}

GameVar& GameVar::operator+=(const int _right)
{
	VarValue+=_right;
	if(!VarTemplate->IsNoBorders() && VarValue>VarTemplate->MaxVal) VarValue=VarTemplate->MaxVal;
	if(DbgLog) DebugLog(this,"+=",_right);
	return *this;
}

GameVar& GameVar::operator-=(const int _right)
{
	VarValue-=_right;
	if(!VarTemplate->IsNoBorders() && VarValue<VarTemplate->MinVal) VarValue=VarTemplate->MinVal;
	if(DbgLog) DebugLog(this,"-=",_right);
	return *this;
}

GameVar& GameVar::operator*=(const int _right)
{
	VarValue*=_right;
	if(!VarTemplate->IsNoBorders() && VarValue>VarTemplate->MaxVal) VarValue=VarTemplate->MaxVal;
	if(DbgLog) DebugLog(this,"*=",_right);
	return *this;
}

GameVar& GameVar::operator/=(const int _right)
{
	VarValue/=_right;
	if(!VarTemplate->IsNoBorders() && VarValue<VarTemplate->MinVal) VarValue=VarTemplate->MinVal;
	if(DbgLog) DebugLog(this,"/=",_right);
	return *this;
}

GameVar& GameVar::operator=(const int _right)
{
	VarValue=_right;
	if(!VarTemplate->IsNoBorders())
	{
		if(VarValue>VarTemplate->MaxVal) VarValue=VarTemplate->MaxVal;
		if(VarValue<VarTemplate->MinVal) VarValue=VarTemplate->MinVal;
	}
	if(DbgLog) DebugLog(this,"=",_right);
	return *this;
}

GameVar& GameVar::operator+=(const GameVar& _right)
{
	VarValue+=_right.VarValue;
	if(!VarTemplate->IsNoBorders() && VarValue>VarTemplate->MaxVal) VarValue=VarTemplate->MaxVal;
	if(DbgLog) DebugLog(this,"+=",_right.VarValue);
	return *this;
}

GameVar& GameVar::operator-=(const GameVar& _right)
{
	VarValue-=_right.VarValue;
	if(!VarTemplate->IsNoBorders() && VarValue<VarTemplate->MinVal) VarValue=VarTemplate->MinVal;
	if(DbgLog) DebugLog(this,"-=",_right.VarValue);
	return *this;
}

GameVar& GameVar::operator*=(const GameVar& _right)
{
	VarValue*=_right.VarValue;
	if(!VarTemplate->IsNoBorders() && VarValue>VarTemplate->MaxVal) VarValue=VarTemplate->MaxVal;
	if(DbgLog) DebugLog(this,"*=",_right.VarValue);
	return *this;
}

GameVar& GameVar::operator/=(const GameVar& _right)
{
	VarValue/=_right.VarValue;
	if(!VarTemplate->IsNoBorders() && VarValue<VarTemplate->MinVal) VarValue=VarTemplate->MinVal;
	if(DbgLog) DebugLog(this,"/=",_right.VarValue);
	return *this;
}

GameVar& GameVar::operator=(const GameVar& _right)
{
	VarValue=_right.VarValue;
	if(!VarTemplate->IsNoBorders())
	{
		if(VarValue>VarTemplate->MaxVal) VarValue=VarTemplate->MaxVal;
		if(VarValue<VarTemplate->MinVal) VarValue=VarTemplate->MinVal;
	}
	if(DbgLog) DebugLog(this,"=",_right.VarValue);
	return *this;
}

int GameVarAddInt(GameVar& var, const int _right){return var+_right;}
int GameVarSubInt(GameVar& var, const int _right){return var-_right;}
int GameVarMulInt(GameVar& var, const int _right){return var*_right;}
int GameVarDivInt(GameVar& var, const int _right){return var/_right;}
int GameVarAddGameVar(GameVar& var, GameVar& _right){return var+_right;}
int GameVarSubGameVar(GameVar& var, GameVar& _right){return var-_right;}
int GameVarMulGameVar(GameVar& var, GameVar& _right){return var*_right;}
int GameVarDivGameVar(GameVar& var, GameVar& _right){return var/_right;}
bool GameVarEqualInt(const GameVar& var, const int _right){return var==_right;}
int GameVarCmpInt(const GameVar& var, const int _right){int cmp=0; if(var<_right) cmp=-1; else if(var>_right) cmp=1; return cmp;}
bool GameVarEqualGameVar(const GameVar& var, const GameVar& _right){return var==_right;}
int GameVarCmpGameVar(const GameVar& var, const GameVar& _right){int cmp=0; if(var<_right) cmp=-1; else if(var>_right) cmp=1; return cmp;}
#endif // FONLINE_SERVER

/**************************************************************************************************
***************************************************************************************************
**************************************************************************************************/
