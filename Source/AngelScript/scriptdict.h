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
        static asIObjectType* ot = ASEngine->GetObjectTypeByDecl( std::string( "dict<" ).append( keyType ).append( "," ).append( valueType ).append( ">" ).c_str() );
        ScriptDict*           scriptDict = (ScriptDict*) ASEngine->CreateScriptObject( ot );
        return *scriptDict;
    }
protected:
    #endif

    #ifndef FONLINE_DLL
    // Factory functions
    static ScriptDict* Create( asIObjectType* ot );
    static ScriptDict* Create( asIObjectType* ot, void* listBuffer );
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
    virtual void* Insert( void* key, void* value );
    virtual bool  Remove( void* key );
    virtual uint  RemoveValues( void* value );
    virtual void  Clear();
    virtual void* Find( void* key );
    virtual void* FindDefault( void* key, void* defaultValue );
    virtual bool  Contains( void* key ) const;
	virtual void* GetMap();

    // GC methods
    virtual int  GetRefCount();
    virtual void SetFlag();
    virtual bool GetFlag();
    virtual void EnumReferences( asIScriptEngine* engine );
    virtual void ReleaseAllHandles( asIScriptEngine* engine );

protected:
    mutable int    refCount;
    mutable bool   gcFlag;
    asIObjectType* objType;
    int            keyTypeId;
    int            valueTypeId;
    void*          dictMap;

    ScriptDict();
    ScriptDict( asIObjectType* ot );
    ScriptDict( asIObjectType* ot, void* initBuf );
    ScriptDict( const ScriptDict& other );
    virtual ~ScriptDict();
};

#ifndef FONLINE_DLL
void RegisterScriptDict( asIScriptEngine* engine );
#endif

#endif
