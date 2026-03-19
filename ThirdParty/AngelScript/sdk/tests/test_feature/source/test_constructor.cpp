#include "utils.h"
#include "scriptmath3d.h"

using namespace std;

namespace TestConstructor
{

static const char * const TESTNAME = "TestConstructor";

static const char *script1 =
"obj g_obj1 = g_obj2;                      \n"
"obj g_obj2();                             \n"
"obj g_obj3(12, 3);                        \n";

static const char *script2 = 
"void TestConstructor()                    \n"
"{                                         \n"
"  obj l_obj1;                             \n"
"  l_obj1.a = 5; l_obj1.b = 7;             \n"
"  obj l_obj2();                           \n"
"  obj l_obj3(3, 4);                       \n"
"  a = l_obj1.a + l_obj2.a + l_obj3.a;     \n"
"  b = l_obj1.b + l_obj2.b + l_obj3.b;     \n"
"}                                         \n";
/*
// Illegal situations
static const char *script3 = 
"obj *g_obj4();                            \n";
*/
// Using constructor to create temporary object
static const char *script4 = 
"void TestConstructor2()                   \n"
"{                                         \n"
"  a = obj(11, 2).a;                       \n"
"  b = obj(23, 13).b;                      \n"
"}                                         \n";

class CTestConstructor
{
public:
	CTestConstructor() {a = 0; b = 0;}
	CTestConstructor(int a, int b) {this->a = a; this->b = b;}

	int a;
	int b;
};

void ConstrObj(CTestConstructor *obj)
{
	new(obj) CTestConstructor();
}

void ConstrObj(int a, int b, CTestConstructor *obj)
{
	new(obj) CTestConstructor(a,b);
}

void ConstrObj_gen1(asIScriptGeneric *gen)
{
	CTestConstructor *obj = (CTestConstructor*)gen->GetObject();
	new(obj) CTestConstructor();
}

void ConstrObj_gen2(asIScriptGeneric *gen)
{
	CTestConstructor *obj = (CTestConstructor*)gen->GetObject();
	int a = gen->GetArgDWord(0);
	int b = gen->GetArgDWord(1);
	new(obj) CTestConstructor(a,b);
}


bool Test()
{
	bool fail = false;
	CBufferedOutStream bout;
	asIScriptEngine* engine;
	asIScriptModule* mod;
	int r;

	// Test accessing parent's properties before calling super
	// Reported by Sam Tupy
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		RegisterStdString(engine);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"class ItemBase { \n"
			"     string name; \n"
			"     ItemBase(string name) {} \n"
			"} \n"
			"class Item: ItemBase { \n"
			"     int fallingSpeed; \n"
			"     Item(string Name, int fallingSpeed) { \n"
			"         super(name); \n" // Must give error, as the property name is accessed before it is initialized by parent class
			"         this.fallingSpeed = fallingSpeed; \n"
			"     } \n"
			"} \n"
			"void main() { \n"
			"     Item itm('Cardboard box', 400); \n"
			"} \n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		engine->ShutDownAndRelease();

		if (bout.buffer != "test (7, 6) : Info    : Compiling Item::Item(string, int)\n"
						   "test (8, 16) : Error   : The member 'name' is accessed before the initialization\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test copy constructor marked as explicit cannot be implicitly called
	// Reported by Patrick Jeeves
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Foo {\n"
			"	Foo() {}\n"
			"	Foo(const Foo &in x) explicit {}\n" // explicit copy constructor
			"   Foo &opAssign(const Foo &in x) delete;\n" // remove the opAssign method
			"}\n"
			"void Bar(Foo &in y) {}\n"
			"void main() {\n"
			"  Foo p;\n"
			"  Bar(p);\n"         // NOK. Copy constructor is implicitly called
			"  Bar(Foo(p));\n"    // OK. Copy constructor is explicitly called
			"  Foo q = p;\n"      // NOK. Copy constructor is implicitly called
			"  Foo s(p);\n"       // OK. Copy constructor is explicitly called
			"}\n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != 
			"TestConstructor (7, 1) : Info    : Compiling void main()\n"
			"TestConstructor (9, 7) : Error   : Can't implicitly call explicit copy constructor\n"
			"TestConstructor (11, 7) : Error   : No appropriate opAssign method found in 'Foo' for value assignment\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test auto generated copy constructor with registered pod value type
	// https://www.gamedev.net/forums/topic/715625-copy-constructors-appear-to-be-broken/5461794/
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, false);

		RegisterScriptMath3D(engine);

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Foo {\n"
			"	Foo() {}\n"
			"	//Foo(int x) {}\n" // Uncomment this to fix the problem
			"	vector3 v = vector3(0,0,0);\n"
			"}\n"
			"Foo LerpTest() {\n"
			"	Foo p;\n"
			"	p.v = vector3(1, 2, 3);\n"
			"	return p;\n"
			"}\n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "assert(LerpTest().v.x == 1 && LerpTest().v.y == 2 && LerpTest().v.z == 3);", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test auto generated copy constructor
	// Reported by Patrick Jeeves
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, false);

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class bar\n"
			"{ \n"
			"  int inherited = 104; \n"
			"} \n"
			"class foo : bar\n"
			"{\n"
			"	foo@ thisWorks() { foo f = this; return f; }\n"
			"   foo@ soDoesThis() { foo f(); f = this; return f; }\n"
			"	foo@ asDoesThis() { return foo(this); }\n"
			"   int value = 42; \n"
			"}\n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "foo f; f.value = 13; f.inherited = 31; foo @g = f.thisWorks(); assert( g.value == 13 ); assert( g.inherited == 31 );", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "foo f; f.value = 13; f.inherited = 31; foo @g = f.soDoesThis(); assert( g.value == 13 ); assert( g.inherited == 31 );", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "foo f; f.value = 13; f.inherited = 31; foo @g = f.asDoesThis(); assert( g.value == 13 ); assert( g.inherited == 31 );", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test that accessing a member before it has been initialized will make it initialize in beginning as default (else it would cause null pointer exception at runtime)
	// It's not an error to access the member, as long as there is no explicit init after it. Should set a flag for the property that it has been accessed before init and give error when initializing it.
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Bar\n"
			"{\n"
			"  Bar() delete; \n" // no default constructor
			"  Bar(int a) { value = a; } \n"
			"  int value; \n"
			"}\n"
			"class Foo\n"
			"{\n"
			"  Foo() {\n"
			"    if( a.value == 0 )\n" // this access is not invalid by itself, as the member could be initialized by the member initialization at the declaration
			"      a = Bar(0);\n"      // however, by doing an explicit initialization afterwards it is a clear error
			"    else\n"
			"      a = Bar(1);\n"
			"  } \n"
			"  Foo(int v) { a.value = 1; }" // The access to the member is OK, it will be auto initialized based on the member declaration
			"  Foo(float v) {\n"
			"    if( v == 0 )\n"
			"    {"
			"      a = Bar(0);\n"
			"      a.value = 1;\n" // this access doesn't invalidate the initialization in the else condition
			"    }\n"
			"    else\n"
			"      a = Bar(1);\n"
			"  } \n"
			"  Bar a = Bar(1);\n"
			"}\n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer !=
			"TestConstructor (9, 3) : Info    : Compiling Foo::Foo()\n"
			"TestConstructor (11, 9) : Error   : The member 'a' is accessed before the initialization\n"
			"TestConstructor (13, 9) : Error   : The member 'a' is accessed before the initialization\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test that the compiler catch errors of members that cannot be initialized with default constructor
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Bar\n"
			"{\n"
			"  Bar() delete; \n" // no default constructor
			"  Bar(int a) { value = a; } \n"
			"  int value; \n"
			"}\n"
			"class Foo\n"
			"{\n"
			"  Foo(int v) {} \n" // Bar doesn't have a default constructor so 'a' must be explicitly initialized
			"  Bar a;\n"
			"}\n"
			"class Foo2 { Bar a; }\n"); // Compiler will attempt to generate default constructor for Foo2, since no other constructor is declared. But will fail
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer !=
			"TestConstructor (9, 3) : Info    : Compiling Foo::Foo(int)\n"
			"TestConstructor (10, 7) : Error   : No default constructor for object of type 'Bar'.\n"
			"TestConstructor (12, 7) : Info    : Compiling auto generated Foo2::Foo2()\n"
			"TestConstructor (12, 18) : Error   : No default constructor for object of type 'Bar'.\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test that the compiler can catch the error of attempting to initialize some members after a condition return (i.e. not all code paths initialize the same set of members)
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Bar\n"
			"{\n"
			"  Bar(int a) { value = a; } \n"
			"  int value; \n"
			"}\n"
			"class Foo\n"
			"{\n"
			"  Foo(int v) { \n"
			"    a = Bar(0); \n"
			"    if( v == 0 ) return; \n"
			"    b = Bar(0); \n" // Must give error, the code potentially will not be reached leading to the member not being initialized
			"  } \n"
			"  Foo(float v) { \n"
			"    b = Bar(0); \n"
			"    if( v == 0 ) { a = Bar(0); return; } \n"
			"    else { a = Bar(1); return; } \n" // this must be allowed as in both cases the member a will be initialized
			"  } \n"
			"  Bar a, b;\n"
			"}\n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer !=
			"TestConstructor (8, 3) : Info    : Compiling Foo::Foo(int)\n"
			"TestConstructor (11, 7) : Error   : Initialization after return. All code paths must initialize the members\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test that initializations of members isn't done in loops and switch cases
	// TODO: Initialization in switch can potentially be done if all code paths do the initialization and there are no fallthroughs that potentially initialize it twice
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Bar\n"
			"{\n"
			"  Bar(int a) { value = a; } \n"
			"  int value; \n"
			"}\n"
			"class Foo\n"
			"{\n"
			"  Foo(int v) { \n"
			"    switch( v) { case 1: a = Bar(1); } \n" // Must give error that member cannot be initialized in switch case
			"    while( v == 0 ) { b = Bar(1); } \n"    // Must give error that member cannot be initialized in loop
			"  } \n" 
			"  Bar a, b;\n"
			"}\n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != 
			"TestConstructor (8, 3) : Info    : Compiling Foo::Foo(int)\n"
			"TestConstructor (9, 28) : Error   : Can't initialize the members in switch\n"
			"TestConstructor (10, 25) : Error   : Can't initialize the members in loops\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		bout.buffer = "";
		mod->AddScriptSection(TESTNAME,
			"class Bar\n"
			"{\n"
			"  Bar(int a) { value = a; } \n"
			"  Bar &opAssign(const Bar &in) delete; \n" // no assignment
			"  int value; \n"
			"}\n"
			"class Foo\n"
			"{\n"
			"  Foo(int v) { \n"
			"    a = Bar(0); \n" // The initialization is happening here
			"    b = Bar(0); \n" 
			"    switch( v) { case 1: a = Bar(1); } \n" // Will attempt a normal assignment
			"    while( v == 0 ) { b = Bar(1); } \n"
			"  } \n"
			"  Bar a, b;\n"
			"}\n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != 
			"TestConstructor (9, 3) : Info    : Compiling Foo::Foo(int)\n"
			"TestConstructor (12, 28) : Error   : No appropriate opAssign method found in 'Bar' for value assignment\n"
			"TestConstructor (13, 25) : Error   : No appropriate opAssign method found in 'Bar' for value assignment\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test that member initializations can be done in if statement, as long as both conditions initialize the same members
	// https://www.gamedev.net/forums/topic/717528-constructors-behaving-strangely
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Bar\n"
			"{\n"
			"  Bar(int a) { value = a; } \n"
			"  int value; \n"
			"}\n"
			"class Foo\n"
			"{\n"
			"  Foo(int a) { if( a == 1 ) b = Bar(10); else b = Bar(15); }\n" // the b member is initialized different values depending on the if condition
			"  Bar b = Bar(1);\n" // By default initialize with value 1
			"}\n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "Foo f(1), g(2); assert( f.b.value == 10 ); \n assert( g.b.value == 15 ); \n", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		mod->AddScriptSection(TESTNAME,
			"class Bar\n"
			"{\n"
			"  Bar(int a) { value = a; } \n"
			"  int value; \n"
			"}\n"
			"class Foo2\n"
			"{\n"
			"  Foo2(int a) {\n"
			"    if( a == 1 ) b = Bar(10);\n" // doesn't work, the member b is not initialized in the else case
			"    if( a == 2 ) {} else { c = Bar(1); }\n" // doesn't work, the member b is not initialized in the else case
			"  }\n"
			"  Bar b = Bar(1);\n"
			"  Bar c;\n"
			"}\n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "TestConstructor (8, 3) : Info    : Compiling Foo2::Foo2(int)\n"
						   "TestConstructor (9, 5) : Error   : Both conditions must initialize member 'b'\n"
						   "TestConstructor (10, 5) : Error   : Both conditions must initialize member 'c'\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test that it is possible initialize a member that doesn't have a default constructor
	// Can be done in the member declaration
	// Can be overridden in the body of the constructor
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Bar\n" 
			"{\n"
			"  Bar() delete; \n"  // cannot initialize with default constructor
			"  Bar(const Bar &in) delete; \n" // cannot copy
			"  Bar &opAssign(const Bar &in) delete; \n" // cannot assign
			"  Bar(int a) { value = a; } \n"
			"  int value; \n"
			"}\n"
			"class Foo\n"
			"{\n"
			"  Foo() { assert( b.value == 1 ); } \n" // Use the default
			"  Foo(int a) { b = Bar(a); assert( b.value == a ); }\n" // Override the defaut initialization
			"  Bar b = Bar(1);\n" // By default initialize with value 1
			"}\n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "Foo f; assert( f.b.value == 1 );\n", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		r = ExecuteString(engine, "Foo f(2); assert( f.b.value == 2 );\n", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test attempt to initialize member with direct call to constructor
	// Reported by Patrick Jeeves
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Object \n"
			"{ \n"
			"	Object(float a, float b) { _a = a; _b = b; } \n"
			"	Object() delete; \n"
			"	Object(Object& in) delete; \n"
			"	Object& opAssign(Object& in) delete; \n"
			"   float _a, _b;"
			"} \n"
			"class Foo \n"
			"{ \n"
			"	Object me(a: 1, b: 2); \n"
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "Foo f; assert( f.me._a == 1 && f.me._b == 2 );\n", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test initialization of member of type that has copy constructor but not assignment operator
	// Reported by Patrick Jeeves
	{
		engine = asCreateScriptEngine();

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME,
			"class Obj1\n" // Copy constructible, but not assignable
			"{\n"
			"	Obj1(const Obj1 &in) { }\n"
			"}\n"
			"class Obj2\n" // Constructible, Copy constructible, but not assignable
			"{\n"
			"   Obj2() {} \n"
			"	Obj2(const Obj2 &in) { }\n"
			"}\n"
			"class Obj3\n" // Constructible, not Copy constructible, Assignable
			"{\n"
			"}\n"
			"class Obj4 \n"
			"{ \n"
			"  Obj4(const Obj4 &in i) \n"
			"  { \n"
			"    o1 = i.o1; \n"
			"    o2 = i.o2; \n"
			"    o3 = i.o3; \n"
			"  } \n"
			"  Obj1 o1; \n"
			"  Obj2 o2; \n"
			"  Obj3 o3; \n"
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

	// Test object with registered constructors
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		RegisterScriptString_Generic(engine);

		r = engine->RegisterObjectType("obj", sizeof(CTestConstructor), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C); assert(r >= 0);
		if (strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY"))
		{
			r = engine->RegisterObjectBehaviour("obj", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstrObj_gen1), asCALL_GENERIC); assert(r >= 0);
			r = engine->RegisterObjectBehaviour("obj", asBEHAVE_CONSTRUCT, "void f(int,int)", asFUNCTION(ConstrObj_gen2), asCALL_GENERIC); assert(r >= 0);
		}
		else
		{
			r = engine->RegisterObjectBehaviour("obj", asBEHAVE_CONSTRUCT, "void f()", asFUNCTIONPR(ConstrObj, (CTestConstructor*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
			r = engine->RegisterObjectBehaviour("obj", asBEHAVE_CONSTRUCT, "void f(int,int)", asFUNCTIONPR(ConstrObj, (int, int, CTestConstructor*), void), asCALL_CDECL_OBJLAST); assert(r >= 0);
		}

		r = engine->RegisterObjectProperty("obj", "int a", asOFFSET(CTestConstructor, a)); assert(r >= 0);
		r = engine->RegisterObjectProperty("obj", "int b", asOFFSET(CTestConstructor, b)); assert(r >= 0);

		int a, b;
		r = engine->RegisterGlobalProperty("int a", &a); assert(r >= 0);
		r = engine->RegisterGlobalProperty("int b", &b); assert(r >= 0);

		bout.buffer = "";
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection(TESTNAME, script1, strlen(script1), 0);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		mod->Build();

		if (bout.buffer != "")
			TEST_FAILED;

		mod->AddScriptSection(TESTNAME, script2, strlen(script2));
		mod->Build();

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		ExecuteString(engine, "TestConstructor()", mod);

		if (a != 8 || b != 11)
			TEST_FAILED;

		
		//	mod->AddScriptSection(0, TESTNAME, script3, strlen(script3));
		//	mod->Build(0);

		//	if( out.buffer != "TestConstructor (1, 12) : Info    : Compiling obj* g_obj4\n"
		//					  "TestConstructor (1, 12) : Error   : Only objects have constructors\n" )
		//		TEST_FAILED;
		
		bout.buffer = "";
		mod->AddScriptSection(TESTNAME, script4, strlen(script4));
		mod->Build();

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		ExecuteString(engine, "TestConstructor2()", mod);

		if (a != 11 || b != 13)
			TEST_FAILED;

		engine->Release();
	}

	// Test that constructor allocates memory for member objects and can properly initialize it
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		
		RegisterScriptMath3D(engine);

		const char *script = 
			"class Obj \n"
			"{  \n"
			"   Obj(const vector3 &in v) \n"
			"   { \n"
			"     pos = v; \n"
			"   } \n"
			"   vector3 pos; \n"
			"} \n";

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		int typeId = mod->GetTypeIdByDecl("Obj");
		asITypeInfo *type = engine->GetTypeInfoById(typeId);
		asIScriptFunction *func = type->GetFactoryByDecl("Obj @Obj(const vector3 &in)");
		if( func == 0 )
			TEST_FAILED;

		asIScriptContext *ctx = engine->CreateContext();
		ctx->Prepare(func);
		Vector3 pos(1,2,3);
		*(Vector3**)ctx->GetAddressOfArg(0) = &pos;
		r = ctx->Execute();
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		asIScriptObject *obj = *(asIScriptObject**)ctx->GetAddressOfReturnValue();
		Vector3 pos2 = *(Vector3*)obj->GetAddressOfProperty(0);
		if( pos2 != pos )
			TEST_FAILED;

		ctx->Release();

		engine->Release();
	}

	// Success
	return fail;
}

}