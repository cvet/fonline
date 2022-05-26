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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Entity.h"

DECLARE_EXCEPTION(ScriptSystemException);
DECLARE_EXCEPTION(ScriptException);
DECLARE_EXCEPTION(ScriptInitException);
DECLARE_EXCEPTION(EntityInitScriptException);

enum class ScriptEnum_uint8 : uchar
{
};
enum class ScriptEnum_uint16 : ushort
{
};
enum class ScriptEnum_int : int
{
};
enum class ScriptEnum_uint : uint
{
};

using GameProperty = ScriptEnum_uint16;
using PlayerProperty = ScriptEnum_uint16;
using ItemProperty = ScriptEnum_uint16;
using CritterProperty = ScriptEnum_uint16;
using MapProperty = ScriptEnum_uint16;
using LocationProperty = ScriptEnum_uint16;

template<typename T>
using InitFunc = hstring;
template<int N>
using ObjInfo = string_view;
template<typename...>
using ScriptFuncName = string_view;

class FOEngineBase;

class ProtoItem;
class ProtoCritter;
class ProtoMap;
class ProtoLocation;

using AbstractItem = Entity;
using AbstractCritter = Entity;
using AbstractMap = Entity;
using AbstractLocation = Entity;

struct GenericScriptFunc
{
    string Name {};
    string Declaration {};
    vector<const type_info*> ArgsType {};
    bool CallSupported {};
    const type_info* RetType {};
    std::function<bool(initializer_list<void*>, void*)> Call {};
};

template<typename TRet, typename... Args>
class ScriptFunc final
{
    friend class ScriptSystem;

public:
    ScriptFunc() = default;
    [[nodiscard]] explicit operator bool() const { return _func != nullptr; }
    [[nodiscard]] auto operator()(Args... args) -> bool { return _func != nullptr ? _func->Call({&args...}, &_ret) : false; }
    [[nodiscard]] auto GetResult() -> TRet { return _ret; }

private:
    explicit ScriptFunc(GenericScriptFunc* f) : _func {f} { }

    GenericScriptFunc* _func {};
    TRet _ret {};
};

template<typename... Args>
class ScriptFunc<void, Args...> final
{
    friend class ScriptSystem;

public:
    ScriptFunc() = default;
    [[nodiscard]] explicit operator bool() const { return _func != nullptr; }
    [[nodiscard]] auto operator()(Args... args) -> bool { return _func != nullptr ? _func->Call({&args...}, nullptr) : false; }

private:
    explicit ScriptFunc(GenericScriptFunc* f) : _func {f} { }

    GenericScriptFunc* _func {};
};

class ScriptSystem
{
public:
    ScriptSystem() = default;
    ScriptSystem(const ScriptSystem&) = delete;
    ScriptSystem(ScriptSystem&&) noexcept = delete;
    auto operator=(const ScriptSystem&) = delete;
    auto operator=(ScriptSystem&&) noexcept = delete;
    virtual ~ScriptSystem() = default;

    void InitModules();

    template<typename TRet, typename... Args>
    [[nodiscard]] auto FindFunc(string_view func_name) -> ScriptFunc<TRet, Args...>
    {
        const auto range = _funcMap.equal_range(string(func_name));
        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second, {&typeid(Args)...}, &typeid(TRet))) {
                return ScriptFunc<TRet, Args...>(&it->second);
            }
        }
        return {};
    }

    template<typename TRet, typename... Args, std::enable_if_t<!std::is_void_v<TRet>, int> = 0>
    [[nodiscard]] auto CallFunc(string_view func_name, Args... args, TRet& ret) -> bool
    {
        auto func = FindFunc<TRet, Args...>(func_name);
        if (func && func(args...)) {
            ret = func.GetResult();
            return true;
        }
        return false;
    }

    template<typename TRet = void, typename... Args, std::enable_if_t<std::is_void_v<TRet>, int> = 0>
    [[nodiscard]] auto CallFunc(string_view func_name, Args... args) -> bool
    {
        auto func = FindFunc<void, Args...>(func_name);
        return func && func(args...);
    }

    struct NativeImpl;
    shared_ptr<NativeImpl> NativeData {};
    struct AngelScriptImpl;
    shared_ptr<AngelScriptImpl> AngelScriptData {};
    struct MonoImpl;
    shared_ptr<MonoImpl> MonoData {};

protected:
    [[nodiscard]] auto ValidateArgs(const GenericScriptFunc& gen_func, initializer_list<const type_info*> args_type, const type_info* ret_type) -> bool;

    std::unordered_multimap<string, GenericScriptFunc> _funcMap {};
    vector<GenericScriptFunc*> _initFunc {};
    bool _nonConstHelper {};
};

class ScriptHelpers final
{
public:
    ScriptHelpers() = delete;

    template<typename T, typename U>
    [[nodiscard]] static auto GetIntConvertibleEntityProperty(const FOEngineBase* engine, U prop_index) -> const Property*
    {
        return GetIntConvertibleEntityProperty(engine, T::ENTITY_CLASS_NAME, static_cast<int>(prop_index));
    }

    [[nodiscard]] static auto GetIntConvertibleEntityProperty(const FOEngineBase* engine, string_view class_name, int prop_index) -> const Property*;

    template<typename T>
    static void CallInitScript(ScriptSystem* script_sys, T* entity, hstring init_script, bool first_time)
    {
        if (init_script) {
            if (auto&& init_func = script_sys->FindFunc<void, T*, bool>(init_script)) {
                if (!init_func(entity, first_time)) {
                    throw EntityInitScriptException("Init func call failed", init_script);
                }
            }
            else {
                throw EntityInitScriptException("Init func not found or has bad signature", init_script);
            }
        }
    }
};
