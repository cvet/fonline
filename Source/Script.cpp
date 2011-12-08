#include "StdAfx.h"
#include "Script.h"
#include "Text.h"
#include "FileManager.h"
#include "Version.h"
#include "AngelScript/scriptstring.h"
#include "AngelScript/scriptany.h"
#include "AngelScript/scriptdictionary.h"
#include "AngelScript/scriptfile.h"
#include "AngelScript/scriptmath.h"
#include "AngelScript/scriptmath3d.h"
#include "AngelScript/scriptarray.h"
#include "AngelScript/Preprocessor/preprocess.h"
#include <strstream>

namespace Script
{

    const char* ContextStatesStr[] =
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
        bool   IsScriptCall;
        int    ScriptFuncId;
        string ModuleName;
        string FuncName;
        string FuncDecl;
        size_t NativeFuncAddr;

        BindFunction( int func_id, size_t native_func_addr, const char* module_name,  const char* func_name, const char* func_decl )
        {
            IsScriptCall = ( native_func_addr == 0 );
            ScriptFuncId = func_id;
            NativeFuncAddr = native_func_addr;
            ModuleName = module_name;
            FuncName = func_name;
            FuncDecl = func_decl;
        }
    };
    typedef vector< BindFunction > BindFunctionVec;

    asIScriptEngine* Engine = NULL;
    void*            EngineLogFile = NULL;
    int              ScriptsPath = PT_SCRIPTS;
    bool             LogDebugInfo = true;
    StrVec           WrongGlobalObjects;

    #ifdef FONLINE_SERVER
    uint   GarbagerCycle = 120000;
    uint   EvaluationCycle = 120000;
    double MaxGarbagerTime = 40.0;
    #else
    uint   GarbageCollectTime = 120000;
    #endif

    #ifdef SCRIPT_MULTITHREADING
    MutexCode GarbageLocker;
    #endif

    BindFunctionVec BindedFunctions;
    #ifdef SCRIPT_MULTITHREADING
    Mutex           BindedFunctionsLocker;
    #endif
    StrVec          ScriptFuncCache;
    IntVec          ScriptFuncBindId;

    bool            ConcurrentExecution = false;
    #ifdef SCRIPT_MULTITHREADING
    Mutex           ConcurrentExecutionLocker;
    #endif

    bool LoadLibraryCompiler = false;

// Contexts
    THREAD asIScriptContext* GlobalCtx[ GLOBAL_CONTEXT_STACK_SIZE ] = { 0 };
    THREAD uint              GlobalCtxIndex = 0;
    class ActiveContext
    {
public:
        asIScriptContext** Contexts;
        uint               StartTick;
        ActiveContext( asIScriptContext** ctx, uint tick ): Contexts( ctx ), StartTick( tick ) {}
        bool operator==( asIScriptContext** ctx ) { return ctx == Contexts; }
    };
    typedef vector< ActiveContext > ActiveContextVec;
    ActiveContextVec ActiveContexts;
    Mutex            ActiveGlobalCtxLocker;

// Timeouts
    uint   RunTimeoutSuspend = 600000; // 10 minutes
    uint   RunTimeoutMessage = 300000; // 5 minutes
    Thread RunTimeoutThread;
    void RunTimeout( void* );


    bool Init( bool with_log, Preprocessor::PragmaCallback* pragma_callback )
    {
        if( with_log && !StartLog() )
        {
            WriteLogF( _FUNC_, " - Log creation error.\n" );
            return false;
        }

        // Create default engine
        Engine = CreateEngine( pragma_callback );
        if( !Engine )
        {
            WriteLogF( _FUNC_, " - Can't create AS engine.\n" );
            return false;
        }

        BindedFunctions.clear();
        BindedFunctions.reserve( 10000 );
        BindedFunctions.push_back( BindFunction( 0, 0, "", "", "" ) ); // None
        BindedFunctions.push_back( BindFunction( 0, 0, "", "", "" ) ); // Temp

        if( !InitThread() )
            return false;

        RunTimeoutThread.Start( RunTimeout, "ScriptTimeout" );
        return true;
    }

    void Finish()
    {
        if( !Engine )
            return;

        EndLog();
        RunTimeoutSuspend = 0;
        RunTimeoutMessage = 0;
        RunTimeoutThread.Wait();

        BindedFunctions.clear();
        Preprocessor::SetPragmaCallback( NULL );
        Preprocessor::UndefAll();
        UnloadScripts();

        FinishEngine( Engine ); // Finish default engine

        #pragma MESSAGE("Client crashed here, disable finishing until fix angelscript.")
        #ifndef FONLINE_CLIENT
        FinishThread();
        #endif
    }

    bool InitThread()
    {
        for( int i = 0; i < GLOBAL_CONTEXT_STACK_SIZE; i++ )
        {
            GlobalCtx[ i ] = CreateContext();
            if( !GlobalCtx[ i ] )
            {
                WriteLogF( _FUNC_, " - Create global contexts fail.\n" );
                Engine->Release();
                Engine = NULL;
                return false;
            }
        }

        return true;
    }

    void FinishThread()
    {
        ActiveGlobalCtxLocker.Lock();
        auto it = std::find( ActiveContexts.begin(), ActiveContexts.end(), (asIScriptContext**) GlobalCtx );
        if( it != ActiveContexts.end() )
            ActiveContexts.erase( it );
        ActiveGlobalCtxLocker.Unlock();

        for( int i = 0; i < GLOBAL_CONTEXT_STACK_SIZE; i++ )
            FinishContext( GlobalCtx[ i ] );

        asThreadCleanup();
    }

    void* LoadDynamicLibrary( const char* dll_name )
    {
        // Find in already loaded
        char        dll_name_lower[ MAX_FOPATH ];
        Str::Copy( dll_name_lower, dll_name );
        EngineData* edata = (EngineData*) Engine->GetUserData();
        auto        it = edata->LoadedDlls.find( dll_name_lower );
        if( it != edata->LoadedDlls.end() )
            return ( *it ).second.second;

        // Make path
        char dll_path[ MAX_FOPATH ];
        Str::Copy( dll_path, dll_name_lower );
        FileManager::EraseExtension( dll_path );

        #if defined ( FO_X64 )
        // Add '64' appendix
        Str::Append( dll_path, "64" );
        #endif

        // DLL extension
        #if defined ( FO_WINDOWS )
        Str::Append( dll_path, ".dll" );
        #else // FO_LINUX
        Str::Append( dll_path, ".so" );
        #endif

        // Client path fixes
        #if defined ( FONLINE_CLIENT )
        Str::Insert( dll_path, FileManager::GetPath( PT_SERVER_SCRIPTS ) );
        Str::Replacement( dll_path, '\\', '.' );
        Str::Replacement( dll_path, '/', '.' );
        #endif

        // Insert base path
        Str::Insert( dll_path, FileManager::GetFullPath( "", ScriptsPath ) );

        // Load dynamic library
        void* dll = DLL_Load( dll_path );
        if( !dll )
            return NULL;

        // Register variables
        size_t* ptr = DLL_GetAddress( dll, "FOnline" );
        if( ptr )
            *ptr = (size_t) &GameOpt;
        ptr = DLL_GetAddress( dll, "ASEngine" );
        if( ptr )
            *ptr = (size_t) Engine;

        // Register functions
        ptr = DLL_GetAddress( dll, "Log" );
        if( ptr )
            *ptr = (size_t) &WriteLog;
        ptr = DLL_GetAddress( dll, "Malloc" );
        if( ptr )
            *ptr = (size_t) &malloc;
        ptr = DLL_GetAddress( dll, "Calloc" );
        if( ptr )
            *ptr = (size_t) &calloc;
        ptr = DLL_GetAddress( dll, "Free" );
        if( ptr )
            *ptr = (size_t) &free;

        // Call init function
        typedef void ( *DllMainEx )( bool );
        DllMainEx func = (DllMainEx) DLL_GetAddress( dll, "DllMainEx" );
        if( func )
            ( *func )( LoadLibraryCompiler );

        // Add to collection for current engine
        edata->LoadedDlls.insert( PAIR( string( dll_name_lower ), PAIR( string( dll_path ), dll ) ) );

        return dll;
    }

    void SetWrongGlobalObjects( StrVec& names )
    {
        WrongGlobalObjects = names;
    }

    void SetConcurrentExecution( bool enabled )
    {
        ConcurrentExecution = enabled;
    }

    void SetLoadLibraryCompiler( bool enabled )
    {
        LoadLibraryCompiler = enabled;
    }

    void UnloadScripts()
    {
        EngineData*      edata = (EngineData*) Engine->GetUserData();
        ScriptModuleVec& modules = edata->Modules;

        for( auto it = modules.begin(), end = modules.end(); it != end; ++it )
        {
            asIScriptModule* module = *it;
            int              result = module->ResetGlobalVars();
            if( result < 0 )
                WriteLogF( _FUNC_, " - Reset global vars fail, module<%s>, error<%d>.\n", module->GetName(), result );
            result = module->UnbindAllImportedFunctions();
            if( result < 0 )
                WriteLogF( _FUNC_, " - Unbind fail, module<%s>, error<%d>.\n", module->GetName(), result );
        }

        for( auto it = modules.begin(), end = modules.end(); it != end; ++it )
        {
            asIScriptModule* module = *it;
            Engine->DiscardModule( module->GetName() );
        }

        modules.clear();
        CollectGarbage();
    }

    bool ReloadScripts( const char* config, const char* key, bool skip_binaries, const char* file_pefix /* = NULL */ )
    {
        WriteLog( "Reload scripts...\n" );

        Script::UnloadScripts();

        int        errors = 0;
        char       buf[ 1024 ];
        string     value;

        istrstream config_( config );
        while( !config_.eof() )
        {
            config_.getline( buf, 1024 );
            if( buf[ 0 ] != '@' )
                continue;

            istrstream str( &buf[ 1 ] );
            str >> value;
            if( str.fail() || value != key )
                continue;
            str >> value;
            if( str.fail() || value != "module" )
                continue;

            str >> value;
            if( str.fail() || !LoadScript( value.c_str(), NULL, skip_binaries, file_pefix ) )
            {
                WriteLog( "Load module fail, name<%s>.\n", value.c_str() );
                errors++;
            }
        }

        errors += BindImportedFunctions();
        errors += RebindFunctions();

        if( errors )
        {
            WriteLog( "Reload scripts fail.\n" );
            return false;
        }

        WriteLog( "Reload scripts complete.\n" );
        return true;
    }

    bool BindReservedFunctions( const char* config, const char* key, ReservedScriptFunction* bind_func, uint bind_func_count )
    {
        WriteLog( "Bind reserved functions...\n" );

        int    errors = 0;
        char   buf[ 1024 ];
        string value;
        for( uint i = 0; i < bind_func_count; i++ )
        {
            ReservedScriptFunction* bf = &bind_func[ i ];
            int                     bind_id = 0;

            istrstream              config_( config );
            while( !config_.eof() )
            {
                config_.getline( buf, 1024 );
                if( buf[ 0 ] != '@' )
                    continue;

                istrstream str( &buf[ 1 ] );
                str >> value;
                if( str.fail() || key != value )
                    continue;
                str >> value;
                if( str.fail() || value != "bind" )
                    continue;
                str >> value;
                if( str.fail() || value != bf->FuncName )
                    continue;

                str >> value;
                if( !str.fail() )
                    bind_id = Bind( value.c_str(), bf->FuncName, bf->FuncDecl, false );
                break;
            }

            if( bind_id > 0 )
            {
                *bf->BindId = bind_id;
            }
            else
            {
                WriteLog( "Bind reserved function fail, name<%s>.\n", bf->FuncName );
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

    void AddRef( void* )
    {
        // Dummy
    }

    void Release( void* )
    {
        // Dummy
    }

    asIScriptEngine* GetEngine()
    {
        return Engine;
    }

    void SetEngine( asIScriptEngine* engine )
    {
        Engine = engine;
    }

    asIScriptEngine* CreateEngine( Preprocessor::PragmaCallback* pragma_callback )
    {
        asIScriptEngine* engine = asCreateScriptEngine( ANGELSCRIPT_VERSION );
        if( !engine )
        {
            WriteLogF( _FUNC_, " - asCreateScriptEngine fail.\n" );
            return false;
        }

        engine->SetMessageCallback( asFUNCTION( CallbackMessage ), NULL, asCALL_CDECL );
        RegisterScriptArray( engine, true );
        RegisterScriptString( engine );
        RegisterScriptStringUtils( engine );
        RegisterScriptAny( engine );
        RegisterScriptDictionary( engine );
        RegisterScriptFile( engine );
        RegisterScriptMath( engine );
        RegisterScriptMath3D( engine );

        EngineData* edata = new EngineData();
        edata->PragmaCB = pragma_callback;
        engine->SetUserData( edata );
        return engine;
    }

    void FinishEngine( asIScriptEngine*& engine )
    {
        if( engine )
        {
            EngineData* edata = (EngineData*) engine->SetUserData( NULL );
            delete edata->PragmaCB;
            for( auto it = edata->LoadedDlls.begin(), end = edata->LoadedDlls.end(); it != end; ++it )
                DLL_Free( ( *it ).second.second );
            delete edata;
            engine->Release();
            engine = NULL;
        }
    }

    asIScriptContext* CreateContext()
    {
        asIScriptContext* ctx = Engine->CreateContext();
        if( !ctx )
        {
            WriteLogF( _FUNC_, " - CreateContext fail.\n" );
            return NULL;
        }

        if( ctx->SetExceptionCallback( asFUNCTION( CallbackException ), NULL, asCALL_CDECL ) < 0 )
        {
            WriteLogF( _FUNC_, " - SetExceptionCallback to fail.\n" );
            ctx->Release();
            return NULL;
        }

        char* buf = new char[ CONTEXT_BUFFER_SIZE ];
        if( !buf )
        {
            WriteLogF( _FUNC_, " - Allocate memory for buffer fail.\n" );
            ctx->Release();
            return NULL;
        }

        Str::Copy( buf, CONTEXT_BUFFER_SIZE, "<error>" );
        ctx->SetUserData( buf );
        return ctx;
    }

    void FinishContext( asIScriptContext*& ctx )
    {
        if( ctx )
        {
            char* buf = (char*) ctx->GetUserData();
            if( buf )
                delete[] buf;
            ctx->Release();
            ctx = NULL;
        }
    }

    asIScriptContext* GetGlobalContext()
    {
        if( GlobalCtxIndex >= GLOBAL_CONTEXT_STACK_SIZE )
        {
            WriteLogF( _FUNC_, " - Script context stack overflow! Context call stack:\n" );
            for( int i = GLOBAL_CONTEXT_STACK_SIZE - 1; i >= 0; i-- )
                WriteLog( "  %d) %s.\n", i, GlobalCtx[ i ]->GetUserData() );
            return NULL;
        }
        GlobalCtxIndex++;
        return GlobalCtx[ GlobalCtxIndex - 1 ];
    }

    void PrintContextCallstack( asIScriptContext* ctx )
    {
        int                      line, column;
        const asIScriptFunction* func;
        int                      stack_size = ctx->GetCallstackSize();
        WriteLog( "Context<%s>, state<%s>, call stack<%d>:\n", ctx->GetUserData(), ContextStatesStr[ (int) ctx->GetState() ], stack_size );

        // Print current function
        if( ctx->GetState() == asEXECUTION_EXCEPTION )
        {
            line = ctx->GetExceptionLineNumber( &column );
            func = Engine->GetFunctionById( ctx->GetExceptionFunction() );
        }
        else
        {
            line = ctx->GetLineNumber( 0, &column );
            func = ctx->GetFunction( 0 );
        }
        if( func )
            WriteLog( "  %d) %s : %s : %d, %d.\n", stack_size - 1, func->GetModuleName(), func->GetDeclaration(), line, column );

        // Print call stack
        for( int i = 1; i < stack_size; i++ )
        {
            func = ctx->GetFunction( i );
            line = ctx->GetLineNumber( i, &column );
            if( func )
                WriteLog( "  %d) %s : %s : %d, %d.\n", stack_size - i - 1, func->GetModuleName(), func->GetDeclaration(), line, column );
        }
    }

    const char* GetActiveModuleName()
    {
        static const char error[] = "<error>";
        asIScriptContext* ctx = asGetActiveContext();
        if( !ctx )
            return error;
        asIScriptFunction* func = ctx->GetFunction( 0 );
        if( !func )
            return error;
        return func->GetModuleName();
    }

    const char* GetActiveFuncName()
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

    asIScriptModule* GetModule( const char* name )
    {
        return Engine->GetModule( name, asGM_ONLY_IF_EXISTS );
    }

    asIScriptModule* CreateModule( const char* module_name )
    {
        // Delete old
        EngineData*      edata = (EngineData*) Engine->GetUserData();
        ScriptModuleVec& modules = edata->Modules;
        for( auto it = modules.begin(), end = modules.end(); it != end; ++it )
        {
            asIScriptModule* module = *it;
            if( Str::Compare( module->GetName(), module_name ) )
            {
                modules.erase( it );
                break;
            }
        }

        // Create new
        asIScriptModule* module = Engine->GetModule( module_name, asGM_ALWAYS_CREATE );
        if( !module )
            return NULL;
        modules.push_back( module );
        return module;
    }

    #ifndef FONLINE_SERVER
    void SetGarbageCollectTime( uint ticks )
    {
        GarbageCollectTime = ticks;
    }
    #endif

    #ifdef FONLINE_SERVER
    uint GetGCStatistics()
    {
        asUINT current_size = 0;
        Engine->GetGCStatistics( &current_size );
        return (uint) current_size;
    }

    void ScriptGarbager()
    {
        static uchar  garbager_state = 5;
        static uint   last_nongarbage = 0;
        static uint   best_count = 0;
        static uint   last_garbager_tick = 0;
        static uint   last_evaluation_tick = 0;

        static uint   garbager_count[ 3 ]; // Uses garbager_state as index
        static double garbager_time[ 3 ];  // Uses garbager_state as index

        switch( garbager_state )
        {
        case 5: // First time
        {
            best_count = 1000;
        }
        case 6: // Statistics init and first time
        {
            CollectGarbage( asGC_FULL_CYCLE | asGC_DESTROY_GARBAGE );

            last_nongarbage = GetGCStatistics();
            garbager_state = 0;
        }
        break;
        case 0: // Statistics stage 0, 1, 2
        case 1:
        case 2:
        {
            uint current_size = GetGCStatistics();

            // Try 1x, 1.5x and 2x count time for time extrapolation
            if( current_size < last_nongarbage + ( best_count * ( 2 + garbager_state ) ) / 2 )
                break;

            garbager_time[ garbager_state ] = Timer::AccurateTick();
            CollectGarbage( asGC_FULL_CYCLE | asGC_DESTROY_GARBAGE );
            garbager_time[ garbager_state ] = Timer::AccurateTick() - garbager_time[ garbager_state ];

            last_nongarbage = GetGCStatistics();
            garbager_count[ garbager_state ] = current_size - last_nongarbage;

            if( !garbager_count[ garbager_state ] )
                break;                                             // Repeat this step
            garbager_state++;
        }
        break;
        case 3: // Statistics last stage, calculate best count
        {
            double obj_times[ 2 ];
            bool   undetermined = false;
            for( int i = 0; i < 2; i++ )
            {
                if( garbager_count[ i + 1 ] == garbager_count[ i ] )
                {
                    undetermined = true;                   // Too low resolution, break statistics and repeat later
                    break;
                }

                obj_times[ i ] = ( garbager_time[ i + 1 ] - garbager_time[ i ] ) / ( (double) garbager_count[ i + 1 ] - (double) garbager_count[ i ] );

                if( obj_times[ i ] <= 0.0f )           // Should no happen
                {
                    undetermined = true;               // Too low resolution, break statistics and repeat later
                    break;
                }
            }
            garbager_state = 4;
            if( undetermined )
                break;

            double object_delete_time = ( obj_times[ 0 ] + obj_times[ 1 ] ) / 2;
            double overhead = 0.0f;
            for( int i = 0; i < 3; i++ )
                overhead += ( garbager_time[ i ] - garbager_count[ i ] * object_delete_time );
            overhead /= 3;
            if( overhead > MaxGarbagerTime )
                overhead = MaxGarbagerTime;                                    // Will result on deletion on every frame
            best_count = (uint) ( ( MaxGarbagerTime - overhead ) / object_delete_time );
        }
        break;
        case 4: // Normal garbage check
        {
            if( GarbagerCycle && Timer::FastTick() - last_garbager_tick >= GarbagerCycle )
            {
                CollectGarbage( asGC_ONE_STEP | asGC_DETECT_GARBAGE );
                last_garbager_tick = Timer::FastTick();
            }

            if( EvaluationCycle && Timer::FastTick() - last_evaluation_tick >= EvaluationCycle )
            {
                garbager_state = 6;               // Enter statistics after normal garbaging is done
                last_evaluation_tick = Timer::FastTick();
            }

            if( GetGCStatistics() > last_nongarbage + best_count )
            {
                CollectGarbage( asGC_FULL_CYCLE | asGC_DESTROY_GARBAGE );
            }
        }
        break;
        default:
            break;
        }
    }

    void CollectGarbage( asDWORD flags )
    {
        # ifdef SCRIPT_MULTITHREADING
        if( LogicMT )
            GarbageLocker.LockCode();
        Engine->GarbageCollect( flags );
        if( LogicMT )
            GarbageLocker.UnlockCode();
        # else
        Engine->GarbageCollect( flags );
        # endif
    }
    #endif

    #ifndef FONLINE_SERVER
    void CollectGarbage( bool force )
    {
        static uint last_tick = Timer::FastTick();
        if( force || ( GarbageCollectTime && Timer::FastTick() - last_tick >= GarbageCollectTime ) )
        {
            # ifdef SCRIPT_MULTITHREADING
            if( LogicMT )
                GarbageLocker.LockCode();
            if( Engine )
                Engine->GarbageCollect( asGC_FULL_CYCLE );
            if( LogicMT )
                GarbageLocker.UnlockCode();
            # else
            if( Engine )
                Engine->GarbageCollect( asGC_FULL_CYCLE );
            # endif

            last_tick = Timer::FastTick();
        }
    }
    #endif

    void RunTimeout( void* data )
    {
        while( RunTimeoutSuspend )
        {
            // Check execution time every 1/10 second
            Sleep( 100 );

            SCOPE_LOCK( ActiveGlobalCtxLocker );

            uint cur_tick = Timer::FastTick();
            for( auto it = ActiveContexts.begin(), end = ActiveContexts.end(); it != end; ++it )
            {
                ActiveContext& actx = *it;
                if( cur_tick >= actx.StartTick + RunTimeoutSuspend )
                {
                    // Suspend all contexts
                    for( int i = GLOBAL_CONTEXT_STACK_SIZE - 1; i >= 0; i-- )
                        if( actx.Contexts[ i ]->GetState() == asEXECUTION_ACTIVE )
                            actx.Contexts[ i ]->Suspend();

                    // Erase from collection
                    ActiveContexts.erase( it );
                    break;
                }
            }
        }
    }

    void SetRunTimeout( uint suspend_timeout, uint message_timeout )
    {
        RunTimeoutSuspend = suspend_timeout;
        RunTimeoutMessage = message_timeout;
    }

/************************************************************************/
/* Load / Bind                                                          */
/************************************************************************/

    void SetScriptsPath( int path_type )
    {
        ScriptsPath = path_type;
    }

    void Define( const char* def )
    {
        Preprocessor::Define( def );
    }

    void Undefine( const char* def )
    {
        if( def )
            Preprocessor::Undefine( def );
        else
            Preprocessor::UndefAll();
    }

    char* Preprocess( const char* fname, bool process_pragmas )
    {
        // Prepare preprocessor
        Preprocessor::VectorOutStream vos;
        Preprocessor::FileSource      fsrc;
        fsrc.CurrentDir = FileManager::GetFullPath( "", ScriptsPath );

        // Set current pragmas
        EngineData* edata = (EngineData*) Engine->GetUserData();
        Preprocessor::SetPragmaCallback( edata->PragmaCB );

        // Preprocess
        if( Preprocessor::Preprocess( fname, fsrc, vos, process_pragmas ) )
        {
            WriteLogF( _FUNC_, " - Unable to preprocess<%s> script.\n", fname );
            return NULL;
        }

        // Store result
        char* result = new char[ vos.GetSize() + 1 ];
        memcpy( result, vos.GetData(), vos.GetSize() );
        result[ vos.GetSize() ] = 0;
        return result;
    }

    void CallPragmas( const StrVec& pragmas )
    {
        EngineData* edata = (EngineData*) Engine->GetUserData();

        // Set current pragmas
        Preprocessor::SetPragmaCallback( edata->PragmaCB );

        // Call pragmas
        for( uint i = 0, j = (uint) pragmas.size() / 2; i < j; i++ )
        {
            Preprocessor::PragmaInstance pi;
            pi.text = pragmas[ i * 2 + 1 ];
            pi.current_file = "";
            pi.current_file_line = 0;
            pi.root_file = "";
            pi.global_line = 0;
            Preprocessor::CallPragma( pragmas[ i * 2 ], pi );
        }
    }

    bool LoadScript( const char* module_name, const char* source, bool skip_binary, const char* file_prefix /* = NULL */ )
    {
        EngineData*      edata = (EngineData*) Engine->GetUserData();
        ScriptModuleVec& modules = edata->Modules;

        // Compute whole version for server, client, mapper
        uint version = ( SERVER_VERSION << 20 ) | ( CLIENT_VERSION << 10 ) | MAPPER_VERSION;

        // Get script names
        char fname_real[ MAX_FOPATH ];
        Str::Copy( fname_real, module_name );
        Str::Replacement( fname_real, '.', DIR_SLASH_C );
        Str::Append( fname_real, ".fos" );
        FileManager::FormatPath( fname_real );

        char fname_script[ MAX_FOPATH ];
        if( file_prefix )
        {
            string temp = module_name;
            size_t pos = temp.find_last_of( DIR_SLASH_C );
            if( pos != string::npos )
            {
                temp.insert( pos + 1, file_prefix );
                Str::Copy( fname_script, temp.c_str() );
            }
            else
            {
                Str::Copy( fname_script, file_prefix );
                Str::Append( fname_script, module_name );
            }
        }
        else
        {
            Str::Copy( fname_script, module_name );
        }
        Str::Replacement( fname_script, '.', DIR_SLASH_C );
        Str::Append( fname_script, ".fos" );
        FileManager::FormatPath( fname_script );

        // Set current pragmas
        Preprocessor::SetPragmaCallback( edata->PragmaCB );

        // Try load precompiled script
        FileManager file_bin;
        if( !skip_binary )
        {
            FileManager file;
            file.LoadFile( fname_real, ScriptsPath );
            file_bin.LoadFile( Str::FormatBuf( "%sb", fname_script ), ScriptsPath );

            if( file_bin.IsLoaded() && file_bin.GetFsize() > sizeof( uint ) )
            {
                // Load file dependencies and pragmas
                char   str[ 1024 ];
                uint   bin_version = file_bin.GetBEUInt();
                uint   dependencies_size = file_bin.GetBEUInt();
                StrVec dependencies;
                for( uint i = 0; i < dependencies_size; i++ )
                {
                    file_bin.GetStr( str );
                    dependencies.push_back( str );
                }
                uint   pragmas_size = file_bin.GetBEUInt();
                StrVec pragmas;
                for( uint i = 0; i < pragmas_size; i++ )
                {
                    file_bin.GetStr( str );
                    pragmas.push_back( str );
                }

                // Check for outdated
                uint64 last_write, last_write_bin;
                file_bin.GetTime( NULL, NULL, &last_write_bin );
                // Main file
                file.GetTime( NULL, NULL, &last_write );
                bool no_all_files = !file.IsLoaded();
                bool outdated = ( file.IsLoaded() && last_write > last_write_bin );
                // Include files
                for( uint i = 0, j = (uint) dependencies.size(); i < j; i++ )
                {
                    FileManager file_dep;
                    file_dep.LoadFile( dependencies[ i ].c_str(), ScriptsPath );
                    file_dep.GetTime( NULL, NULL, &last_write );
                    if( !no_all_files )
                        no_all_files = !file_dep.IsLoaded();
                    if( !outdated )
                        outdated = ( file_dep.IsLoaded() && last_write > last_write_bin );
                }

                if( no_all_files || ( !outdated && bin_version == version ) )
                {
                    if( bin_version != version )
                        WriteLogF( _FUNC_, " - Script<%s> compiled in older server version.\n", module_name );
                    if( outdated )
                        WriteLogF( _FUNC_, " - Script<%s> outdated.\n", module_name );

                    // Delete old
                    for( auto it = modules.begin(), end = modules.end(); it != end; ++it )
                    {
                        asIScriptModule* module = *it;
                        if( Str::Compare( module->GetName(), module_name ) )
                        {
                            WriteLogF( _FUNC_, " - Warning, script for this name<%s> already exist. Discard it.\n", module_name );
                            Engine->DiscardModule( module_name );
                            modules.erase( it );
                            break;
                        }
                    }

                    asIScriptModule* module = Engine->GetModule( module_name, asGM_ALWAYS_CREATE );
                    if( module )
                    {
                        for( uint i = 0, j = (uint) pragmas.size() / 2; i < j; i++ )
                        {
                            Preprocessor::PragmaInstance pi;
                            pi.text = pragmas[ i * 2 + 1 ];
                            pi.current_file = "";
                            pi.current_file_line = 0;
                            pi.root_file = "";
                            pi.global_line = 0;
                            Preprocessor::CallPragma( pragmas[ i * 2 ], pi );
                        }

                        CBytecodeStream binary;
                        binary.Write( file_bin.GetCurBuf(), file_bin.GetFsize() - file_bin.GetCurPos() );

                        if( module->LoadByteCode( &binary ) >= 0 )
                        {
                            Preprocessor::SetParsedPragmas( pragmas );
                            modules.push_back( module );
                            return true;
                        }
                        else
                            WriteLogF( _FUNC_, " - Can't load binary, script<%s>.\n", module_name );
                    }
                    else
                        WriteLogF( _FUNC_, " - Create module fail, script<%s>.\n", module_name );
                }
            }

            if( !file.IsLoaded() )
            {
                WriteLogF( _FUNC_, " - Script<%s> not found.\n", fname_real );
                return false;
            }

            file_bin.UnloadFile();
        }

        Preprocessor::VectorOutStream vos;
        Preprocessor::FileSource      fsrc;
        Preprocessor::VectorOutStream vos_err;
        if( source )
            fsrc.Stream = source;
        fsrc.CurrentDir = FileManager::GetFullPath( "", ScriptsPath );

        if( Preprocessor::Preprocess( fname_real, fsrc, vos, true, &vos_err ) )
        {
            vos_err.PushNull();
            WriteLogF( _FUNC_, " - Unable to preprocess file<%s>, error<%s>.\n", fname_real, vos_err.GetData() );
            return false;
        }
        vos.Format();

        // Delete old
        for( auto it = modules.begin(), end = modules.end(); it != end; ++it )
        {
            asIScriptModule* module = *it;
            if( Str::Compare( module->GetName(), module_name ) )
            {
                WriteLogF( _FUNC_, " - Warning, script for this name<%s> already exist. Discard it.\n", module_name );
                Engine->DiscardModule( module_name );
                modules.erase( it );
                break;
            }
        }

        asIScriptModule* module = Engine->GetModule( module_name, asGM_ALWAYS_CREATE );
        if( !module )
        {
            WriteLogF( _FUNC_, " - Create module fail, script<%s>.\n", module_name );
            return false;
        }

        int result = module->AddScriptSection( module_name, vos.GetData(), vos.GetSize(), 0 );
        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Unable to AddScriptSection module<%s>, result<%d>.\n", module_name, result );
            return false;
        }

        result = module->Build();
        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Unable to Build module<%s>, result<%d>.\n", module_name, result );
            return false;
        }

        // Check not allowed global variables
        if( WrongGlobalObjects.size() )
        {
            IntVec bad_typeids;
            bad_typeids.reserve( WrongGlobalObjects.size() );
            for( auto it = WrongGlobalObjects.begin(), end = WrongGlobalObjects.end(); it != end; ++it )
                bad_typeids.push_back( Engine->GetTypeIdByDecl( ( *it ).c_str() ) & asTYPEID_MASK_SEQNBR );

            IntVec bad_typeids_class;
            for( int m = 0, n = module->GetObjectTypeCount(); m < n; m++ )
            {
                asIObjectType* ot = module->GetObjectTypeByIndex( m );
                for( int i = 0, j = ot->GetPropertyCount(); i < j; i++ )
                {
                    int type = 0;
                    ot->GetProperty( i, NULL, &type, NULL, NULL );
                    type &= asTYPEID_MASK_SEQNBR;
                    for( uint k = 0; k < bad_typeids.size(); k++ )
                    {
                        if( type == bad_typeids[ k ] )
                        {
                            bad_typeids_class.push_back( ot->GetTypeId() & asTYPEID_MASK_SEQNBR );
                            break;
                        }
                    }
                }
            }

            bool global_fail = false;
            for( int i = 0, j = module->GetGlobalVarCount(); i < j; i++ )
            {
                int type = 0;
                module->GetGlobalVar( i, NULL, &type, NULL );

                while( type & asTYPEID_TEMPLATE )
                {
                    asIObjectType* obj = (asIObjectType*) Engine->GetObjectTypeById( type );
                    if( !obj )
                        break;
                    type = obj->GetSubTypeId();
                }

                type &= asTYPEID_MASK_SEQNBR;

                for( uint k = 0; k < bad_typeids.size(); k++ )
                {
                    if( type == bad_typeids[ k ] )
                    {
                        const char* name = NULL;
                        module->GetGlobalVar( i, &name, NULL, NULL );
                        string      msg = "The global variable '" + string( name ) + "' uses a type that cannot be stored globally";
                        Engine->WriteMessage( "", 0, 0, asMSGTYPE_ERROR, msg.c_str() );
                        global_fail = true;
                        break;
                    }
                }
                if( std::find( bad_typeids_class.begin(), bad_typeids_class.end(), type ) != bad_typeids_class.end() )
                {
                    const char* name = NULL;
                    module->GetGlobalVar( i, &name, NULL, NULL );
                    string      msg = "The global variable '" + string( name ) + "' uses a type in class property that cannot be stored globally";
                    Engine->WriteMessage( "", 0, 0, asMSGTYPE_ERROR, msg.c_str() );
                    global_fail = true;
                }
            }

            if( global_fail )
            {
                WriteLogF( _FUNC_, " - Wrong global variables in module<%s>.\n", module_name );
                return false;
            }
        }

        // Done, add to collection
        modules.push_back( module );

        // Save binary version of script and preprocessed version
        if( !skip_binary && !file_bin.IsLoaded() )
        {
            CBytecodeStream binary;
            if( module->SaveByteCode( &binary ) >= 0 )
            {
                std::vector< asBYTE >& data = binary.GetBuf();
                const StrVec&          dependencies = Preprocessor::GetFileDependencies();
                const StrVec&          pragmas = Preprocessor::GetParsedPragmas();

                file_bin.SetBEUInt( version );
                file_bin.SetBEUInt( (uint) dependencies.size() );
                for( uint i = 0, j = (uint) dependencies.size(); i < j; i++ )
                    file_bin.SetData( (uchar*) dependencies[ i ].c_str(), (uint) dependencies[ i ].length() + 1 );
                file_bin.SetBEUInt( (uint) pragmas.size() );
                for( uint i = 0, j = (uint) pragmas.size(); i < j; i++ )
                    file_bin.SetData( (uchar*) pragmas[ i ].c_str(), (uint) pragmas[ i ].length() + 1 );
                file_bin.SetData( &data[ 0 ], (uint) data.size() );

                if( !file_bin.SaveOutBufToFile( Str::FormatBuf( "%sb", fname_script ), ScriptsPath ) )
                    WriteLogF( _FUNC_, " - Can't save bytecode, script<%s>.\n", module_name );
            }
            else
            {
                WriteLogF( _FUNC_, " - Can't write bytecode, script<%s>.\n", module_name );
            }

            FileManager file_prep;
            file_prep.SetData( (void*) vos.GetData(), vos.GetSize() );
            if( !file_prep.SaveOutBufToFile( Str::FormatBuf( "%sp", fname_script ), ScriptsPath ) )
                WriteLogF( _FUNC_, " - Can't write preprocessed file, script<%s>.\n", module_name );
        }
        return true;
    }

    bool LoadScript( const char* module_name, const uchar* bytecode, uint len )
    {
        if( !bytecode || !len )
        {
            WriteLogF( _FUNC_, " - Bytecode empty, module name<%s>.\n", module_name );
            return false;
        }

        EngineData*      edata = (EngineData*) Engine->GetUserData();
        ScriptModuleVec& modules = edata->Modules;

        for( auto it = modules.begin(), end = modules.end(); it != end; ++it )
        {
            asIScriptModule* module = *it;
            if( Str::Compare( module->GetName(), module_name ) )
            {
                WriteLogF( _FUNC_, " - Warning, script for this name<%s> already exist. Discard it.\n", module_name );
                Engine->DiscardModule( module_name );
                modules.erase( it );
                break;
            }
        }

        asIScriptModule* module = Engine->GetModule( module_name, asGM_ALWAYS_CREATE );
        if( !module )
        {
            WriteLogF( _FUNC_, " - Create module fail, script<%s>.\n", module_name );
            return false;
        }

        CBytecodeStream binary;
        binary.Write( bytecode, len );
        int             result = module->LoadByteCode( &binary );
        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Can't load binary, module<%s>, result<%d>.\n", module_name, result );
            return false;
        }

        modules.push_back( module );
        return true;
    }

    int BindImportedFunctions()
    {
        // WriteLog("Bind all imported functions...\n");

        EngineData*      edata = (EngineData*) Engine->GetUserData();
        ScriptModuleVec& modules = edata->Modules;
        int              errors = 0;
        for( auto it = modules.begin(), end = modules.end(); it != end; ++it )
        {
            asIScriptModule* module = *it;
            int              result = module->BindAllImportedFunctions();
            if( result < 0 )
            {
                WriteLogF( _FUNC_, " - Fail to bind imported functions, module<%s>, error<%d>.\n", module->GetName(), result );
                errors++;
                continue;
            }
        }

        // WriteLog("Bind all imported functions complete.\n");
        return errors;
    }

    int Bind( const char* module_name, const char* func_name, const char* decl, bool is_temp, bool disable_log /* = false */ )
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
                    WriteLogF( _FUNC_, " - Module<%s> not found.\n", module_name );
                return 0;
            }

            // Find function
            char decl_[ 256 ];
            Str::Format( decl_, decl, func_name );
            int  result = module->GetFunctionIdByDecl( decl_ );
            if( result <= 0 )
            {
                if( !disable_log )
                    WriteLogF( _FUNC_, " - Function<%s> in module<%s> not found, result<%d>.\n", decl_, module_name, result );
                return 0;
            }

            // Save to temporary bind
            if( is_temp )
            {
                BindedFunctions[ 1 ].IsScriptCall = true;
                BindedFunctions[ 1 ].ScriptFuncId = result;
                BindedFunctions[ 1 ].NativeFuncAddr = 0;
                return 1;
            }

            // Find already binded
            for( int i = 2, j = (int) BindedFunctions.size(); i < j; i++ )
            {
                BindFunction& bf = BindedFunctions[ i ];
                if( bf.IsScriptCall && bf.ScriptFuncId == result )
                    return i;
            }

            // Create new bind
            BindedFunctions.push_back( BindFunction( result, 0, module_name, func_name, decl ) );
        }
        else
        {
            // Load dynamic library
            void* dll = LoadDynamicLibrary( module_name );
            if( !dll )
            {
                if( !disable_log )
                    WriteLogF( _FUNC_, " - Dll<%s> not found in scripts folder, error<%s>.\n", module_name, DLL_Error() );
                return 0;
            }

            // Load function
            size_t func = (size_t) DLL_GetAddress( dll, func_name );
            if( !func )
            {
                if( !disable_log )
                    WriteLogF( _FUNC_, " - Function<%s> in dll<%s> not found, error<%s>.\n", func_name, module_name, DLL_Error() );
                return 0;
            }

            // Save to temporary bind
            if( is_temp )
            {
                BindedFunctions[ 1 ].IsScriptCall = false;
                BindedFunctions[ 1 ].ScriptFuncId = 0;
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
            BindedFunctions.push_back( BindFunction( 0, func, module_name, func_name, decl ) );
        }
        return (int) BindedFunctions.size() - 1;
    }

    int Bind( const char* script_name, const char* decl, bool is_temp, bool disable_log /* = false */ )
    {
        char module_name[ 256 ];
        char func_name[ 256 ];
        if( !ReparseScriptName( script_name, module_name, func_name, disable_log ) )
        {
            WriteLogF( _FUNC_, " - Parse script name<%s> fail.\n", script_name );
            return 0;
        }
        return Bind( module_name, func_name, decl, is_temp, disable_log );
    }

    int RebindFunctions()
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
                int bind_id = Bind( bf.ModuleName.c_str(), bf.FuncName.c_str(), bf.FuncDecl.c_str(), true );
                if( bind_id <= 0 )
                {
                    WriteLogF( _FUNC_, " - Unable to bind function, module<%s>, function<%s>, declaration<%s>.\n", bf.ModuleName.c_str(), bf.FuncName.c_str(), bf.FuncDecl.c_str() );
                    bf.ScriptFuncId = 0;
                    errors++;
                }
                else
                {
                    bf.ScriptFuncId = BindedFunctions[ 1 ].ScriptFuncId;
                }
            }
        }
        return errors;
    }

    bool ReparseScriptName( const char* script_name, char* module_name, char* func_name, bool disable_log /* = false */ )
    {
        if( !script_name || !module_name || !func_name )
        {
            WriteLogF( _FUNC_, " - Some name null ptr.\n" );
            CreateDump( "ReparseScriptName" );
            return false;
        }

        char script[ MAX_SCRIPT_NAME * 2 + 2 ];
        Str::Copy( script, script_name );
        Str::EraseChars( script, ' ' );

        if( Str::Substring( script, "@" ) )
        {
            char* script_ptr = &script[ 0 ];
            Str::CopyWord( module_name, script_ptr, '@' );
            Str::GoTo( ( char* & )script_ptr, '@', true );
            Str::CopyWord( func_name, script_ptr, '\0' );
        }
        else
        {
            Str::Copy( module_name, MAX_SCRIPT_NAME, GetActiveModuleName() );
            Str::Copy( func_name, MAX_SCRIPT_NAME, script );
        }

        if( !Str::Length( module_name ) || Str::Compare( module_name, "<error>" ) )
        {
            if( !disable_log )
                WriteLogF( _FUNC_, " - Script name parse error, string<%s>.\n", script_name );
            module_name[ 0 ] = func_name[ 0 ] = 0;
            return false;
        }
        if( !Str::Length( func_name ) )
        {
            if( !disable_log )
                WriteLogF( _FUNC_, " - Function name parse error, string<%s>.\n", script_name );
            module_name[ 0 ] = func_name[ 0 ] = 0;
            return false;
        }
        return true;
    }

/************************************************************************/
/* Functions accord                                                     */
/************************************************************************/

    const StrVec& GetScriptFuncCache()
    {
        return ScriptFuncCache;
    }

    void ResizeCache( uint count )
    {
        ScriptFuncCache.resize( count );
        ScriptFuncBindId.resize( count );
    }

    uint GetScriptFuncNum( const char* script_name, const char* decl )
    {
        #ifdef SCRIPT_MULTITHREADING
        SCOPE_LOCK( BindedFunctionsLocker );
        #endif

        char full_name[ MAX_SCRIPT_NAME * 2 + 1 ];
        char module_name[ MAX_SCRIPT_NAME + 1 ];
        char func_name[ MAX_SCRIPT_NAME + 1 ];
        if( !Script::ReparseScriptName( script_name, module_name, func_name ) )
            return 0;
        Str::Format( full_name, "%s@%s", module_name, func_name );

        string str_cache = full_name;
        str_cache += "|";
        str_cache += decl;

        // Find already cached
        for( uint i = 0, j = (uint) ScriptFuncCache.size(); i < j; i++ )
            if( ScriptFuncCache[ i ] == str_cache )
                return i + 1;

        // Create new
        int bind_id = Script::Bind( full_name, decl, false );
        if( bind_id <= 0 )
            return 0;

        ScriptFuncCache.push_back( str_cache );
        ScriptFuncBindId.push_back( bind_id );
        return (uint) ScriptFuncCache.size();
    }

    int GetScriptFuncBindId( uint func_num )
    {
        #ifdef SCRIPT_MULTITHREADING
        SCOPE_LOCK( BindedFunctionsLocker );
        #endif

        func_num--;
        if( func_num >= ScriptFuncBindId.size() )
        {
            WriteLogF( _FUNC_, " - Function index<%u> is greater than bind buffer size<%u>.\n", func_num, ScriptFuncBindId.size() );
            return 0;
        }
        return ScriptFuncBindId[ func_num ];
    }

    string GetScriptFuncName( uint func_num )
    {
        #ifdef SCRIPT_MULTITHREADING
        SCOPE_LOCK( BindedFunctionsLocker );
        #endif

        func_num--;
        if( func_num >= ScriptFuncBindId.size() )
        {
            WriteLogF( _FUNC_, " - Function index<%u> is greater than bind buffer size<%u>.\n", func_num, ScriptFuncBindId.size() );
            return "error func index";
        }

        if( ScriptFuncBindId[ func_num ] >= (int) BindedFunctions.size() )
        {
            WriteLogF( _FUNC_, " - Bind index<%u> is greater than bind buffer size<%u>.\n", ScriptFuncBindId[ func_num ], BindedFunctions.size() );
            return "error bind index";
        }

        BindFunction& bf = BindedFunctions[ ScriptFuncBindId[ func_num ] ];
        string        result;
        result += bf.ModuleName;
        result += "@";
        result += bf.FuncName;
        return result;

        /*BindFunction& bf=BindedFunctions[ScriptFuncBindId[func_num]];
           string result;
           char buf[32];
           result+="(";
           result+=_itoa(func_num+1,buf,10);
           result+=") ";
           result+=bf.ModuleName;
           result+="@";
           result+=bf.FuncName;
           result+="(";
           result+=bf.FuncDecl;
           result+=")";
           return result;*/
    }

/************************************************************************/
/* Contexts                                                             */
/************************************************************************/

    THREAD bool              ScriptCall = false;
    THREAD asIScriptContext* CurrentCtx = NULL;
    THREAD size_t            NativeFuncAddr = 0;
    THREAD size_t            NativeArgs[ 256 ] = { 0 };
    THREAD size_t            NativeRetValue = 0;
    THREAD size_t            CurrentArg = 0;
    THREAD int               ExecutionRecursionCounter = 0;

    #ifdef SCRIPT_MULTITHREADING
    uint       SynchronizeThreadId = 0;
    int        SynchronizeThreadCounter = 0;
    MutexEvent SynchronizeThreadLocker;
    Mutex      SynchronizeThreadLocalLocker;

    typedef vector< EndExecutionCallback > EndExecutionCallbackVec;
    THREAD EndExecutionCallbackVec* EndExecutionCallbacks;
    #endif

    void BeginExecution()
    {
        #ifdef SCRIPT_MULTITHREADING
        if( !LogicMT )
            return;

        if( !ExecutionRecursionCounter )
        {
            GarbageLocker.EnterCode();

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

    void EndExecution()
    {
        #ifdef SCRIPT_MULTITHREADING
        if( !LogicMT )
            return;

        ExecutionRecursionCounter--;
        if( !ExecutionRecursionCounter )
        {
            GarbageLocker.LeaveCode();

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
                    SynchronizeThreadLocker.Allow();             // Unlock synchronization section
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
                                         ( *func )( );
                }
                EndExecutionCallbacks->clear();
            }
        }
        #endif
    }

    void AddEndExecutionCallback( EndExecutionCallback func )
    {
        #ifdef SCRIPT_MULTITHREADING
        if( !LogicMT )
            return;

        if( !EndExecutionCallbacks )
            EndExecutionCallbacks = new (nothrow) EndExecutionCallbackVec();
        auto it = std::find( EndExecutionCallbacks->begin(), EndExecutionCallbacks->end(), func );
        if( it == EndExecutionCallbacks->end() )
            EndExecutionCallbacks->push_back( func );
        #endif
    }

    bool PrepareContext( int bind_id, const char* call_func, const char* ctx_info )
    {
        #ifdef SCRIPT_MULTITHREADING
        if( LogicMT )
            BindedFunctionsLocker.Lock();
        #endif

        if( bind_id <= 0 || bind_id >= (int) BindedFunctions.size() )
        {
            WriteLogF( _FUNC_, " - Invalid bind id<%d>. Context info<%s>.\n", bind_id, ctx_info );
            #ifdef SCRIPT_MULTITHREADING
            if( LogicMT )
                BindedFunctionsLocker.Unlock();
            #endif
            return false;
        }

        BindFunction& bf = BindedFunctions[ bind_id ];
        bool          is_script = bf.IsScriptCall;
        int           func_id = bf.ScriptFuncId;
        size_t        func_addr = bf.NativeFuncAddr;

        #ifdef SCRIPT_MULTITHREADING
        if( LogicMT )
            BindedFunctionsLocker.Unlock();
        #endif

        if( is_script )
        {
            if( func_id <= 0 )
                return false;

            asIScriptContext* ctx = GetGlobalContext();
            if( !ctx )
                return false;

            BeginExecution();

            Str::Copy( (char*) ctx->GetUserData(), CONTEXT_BUFFER_SIZE, call_func );
            Str::Append( (char*) ctx->GetUserData(), CONTEXT_BUFFER_SIZE, " : " );
            Str::Append( (char*) ctx->GetUserData(), CONTEXT_BUFFER_SIZE, ctx_info );

            int result = ctx->Prepare( func_id );
            if( result < 0 )
            {
                WriteLogF( _FUNC_, " - Prepare error, context name<%s>, bind_id<%d>, func_id<%d>, error<%d>.\n", ctx->GetUserData(), bind_id, func_id, result );
                GlobalCtxIndex--;
                EndExecution();
                return false;
            }

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

    void SetArgUShort( ushort value )
    {
        if( ScriptCall )
            CurrentCtx->SetArgWord( (asUINT) CurrentArg, value );
        else
            NativeArgs[ CurrentArg ] = value;
        CurrentArg++;
    }

    void SetArgUInt( uint value )
    {
        if( ScriptCall )
            CurrentCtx->SetArgDWord( (asUINT) CurrentArg, value );
        else
            NativeArgs[ CurrentArg ] = value;
        CurrentArg++;
    }

    void SetArgUChar( uchar value )
    {
        if( ScriptCall )
            CurrentCtx->SetArgByte( (asUINT) CurrentArg, value );
        else
            NativeArgs[ CurrentArg ] = value;
        CurrentArg++;
    }

    void SetArgBool( bool value )
    {
        if( ScriptCall )
            CurrentCtx->SetArgByte( (asUINT) CurrentArg, value );
        else
            NativeArgs[ CurrentArg ] = value;
        CurrentArg++;
    }

    void SetArgObject( void* value )
    {
        if( ScriptCall )
            CurrentCtx->SetArgObject( (asUINT) CurrentArg, value );
        else
            NativeArgs[ CurrentArg ] = (size_t) value;
        CurrentArg++;
    }

    void SetArgAddress( void* value )
    {
        if( ScriptCall )
            CurrentCtx->SetArgAddress( (asUINT) CurrentArg, value );
        else
            NativeArgs[ CurrentArg ] = (size_t) value;
        CurrentArg++;
    }

    // Taked from AS sources
    #if defined ( FO_MSVC )
    size_t CallCDeclFunction32( const size_t* args, size_t paramSize, size_t func )
    #else
    size_t __attribute( ( __noinline__ ) ) CallCDeclFunction32( const size_t * args, size_t paramSize, size_t func )
    #endif
    {
        #if defined ( FO_MSVC )
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

            // Restore registers
            pop  ecx

            // return value in EAX or EAX:EDX
        }

        #elif defined ( FO_GCC )
        args = args;
        paramSize = paramSize;
        func = func;

        asm ( "pushl %ecx           \n"
              "fninit               \n"

              // Need to align the stack pointer so that it is aligned to 16 bytes when making the function call.
              // It is assumed that when entering this function, the stack pointer is already aligned, so we need
              // to calculate how much we will put on the stack during this call.
              "movl  12(%ebp), %eax \n" // paramSize
              "addl  $4, %eax       \n" // counting esp that we will push on the stack
              "movl  %esp, %ecx     \n"
              "subl  %eax, %ecx     \n"
              "andl  $15, %ecx      \n"
              "movl  %esp, %eax     \n"
              "subl  %ecx, %esp     \n"
              "pushl %eax           \n" // Store the original stack pointer

              "movl  12(%ebp), %ecx \n" // paramSize
              "movl  8(%ebp), %eax  \n" // args
              "addl  %ecx, %eax     \n" // push arguments on the stack
              "cmp   $0, %ecx       \n"
              "je    endcopy        \n"
              "copyloop:            \n"
              "subl  $4, %eax       \n"
              "pushl (%eax)         \n"
              "subl  $4, %ecx       \n"
              "jne   copyloop       \n"
              "endcopy:             \n"
              "call  *16(%ebp)      \n"
              "addl  12(%ebp), %esp \n" // pop arguments

              // Pop the alignment bytes
              "popl  %esp           \n"

              "popl  %ecx           \n" );
        #endif
    }

    bool RunPrepared()
    {
        if( ScriptCall )
        {
            asIScriptContext* ctx = CurrentCtx;
            uint              tick = Timer::FastTick();

            if( GlobalCtxIndex == 1 ) // First context from stack, add timing
            {
                SCOPE_LOCK( ActiveGlobalCtxLocker );
                ActiveContexts.push_back( ActiveContext( GlobalCtx, tick ) );
            }

            int result = ctx->Execute();

            GlobalCtxIndex--;
            if( GlobalCtxIndex == 0 ) // All scripts execution complete, erase timing
            {
                SCOPE_LOCK( ActiveGlobalCtxLocker );
                auto it = std::find( ActiveContexts.begin(), ActiveContexts.end(), (asIScriptContext**) GlobalCtx );
                if( it != ActiveContexts.end() )
                    ActiveContexts.erase( it );
            }

            uint            delta = Timer::FastTick() - tick;

            asEContextState state = ctx->GetState();
            if( state != asEXECUTION_FINISHED )
            {
                if( state == asEXECUTION_EXCEPTION )
                    WriteLog( "Execution of script stopped due to exception.\n" );
                else if( state == asEXECUTION_SUSPENDED )
                    WriteLog( "Execution of script stopped due to timeout<%u>.\n", RunTimeoutSuspend );
                else
                    WriteLog( "Execution of script stopped due to %s.\n", ContextStatesStr[ (int) state ] );
                PrintContextCallstack( ctx );       // Name and state of context will be printed in this function
                ctx->Abort();
                EndExecution();
                return false;
            }
            else if( RunTimeoutMessage && delta >= RunTimeoutMessage )
            {
                WriteLog( "Script work time<%u> in context<%s>.\n", delta, ctx->GetUserData() );
            }

            if( result < 0 )
            {
                WriteLogF( _FUNC_, " - Context<%s> execute error<%d>, state<%s>.\n", ctx->GetUserData(), result, ContextStatesStr[ (int) state ] );
                EndExecution();
                return false;
            }

            CurrentCtx = ctx;
            ScriptCall = true;
        }
        else
        {
            NativeRetValue = CallCDeclFunction32( NativeArgs, CurrentArg * 4, NativeFuncAddr );
            ScriptCall = false;
        }

        EndExecution();
        return true;
    }

    uint GetReturnedUInt()
    {
        return ScriptCall ? CurrentCtx->GetReturnDWord() : (uint) NativeRetValue;
    }

    bool GetReturnedBool()
    {
        return ScriptCall ? ( CurrentCtx->GetReturnByte() != 0 ) : ( ( NativeRetValue & 1 ) != 0 );
    }

    void* GetReturnedObject()
    {
        return ScriptCall ? CurrentCtx->GetReturnObject() : (void*) NativeRetValue;
    }

    bool SynchronizeThread()
    {
        #ifdef SCRIPT_MULTITHREADING
        if( !ConcurrentExecution )
            return true;

        SynchronizeThreadLocalLocker.Lock(); // Local lock

        if( !SynchronizeThreadId )           // Section is free
        {
            SynchronizeThreadId = Thread::GetCurrentId();
            SynchronizeThreadCounter = 1;
            SynchronizeThreadLocker.Disallow();     // Lock synchronization section
            SynchronizeThreadLocalLocker.Unlock();  // Local unlock

            SyncManager* sync_mngr = SyncManager::GetForCurThread();
            sync_mngr->PushPriority( 10 );
        }
        else if( SynchronizeThreadId == Thread::GetCurrentId() ) // Section busy by current thread
        {
            SynchronizeThreadCounter++;
            SynchronizeThreadLocalLocker.Unlock();               // Local unlock
        }
        else // Section busy by another thread
        {
            SynchronizeThreadLocalLocker.Unlock(); // Local unlock

            SyncManager* sync_mngr = SyncManager::GetForCurThread();
            sync_mngr->Suspend();                  // Allow other threads take objects
            SynchronizeThreadLocker.Wait();        // Sleep until synchronization section locked
            sync_mngr->Resume();                   // Relock busy objects
            return SynchronizeThread();            // Try enter again
        }
        #endif
        return true;
    }

    bool ResynchronizeThread()
    {
        #ifdef SCRIPT_MULTITHREADING
        if( !ConcurrentExecution )
            return true;

        SynchronizeThreadLocalLocker.Lock(); // Local lock

        if( SynchronizeThreadId == Thread::GetCurrentId() )
        {
            if( --SynchronizeThreadCounter == 0 )
            {
                SynchronizeThreadId = 0;
                SynchronizeThreadLocker.Allow();         // Unlock synchronization section
                SynchronizeThreadLocalLocker.Unlock();   // Local unlock

                SyncManager* sync_mngr = SyncManager::GetForCurThread();
                sync_mngr->PopPriority();
            }
            else
            {
                SynchronizeThreadLocalLocker.Unlock();         // Local unlock
            }
        }
        else
        {
            // Invalid call
            SynchronizeThreadLocalLocker.Unlock();     // Local unlock
            return false;
        }
        #endif
        return true;
    }

/************************************************************************/
/* Logging                                                              */
/************************************************************************/

    bool StartLog()
    {
        if( EngineLogFile )
            return true;
        EngineLogFile = FileOpen( DIR_SLASH_SD "FOscript.log", true );
        if( !EngineLogFile )
            return false;
        LogA( "Start logging script system.\n" );
        return true;
    }

    void EndLog()
    {
        if( !EngineLogFile )
            return;
        LogA( "End logging script system.\n" );
        FileClose( EngineLogFile );
        EngineLogFile = NULL;
    }

    void Log( const char* str )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( !ctx )
        {
            LogA( str );
            return;
        }
        asIScriptFunction* func = ctx->GetFunction( 0 );
        if( !func )
        {
            LogA( str );
            return;
        }
        if( LogDebugInfo )
        {
            int line, column;
            line = ctx->GetLineNumber( 0, &column );
            LogA( Str::FormatBuf( "Script callback: %s : %s : %s : %d, %d : %s.\n", str, func->GetModuleName(), func->GetDeclaration( true ), line, column, ctx->GetUserData() ) );
        }
        else
            LogA( Str::FormatBuf( "%s : %s\n", func->GetModuleName(), str ) );
    }

    void LogA( const char* str )
    {
        WriteLog( "%s", str );
        if( !EngineLogFile )
            return;
        FileWrite( EngineLogFile, str, Str::Length( str ) );
    }

    void LogError( const char* call_func, const char* error )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( !ctx )
        {
            LogA( error );
            return;
        }
        asIScriptFunction* func = ctx->GetFunction( 0 );
        if( !func )
        {
            LogA( error );
            return;
        }
        int line, column;
        line = ctx->GetLineNumber( 0, &column );
        LogA( Str::FormatBuf( "%s : Script error: %s : %s : %s : %d, %d : %s.\n", call_func, error, func->GetModuleName(), func->GetDeclaration( true ), line, column, ctx->GetUserData() ) );
    }

    void SetLogDebugInfo( bool enabled )
    {
        LogDebugInfo = enabled;
    }

    void CallbackMessage( const asSMessageInfo* msg, void* param )
    {
        const char* type = "Error";
        if( msg->type == asMSGTYPE_WARNING )
            type = "Warning";
        else if( msg->type == asMSGTYPE_INFORMATION )
            type = "Info";
        LogA( Str::FormatBuf( "Script message: %s : %s : %s : %d, %d.\n", msg->section, type, msg->message, msg->row, msg->col ) );
    }

    void CallbackException( asIScriptContext* ctx, void* param )
    {
        int                line, column;
        line = ctx->GetExceptionLineNumber( &column );
        asIScriptFunction* func = Engine->GetFunctionById( ctx->GetExceptionFunction() );
        if( !func )
        {
            LogA( Str::FormatBuf( "Script exception: %s : %s.\n", ctx->GetExceptionString(), ctx->GetUserData() ) );
            return;
        }
        LogA( Str::FormatBuf( "Script exception: %s : %s : %s : %d, %d : %s.\n", ctx->GetExceptionString(), func->GetModuleName(), func->GetDeclaration( true ), line, column, ctx->GetUserData() ) );
    }

/************************************************************************/
/* Array                                                                */
/************************************************************************/

    CScriptArray* CreateArray( const char* type )
    {
        return new CScriptArray( 0, Engine->GetObjectTypeById( Engine->GetTypeIdByDecl( type ) ) );
    }

/************************************************************************/
/*                                                                      */
/************************************************************************/

}
