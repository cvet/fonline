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

#include <angelscript.h>

FO_BEGIN_NAMESPACE

static constexpr AngelScript::asPWORD AS_TYPE_DICT_CACHE = 1010;

struct ScriptDictTypeData
{
    nptr<AngelScript::asIScriptFunction> CmpFunc {};
    nptr<AngelScript::asIScriptFunction> EqFunc {};
    int32_t CmpFuncReturnCode {};
    int32_t EqFuncReturnCode {};
    ScriptFastCompareFunc FastCompare {};
};

struct ScriptDictInitListValueLayout
{
    size_t Size {};
    bool ReadAsRefSlot {};
};

static auto ScriptDictBufferAsVoid(ptr<AngelScript::asBYTE> buffer) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    return buffer.void_cast();
}

static void AdvanceScriptDictBuffer(ptr<AngelScript::asBYTE>& buffer, size_t offset) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    buffer = buffer.offset(offset);
}

template<typename T>
static auto ReadScriptDictBufferValue(ptr<AngelScript::asBYTE> buffer) noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::is_trivially_copyable_v<T>);

    return *cast_from_void<T*>(ScriptDictBufferAsVoid(buffer).get());
}

static auto GetScriptDictObjectType(ptr<AngelScript::asIScriptEngine> engine, int32_t type_id) -> ptr<AngelScript::asITypeInfo>
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> obj_type = engine->GetTypeInfoById(type_id);
    FO_VERIFY_AND_THROW(obj_type, "Dictionary object type info not found");
    return obj_type;
}

static auto GetScriptDictInitListValueLayout(ptr<AngelScript::asIScriptEngine> engine, int32_t type_id) -> ScriptDictInitListValueLayout
{
    FO_STACK_TRACE_ENTRY();

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        auto obj_type = GetScriptDictObjectType(engine, type_id);
        auto flags = obj_type->GetFlags();

        return {(flags & AngelScript::asOBJ_VALUE) != 0 ? numeric_cast<size_t>(obj_type->GetSize()) : sizeof(void*), (flags & AngelScript::asOBJ_REF) != 0 && (type_id & AngelScript::asTYPEID_OBJHANDLE) == 0};
    }

    if (type_id == AngelScript::asTYPEID_VOID) {
        return {sizeof(void*), false};
    }

    return {numeric_cast<size_t>(engine->GetSizeOfPrimitiveType(type_id)), false};
}

static auto ListElementAlignment(int32_t type_id, size_t element_size) noexcept -> AngelScript::asPWORD
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
            return 4;
        }

        return element_size >= 8 ? 8 : 4;
    }

    if (type_id == AngelScript::asTYPEID_VOID) {
        return 4;
    }

    return element_size >= 8 ? 8 : 4;
}

static void AlignScriptDictInitListBuffer(ptr<AngelScript::asBYTE>& buffer, int32_t type_id, size_t value_size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (value_size < 4) {
        return;
    }

    // Align the buffer cursor to the element's natural alignment (matching the VM list-buffer layout)
    AngelScript::asPWORD element_align = ListElementAlignment(type_id, value_size);
    AngelScript::asPWORD buffer_address = std::bit_cast<AngelScript::asPWORD>(buffer.get());
    size_t misalignment = numeric_cast<size_t>(buffer_address & (element_align - 1));

    if (misalignment != 0) {
        AdvanceScriptDictBuffer(buffer, numeric_cast<size_t>(element_align) - misalignment);
    }
}

static auto ReadScriptDictInitListEntry(ptr<AngelScript::asIScriptEngine> engine, int32_t type_id, ptr<AngelScript::asBYTE>& buffer) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    ScriptDictInitListValueLayout layout = GetScriptDictInitListValueLayout(engine, type_id);

    AlignScriptDictInitListBuffer(buffer, type_id, layout.Size);

    auto value = ScriptDictBufferAsVoid(buffer);
    AdvanceScriptDictBuffer(buffer, layout.Size);

    if (layout.ReadAsRefSlot) {
        auto ref_value = NativeDataProvider::ReadHandleSlot(value);
        FO_VERIFY_AND_THROW(ref_value, "Dictionary init list reference value is null");
        return ref_value;
    }

    return value;
}

static void CleanupScriptDictTypeData(ptr<ScriptDictTypeData> cache) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owned_cache = adopt_unique_ptr(cache);
    ignore_unused(owned_cache);
}

static void CleanupTypeInfoDictCache(AngelScript::asITypeInfo* type)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asITypeInfo> type_info = type;
    auto cache = cast_from_void<ScriptDictTypeData*>(type_info->GetUserData(AS_TYPE_DICT_CACHE));
    if (cache) {
        CleanupScriptDictTypeData(cache);
    }
    type_info->SetUserData(nullptr, AS_TYPE_DICT_CACHE);
}

static auto CreateObject(ptr<AngelScript::asITypeInfo> obj_type, int32_t sub_type_index) -> ptr<void>;
static auto CopyObject(ptr<AngelScript::asITypeInfo> obj_type, int32_t sub_type_index, ptr<void> value) -> ptr<void>;
static void DestroyObject(ptr<AngelScript::asITypeInfo> obj_type, int32_t sub_type_index, ptr<void> value);
static auto Less(int32_t type_id, nptr<const ScriptDictTypeData> type_data, ptr<AngelScript::asIScriptEngine> engine, ptr<void> a, ptr<void> b) -> bool;
static auto Equals(int32_t type_id, nptr<const ScriptDictTypeData> type_data, ptr<AngelScript::asIScriptEngine> engine, ptr<void> a, ptr<void> b) -> bool;
static auto Compare(bool check_less, int32_t type_id, nptr<const ScriptDictTypeData> type_data, ptr<AngelScript::asIScriptEngine> engine, ptr<void> a, ptr<void> b) -> bool;

ScriptDict::ScriptDictComparator::ScriptDictComparator(ptr<ScriptDict> owner) :
    Owner {owner}
{
    FO_STACK_TRACE_ENTRY();
}

auto ScriptDict::ScriptDictComparator::operator()(ptr<void> a, ptr<void> b) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return Less(Owner->_keyTypeId, Owner->_keyTypeData, Owner->_typeInfo->GetEngine(), a, b);
}

auto ScriptDict::Create(ptr<AngelScript::asITypeInfo> ti) -> refcount_ptr<ScriptDict>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptDict>(ti);
}

auto ScriptDict::Create(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list) -> refcount_ptr<ScriptDict>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptDict>(ti, init_list);
}

auto ScriptDict::GetDictTypeId() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo->GetTypeId();
}

static auto ScriptDict_TemplateCallbackExt(AngelScript::asITypeInfo* ti, int32_t sub_type_index, bool& dont_garbage_collect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ti, "Dictionary type info is null");
    ptr<AngelScript::asIScriptEngine> engine = ti->GetEngine();
    int32_t type_id = ti->GetSubTypeId(sub_type_index);

    if (type_id == AngelScript::asTYPEID_VOID) {
        return false;
    }

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        nptr<const AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(type_id);
        FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
        auto flags = sub_type->GetFlags();

        if ((flags & AngelScript::asOBJ_VALUE) != 0 && (flags & AngelScript::asOBJ_POD) == 0) {
            bool found = false;

            for (AngelScript::asUINT i = 0; i < sub_type->GetBehaviourCount(); i++) {
                AngelScript::asEBehaviours beh;
                nptr<const AngelScript::asIScriptFunction> func = sub_type->GetBehaviourByIndex(i, &beh);

                if (!func || beh != AngelScript::asBEHAVE_CONSTRUCT) {
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
        FO_VERIFY_AND_THROW(type_id & AngelScript::asTYPEID_OBJHANDLE, "AngelScript dictionary key type is not an object handle", type_id);
        nptr<const AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(type_id);
        FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
        auto flags = sub_type->GetFlags();

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

    bool key_dont_garbage_collect = false;
    bool value_dont_garbage_collect = false;

    if (!ScriptDict_TemplateCallbackExt(ti, 0, key_dont_garbage_collect)) {
        return false;
    }
    if (!ScriptDict_TemplateCallbackExt(ti, 1, value_dont_garbage_collect)) {
        return false;
    }

    dont_garbage_collect = key_dont_garbage_collect && value_dont_garbage_collect;
    return true;
}

static auto GetDictSubTypeForPrecache(ptr<AngelScript::asITypeInfo> type_info, AngelScript::asUINT index) -> nptr<AngelScript::asITypeInfo>
{
    FO_NO_STACK_TRACE_ENTRY();

    return type_info->GetSubType(index);
}

ScriptDict::ScriptDict(ptr<AngelScript::asITypeInfo> ti) :
    _typeInfo {refcount_ptr<AngelScript::asITypeInfo>::from_add_ref(ti.get())},
    _keyTypeId {ti->GetSubTypeId(0)},
    _valueTypeId {ti->GetSubTypeId(1)},
    _keyTypeData {PrecacheSubTypeData(_keyTypeId, GetDictSubTypeForPrecache(ti, 0))},
    _valueTypeData {PrecacheSubTypeData(_valueTypeId, GetDictSubTypeForPrecache(ti, 1))},
    _data {ScriptDictComparator(make_ptr(this))}
{
    FO_STACK_TRACE_ENTRY();

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
        engine->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }
}

ScriptDict::ScriptDict(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list) :
    _typeInfo {refcount_ptr<AngelScript::asITypeInfo>::from_add_ref(ti.get())},
    _keyTypeId {ti->GetSubTypeId(0)},
    _valueTypeId {ti->GetSubTypeId(1)},
    _keyTypeData {PrecacheSubTypeData(_keyTypeId, GetDictSubTypeForPrecache(ti, 0))},
    _valueTypeData {PrecacheSubTypeData(_valueTypeId, GetDictSubTypeForPrecache(ti, 1))},
    _data {ScriptDictComparator(make_ptr(this))}
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> engine = ti->GetEngine();
    auto buffer = init_list.reinterpret_as<AngelScript::asBYTE>();
    AngelScript::asUINT length = ReadScriptDictBufferValue<AngelScript::asUINT>(buffer);
    AdvanceScriptDictBuffer(buffer, sizeof(AngelScript::asUINT));

    while (length-- != 0) {
        auto key = ReadScriptDictInitListEntry(engine, _keyTypeId, buffer);
        auto value = ReadScriptDictInitListEntry(engine, _valueTypeId, buffer);
        Set(key, value);
    }

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        engine->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }
}

ScriptDict::ScriptDict(const ScriptDict& other) :
    _typeInfo {other._typeInfo},
    _keyTypeId {_typeInfo->GetSubTypeId(0)},
    _valueTypeId {_typeInfo->GetSubTypeId(1)},
    _keyTypeData {PrecacheSubTypeData(_keyTypeId, GetDictSubTypeForPrecache(_typeInfo, 0))},
    _valueTypeData {PrecacheSubTypeData(_valueTypeId, GetDictSubTypeForPrecache(_typeInfo, 1))},
    _data {ScriptDictComparator(make_ptr(this))}
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& kv : other._data) {
        Set(kv.first, kv.second);
    }

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
        engine->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
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

auto ScriptDict::PrecacheSubTypeData(int32_t type_id, nptr<AngelScript::asITypeInfo> ti) const -> nptr<ScriptDictTypeData>
{
    FO_STACK_TRACE_ENTRY();

    if ((type_id & ~AngelScript::asTYPEID_MASK_SEQNBR) == 0) {
        return nullptr;
    }

    FO_VERIFY_AND_THROW(ti, "Dictionary sub-type info is null");
    auto cached_sub_type_data = cast_from_void<ScriptDictTypeData*>(ti->GetUserData(AS_TYPE_DICT_CACHE));

    if (cached_sub_type_data) {
        return cached_sub_type_data;
    }

    AngelScript::asAcquireExclusiveLock();
    auto release_lock = scope_exit([]() noexcept { AngelScript::asReleaseExclusiveLock(); });

    cached_sub_type_data = cast_from_void<ScriptDictTypeData*>(ti->GetUserData(AS_TYPE_DICT_CACHE));

    if (cached_sub_type_data) {
        return cached_sub_type_data;
    }

    auto sub_type_data = SafeAlloc::MakeUnique<ScriptDictTypeData>();

    bool must_be_const = (type_id & AngelScript::asTYPEID_HANDLETOCONST) != 0;
    ptr<AngelScript::asIScriptEngine> engine = ti->GetEngine();
    nptr<const AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(type_id);

    if (sub_type) {
        // Native fast comparator (stored in the sub-type user data) bypasses the script VM dispatch for known value types.
        sub_type_data->FastCompare = GetScriptTypeFastCompare(sub_type);

        for (AngelScript::asUINT i = 0; i < sub_type->GetMethodCount(); i++) {
            nptr<AngelScript::asIScriptFunction> func = sub_type->GetMethodByIndex(i);

            if (!func) {
                continue;
            }

            if (func->GetParamCount() != 1 || (must_be_const && !func->IsReadOnly())) {
                continue;
            }

            AngelScript::asDWORD flags = 0;
            int32_t return_type_id = func->GetReturnTypeId(&flags);

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

            int32_t param_type_id;
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
                if (sub_type_data->CmpFunc || sub_type_data->CmpFuncReturnCode != AngelScript::asSUCCESS) {
                    sub_type_data->CmpFunc.reset();
                    sub_type_data->CmpFuncReturnCode = AngelScript::asMULTIPLE_FUNCTIONS;
                }
                else {
                    sub_type_data->CmpFunc = func;
                }
            }
            else if (is_eq) {
                if (sub_type_data->EqFunc || sub_type_data->EqFuncReturnCode != AngelScript::asSUCCESS) {
                    sub_type_data->EqFunc.reset();
                    sub_type_data->EqFuncReturnCode = AngelScript::asMULTIPLE_FUNCTIONS;
                }
                else {
                    sub_type_data->EqFunc = func;
                }
            }
        }
    }

    if (!sub_type_data->EqFunc && sub_type_data->EqFuncReturnCode == AngelScript::asSUCCESS) {
        sub_type_data->EqFuncReturnCode = AngelScript::asNO_FUNCTION;
    }
    if (!sub_type_data->CmpFunc && sub_type_data->CmpFuncReturnCode == AngelScript::asSUCCESS) {
        sub_type_data->CmpFuncReturnCode = AngelScript::asNO_FUNCTION;
    }

    auto released_sub_type_data = sub_type_data.release();
    ti->SetUserData(released_sub_type_data.void_cast(), AS_TYPE_DICT_CACHE);
    return released_sub_type_data;
}

auto ScriptDict::IsEmpty() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _data.empty();
}

auto ScriptDict::GetSize() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32_t>(_data.size());
}

void ScriptDict::Set(ptr<void> key, ptr<void> value)
{
    FO_STACK_TRACE_ENTRY();

    auto it = _data.find(key.get());

    if (it == _data.end()) {
        auto key_copy = CopyObject(_typeInfo, 0, key);
        auto value_copy = CopyObject(_typeInfo, 1, value);
        _data.emplace(key_copy.get(), value_copy.get());
    }
    else {
        auto value_copy = CopyObject(_typeInfo, 1, value);
        ptr<void> old_value = it->second;
        it->second = value_copy.get();
        DestroyObject(_typeInfo, 1, old_value);
    }
}

void ScriptDict::SetIfNotExist(ptr<void> key, ptr<void> value)
{
    FO_STACK_TRACE_ENTRY();

    auto it = _data.find(key.get());

    if (it == _data.end()) {
        auto key_copy = CopyObject(_typeInfo, 0, key);
        auto value_copy = CopyObject(_typeInfo, 1, value);
        _data.emplace(key_copy.get(), value_copy.get());
    }
}

auto ScriptDict::Remove(ptr<void> key) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto it = _data.find(key.get());

    if (it != _data.end()) {
        ptr<void> key_obj = it->first;
        ptr<void> value_obj = it->second;
        _data.erase(it);
        DestroyObject(_typeInfo, 0, key_obj);
        DestroyObject(_typeInfo, 1, value_obj);
        return true;
    }

    return false;
}

auto ScriptDict::RemoveValues(ptr<void> value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    int32_t result = 0;

    for (auto it = _data.begin(); it != _data.end();) {
        if (Equals(_valueTypeId, _valueTypeData, _typeInfo->GetEngine(), it->second, value)) {
            ptr<void> key_obj = it->first;
            ptr<void> value_obj = it->second;
            it = _data.erase(it);
            DestroyObject(_typeInfo, 0, key_obj);
            DestroyObject(_typeInfo, 1, value_obj);
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

    while (!_data.empty()) {
        auto node = _data.begin();
        ptr<void> key = node->first;
        ptr<void> value = node->second;
        _data.erase(node);
        DestroyObject(_typeInfo, 0, key);
        DestroyObject(_typeInfo, 1, value);
    }
}

auto ScriptDict::Get(ptr<void> key) const -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    auto it = _data.find(key.get());

    if (it == _data.end()) {
        throw ScriptException("Key not found");
    }

    return it->second;
}

auto ScriptDict::GetOrCreate(ptr<void> key) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    auto it = _data.find(key.get());

    if (it == _data.end()) {
        auto key_copy = CopyObject(_typeInfo, 0, key);
        auto value = CreateObject(_typeInfo, 1);
        _data.emplace(key_copy.get(), value.get());
        return value;
    }

    return it->second;
}

auto ScriptDict::GetDefault(ptr<void> key, ptr<void> def_val) const -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    auto it = _data.find(key.get());

    if (it == _data.end()) {
        return def_val;
    }

    return it->second;
}

auto ScriptDict::GetKey(int32_t index) const -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= numeric_cast<int32_t>(_data.size())) {
        throw ScriptException("Index out of bounds");
    }

    auto it = _data.begin();

    for (int32_t i = 0; i < index; i++) {
        ++it;
    }

    return it->first;
}

auto ScriptDict::GetValue(int32_t index) const -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= numeric_cast<int32_t>(_data.size())) {
        throw ScriptException("Index out of bounds");
    }

    auto it = _data.begin();

    for (int32_t i = 0; i < index; i++) {
        ++it;
    }

    return it->second;
}

auto ScriptDict::GetKeys() const -> refcount_ptr<ScriptArray>
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> sub_type = _typeInfo->GetSubType(0);
    FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
    string arr_type_name = strex("{}{}[]", sub_type->GetName(), (_keyTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0 ? "@" : "");
    ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
    auto arr_type = make_nptr(engine->GetTypeInfoByDecl(arr_type_name.c_str()));
    FO_VERIFY_AND_THROW(arr_type, "Array type not found");
    auto arr = ScriptArray::Create(arr_type);

    arr->Reserve(GetSize());

    for (ptr<void> key : _data | std::views::keys) {
        arr->InsertLast(key);
    }

    return arr;
}

auto ScriptDict::GetValues() const -> refcount_ptr<ScriptArray>
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> sub_type = _typeInfo->GetSubType(1);
    FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
    string arr_type_name = strex("{}{}[]", sub_type->GetName(), (_valueTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0 ? "@" : "");
    ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
    auto arr_type = make_nptr(engine->GetTypeInfoByDecl(arr_type_name.c_str()));
    FO_VERIFY_AND_THROW(arr_type, "Array type not found");
    auto arr = ScriptArray::Create(arr_type);

    arr->Reserve(GetSize());

    for (ptr<void> value : _data | std::views::values) {
        arr->InsertLast(value);
    }

    return arr;
}

auto ScriptDict::Exists(ptr<void> key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _data.count(key.get()) != 0;
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

    for (size_t i = 0; i < _data.size(); i++) {
        if (!Equals(_keyTypeId, _keyTypeData, _typeInfo->GetEngine(), it1->first, it2->first)) {
            return false;
        }
        if (!Equals(_valueTypeId, _valueTypeData, _typeInfo->GetEngine(), it1->second, it2->second)) {
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

    _gcFlag.store(false, std::memory_order_relaxed);
    _refCount.fetch_add(1, std::memory_order_acq_rel);
}

void ScriptDict::Release() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag.store(false, std::memory_order_relaxed);

    if (_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete this;
    }
}

auto ScriptDict::GetRefCount() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _refCount.load(std::memory_order_relaxed);
}

void ScriptDict::SetFlag() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag.store(true, std::memory_order_relaxed);
}

bool ScriptDict::GetFlag() const
{
    FO_NO_STACK_TRACE_ENTRY();

    return _gcFlag.load(std::memory_order_relaxed);
}

static void EnumStoredReference(ptr<AngelScript::asIScriptEngine> engine, int32_t type_id, ptr<void> storage)
{
    FO_STACK_TRACE_ENTRY();

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) == 0) {
        return;
    }

    nptr<void> reference = (type_id & AngelScript::asTYPEID_OBJHANDLE) != 0 ? NativeDataProvider::ReadHandleSlot(storage) : nptr<void> {storage};

    if (reference) {
        engine->GCEnumCallback(reference.get_no_const());
    }
}

void ScriptDict::EnumReferences(ptr<AngelScript::asIScriptEngine> engine) const
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& kv : _data) {
        EnumStoredReference(engine, _keyTypeId, kv.first);
        EnumStoredReference(engine, _valueTypeId, kv.second);
    }
}

void ScriptDict::ReleaseAllHandles()
{
    FO_STACK_TRACE_ENTRY();

    Clear();
}

static auto CreateObject(ptr<AngelScript::asITypeInfo> obj_type, int32_t sub_type_index) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    int32_t sub_type_id = obj_type->GetSubTypeId(sub_type_index);
    nptr<AngelScript::asITypeInfo> sub_type = obj_type->GetSubType(sub_type_index);
    ptr<AngelScript::asIScriptEngine> engine = obj_type->GetEngine();

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (sub_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
        nptr<void> obj = engine->CreateScriptObject(sub_type.get());
        FO_VERIFY_AND_THROW(obj, "Created dictionary object is null");
        return obj;
    }

    int32_t element_size;

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        element_size = sizeof(AngelScript::asPWORD);
    }
    else {
        element_size = engine->GetSizeOfPrimitiveType(sub_type_id);
    }

    ptr<void> obj = SafeAlloc::MakeRawArr<uint8_t>(element_size);
    MemFill(obj, 0, element_size);
    return obj;
}

static auto CopyObject(ptr<AngelScript::asITypeInfo> obj_type, int32_t sub_type_index, ptr<void> value) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    int32_t sub_type_id = obj_type->GetSubTypeId(sub_type_index);
    nptr<AngelScript::asITypeInfo> sub_type = obj_type->GetSubType(sub_type_index);
    ptr<AngelScript::asIScriptEngine> engine = obj_type->GetEngine();

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (sub_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
        nptr<void> copy = engine->CreateScriptObjectCopy(value.get(), sub_type.get());
        FO_VERIFY_AND_THROW(copy, "Copied dictionary object is null");
        return copy;
    }

    int32_t element_size;

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        element_size = sizeof(AngelScript::asPWORD);
    }
    else {
        element_size = engine->GetSizeOfPrimitiveType(sub_type_id);
    }

    ptr<void> copied = SafeAlloc::MakeRawArr<uint8_t>(element_size);
    MemFill(copied, 0, element_size);

    if ((sub_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        auto copied_obj = NativeDataProvider::ReadHandleSlot(value);
        NativeDataProvider::WriteHandleSlot(copied, copied_obj);

        if (copied_obj) {
            FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
            engine->AddRefScriptObject(copied_obj.get_no_const(), sub_type.get());
        }
    }
    else if (sub_type_id == AngelScript::asTYPEID_BOOL || sub_type_id == AngelScript::asTYPEID_INT8 || sub_type_id == AngelScript::asTYPEID_UINT8) {
        *cast_from_void<char*>(copied.get()) = *cast_from_void<const char*>(value.get());
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT16 || sub_type_id == AngelScript::asTYPEID_UINT16) {
        *cast_from_void<int16_t*>(copied.get()) = *cast_from_void<const int16_t*>(value.get());
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT32 || sub_type_id == AngelScript::asTYPEID_UINT32 || sub_type_id == AngelScript::asTYPEID_FLOAT) {
        *cast_from_void<int32_t*>(copied.get()) = *cast_from_void<const int32_t*>(value.get());
    }
    else if (sub_type_id == AngelScript::asTYPEID_INT64 || sub_type_id == AngelScript::asTYPEID_UINT64 || sub_type_id == AngelScript::asTYPEID_DOUBLE) {
        *cast_from_void<int64_t*>(copied.get()) = *cast_from_void<const int64_t*>(value.get());
    }
    else if (sub_type_id > AngelScript::asTYPEID_DOUBLE) { // Enums - copy actual size
        MemCopy(copied, value, element_size);
    }

    return copied;
}

static void CleanupScriptDictValueBytes(ptr<uint8_t> value_bytes) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    unique_arr_ptr<uint8_t> owned_value_bytes {value_bytes.get()};
    ignore_unused(owned_value_bytes);
}

static void DestroyObject(ptr<AngelScript::asITypeInfo> obj_type, int32_t sub_type_index, ptr<void> value)
{
    FO_STACK_TRACE_ENTRY();

    int32_t sub_type_id = obj_type->GetSubTypeId(sub_type_index);
    ptr<AngelScript::asIScriptEngine> engine = obj_type->GetEngine();

    if ((sub_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (sub_type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        nptr<AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(sub_type_id);
        FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
        engine->ReleaseScriptObject(value.get(), sub_type.get());
    }
    else {
        nptr<void> obj = (sub_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0 ? NativeDataProvider::ReadHandleSlot(value) : nullptr;

        if (obj) {
            nptr<AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(sub_type_id);
            FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");
            engine->ReleaseScriptObject(obj.get_no_const(), sub_type.get());
        }

        auto value_bytes = value.reinterpret_as<uint8_t>();
        CleanupScriptDictValueBytes(value_bytes);
    }
}

static auto Less(int32_t type_id, nptr<const ScriptDictTypeData> type_data, ptr<AngelScript::asIScriptEngine> engine, ptr<void> a, ptr<void> b) -> bool
{
    if (type_data) {
        if (!type_data->CmpFunc) {
            nptr<const AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(type_id);
            FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");

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

static auto Equals(int32_t type_id, nptr<const ScriptDictTypeData> type_data, ptr<AngelScript::asIScriptEngine> engine, ptr<void> a, ptr<void> b) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (type_data) {
        if (!type_data->CmpFunc && !type_data->EqFunc && (type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
            nptr<const AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(type_id);
            FO_VERIFY_AND_THROW(sub_type, "Dictionary sub-type info not found");

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

static auto Compare(bool check_less, int32_t type_id, nptr<const ScriptDictTypeData> type_data, ptr<AngelScript::asIScriptEngine> engine, ptr<void> a, ptr<void> b) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) == 0) {
        if (check_less) {
            switch (type_id) {
#define COMPARE(T) *cast_from_void<const T*>(a.get()) < *cast_from_void<const T*>(b.get())
            case AngelScript::asTYPEID_BOOL:
                return COMPARE(bool);
            case AngelScript::asTYPEID_INT8:
                return COMPARE(int8_t);
            case AngelScript::asTYPEID_UINT8:
                return COMPARE(uint8_t);
            case AngelScript::asTYPEID_INT16:
                return COMPARE(int16_t);
            case AngelScript::asTYPEID_UINT16:
                return COMPARE(uint16_t);
            case AngelScript::asTYPEID_INT32:
                return COMPARE(int32_t);
            case AngelScript::asTYPEID_UINT32:
                return COMPARE(uint32_t);
            case AngelScript::asTYPEID_INT64:
                return COMPARE(int64_t);
            case AngelScript::asTYPEID_UINT64:
                return COMPARE(uint64_t);
            case AngelScript::asTYPEID_FLOAT:
                return COMPARE(float32_t);
            case AngelScript::asTYPEID_DOUBLE:
                return COMPARE(float64_t);
            default: // Enum types
                switch (engine->GetSizeOfPrimitiveType(type_id)) {
                case 1:
                    return COMPARE(uint8_t);
                case 2:
                    return COMPARE(uint16_t);
                default:
                    return COMPARE(int32_t);
                }
#undef COMPARE
            }
        }
        else {
            switch (type_id) {
#define COMPARE(T) *cast_from_void<const T*>(a.get()) == *cast_from_void<const T*>(b.get())
            case AngelScript::asTYPEID_BOOL:
                return COMPARE(bool);
            case AngelScript::asTYPEID_INT8:
                return COMPARE(int8_t);
            case AngelScript::asTYPEID_UINT8:
                return COMPARE(uint8_t);
            case AngelScript::asTYPEID_INT16:
                return COMPARE(int16_t);
            case AngelScript::asTYPEID_UINT16:
                return COMPARE(uint16_t);
            case AngelScript::asTYPEID_INT32:
                return COMPARE(int32_t);
            case AngelScript::asTYPEID_UINT32:
                return COMPARE(uint32_t);
            case AngelScript::asTYPEID_FLOAT:
                return COMPARE(float32_t);
            case AngelScript::asTYPEID_DOUBLE:
                return COMPARE(float64_t);
            default: // Enum types
                switch (engine->GetSizeOfPrimitiveType(type_id)) {
                case 1:
                    return COMPARE(uint8_t);
                case 2:
                    return COMPARE(uint16_t);
                default:
                    return COMPARE(int32_t);
                }
#undef COMPARE
            }
        }
    }
    else {
        if (type_data && type_data->FastCompare != nullptr && (type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
            ptr<const void> lhs = a.get();
            ptr<const void> rhs = b.get();
            int32_t r = type_data->FastCompare(lhs, rhs);
            return check_less ? r < 0 : r == 0;
        }

        bool is_handle = (type_id & AngelScript::asTYPEID_OBJHANDLE) != 0;
        nptr<void> lhs_obj {};
        nptr<void> rhs_obj {};

        if (is_handle) {
            lhs_obj = NativeDataProvider::ReadHandleSlot(a);
            rhs_obj = NativeDataProvider::ReadHandleSlot(b);
        }

        if (!check_less) {
            if (is_handle) {
                if (lhs_obj == rhs_obj) {
                    return true;
                }
            }
        }
        else {
            if (is_handle) {
                if (!lhs_obj) {
                    return true;
                }
                if (!rhs_obj) {
                    return false;
                }
            }
        }

        if ((type_id & AngelScript::asTYPEID_OBJHANDLE) != 0 && type_data && !type_data->EqFunc && !type_data->CmpFunc) {
            return false;
        }

        auto ctx = make_nptr(AngelScript::asGetActiveContext());
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
        FO_VERIFY_AND_THROW(ctx->GetEngine() == engine, "AngelScript dictionary context belongs to a different engine");

        int32_t as_result = 0;
        FO_AS_VERIFY(ctx->PushState());

        auto release_ctx = scope_exit([&]() noexcept {
            safe_call([&] {
                auto state = ctx->GetState();
                ctx->PopState();

                if (state == AngelScript::asEXECUTION_ABORTED) {
                    ctx->Abort();
                }
            });
        });

        if (!check_less && type_data->EqFunc) {
            FO_AS_VERIFY(ctx->Prepare(type_data->EqFunc.get_no_const()));

            if (is_handle) {
                FO_AS_VERIFY(ctx->SetObject(lhs_obj.get()));
                FO_AS_VERIFY(ctx->SetArgObject(0, rhs_obj.get()));
            }
            else {
                FO_AS_VERIFY(ctx->SetObject(a.get()));
                FO_AS_VERIFY(ctx->SetArgObject(0, b.get()));
            }

            FO_AS_VERIFY(ctx->Execute());

            if (as_result == AngelScript::asEXECUTION_FINISHED) {
                return ctx->GetReturnByte() != 0;
            }

            return false;
        }

        if (type_data->CmpFunc) {
            FO_AS_VERIFY(ctx->Prepare(type_data->CmpFunc.get_no_const()));

            if (is_handle) {
                FO_AS_VERIFY(ctx->SetObject(lhs_obj.get()));
                FO_AS_VERIFY(ctx->SetArgObject(0, rhs_obj.get()));
            }
            else {
                FO_AS_VERIFY(ctx->SetObject(a.get()));
                FO_AS_VERIFY(ctx->SetArgObject(0, b.get()));
            }

            FO_AS_VERIFY(ctx->Execute());

            if (as_result == AngelScript::asEXECUTION_FINISHED) {
                if (check_less) {
                    return static_cast<int32_t>(ctx->GetReturnDWord()) < 0;
                }
                else {
                    return static_cast<int32_t>(ctx->GetReturnDWord()) == 0;
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

    nptr<AngelScript::asITypeInfo> type_info = ti;
    FO_VERIFY_AND_THROW(type_info, "Dictionary type info is null");
    auto dict = ScriptDict::Create(type_info);
    return dict.release_ownership();
}

static auto ScriptDict_CreateList(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> type_info = ti;
    FO_VERIFY_AND_THROW(type_info, "Dictionary type info is null");
    nptr<void> init_list_ptr = init_list;
    FO_VERIFY_AND_THROW(init_list_ptr, "Dictionary init list is null");
    auto dict = ScriptDict::Create(type_info, init_list_ptr);
    return dict.release_ownership();
}

static auto ScriptDict_Factory(AngelScript::asITypeInfo* ti, const ScriptDict* other) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> type_info = ti;
    FO_VERIFY_AND_THROW(type_info, "Dictionary type info is null");

    nptr<const ScriptDict> other_ptr = other;
    if (!other_ptr) {
        throw ScriptException("Dict arg is null");
    }

    auto clone = ScriptDict::Create(type_info);
    *clone = *other_ptr;
    return clone.release_ownership();
}

static auto ScriptDict_Clone(const ScriptDict& dict) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    auto type_info = make_ptr(const_cast<AngelScript::asITypeInfo*>(std::addressof(*dict.GetDictObjectType())));
    auto clone = ScriptDict::Create(type_info);
    *clone = dict;
    return clone.release_ownership();
}

[[nodiscard]] static auto RequireScriptDictValue(nptr<void> value) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(value, "Dictionary value is null");
    return value;
}

static auto ScriptDict_Equals(const ScriptDict& dict, const ScriptDict* other) -> bool
{
    FO_STACK_TRACE_ENTRY();

    nptr<const ScriptDict> other_ptr = other;
    if (!other_ptr) {
        throw ScriptException("Dict arg is null");
    }

    return dict == *other_ptr;
}

static auto ScriptDict_Get(const ScriptDict& dict, void* key) -> void*
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> key_arg = key;
    auto key_ptr = RequireScriptDictValue(key_arg);
    ptr<void> value = dict.Get(key_ptr);
    return value.get();
}

static auto ScriptDict_GetOrCreate(ScriptDict& dict, void* key) -> void*
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> key_arg = key;
    auto key_ptr = RequireScriptDictValue(key_arg);
    ptr<void> value = dict.GetOrCreate(key_ptr);
    return value.get();
}

static auto ScriptDict_Remove(ScriptDict& dict, void* key) -> bool
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> key_arg = key;
    auto key_ptr = RequireScriptDictValue(key_arg);
    return dict.Remove(key_ptr);
}

static auto ScriptDict_RemoveValues(ScriptDict& dict, void* value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptDictValue(value_arg);
    return dict.RemoveValues(value_ptr);
}

static void ScriptDict_Set(ScriptDict& dict, void* key, void* value)
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> key_arg = key;
    nptr<void> value_arg = value;
    auto key_ptr = RequireScriptDictValue(key_arg);
    auto value_ptr = RequireScriptDictValue(value_arg);
    dict.Set(key_ptr, value_ptr);
}

static void ScriptDict_SetIfNotExist(ScriptDict& dict, void* key, void* value)
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> key_arg = key;
    nptr<void> value_arg = value;
    auto key_ptr = RequireScriptDictValue(key_arg);
    auto value_ptr = RequireScriptDictValue(value_arg);
    dict.SetIfNotExist(key_ptr, value_ptr);
}

static auto ScriptDict_GetDefault(const ScriptDict& dict, void* key, void* def_val) -> void*
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> key_arg = key;
    nptr<void> def_val_arg = def_val;
    auto key_ptr = RequireScriptDictValue(key_arg);
    auto def_val_ptr = RequireScriptDictValue(def_val_arg);
    ptr<void> value = dict.GetDefault(key_ptr, def_val_ptr);
    return value.get();
}

static auto ScriptDict_GetKey(const ScriptDict& dict, int32_t index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    ptr<void> key = dict.GetKey(index);
    return key.get();
}

static auto ScriptDict_GetValue(const ScriptDict& dict, int32_t index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    ptr<void> value = dict.GetValue(index);
    return value.get();
}

static auto ScriptDict_GetKeys(const ScriptDict& dict) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    auto keys = dict.GetKeys();
    return keys.release_ownership();
}

static auto ScriptDict_GetValues(const ScriptDict& dict) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    auto values = dict.GetValues();
    return values.release_ownership();
}

static auto ScriptDict_Exists(const ScriptDict& dict, void* key) -> bool
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> key_arg = key;
    auto key_ptr = RequireScriptDictValue(key_arg);
    return dict.Exists(key_ptr);
}

static void ScriptDict_EnumReferences(const ScriptDict& dict, AngelScript::asIScriptEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asIScriptEngine> engine_arg = engine;
    FO_VERIFY_AND_THROW(engine_arg, "Script engine is null");
    dict.EnumReferences(engine_arg);
}

static void ScriptDict_ReleaseAllHandles(ScriptDict& dict, AngelScript::asIScriptEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asIScriptEngine> engine_arg = engine;
    FO_VERIFY_AND_THROW(engine_arg, "Script engine is null");
    dict.ReleaseAllHandles();
}

void RegisterAngelScriptDict(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    as_engine->SetTypeInfoUserDataCleanupCallback(CleanupTypeInfoDictCache, AS_TYPE_DICT_CACHE);

    int32_t as_result = 0;
    FO_AS_VERIFY(as_engine->RegisterObjectType("dict<class T1, class T2>", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_GC | AngelScript::asOBJ_TEMPLATE));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", FO_SCRIPT_FUNC(ScriptDict_TemplateCallback), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_FACTORY, "dict<T1,T2>@ f(int&in)", FO_SCRIPT_FUNC(ScriptDict_Create), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_FACTORY, "dict<T1,T2>@ f(int&in, const dict<T1,T2>@+)", FO_SCRIPT_FUNC(ScriptDict_Factory), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_LIST_FACTORY, "dict<T1,T2>@ f(int&in type, int&in list) {repeat {T1, T2}}", FO_SCRIPT_FUNC(ScriptDict_CreateList), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_METHOD(ScriptDict, AddRef), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_METHOD(ScriptDict, Release), FO_SCRIPT_METHOD_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "T2& opIndex(const T1&in key)", FO_SCRIPT_FUNC_THIS(ScriptDict_GetOrCreate), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T2& opIndex(const T1&in key) const", FO_SCRIPT_FUNC_THIS(ScriptDict_Get), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "void set(const T1&in, const T2&in key)", FO_SCRIPT_FUNC_THIS(ScriptDict_Set), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "void setIfNotExist(const T1&in key, const T2&in defVal)", FO_SCRIPT_FUNC_THIS(ScriptDict_SetIfNotExist), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "bool remove(const T1&in key)", FO_SCRIPT_FUNC_THIS(ScriptDict_Remove), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "int32 removeValues(const T1&in value)", FO_SCRIPT_FUNC_THIS(ScriptDict_RemoveValues), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "int length() const", FO_SCRIPT_METHOD(ScriptDict, GetSize), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "void clear()", FO_SCRIPT_METHOD(ScriptDict, Clear), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T2& get(const T1&in key) const", FO_SCRIPT_FUNC_THIS(ScriptDict_Get), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T2& get(const T1&in key, const T2&in defVal) const", FO_SCRIPT_FUNC_THIS(ScriptDict_GetDefault), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T1& getKey(int index) const", FO_SCRIPT_FUNC_THIS(ScriptDict_GetKey), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "const T2& getValue(int index) const", FO_SCRIPT_FUNC_THIS(ScriptDict_GetValue), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "array<T1>@ getKeys() const", FO_SCRIPT_FUNC_THIS(ScriptDict_GetKeys), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "array<T2>@ getValues() const", FO_SCRIPT_FUNC_THIS(ScriptDict_GetValues), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "bool exists(const T1&in key) const", FO_SCRIPT_FUNC_THIS(ScriptDict_Exists), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "bool isEmpty() const", FO_SCRIPT_METHOD(ScriptDict, IsEmpty), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "dict<T1,T2>@ clone() const", FO_SCRIPT_FUNC_THIS(ScriptDict_Clone), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("dict<T1,T2>", "bool opEquals(const dict<T1,T2>@+ other) const", FO_SCRIPT_FUNC_THIS(ScriptDict_Equals), FO_SCRIPT_FUNC_THIS_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_GETREFCOUNT, "int f()", FO_SCRIPT_METHOD(ScriptDict, GetRefCount), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_SETGCFLAG, "void f()", FO_SCRIPT_METHOD(ScriptDict, SetFlag), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_GETGCFLAG, "bool f()", FO_SCRIPT_METHOD(ScriptDict, GetFlag), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_ENUMREFS, "void f(int&in)", FO_SCRIPT_FUNC_THIS(ScriptDict_EnumReferences), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("dict<T1,T2>", AngelScript::asBEHAVE_RELEASEREFS, "void f(int&in)", FO_SCRIPT_FUNC_THIS(ScriptDict_ReleaseAllHandles), FO_SCRIPT_FUNC_THIS_CONV));
}

FO_END_NAMESPACE

#endif
