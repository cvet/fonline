#include "Common.h"
#include "Entity.h"
#include "Script.h"
#include "ScriptPragmas.h"
#include "ScriptInvoker.h"
#include "angelscript.h"
#include "preprocessor.h"
#include "scriptdict.h"
#include "reflection.h"
#include "AngelScript/sdk/add_on/scriptarray/scriptarray.h"
#include "AngelScript/sdk/add_on/scriptany/scriptany.h"
#include "AngelScript/sdk/add_on/scriptdictionary/scriptdictionary.h"
#include "AngelScript/sdk/add_on/scriptfile/scriptfile.h"
#include "AngelScript/sdk/add_on/scriptstdstring/scriptstdstring.h"
#include "AngelScript/sdk/add_on/scriptmath/scriptmath.h"
#include "AngelScript/sdk/add_on/weakref/weakref.h"
#include "AngelScript/sdk/add_on/scripthelper/scripthelper.h"

int Compile( string target, FileManager& file, string path, string path_prep, const StrVec& defines, const StrVec& run_func );

// Server
namespace ServerBind
{
    #undef BIND_SERVER
    #undef BIND_CLIENT
    #undef BIND_MAPPER
    #undef BIND_CLASS
    #undef BIND_ASSERT
    #undef BIND_DUMMY_DATA
    #define BIND_SERVER
    #define BIND_CLASS    BindClass::
    #define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line " # x ".\n" ); errors++; }
    #define BIND_DUMMY_DATA
    #include "ScriptBind.h"
}

// Client
namespace ClientBind
{
    #undef BIND_SERVER
    #undef BIND_CLIENT
    #undef BIND_MAPPER
    #undef BIND_CLASS
    #undef BIND_ASSERT
    #undef BIND_DUMMY_DATA
    #define BIND_CLIENT
    #define BIND_CLASS    BindClass::
    #define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line " # x ".\n" ); errors++; }
    #define BIND_DUMMY_DATA
    #include "ScriptBind.h"
}

// Mapper
namespace MapperBind
{
    #undef BIND_SERVER
    #undef BIND_CLIENT
    #undef BIND_MAPPER
    #undef BIND_CLASS
    #undef BIND_ASSERT
    #undef BIND_DUMMY_DATA
    #define BIND_MAPPER
    #define BIND_CLASS    BindClass::
    #define BIND_ASSERT( x )    if( ( x ) < 0 ) { WriteLog( "Bind error, line " # x ".\n" ); errors++; }
    #define BIND_DUMMY_DATA
    #include "ScriptBind.h"
}

int main( int argc, char* argv[] )
{
    InitialSetup( argc, argv );

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
    const char* target = nullptr;
    const char* arg_path = argv[ 1 ];
    const char* arg_path_prep = nullptr;
    StrVec      defines;
    StrVec      run_func;
    for( int i = 2; i < argc; i++ )
    {
        // Server / Client / Mapper
        if( argv[ i ] == "-server" )
            target = "SERVER";
        else if( argv[ i ] == "-client" )
            target = "CLIENT";
        else if( argv[ i ] == "-mapper" )
            target = "MAPPER";
        // Preprocessor output
        else if( argv[ i ] == "-p" && i + 1 < argc )
            arg_path_prep = argv[ ++i ];
        // Define
        else if( argv[ i ] == "-d" && i + 1 < argc )
            defines.push_back( argv[ ++i ] );
        // Run function
        else if( argv[ i ] == "-run" && i + 1 < argc )
            run_func.push_back( argv[ ++i ] );
    }

    // Paths
    string path = _str( arg_path ).normalizePathSlashes();
    string path_prep = ( arg_path_prep ? _str( arg_path_prep ).normalizePathSlashes() : "" );

    FileManager::SetCurrentDir( _str( path ).extractDir(), "./" );

    FileManager file;
    if( !file.LoadFile( path ) )
    {
        WriteLog( "File '{}' not found.\n", path );
        return -1;
    }

    string line = file.GetNonEmptyLine();
    if( line.empty() )
    {
        WriteLog( "File '{}' empty.\n", path );
        return -1;
    }

    if( !target )
    {
        // Check script signature
        if( line.find( "FOS" ) == string::npos )
        {
            WriteLog( "FOnline script signature 'FOS' not found in first line.\n" );
            return -1;
        }

        // Compile as server script
        int result = -1;
        if( line.find( "Server" ) != string::npos || line.find( "Common" ) != string::npos )
        {
            result = Compile( "SERVER", file, path, path_prep, defines, run_func );
            if( result != 0 )
                return result;
        }

        // Compile as client script
        if( line.find( "Client" ) != string::npos || line.find( "Common" ) != string::npos )
        {
            result = Compile( "CLIENT", file, path, path_prep, defines, run_func );
            if( result != 0 )
                return result;
        }

        // Compile as mapper script
        if( line.find( "Mapper" ) != string::npos || line.find( "Common" ) != string::npos )
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

int Compile( string target, FileManager& file, string path, string fname_prep, const StrVec& defines, const StrVec& run_func )
{
    // Pragma callback
    int pragma_type = PRAGMA_UNKNOWN;
    if( target == "SERVER" )
        pragma_type = PRAGMA_SERVER;
    else if( target == "CLIENT" )
        pragma_type = PRAGMA_CLIENT;
    else if( target == "MAPPER" )
        pragma_type = PRAGMA_MAPPER;

    ScriptPragmaCallback* pragma_callback = new ScriptPragmaCallback( pragma_type );
    PropertyRegistrator** registrators = pragma_callback->GetPropertyRegistrators();

    // Engine
    if( !Script::Init( pragma_callback, target.c_str(), true, 0, false, false ) )
        return -1;
    asIScriptEngine* engine = Script::GetEngine();
    Script::SetLoadLibraryCompiler( true );

    // Bind
    int bind_errors = 0;
    if( target == "SERVER" )
        bind_errors = ServerBind::Bind( engine, registrators );
    else if( target == "CLIENT" )
        bind_errors = ClientBind::Bind( engine, registrators );
    else if( target == "MAPPER" )
        bind_errors = MapperBind::Bind( engine, registrators );
    if( bind_errors )
        WriteLog( "Warning, bind result: {}.\n", bind_errors );

    // Get file name
    string file_name = _str( path ).extractFileName();

    // Make module name
    string module_name = _str( file_name ).eraseFileExtension();

    // Start compilation
    string target_lower = _str( target ).lower();
    WriteLog( "Compiling '{}' as {} script...\n", file_name, target_lower );
    double tick = Timer::AccurateTick();

    // Preprocessor defines
    Preprocessor::UndefAll();

    Preprocessor::Define( _str( "__VERSION {}", FONLINE_VERSION ) );

    Preprocessor::Define( "__ASCOMPILER" );
    string target_define = "__" + target;
    Preprocessor::Define( target_define );
    for( size_t i = 0; i < defines.size(); i++ )
        Preprocessor::Define( string( defines[ i ] ) );

    // Compile
    if( target == "SERVER" )
        target = "Server";
    else if( target == "CLIENT" )
        target = "Client";
    else if( target == "MAPPER" )
        target = "Mapper";

    if( !Script::ReloadScripts( target.c_str() ) )
        return -1;

    // Finish pragmas
    pragma_callback->Finish();
    if( pragma_callback->IsError() )
    {
        WriteLog( "Finish pragmas fail.\n" );
        return -1;
    }

    // Print compilation time
    WriteLog( "Success ({} ms).\n", Timer::AccurateTick() - tick );

    // Execute functions
    for( size_t i = 0; i < run_func.size(); i++ )
    {
        WriteLog( "Executing 'void {}()'.\n", run_func[ i ] );
        uint bind_id = Script::BindByFuncName( run_func[ i ], "void %s()", true );
        if( bind_id )
        {
            Script::PrepareContext( bind_id, "ASCompiler" );
            Script::RunPrepared();
        }
    }

    // Clean up
    Script::FinishEngine( engine );
    return 0;
}
