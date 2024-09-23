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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// ReSharper disable CppInconsistentNaming
enum class ScriptEnum_uint8 : uint8
{
};
enum class ScriptEnum_uint16 : uint16
{
};
enum class ScriptEnum_int : int
{
};
enum class ScriptEnum_uint : uint
{
};
// ReSharper restore CppInconsistentNaming

using GameComponent = ScriptEnum_int;
using PlayerComponent = ScriptEnum_int;
using ItemComponent = ScriptEnum_int;
using CritterComponent = ScriptEnum_int;
using MapComponent = ScriptEnum_int;
using LocationComponent = ScriptEnum_int;
static_assert(sizeof(GameComponent) == sizeof(hstring::hash_t));

using GameProperty = ScriptEnum_uint16;
using PlayerProperty = ScriptEnum_uint16;
using ItemProperty = ScriptEnum_uint16;
using CritterProperty = ScriptEnum_uint16;
using MapProperty = ScriptEnum_uint16;
using LocationProperty = ScriptEnum_uint16;

template<typename T>
using InitFunc = hstring;
template<typename T>
using CallbackFunc = hstring;
template<typename T>
using PredicateFunc = hstring;
template<int N>
using ObjInfo = string_view;

class FOEngineBase;

class ProtoItem;
class ProtoCritter;
class ProtoMap;
class ProtoLocation;

using AbstractItem = Entity;

struct ScriptFuncDesc
{
    hstring Name {};
    string Declaration {};
    const type_info* RetType {};
    vector<const type_info*> ArgsType {};
    bool CallSupported {};
    std::function<bool(initializer_list<void*>, void*)> Call {};
    bool Delegate {};
};

template<typename TRet, typename... Args>
class ScriptFunc final
{
public:
    ScriptFunc() = default;
    explicit ScriptFunc(ScriptFuncDesc* f) :
        _func {f}
    {
    }
    [[nodiscard]] explicit operator bool() const { return _func != nullptr; }
    [[nodiscard]] auto GetName() const -> hstring { return _func != nullptr ? _func->Name : hstring(); }
    [[nodiscard]] auto IsDelegate() const -> bool { return _func != nullptr && _func->Delegate; }
    [[nodiscard]] auto GetResult() -> TRet { return _ret; }

    auto operator()(const Args&... args) -> bool { return _func != nullptr ? _func->Call({const_cast<void*>(static_cast<const void*>(&args))...}, &_ret) : false; }

private:
    ScriptFuncDesc* _func {};
    TRet _ret {};
};

template<typename... Args>
class ScriptFunc<void, Args...> final
{
public:
    ScriptFunc() = default;
    explicit ScriptFunc(ScriptFuncDesc* f) :
        _func {f}
    {
    }
    [[nodiscard]] explicit operator bool() const { return _func != nullptr; }
    [[nodiscard]] auto GetName() const -> hstring { return _func != nullptr ? _func->Name : hstring(); }
    [[nodiscard]] auto IsDelegate() const -> bool { return _func != nullptr && _func->Delegate; }

    auto operator()(const Args&... args) -> bool { return _func != nullptr ? _func->Call({const_cast<void*>(static_cast<const void*>(&args))...}, nullptr) : false; }

private:
    ScriptFuncDesc* _func {};
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

    virtual void InitSubsystems() { }
    void InitModules();
    void HandleRemoteCall(uint rpc_num, Entity* entity);
    void Process();

    template<typename TRet, typename... Args>
    [[nodiscard]] auto FindFunc(hstring func_name) -> ScriptFunc<TRet, Args...>
    {
        const auto range = _funcMap.equal_range(func_name);
        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second, {&typeid(Args)...}, &typeid(TRet))) {
                return ScriptFunc<TRet, Args...>(&it->second);
            }
        }
        return {};
    }

    template<typename TRet, typename... Args, std::enable_if_t<!std::is_void_v<TRet>, int> = 0>
    [[nodiscard]] auto CallFunc(hstring func_name, const Args&... args, TRet& ret) -> bool
    {
        auto func = FindFunc<TRet, Args...>(func_name);
        if (func && func(args...)) {
            ret = func.GetResult();
            return true;
        }
        return false;
    }

    template<typename TRet = void, typename... Args, std::enable_if_t<std::is_void_v<TRet>, int> = 0>
    [[nodiscard]] auto CallFunc(hstring func_name, const Args&... args) -> bool
    {
        auto func = FindFunc<void, Args...>(func_name);
        return func && func(args...);
    }

protected:
    [[nodiscard]] auto ValidateArgs(const ScriptFuncDesc& func_desc, initializer_list<const type_info*> args_type, const type_info* ret_type) -> bool;

    vector<std::function<void()>> _loopCallbacks {};
    std::unordered_multimap<hstring, ScriptFuncDesc> _funcMap {};
    vector<ScriptFuncDesc*> _initFunc {};
    unordered_map<uint, std::function<void(Entity*)>> _rpcReceivers {};
    bool _nonConstHelper {};
};

class ScriptHelpers final
{
public:
    ScriptHelpers() = delete;

    template<typename T, typename U>
    [[nodiscard]] static auto GetIntConvertibleEntityProperty(const FOEngineBase* engine, U prop_index) -> const Property*
    {
        return GetIntConvertibleEntityProperty(engine, T::ENTITY_TYPE_NAME, static_cast<int>(prop_index));
    }

    [[nodiscard]] static auto GetIntConvertibleEntityProperty(const FOEngineBase* engine, string_view type_name, int prop_index) -> const Property*;

    template<typename T>
    static auto CallInitScript(ScriptSystem* script_sys, T* entity, hstring init_script, bool first_time) -> bool
    {
        if (init_script) {
            if (auto&& init_func = script_sys->FindFunc<void, T*, bool>(init_script)) {
                if (!init_func(entity, first_time)) {
                    return false;
                }
            }
            else {
                return false;
            }
        }

        return true;
    }
};
