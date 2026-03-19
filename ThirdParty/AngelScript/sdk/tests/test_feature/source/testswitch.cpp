// 
// Test designed to verify functionality of the switch case
//
// Author: Andreas Jönsson
//

#include "utils.h"
using namespace std;


static const char * const TESTNAME = "TestSwitch";


static int sum = 0;

static void add(asIScriptGeneric *gen)
{
	sum += (int)gen->GetArgDWord(0);
}

static string _log;
static void Log(asIScriptGeneric *gen)
{
	CScriptString *str = (CScriptString *)gen->GetArgObject(0);
	_log += str->buffer;
}

bool TestSwitch()
{
	bool fail = false;
	int r;

	// Test switch with large 32bit values and values far apart (especially the last case, so it is optimized into a jump table)
	// Reported by power_mcu
	{
		CBufferedOutStream bout;
		asIScriptEngine* engine = asCreateScriptEngine();
		r = engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		asIScriptModule* mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void start(int state, int expectedResult) \n"
			"{ \n"
			"	int result = -1; \n"
			"	if (state == 0) { result = 0; } \n"
			"	else if (state == 1) { result = 1; } \n"
			"	else if (state == 2) { result = 2; } \n"
			"	else if (state == 3) { result = 3; } \n"
			"	else if (state == 2147483642) { result = 4; } \n"
			"	else if (state == 2147483643) { result = 5; } \n"
			"	else if (state == 2147483647) { result = 6; } \n"
			"	else if (state == int(2147483648)) { result = 7; } \n" // without int() it will attempt a int64 comparison which will not match the incoming value of -2147483648
			"	else { result = 8; } \n"
			"	assert(result == expectedResult); \n"
			" \n"
			"	result = -1; \n"
			"	switch (state) \n"
			"	{ \n"
			"	case 0: result = 0; break; \n"
			"	case 1: result = 1; break; \n"
			"	case 2: result = 2; break; \n"
			"	case 3: result = 3; break; \n"
			"	case 2147483642: result = 4; break; \n"
			"	case 2147483643: result = 5; break; \n"
			"	case 2147483647: result = 6; break; \n" // 0x7FFFFFFF. highest number is 
			"	case 2147483648: result = 7; break; \n" // 0x80000000. gets implicitly converted to int (with a warning), i.e. -2147483648
			"	default: result = 8; break; \n"
			"	} \n"
			"	assert(result == expectedResult); \n"
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "start(2147483642, 4)", mod); // 0x7FFFFFFA
		if( r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "start(2147483643, 5)", mod); // 0x7FFFFFFB
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "start(2147483647, 6)", mod); // 0x7FFFFFFF
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "start(2147483648, 7)", mod); // 0x80000000. gets implicitly converted to int (with a warning), i.e. -2147483648
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "start(2147483649, 8)", mod); // 0x80000001. gets implicitly converted to int (with a warning), i.e. -2147483647
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		engine->ShutDownAndRelease();
		if (bout.buffer != "test (1, 1) : Info    : Compiling void start(int, int)\n"
						   "test (25, 7) : Warning : Value is too large for data type\n"
						   "ExecuteString (1, 7) : Warning : Value is too large for data type\n"
						   "ExecuteString (1, 7) : Warning : Value is too large for data type\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test switch with typedef and enums
	// Reported by 1vanK from Urho3D
	{
		CBufferedOutStream bout;

		asIScriptEngine* engine = asCreateScriptEngine();
		r = engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule* mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"enum FOO { BAR } \n"
			"typedef int HI; \n"
			"void func1() { \n"
			"  FOO f; switch( f ) { case BAR: } \n"
			"  HI h; switch( h ) { case 0: } \n"
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test case with declared global constants
	{
		asIScriptEngine* engine;
		CBufferedOutStream bout;

		engine = asCreateScriptEngine();
		r = engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule* mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"const int B = 0; \n"
			// when compiled together it works
			"void func1() { int i; switch( i ) { case B: } } \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		// TODO: This currently fails, because when compiled separately the global B isn't interpreted as a literal constant
		r = ExecuteString(engine, "int i; switch( i ) { case B: }", mod);
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "ExecuteString (1, 27) : Error   : Case expressions must be literal constants\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test attempt to use register global const property as switch case
	{
		asIScriptEngine* engine;
		CBufferedOutStream bout;

		engine = asCreateScriptEngine();
		r = engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		int B;
		r = engine->RegisterGlobalProperty("const int B", (void*)&B);

		r = ExecuteString(engine, "int i; switch( i ) { case B: }");
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "ExecuteString (1, 27) : Error   : Case expressions must be literal constants\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test that compiler is able to detect if all paths in a switch has return statement
	{
		asIScriptEngine* engine = asCreateScriptEngine();
		CBufferedOutStream bout;
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"int a; \n"
			"int hasReturn() { \n"
			"  switch(a) { \n"
			"    case 0: \n"  // has return
			"      if(true) return 0; else return 0; \n"
			"      break; \n" // unreachable code
			"    case 1: \n"  // no return, but falls through to next case
			"    case 2: \n"  // has return 
			"      return 0;"
			"    default: \n"
			"      return 0; \n"
			"  } \n"
			"  return 0; \n" // unreachable code
			"} \n"
			"int noReturn1() { \n"
			"  switch(a) { \n"
			"    case 0: \n"  // no return and ends
			"      break; \n"
			"  } \n"
			"} \n"
			"int noReturn2() { \n"
			"  switch(a) { \n"
			"    case 0: \n"  // has return but no default
			"      return 0; \n"
			"  } \n"
			"} \n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;
		if (bout.buffer != "test (2, 1) : Info    : Compiling int hasReturn()\n"
						   "test (6, 7) : Warning : Unreachable code\n"
						   "test (12, 3) : Warning : Unreachable code\n"
						   "test (14, 1) : Info    : Compiling int noReturn1()\n"
						   "test (14, 17) : Error   : Not all paths return a value\n"
						   "test (20, 1) : Info    : Compiling int noReturn2()\n"
						   "test (20, 17) : Error   : Not all paths return a value\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}



	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	RegisterScriptString(engine);
	engine->RegisterGlobalFunction("void Log(const string &in)", asFUNCTION(Log), asCALL_GENERIC);

	engine->RegisterGlobalFunction("void add(int)", asFUNCTION(add), asCALL_GENERIC);

	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	const char *script =
		"void _switch()                  \n"  // 1
		"{                               \n"  // 2
		"  for( int n = -1; n < 2; ++n ) \n"  // 3
		"    switch( n )                 \n"  // 4
		"    {                           \n"  // 5
		"    case 0:                     \n"  // 6
		"      add(0);                   \n"  // 7
		"      break;                    \n"  // 8
		"    case -1:                    \n"  // 9
		"      add(-1);                  \n"  // 10
		"      break;                    \n"  // 11
		"    case 0x5:                   \n"  // 12
		"      add(5);                   \n"  // 13
		"      break;                    \n"  // 14
		"    case 0xF:                   \n"  // 15
		"      add(15);                  \n"  // 16
		"      break;                    \n"  // 17
		"    default:                    \n"  // 18
		"      add(255);                 \n"  // 19
		"      break;                    \n"  // 20
		"    }                           \n"  // 21
		"}                               \n"; // 22

	mod->AddScriptSection("switch", script);
	r = mod->Build();
	if( r < 0 )
	{
		PRINTF("%s: Failed to build script\n", TESTNAME);
		TEST_FAILED;
	}

	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(engine->GetModule(0)->GetFunctionByDecl("void _switch()"));
	ctx->Execute();

	if( sum != 254 )
	{
		PRINTF("%s: Expected %d, got %d\n", TESTNAME, 254, sum);
		TEST_FAILED;
	}

	ctx->Release();

	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	const char *script2 =
		"const int a = 1;                           \n"
		"const int8 b = 2;                          \n"
		"void _switch2()                            \n"
		"{                                          \n"
		"  const uint c = 3;                        \n"
		"  for( uint8 n = 0; n <= 5; ++n )          \n"
		"  {                                        \n"
		"    switch( n )                            \n"
		"    {                                      \n"
		"    case 5: Log(\"5\"); break;             \n"
		"    case 4: Log(\"4\"); break;             \n"
		"    case c: Log(\"3\"); break;             \n"
		"    case b: Log(\"2\"); break;             \n"
		"    case a: Log(\"1\"); break;             \n"
		"    default: Log(\"d\"); break;            \n"
		"    }                                      \n"
		"  }                                        \n"
		"  Log(\"\\n\");                            \n"
		"  myFunc127(127);                          \n"
		"  myFunc128(128);                          \n"
		"}                                          \n"
		"const uint8 c127 = 127;                    \n"
		"void myFunc127(uint8 value)                \n"
		"{                                          \n"
		"  if(value == c127)                        \n"
		"    Log(\"It is the value we expect\\n\"); \n"
		"                                           \n"
		"  switch(value)                            \n"
		"  {                                        \n"
		"    case c127:                             \n"
		"      Log(\"The switch works\\n\");        \n"
		"      break;                               \n"
		"    default:                               \n"
		"      Log(\"I didnt work\\n\");            \n"
		"      break;                               \n"
		"  }                                        \n"
		"}                                          \n"
		"const uint8 c128 = 128;                    \n"
		"void myFunc128(uint8 value)                \n"
		"{                                          \n"
		"  if(value == c128)                        \n"
		"    Log(\"It is the value we expect\\n\"); \n"
		"                                           \n"
		"  switch(value)                            \n"
		"  {                                        \n"
		"    case c128:                             \n"
		"      Log(\"The switch works\\n\");        \n"
		"      break;                               \n"
		"    default:                               \n"
		"      Log(\"I didnt work\\n\");            \n"
		"      break;                               \n"
		"  }                                        \n"
		"}                                          \n";
	mod->AddScriptSection("switch", script2);
	mod->Build();

	ExecuteString(engine, "_switch2()", mod);

	if( _log != "d12345\n"
		        "It is the value we expect\n"
                "The switch works\n"
                "It is the value we expect\n"
                "The switch works\n" )
	{
		TEST_FAILED;
		PRINTF("%s: Switch failed. Got: %s\n", TESTNAME, _log.c_str());
	}
 
	CBufferedOutStream bout;
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	ExecuteString(engine, "switch(1) {}", mod); 
	if( bout.buffer != "ExecuteString (1, 1) : Error   : Empty switch statement\n" )
		TEST_FAILED;

	// Default case in the middle
	{
		bout.buffer = "";
		script =
			"int func(int d) { \n"
			"  switch (d) \n"
			"  { \n"
			"	case 0: \n"
			"	default: \n"
			"		return (4 * 16); \n"
			"	case 1: \n"
			"		return (4 * 16); \n"
			"	case 2: \n"
			"		return (5 * 16); \n"
			"	case 3: \n"
			"		return (9 * 16); \n"
			"  } \n"
			"  return 0; \n"
			"} \n";

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("name", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "name (1, 1) : Info    : Compiling int func(int)\n"
		                   "name (5, 2) : Error   : The default case must be the last one\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// A switch case must not have duplicate cases
	{
		bout.buffer = "";
		script = "switch( 1 ) { case 1: case 1: }";
		r = ExecuteString(engine, script, mod);
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "ExecuteString (1, 28) : Error   : Duplicate switch case\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test to make sure assert is not failing
	{
		bout.buffer = "";
		script = 
			"class Test \n"
			"{ \n"
			"	int8 State; \n"
			"	void test() \n"
			"	{ \n"
			"		switch (State) \n"
			"		{ \n"
			"       case 0: \n"
			"		} \n"
			"	} \n"
			"}; \n";

		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test to make sure the error message is appropriate when declaring variable in case
	{
		bout.buffer = "";
		script = 
			"	void test() \n"
			"	{ \n"
			"		switch (0) \n"
			"		{ \n"
			"       case 0: \n"
			"         int n; \n"
			"		} \n"
			"	} \n";

		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "script (1, 2) : Info    : Compiling void test()\n"
		                   "script (6, 10) : Error   : Variables cannot be declared in switch cases, except inside statement blocks\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test error message for switch cases where the cases are strings
	{
		bout.buffer = "";
		script = 
			"void func() { \n"
			"  int test; \n"
			"  switch (undeclaredString) \n"
			"  { \n"
			"  case 'a': \n"
			"    test = 1; \n"
			"  } \n"
			"} \n";

		mod->AddScriptSection("script", script);
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;
		if (bout.buffer != "script (1, 1) : Info    : Compiling void func()\n"
						   "script (3, 11) : Error   : No matching symbol 'undeclaredString'\n"
						   "script (5, 8) : Error   : Switch expressions must be integral numbers\n"
						   "script (3, 3) : Error   : Empty switch statement\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	engine->Release();

	return fail;
}
