#include <new>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h> // sprintf
#include "Common.h"
#include "Debugger.h"

#include "scriptdict.h"

static void* AllocMem( size_t size )
{
    #ifdef MEMORY_DEBUG
    if( MemoryDebugLevel >= 1 )
        MEMORY_PROCESS( MEMORY_SCRIPT_DICT, (int) size );
    #endif
    return new char[ size ];
}

static void FreeMem( void* mem, size_t size )
{
    #ifdef MEMORY_DEBUG
    if( MemoryDebugLevel >= 1 )
        MEMORY_PROCESS( MEMORY_SCRIPT_DICT, -(int) size );
    #endif
    delete[] (char*) mem;
}

static void RegisterScriptDict_Native( asIScriptEngine* engine );

// This macro is used to avoid warnings about unused variables.
// Usually where the variables are only used in debug mode.
#define UNUSED_VAR( x )    (void) ( x )

static void* CopyObject( asIObjectType* objType, int subTypeIndex, void* value );
static void  DestroyObject( asIObjectType* objType, int subTypeIndex, void* value );
static bool  Less( int typeId, const void* a, const void* b );
static bool  Equals( int typeId, const void* a, const void* b );

struct DictMapComparator
{
    DictMapComparator( int id )
    {
        typeId = id;
    }

    bool operator()( const void* a, const void* b ) const
    {
        return Less( typeId, a, b );
    }

    int typeId;
};

typedef map< void*, void*, DictMapComparator > DictMap;

ScriptDict* ScriptDict::Create( asIObjectType* ot )
{
    // Allocate the memory
    void* mem = AllocMem( sizeof( ScriptDict ) );
    if( mem == 0 )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Out of memory" );

        return 0;
    }

    // Initialize the object
    ScriptDict* d = new (mem) ScriptDict( ot );

    return d;
}

ScriptDict* ScriptDict::Create( asIObjectType* ot, void* initList )
{
    // Allocate the memory
    void* mem = AllocMem( sizeof( ScriptDict ) );
    if( mem == 0 )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Out of memory" );

        return 0;
    }

    // Initialize the object
    ScriptDict* d = new (mem) ScriptDict( ot, initList );

    return d;
}

// This optional callback is called when the template type is first used by the compiler.
// It allows the application to validate if the template can be instanciated for the requested
// subtype at compile time, instead of at runtime. The output argument dontGarbageCollect
// allow the callback to tell the engine if the template instance type shouldn't be garbage collected,
// i.e. no asOBJ_GC flag.
static bool ScriptDictTemplateCallbackExt( asIObjectType* ot, int subTypeIndex, bool& dontGarbageCollect )
{
    // Make sure the subtype can be instanciated with a default factory/constructor,
    // otherwise we won't be able to instanciate the elements.
    int typeId = ot->GetSubTypeId( subTypeIndex );
    if( typeId == asTYPEID_VOID )
        return false;
    if( ( typeId & asTYPEID_MASK_OBJECT ) && !( typeId & asTYPEID_OBJHANDLE ) )
    {
        asIObjectType* subtype = ot->GetEngine()->GetObjectTypeById( typeId );
        asDWORD        flags = subtype->GetFlags();
        if( ( flags & asOBJ_VALUE ) && !( flags & asOBJ_POD ) )
        {
            // Verify that there is a default constructor
            bool found = false;
            for( asUINT n = 0; n < subtype->GetBehaviourCount(); n++ )
            {
                asEBehaviours      beh;
                asIScriptFunction* func = subtype->GetBehaviourByIndex( n, &beh );
                if( beh != asBEHAVE_CONSTRUCT )
                    continue;

                if( func->GetParamCount() == 0 )
                {
                    // Found the default constructor
                    found = true;
                    break;
                }
            }

            if( !found )
            {
                // There is no default constructor
                ot->GetEngine()->WriteMessage( "dict", 0, 0, asMSGTYPE_ERROR, "The subtype has no default constructor" );
                return false;
            }
        }
        else if( ( flags & asOBJ_REF ) )
        {
            bool found = false;

            // If value assignment for ref type has been disabled then the dict
            // can be created if the type has a default factory function
            if( !ot->GetEngine()->GetEngineProperty( asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE ) )
            {
                // Verify that there is a default factory
                for( asUINT n = 0; n < subtype->GetFactoryCount(); n++ )
                {
                    asIScriptFunction* func = subtype->GetFactoryByIndex( n );
                    if( func->GetParamCount() == 0 )
                    {
                        // Found the default factory
                        found = true;
                        break;
                    }
                }
            }

            if( !found )
            {
                // No default factory
                ot->GetEngine()->WriteMessage( "dict", 0, 0, asMSGTYPE_ERROR, "The subtype has no default factory" );
                return false;
            }
        }

        // If the object type is not garbage collected then the dict also doesn't need to be
        if( !( flags & asOBJ_GC ) )
            dontGarbageCollect = true;
    }
    else if( !( typeId & asTYPEID_OBJHANDLE ) )
    {
        // Dicts with primitives cannot form circular references,
        // thus there is no need to garbage collect them
        dontGarbageCollect = true;
    }
    else
    {
        assert( typeId & asTYPEID_OBJHANDLE );

        // It is not necessary to set the dict as garbage collected for all handle types.
        // If it is possible to determine that the handle cannot refer to an object type
        // that can potentially form a circular reference with the dict then it is not
        // necessary to make the dict garbage collected.
        asIObjectType* subtype = ot->GetEngine()->GetObjectTypeById( typeId );
        asDWORD        flags = subtype->GetFlags();
        if( !( flags & asOBJ_GC ) )
        {
            if( ( flags & asOBJ_SCRIPT_OBJECT ) )
            {
                // Even if a script class is by itself not garbage collected, it is possible
                // that classes that derive from it may be, so it is not possible to know
                // that no circular reference can occur.
                if( ( flags & asOBJ_NOINHERIT ) )
                {
                    // A script class declared as final cannot be inherited from, thus
                    // we can be certain that the object cannot be garbage collected.
                    dontGarbageCollect = true;
                }
            }
            else
            {
                // For application registered classes we assume the application knows
                // what it is doing and don't mark the dict as garbage collected unless
                // the type is also garbage collected.
                dontGarbageCollect = true;
            }
        }
    }

    // The type is ok
    return true;
}

static bool ScriptDictTemplateCallback( asIObjectType* ot, bool& dontGarbageCollect )
{
    return ScriptDictTemplateCallbackExt( ot, 0, dontGarbageCollect ) &&
           ScriptDictTemplateCallbackExt( ot, 1, dontGarbageCollect );
}

// Registers the template dict type
void RegisterScriptDict( asIScriptEngine* engine )
{
    if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) == 0 )
        RegisterScriptDict_Native( engine );
}

static void RegisterScriptDict_Native( asIScriptEngine* engine )
{
    int r = 0;
    UNUSED_VAR( r );

    // Register the dict type as a template
    r = engine->RegisterObjectType( "dict<class T1, class T2>", 0, asOBJ_REF | asOBJ_GC | asOBJ_TEMPLATE );
    assert( r >= 0 );

    // Register a callback for validating the subtype before it is used
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", asFUNCTION( ScriptDictTemplateCallback ), asCALL_CDECL );
    assert( r >= 0 );

    // Templates receive the object type as the first parameter. To the script writer this is hidden
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_FACTORY, "dict<T1,T2>@ f(int&in)", asFUNCTIONPR( ScriptDict::Create, (asIObjectType*), ScriptDict* ), asCALL_CDECL );
    assert( r >= 0 );

    // Register the factory that will be used for initialization lists
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_LIST_FACTORY, "dict<T1,T2>@ f(int&in type, int&in list) {repeat {T1, T2}}", asFUNCTIONPR( ScriptDict::Create, ( asIObjectType *, void* ), ScriptDict* ), asCALL_CDECL );
    assert( r >= 0 );

    // The memory management methods
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_ADDREF, "void f()", asMETHOD( ScriptDict, AddRef ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_RELEASE, "void f()", asMETHOD( ScriptDict, Release ), asCALL_THISCALL );
    assert( r >= 0 );

    // The index operator returns the template subtype
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "const T2& get_opIndex(const T1&in) const", asMETHOD( ScriptDict, Get ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "void set_opIndex(const T1&in, const T2&in)", asMETHOD( ScriptDict, Set ), asCALL_THISCALL );
    assert( r >= 0 );

    // The assignment operator
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "dict<T1,T2>& opAssign(const dict<T1,T2>&in)", asMETHOD( ScriptDict, operator= ), asCALL_THISCALL );
    assert( r >= 0 );

    // Other methods
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "void set(const T1&in, const T2&in)", asMETHOD( ScriptDict, Set ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "void setIfNotExist(const T1&in, const T2&in)", asMETHOD( ScriptDict, SetIfNotExist ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "bool remove(const T1&in)", asMETHOD( ScriptDict, Remove ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "uint removeValues(const T2&in)", asMETHOD( ScriptDict, RemoveValues ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "uint length() const", asMETHOD( ScriptDict, GetSize ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "void clear()", asMETHOD( ScriptDict, Clear ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "const T2& get(const T1&in) const", asMETHOD( ScriptDict, Get ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "const T2& get(const T1&in, const T2&in) const", asMETHOD( ScriptDict, GetDefault ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "const T1& getKey(uint index) const", asMETHOD( ScriptDict, GetKey ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "const T2& getValue(uint index) const", asMETHOD( ScriptDict, GetValue ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "bool contains(const T1&in) const", asMETHOD( ScriptDict, Contains ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "bool opEquals(const dict<T1,T2>&in) const", asMETHOD( ScriptDict, operator== ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "bool isEmpty() const", asMETHOD( ScriptDict, IsEmpty ), asCALL_THISCALL );
    assert( r >= 0 );

    // Register GC behaviors in case the dict needs to be garbage collected
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_GETREFCOUNT, "int f()", asMETHOD( ScriptDict, GetRefCount ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_SETGCFLAG, "void f()", asMETHOD( ScriptDict, SetFlag ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_GETGCFLAG, "bool f()", asMETHOD( ScriptDict, GetFlag ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_ENUMREFS, "void f(int&in)", asMETHOD( ScriptDict, EnumReferences ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_RELEASEREFS, "void f(int&in)", asMETHOD( ScriptDict, ReleaseAllHandles ), asCALL_THISCALL );
    assert( r >= 0 );
}

ScriptDict::ScriptDict( asIObjectType* ot )
{
    refCount = 1;
    gcFlag = false;
    objType = ot;
    objType->AddRef();
    keyTypeId = objType->GetSubTypeId( 0 );
    valueTypeId = objType->GetSubTypeId( 1 );
    dictMap = new DictMap( DictMapComparator( keyTypeId ) );

    // Notify the GC of the successful creation
    if( objType->GetFlags() & asOBJ_GC )
        objType->GetEngine()->NotifyGarbageCollectorOfNewObject( this, objType );
}

ScriptDict::ScriptDict( asIObjectType* ot, void* listBuffer )
{
    refCount = 1;
    gcFlag = false;
    objType = ot;
    objType->AddRef();
    keyTypeId = objType->GetSubTypeId( 0 );
    valueTypeId = objType->GetSubTypeId( 1 );
    dictMap = new DictMap( DictMapComparator( keyTypeId ) );

    asIScriptEngine* engine = ot->GetEngine();
    DictMap*         dict = (DictMap*) dictMap;

    asBYTE*          buffer = (asBYTE*) listBuffer;
    asUINT           length = *(asUINT*) buffer;
    buffer += 4;

    while( length-- )
    {
        if( asPWORD( buffer ) & 0x3 )
            buffer += 4 - ( asPWORD( buffer ) & 0x3 );

        void* key = buffer;
        if( keyTypeId & asTYPEID_MASK_OBJECT )
        {
            asIObjectType* ot = engine->GetObjectTypeById( keyTypeId );
            if( ot->GetFlags() & asOBJ_VALUE )
                buffer += ot->GetSize();
            else
                buffer += sizeof( void* );
            if( ot->GetFlags() & asOBJ_REF )
                key = *(void**) key;
        }
        else if( keyTypeId == asTYPEID_VOID )
        {
            buffer += sizeof( void* );
        }
        else
        {
            buffer += engine->GetSizeOfPrimitiveType( keyTypeId );
        }

        void* value = buffer;
        if( valueTypeId & asTYPEID_MASK_OBJECT )
        {
            asIObjectType* ot = engine->GetObjectTypeById( valueTypeId );
            if( ot->GetFlags() & asOBJ_VALUE )
                buffer += ot->GetSize();
            else
                buffer += sizeof( void* );
            if( ot->GetFlags() & asOBJ_REF )
                value = *(void**) value;
        }
        else if( valueTypeId == asTYPEID_VOID )
        {
            buffer += sizeof( void* );
        }
        else
        {
            buffer += engine->GetSizeOfPrimitiveType( valueTypeId );
        }

        Set( key, value );
    }

    // Notify the GC of the successful creation
    if( objType->GetFlags() & asOBJ_GC )
        objType->GetEngine()->NotifyGarbageCollectorOfNewObject( this, objType );
}

ScriptDict::ScriptDict( const ScriptDict& other )
{
    refCount = 1;
    gcFlag = false;
    objType = other.objType;
    objType->AddRef();
    keyTypeId = objType->GetSubTypeId( 0 );
    valueTypeId = objType->GetSubTypeId( 1 );
    dictMap = new DictMap( DictMapComparator( keyTypeId ) );

    DictMap* dict = (DictMap*) other.dictMap;
    for( auto it = dict->begin(); it != dict->end(); ++it )
        Set( it->first, it->second );

    if( objType->GetFlags() & asOBJ_GC )
        objType->GetEngine()->NotifyGarbageCollectorOfNewObject( this, objType );
}

ScriptDict& ScriptDict::operator=( const ScriptDict& other )
{
    // Only perform the copy if the array types are the same
    if( &other != this && other.objType == objType )
    {
        Clear();

        DictMap* dict = (DictMap*) other.dictMap;
        for( auto it = dict->begin(); it != dict->end(); ++it )
            Set( it->first, it->second );
    }

    return *this;
}

ScriptDict::~ScriptDict()
{
    Clear();
    delete (DictMap*) dictMap;

    if( objType )
        objType->Release();
}

uint ScriptDict::GetSize() const
{
    DictMap* dict = (DictMap*) dictMap;

    return (uint) dict->size();
}

bool ScriptDict::IsEmpty() const
{
    DictMap* dict = (DictMap*) dictMap;

    return dict->empty();
}

void ScriptDict::Set( void* key, void* value )
{
    DictMap* dict = (DictMap*) dictMap;

    auto     it = dict->find( key );
    if( it == dict->end() )
    {
        key = CopyObject( objType, 0, key );
        value = CopyObject( objType, 1, value );
        dict->insert( PAIR( key, value ) );
    }
    else
    {
        DestroyObject( objType, 1, it->second );
        value = CopyObject( objType, 1, value );
        it->second = value;
    }
}

void ScriptDict::SetIfNotExist( void* key, void* value )
{
    DictMap* dict = (DictMap*) dictMap;

    auto     it = dict->find( key );
    if( it == dict->end() )
    {
        key = CopyObject( objType, 0, key );
        value = CopyObject( objType, 1, value );
        dict->insert( PAIR( key, value ) );
    }
}

bool ScriptDict::Remove( void* key )
{
    DictMap* dict = (DictMap*) dictMap;

    auto     it = dict->find( key );
    if( it != dict->end() )
    {
        DestroyObject( objType, 0, it->first );
        DestroyObject( objType, 1, it->second );
        dict->erase( it );
        return true;
    }

    return false;
}

uint ScriptDict::RemoveValues( void* value )
{
    DictMap* dict = (DictMap*) dictMap;
    uint     result = 0;

    for( auto it = dict->begin(); it != dict->end();)
    {
        if( Equals( valueTypeId, it->second, value ) )
        {
            DestroyObject( objType, 0, it->first );
            DestroyObject( objType, 1, it->second );
            it = dict->erase( it );
            result++;
        }
        else
        {
            ++it;
        }
    }

    return result;
}

void ScriptDict::Clear()
{
    DictMap* dict = (DictMap*) dictMap;

    for( auto it = dict->begin(), end = dict->end(); it != end; ++it )
    {
        DestroyObject( objType, 0, it->first );
        DestroyObject( objType, 1, it->second );
    }
    dict->clear();
}

void* ScriptDict::Get( void* key )
{
    DictMap* dict = (DictMap*) dictMap;

    auto     it = dict->find( key );
    if( it == dict->end() )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Key not found" );
        return NULL;
    }

    return ( *it ).second;
}

void* ScriptDict::GetDefault( void* key, void* defaultValue )
{
    DictMap* dict = (DictMap*) dictMap;

    auto     it = dict->find( key );
    if( it == dict->end() )
        return defaultValue;

    return ( *it ).second;
}

void* ScriptDict::GetKey( uint index )
{
    DictMap* dict = (DictMap*) dictMap;

    if( index >= (uint) dict->size() )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Index out of bounds" );
        return NULL;
    }

    auto it = dict->begin();
    while( index-- )
        it++;

    return ( *it ).first;
}

void* ScriptDict::GetValue( uint index )
{
    DictMap* dict = (DictMap*) dictMap;

    if( index >= (uint) dict->size() )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Index out of bounds" );
        return NULL;
    }

    auto it = dict->begin();
    while( index-- )
        it++;

    return ( *it ).second;
}

bool ScriptDict::Contains( void* key ) const
{
    DictMap* dict = (DictMap*) dictMap;

    return dict->count( key ) > 0;
}

bool ScriptDict::operator==( const ScriptDict& other ) const
{
    if( objType != other.objType )
        return false;

    if( GetSize() != other.GetSize() )
        return false;

    DictMap* dict1 = (DictMap*) dictMap;
    DictMap* dict2 = (DictMap*) other.dictMap;

    auto     it1 = dict1->begin();
    auto     it2 = dict2->begin();
    auto     end1 = dict1->end();
    auto     end2 = dict2->end();

    while( it1 != end1 && it2 != end2 )
    {
        if( !Equals( keyTypeId, ( *it1 ).first, ( *it2 ).first ) ||
            !Equals( valueTypeId, ( *it1 ).second, ( *it2 ).second ) )
            return false;
        it1++;
        it2++;
    }

    return true;
}

// GC behaviour
void ScriptDict::EnumReferences( asIScriptEngine* engine )
{
    // TODO: If garbage collection can be done from a separate thread, then this method must be
    //       protected so that it doesn't get lost during the iteration if the array is modified

    // If the array is holding handles, then we need to notify the GC of them
    DictMap* dict = (DictMap*) dictMap;

    bool     keysHandle = ( keyTypeId & asTYPEID_MASK_OBJECT ) != 0;
    bool     valuesHandle = ( valueTypeId & asTYPEID_MASK_OBJECT ) != 0;

    if( keysHandle || valuesHandle )
    {
        for( auto it = dict->begin(), end = dict->end(); it != end; ++it )
        {
            if( keysHandle )
                engine->GCEnumCallback( ( *it ).first );
            if( valuesHandle )
                engine->GCEnumCallback( ( *it ).second );
        }
    }
}

// GC behaviour
void ScriptDict::ReleaseAllHandles( asIScriptEngine* )
{
    // Resizing to zero will release everything
//    Resize( 0 );
}

void ScriptDict::AddRef() const
{
    // Clear the GC flag then increase the counter
    gcFlag = false;
    asAtomicInc( refCount );
}

void ScriptDict::Release() const
{
    // Clearing the GC flag then descrease the counter
    gcFlag = false;
    if( asAtomicDec( refCount ) == 0 )
    {
        // When reaching 0 no more references to this instance
        // exists and the object should be destroyed
        this->~ScriptDict();
        FreeMem( const_cast< ScriptDict* >( this ), sizeof( ScriptDict ) );
    }
}

// GC behaviour
int ScriptDict::GetRefCount()
{
    return refCount;
}

// GC behaviour
void ScriptDict::SetFlag()
{
    gcFlag = true;
}

// GC behaviour
bool ScriptDict::GetFlag()
{
    return gcFlag;
}

void* ScriptDict::GetMap()
{
    return dictMap;
}

// internal
static void* CopyObject( asIObjectType* objType, int subTypeIndex, void* value )
{
    int              subTypeId = objType->GetSubTypeId( subTypeIndex );
    asIObjectType*   subType = objType->GetSubType( subTypeIndex );
    asIScriptEngine* engine = objType->GetEngine();

    int              elementSize;
    if( subTypeId & asTYPEID_MASK_OBJECT )
        elementSize = sizeof( asPWORD );
    else
        elementSize = engine->GetSizeOfPrimitiveType( subTypeId );

    void* ptr = AllocMem( elementSize );
    memzero( ptr, elementSize );

    if( ( subTypeId & ~asTYPEID_MASK_SEQNBR ) && !( subTypeId & asTYPEID_OBJHANDLE ) )
    {
        ptr = engine->CreateScriptObjectCopy( value, subType );
    }
    else if( subTypeId & asTYPEID_OBJHANDLE )
    {
        void* tmp = *(void**) ptr;
        *(void**) ptr = *(void**) value;
        subType->GetEngine()->AddRefScriptObject( *(void**) value, subType );
        if( tmp )
            subType->GetEngine()->ReleaseScriptObject( tmp, subType );
    }
    else if( subTypeId == asTYPEID_BOOL ||
             subTypeId == asTYPEID_INT8 ||
             subTypeId == asTYPEID_UINT8 )
        *(char*) ptr = *(char*) value;
    else if( subTypeId == asTYPEID_INT16 ||
             subTypeId == asTYPEID_UINT16 )
        *(short*) ptr = *(short*) value;
    else if( subTypeId == asTYPEID_INT32 ||
             subTypeId == asTYPEID_UINT32 ||
             subTypeId == asTYPEID_FLOAT ||
             subTypeId > asTYPEID_DOUBLE )      // enums have a type id larger than doubles
        *(int*) ptr = *(int*) value;
    else if( subTypeId == asTYPEID_INT64 ||
             subTypeId == asTYPEID_UINT64 ||
             subTypeId == asTYPEID_DOUBLE )
        *(double*) ptr = *(double*) value;

    return ptr;
}

static void DestroyObject( asIObjectType* objType, int subTypeIndex, void* value )
{
    int              subTypeId = objType->GetSubTypeId( subTypeIndex );
    asIScriptEngine* engine = objType->GetEngine();

    int              elementSize;
    if( subTypeId & asTYPEID_MASK_OBJECT )
        elementSize = sizeof( asPWORD );
    else
        elementSize = engine->GetSizeOfPrimitiveType( subTypeId );

    FreeMem( value, elementSize );
}

static bool Less( int typeId, const void* a, const void* b )
{
    if( !( typeId & ~asTYPEID_MASK_SEQNBR ) )
    {
        // Simple compare of values
        switch( typeId )
        {
            #define COMPARE( T )    *( (T*) a ) < *( (T*) b )
        case asTYPEID_BOOL:
            return COMPARE( bool );
        case asTYPEID_INT8:
            return COMPARE( signed char );
        case asTYPEID_UINT8:
            return COMPARE( unsigned char );
        case asTYPEID_INT16:
            return COMPARE( signed short );
        case asTYPEID_UINT16:
            return COMPARE( unsigned short );
        case asTYPEID_INT32:
            return COMPARE( signed int );
        case asTYPEID_UINT32:
            return COMPARE( unsigned int );
        case asTYPEID_FLOAT:
            return COMPARE( float );
        case asTYPEID_DOUBLE:
            return COMPARE( double );
        default:
            return COMPARE( signed int );                      // All enums fall in this case
            #undef COMPARE
        }
    }
    return a < b;
}

static bool Equals( int typeId, const void* a, const void* b )
{
    if( !( typeId & ~asTYPEID_MASK_SEQNBR ) )
    {
        // Simple compare of values
        switch( typeId )
        {
            #define COMPARE( T )    *( (T*) a ) == *( (T*) b )
        case asTYPEID_BOOL:
            return COMPARE( bool );
        case asTYPEID_INT8:
            return COMPARE( signed char );
        case asTYPEID_UINT8:
            return COMPARE( unsigned char );
        case asTYPEID_INT16:
            return COMPARE( signed short );
        case asTYPEID_UINT16:
            return COMPARE( unsigned short );
        case asTYPEID_INT32:
            return COMPARE( signed int );
        case asTYPEID_UINT32:
            return COMPARE( unsigned int );
        case asTYPEID_FLOAT:
            return COMPARE( float );
        case asTYPEID_DOUBLE:
            return COMPARE( double );
        default:
            return COMPARE( signed int );                      // All enums fall here
            #undef COMPARE
        }
    }
    return a == b;
}
