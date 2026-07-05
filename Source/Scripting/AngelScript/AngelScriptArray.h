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
    friend class SafeAlloc;

    template<typename T>
    static constexpr bool IsHandleAtType = std::is_void_v<std::remove_cv_t<T>> || std::is_same_v<std::remove_cv_t<T>, Entity> || //
        std::is_same_v<std::remove_cv_t<T>, DynamicRefTypeInstance> || std::is_same_v<std::remove_cv_t<T>, ScriptArray> || std::is_same_v<std::remove_cv_t<T>, ScriptDict>;

public:
    static auto Create(ptr<AngelScript::asITypeInfo> ti) -> refcount_ptr<ScriptArray>;
    static auto Create(ptr<AngelScript::asITypeInfo> ti, int32_t length) -> refcount_ptr<ScriptArray>;
    static auto Create(ptr<AngelScript::asITypeInfo> ti, int32_t length, ptr<void> def_val) -> refcount_ptr<ScriptArray>;
    static auto Create(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list) -> refcount_ptr<ScriptArray>;

    ScriptArray(ScriptArray&&) noexcept = delete;
    auto operator=(ScriptArray&&) noexcept -> ScriptArray& = delete;

    auto operator=(const ScriptArray& other) -> ScriptArray&;
    auto operator==(const ScriptArray& other) const -> bool;

    void AddRef() const;
    void Release() const;

    auto GetArrayObjectType() -> ptr<AngelScript::asITypeInfo>;
    auto GetArrayObjectType() const -> ptr<const AngelScript::asITypeInfo>;
    auto GetArrayTypeId() const -> int32_t;
    auto GetElementTypeId() const -> int32_t;

    auto GetSize() const -> int32_t { return static_cast<int32_t>(_buffer.size() / _elementSize); }
    auto GetCapacity() const -> int32_t { return static_cast<int32_t>(_buffer.capacity() / _elementSize); }
    auto IsEmpty() const -> bool { return _buffer.empty(); }
    void Reserve(int32_t max_elements);
    void Resize(int32_t num_elements);
    auto At(int32_t index) const -> ptr<void>;
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

    auto GetRefCount() const -> int32_t;
    void SetFlag() const;
    auto GetFlag() const -> bool;
    void EnumReferences(ptr<AngelScript::asIScriptEngine> engine);
    void ReleaseAllHandles();

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
        ptr<void> value = At(numeric_cast<int32_t>(index));

        return cast_from_void<T*>(value.get());
    }

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
    auto Equals(ptr<void> a, ptr<void> b, nptr<AngelScript::asIScriptContext> nullable_ctx) const -> bool;
    auto Less(ptr<void> a, ptr<void> b, bool asc, nptr<AngelScript::asIScriptContext> nullable_ctx) const -> bool;

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
