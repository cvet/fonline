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
	bool isActive;
	FileManager fileMngr;
	CritData allProtos[MAX_CRIT_PROTOS];

public:
	CritterManager():isActive(false){MEMORY_PROCESS(MEMORY_STATIC,sizeof(CritterManager));}

	bool Init();
	bool IsInit(){return isActive;}
	void Finish();
	void Clear();

	bool LoadProtos();
	CritData* GetProto(WORD proto_id);

#ifdef FONLINE_SERVER
private:
	CrMap allCritters;
	DwordVec crToDelete;
	DWORD lastNpcId;
	DWORD playersCount,npcCount;
	Mutex crLocker;

public:
	void SaveCrittersFile(void(*save_func)(void*,size_t));
	bool LoadCrittersFile(FILE* f, DWORD version);

	void RunInitScriptCritters();

	void CritterToGarbage(Critter* cr);
	void CritterGarbager();

	Npc* CreateNpc(WORD proto_id, DWORD params_count, int* params, DWORD items_count, int* items, const char* script, Map* map, WORD hx, WORD hy, BYTE dir, bool accuracy);
	Npc* CreateNpc(WORD proto_id, bool copy_data);

	void AddCritter(Critter* cr);
	CrMap& GetCrittersNoLock(){return allCritters;}
	void GetCopyNpcs(PcVec& npcs, bool sync_lock);
	void GetCopyPlayers(ClVec& players, bool sync_lock);
	Critter* GetCritter(DWORD crid, bool sync_lock);
	Client* GetPlayer(DWORD crid, bool sync_lock);
	Client* GetPlayer(const char* name, bool sync_lock);
	Npc* GetNpc(DWORD crid, bool sync_lock);
	void EraseCritter(Critter* cr);
	void GetNpcIds(DwordSet& npc_ids);

	DWORD PlayersInGame();
	DWORD NpcInGame();
	DWORD CrittersInGame();

#endif // FONLINE_SERVER
};

extern CritterManager CrMngr;

#endif // __CRITTER_MANAGER__