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

int Compile( const char* target, FileManager& file, const char* path, const char* path_prep, vector< const char* >& defines, vector< const char* >& run_func );

// Server
#define BIND_SERVER
#define BIND_CLASS    BindClass::
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line " # x ".\n" ); bind_errors++; }
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
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line " # x ".\n" ); bind_errors++; }
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
#define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line " # x ".\n" ); bind_errors++; }
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
                  "*can be used multiple times\n" );
        return 0;
    }

    // Parse args
    const char*           target = NULL;
    const char*           arg_path = argv[ 1 ];
    const char*           arg_path_prep = NULL;
    vector< const char* > defines;
    vector< const char* > run_func;
    for( int i = 2; i < argc; i++ )
    {
        // Server / Client / Mapper
        if( Str::CompareCase( argv[ i ], "-server" ) )
            target = "SERVER";
        else if( Str::CompareCase( argv[ i ], "-client" ) )
            target = "CLIENT";
        else if( Str::CompareCase( argv[ i ], "-mapper" ) )
            target = "MAPPER";
        // Preprocessor output
        else if( Str::CompareCase( argv[ i ], "-p" ) && i + 1 < argc )
            arg_path_prep = argv[ ++i ];
        // Define
        else if( Str::CompareCase( argv[ i ], "-d" ) && i + 1 < argc )
            defines.push_back( argv[ ++i ] );
        // Run function
        else if( Str::CompareCase( argv[ i ], "-run" ) && i + 1 < argc )
            run_func.push_back( argv[ ++i ] );
    }

    // Fix path
    char path[ MAX_FOTEXT ];
    char path_prep[ MAX_FOTEXT ];
    Str::Copy( path, arg_path );
    NormalizePathSlashes( path );
    if( arg_path_prep )
    {
        Str::Copy( path_prep, arg_path_prep );
        NormalizePathSlashes( path_prep );
    }
    else
    {
        path_prep[ 0 ] = 0;
    }

    // Set current directory
    char dir[ MAX_FOTEXT ];
    FileManager::ExtractDir( path, dir );
    if( ResolvePath( dir ) )
    {
        #ifdef FO_WINDOWS
        SetCurrentDirectory( dir );
        #else
        chdir( dir );
        #endif
    }

    FileManager file;
    if( !file.LoadFile( path, PT_ROOT ) )
    {
        WriteLog( "File '%s' not found.\n", path );
        return -1;
    }

    char line[ MAX_FOTEXT ];
    if( !file.GetLine( line, MAX_FOTEXT ) )
    {
        WriteLog( "File '%s' empty.\n", path );
        return -1;
    }

    if( !target )
    {
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
            result = Compile( "SERVER", file, path, path_prep, defines, run_func );
            if( result != 0 )
                return result;
        }

        // Compile as client script
        if( Str::Substring( line, "Client" ) || Str::Substring( line, "Common" ) )
        {
            result = Compile( "CLIENT", file, path, path_prep, defines, run_func );
            if( result != 0 )
                return result;
        }

        // Compile as mapper script
        if( Str::Substring( line, "Mapper" ) || Str::Substring( line, "Common" ) )
        {
            result = Compile( "MAPPER", file, path, path_prep, defines, run_func );
            if( result != 0 )
                return result;
        }

        // No one target selected
        if( result == -1 )
        {
            WriteLog( "Compile target (Server/Client/Mapper) not specified.\n" );
            return -1;
        }
    }
    else
    {
        return Compile( target, file, path, path_prep, defines, run_func );
    }
    return 0;
}

int Compile( const char* target, FileManager& file, const char* path, const char* fname_prep, vector< const char* >& defines, vector< const char* >& run_func )
{
    // Pragma callback
    int pragma_type = PRAGMA_UNKNOWN;
    if( Str::Compare( target, "SERVER" ) )
        pragma_type = PRAGMA_SERVER;
    else if( Str::Compare( target, "CLIENT" ) )
        pragma_type = PRAGMA_CLIENT;
    else if( Str::Compare( target, "MAPPER" ) )
        pragma_type = PRAGMA_MAPPER;

    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( pragma_type );
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();

    // Engine
    if( !Script::Init( pragma_callback, target, true, 0, false, false ) )
        return -1;
    asIScriptEngine* engine = Script::GetEngine();
    Script::SetLoadLibraryCompiler( true );

    // Bind
    int bind_errors = 0;
    if( Str::Compare( target, "SERVER" ) )
        bind_errors = ServerBind::Bind( engine, registrators );
    else if( Str::Compare( target, "CLIENT" ) )
        bind_errors = ClientBind::Bind( engine, registrators );
    else if( Str::Compare( target, "MAPPER" ) )
        bind_errors = MapperBind::Bind( engine, registrators );
    if( bind_errors )
        WriteLog( "Warning, bind result: %d.\n", bind_errors );

    // Get file name
    char file_name[ MAX_FOTEXT ];
    FileManager::ExtractFileName( path, file_name );

    // Make module name
    char module_name[ MAX_FOTEXT ];
    Str::Copy(  module_name, file_name );
    FileManager::EraseExtension( module_name );

    // Start compilation
    char target_lower[ MAX_FOTEXT ];
    Str::Copy( target_lower, target );
    Str::Lower( target_lower );
    WriteLog( "Compiling '%s' as %s script...\n", file_name, target_lower );
    double tick = Timer::AccurateTick();

    // Get dir
    char dir[ MAX_FOTEXT ];
    FileManager::ExtractDir( path, dir );

    // Preprocessor defines
    Preprocessor::UndefAll();

    char buf[ MAX_FOTEXT ];
    Preprocessor::Define( Str::Format( buf, "__VERSION %d", FONLINE_VERSION ) );

    Preprocessor::Define( "__ASCOMPILER" );
    char target_define[ MAX_FOTEXT ];
    Str::Copy( target_define, "__" );
    Str::Append( target_define, target );
    Preprocessor::Define( target_define );
    for( size_t i = 0; i < defines.size(); i++ )
        Preprocessor::Define( string( defines[ i ] ) );

    // Compile
    if( !Script::LoadModuleFromFile( module_name, file, dir, NULL ) )
        return -1;

    // Finish pragmas
    pragma_callback->Finish();
    if( pragma_callback->IsError() )
    {
        WriteLog( "Finish pragmas fail.\n" );
        return -1;
    }

    // Print compilation time
    WriteLog( "Success (%g ms).\n", Timer::AccurateTick() - tick );

    // Execute functions
    for( size_t i = 0; i < run_func.size(); i++ )
    {
        WriteLog( "Executing 'void %s()'.\n", run_func[ i ] );
        uint bind_id = Script::BindByModuleFuncName( module_name, run_func[ i ], "void %s()", true );
        if( bind_id && Script::PrepareContext( bind_id, _FUNC_, "ASCompiler" ) )
            Script::RunPrepared();
    }

    // Clean up
    Script::FinishEngine( engine );
    return 0;
}
