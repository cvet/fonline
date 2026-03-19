//
// Tests to verify that temporary variables are 
// correctly released when calling object methods.
//
// Author: Peter Marshall (2004/06/14)
//

#include "utils.h"

static const char * const TESTNAME = "TestTempVar";

struct Object1
{
    int GetInt(void);
    int m_nID;
};
struct Object2
{
    Object1 GetObject1(void);
};
int Object1::GetInt(void)
{
    return m_nID;
}
Object1 Object2::GetObject1(void)
{
    Object1 Object;
    Object.m_nID = 0xdeadbeef;
    return Object;
}
Object2 ScriptObject;

bool TestTempVar()
{
    RET_ON_MAX_PORT;

	bool fail = false;

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    CBufferedOutStream bout;

    engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
    bout.buffer = "";
    engine->RegisterObjectType("Object1", sizeof(Object1), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLINTS);
    engine->RegisterObjectMethod("Object1", "int GetInt()", asMETHOD(Object1,GetInt), asCALL_THISCALL);
    engine->RegisterObjectType("Object2", sizeof(Object2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLINTS);
    engine->RegisterObjectMethod("Object2", "Object1 GetObject1()", asMETHOD(Object2,GetObject1), asCALL_THISCALL);
    engine->RegisterGlobalProperty("Object2 GlobalObject", &ScriptObject);

	int r = ExecuteString(engine, "GlobalObject.GetObject1().GetInt();");
    if (r != asEXECUTION_FINISHED)
        TEST_FAILED;

	engine->Release();

    if (bout.buffer != "")
    {
        PRINTF("%s", bout.buffer.c_str());
        TEST_FAILED;
    }

	// Success
	return fail;
}
