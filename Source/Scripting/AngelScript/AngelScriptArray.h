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

#include "ScriptSystem.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE();

struct ScriptArrayTypeData;

class ScriptArray final
{
    friend class SafeAlloc;

public:
    static auto Create(AngelScript::asITypeInfo* ti) -> ScriptArray*;
    static auto Create(AngelScript::asITypeInfo* ti, int32 length) -> ScriptArray*;
    static auto Create(AngelScript::asITypeInfo* ti, int32 length, void* def_val) -> ScriptArray*;
    static auto Create(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptArray*;

    ScriptArray(ScriptArray&&) noexcept = delete;
    auto operator=(ScriptArray&&) noexcept -> ScriptArray& = delete;

    auto operator=(const ScriptArray& other) -> ScriptArray&;
    auto operator==(const ScriptArray& other) const -> bool;

    void AddRef() const;
    void Release() const;

    auto GetArrayObjectType() -> AngelScript::asITypeInfo*;
    auto GetArrayObjectType() const -> const AngelScript::asITypeInfo*;
    auto GetArrayTypeId() const -> int32;
    auto GetElementTypeId() const -> int32;

    auto GetSize() const -> int32 { return static_cast<int32>(_buffer.size() / _elementSize); }
    auto GetCapacity() const -> int32 { return static_cast<int32>(_buffer.capacity() / _elementSize); }
    auto IsEmpty() const -> bool { return _buffer.empty(); }
    void Reserve(int32 max_elements);
    void Resize(int32 num_elements);
    auto At(int32 index) -> void*;
    auto At(int32 index) const -> const void*;
    void SetValue(int32 index, void* value);
    void InsertAt(int32 index, void* value);
    void InsertAt(int32 index, const ScriptArray& other);
    void InsertLast(void* value);
    void RemoveAt(int32 index);
    void RemoveLast();
    void RemoveRange(int32 start, int32 num_elements);
    void SortAsc();
    void SortDesc();
    void SortAsc(int32 start_at, int32 count);
    void SortDesc(int32 start_at, int32 count);
    void Sort(int32 start_at, int32 count, bool asc);
    void Reverse();
    auto Find(void* value) const -> int32;
    auto Find(int32 start_at, void* value) const -> int32;
    auto FindByRef(void* ref) const -> int32;
    auto FindByRef(int32 start_at, void* ref) const -> int32;
    auto GetBuffer() -> void* { return _buffer.data(); }
    auto GetBuffer() const -> const void* { return _buffer.data(); }

    auto GetRefCount() const -> int32;
    void SetFlag() const;
    auto GetFlag() const -> bool;
    void EnumReferences(AngelScript::asIScriptEngine* engine);
    void ReleaseAllHandles(AngelScript::asIScriptEngine* engine);

private:
    explicit ScriptArray(AngelScript::asITypeInfo* ti, void* init_list);
    explicit ScriptArray(int32 length, AngelScript::asITypeInfo* ti);
    explicit ScriptArray(int32 length, void* def_val, AngelScript::asITypeInfo* ti);
    explicit ScriptArray(const ScriptArray& other);
    ~ScriptArray();

    auto GetArrayItemPointer(int32 index) -> void*;
    auto GetArrayItemPointer(int32 index) const -> const void*;
    auto GetDataPointer(void* buf) const -> void*;
    void Copy(void* dst, void* src) const;
    void PrecacheSubTypeData();
    void CheckArraySize(int32 num_elements) const;
    void Resize(int32 delta, int32 at);
    void CreateBuffer(int32 num_elements);
    void DeleteBuffer() noexcept;
    void CopyBuffer(const ScriptArray& src);
    void Construct(int32 start, int32 end);
    void Destruct(int32 start, int32 end);
    auto Equals(const void* a, const void* b, AngelScript::asIScriptContext* ctx) const -> bool;
    auto Less(const void* a, const void* b, bool asc, AngelScript::asIScriptContext* ctx) const -> bool;

    refcount_ptr<AngelScript::asITypeInfo> _typeInfo;
    int32 _subTypeId;
    int32 _elementSize {};
    raw_ptr<ScriptArrayTypeData> _subTypeData {};
    vector<uint8> _buffer {};
    mutable int32 _refCount {1};
    mutable bool _gcFlag {};
};

void RegisterAngelScriptArray(AngelScript::asIScriptEngine* engine);

FO_END_NAMESPACE();
