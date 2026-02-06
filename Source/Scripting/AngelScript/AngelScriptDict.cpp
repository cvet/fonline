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

#include "AngelScriptDict.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptArray.h"
#include "AngelScriptHelpers.h"

FO_BEGIN_NAMESPACE

static constexpr AngelScript::asPWORD AS_TYPE_DICT_CACHE = 1010;

struct ScriptDictTypeData
{
    raw_ptr<AngelScript::asIScriptFunction> CmpFunc {};
    raw_ptr<AngelScript::asIScriptFunction> EqFunc {};
    int32 CmpFuncReturnCode {};
    int32 EqFuncReturnCode {};
};

static void CleanupTypeInfoDictCache(AngelScript::asITypeInfo* type)
{
    FO_STACK_TRACE_ENTRY();

    const auto* cache = cast_from_void<ScriptDictTypeData*>(type->GetUserData(AS_TYPE_DICT_CACHE));
    delete cache;
    type->SetUserData(nullptr, AS_TYPE_DICT_CACHE);
}

static auto CreateObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index) -> void*;
static auto CopyObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index, void* value) -> void*;
static void DestroyObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index, void* value);
static auto Less(int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, void* a, void* b) -> bool;
static auto Equals(int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, void* a, void* b) -> bool;
static auto Compare(bool check_less, int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, void* a, void* b) -> bool;

ScriptDict::ScriptDictComparator::ScriptDictComparator(ScriptDict* owner) :
    Owner {owner}
{
    FO_STACK_TRACE_ENTRY();
}

auto ScriptDict::ScriptDictComparator::operator()(void* a, void* b) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return Less(Owner->_keyTypeId, Owner->_keyTypeData.get(), Owner->_typeInfo->GetEngine(), a, b);
}

auto ScriptDict::Create(AngelScript::asITypeInfo* ti) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptDict>(ti).release_ownership();
}

auto ScriptDict::Create(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptDict>(ti, init_list).release_ownership();
}

static auto ScriptDict_TemplateCallbackExt(AngelScript::asITypeInfo* ti, int32 sub_type_index, bool& dont_garbage_collect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* engine = ti->GetEngine();
    const auto type_id = ti->GetSubTypeId(sub_type_index);

    if (type_id == AngelScript::asTYPEID_VOID) {
        return false;
    }

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        const auto* sub_type = engine->GetTypeInfoById(type_id);
        const auto flags = sub_type->GetFlags();

        if ((flags & AngelScript::asOBJ_VALUE) != 0 && (flags & AngelScript::asOBJ_POD) == 0) {
            bool found = false;

            for (AngelScript::asUINT i = 0; i < sub_type->GetBehaviourCount(); i++) {
                AngelScript::asEBehaviours beh;
                const auto* func = sub_type->GetBehaviourByIndex(i, &beh);

                if (beh != AngelScript::asBEHAVE_CONSTRUCT) {
                    continue;
                }

                if (func->GetParamCount() == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                engine->WriteMessage("dict", 0, 0, AngelScript::asMSGTYPE_ERROR, "The subtype has no default constructor");
                return false;
            }
        }
        else if ((flags & AngelScript::asOBJ_REF) != 0) {
            engine->WriteMessage("array", 0, 0, AngelScript::asMSGTYPE_ERROR, "Can't store references in dict");
        }

        if ((flags & AngelScript::asOBJ_GC) == 0) {
            dont_garbage_collect = true;
        }
    }
    else if ((type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        dont_garbage_collect = true;
    }
    else {
        FO_RUNTIME_ASSERT(type_id & AngelScript::asTYPEID_OBJHANDLE);
        const auto* sub_type = engine->GetTypeInfoById(type_id);
        const auto flags = sub_type->GetFlags();

        if ((flags & AngelScript::asOBJ_GC) == 0) {
            if ((flags & AngelScript::asOBJ_SCRIPT_OBJECT) != 0) {
                if ((flags & AngelScript::asOBJ_NOINHERIT) != 0) {
                    dont_garbage_collect = true;
                }
            }
            else {
                dont_garbage_collect = true;
            }
        }
    }

    return true;
}

static auto ScriptDict_TemplateCallback(AngelScript::asITypeInfo* ti, bool& dont_garbage_collect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return ScriptDict_TemplateCallbackExt(ti, 0, dont_garbage_collect) && ScriptDict_TemplateCallbackExt(ti, 1, dont_garbage_collect);
}

ScriptDict::ScriptDict(AngelScript::asITypeInfo* ti) :
    _typeInfo {ti},
    _keyTypeId {_typeInfo->GetSubTypeId(0)},
    _valueTypeId {_typeInfo->GetSubTypeId(1)},
    _keyTypeData {PrecacheSubTypeData(_keyTypeId, _typeInfo->GetSubType(0))},
    _valueTypeData {PrecacheSubTypeData(_valueTypeId, _typeInfo->GetSubType(1))},
    _data {ScriptDictComparator(this)}
{
    FO_STACK_TRACE_ENTRY();

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        _typeInfo->GetEngine()->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }
}

ScriptDict::ScriptDict(AngelScript::asITypeInfo* ti, void* init_list) :
    _typeInfo {ti},
    _keyTypeId {_typeInfo->GetSubTypeId(0)},
    _valueTypeId {_typeInfo->GetSubTypeId(1)},
    _keyTypeData {PrecacheSubTypeData(_keyTypeId, _typeInfo->GetSubType(0))},
    _valueTypeData {PrecacheSubTypeData(_valueTypeId, _typeInfo->GetSubType(1))},
    _data {ScriptDictComparator(this)}
{
    FO_STACK_TRACE_ENTRY();

    const auto* engine = ti->GetEngine();
    auto* buffer = cast_from_void<AngelScript::asBYTE*>(init_list);
    auto length = *reinterpret_cast<AngelScript::asUINT*>(buffer);
    buffer += sizeof(AngelScript::asUINT);

    while (length-- != 0) {
        if ((reinterpret_cast<AngelScript::asPWORD>(buffer) & 0x3) != 0) {
            buffer += 4 - (reinterpret_cast<AngelScript::asPWORD>(buffer) & 0x3);
        }

        void* key = buffer;
        if ((_keyTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
            const auto* ot2 = engine->GetTypeInfoById(_keyTypeId);

            if ((ot2->GetFlags() & AngelScript::asOBJ_VALUE) != 0) {
                buffer += ot2->GetSize();
            }
            else {
                buffer += sizeof(void*);
            }

            if ((ot2->GetFlags() & AngelScript::asOBJ_REF) != 0 && (_keyTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
                key = *static_cast<void**>(key);
            }
        }
        else if (_keyTypeId == AngelScript::asTYPEID_VOID) {
            buffer += sizeof(void*);
        }
        else {
            buffer += engine->GetSizeOfPrimitiveType(_keyTypeId);
        }

        void* value = buffer;
        if ((_valueTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
            const auto* ot2 = engine->GetTypeInfoById(_valueTypeId);

            if ((ot2->GetFlags() & AngelScript::asOBJ_VALUE) != 0) {
                buffer += ot2->GetSize();
            }
            else {
                buffer += sizeof(void*);
            }

            if ((ot2->GetFlags() & AngelScript::asOBJ_REF) != 0 && (_valueTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
                value = *static_cast<void**>(value);
            }
        }
        else if (_valueTypeId == AngelScript::asTYPEID_VOID) {
            buffer += sizeof(void*);
        }
        else {
            buffer += engine->GetSizeOfPrimitiveType(_valueTypeId);
        }

        Set(key, value);
    }

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        _typeInfo->GetEngine()->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }
}

ScriptDict::ScriptDict(const ScriptDict& other) :
    _typeInfo {other._typeInfo},
    _keyTypeId {_typeInfo->GetSubTypeId(0)},
    _valueTypeId {_typeInfo->GetSubTypeId(1)},
    _keyTypeData {PrecacheSubTypeData(_keyTypeId, _typeInfo->GetSubType(0))},
    _valueTypeData {PrecacheSubTypeData(_valueTypeId, _typeInfo->GetSubType(1))},
    _data {ScriptDictComparator(this)}
{
    FO_STACK_TRACE_ENTRY();

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        _typeInfo->GetEngine()->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }

    for (const auto& kv : other._data) {
        Set(kv.first, kv.second);
    }
}

auto ScriptDict::operator=(const ScriptDict& other) -> ScriptDict&
{
    FO_STACK_TRACE_ENTRY();

    if (other._typeInfo != _typeInfo) {
        throw ScriptException("Different types on dict assignment");
    }

    if (&other != this) {
        Clear();

        for (const auto& kv : other._data) {
            Set(kv.first, kv.second);
        }
    }

    return *this;
}

ScriptDict::~ScriptDict()
{
    FO_STACK_TRACE_ENTRY();

    Clear();
}

auto ScriptDict::PrecacheSubTypeData(int32 type_id, AngelScript::asITypeInfo* ti) const -> ScriptDictTypeData*
{
    FO_STACK_TRACE_ENTRY();

    if ((type_id & ~AngelScript::asTYPEID_MASK_SEQNBR) == 0) {
        return nullptr;
    }

    auto* sub_type_data = cast_from_void<ScriptDictTypeData*>(ti->GetUserData(AS_TYPE_DICT_CACHE));

    if (sub_type_data != nullptr) {
        return sub_type_data;
    }

    AngelScript::asAcquireExclusiveLock();
    auto release_lock = scope_exit([]() noexcept { AngelScript::asReleaseExclusiveLock(); });

    sub_type_data = cast_from_void<ScriptDictTypeData*>(ti->GetUserData(AS_TYPE_DICT_CACHE));

    if (sub_type_data != nullptr) {
        return sub_type_data;
    }

    sub_type_data = SafeAlloc::MakeRaw<ScriptDictTypeData>();

    const bool must_be_const = (type_id & AngelScript::asTYPEID_HANDLETOCONST) != 0;
    const auto* sub_type = ti->GetEngine()->GetTypeInfoById(type_id);

    if (sub_type != nullptr) {
        for (AngelScript::asUINT i = 0; i < sub_type->GetMethodCount(); i++) {
            auto* func = sub_type->GetMethodByIndex(i);

            if (func->GetParamCount() == 1 && (!must_be_const || func->IsReadOnly())) {
                AngelScript::asDWORD flags = 0;
                const int32 return_type_id = func->GetReturnTypeId(&flags);

                if (flags != AngelScript::asTM_NONE) {
                    continue;
                }

                bool is_cmp = false;
                bool is_eq = false;

                if (return_type_id == AngelScript::asTYPEID_INT32 && string_view(func->GetName()) == "opCmp") {
                    is_cmp = true;
                }
                if (return_type_id == AngelScript::asTYPEID_BOOL && string_view(func->GetName()) == "opEquals") {
                    is_eq = true;
                }

                if (!is_cmp && !is_eq) {
                    continue;
                }

                int32 param_type_id;
                func->GetParam(0, &param_type_id, &flags);

                if ((param_type_id & ~(AngelScript::asTYPEID_OBJHANDLE | AngelScript::asTYPEID_HANDLETOCONST)) != (type_id & ~(AngelScript::asTYPEID_OBJHANDLE | AngelScript::asTYPEID_HANDLETOCONST))) {
                    continue;
                }

                if ((flags & AngelScript::asTM_INREF) != 0) {
                    if ((param_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0 || (must_be_const && (flags & AngelScript::asTM_CONST) == 0)) {
                        continue;
                    }
                }
                else if ((param_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                    if (must_be_const && (param_type_id & AngelScript::asTYPEID_HANDLETOCONST) == 0) {
                        continue;
                    }
                }
                else {
                    continue;
                }

                if (is_cmp) {
                    if (sub_type_data->CmpFunc != nullptr || sub_type_data->CmpFuncReturnCode != 0) {
                        sub_type_data->CmpFunc = nullptr;
                        sub_type_data->CmpFuncReturnCode = AngelScript::asMULTIPLE_FUNCTIONS;
                    }
                    else {
                        sub_type_data->CmpFunc = func;
                    }
                }
                else if (is_eq) {
                    if (sub_type_data->EqFunc != nullptr || sub_type_data->EqFuncReturnCode != 0) {
                        sub_type_data->EqFunc = nullptr;
                        sub_type_data->EqFuncReturnCode = AngelScript::asMULTIPLE_FUNCTIONS;
                    }
                    else {
                        sub_type_data->EqFunc = func;
                    }
                }
            }
        }
    }

    if (sub_type_data->EqFunc == nullptr && sub_type_data->EqFuncReturnCode == 0) {
        sub_type_data->EqFuncReturnCode = AngelScript::asNO_FUNCTION;
    }
    if (sub_type_data->CmpFunc == nullptr && sub_type_data->CmpFuncReturnCode == 0) {
        sub_type_data->CmpFuncReturnCode = AngelScript::asNO_FUNCTION;
    }

    ti->SetUserData(cast_to_void(sub_type_data), AS_TYPE_DICT_CACHE);
    return sub_type_data;
}

auto ScriptDict::IsEmpty() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _data.empty();
}

auto ScriptDict::GetSize() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32>(_data.size());
}

void ScriptDict::Set(void* key, void* value)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        void* key_copy = CopyObject(_typeInfo.get(), 0, key);
        void* value_copy = CopyObject(_typeInfo.get(), 1, value);
        _data.emplace(key_copy, value_copy);
    }
    else {
        DestroyObject(_typeInfo.get(), 1, it->second);
        void* value_copy = CopyObject(_typeInfo.get(), 1, value);
        it->second = value_copy;
    }
}

void ScriptDict::SetIfNotExist(void* key, void* value)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        void* key_copy = CopyObject(_typeInfo.get(), 0, key);
        void* value_copy = CopyObject(_typeInfo.get(), 1, value);
        _data.emplace(key_copy, value_copy);
    }
}

auto ScriptDict::Remove(void* key) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it != _data.end()) {
        DestroyObject(_typeInfo.get(), 0, it->first);
        DestroyObject(_typeInfo.get(), 1, it->second);
        _data.erase(it);
        return true;
    }

    return false;
}

auto ScriptDict::RemoveValues(void* value) -> int32
{
    FO_STACK_TRACE_ENTRY();

    int32 result = 0;

    for (auto it = _data.begin(); it != _data.end();) {
        if (Equals(_valueTypeId, _valueTypeData.get(), _typeInfo->GetEngine(), it->second, value)) {
            DestroyObject(_typeInfo.get(), 0, it->first);
            DestroyObject(_typeInfo.get(), 1, it->second);
            it = _data.erase(it);
            result++;
        }
        else {
            ++it;
        }
    }

    return result;
}

void ScriptDict::Clear()
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& kv : _data) {
        DestroyObject(_typeInfo.get(), 0, kv.first);
        DestroyObject(_typeInfo.get(), 1, kv.second);
    }

    _data.clear();
}

auto ScriptDict::Get(void* key) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        throw ScriptException("Key not found");
    }

    return it->second;
}

void* ScriptDict::GetOrCreate(void* key)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        key = CopyObject(_typeInfo.get(), 0, key);
        void* value = CreateObject(_typeInfo.get(), 1);
        _data.emplace(key, value);
        return value;
    }

    return it->second;
}

auto ScriptDict::GetDefault(void* key, void* def_val) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        return def_val;
    }

    return it->second;
}

auto ScriptDict::GetKey(int32 index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= numeric_cast<int32>(_data.size())) {
        throw ScriptException("Index out of bounds");
    }

    auto it = _data.begin();

    for (int32 i = 0; i < index; i++) {
        ++it;
    }

    return it->first;
}

auto ScriptDict::GetValue(int32 index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= numeric_cast<int32>(_data.size())) {
        throw ScriptException("Index out of bounds");
    }

    auto it = _data.begin();

    for (int32 i = 0; i < index; i++) {
        ++it;
    }

    return it->second;
}

auto ScriptDict::GetKeys() const -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    const string arr_type_name = strex("{}[]", _typeInfo->GetSubType(0)->GetName());
    auto* arr_type = _typeInfo->GetEngine()->GetTypeInfoByName(arr_type_name.c_str());
    auto* arr = ScriptArray::Create(arr_type);

    arr->Reserve(GetSize());

    for (auto* key : _data | std::views::keys) {
        arr->InsertLast(key);
    }

    return arr;
}

auto ScriptDict::GetValues() const -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    const string arr_type_name = strex("{}[]", _typeInfo->GetSubType(1)->GetName());
    auto* arr_type = _typeInfo->GetEngine()->GetTypeInfoByName(arr_type_name.c_str());
    auto* arr = ScriptArray::Create(arr_type);

    arr->Reserve(GetSize());

    for (auto* value : _data | std::views::values) {
        arr->InsertLast(value);
    }

    return arr;
}

auto ScriptDict::Exists(void* key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _data.count(key) != 0;
}

auto ScriptDict::operator==(const ScriptDict& other) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (&other == this) {
        return true;
    }
    if (_typeInfo != other._typeInfo) {
        return false;
    }
    if (GetSize() != other.GetSize()) {
        return false;
    }

    auto it1 = _data.begin();
    auto it2 = other._data.begin();

    for (size_t i = 0; i > _data.size(); i++) {
        if (!Equals(_keyTypeId, _keyTypeData.get(), _typeInfo->GetEngine(), it1->first, it2->first)) {
            return false;
        }
        if (!Equals(_valueTypeId, _valueTypeData.get(), _typeInfo->GetEngine(), it1->second, it2->second)) {
            return false;
        }

        ++it1;
        ++it2;
    }

    return true;
}

void ScriptDict::AddRef() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag = false;
    AngelScript::asAtomicInc(_refCount);
}

void ScriptDict::Release() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag = false;

    if (AngelScript::asAtomicDec(_refCount) == 0) {
        delete this;
    }
}

auto ScriptDict::GetRefCount() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return _refCount;
}

void ScriptDict::SetFlag() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag = true;
}

bool ScriptDict::GetFlag() const
{
    FO_NO_STACK_TRACE_ENTRY();

    return _gcFlag;
}

void ScriptDict::EnumReferences(AngelScript::asIScriptEngine* engine) const
{
    FO_STACK_TRACE_ENTRY();

    const bool keys_handle = (_keyTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0;
    const bool values_handle = (_valueTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0;

    if (keys_handle || values_handle) {
        for (const auto& kv : _data) {
            if (keys_handle) {
                engine->GCEnumCallback(kv.first);
            }
            if (values_handle) {
                engine->GCEnumCallback(kv.second);
            }
        }
    }
}

void ScriptDict::ReleaseAllHandles(AngelScript::asIScriptEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(engine);
    Clear();
}

static auto CreateObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const auto sub_type_id = obj_type->GetSubTypeId(sub_type_index);
    const auto* sub_type = obj_type->GetSubType(sub_type_index);
    auto* engine = obj_type->GetEngine();

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (sub_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        return engine->CreateScriptObject(sub_type);
    }

    int32 element_size;

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        element_size = sizeof(AngelScript::asPWORD);
    }
    else {
        element_size = engine->GetSizeOfPrimitiveType(sub_type_id);
    }

    void* ptr = SafeAlloc::MakeRawArr<uint8>(element_size);
    MemFill(ptr, 0, element_size);
    return ptr;
}

static auto CopyObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index, void* value) -> void*
{
    FO_STACK_TRACE_ENTRY();

    const auto sub_type_id = obj_type->GetSubTypeId(sub_type_index);
    const auto* sub_type = obj_type->GetSubType(sub_type_index);
    auto* engine = obj_type->GetEngine();

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (sub_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        return engine->CreateScriptObjectCopy(value, sub_type);
    }

    int32 element_size;

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        element_size = sizeof(AngelScript::asPWORD);
    }
    else {
        element_size = engine->GetSizeOfPrimitiveType(sub_type_id);
    }

    void* ptr = SafeAlloc::MakeRawArr<uint8>(element_size);
    MemFill(ptr, 0, element_size);

    if ((sub_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        *static_cast<void**>(ptr) = *static_cast<void**>(value);

        if (*static_cast<void**>(value) != nullptr) {
            sub_type->GetEngine()->AddRefScriptObject(*static_cast<void**>(value), sub_type);
        }
    }
    else if (sub_type_id == AngelScript::asTYPEID_BOOL || sub_type_id == AngelScript::asTYPEID_INT8 || sub_type_id == AngelScript::asTYPEID_UINT8) {
        *cast_from_void<char*>(ptr) = *cast_from_void<const char*>(value);
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT16 || sub_type_id == AngelScript::asTYPEID_UINT16) {
        *cast_from_void<int16*>(ptr) = *cast_from_void<const int16*>(value);
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT32 || sub_type_id == AngelScript::asTYPEID_UINT32 || sub_type_id == AngelScript::asTYPEID_FLOAT || sub_type_id > AngelScript::asTYPEID_DOUBLE) { // enums have a type id larger than doubles
        *cast_from_void<int32*>(ptr) = *cast_from_void<const int32*>(value);
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT64 || sub_type_id == AngelScript::asTYPEID_UINT64 || sub_type_id == AngelScript::asTYPEID_DOUBLE) {
        *cast_from_void<int64*>(ptr) = *cast_from_void<const int64*>(value);
    }

    return ptr;
}

static void DestroyObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index, void* value)
{
    FO_STACK_TRACE_ENTRY();

    const auto sub_type_id = obj_type->GetSubTypeId(sub_type_index);
    auto* engine = obj_type->GetEngine();

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (sub_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        engine->ReleaseScriptObject(value, engine->GetTypeInfoById(sub_type_id));
    }
    else {
        if ((sub_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0 && *static_cast<void**>(value) != nullptr) {
            engine->ReleaseScriptObject(*static_cast<void**>(value), engine->GetTypeInfoById(sub_type_id));
        }

        delete[] cast_from_void<const uint8*>(value);
    }
}

static auto Less(int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, void* a, void* b) -> bool
{
    if (type_data != nullptr) {
        if (type_data->CmpFunc == nullptr) {
            const auto* sub_type = engine->GetTypeInfoById(type_id);

            if (type_data->CmpFuncReturnCode == AngelScript::asMULTIPLE_FUNCTIONS) {
                throw ScriptException("Type has multiple matching opCmp methods", sub_type->GetName());
            }
            else {
                throw ScriptException("Type does not have a matching opCmp method", sub_type->GetName());
            }
        }
    }

    return Compare(true, type_id, type_data, engine, a, b);
}

static auto Equals(int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, void* a, void* b) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (type_data != nullptr) {
        if (type_data->CmpFunc == nullptr && type_data->EqFunc == nullptr) {
            const auto* sub_type = engine->GetTypeInfoById(type_id);

            if (type_data->EqFuncReturnCode == AngelScript::asMULTIPLE_FUNCTIONS) {
                throw ScriptException("Type has multiple matching opEquals or opCmp methods", sub_type->GetName());
            }
            else {
                throw ScriptException("Type does not have a matching opEquals or opCmp method", sub_type->GetName());
            }
        }
    }

    return Compare(false, type_id, type_data, engine, a, b);
}

static auto Compare(bool check_less, int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, void* a, void* b) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) == 0) {
        if (check_less) {
            switch (type_id) {
#define COMPARE(T) *((T*)a) < *((T*)b)
            case AngelScript::asTYPEID_BOOL:
                return COMPARE(bool);
            case AngelScript::asTYPEID_INT8:
                return COMPARE(int8);
            case AngelScript::asTYPEID_UINT8:
                return COMPARE(uint8);
            case AngelScript::asTYPEID_INT16:
                return COMPARE(int16);
            case AngelScript::asTYPEID_UINT16:
                return COMPARE(uint16);
            case AngelScript::asTYPEID_INT32:
                return COMPARE(int32);
            case AngelScript::asTYPEID_UINT32:
                return COMPARE(uint32);
            case AngelScript::asTYPEID_INT64:
                return COMPARE(int64);
            case AngelScript::asTYPEID_UINT64:
                return COMPARE(uint64);
            case AngelScript::asTYPEID_FLOAT:
                return COMPARE(float32);
            case AngelScript::asTYPEID_DOUBLE:
                return COMPARE(float64);
            default:
                return COMPARE(int32); // Enums
#undef COMPARE
            }
        }
        else {
            switch (type_id) {
#define COMPARE(T) *((T*)a) == *((T*)b)
            case AngelScript::asTYPEID_BOOL:
                return COMPARE(bool);
            case AngelScript::asTYPEID_INT8:
                return COMPARE(int8);
            case AngelScript::asTYPEID_UINT8:
                return COMPARE(uint8);
            case AngelScript::asTYPEID_INT16:
                return COMPARE(int16);
            case AngelScript::asTYPEID_UINT16:
                return COMPARE(uint16);
            case AngelScript::asTYPEID_INT32:
                return COMPARE(int32);
            case AngelScript::asTYPEID_UINT32:
                return COMPARE(uint32);
            case AngelScript::asTYPEID_FLOAT:
                return COMPARE(float32);
            case AngelScript::asTYPEID_DOUBLE:
                return COMPARE(float64);
            default:
                return COMPARE(int32); // Enums
#undef COMPARE
            }
        }
    }
    else {
        if (!check_less) {
            if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                if (*static_cast<void**>(a) == *static_cast<void**>(b)) {
                    return true;
                }
            }
        }
        else {
            if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                if (*static_cast<void**>(a) == nullptr) {
                    return true;
                }
                if (*static_cast<void**>(b) == nullptr) {
                    return false;
                }
            }
        }

        auto* ctx = AngelScript::asGetActiveContext();
        FO_RUNTIME_ASSERT(ctx);
        FO_RUNTIME_ASSERT(ctx->GetEngine() == engine);

        int32 as_result = 0;
        FO_AS_VERIFY(ctx->PushState());

        auto release_ctx = scope_exit([&]() noexcept {
            safe_call([&] {
                const auto state = ctx->GetState();
                ctx->PopState();

                if (state == AngelScript::asEXECUTION_ABORTED) {
                    ctx->Abort();
                }
            });
        });

        if (!check_less && type_data->EqFunc != nullptr) {
            FO_AS_VERIFY(ctx->Prepare(type_data->EqFunc.get_no_const()));

            if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                FO_AS_VERIFY(ctx->SetObject(*static_cast<void**>(a)));
                FO_AS_VERIFY(ctx->SetArgObject(0, *static_cast<void**>(b)));
            }
            else {
                FO_AS_VERIFY(ctx->SetObject(a));
                FO_AS_VERIFY(ctx->SetArgObject(0, b));
            }

            FO_AS_VERIFY(ctx->Execute());

            if (as_result == AngelScript::asEXECUTION_FINISHED) {
                return ctx->GetReturnByte() != 0;
            }

            return false;
        }

        if (type_data->CmpFunc != nullptr) {
            FO_AS_VERIFY(ctx->Prepare(type_data->CmpFunc.get_no_const()));

            if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                FO_AS_VERIFY(ctx->SetObject(*static_cast<void**>(a)));
                FO_AS_VERIFY(ctx->SetArgObject(0, *static_cast<void**>(b)));
            }
            else {
                FO_AS_VERIFY(ctx->SetObject(a));
                FO_AS_VERIFY(ctx->SetArgObject(0, b));
            }

            FO_AS_VERIFY(ctx->Execute());

            if (as_result == AngelScript::asEXECUTION_FINISHED) {
                if (check_less) {
                    return static_cast<int32>(ctx->GetReturnDWord()) < 0;
                }
                else {
                    return static_cast<int32>(ctx->GetReturnDWord()) == 0;
                }
            }

            return false;
        }
    }

    return false;
}

static auto ScriptDict_Create(AngelScript::asITypeInfo* ti) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    return ScriptDict::Create(ti);
}

static auto ScriptDict_CreateList(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    return ScriptDict::Create(ti, init_list);
}

static auto ScriptDict_Factory(AngelScript::asITypeInfo* ti, const ScriptDict* other) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Dict is null");
    }

    ScriptDict* clone = ScriptDict::Create(ti);
    *clone = *other;
    return clone;
}

static auto ScriptDict_Clone(const ScriptDict& dict) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    ScriptDict* clone = ScriptDict::Create(const_cast<AngelScript::asITypeInfo*>(dict.GetDictObjectType()));
    *clone = dict;
    return clone;
}

static auto ScriptDict_Equals(const ScriptDict& dict, const ScriptDict* other) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Dict is null");
    }

    return dict == *other;
}

void RegisterAngelScriptDict(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    as_engine->SetTypeInfoUserDataCleanupCallback(CleanupTypeInfoDictCache, AS_TYPE_DICT_CACHE);

    int32 as_result = 0;
    FO_AS_VERIFY(as_engine->RegisterObjectType("dict<class T1, class T2>", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_GC | AngelScript::asOBJ_TEMPLATE));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", FO_SCRIPT_FUNC(ScriptDict_TemplateCallback), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_FACTORY, "dict<T1,T2>@ f(int&in)", FO_SCRIPT_FUNC(ScriptDict_Create), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_FACTORY, "dict<T1,T2>@ f(int&in, const dict<T1,T2>@+)", FO_SCRIPT_FUNC(ScriptDict_Factory), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_LIST_FACTORY, "dict<T1,T2>@ f(int&in type, int&in list) {repeat {T1, T2}}", FO_SCRIPT_FUNC(ScriptDict_CreateList), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_METHOD(ScriptDict, AddRef), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_METHOD(ScriptDict, Release), FO_SCRIPT_METHOD_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "T2& opIndex(const T1&in key)", FO_SCRIPT_METHOD(ScriptDict, GetOrCreate), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "void set(const T1&in, const T2&in key)", FO_SCRIPT_METHOD(ScriptDict, Set), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "void setIfNotExist(const T1&in key, const T2&in defVal)", FO_SCRIPT_METHOD(ScriptDict, SetIfNotExist), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "bool remove(const T1&in key)", FO_SCRIPT_METHOD(ScriptDict, Remove), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "int32 removeValues(const T1&in value)", FO_SCRIPT_METHOD(ScriptDict, RemoveValues), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "int length() const", FO_SCRIPT_METHOD(ScriptDict, GetSize), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "void clear()", FO_SCRIPT_METHOD(ScriptDict, Clear), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T2& get(const T1&in key) const", FO_SCRIPT_METHOD(ScriptDict, Get), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T2& get(const T1&in key, const T2&in defVal) const", FO_SCRIPT_METHOD(ScriptDict, GetDefault), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T1& getKey(int index) const", FO_SCRIPT_METHOD(ScriptDict, GetKey), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T2& getValue(int index) const", FO_SCRIPT_METHOD(ScriptDict, GetValue), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "T1[]@ getKeys() const", FO_SCRIPT_METHOD(ScriptDict, GetKeys), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "T2[]@ getValues() const", FO_SCRIPT_METHOD(ScriptDict, GetValues), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "bool exists(const T1&in key) const", FO_SCRIPT_METHOD(ScriptDict, Exists), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "bool isEmpty() const", FO_SCRIPT_METHOD(ScriptDict, IsEmpty), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "dict<T1,T2>@ clone() const", FO_SCRIPT_FUNC_THIS(ScriptDict_Clone), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "bool equals(const dict<T1,T2>@+ other) const", FO_SCRIPT_FUNC_THIS(ScriptDict_Equals), FO_SCRIPT_FUNC_THIS_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_GETREFCOUNT, "int f()", FO_SCRIPT_METHOD(ScriptDict, GetRefCount), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_SETGCFLAG, "void f()", FO_SCRIPT_METHOD(ScriptDict, SetFlag), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_GETGCFLAG, "bool f()", FO_SCRIPT_METHOD(ScriptDict, GetFlag), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_ENUMREFS, "void f(int&in)", FO_SCRIPT_METHOD(ScriptDict, EnumReferences), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_RELEASEREFS, "void f(int&in)", FO_SCRIPT_METHOD(ScriptDict, ReleaseAllHandles), FO_SCRIPT_METHOD_CONV));
}

FO_END_NAMESPACE

#endif
