#include "utils.h"
#if AS_CAN_USE_CPP11
#include "../../../add_on/datetime/datetime.h"
#endif
#include <sstream>

namespace Test_Addon_DateTime
{

bool Test()
{
#if !defined(AS_CAN_USE_CPP11)
	PRINTF("Skipped due to lack of C++11 support\n");
	return false;
#else

	bool fail = false;
	int r;
	COutStream out;
	CBufferedOutStream bout;
	asIScriptEngine *engine;

	// Test the datetime object
	SKIP_ON_MAX_PORT
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		RegisterScriptDateTime(engine);

		CDateTime dt;
		std::stringstream s;
		s << "datetime dt; assert( dt.year == " << dt.getYear() << " && dt.month == " << dt.getMonth() << " );";

		r = ExecuteString(engine, s.str().c_str());
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		engine->Release();
	}

	// Success
	return fail;
#endif
}


} // namespace

