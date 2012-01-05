#ifndef SCRIPTDICTIONARY_H
#define SCRIPTDICTIONARY_H

#include "angelscript.h"
#include "scriptarray.h"
#include "scriptstring.h"
#include <string>
#include <map>

class ScriptDictionary
{
public:
    #ifdef FONLINE_DLL
    static ScriptDictionary& Create()
    {
        static int        typeId = ASEngine->GetTypeIdByDecl( "dictionary" );
        ScriptDictionary* scriptDictionary = (ScriptDictionary*) ASEngine->CreateScriptObject( typeId );
        return *scriptDictionary;
    }
protected:
    #endif

    ScriptDictionary();
    ScriptDictionary( const ScriptDictionary& );
    ScriptDictionary( asIScriptEngine* engine );

public:
    virtual void AddRef() const;
    virtual void Release() const;

    ScriptDictionary& operator=( const ScriptDictionary& other )
    {
        Assign( other );
        return *this;
    }
    virtual void Assign( const ScriptDictionary& other );

    // Sets/Gets a variable type value for a key
    virtual void Set( const ScriptString& key, void* value, int typeId );
    virtual bool Get( const ScriptString& key, void* value, int typeId ) const;

    // Sets/Gets an integer number value for a key
    virtual void Set( const ScriptString& key, asINT64& value );
    virtual bool Get( const ScriptString& key, asINT64& value ) const;

    // Sets/Gets a real number value for a key
    virtual void Set( const ScriptString& key, double& value );
    virtual bool Get( const ScriptString& key, double& value ) const;

    // Returns true if the key is set
    virtual bool Exists( const ScriptString& key ) const;

    // Deletes the key
    virtual void Delete( const ScriptString& key );

    // Deletes all keys
    virtual void DeleteAll();

    // Get all keys
    virtual unsigned int Keys( ScriptArray* keys );

    // Garbage collections behaviours
    virtual int  GetRefCount();
    virtual void SetGCFlag();
    virtual bool GetGCFlag();
    virtual void EnumReferences( asIScriptEngine* engine );
    virtual void ReleaseAllReferences( asIScriptEngine* engine );

protected:
    // The structure for holding the values
    struct valueStruct
    {
        union
        {
            asINT64 valueInt;
            double  valueFlt;
            void*   valueObj;
        };
        int typeId;
    };

    // We don't want anyone to call the destructor directly, it should be called through the Release method
    virtual ~ScriptDictionary();

    // Helper methods
    virtual void FreeValue( valueStruct& value );

    // Our properties
    asIScriptEngine*                     engine;
    mutable int                          refCount;
    std::map< std::string, valueStruct > dict;
};

#ifndef FONLINE_DLL
void RegisterScriptDictionary( asIScriptEngine* engine );
#endif

#endif
