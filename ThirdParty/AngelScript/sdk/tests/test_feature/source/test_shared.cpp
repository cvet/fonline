#include "utils.h"

namespace TestShared
{

bool Test()
{
	bool fail = false;
	CBufferedOutStream bout;
	asIScriptEngine *engine;
	int r;

	// external shared entities should be saved specifically as external in bytecode to avoid increase in file size
	// TODO: as_restore should only populate externalTypes and externalFunctions if not def AS_NO_COMPILER
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod1 = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod1->AddScriptSection("test",
			"shared funcdef void A(); \n"
			"shared void B() {} \n"
			"shared interface C { void foo(); } \n"
			"shared enum D { a, b, c } \n"
			"shared class E : C { void foo() {} } \n");
		r = mod1->Build();
		if (r < 0)
			TEST_FAILED;

		asIScriptModule *mod2 = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		mod2->AddScriptSection("test2",
			"external shared funcdef void A(); \n"
			"external shared void B(); \n"
			"external shared interface C; \n"
			"external shared enum D; \n"
			"external shared class E; \n");
		r = mod2->Build();
		if (r < 0)
			TEST_FAILED;

		CBytecodeStream bc1(__FILE__"1");
		CBytecodeStream bc2(__FILE__"2");
		r = mod1->SaveByteCode(&bc1); assert(r >= 0);
		r = mod2->SaveByteCode(&bc2); assert(r >= 0);

		if (bc1.buffer.size() <= bc2.buffer.size())
			TEST_FAILED;

		asDWORD crc32 = ComputeCRC32(&bc1.buffer[0], asUINT(bc1.buffer.size()));
		if (crc32 != 0x283B95BD)
		{
			PRINTF("The saved byte code has different checksum than the expected. Got 0x%X\n", crc32);
			TEST_FAILED;
		}

		crc32 = ComputeCRC32(&bc2.buffer[0], asUINT(bc2.buffer.size()));
		if (crc32 != 0xDCC2E57D)
		{
			PRINTF("The saved byte code has different checksum than the expected. Got 0x%X\n", crc32);
			TEST_FAILED;
		}

		asIScriptModule *mod3 = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		r = mod3->LoadByteCode(&bc2);
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test external shared funcdef
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"funcdef void A(); \n" // ok
			"shared funcdef void B(); \n" // ok
			"external shared funcdef void C(); \n" // builder should report error due to not finding C
			"shared external funcdef void D(); \n" // builder should report error due to not finding D
			);
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (3, 1) : Error   : External shared entity 'C' not found\n"
						   "test (4, 1) : Error   : External shared entity 'D' not found\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		bout.buffer = "";
		mod->AddScriptSection("test",
			"shared funcdef void TEST();");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		mod = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"external shared funcdef void TEST(); \n");
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

	// Test external shared function
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void A(); \n" // looks like a variable declaration
			"shared void B(); \n" // missing definition
			"external shared void C(); \n" // builder should report error due to not finding C
			"shared external void D() {  }; \n" // builder should report error due to not finding C. Cannot redefine original 
			);
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (1, 1) : Error   : Data type can't be 'void'\n"
						   "test (2, 1) : Error   : Missing definition of 'B'\n"
						   "test (3, 1) : Error   : External shared entity 'C' not found\n"
						   "test (4, 1) : Error   : External shared entity 'D' not found\n"
						   "test (4, 1) : Error   : External shared entity 'D' cannot redefine the original entity\n"
						// TODO: Shouldn't try to compile the variable since the declaration had errors
						   "test (1, 6) : Info    : Compiling void A\n"
						   "test (1, 7) : Error   : Only objects have constructors\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		bout.buffer = "";
		mod->AddScriptSection("test",
			"shared void TEST() {  }");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		mod = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"external shared void TEST(); \n");
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

	// Test external shared class
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"class A; \n" // missing definition
			"shared class B; \n" // missing definition
			"external shared class C; \n" // builder should report error due to not finding C
			"shared external class D { void func() {} }; \n" // builder should report error due to not finding C. Cannot redefine original 
			);
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (1, 7) : Error   : Missing definition of 'A'\n"
			"test (2, 14) : Error   : Missing definition of 'B'\n"
			"test (3, 23) : Error   : External shared entity 'C' not found\n"
			"test (4, 23) : Error   : External shared entity 'D' cannot redefine the original entity\n"
			"test (4, 23) : Error   : External shared entity 'D' not found\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		bout.buffer = "";
		mod->AddScriptSection("test",
			"shared class TEST { void func() {} }");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		mod = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"external shared class TEST; \n");
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

	// Test external shared interface
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"interface A; \n" // missing definition
			"shared interface B; \n" // missing definition
			"external shared interface C; \n" // builder should report error due to not finding C
			"shared external interface D { int func(); }; \n" // builder should report error due to not finding C. Cannot redefine original 
			);
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (1, 11) : Error   : Missing definition of 'A'\n"
						   "test (2, 18) : Error   : Missing definition of 'B'\n"
						   "test (3, 27) : Error   : External shared entity 'C' not found\n"
						   "test (4, 27) : Error   : External shared entity 'D' cannot redefine the original entity\n"
						   "test (4, 27) : Error   : External shared entity 'D' not found\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		bout.buffer = "";
		mod->AddScriptSection("test",
			"shared interface TEST { int func(); }");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		mod = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"external shared interface TEST; \n");
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

	// Test external shared enums
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"enum TEST1; \n" // missing definition
			"shared enum TEST2; \n" // missing definition
			"external shared enum TEST3; \n" // builder should report error due to not finding TEST3
			"shared external enum TEST4 { val }; \n" // builder should report error due to not finding TEST4. Cannot redefine original 
			);
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (1, 6) : Error   : Missing definition of 'TEST1'\n"
						   "test (2, 13) : Error   : Missing definition of 'TEST2'\n"
						   "test (3, 22) : Error   : External shared entity 'TEST3' not found\n"
						   "test (4, 22) : Error   : External shared entity 'TEST4' not found\n"
						   "test (4, 22) : Error   : External shared entity 'TEST4' cannot redefine the original entity\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		bout.buffer = "";
		mod->AddScriptSection("test",
			"shared enum TEST { val = 1 }");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		mod = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"external shared enum TEST; \n");
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

	// Test declaring a shared funcdef that use a non-shared type
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod1 = engine->GetModule("test1", asGM_ALWAYS_CREATE);
		mod1->AddScriptSection("test",
			"class A {} \n"
			"shared funcdef void F(A@); \n");
		r = mod1->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (2, 1) : Error   : Shared code cannot use non-shared type 'A'\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test using a registered funcdef in a shared function
	// https://www.gamedev.net/topic/685120-application-registered-funcdefs-arent-shared/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		engine->RegisterFuncdef("void voidFunc()");

		asIScriptModule *mod1 = engine->GetModule("test1", asGM_ALWAYS_CREATE);
		mod1->AddScriptSection("test", 
			"shared void call(voidFunc@ f) { \n"
			"  f(); \n"
			"} \n");
		r = mod1->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test memory management with shared functions calling shared functions
	// http://www.gamedev.net/topic/638334-assertion-failed-on-exit-with-shared-func/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		const char *script = 
			"shared void func1() { func2(); }\n"
			"shared void func2() { func1(); }\n";

		const char *script2 =
			"shared void func3() { func1(); } \n";

		asIScriptModule *mod1 = engine->GetModule("test1", asGM_ALWAYS_CREATE);
		mod1->AddScriptSection("test", script);
		r = mod1->Build();
		if( r < 0 )
			TEST_FAILED;

		asIScriptFunction *func = mod1->GetFunctionByName("func1");

		asIScriptModule *mod2 = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		mod2->AddScriptSection("test", script);
		mod2->AddScriptSection("test2", script2);
		r = mod2->Build();
		if( r < 0 )
			TEST_FAILED;

		asIScriptFunction *func2 = mod2->GetFunctionByName("func1");
		if( func != func2 )
			TEST_FAILED;

		engine->DiscardModule("test1");

		engine->Release();
	}

	// Test funcdefs in shared interfaces
	// http://www.gamedev.net/topic/639243-funcdef-inside-shared-interface-interface-already-implement-warning/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("A", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("A", 
			"funcdef void Func();\n"
			"shared interface ielement\n"
			"{\n"
			"    Func@ f { get; set; }\n"
			"}\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		mod = engine->GetModule("B", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("B", 
			"funcdef void Func();\n"
			"shared interface ielement\n"
			"{\n"
			"    Func@ f { get; set; }\n"
			"}\n"
			"class celement : ielement\n"
			"{\n"
			"    Func@ fdef;\n"
			"    Func@ get_f()\n"
			"    {\n"
			"       return( this.fdef ); \n"
			"    }\n"
			"    void set_f( Func@  newF )\n"
			"    {\n"
			"       @this.fdef = newF;\n"
			"    }\n"
			"}\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		CBytecodeStream stream("B");
		mod->SaveByteCode(&stream);

		bout.buffer = "";
		mod = engine->GetModule("C", asGM_ALWAYS_CREATE);
		r = mod->LoadByteCode(&stream);
		if( r != 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test different types of virtual properties in shared interfaces
	// http://www.gamedev.net/topic/639243-funcdef-inside-shared-interface-interface-already-implement-warning/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("A", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("A", 
			"shared interface ielement\n"
			"{\n"
			"    float f { get; set; }\n"
			"}\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		mod = engine->GetModule("B", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("B", 
			"shared interface ielement\n"
			"{\n"
			"    int f { get; set; }\n"
			"}\n");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "B (3, 13) : Error   : Shared type 'ielement' doesn't match the original declaration in other module\n"
		                   "B (3, 18) : Error   : Shared type 'ielement' doesn't match the original declaration in other module\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test problem reported by Andrew Ackermann
	{
		const char *script =
			"shared interface I {} \n"
			"shared class C : I {} \n";

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("A", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("A", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		mod = engine->GetModule("B", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("A", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// Make sure the shared type didn't get the interface duplicated
		asITypeInfo *type = mod->GetTypeInfoByName("C");
		if( type->GetInterfaceCount() != 1 )
			TEST_FAILED;

		engine->Release();
	}

	// Compiling a script with a shared class that refers to other non declared entities must give
	// error even if the shared class is already existing in a previous module
	// http://www.gamedev.net/topic/632922-huge-problems-with-precompilde-byte-code/
	{
		const char *script1 = 
			"shared class A { \n"
			"  B @b; \n"
			"  void setB(B@) {} \n"
			"  void func() {B@ l;} \n" // TODO: The method isn't compiled so this error isn't seen. Should it be?
			"  string c; \n"
			"} \n";

		const char *script2 =
			"shared class B {} \n";

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		RegisterStdString(engine);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("A", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("A", script1);
		mod->AddScriptSection("B", script2);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		mod = engine->GetModule("B", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("A", script1);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "A (3, 13) : Error   : Identifier 'B' is not a data type in global namespace\n"
		                   "A (3, 3) : Error   : Shared type 'A' doesn't match the original declaration in other module\n"
		                   "A (2, 3) : Error   : Identifier 'B' is not a data type in global namespace\n"
		                   "A (2, 6) : Error   : Shared type 'A' doesn't match the original declaration in other module\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	{
		int reg;
 		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		engine->RegisterGlobalProperty("int reg", &reg);

		RegisterScriptArray(engine, true);

		engine->RegisterEnum("ESHARED");
		engine->RegisterEnumValue("ESHARED", "ES1", 1);

		asIScriptModule *mod = engine->GetModule("", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("a", 
			"interface badIntf {} \n"
			"shared interface sintf {} \n"
			"shared class T : sintf, badIntf \n" // Allow shared interface, but not non-shared interface
			"{ \n"
			"  void test() \n"
			"  { \n"
			"    var = 0; \n" // Don't allow accessing non-shared global variables
			"    gfunc(); \n" // Don't allow calling non-shared global functions
			"    reg = 1; \n" // Allow accessing registered variables
			"    assert( reg == 1 ); \n" // Allow calling registered functions
			"    badIntf @intf; \n" // Do not allow use of non-shared types in parameters/return type
			"    int cnst = g_cnst; \n" // Allow using literal consts, even if they are declared in global scope
			"    ESHARED es = ES1; \n" // Allow
			"    ENOTSHARED ens = ENS1; \n" // Do not allow. The actual value ENS1 is allowed though, as it is a literal constant
			"    cast<badIntf>(null); \n" // do not allow casting to non-shared types
			"    assert !is null; \n" // Allow taking address of registered functions
			"    gfunc !is null; \n" // Do not allow taking address of non-shared script functions
			"    nonShared(); \n" // Do not allow constructing objects of non-shared type
			"    impfunc(); \n" // Do not allow calling imported function
			"    sfunc(); \n" // Allow calling a shared global function
			"  } \n"
			"  T @dup() const \n" // It must be possible for the shared class to use its own type
			"  { \n"
			"    T d; \n" // Calling the global factory as a shared function
			"    return d; \n" 
			"  } \n"
			"  T() {} \n"
			"  T(int a) \n"
			"  { \n"
			"     var = a; \n" // Constructor of shared class must not access non-shared code
			"  } \n"
			"  void f(badIntf @) {} \n" // Don't allow use of non-shared types in parameters/return type
			"  ESHARED _es; \n" // allow
			"  ESHARED2 _es2; \n" // allow
			"  ENOTSHARED _ens; \n" // Don't allow
			"  void f() \n"
			"  { \n"
			"    array<int> a; \n"
			"  } \n"
			"} \n"
			"int var; \n"
			"void gfunc() {} \n"
			"shared void sfunc() \n"
			"{ \n"
			"  gfunc(); \n" // don't allow
			"  T t; \n" // allow
			"  ESHARED2 s; \n" // allow
			"} \n"
			"enum ENOTSHARED { ENS1 = 1 } \n"
			"shared enum ESHARED2 { ES21 = 0 } \n"
			"const int g_cnst = 42; \n"
			"class nonShared {} \n"
			"import void impfunc() from 'mod'; \n"
			);
		bout.buffer = "";
		r = mod->Build();
		if( r >= 0 ) 
			TEST_FAILED;
		if( bout.buffer != "a (3, 25) : Error   : Shared type cannot implement non-shared interface 'badIntf'\n"
						   "a (32, 3) : Error   : Shared code cannot use non-shared type 'badIntf'\n"
						   "a (35, 3) : Error   : Shared code cannot use non-shared type 'ENOTSHARED'\n"
						   "a (5, 3) : Info    : Compiling void T::test()\n"
						   "a (7, 5) : Error   : Shared code cannot access non-shared global variable 'var'\n"
						   "a (8, 5) : Error   : Shared code cannot call non-shared function 'void gfunc()'\n"
						   "a (11, 5) : Error   : Shared code cannot use non-shared type 'badIntf'\n"
						   "a (14, 5) : Error   : Shared code cannot use non-shared type 'ENOTSHARED'\n"
						   "a (15, 5) : Error   : Shared code cannot use non-shared type 'badIntf'\n"
						   "a (17, 11) : Error   : Shared code cannot call non-shared function 'void gfunc()'\n"
						   "a (18, 5) : Error   : Shared code cannot use non-shared type 'nonShared'\n"
						   "a (19, 5) : Error   : Shared code cannot call non-shared function 'void impfunc()'\n"
						   "a (28, 3) : Info    : Compiling T::T(int)\n"
						   "a (30, 6) : Error   : Shared code cannot access non-shared global variable 'var'\n"
						   "a (43, 1) : Info    : Compiling void sfunc()\n"
						   "a (45, 3) : Error   : Shared code cannot call non-shared function 'void gfunc()'\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		engine->DiscardModule("");
	
		const char *validCode =
			"shared interface I {} \n"
			"shared class T : I \n"
			"{ \n"
			"  void func() {} \n"
			"  int i; \n"
			"} \n"
			"shared void func() {} \n"
			"shared enum eshare { e1, e2 } \n";
		mod = engine->GetModule("1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("a", validCode);
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		int t1 = mod->GetTypeIdByDecl("T");
		if( t1 < 0 )
			TEST_FAILED;

		asIScriptFunction *f1 = mod->GetFunctionByDecl("void func()");
		if( f1 == 0 )
			TEST_FAILED;

		if( t1 >= 0 )
		{
			asIScriptFunction *fact1 = engine->GetTypeInfoById(t1)->GetFactoryByIndex(0);
			if( fact1 < 0 )
				TEST_FAILED;
		
			asIScriptModule *mod2 = engine->GetModule("2", asGM_ALWAYS_CREATE);
			mod2->AddScriptSection("b", validCode);
			r = mod2->Build();
			if( r < 0 )
				TEST_FAILED;

			if( bout.buffer != "" )
			{
				PRINTF("%s", bout.buffer.c_str());
				TEST_FAILED;
			}

			int t2 = mod2->GetTypeIdByDecl("T");
			if( t1 != t2 )
				TEST_FAILED;

			asIScriptFunction *f2 = mod2->GetFunctionByDecl("void func()");
			if( f1 != f2 )
				TEST_FAILED;

			asIScriptFunction *fact2 = engine->GetTypeInfoById(t2)->GetFactoryByIndex(0);
			if( fact1 != fact2 )
				TEST_FAILED;

			CBytecodeStream stream(__FILE__"1");
			mod->SaveByteCode(&stream);

			bout.buffer = "";
			asIScriptModule *mod3 = engine->GetModule("3", asGM_ALWAYS_CREATE);
			r = mod3->LoadByteCode(&stream);
			if( r < 0 )
				TEST_FAILED;

			int t3 = mod3->GetTypeIdByDecl("T");
			if( t1 != t3 )
				TEST_FAILED;
			if( bout.buffer != "" )
			{
				PRINTF("%s", bout.buffer.c_str());
				TEST_FAILED;
			}

			asIScriptFunction *f3 = mod3->GetFunctionByDecl("void func()");
			if( f1 != f3 )
				TEST_FAILED;

			asIScriptFunction *fact3 = engine->GetTypeInfoById(t3)->GetFactoryByIndex(0);
			if( fact1 != fact3 )
				TEST_FAILED;

			bout.buffer = "";
			r = ExecuteString(engine, "T t; t.func(); func();", mod3);
			if( r != asEXECUTION_FINISHED )
				TEST_FAILED;
			if( bout.buffer != "" )
			{
				PRINTF("%s", bout.buffer.c_str());
				TEST_FAILED;
			}
		}

		engine->Release();
	}

	// Test shared classes as members of other classes
	// http://www.gamedev.net/topic/617717-shared-template-factory-stub/
	{
 		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);

		const char *code = 
			"shared class S { int a; } \n"
			"class A { S s; } \n";

		asIScriptModule *mod = engine->GetModule("1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("code", code);
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		mod = engine->GetModule("2", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("code", code);
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test reloading scripts with shared code
	// http://www.gamedev.net/topic/618417-problem-with-shared-keyword/
	{
 		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);

		const char *code = 
			"shared class B {} \n"
			"shared class iMyInterface { \n"
			"  void MyFunc(const B &in) {} \n"
			"} \n"
			"class cMyClass : iMyInterface { \n"
			"  void MyFunc(const B &in) { \n"
			"  } \n"
			"} \n";

		asIScriptModule *mod = engine->GetModule("1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("code", code);
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		int id = mod->GetTypeIdByDecl("cMyClass");
		asITypeInfo *type = engine->GetTypeInfoById(id);
		asIScriptFunction *func = type->GetMethodByDecl("void MyFunc(const B &in)");
		if( func == 0 )
			TEST_FAILED;

		asIScriptObject *obj = (asIScriptObject*)engine->CreateScriptObject(type);
		asIScriptContext *ctx = engine->CreateContext();

		ctx->Prepare(func);
		ctx->SetObject(obj);
		r = ctx->Execute();
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		ctx->Release();
		obj->Release();

		// This will orphan the shared types, but won't destroy them yet
		// as they are still kept alive by the garbage collector
		engine->DiscardModule("1");

		// Build the module again. This will re-use the orphaned 
		// shared classes, the code should still work normally
		mod = engine->GetModule("1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("code", code);
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		id = mod->GetTypeIdByDecl("cMyClass");
		type = engine->GetTypeInfoById(id);
		func = type->GetMethodByDecl("void MyFunc(const B &in)");
		if( func == 0 )
			TEST_FAILED;

		obj = (asIScriptObject*)engine->CreateScriptObject(type);
		ctx = engine->CreateContext();

		ctx->Prepare(func);
		ctx->SetObject(obj);
		r = ctx->Execute();
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		ctx->Release();
		obj->Release();

		engine->Release();
	}

	// Test problem reported by TheAtom
	// http://www.gamedev.net/topic/622841-shared-issues/
	{
 		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);

		const char *code = 
			"shared int f() \n"
			"{ \n"
			"  return 0; \n"
			"} \n";

		asIScriptModule *mod = engine->GetModule("1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("code", code);
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
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

