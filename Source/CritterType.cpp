#include "StdAfx.h"
#include "CritterType.h"
#include "Text.h"
#include "FileManager.h"

CritTypeType CrTypes[MAX_CRIT_TYPES];
int MoveWalk[MAX_CRIT_TYPES][6]={0};

bool CritType::IsEnabled(uint cr_type)
{
	return cr_type && cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled;
}

CritTypeType& CritType::GetCritType(uint cr_type)
{
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled?CrTypes[cr_type]:CrTypes[DEFAULT_CRTYPE];
}

CritTypeType& CritType::GetRealCritType(uint cr_type)
{
	return CrTypes[cr_type];
}

const char* CritType::GetName(uint cr_type)
{
	return GetCritType(cr_type).Name;
}

const char* CritType::GetSoundName(uint cr_type)
{
	CritTypeType& type=GetCritType(cr_type);
	if(type.SoundName[0]) return type.SoundName;
	return type.Name;
}

uint CritType::GetAlias(uint cr_type)
{
	return GetCritType(cr_type).Alias;
}

uint CritType::GetMultihex(uint cr_type)
{
	return GetCritType(cr_type).Multihex;
}

int CritType::GetAnimType(uint cr_type)
{
	return GetCritType(cr_type).AnimType;
}

bool CritType::IsCanWalk(uint cr_type)
{
	return GetCritType(cr_type).CanWalk;
}

bool CritType::IsCanRun(uint cr_type)
{
	return GetCritType(cr_type).CanRun;
}

bool CritType::IsCanAim(uint cr_type)
{
	return GetCritType(cr_type).CanAim;
}

bool CritType::IsCanArmor(uint cr_type)
{
	return GetCritType(cr_type).CanArmor;
}

bool CritType::IsCanRotate(uint cr_type)
{
	return GetCritType(cr_type).CanRotate;
}

bool CritType::IsAnim1(uint cr_type, uint anim1)
{
	if(anim1>=37) return false;
	return GetCritType(cr_type).Anim1[anim1];
}

void CritType::SetWalkParams(uint cr_type, uint time_walk, uint time_run, uint step0, uint step1, uint step2, uint step3)
{
	if(cr_type>=MAX_CRIT_TYPES) return;
	MoveWalk[cr_type][0]=step0;
	MoveWalk[cr_type][1]=step1;
	MoveWalk[cr_type][2]=step2;
	MoveWalk[cr_type][3]=step3;
	MoveWalk[cr_type][4]=time_walk;
	MoveWalk[cr_type][5]=time_run;
}

uint CritType::GetTimeWalk(uint cr_type)
{
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled?MoveWalk[cr_type][4]:0;
}

uint CritType::GetTimeRun(uint cr_type)
{
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled?MoveWalk[cr_type][5]:0;
}

int CritType::GetWalkFrmCnt(uint cr_type, uint step)
{
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled && step<4?MoveWalk[cr_type][step]:0;
}

int CritType::GetRunFrmCnt(uint cr_type, uint step)
{
	return 0;
/*
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled && step<4?MoveWalk[cr_type][step]:0;

	if(cr_type>=MAX_CRIT_TYPES) return 0;
	if(step>3) return 0;

	return 0;*/
}

bool CritType::InitFromFile(FOMsg* fill_msg)
{
	AutoPtrArr<CritTypeType> CrTypesReserved(new CritTypeType[MAX_CRIT_TYPES]);
	if(!CrTypesReserved.IsValid()) return false;
	int MoveWalkReserved[MAX_CRIT_TYPES][6];
	ZeroMemory(CrTypesReserved.Get(),sizeof(CritTypeType)*MAX_CRIT_TYPES);
	ZeroMemory(MoveWalkReserved,sizeof(MoveWalkReserved));

	FileManager file;
	if(!file.LoadFile(CRTYPE_FILE_NAME,PT_SERVER_DATA))
	{
		WriteLog(_FUNC_," - File<%s> not found.\n",FileManager::GetFullPath(CRTYPE_FILE_NAME,PT_SERVER_DATA));
		return false;
	}

	char line[2048];
	string svalue;
	int number;
	int errors=0;
	int success=0;
	bool prev_fail=false;
	while(file.GetLine(line,2048))
	{
		if(prev_fail)
		{
			WriteLog(_FUNC_," - Bad data for critter type information, number<%d>.\n",number);
			prev_fail=false;
			errors++;
		}

		istrstream str(line);

		str >> svalue;
		if(svalue!="@") continue;

		prev_fail=true;

		// Number
		str >> number;
		if(str.fail() || number<0 || number>=MAX_CRIT_TYPES) continue;
		CritTypeType& ct=CrTypesReserved.Get()[number];

		// Name
		str >> svalue;
		if(str.fail()) continue;
		StringCopy(ct.Name,svalue.c_str());

		// Alias, Multihex, 3d, Walk, Run, Aim, Armor, Rotate
		str >> svalue;
		if(svalue=="-") ct.Alias=number;
		else ct.Alias=atoi(svalue.c_str());
		if(str.fail()) continue;
		str >> ct.Multihex;
		if(str.fail() || ct.Multihex>MAX_HEX_OFFSET) continue;
		str >> ct.AnimType;
		if(str.fail()) continue;
		str >> ct.CanWalk;
		if(str.fail()) continue;
		str >> ct.CanRun;
		if(str.fail()) continue;
		str >> ct.CanAim;
		if(str.fail()) continue;
		str >> ct.CanArmor;
		if(str.fail()) continue;
		str >> ct.CanRotate;
		if(str.fail()) continue;

		// A B C D E F G H I J K L M N O P Q R S T
		for(int i=1;i<=ABC_SIZE;i++)
		{
			str >> ct.Anim1[i];
			if(str.fail()) break;
		}
		if(str.fail()) continue;

		// Walk, Run
		str >> MoveWalkReserved[number][4];
		if(str.fail()) continue;
		str >> MoveWalkReserved[number][5];
		if(str.fail()) continue;

		// Frames per hex
		if(ct.AnimType==ANIM_TYPE_FALLOUT)
		{
			str >> MoveWalkReserved[number][0];
			if(str.fail()) continue;
			str >> MoveWalkReserved[number][1];
			if(str.fail()) continue;
			str >> MoveWalkReserved[number][2];
			if(str.fail()) continue;
			str >> MoveWalkReserved[number][3];
			if(str.fail()) continue;
		}
		else if(ct.AnimType!=ANIM_TYPE_3D)
		{
			str >> MoveWalkReserved[number][0];
			if(str.fail()) continue;
			str >> MoveWalkReserved[number][1];
			if(str.fail()) continue;
			MoveWalkReserved[number][2]=0;
			MoveWalkReserved[number][3]=0;
		}

		// Sound name
		str >> svalue;
		if(str.fail()) continue;
		if(svalue=="-") ct.SoundName[0]=0;
		else StringCopy(ct.SoundName,svalue.c_str());

		// Register as valid
		ct.Anim1[0]=true;
		ct.Enabled=true;
		success++;
		prev_fail=false;
	}

	if(errors) return false;

	if(!CrTypesReserved.Get()[0].Enabled)
	{
		WriteLog(_FUNC_," - Default zero type not loaded.\n");
		return false;
	}

	memcpy(CrTypes,CrTypesReserved.Get(),sizeof(CrTypes));
	memcpy(MoveWalk,MoveWalkReserved,sizeof(MoveWalk));

	if(fill_msg)
	{
		char str[MAX_FOTEXT];
		for(int i=0;i<MAX_CRIT_TYPES;i++)
		{
			CritTypeType& ct=CrTypes[i];
			if(!ct.Enabled) continue;

			sprintf(str,"%s %u %u %u %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %s-",ct.Name,
				ct.Alias,ct.Multihex,ct.AnimType,ct.CanWalk,ct.CanRun,ct.CanAim,ct.CanArmor,ct.CanRotate,
				ct.Anim1[1],ct.Anim1[2],ct.Anim1[3],ct.Anim1[4],ct.Anim1[5],ct.Anim1[6],ct.Anim1[7],
				ct.Anim1[8],ct.Anim1[9],ct.Anim1[10],ct.Anim1[11],ct.Anim1[12],ct.Anim1[13],ct.Anim1[14],
				ct.Anim1[15],ct.Anim1[16],ct.Anim1[17],ct.Anim1[18],ct.Anim1[19],ct.Anim1[20],
				ct.Anim1[21],ct.Anim1[22],ct.Anim1[23],ct.Anim1[24],ct.Anim1[25],
				MoveWalkReserved[i][4],MoveWalkReserved[i][5],
				MoveWalkReserved[i][0],MoveWalkReserved[i][1],MoveWalkReserved[i][2],MoveWalkReserved[i][3],
				ct.SoundName);

			fill_msg->AddStr(STR_INTERNAL_CRTYPE(i),str);
		}
	}

	WriteLog(NULL,"Loaded<%d> critter types.\n",success);
	return true;
}

bool CritType::InitFromMsg(FOMsg* msg)
{
	if(!msg)
	{
		WriteLog(_FUNC_," - Msg nullptr.\n");
		return false;
	}

	AutoPtrArr<CritTypeType> CrTypesReserved(new CritTypeType[MAX_CRIT_TYPES]);
	if(!CrTypesReserved.IsValid()) return false;
	int MoveWalkReserved[MAX_CRIT_TYPES][6];
	ZeroMemory(CrTypesReserved.Get(),sizeof(CritTypeType)*MAX_CRIT_TYPES);
	ZeroMemory(MoveWalkReserved,sizeof(MoveWalkReserved));

	int errors=0;
	int success=0;
	char name[256];
	char sound_name[256];
	for(int i=0;i<MAX_CRIT_TYPES;i++)
	{
		if(!msg->Count(STR_INTERNAL_CRTYPE(i))) continue;
		const char* str=msg->GetStr(STR_INTERNAL_CRTYPE(i));
		CritTypeType& ct=CrTypesReserved.Get()[i];

		if(sscanf(str,"%s%u%u%u%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%s",name,
			&ct.Alias,&ct.Multihex,&ct.AnimType,&ct.CanWalk,&ct.CanRun,&ct.CanAim,&ct.CanArmor,&ct.CanRotate,
			&ct.Anim1[1],&ct.Anim1[2],&ct.Anim1[3],&ct.Anim1[4],&ct.Anim1[5],&ct.Anim1[6],&ct.Anim1[7],
			&ct.Anim1[8],&ct.Anim1[9],&ct.Anim1[10],&ct.Anim1[11],&ct.Anim1[12],&ct.Anim1[13],&ct.Anim1[14],
			&ct.Anim1[15],&ct.Anim1[16],&ct.Anim1[17],&ct.Anim1[18],&ct.Anim1[19],&ct.Anim1[20],
			&ct.Anim1[21],&ct.Anim1[22],&ct.Anim1[23],&ct.Anim1[24],&ct.Anim1[25],
			&MoveWalkReserved[i][4],&MoveWalkReserved[i][5],
			&MoveWalkReserved[i][0],&MoveWalkReserved[i][1],&MoveWalkReserved[i][2],&MoveWalkReserved[i][3],
			sound_name)!=41)
		{
			WriteLog(_FUNC_," - Bad data for critter type information, number<%d>, line<%s>.\n",i,str);
			errors++;
			continue;
		}
		sound_name[strlen(sound_name)-1]=0;

		StringCopy(ct.Name,name);
		StringCopy(ct.SoundName,sound_name);
		ct.Anim1[0]=true;
		ct.Enabled=true;
		success++;
	}

	if(errors) return false;

	if(!CrTypesReserved.Get()[0].Enabled)
	{
		WriteLog(_FUNC_," - Default zero type not loaded.\n");
		return false;
	}

	memcpy(CrTypes,CrTypesReserved.Get(),sizeof(CrTypes));
	memcpy(MoveWalk,MoveWalkReserved,sizeof(MoveWalk));
	WriteLog(NULL,"Loaded<%d> critter types.\n",success);
	return true;
}
