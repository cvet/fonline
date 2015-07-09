#ifndef __SCRIPT_PRAGMAS__
#define __SCRIPT_PRAGMAS__

#include "angelscript.h"
#include "preprocessor.h"
#include <set>
#include <string>

#define PRAGMA_UNKNOWN    ( 0 )
#define PRAGMA_SERVER     ( 1 )
#define PRAGMA_CLIENT     ( 2 )
#define PRAGMA_MAPPER     ( 3 )

class PropertyRegistrator;
class IgnorePragma;
class GlobalVarPragma;
class BindFuncPragma;
class EntityPragma;
class PropertyPragma;
class MethodPragma;
class ContentPragma;

typedef vector< Preprocessor::PragmaInstance > Pragmas;

class ScriptPragmaCallback: public Preprocessor::PragmaCallback
{
private:
    int              pragmaType;
    set< string >    alreadyProcessed;
    Pragmas          processedPragmas;
    bool             isError;
    IgnorePragma*    ignorePragma;
    GlobalVarPragma* globalVarPragma;
    BindFuncPragma*  bindFuncPragma;
    EntityPragma*    entityPragma;
    PropertyPragma*  propertyPragma;
    MethodPragma*    methodPragma;
    ContentPragma*   contentPragma;

public:
    ScriptPragmaCallback( int pragma_type );
    ~ScriptPragmaCallback();
    virtual void          CallPragma( const Preprocessor::PragmaInstance& pragma );
    const Pragmas&        GetProcessedPragmas();
    void                  Finish();
    bool                  IsError();
    PropertyRegistrator** GetPropertyRegistrators();
    PropertyRegistrator*  FindEntityRegistrator( const char* class_name );
    void                  RestoreEntity( const char* class_name, uint id, Properties& props );
};

#endif // __SCRIPT_PRAGMAS__
