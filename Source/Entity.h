#ifndef __ENTITY__
#define __ENTITY__

#include "Common.h"

enum class EntityType
{
    Invalid = 0,
    Custom,
    Item,
    Client,
    Npc,
    Location,
    Map,
    CritterCl,
    ItemHex,
    Max,
};

class Entity
{
protected:
    Entity( uint id, EntityType type, PropertyRegistrator* registartor );

public:
    static uint        CurrentId;

    static const uint  GenerateId = uint( -1 );
    static const uint  DeferredId = uint( -2 );

    Properties         Props;
    mutable uint       Id;
    mutable EntityType Type;
    mutable long       RefCounter;
    bool               IsDestroyed;
    bool               IsDestroying;

    uint GetId();
    void AddRef() const;
    void Release() const;
    void SetDeferredId( uint id ) const;
};
typedef vector< Entity* >    EntityVec;
typedef map< uint, Entity* > EntityMap;

class CustomEntity: public Entity
{
public:
    CustomEntity( uint id, uint sub_type, PropertyRegistrator* registrator );
    const uint SubType;
};

#endif // __ENTITY__
