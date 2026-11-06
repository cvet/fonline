// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "ScriptSystem.h"

namespace AngelScript
{
    class asIScriptEngine;
    class asITypeInfo;
}

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ScriptReflectionException);

class ScriptArray;

class ScriptType
{
public:
    explicit ScriptType(ptr<AngelScript::asITypeInfo> ti);

    [[nodiscard]] auto GetSize() const -> int;
    [[nodiscard]] auto IsGlobal() const -> bool;
    [[nodiscard]] auto IsClass() const -> bool;
    [[nodiscard]] auto IsInterface() const -> bool;
    [[nodiscard]] auto IsEnum() const -> bool;
    [[nodiscard]] auto IsFunction() const -> bool;
    [[nodiscard]] auto IsShared() const -> bool;
    [[nodiscard]] auto GetInterfaceCount() const -> int;
    [[nodiscard]] auto Equals(const ScriptType* other) const -> bool;
    [[nodiscard]] auto GetMethodsCount() const -> int;
    [[nodiscard]] auto GetPropertiesCount() const -> int;
    [[nodiscard]] auto GetEnumLength() const -> int;

    void AddRef() const;
    void Release() const;

    auto GetName() const -> string;
    auto GetNameWithoutNamespace() const -> string;
    auto GetNamespace() const -> string;
    auto GetModule() const -> string;
    auto GetBaseType() const -> refcount_nptr<ScriptType>;
    auto GetInterface(int index) const -> refcount_nptr<ScriptType>;
    auto Implements(const ScriptType* other) const -> bool;
    auto DerivesFrom(const ScriptType* other) const -> bool;
    auto GetMethodDeclaration(int index, bool include_object_name, bool include_namespace, bool include_param_names) const -> string;
    auto GetPropertyDeclaration(int index, bool include_namespace) const -> string;
    auto GetEnumNames() const -> refcount_ptr<ScriptArray>;
    auto GetEnumValues() const -> refcount_ptr<ScriptArray>;

    void Instantiate(ptr<void*> out, int out_type_id) const;
    void InstantiateCopy(ptr<void> in, int in_type_id, ptr<void*> out, int out_type_id) const;

protected:
    ptr<AngelScript::asITypeInfo> _typeInfo;
    mutable std::atomic<int32_t> _refCount {1};
};

class ScriptTypeOf
{
public:
    explicit ScriptTypeOf(nptr<AngelScript::asITypeInfo> ti);

    void AddRef() const;
    void Release() const;

    auto ConvertToType() -> refcount_nptr<ScriptType>;

private:
    nptr<AngelScript::asITypeInfo> _typeInfo {};
    mutable int _refCount {1};
};

void RegisterAngelScriptReflection(ptr<AngelScript::asIScriptEngine> as_engine);

FO_END_NAMESPACE

#endif
