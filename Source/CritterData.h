#ifndef __CRITTER_DATA__
#define __CRITTER_DATA__

#include "Item.h"
#include "FileManager.h"
#include "AI.h"
#include "Defines.h"

#define MAX_CRIT_PROTOS		   (10000)
#define MAX_STORED_LOCATIONS   (1000)

struct CritData
{
	WORD HexX;
	WORD HexY;
	WORD WorldX;
	WORD WorldY;
	DWORD BaseType;
	BYTE Dir;
	BYTE Cond;
	BYTE CondExt;
	BYTE Reserved0;
	DWORD ScriptId;
	DWORD ShowCritterDist1;
	DWORD ShowCritterDist2;
	DWORD ShowCritterDist3;
	DWORD CrTimeEventFullMinute;
	DWORD GlobalGroupUid;
	WORD LastHexX;
	WORD LastHexY;
	DWORD Reserved1[4];
	DWORD Id;
	DWORD MapId;
	WORD MapPid;
	WORD Reserved2;
	int Params[MAX_PARAMS];
	DWORD Reserved3[10];
	char Lexems[LEXEMS_SIZE];
	DWORD Reserved4[8];
	bool ClientToDelete;
	BYTE Reserved5;
	WORD Reserved6;
	DWORD Reserved7;
	WORD Reserved8;
	WORD HoloInfoCount;
	DWORD HoloInfo[MAX_HOLO_INFO];
	DWORD Reserved9[10];
	int Scores[SCORES_MAX];
	DWORD Reserved10[100];
	// Npc data
	DWORD HomeMap;
	WORD HomeX;
	WORD HomeY;
	BYTE HomeOri;
	BYTE Reserved11;
	WORD ProtoId;
	DWORD Reserved12;
	DWORD Reserved13;
	DWORD Reserved14;
	DWORD Reserved15;
	bool IsDataExt;
	BYTE Reserved16;
	WORD Reserved17;
	DWORD Reserved18[8];
	WORD FavoriteItemPid[4];
	DWORD Reserved19[10];
	DWORD EnemyStackCount;
	DWORD EnemyStack[MAX_ENEMY_STACK];
	DWORD Reserved20[5];
	BYTE BagCurrentSet[MAX_NPC_BAGS_PACKS];
	short BagRefreshTime;
	BYTE Reserved21;
	BYTE BagSize;
	NpcBagItem Bag[MAX_NPC_BAGS];
	DWORD Reserved22[100];
};

struct CritDataExt
{
	DWORD Reserved23[10];
	BYTE GlobalMapFog[GM_ZONES_FOG_SIZE];
	WORD Reserved24;
	WORD LocationsCount;
	DWORD LocationsId[MAX_STORED_LOCATIONS];
	DWORD Reserved25[100];
};

#endif // __CRITTER_DATA__