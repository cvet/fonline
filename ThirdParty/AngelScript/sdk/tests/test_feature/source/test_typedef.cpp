#include "utils.h"

namespace TestTypedef
{

static const char * const TESTNAME = "TestTypedef";
#define TESTMODULE		"TestTypedef"
#define TESTTYPE		"TestType1"

//	Script for testing attributes
static const char *const script =
"typedef int8  TestType1;								\n"
"typedef int16 TestType2;								\n"
"typedef int32 TestType3;								\n"
"typedef int64 TestType4;								\n"
"typedef float real;                                    \n"
"TestType1 TEST1 = 1;									\n"
"TestType2 TEST2 = 2;									\n"
"TestType3 TEST3 = 4;									\n"
"TestType4 TEST4 = 8;									\n"
"														\n"
"TestType4 func(TestType1 a) {return a;}                \n"
"														\n"
"void Test()											\n"
"{														\n"
"	TestType1	val1;									\n"
"	TestType2	val2;									\n"
"	TestType3	val3;									\n"
"														\n"
"														\n"
"	val1 = TEST1;										\n"
"	if(val1 == TEST1)	{	val2 = 0;	}				\n"
"	if(TEST2 <= TEST3)	{	val2 = 0;	}				\n"
"	if(TEST2 < TEST3)	{	val2 = 0;	}				\n"
"	if(TEST2 > TEST3)	{	val2 = 0;	}				\n"
"	if(TEST2 != TEST3)	{	val2 = 0;	}				\n"
"	if(TEST2 <= TEST3)	{	val2 = 0;	}				\n"
"	if(TEST2 >= TEST3)	{	val2 = 0;	}				\n"
"	val1 = val2;										\n"
"														\n"
"}														\n";


//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

static int testTypedef(CBytecodeStream &codeStream, bool save)
{
	asIScriptEngine		*engine;
	COutStream			out;
	int					r;

 	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if(NULL == engine) 
	{
		return -1;
	}

	r = engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	if( r >= 0 ) r = engine->RegisterTypedef("float32", "float");

	// Test working example
	if(true == save)
	{
		asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		if(r >= 0) 
		{
			r = mod->AddScriptSection(NULL, script);
		}

		if(r >= 0) 
		{
			r = mod->Build();
		}

		r = mod->SaveByteCode(&codeStream);
	}
	else 
	{
		asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		r = mod->LoadByteCode(&codeStream);
		if(r >= 0) 
		{
			mod->BindAllImportedFunctions();
		}
	}

	engine->Release();

	// Success
	return r;
}


bool Test()
{
	int r;
	bool fail = false;

	// Test typedefs in switch cases
	// Reported by Ivan K
	{
		asIScriptEngine* engine;
		CBufferedOutStream bout;

		engine = asCreateScriptEngine();
		r = engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		r = engine->RegisterTypedef("float32", "float");
		//float A;
		//r = engine->RegisterGlobalProperty("const float32 A", (void*)&A);
		r = engine->RegisterTypedef("regular", "int");
		//int B;
		//r = engine->RegisterGlobalProperty("const regular B", (void*)&B);
		r = engine->RegisterTypedef("small", "uint8");
		//unsigned char C;
		//r = engine->RegisterGlobalProperty("const small C", (void*)&C);

		asIScriptModule* mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"const regular B = 0; \n"
			"const small C = 0; \n"
			// typedefs for int should work just as ordinary int
			"void func1() { regular i; switch( i ) { case B: } } \n" 
			"void func2() { small i; switch( i ) { case C: } } \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		mod->AddScriptSection("test",
			"const float32 A = 0; \n"
			// typedefs for float should fail just as for ordinary float
			"void func3() { float32 f; switch( f ) { case A: } } \n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;

		if (bout.buffer != "test (2, 1) : Info    : Compiling void func3()\n"
						   "test (2, 35) : Error   : Switch expressions must be integral numbers\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test saving and loading bytecode with typedefs
	{
		CBytecodeStream	stream(__FILE__"1");

		r = testTypedef(stream, true);
		if (r >= 0)
		{
			r = testTypedef(stream, false);
		}
		
		if (r < 0)
			TEST_FAILED;
	}

	return fail;
}

} // namespace

