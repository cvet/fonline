#include "Common.h"
#include "ScriptPragmas.h"
#include "ScriptEngine.h"
#include "angelscript.h"
#include "preprocessor.h"
#include "scriptany.h"
#include "scriptdictionary.h"
#include "scriptdict.h"
#include "scriptfile.h"
#include "scriptstring.h"
#include "scriptarray.h"
#include "reflection.h"
#include "AngelScript/sdk/add_on/scriptmath/scriptmath.h"
#include "AngelScript/sdk/add_on/weakref/weakref.h"
#include "AngelScript/sdk/add_on/scripthelper/scripthelper.h"

#ifdef FO_LINUX
# include <unistd.h>
# define _stricmp    strcasecmp
#endif

int Compile( const char* fname, const char* fname_prep, vector< char* >& defines, vector< char* >& run_func );

bool RaiseAssert( const char* message, const char* file, int line ) // For RUNTIME_ASSERT
{
    ExitProcess( 0 );
    return 0;
}

asIScriptEngine* Engine = NULL;
bool             IsServer = false;
bool             IsClient = false;
bool             IsMapper = false;
char*            Buf = NULL;
bool             CollectGarbage = false;

const char*      ContextStatesStr[] =
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

void PrintContextCallstack( asIScriptContext* ctx )
{
    int                      line, column;
    const asIScriptFunction* func;
    int                      stack_size = ctx->GetCallstackSize();
    printf( "State<%s>, call stack<%d>:\n", ContextStatesStr[ (int) ctx->GetState() ], stack_size );

    // Print current function
    if( ctx->GetState() == asEXECUTION_EXCEPTION )
    {
        line = ctx->GetExceptionLineNumber( &column );
        func = ctx->GetExceptionFunction();
    }
    else
    {
        line = ctx->GetLineNumber( 0, &column );
        func = ctx->GetFunction( 0 );
    }
    if( func )
        printf( "  %d) %s : %s : %d, %d.\n", stack_size - 1, func->GetModuleName(), func->GetDeclaration(), line, column );

    // Print call stack
    for( int i = 1; i < stack_size; i++ )
    {
        func = ctx->GetFunction( i );
        line = ctx->GetLineNumber( i, &column );
        if( func )
            printf( "  %d) %s : %s : %d, %d.\n", stack_size - i - 1, func->GetModuleName(), func->GetDeclaration(), line, column );
    }
}

void RunMain( asIScriptModule* module, const char* func_str )
{
    // Make function declaration
    char func_decl[ 1024 ];
    sprintf( func_decl, "void %s()", func_str );

    // Run
    printf( "Executing '%s'.\n", func_str );
    asIScriptContext*  ctx = Engine->CreateContext();
    asIScriptFunction* func = module->GetFunctionByDecl( func_decl );
    if( !func )
    {
        printf( "Function '%s' not found.\n", func_decl );
        return;
    }
    int result = ctx->Prepare( func );
    if( result < 0 )
    {
        printf( "Context preparation failure, error code<%d>.\n", result );
        return;
    }

    result = ctx->Execute();
    asEContextState state = ctx->GetState();
    if( state != asEXECUTION_FINISHED )
    {
        if( state == asEXECUTION_EXCEPTION )
            printf( "Execution of script stopped due to exception '%s'.\n", ctx->GetExceptionString() );
        else if( state == asEXECUTION_SUSPENDED )
            printf( "Execution of script stopped due to timeout.\n" );
        else
            printf( "Execution of script stopped due to '%s'.\n", ContextStatesStr[ (int) state ] );
        PrintContextCallstack( ctx );
        ctx->Abort();
        return;
    }

    if( result < 0 )
    {
        printf( "Execution error<%d>, state<%s>.\n", result, ContextStatesStr[ (int) state ] );
    }
    printf( "Execution finished.\n" );
}

void* GetScriptEngine()
{
    return Engine;
}

const char* GetDllTarget()
{
    if( IsServer )
        return "SERVER";
    if( IsClient )
        return "CLIENT";
    if( IsMapper )
        return "MAPPER";
    return "UNKNOWN";
}

void CallBack( const asSMessageInfo* msg, void* param )
{
    const char* type = "ERROR";
    if( msg->type == asMSGTYPE_WARNING )
        type = "WARNING";
    else if( msg->type == asMSGTYPE_INFORMATION )
        type = "INFO";

    if( msg->type != asMSGTYPE_INFORMATION )
    {
        if( msg->row )
        {
            printf( "%s(%d) : %s : %s.\n", Preprocessor::ResolveOriginalFile( msg->row ).c_str(), Preprocessor::ResolveOriginalLine( msg->row ), type, msg->message );
        }
        else
        {
            printf( "%s : %s.\n", type, msg->message );
        }
    }
    else
    {
        printf( "%s(%d) : %s : %s.\n", Preprocessor::ResolveOriginalFile( msg->row ).c_str(), Preprocessor::ResolveOriginalLine( msg->row ), type, msg->message );
    }
}

// Server
#define BIND_SERVER
#define BIND_CLASS    BindClass::
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { printf( "Bind error, line<" # x ">.\n" ); bind_errors++; }
namespace ServerBind
{
    #include "DummyData.h"
    static int Bind( asIScriptEngine* engine, PropertyRegistrator** registrators )
    {
        int bind_errors = 0;
        #include "ScriptBind.h"
        return bind_errors;
    }
}

// Client
#undef BIND_SERVER
#undef BIND_CLASS
#undef BIND_ASSERT
#define BIND_CLIENT
#define BIND_CLASS    BindClass::
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { printf( "Bind error, line<" # x ">.\n" ); bind_errors++; }
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

// Mapper
#undef BIND_CLIENT
#undef BIND_CLASS
#undef BIND_ASSERT
#define BIND_MAPPER
#define BIND_CLASS    BindClass::
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { printf( "Bind error, line<" # x ">.\n" ); bind_errors++; }
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

int main( int argc, char* argv[] )
{
    // Make command line
    SetCommandLine( argc, argv );

    // Initialization
    Timer::Init();

    // Show usage
    if( argc < 2 )
    {
        printf( "FOnline AngleScript compiler. Usage:\n"
                "ASCompiler script_name.fos\n"
                " [-server] (compile as server script)\n"
                " [-client] (compile as client script)\n"
                " [-mapper] (compile as mapper script)\n"
                " [-p preprocessor_output.txt]\n"
                " [-d SOME_DEFINE]*\n"
                " [-run func_name]*\n"
                " [-gc] (collect garbage after execution)\n"
                "*can be used multiple times\n" );
        return 0;
    }

    // Parse args
    char*           str_fname = argv[ 1 ];
    char*           str_prep = NULL;
    vector< char* > defines;
    vector< char* > run_func;
    for( int i = 2; i < argc; i++ )
    {
        // Server / Client / Mapper
        if( !_stricmp( argv[ i ], "-server" ) )
            IsServer = true, IsClient = false, IsMapper = false;
        else if( !_stricmp( argv[ i ], "-client" ) )
            IsServer = false, IsClient = true, IsMapper = false;
        else if( !_stricmp( argv[ i ], "-mapper" ) )
            IsServer = false, IsClient = false, IsMapper = true;
        // Preprocessor output
        else if( !_stricmp( argv[ i ], "-p" ) && i + 1 < argc )
            str_prep = argv[ ++i ];
        // Define
        else if( !_stricmp( argv[ i ], "-d" ) && i + 1 < argc )
            defines.push_back( argv[ ++i ] );
        // Run function
        else if( !_stricmp( argv[ i ], "-run" ) && i + 1 < argc )
            run_func.push_back( argv[ ++i ] );
        // Collect garbage
        else if( !_stricmp( argv[ i ], "-gc" ) )
            CollectGarbage = true;
    }

    // Fix path
    FixPathSlashes( str_fname );
    if( str_prep )
        FixPathSlashes( str_prep );

    // Set current directory
    char path[ 2048 ];
    Str::Copy( path, str_fname );
    FileManager::ExtractPath( path, path );
    if( ResolvePath( path ) )
    {
        #ifdef FO_WINDOWS
        SetCurrentDirectory( path );
        #else
        chdir( path );
        #endif
    }

    if( !IsServer && !IsClient && !IsMapper )
    {
        FILE* f = fopen( str_fname, "rb" );
        if( !f )
        {
            printf( "File<%s> not found.\n", str_fname );
            return -1;
        }

        char line[ MAX_FOTEXT ];
        if( !fgets( line, sizeof( line ), f ) )
        {
            printf( "File<%s> empty.\n", str_fname );
            return -1;
        }
        fclose( f );

        // Check script signature
        if( !Str::Substring( line, "FOS" ) )
        {
            printf( "FOnline script signature 'FOS' not found in first line.\n" );
            return -1;
        }

        // Compile as server script
        int result = -1;
        if( Str::Substring( line, "Server" ) || Str::Substring( line, "Common" ) )
        {
            IsServer = true, IsClient = false, IsMapper = false;
            result = Compile( str_fname, str_prep, defines, run_func );
            if( result != 0 )
                return result;
        }

        // Compile as client script
        if( Str::Substring( line, "Client" ) || Str::Substring( line, "Common" ) )
        {
            IsServer = false, IsClient = true, IsMapper = false;
            result = Compile( str_fname, str_prep, defines, run_func );
            if( result != 0 )
                return result;
        }

        // Compile as mapper script
        if( Str::Substring( line, "Mapper" ) || Str::Substring( line, "Common" ) )
        {
            IsServer = false, IsClient = false, IsMapper = true;
            result = Compile( str_fname, str_prep, defines, run_func );
            if( result != 0 )
                return result;
        }

        // No one target seleted
        if( result == -1 )
        {
            printf( "Compile target (Server/Client/Mapper) not specified.\n" );
            return -1;
        }
    }
    else
    {
        return Compile( str_fname, str_prep, defines, run_func );
    }
    return 0;
}

int Compile( const char* fname, const char* fname_prep, vector< char* >& defines, vector< char* >& run_func )
{
    // Engine
    Engine = asCreateScriptEngine( ANGELSCRIPT_VERSION );
    if( !Engine )
    {
        printf( "Register failed.\n" );
        return -1;
    }
    Engine->SetMessageCallback( asFUNCTION( CallBack ), NULL, asCALL_CDECL );

    // Extensions
    RegisterScriptArray( Engine, true );
    RegisterScriptString( Engine );
    RegisterScriptAny( Engine );
    RegisterScriptDictionary( Engine );
    RegisterScriptDict( Engine );
    RegisterScriptFile( Engine );
    RegisterScriptMath( Engine );
    RegisterScriptWeakRef( Engine );
    RegisterScriptReflection( Engine );

    // Properties
    PropertyRegistrator* registrators[ 6 ] = { NULL, NULL, NULL, NULL, NULL, NULL };
    if( IsServer )
    {
        registrators[ 0 ] = new PropertyRegistrator( true, "GlobalVars" );
        registrators[ 1 ] = new PropertyRegistrator( true, "Critter" );
        registrators[ 2 ] = new PropertyRegistrator( true, "Item" );
        registrators[ 3 ] = new PropertyRegistrator( true, "ProtoItem" );
        registrators[ 4 ] = new PropertyRegistrator( true, "Map" );
        registrators[ 5 ] = new PropertyRegistrator( true, "Location" );
    }
    if( IsClient || IsMapper )
    {
        registrators[ 0 ] = new PropertyRegistrator( false, "GlobalVars" );
        registrators[ 1 ] = new PropertyRegistrator( false, "CritterCl" );
        registrators[ 2 ] = new PropertyRegistrator( false, "ItemCl" );
        registrators[ 3 ] = new PropertyRegistrator( false, "ProtoItem" );
        registrators[ 4 ] = new PropertyRegistrator( false, "Map" );
        registrators[ 5 ] = new PropertyRegistrator( false, "Location" );
    }

    // Bind
    int bind_errors = 0;
    if( IsServer )
        bind_errors = ServerBind::Bind( Engine, registrators );
    else if( IsClient )
        bind_errors = ClientBind::Bind( Engine, registrators );
    else if( IsMapper )
        bind_errors = MapperBind::Bind( Engine, registrators );
    if( bind_errors )
        printf( "Warning, bind result: %d.\n", bind_errors );

    // Start compilation
    printf( "Compiling '%s' as %s script...\n", fname, IsServer ? "server" : ( IsClient ? "client" : "mapper" ) );
    double tick = Timer::AccurateTick();

    // Preprocessor
    int pragma_type = PRAGMA_UNKNOWN;
    if( IsServer )
        pragma_type = PRAGMA_SERVER;
    else if( IsClient )
        pragma_type = PRAGMA_CLIENT;
    else if( IsMapper )
        pragma_type = PRAGMA_MAPPER;

    Preprocessor::SetPragmaCallback( new ScriptPragmaCallback( pragma_type, registrators ) );

    Preprocessor::UndefAll();

    char buf[ MAX_FOTEXT ];
    Preprocessor::Define( Str::Format( buf, "__VERSION %d", FONLINE_VERSION ) );

    Preprocessor::Define( "__ASCOMPILER" );
    if( IsServer )
        Preprocessor::Define( "__SERVER" );
    else if( IsClient )
        Preprocessor::Define( "__CLIENT" );
    else if( IsMapper )
        Preprocessor::Define( "__MAPPER" );
    for( size_t i = 0; i < defines.size(); i++ )
        Preprocessor::Define( string( defines[ i ] ) );

    Preprocessor::StringOutStream result, errors;
    int                           res;
    res = Preprocessor::Preprocess( fname, result, &errors, NULL, false );

    Buf = Str::Duplicate( errors.String.c_str() );

    if( res )
    {
        printf( "Unable to preprocess. Errors:\n%s\n", Buf );
        return -1;
    }
    else
    {
        if( Str::Length( Buf ) > 0 )
            printf( "%s", Buf );
    }

    if( fname_prep )
    {
        FILE* f = fopen( fname_prep, "wt" );
        if( f )
        {
            string result_formatted = result.String;
            FormatPreprocessorOutput( result_formatted );
            fwrite( result_formatted.c_str(), sizeof( char ), result_formatted.length(), f );
            fclose( f );
        }
        else
        {
            printf( "Unable to create preprocessed file<%s>.\n", fname_prep );
        }
    }

    // Break buffer into null-terminated lines
    for( int i = 0; Buf[ i ] != '\0'; i++ )
        if( Buf[ i ] == '\n' )
            Buf[ i ] = '\0';

    // Make module name
    char module_name[ MAX_FOTEXT ];
    FileManager::ExtractFileName( fname, module_name );
    FileManager::EraseExtension( module_name );

    // AS compilation
    asIScriptModule* module = Engine->GetModule( module_name, asGM_ALWAYS_CREATE );
    if( !module )
    {
        printf( "Can't create module.\n" );
        return -1;
    }

    if( module->AddScriptSection( module_name, result.String.c_str() ) < 0 )
    {
        printf( "Unable to add section.\n" );
        return -1;
    }

    if( module->Build() < 0 )
    {
        printf( "Unable to build.\n" );
        return -1;
    }

    // Check global not allowed types, only for server
    if( IsServer )
    {
        int bad_typeids[] =
        {
            Engine->GetTypeIdByDecl( "GlobalVars@" ),
            Engine->GetTypeIdByDecl( "GlobalVars@[]" ),
            Engine->GetTypeIdByDecl( "Critter@" ),
            Engine->GetTypeIdByDecl( "Critter@[]" ),
            Engine->GetTypeIdByDecl( "Item@" ),
            Engine->GetTypeIdByDecl( "Item@[]" ),
            Engine->GetTypeIdByDecl( "Map@" ),
            Engine->GetTypeIdByDecl( "Map@[]" ),
            Engine->GetTypeIdByDecl( "Location@" ),
            Engine->GetTypeIdByDecl( "Location@[]" ),
            Engine->GetTypeIdByDecl( "GameVar@" ),
            Engine->GetTypeIdByDecl( "GameVar@[]" ),
            Engine->GetTypeIdByDecl( "CraftItem@" ),
            Engine->GetTypeIdByDecl( "CraftItem@[]" ),
        };
        int bad_typeids_count = sizeof( bad_typeids ) / sizeof( int );
        for( int k = 0; k < bad_typeids_count; k++ )
            bad_typeids[ k ] &= asTYPEID_MASK_SEQNBR;

        vector< int > bad_typeids_class;
        for( int m = 0, n = module->GetObjectTypeCount(); m < n; m++ )
        {
            asIObjectType* ot = module->GetObjectTypeByIndex( m );
            for( int i = 0, j = ot->GetPropertyCount(); i < j; i++ )
            {
                int type = 0;
                ot->GetProperty( i, NULL, &type, NULL, NULL );
                type &= asTYPEID_MASK_SEQNBR;
                for( int k = 0; k < bad_typeids_count; k++ )
                {
                    if( type == bad_typeids[ k ] )
                    {
                        bad_typeids_class.push_back( ot->GetTypeId() & asTYPEID_MASK_SEQNBR );
                        break;
                    }
                }
            }
        }

        bool g_fail = false;
        bool g_fail_class = false;
        for( int i = 0, j = module->GetGlobalVarCount(); i < j; i++ )
        {
            int type = 0;
            module->GetGlobalVar( i, NULL, NULL, &type, NULL );

            while( type & asTYPEID_TEMPLATE )
            {
                asIObjectType* obj = (asIObjectType*) Engine->GetObjectTypeById( type );
                if( !obj )
                    break;
                type = obj->GetSubTypeId();
            }

            type &= asTYPEID_MASK_SEQNBR;

            for( int k = 0; k < bad_typeids_count; k++ )
            {
                if( type == bad_typeids[ k ] )
                {
                    const char* name = NULL;
                    module->GetGlobalVar( i, &name, NULL, NULL );
                    string      msg = "The global variable '" + string( name ) + "' uses a type that cannot be stored globally";
                    Engine->WriteMessage( "", 0, 0, asMSGTYPE_ERROR, msg.c_str() );
                    g_fail = true;
                    break;
                }
            }
            if( std::find( bad_typeids_class.begin(), bad_typeids_class.end(), type ) != bad_typeids_class.end() )
            {
                const char* name = NULL;
                module->GetGlobalVar( i, &name, NULL, NULL );
                string      msg = "The global variable '" + string( name ) + "' uses a type in class property that cannot be stored globally";
                Engine->WriteMessage( "", 0, 0, asMSGTYPE_ERROR, msg.c_str() );
                g_fail_class = true;
            }
        }

        if( g_fail || g_fail_class )
        {
            if( !g_fail_class )
                printf( "Erase global variable listed above.\n" );
            else
                printf( "Erase global variable or class property listed above.\n" );
            printf( "Classes that cannot be stored in global scope: Critter, Item, ProtoItem, Map, Location, GlobalVar.\n" );
            printf( "Hint: store their Ids, instead of pointers.\n" );
            return -1;
        }
    }

    // Print compilation time
    printf( "Success (%g ms).\n", Timer::AccurateTick() - tick );

    // Execute functions
    for( size_t i = 0; i < run_func.size(); i++ )
        RunMain( module, run_func[ i ] );

    // Collect garbage
    if( CollectGarbage )
        Engine->GarbageCollect( asGC_FULL_CYCLE );

    // Clean up
    SAFEDEL( registrators[ 0 ] );
    SAFEDEL( registrators[ 1 ] );
    SAFEDEL( registrators[ 2 ] );
    SAFEDEL( registrators[ 3 ] );
    SAFEDEL( registrators[ 4 ] );
    SAFEDEL( registrators[ 5 ] );
    Engine->Release();
    if( Buf )
        delete Buf;
    Buf = NULL;

    return 0;
}
