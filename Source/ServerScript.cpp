#include "Common.h"
#include "Server.h"
#include "AngelScript/preprocessor.h"
#include "ScriptFunctions.h"

// Global_LoadImage
#include "PNG/png.h"

void* ASDebugMalloc( size_t size )
{
    size += sizeof( size_t );
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, (int) size );
    size_t* ptr = (size_t*) malloc( size );
    *ptr = size;
    return ++ptr;
}

void ASDebugFree( void* ptr )
{
    size_t* ptr_ = (size_t*) ptr;
    size_t  size = *( --ptr_ );
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, -(int) size );
    free( ptr_ );
}

static bool                 ASDbgMemoryCanWork = false;
static THREAD bool          ASDbgMemoryInUse = false;
static map< void*, string > ASDbgMemoryPtr;
static char                 ASDbgMemoryBuf[ 1024 ];
static Mutex                ASDbgMemoryLocker;

void* ASDeepDebugMalloc( size_t size )
{
    size += sizeof( size_t );
    size_t* ptr = (size_t*) malloc( size );
    *ptr = size;

    if( ASDbgMemoryCanWork && !ASDbgMemoryInUse )
    {
        SCOPE_LOCK( ASDbgMemoryLocker );
        ASDbgMemoryInUse = true;
        const char* module = Script::GetActiveModuleName();
        const char* func = Script::GetActiveFuncName();
        Str::Format( ASDbgMemoryBuf, "AS : %s : %s", module ? module : "<nullptr>", func ? func : "<nullptr>" );
        MEMORY_PROCESS_STR( ASDbgMemoryBuf, (int) size );
        ASDbgMemoryPtr.insert( PAIR( ptr, string( ASDbgMemoryBuf ) ) );
        ASDbgMemoryInUse = false;
    }
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, (int) size );

    return ++ptr;
}

void ASDeepDebugFree( void* ptr )
{
    size_t* ptr_ = (size_t*) ptr;
    size_t  size = *( --ptr_ );

    if( ASDbgMemoryCanWork )
    {
        SCOPE_LOCK( ASDbgMemoryLocker );
        auto it = ASDbgMemoryPtr.find( ptr_ );
        if( it != ASDbgMemoryPtr.end() )
        {
            MEMORY_PROCESS_STR( ( *it ).second.c_str(), -(int) size );
            ASDbgMemoryPtr.erase( it );
        }
    }
    MEMORY_PROCESS( MEMORY_ANGEL_SCRIPT, -(int) size );

    free( ptr_ );
}

bool FOServer::InitScriptSystem()
{
    WriteLog( "Script system initialization...\n" );

    // Memory debugging
    #ifdef MEMORY_DEBUG
    asThreadCleanup();
    if( MemoryDebugLevel >= 2 )
        asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
    else if( MemoryDebugLevel >= 1 )
        asSetGlobalMemoryFunctions( ASDebugMalloc, ASDebugFree );
    else
        asSetGlobalMemoryFunctions( malloc, free );
    #endif

    // Profiler settings
    IniParser& cfg = IniParser::GetServerConfig();
    uint       sample_time = cfg.GetInt( "ProfilerSampleInterval", 0 );
    uint       profiler_mode = cfg.GetInt( "ProfilerMode", 0 );
    if( !profiler_mode )
        sample_time = 0;

    // Init
    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( PRAGMA_SERVER );
    if( !Script::Init( pragma_callback, "SERVER", AllowServerNativeCalls,
                       sample_time, ( profiler_mode & 1 ) != 0, ( profiler_mode & 2 ) != 0 ) )
    {
        WriteLog( "Script system initialization failed.\n" );
        return false;
    }

    // Properties
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();

    // Bind vars and functions, look bind.h
    asIScriptEngine* engine = Script::GetEngine();
    #define BIND_SERVER
    #define BIND_CLASS    FOServer::SScriptFunc::
    #define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLogF( _FUNC_, " - Bind error, line<%d>.\n", __LINE__ ); return false; }
    #include "ScriptBind.h"

    // Load script modules
    Script::Undef( NULL );
    Script::Define( "__SERVER" );
    Script::Define( "__VERSION %d", FONLINE_VERSION );
    if( !Script::ReloadScripts( "Server", false ) )
    {
        Script::Finish();
        WriteLog( "Reload scripts fail.\n" );
        return false;
    }

    // Store property pragmas to synchronize with client
    ServerPropertyPragmas.clear();
    EngineData*    ed = (EngineData*) engine->GetUserData();
    const Pragmas& pragmas = ed->PragmaCB->GetProcessedPragmas();
    for( auto it = pragmas.begin(); it != pragmas.end(); ++it )
    {
        const Preprocessor::PragmaInstance& pragma = *it;
        if( pragma.Name == "property" )
            ServerPropertyPragmas.push_back( pragma );
    }

    // Bind game functions
    ReservedScriptFunction BindGameFunc[] =
    {
        { &ServerFunctions.Init, "init", "void %s()" },
        { &ServerFunctions.Start, "start", "bool %s()" },
        { &ServerFunctions.GetStartTime, "get_start_time", "void %s(uint16&,uint16&,uint16&,uint16&,uint16&,uint16&)" },
        { &ServerFunctions.GenerateWorld, "generate_world", "bool %s()" },
        { &ServerFunctions.Finish, "finish", "void %s()" },
        { &ServerFunctions.Loop, "loop", "uint %s()" },
        { &ServerFunctions.GlobalProcess, "global_process", "void %s(int,Critter&,Item@,float&,float&,float&,float&,float&,uint&,bool&)" },
        { &ServerFunctions.GlobalInvite, "global_invite", "void %s(Critter&,Item@,uint,int,uint&,uint16&,uint16&,uint8&)" },
        { &ServerFunctions.CritterAttack, "critter_attack", "void %s(Critter&,Critter&,ProtoItem&,uint8,ProtoItem@)" },
        { &ServerFunctions.CritterAttacked, "critter_attacked", "void %s(Critter&,Critter&)" },
        { &ServerFunctions.CritterStealing, "critter_stealing", "bool %s(Critter&,Critter&,Item&,uint)" },
        { &ServerFunctions.CritterUseItem, "critter_use_item", "bool %s(Critter&,Item&,Critter@,Item@,Scenery@,uint)" },
        { &ServerFunctions.CritterUseSkill, "critter_use_skill", "bool %s(Critter&,CritterProperty,Critter@,Item@,Scenery@)" },
        { &ServerFunctions.CritterReloadWeapon, "critter_reload_weapon", "void %s(Critter&,Item&,Item@)" },
        { &ServerFunctions.CritterInit, "critter_init", "void %s(Critter&,bool)" },
        { &ServerFunctions.CritterFinish, "critter_finish", "void %s(Critter&,bool)" },
        { &ServerFunctions.CritterIdle, "critter_idle", "void %s(Critter&)" },
        { &ServerFunctions.CritterDead, "critter_dead", "void %s(Critter&,Critter@)" },
        { &ServerFunctions.CritterRespawn, "critter_respawn", "void %s(Critter&)" },
        { &ServerFunctions.CritterCheckMoveItem, "critter_check_move_item", "bool %s(Critter&,Item&,uint8,Item@)" },
        { &ServerFunctions.CritterMoveItem, "critter_move_item", "void %s(Critter&,Item&,uint8)" },
        { &ServerFunctions.MapCritterIn, "map_critter_in", "void %s(Map&,Critter&)" },
        { &ServerFunctions.MapCritterOut, "map_critter_out", "void %s(Map&,Critter&)" },
        { &ServerFunctions.NpcPlaneBegin, "npc_plane_begin", "bool %s(Critter&,NpcPlane&,int,Critter@,Item@)" },
        { &ServerFunctions.NpcPlaneEnd, "npc_plane_end", "bool %s(Critter&,NpcPlane&,int,Critter@,Item@)" },
        { &ServerFunctions.NpcPlaneRun, "npc_plane_run", "bool %s(Critter&,NpcPlane&,int,uint&,uint&,uint&)" },
        { &ServerFunctions.KarmaVoting, "karma_voting", "void %s(Critter&,Critter&,bool)" },
        { &ServerFunctions.CheckLook, "check_look", "bool %s(Map&,Critter&,Critter&)" },
        { &ServerFunctions.ItemCost, "item_cost", "uint %s(Item&,Critter&,Critter&,bool)" },
        { &ServerFunctions.ItemsBarter, "items_barter", "bool %s(Item@[]&,uint[]&,Item@[]&,uint[]&,Critter&,Critter&)" },
        { &ServerFunctions.ItemsCrafted, "items_crafted", "void %s(Item@[]&,uint[]&,Item@[]&,Critter&)" },
        { &ServerFunctions.PlayerLevelUp, "player_levelup", "void %s(Critter&,CritterProperty,uint,CritterProperty)" },
        { &ServerFunctions.TurnBasedBegin, "turn_based_begin", "void %s(Map&)" },
        { &ServerFunctions.TurnBasedEnd, "turn_based_end", "void %s(Map&)" },
        { &ServerFunctions.TurnBasedProcess, "turn_based_process", "void %s(Map&,Critter&,bool)" },
        { &ServerFunctions.TurnBasedSequence, "turn_based_sequence", "void %s(Map&,Critter@[]&,Critter@)" },
        { &ServerFunctions.WorldSave, "world_save", "void %s(uint,uint[]&)" },
        { &ServerFunctions.PlayerRegistration, "player_registration", "bool %s(uint,string&,uint&,uint&,string&)" },
        { &ServerFunctions.PlayerLogin, "player_login", "bool %s(uint,string&,uint,uint&,uint&,string&)" },
        { &ServerFunctions.PlayerGetAccess, "player_getaccess", "bool %s(Critter&,int,string&)" },
        { &ServerFunctions.PlayerAllowCommand, "player_allowcommand", "bool %s(Critter@,string@,uint8)" },
        { &ServerFunctions.CheckTrapLook, "check_trap_look", "bool %s(Map&,Critter&,Item&)" },
        { &ServerFunctions.GetUseApCost, "get_use_ap_cost", "uint %s(Critter&,Item&,uint8)" },
        { &ServerFunctions.GetAttackDistantion, "get_attack_distantion", "uint %s(Critter&,Item&,uint8)" },
    };
    if( !Script::BindReservedFunctions( BindGameFunc, sizeof( BindGameFunc ) / sizeof( BindGameFunc[ 0 ] ) ) )
    {
        Script::Finish();
        WriteLog( "Bind game functions fail.\n" );
        return false;
    }

    ASDbgMemoryCanWork = true;

    GlobalVars::SetPropertyRegistrator( registrators[ 0 ] );
    GlobalVars::PropertiesRegistrator->SetNativeSendCallback( OnSendGlobalValue );
    Globals = new GlobalVars();
    Critter::SetPropertyRegistrator( registrators[ 1 ] );
    Critter::PropertiesRegistrator->SetNativeSendCallback( OnSendCritterValue );
    Critter::PropertiesRegistrator->SetNativeSetCallback( "HandsItemProtoId", OnSetCritterHandsItemProtoId );
    Critter::PropertiesRegistrator->SetNativeSetCallback( "HandsItemMode", OnSetCritterHandsItemMode );
    Item::SetPropertyRegistrator( registrators[ 2 ] );
    Item::PropertiesRegistrator->SetNativeSendCallback( OnSendItemValue );
    Item::PropertiesRegistrator->SetNativeSetCallback( "Count", OnSetItemCount );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsHidden", OnSetItemChangeView );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsAlwaysView", OnSetItemChangeView );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsTrap", OnSetItemChangeView );
    Item::PropertiesRegistrator->SetNativeSetCallback( "TrapValue", OnSetItemChangeView );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsNoBlock", OnSetItemRecacheHex );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsShootThru", OnSetItemRecacheHex );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsGag", OnSetItemRecacheHex );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsGeck", OnSetItemIsGeck );
    Item::PropertiesRegistrator->SetNativeSetCallback( "IsRadio", OnSetItemIsRadio );
    ProtoItem::SetPropertyRegistrator( registrators[ 3 ] );
    Map::SetPropertyRegistrator( registrators[ 4 ] );
    Map::PropertiesRegistrator->SetNativeSendCallback( OnSendMapValue );
    Location::SetPropertyRegistrator( registrators[ 5 ] );
    Location::PropertiesRegistrator->SetNativeSendCallback( OnSendLocationValue );

    WriteLog( "Script system initialization complete.\n" );
    return true;
}

bool FOServer::PostInitScriptSystem()
{
    EngineData* ed = (EngineData*) Script::GetEngine()->GetUserData();
    if( ed->PragmaCB->IsError() )
        return false;
    ed->PragmaCB->Finish();
    if( ed->PragmaCB->IsError() )
        return false;
    return true;
}

void FOServer::FinishScriptSystem()
{
    WriteLog( "Script system finish...\n" );
    Script::Finish();
    WriteLog( "Script system finish complete.\n" );
}

void FOServer::ScriptSystemUpdate()
{
    Script::SetRunTimeout( GameOpt.ScriptRunSuspendTimeout, GameOpt.ScriptRunMessageTimeout );
}

bool FOServer::DialogScriptDemand( DemandResult& demand, Critter* master, Critter* slave )
{
    int bind_id = (int) demand.ParamId;
    if( !Script::PrepareContext( bind_id, _FUNC_, master->GetInfo() ) )
        return 0;
    Script::SetArgObject( master );
    Script::SetArgObject( slave );
    for( int i = 0; i < demand.ValuesCount; i++ )
        Script::SetArgUInt( demand.ValueExt[ i ] );
    if( Script::RunPrepared() )
        return Script::GetReturnedBool();
    return false;
}

uint FOServer::DialogScriptResult( DemandResult& result, Critter* master, Critter* slave )
{
    int bind_id = (int) result.ParamId;
    if( !Script::PrepareContext( bind_id, _FUNC_, Str::FormatBuf( "Critter<%s>, func<%s>", master->GetInfo(), Script::GetBindFuncName( bind_id ).c_str() ) ) )
        return 0;
    Script::SetArgObject( master );
    Script::SetArgObject( slave );
    for( int i = 0; i < result.ValuesCount; i++ )
        Script::SetArgUInt( result.ValueExt[ i ] );
    if( Script::RunPrepared() && result.RetValue )
        return Script::GetReturnedUInt();
    return 0;
}

/************************************************************************/
/* Client script processing                                             */
/************************************************************************/

#undef BIND_SERVER
#undef BIND_CLASS
#undef BIND_ASSERT
#define BIND_CLIENT
#define BIND_CLASS    BindClass::
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLogF( _FUNC_, " - Bind error, line<%d>.\n", __LINE__ ); bind_errors++; }

namespace ClientBind
{
    #include "DummyData.h"
    static int Bind( asIScriptEngine* engine, PropertyRegistrator** registrators )
    {
        int bind_errors = 0;
        #include "ScriptBind.h"
        return bind_errors;
    }
}

bool FOServer::ReloadClientScripts()
{
    WriteLog( "Reload client scripts...\n" );

    // Disable debug allocators
    #ifdef MEMORY_DEBUG
    asThreadCleanup();
    asSetGlobalMemoryFunctions( malloc, free );
    #endif

    // Swap engine
    asIScriptEngine*      old_engine = Script::GetEngine();
    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( PRAGMA_CLIENT );
    asIScriptEngine*      engine = Script::CreateEngine( pragma_callback, "CLIENT", AllowClientNativeCalls );
    if( engine )
        Script::SetEngine( engine );

    // Properties
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();

    // Bind vars and functions
    int bind_errors = 0;
    if( engine )
        bind_errors = ClientBind::Bind( engine, registrators );

    // Check errors
    if( !engine || bind_errors )
    {
        if( !engine )
            WriteLogF( _FUNC_, " - asCreateScriptEngine fail.\n" );
        else
            WriteLog( "Bind fail, errors<%d>.\n", bind_errors );
        Script::FinishEngine( engine );

        #ifdef MEMORY_DEBUG
        asThreadCleanup();
        if( MemoryDebugLevel >= 2 )
            asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
        else if( MemoryDebugLevel >= 1 )
            asSetGlobalMemoryFunctions( ASDebugMalloc, ASDebugFree );
        else
            asSetGlobalMemoryFunctions( malloc, free );
        #endif
        return false;
    }

    // Load script modules
    Script::Undef( "__SERVER" );
    Script::Define( "__CLIENT" );
    Script::Define( "__VERSION %d", FONLINE_VERSION );
    Script::SetLoadLibraryCompiler( true );

    FOMsg msg_script;
    int   num = STR_INTERNAL_SCRIPT_MODULES;
    int   errors = 0;
    if( Script::ReloadScripts( "Client", false, "CLIENT_" ) )
    {
        for( asUINT i = 0; i < engine->GetModuleCount(); i++ )
        {
            asIScriptModule* module = engine->GetModuleByIndex( i );
            CBytecodeStream  binary;
            if( !module || module->SaveByteCode( &binary ) < 0 )
            {
                WriteLogF( _FUNC_, " - Unable to save bytecode of client script<%s>.\n", module->GetName() );
                errors++;
                continue;
            }
            std::vector< asBYTE >& buf = binary.GetBuf();

            // Add module name and bytecode
            msg_script.AddStr( num, module->GetName() );
            msg_script.AddBinary( num + 1, (uchar*) &buf[ 0 ], (uint) buf.size() );
            num += 2;
        }
    }
    else
    {
        errors++;
    }

    // Add native dlls to MSG
    int         dll_num = STR_INTERNAL_SCRIPT_DLLS;
    EngineData* ed = (EngineData*) engine->GetUserData();
    for( auto it = ed->LoadedDlls.begin(), end = ed->LoadedDlls.end(); it != end; ++it )
    {
        const string& dll_name = ( *it ).first;
        const string& dll_path = ( *it ).second.first;

        // Load libraries for all platforms
        // Windows, Linux
        for( int d = 0; d < 2; d++ )
        {
            // Make file name
            const char* extensions[] = { ".dll", ".so" };
            char        fname[ MAX_FOPATH ];
            Str::Copy( fname, dll_path.c_str() );
            FileManager::EraseExtension( fname );
            Str::Append( fname, extensions[ d ] );

            // Erase first './'
            if( Str::CompareCount( fname, DIR_SLASH_SD, Str::Length( DIR_SLASH_SD ) ) )
                Str::EraseInterval( fname, Str::Length( DIR_SLASH_SD ) );

            // Load dll
            FileManager dll;
            if( !dll.LoadFile( fname, PT_ROOT ) )
            {
                if( !d )
                {
                    WriteLogF( _FUNC_, " - Can't load dll<%s>.\n", dll_name.c_str() );
                    errors++;
                }
                continue;
            }

            // Add dll name and binary
            msg_script.AddStr( dll_num, fname );
            msg_script.AddBinary( dll_num + 1, dll.GetBuf(), dll.GetFsize() );
            dll_num += 2;
        }
    }

    // Finish
    Pragmas pragmas = ed->PragmaCB->GetProcessedPragmas();
    Script::FinishEngine( engine );
    Script::Undef( "__CLIENT" );
    Script::Define( "__SERVER" );
    Script::SetLoadLibraryCompiler( false );

    #ifdef MEMORY_DEBUG
    asThreadCleanup();
    if( MemoryDebugLevel >= 2 )
        asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
    else if( MemoryDebugLevel >= 1 )
        asSetGlobalMemoryFunctions( ASDebugMalloc, ASDebugFree );
    else
        asSetGlobalMemoryFunctions( malloc, free );
    #endif
    Script::SetEngine( old_engine );

    // Add config text and pragmas
    uint pragma_index = 0;
    for( size_t i = 0; i < pragmas.size(); i++ )
    {
        if( pragmas[ i ].Name != "property" )
        {
            msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 2, pragmas[ i ].Name.c_str() );
            msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 2 + 1, pragmas[ i ].Text.c_str() );
            pragma_index++;
        }
        else
        {
            // Verify client property, it is must present in server scripts
            bool found = false;
            for( size_t j = 0; j < ServerPropertyPragmas.size(); j++ )
            {
                if( ServerPropertyPragmas[ j ].Text == pragmas[ i ].Text )
                {
                    found = true;
                    break;
                }
            }
            if( !found )
            {
                WriteLog( "Property '%s' not registered in server scripts.\n", pragmas[ i ].Text.c_str() );
                errors++;
            }
        }
    }
    for( size_t i = 0; i < ServerPropertyPragmas.size(); i++ )
    {
        msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 2, ServerPropertyPragmas[ i ].Name.c_str() );
        msg_script.AddStr( STR_INTERNAL_SCRIPT_PRAGMAS + pragma_index * 2 + 1, ServerPropertyPragmas[ i ].Text.c_str() );
        pragma_index++;
    }

    // Exit if have errors
    if( errors )
        return false;

    // Copy critter types
    LanguagePack& default_lang = *LangPacks.begin();
    for( int i = 0; i < MAX_CRIT_TYPES; i++ )
        if( default_lang.Msg[ TEXTMSG_INTERNAL ].Count( STR_INTERNAL_CRTYPE( i ) ) )
            msg_script.AddStr( STR_INTERNAL_CRTYPE( i ), default_lang.Msg[ TEXTMSG_INTERNAL ].GetStr( STR_INTERNAL_CRTYPE( i ) ) );

    // Copy generated MSG to language packs
    for( auto it = LangPacks.begin(), end = LangPacks.end(); it != end; ++it )
    {
        LanguagePack& lang = *it;
        lang.Msg[ TEXTMSG_INTERNAL ] = msg_script;
    }

    // Regenerate update files
    GenerateUpdateFiles();

    WriteLog( "Reload client scripts complete.\n" );
    return true;
}

/************************************************************************/
/* Mapper script processing                                             */
/************************************************************************/

#undef BIND_CLIENT
#undef BIND_CLASS
#undef BIND_ASSERT
#define BIND_MAPPER
#define BIND_CLASS    BindClass::
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLogF( _FUNC_, " - Bind error, line<%d>.\n", __LINE__ ); bind_errors++; }

namespace MapperBind
{
    #include "DummyData.h"
    static int Bind( asIScriptEngine* engine, PropertyRegistrator** registrators )
    {
        int bind_errors = 0;
        #include "ScriptBind.h"
        return bind_errors;
    }
}

bool FOServer::ReloadMapperScripts()
{
    WriteLog( "Reload mapper scripts...\n" );

    // Disable debug allocators
    #ifdef MEMORY_DEBUG
    asThreadCleanup();
    asSetGlobalMemoryFunctions( malloc, free );
    #endif

    // Swap engine
    asIScriptEngine*      old_engine = Script::GetEngine();
    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( PRAGMA_MAPPER );
    asIScriptEngine*      engine = Script::CreateEngine( pragma_callback, "MAPPER", true );
    if( engine )
        Script::SetEngine( engine );

    // Properties
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();

    // Bind vars and functions
    int bind_errors = 0;
    if( engine )
        bind_errors = MapperBind::Bind( engine, registrators );

    // Check errors
    if( !engine || bind_errors )
    {
        if( !engine )
            WriteLogF( _FUNC_, " - asCreateScriptEngine fail.\n" );
        else
            WriteLog( "Bind fail, errors<%d>.\n", bind_errors );
        Script::FinishEngine( engine );

        #ifdef MEMORY_DEBUG
        asThreadCleanup();
        if( MemoryDebugLevel >= 2 )
            asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
        else if( MemoryDebugLevel >= 1 )
            asSetGlobalMemoryFunctions( ASDebugMalloc, ASDebugFree );
        else
            asSetGlobalMemoryFunctions( malloc, free );
        #endif
        return false;
    }

    // Load script modules
    Script::Undef( "__SERVER" );
    Script::Define( "__MAPPER" );
    Script::Define( "__VERSION %d", FONLINE_VERSION );
    Script::SetLoadLibraryCompiler( true );

    int errors = 0;
    if( !Script::ReloadScripts( "Client", false, "MAPPER_" ) )
        errors++;

    // Imported functions
    if( !Script::BindImportedFunctions() )
        errors++;

    // Finish
    Script::FinishEngine( engine );
    Script::Undef( "__MAPPER" );
    Script::Define( "__SERVER" );
    Script::SetLoadLibraryCompiler( false );

    #ifdef MEMORY_DEBUG
    asThreadCleanup();
    if( MemoryDebugLevel >= 2 )
        asSetGlobalMemoryFunctions( ASDeepDebugMalloc, ASDeepDebugFree );
    else if( MemoryDebugLevel >= 1 )
        asSetGlobalMemoryFunctions( ASDebugMalloc, ASDebugFree );
    else
        asSetGlobalMemoryFunctions( malloc, free );
    #endif
    Script::SetEngine( old_engine );

    // Exit if have errors
    if( errors )
        return false;

    WriteLog( "Reload mapper scripts complete.\n" );
    return true;
}

/************************************************************************/
/* Wrapper functions                                                    */
/************************************************************************/

int SortCritterHx = 0, SortCritterHy = 0;
bool SortCritterByDistPred( Critter* cr1, Critter* cr2 )
{
    return DistGame( SortCritterHx, SortCritterHy, cr1->GetHexX(), cr1->GetHexY() ) < DistGame( SortCritterHx, SortCritterHy, cr2->GetHexX(), cr2->GetHexY() );
}
void SortCritterByDist( Critter* cr, CrVec& critters )
{
    SortCritterHx = cr->GetHexX();
    SortCritterHy = cr->GetHexY();
    std::sort( critters.begin(), critters.end(), SortCritterByDistPred );
}
void SortCritterByDist( int hx, int hy, CrVec& critters )
{
    SortCritterHx = hx;
    SortCritterHy = hy;
    std::sort( critters.begin(), critters.end(), SortCritterByDistPred );
}

ScriptString* FOServer::SScriptFunc::ProtoItem_GetScriptName( ProtoItem* proto )
{
    return ScriptString::Create( proto->ScriptName );
}

void FOServer::SScriptFunc::Synchronizer_Constructor( void* memory )
{
    new (memory) SyncObject();
}

void FOServer::SScriptFunc::Synchronizer_Destructor( void* memory )
{
    SyncObject* obj = (SyncObject*) memory;
    obj->Unlock();
    obj->~SyncObject();
}

AIDataPlane* FOServer::SScriptFunc::NpcPlane_GetCopy( AIDataPlane* plane )
{
    return plane->GetCopy();
}

AIDataPlane* FOServer::SScriptFunc::NpcPlane_SetChild( AIDataPlane* plane, AIDataPlane* child_plane )
{
    if( child_plane->Assigned )
        child_plane = child_plane->GetCopy();
    else
        child_plane->AddRef();
    plane->ChildPlane = child_plane;
    return child_plane;
}

AIDataPlane* FOServer::SScriptFunc::NpcPlane_GetChild( AIDataPlane* plane, uint index )
{
    AIDataPlane* result = plane->ChildPlane;
    for( uint i = 0; i < index && result; i++ )
        result = result->ChildPlane;
    return result;
}

bool FOServer::SScriptFunc::NpcPlane_Misc_SetScript( AIDataPlane* plane, ScriptString& func_name )
{
    uint bind_id = Script::Bind( func_name.c_str(), "void %s(Critter&)", false );
    if( !bind_id )
        SCRIPT_ERROR_R0( "Script not found." );
    plane->Misc.ScriptBindId = bind_id;
    return true;
}

Item* FOServer::SScriptFunc::Container_AddItem( Item* cont, hash pid, uint count, uint stack_id )
{
    if( cont->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cont->IsContainer() )
        SCRIPT_ERROR_R0( "Container item is not container type." );
    if( !ItemMngr.GetProtoItem( pid ) )
        SCRIPT_ERROR_R0( "Invalid proto '%s' arg.", HASH_STR( pid ) );
    if( !count )
        count = 1;
    return ItemMngr.AddItemContainer( cont, pid, count, stack_id );
}

uint FOServer::SScriptFunc::Container_GetItems( Item* cont, uint stack_id, ScriptArray* items )
{
    if( !items )
        SCRIPT_ERROR_R0( "Items array arg nullptr." );
    if( cont->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cont->IsContainer() )
        SCRIPT_ERROR_R0( "Container item is not container type." );

    ItemVec items_;
    cont->ContGetItems( items_, stack_id, items != NULL );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

Item* FOServer::SScriptFunc::Container_GetItem( Item* cont, hash pid, uint stack_id )
{
    if( cont->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cont->IsContainer() )
        SCRIPT_ERROR_R0( "Container item is not container type." );
    return cont->ContGetItemByPid( pid, stack_id );
}

bool FOServer::SScriptFunc::Item_IsStackable( Item* item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return item->IsStackable();
}

bool FOServer::SScriptFunc::Item_IsDeteriorable( Item* item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return item->IsDeteriorable();
}

bool FOServer::SScriptFunc::Item_SetScript( Item* item, ScriptString* script )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !script || !script->length() )
    {
        item->SetScriptId( 0 );
    }
    else
    {
        if( !item->ParseScript( script->c_str(), true ) )
            SCRIPT_ERROR_R0( "Script function not found." );
    }
    return true;
}

hash FOServer::SScriptFunc::Item_GetScriptId( Item* item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return item->GetScriptId();
}

bool FOServer::SScriptFunc::Item_SetEvent( Item* item, int event_type, ScriptString* func_name )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( event_type < 0 || event_type >= ITEM_EVENT_MAX )
        SCRIPT_ERROR_R0( "Invalid event type arg." );
    if( !func_name || !func_name->length() )
        item->FuncId[ event_type ] = 0;
    else
    {
        item->FuncId[ event_type ] = Script::Bind( func_name->c_str(), ItemEventFuncName[ event_type ], false );
        if( !item->FuncId[ event_type ] )
            SCRIPT_ERROR_R0( "Function not found." );

        if( event_type == ITEM_EVENT_WALK && item->Accessory == ITEM_ACCESSORY_HEX )
        {
            Map* map = MapMngr.GetMap( item->AccHex.MapId );
            if( map )
                map->SetHexFlag( item->AccHex.HexX, item->AccHex.HexY, FH_WALK_ITEM );
        }
    }
    return true;
}

hash FOServer::SScriptFunc::Item_get_ProtoId( Item* item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return item->GetProtoId();
}

int FOServer::SScriptFunc::Item_get_Type( Item* item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return item->GetType();
}

uint FOServer::SScriptFunc::Item_GetCost( Item* item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return item->GetWholeCost();
}

Map* FOServer::SScriptFunc::Item_GetMapPosition( Item* item, ushort& hx, ushort& hy )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    Map* map = NULL;
    switch( item->Accessory )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        Critter* cr = CrMngr.GetCritter( item->AccCritter.Id, true );
        if( !cr )
            SCRIPT_ERROR_R0( "Critter accessory, critter not found." );
        if( !cr->GetMapId() )
        {
            hx = cr->Data.WorldX;
            hy = cr->Data.WorldY;
            return NULL;
        }
        map = MapMngr.GetMap( cr->GetMapId(), true );
        if( !map )
            SCRIPT_ERROR_R0( "Critter accessory, map not found." );
        hx = cr->GetHexX();
        hy = cr->GetHexY();
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        map = MapMngr.GetMap( item->AccHex.MapId, true );
        if( !map )
            SCRIPT_ERROR_R0( "Hex accessory, map not found." );
        hx = item->AccHex.HexX;
        hy = item->AccHex.HexY;
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
    {
        if( item->GetId() == item->AccContainer.ContainerId )
            SCRIPT_ERROR_R0( "Container accessory, crosslink." );
        Item* cont = ItemMngr.GetItem( item->AccContainer.ContainerId, false );
        if( !cont )
            SCRIPT_ERROR_R0( "Container accessory, container not found." );
        return Item_GetMapPosition( cont, hx, hy );             // Recursion
    }
    break;
    default:
        SCRIPT_ERROR_R0( "Unknown accessory." );
        break;
    }
    return map;
}

bool FOServer::SScriptFunc::Item_ChangeProto( Item* item, hash pid )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
    if( !proto_item )
        SCRIPT_ERROR_R0( "Proto item not found." );
    if( item->GetType() != proto_item->GetType() )
        SCRIPT_ERROR_R0( "Different types." );

    ProtoItem* old_proto_item = item->Proto;
    item->Proto = proto_item;

    if( item->Accessory == ITEM_ACCESSORY_CRITTER )
    {
        Critter* cr = CrMngr.GetCritter( item->AccCritter.Id, false );
        if( !cr )
            return true;
        item->Proto = old_proto_item;
        cr->Send_EraseItem( item );
        item->Proto = proto_item;
        cr->Send_AddItem( item );
        cr->SendAA_MoveItem( item, ACTION_REFRESH, 0 );
    }
    else if( item->Accessory == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( item->AccHex.MapId, true );
        if( !map )
            return true;
        ushort hx = item->AccHex.HexX;
        ushort hy = item->AccHex.HexY;
        item->Proto = old_proto_item;
        map->EraseItem( item->GetId() );
        item->Proto = proto_item;
        map->AddItem( item, hx, hy );
    }
    return true;
}

void FOServer::SScriptFunc::Item_Animate( Item* item, uchar from_frm, uchar to_frm )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    switch( item->Accessory )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        //	Critter* cr=CrMngr.GetCrit(item->ACC_CRITTER.Id);
        //	if(cr) cr->Send_AnimateItem(item,from_frm,to_frm);
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        Map* map = MapMngr.GetMap( item->AccHex.MapId );
        if( map )
            map->AnimateItem( item, from_frm, to_frm );
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
        break;
    default:
        SCRIPT_ERROR_R( "Unknown accessory." );
    }
}

Item* FOServer::SScriptFunc::Item_GetChild( Item* item, uint child_index )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( child_index >= ITEM_MAX_CHILDS )
        SCRIPT_ERROR_R0( "Wrong child index." );
    return item->GetChild( child_index );
}

bool FOServer::SScriptFunc::Item_LockerOpen( Item* item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !item->IsHasLocker() )
        SCRIPT_ERROR_R0( "Door item is no have locker." );
    if( !item->LockerIsChangeble() )
        SCRIPT_ERROR_R0( "Door is not changeble." );
    if( item->LockerIsOpen() )
        return true;

    ushort locker_condition = item->GetLockerCondition();
    SETFLAG( locker_condition, LOCKER_ISOPEN );
    item->SetLockerCondition( locker_condition );

    if( item->IsDoor() )
    {
        bool recache_block = false;
        bool recache_shoot = false;
        if( !item->Proto->GetDoor_NoBlockMove() )
        {
            item->SetIsNoBlock( true );
            recache_block = true;
        }
        if( !item->Proto->GetDoor_NoBlockShoot() )
        {
            item->SetIsShootThru( true );
            recache_shoot = true;
        }
        if( !item->Proto->GetDoor_NoBlockLight() )
        {
            item->SetIsLightThru( true );
        }

        if( item->Accessory == ITEM_ACCESSORY_HEX && ( recache_block || recache_shoot ) )
        {
            Map* map = MapMngr.GetMap( item->AccHex.MapId );
            if( map )
            {
                if( recache_block && recache_shoot )
                    map->RecacheHexBlockShoot( item->AccHex.HexX, item->AccHex.HexY );
                else if( recache_block )
                    map->RecacheHexBlock( item->AccHex.HexX, item->AccHex.HexY );
                else if( recache_shoot )
                    map->RecacheHexShoot( item->AccHex.HexX, item->AccHex.HexY );
            }
        }
    }
    return true;
}

bool FOServer::SScriptFunc::Item_LockerClose( Item* item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !item->IsHasLocker() )
        SCRIPT_ERROR_R0( "Door item is no have locker." );
    if( !item->LockerIsChangeble() )
        SCRIPT_ERROR_R0( "Door is not changeble." );
    if( item->LockerIsClose() )
        return true;

    ushort locker_condition = item->GetLockerCondition();
    UNSETFLAG( locker_condition, LOCKER_ISOPEN );
    item->SetLockerCondition( locker_condition );

    if( item->IsDoor() )
    {
        bool recache_block = false;
        bool recache_shoot = false;
        if( !item->Proto->GetDoor_NoBlockMove() )
        {
            item->SetIsNoBlock( false );
            recache_block = true;
        }
        if( !item->Proto->GetDoor_NoBlockShoot() )
        {
            item->SetIsShootThru( false );
            recache_shoot = true;
        }
        if( !item->Proto->GetDoor_NoBlockLight() )
        {
            item->SetIsLightThru( false );
        }

        if( item->Accessory == ITEM_ACCESSORY_HEX && ( recache_block || recache_shoot ) )
        {
            Map* map = MapMngr.GetMap( item->AccHex.MapId );
            if( map )
            {
                if( recache_block )
                    map->SetHexFlag( item->AccHex.HexX, item->AccHex.HexY, FH_BLOCK_ITEM );
                if( recache_shoot )
                    map->SetHexFlag( item->AccHex.HexX, item->AccHex.HexY, FH_NRAKE_ITEM );
            }
        }
    }
    return true;
}

void FOServer::SScriptFunc::Item_EventFinish( Item* item, bool deleted )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    item->EventFinish( deleted );
}

bool FOServer::SScriptFunc::Item_EventAttack( Item* item, Critter* attacker, Critter* target )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( attacker->IsDestroyed )
        SCRIPT_ERROR_R0( "Attacker critter arg is destroyed." );
    if( target->IsDestroyed )
        SCRIPT_ERROR_R0( "Target critter arg is destroyed." );
    return item->EventAttack( attacker, target );
}

bool FOServer::SScriptFunc::Item_EventUse( Item* item, Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );
    if( on_critter && on_critter->IsDestroyed )
        SCRIPT_ERROR_R0( "On critter arg is destroyed." );
    if( on_item && on_item->IsDestroyed )
        SCRIPT_ERROR_R0( "On item arg is destroyed." );
    return item->EventUse( cr, on_critter, on_item, on_scenery );
}

bool FOServer::SScriptFunc::Item_EventUseOnMe( Item* item, Critter* cr, Item* used_item )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );
    if( used_item && used_item->IsDestroyed )
        SCRIPT_ERROR_R0( "Used item arg is destroyed." );
    return item->EventUseOnMe( cr, used_item );
}

bool FOServer::SScriptFunc::Item_EventSkill( Item* item, Critter* cr, int skill )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );
    return item->EventSkill( cr, skill );
}

void FOServer::SScriptFunc::Item_EventDrop( Item* item, Critter* cr )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    item->EventDrop( cr );
}

void FOServer::SScriptFunc::Item_EventMove( Item* item, Critter* cr, uchar from_slot )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    item->EventMove( cr, from_slot );
}

void FOServer::SScriptFunc::Item_EventWalk( Item* item, Critter* cr, bool entered, uchar dir )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    item->EventWalk( cr, entered, dir );
}

uint FOServer::SScriptFunc::CraftItem_GetShowParams( CraftItem* craft, ScriptArray* nums, ScriptArray* vals, ScriptArray* ors )
{
    if( nums )
        Script::AppendVectorToArray( craft->ShowPNum, nums );
    if( vals )
        Script::AppendVectorToArray( craft->ShowPVal, vals );
    if( ors )
        Script::AppendVectorToArray( craft->ShowPOr, ors );
    return (uint) craft->ShowPNum.size();
}

uint FOServer::SScriptFunc::CraftItem_GetNeedParams( CraftItem* craft, ScriptArray* nums, ScriptArray* vals, ScriptArray* ors )
{
    if( nums )
        Script::AppendVectorToArray( craft->NeedPNum, nums );
    if( vals )
        Script::AppendVectorToArray( craft->NeedPVal, vals );
    if( ors )
        Script::AppendVectorToArray( craft->NeedPOr, ors );
    return (uint) craft->NeedPNum.size();
}

uint FOServer::SScriptFunc::CraftItem_GetNeedTools( CraftItem* craft, ScriptArray* pids, ScriptArray* vals, ScriptArray* ors )
{
    if( pids )
        Script::AppendVectorToArray( craft->NeedTools, pids );
    if( vals )
        Script::AppendVectorToArray( craft->NeedToolsVal, vals );
    if( ors )
        Script::AppendVectorToArray( craft->NeedToolsOr, ors );
    return (uint) craft->NeedTools.size();
}

uint FOServer::SScriptFunc::CraftItem_GetNeedItems( CraftItem* craft, ScriptArray* pids, ScriptArray* vals, ScriptArray* ors )
{
    if( pids )
        Script::AppendVectorToArray( craft->NeedItems, pids );
    if( vals )
        Script::AppendVectorToArray( craft->NeedItemsVal, vals );
    if( ors )
        Script::AppendVectorToArray( craft->NeedItemsOr, ors );
    return (uint) craft->NeedItems.size();
}

uint FOServer::SScriptFunc::CraftItem_GetOutItems( CraftItem* craft, ScriptArray* pids, ScriptArray* vals )
{
    if( pids )
        Script::AppendVectorToArray( craft->OutItems, pids );
    if( vals )
        Script::AppendVectorToArray( craft->OutItemsVal, vals );
    return (uint) craft->OutItems.size();
}

bool FOServer::SScriptFunc::Scen_CallSceneryFunction( MapObject* scenery, Critter* cr, int skill, Item* item )
{
    if( !scenery->RunTime.BindScriptId )
        return false;
    if( !Script::PrepareContext( scenery->RunTime.BindScriptId, _FUNC_, cr->GetInfo() ) )
        return false;

    Script::SetArgObject( cr );
    Script::SetArgObject( scenery );
    Script::SetArgUInt( item ? SKILL_PICK_ON_GROUND : skill );
    Script::SetArgObject( item );
    for( int i = 0, j = MIN( scenery->MScenery.ParamsCount, 5 ); i < j; i++ )
        Script::SetArgUInt( scenery->MScenery.Param[ i ] );
    return Script::RunPrepared() && Script::GetReturnedBool();
}

bool FOServer::SScriptFunc::Crit_IsPlayer( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsPlayer();
}

bool FOServer::SScriptFunc::Crit_IsNpc( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsNpc();
}

bool FOServer::SScriptFunc::Crit_IsCanWalk( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsCanWalk();
}

bool FOServer::SScriptFunc::Crit_IsCanRun( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsCanRun();
}

bool FOServer::SScriptFunc::Crit_IsCanRotate( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return CritType::IsCanRotate( cr->GetCrType() );
}

bool FOServer::SScriptFunc::Crit_IsCanAim( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return CritType::IsCanAim( cr->GetCrType() ) && !cr->GetIsNoAim();
}

bool FOServer::SScriptFunc::Crit_IsAnim1( Critter* cr, uint index )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return CritType::IsAnim1( cr->GetCrType(), index );
}

int FOServer::SScriptFunc::Cl_GetAccess( Critter* cl )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        SCRIPT_ERROR_R0( "Critter is not player." );
    return ( (Client*) cl )->Access;
}

bool FOServer::SScriptFunc::Cl_SetAccess( Critter* cl, int access )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        SCRIPT_ERROR_R0( "Critter is not player." );
    if( access < ACCESS_CLIENT || access > ACCESS_ADMIN )
        SCRIPT_ERROR_R0( "Wrong access type." );

    if( access == ( (Client*) cl )->Access )
        return true;

    bool allow = false;
    if( Script::PrepareContext( ServerFunctions.PlayerGetAccess, _FUNC_, cl->GetInfo() ) )
    {
        ScriptString* pass = ScriptString::Create();
        Script::SetArgObject( cl );
        Script::SetArgUInt( access );
        Script::SetArgObject( pass );
        if( Script::RunPrepared() && Script::GetReturnedBool() )
            allow = true;
        pass->Release();
    }

    if( allow )
        ( (Client*) cl )->Access = access;
    return allow;
}

bool FOServer::SScriptFunc::Crit_SetEvent( Critter* cr, int event_type, ScriptString* func_name )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( event_type < 0 || event_type >= CRITTER_EVENT_MAX )
        SCRIPT_ERROR_R0( "Invalid event type arg." );
    if( !func_name || !func_name->length() )
        cr->FuncId[ event_type ] = 0;
    else
    {
        cr->FuncId[ event_type ] = Script::Bind( func_name->c_str(), CritterEventFuncName[ event_type ], false );
        if( !cr->FuncId[ event_type ] )
            SCRIPT_ERROR_R0( "Function not found." );
    }
    return true;
}

Map* FOServer::SScriptFunc::Crit_GetMap( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return MapMngr.GetMap( cr->GetMapId() );
}

uint FOServer::SScriptFunc::Crit_GetMapId( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->GetMapId();
}

hash FOServer::SScriptFunc::Crit_GetMapProtoId( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->GetMapProtoId();
}

bool FOServer::SScriptFunc::Crit_ChangeCrType( Critter* cr, uint new_type )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !new_type )
        SCRIPT_ERROR_R0( "New type arg is zero." );
    if( !CritType::IsEnabled( new_type ) )
        SCRIPT_ERROR_R0( "New type '%u' is not enabled.", new_type );

    if( cr->GetCrType() != new_type )
    {
        if( cr->Data.Multihex < 0 && cr->GetMapId() && !cr->IsDead() )
        {
            uint old_mh = CritType::GetMultihex( cr->GetCrType() );
            uint new_mh = CritType::GetMultihex( new_type );
            if( new_mh != old_mh )
            {
                Map* map = MapMngr.GetMap( cr->GetMapId() );
                if( map )
                {
                    map->UnsetFlagCritter( cr->GetHexX(), cr->GetHexY(), old_mh, false );
                    map->SetFlagCritter( cr->GetHexX(), cr->GetHexY(), new_mh, false );
                }
            }
        }

        cr->Data.CrType = new_type;
        cr->Send_CustomCommand( cr, OTHER_BASE_TYPE, new_type );
        cr->SendA_CustomCommand( OTHER_BASE_TYPE, new_type );
    }
    return true;
}

void FOServer::SScriptFunc::Cl_DropTimers( Critter* cl )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        return;
    Client* cl_ = (Client*) cl;
    cl_->DropTimers( true );
}

bool FOServer::SScriptFunc::Crit_MoveRandom( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cr->GetMapId() )
        SCRIPT_ERROR_R0( "Critter is on global." );
    return MoveRandom( cr );
}

bool FOServer::SScriptFunc::Crit_MoveToDir( Critter* cr, uchar direction )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
        SCRIPT_ERROR_R0( "Critter is on global." );
    if( direction >= DIRS_COUNT )
        SCRIPT_ERROR_R0( "Invalid direction arg." );

    ushort hx = cr->GetHexX();
    ushort hy = cr->GetHexY();
    MoveHexByDir( hx, hy, direction, map->GetMaxHexX(), map->GetMaxHexY() );
    ushort move_flags = direction | BIN16( 00000000, 00111000 );
    bool   move = Act_Move( cr, hx, hy, move_flags );
    if( !move )
        SCRIPT_ERROR_R0( "Move fail." );
    cr->Send_Move( cr, move_flags );
    return true;
}

bool FOServer::SScriptFunc::Crit_TransitToHex( Critter* cr, ushort hx, ushort hy, uchar dir )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R0( "Transfers locked." );
    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
        SCRIPT_ERROR_R0( "Critter is on global." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    if( hx != cr->GetHexX() || hy != cr->GetHexY() )
    {
        if( dir < DIRS_COUNT && cr->Data.Dir != dir )
            cr->Data.Dir = dir;
        if( !MapMngr.Transit( cr, map, hx, hy, cr->GetDir(), 2, true ) )
            SCRIPT_ERROR_R0( "Transit fail." );
    }
    else if( dir < DIRS_COUNT && cr->Data.Dir != dir )
    {
        cr->Data.Dir = dir;
        cr->Send_Dir( cr );
        cr->SendA_Dir();
    }
    return true;
}

bool FOServer::SScriptFunc::Crit_TransitToMapHex( Critter* cr, uint map_id, ushort hx, ushort hy, uchar dir, bool with_group )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R0( "Transfers locked." );
    if( !map_id )
        SCRIPT_ERROR_R0( "Map id arg is zero." );
    Map* map = MapMngr.GetMap( map_id );
    if( !map )
        SCRIPT_ERROR_R0( "Map not found." );

    if( !cr->GetMapId() )
    {
        if( !with_group )
        {
            MapMngr.GM_LeaveGroup( cr );
            return Crit_TransitToMapHex( cr, map_id, hx, hy, dir, true );
        }
        if( dir < DIRS_COUNT && cr->Data.Dir != dir )
            cr->Data.Dir = dir;
        if( !cr->GroupMove )
            SCRIPT_ERROR_R0( "Group nullptr." );
        if( !MapMngr.GM_GroupToMap( cr->GroupMove, map, 0, hx, hy, cr->GetDir() ) )
            SCRIPT_ERROR_R0( "Transit from global to map fail." );
    }
    else
    {
        if( cr->GetMapId() != map_id || cr->GetHexX() != hx || cr->GetHexY() != hy )
        {
            if( dir < DIRS_COUNT && cr->Data.Dir != dir )
                cr->Data.Dir = dir;
            if( !MapMngr.Transit( cr, map, hx, hy, cr->GetDir(), 2, true ) )
                SCRIPT_ERROR_R0( "Transit from map to map fail." );
        }
        else if( dir < DIRS_COUNT && cr->Data.Dir != dir )
        {
            cr->Data.Dir = dir;
            cr->Send_Dir( cr );
            cr->SendA_Dir();
        }
        else
            return true;
    }

    Location* loc = map->GetLocation( true );
    if( loc && DistSqrt( cr->Data.WorldX, cr->Data.WorldY, loc->Data.WX, loc->Data.WY ) > loc->GetRadius() )
    {
        cr->Data.WorldX = loc->Data.WX;
        cr->Data.WorldY = loc->Data.WY;
    }
    return true;
}

bool FOServer::SScriptFunc::Crit_TransitToMapEntire( Critter* cr, uint map_id, int entire, bool with_group )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R0( "Transfers locked." );
    if( !map_id )
        SCRIPT_ERROR_R0( "Map id arg is zero." );
    // if(cr->GetMap()==map_id) SCRIPT_ERROR_R0("Critter already on need map.");
    Map* map = MapMngr.GetMap( map_id );
    if( !map )
        SCRIPT_ERROR_R0( "Map not found." );

    ushort hx, hy;
    uchar  dir;
    if( !map->GetStartCoord( hx, hy, dir, entire ) )
        SCRIPT_ERROR_R0( "Entire '%d' not found.", entire );
    if( !cr->GetMapId() )
    {
        if( !with_group )
        {
            MapMngr.GM_LeaveGroup( cr );
            return Crit_TransitToMapEntire( cr, map_id, entire, true );
        }
        if( !cr->GroupMove )
            SCRIPT_ERROR_R0( "Group nullptr." );
        if( !MapMngr.GM_GroupToMap( cr->GroupMove, map, 0, hx, hy, dir ) )
            SCRIPT_ERROR_R0( "Transit from global to map fail." );
    }
    else
    {
        if( !MapMngr.Transit( cr, map, hx, hy, dir, 2, true ) )
            SCRIPT_ERROR_R0( "Transit from map to map fail." );
    }

    Location* loc = map->GetLocation( true );
    if( loc && DistSqrt( cr->Data.WorldX, cr->Data.WorldY, loc->Data.WX, loc->Data.WY ) > loc->GetRadius() )
    {
        cr->Data.WorldX = loc->Data.WX;
        cr->Data.WorldY = loc->Data.WY;
    }

    return true;
}

bool FOServer::SScriptFunc::Crit_TransitToGlobal( Critter* cr, bool request_group )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R0( "Transfers locked." );
    if( !cr->GetMapId() )
        return true;                   // SCRIPT_ERROR_R0("Critter already on global.");
    if( !MapMngr.TransitToGlobal( cr, 0, request_group ? FOLLOW_PREP : FOLLOW_FORCE, true ) )
        SCRIPT_ERROR_R0( "Transit fail." );
    return true;
}

bool FOServer::SScriptFunc::Crit_TransitToGlobalWithGroup( Critter* cr, ScriptArray& group )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R0( "Transfers locked." );
    if( !cr->GetMapId() )
        SCRIPT_ERROR_R0( "Critter already on global." );
    if( !MapMngr.TransitToGlobal( cr, 0, FOLLOW_FORCE, true ) )
        SCRIPT_ERROR_R0( "Transit fail." );
    for( int i = 0, j = group.GetSize(); i < j; i++ )
    {
        Critter* cr_ = *(Critter**) group.At( i );
        if( !cr_ || cr_->IsDestroyed || !cr_->GetMapId() )
            continue;
        MapMngr.TransitToGlobal( cr_, cr->GetId(), FOLLOW_FORCE, true );
    }
    return true;
}

bool FOServer::SScriptFunc::Crit_TransitToGlobalGroup( Critter* cr, uint critter_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->LockMapTransfers )
        SCRIPT_ERROR_R0( "Transfers locked." );
    if( !cr->GetMapId() )
        SCRIPT_ERROR_R0( "Critter already on global." );
    Critter* cr_global = CrMngr.GetCritter( critter_id, true );
    if( !cr_global )
        SCRIPT_ERROR_R0( "Critter on global not found." );
    if( cr_global->GetMapId() || !cr_global->GroupMove )
        SCRIPT_ERROR_R0( "Founded critter is not on global." );
    if( !MapMngr.TransitToGlobal( cr, cr_global->GroupMove->Rule->GetId(), FOLLOW_FORCE, true ) )
        SCRIPT_ERROR_R0( "Transit fail." );
    return true;
}

bool FOServer::SScriptFunc::Crit_IsLife( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsLife();
}

bool FOServer::SScriptFunc::Crit_IsKnockout( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsKnockout();
}

bool FOServer::SScriptFunc::Crit_IsDead( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsDead();
}

bool FOServer::SScriptFunc::Crit_IsFree( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsFree() && !cr->IsWait();
}

bool FOServer::SScriptFunc::Crit_IsBusy( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->IsBusy() || cr->IsWait();
}

void FOServer::SScriptFunc::Crit_Wait( Critter* cr, uint ms )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->SetWait( ms );
    if( cr->IsPlayer() )
    {
        Client* cl = (Client*) cr;
        cl->SetBreakTime( ms );
        cl->Send_CustomCommand( cr, OTHER_BREAK_TIME, ms );
    }
}

/*uint FOServer::SScriptFunc::Crit_GetWait(Critter* cr)
   {
        if(cr->IsDestroyed) SCRIPT_ERROR_R0("Attempt to call method on destroyed object.");
        if(cr->IsPlayer())
        {
                cr->SetBreakTime(ms);
                cr->Send_ParamOther(OTHER_BREAK_TIME,cr->GetBreakTime());
        }
        else if(cr->IsNpc())
        {
                ((Npc*)cr)->SetWait(ms);
        }
   }*/

void FOServer::SScriptFunc::Crit_ToDead( Critter* cr, uint anim2, Critter* killer )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDead() )
        return;                  // SCRIPT_ERROR_R("Critter already dead.");

    KillCritter( cr, anim2, killer );
}

bool FOServer::SScriptFunc::Crit_ToLife( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->IsLife() )
        return true;                  // SCRIPT_ERROR_R("Critter already life.");

    if( cr->IsDead() )
    {
        if( !cr->GetMapId() )
            SCRIPT_ERROR_R0( "Critter on global map." );
        Map* map = MapMngr.GetMap( cr->GetMapId() );
        if( !map )
            SCRIPT_ERROR_R0( "Map not found." );
        if( !map->IsHexesPassed( cr->GetHexX(), cr->GetHexY(), cr->GetMultihex() ) )
            SCRIPT_ERROR_R0( "Position busy." );
        RespawnCritter( cr );
    }
    else
    {
        if( cr->GetCurrentHp() <= 0 )
            cr->SetCurrentHp( 1 );
        if( cr->GetCurrentAp() <= 0 )
            cr->SetCurrentAp( AP_DIVIDER );
        cr->KnockoutAp = 0;
        cr->TryUpOnKnockout();
    }

    if( !cr->IsLife() )
        SCRIPT_ERROR_R0( "Respawn critter fail." );
    return true;
}

bool FOServer::SScriptFunc::Crit_ToKnockout( Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, ushort knock_hx, ushort knock_hy )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->IsDead() )
        SCRIPT_ERROR_R0( "Critter is dead." );

    if( cr->IsKnockout() )
    {
        cr->KnockoutAp += lost_ap;
        return true;
    }

    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
        SCRIPT_ERROR_R0( "Critter map not found." );
    if( knock_hx >= map->GetMaxHexX() || knock_hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );

    if( cr->GetHexX() != knock_hx || cr->GetHexY() != knock_hy )
    {
        bool passed = false;
        uint multihex = cr->GetMultihex();
        if( multihex )
        {
            map->UnsetFlagCritter( cr->GetHexX(), cr->GetHexY(), multihex, false );
            passed = map->IsHexesPassed( knock_hx, knock_hy, multihex );
            map->SetFlagCritter( cr->GetHexX(), cr->GetHexY(), multihex, false );
        }
        else
        {
            passed = map->IsHexPassed( knock_hx, knock_hy );
        }
        if( !passed )
            SCRIPT_ERROR_R0( "Knock hexes is busy." );
    }

    KnockoutCritter( cr, anim2begin, anim2idle, anim2end, lost_ap, knock_hx, knock_hy );
    return true;
}

void FOServer::SScriptFunc::Crit_RefreshVisible( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->ProcessVisibleCritters();
    cr->ProcessVisibleItems();
}

void FOServer::SScriptFunc::Crit_ViewMap( Critter* cr, Map* map, uint look, ushort hx, ushort hy, uchar dir )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Map arg is destroyed." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    if( !cr->IsPlayer() )
        return;
    if( dir >= DIRS_COUNT )
        dir = cr->GetDir();
    if( !look )
        look = cr->GetLookDistance();

    cr->ViewMapId = map->GetId();
    cr->ViewMapPid = map->GetPid();
    cr->ViewMapLook = look;
    cr->ViewMapHx = hx;
    cr->ViewMapHy = hy;
    cr->ViewMapDir = dir;
    cr->ViewMapLocId = 0;
    cr->ViewMapLocEnt = 0;
    cr->Send_LoadMap( map );
}

void FOServer::SScriptFunc::Crit_AddHolodiskInfo( Critter* cr, uint holodisk_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    AddPlayerHoloInfo( cr, holodisk_num, true );
}

void FOServer::SScriptFunc::Crit_EraseHolodiskInfo( Critter* cr, uint holodisk_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    ErasePlayerHoloInfo( cr, holodisk_num, true );
}

bool FOServer::SScriptFunc::Crit_IsHolodiskInfo( Critter* cr, uint holodisk_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ScriptArray* holo_info = cr->GetHoloInfo();
    for( int i = 0, j = holo_info->GetSize(); i < j; i++ )
    {
        if( *(uint*) holo_info->At( i ) == holodisk_num )
        {
            SAFEREL( holo_info );
            return true;
        }
    }
    SAFEREL( holo_info );
    return false;
}

void FOServer::SScriptFunc::Crit_Say( Critter* cr, uchar how_say, ScriptString& text )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( how_say == SAY_FLASH_WINDOW )
        text = " ";
    if( !text.length() )
        SCRIPT_ERROR_R( "Text empty." );
    if( cr->IsNpc() && !cr->IsLife() )
        return;                                  // SCRIPT_ERROR_R("Npc is not life.");

    if( how_say >= SAY_NETMSG )
        cr->Send_Text( cr, text.c_str(), how_say );
    else if( cr->GetMapId() )
        cr->SendAA_Text( cr->VisCr, text.c_str(), how_say, false );
}

void FOServer::SScriptFunc::Crit_SayMsg( Critter* cr, uchar how_say, ushort text_msg, uint num_str )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsNpc() && !cr->IsLife() )
        return;                                  // SCRIPT_ERROR_R("Npc is not life.");

    if( how_say >= SAY_NETMSG )
        cr->Send_TextMsg( cr, num_str, how_say, text_msg );
    else if( cr->GetMapId() )
        cr->SendAA_Msg( cr->VisCr, num_str, how_say, text_msg );
}

void FOServer::SScriptFunc::Crit_SayMsgLex( Critter* cr, uchar how_say, ushort text_msg, uint num_str, ScriptString& lexems )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsNpc() && !cr->IsLife() )
        return;                                  // SCRIPT_ERROR_R("Npc is not life.");
    // if(!lexems.length()) SCRIPT_ERROR_R("Lexems arg is empty.");

    if( how_say >= SAY_NETMSG )
        cr->Send_TextMsgLex( cr, num_str, how_say, text_msg, lexems.c_str() );
    else if( cr->GetMapId() )
        cr->SendAA_MsgLex( cr->VisCr, num_str, how_say, text_msg, lexems.c_str() );
}

void FOServer::SScriptFunc::Crit_SetDir( Critter* cr, uchar dir )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( dir >= DIRS_COUNT )
        SCRIPT_ERROR_R( "Invalid direction arg." );
    if( cr->Data.Dir == dir )
        return;                      // SCRIPT_ERROR_R("Direction already set.");

    cr->Data.Dir = dir;
    if( cr->GetMapId() )
    {
        cr->Send_Dir( cr );
        cr->SendA_Dir();
    }
}

bool FOServer::SScriptFunc::Crit_PickItem( Critter* cr, ushort hx, ushort hy, hash pid )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    Map* map = MapMngr.GetMap( cr->GetMapId() );
    if( !map )
        SCRIPT_ERROR_R0( "Map not found." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    bool pick = Act_PickItem( cr, hx, hy, pid );
    if( !pick )
        SCRIPT_ERROR_R0( "Pick fail." );
    return true;
}

void FOServer::SScriptFunc::Crit_SetFavoriteItem( Critter* cr, int slot, hash pid )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    switch( slot )
    {
    case SLOT_HAND1:
        cr->SetFavoriteItemPid( SLOT_HAND1, pid );
        break;
    case SLOT_HAND2:
        cr->SetFavoriteItemPid( SLOT_HAND2, pid );
        break;
    case SLOT_ARMOR:
        cr->SetFavoriteItemPid( SLOT_ARMOR, pid );
        break;
    default:
        SCRIPT_ERROR_R( "Invalid slot arg." );
        break;
    }
}

hash FOServer::SScriptFunc::Crit_GetFavoriteItem( Critter* cr, int slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    switch( slot )
    {
    case SLOT_HAND1:
        return cr->GetFavoriteItemPid( SLOT_HAND1 );
    case SLOT_HAND2:
        return cr->GetFavoriteItemPid( SLOT_HAND2 );
    case SLOT_ARMOR:
        return cr->GetFavoriteItemPid( SLOT_ARMOR );
    default:
        SCRIPT_ERROR_R0( "Invalid slot arg." );
    }
    return 0;
}

uint FOServer::SScriptFunc::Crit_GetCritters( Critter* cr, bool look_on_me, int find_type, ScriptArray* critters )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec cr_vec;
    for( auto it = ( look_on_me ? cr->VisCr.begin() : cr->VisCrSelf.begin() ), end = ( look_on_me ? cr->VisCr.end() : cr->VisCrSelf.end() ); it != end; ++it )
    {
        Critter* cr_ = *it;
        if( cr_->CheckFind( find_type ) )
            cr_vec.push_back( cr_ );
    }

    if( critters )
    {
        SortCritterByDist( cr, cr_vec );
        for( auto it = cr_vec.begin(), end = cr_vec.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Critter* >( cr_vec, critters );
    }
    return (uint) cr_vec.size();
}

uint FOServer::SScriptFunc::Crit_GetFollowGroup( Critter* cr, int find_type, ScriptArray* critters )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec cr_vec;
    for( auto it = cr->VisCrSelf.begin(), end = cr->VisCrSelf.end(); it != end; ++it )
    {
        Critter* cr_ = *it;
        if( cr_->GetFollowCrit() == cr->GetId() && cr_->CheckFind( find_type ) )
            cr_vec.push_back( cr_ );
    }

    if( critters )
    {
        SortCritterByDist( cr, cr_vec );
        for( auto it = cr_vec.begin(), end = cr_vec.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Critter* >( cr_vec, critters );
    }
    return (uint) cr_vec.size();
}

Critter* FOServer::SScriptFunc::Crit_GetFollowLeader( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    uint leader_id = cr->GetFollowCrit();
    if( !leader_id )
        return NULL;
    return cr->GetCritSelf( leader_id, true );
}

ScriptArray* FOServer::SScriptFunc::Crit_GetGlobalGroup( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->GetMapId() || !cr->GroupMove )
        return NULL;
    ScriptArray* result = MapMngr.GM_CreateGroupArray( cr->GroupMove );
    if( !result )
        SCRIPT_ERROR_R0( "Fail to create group." );
    return result;
}

bool FOServer::SScriptFunc::Crit_IsGlobalGroupLeader( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return !cr->GetMapId() && cr->GroupMove && cr->GroupMove->Rule == cr;
}

void FOServer::SScriptFunc::Crit_LeaveGlobalGroup( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    MapMngr.GM_LeaveGroup( cr );
}

void FOServer::SScriptFunc::Crit_GiveGlobalGroupLead( Critter* cr, Critter* to_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( to_cr->IsDestroyed )
        SCRIPT_ERROR_R( "To critter this is destroyed." );
    MapMngr.GM_GiveRule( cr, to_cr );
}

uint FOServer::SScriptFunc::Npc_GetTalkedPlayers( Critter* cr, ScriptArray* players )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !cr->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );

    uint  talk = 0;
    CrVec players_;
    for( auto it = cr->VisCr.begin(), end = cr->VisCr.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( !cr->IsPlayer() )
            continue;

        Client* cl = (Client*) cr;
        if( cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == cr->GetId() )
        {
            talk++;
            if( players )
                players_.push_back( cl );
        }
    }

    if( players )
    {
        SortCritterByDist( cr, players_ );
        for( auto it = players_.begin(), end = players_.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Critter* >( players_, players );
    }
    return talk;
}

bool FOServer::SScriptFunc::Crit_IsSeeCr( Critter* cr, Critter* cr_ )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr_->IsDestroyed )
        return false;
    if( cr == cr_ )
        return true;
    CrVec& critters = ( cr->GetMapId() ? cr->VisCrSelf : cr->GroupMove->CritMove );
    return std::find( critters.begin(), critters.end(), cr_ ) != critters.end();
}

bool FOServer::SScriptFunc::Crit_IsSeenByCr( Critter* cr, Critter* cr_ )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr_->IsDestroyed )
        return false;
    if( cr == cr_ )
        return true;
    CrVec& critters = ( cr->GetMapId() ? cr->VisCr : cr->GroupMove->CritMove );
    return std::find( critters.begin(), critters.end(), cr_ ) != critters.end();
}

bool FOServer::SScriptFunc::Crit_IsSeeItem( Critter* cr, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( item->IsDestroyed )
        return false;
    return cr->CountIdVisItem( item->GetId() );
}

Item* FOServer::SScriptFunc::Crit_AddItem( Critter* cr, hash pid, uint count )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !pid )
        SCRIPT_ERROR_R0( "Proto id arg is zero." );
    if( !ItemMngr.GetProtoItem( pid ) )
        SCRIPT_ERROR_R0( "Invalid proto '%s'.", HASH_STR( pid ) );
    if( !count )
        count = 1;
    return ItemMngr.AddItemCritter( cr, pid, count );
}

bool FOServer::SScriptFunc::Crit_DeleteItem( Critter* cr, hash pid, uint count )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !pid )
        SCRIPT_ERROR_R0( "Proto id arg is zero." );
    if( !count )
        count = cr->CountItemPid( pid );
    return ItemMngr.SubItemCritter( cr, pid, count );
}

uint FOServer::SScriptFunc::Crit_ItemsCount( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->CountItems();
}

uint FOServer::SScriptFunc::Crit_ItemsWeight( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->GetItemsWeight();
}

uint FOServer::SScriptFunc::Crit_ItemsVolume( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->GetItemsVolume();
}

uint FOServer::SScriptFunc::Crit_CountItem( Critter* cr, hash proto_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->CountItemPid( proto_id );
}

Item* FOServer::SScriptFunc::Crit_GetItem( Critter* cr, hash proto_id, int slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( proto_id && slot >= 0 && slot < SLOT_GROUND )
        return cr->GetItemByPidSlot( proto_id, slot );
    else if( proto_id )
        return cr->GetItemByPidInvPriority( proto_id );
    else if( slot >= 0 && slot < SLOT_GROUND )
        return cr->GetItemSlot( slot );
    return NULL;
}

Item* FOServer::SScriptFunc::Crit_GetItemById( Critter* cr, uint item_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->GetItem( item_id, false );
}

uint FOServer::SScriptFunc::Crit_GetItems( Critter* cr, int slot, ScriptArray* items )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ItemVec items_;
    cr->GetItemsSlot( slot, items_, items != NULL );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

uint FOServer::SScriptFunc::Crit_GetItemsByType( Critter* cr, int type, ScriptArray* items )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ItemVec items_;
    cr->GetItemsType( type, items_, items != NULL );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

ProtoItem* FOServer::SScriptFunc::Crit_GetSlotProto( Critter* cr, int slot, uchar& mode )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    Item* item = NULL;
    switch( slot )
    {
    case SLOT_HAND1:
        item = cr->ItemSlotMain;
        break;
    case SLOT_HAND2:
        item = ( cr->ItemSlotExt->GetId() ? cr->ItemSlotExt : cr->GetHandsItem() );
        break;
    case SLOT_ARMOR:
        item = cr->ItemSlotArmor;
        break;
    default:
        item = cr->GetItemSlot( slot );
        break;
    }
    if( !item )
        return NULL;

    mode = item->GetMode();
    if( !item->GetId() && ( item == cr->ItemSlotMain || item == cr->ItemSlotExt ) )
        mode = cr->GetHandsItemMode();
    return item->Proto;
}

bool FOServer::SScriptFunc::Crit_MoveItem( Critter* cr, uint item_id, uint count, uchar to_slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !item_id )
        SCRIPT_ERROR_R0( "Item id arg is zero." );
    Item* item = cr->GetItem( item_id, cr->IsPlayer() );
    if( !item )
        SCRIPT_ERROR_R0( "Item not found." );
    if( !count )
        count = item->GetCount();
    if( item->AccCritter.Slot == to_slot )
        return true;                                    // SCRIPT_ERROR_R0("To slot arg is equal of current item slot.");
    if( count > item->GetCount() )
        SCRIPT_ERROR_R0( "Item count arg is greater than items count." );
    bool result = cr->MoveItem( item->AccCritter.Slot, to_slot, item_id, count );
    if( !result )
        return false;             // SCRIPT_ERROR_R0("Fail to move item.");
    cr->Send_AddItem( item );
    return true;
}

uint FOServer::SScriptFunc::Npc_ErasePlane( Critter* npc, int plane_type, bool all )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );

    Npc*            npc_ = (Npc*) npc;
    AIDataPlaneVec& planes = npc_->GetPlanes();
    uint            result = 0;
    for( auto it = planes.begin(); it != planes.end();)
    {
        AIDataPlane* p = *it;
        if( p->Type == plane_type || plane_type == -1 )
        {
            if( !result && it == planes.begin() )
                npc->SendA_XY();

            p->Assigned = false;
            p->Release();
            it = planes.erase( it );

            result++;
            if( !all )
                break;
        }
        else
            ++it;
    }

    return result;
}

bool FOServer::SScriptFunc::Npc_ErasePlaneIndex( Critter* npc, uint index )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );

    Npc*            npc_ = (Npc*) npc;
    AIDataPlaneVec& planes = npc_->GetPlanes();
    if( index >= planes.size() )
        SCRIPT_ERROR_R0( "Invalid index arg." );

    AIDataPlane* p = planes[ index ];
    if( !index )
        npc->SendA_XY();
    p->Assigned = false;
    p->Release();
    planes.erase( planes.begin() + index );
    return true;
}

void FOServer::SScriptFunc::Npc_DropPlanes( Critter* npc )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R( "Critter is not npc." );

    Npc*            npc_ = (Npc*) npc;
    AIDataPlaneVec& planes = npc_->GetPlanes();
    npc_->DropPlanes();
}

bool FOServer::SScriptFunc::Npc_IsNoPlanes( Critter* npc )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );
    return ( (Npc*) npc )->IsNoPlanes();
}

bool FOServer::SScriptFunc::Npc_IsCurPlane( Critter* npc, int plane_type )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );
    Npc* npc_ = (Npc*) npc;
    return npc_->IsCurPlane( plane_type );
}

AIDataPlane* FOServer::SScriptFunc::Npc_GetCurPlane( Critter* npc )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );
    Npc* npc_ = (Npc*) npc;
    if( npc_->IsNoPlanes() )
        return NULL;
    return npc_->GetPlanes()[ 0 ];
}

uint FOServer::SScriptFunc::Npc_GetPlanes( Critter* npc, ScriptArray* arr )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );
    Npc* npc_ = (Npc*) npc;
    if( npc_->IsNoPlanes() )
        return 0;
    if( arr )
        Script::AppendVectorToArrayRef< AIDataPlane* >( npc_->GetPlanes(), arr );
    return (uint) npc_->GetPlanes().size();
}

uint FOServer::SScriptFunc::Npc_GetPlanesIdentifier( Critter* npc, int identifier, ScriptArray* arr )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );
    Npc* npc_ = (Npc*) npc;
    if( npc_->IsNoPlanes() )
        return 0;
    AIDataPlaneVec planes = npc_->GetPlanes();   // Copy
    for( auto it = planes.begin(); it != planes.end();)
        if( ( *it )->Identifier != identifier )
            it = planes.erase( it );
        else
            ++it;
    if( !arr )
        return (uint) planes.size();
    Script::AppendVectorToArrayRef< AIDataPlane* >( planes, arr );
    return (uint) planes.size();
}

uint FOServer::SScriptFunc::Npc_GetPlanesIdentifier2( Critter* npc, int identifier, uint identifier_ext, ScriptArray* arr )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );
    Npc* npc_ = (Npc*) npc;
    if( npc_->IsNoPlanes() )
        return 0;
    AIDataPlaneVec planes = npc_->GetPlanes();   // Copy
    for( auto it = planes.begin(); it != planes.end();)
        if( ( *it )->Identifier != identifier || ( *it )->IdentifierExt != identifier_ext )
            it = planes.erase( it );
        else
            ++it;
    if( !arr )
        return (uint) planes.size();
    Script::AppendVectorToArrayRef< AIDataPlane* >( planes, arr );
    return (uint) planes.size();
}

bool FOServer::SScriptFunc::Npc_AddPlane( Critter* npc, AIDataPlane& plane )
{
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Critter is not npc." );
    Npc* npc_ = (Npc*) npc;
    if( npc_->IsNoPlanes() )
        npc_->SetWait( 0 );
    if( plane.Assigned )
    {
        npc_->AddPlane( REASON_FROM_SCRIPT, plane.GetCopy(), false );
    }
    else
    {
        plane.AddRef();
        npc_->AddPlane( REASON_FROM_SCRIPT, &plane, false );
    }
    return true;
}

void FOServer::SScriptFunc::Crit_SendMessage( Critter* cr, int num, int val, int to )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->SendMessage( num, val, to );
}

void FOServer::SScriptFunc::Crit_SendCombatResult( Critter* cr, ScriptArray& arr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( arr.GetElementSize() != sizeof( uint ) )
        SCRIPT_ERROR_R( "Element size is not equal to 4." );
    if( arr.GetSize() > GameOpt.FloodSize / sizeof( uint ) )
        SCRIPT_ERROR_R( "Elements count is greater than maximum." );
    cr->Send_CombatResult( (uint*) arr.At( 0 ), arr.GetSize() );
}

void FOServer::SScriptFunc::Crit_Action( Critter* cr, int action, int action_ext, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->SendAA_Action( action, action_ext, item );
}

void FOServer::SScriptFunc::Crit_Animate( Critter* cr, uint anim1, uint anim2, Item* item, bool clear_sequence, bool delay_play )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->SendAA_Animate( anim1, anim2, item, clear_sequence, delay_play );
}

void FOServer::SScriptFunc::Crit_SetAnims( Critter* cr, int cond, uint anim1, uint anim2 )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cond == 0 || cond == COND_LIFE )
    {
        cr->Data.Anim1Life = anim1;
        cr->Data.Anim2Life = anim2;
    }
    if( cond == 0 || cond == COND_KNOCKOUT )
    {
        cr->Data.Anim1Knockout = anim1;
        cr->Data.Anim2Knockout = anim2;
    }
    if( cond == 0 || cond == COND_DEAD )
    {
        cr->Data.Anim1Dead = anim1;
        cr->Data.Anim2Dead = anim2;
    }
    cr->SendAA_SetAnims( cond, anim1, anim2 );
}

void FOServer::SScriptFunc::Crit_PlaySound( Critter* cr, ScriptString& sound_name, bool send_self )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    char sound_name_[ 100 ];
    Str::Copy( sound_name_, sound_name.c_str() );
    uint crid = cr->GetId();

    if( send_self )
        cr->Send_PlaySound( crid, sound_name_ );
    for( auto it = cr->VisCr.begin(), end = cr->VisCr.end(); it != end; ++it )
    {
        Critter* cr_ = *it;
        cr_->Send_PlaySound( crid, sound_name_ );
    }
}

void FOServer::SScriptFunc::Crit_PlaySoundType( Critter* cr, uchar sound_type, uchar sound_type_ext, uchar sound_id, uchar sound_id_ext, bool send_self )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    uint crid = cr->GetId();
    if( send_self )
        cr->Send_PlaySoundType( crid, sound_type, sound_type_ext, sound_id, sound_id_ext );
    for( auto it = cr->VisCr.begin(), end = cr->VisCr.end(); it != end; ++it )
    {
        Critter* cr_ = *it;
        cr_->Send_PlaySoundType( crid, sound_type, sound_type_ext, sound_id, sound_id_ext );
    }
}

bool FOServer::SScriptFunc::Crit_IsKnownLoc( Critter* cr, bool by_id, uint loc_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( by_id )
        return cr->CheckKnownLocById( loc_num );
    return cr->CheckKnownLocByPid( loc_num );
}

bool FOServer::SScriptFunc::Crit_SetKnownLoc( Critter* cr, bool by_id, uint loc_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    Location* loc = ( by_id ? MapMngr.GetLocation( loc_num ) : MapMngr.GetLocationByPid( loc_num, 0 ) );
    if( !loc )
        SCRIPT_ERROR_R0( "Location not found." );

    cr->AddKnownLoc( loc->GetId() );
    if( loc->IsAutomaps() )
        cr->Send_AutomapsInfo( NULL, loc );
    if( !cr->GetMapId() )
        cr->Send_GlobalLocation( loc, true );

    int zx = GM_ZONE( loc->Data.WX );
    int zy = GM_ZONE( loc->Data.WY );
    if( cr->GMapFog.Get2Bit( zx, zy ) == GM_FOG_FULL )
    {
        cr->GMapFog.Set2Bit( zx, zy, GM_FOG_HALF );
        if( !cr->GetMapId() )
            cr->Send_GlobalMapFog( zx, zy, cr->GMapFog.Get2Bit( zx, zy ) );
    }
    return true;
}

bool FOServer::SScriptFunc::Crit_UnsetKnownLoc( Critter* cr, bool by_id, uint loc_num )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    Location* loc = ( by_id ? MapMngr.GetLocation( loc_num ) : MapMngr.GetLocationByPid( loc_num, 0 ) );
    if( !loc )
        SCRIPT_ERROR_R0( "Location not found." );
    if( !cr->CheckKnownLocById( loc->GetId() ) )
        SCRIPT_ERROR_R0( "Player is not know this location." );

    cr->EraseKnownLoc( loc->GetId() );
    if( !cr->GetMapId() )
        cr->Send_GlobalLocation( loc, false );
    return true;
}

void FOServer::SScriptFunc::Crit_SetFog( Critter* cr, ushort zone_x, ushort zone_y, int fog )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( zone_x >= GameOpt.GlobalMapWidth || zone_y >= GameOpt.GlobalMapHeight )
        SCRIPT_ERROR_R( "Invalid world map pos arg." );
    if( fog < GM_FOG_FULL || fog > GM_FOG_NONE )
        SCRIPT_ERROR_R( "Invalid fog arg." );

    cr->GMapFog.Set2Bit( zone_x, zone_y, fog );
    if( !cr->GetMapId() )
        cr->Send_GlobalMapFog( zone_x, zone_y, fog );
}

int FOServer::SScriptFunc::Crit_GetFog( Critter* cr, ushort zone_x, ushort zone_y )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( zone_x >= GameOpt.GlobalMapWidth || zone_y >= GameOpt.GlobalMapHeight )
        SCRIPT_ERROR_R0( "Invalid world map pos arg." );

    return cr->GMapFog.Get2Bit( zone_x, zone_y );
}

void FOServer::SScriptFunc::Cl_ShowContainer( Critter* cl, Critter* cr_cont, Item* item_cont, uchar transfer_type )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        return;                     // SCRIPT_ERROR_R("Critter is not player.");
    if( cr_cont )
    {
        if( cr_cont->IsDestroyed )
            SCRIPT_ERROR_R( "Critter container is destroyed." );
        ( (Client*) cl )->Send_ContainerInfo( cr_cont, transfer_type, true );
    }
    else if( item_cont )
    {
        if( item_cont->IsDestroyed )
            SCRIPT_ERROR_R( "Item container is destroyed." );
        ( (Client*) cl )->Send_ContainerInfo( item_cont, transfer_type, true );
    }
    else
    {
        ( (Client*) cl )->Send_ContainerInfo();
    }
}

void FOServer::SScriptFunc::Cl_ShowScreen( Critter* cl, int screen_type, uint param, ScriptString* func_name )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        return;                     // SCRIPT_ERROR_R("Critter is not player.");

    uint bind_id = 0;
    if( func_name && func_name->length() )
    {
        bind_id = Script::Bind( func_name->c_str(), "void %s(Critter&,uint,string&)", false );
        if( !bind_id )
            SCRIPT_ERROR_R( "Function not found." );
    }

    Client* cl_ = (Client*) cl;
    cl_->ScreenCallbackBindId = bind_id;
    cl_->Send_ShowScreen( screen_type, param, bind_id != 0 );
}

void FOServer::SScriptFunc::Cl_RunClientScript( Critter* cl, ScriptString& func_name, int p0, int p1, int p2, ScriptString* p3, ScriptArray* p4 )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        SCRIPT_ERROR_R( "Critter is not player." );

    UIntVec dw;
    if( p4 )
        Script::AssignScriptArrayInVector< uint >( dw, p4 );

    Client* cl_ = (Client*) cl;
    cl_->Send_RunClientScript( func_name.c_str(), p0, p1, p2, p3 ? p3->c_str() : NULL, dw );
}

void FOServer::SScriptFunc::Cl_Disconnect( Critter* cl )
{
    if( cl->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !cl->IsPlayer() )
        SCRIPT_ERROR_R( "Critter is not player." );
    Client* cl_ = (Client*) cl;
    if( cl_->IsOnline() )
        cl_->Disconnect();
}

bool FOServer::SScriptFunc::Crit_SetScript( Critter* cr, ScriptString* script )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !script || !script->length() )
    {
        cr->SetScriptId( 0 );
    }
    else
    {
        if( !cr->ParseScript( script->c_str(), true ) )
            SCRIPT_ERROR_R0( "Script function not found." );
    }
    return true;
}

hash FOServer::SScriptFunc::Crit_get_ProtoId( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->Data.ProtoId;
}

uint FOServer::SScriptFunc::Crit_GetMultihex( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->GetMultihex();
}

void FOServer::SScriptFunc::Crit_SetMultihex( Critter* cr, int value )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( value < -1 || value > MAX_HEX_OFFSET )
        SCRIPT_ERROR_R( "Invalid multihex arg value." );
    if( cr->Data.Multihex == value )
        return;

    uint old_mh = cr->GetMultihex();
    cr->Data.Multihex = value;
    uint new_mh = cr->GetMultihex();

    if( old_mh != new_mh && cr->GetMapId() && !cr->IsDead() )
    {
        Map* map = MapMngr.GetMap( cr->GetMapId() );
        if( map )
        {
            map->UnsetFlagCritter( cr->GetHexX(), cr->GetHexY(), old_mh, false );
            map->SetFlagCritter( cr->GetHexX(), cr->GetHexY(), new_mh, false );
        }
    }

    cr->Send_CustomCommand( cr, OTHER_MULTIHEX, value );
    cr->SendA_CustomCommand( OTHER_MULTIHEX, value );
}

void FOServer::SScriptFunc::Crit_AddEnemyToStack( Critter* cr, uint critter_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !critter_id )
        SCRIPT_ERROR_R( "Critter id is zero." );
    cr->AddEnemyToStack( critter_id );
}

bool FOServer::SScriptFunc::Crit_CheckEnemyInStack( Critter* cr, uint critter_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->CheckEnemyInStack( critter_id );
}

void FOServer::SScriptFunc::Crit_EraseEnemyFromStack( Critter* cr, uint critter_id )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->EraseEnemyInStack( critter_id );
}

void FOServer::SScriptFunc::Crit_ClearEnemyStack( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    ScriptArray* enemy_stack = Script::CreateArray( "uint[]" );
    cr->SetEnemyStack( enemy_stack );
    enemy_stack->Release();
}

void FOServer::SScriptFunc::Crit_ClearEnemyStackNpc( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    ScriptArray* enemy_stack = cr->GetEnemyStack();
    bool         removed = false;
    for( uint i = 0; i < enemy_stack->GetSize();)
    {
        if( !IS_CLIENT_ID( *(uint*) enemy_stack->At( i ) ) )
        {
            enemy_stack->RemoveAt( i );
            removed = true;
        }
        else
        {
            i++;
        }
    }
    if( removed )
        cr->SetEnemyStack( enemy_stack );
    SAFEREL( enemy_stack );
}

bool FOServer::SScriptFunc::Crit_AddTimeEvent( Critter* cr, ScriptString& func_name, uint duration, int identifier )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !func_name.length() )
        SCRIPT_ERROR_R0( "Script name is empty." );
    hash func_num = Script::BindScriptFuncNum( func_name.c_str(), "uint %s(Critter&,int,uint&)" );
    if( !func_num )
        SCRIPT_ERROR_R0( "Function not found." );
    cr->AddCrTimeEvent( func_num, 0, duration, identifier );
    return true;
}

bool FOServer::SScriptFunc::Crit_AddTimeEventRate( Critter* cr, ScriptString& func_name, uint duration, int identifier, uint rate )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !func_name.length() )
        SCRIPT_ERROR_R0( "Script name is empty." );
    hash func_num = Script::BindScriptFuncNum( func_name.c_str(), "uint %s(Critter&,int,uint&)" );
    if( !func_num )
        SCRIPT_ERROR_R0( "Function not found." );
    cr->AddCrTimeEvent( func_num, rate, duration, identifier );
    return true;
}

uint FOServer::SScriptFunc::Crit_GetTimeEvents( Critter* cr, int identifier, ScriptArray* indexes, ScriptArray* durations, ScriptArray* rates )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    uint    index = 0;
    UIntVec te_vec;
    for( auto it = cr->CrTimeEvents.begin(), end = cr->CrTimeEvents.end(); it != end; ++it )
    {
        Critter::CrTimeEvent& cte = *it;
        if( cte.Identifier == identifier )
            te_vec.push_back( index );
        index++;
    }

    uint size = (uint) te_vec.size();
    if( !size || ( !indexes && !durations && !rates ) )
        return size;

    uint indexes_size = 0, durations_size = 0, rates_size = 0;
    if( indexes )
    {
        indexes_size = indexes->GetSize();
        indexes->Resize( indexes_size + size );
    }
    if( durations )
    {
        durations_size = durations->GetSize();
        durations->Resize( durations_size + size );
    }
    if( rates )
    {
        rates_size = rates->GetSize();
        rates->Resize( rates_size + size );
    }

    for( uint i = 0; i < size; i++ )
    {
        Critter::CrTimeEvent& cte = cr->CrTimeEvents[ te_vec[ i ] ];
        if( indexes )
            *(uint*) indexes->At( indexes_size + i ) = te_vec[ i ];
        if( durations )
            *(uint*) durations->At( durations_size + i ) = ( cte.NextTime > GameOpt.FullSecond ? cte.NextTime - GameOpt.FullSecond : 0 );
        if( rates )
            *(uint*) rates->At( rates_size + i ) = cte.Rate;
    }
    return size;
}

uint FOServer::SScriptFunc::Crit_GetTimeEventsArr( Critter* cr, ScriptArray& find_identifiers, ScriptArray* identifiers, ScriptArray* indexes, ScriptArray* durations, ScriptArray* rates )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    IntVec find_vec;
    Script::AssignScriptArrayInVector( find_vec, &find_identifiers );

    uint    index = 0;
    UIntVec te_vec;
    for( auto it = cr->CrTimeEvents.begin(), end = cr->CrTimeEvents.end(); it != end; ++it )
    {
        Critter::CrTimeEvent& cte = *it;
        if( std::find( find_vec.begin(), find_vec.end(), cte.Identifier ) != find_vec.end() )
            te_vec.push_back( index );
        index++;
    }

    uint size = (uint) te_vec.size();
    if( !size || ( !identifiers && !indexes && !durations && !rates ) )
        return size;

    uint identifiers_size = 0, indexes_size = 0, durations_size = 0, rates_size = 0;
    if( identifiers )
    {
        identifiers_size = identifiers->GetSize();
        identifiers->Resize( identifiers_size + size );
    }
    if( indexes )
    {
        indexes_size = indexes->GetSize();
        indexes->Resize( indexes_size + size );
    }
    if( durations )
    {
        durations_size = durations->GetSize();
        durations->Resize( durations_size + size );
    }
    if( rates )
    {
        rates_size = rates->GetSize();
        rates->Resize( rates_size + size );
    }

    for( uint i = 0; i < size; i++ )
    {
        Critter::CrTimeEvent& cte = cr->CrTimeEvents[ te_vec[ i ] ];
        if( identifiers )
            *(int*) identifiers->At( identifiers_size + i ) = cte.Identifier;
        if( indexes )
            *(uint*) indexes->At( indexes_size + i ) = te_vec[ i ];
        if( durations )
            *(uint*) durations->At( durations_size + i ) = ( cte.NextTime > GameOpt.FullSecond ? cte.NextTime - GameOpt.FullSecond : 0 );
        if( rates )
            *(uint*) rates->At( rates_size + i ) = cte.Rate;
    }
    return size;
}

void FOServer::SScriptFunc::Crit_ChangeTimeEvent( Critter* cr, uint index, uint new_duration, uint new_rate )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( index >= cr->CrTimeEvents.size() )
        SCRIPT_ERROR_R( "Index arg is greater than maximum time events." );
    Critter::CrTimeEvent cte = cr->CrTimeEvents[ index ];
    cr->EraseCrTimeEvent( index );
    cr->AddCrTimeEvent( cte.FuncNum, new_rate, new_duration, cte.Identifier );
}

void FOServer::SScriptFunc::Crit_EraseTimeEvent( Critter* cr, uint index )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( index >= cr->CrTimeEvents.size() )
        SCRIPT_ERROR_R( "Index arg is greater than maximum time events." );
    cr->EraseCrTimeEvent( index );
}

uint FOServer::SScriptFunc::Crit_EraseTimeEvents( Critter* cr, int identifier )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    uint result = 0;
    for( auto it = cr->CrTimeEvents.begin(); it != cr->CrTimeEvents.end();)
    {
        Critter::CrTimeEvent& cte = *it;
        if( cte.Identifier == identifier )
        {
            it = cr->CrTimeEvents.erase( it );
            result++;
        }
        else
            ++it;
    }
    return result;
}

uint FOServer::SScriptFunc::Crit_EraseTimeEventsArr( Critter* cr, ScriptArray& identifiers )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    uint result = 0;
    for( int i = 0, j = identifiers.GetSize(); i < j; i++ )
    {
        int identifier = *(int*) identifiers.At( i );
        for( auto it = cr->CrTimeEvents.begin(); it != cr->CrTimeEvents.end();)
        {
            Critter::CrTimeEvent& cte = *it;
            if( cte.Identifier == identifier )
            {
                it = cr->CrTimeEvents.erase( it );
                result++;
            }
            else
                ++it;
        }
    }
    return result;
}

void FOServer::SScriptFunc::Crit_EventIdle( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->EventIdle();
}

void FOServer::SScriptFunc::Crit_EventFinish( Critter* cr, bool deleted )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->EventFinish( deleted );
}

void FOServer::SScriptFunc::Crit_EventDead( Critter* cr, Critter* killer )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( killer && killer->IsDestroyed )
        SCRIPT_ERROR_R( "Killer critter arg is destroyed." );
    cr->EventDead( killer );
}

void FOServer::SScriptFunc::Crit_EventRespawn( Critter* cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->EventRespawn();
}

void FOServer::SScriptFunc::Crit_EventShowCritter( Critter* cr, Critter* show_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( show_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Show critter arg is destroyed." );
    cr->EventShowCritter( show_cr );
}

void FOServer::SScriptFunc::Crit_EventShowCritter1( Critter* cr, Critter* show_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( show_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Show critter arg is destroyed." );
    cr->EventShowCritter1( show_cr );
}

void FOServer::SScriptFunc::Crit_EventShowCritter2( Critter* cr, Critter* show_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( show_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Show critter arg is destroyed." );
    cr->EventShowCritter2( show_cr );
}

void FOServer::SScriptFunc::Crit_EventShowCritter3( Critter* cr, Critter* show_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( show_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Show critter arg is destroyed." );
    cr->EventShowCritter3( show_cr );
}

void FOServer::SScriptFunc::Crit_EventHideCritter( Critter* cr, Critter* hide_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hide_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Hide critter arg is destroyed." );
    cr->EventHideCritter( hide_cr );
}

void FOServer::SScriptFunc::Crit_EventHideCritter1( Critter* cr, Critter* hide_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hide_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Hide critter arg is destroyed." );
    cr->EventHideCritter1( hide_cr );
}

void FOServer::SScriptFunc::Crit_EventHideCritter2( Critter* cr, Critter* hide_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hide_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Hide critter arg is destroyed." );
    cr->EventHideCritter2( hide_cr );
}

void FOServer::SScriptFunc::Crit_EventHideCritter3( Critter* cr, Critter* hide_cr )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hide_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Hide critter arg is destroyed." );
    cr->EventHideCritter3( hide_cr );
}

void FOServer::SScriptFunc::Crit_EventShowItemOnMap( Critter* cr, Item* show_item, bool added, Critter* dropper )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( show_item->IsDestroyed )
        SCRIPT_ERROR_R( "Show item arg is destroyed." );
    if( dropper && dropper->IsDestroyed )
        SCRIPT_ERROR_R( "Dropper critter arg is destroyed." );
    cr->EventShowItemOnMap( show_item, added, dropper );
}

void FOServer::SScriptFunc::Crit_EventChangeItemOnMap( Critter* cr, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    cr->EventChangeItemOnMap( item );
}

void FOServer::SScriptFunc::Crit_EventHideItemOnMap( Critter* cr, Item* hide_item, bool removed, Critter* picker )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hide_item->IsDestroyed )
        SCRIPT_ERROR_R( "Hide item arg is destroyed." );
    if( picker && picker->IsDestroyed )
        SCRIPT_ERROR_R( "Picker critter arg is destroyed." );
    cr->EventHideItemOnMap( hide_item, removed, picker );
}

bool FOServer::SScriptFunc::Crit_EventAttack( Critter* cr, Critter* target )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( target->IsDestroyed )
        SCRIPT_ERROR_R0( "Target critter arg is destroyed." );
    return cr->EventAttack( target );
}

bool FOServer::SScriptFunc::Crit_EventAttacked( Critter* cr, Critter* attacker )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( attacker->IsDestroyed )
        SCRIPT_ERROR_R0( "Attacker critter arg is destroyed." );
    return cr->EventAttacked( attacker );
}

bool FOServer::SScriptFunc::Crit_EventStealing( Critter* cr, Critter* thief, Item* item, uint count )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( thief->IsDestroyed )
        SCRIPT_ERROR_R0( "Thief critter arg is destroyed." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Item arg is destroyed." );
    return cr->EventStealing( thief, item, count );
}

void FOServer::SScriptFunc::Crit_EventMessage( Critter* cr, Critter* from_cr, int message, int value )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    cr->EventMessage( from_cr, message, value );
}

bool FOServer::SScriptFunc::Crit_EventUseItem( Critter* cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R0( "Item arg is destroyed." );
    if( on_critter && on_critter->IsDestroyed )
        SCRIPT_ERROR_R0( "On critter arg is destroyed." );
    if( on_item && on_item->IsDestroyed )
        SCRIPT_ERROR_R0( "On item arg is destroyed." );
    return cr->EventUseItem( item, on_critter, on_item, on_scenery );
}

bool FOServer::SScriptFunc::Crit_EventUseItemOnMe( Critter* cr, Critter* who_use, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( who_use->IsDestroyed )
        SCRIPT_ERROR_R0( "Who use critter arg is destroyed." );
    return cr->EventUseItemOnMe( who_use, item );
}

bool FOServer::SScriptFunc::Crit_EventUseSkill( Critter* cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( on_critter && on_critter->IsDestroyed )
        SCRIPT_ERROR_R0( "On critter arg is destroyed." );
    if( on_item && on_item->IsDestroyed )
        SCRIPT_ERROR_R0( "On item arg is destroyed." );
    return cr->EventUseSkill( skill, on_critter, on_item, on_scenery );
}

bool FOServer::SScriptFunc::Crit_EventUseSkillOnMe( Critter* cr, Critter* who_use, int skill )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( who_use->IsDestroyed )
        SCRIPT_ERROR_R0( "Who use critter arg is destroyed." );
    return cr->EventUseSkillOnMe( who_use, skill );
}

void FOServer::SScriptFunc::Crit_EventDropItem( Critter* cr, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    cr->EventDropItem( item );
}

void FOServer::SScriptFunc::Crit_EventMoveItem( Critter* cr, Item* item, uchar from_slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    cr->EventMoveItem( item, from_slot );
}

void FOServer::SScriptFunc::Crit_EventKnockout( Critter* cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, uint knock_dist )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    cr->EventKnockout( anim2begin, anim2idle, anim2end, lost_ap, knock_dist );
}

void FOServer::SScriptFunc::Crit_EventSmthDead( Critter* cr, Critter* from_cr, Critter* killer )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    if( killer && killer->IsDestroyed )
        SCRIPT_ERROR_R( "Killer critter arg is destroyed." );
    cr->EventSmthDead( from_cr, killer );
}

void FOServer::SScriptFunc::Crit_EventSmthStealing( Critter* cr, Critter* from_cr, Critter* thief, bool success, Item* item, uint count )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    if( thief->IsDestroyed )
        SCRIPT_ERROR_R( "Thief critter arg is destroyed." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    cr->EventSmthStealing( from_cr, thief, success, item, count );
}

void FOServer::SScriptFunc::Crit_EventSmthAttack( Critter* cr, Critter* from_cr, Critter* target )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    if( target->IsDestroyed )
        SCRIPT_ERROR_R( "Target critter arg is destroyed." );
    cr->EventSmthAttack( from_cr, target );
}

void FOServer::SScriptFunc::Crit_EventSmthAttacked( Critter* cr, Critter* from_cr, Critter* attacker )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    if( attacker->IsDestroyed )
        SCRIPT_ERROR_R( "Attacker critter arg is destroyed." );
    cr->EventSmthAttacked( from_cr, attacker );
}

void FOServer::SScriptFunc::Crit_EventSmthUseItem( Critter* cr, Critter* from_cr, Item* item, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    if( on_critter && on_critter->IsDestroyed )
        SCRIPT_ERROR_R( "On critter arg is destroyed." );
    if( on_item && on_item->IsDestroyed )
        SCRIPT_ERROR_R( "On item arg is destroyed." );
    cr->EventSmthUseItem( from_cr, item, on_critter, on_item, on_scenery );
}

void FOServer::SScriptFunc::Crit_EventSmthUseSkill( Critter* cr, Critter* from_cr, int skill, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    if( on_critter && on_critter->IsDestroyed )
        SCRIPT_ERROR_R( "On critter arg is destroyed." );
    if( on_item && on_item->IsDestroyed )
        SCRIPT_ERROR_R( "On item arg is destroyed." );
    cr->EventSmthUseSkill( from_cr, skill, on_critter, on_item, on_scenery );
}

void FOServer::SScriptFunc::Crit_EventSmthDropItem( Critter* cr, Critter* from_cr, Item* item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    cr->EventSmthDropItem( from_cr, item );
}

void FOServer::SScriptFunc::Crit_EventSmthMoveItem( Critter* cr, Critter* from_cr, Item* item, uchar from_slot )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    cr->EventSmthMoveItem( from_cr, item, from_slot );
}

void FOServer::SScriptFunc::Crit_EventSmthKnockout( Critter* cr, Critter* from_cr, uint anim2begin, uint anim2idle, uint anim2end, uint lost_ap, uint knock_dist )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter arg is destroyed." );
    cr->EventSmthKnockout( from_cr, anim2begin, anim2idle, anim2end, lost_ap, knock_dist );
}

int FOServer::SScriptFunc::Crit_EventPlaneBegin( Critter* cr, AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->EventPlaneBegin( plane, reason, some_cr, some_item );
}

int FOServer::SScriptFunc::Crit_EventPlaneEnd( Critter* cr, AIDataPlane* plane, int reason, Critter* some_cr, Item* some_item )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->EventPlaneEnd( plane, reason, some_cr, some_item );
}

int FOServer::SScriptFunc::Crit_EventPlaneRun( Critter* cr, AIDataPlane* plane, int reason, uint& p0, uint& p1, uint& p2 )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->EventPlaneRun( plane, reason, p0, p1, p2 );
}

bool FOServer::SScriptFunc::Crit_EventBarter( Critter* cr, Critter* cr_barter, bool attach, uint barter_count )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr_barter->IsDestroyed )
        SCRIPT_ERROR_R0( "Barter critter is destroyed." );
    return cr->EventBarter( cr_barter, attach, barter_count );
}

bool FOServer::SScriptFunc::Crit_EventTalk( Critter* cr, Critter* cr_talk, bool attach, uint talk_count )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr_talk->IsDestroyed )
        SCRIPT_ERROR_R0( "Barter critter is destroyed." );
    return cr->EventTalk( cr_talk, attach, talk_count );
}

bool FOServer::SScriptFunc::Crit_EventGlobalProcess( Critter* cr, int type, Item* car, float& x, float& y, float& to_x, float& to_y, float& speed, uint& encounter_descriptor, bool& wait_for_answer )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->EventGlobalProcess( type, car, x, y, to_x, to_y, speed, encounter_descriptor, wait_for_answer );
}

bool FOServer::SScriptFunc::Crit_EventGlobalInvite( Critter* cr, Item* car, uint encounter_descriptor, int combat_mode, uint& map_id, ushort& hx, ushort& hy, uchar& dir )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return cr->EventGlobalInvite( car, encounter_descriptor, combat_mode, map_id, hx, hy, dir );
}

void FOServer::SScriptFunc::Crit_EventTurnBasedProcess( Critter* cr, Map* map, bool begin_turn )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Map is destroyed." );
    cr->EventTurnBasedProcess( map, begin_turn );
}

void FOServer::SScriptFunc::Crit_EventSmthTurnBasedProcess( Critter* cr, Critter* from_cr, Map* map, bool begin_turn )
{
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter is destroyed." );
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Map is destroyed." );
    cr->EventSmthTurnBasedProcess( from_cr, map, begin_turn );
}

uint FOServer::SScriptFunc::Map_GetId( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetId();
}

hash FOServer::SScriptFunc::Map_get_ProtoId( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetPid();
}

Location* FOServer::SScriptFunc::Map_GetLocation( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetLocation( true );
}

bool FOServer::SScriptFunc::Map_SetScript( Map* map, ScriptString* script )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !script || !script->length() )
    {
        map->Data.ScriptId = 0;
    }
    else
    {
        if( !map->ParseScript( script->c_str(), true ) )
            SCRIPT_ERROR_R0( "Script function not found." );
    }
    return true;
}

hash FOServer::SScriptFunc::Map_GetScriptId( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->Data.ScriptId;
}

bool FOServer::SScriptFunc::Map_SetEvent( Map* map, int event_type, ScriptString* func_name )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( event_type < 0 || event_type >= MAP_EVENT_MAX )
        SCRIPT_ERROR_R0( "Invalid event type arg." );
    if( !func_name || !func_name->length() )
        map->FuncId[ event_type ] = 0;
    else
        map->FuncId[ event_type ] = Script::Bind( func_name->c_str(), MapEventFuncName[ event_type ], false );

    if( event_type >= MAP_EVENT_LOOP_0 && event_type <= MAP_EVENT_LOOP_4 )
    {
        map->NeedProcess = false;
        for( int i = 0; i < MAP_LOOP_FUNC_MAX; i++ )
        {
            if( !map->FuncId[ MAP_EVENT_LOOP_0 + i ] )
            {
                map->LoopEnabled[ i ] = false;
            }
            else
            {
                if( event_type == MAP_EVENT_LOOP_0 + i )
                    map->LoopLastTick[ i ] = Timer::GameTick();
                map->LoopEnabled[ i ] = true;
                map->NeedProcess = true;
            }
        }
    }

    if( func_name && func_name->length() && map->FuncId[ event_type ] <= 0 )
        SCRIPT_ERROR_R0( "Function not found." );
    return true;
}

void FOServer::SScriptFunc::Map_SetLoopTime( Map* map, uint loop_num, uint ms )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( loop_num >= MAP_LOOP_FUNC_MAX )
        SCRIPT_ERROR_R( "Invalid loop number arg." );
    map->SetLoopTime( loop_num, ms );
}

uchar FOServer::SScriptFunc::Map_GetRain( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetRain();
}

void FOServer::SScriptFunc::Map_SetRain( Map* map, uchar capacity )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->SetRain( capacity );
}

int FOServer::SScriptFunc::Map_GetTime( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetTime();
}

void FOServer::SScriptFunc::Map_SetTime( Map* map, int time )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->SetTime( time );
}

uint FOServer::SScriptFunc::Map_GetDayTime( Map* map, uint day_part )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( day_part >= 4 )
        SCRIPT_ERROR_R0( "Invalid day part arg." );
    return map->GetDayTime( day_part );
}

void FOServer::SScriptFunc::Map_SetDayTime( Map* map, uint day_part, uint time )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( day_part >= 4 )
        SCRIPT_ERROR_R( "Invalid day part arg." );
    if( time >= 1440 )
        SCRIPT_ERROR_R( "Invalid time arg." );
    map->SetDayTime( day_part, time );
}

void FOServer::SScriptFunc::Map_GetDayColor( Map* map, uint day_part, uchar& r, uchar& g, uchar& b )
{
    r = g = b = 0;
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( day_part >= 4 )
        SCRIPT_ERROR_R( "Invalid day part arg." );
    map->GetDayColor( day_part, r, g, b );
}

void FOServer::SScriptFunc::Map_SetDayColor( Map* map, uint day_part, uchar r, uchar g, uchar b )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( day_part >= 4 )
        SCRIPT_ERROR_R( "Invalid day part arg." );
    map->SetDayColor( day_part, r, g, b );
}

void FOServer::SScriptFunc::Map_SetTurnBasedAvailability( Map* map, bool value )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->Data.IsTurnBasedAviable = value;
}

bool FOServer::SScriptFunc::Map_IsTurnBasedAvailability( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->Data.IsTurnBasedAviable;
}

void FOServer::SScriptFunc::Map_BeginTurnBased( Map* map, Critter* first_turn_crit )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( first_turn_crit && first_turn_crit->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is not valid." );
    map->BeginTurnBased( first_turn_crit );
}

bool FOServer::SScriptFunc::Map_IsTurnBased( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->IsTurnBasedOn;
}

void FOServer::SScriptFunc::Map_EndTurnBased( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->NeedEndTurnBased = true;
}

int FOServer::SScriptFunc::Map_GetTurnBasedSequence( Map* map, ScriptArray& critters_ids )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !map->IsTurnBasedOn )
        SCRIPT_ERROR_R0( "Map is not in turn based state." );
    Script::AppendVectorToArray( map->TurnSequence, &critters_ids );
    return ( map->TurnSequenceCur >= 0 && map->TurnSequenceCur < (int) map->TurnSequence.size() ) ? map->TurnSequenceCur : -1;
}

void FOServer::SScriptFunc::Map_SetData( Map* map, uint index, int value )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( index >= MAP_MAX_DATA )
        SCRIPT_ERROR_R( "Index arg invalid value." );
    map->SetData( index, value );
}

int FOServer::SScriptFunc::Map_GetData( Map* map, uint index )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( index >= MAP_MAX_DATA )
        SCRIPT_ERROR_R0( "Index arg invalid value." );
    return map->GetData( index );
}

Item* FOServer::SScriptFunc::Map_AddItem( Map* map, ushort hx, ushort hy, hash proto_id, uint count )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    ProtoItem* proto_item = ItemMngr.GetProtoItem( proto_id );
    if( !proto_item )
        SCRIPT_ERROR_R0( "Invalid proto '%s' arg.", HASH_STR( proto_id ) );
    if( proto_item->IsBlockLines() && !map->IsPlaceForItem( hx, hy, proto_item ) )
        SCRIPT_ERROR_R0( "No place for item." );
    if( !count )
        count = 1;
    return CreateItemOnHex( map, hx, hy, proto_id, count );
}

uint FOServer::SScriptFunc::Map_GetItemsHex( Map* map, ushort hx, ushort hy, ScriptArray* items )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    ItemVec items_;
    map->GetItemsHex( hx, hy, items_, items != NULL );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

uint FOServer::SScriptFunc::Map_GetItemsHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid, ScriptArray* items )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    ItemVec items_;
    map->GetItemsHexEx( hx, hy, radius, pid, items_, items != NULL );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

uint FOServer::SScriptFunc::Map_GetItemsByPid( Map* map, hash pid, ScriptArray* items )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ItemVec items_;
    map->GetItemsPid( pid, items_, items != NULL );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

uint FOServer::SScriptFunc::Map_GetItemsByType( Map* map, int type, ScriptArray* items )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ItemVec items_;
    map->GetItemsType( type, items_, items != NULL );
    if( items )
        Script::AppendVectorToArrayRef< Item* >( items_, items );
    return (uint) items_.size();
}

Item* FOServer::SScriptFunc::Map_GetItem( Map* map, uint item_id )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !item_id )
        SCRIPT_ERROR_R0( "Item id arg is zero." );
    return map->GetItem( item_id );
}

Item* FOServer::SScriptFunc::Map_GetItemHex( Map* map, ushort hx, ushort hy, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    return map->GetItemHex( hx, hy, pid, NULL );
}

Critter* FOServer::SScriptFunc::Map_GetCritterHex( Map* map, ushort hx, ushort hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    Critter* cr = map->GetHexCritter( hx, hy, false, true );
    if( !cr )
        cr = map->GetHexCritter( hx, hy, true, true );
    return cr;
}

Item* FOServer::SScriptFunc::Map_GetDoor( Map* map, ushort hx, ushort hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    return map->GetItemDoor( hx, hy );
}

Item* FOServer::SScriptFunc::Map_GetCar( Map* map, ushort hx, ushort hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    return map->GetItemCar( hx, hy );
}

MapObject* FOServer::SScriptFunc::Map_GetSceneryHex( Map* map, ushort hx, ushort hy, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    return map->Proto->GetMapScenery( hx, hy, pid );
}

uint FOServer::SScriptFunc::Map_GetSceneriesHex( Map* map, ushort hx, ushort hy, ScriptArray* sceneries )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    MapObjectPtrVec mobjs;
    map->Proto->GetMapSceneriesHex( hx, hy, mobjs );
    if( !mobjs.size() )
        return 0;
    if( sceneries )
        Script::AppendVectorToArrayRef( mobjs, sceneries );
    return (uint) mobjs.size();
}

uint FOServer::SScriptFunc::Map_GetSceneriesHexEx( Map* map, ushort hx, ushort hy, uint radius, hash pid, ScriptArray* sceneries )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    MapObjectPtrVec mobjs;
    map->Proto->GetMapSceneriesHexEx( hx, hy, radius, pid, mobjs );
    if( !mobjs.size() )
        return 0;
    if( sceneries )
        Script::AppendVectorToArrayRef( mobjs, sceneries );
    return (uint) mobjs.size();
}

uint FOServer::SScriptFunc::Map_GetSceneriesByPid( Map* map, hash pid, ScriptArray* sceneries )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    MapObjectPtrVec mobjs;
    map->Proto->GetMapSceneriesByPid( pid, mobjs );
    if( !mobjs.size() )
        return 0;
    if( sceneries )
        Script::AppendVectorToArrayRef( mobjs, sceneries );
    return (uint) mobjs.size();
}

Critter* FOServer::SScriptFunc::Map_GetCritterById( Map* map, uint crid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    Critter* cr = map->GetCritter( crid, true );
    // if(!cr) SCRIPT_ERROR_R0("Critter not found.");
    return cr;
}

uint FOServer::SScriptFunc::Map_GetCritters( Map* map, ushort hx, ushort hy, uint radius, int find_type, ScriptArray* critters )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    CrVec cr_vec;
    map->GetCrittersHex( hx, hy, radius, find_type, cr_vec, true /*critters!=NULL*/ );
    if( critters )
    {
        SortCritterByDist( hx, hy, cr_vec );
        Script::AppendVectorToArrayRef< Critter* >( cr_vec, critters );
    }
    return (uint) cr_vec.size();
}

uint FOServer::SScriptFunc::Map_GetCrittersByPids( Map* map, hash pid, int find_type, ScriptArray* critters )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec cr_vec;
    if( !pid )
    {
        CrVec map_critters;
        map->GetCritters( map_critters, true );
        cr_vec.reserve( map_critters.size() );
        for( auto it = map_critters.begin(), end = map_critters.end(); it != end; ++it )
        {
            Critter* cr = *it;
            if( cr->CheckFind( find_type ) )
                cr_vec.push_back( cr );
        }
    }
    else
    {
        PcVec map_npcs;
        map->GetNpcs( map_npcs, true );
        cr_vec.reserve( map_npcs.size() );
        for( auto it = map_npcs.begin(), end = map_npcs.end(); it != end; ++it )
        {
            Npc* npc = *it;
            if( npc->Data.ProtoId == pid && npc->CheckFind( find_type ) )
                cr_vec.push_back( npc );
        }
    }

    if( critters )
    {
        // for(auto it=cr_vec.begin(),end=cr_vec.end();it!=end;++it) SYNC_LOCK(*it);
        Script::AppendVectorToArrayRef< Critter* >( cr_vec, critters );
    }
    return (uint) cr_vec.size();
}

uint FOServer::SScriptFunc::Map_GetCrittersInPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ScriptArray* critters )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    CrVec     cr_vec;
    TraceData trace;
    trace.TraceMap = map;
    trace.BeginHx = from_hx;
    trace.BeginHy = from_hy;
    trace.EndHx = to_hx;
    trace.EndHy = to_hy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &cr_vec;
    trace.FindType = find_type;
    MapMngr.TraceBullet( trace );
    if( critters )
    {
        for( auto it = cr_vec.begin(), end = cr_vec.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Critter* >( cr_vec, critters );
    }
    return (uint) cr_vec.size();
}

uint FOServer::SScriptFunc::Map_GetCrittersInPathBlock( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, float angle, uint dist, int find_type, ScriptArray* critters, ushort& pre_block_hx, ushort& pre_block_hy, ushort& block_hx, ushort& block_hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    CrVec      cr_vec;
    UShortPair block, pre_block;
    TraceData  trace;
    trace.TraceMap = map;
    trace.BeginHx = from_hx;
    trace.BeginHy = from_hy;
    trace.EndHx = to_hx;
    trace.EndHy = to_hy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.Critters = &cr_vec;
    trace.FindType = find_type;
    trace.PreBlock = &pre_block;
    trace.Block = &block;
    MapMngr.TraceBullet( trace );
    if( critters )
    {
        for( auto it = cr_vec.begin(), end = cr_vec.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Critter* >( cr_vec, critters );
    }
    pre_block_hx = pre_block.first;
    pre_block_hy = pre_block.second;
    block_hx = block.first;
    block_hy = block.second;
    return (uint) cr_vec.size();
}

uint FOServer::SScriptFunc::Map_GetCrittersWhoViewPath( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, int find_type, ScriptArray* critters )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec cr_vec;
    if( critters )
        Script::AssignScriptArrayInVector< Critter* >( cr_vec, critters );

    CrVec map_critters;
    map->GetCritters( map_critters, true );
    for( auto it = map_critters.begin(), end = map_critters.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( cr->CheckFind( find_type ) &&
            std::find( cr_vec.begin(), cr_vec.end(), cr ) == cr_vec.end() &&
            IntersectCircleLine( cr->GetHexX(), cr->GetHexY(), cr->GetLookDistance(), from_hx, from_hy, to_hx, to_hy ) )
            cr_vec.push_back( cr );
    }

    if( critters )
    {
        // for(auto it=cr_vec.begin(),end=cr_vec.end();it!=end;++it) SYNC_LOCK(*it);
        Script::AppendVectorToArrayRef< Critter* >( cr_vec, critters );
    }
    return (uint) cr_vec.size();
}

uint FOServer::SScriptFunc::Map_GetCrittersSeeing( Map* map, ScriptArray& critters, bool look_on_them, int find_type, ScriptArray* result_critters )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    CrVec cr_vec;
    Script::AssignScriptArrayInVector< Critter* >( cr_vec, &critters );

    for( int i = 0, j = critters.GetSize(); i < j; i++ )
    {
        Critter* cr = *(Critter**) critters.At( i );
        cr->GetCrFromVisCr( cr_vec, find_type, !look_on_them, true );
    }

    if( result_critters )
        Script::AppendVectorToArrayRef< Critter* >( cr_vec, result_critters );
    return (uint) cr_vec.size();
}

void FOServer::SScriptFunc::Map_GetHexInPath( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    UShortPair pre_block, block;
    TraceData  trace;
    trace.TraceMap = map;
    trace.BeginHx = from_hx;
    trace.BeginHy = from_hy;
    trace.EndHx = to_hx;
    trace.EndHy = to_hy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.PreBlock = &pre_block;
    trace.Block = &block;
    MapMngr.TraceBullet( trace );
    to_hx = pre_block.first;
    to_hy = pre_block.second;
}

void FOServer::SScriptFunc::Map_GetHexInPathWall( Map* map, ushort from_hx, ushort from_hy, ushort& to_hx, ushort& to_hy, float angle, uint dist )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    UShortPair last_passed;
    TraceData  trace;
    trace.TraceMap = map;
    trace.BeginHx = from_hx;
    trace.BeginHy = from_hy;
    trace.EndHx = to_hx;
    trace.EndHy = to_hy;
    trace.Dist = dist;
    trace.Angle = angle;
    trace.LastPassed = &last_passed;
    MapMngr.TraceBullet( trace );
    if( trace.IsHaveLastPassed )
    {
        to_hx = last_passed.first;
        to_hy = last_passed.second;
    }
    else
    {
        to_hx = from_hx;
        to_hy = from_hy;
    }
}

uint FOServer::SScriptFunc::Map_GetPathLengthHex( Map* map, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy, uint cut )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( from_hx >= map->GetMaxHexX() || from_hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid from hexes args." );
    if( to_hx >= map->GetMaxHexX() || to_hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    PathFindData pfd;
    pfd.Clear();
    pfd.MapId = map->GetId();
    pfd.FromX = from_hx;
    pfd.FromY = from_hy;
    pfd.ToX = to_hx;
    pfd.ToY = to_hy;
    pfd.Cut = cut;
    uint result = MapMngr.FindPath( pfd );
    if( result != FPATH_OK )
        return 0;
    PathStepVec& path = MapMngr.GetPath( pfd.PathNum );
    return (uint) path.size();
}

uint FOServer::SScriptFunc::Map_GetPathLengthCr( Map* map, Critter* cr, ushort to_hx, ushort to_hy, uint cut )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );
    if( to_hx >= map->GetMaxHexX() || to_hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid to hexes args." );

    PathFindData pfd;
    pfd.Clear();
    pfd.MapId = map->GetId();
    pfd.FromCritter = cr;
    pfd.FromX = cr->GetHexX();
    pfd.FromY = cr->GetHexY();
    pfd.ToX = to_hx;
    pfd.ToY = to_hy;
    pfd.Multihex = cr->GetMultihex();
    pfd.Cut = cut;
    uint result = MapMngr.FindPath( pfd );
    if( result != FPATH_OK )
        return 0;
    PathStepVec& path = MapMngr.GetPath( pfd.PathNum );
    return (uint) path.size();
}

Critter* FOServer::SScriptFunc::Map_AddNpc( Map* map, hash proto_id, ushort hx, ushort hy, uchar dir, ScriptArray* params, ScriptArray* items, ScriptString* script )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( params && params->GetSize() & 1 )
        SCRIPT_ERROR_R0( "Invalid params array size (%u).", items->GetSize() );
    if( items && items->GetSize() % 3 )
        SCRIPT_ERROR_R0( "Invalid items array size (%u).", items->GetSize() );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    if( !CrMngr.GetProto( proto_id ) )
        SCRIPT_ERROR_R0( "Proto '%s' not found.", HASH_STR( proto_id ) );

    IntVec params_;
    if( params )
    {
        Script::AssignScriptArrayInVector( params_, params );
        for( size_t i = 0, j = params_.size() / 2; i < j; i++ )
        {
            int       enum_value = params_[ i * 2 ];
            Property* prop = Critter::PropertiesRegistrator->FindByEnum( enum_value );
            if( !prop )
                SCRIPT_ERROR_R0( "Some property not found" );
            if( !prop->IsPOD() )
                SCRIPT_ERROR_R0( "Some property is not POD" );
        }
    }

    IntVec items_;
    if( items )
    {
        Script::AssignScriptArrayInVector( items_, items );
        for( uint i = 0, j = (uint) items_.size(); i < j; i += 3 )
        {
            int pid = items_[ i ];
            int count = items_[ i + 1 ];
            int slot = items_[ i + 2 ];
            if( pid && !ItemMngr.GetProtoItem( pid ) )
                SCRIPT_ERROR_R0( "Invalid item pid value." );
            if( count < 0 )
                SCRIPT_ERROR_R0( "Invalid items count value." );
            if( slot < 0 || slot >= 0xFF || !Critter::SlotEnabled[ slot ] )
                SCRIPT_ERROR_R0( "Invalid items slot value." );
        }
    }

    Critter* npc = CrMngr.CreateNpc( proto_id, &params_, &items_, script && script->length() ? script->c_str() : NULL, map, hx, hy, dir, false );
    if( !npc )
        SCRIPT_ERROR_R0( "Create npc fail." );
    return npc;
}

uint FOServer::SScriptFunc::Map_GetNpcCount( Map* map, int npc_role, int find_type )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetNpcCount( npc_role, find_type );
}

Critter* FOServer::SScriptFunc::Map_GetNpc( Map* map, int npc_role, int find_type, uint skip_count )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetNpc( npc_role, find_type, skip_count, true );
}

uint FOServer::SScriptFunc::Map_CountEntire( Map* map, int entire )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->Proto->CountEntire( entire );
}

uint FOServer::SScriptFunc::Map_GetEntires( Map* map, int entire, ScriptArray* entires, ScriptArray* hx, ScriptArray* hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoMap::EntiresVec entires_;
    map->Proto->GetEntires( entire, entires_ );
    if( entires_.empty() )
        return 0;
    if( entires )
    {
        uint size = entires->GetSize();
        entires->Resize( size + (uint) entires_.size() );
        for( uint i = 0, j = (uint) entires_.size(); i < j; i++ )
            *(int*) entires->At( size + i ) = entires_[ i ].Number;
    }
    if( hx )
    {
        uint size = hx->GetSize();
        hx->Resize( size + (uint) entires_.size() );
        for( uint i = 0, j = (uint) entires_.size(); i < j; i++ )
            *(ushort*) hx->At( size + i ) = entires_[ i ].HexX;
    }
    if( hy )
    {
        uint size = hy->GetSize();
        hy->Resize( size + (uint) entires_.size() );
        for( uint i = 0, j = (uint) entires_.size(); i < j; i++ )
            *(ushort*) hy->At( size + i ) = entires_[ i ].HexY;
    }
    return (uint) entires_.size();
}

bool FOServer::SScriptFunc::Map_GetEntireCoords( Map* map, int entire, uint skip, ushort& hx, ushort& hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoMap::MapEntire* e = map->Proto->GetEntire( entire, skip );
    if( !e )
        return false;        // SCRIPT_ERROR_R0("Entire not found.");
    hx = e->HexX;
    hy = e->HexY;
    return true;
}

bool FOServer::SScriptFunc::Map_GetEntireCoordsDir( Map* map, int entire, uint skip, ushort& hx, ushort& hy, uchar& dir )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoMap::MapEntire* e = map->Proto->GetEntire( entire, skip );
    if( !e )
        return false;        // SCRIPT_ERROR_R0("Entire not found.");
    hx = e->HexX;
    hy = e->HexY;
    dir = e->Dir;
    return true;
}

bool FOServer::SScriptFunc::Map_GetNearEntireCoords( Map* map, int& entire, ushort& hx, ushort& hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoMap::MapEntire* near_entire = map->Proto->GetEntireNear( entire, hx, hy );
    if( near_entire )
    {
        entire = near_entire->Number;
        hx = near_entire->HexX;
        hy = near_entire->HexY;
        return true;
    }
    return false;
}

bool FOServer::SScriptFunc::Map_GetNearEntireCoordsDir( Map* map, int& entire, ushort& hx, ushort& hy, uchar& dir )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoMap::MapEntire* near_entire = map->Proto->GetEntireNear( entire, hx, hy );
    if( near_entire )
    {
        entire = near_entire->Number;
        hx = near_entire->HexX;
        hy = near_entire->HexY;
        dir = near_entire->Dir;
        return true;
    }
    return false;
}

bool FOServer::SScriptFunc::Map_IsHexPassed( Map* map, ushort hex_x, ushort hex_y )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetMaxHexX() || hex_y >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    return map->IsHexPassed( hex_x, hex_y );
}

bool FOServer::SScriptFunc::Map_IsHexRaked( Map* map, ushort hex_x, ushort hex_y )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetMaxHexX() || hex_y >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    return map->IsHexRaked( hex_x, hex_y );
}

void FOServer::SScriptFunc::Map_SetText( Map* map, ushort hex_x, ushort hex_y, uint color, ScriptString& text )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetMaxHexX() || hex_y >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    map->SetText( hex_x, hex_y, color, text.c_str(), (uint) text.c_std_str().length(), 0, false );
}

void FOServer::SScriptFunc::Map_SetTextMsg( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetMaxHexX() || hex_y >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    map->SetTextMsg( hex_x, hex_y, color, text_msg, str_num );
}

void FOServer::SScriptFunc::Map_SetTextMsgLex( Map* map, ushort hex_x, ushort hex_y, uint color, ushort text_msg, uint str_num, ScriptString& lexems )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hex_x >= map->GetMaxHexX() || hex_y >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    map->SetTextMsgLex( hex_x, hex_y, color, text_msg, str_num, lexems.c_str(), (uint) lexems.length() );
}

void FOServer::SScriptFunc::Map_RunEffect( Map* map, hash eff_pid, ushort hx, ushort hy, uint radius )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !eff_pid )
        SCRIPT_ERROR_R( "Effect pid invalid arg." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    map->SendEffect( eff_pid, hx, hy, radius );
}

void FOServer::SScriptFunc::Map_RunFlyEffect( Map* map, hash eff_pid, Critter* from_cr, Critter* to_cr, ushort from_hx, ushort from_hy, ushort to_hx, ushort to_hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( !eff_pid )
        SCRIPT_ERROR_R( "Effect pid invalid arg." );
    if( from_hx >= map->GetMaxHexX() || from_hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid from hexes args." );
    if( to_hx >= map->GetMaxHexX() || to_hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid to hexes args." );
    if( from_cr && from_cr->IsDestroyed )
        SCRIPT_ERROR_R( "From critter is destroyed." );
    if( to_cr && to_cr->IsDestroyed )
        SCRIPT_ERROR_R( "To critter is destroyed." );
    uint from_crid = ( from_cr ? from_cr->GetId() : 0 );
    uint to_crid = ( to_cr ? to_cr->GetId() : 0 );
    map->SendFlyEffect( eff_pid, from_crid, to_crid, from_hx, from_hy, to_hx, to_hy );
}

bool FOServer::SScriptFunc::Map_CheckPlaceForItem( Map* map, ushort hx, ushort hy, hash pid )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
    if( !proto_item )
        SCRIPT_ERROR_R0( "Proto item not found." );
    return map->IsPlaceForItem( hx, hy, proto_item );
}

void FOServer::SScriptFunc::Map_BlockHex( Map* map, ushort hx, ushort hy, bool full )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    map->SetHexFlag( hx, hy, FH_BLOCK_ITEM );
    if( full )
        map->SetHexFlag( hx, hy, FH_NRAKE_ITEM );
}

void FOServer::SScriptFunc::Map_UnblockHex( Map* map, ushort hx, ushort hy )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexes args." );
    map->UnsetHexFlag( hx, hy, FH_BLOCK_ITEM );
    map->UnsetHexFlag( hx, hy, FH_NRAKE_ITEM );
}

void FOServer::SScriptFunc::Map_PlaySound( Map* map, ScriptString& sound_name )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );

    char sound_name_[ 100 ];
    Str::Copy( sound_name_, sound_name.c_str() );

    ClVec players;
    map->GetPlayers( players, false );
    for( auto it = players.begin(), end = players.end(); it != end; ++it )
    {
        Critter* cr = *it;
        cr->Send_PlaySound( 0, sound_name_ );
    }
}

void FOServer::SScriptFunc::Map_PlaySoundRadius( Map* map, ScriptString& sound_name, ushort hx, ushort hy, uint radius )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexes args." );

    char sound_name_[ 100 ];
    Str::Copy( sound_name_, sound_name.c_str() );

    ClVec players;
    map->GetPlayers( players, false );
    for( auto it = players.begin(), end = players.end(); it != end; ++it )
    {
        Critter* cr = *it;
        if( CheckDist( hx, hy, cr->GetHexX(), cr->GetHexY(), radius == 0 ? cr->LookCacheValue : radius ) )
            cr->Send_PlaySound( 0, sound_name_ );
    }
}

bool FOServer::SScriptFunc::Map_Reload( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( !RegenerateMap( map ) )
        SCRIPT_ERROR_R0( "Reload map fail." );
    return true;
}

ushort FOServer::SScriptFunc::Map_GetWidth( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetMaxHexX();
}

ushort FOServer::SScriptFunc::Map_GetHeight( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return map->GetMaxHexY();
}

void FOServer::SScriptFunc::Map_MoveHexByDir( Map* map, ushort& hx, ushort& hy, uchar dir, uint steps )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( dir >= DIRS_COUNT )
        SCRIPT_ERROR_R( "Invalid dir arg." );
    if( !steps )
        SCRIPT_ERROR_R( "Steps arg is zero." );
    ushort maxhx = map->GetMaxHexX();
    ushort maxhy = map->GetMaxHexY();
    if( steps > 1 )
    {
        for( uint i = 0; i < steps; i++ )
            MoveHexByDir( hx, hy, dir, maxhx, maxhy );
    }
    else
    {
        MoveHexByDir( hx, hy, dir, maxhx, maxhy );
    }
}

bool FOServer::SScriptFunc::Map_VerifyTrigger( Map* map, Critter* cr, ushort hx, ushort hy, uchar dir )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter arg is destroyed." );
    if( hx >= map->GetMaxHexX() || hy >= map->GetMaxHexY() )
        SCRIPT_ERROR_R0( "Invalid hexes args." );
    if( dir >= DIRS_COUNT )
        SCRIPT_ERROR_R0( "Invalid dir arg." );
    ushort from_hx = hx, from_hy = hy;
    MoveHexByDir( from_hx, from_hy, ReverseDir( dir ), map->GetMaxHexX(), map->GetMaxHexY() );
    return VerifyTrigger( map, cr, from_hx, from_hy, hx, hy, dir );
}

void FOServer::SScriptFunc::Map_EventFinish( Map* map, bool deleted )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->EventFinish( deleted );
}

void FOServer::SScriptFunc::Map_EventLoop0( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->EventLoop( 0 );
}

void FOServer::SScriptFunc::Map_EventLoop1( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->EventLoop( 1 );
}

void FOServer::SScriptFunc::Map_EventLoop2( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->EventLoop( 2 );
}

void FOServer::SScriptFunc::Map_EventLoop3( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->EventLoop( 3 );
}

void FOServer::SScriptFunc::Map_EventLoop4( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->EventLoop( 4 );
}

void FOServer::SScriptFunc::Map_EventInCritter( Map* map, Critter* cr )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    map->EventInCritter( cr );
}

void FOServer::SScriptFunc::Map_EventOutCritter( Map* map, Critter* cr )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    map->EventOutCritter( cr );
}

void FOServer::SScriptFunc::Map_EventCritterDead( Map* map, Critter* cr, Critter* killer )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    if( killer && killer->IsDestroyed )
        SCRIPT_ERROR_R( "Killer arg is destroyed." );
    map->EventCritterDead( cr, killer );
}

void FOServer::SScriptFunc::Map_EventTurnBasedBegin( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->EventTurnBasedBegin();
}

void FOServer::SScriptFunc::Map_EventTurnBasedEnd( Map* map )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    map->EventTurnBasedEnd();
}

void FOServer::SScriptFunc::Map_EventTurnBasedProcess( Map* map, Critter* cr, bool begin_turn )
{
    if( map->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    if( cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    map->EventTurnBasedProcess( cr, begin_turn );
}

hash FOServer::SScriptFunc::Location_get_ProtoId( Location* loc )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return loc->GetPid();
}

bool FOServer::SScriptFunc::Location_SetEvent( Location* loc, int event_type, ScriptString* func_name )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Location is destroyed." );
    if( event_type < 0 || event_type >= LOCATION_EVENT_MAX )
        SCRIPT_ERROR_R0( "Invalid event type arg for location." );
    if( !func_name || !func_name->length() )
        loc->FuncId[ event_type ] = 0;
    else
        loc->FuncId[ event_type ] = Script::Bind( func_name->c_str(), LocationEventFuncName[ event_type ], false );

    if( func_name && func_name->length() && !loc->FuncId[ event_type ] )
        SCRIPT_ERROR_R0( "Function not found." );
    return true;
}

uint FOServer::SScriptFunc::Location_GetMapCount( Location* loc )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return loc->GetMapsCount();
}

Map* FOServer::SScriptFunc::Location_GetMap( Location* loc, hash map_pid )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    MapVec& maps = loc->GetMapsNoLock();
    for( auto it = maps.begin(), end = maps.end(); it != end; ++it )
    {
        Map* map = *it;
        if( map->GetPid() == map_pid )
        {
            SYNC_LOCK( map );
            return map;
        }
    }
    return NULL;
}

Map* FOServer::SScriptFunc::Location_GetMapByIndex( Location* loc, uint index )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    MapVec& maps = loc->GetMapsNoLock();
    if( index >= maps.size() )
        SCRIPT_ERROR_R0( "Invalid index arg." );
    Map* map = maps[ index ];
    SYNC_LOCK( map );
    return map;
}

uint FOServer::SScriptFunc::Location_GetMaps( Location* loc, ScriptArray* maps )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    MapVec maps_;
    loc->GetMaps( maps_, maps != NULL );
    if( maps )
        Script::AppendVectorToArrayRef< Map* >( maps_, maps );
    return (uint) maps_.size();
}

bool FOServer::SScriptFunc::Location_GetEntrance( Location* loc, uint entrance, uint& map_index, uint& entire )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    if( entrance >= loc->Proto->Entrance.size() )
        SCRIPT_ERROR_R0( "Invalid entrance." );

    map_index = loc->Proto->Entrance[ entrance ].first;
    entire = loc->Proto->Entrance[ entrance ].second;

    return true;
}

uint FOServer::SScriptFunc::Location_GetEntrances( Location* loc, ScriptArray* maps_index, ScriptArray* entires )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );

    uint size = (uint) loc->Proto->Entrance.size();

    if( maps_index || entires )
    {
        for( uint e = 0; e < size; e++ )
        {
            if( maps_index )
                maps_index->InsertLast( &loc->Proto->Entrance[ e ].first );
            if( entires )
                entires->InsertLast( &loc->Proto->Entrance[ e ].second );
        }
    }

    return size;
}

bool FOServer::SScriptFunc::Location_Reload( Location* loc )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    MapVec maps;
    loc->GetMaps( maps, true );
    for( auto it = maps.begin(), end = maps.end(); it != end; ++it )
    {
        Map* map = *it;
        if( !RegenerateMap( map ) )
            SCRIPT_ERROR_R0( "Reload map in location fail." );
    }
    return true;
}

void FOServer::SScriptFunc::Location_Update( Location* loc )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    loc->Update();
}

void FOServer::SScriptFunc::Location_EventFinish( Location* loc, bool deleted )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R( "Attempt to call method on destroyed object." );
    loc->EventFinish( deleted );
}

bool FOServer::SScriptFunc::Location_EventEnter( Location* loc, ScriptArray& group, uchar entrance )
{
    if( loc->IsDestroyed )
        SCRIPT_ERROR_R0( "Attempt to call method on destroyed object." );
    return loc->EventEnter( &group, entrance );
}

uint FOServer::SScriptFunc::Global_GetCrittersDistantion( Critter* cr1, Critter* cr2 )
{
    if( cr1->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter1 arg is destroyed." );
    if( cr2->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter2 arg is destroyed." );
    if( cr1->GetMapId() != cr2->GetMapId() )
        SCRIPT_ERROR_R0( "Differernt maps." );
    return DistGame( cr1->GetHexX(), cr1->GetHexY(), cr2->GetHexX(), cr2->GetHexY() );
}

ProtoItem* FOServer::SScriptFunc::Global_GetProtoItem( hash pid )
{
    return ItemMngr.GetProtoItem( pid );
}

Item* FOServer::SScriptFunc::Global_GetItem( uint item_id )
{
    if( !item_id )
        SCRIPT_ERROR_R0( "Item id arg is zero." );
    Item* item = ItemMngr.GetItem( item_id, true );
    if( !item || item->IsDestroyed )
        return NULL;
    return item;
}

void FOServer::SScriptFunc::Global_MoveItemCr( Item* item, uint count, Critter* to_cr )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    if( to_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    if( !count )
        count = item->GetCount();
    if( count > item->GetCount() )
        SCRIPT_ERROR_R( "Count arg is greater than maximum." );
    ItemMngr.MoveItem( item, count, to_cr );
}

void FOServer::SScriptFunc::Global_MoveItemMap( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    if( to_map->IsDestroyed )
        SCRIPT_ERROR_R( "Container arg is destroyed." );
    if( to_hx >= to_map->GetMaxHexX() || to_hy >= to_map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexex args." );
    if( !count )
        count = item->GetCount();
    if( count > item->GetCount() )
        SCRIPT_ERROR_R( "Count arg is greater than maximum." );
    ItemMngr.MoveItem( item, count, to_map, to_hx, to_hy );
}

void FOServer::SScriptFunc::Global_MoveItemCont( Item* item, uint count, Item* to_cont, uint stack_id )
{
    if( item->IsDestroyed )
        SCRIPT_ERROR_R( "Item arg is destroyed." );
    if( to_cont->IsDestroyed )
        SCRIPT_ERROR_R( "Container arg is destroyed." );
    if( !to_cont->IsContainer() )
        SCRIPT_ERROR_R( "Container arg is not container type." );
    if( !count )
        count = item->GetCount();
    if( count > item->GetCount() )
        SCRIPT_ERROR_R( "Count arg is greater than maximum." );
    ItemMngr.MoveItem( item, count, to_cont, stack_id );
}

void FOServer::SScriptFunc::Global_MoveItemsCr( ScriptArray& items, Critter* to_cr )
{
    if( to_cr->IsDestroyed )
        SCRIPT_ERROR_R( "Critter arg is destroyed." );
    for( int i = 0, j = items.GetSize(); i < j; i++ )
    {
        Item* item = *(Item**) items.At( i );
        if( !item || item->IsDestroyed )
            continue;
        ItemMngr.MoveItem( item, item->GetCount(), to_cr );
    }
}

void FOServer::SScriptFunc::Global_MoveItemsMap( ScriptArray& items, Map* to_map, ushort to_hx, ushort to_hy )
{
    if( to_map->IsDestroyed )
        SCRIPT_ERROR_R( "Container arg is destroyed." );
    if( to_hx >= to_map->GetMaxHexX() || to_hy >= to_map->GetMaxHexY() )
        SCRIPT_ERROR_R( "Invalid hexex args." );
    for( int i = 0, j = items.GetSize(); i < j; i++ )
    {
        Item* item = *(Item**) items.At( i );
        if( !item || item->IsDestroyed )
            continue;
        ItemMngr.MoveItem( item, item->GetCount(), to_map, to_hx, to_hy );
    }
}

void FOServer::SScriptFunc::Global_MoveItemsCont( ScriptArray& items, Item* to_cont, uint stack_id )
{
    if( to_cont->IsDestroyed )
        SCRIPT_ERROR_R( "Container arg is destroyed." );
    if( !to_cont->IsContainer() )
        SCRIPT_ERROR_R( "Container arg is not container type." );
    for( int i = 0, j = items.GetSize(); i < j; i++ )
    {
        Item* item = *(Item**) items.At( i );
        if( !item || item->IsDestroyed )
            continue;
        ItemMngr.MoveItem( item, item->GetCount(), to_cont, stack_id );
    }
}

void FOServer::SScriptFunc::Global_DeleteItem( Item* item )
{
    ItemMngr.DeleteItem( item );
}

void FOServer::SScriptFunc::Global_DeleteItemById( uint item_id )
{
    Item* item = ItemMngr.GetItem( item_id, false );
    if( item )
        ItemMngr.DeleteItem( item );
}

void FOServer::SScriptFunc::Global_DeleteItems( ScriptArray& items )
{
    for( int i = 0, j = items.GetSize(); i < j; i++ )
    {
        Item* item = *(Item**) items.At( i );
        if( item )
            ItemMngr.DeleteItem( item );
    }
}

void FOServer::SScriptFunc::Global_DeleteItemsById( ScriptArray& items )
{
    ItemVec items_to_delete;
    for( int i = 0, j = items.GetSize(); i < j; i++ )
    {
        uint item_id = *(uint*) items.At( i );
        if( item_id )
        {
            Item* item = ItemMngr.GetItem( item_id, false );
            if( item )
                ItemMngr.DeleteItem( item );
        }
    }
}

void FOServer::SScriptFunc::Global_DeleteNpc( Critter* npc )
{
    CrMngr.DeleteNpc( npc );
}

void FOServer::SScriptFunc::Global_DeleteNpcById( uint npc_id )
{
    Critter* npc = CrMngr.GetNpc( npc_id, false );
    if( !npc )
        CrMngr.DeleteNpc( npc );
}

void FOServer::SScriptFunc::Global_RadioMessage( ushort channel, ScriptString& text )
{
    ItemMngr.RadioSendTextEx( channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, text.c_str(), (uint) text.length(), 0, false, 0, 0, NULL );
}

void FOServer::SScriptFunc::Global_RadioMessageMsg( ushort channel, ushort text_msg, uint num_str )
{
    ItemMngr.RadioSendTextEx( channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, NULL, 0, 0, false, text_msg, num_str, NULL );
}

void FOServer::SScriptFunc::Global_RadioMessageMsgLex( ushort channel, ushort text_msg, uint num_str, ScriptString* lexems )
{
    ItemMngr.RadioSendTextEx( channel, RADIO_BROADCAST_FORCE_ALL, 0, 0, 0, NULL, 0, 0, false, text_msg, num_str, lexems && lexems->length() ? lexems->c_str() : NULL );
}

uint FOServer::SScriptFunc::Global_GetFullSecond( ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    if( !year )
        year = GameOpt.Year;
    else
        year = CLAMP( year, GameOpt.YearStart, GameOpt.YearStart + 130 );
    if( !month )
        month = GameOpt.Month;
    else
        month = CLAMP( month, 1, 12 );
    if( !day )
        day = GameOpt.Day;
    else
    {
        uint month_day = Timer::GameTimeMonthDay( year, month );
        day = CLAMP( day, 1, month_day );
    }
    if( hour > 23 )
        hour = 23;
    if( minute > 59 )
        minute = 59;
    if( second > 59 )
        second = 59;
    return Timer::GetFullSecond( year, month, day, hour, minute, second );
}

void FOServer::SScriptFunc::Global_GetGameTime( uint full_second, ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second )
{
    DateTimeStamp dt = Timer::GetGameTime( full_second );
    year = dt.Year;
    month = dt.Month;
    day_of_week = dt.DayOfWeek;
    day = dt.Day;
    hour = dt.Hour;
    minute = dt.Minute;
    second = dt.Second;
}

uint FOServer::SScriptFunc::Global_CreateLocation( hash loc_pid, ushort wx, ushort wy, ScriptArray* critters )
{
    // Create and generate location
    Location* loc = MapMngr.CreateLocation( loc_pid, wx, wy, 0 );
    if( !loc )
    {
        WriteLogF( _FUNC_, " - Unable to create location<%s>.\n", HASH_STR( loc_pid ) );
        SCRIPT_ERROR_R0( "Unable to create location." );
    }

    // Add Known locations to critters
    if( !critters )
        return loc->GetId();
    for( uint i = 0, j = critters->GetSize(); i < j; i++ )
    {
        Critter* cr = *(Critter**) critters->At( i );

        cr->AddKnownLoc( loc->GetId() );
        if( !cr->GetMapId() )
            cr->Send_GlobalLocation( loc, true );
        if( loc->IsAutomaps() )
            cr->Send_AutomapsInfo( NULL, loc );

        ushort zx = GM_ZONE( loc->Data.WX );
        ushort zy = GM_ZONE( loc->Data.WY );
        if( cr->GMapFog.Get2Bit( zx, zy ) == GM_FOG_FULL )
        {
            cr->GMapFog.Set2Bit( zx, zy, GM_FOG_HALF );
            if( !cr->GetMapId() )
                cr->Send_GlobalMapFog( zx, zy, GM_FOG_HALF );
        }
    }
    return loc->GetId();
}

void FOServer::SScriptFunc::Global_DeleteLocation( Location* loc )
{
    MapMngr.DeleteLocation( loc, NULL );
}

void FOServer::SScriptFunc::Global_DeleteLocationById( uint loc_id )
{
    Location* loc = MapMngr.GetLocation( loc_id );
    if( loc )
        MapMngr.DeleteLocation( loc, NULL );
}

// void FOServer::SScriptFunc::Global_GetProtoCritter( hash proto_id, ScriptArray& data )
// {
//     CritData* data_ = CrMngr.GetProto( proto_id );
//     if( !data_ )
//         SCRIPT_ERROR_R( "Proto critter not found." );
// //     IntVec data__;
// //     data__.resize( MAX_PARAMS );
// //     memcpy( &data__[ 0 ], data_->Params, sizeof( data_->Params ) );
// //     Script::AppendVectorToArray( data__, &data );
// }

CraftItem* FOServer::SScriptFunc::Global_GetCraftItem( uint num )
{
    return MrFixit.GetCraft( num );
}

Critter* FOServer::SScriptFunc::Global_GetCritter( uint crid )
{
    if( !crid )
        return NULL;          // SCRIPT_ERROR_R0("Critter id arg is zero.");
    return CrMngr.GetCritter( crid, true );
}

Critter* FOServer::SScriptFunc::Global_GetPlayer( ScriptString& name )
{
    uint len_utf8 = Str::LengthUTF8( name.c_str() );
    if( len_utf8 < MIN_NAME || len_utf8 < GameOpt.MinNameLength )
        return NULL;
    if( len_utf8 > MAX_NAME || len_utf8 > GameOpt.MaxNameLength )
        return NULL;

    return CrMngr.GetPlayer( name.c_str(), true );
}

uint FOServer::SScriptFunc::Global_GetPlayerId( ScriptString& name )
{
    uint len_utf8 = Str::LengthUTF8( name.c_str() );
    if( len_utf8 < MIN_NAME || len_utf8 < GameOpt.MinNameLength )
        return 0;                                               // SCRIPT_ERROR_R0("Name length is less than minimum.");
    if( len_utf8 > MAX_NAME || len_utf8 > GameOpt.MaxNameLength )
        return 0;                                               // SCRIPT_ERROR_R0("Name length is greater than maximum.");

    uint id = MAKE_CLIENT_ID( name.c_str() );

    SCOPE_LOCK( ClientsDataLocker );
    ClientData* data = GetClientData( id );
    if( data )
        return id;
    return 0;
}

ScriptString* FOServer::SScriptFunc::Global_GetPlayerName( uint id )
{
    if( !id )
        return NULL;         // SCRIPT_ERROR_R0("Id arg is zero.");

    if( Singleplayer )
    {
        if( id == 1 )
            return ScriptString::Create( SingleplayerSave.CrData.Name );
        return NULL;
    }

    SCOPE_LOCK( ClientsDataLocker );
    ClientData* data = GetClientData( id );
    if( !data )
        return NULL;           // SCRIPT_ERROR_R0("Player not found.");
    return ScriptString::Create( data->ClientName );
}

uint FOServer::SScriptFunc::Global_GetGlobalMapCritters( ushort wx, ushort wy, uint radius, int find_type, ScriptArray* critters )
{
    CrVec critters_;
    CrMngr.GetGlobalMapCritters( wx, wy, radius, find_type, critters_, true );
    if( critters )
        Script::AppendVectorToArrayRef( critters_, critters );
    return (uint) critters_.size();
}

Map* FOServer::SScriptFunc::Global_GetMap( uint map_id )
{
    if( !map_id )
        SCRIPT_ERROR_R0( "Map id arg is zero." );
    return MapMngr.GetMap( map_id );
}

Map* FOServer::SScriptFunc::Global_GetMapByPid( hash map_pid, uint skip_count )
{
    if( !map_pid )
        SCRIPT_ERROR_R0( "Invalid zero map proto id arg." );
    return MapMngr.GetMapByPid( map_pid, skip_count );
}

Location* FOServer::SScriptFunc::Global_GetLocation( uint loc_id )
{
    if( !loc_id )
        SCRIPT_ERROR_R0( "Location id arg is zero." );
    return MapMngr.GetLocation( loc_id );
}

Location* FOServer::SScriptFunc::Global_GetLocationByPid( hash loc_pid, uint skip_count )
{
    if( !loc_pid )
        SCRIPT_ERROR_R0( "Invalid zero location proto id arg." );
    return MapMngr.GetLocationByPid( loc_pid, skip_count );
}

uint FOServer::SScriptFunc::Global_GetLocations( ushort wx, ushort wy, uint radius, ScriptArray* locations )
{
    LocVec locs;
    MapMngr.GetLocations( locs, false );
    LocVec locs_;
    locs_.reserve( locs.size() );
    for( auto it = locs.begin(), end = locs.end(); it != end; ++it )
    {
        Location* loc = *it;
        if( DistSqrt( wx, wy, loc->Data.WX, loc->Data.WY ) <= radius + loc->GetRadius() )
            locs_.push_back( loc );
    }

    if( locations )
    {
        for( auto it = locs_.begin(), end = locs_.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Location* >( locs_, locations );
    }
    return (uint) locs_.size();
}

uint FOServer::SScriptFunc::Global_GetVisibleLocations( ushort wx, ushort wy, uint radius, Critter* cr, ScriptArray* locations )
{
    LocVec locs;
    MapMngr.GetLocations( locs, false );
    LocVec locs_;
    locs_.reserve( locs.size() );
    for( auto it = locs.begin(), end = locs.end(); it != end; ++it )
    {
        Location* loc = *it;
        if( DistSqrt( wx, wy, loc->Data.WX, loc->Data.WY ) <= radius + loc->GetRadius() &&
            ( loc->IsVisible() || ( cr && cr->IsPlayer() && ( (Client*) cr )->CheckKnownLocById( loc->GetId() ) ) ) )
            locs_.push_back( loc );
    }

    if( locations )
    {
        for( auto it = locs_.begin(), end = locs_.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Location* >( locs_, locations );
    }
    return (uint) locs_.size();
}

uint FOServer::SScriptFunc::Global_GetZoneLocationIds( ushort zx, ushort zy, uint zone_radius, ScriptArray* locations )
{
    UIntVec loc_ids;
    MapMngr.GetZoneLocations( zx, zy, zone_radius, loc_ids );

    if( locations )
        Script::AppendVectorToArray< uint >( loc_ids, locations );
    return (uint) loc_ids.size();
}

bool FOServer::SScriptFunc::Global_RunDialogNpc( Critter* player, Critter* npc, bool ignore_distance )
{
    if( player->IsDestroyed )
        SCRIPT_ERROR_R0( "Player arg is destroyed." );
    if( !player->IsPlayer() )
        SCRIPT_ERROR_R0( "Player arg is not player." );
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Npc arg is destroyed." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Npc arg is not npc." );
    Client* cl = (Client*) player;
    if( cl->Talk.Locked )
        SCRIPT_ERROR_R0( "Can't open new dialog from demand, result or dialog functions." );                   // if(DlgCtx->GetState()==asEXECUTION_ACTIVE)
    Dialog_Begin( cl, (Npc*) npc, 0, 0, 0, ignore_distance );
    return cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == npc->GetId();
}

bool FOServer::SScriptFunc::Global_RunDialogNpcDlgPack( Critter* player, Critter* npc, uint dlg_pack, bool ignore_distance )
{
    if( player->IsDestroyed )
        SCRIPT_ERROR_R0( "Player arg is destroyed." );
    if( !player->IsPlayer() )
        SCRIPT_ERROR_R0( "Player arg is not player." );
    if( npc->IsDestroyed )
        SCRIPT_ERROR_R0( "Npc arg is destroyed." );
    if( !npc->IsNpc() )
        SCRIPT_ERROR_R0( "Npc arg is not npc." );
    Client* cl = (Client*) player;
    if( cl->Talk.Locked )
        SCRIPT_ERROR_R0( "Can't open new dialog from demand, result or dialog functions." );                   // if(DlgCtx->GetState()==asEXECUTION_ACTIVE)
    Dialog_Begin( cl, (Npc*) npc, dlg_pack, 0, 0, ignore_distance );
    return cl->Talk.TalkType == TALK_WITH_NPC && cl->Talk.TalkNpc == npc->GetId();
}

bool FOServer::SScriptFunc::Global_RunDialogHex( Critter* player, uint dlg_pack, ushort hx, ushort hy, bool ignore_distance )
{
    if( player->IsDestroyed )
        SCRIPT_ERROR_R0( "Player arg is destroyed." );
    if( !player->IsPlayer() )
        SCRIPT_ERROR_R0( "Player arg is not player." );
    if( !DlgMngr.GetDialog( dlg_pack ) )
        SCRIPT_ERROR_R0( "Dialog not found." );
    Client* cl = (Client*) player;
    if( cl->Talk.Locked )
        SCRIPT_ERROR_R0( "Can't open new dialog from demand, result or dialog functions." );                   // if(DlgCtx->GetState()==asEXECUTION_ACTIVE)
    Dialog_Begin( cl, NULL, dlg_pack, hx, hy, ignore_distance );
    return cl->Talk.TalkType == TALK_WITH_HEX && cl->Talk.TalkHexX == hx && cl->Talk.TalkHexY == hy;
}

int64 FOServer::SScriptFunc::Global_WorldItemCount( hash pid )
{
    if( !ItemMngr.GetProtoItem( pid ) )
        SCRIPT_ERROR_R0( "Invalid protoId arg." );
    return ItemMngr.GetItemStatistics( pid );
}

bool FOServer::SScriptFunc::Global_AddTextListener( int say_type, ScriptString& first_str, uint parameter, ScriptString& script_name )
{
    if( first_str.length() > TEXT_LISTEN_FIRST_STR_MAX_LEN )
        SCRIPT_ERROR_R0( "First string arg length greater than maximum." );

    uint func_id = Script::Bind( script_name.c_str(), "void %s(Critter&,string&)", false );
    if( !func_id )
        SCRIPT_ERROR_R0( "Unable to bind script function." );

    TextListen tl;
    tl.FuncId = func_id;
    tl.SayType = say_type;
    Str::Copy( tl.FirstStr, first_str.c_str() );
    tl.FirstStrLen = Str::Length( tl.FirstStr );
    tl.Parameter = parameter;

    SCOPE_LOCK( TextListenersLocker );

    TextListeners.push_back( tl );
    return true;
}

void FOServer::SScriptFunc::Global_EraseTextListener( int say_type, ScriptString& first_str, uint parameter )
{
    SCOPE_LOCK( TextListenersLocker );

    for( auto it = TextListeners.begin(), end = TextListeners.end(); it != end; ++it )
    {
        TextListen& tl = *it;
        if( say_type == tl.SayType && Str::CompareCaseUTF8( first_str.c_str(), tl.FirstStr ) && tl.Parameter == parameter )
        {
            TextListeners.erase( it );
            return;
        }
    }
}

AIDataPlane* FOServer::SScriptFunc::Global_CreatePlane()
{
    return new AIDataPlane( 0, 0 );
}

uint FOServer::SScriptFunc::Global_GetBagItems( uint bag_id, ScriptArray* pids, ScriptArray* min_counts, ScriptArray* max_counts, ScriptArray* slots )
{
    NpcBag& bag = AIMngr.GetBag( bag_id );
    if( bag.empty() )
        return 0;

    MaxTVec bag_data;
    bag_data.reserve( 100 );
    for( uint i = 0, j = (uint) bag.size(); i < j; i++ )
    {
        NpcBagCombination& bc = bag[ i ];
        for( uint k = 0, l = (uint) bc.size(); k < l; k++ )
        {
            NpcBagItems& bci = bc[ k ];
            for( uint m = 0, n = (uint) bci.size(); m < n; m++ )
            {
                NpcBagItem& item = bci[ m ];
                bag_data.push_back( item.ItemPid );
                bag_data.push_back( item.MinCnt );
                bag_data.push_back( item.MaxCnt );
                bag_data.push_back( item.ItemSlot );
            }
        }
    }

    uint count = (uint) bag_data.size() / 4;
    if( pids || min_counts || max_counts || slots )
    {
        if( pids )
            pids->Resize( pids->GetSize() + count );
        if( min_counts )
            min_counts->Resize( min_counts->GetSize() + count );
        if( max_counts )
            max_counts->Resize( max_counts->GetSize() + count );
        if( slots )
            slots->Resize( slots->GetSize() + count );
        for( uint i = 0; i < count; i++ )
        {
            if( pids )
                *(hash*) pids->At( pids->GetSize() - count + i ) = (hash) bag_data[ i * 4 ];
            if( min_counts )
                *(uint*) min_counts->At( min_counts->GetSize() - count + i ) = (uint) bag_data[ i * 4 + 1 ];
            if( max_counts )
                *(uint*) max_counts->At( max_counts->GetSize() - count + i ) = (uint) bag_data[ i * 4 + 2 ];
            if( slots )
                *(int*) slots->At( slots->GetSize() - count + i ) = (int) bag_data[ i * 4 + 3 ];
        }
    }
    return count;
}

template< typename Ty >
void SwapArray( Ty& arr1, Ty& arr2 )
{
    Ty tmp;
    memcpy( tmp, arr1, sizeof( tmp ) );
    memcpy( arr1, arr2, sizeof( tmp ) );
    memcpy( arr2, tmp, sizeof( tmp ) );
}

void SwapCrittersRefreshNpc( Npc* npc )
{
    UNSETFLAG( npc->Flags, FCRIT_PLAYER );
    SETFLAG( npc->Flags, FCRIT_NPC );
    AIDataPlaneVec& planes = npc->GetPlanes();
    for( auto it = planes.begin(), end = planes.end(); it != end; ++it )
        delete *it;
    planes.clear();
    npc->NextRefreshBagTick = Timer::GameTick() + GameOpt.BagRefreshTime * 60 * 1000;
}

void SwapCrittersRefreshClient( Client* cl, Map* map, Map* prev_map )
{
    UNSETFLAG( cl->Flags, FCRIT_NPC );
    SETFLAG( cl->Flags, FCRIT_PLAYER );

    if( cl->Talk.TalkType != TALK_NONE )
        cl->CloseTalk();

    if( map != prev_map )
    {
        cl->Send_LoadMap( NULL );
    }
    else
    {
        cl->Send_AllProperties();
        cl->Send_AddAllItems();
        cl->Send_HoloInfo( true, 0, 0 );
        cl->Send_AllAutomapsInfo();

        if( map->IsTurnBasedOn )
        {
            if( map->IsCritterTurn( cl ) )
            {
                cl->Send_CustomCommand( cl, OTHER_YOU_TURN, map->GetCritterTurnTime() );
            }
            else
            {
                Critter* cr = cl->GetCritSelf( map->GetCritterTurnId(), false );
                if( cr )
                    cl->Send_CustomCommand( cr, OTHER_YOU_TURN, map->GetCritterTurnTime() );
            }
        }
        else if( TB_BATTLE_TIMEOUT_CHECK( cl->GetTimeoutBattle() ) )
        {
            cl->SetTimeoutBattle( 0 );
        }
    }
}

bool FOServer::SScriptFunc::Global_SwapCritters( Critter* cr1, Critter* cr2, bool with_inventory )
{
    // Check
    if( cr1->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter1 is destroyed." );
    if( cr2->IsDestroyed )
        SCRIPT_ERROR_R0( "Critter2 is destroyed." );
    if( cr1 == cr2 )
        SCRIPT_ERROR_R0( "Critter1 is equal to Critter2." );
    if( !cr1->GetMapId() )
        SCRIPT_ERROR_R0( "Critter1 is on global map." );
    if( !cr2->GetMapId() )
        SCRIPT_ERROR_R0( "Critter2 is on global map." );

    // Swap positions
    Map* map1 = MapMngr.GetMap( cr1->GetMapId(), true );
    if( !map1 )
        SCRIPT_ERROR_R0( "Map of Critter1 not found." );
    Map* map2 = MapMngr.GetMap( cr2->GetMapId(), true );
    if( !map2 )
        SCRIPT_ERROR_R0( "Map of Critter2 not found." );

    map1->Lock();
    map2->Lock();

    CrVec& cr_map1 = map1->GetCrittersNoLock();
    ClVec& cl_map1 = map1->GetPlayersNoLock();
    PcVec& npc_map1 = map1->GetNpcsNoLock();
    auto   it_cr = std::find( cr_map1.begin(), cr_map1.end(), cr1 );
    if( it_cr != cr_map1.end() )
        cr_map1.erase( it_cr );
    auto it_cl = std::find( cl_map1.begin(), cl_map1.end(), (Client*) cr1 );
    if( it_cl != cl_map1.end() )
        cl_map1.erase( it_cl );
    auto it_pc = std::find( npc_map1.begin(), npc_map1.end(), (Npc*) cr1 );
    if( it_pc != npc_map1.end() )
        npc_map1.erase( it_pc );

    CrVec& cr_map2 = map2->GetCrittersNoLock();
    ClVec& cl_map2 = map2->GetPlayersNoLock();
    PcVec& npc_map2 = map2->GetNpcsNoLock();
    it_cr = std::find( cr_map2.begin(), cr_map2.end(), cr1 );
    if( it_cr != cr_map2.end() )
        cr_map2.erase( it_cr );
    it_cl = std::find( cl_map2.begin(), cl_map2.end(), (Client*) cr1 );
    if( it_cl != cl_map2.end() )
        cl_map2.erase( it_cl );
    it_pc = std::find( npc_map2.begin(), npc_map2.end(), (Npc*) cr1 );
    if( it_pc != npc_map2.end() )
        npc_map2.erase( it_pc );

    cr_map2.push_back( cr1 );
    if( cr1->IsNpc() )
        npc_map2.push_back( (Npc*) cr1 );
    else
        cl_map2.push_back( (Client*) cr1 );
    cr_map1.push_back( cr2 );
    if( cr2->IsNpc() )
        npc_map1.push_back( (Npc*) cr2 );
    else
        cl_map1.push_back( (Client*) cr2 );

    cr1->SetMaps( map2->GetId(), map2->GetPid() );
    cr2->SetMaps( map1->GetId(), map1->GetPid() );

    map2->Unlock();
    map1->Unlock();

    // Swap data
    std::swap( cr1->Data, cr2->Data );
    std::swap( cr1->KnockoutAp, cr2->KnockoutAp );
    std::swap( cr1->Flags, cr2->Flags );
    SwapArray( cr1->FuncId, cr2->FuncId );
    cr1->SetBreakTime( 0 );
    cr2->SetBreakTime( 0 );
    std::swap( cr1->AccessContainerId, cr2->AccessContainerId );
    std::swap( cr1->ItemTransferCount, cr2->ItemTransferCount );
    std::swap( cr1->ApRegenerationTick, cr2->ApRegenerationTick );
    std::swap( cr1->CrTimeEvents, cr2->CrTimeEvents );

    // Swap inventory
    if( with_inventory )
    {
        ItemVec items1 = cr1->GetInventory();
        ItemVec items2 = cr2->GetInventory();
        for( auto it = items1.begin(), end = items1.end(); it != end; ++it )
            cr1->EraseItem( *it, false );
        for( auto it = items2.begin(), end = items2.end(); it != end; ++it )
            cr2->EraseItem( *it, false );
        for( auto it = items1.begin(), end = items1.end(); it != end; ++it )
            cr2->AddItem( *it, false );
        for( auto it = items2.begin(), end = items2.end(); it != end; ++it )
            cr1->AddItem( *it, false );
    }

    // Swap properties
    cr2->Props = cr1->Props;

    // Refresh
    cr1->ClearVisible();
    cr2->ClearVisible();

    if( cr1->IsNpc() )
        SwapCrittersRefreshNpc( (Npc*) cr1 );
    else
        SwapCrittersRefreshClient( (Client*) cr1, map2, map1 );
    if( cr2->IsNpc() )
        SwapCrittersRefreshNpc( (Npc*) cr2 );
    else
        SwapCrittersRefreshClient( (Client*) cr2, map1, map2 );
    if( map1 == map2 )
    {
        cr1->Send_CustomCommand( cr1, OTHER_CLEAR_MAP, 0 );
        cr2->Send_CustomCommand( cr2, OTHER_CLEAR_MAP, 0 );
        cr1->Send_Dir( cr1 );
        cr2->Send_Dir( cr2 );
        cr1->Send_CustomCommand( cr1, OTHER_TELEPORT, ( cr1->GetHexX() << 16 ) | ( cr1->GetHexY() ) );
        cr2->Send_CustomCommand( cr2, OTHER_TELEPORT, ( cr2->GetHexX() << 16 ) | ( cr2->GetHexY() ) );
        cr1->Send_CustomCommand( cr1, OTHER_BASE_TYPE, cr1->GetCrType() );
        cr2->Send_CustomCommand( cr2, OTHER_BASE_TYPE, cr2->GetCrType() );
        cr1->ProcessVisibleCritters();
        cr2->ProcessVisibleCritters();
        cr1->ProcessVisibleItems();
        cr2->ProcessVisibleItems();
    }
    return true;
}

uint FOServer::SScriptFunc::Global_GetAllItems( hash pid, ScriptArray* items )
{
    ItemVec game_items;
    ItemMngr.GetGameItems( game_items );
    ItemVec game_items_;
    game_items_.reserve( game_items.size() );
    for( auto it = game_items.begin(), end = game_items.end(); it != end; ++it )
    {
        Item* item = *it;
        SYNC_LOCK( item );
        if( !item->IsDestroyed && ( !pid || pid == item->GetProtoId() ) )
            game_items_.push_back( item );
    }
    if( !game_items_.size() )
        return 0;
    if( items )
        Script::AppendVectorToArrayRef< Item* >( game_items_, items );
    return (uint) game_items_.size();
}

uint FOServer::SScriptFunc::Global_GetAllPlayers( ScriptArray* players )
{
    ClVec players_;
    CrVec players__;
    CrMngr.GetClients( players_, true );
    players__.reserve( players_.size() );
    for( auto it = players_.begin(), end = players_.end(); it != end; ++it )
    {
        Critter* player_ = *it;
        if( !player_->IsDestroyed && player_->IsPlayer() )
            players__.push_back( player_ );
    }
    if( !players__.size() )
        return 0;
    if( players )
        Script::AppendVectorToArrayRef< Critter* >( players__, players );
    return (uint) players__.size();
}

uint FOServer::SScriptFunc::Global_GetRegisteredPlayers( ScriptArray* ids, ScriptArray* names )
{
    if( ids || names )
    {
        UIntVec                 ids_;
        vector< ScriptString* > names_;
        for( auto it = ClientsData.begin(), end = ClientsData.end(); it != end; ++it )
        {
            ids_.push_back( it->first );
            names_.push_back( ScriptString::Create( it->second->ClientName ) );
        }

        if( !ids_.size() )
            return 0;

        if( ids )
            Script::AppendVectorToArray< uint >( ids_, ids );
        if( names )
            Script::AppendVectorToArrayRef< ScriptString* >( names_, names );

        return (uint) ids_.size();
    }

    return (uint) ClientsData.size();
}

uint FOServer::SScriptFunc::Global_GetAllNpc( hash pid, ScriptArray* npc )
{
    PcVec npcs;
    CrVec npcs_;
    CrMngr.GetNpcs( npcs, true );
    npcs_.reserve( npcs.size() );
    for( auto it = npcs.begin(), end = npcs.end(); it != end; ++it )
    {
        Npc* npc_ = *it;
        if( !npc_->IsDestroyed && ( !pid || pid == npc_->Data.ProtoId ) )
            npcs_.push_back( npc_ );
    }
    if( !npcs_.size() )
        return 0;
    if( npc )
        Script::AppendVectorToArrayRef< Critter* >( npcs_, npc );
    return (uint) npcs_.size();
}

uint FOServer::SScriptFunc::Global_GetAllMaps( hash pid, ScriptArray* maps )
{
    MapVec maps_;
    MapMngr.GetMaps( maps_, false );
    MapVec maps__;
    maps__.reserve( maps_.size() );
    for( auto it = maps_.begin(), end = maps_.end(); it != end; ++it )
    {
        Map* map = *it;
        if( !pid || pid == map->GetPid() )
            maps__.push_back( map );
    }

    if( maps )
    {
        for( auto it = maps__.begin(), end = maps__.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Map* >( maps__, maps );
    }
    return (uint) maps__.size();
}

uint FOServer::SScriptFunc::Global_GetAllLocations( hash pid, ScriptArray* locations )
{
    LocVec locs;
    MapMngr.GetLocations( locs, false );
    LocVec locs_;
    locs_.reserve( locs.size() );
    for( auto it = locs.begin(), end = locs.end(); it != end; ++it )
    {
        Location* loc = *it;
        if( !pid || pid == loc->GetPid() )
            locs_.push_back( loc );
    }

    if( locations )
    {
        for( auto it = locs_.begin(), end = locs_.end(); it != end; ++it )
            SYNC_LOCK( *it );
        Script::AppendVectorToArrayRef< Location* >( locs_, locations );
    }
    return (uint) locs_.size();
}

ScriptString* FOServer::SScriptFunc::Global_GetScriptName( hash script_id )
{
    return ScriptString::Create( Script::GetScriptFuncName( script_id ) );
}

void FOServer::SScriptFunc::Global_GetTime( ushort& year, ushort& month, ushort& day, ushort& day_of_week, ushort& hour, ushort& minute, ushort& second, ushort& milliseconds )
{
    DateTimeStamp cur_time;
    Timer::GetCurrentDateTime( cur_time );
    year = cur_time.Year;
    month = cur_time.Month;
    day_of_week = cur_time.DayOfWeek;
    day = cur_time.Day;
    hour = cur_time.Hour;
    minute = cur_time.Minute;
    second = cur_time.Second;
    milliseconds = cur_time.Milliseconds;
}

void FOServer::SScriptFunc::Global_SetTime( ushort multiplier, ushort year, ushort month, ushort day, ushort hour, ushort minute, ushort second )
{
    SetGameTime( multiplier, year, month, day, hour, minute, second );
}

bool FOServer::SScriptFunc::Global_SetPropertyGetCallback( int prop_enum_value, ScriptString& script_func )
{
    Property* prop = GlobalVars::PropertiesRegistrator->FindByEnum( prop_enum_value );
    prop = ( prop ? prop : Critter::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    prop = ( prop ? prop : Item::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    if( !prop )
        SCRIPT_ERROR_R0( "Property '%s' not found.", HASH_STR( prop_enum_value ) );

    string result = prop->SetGetCallback( script_func.c_str() );
    if( result != "" )
        SCRIPT_ERROR_R0( result.c_str() );
    return true;
}

bool FOServer::SScriptFunc::Global_AddPropertySetCallback( int prop_enum_value, ScriptString& script_func )
{
    Property* prop = Critter::PropertiesRegistrator->FindByEnum( prop_enum_value );
    prop = ( prop ? prop : Item::PropertiesRegistrator->FindByEnum( prop_enum_value ) );
    if( !prop )
        SCRIPT_ERROR_R0( "Property '%s' not found.", HASH_STR( prop_enum_value ) );

    string result = prop->AddSetCallback( script_func.c_str() );
    if( result != "" )
        SCRIPT_ERROR_R0( result.c_str() );
    return true;
}

void FOServer::SScriptFunc::Global_AllowSlot( uchar index, bool enable_send )
{
    Critter::SlotEnabled[ index ] = true;
    Critter::SlotDataSendEnabled[ index ] = enable_send;
}

void FOServer::SScriptFunc::Global_AddRegistrationProperty( int cr_prop )
{
    Critter::RegProperties.insert( cr_prop );

    ScriptArray* props_array;
    int          props_array_index = Script::GetEngine()->GetGlobalPropertyIndexByName( "CritterPropertyRegProperties" );
    Script::GetEngine()->GetGlobalPropertyByIndex( props_array_index, NULL, NULL, NULL, NULL, NULL, (void**) &props_array );
    props_array->Resize( 0 );
    for( auto it = Critter::RegProperties.begin(); it != Critter::RegProperties.end(); ++it )
        props_array->InsertLast( (void*) &( *it ) );
}

bool FOServer::SScriptFunc::Global_LoadDataFile( ScriptString& dat_name )
{
    if( FileManager::LoadDataFile( dat_name.c_str() ) )
    {
        ConstantsManager::Initialize( PT_SERVER_CONFIGS );
        return true;
    }
    return false;
}

int FOServer::SScriptFunc::Global_GetConstantValue( int const_collection, ScriptString* name )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    if( !name || !name->length() )
        SCRIPT_ERROR_R0( "Invalid name arg." );
    return ConstantsManager::GetValue( const_collection, name->c_str() );
}

ScriptString* FOServer::SScriptFunc::Global_GetConstantName( int const_collection, int value )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    const char* name = ConstantsManager::GetName( const_collection, value );
    return ScriptString::Create( name ? name : "" );
}

void FOServer::SScriptFunc::Global_AddConstant( int const_collection, ScriptString* name, int value )
{
    if( !ConstantsManager::IsCollectionInit( const_collection ) )
        SCRIPT_ERROR_R( "Invalid namesFile arg." );
    if( !name || !name->length() )
        SCRIPT_ERROR_R( "Invalid name arg." );
    ConstantsManager::AddConstant( const_collection, name->c_str(), value );
}

bool FOServer::SScriptFunc::Global_LoadConstants( int const_collection, ScriptString* file_name, int path_type )
{
    if( const_collection < 0 || const_collection > 1000 )
        SCRIPT_ERROR_R0( "Invalid namesFile arg." );
    if( !file_name || !file_name->length() )
        SCRIPT_ERROR_R0( "Invalid fileName arg." );
    return ConstantsManager::AddCollection( const_collection, file_name->c_str(), path_type );
}

bool FOServer::SScriptFunc::Global_IsCritterCanWalk( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanWalk( cr_type );
}

bool FOServer::SScriptFunc::Global_IsCritterCanRun( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanRun( cr_type );
}

bool FOServer::SScriptFunc::Global_IsCritterCanRotate( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanRotate( cr_type );
}

bool FOServer::SScriptFunc::Global_IsCritterCanAim( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanAim( cr_type );
}

bool FOServer::SScriptFunc::Global_IsCritterCanArmor( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsCanArmor( cr_type );
}

bool FOServer::SScriptFunc::Global_IsCritterAnim1( uint cr_type, uint index )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::IsAnim1( cr_type, index );
}

int FOServer::SScriptFunc::Global_GetCritterAnimType( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::GetAnimType( cr_type );
}

uint FOServer::SScriptFunc::Global_GetCritterAlias( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return CritType::GetAlias( cr_type );
}

ScriptString* FOServer::SScriptFunc::Global_GetCritterTypeName( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return ScriptString::Create( CritType::GetCritType( cr_type ).Name );
}

ScriptString* FOServer::SScriptFunc::Global_GetCritterSoundName( uint cr_type )
{
    if( !CritType::IsEnabled( cr_type ) )
        SCRIPT_ERROR_R0( "Invalid critter type arg." );
    return ScriptString::Create( CritType::GetSoundName( cr_type ) );
}

struct ServerImage
{
    UCharVec Data;
    uint     Width;
    uint     Height;
    uint     Depth;
};
vector< ServerImage* > ServerImages;
bool FOServer::SScriptFunc::Global_LoadImage( uint index, ScriptString* image_name, uint image_depth, int path_type )
{
    // Delete old
    if( index >= ServerImages.size() )
        ServerImages.resize( index + 1 );
    if( ServerImages[ index ] )
    {
        MEMORY_PROCESS( MEMORY_IMAGE, -(int) ServerImages[ index ]->Data.capacity() );
        delete ServerImages[ index ];
        ServerImages[ index ] = NULL;
    }
    if( !image_name || !image_name->length() )
        return true;

    // Check depth
    static uint image_depth_;
    image_depth_ = image_depth; // Avoid GCC warning "argument 'image_depth' might be clobbered by 'longjmp' or 'vfork'"
    if( image_depth < 1 || image_depth > 4 )
        SCRIPT_ERROR_R0( "Wrong image depth arg." );

    // Check extension
    const char* ext = FileManager::GetExtension( image_name->c_str() );
    if( !ext || !Str::CompareCase( ext, "png" ) )
        SCRIPT_ERROR_R0( "Wrong extension. Allowed only PNG." );

    // Load file to memory
    FileManager fm;
    if( !fm.LoadFile( image_name->c_str(), PT_SERVER_MAPS ) )
        SCRIPT_ERROR_R0( "File not found." );

    // Load PNG from memory
    png_structp pp = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    png_infop   info = NULL;
    if( pp )
        info = png_create_info_struct( pp );
    if( !pp || !info )
    {
        if( pp )
            png_destroy_read_struct( &pp, NULL, NULL );
        SCRIPT_ERROR_R0( "Cannot allocate memory to read PNG data." );
    }

    if( setjmp( png_jmpbuf( pp ) ) )
    {
        png_destroy_read_struct( &pp, &info, NULL );
        SCRIPT_ERROR_R0( "PNG data contains errors." );
    }

    struct png_mem_data_
    {
        png_structp          pp;
        const unsigned char* current;
        const unsigned char* last;

        static void          png_read_data_from_mem( png_structp png_ptr, png_bytep data, png_size_t length )
        {
            png_mem_data_* png_mem_data = (png_mem_data_*) png_get_io_ptr( png_ptr );
            if( png_mem_data->current + length > png_mem_data->last )
            {
                png_error( png_mem_data->pp, "Invalid attempt to read row data." );
                return;
            }
            memcpy( data, png_mem_data->current, length );
            png_mem_data->current += length;
        }
    } png_mem_data;
    png_mem_data.current = fm.GetBuf();
    png_mem_data.last = fm.GetBuf() + fm.GetFsize();
    png_mem_data.pp = pp;
    png_set_read_fn( pp, ( png_voidp ) & png_mem_data, png_mem_data.png_read_data_from_mem );

    png_read_info( pp, info );

    if( png_get_color_type( pp, info ) == PNG_COLOR_TYPE_PALETTE )
        png_set_expand( pp );

    int channels = 1;
    if( png_get_color_type( pp, info ) & PNG_COLOR_MASK_COLOR )
        channels = 3;

    int num_trans = 0;
    png_get_tRNS( pp, info, 0, &num_trans, 0 );
    if( ( png_get_color_type( pp, info ) & PNG_COLOR_MASK_ALPHA ) || ( num_trans != 0 ) )
        channels++;

    int w = (int) png_get_image_width( pp, info );
    int h = (int) png_get_image_height( pp, info );
    int d = channels;

    if( png_get_bit_depth( pp, info ) < 8 )
    {
        png_set_packing( pp );
        png_set_expand( pp );
    }
    else if( png_get_bit_depth( pp, info ) == 16 )
        png_set_strip_16( pp );

    if( png_get_valid( pp, info, PNG_INFO_tRNS ) )
        png_set_tRNS_to_alpha( pp );

    uchar*     data = new uchar[ w * h * d ];
    png_bytep* rows = new png_bytep[ h ];

    for( int i = 0; i < h; i++ )
        rows[ i ] = (png_bytep) ( data + i * w * d );

    for( int i = png_set_interlace_handling( pp ); i > 0; i-- )
        png_read_rows( pp, rows, NULL, h );

    delete[] rows;
    png_read_end( pp, info );
    png_destroy_read_struct( &pp, &info, NULL );

    // Copy data
    ServerImage* simg = new ServerImage();
    simg->Width = w;
    simg->Height = h;
    simg->Depth = image_depth_;
    simg->Data.resize( simg->Width * simg->Height * simg->Depth + 3 ); // +3 padding

    const uint argb_offs[ 4 ] = { 2, 1, 0, 3 };
    uint       min_depth = MIN( (uint) d, simg->Depth );
    uint       data_index = 0;
    uint       png_data_index = 0;
    for( uint y = 0; y < simg->Height; y++ )
    {
        for( uint x = 0; x < simg->Width; x++ )
        {
            memzero( &simg->Data[ data_index ], simg->Depth );
            for( uint j = 0; j < min_depth; j++ )
                simg->Data[ data_index + j ] = *( data + png_data_index + argb_offs[ j ] );
            png_data_index += d;
            data_index += simg->Depth;
        }
    }
    delete[] data;

    ServerImages[ index ] = simg;
    MEMORY_PROCESS( MEMORY_IMAGE, (int) ServerImages[ index ]->Data.capacity() );
    return true;
}

uint FOServer::SScriptFunc::Global_GetImageColor( uint index, uint x, uint y )
{
    if( index >= ServerImages.size() || !ServerImages[ index ] )
        SCRIPT_ERROR_R0( "Image not loaded." );
    ServerImage* simg = ServerImages[ index ];
    if( x >= simg->Width || y >= simg->Height )
        SCRIPT_ERROR_R0( "Invalid coords arg." );

    uint* data = (uint*) ( &simg->Data[ 0 ] + y * simg->Width * simg->Depth + x * simg->Depth );
    uint  result = *data;
    switch( simg->Depth )
    {
    case 1:
        result &= 0xFF;
        break;
    case 2:
        result &= 0xFFFF;
        break;
    case 3:
        result &= 0xFFFFFF;
        break;
    default:
        break;
    }
    return result;
}

hash FOServer::SScriptFunc::Global_GetScriptId( ScriptString& script_name, ScriptString& func_decl )
{
    return Script::BindScriptFuncNum( script_name.c_str(), func_decl.c_str() );
}

void FOServer::SScriptFunc::Global_Synchronize()
{
    if( !Script::SynchronizeThread() )
        SCRIPT_ERROR_R( "Invalid call." );
}

void FOServer::SScriptFunc::Global_Resynchronize()
{
    if( !Script::ResynchronizeThread() )
        SCRIPT_ERROR_R( "Invalid call." );
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
