#include <assert.h>
#include <string.h>
#include "scriptdictionary.h"
#include "scriptstring.h"

#define AS_USE_STLNAMES    0

using namespace std;

// --------------------------------------------------------------------------
// CScriptDictionary implementation

ScriptDictionary::ScriptDictionary( asIScriptEngine* engine )
{
    // We start with one reference
    refCount = 1;

    // Keep a reference to the engine for as long as we live
    // We don't increment the reference counter, because the
    // engine will hold a pointer to the object.
    this->engine = engine;

    // Notify the garbage collector of this object
    // TODO: The type id should be cached
    engine->NotifyGarbageCollectorOfNewObject( this, engine->GetObjectTypeByName( "dictionary" ) );
}

ScriptDictionary::~ScriptDictionary()
{
    // Delete all keys and values
    DeleteAll();
}

void ScriptDictionary::AddRef() const
{
    // We need to clear the GC flag
    refCount = ( refCount & 0x7FFFFFFF ) + 1;
}

void ScriptDictionary::Release() const
{
    // We need to clear the GC flag
    refCount = ( refCount & 0x7FFFFFFF ) - 1;
    if( refCount == 0 )
        delete this;
}

int ScriptDictionary::GetRefCount()
{
    return refCount & 0x7FFFFFFF;
}

void ScriptDictionary::SetGCFlag()
{
    refCount |= 0x80000000;
}

bool ScriptDictionary::GetGCFlag()
{
    return ( refCount & 0x80000000 ) ? true : false;
}

void ScriptDictionary::EnumReferences( asIScriptEngine* engine )
{
    // Call the gc enum callback for each of the objects
    map< string, valueStruct >::iterator it;
    for( it = dict.begin(); it != dict.end(); it++ )
    {
        if( it->second.typeId & asTYPEID_MASK_OBJECT )
            engine->GCEnumCallback( it->second.valueObj );
    }
}

void ScriptDictionary::ReleaseAllReferences( asIScriptEngine* /*engine*/ )
{
    // We're being told to release all references in
    // order to break circular references for dead objects
    DeleteAll();
}

void ScriptDictionary::Assign( const ScriptDictionary& other )
{
    // Clear everything we had before
    DeleteAll();

    // Do a shallow copy of the dictionary
    ScriptString*                              tmp_str = NULL;
    map< string, valueStruct >::const_iterator it;
    for( it = other.dict.begin(); it != other.dict.end(); it++ )
    {
        if( !tmp_str )
            tmp_str = new ScriptString( it->first );
        else
            *tmp_str = it->first;

        if( it->second.typeId & asTYPEID_OBJHANDLE )
            Set( *tmp_str, (void*) &it->second.valueObj, it->second.typeId );
        else if( it->second.typeId & asTYPEID_MASK_OBJECT )
            Set( *tmp_str, (void*) it->second.valueObj, it->second.typeId );
        else
            Set( *tmp_str, (void*) &it->second.valueInt, it->second.typeId );
    }
    if( tmp_str )
        tmp_str->Release();
}

void ScriptDictionary::Set( const ScriptString& key, void* value, int typeId )
{
    valueStruct valStruct = { { 0 }, 0 };
    valStruct.typeId = typeId;
    if( typeId & asTYPEID_OBJHANDLE )
    {
        // We're receiving a reference to the handle, so we need to dereference it
        valStruct.valueObj = *(void**) value;
        engine->AddRefScriptObject( valStruct.valueObj, typeId );
    }
    else if( typeId & asTYPEID_MASK_OBJECT )
    {
        // Create a copy of the object
        valStruct.valueObj = engine->CreateScriptObjectCopy( value, typeId );
    }
    else
    {
        // Copy the primitive value
        // We receive a pointer to the value.
        int size = engine->GetSizeOfPrimitiveType( typeId );
        memcpy( &valStruct.valueInt, value, size );
    }

    map< string, valueStruct >::iterator it;
    it = dict.find( key.c_std_str() );
    if( it != dict.end() )
    {
        FreeValue( it->second );

        // Insert the new value
        it->second = valStruct;
    }
    else
    {
        dict.insert( map< string, valueStruct >::value_type( key.c_std_str(), valStruct ) );
    }
}

// This overloaded method is implemented so that all integer and
// unsigned integers types will be stored in the dictionary as int64
// through implicit conversions. This simplifies the management of the
// numeric types when the script retrieves the stored value using a
// different type.
void ScriptDictionary::Set( const ScriptString& key, asINT64& value )
{
    Set( key, &value, asTYPEID_INT64 );
}

// This overloaded method is implemented so that all floating point types
// will be stored in the dictionary as double through implicit conversions.
// This simplifies the management of the numeric types when the script
// retrieves the stored value using a different type.
void ScriptDictionary::Set( const ScriptString& key, double& value )
{
    Set( key, &value, asTYPEID_DOUBLE );
}

// Returns true if the value was successfully retrieved
bool ScriptDictionary::Get( const ScriptString& key, void* value, int typeId ) const
{
    map< string, valueStruct >::const_iterator it;
    it = dict.find( key.c_std_str() );
    if( it != dict.end() )
    {
        // Return the value
        if( typeId & asTYPEID_OBJHANDLE )
        {
            // A handle can be retrieved if the stored type is a handle of same or compatible type
            // or if the stored type is an object that implements the interface that the handle refer to.
            if( ( it->second.typeId & asTYPEID_MASK_OBJECT ) &&
                engine->IsHandleCompatibleWithObject( it->second.valueObj, it->second.typeId, typeId ) )
            {
                engine->AddRefScriptObject( it->second.valueObj, it->second.typeId );
                *(void**) value = it->second.valueObj;

                return true;
            }
        }
        else if( typeId & asTYPEID_MASK_OBJECT )
        {
            // Verify that the copy can be made
            bool isCompatible = false;
            if( it->second.typeId == typeId )
                isCompatible = true;

            // Copy the object into the given reference
            if( isCompatible )
            {
                engine->AssignScriptObject( value, it->second.valueObj, typeId );

                return true;
            }
        }
        else
        {
            if( it->second.typeId == typeId )
            {
                int size = engine->GetSizeOfPrimitiveType( typeId );
                memcpy( value, &it->second.valueInt, size );
                return true;
            }

            // We know all numbers are stored as either int64 or double, since we register overloaded functions for those
            if( it->second.typeId == asTYPEID_INT64 && typeId == asTYPEID_DOUBLE )
            {
                *(double*) value = double(it->second.valueInt);
                return true;
            }
            else if( it->second.typeId == asTYPEID_DOUBLE && typeId == asTYPEID_INT64 )
            {
                *(asINT64*) value = asINT64( it->second.valueFlt );
                return true;
            }
        }
    }

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

bool ScriptDictionary::Exists( const ScriptString& key ) const
{
    map< string, valueStruct >::const_iterator it;
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

void ScriptDictionary::Delete( const ScriptString& key )
{
    map< string, valueStruct >::iterator it;
    it = dict.find( key.c_std_str() );
    if( it != dict.end() )
    {
        FreeValue( it->second );

        dict.erase( it );
    }
}

void ScriptDictionary::DeleteAll()
{
    map< string, valueStruct >::iterator it;
    for( it = dict.begin(); it != dict.end(); it++ )
        FreeValue( it->second );

    dict.clear();
}

asUINT ScriptDictionary::Keys( ScriptArray* keys )
{
    if( keys && !dict.empty() )
    {
        asUINT i = keys->GetSize();
        keys->Resize( i + (asUINT) dict.size() );

        map< string, valueStruct >::iterator it, end;
        for( it = dict.begin(), end = dict.end(); it != end; ++it )
        {
            ScriptString** p = (ScriptString**) keys->At( i );
            *p = new ScriptString( ( *it ).first );
            i++;
        }
    }
    return (unsigned int) dict.size();
}

void ScriptDictionary::FreeValue( valueStruct& value )
{
    // If it is a handle or a ref counted object, call release
    if( value.typeId & asTYPEID_MASK_OBJECT )
    {
        // Let the engine release the object
        engine->ReleaseScriptObject( value.valueObj, value.typeId );
        value.valueObj = 0;
        value.typeId = 0;
    }

    // For primitives, there's nothing to do
}

void ScriptDictionaryFactory_Generic( asIScriptGeneric* gen )
{
    *(ScriptDictionary**) gen->GetAddressOfReturnLocation() = new ScriptDictionary( gen->GetEngine() );
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

    r = engine->RegisterObjectType( "dictionary", sizeof( ScriptDictionary ), asOBJ_REF | asOBJ_GC );
    assert( r >= 0 );
    // Use the generic interface to construct the object since we need the engine pointer, we could also have retrieved the engine pointer from the active context
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_FACTORY, "dictionary@ f()", asFUNCTION( ScriptDictionaryFactory_Generic ), asCALL_GENERIC );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_ADDREF, "void f()", asMETHOD( ScriptDictionary, AddRef ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "dictionary", asBEHAVE_RELEASE, "void f()", asMETHOD( ScriptDictionary, Release ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "dictionary &opAssign(const dictionary &in)", asMETHODPR( ScriptDictionary, operator=, ( const ScriptDictionary & ), ScriptDictionary & ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "void set(const string &in, ?&in)", asMETHODPR( ScriptDictionary, Set, ( const ScriptString &, void*, int ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool get(const string &in, ?&out) const", asMETHODPR( ScriptDictionary, Get, ( const ScriptString &, void*, int ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "void set(const string &in, int64&in)", asMETHODPR( ScriptDictionary, Set, ( const ScriptString &, asINT64 & ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool get(const string &in, int64&out) const", asMETHODPR( ScriptDictionary, Get, ( const ScriptString &, asINT64 & ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "void set(const string &in, double&in)", asMETHODPR( ScriptDictionary, Set, ( const ScriptString &, double& ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool get(const string &in, double&out) const", asMETHODPR( ScriptDictionary, Get, ( const ScriptString &, double& ) const, bool ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "bool exists(const string &in) const", asMETHOD( ScriptDictionary, Exists ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "bool isEmpty() const", asMETHOD( ScriptDictionary, IsEmpty ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "uint getSize() const", asMETHOD( ScriptDictionary, GetSize ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "void delete(const string &in)", asMETHOD( ScriptDictionary, Delete ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "dictionary", "void deleteAll()", asMETHOD( ScriptDictionary, DeleteAll ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "dictionary", "uint keys(array<string@>@+) const", asMETHOD( ScriptDictionary, Keys ), asCALL_THISCALL );
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

    #if AS_USE_STLNAMES == 1
    // Same as isEmpty
    r = engine->RegisterObjectMethod( "dictionary", "bool empty() const", asMETHOD( ScriptDictionary, IsEmpty ), asCALL_THISCALL );
    assert( r >= 0 );
    // Same as getSize
    r = engine->RegisterObjectMethod( "dictionary", "uint size() const", asMETHOD( ScriptDictionary, GetSize ), asCALL_THISCALL );
    assert( r >= 0 );
    // Same as delete
    r = engine->RegisterObjectMethod( "dictionary", "void erase(const string &in)", asMETHOD( ScriptDictionary, Delete ), asCALL_THISCALL );
    assert( r >= 0 );
    // Same as deleteAll
    r = engine->RegisterObjectMethod( "dictionary", "void clear()", asMETHOD( ScriptDictionary, DeleteAll ), asCALL_THISCALL );
    assert( r >= 0 );
    #endif
}

void RegisterScriptDictionary_Generic( asIScriptEngine* engine )
{}
