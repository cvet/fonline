#ifndef SCRIPTTYPE_H
#define SCRIPTTYPE_H

#include "angelscript.h"
#include "scriptarray.h"
#include "scriptstring.h"

class ScriptType
{
public:
    ScriptType( asIObjectType* type );
    ScriptString* GetName() const;
    ScriptString* GetNameWithoutNamespace() const;
    ScriptString* GetNamespace() const;
    ScriptString* GetModule() const;
    uint          GetSize() const;
    bool          IsAssigned() const;
    bool          IsGlobal() const;
    bool          IsClass() const;
    bool          IsInterface() const;
    bool          IsEnum() const;
    bool          IsFunction() const;
    bool          IsShared() const;
    ScriptType    GetBaseType() const;
    uint          GetInterfaceCount() const;
    ScriptType    GetInterface( uint index ) const;
    bool          Implements( const ScriptType& other ) const;
    bool          Equals( const ScriptType& other );
    bool          DerivesFrom( const ScriptType& other );
    void          Instantiate( void* out, int out_type_id ) const;
    void          InstantiateCopy( void* in, int in_type_id, void* out, int out_type_id ) const;
    uint          GetMethodsCount() const;
    ScriptString* GetMethodDeclaration( uint index, bool include_object_name, bool include_namespace, bool include_param_names ) const;
    uint          GetPropertiesCount() const;
    ScriptString* GetPropertyDeclaration( uint index, bool include_namespace ) const;
    uint          GetEnumLength() const;
    ScriptArray*  GetEnumNames() const;
    ScriptArray*  GetEnumValues() const;

    asIObjectType* ObjType;

    bool           Enum;
    const char*    EnumName;
    const char*    EnumNamespace;
    const char*    EnumModule;
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
