#include "Common.h"
#include "Script.h"
#include "Text.h"
#include "FileManager.h"
#include "AngelScript/scriptstring.h"
#include "AngelScript/scriptany.h"
#include "AngelScript/scriptdictionary.h"
#include "AngelScript/scriptdict.h"
#include "AngelScript/scriptfile.h"
#include "AngelScript/scriptarray.h"
#include "AngelScript/reflection.h"
#include "AngelScript/preprocessor.h"
#include "AngelScript/sdk/add_on/scriptmath/scriptmath.h"
#include "AngelScript/sdk/add_on/weakref/weakref.h"
#include "AngelScript/sdk/add_on/scripthelper/scripthelper.h"
#include <strstream>

static const char* ContextStatesStr[] =
{
    "Finished",
    "Suspended",
    "Aborted",
    "Exception",
    "Prepared",
    "Uninitialized",
    "Active",
    "Error",
};

class BindFunction
{
public:
    bool               IsScriptCall;
    asIScriptFunction* ScriptFunc;
    string             ModuleName;
    string             FuncName;
    size_t             NativeFuncAddr;

    BindFunction()
    {
        IsScriptCall = false;
        ScriptFunc = nullptr;
        NativeFuncAddr = 0;
    }

    BindFunction( asIScriptFunction* func )
    {
        IsScriptCall = true;
        ScriptFunc = func;
        const char* module_name = func->GetModuleName();
        ModuleName = ( module_name ? module_name : "unknown" );
        FuncName = func->GetDeclaration();
        func->AddRef();
        NativeFuncAddr = 0;
    }

    BindFunction( size_t native_func_addr, const char* module_name,  const char* func_name )
    {
        IsScriptCall = false;
        NativeFuncAddr = native_func_addr;
        ModuleName = module_name;
        FuncName = func_name;
        ScriptFunc = nullptr;
    }

    void Clear()
    {
        if( ScriptFunc )
            ScriptFunc->Release();
        memzero( this, sizeof( BindFunction ) );
    }
};
typedef vector< BindFunction > BindFunctionVec;

static asIScriptEngine* Engine = nullptr;
static bool             LogDebugInfo = false;

static BindFunctionVec  BindedFunctions;
#ifdef SCRIPT_MULTITHREADING
static Mutex            BindedFunctionsLocker;
#endif
static HashIntMap       ScriptFuncBinds;   // Func Num -> Bind Id

static bool             ConcurrentExecution = false;
#ifdef SCRIPT_MULTITHREADING
static Mutex            ConcurrentExecutionLocker;
#endif

static bool LoadLibraryCompiler = false;

// Contexts
struct ContextData
{
    char              Info[ MAX_FOTEXT ];
    uint              StartTick;
    uint              SuspendEndTick;
    Entity*           EntitiesArgs[ 100 ];
    uint              EntitiesArgsCount;
    asIScriptContext* Parent;
};
static ContextVec FreeContexts;
static ContextVec BusyContexts;
static Mutex      ContextsLocker;

// Script watcher
static Thread ScriptWatcherThread;
static bool   ScriptWatcherFinish = false;
static void ScriptWatcher( void* );
static uint   RunTimeoutAbort = 600000;   // 10 minutes
static uint   RunTimeoutMessage = 300000; // 5 minutes

bool Script::Init( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls,
                   uint profiler_sample_time, bool profiler_save_to_file, bool profiler_dynamic_display )
{
    // Create default engine
    Engine = CreateEngine( pragma_callback, dll_target, allow_native_calls );
    if( !Engine )
    {
        WriteLogF( _FUNC_, " - Can't create AS engine.\n" );
        return false;
    }

    // Enable profiler
    if( profiler_sample_time > 0 )
    {
        EngineData* edata = (EngineData*) Engine->GetUserData();
        edata->Profiler = new ScriptProfiler();
        if( !edata->Profiler->Init( Engine, profiler_sample_time, profiler_save_to_file, profiler_dynamic_display ) )
            return false;
    }

    for( auto it = BindedFunctions.begin(), end = BindedFunctions.end(); it != end; ++it )
        it->Clear();
    BindedFunctions.clear();
    BindedFunctions.reserve( 10000 );
    BindedFunctions.push_back( BindFunction() );     // None
    BindedFunctions.push_back( BindFunction() );     // Temp
    ScriptFuncBinds.clear();

    Engine->SetModuleUserDataCleanupCallback([] ( asIScriptModule * module )
                                             {
                                                 Preprocessor::DeleteLineNumberTranslator( (Preprocessor::LineNumberTranslator*) module->GetUserData() );
                                                 module->SetUserData( nullptr );
                                             } );

    while( FreeContexts.size() < 10 )
        CreateContext();

    if( !InitThread() )
        return false;

    // Game options callback
    struct GameOptScript
    {
        static bool ScriptLoadModule( const char* module_name )
        {
            FilesCollection scripts( "fos" );
            const char*     path;
            FileManager&    file = scripts.FindFile( module_name, &path );
            if( file.IsLoaded() )
            {
                char dir[ MAX_FOPATH ];
                FileManager::ExtractDir( path, dir );
                return Script::LoadModuleFromFile( module_name, file, dir, nullptr ) && Script::BindImportedFunctions();
            }
            return nullptr;
        }
        static uint ScriptBind( const char* script_name, const char* func_decl, bool temporary_id )
        {
            return Script::BindByScriptName( script_name, func_decl, temporary_id, false );
        }
        static bool ScriptPrepare( uint bind_id )
        {
            return Script::PrepareContext( bind_id, _FUNC_, "ScriptDllCall" );
        }
        static void ScriptSetArgInt8( char value )
        {
            Script::SetArgUChar( value );
        }
        static void ScriptSetArgInt16( short value )
        {
            Script::SetArgUShort( value );
        }
        static void ScriptSetArgInt( int value )
        {
            Script::SetArgUInt( value );
        }
        static void ScriptSetArgInt64( int64 value )
        {
            Script::SetArgUInt64( value );
        }
        static void ScriptSetArgUInt8( uchar value )
        {
            Script::SetArgUChar( value );
        }
        static void ScriptSetArgUInt16( ushort value )
        {
            Script::SetArgUShort( value );
        }
        static void ScriptSetArgUInt( uint value )
        {
            Script::SetArgUInt( value );
        }
        static void ScriptSetArgUInt64( uint64 value )
        {
            Script::SetArgUInt64( value );
        }
        static void ScriptSetArgBool( bool value )
        {
            Script::SetArgBool( value );
        }
        static void ScriptSetArgFloat( float value )
        {
            Script::SetArgFloat( value );
        }
        static void ScriptSetArgDouble( double value )
        {
            Script::SetArgDouble( value );
        }
        static void ScriptSetArgObject( void* value )
        {
            Script::SetArgObject( value );
        }
        static void ScriptSetArgAddress( void* value )
        {
            Script::SetArgAddress( value );
        }
        static bool ScriptRunPrepared()
        {
            return Script::RunPrepared();
        }
        static char ScriptGetReturnedInt8()
        {
            return *(char*) Script::GetReturnedRawAddress();
        }
        static short ScriptGetReturnedInt16()
        {
            return *(short*) Script::GetReturnedRawAddress();
        }
        static int ScriptGetReturnedInt()
        {
            return *(int*) Script::GetReturnedRawAddress();
        }
        static int64 ScriptGetReturnedInt64()
        {
            return *(int64*) Script::GetReturnedRawAddress();
        }
        static uchar ScriptGetReturnedUInt8()
        {
            return *(uchar*) Script::GetReturnedRawAddress();
        }
        static ushort ScriptGetReturnedUInt16()
        {
            return *(ushort*) Script::GetReturnedRawAddress();
        }
        static uint ScriptGetReturnedUInt()
        {
            return *(uint*) Script::GetReturnedRawAddress();
        }
        static uint64 ScriptGetReturnedUInt64()
        {
            return *(uint64*) Script::GetReturnedRawAddress();
        }
        static bool ScriptGetReturnedBool()
        {
            return *(bool*) Script::GetReturnedRawAddress();
        }
        static float ScriptGetReturnedFloat()
        {
            return Script::GetReturnedFloat();
        }
        static double ScriptGetReturnedDouble()
        {
            return Script::GetReturnedDouble();
        }
        static void* ScriptGetReturnedObject()
        {
            return *(void**) Script::GetReturnedRawAddress();
        }
        static void* ScriptGetReturnedAddress()
        {
            return *(void**) Script::GetReturnedRawAddress();
        }
    };
    GameOpt.ScriptLoadModule = &GameOptScript::ScriptLoadModule;
    GameOpt.ScriptBind = &GameOptScript::ScriptBind;
    GameOpt.ScriptPrepare = &GameOptScript::ScriptPrepare;
    GameOpt.ScriptSetArgInt8 = &GameOptScript::ScriptSetArgInt8;
    GameOpt.ScriptSetArgInt16 = &GameOptScript::ScriptSetArgInt16;
    GameOpt.ScriptSetArgInt = &GameOptScript::ScriptSetArgInt;
    GameOpt.ScriptSetArgInt64 = &GameOptScript::ScriptSetArgInt64;
    GameOpt.ScriptSetArgUInt8 = &GameOptScript::ScriptSetArgUInt8;
    GameOpt.ScriptSetArgUInt16 = &GameOptScript::ScriptSetArgUInt16;
    GameOpt.ScriptSetArgUInt = &GameOptScript::ScriptSetArgUInt;
    GameOpt.ScriptSetArgUInt64 = &GameOptScript::ScriptSetArgUInt64;
    GameOpt.ScriptSetArgBool = &GameOptScript::ScriptSetArgBool;
    GameOpt.ScriptSetArgFloat = &GameOptScript::ScriptSetArgFloat;
    GameOpt.ScriptSetArgDouble = &GameOptScript::ScriptSetArgDouble;
    GameOpt.ScriptSetArgObject = &GameOptScript::ScriptSetArgObject;
    GameOpt.ScriptSetArgAddress = &GameOptScript::ScriptSetArgAddress;
    GameOpt.ScriptRunPrepared = &GameOptScript::ScriptRunPrepared;
    GameOpt.ScriptGetReturnedInt8 = &GameOptScript::ScriptGetReturnedInt8;
    GameOpt.ScriptGetReturnedInt16 = &GameOptScript::ScriptGetReturnedInt16;
    GameOpt.ScriptGetReturnedInt = &GameOptScript::ScriptGetReturnedInt;
    GameOpt.ScriptGetReturnedInt64 = &GameOptScript::ScriptGetReturnedInt64;
    GameOpt.ScriptGetReturnedUInt8 = &GameOptScript::ScriptGetReturnedUInt8;
    GameOpt.ScriptGetReturnedUInt16 = &GameOptScript::ScriptGetReturnedUInt16;
    GameOpt.ScriptGetReturnedUInt = &GameOptScript::ScriptGetReturnedUInt;
    GameOpt.ScriptGetReturnedUInt64 = &GameOptScript::ScriptGetReturnedUInt64;
    GameOpt.ScriptGetReturnedBool = &GameOptScript::ScriptGetReturnedBool;
    GameOpt.ScriptGetReturnedFloat = &GameOptScript::ScriptGetReturnedFloat;
    GameOpt.ScriptGetReturnedDouble = &GameOptScript::ScriptGetReturnedDouble;
    GameOpt.ScriptGetReturnedObject = &GameOptScript::ScriptGetReturnedObject;
    GameOpt.ScriptGetReturnedAddress = &GameOptScript::ScriptGetReturnedAddress;

    ScriptWatcherFinish = false;
    ScriptWatcherThread.Start( ScriptWatcher, "ScriptWatcher" );
    return true;
}

void Script::Finish()
{
    if( !Engine )
        return;

    EngineData* edata = (EngineData*) Engine->GetUserData();
    if( edata->Profiler )
        edata->Profiler->Finish();
    SAFEDEL( edata->Profiler );
    SAFEDEL( edata->Invoker );
    RunTimeoutAbort = 0;
    RunTimeoutMessage = 0;
    ScriptWatcherFinish = true;
    ScriptWatcherThread.Wait();

    for( auto it = BindedFunctions.begin(), end = BindedFunctions.end(); it != end; ++it )
        it->Clear();
    BindedFunctions.clear();
    ScriptFuncBinds.clear();

    Preprocessor::SetPragmaCallback( nullptr );
    Preprocessor::UndefAll();
    UnloadScripts();

    RUNTIME_ASSERT( BusyContexts.empty() );
    while( !FreeContexts.empty() )
        FinishContext( FreeContexts[ 0 ] );

    FinishEngine( Engine );     // Finish default engine
    FinishThread();
}

bool Script::InitThread()
{
    // Dummy
    return true;
}

void Script::FinishThread()
{
    asThreadCleanup();
}

void* Script::LoadDynamicLibrary( const char* dll_name )
{
    // Check for disabled client native calls
    EngineData* edata = (EngineData*) Engine->GetUserData();
    if( !edata->AllowNativeCalls )
    {
        WriteLogF( _FUNC_, " - Unable to load dll '%s', native calls not allowed.\n", dll_name );
        return nullptr;
    }

    // Find in already loaded
    char dll_name_lower[ MAX_FOPATH ];
    Str::Copy( dll_name_lower, dll_name );
    #ifdef FO_WINDOWS
    Str::Lower( dll_name_lower );
    #endif
    auto it = edata->LoadedDlls.find( dll_name_lower );
    if( it != edata->LoadedDlls.end() )
        return it->second.second;

    // Make path
    char dll_path[ MAX_FOPATH ];
    Str::Copy( dll_path, dll_name_lower );
    FileManager::EraseExtension( dll_path );

    #if defined ( FO_X64 )
    // Add '64' appendix
    Str::Append( dll_path, "64" );
    #endif

    // Client path fixes
    #ifdef FONLINE_CLIENT
    Str::Insert( dll_path, FileManager::GetDataPath( "", PT_CACHE ) );
    Str::Replacement( dll_path, '\\', '.' );
    Str::Replacement( dll_path, '/', '.' );
    #endif

    // Server path fixes
    #ifdef FONLINE_SERVER
    # ifdef FO_WINDOWS
    FilesCollection dlls( "dll" );
    # else
    FilesCollection dlls( "so" );
    # endif
    bool            founded = false;
    while( dlls.IsNextFile() )
    {
        const char* name, * path;
        dlls.GetNextFile( &name, &path, true );
        if( Str::CompareCase( dll_path, name ) )
        {
            founded = true;
            Str::Copy( dll_path, FileManager::GetDataPath( path, PT_SERVER_MODULES ) );
            break;
        }
    }
    if( !founded )
    {
        # ifdef FO_WINDOWS
        Str::Append( dll_path, ".dll" );
        # else
        Str::Append( dll_path, ".so" );
        # endif
    }
    #endif

    // Set current directory to DLL
    char new_path[ MAX_FOPATH ];
    FileManager::ExtractDir( dll_path, new_path );
    ResolvePath( new_path );
    char cur_path[ MAX_FOPATH ];
    #ifdef FO_WINDOWS
    GetCurrentDirectory( MAX_FOPATH, cur_path );
    SetCurrentDirectory( new_path );
    #else
    getcwd( cur_path, MAX_FOPATH );
    chdir( new_path );
    #endif
    char file_name[ MAX_FOPATH ];
    FileManager::ExtractFileName( dll_path, file_name );

    // Load dynamic library
    void* dll = DLL_Load( file_name );
    #ifdef FO_WINDOWS
    SetCurrentDirectory( cur_path );
    #else
    chdir( cur_path );
    #endif
    if( !dll )
        return nullptr;

    // Verify compilation target
    size_t* ptr = DLL_GetAddress( dll, edata->DllTarget.c_str() );
    if( !ptr )
    {
        WriteLogF( _FUNC_, " - Wrong script DLL '%s', expected target '%s', but found '%s%s%s%s'.\n", dll_name, edata->DllTarget.c_str(),
                   DLL_GetAddress( dll, "SERVER" ) ? "SERVER" : "", DLL_GetAddress( dll, "CLIENT" ) ? "CLIENT" : "", DLL_GetAddress( dll, "MAPPER" ) ? "MAPPER" : "",
                   !DLL_GetAddress( dll, "SERVER" ) && !DLL_GetAddress( dll, "CLIENT" ) && !DLL_GetAddress( dll, "MAPPER" ) ? "Nothing" : "" );
        DLL_Free( dll );
        return nullptr;
    }

    // Register variables
    ptr = DLL_GetAddress( dll, "FOnline" );
    if( ptr )
        *ptr = (size_t) &GameOpt;
    ptr = DLL_GetAddress( dll, "ASEngine" );
    if( ptr )
        *ptr = (size_t) Engine;

    // Register functions
    ptr = DLL_GetAddress( dll, "RaiseAssert" );
    if( ptr )
        *ptr = (size_t) &RaiseAssert;
    ptr = DLL_GetAddress( dll, "Log" );
    if( ptr )
        *ptr = (size_t) &WriteLog;
    ptr = DLL_GetAddress( dll, "ScriptGetActiveContext" );
    if( ptr )
        *ptr = (size_t) &asGetActiveContext;
    ptr = DLL_GetAddress( dll, "ScriptGetLibraryOptions" );
    if( ptr )
        *ptr = (size_t) &asGetLibraryOptions;
    ptr = DLL_GetAddress( dll, "ScriptGetLibraryVersion" );
    if( ptr )
        *ptr = (size_t) &asGetLibraryVersion;

    // Call init function
    typedef void ( *DllMainEx )( bool );
    DllMainEx func = (DllMainEx) DLL_GetAddress( dll, "DllMainEx" );
    if( func )
        (func) ( LoadLibraryCompiler );

    // Add to collection for current engine
    auto value = PAIR( string( dll_path ), dll );
    edata->LoadedDlls.insert( PAIR( string( dll_name_lower ), value ) );

    return dll;
}

void Script::SetConcurrentExecution( bool enabled )
{
    ConcurrentExecution = enabled;
}

void Script::SetLoadLibraryCompiler( bool enabled )
{
    LoadLibraryCompiler = enabled;
}

void Script::UnloadScripts()
{
    for( asUINT i = 0, j = Engine->GetModuleCount(); i < j; i++ )
    {
        asIScriptModule* module = Engine->GetModuleByIndex( i );
        int              result = module->ResetGlobalVars();
        if( result < 0 )
            WriteLogF( _FUNC_, " - Reset global vars fail, module '%s', error %d.\n", module->GetName(), result );
        result = module->UnbindAllImportedFunctions();
        if( result < 0 )
            WriteLogF( _FUNC_, " - Unbind fail, module '%s', error %d.\n", module->GetName(), result );
    }

    while( Engine->GetModuleCount() > 0 )
        Engine->GetModuleByIndex( 0 )->Discard();
}

bool Script::ReloadScripts( const char* target, const char* cache_pefix )
{
    WriteLog( "Reload scripts...\n" );

    Script::UnloadScripts();

    EngineData* edata = (EngineData*) Engine->GetUserData();
    int         errors = 0;
    bool        load_from_raw = ( cache_pefix == nullptr );

    // Get last write time in cache scripts
    uint64          cache_write_time = 0;
    StrSet          cache_file_names;
    FilesCollection cache_files = FilesCollection( "fosb", PT_SERVER_CACHE );
    while( cache_files.IsNextFile() )
    {
        const char*  file_name;
        FileManager& cache_file = cache_files.GetNextFile( &file_name );
        RUNTIME_ASSERT( cache_file.IsLoaded() );
        cache_write_time = ( cache_write_time ? MIN( cache_write_time, cache_file.GetWriteTime() ) : cache_file.GetWriteTime() );
        cache_file_names.insert( file_name );
        uint bin_version = cache_file.GetBEUInt();
        if( bin_version != FONLINE_VERSION )
        {
            load_from_raw = true;
            cache_file_names.clear();
            break;
        }
    }

    // Get last write time in raw scripts
    FilesCollection raw_files = FilesCollection( "fos" );
    while( !load_from_raw && raw_files.IsNextFile() )
    {
        const char*  file_name;
        FileManager& raw_file = raw_files.GetNextFile( &file_name, NULL, true );
        RUNTIME_ASSERT( raw_file.IsLoaded() );
        if( raw_file.GetWriteTime() > cache_write_time )
        {
            load_from_raw = true;
            break;
        }
    }

    // Collect module names
    vector< pair< string, string > > module_names;
    raw_files.ResetCounter();
    while( raw_files.IsNextFile() )
    {
        const char*  file_name, * file_full_name;
        FileManager& file = raw_files.GetNextFile( &file_name, &file_full_name );
        if( !file.IsLoaded() )
        {
            WriteLog( "Unable to open file '%s'.\n", file_name );
            errors++;
            continue;
        }

        // Get first line
        char line[ MAX_FOTEXT ];
        if( !file.GetLine( line, sizeof( line ) ) )
        {
            WriteLog( "Error in script '%s', file empty.\n", file_name );
            errors++;
            continue;
        }

        // Check signature
        if( !Str::Substring( line, "FOS" ) )
        {
            WriteLog( "Error in script '%s', invalid header '%s'.\n", file_name, line );
            errors++;
            continue;
        }

        // Skip headers
        if( Str::Substring( line, "Header" ) )
            continue;

        // Skip different targets
        if( !Str::Substring( line, target ) && !Str::Substring( line, "Common" ) )
            continue;

        // Add file names
        module_names.push_back( PAIR( string( file_name ), string( file_full_name ) ) );

        // Check in cache
        if( !load_from_raw && !cache_file_names.count( ( string( cache_pefix ) + file_name ).c_str() ) )
            load_from_raw = true;
    }

    // Load modules
    for( size_t i = 0; i < module_names.size(); i++ )
    {
        const char* name = module_names[ i ].first.c_str();
        const char* path = module_names[ i ].second.c_str();

        // Load module
        if( load_from_raw )
        {
            char dir[ MAX_FOPATH ];
            FileManager::ExtractDir( path, dir );
            Str::Insert( dir, FileManager::GetWritePath( "", PT_SERVER_MODULES ) );
            if( !LoadModuleFromFile( name, raw_files.FindFile( name ), dir, cache_pefix ) )
            {
                WriteLog( "Load module '%s' from file fail.\n", name );
                errors++;
                continue;
            }
        }
        else
        {
            if( !LoadModuleFromCache( name, cache_pefix ) )
            {
                WriteLog( "Load module '%s' from cache fail. Clear cache.\n", name );
                errors++;
                continue;
            }
        }

        // Add to profiler
        if( edata->Profiler )
        {
            RUNTIME_ASSERT( cache_pefix );
            #ifdef FONLINE_SERVER
            edata->Profiler->AddModule( PT_SERVER_CACHE, name, cache_pefix );
            #else
            edata->Profiler->AddModule( PT_CACHE, name, cache_pefix );
            #endif
        }
    }

    // Finalize profiler
    if( edata->Profiler )
        edata->Profiler->EndModules();

    // Bind imported functions
    if( !errors && !BindImportedFunctions() )
        errors++;

    #ifdef FONLINE_SERVER
    if( !errors && Str::Compare( target, "Server" ) && !RebindFunctions() )
        errors++;
    #endif

    CacheEnumValues();

    if( errors )
    {
        WriteLog( "Reload scripts fail.\n" );
        return false;
    }

    WriteLog( "Reload scripts complete.\n" );
    return true;
}

bool Script::BindReservedFunctions( ReservedScriptFunction* bind_func, uint bind_func_count )
{
    WriteLog( "Bind reserved functions...\n" );

    int errors = 0;

    // Collect functions in all modules
    multimap< string, asIScriptFunction* > all_functions;
    asUINT                                 module_count = Engine->GetModuleCount();
    for( asUINT m = 0; m < module_count; m++ )
    {
        asIScriptModule* module = Engine->GetModuleByIndex( m );
        asUINT           func_count = module->GetFunctionCount();
        for( asUINT f = 0; f < func_count; f++ )
        {
            asIScriptFunction* func = module->GetFunctionByIndex( f );
            all_functions.insert( PAIR( func->GetName(), func ) );
        }
    }

    // Automatically find function
    for( uint i = 0; i < bind_func_count; i++ )
    {
        ReservedScriptFunction* bf = &bind_func[ i ];
        uint                    count = (uint) all_functions.count( bf->FuncName );
        if( count == 1 )
        {
            asIScriptFunction* func = all_functions.find( bf->FuncName )->second;
            int                bind_id = BindByFunc( func, false );
            if( bind_id > 0 )
            {
                *bf->BindId = bind_id;
            }
            else
            {
                WriteLog( "Bind reserved function fail, module '%s', name '%s'.\n", func->GetModuleName(), bf->FuncName );
                errors++;
            }
        }
        else if( count == 0 )
        {
            WriteLog( "Function for reserved function '%s' not founded.\n", bf->FuncName );
            errors++;
        }
        else
        {
            WriteLog( "Multiplied functions founded for reserved function '%s'.\n", bf->FuncName );
            auto it = all_functions.find( bf->FuncName );
            for( uint j = 0; j < count; j++, it++ )
                WriteLog( "- In module '%s'.\n", it->second->GetModuleName() );
            errors++;
        }
    }

    if( errors )
    {
        WriteLog( "Bind reserved functions fail.\n" );
        return false;
    }

    WriteLog( "Bind reserved functions complete.\n" );
    return true;
}

bool Script::RunModuleInitFunctions()
{
    for( asUINT i = 0; i < Engine->GetModuleCount(); i++ )
    {
        asIScriptModule* module = Engine->GetModuleByIndex( i );
        uint             bind_id = Script::BindByScriptName( Str::FormatBuf( "%s@module_init", module->GetName() ), "bool %s()", true, true );
        if( !bind_id )
            bind_id = Script::BindByScriptName( Str::FormatBuf( "%s@ModuleInit", module->GetName() ), "bool %s()", true, true );
        if( bind_id && Script::PrepareContext( bind_id, _FUNC_, "Script" ) )
        {
            if( !Script::RunPrepared() )
            {
                WriteLog( "Error executing init function, module '%s'.\n", module->GetName() );
                return false;
            }

            if( !Script::GetReturnedBool() )
            {
                WriteLog( "Initialization stopped by module '%s'.\n", module->GetName() );
                return false;
            }
        }
    }

    return true;
}

asIScriptEngine* Script::GetEngine()
{
    return Engine;
}

void Script::SetEngine( asIScriptEngine* engine )
{
    Engine = engine;
}

asIScriptEngine* Script::CreateEngine( ScriptPragmaCallback* pragma_callback, const char* dll_target, bool allow_native_calls )
{
    asIScriptEngine* engine = asCreateScriptEngine( ANGELSCRIPT_VERSION );
    if( !engine )
    {
        WriteLogF( _FUNC_, " - asCreateScriptEngine fail.\n" );
        return nullptr;
    }

    engine->SetMessageCallback( asFUNCTION( CallbackMessage ), nullptr, asCALL_CDECL );
    RegisterScriptArray( engine, true );
    RegisterScriptString( engine );
    RegisterScriptAny( engine );
    RegisterScriptDictionary( engine );
    RegisterScriptDict( engine );
    RegisterScriptFile( engine );
    RegisterScriptMath( engine );
    RegisterScriptWeakRef( engine );
    RegisterScriptReflection( engine );

    EngineData* edata = new EngineData();
    edata->PragmaCB = pragma_callback;
    edata->DllTarget = dll_target;
    edata->AllowNativeCalls = allow_native_calls;
    edata->Invoker = new ScriptInvoker();
    edata->Profiler = nullptr;
    engine->SetUserData( edata );
    return engine;
}

void Script::FinishEngine( asIScriptEngine*& engine )
{
    if( engine )
    {
        EngineData* edata = (EngineData*) engine->SetUserData( nullptr );
        delete edata->PragmaCB;
        for( auto it = edata->LoadedDlls.begin(), end = edata->LoadedDlls.end(); it != end; ++it )
            DLL_Free( it->second.second );
        delete edata;
        engine->ShutDownAndRelease();
        engine = nullptr;
    }
}

void Script::CreateContext()
{
    asIScriptContext* ctx = Engine->CreateContext();
    RUNTIME_ASSERT( ctx );
    int               r = ctx->SetExceptionCallback( asFUNCTION( CallbackException ), nullptr, asCALL_CDECL );
    RUNTIME_ASSERT( r >= 0 );

    ContextData* ctx_data = new ContextData();
    memzero( ctx_data, sizeof( ContextData ) );
    ctx->SetUserData( ctx_data );

    EngineData* edata = (EngineData*) Engine->GetUserData();
    if( edata->Profiler )
        ctx->SetLineCallback( asFUNCTION( ProfilerContextCallback ), edata->Profiler, asCALL_CDECL );

    SCOPE_LOCK( ContextsLocker );
    FreeContexts.push_back( ctx );
}

void Script::FinishContext( asIScriptContext* ctx )
{
    ContextsLocker.Lock();
    auto it = std::find( FreeContexts.begin(), FreeContexts.end(), ctx );
    RUNTIME_ASSERT( it != FreeContexts.end() );
    FreeContexts.erase( it );
    ContextsLocker.Unlock();

    delete (ContextData*) ctx->GetUserData();
    ctx->Release();
    ctx = nullptr;
}

asIScriptContext* Script::RequestContext()
{
    SCOPE_LOCK( ContextsLocker );

    if( FreeContexts.empty() )
        CreateContext();

    asIScriptContext* ctx = FreeContexts.back();
    FreeContexts.pop_back();
    BusyContexts.push_back( ctx );
    return ctx;
}

void Script::ReturnContext( asIScriptContext* ctx )
{
    SCOPE_LOCK( ContextsLocker );

    auto it = std::find( BusyContexts.begin(), BusyContexts.end(), ctx );
    RUNTIME_ASSERT( it != BusyContexts.end() );
    BusyContexts.erase( it );
    FreeContexts.push_back( ctx );

    ContextData* ctx_data = (ContextData*) ctx->GetUserData();
    memzero( ctx_data, sizeof( ContextData ) );
}

void Script::GetExecutionContexts( ContextVec& contexts )
{
    ContextsLocker.Lock();
    contexts = BusyContexts;
}

void Script::ReleaseExecutionContexts()
{
    ContextsLocker.Unlock();
}

void Script::RaiseException( const char* message, ... )
{
    asIScriptContext* ctx = asGetActiveContext();
    if( ctx && ctx->GetState() == asEXECUTION_EXCEPTION )
        return;

    va_list list;
    va_start( list, message );
    char    buf[ MAX_FOTEXT ];
    vsnprintf( buf, MAX_FOTEXT, message, list );
    va_end( list );

    if( ctx )
        ctx->SetException( buf );
    else
        HandleException( nullptr, "Engine exception: %s\n", buf );
}

void Script::PassException()
{
    RaiseException( "Pass" );
}

void Script::HandleException( asIScriptContext* ctx, const char* message, ... )
{
    va_list list;
    va_start( list, message );
    char    buf[ MAX_FOTEXT ];
    vsnprintf( buf, MAX_FOTEXT, message, list );
    va_end( list );

    if( ctx )
    {
        Str::Append( buf, "\n" );

        asIScriptContext* ctx_ = ctx;
        while( ctx_ )
        {
            Str::Append( buf, MakeContextTraceback( ctx_, ctx_ == ctx, false ).c_str() );
            ContextData* ctx_data = (ContextData*) ctx_->GetUserData();
            ctx_ = ctx_data->Parent;
        }
    }

    WriteLog( "%s", buf );

    #ifndef FONLINE_SERVER
    CreateDump( "ScriptException" );
    ShowMessage( buf );
    ExitProcess( 0 );
    #endif
}

string Script::MakeContextTraceback( asIScriptContext* ctx, bool include_header, bool extended_header )
{
    string                   result = "";
    char                     buf[ MAX_FOTEXT ];

    ContextData*             ctx_data = (ContextData*) ctx->GetUserData();
    int                      line;
    const asIScriptFunction* func;
    int                      stack_size = ctx->GetCallstackSize();

    // Header
    if( include_header )
    {
        if( !extended_header )
            result += Str::Format( buf, " Traceback:\n" );
        else
            result += Str::Format( buf, " Traceback: (%s, %s)\n", ContextStatesStr[ (int) ctx->GetState() ], ctx_data->Info );
    }

    // Print system function
    asIScriptFunction* sys_func = ctx->GetSystemFunction();
    if( sys_func )
        result += Str::Format( buf, "  %s\n", sys_func->GetDeclaration( true, true, true ) );

    // Print current function
    if( ctx->GetState() == asEXECUTION_EXCEPTION )
    {
        line = ctx->GetExceptionLineNumber();
        func = ctx->GetExceptionFunction();
    }
    else
    {
        line = ctx->GetLineNumber( 0 );
        func = ctx->GetFunction( 0 );
    }
    if( func )
    {
        Preprocessor::LineNumberTranslator* lnt = (Preprocessor::LineNumberTranslator*) func->GetModule()->GetUserData();
        result += Str::Format( buf, "  %s : %s : Line %d\n", Preprocessor::ResolveOriginalFile( line, lnt ), func->GetDeclaration(), Preprocessor::ResolveOriginalLine( line, lnt ) );
    }

    // Print call stack
    for( int i = 1; i < stack_size; i++ )
    {
        func = ctx->GetFunction( i );
        line = ctx->GetLineNumber( i );
        if( func )
        {
            Preprocessor::LineNumberTranslator* lnt = (Preprocessor::LineNumberTranslator*) func->GetModule()->GetUserData();
            result += Str::Format( buf, "  %s : %s : Line %d\n", Preprocessor::ResolveOriginalFile( line, lnt ), func->GetDeclaration(), Preprocessor::ResolveOriginalLine( line, lnt ) );
        }
    }

    return result;
}

ScriptInvoker* Script::GetInvoker()
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    return edata->Invoker;
}

string Script::GetDeferredCallsStatistics()
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    return edata->Invoker->GetStatistics();
}

void Script::ProcessDeferredCalls()
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    edata->Invoker->Process();
}

void Script::SaveDeferredCalls( IniParser& data )
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    edata->Invoker->SaveDeferredCalls( data );
}

bool Script::LoadDeferredCalls( IniParser& data )
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    return edata->Invoker->LoadDeferredCalls( data );
}

void Script::ProfilerContextCallback( asIScriptContext* ctx, void* obj )
{
    ScriptProfiler* profiler = (ScriptProfiler*) obj;
    profiler->Process( ctx );
}

string Script::GetProfilerStatistics()
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    if( edata->Profiler )
        return edata->Profiler->GetStatistics();
    return "Profiler not enabled.";
}

bool Script::RestoreEntity( const char* class_name, uint id, const StrMap& props_data )
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    return edata->PragmaCB->RestoreEntity( class_name, id, props_data );
}

const char* Script::GetActiveModuleName()
{
    asIScriptContext* ctx = asGetActiveContext();
    if( !ctx )
        return nullptr;
    asIScriptFunction* func = ctx->GetFunction( 0 );
    if( !func )
        return nullptr;
    return func->GetModuleName();
}

const char* Script::GetActiveFuncName()
{
    static const char error[] = "<error>";
    asIScriptContext* ctx = asGetActiveContext();
    if( !ctx )
        return error;
    asIScriptFunction* func = ctx->GetFunction( 0 );
    if( !func )
        return error;
    return func->GetName();
}

void ScriptWatcher( void* )
{
    Script::Watcher();
}

void Script::Watcher()
{
    while( !ScriptWatcherFinish )
    {
        // Execution timeout
        if( RunTimeoutAbort )
        {
            SCOPE_LOCK( ContextsLocker );

            uint cur_tick = Timer::FastTick();
            for( auto it = BusyContexts.begin(); it != BusyContexts.end(); ++it )
            {
                asIScriptContext* ctx = *it;
                ContextData*      ctx_data = (ContextData*) ctx->GetUserData();
                if( ctx->GetState() == asEXECUTION_ACTIVE && ctx_data->StartTick && cur_tick >= ctx_data->StartTick + RunTimeoutAbort )
                    ctx->Abort();
            }
        }

        Thread::Sleep( 100 );
    }
}

void Script::SetRunTimeout( uint abort_timeout, uint message_timeout )
{
    RunTimeoutAbort = abort_timeout;
    RunTimeoutMessage = message_timeout;
}

/************************************************************************/
/* Load / Bind                                                          */
/************************************************************************/

void Script::Define( const char* def, ... )
{
    va_list list;
    va_start( list, def );
    char    buf[ MAX_FOTEXT ];
    vsnprintf( buf, MAX_FOTEXT, def, list );
    va_end( list );
    Preprocessor::Define( buf );
}

void Script::Undef( const char* def )
{
    if( def )
        Preprocessor::Undef( def );
    else
        Preprocessor::UndefAll();
}

void Script::CallPragmas( const Pragmas& pragmas )
{
    EngineData* edata = (EngineData*) Engine->GetUserData();

    // Set current pragmas
    Preprocessor::SetPragmaCallback( edata->PragmaCB );

    // Call pragmas
    for( size_t i = 0; i < pragmas.size(); i++ )
        Preprocessor::CallPragma( pragmas[ i ] );
}

bool Script::LoadModuleFromFile( const char* module_name, FileManager& file, const char* dir, const char* cache_pefix )
{
    RUNTIME_ASSERT( file.IsLoaded() );

    // Binary version
    uint version = FONLINE_VERSION;

    // Set current pragmas
    EngineData* edata = (EngineData*) Engine->GetUserData();
    Preprocessor::SetPragmaCallback( edata->PragmaCB );

    // File loader
    class MemoryFileLoader: public Preprocessor::FileLoader
    {
public:
        FileManager* InitialFile;
        MemoryFileLoader( FileManager& file ): InitialFile( &file ) {}
        virtual ~MemoryFileLoader() {}

        virtual bool LoadFile( const std::string& dir, const std::string& file_name, std::vector< char >& data )
        {
            if( InitialFile )
            {
                data.resize( InitialFile->GetFsize() );
                if( !data.empty() )
                    memcpy( &data[ 0 ], InitialFile->GetBuf(), data.size() );
                InitialFile = nullptr;
                return true;
            }

            char fname[ MAX_FOPATH ];
            Str::Copy( fname, dir.c_str() );
            Str::Append( fname, file_name.c_str() );
            FileManager::FormatPath( fname );

            void* file = FileOpen( fname, false );
            if( file )
            {
                uint size = FileGetSize( file );
                data.resize( size );
                if( size )
                {
                    if( !FileRead( file, &data[ 0 ], size ) )
                    {
                        FileClose( file );
                        return false;
                    }
                }
                FileClose( file );
                return true;
            }
            return false;
        }
    };

    // Base script path
    char path[ MAX_FOPATH ];
    Str::Copy( path, dir );
    Str::Append( path, module_name );
    const char* ext = FileManager::GetExtension( module_name );
    bool        is_header = ( ext && Str::CompareCase( ext, "fosh" ) );
    if( !is_header )
        Str::Append( path, ".fos" );
    else
        FileManager::EraseExtension( path );

    // Preprocess
    MemoryFileLoader              loader( file );
    Preprocessor::StringOutStream result, errors;
    int                           errors_count = Preprocessor::Preprocess( path, result, &errors, &loader );

    if( errors.String != "" )
    {
        while( errors.String[ errors.String.length() - 1 ] == '\n' )
            errors.String.pop_back();
        WriteLogF( _FUNC_, " - Module '%s' preprocessor message '%s'.\n", module_name, errors.String.c_str() );
    }

    if( errors_count )
    {
        WriteLogF( _FUNC_, " - Unable to preprocess module '%s'.\n", module_name );
        return false;
    }

    // Delete loaded module
    for( asUINT i = 0, j = Engine->GetModuleCount(); i < j; i++ )
    {
        asIScriptModule* module = Engine->GetModuleByIndex( i );
        if( Str::Compare( module->GetName(), module_name ) )
        {
            WriteLogF( _FUNC_, " - Warning, script for this name '%s' already exist. Discard it.\n", module_name );
            module->Discard();
            break;
        }
    }

    // Add new
    asIScriptModule* module = Engine->GetModule( module_name, asGM_ALWAYS_CREATE );
    if( !module )
    {
        WriteLogF( _FUNC_, " - Create module fail, script '%s'.\n", module_name );
        return false;
    }

    // Store line number translator
    Preprocessor::LineNumberTranslator* lnt = Preprocessor::GetLineNumberTranslator();
    UCharVec                            lnt_data;
    Preprocessor::StoreLineNumberTranslator( lnt, lnt_data );
    module->SetUserData( lnt );

    // Add single script section
    int as_result = module->AddScriptSection( module_name, result.String.c_str() );
    if( as_result < 0 )
    {
        WriteLogF( _FUNC_, " - Unable to AddScriptSection module '%s', result %d.\n", module_name, as_result );
        module->Discard();
        return false;
    }

    // Build module
    as_result = module->Build();
    if( as_result < 0 )
    {
        WriteLogF( _FUNC_, " - Unable to Build module '%s', result %d.\n", module_name, as_result );
        module->Discard();
        return false;
    }

    // Save binary in cache
    if( cache_pefix )
    {
        CBytecodeStream binary;
        if( module->SaveByteCode( &binary ) >= 0 )
        {
            std::vector< asBYTE >& data = binary.GetBuf();
            const Pragmas&         pragmas = edata->PragmaCB->GetProcessedPragmas();

            // Fill data
            FileManager file_bin;
            file_bin.SetBEUInt( version );
            file_bin.SetBEUInt( (uint) pragmas.size() );
            for( uint i = 0, j = (uint) pragmas.size(); i < j; i++ )
            {
                const Preprocessor::PragmaInstance& pragma = pragmas[ i ];
                file_bin.SetStrNT( "%s", pragma.Name.c_str() );
                file_bin.SetStrNT( "%s", pragma.Text.c_str() );
                file_bin.SetStrNT( "%s", pragma.CurrentFile.c_str() );
                file_bin.SetBEUInt( pragma.CurrentFileLine );
                file_bin.SetStrNT( "%s", pragma.RootFile.c_str() );
                file_bin.SetBEUInt( pragma.GlobalLine );
            }
            file_bin.SetBEUInt( (uint) lnt_data.size() );
            file_bin.SetData( &lnt_data[ 0 ], (uint) lnt_data.size() );
            file_bin.SetData( &data[ 0 ], (uint) data.size() );

            // Flush on disk
            char bin_fname[ MAX_FOPATH ];
            Str::Format( bin_fname, "%s%s.fosb", cache_pefix, module_name );
            if( !file_bin.SaveOutBufToFile( bin_fname, PT_SERVER_CACHE ) )
                WriteLogF( _FUNC_, " - Can't save bytecode, script '%s'.\n", module_name );
        }
        else
        {
            WriteLogF( _FUNC_, " - Can't write bytecode, script '%s'.\n", module_name );
        }

        // Safe preprocessor output
        char        prep_fname[ MAX_FOPATH ];
        Str::Format( prep_fname, "%s%s.fosp", cache_pefix, module_name );
        FileManager file_prep;
        FormatPreprocessorOutput( result.String );
        file_prep.SetData( (void*) result.String.c_str(), (uint) result.String.length() );
        if( !file_prep.SaveOutBufToFile( prep_fname, PT_SERVER_CACHE ) )
            WriteLogF( _FUNC_, " - Can't write preprocessed file, module '%s'.\n", module_name );
    }

    return true;
}

bool Script::LoadModuleFromCache( const char* module_name, const char* cache_pefix )
{
    RUNTIME_ASSERT( cache_pefix );

    // Load binary file
    char        bin_fname[ MAX_FOPATH ];
    Str::Format( bin_fname, "%s%s.fosb", cache_pefix, module_name );
    FileManager file_bin;
    file_bin.LoadFile( bin_fname, PT_SERVER_CACHE );
    RUNTIME_ASSERT( file_bin.IsLoaded() );
    uint bin_version = file_bin.GetBEUInt();
    RUNTIME_ASSERT( bin_version == FONLINE_VERSION );

    // Load pragmas
    uint    pragmas_count = file_bin.GetBEUInt();
    Pragmas pragmas;
    for( uint i = 0; i < pragmas_count; i++ )
    {
        Preprocessor::PragmaInstance pragma;
        char                         str[ MAX_FOTEXT ];
        file_bin.GetStrNT( str );
        pragma.Name = str;
        file_bin.GetStrNT( str );
        pragma.Text = str;
        file_bin.GetStrNT( str );
        pragma.CurrentFile = str;
        pragma.CurrentFileLine = file_bin.GetBEUInt();
        file_bin.GetStrNT( str );
        pragma.RootFile = str;
        pragma.GlobalLine = file_bin.GetBEUInt();
        pragmas.push_back( pragma );
    }

    // Load line number translator data
    uint     lnt_data_size = file_bin.GetBEUInt();
    UCharVec lnt_data( lnt_data_size );
    file_bin.CopyMem( &lnt_data[ 0 ], lnt_data_size );

    // Delete old
    for( asUINT i = 0, j = Engine->GetModuleCount(); i < j; i++ )
    {
        asIScriptModule* module = Engine->GetModuleByIndex( i );
        if( Str::Compare( module->GetName(), module_name ) )
        {
            WriteLogF( _FUNC_, " - Warning, module for this name '%s' already exist. Discard it.\n", module_name );
            module->Discard();
            break;
        }
    }

    // Create new
    asIScriptModule* module = Engine->GetModule( module_name, asGM_ALWAYS_CREATE );
    if( !module )
    {
        WriteLogF( _FUNC_, " - Create module '%s' fail. Make clean up.\n", module_name );
        return false;
    }

    // Set line number translator
    Preprocessor::LineNumberTranslator* lnt = Preprocessor::RestoreLineNumberTranslator( lnt_data );
    module->SetUserData( lnt );

    // Call pragmas
    CallPragmas( pragmas );

    // Restore from bytecode
    CBytecodeStream binary;
    binary.Write( file_bin.GetCurBuf(), file_bin.GetFsize() - file_bin.GetCurPos() );
    if( module->LoadByteCode( &binary ) < 0 )
    {
        WriteLogF( _FUNC_, " - Can't load binary for module '%s'. Make clean up.\n", module_name );
        return false;
    }
    return true;
}

bool Script::RestoreModuleFromBinary( const char* module_name, const UCharVec& bytecode, const UCharVec& lnt_data )
{
    RUNTIME_ASSERT( !bytecode.empty() );
    RUNTIME_ASSERT( !lnt_data.empty() );

    for( asUINT i = 0, j = Engine->GetModuleCount(); i < j; i++ )
    {
        asIScriptModule* module = Engine->GetModuleByIndex( i );
        if( Str::Compare( module->GetName(), module_name ) )
        {
            WriteLogF( _FUNC_, " - Warning, script for this name '%s' already exist. Discard it.\n", module_name );
            module->Discard();
            break;
        }
    }

    asIScriptModule* module = Engine->GetModule( module_name, asGM_ALWAYS_CREATE );
    if( !module )
    {
        WriteLogF( _FUNC_, " - Create module fail, script '%s'.\n", module_name );
        return false;
    }

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::RestoreLineNumberTranslator( lnt_data );
    module->SetUserData( lnt );

    CBytecodeStream binary;
    binary.Write( &bytecode[ 0 ], (asUINT) bytecode.size() );
    int             result = module->LoadByteCode( &binary );
    if( result < 0 )
    {
        WriteLogF( _FUNC_, " - Can't load binary, module '%s', result %d.\n", module_name, result );
        module->Discard();
        return false;
    }

    return true;
}

bool Script::BindImportedFunctions()
{
    int errors = 0;
    for( asUINT i = 0; i < Engine->GetModuleCount(); i++ )
    {
        asIScriptModule* module = Engine->GetModuleByIndex( i );
        for( asUINT j = 0; j < module->GetImportedFunctionCount(); j++ )
        {
            const char*      name = module->GetImportedFunctionSourceModule( j );
            const char*      decl = module->GetImportedFunctionDeclaration( j );

            asIScriptModule* mod = Engine->GetModule( name, asGM_ONLY_IF_EXISTS );
            if( !mod )
            {
                WriteLog( "Module '%s' fail to bind: import %s from \"%s\" - source module does not exists.\n", module->GetName(), decl, name );
                errors++;
                continue;
            }

            asIScriptFunction* func = mod->GetFunctionByDecl( decl );
            if( !func )
            {
                WriteLog( "Module '%s' fail to bind: import %s from \"%s\" - source function does not exists.\n", module->GetName(), decl, name );
                errors++;
                continue;
            }

            int r = module->BindImportedFunction( j, func );
            if( r < 0 )
            {
                WriteLog( "Module '%s' fail to bind: import %s from \"%s\" - bind error %d.\n", module->GetName(), decl, name, r );
                errors++;
                continue;
            }
        }
    }
    if( errors )
    {
        for( asUINT i = 0, j = Engine->GetModuleCount(); i < j; i++ )
        {
            asIScriptModule* module = Engine->GetModuleByIndex( i );
            for( asUINT m = 0; m < module->GetImportedFunctionCount(); m++ )
                module->UnbindAllImportedFunctions();
        }
    }
    return errors == 0;
}

uint Script::BindByModuleFuncName( const char* module_name, const char* func_name, const char* decl, bool is_temp, bool disable_log /* = false */ )
{
    #ifdef SCRIPT_MULTITHREADING
    SCOPE_LOCK( BindedFunctionsLocker );
    #endif

    // Detect native dll
    if( !Str::Substring( module_name, ".dll" ) )
    {
        // Find module
        asIScriptModule* module = Engine->GetModule( module_name, asGM_ONLY_IF_EXISTS );
        if( !module )
        {
            if( !disable_log )
                WriteLogF( _FUNC_, " - Module '%s' not found.\n", module_name );
            return 0;
        }

        // Find function
        char decl_[ MAX_FOTEXT ];
        if( decl && decl[ 0 ] )
            Str::Format( decl_, decl, func_name );
        else
            Str::Copy( decl_, func_name );
        asIScriptFunction* script_func = module->GetFunctionByDecl( decl_ );
        if( !script_func )
        {
            if( !disable_log )
                WriteLogF( _FUNC_, " - Function '%s' in module '%s' not found.\n", decl_, module_name );
            return 0;
        }

        // Save to temporary bind
        if( is_temp )
        {
            BindedFunctions[ 1 ].IsScriptCall = true;
            BindedFunctions[ 1 ].ScriptFunc = script_func;
            BindedFunctions[ 1 ].NativeFuncAddr = 0;
            return 1;
        }

        // Find already binded
        for( int i = 2, j = (int) BindedFunctions.size(); i < j; i++ )
        {
            BindFunction& bf = BindedFunctions[ i ];
            if( bf.IsScriptCall && bf.ScriptFunc == script_func )
                return i;
        }

        // Create new bind
        BindedFunctions.push_back( BindFunction( script_func ) );
    }
    else
    {
        // Load dynamic library
        void* dll = LoadDynamicLibrary( module_name );
        if( !dll )
        {
            if( !disable_log )
                WriteLogF( _FUNC_, " - Dll '%s' not found in scripts folder, error '%s'.\n", module_name, DLL_Error() );
            return 0;
        }

        // Load function
        size_t func = (size_t) DLL_GetAddress( dll, func_name );
        if( !func )
        {
            if( !disable_log )
                WriteLogF( _FUNC_, " - Function '%s' in dll '%s' not found, error '%s'.\n", func_name, module_name, DLL_Error() );
            return 0;
        }

        // Save to temporary bind
        if( is_temp )
        {
            BindedFunctions[ 1 ].IsScriptCall = false;
            BindedFunctions[ 1 ].ScriptFunc = nullptr;
            BindedFunctions[ 1 ].NativeFuncAddr = func;
            return 1;
        }

        // Find already binded
        for( int i = 2, j = (int) BindedFunctions.size(); i < j; i++ )
        {
            BindFunction& bf = BindedFunctions[ i ];
            if( !bf.IsScriptCall && bf.NativeFuncAddr == func )
                return i;
        }

        // Create new bind
        BindedFunctions.push_back( BindFunction( func, module_name, func_name ) );
    }
    return (uint) BindedFunctions.size() - 1;
}

uint Script::BindByScriptName( const char* script_name, const char* decl, bool is_temp, bool disable_log /* = false */ )
{
    char module_name[ MAX_FOTEXT ];
    char func_name[ MAX_FOTEXT ];
    if( !ParseScriptName( script_name, module_name, func_name, disable_log ) )
        return 0;
    return BindByModuleFuncName( module_name, func_name, decl, is_temp, disable_log );
}

uint Script::BindByFuncNameInRuntime( const char* func_name, const char* decl, bool is_temp, bool disable_log /* = false */ )
{
    char script_name[ MAX_FOTEXT ];
    Script::MakeScriptNameInRuntime( func_name, script_name );
    return BindByScriptName( script_name, decl, is_temp, disable_log );
}

uint Script::BindByFunc( asIScriptFunction* func, bool is_temp, bool disable_log /* = false */ )
{
    // Save to temporary bind
    if( is_temp )
    {
        BindedFunctions[ 1 ].IsScriptCall = true;
        BindedFunctions[ 1 ].ScriptFunc = func;
        BindedFunctions[ 1 ].NativeFuncAddr = 0;
        return 1;
    }

    // Find already binded
    for( int i = 2, j = (int) BindedFunctions.size(); i < j; i++ )
    {
        BindFunction& bf = BindedFunctions[ i ];
        if( bf.IsScriptCall && bf.ScriptFunc == func )
            return i;
    }

    // Create new bind
    BindedFunctions.push_back( BindFunction( func ) );
    return (uint) BindedFunctions.size() - 1;
}

uint Script::BindByFuncNum( hash func_num, bool is_temp, bool disable_log /* = false */ )
{
    asIScriptFunction* func = FindFunc( func_num );
    if( !func )
    {
        if( !disable_log )
            WriteLogF( _FUNC_, " - Function '%s' not found.\n", Str::GetName( func_num ) );
        return 0;
    }

    return BindByFunc( func, is_temp, disable_log );
}

bool Script::RebindFunctions()
{
    #ifdef SCRIPT_MULTITHREADING
    SCOPE_LOCK( BindedFunctionsLocker );
    #endif

    int errors = 0;
    for( int i = 2, j = (int) BindedFunctions.size(); i < j; i++ )
    {
        BindFunction& bf = BindedFunctions[ i ];
        if( bf.IsScriptCall )
        {
            if( bf.ScriptFunc )
                bf.ScriptFunc->Release();

            int bind_id = BindByModuleFuncName( bf.ModuleName.c_str(), bf.FuncName.c_str(), nullptr, true, false );
            if( bind_id <= 0 )
            {
                WriteLogF( _FUNC_, " - Unable to bind function, module '%s', function '%s'.\n", bf.ModuleName.c_str(), bf.FuncName.c_str() );
                bf.ScriptFunc = nullptr;
                errors++;
            }
            else
            {
                bf.ScriptFunc = BindedFunctions[ 1 ].ScriptFunc;
                bf.ScriptFunc->AddRef();
            }
        }
    }
    return errors == 0;
}

bool Script::ParseScriptName( const char* script_name, char* module_name, char* func_name, bool disable_log /* = false */ )
{
    RUNTIME_ASSERT( script_name );
    RUNTIME_ASSERT( module_name );
    RUNTIME_ASSERT( func_name );

    if( !Str::Substring( script_name, "@" ) )
    {
        if( !disable_log )
            WriteLog( "Script name parse error, string '%s'.\n", script_name );
        return false;
    }

    char str[ MAX_FOTEXT ];
    Str::Copy( str, script_name );
    Str::EraseChars( str, ' ' );
    Str::CopyWord( module_name, str, '@' );
    char* str_ = &str[ 0 ];
    Str::GoTo( str_, '@', true );
    Str::CopyWord( func_name, str_, 0 );

    if( !module_name[ 0 ] )
    {
        if( !disable_log )
            WriteLog( "Module name parse error, string '%s'.\n", script_name );
        return false;
    }
    if( !func_name[ 0 ] )
    {
        if( !disable_log )
            WriteLog( "Function name parse error, string '%s'.\n", script_name );
        return false;
    }
    return true;
}

string Script::GetBindFuncName( uint bind_id )
{
    #ifdef SCRIPT_MULTITHREADING
    SCOPE_LOCK( BindedFunctionsLocker );
    #endif

    if( !bind_id || bind_id >= (uint) BindedFunctions.size() )
    {
        WriteLogF( _FUNC_, " - Wrong bind id %u, bind buffer size %u.\n", bind_id, BindedFunctions.size() );
        return "";
    }

    BindFunction& bf = BindedFunctions[ bind_id ];
    string        result;
    result += bf.ModuleName;
    result += " : ";
    result += bf.FuncName;
    return result;
}

void Script::MakeScriptNameInRuntime( const char* func_name, char(&script_name)[ MAX_FOTEXT ] )
{
    RUNTIME_ASSERT( func_name );
    const char* active_module_name = GetActiveModuleName();
    RUNTIME_ASSERT( active_module_name );

    // Copy base
    if( script_name != func_name )
        Str::Copy( script_name, func_name );

    // Append module
    const char* mod_separator = Str::Substring( func_name, "@" );
    if( !mod_separator )
    {
        Str::Insert( script_name, "@" );
        Str::Insert( script_name, active_module_name );
    }
}

/************************************************************************/
/* Functions association                                                */
/************************************************************************/

hash Script::GetFuncNum( asIScriptFunction* func )
{
    uint64 h = (uint64) func->GetUserData();
    hash   func_num = (hash) h;
    if( !func_num )
    {
        char script_name[ MAX_FOTEXT ];
        Str::Copy( script_name, func->GetModuleName() );
        Str::Append( script_name, "@" );
        Str::Append( script_name, func->GetName() );
        func_num = Str::GetHash( script_name );
        func->SetUserData( (void*) func_num );
    }
    return func_num;
}

asIScriptFunction* Script::FindFunc( hash func_num )
{
    for( asUINT i = 0, j = Engine->GetModuleCount(); i < j; i++ )
    {
        asIScriptModule* module = Engine->GetModuleByIndex( i );
        for( asUINT n = 0, m = module->GetFunctionCount(); n < m; n++ )
        {
            asIScriptFunction* func = module->GetFunctionByIndex( n );
            if( GetFuncNum( func ) == func_num )
                return func;
        }
    }
    return nullptr;
}

hash Script::BindScriptFuncNumByFuncNameInRuntime( const char* func_name, const char* decl )
{
    char script_name[ MAX_FOTEXT ];
    Script::MakeScriptNameInRuntime( func_name, script_name );
    return BindScriptFuncNumByScriptName( script_name, decl );
}

hash Script::BindScriptFuncNumByScriptName( const char* script_name, const char* decl )
{
    #ifdef SCRIPT_MULTITHREADING
    SCOPE_LOCK( BindedFunctionsLocker );
    #endif

    // Bind function
    int bind_id = Script::BindByScriptName( script_name, decl, false );
    if( bind_id <= 0 )
        return 0;

    // Native and broken binds not allowed
    BindFunction& bf = BindedFunctions[ bind_id ];
    if( !bf.IsScriptCall || !bf.ScriptFunc )
        return 0;

    // Get func num
    hash func_num = GetFuncNum( bf.ScriptFunc );

    // Duplicate checking
    auto it = ScriptFuncBinds.find( func_num );
    if( it != ScriptFuncBinds.end() && it->second != bind_id )
        return 0;

    // Store
    if( it == ScriptFuncBinds.end() )
        ScriptFuncBinds.insert( PAIR( func_num, bind_id ) );

    return func_num;
}

hash Script::BindScriptFuncNumByFunc( asIScriptFunction* func )
{
    #ifdef SCRIPT_MULTITHREADING
    SCOPE_LOCK( BindedFunctionsLocker );
    #endif

    // Bind function
    int bind_id = Script::BindByFunc( func, false );
    if( bind_id <= 0 )
        return 0;

    // Get func num
    hash func_num = GetFuncNum( func );

    // Duplicate checking
    auto it = ScriptFuncBinds.find( func_num );
    if( it != ScriptFuncBinds.end() && it->second != bind_id )
        return 0;

    // Store
    if( it == ScriptFuncBinds.end() )
        ScriptFuncBinds.insert( PAIR( func_num, bind_id ) );

    return func_num;
}

uint Script::GetScriptFuncBindId( hash func_num )
{
    #ifdef SCRIPT_MULTITHREADING
    SCOPE_LOCK( BindedFunctionsLocker );
    #endif

    if( !func_num )
        return 0;

    // Indexing by hash
    auto it = ScriptFuncBinds.find( func_num );
    if( it != ScriptFuncBinds.end() )
        return it->second;

    // Function not binded, try find and bind it
    asIScriptFunction* func = FindFunc( func_num );
    if( !func )
        return 0;

    // Bind
    func_num = Script::BindScriptFuncNumByFunc( func );
    if( func_num )
        return ScriptFuncBinds[ func_num ];

    return 0;
}

bool Script::PrepareScriptFuncContext( hash func_num, const char* call_func, const char* ctx_info )
{
    uint bind_id = GetScriptFuncBindId( func_num );
    if( !bind_id )
    {
        WriteLogF( _FUNC_, " - Function %u '%s' not found. Context info '%s', call func '%s'.\n", func_num, Str::GetName( func_num ), ctx_info, call_func );
        return false;
    }
    return PrepareContext( bind_id, call_func, ctx_info );
}

void Script::CacheEnumValues()
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    auto&       cached_enums = edata->CachedEnums;
    auto&       cached_enum_names = edata->CachedEnumNames;
    char        buf[ MAX_FOTEXT ];

    // Enums
    cached_enums.clear();
    cached_enum_names.clear();
    for( asUINT i = 0, j = Engine->GetEnumCount(); i < j; i++ )
    {
        int         enum_type_id;
        const char* enum_ns;
        const char* enum_name = Engine->GetEnumByIndex( i, &enum_type_id, &enum_ns );
        Str::Format( buf, "%s%s%s", enum_ns, enum_ns[ 0 ] ? "::" : "", enum_name );
        IntStrMap&  value_names = cached_enum_names.insert( PAIR( string( buf ), IntStrMap() ) ).first->second;
        for( asUINT k = 0, l = Engine->GetEnumValueCount( enum_type_id ); k < l; k++ )
        {
            int         value;
            const char* value_name = Engine->GetEnumValueByIndex( enum_type_id, k, &value );
            Str::Format( buf, "%s%s%s::%s", enum_ns, enum_ns[ 0 ] ? "::" : "", enum_name, value_name );
            cached_enums.insert( PAIR( string( buf ), value ) );
            value_names.insert( PAIR( value, string( value_name ) ) );
        }
    }

    // Content
    for( asUINT i = 0, j = Engine->GetGlobalPropertyCount(); i < j; i++ )
    {
        const char* name;
        const char* ns;
        int         type_id;
        hash*       value;
        Engine->GetGlobalPropertyByIndex( i, &name, &ns, &type_id, nullptr, nullptr, (void**) &value );
        if( ns[ 0 ] && Str::CompareCount( ns, "Content", 7 ) )
        {
            RUNTIME_ASSERT( type_id == asTYPEID_UINT32 ); // hash
            Str::Format( buf, "%s::%s", ns, name );
            cached_enums.insert( PAIR( string( buf ), int(value) ) );
        }
    }
}

int Script::GetEnumValue( const char* enum_value_name, bool& fail )
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    const auto& cached_enums = edata->CachedEnums;
    string      key = enum_value_name;
    auto        it = cached_enums.find( key );
    if( it == cached_enums.end() )
    {
        WriteLog( "Enum value '%s' not found.\n", enum_value_name );
        fail = true;
        return 0;
    }
    return it->second;
}

int Script::GetEnumValue( const char* enum_name, const char* value_name, bool& fail )
{
    if( Str::IsNumber( value_name ) )
        return Str::AtoI( value_name );

    char buf[ MAX_FOTEXT ];
    Str::Copy( buf, enum_name );
    Str::Append( buf, "::" );
    Str::Append( buf, value_name );
    return GetEnumValue( buf, fail );
}

const char* Script::GetEnumValueName( const char* enum_name, int value )
{
    EngineData* edata = (EngineData*) Engine->GetUserData();
    const auto& cached_enum_names = edata->CachedEnumNames;
    auto        it = cached_enum_names.find( enum_name );
    RUNTIME_ASSERT( it != cached_enum_names.end() );
    auto        it_value = it->second.find( value );
    return it_value != it->second.end() ? it_value->second.c_str() : Str::ItoA( value );
}

/************************************************************************/
/* Contexts                                                             */
/************************************************************************/

static THREAD bool              ScriptCall = false;
static THREAD asIScriptContext* CurrentCtx = nullptr;
static THREAD size_t            NativeFuncAddr = 0;
static THREAD size_t            NativeArgs[ 256 ] = { 0 };
static THREAD size_t            RetValue[ 2 ] = { 0 }; // EAX:EDX
static THREAD size_t            CurrentArg = 0;
#ifdef SCRIPT_MULTITHREADING
static THREAD int               ExecutionRecursionCounter = 0;
#endif

#ifdef SCRIPT_MULTITHREADING
static size_t     SynchronizeThreadId = 0;
static int        SynchronizeThreadCounter = 0;
static MutexEvent SynchronizeThreadLocker;
static Mutex      SynchronizeThreadLocalLocker;

typedef vector< EndExecutionCallback > EndExecutionCallbackVec;
static THREAD EndExecutionCallbackVec* EndExecutionCallbacks;
#endif

void Script::BeginExecution()
{
    #ifdef SCRIPT_MULTITHREADING
    if( !LogicMT )
        return;

    if( !ExecutionRecursionCounter )
    {
        SyncManager* sync_mngr = SyncManager::GetForCurThread();
        if( !ConcurrentExecution )
        {
            sync_mngr->Suspend();
            ConcurrentExecutionLocker.Lock();
            sync_mngr->PushPriority( 5 );
            sync_mngr->Resume();
        }
        else
        {
            sync_mngr->PushPriority( 5 );
        }
    }
    ExecutionRecursionCounter++;
    #endif
}

void Script::EndExecution()
{
    #ifdef SCRIPT_MULTITHREADING
    if( !LogicMT )
        return;

    ExecutionRecursionCounter--;
    if( !ExecutionRecursionCounter )
    {
        if( !ConcurrentExecution )
        {
            ConcurrentExecutionLocker.Unlock();

            SyncManager* sync_mngr = SyncManager::GetForCurThread();
            sync_mngr->PopPriority();
        }
        else
        {
            SyncManager* sync_mngr = SyncManager::GetForCurThread();
            sync_mngr->PopPriority();

            SynchronizeThreadLocalLocker.Lock();
            bool sync_not_closed = ( SynchronizeThreadId == Thread::GetCurrentId() );
            if( sync_not_closed )
            {
                SynchronizeThreadId = 0;
                SynchronizeThreadCounter = 0;
                SynchronizeThreadLocker.Allow();                 // Unlock synchronization section
            }
            SynchronizeThreadLocalLocker.Unlock();

            if( sync_not_closed )
            {
                WriteLogF( _FUNC_, " - Synchronization section is not closed in script!\n" );
                sync_mngr->PopPriority();
            }
        }

        if( EndExecutionCallbacks && !EndExecutionCallbacks->empty() )
        {
            for( auto it = EndExecutionCallbacks->begin(), end = EndExecutionCallbacks->end(); it != end; ++it )
            {
                EndExecutionCallback func = *it;
                (func) ( );
            }
            EndExecutionCallbacks->clear();
        }
    }
    #endif
}

void Script::AddEndExecutionCallback( EndExecutionCallback func )
{
    #ifdef SCRIPT_MULTITHREADING
    if( !LogicMT )
        return;

    if( !EndExecutionCallbacks )
        EndExecutionCallbacks = new EndExecutionCallbackVec();
    auto it = std::find( EndExecutionCallbacks->begin(), EndExecutionCallbacks->end(), func );
    if( it == EndExecutionCallbacks->end() )
        EndExecutionCallbacks->push_back( func );
    #endif
}

bool Script::PrepareContext( uint bind_id, const char* call_func, const char* ctx_info )
{
    #ifdef SCRIPT_MULTITHREADING
    if( LogicMT )
        BindedFunctionsLocker.Lock();
    #endif

    if( !bind_id || bind_id >= (uint) BindedFunctions.size() )
    {
        WriteLogF( _FUNC_, " - Invalid bind id '%u'. Context info '%s'.\n", bind_id, ctx_info );
        #ifdef SCRIPT_MULTITHREADING
        if( LogicMT )
            BindedFunctionsLocker.Unlock();
        #endif
        return false;
    }

    BindFunction&      bf = BindedFunctions[ bind_id ];
    bool               is_script = bf.IsScriptCall;
    asIScriptFunction* script_func = bf.ScriptFunc;
    size_t             func_addr = bf.NativeFuncAddr;

    #ifdef SCRIPT_MULTITHREADING
    if( LogicMT )
        BindedFunctionsLocker.Unlock();
    #endif

    if( is_script )
    {
        if( !script_func )
            return false;

        asIScriptContext* ctx = RequestContext();
        RUNTIME_ASSERT( ctx );

        BeginExecution();

        ContextData* ctx_data = (ContextData*) ctx->GetUserData();
        Str::Format( ctx_data->Info, "Caller '%s', CallFunc '%s', Thread '%s'", ctx_info, call_func, Thread::GetCurrentName() );

        int result = ctx->Prepare( script_func );
        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Prepare error, context name '%s', bind_id '%u', func '%s', error '%d'.\n", ctx_data->Info, bind_id, script_func->GetDeclaration(), result );
            ctx->Abort();
            ReturnContext( ctx );
            EndExecution();
            return false;
        }

        RUNTIME_ASSERT( !CurrentCtx );
        CurrentCtx = ctx;
        ScriptCall = true;
    }
    else
    {
        BeginExecution();

        NativeFuncAddr = func_addr;
        ScriptCall = false;
    }

    CurrentArg = 0;
    return true;
}

void Script::SetArgUChar( uchar value )
{
    if( ScriptCall )
        CurrentCtx->SetArgByte( (asUINT) CurrentArg, value );
    else
        NativeArgs[ CurrentArg ] = value;
    CurrentArg++;
}

void Script::SetArgUShort( ushort value )
{
    if( ScriptCall )
        CurrentCtx->SetArgWord( (asUINT) CurrentArg, value );
    else
        NativeArgs[ CurrentArg ] = value;
    CurrentArg++;
}

void Script::SetArgUInt( uint value )
{
    if( ScriptCall )
        CurrentCtx->SetArgDWord( (asUINT) CurrentArg, value );
    else
        NativeArgs[ CurrentArg ] = value;
    CurrentArg++;
}

void Script::SetArgUInt64( uint64 value )
{
    if( ScriptCall )
        CurrentCtx->SetArgQWord( (asUINT) CurrentArg, value );
    else
    {
        *( (uint64*) &NativeArgs[ CurrentArg ] ) = value;
        CurrentArg++;
    }
    CurrentArg++;
}

void Script::SetArgBool( bool value )
{
    if( ScriptCall )
        CurrentCtx->SetArgByte( (asUINT) CurrentArg, value );
    else
        NativeArgs[ CurrentArg ] = value;
    CurrentArg++;
}

void Script::SetArgFloat( float value )
{
    if( ScriptCall )
        CurrentCtx->SetArgFloat( (asUINT) CurrentArg, value );
    else
        *( (float*) &NativeArgs[ CurrentArg ] ) = value;
    CurrentArg++;
}

void Script::SetArgDouble( double value )
{
    if( ScriptCall )
        CurrentCtx->SetArgDouble( (asUINT) CurrentArg, value );
    else
        *( (double*) &NativeArgs[ CurrentArg++ ] ) = value;
    CurrentArg++;
}

void Script::SetArgObject( void* value )
{
    if( ScriptCall )
        CurrentCtx->SetArgObject( (asUINT) CurrentArg, value );
    else
        NativeArgs[ CurrentArg ] = (size_t) value;
    CurrentArg++;
}

void Script::SetArgEntity( Entity* value )
{
    RUNTIME_ASSERT( !value || !value->IsDestroyed );

    if( ScriptCall )
    {
        if( value )
        {
            ContextData* ctx_data = (ContextData*) CurrentCtx->GetUserData();
            ctx_data->EntitiesArgs[ ctx_data->EntitiesArgsCount++ ] = value;
        }
        CurrentCtx->SetArgObject( (asUINT) CurrentArg, value );
    }
    else
    {
        NativeArgs[ CurrentArg ] = (size_t) value;
    }
    CurrentArg++;
}

void Script::SetArgEntityOK( Entity* value )
{
    RUNTIME_ASSERT( !value || !value->IsDestroyed );

    if( ScriptCall )
    {
        if( value )
        {
            ContextData* ctx_data = (ContextData*) CurrentCtx->GetUserData();
            ctx_data->EntitiesArgs[ ctx_data->EntitiesArgsCount++ ] = value;
            value->AddRef();
        }
        CurrentCtx->SetArgObject( (asUINT) CurrentArg, value );
    }
    else
    {
        NativeArgs[ CurrentArg ] = (size_t) value;
    }
    CurrentArg++;
}

void Script::SetArgAddress( void* value )
{
    if( ScriptCall )
        CurrentCtx->SetArgAddress( (asUINT) CurrentArg, value );
    else
        NativeArgs[ CurrentArg ] = (size_t) value;
    CurrentArg++;
}

// Taked from AS sources
#if defined ( FO_MSVC )
static uint64 CallCDeclFunction32( const size_t* args, size_t paramSize, size_t func )
#else
static uint64 __attribute( ( __noinline__ ) ) CallCDeclFunction32( const size_t * args, size_t paramSize, size_t func )
#endif
{
    volatile uint64 retQW = 0;

    #if defined ( FO_MSVC ) && defined ( FO_X86 )
    // Copy the data to the real stack. If we fail to do
    // this we may run into trouble in case of exceptions.
    __asm
    {
        // We must save registers that are used
        push ecx

        // Clear the FPU stack, in case the called function doesn't do it by itself
        fninit

        // Copy arguments from script
        // stack to application stack
        mov ecx, paramSize
        mov  eax, args
        add  eax, ecx
        cmp  ecx, 0
        je   endcopy
copyloop:
        sub  eax, 4
        push dword ptr[ eax ]
        sub  ecx, 4
        jne  copyloop
endcopy:

        // Call function
        call[ func ]

        // Pop arguments from stack
        add  esp, paramSize

        // Copy return value from EAX:EDX
        lea  ecx, retQW
            mov[ ecx ], eax
            mov  4[ ecx ], edx

        // Restore registers
        pop  ecx
    }

    #elif defined ( FO_GCC ) && !defined ( FO_OSX_IOS )
    // It is not possible to rely on ESP or BSP to refer to variables or arguments on the stack
    // depending on compiler settings BSP may not even be used, and the ESP is not always on the
    // same offset from the local variables. Because the code adjusts the ESP register it is not
    // possible to inform the arguments through symbolic names below.

    // It's not also not possible to rely on the memory layout of the function arguments, because
    // on some compiler versions and settings the arguments may be copied to local variables with a
    // different ordering before they are accessed by the rest of the code.

    // I'm copying the arguments into this array where I know the exact memory layout. The address
    // of this array will then be passed to the inline asm in the EDX register.
    volatile size_t a[] = { size_t( args ), size_t( paramSize ), size_t( func ) };

    asm __volatile__ (
        "fninit                 \n"
        "pushl %%ebx            \n"
        "movl  %%edx, %%ebx     \n"

        // Need to align the stack pointer so that it is aligned to 16 bytes when making the function call.
        // It is assumed that when entering this function, the stack pointer is already aligned, so we need
        // to calculate how much we will put on the stack during this call.
        "movl  4(%%ebx), %%eax  \n"         // paramSize
        "addl  $4, %%eax        \n"         // counting esp that we will push on the stack
        "movl  %%esp, %%ecx     \n"
        "subl  %%eax, %%ecx     \n"
        "andl  $15, %%ecx       \n"
        "movl  %%esp, %%eax     \n"
        "subl  %%ecx, %%esp     \n"
        "pushl %%eax            \n"         // Store the original stack pointer

        // Copy all arguments to the stack and call the function
        "movl  4(%%ebx), %%ecx  \n"         // paramSize
        "movl  0(%%ebx), %%eax  \n"         // args
        "addl  %%ecx, %%eax     \n"         // push arguments on the stack
        "cmp   $0, %%ecx        \n"
        "je    endcopy          \n"
        "copyloop:              \n"
        "subl  $4, %%eax        \n"
        "pushl (%%eax)          \n"
        "subl  $4, %%ecx        \n"
        "jne   copyloop         \n"
        "endcopy:               \n"
        "call  *8(%%ebx)        \n"
        "addl  4(%%ebx), %%esp  \n"         // pop arguments

        // Pop the alignment bytes
        "popl  %%esp            \n"
        "popl  %%ebx            \n"

        // Copy EAX:EDX to retQW. As the stack pointer has been
        // restored it is now safe to access the local variable
        "leal  %1, %%ecx        \n"
        "movl  %%eax, 0(%%ecx)  \n"
        "movl  %%edx, 4(%%ecx)  \n"
        :                                   // output
        : "d" ( a ), "m" ( retQW )          // input - pass pointer of args in edx, pass pointer of retQW in memory argument
        : "%eax", "%ecx"                    // clobber
        );
    #else
    UNUSED_VARIABLE( retQW );
    #endif

    return retQW;
}

bool Script::RunPrepared()
{
    if( ScriptCall )
    {
        RUNTIME_ASSERT( CurrentCtx );
        asIScriptContext* ctx = CurrentCtx;
        ContextData*      ctx_data = (ContextData*) ctx->GetUserData();
        uint              tick = Timer::FastTick();
        CurrentCtx = nullptr;
        ctx_data->StartTick = tick;
        ctx_data->Parent = asGetActiveContext();

        int             result = ctx->Execute();

        uint            delta = Timer::FastTick() - tick;

        asEContextState state = ctx->GetState();
        if( state == asEXECUTION_SUSPENDED )
        {
            *(uint64*) RetValue = 0;
            EndExecution();
            return true;
        }
        else if( state != asEXECUTION_FINISHED )
        {
            if( state != asEXECUTION_EXCEPTION )
            {
                if( state == asEXECUTION_ABORTED )
                    HandleException( ctx, "Execution of script stopped due to timeout %u (ms).", RunTimeoutAbort );
                else
                    HandleException( ctx, "Execution of script stopped due to %s.", ContextStatesStr[ (int) state ] );
            }
            ctx->Abort();
            ReturnContext( ctx );
            EndExecution();
            return false;
        }
        else if( RunTimeoutMessage && delta >= RunTimeoutMessage )
        {
            WriteLog( "Script work time %u in context '%s'.\n", delta, ctx_data->Info );
        }

        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Context '%s' execute error %d, state '%s'.\n", ctx_data->Info, result, ContextStatesStr[ (int) state ] );
            ctx->Abort();
            ReturnContext( ctx );
            EndExecution();
            return false;
        }

        *(uint64*) RetValue = *(uint64*) ctx->GetAddressOfReturnValue();
        ReturnContext( ctx );
    }
    else
    {
        *(uint64*) RetValue = CallCDeclFunction32( NativeArgs, CurrentArg * 4, NativeFuncAddr );
    }

    EndExecution();
    return true;
}

void Script::RunPreparedSuspend()
{
    RUNTIME_ASSERT( ScriptCall );
    RUNTIME_ASSERT( CurrentCtx );

    asIScriptContext* ctx = CurrentCtx;
    ContextData*      ctx_data = (ContextData*) ctx->GetUserData();
    uint              tick = Timer::FastTick();
    CurrentCtx = nullptr;
    ctx_data->StartTick = tick;
    ctx_data->Parent = asGetActiveContext();
    *(uint64*) RetValue = 0;
    EndExecution();
}

asIScriptContext* Script::SuspendCurrentContext( uint time )
{
    asIScriptContext* ctx = asGetActiveContext();
    ContextsLocker.Lock();
    RUNTIME_ASSERT( std::find( BusyContexts.begin(), BusyContexts.end(), ctx ) != BusyContexts.end() );
    ContextsLocker.Unlock();
    if( ctx->GetFunction( ctx->GetCallstackSize() - 1 )->GetReturnTypeId() != asTYPEID_VOID )
        SCRIPT_ERROR_R0( "Can't yield context which must return value." );
    ctx->Suspend();
    ContextData* ctx_data = (ContextData*) ctx->GetUserData();
    ctx_data->SuspendEndTick = ( time != uint( -1 ) ? ( time ? Timer::FastTick() + time : 0 ) : uint( -1 ) );
    return ctx;
}

void Script::ResumeContext( asIScriptContext* ctx )
{
    RUNTIME_ASSERT( ctx->GetState() == asEXECUTION_SUSPENDED );
    ContextData* ctx_data = (ContextData*) ctx->GetUserData();
    RUNTIME_ASSERT( ctx_data->SuspendEndTick == uint( -1 ) );
    ctx_data->SuspendEndTick = Timer::FastTick();
}

void Script::RunSuspended()
{
    // Collect contexts to resume
    ContextVec resume_contexts;
    {
        SCOPE_LOCK( ContextsLocker );

        if( BusyContexts.empty() )
            return;

        uint tick = Timer::FastTick();
        for( auto it = BusyContexts.begin(), end = BusyContexts.end(); it != end; ++it )
        {
            asIScriptContext* ctx = *it;
            ContextData*      ctx_data = (ContextData*) ctx->GetUserData();
            if( ( ctx->GetState() == asEXECUTION_PREPARED || ctx->GetState() == asEXECUTION_SUSPENDED ) &&
                ctx_data->SuspendEndTick != uint( -1 ) && tick >= ctx_data->SuspendEndTick )
            {
                resume_contexts.push_back( ctx );
            }
        }

        if( resume_contexts.empty() )
            return;
    }

    // Resume
    for( auto& ctx : resume_contexts )
    {
        if( ctx->GetState() == asEXECUTION_PREPARED && !CheckContextEntities( ctx ) )
        {
            ReturnContext( ctx );
            continue;
        }

        BeginExecution();
        CurrentCtx = ctx;
        ScriptCall = true;
        RunPrepared();
    }
}

void Script::RunMandatorySuspended()
{
    uint i = 0;
    while( true )
    {
        // Collect contexts to resume
        ContextVec resume_contexts;
        {
            SCOPE_LOCK( ContextsLocker );

            if( BusyContexts.empty() )
                break;

            for( auto it = BusyContexts.begin(), end = BusyContexts.end(); it != end; ++it )
            {
                asIScriptContext* ctx = *it;
                ContextData*      ctx_data = (ContextData*) ctx->GetUserData();
                if( ( ctx->GetState() == asEXECUTION_PREPARED || ctx->GetState() == asEXECUTION_SUSPENDED ) && ctx_data->SuspendEndTick == 0 )
                    resume_contexts.push_back( ctx );
            }

            if( resume_contexts.empty() )
                break;
        }

        // Resume
        for( auto& ctx : resume_contexts )
        {
            if( ctx->GetState() == asEXECUTION_PREPARED && !CheckContextEntities( ctx ) )
            {
                ReturnContext( ctx );
                continue;
            }

            BeginExecution();
            CurrentCtx = ctx;
            ScriptCall = true;
            RunPrepared();
        }

        // Detect recursion
        if( ++i % 10000 == 0 )
            WriteLog( "Big loops in suspended contexts.\n" );
    }
}

bool Script::CheckContextEntities( asIScriptContext* ctx )
{
    ContextData* ctx_data = (ContextData*) ctx->GetUserData();
    for( uint i = 0; i < ctx_data->EntitiesArgsCount; i++ )
        if( ctx_data->EntitiesArgs[ i ]->IsDestroyed )
            return false;
    return true;
}

uint Script::GetReturnedUInt()
{
    return *(uint*) RetValue;
}

bool Script::GetReturnedBool()
{
    return *(bool*) RetValue;
}

void* Script::GetReturnedObject()
{
    return *(void**) RetValue;
}

float Script::GetReturnedFloat()
{
    if( ScriptCall )
    {
        return *(float*) RetValue;
    }
    else
    {
        float            f;
        #if defined ( FO_MSVC ) && defined ( FO_X86 )
        __asm fstp dword ptr[ f ]
        #elif defined ( FO_GCC ) && !defined ( FO_OSX_IOS )
        asm ( "fstps %0 \n" : "=m" ( f ) );
        #else
        f = 0.0f;
        #endif
        return f;
    }
}

double Script::GetReturnedDouble()
{
    if( ScriptCall )
    {
        return *(double*) RetValue;
    }
    else
    {
        double           d;
        #if defined ( FO_MSVC ) && defined ( FO_X86 )
        __asm fstp qword ptr[ d ]
        #elif defined ( FO_GCC ) && !defined ( FO_OSX_IOS )
        asm ( "fstpl %0 \n" : "=m" ( d ) );
        #else
        d = 0.0;
        #endif
        return d;
    }
}

void* Script::GetReturnedRawAddress()
{
    return (void*) RetValue;
}

bool Script::SynchronizeThread()
{
    #ifdef SCRIPT_MULTITHREADING
    if( !ConcurrentExecution )
        return true;

    SynchronizeThreadLocalLocker.Lock();     // Local lock

    if( !SynchronizeThreadId )               // Section is free
    {
        SynchronizeThreadId = Thread::GetCurrentId();
        SynchronizeThreadCounter = 1;
        SynchronizeThreadLocker.Disallow();         // Lock synchronization section
        SynchronizeThreadLocalLocker.Unlock();      // Local unlock

        SyncManager* sync_mngr = SyncManager::GetForCurThread();
        sync_mngr->PushPriority( 10 );
    }
    else if( SynchronizeThreadId == Thread::GetCurrentId() ) // Section busy by current thread
    {
        SynchronizeThreadCounter++;
        SynchronizeThreadLocalLocker.Unlock();               // Local unlock
    }
    else                                                     // Section busy by another thread
    {
        SynchronizeThreadLocalLocker.Unlock();               // Local unlock

        SyncManager* sync_mngr = SyncManager::GetForCurThread();
        sync_mngr->Suspend();                                // Allow other threads take objects
        SynchronizeThreadLocker.Wait();                      // Sleep until synchronization section locked
        sync_mngr->Resume();                                 // Relock busy objects
        return SynchronizeThread();                          // Try enter again
    }
    #endif
    return true;
}

bool Script::ResynchronizeThread()
{
    #ifdef SCRIPT_MULTITHREADING
    if( !ConcurrentExecution )
        return true;

    SynchronizeThreadLocalLocker.Lock();     // Local lock

    if( SynchronizeThreadId == Thread::GetCurrentId() )
    {
        if( --SynchronizeThreadCounter == 0 )
        {
            SynchronizeThreadId = 0;
            SynchronizeThreadLocker.Allow();             // Unlock synchronization section
            SynchronizeThreadLocalLocker.Unlock();       // Local unlock

            SyncManager* sync_mngr = SyncManager::GetForCurThread();
            sync_mngr->PopPriority();
        }
        else
        {
            SynchronizeThreadLocalLocker.Unlock();             // Local unlock
        }
    }
    else
    {
        // Invalid call
        SynchronizeThreadLocalLocker.Unlock();         // Local unlock
        return false;
    }
    #endif
    return true;
}

/************************************************************************/
/* Logging                                                              */
/************************************************************************/

void Script::Log( const char* str )
{
    asIScriptContext* ctx = asGetActiveContext();
    if( !ctx )
    {
        WriteLog( "<unknown> : %s.\n", str );
        return;
    }
    asIScriptFunction* func = ctx->GetFunction( 0 );
    if( !func )
    {
        WriteLog( "<unknown> : %s.\n", str );
        return;
    }
    if( LogDebugInfo )
    {
        int                                 line = ctx->GetLineNumber( 0 );
        Preprocessor::LineNumberTranslator* lnt = (Preprocessor::LineNumberTranslator*) func->GetModule()->GetUserData();
        ContextData*                        ctx_data = (ContextData*) ctx->GetUserData();
        WriteLog( "Script callback: %s : %s : %s : Line %d : %s.\n", str, Preprocessor::ResolveOriginalFile( line, lnt ), func->GetDeclaration( true ), Preprocessor::ResolveOriginalLine( line, lnt ), ctx_data->Info );
    }
    else
    {
        int                                 line = ctx->GetLineNumber( 0 );
        Preprocessor::LineNumberTranslator* lnt = (Preprocessor::LineNumberTranslator*) func->GetModule()->GetUserData();
        WriteLog( "%s : %s\n", Preprocessor::ResolveOriginalFile( line, lnt ), str );
    }
}

void Script::SetLogDebugInfo( bool enabled )
{
    LogDebugInfo = enabled;
}

void Script::CallbackMessage( const asSMessageInfo* msg, void* param )
{
    const char* type = "Error";
    if( msg->type == asMSGTYPE_WARNING )
        type = "Warning";
    else if( msg->type == asMSGTYPE_INFORMATION )
        type = "Info";
    WriteLog( "%s : %s : %s : Line %d.\n", Preprocessor::ResolveOriginalFile( msg->row ), type, msg->message, Preprocessor::ResolveOriginalLine( msg->row ) );
}

void Script::CallbackException( asIScriptContext* ctx, void* param )
{
    const char* str = ctx->GetExceptionString();
    uint        str_len = Str::Length( str );
    if( !Str::Compare( str, "Pass" ) )
        HandleException( ctx, "Script exception: %s%s", str, str_len > 0 && str[ str_len - 1 ] != '.' ? "." : "" );
}

/************************************************************************/
/* Array                                                                */
/************************************************************************/

ScriptArray* Script::CreateArray( const char* type )
{
    return ScriptArray::Create( Engine->GetObjectTypeById( Engine->GetTypeIdByDecl( type ) ), 0, nullptr );
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
