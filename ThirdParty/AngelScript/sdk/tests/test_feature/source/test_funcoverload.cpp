#include "utils.h"
#include "../../../add_on/scriptdictionary/scriptdictionary.h"

namespace TestFuncOverload
{
static const char* const TESTNAME = "TestFuncOverload";

class Obj
{
public:
	void* p;
	void* Value() { return p; }
	void Set(const std::string&, void*) {}
};

static Obj o;

void FuncVoid()
{
}

void FuncInt(int)
{
}

bool Test2();

bool script_dictionary_get_string(CScriptDictionary* dict, const std::string& key, std::string* value) {
	return dict->Get(key, value, asGetActiveContext()->GetEngine()->GetStringFactory());
}

int set(asINT64) { return 0; }
int get(asINT64&) { return 0; }
int set(double) { return 1; }
int get(double&) { return 1;}
int set(const std::string& ){return 2;}
int get(std::string&) { return 2; }
int set(void*, int /*typeId*/) { return 3; }
int get(void*, int /*typeId*/) { return 3; }

bool Test()
{
	bool fail = Test2();
	COutStream out;
	CBufferedOutStream bout;

	// Test function overload for &out parameters. The same prioritization order is used as for input arguments, even  
	// though technically the &out parameter will be converted to the argument and not the other way around
	SKIP_ON_MAX_PORT
	{
		asIScriptEngine* engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		RegisterStdString(engine);

		engine->RegisterGlobalFunction("int set(int64)", asFUNCTIONPR(set, (asINT64), int), asCALL_CDECL);
		engine->RegisterGlobalFunction("int get(int64 &out)", asFUNCTIONPR(get, (asINT64 &), int), asCALL_CDECL);
		engine->RegisterGlobalFunction("int set(double)", asFUNCTIONPR(set, (double), int), asCALL_CDECL);
		engine->RegisterGlobalFunction("int get(double &out)", asFUNCTIONPR(get, (double&), int), asCALL_CDECL);
		engine->RegisterGlobalFunction("int set(const string &in)", asFUNCTIONPR(set, (const std::string &), int), asCALL_CDECL);
		engine->RegisterGlobalFunction("int get(string &out)", asFUNCTIONPR(get, (std::string &), int), asCALL_CDECL);
		engine->RegisterGlobalFunction("int set(const ? &in)", asFUNCTIONPR(set, (void *, int), int), asCALL_CDECL);
		engine->RegisterGlobalFunction("int get(? &out)", asFUNCTIONPR(get, (void *, int), int), asCALL_CDECL);

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		asIScriptModule* mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", R"(
class c_from_str {
    c_from_str(string s = "") {}
}
class c_to_str {
	string opImplConv() { return ""; }
}
enum E { val }
void main() {
    c_from_str b;
    assert(set(b) == 3); // match set(?), since c_from_str cannot be converted to string
	assert(get(b) == 3); // match get(?), since c_from_str cannot be converted to string (even though a string can be converted to c_from_str)
	c_to_str a;
	assert(set(a) == 2); // match set(string), since c_to_str can be converted to string
	assert(get(a) == 3); // match get(?), since c_to_str cannot be created from string
	string str;
	assert(set(str) == 2);
	assert(get(str) == 2); 
	int i = 0;
	assert(set(i) == 0);
	assert(get(i) == 0);
	float f = 0;
	assert(set(f) == 1);
	assert(get(f) == 1);
	E e = val;
	assert(set(e) == 0); // match set(int64)
	assert(get(e) == 3); // match get(?), since int64 cannot be converted to enum
}
)");
		int r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		asIScriptContext* ctx = engine->CreateContext();
		r = ExecuteString(engine, "main()", mod, ctx);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("%s", GetExceptionInfo(ctx).c_str());
			TEST_FAILED;
		}
		ctx->Release();

		engine->ShutDownAndRelease();

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test proper function overload for &out parameters
	// Reported by Sam Tupy
	SKIP_ON_MAX_PORT
	{
		asIScriptEngine *engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		RegisterStdString(engine);
		RegisterScriptArray(engine, false);
		RegisterScriptDictionary(engine);

		// TODO: Need to have an engine property to allow &out to match arg for &out using arg to param type (as was done before 2.38.0), instead of param to arg type (as is done in 2.38.0)

		engine->RegisterObjectMethod("dictionary", "bool get(const string&in key, string&out value) const", asFUNCTION(script_dictionary_get_string), asCALL_CDECL_OBJFIRST);

		// Now there are 4 overloads for get
		//  get(key, int64 &out)
		//  get(key, double &out)
		//  get(key, string &out)
		//  get(key, ? &out)

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", R"(
class broken {
    broken(string s = "") {}
}
void main() {
    dictionary d = {{"broken", @broken()}, {"int", 123}, {"dbl", 3.14}, {"str", "text"}};
    broken@ b;
    assert(d.get("broken", @b) && b !is null); // match get(?), even though string can be converted to broken
	string str;
	assert(d.get("str", str) && str == "text"); // match get(string)
	int i;
	assert(d.get("int", i) && i == 123); // match get(int64)
	float f;
	assert(d.get("dbl", f) && f == 3.14); // match get(double)
}
)");
		int r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		asIScriptContext* ctx = engine->CreateContext();
		r = ExecuteString(engine, "main()", mod, ctx);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("%s", GetExceptionInfo(ctx).c_str());
			TEST_FAILED;
		}
		ctx->Release();

		engine->ShutDownAndRelease();

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test function overload between obj@ and const obj@
	// Reported by Patrick Jeeves
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);


		asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		const char* script1 =
			"int called = 0; \n"
			"MeshComponent@ GetMesh(GameObject@, int part = 0) { called = 1; return null; } \n"
			"const MeshComponent@ GetMesh(const GameObject@, int part = 0) { called = 2; return null; } \n"
			"class MeshComponent {} \n"
			"class GameObject {} \n";
		mod->AddScriptSection(TESTNAME, script1);
		int r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "GameObject @obj; const GameObject @cobj = obj; GetMesh(cobj); assert( called == 2 );", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		r = ExecuteString(engine, "GameObject @obj; GetMesh(obj); assert( called == 1 );", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test function overload where one option is ?&in
	// https://www.gamedev.net/forums/topic/715439-recent-change-fixing-funtion-overload-between-obj-and-const-obj-broke-a-lot-of-bindings/5460950/
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		RegisterStdString(engine);

		engine->RegisterGlobalFunction("void PushID(const ?&in)", asFUNCTION(0), asCALL_GENERIC);
		engine->RegisterGlobalFunction("void PushID(const string &in)", asFUNCTION(0), asCALL_GENERIC);

		asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		const char* script1 =
			"class wstring { string opImplConv() const { return ''; } } \n"
			"void main() { \n"
			"  wstring s; \n"
			"  PushID(s); \n"
			"} \n";
		mod->AddScriptSection(TESTNAME, script1);
		int r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test function overload when there are two options, one taking obj@ and another taking const obj@
	// Reported by Patrick Jeeves
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		const char* script1 =
			"class Foo { int k; }; \n"
			"int Func0(Foo@) { return 1; } \n"
			"int Func0(const Foo@) { return 2; } \n"
			"void main() { Foo f; \nassert( Func0(f) == 1 ); \nassert( Func0(cast<const Foo>(f)) == 2 ); } \n";
		mod->AddScriptSection(TESTNAME, script1);
		int r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "main()", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		engine->ShutDownAndRelease();
	}

	// Test basic function overload
	SKIP_ON_MAX_PORT
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptString(engine);

		engine->RegisterObjectType("Data", sizeof(void*), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE);

		engine->RegisterObjectType("Obj", sizeof(Obj), asOBJ_REF | asOBJ_NOHANDLE);
		engine->RegisterObjectMethod("Obj", "Data &Value()", asMETHOD(Obj, Value), asCALL_THISCALL);
		engine->RegisterObjectMethod("Obj", "void Set(string &in, Data &in)", asMETHOD(Obj, Set), asCALL_THISCALL);
		engine->RegisterObjectMethod("Obj", "void Set(string &in, string &in)", asMETHOD(Obj, Set), asCALL_THISCALL);
		engine->RegisterGlobalProperty("Obj TX", &o);

		engine->RegisterGlobalFunction("void func()", asFUNCTION(FuncVoid), asCALL_CDECL);
		engine->RegisterGlobalFunction("void func(int)", asFUNCTION(FuncInt), asCALL_CDECL);

		asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		const char* script1 =
			"void Test()                               \n"
			"{                                         \n"
			"  TX.Set(\"user\", TX.Value());           \n"
			"}                                         \n";
		mod->AddScriptSection(TESTNAME, script1, strlen(script1), 0);
		int r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "func(func(3));", mod);
		if (r != asEXECUTION_FINISHED) TEST_FAILED;

		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		const char* script2 =
			"void ScriptFunc(void m)                   \n"
			"{                                         \n"
			"}                                         \n";
		mod->AddScriptSection(TESTNAME, script2, strlen(script2), 0);
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;
		if (bout.buffer != "TestFuncOverload (1, 1) : Info    : Compiling void ScriptFunc(void)\n"
			"TestFuncOverload (1, 1) : Error   : Parameter type can't be 'void', because the type cannot be instantiated.\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// Permit void parameter list
		r = engine->RegisterGlobalFunction("void func2(void)", asFUNCTION(FuncVoid), asCALL_CDECL); assert(r >= 0);

		// Don't permit void parameters
		r = engine->RegisterGlobalFunction("void func3(void n)", asFUNCTION(FuncVoid), asCALL_CDECL); assert(r < 0);

		engine->Release();
	}

	return fail;
}

// This test verifies that it is possible to find a best match even if the first argument
// may give a better match for another function. Also the order of the function declarations
// should not affect the result.
bool Test2()
{
	bool fail = false;
	COutStream out;
	int r;

	asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
	engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

	const char* script1 =
		"class A{}  \n"
		"class B{}  \n"
		"int choice; \n"
		"void func(A&in, A&in) { choice = 1; } \n"
		"void func(const A&in, const B&in) { choice = 2; } \n"
		"void test()  \n"
		"{ \n"
		"  A a; B b; \n"
		"  func(a,b); \n"
		"  assert( choice == 2 ); \n"
		"}\n";

	asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	r = mod->AddScriptSection("test", script1, strlen(script1));
	r = mod->Build();
	if (r < 0)
		TEST_FAILED;
	r = ExecuteString(engine, "test()", mod);
	if (r != asEXECUTION_FINISHED)
		TEST_FAILED;

	const char* script2 =
		"class A{}  \n"
		"class B{}  \n"
		"int choice; \n"
		"void func(const A&in, const B&in) { choice = 1; } \n"
		"void func(A&in, A&in) { choice = 2; } \n"
		"void test()  \n"
		"{ \n"
		"  A a; B b; \n"
		"  func(a,b); \n"
		"  assert( choice == 1 ); \n"
		"}\n";

	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	r = mod->AddScriptSection("test", script2, strlen(script2));
	r = mod->Build();
	if (r < 0)
		TEST_FAILED;
	r = ExecuteString(engine, "test()", mod);
	if (r != asEXECUTION_FINISHED)
		TEST_FAILED;

	const char* script3 =
		"int choice = 0; \n"
		"void func(int, float, double) { choice = 1; } \n"
		"void func(float, int, double) { choice = 2; } \n"
		"void func(double, float, int) { choice = 3; } \n"
		"void main() \n"
		"{ \n"
		"  func(1, 1.0f, 1.0); assert( choice == 1 ); \n" // perfect match
		"  func(1.0f, 1, 1.0); assert( choice == 2 ); \n" // perfect match
		"  func(1.0, 1.0f, 1); assert( choice == 3 ); \n" // perfect match
		"  func(1.0, 1, 1); assert( choice == 3 ); \n" // second arg converted
		"  func(1.0f, 1.0, 1.0); assert( choice == 2 ); \n" // second arg converted
		"  func(1.0f, 1.0f, 1); assert( choice == 3 ); \n" // first arg converted
		"} \n";
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	r = mod->AddScriptSection("test", script3);
	r = mod->Build();
	if (r < 0)
		TEST_FAILED;
	r = ExecuteString(engine, "main()", mod);
	if (r != asEXECUTION_FINISHED)
		TEST_FAILED;

	const char* script4 =
		"class A { \n"
		"  void func(int) { choice = 1; } \n"
		"  void func(int) const { choice = 2; } \n"
		"} \n"
		"int choice; \n"
		"void main() \n"
		"{ \n"
		"  A@ a = A(); \n"
		"  const A@ b = A(); \n"
		"  a.func(1); assert( choice == 1 ); \n"
		"  b.func(1); assert( choice == 2 ); \n"
		"} \n";
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	r = mod->AddScriptSection("test", script4);
	r = mod->Build();
	if (r < 0)
		TEST_FAILED;
	r = ExecuteString(engine, "main()", mod);
	if (r != asEXECUTION_FINISHED)
		TEST_FAILED;

	const char* script5 =
		"void func(int8, double a = 1.0) { choice = 1; } \n"
		"void func(float) { choice = 2; } \n"
		"int choice; \n"
		"void main() \n"
		"{ \n"
		"  func(1); assert( choice == 1 ); \n"
		"} \n";
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	r = mod->AddScriptSection("test", script5);
	r = mod->Build();
	if (r < 0)
		TEST_FAILED;
	r = ExecuteString(engine, "main()", mod);
	if (r != asEXECUTION_FINISHED)
		TEST_FAILED;

	engine->Release();

	return fail;
}

} // end namespace
