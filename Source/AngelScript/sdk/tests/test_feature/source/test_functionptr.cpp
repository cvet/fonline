#include "utils.h"

namespace TestFunctionPtr
{

bool receivedFuncPtrIsOK = false;

void ReceiveFuncPtr(asIScriptFunction *funcPtr)
{
	if( funcPtr == 0 ) return;

	if( strcmp(funcPtr->GetName(), "test") == 0 ) 
		receivedFuncPtrIsOK = true;

	funcPtr->Release();
}

bool Test()
{
	RET_ON_MAX_PORT

	bool fail = false;
	int r;
	COutStream out;
	asIScriptEngine *engine;
	asIScriptModule *mod;
	asIScriptContext *ctx;
	CBufferedOutStream bout;
	const char *script;

	// Test proper error when taking address of method by mistake
	// Reported by Polyak Istvan
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		bout.buffer = "";

		const char *script =
			"class C1\n"
			"{\n"
			"    C1 ()\n"
			"    {\n"
			//"        write('C1\\n');\n"
			"    }\n"
			"    int m1 ()\n"
			"    {\n"
			//"        write('m1\\n');\n"
			"        return 2;\n"
			"	}\n"
			"}\n"
			"class C2\n"
			"{\n"
			"    C2 (int )\n"
			"    {\n"
			//"        write('C2 int\\n');\n"
			"    }\n"
			"    C2 (const C1 &in c1)\n"
			"    {\n"
			//"        write('C2 C1\\n');\n"
			"    } \n"
			"} \n"
			"void main () \n"
			"{ \n"
			"    C1 c1;            \n"  // C1
			"    C2 c2_4(c1.m1);   \n"  // problem: C2 C1 was called instead of error
			"} \n";

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (20, 1) : Info    : Compiling void main()\n"
						   "test (23, 12) : Error   : No matching signatures to 'C2(C1::m1)'\n"
						   "test (23, 12) : Error   : Can't pass class method as arg directly. Use a delegate object instead\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test calling function pointer retrieved from property accessor
	// http://www.gamedev.net/topic/664944-stack-corruption-when-using-funcdef-and-class-property/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		RegisterScriptArray(engine, false);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"funcdef void Callback( array<int>@ pArray );\n"
			"class Class\n"
			"{\n"
			"	private Callback@ m_pCallback;\n"
			"	Callback@ Callback\n"
			"	{\n"
			"		get const { return m_pCallback; }\n"
			"	}\n"
			"	Class( Callback@ pCallback )\n"
			"	{\n"
			"		@m_pCallback = @pCallback;\n"
			"	}\n"
			"}\n"
			"void CallbackFn( array<int>@ pArray )\n"
			"{\n"
			"	uint uiLength = pArray.length(); \n"//Crash occurs here
			"   g_length = uiLength; \n"
			"}\n"
			"uint g_length; \n"
			"void test()\n"
			"{\n"
			"	Class instance( @CallbackFn );\n"
			"	array<int> arr = {1,2,3};\n"
			"	g_length = 0; \n"
			"	instance.Callback( @arr );\n"
			"	assert( g_length == 3 ); \n"
			"	g_length = 0; \n"
			"	Callback@ pCallback = instance.Callback; \n"
			"	pCallback( @arr ); \n"
			"	assert( g_length == 3 ); \n"
			"}\n";

		mod = engine->GetModule("TEst", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "test()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->ShutDownAndRelease();
	}

	// Test invalid use of function pointer
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		bout.buffer = "";
		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void func2() { \n"
			"  func; \n"
			"} \n"
			"void func() {} \n");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (1, 1) : Info    : Compiling void func2()\n"
						   "test (2, 3) : Error   : Invalid expression: ambiguous name\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test value comparison for function pointers
/*
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"funcdef void CALLBACK(); \n"
			"void func1() {} \n"
			"void func2() {} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "CALLBACK@ c1 = func1, c2 = func1; \n"
								  "assert( c1 == c2 ); \n", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// TODO: Test that two different function pointers give false
		// TODO: Test for delegate objects

		engine->Release();
	}
*/

	// Proper error message when trying to pass class method as function pointer directly
	// http://www.gamedev.net/topic/655390-is-there-a-bug-with-function-callbacks/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		
		bout.buffer = "";

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"funcdef void CALLBACK(int); \n"
			"class Test { \n"
			"  void Init() { \n"
			"    SetCallback(MyCallback); \n" // This should fail, since delegate is necessary
			"  } \n"
			"  void MyCallback(int) {} \n"
			"} \n"
			"void SetCallback(CALLBACK @) {} \n");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (3, 3) : Info    : Compiling void Test::Init()\n"
						   "test (4, 5) : Error   : No matching signatures to 'SetCallback(Test::MyCallback)'\n"
						   "test (4, 5) : Error   : Can't pass class method as arg directly. Use a delegate object instead\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// opEquals with funcdef
	// http://www.gamedev.net/topic/647797-difference-between-xopequalsy-and-xy-with-funcdefs/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"funcdef void CALLBACK(); \n"
			"class Test { \n"
			"  bool opEquals(CALLBACK @f) { \n"
			"    return f is func; \n"
			"  } \n"
			"  CALLBACK @func; \n"
			"} \n"
			"void func() {} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "Test t; \n"
			                      "@t.func = func; \n"
								  "assert( t == func );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"funcdef void CALLBACK(); \n"
			"class Test { \n"
			"  bool opEquals(CALLBACK @f) { \n"
			"    return f is func; \n"
			"  } \n"
			"  CALLBACK @func; \n"
			"} \n"
			"namespace ns { \n"
			"void func() {} \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "Test t; \n"
								  "@t.func = ns::func; \n"
								  "assert( t == ns::func );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test funcdefs and namespaces
	// http://www.gamedev.net/topic/644586-application-function-returning-a-funcdef-handle-crashes-when-called-in-as/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"bool called = false; \n"
			"funcdef void simpleFuncDef(); \n"
			"namespace foo { \n"
			"  void simpleFunction() { called = true; } \n"
			"} \n"
			"void takeSimpleFuncDef(simpleFuncDef@ f) { f(); } \n"
			"void main() { \n"
			"  takeSimpleFuncDef(foo::simpleFunction); \n"
			"  assert( called ); \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		mod->AddScriptSection("test",
			"bool called = false; \n"
			"funcdef void simpleFuncDef();\n"
			"namespace foo {\n"
			"  void simpleFunction() { called = true; }\n"
			"}\n"
			"void main() {\n"
			"  simpleFuncDef@ bar = foo::simpleFunction;\n"
			"  bar(); \n"
			"  assert( called ); \n"
			"}\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		engine->Release();
	}

	// Test registering global property of funcdef type
	// http://www.gamedev.net/topic/644586-application-function-returning-a-funcdef-handle-crashes-when-called-in-as/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptFunction *f = 0;
		engine->RegisterFuncdef("void myfunc()");
		r = engine->RegisterGlobalProperty("myfunc @f", &f);
		if( r < 0 )
			TEST_FAILED;

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void func() {} \n");
		mod->Build();

		r = ExecuteString(engine, "@f = func; \n", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		if( f == 0 )
			TEST_FAILED;
		if( strcmp(f->GetName(), "func") != 0 )
			TEST_FAILED;

		f->Release();
		f = 0;

		engine->Release();
	}

	// Test casting with funcdefs
	// http://www.gamedev.net/topic/644586-application-function-returning-a-funcdef-handle-crashes-when-called-in-as/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"funcdef void myfunc1(); \n"
			"funcdef void myfunc2(); \n"
			"funcdef void myfunc3(); \n"
			"bool called = false; \n"
			"void func() { called = true; } \n"
			"void main() \n"
			"{ \n"
			"  myfunc1 @f1 = func; \n"
			"  myfunc2 @f2 = cast<myfunc2>(f1); \n" // explicit cast
			"  myfunc3 @f3 = f2; \n"                // implicit cast
			"  assert( f1 is f2 ); \n"
			"  assert( f2 is f3 ); \n"
			"  assert( f3 is func ); \n"
			"  f3(); \n"
			"  assert( called ); \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Don't allow application to register additional behaviours to funcdefs
	// http://www.gamedev.net/topic/644586-application-function-returning-a-funcdef-handle-crashes-when-called-in-as/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterObjectType("jjOBJ", 0, asOBJ_REF | asOBJ_NOCOUNT);
		engine->RegisterFuncdef("void jjBEHAVIOR(jjOBJ@)");
		engine->RegisterFuncdef("void DifferentFunctionPointer()");
		r = engine->RegisterObjectMethod("jjBEHAVIOR", "DifferentFunctionPointer@ opImplCast()", asFUNCTION(0), asCALL_CDECL_OBJLAST);
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != " (0, 0) : Error   : Failed in call to function 'RegisterObjectMethod' with 'jjBEHAVIOR' and 'DifferentFunctionPointer@ opImplCast()' (Code: -5)\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test delegate function pointers for object methods
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		RegisterScriptArray(engine, false);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		RegisterStdString(engine);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"class Test { \n"
			"  void method() {} \n"
			"  int func(int) { return 0; } \n"
			"  void func() { called = true; } \n" // The compiler should pick the right overload
			"  bool called = false; \n"
			"} \n"
			"funcdef void CALLBACK(); \n"
			"void main() { \n"
			"  Test t; \n"
			"  CALLBACK @cb = CALLBACK(t.func); \n" // instanciate a delegate
			"  cb(); \n" // invoke the delegate
			"  assert( t.called ); \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Must be possible to save/load bytecode
		CBytecodeStream stream("test");
		mod->SaveByteCode(&stream);

		mod = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		r = mod->LoadByteCode(&stream);
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Must be possible to create delegate from within class method, i.e. implicit this.method
		bout.buffer = "";
		mod->AddScriptSection("test",
			"funcdef void CALL(); \n"
			"class Test { \n"
			"  bool called = false; \n"
			"  void callMe() { called = true; } \n"
			"  CALL @GetCallback() { return CALL(callMe); } \n"
			"} \n"
			"void main() { \n"
			"  Test t; \n"
			"  CALL @cb = t.GetCallback(); \n"
			"  cb(); \n"
			"  assert( t.called ); \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// A delegate to own method held as member of class must be properly resolved by gc
		bout.buffer = "";
		mod->AddScriptSection("test",
			"funcdef void CALL(); \n"
			"class Test { \n"
			"  void call() {}; \n"
			"  CALL @c; \n"
			"} \n"
			"void main() { \n"
			"  Test t; \n"
			"  @t.c = CALL(t.call); \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->GarbageCollect();

		asUINT currSize, totalDestr, totalDetect;
		engine->GetGCStatistics(&currSize, &totalDestr, &totalDetect);

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->GarbageCollect();

		asUINT currSize2, totalDestr2, totalDetect2;
		engine->GetGCStatistics(&currSize2, &totalDestr2, &totalDetect2);

		if( totalDetect2 == totalDetect )
			TEST_FAILED;

		// Must be possible to call delegate from application
		mod->AddScriptSection("test",
			"funcdef void CALL(); \n"
			"class Test { \n"
			"  bool called = false; \n"
			"  void call() { called = true; } \n"
			"} \n"
			"Test t; \n"
			"CALL @callback = CALL(t.call); \n");
		r = mod->Build();
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		int idx = mod->GetGlobalVarIndexByDecl("CALL @callback");
		if( idx < 0 )
			TEST_FAILED;

		asIScriptFunction *callback = *(asIScriptFunction**)mod->GetAddressOfGlobalVar(idx);
		if( callback == 0 )
			TEST_FAILED;
		if( callback->GetFuncType() != asFUNC_DELEGATE )
			TEST_FAILED;
		if( callback->GetDelegateObject() == 0 )
			TEST_FAILED;
		if( std::string(callback->GetDelegateFunction()->GetDeclaration()) != "void Test::call()" )
			TEST_FAILED;

		ctx = engine->CreateContext();
		ctx->Prepare(callback);
		r = ctx->Execute();
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		ctx->Release();

		r = ExecuteString(engine, "assert( t.called );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Must be possible to create the delegate from the application
		asIScriptObject *obj = (asIScriptObject*)mod->GetAddressOfGlobalVar(mod->GetGlobalVarIndexByDecl("Test t"));
		asIScriptFunction *func = obj->GetObjectType()->GetMethodByName("call");
		asIScriptFunction *delegate = engine->CreateDelegate(func, obj);
		if( delegate == 0 )
			TEST_FAILED;
		delegate->Release();

		// Must be possible to create delegate for registered type too
		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"funcdef bool EMPTY(); \n"
			"void main() { \n"
			"  array<int> a; \n"
			"  EMPTY @empty = EMPTY(a.isEmpty); \n"
			"  assert( empty() == true ); \n"
			"  a.insertLast(42); \n"
			"  assert( empty() == false ); \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Must not be possible to create delegate with const object and non-const method
		bout.buffer = "";
		mod->AddScriptSection("test",
			"funcdef void F(); \n"
			"class Test { \n"
			"  void f() {} \n"
			"} \n"
			"void main() { \n"
			" const Test @t; \n"
			" F @f = F(t.f); \n" // t is read-only, so this delegate must not be allowed
			"} \n");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		// TODO: Error message should be better, so it is understood that the error is because of const object
		if( bout.buffer != "test (5, 1) : Info    : Compiling void main()\n"
		                   "test (7, 9) : Error   : No matching signatures to 'void F()'\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// Must not be possible to create delegates for non-reference types
		bout.buffer = "";
		mod->AddScriptSection("test",
			"funcdef bool CB(); \n"
			"string s; \n"
			"CB @cb = CB(s.isEmpty); \n");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (3, 5) : Info    : Compiling CB@ cb\n"
		                   "test (3, 10) : Error   : Can't create delegate for types that do not support handles\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test ordinary function pointers for global functions
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		// Test the declaration of new function signatures
		script = "funcdef void functype();\n"
		// It must be possible to declare variables of the funcdef type
				 "functype @myFunc = null;\n"
		// It must be possible to initialize the function pointer directly
				 "functype @myFunc1 = @func;\n"
		 		 "void func() { called = true; }\n"
				 "bool called = false;\n"
		// It must be possible to compare the function pointer with another
				 "void main() { \n"
				 "  assert( myFunc1 !is null ); \n"
				 "  assert( myFunc1 is func ); \n"
		// It must be possible to call a function through the function pointer
	    		 "  myFunc1(); \n"
				 "  assert( called ); \n"
		// Local function pointer variables are also possible
				 "  functype @myFunc2 = @func;\n"
				 "} \n";
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r != 0 )
			TEST_FAILED;
		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// It must be possible to save the byte code with function handles
		CBytecodeStream bytecode(__FILE__"1");
		mod->SaveByteCode(&bytecode);
		{
			asIScriptModule *mod2 = engine->GetModule("mod2", asGM_ALWAYS_CREATE);
			mod2->LoadByteCode(&bytecode);
			r = ExecuteString(engine, "main()", mod2);
			if( r != asEXECUTION_FINISHED )
				TEST_FAILED;
		}

		// Test function pointers as members of classes. It should be possible to call the function
		// from within a class method. It should also be possible to call it from outside through the . operator.
		script = "funcdef void FUNC();       \n"
				 "class CMyObj               \n"
				 "{                          \n"
				 "  CMyObj() { @f = @func; } \n"
				 "  FUNC@ f;                 \n"
				 "  void test()              \n"
				 "  {                        \n"
				 "    this.f();              \n"
				 "    f();                   \n"
				 "    CMyObj o;              \n"
				 "    o.f();                 \n"
				 "    main();                \n"
				 "    assert( called == 4 ); \n"
				 "  }                        \n"
				 "}                          \n"
				 "void main()                \n"
				 "{                          \n"
				 "  CMyObj o;                \n"
				 "  o.f();                   \n"
				 "}                          \n"
				 "int called = 0;            \n"
				 "void func() { called++; }  \n";
		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		asIScriptContext *ctx = engine->CreateContext();
		r = ExecuteString(engine, "CMyObj o; o.test();", mod, ctx);
		if( r != asEXECUTION_FINISHED )
		{
			TEST_FAILED;
			if( r == asEXECUTION_EXCEPTION )
				PRINTF("%s", GetExceptionInfo(ctx).c_str());
		}
		ctx->Release();

		// funcdefs cannot be instantiated without being a delegate
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		script = "funcdef void functype();\n"
				 "functype myFunc;\n";
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "script (2, 10) : Info    : Compiling functype myFunc\n"
						   "script (2, 10) : Error   : No default constructor for object of type 'functype'.\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// It must not be possible to invoke the funcdef
		bout.buffer = "";
		script = "funcdef void functype();\n"
				 "void func() { functype(); } \n";
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "script (2, 1) : Info    : Compiling void func()\n"
						   "script (2, 15) : Error   : No matching signatures to 'functype()'\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// Test that a funcdef can't have the same name as other global entities
		bout.buffer = "";
		script = "funcdef void test();  \n"
				 "int test; \n";
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "script (2, 5) : Error   : Name conflict. 'test' is a funcdef.\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// It is possible to take the address of class methods, but not to assign to funcdef variable
		bout.buffer = "";
		script = 
			"funcdef void F(); \n"
			"class t { \n"
			"  void func() { \n"
			"    @func; \n"
			"    F @f = @func; \n"
			"    } \n"
			"} \n";
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		// TODO: The error message should be better
		if( bout.buffer != "script (3, 3) : Info    : Compiling void t::func()\n"
			               "script (4, 5) : Error   : Invalid expression: ambiguous name\n"
		                   "script (5, 12) : Error   : Can't implicitly convert from 't' to 'F@&'.\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// A more complex sample
		bout.buffer = "";
		script = 
			"funcdef bool CALLBACK(int, int); \n"
			"funcdef bool CALLBACK2(CALLBACK @); \n"
			"void main() \n"
			"{ \n"
			"	CALLBACK @func = @myCompare; \n"
			"	CALLBACK2 @func2 = @test; \n"
			"	func2(func); \n"
			"} \n"
			"bool test(CALLBACK @func) \n"
			"{ \n"
			"	return func(1, 2); \n"
			"} \n"
			"bool myCompare(int a, int b) \n"
			"{ \n"
			"	return a > b; \n"
			"} \n";
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// It must be possible to register the function signature from the application
		r = engine->RegisterFuncdef("void AppCallback()");
		if( r < 0 )
			TEST_FAILED;

		r = engine->RegisterGlobalFunction("void ReceiveFuncPtr(AppCallback @)", asFUNCTION(ReceiveFuncPtr), asCALL_CDECL); assert( r >= 0 );

		// It must be possible to use the registered funcdef
		// It must be possible to receive a function pointer for a registered func def
		bout.buffer = "";
		script = 
			"void main() \n"
			"{ \n"
			"	AppCallback @func = @test; \n"
			"   func(); \n"
			"   ReceiveFuncPtr(func); \n"
			"} \n"
			"void test() \n"
			"{ \n"
			"} \n";
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		if( !receivedFuncPtrIsOK )
			TEST_FAILED;

		mod->SaveByteCode(&bytecode);
		{
			receivedFuncPtrIsOK = false;
			asIScriptModule *mod2 = engine->GetModule("mod2", asGM_ALWAYS_CREATE);
			mod2->LoadByteCode(&bytecode);
			r = ExecuteString(engine, "main()", mod2);
			if( r != asEXECUTION_FINISHED )
				TEST_FAILED;

			if( !receivedFuncPtrIsOK )
				TEST_FAILED;
		}

		// The compiler should be able to determine the right function overload
		// by the destination of the function pointer
		bout.buffer = "";
		mod->AddScriptSection("test",
		         "funcdef void f(); \n"
				 "f @fp = @func;  \n"
				 "bool called = false; \n"
				 "void func() { called = true; }    \n"
				 "void func(int) {} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		r = ExecuteString(engine, "fp(); assert( called );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		//----------------------------------------------------------
		// TODO: Future improvements below

		// If the function referred to when taking a function pointer is removed from the module,
		// the code must not be invalidated. After removing func() from the module, it must still 
		// be possible to execute func2()
		script = "funcdef void FUNC(); \n"
				 "void func() {} \n";
				 "void func2() { FUNC@ f = @func; f(); } \n";

		// Test that the function in a function pointer isn't released while the function 
		// is being executed, even though the function pointer itself is cleared
		script = "DYNFUNC@ funcPtr;        \n"
				 "funcdef void DYNFUNC(); \n"
				 "@funcPtr = @CompileDynFunc('void func() { @funcPtr = null; }'); \n";

		// Test that it is possible to declare the function signatures out of order
		// This also tests the circular reference between the function signatures
		script = "funcdef void f1(f2@) \n"
				 "funcdef void f2(f1@) \n";

		// It must be possible to identify a function handle type from the type id

		// It must be possible enumerate the function definitions in the module, 
		// and to enumerate the parameters the function accepts

		// A funcdef defined in multiple modules must share the id and signature so that a function implemented 
		// in one module can be called from another module by storing the handle in the funcdef variable

		// An interface that takes a funcdef as parameter must still have its typeid shared if the funcdef can also be shared
		// If the funcdef takes an interface as parameter, it must still be shared

		// Must have a generic function pointer that can store any signature. The function pointer
		// can then be dynamically cast to the correct function signature so that the function it points
		// to can be invoked.

		engine->Release();
	}

	// Test function pointers with virtual property accessors
	// http://www.gamedev.net/topic/639243-funcdef-inside-shared-interface-interface-already-implement-warning/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"funcdef void funcdef1( ifuncdef1_1& i ); \n"
			"shared interface ifuncdef1_1 \n"
			"{ \n"
			"    ifuncdef1_2@ events { get; set; } \n"
			"    void crashme(); \n"
			"} \n"
			"shared interface ifuncdef1_2 \n"
			"{ \n"
			"    funcdef1@ f { get; set; } \n"
			"} \n"
			"class cfuncdef1_1 : ifuncdef1_1 \n"
			"{ \n"
			"    ifuncdef1_2@ _events_; \n"
			"    cfuncdef1_1() { @this._events_ = cfuncdef1_2(); } \n"
			"    ifuncdef1_2@ get_events() { return( this._events_ ); } \n"
			"    void set_events( ifuncdef1_2@ events ) { @this._events_ = events; } \n"
			"    void crashme() \n"
			"    { \n"
			"         if( this._events_ !is null && this._events_.f !is null ) \n"
			"         { \n"
			"            this.events.f( this ); \n"
//			"            this.get_events().get_f()( this ); \n" // This should produce the same bytecode as the above
			"         } \n"
			"    } \n"
			"} \n"
			"class cfuncdef1_2 : ifuncdef1_2 \n"
			"{ \n"
			"    funcdef1@ ff; \n"
			"    cfuncdef1_2() { @ff = null; } \n"
			"    funcdef1@ get_f() { return( @this.ff ); } \n"
			"    void set_f( funcdef1@ _f ) { @this.ff = _f; } \n"
			"} \n"
			"void start() \n"
			"{ \n"
			"    ifuncdef1_1@ i = cfuncdef1_1(); \n"
			"    @i.events.f = end; \n" // TODO: Shouldn't this give an error? It's attempting to do an value assignment to a function pointer
			"    i.crashme(); \n"
			"} \n"
			"bool called = false; \n"
			"void end( ifuncdef1_1& i  ) \n"
			"{ \n"
			"    called = true; \n"
			"} \n");

		r = mod->Build(); 
		if( r < 0 )
			TEST_FAILED;

		ctx = engine->CreateContext();
		r = ExecuteString(engine, "start(); assert( called );", mod, ctx);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		if( r == asEXECUTION_EXCEPTION )
			PRINTF("%s", GetExceptionInfo(ctx).c_str());
		ctx->Release();

		CBytecodeStream stream(__FILE__"1");
		
		r = mod->SaveByteCode(&stream);
		if( r < 0 )
			TEST_FAILED;
		
		engine->Release();

		// Load the bytecode 
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		stream.Restart();
		mod = engine->GetModule("A", asGM_ALWAYS_CREATE);
		r = mod->LoadByteCode(&stream);
		if( r < 0 )
			TEST_FAILED;

		ctx = engine->CreateContext();
		r = ExecuteString(engine, "start(); assert( called );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		ctx->Release();

		stream.Restart();
		mod = engine->GetModule("B", asGM_ALWAYS_CREATE);
		r = mod->LoadByteCode(&stream);
		if( r < 0 )
			TEST_FAILED;

		ctx = engine->CreateContext();
		r = ExecuteString(engine, "start(); assert( called );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		ctx->Release();

		engine->Release();
	}

	// Test clean up with registered function definitions
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		RegisterScriptString(engine);
		r = engine->RegisterFuncdef("void MSG_NOTIFY_CB(const string& strCommand, const string& strTarget)"); assert(r>=0);

		engine->Release();
	}

	// Test registering function pointer as property
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		r = engine->RegisterFuncdef("void fptr()");
		r = engine->RegisterGlobalProperty("fptr f", 0);
		if( r >= 0 ) TEST_FAILED;
		engine->RegisterObjectType("obj", 0, asOBJ_REF);
		r = engine->RegisterObjectProperty("obj", "fptr f", 0);
		if( r >= 0 ) TEST_FAILED;

		engine->Release();
	}

	// Test passing handle to function pointer
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		mod->AddScriptSection("script",
			"class CTempObj             \n"
			"{                          \n"
			"  int Temp;                \n"
			"}                          \n"
			"funcdef void FUNC2(CTempObj@);\n"
			"class CMyObj               \n"
			"{                          \n"
			"  CMyObj() { @f2= @func2; }\n"
			"  FUNC2@ f2;               \n"
			"}                          \n"
			"void main()                \n"
			"{                          \n"
			"  CMyObj o;                \n"
			"  CTempObj t;              \n"
			"  o.f2(t);                 \n"
			"  assert( called == 1 );   \n"
			"}                          \n"
			"int called = 0;            \n"
			"void func2(CTempObj@ Obj)  \n"
			"{ called++; }              \n");

		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test out of order declaration with function pointers
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		mod->AddScriptSection("script",
			"funcdef void FUNC2(CTempObj@);\n"
			"class CTempObj {}             \n");

		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// It must be possible calling system functions through pointers too
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		engine->RegisterFuncdef("bool fun(bool)");
		engine->RegisterGlobalFunction("bool assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		r = ExecuteString(engine, "fun @f = assert; f(true);");
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// It should be possible to call functions through function pointers returned by an expression
	// http://www.gamedev.net/topic/627386-bug-with-parsing-of-callable-expressions/
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, false);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		mod->AddScriptSection("Test",
			"funcdef void F(int); \n"
			"array<F@> arr(1); \n"
			"F@ g()            \n"
			"{                 \n"
			"  return test;    \n"
			"}                 \n"
			"void test(int a)  \n"
			"{                 \n"
			"  assert(a == 42); \n"
			"  called++;       \n"
			"}                 \n"
			"int called = 0;   \n"
			"void f()          \n"
			"{                 \n"
			"  @arr[0] = test; \n"
			"  arr[0](42);     \n"
			"  g()(42);        \n"
			"  F@ p; \n"
			"  (@p = arr[0])(42);     \n"
			"  (@p = g())(42);        \n"
			"}                        \n");

	//	engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, false);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "f()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		int idx = mod->GetGlobalVarIndexByName("called");
		int *called = (int*)mod->GetAddressOfGlobalVar(idx);
		if( *called != 4 )
			TEST_FAILED;

		engine->Release();
	}

	// Global function pointers must not overload local class methods
	// Local variables take precedence over class methods
	// http://www.gamedev.net/topic/626746-function-call-operators-in-the-future/
	{
		const char *script = 
			"funcdef void FUNC(); \n"
			"FUNC @func; \n"
			"class Class \n"
			"{ \n"
			"  void func() {} \n"
			"  void method() \n"
			"  { \n"
			"    func(); \n"       // Should call Class::func()
			"  } \n"
			"  void func2() {} \n"
			"  void method2() \n"
			"  { \n"
			"    FUNC @func2; \n"
			"    func2(); \n"      // Should call variable func2
			"  } \n"
			"} \n";

		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "Class c; c.method();", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		r = ExecuteString(engine, "Class c; c.method2();", mod);
		if( r != asEXECUTION_EXCEPTION )
			TEST_FAILED;

		engine->Release();
	}

	// Success
 	return fail;
}

} // namespace

