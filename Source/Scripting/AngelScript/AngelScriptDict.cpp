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

#include "AngelScriptDict.h"
#include "AngelScriptWrappedCall.h"

FO_BEGIN_NAMESPACE();

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
    FO_NO_STACK_TRACE_ENTRY();

    const auto* cache = static_cast<ScriptDictTypeData*>(type->GetUserData(AS_TYPE_DICT_CACHE));
    delete cache;
    type->SetUserData(nullptr, AS_TYPE_DICT_CACHE);
}

static auto CreateObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index) -> void*;
static auto CopyObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index, void* value) -> void*;
static void DestroyObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index, void* value);
static auto Less(int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, const void* a, const void* b) -> bool;
static auto Equals(int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, const void* a, const void* b) -> bool;
static auto Compare(bool check_less, int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, const void* a, const void* b) -> bool;

ScriptDict::ScriptDictComparator::ScriptDictComparator(ScriptDict* owner) :
    Owner {owner}
{
    FO_NO_STACK_TRACE_ENTRY();
}

auto ScriptDict::ScriptDictComparator::operator()(const void* a, const void* b) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return Less(Owner->_keyTypeId, Owner->_keyTypeData.get(), Owner->_typeInfo->GetEngine(), a, b);
}

auto ScriptDict::Create(AngelScript::asITypeInfo* ti) -> ScriptDict*
{
    FO_NO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptDict>(ti).release_ownership();
}

auto ScriptDict::Create(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptDict*
{
    FO_NO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptDict>(ti, init_list).release_ownership();
}

static auto CScriptDict_TemplateCallbackExt(AngelScript::asITypeInfo* ti, int32 sub_type_index, bool& dont_garbage_collect) -> bool
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

static auto CScriptDict_TemplateCallback(AngelScript::asITypeInfo* ti, bool& dont_garbage_collect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return CScriptDict_TemplateCallbackExt(ti, 0, dont_garbage_collect) && CScriptDict_TemplateCallbackExt(ti, 1, dont_garbage_collect);
}

ScriptDict::ScriptDict(AngelScript::asITypeInfo* ti) :
    _typeInfo {ti},
    _keyTypeId {_typeInfo->GetSubTypeId(0)},
    _valueTypeId {_typeInfo->GetSubTypeId(1)},
    _keyTypeData {PrecacheSubTypeData(_keyTypeId, _typeInfo->GetSubType(0))},
    _valueTypeData {PrecacheSubTypeData(_valueTypeId, _typeInfo->GetSubType(1))},
    _data {ScriptDictComparator(this)}
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    const auto* engine = ti->GetEngine();
    auto* buffer = static_cast<AngelScript::asBYTE*>(init_list);
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
    FO_NO_STACK_TRACE_ENTRY();

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        _typeInfo->GetEngine()->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }

    for (const auto& kv : other._data) {
        Set(kv.first, kv.second);
    }
}

auto ScriptDict::operator=(const ScriptDict& other) -> ScriptDict&
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    Clear();
}

auto ScriptDict::PrecacheSubTypeData(int32 type_id, AngelScript::asITypeInfo* ti) const -> ScriptDictTypeData*
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((type_id & ~AngelScript::asTYPEID_MASK_SEQNBR) == 0) {
        return nullptr;
    }

    auto* sub_type_data = static_cast<ScriptDictTypeData*>(ti->GetUserData(AS_TYPE_DICT_CACHE));

    if (sub_type_data != nullptr) {
        return sub_type_data;
    }

    AngelScript::asAcquireExclusiveLock();
    auto release_lock = ScopeCallback([]() noexcept { AngelScript::asReleaseExclusiveLock(); });

    sub_type_data = static_cast<ScriptDictTypeData*>(ti->GetUserData(AS_TYPE_DICT_CACHE));

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

    ti->SetUserData(sub_type_data, AS_TYPE_DICT_CACHE);
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
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        key = CopyObject(_typeInfo.get(), 0, key);
        value = CopyObject(_typeInfo.get(), 1, value);
        _data.emplace(key, value);
    }
    else {
        DestroyObject(_typeInfo.get(), 1, it->second);
        value = CopyObject(_typeInfo.get(), 1, value);
        it->second = value;
    }
}

void ScriptDict::SetIfNotExist(void* key, void* value)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        key = CopyObject(_typeInfo.get(), 0, key);
        value = CopyObject(_typeInfo.get(), 1, value);
        _data.emplace(key, value);
    }
}

auto ScriptDict::Remove(void* key) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    for (const auto& kv : _data) {
        DestroyObject(_typeInfo.get(), 0, kv.first);
        DestroyObject(_typeInfo.get(), 1, kv.second);
    }

    _data.clear();
}

auto ScriptDict::Get(void* key) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        throw ScriptException("Key not found");
    }

    return it->second;
}

void* ScriptDict::GetOrCreate(void* key)
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _data.find(key);

    if (it == _data.end()) {
        return def_val;
    }

    return it->second;
}

auto ScriptDict::GetKey(int32 index) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= numeric_cast<int32>(_data.size())) {
        throw ScriptException("Index out of bounds");
    }

    auto it = _data.begin();

    for (int32 i = 0; i < index; i++) {
        ++it;
    }

    return it->second;
}

auto ScriptDict::Exists(void* key) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _data.count(key) != 0;
}

auto ScriptDict::operator==(const ScriptDict& other) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(engine);
    Clear();
}

static auto CreateObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
        *static_cast<char*>(ptr) = *static_cast<char*>(value);
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT16 || sub_type_id == AngelScript::asTYPEID_UINT16) {
        *static_cast<int16*>(ptr) = *static_cast<int16*>(value);
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT32 || sub_type_id == AngelScript::asTYPEID_UINT32 || sub_type_id == AngelScript::asTYPEID_FLOAT || sub_type_id > AngelScript::asTYPEID_DOUBLE) { // enums have a type id larger than doubles
        *static_cast<int32*>(ptr) = *static_cast<int32*>(value);
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT64 || sub_type_id == AngelScript::asTYPEID_UINT64 || sub_type_id == AngelScript::asTYPEID_DOUBLE) {
        *static_cast<int64*>(ptr) = *static_cast<int64*>(value);
    }

    return ptr;
}

static void DestroyObject(AngelScript::asITypeInfo* obj_type, int32 sub_type_index, void* value)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto sub_type_id = obj_type->GetSubTypeId(sub_type_index);
    auto* engine = obj_type->GetEngine();

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (sub_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        engine->ReleaseScriptObject(value, engine->GetTypeInfoById(sub_type_id));
    }
    else {
        if ((sub_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0 && *static_cast<void**>(value) != nullptr) {
            engine->ReleaseScriptObject(*static_cast<void**>(value), engine->GetTypeInfoById(sub_type_id));
        }

        delete[] static_cast<uint8*>(value);
    }
}

static auto Less(int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, const void* a, const void* b) -> bool
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

static auto Equals(int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, const void* a, const void* b) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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

static auto Compare(bool check_less, int32 type_id, const ScriptDictTypeData* type_data, AngelScript::asIScriptEngine* engine, const void* a, const void* b) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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
                if (*static_cast<void* const*>(a) == *static_cast<void* const*>(b)) {
                    return true;
                }
            }
        }
        else {
            if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                if (*static_cast<void* const*>(a) == nullptr) {
                    return true;
                }
                if (*static_cast<void* const*>(b) == nullptr) {
                    return false;
                }
            }
        }

        auto* ctx = AngelScript::asGetActiveContext();
        FO_RUNTIME_ASSERT(ctx);
        FO_RUNTIME_ASSERT(ctx->GetEngine() == engine);

        int32 r = ctx->PushState();
        FO_RUNTIME_ASSERT(r >= 0);

        auto release_ctx = ScopeCallback([&]() noexcept {
            safe_call([&] {
                const auto state = ctx->GetState();
                ctx->PopState();

                if (state == AngelScript::asEXECUTION_ABORTED) {
                    ctx->Abort();
                }
            });
        });

        if (!check_less && type_data->EqFunc != nullptr) {
            r = ctx->Prepare(type_data->EqFunc.get_no_const());
            FO_RUNTIME_ASSERT(r >= 0);

            if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                r = ctx->SetObject(*static_cast<void* const*>(a));
                FO_RUNTIME_ASSERT(r >= 0);
                r = ctx->SetArgObject(0, *static_cast<void* const*>(b));
                FO_RUNTIME_ASSERT(r >= 0);
            }
            else {
                r = ctx->SetObject(const_cast<void**>(static_cast<void* const*>(a))); // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
                FO_RUNTIME_ASSERT(r >= 0);
                r = ctx->SetArgObject(0, const_cast<void**>(static_cast<void* const*>(b))); // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
                FO_RUNTIME_ASSERT(r >= 0);
            }

            r = ctx->Execute();

            if (r == AngelScript::asEXECUTION_FINISHED) {
                return ctx->GetReturnByte() != 0;
            }

            return false;
        }

        if (type_data->CmpFunc != nullptr) {
            r = ctx->Prepare(type_data->CmpFunc.get_no_const());
            FO_RUNTIME_ASSERT(r >= 0);

            if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                r = ctx->SetObject(*static_cast<void* const*>(a));
                FO_RUNTIME_ASSERT(r >= 0);
                r = ctx->SetArgObject(0, *static_cast<void* const*>(b));
                FO_RUNTIME_ASSERT(r >= 0);
            }
            else {
                r = ctx->SetObject(const_cast<void**>(static_cast<void* const*>(a))); // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
                FO_RUNTIME_ASSERT(r >= 0);
                r = ctx->SetArgObject(0, const_cast<void**>(static_cast<void* const*>(b))); // NOLINT(bugprone-multi-level-implicit-pointer-conversion)
                FO_RUNTIME_ASSERT(r >= 0);
            }

            r = ctx->Execute();

            if (r == AngelScript::asEXECUTION_FINISHED) {
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

static auto CScriptDict_Create(AngelScript::asITypeInfo* ti) -> ScriptDict*
{
    FO_NO_STACK_TRACE_ENTRY();

    return ScriptDict::Create(ti);
}

static auto CScriptDict_CreateList(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptDict*
{
    FO_NO_STACK_TRACE_ENTRY();

    return ScriptDict::Create(ti, init_list);
}

static auto ScriptDict_Factory(AngelScript::asITypeInfo* ti, const ScriptDict* other) -> ScriptDict*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Dict is null");
    }

    ScriptDict* clone = ScriptDict::Create(ti);
    *clone = *other;
    return clone;
}

static auto ScriptDict_Clone(const ScriptDict& dict) -> ScriptDict*
{
    FO_NO_STACK_TRACE_ENTRY();

    ScriptDict* clone = ScriptDict::Create(const_cast<AngelScript::asITypeInfo*>(dict.GetDictObjectType()));
    *clone = dict;
    return clone;
}

static auto ScriptDict_Equals(const ScriptDict& dict, const ScriptDict* other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Dict is null");
    }

    return dict == *other;
}

void RegisterAngelScriptDict(AngelScript::asIScriptEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    engine->SetTypeInfoUserDataCleanupCallback(CleanupTypeInfoDictCache, AS_TYPE_DICT_CACHE);

    int32 r = engine->RegisterObjectType("dict<class T1, class T2>", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_GC | AngelScript::asOBJ_TEMPLATE);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", SCRIPT_FUNC(CScriptDict_TemplateCallback), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_FACTORY, "dict<T1,T2>@ f(int&in)", SCRIPT_FUNC(CScriptDict_Create), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_FACTORY, "dict<T1,T2>@ f(int& in, const dict<T1,T2>@+)", SCRIPT_FUNC(ScriptDict_Factory), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_LIST_FACTORY, "dict<T1,T2>@ f(int&in type, int&in list) {repeat {T1, T2}}", SCRIPT_FUNC(CScriptDict_CreateList), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD(ScriptDict, AddRef), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD(ScriptDict, Release), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("dict<T1,T2>", "T2& opIndex(const T1&in)", SCRIPT_METHOD(ScriptDict, GetOrCreate), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "void set(const T1&in, const T2&in)", SCRIPT_METHOD(ScriptDict, Set), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "void setIfNotExist(const T1&in, const T2&in)", SCRIPT_METHOD(ScriptDict, SetIfNotExist), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "bool remove(const T1&in)", SCRIPT_METHOD(ScriptDict, Remove), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "int length() const", SCRIPT_METHOD(ScriptDict, GetSize), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "void clear()", SCRIPT_METHOD(ScriptDict, Clear), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "const T2& get(const T1&in) const", SCRIPT_METHOD(ScriptDict, Get), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "const T2& get(const T1&in, const T2&in) const", SCRIPT_METHOD(ScriptDict, GetDefault), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "const T1& getKey(int index) const", SCRIPT_METHOD(ScriptDict, GetKey), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "const T2& getValue(int index) const", SCRIPT_METHOD(ScriptDict, GetValue), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "bool exists(const T1&in) const", SCRIPT_METHOD(ScriptDict, Exists), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "bool isEmpty() const", SCRIPT_METHOD(ScriptDict, IsEmpty), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "dict<T1,T2>@ clone() const", SCRIPT_FUNC_THIS(ScriptDict_Clone), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "bool equals(const dict<T1,T2>@+) const", SCRIPT_FUNC_THIS(ScriptDict_Equals), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_GETREFCOUNT, "int f()", SCRIPT_METHOD(ScriptDict, GetRefCount), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_SETGCFLAG, "void f()", SCRIPT_METHOD(ScriptDict, SetFlag), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_GETGCFLAG, "bool f()", SCRIPT_METHOD(ScriptDict, GetFlag), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_ENUMREFS, "void f(int&in)", SCRIPT_METHOD(ScriptDict, EnumReferences), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_RELEASEREFS, "void f(int&in)", SCRIPT_METHOD(ScriptDict, ReleaseAllHandles), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
}

FO_END_NAMESPACE();
