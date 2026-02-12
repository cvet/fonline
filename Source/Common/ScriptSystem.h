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

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ScriptSystemException);
FO_DECLARE_EXCEPTION(ScriptException);
FO_DECLARE_EXCEPTION(ScriptCoreException);
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

class FileSystem;
class Property;

class EngineMetadata;
class BaseEngine;

class Entity;
using AbstractItem = Entity;
using ScriptSelfEntity = Entity;

class ScriptSystem;

template<typename T>
using readonly_vector = const vector<T>&;
template<typename K, typename V>
using readonly_map = const map<K, V>&;

struct ScriptFuncDesc;

struct DataAccessor
{
    [[nodiscard]] virtual auto GetBackendIndex() const noexcept -> int32 = 0;
    [[nodiscard]] virtual auto GetArraySize(void* /*data*/) const -> size_t { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto GetArrayElement(void* /*data*/, size_t /*index*/) const -> void* { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto GetDictSize(void* /*data*/) const -> size_t { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto GetDictElement(void* /*data*/, size_t /*index*/) const -> pair<void*, void*> { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto GetCallback(void* /*data*/) const -> unique_del_ptr<ScriptFuncDesc> { throw InvalidCallException(FO_LINE_STR); }
    virtual void ClearArray(void* /*data*/) const { throw InvalidCallException(FO_LINE_STR); }
    virtual void AddArrayElement(void* /*data*/, void* /*value*/) const { throw InvalidCallException(FO_LINE_STR); }
    virtual void ClearDict(void* /*data*/) const { throw InvalidCallException(FO_LINE_STR); }
    virtual void AddDictElement(void* /*data*/, void* /*key*/, void* /*value*/) const { throw InvalidCallException(FO_LINE_STR); }
    virtual ~DataAccessor() = default;
};

struct FuncCallData
{
    raw_ptr<const DataAccessor> Accessor;
    const_span<void*> ArgsData {};
    void* RetData {};
};

namespace NativeDataProvider
{
    class ArrayDataProxy
    {
    public:
        // Mutable array
        template<typename T>
            requires(vector_collection<T>)
        explicit ArrayDataProxy(T& cont) :
            _ptrs {vec_transform(cont, [](auto&& e) -> void* { return cast_to_void(&e); })}
        {
            _clearCallback = [&]() FO_DEFERRED { cont.clear(); };
            _addCallback = [&](void* value) FO_DEFERRED { cont.emplace_back(*cast_from_void<typename T::value_type*>(value)); };
        }

        // Const array
        template<typename T>
            requires(vector_collection<T>)
        explicit ArrayDataProxy(const T& cont) :
            _ptrs {vec_transform(cont, [](auto&& e) -> void* { return cast_to_void(&e); })}
        {
            _clearCallback = [&]() FO_DEFERRED { throw InvalidCallException(FO_LINE_STR); };
            _addCallback = [&](void* /*value*/) FO_DEFERRED { throw InvalidCallException(FO_LINE_STR); };
        }

        auto Size() const noexcept -> size_t { return _ptrs.size(); }
        auto Get(size_t index) const noexcept -> void* { return _ptrs[index]; }
        void Clear() { _ptrs.clear(), _clearCallback(); }
        void Add(void* value) { _addCallback(value), _ptrs.emplace_back(value); }

    private:
        vector<void*> _ptrs;
        function<void()> _clearCallback {};
        function<void(void*)> _addCallback {};
    };

    class DictDataProxy
    {
    public:
        // Mutable dict
        template<typename T>
            requires(map_collection<T>)
        explicit DictDataProxy(T& cont) :
            _ptrs {vec_transform(cont, [](auto&& e) -> pair<void*, void*> { return {cast_to_void(&e.first), cast_to_void(&e.second)}; })}
        {
            _clearCallback = [&]() FO_DEFERRED { cont.clear(); };
            _addCallback = [&](void* key, void* value) FO_DEFERRED { cont.emplace_back(*cast_from_void<const typename T::key_type*>(key), *cast_from_void<typename T::mapped_type*>(value)); };
        }

        // Const dict
        template<typename T>
            requires(map_collection<T>)
        explicit DictDataProxy(const T& cont) :
            _ptrs {vec_transform(cont, [](auto&& e) -> pair<void*, void*> { return {cast_to_void(&e.first), cast_to_void(&e.second)}; })}
        {
            _clearCallback = []() FO_DEFERRED { throw InvalidCallException(FO_LINE_STR); };
            _addCallback = [](void* /*key*/, void* /*value*/) FO_DEFERRED { throw InvalidCallException(FO_LINE_STR); };
        }

        auto Size() const noexcept -> size_t { return _ptrs.size(); }
        auto Get(size_t index) const noexcept -> pair<void*, void*> { return _ptrs[index]; }
        void Clear() { _ptrs.clear(), _clearCallback(); }
        void Add(void* key, void* value) { _addCallback(key, value), _ptrs.emplace_back(key, value); }

    private:
        vector<pair<void*, void*>> _ptrs;
        function<void()> _clearCallback {};
        function<void(void*, void*)> _addCallback {};
    };

    using StorageEntryType = variant<int32, ArrayDataProxy, DictDataProxy, Entity*>;

    struct NativeDataAccessor final : DataAccessor
    {
        [[nodiscard]] auto GetBackendIndex() const noexcept -> int32 override { return -1; }
        [[nodiscard]] auto GetArraySize(void* data) const -> size_t override { return cast_from_void<ArrayDataProxy*>(data)->Size(); }
        [[nodiscard]] auto GetArrayElement(void* data, size_t index) const -> void* override { return cast_from_void<ArrayDataProxy*>(data)->Get(index); }
        [[nodiscard]] auto GetDictSize(void* data) const -> size_t override { return cast_from_void<DictDataProxy*>(data)->Size(); }
        [[nodiscard]] auto GetDictElement(void* data, size_t index) const -> pair<void*, void*> override { return cast_from_void<DictDataProxy*>(data)->Get(index); }
        [[nodiscard]] auto GetCallback(void* /*data*/) const -> unique_del_ptr<ScriptFuncDesc> override { throw NotSupportedException(FO_LINE_STR); }

        void ClearArray(void* data) const override { cast_from_void<ArrayDataProxy*>(data)->Clear(); }
        void AddArrayElement(void* data, void* value) const override { cast_from_void<ArrayDataProxy*>(data)->Add(value); }
        void ClearDict(void* data) const override { cast_from_void<DictDataProxy*>(data)->Clear(); }
        void AddDictElement(void* data, void* key, void* value) const override { cast_from_void<DictDataProxy*>(data)->Add(key, value); }
    };

    static constexpr NativeDataAccessor NATIVE_DATA_ACCESSOR;

    template<class T>
    static auto NormalizeArg(T&& arg, StorageEntryType& temp_storage) -> void* // NOLINT(cppcoreguidelines-missing-std-forward)
    {
        using raw_t = std::remove_cvref_t<T>;

        if constexpr (vector_collection<raw_t>) {
            return cast_to_void(&temp_storage.emplace<ArrayDataProxy>(arg));
        }
        else if constexpr (map_collection<raw_t>) {
            return cast_to_void(&temp_storage.emplace<DictDataProxy>(arg));
        }
        else if constexpr (std::is_base_of_v<Entity, std::remove_pointer_t<raw_t>>) {
            return cast_to_void(&temp_storage.emplace<Entity*>(arg));
        }
        else {
            return cast_to_void(&arg);
        }
    }
}

struct ScriptFuncDesc
{
    using CallType = function<void(FuncCallData&)>;

    hstring Name {};
    vector<ArgDesc> Args {};
    ComplexTypeDesc Ret {};
    CallType Call {};
    uintptr_t DelegateObj {};
};

using ScriptFuncName = pair<hstring, uintptr_t>; // Name + Delegate object address

template<typename TRet, typename... Args>
class ScriptFunc final
{
public:
    ScriptFunc() noexcept = default;

    explicit ScriptFunc(ScriptFuncDesc* func) noexcept :
        _func {unique_del_ptr<ScriptFuncDesc>(func, [](auto&&) { })}
    {
    }

    explicit ScriptFunc(unique_del_ptr<ScriptFuncDesc> func) noexcept :
        _func {std::move(func)}
    {
    }

    ScriptFunc(const ScriptFunc&) = delete;
    ScriptFunc(ScriptFunc&& other) noexcept = default;
    auto operator=(const ScriptFunc&) = delete;
    auto operator=(ScriptFunc&& other) noexcept -> ScriptFunc& = default;

    [[nodiscard]] explicit operator bool() const noexcept { return !!_func; }
    [[nodiscard]] auto IsDelegate() const noexcept -> bool { return _func && _func->DelegateObj != 0; }
    [[nodiscard]] auto GetName() const noexcept -> ScriptFuncName { return _func ? ScriptFuncName(_func->Name, _func->DelegateObj) : ScriptFuncName(); }

    [[nodiscard]] auto GetResult() noexcept -> TRet
    {
        static_assert(!std::is_same_v<TRet, void> || always_false_v<TRet>);
        return _ret;
    }

    auto Call(const Args&... args) noexcept -> bool
    {
        if (_func) {
            if constexpr (std::is_same_v<TRet, void>) {
                array<NativeDataProvider::StorageEntryType, sizeof...(Args)> temp_storage {};
                size_t storage_index = 0;
                array<void*, sizeof...(Args)> args_data {([&] { return NativeDataProvider::NormalizeArg(args, temp_storage[storage_index++]); }())...};
                FuncCallData call {.Accessor = &NativeDataProvider::NATIVE_DATA_ACCESSOR};
                call.ArgsData = args_data;

                try {
                    _func->Call(call);
                    return true;
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
            }
            else {
                array<NativeDataProvider::StorageEntryType, sizeof...(Args) + 1> temp_storage {};
                size_t storage_index = 0;
                array<void*, sizeof...(Args)> args_data {([&] { return NativeDataProvider::NormalizeArg(args, temp_storage[storage_index++]); }())...};
                FuncCallData call {.Accessor = &NativeDataProvider::NATIVE_DATA_ACCESSOR};
                call.ArgsData = args_data;
                call.RetData = NativeDataProvider::NormalizeArg(&_ret, temp_storage[storage_index]);

                try {
                    _func->Call(call);
                    return true;
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                }
            }
        }

        return false;
    }

private:
    unique_del_ptr<ScriptFuncDesc> _func;
    std::conditional_t<std::is_same_v<TRet, void>, int, TRet> _ret {};
};

namespace NativeDataCaller
{
    template<typename Fn>
    struct NativeCallTraits;

    template<typename R, typename... Args>
    struct NativeCallTraits<R (*)(Args...)>
    {
        using return_type = R;
        using args_tuple = tuple<Args...>;
        static constexpr size_t arity = sizeof...(Args);
    };

    template<typename T, typename U>
    auto ConvertArg(void* data, const DataAccessor* accessor, U& temp) -> T
    {
        using raw_t = std::remove_cvref_t<T>;

        if constexpr (vector_collection<raw_t>) {
            auto& v = temp.emplace();
            const auto size = accessor->GetArraySize(data);
            v.reserve(size);

            for (size_t i = 0; i < size; i++) {
                v.emplace_back(*cast_from_void<typename raw_t::value_type*>(accessor->GetArrayElement(data, i)));
            }

            return v;
        }
        else if constexpr (map_collection<raw_t>) {
            auto& m = temp.emplace();
            const auto size = accessor->GetDictSize(data);

            for (size_t i = 0; i < size; i++) {
                const auto kv = accessor->GetDictElement(data, i);
                m.emplace(*cast_from_void<const typename raw_t::key_type*>(kv.first), *cast_from_void<typename raw_t::mapped_type*>(kv.second));
            }

            return m;
        }
        else if constexpr (specialization_of<T, ScriptFunc>) {
            auto callback = accessor->GetCallback(data);
            return T(std::move(callback));
        }
        else if constexpr (std::is_same_v<raw_t, string_view>) {
            return temp.emplace(*cast_from_void<string*>(data));
        }
        else if constexpr (std::is_base_of_v<Entity, std::remove_pointer_t<raw_t>>) {
            auto* base_entity = *cast_from_void<Entity**>(data);
            auto* target_entity = dynamic_cast<std::remove_pointer_t<raw_t>*>(base_entity);
            FO_RUNTIME_ASSERT(!base_entity || target_entity);
            FO_RUNTIME_ASSERT(!target_entity || !target_entity->IsDestroyed());
            return temp.emplace(target_entity);
        }
        else if constexpr (std::is_reference_v<T>) {
            return **cast_from_void<raw_t**>(data);
        }
        else {
            return *cast_from_void<raw_t*>(data);
        }
    }

    template<typename T, typename U>
    void ReturnArg(void* data, const DataAccessor* accessor, U& temp)
    {
        using raw_t = std::remove_cvref_t<T>;

        // Copy back mutable collections or return value
        if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>) {
            if constexpr (vector_collection<raw_t>) {
                auto& v = temp.value();
                accessor->ClearArray(data);

                for (auto& e : v) {
                    accessor->AddArrayElement(data, cast_to_void(&e));
                }
            }
            else if constexpr (map_collection<raw_t>) {
                auto& v = temp.value();
                accessor->ClearDict(data);

                for (auto& e : v) {
                    accessor->AddDictElement(data, cast_to_void(&e.first), cast_to_void(&e.second));
                }
            }
            else if constexpr (std::is_base_of_v<Entity, std::remove_pointer_t<raw_t>>) {
                FO_RUNTIME_ASSERT(temp.has_value());
                *cast_from_void<Entity**>(data) = static_cast<Entity*>(temp.value());
            }
            else {
                if (temp.has_value()) {
                    *cast_from_void<raw_t*>(data) = std::move(temp.value());
                }
            }
        }
    }

    template<typename R, typename... Args, size_t... I>
    auto NativeCallImpl(R (*fn)(Args...), FuncCallData& call, std::index_sequence<I...> /**/)
    {
        tuple<optional<std::remove_cvref_t<Args>>...> temp_data;

        if constexpr (!std::is_void_v<R>) {
            R&& r = fn(ConvertArg<Args>(call.ArgsData[I], call.Accessor.get(), std::get<I>(temp_data))...);
            optional<std::remove_cvref_t<R>> temp_r = std::move(r);
            ReturnArg<std::add_lvalue_reference_t<R>>(call.RetData, call.Accessor.get(), temp_r);
        }
        else {
            fn(ConvertArg<Args>(call.ArgsData[I], call.Accessor.get(), std::get<I>(temp_data))...);
        }

        (void)std::initializer_list<int> {(ReturnArg<Args>(call.ArgsData[I], call.Accessor.get(), std::get<I>(temp_data)), 0)...};
    }

    template<auto Fn>
    void NativeCall(FuncCallData& call)
    {
        using Traits = NativeCallTraits<decltype(Fn)>;

        FO_RUNTIME_ASSERT(call.ArgsData.size() == Traits::arity);
        FO_RUNTIME_ASSERT(!!call.RetData == !std::is_void_v<typename Traits::return_type>);

        NativeCallImpl(Fn, call, std::make_index_sequence<Traits::arity> {});
    }
}

class ScriptSystemBackend
{
public:
    static constexpr int32 ANGELSCRIPT_BACKEND_INDEX = 0;
    // static constexpr int32 MONO_BACKEND_INDEX = 1;
    virtual ~ScriptSystemBackend() = default;
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

    void InitSubsystems(BaseEngine* engine);
    void InitSubsystems(EngineMetadata* meta, const FileSystem& resources);
    void InitModules();
    void ProcessScriptEvents();

    void RegisterBackend(size_t index, unique_ptr<ScriptSystemBackend> backend);
    void ShutdownBackends();

    template<typename T>
        requires(std::is_base_of_v<ScriptSystemBackend, T>)
    [[nodiscard]] auto GetBackend(size_t index) noexcept -> T*
    {
        return static_cast<T*>(_backends[index].get());
    }

    template<typename T>
        requires(std::is_base_of_v<ScriptSystemBackend, T>)
    [[nodiscard]] auto GetBackend(size_t index) const noexcept -> const T*
    {
        return static_cast<const T*>(_backends[index].get());
    }

    template<typename TRet, typename... Args>
    [[nodiscard]] auto FindFunc(hstring func_name) noexcept -> ScriptFunc<TRet, Args...>
    {
        const auto range = _globalFuncMap.equal_range(func_name);
        const array<size_t, sizeof...(Args)> args_arr {ArgMapTypeIndex<Args>()...};

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second.get(), args_arr, ArgMapTypeIndex<TRet>())) {
                return ScriptFunc<TRet, Args...>(it->second.get());
            }
        }

        return {};
    }

    [[nodiscard]] auto FindFunc(hstring func_name, const_span<size_t> arg_types) noexcept -> ScriptFuncDesc*
    {
        const auto range = _globalFuncMap.equal_range(func_name);

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second.get(), arg_types, ArgMapTypeIndex<void>())) {
                return it->second.get();
            }
        }

        return nullptr;
    }

    template<typename TRet, typename... Args>
    [[nodiscard]] auto CheckFunc(hstring func_name) const noexcept -> bool
    {
        const auto range = _globalFuncMap.equal_range(func_name);
        const array<size_t, sizeof...(Args)> args_arr {ArgMapTypeIndex<Args>()...};

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second.get(), args_arr, ArgMapTypeIndex<TRet>())) {
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

        if (func && func.Call(args...)) {
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
        return func && func.Call(args...);
    }

    [[nodiscard]] auto ValidateArgs(const ScriptFuncDesc* func, const_span<size_t> arg_types, size_t ret_type) const noexcept -> bool;

    void AddLoopCallback(function<void()> callback);
    void AddGlobalScriptFunc(ScriptFuncDesc* func);
    void AddInitFunc(ScriptFunc<void> func, int32 priority);

    template<typename T>
        requires(!std::is_pointer_v<T>)
    void MapEngineType(const BaseTypeDesc& type)
    {
        using raw_t = std::remove_cvref_t<T>;

        _engineTypes.emplace(typeid(raw_t).hash_code(), ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = type});
        _engineTypes.emplace(typeid(raw_t*).hash_code(), ComplexTypeDesc {.Kind = ComplexTypeKind::Simple, .BaseType = type, .IsMutable = true});

        // Skip vector of bool due to temporary address of indexed element
        if constexpr (!std::is_same_v<T, bool>) {
            _engineTypes.emplace(typeid(vector<raw_t>).hash_code(), ComplexTypeDesc {.Kind = ComplexTypeKind::Array, .BaseType = type});
            _engineTypes.emplace(typeid(vector<raw_t>*).hash_code(), ComplexTypeDesc {.Kind = ComplexTypeKind::Array, .BaseType = type, .IsMutable = true});
        }
    }

private:
    template<typename T>
    static constexpr auto ArgMapTypeIndex() -> size_t
    {
        using raw_t = std::remove_cvref_t<std::remove_pointer_t<T>>;

        if constexpr (std::is_lvalue_reference_v<T>) {
            return typeid(raw_t*).hash_code();
        }
        else {
            return typeid(raw_t).hash_code();
        }
    }

    vector<unique_ptr<ScriptSystemBackend>> _backends {};
    vector<function<void()>> _loopCallbacks {};
    unordered_map<size_t, ComplexTypeDesc> _engineTypes {};
    unordered_multimap<hstring, raw_ptr<ScriptFuncDesc>> _globalFuncMap {};
    vector<pair<ScriptFunc<void>, int32>> _initFunc {};
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
    static auto CallInitScript(ScriptSystem* script_sys, T* entity, hstring init_script, bool first_time) -> bool
    {
        if (init_script) {
            if (auto init_func = script_sys->FindFunc<void, T*, bool>(init_script)) {
                if (!init_func.Call(entity, first_time)) {
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

FO_END_NAMESPACE
