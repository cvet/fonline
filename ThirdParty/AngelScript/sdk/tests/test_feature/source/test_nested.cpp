#include "utils.h"
#include "../../../add_on/scripthandle/scripthandle.h"
using namespace std;

static const char * const TESTNAME = "TestNested";

static void CallExecuteString(string &str)
{
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	ctx->PushState();
	int r = ExecuteString(engine, str.c_str(), 0, ctx);
	ctx->Unprepare(); // explicitly calling Unprepare is allowed but not necessary
	ctx->PopState();

	if( r < 0)
		ctx->SetException("ExecuteString() failed\n");
}

static void CallExecuteString_gen(asIScriptGeneric *gen)
{
	string str = ((CScriptString*)gen->GetArgAddress(0))->buffer;
	CallExecuteString(str);
}

static int i = 0;

CScriptHandle CreateObject(const string &s)
{
	asIScriptContext *ctx = asGetActiveContext();
	asIScriptEngine *engine = ctx->GetEngine();
	asIScriptModule *mod = engine->GetModule("test");
	asITypeInfo *type = mod->GetTypeInfoByName(s.c_str());

	// The object will start with ref count == 1
	asIScriptObject *obj = reinterpret_cast<asIScriptObject*>(engine->CreateScriptObject(type));

	// Put the object in the generic handle container for return
	CScriptHandle ref(obj, type);

	// The CScriptHandle increased the ref count when it 
	// stored the handle internally so we need to release one
	obj->Release();

	return ref;
}

string WhoIsCalling()
{
	asIScriptContext* ctx = asGetActiveContext();
	string callingFunction;

	// Test calling GetThisPointer at each level, including the ones indicating nested states
	for (asUINT n = 0; n < ctx->GetCallstackSize(); n++)
	{
		asIScriptFunction *func = ctx->GetFunction(n);
		callingFunction = func ? func->GetName() : "";

		void *obj = ctx->GetThisPointer(n);
		assert(obj == 0);
		void* var = ctx->GetAddressOfVar(0, n);
	}
	return callingFunction;
}

bool TestNested()
{
	bool fail = false;

	// Test using GetThisPointer on context with nested states
	// Reported by Patrick Jeeves
	SKIP_ON_MAX_PORT
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		COutStream out;
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
	
		engine->RegisterObjectType("Dummy", 0, asOBJ_REF | asOBJ_NOCOUNT);
		engine->RegisterObjectMethod("Dummy", "void CallExecuteString(string &in)", asFUNCTION(CallExecuteString_gen), asCALL_GENERIC);
		engine->RegisterGlobalFunction("string WhoIsCalling()", asFUNCTION(WhoIsCalling), asCALL_CDECL);
		engine->RegisterGlobalProperty("Dummy dummy", &fail);

		asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME, 
			"void TestNested()                         \n"
			"{                                         \n"
			"  dummy.CallExecuteString(\"assert(WhoIsCalling() == 'TestNested');\");           \n"
			"}                                         \n");
		mod->Build();

		// Make the call with a separate context (should work)
		asIScriptContext* ctx = engine->CreateContext();
		ctx->Prepare(engine->GetModule(0)->GetFunctionByIndex(0));
		int r = ctx->Execute();
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		ctx->Release();

		engine->ShutDownAndRelease();
	}

	// Test calling another script function from a registered function called from a script
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		RegisterScriptString_Generic(engine);

		engine->RegisterGlobalProperty("int i", &i);
		if( strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") )
			engine->RegisterGlobalFunction("void CallExecuteString(string &in)", asFUNCTION(CallExecuteString_gen), asCALL_GENERIC);
		else
			engine->RegisterGlobalFunction("void CallExecuteString(string &in)", asFUNCTION(CallExecuteString), asCALL_CDECL);

		COutStream out;

		asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME, 
			"void TestNested()                         \n"
			"{                                         \n"
			"  CallExecuteString(\"i = 2\");           \n"
			"  i = i + 2;                              \n"
			"}                                         \n");
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		mod->Build();

		// Make the call with a separate context (should work)
		asIScriptContext *ctx = engine->CreateContext();
		ctx->Prepare(engine->GetModule(0)->GetFunctionByIndex(0));
		ctx->Execute();

		if( i != 4 )
		{
			PRINTF("%s: Failed to call nested ExecuteString() from other context\n", TESTNAME);
			TEST_FAILED;
		}

		ctx->Release();

		// Make the call with ExecuteString 
		i = 0;
		int r = ExecuteString(engine, "TestNested()", mod);
		if( r != asEXECUTION_FINISHED )
		{
			PRINTF("%s: ExecuteString() didn't succeed\n", TESTNAME);
			TEST_FAILED;
		}

		if( i != 4 )
		{
			PRINTF("%s: Failed to call nested ExecuteString() from ExecuteString()\n", TESTNAME);
			TEST_FAILED;
		}

		engine->Release();
	}

	// http://www.gamedev.net/topic/632642-ref-count-fails/
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		
		COutStream out;	
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	
		RegisterStdString(engine);
		RegisterScriptHandle(engine);

#ifndef AS_MAX_PORTABILITY
		int r = engine->RegisterGlobalFunction("ref@ CreateObject(const string& in)", asFUNCTIONPR(CreateObject, (const std::string&), CScriptHandle), asCALL_CDECL); assert( r >= 0 );
#else
		int r = engine->RegisterGlobalFunction("ref@ CreateObject(const string& in)", WRAP_FN_PR(CreateObject, (const std::string&), CScriptHandle), asCALL_GENERIC); assert( r >= 0 );
#endif

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"class A { } \n"
			"class B \n"
			"{ \n"
			"  void init() \n"
            "  { \n"
            "    CreateObject('A'); \n"
			"  } \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "B b; b.init(); ", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Success
	return fail;
}
