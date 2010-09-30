#include "StdAfx.h"
#include "Names.h"
#include "Log.h"
#include "Text.h"
#include "FileManager.h"

struct SomeName
{
	int Index;
	string Name;
	SomeName(int index, const char* name){Index=index; Name=name;}
	bool operator==(const int index) const {return Index==index;}
	bool operator==(const char* name) const {return Name==name;}
};
typedef vector<SomeName> SomeNameVec;
typedef vector<SomeName>::iterator SomeNameVecIt;

SomeNameVec Names[FONAME_MAX];
const char* NamesFile[]=
{
	{"ParamNames.lst"},
	{"ItemNames.lst"},
	{"DefineNames.lst"},
	{"PictureNames.lst"},
};

StrVec FONames::GetNames(int names)
{
	IntVec val_added;
	StrVec result;
	for(SomeNameVecIt it=Names[names].begin(),end=Names[names].end();it!=end;++it)
	{
		if(names==FONAME_DEFINE || std::find(val_added.begin(),val_added.end(),(*it).Index)==val_added.end())
		{
			result.push_back((*it).Name);
			val_added.push_back((*it).Index);
		}
	}
	return result;
}

int FONames::GetParamId(const char* str)
{
	SomeNameVecIt it=std::find(Names[FONAME_PARAM].begin(),Names[FONAME_PARAM].end(),str);
	if(it==Names[FONAME_PARAM].end()) return -1;
	return (*it).Index;
}

const char* FONames::GetParamName(DWORD index)
{
	SomeNameVecIt it=std::find(Names[FONAME_PARAM].begin(),Names[FONAME_PARAM].end(),index);
	if(it==Names[FONAME_PARAM].end()) return NULL;
	return (*it).Name.c_str();
}

int FONames::GetItemPid(const char* str)
{
	SomeNameVecIt it=std::find(Names[FONAME_ITEM].begin(),Names[FONAME_ITEM].end(),str);
	if(it==Names[FONAME_ITEM].end()) return -1;
	return (*it).Index;
}

const char* FONames::GetItemName(WORD pid)
{
	SomeNameVecIt it=std::find(Names[FONAME_ITEM].begin(),Names[FONAME_ITEM].end(),pid);
	if(it==Names[FONAME_ITEM].end()) return NULL;
	return (*it).Name.c_str();
}

int FONames::GetDefineValue(const char* str)
{
	if(Str::IsNumber(str)) return atoi(str);

	SomeNameVecIt it=std::find(Names[FONAME_DEFINE].begin(),Names[FONAME_DEFINE].end(),str);
	if(it==Names[FONAME_DEFINE].end())
	{
		WriteLog(__FUNCTION__" - Define<%s> not found, taked zero by default.\n",str);
		return 0;
	}
	return (*it).Index;
}

const char* FONames::GetPictureName(DWORD index)
{
	SomeNameVecIt it=std::find(Names[FONAME_PICTURE].begin(),Names[FONAME_PICTURE].end(),index);
	if(it==Names[FONAME_PICTURE].end()) return NULL;
	return (*it).Name.c_str();
}

void FONames::GenerateFoNames(int path_type)
{
	for(int i=0;i<FONAME_MAX;i++)
	{
		SomeNameVec& names=Names[i];
		FileManager fm;
		if(!fm.LoadFile(NamesFile[i],path_type)) continue;

		char line[512];
		int offset=0;
		while(fm.GetLine(line,512))
		{
			if(line[0]=='*')
			{
				offset=atoi(&line[1]);
			}
			else
			{
				int num;
				char name[512];
				istrstream str(line);

				str >> num;
				if(str.fail()) continue;
				str >> name;
				if(str.fail()) continue;

				names.push_back(SomeName(num+offset,name));
			}
		}
	}
}

