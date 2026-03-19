#include "utils.h"
#include "../../../add_on/autowrapper/aswrappedcall.h"

namespace Test_Addon_Autowrapper
{

bool Test()
{
	bool fail = false;
	int r;
	COutStream out;
	CBufferedOutStream bout;
	asIScriptEngine *engine;

	// Test use of WRAP_FN_PR with std math functions
	// https://github.com/anjo76/angelscript/issues/26
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		r = engine->RegisterGlobalFunction("float atan2(float, float)", WRAP_FN_PR(atan2, (float, float), float), asCALL_GENERIC);
		if( r < 0 )
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Success
	return fail;
}


} // namespace

