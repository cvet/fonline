#include "ScriptPragmas.h"
#include "AngelScript/angelscript.h"

#ifdef FONLINE_SCRIPT_COMPILER
# include "PlatformSpecific.h"
# include "Defines.h"
# include "Types.h"
# include "../ASCompiler/ScriptEngine.h"
# include "AngelScript/scriptstring.h"
# include <stdio.h>
# include <strstream>

# if defined ( FO_WINDOWS )
#  include <Windows.h>
# endif

// DLL
# if defined ( FO_WINDOWS )
static char DLLTempBuf[ 64 ];
#  define DLL_Load( name )              (void*) LoadLibrary( name )
#  define DLL_Free( h )                 FreeLibrary( (HMODULE) h )
#  define DLL_GetAddress( h, pname )    (size_t*) GetProcAddress( (HMODULE) h, pname )
#  define DLL_Error()                   _itoa( GetLastError(), DLLTempBuf, 10 )
# else // FO_LINUX
#  include <dlfcn.h>
#  define DLL_Load( name )              (void*) dlopen( name, RTLD_NOW | RTLD_LOCAL )
#  define DLL_Free( h )                 dlclose( h )
#  define DLL_GetAddress( h, pname )    (size_t*) dlsym( h, pname )
#  define DLL_Error()                   dlerror()
# endif

namespace Script
{
    asIScriptEngine* GetEngine()
    {
        return (asIScriptEngine*) GetScriptEngine();
    }

    void* LoadDynamicLibrary( const char* dll_name )
    {
        char dll_name_[ MAX_FOPATH ];
        strcpy( dll_name_, dll_name );

        // Erase extension
        char* str = dll_name_;
        char* last_dot = NULL;
        for( ; *str; str++ )
            if( *str == '.' )
                last_dot = str;
        if( !last_dot || !last_dot[ 1 ] )
            return NULL;
        *last_dot = 0;

        # if defined ( FO_X64 )
        // Add '64' appendix
        strcat( dll_name_, "64" );
        # endif

        // DLL extension
        # if defined ( FO_WINDOWS )
        strcat( dll_name_, ".dll" );
        # else // FO_LINUX
        strcat( dll_name_, ".so" );
        # endif

        // Register global function and vars
        static map< string, void* > alreadyLoadedDll;
        string                      dll_name_str = dll_name_;
        auto                        it = alreadyLoadedDll.find( dll_name_str );
        if( it != alreadyLoadedDll.end() )
            return ( *it ).second;
        alreadyLoadedDll.insert( PAIR( dll_name_str, (void*) NULL ) );

        // Load dynamic library
        void* dll = DLL_Load( dll_name_str.c_str() );
        if( !dll )
            return NULL;

        // Verify compilation target
        const char* target = GetDllTarget();
        size_t*     ptr = DLL_GetAddress( dll, target );
        if( !ptr )
        {
            printf( "Wrong script DLL<%s>, expected target<%s>, but found<%s%s%s%s>.\n", dll_name, target,
                    DLL_GetAddress( dll, "SERVER" ) ? "SERVER" : "", DLL_GetAddress( dll, "CLIENT" ) ? "CLIENT" : "", DLL_GetAddress( dll, "MAPPER" ) ? "MAPPER" : "",
                    !DLL_GetAddress( dll, "SERVER" ) && !DLL_GetAddress( dll, "CLIENT" ) && !DLL_GetAddress( dll, "MAPPER" ) ? "Nothing" : "" );
            DLL_Free( dll );
            return NULL;
        }

        // Register variables
        ptr = DLL_GetAddress( dll, "FOnline" );
        if( ptr )
            *ptr = (size_t) NULL;
        ptr = DLL_GetAddress( dll, "ASEngine" );
        if( ptr )
            *ptr = (size_t) GetEngine();

        // Register functions
        ptr = DLL_GetAddress( dll, "Log" );
        if( ptr )
            *ptr = (size_t) &printf;

        // Call init function
        typedef void ( *DllMainEx )( bool );
        DllMainEx func = (DllMainEx) DLL_GetAddress( dll, "DllMainEx" );
        if( func )
            ( *func )( true );

        alreadyLoadedDll[ dll_name_str ] = dll;
        return dll;
    }
}

# define WriteLog    printf
# define int64       __int64

#else

# include "Script.h"

# ifdef FONLINE_SERVER
#  include "Critter.h"
# else
#  include "CritterCl.h"
# endif
# include "Item.h"

#endif


// #pragma globalvar "int __MyGlobalVar = 100"
class GlobalVarPragma
{
private:
    list< int >           intArray;
    list< int64 >         int64Array;
    list< ScriptString* > stringArray;
    list< float >         floatArray;
    list< double >        doubleArray;
    list< char >          boolArray;

public:
    void Call( const string& text )
    {
        asIScriptEngine* engine = Script::GetEngine();
        string           type, decl, value;
        char             ch = 0;
        istrstream       str( text.c_str() );
        str >> type >> decl >> ch >> value;

        if( decl == "" )
        {
            WriteLog( "Global var name not found, pragma<%s>.\n", text.c_str() );
            return;
        }

        int    int_value = ( ch == '=' ? atoi( value.c_str() ) : 0 );
        double float_value = ( ch == '=' ? atof( value.c_str() ) : 0.0 );
        string name = type + " " + decl;

        // Register
        if( type == "int8" || type == "int16" || type == "int32" || type == "int" || type == "uint8" || type == "uint16" || type == "uint32" || type == "uint" )
        {
            auto it = intArray.insert( intArray.begin(), int_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 ) WriteLog( "Unable to register integer global var, pragma<%s>.\n", text.c_str() );
        }
        else if( type == "int64" || type == "uint64" )
        {
            auto it = int64Array.insert( int64Array.begin(), int_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 ) WriteLog( "Unable to register integer64 global var, pragma<%s>.\n", text.c_str() );
        }
        else if( type == "string" )
        {
            if( value != "" ) value = text.substr( text.find( value ), string::npos );
            auto it = stringArray.insert( stringArray.begin(), new ScriptString( value ) );
            if( engine->RegisterGlobalProperty( name.c_str(), ( *it ) ) < 0 ) WriteLog( "Unable to register string global var, pragma<%s>.\n", text.c_str() );
        }
        else if( type == "float" )
        {
            auto it = floatArray.insert( floatArray.begin(), (float) float_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 ) WriteLog( "Unable to register float global var, pragma<%s>.\n", text.c_str() );
        }
        else if( type == "double" )
        {
            auto it = doubleArray.insert( doubleArray.begin(), float_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 ) WriteLog( "Unable to register double global var, pragma<%s>.\n", text.c_str() );
        }
        else if( type == "bool" )
        {
            value = ( ch == '=' ? value : "false" );
            if( value != "true" && value != "false" )
            {
                WriteLog( "Invalid start value of boolean type, pragma<%s>.\n", text.c_str() );
                return;
            }
            auto it = boolArray.insert( boolArray.begin(), value == "true" ? true : false );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 ) WriteLog( "Unable to register boolean global var, pragma<%s>.\n", text.c_str() );
        }
        else
        {
            WriteLog( "Global var not registered, unknown type, pragma<%s>.\n", text.c_str() );
        }
    }
};

// #pragma crdata "Stat 0 199"
class CrDataPragma
{
private:
    int           pragmaType;
    set< string > parametersAlready;
    uint          parametersIndex;

public:
    CrDataPragma( int pragma_type ): pragmaType( pragma_type ), parametersIndex( 1 /*0 is ParamBase*/ ) {}

    void Call( const string& text )
    {
        if( pragmaType == PRAGMA_SERVER )
            RegisterCrData( text.c_str() );
        else if( pragmaType == PRAGMA_CLIENT )
            RegisterCrClData( text.c_str() );
    }

    bool RegisterCrData( const char* text )
    {
        asIScriptEngine* engine = Script::GetEngine();
        string           name;
        uint             min, max;
        istrstream       str( text );
        str >> name >> min >> max;
        if( str.fail() ) return false;
        if( min > max || max >= MAX_PARAMS ) return false;
        if( parametersAlready.count( name ) ) return true;
        if( parametersIndex >= MAX_PARAMETERS_ARRAYS ) return false;

        char decl_val[ 128 ];
        sprintf( decl_val, "DataVal %s", name.c_str() );
        char decl_ref[ 128 ];
        sprintf( decl_ref, "DataRef %sBase", name.c_str() );

        #ifdef FONLINE_SERVER
        // Real registration
        if( engine->RegisterObjectProperty( "Critter", decl_val, OFFSETOF( Critter, ThisPtr[ 0 ] ) + sizeof( void* ) * parametersIndex ) < 0 ) return false;
        if( engine->RegisterObjectProperty( "Critter", decl_ref, OFFSETOF( Critter, ThisPtr[ 0 ] ) + sizeof( void* ) * parametersIndex ) < 0 ) return false;
        Critter::ParametersMin[ parametersIndex ] = min;
        Critter::ParametersMax[ parametersIndex ] = max;
        Critter::ParametersOffset[ parametersIndex ] = ( Str::Substring( text, "+" ) != NULL );
        #else
        // Fake registration
        if( engine->RegisterObjectProperty( "Critter", decl_val, 10000 + parametersIndex * 4 ) < 0 ) return false;
        if( engine->RegisterObjectProperty( "Critter", decl_ref, 10000 + parametersIndex * 4 ) < 0 ) return false;
        #endif

        parametersIndex++;
        parametersAlready.insert( name );
        return true;
    }

    bool RegisterCrClData( const char* text )
    {
        asIScriptEngine* engine = Script::GetEngine();
        string           name;
        uint             min, max;
        istrstream       str( text );
        str >> name >> min >> max;
        if( str.fail() ) return false;
        if( min > max || max >= MAX_PARAMS ) return false;
        if( parametersAlready.count( name ) ) return true;
        if( parametersIndex >= MAX_PARAMETERS_ARRAYS ) return false;

        char decl_val[ 128 ];
        sprintf( decl_val, "DataVal %s", name.c_str() );
        char decl_ref[ 128 ];
        sprintf( decl_ref, "DataRef %sBase", name.c_str() );

        #ifdef FONLINE_CLIENT
        // Real registration
        if( engine->RegisterObjectProperty( "CritterCl", decl_val, OFFSETOF( CritterCl, ThisPtr[ 0 ] ) + sizeof( void* ) * parametersIndex ) < 0 ) return false;
        if( engine->RegisterObjectProperty( "CritterCl", decl_ref, OFFSETOF( CritterCl, ThisPtr[ 0 ] ) + sizeof( void* ) * parametersIndex ) < 0 ) return false;
        CritterCl::ParametersMin[ parametersIndex ] = min;
        CritterCl::ParametersMax[ parametersIndex ] = max;
        CritterCl::ParametersOffset[ parametersIndex ] = ( Str::Substring( text, "+" ) != NULL );
        #else
        // Fake registration
        if( engine->RegisterObjectProperty( "CritterCl", decl_val, 10000 + parametersIndex * 4 ) < 0 ) return false;
        if( engine->RegisterObjectProperty( "CritterCl", decl_ref, 10000 + parametersIndex * 4 ) < 0 ) return false;
        #endif

        parametersIndex++;
        parametersAlready.insert( name );
        return true;
    }
};

// #pragma bindfunc "int MyObject::MyMethod(int, uint) -> my.dll MyDllFunc"
class BindFuncPragma
{
public:
    void Call( const string& text )
    {
        asIScriptEngine* engine = Script::GetEngine();
        string           func_name, dll_name, func_dll_name;
        istrstream       str( text.c_str() );
        while( true )
        {
            string s;
            str >> s;
            if( str.fail() || s == "->" ) break;
            if( func_name != "" ) func_name += " ";
            func_name += s;
        }
        str >> dll_name;
        str >> func_dll_name;

        if( str.fail() )
        {
            WriteLog( "Error in 'bindfunc' pragma<%s>, parse fail.\n", text.c_str() );
            return;
        }

        void* dll = Script::LoadDynamicLibrary( dll_name.c_str() );
        if( !dll )
        {
            WriteLog( "Error in 'bindfunc' pragma<%s>, dll not found, error<%s>.\n", text.c_str(), DLL_Error() );
            return;
        }

        // Find function
        size_t* func = DLL_GetAddress( dll, func_dll_name.c_str() );
        if( !func )
        {
            WriteLog( "Error in 'bindfunc' pragma<%s>, function not found, error<%s>.\n", text.c_str(), DLL_Error() );
            return;
        }

        int result = 0;
        if( func_name.find( "::" ) == string::npos )
        {
            // Register global function
            result = engine->RegisterGlobalFunction( func_name.c_str(), asFUNCTION( func ), asCALL_CDECL );
        }
        else
        {
            // Register class method
            string::size_type i = func_name.find( " " );
            string::size_type j = func_name.find( "::" );
            if( i == string::npos || i + 1 >= j )
            {
                WriteLog( "Error in 'bindfunc' pragma<%s>, parse class name fail.\n", text.c_str() );
                return;
            }
            i++;
            string class_name;
            class_name.assign( func_name, i, j - i );
            func_name.erase( i, j - i + 2 );
            result = engine->RegisterObjectMethod( class_name.c_str(), func_name.c_str(), asFUNCTION( func ), asCALL_CDECL_OBJFIRST );
        }
        if( result < 0 ) WriteLog( "Error in 'bindfunc' pragma<%s>, script registration fail, error<%d>.\n", text.c_str(), result );
    }
};

// #pragma bindfield "[const] int MyObject::Field -> offset"
class BindFieldPragma
{
private:
    vector< string >         className;
    vector< vector< bool > > busyBytes;
    vector< int >            baseOffset;
    vector< int >            dataSize;

public:
    void Add( const string& class_name, int base_offset, int data_size )
    {
        className.push_back( class_name );
        busyBytes.push_back( vector< bool >() );
        baseOffset.push_back( base_offset );
        dataSize.push_back( data_size );
    }

    void Call( const string& text )
    {
        asIScriptEngine* engine = Script::GetEngine();
        string           s, type_name, field_name, class_name;
        int              offset = 0;
        istrstream       str( text.c_str() );

        str >> s;
        type_name += s + " ";
        if( s == "const" )
        {
            str >> s;
            type_name += s + " ";
        }

        str >> s;
        size_t ns = s.find( "::" );
        if( ns == string::npos )
        {
            WriteLog( "Error in 'bindfield' pragma<%s>, '::' not found.\n", text.c_str() );
            return;
        }
        field_name.assign( s, ns + 2, s.size() - ns - 2 );
        class_name.assign( s, 0, ns );

        str >> s >> offset;
        if( s != "->" || str.fail() )
        {
            WriteLog( "Error in 'bindfield' pragma<%s>, offset parse fail.\n", text.c_str() );
            return;
        }

        // Get class data
        bool            founded = false;
        vector< bool >* busy_bytes;
        int             base_offset;
        int             data_size;
        for( int i = 0; i < (int) className.size(); i++ )
        {
            if( class_name == className[ i ] )
            {
                busy_bytes = &busyBytes[ i ];
                base_offset = baseOffset[ i ];
                data_size = dataSize[ i ];
                founded = true;
                break;
            }
        }
        if( !founded )
        {
            WriteLog( "Error in 'bindfield' pragma<%s>, unknown class name<%s>.\n", text.c_str(), class_name.c_str() );
            return;
        }

        // Check primitive
        int size = engine->GetSizeOfPrimitiveType( engine->GetTypeIdByDecl( type_name.c_str() ) );
        if( size <= 0 )
        {
            WriteLog( "Error in 'bindfield' pragma<%s>, wrong type<%s>.\n", text.c_str(), type_name.c_str() );
            return;
        }

        // Check offset
        if( offset < 0 || offset + size >= data_size )
        {
            WriteLog( "Error in 'bindfield' pragma<%s>, wrong offset<%d> data.\n", text.c_str(), offset );
            return;
        }

        // Check for already binded on this position
        if( (int) busy_bytes->size() < offset + size ) busy_bytes->resize( offset + size );
        bool busy = false;
        for( int i = offset; i < offset + size; i++ )
        {
            if( ( *busy_bytes )[ i ] )
            {
                busy = true;
                break;
            }
        }
        if( busy )
        {
            WriteLog( "Error in 'bindfield' pragma<%s>, data bytes<%d..%d> already in use.\n", text.c_str(), offset, offset + size - 1 );
            return;
        }

        // Check name for already used
        for( int i = 0, ii = engine->GetObjectTypeCount(); i < ii; i++ )
        {
            asIObjectType* ot = engine->GetObjectTypeByIndex( i );
            if( !strcmp( ot->GetName(), class_name.c_str() ) )
            {
                for( int j = 0, jj = ot->GetPropertyCount(); j < jj; j++ )
                {
                    const char* name;
                    ot->GetProperty( j, &name );
                    if( !strcmp( name, field_name.c_str() ) )
                    {
                        WriteLog( "Error in 'bindfield' pragma<%s>, property<%s> already available.\n", text.c_str(), name );
                        return;
                    }
                }
            }
        }

        // Bind
        int result = engine->RegisterObjectProperty( class_name.c_str(), ( type_name + field_name ).c_str(), base_offset + offset );
        if( result < 0 )
        {
            WriteLog( "Error in 'bindfield' pragma<%s>, register object property fail, error<%d>.\n", text.c_str(), result );
            return;
        }
        for( int i = offset; i < offset + size; i++ ) ( *busy_bytes )[ i ] = true;
    }
};

ScriptPragmaCallback::ScriptPragmaCallback( int pragma_type )
{
    pragmaType = pragma_type;
    if( pragmaType != PRAGMA_SERVER && pragmaType != PRAGMA_CLIENT && pragmaType != PRAGMA_MAPPER )
        pragmaType = PRAGMA_UNKNOWN;

    if( pragmaType != PRAGMA_UNKNOWN )
    {
        globalVarPragma = new GlobalVarPragma();
        crDataPragma = new CrDataPragma( pragmaType );
        bindFuncPragma = new BindFuncPragma();
        bindFieldPragma = new BindFieldPragma();

        #ifdef FONLINE_SCRIPT_COMPILER
        bindFieldPragma->Add( "ProtoItem", 0, PROTO_ITEM_USER_DATA_SIZE );
        bindFieldPragma->Add( "Critter", 0, CRITTER_USER_DATA_SIZE );
        #else
        bindFieldPragma->Add( "ProtoItem", OFFSETOF( ProtoItem, UserData ), PROTO_ITEM_USER_DATA_SIZE );
        # ifdef FONLINE_SERVER
        bindFieldPragma->Add( "Critter", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, UserData ), CRITTER_USER_DATA_SIZE );
        # endif
        #endif
    }
}

void ScriptPragmaCallback::CallPragma( const string& name, const Preprocessor::PragmaInstance& instance )
{
    if( alreadyProcessed.count( name + instance.text ) )
        return;
    alreadyProcessed.insert( name + instance.text );

    if( name == "globalvar" && globalVarPragma )
        globalVarPragma->Call( instance.text );
    else if( name == "crdata" && crDataPragma )
        crDataPragma->Call( instance.text );
    else if( name == "bindfunc" && bindFuncPragma )
        bindFuncPragma->Call( instance.text );
    else if( name == "bindfield" && bindFieldPragma )
        bindFieldPragma->Call( instance.text );
    else
        WriteLog( "Unknown pragma instance, name<%s> text<%s>.\n", name.c_str(), instance.text.c_str() );
}
