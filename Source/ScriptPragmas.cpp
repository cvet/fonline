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

    bool Call( const string& text )
    {
        if( text.length() > 0 )
            ignoredPragmas.push_back( text );
        return true;
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
    bool Call( const string& text )
    {
        asIScriptEngine* engine = Script::GetEngine();
        string           type, decl, value;
        char             ch = 0;
        istrstream       str( text.c_str() );
        str >> type >> decl >> ch >> value;

        if( decl == "" )
        {
            WriteLog( "Global var name not found, pragma<%s>.\n", text.c_str() );
            return false;
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
                return false;
            }
            auto it = boolArray.insert( boolArray.begin(), value == "true" ? true : false );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 ) WriteLog( "Unable to register boolean global var, pragma<%s>.\n", text.c_str() );
        }
        else
        {
            WriteLog( "Global var not registered, unknown type, pragma<%s>.\n", text.c_str() );
        }
        return true;
    }
};

// #pragma bindfunc "int MyObject::MyMethod(int, uint) -> my.dll MyDllFunc"
class BindFuncPragma
{
public:
    bool Call( const string& text )
    {
        asIScriptEngine* engine = Script::GetEngine();
        string           func_name, dll_name, func_dll_name;
        istrstream       str( text.c_str() );
        while( true )
        {
            string s;
            str >> s;
            if( str.fail() || s == "->" )
                break;
            if( func_name != "" )
                func_name += " ";
            func_name += s;
        }
        str >> dll_name;
        str >> func_dll_name;

        if( str.fail() )
        {
            WriteLog( "Error in 'bindfunc' pragma<%s>, parse fail.\n", text.c_str() );
            return false;
        }

        void* dll = Script::LoadDynamicLibrary( dll_name.c_str() );
        if( !dll )
        {
            WriteLog( "Error in 'bindfunc' pragma<%s>, dll not found, error<%s>.\n", text.c_str(), DLL_Error() );
            return false;
        }

        // Find function
        size_t* func = DLL_GetAddress( dll, func_dll_name.c_str() );
        if( !func )
        {
            WriteLog( "Error in 'bindfunc' pragma<%s>, function not found, error<%s>.\n", text.c_str(), DLL_Error() );
            return false;
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
                return false;
            }
            i++;
            string class_name;
            class_name.assign( func_name, i, j - i );
            func_name.erase( i, j - i + 2 );
            result = engine->RegisterObjectMethod( class_name.c_str(), func_name.c_str(), asFUNCTION( func ), asCALL_CDECL_OBJFIRST );
        }
        if( result < 0 )
        {
            WriteLog( "Error in 'bindfunc' pragma<%s>, script registration fail, error<%d>.\n", text.c_str(), result );
            return false;
        }
        return true;
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

    bool Call( const string& text )
    {
        // Read all
        istrstream str( text.c_str() );
        string     class_name, access_str, property_type_name, property_name;
        str >> class_name >> access_str;

        // Defaults
        bool is_defaults = false;
        if( Str::CompareCase( access_str.c_str(), "Defaults" ) )
            is_defaults = true;

        // Property type and name
        if( !is_defaults )
        {
            str >> property_type_name >> property_name;
            if( property_name == "<" && !str.fail() )
            {
                while( property_name != ">" && !str.fail() )
                {
                    property_type_name += property_name;
                    str >> property_name;
                }
                property_type_name += property_name;
                str >> property_name;
            }
            if( property_name == "[" && !str.fail() )
            {
                while( property_name != "]" && !str.fail() )
                {
                    property_type_name += property_name;
                    str >> property_name;
                }
                property_type_name += property_name;
                str >> property_name;
            }
        }

        if( str.fail() )
        {
            WriteLog( "Error in 'property' pragma<%s>.\n", text.c_str() );
            return false;
        }

        char options_buf[ MAX_FOTEXT ] = { 0 };
        str.getline( options_buf, MAX_FOTEXT );

        // Parse access type
        Property::AccessType access = (Property::AccessType) 0;
        if( !is_defaults )
        {
            if( Str::CompareCase( access_str.c_str(), "Private" ) )
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
            else if( Str::CompareCase( access_str.c_str(), "VirtualPrivate" ) )
                access = Property::VirtualPrivate;
            else if( Str::CompareCase( access_str.c_str(), "VirtualPrivateClient" ) )
                access = Property::VirtualPrivateClient;
            else if( Str::CompareCase( access_str.c_str(), "VirtualPrivateServer" ) )
                access = Property::VirtualPrivateServer;
            else if( Str::CompareCase( access_str.c_str(), "VirtualPublic" ) )
                access = Property::VirtualPublic;
            else if( Str::CompareCase( access_str.c_str(), "VirtualProtected" ) )
                access = Property::VirtualProtected;
            if( access == (Property::AccessType) 0 )
            {
                WriteLog( "Error in 'property' pragma<%s>, invalid access type<%s>.\n", text.c_str(), access_str.c_str() );
                return false;
            }
        }

        // Parse options
        string group;
        bool   generate_random_value = false;
        bool   set_default_value = false;
        int64  default_value = 0;
        bool   check_min_value = false;
        int64  min_value = 0;
        bool   check_max_value = false;
        int64  max_value = 0;
        string get_callback;
        StrVec set_callbacks;
        StrVec opt_entries;
        Str::ParseLine( options_buf, ',', opt_entries, Str::ParseLineDummy );
        for( size_t i = 0, j = opt_entries.size(); i < j; i++ )
        {
            StrVec opt_entry;
            Str::ParseLine( opt_entries[ i ].c_str(), '=', opt_entry, Str::ParseLineDummy );
            if( opt_entry.size() != 2 )
            {
                WriteLog( "Error in 'property' pragma<%s>, invalid options entry.\n", text.c_str() );
                return false;
            }

            const char*   opt_name = opt_entry[ 0 ].c_str();
            const string& opt_svalue = opt_entry[ 1 ];
            int64         opt_ivalue = ConvertParamValue( opt_svalue.c_str() );

            if( Str::CompareCase( opt_name, "Group" ) )
            {
                group = opt_svalue;
            }
            else if( Str::CompareCase( opt_name, "Default" ) )
            {
                set_default_value = true;
                default_value = opt_ivalue;
            }
            else if( Str::CompareCase( opt_name, "Min" ) )
            {
                check_min_value = true;
                min_value = opt_ivalue;
            }
            else if( Str::CompareCase( opt_name, "Max" ) )
            {
                check_max_value = true;
                max_value = opt_ivalue;
            }
            else if( Str::CompareCase( opt_name, "Random" ) )
            {
                generate_random_value = ( opt_ivalue != 0 );
            }
            else if( Str::CompareCase( opt_name, "GetCallabck" ) )
            {
                get_callback = opt_svalue;
            }
            else if( Str::CompareCase( opt_name, "SetCallabck" ) )
            {
                set_callbacks.push_back( opt_svalue );
            }
        }

        // Choose registrator
        PropertyRegistrator* registrator = NULL;
        if( class_name == "Critter" )
            registrator = propertyRegistrators[ 0 ];
        else if( class_name == "Item" )
            registrator = propertyRegistrators[ 1 ];
        else if( class_name == "ProtoItem" )
            registrator = propertyRegistrators[ 2 ];
        else
            WriteLog( "Invalid class in 'property' pragma<%s>.\n", text.c_str() );
        if( !registrator )
            return false;

        // Register
        if( !is_defaults )
        {
            if( !registrator->Register( property_type_name.c_str(), property_name.c_str(), access,
                                        group.length() > 0 ? group.c_str() : NULL, generate_random_value ? &generate_random_value : NULL,
                                        set_default_value ? &default_value : NULL, check_min_value ? &min_value : NULL, check_max_value ? &max_value : NULL ) )
            {
                WriteLog( "Unable to register 'property' pragma<%s>.\n", text.c_str() );
                return false;
            }
        }

        // Work with defaults
        if( is_defaults )
        {
            registrator->SetDefaults( group.length() > 0 ? group.c_str() : NULL, generate_random_value ? &generate_random_value : NULL,
                                      set_default_value ? &default_value : NULL, check_min_value ? &min_value : NULL, check_max_value ? &max_value : NULL );
        }

        return true;
    }

    bool Finish()
    {
        return true;
    }
};

// #pragma content Group fileName
#ifdef FONLINE_SERVER
# include "Dialogs.h"
# include "ItemManager.h"
# include "CritterManager.h"
#endif
class ContentPragma
{
private:
    list< hash > dataStorage;
    StrUIntMap   filesToCheck[ 3 ];

public:
    bool Call( const string& text )
    {
        // Read all
        istrstream str( text.c_str() );
        string     group, file_name;
        str >> group >> file_name;
        if( str.fail() )
        {
            WriteLog( "Unable to parse 'content' pragma<%s>.\n", text.c_str() );
            return false;
        }

        // Make hash
        hash h = Str::GetHash( file_name.c_str() );

        // Verify file
        asIScriptEngine* engine = Script::GetEngine();
        int              group_index;
        const char*      ns;
        if( Str::CompareCase( group.c_str(), "Dialog" ) )
        {
            group_index = 0;
            ns = "Content::Dialog";
        }
        else if( Str::CompareCase( group.c_str(), "Item" ) )
        {
            group_index = 1;
            ns = "Content::Item";
        }
        else if( Str::CompareCase( group.c_str(), "Critter" ) )
        {
            group_index = 2;
            ns = "Content::Critter";
        }
        else
        {
            WriteLog( "Invalid group name in 'content' pragma<%s>.\n", text.c_str() );
            return false;
        }

        if( filesToCheck[ group_index ].count( file_name ) )
            return true;
        filesToCheck[ group_index ].insert( PAIR( file_name, h ) );

        // Register file
        dataStorage.push_back( h );
        engine->SetDefaultNamespace( "Content" );
        engine->SetDefaultNamespace( ns );
        char prop_name[ MAX_FOTEXT ];
        Str::Format( prop_name, "const hash %s", file_name.c_str() );
        int  result = engine->RegisterGlobalProperty( prop_name, &dataStorage.back() );
        engine->SetDefaultNamespace( "" );
        if( result < 0 )
        {
            WriteLog( "Error in 'content' pragma<%s>, error<%d>.\n", text.c_str(), result );
            return false;
        }

        return true;
    }

    bool Finish()
    {
        int errors = 0;
        #ifdef FONLINE_SERVER
        for( auto it = filesToCheck[ 0 ].begin(); it != filesToCheck[ 0 ].end(); ++it )
        {
            if( !DlgMngr.GetDialog( it->second ) )
            {
                WriteLog( "Dialog file '%s' not found.\n", it->first.c_str() );
                errors++;
            }
        }
        for( auto it = filesToCheck[ 1 ].begin(); it != filesToCheck[ 1 ].end(); ++it )
        {
            if( !ItemMngr.GetProtoItem( it->second ) )
            {
                WriteLog( "Item file '%s' not found.\n", it->first.c_str() );
                errors++;
            }
        }
        for( auto it = filesToCheck[ 2 ].begin(); it != filesToCheck[ 2 ].end(); ++it )
        {
            if( !CrMngr.GetProto( it->second ) )
            {
                WriteLog( "Critter file '%s' not found.\n", it->first.c_str() );
                errors++;
            }
        }
        #endif
        return errors == 0;
    }
};

ScriptPragmaCallback::ScriptPragmaCallback( int pragma_type, PropertyRegistrator** properties_registrators )
{
    isError = false;
    pragmaType = pragma_type;
    if( pragmaType != PRAGMA_SERVER && pragmaType != PRAGMA_CLIENT && pragmaType != PRAGMA_MAPPER )
        pragmaType = PRAGMA_UNKNOWN;

    ignorePragma = NULL;
    globalVarPragma = NULL;
    bindFuncPragma = NULL;
    propertyPragma = NULL;
    contentPragma = NULL;

    if( pragmaType != PRAGMA_UNKNOWN )
    {
        ignorePragma = new IgnorePragma();
        globalVarPragma = new GlobalVarPragma();
        bindFuncPragma = new BindFuncPragma();
        propertyPragma = new PropertyPragma( properties_registrators );
        contentPragma = new ContentPragma();
    }
}

ScriptPragmaCallback::~ScriptPragmaCallback()
{
    SAFEDEL( ignorePragma );
    SAFEDEL( globalVarPragma );
    SAFEDEL( bindFuncPragma );
    SAFEDEL( propertyPragma );
    SAFEDEL( contentPragma );
}

void ScriptPragmaCallback::CallPragma( const Preprocessor::PragmaInstance& pragma )
{
    string pragma_instance = pragma.CurrentFile + Str::ItoA( pragma.CurrentFileLine );
    if( alreadyProcessed.count( pragma_instance ) )
        return;
    alreadyProcessed.insert( pragma_instance );

    if( ignorePragma && ignorePragma->IsIgnored( pragma.Name ) )
        return;

    bool ok = false;
    if( Str::CompareCase( pragma.Name.c_str(), "ignore" ) && ignorePragma )
        ok = ignorePragma->Call( pragma.Text );
    else if( Str::CompareCase( pragma.Name.c_str(), "globalvar" ) && globalVarPragma )
        ok = globalVarPragma->Call( pragma.Text );
    else if( Str::CompareCase( pragma.Name.c_str(), "bindfunc" ) && bindFuncPragma )
        ok = bindFuncPragma->Call( pragma.Text );
    else if( Str::CompareCase( pragma.Name.c_str(), "property" ) && propertyPragma )
        ok = propertyPragma->Call( pragma.Text );
    else if( Str::CompareCase( pragma.Name.c_str(), "content" ) && contentPragma )
        ok = contentPragma->Call( pragma.Text );
    else
        WriteLog( "Unknown pragma instance, name<%s> text<%s>.\n", pragma.Name.c_str(), pragma.Text.c_str() ), ok = false;

    if( ok )
        processedPragmas.push_back( pragma );
    else
        isError = true;
}

const Pragmas& ScriptPragmaCallback::GetProcessedPragmas()
{
    return processedPragmas;
}

void ScriptPragmaCallback::Finish()
{
    if( !propertyPragma->Finish() )
        isError = true;
    if( !contentPragma->Finish() )
        isError = true;
}

bool ScriptPragmaCallback::IsError()
{
    return isError;
}
