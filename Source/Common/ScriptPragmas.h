#pragma once

#include "Common.h"

#include "DataBase.h"
#include "Entity.h"

#include "angelscript.h"
#include "preprocessor.h"

#define PRAGMA_UNKNOWN (0)
#define PRAGMA_SERVER (1)
#define PRAGMA_CLIENT (2)
#define PRAGMA_MAPPER (3)

class PropertyRegistrator;
class IgnorePragma; // just delete
class GlobalVarPragma; // just delete
class EntityPragma; // class NewEntity { [Protected] uint Field1; }
class PropertyPragma; // extend class Critter { [Protected] uint LockerId; }
class ContentPragma; // improve dynamic scan
class EnumPragma; // extend enum MyEnum { NewA, NewB }
class EventPragma; // [Event] void MyEvent(...)
class RpcPragma; // [ServerRpc] void MyRpc(...)
// add [Export] to allow access from other modules
// ???

class EntityManager;
class ProtoManager;
using Pragmas = vector<Preprocessor::PragmaInstance>;

class ScriptPragmaCallback : public Preprocessor::PragmaCallback
{
private:
    int pragmaType;
    Pragmas processedPragmas;
    bool isError;
    IgnorePragma* ignorePragma;
    GlobalVarPragma* globalVarPragma;
    EntityPragma* entityPragma;
    PropertyPragma* propertyPragma;
    ContentPragma* contentPragma;
    EnumPragma* enumPragma;
    EventPragma* eventPragma;
    RpcPragma* rpcPragma;

public:
    ScriptPragmaCallback(int pragma_type, ProtoManager* proto_mngr, EntityManager* entity_mngr);
    ~ScriptPragmaCallback();
    virtual void CallPragma(const Preprocessor::PragmaInstance& pragma);
    const Pragmas& GetProcessedPragmas();
    void Finish();
    bool IsError();
    PropertyRegistrator** GetPropertyRegistrators();
    StrVec GetCustomEntityTypes();
#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    bool RestoreCustomEntity(const string& class_name, uint id, const DataBase::Document& doc);
#endif
    void* FindInternalEvent(const string& event_name);
    bool RaiseInternalEvent(void* event_ptr, va_list args);
    const IntVec& GetInternalEventArgInfos(void* event_ptr);
    void RemoveEventsEntity(Entity* entity);
    void HandleRpc(void* context);
};
