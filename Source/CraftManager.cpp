#include "StdAfx.h"
#include "CraftManager.h"
#include "Names.h"
#include "FileManager.h"

#ifdef FONLINE_SERVER
#include "Critter.h"
#include "CritterManager.h"
#include "ItemManager.h"
#endif

#ifdef FONLINE_CLIENT
#include "CritterCl.h"
#endif

// Fix boy function call states
#define FIXBOY_LIST                (0)
#define FIXBOY_BUTTON              (1)
#define FIXBOY_CRAFT               (2)
// Fix boy craft results
#define FIXBOY_ALLOW_CRAFT         (0x0001)
#define FIXBOY_CHECK_TIMEOUT       (0x0002)
#define FIXBOY_SET_TIMEOUT         (0x0004)
#define FIXBOY_CHECK_PARAMS        (0x0008)
#define FIXBOY_CHECK_MATERIALS     (0x0010)
#define FIXBOY_CHECK_TOOLS         (0x0020)
#define FIXBOY_SUB_MATERIALS       (0x0040)
#define FIXBOY_ADD_CRAFT_ITEMS     (0x0080)
#define FIXBOY_ADD_EXPERIENCE      (0x0100)
#define FIXBOY_SEND_SUCC_MESSAGE   (0x0200)
#define FIXBOY_SEND_FAIL_MESSAGE   (0x0400)
#define FIXBOY_DEFAULT             (0xFFFF)
#define CRAFT_RETURN_FAIL          {if(FLAG(flags,FIXBOY_SEND_FAIL_MESSAGE)) return CRAFT_RESULT_FAIL; return CRAFT_RESULT_NONE;}
#define CRAFT_RETURN_SUCC          {if(FLAG(flags,FIXBOY_SEND_SUCC_MESSAGE)) return CRAFT_RESULT_SUCC; return CRAFT_RESULT_NONE;}
#define CRAFT_RETURN_TIMEOUT       {if(FLAG(flags,FIXBOY_SEND_FAIL_MESSAGE)) return CRAFT_RESULT_TIMEOUT; return CRAFT_RESULT_NONE;}

CraftManager MrFixit;


CraftItem::CraftItem()
{
	Num=0;
	Name="";
	Info="";
}

CraftItem& CraftItem::operator=(const CraftItem& _right)
{
	// Number, name, info
	Num=_right.Num;
	Name=_right.Name;
	Info=_right.Info;

	// Need parameters to show craft
	ShowPNum=_right.ShowPNum;
	ShowPVal=_right.ShowPVal;
	ShowPOr=_right.ShowPOr;

	// Need parameters to craft
	NeedPNum=_right.NeedPNum;
	NeedPVal=_right.NeedPVal;
	NeedPOr=_right.NeedPOr;

	// Need items to craft
	NeedItems=_right.NeedItems;
	NeedItemsVal=_right.NeedItemsVal;
	NeedItemsOr=_right.NeedItemsOr;

	// Need tools to craft
	NeedTools=_right.NeedTools;
	NeedToolsVal=_right.NeedToolsVal;
	NeedToolsOr=_right.NeedToolsOr;

	// New items
	OutItems=_right.OutItems;
	OutItemsVal=_right.OutItemsVal;

	// Other
	Script=_right.Script;
	ScriptBindId=_right.ScriptBindId;
	Experience=_right.Experience;
	return *this;
}

bool CraftItem::IsValid()
{
	if(!Num) return false;
	if(!Name.length()) return false;
	if(NeedItems.empty()) return false;
	if(OutItems.empty()) return false;
	return true;
}

void CraftItem::Clear()
{
	Num=0;
	Name="";
	Info="";
	ShowPNum.clear();
	ShowPVal.clear();
	NeedPNum.clear();
	NeedPVal.clear();
	NeedItems.clear();
	NeedItemsVal.clear();
	NeedTools.clear();
	NeedToolsVal.clear();
	OutItems.clear();
	OutItemsVal.clear();
	Script.clear();
	ScriptBindId=0;
	Experience=0;
}

#ifdef FONLINE_CLIENT
void CraftItem::SetName(FOMsg& msg_game, FOMsg& msg_item)
{
	Name="";

	int i,j;
	ProtoItem* proto;
/*
//Need items
	for(i=0,j=NeedItems.size();i<j;i++)
	{
		proto=ItemMngr.GetProtoItem(NeedItems[i]);

		if(!proto)
			Name+="???";
		else
			Name+=msg_item.GetStr(proto->NameId);

		if(NeedItemsVal[i]>1)
		{
			Name+=" ";
			Name+=Str::DWtoA(NeedItemsVal[i]);
			Name+=" шт.";
		}

		if(i==j-1) break;

		if(NeedItemsOr[i])
			Name+=" или ";
		else
			Name+=" и ";
	}

	Name+=" => ";
*/
//Out items
	for(i=0,j=OutItems.size();i<j;i++)
	{
		proto=ItemMngr.GetProtoItem(OutItems[i]);

		if(!proto)
			Name+="???";
		else
			Name+=msg_item.GetStr(proto->GetInfo());

		if(OutItemsVal[i]>1)
		{
			Name+=" ";
			Name+=Str::DWtoA(OutItemsVal[i]);
			Name+=" ";
			Name+=msg_game.GetStr(STR_FIX_PIECES);
		}

		if(i==j-1) break;
		Name+=msg_game.GetStr(STR_AND);
	}
}
#endif // FONLINE_CLIENT

template<class T>
void SetStrMetadata(T& v, const char*& str)
{
	int val;
	if(sscanf(str,"%d",&val)!=1) return;
	for(str++;*str && *str!=' ';str++);
	for(int i=0,j=val;i<j;i++)
	{
		if(sscanf(str,"%d",&val)!=1) return;
		for(str++;*str && *str!=' ';str++);
		v.push_back(val);
	}
}

template<class T>
void GetStrMetadata(T& v, char* str)
{
	char buf[32];
	StringAppend(str,MAX_FOTEXT,_itoa(v.size(),buf,10));
	StringAppend(str,MAX_FOTEXT," ");
	for(int i=0;i<v.size();i++)
	{
		StringAppend(str,MAX_FOTEXT,_itoa(v[i],buf,10));
		StringAppend(str,MAX_FOTEXT," ");
	}
}

int CraftItem::SetStr(DWORD num, const char* str_in)
{
	// Prepare
	const char* pstr_in=str_in;
	char str[MAX_FOTEXT];
	char* pstr;
	bool metadata=false;

	Clear();
	if(!pstr_in || !*pstr_in) return -1;

	if(*pstr_in==MRFIXIT_METADATA)
	{
		metadata=true;
		pstr_in++;
	}

	// Set number
	Num=num;

	// Parse name
	pstr=str;
	while(*pstr_in!=MRFIXIT_NEXT)
	{
		if(!*pstr_in) return -2;

		*pstr=*pstr_in;
		pstr_in++;
		pstr++;
	}

	pstr_in++;
	*pstr='\0';
	Name=str;

	// Parse info
	pstr=str;
	while(*pstr_in!=MRFIXIT_NEXT)
	{
		if(!*pstr_in) return -3;

		*pstr=*pstr_in;
		pstr_in++;
		pstr++;
	}

	pstr_in++;
	*pstr='\0';
	Info=str;

	if(metadata)
	{
		SetStrMetadata(ShowPNum,pstr_in);
		SetStrMetadata(ShowPVal,pstr_in);
		SetStrMetadata(ShowPOr,pstr_in);
		SetStrMetadata(NeedPNum,pstr_in);
		SetStrMetadata(NeedPVal,pstr_in);
		SetStrMetadata(NeedPOr,pstr_in);
		SetStrMetadata(NeedItems,pstr_in);
		SetStrMetadata(NeedItemsVal,pstr_in);
		SetStrMetadata(NeedItemsOr,pstr_in);
		SetStrMetadata(NeedTools,pstr_in);
		SetStrMetadata(NeedToolsVal,pstr_in);
		SetStrMetadata(NeedToolsOr,pstr_in);
		SetStrMetadata(OutItems,pstr_in);
		SetStrMetadata(OutItemsVal,pstr_in);
		if(strstr(pstr_in,"script")) Script="true";
		else Script="";
		return 0;
	}

#if defined(FONLINE_SERVER) || defined(FONLINE_MRFIXIT)
	// Parse show params
	int res=SetStrParam(pstr_in,ShowPNum,ShowPVal,ShowPOr);
	if(res<0) return num-10;

	// Parse need params
	res=SetStrParam(pstr_in,NeedPNum,NeedPVal,NeedPOr);
	if(res<0) return num-20;

	// Parse need items
	res=SetStrItem(pstr_in,NeedItems,NeedItemsVal,NeedItemsOr);
	if(res<0) return num-30;

	// Parse need tools
	res=SetStrItem(pstr_in,NeedTools,NeedToolsVal,NeedToolsOr);
	if(res<0) return num-40;

	// Parse out items
	ByteVec tmp_vec;
	res=SetStrItem(pstr_in,OutItems,OutItemsVal,tmp_vec);
	if(res<0) return num-50;

	// Parse script
	if(strstr(pstr_in,"script ")) Script=strstr(pstr_in,"script ")+strlen("script ");
	else Script="";

	// Experience
	if(strstr(pstr_in,"exp ")) Experience=atoi(strstr(pstr_in,"exp ")+strlen("exp "));
	else Experience=0;
#endif
	return 0;
}

#if defined(FONLINE_SERVER) || defined(FONLINE_MRFIXIT)
int CraftItem::SetStrParam(const char*& pstr_in, DwordVec& num_vec, IntVec& val_vec, ByteVec& or_vec)
{
	char str[MAX_FOTEXT];
	char* pstr=str;

	int param_num;
	char param_name[128];
	int param_val;

	param_name[0]='\0';

	while(*pstr_in!=MRFIXIT_NEXT)
	{
		if(!*pstr_in) return -1;

		*pstr=*pstr_in;
		pstr_in++;
		pstr++;

		if(*pstr_in==MRFIXIT_SPACE)
		{
			*pstr='\0';

			if((param_num=FONames::GetParamId(str))<0) return -3;

			pstr=str;
			pstr_in++;
		}
		else if(*pstr_in==MRFIXIT_AND || *pstr_in==MRFIXIT_OR || *pstr_in==MRFIXIT_NEXT)
		{
			*pstr='\0';
			param_val=atoi(str);

			num_vec.push_back(param_num);
			val_vec.push_back(param_val);

			if(*pstr_in==MRFIXIT_AND) or_vec.push_back(0);
			else if(*pstr_in==MRFIXIT_OR) or_vec.push_back(1);

			if(*pstr_in==MRFIXIT_NEXT) break;

			param_name[0]='\0';
			pstr=str;
			pstr_in++;
		}
	}

	or_vec.push_back(0);
	pstr_in++;

	return 0;
}

int CraftItem::SetStrItem(const char*& pstr_in, WordVec& pid_vec, DwordVec& count_vec, ByteVec& or_vec)
{
	char str[MAX_FOTEXT];
	char* pstr=str;

	int item_pid=0;
	DWORD item_count;

	while(*pstr_in!=MRFIXIT_NEXT)
	{
		if(!*pstr_in) return -1;

		*pstr=*pstr_in;
		pstr_in++;
		pstr++;

		if(*pstr_in==MRFIXIT_SPACE)
		{
			*pstr='\0';

			item_pid=FONames::GetItemPid(str);
			if(item_pid==-1) return -2;

			pstr=str;
			pstr_in++;
		}
		else if(*pstr_in==MRFIXIT_AND || *pstr_in==MRFIXIT_OR || *pstr_in==MRFIXIT_NEXT)
		{
			*pstr='\0';

			item_count=atoi(str);
			if(!item_count) return -3;	

			pid_vec.push_back(item_pid);
			count_vec.push_back(item_count);

			if(*pstr_in==MRFIXIT_AND) or_vec.push_back(0);
			else if(*pstr_in==MRFIXIT_OR) or_vec.push_back(1);

			if(*pstr_in==MRFIXIT_NEXT) break;

			pstr=str;
			pstr_in++;
		}
	}

	or_vec.push_back(0);
	pstr_in++;

	return 0;
}
#endif

const char* CraftItem::GetStr(bool metadata)
{
	static THREAD char str[MAX_FOTEXT];

	if(metadata)
	{
		sprintf(str,"%c%s%c%s%c",MRFIXIT_METADATA,Name.c_str(),MRFIXIT_NEXT,Info.c_str(),MRFIXIT_NEXT);
		GetStrMetadata(ShowPNum,str);
		GetStrMetadata(ShowPVal,str);
		GetStrMetadata(ShowPOr,str);
		GetStrMetadata(NeedPNum,str);
		GetStrMetadata(NeedPVal,str);
		GetStrMetadata(NeedPOr,str);
		GetStrMetadata(NeedItems,str);
		GetStrMetadata(NeedItemsVal,str);
		GetStrMetadata(NeedItemsOr,str);
		GetStrMetadata(NeedTools,str);
		GetStrMetadata(NeedToolsVal,str);
		GetStrMetadata(NeedToolsOr,str);
		GetStrMetadata(OutItems,str);
		GetStrMetadata(OutItemsVal,str);
		if(Script.length()) StringAppend(str,"script");
		return str;
	}

#if defined(FONLINE_SERVER) || defined(FONLINE_MRFIXIT)
	// Name, info
	sprintf(str,"%s%c%s%c",Name.c_str(),MRFIXIT_NEXT,Info.c_str(),MRFIXIT_NEXT);

	// Need params to show
	GetStrParam(str,ShowPNum,ShowPVal,ShowPOr);

	// Need params to craft
	GetStrParam(str,NeedPNum,NeedPVal,NeedPOr);

	// Need items to craft
	GetStrItem(str,NeedItems,NeedItemsVal,NeedItemsOr);

	// Need tools to craft
	GetStrItem(str,NeedTools,NeedToolsVal,NeedToolsOr);

	// New items
	ByteVec or_vec; //Temp vector
	for(int i=0;i<OutItems.size();i++) or_vec.push_back(0);
	GetStrItem(str,OutItems,OutItemsVal,or_vec);

	// Experience
	if(Experience)
	{
		char buf[32];
		StringAppend(str,"exp ");
		StringAppend(str,_itoa(Experience,buf,10));
		StringAppend(str," ");
	}

	// Script
	if(Script.length())
	{
		StringAppend(str,"script ");
		StringAppend(str,Script.c_str());
	}
#endif
	return str;
}

#if defined(FONLINE_SERVER) || defined(FONLINE_MRFIXIT)
void CraftItem::GetStrParam(char* pstr_out, DwordVec& num_vec, IntVec& val_vec, ByteVec& or_vec)
{
	for(size_t i=0,j=num_vec.size();i<j;i++)
	{
		const char* s=FONames::GetParamName(num_vec[i]);
		if(!s) continue;

		char str[128];
		sprintf(str,"%s%c%d",s,MRFIXIT_SPACE,val_vec[i]);

		if(i!=j-1)
		{
			if(or_vec[i]) StringAppend(str,MAX_FOTEXT,MRFIXIT_OR_S);
			else StringAppend(str,MAX_FOTEXT,MRFIXIT_AND_S);
		}

		StringAppend(pstr_out,MAX_FOTEXT,str);
	}

	StringAppend(pstr_out,MAX_FOTEXT,MRFIXIT_NEXT_S);
}

void CraftItem::GetStrItem(char* pstr_out, WordVec& pid_vec, DwordVec& count_vec, ByteVec& or_vec)
{
	for(size_t i=0,j=pid_vec.size();i<j;i++)
	{
		const char* s=FONames::GetItemName(pid_vec[i]);
		if(!s) continue;

		char str[128];
		sprintf(str,"%s%c%u",s,MRFIXIT_SPACE,count_vec[i]);

		if(i!=j-1)
		{
			if(or_vec[i]) StringAppend(str,MRFIXIT_OR_S);
			else StringAppend(str,MRFIXIT_AND_S);
		}

		StringAppend(pstr_out,MAX_FOTEXT,str);
	}

	StringAppend(pstr_out,MAX_FOTEXT,MRFIXIT_NEXT_S);
}
#endif

bool CraftManager::operator==(const CraftManager& r)
{
	if(itemCraft.size()!=r.itemCraft.size())
	{
		WriteLog(__FUNCTION__" - Different sizes.\n");
		return false;
	}

	CraftItemMapIt it=itemCraft.begin(),end=itemCraft.end();
	CraftItemMapIt it_=const_cast<CraftManager&>(r).itemCraft.begin(),end_=const_cast<CraftManager&>(r).itemCraft.end();
	for(;it!=end;++it,++it_)
	{
		CraftItem* craft=(*it).second;
		CraftItem* rcraft=(*it_).second;

#define COMPARE_CRAFT(p) if(craft->p!=rcraft->p){WriteLog(__FUNCTION__" - Different<"#p">, craft<%u>.\n",craft->Num); return false;}
		COMPARE_CRAFT(Num);
		COMPARE_CRAFT(ShowPNum);
		COMPARE_CRAFT(ShowPVal);
		COMPARE_CRAFT(ShowPOr);
		COMPARE_CRAFT(NeedPNum);
		COMPARE_CRAFT(NeedPVal);
		COMPARE_CRAFT(NeedPOr);
		COMPARE_CRAFT(NeedItems);
		COMPARE_CRAFT(NeedItemsVal);
		COMPARE_CRAFT(NeedItemsOr);
		COMPARE_CRAFT(NeedTools);
		COMPARE_CRAFT(NeedToolsVal);
		COMPARE_CRAFT(NeedToolsOr);
		COMPARE_CRAFT(OutItems);
		COMPARE_CRAFT(OutItemsVal);
		COMPARE_CRAFT(Script);
		COMPARE_CRAFT(Experience);
	}
	return true;
}

#ifdef FONLINE_MRFIXIT
bool CraftManager::LoadCrafts(const char* path)
{
	FOMsg msg;
	if(msg.LoadMsgFile(path,-1)<0) return false;
	return LoadCrafts(msg);
}

bool CraftManager::SaveCrafts(const char* path)
{
	FOMsg msg;
	CraftItemMapIt it=itemCraft.begin();
	CraftItemMapIt it_end=itemCraft.end();

	CraftItem* craft;
	string str;
	for(;it!=it_end;++it)
	{
		craft=(*it).second;
		msg.AddStr(craft->Num,string(craft->GetStr(false)));
	}

	if(msg.SaveMsgFile(path,-1)<0) return false;
	return true;
}

void CraftManager::EraseCraft(DWORD num)
{
	if(!IsCraftExist(num)) return;

	itemCraft.erase(num);
}
#endif // FONLINE_MRFIXIT

bool CraftManager::LoadCrafts(FOMsg& msg)
{
	int load_fail=0;
	DwordStrMulMap& msg_str=msg.GetData();
	DwordStrMulMapIt it=msg_str.begin();
	DwordStrMulMapIt it_end=msg_str.end();

	it++; // Skip ERROR

	for(;it!=it_end;++it)
	{
		DWORD num=(*it).first;
		string& str=(*it).second;
		
		if(!AddCraft(num,str.c_str()))
		{
			WriteLog(__FUNCTION__" - Craft<%d> string<%s> load fail.\n",num,str.c_str());
			load_fail++;
			continue;
		}

#ifdef FONLINE_SERVER
		CraftItem* craft=GetCraft(num);
		str=craft->GetStr(true);
#endif
	}

#ifdef FONLINE_SERVER
	msg.CalculateHash();
#endif
	return load_fail==0;
}

#ifdef FONLINE_CLIENT
void CraftManager::GenerateNames(FOMsg& msg_game, FOMsg& msg_item)
{
	CraftItemMapIt it=itemCraft.begin();
	CraftItemMapIt it_end=itemCraft.end();

	for(;it!=it_end;++it)
	{
		(*it).second->SetName(msg_game,msg_item);
	}
}
#endif //FONLINE_CLIENT

void CraftManager::Finish()
{
	CraftItemMapIt it=itemCraft.begin();
	CraftItemMapIt it_end=itemCraft.end();

	for(;it!=it_end;++it)
		delete (*it).second;
	itemCraft.clear();
}

bool CraftManager::AddCraft(DWORD num, const char* str)
{
	// Create new craft item
	CraftItem* craft=new CraftItem;
	if(!craft)
	{
		delete craft;
		WriteLog(__FUNCTION__" - Allocation fail.\n");
		return false;
	}

	// Begin parse string
	if(craft->SetStr(num,str)<0)
	{
		delete craft;
		WriteLog(__FUNCTION__" - Parse fail.\n");
		return false;
	}

#ifdef FONLINE_SERVER
	if(craft->Script.length())
	{
		craft->ScriptBindId=Script::Bind(craft->Script.c_str(),"int %s(Critter&, int)",false);
		if(!craft->ScriptBindId)
		{
			delete craft;
			WriteLog(__FUNCTION__" - Bind function fail.\n");
			return false;
		}
	}
#endif

	// Add
	return AddCraft(craft,false);
}

bool CraftManager::AddCraft(CraftItem* craft, bool make_copy)
{
	if(!craft) return false;
	if(!craft->IsValid()) return false;
	if(IsCraftExist(craft->Num)) return false;

	if(make_copy)
	{
		CraftItem* craft2=new CraftItem;
		*craft2=*craft;
		craft=craft2;
	}

	itemCraft.insert(CraftItemMapVal(craft->Num,craft));
	return true;
}

CraftItem* CraftManager::GetCraft(DWORD num)
{
	CraftItemMapIt it=itemCraft.find(num);
	return it!=itemCraft.end()?(*it).second:NULL;
}

bool CraftManager::IsCraftExist(DWORD num)
{
	return itemCraft.count(num)!=0;
}

#if defined(FONLINE_SERVER) || defined(FONLINE_CLIENT)

#ifdef FONLINE_SERVER
bool CraftManager::IsShowCraft(Critter* cr, DWORD num)
{
	CraftItem* craft=GetCraft(num);
	if(!craft) return false;

	DWORD flags=0xFFFFFFFF;
	if(craft->ScriptBindId)
	{
		if(!Script::PrepareContext(craft->ScriptBindId,CALL_FUNC_STR,cr->GetInfo())) return false;
		Script::SetArgObject(cr);
		Script::SetArgDword(FIXBOY_LIST);
		if(!Script::RunPrepared()) return false;
		flags=Script::GetReturnedDword();
	}

	if(!FLAG(flags,FIXBOY_ALLOW_CRAFT)) return false;
	if(FLAG(flags,FIXBOY_CHECK_PARAMS) && !IsTrueParams(cr,craft->ShowPNum,craft->ShowPVal,craft->ShowPOr)) return false;
	return true;
}
#endif
#ifdef FONLINE_CLIENT
bool CraftManager::IsShowCraft(CritterCl* cr, DWORD num)
{
	CraftItem* craft=GetCraft(num);
	if(!craft) return false;

	DWORD flags=0xFFFFFFFF;
	if(!FLAG(flags,FIXBOY_ALLOW_CRAFT)) return false;
	if(FLAG(flags,FIXBOY_CHECK_PARAMS) && !IsTrueParams(cr,craft->ShowPNum,craft->ShowPVal,craft->ShowPOr)) return false;
	return true;
}
#endif
#ifdef FONLINE_SERVER
void CraftManager::GetShowCrafts(Critter* cr, CraftItemVec& craft_vec)
{
	craft_vec.clear();
	for(CraftItemMapIt it=itemCraft.begin(),end=itemCraft.end();it!=end;++it)
	{
		if(IsShowCraft(cr,(*it).first)) craft_vec.push_back((*it).second);
	}
}
#endif
#ifdef FONLINE_CLIENT
void CraftManager::GetShowCrafts(CritterCl* cr, CraftItemVec& craft_vec)
{
	craft_vec.clear();
	for(CraftItemMapIt it=itemCraft.begin(),end=itemCraft.end();it!=end;++it)
	{
		if(IsShowCraft(cr,(*it).first)) craft_vec.push_back((*it).second);
	}
}
#endif
#ifdef FONLINE_SERVER
bool CraftManager::IsTrueCraft(Critter* cr, DWORD num)
{
	CraftItem* craft=GetCraft(num);
	if(!craft) return false;

	return\
		IsTrueParams(cr,craft->NeedPNum,craft->NeedPVal,craft->NeedPOr) &&\
		IsTrueItems(cr,craft->NeedTools,craft->NeedToolsVal,craft->NeedToolsOr) &&\
		IsTrueItems(cr,craft->NeedItems,craft->NeedItemsVal,craft->NeedItemsOr);
}
#endif
#ifdef FONLINE_CLIENT
bool CraftManager::IsTrueCraft(CritterCl* cr, DWORD num)
{
	CraftItem* craft=GetCraft(num);
	if(!craft) return false;

	return\
		IsTrueParams(cr,craft->NeedPNum,craft->NeedPVal,craft->NeedPOr) &&\
		IsTrueItems(cr,craft->NeedTools,craft->NeedToolsVal,craft->NeedToolsOr) &&\
		IsTrueItems(cr,craft->NeedItems,craft->NeedItemsVal,craft->NeedItemsOr);
}
#endif
#ifdef FONLINE_SERVER
void CraftManager::GetTrueCrafts(Critter* cr, CraftItemVec& craft_vec)
{
	craft_vec.clear();
	CraftItemMapIt it=itemCraft.begin();
	CraftItemMapIt it_end=itemCraft.end();
	for(;it!=it_end;++it)
	{
		if(IsTrueCraft(cr,(*it).first)) craft_vec.push_back((*it).second);
	}
}
#endif
#ifdef FONLINE_CLIENT
void CraftManager::GetTrueCrafts(CritterCl* cr, CraftItemVec& craft_vec)
{
	craft_vec.clear();
	CraftItemMapIt it=itemCraft.begin();
	CraftItemMapIt it_end=itemCraft.end();
	for(;it!=it_end;++it)
	{
		if(IsTrueCraft(cr,(*it).first)) craft_vec.push_back((*it).second);
	}
}
#endif
#ifdef FONLINE_SERVER
bool CraftManager::IsTrueParams(Critter* cr, DwordVec& num_vec, IntVec& val_vec, ByteVec& or_vec)
{
	for(int i=0,j=num_vec.size();i<j;i++)
	{
		DWORD param_num=num_vec[i];
		int param_val=val_vec[i];
		BYTE param_or=or_vec[i];

		if(param_num>=MAX_PARAMS || cr->GetParam(param_num)<param_val) // Fail
		{
			if(i==j-1) return false; // Is last element
			if(!param_or) return false; // AND
		}
		else // True
		{
			if(i==j-1) return true; // Is last element
			// OR, skip elements
			if(param_or) for(i++;i<j-1 && or_vec[i];i++);
		}
	}

	return true;
}
#endif
#ifdef FONLINE_CLIENT
bool CraftManager::IsTrueParams(CritterCl* cr, DwordVec& num_vec, IntVec& val_vec, ByteVec& or_vec)
{
	for(int i=0,j=num_vec.size();i<j;i++)
	{
		DWORD param_num=num_vec[i];
		int param_val=val_vec[i];
		BYTE param_or=or_vec[i];

		if(param_num>=MAX_PARAMS || cr->GetParam(param_num)<param_val) // Fail
		{
			if(i==j-1) return false; // Is last element
			if(!param_or) return false; // AND
		}
		else // True
		{
			if(i==j-1) return true; // Is last element
			// OR, skip elements
			if(param_or) for(i++;i<j-1 && or_vec[i];i++);
		}
	}

	return true;
}
#endif
#ifdef FONLINE_SERVER
bool CraftManager::IsTrueItems(Critter* cr, WordVec& pid_vec, DwordVec& count_vec, ByteVec& or_vec)
{
	for(int i=0,j=pid_vec.size();i<j;i++)
	{
		bool next=true;

		WORD item_pid=pid_vec[i];
		DWORD item_count=count_vec[i];
		BYTE item_or=or_vec[i];

		if(cr->CountItemPid(item_pid)<item_count) next=false;

		if(!next)
		{
			if(i==j-1) return false; // Is last element
			// AND
			if(!item_or) return false;
		}
		else
		{
			if(i==j-1) return true; // Is last element
			// OR, skip elements
			for(;i<j-1 && or_vec[i];i++);
		}
	}

	return true;
}
#endif
#ifdef FONLINE_CLIENT
bool CraftManager::IsTrueItems(CritterCl* cr, WordVec& pid_vec, DwordVec& count_vec, ByteVec& or_vec)
{
	for(int i=0,j=pid_vec.size();i<j;i++)
	{
		bool next=true;

		WORD item_pid=pid_vec[i];
		DWORD item_count=count_vec[i];
		BYTE item_or=or_vec[i];

		if(cr->CountItemPid(item_pid)<item_count) next=false;

		if(!next)
		{
			if(i==j-1) return false; // Is last element
			// AND
			if(!item_or) return false;
		}
		else
		{
			if(i==j-1) return true; // Is last element
			// OR, skip elements
			for(;i<j-1 && or_vec[i];i++);
		}
	}

	return true;
}
#endif

#ifdef FONLINE_SERVER
int CraftManager::ProcessCraft(Critter* cr, DWORD num)
{
	CraftItem* craft=GetCraft(num);
	if(!craft) return CRAFT_RESULT_FAIL;

	DWORD flags=0xFFFFFFFF;
	if(craft->ScriptBindId)
	{
		if(!Script::PrepareContext(craft->ScriptBindId,CALL_FUNC_STR,cr->GetInfo())) return CRAFT_RESULT_FAIL;
		Script::SetArgObject(cr);
		Script::SetArgDword(FIXBOY_BUTTON);
		if(!Script::RunPrepared()) return CRAFT_RESULT_FAIL;
		flags=Script::GetReturnedDword();
	}

	if(!FLAG(flags,FIXBOY_ALLOW_CRAFT)) CRAFT_RETURN_FAIL;

	if(FLAG(flags,FIXBOY_CHECK_TIMEOUT) && (cr->GetTimeout(TO_SK_SCIENCE) || cr->GetTimeout(TO_SK_REPAIR))) CRAFT_RETURN_TIMEOUT;
	if(FLAG(flags,FIXBOY_CHECK_PARAMS) && !IsTrueParams(cr,craft->NeedPNum,craft->NeedPVal,craft->NeedPOr)) CRAFT_RETURN_FAIL;
	if(FLAG(flags,FIXBOY_CHECK_MATERIALS) && !IsTrueItems(cr,craft->NeedTools,craft->NeedToolsVal,craft->NeedToolsOr)) CRAFT_RETURN_FAIL;
	if(FLAG(flags,FIXBOY_CHECK_TOOLS) && !IsTrueItems(cr,craft->NeedItems,craft->NeedItemsVal,craft->NeedItemsOr)) CRAFT_RETURN_FAIL;

	if(craft->ScriptBindId)
	{
		if(!Script::PrepareContext(craft->ScriptBindId,CALL_FUNC_STR,cr->GetInfo())) return CRAFT_RESULT_FAIL;
		Script::SetArgObject(cr);
		Script::SetArgDword(FIXBOY_CRAFT);
		if(!Script::RunPrepared()) return CRAFT_RESULT_FAIL;
		flags=Script::GetReturnedDword();
	}

	if(!FLAG(flags,FIXBOY_ALLOW_CRAFT)) CRAFT_RETURN_FAIL;

	ItemPtrVec sub_items;
	if(FLAG(flags,FIXBOY_SUB_MATERIALS)) // Sub items
	{
		for(int i=0,j=craft->NeedItems.size();i<j;i++)
		{
			WORD pid=craft->NeedItems[i];
			DWORD count=craft->NeedItemsVal[i];
			BYTE or=craft->NeedItemsOr[i];

			if(cr->CountItemPid(pid)<count) continue;
			ItemMngr.SubItemCritter(cr,pid,count,&sub_items);

			// Skip or
			if(or) for(;i<j-1 && craft->NeedItemsOr[i];i++);
		}
	}

	if(FLAG(flags,FIXBOY_ADD_CRAFT_ITEMS)) // Add items
	{
		ItemPtrVec crafted;
		DwordVec crafted_count;
		for(int i=0,j=craft->OutItems.size();i<j;i++)
		{
			WORD pid=craft->OutItems[i];
			DWORD count=craft->OutItemsVal[i];
			ProtoItem* proto_item=ItemMngr.GetProtoItem(pid);
			if(!proto_item) continue;

			if(proto_item->IsGrouped())
			{
				Item* item=ItemMngr.AddItemCritter(cr,pid,count);
				if(item)
				{
					crafted.push_back(item);
					crafted_count.push_back(count);
				}
			}
			else
			{
				for(int j=0;j<count;j++)
				{
					Item* item=ItemMngr.AddItemCritter(cr,pid,1);
					if(item)
					{
						crafted.push_back(item);
						crafted_count.push_back(1);
					}
				}
			}
		}

		if(crafted.size() && Script::PrepareContext(ServerFunctions.ItemsCrafted,CALL_FUNC_STR,cr->GetInfo()))
		{
			CScriptArray* crafted_=Script::CreateArray("Item@[]");
			CScriptArray* crafted_count_=Script::CreateArray("uint[]");
			CScriptArray* sub_items_=Script::CreateArray("Item@[]");
			Script::AppendVectorToArrayRef(crafted,crafted_);
			Script::AppendVectorToArray(crafted_count,crafted_count_);
			Script::AppendVectorToArrayRef(sub_items,sub_items_);

			Script::SetArgObject(crafted_);
			Script::SetArgObject(crafted_count_);
			Script::SetArgObject(sub_items_);
			Script::SetArgObject(cr);
			Script::RunPrepared();

			crafted_->Release();
			crafted_count_->Release();
			sub_items_->Release();
			for(ItemPtrVecIt it=sub_items.begin(),end=sub_items.end();it!=end;++it)
			{
				Item* sub_item=*it;
				if(!sub_item->IsNotValid && !sub_item->IsValidAccessory()) ItemMngr.ItemToGarbage(sub_item);
			}
		}
	}

	if(FLAG(flags,FIXBOY_SET_TIMEOUT))
	{
		cr->SetTimeout(TO_SK_SCIENCE,FIXBOY_TIME_OUT);
		cr->SetTimeout(TO_SK_REPAIR,FIXBOY_TIME_OUT);
	}

	if(FLAG(flags,FIXBOY_ADD_EXPERIENCE))
	{
		cr->ChangeParam(ST_EXPERIENCE);
		if(craft->Experience) cr->Data.Params[ST_EXPERIENCE]+=craft->Experience;
		else if(GameOpt.FixBoyDefaultExperience) cr->Data.Params[ST_EXPERIENCE]+=GameOpt.FixBoyDefaultExperience;
	}

	CRAFT_RETURN_SUCC;
}
#endif // FONLINE_SERVER
#endif // #if defined(FONLINE_SERVER) || defined(FONLINE_CLIENT)