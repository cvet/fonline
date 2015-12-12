#ifndef __CRITTER_MANAGER__
#define __CRITTER_MANAGER__

#include "Common.h"
#include "Map.h"
#include "Critter.h"

class CritterManager
{
public:
    Npc* CreateNpc( hash proto_id, Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy );
    bool RestoreNpc( uint id, hash proto_id, const StrMap& props_data );
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
};

extern CritterManager CrMngr;

#endif // __CRITTER_MANAGER__
