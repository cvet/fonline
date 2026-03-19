#include "utils.h"

namespace TestVarType
{

static const char * const TESTNAME = "TestVarType";

void sumi_generic(asIScriptGeneric* gen)
{
	int result = 0;

	int count = gen->GetArgCount();
	for (int i = 0; i < count; ++i)
	{
		asDWORD flags;
		int typeId = gen->GetArgTypeId(i, &flags);
		if (typeId & ~asTYPEID_MASK_SEQNBR)
			continue;

		switch (typeId)
		{
		case asTYPEID_INT32:
			if ((flags & asTM_INREF) || (flags & asTM_INOUTREF))
				result += *(int*)gen->GetArgAddress(i);
			else
				result += (int)gen->GetArgDWord(i);
			break;
		}
	}

	gen->SetReturnDWord(result);
}

void sum_generic(asIScriptGeneric* gen)
{
	float result = gen->GetArgFloat(0);

	int count = gen->GetArgCount();
	for (int i = 1; i < count; ++i)
	{
		asDWORD flags;
		int typeId = gen->GetArgTypeId(i, &flags);
		if (typeId & ~asTYPEID_MASK_SEQNBR)
			continue;

		assert(flags & asTM_INREF);

		switch (typeId)
		{
		case asTYPEID_INT32:
			result += *(int*)gen->GetArgAddress(i);
			break;

		case asTYPEID_FLOAT:
			result += *(float*)gen->GetArgAddress(i);
		}
	}

	gen->SetReturnFloat(result);
}


class my_ints
{
public:
	std::vector<int> values;

	my_ints* get()
	{
		return this;
	}

	static void append_generic(asIScriptGeneric* gen)
	{
		my_ints& is = *(my_ints*)gen->GetObject();

		int i = 0;
		for (; i < gen->GetArgCount(); ++i)
		{
			assert(gen->GetArgTypeId(i) == asTYPEID_INT32);
			int v = (int)gen->GetArgDWord(i);

			is.values.push_back(v);
		}

		gen->SetReturnDWord(i);
	}
};

// AngelScript syntax: void testFuncI(?& in)
// C++ syntax: void testFuncI(void *ref, int typeId)
// void testFuncI(void *ref, int typeId)
void testFuncI(asIScriptGeneric *gen)
{
	void *ref = gen->GetArgAddress(0);
	int typeId = gen->GetArgTypeId(0);
	if( typeId == gen->GetEngine()->GetTypeIdByDecl("int") )
		assert(*(int*)ref == 42);
	else if( typeId == gen->GetEngine()->GetTypeIdByDecl("string") )
		assert(((CScriptString*)ref)->buffer == "test");
	else if( typeId == gen->GetEngine()->GetTypeIdByDecl("string@") )
		assert((*(CScriptString**)ref)->buffer == "test");
	else if( typeId == 0 )
		assert( *(void**)ref == 0 );
	else
		assert(false);
}

// AngelScript syntax: void testFuncS(string &in)
// C++ syntax: void testFuncS(string &)
void testFuncS(asIScriptGeneric *gen)
{
	CScriptString *str = (CScriptString*)gen->GetArgAddress(0);
	assert( str->buffer == "test" );
}

// AngelScript syntax: void testFuncO(?& out)
// C++ syntax: void testFuncO(void *ref, int typeId)
// void testFuncO(void *ref, int typeId)
void testFuncO(asIScriptGeneric *gen)
{
	void *ref = gen->GetArgAddress(0);
	int typeId = gen->GetArgTypeId(0);
	if( typeId == gen->GetEngine()->GetTypeIdByDecl("int") )
		*(int*)ref = 42;
	else if( typeId == gen->GetEngine()->GetTypeIdByDecl("string") )
		((CScriptString*)ref)->buffer = "test";
	else if( typeId == gen->GetEngine()->GetTypeIdByDecl("string@") )
		*(CScriptString**)ref = new CScriptString("test");
	else if( typeId == 0 )
		assert( *(void**)ref == 0 );
	else
		assert(false);
}

// AngelScript syntax: void testFuncIS(?& in, const string &in)
// C++ syntax: void testFuncIS(void *ref, int typeId, CScriptString &)
void testFuncIS(void *ref, int typeId, CScriptString &str)
{
	assert(str.buffer == "test");

	// Primitives are received as a pointer to the value
	// Handles are received as a pointer to the handle
	// Objects are received as a pointer to the object 
	if( typeId & asTYPEID_OBJHANDLE )
		assert( **(std::string**)ref == "t" );
	else if( typeId & asTYPEID_MASK_OBJECT )
		assert( *(std::string*)ref == "t" );
	else
		assert( *(int*)ref == 42 );
}

void testFuncIS_generic(asIScriptGeneric *gen)
{
	void *ref = gen->GetArgAddress(0);
	int typeId = gen->GetArgTypeId(0);
	CScriptString *str = *(CScriptString**)gen->GetAddressOfArg(1);

	testFuncIS(ref,typeId,*str);
}

// AngelScript syntax: void testFuncSI(const string &in, ?& in)
// C++ syntax: void testFuncSI(CScriptString &, void *ref, int typeId)
void testFuncSI(CScriptString &str, void *ref, int /*typeId*/)
{
	assert(str.buffer == "test");
	assert(*(int*)ref == 42);
}

void testFuncSI_generic(asIScriptGeneric *gen)
{
	CScriptString *str = *(CScriptString**)gen->GetAddressOfArg(0);
	void *ref = gen->GetArgAddress(1);
	int typeId = gen->GetArgTypeId(1);

	testFuncSI(*str, ref, typeId);
}


void testSetReturnObject(asIScriptGeneric* gen)
{
	std::string t = "test";
	gen->SetReturnObject(&t);
}


int numArgs = 0;
void testFactVariadic(asIScriptGeneric* gen)
{
	numArgs = gen->GetArgCount();
}

asIScriptFunction* calledFunc = 0;
void func1(asIScriptGeneric* gen)
{
	calledFunc = gen->GetFunction();
}

void funcConstruct(asIScriptGeneric* gen)
{
	calledFunc = gen->GetFunction();
	gen->SetReturnAddress((void*)1); // Dummy pointer
}

void funcConstructNoArgs(asIScriptGeneric* gen)
{
	gen->SetReturnAddress((void*)1); // Dummy pointer
}

void dummy(asIScriptGeneric* gen)
{
	// Do nothing
}

bool Test()
{
	RET_ON_MAX_PORT

	bool fail = false;
	int r;
	COutStream out;
	CBufferedOutStream bout;
 	asIScriptEngine *engine = 0;
	asIScriptModule *mod = 0;
	asIScriptContext *ctx = 0;

	// Test registering void f(?[]&). Must fail with appropriate error message
	// Reported by Aleksander Jaronik
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		RegisterScriptArray(engine, true);
		r = engine->RegisterGlobalFunction("void func(?[]&)", asFUNCTION(0), asCALL_GENERIC);
		if (r >= 0)
			TEST_FAILED;
		r = engine->RegisterGlobalFunction("void func2(array<?>&)", asFUNCTION(0), asCALL_GENERIC);
		if (r >= 0)
			TEST_FAILED;
		if (bout.buffer != "System function (1, 12) : Error   : Data type can't be '?'\n"
						   " (0, 0) : Error   : Failed in call to function 'RegisterGlobalFunction' with 'void func(?[]&)' (Code: asINVALID_DECLARATION, -10)\n"
						   "System function (1, 18) : Error   : Expected data type\n"
						   "System function (1, 18) : Error   : Instead found '?'\n"
						   " (0, 0) : Error   : Failed in call to function 'RegisterGlobalFunction' with 'void func2(array<?>&)' (Code: asINVALID_DECLARATION, -10)\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		engine->ShutDownAndRelease();
	}

	// Test saving and loading bytecode with variadic functions
	// Reported by Aleksander Jaronik
	{
		engine = asCreateScriptEngine();
		engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 0);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void Print(const ?&in ...)", asFUNCTION(0), asCALL_GENERIC);
		engine->RegisterGlobalFunction("string Format(const ?&in ...)", asFUNCTION(0), asCALL_GENERIC);
		engine->RegisterObjectMethod("string", "string Mthd(const ?&in ...)", asFUNCTION(0), asCALL_GENERIC);
		engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT, "void f(const ?&in ...)", asFUNCTION(0), asCALL_GENERIC);
		engine->RegisterFuncdef("string CB(const ?&in ...)");

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void main() \n"
			"{ \n"
			"	for (uint i = 0; i < 10; i++) \n"
			"	{ \n"
			"		Print(5, 3, 1); \n" // global function
			"		string s = Format(5, 3, 1); \n" // global function returning object by value
			"		('a'+'b').Mthd(5, 3, 1); \n" // class method returning object by value
			"		string t(5, 3, 1); \n" // constructor
			"		CB @cb = Format; \n"
			"		cb(5, 3, 1); \n" // function pointer
			"	} \n"
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		CBytecodeStream stream((std::string("AS_DEBUG/bc_") + (sizeof(void*) == 4 ? "32" : "64")).c_str());
		r = mod->SaveByteCode(&stream);
		if (r < 0)
			TEST_FAILED;

		mod = engine->GetModule("mod2", asGM_ALWAYS_CREATE);
		r = mod->LoadByteCode(&stream);
		if (r < 0)
			TEST_FAILED;

		engine->ShutDownAndRelease();
		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}


	// Test behavior of overload when matching function with vartype compared to function with default args
	// Reported by Aleksander Jaronik
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		engine->RegisterObjectType("MyType", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("MyType", asBEHAVE_FACTORY, "MyType@ f()", asFUNCTION(funcConstructNoArgs), asCALL_GENERIC);
		r = engine->RegisterObjectBehaviour("MyType", asBEHAVE_FACTORY, "MyType@ f(int a, int b = 0)", asFUNCTION(funcConstruct), asCALL_GENERIC);
		asIScriptFunction* expectedFunc = engine->GetFunctionById(r);
		engine->RegisterObjectBehaviour("MyType", asBEHAVE_FACTORY, "MyType@ f(?&in a)", asFUNCTION(funcConstruct), asCALL_GENERIC);
		engine->RegisterObjectBehaviour("MyType", asBEHAVE_ADDREF, "void f()", asFUNCTION(dummy), asCALL_GENERIC);
		engine->RegisterObjectBehaviour("MyType", asBEHAVE_RELEASE, "void f()", asFUNCTION(dummy), asCALL_GENERIC);
		engine->RegisterObjectMethod("MyType", "MyType &opAssign(const MyType &in)", asFUNCTION(dummy), asCALL_GENERIC);

		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void main1() { \n"
			"  MyType t1(42); \n" // must call f(int a, int b = 0)
			"} \n"
			"void main2() { \n"
			"  MyType t2 = MyType(42); \n" // must call f(int a, int b = 0)
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		calledFunc = 0;
		r = ExecuteString(engine, "main1()", mod);
		if( r!= asEXECUTION_FINISHED )
			TEST_FAILED;
		if (calledFunc != expectedFunc)
			TEST_FAILED;

		calledFunc = 0;
		r = ExecuteString(engine, "main2()", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		if (calledFunc != expectedFunc)
			TEST_FAILED;

		engine->ShutDownAndRelease();
		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test that variadic functions accept 0 args
	// https://www.gamedev.net/forums/topic/719538-variadic-functions-can-not-take-0-args/5472673/
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		r = engine->RegisterGlobalFunction("void func(const ?&in ...)", asFUNCTION(testFactVariadic), asCALL_GENERIC);
		if (r < 0)
			TEST_FAILED;
		numArgs = -1;
		r = ExecuteString(engine, "func();");
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		if (numArgs != 0)
			TEST_FAILED;
		engine->ShutDownAndRelease();
		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test function overloads with variadic functions
	// https://github.com/anjo76/angelscript/issues/14
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		int fn1 = engine->RegisterGlobalFunction("void fn(int, const ?&in ...)", asFUNCTION(func1), asCALL_GENERIC);
		int fn2 = engine->RegisterGlobalFunction("void fn(int, const ?&in a)", asFUNCTION(func1), asCALL_GENERIC);
		if( fn2 < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "fn(1, 2);");
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		if( calledFunc == 0 || calledFunc->GetId() != fn2 )
			TEST_FAILED;

		r = ExecuteString(engine, "fn(1, 2, 3);");
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		if (calledFunc == 0 || calledFunc->GetId() != fn1)
			TEST_FAILED;

		engine->ShutDownAndRelease();
		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test saving and loading bytecode using variadic functions
	// Reported by Paril
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterStdString(engine);

		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void func() \n"
			"{ \n"
			"	string str = format('{} {} {} {}', 1, 3, 5, 7); \n"
			"   assert( str == '1 3 5 7' ); \n"
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "func()", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		CBytecodeStream stream((std::string("AS_DEBUG/bc_") + (sizeof(void*) == 4 ? "32" : "64")).c_str());
		r = mod->SaveByteCode(&stream); assert(r >= 0);
		mod->Discard();

		mod = engine->GetModule("test2", asGM_ALWAYS_CREATE);
		r = mod->LoadByteCode(&stream);
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "func()", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		engine->ShutDownAndRelease();

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test func overload between 'int &out' and '? &out' with enum expression. The correct one to use is '? &out'
	// Reported by Paril
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterStdString(engine);

		bout.buffer = "";

		engine->RegisterGlobalFunction("void test(int32 & out)", asFUNCTION(func1), asCALL_GENERIC);
		engine->RegisterGlobalFunction("void test(? & out)", asFUNCTION(func1), asCALL_GENERIC);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"enum MyEnum { Val = 50 } \n"
			"void func() \n"
			"{ \n"
			"	MyEnum v = MyEnum::Val; \n"
			"	test(v); \n" // Which function is called?
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		calledFunc = 0;
		r = ExecuteString(engine, "func()", mod);
		if (r < 0)
			TEST_FAILED;

		if (std::string(calledFunc->GetDeclaration()) != "void test(?&out)")
			TEST_FAILED;

		engine->ShutDownAndRelease();

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test ... in constructor
	// Reported by Paril
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterStdString(engine);

		bout.buffer = "";

		engine->RegisterObjectType("test", 0, asOBJ_REF | asOBJ_NOCOUNT);
		engine->RegisterObjectBehaviour("test", asBEHAVE_FACTORY, "test @f(const ?&in ...)", asFUNCTION(testFactVariadic), asCALL_GENERIC);

		numArgs = 0;
		r = ExecuteString(engine, "test @t = test(1,2,3);");
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		engine->ShutDownAndRelease();

		if (numArgs != 3)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test issue with asIGeneric::SetReturnObject
	// https://www.gamedev.net/forums/topic/717861-returning-an-object-on-the-stack-in-a-variadic-function-causes-a-crash/5468071/
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterStdString(engine);

		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		engine->RegisterGlobalFunction("string test(const ?&in ...)", asFUNCTION(testSetReturnObject), asCALL_GENERIC);

		r = ExecuteString(engine, "assert( test(1,2) == 'test' );");
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->ShutDownAndRelease();

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test issue with variadic and stack size
	// Reported by Paril
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterStdString(engine);

		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"string func(string a, string b, string c) { \n"
			"  assert( a == 'a' ); \n"
			"  assert( b == 'b' ); \n"
			"  assert( c == '1;2;' ); \n"
			"  return a + b + c; \n"
			"} \n"
			"void main() {\n"
			"  string t = func('a', 'b', format('{};{};', 1, 2) ); \n"
			"  assert( t == 'ab1;2;' ); \n"
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		ctx = engine->CreateContext();
		r = ExecuteString(engine, "main()", mod, ctx);
		if (r != asEXECUTION_FINISHED)
		{
			TEST_FAILED;
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("%s\n", GetExceptionInfo(ctx, true).c_str());
		}
		ctx->Release();

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test variadic with fixed type
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		r = engine->RegisterGlobalFunction(
			"int sumi(int init, const int&in ...)",
			asFUNCTION(sumi_generic),
			asCALL_GENERIC
		);
		if (r < 0)
			TEST_FAILED;

		int sum;
		r = ExecuteString(engine, "return sumi(1000, 100, 10);", &sum, asTYPEID_INT32);
		if (r < 0)
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		if (sum != 1110)
		{
			PRINTF("%d\n", sum);
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test variadic with var arg type
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		r = engine->RegisterGlobalFunction(
			"float sum(float init, const ?&in ...)",
			asFUNCTION(sum_generic),
			asCALL_GENERIC
		);
		if (r < 0)
			TEST_FAILED;

		float sum;
		r = ExecuteString(engine, "return sum(1.5f, 5, 0.6f);", &sum, asTYPEID_FLOAT);
		if (r < 0)
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		if ((int)sum != 7)
		{
			PRINTF("%f\n", sum);
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test variadic in class methods
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		r = engine->RegisterObjectType("my_ints", sizeof(my_ints), asOBJ_REF | asOBJ_NOHANDLE);
		if (r < 0)
			TEST_FAILED;

		r = engine->RegisterObjectMethod(
			"my_ints",
			"int append(int...)",
			asFUNCTION(&my_ints::append_generic),
			asCALL_GENERIC
		);
		if (r < 0)
			TEST_FAILED;

		my_ints is;

		r = engine->RegisterGlobalFunction(
			"my_ints& get()",
			asMETHOD(my_ints, get),
			asCALL_THISCALL_ASGLOBAL,
			&is
		);
		if (r < 0)
			TEST_FAILED;

		int count;
		r = ExecuteString(engine, "return get().append(1, 10, 100);", &count, asTYPEID_INT32);
		if (r < 0)
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		if (count != 3)
		{
			PRINTF("%d\n", count);
			TEST_FAILED;
		}

		assert(is.values[0] == 1);
		assert(is.values[1] == 10);
		assert(is.values[2] == 100);

		engine->ShutDownAndRelease();
	}

	// Test behaviour of var type with unsafe references
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true);

		static bool expectNullRef;
		static bool expectHandleType;
		struct Test {
			static void Func(void *ref, int typeId) 
			{
				assert( (expectNullRef && ref == 0) || (!expectNullRef && ref != 0) );
				assert( (expectHandleType && (typeId & asTYPEID_OBJHANDLE)) || (!expectHandleType && (typeId & asTYPEID_OBJHANDLE) == 0) );
			}
		};

		engine->RegisterGlobalFunction("void funcO(?& out)", asFUNCTION(Test::Func), asCALL_CDECL);
		engine->RegisterGlobalFunction("void funcIO(?&)", asFUNCTION(Test::Func), asCALL_CDECL);

		const char *script = "class ScriptClass { int a = 0; } \n";
		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		ctx = engine->CreateContext();

		expectNullRef = false;
		expectHandleType = false;
		r = ExecuteString(engine, "ScriptClass @t = ScriptClass(); funcO(t);", mod, ctx);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		if( r == asEXECUTION_EXCEPTION )
			PRINTF("%s", GetExceptionInfo(ctx).c_str());

		expectNullRef = true;
		expectHandleType = false;
		r = ExecuteString(engine, "ScriptClass @t; funcIO(t);", mod, ctx);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		if( r == asEXECUTION_EXCEPTION )
			PRINTF("%s", GetExceptionInfo(ctx).c_str());

		expectNullRef = false;
		expectHandleType = true;
		r = ExecuteString(engine, "ScriptClass @t; funcO(@t);", mod, ctx);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		if( r == asEXECUTION_EXCEPTION )
			PRINTF("%s", GetExceptionInfo(ctx).c_str());

		expectNullRef = false;
		expectHandleType = true;
		r = ExecuteString(engine, "ScriptClass @t; funcIO(@t);", mod, ctx);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		if( r == asEXECUTION_EXCEPTION )
			PRINTF("%s", GetExceptionInfo(ctx).c_str());

		// When disallowing value assignments, only the handle is passed to var args
		engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true);

		expectNullRef = false;
		expectHandleType = true;
		r = ExecuteString(engine, "ScriptClass @t; funcO(t); funcO(@t); funcIO(t); funcIO(@t);", mod, ctx);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		if( r == asEXECUTION_EXCEPTION )
			PRINTF("%s", GetExceptionInfo(ctx).c_str());

		ctx->Release();
		engine->Release();
	}

	// It must not be possible to declare global variables of the var type ?
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	const char *script1 = "? globvar;";
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection("script", script1);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 1) : Error   : Unexpected token '?'\n" ) TEST_FAILED;
	bout.buffer = "";

	// It must not be possible to declare local variables of the var type ?
	const char *script2 = "void func() {? localvar;}";
	mod->AddScriptSection("script", script2);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 1) : Info    : Compiling void func()\n"
                       "script (1, 14) : Error   : Expected expression value\n"
					   "script (1, 14) : Error   : Instead found '?'\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";

	// It must not be possible to register global properties of the var type ?
	r = engine->RegisterGlobalProperty("? prop", (void*)1);
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "Property (1, 1) : Error   : Expected data type\n"
		               "Property (1, 1) : Error   : Instead found '?'\n"
	                   " (0, 0) : Error   : Failed in call to function 'RegisterGlobalProperty' with '? prop' (Code: asINVALID_DECLARATION, -10)\n" ) 
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";
	engine->Release();

	// It must not be possible to register object members of the var type ?
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	r = engine->RegisterObjectType("test", 0, asOBJ_REF); assert( r >= 0 );
	r = engine->RegisterObjectProperty("test", "? prop", 0);
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "Property (1, 1) : Error   : Expected data type\n"
		               "Property (1, 1) : Error   : Instead found '?'\n"
		               " (0, 0) : Error   : Failed in call to function 'RegisterObjectProperty' with 'test' and '? prop' (Code: asINVALID_DECLARATION, -10)\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";
	engine->Release();

	// It must not be possible to declare script class members of the var type ?
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	const char *script3 = "class c {? member;}";
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection("script", script3);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 10) : Error   : Expected method or property\n"
		               "script (1, 10) : Error   : Instead found '?'\n"
		               "script (1, 19) : Error   : Unexpected token '}'\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";
	
	// It must not be possible to declare script functions that take the var type ? as parameter 
	const char *script4 = "void func(?&in a) {}";
	mod->AddScriptSection("script", script4);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 11) : Error   : Expected data type\n"
		               "script (1, 11) : Error   : Instead found '?'\n")
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";

	// It must not be possible to declare script functions that return the var type ?
	const char *script5 = "? func() {}";
	mod->AddScriptSection("script", script5);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 1) : Error   : Unexpected token '?'\n" ) TEST_FAILED;
	bout.buffer = "";

	// It must not be possible to declare script class methods that take the var type ? as parameter
	const char *script6 = "class c {void method(?& in a) {}}";
	mod->AddScriptSection("script", script6);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 22) : Error   : Expected data type\n" 
		               "script (1, 22) : Error   : Instead found '?'\n"
					   "script (1, 33) : Error   : Unexpected token '}'\n" ) 
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";

	// It must not be possible to declare script class methods that return the var type ?
	const char *script7 = "class c {? method() {}}";
	mod->AddScriptSection("script", script7);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 10) : Error   : Expected method or property\n"
		               "script (1, 10) : Error   : Instead found '?'\n"
		               "script (1, 23) : Error   : Unexpected token '}'\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";

	// It must not be possible to declare arrays of the var type ?
	const char *script8 = "void func() { ?[] array; }";
	mod->AddScriptSection("script", script8);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 1) : Info    : Compiling void func()\n"
		               "script (1, 15) : Error   : Expected expression value\n"
					   "script (1, 15) : Error   : Instead found '?'\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";

	// It must not be possible to declare handles of the var type ?
	const char *script9 = "void func() { ?@ handle; }";
	mod->AddScriptSection("script", script9);
	r = mod->Build();
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "script (1, 1) : Info    : Compiling void func()\n"
		               "script (1, 15) : Error   : Expected expression value\n"
					   "script (1, 15) : Error   : Instead found '?'\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";

	// It must not be possible to register functions that return the var type ?
	r = engine->RegisterGlobalFunction("? testFunc()", asFUNCTION(testFuncI), asCALL_GENERIC);
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "System function (1, 1) : Error   : Expected data type\n"
					   "System function (1, 1) : Error   : Instead found '?'\n"
		               " (0, 0) : Error   : Failed in call to function 'RegisterGlobalFunction' with '? testFunc()' (Code: asINVALID_DECLARATION, -10)\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	bout.buffer = "";
	engine->Release();

	// It must be possible to register functions that take the var type ? as parameter
	// Only when the expression is explicitly sent as @ should the type id be @
	// const ? & in
	// ? & in
	// TODO: Should have syntax to inform that only handle or only non-handle can be informed
	//       Maybe 'const ? @ & in' for only handle 
	//             '? @- & in' for only non-handle
	//             '? @+ & in' for only handle with auto-handle
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	RegisterScriptString(engine);
	r = engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

	r = engine->RegisterGlobalFunction("void testFuncI(?& in)", asFUNCTION(testFuncI), asCALL_GENERIC);
	if( r < 0 ) TEST_FAILED;
	r = engine->RegisterGlobalFunction("void testFuncCI(const?&in)", asFUNCTION(testFuncI), asCALL_GENERIC);
	if( r < 0 ) TEST_FAILED;
	r = engine->RegisterGlobalFunction("void testFuncS(string &in)", asFUNCTION(testFuncS), asCALL_GENERIC);

	r = ExecuteString(engine, "int a = 42; testFuncI(a);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "string a = \"test\"; testFuncI(a);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "string @a = \"test\"; testFuncI(@a);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "testFuncI(null);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;

	// Both functions should receive the string by reference
	r = ExecuteString(engine, "string a = 'test'; testFuncI(a); testFuncS(a);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;

	// It must be possible to register with 'out' references
	// ? & out
	// TODO: Should have syntax to inform that only handle or only non-handle can be informed
	//       Maybe '? @ & out' for only handle
	//             '? @- & out' for non-handle
	//             '? @+ & out' for auto handle
	r = engine->RegisterGlobalFunction("void testFuncO(?&out)", asFUNCTION(testFuncO), asCALL_GENERIC);
	if( r < 0 ) TEST_FAILED;

	r = ExecuteString(engine, "testFuncO(0)"); // skip out value
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "testFuncO(void)"); // skip out value
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "testFuncO(null)"); // skip out value
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "int a; testFuncO(a); assert(a == 42);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "string a; testFuncO(a); assert(a == \"test\");");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "string @a; testFuncO(@a); assert(a == \"test\");");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;

	// It must be possible to mix normal parameter types with the var type ?
	// e.g. func(const string &in, const ?& in), or func(const ?& in, const string &in)
	r = engine->RegisterGlobalFunction("void testFuncIS(?& in, const string &in)", asFUNCTION(testFuncIS_generic), asCALL_GENERIC);
	if( r < 0 ) TEST_FAILED;
	r = engine->RegisterGlobalFunction("void testFuncSI(const string &in, ?& in)", asFUNCTION(testFuncSI_generic), asCALL_GENERIC);
	if( r < 0 ) TEST_FAILED;

	r = ExecuteString(engine, "int a = 42; testFuncIS(a, \"test\");");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "int a = 42; testFuncSI(\"test\", a);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "string a = \"t\"; testFuncIS(a, \"test\");");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "string a = \"t\"; testFuncIS(@a, \"test\");");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;


	// It must be possible to use native functions
	SKIP_ON_MAX_PORT
	{
		r = engine->RegisterGlobalFunction("void _testFuncIS(?& in, const string &in)", asFUNCTION(testFuncIS), asCALL_CDECL);
		if( r < 0 ) TEST_FAILED;
		r = engine->RegisterGlobalFunction("void _testFuncSI(const string &in, ?& in)", asFUNCTION(testFuncSI), asCALL_CDECL);
		if( r < 0 ) TEST_FAILED;

		r = ExecuteString(engine, "int a = 42; _testFuncIS(a, \"test\");");
		if( r != asEXECUTION_FINISHED ) TEST_FAILED;
		r = ExecuteString(engine, "int a = 42; _testFuncSI(\"test\", a);");
		if( r != asEXECUTION_FINISHED ) TEST_FAILED;
		r = ExecuteString(engine, "string a = \"t\"; _testFuncIS(a, \"test\");");
		if( r != asEXECUTION_FINISHED ) TEST_FAILED;
		r = ExecuteString(engine, "string a = \"t\"; _testFuncIS(@a, \"test\");");
		if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	}

	// Don't give error on passing reference to const to ?&out
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	const char *script = 
	"class C { string @a; } \n";
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection("script", script);
	mod->Build();
	r = ExecuteString(engine, "const C c; testFuncO(@c.a);", mod);
	if( r >= 0 ) TEST_FAILED;
	if( bout.buffer != "ExecuteString (1, 23) : Error   : Output argument expression is not assignable\n" ) TEST_FAILED;
	bout.buffer = "";

	// ?& with opAssign is allowed, but won't be used with the assignment operator
	// TODO: Support ?& with the operators as well
	engine->RegisterObjectType("type", sizeof(int), asOBJ_VALUE | asOBJ_APP_PRIMITIVE);
	r = engine->RegisterObjectMethod("type", "type &opAssign(const ?& in)", asFUNCTION(testFuncSI_generic), asCALL_GENERIC);
	if( r < 0 )
		TEST_FAILED;
	// TODO: This is a valid class method, but should perhaps not be allowed to be used as operator
	/*
	r = engine->RegisterObjectMethod("type", "type opAdd(const ?& in)", asFUNCTION(testFuncSI_generic), asCALL_GENERIC);
	if( r >= 0 )
		TEST_FAILED;
	*/
	
	// Don't allow use of ? without being reference
	r = engine->RegisterGlobalFunction("void testFunc_err(const ?)", asFUNCTION(testFuncSI_generic), asCALL_GENERIC);
	if( r >= 0 )
		TEST_FAILED;

	// Don't allow use of 'inout' reference, yet
	// ? & [inout]
	// const ? & [inout]
	r = engine->RegisterGlobalFunction("void testFuncIO(?&)", asFUNCTION(testFuncSI_generic), asCALL_GENERIC);
	if( r >= 0 ) TEST_FAILED;
	r = engine->RegisterGlobalFunction("void testFuncCIO(const?&)", asFUNCTION(testFuncSI_generic), asCALL_GENERIC);
	if( r >= 0 ) TEST_FAILED;

	engine->Release();

	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		RegisterScriptString(engine);
		r = engine->RegisterObjectType("obj", sizeof(int), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE); assert( r >= 0 );
		r = engine->RegisterObjectMethod("obj", "string @fmt(const string &in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in, ?&in)", asFUNCTION(testFuncSI_generic), asCALL_GENERIC); assert( r >= 0 );

		mod = engine->GetModule("1", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", 
			"class App {\n"
			"	int Run() {\n"
			"		return 0;\n"
			"	}\n"
			"}\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	return fail;
}

} // namespace

