//
// Test author: Andreas Jonsson
//

#include "utils.h"
#include "../../../add_on/scriptarray/scriptarray.h"

namespace TestArray
{

#define TESTNAME "TestArray"

static const char *script =
"void TestArray()                              \n"
"{                                             \n"
"    for( uint i = 0; i < 1000000; i++ )       \n"
"    {                                         \n"
"        array<int> a = {0,1,2,3,4,5,6,7,8,0}; \n"
"        for( uint p = 0; p < 10; p++ )        \n"
"            a[p]++;                           \n"
"    }                                         \n"
"}                                             \n";

void Test(double *testTime)
{
 	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	COutStream out;
	engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);
	RegisterScriptArray(engine, false);

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	mod->AddScriptSection(TESTNAME, script, strlen(script), 0);
	mod->Build();

#ifndef _DEBUG
	asIScriptContext *ctx = engine->CreateContext();
	ctx->Prepare(mod->GetFunctionByDecl("void TestArray()"));

	double time = GetSystemTimer();

	int r = ctx->Execute();

	time = GetSystemTimer() - time;

	if( r != 0 )
	{
		printf("Execution didn't terminate with asEXECUTION_FINISHED\n", TESTNAME);
		if( r == asEXECUTION_EXCEPTION )
		{
			printf("Script exception\n");
			asIScriptFunction *func = ctx->GetExceptionFunction();
			printf("Func: %s\n", func->GetName());
			printf("Line: %d\n", ctx->GetExceptionLineNumber());
			printf("Desc: %s\n", ctx->GetExceptionString());
		}
	}
	else
		*testTime = time;

	ctx->Release();
#endif
	engine->Release();
}

} // namespace



//---------------------------------------------------
// This is the same test in LUA script
//

/*

function func5()
    n = n + zfx.average( n, n )
end

function func4()
    n = n + 2 * zfx.average( n+1, n+2 )
end

function func3()
    n = n * 2.1 * n
end

function func2()
    n = n / 3.5
end

function recursion( rec )
    if rec >= 1 then
        recursion( rec - 1 )
    end

    if rec == 5 then func5()
        else if rec==4 then func4()
                else if rec==3 then func3()
                        else if rec==2 then func2()
                                else n = n * 1.5 
                                end
                        end
                end
        end
end

n = 0
i = 0

for i = 0, 249999, 0.25 do
    zfx.average( i, i + 1 ) 
    recursion( 5 )
    if n > 100 then n = 0 end
end

*/



