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
        RUNTIME_ASSERT( prop->Offset != uint( -1 ) );
        memcpy( ret_value, &properties->podData[ prop->Offset ], prop->Size );
    }
    else
    {
        RUNTIME_ASSERT( prop->ComplexTypeIndex != uint( -1 ) );
        void*& p = *(void**) &properties->complexData[ prop->ComplexTypeIndex ];
        if( !p )
        {
            if( prop->Type == Property::String )
                p = ScriptString::Create();
            else if( prop->Type == Property::Array )
                p = ScriptArray::Create( 0, prop->ComplexDataSubType );
        }
        memcpy( ret_value, &p, prop->Size );
    }
}

void PropertyAccessor::GenericSet( void* obj, void* new_value )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];

    // Get current value
    void* cur_value;
    if( prop->Type == Property::POD )
    {
        RUNTIME_ASSERT( prop->Offset != uint( -1 ) );
        cur_value = &properties->podData[ prop->Offset ];
    }
    else
    {
        RUNTIME_ASSERT( prop->ComplexTypeIndex != uint( -1 ) );
        cur_value = &properties->complexData[ prop->ComplexTypeIndex ];
    }

    // Clamp
    if( prop->Type == Property::POD && ( prop->CheckMinValue || prop->CheckMaxValue ) )
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

    // Reference counting
    if( prop->Type != Property::POD )
    {
        if( prop->Type == Property::String )
        {
            ScriptString*& cur_value_obj = *(ScriptString**) cur_value;
            if( cur_value_obj && cur_value_obj->length() > 0 )
                cur_value_obj->AddRef();
            else
                cur_value_obj = NULL;

            ScriptString*& old_value_obj = *(ScriptString**) &old_value;
            if( old_value_obj )
                old_value_obj->Release();
        }
        else if( prop->Type == Property::Array )
        {
            ScriptArray*& cur_value_obj = *(ScriptArray**) cur_value;
            if( cur_value_obj && cur_value_obj->GetSize() > 0 )
                cur_value_obj->AddRef();
            else
                cur_value_obj = NULL;

            ScriptArray*& old_value_obj = *(ScriptArray**) &old_value;
            if( old_value_obj )
                old_value_obj->Release();
        }
    }
}

void* PropertyAccessor::GetData( void* obj, uint& data_size )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];

    if( prop->Type == Property::POD )
    {
        data_size = prop->Size;
        return &properties->podData[ prop->Offset ];
    }
    else if( prop->Type == Property::String )
    {
        ScriptString* obj = *(ScriptString**) &properties->complexData[ prop->ComplexTypeIndex ];
        data_size = ( obj ? obj->length() : 0 );
        return data_size ? (char*) obj->c_str() : NULL;
    }
    else if( prop->Type == Property::Array )
    {
        ScriptArray* obj = *(ScriptArray**) &properties->complexData[ prop->ComplexTypeIndex ];
        data_size = ( obj ? obj->GetSize() * obj->GetElementSize() : 0 );
        return data_size ? (char*) obj->At( 0 ) : NULL;
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }
    return NULL;
}

void PropertyAccessor::SetData( void* obj, void* data, uint data_size )
{
    Properties* properties = (Properties*) obj;
    Property*   prop = properties->registrator->registeredProperties[ propertyIndex ];

    if( prop->Type == Property::POD )
    {
        RUNTIME_ASSERT( data_size == prop->Size );
        GenericSet( obj, data );
    }
    else if( prop->Type == Property::String )
    {
        ScriptString* str = ScriptString::Create( data_size ? (char*) data : "", data_size );
        GenericSet( obj, &str );
        str->Release();
    }
    else if( prop->Type == Property::Array )
    {
        ScriptArray* arr = ScriptArray::Create( prop->ComplexDataSubType );
        if( data_size && ( data_size % arr->GetElementSize() ) == 0 )
        {
            arr->Resize( data_size / arr->GetElementSize() );
            memcpy( arr->At( 0 ), data, data_size );
        }
        prop->Accessor->GenericSet( obj, &arr );
        arr->Release();
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
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

    // Set default values
    for( size_t i = 0, j = registrator->registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->Offset == uint( -1 ) )
            continue;

        // Todo: complex string value

        if( prop->SetDefaultValue )
        {
            memcpy( &podData[ prop->Offset ], &prop->DefaultValue, prop->Size );
        }
        else if( prop->GenerateRandomValue )
        {
            // Todo: fix min/max
            for( uint i = 0; i < prop->Size; i++ )
                podData[ prop->Offset + i ] = Random( 0, 255 );
        }
    }
}

Properties::~Properties()
{
    registrator->podDataPool.push_back( podData );
    for( size_t i = 0; i < unresolvedProperties.size(); i++ )
        delete unresolvedProperties[ i ];
    unresolvedProperties.clear();
}

Properties& Properties::operator=( const Properties& other )
{
    RUNTIME_ASSERT( registrator == other.registrator );
    memcpy( &podData[ 0 ], &other.podData[ 0 ], registrator->wholePodDataSize );
    return *this;
}

void* Properties::FindData( const char* property_name )
{
    Property* prop = registrator->Find( property_name );
    RUNTIME_ASSERT( prop );
    RUNTIME_ASSERT( prop->Offset != uint( -1 ) );
    return prop ? &podData[ prop->Offset ] : NULL;
}

uint Properties::StoreData( bool with_protected, PUCharVec** data, UIntVec** data_sizes )
{
    uint whole_size = 0;
    *data = &storeData;
    *data_sizes = &storeDataSizes;
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
            if( prop->Type == Property::String )
            {
                ScriptString* str = (ScriptString*) complexData[ prop->ComplexTypeIndex ];
                storeData.push_back( str ? (uchar*) str->c_str() : NULL );
                storeDataSizes.push_back( str ? (uint) str->length() : 0 );
                whole_size += storeDataSizes.back();
            }
            else if( prop->Type == Property::Array )
            {
                ScriptArray* arr = (ScriptArray*) complexData[ prop->ComplexTypeIndex ];
                storeData.push_back( arr ? (uchar*) arr->At( 0 ) : NULL );
                storeDataSizes.push_back( arr ? arr->GetSize() * arr->GetElementSize() : 0 );
                whole_size += storeDataSizes.back();
            }
            else
            {
                RUNTIME_ASSERT( !"Unexpected type" );
            }
        }
    }
    return whole_size;
}

void Properties::RestoreData( bool with_protected, const UCharVecVec& data )
{
    // Restore POD properties data
    memcpy( &podData[ 0 ], &data[ 0 ][ 0 ], data[ 0 ].size() );
    memzero( &podData[ data[ 0 ].size() ], registrator->wholePodDataSize - (uint) data[ 0 ].size() );

    // Restore complex properties data
    for( size_t i = 0; i < registrator->registeredProperties.size(); i++ )
    {
        Property* prop = registrator->registeredProperties[ i ];
        if( prop->Type != Property::POD && ( prop->Access & Property::PublicMask || ( with_protected && prop->Access & Property::ProtectedMask ) ) )
        {
            RUNTIME_ASSERT( prop->ComplexTypeIndex < (uint) data.size() );
            uint         size = (uint) data[ prop->ComplexTypeIndex ].size();
            const uchar* p = ( size ? &data[ prop->ComplexTypeIndex ][ 0 ] : NULL );
            if( prop->Type == Property::String )
            {
                ScriptString*& str = *(ScriptString**) &complexData[ prop->ComplexTypeIndex ];
                if( size )
                {
                    if( !str )
                        str = ScriptString::Create( (char*) p, size );
                    else
                        str->assign( (char*) p, size );
                }
                else
                {
                    SAFEREL( str );
                }
            }
            else if( prop->Type == Property::Array )
            {
                ScriptArray*& arr = *(ScriptArray**) &complexData[ prop->ComplexTypeIndex ];
                if( size )
                {
                    if( !arr )
                        arr = ScriptArray::Create( prop->ComplexDataSubType );
                    arr->Resize( size / arr->GetElementSize() );
                    if( size )
                        memcpy( arr->At( 0 ), p, size );
                }
                else
                {
                    SAFEREL( arr );
                }
            }
            else
            {
                RUNTIME_ASSERT( !"Unexpected type" );
            }
        }
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
        save_func( &podData[ prop->Offset ], prop->Size );
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
            void* data = &podData[ prop->Offset ];
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
    wholePodDataSize = 0;
    complexPropertiesCount = 0;
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
        if( as_obj_type->GetTypeId() & asTYPEID_MASK_OBJECT )
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
        Str::Format( decl, "%s%s get_%s() const", type_name, data_type != Property::POD ? "&" : "", name );
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
    prop->ComplexTypeIndex = ( data_type != Property::POD ? complexPropertiesCount++ : uint( -1 ) );
    prop->Offset = data_base_offset;
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
        if( prop->Offset == uint( -1 ) )
            continue;

        if( prop->Access & Property::ProtectedMask )
            prop->Offset += (uint) publicPodDataSpace.size();
        else if( prop->Access & Property::PrivateMask )
            prop->Offset += (uint) publicPodDataSpace.size() + (uint) protectedPodDataSpace.size();
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
