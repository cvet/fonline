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
    // Write: produce the raw bytes for the ref-type value at `arg` (the FuncCallData argument pointer).
    function<vector<uint8_t>(const BaseTypeDesc&, ptr<void>)> RefTypeToRaw {};
    // Read: create a backend ref-type value from raw bytes and return the FuncCallData argument pointer. The
    // backend owns the created object's lifetime (e.g. via storage captured in the callback).
    function<ptr<void>(const BaseTypeDesc&, span<const uint8_t>)> RawToRefType {};
};

// Keeps deserialized scalar values (primitive / enum bytes, string, hstring, struct bytes) alive for the duration
// of one inbound call; ReadRemoteCallSimple returns pointers into this storage, never into the transient reader
// buffer — primitives and enums are copied into aligned storage so the call site reads them at natural alignment.
// Ref-type and collection lifetimes are owned by the caller (backend-specific containers).
class RemoteCallReadStorage final
{
public:
    auto StoreString(string&& value) -> ptr<void> { return cast_to_void(&std::get<string>(_items.emplace_back(std::move(value)))); }
    auto StoreHashed(hstring value) -> ptr<void> { return cast_to_void(&std::get<hstring>(_items.emplace_back(value))); }
    auto StoreStructBytes(size_t size) -> ptr<uint8_t> { return std::get<vector<uint8_t>>(_items.emplace_back(vector<uint8_t>(size, 0))).data(); }
    auto StorePlainBytes() -> ptr<uint8_t> { return std::get<PlainData>(_items.emplace_back(PlainData {})).Bytes; }

private:
    struct PlainData
    {
        alignas(uint64_t) uint8_t Bytes[sizeof(uint64_t)] {};
    };

    list<variant<string, hstring, vector<uint8_t>, PlainData>> _items {};
};

// Serialize one simple (non-collection) remote-call value to the wire. Mirrors the format the AngelScript
// backend has always written; keep byte-compatible.
void WriteRemoteCallSimple(DataWriter& writer, ptr<void> value, const BaseTypeDesc& type, const RemoteCallWireHooks& hooks);

// Deserialize one simple (non-collection) remote-call value from the wire; returns the FuncCallData argument
// pointer (into `storage` for scalars, or — for ref types — a backend object via the hook).
auto ReadRemoteCallSimple(MutableDataReader& reader, const BaseTypeDesc& type, const HashResolver& hashes, RemoteCallReadStorage& storage, const RemoteCallWireHooks& hooks) -> ptr<void>;

FO_END_NAMESPACE
