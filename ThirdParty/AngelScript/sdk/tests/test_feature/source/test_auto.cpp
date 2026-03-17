#include "utils.h"
#include "../../add_on/scriptmath/scriptmathcomplex.h"

using namespace std;

namespace TestAuto
{

bool Test()
{
	bool fail = false;
	int r;
	CBufferedOutStream bout;
	COutStream out;
	asIScriptModule *mod;
	asIScriptEngine *engine;

	// Test auto when it is not possible to determine type from expression
	// http://www.gamedev.net/topic/677273-various-unexpected-behaviors-of-angelscript-2310/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		// local vars
		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", "void func() { auto var = null; }");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		// global vars
		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", "auto var = null;");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		// class members
		// TODO: Currently auto in class member declarations are not supported, but if they are one day then this must also be properly handled
		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", "class foo { auto var = null; }");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (1, 1) : Info    : Compiling void func()\n"
						   "test (1, 20) : Error   : Unable to resolve auto type\n"
						   "test (1, 6) : Info    : Compiling <auto> var\n"
						   "test (1, 6) : Error   : Unable to resolve auto type\n"
						   "test (1, 13) : Error   : Auto is not allowed here\n"
						   "test (1, 30) : Error   : Unexpected token '}'\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test that anonymous functions with auto generates proper error
	// http://www.gamedev.net/topic/671632-auto-causes-crash-with-anonymous-functions/
	{
		const char *script =
			"auto g_cb2 = function() {}; \n"
			"auto @g_cb3 = function() {}; \n"
			"void test() { \n"
			" auto cb2 = function() {}; \n"
			" auto @cb3 = function() {}; \n"
			"} \n";

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (1, 6) : Info    : Compiling <auto> g_cb2\n"
						   "test (1, 6) : Error   : Unable to resolve auto type\n"
						   "test (2, 7) : Info    : Compiling <auto>@ g_cb3\n"
						   "test (2, 7) : Error   : Unable to resolve auto type\n"
						   "test (3, 1) : Info    : Compiling void test()\n"
						   "test (4, 7) : Error   : Unable to resolve auto type\n"
						   "test (5, 8) : Error   : Unable to resolve auto type\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	//Test auto from literals
	{
		const char *script =
			"void test() {\n"
			"  auto anInt = 18; assert(anInt == 18);\n"
			"  auto aFloat = 1.24f; assert(aFloat == 1.24f);\n"
			"  auto aString = \"test\"; assert(aString == \"test\");\n"
			"}\n"
			"\n";

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "test();", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	//Test auto from expressions
	{
		const char *script =
			"int getInt() { return 18; }\n"
			"float getFloat() { return 18.f; }\n"
			"void test() {\n"
			"  auto anInt = getInt(); assert(anInt == 18);\n"
			"  auto aFloat = getFloat(); assert(aFloat == 18.f);\n"
			"  auto combined = anInt + aFloat; assert(combined == (18 + 18.f));\n"
			"}\n"
			"\n";

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "test();", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	//Test global auto
	{
		const char *script =
			"const auto aConstant = 18;\n"
			"auto str = \"some string\";\n"
			"auto concat = str + aConstant;\n"
			"void test() {\n"
			"  assert(aConstant == 18);\n"
			"  assert(str == \"some string\");\n"
			"  assert(concat == \"some string18\");\n"
			"}\n"
			"\n";

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "test();", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	//Test handles to auto
	{
		const char *script =
			"class Object {}\n"
			"Object obj;\n"
			"Object@ getHandleOf() { return obj; }\n"
			"Object& getReferenceOf() { return obj; }\n"
			"Object getValueOf() { return obj; }\n"
			"void test() {\n"
			"  auto copy = obj; assert(copy !is obj);\n"
			"  auto@ handle = obj; assert(handle is obj);\n"
			"  auto handleReturn = getHandleOf(); assert(handleReturn is obj);\n"
			"  auto@ explicitHandle = getHandleOf(); assert(explicitHandle is obj);\n"
			"  auto copyReturn = getReferenceOf(); assert(copyReturn !is obj);\n"
			"  auto valueReturn = getValueOf(); assert(valueReturn !is obj);\n"
			"  auto@ handleToReference = getReferenceOf(); assert(handleToReference is obj);\n"
			"  auto@ handleToCopy = getValueOf(); assert(handleToCopy !is obj);\n"
			"}\n"
			"\n";

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "test();", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	//Test that auto cannot be used in class bodies
	// This is because class member initialization is done inside the constructor,
	// and can access arguments from the constructor. There is no sane way to auto-resolve those.
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		mod = engine->GetModule("mod1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"class Object {\n"
			"  auto member = 1;\n"
			"}\n"
			"\n");

		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (2, 3) : Error   : Auto is not allowed here\n"
						   "test (3, 1) : Error   : Unexpected token '}'\n"
						   )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	//Test that circular autos in globals fails
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		mod = engine->GetModule("mod1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"auto x = y;"
			"auto y = x;"
			"\n");

		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (1, 6) : Info    : Compiling <auto> x\n"
						   "test (1, 10) : Error   : 'y' is not declared\n"
						   "test (1, 17) : Info    : Compiling <auto> y\n"
						   "test (1, 21) : Error   : 'x' is not declared\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test error when no expression is given
	// http://www.gamedev.net/topic/655641-assert-when-compiling-with-auto/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		r = ExecuteString(engine, "auto i;");
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "ExecuteString (1, 6) : Error   : Unable to resolve auto type\n" )
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

