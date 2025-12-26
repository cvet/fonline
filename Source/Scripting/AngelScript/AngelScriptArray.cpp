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

#include "AngelScriptArray.h"
#include "AngelScriptWrappedCall.h"

FO_BEGIN_NAMESPACE();

constexpr AngelScript::asPWORD AS_TYPE_ARRAY_CACHE = 1000;

struct ScriptArrayTypeData
{
    raw_ptr<AngelScript::asIScriptFunction> CmpFunc {};
    raw_ptr<AngelScript::asIScriptFunction> EqFunc {};
    int32 CmpFuncReturnCode {};
    int32 EqFuncReturnCode {};
};

static void CleanupTypeInfoArrayCache(AngelScript::asITypeInfo* type)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* cache = static_cast<ScriptArrayTypeData*>(type->GetUserData(AS_TYPE_ARRAY_CACHE));
    delete cache;
    type->SetUserData(nullptr, AS_TYPE_ARRAY_CACHE);
}

auto ScriptArray::Create(AngelScript::asITypeInfo* ti, int32 length) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptArray>(length, ti).release_ownership();
}

auto ScriptArray::Create(AngelScript::asITypeInfo* ti, void* init_list) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptArray>(ti, init_list).release_ownership();
}

auto ScriptArray::Create(AngelScript::asITypeInfo* ti, int32 length, void* def_val) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRefCounted<ScriptArray>(length, def_val, ti).release_ownership();
}

auto ScriptArray::Create(AngelScript::asITypeInfo* ti) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    return Create(ti, 0);
}

static auto ScriptArrayTemplateCallback(AngelScript::asITypeInfo* ti, bool& dont_garbage_collect) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* engine = ti->GetEngine();
    const int32 type_id = ti->GetSubTypeId();

    if (type_id == AngelScript::asTYPEID_VOID) {
        return false;
    }

    if ((type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (type_id & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        const auto* sub_type = engine->GetTypeInfoById(type_id);
        const auto flags = sub_type->GetFlags();

        if ((flags & AngelScript::asOBJ_VALUE) != 0 && (flags & AngelScript::asOBJ_POD) == 0) {
            bool has_default_ctor = false;

            for (AngelScript::asUINT i = 0; i < sub_type->GetBehaviourCount(); i++) {
                AngelScript::asEBehaviours beh;
                const auto* func = sub_type->GetBehaviourByIndex(i, &beh);

                if (beh == AngelScript::asBEHAVE_CONSTRUCT && func->GetParamCount() == 0) {
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

ScriptArray::ScriptArray(AngelScript::asITypeInfo* ti, void* init_list) :
    _typeInfo {ti},
    _subTypeId {_typeInfo->GetSubTypeId()}
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(ti && string_view(ti->GetName()) == "array");

    auto* engine = ti->GetEngine();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        _elementSize = sizeof(void*);
    }
    else {
        _elementSize = engine->GetSizeOfPrimitiveType(_subTypeId);
    }

    PrecacheSubTypeData();

    const int32 length = *static_cast<int32*>(init_list);
    CheckArraySize(length);

    if ((ti->GetSubTypeId() & AngelScript::asTYPEID_MASK_OBJECT) == 0) {
        CreateBuffer(length);

        if (length != 0) {
            MemCopy(At(0), static_cast<int32*>(init_list) + 1, numeric_cast<size_t>(length * _elementSize));
        }
    }
    else if ((ti->GetSubTypeId() & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        CreateBuffer(length);

        if (length != 0) {
            MemCopy(At(0), static_cast<int32*>(init_list) + 1, numeric_cast<size_t>(length * _elementSize));
            MemFill(static_cast<int32*>(init_list) + 1, 0, numeric_cast<size_t>(length * _elementSize));
        }
    }
    else {
        CreateBuffer(length);

        for (int32 i = 0; i < length; i++) {
            void* obj = At(i);
            auto* src_obj = static_cast<AngelScript::asBYTE*>(init_list);
            src_obj += sizeof(int32) + numeric_cast<size_t>(i * ti->GetSubType()->GetSize());
            engine->AssignScriptObject(obj, src_obj, ti->GetSubType());
        }
    }

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        _typeInfo->GetEngine()->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }
}

ScriptArray::ScriptArray(int32 length, AngelScript::asITypeInfo* ti) :
    _typeInfo {ti},
    _subTypeId {_typeInfo->GetSubTypeId()}
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(ti && string(ti->GetName()) == "array");

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        _elementSize = sizeof(void*);
    }
    else {
        _elementSize = _typeInfo->GetEngine()->GetSizeOfPrimitiveType(_subTypeId);
    }

    PrecacheSubTypeData();
    CheckArraySize(length);
    CreateBuffer(length);

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        _typeInfo->GetEngine()->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }
}

ScriptArray::ScriptArray(int32 length, void* def_val, AngelScript::asITypeInfo* ti) :
    _typeInfo {ti},
    _subTypeId {_typeInfo->GetSubTypeId()}
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(ti && string(ti->GetName()) == "array");

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        _elementSize = sizeof(void*);
    }
    else {
        _elementSize = _typeInfo->GetEngine()->GetSizeOfPrimitiveType(_subTypeId);
    }

    PrecacheSubTypeData();
    CheckArraySize(length);
    CreateBuffer(length);

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        _typeInfo->GetEngine()->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }

    for (int32 i = 0; i < GetSize(); i++) {
        SetValue(i, def_val);
    }
}

ScriptArray::ScriptArray(const ScriptArray& other) :
    _typeInfo {other._typeInfo},
    _subTypeId {other._subTypeId}
{
    FO_NO_STACK_TRACE_ENTRY();

    _elementSize = other._elementSize;

    PrecacheSubTypeData();

    if ((_typeInfo->GetFlags() & AngelScript::asOBJ_GC) != 0) {
        _typeInfo->GetEngine()->NotifyGarbageCollectorOfNewObject(this, _typeInfo.get());
    }

    CreateBuffer(0);
    *this = other;
}

auto ScriptArray::operator=(const ScriptArray& other) -> ScriptArray&
{
    FO_NO_STACK_TRACE_ENTRY();

    if (other._typeInfo != _typeInfo) {
        throw ScriptException("Different types on array assignment");
    }

    if (&other != this) {
        Resize(other.GetSize());
        CopyBuffer(other);
    }

    return *this;
}

void ScriptArray::SetValue(int32 index, void* value)
{
    FO_NO_STACK_TRACE_ENTRY();

    void* ptr = At(index);

    if (ptr == nullptr) {
        return;
    }

    if ((_subTypeId & ~AngelScript::asTYPEID_MASK_SEQNBR) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        _typeInfo->GetEngine()->AssignScriptObject(ptr, value, _typeInfo->GetSubType());
    }
    else if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        void* tmp = *static_cast<void**>(ptr);
        *static_cast<void**>(ptr) = *static_cast<void**>(value);

        if (*static_cast<void**>(value) != nullptr) {
            _typeInfo->GetEngine()->AddRefScriptObject(*static_cast<void**>(value), _typeInfo->GetSubType());
        }
        if (tmp != nullptr) {
            _typeInfo->GetEngine()->ReleaseScriptObject(tmp, _typeInfo->GetSubType());
        }
    }
    else if (_subTypeId == AngelScript::asTYPEID_BOOL || _subTypeId == AngelScript::asTYPEID_INT8 || _subTypeId == AngelScript::asTYPEID_UINT8) {
        *static_cast<char*>(ptr) = *static_cast<char*>(value);
    }
    else if (_subTypeId == AngelScript::asTYPEID_INT16 || _subTypeId == AngelScript::asTYPEID_UINT16) {
        *static_cast<short*>(ptr) = *static_cast<short*>(value);
    }
    else if (_subTypeId == AngelScript::asTYPEID_INT32 || _subTypeId == AngelScript::asTYPEID_UINT32 || _subTypeId == AngelScript::asTYPEID_FLOAT || _subTypeId > AngelScript::asTYPEID_DOUBLE) { // enums have a type id larger than doubles
        *static_cast<int32*>(ptr) = *static_cast<int32*>(value);
    }
    else if (_subTypeId == AngelScript::asTYPEID_INT64 || _subTypeId == AngelScript::asTYPEID_UINT64 || _subTypeId == AngelScript::asTYPEID_DOUBLE) {
        *static_cast<double*>(ptr) = *static_cast<double*>(value);
    }
}

ScriptArray::~ScriptArray()
{
    FO_NO_STACK_TRACE_ENTRY();

    DeleteBuffer();
}

void ScriptArray::Reserve(int32 max_elements)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (max_elements <= GetCapacity()) {
        return;
    }

    CheckArraySize(max_elements);
    _buffer.reserve(numeric_cast<size_t>(max_elements * _elementSize));
}

void ScriptArray::Resize(int32 num_elements)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (num_elements == GetSize()) {
        return;
    }

    CheckArraySize(num_elements);
    Resize(num_elements - GetSize(), std::numeric_limits<int32>::max());
}

void ScriptArray::RemoveRange(int32 start, int32 num_elements)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (num_elements <= 0) {
        return;
    }

    if (start < 0 || start > GetSize()) {
        throw ScriptException("Index out of bounds");
    }

    if (start + num_elements > GetSize()) {
        num_elements = GetSize() - start;
    }

    Destruct(start, start + num_elements);
}

void ScriptArray::Resize(int32 delta, int32 at)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (delta < 0) {
        FO_RUNTIME_ASSERT(-delta <= GetSize());
        at = std::clamp(at, 0, GetSize() + delta);
        Destruct(at, at - delta);
    }
    else if (delta > 0) {
        CheckArraySize(GetSize() + delta);
        at = std::clamp(at, 0, GetSize());
        Construct(at, at + delta);
    }
}

void ScriptArray::CheckArraySize(int32 num_elements) const
{
    FO_NO_STACK_TRACE_ENTRY();

    if (num_elements < 0) {
        throw ScriptException("Negative array size", num_elements);
    }

    FO_RUNTIME_ASSERT(_elementSize != 0);
    const int32 max_size = 0x7FFFFFFF / _elementSize;

    if (num_elements > max_size) {
        throw ScriptException("Array size is too large", num_elements, max_size);
    }
}

auto ScriptArray::GetArrayObjectType() -> AngelScript::asITypeInfo*
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo.get();
}

auto ScriptArray::GetArrayObjectType() const -> const AngelScript::asITypeInfo*
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo.get();
}

auto ScriptArray::GetArrayTypeId() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return _typeInfo->GetTypeId();
}

auto ScriptArray::GetElementTypeId() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return _subTypeId;
}

void ScriptArray::InsertAt(int32 index, void* value)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0 || index > GetSize()) {
        throw ScriptException("Index out of bounds");
    }

    Resize(1, index);
    SetValue(index, value);
}

void ScriptArray::InsertAt(int32 index, const ScriptArray& other)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0 || index > GetSize()) {
        throw ScriptException("Index out of bounds");
    }
    if (_typeInfo != other._typeInfo) {
        throw ScriptException("Mismatching array types");
    }

    const auto num_elements = other.GetSize();
    Resize(num_elements, index);

    if (&other != this) {
        for (int32 i = 0; i < other.GetSize(); i++) {
            auto* value = const_cast<void*>(other.At(i));
            SetValue(index + i, value);
        }
    }
    else {
        for (int32 i = 0; i < index; i++) {
            auto* value = const_cast<void*>(other.At(i));
            SetValue(index + i, value);
        }
        for (int32 i = index + num_elements, j = 0; i < other.GetSize(); i++, j++) {
            auto* value = const_cast<void*>(other.At(i));
            SetValue(index + index + j, value);
        }
    }
}

void ScriptArray::InsertLast(void* value)
{
    FO_NO_STACK_TRACE_ENTRY();

    InsertAt(GetSize(), value);
}

void ScriptArray::RemoveAt(int32 index)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= GetSize()) {
        throw ScriptException("Index out of bounds");
    }

    Resize(-1, index);
}

void ScriptArray::RemoveLast()
{
    FO_NO_STACK_TRACE_ENTRY();

    RemoveAt(GetSize() - 1);
}

auto ScriptArray::At(int32 index) const -> const void*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= GetSize()) {
        throw ScriptException("Index out of bounds");
    }

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        return *static_cast<void* const*>(GetArrayItemPointer(index));
    }
    else {
        return GetArrayItemPointer(index);
    }
}

auto ScriptArray::At(int32 index) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0 || index >= GetSize()) {
        throw ScriptException("Index out of bounds");
    }

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        return *static_cast<void**>(GetArrayItemPointer(index));
    }
    else {
        return GetArrayItemPointer(index);
    }
}

void ScriptArray::CreateBuffer(int32 num_elements)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(num_elements >= 0);

    Construct(0, num_elements);
}

void ScriptArray::DeleteBuffer() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    safe_call([this] { Destruct(0, GetSize()); });
}

void ScriptArray::Construct(int32 start, int32 end)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(start <= end);

    if (start == end) {
        return;
    }

    // Handle capacity on insert last
    if (start == end - 1 && _buffer.size() >= _buffer.capacity()) {
        _buffer.reserve(std::max(_buffer.capacity(), static_cast<size_t>(10 * _elementSize)));
    }

    const auto it_start = _buffer.begin() + numeric_cast<ptrdiff_t>(start * _elementSize);
    const auto count = numeric_cast<size_t>((end - start) * _elementSize);
    _buffer.insert(it_start, count, {});

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        auto** max = static_cast<void**>(GetArrayItemPointer(end));
        auto** d = static_cast<void**>(GetArrayItemPointer(start));

        auto* engine = _typeInfo->GetEngine();
        const auto* sub_type = _typeInfo->GetSubType();

        for (; d < max; d++) {
            *d = engine->CreateScriptObject(sub_type);

            if (*d == nullptr) {
                return;
            }
        }
    }
}

void ScriptArray::Destruct(int32 start, int32 end)
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        auto** max = static_cast<void**>(GetArrayItemPointer(end));
        auto** d = static_cast<void**>(GetArrayItemPointer(start));

        auto* engine = _typeInfo->GetEngine();

        for (; d < max; d++) {
            if (*d != nullptr) {
                engine->ReleaseScriptObject(*d, _typeInfo->GetSubType());
            }
        }
    }

    const auto it_start = _buffer.begin() + numeric_cast<ptrdiff_t>(start * _elementSize);
    const auto it_end = _buffer.begin() + numeric_cast<ptrdiff_t>(end * _elementSize);
    _buffer.erase(it_start, it_end);
}

auto ScriptArray::Equals(const void* a, const void* b, AngelScript::asIScriptContext* ctx) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_subTypeData) {
        switch (_subTypeId) {
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
            return COMPARE(int32); // All enums fall here
#undef COMPARE
        }
    }
    else {
        int32 r = 0;

        if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
            if (*static_cast<void* const*>(a) == *static_cast<void* const*>(b)) {
                return true;
            }
        }

        if (_subTypeData->EqFunc != nullptr) {
            r = ctx->Prepare(_subTypeData->EqFunc.get_no_const());
            FO_RUNTIME_ASSERT(r >= 0);

            if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
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

        if (_subTypeData->CmpFunc != nullptr) {
            r = ctx->Prepare(_subTypeData->CmpFunc.get_no_const());
            FO_RUNTIME_ASSERT(r >= 0);

            if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
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
                return static_cast<int32>(ctx->GetReturnDWord()) == 0;
            }

            return false;
        }
    }

    return false;
}

auto ScriptArray::Less(const void* a, const void* b, bool asc, AngelScript::asIScriptContext* ctx) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!asc) {
        const void* temp = a;
        a = b;
        b = temp;
    }

    if (!_subTypeData) {
        switch (_subTypeId) {
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
        case AngelScript::asTYPEID_FLOAT:
            return COMPARE(float32);
        case AngelScript::asTYPEID_DOUBLE:
            return COMPARE(float64);
        default:
            return COMPARE(int32); // All enums fall in this case
#undef COMPARE
        }
    }
    else {
        int32 r = 0;

        if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
            if (*static_cast<void* const*>(a) == nullptr) {
                return true;
            }
            if (*static_cast<void* const*>(b) == nullptr) {
                return false;
            }
        }

        if (_subTypeData->CmpFunc != nullptr) {
            r = ctx->Prepare(_subTypeData->CmpFunc.get_no_const());
            FO_RUNTIME_ASSERT(r >= 0);

            if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
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
                return static_cast<int32>(ctx->GetReturnDWord()) < 0;
            }
        }
    }

    return false;
}

void ScriptArray::Reverse()
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32 size = GetSize();

    if (size >= 2) {
        AngelScript::asBYTE temp[16];

        for (int32 i = 0; i < size / 2; i++) {
            Copy(temp, GetArrayItemPointer(i));
            Copy(GetArrayItemPointer(i), GetArrayItemPointer(size - i - 1));
            Copy(GetArrayItemPointer(size - i - 1), temp);
        }
    }
}

auto ScriptArray::operator==(const ScriptArray& other) const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_typeInfo != other._typeInfo) {
        return false;
    }
    if (GetSize() != other.GetSize()) {
        return false;
    }

    if (_subTypeData) {
        if (_subTypeData->CmpFunc == nullptr && _subTypeData->EqFunc == nullptr) {
            const auto* sub_type = _typeInfo->GetEngine()->GetTypeInfoById(_subTypeId);

            if (_subTypeData->EqFuncReturnCode == AngelScript::asMULTIPLE_FUNCTIONS) {
                throw ScriptException("Type has multiple matching opEquals or opCmp methods", sub_type->GetName());
            }
            else {
                throw ScriptException("Type does not have a matching opEquals or opCmp method", sub_type->GetName());
            }
        }
    }

    AngelScript::asIScriptContext* ctx = nullptr;

    if (_subTypeData) {
        ctx = AngelScript::asGetActiveContext();
        FO_RUNTIME_ASSERT(ctx);
        FO_RUNTIME_ASSERT(ctx->GetEngine() == _typeInfo->GetEngine());

        const int32 r = ctx->PushState();
        FO_RUNTIME_ASSERT(r >= 0);
    }

    auto release_ctx = ScopeCallback([&]() noexcept {
        if (ctx != nullptr) {
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

    for (int32 i = 0; i < GetSize(); i++) {
        if (!Equals(At(i), other.At(i), ctx)) {
            is_equal = false;
            break;
        }
    }

    return is_equal;
}

auto ScriptArray::FindByRef(void* ref) const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return FindByRef(0, ref);
}

auto ScriptArray::FindByRef(int32 start_at, void* ref) const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto size = GetSize();

    if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        ref = *static_cast<void**>(ref);

        for (int32 i = start_at; i < size; i++) {
            if (*static_cast<void* const*>(At(i)) == ref) {
                return i;
            }
        }
    }
    else {
        for (int32 i = start_at; i < size; i++) {
            if (At(i) == ref) {
                return i;
            }
        }
    }

    return -1;
}

auto ScriptArray::Find(void* value) const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return Find(0, value);
}

auto ScriptArray::Find(int32 start_at, void* value) const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_subTypeData) {
        if (_subTypeData->CmpFunc == nullptr && _subTypeData->EqFunc == nullptr) {
            const auto* sub_type = _typeInfo->GetEngine()->GetTypeInfoById(_subTypeId);

            if (_subTypeData->EqFuncReturnCode == AngelScript::asMULTIPLE_FUNCTIONS) {
                throw ScriptException("Type has multiple matching opEquals or opCmp methods", sub_type->GetName());
            }
            else {
                throw ScriptException("Type does not have a matching opEquals or opCmp method", sub_type->GetName());
            }
        }
    }

    AngelScript::asIScriptContext* ctx = nullptr;

    if (_subTypeData) {
        ctx = AngelScript::asGetActiveContext();
        FO_RUNTIME_ASSERT(ctx);
        FO_RUNTIME_ASSERT(ctx->GetEngine() == _typeInfo->GetEngine());

        const int32 r = ctx->PushState();
        FO_RUNTIME_ASSERT(r >= 0);
    }

    auto release_ctx = ScopeCallback([&]() noexcept {
        if (ctx != nullptr) {
            safe_call([&] {
                const auto state = ctx->GetState();
                ctx->PopState();

                if (state == AngelScript::asEXECUTION_ABORTED) {
                    ctx->Abort();
                }
            });
        }
    });

    int32 ret = -1;
    const int32 size = GetSize();

    for (int32 i = start_at; i < size; i++) {
        if (Equals(At(i), value, ctx)) {
            ret = i;
            break;
        }
    }

    return ret;
}

void ScriptArray::Copy(void* dst, void* src) const
{
    FO_NO_STACK_TRACE_ENTRY();

    MemCopy(dst, src, numeric_cast<size_t>(_elementSize));
}

auto ScriptArray::GetArrayItemPointer(int32 index) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return static_cast<uint8*>(GetBuffer()) + numeric_cast<size_t>(index * _elementSize);
}

auto ScriptArray::GetArrayItemPointer(int32 index) const -> const void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return static_cast<const uint8*>(GetBuffer()) + numeric_cast<size_t>(index * _elementSize);
}

auto ScriptArray::GetDataPointer(void* buf) const -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0 && (_subTypeId & AngelScript::asTYPEID_OBJHANDLE) == 0) {
        return *static_cast<void**>(buf);
    }
    else {
        return buf;
    }
}

void ScriptArray::SortAsc()
{
    FO_NO_STACK_TRACE_ENTRY();

    Sort(0, GetSize(), true);
}

void ScriptArray::SortAsc(int32 start_at, int32 count)
{
    FO_NO_STACK_TRACE_ENTRY();

    Sort(start_at, count, true);
}

void ScriptArray::SortDesc()
{
    FO_NO_STACK_TRACE_ENTRY();

    Sort(0, GetSize(), false);
}

void ScriptArray::SortDesc(int32 start_at, int32 count)
{
    FO_NO_STACK_TRACE_ENTRY();

    Sort(start_at, count, false);
}

void ScriptArray::Sort(int32 start_at, int32 count, bool asc)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_subTypeData) {
        if (_subTypeData->CmpFunc == nullptr) {
            const auto* sub_type = _typeInfo->GetEngine()->GetTypeInfoById(_subTypeId);

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

    const int32 start = start_at;
    const int32 end = start_at + count;

    if (start < 0 || end < 0 || start >= GetSize() || end > GetSize()) {
        throw ScriptException("Index out of bounds");
    }

    AngelScript::asIScriptContext* ctx = nullptr;

    if (_subTypeData) {
        ctx = AngelScript::asGetActiveContext();
        FO_RUNTIME_ASSERT(ctx);
        FO_RUNTIME_ASSERT(ctx->GetEngine() == _typeInfo->GetEngine());

        const int32 r = ctx->PushState();
        FO_RUNTIME_ASSERT(r >= 0);
    }

    auto release_ctx = ScopeCallback([&]() noexcept {
        if (ctx != nullptr) {
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
    for (int32 i = start + 1; i < end; i++) {
        AngelScript::asBYTE tmp[16];
        Copy(tmp, GetArrayItemPointer(i));

        int32 j = i - 1;

        while (j >= start && Less(GetDataPointer(tmp), At(j), asc, ctx)) {
            Copy(GetArrayItemPointer(j + 1), GetArrayItemPointer(j));
            j--;
        }

        Copy(GetArrayItemPointer(j + 1), tmp);
    }
}

void ScriptArray::CopyBuffer(const ScriptArray& src)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* engine = _typeInfo->GetEngine();
    const auto count = std::min(GetSize(), src.GetSize());

    if ((_subTypeId & AngelScript::asTYPEID_OBJHANDLE) != 0) {
        if (count != 0) {
            auto** max = static_cast<void**>(GetArrayItemPointer(count));
            auto** d = static_cast<void**>(GetArrayItemPointer(0));
            auto** s = static_cast<void**>(const_cast<void*>(src.GetArrayItemPointer(0)));

            for (; d < max; d++, s++) {
                void* tmp = *d;
                *d = *s;

                if (*d != nullptr) {
                    engine->AddRefScriptObject(*d, _typeInfo->GetSubType());
                }

                if (tmp != nullptr) {
                    engine->ReleaseScriptObject(tmp, _typeInfo->GetSubType());
                }
            }
        }
    }
    else if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        if (count != 0) {
            auto** max = static_cast<void**>(GetArrayItemPointer(count));
            auto** d = static_cast<void**>(GetArrayItemPointer(0));
            auto** s = static_cast<void**>(const_cast<void*>(src.GetArrayItemPointer(0)));

            const auto* sub_type = _typeInfo->GetSubType();

            for (; d < max; d++, s++) {
                engine->AssignScriptObject(*d, *s, sub_type);
            }
        }
        else {
            MemCopy(GetBuffer(), src.GetBuffer(), numeric_cast<size_t>(count * _elementSize));
        }
    }
}

void ScriptArray::PrecacheSubTypeData()
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((_subTypeId & ~AngelScript::asTYPEID_MASK_SEQNBR) == 0) {
        return;
    }

    _subTypeData = static_cast<ScriptArrayTypeData*>(_typeInfo->GetUserData(AS_TYPE_ARRAY_CACHE));

    if (_subTypeData) {
        return;
    }

    AngelScript::asAcquireExclusiveLock();
    auto release_lock = ScopeCallback([]() noexcept { AngelScript::asReleaseExclusiveLock(); });

    _subTypeData = static_cast<ScriptArrayTypeData*>(_typeInfo->GetUserData(AS_TYPE_ARRAY_CACHE));

    if (_subTypeData) {
        return;
    }

    _subTypeData = SafeAlloc::MakeRaw<ScriptArrayTypeData>();

    const bool must_be_const = (_subTypeId & AngelScript::asTYPEID_HANDLETOCONST) != 0;
    const auto* sub_type = _typeInfo->GetEngine()->GetTypeInfoById(_subTypeId);

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
                    if (_subTypeData->CmpFunc != nullptr || _subTypeData->CmpFuncReturnCode != 0) {
                        _subTypeData->CmpFunc = nullptr;
                        _subTypeData->CmpFuncReturnCode = AngelScript::asMULTIPLE_FUNCTIONS;
                    }
                    else {
                        _subTypeData->CmpFunc = func;
                    }
                }
                else if (is_eq) {
                    if (_subTypeData->EqFunc != nullptr || _subTypeData->EqFuncReturnCode != 0) {
                        _subTypeData->EqFunc = nullptr;
                        _subTypeData->EqFuncReturnCode = AngelScript::asMULTIPLE_FUNCTIONS;
                    }
                    else {
                        _subTypeData->EqFunc = func;
                    }
                }
            }
        }
    }

    if (_subTypeData->EqFunc == nullptr && _subTypeData->EqFuncReturnCode == 0) {
        _subTypeData->EqFuncReturnCode = AngelScript::asNO_FUNCTION;
    }
    if (_subTypeData->CmpFunc == nullptr && _subTypeData->CmpFuncReturnCode == 0) {
        _subTypeData->CmpFuncReturnCode = AngelScript::asNO_FUNCTION;
    }

    _typeInfo->SetUserData(_subTypeData.get(), AS_TYPE_ARRAY_CACHE);
}

void ScriptArray::EnumReferences(AngelScript::asIScriptEngine* engine)
{
    FO_NO_STACK_TRACE_ENTRY();

    if ((_subTypeId & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
        auto** d = static_cast<void**>(GetBuffer());

        for (int32 i = 0; i < GetSize(); i++) {
            if (d[i] != nullptr) {
                engine->GCEnumCallback(d[i]);
            }
        }
    }
}

void ScriptArray::ReleaseAllHandles(AngelScript::asIScriptEngine* engine)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(engine);
    Resize(0);
}

void ScriptArray::AddRef() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag = false;
    AngelScript::asAtomicInc(_refCount);
}

void ScriptArray::Release() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag = false;

    if (AngelScript::asAtomicDec(_refCount) == 0) {
        delete this;
    }
}

auto ScriptArray::GetRefCount() const -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return _refCount;
}

void ScriptArray::SetFlag() const
{
    FO_NO_STACK_TRACE_ENTRY();

    _gcFlag = true;
}

auto ScriptArray::GetFlag() const -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _gcFlag;
}

static auto CScriptArray_InsertFirst(ScriptArray& arr, void* value) -> void
{
    FO_NO_STACK_TRACE_ENTRY();

    arr.InsertAt(0, value);
}

static auto CScriptArray_RemoveFirst(ScriptArray& arr) -> void
{
    FO_NO_STACK_TRACE_ENTRY();

    arr.RemoveAt(0);
}

static auto CScriptArray_Grow(ScriptArray& arr, int32 count) -> void
{
    FO_NO_STACK_TRACE_ENTRY();

    if (count <= 0) {
        return;
    }

    arr.Resize(arr.GetSize() + count);
}

static auto CScriptArray_Reduce(ScriptArray& arr, int32 count) -> void
{
    FO_NO_STACK_TRACE_ENTRY();

    if (count <= 0) {
        return;
    }

    const auto size = numeric_cast<int32>(arr.GetSize());

    if (count > size) {
        throw ScriptException("Array size is less than reduce count");
    }

    arr.Resize(size - count);
}

static auto CScriptArray_First(ScriptArray& arr) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return arr.At(0);
}

static auto CScriptArray_Last(ScriptArray& arr) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    return arr.At(arr.GetSize() - 1);
}

static void CScriptArray_Clear(ScriptArray& arr)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (arr.GetSize() > 0) {
        arr.Resize(0);
    }
}

static auto CScriptArray_Exists(const ScriptArray& arr, void* value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return arr.Find(0, value) != -1;
}

static auto CScriptArray_Remove(ScriptArray& arr, void* value) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    const int32 index = arr.Find(0, value);

    if (index != -1) {
        arr.RemoveAt(index);
        return true;
    }

    return false;
}

static auto CScriptArray_RemoveAll(ScriptArray& arr, void* value) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    int32 count = 0;
    int32 index = 0;

    while (index < numeric_cast<int32>(arr.GetSize())) {
        index = arr.Find(index, value);

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

static auto CScriptArray_Factory(AngelScript::asITypeInfo* ti, const ScriptArray* other) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Array is null");
    }

    ScriptArray* clone = ScriptArray::Create(ti);
    *clone = *other;
    return clone;
}

static auto CScriptArray_Clone(const ScriptArray& arr) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    ScriptArray* clone = ScriptArray::Create(const_cast<AngelScript::asITypeInfo*>(arr.GetArrayObjectType()));
    *clone = arr;
    return clone;
}

static void CScriptArray_Set(ScriptArray& arr, const ScriptArray* other)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Array is null");
    }

    arr = *other;
}

static void CScriptArray_InsertArrAt(ScriptArray& arr, int32 index, const ScriptArray* other)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0) {
        return;
    }

    if (other == nullptr) {
        throw ScriptException("Array is null");
    }

    arr.InsertAt(index, *other);
}

static void CScriptArray_InsertArrFirst(ScriptArray& arr, const ScriptArray* other)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Array is null");
    }

    arr.InsertAt(0, *other);
}

static void CScriptArray_InsertArrLast(ScriptArray& arr, const ScriptArray* other)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Array is null");
    }

    arr.InsertAt(arr.GetSize(), *other);
}

static auto CScriptArray_Equals(ScriptArray& arr, const ScriptArray* other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (other == nullptr) {
        throw ScriptException("Array is null");
    }

    return arr == *other;
}

void RegisterAngelScriptArray(AngelScript::asIScriptEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    engine->SetTypeInfoUserDataCleanupCallback(CleanupTypeInfoArrayCache, AS_TYPE_ARRAY_CACHE);

    int32 r = engine->RegisterObjectType("array<class T>", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_GC | AngelScript::asOBJ_TEMPLATE);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)", SCRIPT_FUNC(ScriptArrayTemplateCallback), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_FACTORY, "array<T>@ f(int&in)", SCRIPT_FUNC_EXT(ScriptArray::Create, (AngelScript::asITypeInfo*), ScriptArray*), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_FACTORY, "array<T>@ f(int&in, int length)", SCRIPT_FUNC_EXT(ScriptArray::Create, (AngelScript::asITypeInfo*, int32), ScriptArray*), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_FACTORY, "array<T>@ f(int&in, int length, const T &in value)", SCRIPT_FUNC_EXT(ScriptArray::Create, (AngelScript::asITypeInfo*, int32, void*), ScriptArray*), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_FACTORY, "array<T>@ f(int& in, const array<T>@+)", SCRIPT_FUNC(CScriptArray_Factory), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_LIST_FACTORY, "array<T>@ f(int&in type, int&in list) {repeat T}", SCRIPT_FUNC_EXT(ScriptArray::Create, (AngelScript::asITypeInfo*, void*), ScriptArray*), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD(ScriptArray, AddRef), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD(ScriptArray, Release), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("array<T>", "T &opIndex(int index)", SCRIPT_METHOD_EXT(ScriptArray, At, (int32), void*), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "const T &opIndex(int index) const", SCRIPT_METHOD_EXT(ScriptArray, At, (int32) const, const void*), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("array<T>", "void insertAt(int index, const T&in value)", SCRIPT_METHOD_EXT(ScriptArray, InsertAt, (int32, void*), void), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertLast(const T&in value)", SCRIPT_METHOD(ScriptArray, InsertLast), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void removeAt(int index)", SCRIPT_METHOD(ScriptArray, RemoveAt), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void removeLast()", SCRIPT_METHOD(ScriptArray, RemoveLast), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void removeRange(int start, int count)", SCRIPT_METHOD(ScriptArray, RemoveRange), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "int length() const", SCRIPT_METHOD(ScriptArray, GetSize), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void reserve(int length)", SCRIPT_METHOD(ScriptArray, Reserve), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void resize(int length)", SCRIPT_METHOD_EXT(ScriptArray, Resize, (int32), void), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void sortAsc()", SCRIPT_METHOD_EXT(ScriptArray, SortAsc, (), void), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void sortAsc(int startAt, int count)", SCRIPT_METHOD_EXT(ScriptArray, SortAsc, (int32, int32), void), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void sortDesc()", SCRIPT_METHOD_EXT(ScriptArray, SortDesc, (), void), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void sortDesc(int startAt, int count)", SCRIPT_METHOD_EXT(ScriptArray, SortDesc, (int32, int32), void), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void reverse()", SCRIPT_METHOD(ScriptArray, Reverse), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "int find(const T&in if_handle_then_const value) const", SCRIPT_METHOD_EXT(ScriptArray, Find, (void*) const, int32), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "int find(int startAt, const T&in if_handle_then_const value) const", SCRIPT_METHOD_EXT(ScriptArray, Find, (int32, void*) const, int32), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "int findByRef(const T&in if_handle_then_const value) const", SCRIPT_METHOD_EXT(ScriptArray, FindByRef, (void*) const, int32), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "int findByRef(int startAt, const T&in if_handle_then_const value) const", SCRIPT_METHOD_EXT(ScriptArray, FindByRef, (int32, void*) const, int32), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool isEmpty() const", SCRIPT_METHOD(ScriptArray, IsEmpty), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("array<T>", "int get_length() const", SCRIPT_METHOD(ScriptArray, GetSize), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void set_length(int)", SCRIPT_METHOD_EXT(ScriptArray, Resize, (int32), void), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_GETREFCOUNT, "int f()", SCRIPT_METHOD(ScriptArray, GetRefCount), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_SETGCFLAG, "void f()", SCRIPT_METHOD(ScriptArray, SetFlag), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_GETGCFLAG, "bool f()", SCRIPT_METHOD(ScriptArray, GetFlag), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_ENUMREFS, "void f(int&in)", SCRIPT_METHOD(ScriptArray, EnumReferences), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", AngelScript::asBEHAVE_RELEASEREFS, "void f(int&in)", SCRIPT_METHOD(ScriptArray, ReleaseAllHandles), SCRIPT_METHOD_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("array<T>", "void insertFirst(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_InsertFirst), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void removeFirst()", SCRIPT_FUNC_THIS(CScriptArray_RemoveFirst), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void grow(int)", SCRIPT_FUNC_THIS(CScriptArray_Grow), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void reduce(int)", SCRIPT_FUNC_THIS(CScriptArray_Reduce), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& first()", SCRIPT_FUNC_THIS(CScriptArray_First), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "const T& first() const", SCRIPT_FUNC_THIS(CScriptArray_First), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& last()", SCRIPT_FUNC_THIS(CScriptArray_Last), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "const T& last() const", SCRIPT_FUNC_THIS(CScriptArray_Last), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void clear()", SCRIPT_FUNC_THIS(CScriptArray_Clear), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool exists(const T&in) const", SCRIPT_FUNC_THIS(CScriptArray_Exists), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool remove(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_Remove), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "int removeAll(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_RemoveAll), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "array<T>@ clone() const", SCRIPT_FUNC_THIS(CScriptArray_Clone), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void set(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_Set), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertAt(int, const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrAt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertFirst(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrFirst), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertLast(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrLast), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool equals(const array<T>@+) const", SCRIPT_FUNC_THIS(CScriptArray_Equals), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterDefaultArrayType("array<T>");
    FO_RUNTIME_ASSERT(r >= 0);
}

FO_END_NAMESPACE();
