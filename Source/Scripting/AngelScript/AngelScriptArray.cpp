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

#include "AngelScriptArray.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptHelpers.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

constexpr AngelScript::asPWORD AS_TYPE_ARRAY_CACHE = 1000;

struct ScriptArrayTypeData
{
    nptr<AngelScript::asIScriptFunction> CmpFunc {};
    nptr<AngelScript::asIScriptFunction> EqFunc {};
    int32_t CmpFuncReturnCode {};
    int32_t EqFuncReturnCode {};
};

static auto ScriptArrayInitListBytesAt(ptr<void> init_list, size_t offset) noexcept -> ptr<AngelScript::asBYTE>
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asBYTE> bytes = cast_from_void<AngelScript::asBYTE*>(init_list.get());
    return bytes.get() + offset;
}

static auto ScriptArrayInitListPayload(ptr<void> init_list, int32_t element_size) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    const size_t payload_offset = element_size >= 8 ? sizeof(int64_t) : sizeof(int32_t);
    auto payload = ScriptArrayInitListBytesAt(init_list, payload_offset);
    return cast_to_void(payload.get());
}

template<typename T>
static auto ReadScriptArrayInitListValue(ptr<void> init_list) noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(std::is_trivially_copyable_v<T>);

    ptr<T> value = cast_from_void<T*>(init_list.get());
    return *value;
}

static auto ScriptArrayInitListObjectAt(ptr<void> init_list, int32_t index, ptr<AngelScript::asITypeInfo> sub_type) noexcept -> ptr<AngelScript::asBYTE>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(index >= 0, "Init list element index is negative");

    const int32_t sub_size = sub_type->GetSize();
    const size_t elem_align = sub_size >= 8 ? 8u : 4u;
    const size_t header = sub_size >= 4 ? ((sizeof(int32_t) + (elem_align - 1)) & ~(elem_align - 1)) : sizeof(int32_t);
    const size_t stride = sub_size >= 4 ? ((numeric_cast<size_t>(sub_size) + (elem_align - 1)) & ~(elem_align - 1)) : numeric_cast<size_t>(sub_size);
    const size_t object_offset = numeric_cast<size_t>(index) * stride;
    return ScriptArrayInitListBytesAt(init_list, header + object_offset);
}

static void CleanupScriptArrayTypeData(ptr<ScriptArrayTypeData> cache) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owned_cache = adopt_unique_ptr(cache);
    ignore_unused(owned_cache);
}

static void CleanupTypeInfoArrayCache(AngelScript::asITypeInfo* type)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asITypeInfo> type_info = type;
    nptr<ScriptArrayTypeData> cache = cast_from_void<ScriptArrayTypeData*>(type_info->GetUserData(AS_TYPE_ARRAY_CACHE));
    if (cache) {
        CleanupScriptArrayTypeData(cache.as_ptr());
    }
    type_info->SetUserData(nullptr, AS_TYPE_ARRAY_CACHE);
}

static auto ScriptArrayTemplateCallback(AngelScript::asITypeInfo* ti, bool& dont_garbage_collect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asITypeInfo> type_info = ti;
    ptr<AngelScript::asIScriptEngine> engine = type_info->GetEngine();
    const int32_t type_id = type_info->GetSubTypeId();

    if (type_id == AngelScript::asTYPEID_VOID) {
        return false;
    }

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        nptr<AngelScript::asITypeInfo> nullable_sub_type = engine->GetTypeInfoById(type_id);
        FO_VERIFY_AND_THROW(nullable_sub_type, "Array sub-type info not found");
        auto sub_type = nullable_sub_type.as_ptr();
        const auto flags = sub_type->GetFlags();

        if ((flags & AngelScript::asOBJ_VALUE) != 0 && (flags & AngelScript::asOBJ_POD) == 0) {
            bool has_default_ctor = false;

            for (AngelScript::asUINT i = 0; i < sub_type->GetBehaviourCount(); i++) {
                AngelScript::asEBehaviours beh;
                nptr<const AngelScript::asIScriptFunction> nullable_func = sub_type->GetBehaviourByIndex(i, &beh);

                if (!nullable_func || beh != AngelScript::asBEHAVE_CONSTRUCT) {
                    continue;
                }

                auto func = nullable_func.as_ptr();

                if (func->GetParamCount() == 0) {
                    has_default_ctor = true;
                    break;
                }
            }

            if (!has_default_ctor) {
                engine->WriteMessage("array", 0, 0, AngelScript::asMSGTYPE_ERROR, "The subtype has no default constructor");
                return false;
            }
        }
        else if ((flags & AngelScript::asOBJ_REF) != 0) {
            engine->WriteMessage("array", 0, 0, AngelScript::asMSGTYPE_ERROR, "Can't store references in array");
            return false;
        }

        if ((flags & AngelScript::asOBJ_GC) == 0) {
            dont_garbage_collect = true;
        }
    }
    else if ((type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        // Array of primitives
        dont_garbage_collect = true;
    }
    else {
        nptr<AngelScript::asITypeInfo> nullable_sub_type = engine->GetTypeInfoById(type_id);
        FO_VERIFY_AND_THROW(nullable_sub_type, "Array sub-type info not found");
        auto sub_type = nullable_sub_type.as_ptr();
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

static auto ScriptArray_Create(AngelScript::asITypeInfo* ti) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> type_info = ti;
    FO_VERIFY_AND_THROW(!!type_info, "Array type info is null");
    auto arr = ScriptArray::Create(type_info.as_ptr());
    return ReleaseScriptOwnership(std::move(arr));
}

static auto ScriptArray_CreateWithLength(AngelScript::asITypeInfo* ti, int32_t length) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> type_info = ti;
    FO_VERIFY_AND_THROW(!!type_info, "Array type info is null");
    auto arr = ScriptArray::Create(type_info.as_ptr(), length);
    return ReleaseScriptOwnership(std::move(arr));
}

static auto ScriptArray_CreateList(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> type_info = ti;
    FO_VERIFY_AND_THROW(!!type_info, "Array type info is null");
    nptr<void> init_list_ptr = init_list;
    FO_VERIFY_AND_THROW(!!init_list_ptr, "Array init list is null");
    auto arr = ScriptArray::Create(type_info.as_ptr(), init_list_ptr.as_ptr());
    return ReleaseScriptOwnership(std::move(arr));
}

static auto ScriptArray_CreateWithDefault(AngelScript::asITypeInfo* ti, int32_t length, void* def_val) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> type_info = ti;
    FO_VERIFY_AND_THROW(!!type_info, "Array type info is null");
    nptr<void> def_val_ptr = def_val;
    if (length == 0) {
        auto arr = ScriptArray::Create(type_info.as_ptr(), length);
        return ReleaseScriptOwnership(std::move(arr));
    }
    FO_VERIFY_AND_THROW(!!def_val_ptr, "Array default value is null");
    auto arr = ScriptArray::Create(type_info.as_ptr(), length, def_val_ptr.as_ptr());
    return ReleaseScriptOwnership(std::move(arr));
}

[[nodiscard]] static auto RequireScriptArrayValue(nptr<void> nullable_value) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(nullable_value, "Array value is null");
    return nullable_value.as_ptr();
}

auto ScriptArray::Create(ptr<AngelScript::asITypeInfo> ti, int32_t length) -> refcount_ptr<ScriptArray>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptArray>(length, ti);
}

auto ScriptArray::Create(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list) -> refcount_ptr<ScriptArray>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptArray>(ti, init_list);
}

auto ScriptArray::Create(ptr<AngelScript::asITypeInfo> ti, int32_t length, ptr<void> def_val) -> refcount_ptr<ScriptArray>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptArray>(length, def_val, ti);
}

auto ScriptArray::Create(ptr<AngelScript::asITypeInfo> ti) -> refcount_ptr<ScriptArray>
{
    FO_STACK_TRACE_ENTRY();

    return Create(ti, 0);
}

ScriptArray::ScriptArray(ptr<AngelScript::asITypeInfo> ti, ptr<void> init_list) :
    _typeInfo {refcount_ptr<AngelScript::asITypeInfo>::from_add_ref(ti.get())},
    _subTypeId {ti->GetSubTypeId()}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(string_view(ti->GetName()) == "array", "AngelScript type info is not an array type");

    ptr<AngelScript::asIScriptEngine> engine = ti->GetEngine();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        _elementSize = sizeof(void*);
    }
    else {
        _elementSize = engine->GetSizeOfPrimitiveType(_subTypeId);
        FO_VERIFY_AND_THROW(_elementSize != 0, "Element size must be non-zero", _elementSize);
    }

    PrecacheSubTypeData();

    const int32_t length = ReadScriptArrayInitListValue<int32_t>(init_list);
    CheckArraySize(length);

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) == 0) {
        CreateBuffer(length);

        if (length != 0) {
            auto init_payload = ScriptArrayInitListPayload(init_list, _elementSize);
            MemCopy(At(0), init_payload, numeric_cast<size_t>(length * _elementSize));
        }
    }
    else if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        CreateBuffer(length);

        if (length != 0) {
            auto init_payload = ScriptArrayInitListBytesAt(init_list, sizeof(int32_t));
            MemCopy(At(0), init_payload, numeric_cast<size_t>(length * _elementSize));
            MemFill(init_payload, 0, numeric_cast<size_t>(length * _elementSize));
        }
    }
    else {
        CreateBuffer(length);

        nptr<AngelScript::asITypeInfo> nullable_sub_type = ti->GetSubType();
        FO_VERIFY_AND_THROW(nullable_sub_type, "Array sub-type info not found");
        auto sub_type = nullable_sub_type.as_ptr();

        for (int32_t i = 0; i < length; i++) {
            auto obj = At(i);
            auto src_obj = ScriptArrayInitListObjectAt(init_list, i, sub_type);
            engine->AssignScriptObject(obj.get(), src_obj.get(), sub_type.get());
        }
    }

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        engine->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }
}

ScriptArray::ScriptArray(int32_t length, ptr<AngelScript::asITypeInfo> ti) :
    _typeInfo {refcount_ptr<AngelScript::asITypeInfo>::from_add_ref(ti.get())},
    _subTypeId {ti->GetSubTypeId()}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(string_view(ti->GetName()) == "array", "AngelScript type info is not an array type");

    ptr<AngelScript::asIScriptEngine> engine = ti->GetEngine();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        _elementSize = sizeof(void*);
    }
    else {
        _elementSize = engine->GetSizeOfPrimitiveType(_subTypeId);
        FO_VERIFY_AND_THROW(_elementSize != 0, "Element size must be non-zero", _elementSize);
    }

    PrecacheSubTypeData();
    CheckArraySize(length);
    CreateBuffer(length);

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        engine->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }
}

ScriptArray::ScriptArray(int32_t length, ptr<void> def_val, ptr<AngelScript::asITypeInfo> ti) :
    _typeInfo {refcount_ptr<AngelScript::asITypeInfo>::from_add_ref(ti.get())},
    _subTypeId {ti->GetSubTypeId()}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(string(ti->GetName()) == "array", "AngelScript type info is not an array type");

    ptr<AngelScript::asIScriptEngine> engine = ti->GetEngine();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        _elementSize = sizeof(void*);
    }
    else {
        _elementSize = engine->GetSizeOfPrimitiveType(_subTypeId);
        FO_VERIFY_AND_THROW(_elementSize != 0, "Element size must be non-zero", _elementSize);
    }

    PrecacheSubTypeData();
    CheckArraySize(length);
    CreateBuffer(length);

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        engine->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }

    for (int32_t i = 0; i < GetSize(); i++) {
        SetValue(i, def_val);
    }
}

ScriptArray::ScriptArray(const ScriptArray& other) :
    _typeInfo {other._typeInfo},
    _subTypeId {other._subTypeId}
{
    FO_STACK_TRACE_ENTRY();

    _elementSize = other._elementSize;
    FO_VERIFY_AND_THROW(_elementSize != 0, "Element size must be non-zero", _elementSize);

    PrecacheSubTypeData();

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
        engine->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }

    CreateBuffer(0);
    *this = other;
}

auto ScriptArray::operator=(const ScriptArray& other) -> ScriptArray&
{
    FO_STACK_TRACE_ENTRY();

    if (other._typeInfo != _typeInfo) {
        throw ScriptException("Different types on array assignment");
    }

    if (&other != this) {
        Resize(other.GetSize());
        CopyBuffer(other);
    }

    return *this;
}

ScriptArray::~ScriptArray()
{
    FO_STACK_TRACE_ENTRY();

    DeleteBuffer();
}

void ScriptArray::SetValue(int32_t index, ptr<void> value)
{
    FO_STACK_TRACE_ENTRY();

    auto dst = At(index);

    if ((_subTypeId & ~AngelScript::asTYPEID_MASK_SEQNBR) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
        nptr<AngelScript::asITypeInfo> sub_type = _typeInfo->GetSubType();
        FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");
        engine->AssignScriptObject(dst.get(), value.get(), sub_type.get());
    }
    else if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
        nptr<AngelScript::asITypeInfo> sub_type = _typeInfo->GetSubType();
        FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");
        // dst (array element) and value (incoming AngelScript stack slot) are only asDWORD-aligned, so read/
        // write the handles through aligned locals to avoid a misaligned 8-byte pointer access (UBSan)
        const nptr<void> old_obj = MemReadUnaligned<void*>(dst);
        const nptr<void> new_obj = MemReadUnaligned<void*>(value);
        MemWriteUnaligned<void*>(dst, new_obj.get_no_const());

        if (new_obj) {
            engine->AddRefScriptObject(new_obj.get_no_const(), sub_type.get());
        }
        if (old_obj) {
            engine->ReleaseScriptObject(old_obj.get_no_const(), sub_type.get());
        }
    }
    else if (_subTypeId == AngelScript::asTYPEID_BOOL || _subTypeId == AngelScript::asTYPEID_INT8 || _subTypeId == AngelScript::asTYPEID_UINT8) {
        *cast_from_void<char*>(dst.get()) = *cast_from_void<const char*>(value.get());
    }
    else if (_subTypeId == AngelScript::asTYPEID_INT16 || _subTypeId == AngelScript::asTYPEID_UINT16) {
        *cast_from_void<short*>(dst.get()) = *cast_from_void<const short*>(value.get());
    }
    else if (_subTypeId == AngelScript::asTYPEID_INT32 || _subTypeId == AngelScript::asTYPEID_UINT32 || _subTypeId == AngelScript::asTYPEID_FLOAT) {
        *cast_from_void<int32_t*>(dst.get()) = *cast_from_void<const int32_t*>(value.get());
    }
    else if (_subTypeId == AngelScript::asTYPEID_INT64 || _subTypeId == AngelScript::asTYPEID_UINT64 || _subTypeId == AngelScript::asTYPEID_DOUBLE) {
        *cast_from_void<double*>(dst.get()) = *cast_from_void<const double*>(value.get());
    }
    else if (_subTypeId > AngelScript::asTYPEID_DOUBLE) { // Enums - copy actual size
        MemCopy(dst, value, _elementSize);
    }
}

void ScriptArray::Reserve(int32_t max_elements)
{
    FO_STACK_TRACE_ENTRY();

    if (max_elements <= GetCapacity()) {
        return;
    }

    CheckArraySize(max_elements);
    _buffer.reserve(numeric_cast<size_t>(max_elements * _elementSize));
}

void ScriptArray::Resize(int32_t num_elements)
{
    FO_STACK_TRACE_ENTRY();

    if (num_elements == GetSize()) {
        return;
    }

    CheckArraySize(num_elements);
    Resize(num_elements - GetSize(), std::numeric_limits<int32_t>::max());
}

void ScriptArray::RemoveRange(int32_t start, int32_t num_elements)
{
    FO_STACK_TRACE_ENTRY();

    if (num_elements <= 0) {
        return;
    }

    if (start < 0 || start > GetSize()) {
        throw ScriptException("Index out of bounds", start, GetSize());
    }

    if (start + num_elements > GetSize()) {
        num_elements = GetSize() - start;
    }

    Destruct(start, start + num_elements);
}

void ScriptArray::Resize(int32_t delta, int32_t at)
{
    FO_STACK_TRACE_ENTRY();

    if (delta < 0) {
        FO_VERIFY_AND_THROW(-delta <= GetSize(), "Script array resize cannot remove more elements than the array contains", delta, GetSize(), at);
        at = std::clamp(at, 0, GetSize() + delta);
        Destruct(at, at - delta);
    }
    else if (delta > 0) {
        CheckArraySize(GetSize() + delta);
        at = std::clamp(at, 0, GetSize());
        Construct(at, at + delta);
    }
}

void ScriptArray::CheckArraySize(int32_t num_elements) const
{
    FO_STACK_TRACE_ENTRY();

    if (num_elements < 0) {
        throw ScriptException("Negative array size", num_elements);
    }

    FO_VERIFY_AND_THROW(_elementSize != 0, "Element size must be non-zero", _elementSize);
    const int32_t max_size = 0x7FFFFFFF / _elementSize;

    if (num_elements > max_size) {
        throw ScriptException("Array size is too large", num_elements, max_size);
    }
}

auto ScriptArray::GetArrayObjectType() -> ptr<AngelScript::asITypeInfo>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo;
}

auto ScriptArray::GetArrayObjectType() const -> ptr<const AngelScript::asITypeInfo>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo;
}

auto ScriptArray::GetArrayTypeId() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo->GetTypeId();
}

auto ScriptArray::GetElementTypeId() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _subTypeId;
}

void ScriptArray::InsertAt(int32_t index, ptr<void> value)
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0 || index > GetSize()) {
        throw ScriptException("Index out of bounds", index, GetSize());
    }

    Resize(1, index);
    SetValue(index, value);
}

void ScriptArray::InsertAt(int32_t index, const ScriptArray& other)
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0 || index > GetSize()) {
        throw ScriptException("Index out of bounds", index, GetSize());
    }
    if (_typeInfo != other._typeInfo) {
        throw ScriptException("Mismatching array types");
    }

    const auto num_elements = other.GetSize();
    Resize(num_elements, index);

    if (&other != this) {
        for (int32_t i = 0; i < other.GetSize(); i++) {
            ptr<void> value = other.At(i);
            SetValue(index + i, value);
        }
    }
    else {
        for (int32_t i = 0; i < index; i++) {
            ptr<void> value = other.At(i);
            SetValue(index + i, value);
        }
        for (int32_t i = index + num_elements, j = 0; i < other.GetSize(); i++, j++) {
            ptr<void> value = other.At(i);
            SetValue(index + index + j, value);
        }
    }
}

void ScriptArray::InsertLast(ptr<void> value)
{
    FO_STACK_TRACE_ENTRY();

    InsertAt(GetSize(), value);
}

void ScriptArray::RemoveAt(int32_t index)
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= GetSize()) {
        throw ScriptException("Index out of bounds", index, GetSize());
    }

    Resize(-1, index);
}

void ScriptArray::RemoveLast()
{
    FO_STACK_TRACE_ENTRY();

    RemoveAt(GetSize() - 1);
}

auto ScriptArray::At(int32_t index) const -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= GetSize()) {
        throw ScriptException("Index out of bounds", index, GetSize());
    }

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        auto nullable_object = NativeDataProvider::ReadHandleSlot(GetArrayItemPointer(index));
        FO_VERIFY_AND_THROW(nullable_object, "Array element object is null");
        return nullable_object.as_ptr();
    }
    else {
        return GetArrayItemPointer(index);
    }
}

void ScriptArray::CreateBuffer(int32_t num_elements)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(num_elements >= 0, "Num elements is negative");

    Construct(0, num_elements);
}

void ScriptArray::DeleteBuffer() noexcept
{
    FO_STACK_TRACE_ENTRY();

    safe_call([this] { Destruct(0, GetSize()); });
}

void ScriptArray::Construct(int32_t start, int32_t end)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(start <= end, "Script array construction range has inverted boundaries", start, end, GetSize());

    if (start == end) {
        return;
    }

    // Handle capacity on insert last
    if (start == end - 1 && _buffer.size() >= _buffer.capacity()) {
        _buffer.reserve(std::max(_buffer.capacity(), numeric_cast<size_t>(10 * _elementSize)));
    }

    const auto it_start = _buffer.begin() + numeric_cast<ptrdiff_t>(start * _elementSize);
    const auto count = numeric_cast<size_t>((end - start) * _elementSize);
    _buffer.insert(it_start, count, {});

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
        nptr<AngelScript::asITypeInfo> sub_type = _typeInfo->GetSubType();
        FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");

        for (int32_t index = start; index < end; index++) {
            auto slot = NativeDataProvider::GetHandleSlot(GetArrayItemPointer(index));
            const nptr<void> new_obj = engine->CreateScriptObject(sub_type.get());
            *slot = new_obj.get_no_const();

            if (!new_obj) {
                return;
            }
        }
    }
}

void ScriptArray::Destruct(int32_t start, int32_t end)
{
    FO_STACK_TRACE_ENTRY();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
        nptr<AngelScript::asITypeInfo> sub_type = _typeInfo->GetSubType();
        FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");

        for (int32_t index = start; index < end; index++) {
            const auto obj = NativeDataProvider::ReadHandleSlot(GetArrayItemPointer(index));

            if (obj) {
                engine->ReleaseScriptObject(obj.get_no_const(), sub_type.get());
            }
        }
    }

    const auto it_start = _buffer.begin() + numeric_cast<ptrdiff_t>(start * _elementSize);
    const auto it_end = _buffer.begin() + numeric_cast<ptrdiff_t>(end * _elementSize);
    _buffer.erase(it_start, it_end);
}

auto ScriptArray::Equals(ptr<void> a, ptr<void> b, nptr<AngelScript::asIScriptContext> nullable_ctx) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!_subTypeData) {
        switch (_subTypeId) {
#define COMPARE(T) *cast_from_void<T*>(a.get()) == *cast_from_void<T*>(b.get())
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
            switch (_elementSize) {
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
        int32_t as_result = 0;
        FO_VERIFY_AND_THROW(nullable_ctx, "Script execution context is null");
        auto script_ctx = nullable_ctx.as_ptr();

        if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
            if (NativeDataProvider::ReadHandleSlot(a) == NativeDataProvider::ReadHandleSlot(b)) {
                return true;
            }
        }

        if (_subTypeData->EqFunc) {
            FO_AS_VERIFY(script_ctx->Prepare(_subTypeData->EqFunc.get_no_const()));

            if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                SetScriptObjectFromHandleSlot(script_ctx, a);
                SetScriptArgObjectFromHandleSlot(script_ctx, 0, b);
            }
            else {
                FO_AS_VERIFY(script_ctx->SetObject(a.get()));
                FO_AS_VERIFY(script_ctx->SetArgObject(0, b.get()));
            }

            FO_AS_VERIFY(script_ctx->Execute());

            if (as_result == AngelScript::asEXECUTION_FINISHED) {
                return script_ctx->GetReturnByte() != 0;
            }

            return false;
        }

        if (_subTypeData->CmpFunc) {
            FO_AS_VERIFY(script_ctx->Prepare(_subTypeData->CmpFunc.get_no_const()));

            if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                SetScriptObjectFromHandleSlot(script_ctx, a);
                SetScriptArgObjectFromHandleSlot(script_ctx, 0, b);
            }
            else {
                FO_AS_VERIFY(script_ctx->SetObject(a.get()));
                FO_AS_VERIFY(script_ctx->SetArgObject(0, b.get()));
            }

            FO_AS_VERIFY(script_ctx->Execute());

            if (as_result == AngelScript::asEXECUTION_FINISHED) {
                return std::bit_cast<int32_t>(script_ctx->GetReturnDWord()) == 0;
            }

            return false;
        }
    }

    return false;
}

auto ScriptArray::Less(ptr<void> a, ptr<void> b, bool asc, nptr<AngelScript::asIScriptContext> nullable_ctx) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!asc) {
        std::swap(a, b);
    }

    if (!_subTypeData) {
        switch (_subTypeId) {
#define COMPARE(T) *cast_from_void<T*>(a.get()) < *cast_from_void<T*>(b.get())
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
            switch (_elementSize) {
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
        int32_t as_result = 0;
        FO_VERIFY_AND_THROW(nullable_ctx, "Script execution context is null");
        auto script_ctx = nullable_ctx.as_ptr();
        nptr<void> lhs_obj {};
        nptr<void> rhs_obj {};

        if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
            lhs_obj = NativeDataProvider::ReadHandleSlot(a);
            rhs_obj = NativeDataProvider::ReadHandleSlot(b);

            if (!lhs_obj) {
                return true;
            }
            if (!rhs_obj) {
                return false;
            }
        }

        if (_subTypeData->CmpFunc) {
            FO_AS_VERIFY(script_ctx->Prepare(_subTypeData->CmpFunc.get_no_const()));

            if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                FO_AS_VERIFY(script_ctx->SetObject(lhs_obj.get()));
                FO_AS_VERIFY(script_ctx->SetArgObject(0, rhs_obj.get()));
            }
            else {
                FO_AS_VERIFY(script_ctx->SetObject(a.get()));
                FO_AS_VERIFY(script_ctx->SetArgObject(0, b.get()));
            }

            FO_AS_VERIFY(script_ctx->Execute());

            if (as_result == AngelScript::asEXECUTION_FINISHED) {
                return std::bit_cast<int32_t>(script_ctx->GetReturnDWord()) < 0;
            }
        }
    }

    return false;
}

void ScriptArray::Reverse()
{
    FO_STACK_TRACE_ENTRY();

    const int32_t size = GetSize();

    if (size >= 2) {
        AngelScript::asBYTE temp[16];

        for (int32_t i = 0; i < size / 2; i++) {
            Copy(temp, GetArrayItemPointer(i));
            Copy(GetArrayItemPointer(i), GetArrayItemPointer(size - i - 1));
            Copy(GetArrayItemPointer(size - i - 1), temp);
        }
    }
}

auto ScriptArray::operator==(const ScriptArray& other) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_typeInfo != other._typeInfo) {
        return false;
    }
    if (GetSize() != other.GetSize()) {
        return false;
    }

    if (_subTypeData) {
        if (!_subTypeData->CmpFunc && !_subTypeData->EqFunc && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
            ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
            nptr<const AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(_subTypeId);
            FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");

            if (_subTypeData->EqFuncReturnCode == AngelScript::asMULTIPLE_FUNCTIONS) {
                throw ScriptException("Type has multiple matching opEquals or opCmp methods", sub_type->GetName());
            }
            else {
                throw ScriptException("Type does not have a matching opEquals or opCmp method", sub_type->GetName());
            }
        }
    }

    nptr<AngelScript::asIScriptContext> ctx;

    if (_subTypeData) {
        ctx = AngelScript::asGetActiveContext();
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
        FO_VERIFY_AND_THROW(ctx->GetEngine() == _typeInfo->GetEngine(), "AngelScript array context belongs to a different engine");

        int32_t as_result = 0;
        FO_AS_VERIFY(ctx->PushState());
    }

    auto release_ctx = scope_exit([&]() noexcept {
        if (ctx) {
            safe_call([&] {
                const auto state = ctx->GetState();
                ctx->PopState();

                if (state == AngelScript::asEXECUTION_ABORTED) {
                    ctx->Abort();
                }
            });
        }
    });

    bool is_equal = true;

    for (int32_t i = 0; i < GetSize(); i++) {
        if (!Equals(At(i), other.At(i), ctx)) {
            is_equal = false;
            break;
        }
    }

    return is_equal;
}

auto ScriptArray::FindByRef(ptr<void> ref) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return FindByRef(0, ref);
}

auto ScriptArray::FindByRef(int32_t start_at, ptr<void> ref) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto size = GetSize();

    if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        const auto handle = NativeDataProvider::ReadHandleSlot(ref);

        for (int32_t i = start_at; i < size; i++) {
            if (NativeDataProvider::ReadHandleSlot(At(i)) == handle) {
                return i;
            }
        }
    }
    else {
        for (int32_t i = start_at; i < size; i++) {
            if (At(i) == ref) {
                return i;
            }
        }
    }

    return -1;
}

auto ScriptArray::Find(ptr<void> value) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return Find(0, value);
}

auto ScriptArray::Find(int32_t start_at, ptr<void> value) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (_subTypeData) {
        if (!_subTypeData->CmpFunc && !_subTypeData->EqFunc && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
            ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
            nptr<const AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(_subTypeId);
            FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");

            if (_subTypeData->EqFuncReturnCode == AngelScript::asMULTIPLE_FUNCTIONS) {
                throw ScriptException("Type has multiple matching opEquals or opCmp methods", sub_type->GetName());
            }
            else {
                throw ScriptException("Type does not have a matching opEquals or opCmp method", sub_type->GetName());
            }
        }
    }

    nptr<AngelScript::asIScriptContext> ctx;

    if (_subTypeData) {
        ctx = AngelScript::asGetActiveContext();
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
        FO_VERIFY_AND_THROW(ctx->GetEngine() == _typeInfo->GetEngine(), "AngelScript array context belongs to a different engine");

        int32_t as_result = 0;
        FO_AS_VERIFY(ctx->PushState());
    }

    auto release_ctx = scope_exit([&]() noexcept {
        if (ctx) {
            safe_call([&] {
                const auto state = ctx->GetState();
                ctx->PopState();

                if (state == AngelScript::asEXECUTION_ABORTED) {
                    ctx->Abort();
                }
            });
        }
    });

    int32_t ret = -1;
    const int32_t size = GetSize();

    for (int32_t i = start_at; i < size; i++) {
        if (Equals(At(i), value, ctx)) {
            ret = i;
            break;
        }
    }

    return ret;
}

void ScriptArray::Copy(ptr<void> dst, ptr<void> src) const
{
    FO_STACK_TRACE_ENTRY();

    MemCopy(dst, src, numeric_cast<size_t>(_elementSize));
}

auto ScriptArray::GetArrayItemPointer(int32_t index) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    auto buffer = GetBuffer();
    FO_VERIFY_AND_THROW(buffer, "Array buffer is null");
    return void_ptr_offset(buffer.get(), numeric_cast<size_t>(index * _elementSize));
}

auto ScriptArray::GetArrayItemPointer(int32_t index) const -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    auto buffer = GetBuffer();
    FO_VERIFY_AND_THROW(buffer, "Array buffer is null");
    return void_ptr_offset(buffer.get(), numeric_cast<size_t>(index * _elementSize));
}

auto ScriptArray::GetDataPointer(ptr<void> buf) const -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        auto nullable_object = NativeDataProvider::ReadHandleSlot(buf);
        FO_VERIFY_AND_THROW(nullable_object, "Array element object is null");
        return nullable_object.as_ptr();
    }
    else {
        return buf;
    }
}

void ScriptArray::SortAsc()
{
    FO_STACK_TRACE_ENTRY();

    Sort(0, GetSize(), true);
}

void ScriptArray::SortAsc(int32_t start_at, int32_t count)
{
    FO_STACK_TRACE_ENTRY();

    Sort(start_at, count, true);
}

void ScriptArray::SortDesc()
{
    FO_STACK_TRACE_ENTRY();

    Sort(0, GetSize(), false);
}

void ScriptArray::SortDesc(int32_t start_at, int32_t count)
{
    FO_STACK_TRACE_ENTRY();

    Sort(start_at, count, false);
}

void ScriptArray::Sort(int32_t start_at, int32_t count, bool asc)
{
    FO_STACK_TRACE_ENTRY();

    if (_subTypeData) {
        if (!_subTypeData->CmpFunc) {
            ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
            nptr<const AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(_subTypeId);
            FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");

            if (_subTypeData->CmpFuncReturnCode == AngelScript::asMULTIPLE_FUNCTIONS) {
                throw ScriptException("Type has multiple matching opCmp methods", sub_type->GetName());
            }
            else {
                throw ScriptException("Type does not have a matching opCmp method", sub_type->GetName());
            }
        }
    }

    if (count < 2) {
        return;
    }

    const int32_t start = start_at;
    const int32_t end = start_at + count;

    if (start < 0 || end < 0 || start >= GetSize() || end > GetSize()) {
        throw ScriptException("Index out of bounds", start, end, GetSize());
    }

    nptr<AngelScript::asIScriptContext> ctx;

    if (_subTypeData) {
        ctx = AngelScript::asGetActiveContext();
        FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
        FO_VERIFY_AND_THROW(ctx->GetEngine() == _typeInfo->GetEngine(), "AngelScript array context belongs to a different engine");

        int32_t as_result = 0;
        FO_AS_VERIFY(ctx->PushState());
    }

    auto release_ctx = scope_exit([&]() noexcept {
        if (ctx) {
            safe_call([&] {
                const auto state = ctx->GetState();
                ctx->PopState();

                if (state == AngelScript::asEXECUTION_ABORTED) {
                    ctx->Abort();
                }
            });
        }
    });

    // Insertion sort
    for (int32_t i = start + 1; i < end; i++) {
        AngelScript::asBYTE tmp[16];
        Copy(tmp, GetArrayItemPointer(i));

        int32_t j = i - 1;

        while (j >= start && Less(GetDataPointer(tmp), At(j), asc, ctx)) {
            Copy(GetArrayItemPointer(j + 1), GetArrayItemPointer(j));
            j--;
        }

        Copy(GetArrayItemPointer(j + 1), tmp);
    }
}

void ScriptArray::CopyBuffer(const ScriptArray& src)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
    const auto count = std::min(GetSize(), src.GetSize());

    if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        if (count != 0) {
            nptr<AngelScript::asITypeInfo> sub_type = _typeInfo->GetSubType();
            FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");

            for (int32_t index = 0; index < count; index++) {
                auto dst_slot = NativeDataProvider::GetHandleSlot(GetArrayItemPointer(index));
                auto src_slot = NativeDataProvider::GetHandleSlot(src.GetArrayItemPointer(index));
                const nptr<void> old_obj = *dst_slot;
                const nptr<void> new_obj = *src_slot;
                *dst_slot = new_obj.get_no_const();

                if (new_obj) {
                    engine->AddRefScriptObject(new_obj.get_no_const(), sub_type.get());
                }

                if (old_obj) {
                    engine->ReleaseScriptObject(old_obj.get_no_const(), sub_type.get());
                }
            }
        }
    }
    else if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        if (count != 0) {
            nptr<AngelScript::asITypeInfo> sub_type = _typeInfo->GetSubType();
            FO_VERIFY_AND_THROW(!!sub_type, "Array sub-type info not found");

            for (int32_t index = 0; index < count; index++) {
                auto dst_slot = NativeDataProvider::GetHandleSlot(GetArrayItemPointer(index));
                auto src_slot = NativeDataProvider::GetHandleSlot(src.GetArrayItemPointer(index));
                engine->AssignScriptObject(*dst_slot, *src_slot, sub_type.get());
            }
        }
    }
    else if (count != 0) {
        auto dst_buffer = GetBuffer().as_ptr();
        auto src_buffer = src.GetBuffer().as_ptr();
        MemCopy(dst_buffer, src_buffer, numeric_cast<size_t>(count * _elementSize));
    }
}

void ScriptArray::PrecacheSubTypeData()
{
    FO_STACK_TRACE_ENTRY();

    if ((_subTypeId & ~AngelScript::asTYPEID_MASK_SEQNBR) == 0) {
        return;
    }

    _subTypeData = cast_from_void<ScriptArrayTypeData*>(_typeInfo->GetUserData(AS_TYPE_ARRAY_CACHE));

    if (_subTypeData) {
        return;
    }

    AngelScript::asAcquireExclusiveLock();
    auto release_lock = scope_exit([]() noexcept { AngelScript::asReleaseExclusiveLock(); });

    _subTypeData = cast_from_void<ScriptArrayTypeData*>(_typeInfo->GetUserData(AS_TYPE_ARRAY_CACHE));

    if (_subTypeData) {
        return;
    }

    _subTypeData = SafeAlloc::MakeRaw<ScriptArrayTypeData>();

    const bool must_be_const = (_subTypeId & AngelScript::asTYPEID_HANDLETOCONST) != 0;
    ptr<AngelScript::asIScriptEngine> engine = _typeInfo->GetEngine();
    nptr<AngelScript::asITypeInfo> sub_type = engine->GetTypeInfoById(_subTypeId);

    if (sub_type) {
        for (AngelScript::asUINT i = 0; i < sub_type->GetMethodCount(); i++) {
            nptr<AngelScript::asIScriptFunction> func = sub_type->GetMethodByIndex(i);

            if (func && func->GetParamCount() == 1 && (!must_be_const || func->IsReadOnly())) {
                AngelScript::asDWORD flags = 0;
                const int32_t return_type_id = func->GetReturnTypeId(&flags);

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

                if ((param_type_id & ~(AngelScript::asTYPEID_OBJHANDLE | AngelScript::asTYPEID_HANDLETOCONST)) != (_subTypeId & ~(AngelScript::asTYPEID_OBJHANDLE | AngelScript::asTYPEID_HANDLETOCONST))) {
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
                    if (_subTypeData->CmpFunc || _subTypeData->CmpFuncReturnCode != AngelScript::asSUCCESS) {
                        _subTypeData->CmpFunc.reset();
                        _subTypeData->CmpFuncReturnCode = AngelScript::asMULTIPLE_FUNCTIONS;
                    }
                    else {
                        _subTypeData->CmpFunc = func;
                    }
                }
                else if (is_eq) {
                    if (_subTypeData->EqFunc || _subTypeData->EqFuncReturnCode != AngelScript::asSUCCESS) {
                        _subTypeData->EqFunc.reset();
                        _subTypeData->EqFuncReturnCode = AngelScript::asMULTIPLE_FUNCTIONS;
                    }
                    else {
                        _subTypeData->EqFunc = func;
                    }
                }
            }
        }
    }

    if (!_subTypeData->EqFunc && _subTypeData->EqFuncReturnCode == AngelScript::asSUCCESS) {
        _subTypeData->EqFuncReturnCode = AngelScript::asNO_FUNCTION;
    }
    if (!_subTypeData->CmpFunc && _subTypeData->CmpFuncReturnCode == AngelScript::asSUCCESS) {
        _subTypeData->CmpFuncReturnCode = AngelScript::asNO_FUNCTION;
    }

    auto sub_type_data = _subTypeData.as_ptr();
    _typeInfo->SetUserData(cast_to_void(sub_type_data.get()), AS_TYPE_ARRAY_CACHE);
}

void ScriptArray::EnumReferences(ptr<AngelScript::asIScriptEngine> engine)
{
    FO_STACK_TRACE_ENTRY();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        for (int32_t i = 0; i < GetSize(); i++) {
            const auto obj = NativeDataProvider::ReadHandleSlot(GetArrayItemPointer(i));

            if (obj) {
                engine->GCEnumCallback(obj.get_no_const());
            }
        }
    }
}

void ScriptArray::ReleaseAllHandles()
{
    FO_STACK_TRACE_ENTRY();

    Resize(0);
}

void ScriptArray::AddRef() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag.store(false, std::memory_order_relaxed);
    _refCount.fetch_add(1, std::memory_order_acq_rel);
}

void ScriptArray::Release() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag.store(false, std::memory_order_relaxed);

    if (_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete this;
    }
}

auto ScriptArray::GetRefCount() const -> int32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return _refCount.load(std::memory_order_relaxed);
}

void ScriptArray::SetFlag() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag.store(true, std::memory_order_relaxed);
}

auto ScriptArray::GetFlag() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _gcFlag.load(std::memory_order_relaxed);
}

static auto ScriptArray_InsertFirst(ScriptArray& arr, void* value) -> void
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    arr.InsertAt(0, value_ptr);
}

static auto ScriptArray_InsertAt(ScriptArray& arr, int32_t index, void* value) -> void
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    arr.InsertAt(index, value_ptr);
}

static auto ScriptArray_InsertLast(ScriptArray& arr, void* value) -> void
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    arr.InsertLast(value_ptr);
}

static auto ScriptArray_RemoveFirst(ScriptArray& arr) -> void
{
    FO_STACK_TRACE_ENTRY();

    arr.RemoveAt(0);
}

static auto ScriptArray_Grow(ScriptArray& arr, int32_t count) -> void
{
    FO_STACK_TRACE_ENTRY();

    if (count <= 0) {
        return;
    }

    arr.Resize(arr.GetSize() + count);
}

static auto ScriptArray_Reduce(ScriptArray& arr, int32_t count) -> void
{
    FO_STACK_TRACE_ENTRY();

    if (count <= 0) {
        return;
    }

    const auto size = numeric_cast<int32_t>(arr.GetSize());

    if (count > size) {
        throw ScriptException("Array size is less than reduce count", count, size);
    }

    arr.Resize(size - count);
}

static auto ScriptArray_First(ScriptArray& arr) -> void*
{
    FO_STACK_TRACE_ENTRY();

    ptr<void> value = arr.At(0);
    return value.get();
}

static auto ScriptArray_Last(ScriptArray& arr) -> void*
{
    FO_STACK_TRACE_ENTRY();

    ptr<void> value = arr.At(arr.GetSize() - 1);
    return value.get();
}

static auto ScriptArray_At(ScriptArray& arr, int32_t index) -> void*
{
    FO_STACK_TRACE_ENTRY();

    ptr<void> value = arr.At(index);
    return value.get();
}

static void ScriptArray_Clear(ScriptArray& arr)
{
    FO_STACK_TRACE_ENTRY();

    if (arr.GetSize() > 0) {
        arr.Resize(0);
    }
}

static auto ScriptArray_Exists(const ScriptArray& arr, void* value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    return arr.Find(0, value_ptr) != -1;
}

static auto ScriptArray_Remove(ScriptArray& arr, void* value) -> bool
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    const int32_t index = arr.Find(0, value_ptr);

    if (index != -1) {
        arr.RemoveAt(index);
        return true;
    }

    return false;
}

static auto ScriptArray_RemoveAll(ScriptArray& arr, void* value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    int32_t count = 0;
    int32_t index = 0;

    while (index < numeric_cast<int32_t>(arr.GetSize())) {
        index = arr.Find(index, value_ptr);

        if (index != -1) {
            arr.RemoveAt(index);
            count++;
        }
        else {
            break;
        }
    }

    return count;
}

static auto ScriptArray_Find(const ScriptArray& arr, void* value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    return arr.Find(value_ptr);
}

static auto ScriptArray_FindFrom(const ScriptArray& arr, int32_t start_at, void* value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    return arr.Find(start_at, value_ptr);
}

static auto ScriptArray_FindByRef(const ScriptArray& arr, void* value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    return arr.FindByRef(value_ptr);
}

static auto ScriptArray_FindByRefFrom(const ScriptArray& arr, int32_t start_at, void* value) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> value_arg = value;
    auto value_ptr = RequireScriptArrayValue(value_arg);
    return arr.FindByRef(start_at, value_ptr);
}

static auto ScriptArray_Factory(AngelScript::asITypeInfo* ti, const ScriptArray* other) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asITypeInfo> type_info = ti;
    FO_VERIFY_AND_THROW(!!type_info, "Array type info is null");
    const nptr<const ScriptArray> other_ptr = other;

    if (!other_ptr) {
        throw ScriptException("Array arg is null");
    }

    auto clone = ScriptArray::Create(type_info.as_ptr());
    *clone = *other_ptr;
    return ReleaseScriptOwnership(std::move(clone));
}

static auto ScriptArray_Clone(const ScriptArray& arr) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asITypeInfo> type_info = ScriptMutablePtr(arr.GetArrayObjectType());
    auto clone = ScriptArray::Create(type_info);
    *clone = arr;
    return ReleaseScriptOwnership(std::move(clone));
}

static void ScriptArray_EnumReferences(ScriptArray& arr, AngelScript::asIScriptEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asIScriptEngine> engine_arg = engine;
    FO_VERIFY_AND_THROW(engine_arg, "Script engine is null");
    arr.EnumReferences(engine_arg.as_ptr());
}

static void ScriptArray_ReleaseAllHandles(ScriptArray& arr, AngelScript::asIScriptEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asIScriptEngine> engine_arg = engine;
    FO_VERIFY_AND_THROW(engine_arg, "Script engine is null");
    arr.ReleaseAllHandles();
}

static void ScriptArray_Set(ScriptArray& arr, const ScriptArray* other)
{
    FO_STACK_TRACE_ENTRY();

    const nptr<const ScriptArray> other_ptr = other;

    if (!other_ptr) {
        throw ScriptException("Array arg is null");
    }

    arr = *other_ptr;
}

static void ScriptArray_InsertArrAt(ScriptArray& arr, int32_t index, const ScriptArray* other)
{
    FO_STACK_TRACE_ENTRY();

    if (index < 0) {
        return;
    }

    const nptr<const ScriptArray> other_ptr = other;

    if (!other_ptr) {
        throw ScriptException("Array arg is null");
    }

    arr.InsertAt(index, *other_ptr);
}

static void ScriptArray_InsertArrFirst(ScriptArray& arr, const ScriptArray* other)
{
    FO_STACK_TRACE_ENTRY();

    const nptr<const ScriptArray> other_ptr = other;

    if (!other_ptr) {
        throw ScriptException("Array arg is null");
    }

    arr.InsertAt(0, *other_ptr);
}

static void ScriptArray_InsertArrLast(ScriptArray& arr, const ScriptArray* other)
{
    FO_STACK_TRACE_ENTRY();

    const nptr<const ScriptArray> other_ptr = other;

    if (!other_ptr) {
        throw ScriptException("Array arg is null");
    }

    arr.InsertAt(arr.GetSize(), *other_ptr);
}

static auto ScriptArray_Equals(ScriptArray& arr, const ScriptArray* other) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const nptr<const ScriptArray> other_ptr = other;

    if (!other_ptr) {
        throw ScriptException("Array arg is null");
    }

    return arr == *other_ptr;
}

void RegisterAngelScriptArray(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    as_engine->SetTypeInfoUserDataCleanupCallback(CleanupTypeInfoArrayCache, AS_TYPE_ARRAY_CACHE);

    int32_t as_result = 0;
    FO_AS_VERIFY(as_engine->RegisterObjectType("array<class T>", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_GC | AngelScript::asOBJ_TEMPLATE));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", FO_SCRIPT_FUNC(ScriptArrayTemplateCallback), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_FACTORY, "array<T>@ f(int&in)", FO_SCRIPT_FUNC(ScriptArray_Create), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_FACTORY, "array<T>@ f(int&in, int length)", FO_SCRIPT_FUNC(ScriptArray_CreateWithLength), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_FACTORY, "array<T>@ f(int&in, int length, const T&in value)", FO_SCRIPT_FUNC(ScriptArray_CreateWithDefault), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_FACTORY, "array<T>@ f(int&in, const array<T>@+)", FO_SCRIPT_FUNC(ScriptArray_Factory), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_LIST_FACTORY, "array<T>@ f(int&in type, int&in list) {repeat T}", FO_SCRIPT_FUNC(ScriptArray_CreateList), FO_SCRIPT_FUNC_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_METHOD(ScriptArray, AddRef), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_METHOD(ScriptArray, Release), FO_SCRIPT_METHOD_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "T &opIndex(int index)", FO_SCRIPT_FUNC_THIS(ScriptArray_At), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "const T &opIndex(int index) const", FO_SCRIPT_FUNC_THIS(ScriptArray_At), FO_SCRIPT_FUNC_THIS_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void insertAt(int index, const T&in value)", FO_SCRIPT_FUNC_THIS(ScriptArray_InsertAt), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void insertLast(const T&in value)", FO_SCRIPT_FUNC_THIS(ScriptArray_InsertLast), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void removeAt(int index)", FO_SCRIPT_METHOD(ScriptArray, RemoveAt), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void removeLast()", FO_SCRIPT_METHOD(ScriptArray, RemoveLast), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void removeRange(int start, int count)", FO_SCRIPT_METHOD(ScriptArray, RemoveRange), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "int length() const", FO_SCRIPT_METHOD(ScriptArray, GetSize), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void reserve(int length)", FO_SCRIPT_METHOD(ScriptArray, Reserve), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void resize(int length)", FO_SCRIPT_METHOD_EXT(ScriptArray, Resize, (int32_t), void), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void sortAsc()", FO_SCRIPT_METHOD_EXT(ScriptArray, SortAsc, (), void), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void sortAsc(int startAt, int count)", FO_SCRIPT_METHOD_EXT(ScriptArray, SortAsc, (int32_t, int32_t), void), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void sortDesc()", FO_SCRIPT_METHOD_EXT(ScriptArray, SortDesc, (), void), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void sortDesc(int startAt, int count)", FO_SCRIPT_METHOD_EXT(ScriptArray, SortDesc, (int32_t, int32_t), void), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void reverse()", FO_SCRIPT_METHOD(ScriptArray, Reverse), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "int find(const T&in if_handle_then_const value) const", FO_SCRIPT_FUNC_THIS(ScriptArray_Find), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "int find(int startAt, const T&in if_handle_then_const value) const", FO_SCRIPT_FUNC_THIS(ScriptArray_FindFrom), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "int findByRef(const T&in if_handle_then_const value) const", FO_SCRIPT_FUNC_THIS(ScriptArray_FindByRef), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "int findByRef(int startAt, const T&in if_handle_then_const value) const", FO_SCRIPT_FUNC_THIS(ScriptArray_FindByRefFrom), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "bool isEmpty() const", FO_SCRIPT_METHOD(ScriptArray, IsEmpty), FO_SCRIPT_METHOD_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_GETREFCOUNT, "int f()", FO_SCRIPT_METHOD(ScriptArray, GetRefCount), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_SETGCFLAG, "void f()", FO_SCRIPT_METHOD(ScriptArray, SetFlag), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_GETGCFLAG, "bool f()", FO_SCRIPT_METHOD(ScriptArray, GetFlag), FO_SCRIPT_METHOD_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_ENUMREFS, "void f(int&in)", FO_SCRIPT_FUNC_THIS(ScriptArray_EnumReferences), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_RELEASEREFS, "void f(int&in)", FO_SCRIPT_FUNC_THIS(ScriptArray_ReleaseAllHandles), FO_SCRIPT_FUNC_THIS_CONV));

    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void insertFirst(const T&in)", FO_SCRIPT_FUNC_THIS(ScriptArray_InsertFirst), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void removeFirst()", FO_SCRIPT_FUNC_THIS(ScriptArray_RemoveFirst), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void grow(int)", FO_SCRIPT_FUNC_THIS(ScriptArray_Grow), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void reduce(int)", FO_SCRIPT_FUNC_THIS(ScriptArray_Reduce), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "T& first()", FO_SCRIPT_FUNC_THIS(ScriptArray_First), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "const T& first() const", FO_SCRIPT_FUNC_THIS(ScriptArray_First), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "T& last()", FO_SCRIPT_FUNC_THIS(ScriptArray_Last), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "const T& last() const", FO_SCRIPT_FUNC_THIS(ScriptArray_Last), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void clear()", FO_SCRIPT_FUNC_THIS(ScriptArray_Clear), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "bool exists(const T&in) const", FO_SCRIPT_FUNC_THIS(ScriptArray_Exists), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "bool remove(const T&in)", FO_SCRIPT_FUNC_THIS(ScriptArray_Remove), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "int removeAll(const T&in)", FO_SCRIPT_FUNC_THIS(ScriptArray_RemoveAll), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "array<T>@ clone() const", FO_SCRIPT_FUNC_THIS(ScriptArray_Clone), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void set(const array<T>@+)", FO_SCRIPT_FUNC_THIS(ScriptArray_Set), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void insertAt(int, const array<T>@+)", FO_SCRIPT_FUNC_THIS(ScriptArray_InsertArrAt), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void insertFirst(const array<T>@+)", FO_SCRIPT_FUNC_THIS(ScriptArray_InsertArrFirst), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "void insertLast(const array<T>@+)", FO_SCRIPT_FUNC_THIS(ScriptArray_InsertArrLast), FO_SCRIPT_FUNC_THIS_CONV));
    FO_AS_VERIFY(as_engine->RegisterObjectMethod("array<T>", "bool opEquals(const array<T>@+) const", FO_SCRIPT_FUNC_THIS(ScriptArray_Equals), FO_SCRIPT_FUNC_THIS_CONV));

    FO_AS_VERIFY(as_engine->RegisterDefaultArrayType("array<T>"));
}

FO_END_NAMESPACE

#endif
