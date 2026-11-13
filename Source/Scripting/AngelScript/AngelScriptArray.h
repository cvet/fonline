// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "ScriptSystem.h"

namespace AngelScript
{
    class asIScriptContext;
    class asIScriptEngine;
    class asITypeInfo;
}

FO_BEGIN_NAMESPACE

struct ScriptArrayTypeData;
class ScriptDict;

class ScriptArray final
{
    friend class safe_alloc;

    template<typename T>
    static constexpr bool IsHandleAtType {std::is_void_v<std::remove_cv_t<T>> || std::is_same_v<std::remove_cv_t<T>, Entity> || //
        std::is_same_v<std::remove_cv_t<T>, DynamicRefTypeInstance> || std::is_same_v<std::remove_cv_t<T>, ScriptArray> || std::is_same_v<std::remove_cv_t<T>, ScriptDict>};

public:
    static auto Create(ptr<AngelScript::asITypeInfo> ti) -> refcount_ptr<ScriptArray>;
    static auto Create(ptr<AngelScript::asITypeInfo> ti, int32_t length) -> refcount_ptr<ScriptArray>;
    static auto Create(ptr<AngelScript::asITypeInfo> ti, int32_t length, ptr<void> def_val) -> refcount_ptr<ScriptArray>;
    static auto Create(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list) -> refcount_ptr<ScriptArray>;

    ScriptArray(ScriptArray&&) noexcept = delete;
    auto operator=(ScriptArray&&) noexcept -> ScriptArray& = delete;

    auto operator=(const ScriptArray& other) -> ScriptArray&;

    [[nodiscard]] auto GetArrayObjectType() -> ptr<AngelScript::asITypeInfo>;
    [[nodiscard]] auto GetArrayObjectType() const -> ptr<const AngelScript::asITypeInfo>;
    [[nodiscard]] auto GetArrayTypeId() const -> int32_t;
    [[nodiscard]] auto GetElementTypeId() const -> int32_t;
    [[nodiscard]] auto GetSize() const -> int32_t { return static_cast<int32_t>(_buffer.size() / _elementSize); }
    [[nodiscard]] auto GetCapacity() const -> int32_t { return static_cast<int32_t>(_buffer.capacity() / _elementSize); }
    [[nodiscard]] auto IsEmpty() const -> bool { return _buffer.empty(); }
    [[nodiscard]] auto At(int32_t index) const -> ptr<void>;
    [[nodiscard]] auto GetRefCount() const -> int32_t;
    [[nodiscard]] auto GetFlag() const -> bool;

    template<typename T, typename Index>
        requires(IsHandleAtType<T>)
    [[nodiscard]] auto AtAs(Index index) const -> nptr<T>
    {
        if constexpr (std::is_void_v<std::remove_cv_t<T>>) {
            return NativeDataProvider::ReadHandleSlot(At(numeric_cast<int32_t>(index)));
        }
        else {
            return NativeDataProvider::ReadTypedHandleSlot<std::remove_const_t<T>>(At(numeric_cast<int32_t>(index)));
        }
    }

    template<typename T, typename Index>
        requires(!IsHandleAtType<T>)
    [[nodiscard]] auto AtAs(Index index) const -> ptr<T>
    {
        ptr<void> value {At(numeric_cast<int32_t>(index))};

        return cast_from_void<T*>(value.get());
    }

    auto operator==(const ScriptArray& other) const -> bool;

    void AddRef() const;
    void Release() const;

    void Reserve(int32_t max_elements);
    void Resize(int32_t num_elements);
    void SetValue(int32_t index, ptr<void> value);
    void InsertAt(int32_t index, ptr<void> value);
    void InsertAt(int32_t index, const ScriptArray& other);
    void InsertLast(ptr<void> value);
    void RemoveAt(int32_t index);
    void RemoveLast();
    void RemoveRange(int32_t start, int32_t num_elements);
    void SortAsc();
    void SortDesc();
    void SortAsc(int32_t start_at, int32_t count);
    void SortDesc(int32_t start_at, int32_t count);
    void Sort(int32_t start_at, int32_t count, bool asc);
    void Reverse();
    auto Find(ptr<void> value) const -> int32_t;
    auto Find(int32_t start_at, ptr<void> value) const -> int32_t;
    auto FindByRef(ptr<void> ref) const -> int32_t;
    auto FindByRef(int32_t start_at, ptr<void> ref) const -> int32_t;
    auto GetBuffer() -> nptr<void>;
    auto GetBuffer() const -> nptr<void>;

    void SetFlag() const;
    void EnumReferences(ptr<AngelScript::asIScriptEngine> engine);
    void ReleaseAllHandles();

private:
    explicit ScriptArray(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list);
    explicit ScriptArray(int32_t length, ptr<AngelScript::asITypeInfo> ti);
    explicit ScriptArray(int32_t length, ptr<void> def_val, ptr<AngelScript::asITypeInfo> ti);
    explicit ScriptArray(const ScriptArray& other);
    ~ScriptArray();

    auto GetArrayItemPointer(int32_t index) -> ptr<void>;
    auto GetArrayItemPointer(int32_t index) const -> ptr<void>;
    auto GetDataPointer(ptr<void> buf) const -> ptr<void>;
    void Copy(ptr<void> dst, ptr<void> src) const;
    void PrecacheSubTypeData();
    void CheckArraySize(int32_t num_elements) const;
    void Resize(int32_t delta, int32_t at);
    void CreateBuffer(int32_t num_elements);
    void DeleteBuffer() noexcept;
    void CopyBuffer(const ScriptArray& src);
    void Construct(int32_t start, int32_t end);
    void Destruct(int32_t start, int32_t end);
    auto Equals(ptr<void> a, ptr<void> b, nptr<AngelScript::asIScriptContext> ctx) const -> bool;
    auto Less(ptr<void> a, ptr<void> b, bool asc, nptr<AngelScript::asIScriptContext> ctx) const -> bool;

    refcount_ptr<AngelScript::asITypeInfo> _typeInfo;
    int32_t _subTypeId;
    int32_t _elementSize {};
    nptr<ScriptArrayTypeData> _subTypeData {};
    vector<uint8_t> _buffer {};
    mutable std::atomic<int32_t> _refCount {1};
    mutable std::atomic<bool> _gcFlag {};
};

void RegisterAngelScriptArray(ptr<AngelScript::asIScriptEngine> as_engine);

FO_END_NAMESPACE

#endif
