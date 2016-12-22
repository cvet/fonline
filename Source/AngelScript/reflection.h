#ifndef SCRIPTTYPE_H
#define SCRIPTTYPE_H

#ifndef ANGELSCRIPT_H 
// Avoid having to inform include path if header is already include before
#include <angelscript.h>
#endif

#include <string>

BEGIN_AS_NAMESPACE

class CScriptArray;

class ScriptType
{
public:
    ScriptType(asITypeInfo* type);
    std::string GetName() const;
    std::string GetNameWithoutNamespace() const;
    std::string GetNamespace() const;
    std::string GetModule() const;
    asUINT GetSize() const;
    bool IsAssigned() const;
    bool IsGlobal() const;
    bool IsClass() const;
    bool IsInterface() const;
    bool IsEnum() const;
    bool IsFunction() const;
    bool IsShared() const;
    ScriptType GetBaseType() const;
    asUINT GetInterfaceCount() const;
    ScriptType GetInterface(asUINT index) const;
    bool Implements(const ScriptType& other) const;
    bool Equals(const ScriptType& other);
    bool DerivesFrom(const ScriptType& other);
    void Instantiate(void* out, int out_type_id) const;
    void InstantiateCopy(void* in, int in_type_id, void* out, int out_type_id) const;
    asUINT GetMethodsCount() const;
    std::string GetMethodDeclaration(asUINT index, bool include_object_name, bool include_namespace, bool include_param_names) const;
    asUINT GetPropertiesCount() const;
    std::string GetPropertyDeclaration(asUINT index, bool include_namespace) const;
    asUINT GetEnumLength() const;
    CScriptArray* GetEnumNames() const;
    CScriptArray* GetEnumValues() const;

    asITypeInfo* ObjType;
};

class ScriptTypeOf : public ScriptType
{
public:
    ScriptTypeOf(asITypeInfo* type);
    void       AddRef() const;
    void       Release() const;
    ScriptType ConvertToType() const;

protected:
    ~ScriptTypeOf();

    mutable int refCount;
};

void RegisterScriptReflection(asIScriptEngine* engine);
void RegisterScriptReflection_Native(asIScriptEngine* engine);
void RegisterScriptReflection_Generic(asIScriptEngine* engine);

END_AS_NAMESPACE

#endif
