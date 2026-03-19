#include "utils.h"

namespace TestLiteral
{

bool Test()
{
	bool fail = false;
	int r;
	COutStream out;
	CBufferedOutStream bout;

	// Test digit separators
	// Contribution by Paril
	{
		asIScriptEngine* engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		asIScriptModule* mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"int a = 1'000'000; \n"
			"int b = 0b1010'1010; \n"
			"int c = 0o12'34'56; \n"
			"int d = 0x12'34'56; \n"
			"float e = 1.5'2; \n"
			"double f = 1.5'2e3'4; \n"
			"double g = 1.5'2e+3'4; \n"
			"double h = 1.5'2e-3'4; \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		r = ExecuteString(engine, "assert(a == 1000000); \n", mod); if (r != asEXECUTION_FINISHED) TEST_FAILED;
		r = ExecuteString(engine, "assert(b == 0b10101010); \n", mod); if (r != asEXECUTION_FINISHED) TEST_FAILED;
		r = ExecuteString(engine, "assert(c == 0o123456); \n", mod); if (r != asEXECUTION_FINISHED) TEST_FAILED;
		r = ExecuteString(engine, "assert(d == 0x123456); \n", mod); if (r != asEXECUTION_FINISHED) TEST_FAILED;
		r = ExecuteString(engine, "assert(e == 1.52f); \n", mod); if (r != asEXECUTION_FINISHED) TEST_FAILED;
		r = ExecuteString(engine, "assert(f == 1.52e34); \n", mod); if (r != asEXECUTION_FINISHED) TEST_FAILED;
		r = ExecuteString(engine, "assert(g == 1.52e34); \n", mod); if (r != asEXECUTION_FINISHED) TEST_FAILED;
		r = ExecuteString(engine, "assert(h == 1.52e-34); \n", mod); if (r != asEXECUTION_FINISHED) TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		mod->AddScriptSection("test_fail",
			"int a = 1''000; \n" // 1 + '' + 000
			"int b = 0b1010''1010; \n" // 0b1010 + '' + 1010
			"int c = 0o12''34'56; \n" // 0o12 + '' + 34'56
			"int d = 0x12'34''56; \n" // 0x12'34 + '' + 56
			"float e = 1.5''2; \n" // 1.5 + '' + 2
			"double f = 1.5'2e3''4f; \n" // 1.5'2e3 + '' + 4f
			"double g = 1.5'2e+3''4; \n" // 1.5'2e+3 + '' + 4
			"double h = 1.5'2e-3''4; \n" // 1.5'2e-3 + '' + 4
			"int i = ''100; \n" // '' + 100
			"int j = 100''; \n" // 100 + ''
			"float k = 12'.00; \n" // 12 + multi-line string '.00;
			"float l = 123.'00; \n" // 123. + multi-line string '00;
			"float m = 123.'00f; \n" // 123. + multi-line string '00f;
			"double n = 1234.33'e+123; \n" // 1234.33 + multi-line string e+123;
			"double o = 1234.33e'+123; \n" // 1234.33e + multi-line string '+123;
			"double p = 1234.33e+'123; \n"); // 1234.33e+ + multi-line string '123;
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != 
			"test_fail (1, 5) : Info    : Compiling int a\n"
			"test_fail (1, 10) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (2, 5) : Info    : Compiling int b\n"
			"test_fail (2, 15) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (3, 5) : Info    : Compiling int c\n"
			"test_fail (3, 13) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (4, 5) : Info    : Compiling int d\n"
			"test_fail (4, 16) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (5, 7) : Info    : Compiling float e\n"
			"test_fail (5, 14) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (6, 8) : Info    : Compiling double f\n"
			"test_fail (6, 19) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (7, 8) : Info    : Compiling double g\n"
			"test_fail (7, 20) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (8, 8) : Info    : Compiling double h\n"
			"test_fail (8, 20) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (9, 5) : Info    : Compiling int i\n"
			"test_fail (9, 11) : Error   : Unexpected token '<integer constant>'\n"
			"test_fail (10, 5) : Info    : Compiling int j\n"
			"test_fail (10, 12) : Error   : Unexpected token '<string constant>'\n"
			"test_fail (11, 7) : Info    : Compiling float k\n"
			"test_fail (11, 13) : Error   : Unexpected token '<multiline string constant>'\n"
			"test_fail (13, 7) : Info    : Compiling float m\n"
			"test_fail (13, 15) : Error   : Unexpected token '<multiline string constant>'\n"
			"test_fail (15, 8) : Info    : Compiling double o\n"
			"test_fail (15, 20) : Error   : Unexpected token '<multiline string constant>'\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Success
	return fail;
}

} // namespace

