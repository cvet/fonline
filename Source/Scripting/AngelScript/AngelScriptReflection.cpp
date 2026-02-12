//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "AngelScriptReflection.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptArray.h"
#include "AngelScriptHelpers.h"

#include <as_datatype.h>
#include <as_scriptengine.h>
// ReSharper disable CppRedundantQualifier

#include "WinApiUndef-Include.h" // Remove garbage from includes above

FO_BEGIN_NAMESPACE

static auto TryCastToEnumType(const AngelScript::asITypeInfo* ti) -> const AngelScript::asCEnumType*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* type = dynamic_cast<const AngelScript::asCTypeInfo*>(ti);
    FO_RUNTIME_ASSERT(type);
    return CastToEnumType(const_cast<AngelScript::asCTypeInfo*>(type));
}

static auto GetTypeInfoById(AngelScript::asIScriptEngine* engine, int32 typeId) -> AngelScript::asITypeInfo*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto dt = dynamic_cast<AngelScript::asCScriptEngine*>(engine)->GetDataTypeFromTypeId(typeId);

    if (dt.IsValid()) {
        return dt.GetTypeInfo();
    }

    return nullptr;
}

ScriptType::ScriptType(AngelScript::asITypeInfo* ti) :
    _typeInfo {ti}
{
    FO_NO_STACK_TRACE_ENTRY();
}

void ScriptType::AddRef() const
{
    FO_NO_STACK_TRACE_ENTRY();

    AngelScript::asAtomicInc(_refCount);
}

void ScriptType::Release() const
{
    FO_NO_STACK_TRACE_ENTRY();

    if (AngelScript::asAtomicDec(_refCount) == 0) {
        delete this;
    }
}

auto ScriptType::GetName() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const char* ns = _typeInfo->GetNamespace();

    if (ns[0] != 0) {
        return string(ns).append("::").append(_typeInfo->GetName());
    }

    return _typeInfo->GetName();
}

auto ScriptType::GetNameWithoutNamespace() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo->GetName();
}

auto ScriptType::GetNamespace() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo->GetNamespace();
}

auto ScriptType::GetModule() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo->GetModule() != nullptr ? _typeInfo->GetModule()->GetName() : "(global)";
}

auto ScriptType::GetSize() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32>(_typeInfo->GetSize());
}

auto ScriptType::IsGlobal() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo->GetModule() == nullptr;
}

auto ScriptType::IsClass() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !IsInterface() && !IsEnum() && !IsFunction();
}

auto ScriptType::IsInterface() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return dynamic_cast<const AngelScript::asCObjectType*>(_typeInfo.get())->IsInterface();
}

auto ScriptType::IsEnum() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* enum_type = TryCastToEnumType(_typeInfo.get());
    return enum_type != nullptr ? enum_type->enumValues.GetLength() != 0 : false;
}

auto ScriptType::IsFunction() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return strcmp(_typeInfo->GetName(), "_builtin_function_") == 0;
}

auto ScriptType::IsShared() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return dynamic_cast<const AngelScript::asCObjectType*>(_typeInfo.get())->IsShared();
}

auto ScriptType::GetBaseType() const -> ScriptType*
{
    FO_NO_STACK_TRACE_ENTRY();

    AngelScript::asITypeInfo* base = _typeInfo->GetBaseType();
    return base != nullptr ? SafeAlloc::MakeRefCounted<ScriptType>(base).release_ownership() : nullptr;
}

auto ScriptType::GetInterfaceCount() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32>(_typeInfo->GetInterfaceCount());
}

auto ScriptType::GetInterface(int32 index) const -> ScriptType*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index >= 0 && index < numeric_cast<int32>(_typeInfo->GetInterfaceCount())) {
        return SafeAlloc::MakeRefCounted<ScriptType>(_typeInfo->GetInterface(index)).release_ownership();
    }

    return nullptr;
}

auto ScriptType::Implements(const ScriptType* other) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return other != nullptr && _typeInfo->Implements(other->_typeInfo.get());
}

auto ScriptType::Equals(const ScriptType* other) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return other != nullptr && _typeInfo == other->_typeInfo;
}

auto ScriptType::DerivesFrom(const ScriptType* other) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return other != nullptr && _typeInfo->DerivesFrom(other->_typeInfo.get());
}

auto ScriptType::GetMethodsCount() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32>(_typeInfo->GetMethodCount());
}

auto ScriptType::GetMethodDeclaration(int32 index, bool include_object_name, bool include_namespace, bool include_param_names) const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index >= numeric_cast<int32>(_typeInfo->GetMethodCount())) {
        return "";
    }

    return _typeInfo->GetMethodByIndex(index)->GetDeclaration(include_object_name, include_namespace, include_param_names);
}

auto ScriptType::GetPropertiesCount() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32>(_typeInfo->GetPropertyCount());
}

auto ScriptType::GetPropertyDeclaration(int32 index, bool include_namespace) const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= numeric_cast<int32>(_typeInfo->GetPropertyCount())) {
        return "";
    }

    return _typeInfo->GetPropertyDeclaration(index, include_namespace);
}

auto ScriptType::GetEnumLength() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* enum_type = TryCastToEnumType(_typeInfo.get());
    return enum_type != nullptr ? numeric_cast<int32>(enum_type->enumValues.GetLength()) : 0;
}

auto ScriptType::GetEnumNames() const -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);

    const auto* enum_type = TryCastToEnumType(_typeInfo.get());
    ScriptArray* result = ScriptArray::Create(ctx->GetEngine()->GetTypeInfoByDecl("string[]"));

    if (enum_type != nullptr) {
        for (int32 i = 0; i < numeric_cast<int32>(enum_type->enumValues.GetLength()); i++) {
            const string name = enum_type->enumValues[i]->name.AddressOf();
            result->InsertLast(cast_to_void(&name));
        }
    }

    return result;
}

auto ScriptType::GetEnumValues() const -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);

    const auto* enum_type = TryCastToEnumType(_typeInfo.get());
    ScriptArray* result = ScriptArray::Create(ctx->GetEngine()->GetTypeInfoByDecl("int[]"));

    if (enum_type != nullptr) {
        for (int32 i = 0; i < numeric_cast<int32>(enum_type->enumValues.GetLength()); i++) {
            result->InsertLast(cast_to_void(&enum_type->enumValues[i]->value));
        }
    }

    return result;
}

void ScriptType::Instantiate(void* out, int32 out_type_id) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* engine = _typeInfo->GetEngine();

    if ((out_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        throw ScriptException("Invalid 'instance' argument, not an handle");
    }
    if (*static_cast<void**>(out) != nullptr) {
        throw ScriptException("Invalid 'instance' argument, handle must be null");
    }
    if (!_typeInfo->DerivesFrom(GetTypeInfoById(engine, out_type_id))) {
        throw ScriptException("Invalid 'instance' argument, incompatible types");
    }

    *static_cast<void**>(out) = engine->CreateScriptObject(_typeInfo.get());
}

void ScriptType::InstantiateCopy(void* in, int32 in_type_id, void* out, int32 out_type_id) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* engine = _typeInfo->GetEngine();

    if ((out_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        throw ScriptException("Invalid 'instance' argument, not an handle");
    }
    if (*static_cast<void**>(out) != nullptr) {
        throw ScriptException("Invalid 'instance' argument, handle must be null");
    }
    if (!_typeInfo->DerivesFrom(GetTypeInfoById(engine, out_type_id))) {
        throw ScriptException("Invalid 'instance' argument, incompatible types");
    }

    if ((in_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        throw ScriptException("Invalid 'copyFrom' argument, not an handle");
    }
    if (*static_cast<void**>(in) == nullptr) {
        throw ScriptException("Invalid 'copyFrom' argument, handle must be not null");
    }

    in = *static_cast<void**>(in);
    const auto* in_obj = cast_from_void<AngelScript::asIScriptObject*>(in);

    if (in_obj->GetObjectType() != _typeInfo) {
        throw ScriptException("Invalid 'copyFrom' argument, incompatible types");
    }

    *static_cast<void**>(out) = engine->CreateScriptObjectCopy(in, _typeInfo.get());
}

static auto ScriptTypeOfTemplateCallback(AngelScript::asITypeInfo* ot, bool& dont_garbage_collect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(dont_garbage_collect);

    if (ot->GetSubTypeCount() != 1 || (ot->GetSubTypeId() & AngelScript::asTYPEID_MASK_SEQNBR) <= AngelScript::asTYPEID_DOUBLE) {
        return false;
    }

    return true;
}

static auto ScriptTypeOfFactory(AngelScript::asITypeInfo* ot) -> ScriptTypeOf*
{
    FO_NO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptTypeOf>(ot->GetSubType()).release_ownership();
}

static auto ScriptTypeOfFactory2(AngelScript::asITypeInfo* ot, void* ref) -> ScriptTypeOf*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (ot->GetSubType()->GetTypeId() <= AngelScript::asTYPEID_DOUBLE) {
        return SafeAlloc::MakeRefCounted<ScriptTypeOf>(nullptr).release_ownership();
    }

    ref = *static_cast<void**>(ref);
    const auto* ref_obj = cast_from_void<AngelScript::asIScriptObject*>(ref);
    return SafeAlloc::MakeRefCounted<ScriptTypeOf>(ref_obj->GetObjectType()).release_ownership();
}

ScriptTypeOf::ScriptTypeOf(AngelScript::asITypeInfo* ti) :
    ScriptType(ti)
{
    FO_NO_STACK_TRACE_ENTRY();
}

auto ScriptTypeOf::ConvertToType() -> ScriptType*
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo != nullptr ? SafeAlloc::MakeRefCounted<ScriptType>(_typeInfo.get()).release_ownership() : nullptr;
}

static auto GetAngelScriptLoadedModules() -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);

    const auto* engine = ctx->GetEngine();
    auto* modules = ScriptArray::Create(engine->GetTypeInfoByDecl("string[]"));

    for (int32 i = 0; i < numeric_cast<int32>(engine->GetModuleCount()); i++) {
        const string name = engine->GetModuleByIndex(i)->GetName();
        modules->InsertLast(cast_to_void(&name));
    }

    return modules;
}

static auto GetAngelScriptModule(const char* name) -> AngelScript::asIScriptModule*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);

    if (name != nullptr) {
        return ctx->GetEngine()->GetModule(name, AngelScript::asGM_ONLY_IF_EXISTS);
    }
    else {
        return ctx->GetFunction(0)->GetModule();
    }
}

static auto GetCurrentModule() -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetAngelScriptModule(nullptr)->GetName();
}

static auto GetEnumsInternal(bool global, const char* module_name) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);

    const auto* engine = ctx->GetEngine();
    auto* enums = ScriptArray::Create(engine->GetTypeInfoByDecl("reflection::type[]"));

    const AngelScript::asIScriptModule* module = nullptr;

    if (!global) {
        module = GetAngelScriptModule(module_name);

        if (module == nullptr) {
            return enums;
        }
    }

    const auto count = global ? engine->GetEnumCount() : module->GetEnumCount();

    for (AngelScript::asUINT i = 0; i < count; i++) {
        AngelScript::asITypeInfo* enum_type;

        if (global) {
            enum_type = engine->GetEnumByIndex(i);
        }
        else {
            enum_type = module->GetEnumByIndex(i);
        }

        auto type = SafeAlloc::MakeRefCounted<ScriptType>(enum_type);
        enums->InsertLast(cast_to_void(type.get_pp()));
    }

    return enums;
}

static auto GetGlobalEnums() -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetEnumsInternal(true, nullptr);
}

static auto GetEnums() -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetEnumsInternal(false, nullptr);
}

static auto GetEnumsModule(string module_name) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    return GetEnumsInternal(false, module_name.c_str());
}

static auto GetCallstack(ScriptArray*& modules, ScriptArray*& names, ScriptArray*& lines, ScriptArray*& columns, bool include_object_name, bool include_namespace, bool include_param_names) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);

    int32 count = 0;
    const auto stack_size = ctx->GetCallstackSize();

    for (AngelScript::asUINT i = 0; i < stack_size; i++) {
        const auto* func = ctx->GetFunction(i);
        int32 column;
        int32 line = ctx->GetLineNumber(i, &column);

        if (func != nullptr) {
            const string name = func->GetModuleName();
            modules->InsertLast(cast_to_void(&name));

            const bool include_ns = include_namespace && func->GetNamespace()[0] != 0;
            const string decl = func->GetDeclaration(include_object_name, include_ns, include_param_names);
            names->InsertLast(cast_to_void(&decl));

            lines->InsertLast(cast_to_void(&line));
            columns->InsertLast(cast_to_void(&column));

            count++;
        }
    }

    return count;
}

static void RegisterTypeMethod(AngelScript::asIScriptEngine* engine, const char* declaration, const AngelScript::asSFuncPtr& func_pointer)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    FO_AS_VERIFY(engine->RegisterObjectMethod("type", declaration, func_pointer, FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(engine->RegisterObjectMethod("typeof<T>", declaration, func_pointer, FO_SCRIPT_METHOD_CONV));
}

void RegisterAngelScriptReflection(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    FO_AS_VERIFY(as_engine->SetDefaultNamespace("reflection"));

    FO_AS_VERIFY(as_engine->RegisterObjectType("type", sizeof(ScriptType), AngelScript::asOBJ_REF));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("type", AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_METHOD(ScriptType, AddRef), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("type", AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_METHOD(ScriptType, Release), FO_SCRIPT_METHOD_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectType("typeof<class T>", sizeof(ScriptTypeOf), AngelScript::asOBJ_REF | AngelScript::asOBJ_TEMPLATE));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", FO_SCRIPT_FUNC(ScriptTypeOfTemplateCallback), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_FACTORY, "typeof<T>@ f(int&in)", FO_SCRIPT_FUNC(ScriptTypeOfFactory), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_FACTORY, "typeof<T>@ f(int&in, const T&in)", FO_SCRIPT_FUNC(ScriptTypeOfFactory2), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_METHOD(ScriptTypeOf, AddRef), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_METHOD(ScriptTypeOf, Release), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("typeof<T>", "type@ opImplConv()", FO_SCRIPT_METHOD(ScriptTypeOf, ConvertToType), FO_SCRIPT_METHOD_CONV));

    RegisterTypeMethod(as_engine, "string get_name() const", FO_SCRIPT_METHOD(ScriptType, GetName));
    RegisterTypeMethod(as_engine, "string get_nameWithoutNamespace() const", FO_SCRIPT_METHOD(ScriptType, GetNameWithoutNamespace));
    RegisterTypeMethod(as_engine, "string get_namespace() const", FO_SCRIPT_METHOD(ScriptType, GetNamespace));
    RegisterTypeMethod(as_engine, "string get_module() const", FO_SCRIPT_METHOD(ScriptType, GetModule));
    RegisterTypeMethod(as_engine, "int get_size() const", FO_SCRIPT_METHOD(ScriptType, GetSize));
    RegisterTypeMethod(as_engine, "bool get_isGlobal() const", FO_SCRIPT_METHOD(ScriptType, IsGlobal));
    RegisterTypeMethod(as_engine, "bool get_isClass() const", FO_SCRIPT_METHOD(ScriptType, IsClass));
    RegisterTypeMethod(as_engine, "bool get_isInterface() const", FO_SCRIPT_METHOD(ScriptType, IsInterface));
    RegisterTypeMethod(as_engine, "bool get_isEnum() const", FO_SCRIPT_METHOD(ScriptType, IsEnum));
    RegisterTypeMethod(as_engine, "bool get_isFunction() const", FO_SCRIPT_METHOD(ScriptType, IsFunction));
    RegisterTypeMethod(as_engine, "bool get_isShared() const", FO_SCRIPT_METHOD(ScriptType, IsShared));
    RegisterTypeMethod(as_engine, "type@ get_baseType() const", FO_SCRIPT_METHOD(ScriptType, GetBaseType));
    RegisterTypeMethod(as_engine, "int get_interfaceCount() const", FO_SCRIPT_METHOD(ScriptType, GetInterfaceCount));
    RegisterTypeMethod(as_engine, "type@ getInterface(int index) const", FO_SCRIPT_METHOD(ScriptType, GetInterface));
    RegisterTypeMethod(as_engine, "bool implements(const type@+ other) const", FO_SCRIPT_METHOD(ScriptType, Implements));
    RegisterTypeMethod(as_engine, "bool opEquals(const type@+ other) const", FO_SCRIPT_METHOD(ScriptType, Equals));
    RegisterTypeMethod(as_engine, "bool derivesFrom(const type@+ other) const", FO_SCRIPT_METHOD(ScriptType, DerivesFrom));
    RegisterTypeMethod(as_engine, "void instantiate(?&out instance) const", FO_SCRIPT_METHOD(ScriptType, Instantiate));
    RegisterTypeMethod(as_engine, "void instantiate(?&in copyFrom, ?&out instance) const", FO_SCRIPT_METHOD(ScriptType, InstantiateCopy));
    RegisterTypeMethod(as_engine, "int get_methodsCount() const", FO_SCRIPT_METHOD(ScriptType, GetMethodsCount));
    RegisterTypeMethod(as_engine, "string getMethodDeclaration(int index, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true) const", FO_SCRIPT_METHOD(ScriptType, GetMethodDeclaration));
    RegisterTypeMethod(as_engine, "int get_propertiesCount() const", FO_SCRIPT_METHOD(ScriptType, GetPropertiesCount));
    RegisterTypeMethod(as_engine, "string getPropertyDeclaration(int index, bool includeNamespace = false) const", FO_SCRIPT_METHOD(ScriptType, GetPropertyDeclaration));
    RegisterTypeMethod(as_engine, "int get_enumLength() const", FO_SCRIPT_METHOD(ScriptType, GetEnumLength));
    RegisterTypeMethod(as_engine, "string[]@ get_enumNames() const", FO_SCRIPT_METHOD(ScriptType, GetEnumNames));
    RegisterTypeMethod(as_engine, "int[]@ get_enumValues() const", FO_SCRIPT_METHOD(ScriptType, GetEnumValues));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("string[]@ getLoadedModules()", FO_SCRIPT_FUNC(GetAngelScriptLoadedModules), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("string getCurrentModule()", FO_SCRIPT_FUNC(GetCurrentModule), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("type@[]@ getGlobalEnums()", FO_SCRIPT_FUNC(GetGlobalEnums), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("type@[]@ getEnums()", FO_SCRIPT_FUNC(GetEnums), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("type@[]@ getEnums(string moduleName)", FO_SCRIPT_FUNC(GetEnumsModule), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("int getCallstack(string[]& modules, string[]& names, int[]& lines, int[]& columns, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true)", FO_SCRIPT_FUNC(GetCallstack), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->SetDefaultNamespace(""));
}

FO_END_NAMESPACE

#endif
