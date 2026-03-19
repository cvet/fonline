#include "utils.h"
#include "../../../add_on/scriptarray/scriptarray.h"
#include "../../../add_on/serializer/serializer.h"
#include "../../../add_on/scriptmath/scriptmathcomplex.h"
#include <iostream>

namespace Test_Addon_Serializer
{

struct CStringType : public CUserType
{
	void *AllocateUnitializedMemory(CSerializedValue* /*value*/)
	{
		// This must not be done for strings
		assert(false);
		return 0;
	}
	void Store(CSerializedValue *val, void *ptr)
	{
		val->SetUserData(new std::string(*(std::string*)ptr));
	}
	void Restore(CSerializedValue *val, void *ptr)
	{
		std::string *buffer = (std::string*)val->GetUserData();
		*(std::string*)ptr = *buffer;
	}
	void CleanupUserData(CSerializedValue *val)
	{
		std::string *buffer = (std::string*)val->GetUserData();
		delete buffer;
	}
};

struct CArrayType : public CUserType
{
	void* AllocateUnitializedMemory(CSerializedValue* value)
	{
		CScriptArray* arr = CScriptArray::Create(value->GetType());
		return arr;
	}
	void Store(CSerializedValue *val, void *ptr)
	{
		CScriptArray *arr = (CScriptArray*)ptr;

		for( unsigned int i = 0; i < arr->GetSize(); i++ )
			val->m_children.push_back(new CSerializedValue(val ,"", "", arr->At(i), arr->GetElementTypeId()));
	}
	void Restore(CSerializedValue *val, void *ptr)
	{
		CScriptArray *arr = (CScriptArray*)ptr;
		arr->Resize(asUINT(val->m_children.size()));

		for( size_t i = 0; i < val->m_children.size(); ++i )
			val->m_children[i]->Restore(arr->At(asUINT(i)), arr->GetElementTypeId());
	}
};

class CDummy
{
public:
	CDummy() : refCount(1) {};
	void AddRef() { refCount++; }
	void Release() { refCount--; }
	int refCount;
};

struct CppObj
{
	CppObj(asIScriptEngine* /* engine*/) : refCount(1), gcFlag(0) // FIX: Need to initialize the members
	{

	}
	CppObj(CppObj& other) : refCount(1), gcFlag(0), myString(other.myString) // FIX: Need appropriate copy constructor
	{
	}

	void AddRef()
	{
		// Increase counter and clear flag set by GC
		gcFlag = false;
		asAtomicInc(refCount);
	}

	void Release()
	{
		// Decrease the ref counter
		gcFlag = false;
		if (asAtomicDec(refCount) == 0)
		{
			// Delete this object as no more references to it exists
			delete this;
		}
	}
	mutable asBYTE gcFlag : 1;
	mutable int refCount;


	std::string myString;
	std::string getString() { return myString; }
	void setString(std::string & in) { myString = in; }
};

static void CppObjFactory(asIScriptGeneric* gen)
{
	asIScriptEngine* engine = gen->GetEngine();
	*(CppObj**)gen->GetAddressOfReturnLocation() = new CppObj(engine);
}

void RegisterCppObjType(asIScriptEngine* engine)
{
	int r;
	r = engine->RegisterObjectType("CppObj", sizeof(CppObj), asOBJ_REF);

	r = engine->RegisterObjectBehaviour("CppObj", asBEHAVE_FACTORY, "CppObj@ f()", asFUNCTION(CppObjFactory), asCALL_GENERIC);
	r = engine->RegisterObjectMethod("CppObj", "string str()", asMETHODPR(CppObj, CppObj::getString, (void), std::string), asCALL_THISCALL);
	r = engine->RegisterObjectMethod("CppObj", "void setStr(string& in)", asMETHODPR(CppObj, CppObj::setString, (std::string&), void), asCALL_THISCALL);
	r = engine->RegisterObjectBehaviour("CppObj", asBEHAVE_ADDREF, "void f()", asMETHOD(CppObj, AddRef), asCALL_THISCALL);
	r = engine->RegisterObjectBehaviour("CppObj", asBEHAVE_RELEASE, "void f()", asMETHOD(CppObj, Release), asCALL_THISCALL);
}

// This serializes the std::string type
struct CppObjType : public CUserType
{
	void* AllocateUnitializedMemory(CSerializedValue* val) // FIX: Needed a way to create new instances
	{
		return new CppObj(val->GetType()->GetEngine());
	}
	void Store(CSerializedValue* val, void* ptr)
	{
		val->SetUserData(new CppObj(*(CppObj*)ptr));
	}
	void Restore(CSerializedValue* val, void* ptr)
	{
		CppObj* buffer = (CppObj*)val->GetUserData();
		*(CppObj*)ptr = *buffer;
	}
	void CleanupUserData(CSerializedValue* val) // FIX: Since data is stored in user data it needs to be cleaned up
	{
		CppObj* obj = (CppObj*)val->GetUserData();
		obj->Release();
	}
};


void printString(const std::string &msg) // FIX: The function was declared to take the string by value, but registered to take by ref
{
//	std::cout << "Script says: " << msg << "\n";
};

int buildModule(asIScriptEngine* engine, asIScriptContext* /* ctx */)
{
	asIScriptModule *mod = engine->GetModule("TestModule", asGM_ALWAYS_CREATE);
	mod->AddScriptSection("test",
		"class Main \n"
		"{ \n"
		"	private string m_string; \n"
		"	private CppObj@ m_obj; \n"
		"	Main() \n"
		"	{ \n"
		"		m_string = 'Hello, '; \n"
		
		"		CppObj aux; \n"
		"		aux.setStr('world!'); \n"

		"		@m_obj = @aux; \n"
		"	} \n"
		"	void Update() \n"
		"	{ \n"
		"		print(m_string); \n"
		"		print(m_obj.str()); \n"
		"	} \n"
		"}; \n");
	int r = mod->Build();

	return r;
}

int buildModule2(asIScriptEngine* engine, asIScriptContext* /* ctx */)
{
	asIScriptModule* mod = engine->GetModule("TestModule", asGM_ALWAYS_CREATE);
	mod->AddScriptSection("test",
		"class Main \n"
		"{ \n"
		"	private string m_string; \n"
		"	private string[]@ m_array; \n"
		"	Main() \n"
		"	{ \n"
		"		print('CTOR'); \n"
		"		m_string = 'Hello, world!'; \n"
		"		string aux = 'Goodbye, cruel world'; \n"
		"		string[] somearray; \n"
		"		somearray.insertLast(aux); \n"
		"		@m_array = @somearray; \n"
		"		print(m_array[0]); \n"

		"		print('CTOR END'); \n"
		"	} \n"
		"	void Update() \n"
		"	{ \n"
		"		print(m_string); \n"
		"		print(m_array[0]); \n"
		"	} \n"
		"}; \n");
	int r = mod->Build();

	return r;
}

asIScriptObject* instantializeClass(bool initMembers, asIScriptEngine* engine, asIScriptContext* ctx)
{
	asIScriptModule* module = engine->GetModule("TestModule");
	asITypeInfo* type = module->GetTypeInfoByDecl("Main");

	asIScriptObject* mainObj;
	if (initMembers)
	{
		asIScriptFunction* factory = type->GetFactoryByDecl("Main @Main()");
		std::string test = "Hello ";

		ctx->SetArgAddress(0, &test);
		ctx->Prepare(factory);
		ctx->Execute();

		mainObj = *(asIScriptObject**)ctx->GetAddressOfReturnValue();
		mainObj->AddRef();
	}
	else
	{
		mainObj = reinterpret_cast<asIScriptObject*>(engine->CreateUninitializedScriptObject(type));
		// FIX: Don't call AddRef for this
	}

	return mainObj;
}

bool Test()
{
	bool fail = false;
	int r;
	COutStream out;
	CBufferedOutStream bout;
 	asIScriptEngine *engine;
	asIScriptModule *mod;

	// Test serializing a script object with a handle holding the only reference to a script array
	// https://www.gamedev.net/forums/topic/712813-serializing-an-object-handle-holding-the-only-reference-to-an-object/
	SKIP_ON_MAX_PORT
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		RegisterStdString(engine);
		RegisterScriptArray(engine, true);
		r = engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(printString), asCALL_CDECL); assert(r >= 0);

		asIScriptContext* ctx = engine->CreateContext();

		r = buildModule2(engine, ctx);
		if (r < 0)
			TEST_FAILED;

		asIScriptObject* mainObj = instantializeClass(true, engine, ctx);

		///////////////////////////////////Simulate app running for some time
		for (int i = 0; i < 100; i++)
		{
			asIScriptModule* module = engine->GetModule("TestModule");
			asITypeInfo* type = module->GetTypeInfoByDecl("Main");

			asIScriptFunction* func = type->GetMethodByDecl("void Update()");

			ctx->Prepare(func);
			ctx->SetObject(mainObj);
			r = ctx->Execute();
			if (r != asEXECUTION_FINISHED)
				TEST_FAILED;
		}

		///////////////////////////////////Hot reload part
		asIScriptModule* module = engine->GetModule("TestModule");
		CSerializer serializer;

		serializer.AddUserType(new CStringType(), "string"); // FIX: Need to know how to serialize string type too
		serializer.AddUserType(new CArrayType(), "array");

		serializer.AddExtraObjectToStore(mainObj);
		auto cpy = mainObj;
		serializer.Store(module);

		mainObj->Release();
		module->Discard();
		r = buildModule2(engine, ctx);
		if (r < 0)
			TEST_FAILED;

		// mainObj = instantializeClass(false, engine, ctx); // FIX: Don't do this, let the serializer do it

		module = engine->GetModule("TestModule");
		serializer.Restore(module);
		mainObj = static_cast<asIScriptObject*>(serializer.GetPointerToRestoredObject(cpy));
		mainObj->AddRef(); // FIX: Need to add the ref, as the serializer will release its reference when destroyed
//		std::cout << "Post restore\n";

		///////////////////////////////////Simulate app running for some time
		for (int i = 0; i < 4; i++)
		{
			module = engine->GetModule("TestModule");
			asITypeInfo* type = module->GetTypeInfoByDecl("Main");

			asIScriptFunction* func = type->GetMethodByDecl("void Update()");

			ctx->Prepare(func);
			ctx->SetObject(mainObj);
			r = ctx->Execute();
			if (r != asEXECUTION_FINISHED)
				TEST_FAILED;
		}

		mainObj->Release(); // FIX: Release the main object when done
		ctx->Release();
		serializer.Clear(); // FIX: Release references held by serializer

		engine->ShutDownAndRelease();
	}

	// Test serializing a script object with a handle holding the only reference to a registered type
	// https://www.gamedev.net/forums/topic/712813-serializing-an-object-handle-holding-the-only-reference-to-an-object/
	SKIP_ON_MAX_PORT
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		RegisterStdString(engine);
		RegisterScriptArray(engine, true);
		RegisterCppObjType(engine);
		r = engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(printString), asCALL_CDECL);

		asIScriptContext* ctx = engine->CreateContext();

		r = buildModule(engine, ctx);
		if (r < 0)
			TEST_FAILED;

		asIScriptObject* mainObj = instantializeClass(true, engine, ctx);

		///////////////////////////////////Simulate app running for some time
		for (int i = 0; i < 100; i++)
		{
			asIScriptModule* module = engine->GetModule("TestModule");
			asITypeInfo* type = module->GetTypeInfoByDecl("Main");

			asIScriptFunction* func = type->GetMethodByDecl("void Update()");

			ctx->Prepare(func);
			ctx->SetObject(mainObj);
			r = ctx->Execute();
			if (r != asEXECUTION_FINISHED)
				TEST_FAILED;
		}

		///////////////////////////////////Hot reload part
		asIScriptModule* module = engine->GetModule("TestModule");
		CSerializer serializer;

		serializer.AddUserType(new CArrayType(), "array");
		serializer.AddUserType(new CStringType(), "string");
		serializer.AddUserType(new CppObjType(), "CppObj");

		serializer.AddExtraObjectToStore(mainObj);
		auto cpy = mainObj;
		serializer.Store(module);

		mainObj->Release();
		module->Discard();
		r = buildModule(engine, ctx);
		if (r < 0)
			TEST_FAILED;

		// mainObj = instantializeClass(false, engine, ctx); // FIX: Don't do this, let the serializer do it

		module = engine->GetModule("TestModule");
		serializer.Restore(module);
		mainObj = static_cast<asIScriptObject*>(serializer.GetPointerToRestoredObject(cpy));
		mainObj->AddRef(); // FIX: Need to add the ref, as the serializer will release its reference when destroyed
//		std::cout << "Post restore\n";

		///////////////////////////////////Simulate app running for some time
		for (int i = 0; i < 4; i++)
		{
			module = engine->GetModule("TestModule");
			asITypeInfo* type = module->GetTypeInfoByDecl("Main");

			asIScriptFunction* func = type->GetMethodByDecl("void Update()");

			ctx->Prepare(func);
			ctx->SetObject(mainObj);
			r = ctx->Execute();
			if (r != asEXECUTION_FINISHED)
				TEST_FAILED;
		}

		mainObj->Release(); // FIX: Release the main object when done
		ctx->Release();
		serializer.Clear(); // FIX: Release references held by serializer
		engine->ShutDownAndRelease();
	}

	SKIP_ON_MAX_PORT
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		RegisterScriptString(engine);
		RegisterScriptArray(engine, false);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		// Register an application type that cannot be created from script. The serializer
		// will then keep the same pointer, while properly maintaining the refcount
		engine->RegisterObjectType("dummy", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("dummy", asBEHAVE_ADDREF, "void f()", asMETHOD(CDummy,AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("dummy", asBEHAVE_RELEASE, "void f()", asMETHOD(CDummy,Release), asCALL_THISCALL);

		const char *script = 
			"float f; \n"
			"string str; \n"
			"array<int> arr; \n"
			"class CTest \n"
			"{ \n"
			"  int a; \n"
			"  string str; \n"
			"  dummy @d; \n"
			"} \n"
			"CTest @t; \n"
			"CTest a; \n"
			"CTest @b; \n"
			"CTest @t2 = @a; \n"
			"CTest @n = @a; \n"
			"array<CTest> arrOfTest; \n";

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		// Populate the handle to the external object
		CDummy dummy;
		asIScriptObject *obj = (asIScriptObject*)mod->GetAddressOfGlobalVar(mod->GetGlobalVarIndexByName("a"));
		*(CDummy**)obj->GetAddressOfProperty(2) = &dummy;
		dummy.AddRef();
		
		r = ExecuteString(engine, "f = 3.14f; \n"
			                      "str = 'test'; \n"
								  "arr.resize(3); arr[0] = 1; arr[1] = 2; arr[2] = 3; \n"
								  "a.a = 42; \n"
								  "a.str = 'hello'; \n"
								  "@b = @a; \n"
								  "@t = CTest(); \n"
								  "t.a = 24; \n"
								  "t.str = 'olleh'; \n"
								  "@t2 = t; \n"
								  "@n = null; \n"
								  "arrOfTest.insertLast(CTest());\n"
								  "arrOfTest[0].str = 'blah';\n", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// Add an extra object for serialization
		asIScriptObject *scriptObj = reinterpret_cast<asIScriptObject*>(engine->CreateScriptObject(mod->GetTypeInfoByName("CTest")));
		((std::string*)(scriptObj->GetAddressOfProperty(1)))->assign("external object");

		// Reload the script while keeping the object states
		{
			CSerializer modStore;
			modStore.AddUserType(new CStringType(), "string");
			modStore.AddUserType(new CArrayType(), "array");

			modStore.AddExtraObjectToStore(scriptObj);

			r = modStore.Store(mod);
			if( r < 0 )
				TEST_FAILED;

			engine->DiscardModule(0);

			mod = engine->GetModule("2", asGM_ALWAYS_CREATE);
			mod->AddScriptSection("script", script);
			r = mod->Build();
			if( r < 0 )
				TEST_FAILED;

			r = modStore.Restore(mod);
			if( r < 0 )
				TEST_FAILED;

			// Restore the extra object
			asIScriptObject *obj2 = (asIScriptObject*)modStore.GetPointerToRestoredObject(scriptObj);
			scriptObj->Release();
			scriptObj = obj2;
			scriptObj->AddRef();
		}

		r = ExecuteString(engine, "assert(f == 3.14f); \n"
		                          "assert(str == 'test'); \n"
								  "assert(arr.length() == 3 && arr[0] == 1 && arr[1] == 2 && arr[2] == 3); \n"
								  "assert(a.a == 42); \n"
								  "assert(a.str == 'hello'); \n"
								  "assert(b is a); \n"
								  "assert(t !is null); \n"
								  "assert(t.a == 24); \n"
								  "assert(t.str == 'olleh'); \n"
								  "assert(t is t2); \n"
								  "assert(n is null); \n"
								  "assert(arrOfTest.length() == 1 && arrOfTest[0].str == 'blah'); \n"
								  "assert(a.d !is null); \n", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// The new object has the same content
		if( *(std::string*)scriptObj->GetAddressOfProperty(1) != "external object" )
			TEST_FAILED;
		scriptObj->Release();

		// After the serializer has been destroyed the refCount for the external handle must be the same as before
		if( dummy.refCount != 2 )
			TEST_FAILED;
		
		engine->Release();
	}

	// Report proper error when missing user type for non-POD type
	SKIP_ON_MAX_PORT
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream,Callback), &bout, asCALL_THISCALL);
		RegisterStdString(engine);
		RegisterScriptMathComplex(engine);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		const char* script =
			"string str; \n"     // non-POD: error
			"complex cmplx; \n"; // POD:     no error

		mod->AddScriptSection( 0, script );
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		bout.buffer = "";
		CSerializer modStore;

		r = modStore.Store(mod);
		if( r < 0 )
			TEST_FAILED;

		r = modStore.Restore(mod);
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != " (0, 0) : Error   : Cannot restore type 'string'\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Make sure it is possible to restore objects, where the constructor itself is changing other objects
	// http://www.gamedev.net/topic/604890-dynamic-reloading-script/page__st__20
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
		RegisterScriptString(engine);
		RegisterScriptArray(engine, false);

		mod = engine->GetModule(0, asGM_ALWAYS_CREATE);

		const char* script =
			"array<TestScript@> arr; \n"
			"class TestScript \n"
			"{ \n"
			"         TestScript()   \n"
			"         { \n"
			"                  arr.insertLast( this ); \n"
			"         }       \n"
			"} \n"
			"void startGame()          \n"
			"{ \n"
			"         TestScript @t = TestScript(); \n"
			"}  \n";

		mod->AddScriptSection( 0, script );
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = ExecuteString(engine, "startGame()", mod);

		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		CSerializer modStore;
		modStore.AddUserType(new CStringType(), "string");
		modStore.AddUserType(new CArrayType(), "array");

		r = modStore.Store(mod);
		if( r < 0 )
			TEST_FAILED;

		engine->DiscardModule(0);

		mod = engine->GetModule("2", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("script", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		r = modStore.Restore(mod);
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	return fail;
}

} // namespace

