#ifndef SCRIPTTYPE_H
#define SCRIPTTYPE_H

#include "angelscript.h"
#include "scriptstring.h"

class ScriptType
{
public:
    ScriptString* GetName() const;
    ScriptString* GetNameWithoutNamespace() const;
    ScriptString* GetNamespace() const;
    bool          IsAssigned() const;
    bool          Equals( const ScriptType& other );
    bool          DerivesFrom( const ScriptType& other );
    void          Instanciate( void* out, int out_type_id ) const;
    void          InstanciateCopy( void* in, int in_type_id, void* out, int out_type_id ) const;
    ScriptString* ShowMembers();

    asIObjectType* ObjType;
};

class ScriptTypeOf: public ScriptType
{
public:
    ScriptTypeOf( asIObjectType* type );
    void       AddRef() const;
    void       Release() const;
    ScriptType ConvertToType() const;

protected:
    ~ScriptTypeOf();

    mutable int refCount;
};

void RegisterScriptReflection( asIScriptEngine* engine );

#endif
