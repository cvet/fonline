#pragma warning(disable:4042)

#include "angelscript.h"

#include "data.h"

extern "C" __declspec (dllexport) int Bind(asIScriptEngine* engine)
{
	int result=0;

#define BIND_ERROR result++
#define WriteLog /##/
#include "bind.h"

	return result;
}