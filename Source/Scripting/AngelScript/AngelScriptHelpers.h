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

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptArray.h"
#include "AngelScriptDict.h"
#include "Entity.h"
#include "Properties.h"
#include "ScriptSystem.h"

namespace AngelScript
{
    class asIScriptContext;
    class asIScriptEngine;
    class asIScriptFunction;
    class asIScriptGeneric;
}

FO_BEGIN_NAMESPACE

#define FO_AS_VERIFY(expr) \
    as_result = expr; \
    if (as_result < 0) { \
        ThrowScriptCoreException(__FILE__, __LINE__, as_result); \
    }

#ifdef AS_MAX_PORTABILITY
#define FO_SCRIPT_GENERIC(name) AngelScript::asFUNCTION(name)
#define FO_SCRIPT_FUNC(name) FO_AS_WRAP_FN(name)
#define FO_SCRIPT_FUNC_EXT(name, params, ret) FO_AS_WRAP_FN_PR(name, params, ret)
#define FO_SCRIPT_FUNC_THIS(name) FO_AS_WRAP_OBJ_FIRST(name)
#define FO_SCRIPT_METHOD(type, name) FO_AS_WRAP_MFN(type, name)
#define FO_SCRIPT_METHOD_EXT(type, name, params, ret) FO_AS_WRAP_MFN_PR(type, name, params, ret)
#define FO_SCRIPT_GENERIC_CONV AngelScript::asCALL_GENERIC
#define FO_SCRIPT_FUNC_CONV AngelScript::asCALL_GENERIC
#define FO_SCRIPT_FUNC_THIS_CONV AngelScript::asCALL_GENERIC
#define FO_SCRIPT_METHOD_CONV AngelScript::asCALL_GENERIC
#else
#define FO_SCRIPT_GENERIC(name) AngelScript::asFUNCTION(name)
#define FO_SCRIPT_FUNC(name) AngelScript::asFUNCTION(name)
#define FO_SCRIPT_FUNC_EXT(name, params, ret) AngelScript::asFUNCTIONPR(name, params, ret)
#define FO_SCRIPT_FUNC_THIS(name) AngelScript::asFUNCTION(name)
#define FO_SCRIPT_METHOD(type, name) AngelScript::asMETHOD(type, name)
#define FO_SCRIPT_METHOD_EXT(type, name, params, ret) AngelScript::asMETHODPR(type, name, params, ret)
#define FO_SCRIPT_GENERIC_CONV AngelScript::asCALL_GENERIC
#define FO_SCRIPT_FUNC_CONV AngelScript::asCALL_CDECL
#define FO_SCRIPT_FUNC_THIS_CONV AngelScript::asCALL_CDECL_OBJFIRST
#define FO_SCRIPT_METHOD_CONV AngelScript::asCALL_THISCALL
#endif

class BaseEngine;
class AngelScriptBackend;
using ScriptFastCompareFunc = int32_t (*)(ptr<const void> a, ptr<const void> b);

[[noreturn]] void ThrowScriptCoreException(string_view file, int32_t line, int32_t result);
auto GetScriptBackend(ptr<BaseEngine> engine) -> ptr<AngelScriptBackend>;
auto GetScriptBackend(ptr<AngelScript::asIScriptEngine> as_engine) -> ptr<AngelScriptBackend>;
auto GetEngineMetadata(ptr<AngelScript::asIScriptEngine> as_engine) -> ptr<const EngineMetadata>;
auto GetGameEngine(ptr<AngelScript::asIScriptEngine> as_engine) -> ptr<BaseEngine>;
void CheckScriptEntityNonNull(nptr<const Entity> entity);
void CheckScriptEntityNonDestroyed(nptr<const Entity> entity);
void CheckScriptEntityAccessAndNonDestroyed(nptr<const Entity> entity);

auto MakeScriptTypeName(const BaseTypeDesc& type) -> string;
auto MakeScriptTypeName(const ComplexTypeDesc& type) -> string;
auto MakeScriptArgName(const ComplexTypeDesc& type, bool nullable = false) -> string;
auto MakeScriptArgsName(const_span<ArgDesc> args) -> string;
auto MakeScriptReturnName(const ComplexTypeDesc& type, bool pass_ownership = false, bool nullable = false) -> string;
auto MakeScriptPropertyName(ptr<const Property> prop) -> string;
auto NormalizeScriptPropertyDecl(string_view decl) -> string;
auto CreateScriptArray(ptr<AngelScript::asIScriptEngine> as_engine, string_view type) -> refcount_ptr<ScriptArray>;
auto CreateScriptDict(ptr<AngelScript::asIScriptEngine> as_engine, string_view type) -> refcount_ptr<ScriptDict>;
auto CalcConstructAddrSpace(ptr<const Property> prop) -> size_t;
void FreeConstructAddrSpace(ptr<const Property> prop, ptr<void> construct_addr);
void ConvertPropsToScriptObject(ptr<const Property> prop, PropertyRawData& prop_data, ptr<void> construct_addr, ptr<AngelScript::asIScriptEngine> as_engine);
auto ConvertScriptToPropsObject(ptr<const Property> prop, ptr<void> as_obj) -> PropertyRawData;
auto GetScriptObjectInfo(ptr<const void> script_obj, int32_t type_id) -> string;
auto ReadEnumValueAsInt32(ptr<const void> ptr, const BaseTypeDesc& enum_type) -> int32_t;
void WriteEnumValueFromInt32(ptr<void> ptr, const BaseTypeDesc& enum_type, int32_t value);
auto GetScriptFuncName(ptr<const AngelScript::asIScriptFunction> func, HashResolver& hash_resolver) -> hstring;
auto IsScriptNamespaceAllowed(string_view ns, const vector<string>& allowed_namespaces) noexcept -> bool;
auto CreateRefTypeScriptObjectFromRawData(const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> refcount_ptr<DynamicRefTypeInstance>;
auto ConvertRefTypeScriptObjectToRawData(const BaseTypeDesc& base_type, nptr<void> as_obj) -> vector<uint8_t>;
auto CreateRefTypeScriptObjectFromProperty(ptr<const Property> prop, span<const uint8_t> raw_data) -> refcount_ptr<DynamicRefTypeInstance>;
auto ConvertRefTypeScriptObjectToProperty(ptr<const Property> prop, nptr<void> as_obj) -> PropertyRawData;
void SetScriptTypeFastCompare(ptr<AngelScript::asITypeInfo> type, ScriptFastCompareFunc func);
auto GetScriptTypeFastCompare(ptr<const AngelScript::asITypeInfo> type) -> ScriptFastCompareFunc;

[[nodiscard]] auto GetGenericObject(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<void>;
[[nodiscard]] auto GetGenericAuxiliary(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<void>;
// GetArgAddress returns the argument value, which for a handle/pointer arg may legitimately be null, so it is nullable.
[[nodiscard]] auto GetGenericArgAddress(ptr<AngelScript::asIScriptGeneric> gen, uint32_t arg_index) noexcept -> nptr<void>;
// GetAddressOfArg returns the address of the argument's storage slot, which always exists, so it is non-null.
[[nodiscard]] auto GetGenericAddressArg(ptr<AngelScript::asIScriptGeneric> gen, uint32_t arg_index) noexcept -> ptr<void>;
[[nodiscard]] auto GetNullableHandleSlotAddress(ptr<nptr<void>> slot) noexcept -> ptr<void>;
[[nodiscard]] auto GetContextAddressOfArg(ptr<AngelScript::asIScriptContext> ctx, uint32_t arg_index) noexcept -> ptr<void>;
[[nodiscard]] auto GetContextAddressOfReturnValue(ptr<AngelScript::asIScriptContext> ctx) noexcept -> ptr<void>;
[[nodiscard]] auto MakeAngelScriptFuncDescBorrow(ptr<ScriptFuncDesc> func_desc, refcount_ptr<AngelScript::asIScriptFunction> func_lifetime) -> unique_del_ptr<ScriptFuncDesc>;
void ReturnGenericEntity(ptr<AngelScript::asIScriptGeneric> gen, nptr<Entity> entity) noexcept;
void ReturnGenericScriptArray(ptr<AngelScript::asIScriptGeneric> gen, ptr<ScriptArray> arr) noexcept;
void ReturnGenericScriptArray(ptr<AngelScript::asIScriptGeneric> gen, refcount_ptr<ScriptArray>&& arr) noexcept;
void SetScriptObjectFromHandleSlot(ptr<AngelScript::asIScriptContext> ctx, ptr<void> slot);
void SetScriptArgObjectFromHandleSlot(ptr<AngelScript::asIScriptContext> ctx, uint32_t arg_index, ptr<void> slot);

template<typename T>
[[nodiscard]] inline auto GetGenericObjectAs(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto object = GetGenericObject(gen);

    if constexpr (std::is_void_v<std::remove_cv_t<T>>) {
        return object;
    }
    else {
        return cast_from_void<T*>(object.get());
    }
}

template<typename T>
[[nodiscard]] inline auto GetGenericAuxiliaryAs(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    return cast_from_void<T*>(GetGenericAuxiliary(gen).get());
}

template<typename T>
[[nodiscard]] inline auto GetGenericAddressArgAs(ptr<AngelScript::asIScriptGeneric> gen, uint32_t arg_index) noexcept -> ptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto arg_address = GetGenericAddressArg(gen, arg_index);

    if constexpr (std::is_void_v<std::remove_cv_t<T>>) {
        return arg_address;
    }
    else {
        return cast_from_void<T*>(arg_address.get());
    }
}

template<typename T>
[[nodiscard]] inline auto GetGenericArgAddressAs(ptr<AngelScript::asIScriptGeneric> gen, uint32_t arg_index) noexcept -> nptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto arg_address = GetGenericArgAddress(gen, arg_index);

    if constexpr (std::is_void_v<std::remove_cv_t<T>>) {
        return arg_address;
    }
    else {
        return cast_from_void<T*>(arg_address.get());
    }
}

template<typename T>
[[nodiscard]] inline auto GenericValueAs(ptr<const void> value) noexcept -> ptr<const T>
{
    FO_NO_STACK_TRACE_ENTRY();

    return value.reinterpret_as<T>();
}

#ifdef AS_MAX_PORTABILITY

FO_END_NAMESPACE
#include <angelscript.h>
FO_BEGIN_NAMESPACE

#define FO_AS_WRAP_FN(name) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::FunctionWrapper<name>::f))
#define FO_AS_WRAP_MFN(ClassType, name) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::MethodWrapper<&ClassType::name>::f))
#define FO_AS_WRAP_OBJ_FIRST(name) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::ObjFirstWrapper<name>::f))
#define FO_AS_WRAP_OBJ_LAST(name) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::ObjLastWrapper<name>::f))
#define FO_AS_WRAP_FN_PR(name, Parameters, ReturnType) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::FunctionWrapper<static_cast<ReturnType(*) Parameters>(name)>::f))
#define FO_AS_WRAP_MFN_PR(ClassType, name, Parameters, ReturnType) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::MethodWrapper<AS_METHOD_AMBIGUITY_CAST(ReturnType(ClassType::*) Parameters)(&ClassType::name)>::f))
#define FO_AS_WRAP_OBJ_FIRST_PR(name, Parameters, ReturnType) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::ObjFirstWrapper<static_cast<ReturnType(*) Parameters>(name)>::f))
#define FO_AS_WRAP_OBJ_LAST_PR(name, Parameters, ReturnType) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::ObjLastWrapper<static_cast<ReturnType(*) Parameters>(name)>::f))
#define FO_AS_WRAP_CON(ClassType, Parameters) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::ConstructorWrapper<ClassType Parameters>::f))
#define FO_AS_WRAP_DES(ClassType) AngelScript::asFUNCTION((FO_NAMESPACE aswrap::destroy<ClassType>))

namespace aswrap
{
    template<typename T>
    struct ArgReader
    {
        static auto read(AngelScript::asIScriptGeneric* gen, int index) -> T
        {
            const auto arg_index = static_cast<AngelScript::asUINT>(index);

            if constexpr (std::is_lvalue_reference_v<T>) {
                using BaseType = std::remove_reference_t<T>;
                return *GetGenericArgAddressAs<BaseType>(gen, arg_index);
            }
            else if constexpr (std::is_pointer_v<T>) {
                if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
                    static_assert(sizeof(T) == sizeof(void*));
                    return std::bit_cast<T>(GetGenericArgAddress(gen, arg_index).get());
                }
                else if constexpr (std::is_void_v<remove_all_pointers_t<T>>) {
                    return GetGenericArgAddress(gen, arg_index).get();
                }
                else {
                    return GetGenericArgAddressAs<std::remove_pointer_t<T>>(gen, arg_index).get();
                }
            }
            else {
                using BaseType = std::remove_cv_t<T>;
                return *GetGenericAddressArgAs<BaseType>(gen, arg_index);
            }
        }
    };

    template<typename T>
    struct ObjectReader
    {
        static auto read(AngelScript::asIScriptGeneric* gen) -> T
        {
            if constexpr (std::is_lvalue_reference_v<T>) {
                using BaseType = std::remove_reference_t<T>;
                return *GetGenericObjectAs<BaseType>(gen);
            }
            else if constexpr (std::is_pointer_v<T>) {
                auto object = GetGenericObject(gen);

                if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
                    static_assert(sizeof(T) == sizeof(void*));
                    return std::bit_cast<T>(object.get());
                }
                else if constexpr (std::is_void_v<remove_all_pointers_t<T>>) {
                    return object.get();
                }
                else {
                    return cast_from_void<T>(object.get()).get();
                }
            }
            else {
                using BaseType = std::remove_cv_t<T>;
                return *GetGenericObjectAs<BaseType>(gen);
            }
        }
    };

    template<typename T>
    static auto PointerReturnValueAsAddress(T value) noexcept -> void*
    {
        FO_NO_STACK_TRACE_ENTRY();

        if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
            static_assert(sizeof(T) == sizeof(void*));
            return std::bit_cast<void*>(value);
        }
        else if constexpr (std::is_void_v<remove_all_pointers_t<T>>) {
            return const_cast<void*>(static_cast<const void*>(value));
        }
        else {
            return make_nptr(value).void_cast();
        }
    }

    template<typename T>
    static auto ReferenceReturnValueAsAddress(T& value) noexcept -> void*
    {
        FO_NO_STACK_TRACE_ENTRY();

        if constexpr (std::is_function_v<T>) {
            static_assert(sizeof(T*) == sizeof(void*));
            return std::bit_cast<void*>(std::addressof(value));
        }
        else {
            return make_nptr(std::addressof(value)).void_cast();
        }
    }

    template<typename T>
    static void StoreReturnValue(AngelScript::asIScriptGeneric* gen, T value)
    {
        if constexpr (std::is_lvalue_reference_v<T>) {
            const auto as_result = gen->SetReturnAddress(ReferenceReturnValueAsAddress(value));
            if (as_result < 0) {
                ThrowScriptCoreException(__FILE__, __LINE__, as_result);
            }
        }
        else if constexpr (std::is_pointer_v<T>) {
            const auto as_result = gen->SetReturnAddress(PointerReturnValueAsAddress(value));
            if (as_result < 0) {
                ThrowScriptCoreException(__FILE__, __LINE__, as_result);
            }
        }
        else {
            using BaseType = std::remove_cv_t<T>;
            new (gen->GetAddressOfReturnLocation()) BaseType(value);
        }
    }

    template<auto Func>
    struct FunctionWrapper;

    template<typename R, typename... Args, R (*Func)(Args...)>
    struct FunctionWrapper<Func>
    {
        static void f(AngelScript::asIScriptGeneric* gen) { invoke(gen, std::index_sequence_for<Args...> {}); }

    private:
        template<std::size_t... Index>
        static void invoke(AngelScript::asIScriptGeneric* gen, std::index_sequence<Index...>)
        {
            if constexpr (std::is_void_v<R>) {
                Func(ArgReader<Args>::read(gen, static_cast<int>(Index))...);
            }
            else {
                StoreReturnValue<R>(gen, Func(ArgReader<Args>::read(gen, static_cast<int>(Index))...));
            }
        }
    };

    template<typename MethodType, MethodType Method>
    struct MethodWrapperImpl;

    template<auto Method>
    struct MethodWrapper : MethodWrapperImpl<decltype(Method), Method>
    {
    };

    template<typename C, typename R, typename... Args, R (C::*Method)(Args...)>
    struct MethodWrapperImpl<R (C::*)(Args...), Method>
    {
        static void f(AngelScript::asIScriptGeneric* gen) { invoke(gen, std::index_sequence_for<Args...> {}); }

    private:
        template<std::size_t... Index>
        static void invoke(AngelScript::asIScriptGeneric* gen, std::index_sequence<Index...>)
        {
            auto object = GetGenericObjectAs<C>(gen);

            if constexpr (std::is_void_v<R>) {
                (object->*Method)(ArgReader<Args>::read(gen, static_cast<int>(Index))...);
            }
            else {
                StoreReturnValue<R>(gen, (object->*Method)(ArgReader<Args>::read(gen, static_cast<int>(Index))...));
            }
        }
    };

    template<typename C, typename R, typename... Args, R (C::*Method)(Args...) const>
    struct MethodWrapperImpl<R (C::*)(Args...) const, Method>
    {
        static void f(AngelScript::asIScriptGeneric* gen) { invoke(gen, std::index_sequence_for<Args...> {}); }

    private:
        template<std::size_t... Index>
        static void invoke(AngelScript::asIScriptGeneric* gen, std::index_sequence<Index...>)
        {
            auto object = GetGenericObjectAs<const C>(gen);

            if constexpr (std::is_void_v<R>) {
                (object->*Method)(ArgReader<Args>::read(gen, static_cast<int>(Index))...);
            }
            else {
                StoreReturnValue<R>(gen, (object->*Method)(ArgReader<Args>::read(gen, static_cast<int>(Index))...));
            }
        }
    };

    template<auto Func>
    struct ObjFirstWrapper;

    template<typename R, typename T, typename... Args, R (*Func)(T, Args...)>
    struct ObjFirstWrapper<Func>
    {
        static void f(AngelScript::asIScriptGeneric* gen) { invoke(gen, std::index_sequence_for<Args...> {}); }

    private:
        template<std::size_t... Index>
        static void invoke(AngelScript::asIScriptGeneric* gen, std::index_sequence<Index...>)
        {
            if constexpr (std::is_void_v<R>) {
                Func(ObjectReader<T>::read(gen), ArgReader<Args>::read(gen, static_cast<int>(Index))...);
            }
            else {
                StoreReturnValue<R>(gen, Func(ObjectReader<T>::read(gen), ArgReader<Args>::read(gen, static_cast<int>(Index))...));
            }
        }
    };

    template<auto Func>
    struct ObjLastWrapper;

    template<typename R, typename... Args, R (*Func)(Args...)>
    struct ObjLastWrapper<Func>
    {
        static_assert(sizeof...(Args) != 0);

        using ArgsTuple = std::tuple<Args...>;
        using ObjectArg = std::tuple_element_t<sizeof...(Args) - 1, ArgsTuple>;

        static void f(AngelScript::asIScriptGeneric* gen) { invoke(gen, std::make_index_sequence<sizeof...(Args) - 1> {}); }

    private:
        template<std::size_t... Index>
        static void invoke(AngelScript::asIScriptGeneric* gen, std::index_sequence<Index...>)
        {
            if constexpr (std::is_void_v<R>) {
                Func(ArgReader<std::tuple_element_t<Index, ArgsTuple>>::read(gen, static_cast<int>(Index))..., ObjectReader<ObjectArg>::read(gen));
            }
            else {
                StoreReturnValue<R>(gen, Func(ArgReader<std::tuple_element_t<Index, ArgsTuple>>::read(gen, static_cast<int>(Index))..., ObjectReader<ObjectArg>::read(gen)));
            }
        }
    };

    template<typename Signature>
    struct ConstructorWrapper;

    template<typename T, typename... Args>
    struct ConstructorWrapper<T(Args...)>
    {
        static void f(AngelScript::asIScriptGeneric* gen) { invoke(gen, std::index_sequence_for<Args...> {}); }

    private:
        template<std::size_t... Index>
        static void invoke(AngelScript::asIScriptGeneric* gen, std::index_sequence<Index...>)
        {
            new (GetGenericObjectAs<T>(gen).get()) T(ArgReader<Args>::read(gen, static_cast<int>(Index))...);
        }
    };

    template<typename T>
    void destroy(AngelScript::asIScriptGeneric* gen)
    {
        GetGenericObjectAs<T>(gen)->~T();
    }
}

#endif

FO_END_NAMESPACE

#endif
