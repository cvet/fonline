//
// Test to see if engine can be created.
//
// Author: Fredrik Ehnbom
//

#include "utils.h"

bool TestCreateEngine()
{
	bool fail = false;
	asIScriptEngine *engine = asCreateScriptEngine();
	if( engine == 0 )
	{
		// Failure
		PRINTF("TestCreateEngine: asCreateScriptEngine() failed\n");
		TEST_FAILED;
	}
	else
	{
		// Set a message callback
		CBufferedOutStream bout;
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		// Although it's not recommended that two engines are created, it shouldn't be a problem
		asIScriptEngine *engine2 = asCreateScriptEngine();
		if( engine2 == 0 )
		{
			// Failure
			PRINTF("TestCreateEngine: asCreateScriptEngine() failed for 2nd engine\n");
			TEST_FAILED;
		}
		else
		{
			// Attempt to reuse the message callback from the first engine
			asSFuncPtr msgCallback;
			void* obj;
			asDWORD callConv;
			engine->GetMessageCallback(&msgCallback, &obj, &callConv);
			engine2->SetMessageCallback(msgCallback, obj, callConv);

			engine2->WriteMessage("test", 0, 0, asMSGTYPE_INFORMATION, "Hello from engine2");

			engine2->ShutDownAndRelease();
		}

		engine->ShutDownAndRelease();

		if (bout.buffer != "test (0, 0) : Info    : Hello from engine2\n")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
	}
	
	return fail;
}
