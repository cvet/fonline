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

#include <angelscript.h>

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

[[noreturn]] void ThrowScriptCoreException(string_view file, int32 line, int32 result);
auto GetScriptBackend(BaseEngine* engine) -> AngelScriptBackend*;
auto GetScriptBackend(AngelScript::asIScriptEngine* as_engine) -> AngelScriptBackend*;
auto GetEngineMetadata(AngelScript::asIScriptEngine* as_engine) -> const EngineMetadata*;
auto GetGameEngine(AngelScript::asIScriptEngine* as_engine) -> BaseEngine*;
void CheckScriptEntityNonNull(const Entity* entity);
void CheckScriptEntityNonDestroyed(const Entity* entity);
auto MakeScriptTypeName(const BaseTypeDesc& type) -> string;
auto MakeScriptTypeName(const ComplexTypeDesc& type) -> string;
auto MakeScriptArgName(const ComplexTypeDesc& type) -> string;
auto MakeScriptArgsName(const_span<ArgDesc> args) -> string;
auto MakeScriptReturnName(const ComplexTypeDesc& type, bool pass_ownership = false) -> string;
auto MakeScriptPropertyName(const Property* prop) -> string;
auto NormalizeScriptPropertyDecl(string_view decl) -> string;
auto CreateScriptArray(AngelScript::asIScriptEngine* as_engine, const char* type) -> ScriptArray*;
auto CreateScriptDict(AngelScript::asIScriptEngine* as_engine, const char* type) -> ScriptDict*;
auto CalcConstructAddrSpace(const Property* prop) -> size_t;
void FreeConstructAddrSpace(const Property* prop, void* construct_addr);
void ConvertPropsToScriptObject(const Property* prop, PropertyRawData& prop_data, void* construct_addr, AngelScript::asIScriptEngine* as_engine);
auto ConvertScriptToPropsObject(const Property* prop, void* as_obj) -> PropertyRawData;
auto GetScriptObjectInfo(const void* ptr, int32 type_id) -> string;
auto GetScriptFuncName(const AngelScript::asIScriptFunction* func, HashResolver& hash_resolver) -> hstring;

#ifdef AS_MAX_PORTABILITY

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
                return *static_cast<BaseType*>(gen->GetArgAddress(arg_index));
            }
            else if constexpr (std::is_pointer_v<T>) {
                return static_cast<T>(gen->GetArgAddress(arg_index));
            }
            else {
                using BaseType = std::remove_cv_t<T>;
                return *static_cast<BaseType*>(gen->GetAddressOfArg(arg_index));
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
                return *static_cast<BaseType*>(gen->GetObject());
            }
            else if constexpr (std::is_pointer_v<T>) {
                return static_cast<T>(gen->GetObject());
            }
            else {
                using BaseType = std::remove_cv_t<T>;
                return *static_cast<BaseType*>(gen->GetObject());
            }
        }
    };

    template<typename T>
    static void StoreReturnValue(AngelScript::asIScriptGeneric* gen, T value)
    {
        if constexpr (std::is_lvalue_reference_v<T>) {
            int32 as_result = gen->SetReturnAddress(const_cast<void*>(static_cast<const void*>(std::addressof(value))));
            if (as_result < 0) {
                ThrowScriptCoreException(__FILE__, __LINE__, as_result);
            }
        }
        else if constexpr (std::is_pointer_v<T>) {
            int32 as_result = gen->SetReturnAddress(const_cast<void*>(reinterpret_cast<const void*>(value)));
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
            auto* object = static_cast<C*>(gen->GetObject());

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
            const auto* object = static_cast<const C*>(gen->GetObject());

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
            new (gen->GetObject()) T(ArgReader<Args>::read(gen, static_cast<int>(Index))...);
        }
    };

    template<typename T>
    void destroy(AngelScript::asIScriptGeneric* gen)
    {
        static_cast<T*>(gen->GetObject())->~T();
    }
}

#endif

FO_END_NAMESPACE

#endif
