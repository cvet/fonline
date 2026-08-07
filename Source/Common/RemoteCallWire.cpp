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

#include "RemoteCallWire.h"

FO_BEGIN_NAMESPACE

void WriteRemoteCallSimple(DataWriter& writer, ptr<void> value, const BaseTypeDesc& type, const RemoteCallWireHooks& hooks)
{
    FO_STACK_TRACE_ENTRY();

    if (type.IsPrimitive) {
        VisitBaseTypePrimitive(value.get(), type, [&](auto&& v) {
            using t = std::decay_t<decltype(v)>;
            writer.Write<t>(v);
        });
    }
    else if (type.IsEnum) {
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Enum type has no underlying type to serialize");
        FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type must be integral to serialize");
        writer.WritePtr(value.reinterpret_as<const uint8_t>().get(), type.Size);
    }
    else if (type.IsString) {
        const auto& str = *value.reinterpret_as<const string>();
        writer.Write<int32_t>(numeric_cast<int32_t>(str.length()));
        writer.WritePtr(str.c_str(), str.length());
    }
    else if (type.IsHashedString) {
        const auto& hstr = *value.reinterpret_as<const hstring>();
        writer.Write<hstring::hash_t>(hstr.as_hash());
    }
    else if (type.IsRefType) {
        const auto raw_data = hooks.RefTypeToRaw(type, value);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(raw_data.size()));

        if (!raw_data.empty()) {
            writer.WritePtr(raw_data.data(), raw_data.size());
        }
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            WriteRemoteCallSimple(writer, value.reinterpret_as<uint8_t>().offset(field.Offset), field.Type, hooks);
        }
    }
    else {
        throw NotSupportedException(FO_LINE_STR);
    }
}

auto ReadRemoteCallSimple(DataReader& reader, const BaseTypeDesc& type, const HashResolver& hashes, RemoteCallReadStorage& storage, const RemoteCallWireHooks& hooks) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    // Copy primitive/enum bytes out of the transient reader buffer into aligned per-call storage; the call site
    // dereferences the returned pointer as the primitive's type, which requires natural alignment.
    const auto read_plain = [&](size_t size) -> ptr<void> {
        FO_VERIFY_AND_THROW(size <= sizeof(uint64_t), "Remote call plain argument is too large", size, sizeof(uint64_t));
        ptr<uint8_t> buf = storage.StorePlainBytes();
        reader.ReadPtr(buf.get(), size);
        return ptr<void> {buf};
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
        const nptr<const char> s = reader.ReadPtr<char>(str_len);
        return storage.StoreString(string(s.get(), str_len));
    }
    else if (type.IsHashedString) {
        const auto hash = reader.Read<hstring::hash_t>();
        return storage.StoreHashed(hashes.ResolveHash(hash));
    }
    else if (type.IsRefType) {
        const uint32_t raw_size = reader.Read<uint32_t>();
        const nptr<const uint8_t> raw_data = reader.ReadPtr<uint8_t>(raw_size);
        return hooks.RawToRefType(type, {raw_data.get(), raw_size});
    }
    else if (type.IsStruct) {
        ptr<uint8_t> buf = storage.StoreStructBytes(type.Size);

        for (const auto& field : type.StructLayout->Fields) {
            const ptr<void> field_data = ReadRemoteCallSimple(reader, field.Type, hashes, storage, hooks);
            MemCopy(buf.offset(field.Offset), field_data, field.Type.Size);
        }

        return ptr<void> {buf};
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

FO_END_NAMESPACE
