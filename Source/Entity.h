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
    HashSet      Components;
    const hash   ProtoId;
    mutable long RefCounter;

    string GetName() const;
    bool   HaveComponent( hash name ) const;
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
    EntityVec GetChildren() const;
    void      AddRef() const;
    void      Release() const;
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
    CLASS_PROPERTY( ushort, YearStart );
    CLASS_PROPERTY( ushort, Year );
    CLASS_PROPERTY( ushort, Month );
    CLASS_PROPERTY( ushort, Day );
    CLASS_PROPERTY( ushort, Hour );
    CLASS_PROPERTY( ushort, Minute );
    CLASS_PROPERTY( ushort, Second );
    CLASS_PROPERTY( ushort, TimeMultiplier );
    CLASS_PROPERTY( uint, LastEntityId );
    CLASS_PROPERTY( uint, LastDeferredCallId );
    CLASS_PROPERTY( uint, HistoryRecordsId );

    GlobalVars();
};
extern GlobalVars* Globals;

#endif // __ENTITY__
