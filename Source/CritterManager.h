#ifndef __CRITTER_MANAGER__
#define __CRITTER_MANAGER__

#include "Common.h"
#include "CritterData.h"

#ifdef FONLINE_SERVER
# include "Critter.h"
#endif


class CritterManager
{
private:
    CritDataMap allProtos;

public:
    CritterManager() { MEMORY_PROCESS( MEMORY_STATIC, sizeof( CritterManager ) ); }
    ~CritterManager() { MEMORY_PROCESS( MEMORY_STATIC, -(int) sizeof( CritterManager ) ); }

    bool Init();
    void Finish();
    void Clear();

    bool         LoadProtos();
    CritData*    GetProto( hash proto_id );
    CritDataMap& GetAllProtos();

    #ifdef FONLINE_SERVER
private:
    CrMap   allCritters;
    UIntVec crToDelete;
    uint    lastNpcId;
    uint    playersCount, npcCount;
    Mutex   crLocker;

public:
    void SaveCrittersFile( void ( * save_func )( void*, size_t ) );
    bool LoadCrittersFile( void* f, uint version );

    void RunInitScriptCritters();

    void CritterToGarbage( Critter* cr );
    void CritterGarbager();

    Npc* CreateNpc( hash proto_id, uint params_count, int* params, uint items_count, int* items, const char* script, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy );
    Npc* CreateNpc( hash proto_id, bool copy_data );

    void     AddCritter( Critter* cr );
    CrMap&   GetCrittersNoLock() { return allCritters; }
    void     GetCopyCritters( CrVec& critters, bool sync_lock );
    void     GetCopyNpcs( PcVec& npcs, bool sync_lock );
    void     GetCopyPlayers( ClVec& players, bool sync_lock );
    void     GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type, CrVec& critters, bool sync_lock );
    Critter* GetCritter( uint crid, bool sync_lock );
    Client*  GetPlayer( uint crid, bool sync_lock );
    Client*  GetPlayer( const char* name, bool sync_lock );
    Npc*     GetNpc( uint crid, bool sync_lock );
    void     EraseCritter( Critter* cr );
    void     GetNpcIds( UIntSet& npc_ids );

    uint PlayersInGame();
    uint NpcInGame();
    uint CrittersInGame();
    #endif // FONLINE_SERVER
};

extern CritterManager CrMngr;

#endif // __CRITTER_MANAGER__
