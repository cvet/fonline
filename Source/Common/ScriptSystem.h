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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(ScriptSystemException);
FO_DECLARE_EXCEPTION(ScriptException);
FO_DECLARE_EXCEPTION(ScriptInitException);
FO_DECLARE_EXCEPTION(ScriptCallException);
FO_DECLARE_EXCEPTION(ScriptCompilerException);

// ReSharper disable CppInconsistentNaming
enum class ScriptEnum_uint8 : uint8
{
};
enum class ScriptEnum_uint16 : uint16
{
};
enum class ScriptEnum_int32 : int32
{
};
enum class ScriptEnum_uint32 : uint32
{
};
// ReSharper restore CppInconsistentNaming

using GameComponent = ScriptEnum_int32;
using PlayerComponent = ScriptEnum_int32;
using ItemComponent = ScriptEnum_int32;
using CritterComponent = ScriptEnum_int32;
using MapComponent = ScriptEnum_int32;
using LocationComponent = ScriptEnum_int32;
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

class BaseEngine;

class ProtoItem;
class ProtoCritter;
class ProtoMap;
class ProtoLocation;

using AbstractItem = Entity;
using ScriptSelfEntity = Entity;

template<typename... Args>
struct hstring_variadic : hstring
{
    // ReSharper disable once CppNonExplicitConvertingConstructor
    hstring_variadic(hstring s) :
        hstring(s)
    {
    }
};
template<typename... Args>
using ScriptFuncName = hstring_variadic<Args...>;

struct ScriptTypeInfo
{
    struct DataAccessor
    {
        virtual ~DataAccessor() = default;
        [[nodiscard]] virtual auto IsArray() const noexcept -> bool { return false; }
        [[nodiscard]] virtual auto IsReference() const noexcept -> bool { return false; }
        [[nodiscard]] virtual auto IsObject() const noexcept -> bool { return false; }
        [[nodiscard]] virtual auto IsEntity() const noexcept -> bool { return false; }
        [[nodiscard]] virtual auto IsPlainData() const noexcept -> bool { return false; }
        [[nodiscard]] virtual auto GetData(void* /*data*/) -> void* { throw InvalidCallException(FO_LINE_STR); }
        [[nodiscard]] virtual auto GetDataSize() -> size_t { throw InvalidCallException(FO_LINE_STR); }
        [[nodiscard]] virtual auto GetArraySize(void* /*data*/) -> size_t { throw InvalidCallException(FO_LINE_STR); }
        [[nodiscard]] virtual auto GetArrayElement(void* /*data*/, size_t /*index*/) -> void* { throw InvalidCallException(FO_LINE_STR); }
    };

    template<typename T>
    struct ArrayDataAccessor final : DataAccessor
    {
        [[nodiscard]] auto IsArray() const noexcept -> bool override { return true; }
        [[nodiscard]] auto GetArraySize(void* data) -> size_t override { return static_cast<vector<T>*>(data)->size(); }
        [[nodiscard]] auto GetArrayElement(void* data, size_t index) -> void* override { return static_cast<void*>(&(Element = static_cast<vector<T>*>(data)->operator[](index))); }
        T Element {}; // For supporting vector of bool
    };

    template<typename T>
    struct PlainDataAccessor final : DataAccessor
    {
        [[nodiscard]] auto IsPlainData() const noexcept -> bool override { return true; }
        [[nodiscard]] auto GetData(void* data) -> void* override { return static_cast<void*>(static_cast<T*>(data)); }
        [[nodiscard]] auto GetDataSize() -> size_t override { return sizeof(T); }
    };

    template<typename T>
    struct ReferenceDataAccessor final : DataAccessor
    {
        [[nodiscard]] auto IsReference() const noexcept -> bool override { return true; }
        [[nodiscard]] auto GetData(void* data) -> void* override { return static_cast<void*>(*static_cast<T**>(data)); }
    };

    template<typename T>
    struct ObjectDataAccessor final : DataAccessor
    {
        [[nodiscard]] auto IsObject() const noexcept -> bool override { return true; }
        [[nodiscard]] auto GetData(void* data) -> void* override { return static_cast<void*>(static_cast<T*>(data)); }
    };

    template<typename T>
    struct EntityDataAccessor final : DataAccessor
    {
        [[nodiscard]] auto IsEntity() const noexcept -> bool override { return true; }
        [[nodiscard]] auto GetData(void* data) -> void* override { return static_cast<void*>(*static_cast<T**>(data)); }
    };

    ScriptTypeInfo(string_view name_, shared_ptr<DataAccessor> accessor_) :
        Name {name_},
        Accessor {std::move(accessor_)}
    {
    }

    string Name {};
    shared_ptr<DataAccessor> Accessor {};
};

struct ScriptFuncDesc
{
    hstring Name {};
    string Declaration {};
    shared_ptr<ScriptTypeInfo> RetType {};
    vector<shared_ptr<ScriptTypeInfo>> ArgsType {};
    bool CallSupported {};
    function<bool(initializer_list<void*>, void*) /*noexcept*/> Call {};
    bool Delegate {};
};

template<typename TRet, typename... Args>
class ScriptFunc final
{
public:
    ScriptFunc() noexcept = default;
    explicit ScriptFunc(ScriptFuncDesc* f) noexcept :
        _func {f}
    {
    }
    [[nodiscard]] explicit operator bool() const noexcept { return _func != nullptr; }
    [[nodiscard]] auto GetName() const noexcept -> hstring { return _func != nullptr ? _func->Name : hstring(); }
    [[nodiscard]] auto IsDelegate() const noexcept -> bool { return _func != nullptr && _func->Delegate; }
    [[nodiscard]] auto GetResult() noexcept -> TRet { return _ret; }

    auto operator()(const Args&... args) noexcept -> bool { return _func != nullptr ? _func->Call({const_cast<void*>(static_cast<const void*>(&args))...}, &_ret) : false; }

private:
    raw_ptr<ScriptFuncDesc> _func {};
    TRet _ret {};
};

template<typename... Args>
class ScriptFunc<void, Args...> final
{
public:
    ScriptFunc() noexcept = default;
    explicit ScriptFunc(ScriptFuncDesc* f) noexcept :
        _func {f}
    {
    }
    [[nodiscard]] explicit operator bool() const noexcept { return _func != nullptr; }
    [[nodiscard]] auto GetName() const noexcept -> hstring { return _func != nullptr ? _func->Name : hstring(); }
    [[nodiscard]] auto IsDelegate() const noexcept -> bool { return _func != nullptr && _func->Delegate; }

    auto operator()(const Args&... args) noexcept -> bool { return _func != nullptr ? _func->Call({const_cast<void*>(static_cast<const void*>(&args))...}, nullptr) : false; }

private:
    raw_ptr<ScriptFuncDesc> _func {};
};

class ScriptSystemBackend
{
public:
    virtual ~ScriptSystemBackend() = default;
};

class ScriptSystem
{
public:
    ScriptSystem();
    ScriptSystem(const ScriptSystem&) = delete;
    ScriptSystem(ScriptSystem&&) noexcept = delete;
    auto operator=(const ScriptSystem&) = delete;
    auto operator=(ScriptSystem&&) noexcept = delete;
    virtual ~ScriptSystem() = default;

    void InitModules();
    void HandleRemoteCall(uint32 rpc_num, Entity* entity);
    void Process();

    void RegisterBackend(size_t index, shared_ptr<ScriptSystemBackend> backend);

    template<typename T>
    [[nodiscard]] FO_FORCE_INLINE auto GetBackend(size_t index) noexcept -> T*
    {
        static_assert(std::is_base_of_v<ScriptSystemBackend, T>);
        return static_cast<T*>(_backends[index].get());
    }

    template<typename TRet, typename... Args>
    [[nodiscard]] auto FindFunc(hstring func_name) noexcept -> ScriptFunc<TRet, Args...>
    {
        const auto range = _funcMap.equal_range(func_name);

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second, {std::type_index(typeid(Args))...}, std::type_index(typeid(TRet)))) {
                return ScriptFunc<TRet, Args...>(&it->second);
            }
        }

        return {};
    }

    [[nodiscard]] auto FindFunc(hstring func_name, initializer_list<std::type_index> args_type) noexcept -> ScriptFuncDesc*
    {
        const auto range = _funcMap.equal_range(func_name);

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second, args_type, std::type_index(typeid(void)))) {
                return &it->second;
            }
        }

        return nullptr;
    }

    template<typename TRet, typename... Args>
    [[nodiscard]] auto CheckFunc(hstring func_name) const noexcept -> bool
    {
        const auto range = _funcMap.equal_range(func_name);

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second, {std::type_index(typeid(Args))...}, std::type_index(typeid(TRet)))) {
                return true;
            }
        }

        return false;
    }

    template<typename TRet, typename... Args>
        requires(!std::is_void_v<TRet>)
    [[nodiscard]] auto CallFunc(hstring func_name, const Args&... args, TRet& ret) noexcept -> bool
    {
        auto func = FindFunc<TRet, Args...>(func_name);

        if (func && func(args...)) {
            ret = func.GetResult();
            return true;
        }

        return false;
    }

    template<typename TRet = void, typename... Args>
        requires(std::is_void_v<TRet>)
    [[nodiscard]] auto CallFunc(hstring func_name, const Args&... args) noexcept -> bool
    {
        auto func = FindFunc<void, Args...>(func_name);
        return func && func(args...);
    }

    [[nodiscard]] auto CallFunc(hstring func_name, initializer_list<std::type_index> args_type, initializer_list<void*> args) noexcept -> bool
    {
        const auto range = _funcMap.equal_range(func_name);

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second, args_type, std::type_index(typeid(void)))) {
                return it->second.Call(args, nullptr);
            }
        }

        return false;
    }

    [[nodiscard]] auto ValidateArgs(const ScriptFuncDesc& func_desc, initializer_list<std::type_index> args_type, std::type_index ret_type) const noexcept -> bool;

    [[nodiscard]] auto ResolveEngineType(std::type_index ti) const -> shared_ptr<ScriptTypeInfo>;
    [[nodiscard]] auto GetEngineTypeMap() const -> const unordered_map<string, shared_ptr<ScriptTypeInfo>>& { return _engineToScriptType; }

    template<typename T>
    void MapEnginePlainType(string_view type_name)
    {
        static_assert(!std::is_pointer_v<T>);
        auto raw_type = SafeAlloc::MakeShared<ScriptTypeInfo>(type_name, SafeAlloc::MakeShared<ScriptTypeInfo::PlainDataAccessor<T>>());
        _engineToScriptType.emplace(typeid(T).name(), raw_type);
        auto ref_type = SafeAlloc::MakeShared<ScriptTypeInfo>(type_name, SafeAlloc::MakeShared<ScriptTypeInfo::ReferenceDataAccessor<T>>());
        _engineToScriptType.emplace(typeid(T*).name(), ref_type);
        auto arr_type = SafeAlloc::MakeShared<ScriptTypeInfo>(type_name, SafeAlloc::MakeShared<ScriptTypeInfo::ArrayDataAccessor<T>>());
        _engineToScriptType.emplace(typeid(vector<T>).name(), arr_type);
    }

    template<typename T>
    void MapEngineObjectType(string_view type_name)
    {
        static_assert(!std::is_pointer_v<T>);
        auto obj_type = SafeAlloc::MakeShared<ScriptTypeInfo>(type_name, SafeAlloc::MakeShared<ScriptTypeInfo::ObjectDataAccessor<T>>());
        _engineToScriptType.emplace(typeid(T).name(), obj_type);
        auto ref_type = SafeAlloc::MakeShared<ScriptTypeInfo>(type_name, SafeAlloc::MakeShared<ScriptTypeInfo::ReferenceDataAccessor<T>>());
        _engineToScriptType.emplace(typeid(T*).name(), ref_type);
        auto arr_type = SafeAlloc::MakeShared<ScriptTypeInfo>(type_name, SafeAlloc::MakeShared<ScriptTypeInfo::ArrayDataAccessor<T>>());
        _engineToScriptType.emplace(typeid(vector<T>).name(), arr_type);
    }

    template<typename T, typename T2 = void>
    void MapEngineEntityType(string_view type_name)
    {
        static_assert(!std::is_pointer_v<T>);
        auto ent_type = SafeAlloc::MakeShared<ScriptTypeInfo>(type_name, SafeAlloc::MakeShared<ScriptTypeInfo::EntityDataAccessor<T>>());
        _engineToScriptType.emplace(typeid(T*).name(), ent_type);
        _engineToScriptType.emplace(typeid(T).name(), ent_type); // Access to entity by typeid(*entity)
        auto arr_type = SafeAlloc::MakeShared<ScriptTypeInfo>(type_name, SafeAlloc::MakeShared<ScriptTypeInfo::ArrayDataAccessor<T*>>());
        _engineToScriptType.emplace(typeid(vector<T*>).name(), arr_type);

        if constexpr (!std::same_as<T2, void>) {
            _engineToScriptType.emplace(typeid(T2*).name(), ent_type);
            _engineToScriptType.emplace(typeid(T2).name(), ent_type);
            _engineToScriptType.emplace(typeid(vector<T2*>).name(), arr_type);
        }
    }

    void AddLoopCallback(function<void()> callback) { _loopCallbacks.emplace_back(std::move(callback)); }
    auto AddScriptFunc(hstring name) -> ScriptFuncDesc* { return &_funcMap.emplace(name, ScriptFuncDesc())->second; }
    void AddInitFunc(ScriptFuncDesc* func, int32 priority);

    void BindRemoteCallReceiver(uint32 hash, function<void(Entity*)> func)
    {
        FO_RUNTIME_ASSERT(_rpcReceivers.count(hash) == 0);
        _rpcReceivers.emplace(hash, std::move(func));
    }

private:
    vector<shared_ptr<ScriptSystemBackend>> _backends {};
    unordered_map<string, shared_ptr<ScriptTypeInfo>> _engineToScriptType {};
    vector<function<void()>> _loopCallbacks {};
    unordered_multimap<hstring, ScriptFuncDesc> _funcMap {};
    vector<pair<raw_ptr<ScriptFuncDesc>, int32>> _initFunc {};
    unordered_map<uint32, function<void(Entity*)>> _rpcReceivers {};
    bool _nonConstHelper {};
};

class ScriptHelpers final
{
public:
    ScriptHelpers() = delete;

    template<typename T, typename U>
    [[nodiscard]] static auto GetIntConvertibleEntityProperty(const BaseEngine* engine, U prop_index) -> const Property*
    {
        return GetIntConvertibleEntityProperty(engine, T::ENTITY_TYPE_NAME, static_cast<int32>(prop_index));
    }

    [[nodiscard]] static auto GetIntConvertibleEntityProperty(const BaseEngine* engine, string_view type_name, int32 prop_index) -> const Property*;

    template<typename T>
    static auto CallInitScript(ScriptSystem& script_sys, T* entity, hstring init_script, bool first_time) -> bool
    {
        if (init_script) {
            if (auto init_func = script_sys.FindFunc<void, T*, bool>(init_script)) {
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

FO_END_NAMESPACE();
