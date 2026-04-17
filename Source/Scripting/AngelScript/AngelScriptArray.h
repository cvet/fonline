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

class ScriptArray final
{
    friend class SafeAlloc;

public:
    static auto Create(AngelScript::asITypeInfo* ti) -> ScriptArray*;
    static auto Create(AngelScript::asITypeInfo* ti, int32_t length) -> ScriptArray*;
    static auto Create(AngelScript::asITypeInfo* ti, int32_t length, void* def_val) -> ScriptArray*;
    static auto Create(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptArray*;

    ScriptArray(ScriptArray&&) noexcept = delete;
    auto operator=(ScriptArray&&) noexcept -> ScriptArray& = delete;

    auto operator=(const ScriptArray& other) -> ScriptArray&;
    auto operator==(const ScriptArray& other) const -> bool;

    void AddRef() const;
    void Release() const;

    auto GetArrayObjectType() -> AngelScript::asITypeInfo*;
    auto GetArrayObjectType() const -> const AngelScript::asITypeInfo*;
    auto GetArrayTypeId() const -> int32_t;
    auto GetElementTypeId() const -> int32_t;

    auto GetSize() const -> int32_t { return static_cast<int32_t>(_buffer.size() / _elementSize); }
    auto GetCapacity() const -> int32_t { return static_cast<int32_t>(_buffer.capacity() / _elementSize); }
    auto IsEmpty() const -> bool { return _buffer.empty(); }
    void Reserve(int32_t max_elements);
    void Resize(int32_t num_elements);
    auto At(int32_t index) const -> void*;
    void SetValue(int32_t index, void* value);
    void InsertAt(int32_t index, void* value);
    void InsertAt(int32_t index, const ScriptArray& other);
    void InsertLast(void* value);
    void RemoveAt(int32_t index);
    void RemoveLast();
    void RemoveRange(int32_t start, int32_t num_elements);
    void SortAsc();
    void SortDesc();
    void SortAsc(int32_t start_at, int32_t count);
    void SortDesc(int32_t start_at, int32_t count);
    void Sort(int32_t start_at, int32_t count, bool asc);
    void Reverse();
    auto Find(void* value) const -> int32_t;
    auto Find(int32_t start_at, void* value) const -> int32_t;
    auto FindByRef(void* ref) const -> int32_t;
    auto FindByRef(int32_t start_at, void* ref) const -> int32_t;
    auto GetBuffer() -> void* { return cast_to_void(_buffer.data()); }
    auto GetBuffer() const -> void* { return cast_to_void(_buffer.data()); }

    auto GetRefCount() const -> int32_t;
    void SetFlag() const;
    auto GetFlag() const -> bool;
    void EnumReferences(AngelScript::asIScriptEngine* engine);
    void ReleaseAllHandles(AngelScript::asIScriptEngine* engine);

private:
    explicit ScriptArray(AngelScript::asITypeInfo* ti, void* init_list);
    explicit ScriptArray(int32_t length, AngelScript::asITypeInfo* ti);
    explicit ScriptArray(int32_t length, void* def_val, AngelScript::asITypeInfo* ti);
    explicit ScriptArray(const ScriptArray& other);
    ~ScriptArray();

    auto GetArrayItemPointer(int32_t index) -> void*;
    auto GetArrayItemPointer(int32_t index) const -> void*;
    auto GetDataPointer(void* buf) const -> void*;
    void Copy(void* dst, void* src) const;
    void PrecacheSubTypeData();
    void CheckArraySize(int32_t num_elements) const;
    void Resize(int32_t delta, int32_t at);
    void CreateBuffer(int32_t num_elements);
    void DeleteBuffer() noexcept;
    void CopyBuffer(const ScriptArray& src);
    void Construct(int32_t start, int32_t end);
    void Destruct(int32_t start, int32_t end);
    auto Equals(void* a, void* b, AngelScript::asIScriptContext* ctx) const -> bool;
    auto Less(void* a, void* b, bool asc, AngelScript::asIScriptContext* ctx) const -> bool;

    refcount_ptr<AngelScript::asITypeInfo> _typeInfo;
    int32_t _subTypeId;
    int32_t _elementSize {};
    raw_ptr<ScriptArrayTypeData> _subTypeData {};
    vector<uint8_t> _buffer {};
    mutable int32_t _refCount {1};
    mutable bool _gcFlag {};
};

void RegisterAngelScriptArray(AngelScript::asIScriptEngine* as_engine);

FO_END_NAMESPACE

#endif
