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

#include "Properties.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ScriptSystemException);
FO_DECLARE_EXCEPTION(ScriptException);
FO_DECLARE_EXCEPTION(ScriptCoreException);
FO_DECLARE_EXCEPTION(ScriptCallException);
FO_DECLARE_EXCEPTION(ScriptCompilerException);

// ReSharper disable CppInconsistentNaming
enum class ScriptEnum_uint8 : uint8_t
{
};
enum class ScriptEnum_uint16 : uint16_t
{
};
enum class ScriptEnum_int32 : int32_t
{
};
enum class ScriptEnum_uint32 : uint32_t
{
};
// ReSharper restore CppInconsistentNaming

using GameProperty = ScriptEnum_uint16;
using PlayerProperty = ScriptEnum_uint16;
using ItemProperty = ScriptEnum_uint16;
using CritterProperty = ScriptEnum_uint16;
using MapProperty = ScriptEnum_uint16;
using LocationProperty = ScriptEnum_uint16;

class ScriptSystem;
class FileSystem;

class Property;
class PropertyRawData;
class PropertyRegistrator;

class EngineMetadata;
class BaseEngine;

class Entity;
using AbstractItem = Entity;
using ScriptSelfEntity = Entity;

class DynamicRefTypeInstance final : public RefCounted<DynamicRefTypeInstance>
{
public:
    explicit DynamicRefTypeInstance(ptr<const PropertyRegistrator> registrator) noexcept;
    DynamicRefTypeInstance(const DynamicRefTypeInstance&) = delete;
    DynamicRefTypeInstance(DynamicRefTypeInstance&&) = delete;
    auto operator=(const DynamicRefTypeInstance&) -> DynamicRefTypeInstance& = delete;
    auto operator=(DynamicRefTypeInstance&&) -> DynamicRefTypeInstance& = delete;
    ~DynamicRefTypeInstance() noexcept;

    [[nodiscard]] auto GetRawData(ptr<const Property> prop) const -> span<const uint8_t>;
    [[nodiscard]] auto GetSerializedRawData(const BaseTypeDesc& base_type) -> const_span<uint8_t>;

    void LoadFromRawData(const BaseTypeDesc& base_type, span<const uint8_t> raw_data);
    void SetValue(ptr<const Property> prop, PropertyRawData& prop_data);

private:
    [[nodiscard]] auto GetProps() noexcept -> ptr<Properties>;
    [[nodiscard]] auto GetProps() const noexcept -> ptr<const Properties>;

    ptr<const PropertyRegistrator> _registrator;
    optional<Properties> _props {};
    vector<uint8_t> _cachedRawData {};
    bool _cachedRawDataDirty {};
};

template<typename T>
using readonly_vector = const vector<T>&;
template<typename K, typename V>
using readonly_map = const map<K, V>&;

struct ScriptFuncDesc;

struct DataAccessor
{
    [[nodiscard]] virtual auto GetBackendIndex() const noexcept -> int32_t = 0;
    [[nodiscard]] virtual auto GetArraySize(ptr<void> /*data*/) const -> size_t { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto GetArrayElement(ptr<void> /*data*/, size_t /*index*/) const -> ptr<void> { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto GetDictSize(ptr<void> /*data*/) const -> size_t { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto GetDictElement(ptr<void> /*data*/, size_t /*index*/) const -> pair<ptr<void>, ptr<void>> { throw InvalidCallException(FO_LINE_STR); }
    [[nodiscard]] virtual auto GetCallback(ptr<void> /*data*/) const -> unique_del_nptr<ScriptFuncDesc> { throw InvalidCallException(FO_LINE_STR); }
    virtual void ClearArray(ptr<void> /*data*/) const { throw InvalidCallException(FO_LINE_STR); }
    virtual void AddArrayElement(ptr<void> /*data*/, ptr<void> /*value*/) const { throw InvalidCallException(FO_LINE_STR); }
    virtual void ClearDict(ptr<void> /*data*/) const { throw InvalidCallException(FO_LINE_STR); }
    virtual void AddDictElement(ptr<void> /*data*/, ptr<void> /*key*/, ptr<void> /*value*/) const { throw InvalidCallException(FO_LINE_STR); }
    virtual ~DataAccessor() = default;
};

struct FuncCallData
{
    ptr<const DataAccessor> Accessor;
    const_span<ptr<void>> ArgsData {};
    nptr<void> RetData {};
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
            _ptrs {to_vector(vec_transform(cont, [](auto&& e) -> ptr<void> {
                auto element = make_ptr(&e).void_cast();
                return element;
            }))}
        {
            _clearCallback = [&]() FO_DEFERRED { cont.clear(); };
            _addCallback = [&](ptr<void> value) FO_DEFERRED { cont.emplace_back(*cast_from_void<typename T::value_type*>(value.get())); };
        }

        // Const array
        template<typename T>
            requires(vector_collection<T>)
        explicit ArrayDataProxy(const T& cont) :
            _ptrs {to_vector(vec_transform(cont, [](auto&& e) -> ptr<void> {
                auto element = make_ptr(&e).void_cast();
                return element;
            }))}
        {
            _clearCallback = [&]() FO_DEFERRED { throw InvalidCallException(FO_LINE_STR); };
            _addCallback = [&](ptr<void> /*value*/) FO_DEFERRED { throw InvalidCallException(FO_LINE_STR); };
        }

        auto Size() const noexcept -> size_t { return _ptrs.size(); }
        auto Get(size_t index) const noexcept -> ptr<void> { return _ptrs[index]; }
        void Clear() { _ptrs.clear(), _clearCallback(); }
        void Add(ptr<void> value) { _addCallback(value), _ptrs.emplace_back(value); }

    private:
        vector<ptr<void>> _ptrs;
        function<void()> _clearCallback {};
        function<void(ptr<void>)> _addCallback {};
    };

    class DictDataProxy
    {
    public:
        // Mutable dict
        template<typename T>
            requires(map_collection<T>)
        explicit DictDataProxy(T& cont) :
            _ptrs {vec_transform(cont, [](auto&& e) -> pair<ptr<void>, ptr<void>> { return {make_ptr(&e.first).void_cast(), make_ptr(&e.second).void_cast()}; })}
        {
            _clearCallback = [&]() FO_DEFERRED { cont.clear(); };
            _addCallback = [&](ptr<void> key, ptr<void> value) FO_DEFERRED { cont.emplace(*cast_from_void<const typename T::key_type*>(key.get()), *cast_from_void<typename T::mapped_type*>(value.get())); };
        }

        // Const dict
        template<typename T>
            requires(map_collection<T>)
        explicit DictDataProxy(const T& cont) :
            _ptrs {vec_transform(cont, [](auto&& e) -> pair<ptr<void>, ptr<void>> { return {make_ptr(&e.first).void_cast(), make_ptr(&e.second).void_cast()}; })}
        {
            _clearCallback = []() FO_DEFERRED { throw InvalidCallException(FO_LINE_STR); };
            _addCallback = [](ptr<void> /*key*/, ptr<void> /*value*/) FO_DEFERRED { throw InvalidCallException(FO_LINE_STR); };
        }

        auto Size() const noexcept -> size_t { return _ptrs.size(); }
        auto Get(size_t index) const noexcept -> pair<ptr<void>, ptr<void>> { return _ptrs[index]; }
        void Clear() { _ptrs.clear(), _clearCallback(); }
        void Add(ptr<void> key, ptr<void> value) { _addCallback(key, value), _ptrs.emplace_back(key, value); }

    private:
        vector<pair<ptr<void>, ptr<void>>> _ptrs;
        function<void()> _clearCallback {};
        function<void(ptr<void>, ptr<void>)> _addCallback {};
    };

    using StorageEntryType = variant<int32_t, ArrayDataProxy, DictDataProxy, nptr<Entity>>;

    struct NativeDataAccessor final : DataAccessor
    {
        [[nodiscard]] auto GetBackendIndex() const noexcept -> int32_t override { return -1; }
        [[nodiscard]] auto GetArraySize(ptr<void> data) const -> size_t override { return cast_from_void<ArrayDataProxy*>(data.get())->Size(); }
        [[nodiscard]] auto GetArrayElement(ptr<void> data, size_t index) const -> ptr<void> override { return cast_from_void<ArrayDataProxy*>(data.get())->Get(index); }
        [[nodiscard]] auto GetDictSize(ptr<void> data) const -> size_t override { return cast_from_void<DictDataProxy*>(data.get())->Size(); }
        [[nodiscard]] auto GetDictElement(ptr<void> data, size_t index) const -> pair<ptr<void>, ptr<void>> override { return cast_from_void<DictDataProxy*>(data.get())->Get(index); }
        [[nodiscard]] auto GetCallback(ptr<void> /*data*/) const -> unique_del_nptr<ScriptFuncDesc> override { throw NotSupportedException(FO_LINE_STR); }

        void ClearArray(ptr<void> data) const override { cast_from_void<ArrayDataProxy*>(data.get())->Clear(); }
        void AddArrayElement(ptr<void> data, ptr<void> value) const override { cast_from_void<ArrayDataProxy*>(data.get())->Add(value); }
        void ClearDict(ptr<void> data) const override { cast_from_void<DictDataProxy*>(data.get())->Clear(); }
        void AddDictElement(ptr<void> data, ptr<void> key, ptr<void> value) const override { cast_from_void<DictDataProxy*>(data.get())->Add(key, value); }
    };

    static constexpr NativeDataAccessor NATIVE_DATA_ACCESSOR;

    template<class T>
    static auto NormalizeArg(T&& arg, StorageEntryType& temp_storage) -> ptr<void> // NOLINT(cppcoreguidelines-missing-std-forward)
    {
        using raw_t = std::remove_cvref_t<T>;

        if constexpr (vector_collection<raw_t>) {
            return make_ptr(&temp_storage.emplace<ArrayDataProxy>(arg)).void_cast();
        }
        else if constexpr (map_collection<raw_t>) {
            return make_ptr(&temp_storage.emplace<DictDataProxy>(arg)).void_cast();
        }
        else if constexpr (is_borrow_pointer_wrapper_v<raw_t>) {
            using wrapped_t = std::remove_const_t<typename raw_t::element_type>;
            if constexpr (std::is_base_of_v<Entity, wrapped_t>) {
                ptr<nptr<Entity>> entity = &temp_storage.emplace<nptr<Entity>>(arg);
                return make_ptr(entity->get_pp()).void_cast();
            }
            else {
                return make_ptr(&arg).void_cast();
            }
        }
        else if constexpr (std::is_pointer_v<raw_t>) {
            static_assert(always_false_v<T>, "Raw pointer native script ABI arguments are not supported; use ptr/nptr");
        }
        else {
            return make_ptr(&arg).void_cast();
        }
    }
}

struct ScriptFuncDesc
{
    using CallType = function<void(FuncCallData&)>;
    using AttributeCheckerType = function<bool(string_view)>;
    using ReturnValueCleanerType = function<void(ptr<void>)>;

    hstring Name {};
    vector<ArgDesc> Args {};
    ComplexTypeDesc Ret {};
    CallType Call {};
    AttributeCheckerType AttributeChecker {};
    ReturnValueCleanerType ReturnValueCleaner {};
    uintptr_t DelegateObj {};
};

using ScriptFuncName = pair<hstring, uintptr_t>; // Name + Delegate object address

inline void IgnoreBorrowedScriptFuncDesc(ptr<ScriptFuncDesc> func) noexcept
{
    ignore_unused(func);
}

inline auto MakeBorrowedScriptFuncDesc(ptr<ScriptFuncDesc> func) -> unique_del_ptr<ScriptFuncDesc>
{
    return make_unique_del_ptr(func, IgnoreBorrowedScriptFuncDesc);
}

template<typename TRet, typename... Args>
class ScriptFunc final
{
public:
    ScriptFunc() noexcept = default;

    explicit ScriptFunc(ptr<ScriptFuncDesc> func) noexcept :
        _func {MakeBorrowedScriptFuncDesc(func)}
    {
        if constexpr (!std::is_same_v<TRet, void>) {
            _returnValueCleaner = func->ReturnValueCleaner;
        }
    }

    explicit ScriptFunc(unique_del_nptr<ScriptFuncDesc> func) noexcept :
        _func {std::move(func)}
    {
        if constexpr (!std::is_same_v<TRet, void>) {
            _returnValueCleaner = _func ? _func->ReturnValueCleaner : ScriptFuncDesc::ReturnValueCleanerType {};
        }
    }

    ScriptFunc(const ScriptFunc&) = delete;
    ScriptFunc(ScriptFunc&& other) noexcept :
        _func {std::move(other._func)},
        _ret {std::move(other._ret)}
    {
        if constexpr (!std::is_same_v<TRet, void>) {
            _returnValueCleaner = std::move(other._returnValueCleaner);
            other._returnValueCleaner = {};
        }
    }
    auto operator=(const ScriptFunc&) = delete;
    auto operator=(ScriptFunc&& other) noexcept -> ScriptFunc&
    {
        if (this != std::addressof(other)) {
            ClearStoredReturn();
            _func = std::move(other._func);
            _ret = std::move(other._ret);

            if constexpr (!std::is_same_v<TRet, void>) {
                _returnValueCleaner = std::move(other._returnValueCleaner);
                other._returnValueCleaner = {};
            }
        }

        return *this;
    }
    ~ScriptFunc() noexcept { ClearStoredReturn(); }

    [[nodiscard]] explicit operator bool() const noexcept { return !!_func; }
    [[nodiscard]] auto IsDelegate() const noexcept -> bool
    {
        if (!_func) {
            return false;
        }

        FO_STRONG_ASSERT(_func, "Script function is null");
        return _func->DelegateObj != 0;
    }
    [[nodiscard]] auto GetName() const noexcept -> ScriptFuncName
    {
        if (_func) {
            FO_STRONG_ASSERT(_func, "Script function is null");
            return ScriptFuncName(_func->Name, _func->DelegateObj);
        }

        return ScriptFuncName();
    }
    [[nodiscard]] auto HasAttribute(string_view attribute) const noexcept -> bool
    {
        if (!_func) {
            return false;
        }

        FO_STRONG_ASSERT(_func, "Script function is null");
        return _func->AttributeChecker(attribute);
    }

    [[nodiscard]] auto GetResult() noexcept -> TRet
    {
        static_assert(!std::is_same_v<TRet, void> || always_false_v<TRet>);
        return _ret;
    }

    auto Call(const Args&... args) noexcept -> bool
    {
        if (!_func) {
            return false;
        }

        FO_STRONG_ASSERT(_func, "Script function is null");

        if constexpr (std::is_same_v<TRet, void>) {
            array<NativeDataProvider::StorageEntryType, sizeof...(Args)> temp_storage {};
            size_t storage_index = 0;
            array<ptr<void>, sizeof...(Args)> args_data {([&] { return NativeDataProvider::NormalizeArg(args, temp_storage[storage_index++]); }())...};
            auto accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR);
            FuncCallData call {.Accessor = accessor};
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
            array<ptr<void>, sizeof...(Args)> args_data {([&] { return NativeDataProvider::NormalizeArg(args, temp_storage[storage_index++]); }())...};
            auto accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR);
            FuncCallData call {.Accessor = accessor};
            call.ArgsData = args_data;
            call.RetData = NativeDataProvider::NormalizeArg(_ret, temp_storage[storage_index]);

            try {
                _func->Call(call);
                return true;
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }

        return false;
    }

private:
    void ClearStoredReturn() noexcept
    {
        if constexpr (!std::is_same_v<TRet, void>) {
            if (_returnValueCleaner) {
                safe_call([this] { _returnValueCleaner(make_ptr(&_ret).void_cast()); });
            }
        }
    }

    using ReturnValueCleanerStorage = std::conditional_t<std::is_same_v<TRet, void>, std::nullptr_t, ScriptFuncDesc::ReturnValueCleanerType>;

    unique_del_nptr<ScriptFuncDesc> _func {};
    [[no_unique_address]] ReturnValueCleanerStorage _returnValueCleaner {};
    std::conditional_t<std::is_same_v<TRet, void>, int, TRet> _ret {};
};

namespace NativeDataProvider
{
    inline auto GetHandleSlot(ptr<void> slot) noexcept -> ptr<void*>
    {
        return slot.reinterpret_as<void*>();
    }

    inline auto ReadIndirectHandleSlotPointer(ptr<void> slot_address) noexcept -> ptr<void*>
    {
        return *slot_address.reinterpret_as<void**>();
    }

    template<typename T>
    inline auto ReadTypedHandleSlot(ptr<void> slot) noexcept -> nptr<T>
    {
        if constexpr (std::is_void_v<T>) {
            return *GetHandleSlot(slot);
        }
        else {
            return *slot.reinterpret_as<T*>();
        }
    }

    template<typename T>
    inline auto ReadConstTypedHandleSlot(nptr<const void> slot) noexcept -> nptr<const T>
    {
        if (!slot) {
            return nullptr;
        }

        if constexpr (std::is_void_v<T>) {
            return *slot.reinterpret_as<const void*>();
        }
        else {
            return *slot.reinterpret_as<const T*>();
        }
    }

    template<typename T>
    inline void WriteTypedHandleSlot(ptr<void> slot, nptr<T> value) noexcept
    {
        if constexpr (std::is_void_v<T>) {
            *GetHandleSlot(slot) = value.get();
        }
        else {
            *slot.reinterpret_as<T*>() = value.get();
        }
    }

    inline auto ReadHandleSlot(ptr<const void> slot) noexcept -> nptr<void>
    {
        return *slot.reinterpret_as<void*>();
    }

    inline auto ReadIndirectHandleSlot(ptr<void> slot_address) noexcept -> nptr<void>
    {
        return *ReadIndirectHandleSlotPointer(slot_address);
    }

    inline void WriteHandleSlot(ptr<void> slot, nptr<void> value) noexcept
    {
        *GetHandleSlot(slot) = value.get();
    }

    inline void CheckArgNotNull(const FuncCallData& call, size_t arg_index, string_view method_name, string_view arg_name, string_view type_name)
    {
        const auto arg_object = ReadHandleSlot(call.ArgsData[arg_index]);

        if (!arg_object) {
            throw ScriptException("Null passed to non-nullable parameter", method_name, arg_name, type_name);
        }
    }

    inline void CheckReturnNotNull(const FuncCallData& call, string_view method_name, string_view type_name)
    {
        FO_VERIFY_AND_THROW(call.RetData, "Script call has no return value storage");

        const auto ret_object = ReadHandleSlot(call.RetData);

        if (!ret_object) {
            throw ScriptException("Non-nullable method returned null", method_name, type_name);
        }
    }
}

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
    auto ConvertArg(ptr<void> data, const DataAccessor& accessor, U& temp) -> T
    {
        using raw_t = std::remove_cvref_t<T>;

        if constexpr (vector_collection<raw_t>) {
            auto& v = temp.emplace();
            const auto size = accessor.GetArraySize(data);
            v.reserve(size);

            for (size_t i = 0; i < size; i++) {
                v.emplace_back(*cast_from_void<const typename raw_t::value_type*>(accessor.GetArrayElement(data, i).get()));
            }

            return v;
        }
        else if constexpr (map_collection<raw_t>) {
            auto& m = temp.emplace();
            const auto size = accessor.GetDictSize(data);

            for (size_t i = 0; i < size; i++) {
                const auto kv = accessor.GetDictElement(data, i);
                m.emplace(*cast_from_void<const typename raw_t::key_type*>(kv.first.get()), *cast_from_void<const typename raw_t::mapped_type*>(kv.second.get()));
            }

            return m;
        }
        else if constexpr (specialization_of<T, ScriptFunc>) {
            auto callback = accessor.GetCallback(data);
            return T(std::move(callback));
        }
        else if constexpr (std::is_same_v<raw_t, string_view>) {
            return temp.emplace(*cast_from_void<string*>(data.get()));
        }
        else if constexpr (specialization_of<raw_t, ptr> || specialization_of<raw_t, nptr>) {
            using elem_t = typename raw_t::element_type;
            if constexpr (std::is_base_of_v<Entity, std::remove_const_t<elem_t>>) {
                nptr<Entity> base_entity = NativeDataProvider::ReadTypedHandleSlot<Entity>(data);
                nptr<std::remove_const_t<elem_t>> target_entity = base_entity.template dyn_cast<std::remove_const_t<elem_t>>();
                FO_VERIFY_AND_THROW(!base_entity || target_entity, "Base entity exists but target entity lookup failed");
                FO_VERIFY_AND_THROW(!target_entity || !target_entity->IsDestroyed(), "Target entity lookup returned destroyed entity");
                return raw_t {target_entity};
            }
            else {
                return raw_t {*cast_from_void<elem_t**>(data.get())};
            }
        }
        else if constexpr (std::is_pointer_v<raw_t>) {
            static_assert(always_false_v<T>, "Raw pointer native script ABI arguments are not supported; use ptr/nptr");
        }
        else if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>) {
            // Mutable slot is the address of the caller's variable, so bind the reference to it directly:
            // the native callee mutates the caller's storage in place and no write-back is needed
            return *cast_from_void<raw_t*>(data.get());
        }
        else if constexpr (std::is_reference_v<T>) {
            return **cast_from_void<raw_t**>(data.get());
        }
        else {
            return *cast_from_void<raw_t*>(data.get());
        }
    }

    template<typename T, typename U>
    void ReturnArg(ptr<void> data, const DataAccessor& accessor, U& temp)
    {
        using raw_t = std::remove_cvref_t<T>;

        // Copy back mutable collections or return value
        if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>) {
            if constexpr (vector_collection<raw_t>) {
                auto& v = temp.value();
                accessor.ClearArray(data);

                for (auto& e : v) {
                    accessor.AddArrayElement(data, make_nptr(&e).void_cast());
                }
            }
            else if constexpr (map_collection<raw_t>) {
                auto& v = temp.value();
                accessor.ClearDict(data);

                for (auto& e : v) {
                    accessor.AddDictElement(data, make_nptr(&e.first).void_cast(), make_nptr(&e.second).void_cast());
                }
            }
            else if constexpr (specialization_of<raw_t, ptr> || specialization_of<raw_t, nptr>) {
                using elem_t = typename raw_t::element_type;
                FO_VERIFY_AND_THROW(temp.has_value(), "Optional value is not set");
                if constexpr (std::is_base_of_v<Entity, std::remove_const_t<elem_t>>) {
                    const nptr<elem_t> target_entity = temp.value();
                    NativeDataProvider::WriteTypedHandleSlot<Entity>(data, target_entity);
                }
                else {
                    *cast_from_void<elem_t**>(data.get()) = temp.value().get();
                }
            }
            else if constexpr (std::is_pointer_v<raw_t>) {
                static_assert(always_false_v<T>, "Raw pointer native script ABI arguments are not supported; use ptr/nptr");
            }
            else {
                if (temp.has_value()) {
                    *cast_from_void<raw_t*>(data.get()) = std::move(temp.value());
                }
            }
        }
    }

    template<typename R, typename... Args, size_t... I>
    auto NativeCallImpl(R (*fn)(Args...), FuncCallData& call, std::index_sequence<I...> /**/)
    {
        tuple<optional<std::remove_cvref_t<Args>>...> temp_data;
        ptr<const DataAccessor> accessor = call.Accessor;

        if constexpr (!std::is_void_v<R>) {
            R&& r = fn(ConvertArg<Args>(call.ArgsData[I], *accessor, std::get<I>(temp_data))...);
            optional<std::remove_cvref_t<R>> temp_r = std::move(r);
            ReturnArg<std::add_lvalue_reference_t<R>>(call.RetData, *accessor, temp_r);
        }
        else {
            fn(ConvertArg<Args>(call.ArgsData[I], *accessor, std::get<I>(temp_data))...);
        }

        (void)std::initializer_list<int> {(ReturnArg<Args>(call.ArgsData[I], *accessor, std::get<I>(temp_data)), 0)...};
    }

    template<auto Fn>
    void NativeCall(FuncCallData& call)
    {
        using Traits = NativeCallTraits<decltype(Fn)>;

        FO_VERIFY_AND_THROW(call.ArgsData.size() == Traits::arity, "Native script call argument storage does not match native function arity", call.ArgsData.size(), Traits::arity);
        FO_VERIFY_AND_THROW((call.RetData != nullptr) == !std::is_void_v<typename Traits::return_type>, "Native script call return storage does not match native function return type", call.RetData != nullptr, !std::is_void_v<typename Traits::return_type>);

        NativeCallImpl(Fn, call, std::make_index_sequence<Traits::arity> {});
    }
}

class ScriptSystemBackend
{
public:
    static constexpr int32_t ANGELSCRIPT_BACKEND_INDEX = 0;
    // static constexpr int32_t MONO_BACKEND_INDEX = 1;
    virtual ~ScriptSystemBackend() = default;
};

namespace ScriptTypeIndex
{
    template<typename T>
    struct MutableArg final
    {
    };
}

class ScriptSystem
{
public:
    ScriptSystem() = default;
    ScriptSystem(const ScriptSystem&) = delete;
    ScriptSystem(ScriptSystem&&) noexcept = delete;
    auto operator=(const ScriptSystem&) = delete;
    auto operator=(ScriptSystem&&) noexcept = delete;
    virtual ~ScriptSystem() = default;

    void MapScriptTypes(ptr<EngineMetadata> meta);
    void InitModules();

    auto IsGlobalVarsFrozen() const noexcept -> bool { return _globalVarsFrozen.load(std::memory_order_acquire); }
    void FreezeGlobalVars() noexcept { _globalVarsFrozen.store(true, std::memory_order_release); }
    void UnfreezeGlobalVars() noexcept { _globalVarsFrozen.store(false, std::memory_order_release); }

    void RegisterBackend(size_t index, unique_ptr<ScriptSystemBackend> backend);
    void ShutdownBackends();

    template<typename T>
        requires(std::is_base_of_v<ScriptSystemBackend, T>)
    [[nodiscard]] auto GetBackend(size_t index) noexcept -> nptr<T>
    {
        const auto it = _backends.find(index);
        if (it == _backends.end()) {
            return nullptr;
        }

        return ptr<ScriptSystemBackend> {it->second}.template dyn_cast<T>();
    }

    template<typename T>
        requires(std::is_base_of_v<ScriptSystemBackend, T>)
    [[nodiscard]] auto GetBackend(size_t index) const noexcept -> nptr<const T>
    {
        const auto it = _backends.find(index);
        if (it == _backends.end()) {
            return nullptr;
        }

        return ptr<const ScriptSystemBackend> {it->second}.template dyn_cast<T>();
    }

    template<typename TRet, typename... Args>
    [[nodiscard]] auto FindFunc(hstring func_name) noexcept -> ScriptFunc<TRet, Args...>
    {
        const auto range = _globalFuncMap.equal_range(func_name);
        const array<size_t, sizeof...(Args)> args_arr {ArgMapTypeIndex<Args>()...};

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second, args_arr, ArgMapTypeIndex<TRet>())) {
                return ScriptFunc<TRet, Args...>(it->second);
            }
        }

        return {};
    }

    [[nodiscard]] auto FindFunc(hstring func_name, const_span<size_t> arg_types) noexcept -> nptr<ScriptFuncDesc>;
    [[nodiscard]] auto FindFunc(hstring func_name, span<const ComplexTypeDesc> arg_types) noexcept -> nptr<ScriptFuncDesc>;

    template<typename TRet, typename... Args>
    [[nodiscard]] auto CheckFunc(hstring func_name, string_view attribute = {}) const noexcept -> bool
    {
        const auto range = _globalFuncMap.equal_range(func_name);
        const array<size_t, sizeof...(Args)> args_arr {ArgMapTypeIndex<Args>()...};

        for (auto it = range.first; it != range.second; ++it) {
            if (ValidateArgs(it->second, args_arr, ArgMapTypeIndex<TRet>()) && (attribute.empty() || it->second->AttributeChecker(attribute))) {
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

    template<typename TRet = void, typename... Args>
        requires(std::is_void_v<TRet>)
    [[nodiscard]] auto CallAdminFunc(hstring func_name, const Args&... args) noexcept -> bool
    {
        const auto range = _globalFuncMap.equal_range(func_name);
        const array<size_t, sizeof...(Args)> args_arr {ArgMapTypeIndex<Args>()...};

        for (auto it = range.first; it != range.second; ++it) {
            if (!it->second->AttributeChecker("AdminRemoteCall") || !ValidateArgs(it->second, args_arr, ArgMapTypeIndex<void>())) {
                continue;
            }

            auto func = ScriptFunc<void, Args...>(it->second);
            return func && func.Call(args...);
        }

        return false;
    }

    [[nodiscard]] auto ValidateArgs(ptr<const ScriptFuncDesc> func, const_span<size_t> arg_types, size_t ret_type) const noexcept -> bool;

    void AddGlobalScriptFunc(ptr<ScriptFuncDesc> func);
    void AddInitFunc(ScriptFunc<void> func, int32_t priority);

    template<typename T>
        requires(!std::is_pointer_v<T>)
    void MapEngineType(const BaseTypeDesc& type)
    {
        using raw_t = std::remove_cvref_t<T>;

        const ComplexTypeDesc simple_type {.Kind = ComplexTypeKind::Simple, .BaseType = type};
        const ComplexTypeDesc mutable_simple_type {.Kind = ComplexTypeKind::Simple, .BaseType = type, .IsMutable = true};

        _engineTypes.emplace(typeid(raw_t).hash_code(), simple_type);
        _engineTypes.emplace(typeid(ptr<raw_t>).hash_code(), simple_type);
        _engineTypes.emplace(typeid(nptr<raw_t>).hash_code(), simple_type);
        _engineTypes.emplace(typeid(ptr<const raw_t>).hash_code(), simple_type);
        _engineTypes.emplace(typeid(nptr<const raw_t>).hash_code(), simple_type);
        _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<raw_t>).hash_code(), mutable_simple_type);
        _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<ptr<raw_t>>).hash_code(), mutable_simple_type);
        _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<nptr<raw_t>>).hash_code(), mutable_simple_type);
        _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<ptr<const raw_t>>).hash_code(), mutable_simple_type);
        _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<nptr<const raw_t>>).hash_code(), mutable_simple_type);

        // Skip vector of bool due to temporary address of indexed element
        if constexpr (!std::is_same_v<T, bool>) {
            const ComplexTypeDesc array_type {.Kind = ComplexTypeKind::Array, .BaseType = type};
            const ComplexTypeDesc mutable_array_type {.Kind = ComplexTypeKind::Array, .BaseType = type, .IsMutable = true};

            _engineTypes.emplace(typeid(vector<raw_t>).hash_code(), array_type);
            _engineTypes.emplace(typeid(vector<ptr<raw_t>>).hash_code(), array_type);
            _engineTypes.emplace(typeid(vector<nptr<raw_t>>).hash_code(), array_type);
            _engineTypes.emplace(typeid(vector<ptr<const raw_t>>).hash_code(), array_type);
            _engineTypes.emplace(typeid(vector<nptr<const raw_t>>).hash_code(), array_type);
            _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<vector<raw_t>>).hash_code(), mutable_array_type);
            _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<vector<ptr<raw_t>>>).hash_code(), mutable_array_type);
            _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<vector<nptr<raw_t>>>).hash_code(), mutable_array_type);
            _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<vector<ptr<const raw_t>>>).hash_code(), mutable_array_type);
            _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<vector<nptr<const raw_t>>>).hash_code(), mutable_array_type);
        }
    }

    template<typename TKey, typename TValue>
        requires(!std::is_pointer_v<TKey> && !std::is_pointer_v<TValue>)
    void MapEngineDictType(const BaseTypeDesc& key_type, const BaseTypeDesc& value_type)
    {
        using raw_key_t = std::remove_cvref_t<TKey>;
        using raw_value_t = std::remove_cvref_t<TValue>;

        _engineTypes.emplace(typeid(map<raw_key_t, raw_value_t>).hash_code(), ComplexTypeDesc {.Kind = ComplexTypeKind::Dict, .BaseType = value_type, .KeyType = key_type});
        _engineTypes.emplace(typeid(ScriptTypeIndex::MutableArg<map<raw_key_t, raw_value_t>>).hash_code(), ComplexTypeDesc {.Kind = ComplexTypeKind::Dict, .BaseType = value_type, .KeyType = key_type, .IsMutable = true});
    }

private:
    template<typename T>
    static constexpr auto ArgMapTypeIndex() -> size_t
    {
        using arg_t = std::remove_reference_t<T>;
        using raw_t = std::remove_cvref_t<T>;

        static_assert(!std::is_pointer_v<arg_t>, "Raw pointer script signatures are not supported; use ptr/nptr");

        if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<arg_t>) {
            return typeid(ScriptTypeIndex::MutableArg<raw_t>).hash_code();
        }
        else {
            return typeid(raw_t).hash_code();
        }
    }

    unordered_map<size_t, unique_ptr<ScriptSystemBackend>> _backends {};
    unordered_map<size_t, ComplexTypeDesc> _engineTypes {};
    unordered_multimap<hstring, ptr<ScriptFuncDesc>> _globalFuncMap {};
    vector<pair<ScriptFunc<void>, int32_t>> _initFunc {};
    std::atomic_bool _globalVarsFrozen {};
};

class ScriptHelpers final
{
public:
    ScriptHelpers() = delete;

    template<typename T, typename U>
    [[nodiscard]] static auto GetIntConvertibleEntityProperty(ptr<const BaseEngine> engine, U prop_index) -> ptr<const Property>
    {
        return GetIntConvertibleEntityProperty(engine, T::ENTITY_TYPE_NAME, static_cast<int32_t>(prop_index));
    }

    [[nodiscard]] static auto GetIntConvertibleEntityProperty(ptr<const BaseEngine> engine, string_view type_name, int32_t prop_index) -> ptr<const Property>;

    // Returns false only when the init function itself threw; that exception is already reported by ScriptFunc::Call.
    // An unresolvable init function is a hard error and throws, so it can never degrade into a silent no-op.
    template<typename T>
    static auto CallInitScript(ptr<ScriptSystem> script_sys, ptr<T> entity, hstring init_script, bool first_time) -> bool
    {
        if (init_script) {
            auto init_func = script_sys->FindFunc<void, ptr<T>, bool>(init_script);

            if (!init_func) {
                throw ScriptException("Init function not found or has a mismatched signature", init_script, T::ENTITY_TYPE_NAME);
            }

            if (!init_func.Call(entity, first_time)) {
                return false;
            }
        }

        return true;
    }
};

template<typename T, typename TContainer, typename TResolver>
[[nodiscard]] auto MakeScriptHandleVectorWith(const TContainer& entries, TResolver&& resolver)
{
    vector<ptr<T>> result;
    result.reserve(entries.size());

    for (auto&& entry : entries) {
        auto entry_ptr = resolver(entry);
        result.emplace_back(entry_ptr);
    }

    return result;
}

template<typename T, typename TContainer>
[[nodiscard]] auto MakeScriptHandleVector(const TContainer& entries)
{
    return MakeScriptHandleVectorWith<T>(entries, [](const auto& entry) noexcept -> ptr<T> { return entry.get_no_const(); });
}

template<typename T, typename TContainer>
[[nodiscard]] auto MakeMutableScriptHandleVector(const TContainer& entries)
{
    return MakeScriptHandleVectorWith<T>(entries, [](const auto& entry) noexcept -> ptr<T> { return make_ptr(const_cast<T*>(std::addressof(*entry))); });
}

template<typename T, typename U, typename TContainer>
[[nodiscard]] auto MakeScriptHandleVectorAs(const TContainer& entries)
{
    return MakeScriptHandleVectorWith<T>(entries, [](const auto& entry) noexcept -> ptr<U> { return entry.get_no_const(); });
}

template<typename T, typename U, typename TContainer>
[[nodiscard]] auto MakeScriptRefHandleVectorAs(const TContainer& entries)
{
    vector<ptr<T>> result;
    result.reserve(entries.size());

    for (size_t i = 0; i < entries.size(); i++) {
        auto mutable_entry = make_ptr(const_cast<U*>(std::addressof(*entries[i])));
        result.emplace_back(mutable_entry);
    }

    return result;
}

template<typename TParent, typename TEntity>
inline auto RequireParent(ptr<TEntity> entity, string_view error_message) -> refcount_ptr<TParent>
{
    auto parent = entity->template GetParent<TParent>();

    if (!parent) {
        throw ScriptException(error_message);
    }

    return std::move(parent).take_not_null();
}

FO_END_NAMESPACE
