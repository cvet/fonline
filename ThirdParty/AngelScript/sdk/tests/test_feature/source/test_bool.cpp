#include "utils.h"

namespace TestBool
{


class TestBoolClass
{
public:
	TestBoolClass()
	{
		m_fail = false;
	}

	void TestTrue(bool value)
	{
		if( !value )
		{
			m_fail = true;
		}
	}

	void TestFalse(bool value)
	{
		if( value )
		{
			m_fail = true;
		}
	}

	bool m_fail;
};

class tst
{
public:

  int test_f(unsigned int /*param*/)
  {
	if( sizeof(bool) == 1 )
	{
		// Force return false with trash in upper bytes, to test if AngelScript is able to handle this
		// We need to make both the high and low bytes 0, because depending on the system the high or low byte is used as the returned value,
		// on intel it is the low byte, on 32 bit ppc it is the low byte, on 64 bit ppc it is the high byte
		return 0x00FFFF00;
	}
	else
		return 0;
  }
};


void CFunc(float /*f*/, int a, int b, const std::string & /*name*/)
{
	if( (a & 0xFFFFFF00) || (b & 0xFFFFFF00) )
	{
		PRINTF("Receiving boolean value with scrap in higher bytes. Not sure this is an error.\n");
	}
}

bool test_t()
{
	return true;
}

void GiveFalse(int &boolean)
{
	if( sizeof(bool) == 1 )
	{
		// Force return false with trash in upper bytes, to test if AngelScript is able to handle this
		// We need to make both the high and low bytes 0, because depending on the system the high or low byte is used as the returned value,
		// on intel it is the low byte, on 32 bit ppc it is the low byte, on 64 bit ppc it is the high byte
		boolean = 0x00FFFF00;
	}
	else
		boolean = 0;
}

std::string buf;
void Print(std::string &str)
{
	buf += str + "\n";
//	PRINTF("%s\n", str.c_str());
}

bool ConvToBool(int &obj)
{
	return obj == 0 ? false : true;
}

void ConstructFromBool(bool v, int &obj)
{
	obj = v ? 1 : 0;
}

bool Test()
{
	RET_ON_MAX_PORT

	bool fail = false;
	int r;
	COutStream out;
	CBufferedOutStream bout;

	// Test explicit conv from bool to uint
	// https://github.com/anjo76/angelscript/issues/27
	{
		asIScriptEngine *engine = asCreateScriptEngine();
		r = engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		r = ExecuteString(engine, "bool f = false; assert( uint(f) == 0 );");
		if (r >= 0)
			TEST_FAILED;

		r = ExecuteString(engine, "assert( uint(true) == 1 );");
		if (r >= 0)
			TEST_FAILED;

		engine->ShutDownAndRelease();

		if (bout.buffer != "ExecuteString (1, 25) : Error   : No conversion from 'bool' to 'const uint' available.\n"
						   "ExecuteString (1, 9) : Error   : No conversion from 'const bool' to 'const uint' available.\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// Test use of explicit opConv to bool in conditions, since it is known it will always be a boolean
	// https://www.gamedev.net/forums/topic/717477-feature-request-automatically-use-bool-opconv-in-a-if-statement/
	// Code submitted by HenryAWE
	// ref: https://en.cppreference.com/w/cpp/language/implicit_conversion (see Contextual conversions)
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->SetEngineProperty(asEP_BOOL_CONVERSION_MODE, 1);

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char* script =
			"class TestConv\n"
			"{\n"
			"  bool opConv() const\n"
			"  {\n"
			"    return false;\n"
			"  }\n"
			"} \n"
			"void func(bool arg) {} \n";

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		// test while, do-while, for, and ternary conditions as well
		r = ExecuteString(engine, "TestConv t; if(t) assert(false);", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "TestConv t; while(t) assert(false);", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "for( TestConv t; t; ) assert(false);", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "TestConv t; do {} while(t);", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;
		r = ExecuteString(engine, "TestConv t; assert( t ? false : true );", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		// Test operators on boolean values, !/not, &&/and, ||/or, ^^/xor
		r = ExecuteString(engine, "TestConv t; \n assert( !t ); \n assert( t || true ); \n assert( !(t && true) ); \n assert( t ^^ true ); \n", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		// Test that it still fails with other operations, for example calling a function that expects a bool
		r = ExecuteString(engine, "TestConv t; func(t);", mod);
		if (r > 0)
			TEST_FAILED;

		// Test that null pointer exception is thrown for handles
		asIScriptContext* ctx = engine->CreateContext();
		r = ExecuteString(engine, "TestConv @t; if( t ) {}", mod, ctx);
		if (r != asEXECUTION_EXCEPTION)
			TEST_FAILED;
		else if( std::string(ctx->GetExceptionString()) != "Null pointer access" )
		{
			PRINTF("got exception: %s\n", ctx->GetExceptionString());
			TEST_FAILED;
		}
		ctx->Release();

		if (bout.buffer != 
			"ExecuteString (1, 13) : Error   : No matching signatures to 'func(TestConv&)'\n"
			"ExecuteString (1, 13) : Info    : Candidates are:\n"
			"ExecuteString (1, 13) : Info    : void func(bool arg)\n"
			"ExecuteString (1, 13) : Info    : Rejected due to type mismatch on parameter 'arg'\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// Test the same without the explicit bool conversion
		bout.buffer = "";
		engine->SetEngineProperty(asEP_BOOL_CONVERSION_MODE, 0);

		r = ExecuteString(engine, "TestConv t; if(t) assert(false);", mod);
		if (r >= 0)
			TEST_FAILED;
		r = ExecuteString(engine, "TestConv t; if(t) assert(false);", mod);
		if (r >= 0)
			TEST_FAILED;
		r = ExecuteString(engine, "TestConv t; while(t) assert(false);", mod);
		if (r >= 0)
			TEST_FAILED;
		r = ExecuteString(engine, "for( TestConv t; t; ) assert(false);", mod);
		if (r >= 0)
			TEST_FAILED;
		r = ExecuteString(engine, "TestConv t; do {} while(t);", mod);
		if (r >= 0)
			TEST_FAILED;
		r = ExecuteString(engine, "TestConv t; assert( t ? false : true );", mod);
		if (r >= 0)
			TEST_FAILED;
		r = ExecuteString(engine, "TestConv t; \n assert( !t ); \n assert( t || true ); \n assert( !(t && true) ); \n assert( t ^^ true ); \n", mod);
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != 
			"ExecuteString (1, 16) : Error   : Expression must be of boolean type, instead found 'TestConv&'\n"
			"ExecuteString (1, 16) : Error   : Expression must be of boolean type, instead found 'TestConv&'\n"
			"ExecuteString (1, 19) : Error   : Expression must be of boolean type, instead found 'TestConv&'\n"
			"ExecuteString (1, 18) : Error   : Expression must be of boolean type, instead found 'TestConv&'\n"
			"ExecuteString (1, 16) : Error   : Expression must be of boolean type, instead found 'TestConv&'\n"
			"ExecuteString (1, 21) : Error   : Expression must be of boolean type, instead found 'TestConv&'\n"
			"ExecuteString (2, 10) : Error   : Illegal operation on this datatype\n"
			"ExecuteString (3, 12) : Error   : No conversion from 'TestConv&' to 'bool' available.\n"
			"ExecuteString (4, 14) : Error   : No conversion from 'TestConv&' to 'bool' available.\n"
			"ExecuteString (5, 12) : Error   : No conversion from 'TestConv&' to 'bool' available.\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Safe bool issue (where a value with boolean conversion is accidentally used for other purpose)
	// ref: http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Safe_bool

	// Testing implicit casts to bool from primitives (not allowed)
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		bout.buffer = "";
		const char *script = 
			"  int a = 1; \n"
			"  bool correct = false; \n"
			"  if( a ) \n"                 // implicit conversion to bool is not allowed by default
			"    correct = true; \n"
			"  assert( correct ); \n"
			"  correct = false; \n"
			"  if( bool(a) ) \n"           // explicit conversion to bool is not available by default
			"    correct = true; \n"
			"  assert( correct ); \n"
			"  correct = false; \n"
			"  if( a && true ) \n"         // binary logical operator
			"    correct = true; \n"
			"  assert( correct ); \n"
			"  correct = false; \n"
			"  if( !a == false ) \n"       // unary logical operator
			"    correct = true; \n"
			"  assert( correct ); \n";

		r = ExecuteString(engine, script);
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "ExecuteString (3, 7) : Error   : Expression must be of boolean type, instead found 'int'\n"
						   "ExecuteString (7, 7) : Error   : No conversion from 'int' to 'const bool' available.\n"
						   "ExecuteString (11, 9) : Error   : No conversion from 'int' to 'bool' available.\n"
						   "ExecuteString (15, 7) : Error   : Illegal operation on this datatype\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
/*
		// TODO: Should I allow implicit conversion of bool to/from math primitives?
		engine->SetEngineProperty(asEP_ALT_BOOL_CONV, 1); // allow bool to be used as arithmetic type, and also to allow arithmetic types to be used as bools

		r = ExecuteString(engine, script);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
*/
		engine->Release();
	}

	// Value types should allow bool opImplConv() to be used
	// http://www.gamedev.net/topic/662813-opconv-and-opimplconv-with-bool/
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterObjectType("MyBool", sizeof(int), asOBJ_APP_PRIMITIVE | asOBJ_VALUE | asOBJ_POD);
		engine->RegisterObjectMethod("MyBool", "bool opImplConv() const", asFUNCTION(ConvToBool), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("MyBool", asBEHAVE_CONSTRUCT, "void f(bool)", asFUNCTION(ConstructFromBool), asCALL_CDECL_OBJLAST);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		r = ExecuteString(engine,
			"MyBool b = false; \n"
			"assert( !b ); \n"
			"b = true; \n"
			"bool correct = false; \n"
			"if( b ) correct = true; \n"
			"assert( correct ); \n"
			"assert( b ); \n"
			"correct = false; \n"
			"if( b && true ) correct = true; \n"
			"assert( correct ); \n");
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}


	// TODO: Test with ref object types
	// For ref types with bool opImplConv() it should only be possible to directly convert if engine property is set
	// For ref types, if the handle is null an exception will be thrown, so the compiler should warn on the implicit conversion
	// if( @expr ) would be the same as if( expr !is null ) if the engine property is turned on

 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	RegisterScriptArray(engine, true);
	RegisterScriptString(engine);
	engine->RegisterGlobalFunction("void Assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);


	// TEST 1
	engine->RegisterGlobalFunction("void CFunc(float, bool, bool, const string &in)", asFUNCTION(CFunc), asCALL_CDECL);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	static const char *declarations = "  \n"
		"bool south = false;              \n"
		"bool north = true;               \n"
		"bool east = false;              \n"
		"bool west = true;               \n"
		"class MyClass                  \n"
		"{                              \n"
		"	string myName;             \n"
		"	float myFloat;             \n"
		"	bool myBool1;              \n"
		"	bool myBool2;              \n"
		"}                              \n"
		"MyClass[] a(4);                \n"
		"int maxCnt = 4;				\n"
		"int cnt = 0;				   \n"
		"\n";

	mod->AddScriptSection("decl", declarations, strlen(declarations));

	const char *script = "  \n"
		"void addToArray(string _name, float _myFloat, bool _bool1, bool _bool2) \n"
		"{							   \n"
		"	if(maxCnt == cnt)		   \n"
		"		return;				   \n"
		"	a[cnt].myName = _name;	   \n"
		"	a[cnt].myFloat = _myFloat; \n"
		"	a[cnt].myBool1 = _bool1;   \n"
		"	a[cnt].myBool2 = _bool2;   \n"
		"	cnt++;					   \n"
		"}							   \n"
		"							   \n"
		"void MyTest()                  \n"
		"{                              \n"
		"  MyClass c;                   \n"
		"  c.myName = \"test\";         \n"
		"  c.myFloat = 3.14f;           \n"
		"  c.myBool1 = south;           \n"
		"  c.myBool2 = south;           \n"
		"  Assert(c.myBool1 == false);  \n"
		"  Assert(c.myBool2 == false);  \n"
		"  c.myBool1 = north;            \n"
		"  Assert(c.myBool1 == true);   \n"
		"  Assert(c.myBool2 == false);  \n"
		"  c.myBool2 = north;            \n"
		"  Assert(c.myBool1 == true);   \n"
		"  Assert(c.myBool2 == true);   \n"
		"  c.myBool1 = south;           \n"
		"  Assert(c.myBool1 == false);  \n"
		"  Assert(c.myBool2 == true);   \n"
		"  Assert(c.myFloat == 3.14f);  \n"
		"  CFunc(c.myFloat, c.myBool1, c.myBool2, c.myName); \n"
		"								\n"
		"  addToArray(c.myName, 3.14f, south, east); \n"
		"  addToArray(c.myName, 3.14f, north, east); \n"
		"  addToArray(c.myName, 3.14f, south, west); \n"
		"  addToArray(c.myName, 3.14f, north, west); \n"
		"								\n"
		"  Assert(a[0].myBool1 == false);  \n"
		"  Assert(a[0].myBool2 == false);  \n"
		"  Assert(a[1].myBool1 == true);   \n"
		"  Assert(a[1].myBool2 == false);  \n"
		"  Assert(a[2].myBool1 == false);  \n"
		"  Assert(a[2].myBool2 == true);   \n"
		"  Assert(a[3].myBool1 == true);   \n"
		"  Assert(a[3].myBool2 == true);   \n"
		"  CFunc(a[0].myFloat, a[0].myBool1, a[0].myBool2, a[0].myName); \n"
		"  CFunc(a[1].myFloat, a[1].myBool1, a[1].myBool2, a[1].myName); \n"
		"  CFunc(a[2].myFloat, a[2].myBool1, a[2].myBool2, a[2].myName); \n"
		"  CFunc(a[3].myFloat, a[3].myBool1, a[3].myBool2, a[3].myName); \n"
		"}                              \n";
	mod->AddScriptSection("script", script, strlen(script));
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;

	r = ExecuteString(engine, "MyTest()", mod);
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;

	
	// TEST 2
	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	const char *script2 =
		"bool gFlag = false;\n"
		"void Set() {gFlag = true;}\n"
		"void DoNothing() {}\n";
	mod->AddScriptSection("script", script2, strlen(script2));
	r = mod->Build();
	if( r < 0 ) TEST_FAILED;

	int idx = engine->GetModule(0)->GetGlobalVarIndexByName("gFlag");
	bool *flag = (bool*)engine->GetModule(0)->GetAddressOfGlobalVar(idx);
	*(int*)flag = 0xCDCDCDCD;

	ExecuteString(engine, "Set()", mod);
	if( *flag != true )
		TEST_FAILED;
	r = ExecuteString(engine, "Assert(gFlag == true)", mod);
	if (r != asEXECUTION_FINISHED)
		TEST_FAILED;

	ExecuteString(engine, "gFlag = false; DoNothing()", mod);
	if( *flag != false )
		fail = false;
	r = ExecuteString(engine, "Assert(gFlag == false)", mod);
	if (r != asEXECUTION_FINISHED)
		TEST_FAILED;

	ExecuteString(engine, "gFlag = true; DoNothing()", mod);
	if( *flag != true )
		fail = false;
	r = ExecuteString(engine, "Assert(gFlag == true)", mod);
	if (r != asEXECUTION_FINISHED)
		TEST_FAILED;

	// TEST 3
	// It was reported that if( t.test_f() ) would always be true, even though the method returns false
	// The bug was that the function didn't return 0 in the upper bytes, thus the 32bit value was not 0, even though the low byte was
	engine->RegisterObjectType("tst", sizeof(tst), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);
	engine->RegisterObjectMethod("tst", "bool test_f(uint)", asMETHOD(tst, test_f), asCALL_THISCALL);
	
	r = ExecuteString(engine, "tst t; if( t.test_f(2000) == true ) Assert(false);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	
	r = ExecuteString(engine, "tst t; if( !(t.test_f(2000) == false) ) Assert(false);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	
//	engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 0);
	r = ExecuteString(engine, "tst t; if( t.test_f(2000) ) Assert(false);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
		
	engine->RegisterGlobalFunction("bool test_t()", asFUNCTION(test_t), asCALL_CDECL);
	r = ExecuteString(engine, "Assert( test_t() );");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
		
	// TEST 4
	// Return a false value as out parameter. The value must be properly interpreted, even with trash in upper bytes
	engine->RegisterGlobalFunction("void GiveFalse(bool &out)", asFUNCTION(GiveFalse), asCALL_CDECL);
	r = ExecuteString(engine, "bool f; GiveFalse(f); Assert( !f );");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "bool f; GiveFalse(f); if( f ) Assert(false);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "bool f, f2 = false; GiveFalse(f); Assert( !(f || f2) );");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;

	// TEST 5
	// The same test with global variable
	int falseValue = 0;
	if( sizeof(bool) == 1 )
		falseValue = 0x00FFFF00;
	engine->RegisterGlobalProperty("bool falseValue", &falseValue);
	r = ExecuteString(engine, "Assert( !falseValue );");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "if( falseValue ) Assert(false);");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;
	r = ExecuteString(engine, "bool f2 = false; Assert( !(falseValue || f2) );");
	if( r != asEXECUTION_FINISHED ) TEST_FAILED;

	// TEST 6
	// Test to make sure bools can be passed to member functions properly
	engine->RegisterObjectType("BoolTester", sizeof(TestBoolClass), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);
	engine->RegisterObjectMethod("BoolTester", "void TestTrue(bool)", asMETHOD(TestBoolClass, TestTrue), asCALL_THISCALL);
	engine->RegisterObjectMethod("BoolTester", "void TestFalse(bool)", asMETHOD(TestBoolClass, TestFalse), asCALL_THISCALL);	
	TestBoolClass testBool;
	r = engine->RegisterGlobalProperty("BoolTester TestBoolClass", &testBool );
	if( r < 0 ) TEST_FAILED;
	const char *script3 =
		"void TestBoolToMember()           \n"
		"{                                 \n"
		"   bool flag = true;              \n"
		"   TestBoolClass.TestTrue(flag);  \n"
		"   flag = false;                  \n"
		"   TestBoolClass.TestFalse(flag); \n"
		"}                                 \n";

	mod->AddScriptSection("script", script3, strlen(script3));
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
	}
	else
	{
		r = ExecuteString(engine, "TestBoolToMember();", mod);
		if( r != asEXECUTION_FINISHED ) TEST_FAILED;

		if( testBool.m_fail ) TEST_FAILED;
	}

	// TEST 7
	engine->RegisterGlobalFunction("void Print(const string &in)", asFUNCTION(Print), asCALL_CDECL); assert( r >= 0 );
	const char *script4 =
		"void test()                 \n"
		"{                           \n"
		"  bool low = false;         \n"
		"                            \n"
		"  if (low == false)         \n"
		"  {                         \n"
		"    Print(\"false\");       \n"
		"  }                         \n"
		"  else                      \n"
		"  {                         \n"
		"    Print(\"true\");        \n"
		"  }                         \n"
		"                            \n"
		"  if (low xor false)        \n"
		"  {                         \n"
		"    Print(\"false\");       \n"
		"  }                         \n"
		"  else                      \n"
		"  {                         \n"
		"    Print(\"true\");        \n"
		"  }                         \n"
		"                            \n"
		"  if (low == false)         \n"
		"  {                         \n"
		"    Print(\"false\");       \n"
		"  }                         \n"
		"  else                      \n"
		"  {                         \n"
		"    Print(\"true\");        \n"
		"  }                         \n"
		"}                           \n";
	mod->AddScriptSection("script", script4, strlen(script4));
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
	}
	else
	{
		r = ExecuteString(engine, "test();", mod);
		if( r != asEXECUTION_FINISHED ) TEST_FAILED;

		if( buf != "false\ntrue\nfalse\n" )
			TEST_FAILED;
	}


	// The tokenizer must not mistake '!isTrue' for '!is' + 'True' instead of '!' + 'isTrue'
	const char *script5 = 
		"class CTest                        \n"
		"{                                  \n"
		"  bool isTrue() { return true; }   \n"
		"  void func() { if( !isTrue() ) {} } \n"
		"}                                  \n";

	mod->AddScriptSection("script", script5, strlen(script5));
	r = mod->Build();
	if( r < 0 )
		TEST_FAILED;

	engine->Release();

	return fail;
}

} // namespace

