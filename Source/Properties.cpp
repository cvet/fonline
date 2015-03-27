#include "Properties.h"
#include "FileSystem.h"
#include "Script.h"

PropertyAccessor::PropertyAccessor( uint index )
{
    propertyIndex = index;
    getCallbackLocked = false;
    setCallbackLocked = false;
}

void* PropertyAccessor::CreateComplexValue( Property* prop, uchar* data, uint data_size )
{
    if( prop->Type == Property::String )
    {
        return ScriptString::Create( data_size ? (char*) data : "", data_size );
    }
    else if( prop->Type == Property::Array )
    {
        asUINT       arr_size = data_size / prop->ComplexDataSubType->GetSize();
        ScriptArray* arr = ScriptArray::Create( prop->ComplexDataSubType, arr_size );
        if( arr_size )
            memcpy( arr->At( 0 ), data, arr_size * prop->ComplexDataSubType->GetSize() );
        return arr;
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }
    return NULL;
}

uchar* PropertyAccessor::ExpandComplexValueData( Property* prop, void* base_ptr, uint& data_size, bool& need_delete )
{
    need_delete = false;
    if( prop->Type == Property::String )
    {
        ScriptString* str = *(ScriptString**) base_ptr;
        data_size = ( str ? str->length() : 0 );
        return data_size ? (uchar*) str->c_str() : NULL;
    }
    else if( prop->Type == Property::Array )
    {
        ScriptArray* arr = *(ScriptArray**) base_ptr;
        data_size = ( arr ? arr->GetSize() * arr->GetElementSize() : 0 );
        return data_size ? (uchar*) arr->At( 0 ) : NULL;
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }
    return NULL;
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

        // Error
        #ifndef FONLINE_SCRIPT_COMPILER
        Script::RaiseException( "Error in Get callback execution for virtual property '%s %s::%s'",
                                prop->TypeName.c_str(), properties->registrator->scriptClassName.c_str(), prop->Name.c_str() );
        #endif
        memzero( ret_value, prop->Size );
        return;
    }

    // Raw property
    if( prop->Type == Property::POD )
    {
        RUNTIME_ASSERT( prop->PODDataOffset != uint( -1 ) );
        memcpy( ret_value, &properties->podData[ prop->PODDataOffset ], prop->Size );
    }
    else
    {
        RUNTIME_ASSERT( prop->ComplexDataIndex != uint( -1 ) );
        uchar* data = properties->complexData[ prop->ComplexDataIndex ];
        uint   data_size = properties->complexDataSizes[ prop->ComplexDataIndex ];
        void*  result = CreateComplexValue( prop, data, data_size );
        memcpy( ret_value, &result, prop->Size );
    }
}

void PropertyAccessor::GenericSet( void* obj, void* new_value )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];

    // Get current POD value
    void* cur_value;
    if( prop->Type == Property::POD )
    {
        RUNTIME_ASSERT( prop->PODDataOffset != uint( -1 ) );
        cur_value = &properties->podData[ prop->PODDataOffset ];

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
    }

    // Get current complex value
    void* complex_value = NULL;
    if( prop->Type != Property::POD )
    {
        RUNTIME_ASSERT( prop->ComplexDataIndex != uint( -1 ) );

        // Expand new value data for comparison
        bool  need_delete;
        uint  new_value_data_size;
        void* new_value_data = ExpandComplexValueData( prop, new_value, new_value_data_size, need_delete );

        // Get current data for comparison
        uint  cur_value_data_size = properties->complexDataSizes[ prop->ComplexDataIndex ];
        void* cur_value_data = properties->complexData[ prop->ComplexDataIndex ];

        // Compare for ignore void calls
        if( new_value_data_size == cur_value_data_size && ( cur_value_data_size == 0 || !memcmp( new_value_data, cur_value_data, cur_value_data_size ) ) )
            return;

        // Only now create temporary value for use in callbacks as old value
        // Ignore creating complex values for engine callbacks
        if( !setCallbackLocked && !prop->SetCallbacks.empty() )
        {
            uchar* data = properties->complexData[ prop->ComplexDataIndex ];
            uint   data_size = properties->complexDataSizes[ prop->ComplexDataIndex ];
            complex_value = CreateComplexValue( prop, data, data_size );
        }
        cur_value = &complex_value;

        // Copy new property data
        SAFEDELA( properties->complexData[ prop->ComplexDataIndex ] );
        properties->complexDataSizes[ prop->ComplexDataIndex ] = new_value_data_size;
        if( new_value_data_size )
        {
            properties->complexData[ prop->ComplexDataIndex ] = new uchar[ new_value_data_size ];
            memcpy( properties->complexData[ prop->ComplexDataIndex ], new_value_data, new_value_data_size );
        }

        // Deallocate data
        if( need_delete && new_value_data_size )
            SAFEDELA( new_value_data );
    }

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
    if( prop->NativeSetCallback && !setCallbackLocked )
        prop->NativeSetCallback( obj, prop, cur_value, &old_value );

    // Native send callback
    if( prop->NativeSendCallback && obj != sendIgnoreObj && !setCallbackLocked )
    {
        if( ( properties->registrator->isServer && ( prop->Access & ( Property::PublicMask | Property::ProtectedMask ) ) ) ||
            ( !properties->registrator->isServer && ( prop->Access & Property::ModifiableMask ) ) )
        {
            prop->NativeSendCallback( obj, prop, cur_value, &old_value );
        }
    }

    // Reference counting
    if( prop->Type != Property::POD && complex_value )
    {
        if( prop->Type == Property::String )
            ( (ScriptString*) complex_value )->Release();
        else if( prop->Type == Property::Array )
            ( (ScriptArray*) complex_value )->Release();
    }
}

uchar* PropertyAccessor::GetData( void* obj, uint& data_size )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];

    if( prop->Type == Property::POD )
    {
        RUNTIME_ASSERT( prop->PODDataOffset != uint( -1 ) );
        data_size = prop->Size;
        return &properties->podData[ prop->PODDataOffset ];
    }

    RUNTIME_ASSERT( prop->ComplexDataIndex != uint( -1 ) );
    data_size = properties->complexDataSizes[ prop->ComplexDataIndex ];
    return properties->complexData[ prop->ComplexDataIndex ];
}

void PropertyAccessor::SetData( void* obj, uchar* data, uint data_size, bool call_callbacks )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];

    if( !call_callbacks )
        prop->Accessor->setCallbackLocked = true;

    if( prop->Type == Property::POD )
    {
        RUNTIME_ASSERT( data_size == prop->Size );
        GenericSet( obj, data );
    }
    else if( prop->Type == Property::String )
    {
        ScriptString* str = (ScriptString*) CreateComplexValue( prop, data, data_size );
        GenericSet( obj, &str );
        str->Release();
    }
    else if( prop->Type == Property::Array )
    {
        ScriptArray* arr = (ScriptArray*) CreateComplexValue( prop, data, data_size );
        GenericSet( obj, &arr );
        arr->Release();
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }

    if( !call_callbacks )
        prop->Accessor->setCallbackLocked = false;
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

    // Allocate POD data
    if( !registrator->podDataPool.empty() )
    {
        podData = registrator->podDataPool.back();
        registrator->podDataPool.pop_back();
    }
    else
    {
        podData = new uchar[ registrator->wholePodDataSize ];
    }
    memzero( podData, registrator->wholePodDataSize );

    // Complex data
    complexData.resize( registrator->complexPropertiesCount );
    complexDataSizes.resize( registrator->complexPropertiesCount );

    // Set default values
    for( size_t i = 0, j = registrator->registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->PODDataOffset == uint( -1 ) )
            continue;

        // Todo: complex string value

        if( prop->SetDefaultValue )
        {
            memcpy( &podData[ prop->PODDataOffset ], &prop->DefaultValue, prop->Size );
        }
        else if( prop->GenerateRandomValue )
        {
            // Todo: fix min/max
            for( uint i = 0; i < prop->Size; i++ )
                podData[ prop->PODDataOffset + i ] = Random( 0, 255 );
        }
    }
}

Properties::~Properties()
{
    registrator->podDataPool.push_back( podData );
    for( size_t i = 0; i < unresolvedProperties.size(); i++ )
    {
        SAFEDELA( unresolvedProperties[ i ]->Name );
        SAFEDELA( unresolvedProperties[ i ]->TypeName );
        SAFEDELA( unresolvedProperties[ i ]->Data );
        SAFEDEL( unresolvedProperties[ i ] );
    }
    unresolvedProperties.clear();
}

Properties& Properties::operator=( const Properties& other )
{
    RUNTIME_ASSERT( registrator == other.registrator );

    // Copy POD data
    memcpy( &podData[ 0 ], &other.podData[ 0 ], registrator->wholePodDataSize );

    // Copy complex data
    for( size_t i = 0; i < registrator->registeredProperties.size(); i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->Type != Property::POD )
        {
            uint   data_size;
            uchar* data = prop->Accessor->GetData( (void*) &other, data_size );
            prop->Accessor->SetData( this, data, data_size, false );
        }
    }

    return *this;
}

void* Properties::FindData( const char* property_name )
{
    Property* prop = registrator->Find( property_name );
    RUNTIME_ASSERT( prop );
    RUNTIME_ASSERT( prop->PODDataOffset != uint( -1 ) );
    return prop ? &podData[ prop->PODDataOffset ] : NULL;
}

uint Properties::StoreData( bool with_protected, PUCharVec** all_data, UIntVec** all_data_sizes )
{
    uint whole_size = 0;
    *all_data = &storeData;
    *all_data_sizes = &storeDataSizes;
    storeData.resize( 0 );
    storeDataSizes.resize( 0 );
    storeData.reserve( 1 + complexData.size() );
    storeDataSizes.reserve( 1 + complexData.size() );

    // Store POD properties data
    storeData.push_back( podData );
    storeDataSizes.push_back( (uint) registrator->publicPodDataSpace.size() + ( with_protected ? (uint) registrator->protectedPodDataSpace.size() : 0 ) );
    whole_size += storeDataSizes.back();

    // Store complex properties data
    for( size_t i = 0; i < registrator->registeredProperties.size(); i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->Type != Property::POD && ( prop->Access & Property::PublicMask || ( with_protected && prop->Access & Property::ProtectedMask ) ) )
        {
            storeData.push_back( complexData[ prop->ComplexDataIndex ] );
            storeDataSizes.push_back( complexDataSizes[ prop->ComplexDataIndex ] );
            whole_size += storeDataSizes.back();
        }
    }
    return whole_size;
}

void Properties::RestoreData( bool with_protected, const UCharVecVec& all_data )
{
    // Restore POD data
    memcpy( &podData[ 0 ], &all_data[ 0 ][ 0 ], all_data[ 0 ].size() );

    // Restore complex data
    for( size_t i = 0; i < registrator->registeredProperties.size(); i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->Type != Property::POD && ( prop->Access & Property::PublicMask || ( with_protected && prop->Access & Property::ProtectedMask ) ) )
        {
            RUNTIME_ASSERT( prop->ComplexDataIndex < (uint) all_data.size() );
            uint   data_size = (uint) all_data[ prop->ComplexDataIndex ].size();
            uchar* data = ( data_size ? (uchar*) &all_data[ prop->ComplexDataIndex ][ 0 ] : NULL );
            prop->Accessor->SetData( this, data, data_size, false );
        }
    }
}

void Properties::Save( void ( * save_func )( void*, size_t ) )
{
    RUNTIME_ASSERT( registrator->registrationFinished );

    uint count = registrator->serializedPropertiesCount + (uint) unresolvedProperties.size();
    save_func( &count, sizeof( count ) );
    for( size_t i = 0, j = registrator->registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->PODDataOffset == uint( -1 ) && prop->ComplexDataIndex == uint( -1 ) )
            continue;

        ushort name_len = (ushort) prop->Name.length();
        save_func( &name_len, sizeof( name_len ) );
        save_func( (void*) prop->Name.c_str(), name_len );

        uchar type_name_len = (uchar) prop->TypeName.length();
        save_func( &type_name_len, sizeof( type_name_len ) );
        save_func( (void*) prop->TypeName.c_str(), type_name_len );

        uint   data_size;
        uchar* data = prop->Accessor->GetData( this, data_size );
        save_func( &data_size, sizeof( data_size ) );
        if( data_size )
            save_func( data, data_size );
    }
    for( size_t i = 0, j = unresolvedProperties.size(); i < j; i++ )
    {
        UnresolvedProperty* unresolved_property = unresolvedProperties[ i ];

        ushort              name_len = ( ushort ) Str::Length( unresolved_property->Name );
        save_func( &name_len, sizeof( name_len ) );
        save_func( (void*) unresolved_property->Name, name_len );

        uchar type_name_len = ( uchar ) Str::Length( unresolved_property->TypeName );
        save_func( &type_name_len, sizeof( type_name_len ) );
        save_func( (void*) unresolved_property->TypeName, type_name_len );

        uint   data_size = unresolved_property->DataSize;
        uchar* data = unresolved_property->Data;
        save_func( &data_size, sizeof( data_size ) );
        if( data_size )
            save_func( data, data_size );
    }
}

void Properties::Load( void* file, uint version )
{
    RUNTIME_ASSERT( registrator->registrationFinished );

    UCharVec data;
    uint     count = 0;
    FileRead( file, &count, sizeof( count ) );
    for( uint i = 0; i < count; i++ )
    {
        ushort name_len = 0;
        FileRead( file, &name_len, sizeof( name_len ) );
        char   name[ MAX_FOTEXT ];
        FileRead( file, name, name_len );
        name[ name_len ] = 0;

        uchar type_name_len = 0;
        FileRead( file, &type_name_len, sizeof( type_name_len ) );
        char  type_name[ MAX_FOTEXT ];
        FileRead( file, type_name, type_name_len );
        type_name[ type_name_len ] = 0;

        uint data_size = 0;
        FileRead( file, &data_size, sizeof( data_size ) );
        data.resize( data_size );
        if( data_size )
            FileRead( file, &data[ 0 ], data_size );

        Property* prop = registrator->Find( name );
        if( prop && Str::Compare( prop->TypeName.c_str(), type_name ) )
        {
            prop->Accessor->SetData( this, data_size ? &data[ 0 ] : NULL, data_size, false );
        }
        else
        {
            UnresolvedProperty* unresolved_property = new UnresolvedProperty();
            unresolved_property->Name = Str::Duplicate( name );
            unresolved_property->TypeName = Str::Duplicate( type_name );
            unresolved_property->DataSize = data_size;
            unresolved_property->Data = ( data_size ? new uchar[ data_size ] : NULL );
            if( data_size )
                memcpy( unresolved_property->Data, &data[ 0 ], data_size );
            unresolvedProperties.push_back( unresolved_property );
        }
    }
}

PropertyRegistrator::PropertyRegistrator( bool is_server, const char* script_class_name )
{
    registrationFinished = false;
    isServer = is_server;
    scriptClassName = script_class_name;
    wholePodDataSize = 0;
    complexPropertiesCount = 0;
    serializedPropertiesCount = 0;
}

PropertyRegistrator::~PropertyRegistrator()
{
    scriptClassName = "";
    wholePodDataSize = 0;

    for( size_t i = 0; i < registeredProperties.size(); i++ )
    {
        SAFEDEL( registeredProperties[ i ]->Accessor );
        SAFEREL( registeredProperties[ i ]->ComplexDataSubType );
        SAFEDEL( registeredProperties[ i ] );
    }
    registeredProperties.clear();

    for( size_t i = 0; i < podDataPool.size(); i++ )
        delete[] podDataPool[ i ];
    podDataPool.clear();
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

    // Extract type
    int type_id = engine->GetTypeIdByDecl( type_name );
    if( type_id < 0 )
    {
        WriteLogF( _FUNC_, " - Invalid type<%s>.\n", type_name );
        return NULL;
    }

    Property::DataType data_type;
    uint               data_size = 0;
    asIObjectType*     as_obj_type = engine->GetObjectTypeById( type_id );
    asIObjectType*     as_obj_sub_type = NULL;
    if( type_id & asTYPEID_OBJHANDLE )
    {
        WriteLogF( _FUNC_, " - Invalid property type<%s>, handles not allowed.\n", type_name );
        return NULL;
    }
    else if( !( type_id & asTYPEID_MASK_OBJECT ) )
    {
        data_type = Property::POD;

        int primitive_size = engine->GetSizeOfPrimitiveType( engine->GetTypeIdByDecl( type_name ) );
        if( primitive_size <= 0 )
        {
            WriteLogF( _FUNC_, " - Invalid property POD type<%s>.\n", type_name );
            return NULL;
        }

        data_size = (uint) primitive_size;
        if( data_size != 1 && data_size != 2 && data_size != 4 && data_size != 8 )
        {
            WriteLogF( _FUNC_, " - Invalid size of property POD type<%s>, size<%d>.\n", type_name, data_size );
            return NULL;
        }
    }
    else if( Str::Compare( as_obj_type->GetName(), "string" ) )
    {
        data_type = Property::String;

        data_size = sizeof( void* );
    }
    else if( Str::Compare( as_obj_type->GetName(), "array" ) )
    {
        data_type = Property::Array;

        data_size = sizeof( void* );
        as_obj_sub_type = as_obj_type->GetSubType();
        if( as_obj_type->GetSubTypeId() & asTYPEID_MASK_OBJECT )
        {
            WriteLogF( _FUNC_, " - Invalid property type<%s>, array elements must have POD type.\n", type_name );
            return NULL;
        }
    }
    else
    {
        WriteLogF( _FUNC_, " - Invalid property type<%s>.\n", type_name );
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
    if( !isServer && ( ( access & Property::PublicMask ) || ( access & Property::ProtectedMask ) ) && !( access & Property::ModifiableMask ) )
        disable_set = true;

    // Register default getter
    if( !disable_get )
    {
        char decl[ MAX_FOTEXT ];
        Str::Format( decl, "%s%s get_%s() const", type_name, data_type != Property::POD ? "@" : "", name );
        int  result = -1;
        if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Get< void* >, (void*), void* ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Get< uchar >, (void*), uchar ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Get< ushort >, (void*), ushort ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Get< uint >, (void*), uint ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( data_size == 8 )
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
        Str::Format( decl, "void set_%s(%s%s)", name, type_name, data_type != Property::POD ? "&" : "" );
        int  result = -1;
        if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< void* >, ( void*, void* ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< uchar >, ( void*, uchar ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< ushort >, ( void*, ushort ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< uint >, ( void*, uint ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( PropertyAccessor, Set< uint64 >, ( void*, uint64 ), void ), asCALL_THISCALL_OBJFIRST, property_accessor );
        if( result < 0 )
        {
            WriteLogF( _FUNC_, " - Register object property<%s> setter fail, error<%d>.\n", name, result );
            delete property_accessor;
            return NULL;
        }
    }

    // Calculate POD property data offset
    uint data_base_offset = uint( -1 );
    if( data_type == Property::POD && !disable_get )
    {
        bool     is_public = ( access & Property::PublicMask ) != 0;
        bool     is_protected = ( access & Property::ProtectedMask ) != 0;
        BoolVec& space = ( is_public ? publicPodDataSpace : ( is_protected ? protectedPodDataSpace : privatePodDataSpace ) );

        uint     space_size = (uint) space.size();
        uint     space_pos = 0;
        while( space_pos + data_size <= space_size )
        {
            bool fail = false;
            for( uint i = 0; i < data_size; i++ )
            {
                if( space[ space_pos + i ] )
                {
                    fail = true;
                    break;
                }
            }
            if( !fail )
                break;
            space_pos += data_size;
        }

        uint new_size = space_pos + data_size;
        new_size += 8 - new_size % 8;
        if( new_size > (uint) space.size() )
            space.resize( new_size );

        for( uint i = 0; i < data_size; i++ )
            space[ space_pos + i ] = true;

        data_base_offset = space_pos;

        wholePodDataSize = (uint) ( publicPodDataSpace.size() + protectedPodDataSpace.size() + privatePodDataSpace.size() );
        RUNTIME_ASSERT( !( wholePodDataSize % 8 ) );
    }

    // Make entry
    Property* prop = new Property();

    prop->Index = property_index;
    prop->ComplexDataIndex = ( data_type != Property::POD ? complexPropertiesCount++ : uint( -1 ) );
    prop->PODDataOffset = data_base_offset;
    prop->Size = data_size;
    prop->Accessor = property_accessor;
    prop->GetCallback = 0;
    prop->NativeSetCallback = NULL;

    prop->Name = name;
    prop->TypeName = type_name;
    prop->Type = data_type;
    prop->Access = access;
    prop->ComplexDataSubType = as_obj_sub_type;
    prop->GenerateRandomValue = generate_random_value;
    prop->SetDefaultValue = ( default_value != NULL );
    prop->CheckMinValue = ( min_value != NULL );
    prop->CheckMaxValue = ( max_value != NULL );
    prop->DefaultValue = ( default_value ? *default_value : 0 );
    prop->MinValue = ( min_value ? *min_value : 0 );
    prop->MaxValue = ( max_value ? *max_value : 0 );

    registeredProperties.push_back( prop );

    if( as_obj_sub_type )
        as_obj_sub_type->AddRef();

    if( prop->PODDataOffset != uint( -1 ) || prop->ComplexDataIndex != uint( -1 ) )
        serializedPropertiesCount++;

    return prop;
}

void PropertyRegistrator::FinishRegistration()
{
    RUNTIME_ASSERT( !registrationFinished );
    registrationFinished = true;

    // Fix POD data offsets
    for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registeredProperties[ i ];
        if( prop->PODDataOffset == uint( -1 ) )
            continue;

        if( prop->Access & Property::ProtectedMask )
            prop->PODDataOffset += (uint) publicPodDataSpace.size();
        else if( prop->Access & Property::PrivateMask )
            prop->PODDataOffset += (uint) publicPodDataSpace.size() + (uint) protectedPodDataSpace.size();
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
    return wholePodDataSize;
}
