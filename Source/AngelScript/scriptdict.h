#ifndef SCRIPTDICT_H
#define SCRIPTDICT_H

#include "angelscript.h"
#include <map>

class ScriptDict
{
public:
    #ifdef FONLINE_DLL
    static ScriptDict& Create( const char* keyType, const char* valueType )
    {
        static asITypeInfo* ot = ASEngine->GetTypeInfoByDecl( std::string( "dict<" ).append( keyType ).append( "," ).append( valueType ).append( ">" ).c_str() );
        ScriptDict*         scriptDict = (ScriptDict*) ASEngine->CreateScriptObject( ot );
        return *scriptDict;
    }
protected:
    #endif

    #ifndef FONLINE_DLL
    // Factory functions
    static ScriptDict* Create( asITypeInfo* ot );
    static ScriptDict* Create( asITypeInfo* ot, void* listBuffer );
    #endif

public:
    // Memory management
    virtual void AddRef() const;
    virtual void Release() const;

    // Copy the contents of one dict to another (only if the types are the same)
    virtual ScriptDict& operator=( const ScriptDict& other );

    // Compare two dicts
    virtual bool operator==( const ScriptDict& ) const;

    // Dict manipulation
    virtual uint  GetSize() const;
    virtual bool  IsEmpty() const;
    virtual void  Set( void* key, void* value );
    virtual void  SetIfNotExist( void* key, void* value );
    virtual bool  Remove( void* key );
    virtual uint  RemoveValues( void* value );
    virtual void  Clear();
    virtual void* Get( void* key );
    virtual void* GetDefault( void* key, void* defaultValue );
    virtual void* GetKey( uint index );
    virtual void* GetValue( uint index );
    virtual bool  Exists( void* key ) const;
    virtual void  GetMap( std::vector< std::pair< void*, void* > >& data );

    // GC methods
    virtual int  GetRefCount();
    virtual void SetFlag();
    virtual bool GetFlag();
    virtual void EnumReferences( asIScriptEngine* engine );
    virtual void ReleaseAllHandles( asIScriptEngine* engine );

protected:
    mutable int  refCount;
    mutable bool gcFlag;
    asITypeInfo* objType;
    int          keyTypeId;
    int          valueTypeId;
    void*        dictMap;

    ScriptDict();
    ScriptDict( asITypeInfo* ot );
    ScriptDict( asITypeInfo* ot, void* initBuf );
    ScriptDict( const ScriptDict& other );
    virtual ~ScriptDict();
};

#ifndef FONLINE_DLL
void RegisterScriptDict( asIScriptEngine* engine );
#endif

#endif
