#include "Common.h"
#include "ScriptPragmas.h"
#include "angelscript.h"
#include "ConstantsManager.h"

#ifdef FONLINE_SCRIPT_COMPILER
# include "ScriptEngine.h"

namespace Script
{
    asIScriptEngine* GetEngine()
    {
        return (asIScriptEngine*) GetScriptEngine();
    }

    void* LoadDynamicLibrary( const char* dll_name )
    {
        char dll_name_[ MAX_FOPATH ] = { 0 };
        # ifndef FO_WINDOWS
        strcat( dll_name_, "./" );
        # endif
        strcat( dll_name_, dll_name );

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
        # ifdef FO_WINDOWS
        strcat( dll_name_, ".dll" );
        # else
        strcat( dll_name_, ".so" );
        # endif

        // Register global function and vars
        static map< string, void* > alreadyLoadedDll;
        string                      dll_name_str = dll_name_;
        # ifdef FO_WINDOWS
        for( uint i = 0, j = dll_name_str.length(); i < j; i++ )
            tolower( dll_name_str[ i ] );
        # endif
        auto it = alreadyLoadedDll.find( dll_name_str );
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
            ( *func )( true );

        alreadyLoadedDll[ dll_name_str ] = dll;
        return dll;
    }
}

# define WriteLog    printf

#else

# include "Script.h"

# ifdef FONLINE_SERVER
#  include "Critter.h"
# else
#  include "CritterCl.h"
# endif
# include "Item.h"

#endif

// #pragma ignore "other_pragma"
// #pragma other_pragma "arguments" <- not processed
class IgnorePragma
{
private:
    vector< string > ignoredPragmas;

public:
    bool IsIgnored( const string& text )
    {
        return std::find( ignoredPragmas.begin(), ignoredPragmas.end(), text ) != ignoredPragmas.end();
    }

    void Call( const string& text )
    {
        if( text.length() > 0 )
            ignoredPragmas.push_back( text );
    }
};

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
            auto it = stringArray.insert( stringArray.begin(), ScriptString::Create( value ) );
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
        if( result < 0 )
            WriteLog( "Error in 'bindfunc' pragma<%s>, script registration fail, error<%d>.\n", text.c_str(), result );
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
        string           s, type_name, property_name, class_name;
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
        property_name.assign( s, ns + 2, s.size() - ns - 2 );
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
                    if( !strcmp( name, property_name.c_str() ) )
                    {
                        WriteLog( "Error in 'bindfield' pragma<%s>, property<%s> already available.\n", text.c_str(), name );
                        return;
                    }
                }
            }
        }

        // Bind
        int result = engine->RegisterObjectProperty( class_name.c_str(), ( type_name + property_name ).c_str(), base_offset + offset );
        if( result < 0 )
        {
            WriteLog( "Error in 'bindfield' pragma<%s>, register object property fail, error<%d>.\n", text.c_str(), result );
            return;
        }
        for( int i = offset; i < offset + size; i++ ) ( *busy_bytes )[ i ] = true;
    }
};

// #pragma property "MyObject Virtual int PropName Default = 100 Min = 10 Max = 50 Random = true"
class PropertyPragma
{
private:
    PropertyRegistrator** propertyRegistrators;

public:
    PropertyPragma( PropertyRegistrator** property_registrators )
    {
        propertyRegistrators = property_registrators;
    }

    void Call( const string& text )
    {
        // Read all
        istrstream str( text.c_str() );
        string     class_name, access_str, property_type_name, property_name;
        str >> class_name >> access_str >> property_type_name >> property_name;
        if( str.fail() )
        {
            WriteLog( "Error in 'property' pragma<%s>.\n", text.c_str() );
            return;
        }
		char options_buf[ MAX_FOTEXT ] = { 0 };
		str.getline( options_buf, MAX_FOTEXT );

        // Parse access type
        Property::AccessType access = (Property::AccessType) 0;
        if( Str::CompareCase( access_str.c_str(), "Virtual" ) )
            access = Property::Virtual;
        else if( Str::CompareCase( access_str.c_str(), "VirtualClient" ) )
            access = Property::VirtualClient;
        else if( Str::CompareCase( access_str.c_str(), "VirtualServer" ) )
            access = Property::VirtualServer;
        else if( Str::CompareCase( access_str.c_str(), "Private" ) )
            access = Property::Private;
        else if( Str::CompareCase( access_str.c_str(), "PrivateClient" ) )
            access = Property::PrivateClient;
        else if( Str::CompareCase( access_str.c_str(), "PrivateServer" ) )
            access = Property::PrivateServer;
        else if( Str::CompareCase( access_str.c_str(), "Public" ) )
            access = Property::Public;
        else if( Str::CompareCase( access_str.c_str(), "PublicModifiable" ) )
            access = Property::PublicModifiable;
        else if( Str::CompareCase( access_str.c_str(), "Protected" ) )
            access = Property::Protected;
        else if( Str::CompareCase( access_str.c_str(), "ProtectedModifiable" ) )
            access = Property::ProtectedModifiable;
        if( access == (Property::AccessType) 0 )
        {
            WriteLog( "Error in 'property' pragma<%s>, invalid access type<%s>.\n", text.c_str(), access_str.c_str() );
            return;
        }

        // Parse options
        StrInt64Map options_map;
        StrVec      opt_entries;
        Str::ParseLine( options_buf, ',', opt_entries, Str::ParseLineDummy );
        for( size_t i = 0, j = opt_entries.size(); i < j; i++ )
        {
            StrVec opt_entry;
            Str::ParseLine( opt_entries[ i ].c_str(), '=', opt_entry, Str::ParseLineDummy );
            if( opt_entry.size() != 2 )
            {
                WriteLog( "Error in 'property' pragma<%s>, invalid options entry.\n", text.c_str() );
                return;
            }
            int64 value = ConvertParamValue( opt_entry[ 1 ].c_str() );
            options_map.insert( PAIR( opt_entry[ 0 ], value ) );
        }

        // Affect options
        bool  generate_random_value = false;
        bool  set_default_value = false;
        int64 default_value = 0;
        bool  check_min_value = false;
        int64 min_value = 0;
        bool  check_max_value = false;
        int64 max_value = 0;
        for( auto it = options_map.begin(); it != options_map.end(); ++it )
        {
            const char* opt_name = ( *it ).first.c_str();
            int64       opt_value = ( *it ).second;
            if( Str::CompareCase( opt_name, "default" ) )
            {
                set_default_value = true;
                default_value = opt_value;
            }
            else if( Str::CompareCase( opt_name, "min" ) )
            {
                check_min_value = true;
                min_value = opt_value;
            }
            else if( Str::CompareCase( opt_name, "max" ) )
            {
                check_max_value = true;
                max_value = opt_value;
            }
            else if( Str::CompareCase( opt_name, "random" ) )
            {
                generate_random_value = ( opt_value != 0 );
            }
        }

        // Register
        PropertyRegistrator* registrator = NULL;
        if( class_name == "Item" )
            registrator = propertyRegistrators[ 0 ];
        else
            WriteLog( "Invalid class in 'property' pragma<%s>.\n", text.c_str() );

        if( registrator )
        {
            registrator->Register( property_type_name.c_str(), property_name.c_str(), access,
                                   generate_random_value, set_default_value ? &default_value : NULL,
                                   check_min_value ? &min_value : NULL, check_max_value ? &max_value : NULL );
        }
    }
};

ScriptPragmaCallback::ScriptPragmaCallback( int pragma_type, PropertyRegistrator** properties_registrators )
{
    pragmaType = pragma_type;
    if( pragmaType != PRAGMA_SERVER && pragmaType != PRAGMA_CLIENT && pragmaType != PRAGMA_MAPPER )
        pragmaType = PRAGMA_UNKNOWN;

    if( pragmaType != PRAGMA_UNKNOWN )
    {
        ignorePragma = new IgnorePragma();
        globalVarPragma = new GlobalVarPragma();
        crDataPragma = new CrDataPragma( pragmaType );
        bindFuncPragma = new BindFuncPragma();
        bindFieldPragma = new BindFieldPragma();
        propertyPragma = new PropertyPragma( properties_registrators );

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
    if( alreadyProcessed.count( name + instance.Text ) )
        return;
    alreadyProcessed.insert( name + instance.Text );

    if( ignorePragma && ignorePragma->IsIgnored( name ) )
        return;

    if( Str::CompareCase( name.c_str(), "ignore" ) && ignorePragma )
        ignorePragma->Call( instance.Text );
    else if( Str::CompareCase( name.c_str(), "globalvar" ) && globalVarPragma )
        globalVarPragma->Call( instance.Text );
    else if( Str::CompareCase( name.c_str(), "crdata" ) && crDataPragma )
        crDataPragma->Call( instance.Text );
    else if( Str::CompareCase( name.c_str(), "bindfunc" ) && bindFuncPragma )
        bindFuncPragma->Call( instance.Text );
    else if( Str::CompareCase( name.c_str(), "bindfield" ) && bindFieldPragma )
        bindFieldPragma->Call( instance.Text );
    else if( Str::CompareCase( name.c_str(), "property" ) && propertyPragma )
        propertyPragma->Call( instance.Text );
    else
        WriteLog( "Unknown pragma instance, name<%s> text<%s>.\n", name.c_str(), instance.Text.c_str() );
}
