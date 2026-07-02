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

// Shared remote-call wire (de)serializer (Phase A2). Both scripting backends serialize remote-call arguments
// to the same byte format; this header owns the per-value scalar I/O so the format has one source of truth (no
// drift between AngelScript and the C# Managed backend). The collection framing (`int32 count` + elements) is a
// shared convention each backend's loop applies, calling WriteRemoteCallSimple/ReadRemoteCallSimple per element.
// Ref-type value <-> raw-bytes conversion is backend-specific (AngelScript script object vs managed object), so
// it is supplied via the RemoteCallWireHooks callbacks; the wire framing (`uint32 size + bytes`) stays here.

#include "Common.h"

#include "DataSerialization.h"
#include "HashedString.h"

FO_BEGIN_NAMESPACE

// Backend hooks for ref-type values: the wire layout is fixed here, but reading/creating the backend's script
// object is backend-specific.
struct RemoteCallWireHooks
{
    // Write: produce the raw bytes for the ref-type value at `arg_ptr` (the FuncCallData argument pointer).
    function<vector<uint8_t>(const BaseTypeDesc&, const void*)> RefTypeToRaw {};
    // Read: create a backend ref-type value from raw bytes and return the FuncCallData argument pointer. The
    // backend owns the created object's lifetime (e.g. via storage captured in the callback).
    function<void*(const BaseTypeDesc&, span<const uint8_t>)> RawToRefType {};
};

// Keeps deserialized scalar values (primitive / enum bytes, string, hstring, struct bytes) alive for the duration
// of one inbound call; ReadRemoteCallSimple returns pointers into this storage, never into the transient reader
// buffer — primitives and enums are copied into aligned storage so the call site reads them at natural alignment.
// Ref-type and collection lifetimes are owned by the caller (backend-specific containers).
class RemoteCallReadStorage final
{
public:
    auto StoreString(string&& value) -> void* { return cast_to_void(&std::get<string>(_items.emplace_back(std::move(value)))); }
    auto StoreHashed(hstring value) -> void* { return cast_to_void(&std::get<hstring>(_items.emplace_back(value))); }
    auto StoreStructBytes(size_t size) -> uint8_t* { return std::get<vector<uint8_t>>(_items.emplace_back(vector<uint8_t>(size, 0))).data(); }
    auto StorePlainBytes() -> uint8_t* { return std::get<PlainData>(_items.emplace_back(PlainData {})).Bytes; }

private:
    struct PlainData
    {
        alignas(uint64_t) uint8_t Bytes[sizeof(uint64_t)] {};
    };

    list<variant<string, hstring, vector<uint8_t>, PlainData>> _items {};
};

// Serialize one simple (non-collection) remote-call value to the wire. Mirrors the format the AngelScript
// backend has always written; keep byte-compatible.
inline void WriteRemoteCallSimple(DataWriter& writer, const void* ptr, const BaseTypeDesc& type, const RemoteCallWireHooks& hooks)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsPrimitive) {
        VisitBaseTypePrimitive(ptr, type, [&](auto&& v) {
            using t = std::decay_t<decltype(v)>;
            writer.Write<t>(v);
        });
    }
    else if (type.IsEnum) {
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Enum type has no underlying type to serialize");
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type must be integral to serialize");
        writer.WritePtr(static_cast<const uint8_t*>(ptr), type.Size);
    }
    else if (type.IsString) {
        const auto& str = *cast_from_void<const string*>(ptr);
        writer.Write<int32_t>(numeric_cast<int32_t>(str.length()));
        writer.WritePtr(str.c_str(), str.length());
    }
    else if (type.IsHashedString) {
        const auto& hstr = *cast_from_void<const hstring*>(ptr);
        writer.Write<hstring::hash_t>(hstr.as_hash());
    }
    else if (type.IsRefType) {
        const auto raw_data = hooks.RefTypeToRaw(type, ptr);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(raw_data.size()));

        if (!raw_data.empty()) {
            writer.WritePtr(raw_data.data(), raw_data.size());
        }
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            WriteRemoteCallSimple(writer, static_cast<const uint8_t*>(ptr) + field.Offset, field.Type, hooks);
        }
    }
    else {
        throw NotSupportedException(FO_LINE_STR);
    }
}

// Deserialize one simple (non-collection) remote-call value from the wire; returns the FuncCallData argument
// pointer (into `storage` for scalars, or — for ref types — a backend object via the hook).
inline auto ReadRemoteCallSimple(MutableDataReader& reader, const BaseTypeDesc& type, const HashResolver& hashes, RemoteCallReadStorage& storage, const RemoteCallWireHooks& hooks) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    // Copy primitive/enum bytes out of the transient reader buffer into aligned per-call storage; the call site
    // dereferences the returned pointer as the primitive's type, which requires natural alignment.
    const auto read_plain = [&](size_t size) -> void* {
        FO_VERIFY_AND_THROW(size <= sizeof(uint64_t), "Remote call plain argument is too large", size, sizeof(uint64_t));
        uint8_t* buf = storage.StorePlainBytes();
        reader.ReadPtr(buf, size);
        return cast_to_void(buf);
    };

    if (type.IsPrimitive) {
        return read_plain(type.Size);
    }
    else if (type.IsEnum) {
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Enum type has no underlying type to deserialize");
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type must be integral to deserialize");
        return read_plain(type.Size);
    }
    else if (type.IsString) {
        const auto str_len = reader.Read<int32_t>();
        FO_VERIFY_AND_THROW(str_len >= 0, "Wire string length must be non-negative");
        const auto* s = reader.ReadPtr<char>(str_len);
        return storage.StoreString(string(s, str_len));
    }
    else if (type.IsHashedString) {
        const auto hash = reader.Read<hstring::hash_t>();
        return storage.StoreHashed(hashes.ResolveHash(hash));
    }
    else if (type.IsRefType) {
        const uint32_t raw_size = reader.Read<uint32_t>();
        const auto* raw_data = reader.ReadPtr<uint8_t>(raw_size);
        return hooks.RawToRefType(type, {raw_data, raw_size});
    }
    else if (type.IsStruct) {
        uint8_t* buf = storage.StoreStructBytes(type.Size);

        for (const auto& field : type.StructLayout->Fields) {
            void* field_data = ReadRemoteCallSimple(reader, field.Type, hashes, storage, hooks);
            MemCopy(buf + field.Offset, field_data, field.Type.Size);
        }

        return cast_to_void(buf);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

FO_END_NAMESPACE
