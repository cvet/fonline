#include <assert.h>
#include <string.h>
#include "scriptdictionary.h"
#include "scriptstring.h"

#define AS_USE_STLNAMES    0

using namespace std;

// --------------------------------------------------------------------------
// ScriptDictionary implementation

ScriptDictionary* ScriptDictionary::Create( asIScriptEngine* engine )
{
    // Use the custom memory routine from AngelScript to allow application to better control how much memory is used
    ScriptDictionary* obj = (ScriptDictionary*) asAllocMem( sizeof( ScriptDictionary ) );
    new (obj) ScriptDictionary( engine );
    return obj;
}

ScriptDictionary* ScriptDictionary::Create( asBYTE* buffer )
{
    // Use the custom memory routine from AngelScript to allow application to better control how much memory is used
    ScriptDictionary* obj = (ScriptDictionary*) asAllocMem( sizeof( ScriptDictionary ) );
    new (obj) ScriptDictionary( buffer );
    return obj;
}

ScriptDictionary::ScriptDictionary( asIScriptEngine* engine )
{
    // We start with one reference
    refCount = 1;
    gcFlag = false;

    // Keep a reference to the engine for as long as we live
    // We don't increment the reference counter, because the
    // engine will hold a pointer to the object.
    this->engine = engine;

    // Notify the garbage collector of this object
    // TODO: The object type should be cached
    engine->NotifyGarbageCollectorOfNewObject( this, engine->GetTypeInfoByName( "dictionary" ) );
}

ScriptDictionary::ScriptDictionary( asBYTE* buffer )
{
    // We start with one reference
    refCount = 1;
    gcFlag = false;

    // This constructor will always be called from a script
    // so we can get the engine from the active context
    asIScriptContext* ctx = asGetActiveContext();
    engine = ctx->GetEngine();

    // Notify the garbage collector of this object
    // TODO: The type id should be cached
    engine->NotifyGarbageCollectorOfNewObject( this, engine->GetTypeInfoByName( "dictionary" ) );

    // Initialize the dictionary from the buffer
    asUINT length = *(asUINT*) buffer;
    buffer += 4;

    while( length-- )
    {
        // Align the buffer pointer on a 4 byte boundary in
        // case previous value was smaller than 4 bytes
        if( asPWORD( buffer ) & 0x3 )
            buffer += 4 - ( asPWORD( buffer ) & 0x3 );

        // Get the name value pair from the buffer and insert it in the dictionary
        ScriptString* name = *(ScriptString**) buffer;
        buffer += sizeof( ScriptString * * );

        // Get the type id of the value
        int typeId = *(int*) buffer;
        buffer += sizeof( int );

        // Depending on the type id, the value will inline in the buffer or a pointer
        void* ref = (void*) buffer;

        if( typeId >= asTYPEID_INT8 && typeId <= asTYPEID_DOUBLE )
        {
            // Convert primitive values to either int64 or double, so we can use the overloaded Set methods
            asINT64 i64;
            double  d;
            switch( typeId )
            {
            case asTYPEID_INT8:
                i64 = *(char*) ref;
                break;
            case asTYPEID_INT16:
                i64 = *(short*) ref;
                break;
            case asTYPEID_INT32:
                i64 = *(int*) ref;
                break;
            case asTYPEID_INT64:
                i64 = *(asINT64*) ref;
                break;
            case asTYPEID_UINT8:
                i64 = *(unsigned char*) ref;
                break;
            case asTYPEID_UINT16:
                i64 = *(unsigned short*) ref;
                break;
            case asTYPEID_UINT32:
                i64 = *(unsigned int*) ref;
                break;
            case asTYPEID_UINT64:
                i64 = *(asINT64*) ref;
                break;
            case asTYPEID_FLOAT:
                d = *(float*) ref;
                break;
            case asTYPEID_DOUBLE:
                d = *(double*) ref;
                break;
            }

            if( typeId >= asTYPEID_FLOAT )
                Set( *name, d );
            else
                Set( *name, i64 );
        }
        else
        {
            if( ( typeId & asTYPEID_MASK_OBJECT ) &&
                !( typeId & asTYPEID_OBJHANDLE ) &&
                ( engine->GetTypeInfoById( typeId )->GetFlags() & asOBJ_REF ) )
            {
                // Dereference the pointer to get the reference to the actual object
                ref = *(void**) ref;
            }

            Set( *name, ref, typeId );
        }

        // Advance the buffer pointer with the size of the value
        if( typeId & asTYPEID_MASK_OBJECT )
        {
            asITypeInfo* ot = engine->GetTypeInfoById( typeId );
            if( ot->GetFlags() & asOBJ_VALUE )
                buffer += ot->GetSize();
            else
                buffer += sizeof( void* );
        }
        else if( typeId == 0 )
        {
            // null pointer
            buffer += sizeof( void* );
        }
        else
        {
            buffer += engine->GetSizeOfPrimitiveType( typeId );
        }
    }
}

ScriptDictionary::~ScriptDictionary()
{
    // Delete all keys and values
    DeleteAll();
}

void ScriptDictionary::AddRef() const
{
    // We need to clear the GC flag
    gcFlag = false;
    asAtomicInc( refCount );
}

void ScriptDictionary::Release() const
{
    // We need to clear the GC flag
    gcFlag = false;
    if( asAtomicDec( refCount ) == 0 )
    {
        this->~ScriptDictionary();
        asFreeMem( const_cast< ScriptDictionary* >( this ) );
    }
}

int ScriptDictionary::GetRefCount()
{
    return refCount;
}

void ScriptDictionary::SetGCFlag()
{
    gcFlag = true;
}

bool ScriptDictionary::GetGCFlag()
{
    return gcFlag;
}

void ScriptDictionary::EnumReferences( asIScriptEngine* engine )
{
    // TODO: If garbage collection can be done from a separate thread, then this method must be
    //       protected so that it doesn't get lost during the iteration if the dictionary is modified

    // Call the gc enum callback for each of the objects
    map< string, ScriptDictValue >::iterator it;
    for( it = dict.begin(); it != dict.end(); it++ )
    {
        if( it->second.m_typeId & asTYPEID_MASK_OBJECT )
            engine->GCEnumCallback( it->second.m_valueObj );
    }
}

void ScriptDictionary::ReleaseAllReferences( asIScriptEngine* /*engine*/ )
{
    // We're being told to release all references in
    // order to break circular references for dead objects
    DeleteAll();
}

ScriptDictionary& ScriptDictionary::operator=( const ScriptDictionary& other )
{
    // Clear everything we had before
    DeleteAll();

    // Do a shallow copy of the dictionary
    map< string, ScriptDictValue >::const_iterator it;
    for( it = other.dict.begin(); it != other.dict.end(); it++ )
    {
        if( it->second.m_typeId & asTYPEID_OBJHANDLE )
            Set( it->first, (void*) &it->second.m_valueObj, it->second.m_typeId );
        else if( it->second.m_typeId & asTYPEID_MASK_OBJECT )
            Set( it->first, (void*) it->second.m_valueObj, it->second.m_typeId );
        else
            Set( it->first, (void*) &it->second.m_valueInt, it->second.m_typeId );
    }

    return *this;
}

ScriptDictValue* ScriptDictionary::operator[]( const ScriptString& key )
{
    // Return the existing value if it exists, else insert an empty value
    map< string, ScriptDictValue >::iterator it;
    it = dict.find( key.c_std_str() );
    if( it == dict.end() )
        it = dict.insert( map< string, ScriptDictValue >::value_type( key.c_std_str(), ScriptDictValue() ) ).first;

    return &it->second;
}

const ScriptDictValue* ScriptDictionary::operator[]( const ScriptString& key ) const
{
    // Return the existing value if it exists
    map< string, ScriptDictValue >::const_iterator it;
    it = dict.find( key.c_std_str() );
    if( it != dict.end() )
        return &it->second;

    // Else raise an exception
    asIScriptContext* ctx = asGetActiveContext();
    if( ctx )
        ctx->SetException( "Invalid access to non-existing value" );

    return 0;
}

void ScriptDictValue::Set( asIScriptEngine* engine, ScriptDictValue& value )
{
    if( value.m_typeId & asTYPEID_OBJHANDLE )
        Set( engine, (void*) &value.m_valueObj, value.m_typeId );
    else if( value.m_typeId & asTYPEID_MASK_OBJECT )
        Set( engine, (void*) value.m_valueObj, value.m_typeId );
    else
        Set( engine, (void*) &value.m_valueInt, value.m_typeId );
}

void ScriptDictionary::Set( const ScriptString& key, void* value, int typeId )
{
    map< string, ScriptDictValue >::iterator it;
    it = dict.find( key.c_std_str() );
    if( it == dict.end() )
        it = dict.insert( map< string, ScriptDictValue >::value_type( key.c_std_str(), ScriptDictValue() ) ).first;

    it->second.Set( engine, value, typeId );
}

// This overloaded method is implemented so that all integer and
// unsigned integers types will be stored in the dictionary as int64
// through implicit conversions. This simplifies the management of the
// numeric types when the script retrieves the stored value using a
// different type.
void ScriptDictionary::Set( const ScriptString& key, const asINT64& value )
{
    Set( key, const_cast< asINT64* >( &value ), asTYPEID_INT64 );
}

// This overloaded method is implemented so that all floating point types
// will be stored in the dictionary as double through implicit conversions.
// This simplifies the management of the numeric types when the script
// retrieves the stored value using a different type.
void ScriptDictionary::Set( const ScriptString& key, const double& value )
{
    Set( key, const_cast< double* >( &value ), asTYPEID_DOUBLE );
}

void ScriptDictionary::Set( const string& key, void* value, int typeId )
{
    map< string, ScriptDictValue >::iterator it;
    it = dict.find( key );
    if( it == dict.end() )
        it = dict.insert( map< string, ScriptDictValue >::value_type( key, ScriptDictValue() ) ).first;

    it->second.Set( engine, value, typeId );
}

void ScriptDictionary::Set( const string& key, const asINT64& value )
{
    Set( key, const_cast< asINT64* >( &value ), asTYPEID_INT64 );
}

void ScriptDictionary::Set( const string& key, const double& value )
{
    Set( key, const_cast< double* >( &value ), asTYPEID_DOUBLE );
}

// Returns true if the value was successfully retrieved
bool ScriptDictionary::Get( const ScriptString& key, void* value, int typeId ) const
{
    map< string, ScriptDictValue >::const_iterator it;
    it = dict.find( key.c_std_str() );
    if( it != dict.end() )
        return it->second.Get( engine, value, typeId );

    // AngelScript has already initialized the value with a default value,
    // so we don't have to do anything if we don't find the element, or if
    // the element is incompatible with the requested type.

    return false;
}

bool ScriptDictionary::Get( const ScriptString& key, asINT64& value ) const
{
    return Get( key, &value, asTYPEID_INT64 );
}

bool ScriptDictionary::Get( const ScriptString& key, double& value ) const
{
    return Get( key, &value, asTYPEID_DOUBLE );
}

// Returns the type id of the stored value
int ScriptDictionary::GetTypeId( const string& key ) const
{
    map< string, ScriptDictValue >::const_iterator it;
    it = dict.find( key );
    if( it != dict.end() )
        return it->second.m_typeId;

    return -1;
}

bool ScriptDictionary::Exists( const ScriptString& key ) const
{
    map< string, ScriptDictValue >::const_iterator it;
    it = dict.find( key.c_std_str() );
    if( it != dict.end() )
        return true;

    return false;
}

bool ScriptDictionary::IsEmpty() const
{
    if( dict.size() == 0 )
        return true;

    return false;
}

asUINT ScriptDictionary::GetSize() const
{
    return asUINT( dict.size() );
}

bool ScriptDictionary::Delete( const ScriptString& key )
{
    map< string, ScriptDictValue >::iterator it;
    it = dict.find( key.c_std_str() );
    if( it != dict.end() )
    {
        it->second.FreeValue( engine );
        dict.erase( it );
        return true;
    }
    return false;
}

void ScriptDictionary::DeleteAll()
{
    map< string, ScriptDictValue >::iterator it;
    for( it = dict.begin(); it != dict.end(); it++ )
        it->second.FreeValue( engine );

    dict.clear();
}

ScriptArray* ScriptDictionary::GetKeys() const
{
    // TODO: optimize: The string array type should only be determined once.
    //                 It should be recomputed when registering the dictionary class.
    //                 Only problem is if multiple engines are used, as they may not
    //                 share the same type id. Alternatively it can be stored in the
    //                 user data for the dictionary type.
    asITypeInfo* ot = engine->GetTypeInfoByDecl( "array<string>" );

    // Create the array object
    ScriptArray*                                        array = ScriptArray::Create( ot, asUINT( dict.size() ) );
    long                                                current = -1;
    std::map< string, ScriptDictValue >::const_iterator it;
    for( it = dict.begin(); it != dict.end(); it++ )
    {
        current++;
        *(ScriptString*) array->At( current ) = it->first;
    }

    return array;
}

void ScriptDictionaryFactory_Generic( asIScriptGeneric* gen )
{
    *(ScriptDictionary**) gen->GetAddressOfReturnLocation() = ScriptDictionary::Create( gen->GetEngine() );
}

void ScriptDictionaryListFactory_Generic( asIScriptGeneric* gen )
{
    asBYTE* buffer = (asBYTE*) gen->GetArgAddress( 0 );
    *(ScriptDictionary**) gen->GetAddressOfReturnLocation() = ScriptDictionary::Create( buffer );
}

// -------------------------------------------------------------------------
// ScriptDictValue

ScriptDictValue::ScriptDictValue()
{
    m_valueObj = 0;
    m_typeId   = 0;
}

ScriptDictValue::ScriptDictValue( asIScriptEngine* engine, void* value, int typeId )
{
    m_valueObj = 0;
    m_typeId   = 0;
    Set( engine, value, typeId );
}

ScriptDictValue::~ScriptDictValue()
{
    // Must not hold an object when destroyed, as then the object will never be freed
    assert( ( m_typeId & asTYPEID_MASK_OBJECT ) == 0 );
}

void ScriptDictValue::FreeValue( asIScriptEngine* engine )
{
    // If it is a handle or a ref counted object, call release
    if( m_typeId & asTYPEID_MASK_OBJECT )
    {
        // Let the engine release the object
        engine->ReleaseScriptObject( m_valueObj, engine->GetTypeInfoById( m_typeId ) );
        m_valueObj = 0;
        m_typeId = 0;
    }

    // For primitives, there's nothing to do
}

void ScriptDictValue::Set( asIScriptEngine* engine, void* value, int typeId )
{
    FreeValue( engine );

    m_typeId = typeId;
    if( typeId & asTYPEID_OBJHANDLE )
    {
        // We're receiving a reference to the handle, so we need to dereference it
        m_valueObj = *(void**) value;
        engine->AddRefScriptObject( m_valueObj, engine->GetTypeInfoById( typeId ) );
    }
    else if( typeId & asTYPEID_MASK_OBJECT )
    {
        // Create a copy of the object
        m_valueObj = engine->CreateScriptObjectCopy( value, engine->GetTypeInfoById( typeId ) );
    }
    else
    {
        // Copy the primitive value
        // We receive a pointer to the value.
        int size = engine->GetSizeOfPrimitiveType( typeId );
        memcpy( &m_valueInt, value, size );
    }
}

// This overloaded method is implemented so that all integer and
// unsigned integers types will be stored in the dictionary as int64
// through implicit conversions. This simplifies the management of the
// numeric types when the script retrieves the stored value using a
// different type.
void ScriptDictValue::Set( asIScriptEngine* engine, const asINT64& value )
{
    Set( engine, const_cast< asINT64* >( &value ), asTYPEID_INT64 );
}

// This overloaded method is implemented so that all floating point types
// will be stored in the dictionary as double through implicit conversions.
// This simplifies the management of the numeric types when the script
// retrieves the stored value using a different type.
void ScriptDictValue::Set( asIScriptEngine* engine, const double& value )
{
    Set( engine, const_cast< double* >( &value ), asTYPEID_DOUBLE );
}

bool ScriptDictValue::Get( asIScriptEngine* engine, void* value, int typeId ) const
{
    // Return the value
    if( typeId & asTYPEID_OBJHANDLE )
    {
        // A handle can be retrieved if the stored type is a handle of same or compatible type
        // or if the stored type is an object that implements the interface that the handle refer to.
        if( ( m_typeId & asTYPEID_MASK_OBJECT ) )
        {
            // Don't allow the get if the stored handle is to a const, but the desired handle is not
            if( ( m_typeId & asTYPEID_HANDLETOCONST ) && !( typeId & asTYPEID_HANDLETOCONST ) )
                return false;

            // RefCastObject will increment the refcount if successful
            engine->RefCastObject( m_valueObj, engine->GetTypeInfoById( m_typeId ), engine->GetTypeInfoById( typeId ), reinterpret_cast< void** >( value ) );

            return true;
        }
    }
    else if( typeId & asTYPEID_MASK_OBJECT )
    {
        // Verify that the copy can be made
        bool isCompatible = false;

        // Allow a handle to be value assigned if the wanted type is not a handle
        if( ( m_typeId & ~asTYPEID_OBJHANDLE ) == typeId && m_valueObj != 0 )
            isCompatible = true;

        // Copy the object into the given reference
        if( isCompatible )
        {
            engine->AssignScriptObject( value, m_valueObj, engine->GetTypeInfoById( typeId ) );

            return true;
        }
    }
    else
    {
        if( m_typeId == typeId )
        {
            int size = engine->GetSizeOfPrimitiveType( typeId );
            memcpy( value, &m_valueInt, size );
            return true;
        }

        // We know all numbers are stored as either int64 or double, since we register overloaded functions for those
        // Only bool and enums needs to be treated separately
        if( typeId == asTYPEID_DOUBLE )
        {
            if( m_typeId == asTYPEID_INT64 )
                *(double*) value = double(m_valueInt);
            else if( m_typeId == asTYPEID_BOOL )
            {
                // Use memcpy instead of type cast to make sure the code is endianess agnostic
                char localValue;
                memcpy( &localValue, &m_valueInt, sizeof( char ) );
                *(double*) value = localValue ? 1.0 : 0.0;
            }
            else if( m_typeId > asTYPEID_DOUBLE && ( m_typeId & asTYPEID_MASK_OBJECT ) == 0 )
            {
                // Use memcpy instead of type cast to make sure the code is endianess agnostic
                int localValue;
                memcpy( &localValue, &m_valueInt, sizeof( int ) );
                *(double*) value = double(localValue); // enums are 32bit
            }
            else
            {
                // The stored type is an object
                // TODO: Check if the object has a conversion operator to a primitive value
                *(double*) value = 0;
            }
            return true;
        }
        else if( typeId == asTYPEID_INT64 )
        {
            if( m_typeId == asTYPEID_DOUBLE )
                *(asINT64*) value = asINT64( m_valueFlt );
            else if( m_typeId == asTYPEID_BOOL )
            {
                // Use memcpy instead of type cast to make sure the code is endianess agnostic
                char localValue;
                memcpy( &localValue, &m_valueInt, sizeof( char ) );
                *(asINT64*) value = localValue ? 1 : 0;
            }
            else if( m_typeId > asTYPEID_DOUBLE && ( m_typeId & asTYPEID_MASK_OBJECT ) == 0 )
            {
                // Use memcpy instead of type cast to make sure the code is endianess agnostic
                int localValue;
                memcpy( &localValue, &m_valueInt, sizeof( int ) );
                *(asINT64*) value = localValue; // enums are 32bit
            }
            else
            {
                // The stored type is an object
                // TODO: Check if the object has a conversion operator to a primitive value
                *(asINT64*) value = 0;
            }
            return true;
        }
        else if( typeId > asTYPEID_DOUBLE && ( m_typeId & asTYPEID_MASK_OBJECT ) == 0 )
        {
            // The desired type is an enum. These are always 32bit integers
            if( m_typeId == asTYPEID_DOUBLE )
                *(int*) value = int(m_valueFlt);
            else if( m_typeId == asTYPEID_INT64 )
                *(int*) value = int(m_valueInt);
            else if( m_typeId == asTYPEID_BOOL )
            {
                // Use memcpy instead of type cast to make sure the code is endianess agnostic
                char localValue;
                memcpy( &localValue, &m_valueInt, sizeof( char ) );
                *(int*) value = localValue ? 1 : 0;
            }
            else if( m_typeId > asTYPEID_DOUBLE && ( m_typeId & asTYPEID_MASK_OBJECT ) == 0 )
            {
                // Use memcpy instead of type cast to make sure the code is endianess agnostic
                int localValue;
                memcpy( &localValue, &m_valueInt, sizeof( int ) );
                *(int*) value = localValue; // enums are 32bit
            }
            else
            {
                // The stored type is an object
                // TODO: Check if the object has a conversion operator to a primitive value
                *(int*) value = 0;
            }
        }
        else if( typeId == asTYPEID_BOOL )
        {
            if( m_typeId & asTYPEID_OBJHANDLE )
            {
                // TODO: Check if the object has a conversion operator to a primitive value
                *(bool*) value = m_valueObj ? true : false;
            }
            else if( m_typeId & asTYPEID_MASK_OBJECT )
            {
                // TODO: Check if the object has a conversion operator to a primitive value
                *(bool*) value = true;
            }
            else
            {
                // Compare only the bytes that were actually set
                asQWORD zero = 0;
                int     size = engine->GetSizeOfPrimitiveType( m_typeId );
                *(bool*) value = memcmp( &m_valueInt, &zero, size ) == 0 ? false : true;
            }
        }
    }

    // It was not possible to retrieve the value using the desired typeId
    return false;
}

bool ScriptDictValue::Get( asIScriptEngine* engine, asINT64& value ) const
{
    return Get( engine, &value, asTYPEID_INT64 );
}

bool ScriptDictValue::Get( asIScriptEngine* engine, double& value ) const
{
    return Get( engine, &value, asTYPEID_DOUBLE );
}

int ScriptDictValue::GetTypeId() const
{
    return m_typeId;
}

static void ScriptDictValue_Construct( void* mem )
{
    new (mem) ScriptDictValue();
}

static void ScriptDictValue_Destruct( ScriptDictValue* obj )
{
    asIScriptContext* ctx = asGetActiveContext();
    if( ctx )
    {
        asIScriptEngine* engine = ctx->GetEngine();
        obj->FreeValue( engine );
    }
    obj->~ScriptDictValue();
}

static ScriptDictValue& ScriptDictValue_opAssign( void* ref, int typeId, ScriptDictValue* obj )
{
    asIScriptContext* ctx = asGetActiveContext();
    if( ctx )
    {
        asIScriptEngine* engine = ctx->GetEngine();
        obj->Set( engine, ref, typeId );
    }
    return *obj;
}

static ScriptDictValue& ScriptDictValue_opAssign( const ScriptDictValue& other, ScriptDictValue* obj )
{
    asIScriptContext* ctx = asGetActiveContext();
    if( ctx )
    {
        asIScriptEngine* engine = ctx->GetEngine();
        obj->Set( engine, const_cast< ScriptDictValue& >( other ) );
    }

    return *obj;
}

static ScriptDictValue& ScriptDictValue_opAssign( double val, ScriptDictValue* obj )
{
    return ScriptDictValue_opAssign( &val, asTYPEID_DOUBLE, obj );
}

static ScriptDictValue& ScriptDictValue_opAssign( asINT64 val, ScriptDictValue* obj )
{
    return ScriptDictValue_opAssign( &val, asTYPEID_INT64, obj );
}

static void ScriptDictValue_opCast( void* ref, int typeId, ScriptDictValue* obj )
{
    asIScriptContext* ctx = asGetActiveContext();
    if( ctx )
    {
        asIScriptEngine* engine = ctx->GetEngine();
        obj->Get( engine, ref, typeId );
    }
}

static asINT64 ScriptDictValue_opConvInt( ScriptDictValue* obj )
{
    asINT64 value;
    ScriptDictValue_opCast( &value, asTYPEID_INT64, obj );
    return value;
}

static double ScriptDictValue_opConvDouble( ScriptDictValue* obj )
{
    double value;
    ScriptDictValue_opCast( &value, asTYPEID_DOUBLE, obj );
    return value;
}

// --------------------------------------------------------------------------
// Register the type

void RegisterScriptDictionary_Native( asIScriptEngine* engine );
void RegisterScriptDictionary_Generic( asIScriptEngine* engine );
void RegisterScriptDictionary( asIScriptEngine* engine )
{
    if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) )
        RegisterScriptDictionary_Generic( engine );
    else
        RegisterScriptDictionary_Native( engine );
}

void RegisterScriptDictionary_Native( asIScriptEngine* engine )
{
    int r;

    #if AS_CAN_USE_CPP11
    // With C++11 it is possible to use asGetTypeTraits to automatically determine the correct flags that represents the C++ class
    r = engine->RegisterObjectType( "dictionaryValue", sizeof( ScriptDictValue ), asOBJ_VALUE | asOBJ_ASHANDLE | asGetTypeTraits< ScriptDictValue >() );
    assert( r >= 0 );
    #else
    r = engine->RegisterObjectType( "dictionaryValue", sizeof( ScriptDictValue ), asOBJ_VALUE | asOBJ_ASHANDLE | asOBJ_APP_CLASS_CD );
    assert( r >= 0 );
    #endif
    r = engine->RegisterObjectBehaviour( "dictionaryValue", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( ScriptDictValue_Construct ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionaryValue", asBEHAVE_DESTRUCT, "void f()", asFUNCTION( ScriptDictValue_Destruct ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "dictionaryValue &opAssign(const dictionaryValue &in)", asFUNCTIONPR( ScriptDictValue_opAssign, ( const ScriptDictValue &, ScriptDictValue* ), ScriptDictValue & ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "dictionaryValue &opHndlAssign(const ?&in)", asFUNCTIONPR( ScriptDictValue_opAssign, ( void*, int, ScriptDictValue* ), ScriptDictValue & ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "dictionaryValue &opAssign(const ?&in)", asFUNCTIONPR( ScriptDictValue_opAssign, ( void*, int, ScriptDictValue* ), ScriptDictValue & ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "dictionaryValue &opAssign(double)", asFUNCTIONPR( ScriptDictValue_opAssign, ( double, ScriptDictValue* ), ScriptDictValue & ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "dictionaryValue &opAssign(int64)", asFUNCTIONPR( ScriptDictValue_opAssign, ( asINT64, ScriptDictValue* ), ScriptDictValue & ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "void opCast(?&out)", asFUNCTIONPR( ScriptDictValue_opCast, ( void*, int, ScriptDictValue* ), void ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "void opConv(?&out)", asFUNCTIONPR( ScriptDictValue_opCast, ( void*, int, ScriptDictValue* ), void ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "int64 opConv()", asFUNCTIONPR( ScriptDictValue_opConvInt, (ScriptDictValue*), asINT64 ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionaryValue", "double opConv()", asFUNCTIONPR( ScriptDictValue_opConvDouble, (ScriptDictValue*), double ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );

    r = engine->RegisterObjectType( "dictionary", sizeof( ScriptDictionary ), asOBJ_REF | asOBJ_GC );
    assert( r >= 0 );
    // Use the generic interface to construct the object since we need the engine pointer, we could also have retrieved the engine pointer from the active context
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_FACTORY, "dictionary@ f()", asFUNCTION( ScriptDictionaryFactory_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_LIST_FACTORY, "dictionary @f(int &in) {repeat {string, ?}}", asFUNCTION( ScriptDictionaryListFactory_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_ADDREF, "void f()", asMETHOD( ScriptDictionary, AddRef ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_RELEASE, "void f()", asMETHOD( ScriptDictionary, Release ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "dictionary &opAssign(const dictionary &in)", asMETHODPR( ScriptDictionary, operator=, ( const ScriptDictionary & ), ScriptDictionary & ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "void set(const string &in, const ?&in)", asMETHODPR( ScriptDictionary, Set, ( const ScriptString &, void*, int ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool get(const string &in, ?&out) const", asMETHODPR( ScriptDictionary, Get, ( const ScriptString &, void*, int ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "void set(const string &in, const int64&in)", asMETHODPR( ScriptDictionary, Set, ( const ScriptString &, const asINT64 & ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool get(const string &in, int64&out) const", asMETHODPR( ScriptDictionary, Get, ( const ScriptString &, asINT64 & ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "void set(const string &in, const double&in)", asMETHODPR( ScriptDictionary, Set, ( const ScriptString &, const double& ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool get(const string &in, double&out) const", asMETHODPR( ScriptDictionary, Get, ( const ScriptString &, double& ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "bool exists(const string &in) const", asMETHOD( ScriptDictionary, Exists ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool isEmpty() const", asMETHOD( ScriptDictionary, IsEmpty ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "uint getSize() const", asMETHOD( ScriptDictionary, GetSize ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool delete(const string &in)", asMETHOD( ScriptDictionary, Delete ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "void deleteAll()", asMETHOD( ScriptDictionary, DeleteAll ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "array<string> @getKeys() const", asMETHOD( ScriptDictionary, GetKeys ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "dictionaryValue &opIndex(const string &in)", asMETHODPR( ScriptDictionary, operator[], ( const ScriptString & ), ScriptDictValue* ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "const dictionaryValue &opIndex(const string &in) const", asMETHODPR( ScriptDictionary, operator[], ( const ScriptString & ) const, const ScriptDictValue* ), asCALL_THISCALL );
    assert( r >= 0 );

    // Register GC behaviours
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_GETREFCOUNT, "int f()", asMETHOD( ScriptDictionary, GetRefCount ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_SETGCFLAG, "void f()", asMETHOD( ScriptDictionary, SetGCFlag ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_GETGCFLAG, "bool f()", asMETHOD( ScriptDictionary, GetGCFlag ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_ENUMREFS, "void f(int&in)", asMETHOD( ScriptDictionary, EnumReferences ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_RELEASEREFS, "void f(int&in)", asMETHOD( ScriptDictionary, ReleaseAllReferences ), asCALL_THISCALL );
    assert( r >= 0 );
}

void RegisterScriptDictionary_Generic( asIScriptEngine* engine )
{
    // Generic stuff deleted
    assert( false );
}

// ------------------------------------------------------------------
// Iterator implementation

ScriptDictionary::Iterator ScriptDictionary::begin() const
{
    return Iterator( *this, dict.begin() );
}

ScriptDictionary::Iterator ScriptDictionary::end() const
{
    return Iterator( *this, dict.end() );
}

ScriptDictionary::Iterator::Iterator( const ScriptDictionary& dict, std::map< std::string, ScriptDictValue >::const_iterator it ): m_it( it ), m_dict( dict )
{}

void ScriptDictionary::Iterator::operator++()
{
    ++m_it;
}

void ScriptDictionary::Iterator::operator++( int )
{
    ++m_it;

    // Normally the post increment would return a copy of the object with the original state,
    // but it is rarely used so we skip this extra copy to avoid unnecessary overhead
}

ScriptDictionary::Iterator& ScriptDictionary::Iterator::operator*()
{
    return *this;
}

bool ScriptDictionary::Iterator::operator==( const Iterator& other ) const
{
    return m_it == other.m_it;
}

bool ScriptDictionary::Iterator::operator!=( const Iterator& other ) const
{
    return m_it != other.m_it;
}

const std::string& ScriptDictionary::Iterator::GetKey() const
{
    return m_it->first;
}

int ScriptDictionary::Iterator::GetTypeId() const
{
    return m_it->second.m_typeId;
}

bool ScriptDictionary::Iterator::GetValue( asINT64& value ) const
{
    return m_it->second.Get( m_dict.engine, &value, asTYPEID_INT64 );
}

bool ScriptDictionary::Iterator::GetValue( double& value ) const
{
    return m_it->second.Get( m_dict.engine, &value, asTYPEID_DOUBLE );
}

bool ScriptDictionary::Iterator::GetValue( void* value, int typeId ) const
{
    return m_it->second.Get( m_dict.engine, value, typeId );
}
