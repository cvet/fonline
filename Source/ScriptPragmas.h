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
class PropertyPragma;
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
    PropertyPragma*  propertyPragma;
    ContentPragma*   contentPragma;

public:
    ScriptPragmaCallback( int pragma_type, PropertyRegistrator** property_registrators );
    ~ScriptPragmaCallback();
    virtual void   CallPragma( const Preprocessor::PragmaInstance& pragma );
    const Pragmas& GetProcessedPragmas();
    void           Finish();
    bool           IsError();
};

#endif // __SCRIPT_PRAGMAS__
