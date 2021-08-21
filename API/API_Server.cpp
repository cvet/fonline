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

bool Global_RunCritterScript( Critter* cr, const char* script_name, int p0, int p1, int p2, const char* p3_raw, const uint* p4_ptr, size_t p4_size )
{
	ScriptString* p3 = NULL;
	if( p3_raw ) {
		p3 = new ScriptString(p3_raw);
	}

	ScriptArray*  p4 = NULL;
	if( p4_ptr && p4_size) {
		p4 = Script::CreateArray( "int[]" );
		if(p4) {
			p4->Resize(p4_size);
			memcpy( p4->At(0), p4_ptr, p4_size * sizeof( int ) );
		}
	}
	

	char module_name[ MAX_SCRIPT_NAME + 1 ] = { 0 };
    char func_name[ MAX_SCRIPT_NAME + 1 ] = { 0 };

	Script::ReparseScriptName(script_name, module_name, func_name);

	int bind_id = Script::Bind( module_name, func_name, "void %s(Critter@,int,int,int,string@,int[]@)", true );

	bool ok = false;
	if( bind_id > 0 && Script::PrepareContext( bind_id, _FUNC_, cr ? cr->GetInfo() : "Global_RunCritterScript" ) )
    {
        Script::SetArgObject( cr );
        Script::SetArgUInt( p0 );
        Script::SetArgUInt( p1 );
        Script::SetArgUInt( p2 );
        Script::SetArgObject( p3 );
        Script::SetArgObject( p4 );
        ok = Script::RunPrepared();
    }

    if( p3 )
        p3->Release();
    if( p4 )
        p4->Release();

	return ok;
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

const ServerStatistics* Server_Statistics() {
	return &FOServer::Statistics;
}

uint Timer_GameTick() {
	return Timer::GameTick();
}
uint Timer_FastTick() {
	return Timer::FastTick();
}
