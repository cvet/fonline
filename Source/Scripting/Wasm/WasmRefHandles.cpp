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

#include "WasmRefHandles.h"

#if FO_WASM_SCRIPTING

#include "Properties.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

static auto FindWasmExportRefLifecycleMethod(const BaseTypeDesc& type, string_view name) noexcept -> const MethodDesc*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type.IsRefType || type.RefType == nullptr) {
        return nullptr;
    }

    for (const MethodDesc& method : type.RefType->Methods) {
        if (method.Name == name && method.Call != nullptr) {
            return &method;
        }
    }

    return nullptr;
}

static void CallWasmExportRefLifecycle(const BaseTypeDesc& type, void* handle, bool add_ref)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.IsRefType, "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(type.RefType != nullptr, "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(handle != nullptr, "WASM ref-handle invariant failed");

    if (type.RefType->FieldsRegistrator != nullptr) {
        ptr<DynamicRefTypeInstance> instance = static_cast<DynamicRefTypeInstance*>(handle);

        if (instance->GetRegistrator() != type.RefType->FieldsRegistrator) {
            throw ScriptCallException("WASM export mutable dynamic ref handle type mismatch", type.Name);
        }

        if (add_ref) {
            instance->AddRef();
        }
        else {
            instance->Release();
        }
        return;
    }

    const MethodDesc* method = FindWasmExportRefLifecycleMethod(type, add_ref ? "__AddRef" : "__Release");

    if (method == nullptr) {
        throw ScriptCallException("WASM export mutable ref type has no lifecycle method", type.Name, add_ref ? "__AddRef" : "__Release");
    }

    void* receiver = handle;
    array<ptr<void>, 1> args_data = {make_nptr(&receiver).void_cast()};
    FuncCallData call {
        .Accessor = &NativeDataProvider::NATIVE_DATA_ACCESSOR,
        .ArgsData = args_data,
    };
    method->Call(call);
}

static auto ReadWasmExportRefU32(span<const uint8_t> raw_data, size_t& offset, string_view role) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    if (offset + sizeof(uint32_t) > raw_data.size()) {
        throw ScriptCallException(strex("WASM export mutable ref {} header is out of bounds", role), offset, raw_data.size());
    }

    uint32_t value = 0;
    MemCopy(&value, raw_data.data() + offset, sizeof(value));
    offset += sizeof(value);
    return value;
}

static auto GetWasmExportRefElementWireSize(const BaseTypeDesc& type) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsEntity || type.IsEntityProto || type.IsFixedType || type.IsRefType || type.IsHashedString) {
        return sizeof(uint64_t);
    }
    if (type.Size == 0) {
        throw ScriptCallException("WASM export mutable ref collection element has no fixed size", type.Name);
    }

    return type.Size;
}

static auto ReadWasmExportRefElement(span<const uint8_t> raw_data, size_t& offset, const BaseTypeDesc& type, string_view role) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsString) {
        uint32_t text_size = ReadWasmExportRefU32(raw_data, offset, role);

        if (offset + text_size > raw_data.size()) {
            throw ScriptCallException(strex("WASM export mutable ref {} text is out of bounds", role), type.Name, text_size, offset, raw_data.size());
        }

        offset += text_size;
        return 0;
    }

    size_t element_size = GetWasmExportRefElementWireSize(type);

    if (offset + element_size > raw_data.size()) {
        throw ScriptCallException(strex("WASM export mutable ref {} element is out of bounds", role), type.Name, element_size, offset, raw_data.size());
    }

    uint64_t raw_value = 0;

    if (type.IsRefType) {
        FO_STRONG_ASSERT(element_size == sizeof(raw_value), "WASM ref-handle invariant failed");
        MemCopy(&raw_value, raw_data.data() + offset, sizeof(raw_value));
    }

    offset += element_size;
    return raw_value;
}

WasmExportRefHandleScope::~WasmExportRefHandleScope() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    ReleaseRetainedNoexcept();
}

auto WasmExportRefHandleScope::HasLifecycle(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type.IsRefType || type.RefType == nullptr) {
        return false;
    }
    if (type.RefType->FieldsRegistrator != nullptr) {
        return true;
    }

    return FindWasmExportRefLifecycleMethod(type, "__AddRef") != nullptr && FindWasmExportRefLifecycleMethod(type, "__Release") != nullptr;
}

auto WasmExportRefHandleScope::ContainsRefHandles(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind == ComplexTypeKind::Simple || type.Kind == ComplexTypeKind::Array) {
        return type.BaseType.IsRefType;
    }
    if (type.Kind == ComplexTypeKind::Dict || type.Kind == ComplexTypeKind::DictOfArray) {
        return type.BaseType.IsRefType || (type.KeyType.has_value() && type.KeyType->IsRefType);
    }

    return false;
}

void WasmExportRefHandleScope::TrackBorrowed(const BaseTypeDesc& type, const void* handle)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.IsRefType, "WASM ref-handle invariant failed");

    if (handle == nullptr) {
        return;
    }

    _borrowedHandles[type.Name].emplace(reinterpret_cast<uintptr_t>(handle));
}

void WasmExportRefHandleScope::ValidateAndRetainMutableValue(const ComplexTypeDesc& type, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Simple, "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(type.IsMutable, "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(type.BaseType.IsRefType, "WASM ref-handle invariant failed");

    if (raw_data.size() != sizeof(uint64_t)) {
        throw ScriptCallException("WASM export mutable ref value has invalid byte length", type.BaseType.Name, raw_data.size(), sizeof(uint64_t));
    }

    uint64_t raw_handle = 0;
    MemCopy(&raw_handle, raw_data.data(), sizeof(raw_handle));
    ValidateAndRetain(type.BaseType, raw_handle);
}

void WasmExportRefHandleScope::ValidateAndRetainMutableArray(const ComplexTypeDesc& type, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Array, "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(type.IsMutable, "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(type.BaseType.IsRefType, "WASM ref-handle invariant failed");

    if (raw_data.size() % sizeof(uint64_t) != 0) {
        throw ScriptCallException("WASM export mutable ref array byte length is not aligned", type.BaseType.Name, raw_data.size(), sizeof(uint64_t));
    }

    for (size_t offset = 0; offset < raw_data.size(); offset += sizeof(uint64_t)) {
        uint64_t raw_handle = 0;
        MemCopy(&raw_handle, raw_data.data() + offset, sizeof(raw_handle));
        ValidateAndRetain(type.BaseType, raw_handle);
    }
}

void WasmExportRefHandleScope::ValidateAndRetainMutableDict(const ComplexTypeDesc& type, span<const uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.Kind == ComplexTypeKind::Dict || type.Kind == ComplexTypeKind::DictOfArray, "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(type.IsMutable, "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(type.KeyType.has_value(), "WASM ref-handle invariant failed");
    FO_STRONG_ASSERT(ContainsRefHandles(type), "WASM ref-handle invariant failed");

    if (raw_data.empty()) {
        return;
    }

    size_t offset = 0;
    uint32_t dict_size = ReadWasmExportRefU32(raw_data, offset, "dictionary");

    for (uint32_t index = 0; index < dict_size; index++) {
        uint64_t raw_key = ReadWasmExportRefElement(raw_data, offset, *type.KeyType, "dictionary key");

        if (type.KeyType->IsRefType) {
            ValidateAndRetain(*type.KeyType, raw_key);
        }

        if (type.Kind == ComplexTypeKind::DictOfArray) {
            uint32_t array_size = ReadWasmExportRefU32(raw_data, offset, "dictionary value array");

            for (uint32_t value_index = 0; value_index < array_size; value_index++) {
                uint64_t raw_value = ReadWasmExportRefElement(raw_data, offset, type.BaseType, "dictionary value array");

                if (type.BaseType.IsRefType) {
                    ValidateAndRetain(type.BaseType, raw_value);
                }
            }
        }
        else {
            uint64_t raw_value = ReadWasmExportRefElement(raw_data, offset, type.BaseType, "dictionary value");

            if (type.BaseType.IsRefType) {
                ValidateAndRetain(type.BaseType, raw_value);
            }
        }
    }

    if (offset != raw_data.size()) {
        throw ScriptCallException("WASM export mutable ref dictionary has trailing bytes", type.BaseType.Name, offset, raw_data.size());
    }
}

void WasmExportRefHandleScope::ReleaseRetained()
{
    FO_STACK_TRACE_ENTRY();

    while (!_retainedHandles.empty()) {
        RetainedHandle retained = _retainedHandles.back();
        FO_STRONG_ASSERT(retained.Type != nullptr, "WASM ref-handle invariant failed");
        FO_STRONG_ASSERT(retained.Handle != nullptr, "WASM ref-handle invariant failed");
        CallWasmExportRefLifecycle(*retained.Type, retained.Handle, false);
        _retainedHandles.pop_back();
    }
}

void WasmExportRefHandleScope::ValidateAndRetain(const BaseTypeDesc& type, uint64_t raw_handle)
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(type.IsRefType, "WASM ref-handle invariant failed");

    if (raw_handle == 0) {
        return;
    }

    auto borrowed_it = _borrowedHandles.find(type.Name);
    uintptr_t handle_value = numeric_cast<uintptr_t>(raw_handle);

    if (borrowed_it == _borrowedHandles.end() || borrowed_it->second.count(handle_value) == 0) {
        throw ScriptCallException("WASM export mutable ref handle was not borrowed into the current call", type.Name, raw_handle);
    }
    if (!HasLifecycle(type)) {
        throw ScriptCallException("WASM export mutable ref type has no retain/release lifecycle", type.Name);
    }

    void* handle = reinterpret_cast<void*>(handle_value);
    _retainedHandles.emplace_back(RetainedHandle {.Type = &type, .Handle = handle});

    try {
        CallWasmExportRefLifecycle(type, handle, true);
    }
    catch (...) {
        _retainedHandles.pop_back();
        throw;
    }
}

void WasmExportRefHandleScope::ReleaseRetainedNoexcept() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    while (!_retainedHandles.empty()) {
        RetainedHandle retained = _retainedHandles.back();
        _retainedHandles.pop_back();

        try {
            CallWasmExportRefLifecycle(*retained.Type, retained.Handle, false);
        }
        catch (...) {
        }
    }
}

FO_END_NAMESPACE

#endif
