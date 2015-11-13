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
    ClientMap,
    ClientLocation,
    ProtoItem,
    ProtoItemExt,
    ProtoCritter,
    Max,
};

class Entity
{
protected:
    Entity( uint id, EntityType type, PropertyRegistrator* registartor );

public:
    Properties       Props;
    Methods          Meths;
    const uint       Id;
    const EntityType Type;
    hash             ProtoId;
    mutable long     RefCounter;
    bool             IsDestroyed;
    bool             IsDestroying;

    // Entity*   Parent;
    // EntityVec Children;
    // void       SetParent( Entity* parent ) { Parent = parent; }
    // EntityType GetParentType()             { return Parent ? Parent->Type : EntityType::None; }
    // uint       GetParentId()               { return Parent ? Parent->Id : 0; }

    uint GetId();
    void SetId( uint id );
    void AddRef() const;
    void Release() const;
};
typedef vector< Entity* >    EntityVec;
typedef map< uint, Entity* > EntityMap;

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
    CLASS_PROPERTY( ScriptArray *, BestScores );
    GlobalVars();
};
extern GlobalVars* Globals;

class ClientMap: public Entity
{
public:
    PROPERTIES_HEADER();
    ClientMap();
};
extern ClientMap* ClientCurMap;

class ClientLocation: public Entity
{
public:
    PROPERTIES_HEADER();
    ClientLocation();
};
extern ClientLocation* ClientCurLocation;

#endif // __ENTITY__
