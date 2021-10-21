#ifndef __ENTITY__
#define __ENTITY__

#include "Common.h"
#include "Properties.h"
#include "Methods.h"
#include "Interface.h"

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
class FOMsg;

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

class ProtoCustomEntity : public ProtoEntity
{
public:
	ProtoCustomEntity(hash pid, uint subType);

	vector< uint >  TextsLang;
	vector< FOMsg* > Texts;
};

typedef map< hash, ProtoCustomEntity* > ProtoCustomEntityMap;
typedef vector< ProtoCustomEntity* >    ProtoCustomEntityVec;

class CustomEntity: public Entity
{
#ifdef FONLINE_SERVER
	std::vector<uint> Observers;
#endif

public:
	PROPERTIES_HEADER();
    CustomEntity( uint id, uint sub_type, PropertyRegistrator* registrator, ProtoCustomEntity* proto);
	CustomEntity(uint id, uint sub_type, ProtoCustomEntity* proto);
    const uint SubType;

	uint GetSubType();
#ifdef FONLINE_SERVER
	bool IsObserver(uint entityId);
	bool AddObserver(uint entityId);
	bool EraseObserver(uint entityId);
	std::vector<uint> GetObservers();
#endif
#if defined FONLINE_CLIENT || defined ( FONLINE_MAPPER )
	static CustomEntity* GetCustomEntity(uint sub, uint id);
#endif
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
