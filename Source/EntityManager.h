#ifndef __ENTITY_MANAGER__
#define __ENTITY_MANAGER__

#include "Common.h"
#include "Entity.h"
#include "Item.h"
#include "Critter.h"
#include "Map.h"

class EntityManager
{
private:
    uint      currentId;
    EntityMap allEntities;
    uint      entitiesCount[ (int) EntityType::Max ];
    Mutex     entitiesLocker;

public:
    EntityManager();

    void    RegisterEntity( Entity* entity );
    void    UnregisterEntity( Entity* entity );
    Entity* GetEntity( uint id, EntityType type );
    void    GetEntities( EntityType type, EntityVec& entities );
    uint    GetEntitiesCount( EntityType type );

    void      GetItems( ItemVec& items );
    void      GetCritterItems( uint crid, ItemVec& items );
    Critter*  GetCritter( uint crid );
    void      GetCritters( CrVec& critters );
    Map*      GetMapByPid( hash pid, uint skip_count );
    void      GetMaps( MapVec& maps );
    Location* GetLocationByPid( hash pid, uint skip_count );
    void      GetLocations( LocVec& locations );

    void SaveEntities( void ( * save_func )( void*, size_t ) );
    bool LoadEntities( void* file, uint version );
    void InitEntities();
    void FinishEntities();
    void ClearEntities();
};

extern EntityManager EntityMngr;

#endif // __ENTITY_MANAGER__
