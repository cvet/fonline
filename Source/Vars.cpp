#include "StdAfx.h"
#include "Vars.h"
#include "Text.h"

#ifdef FONLINE_SERVER
#include "Critter.h"
#endif

CVarMngr VarMngr;
FileLogger* DbgLog=NULL;

bool CVarMngr::Init(const char* fpath)
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
void CVarMngr::SaveVarsDataFile(void(*save_func)(void*,size_t))
{
	DWORD count=allVars.size();
	save_func(&count,sizeof(count));
	for(VarsMapIt it=allVars.begin(),end=allVars.end();it!=end;++it)
	{
		GameVar* var=(*it).second;
		save_func(&var->VarId,sizeof(var->VarId));
		save_func(&var->VarTemplate->TempId,sizeof(var->VarTemplate->TempId));
		save_func(&var->VarValue,sizeof(var->VarValue));
	}
}

bool CVarMngr::LoadVarsDataFile(FILE* f)
{
	WriteLog("Load vars...");
	allQuestVars.reserve(100000); // 400kb

	DWORD count=0;
	fread(&count,sizeof(count),1,f);
	for(DWORD i=0;i<count;i++)
	{
		ULONGLONG id;
		fread(&id,sizeof(id),1,f);
		WORD temp_id;
		fread(&temp_id,sizeof(temp_id),1,f);
		int val;
		fread(&val,sizeof(val),1,f);

		TemplateVar* tvar=GetTempVar(temp_id);
		if(!tvar)
		{
			WriteLog("Template var not found, tid<%u>.\n",temp_id);
			continue;
		}

		GameVar* var=new GameVar(id,tvar,val);
		if(allVars.count(id))
		{
			char buf[64];
			WriteLog("Id already added, id<%s>.\n",_ui64toa(id,buf,10));
			continue;
		}
		allVars.insert(VarsMapVal(id,var));

		if(tvar->IsQuest())
		{
			var->QuestVarIndex=allQuestVars.size();
			allQuestVars.push_back(var);
		}
	}

	WriteLog("complete, count<%u>.\n",count);
	return true;
}
#endif // FONLINE_SERVER

void CVarMngr::Finish()
{
	WriteLog("Var manager finish...\n");

#ifdef FONLINE_SERVER
	for(VarsMapIt it=allVars.begin(),end=allVars.end();it!=end;++it)
		(*it).second->Release();
	allVars.clear();
	allQuestVars.clear();
#endif

	for(TempVarMapIt it=tempVars.begin(),end=tempVars.end();it!=end;++it)
		delete (*it).second;
	tempVars.clear();
	varsNames.clear();

	varsPath="";
	SAFEDEL(DbgLog);
	isInit=false;
	WriteLog("Var manager finish complete.\n");
}

bool CVarMngr::UpdateVarsTemplate()
{
	WriteLog("Update template vars...");

	string full_path=varsPath+string(VAR_FNAME_VARS);
	FILE* f=fopen(full_path.c_str(),"rb");
	if(!f)
	{
		WriteLog("file<%s> not found.\n",full_path);
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

bool CVarMngr::LoadTemplateVars(FILE* f, TempVarVec& vars)
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

bool CVarMngr::AddTemplateVar(TemplateVar* var)
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

	if(varsNames.count(var->Name))
	{
		WriteLog(__FUNCTION__" - Name already used, name<%s>, id<%u>.\n",var->Name.c_str(),var->TempId);
		return false;
	}

	if(tempVars.count(var->TempId))
	{
		WriteLog(__FUNCTION__" - Id already used, name<%s>, id<%u>.\n",var->Name.c_str(),var->TempId);
		return false;
	}

	tempVars.insert(TempVarMapVal(var->TempId,var));
	varsNames.insert(StrWordMapVal(var->Name,var->TempId));
	return true;
}

void CVarMngr::EraseTemplateVar(WORD temp_id)
{
	TemplateVar* var=GetTempVar(temp_id);
	if(!var) return;

	varsNames.erase(var->Name);
	tempVars.erase(temp_id);
	delete var;
}

WORD CVarMngr::GetTempVarId(const string& var_name)
{
	StrWordMapIt it=varsNames.find(var_name);
	if(it==varsNames.end()) return 0;
	return (*it).second;
}

TemplateVar* CVarMngr::GetTempVar(WORD temp_id)
{
	TempVarMapIt it=tempVars.find(temp_id);
	if(it==tempVars.end()) return NULL;
	return (*it).second;
}

void CVarMngr::SaveTemplateVars()
{
	WriteLog("Save vars...");

	string full_path=varsPath+string(VAR_FNAME_VARS);
	FILE* f=fopen(full_path.c_str(),"wt");
	if(f)
	{
		fprintf(f,"#ifndef __VARS__\n");
		fprintf(f,"#define __VARS__\n");
		fprintf(f,"/*************************************************************************************\n");
		fprintf(f,"***  VARS  *****  COUNT: %08d  ***************************************************\n",tempVars.size());
		fprintf(f,"*************************************************************************************/\n");
		fprintf(f,"\n\n");
		for(TempVarMapIt it=tempVars.begin();it!=tempVars.end();++it)
		{
			TemplateVar* var=(*it).second;

			fprintf(f,"#define ");
			if(var->Type==VAR_GLOBAL) fprintf(f,"GVAR_");
			else if(var->Type==VAR_LOCAL) fprintf(f,"LVAR_");
			else if(var->Type==VAR_UNICUM) fprintf(f,"UVAR_");
			else fprintf(f,"?VAR_");
			fprintf(f,"%s",var->Name.c_str());
			for(int i=0,j=30-var->Name.length();i<j;i++) fprintf(f," ");
			fprintf(f,"(%u)\n",var->TempId);
		}

		fprintf(f,"\n\n");
		fprintf(f,"/*************************************************************************************\n");
		fprintf(f,"\tId\tType\tName\t\tStart\tMin\tMax\tFlags\n");
		fprintf(f,"**************************************************************************************\n");

		for(TempVarMapIt it=tempVars.begin();it!=tempVars.end();++it)
		{
			TemplateVar* var=(*it).second;

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

bool CVarMngr::CheckVar(const string& var_name, DWORD master_id, DWORD slave_id, char oper, int val)
{
	WORD temp_id=GetTempVarId(var_name);
	if(!temp_id) return false;
	GameVar* uvar=GetVar(temp_id,master_id,slave_id,true);
	if(!uvar) return false;
	return CheckVar(uvar,oper,val);
}

bool CVarMngr::CheckVar(WORD template_id, DWORD master_id, DWORD slave_id, char oper, int val)
{
	GameVar* var=GetVar(template_id,master_id,slave_id,true);
	if(!var) return false;
	return CheckVar(var,oper,val);
}

GameVar* CVarMngr::ChangeVar(const string& var_name, DWORD master_id, DWORD slave_id, char oper, int val)
{
	WORD temp_id=GetTempVarId(var_name);
	if(!temp_id) return NULL;
	GameVar* var=GetVar(temp_id,master_id,slave_id,true);
	if(!var) return NULL;
	ChangeVar(var,oper,val);
	return var;
}

GameVar* CVarMngr::ChangeVar(WORD template_id, DWORD master_id, DWORD slave_id, char oper, int val)
{
	GameVar* var=GetVar(template_id,master_id,slave_id,true);
	if(!var) return NULL;
	ChangeVar(var,oper,val);
	return var;
}

/**************************************************************************************************
***************************************************************************************************
**************************************************************************************************/

bool CVarMngr::CheckVar(GameVar* var, char oper, int val)
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

void CVarMngr::ChangeVar(GameVar* var, char oper, int val)
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

GameVar* CVarMngr::GetVar(const string& name, DWORD master_id, DWORD slave_id,  bool create)
{
	WORD temp_id=GetTempVarId(name);
	if(!temp_id) return NULL;
	return GetVar(temp_id,master_id,slave_id,create);
}

GameVar* CVarMngr::GetVar(WORD temp_id, DWORD master_id, DWORD slave_id,  bool create)
{
	TemplateVar* tvar=GetTempVar(temp_id);
	if(!tvar) return NULL;

	switch(tvar->Type)
	{
	case VAR_GLOBAL:
		master_id=0;
		slave_id=0;
		break;
	case VAR_LOCAL:
		if(!master_id) return NULL;
		slave_id=0;
		break;
	case VAR_UNICUM:
		if(!master_id) return NULL;
		if(!slave_id) return NULL;
		break;
	default:
		break;
	}

	master_id&=0xFFFFFF;
	slave_id&=0xFFFFFF;
	ULONGLONG id=0;
	id=((ULONGLONG)master_id<<24)|slave_id;
	id=((ULONGLONG)temp_id<<48)|id;

	bool allocated=false; // DbgLog
	GameVar* var;
	VarsMapIt it=allVars.find(id);
	if(it==allVars.end())
	{
		if(!create) return NULL;
		var=CreateVar(id,tvar);
		if(!var) return NULL;
		allocated=true;
	}
	else
	{
		var=(*it).second;
	}

	if(DbgLog)
	{
		if(tvar->Type==VAR_GLOBAL) DbgLog->Write("Reading gvar<%s> value<%d>.%s\n",tvar->Name.c_str(),var->GetValue(),allocated?" Allocated.":"");
		else if(tvar->Type==VAR_LOCAL) DbgLog->Write("Reading lvar<%s> masterId<%u> value<%d>.%s\n",tvar->Name.c_str(),master_id,var->GetValue(),allocated?" Allocated.":"");
		else if(tvar->Type==VAR_UNICUM) DbgLog->Write("Reading uvar<%s> masterId<%u> slaveId<%u> value<%d>.%s\n",tvar->Name.c_str(),master_id,slave_id,var->GetValue(),allocated?" Allocated.":"");
	}

	return var;
}

GameVar* CVarMngr::CreateVar(ULONGLONG id, TemplateVar* tvar)
{
	if(allVars.count(id))
	{
		char buf[64];
		WriteLog(__FUNCTION__" - Id already added, id<%s>.\n",_ui64toa(id,buf,10));
		return false;
	}

	GameVar* var=new GameVar(id,tvar,tvar->IsRandom()?Random(tvar->MinVal,tvar->MaxVal):tvar->StartVal);
	if(!var) return NULL;

	allVars.insert(VarsMapVal(id,var));
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
	return var;
}

void CVarMngr::SwapVars(DWORD id1, DWORD id2)
{
	/*VarsVec swap_vars;
	for(VarsMapIt it=allVars.begin();it!=allVars.end();)
	{
		GameVar* var=(*it).second;
		TemplateVar* tvar=var->GetTemplateVar();

		if((tvar->Type==VAR_LOCAL && var->GetMasterId()==id1)
		|| (tvar->Type==VAR_UNICUM && (var->GetMasterId()==id1 || var->GetSlaveId()==id1)))
		{
			var->SetVarId(,);
			swap_vars.push_back(var);
			it=allVars.erase(it);
		}
		else ++it;
	}*/
}

DWORD CVarMngr::DeleteVars(DWORD id)
{
	DWORD result=0;
	for(VarsMapIt it=allVars.begin();it!=allVars.end();)
	{
		GameVar* var=(*it).second;
		if((var->Type==VAR_LOCAL && var->GetMasterId()==id) ||
			(var->Type==VAR_UNICUM && (var->GetMasterId()==id || var->GetSlaveId()==id)))
		{
			if(var->IsQuest()) allQuestVars[var->QuestVarIndex]=NULL;
			var->Release();
			it=allVars.erase(it);
			result++;
		}
		else ++it;
	}
	return result;
}

/**************************************************************************************************
***************************************************************************************************
**************************************************************************************************/

void DebugLog(GameVar* var, const char* op, int value)
{
	DWORD master_id=var->GetMasterId();
	DWORD slave_id=var->GetSlaveId();
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
