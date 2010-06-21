#ifndef __FOLOCATION__H__
#define __FOLOCATION__H__

#include "common.h"

/********************************************************************
	created:	22:05:2007   19:39

	author:		Anton Tvetinsky aka Cvet
	
	purpose:	
*********************************************************************/

#include "CFileMngr.h"

//type
#define LOCATION_TYPE_NONE		0
#define LOCATION_TYPE_CITY		1
#define LOCATION_TYPE_ENCAUNTER	2
#define LOCATION_TYPE_SPECIAL	3
#define LOCATION_TYPE_QUEST		4

//Locations
#define VERSION_PROTOTYPE_LOCATION	1
#define MAX_PROTO_LOCATIONS		100
#define LOCATION_PROTO_EXT		".loc"

#define MAX_MAPS_IN_LOCATION	20

#define MAX_GROUPS_ON_MAP		10

//Encaunters District
#define DISTRICT_WASTELAND		0x1
#define DISTRICT_MOUNTAINS		0x2
#define DISTRICT_FOREST			0x4
#define DISTRICT_OCEAN			0x8

class CProtoLocation
{
private:
	WORD pid;
	CFileMngr* fm;

public:
	BYTE type;

	BYTE radius;
	DWORD max_players;

	vector<WORD> proto_maps;

	union
	{
		struct
		{
			WORD steal;
			WORD count_starts_maps;
			DWORD population;
			DWORD life_level;
		} CITY;
		
		struct
		{
			WORD district;
			WORD max_groups;
		} ENCAUNTER;

		struct
		{
			DWORD reserved;
		} SPECIAL;

		struct
		{
			DWORD reserved;
		} QUEST;
	};

	BOOL Init(WORD _pid, CFileMngr* _fm);
	void Clear();
	BOOL Load(int PathType);
	BOOL Save(int PathType);

	BOOL IsInit(){if(pid) return TRUE; return FALSE;};

	BYTE GetType(){return type;};
	WORD GetPid(){return pid;};

	CProtoLocation():pid(0),radius(0),max_players(0),fm(NULL){};
};

#endif //__FOLOCATION__H__