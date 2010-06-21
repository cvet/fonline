#ifndef __CRITTER_MANAGER__
#define __CRITTER_MANAGER__

#include "Common.h"
#include "CritterData.h"

#ifdef FONLINE_SERVER
#include "Critter.h"
#endif


class CritterManager
{
private:
	FileManager fileMngr;
	CritData allProtos[MAX_CRIT_PROTOS];
	DWORD lastNpcId;
	bool active;

public:
	CritterManager():active(false),lastNpcId(NPC_START_ID){MEMORY_PROCESS(MEMORY_STATIC,sizeof(CritterManager));}

	bool Init();
	bool IsInit(void) const {return active;}
	void Finish();

	bool LoadProtos();
	CritData* GetProto(WORD proto_id);

#ifdef FONLINE_SERVER
private:
	CrMap crMap;
	DwordVec crToDelete;
	DWORD playersCount,npcCount;

public:
	void SaveCrittersFile(void(*save_func)(void*,size_t));
	bool LoadCrittersFile(FILE* f);

	void RunInitScriptCritters();

	void CritterToGarbage(Critter* cr);
	void CritterGarbager(DWORD cycle_tick);

	Npc* CreateNpc(WORD proto_id, DWORD params_count, int* params, DWORD items_count, int* items, const char* script, Map* map, WORD hx, WORD hy, BYTE dir, bool accuracy);
	Npc* CreateNpc(WORD proto_id, bool copy_data);

	void AddCritter(Critter* cr);
	CrMap& GetCritters(){return crMap;}
	void GetCopyNpcs(PcVec& npcs);
	void GetCopyPlayers(ClVec& players);
	Critter* GetCritter(DWORD crid);
	Client* GetPlayer(DWORD crid);
	Client* GetPlayer(const char* name);
	Npc* GetNpc(DWORD crid);
	void EraseCritter(Critter* cr);
	void DeleteCritter(Critter* cr);

	DWORD PlayersInGame();
	DWORD NpcInGame();
	DWORD CrittersInGame();

	void ProcessCrittersVisible();
	void ProcessItemsVisible();

#endif // FONLINE_SERVER
};

extern CritterManager CrMngr;

#endif // __CRITTER_MANAGER__