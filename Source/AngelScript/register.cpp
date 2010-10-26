
#include "as_config.h"
#include "scriptany.h"
#include "scriptdictionary.h"
#include "scriptfile.h"
#include "scriptmath.h"
#include "scriptmath3d.h"
#include "scriptstring.h"
#include "scriptarray.h"
#include <locale.h>

extern "C" __declspec(dllexport) asIScriptEngine* Register()
{
	setlocale(LC_ALL,"Russian");
	asIScriptEngine* engine=asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if(!engine) return NULL;
	RegisterScriptArray(engine,true);
	RegisterScriptString(engine);
	RegisterScriptStringUtils(engine);
	RegisterScriptAny(engine);
	RegisterScriptDictionary(engine);
	RegisterScriptFile(engine);
	RegisterScriptMath(engine);
	RegisterScriptMath3D(engine);
	return engine;
}