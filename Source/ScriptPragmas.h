#ifndef __SCRIPT_PRAGMAS__
#define __SCRIPT_PRAGMAS__

#include "AngelScript/angelscript.h"
#include "AngelScript/Preprocessor/pragma.h"
#include <set>
#include <string>
using namespace std;

#define PRAGMA_UNKNOWN        (0)
#define PRAGMA_SERVER         (1)
#define PRAGMA_CLIENT         (2)
#define PRAGMA_MAPPER         (3)

class GlobalVarPragma;
class CrDataPragma;
class BindFuncPragma;
class BindFieldPragma;

class ScriptPragmaCallback : public Preprocessor::PragmaCallback
{
private:
	int pragmaType;
	set<string> alreadyProcessed;
	GlobalVarPragma* globalVarPragma;
	CrDataPragma* crDataPragma;
	BindFuncPragma* bindFuncPragma;
	BindFieldPragma* bindFieldPragma;

public:
	ScriptPragmaCallback(int pragma_type);
	void CallPragma(const string& name, const Preprocessor::PragmaInstance& instance);
};

#endif // __SCRIPT_PRAGMAS__