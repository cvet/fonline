#include "Properties.h"
#include "FileSystem.h"
#include "Script.h"

PropertyAccessor::PropertyAccessor( uint index )
{
    propertyIndex = index;
    getCallbackLocked = false;
    setCallbackLocked = false;
}

void PropertyAccessor::GenericGet( void* obj, void* ret_value )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];

    // Virtual property
    if( prop->Access & Property::VirtualMask )
    {
        if( !prop->GetCallback )
        {
            memzero( ret_value, prop->Size );
            SCRIPT_ERROR_R( "'Get' callback is not assigned for virtual property '%s %s::%s'.",
                            prop->TypeName.c_str(), properties->registrator->scriptClassName.c_str(), prop->Name.c_str() );
            return;
        }

        if( getCallbackLocked )
        {
            memzero( ret_value, prop->Size );
            SCRIPT_ERROR_R( "Recursive call for virtual property '%s %s::%s'.",
                            prop->TypeName.c_str(), properties->registrator->scriptClassName.c_str(), prop->Name.c_str() );
            return;
        }

        getCallbackLocked = true;

        #ifndef FONLINE_SCRIPT_COMPILER
        if( Script::PrepareContext( prop->GetCallback, _FUNC_, "" ) )
        {
            Script::SetArgObject( obj );
            if( Script::RunPrepared() )
            {
                getCallbackLocked = false;
                memcpy( ret_value, Script::GetReturnedRawAddress(), prop->Size );
                return;
            }
        }
        #endif

        getCallbackLocked = false;

        // Error, return zero
        memzero( ret_value, prop->Size );
        return;
    }

    // Raw property
    RUNTIME_ASSERT( prop->Offset != uint( -1 ) );
    memcpy( ret_value, &properties->propertiesData[ prop->Offset ], prop->Size );
}

void PropertyAccessor::GenericSet( void* obj, void* new_value )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];
    RUNTIME_ASSERT( prop->Offset != uint( -1 ) );
    void*       cur_value = &properties->propertiesData[ prop->Offset ];

    // Clamp
    if( prop->CheckMinValue || prop->CheckMaxValue )
    {
        uint64 check_value = 0;
        if( prop->Size == 1 )
            check_value = *(uchar*) new_value;
        else if( prop->Size == 2 )
            check_value = *(ushort*) new_value;
        else if( prop->Size == 4 )
            check_value = *(uint*) new_value;
        else if( prop->Size == 8 )
            check_value = *(uint64*) new_value;

        if( prop->CheckMinValue && check_value < (uint64) prop->MinValue )
        {
            check_value = prop->MinValue;
            GenericSet( obj, &check_value );
            return;
        }
        else if( prop->CheckMaxValue && check_value > (uint64) prop->MaxValue )
        {
            check_value = prop->MaxValue;
            GenericSet( obj, &check_value );
            return;
        }
    }

    // Ignore void calls
    if( !memcmp( new_value, cur_value, prop->Size ) )
        return;

    // Store old value
    uint64 old_value = 0;
    memcpy( &old_value, cur_value, prop->Size );

    // Assign value
    memcpy( cur_value, new_value, prop->Size );

    // Set callbacks
    if( !prop->SetCallbacks.empty() && !setCallbackLocked )
    {
        setCallbackLocked = true;

        for( size_t i = 0; i < prop->SetCallbacks.size(); i++ )
        {
            #ifndef FONLINE_SCRIPT_COMPILER
            if( Script::PrepareContext( prop->SetCallbacks[ i ], _FUNC_, "" ) )
            {
                Script::SetArgObject( obj );
                Script::SetArgAddress( &old_value );
                Script::RunPrepared();
            }
            #endif
        }

        setCallbackLocked = false;
    }

    // Native set callback
    if( prop->NativeSetCallback && !setCallbackLocked && memcmp( cur_value, &old_value, prop->Size ) )
        prop->NativeSetCallback( obj, prop, cur_value, &old_value );

    // Native send callback
    if( prop->NativeSendCallback && obj != sendIgnoreObj && !setCallbackLocked && memcmp( cur_value, &old_value, prop->Size ) )
    {
        if( ( properties->registrator->isServer && ( prop->Access & ( Property::PublicMask | Property::ProtectedMask ) ) ) ||
            ( !properties->registrator->isServer && ( prop->Access & Property::ModifiableMask ) ) )
        {
            prop->NativeSendCallback( obj, prop, cur_value, &old_value );
        }
    }
}

void PropertyAccessor::SetSendIgnore( void* obj )
{
    sendIgnoreObj = obj;
}

Properties::Properties( PropertyRegistrator* reg )
{
    registrator = reg;
    RUNTIME_ASSERT( registrator );
    RUNTIME_ASSERT( registrator->registrationFinished );

    if( !registrator->propertiesDataPool.empty() )
    {
        propertiesData = registrator->propertiesDataPool.back();
        registrator->propertiesDataPool.pop_back();
    }
    else
    {
        propertiesData = new uchar[ registrator->wholeDataSize ];
    }
    memzero( propertiesData, registrator->wholeDataSize );

    // Set default values
    for( size_t i = 0, j = registrator->registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->Offset == uint( -1 ) )
            continue;

        if( prop->SetDefaultValue )
        {
            memcpy( &propertiesData[ prop->Offset ], &prop->DefaultValue, prop->Size );
        }
        else if( prop->GenerateRandomValue )
        {
            // Todo: fix min/max
            for( uint i = 0; i < prop->Size; i++ )
                propertiesData[ prop->Offset + i ] = Random( 0, 255 );
        }
    }
}

Properties::~Properties()
{
    registrator->propertiesDataPool.push_back( propertiesData );
    for( size_t i = 0; i < unresolvedProperties.size(); i++ )
        delete unresolvedProperties[ i ];
    unresolvedProperties.clear();
}

Properties& Properties::operator=( const Properties& other )
{
    RUNTIME_ASSERT( registrator == other.registrator );
    memcpy( &propertiesData[ 0 ], &other.propertiesData[ 0 ], registrator->wholeDataSize );
    return *this;
}

void* Properties::FindData( const char* property_name )
{
    Property* prop = registrator->Find( property_name );
    RUNTIME_ASSERT( prop );
    RUNTIME_ASSERT( prop->Offset != uint( -1 ) );
    return prop ? &propertiesData[ prop->Offset ] : NULL;
}

uchar* Properties::StoreData( bool with_protected, uint& size )
{
    size = (uint) registrator->publicDataSpace.size() + ( with_protected ? (uint) registrator->protectedDataSpace.size() : 0 );
    return size ? &propertiesData[ 0 ] : NULL;
}

void Properties::RestoreData( const UCharVec* properties_data )
{
    if( properties_data && !properties_data->empty() )
    {
        memcpy( &propertiesData[ 0 ], &properties_data->at( 0 ), properties_data->size() );
        memzero( &propertiesData[ properties_data->size() ], registrator->wholeDataSize - properties_data->size() );
    }
    else
    {
        memzero( propertiesData, registrator->wholeDataSize );
    }
}

void Properties::Save( void ( * save_func )( void*, size_t ) )
{
    RUNTIME_ASSERT( registrator->registrationFinished );

    uint count = (uint) registrator->registeredProperties.size();
    save_func( &count, sizeof( count ) );
    for( size_t i = 0, j = registrator->registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->Offset == uint( -1 ) )
            continue;

        ushort name_len = (ushort) prop->Name.length();
        save_func( &name_len, sizeof( name_len ) );
        save_func( (void*) prop->Name.c_str(), name_len );
        uchar size = (uchar) prop->Size;
        save_func( &size, sizeof( size ) );
        save_func( &propertiesData[ prop->Offset ], prop->Size );
    }
}

void Properties::Load( void* file, uint version )
{
    RUNTIME_ASSERT( registrator->registrationFinished );

    uint count = 0;
    FileRead( file, &count, sizeof( count ) );
    for( uint i = 0; i < count; i++ )
    {
        ushort name_len = 0;
        FileRead( file, &name_len, sizeof( name_len ) );
        char   name[ MAX_FOTEXT ];
        FileRead( file, name, name_len );
        name[ name_len ] = 0;
        uchar  size = 0;
        FileRead( file, &size, sizeof( size ) );
        uint64 value = 0;
        FileRead( file, &value, size );

        Property* prop = registrator->Find( name );
        if( prop && prop->Offset != uint( -1 ) )
        {
            void* data = &propertiesData[ prop->Offset ];
            memzero( data, prop->Size );
            memcpy( data, &value, MIN( size, prop->Size ) );
        }
        else
        {
            UnresolvedProperty* unresolved_property = new UnresolvedProperty();
            unresolved_property->Name = name;
            unresolved_property->Size = size;
            unresolved_property->Value = value;
            unresolvedProperties.push_back( unresolved_property );
        }
    }
}

PropertyRegistrator::PropertyRegistrator( bool is_server, const char* script_class_name )
{
    registrationFinished = false;
    isServer = is_server;
    scriptClassName = script_class_name;
    wholeDataSize = 0;
}

PropertyRegistrator::~PropertyRegistrator()
{
    scriptClassName = "";
    wholeDataSize = 0;

    for( size_t i = 0; i < registeredProperties.size(); i++ )
    {
        delete registeredProperties[ i ]->Accessor;
        delete registeredProperties[ i ];
    }
    registeredProperties.clear();

    for( size_t i = 0; i < propertiesDataPool.size(); i++ )
        delete[] propertiesDataPool[ i ];
    propertiesDataPool.clear();
}

Property* PropertyRegistrator::Register(
    const char* type_name,
    const char* name,
    Property::AccessType access,
    bool generate_random_value /* = false */,
    int64* default_value /* = NULL */,
    int64* min_value /* = NULL */,
    int64* max_value /* = NULL */
    )
{
    if( registrationFinished )
    {
        WriteLogF( _FUNC_, " - Registration of class properties is finished.\n" );
        return NULL;
    }

    asIScriptEngine* engine = Script::GetEngine();
    RUNTIME_ASSERT( engine );

    // Check primitive
    int primitive_size = engine->GetSizeOfPrimitiveType( engine->GetTypeIdByDecl( type_name ) );
    if( primitive_size <= 0 )
    {
        WriteLogF( _FUNC_, " - Invalid property type<%s>.\n", type_name );
        return NULL;
    }
    uint property_data_size = (uint) primitive_size;
    if( property_data_size != 1 && property_data_size != 2 && property_data_size != 4 && property_data_size != 8 )
    {
        WriteLogF( _FUNC_, " - Invalid size of property type<%s>, size<%d>.\n", type_name, property_data_size );
        return NULL;
    }

    // Check name for already used
    asIObjectType* ot = engine->GetObjectTypeByName( scriptClassName.c_str() );
    RUNTIME_ASSERT( ot );
    for( asUINT i = 0, j = ot->GetPropertyCount(); i < j; i++ )
    {
        const char* n;
        ot->GetProperty( i, &n );
        if( Str::Compare( n, name ) )
        {
            WriteLogF( _FUNC_, " - Trying to register already registered property<%s>.\n", name );
            return NULL;
        }
    }

    // Cerate accessor
    uint              property_index = (uint) registeredProperties.size();
    PropertyAccessor* property_accessor = new PropertyAccessor( property_index );

    // Disallow set or get accessors
    bool disable_get = false;
    bool disable_set = false;
    if( access & Property::VirtualMask )
        disable_set = true;
    if( isServer && ( access & Property::ClientMask ) )
        disable_get = disable_set = true;
    if( !isServer && ( access & Property::ServerMask ) )
        disable_get = disable_set = true;

    // Register default getter
    if( !disable_get )
    {
        char decl[ MAX_FOTEXT ];
        Str::Format( decl, "%s get_%s() const", type_name, name );
        int  result = -1;
        if( property_data_size == 1 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Get< uchar >, (void*), uchar ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( property_data_size == 2 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Get< ushort >, (void*), ushort ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( property_data_size == 4 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Get< uint >, (void*), uint ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( property_data_size == 8 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Get< uint64 >, (void*), uint64 ), asCALL_THISCALL_OBJFIRST, property_accessor );
        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Register object property<%s> getter fail, error<%d>.\n", name, result );
            delete property_accessor;
            return NULL;
        }
    }

    // Register setter
    if( !disable_set )
    {
        char decl[ MAX_FOTEXT ];
        Str::Format( decl, "void set_%s(%s)", name, type_name );
        int  result = -1;
        if( property_data_size == 1 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< uchar >, ( void*, uchar ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( property_data_size == 2 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< ushort >, ( void*, ushort ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( property_data_size == 4 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< uint >, ( void*, uint ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( property_data_size == 8 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< uint64 >, ( void*, uint64 ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Register object property<%s> setter fail, error<%d>.\n", name, result );
            delete property_accessor;
            return NULL;
        }
    }

    // Calculate property data offset
    uint property_data_base_offset = uint( -1 );
    if( !disable_set )
    {
        bool     is_public = ( access & Property::PublicMask ) != 0;
        bool     is_protected = ( access & Property::ProtectedMask ) != 0;
        BoolVec& space = ( is_public ? publicDataSpace : ( is_protected ? protectedDataSpace : privateDataSpace ) );

        uint     space_size = (uint) space.size();
        uint     space_pos = 0;
        while( space_pos + property_data_size <= space_size )
        {
            bool fail = false;
            for( uint i = 0; i < property_data_size; i++ )
            {
                if( space[ space_pos + i ] )
                {
                    fail = true;
                    break;
                }
            }
            if( !fail )
                break;
            space_pos += property_data_size;
        }

        uint new_size = space_pos + property_data_size;
        new_size += 8 - new_size % 8;
        if( new_size > (uint) space.size() )
            space.resize( new_size );

        for( uint i = 0; i < property_data_size; i++ )
            space[ space_pos + i ] = true;

        property_data_base_offset = space_pos;

        wholeDataSize = (uint) ( publicDataSpace.size() + protectedDataSpace.size() + privateDataSpace.size() );
        RUNTIME_ASSERT( !( wholeDataSize % 8 ) );
    }

    // Make entry
    Property* prop = new Property();

    prop->Index = property_index;
    prop->Offset = property_data_base_offset;
    prop->Size = property_data_size;
    prop->Accessor = property_accessor;
    prop->GetCallback = 0;
    prop->NativeSetCallback = NULL;

    prop->Name = name;
    prop->TypeName = type_name;
    prop->Access = access;
    prop->GenerateRandomValue = generate_random_value;
    prop->SetDefaultValue = ( default_value != NULL );
    prop->CheckMinValue = ( min_value != NULL );
    prop->CheckMaxValue = ( max_value != NULL );
    prop->DefaultValue = ( default_value ? *default_value : 0 );
    prop->MinValue = ( min_value ? *min_value : 0 );
    prop->MaxValue = ( max_value ? *max_value : 0 );

    registeredProperties.push_back( prop );

    return prop;
}

void PropertyRegistrator::FinishRegistration()
{
    RUNTIME_ASSERT( !registrationFinished );
    registrationFinished = true;

    for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registeredProperties[ i ];
        if( prop->Offset == uint( -1 ) )
            continue;

        if( prop->Access & Property::ProtectedMask )
            prop->Offset += (uint) publicDataSpace.size();
        else if( prop->Access & Property::PrivateMask )
            prop->Offset += (uint) publicDataSpace.size() + (uint) protectedDataSpace.size();
    }
}

Property* PropertyRegistrator::Get( uint property_index )
{
    if( property_index < (uint) registeredProperties.size() )
        return registeredProperties[ property_index ];
    return NULL;
}

Property* PropertyRegistrator::Find( const char* property_name )
{
    for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
        if( Str::Compare( property_name, registeredProperties[ i ]->Name.c_str() ) )
            return registeredProperties[ i ];
    return NULL;
}

string PropertyRegistrator::SetGetCallback( const char* property_name, const char* script_func )
{
    #ifndef FONLINE_SCRIPT_COMPILER
    Property* prop = Find( property_name );
    if( !prop )
        return "Property '" + string( property_name ) + "' in class '" + scriptClassName + "' not found.";

    char decl[ MAX_FOTEXT ];
    Str::Format( decl, "%s %s(%s&)", prop->TypeName.c_str(), "%s", scriptClassName.c_str() );

    int bind_id = Script::Bind( script_func, decl, false );
    if( bind_id <= 0 )
    {
        char buf[ MAX_FOTEXT ];
        Str::Format( buf, decl, script_func );
        return "Unable to bind function '" + string( buf ) + "'.";
    }

    prop->GetCallback = bind_id;
    #endif
    return "";
}

string PropertyRegistrator::AddSetCallback( const char* property_name, const char* script_func )
{
    #ifndef FONLINE_SCRIPT_COMPILER
    Property* prop = Find( property_name );
    if( !prop )
        return "Property '" + string( property_name ) + "' in class '" + scriptClassName + "' not found.";

    char decl[ MAX_FOTEXT ];
    Str::Format( decl, "void %s(%s&,%s)", "%s", scriptClassName.c_str(), prop->TypeName.c_str() );

    int bind_id = Script::Bind( script_func, decl, false );
    if( bind_id <= 0 )
    {
        char buf[ MAX_FOTEXT ];
        Str::Format( buf, decl, script_func );
        return "Unable to bind function '" + string( buf ) + "'.";
    }

    prop->SetCallbacks.push_back( bind_id );
    #endif
    return "";
}

void PropertyRegistrator::SetNativeSetCallback( const char* property_name, NativeCallback callback )
{
    if( !property_name )
    {
        for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
            registeredProperties[ i ]->NativeSetCallback = callback;
    }
    else
    {
        Find( property_name )->NativeSetCallback = callback;
    }
}

void PropertyRegistrator::SetNativeSendCallback( NativeCallback callback )
{
    for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
        registeredProperties[ i ]->NativeSendCallback = callback;
}

uint PropertyRegistrator::GetWholeDataSize()
{
    return wholeDataSize;
}
