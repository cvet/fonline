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

#include <angelscript.h>
#include <as_datatype.h>
#include <as_scriptengine.h>
// ReSharper disable CppRedundantQualifier

#include "WinApiUndef.inc" // Remove garbage from includes above

FO_BEGIN_NAMESPACE

static auto TryCastToEnumType(ptr<const AngelScript::asITypeInfo> ti) -> nptr<const AngelScript::asCEnumType>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type = ti.dyn_cast<const AngelScript::asCTypeInfo>();
    FO_VERIFY_AND_THROW(type, "Missing type descriptor");
    return CastToEnumType(const_cast<AngelScript::asCTypeInfo*>(std::addressof(*type)));
}

static auto GetTypeInfoById(ptr<AngelScript::asIScriptEngine> engine, int32_t typeId) -> nptr<AngelScript::asITypeInfo>
{
    FO_NO_STACK_TRACE_ENTRY();

    return engine->GetTypeInfoById(typeId);
}

static auto GetRefTypeInfoById(ptr<AngelScript::asIScriptEngine> engine, int32_t typeId) -> nptr<AngelScript::asITypeInfo>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type_info = GetTypeInfoById(engine, typeId);

    if (!type_info) {
        return nullptr;
    }

    if ((type_info->GetFlags() & AngelScript::asOBJ_REF) == 0) {
        return nullptr;
    }

    return type_info;
}

static auto DescribeTypeId(ptr<AngelScript::asIScriptEngine> engine, int32_t typeId) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    auto type_info = GetTypeInfoById(engine, typeId);
    const nptr<const char> decl = engine->GetTypeDeclaration(typeId, true);
    AngelScript::asQWORD type_flags = 0;

    if (type_info) {
        type_flags = type_info->GetFlags();
    }

    return strex("typeId={}, decl='{}', isHandle={}, isObj={}, flags=0x{:X}", typeId, decl ? decl.get() : "<null>", (typeId & AngelScript::asTYPEID_OBJHANDLE) != 0, (typeId & AngelScript::asTYPEID_MASK_OBJECT) != 0, type_flags);
}

static auto DescribeTypeInfo(ptr<AngelScript::asIScriptEngine> engine, nptr<AngelScript::asITypeInfo> type_info) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type_info) {
        return "<null>";
    }

    const nptr<const char> decl = engine->GetTypeDeclaration(type_info->GetTypeId(), true);
    return strex("typeId={}, decl='{}', flags=0x{:X}", type_info->GetTypeId(), decl ? decl.get() : "<null>", type_info->GetFlags());
}

ScriptType::ScriptType(ptr<AngelScript::asITypeInfo> ti) :
    _typeInfo {ti}
{
    FO_NO_STACK_TRACE_ENTRY();
}

void ScriptType::AddRef() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _refCount.fetch_add(1, std::memory_order_acq_rel);
}

void ScriptType::Release() const
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete this;
    }
}

auto ScriptType::GetName() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<const char> ns = _typeInfo->GetNamespace();
    const nptr<const char> name = _typeInfo->GetName();
    const string_view ns_view = ns ? string_view {ns.get()} : string_view {};
    const string_view name_view = name ? string_view {name.get()} : string_view {};

    if (!ns_view.empty()) {
        return string(ns_view).append("::").append(name_view);
    }

    return string(name_view);
}

auto ScriptType::GetNameWithoutNamespace() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<const char> name = _typeInfo->GetName();
    return name ? string {name.get()} : string {};
}

auto ScriptType::GetNamespace() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<const char> ns = _typeInfo->GetNamespace();
    return ns ? string {ns.get()} : string {};
}

auto ScriptType::GetModule() const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<AngelScript::asIScriptModule> module = _typeInfo->GetModule();
    if (!module) {
        return "(global)";
    }

    return module->GetName();
}

auto ScriptType::GetSize() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32_t>(_typeInfo->GetSize());
}

auto ScriptType::IsGlobal() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<AngelScript::asIScriptModule> module = _typeInfo->GetModule();
    return !module;
}

auto ScriptType::IsClass() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return !IsInterface() && !IsEnum() && !IsFunction();
}

auto ScriptType::IsInterface() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto object_type = _typeInfo.dyn_cast<AngelScript::asCObjectType>();
    FO_VERIFY_AND_THROW(object_type, "Script type is not an object type");
    return object_type->IsInterface();
}

auto ScriptType::IsEnum() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto enum_type = TryCastToEnumType(_typeInfo);

    if (!enum_type) {
        return false;
    }

    return enum_type->enumValues.GetLength() != 0;
}

auto ScriptType::IsFunction() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return strcmp(_typeInfo->GetName(), "_builtin_function_") == 0;
}

auto ScriptType::IsShared() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto object_type = _typeInfo.dyn_cast<AngelScript::asCObjectType>();
    FO_VERIFY_AND_THROW(object_type, "Script type is not an object type");
    return object_type->IsShared();
}

auto ScriptType::GetBaseType() const -> refcount_nptr<ScriptType>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> base = _typeInfo->GetBaseType();

    if (!base) {
        return nullptr;
    }

    return SafeAlloc::MakeRefCounted<ScriptType>(base);
}

auto ScriptType::GetInterfaceCount() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32_t>(_typeInfo->GetInterfaceCount());
}

auto ScriptType::GetInterface(int32_t index) const -> refcount_nptr<ScriptType>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index >= 0 && index < numeric_cast<int32_t>(_typeInfo->GetInterfaceCount())) {
        nptr<AngelScript::asITypeInfo> type_info = _typeInfo->GetInterface(index);
        FO_VERIFY_AND_THROW(type_info, "Missing interface type info");
        return SafeAlloc::MakeRefCounted<ScriptType>(type_info);
    }

    return nullptr;
}

auto ScriptType::Implements(const ScriptType* other) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<const ScriptType> other_ref = other;
    return other_ref && _typeInfo->Implements(other_ref->_typeInfo.get());
}

auto ScriptType::Equals(const ScriptType* other) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<const ScriptType> other_ref = other;
    return other_ref && _typeInfo == other_ref->_typeInfo;
}

auto ScriptType::DerivesFrom(const ScriptType* other) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<const ScriptType> other_ref = other;
    return other_ref && _typeInfo->DerivesFrom(other_ref->_typeInfo.get());
}

auto ScriptType::GetMethodsCount() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32_t>(_typeInfo->GetMethodCount());
}

auto ScriptType::GetMethodDeclaration(int32_t index, bool include_object_name, bool include_namespace, bool include_param_names) const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index >= numeric_cast<int32_t>(_typeInfo->GetMethodCount())) {
        return "";
    }

    return _typeInfo->GetMethodByIndex(index)->GetDeclaration(include_object_name, include_namespace, include_param_names);
}

auto ScriptType::GetPropertiesCount() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32_t>(_typeInfo->GetPropertyCount());
}

auto ScriptType::GetPropertyDeclaration(int32_t index, bool include_namespace) const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= numeric_cast<int32_t>(_typeInfo->GetPropertyCount())) {
        return "";
    }

    return _typeInfo->GetPropertyDeclaration(index, include_namespace);
}

auto ScriptType::GetEnumLength() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    auto enum_type = TryCastToEnumType(_typeInfo);

    if (!enum_type) {
        return 0;
    }

    return numeric_cast<int32_t>(enum_type->enumValues.GetLength());
}

auto ScriptType::GetEnumNames() const -> refcount_ptr<ScriptArray>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    auto enum_type = TryCastToEnumType(_typeInfo);
    ptr<AngelScript::asIScriptEngine> engine = ctx->GetEngine();
    nptr<AngelScript::asITypeInfo> array_type = engine->GetTypeInfoByDecl("string[]");
    FO_VERIFY_AND_THROW(array_type, "Missing string array type info");
    auto result = ScriptArray::Create(array_type);

    if (enum_type) {
        for (int32_t i = 0; i < numeric_cast<int32_t>(enum_type->enumValues.GetLength()); i++) {
            const string name = enum_type->enumValues[i]->name.AddressOf();
            result->InsertLast(make_nptr(&name).void_cast());
        }
    }

    return result;
}

auto ScriptType::GetEnumValues() const -> refcount_ptr<ScriptArray>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    auto enum_type = TryCastToEnumType(_typeInfo);
    ptr<AngelScript::asIScriptEngine> engine = ctx->GetEngine();
    nptr<AngelScript::asITypeInfo> array_type = engine->GetTypeInfoByDecl("int[]");
    FO_VERIFY_AND_THROW(array_type, "Missing int array type info");
    auto result = ScriptArray::Create(array_type);

    if (enum_type) {
        for (int32_t i = 0; i < numeric_cast<int32_t>(enum_type->enumValues.GetLength()); i++) {
            result->InsertLast(make_nptr(&enum_type->enumValues[i]->value).void_cast());
        }
    }

    return result;
}

void ScriptType::Instantiate(ptr<void*> out, int32_t out_type_id) const
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
    auto out_type_info = GetRefTypeInfoById(engine, out_type_id);

    if (!out_type_info) {
        throw ScriptException(strex("Invalid 'instance' argument, not an handle ({})", DescribeTypeId(engine, out_type_id)));
    }
    const nptr<void> out_object = *out;

    if (out_object) {
        throw ScriptException(strex("Invalid 'instance' argument, handle must be null ({})", DescribeTypeId(engine, out_type_id)));
    }
    if (!_typeInfo->DerivesFrom(out_type_info.get())) {
        throw ScriptException(strex("Invalid 'instance' argument, incompatible types (instance: {}, expected='{}')", DescribeTypeId(engine, out_type_id), _typeInfo->GetName()));
    }

    *out = engine->CreateScriptObject(_typeInfo.get());
}

void ScriptType::InstantiateCopy(ptr<void> in, int32_t in_type_id, ptr<void*> out, int32_t out_type_id) const
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
    auto out_type_info = GetRefTypeInfoById(engine, out_type_id);
    auto in_type_info = GetRefTypeInfoById(engine, in_type_id);

    if (!out_type_info) {
        throw ScriptException(strex("Invalid 'instance' argument, not an handle ({})", DescribeTypeId(engine, out_type_id)));
    }
    const nptr<void> out_object = *out;

    if (out_object) {
        throw ScriptException(strex("Invalid 'instance' argument, handle must be null ({})", DescribeTypeId(engine, out_type_id)));
    }
    if (!_typeInfo->DerivesFrom(out_type_info.get())) {
        throw ScriptException(strex("Invalid 'instance' argument, incompatible types (instance: {}, expected='{}')", DescribeTypeId(engine, out_type_id), _typeInfo->GetName()));
    }

    if (!in_type_info) {
        throw ScriptException(strex("Invalid 'copyFrom' argument, not an handle ({})", DescribeTypeId(engine, in_type_id)));
    }
    auto in_object = NativeDataProvider::ReadHandleSlot(in);

    if (!in_object) {
        throw ScriptException(strex("Invalid 'copyFrom' argument, handle must be not null ({})", DescribeTypeId(engine, in_type_id)));
    }

    auto in_obj = in_object.reinterpret_as<const AngelScript::asIScriptObject>();

    if (in_obj->GetObjectType() != _typeInfo.get()) {
        throw ScriptException(strex("Invalid 'copyFrom' argument, incompatible runtime type (copyFrom: {}, runtime: {}, expected='{}')", DescribeTypeId(engine, in_type_id), DescribeTypeInfo(engine, in_obj->GetObjectType()), _typeInfo->GetName()));
    }

    *out = engine->CreateScriptObjectCopy(in_object.get(), _typeInfo.get());
}

static auto ScriptTypeOfTemplateCallback(AngelScript::asITypeInfo* raw_ot, bool& dont_garbage_collect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(dont_garbage_collect);

    FO_VERIFY_AND_THROW(raw_ot != nullptr, "Template type info is null");
    auto ot = make_ptr(raw_ot);

    if (ot->GetSubTypeCount() != 1 || (ot->GetSubTypeId() & AngelScript::asTYPEID_MASK_SEQNBR) <= AngelScript::asTYPEID_DOUBLE) {
        return false;
    }

    return true;
}

static auto ScriptTypeOfFactory(AngelScript::asITypeInfo* raw_ot) -> ScriptTypeOf*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(raw_ot != nullptr, "Template type info is null");
    auto ot = make_ptr(raw_ot);
    nptr<AngelScript::asITypeInfo> sub_type = ot->GetSubType();
    FO_VERIFY_AND_THROW(sub_type, "Template sub type info is null");
    auto script_type = SafeAlloc::MakeRefCounted<ScriptTypeOf>(sub_type);
    return script_type.release_ownership();
}

static auto ScriptTypeOfFactory2(AngelScript::asITypeInfo* raw_ot, void* raw_ref) -> ScriptTypeOf*
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(raw_ot != nullptr, "Template type info is null");
    auto ot = make_ptr(raw_ot);
    nptr<AngelScript::asITypeInfo> sub_type = ot->GetSubType();
    FO_VERIFY_AND_THROW(sub_type, "Template sub type info is null");

    if (sub_type->GetTypeId() <= AngelScript::asTYPEID_DOUBLE) {
        auto script_type = SafeAlloc::MakeRefCounted<ScriptTypeOf>(nullptr);
        return script_type.release_ownership();
    }

    FO_VERIFY_AND_THROW(raw_ref != nullptr, "Reference handle pointer is null");
    auto ref = make_ptr(raw_ref);
    auto ref_object = NativeDataProvider::ReadHandleSlot(ref);
    FO_VERIFY_AND_THROW(ref_object, "Reference handle object is null");
    auto ref_obj = ref_object.reinterpret_as<const AngelScript::asIScriptObject>();
    auto script_type = SafeAlloc::MakeRefCounted<ScriptTypeOf>(ref_obj->GetObjectType());
    return script_type.release_ownership();
}

ScriptTypeOf::ScriptTypeOf(nptr<AngelScript::asITypeInfo> ti) :
    _typeInfo {ti}
{
    FO_NO_STACK_TRACE_ENTRY();
}

void ScriptTypeOf::AddRef() const
{
    FO_NO_STACK_TRACE_ENTRY();

    AngelScript::asAtomicInc(_refCount);
}

void ScriptTypeOf::Release() const
{
    FO_NO_STACK_TRACE_ENTRY();

    if (AngelScript::asAtomicDec(_refCount) == 0) {
        delete this;
    }
}

auto ScriptTypeOf::ConvertToType() -> refcount_nptr<ScriptType>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_typeInfo) {
        return nullptr;
    }

    return SafeAlloc::MakeRefCounted<ScriptType>(_typeInfo);
}

static auto CreateAngelScriptLoadedModules() -> refcount_ptr<ScriptArray>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    ptr<AngelScript::asIScriptEngine> engine = ctx->GetEngine();
    nptr<AngelScript::asITypeInfo> array_type = engine->GetTypeInfoByDecl("string[]");
    FO_VERIFY_AND_THROW(array_type, "Missing string array type info");
    auto modules = ScriptArray::Create(array_type);

    for (int32_t i = 0; i < numeric_cast<int32_t>(engine->GetModuleCount()); i++) {
        nptr<AngelScript::asIScriptModule> module = engine->GetModuleByIndex(i);
        FO_VERIFY_AND_THROW(module, "Missing script module at index");
        const string name = module->GetName();
        modules->InsertLast(make_nptr(&name).void_cast());
    }

    return modules;
}

static auto GetAngelScriptLoadedModules() -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto modules = CreateAngelScriptLoadedModules();
    return modules.release_ownership();
}

static auto GetAngelScriptModule(nptr<const char> name) -> nptr<AngelScript::asIScriptModule>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    if (name) {
        ptr<AngelScript::asIScriptEngine> engine = ctx->GetEngine();
        return engine->GetModule(name.get(), AngelScript::asGM_ONLY_IF_EXISTS);
    }
    else {
        nptr<AngelScript::asIScriptFunction> func = ctx->GetFunction(0);
        FO_VERIFY_AND_THROW(func, "Missing current script function");
        return func->GetModule();
    }
}

static auto GetCurrentModule() -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    auto module = GetAngelScriptModule(nullptr);
    FO_VERIFY_AND_THROW(module, "Missing current script module");

    return module->GetName();
}

static auto CreateEnumsInternal(bool global, nptr<const char> module_name) -> refcount_ptr<ScriptArray>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    ptr<AngelScript::asIScriptEngine> engine = ctx->GetEngine();
    nptr<AngelScript::asITypeInfo> array_type = engine->GetTypeInfoByDecl("reflection::type[]");
    FO_VERIFY_AND_THROW(array_type, "Missing reflection type array type info");
    auto enums = ScriptArray::Create(array_type);

    if (global) {
        const auto count = engine->GetEnumCount();

        for (AngelScript::asUINT i = 0; i < count; i++) {
            nptr<AngelScript::asITypeInfo> enum_type = engine->GetEnumByIndex(i);
            FO_VERIFY_AND_THROW(enum_type, "Missing global enum type at index");

            auto type = SafeAlloc::MakeRefCounted<ScriptType>(enum_type);
            auto value = make_ptr(type.get_pp()).reinterpret_as<void>();
            enums->InsertLast(value);
        }
    }
    else {
        auto module = GetAngelScriptModule(module_name);

        if (!module) {
            return enums;
        }

        const auto count = module->GetEnumCount();

        for (AngelScript::asUINT i = 0; i < count; i++) {
            nptr<AngelScript::asITypeInfo> enum_type = module->GetEnumByIndex(i);
            FO_VERIFY_AND_THROW(enum_type, "Missing module enum type at index");

            auto type = SafeAlloc::MakeRefCounted<ScriptType>(enum_type);
            auto value = make_ptr(type.get_pp()).reinterpret_as<void>();
            enums->InsertLast(value);
        }
    }

    return enums;
}

static auto GetGlobalEnums() -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto enums = CreateEnumsInternal(true, nullptr);
    return enums.release_ownership();
}

static auto GetEnums() -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto enums = CreateEnumsInternal(false, nullptr);
    return enums.release_ownership();
}

static auto GetEnumsModule(string module_name) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto module_name_cstr = make_ptr(module_name.c_str());
    auto enums = CreateEnumsInternal(false, module_name_cstr);
    return enums.release_ownership();
}

static auto GetCallstack(ScriptArray*& modules, ScriptArray*& names, ScriptArray*& lines, ScriptArray*& columns, bool include_object_name, bool include_namespace, bool include_param_names) -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");

    int32_t count = 0;
    const auto stack_size = ctx->GetCallstackSize();

    for (AngelScript::asUINT i = 0; i < stack_size; i++) {
        nptr<const AngelScript::asIScriptFunction> func = ctx->GetFunction(i);
        int32_t column;
        int32_t line = ctx->GetLineNumber(i, &column);

        if (func) {
            const string name = func->GetModuleName();
            modules->InsertLast(make_nptr(&name).void_cast());

            const bool include_ns = include_namespace && func->GetNamespace()[0] != 0;
            const string decl = func->GetDeclaration(include_object_name, include_ns, include_param_names);
            names->InsertLast(make_nptr(&decl).void_cast());

            lines->InsertLast(make_nptr(&line).void_cast());
            columns->InsertLast(make_nptr(&column).void_cast());

            count++;
        }
    }

    return count;
}

static auto ScriptType_GetBaseType(const ScriptType& type) -> ScriptType*
{
    FO_STACK_TRACE_ENTRY();

    auto base_type = type.GetBaseType();
    return base_type ? base_type.take_not_null().release_ownership() : nullptr;
}

static auto ScriptType_GetInterface(const ScriptType& type, int32_t index) -> ScriptType*
{
    FO_STACK_TRACE_ENTRY();

    auto interface_type = type.GetInterface(index);
    return interface_type ? interface_type.take_not_null().release_ownership() : nullptr;
}

static auto ScriptType_GetEnumNames(const ScriptType& type) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    auto enum_names = type.GetEnumNames();
    return enum_names.release_ownership();
}

static auto ScriptType_GetEnumValues(const ScriptType& type) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    auto enum_values = type.GetEnumValues();
    return enum_values.release_ownership();
}

static auto ScriptTypeOf_ConvertToType(ScriptTypeOf& type_of) -> ScriptType*
{
    FO_STACK_TRACE_ENTRY();

    refcount_nptr<ScriptType> type = type_of.ConvertToType();
    return type ? type.take_not_null().release_ownership() : nullptr;
}

static void RegisterTypeMethod(ptr<AngelScript::asIScriptEngine> engine, string_view declaration, const AngelScript::asSFuncPtr& func_pointer)
{
    FO_STACK_TRACE_ENTRY();

    const string declaration_str(declaration);

    int32_t as_result = 0;
    FO_AS_VERIFY(engine->RegisterObjectMethod("type", declaration_str.c_str(), func_pointer, FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(engine->RegisterObjectMethod("typeof<T>", declaration_str.c_str(), func_pointer, FO_SCRIPT_METHOD_CONV));
}

static void RegisterTypeFuncThisMethod(ptr<AngelScript::asIScriptEngine> engine, string_view declaration, const AngelScript::asSFuncPtr& func_pointer)
{
    FO_STACK_TRACE_ENTRY();

    const string declaration_str(declaration);

    int32_t as_result = 0;
    FO_AS_VERIFY(engine->RegisterObjectMethod("type", declaration_str.c_str(), func_pointer, FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(engine->RegisterObjectMethod("typeof<T>", declaration_str.c_str(), func_pointer, FO_SCRIPT_FUNC_THIS_CONV));
}

static auto GetGenericScriptTypeObject(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<ScriptType>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto object = cast_from_void<ScriptType*>(gen->GetObject());
    FO_STRONG_ASSERT(object, "Generic script type object is null");
    return object;
}

static auto GetGenericOutObjectSlot(ptr<AngelScript::asIScriptGeneric> gen, AngelScript::asUINT arg_index) noexcept -> ptr<void*>
{
    FO_NO_STACK_TRACE_ENTRY();

    return NativeDataProvider::GetHandleSlot(GetGenericArgAddress(gen, arg_index));
}

static void ScriptType_Instantiate_Generic(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptGeneric> generic = gen;
    auto self = GetGenericScriptTypeObject(generic);
    auto out = GetGenericOutObjectSlot(generic, 0);
    const auto out_type_id = generic->GetArgTypeId(0);

    self->Instantiate(out, out_type_id);
}

static void ScriptType_InstantiateCopy_Generic(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptGeneric> generic = gen;
    auto self = GetGenericScriptTypeObject(generic);
    auto in = GetGenericArgAddress(generic, 0);
    FO_VERIFY_AND_THROW(in, "Input object is null");
    const auto in_type_id = generic->GetArgTypeId(0);
    auto out = GetGenericOutObjectSlot(generic, 1);
    const auto out_type_id = generic->GetArgTypeId(1);

    self->InstantiateCopy(in, in_type_id, out, out_type_id);
}

void RegisterAngelScriptReflection(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    FO_AS_VERIFY(as_engine->SetDefaultNamespace("reflection"));
    auto restore_ns = scope_fail([&as_engine]() noexcept { (void)as_engine->SetDefaultNamespace(""); });

    FO_AS_VERIFY(as_engine->RegisterObjectType("type", sizeof(ScriptType), AngelScript::asOBJ_REF));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("type", AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_METHOD(ScriptType, AddRef), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("type", AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_METHOD(ScriptType, Release), FO_SCRIPT_METHOD_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectType("typeof<class T>", sizeof(ScriptTypeOf), AngelScript::asOBJ_REF | AngelScript::asOBJ_TEMPLATE));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", FO_SCRIPT_FUNC(ScriptTypeOfTemplateCallback), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_FACTORY, "typeof<T>@ f(int&in)", FO_SCRIPT_FUNC(ScriptTypeOfFactory), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_FACTORY, "typeof<T>@ f(int&in, const T&in)", FO_SCRIPT_FUNC(ScriptTypeOfFactory2), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_METHOD(ScriptTypeOf, AddRef), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("typeof<T>", AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_METHOD(ScriptTypeOf, Release), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("typeof<T>", "type@ opImplConv()", FO_SCRIPT_FUNC_THIS(ScriptTypeOf_ConvertToType), FO_SCRIPT_FUNC_THIS_CONV));

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
    RegisterTypeFuncThisMethod(as_engine, "type@ get_baseType() const", FO_SCRIPT_FUNC_THIS(ScriptType_GetBaseType));
    RegisterTypeMethod(as_engine, "int get_interfaceCount() const", FO_SCRIPT_METHOD(ScriptType, GetInterfaceCount));
    RegisterTypeFuncThisMethod(as_engine, "type@ getInterface(int index) const", FO_SCRIPT_FUNC_THIS(ScriptType_GetInterface));
    RegisterTypeMethod(as_engine, "bool implements(const type@+ other) const", FO_SCRIPT_METHOD(ScriptType, Implements));
    RegisterTypeMethod(as_engine, "bool opEquals(const type@+ other) const", FO_SCRIPT_METHOD(ScriptType, Equals));
    RegisterTypeMethod(as_engine, "bool derivesFrom(const type@+ other) const", FO_SCRIPT_METHOD(ScriptType, DerivesFrom));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("type", "void instantiate(?&out instance) const", FO_SCRIPT_GENERIC(ScriptType_Instantiate_Generic), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("typeof<T>", "void instantiate(?&out instance) const", FO_SCRIPT_GENERIC(ScriptType_Instantiate_Generic), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("type", "void instantiate(?&in copyFrom, ?&out instance) const", FO_SCRIPT_GENERIC(ScriptType_InstantiateCopy_Generic), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("typeof<T>", "void instantiate(?&in copyFrom, ?&out instance) const", FO_SCRIPT_GENERIC(ScriptType_InstantiateCopy_Generic), FO_SCRIPT_GENERIC_CONV));
    RegisterTypeMethod(as_engine, "int get_methodsCount() const", FO_SCRIPT_METHOD(ScriptType, GetMethodsCount));
    RegisterTypeMethod(as_engine, "string getMethodDeclaration(int index, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true) const", FO_SCRIPT_METHOD(ScriptType, GetMethodDeclaration));
    RegisterTypeMethod(as_engine, "int get_propertiesCount() const", FO_SCRIPT_METHOD(ScriptType, GetPropertiesCount));
    RegisterTypeMethod(as_engine, "string getPropertyDeclaration(int index, bool includeNamespace = false) const", FO_SCRIPT_METHOD(ScriptType, GetPropertyDeclaration));
    RegisterTypeMethod(as_engine, "int get_enumLength() const", FO_SCRIPT_METHOD(ScriptType, GetEnumLength));
    RegisterTypeFuncThisMethod(as_engine, "array<string>@ get_enumNames() const", FO_SCRIPT_FUNC_THIS(ScriptType_GetEnumNames));
    RegisterTypeFuncThisMethod(as_engine, "array<int>@ get_enumValues() const", FO_SCRIPT_FUNC_THIS(ScriptType_GetEnumValues));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("array<string>@ getLoadedModules()", FO_SCRIPT_FUNC(GetAngelScriptLoadedModules), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("string getCurrentModule()", FO_SCRIPT_FUNC(GetCurrentModule), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("array<type@>@ getGlobalEnums()", FO_SCRIPT_FUNC(GetGlobalEnums), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("array<type@>@ getEnums()", FO_SCRIPT_FUNC(GetEnums), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("array<type@>@ getEnums(string moduleName)", FO_SCRIPT_FUNC(GetEnumsModule), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("int getCallstack(string[]& modules, string[]& names, int[]& lines, int[]& columns, bool includeObjectName = false, bool includeNamespace = false, bool includeParamNames = true)", FO_SCRIPT_FUNC(GetCallstack), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->SetDefaultNamespace(""));
}

FO_END_NAMESPACE

#endif
