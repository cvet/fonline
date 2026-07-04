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
    class asIScriptEngine;
    class asITypeInfo;
}

FO_BEGIN_NAMESPACE

class ScriptArray;
struct ScriptDictTypeData;

class ScriptDict
{
    friend class SafeAlloc;

public:
    struct ScriptDictComparator
    {
        explicit ScriptDictComparator(ptr<ScriptDict> owner);
        auto operator()(ptr<void> a, ptr<void> b) const -> bool;
        ptr<ScriptDict> Owner;
    };

    static auto Create(ptr<AngelScript::asITypeInfo> ti) -> refcount_ptr<ScriptDict>;
    static auto Create(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list) -> refcount_ptr<ScriptDict>;

    ScriptDict(ScriptDict&&) noexcept = delete;
    auto operator=(ScriptDict&&) noexcept -> ScriptDict& = delete;

    auto operator=(const ScriptDict& other) -> ScriptDict&;
    auto operator==(const ScriptDict& other) const -> bool;

    void AddRef() const;
    void Release() const;

    auto GetDictObjectType() -> ptr<AngelScript::asITypeInfo>
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _typeInfo;
    }
    auto GetDictObjectType() const -> ptr<const AngelScript::asITypeInfo>
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _typeInfo;
    }
    auto GetDictTypeId() const -> int32_t;
    auto GetMap() const -> ptr<const map<void*, void*, ScriptDictComparator>> { return &_data; }
    auto IsEmpty() const -> bool;
    auto GetSize() const -> int32_t;
    auto Get(ptr<void> key) const -> ptr<void>;
    auto GetOrCreate(ptr<void> key) -> ptr<void>;
    auto GetDefault(ptr<void> key, ptr<void> def_val) const -> ptr<void>;
    auto GetKey(int32_t index) const -> ptr<void>;
    auto GetValue(int32_t index) const -> ptr<void>;
    auto GetKeys() const -> refcount_ptr<ScriptArray>;
    auto GetValues() const -> refcount_ptr<ScriptArray>;
    auto Exists(ptr<void> key) const -> bool;

    void Set(ptr<void> key, ptr<void> value);
    void SetIfNotExist(ptr<void> key, ptr<void> value);
    auto Remove(ptr<void> key) -> bool;
    auto RemoveValues(ptr<void> value) -> int32_t;
    void Clear();

    auto GetRefCount() const -> int32_t;
    void SetFlag() const;
    auto GetFlag() const -> bool;
    void EnumReferences(ptr<AngelScript::asIScriptEngine> engine) const;
    void ReleaseAllHandles();

private:
    explicit ScriptDict(ptr<AngelScript::asITypeInfo> ti);
    explicit ScriptDict(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list);
    explicit ScriptDict(const ScriptDict& other);
    ~ScriptDict();

    auto PrecacheSubTypeData(int32_t type_id, nptr<AngelScript::asITypeInfo> nullable_ti) const -> nptr<ScriptDictTypeData>;

    refcount_ptr<AngelScript::asITypeInfo> _typeInfo;
    int32_t _keyTypeId;
    int32_t _valueTypeId;
    nptr<ScriptDictTypeData> _keyTypeData;
    nptr<ScriptDictTypeData> _valueTypeData;
    map<void*, void*, ScriptDictComparator> _data;
    mutable std::atomic<int32_t> _refCount {1};
    mutable std::atomic<bool> _gcFlag {};
};

void RegisterAngelScriptDict(ptr<AngelScript::asIScriptEngine> as_engine);

FO_END_NAMESPACE

#endif
