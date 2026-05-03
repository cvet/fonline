//
// Tests calling of a c-function from a script with four parameters
// of different types
//
// Test author: Fredrik Ehnbom
//

#include "utils.h"

static const char * const TESTNAME = "TestMixedArgs";

static bool testVal = false;
static bool called = false;

static int    t1 = 0;
static float  t2 = 0;
static double t3 = 0;
static char   t4 = 0;

static int i[6] = { 0 };

static void cfunction(int f1, float f2, double f3, int f4)
{
	called = true;

	t1 = f1;
	t2 = f2;
	t3 = f3;
	t4 = (char)f4;

	testVal = (f1 == 10) && (f2 == 1.92f) && (f3 == 3.88) && (f4 == 97);
}


static void cfunction_gen(asIScriptGeneric *gen)
{
	called = true;

	t1 = gen->GetArgDWord(0);
	t2 = gen->GetArgFloat(1);
	t3 = gen->GetArgDouble(2);
	t4 = (char)gen->GetArgDWord(3);

	testVal = (t1 == 10) && (t2 == 1.92f) && (t3 == 3.88) && (t4 == 97);
}

static asINT64 g1 = 0;
static float   g2 = 0;
static char    g3 = 0;
static int     g4 = 0;

static void cfunction2(asINT64 i1, float f2, char i3, int i4)
{
	called = true;

	g1 = i1;
	g2 = f2;
	g3 = i3;
	g4 = i4;

	testVal = ((i1 == I64(0x102030405)) && (f2 == 3) && (i3 == 24) && (i4 == 128));
}

static void cfunction3(int f1, double f3, float f2, int f4)
{
	called = true;

	t1 = f1;
	t2 = f2;
	t3 = f3;
	t4 = (char)f4;

	testVal = (f1 == 10) && (f2 == 1.92f) && (f3 == 3.88) && (f4 == 97);
}

static void cfunction4(int i4, asINT64 i1, float f2, char i3)
{
	called = true;

	g1 = i1;
	g2 = f2;
	g3 = i3;
	g4 = i4;

	testVal = ((i1 == I64(0x102030405)) && (f2 == 3) && (i3 == 24) && (i4 == 128));
}

// This class is implemented to guarantee that it is treated as a complex type on all platforms
class CComplex
{
public:
	// The existance of constructor, copy constructor, destructor, and assignment operator should be enough
	CComplex() { buf = new char[100]; }
	~CComplex() { if( buf ) delete[] buf; }
	CComplex(const CComplex &o) { buf = new char[100]; memcpy(buf, o.buf, 100); }
	CComplex &operator=(const CComplex &o) { if( buf ) memcpy(buf, o.buf, 100); return *this; }

	static void Construct(void *p) {new(p) CComplex();}
	static void CopyConstruct(const CComplex &o, void *p) {new(p) CComplex(o);}
	static void Destruct(CComplex *p) {p->~CComplex();}

	static void Register(asIScriptEngine *engine)
	{
		engine->RegisterObjectType("CComplex", sizeof(CComplex), asOBJ_VALUE | asOBJ_APP_CLASS_CDAK);
		engine->RegisterObjectBehaviour("CComplex", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Construct), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("CComplex", asBEHAVE_CONSTRUCT, "void f(const CComplex &in)", asFUNCTION(CopyConstruct), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("CComplex", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Destruct), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("CComplex", "CComplex &opAssign(const CComplex &in)", asMETHOD(CComplex, operator=), asCALL_THISCALL);
	}

private:
	// But if it is not, the size of class should do it
	char *buf;
	double a, b, c, d;
};

static CComplex cfunction5(int f1, double f3, float f2, int f4)
{
	called = true;

	t1 = f1;
	t2 = f2;
	t3 = f3;
	t4 = (char)f4;

	testVal = (f1 == 10) && (f2 == 1.92f) && (f3 == 3.88) && (f4 == 97);

	return CComplex();
}

static void cfunction6(int a, void* a1, int ti1, void* a2, int ti2, void* a3, int ti3, void* a4, int ti4, void* a5, int ti5)
{
	called = true;

	i[0] = a;
	i[1] = *(int*)a1;
	i[2] = *(int*)a2;
	i[3] = *(int*)a3;
	i[4] = *(int*)a4;
	i[5] = *(int*)a5;

	testVal = (ti1 == asTYPEID_INT32) && (ti2 == asTYPEID_INT32) && (ti3 == asTYPEID_INT32) && (ti4 == asTYPEID_INT32) && (ti5 == asTYPEID_INT32) &&
		(a == 42) && (*(int*)a1 == 1) && (*(int*)a2 == 2) && (*(int*)a3 == 3) && (*(int*)a4 == 4) && (*(int*)a5 == 5);
}


bool TestExecuteMixedArgs()
{
	bool fail = false;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if( strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") )
		engine->RegisterGlobalFunction("void cfunction(int, float, double, int)", asFUNCTION(cfunction_gen), asCALL_GENERIC);
	else
		engine->RegisterGlobalFunction("void cfunction(int, float, double, int)", asFUNCTION(cfunction), asCALL_CDECL);

	ExecuteString(engine, "cfunction(10, 1.92f, 3.88, 97)");

	if (!called) {
		PRINTF("\n%s: cfunction not called from script\n\n", TESTNAME);
		TEST_FAILED;
	} else if (!testVal) {
		PRINTF("\n%s: testVal is not of expected value. Got (%d, %f, %f, %c), expected (%d, %f, %f, %c)\n\n", TESTNAME, t1, t2, t3, t4, 10, 1.92f, 3.88, 97);
		TEST_FAILED;
	}

	SKIP_ON_MAX_PORT
	{
		COutStream out;
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		engine->RegisterGlobalFunction("void cfunction2(int64, float, int8, int)", asFUNCTION(cfunction2), asCALL_CDECL);
		engine->RegisterGlobalFunction("void cfunction3(int, double, float, int)", asFUNCTION(cfunction3), asCALL_CDECL);
		engine->RegisterGlobalFunction("void cfunction4(int, int64, float, int8)", asFUNCTION(cfunction4), asCALL_CDECL);

		CComplex::Register(engine);
		engine->RegisterGlobalFunction("CComplex cfunction5(int, double, float, int)", asFUNCTION(cfunction5), asCALL_CDECL);

		called = false;
		testVal = false;
		ExecuteString(engine, "cfunction2(0x102030405, 3, 24, 128)");
		if( !called )
		{
			PRINTF("%s: cfunction2 not called\n", TESTNAME);
			TEST_FAILED;
		}
		else if( !testVal )
		{
			PRINTF("%s: testVal not of expected value. Got(%lld, %g, %d, %d)\n", TESTNAME, g1, g2, g3, g4);
			TEST_FAILED;
		}

		called = false;
		testVal = false;
		ExecuteString(engine, "cfunction3(10, 3.88, 1.92f, 97)");
		if (!called) {
			PRINTF("\n%s: cfunction3 not called from script\n\n", TESTNAME);
			TEST_FAILED;
		} else if (!testVal) {
			PRINTF("\n%s: testVal is not of expected value. Got (%d, %f, %f, %c), expected (%d, %f, %f, %c)\n\n", TESTNAME, t1, t2, t3, t4, 10, 1.92f, 3.88, 97);
			TEST_FAILED;
		}

		called = false;
		testVal = false;
		ExecuteString(engine, "cfunction4(128, 0x102030405, 3, 24)");
		if( !called )
		{
			PRINTF("%s: cfunction4 not called\n", TESTNAME);
			TEST_FAILED;
		}
		else if( !testVal )
		{
			PRINTF("%s: testVal not of expected value. Got(%lld, %g, %d, %d)\n", TESTNAME, g1, g2, g3, g4);
			TEST_FAILED;
		}

		called = false;
		testVal = false;
		ExecuteString(engine, "cfunction5(10, 3.88, 1.92f, 97)");
		if( !called )
		{
			PRINTF("%s: cfunction4 not called\n", TESTNAME);
			TEST_FAILED;
		}
		else if (!testVal) {
			PRINTF("\n%s: testVal is not of expected value. Got (%d, %f, %f, %c), expected (%d, %f, %f, %c)\n\n", TESTNAME, t1, t2, t3, t4, 10, 1.92f, 3.88, 97);
			TEST_FAILED;
		}
	}

	// Test with multiple ?&in to ensure they are properly split across registers
	// https://www.gamedev.net/forums/topic/715485-arm64-and-amp-parameters/
	SKIP_ON_MAX_PORT
	{
		engine->RegisterGlobalFunction("void cfunction6(int, ?&in, ?&in, ?&in, ?&in, ?&in)", asFUNCTION(cfunction6), asCALL_CDECL);

		called = false;
		testVal = false;
		ExecuteString(engine, "cfunction6(42, 1, 2, 3, 4, 5)");
		if (!called)
		{
			PRINTF("%s: cfunction6 not called\n", TESTNAME);
			TEST_FAILED;
		}
		else if (!testVal) {
			PRINTF("\n%s: testVal is not of expected value. Got (%d, %d, %d, %d, %d, %d), expected (%d, %d, %d, %d, %d, %d)\n\n", TESTNAME, i[0], i[1], i[2], i[3], i[4], i[5], 42, 1, 2, 3, 4, 5);
			TEST_FAILED;
		}
	}

	engine->Release();
	engine = NULL;

	return fail;
}
