#include "utils.h"
#include "../../../add_on/scriptmath/scriptmathcomplex.h"
#include "scriptmath3d.h"

using namespace std;

namespace TestGetSet
{

bool Test2();

class CLevel
{
public:
	float attr;
};

CLevel g_level;
CLevel &get_Level() { return g_level; }

class CNode
{
public:
	static CNode *CNodeFactory() {return new CNode();}
	CNode() {refCount = 1; child = 0;}
	~CNode() {if( child ) child->Release();}
	void AddRef() {refCount++;}
	void Release() {if( --refCount == 0 ) delete this;}

	CNode *GetChild() {return child;}
	void  SetChild(CNode *n) {if( child ) child->Release(); child = n; n->AddRef();}

	Vector3 GetVector() const {return vector;}
	void SetVector(const Vector3 &v) {vector = v;}

	Vector3 vector;
	CNode *child;

private:
	int refCount;
};

void Log(const string& s)
{
	assert( s == "hello" );
//	PRINTF("Log: %s\n", s.c_str());
}

class TestClass 
{
    float m_offsetVars[5];
public:
    TestClass()
    {
         m_offsetVars[0] = 0.666f;
         m_offsetVars[1] = 1.666f;
         m_offsetVars[2] = 2.666f;
         m_offsetVars[3] = 3.666f;
         m_offsetVars[4] = 4.666f;
    }
    float get_OffsetVars(unsigned int index)
    {
        return m_offsetVars[index];
    }
};

void formattedPrintAS( std::string& /*format*/, void* a, int typeId_a )
{
	bool fail = false;
	if( typeId_a != asTYPEID_FLOAT ) fail = true;
	if( *(float*)a > 0.667f || *(float*)a < 0.665f ) fail = true;

	if( fail )
	{
		asIScriptContext *ctx = asGetActiveContext();
		ctx->SetException("Wrong args");
	}
}

void CharAssign(char & me, const char & other)
{
	assert(other == 'A');
	me = other;
}

std::string CharToStr(char & me)
{
	std::string result = std::string(1, me);

	assert(result == "A");

	return result;
}

void StringReplace(asIScriptGeneric *gen)
{
	string s = "foo";
	gen->SetReturnObject(&s);
}

bool Test()
{
	RET_ON_MAX_PORT

	bool fail = false;
	int r;
	CBufferedOutStream bout;
	COutStream out;
	asIScriptModule *mod;
	asIScriptEngine *engine;

	// Test complex expression with get property accessor and temporary variables
	// Reported by Phong Ba
	{
		engine = asCreateScriptEngine();
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		RegisterStdString(engine);
		engine->RegisterObjectMethod("string", "string Replace(const string, const string) const", asFUNCTION(StringReplace), asCALL_GENERIC);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE); assert(mod != NULL);
		r = mod->AddScriptSection("test",
			"class TMatch { string Value { get const { return v; } } string v; }"
			"string QuotedReplacer(TMatch &in match) \n"
			"{ \n"
			"  string source = 'c';  \n"
			"  return match.Value + source.Replace('a', 'z');  \n"
			"} \n"
			"string QuotedReplacer2(TMatch &in match) \n"
			"{ \n"
			"  string source = 'c';  \n"
			"  return match.get_Value() + source.Replace('a', 'z');  \n"
			"} \n"); assert(r >= 0);
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;
		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "TMatch t; t.v = 'bar'; string s = QuotedReplacer(t); assert(s == 'barfoo');", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		r = ExecuteString(engine, "TMatch t; t.v = 'bar'; string s = QuotedReplacer2(t); assert(s == 'barfoo');", mod);
		if (r != asEXECUTION_FINISHED)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = engine->ShutDownAndRelease();
	}

	// Test global property with conversion to string
	// Reported by Phong Ba
	{
		engine = asCreateScriptEngine();
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterStdString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		r = engine->RegisterObjectType("char", sizeof(char), asOBJ_VALUE | asOBJ_POD); assert(r >= 0);
		r = engine->RegisterObjectMethod("char", "char &opAssign(const char &in)", asFUNCTION(&CharAssign), asCALL_CDECL_OBJFIRST); assert(r >= 0);
		r = engine->RegisterObjectMethod("char", "char &opAssign(const uint16 &in)", asFUNCTION(&CharAssign), asCALL_CDECL_OBJFIRST); assert(r >= 0);
		r = engine->RegisterObjectMethod("char", "string opImplConv() const", asFUNCTION(&CharToStr), asCALL_CDECL_OBJFIRST); assert(r >= 0);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE); assert(mod != NULL);
		r = mod->AddScriptSection("prop", "char get_Prop(){char c = 0x41; return c;}"); assert(r >= 0);
		r = mod->AddScriptSection("main", "void main(){string s = string(Prop); assert(s == 'A');}"); assert(r >= 0);
		r = mod->Build();
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

		r = engine->ShutDownAndRelease();
	}

	// Test with namespace
	// http://www.gamedev.net/topic/670216-patch-for-namespace-support-in-getsetters/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		RegisterScriptMath3D(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"namespace nsTest {\n"
			"   class Foo {\n"
			"       Foo() { }\n"
			"   }\n"
			"}\n"
			"class Test {\n"
			"   nsTest::Foo@ mFoo = null;\n"
			"   nsTest::Foo@ foo {\n"
			"       get {\n"
			"           if( this.mFoo is null )\n"
			"              @this.mFoo = nsTest::Foo();\n"
			"           return @this.mFoo;\n"
			"       }\n"
			"   }\n"
			"}\n");
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

	// Test compound assignment with get/set on object returned as handle from other get/set
	// http://www.gamedev.net/topic/666081-virtual-property-compound-assignment-on-temporary-object-handle-v2300/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"class Obj { \n"
			"  uint16 prop { \n"
			"    get { return _prop; } \n"
			"    set { _prop = value; } \n"
			"  } \n"
			"  uint16 _prop = 0; \n"
			"} \n"
			"Obj @get_Objs(uint idx) { \n"
			"  return _obj; \n"
			"} \n"
			"Obj @_obj = Obj(); \n";

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "_obj.prop += 1; \n"         // direct access
			                      "get_Objs(0).prop += 1; \n"  // returned from function
			                      "Objs[0].prop += 1; \n"      // returned from indexed get accessor
			                      "assert( _obj._prop == 3 );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->ShutDownAndRelease();
	}

	// Test get/set with handle
	// http://www.gamedev.net/topic/665609-with-handle-properies-doesnt-work/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		const char *script = 
			"class SceneObject {} \n"
			"SceneObject @object { \n"
			"	get { \n"
			"		return object_; \n"
			"	} \n"
			"} \n"
			"SceneObject @object_; \n"
			"void func() { \n"
			"	if (@object != null) {}; \n"
			"} \n";

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
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

	// Test compound assignment with virtual properties
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		RegisterStdString(engine);
		RegisterScriptArray(engine, false);

		const char *script = 
			"int iprop { get { return g_ivar; } set { g_ivar = value; } } \n"
			"int g_ivar = 0; \n"
			"string sprop { get { return g_svar; } set { g_svar = value; } } \n"
			"string g_svar = 'foo'; \n";

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		// This shall be expanded to "set_iprop(get_iprop() + 2)"
		r = ExecuteString(engine, "iprop += 2; assert( g_ivar == 2 );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// This shall be expanded to "set_iprop(get_iprop() / 2)"
		r = ExecuteString(engine, "iprop /= 2; assert( g_ivar == 1 );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Using value objects
		r = ExecuteString(engine, "sprop += 'bar'; assert( g_svar == 'foobar' );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Test member virtual properties
		script = 
			"class Test { \n"
			"  int iprop { get { return m_ivar; } set { m_ivar = value; } } \n"
			"  int m_ivar = 0; \n"
			"  string sprop { get { return m_svar; } set { m_svar = value; } } \n"
			"  string m_svar = 'foo'; \n"
			"} \n";

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		// This shall be expanded to "t.set_iprop(t.get_iprop() + 2)"
		// TODO: optimize: The bytecode production is very sub-optimal
		r = ExecuteString(engine, "Test t; t.iprop += 2; assert( t.m_ivar == 2 );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// This shall be expanded to "t.set_iprop(t.get_iprop() / 2)"
		r = ExecuteString(engine, "Test t; t.iprop = 2; t.iprop /= 2; assert( t.m_ivar == 1 );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Using value objects
		r = ExecuteString(engine, "Test t; t.sprop += 'bar'; assert( t.m_svar == 'foobar' );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// When the object is retrieved as reference
		r = ExecuteString(engine, "array<Test> t(1); t[0].iprop += 2; assert( t[0].m_ivar == 2 );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// When the object is retrieved as reference
		r = ExecuteString(engine, "array<Test> t(1); t[0].sprop += 'bar'; assert( t[0].m_svar == 'foobar' );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		
		// TODO: Test with ++ too
		// This shall be expanded to "set_prop(get_prop() + 1)"
/*		r = ExecuteString(engine, "prop++; assert( g_var == 2 );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
*/

		// Compound assignments will not be allowed for properties that are members of value
		// types since it is not possible to guarantee the life time of the object 
		// TODO: It could be allowed if the object is a local variable (but wouldn't it be confusing to allow it sometimes but other times not?)
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterObjectType("type", 4, asOBJ_VALUE | asOBJ_POD);
		engine->RegisterObjectMethod("type", "int get_prop() const", asFUNCTION(0), asCALL_GENERIC);
		engine->RegisterObjectMethod("type", "void set_prop(int)", asFUNCTION(0), asCALL_GENERIC);
		bout.buffer = "";
		r = ExecuteString(engine, "type t; t.prop += 1;");
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "ExecuteString (1, 16) : Error   : Compound assignments with property accessors on value types are not supported\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// Compound assignments for indexed property accessors are not allowed
		bout.buffer = "";
		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"class T { \n"
			"  int get_idx(int i) { return 0; } \n"
			"  void set_idx(int i, int value) {} \n"
			"} \n"
			"void main() { \n"
			"  T t; t.idx[0] += 1; \n"
			"} \n");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "test (5, 1) : Info    : Compiling void main()\n"
		                   "test (6, 17) : Error   : Compound assignments with indexed property accessors are not supported\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}


	// Test memory leak with shared classes and virtual properties
	// http://www.gamedev.net/topic/644919-memory-leak-in-virtual-properties/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
 
		const char *script1 = "shared class Test { \n"
			" int mProp { \n"
			"   get { \n"
			"     return 0; \n"
			"   } \n"
			" } \n"
			"} \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script1);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		asIScriptModule *mod2 = engine->GetModule("2", asGM_ALWAYS_CREATE);
		mod2->AddScriptSection("test2", script1);
		r = mod2->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// http://www.gamedev.net/topic/639046-assert-in-as-compilercpp-temp-variables/
	{
		bout.buffer = "";
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		RegisterStdString(engine);
 
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"class RayQueryResult { \n"
			"  Drawable @get_drawable() const { return Drawable(); } \n"
			"} \n"
			"class Drawable { \n"
			"  const string &get_typename() const { return tn; } \n"
			"  string tn = 'AnimatedModel'; \n"
			"} \n"
			"void func() \n"
			"{ \n"
			"   RayQueryResult res; \n"
			"	assert( res.drawable.typename == 'AnimatedModel' ); \n"
			"} \n");

		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "func();", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();	
	}

	// Test problem reported by FDsagizi
	// http://www.gamedev.net/topic/632813-compiller-bug/
	// virtual property accessor without specifying getter nor setter
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"int some_val{ }");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "test (1, 5) : Error   : Virtual property must have at least one get or set accessor\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test problem reported by Eero Tanskanen
	// getter returning reference
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		
		r = engine->RegisterObjectType ("Container", 4, asOBJ_VALUE | asOBJ_APP_CLASS_CDA) ; assert (r > 0) ;
		r = engine->RegisterObjectType ("Container_Real", 0, asOBJ_REF | asOBJ_NOHANDLE) ; assert (r > 0) ;
		r = engine->RegisterObjectBehaviour ("Container", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(0), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
		r = engine->RegisterObjectBehaviour ("Container", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(0), asCALL_CDECL_OBJLAST); assert( r >= 0 );
		r = engine->RegisterObjectMethod ("Container", "Container_Real& get_Payload()", asFUNCTION(0), asCALL_THISCALL) ; assert (r > 0) ;
		r = engine->RegisterGlobalFunction ("Container Get_Container()", asFUNCTION(0), asCALL_CDECL) ; assert (r > 0) ;

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void Trip_Assert () { Get_Container().Payload; }" // This was causing an assert failure
			"void Dont_Trip_Assert ()	{ Get_Container().get_Payload(); }"); // This should give the exact same bytecode as the above
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// Test problem reported by virious
	// virtual property access with index and var args must work together
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		RegisterStdString(engine);
		RegisterScriptArray(engine, true);
		engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true);
		r = engine->RegisterGlobalFunction("void Log(const string&, ?&)", asFUNCTIONPR(formattedPrintAS, (std::string&, void*, int), void), asCALL_CDECL);
		r = engine->RegisterObjectType("TestClass", 0, asOBJ_REF | asOBJ_NOCOUNT); 
		r = engine->RegisterObjectMethod("TestClass", "float get_OffsetVars(uint)", asMETHOD(TestClass, get_OffsetVars), asCALL_THISCALL);

		mod = engine->GetModule("Test", asGM_ALWAYS_CREATE);

		mod->AddScriptSection("test",
			"void main( TestClass@ a ) \n"
			"{ \n"
			"    Log( 'ladder - %0.3f', a.OffsetVars[ 0 ] ); \n"
			"    Log( 'ladder - %0.3f', a.get_OffsetVars( 0 ) ); \n"
			"}\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		asIScriptFunction *func = mod->GetFunctionByDecl("void main( TestClass@ )");
		
		asIScriptContext *ctx = engine->CreateContext();
		ctx->Prepare(func);

		TestClass testClass;
		ctx->SetArgObject( 0, &testClass );

		r = ctx->Execute();
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		ctx->Release();

		engine->Release();
	}


	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
	RegisterScriptArray(engine, true);
	RegisterScriptString(engine);
	engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

	mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

	// The getter can return a handle while the setter takes a reference
	{
		const char *script = 
			"class Test \n"
			"{ \n"
			"  string @get_s() { return 'test'; } \n"
			"  void set_s(const string &in) {} \n"
			"} \n"
			"void func() \n"
			"{ \n"
			"  Test t; \n"
			"  string s = t.s; \n" 
			"  t.s = s; \n"
			"} \n";

		mod->AddScriptSection("script", script);
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
		{
			TEST_FAILED;
			PRINTF("Failed to compile the script\n");
		}

		r = ExecuteString(engine, "Test t; @t.s = 'test';", mod);
		if( r >= 0 )
		{
			TEST_FAILED;
			PRINTF("Shouldn't be allowed\n");
		}
		if( bout.buffer != "ExecuteString (1, 14) : Error   : It is not allowed to perform a handle assignment on a non-handle property\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}

	// main1 and main2 should produce the same bytecode
	const char *script1 = 
		"class Test                            \n"
		"{                                     \n"
		"  int get_prop() { return _prop; }    \n"
		"  void set_prop(int v) { _prop = v; } \n"
        "  int _prop;                          \n"
		"}                                     \n"
		"void main1()                          \n"
		"{                                     \n"
		"  Test t;                             \n"
		"  t.set_prop(42);                     \n"
		"  assert( t.get_prop() == 42 );       \n"
		"}                                     \n"
		"void main2()                          \n"
		"{                                     \n"
		"  Test t;                             \n"
		"  t.prop = 42;                        \n"
		"  assert( t.prop == 42 );             \n"
		"}                                     \n";
		
	mod->AddScriptSection("script", script1);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	r = ExecuteString(engine, "main1()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	r = ExecuteString(engine, "main2()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	// Test compound assignment with accessors (not allowed)
	const char *script2 = 
		"class Test                            \n"
		"{                                     \n"
		"  void set_prop(int v) { _prop = v; } \n"
        "  int _prop;                          \n"
		"}                                     \n"
		"void main1()                          \n"
		"{                                     \n"
		"  Test t;                             \n"
		"  t.prop += 42;                       \n"
		"}                                     \n";

	mod->AddScriptSection("script", script2);
	bout.buffer = "";
	r = mod->Build();
	if( r >= 0 )
	{
		TEST_FAILED;
	}
	if( bout.buffer != "script (6, 1) : Info    : Compiling void main1()\n"
	                   "script (9, 10) : Error   : Compound assignments with property accessors require both get and set accessors\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test get accessor with boolean operators
	const char *script3 = 
		"class Test                              \n"
		"{                                       \n"
		"  bool get_boolProp() { return true; }  \n"
		"}                                       \n"
		"void main1()                            \n"
		"{                                       \n"
		"  Test t;                               \n"
		"  if( t.boolProp ) {}                   \n"
		"  if( t.boolProp && true ) {}           \n"
		"  if( false || t.boolProp ) {}          \n"
		"  if( t.boolProp ^^ t.boolProp ) {}     \n"
		"  if( !t.boolProp ) {}                  \n"
		"  t.boolProp ? t.boolProp : t.boolProp; \n"
		"}                                       \n";

	mod->AddScriptSection("script", script3);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test get accessor with math operators
	const char *script4 = 
		"class Test                              \n"
		"{                                       \n"
		"  float get_prop() { return 1.0f; }     \n"
		"}                                       \n"
		"void main1()                            \n"
		"{                                       \n"
		"  Test t;                               \n"
		"  float f = t.prop * 1;                 \n"
		"  f = (t.prop) + 1;                     \n"
		"  10 / t.prop;                          \n"
		"  -t.prop;                              \n"
		"}                                       \n";

	mod->AddScriptSection("script", script4);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test get accessor with bitwise operators
	const char *script5 = 
		"class Test                              \n"
		"{                                       \n"
		"  uint get_prop() { return 1; }         \n"
		"}                                       \n"
		"void main1()                            \n"
		"{                                       \n"
		"  Test t;                               \n"
		"  t.prop << t.prop;                     \n"
		"  t.prop & t.prop;                      \n"
		"  ~t.prop;                              \n"
		"}                                       \n";

	mod->AddScriptSection("script", script5);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test multiple get accessors for same property. Should give error
	// Test multiple set accessors for same property. Should give error
	const char *script6 = 
		"class Test                  \n"
		"{                           \n"
		"  uint get_p() {return 0;}  \n"
		"  float get_p() {return 0;} \n"
		"  void set_s(float) {}      \n"
		"  void set_s(uint) {}       \n"
		"}                           \n"
		"void main()                 \n"
		"{                           \n"
		"  Test t;                   \n"
		"  t.p;                      \n"
		"  t.s = 0;                  \n"
		"}                           \n";
	mod->AddScriptSection("script", script6);
	bout.buffer = "";
	r = mod->Build();
	if( r >= 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "script (4, 3) : Error   : A function with the same name and parameters already exists\n"
			           "script (8, 1) : Info    : Compiling void main()\n"
	                   "script (11, 4) : Error   : Found multiple get accessors for property 'p'\n"
	                   "script (11, 4) : Info    : uint Test::get_p()\n"
	                   "script (11, 4) : Info    : float Test::get_p()\n"
	                   "script (12, 4) : Error   : Found multiple set accessors for property 's'\n"
	                   "script (12, 4) : Info    : void Test::set_s(float)\n"
	                   "script (12, 4) : Info    : void Test::set_s(uint)\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test mismatching type between get accessor and set accessor. Should give error
	const char *script7 = 
		"class Test                  \n"
		"{                           \n"
		"  uint get_p() {return 0;}  \n"
		"  void set_p(float) {}      \n"
		"}                           \n"
		"void main()                 \n"
		"{                           \n"
		"  Test t;                   \n"
		"  t.p;                      \n"
		"}                           \n";
	mod->AddScriptSection("script", script7);
	bout.buffer = "";
	r = mod->Build();
	if( r >= 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "script (6, 1) : Info    : Compiling void main()\n"
                       "script (9, 4) : Error   : The property 'p' has mismatching types for the get and set accessors\n"
                       "script (9, 4) : Info    : uint Test::get_p()\n"
                       "script (9, 4) : Info    : void Test::set_p(float)\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test only set accessor for read expression
	// Test only get accessor for write expression
	const char *script8 = 
		"class Test                  \n"
		"{                           \n"
		"  uint get_g() {return 0;}  \n"
		"  void set_s(float) {}      \n"
		"}                           \n"
		"void main()                 \n"
		"{                           \n"
		"  Test t;                   \n"
		"  t.g = 0;                  \n"
        "  t.s + 1;                  \n"
		"}                           \n";
	mod->AddScriptSection("script", script8);
	bout.buffer = "";
	r = mod->Build();
	if( r >= 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "script (6, 1) : Info    : Compiling void main()\n"
					   "script (9, 7) : Error   : The property has no set accessor\n"
					   "script (10, 7) : Error   : The property has no get accessor\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test pre and post ++. Should fail, since the expression is not a variable
	const char *script9 = 
		"class Test                  \n"
		"{                           \n"
		"  uint get_p() {return 0;}  \n"
		"  void set_p(uint) {}       \n"
		"}                           \n"
		"void main()                 \n"
		"{                           \n"
		"  Test t;                   \n"
		"  t.p++;                    \n"
        "  --t.p;                    \n"
		"}                           \n";
	mod->AddScriptSection("script", script9);
	bout.buffer = "";
	r = mod->Build();
	if( r >= 0 )
	{
		TEST_FAILED;
		PRINTF("Didn't fail to compile the script\n");
	}
	if( bout.buffer != "script (6, 1) : Info    : Compiling void main()\n"
					   "script (9, 6) : Error   : Invalid reference. Property accessors cannot be used in combined read/write operations\n"
				 	   "script (10, 3) : Error   : Invalid reference. Property accessors cannot be used in combined read/write operations\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test using property accessors from within class methods without 'this'
	// Test accessor where the object is a handle
	const char *script10 = 
		"class Test                 \n"
		"{                          \n"
		"  uint get_p() {return 0;} \n"
		"  void set_p(uint) {}      \n"
		"  void test()              \n"
		"  {                        \n"
		"    p = 0;                 \n"
		"    int a = p;             \n"
		"  }                        \n"
		"}                          \n"
		"void func()                \n"
		"{                          \n"
		"  Test @a = Test();        \n"
		"  a.p = 1;                 \n"
		"  int b = a.p;             \n"
		"}                          \n";
	mod->AddScriptSection("script", script10);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	// Test accessors with function arguments (by value, in ref, out ref)
	const char *script11 = 
		"class Test                 \n"
		"{                          \n"
		"  uint get_p() {return 0;} \n"
		"  void set_p(uint) {}      \n"
		"}                          \n"
		"void func()                \n"
		"{                          \n"
		"  Test a();                \n"
		"  byVal(a.p);              \n"
		"  inArg(a.p);              \n"
		"  outArg(a.p);             \n"
		"}                          \n"
		"void byVal(int v) {}       \n"
		"void inArg(int &in v) {}   \n"
		"void outArg(int &out v) {} \n";
	mod->AddScriptSection("script", script11);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	// When the property is an object type, then the set accessor should be 
	// used instead of the overloaded assignment operator to set the value. 
	// Properties of object properties, must allow having different 
	// types for get and set. IsEqualExceptConstAndRef should be used.
	engine->RegisterGlobalFunction("void Log(const string&inout)", asFUNCTION(Log), asCALL_CDECL);
	const char *script12 = 
		"class Test                                   \n"
		"{                                            \n"
		"  string get_s() {return _s;}                \n"
		"  void set_s(const string &in n) {_s = n;}   \n"
		"  string _s;                                 \n"
		"}                                            \n"
		"void func()                \n"
		"{                          \n"
		"  Test t;                  \n"
		"  t.s = 'hello';           \n"
		"  assert(t.s == 'hello');  \n"
		"  Log(t.s);                \n" // &inout parameter wasn't working
		"  Log(t.get_s());          \n"
		"}                          \n";
	mod->AddScriptSection("script", script12);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	// Test @t.prop = @obj; Property is a handle, and the property is assigned a new handle. Should work
	const char *script13 = 
		"class Test                                   \n"
		"{                                            \n"
		"  string@ get_s() {return _s;}               \n"
		"  void set_s(string @n) {@_s = @n;}          \n"
		"  string@ _s;                                \n"
		"}                          \n"
		"void func()                \n"
		"{                          \n"
		"  Test t;                  \n"
		"  string s = 'hello';      \n"
		"  @t.s = @s;               \n" // handle assignment
		"  assert(t.s is s);        \n"
		"  t.s = 'other';           \n" // value assignment
		"  assert(s == 'other');    \n"
		"}                          \n";
	mod->AddScriptSection("script", script13);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	// Test accessing members of an object property
	const char *script14 = 
		"class Test                                   \n"
		"{                                            \n"
		"  string get_s() {return _s;}                \n"
		"  void set_s(string n) {_s = n;}             \n"
		"  string _s;                                 \n"
		"}                            \n"
		"void func()                  \n"
		"{                            \n"
		"  Test t;                    \n"
		"  t.s = 'hello';             \n" // value assignment
		"  assert(t.s == 'hello');    \n"
		"  assert(t.s.length() == 5); \n" // this should work as length is const
		"}                            \n";
	mod->AddScriptSection("script", script14);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}

	// Test accessing a non-const method on an object through a get accessor
	// Should at least warn since the object is just a temporary one
/*
	// This warning isn't done anymore as there are times when it is valid to call a non-const method on temporary objects, for example if a stream like object is implemented
	bout.buffer = "";
	r = ExecuteString(engine, "Test t; t.s.resize(4);", mod);
	if( r < 0 )
		TEST_FAILED;
	if( (sizeof(void*) == 4 &&
		 bout.buffer != "ExecuteString (1, 13) : Warning : A non-const method is called on temporary object. Changes to the object may be lost.\n"
		                "ExecuteString (1, 13) : Info    : void string::resize(uint)\n") ||
		(sizeof(void*) == 8 &&
		 bout.buffer != "ExecuteString (1, 13) : Warning : A non-const method is called on temporary object. Changes to the object may be lost.\n"
		                "ExecuteString (1, 13) : Info    : void string::resize(uint64)\n") )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
*/

	// Test opNeg for object through get accessor
	const char *script15 = 
		"class Val { int opNeg() const { return -1; } } \n"
		"class Test                          \n"
		"{                                   \n"
		"  Val get_s() const {return Val();} \n"
		"}                                   \n"
		"void func()                  \n"
		"{                            \n"
		"  Test t;                    \n"
		"  assert( -t.s == -1 );      \n"
		"}                            \n";
	mod->AddScriptSection("script", script15);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}	

	// Test index operator for object through get accessor
	const char *script16 = 
		"class Test                          \n"
		"{                                   \n"
		"  int[] get_s() const { int[] a(1); a[0] = 42; return a; } \n"
		"}                                   \n"
		"void func()                  \n"
		"{                            \n"
		"  Test t;                    \n"
		"  assert( t.s[0] == 42 );    \n"
		"}                            \n";
	mod->AddScriptSection("script", script16);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}	

	// Test accessing normal properties for object through get accessor
	const char *script17 = 
		"class Val { int val; } \n"
		"class Test                          \n"
		"{                                   \n"
		"  Val get_s() const { Val v; v.val = 42; return v;} \n"
		"}                                   \n"
		"void func()                  \n"
		"{                            \n"
		"  Test t;                    \n"
		"  assert( t.s.val == 42 );   \n"
		"}                            \n";
	mod->AddScriptSection("script", script17);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
	{
		TEST_FAILED;
		PRINTF("Failed to compile the script\n");
	}
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
	{
		TEST_FAILED;
	}	

	// Test const/non-const get and set accessors
	const char *script18 = 
		"class Test                          \n"
		"{                                   \n"
		"  int get_p() { return 42; }        \n"
		"  int get_c() const { return 42; }  \n"
		"  void set_s(int) {}                \n"
		"}                                   \n"
		"void func()                  \n"
		"{                            \n"
		"  const Test @t = @Test();   \n"
		"  assert( t.p == 42 );       \n" // Fail
		"  assert( t.c == 42 );       \n" // Success
		"  t.s = 42;                  \n" // Fail
		"}                            \n";
	mod->AddScriptSection("script", script18);
	bout.buffer = "";
	r = mod->Build();
	if( r >= 0 )
		TEST_FAILED;
	if( bout.buffer != "script (7, 1) : Info    : Compiling void func()\n"
	                   "script (10, 15) : Error   : Non-const method call on read-only object reference\n"
	                   "script (10, 15) : Info    : int Test::get_p()\n"
					   "script (12, 7) : Error   : Non-const method call on read-only object reference\n"
	                   "script (12, 7) : Info    : void Test::set_s(int)\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	// Test accessor with property of the same name
	const char *script19 = 
		"int direction; \n"
		"void set_direction(int val) { direction = val; } \n"
		"void test_set() \n"
		"{ \n"
		"  direction = 9; \n" // calls the set_direction property accessor
		"} \n"
		"void test_get() \n"
		"{ \n"
		"  assert( direction == 9 ); \n" // fails, since there is no get accessor
		"} \n";
	mod->AddScriptSection("script", script19);
	bout.buffer = "";
	r = mod->Build();
	if( r >= 0 )
		TEST_FAILED;
	if( bout.buffer != "script (7, 1) : Info    : Compiling void test_get()\n"
	                   "script (9, 21) : Error   : The property has no get accessor\n" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	const char *script20 = 
		"class Test { \n"
		"  int direction; \n"
		"  void set_direction(int val) { direction = val; } \n"
		"} \n";
	mod->AddScriptSection("script", script20);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
		TEST_FAILED;
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "Test t; t.set_direction(3);", mod);
	if( r != asEXECUTION_FINISHED )
		TEST_FAILED;
	
	// Test accessing property of the same name on a member object
	const char *script21 =
		"class Test { \n"
		" int a; \n"
		" Test @member; \n"
		" int get_a() const { return a; } \n"
		" void set_a(int val) {a = val; if( member !is null ) member.a = val;} \n"
		"} \n";
	mod->AddScriptSection("script", script21);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
		TEST_FAILED;
	r = ExecuteString(engine, "Test t, s, u; @t.member = s; @s.member = u; t.set_a(3); assert( u.a == 3 );", mod);
	if( r != asEXECUTION_FINISHED )
		TEST_FAILED;
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	engine->GarbageCollect();

	// Test const/non-const overloads for get and set accessors
	const char *script22 = 
		"class Test                                       \n"
		"{                                                \n"
		"  int get_c() { return 41; }                     \n"
		"  int get_c() const { return 42; }               \n"
		"  void set_c(int v) { assert( v == 41 ); }       \n"
		"  void set_c(int v) const { assert( v == 42 ); } \n"
		"}                                                \n"
		"void func()                  \n"
		"{                            \n"
		"  Test @s = @Test();         \n"
		"  const Test @t = @s;        \n"
		"  assert( s.c == 41 );       \n"
		"  assert( t.c == 42 );       \n"
		"  s.c = 41;                  \n"
		"  t.c = 42;                  \n"
		"}                            \n";
	mod->AddScriptSection("script", script22);
	bout.buffer = "";
	r = mod->Build();
	if( r < 0 )
		TEST_FAILED;
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}
	r = ExecuteString(engine, "func()", mod);
	if( r != asEXECUTION_FINISHED )
		TEST_FAILED;

	// TODO: Test non-const get accessor for object type with const overloaded dual operator
	
	// TODO: Test get accessor that returns a reference (only from application func to start with)
		
	// TODO: Test property accessor with inout references. Shouldn't be allowed as the value is not a valid reference

	// TODO: Test set accessor with parameter declared as out ref (shouldn't be found)

	// TODO: What should be done with expressions like t.prop; Should the get accessor be called even though 
	//       the value is never used?

	// TODO: Accessing a class member from within the property accessor with the same name as the property 
	//       shouldn't call the accessor again. Instead it should access the real member. FindPropertyAccessor() 
	//       shouldn't find any if the function being compiler is the property accessor itself

	engine->Release();


	// Test private property accessors
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		const char *script = 
			"class TestClass \n"
			"{ \n"
			"        private int MyProp \n"
			"        { \n"
			"                get { return 1; } \n"
			"        } \n"
			"} \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		int typeId = mod->GetTypeIdByDecl("TestClass");
		asITypeInfo *type = engine->GetTypeInfoById(typeId);
		if( type->GetMethodCount() != 1 )
			TEST_FAILED;
		asIScriptFunction *func = type->GetMethodByDecl("int get_MyProp()");
		if( func == 0 || !func->IsPrivate() )
			TEST_FAILED;

		engine->Release();
	}

	// Test property accessor on temporary object handle
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterStdString(engine);

		const char *script = "class Obj { void set_opacity(float v) {} }\n"
			                 "Obj @GetObject() { return @Obj(); } \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "GetObject().opacity = 1.0f;", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test bug reported by Scarabus2
	// The bug was an incorrect reusage of temporary variable by the  
	// property get accessor when compiling a binary operator
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"class Object { \n"
			"  Object() {rot = 0;} \n"
			"  void set_rotation(float r) {rot = r;} \n"
			"  float get_rotation() const {return rot;} \n"
			"  float rot; } \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "Object obj; \n"
								  "float elapsed = 1.0f; \n"
								  "float temp = obj.rotation + elapsed * 1.0f; \n"
								  "obj.rotation = obj.rotation + elapsed * 1.0f; \n"
								  "assert( obj.rot == 1 ); \n", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test global property accessor
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"int _s = 0;  \n"
			"int get_s() { return _s; } \n"
			"void set_s(int v) { _s = v; } \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "s = 10; assert( s == 10 );", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();

		// The global property accessors are available to initialize global 
		// variables, but can possibly throw an exception if used inappropriately.
		// This test also verifies that circular references between global 
		// properties and functions is properly resolved by the GC.
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterStdString(engine);

		bout.buffer = "";

		script =
			"string _s = s; \n"
			"string get_s() { return _s; } \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r != asINIT_GLOBAL_VARS_FAILED )
			TEST_FAILED;

		if( bout.buffer != "script (1, 8) : Error   : Failed to initialize global variable '_s'\n"
		                   "script (2, 0) : Info    : Exception 'Null pointer access' in 'string get_s()'\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test property accessor for object in array
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, true);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"class MyObj { bool get_Active() { return true; } } \n";
			
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "MyObj[] a(1); if( a[0].Active == true ) { } if( a[0].get_Active() == true ) { }", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test property accessor from within class method
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		const char *script = 
			"class Vector3 \n"
			"{ \n"
			"  float x; \n"
			"  float y; \n"
			"  float z; \n"
			"}; \n"
			"class Hoge \n"
			"{ \n"
			"    const Vector3 get_pos() { return mPos; } \n"
			"    const Vector3 foo() { return pos;  } \n"
			"    const Vector3 zoo() { return get_pos(); } \n"
			"    Vector3 mPos; \n"
			"}; \n"
			"void main() \n"
			"{ \n"
			"    Hoge h; \n"
			"    Vector3 vec; \n"
			"    vec = h.zoo(); \n" // ok
			"    vec = h.foo(); \n" // runtime exception
			"} \n";
			
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test property accessor in type conversion 
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, true);

		const char *script = 
			"class sound \n"
			"{ \n"
			"  int get_pitch() { return 1; } \n"
			"  void set_pitch(int p) {} \n"
			"} \n"
			"void main() \n"
			"{ \n"
			"  sound[] sounds(1) ; \n"
			"  sounds[0].pitch = int(sounds[0].pitch)/2; \n"
			"} \n";


		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test property accessor in type conversion (2)
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		const char *script = 
			"class sound \n"
			"{ \n"
			"  const int &get_id() const { return i; } \n"
			"  int i; \n"
			"} \n"
			"void main() \n"
			"{ \n"
			"  sound s; \n"
			"  if( s.id == 1 ) \n"
			"    return; \n"
			"} \n";


		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test property accessors for opIndex
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterScriptArray(engine, false);
		RegisterScriptString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"class CTest \n"
			"{ \n"
			"  CTest() { arr.resize(5); } \n"
			"  int get_opIndex(int i) const { return arr[i]; } \n"
			"  void set_opIndex(int i, int v) { arr[i] = v; } \n"
			"  array<int> arr; \n"
			"} \n"
			"class CTest2 \n"
			"{ \n"
			"  CTest2() { arr.resize(1); } \n"
			"  CTest @get_opIndex(int i) const { return arr[i]; } \n"
			"  void set_opIndex(int i, CTest @v) { @arr[i] = v; } \n"
			"  array<CTest@> arr; \n"
			"} \n"
			"void main() \n"
			"{ \n"
			"  CTest s; \n"
			"  s[0] = 42; \n"
			"  assert( s[0] == 42 ); \n"
			"  s[1] = 24; \n"
			"  assert( s[1] == 24 ); \n"
			"  CTest2 t; \n"
			"  @t[0] = s; \n"
			"  assert( t[0] is s ); \n"
			"} \n";

		bout.buffer = "";
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Test error
		script = 
			"class CTest \n"
			"{ \n"
			"  CTest() { } \n"
			"  int get_opIndex(int i) const { return arr[i]; } \n"
			"  void set_opIndex(int i, int v) { arr[i] = v; } \n"
			"  array<int> arr; \n"
			"} \n"
			"class CTest2 \n"
			"{ \n"
			"  CTest2() { } \n"
			"  CTest get_opIndex(int i) const { return arr[i]; } \n"
			"  void set_opIndex(int i, CTest v) { @arr[i] = v; } \n"
			"  array<CTest@> arr; \n"
			"} \n"
			"void main() \n"
			"{ \n"
			"  CTest s; \n"
			"  s[0] += 42; \n" // compound assignment is not allowed
			"  CTest2 t; \n"
			"  @t[0] = s; \n" // handle assign is not allowed for non-handle property
			"} \n";
		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r > 0 )
			TEST_FAILED;
		if( bout.buffer != "script (15, 1) : Info    : Compiling void main()\n"
		                   "script (18, 8) : Error   : Compound assignments with indexed property accessors are not supported\n"
		                   "script (20, 9) : Error   : It is not allowed to perform a handle assignment on a non-handle property\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}


		engine->Release();
	}

	// Test global property accessors with index argument
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, false);
		RegisterScriptString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"  int get_arr(int i) { arr.resize(5); return arr[i]; } \n"
			"  void set_arr(int i, int v) { arr.resize(5); arr[i] = v; } \n"
			"  array<int> arr; \n"
			"void main() \n"
			"{ \n"
			"  arr[0] = 42; \n"
			"  assert( arr[0] == 42 ); \n"
			"  arr[1] = 24; \n"
			"  assert( arr[1] == 24 ); \n"
			"} \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test member property accessors with index argument
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, false);
		RegisterScriptString(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"class CTest \n"
			"{ \n"
			"  CTest() { arr.resize(5); } \n"
			"  int get_arr(int i) { return arr[i]; } \n"
			"  void set_arr(int i, int v) { arr[i] = v; } \n"
			"  private array<int> arr; \n"
			"  void test() \n"
			"  { \n"
			"    arr[0] = 42; \n"
			"    assert( arr[0] == 42 ); \n"
			"    arr[1] = 24; \n"
			"    assert( arr[1] == 24 ); \n"
			"  } \n"
			"} \n"
			"void main() \n"
			"{ \n"
			"  CTest s; \n"
			"  s.arr[0] = 42; \n"
			"  assert( s.arr[0] == 42 ); \n"
			"  s.arr[1] = 24; \n"
			"  assert( s.arr[1] == 24 ); \n"
			"  s.test(); \n"
			"} \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "main()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Test member property accessors with ++ where the set accessor takes a reference
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		bout.buffer = "";
		const char *script = 
			"class CTest \n"
			"{ \n"
			"  double _vol; \n"
			"  double get_vol() const { return _vol; } \n"
			"  void set_vol(double &in v) { _vol = v; } \n"
			"} \n"
			"CTest t; \n"
			"void main() \n"
			"{ \n"
			"  for( t.vol = 0; t.vol < 10; t.vol++ ); \n"
			"} \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "script (8, 1) : Info    : Compiling void main()\n"
		                   "script (10, 36) : Error   : Invalid reference. Property accessors cannot be used in combined read/write operations\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test get property returning reference
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		engine->RegisterObjectType("LevelType", sizeof(CLevel), asOBJ_VALUE | asOBJ_POD);
		engine->RegisterObjectProperty("LevelType", "float attr", asOFFSET(CLevel, attr));
		engine->RegisterGlobalFunction("LevelType &get_Level()", asFUNCTION(get_Level), asCALL_CDECL);
		
		r = ExecuteString(engine, "Level.attr = 0.5f;");
		if( r != asEXECUTION_FINISHED ) 
			TEST_FAILED;

		if( g_level.attr != 0.5f )
			TEST_FAILED;

		engine->Release();
	}

	// Make sure it is possible to update properties of objects returned by reference through getter
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptMath3D(engine);
		engine->RegisterObjectType("node", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("node", asBEHAVE_FACTORY, "node @f()", asFUNCTION(CNode::CNodeFactory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("node", asBEHAVE_ADDREF, "void f()", asMETHOD(CNode, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("node", asBEHAVE_RELEASE, "void f()", asMETHOD(CNode, Release), asCALL_THISCALL);
		engine->RegisterObjectMethod("node", "node @+ get_child()", asMETHOD(CNode, GetChild), asCALL_THISCALL);
		engine->RegisterObjectMethod("node", "void set_child(node @+)", asMETHOD(CNode, SetChild), asCALL_THISCALL);
		engine->RegisterObjectProperty("node", "vector3 vector", asOFFSET(CNode, vector));
		engine->RegisterObjectProperty("node", "float x", asOFFSET(CNode, vector));

		r = ExecuteString(engine, "node @a = node(); \n"
								  "@a.child = node(); \n"
								  "a.child.x = 0; \n"
								  "a.child.vector = vector3(0,0,0); \n");
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		engine->Release();
	}

	// Make sure it is not possible to update properties of objects returned by value through getter
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterScriptMath3D(engine);
		engine->RegisterObjectType("node", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("node", asBEHAVE_FACTORY, "node @f()", asFUNCTION(CNode::CNodeFactory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("node", asBEHAVE_ADDREF, "void f()", asMETHOD(CNode, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("node", asBEHAVE_RELEASE, "void f()", asMETHOD(CNode, Release), asCALL_THISCALL);
		engine->RegisterObjectMethod("node", "vector3 get_vector() const", asMETHOD(CNode, GetVector), asCALL_THISCALL);
		engine->RegisterObjectMethod("node", "void set_vector(const vector3 &in)", asMETHOD(CNode, SetVector), asCALL_THISCALL);

		r = ExecuteString(engine, "node @a = node(); \n"
								  "a.vector.x = 1; \n"              // Not OK
								  "a.vector = vector3(1,0,0); \n"); // OK

		if( r >= 0 )
			TEST_FAILED;
		if( bout.buffer != "ExecuteString (2, 1) : Error   : Expression is not an l-value\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test the alternative syntax for declaring property getters and setters
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		RegisterScriptMathComplex(engine);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"class T \n"
			"{ \n"
			// TODO: getset: Builder should provide automatic implementations
//			"  int prop1 { get; set; } \n"
//			"  int prop2 { get const final; set final; } \n"
			"  int prop3 { \n"
			"    get const final { return _prop3; } \n"
			"    set { _prop3 = value; } \n"
			"  } \n"
			"  int propInt { get { return propInt; } } int propInt; \n"
			"  double propDouble { get { return propDouble; } } double propDouble; \n"
			"  complex propComplex { get { return propComplex; } } complex propComplex; \n"
			"  T@ propT { get { return propT; } } T @propT; \n"
			"  private int _prop3; \n"
			"} \n"
			"uint globalProp { get { return 1; } set { } } \n"
			"void func() \n"
			"{ \n"
			"  T t; \n"
			"  int a = t.prop3; \n"
			"} \n");
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

	// Test problem reported by Andrew Ackermann
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"class Test {} \n"
			"Test get_test(int a) { \n"
			"    return Test(); \n"
			"} \n"
			"void f() { \n"
			"    test[0]; \n"
			"} \n");
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

	// Test problem reported by Andrew Ackermann
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		RegisterScriptMath3D(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"vector3 global; \n"
			"vector3 get_global_accessor() { return vector3(1,1,1); } \n"
			"void f() { \n"
			"   global = global_accessor; \n"
			"   assert( global.x == 1 && global.y == 1 && global.z == 1 ); \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "f()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		
		engine->Release();
	}

	// Test problem reported by Andrew Ackermann
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		bout.buffer = "";
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		RegisterScriptMath3D(engine);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"class Index { \n"
			"	uint opIndex(uint i) { \n"
			"		return i; \n"
			"	} \n"
			"}; \n"
			"class IndexProperty { \n"
			"	Index@ get_instance(uint i) { \n"
			"		return Index(); \n"
			"	} \n"
			"}; \n"
			"void f() { \n"
			"	IndexProperty test; \n"
			"    \n"
			"	//Works \n"
			"	uint a = test.get_instance(0)[0]; \n"
			"	//Errors (Can't cast Index@ to int) \n"
			"	uint x = test.instance[0][0]; \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "f()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;
		
		engine->Release();
	}

	// Test memory leak in interface
	// http://www.gamedev.net/topic/629718-memory-leak/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"interface Intf { \n"
			"	int prop { get; set; } \n"
			"}; \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		
		engine->Release();
	}

	fail = Test2() || fail;

	// Success
	return fail;
}

class CMyObj
{
public:
	CMyObj() { refCount = 1; }
	void set_Text(const string &s)
	{
		assert( s == "Hello world!" );
	}

	void AddRef() { refCount++; }
	void Release() { if( --refCount == 0 ) delete this; }

	int refCount;
};

CMyObj *MyObj_factory() 
{
	return new CMyObj;
}

bool Test2()
{
	bool fail = false;
	COutStream out;
	CBufferedOutStream bout;
	int r;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

	RegisterStdString(engine);

	engine->RegisterObjectType("CMyObj", 0, asOBJ_REF);
	engine->RegisterObjectBehaviour("CMyObj", asBEHAVE_FACTORY, "CMyObj @f()", asFUNCTION(MyObj_factory), asCALL_CDECL);
	engine->RegisterObjectBehaviour("CMyObj", asBEHAVE_ADDREF, "void f()", asMETHOD(CMyObj, AddRef), asCALL_THISCALL);
	engine->RegisterObjectBehaviour("CMyObj", asBEHAVE_RELEASE, "void f()", asMETHOD(CMyObj, Release), asCALL_THISCALL);
	engine->RegisterObjectMethod("CMyObj", "void set_Text(const string &in)", asMETHOD(CMyObj, set_Text), asCALL_THISCALL);

	const char *string = 
		"void main() { \n"
		"  CMyObj @obj = @CMyObj(); \n"
		"  obj.Text = 'Hello world!'; \n"
		"} \n";
	asIScriptModule *mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
	mod->AddScriptSection("string", string);
	r = mod->Build();
	if( r < 0 ) 
		TEST_FAILED;

	r = ExecuteString(engine, "main()", mod);
	if( r != asEXECUTION_FINISHED )
		TEST_FAILED;

	// Test disabling property accessors
	engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
	engine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 0);
	r = ExecuteString(engine, "CMyObj o; o.Text = 'hello';");
	if( r >= 0 )
		TEST_FAILED;
	if( bout.buffer != "ExecuteString (1, 12) : Error   : 'Text' is not a member of 'CMyObj'\n" )
	{
		TEST_FAILED;
		PRINTF("%s", bout.buffer.c_str());
	}

	// Test disabling property accessors in script
	bout.buffer = "";
	engine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE, 1);
	mod->AddScriptSection("test",
		"class CTest { \n"
		"  void get_prop() {} \n"
		"  void set_prop(int v) { prop = v; } \n"
		"  int prop; \n"
		"} \n"
		"void func() \n"
		"{ \n"
		"  CTest t; t.prop = 1; \n"
		"} \n");
	r = mod->Build();
	if( r < 0 )
		TEST_FAILED;
	if( bout.buffer != "" )
	{
		PRINTF("%s", bout.buffer.c_str());
		TEST_FAILED;
	}

	engine->Release();

	return fail;
}


} // namespace

