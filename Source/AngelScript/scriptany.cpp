#include "scriptany.h"
#include <new>
#include <assert.h>
#include <string.h>

// We'll use the generic interface for the factories as we need the engine pointer
static void ScriptAnyFactory_Generic( asIScriptGeneric* gen )
{
    asIScriptEngine* engine = gen->GetEngine();

    *(ScriptAny**) gen->GetAddressOfReturnLocation() = new ScriptAny( engine );
}

static void ScriptAnyFactory2_Generic( asIScriptGeneric* gen )
{
    asIScriptEngine* engine = gen->GetEngine();
    void*            ref = (void*) gen->GetArgAddress( 0 );
    int              refType = gen->GetArgTypeId( 0 );

    *(ScriptAny**) gen->GetAddressOfReturnLocation() = new ScriptAny( ref, refType, engine );
}

static ScriptAny& ScriptAnyAssignment( ScriptAny* other, ScriptAny* self )
{
    return *self = *other;
}

static void ScriptAnyAssignment_Generic( asIScriptGeneric* gen )
{
    ScriptAny* other = (ScriptAny*) gen->GetArgObject( 0 );
    ScriptAny* self = (ScriptAny*) gen->GetObject();

    *self = *other;

    gen->SetReturnObject( self );
}

static void ScriptAny_Store_Generic( asIScriptGeneric* gen )
{
    void*      ref = (void*) gen->GetArgAddress( 0 );
    int        refTypeId = gen->GetArgTypeId( 0 );
    ScriptAny* self = (ScriptAny*) gen->GetObject();

    self->Store( ref, refTypeId );
}

static void ScriptAny_StoreInt_Generic( asIScriptGeneric* gen )
{
    asINT64*   ref = (asINT64*) gen->GetArgAddress( 0 );
    ScriptAny* self = (ScriptAny*) gen->GetObject();

    self->Store( *ref );
}

static void ScriptAny_StoreFlt_Generic( asIScriptGeneric* gen )
{
    double*    ref = (double*) gen->GetArgAddress( 0 );
    ScriptAny* self = (ScriptAny*) gen->GetObject();

    self->Store( *ref );
}

static void ScriptAny_Retrieve_Generic( asIScriptGeneric* gen )
{
    void*      ref = (void*) gen->GetArgAddress( 0 );
    int        refTypeId = gen->GetArgTypeId( 0 );
    ScriptAny* self = (ScriptAny*) gen->GetObject();

    *(bool*) gen->GetAddressOfReturnLocation() = self->Retrieve( ref, refTypeId );
}

static void ScriptAny_RetrieveInt_Generic( asIScriptGeneric* gen )
{
    asINT64*   ref = (asINT64*) gen->GetArgAddress( 0 );
    ScriptAny* self = (ScriptAny*) gen->GetObject();

    *(bool*) gen->GetAddressOfReturnLocation() = self->Retrieve( *ref );
}

static void ScriptAny_RetrieveFlt_Generic( asIScriptGeneric* gen )
{
    double*    ref = (double*) gen->GetArgAddress( 0 );
    ScriptAny* self = (ScriptAny*) gen->GetObject();

    *(bool*) gen->GetAddressOfReturnLocation() = self->Retrieve( *ref );
}

static void ScriptAny_AddRef_Generic( asIScriptGeneric* gen )
{
    ScriptAny* self = (ScriptAny*) gen->GetObject();
    self->AddRef();
}

static void ScriptAny_Release_Generic( asIScriptGeneric* gen )
{
    ScriptAny* self = (ScriptAny*) gen->GetObject();
    self->Release();
}

static void ScriptAny_GetRefCount_Generic( asIScriptGeneric* gen )
{
    ScriptAny* self = (ScriptAny*) gen->GetObject();
    *(int*) gen->GetAddressOfReturnLocation() = self->GetRefCount();
}

static void ScriptAny_SetFlag_Generic( asIScriptGeneric* gen )
{
    ScriptAny* self = (ScriptAny*) gen->GetObject();
    self->SetFlag();
}

static void ScriptAny_GetFlag_Generic( asIScriptGeneric* gen )
{
    ScriptAny* self = (ScriptAny*) gen->GetObject();
    *(bool*) gen->GetAddressOfReturnLocation() = self->GetFlag();
}

static void ScriptAny_EnumReferences_Generic( asIScriptGeneric* gen )
{
    ScriptAny*       self = (ScriptAny*) gen->GetObject();
    asIScriptEngine* engine = *(asIScriptEngine**) gen->GetAddressOfArg( 0 );
    self->EnumReferences( engine );
}

static void ScriptAny_ReleaseAllHandles_Generic( asIScriptGeneric* gen )
{
    ScriptAny*       self = (ScriptAny*) gen->GetObject();
    asIScriptEngine* engine = *(asIScriptEngine**) gen->GetAddressOfArg( 0 );
    self->ReleaseAllHandles( engine );
}

void RegisterScriptAny_Native( asIScriptEngine* engine );
void RegisterScriptAny_Generic( asIScriptEngine* engine );
void RegisterScriptAny( asIScriptEngine* engine )
{
    if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) )
        RegisterScriptAny_Generic( engine );
    else
        RegisterScriptAny_Native( engine );
}

void RegisterScriptAny_Native( asIScriptEngine* engine )
{
    int r;
    r = engine->RegisterObjectType( "any", sizeof( ScriptAny ), asOBJ_REF | asOBJ_GC );
    assert( r >= 0 );

    // We'll use the generic interface for the constructor as we need the engine pointer
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_FACTORY, "any@ f()", asFUNCTION( ScriptAnyFactory_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_FACTORY, "any@ f(?&in)", asFUNCTION( ScriptAnyFactory2_Generic ), asCALL_GENERIC );
    assert( r >= 0 );

    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_ADDREF, "void f()", asMETHOD( ScriptAny, AddRef ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_RELEASE, "void f()", asMETHOD( ScriptAny, Release ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "any &opAssign(any&in)", asFUNCTION( ScriptAnyAssignment ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "void store(?&in)", asMETHODPR( ScriptAny, Store, ( void*, int ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "void store(int64&in)", asMETHODPR( ScriptAny, Store, ( asINT64 & ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "void store(double&in)", asMETHODPR( ScriptAny, Store, ( double& ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "bool retrieve(?&out)", asMETHODPR( ScriptAny, Retrieve, ( void*, int ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "bool retrieve(int64&out)", asMETHODPR( ScriptAny, Retrieve, ( asINT64 & ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "bool retrieve(double&out)", asMETHODPR( ScriptAny, Retrieve, ( double& ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );

    // Register GC behaviours
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_GETREFCOUNT, "int f()", asMETHOD( ScriptAny, GetRefCount ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_SETGCFLAG, "void f()", asMETHOD( ScriptAny, SetFlag ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_GETGCFLAG, "bool f()", asMETHOD( ScriptAny, GetFlag ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_ENUMREFS, "void f(int&in)", asMETHOD( ScriptAny, EnumReferences ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_RELEASEREFS, "void f(int&in)", asMETHOD( ScriptAny, ReleaseAllHandles ), asCALL_THISCALL );
    assert( r >= 0 );
}

void RegisterScriptAny_Generic( asIScriptEngine* engine )
{
    int r;
    r = engine->RegisterObjectType( "any", sizeof( ScriptAny ), asOBJ_REF | asOBJ_GC );
    assert( r >= 0 );

    // We'll use the generic interface for the constructor as we need the engine pointer
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_FACTORY, "any@ f()", asFUNCTION( ScriptAnyFactory_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_FACTORY, "any@ f(?&in)", asFUNCTION( ScriptAnyFactory2_Generic ), asCALL_GENERIC );
    assert( r >= 0 );

    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_ADDREF, "void f()", asFUNCTION( ScriptAny_AddRef_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_RELEASE, "void f()", asFUNCTION( ScriptAny_Release_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "any &opAssign(any&in)", asFUNCTION( ScriptAnyAssignment_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "void store(?&in)", asFUNCTION( ScriptAny_Store_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "void store(int64&in)", asFUNCTION( ScriptAny_StoreInt_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "void store(double&in)", asFUNCTION( ScriptAny_StoreFlt_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "bool retrieve(?&out) const", asFUNCTION( ScriptAny_Retrieve_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "bool retrieve(int64&out) const", asFUNCTION( ScriptAny_RetrieveInt_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "any", "bool retrieve(double&out) const", asFUNCTION( ScriptAny_RetrieveFlt_Generic ), asCALL_GENERIC );
    assert( r >= 0 );

    // Register GC behaviours
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_GETREFCOUNT, "int f()", asFUNCTION( ScriptAny_GetRefCount_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_SETGCFLAG, "void f()", asFUNCTION( ScriptAny_SetFlag_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_GETGCFLAG, "bool f()", asFUNCTION( ScriptAny_GetFlag_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_ENUMREFS, "void f(int&in)", asFUNCTION( ScriptAny_EnumReferences_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "any", asBEHAVE_RELEASEREFS, "void f(int&in)", asFUNCTION( ScriptAny_ReleaseAllHandles_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
}

void ScriptAny::Assign( const ScriptAny& other )
{
    // Hold on to the object type reference so it isn't destroyed too early
    if( other.value.valueObj && ( other.value.typeId & asTYPEID_MASK_OBJECT ) )
    {
        asITypeInfo* ot = engine->GetTypeInfoById( other.value.typeId );
        if( ot )
            ot->AddRef();
    }

    FreeObject();

    value.typeId = other.value.typeId;
    if( value.typeId & asTYPEID_OBJHANDLE )
    {
        // For handles, copy the pointer and increment the reference count
        value.valueObj = other.value.valueObj;
        engine->AddRefScriptObject( value.valueObj, engine->GetTypeInfoById( value.typeId ) );
    }
    else if( value.typeId & asTYPEID_MASK_OBJECT )
    {
        // Create a copy of the object
        value.valueObj = engine->CreateScriptObjectCopy( other.value.valueObj, engine->GetTypeInfoById(value.typeId));
    }
    else
    {
        // Primitives can be copied directly
        value.valueInt = other.value.valueInt;
    }
}

int ScriptAny::CopyFrom( const ScriptAny* other )
{
    if( other == 0 )
        return asINVALID_ARG;

    *this = *(ScriptAny*) other;

    return 0;
}

ScriptAny::ScriptAny( asIScriptEngine* engine )
{
    this->engine = engine;
    refCount = 1;
	gcFlag = false;

    value.typeId = 0;
    value.valueInt = 0;

    // Notify the garbage collector of this object
    engine->NotifyGarbageCollectorOfNewObject( this, engine->GetTypeInfoByName( "any" ) );
}

ScriptAny::ScriptAny( void* ref, int refTypeId, asIScriptEngine* engine )
{
    this->engine = engine;
    refCount = 1;
	gcFlag = false;

    value.typeId = 0;
    value.valueInt = 0;

    // Notify the garbage collector of this object
    engine->NotifyGarbageCollectorOfNewObject( this, engine->GetTypeInfoByName( "any" ) );

    Store( ref, refTypeId );
}

ScriptAny::~ScriptAny()
{
    FreeObject();
}

void ScriptAny::Store( void* ref, int refTypeId )
{
	// This method is not expected to be used for primitive types, except for bool, int64, or double
	assert(refTypeId > asTYPEID_DOUBLE || refTypeId == asTYPEID_VOID || refTypeId == asTYPEID_BOOL || refTypeId == asTYPEID_INT64 || refTypeId == asTYPEID_DOUBLE);

    // Hold on to the object type reference so it isn't destroyed too early
    if( *(void**) ref && ( refTypeId & asTYPEID_MASK_OBJECT ) )
    {
        asITypeInfo* ot = engine->GetTypeInfoById( refTypeId );
        if( ot )
            ot->AddRef();
    }

    FreeObject();

    value.typeId = refTypeId;
    if( value.typeId & asTYPEID_OBJHANDLE )
    {
        // We're receiving a reference to the handle, so we need to dereference it
        value.valueObj = *(void**) ref;
        engine->AddRefScriptObject( value.valueObj, engine->GetTypeInfoById(value.typeId));
    }
    else if( value.typeId & asTYPEID_MASK_OBJECT )
    {
        // Create a copy of the object
        value.valueObj = engine->CreateScriptObjectCopy( ref, engine->GetTypeInfoById(value.typeId));
    }
    else
    {
        // Primitives can be copied directly
        value.valueInt = 0;

        // Copy the primitive value
        // We receive a pointer to the value.
        int size = engine->GetSizeOfPrimitiveType( value.typeId );
        memcpy( &value.valueInt, ref, size );
    }
}

void ScriptAny::Store( double& ref )
{
    Store( &ref, asTYPEID_DOUBLE );
}

void ScriptAny::Store( asINT64& ref )
{
    Store( &ref, asTYPEID_INT64 );
}


bool ScriptAny::Retrieve( void* ref, int refTypeId ) const
{
	// This method is not expected to be used for primitive types, except for bool, int64, or double
	assert(refTypeId > asTYPEID_DOUBLE || refTypeId == asTYPEID_BOOL || refTypeId == asTYPEID_INT64 || refTypeId == asTYPEID_DOUBLE);

    if( refTypeId & asTYPEID_OBJHANDLE )
    {
        // Is the handle type compatible with the stored value?

        // A handle can be retrieved if the stored type is a handle of same or compatible type
        // or if the stored type is an object that implements the interface that the handle refer to.
        if( ( value.typeId & asTYPEID_MASK_OBJECT ) )
        {
			// RefCastObject will increment the refCount of the returned pointer if successful
			engine->RefCastObject(value.valueObj, engine->GetTypeInfoById(value.typeId), engine->GetTypeInfoById(refTypeId), reinterpret_cast<void**>(ref));
			if (*(asPWORD*)ref == 0)
				return false;
            return true;
        }
    }
    else if( refTypeId & asTYPEID_MASK_OBJECT )
    {
        // Is the object type compatible with the stored value?

        // Copy the object into the given reference
        if( value.typeId == refTypeId )
        {
            engine->AssignScriptObject( ref, value.valueObj, engine->GetTypeInfoById(value.typeId));

            return true;
        }
    }
    else
    {
        // Is the primitive type compatible with the stored value?

        if( value.typeId == refTypeId )
        {
            int size = engine->GetSizeOfPrimitiveType( refTypeId );
            memcpy( ref, &value.valueInt, size );
            return true;
        }

        // We know all numbers are stored as either int64 or double, since we register overloaded functions for those
        if( value.typeId == asTYPEID_INT64 && refTypeId == asTYPEID_DOUBLE )
        {
            *(double*) ref = double(value.valueInt);
            return true;
        }
        else if( value.typeId == asTYPEID_DOUBLE && refTypeId == asTYPEID_INT64 )
        {
            *(asINT64*) ref = asINT64( value.valueFlt );
            return true;
        }
    }

    return false;
}

bool ScriptAny::Retrieve( asINT64& value ) const
{
    return Retrieve( &value, asTYPEID_INT64 );
}

bool ScriptAny::Retrieve( double& value ) const
{
    return Retrieve( &value, asTYPEID_DOUBLE );
}

int ScriptAny::GetTypeId() const
{
    return value.typeId;
}

void ScriptAny::FreeObject()
{
    // If it is a handle or a ref counted object, call release
    if( value.typeId & asTYPEID_MASK_OBJECT )
    {
        // Let the engine release the object
        engine->ReleaseScriptObject( value.valueObj, engine->GetTypeInfoById(value.typeId));

        // Release the object type info
        asITypeInfo* ot = engine->GetTypeInfoById( value.typeId );
        if( ot )
            ot->Release();

        value.valueObj = 0;
        value.typeId = 0;
    }

    // For primitives, there's nothing to do
}


void ScriptAny::EnumReferences( asIScriptEngine* engine )
{
    // If we're holding a reference, we'll notify the garbage collector of it
    if( value.valueObj && ( value.typeId & asTYPEID_MASK_OBJECT ) )
    {
        engine->GCEnumCallback( value.valueObj );

        // The object type itself is also garbage collected
        asITypeInfo* ot = engine->GetTypeInfoById( value.typeId );
        if( ot )
            engine->GCEnumCallback( ot );
    }
}

void ScriptAny::ReleaseAllHandles( asIScriptEngine* /*engine*/ )
{
    FreeObject();
}

void ScriptAny::AddRef() const
{
	// Increase counter and clear flag set by GC
	gcFlag = false;
	asAtomicInc(refCount);
}

void ScriptAny::Release() const
{
	// Decrease the ref counter
	gcFlag = false;
	if (asAtomicDec(refCount) == 0)
	{
		// Delete this object as no more references to it exists
		delete this;
		return;
	}
}

int ScriptAny::GetRefCount()
{
	return refCount;
}

void ScriptAny::SetFlag()
{
	gcFlag = true;
}

bool ScriptAny::GetFlag()
{
	return gcFlag;
}
