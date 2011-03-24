#include "StdAfx.h"
#include "ConstantsManager.h"
#include "Log.h"
#include "Text.h"
#include "FileManager.h"

const char* CollectionFiles[]=
{
	"ParamNames.lst",
	"ItemNames.lst",
	"DefineNames.lst",
	"PictureNames.lst",
	"HashNames.lst",
};

struct ConstCollection
{
	bool Init;
	UIntStrMap ValueName;
	StrUIntMap NameValue;
	ConstCollection():Init(false){}
};
vector<ConstCollection> ConstCollections;


void ConstantsManager::Initialize(int path_type, const char* path /* = NULL */)
{
	for(int i=0;i<=CONSTANTS_HASH;i++)
	{
		if(path)
		{
			AddCollection(i,Str::Format("%s%s",path,CollectionFiles[i]),path_type);
		}
		else
		{
			AddCollection(i,CollectionFiles[i],path_type);
		}
	}
}

bool ConstantsManager::AddCollection(int collection, const char* fname, int path_type)
{
	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return false;

	if(collection>=(int)ConstCollections.size()) ConstCollections.resize(collection+1);
	UIntStrMap& value_name=ConstCollections[collection].ValueName;
	StrUIntMap& name_value=ConstCollections[collection].NameValue;

	ConstCollections[collection].Init=true;
	value_name.clear();
	name_value.clear();

	bool revert=false;
	char line[MAX_FOTEXT];
	string name;
	int offset=0;
	int num;
	while(fm.GetLine(line,MAX_FOTEXT))
	{
		if(line[0]=='*')
		{
			if(line[1]=='*') revert=!revert;
			else offset=atoi(&line[1]);
		}
		else
		{
			istrstream str(line);

			if(!revert)
			{
				if((str >> num).fail()) continue;
				if((str >> name).fail()) continue;
			}
			else
			{
				if((str >> name).fail()) continue;
				if((str >> num).fail()) continue;
			}

			value_name.insert(UIntStrMapVal(num+offset,name));
			name_value.insert(StrUIntMapVal(name,num+offset));
			Str::AddNameHash(name.c_str());
		}
	}

	return true;
}

void ConstantsManager::AddConstant(int collection, const char* str, int value)
{
	ConstCollections[collection].ValueName.insert(UIntStrMapVal(value,str));
	ConstCollections[collection].NameValue.insert(StrUIntMapVal(str,value));
	Str::AddNameHash(str);
}

StrVec ConstantsManager::GetCollection(int collection)
{
	UIntVec val_added;
	StrVec result;
	for(UIntStrMapIt it=ConstCollections[collection].ValueName.begin(),end=ConstCollections[collection].ValueName.end();it!=end;++it)
	{
		if(collection==CONSTANTS_DEFINE || std::find(val_added.begin(),val_added.end(),(*it).first)==val_added.end())
		{
			result.push_back((*it).second);
			val_added.push_back((*it).first);
		}
	}
	return result;
}

bool ConstantsManager::IsCollectionInit(int collection)
{
	return collection>=0 && collection<(int)ConstCollections.size() && ConstCollections[collection].Init;
}

int ConstantsManager::GetValue(int collection, const char* str)
{
	StrUIntMapIt it=ConstCollections[collection].NameValue.find(str);
	if(it==ConstCollections[collection].NameValue.end()) return -1;
	return (*it).second;
}

const char* ConstantsManager::GetName(int collection, int value)
{
	UIntStrMapIt it=ConstCollections[collection].ValueName.find(value);
	if(it==ConstCollections[collection].ValueName.end()) return NULL;
	return (*it).second.c_str();
}

int ConstantsManager::GetParamId(const char* str)
{
	return GetValue(CONSTANTS_PARAM,str);
}

const char* ConstantsManager::GetParamName(uint index)
{
	return GetName(CONSTANTS_PARAM,index);
}

int ConstantsManager::GetItemPid(const char* str)
{
	return GetValue(CONSTANTS_ITEM,str);
}

const char* ConstantsManager::GetItemName(ushort pid)
{
	return GetName(CONSTANTS_ITEM,pid);
}

int ConstantsManager::GetDefineValue(const char* str)
{
	if(Str::IsNumber(str)) return atoi(str);

	if(!_stricmp(str,"true")) return 1;
	else if(!_stricmp(str,"false")) return 0;

	StrUIntMapIt it=ConstCollections[CONSTANTS_DEFINE].NameValue.find(str);
	if(it==ConstCollections[CONSTANTS_DEFINE].NameValue.end())
	{
		WriteLog(_FUNC_," - Define<%s> not found, taked zero by default.\n",str);
		return 0;
	}
	return (*it).second;
}

const char* ConstantsManager::GetPictureName(uint index)
{
	return GetName(CONSTANTS_PICTURE,index);
}

