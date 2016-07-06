#ifndef __SCRIPT_PRAGMAS__
#define __SCRIPT_PRAGMAS__

#include "Entity.h"
#include "angelscript.h"
#include "preprocessor.h"
#include <set>
#include <string>

#define PRAGMA_UNKNOWN    ( 0 )
#define PRAGMA_SERVER     ( 1 )
#define PRAGMA_CLIENT     ( 2 )
#define PRAGMA_MAPPER     ( 3 )

class PropertyRegistrator;
class IgnorePragma;    // just delete
class GlobalVarPragma; // just delete
class BindFuncPragma;  // [Extern = ...]
class EntityPragma;    // class NewEntity { [Protected] uint Field1; }
class PropertyPragma;  // extend class Critter { [Protected] uint LockerId; }
class MethodPragma;    // extend class Critter { [Protected] void Foo() {} }
class ContentPragma;   // improve dynamic scan
class EnumPragma;      // extend enum MyEnum { NewA, NewB }
class EventPragma;     // [Event] void MyEvent(...)
class RpcPragma;       // [ServerRpc] void MyRpc(...)
// add [Export] to allow access from other modules
// ???

typedef vector< Preprocessor::PragmaInstance > Pragmas;

class ScriptPragmaCallback: public Preprocessor::PragmaCallback
{
private:
    int              pragmaType;
    Pragmas          processedPragmas;
    bool             isError;
    IgnorePragma*    ignorePragma;
    GlobalVarPragma* globalVarPragma;
    BindFuncPragma*  bindFuncPragma;
    EntityPragma*    entityPragma;
    PropertyPragma*  propertyPragma;
    MethodPragma*    methodPragma;
    ContentPragma*   contentPragma;
    EnumPragma*      enumPragma;
    EventPragma*     eventPragma;
    RpcPragma*       rpcPragma;

public:
    ScriptPragmaCallback( int pragma_type );
    ~ScriptPragmaCallback();
    virtual void          CallPragma( const Preprocessor::PragmaInstance& pragma );
    const Pragmas&        GetProcessedPragmas();
    void                  Finish();
    bool                  IsError();
    PropertyRegistrator** GetPropertyRegistrators();
    bool                  RestoreEntity( const char* class_name, uint id, const StrMap& props_data );
    void*                 FindInternalEvent( const char* event_name );
    bool                  RaiseInternalEvent( void* event_ptr, va_list args );
    void                  RemoveEventsEntity( Entity* entity );
    void                  HandleRpc( void* context );
};

#endif // __SCRIPT_PRAGMAS__
