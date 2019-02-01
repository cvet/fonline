#include "Common.h"
#include "ScriptPragmas.h"
#include "angelscript.h"
#include "Script.h"
#include "ProtoManager.h"

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
    list< int >    intArray;
    list< int64 >  int64Array;
    list< string > stringArray;
    list< float >  floatArray;
    list< double > doubleArray;
    list< char >   boolArray;

public:
    bool Call( const string& text )
    {
        asIScriptEngine* engine = Script::GetEngine();
        string           type, decl, value;
        char             ch = 0;
        istringstream    str( text );
        str >> type >> decl >> ch >> value;

        if( decl == "" )
        {
            WriteLog( "Global var name not found, pragma '{}'.\n", text );
            return false;
        }

        int    int_value = ( ch == '=' ? _str( value ).toInt() : 0 );
        double float_value = ( ch == '=' ? _str( value ).toDouble() : 0.0 );
        string name = type + " " + decl;

        // Register
        if( type == "int8" || type == "int16" || type == "int32" || type == "int" || type == "uint8" || type == "uint16" || type == "uint32" || type == "uint" )
        {
            auto it = intArray.insert( intArray.begin(), int_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 )
                WriteLog( "Unable to register integer global var, pragma '{}'.\n", text );
        }
        else if( type == "int64" || type == "uint64" )
        {
            auto it = int64Array.insert( int64Array.begin(), int_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 )
                WriteLog( "Unable to register integer64 global var, pragma '{}'.\n", text );
        }
        else if( type == "string" )
        {
            if( value != "" )
                value = text.substr( text.find( value ), string::npos );
            auto it = stringArray.insert( stringArray.begin(), value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 )
                WriteLog( "Unable to register string global var, pragma '{}'.\n", text );
        }
        else if( type == "float" )
        {
            auto it = floatArray.insert( floatArray.begin(), (float) float_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 )
                WriteLog( "Unable to register float global var, pragma '{}'.\n", text );
        }
        else if( type == "double" )
        {
            auto it = doubleArray.insert( doubleArray.begin(), float_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 )
                WriteLog( "Unable to register double global var, pragma '{}'.\n", text );
        }
        else if( type == "bool" )
        {
            value = ( ch == '=' ? value : "false" );
            if( value != "true" && value != "false" )
            {
                WriteLog( "Invalid start value of boolean type, pragma '{}'.\n", text );
                return false;
            }
            auto it = boolArray.insert( boolArray.begin(), value == "true" ? true : false );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 )
                WriteLog( "Unable to register boolean global var, pragma '{}'.\n", text );
        }
        else if( engine->RegisterEnum( type.c_str() ) == asALREADY_REGISTERED )
        {
            auto it = intArray.insert( intArray.begin(), int_value );
            if( engine->RegisterGlobalProperty( name.c_str(), &( *it ) ) < 0 )
                WriteLog( "Unable to register enum global var, pragma '{}'.\n", text );
        }
        else
        {
            WriteLog( "Global var not registered, unknown type, pragma '{}'.\n", text );
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
        istringstream    str( text );
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
            WriteLog( "Error in 'bindfunc' pragma '{}', parse fail.\n", text );
            return false;
        }

        void* dll = Script::LoadDynamicLibrary( dll_name );
        if( !dll )
        {
            WriteLog( "Error in 'bindfunc' pragma '{}', dll not found, error '{}'.\n", text, DLL_Error() );
            return false;
        }

        // Find function
        size_t* func = DLL_GetAddress( dll, func_dll_name );
        if( !func )
        {
            WriteLog( "Error in 'bindfunc' pragma '{}', function not found, error '{}'.\n", text, DLL_Error() );
            return false;
        }

        int result = 0;
        if( func_name.find( "::" ) == string::npos )
        {
            // Register global function
            result = engine->RegisterGlobalFunction( func_name.c_str(), asFUNCTION( func ), SCRIPT_FUNC_CONV );
        }
        else
        {
            // Register class method
            string::size_type i = func_name.find( " " );
            string::size_type j = func_name.find( "::" );
            if( i == string::npos || i + 1 >= j )
            {
                WriteLog( "Error in 'bindfunc' pragma '{}', parse class name fail.\n", text );
                return false;
            }
            i++;
            string class_name;
            class_name.assign( func_name, i, j - i );
            func_name.erase( i, j - i + 2 );
            result = engine->RegisterObjectMethod( class_name.c_str(), func_name.c_str(), asFUNCTION( func ), SCRIPT_FUNC_THIS_CONV );
        }
        if( result < 0 )
        {
            WriteLog( "Error in 'bindfunc' pragma '{}', script registration fail, error '{}'.\n", text, result );
            return false;
        }
        return true;
    }
};

// #pragma entity EntityName Movable = true
#ifdef FONLINE_SERVER
# include "EntityManager.h"
#endif

class EntityCreator
{
public:
    EntityCreator( bool is_server, const string& class_name )
    {
        ClassName = class_name;
        Registrator = new PropertyRegistrator( is_server, class_name );
        static uint sub_type_id = 0;
        SubType = ++sub_type_id;
    }

    string               ClassName;
    PropertyRegistrator* Registrator;
    uint                 SubType;

    #ifdef FONLINE_SERVER
    CustomEntity* CreateEntity()
    {
        CustomEntity* entity = new CustomEntity( 0, SubType, Registrator );
        EntityMngr.RegisterEntity( entity );
        return entity;
    }

    static void CreateEntity_Generic( asIScriptGeneric* gen )
    {
        gen->SetReturnObject( ( (EntityCreator*) gen->GetAuxiliary() )->CreateEntity() );
    }

    bool RestoreEntity( uint id, const DataBase::Document& doc )
    {
        CustomEntity* entity = new CustomEntity( id, SubType, Registrator );
        if( !entity->Props.LoadFromDbDocument( doc ) )
        {
            WriteLog( "Fail to restore properties for custom entity '{}' ({}).\n", Registrator->GetClassName(), id );
            entity->Release();
            return false;
        }
        EntityMngr.RegisterEntity( entity );
        return true;
    }

    void DeleteEntity( CustomEntity* entity )
    {
        RUNTIME_ASSERT( entity->SubType == SubType );
        entity->IsDestroyed = true;
        EntityMngr.UnregisterEntity( entity );
        entity->Release();
    }

    static void DeleteEntity_Generic( asIScriptGeneric* gen )
    {
        ( (EntityCreator*) gen->GetAuxiliary() )->DeleteEntity( (CustomEntity*) gen->GetArgObject( 0 ) );
    }

    void DeleteEntityById( uint id )
    {
        CustomEntity* entity = (CustomEntity*) EntityMngr.GetEntity( id, EntityType::Custom );
        if( !entity || entity->SubType != SubType )
            return;
        entity->IsDestroyed = true;
        EntityMngr.UnregisterEntity( entity );
        entity->Release();
    }

    static void DeleteEntityById_Generic( asIScriptGeneric* gen )
    {
        ( (EntityCreator*) gen->GetAuxiliary() )->DeleteEntityById( gen->GetArgDWord( 0 ) );
    }

    CustomEntity* GetEntity( uint id )
    {
        CustomEntity* entity = (CustomEntity*) EntityMngr.GetEntity( id, EntityType::Custom );
        if( entity && entity->SubType == SubType )
            return entity;
        return nullptr;
    }

    static void GetEntity_Generic( asIScriptGeneric* gen )
    {
        gen->SetReturnObject( ( (EntityCreator*) gen->GetAuxiliary() )->GetEntity( gen->GetArgDWord( 0 ) ) );
    }

    #else
    CustomEntity* CreateEntity()                                { return nullptr; }
    static void   CreateEntity_Generic( asIScriptGeneric* gen ) {}
    # ifdef FONLINE_SERVER
    bool RestoreEntity( uint id, const DataBase::Document& doc ) { return false; }
    # endif
    void          DeleteEntity( CustomEntity* entity )              {}
    static void   DeleteEntity_Generic( asIScriptGeneric* gen )     {}
    void          DeleteEntityById( uint id )                       {}
    static void   DeleteEntityById_Generic( asIScriptGeneric* gen ) {}
    CustomEntity* GetEntity( uint id )                              { return nullptr; }
    static void   GetEntity_Generic( asIScriptGeneric* gen )        {}
    #endif
};

class EntityPragma
{
    friend class PropertyPragma;

private:
    map< string, EntityCreator* > entityCreators;
    bool                          isServer;

public:
    EntityPragma( int pragma_type )
    {
        isServer = ( pragma_type == PRAGMA_SERVER || pragma_type == PRAGMA_MAPPER );
    }

    ~EntityPragma()
    {
        for( auto it = entityCreators.begin(); it != entityCreators.end(); ++it )
            SAFEDEL( it->second );
        entityCreators.clear();
    }

    bool Call( const string& text )
    {
        // Read all
        istringstream str( text );
        string        class_name;
        str >> class_name;

        // Check already registered classes
        asIScriptEngine* engine = Script::GetEngine();
        if( engine->GetTypeInfoByName( class_name.c_str() ) )
        {
            WriteLog( "Error in 'entity' pragma '{}', class already registered.\n", text );
            return false;
        }

        // Create object
        if( engine->RegisterObjectType( class_name.c_str(), 0, asOBJ_REF ) < 0 ||
            engine->RegisterObjectBehaviour( class_name.c_str(), asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD( Entity, AddRef ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectBehaviour( class_name.c_str(), asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD( Entity, Release ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectProperty( class_name.c_str(), "const uint Id", OFFSETOF( Entity, Id ) ) < 0 ||
            engine->RegisterObjectProperty( class_name.c_str(), "const bool IsDestroyed", OFFSETOF( Entity, IsDestroyed ) ) < 0 ||
            engine->RegisterObjectProperty( class_name.c_str(), "const bool IsDestroying", OFFSETOF( Entity, IsDestroying ) ) < 0 )
        {
            WriteLog( "Error in 'entity' pragma '{}', fail to register object type.\n", text );
            return false;
        }

        // Create creator
        EntityCreator* entity_creator = new EntityCreator( isServer, class_name );
        if( isServer )
        {
            #ifdef AS_MAX_PORTABILITY
            if( engine->RegisterGlobalFunction( _str( "{}@+ Create{}()", class_name, class_name ).c_str(), asFUNCTION( EntityCreator::CreateEntity_Generic ), asCALL_GENERIC, entity_creator ) < 0 ||
                engine->RegisterGlobalFunction( _str( "void Delete{}({}@+ entity)", class_name, class_name ).c_str(), asFUNCTION( EntityCreator::DeleteEntity_Generic ), asCALL_GENERIC, entity_creator ) < 0 ||
                engine->RegisterGlobalFunction( _str( "void Delete{}(uint id)", class_name ).c_str(), asFUNCTION( EntityCreator::DeleteEntityById_Generic ), asCALL_GENERIC, entity_creator ) < 0 ||
                engine->RegisterGlobalFunction( _str( "{}@+ Get{}(uint id)", class_name, class_name ).c_str(), asFUNCTION( EntityCreator::GetEntity_Generic ), asCALL_GENERIC, entity_creator ) < 0 )
            #else
            if( engine->RegisterGlobalFunction( _str( "{}@+ Create{}()", class_name, class_name ).c_str(), asMETHOD( EntityCreator, CreateEntity ), asCALL_THISCALL_ASGLOBAL, entity_creator ) < 0 ||
                engine->RegisterGlobalFunction( _str( "void Delete{}({}@+ entity)", class_name, class_name ).c_str(), asMETHOD( EntityCreator, DeleteEntity ), asCALL_THISCALL_ASGLOBAL, entity_creator ) < 0 ||
                engine->RegisterGlobalFunction( _str( "void Delete{}(uint id)", class_name ).c_str(), asMETHOD( EntityCreator, DeleteEntityById ), asCALL_THISCALL_ASGLOBAL, entity_creator ) < 0 ||
                engine->RegisterGlobalFunction( _str( "{}@+ Get{}(uint id)", class_name, class_name ).c_str(), asMETHOD( EntityCreator, GetEntity ), asCALL_THISCALL_ASGLOBAL, entity_creator ) < 0 )
            #endif
            {
                WriteLog( "Error in 'entity' pragma '{}', fail to register management functions.\n", text );
                return false;
            }
        }

        // Init registrator
        if( !entity_creator->Registrator->Init() )
        {
            WriteLog( "Error in 'entity' pragma '{}', fail to initialize property registrator.\n", text );
            return false;
        }

        // Add creator
        RUNTIME_ASSERT( !entityCreators.count( class_name ) );
        entityCreators.insert( std::make_pair( class_name, entity_creator ) );
        return true;
    }

    bool Finish()
    {
        for( auto it = entityCreators.begin(); it != entityCreators.end(); ++it )
            it->second->Registrator->FinishRegistration();
        return true;
    }

    #ifdef FONLINE_SERVER
    bool RestoreEntity( const string& class_name, uint id, const DataBase::Document& doc )
    {
        EntityCreator* entity_creator = entityCreators[ class_name ];
        return entity_creator->RestoreEntity( id, doc );
    }
    #endif

    StrVec GetTypeNames()
    {
        StrVec result;
        for( auto& creator : entityCreators )
            result.push_back( creator.first );
        return result;
    }
};

// #pragma property "MyObject Virtual int PropName Default = 100 Min = 10 Max = 50 Random = true"
class PropertyPragma
{
private:
    PropertyRegistrator** propertyRegistrators;
    EntityPragma*         entitiesRegistrators;

public:
    PropertyPragma( int pragma_type, EntityPragma* entities_registrators )
    {
        bool is_server = ( pragma_type == PRAGMA_SERVER );
        bool is_mapper = ( pragma_type == PRAGMA_MAPPER );

        propertyRegistrators = new PropertyRegistrator*[ 5 ];
        propertyRegistrators[ 0 ] = new PropertyRegistrator( is_server || is_mapper, "GlobalVars" );
        propertyRegistrators[ 1 ] = new PropertyRegistrator( is_server || is_mapper, "Critter" );
        propertyRegistrators[ 2 ] = new PropertyRegistrator( is_server || is_mapper, "Item" );
        propertyRegistrators[ 3 ] = new PropertyRegistrator( is_server || is_mapper, "Map" );
        propertyRegistrators[ 4 ] = new PropertyRegistrator( is_server || is_mapper, "Location" );

        entitiesRegistrators = entities_registrators;
    }

    ~PropertyPragma()
    {
        for( int i = 0; i < 5; i++ )
            SAFEDEL( propertyRegistrators[ i ] );
        SAFEDEL( propertyRegistrators );
    }

    PropertyRegistrator** GetPropertyRegistrators()
    {
        return propertyRegistrators;
    }

    bool Call( const string& text )
    {
        // Read all
        istringstream str( text );
        string        class_name, access_str, property_type_name, property_name;
        str >> class_name >> access_str;

        // Component
        bool   is_component = false;
        string component_name;
        if( access_str == "Component" )
        {
            is_component = true;
            str >> component_name;
        }

        // Defaults
        bool is_defaults = false;
        if( access_str == "Defaults" )
            is_defaults = true;

        // Property type and name
        bool is_const = false;
        if( !is_component && !is_defaults )
        {
            str >> property_type_name;
            if( property_type_name == "const" )
            {
                is_const = true;
                str >> property_type_name;
            }

            str >> property_name;
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
            WriteLog( "Error in 'property' pragma '{}'.\n", text );
            return false;
        }

        string options;
        std::getline( str, options, '\n' );

        // Parse access type
        Property::AccessType access = (Property::AccessType) 0;
        if( !is_component && !is_defaults )
        {
            if( access_str == "PrivateCommon" )
                access = Property::PrivateCommon;
            else if( access_str == "PrivateClient" )
                access = Property::PrivateClient;
            else if( access_str == "PrivateServer" )
                access = Property::PrivateServer;
            else if( access_str == "Public" )
                access = Property::Public;
            else if( access_str == "PublicModifiable" )
                access = Property::PublicModifiable;
            else if( access_str == "PublicFullModifiable" )
                access = Property::PublicFullModifiable;
            else if( access_str == "Protected" )
                access = Property::Protected;
            else if( access_str == "ProtectedModifiable" )
                access = Property::ProtectedModifiable;
            else if( access_str == "VirtualPrivateCommon" )
                access = Property::VirtualPrivateCommon;
            else if( access_str == "VirtualPrivateClient" )
                access = Property::VirtualPrivateClient;
            else if( access_str == "VirtualPrivateServer" )
                access = Property::VirtualPrivateServer;
            else if( access_str == "VirtualPublic" )
                access = Property::VirtualPublic;
            else if( access_str == "VirtualProtected" )
                access = Property::VirtualProtected;
            if( access == (Property::AccessType) 0 )
            {
                WriteLog( "Error in 'property' pragma '{}', invalid access type '{}'.\n", text, access_str );
                return false;
            }
        }

        // Parse options
        bool   fail = false;
        string group;
        bool   check_min_value = false;
        int64  min_value = 0;
        bool   check_max_value = false;
        int64  max_value = 0;
        string get_callback;
        StrVec set_callbacks;
        bool   is_temporary = false;
        bool   is_no_history = false;
        for( const string& opt_entry_str : _str( options ).split( ',' ) )
        {
            StrVec        opt_entry = _str( opt_entry_str ).split( '=' );
            const string& opt_name = opt_entry[ 0 ];
            const string& opt_svalue = ( opt_entry.size() > 1 ? opt_entry[ 1 ] : "" );

            if( opt_name != "Temporary" && opt_name != "NoHistory" && opt_entry.size() != 2 )
            {
                WriteLog( "Error in 'property' pragma '{}', invalid options entry.\n", text );
                return false;
            }

            if( opt_name == "Group" )
            {
                group = opt_svalue;
            }
            else if( opt_name == "Min" )
            {
                check_min_value = true;
                min_value = ConvertParamValue( opt_svalue, fail );
            }
            else if( opt_name == "Max" )
            {
                check_max_value = true;
                max_value = ConvertParamValue( opt_svalue, fail );
            }
            else if( opt_name == "GetCallabck" )
            {
                get_callback = opt_svalue;
            }
            else if( opt_name == "SetCallabck" )
            {
                set_callbacks.push_back( opt_svalue );
            }
            else if( opt_name == "Temporary" )
            {
                is_temporary = true;
            }
            else if( opt_name == "NoHistory" )
            {
                is_no_history = true;
            }
        }
        if( fail )
        {
            WriteLog( "Error in 'property' pragma '{}', value conversation failed.\n", text );
            return false;
        }

        // Choose registrator
        PropertyRegistrator* registrator = nullptr;
        if( class_name == "Global" )
            registrator = propertyRegistrators[ 0 ];
        else if( class_name == "Critter" )
            registrator = propertyRegistrators[ 1 ];
        else if( class_name == "Item" )
            registrator = propertyRegistrators[ 2 ];
        else if( class_name == "Map" )
            registrator = propertyRegistrators[ 3 ];
        else if( class_name == "Location" )
            registrator = propertyRegistrators[ 4 ];
        else if( entitiesRegistrators->entityCreators.count( class_name ) )
            registrator = entitiesRegistrators->entityCreators[ class_name ]->Registrator;
        else
            WriteLog( "Invalid class in 'property' pragma '{}'.\n", text );
        if( !registrator )
            return false;

        // Register
        if( !is_component && !is_defaults )
        {
            if( !registrator->Register( property_type_name.c_str(), property_name.c_str(), access, is_const,
                                        group.length() > 0 ? group.c_str() : nullptr,
                                        check_min_value ? &min_value : nullptr, check_max_value ? &max_value : nullptr,
                                        is_temporary, is_no_history ) )
            {
                WriteLog( "Unable to register 'property' pragma '{}'.\n", text );
                return false;
            }
        }

        // New component
        if( is_component )
        {
            if( !registrator->RegisterComponent( component_name ) )
            {
                WriteLog( "Unable to register component '{}' for 'property' pragma '{}'.\n", component_name, text );
                return false;
            }
        }

        // Work with defaults
        if( is_defaults )
        {
            registrator->SetDefaults( group.length() > 0 ? group.c_str() : nullptr,
                                      check_min_value ? &min_value : nullptr, check_max_value ? &max_value : nullptr,
                                      is_temporary, is_no_history );
        }

        return true;
    }

    bool Finish()
    {
        return true;
    }
};

// #pragma method MyObject CallType void Foo(int a, int b) FuncToBind
class MethodPragma
{
private:
    MethodRegistrator** methodRegistrator;
    EntityPragma*       entitiesRegistrators;

public:
    MethodPragma( int pragma_type, EntityPragma* entities_registrators )
    {
        bool is_server = ( pragma_type == PRAGMA_SERVER );
        bool is_mapper = ( pragma_type == PRAGMA_MAPPER );

        methodRegistrator = new MethodRegistrator*[ 6 ];
        methodRegistrator[ 0 ] = new MethodRegistrator( is_server || is_mapper, "GlobalVars" );
        methodRegistrator[ 1 ] = new MethodRegistrator( is_server || is_mapper, "Critter" );
        methodRegistrator[ 2 ] = new MethodRegistrator( is_server || is_mapper, "Item" );
        methodRegistrator[ 3 ] = new MethodRegistrator( is_server || is_mapper, "Map" );
        methodRegistrator[ 4 ] = new MethodRegistrator( is_server || is_mapper, "Location" );

        entitiesRegistrators = entities_registrators;
    }

    bool Call( const string& text )
    {
        // Read all
        istringstream str( text );
        string        class_name, call_type_str;
        str >> class_name >> call_type_str;
        string        line;
        std::getline( str, line, '\n' );
        if( str.fail() )
        {
            WriteLog( "Error in 'method' pragma '{}'.\n", text );
            return false;
        }

        // Decl
        size_t separator =  line.find( '=' );
        if( separator == string::npos )
        {
            WriteLog( "Error in 'method' pragma declaration '{}'.\n", text );
            return false;
        }

        string decl = _str( line.substr( 0, separator ) ).trim();
        string script_func = _str( line.substr( separator + 1 ) ).trim();

        // Parse access type
        Method::CallType call_type = (Method::CallType) 0;
        if( call_type_str == "LocalServer" )
            call_type = Method::LocalServer;
        else if( call_type_str == "LocalClient" )
            call_type = Method::LocalClient;
        else if( call_type_str == "LocalCommon" )
            call_type = Method::LocalCommon;
        else if( call_type_str == "RemoteServer" )
            call_type = Method::RemoteServer;
        else if( call_type_str == "RemoteClient" )
            call_type = Method::RemoteClient;
        if( call_type == (Method::CallType) 0 )
        {
            WriteLog( "Error in 'method' pragma '{}', invalid call type '{}'.\n", text, call_type_str );
            return false;
        }

        // Choose registrator
        MethodRegistrator* registrator = nullptr;
        if( class_name == "Global" )
            registrator = methodRegistrator[ 0 ];
        else if( class_name == "Critter" )
            registrator = methodRegistrator[ 1 ];
        else if( class_name == "Item" )
            registrator = methodRegistrator[ 2 ];
        else if( class_name == "Map" )
            registrator = methodRegistrator[ 3 ];
        else if( class_name == "Location" )
            registrator = methodRegistrator[ 4 ];
        // else if (entitiesRegistrators->entityCreators.count(class_name))
        //	registrator = entitiesRegistrators->entityCreators[class_name]->Registrator;
        else
            WriteLog( "Invalid class in 'property' pragma '{}'.\n", text );
        if( !registrator )
            return false;

        // Register
        if( !registrator->Register( decl.c_str(), script_func.c_str(), call_type ) )
        {
            WriteLog( "Unable to register 'method' pragma '{}'.\n", text );
            return false;
        }

        return true;
    }

    bool Finish()
    {
        return true; // methodRegistrator->FinishRegistration();
    }
};

// #pragma content Group fileName
#ifdef FONLINE_SERVER
# include "Dialogs.h"
#endif

class ContentPragma
{
private:
    list< hash > dataStorage;
    StrUIntMap   filesToCheck[ 5 ];

public:
    bool Call( const string& text )
    {
        // Read all
        istringstream str( text );
        string        group;
        string        name;
        str >> group >> name;
        if( str.fail() )
        {
            WriteLog( "Unable to parse 'content' pragma '{}'.\n", text );
            return false;
        }

        // Verify file
        asIScriptEngine* engine = Script::GetEngine();
        int              group_index;
        const char*      ns;
        if( group == "Dialog" )
        {
            group_index = 0;
            ns = "Content::Dialog";
        }
        else if( group == "Item" )
        {
            group_index = 1;
            ns = "Content::Item";
        }
        else if( group == "Critter" )
        {
            group_index = 2;
            ns = "Content::Critter";
        }
        else if( group == "Location" )
        {
            group_index = 3;
            ns = "Content::Location";
        }
        else if( group == "Map" )
        {
            group_index = 4;
            ns = "Content::Map";
        }
        else
        {
            WriteLog( "Invalid group name in 'content' pragma '{}'.\n", text );
            return false;
        }

        // Ignore redundant calls
        if( filesToCheck[ group_index ].count( name ) )
            return true;

        // Add to collection
        hash h = _str( name ).toHash();
        filesToCheck[ group_index ].insert( std::make_pair( name, h ) );

        // Register file
        engine->SetDefaultNamespace( "Content" );
        engine->SetDefaultNamespace( ns );
        string prop_name = _str( "const hash {}", name );
        dataStorage.push_back( h );
        int    result = engine->RegisterGlobalProperty( prop_name.c_str(), &dataStorage.back() );
        engine->SetDefaultNamespace( "" );
        if( result < 0 )
        {
            WriteLog( "Error in 'content' pragma '{}', error {}.\n", text, result );
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
                WriteLog( "Dialog file '{}' not found.\n", it->first );
                errors++;
            }
        }
        for( auto it = filesToCheck[ 1 ].begin(); it != filesToCheck[ 1 ].end(); ++it )
        {
            if( !ProtoMngr.GetProtoItem( it->second ) )
            {
                WriteLog( "Item file '{}' not found.\n", it->first );
                errors++;
            }
        }
        for( auto it = filesToCheck[ 2 ].begin(); it != filesToCheck[ 2 ].end(); ++it )
        {
            if( !ProtoMngr.GetProtoCritter( it->second ) )
            {
                WriteLog( "Critter file '{}' not found.\n", it->first );
                errors++;
            }
        }
        for( auto it = filesToCheck[ 3 ].begin(); it != filesToCheck[ 3 ].end(); ++it )
        {
            if( !ProtoMngr.GetProtoLocation( it->second ) )
            {
                WriteLog( "Location file '{}' not found.\n", it->first );
                errors++;
            }
        }
        for( auto it = filesToCheck[ 4 ].begin(); it != filesToCheck[ 4 ].end(); ++it )
        {
            if( !ProtoMngr.GetProtoMap( it->second ) )
            {
                WriteLog( "Map file '{}' not found.\n", it->first );
                errors++;
            }
        }
        #endif
        return errors == 0;
    }
};

// #pragma enum EnumName ValueName [= value]
class EnumPragma
{
public:
    bool Call( const string& text )
    {
        // Read all
        istringstream str( text );
        string        enum_name;
        string        value_name;
        str >> enum_name >> value_name;
        if( str.fail() )
        {
            WriteLog( "Unable to parse 'enum' pragma '{}'.\n", text );
            return false;
        }
        string other;
        std::getline( str, other, '\n' );

        asIScriptEngine* engine = Script::GetEngine();
        int              result = engine->RegisterEnum( enum_name.c_str() );
        if( result < 0 && result != asALREADY_REGISTERED )
        {
            WriteLog( "Error in 'enum' pragma '{}', register enum error {}.\n", text, result );
            return false;
        }

        int    value;
        size_t separator = other.find( '=' );
        if( separator != string::npos )
        {
            bool fail = false;
            value = ConvertParamValue( _str( other.substr( separator + 1 ) ).trim(), fail );
            if( fail )
            {
                WriteLog( "Error in 'enum' pragma '{}', value conversation error.\n", text );
                return false;
            }
        }
        else
        {
            value = _str( "{}::{}", enum_name, value_name ).toHash();
        }

        result = engine->RegisterEnumValue( enum_name.c_str(), value_name.c_str(), value );
        if( result < 0 )
        {
            WriteLog( "Error in 'enum' pragma '{}', register enum value error {}.\n", text, result );
            return false;
        }

        return true;
    }
};

// #pragma event MyEvent (Critter&, int, bool)
extern bool as_builder_ForceAutoHandles;
class EventPragma
{
    class ScriptEvent
    {
public:
        typedef vector< asIScriptFunction* >           FuncVec;
        typedef multimap< uint64, asIScriptFunction* > FuncMulMap;
        typedef multimap< Entity*, FuncMulMap* >       EntityFuncMulMap;

        struct ArgInfo
        {
            bool       IsObjectEntity;
            bool       IsObject;
            bool       IsPodRef;
            uint       PodSize;
            FuncMulMap Callbacks;
        };
        typedef vector< ArgInfo > ArgInfoVec;

        string            Name;
        mutable int       RefCount;
        bool              Deferred;
        FuncVec           Callbacks;
        ArgInfoVec        ArgInfos;
        EntityFuncMulMap* EntityCallbacks;

        ScriptEvent()
        {
            RefCount = 1;
        }

        ~ScriptEvent()
        {
            UnsubscribeAll();
        }

        void AddRef() const
        {
            asAtomicInc( RefCount );
        }

        void Release() const
        {
            if( asAtomicDec( RefCount ) == 0 )
                delete this;
        }

        void Subscribe( asIScriptFunction* callback )
        {
            callback->AddRef();

            Unsubscribe( callback );

            Callbacks.push_back( callback );
        }

        void Unsubscribe( asIScriptFunction* callback )
        {
            if( Callbacks.empty() )
                return;

            auto it = std::find( Callbacks.begin(), Callbacks.end(), callback );
            if( it == Callbacks.end() && callback->GetFuncType() == asFUNC_DELEGATE )
            {
                it = std::find_if( Callbacks.begin(), Callbacks.end(), [ &callback ] ( asIScriptFunction * cb )
                                   {
                                       return cb->GetFuncType() == asFUNC_DELEGATE && cb->GetDelegateFunction() == callback->GetDelegateFunction();
                                   } );
            }

            if( it != Callbacks.end() )
            {
                ( *it )->Release();
                Callbacks.erase( it );
            }
        }

        static void SubscribeTo( asIScriptGeneric* gen )
        {
            UnsubscribeFrom( gen );

            int          arg_index = *(int*) gen->GetAuxiliary();
            ScriptEvent* event = (ScriptEvent*) gen->GetObject();
            ArgInfo&     arg_info = event->ArgInfos[ arg_index ];

            uint64       value = 0;
            if( arg_info.IsObject )
                value = (uint64) gen->GetArgObject( 0 );
            else if( arg_info.IsPodRef )
                memcpy( &value, gen->GetArgAddress( 0 ), arg_info.PodSize );
            else
                memcpy( &value, gen->GetAddressOfArg( 0 ), arg_info.PodSize );

            asIScriptFunction* callback = (asIScriptFunction*) gen->GetArgObject( 1 );
            callback->AddRef();
            arg_info.Callbacks.insert( std::make_pair( value, callback ) );

            if( arg_info.IsObjectEntity )
                event->EntityCallbacks->insert( std::make_pair( (Entity*) gen->GetArgObject( 0 ), &arg_info.Callbacks ) );
        }

        static void UnsubscribeFrom( asIScriptGeneric* gen )
        {
            int                arg_index = *(int*) gen->GetAuxiliary();
            ScriptEvent*       event = (ScriptEvent*) gen->GetObject();
            ArgInfo&           arg_info = event->ArgInfos[ arg_index ];
            asIScriptFunction* callback = (asIScriptFunction*) gen->GetArgObject( 1 );

            uint64             value = 0;
            if( arg_info.IsObject )
                value = (uint64) gen->GetArgObject( 0 );
            else if( arg_info.IsPodRef )
                memcpy( &value, gen->GetArgAddress( 0 ), arg_info.PodSize );
            else
                memcpy( &value, gen->GetAddressOfArg( 0 ), arg_info.PodSize );

            // Erase from arg callbacks
            auto range = arg_info.Callbacks.equal_range( value );
            auto it = std::find_if( range.first, range.second, [ &callback ] ( FuncMulMap::value_type & kv ) { return kv.second == callback; } );
            if( it == range.second && callback->GetFuncType() == asFUNC_DELEGATE )
            {
                it = std::find_if( range.first, range.second, [ &callback ] ( FuncMulMap::value_type & kv )
                                   {
                                       return kv.second->GetFuncType() == asFUNC_DELEGATE && kv.second->GetDelegateFunction() == callback->GetDelegateFunction();
                                   } );
            }

            if( it != range.second )
            {
                // Erase from entity callbacks
                if( arg_info.IsObjectEntity )
                {
                    auto range_ = event->EntityCallbacks->equal_range( (Entity*) gen->GetArgObject( 0 ) );
                    auto cb_ptr = &arg_info.Callbacks;
                    auto it_ = std::find_if( range_.first, range_.second, [ &cb_ptr ] ( EntityFuncMulMap::value_type & kv ) { return kv.second == cb_ptr; } );
                    RUNTIME_ASSERT( it_ != range_.second );
                    event->EntityCallbacks->erase( it_ );
                }

                it->second->Release();
                arg_info.Callbacks.erase( it );
            }
        }

        void UnsubscribeAll()
        {
            // Global callbacks
            for( asIScriptFunction* callback : Callbacks )
                callback->Release();
            Callbacks.clear();

            // Args callbacks
            for( auto& arg_info : ArgInfos )
            {
                for( auto& arg_callback : arg_info.Callbacks )
                    arg_callback.second->Release();
                arg_info.Callbacks.clear();
            }

            // Entity callbacks
            for( auto it = EntityCallbacks->begin(); it != EntityCallbacks->end();)
            {
                bool erased = false;
                for( auto it_ = ArgInfos.begin(); it_ != ArgInfos.end(); ++it_ )
                {
                    if( it->second == &it_->Callbacks )
                    {
                        it = EntityCallbacks->erase( it );
                        erased = true;
                        break;
                    }
                }
                if( !erased )
                    ++it;
            }
        }

        static void Raise( asIScriptGeneric* gen )
        {
            ScriptEvent* event = (ScriptEvent*) gen->GetObject();
            *(bool*) gen->GetAddressOfReturnLocation() = event->RaiseImpl( gen, nullptr );
        }

        static bool RaiseInternal( void* event_ptr, va_list args )
        {
            ScriptEvent* event = (ScriptEvent*) event_ptr;

            UInt64Vec    va_args;
            for( size_t i = 0; i < event->ArgInfos.size(); i++ )
            {
                const ArgInfo& arg_info = event->ArgInfos[ i ];
                if( arg_info.IsObject )
                    va_args.push_back( (uint64) va_arg( args, void* ) );
                else if( arg_info.IsPodRef )
                    va_args.push_back( (uint64) va_arg( args, void* ) );
                else if( arg_info.PodSize == 1 )
                    va_args.push_back( (uint64) va_arg( args, int ) );
                else if( arg_info.PodSize == 2 )
                    va_args.push_back( (uint64) va_arg( args, int ) );
                else if( arg_info.PodSize == 4 )
                    va_args.push_back( (uint64) va_arg( args, int ) );
                else if( arg_info.PodSize == 8 )
                    va_args.push_back( (uint64) va_arg( args, int64 ) );
                else
                    RUNTIME_ASSERT( !"Unreachable place" );
            }

            return event->RaiseImpl( nullptr, &va_args );
        }

        bool RaiseImpl( asIScriptGeneric* gen_args, UInt64Vec* va_args )
        {
            #define GET_ARG_ADDR    ( gen_args ? gen_args->GetAddressOfArg( (asUINT) i ) : &va_args->at( i ) )
            #define GET_ARG( type )    ( *(type*) GET_ARG_ADDR )

            FuncVec callbacks_to_call;

            // Global callbacks
            if( !Callbacks.empty() )
                callbacks_to_call = Callbacks;

            // Arg callbacks
            for( size_t i = 0; i < ArgInfos.size(); i++ )
            {
                const ArgInfo& arg_info = ArgInfos[ i ];
                if( arg_info.Callbacks.empty() )
                    continue;

                uint64 value = 0;
                if( arg_info.IsObject )
                    memcpy( &value, GET_ARG_ADDR, sizeof( void* ) );
                else if( arg_info.IsPodRef )
                    memcpy( &value, GET_ARG( void* ), arg_info.PodSize );
                else if( arg_info.PodSize == 1 )
                    memcpy( &value, GET_ARG_ADDR, arg_info.PodSize );
                else if( arg_info.PodSize == 2 )
                    memcpy( &value, GET_ARG_ADDR, arg_info.PodSize );
                else if( arg_info.PodSize == 4 )
                    memcpy( &value, GET_ARG_ADDR, arg_info.PodSize );
                else if( arg_info.PodSize == 8 )
                    memcpy( &value, GET_ARG_ADDR, arg_info.PodSize );
                else
                    RUNTIME_ASSERT( !"Unreachable place" );

                auto range = arg_info.Callbacks.equal_range( value );
                for( auto it = range.first; it != range.second; ++it )
                    callbacks_to_call.push_back( it->second );
            }

            // Invoke callbacks
            for( int j = (int) callbacks_to_call.size() - 1; j >= 0; j-- )
            {
                asIScriptFunction* callback = callbacks_to_call[ j ];

                // Check entities
                for( size_t i = 0; i < ArgInfos.size(); i++ )
                {
                    const ArgInfo& arg_info = ArgInfos[ i ];
                    if( arg_info.IsObjectEntity )
                    {
                        Entity* entity = (Entity*) GET_ARG( void* );
                        if( entity && entity->IsDestroyed )
                            return false;
                    }
                }

                uint bind_id = Script::BindByFunc( callback, true );
                Script::PrepareContext( bind_id, "Event" );

                for( size_t i = 0; i < ArgInfos.size(); i++ )
                {
                    const ArgInfo& arg_info = ArgInfos[ i ];
                    if( arg_info.IsObjectEntity )
                        Script::SetArgEntity( (Entity*) GET_ARG( void* ) );
                    else if( arg_info.IsObject )
                        Script::SetArgObject( GET_ARG( void* ) );
                    else if( arg_info.IsPodRef )
                        Script::SetArgAddress( GET_ARG( void* ) );
                    else if( arg_info.PodSize == 1 )
                        Script::SetArgUChar( GET_ARG( uchar ) );
                    else if( arg_info.PodSize == 2 )
                        Script::SetArgUShort( GET_ARG( ushort ) );
                    else if( arg_info.PodSize == 4 )
                        Script::SetArgUInt( GET_ARG( uint ) );
                    else if( arg_info.PodSize == 8 )
                        Script::SetArgUInt64( GET_ARG( uint64 ) );
                    else
                        RUNTIME_ASSERT( !"Unreachable place" );
                }

                if( !Deferred )
                {
                    if( Script::RunPrepared() )
                    {
                        // Interrupted
                        if( callback->GetReturnTypeId() == asTYPEID_BOOL && !Script::GetReturnedBool() )
                            return false;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    Script::RunPreparedSuspend();
                }
            }
            #undef GET_ARG
            return true;
        }
    };

    list< ScriptEvent* >          events;
    ScriptEvent::EntityFuncMulMap entityCallbacks;

public:
    EventPragma()
    {
        // ...
    }

    ~EventPragma()
    {
        for( ScriptEvent* event : events )
        {
            event->UnsubscribeAll();
            event->Release();
        }
        events.clear();
    }

    bool Call( const string& text )
    {
        size_t args_begin = text.find_first_of( '(' );
        size_t args_end = text.find_first_of( ')' );
        if( args_begin == string::npos || args_end == string::npos || args_begin > args_end )
        {
            WriteLog( "Unable to parse 'event' pragma '{}'.\n", text );
            return false;
        }

        string event_name = _str( text.substr( 0, args_begin ) ).trim();
        if( event_name.empty() )
        {
            WriteLog( "Unable to parse 'event' pragma '{}'.\n", text );
            return false;
        }

        string           args = _str( text.substr( args_begin + 1, args_end - args_begin - 1 ) ).trim();
        asIScriptEngine* engine = Script::GetEngine();
        int              func_def_id;
        as_builder_ForceAutoHandles = true;
        if( ( func_def_id = engine->RegisterFuncdef( _str( "void {}Func({})", event_name, args ).c_str() ) ) < 0 ||
            engine->RegisterFuncdef( _str( "bool {}FuncBool({})", event_name, args ).c_str() ) < 0 ||
            engine->RegisterObjectType( event_name.c_str(), 0, asOBJ_REF ) < 0 ||
            engine->RegisterObjectBehaviour( event_name.c_str(), asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD( ScriptEvent, AddRef ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectBehaviour( event_name.c_str(), asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD( ScriptEvent, Release ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectMethod( event_name.c_str(), _str( "void Subscribe({}Func@+)", event_name ).c_str(), SCRIPT_METHOD( ScriptEvent, Subscribe ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectMethod( event_name.c_str(), _str( "void Subscribe({}FuncBool@+)", event_name ).c_str(), SCRIPT_METHOD( ScriptEvent, Subscribe ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectMethod( event_name.c_str(), _str( "void Unsubscribe({}Func@+)", event_name ).c_str(), SCRIPT_METHOD( ScriptEvent, Unsubscribe ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectMethod( event_name.c_str(), _str( "void Unsubscribe({}FuncBool@+)", event_name ).c_str(), SCRIPT_METHOD( ScriptEvent, Unsubscribe ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectMethod( event_name.c_str(), "void UnsubscribeAll()", SCRIPT_METHOD( ScriptEvent, UnsubscribeAll ), SCRIPT_METHOD_CONV ) < 0 ||
            engine->RegisterObjectMethod( event_name.c_str(), _str( "bool Raise({})", args ).c_str(), asFUNCTION( ScriptEvent::Raise ), asCALL_GENERIC ) < 0 )
        {
            as_builder_ForceAutoHandles = false;
            return false;
        }
        as_builder_ForceAutoHandles = false;

        ScriptEvent* event = new ScriptEvent();
        event->Name = event_name;
        event->EntityCallbacks = &entityCallbacks;

        asIScriptFunction* func_def = engine->GetFunctionById( func_def_id );
        event->ArgInfos.resize( func_def->GetParamCount() );
        for( asUINT i = 0; i < func_def->GetParamCount(); i++ )
        {
            int         type_id;
            asDWORD     flags;
            const char* name;
            func_def->GetParam( i, &type_id, &flags, &name );

            ScriptEvent::ArgInfo& arg_info = event->ArgInfos[ i ];
            arg_info.IsObject = ( type_id & asTYPEID_MASK_OBJECT ) != 0;
            arg_info.IsPodRef = ( type_id >= asTYPEID_BOOL && type_id <= asTYPEID_DOUBLE && flags & asTM_INOUTREF );
            arg_info.PodSize = engine->GetSizeOfPrimitiveType( type_id );

            arg_info.IsObjectEntity = false;
            if( arg_info.IsObject && type_id & asTYPEID_APPOBJECT )
            {
                int          matches = 0;
                asITypeInfo* obj_type = engine->GetTypeInfoById( type_id );
                for( asUINT j = 0; j < obj_type->GetPropertyCount() && matches < 3; j++ )
                {
                    const char* decl = obj_type->GetPropertyDeclaration( j );
                    if( Str::Compare( decl, "const uint Id" ) ||
                        Str::Compare( decl, "const bool IsDestroyed" ) ||
                        Str::Compare( decl, "const bool IsDestroying" ) )
                        matches++;
                }
                if( matches == 3 )
                    arg_info.IsObjectEntity = true;
            }

            if( name && name[ 0 ] )
            {
                string arg_type = engine->GetTypeDeclaration( type_id );
                if( flags & asTM_INOUTREF )
                    arg_type += "&";

                string arg_name = name;
                arg_name = _str( arg_name.substr( 0, 1 ) ).upper() + _str( arg_name.substr( 1 ) );
                if( engine->RegisterObjectMethod( event_name.c_str(), _str( "void SubscribeTo{}({}, {}Func@+)", arg_name, arg_type, event_name ).c_str(), asFUNCTION( ScriptEvent::SubscribeTo ), asCALL_GENERIC, new int(i) ) < 0 ||
                    engine->RegisterObjectMethod( event_name.c_str(), _str( "void SubscribeTo{}({}, {}FuncBool@+)", arg_name, arg_type, event_name ).c_str(), asFUNCTION( ScriptEvent::SubscribeTo ), asCALL_GENERIC, new int(i) ) < 0 ||
                    engine->RegisterObjectMethod( event_name.c_str(), _str( "void UnsubscribeFrom{}({}, {}Func@+)", arg_name, arg_type, event_name ).c_str(), asFUNCTION( ScriptEvent::UnsubscribeFrom ), asCALL_GENERIC, new int(i) ) < 0 ||
                    engine->RegisterObjectMethod( event_name.c_str(), _str( "void UnsubscribeFrom{}({}, {}FuncBool@+)", arg_name, arg_type, event_name ).c_str(), asFUNCTION( ScriptEvent::UnsubscribeFrom ), asCALL_GENERIC, new int(i) ) < 0 )
                    return false;
            }
        }

        string options;
        options = text.substr( args_end + 1 );
        event->Deferred = ( options.find( "deferred" ) != string::npos );

        events.push_back( event );
        if( engine->RegisterGlobalProperty( _str( "{}@ __{}", event_name, event_name ).c_str(), &events.back() ) < 0 )
        {
            events.pop_back();
            return false;
        }

        return true;
    }

    void* Find( const string& event_name )
    {
        for( ScriptEvent* event : events )
            if( event->Name == event_name )
                return event;
        RUNTIME_ASSERT_STR( false, event_name );
        return nullptr;
    }

    bool Raise( void* event_ptr, va_list args )
    {
        return ScriptEvent::RaiseInternal( event_ptr, args );
    }

    void RemoveEntity( Entity* entity )
    {
        auto range = entityCallbacks.equal_range( entity );
        if( range.first != range.second )
        {
            for( auto it = range.first; it != range.second; ++it )
            {
                auto range_ = it->second->equal_range( (uint64) entity );
                RUNTIME_ASSERT( range_.first != range_.second );
                it->second->erase( range_.first );
            }
            entityCallbacks.erase( range.first, range.second );
        }
    }
};

// #pragma rpc
#ifdef FONLINE_SERVER
# include "Critter.h"
#endif
#ifdef FONLINE_CLIENT
# include "Client.h"
#endif
class RpcPragma
{
    typedef vector< pair< string, string > > FuncDescVec;
    typedef vector< asIScriptFunction* >     FuncVec;

    bool inited;
    int pragmaType;
    int curOutIndex;
    FuncDescVec inFuncDesc;
    FuncVec inFunc;
    UIntVec inFuncBind;

public:
    RpcPragma( int pragma_type )
    {
        inited = false;
        pragmaType = pragma_type;
        curOutIndex = 0;
    }

    void Init()
    {
        asIScriptEngine* engine = Script::GetEngine();
        int r = engine->RegisterObjectType( "RpcCaller", 1, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS );
        RUNTIME_ASSERT( r >= 0 );

        if( pragmaType == PRAGMA_SERVER )
            r = engine->RegisterObjectProperty( "Critter", "RpcCaller Rpc", 0 );
        else
            r = engine->RegisterGlobalProperty( "RpcCaller ServerRpc", this );
        RUNTIME_ASSERT( r >= 0 );
    }

    bool Call( const string& text, const string& cur_file )
    {
        if( !inited )
        {
            inited = true;
            Init();
        }

        string type;
        string name;
        istringstream str( text );
        str >> type >> name;
        if( str.fail() )
        {
            WriteLog( "Unable to parse 'rpc' pragma '{}'.\n", text );
            return false;
        }
        if( type != "Server" && type != "Client" )
        {
            WriteLog( "Invalid type in 'rpc' pragma '{}'.\n", text );
            return false;
        }

        size_t args_begin = text.find_first_of( '(' );
        size_t args_end = text.find_first_of( ')' );
        if( args_begin == string::npos || args_end == string::npos || args_begin > args_end )
        {
            WriteLog( "Unable to parse 'rpc' pragma '{}'.\n", text );
            return false;
        }
        string args = _str( text.substr( args_begin + 1, args_end - args_begin - 1 ) ).trim();

        // Verify args
        asIScriptEngine* engine = Script::GetEngine();

        int func_def_id = engine->RegisterFuncdef( _str( "void {}RpcFunc({})", name, args ).c_str() );
        if( func_def_id < 0 )
        {
            WriteLog( "Invalid function in 'rpc' pragma '{}'.\n", text );
            return false;
        }

        if( args.find( '@' ) != string::npos )
        {
            WriteLog( "Handles is not allowed for 'rpc' pragma '{}'.\n", text );
            return false;
        }

        asIScriptFunction* func_def = engine->GetFunctionById( func_def_id );
        for( asUINT i = 0; i < func_def->GetParamCount(); i++ )
        {
            int type_id;
            asDWORD flags;
            func_def->GetParam( i, &type_id, &flags );

            if( type_id & asTYPEID_MASK_OBJECT )
            {
                asITypeInfo* obj_type = engine->GetTypeInfoById( type_id );
                RUNTIME_ASSERT( obj_type );
                if( Str::Compare( obj_type->GetName(), "array" ) )
                {
                    if( obj_type->GetSubTypeId( 0 ) & asTYPEID_MASK_OBJECT && !Str::Compare( obj_type->GetSubType( 0 )->GetName(), "string" ) )
                    {
                        WriteLog( "Invalid type '{}' in array in 'rpc' pragma '{}'.\n", obj_type->GetSubType( 0 )->GetName(), text );
                        return false;
                    }
                }
                else if( Str::Compare( obj_type->GetName(), "dict" ) )
                {
                    if( obj_type->GetSubTypeId( 0 ) & asTYPEID_MASK_OBJECT && !Str::Compare( obj_type->GetSubType( 0 )->GetName(), "string" ) )
                    {
                        WriteLog( "Invalid type '{}' in dict key in 'rpc' pragma '{}'.\n", obj_type->GetSubType( 0 )->GetName(), text );
                        return false;
                    }
                    if( obj_type->GetSubTypeId( 1 ) & asTYPEID_MASK_OBJECT && !Str::Compare( obj_type->GetSubType( 1 )->GetName(), "string" ) )
                    {
                        WriteLog( "Invalid type '{}' in dict value in 'rpc' pragma '{}'.\n", obj_type->GetSubType( 1 )->GetName(), text );
                        return false;
                    }
                }
                else if( !Str::Compare( obj_type->GetName(), "string" ) )
                {
                    WriteLog( "Invalid type '{}' in 'rpc' pragma '{}'.\n", obj_type->GetName(), text );
                    return false;
                }
            }
        }

        // Register
        if( ( pragmaType == PRAGMA_SERVER ) == ( type == "Client" ) )
        {
            string method_name = _str( "void {}({}) const", name, args );
            if( engine->RegisterObjectMethod( "RpcCaller", method_name.c_str(),
                                              asFUNCTION( Rpc ), asCALL_GENERIC, new uint( curOutIndex++ ) ) < 0 )
            {
                WriteLog( "Invalid method '{}' in 'rpc' pragma '{}'.\n", method_name, text );
                return false;
            }
        }
        else
        {
            string func_name = _str( "{}::{}", cur_file, name );
            inFuncDesc.push_back( std::make_pair( func_name, string( args ) ) );
        }

        return true;
    }

    bool Finish()
    {
        uint errors = 0;

        #if defined ( FONLINE_SERVER ) || defined ( FONLINE_CLIENT )
        for( auto& func_desc : inFuncDesc )
        {
            string decl = _str( "void {}({}{}{})", "%s", pragmaType == PRAGMA_SERVER ? "Critter" : "",
                                pragmaType == PRAGMA_SERVER && func_desc.second.length() > 0 ? ", " : "", func_desc.second );
            uint bind_id = Script::BindByFuncName( func_desc.first, decl, false, false );
            if( bind_id )
            {
                inFunc.push_back( Script::GetBindFunc( bind_id ) );
                inFuncBind.push_back( bind_id );
            }
            else
            {
                WriteLog( "Can't bind Rpc function '{}'.\n", func_desc.first );
                errors++;
            }
        }
        #endif

        return errors == 0;
    }

    static void Rpc( asIScriptGeneric* gen )
    {
        #if defined ( FONLINE_SERVER ) || defined ( FONLINE_CLIENT )
        uint msg = NETMSG_RPC;
        uint func_index = *(uint*) gen->GetAuxiliary();
        uint msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( func_index );

        asIScriptEngine* engine = Script::GetEngine();
        StrVec args( gen->GetArgCount() );
        for( int i = 0; i < gen->GetArgCount(); i++ )
        {
            int type_id = gen->GetArgTypeId( i );
            asITypeInfo* obj_type = ( type_id & asTYPEID_MASK_OBJECT ? engine->GetTypeInfoById( type_id ) : nullptr );
            bool is_hashes[] = { false, false, false, false };
            void* ptr = ( type_id & asTYPEID_MASK_OBJECT ? gen->GetArgObject( i ) : gen->GetAddressOfArg( i ) );
            args[ i ] = WriteValue( ptr, type_id, obj_type, is_hashes, 0 );
            msg_len += sizeof( ushort ) + (ushort) args[ i ].size();

            if( args[ i ].size() > 0xFFFF )
                SCRIPT_ERROR_R( "Some of argument is too large (more than 65535 bytes length)" );
        }

        # ifdef FONLINE_SERVER
        Critter* cr = (Critter*) gen->GetObject();
        if( !cr->IsPlayer() )
        {
            SCRIPT_ERROR_R( "Critter is not player" );
            return;
        }

        Client* cl = (Client*) cr;
        BufferManager& net_buf = cl->Connection->Bout;
        # else
        BufferManager& net_buf = FOClient::Self->Bout;
        # endif

        # ifdef FONLINE_SERVER
        BOUT_BEGIN( cl );
        # endif
        net_buf << NETMSG_RPC;
        net_buf << msg_len;
        net_buf << func_index;
        for( size_t i = 0; i < args.size(); i++ )
        {
            ushort len = (ushort) args[ i ].size();
            net_buf << len;
            if( len )
                net_buf.Push( &args[ i ][ 0 ], len );
        }
        # ifdef FONLINE_SERVER
        BOUT_END( cl );
        # endif
        #endif
    }

    void HandleRpc( void* context )
    {
        #if defined ( FONLINE_SERVER ) || defined ( FONLINE_CLIENT )
        # ifdef FONLINE_SERVER
        Client* cl = (Client*) context;
        BufferManager& net_buf = cl->Connection->Bin;
        # else
        BufferManager& net_buf = *(BufferManager*) context;
        # endif

        uint msg_len;
        uint func_index;
        net_buf >> msg_len;
        net_buf >> func_index;
        if( net_buf.IsError() )
            return;

        if( func_index >= (uint) inFunc.size() )
        {
            net_buf.SetError( true );
            return;
        }

        Script::PrepareContext( inFuncBind[ func_index ], "Rpc" );

        char buf[ 0xFFFF + 1 ];
        uchar pod_buf[ 8 ];
        bool is_error = false;

        asIScriptEngine* engine = Script::GetEngine();
        asIScriptFunction* func = inFunc[ func_index ];
        for( asUINT i = 0; i < func->GetParamCount(); i++ )
        {
            # ifdef FONLINE_SERVER
            if( i == 0 )
            {
                Script::SetArgEntity( cl );
                continue;
            }
            # endif

            ushort len;
            net_buf >> len;
            if( net_buf.IsError() )
                return;
            net_buf.Pop( buf, len );
            if( net_buf.IsError() )
                return;
            buf[ len ] = 0;

            int type_id;
            asDWORD flags;
            func->GetParam( i, &type_id, &flags );
            asITypeInfo* obj_type = ( type_id & asTYPEID_MASK_OBJECT ? engine->GetTypeInfoById( type_id ) : nullptr );
            bool is_hashes[] = { false, false, false, false };
            void* value = ReadValue( buf, type_id, obj_type, is_hashes, 0, pod_buf, is_error );
            if( is_error )
            {
                net_buf.SetError( true );
                return;
            }

            if( !( type_id & asTYPEID_MASK_OBJECT ) )
            {
                int size = engine->GetSizeOfPrimitiveType( type_id );
                if( size == 1 )
                    Script::SetArgUChar( *(uchar*) value );
                else if( size == 2 )
                    Script::SetArgUShort( *(ushort*) value );
                else if( size == 4 )
                    Script::SetArgUInt( *(uint*) value );
                else if( size == 8 )
                    Script::SetArgUInt64( *(uint64*) value );
                else
                    RUNTIME_ASSERT( !"Unreachable place" );
            }
            else
            {
                Script::SetArgObject( value );
                Script::GetEngine()->ReleaseScriptObject( value, obj_type );
            }
        }

        Script::RunPrepared();
        #endif
    }
};

ScriptPragmaCallback::ScriptPragmaCallback( int pragma_type )
{
    isError = false;
    pragmaType = pragma_type;
    if( pragmaType != PRAGMA_SERVER && pragmaType != PRAGMA_CLIENT && pragmaType != PRAGMA_MAPPER )
        pragmaType = PRAGMA_UNKNOWN;

    ignorePragma = nullptr;
    globalVarPragma = nullptr;
    bindFuncPragma = nullptr;
    entityPragma = nullptr;
    propertyPragma = nullptr;
    methodPragma = nullptr;
    contentPragma = nullptr;
    enumPragma = nullptr;
    eventPragma = nullptr;
    rpcPragma = nullptr;

    if( pragmaType != PRAGMA_UNKNOWN )
    {
        ignorePragma = new IgnorePragma();
        globalVarPragma = new GlobalVarPragma();
        bindFuncPragma = new BindFuncPragma();
        entityPragma = new EntityPragma( pragmaType );
        propertyPragma = new PropertyPragma( pragmaType, entityPragma );
        methodPragma = new MethodPragma( pragmaType, entityPragma );
        contentPragma = new ContentPragma();
        enumPragma = new EnumPragma();
        eventPragma = new EventPragma();
        rpcPragma = new RpcPragma( pragmaType );
    }
}

ScriptPragmaCallback::~ScriptPragmaCallback()
{
    SAFEDEL( ignorePragma );
    SAFEDEL( globalVarPragma );
    SAFEDEL( bindFuncPragma );
    SAFEDEL( entityPragma );
    SAFEDEL( propertyPragma );
    SAFEDEL( methodPragma );
    SAFEDEL( contentPragma );
    SAFEDEL( enumPragma );
    SAFEDEL( eventPragma );
}

void ScriptPragmaCallback::CallPragma( const Preprocessor::PragmaInstance& pragma )
{
    if( ignorePragma && ignorePragma->IsIgnored( pragma.Name ) )
        return;

    bool ok = false;
    if( _str( pragma.Name ).compareIgnoreCase( "ignore" ) && ignorePragma )
        ok = ignorePragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "globalvar" ) && globalVarPragma )
        ok = globalVarPragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "bindfunc" ) && bindFuncPragma )
        ok = bindFuncPragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "property" ) && propertyPragma )
        ok = propertyPragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "method" ) && methodPragma )
        ok = methodPragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "entity" ) && entityPragma )
        ok = entityPragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "content" ) && contentPragma )
        ok = contentPragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "enum" ) && enumPragma )
        ok = enumPragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "event" ) && eventPragma )
        ok = eventPragma->Call( pragma.Text );
    else if( _str( pragma.Name ).compareIgnoreCase( "rpc" ) && rpcPragma )
        ok = rpcPragma->Call( pragma.Text, pragma.CurrentFile );
    else
        WriteLog( "Unknown pragma instance, name '{}' text '{}'.\n", pragma.Name, pragma.Text ), ok = false;

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
    if( !entityPragma->Finish() )
        isError = true;
    if( !propertyPragma->Finish() )
        isError = true;
    if( !methodPragma->Finish() )
        isError = true;
    if( !contentPragma->Finish() )
        isError = true;
    if( !rpcPragma->Finish() )
        isError = true;
}

bool ScriptPragmaCallback::IsError()
{
    return isError;
}

PropertyRegistrator** ScriptPragmaCallback::GetPropertyRegistrators()
{
    return propertyPragma->GetPropertyRegistrators();
}

StrVec ScriptPragmaCallback::GetCustomEntityTypes()
{
    return entityPragma->GetTypeNames();
}

#ifdef FONLINE_SERVER
bool ScriptPragmaCallback::RestoreCustomEntity( const string& class_name, uint id, const DataBase::Document& doc )
{
    return entityPragma->RestoreEntity( class_name, id, doc );
}
#endif

void* ScriptPragmaCallback::FindInternalEvent( const string& event_name )
{
    return eventPragma->Find( event_name );
}

bool ScriptPragmaCallback::RaiseInternalEvent( void* event_ptr, va_list args )
{
    return eventPragma->Raise( event_ptr, args );
}

void ScriptPragmaCallback::RemoveEventsEntity( Entity* entity )
{
    eventPragma->RemoveEntity( entity );
}

void ScriptPragmaCallback::HandleRpc( void* context )
{
    rpcPragma->HandleRpc( context );
}
