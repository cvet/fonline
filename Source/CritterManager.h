#ifndef __CRITTER_MANAGER__
#define __CRITTER_MANAGER__

#include "Common.h"
#include "CritterData.h"

#ifdef FONLINE_SERVER
# include "Critter.h"
#else
# include "CritterCl.h"
#endif


class CritterManager
{
private:
    ProtoCritterMap allProtos;

public:
    CritterManager() { MEMORY_PROCESS( MEMORY_STATIC, sizeof( CritterManager ) ); }
    ~CritterManager() { MEMORY_PROCESS( MEMORY_STATIC, -(int) sizeof( CritterManager ) ); }

    bool Init();
    void Finish();
    void Clear();

    bool             LoadProtos();
    ProtoCritter*    GetProto( hash proto_id );
    ProtoCritterMap& GetAllProtos();

    #ifdef FONLINE_SERVER
public:
    Npc* CreateNpc( hash proto_id, IntVec* props_data, IntVec* items_data, const char* script, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy );
    bool RestoreNpc( uint id, CritData& data, Properties& props, Critter::CrTimeEventVec time_events );
    void DeleteNpc( Critter* cr );

    void     GetCritters( CrVec& critters, bool sync_lock );
    void     GetNpcs( PcVec& npcs, bool sync_lock );
    void     GetClients( ClVec& players, bool sync_lock, bool on_global_map = false );
    void     GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type, CrVec& critters, bool sync_lock );
    Critter* GetCritter( uint crid, bool sync_lock );
    Client*  GetPlayer( uint crid, bool sync_lock );
    Client*  GetPlayer( const char* name, bool sync_lock );
    Npc*     GetNpc( uint crid, bool sync_lock );

    uint PlayersInGame();
    uint NpcInGame();
    uint CrittersInGame();
    #endif // FONLINE_SERVER
};

extern CritterManager CrMngr;

#endif // __CRITTER_MANAGER__
