//
// This test shows how to register the std::string to be used in the scripts.
// It also used to verify that objects are always constructed before destructed.
//
// Author: Andreas Jonsson
//

#include "utils.h"
#include <string>
using namespace std;

namespace Test_Addon_StdString
{
	bool Test()
	{
		bool fail = false;
		int r;

		COutStream out;

		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		// Test type conversions
		r = ExecuteString(engine, 
			"string a = 123; assert( a == '123' ); \n"
			"a += 123; assert( a == '123123' ); \n");
		if (r != asEXECUTION_FINISHED) TEST_FAILED;

		r = ExecuteString(engine,
			"string a = 123.4; \n"
			"assert( a == '123.4' ); \n"
			"a += 123.4; \n"
			"assert( a == '123.4123.4' ); \n");
		if (r != asEXECUTION_FINISHED) TEST_FAILED;

		// Test find routines
		r = ExecuteString(engine,
			"string a = 'The brown fox jumped the white fox'; \n"
			"assert( a.findFirst('fox') == 10); \n"
			"assert( a.findFirstOf('fjq') == 10); \n"
			"assert( a.findFirstNotOf('The') == 3); \n"
			"assert( a.findLast('fox') == 31); \n"
			"assert( a.findLastOf('fjq') == 31); \n"
			"assert( a.findLastNotOf('fox') == 33); \n");
		if (r != asEXECUTION_FINISHED) TEST_FAILED;

		// Test insert and erase
		r = ExecuteString(engine,
			"string a; \n"
			"a.insert(5, 'hello'); \n");  // attempt to insert beyond the size of the string
		if (r != asEXECUTION_EXCEPTION) TEST_FAILED;

		r = ExecuteString(engine,
			"string a; \n"
			"a.insert(0, 'hello'); \n" // at index 1 beyond the last it is allowed
			"a.erase(2,2); \n"
			"assert( a == 'heo' ); \n"); 
		if (r != asEXECUTION_FINISHED) TEST_FAILED;

		engine->ShutDownAndRelease();

		return fail;
	}
}