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

static bool Less( int typeId, const void* a, const void* b );
static bool Equals( int typeId, const void* a, const void* b );

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
    asIScriptContext* ctx = asGetActiveContext();

    // Allocate the memory
    void* mem = AllocMem( sizeof( ScriptDict ) );
    if( mem == 0 )
    {
        if( ctx )
            ctx->SetException( "Out of memory" );

        return 0;
    }

    // Initialize the object
    ScriptDict* d = new (mem) ScriptDict( ot );

    // It's possible the constructor raised a script exception, in which case we
    // need to free the memory and return null instead, else we get a memory leak.
    if( ctx && ctx->GetState() == asEXECUTION_EXCEPTION )
    {
        d->Release();
        return 0;
    }

    return d;
}

ScriptDict* ScriptDict::Create( asIObjectType* ot, void* initList )
{
    asIScriptContext* ctx = asGetActiveContext();

    // Allocate the memory
    void* mem = AllocMem( sizeof( ScriptDict ) );
    if( mem == 0 )
    {
        if( ctx )
            ctx->SetException( "Out of memory" );

        return 0;
    }

    // Initialize the object
    ScriptDict* d = new (mem) ScriptDict( ot, initList );

    // It's possible the constructor raised a script exception, in which case we
    // need to free the memory and return null instead, else we get a memory leak.
    if( ctx && ctx->GetState() == asEXECUTION_EXCEPTION )
    {
        d->Release();
        return 0;
    }

    return d;
}

// This optional callback is called when the template type is first used by the compiler.
// It allows the application to validate if the template can be instanciated for the requested
// subtype at compile time, instead of at runtime. The output argument dontGarbageCollect
// allow the callback to tell the engine if the template instance type shouldn't be garbage collected,
// i.e. no asOBJ_GC flag.
static bool ScriptDictTemplateCallbackExt( asIObjectType* ot, bool& dontGarbageCollect )
{
    // Make sure the subtype can be instanciated with a default factory/constructor,
    // otherwise we won't be able to instanciate the elements.
    int typeId = ot->GetTypeId();
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
    return ScriptDictTemplateCallbackExt( ot->GetSubType( 0 ), dontGarbageCollect ) &&
           ScriptDictTemplateCallbackExt( ot->GetSubType( 1 ), dontGarbageCollect );
}

// Registers the template dict type
void RegisterScriptDict( asIScriptEngine* engine )
{
    // if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) == 0 )
    //    RegisterScriptDict_Native( engine );
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
    // r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_LIST_FACTORY, "dict<T1>@ f(int&in type, int&in list) {repeat T1}", asFUNCTIONPR( ScriptDict::Create, ( asIObjectType*, void* ), ScriptDict* ), asCALL_CDECL );
    // assert( r >= 0 );

    // The memory management methods
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_ADDREF, "void f()", asMETHOD( ScriptDict, AddRef ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_RELEASE, "void f()", asMETHOD( ScriptDict, Release ), asCALL_THISCALL );
    assert( r >= 0 );

    // The index operator returns the template subtype
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "const T2& get_opIndex(const T1&in) const", asMETHODPR( ScriptDict, Find, (void*) const, const void* ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "void set_opIndex(const T1&in, const T2&in)", asMETHODPR( ScriptDict, Find, (void*), void* ), asCALL_THISCALL );
    assert( r >= 0 );

    // The assignment operator
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "dict<T1,T2>& opAssign(const dict<T1,T2>&in)", asMETHOD( ScriptDict, operator= ), asCALL_THISCALL );
    assert( r >= 0 );

    // Other methods
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "void insert(const T1&in, const T2&in)", asMETHOD( ScriptDict, Insert ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "bool remove(const T1&in)", asMETHOD( ScriptDict, Remove ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "bool removeValues(const T2&in)", asMETHOD( ScriptDict, RemoveValues ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "uint length() const", asMETHOD( ScriptDict, GetSize ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "void clear()", asMETHODPR( ScriptDict, Clear, ( ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "const T2& get(const T1&in, const T2&in)", asMETHODPR( ScriptDict, FindInsert, ( void*, void* ), void* ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dict<T1,T2>", "bool contains(const T1&in) const", asMETHODPR( ScriptDict, Contains, (void*) const, bool ), asCALL_THISCALL );
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
    dictMap = new DictMap( DictMapComparator( ot->GetSubTypeId( 0 ) ) );

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
    dictMap = new DictMap( DictMapComparator( ot->GetSubTypeId( 0 ) ) );

/*
    asIScriptEngine* engine = ot->GetEngine();

    // Determine element size
    if( subTypeId & asTYPEID_MASK_OBJECT )
        elementSize = sizeof( asPWORD );
    else
        elementSize = engine->GetSizeOfPrimitiveType( subTypeId );

    // Determine the initial size from the buffer
    asUINT length = *(asUINT*) buf;

    // Make sure the array size isn't too large for us to handle
    if( !CheckMaxSize( length ) )
    {
        // Don't continue with the initialization
        return;
    }

    // Copy the values of the array elements from the buffer
    if( ( ot->GetSubTypeId() & asTYPEID_MASK_OBJECT ) == 0 )
    {
        CreateBuffer( &buffer, length );

        // Copy the values of the primitive type into the internal buffer
        if( length > 0 )
            memcpy( At( 0 ), ( ( (asUINT*) buf ) + 1 ), length * elementSize );
    }
    else if( ot->GetSubTypeId() & asTYPEID_OBJHANDLE )
    {
        CreateBuffer( &buffer, length );

        // Copy the handles into the internal buffer
        if( length > 0 )
            memcpy( At( 0 ), ( ( (asUINT*) buf ) + 1 ), length * elementSize );

        // With object handles it is safe to clear the memory in the received buffer
        // instead of increasing the ref count. It will save time both by avoiding the
        // call the increase ref, and also relieve the engine from having to release
        // its references too
        memset( ( ( (asUINT*) buf ) + 1 ), 0, length * elementSize );
    }
    else if( ot->GetSubType()->GetFlags() & asOBJ_REF )
    {
        // Only allocate the buffer, but not the objects
        subTypeId |= asTYPEID_OBJHANDLE;
        CreateBuffer( &buffer, length );
        subTypeId &= ~asTYPEID_OBJHANDLE;

        // Copy the handles into the internal buffer
        if( length > 0 )
            memcpy( buffer->data, ( ( (asUINT*) buf ) + 1 ), length * elementSize );

        // For ref types we can do the same as for handles, as they are
        // implicitly stored as handles.
        memset( ( ( (asUINT*) buf ) + 1 ), 0, length * elementSize );
    }
    else
    {
        // TODO: Optimize by calling the copy constructor of the object instead of
        //       constructing with the default constructor and then assigning the value
        // TODO: With C++11 ideally we should be calling the move constructor, instead
        //       of the copy constructor as the engine will just discard the objects in the
        //       buffer afterwards.
        CreateBuffer( &buffer, length );

        // For value types we need to call the opAssign for each individual object
        for( asUINT n = 0; n < length; n++ )
        {
            void*                obj = At( n );
            asBYTE*              srcObj = (asBYTE*) buf;
            srcObj += 4 + n* ot->GetSubType()->GetSize();
            engine->AssignScriptObject( obj, srcObj, ot->GetSubType() );
        }
    }*/

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

    *(DictMap*) dictMap = *(DictMap*) other.dictMap;

    if( objType->GetFlags() & asOBJ_GC )
        objType->GetEngine()->NotifyGarbageCollectorOfNewObject( this, objType );
}

ScriptDict& ScriptDict::operator=( const ScriptDict& other )
{
    // Only perform the copy if the array types are the same
    if( &other != this && other.objType == objType )
        *(DictMap*) dictMap = *(DictMap*) other.dictMap;

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

void ScriptDict::Insert( void* key, void* value )
{
    DictMap* dict = (DictMap*) dictMap;

    dict->insert( PAIR( key, value ) );
}

bool ScriptDict::Remove( void* key )
{
//     if( index >= buffer->numElements )
//     {
//         // If this is called from a script we raise a script exception
//         asIScriptContext* ctx = asGetActiveContext();
//         if( ctx )
//             ctx->SetException( "Index out of bounds" );
//         return;
//     }
//
//     // Remove the element
//     Resize( -1, index );
    return 0;
}

uint ScriptDict::RemoveValues( void* value )
{
//     if( index >= buffer->numElements )
//     {
//         // If this is called from a script we raise a script exception
//         asIScriptContext* ctx = asGetActiveContext();
//         if( ctx )
//             ctx->SetException( "Index out of bounds" );
//         return;
//     }
//
//     // Remove the element
//     Resize( -1, index );
    return 0;
}

void ScriptDict::Clear()
{
    DictMap*       dict = (DictMap*) dictMap;

    asIObjectType* otKey = objType->GetSubType( 0 );
    asIObjectType* otValue = objType->GetSubType( 0 );

    bool           releaseKeys = ( otKey->GetTypeId() & asTYPEID_MASK_OBJECT ) != 0;
    bool           releaseValues = ( otValue->GetTypeId() & asTYPEID_MASK_OBJECT ) != 0;

    if( releaseKeys || releaseValues )
    {
        asIScriptEngine* engine = objType->GetEngine();
        for( auto it = dict->begin(), end = dict->end(); it != end; ++it )
        {
            if( releaseKeys && ( *it ).first )
                engine->ReleaseScriptObject( ( *it ).first, otKey );
            if( releaseKeys && ( *it ).second )
                engine->ReleaseScriptObject( ( *it ).second, otValue );
        }
    }

    dict->clear();
}

// Return a pointer to the array element. Returns 0 if the index is out of bounds
const void* ScriptDict::Find( void* key ) const
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

void* ScriptDict::Find( void* key )
{
    return const_cast< void* >( const_cast< const ScriptDict* >( this )->Find( key ) );
}

void* ScriptDict::FindInsert( void* key, void* value )
{
    DictMap* dict = (DictMap*) dictMap;

    auto     it = dict->find( key );
    if( it == dict->end() )
    {
        Insert( key, value );
        return value;
    }

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

    int      keyTypeId = objType->GetSubTypeId( 0 );
    int      valueTypeId = objType->GetSubTypeId( 1 );

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

    bool     keysHandle = ( objType->GetSubTypeId( 0 ) & asTYPEID_MASK_OBJECT ) != 0;
    bool     valuesHandle = ( objType->GetSubTypeId( 1 ) & asTYPEID_MASK_OBJECT ) != 0;

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

// internal
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
