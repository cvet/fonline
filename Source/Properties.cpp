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
            #ifndef FONLINE_SCRIPT_COMPILER
            Script::RaiseException( "'Get' callback is not assigned for virtual property '%s %s::%s'",
                                    prop->TypeName.c_str(), properties->registrator->scriptClassName.c_str(), prop->Name.c_str() );
            #endif
            memzero( ret_value, prop->Size );
            return;
        }

        if( getCallbackLocked )
        {
            #ifndef FONLINE_SCRIPT_COMPILER
            Script::RaiseException( "Recursive call for virtual property '%s %s::%s'",
                                    prop->TypeName.c_str(), properties->registrator->scriptClassName.c_str(), prop->Name.c_str() );
            #endif
            memzero( ret_value, prop->Size );
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
    memcpy( ret_value, &properties->propertiesData[ prop->Offset ], prop->Size );
}

void PropertyAccessor::GenericSet( void* obj, void* new_value )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];
    RUNTIME_ASSERT( !( prop->Access & Property::VirtualMask ) );
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
        if( ( properties->registrator->isServer && ( prop->Access & Property::PublicProtectedMask ) ) ||
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
    RUNTIME_ASSERT( registrator->RegistrationFinished );

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
        if( prop->Access & Property::VirtualMask )
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
    RUNTIME_ASSERT( !( prop->Access & Property::VirtualMask ) );
    return prop ? &propertiesData[ prop->Offset ] : NULL;
}

UCharVec* Properties::StoreData( Property::AccessType access_mask )
{
    tmpStoreData.clear();

    for( size_t i = 0, j = registrator->registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( !( prop->Access & access_mask ) )
            continue;

        uint   pos = (uint) tmpStoreData.size();
        tmpStoreData.resize( tmpStoreData.size() + sizeof( ushort ) + prop->Size );
        uchar* data = &tmpStoreData[ pos ];
        *(ushort*) data = (ushort) prop->Index;
        memcpy( data + sizeof( ushort ), &propertiesData[ prop->Offset ], prop->Size );
    }

    return !tmpStoreData.empty() ? &tmpStoreData : NULL;
}

void Properties::RestoreData( const UCharVec* properties_data )
{
    memzero( propertiesData, registrator->wholeDataSize );
    if( !properties_data || properties_data->empty() )
        return;

    const uchar* data = &properties_data->at( 0 );
    uint         size = (uint) properties_data->size();
    const uchar* end_data = data + size;
    while( data < end_data )
    {
        uint      property_index = *(ushort*) data;
        data += sizeof( ushort );
        Property* prop = registrator->registeredProperties[ property_index ];
        RUNTIME_ASSERT( !( prop->Access & Property::VirtualMask ) );
        memcpy( &propertiesData[ prop->Offset ], data, prop->Size );
        data += prop->Size;
    }
}

void Properties::Save( void ( * save_func )( void*, size_t ) )
{
    RUNTIME_ASSERT( registrator->RegistrationFinished );

    uint count = (uint) registrator->registeredProperties.size();
    save_func( &count, sizeof( count ) );
    for( size_t i = 0, j = registrator->registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->Access & Property::VirtualMask )
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
    RUNTIME_ASSERT( registrator->RegistrationFinished );

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
        if( prop && !( prop->Access & Property::VirtualMask ) )
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
    RegistrationFinished = false;
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
    if( RegistrationFinished )
    {
        WriteLogF( _FUNC_, " - Registration of class properties is finished.\n" );
        return NULL;
    }

    asIScriptEngine* engine = Script::GetEngine();
    RUNTIME_ASSERT( engine );

    // Check primitive
    int property_data_size = engine->GetSizeOfPrimitiveType( engine->GetTypeIdByDecl( type_name ) );
    if( property_data_size <= 0 )
    {
        WriteLogF( _FUNC_, " - Invalid property type<%s>.\n", type_name );
        return NULL;
    }
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

    // Disallow client set accessors
    bool disable_set = false;
    if( access & Property::VirtualMask )
        disable_set = true;
    if( isServer && ( access & Property::ClientMask ) )
        disable_set = true;
    if( !isServer && ( access & Property::ServerMask ) )
        disable_set = true;

    // Cerate accessor
    uint              property_index = (uint) registeredProperties.size();
    PropertyAccessor* property_accessor = new PropertyAccessor( property_index );

    // Register default getter
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

    // Register setter
    if( !disable_set )
    {
        Str::Format( decl, "void set_%s(%s)", name, type_name );
        result = -1;
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
    uint property_data_offset = 0;
    if( !( access & Property::VirtualMask ) )
    {
        property_data_offset = ( registeredProperties.empty() ? 0 : registeredProperties.back()->Offset + registeredProperties.back()->Size );
        property_data_offset += property_data_offset % property_data_size;
        wholeDataSize = property_data_offset + property_data_size;
        wholeDataSize += wholeDataSize % 8;
    }

    // Make entry
    Property* prop = new Property();

    prop->Index = property_index;
    prop->Offset = property_data_offset;
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

bool PropertyRegistrator::SetGetCallback( const char* property_name, const char* script )
{
    return false;
}

bool PropertyRegistrator::AddSetCallback( const char* property_name, const char* script )
{
    return false;
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
