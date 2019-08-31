#include "Properties.h"
#include "Log.h"
#include "Testing.h"
#include "Entity.h"
#include "IniFile.h"
#include "Script.h"
#include "StringUtils.h"
#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
# include "DataBase.h"
#endif

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
        SCRIPT_ERROR_R( "Attempt to get property '{} {}::{}' on destroyed entity.",
                        typeName, properties->registrator->scriptClassName, propName );
    }

    // Virtual property
    if( accessType & Property::VirtualMask )
    {
        if( !getCallback )
        {
            SCRIPT_ERROR_R( "'Get' callback is not assigned for virtual property '{} {}::{}'.",
                            typeName, properties->registrator->scriptClassName, propName );
        }

        if( properties->getCallbackLocked[ getIndex ] )
        {
            SCRIPT_ERROR_R( "Recursive call for virtual property '{} {}::{}'.",
                            typeName, properties->registrator->scriptClassName, propName );
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
                memcpy( ret_value, Script::GetReturnedRawAddress(), sizeof( void* ) );
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
        SCRIPT_ERROR_R( "Attempt to set property '{} {}::{}' on destroyed entity.",
                        typeName, properties->registrator->scriptClassName, propName );
    }

    // Dereference ref types
    if( dataType == DataType::Array || dataType == DataType::Dict )
        new_value = *(void**) new_value;

    // Virtual property
    if( accessType & Property::VirtualMask )
    {
        if( setCallbacks.empty() )
        {
            SCRIPT_ERROR_R( "'Set' callback is not assigned for virtual property '{} {}::{}'.",
                            typeName, properties->registrator->scriptClassName, propName );
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
            SCRIPT_ERROR_R( "Attempt to set null on property '{} {}::{}'.",
                            typeName, properties->registrator->scriptClassName, propName );
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

string Property::GetName()
{
    return propName;
}

string Property::GetTypeName()
{
    return typeName;
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

bool Property::IsTemporary()
{
    return isTemporary;
}

bool Property::IsNoHistory()
{
    return isNoHistory;
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
       X_str( decl, "%s%s %s(const %s&,%s)", typeName.c_str(), !IsPOD() ? "@" : "", "%s", registrator->scriptClassName.c_str(), registrator->enumTypeName.c_str() );
       uint bind_id = Script::BindByFuncNameInRuntime( script_func, decl, false, true );
       if( !bind_id )
       {
        X_str( decl, "%s%s %s(const %s&)", typeName.c_str(), !IsPOD() ? "@" : "", "%s", registrator->scriptClassName.c_str() );
        bind_id = Script::BindByFuncNameInRuntime( script_func, decl, false, true );
        if( !bind_id )
        {
            char buf[ MAX_FOTEXT ];
            X_str( buf, decl, script_func );
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
       X_str( decl, "void %s(%s%s&,%s,%s&)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str(), registrator->enumTypeName.c_str(), typeName.c_str() );
       uint  bind_id = ( !deferred ? Script::BindByFuncNameInRuntime( script_func, decl, false, true ) : 0 );
       if( !bind_id )
       {
        X_str( decl, "void %s(%s%s&,%s&,%s)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str(), typeName.c_str(), registrator->enumTypeName.c_str() );
        bind_id = ( !deferred ? Script::BindByFuncNameInRuntime( script_func, decl, false, true ) : 0 );
        if( !bind_id )
        {
            X_str( decl, "void %s(%s%s&,%s)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str(), registrator->enumTypeName.c_str() );
            bind_id = Script::BindByFuncNameInRuntime( script_func, decl, false, true );
            if( !bind_id )
            {
                X_str( decl, "void %s(%s%s&,%s&)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str(), typeName.c_str() );
                bind_id = ( !deferred ? Script::BindByFuncNameInRuntime( script_func, decl, false, true ) : 0 );
                if( !bind_id )
                {
                    X_str( decl, "void %s(%s%s&)", "%s", deferred ? "" : "const ", registrator->scriptClassName.c_str() );
                    bind_id = Script::BindByFuncNameInRuntime( script_func, decl, false, true );
                    if( !bind_id )
                    {
                        char buf[ MAX_FOTEXT ];
                        X_str( buf, decl, script_func );
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

Property* Properties::FindByEnum( int enum_value )
{
    return registrator->FindByEnum( enum_value );
}

void* Properties::FindData( const string& property_name )
{
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
    RUNTIME_ASSERT( ( all_data_sizes[ 0 ] == publicSize || all_data_sizes[ 0 ] == publicSize + protectedSize ) );
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
    utf8::Decode( s, &length );
    while( length == 1 && ( *s == ' ' || *s == '\t' ) )
        utf8::Decode( ++s, &length );

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
                    utf8::Decode( s, &length );
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
            utf8::Decode( s, &length );
        }
    }
    else
    {
        begin = s;
        while( *s )
        {
            if( length == 1 && *s == '\\' )
            {
                utf8::Decode( ++s, &length );
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
            utf8::Decode( s, &length );
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
    if( !need_braces && deep == 0 && !str.empty() && ( str.find_first_of( " \t" ) == 0 || str.find_last_of( " \t" ) == str.length() - 1 ) )
        need_braces = true;
    if( !need_braces && str.length() >= 2 && str.back() == '\\' && str[ str.length() - 2 ] == ' ' )
        need_braces = true;

    string result;
    result.reserve( str.length() * 2 );

    if( need_braces )
        result.append( "{" );

    const char* s = str.c_str();
    uint length;
    while( *s )
    {
        utf8::Decode( s, &length );
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

    utf8::Decode( s, &length );
    bool is_braces = ( length == 1 && *s == '{' );
    if( is_braces )
        s++;

    while( *s )
    {
        utf8::Decode( s, &length );
        if( length == 1 && *s == '\\' )
        {
            utf8::Decode( ++s, &length );

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
        #define CHECK_PRIMITIVE( astype, ctype ) \
            if( type_id == astype )              \
                return _str( "{}", VALUE_AS( ctype ) )
        #define CHECK_PRIMITIVE_EXT( astype, ctype, ctype_str ) \
            if( type_id == astype )                             \
                return _str( "{}", (ctype_str) VALUE_AS( ctype ) )

        if( is_hashes[ deep ] )
        {
            RUNTIME_ASSERT( type_id == asTYPEID_UINT32 );
            return VALUE_AS( hash ) ? CodeString( _str().parseHash( VALUE_AS( hash ) ), deep ) : CodeString( "", deep );
        }

        CHECK_PRIMITIVE( asTYPEID_BOOL, bool );
        CHECK_PRIMITIVE_EXT( asTYPEID_INT8, char, int );
        CHECK_PRIMITIVE( asTYPEID_INT16, short );
        CHECK_PRIMITIVE( asTYPEID_INT32, int );
        CHECK_PRIMITIVE( asTYPEID_INT64, int64 );
        CHECK_PRIMITIVE( asTYPEID_UINT8, uchar );
        CHECK_PRIMITIVE( asTYPEID_UINT16, ushort );
        CHECK_PRIMITIVE( asTYPEID_UINT32, uint );
        CHECK_PRIMITIVE( asTYPEID_UINT64, uint64 );
        CHECK_PRIMITIVE( asTYPEID_DOUBLE, double );
        CHECK_PRIMITIVE( asTYPEID_FLOAT, float );
        return Script::GetEnumValueName( Script::GetEngine()->GetTypeDeclaration( type_id ), VALUE_AS( int ) );

        #undef VALUE_AS
        #undef CHECK_PRIMITIVE
        #undef CHECK_PRIMITIVE_EXT
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
                ctype v = (ctype) _str( value ).ato();  \
                memcpy( pod_buf, &v, sizeof( ctype ) ); \
                return pod_buf;                         \
            }

        if( is_hashes[ deep ] )
        {
            RUNTIME_ASSERT( type_id == asTYPEID_UINT32 );
            hash v = _str( DecodeString( value ) ).toHash();
            memcpy( pod_buf, &v, sizeof( v ) );
            return pod_buf;
        }

        CHECK_PRIMITIVE( asTYPEID_BOOL, bool, toBool );
        CHECK_PRIMITIVE( asTYPEID_INT8, char, toInt );
        CHECK_PRIMITIVE( asTYPEID_INT16, short, toInt );
        CHECK_PRIMITIVE( asTYPEID_INT32, int, toInt );
        CHECK_PRIMITIVE( asTYPEID_INT64, int64, toInt64 );
        CHECK_PRIMITIVE( asTYPEID_UINT8, uchar, toUInt );
        CHECK_PRIMITIVE( asTYPEID_UINT16, ushort, toUInt );
        CHECK_PRIMITIVE( asTYPEID_UINT32, uint, toUInt );
        CHECK_PRIMITIVE( asTYPEID_UINT64, uint64, toUInt64 );
        CHECK_PRIMITIVE( asTYPEID_DOUBLE, double, toDouble );
        CHECK_PRIMITIVE( asTYPEID_FLOAT, float, toFloat );

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
        const string& key = kv.first;
        const string& value = kv.second;

        // Skip technical fields
        if( key.empty() || key[ 0 ] == '$' || key[ 0 ] == '_' )
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
    RUNTIME_ASSERT( ( !base || registrator == base->registrator ) );

    for( auto& prop : registrator->registeredProperties )
    {
        // Skip pure virtual properties
        if( prop->podDataOffset == uint( -1 ) && prop->complexDataIndex == uint( -1 ) )
            continue;

        // Skip temporary properties
        if( prop->isTemporary )
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
        key_values.insert( std::make_pair( prop->propName, SavePropertyToText( prop ) ) );
    }
}

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
DataBase::Document Properties::SaveToDbDocument( Properties* base )
{
    RUNTIME_ASSERT( ( !base || registrator == base->registrator ) );

    DataBase::Document doc;
    for( auto& prop : registrator->registeredProperties )
    {
        // Skip pure virtual properties
        if( prop->podDataOffset == uint( -1 ) && prop->complexDataIndex == uint( -1 ) )
            continue;

        // Skip temporary properties
        if( prop->isTemporary )
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

        doc.insert( std::make_pair( prop->propName, SavePropertyToDbValue( prop ) ) );
    }

    return doc;
}

bool Properties::LoadFromDbDocument( const DataBase::Document& doc )
{
    bool is_error = false;
    for( const auto& kv : doc )
    {
        const string& key = kv.first;
        const DataBase::Value& value = kv.second;

        // Skip technical fields
        if( key.empty() || key[ 0 ] == '$' || key[ 0 ] == '_' )
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

        // Parse value
        if( prop->isEnumDataType )
        {
            if( value.which() != DataBase::StringValue )
            {
                WriteLog( "Wrong enum value type, property '{}'.\n", prop->GetName() );
                is_error = true;
                continue;
            }

            int e = Script::GetEnumValue( prop->asObjType->GetName(), value.get< string >(), is_error );
            prop->SetPropRawData( this, (uchar*) &e, prop->baseSize );
        }
        else if( prop->isHash || prop->isResource )
        {
            if( value.which() != DataBase::StringValue )
            {
                WriteLog( "Wrong hash value type, property '{}'.\n", prop->GetName() );
                is_error = true;
                continue;
            }

            hash h = _str( value.get< string >() ).toHash();
            prop->SetPropRawData( this, (uchar*) &h, prop->baseSize );
        }
        else if( prop->isIntDataType || prop->isFloatDataType || prop->isBoolDataType )
        {
            if( value.which() == DataBase::StringValue || value.which() == DataBase::ArrayValue || value.which() == DataBase::DictValue )
            {
                WriteLog( "Wrong integer value type, property '{}'.\n", prop->GetName() );
                is_error = true;
                continue;
            }

            # define PARSE_VALUE( t )                                     \
                do                                                        \
                {                                                         \
                    if( prop->asObjTypeId == asTYPEID_INT8 )              \
                        *(char*) pod_data = (char) value.get< t >();      \
                    else if( prop->asObjTypeId == asTYPEID_INT16 )        \
                        *(short*) pod_data = (short) value.get< t >();    \
                    else if( prop->asObjTypeId == asTYPEID_INT32 )        \
                        *(int*) pod_data = (int) value.get< t >();        \
                    else if( prop->asObjTypeId == asTYPEID_INT64 )        \
                        *(int64*) pod_data = (int64) value.get< t >();    \
                    else if( prop->asObjTypeId == asTYPEID_UINT8 )        \
                        *(uchar*) pod_data = (uchar) value.get< t >();    \
                    else if( prop->asObjTypeId == asTYPEID_UINT16 )       \
                        *(short*) pod_data = (short) value.get< t >();    \
                    else if( prop->asObjTypeId == asTYPEID_UINT32 )       \
                        *(uint*) pod_data = (uint) value.get< t >();      \
                    else if( prop->asObjTypeId == asTYPEID_UINT64 )       \
                        *(uint64*) pod_data = (uint64) value.get< t >();  \
                    else if( prop->asObjTypeId == asTYPEID_FLOAT )        \
                        *(float*) pod_data = (float) value.get< t >();    \
                    else if( prop->asObjTypeId == asTYPEID_DOUBLE )       \
                        *(double*) pod_data = (double) value.get< t >();  \
                    else if( prop->asObjTypeId == asTYPEID_BOOL )         \
                        *(bool*) pod_data = value.get< t >() != 0;        \
                    else                                                  \
                        RUNTIME_ASSERT( !"Unreachable place" );           \
                }                                                         \
                while( false )

            uchar pod_data[ 8 ];
            if( value.which() == DataBase::IntValue )
                PARSE_VALUE( int );
            else if( value.which() == DataBase::Int64Value )
                PARSE_VALUE( int64 );
            else if( value.which() == DataBase::DoubleValue )
                PARSE_VALUE( double );
            else if( value.which() == DataBase::BoolValue )
                PARSE_VALUE( bool );
            else
                RUNTIME_ASSERT( !"Unreachable place" );

            # undef PARSE_VALUE

            prop->SetPropRawData( this, pod_data, prop->baseSize );
        }
        else if( prop->dataType == Property::String )
        {
            if( value.which() != DataBase::StringValue )
            {
                WriteLog( "Wrong string value type, property '{}'.\n", prop->GetName() );
                is_error = true;
                continue;
            }

            prop->SetPropRawData( this, (uchar*) value.get< string >().c_str(), (uint) value.get< string >().length() );
        }
        else if( prop->dataType == Property::Array )
        {
            if( value.which() != DataBase::ArrayValue )
            {
                WriteLog( "Wrong array value type, property '{}'.\n", prop->GetName() );
                is_error = true;
                continue;
            }

            const DataBase::Array& arr = value.get< DataBase::Array >();
            if( arr.empty() )
            {
                prop->SetPropRawData( this, nullptr, 0 );
                continue;
            }

            if( prop->isHashSubType0 )
            {
                if( arr[ 0 ].which() != DataBase::StringValue )
                {
                    WriteLog( "Wrong array hash element value type, property '{}'.\n", prop->GetName() );
                    is_error = true;
                    continue;
                }

                uint data_size = (uint) arr.size() * sizeof( hash );
                uchar* data = new uchar[ data_size ];
                for( size_t i = 0; i < arr.size(); i++ )
                {
                    RUNTIME_ASSERT( ( arr[ i ].which() == DataBase::StringValue ) );

                    hash h = _str( arr[ i ].get< string >() ).toHash();
                    *(hash*) ( data + i * sizeof( hash ) ) = h;
                }

                prop->SetPropRawData( this, data, data_size );
                delete[] data;
            }
            else if( !( prop->asObjType->GetSubTypeId() & asTYPEID_MASK_OBJECT ) && prop->asObjType->GetSubTypeId() > asTYPEID_DOUBLE )
            {
                if( arr[ 0 ].which() != DataBase::StringValue )
                {
                    WriteLog( "Wrong array enum element value type, property '{}'.\n", prop->GetName() );
                    is_error = true;
                    continue;
                }

                string enum_name = prop->asObjType->GetSubType()->GetName();
                uint data_size = (uint) arr.size() * sizeof( int );
                uchar* data = new uchar[ data_size ];
                for( size_t i = 0; i < arr.size(); i++ )
                {
                    RUNTIME_ASSERT( ( arr[ i ].which() == DataBase::StringValue ) );

                    int e = Script::GetEnumValue( enum_name, arr[ i ].get< string >(), is_error );
                    *(int*) ( data + i * sizeof( int ) ) = e;
                }

                prop->SetPropRawData( this, data, data_size );
                delete[] data;
            }
            else if( !( prop->asObjType->GetSubTypeId() & asTYPEID_MASK_OBJECT ) )
            {
                if( arr[ 0 ].which() == DataBase::StringValue || arr[ 0 ].which() == DataBase::ArrayValue || arr[ 0 ].which() == DataBase::DictValue )
                {
                    WriteLog( "Wrong array element value type, property '{}'.\n", prop->GetName() );
                    is_error = true;
                    continue;
                }

                int element_type_id = prop->asObjType->GetSubTypeId();
                int element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( element_type_id );
                uint data_size = element_size * (int) arr.size();
                uchar* data = new uchar[ data_size ];
                int arr_element_index = arr[ 0 ].which();

                # define PARSE_VALUE( t )                                                          \
                    do                                                                             \
                    {                                                                              \
                        for( size_t i = 0; i < arr.size(); i++ )                                   \
                        {                                                                          \
                            RUNTIME_ASSERT( arr[ i ].which() == arr_element_index );               \
                            if( arr_element_index == DataBase::IntValue )                          \
                                *(t*) ( data + i * element_size ) = (t) arr[ i ].get< int >();     \
                            else if( arr_element_index == DataBase::Int64Value )                   \
                                *(t*) ( data + i * element_size ) = (t) arr[ i ].get< int64 >();   \
                            else if( arr_element_index == DataBase::DoubleValue )                  \
                                *(t*) ( data + i * element_size ) = (t) arr[ i ].get< double >();  \
                            else if( arr_element_index == DataBase::BoolValue )                    \
                                *(t*) ( data + i * element_size ) = (t) arr[ i ].get< bool >();    \
                            else                                                                   \
                                RUNTIME_ASSERT( !"Unreachable place" );                            \
                        }                                                                          \
                    } while( false )

                if( element_type_id == asTYPEID_INT8 )
                    PARSE_VALUE( char );
                else if( element_type_id == asTYPEID_INT16 )
                    PARSE_VALUE( short );
                else if( element_type_id == asTYPEID_INT32 )
                    PARSE_VALUE( int );
                else if( element_type_id == asTYPEID_INT64 )
                    PARSE_VALUE( int64 );
                else if( element_type_id == asTYPEID_UINT8 )
                    PARSE_VALUE( uchar );
                else if( element_type_id == asTYPEID_UINT16 )
                    PARSE_VALUE( ushort );
                else if( element_type_id == asTYPEID_UINT32 )
                    PARSE_VALUE( uint );
                else if( element_type_id == asTYPEID_UINT64 )
                    PARSE_VALUE( uint64 );
                else if( element_type_id == asTYPEID_FLOAT )
                    PARSE_VALUE( float );
                else if( element_type_id == asTYPEID_DOUBLE )
                    PARSE_VALUE( double );
                else if( element_type_id == asTYPEID_BOOL )
                    PARSE_VALUE( bool );
                else
                    RUNTIME_ASSERT( !"Unreachable place" );

                # undef PARSE_VALUE

                prop->SetPropRawData( this, data, data_size );
                delete[] data;
            }
            else
            {
                RUNTIME_ASSERT( prop->isArrayOfString );

                if( arr[ 0 ].which() != DataBase::StringValue )
                {
                    WriteLog( "Wrong array element value type, property '{}'.\n", prop->GetName() );
                    is_error = true;
                    continue;
                }

                uint data_size = sizeof( uint );
                for( size_t i = 0; i < arr.size(); i++ )
                {
                    RUNTIME_ASSERT( ( arr[ i ].which() == DataBase::StringValue ) );

                    const string& str = arr[ i ].get< string >();
                    data_size += sizeof( uint ) + (uint) str.length();
                }

                uchar* data = new uchar[ data_size ];
                *(uint*) data = (uint) arr.size();
                size_t data_pos = sizeof( uint );
                for( size_t i = 0; i < arr.size(); i++ )
                {
                    const string& str = arr[ i ].get< string >();
                    *(uint*) ( data + data_pos ) = (uint) str.length();
                    if( !str.empty() )
                        memcpy( data + data_pos + sizeof( uint ), str.c_str(), str.length() );
                    data_pos += sizeof( uint ) + str.length();
                }
                RUNTIME_ASSERT( data_pos == data_size );

                prop->SetPropRawData( this, data, data_size );
                delete[] data;
            }
        }
        else if( prop->dataType == Property::Dict )
        {
            if( value.which() != DataBase::DictValue )
            {
                WriteLog( "Wrong dict value type, property '{}'.\n", prop->GetName() );
                is_error = true;
                continue;
            }

            const DataBase::Dict& dict = value.get< DataBase::Dict >();
            if( dict.empty() )
            {
                prop->SetPropRawData( this, nullptr, 0 );
                continue;
            }

            int key_element_type_id = prop->asObjType->GetSubTypeId();
            RUNTIME_ASSERT( !( key_element_type_id & asTYPEID_MASK_OBJECT ) );
            int key_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( key_element_type_id );
            string key_element_type_name = prop->asObjType->GetSubType() ? prop->asObjType->GetSubType()->GetName() : "";
            int value_element_type_id = prop->asObjType->GetSubTypeId( 1 );
            int value_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( value_element_type_id );
            string value_element_type_name = prop->asObjType->GetSubType( 1 ) ? prop->asObjType->GetSubType( 1 )->GetName() : "";

            // Measure data length
            uint data_size = 0;
            bool wrong_input = false;
            for( auto& kv : dict )
            {
                data_size += key_element_size;

                if( prop->isDictOfArray )
                {
                    if( kv.second.which() != DataBase::ArrayValue )
                    {
                        WriteLog( "Wrong dict array value type, property '{}'.\n", prop->GetName() );
                        wrong_input = true;
                        break;
                    }

                    const DataBase::Array& arr = kv.second.get< DataBase::Array >();
                    int arr_element_type_id = prop->asObjType->GetSubType( 1 )->GetSubTypeId();
                    int arr_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( arr_element_type_id );

                    data_size += sizeof( uint );

                    if( !( arr_element_type_id & asTYPEID_MASK_OBJECT ) && arr_element_type_id > asTYPEID_DOUBLE )
                    {
                        for( auto& e : arr )
                        {
                            if( e.which() != DataBase::StringValue )
                            {
                                WriteLog( "Wrong dict array element enum value type, property '{}'.\n", prop->GetName() );
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += (uint) arr.size() * sizeof( int );
                    }
                    else if( prop->isHashSubType2 )
                    {
                        for( auto& e : arr )
                        {
                            if( e.which() != DataBase::StringValue )
                            {
                                WriteLog( "Wrong dict array element hash value type, property '{}'.\n", prop->GetName() );
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += (uint) arr.size() * sizeof( hash );
                    }
                    else if( prop->isDictOfArrayOfString )
                    {
                        for( auto& e : arr )
                        {
                            if( e.which() != DataBase::StringValue )
                            {
                                WriteLog( "Wrong dict array element string value type, property '{}'.\n", prop->GetName() );
                                wrong_input = true;
                                break;
                            }

                            data_size += sizeof( uint ) + (uint) e.get< string >().length();
                        }
                    }
                    else
                    {
                        for( auto& e : arr )
                        {
                            if( ( e.which() != DataBase::IntValue && e.which() != DataBase::Int64Value &&
                                  e.which() != DataBase::DoubleValue && e.which() != DataBase::BoolValue ) || e.which() != arr[ 0 ].which() )
                            {
                                WriteLog( "Wrong dict array element value type, property '{}'.\n", prop->GetName() );
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += (int) arr.size() * arr_element_size;
                    }
                }
                else if( prop->isDictOfString )
                {
                    if( kv.second.which() != DataBase::StringValue )
                    {
                        WriteLog( "Wrong dict string element value type, property '{}'.\n", prop->GetName() );
                        wrong_input = true;
                        break;
                    }

                    data_size += (uint) kv.second.get< string >().length();
                }
                else if( !( value_element_type_id & asTYPEID_MASK_OBJECT ) && value_element_type_id > asTYPEID_DOUBLE )
                {
                    if( kv.second.which() != DataBase::StringValue )
                    {
                        WriteLog( "Wrong dict enum element value type, property '{}'.\n", prop->GetName() );
                        wrong_input = true;
                        break;
                    }

                    RUNTIME_ASSERT( value_element_size == sizeof( int ) );
                    data_size += sizeof( int );
                }
                else if( prop->isHashSubType1 )
                {
                    if( kv.second.which() != DataBase::StringValue )
                    {
                        WriteLog( "Wrong dict hash element value type, property '{}'.\n", prop->GetName() );
                        wrong_input = true;
                        break;
                    }

                    RUNTIME_ASSERT( value_element_size == sizeof( hash ) );
                    data_size += sizeof( hash );
                }
                else
                {
                    if( kv.second.which() != DataBase::IntValue && kv.second.which() != DataBase::Int64Value &&
                        kv.second.which() != DataBase::DoubleValue && kv.second.which() != DataBase::BoolValue )
                    {
                        WriteLog( "Wrong dict number element value type, property '{}'.\n", prop->GetName() );
                        wrong_input = true;
                        break;
                    }

                    data_size += value_element_size;
                }

                if( wrong_input )
                    break;
            }

            if( wrong_input )
            {
                is_error = true;
                continue;
            }

            // Write data
            uchar* data = new uchar[ data_size ];
            size_t data_pos = 0;
            for( auto& kv : dict )
            {
                if( prop->isHashSubType0 )
                    *(hash*) ( data + data_pos ) = _str( kv.first ).toHash();
                else if( key_element_type_id == asTYPEID_INT8 )
                    *(char*) ( data + data_pos ) = (char) _str( kv.first ).toInt64();
                else if( key_element_type_id == asTYPEID_INT16 )
                    *(short*) ( data + data_pos ) = (short) _str( kv.first ).toInt64();
                else if( key_element_type_id == asTYPEID_INT32 )
                    *(int*) ( data + data_pos ) = (int) _str( kv.first ).toInt64();
                else if( key_element_type_id == asTYPEID_INT64 )
                    *(int64*) ( data + data_pos ) = (int64) _str( kv.first ).toInt64();
                else if( key_element_type_id == asTYPEID_UINT8 )
                    *(uchar*) ( data + data_pos ) = (uchar) _str( kv.first ).toInt64();
                else if( key_element_type_id == asTYPEID_UINT16 )
                    *(ushort*) ( data + data_pos ) = (ushort) _str( kv.first ).toInt64();
                else if( key_element_type_id == asTYPEID_UINT32 )
                    *(uint*) ( data + data_pos ) = (uint) _str( kv.first ).toInt64();
                else if( key_element_type_id == asTYPEID_UINT64 )
                    *(uint64*) ( data + data_pos ) = (uint64) _str( kv.first ).toInt64();
                else if( key_element_type_id == asTYPEID_FLOAT )
                    *(float*) ( data + data_pos ) = (float) _str( kv.first ).toDouble();
                else if( key_element_type_id == asTYPEID_DOUBLE )
                    *(double*) ( data + data_pos ) = (double) _str( kv.first ).toDouble();
                else if( key_element_type_id == asTYPEID_BOOL )
                    *(bool*) ( data + data_pos ) = (bool) _str( kv.first ).toBool();
                else if( !( key_element_type_id & asTYPEID_MASK_OBJECT ) && key_element_type_id > asTYPEID_DOUBLE )
                    *(int*) ( data + data_pos ) = Script::GetEnumValue( key_element_type_name, kv.first, is_error );
                else
                    RUNTIME_ASSERT( !"Unreachable place" );

                data_pos += key_element_size;

                if( prop->isDictOfArray )
                {
                    const DataBase::Array& arr = kv.second.get< DataBase::Array >();

                    *(uint*) ( data + data_pos ) = (uint) arr.size();
                    data_pos += sizeof( uint );

                    int arr_element_type_id = prop->asObjType->GetSubType( 1 )->GetSubTypeId();
                    int arr_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( arr_element_type_id );

                    if( !( arr_element_type_id & asTYPEID_MASK_OBJECT ) && arr_element_type_id > asTYPEID_DOUBLE )
                    {
                        string arr_element_type_name = prop->asObjType->GetSubType( 1 )->GetSubType()->GetName();
                        for( auto& e : arr )
                        {
                            *(int*) ( data + data_pos ) = Script::GetEnumValue( arr_element_type_name, e.get< string >(), is_error );
                            data_pos += sizeof( int );
                        }
                    }
                    else if( prop->isHashSubType2 )
                    {
                        for( auto& e : arr )
                        {
                            *(hash*) ( data + data_pos ) = _str( e.get< string >() ).toHash();
                            data_pos += sizeof( hash );
                        }
                    }
                    else if( prop->isDictOfArrayOfString )
                    {
                        for( auto& e : arr )
                        {
                            const string& str = e.get< string >();
                            *(uint*) ( data + data_pos ) = (uint) str.length();
                            data_pos += sizeof( uint );
                            if( !str.empty() )
                            {
                                memcpy( data + data_pos, str.c_str(), str.length() );
                                data_pos += (uint) str.length();
                            }
                        }
                    }
                    else
                    {
                        for( auto& e : arr )
                        {
                            # define PARSE_VALUE( t )                                       \
                                do                                                          \
                                {                                                           \
                                    RUNTIME_ASSERT( sizeof( t ) == arr_element_size );      \
                                    if( e.which() == DataBase::IntValue )                   \
                                        *(t*) ( data + data_pos ) = (t) e.get< int >();     \
                                    else if( e.which() == DataBase::Int64Value )            \
                                        *(t*) ( data + data_pos ) = (t) e.get< int64 >();   \
                                    else if( e.which() == DataBase::DoubleValue )           \
                                        *(t*) ( data + data_pos ) = (t) e.get< double >();  \
                                    else if( e.which() == DataBase::BoolValue )             \
                                        *(t*) ( data + data_pos ) = (t) e.get< bool >();    \
                                    else                                                    \
                                        RUNTIME_ASSERT( !"Unreachable place" );             \
                                } while( false )

                            if( arr_element_type_id == asTYPEID_INT8 )
                                PARSE_VALUE( char );
                            else if( arr_element_type_id == asTYPEID_INT16 )
                                PARSE_VALUE( short );
                            else if( arr_element_type_id == asTYPEID_INT32 )
                                PARSE_VALUE( int );
                            else if( arr_element_type_id == asTYPEID_INT64 )
                                PARSE_VALUE( int64 );
                            else if( arr_element_type_id == asTYPEID_UINT8 )
                                PARSE_VALUE( uchar );
                            else if( arr_element_type_id == asTYPEID_UINT16 )
                                PARSE_VALUE( ushort );
                            else if( arr_element_type_id == asTYPEID_UINT32 )
                                PARSE_VALUE( uint );
                            else if( arr_element_type_id == asTYPEID_UINT64 )
                                PARSE_VALUE( uint64 );
                            else if( arr_element_type_id == asTYPEID_FLOAT )
                                PARSE_VALUE( float );
                            else if( arr_element_type_id == asTYPEID_DOUBLE )
                                PARSE_VALUE( double );
                            else if( arr_element_type_id == asTYPEID_BOOL )
                                PARSE_VALUE( bool );
                            else
                                RUNTIME_ASSERT( !"Unreachable place" );

                            # undef PARSE_VALUE

                            data_pos += arr_element_size;
                        }
                    }
                }
                else if( prop->isDictOfString )
                {
                    const string& str = kv.second.get< string >();

                    *(uint*) ( data + data_pos ) = (uint) str.length();
                    data_pos += sizeof( uint );

                    if( !str.empty() )
                    {
                        memcpy( data + data_pos, str.c_str(), str.length() );
                        data_pos += str.length();
                    }
                }
                else
                {
                    RUNTIME_ASSERT( !( value_element_type_id & asTYPEID_MASK_OBJECT ) );

                    # define PARSE_VALUE( t )                                               \
                        do                                                                  \
                        {                                                                   \
                            RUNTIME_ASSERT( sizeof( t ) == value_element_size );            \
                            if( kv.second.which() == DataBase::IntValue )                   \
                                *(t*) ( data + data_pos ) = (t) kv.second.get< int >();     \
                            else if( kv.second.which() == DataBase::Int64Value )            \
                                *(t*) ( data + data_pos ) = (t) kv.second.get< int64 >();   \
                            else if( kv.second.which() == DataBase::DoubleValue )           \
                                *(t*) ( data + data_pos ) = (t) kv.second.get< double >();  \
                            else if( kv.second.which() == DataBase::BoolValue )             \
                                *(t*) ( data + data_pos ) = (t) kv.second.get< bool >();    \
                            else                                                            \
                                RUNTIME_ASSERT( !"Unreachable place" );                     \
                        } while( false )

                    if( prop->isHashSubType1 )
                        *(hash*) ( data + data_pos ) = _str( kv.second.get< string >() ).toHash();
                    else if( value_element_type_id == asTYPEID_INT8 )
                        PARSE_VALUE( char );
                    else if( value_element_type_id == asTYPEID_INT16 )
                        PARSE_VALUE( short );
                    else if( value_element_type_id == asTYPEID_INT32 )
                        PARSE_VALUE( int );
                    else if( value_element_type_id == asTYPEID_INT64 )
                        PARSE_VALUE( int64 );
                    else if( value_element_type_id == asTYPEID_UINT8 )
                        PARSE_VALUE( uchar );
                    else if( value_element_type_id == asTYPEID_UINT16 )
                        PARSE_VALUE( ushort );
                    else if( value_element_type_id == asTYPEID_UINT32 )
                        PARSE_VALUE( uint );
                    else if( value_element_type_id == asTYPEID_UINT64 )
                        PARSE_VALUE( uint64 );
                    else if( value_element_type_id == asTYPEID_FLOAT )
                        PARSE_VALUE( float );
                    else if( value_element_type_id == asTYPEID_DOUBLE )
                        PARSE_VALUE( double );
                    else if( value_element_type_id == asTYPEID_BOOL )
                        PARSE_VALUE( bool );
                    else
                        *(int*) ( data + data_pos ) = Script::GetEnumValue( value_element_type_name, kv.second.get< string >(), is_error );

                    # undef PARSE_VALUE

                    data_pos += value_element_size;
                }
            }

            if( data_pos != data_size )
                WriteLog( "{} {}\n", data_pos, data_size );
            RUNTIME_ASSERT( data_pos == data_size );

            prop->SetPropRawData( this, data, data_size );
            delete[] data;
        }
        else
        {
            RUNTIME_ASSERT( !"Unreachable place" );
        }
    }
    return !is_error;
}

DataBase::Value Properties::SavePropertyToDbValue( Property* prop )
{
    RUNTIME_ASSERT( ( prop->podDataOffset != uint( -1 ) || prop->complexDataIndex != uint( -1 ) ) );
    RUNTIME_ASSERT( !prop->isTemporary );

    uint data_size;
    uchar* data = prop->GetPropRawData( this, data_size );

    if( prop->dataType == Property::POD )
    {
        RUNTIME_ASSERT( prop->podDataOffset != uint( -1 ) );

        if( prop->isHash || prop->isResource )
            return _str().parseHash( *(hash*) &podData[ prop->podDataOffset ] ).str();

        # define PARSE_VALUE( as_t, t, ret_t ) \
            if( prop->asObjTypeId == as_t )    \
                return (ret_t) *(t*) &podData[ prop->podDataOffset ];

        PARSE_VALUE( asTYPEID_INT8, char, int );
        PARSE_VALUE( asTYPEID_INT16, short, int );
        PARSE_VALUE( asTYPEID_INT32, int, int );
        PARSE_VALUE( asTYPEID_INT64, int64, int64 );
        PARSE_VALUE( asTYPEID_UINT8, uchar, int );
        PARSE_VALUE( asTYPEID_UINT16, ushort, int );
        PARSE_VALUE( asTYPEID_UINT32, uint, int );
        PARSE_VALUE( asTYPEID_UINT64, uint64, int64 );
        PARSE_VALUE( asTYPEID_FLOAT, float, double );
        PARSE_VALUE( asTYPEID_DOUBLE, double, double );
        PARSE_VALUE( asTYPEID_BOOL, bool, bool );

        # undef PARSE_VALUE

        return Script::GetEnumValueName( prop->asObjType->GetName(), *(int*) &podData[ prop->podDataOffset ] );
    }
    else if( prop->dataType == Property::String )
    {
        RUNTIME_ASSERT( prop->complexDataIndex != uint( -1 ) );

        uchar* data = complexData[ prop->complexDataIndex ];
        uint   data_size = complexDataSizes[ prop->complexDataIndex ];
        if( data_size )
            return string( (char*) data, data_size );
        return string();
    }
    else if( prop->dataType == Property::Array )
    {
        if( prop->isArrayOfString )
        {
            if( data_size )
            {
                uint arr_size;
                memcpy( &arr_size, data, sizeof( arr_size ) );
                data += sizeof( uint );
                DataBase::Array arr;
                arr.reserve( arr_size );
                for( uint i = 0; i < arr_size; i++ )
                {
                    uint str_size;
                    memcpy( &str_size, data, sizeof( str_size ) );
                    data += sizeof( uint );
                    string str( (char*) data, str_size );
                    arr.push_back( std::move( str ) );
                    data += str_size;
                }
                return arr;
            }
            else
            {
                return DataBase::Array();
            }
        }
        else
        {
            int element_type_id = prop->asObjType->GetSubTypeId();
            uint element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( element_type_id );
            string element_type_name = prop->asObjType->GetSubType() ? prop->asObjType->GetSubType()->GetName() : "";
            uint arr_size = data_size / element_size;
            DataBase::Array arr;
            arr.reserve( arr_size );
            for( uint i = 0; i < arr_size; i++ )
            {
                if( prop->isHashSubType0 )
                    arr.push_back( _str().parseHash( *(hash*) ( data + i * element_size ) ).str() );
                else if( element_type_id == asTYPEID_INT8 )
                    arr.push_back( (int) *(char*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_INT16 )
                    arr.push_back( (int) *(short*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_INT32 )
                    arr.push_back( (int) *(int*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_INT64 )
                    arr.push_back( ( int64 ) * (int64*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_UINT8 )
                    arr.push_back( (int) *(uchar*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_UINT16 )
                    arr.push_back( (int) *(ushort*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_UINT32 )
                    arr.push_back( (int) *(uint*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_UINT64 )
                    arr.push_back( ( int64 ) * (uint64*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_FLOAT )
                    arr.push_back( (double) *(float*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_DOUBLE )
                    arr.push_back( (double) *(double*) ( data + i * element_size ) );
                else if( element_type_id == asTYPEID_BOOL )
                    arr.push_back( (bool) *(bool*) ( data + i * element_size ) );
                else if( !( element_type_id & asTYPEID_MASK_OBJECT ) && element_type_id > asTYPEID_DOUBLE )
                    arr.push_back( Script::GetEnumValueName( element_type_name, *(int*) ( data + i * element_size ) ) );
                else
                    RUNTIME_ASSERT( !"Unreachable place" );
            }
            return arr;
        }
    }
    else if( prop->dataType == Property::Dict )
    {
        DataBase::Dict dict;
        if( data_size )
        {
            auto get_key_string = [] ( void* p, int type_id, string type_name, bool is_hash )->string
            {
                if( is_hash )
                    return _str().parseHash( *(hash*) p ).str();
                else if( type_id == asTYPEID_INT8 )
                    return _str( "{}", (int) *(char*) p ).str();
                else if( type_id == asTYPEID_INT16 )
                    return _str( "{}", (int) *(short*) p ).str();
                else if( type_id == asTYPEID_INT32 )
                    return _str( "{}", (int) *(int*) p ).str();
                else if( type_id == asTYPEID_INT64 )
                    return _str( "{}", ( int64 ) * (int64*) p ).str();
                else if( type_id == asTYPEID_UINT8 )
                    return _str( "{}", (int) *(uchar*) p ).str();
                else if( type_id == asTYPEID_UINT16 )
                    return _str( "{}", (int) *(ushort*) p ).str();
                else if( type_id == asTYPEID_UINT32 )
                    return _str( "{}", (int) *(uint*) p ).str();
                else if( type_id == asTYPEID_UINT64 )
                    return _str( "{}", ( int64 ) * (uint64*) p ).str();
                else if( type_id == asTYPEID_FLOAT )
                    return _str( "{}", (double) *(float*) p ).str();
                else if( type_id == asTYPEID_DOUBLE )
                    return _str( "{}", (double) *(double*) p ).str();
                else if( type_id == asTYPEID_BOOL )
                    return _str( "{}", (bool) *(bool*) p ).str();
                else if( !( type_id & asTYPEID_MASK_OBJECT ) && type_id > asTYPEID_DOUBLE )
                    return Script::GetEnumValueName( type_name, *(int*) p );
                else
                    RUNTIME_ASSERT( !"Unreachable place" );
                return string();
            };

            int          key_element_type_id = prop->asObjType->GetSubTypeId();
            string       key_element_type_name = prop->asObjType->GetSubType() ? prop->asObjType->GetSubType()->GetName() : "";
            uint key_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( key_element_type_id );

            if( prop->isDictOfArray )
            {
                asITypeInfo* arr_type = prop->asObjType->GetSubType( 1 );
                int          arr_element_type_id = arr_type->GetSubTypeId();
                string       arr_element_type_name = arr_type->GetSubType() ? arr_type->GetSubType()->GetName() : "";
                uint         arr_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( arr_element_type_id );
                uchar*       data_end = data + data_size;
                while( data < data_end )
                {
                    void* key = data;
                    data += key_element_size;
                    uint  arr_size;
                    memcpy( &arr_size, data, sizeof( arr_size ) );
                    data += sizeof( uint );
                    DataBase::Array arr;
                    arr.reserve( arr_size );
                    if( arr_size )
                    {
                        if( prop->isDictOfArrayOfString )
                        {
                            for( uint i = 0; i < arr_size; i++ )
                            {
                                uint str_size;
                                memcpy( &str_size, data, sizeof( str_size ) );
                                data += sizeof( uint );
                                string str( (char*) data, str_size );
                                arr.push_back( std::move( str ) );
                                data += str_size;
                            }
                        }
                        else
                        {
                            for( uint i = 0; i < arr_size; i++ )
                            {
                                if( prop->isHashSubType2 )
                                    arr.push_back( _str().parseHash( *(hash*) ( data + i * arr_element_size ) ).str() );
                                else if( arr_element_type_id == asTYPEID_INT8 )
                                    arr.push_back( (int) *(char*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_INT16 )
                                    arr.push_back( (int) *(short*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_INT32 )
                                    arr.push_back( (int) *(int*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_INT64 )
                                    arr.push_back( ( int64 ) * (int64*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_UINT8 )
                                    arr.push_back( (int) *(uchar*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_UINT16 )
                                    arr.push_back( (int) *(ushort*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_UINT32 )
                                    arr.push_back( (int) *(uint*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_UINT64 )
                                    arr.push_back( ( int64 ) * (uint64*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_FLOAT )
                                    arr.push_back( (double) *(float*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_DOUBLE )
                                    arr.push_back( (double) *(double*) ( data + i * arr_element_size ) );
                                else if( arr_element_type_id == asTYPEID_BOOL )
                                    arr.push_back( (bool) *(bool*) ( data + i * arr_element_size ) );
                                else if( !( arr_element_type_id & asTYPEID_MASK_OBJECT ) && arr_element_type_id > asTYPEID_DOUBLE )
                                    arr.push_back( Script::GetEnumValueName( arr_element_type_name, *(int*) ( data + i * arr_element_size ) ) );
                                else
                                    RUNTIME_ASSERT( !"Unreachable place" );
                            }

                            data += arr_size * arr_element_size;
                        }
                    }

                    string key_str = get_key_string( key, key_element_type_id, key_element_type_name, prop->isHashSubType0 );
                    dict.insert( std::make_pair( std::move( key_str ), std::move( arr ) ) );
                }
            }
            else if( prop->isDictOfString )
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
                    data += str_size;

                    string key_str = get_key_string( key, key_element_type_id, key_element_type_name, prop->isHashSubType0 );
                    dict.insert( std::make_pair( std::move( key_str ), std::move( str ) ) );
                }
            }
            else
            {
                int value_type_id = prop->asObjType->GetSubTypeId( 1 );
                uint value_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType( value_type_id );
                string     value_element_type_name = prop->asObjType->GetSubType( 1 ) ? prop->asObjType->GetSubType( 1 )->GetName() : "";
                uint whole_element_size = key_element_size + value_element_size;
                uint dict_size = data_size / whole_element_size;
                for( uint i = 0; i < dict_size; i++ )
                {
                    uchar* pkey = data + i * whole_element_size;
                    uchar* pvalue = data + i * whole_element_size + key_element_size;
                    string key_str = get_key_string( pkey, key_element_type_id, key_element_type_name, prop->isHashSubType0 );

                    if( prop->isHashSubType1 )
                        dict.insert( std::make_pair( std::move( key_str ), _str().parseHash( *(hash*) pvalue ).str() ) );
                    else if( value_type_id == asTYPEID_INT8 )
                        dict.insert( std::make_pair( std::move( key_str ), (int) *(char*) pvalue ) );
                    else if( value_type_id == asTYPEID_INT16 )
                        dict.insert( std::make_pair( std::move( key_str ), (int) *(short*) pvalue ) );
                    else if( value_type_id == asTYPEID_INT32 )
                        dict.insert( std::make_pair( std::move( key_str ), (int) *(int*) pvalue ) );
                    else if( value_type_id == asTYPEID_INT64 )
                        dict.insert( std::make_pair( std::move( key_str ), ( int64 ) * (int64*) pvalue ) );
                    else if( value_type_id == asTYPEID_UINT8 )
                        dict.insert( std::make_pair( std::move( key_str ), (int) *(uchar*) pvalue ) );
                    else if( value_type_id == asTYPEID_UINT16 )
                        dict.insert( std::make_pair( std::move( key_str ), (int) *(ushort*) pvalue ) );
                    else if( value_type_id == asTYPEID_UINT32 )
                        dict.insert( std::make_pair( std::move( key_str ), (int) *(uint*) pvalue ) );
                    else if( value_type_id == asTYPEID_UINT64 )
                        dict.insert( std::make_pair( std::move( key_str ), ( int64 ) * (uint64*) pvalue ) );
                    else if( value_type_id == asTYPEID_FLOAT )
                        dict.insert( std::make_pair( std::move( key_str ), (double) *(float*) pvalue ) );
                    else if( value_type_id == asTYPEID_DOUBLE )
                        dict.insert( std::make_pair( std::move( key_str ), (double) *(double*) pvalue ) );
                    else if( value_type_id == asTYPEID_BOOL )
                        dict.insert( std::make_pair( std::move( key_str ), (bool) *(bool*) pvalue ) );
                    else if( !( value_type_id & asTYPEID_MASK_OBJECT ) && value_type_id > asTYPEID_DOUBLE )
                        dict.insert( std::make_pair( std::move( key_str ), Script::GetEnumValueName( value_element_type_name, *(int*) pvalue ) ) );
                    else
                        RUNTIME_ASSERT( !"Unreachable place" );
                }
            }
        }
        return dict;
    }
    else
    {
        RUNTIME_ASSERT( !"Unexpected type" );
    }
    return DataBase::Value();
}
#endif

bool Properties::LoadPropertyFromText( Property* prop, const string& text )
{
    RUNTIME_ASSERT( prop );
    RUNTIME_ASSERT( registrator == prop->registrator );
    RUNTIME_ASSERT( ( prop->podDataOffset != uint( -1 ) || prop->complexDataIndex != uint( -1 ) ) );
    bool is_error = false;

    // Parse
    uchar pod_buf[ 8 ];
    bool is_hashes[] = { prop->isHash || prop->isResource, prop->isHashSubType0, prop->isHashSubType1, prop->isHashSubType2 };
    void* value = ReadValue( text.c_str(), prop->asObjTypeId, prop->asObjType, is_hashes, 0, pod_buf, is_error );

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
    RUNTIME_ASSERT( ( prop->podDataOffset != uint( -1 ) || prop->complexDataIndex != uint( -1 ) ) );

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
        SCRIPT_ERROR_R0( "Enum '{}' not found", enum_value );
    if( !prop->IsPOD() )
        SCRIPT_ERROR_R0( "Can't retreive integer value from non POD property '{}'", prop->GetName() );
    if( !prop->IsReadable() )
        SCRIPT_ERROR_R0( "Can't retreive integer value from non readable property '{}'", prop->GetName() );

    return prop->GetPODValueAsInt( entity );
}

void Properties::SetValueAsInt( Entity* entity, int enum_value, int value )
{
    Property* prop = entity->Props.registrator->FindByEnum( enum_value );
    if( !prop )
        SCRIPT_ERROR_R( "Enum '{}' not found", enum_value );
    if( !prop->IsPOD() )
        SCRIPT_ERROR_R( "Can't set integer value to non POD property '{}'", prop->GetName() );
    if( !prop->IsWritable() )
        SCRIPT_ERROR_R( "Can't set integer value to non writable property '{}'", prop->GetName() );

    prop->SetPODValueAsInt( entity, value );
}

bool Properties::SetValueAsIntByName( Entity* entity, const string& enum_name, int value )
{
    Property* prop = entity->Props.registrator->Find( enum_name );
    if( !prop )
        SCRIPT_ERROR_R0( "Enum '{}' not found", enum_name );
    if( !prop->IsPOD() )
        SCRIPT_ERROR_R0( "Can't set by name integer value from non POD property '{}'", prop->GetName() );
    if( !prop->IsWritable() )
        SCRIPT_ERROR_R0( "Can't set integer value to non writable property '{}'", prop->GetName() );

    prop->SetPODValueAsInt( entity, value );
    return true;
}

bool Properties::SetValueAsIntProps( Properties* props, int enum_value, int value )
{
    Property* prop = props->registrator->FindByEnum( enum_value );
    if( !prop )
        SCRIPT_ERROR_R0( "Enum '{}' not found", enum_value );
    if( !prop->IsPOD() )
        SCRIPT_ERROR_R0( "Can't set integer value to non POD property '{}'", prop->GetName() );
    if( !prop->IsWritable() )
        SCRIPT_ERROR_R0( "Can't set integer value to non writable property '{}'", prop->GetName() );
    if( prop->accessType & Property::VirtualMask )
        SCRIPT_ERROR_R0( "Can't set integer value to virtual property '{}'", prop->GetName() );

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

PropertyRegistrator::PropertyRegistrator( bool is_server, const string& script_class_name )
{
    registrationFinished = false;
    isServer = is_server;
    scriptClassName = script_class_name;
    wholePodDataSize = 0;
    complexPropertiesCount = 0;
    defaultGroup = nullptr;
    defaultMinValue = nullptr;
    defaultMaxValue = nullptr;
    defaultTemporary = false;
    defaultNoHistory = false;
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
        WriteLog( "Register entity property enum '{}' fail, error {}.\n", enum_type, result );
        return false;
    }

    result = engine->RegisterEnumValue( enum_type.c_str(), "Invalid", 0 );
    if( result < 0 )
    {
        WriteLog( "Register entity property enum '{}::Invalid' zero value fail, error {}.\n", enum_type, result );
        return false;
    }

    string decl = _str( "int GetAsInt({}) const", enum_type );
    result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl.c_str(), SCRIPT_FUNC_THIS( Properties::GetValueAsInt ), SCRIPT_FUNC_THIS_CONV );
    if( result < 0 )
    {
        WriteLog( "Register entity method '{}' fail, error {}.\n", decl, result );
        return false;
    }

    decl = _str( "void SetAsInt({},int)", enum_type );
    result = engine->RegisterObjectMethod( scriptClassName.c_str(), decl.c_str(), SCRIPT_FUNC_THIS( Properties::SetValueAsInt ), SCRIPT_FUNC_THIS_CONV );
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
    const string& type_name,
    const string& name,
    Property::AccessType access,
    bool is_const,
    const char* group /* = nullptr */,
    int64* min_value /* = nullptr */,
    int64* max_value /* = nullptr */,
    bool is_temporary /* = false */,
    bool is_no_history /* = false */
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
    int type_id = engine->GetTypeIdByDecl( type_name.c_str() );
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

        int type_id = engine->GetTypeIdByDecl( type_name.c_str() );
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
        is_signed_int_data_type = ( type_id >= asTYPEID_INT8 && type_id <= asTYPEID_INT64 );
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
        if( _str( type_name ).str().find( "resource" ) != string::npos )
        {
            WriteLog( "Invalid property type '{}', array elements can't be resource type.\n", type_name );
            return nullptr;
        }

        is_hash_sub0 = ( _str( type_name ).str().find( "hash" ) != string::npos );
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
        if( _str( type_name ).str().find( "resource" ) != string::npos )
        {
            WriteLog( "Invalid property type '{}', dict elements can't be resource type.\n", type_name );
            return nullptr;
        }

        string t = _str( type_name ).str();
        is_hash_sub0 = ( t.find( "hash" ) != string::npos && t.find( ",", t.find( "hash" ) ) != string::npos );
        is_hash_sub1 = ( t.find( "hash" ) != string::npos && t.find( ",", t.find( "hash" ) ) == string::npos && !is_dict_of_array );
        is_hash_sub2 = ( t.find( "hash" ) != string::npos && t.find( ",", t.find( "hash" ) ) == string::npos && is_dict_of_array );
    }
    else
    {
        WriteLog( "Invalid property type '{}'.\n", type_name );
        return nullptr;
    }

    // Check for component
    size_t component_separator = name.find( '.' );
    string component_name;
    string property_name;
    if( component_separator != string::npos )
    {
        component_name = name.substr( 0, component_separator );
        property_name = name.substr( component_separator + 1 );
    }
    else
    {
        component_name = "";
        property_name = name;
    }

    // Check name for already used
    string class_name = scriptClassName + component_name;
    asITypeInfo* ot = engine->GetTypeInfoByName( class_name.c_str() );
    RUNTIME_ASSERT_STR( ot, class_name );
    for( asUINT i = 0, j = ot->GetPropertyCount(); i < j; i++ )
    {
        const char* n;
        ot->GetProperty( i, &n );
        if( property_name == n )
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
    #ifdef FONLINE_EDITOR
    disable_get = false;
    #endif

    // Register default getter
    bool is_handle = ( data_type == Property::Array || data_type == Property::Dict );
    if( !disable_get )
    {
        string decl = _str( "const {}{} get_{}() const", type_name, is_handle ? "@" : "", property_name );
        int  result = -1;
        #ifdef AS_MAX_PORTABILITY
        if( data_type == Property::String )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( Property_GetValue_Generic< string >), asCALL_GENERIC, prop );
        else if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( Property_GetValue_Generic< void* >), asCALL_GENERIC, prop );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( Property_GetValue_Generic< char >), asCALL_GENERIC, prop );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( Property_GetValue_Generic< short >), asCALL_GENERIC, prop );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( Property_GetValue_Generic< int >), asCALL_GENERIC, prop );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( Property_GetValue_Generic< int64 >), asCALL_GENERIC, prop );
        #else
        if( data_type == Property::String )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, GetValue< string >, (Entity*), string ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, GetValue< void* >, (Entity*), void* ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, GetValue< char >, (Entity*), char ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, GetValue< short >, (Entity*), short ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, GetValue< int >, (Entity*), int ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, GetValue< int64 >, (Entity*), int64 ), asCALL_THISCALL_OBJFIRST, prop );
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
        string decl = _str( "void set_{}({}{}{})", property_name, is_handle ? "const " : "", type_name, is_handle ? "@" : "" );
        int  result = -1;
        #ifdef AS_MAX_PORTABILITY
        if( data_type == Property::String )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( ( Property_SetValue_Generic< string >) ), asCALL_GENERIC, prop );
        else if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( ( Property_SetValue_Generic< void* >) ), asCALL_GENERIC, prop );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( ( Property_SetValue_Generic< char >) ), asCALL_GENERIC, prop );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( ( Property_SetValue_Generic< short >) ), asCALL_GENERIC, prop );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( ( Property_SetValue_Generic< int >) ), asCALL_GENERIC, prop );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asFUNCTION( ( Property_SetValue_Generic< int64 >) ), asCALL_GENERIC, prop );
        #else
        if( data_type == Property::String )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, SetValue< string >, ( Entity *, string ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_type != Property::POD )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, SetValue< void* >, ( Entity *, void* ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 1 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, SetValue< char >, ( Entity *, char ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 2 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, SetValue< short >, ( Entity *, short ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 4 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, SetValue< int >, ( Entity *, int ), void ), asCALL_THISCALL_OBJFIRST, prop );
        else if( data_size == 8 )
            result = engine->RegisterObjectMethod( class_name.c_str(), decl.c_str(), asMETHODPR( Property, SetValue< int64 >, ( Entity *, int64 ), void ), asCALL_THISCALL_OBJFIRST, prop );
        #endif
        if( result < 0 )
        {
            WriteLog( "Register entity property '{}' setter fail, error {}.\n", name, result );
            return nullptr;
        }
    }

    // Register enum values for property reflection
    string enum_value_name = _str( name ).replace( '.', '_' );
    int  enum_value = (int) _str( "{}::{}", enumTypeName, enum_value_name ).toHash();
    int  result = engine->RegisterEnumValue( enumTypeName.c_str(), enum_value_name.c_str(), enum_value );
    if( result < 0 )
    {
        WriteLog( "Register entity property enum '{}::{}' value {} fail, error {}.\n", enumTypeName, enum_value_name, enum_value, result );
        return nullptr;
    }

    // Add property to group
    if( group )
    {
        string full_decl = _str( "const array<{}>@ {}{}", enumTypeName, enumTypeName, group );

        CScriptArray* group_array = nullptr;
        if( enumGroups.count( full_decl ) )
            group_array = enumGroups[ full_decl ];

        if( !group_array )
        {
            string decl = _str( "array<{}>", enumTypeName );
            asITypeInfo* enum_array_type = engine->GetTypeInfoByDecl( decl.c_str() );
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

            enumGroups.insert( std::make_pair( string( full_decl ), group_array ) );

            int result = engine->RegisterGlobalProperty( full_decl.c_str(), &enumGroups[ full_decl ] );
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

    prop->propName = _str( name ).replace( '.', '_' );
    prop->typeName = type_name;
    prop->componentName = component_name;
    prop->dataType = data_type;
    prop->accessType = access;
    prop->isConst = is_const;
    prop->asObjTypeId = type_id;
    prop->asObjType = as_obj_type;
    prop->isHash = type_name == "hash";
    prop->isHashSubType0 = is_hash_sub0;
    prop->isHashSubType1 = is_hash_sub1;
    prop->isHashSubType2 = is_hash_sub2;
    prop->isResource = type_name == "resource";
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
    prop->isTemporary = ( defaultTemporary || is_temporary );
    prop->isNoHistory = ( defaultNoHistory || is_no_history );
    prop->checkMinValue = ( min_value != nullptr && ( is_int_data_type || is_float_data_type ) );
    prop->checkMaxValue = ( max_value != nullptr && ( is_int_data_type || is_float_data_type ) );
    prop->minValue = ( min_value ? *min_value : 0 );
    prop->maxValue = ( max_value ? *max_value : 0 );

    registeredProperties.push_back( prop );

    if( prop->asObjType )
        prop->asObjType->AddRef();

    return prop;
}

static void GetEntityComponent( asIScriptGeneric* gen )
{
    Entity* entity = (Entity*) gen->GetObject();
    hash component_name = *(hash*) gen->GetAuxiliary();
    if( entity->Proto->HaveComponent( component_name ) )
        *(Entity**) gen->GetAddressOfReturnLocation() = entity;
    else
        *(Entity**) gen->GetAddressOfReturnLocation() = nullptr;
}

bool PropertyRegistrator::RegisterComponent( const string& name )
{
    asIScriptEngine* engine = Script::GetEngine();
    RUNTIME_ASSERT( engine );

    string class_name = scriptClassName + name;
    int r = engine->RegisterObjectType( class_name.c_str(), 0, asOBJ_REF );
    if( r < 0 )
    {
        WriteLog( "Can't register '{}' component '{}'.\n", scriptClassName, class_name );
        return false;
    }

    r = engine->RegisterObjectBehaviour( class_name.c_str(), asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD( Entity, AddRef ), SCRIPT_METHOD_CONV );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectBehaviour( class_name.c_str(), asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD( Entity, Release ), SCRIPT_METHOD_CONV );
    RUNTIME_ASSERT( r >= 0 );

    hash* name_hash = new hash( _str( name ).toHash() );
    string decl = _str( "{}@+ get_{}()", class_name, name );
    r = engine->RegisterObjectMethod( scriptClassName.c_str(), decl.c_str(), asFUNCTION( GetEntityComponent ), asCALL_GENERIC, name_hash );
    if( r < 0 )
    {
        WriteLog( "Can't register '{}' component '{}' method '{}'.\n", scriptClassName, class_name, decl );
        return false;
    }

    string const_decl = "const " + decl + " const";
    r = engine->RegisterObjectMethod( scriptClassName.c_str(), const_decl.c_str(), asFUNCTION( GetEntityComponent ), asCALL_GENERIC, name_hash );
    if( r < 0 )
    {
        WriteLog( "Can't register '{}' component '{}' const method '{}'.\n", scriptClassName, class_name, const_decl );
        return false;
    }

    registeredComponents.insert( *name_hash );
    return true;
}

void PropertyRegistrator::SetDefaults(
    const char* group /* = nullptr */,
    int64* min_value /* = nullptr */,
    int64* max_value /* = nullptr */,
    bool is_temporary /* = false */,
    bool is_no_history     /* = false */
    )
{
    SAFEDELA( defaultGroup );
    SAFEDEL( defaultMinValue );
    SAFEDEL( defaultMaxValue );
    defaultTemporary = false;
    defaultNoHistory = false;

    if( group )
        defaultGroup = Str::Duplicate( group );
    if( min_value )
        defaultMinValue = new int64( *min_value );
    if( max_value )
        defaultMaxValue = new int64( *max_value );
    if( is_temporary )
        defaultTemporary = true;
    if( is_no_history )
        defaultNoHistory = true;
}

void PropertyRegistrator::FinishRegistration()
{
    if( registrationFinished )
        return;
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

Property* PropertyRegistrator::Find( const string& property_name )
{
    string key = property_name;
    size_t separator = key.find( '.' );
    if( separator != string::npos )
        key[ separator ] = '_';

    for( auto& prop : registeredProperties )
        if( prop->propName == key )
            return prop;
    return nullptr;
}

Property* PropertyRegistrator::FindByEnum( int enum_value )
{
    for( auto& prop : registeredProperties )
        if( prop->enumValue == enum_value )
            return prop;
    return nullptr;
}

bool PropertyRegistrator::IsComponentRegistered( hash component_name )
{
    return registeredComponents.count( component_name ) > 0;
}

void PropertyRegistrator::SetNativeSetCallback( const string& property_name, NativeCallback callback )
{
    RUNTIME_ASSERT( !property_name.empty() );
    Find( property_name )->nativeSetCallback = callback;
}

void PropertyRegistrator::SetNativeSendCallback( NativeSendCallback callback )
{
    for( auto& prop : registeredProperties )
        prop->nativeSendCallback = callback;
}

uint PropertyRegistrator::GetWholeDataSize()
{
    return wholePodDataSize;
}

string PropertyRegistrator::GetClassName()
{
    return scriptClassName;
}
