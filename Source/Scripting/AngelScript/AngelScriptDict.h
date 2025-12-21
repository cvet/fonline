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

FO_DECLARE_EXCEPTION(ScriptDictException);

struct ScriptDictTypeData;

class ScriptDict
{
    friend class SafeAlloc;

public:
    struct ScriptDictComparator
    {
        explicit ScriptDictComparator(ScriptDict* owner);
        auto operator()(const void* a, const void* b) const -> bool;
        raw_ptr<ScriptDict> Owner;
    };

    static auto Create(AngelScript::asITypeInfo* ti) -> ScriptDict*;
    static auto Create(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptDict*;

    ScriptDict(ScriptDict&&) noexcept = delete;
    auto operator=(ScriptDict&&) noexcept -> ScriptDict& = delete;

    auto operator=(const ScriptDict& other) -> ScriptDict&;
    auto operator==(const ScriptDict& other) const -> bool;

    void AddRef() const;
    void Release() const;

    auto GetDictObjectType() -> AngelScript::asITypeInfo* { return _typeInfo.get(); }
    auto GetDictObjectType() const -> const AngelScript::asITypeInfo* { return _typeInfo.get(); }
    auto GetDictTypeId() const -> int32 { return _typeInfo->GetTypeId(); }
    auto GetMap() const -> const map<void*, void*, ScriptDictComparator>& { return _data; }
    auto IsEmpty() const -> bool;
    auto GetSize() const -> int32;
    auto Get(void* key) -> void*;
    auto GetOrCreate(void* key) -> void*;
    auto GetDefault(void* key, void* def_val) -> void*;
    auto GetKey(int32 index) -> void*;
    auto GetValue(int32 index) -> void*;
    auto Exists(void* key) const -> bool;

    void Set(void* key, void* value);
    void SetIfNotExist(void* key, void* value);
    auto Remove(void* key) -> bool;
    auto RemoveValues(void* value) -> int32;
    void Clear();

    auto GetRefCount() const -> int32;
    void SetFlag() const;
    auto GetFlag() const -> bool;
    void EnumReferences(AngelScript::asIScriptEngine* engine) const;
    void ReleaseAllHandles(AngelScript::asIScriptEngine* engine);

private:
    explicit ScriptDict(AngelScript::asITypeInfo* ti);
    explicit ScriptDict(AngelScript::asITypeInfo* ti, void* init_list);
    explicit ScriptDict(const ScriptDict& other);
    ~ScriptDict();

    auto PrecacheSubTypeData(int32 type_id, AngelScript::asITypeInfo* ti) const -> ScriptDictTypeData*;

    refcount_ptr<AngelScript::asITypeInfo> _typeInfo;
    int32 _keyTypeId;
    int32 _valueTypeId;
    raw_ptr<ScriptDictTypeData> _keyTypeData;
    raw_ptr<ScriptDictTypeData> _valueTypeData;
    map<void*, void*, ScriptDictComparator> _data;
    mutable int32 _refCount {1};
    mutable bool _gcFlag {};
};

void RegisterAngelScriptDict(AngelScript::asIScriptEngine* engine);

FO_END_NAMESPACE();
