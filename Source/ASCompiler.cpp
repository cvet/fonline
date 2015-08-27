#include "Common.h"
#include "Entity.h"
#include "Script.h"
#include "ScriptPragmas.h"
#include "ScriptInvoker.h"
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

bool        IsServer = false;
bool        IsClient = false;
bool        IsMapper = false;
char*       Buf = NULL;
bool        CollectGarbage = false;

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

void PrintContextCallstack( asIScriptContext* ctx )
{
    int                      line;
    const asIScriptFunction* func;
    int                      stack_size = ctx->GetCallstackSize();
    WriteLog( "State '%s', call stack:\n", ContextStatesStr[ (int) ctx->GetState() ] );

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
        WriteLog( "  %d) %s : %s : %d.\n", stack_size - 1, Preprocessor::ResolveOriginalFile( line ), func->GetDeclaration(), Preprocessor::ResolveOriginalLine( line ) );

    // Print call stack
    for( int i = 1; i < stack_size; i++ )
    {
        func = ctx->GetFunction( i );
        line = ctx->GetLineNumber( i );
        if( func )
            WriteLog( "  %d) %s : %s : %d.\n", stack_size - i - 1, Preprocessor::ResolveOriginalFile( line ), func->GetDeclaration(), Preprocessor::ResolveOriginalLine( line ) );
    }
}

void RunMain( asIScriptModule* module, const char* func_str )
{
    // Make function declaration
    char func_decl[ 1024 ];
    sprintf( func_decl, "void %s()", func_str );

    // Run
    WriteLog( "Executing '%s'.\n", func_str );
    asIScriptContext*  ctx = module->GetEngine()->CreateContext();
    asIScriptFunction* func = module->GetFunctionByDecl( func_decl );
    if( !func )
    {
        WriteLog( "Function '%s' not found.\n", func_decl );
        return;
    }
    int result = ctx->Prepare( func );
    if( result < 0 )
    {
        WriteLog( "Context preparation failure, error code<%d>.\n", result );
        return;
    }

    result = ctx->Execute();
    asEContextState state = ctx->GetState();
    if( state != asEXECUTION_FINISHED )
    {
        if( state == asEXECUTION_EXCEPTION )
            WriteLog( "Execution of script stopped due to exception '%s'.\n", ctx->GetExceptionString() );
        else if( state == asEXECUTION_SUSPENDED )
            WriteLog( "Execution of script stopped due to timeout.\n" );
        else
            WriteLog( "Execution of script stopped due to '%s'.\n", ContextStatesStr[ (int) state ] );
        PrintContextCallstack( ctx );
        ctx->Abort();
        return;
    }

    if( result < 0 )
        WriteLog( "Execution error<%d>, state<%s>.\n", result, ContextStatesStr[ (int) state ] );
    WriteLog( "Execution finished.\n" );
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
            WriteLog( "%s(%d) : %s : %s.\n", Preprocessor::ResolveOriginalFile( msg->row ), Preprocessor::ResolveOriginalLine( msg->row ), type, msg->message );
        }
        else
        {
            WriteLog( "%s : %s.\n", type, msg->message );
        }
    }
    else
    {
        WriteLog( "%s(%d) : %s : %s.\n", Preprocessor::ResolveOriginalFile( msg->row ), Preprocessor::ResolveOriginalLine( msg->row ), type, msg->message );
    }
}

// Server
#define BIND_SERVER
#define BIND_CLASS    BindClass::
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line<" # x ">.\n" ); bind_errors++; }
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
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line<" # x ">.\n" ); bind_errors++; }
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
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line<" # x ">.\n" ); bind_errors++; }
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
        WriteLog( "FOnline AngleScript compiler. Usage:\n"
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
            WriteLog( "File<%s> not found.\n", str_fname );
            return -1;
        }

        char line[ MAX_FOTEXT ];
        if( !fgets( line, sizeof( line ), f ) )
        {
            WriteLog( "File<%s> empty.\n", str_fname );
            return -1;
        }
        fclose( f );

        // Check script signature
        if( !Str::Substring( line, "FOS" ) )
        {
            WriteLog( "FOnline script signature 'FOS' not found in first line.\n" );
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
            WriteLog( "Compile target (Server/Client/Mapper) not specified.\n" );
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
    // Pragma callback
    int pragma_type = PRAGMA_UNKNOWN;
    if( IsServer )
        pragma_type = PRAGMA_SERVER;
    else if( IsClient )
        pragma_type = PRAGMA_CLIENT;
    else if( IsMapper )
        pragma_type = PRAGMA_MAPPER;

    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( pragma_type );
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();

    // Engine
    if( !Script::Init( pragma_callback, IsServer ? "SERVER" : ( IsClient ? "CLIENT" : "MAPPER" ), true, 0, false, false ) )
        return -1;
    asIScriptEngine* engine = Script::GetEngine();
    engine->SetMessageCallback( asFUNCTION( CallBack ), NULL, asCALL_CDECL );
    Script::SetLoadLibraryCompiler( true );

    // Bind
    int bind_errors = 0;
    if( IsServer )
        bind_errors = ServerBind::Bind( engine, registrators );
    else if( IsClient )
        bind_errors = ClientBind::Bind( engine, registrators );
    else if( IsMapper )
        bind_errors = MapperBind::Bind( engine, registrators );
    if( bind_errors )
        WriteLog( "Warning, bind result: %d.\n", bind_errors );

    // Start compilation
    WriteLog( "Compiling '%s' as %s script...\n", fname, IsServer ? "server" : ( IsClient ? "client" : "mapper" ) );
    double tick = Timer::AccurateTick();

    // Preprocessor
    Preprocessor::SetPragmaCallback( pragma_callback );
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
        WriteLog( "Unable to preprocess. Errors:\n%s\n", Buf );
        return -1;
    }
    else
    {
        if( Str::Length( Buf ) > 0 )
            WriteLog( "%s", Buf );
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
            WriteLog( "Unable to create preprocessed file<%s>.\n", fname_prep );
        }
    }

    // Break buffer into null-terminated lines
    for( int i = 0; Buf[ i ] != '\0'; i++ )
        if( Buf[ i ] == '\n' )
            Buf[ i ] = '\0';

    // Finish pragmas
    pragma_callback->Finish();
    if( pragma_callback->IsError() )
    {
        WriteLog( "Finish pragmas fail.\n" );
        return -1;
    }

    // Make module name
    char module_name[ MAX_FOTEXT ];
    FileManager::ExtractFileName( fname, module_name );
    FileManager::EraseExtension( module_name );

    // AS compilation
    asIScriptModule* module = engine->GetModule( module_name, asGM_ALWAYS_CREATE );
    if( !module )
    {
        WriteLog( "Can't create module.\n" );
        return -1;
    }

    if( module->AddScriptSection( module_name, result.String.c_str() ) < 0 )
    {
        WriteLog( "Unable to add section.\n" );
        return -1;
    }

    if( module->Build() < 0 )
    {
        WriteLog( "Unable to build.\n" );
        return -1;
    }

    // Print compilation time
    WriteLog( "Success (%g ms).\n", Timer::AccurateTick() - tick );

    // Execute functions
    for( size_t i = 0; i < run_func.size(); i++ )
        RunMain( module, run_func[ i ] );

    // Clean up
    Script::FinishEngine( engine );
    SAFEDEL( Buf );
    return 0;
}
