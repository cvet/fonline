#pragma once

#include "Common.h"
#include "Entity.h"

class ProtoManager;
class EntityManager;
class MapManager;
class ItemManager;

class CritterManager
{
public:
    CritterManager( ProtoManager& proto_mngr, EntityManager& entity_mngr, MapManager& map_mngr, ItemManager& item_mngr );

    Npc* CreateNpc( hash proto_id, Properties* props, Map* map, ushort hx, ushort hy, uchar dir, bool accuracy );
    bool RestoreNpc( uint id, hash proto_id, const DataBase::Document& doc );
    void DeleteNpc( Critter* cr );
    void DeleteInventory( Critter* cr );

    void AddItemToCritter( Critter* cr, Item*& item, bool send );
    void EraseItemFromCritter( Critter* cr, Item* item, bool send );

    void     GetCritters( CritterVec& critters );
    void     GetNpcs( NpcVec& npcs );
    void     GetClients( ClientVec& players, bool on_global_map = false );
    void     GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type, CritterVec& critters );
    Critter* GetCritter( uint crid );
    Client*  GetPlayer( uint crid );
    Client*  GetPlayer( const char* name );
    Npc*     GetNpc( uint crid );

    Item* GetItemByPidInvPriority( Critter* cr, hash item_pid );

    void ProcessTalk( Client* cl, bool force );
    void CloseTalk( Client* cl );

    uint PlayersInGame();
    uint NpcInGame();
    uint CrittersInGame();

private:
    ProtoManager&  protoMngr;
    EntityManager& entityMngr;
    MapManager&    mapMngr;
    ItemManager&   itemMngr;
};
