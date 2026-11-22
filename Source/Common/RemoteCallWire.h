// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

// Shared remote-call wire serialization

#include "Common.h"

#include "DataSerialization.h"
#include "HashedString.h"

FO_BEGIN_NAMESPACE

// Backend hooks for ref-type values: the wire layout is fixed here, but reading/creating the backend's script
// object is backend-specific
struct RemoteCallWireHooks
{
    // Write: produce the raw bytes for the ref-type value at `arg` (the FuncCallData argument pointer).
    function<vector<uint8_t>(const BaseTypeDesc&, ptr<void>)> RefTypeToRaw {};
    // Read: create a backend ref-type value from raw bytes and return the FuncCallData argument pointer. The
    // backend owns the created object's lifetime (e.g. via storage captured in the callback).
    function<ptr<void>(const BaseTypeDesc&, span<const uint8_t>)> RawToRefType {};
};

// Owns the deserialized scalar storage of one inbound call: ReadRemoteCallSimple returns pointers into this
// storage, never into the transient reader buffer
class RemoteCallReadStorage final
{
public:
    ~RemoteCallReadStorage();

    auto StoreString(string&& value) -> ptr<void> { return make_ptr(&std::get<string>(_items.emplace_back(std::move(value)))).reinterpret_as<void>(); }
    auto StoreHashed(hstring value) -> ptr<void> { return make_ptr(&std::get<hstring>(_items.emplace_back(value))).reinterpret_as<void>(); }
    auto StoreStructBytes(size_t size) -> ptr<uint8_t>;
    void StoreStructHash(ptr<uint8_t> address, hstring value);
    auto StorePlainBytes() -> ptr<uint8_t> { return std::get<PlainData>(_items.emplace_back(PlainData {})).Bytes; }

private:
    struct PlainData
    {
        alignas(uint64_t) uint8_t Bytes[sizeof(uint64_t)] {};
    };

    list<variant<string, hstring, unique_del_ptr<void>, PlainData>> _items {};
    vector<ptr<hstring>> _structHashes {};
};

// Serialize one simple (non-collection) remote-call value to the wire. Mirrors the format the AngelScript
// backend has always written; keep byte-compatible
void WriteRemoteCallSimple(data_writer& writer, ptr<void> value, const BaseTypeDesc& type, const RemoteCallWireHooks& hooks);

// Deserialize one simple (non-collection) remote-call value from the wire; returns the FuncCallData argument
// pointer (into `storage` for scalars, or — for ref types — a backend object via the hook).
auto ReadRemoteCallSimple(data_reader& reader, const BaseTypeDesc& type, const hash_resolver& hashes, RemoteCallReadStorage& storage, const RemoteCallWireHooks& hooks) -> ptr<void>;

FO_END_NAMESPACE
