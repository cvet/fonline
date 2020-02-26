#include "StdAfx.h"
#include "Server.h"

#undef SCRIPT_ERROR_R0
#define SCRIPT_ERROR_R0( error )       do { FOServer::SScriptFunc::ScriptLastError = error; Script::LogError( _FUNC_, error ); return 0; } while( 0 )

#define __API_IMPL__
#include "API_Server.h"

bool Cl_RunClientScript( Critter* cl, const char* func_name, int p0, int p1, int p2, const char* p3, const uint* p4, size_t p4_size )
{
	UIntVec dw;
	if( p4 && p4_size)
		dw.assign(p4, p4 + p4_size);
	Client* cl_ = (Client*) cl;
	cl_->Send_RunClientScript( func_name, p0, p1, p2, p3, dw );
	return true;
}

Critter* Global_GetCritter(uint crid)
{
	return FOServer::SScriptFunc::Global_GetCritter(crid);
}

ScriptString* Global_GetMsgStr(size_t lang, size_t textMsg, uint strNum)
{
	if( lang >= FOServer::LangPacks.size() || textMsg >= TEXTMSG_COUNT )
		return NULL;

	LanguagePack& lang_pack = FOServer::LangPacks[lang];
	FOMsg& msg = lang_pack.Msg[textMsg];

	if( msg.Count(strNum) == 0 )
		return NULL;

	const char* c_str = msg.GetStr(strNum);

	return new ScriptString(c_str);
}

ScriptString* Item_GetLexems(Item* item)
{
	if( !item || item->IsNotValid )
        SCRIPT_ERROR_R0( "This nullptr." );
	if( !item->PLexems )
		return NULL;

	return new ScriptString(item->PLexems);
}

int ConstantsManager_GetValue(size_t collection, ScriptString* string)
{
	if( string == NULL || collection > CONSTANTS_HASH)
		return -1;

	const char* c_str = string->c_str();

	return ConstantsManager::GetValue(collection, c_str);
}
