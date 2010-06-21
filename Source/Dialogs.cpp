#include "StdAfx.h"
#include "Dialogs.h"
#include "Names.h"

DialogManager DlgMngr;

bool DialogManager::LoadDialogs(const char* path, const char* list_name)
{
	WriteLog("Load Dialogs...");

	if(!path || !list_name)
	{
		WriteLog("Null ptr path or list name\n");
		return false;
	}

	char full_path[2048];
	StringCopy(full_path,path);
	strcat(full_path,list_name);

	WriteLog("from list<%s>.\n",full_path);

	FILE* file=fopen(full_path,"rb");
	if(!file)
	{
		WriteLog("Error - Open file failed\n");
		return false;
	}

	DialogPack* pack;
	DWORD dlg_num;
	char dlg_name[128];
	DWORD dlg_len;
	char* dlg_buffer=new char[MAX_DLG_LEN_IN_BYTES+1];
	int dlg_count=0;
	int dlg_loaded=0;
	char ch=0;

	//FILE* fw=fopen("dlgs.fos","wt");

	while(!feof(file))
	{
		if(fscanf(file,"%c",&ch)!=1) break;
		if(ch!='$') continue;

		dlg_count++;

		if(fscanf(file,"%u%s",&dlg_num,dlg_name)!=2)
		{
			WriteLog("Unable to read num and name dialog file name.\n");
			continue;
		}

		//fprintf(fw,"#define DIALOG_%-40s(%d)\n",dlg_name,dlg_num);

		if(DialogsPacks.count(dlg_num))
		{
			WriteLog("Dialogs num is already parse.\n");
			continue;
		}

		StringCopy(full_path,path);
		strcat(full_path,dlg_name);
		strcat(full_path,DIALOG_FILE_EXT);

		FILE* fdlg=fopen(full_path,"rb");
		if(!fdlg)
		{
			WriteLog("Unable to open dialog file, num<%u>, path<%s>.\n",dlg_num,dlg_name);
			continue;
		}

		dlg_len=fread(dlg_buffer,sizeof(char),MAX_DLG_LEN_IN_BYTES-1,fdlg);
		dlg_buffer[dlg_len]=0;

		if(!(pack=ParseDialog(dlg_name,dlg_num,dlg_buffer))) 
		{
			WriteLog("Unable to parse dialog, num<%u>, path<%s>.\n",dlg_num,dlg_name);
			fclose(fdlg);
			continue;
		}

		fclose(fdlg);

		if(!AddDialogs(pack))
		{
			WriteLog("Unable to add dialogs pack, num<%u>, path<%s>.\n",dlg_num,dlg_name);
			continue;
		}

		dlg_loaded++;
	}

	fclose(file);
	//fclose(fw);
	SAFEDELA(dlg_buffer);
	WriteLog("Loading dialogs finish, loaded<%u/%u>.\n",dlg_loaded,dlg_count);
	return dlg_count==dlg_loaded;
}

void DialogManager::SaveList(const char* list_path, const char* list_name)
{
	if(!list_path || !list_name) return;
	char full_path[1024];
	StringCopy(full_path,list_path);
	strcat(full_path,list_name);

	FILE* f;
	if(!(f=fopen(full_path,"wt"))) return;

	fprintf(f,"**************************************************************************************\n");
	fprintf(f,"***  DIALOGS  *********  COUNT: %08d  ********************************************\n",DlgPacksNames.size());
	fprintf(f,"**************************************************************************************\n");

	for(StrDWordMap::iterator it=DlgPacksNames.begin();it!=DlgPacksNames.end();++it)
	{
		fprintf(f,"$\t%u\t%s\n",(*it).second,(*it).first.c_str());
	}

	fprintf(f,"**************************************************************************************\n");
	fprintf(f,"**************************************************************************************\n");
	fprintf(f,"**************************************************************************************\n");

	fclose(f);
}

bool DialogManager::AddDialogs(DialogPack* pack)
{
	if(DialogsPacks.find(pack->PackId)!=DialogsPacks.end()) return false;
	if(DlgPacksNames.find(pack->PackName)!=DlgPacksNames.end()) return false;

	DialogsPacks.insert(DialogPackMap::value_type(pack->PackId,pack));
	DlgPacksNames.insert(StrDWordMap::value_type(string(pack->PackName),pack->PackId));
	return true;
}

DialogPack* DialogManager::GetDialogPack(DWORD num_pack)
{
	DialogPackMap::iterator it=DialogsPacks.find(num_pack);
	return it==DialogsPacks.end()?NULL:(*it).second;
}

DialogsVec* DialogManager::GetDialogs(DWORD num_pack)
{
	//	DialogsVecIt it=std::find(DialogsPacks.begin(),DialogsPacks.end(),num_pack);
	//	return it!=DialogsPacks.end()?&(*it):NULL;
	DialogPackMap::iterator it=DialogsPacks.find(num_pack);
	return it==DialogsPacks.end()?NULL:&(*it).second->Dialogs;
}

void DialogManager::EraseDialogs(DWORD num_pack)
{
	DialogPackMap::iterator it=DialogsPacks.find(num_pack);
	if(it==DialogsPacks.end()) return;

	DlgPacksNames.erase((*it).second->PackName);
	delete (*it).second;
	DialogsPacks.erase(it);
}

void DialogManager::EraseDialogs(string name_pack)
{
	StrDWordMap::iterator it=DlgPacksNames.find(name_pack);
	if(it==DlgPacksNames.end()) return;
	EraseDialogs((*it).second);
}

void DialogManager::Finish()
{
	WriteLog("Dialog manager finish...\n");
	DialogsPacks.clear();
	DlgPacksNames.clear();
	WriteLog("Dialog manager finish Ok.\n");
}

string ParseLangKey(const char* str)
{
	while(*str==' ') str++;
	return string(str);
}

DialogPack* DialogManager::ParseDialog(const char* name, DWORD id, const char* data)
{
	LastErrors="";

	if(!data)
	{
		AddError("Data nullptr.");
		return NULL;
	}

	IniParser fodlg;
	if(!fodlg.LoadFile((BYTE*)data,strlen(data)))
	{
		AddError("Internal error.");
		return NULL;
	}

#define LOAD_FAIL(err) {AddError(err); goto load_false;}
	DialogPack* pack=new DialogPack(id,string(name));
	char* dlg_buf=fodlg.GetApp("dialog");
	istrstream input(dlg_buf,strlen(dlg_buf));
	char* lang_buf=NULL;
	pack->PackId=id;
	pack->PackName=name;

	// Comment
	char* comment=fodlg.GetApp("comment");
	if(comment) pack->Comment=comment;
	SAFEDELA(comment);

	// Texts
	char lang_key[256];
	if(!fodlg.GetStr("data","lang","russ",lang_key)) LOAD_FAIL("Lang app not found.");

	StrVec& lang=pack->TextsLang;
	Str::ParseLine<StrVec,string(*)(const char*)>(lang_key,' ',lang,ParseLangKey);
	if(!lang.size()) LOAD_FAIL("Lang app is empty.");

	for(int i=0,j=lang.size();i<j;i++)
	{
		string& l=lang[i];
		lang_buf=fodlg.GetApp(l.c_str());
		if(!lang_buf) LOAD_FAIL("One of the lang section not found.");
		pack->Texts.push_back(new FOMsg);
		pack->Texts[i]->LoadMsgFile(lang_buf,strlen(lang_buf));
		SAFEDELA(lang_buf);
	}

	// Dialog
	if(!dlg_buf) LOAD_FAIL("Dialog section not found.");

	char ch;
	input >> ch;
	if(ch!='&') return NULL;

	DWORD dlg_id; 
	DWORD text_id;
	DWORD link;
	char read_str[256];

#ifdef FONLINE_NPCEDITOR
	char script[1024];
#else
	int script;
#endif
	DWORD flags;

	while(true)
	{
		input >> dlg_id;
		if(input.eof()) goto load_done;
		if(input.fail()) LOAD_FAIL("Bad dialog id number.");
		input >> text_id;
		if(input.fail()) LOAD_FAIL("Bad text link.");
		input >> read_str;
		if(input.fail()) LOAD_FAIL("Bad not answer action.");
#ifdef FONLINE_NPCEDITOR
		StringCopy(script,read_str);
		if(!_stricmp(script,"NOT_ANSWER_CLOSE_DIALOG")) StringCopy(script,"None");
		else if(!_stricmp(script,"NOT_ANSWER_BEGIN_BATTLE")) StringCopy(script,"Attack");
#else
		script=GetNotAnswerAction(read_str);
		if(script<0)
		{
			WriteLog("Unable to parse<%s>.\n",read_str);
			LOAD_FAIL("Invalid not answer action.");
		}
#endif
		input >> flags;
		if(input.fail()) LOAD_FAIL("Bad flags.");

		Dialog current_dialog;
		current_dialog.Id=dlg_id;
		current_dialog.TextId=text_id;
		current_dialog.DlgScript=script;
		current_dialog.Flags=flags; 

		// Read answers
		input >> ch;
		if(input.fail()) LOAD_FAIL("Dialog corrupted.");
		if(ch=='@') // End of current dialog node
		{
			pack->Dialogs.push_back(current_dialog);
			continue;
		}
		if(ch=='&') // End of all
		{
			pack->Dialogs.push_back(current_dialog);
			break;
		}
		if(ch!='#') LOAD_FAIL("Parse error0.");

		while(!input.eof())
		{
			input >> link;
			if(input.fail()) LOAD_FAIL("Bad link in dialog.");
			input >> text_id;
			if(input.fail()) LOAD_FAIL("Bad text link in dialog.");
			DialogAnswer current_answer;
			current_answer.Link=link;
			current_answer.TextId=text_id;

			bool read_demands=true; // Deprecated
			while(true)
			{
				input >> ch;
				if(input.fail()) LOAD_FAIL("Parse answer character fail.");

				// Demands, results; Deprecated
				if(ch=='d') read_demands=true;
				else if(ch=='r') read_demands=false;
				else if(ch=='*')
				{
					DemandResult* dr=LoadDemandResult(input,read_demands);
					if(!dr) LOAD_FAIL("Demand or result not loaded.");
					if(read_demands) current_answer.Demands.push_back(*dr);
					else current_answer.Results.push_back(*dr);
				}
				// Demands
				else if(ch=='D')
				{
					DemandResult* d=LoadDemandResult(input,true);
					if(!d) LOAD_FAIL("Demand not loaded.");
					current_answer.Demands.push_back(*d);
				}
				// Results
				else if(ch=='R')
				{
					DemandResult* r=LoadDemandResult(input,false);
					if(!r) LOAD_FAIL("Result not loaded.");
					current_answer.Results.push_back(*r);
				}
				// Flags
				/*else if(ch=='F')
				{
					input >> read_str;
					if(input.fail()) LOAD_FAIL("Parse flags fail.");
					if(!_stricmp(read_str,"no_recheck_demand") && current_dialog.Answers.size()) current_answer.NoRecheck=true;
				}*/
				else break;
			}
			current_dialog.Answers.push_back(current_answer);

			if(ch=='#') continue; // Next
			if(ch=='@') break; // End of current dialog node
			if(ch=='&') // End of all
			{
				pack->Dialogs.push_back(current_dialog);
				goto load_done;
			}
		}
		pack->Dialogs.push_back(current_dialog);
	}

load_done:
	SAFEDELA(dlg_buf);
	SAFEDELA(lang_buf);
	return pack;

load_false:
	AddError("Bad node<%d>.",dlg_id);
	WriteLog(__FUNCTION__" - Errors:\n%s",LastErrors.c_str());
	delete pack;
	SAFEDELA(dlg_buf);
	SAFEDELA(lang_buf);
	return NULL;
}

DemandResult* DialogManager::LoadDemandResult(istrstream& input, bool is_demand)
{
	int errors=0;
	char who='p';
	char oper='=';
	int values_count=0;
	int value=0;
	int id=0;
	char type_str[256];
	char name[256]={0};
	bool no_recheck=false;

#ifdef FONLINE_NPCEDITOR
	string script_val[5];
#else
	int script_val[5]={0,0,0,0,0};
#endif

	input >> type_str;
	if(input.fail())
	{
		AddError("Parse dr type fail.");
		return NULL;
	}
	int type=GetDRType(type_str);

	if(type==DR_NO_RECHECK)
	{
		no_recheck=true;
		input >> type_str;
		if(input.fail())
		{
			AddError("Parse dr type fail2.");
			return NULL;
		}
		type=GetDRType(type_str);
	}

	switch(type)
	{
	case DR_PARAM:
		{
			// Who
			if(_stricmp(type_str,"loy") && _stricmp(type_str,"kill")) // Deprecated
			{
				input >> who;
				if(!CheckWho(who))
				{
					AddError("Invalid DR param who<%c>.",who);
					errors++;
				}
			}
			// Name
			input >> name;
			if((id=FONames::GetParamId(name))<0)
			{
				AddError("Invalid DR parameter<%s>.",name);
				errors++;
			}
			// Operator
			input >> oper;
			if(!CheckOper(oper))
			{
				AddError("Invalid DR param oper<%c>.",oper);
				errors++;
			}
			// Value
			input >> value;
		}
		break;
	case DR_VAR:
		{
			// Who
			input >> who;
			if(!CheckWho(who))
			{
				AddError("Invalid DR var who<%c>.",who);
				errors++;
			}
			// Name
			input >> name;
			if((id=GetTempVarId(name))==0)
			{
				AddError("Invalid DR var name<%s>.",name);
				errors++;
			}
			// Operator
			input >> oper;
			if(!CheckOper(oper))
			{
				AddError("Invalid DR var oper<%c>.",oper);
				errors++;
			}
			// Value
			input >> value;
		}
		break;
	case DR_ITEM:
		{
			// Who
			input >> who;
			if(!CheckWho(who))
			{
				AddError("Invalid DR item who<%c>.",who);
				errors++;
			}
			// Name
			input >> name;
			id=FONames::GetItemPid(name);
			if(id==-1)
			{
				id=atoi(name);
				const char* name_=FONames::GetItemName(id);
				if(!name_)
				{
					AddError("Invalid DR item<%s>.",name);
					errors++;
				}
				if(name_!=0) StringCopy(name,name_);
			}
			// Operator
			input >> oper;
			if(!CheckOper(oper))
			{
				AddError("Invalid DR item oper<%c>.",oper);
				errors++;
			}
			// Value
			input >> value;
		}
		break;
	case DR_SCRIPT:
		{
			// Script name
			input >> name;
			// Operator, not used
			if(_stricmp(type_str,"_script") && is_demand) input >> oper; // Deprecated
			// Values count
			input >> values_count;
			// Values
			if(!_stricmp(type_str,"_script"))
			{
#ifdef FONLINE_NPCEDITOR
#define READ_SCRIPT_VALUE_(val) {input >> value_str; val=value_str;}
#else
#define READ_SCRIPT_VALUE_(val) {input >> value_str; val=FONames::GetDefineValue(value_str);}
#endif
				char value_str[256];
				if(values_count>0) READ_SCRIPT_VALUE_(script_val[0]);
				if(values_count>1) READ_SCRIPT_VALUE_(script_val[1]);
				if(values_count>2) READ_SCRIPT_VALUE_(script_val[2]);
				if(values_count>3) READ_SCRIPT_VALUE_(script_val[3]);
				if(values_count>4) READ_SCRIPT_VALUE_(script_val[4]);
				if(values_count>5)
				{
					AddError("Invalid DR script values count<%d>.",values_count);
					values_count=0;
					errors++;
				}
			}
			else // Deprecated
			{
				char ch=*input.str();
				input.rdbuf()->freeze(false);
				if(ch==' ')
				{
#ifdef FONLINE_NPCEDITOR
#define READ_SCRIPT_VALUE(val) {input >> value_int; char buf[64]; val=itoa(value_int,buf,10);}
#else
#define READ_SCRIPT_VALUE(val) {input >> value_int; val=value_int;}
#endif
					int value_int;
					if(values_count>0) READ_SCRIPT_VALUE(script_val[0]);
					if(values_count>1) READ_SCRIPT_VALUE(script_val[1]);
					if(values_count>2) READ_SCRIPT_VALUE(script_val[2]);
					if(values_count>3) READ_SCRIPT_VALUE(script_val[3]);
					if(values_count>4) READ_SCRIPT_VALUE(script_val[4]);
					if(values_count>5)
					{
						AddError("Invalid DR script values count<%d>.",values_count);
						values_count=0;
						errors++;
					}
				}
				else
				{
#ifdef FONLINE_NPCEDITOR
					char buf[64];
					script_val[0]=_itoa(values_count,buf,10);
#else
					script_val[0]=values_count;
#endif
					values_count=1;
				}
			}

#ifdef FONLINE_SERVER
			// Bind function
#define BIND_D_FUNC(params) {id=Script::Bind(name,"bool %s(Critter&,Critter@"params,false);}
#define BIND_R_FUNC(params) {id=Script::Bind(name,"void %s(Critter&,Critter@"params,false);}
			switch(values_count)
			{
			case 1: if(is_demand) BIND_D_FUNC(",int)") else BIND_R_FUNC(",int)") break;
			case 2: if(is_demand) BIND_D_FUNC(",int,int)") else BIND_R_FUNC(",int,int)") break;
			case 3: if(is_demand) BIND_D_FUNC(",int,int,int)") else BIND_R_FUNC(",int,int,int)") break;
			case 4: if(is_demand) BIND_D_FUNC(",int,int,int,int)") else BIND_R_FUNC(",int,int,int,int)") break;
			case 5: if(is_demand) BIND_D_FUNC(",int,int,int,int,int)") else BIND_R_FUNC(",int,int,int,int,int)") break;
			default: if(is_demand) BIND_D_FUNC(")") else BIND_R_FUNC(")") break;
			}
			if(id<=0)
			{
				WriteLog(__FUNCTION__" - Script<%s> bind error.\n",name);
				return NULL;
			}
			if(id>0xFFFF) WriteLog(__FUNCTION__" - Id greater than 0xFFFF.\n");
#endif
		}
		break;
	case DR_LOCK:
		{
			input >> value;
			if(!CheckLockTime(value))
			{
				AddError("Invalid DR lock time<%d>.",value);
				errors++;
			}
		}
		break;
	case DR_OR:
		break;
	default:
		return NULL;
	}

	// Validate parsing
	if(input.fail())
	{
		AddError("DR parse fail.");
		errors++;
	}

	// Fill
	static DemandResult result;
	result.Type=type;
	result.Who=who;
	result.ParamId=id;
	result.Op=oper;
	result.ValuesCount=values_count;
	result.Value=value;
	result.NoRecheck=no_recheck;
#ifdef FONLINE_NPCEDITOR
	result.ParamName=name;
	result.ValuesNames[0]=script_val[0];
	result.ValuesNames[1]=script_val[1];
	result.ValuesNames[2]=script_val[2];
	result.ValuesNames[3]=script_val[3];
	result.ValuesNames[4]=script_val[4];
#else
	result.ValueExt[0]=script_val[0];
	result.ValueExt[1]=script_val[1];
	result.ValueExt[2]=script_val[2];
	result.ValueExt[3]=script_val[3];
	result.ValueExt[4]=script_val[4];
	if(errors) return NULL;
#endif
	return &result;
} 

WORD DialogManager::GetTempVarId(const char* str)
{
	WORD tid=VarMngr.GetTempVarId(string(str));
	if(!tid) WriteLog(__FUNCTION__" - Template var not found, name<%s>.\n",str);
	return tid;
}

bool DialogManager::CheckLockTime(int time)
{
	return time>=LOCK_TIME_MIN && time<=LOCK_TIME_MAX;
}

int DialogManager::GetNotAnswerAction(const char * str)
{
	if(!_stricmp(str,"NOT_ANSWER_CLOSE_DIALOG") || !_stricmp(str,"None"))
		return NOT_ANSWER_CLOSE_DIALOG;
	else if(!_stricmp(str,"NOT_ANSWER_BEGIN_BATTLE") || !_stricmp(str,"Attack"))
		return NOT_ANSWER_BEGIN_BATTLE;
#ifdef FONLINE_SERVER
	else
		return Script::Bind(str,"void %s(Critter&,Critter@,string@)",false); //Bind function
#endif // FONLINE_SERVER

	return -1;
}

int DialogManager::GetDRType(const char* str)
{
	if(!str)
	{
		WriteLog(__FUNCTION__" - Invalid argument.\n");
		return DR_NONE;
	}

	if(!_stricmp(str,"_param"))          return DR_PARAM;
	else if(!_stricmp(str,"_item"))      return DR_ITEM;
	else if(!_stricmp(str,"_lock"))      return DR_LOCK;
	else if(!_stricmp(str,"_script"))    return DR_SCRIPT;
	else if(!_stricmp(str,"_var"))       return DR_VAR;
	else if(!_stricmp(str,"no_recheck")) return DR_NO_RECHECK;
	else if(!_stricmp(str,"or"))         return DR_OR;
	else if(!_stricmp(str,"stat"))  return DR_PARAM;  // Deprecated
	else if(!_stricmp(str,"skill")) return DR_PARAM;  // Deprecated
	else if(!_stricmp(str,"perk"))  return DR_PARAM;  // Deprecated
	else if(!_stricmp(str,"var"))   return DR_VAR;    // Deprecated
	else if(!_stricmp(str,"gvar"))  return DR_VAR;    // Deprecated
	else if(!_stricmp(str,"lvar"))  return DR_VAR;    // Deprecated
	else if(!_stricmp(str,"uvar"))  return DR_VAR;    // Deprecated
	else if(!_stricmp(str,"item"))  return DR_ITEM;   // Deprecated
	else if(!_stricmp(str,"lock"))  return DR_LOCK;   // Deprecated
	else if(!_stricmp(str,"script"))return DR_SCRIPT; // Deprecated
	else if(!_stricmp(str,"kill"))  return DR_PARAM;  // Deprecated
	else if(!_stricmp(str,"loy"))   return DR_PARAM;  // Deprecated
	return DR_NONE;
}

bool DialogManager::CheckOper(char oper)
{
	return oper=='>' || oper=='<' || oper=='=' || oper=='+' || oper=='-' || oper=='*'
		|| oper=='/' || oper=='=' || oper=='!' || oper=='}' || oper=='{';
}

bool DialogManager::CheckWho(char who)
{
	return who=='p' || who=='n';
}

void DialogManager::AddError(const char* fmt, ...)
{
	static char res[1024];

	va_list list;
	va_start(list,fmt);
	vsprintf(res,fmt,list);
	va_end(list);

	LastErrors+=res;
	LastErrors+="\n";
}

