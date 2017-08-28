#ifndef __ENTITY__
#define __ENTITY__

#include "Common.h"
#include "Properties.h"
#include "Methods.h"

enum class EntityType
{
    None = 0,
    Custom,
    Item,
    Client,
    Npc,
    Location,
    Map,
    CritterCl,
    ItemHex,
    Global,
    Max,
};

class Entity;
using EntityVec = vector< Entity* >;
using EntityMap = map< uint, Entity* >;
class ProtoEntity;
using ProtoEntityVec = vector< ProtoEntity* >;
using ProtoEntityMap = map< hash, ProtoEntity* >;

class ProtoEntity
{
protected:
    ProtoEntity( hash proto_id, PropertyRegistrator* registartor );
    virtual ~ProtoEntity();

public:
    Properties   Props;
    const hash   ProtoId;
    mutable long RefCounter;

    string GetName() const;
    void   AddRef() const;
    void   Release() const;
};

class Entity
{
protected:
    Entity( uint id, EntityType type, PropertyRegistrator* registartor, ProtoEntity* proto );
    ~Entity();

public:
    Properties       Props;
    Methods          Meths;
    const uint       Id;
    const EntityType Type;
    ProtoEntity*     Proto;
    mutable long     RefCounter;
    bool             IsDestroyed;
    bool             IsDestroying;

    uint      GetId() const;
    void      SetId( uint id );
    hash      GetProtoId() const;
    string    GetName() const;
    void      AddRef() const;
    void      Release() const;
    EntityVec GetChildren() const;
};

class CustomEntity: public Entity
{
public:
    CustomEntity( uint id, uint sub_type, PropertyRegistrator* registrator );
    const uint SubType;
};

class GlobalVars: public Entity
{
public:
    PROPERTIES_HEADER();
    GlobalVars();
};
extern GlobalVars* Globals;

#endif // __ENTITY__
