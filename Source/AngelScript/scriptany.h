#ifndef SCRIPTANY_H
#define SCRIPTANY_H

#include "angelscript.h"

class ScriptAny
{
public:
    #ifdef FONLINE_DLL
    static ScriptAny& Create()
    {
        static int typeId = ASEngine->GetTypeIdByDecl( "any" );
        ScriptAny* scriptAny = (ScriptAny*) ASEngine->CreateScriptObject( typeId );
        return *scriptAny;
    }
protected:
    #endif

    // Constructors
    ScriptAny();
    ScriptAny( const ScriptAny& );
    ScriptAny( asIScriptEngine* engine );
    ScriptAny( void* ref, int refTypeId, asIScriptEngine* engine );

public:
    // Memory management
    virtual void AddRef() const;
    virtual void Release() const;

    // Copy the stored value from another any object
    ScriptAny& operator=( const ScriptAny& other )
    {
        Assign( other );
        return *this;
    }
    virtual void Assign( const ScriptAny& other );
    virtual int  CopyFrom( const ScriptAny* other );

    // Store the value, either as variable type, integer number, or real number
    virtual void Store( void* ref, int refTypeId );
    virtual void Store( asINT64& value );
    virtual void Store( double& value );

    // Retrieve the stored value, either as variable type, integer number, or real number
    virtual bool Retrieve( void* ref, int refTypeId ) const;
    virtual bool Retrieve( asINT64& value ) const;
    virtual bool Retrieve( double& value ) const;

    // Get the type id of the stored value
    virtual int GetTypeId() const;

    // GC methods
    virtual int  GetRefCount();
    virtual void SetFlag();
    virtual bool GetFlag();
    virtual void EnumReferences( asIScriptEngine* engine );
    virtual void ReleaseAllHandles( asIScriptEngine* engine );

protected:
    virtual ~ScriptAny();
    virtual void FreeObject();

    mutable int      refCount;
    asIScriptEngine* engine;

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

    valueStruct value;
};

#ifndef FONLINE_DLL
void RegisterScriptAny( asIScriptEngine* engine );
#endif

#endif
