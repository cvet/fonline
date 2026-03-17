//
// This test was designed to test the asOBJ_CLASS flag with THISCALL
//
// Author: Andreas Jonsson
//

#include "utils.h"

namespace TestThiscallAsGlobal
{

static const char * const TESTNAME = "TestThiscallAsGlobal";

class Class1
{
public:
	void TestMe(asDWORD newValue) { a = newValue; }
	asDWORD a;
};

class Base
{
public:
	virtual void Print() { str = "Called from Base"; }

	std::string str;
};

class Derived : public Base
{
public:
	virtual void Print() { str = "Called from Derived"; }
};

static Class1 c1;

class PointF
{
public:
	PointF() {x = 3.14f; y = 42.0f;};
	PointF( const PointF &other) { x = other.x; y = other.y; };

	static void ConstructorDefaultPointF(PointF *m) { new(m) PointF(); };
	static void DestructorPointF(PointF *) {  };

	float x, y;
};

class Renderer
{
public:
	Renderer() { allOK = false; }
	void DrawSprite(int, int, PointF p) { if( p.x == 3.14f && p.y == 42.0f ) allOK = true; }
	bool allOK; 
};

bool Test()
{
	RET_ON_MAX_PORT

	bool fail = false;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

	engine->RegisterGlobalFunction("void TestMe(uint val)", asMETHOD(Class1, TestMe), asCALL_THISCALL_ASGLOBAL, &c1);

	c1.a = 0;


	int r = ExecuteString(engine, "TestMe(0xDEADC0DE);");
	if( r < 0 )
	{
		PRINTF("%s: ExecuteString() failed %d\n", TESTNAME, r);
		TEST_FAILED;
	}

	if( c1.a != 0xDEADC0DE )
	{
		PRINTF("Class member wasn't updated correctly\n");
		TEST_FAILED;
	}

	// Register and call a derived method
	Base *obj = new Derived();
 	engine->RegisterGlobalFunction("void Print()", asMETHOD(Base, Print), asCALL_THISCALL_ASGLOBAL, obj);

	r = ExecuteString(engine, "Print()");
	if( r < 0 )
		TEST_FAILED;

	if( obj->str != "Called from Derived" )
		TEST_FAILED;

	delete obj;


	// It must not be possible to register without the object pointer
	CBufferedOutStream bout;
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
	r = engine->RegisterGlobalFunction("void Fail()", asMETHOD(Class1, TestMe), asCALL_THISCALL_ASGLOBAL, 0);
	if( r != asINVALID_ARG )
		TEST_FAILED;
	if( bout.buffer != " (0, 0) : Error   : Failed in call to function 'RegisterGlobalFunction' with 'void Fail()' (Code: -5)\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
	}
	engine->Release();

	// Test proper clean-up args passed by value
	// http://www.gamedev.net/topic/670017-weird-crash-in-userfree-on-android/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		r = engine->RegisterObjectType("PointF", sizeof(PointF), asOBJ_VALUE|asOBJ_POD|asOBJ_APP_CLASS_CK); assert( r >= 0);
		r = engine->RegisterObjectBehaviour( "PointF", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(PointF::ConstructorDefaultPointF), asCALL_CDECL_OBJLAST); assert( r >= 0);
		r = engine->RegisterObjectBehaviour( "PointF", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(PointF::DestructorPointF), asCALL_CDECL_OBJLAST); assert( r >= 0);

		Renderer render;
		r = engine->RegisterGlobalFunction( "void DrawSprite( int,int,PointF)", asMETHOD(Renderer, DrawSprite), asCALL_THISCALL_ASGLOBAL, &render); assert(r >= 0);
	
		r = ExecuteString(engine, "PointF p; DrawSprite(1,2,p);");
		if( r < 0 )
			TEST_FAILED;

		if( !render.allOK )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
		}

		engine->Release();
	}

	return fail;
}

} // namespace
