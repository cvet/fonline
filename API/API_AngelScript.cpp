#include "StdAfx.h"
#include "Script.h"

//#undef SCRIPT_ERROR_R0
//#define SCRIPT_ERROR_R0( error )       do { FOServer::SScriptFunc::ScriptLastError = error; Script::LogError( _FUNC_, error ); return 0; } while( 0 )

#define __API_IMPL__
#include "API_AngelScript.h"

int Script_RegisterObjectType(const char *obj, int byteSize, asDWORD flags) {
	asIScriptEngine* engine = Script::GetEngine();
	if( !engine ) {
        return asERROR;
	}
	return engine->RegisterObjectType(obj, byteSize, flags);
}

int Script_RegisterObjectProperty(const char *obj, const char *declaration, int byteOffset) {
	asIScriptEngine* engine = Script::GetEngine();
	if( !engine ) {
        return asERROR;
	}
	return engine->RegisterObjectProperty(obj, declaration, byteOffset);
}

int Script_RegisterObjectMethod(const char *obj, const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv) {
	asIScriptEngine* engine = Script::GetEngine();
	if( !engine ) {
        return asERROR;
	}
	return engine->RegisterObjectMethod(obj, declaration, funcPointer, callConv);
}

int Script_RegisterObjectBehaviour(const char *obj, asEBehaviours behaviour, const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv) {
	asIScriptEngine* engine = Script::GetEngine();
	if( !engine ) {
        return asERROR;
	}
	return engine->RegisterObjectBehaviour(obj, behaviour, declaration, funcPointer, callConv);
}

ScriptString* Script_String(const char *c_str) {
	return new ScriptString(c_str);
}
