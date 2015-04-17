#ifndef __SCRIPT_PRAGMAS__
#define __SCRIPT_PRAGMAS__

#include "angelscript.h"
#include "preprocessor.h"
#include <set>
#include <string>
#include "Properties.h"

#define PRAGMA_UNKNOWN    ( 0 )
#define PRAGMA_SERVER     ( 1 )
#define PRAGMA_CLIENT     ( 2 )
#define PRAGMA_MAPPER     ( 3 )

class IgnorePragma;
class GlobalVarPragma;
class BindFuncPragma;
class PropertyPragma;

class ScriptPragmaCallback: public Preprocessor::PragmaCallback
{
private:
    int              pragmaType;
    set< string >    alreadyProcessed;
    bool             isError;
    IgnorePragma*    ignorePragma;
    GlobalVarPragma* globalVarPragma;
    BindFuncPragma*  bindFuncPragma;
    PropertyPragma*  propertyPragma;

public:
    ScriptPragmaCallback( int pragma_type, PropertyRegistrator** property_registrators );
    virtual void CallPragma( const string& name, const Preprocessor::PragmaInstance& instance );
    void         Finish();
    bool         IsError();
};

#endif // __SCRIPT_PRAGMAS__
