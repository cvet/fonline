#include "Common.h"
#include "Properties.h"
#include "Entity.h"
#include "IniParser.h"
#include "Script.h"

NativeCallbackVec PropertyRegistrator::GlobalSetCallbacks;

Property::Property()
{
    nativeSendCallback = nullptr;
    nativeSetCallback = nullptr;
    setCallbacksAnyNewValue = false;
}

void* Property::CreateRefValue( uchar* data, uint data_size )
{
    asIScriptEngine* engine = asObjType->GetEngine();
    if( dataType == Property::Array )
    {
        if( isArrayOfString )
        {
            if( data_size )
            {
                uint arr_size;
                memcpy( &arr_size, data, sizeof( arr_size ) );
                data += sizeof( uint );
                CScriptArray* arr = CScriptArray::Create( asObjType, arr_size );
                RUNTIME_ASSERT( arr );
                for( uint i = 0; i < arr_size; i++ )
                {
                    uint str_size;
                    memcpy( &str_size, data, sizeof( str_size ) );
                    data += sizeof( uint );
                    string str( (char*) data, str_size );
                    arr->SetValue( i, &str );
                    data += str_size;
                }
                return arr;
            }
            else
            {
                CScriptArray* arr = CScriptArray::Create( asObjType );
                RUNTIME_ASSERT( arr );
                return arr;
            }
        }
        else
        {
            uint          element_size = engine->GetSizeOfPrimitiveType( asObjType->GetSubTypeId() );
            uint          arr_size = data_size / element_size;
            CScriptArray* arr = CScriptArray::Create( asObjType, arr_size );
            RUNTIME_ASSERT( arr );
            if( arr_size )
                memcpy( arr->At( 0 ), data, arr_size * element_size );
            return arr;
        }
    }
    else if( dataType == Property::Dict )
    {
        CScriptDict* dict = CScriptDict::Create( asObjType );
        RUNTIME_ASSERT( dict );
        if( data_size )
        {
            uint key_element_size = engine->GetSizeOfPrimitiveType( asObjType->GetSubTypeId( 0 ) );
            if( isDictOfArray )
            {
                asITypeInfo* arr_type = asObjType->GetSubType( 1 );
                uint         arr_element_size = engine->GetSizeOfPrimitiveType( arr_type->GetSubTypeId() );
                uchar*       data_end = data + data_size;
                while( data < data_end )
                {
                    void* key = data;
                    data += key_element_size;
                    uint  arr_size;
                    memcpy( &arr_size, data, sizeof( arr_size ) );
                    data += sizeof( uint );
                    CScriptArray* arr = CScriptArray::Create( arr_type, arr_size );
                    RUNTIME_ASSERT( arr );
                    if( arr_size )
                    {
                        if( isDictOfArrayOfString )
                        {
                            for( uint i = 0; i < arr_size; i++ )
                            {
                                uint str_size;
                                memcpy( &str_size, data, sizeof( str_size ) );
                                data += sizeof( uint );
                                string str( (char*) data, str_size );
                                arr->SetValue( i, &str );
                                data += str_size;
                            }
                        }
                        else
                        {
                            memcpy( arr->At( 0 ), data, arr_size * arr_element_size );
                            data += arr_size * arr_element_size;
                        }
                    }
                    dict->Set( key, &arr );
                    arr->Release();
                }
            }
            else if( isDictOfString )
            {
                uchar* data_end = data + data_size;
                while( data < data_end )
                {
                    void* key = data;
                    data += key_element_size;
                    uint  str_size;
                    memcpy( &str_size, data, sizeof( str_size ) );
                    data += sizeof( uint );
                    string str( (char*) data, str_size );
                    dict->Set( key, &str );
                    data += str_size;
                }
            }
            else
            {
                uint value_element_size = engine->GetSizeOfPrimitiveType( asObjType->GetSubTypeId( 1 ) );
                uint whole_element_size = key_element_size + value_element_size;
                uint dict_size = data_size / whole_element_size;
                for( uint i = 0; i < dict_size; i++ )
                    dict->Set( data + i * whole_element_size, data + i * whole_element_size + key_element_size );
            }
        }
        return dict;
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }
    return nullptr;
}

void Property::ReleaseRefValue( void* value )
{
    if( dataType == Property::Array )
        ( (CScriptArray*) value )->Release();
    else if( dataType == Property::Dict )
        ( (CScriptDict*) value )->Release();
    else
        RUNTIME_ASSERT( !"Unexpected type" );
}

uchar* Property::ExpandComplexValueData( void* value, uint& data_size, bool& need_delete )
{
    need_delete = false;
    if( dataType == Property::String )
    {
        string& str = *(string*) value;
        data_size = (uint) str.length();
        return data_size ? (uchar*) str.c_str() : nullptr;
    }
    else if( dataType == Property::Array )
    {
        CScriptArray* arr = (CScriptArray*) value;
        if( isArrayOfString )
        {
            data_size = 0;
            uint arr_size = arr->GetSize();
            if( arr_size )
            {
                need_delete = true;

                // Calculate size
                data_size += sizeof( uint );
                for( uint i = 0; i < arr_size; i++ )
                {
                    string& str = *(string*) arr->At( i );
                    data_size += sizeof( uint ) + (uint) str.length();
                }

                // Make buffer
                uchar* init_buf = new uchar[ data_size ];
                uchar* buf = init_buf;
                memcpy( buf, &arr_size, sizeof( uint ) );
                buf += sizeof( uint );
                for( uint i = 0; i < arr_size; i++ )
                {
                    string& str = *(string*) arr->At( i );
                    uint    str_size = (uint) str.length();
                    memcpy( buf, &str_size, sizeof( uint ) );
                    buf += sizeof( uint );
                    if( str_size )
                    {
                        memcpy( buf, str.c_str(), str_size );
                        buf += str_size;
                    }
                }
                return init_buf;
            }
            return nullptr;
        }
        else
        {
            int element_size;
            int type_id = arr->GetElementTypeId();
            if( type_id & asTYPEID_MASK_OBJECT )
                element_size = sizeof( asPWORD );
            else
                element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType( type_id );

            data_size = ( arr ? arr->GetSize() * element_size : 0 );
            return data_size ? (uchar*) arr->At( 0 ) : nullptr;
        }
    }
    else if( dataType == Property::Dict )
    {
        CScriptDict* dict = (CScriptDict*) value;
        if( isDictOfArray )
        {
            data_size = ( dict ? dict->GetSize() : 0 );
            if( data_size )
            {
                need_delete = true;
                uint key_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType( asObjType->GetSubTypeId( 0 ) );
                uint arr_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType( asObjType->GetSubType( 1 )->GetSubTypeId() );

                // Calculate size
                data_size = 0;
                vector< pair< void*, void* > > dict_map;
                dict->GetMap( dict_map );
                for( const auto& kv : dict_map )
                {
                    CScriptArray* arr = *(CScriptArray**) kv.second;
                    uint          arr_size = ( arr ? arr->GetSize() : 0 );
                    data_size += key_element_size + sizeof( uint );
                    if( isDictOfArrayOfString )
                    {
                        for( uint i = 0; i < arr_size; i++ )
                        {
                            string& str = *(string*) arr->At( i );
                            data_size += sizeof( uint ) + (uint) str.length();
                        }
                    }
                    else
                    {
                        data_size += arr_size * arr_element_size;
                    }
                }

                // Make buffer
                uchar* init_buf = new uchar[ data_size ];
                uchar* buf = init_buf;
                for( const auto& kv : dict_map )
                {
                    CScriptArray* arr = *(CScriptArray**) kv.second;
                    memcpy( buf, kv.first, key_element_size );
                    buf += key_element_size;
                    uint arr_size = ( arr ? arr->GetSize() : 0 );
                    memcpy( buf, &arr_size, sizeof( uint ) );
                    buf += sizeof( uint );
                    if( arr_size )
                    {
                        if( isDictOfArrayOfString )
                        {
                            for( uint i = 0; i < arr_size; i++ )
                            {
                                string& str = *(string*) arr->At( i );
                                uint    str_size = (uint) str.length();
                                memcpy( buf, &str_size, sizeof( uint ) );
                                buf += sizeof( uint );
                                if( str_size )
                                {
                                    memcpy( buf, str.c_str(), str_size );
                                    buf += arr_size;
                                }
                            }
                        }
                        else
                        {
                            memcpy( buf, arr->At( 0 ), arr_size * arr_element_size );
                            buf += arr_size * arr_element_size;
                        }
                    }
                }
                return init_buf;
            }
        }
        else if( isDictOfString )
        {
            uint key_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType( asObjType->GetSubTypeId( 0 ) );
            data_size = ( dict ? dict->GetSize() : 0 );
            if( data_size )
            {
                need_delete = true;

                // Calculate size
                data_size = 0;
                vector< pair< void*, void* > > dict_map;
                dict->GetMap( dict_map );
                for( const auto& kv : dict_map )
                {
                    string& str = *(string*) kv.second;
                    uint    str_size = (uint) str.length();
                    data_size += key_element_size + sizeof( uint ) + str_size;
                }

                // Make buffer
                uchar* init_buf = new uchar[ data_size ];
                uchar* buf = init_buf;
                for( const auto& kv : dict_map )
                {
                    string& str = *(string*) kv.second;
                    memcpy( buf, kv.first, key_element_size );
                    buf += key_element_size;
                    uint str_size = (uint) str.length();
                    memcpy( buf, &str_size, sizeof( uint ) );
                    buf += sizeof( uint );
                    if( str_size )
                    {
                        memcpy( buf, str.c_str(), str_size );
                        buf += str_size;
                    }
                }
                return init_buf;
            }
        }
        else
        {
            uint key_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType( asObjType->GetSubTypeId( 0 ) );
            uint value_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType( asObjType->GetSubTypeId( 1 ) );
            uint whole_element_size = key_element_size + value_element_size;
            data_size = ( dict ? dict->GetSize() * whole_element_size : 0 );
            if( data_size )
            {
                need_delete = true;
                uchar*                         init_buf = new uchar[ data_size ];
                uchar*                         buf = init_buf;
                vector< pair< void*, void* > > dict_map;
                dict->GetMap( dict_map );
                for( const auto& kv : dict_map )
                {
                    memcpy( buf, kv.first, key_element_size );
                    buf += key_element_size;
                    memcpy( buf, kv.second, value_element_size );
                    buf += value_element_size;
                }
                return init_buf;
            }
        }
        return nullptr;
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }
    return nullptr;
}

void Property::GenericGet( Entity* entity, void* ret_value )
{
    Properties* properties = &entity->Props;

    // Validate entity
    if( entity->IsDestroyed )
    {
        SCRIPT_ERROR_R( "Attempt to get property '%s %s::%s' on destroyed entity.",
                        typeName.c_str(), properties->registrator->scriptClassName.c_str(), propName.c_str() );
    }

    // Virtual property
    if( accessType & Property::VirtualMask )
    {
        if( !getCallback )
        {
            SCRIPT_ERROR_R( "'Get' callback is not assigned for virtual property '%s %s::%s'.",
                            typeName.c_str(), properties->registrator->scriptClassName.c_str(), propName.c_str() );
        }

        if( properties->getCallbackLocked[ getIndex ] )
        {
            SCRIPT_ERROR_R( "Recursive call for virtual property '%s %s::%s'.",
                            typeName.c_str(), properties->registrator->scriptClassName.c_str(), propName.c_str() );
        }

        properties->getCallbackLocked[ getIndex ] = true;

        Script::PrepareContext( getCallback, GetName() );
        if( getCallbackArgs > 0 )
            Script::SetArgEntity( entity );
        if( getCallbackArgs > 1 )
            Script::SetArgUInt( enumValue );
        if( Script::RunPrepared() )
        {
            properties->getCallbackLocked[ getIndex ] = false;

            if( dataType == Property::String )
            {
                *(string*) ret_value = *(string*) Script::GetReturnedRawAddress();
            }
            else if( dataType == Property::POD )
            {
                memcpy( ret_value, Script::GetReturnedRawAddress(), baseSize );
            }
            else if( dataType == Property::Array || dataType == Property::Dict )
            {
                void*& val = *(void**) ret_value;
                if( val )
                {
                    if( dataType == Property::Array )
                        ( (CScriptArray*) val )->AddRef();
                    else
                        ( (CScriptDict*) val )->AddRef();
                }
                else
                {
                    val = CreateRefValue( nullptr, 0 );
                }
            }
            else
            {
                RUNTIME_ASSERT( !"Unexpected type" );
            }

            return;
        }

        properties->getCallbackLocked[ getIndex ] = false;

        // Error
        Script::PassException();
        return;
    }

    // Raw property
    if( dataType == Property::String )
    {
        RUNTIME_ASSERT( complexDataIndex != uint( -1 ) );
        uchar* data = properties->complexData[ complexDataIndex ];
        uint   data_size = properties->complexDataSizes[ complexDataIndex ];
        if( data_size )
            ( *(string*) ret_value ).assign( (char*) data, data_size );
    }
    else if( dataType == Property::POD )
    {
        RUNTIME_ASSERT( podDataOffset != uint( -1 ) );
        memcpy( ret_value, &properties->podData[ podDataOffset ], baseSize );
    }
    else if( dataType == Property::Array || dataType == Property::Dict )
    {
        RUNTIME_ASSERT( complexDataIndex != uint( -1 ) );
        uchar* data = properties->complexData[ complexDataIndex ];
        uint   data_size = properties->complexDataSizes[ complexDataIndex ];
        *(void**) ret_value = CreateRefValue( data, data_size );
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }
}

void Property::GenericSet( Entity* entity, void* new_value )
{
    Properties* properties = &entity->Props;

    // Validate entity
    if( entity->IsDestroyed )
    {
        SCRIPT_ERROR_R( "Attempt to set property '%s %s::%s' on destroyed entity.",
                        typeName.c_str(), properties->registrator->scriptClassName.c_str(), propName.c_str() );
    }

    // Dereference ref types
    if( dataType == DataType::Array || dataType == DataType::Dict )
        new_value = *(void**) new_value;

    // Virtual property
    if( accessType & Property::VirtualMask )
    {
        if( setCallbacks.empty() )
        {
            SCRIPT_ERROR_R( "'Set' callback is not assigned for virtual property '%s %s::%s'.",
                            typeName.c_str(), properties->registrator->scriptClassName.c_str(), propName.c_str() );
        }

        for( size_t i = 0; i < setCallbacks.size(); i++ )
        {
            Script::PrepareContext( setCallbacks[ i ], GetName() );
            Script::SetArgObject( entity );
            if( setCallbacksArgs[ i ] > 1 )
            {
                Script::SetArgUInt( enumValue );
                if( setCallbacksArgs[ i ] == 3 )
                    Script::SetArgAddress( new_value );
            }
            else if( setCallbacksArgs[ i ] < -1 )
            {
                Script::SetArgAddress( new_value );
                if( setCallbacksArgs[ i ] == -3 )
                    Script::SetArgUInt( enumValue );
            }

            bool run_ok = true;
            if( setCallbacksDeferred[ i ] )
                Script::RunPreparedSuspend();
            else
                run_ok = Script::RunPrepared();
            RUNTIME_ASSERT( !entity->IsDestroyed );
            if( !run_ok )
                break;
        }

        return;
    }

    // Get current value
    void*  cur_pod_value = nullptr;
    uint64 old_pod_value = 0;
    bool   new_value_data_need_delete = false;
    uint   new_value_data_size = 0;
    uchar* new_value_data = nullptr;
    if( dataType == Property::POD )
    {
        RUNTIME_ASSERT( podDataOffset != uint( -1 ) );
        cur_pod_value = &properties->podData[ podDataOffset ];

        // Ignore void calls
        if( !memcmp( new_value, cur_pod_value, baseSize ) )
            return;

        // Store old value for native callbacks
        memcpy( &old_pod_value, cur_pod_value, baseSize );
    }
    else
    {
        RUNTIME_ASSERT( complexDataIndex != uint( -1 ) );

        // Check for null
        if( !new_value )
        {
            SCRIPT_ERROR_R( "Attempt to set null on property '%s %s::%s'.",
                            typeName.c_str(), properties->registrator->scriptClassName.c_str(), propName.c_str() );
        }

        // Expand new value data for comparison
        new_value_data = ExpandComplexValueData( new_value, new_value_data_size, new_value_data_need_delete );

        // Get current data for comparison
        uint   cur_value_data_size = properties->complexDataSizes[ complexDataIndex ];
        uchar* cur_value_data = properties->complexData[ complexDataIndex ];

        // Compare for ignore void calls
        bool void_call = false;
        if( new_value_data_size == cur_value_data_size && ( cur_value_data_size == 0 || !memcmp( new_value_data, cur_value_data, cur_value_data_size ) ) )
            void_call = true;

        // Deallocate data
        if( new_value_data_need_delete && new_value_data_size && ( setCallbacksAnyNewValue || void_call ) )
            SAFEDELA( new_value_data );

        // Ignore void calls
        if( void_call )
            return;
    }

    // Script callbacks
    if( !setCallbacks.empty() )
    {
        for( size_t i = 0; i < setCallbacks.size(); i++ )
        {
            Script::PrepareContext( setCallbacks[ i ], GetName() );
            Script::SetArgObject( entity );
            if( setCallbacksArgs[ i ] > 1 )
            {
                Script::SetArgUInt( enumValue );
                if( setCallbacksArgs[ i ] == 3 )
                    Script::SetArgAddress( new_value );
            }
            else if( setCallbacksArgs[ i ] < -1 )
            {
                Script::SetArgAddress( new_value );
                if( setCallbacksArgs[ i ] == -3 )
                    Script::SetArgUInt( enumValue );
            }

            bool run_ok = true;
            if( setCallbacksDeferred[ i ] )
                Script::RunPreparedSuspend();
            else
                run_ok = Script::RunPrepared();
            RUNTIME_ASSERT( !entity->IsDestroyed );
            if( !run_ok )
                break;
        }
    }

    // Check min/max for POD value and store
    if( dataType == Property::POD )
    {
        // Clamp
        if( checkMaxValue )
        {
            #define CHECK_MAX_VALUE( type )          \
                do {                                 \
                    type max_v = (type) maxValue;    \
                    if( *(type*) new_value > max_v ) \
                        *(type*) new_value = max_v;  \
                }                                    \
                while( 0 )
            if( isIntDataType && isSignedIntDataType )
            {
                if( baseSize == 1 )
                    CHECK_MAX_VALUE( char );
                else if( baseSize == 2 )
                    CHECK_MAX_VALUE( short );
                else if( baseSize == 4 )
                    CHECK_MAX_VALUE( int );
                else if( baseSize == 8 )
                    CHECK_MAX_VALUE( int64 );
            }
            else if( isIntDataType && !isSignedIntDataType )
            {
                if( baseSize == 1 )
                    CHECK_MAX_VALUE( uchar );
                else if( baseSize == 2 )
                    CHECK_MAX_VALUE( ushort );
                else if( baseSize == 4 )
                    CHECK_MAX_VALUE( uint );
                else if( baseSize == 8 )
                    CHECK_MAX_VALUE( uint64 );
            }
            else if( isFloatDataType )
            {
                if( baseSize == 4 )
                    CHECK_MAX_VALUE( float );
                else if( baseSize == 8 )
                    CHECK_MAX_VALUE( double );
            }
            #undef CHECK_MAX_VALUE
        }
        if( checkMinValue )
        {
            #define CHECK_MIN_VALUE( type )          \
                do {                                 \
                    type min_v = (type) minValue;    \
                    if( *(type*) new_value < min_v ) \
                        *(type*) new_value = min_v;  \
                }                                    \
                while( 0 )
            if( isIntDataType && isSignedIntDataType )
            {
                if( baseSize == 1 )
                    CHECK_MIN_VALUE( char );
                else if( baseSize == 2 )
                    CHECK_MIN_VALUE( short );
                else if( baseSize == 4 )
                    CHECK_MIN_VALUE( int );
                else if( baseSize == 8 )
                    CHECK_MIN_VALUE( int64 );
            }
            else if( isIntDataType && !isSignedIntDataType )
            {
                if( baseSize == 1 )
                    CHECK_MIN_VALUE( uchar );
                else if( baseSize == 2 )
                    CHECK_MIN_VALUE( ushort );
                else if( baseSize == 4 )
                    CHECK_MIN_VALUE( uint );
                else if( baseSize == 8 )
                    CHECK_MIN_VALUE( uint64 );
            }
            else if( isFloatDataType )
            {
                if( baseSize == 4 )
                    CHECK_MIN_VALUE( float );
                else if( baseSize == 8 )
                    CHECK_MIN_VALUE( double );
            }
            #undef CHECK_MIN_VALUE
        }

        // Ignore void calls
        if( !memcmp( new_value, cur_pod_value, baseSize ) )
            return;

        // Store
        memcpy( cur_pod_value, new_value, baseSize );
    }

    // Store complex value
    if( dataType != Property::POD )
    {
        RUNTIME_ASSERT( complexDataIndex != uint( -1 ) );

        // Expand new value data for comparison
        if( !new_value_data )
            new_value_data = ExpandComplexValueData( new_value, new_value_data_size, new_value_data_need_delete );

        // Get current data for comparison
        uint   cur_value_data_size = properties->complexDataSizes[ complexDataIndex ];
        uchar* cur_value_data = properties->complexData[ complexDataIndex ];

        // Compare for ignore void calls
        if( new_value_data_size == cur_value_data_size && ( cur_value_data_size == 0 || !memcmp( new_value_data, cur_value_data, cur_value_data_size ) ) )
        {
            // Deallocate data
            if( new_value_data_need_delete && new_value_data_size )
                SAFEDELA( new_value_data );
            return;
        }

        // Copy new property data
        if( properties->complexDataSizes[ complexDataIndex ] != new_value_data_size && new_value_data_size && new_value_data_need_delete )
        {
            new_value_data_need_delete = false;
            SAFEDELA( properties->complexData[ complexDataIndex ] );
            properties->complexDataSizes[ complexDataIndex ] = new_value_data_size;
            properties->complexData[ complexDataIndex ] = new_value_data;
        }
        else
        {
            if( properties->complexDataSizes[ complexDataIndex ] != new_value_data_size )
            {
                SAFEDELA( properties->complexData[ complexDataIndex ] );
                properties->complexDataSizes[ complexDataIndex ] = new_value_data_size;
                if( new_value_data_size )
                    properties->complexData[ complexDataIndex ] = new uchar[ new_value_data_size ];
            }
            if( new_value_data_size )
                memcpy( properties->complexData[ complexDataIndex ], new_value_data, new_value_data_size );
        }

        // Deallocate data
        if( new_value_data_need_delete && new_value_data_size )
            SAFEDELA( new_value_data );
    }

    // Native set callbacks
    if( nativeSetCallback || !PropertyRegistrator::GlobalSetCallbacks.empty() )
    {
        if( nativeSetCallback )
            nativeSetCallback( entity, this, new_value, &old_pod_value );
        for( size_t i = 0, j = PropertyRegistrator::GlobalSetCallbacks.size(); i < j; i++ )
            PropertyRegistrator::GlobalSetCallbacks[ i ] ( entity, this, new_value, &old_pod_value );

        // Maybe callback reset value
        if( dataType == Property::POD && !memcmp( new_value, &old_pod_value, baseSize ) )
            return;
    }

    // Native send callback
    if( nativeSendCallback && !( this == properties->sendIgnoreProperty && entity == properties->sendIgnoreEntity ) )
    {
        if( ( properties->registrator->isServer && ( accessType & ( Property::PublicMask | Property::ProtectedMask ) ) ) ||
            ( !properties->registrator->isServer && ( accessType & Property::ModifiableMask ) ) )
        {
            nativeSendCallback( entity, this );
        }
    }
}

const char* Property::GetName()
{
    return propName.c_str();
}

const char* Property::GetTypeName()
{
    return typeName.c_str();
}

uint Property::GetRegIndex()
{
    return regIndex;
}

int Property::GetEnumValue()
{
    return enumValue;
}

Property::AccessType Property::GetAccess()
{
    return accessType;
}

uint Property::GetBaseSize()
{
    return baseSize;
}

asITypeInfo* Property::GetASObjectType()
{
    return asObjType;
}

bool Property::IsPOD()
{
    return dataType == Property::POD;
}

bool Property::IsDict()
{
    return dataType == Property::Dict;
}

bool Property::IsHash()
{
    return isHash;
}

bool Property::IsResource()
{
    return isResource;
}

bool Property::IsEnum()
{
    return isEnumDataType;
}

bool Property::IsReadable()
{
    return isReadable;
}

bool Property::IsWritable()
{
    return isWritable;
}

bool Property::IsConst()
{
    return isConst;
}

uchar* Property::GetRawData( Entity* entity, uint& data_size )
{
    return GetPropRawData( &entity->Props, data_size );
}

uchar* Property::GetPropRawData( Properties* properties, uint& data_size )
{
    if( dataType == Property::POD )
    {
        RUNTIME_ASSERT( podDataOffset != uint( -1 ) );
        data_size = baseSize;
        return &properties->podData[ podDataOffset ];
    }

    RUNTIME_ASSERT( complexDataIndex != uint( -1 ) );
    data_size = properties->complexDataSizes[ complexDataIndex ];
    return properties->complexData[ complexDataIndex ];
}

void Property::SetPropRawData( Properties* properties, uchar* data, uint data_size )
{
    if( IsPOD() )
    {
        RUNTIME_ASSERT( podDataOffset != uint( -1 ) );
        RUNTIME_ASSERT( baseSize == data_size );
        memcpy( &properties->podData[ podDataOffset ], data, data_size );
    }
    else
    {
        RUNTIME_ASSERT( complexDataIndex != uint( -1 ) );
        if( data_size != properties->complexDataSizes[ complexDataIndex ] )
        {
            properties->complexDataSizes[ complexDataIndex ] = data_size;
            SAFEDELA( properties->complexData[ complexDataIndex ] );
            if( data_size )
                properties->complexData[ complexDataIndex ] = new uchar[ data_size ];
        }
        if( data_size )
            memcpy( properties->complexData[ complexDataIndex ], data, data_size );
    }
}

void Property::SetData( Entity* entity, uchar* data, uint data_size )
{
    if( dataType == Property::String )
    {
        string str;
        if( data_size )
            str.assign( (char*) data, data_size );
        GenericSet( entity, &str );
    }
    else if( dataType == Property::POD )
    {
        RUNTIME_ASSERT( data_size == baseSize );
        GenericSet( entity, data );
    }
    else if( dataType == Property::Array || dataType == Property::Dict )
    {
        void* value = CreateRefValue( data, data_size );
        GenericSet( entity, &value );
        ReleaseRefValue( value );
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }
}

int Property::GetPODValueAsInt( Entity* entity )
{
    RUNTIME_ASSERT( dataType == Property::POD );
    if( isBoolDataType )
    {
        return GetValue< bool >( entity ) ? 1 : 0;
    }
    else if( isFloatDataType )
    {
        if( baseSize == 4 )
            return (int) GetValue< float >( entity );
        else if( baseSize == 8 )
            return (int) GetValue< double >( entity );
    }
    else if( isSignedIntDataType )
    {
        if( baseSize == 1 )
            return (int) GetValue< char >( entity );
        if( baseSize == 2 )
            return (int) GetValue< short >( entity );
        if( baseSize == 4 )
            return (int) GetValue< int >( entity );
        if( baseSize == 8 )
            return (int) GetValue< int64 >( entity );
    }
    else
    {
        if( baseSize == 1 )
            return (int) GetValue< uchar >( entity );
        if( baseSize == 2 )
            return (int) GetValue< ushort >( entity );
        if( baseSize == 4 )
            return (int) GetValue< uint >( entity );
        if( baseSize == 8 )
            return (int) GetValue< uint64 >( entity );
    }
    RUNTIME_ASSERT( !"Unreachable place" );
    return 0;
}

void Property::SetPODValueAsInt( Entity* entity, int value )
{
    RUNTIME_ASSERT( dataType == Property::POD );
    if( isBoolDataType )
    {
        SetValue< bool >( entity, value != 0 );
    }
    else if( isFloatDataType )
    {
        if( baseSize == 4 )
            SetValue< float >( entity, (float) value );
        else if( baseSize == 8 )
            SetValue< double >( entity, (double) value );
    }
    else if( isSignedIntDataType )
    {
        if( baseSize == 1 )
            SetValue< char >( entity, (char) value );
        else if( baseSize == 2 )
            SetValue< short >( entity, (short) value );
        else if( baseSize == 4 )
            SetValue< int >( entity, (int) value );
        else if( baseSize == 8 )
            SetValue< int64 >( entity, (int64) value );
    }
    else
    {
        if( baseSize == 1 )
            SetValue< uchar >( entity, (uchar) value );
        else if( baseSize == 2 )
            SetValue< ushort >( entity, (ushort) value );
        else if( baseSize == 4 )
            SetValue< uint >( entity, (uint) value );
        else if( baseSize == 8 )
            SetValue< uint64 >( entity, (uint64) value );
    }
}

string Property::SetGetCallback( asIScriptFunction* func )
{
    // Todo: Check can get

    uint bind_id = Script::BindByFunc( func, false );
    if( !bind_id )
        return "Unable to bind function '" + string( func->GetName() ) + "'.";

    getCallback = bind_id;
    getCallbackArgs = func->GetParamCount();

    return "";

    /*char decl[ MAX_FOTEXT ];
       Str::Format( decl, "%s%s %s(const %s&,%s)", typeName.c_str(), !IsPOD() ? "@" : "", "%s", registrator->scriptClassName.c_str(), registrator->enumTypeName.c_str() );
       uint bind_id = Script::BindByFuncNameInRuntime( script_func, decl, false, true );
       if( !bind_id )
       {
        Str::Format( decl, "%s%s %s(const %s&)", typeName.c_str(), !IsPOD() ? "@" : "", "%s", registrator->scriptClassName.c_str() );
        bind_id = Script::BindByFuncNameInRuntime( script_func, decl, false, true );
        if( !bind_id )
        {
            char buf[ MAX_FOTEXT ];
            Str::Format( buf, decl, script_func );
            return "Unable to bind function '" + string( buf ) + "'.";
        }
        getCallbackArgs = 1;
       }
       else
       {
        getCallbackArgs = 2;
       }

       getCallback = bind_id;
       return "";*/
}

string Property::AddSetCallback( asIScriptFunction* func, bool deferred )
{
    uint bind_id = Script::BindByFunc( func, false );
    if( !bind_id )
        return "Unable to bind function '" + string( func->GetName() ) + "'.";

    setCallbacksArgs.push_back( func->GetParamCount() );
    asDWORD flags;
    if( setCallbacksArgs.back() >= 2 && func->GetParam( 1, nullptr, &flags ) > 0 && flags & asTM_INOUTREF )
        setCallbacksArgs.back() = -setCallbacksArgs.back();
    if( setCallbacksArgs.back() == 3 || setCallbacksArgs.back() == -3 || setCallbacksArgs.back() == -2 )
        setCallbacksAnyNewValue = true;

    setCallbacks.push_back( bind_id );
    setCallbacksDeferred.push_back( deferred );

    return "";

    /*char decl[ MAX_FOTEXT ];
       Str::Format( decl, "void %s(%s%s&,%s,%s&)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str(), registrator->enumTypeName.c_str(), typeName.c_str() );
       uint  bind_id = ( !deferred ? Script::BindByFuncNameInRuntime( script_func, decl, false, true ) : 0 );
       if( !bind_id )
       {
        Str::Format( decl, "void %s(%s%s&,%s&,%s)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str(), typeName.c_str(), registrator->enumTypeName.c_str() );
        bind_id = ( !deferred ? Script::BindByFuncNameInRuntime( script_func, decl, false, true ) : 0 );
        if( !bind_id )
        {
            Str::Format( decl, "void %s(%s%s&,%s)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str(), registrator->enumTypeName.c_str() );
            bind_id = Script::BindByFuncNameInRuntime( script_func, decl, false, true );
            if( !bind_id )
            {
                Str::Format( decl, "void %s(%s%s&,%s&)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str(), typeName.c_str() );
                bind_id = ( !deferred ? Script::BindByFuncNameInRuntime( script_func, decl, false, true ) : 0 );
                if( !bind_id )
                {
                    Str::Format( decl, "void %s(%s%s&)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str() );
                    bind_id = Script::BindByFuncNameInRuntime( script_func, decl, false, true );
                    if( !bind_id )
                    {
                        char buf[ MAX_FOTEXT ];
                        Str::Format( buf, decl, script_func );
                        return "Unable to bind function '" + string( buf ) + "'.";
                    }
                    else
                    {
                        setCallbacksArgs.push_back( 1 );
                    }
                }
                else
                {
                    setCallbacksArgs.push_back( -2 );
                    setCallbacksAnyNewValue = true;
                }
            }
            else
            {
                setCallbacksArgs.push_back( 2 );
            }
        }
        else
        {
            setCallbacksArgs.push_back( -3 );
            setCallbacksAnyNewValue = true;
        }
       }
       else
       {
        setCallbacksArgs.push_back( 3 );
        setCallbacksAnyNewValue = true;
       }

       setCallbacks.push_back( bind_id );
       setCallbacksDeferred.push_back( deferred );
       return "";*/
}

Properties::Properties( PropertyRegistrator* reg )
{
    registrator = reg;
    RUNTIME_ASSERT( registrator );
    RUNTIME_ASSERT( registrator->registrationFinished );

    getCallbackLocked = new bool[ registrator->getPropertiesCount ];
    memzero( getCallbackLocked, registrator->getPropertiesCount * sizeof( bool ) );
    sendIgnoreEntity = nullptr;
    sendIgnoreProperty = nullptr;

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
}

Properties::Properties( const Properties& other ): Properties( other.registrator )
{
    // Copy POD data
    memcpy( &podData[ 0 ], &other.podData[ 0 ], registrator->wholePodDataSize );

    // Copy complex data
    for( size_t i = 0; i < other.complexData.size(); i++ )
    {
        uint size = other.complexDataSizes[ i ];
        complexDataSizes[ i ] = size;
        complexData[ i ] = ( size ? new uchar[ size ] : NULL );
        if( size )
            memcpy( complexData[ i ], other.complexData[ i ], size );
    }
}

Properties::~Properties()
{
    delete[] getCallbackLocked;

    registrator->podDataPool.push_back( podData );
    podData = nullptr;

    for( size_t i = 0; i < complexData.size(); i++ )
        SAFEDELA( complexData[ i ] );
    complexData.clear();
    complexDataSizes.clear();
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
        if( prop->complexDataIndex != uint( -1 ) )
            prop->SetPropRawData( this, other.complexData[ prop->complexDataIndex ], other.complexDataSizes[ prop->complexDataIndex ] );
    }

    return *this;
}

template< >
string Properties::GetPropValue< string >( Property* prop )
{
    RUNTIME_ASSERT( prop->dataType == Property::DataType::String );
    RUNTIME_ASSERT( sizeof( string ) == prop->baseSize );
    uint   data_size;
    uchar* data = prop->GetPropRawData( this, data_size );
    return string( (char*) data, data_size );
}

Property* Properties::FindByEnum( int enum_value )
{
    return registrator->FindByEnum( enum_value );
}

void* Properties::FindData( const char* property_name )
{
    if( !registrator )
        return 0;
    Property* prop = registrator->Find( property_name );
    RUNTIME_ASSERT( prop );
    RUNTIME_ASSERT( prop->podDataOffset != uint( -1 ) );
    return prop ? &podData[ prop->podDataOffset ] : nullptr;
}

uint Properties::StoreData( bool with_protected, PUCharVec** all_data, UIntVec** all_data_sizes )
{
    uint whole_size = 0;
    *all_data = &storeData;
    *all_data_sizes = &storeDataSizes;
    storeData.resize( 0 );
    storeDataSizes.resize( 0 );

    // Store POD properties data
    storeData.push_back( podData );
    storeDataSizes.push_back( (uint) registrator->publicPodDataSpace.size() + ( with_protected ? (uint) registrator->protectedPodDataSpace.size() : 0 ) );
    whole_size += storeDataSizes.back();

    // Calculate complex data to send
    storeDataComplexIndicies = ( with_protected ? registrator->publicProtectedComplexDataProps : registrator->publicComplexDataProps );
    for( size_t i = 0; i < storeDataComplexIndicies.size();)
    {
        Property* prop = registrator->registeredProperties[ storeDataComplexIndicies[ i ] ];
        RUNTIME_ASSERT( prop->complexDataIndex != uint( -1 ) );
        if( !complexDataSizes[ prop->complexDataIndex ] )
            storeDataComplexIndicies.erase( storeDataComplexIndicies.begin() + i );
        else
            i++;
    }

    // Store complex properties data
    if( !storeDataComplexIndicies.empty() )
    {
        storeData.push_back( (uchar*) &storeDataComplexIndicies[ 0 ] );
        storeDataSizes.push_back( (uint) storeDataComplexIndicies.size() * sizeof( short ) );
        whole_size += storeDataSizes.back();
        for( size_t i = 0; i < storeDataComplexIndicies.size(); i++ )
        {
            Property* prop = registrator->registeredProperties[ storeDataComplexIndicies[ i ] ];
            storeData.push_back( complexData[ prop->complexDataIndex ] );
            storeDataSizes.push_back( complexDataSizes[ prop->complexDataIndex ] );
            whole_size += storeDataSizes.back();
        }
    }
    return whole_size;
}

void Properties::RestoreData( PUCharVec& all_data, UIntVec& all_data_sizes )
{
    // Restore POD data
    uint publicSize = (uint) registrator->publicPodDataSpace.size();
    uint protectedSize = (uint) registrator->protectedPodDataSpace.size();
    RUNTIME_ASSERT( all_data_sizes[ 0 ] == publicSize || all_data_sizes[ 0 ] == publicSize + protectedSize );
    if( all_data_sizes[ 0 ] )
        memcpy( podData, all_data[ 0 ], all_data_sizes[ 0 ] );

    // Restore complex data
    if( all_data.size() > 1 )
    {
        uint      comlplex_data_count = all_data_sizes[ 1 ] / sizeof( ushort );
        RUNTIME_ASSERT( comlplex_data_count > 0 );
        UShortVec complex_indicies( comlplex_data_count );
        memcpy( &complex_indicies[ 0 ], all_data[ 1 ], all_data_sizes[ 1 ] );

        for( size_t i = 0; i < complex_indicies.size(); i++ )
        {
            RUNTIME_ASSERT( complex_indicies[ i ] < registrator->registeredProperties.size() );
            Property* prop = registrator->registeredProperties[ complex_indicies[ i ] ];
            RUNTIME_ASSERT( prop->complexDataIndex != uint( -1 ) );
            uint      data_size = all_data_sizes[ 2 + i ];
            uchar*    data = all_data[ 2 + i ];
            prop->SetPropRawData( this, data, data_size );
        }
    }
}

void Properties::RestoreData( UCharVecVec& all_data )
{
    PUCharVec all_data_ext( all_data.size() );
    UIntVec   all_data_sizes( all_data.size() );
    for( size_t i = 0; i < all_data.size(); i++ )
    {
        all_data_ext[ i ] = ( !all_data[ i ].empty() ? &all_data[ i ][ 0 ] : nullptr );
        all_data_sizes[ i ] = (uint) all_data[ i ].size();
    }
    RestoreData( all_data_ext, all_data_sizes );
}

static const char* ReadToken( const char* str, string& result )
{
    if( !*str )
        return nullptr;

    const char* begin;
    const char* s = str;

    uint length;
    Str::DecodeUTF8( s, &length );
    while( length == 1 && ( *s == ' ' || *s == '\t' ) )
        Str::DecodeUTF8( ++s, &length );

    if( !*s )
        return nullptr;

    if( length == 1 && *s == '{' )
    {
        s++;
        begin = s;

        int braces = 1;
        while( *s )
        {
            if( length == 1 && *s == '\\' )
            {
                ++s;
                if( *s )
                {
                    Str::DecodeUTF8( s, &length );
                    s += length;
                }
            }
            else if( length == 1 && *s == '{' )
            {
                braces++;
                s++;
            }
            else if( length == 1 && *s == '}' )
            {
                braces--;
                if( braces == 0 )
                    break;
                s++;
            }
            else
            {
                s += length;
            }
            Str::DecodeUTF8( s, &length );
        }
    }
    else
    {
        begin = s;
        while( *s )
        {
            if( length == 1 && *s == '\\' )
            {
                Str::DecodeUTF8( ++s, &length );
                s += length;
            }
            else if( length == 1 && ( *s == ' ' || *s == '\t' ) )
            {
                break;
            }
            else
            {
                s += length;
            }
            Str::DecodeUTF8( s, &length );
        }
    }

    result.assign( begin, (uint) ( s - begin ) );
    return *s ? s + 1 : s;
}

static string CodeString( const string& str, int deep )
{
    bool need_braces = false;
    if( deep > 0 && ( str.empty() || str.find_first_of( " \t" ) != string::npos ) )
        need_braces = true;
    if( deep == 0 && !str.empty() && ( str.find_first_of( " \t" ) == 0 || str.find_last_of( " \t" ) == str.length() - 1 ) )
        need_braces = true;

    string result;
    result.reserve( str.length() * 2 );

    if( need_braces )
        result.append( "{" );

    const char* s = str.c_str();
    uint length;
    while( *s )
    {
        Str::DecodeUTF8( s, &length );
        if( length == 1 )
        {
            switch( *s )
            {
            case '\r':
                break;
            case '\n':
                result.append( "\\n" );
                break;
            case '{':
                result.append( "\\{" );
                break;
            case '}':
                result.append( "\\}" );
                break;
            case '\\':
                result.append( "\\\\" );
                break;
            default:
                result.append( s, 1 );
                break;
            }
        }
        else
        {
            result.append( s, length );
        }

        s += length;
    }

    if( need_braces )
        result.append( "}" );

    return result;
}

static string DecodeString( const string& str )
{
    if( str.empty() )
        return str;

    string result;
    result.reserve( str.length() );

    const char* s = str.c_str();
    uint length;

    Str::DecodeUTF8( s, &length );
    bool is_braces = ( length == 1 && *s == '{' );
    if( is_braces )
        s++;

    while( *s )
    {
        Str::DecodeUTF8( s, &length );
        if( length == 1 && *s == '\\' )
        {
            Str::DecodeUTF8( ++s, &length );

            switch( *s )
            {
            case 'n':
                result.append( "\n" );
                break;
            case '{':
                result.append( "{" );
                break;
            case '}':
                result.append( "\n" );
                break;
            case '\\':
                result.append( "\\" );
                break;
            default:
                result.append( 1, '\\' );
                result.append( s, length );
                break;
            }
        }
        else
        {
            result.append( s, length );
        }

        s += length;
    }

    if( is_braces && length == 1 && result.back() == '}' )
        result.pop_back();

    return result;
}

string WriteValue( void* ptr, int type_id, asITypeInfo* as_obj_type, bool* is_hashes, int deep )
{
    if( !( type_id & asTYPEID_MASK_OBJECT ) )
    {
        RUNTIME_ASSERT( type_id != asTYPEID_VOID );

        #define VALUE_AS( ctype )    ( *(ctype*) ( ptr ) )
        #define CHECK_PRIMITIVE( astype, ctype, toa ) \
            if( type_id == astype )                   \
                return toa( VALUE_AS( ctype ) );

        if( is_hashes[ deep ] )
        {
            if( type_id != asTYPEID_UINT32 )
                RUNTIME_ASSERT( type_id == asTYPEID_UINT32 );
            return VALUE_AS( hash ) ? CodeString( Str::GetName( VALUE_AS( hash ) ), deep ) : CodeString( "", deep );
        }

        CHECK_PRIMITIVE( asTYPEID_BOOL, bool, Str::BtoA );
        CHECK_PRIMITIVE( asTYPEID_INT8, char, Str::ItoA );
        CHECK_PRIMITIVE( asTYPEID_INT16, short, Str::ItoA );
        CHECK_PRIMITIVE( asTYPEID_INT32, int, Str::ItoA );
        CHECK_PRIMITIVE( asTYPEID_INT64, int64, Str::I64toA );
        CHECK_PRIMITIVE( asTYPEID_UINT8, uchar, Str::UItoA );
        CHECK_PRIMITIVE( asTYPEID_UINT16, ushort, Str::UItoA );
        CHECK_PRIMITIVE( asTYPEID_UINT32, uint, Str::UItoA );
        CHECK_PRIMITIVE( asTYPEID_UINT64, uint64, Str::UI64toA );
        CHECK_PRIMITIVE( asTYPEID_DOUBLE, double, Str::DFtoA );
        CHECK_PRIMITIVE( asTYPEID_FLOAT, float, Str::FtoA );
        return Script::GetEnumValueName( Script::GetEngine()->GetTypeDeclaration( type_id ), VALUE_AS( int ) );

        #undef VALUE_AS
        #undef CHECK_PRIMITIVE
    }
    else if( Str::Compare( as_obj_type->GetName(), "string" ) )
    {
        string& str = *(string*) ptr;
        return CodeString( str, deep );
    }
    else if( Str::Compare( as_obj_type->GetName(), "array" ) )
    {
        string result = ( deep > 0 ? "{" : "" );
        CScriptArray* arr = (CScriptArray*) ptr;
        if( deep > 0 )
            arr = *(CScriptArray**) ptr;
        asUINT arr_size = arr->GetSize();
        if( arr_size > 0 )
        {
            int value_type_id = as_obj_type->GetSubTypeId( 0 );
            asITypeInfo* value_type = as_obj_type->GetSubType( 0 );
            for( asUINT i = 0; i < arr_size; i++ )
                result.append( WriteValue( arr->At( i ), value_type_id, value_type, is_hashes, deep + 1 ) ).append( " " );
            result.pop_back();
        }
        if( deep > 0 )
            result.append( "}" );
        return result;
    }
    else if( Str::Compare( as_obj_type->GetName(), "dict" ) )
    {
        string result = ( deep > 0 ? "{" : "" );
        CScriptDict* dict = (CScriptDict*) ptr;
        if( dict->GetSize() > 0 )
        {
            int key_type_id = as_obj_type->GetSubTypeId( 0 );
            int value_type_id = as_obj_type->GetSubTypeId( 1 );
            asITypeInfo* key_type = as_obj_type->GetSubType( 0 );
            asITypeInfo* value_type = as_obj_type->GetSubType( 1 );
            vector< pair< void*, void* > > dict_map;
            dict->GetMap( dict_map );
            for( const auto& dict_kv : dict_map )
            {
                result.append( WriteValue( dict_kv.first, key_type_id, key_type, is_hashes, deep + 1 ) ).append( " " );
                result.append( WriteValue( dict_kv.second, value_type_id, value_type, is_hashes, deep + 2 ) ).append( " " );
            }
            result.pop_back();
        }
        if( deep > 0 )
            result.append( "}" );
        return result;
    }
    RUNTIME_ASSERT( !"Unreachable place" );
    return "";
}

void* ReadValue( const char* value, int type_id, asITypeInfo* as_obj_type, bool* is_hashes, int deep, void* pod_buf, bool& is_error )
{
    RUNTIME_ASSERT( deep <= 3 );

    if( !( type_id & asTYPEID_MASK_OBJECT ) )
    {
        RUNTIME_ASSERT( type_id != asTYPEID_VOID );

        #define CHECK_PRIMITIVE( astype, ctype, ato )   \
            if( type_id == astype )                     \
            {                                           \
                ctype v = (ctype) ato( value );         \
                memcpy( pod_buf, &v, sizeof( ctype ) ); \
                return pod_buf;                         \
            }

        if( is_hashes[ deep ] )
        {
            RUNTIME_ASSERT( type_id == asTYPEID_UINT32 );
            hash v = Str::GetHash( DecodeString( value ) );
            memcpy( pod_buf, &v, sizeof( v ) );
            return pod_buf;
        }

        CHECK_PRIMITIVE( asTYPEID_BOOL, bool, Str::AtoB );
        CHECK_PRIMITIVE( asTYPEID_INT8, char, Str::AtoI );
        CHECK_PRIMITIVE( asTYPEID_INT16, short, Str::AtoI );
        CHECK_PRIMITIVE( asTYPEID_INT32, int, Str::AtoI );
        CHECK_PRIMITIVE( asTYPEID_INT64, int64, Str::AtoI64 );
        CHECK_PRIMITIVE( asTYPEID_UINT8, uchar, Str::AtoUI );
        CHECK_PRIMITIVE( asTYPEID_UINT16, ushort, Str::AtoUI );
        CHECK_PRIMITIVE( asTYPEID_UINT32, uint, Str::AtoUI );
        CHECK_PRIMITIVE( asTYPEID_UINT64, uint64, Str::AtoUI64 );
        CHECK_PRIMITIVE( asTYPEID_DOUBLE, double, Str::AtoDF );
        CHECK_PRIMITIVE( asTYPEID_FLOAT, float, Str::AtoF );

        int v = Script::GetEnumValue( Script::GetEngine()->GetTypeDeclaration( type_id ), value, is_error );
        memcpy( pod_buf, &v, sizeof( v ) );
        return pod_buf;

        #undef CHECK_PRIMITIVE
    }
    else if( Str::Compare( as_obj_type->GetName(), "string" ) )
    {
        string* str = (string*) Script::GetEngine()->CreateScriptObject( as_obj_type );
        *str = DecodeString( value );
        return str;
    }
    else if( Str::Compare( as_obj_type->GetName(), "array" ) )
    {
        CScriptArray* arr = CScriptArray::Create( as_obj_type );
        int value_type_id = as_obj_type->GetSubTypeId( 0 );
        asITypeInfo* value_type = as_obj_type->GetSubType( 0 );
        string str;
        uchar arr_pod_buf[ 8 ];
        while( ( value = ReadToken( value, str ) ) )
        {
            void* v = ReadValue( str.c_str(), value_type_id, value_type, is_hashes, deep + 1, arr_pod_buf, is_error );
            arr->InsertLast( value_type_id & asTYPEID_OBJHANDLE ? &v : v );
            if( v != arr_pod_buf )
                Script::GetEngine()->ReleaseScriptObject( v, value_type );
        }
        return arr;
    }
    else if( Str::Compare( as_obj_type->GetName(), "dict" ) )
    {
        CScriptDict* dict = CScriptDict::Create( as_obj_type );
        int key_type_id = as_obj_type->GetSubTypeId( 0 );
        int value_type_id = as_obj_type->GetSubTypeId( 1 );
        asITypeInfo* key_type = as_obj_type->GetSubType( 0 );
        asITypeInfo* value_type = as_obj_type->GetSubType( 1 );
        string str1, str2;
        uchar dict_pod_buf1[ 8 ];
        uchar dict_pod_buf2[ 8 ];
        while( ( value = ReadToken( value, str1 ) ) && ( value = ReadToken( value, str2 ) ) )
        {
            void* v1 = ReadValue( str1.c_str(), key_type_id, key_type, is_hashes, deep + 1, dict_pod_buf1, is_error );
            void* v2 = ReadValue( str2.c_str(), value_type_id, value_type, is_hashes, deep + 2, dict_pod_buf2, is_error );
            dict->Set( key_type_id & asTYPEID_OBJHANDLE ? &v1 : v1, value_type_id & asTYPEID_OBJHANDLE ? &v2 : v2 );
            if( v1 != dict_pod_buf1 )
                Script::GetEngine()->ReleaseScriptObject( v1, key_type );
            if( v2 != dict_pod_buf2 )
                Script::GetEngine()->ReleaseScriptObject( v2, value_type );
        }
        return dict;
    }
    RUNTIME_ASSERT( !"Unreachable place" );
    return nullptr;
}

bool Properties::LoadFromText( const StrMap& key_values )
{
    bool is_error = false;
    for( const auto& kv : key_values )
    {
        const char* key = kv.first.c_str();
        const char* value = kv.second.c_str();

        // Skip technical fields
        if( !key[ 0 ] || key[ 0 ] == '$' )
            continue;

        // Find property
        Property* prop = registrator->Find( key );
        if( !prop || ( prop->podDataOffset == uint( -1 ) && prop->complexDataIndex == uint( -1 ) ) )
        {
            if( !prop )
                WriteLog( "Unknown property '{}'.\n", key );
            else
                WriteLog( "Invalid property '{}' for reading.\n", prop->GetName() );

            is_error = true;
            continue;
        }

        // Parse
        if( !LoadPropertyFromText( prop, value ) )
            is_error = true;
    }
    return !is_error;
}

void Properties::SaveToText( StrMap& key_values, Properties* base )
{
    RUNTIME_ASSERT( !base || registrator == base->registrator );

    for( auto& prop : registrator->registeredProperties )
    {
        // Skip pure virtual properties
        if( prop->podDataOffset == uint( -1 ) && prop->complexDataIndex == uint( -1 ) )
            continue;

        // Skip same
        if( base )
        {
            if( prop->podDataOffset != uint( -1 ) )
            {
                if( !memcmp( &podData[ prop->podDataOffset ], &base->podData[ prop->podDataOffset ], prop->baseSize ) )
                    continue;
            }
            else
            {
                if( !complexDataSizes[ prop->complexDataIndex ] && !base->complexDataSizes[ prop->complexDataIndex ] )
                    continue;
                if( complexDataSizes[ prop->complexDataIndex ] == base->complexDataSizes[ prop->complexDataIndex ] &&
                    !memcmp( complexData[ prop->complexDataIndex ], base->complexData[ prop->complexDataIndex ], complexDataSizes[ prop->complexDataIndex ] ) )
                    continue;
            }
        }
        else
        {
            if( prop->podDataOffset != uint( -1 ) )
            {
                uint64 pod_zero = 0;
                RUNTIME_ASSERT( prop->baseSize <= sizeof( pod_zero ) );
                if( !memcmp( &podData[ prop->podDataOffset ], &pod_zero, prop->baseSize ) )
                    continue;
            }
            else
            {
                if( !complexDataSizes[ prop->complexDataIndex ] )
                    continue;
            }
        }

        // Serialize to text and store in map
        key_values.insert( PAIR( prop->propName, SavePropertyToText( prop ) ) );
    }
}

bool Properties::LoadPropertyFromText( Property* prop, const char* text )
{
    RUNTIME_ASSERT( prop );
    RUNTIME_ASSERT( registrator == prop->registrator );
    RUNTIME_ASSERT( prop->podDataOffset != uint( -1 ) || prop->complexDataIndex != uint( -1 ) );
    bool is_error = false;

    // Parse
    uchar pod_buf[ 8 ];
    bool is_hashes[] = { prop->isHash || prop->isResource, prop->isHashSubType0, prop->isHashSubType1, prop->isHashSubType2 };
    void* value = ReadValue( text, prop->asObjTypeId, prop->asObjType, is_hashes, 0, pod_buf, is_error );

    // Assign
    if( prop->podDataOffset != uint( -1 ) )
    {
        RUNTIME_ASSERT( value == pod_buf );
        prop->SetPropRawData( this, pod_buf, prop->baseSize );
    }
    else if( prop->complexDataIndex != uint( -1 ) )
    {
        bool need_delete;
        uint data_size;
        uchar* data = prop->ExpandComplexValueData( value, data_size, need_delete );
        prop->SetPropRawData( this, data, data_size );
        if( need_delete )
            SAFEDELA( data );
        Script::GetEngine()->ReleaseScriptObject( value, prop->asObjType );
    }

    return !is_error;
}

string Properties::SavePropertyToText( Property* prop )
{
    RUNTIME_ASSERT( prop );
    RUNTIME_ASSERT( registrator == prop->registrator );
    RUNTIME_ASSERT( prop->podDataOffset != uint( -1 ) || prop->complexDataIndex != uint( -1 ) );

    uint data_size;
    void* data = prop->GetPropRawData( this, data_size );

    void* value = data;
    string str;
    if( prop->dataType == Property::String )
    {
        value = &str;
        if( data_size )
            str.assign( (char*) data, data_size );
    }
    else if( prop->dataType == Property::Array || prop->dataType == Property::Dict )
    {
        value = prop->CreateRefValue( (uchar*) data, data_size );
    }

    bool is_hashes[] = { prop->isHash || prop->isResource, prop->isHashSubType0, prop->isHashSubType1, prop->isHashSubType2 };
    string text = WriteValue( value, prop->asObjTypeId, prop->asObjType, is_hashes, 0 );

    if( prop->dataType == Property::Array || prop->dataType == Property::Dict )
        prop->ReleaseRefValue( value );

    return text;
}

int Properties::GetValueAsInt( Entity* entity, int enum_value )
{
    Property* prop = entity->Props.registrator->FindByEnum( enum_value );
    if( !prop )
        SCRIPT_ERROR_R0( "Enum '%d' not found", enum_value );
    if( !prop->IsPOD() )
        SCRIPT_ERROR_R0( "Can't retreive integer value from non POD property '%s'", prop->GetName() );
    if( !prop->IsReadable() )
        SCRIPT_ERROR_R0( "Can't retreive integer value from non readable property '%s'", prop->GetName() );

    return prop->GetPODValueAsInt( entity );
}

void Properties::SetValueAsInt( Entity* entity, int enum_value, int value )
{
    Property* prop = entity->Props.registrator->FindByEnum( enum_value );
    if( !prop )
        SCRIPT_ERROR_R( "Enum '%d' not found", enum_value );
    if( !prop->IsPOD() )
        SCRIPT_ERROR_R( "Can't set integer value to non POD property '%s'", prop->GetName() );
    if( !prop->IsWritable() )
        SCRIPT_ERROR_R( "Can't set integer value to non writable property '%s'", prop->GetName() );

    prop->SetPODValueAsInt( entity, value );
}

bool Properties::SetValueAsIntByName( Entity* entity, const char* enum_name, int value )
{
    Property* prop = entity->Props.registrator->Find( enum_name );
    if( !prop )
        SCRIPT_ERROR_R0( "Enum '%s' not found", enum_name );
    if( !prop->IsPOD() )
        SCRIPT_ERROR_R0( "Can't set by name integer value from non POD property '%s'", prop->GetName() );
    if( !prop->IsWritable() )
        SCRIPT_ERROR_R0( "Can't set integer value to non writable property '%s'", prop->GetName() );

    prop->SetPODValueAsInt( entity, value );
    return true;
}

bool Properties::SetValueAsIntProps( Properties* props, int enum_value, int value )
{
    Property* prop = props->registrator->FindByEnum( enum_value );
    if( !prop )
        SCRIPT_ERROR_R0( "Enum '%d' not found", enum_value );
    if( !prop->IsPOD() )
        SCRIPT_ERROR_R0( "Can't set integer value to non POD property '%s'", prop->GetName() );
    if( !prop->IsWritable() )
        SCRIPT_ERROR_R0( "Can't set integer value to non writable property '%s'", prop->GetName() );
    if( prop->accessType & Property::VirtualMask )
        SCRIPT_ERROR_R0( "Can't set integer value to virtual property '%s'", prop->GetName() );

    if( prop->isBoolDataType )
    {
        props->SetPropValue< bool >( prop, value != 0 );
    }
    else if( prop->isFloatDataType )
    {
        if( prop->baseSize == 4 )
            props->SetPropValue< float >( prop, (float) value );
        else if( prop->baseSize == 8 )
            props->SetPropValue< double >( prop, (double) value );
    }
    else if( prop->isSignedIntDataType )
    {
        if( prop->baseSize == 1 )
            props->SetPropValue< char >( prop, (char) value );
        else if( prop->baseSize == 2 )
            props->SetPropValue< short >( prop, (short) value );
        else if( prop->baseSize == 4 )
            props->SetPropValue< int >( prop, (int) value );
        else if( prop->baseSize == 8 )
            props->SetPropValue< int64 >( prop, (int64) value );
    }
    else
    {
        if( prop->baseSize == 1 )
            props->SetPropValue< uchar >( prop, (uchar) value );
        else if( prop->baseSize == 2 )
            props->SetPropValue< ushort >( prop, (ushort) value );
        else if( prop->baseSize == 4 )
            props->SetPropValue< uint >( prop, (uint) value );
        else if( prop->baseSize == 8 )
            props->SetPropValue< uint64 >( prop, (uint64) value );
    }
    return true;
}

string Properties::GetClassName()
{
    return registrator->scriptClassName;
}

void Properties::SetSendIgnore( Property* prop, Entity* entity )
{
    if( prop )
    {
        RUNTIME_ASSERT( sendIgnoreEntity == nullptr );
        RUNTIME_ASSERT( sendIgnoreProperty == nullptr );
    }
    else
    {
        RUNTIME_ASSERT( sendIgnoreEntity != nullptr );
        RUNTIME_ASSERT( sendIgnoreProperty != nullptr );
    }

    sendIgnoreEntity = entity;
    sendIgnoreProperty = prop;
}

PropertyRegistrator::PropertyRegistrator( bool is_server, const char* script_class_name )
{
    registrationFinished = false;
    isServer = is_server;
    scriptClassName = script_class_name;
    wholePodDataSize = 0;
    complexPropertiesCount = 0;
    defaultGroup = nullptr;
    defaultMinValue = nullptr;
    defaultMaxValue = nullptr;
    getPropertiesCount = 0;
}

PropertyRegistrator::~PropertyRegistrator()
{
    scriptClassName = "";
    wholePodDataSize = 0;

    for( size_t i = 0; i < registeredProperties.size(); i++ )
    {
        SAFEREL( registeredProperties[ i ]->asObjType );
        SAFEDEL( registeredProperties[ i ] );
    }
    registeredProperties.clear();

    for( size_t i = 0; i < podDataPool.size(); i++ )
        delete[] podDataPool[ i ];
    podDataPool.clear();

    SetDefaults();
}

bool PropertyRegistrator::Init()
{
    // Register common stuff on first property registration
    string enum_type = ( scriptClassName.find( "Cl" ) != string::npos ? scriptClassName.substr( 0, scriptClassName.size() - 2 ) : scriptClassName ) + "Property";
    enumTypeName = enum_type;
    RUNTIME_ASSERT( enumTypeName.length() > 0 );

    asIScriptEngine* engine = Script::GetEngine();
    RUNTIME_ASSERT( engine );

    int result = engine->RegisterEnum( enum_type.c_str() );
    if( result < 0 )
    {
        WriteLog( "Register entity property enum '{}' fail, error {}.\n", enum_type.c_str(), result );
        return false;
    }

    result = engine->RegisterEnumValue( enum_type.c_str(), "Invalid", 0 );
    if( result < 0 )
    {
        WriteLog( "Register entity property enum '{}::Invalid' zero value fail, error {}.\n", enum_type.c_str(), result );
        return false;
    }

    char decl[ MAX_FOTEXT ];
    Str::Format( decl, "int GetAsInt(%s) const", enum_type.c_str() );
    result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, SCRIPT_FUNC_THIS( Properties::GetValueAsInt ), SCRIPT_FUNC_THIS_CONV );
    if( result < 0 )
    {
        WriteLog( "Register entity method '{}' fail, error {}.\n", decl, result );
        return false;
    }

    Str::Format( decl, "void SetAsInt(%s,int)", enum_type.c_str() );
    result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, SCRIPT_FUNC_THIS( Properties::SetValueAsInt ), SCRIPT_FUNC_THIS_CONV );
    if( result < 0 )
    {
        WriteLog( "Register entity method '{}' fail, error {}.\n", decl, result );
        return false;
    }

    return true;
}

template< typename T >
static void Property_GetValue_Generic( asIScriptGeneric* gen )
{
    new ( gen->GetAddressOfReturnLocation() )T( ( (Property*) gen->GetAuxiliary() )->GetValue< T >( (Entity*) gen->GetObject() ) );
}

template< typename T >
static void Property_SetValue_Generic( asIScriptGeneric* gen )
{
    ( (Property*) gen->GetAuxiliary() )->SetValue< T >( (Entity*) gen->GetObject(), *(T*) gen->GetAddressOfArg( 0 ) );
}

Property* PropertyRegistrator::Register(
    const char* type_name,
    const char* name,
    Property::AccessType access,
    bool is_const,
    const char* group /* = NULL */,
    int64* min_value /* = NULL */,
    int64* max_value /* = NULL */
    )
{
    if( registrationFinished )
    {
        WriteLog( "Registration of class properties is finished.\n" );
        return nullptr;
    }

    // Check defaults
    group = ( group ? group : defaultGroup );
    min_value = ( min_value ? min_value : defaultMinValue );
    max_value = ( max_value ? max_value : defaultMaxValue );

    // Get engine
    asIScriptEngine* engine = Script::GetEngine();
    RUNTIME_ASSERT( engine );

    // Extract type
    int type_id = engine->GetTypeIdByDecl( type_name );
    if( type_id < 0 )
    {
        WriteLog( "Invalid type '{}'.\n", type_name );
        return nullptr;
    }

    Property::DataType data_type;
    uint               data_size = 0;
    asITypeInfo*       as_obj_type = engine->GetTypeInfoById( type_id );
    bool               is_int_data_type = false;
    bool               is_signed_int_data_type = false;
    bool               is_float_data_type = false;
    bool               is_bool_data_type = false;
    bool               is_enum_data_type = false;
    bool               is_array_of_string = false;
    bool               is_dict_of_string = false;
    bool               is_dict_of_array = false;
    bool               is_dict_of_array_of_string = false;
    bool               is_hash_sub0 = false;
    bool               is_hash_sub1 = false;
    bool               is_hash_sub2 = false;
    if( !( type_id & asTYPEID_MASK_OBJECT ) )
    {
        data_type = Property::POD;

        int type_id = engine->GetTypeIdByDecl( type_name );
        int primitive_size = engine->GetSizeOfPrimitiveType( type_id );
        if( primitive_size <= 0 )
        {
            WriteLog( "Invalid property POD type '{}'.\n", type_name );
            return nullptr;
        }

        data_size = (uint) primitive_size;
        if( data_size != 1 && data_size != 2 && data_size != 4 && data_size != 8 )
        {
            WriteLog( "Invalid size of property POD type '{}', size {}.\n", type_name, data_size );
            return nullptr;
        }

        is_int_data_type = ( type_id >= asTYPEID_INT8 && type_id <= asTYPEID_UINT64 );
        is_signed_int_data_type = ( type_id >= asTYPEID_INT8 && asTYPEID_UINT8 <= asTYPEID_INT64 );
        is_float_data_type = ( type_id == asTYPEID_FLOAT || type_id == asTYPEID_DOUBLE );
        is_bool_data_type = ( type_id == asTYPEID_BOOL );
        is_enum_data_type = ( type_id > asTYPEID_DOUBLE );
    }
    else if( Str::Compare( as_obj_type->GetName(), "string" ) )
    {
        data_type = Property::String;
        data_size = sizeof( string );
    }
    else if( Str::Compare( as_obj_type->GetName(), "array" ) )
    {
        data_type = Property::Array;
        data_size = sizeof( void* );

        bool is_array_of_pod = !( as_obj_type->GetSubTypeId() & asTYPEID_MASK_OBJECT );
        is_array_of_string = ( !is_array_of_pod && Str::Compare( as_obj_type->GetSubType()->GetName(), "string" ) );
        if( !is_array_of_pod && !is_array_of_string )
        {
            WriteLog( "Invalid property type '{}', array elements must have POD/string type.\n", type_name );
            return nullptr;
        }
        if( Str::Substring( type_name, "resource" ) )
        {
            WriteLog( "Invalid property type '{}', array elements can't be resource type.\n", type_name );
            return nullptr;
        }

        is_hash_sub0 = ( Str::Substring( type_name, "hash" ) != nullptr );
    }
    else if( Str::Compare( as_obj_type->GetName(), "dict" ) )
    {
        data_type = Property::Dict;
        data_size = sizeof( void* );

        if( as_obj_type->GetSubTypeId( 0 ) & asTYPEID_MASK_OBJECT )
        {
            WriteLog( "Invalid property type '{}', dict key must have POD type.\n", type_name );
            return nullptr;
        }

        int value_sub_type_id = as_obj_type->GetSubTypeId( 1 );
        asITypeInfo* value_sub_type = as_obj_type->GetSubType( 1 );
        if( value_sub_type_id & asTYPEID_MASK_OBJECT )
        {
            is_dict_of_string = Str::Compare( value_sub_type->GetName(), "string" );
            is_dict_of_array = Str::Compare( value_sub_type->GetName(), "array" );
            is_dict_of_array_of_string = ( is_dict_of_array && value_sub_type->GetSubType() && Str::Compare( value_sub_type->GetSubType()->GetName(), "string" ) );
            bool is_dict_array_of_pod = !( value_sub_type->GetSubTypeId() & asTYPEID_MASK_OBJECT );
            if( !is_dict_of_string && !is_dict_array_of_pod && !is_dict_of_array_of_string )
            {
                WriteLog( "Invalid property type '{}', dict value must have POD/string type or array of POD/string type.\n", type_name );
                return nullptr;
            }
        }
        if( ( Str::Substring( type_name, "resource" ) != nullptr && Str::Substring( Str::Substring( type_name, "resource" ), "," ) != nullptr ) ||
            ( Str::Substring( type_name, "resource" ) != nullptr && Str::Substring( Str::Substring( type_name, "resource" ), "," ) == nullptr && !is_dict_of_array ) ||
            ( Str::Substring( type_name, "resource" ) != nullptr && Str::Substring( Str::Substring( type_name, "resource" ), "," ) == nullptr && is_dict_of_array ) )
        {
            WriteLog( "Invalid property type '{}', dict elements can't be resource type.\n", type_name );
            return nullptr;
        }

        is_hash_sub0 = ( Str::Substring( type_name, "hash" ) != nullptr && Str::Substring( Str::Substring( type_name, "hash" ), "," ) != nullptr );
        is_hash_sub1 = ( Str::Substring( type_name, "hash" ) != nullptr && Str::Substring( Str::Substring( type_name, "hash" ), "," ) == nullptr && !is_dict_of_array );
        is_hash_sub2 = ( Str::Substring( type_name, "hash" ) != nullptr && Str::Substring( Str::Substring( type_name, "hash" ), "," ) == nullptr && is_dict_of_array );
    }
    else
    {
        WriteLog( "Invalid property type '{}'.\n", type_name );
        return nullptr;
    }

    // Check name for already used
    asITypeInfo* ot = engine->GetTypeInfoByName( scriptClassName.c_str() );
    RUNTIME_ASSERT_STR( ot, scriptClassName.c_str() );
    for( asUINT i = 0, j = ot->GetPropertyCount(); i < j; i++ )
    {
        const char* n;
        ot->GetProperty( i, &n );
        if( Str::Compare( n, name ) )
        {
            WriteLog( "Trying to register already registered property '{}'.\n", name );
            return nullptr;
        }
    }

    // Allocate property
    Property* prop = new Property();
    uint      reg_index = (uint) registeredProperties.size();

    // Disallow set or get accessors
    bool disable_get = false;
    bool disable_set = false;
    if( isServer && ( access & Property::ClientOnlyMask ) )
        disable_get = disable_set = true;
    if( !isServer && ( access & Property::ServerOnlyMask ) )
        disable_get = disable_set = true;
    if( !isServer && ( ( access & Property::PublicMask ) || ( access & Property::ProtectedMask ) ) && !( access & Property::ModifiableMask ) )
        disable_set = true;
    if( is_const )
        disable_set = true;
    #ifdef FONLINE_MAPPER
    disable_get = false;
    #endif

    // Register default getter
    bool is_handle = ( data_type == Property::Array || data_type == Property::Dict );
    if( !disable_get )
    {
        char decl[ MAX_FOTEXT ];
        Str::Format( decl, "const %s%s get_%s() const", type_name, is_handle ? "@" : "", name );
        int  result = -1;
        #ifdef AS_MAX_PORTABILITY
        if( data_type == Property::String )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( Property_GetValue_Generic< string >), asCALL_GENERIC, prop );
        else if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( Property_GetValue_Generic< void* >), asCALL_GENERIC, prop );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( Property_GetValue_Generic< char >), asCALL_GENERIC, prop );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( Property_GetValue_Generic< short >), asCALL_GENERIC, prop );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( Property_GetValue_Generic< int >), asCALL_GENERIC, prop );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( Property_GetValue_Generic< int64 >), asCALL_GENERIC, prop );
        #else
        if( data_type == Property::String )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, GetValue< string >, (Entity*), string ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, GetValue< void* >, (Entity*), void* ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, GetValue< char >, (Entity*), char ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, GetValue< short >, (Entity*), short ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, GetValue< int >, (Entity*), int ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, GetValue< int64 >, (Entity*), int64 ), asCALL_THISCALL_OBJFIRST, prop );
        #endif
        if( result < 0 )
        {
            WriteLog( "Register entity property '{}' getter fail, error {}.\n", name, result );
            return nullptr;
        }
    }

    // Register setter
    if( !disable_set )
    {
        char decl[ MAX_FOTEXT ];
        Str::Format( decl, "void set_%s(%s%s%s)", name, is_handle ? "const " : "", type_name, is_handle ? "@" : "" );
        int  result = -1;
        #ifdef AS_MAX_PORTABILITY
        if( data_type == Property::String )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( ( Property_SetValue_Generic< string >) ), asCALL_GENERIC, prop );
        else if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( ( Property_SetValue_Generic< void* >) ), asCALL_GENERIC, prop );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( ( Property_SetValue_Generic< char >) ), asCALL_GENERIC, prop );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( ( Property_SetValue_Generic< short >) ), asCALL_GENERIC, prop );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( ( Property_SetValue_Generic< int >) ), asCALL_GENERIC, prop );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asFUNCTION( ( Property_SetValue_Generic< int64 >) ), asCALL_GENERIC, prop );
        #else
        if( data_type == Property::String )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, SetValue< string >, ( Entity *, string ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, SetValue< void* >, ( Entity *, void* ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, SetValue< char >, ( Entity *, char ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, SetValue< short >, ( Entity *, short ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, SetValue< int >, ( Entity *, int ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl, asMETHODPR( Property, SetValue< int64 >, ( Entity *, int64 ), void ), asCALL_THISCALL_OBJFIRST, prop );
        #endif
        if( result < 0 )
        {
            WriteLog( "Register entity property '{}' setter fail, error {}.\n", name, result );
            return nullptr;
        }
    }

    // Register enum values for property reflection
    char enum_value_name[ MAX_FOTEXT ];
    Str::Format( enum_value_name, "%s::%s", enumTypeName.c_str(), name );
    int  enum_value = (int) Str::GetHash( enum_value_name );
    int  result = engine->RegisterEnumValue( enumTypeName.c_str(), name, enum_value );
    if( result < 0 )
    {
        WriteLog( "Register entity property enum '{}::{}' value {} fail, error {}.\n", enumTypeName.c_str(), name, enum_value, result );
        return nullptr;
    }

    // Add property to group
    if( group )
    {
        char full_decl[ MAX_FOTEXT ];
        Str::Format( full_decl, "const array<%s>@ %s%s", enumTypeName.c_str(), enumTypeName.c_str(), group );

        CScriptArray* group_array = nullptr;
        if( enumGroups.count( full_decl ) )
            group_array = enumGroups[ full_decl ];

        if( !group_array )
        {
            char decl[ MAX_FOTEXT ];
            Str::Format( decl, "array<%s>", enumTypeName.c_str() );
            asITypeInfo* enum_array_type = engine->GetTypeInfoByDecl( decl );
            if( !enum_array_type )
            {
                WriteLog( "Invalid type for property group '{}'.\n", decl );
                return nullptr;
            }

            group_array = CScriptArray::Create( enum_array_type );
            if( !enum_array_type )
            {
                WriteLog( "Can not create array type for property group '{}'.\n", decl );
                return nullptr;
            }

            enumGroups.insert( PAIR( string( full_decl ), group_array ) );

            int result = engine->RegisterGlobalProperty( full_decl, &enumGroups[ full_decl ] );
            if( result < 0 )
            {
                WriteLog( "Register entity property group '{}' fail, error {}.\n", full_decl, result );
                enumGroups.erase( full_decl );
                return nullptr;
            }
        }

        group_array->InsertLast( &enum_value );
    }

    // POD property data offset
    uint data_base_offset = uint( -1 );
    if( data_type == Property::POD && !disable_get && !( access & Property::VirtualMask ) )
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

    // Complex property data index
    uint complex_data_index = uint( -1 );
    if( data_type != Property::POD && ( !disable_get || !disable_set ) && !( access & Property::VirtualMask ) )
    {
        complex_data_index = complexPropertiesCount++;
        if( access & Property::PublicMask )
        {
            publicComplexDataProps.push_back( (ushort) reg_index );
            publicProtectedComplexDataProps.push_back( (ushort) reg_index );
        }
        else if( access & Property::ProtectedMask )
        {
            protectedComplexDataProps.push_back( (ushort) reg_index );
            publicProtectedComplexDataProps.push_back( (ushort) reg_index );
        }
        else if( access & Property::PrivateMask )
        {
            privateComplexDataProps.push_back( (ushort) reg_index );
        }
        else
        {
            RUNTIME_ASSERT( !"Unreachable place" );
        }
    }

    // Make entry
    prop->registrator = this;
    prop->regIndex = reg_index;
    prop->getIndex = ( !disable_get ? getPropertiesCount++ : uint( -1 ) );
    prop->enumValue = enum_value;
    prop->complexDataIndex = complex_data_index;
    prop->podDataOffset = data_base_offset;
    prop->baseSize = data_size;
    prop->getCallback = 0;
    prop->getCallbackArgs = 0;
    prop->nativeSetCallback = nullptr;

    prop->propName = name;
    prop->typeName = type_name;
    prop->dataType = data_type;
    prop->accessType = access;
    prop->isConst = is_const;
    prop->asObjTypeId = type_id;
    prop->asObjType = as_obj_type;
    prop->isHash = Str::Compare( type_name, "hash" );
    prop->isHashSubType0 = is_hash_sub0;
    prop->isHashSubType1 = is_hash_sub1;
    prop->isHashSubType2 = is_hash_sub2;
    prop->isResource = Str::Compare( type_name, "resource" );
    prop->isIntDataType = is_int_data_type;
    prop->isSignedIntDataType = is_signed_int_data_type;
    prop->isFloatDataType = is_float_data_type;
    prop->isEnumDataType = is_enum_data_type;
    prop->isBoolDataType = is_bool_data_type;
    prop->isArrayOfString = is_array_of_string;
    prop->isDictOfString = is_dict_of_string;
    prop->isDictOfArray = is_dict_of_array;
    prop->isDictOfArrayOfString = is_dict_of_array_of_string;
    prop->isReadable = !disable_get;
    prop->isWritable = !disable_set;
    prop->checkMinValue = ( min_value != nullptr && ( is_int_data_type || is_float_data_type ) );
    prop->checkMaxValue = ( max_value != nullptr && ( is_int_data_type || is_float_data_type ) );
    prop->minValue = ( min_value ? *min_value : 0 );
    prop->maxValue = ( max_value ? *max_value : 0 );

    registeredProperties.push_back( prop );

    if( prop->asObjType )
        prop->asObjType->AddRef();

    return prop;
}

void PropertyRegistrator::SetDefaults(
    const char* group /* = NULL */,
    int64* min_value /* = NULL */,
    int64* max_value     /* = NULL */
    )
{
    SAFEDELA( defaultGroup );
    SAFEDEL( defaultMinValue );
    SAFEDEL( defaultMaxValue );

    if( group )
        defaultGroup = Str::Duplicate( group );
    if( min_value )
        defaultMinValue = new int64( *min_value );
    if( max_value )
        defaultMaxValue = new int64( *max_value );
}

void PropertyRegistrator::FinishRegistration()
{
    RUNTIME_ASSERT( !registrationFinished );
    registrationFinished = true;

    // Fix POD data offsets
    for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
    {
        Property* prop = registeredProperties[ i ];
        if( prop->podDataOffset == uint( -1 ) )
            continue;

        if( prop->accessType & Property::ProtectedMask )
            prop->podDataOffset += (uint) publicPodDataSpace.size();
        else if( prop->accessType & Property::PrivateMask )
            prop->podDataOffset += (uint) publicPodDataSpace.size() + (uint) protectedPodDataSpace.size();
    }
}

uint PropertyRegistrator::GetCount()
{
    return (uint) registeredProperties.size();
}

Property* PropertyRegistrator::Get( uint property_index )
{
    if( property_index < (uint) registeredProperties.size() )
        return registeredProperties[ property_index ];
    return nullptr;
}

Property* PropertyRegistrator::Find( const char* property_name )
{
    for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
        if( Str::Compare( property_name, registeredProperties[ i ]->propName.c_str() ) )
            return registeredProperties[ i ];
    return nullptr;
}

Property* PropertyRegistrator::FindByEnum( int enum_value )
{
    for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
        if( registeredProperties[ i ]->enumValue == enum_value )
            return registeredProperties[ i ];
    return nullptr;
}

void PropertyRegistrator::SetNativeSetCallback( const char* property_name, NativeCallback callback )
{
    RUNTIME_ASSERT( property_name );
    Find( property_name )->nativeSetCallback = callback;
}

void PropertyRegistrator::SetNativeSendCallback( NativeSendCallback callback )
{
    for( size_t i = 0, j = registeredProperties.size(); i < j; i++ )
        registeredProperties[ i ]->nativeSendCallback = callback;
}

uint PropertyRegistrator::GetWholeDataSize()
{
    return wholePodDataSize;
}

string PropertyRegistrator::GetClassName()
{
    return scriptClassName;
}
