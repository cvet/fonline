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

#include "ModelAnimationData.h"

#if FO_ENABLE_3D

#include "ozz/base/memory/allocator.h"

FO_BEGIN_NAMESPACE

static constexpr uint64_t MODEL_ANIMATION_ARCHIVE_HASH_OFFSET = 14695981039346656037ULL;
static constexpr uint64_t MODEL_ANIMATION_ARCHIVE_HASH_PRIME = 1099511628211ULL;

struct ModelAnimationAllocationHeader
{
    uintptr_t AllocationAddress {};
    size_t AllocationSize {};
};

class ModelAnimationAllocator final : public ozz::memory::Allocator
{
public:
    [[nodiscard]] auto Allocate(size_t size, size_t alignment) -> void* override
    {
        FO_NO_STACK_TRACE_ENTRY();

        constexpr size_t max_size = std::numeric_limits<size_t>::max();

        if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
            ReportBadAlloc("Model animation allocator received invalid alignment", "byte", 1, size);
            ReportAndExit("Model animation allocation alignment is invalid");
        }
        if (alignment - 1 > max_size - sizeof(ModelAnimationAllocationHeader) || size > max_size - sizeof(ModelAnimationAllocationHeader) - (alignment - 1)) {
            ReportBadAlloc("Model animation allocator size overflow", "byte", 1, size);
            ReportAndExit("Model animation allocation size overflow");
        }

        const size_t allocation_size = size + sizeof(ModelAnimationAllocationHeader) + alignment - 1;
        constexpr SafeAllocator<uint8_t> allocator;
        const ptr<uint8_t> allocation {allocator.allocate(allocation_size)};
        const uintptr_t aligned_address = align_up(allocation.as_uintptr() + sizeof(ModelAnimationAllocationHeader), static_cast<uintptr_t>(alignment));
        ptr<ModelAnimationAllocationHeader> header {reinterpret_cast<ModelAnimationAllocationHeader*>(aligned_address - sizeof(ModelAnimationAllocationHeader))};
        header->AllocationAddress = allocation.as_uintptr();
        header->AllocationSize = allocation_size;
        return reinterpret_cast<void*>(aligned_address);
    }

    void Deallocate(void* block) override
    {
        FO_NO_STACK_TRACE_ENTRY();

        if (block != nullptr) {
            const ptr<ModelAnimationAllocationHeader> header {reinterpret_cast<ModelAnimationAllocationHeader*>(reinterpret_cast<uintptr_t>(block) - sizeof(ModelAnimationAllocationHeader))};
            constexpr SafeAllocator<uint8_t> allocator;
            allocator.deallocate(reinterpret_cast<uint8_t*>(header->AllocationAddress), header->AllocationSize);
        }
    }
};

static auto IsModelAnimationArchiveKindValid(ModelAnimationArchiveKind kind) noexcept -> bool;
static auto GetModelAnimationArchiveContext(const ModelAnimationArchiveMetadata& metadata) -> string;
static void ValidateModelAnimationArchiveMetadata(const ModelAnimationArchiveMetadata& metadata);
static auto GetModelAnimationArchiveWireSize(const ModelAnimationArchiveMetadata& metadata, size_t payload_size) -> size_t;
template<typename T>
static void AppendModelAnimationArchiveLittleEndian(vector<uint8_t>& data, T value);
static void AppendModelAnimationArchiveBytes(vector<uint8_t>& data, const_span<uint8_t> bytes);
static void AppendModelAnimationArchiveString(vector<uint8_t>& data, string_view value, string_view field_name, const ModelAnimationArchiveMetadata& metadata);
static auto ReadModelAnimationArchiveBytes(const_span<uint8_t> data, size_t& read_pos, size_t size, string_view field_name, string_view context) -> const_span<uint8_t>;
template<typename T>
static auto ReadModelAnimationArchiveLittleEndian(const_span<uint8_t> data, size_t& read_pos, string_view field_name, string_view context) -> T;
static auto ReadModelAnimationArchiveString(const_span<uint8_t> data, size_t& read_pos, string_view field_name, string_view context) -> string;
static auto HashModelAnimationArchivePayload(const_span<uint8_t> payload) noexcept -> uint64_t;

void InitializeModelAnimationMemory() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    static ModelAnimationAllocator allocator;
    static const ozz::memory::Allocator* previous_allocator = ozz::memory::SetDefaulAllocator(&allocator);
    ignore_unused(previous_allocator);
}

auto WriteModelAnimationArchive(const ModelAnimationArchiveMetadata& metadata, const_span<uint8_t> payload) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationArchiveMetadata(metadata);

    if (payload.empty()) {
        throw ModelAnimationArchiveException("Can't write an LF model animation archive with an empty payload", GetModelAnimationArchiveContext(metadata));
    }

    const uint64_t payload_length = numeric_cast<uint64_t>(payload.size());
    const size_t wire_size = GetModelAnimationArchiveWireSize(metadata, payload.size());
    vector<uint8_t> data;

    if (wire_size > data.max_size()) {
        throw ModelAnimationArchiveException("LF model animation archive is too large for a byte vector", GetModelAnimationArchiveContext(metadata), wire_size);
    }

    data.reserve(wire_size);
    AppendModelAnimationArchiveBytes(data, {MODEL_ANIMATION_ARCHIVE_MAGIC.data(), MODEL_ANIMATION_ARCHIVE_MAGIC.size()});
    AppendModelAnimationArchiveLittleEndian(data, MODEL_ANIMATION_ARCHIVE_SCHEMA_VERSION);
    AppendModelAnimationArchiveLittleEndian(data, static_cast<uint16_t>(metadata.Kind));
    AppendModelAnimationArchiveLittleEndian(data, metadata.Flags);
    AppendModelAnimationArchiveLittleEndian(data, metadata.RigSignature);
    AppendModelAnimationArchiveLittleEndian(data, metadata.SourceSignature);
    AppendModelAnimationArchiveLittleEndian(data, metadata.CacheSignature);
    AppendModelAnimationArchiveString(data, MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION, "codec revision", metadata);
    AppendModelAnimationArchiveString(data, metadata.SourceAsset, "source asset", metadata);
    AppendModelAnimationArchiveString(data, metadata.ObjectName, "object name", metadata);
    AppendModelAnimationArchiveLittleEndian(data, payload_length);
    AppendModelAnimationArchiveLittleEndian(data, HashModelAnimationArchivePayload(payload));
    AppendModelAnimationArchiveBytes(data, payload);
    FO_STRONG_ASSERT(data.size() == wire_size, "LF model animation archive wire-size preflight is out of sync", data.size(), wire_size);
    return data;
}

auto ReadModelAnimationArchive(const_span<uint8_t> data, const ModelAnimationArchiveMetadata& expected_metadata) -> ModelAnimationArchive
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationArchiveMetadata(expected_metadata);
    const string context = GetModelAnimationArchiveContext(expected_metadata);
    size_t read_pos = 0;

    const const_span<uint8_t> magic = ReadModelAnimationArchiveBytes(data, read_pos, MODEL_ANIMATION_ARCHIVE_MAGIC.size(), "magic", context);

    if (!std::equal(magic.begin(), magic.end(), MODEL_ANIMATION_ARCHIVE_MAGIC.begin())) {
        throw ModelAnimationArchiveException("Invalid LF model animation archive magic; expected 'LFOZZARC'", context);
    }

    ModelAnimationArchive archive;
    archive.SchemaVersion = ReadModelAnimationArchiveLittleEndian<uint16_t>(data, read_pos, "schema version", context);

    if (archive.SchemaVersion != MODEL_ANIMATION_ARCHIVE_SCHEMA_VERSION) {
        throw ModelAnimationArchiveException("Unsupported LF model animation archive schema (expected vs got)", context, MODEL_ANIMATION_ARCHIVE_SCHEMA_VERSION, archive.SchemaVersion);
    }

    const uint16_t raw_kind = ReadModelAnimationArchiveLittleEndian<uint16_t>(data, read_pos, "payload kind", context);
    archive.Metadata.Kind = static_cast<ModelAnimationArchiveKind>(raw_kind);

    if (!IsModelAnimationArchiveKindValid(archive.Metadata.Kind)) {
        throw ModelAnimationArchiveException("Invalid LF model animation archive payload kind", context, raw_kind);
    }
    if (archive.Metadata.Kind != expected_metadata.Kind) {
        throw ModelAnimationArchiveException("LF model animation archive payload kind mismatch (expected vs got)", context, static_cast<uint16_t>(expected_metadata.Kind), raw_kind);
    }

    archive.Metadata.Flags = ReadModelAnimationArchiveLittleEndian<uint32_t>(data, read_pos, "flags", context);

    if ((archive.Metadata.Flags & ~MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS) != 0) {
        throw ModelAnimationArchiveException("LF model animation archive contains unsupported flags", context, archive.Metadata.Flags);
    }
    if (archive.Metadata.Flags != expected_metadata.Flags) {
        throw ModelAnimationArchiveException("LF model animation archive flags mismatch (expected vs got)", context, expected_metadata.Flags, archive.Metadata.Flags);
    }

    archive.Metadata.RigSignature = ReadModelAnimationArchiveLittleEndian<uint64_t>(data, read_pos, "rig signature", context);

    if (archive.Metadata.RigSignature != expected_metadata.RigSignature) {
        throw ModelAnimationArchiveException("LF model animation archive rig signature mismatch (expected vs got)", context, expected_metadata.RigSignature, archive.Metadata.RigSignature);
    }

    archive.Metadata.SourceSignature = ReadModelAnimationArchiveLittleEndian<uint64_t>(data, read_pos, "source signature", context);

    if (archive.Metadata.SourceSignature != expected_metadata.SourceSignature) {
        throw ModelAnimationArchiveException("LF model animation archive source signature mismatch (expected vs got)", context, expected_metadata.SourceSignature, archive.Metadata.SourceSignature);
    }

    archive.Metadata.CacheSignature = ReadModelAnimationArchiveLittleEndian<uint64_t>(data, read_pos, "cache signature", context);

    if (archive.Metadata.CacheSignature != expected_metadata.CacheSignature) {
        throw ModelAnimationArchiveException("LF model animation archive cache signature mismatch (expected vs got)", context, expected_metadata.CacheSignature, archive.Metadata.CacheSignature);
    }

    archive.PayloadRevision = ReadModelAnimationArchiveString(data, read_pos, "codec revision", context);

    if (archive.PayloadRevision != MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION) {
        throw ModelAnimationArchiveException("LF model animation archive revision mismatch (expected vs got)", context, MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION, archive.PayloadRevision);
    }

    archive.Metadata.SourceAsset = ReadModelAnimationArchiveString(data, read_pos, "source asset", context);

    if (archive.Metadata.SourceAsset != expected_metadata.SourceAsset) {
        throw ModelAnimationArchiveException("LF model animation archive source asset mismatch (expected vs got)", context, expected_metadata.SourceAsset, archive.Metadata.SourceAsset);
    }

    archive.Metadata.ObjectName = ReadModelAnimationArchiveString(data, read_pos, "object name", context);

    if (archive.Metadata.ObjectName != expected_metadata.ObjectName) {
        throw ModelAnimationArchiveException("LF model animation archive object name mismatch (expected vs got)", context, expected_metadata.ObjectName, archive.Metadata.ObjectName);
    }

    const uint64_t payload_length = ReadModelAnimationArchiveLittleEndian<uint64_t>(data, read_pos, "payload length", context);
    archive.PayloadHash = ReadModelAnimationArchiveLittleEndian<uint64_t>(data, read_pos, "payload hash", context);

    if (payload_length == 0) {
        throw ModelAnimationArchiveException("LF model animation archive has an empty payload", context);
    }
    if (payload_length > std::numeric_limits<size_t>::max()) {
        throw ModelAnimationArchiveException("LF model animation archive payload is too large for this platform", context, payload_length);
    }

    const size_t expected_payload_size = static_cast<size_t>(payload_length);
    const size_t remaining_size = data.size() - read_pos;

    if (remaining_size < expected_payload_size) {
        throw ModelAnimationArchiveException("Truncated LF model animation archive payload (declared vs remaining)", context, expected_payload_size, remaining_size);
    }
    if (remaining_size > expected_payload_size) {
        throw ModelAnimationArchiveException("LF model animation archive has trailing bytes after its payload", context, remaining_size - expected_payload_size, expected_payload_size);
    }

    const const_span<uint8_t> payload = ReadModelAnimationArchiveBytes(data, read_pos, expected_payload_size, "payload", context);
    const uint64_t actual_payload_hash = HashModelAnimationArchivePayload(payload);

    if (archive.PayloadHash != actual_payload_hash) {
        throw ModelAnimationArchiveException("LF model animation archive payload hash mismatch (stored vs computed)", context, archive.PayloadHash, actual_payload_hash);
    }

    archive.Payload.assign(payload.begin(), payload.end());
    return archive;
}

static auto IsModelAnimationArchiveKindValid(ModelAnimationArchiveKind kind) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    switch (kind) {
    case ModelAnimationArchiveKind::Skeleton:
    case ModelAnimationArchiveKind::Animation:
    case ModelAnimationArchiveKind::JointRemap:
        return true;
    default:
        return false;
    }
}

static auto GetModelAnimationArchiveContext(const ModelAnimationArchiveMetadata& metadata) -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex("'{}#{}'", metadata.SourceAsset, metadata.ObjectName);
}

static void ValidateModelAnimationArchiveMetadata(const ModelAnimationArchiveMetadata& metadata)
{
    FO_STACK_TRACE_ENTRY();

    const string context = GetModelAnimationArchiveContext(metadata);

    if (!IsModelAnimationArchiveKindValid(metadata.Kind)) {
        throw ModelAnimationArchiveException("Invalid LF model animation archive payload kind", context, static_cast<uint16_t>(metadata.Kind));
    }
    if ((metadata.Flags & ~MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS) != 0) {
        throw ModelAnimationArchiveException("LF model animation archive metadata contains unsupported flags", context, metadata.Flags);
    }
    if (metadata.RigSignature == 0) {
        throw ModelAnimationArchiveException("LF model animation archive metadata has an empty rig signature", context);
    }
    if (metadata.SourceSignature == 0) {
        throw ModelAnimationArchiveException("LF model animation archive metadata has an empty source signature", context);
    }
    if (metadata.CacheSignature == 0) {
        throw ModelAnimationArchiveException("LF model animation archive metadata has an empty cache signature", context);
    }
    if (metadata.SourceAsset.empty()) {
        throw ModelAnimationArchiveException("LF model animation archive metadata has an empty source asset", context);
    }
    if (!strvex(metadata.SourceAsset).is_valid_utf8() || metadata.SourceAsset.find('\0') != string::npos) {
        throw ModelAnimationArchiveException("LF model animation archive metadata has a source asset that is not valid UTF-8 or contains an embedded NUL", context);
    }
    if (metadata.ObjectName.empty()) {
        throw ModelAnimationArchiveException("LF model animation archive metadata has an empty object name", context);
    }
    if (!strvex(metadata.ObjectName).is_valid_utf8() || metadata.ObjectName.find('\0') != string::npos) {
        throw ModelAnimationArchiveException("LF model animation archive metadata has an object name that is not valid UTF-8 or contains an embedded NUL", context);
    }
}

static auto GetModelAnimationArchiveWireSize(const ModelAnimationArchiveMetadata& metadata, size_t payload_size) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t fixed_size = MODEL_ANIMATION_ARCHIVE_MAGIC.size() + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + 3 * sizeof(uint64_t) + 3 * sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t);
    const array<size_t, 4> variable_sizes {MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION.size(), metadata.SourceAsset.size(), metadata.ObjectName.size(), payload_size};
    size_t wire_size = fixed_size;

    for (size_t variable_size : variable_sizes) {
        if (variable_size > std::numeric_limits<size_t>::max() - wire_size) {
            throw ModelAnimationArchiveException("LF model animation archive size overflows this platform", GetModelAnimationArchiveContext(metadata));
        }

        wire_size += variable_size;
    }

    return wire_size;
}

template<typename T>
static void AppendModelAnimationArchiveLittleEndian(vector<uint8_t>& data, T value)
{
    FO_STACK_TRACE_ENTRY();

    static_assert(std::is_unsigned_v<T>);

    for (size_t i = 0; i < sizeof(T); i++) {
        data.emplace_back(static_cast<uint8_t>((value >> (i * 8)) & static_cast<T>(0xFF)));
    }
}

static void AppendModelAnimationArchiveBytes(vector<uint8_t>& data, const_span<uint8_t> bytes)
{
    FO_STACK_TRACE_ENTRY();

    data.insert(data.end(), bytes.begin(), bytes.end());
}

static void AppendModelAnimationArchiveString(vector<uint8_t>& data, string_view value, string_view field_name, const ModelAnimationArchiveMetadata& metadata)
{
    FO_STACK_TRACE_ENTRY();

    if (value.size() > std::numeric_limits<uint32_t>::max()) {
        throw ModelAnimationArchiveException("LF model animation archive field is too long", field_name, GetModelAnimationArchiveContext(metadata), value.size());
    }

    AppendModelAnimationArchiveLittleEndian(data, static_cast<uint32_t>(value.size()));

    for (char value_char : value) {
        data.emplace_back(static_cast<uint8_t>(value_char));
    }
}

static auto ReadModelAnimationArchiveBytes(const_span<uint8_t> data, size_t& read_pos, size_t size, string_view field_name, string_view context) -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(read_pos <= data.size(), "LF model animation archive reader position is outside the input", read_pos, data.size());
    const size_t remaining_size = data.size() - read_pos;

    if (size > remaining_size) {
        throw ModelAnimationArchiveException("Truncated LF model animation archive while reading a field (need vs remain)", context, field_name, size, remaining_size);
    }

    const const_span<uint8_t> bytes = data.subspan(read_pos, size);
    read_pos += size;
    return bytes;
}

template<typename T>
static auto ReadModelAnimationArchiveLittleEndian(const_span<uint8_t> data, size_t& read_pos, string_view field_name, string_view context) -> T
{
    FO_STACK_TRACE_ENTRY();

    static_assert(std::is_unsigned_v<T>);
    const const_span<uint8_t> bytes = ReadModelAnimationArchiveBytes(data, read_pos, sizeof(T), field_name, context);
    T value = 0;

    for (size_t i = 0; i < sizeof(T); i++) {
        value |= static_cast<T>(bytes[i]) << (i * 8);
    }

    return value;
}

static auto ReadModelAnimationArchiveString(const_span<uint8_t> data, size_t& read_pos, string_view field_name, string_view context) -> string
{
    FO_STACK_TRACE_ENTRY();

    size_t string_read_pos = read_pos;
    const uint32_t size = ReadModelAnimationArchiveLittleEndian<uint32_t>(data, string_read_pos, strex("{} length", field_name), context);
    const const_span<uint8_t> bytes = ReadModelAnimationArchiveBytes(data, string_read_pos, size, field_name, context);
    string value;
    value.reserve(size);

    for (uint8_t value_byte : bytes) {
        value.push_back(static_cast<char>(value_byte));
    }

    read_pos = string_read_pos;
    return value;
}

static auto HashModelAnimationArchivePayload(const_span<uint8_t> payload) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = MODEL_ANIMATION_ARCHIVE_HASH_OFFSET;

    for (uint8_t value : payload) {
        hash ^= value;
        hash *= MODEL_ANIMATION_ARCHIVE_HASH_PRIME;
    }

    return hash;
}

static constexpr uint32_t MODEL_ANIMATION_RIG_BINDING_REVERSED = 0x01;
static constexpr uint32_t MODEL_ANIMATION_RIG_BINDING_SUPPORTED_FLAGS = MODEL_ANIMATION_RIG_BINDING_REVERSED;
static constexpr size_t MODEL_ANIMATION_RIG_BINDING_WIRE_SIZE = sizeof(uint32_t) * 4;
static constexpr size_t MODEL_ANIMATION_RIG_MIN_ARCHIVE_MANIFEST_SIZE = sizeof(uint64_t) + sizeof(uint32_t) + 1 + sizeof(uint32_t) + 1 + sizeof(uint64_t) + 1;

static void ValidateModelAnimationRigData(const ModelAnimationRigData& rig, string_view context);
static void ValidateModelAnimationRigArchiveData(const ModelAnimationRigArchiveData& archive, ModelAnimationArchiveKind kind, uint64_t rig_signature, uint64_t cache_signature, string_view context);
static void AppendModelAnimationRigArchiveData(vector<uint8_t>& data, const ModelAnimationRigArchiveData& archive, string_view context);
static auto ReadModelAnimationRigArchiveData(const_span<uint8_t> data, size_t& read_pos, ModelAnimationArchiveKind kind, uint64_t rig_signature, uint64_t cache_signature, string_view context) -> ModelAnimationRigArchiveData;
template<typename T>
static void AppendModelAnimationRigDataLittleEndian(vector<uint8_t>& data, T value);
static void AppendModelAnimationRigDataBytes(vector<uint8_t>& data, const_span<uint8_t> bytes);
static void AppendModelAnimationRigDataString(vector<uint8_t>& data, string_view value, string_view field, string_view context);
template<typename T>
static auto ReadModelAnimationRigDataLittleEndian(const_span<uint8_t> data, size_t& read_pos, string_view field, string_view context) -> T;
static auto ReadModelAnimationRigDataBytes(const_span<uint8_t> data, size_t& read_pos, size_t size, string_view field, string_view context) -> const_span<uint8_t>;
static auto ReadModelAnimationRigDataString(const_span<uint8_t> data, size_t& read_pos, string_view field, string_view context) -> string;

auto WriteModelAnimationJointRemapPayload(const ModelAnimationJointRemap& remap, string_view context) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationJointRemap(remap, context);
    const size_t wire_size = MODEL_ANIMATION_JOINT_REMAP_MAGIC.size() + sizeof(uint16_t) * 2 + sizeof(uint32_t) * 4 + remap.SourceToCanonicalJointIndices.size() * sizeof(uint32_t) + remap.CanonicalJointPresent.size() + remap.NearestSampleTimes.size() * sizeof(uint32_t);
    vector<uint8_t> result;
    result.reserve(wire_size);
    AppendModelAnimationRigDataBytes(result, {MODEL_ANIMATION_JOINT_REMAP_MAGIC.data(), MODEL_ANIMATION_JOINT_REMAP_MAGIC.size()});
    AppendModelAnimationRigDataLittleEndian(result, MODEL_ANIMATION_JOINT_REMAP_SCHEMA_VERSION);
    AppendModelAnimationRigDataLittleEndian(result, uint16_t {0});
    AppendModelAnimationRigDataLittleEndian(result, std::bit_cast<uint32_t>(remap.Duration));
    AppendModelAnimationRigDataLittleEndian(result, remap.CanonicalJointCount);
    AppendModelAnimationRigDataLittleEndian(result, numeric_cast<uint32_t>(remap.SourceToCanonicalJointIndices.size()));
    AppendModelAnimationRigDataLittleEndian(result, numeric_cast<uint32_t>(remap.NearestSampleTimes.size()));

    for (uint32_t canonical_index : remap.SourceToCanonicalJointIndices) {
        AppendModelAnimationRigDataLittleEndian(result, canonical_index);
    }

    AppendModelAnimationRigDataBytes(result, remap.CanonicalJointPresent);

    for (float32_t time : remap.NearestSampleTimes) {
        AppendModelAnimationRigDataLittleEndian(result, std::bit_cast<uint32_t>(time));
    }

    FO_STRONG_ASSERT(result.size() == wire_size, "Animation joint-remap wire-size preflight is out of sync", result.size(), wire_size, context);
    return result;
}

auto ReadModelAnimationJointRemapPayload(const_span<uint8_t> payload, string_view context) -> ModelAnimationJointRemap
{
    FO_STACK_TRACE_ENTRY();

    size_t read_pos = 0;
    const const_span<uint8_t> magic = ReadModelAnimationRigDataBytes(payload, read_pos, MODEL_ANIMATION_JOINT_REMAP_MAGIC.size(), "magic", context);

    if (!std::equal(magic.begin(), magic.end(), MODEL_ANIMATION_JOINT_REMAP_MAGIC.begin())) {
        throw ModelAnimationRigDataException("Invalid animation joint-remap magic", context);
    }

    const uint16_t schema = ReadModelAnimationRigDataLittleEndian<uint16_t>(payload, read_pos, "schema", context);
    const uint16_t reserved = ReadModelAnimationRigDataLittleEndian<uint16_t>(payload, read_pos, "reserved flags", context);

    if (schema != MODEL_ANIMATION_JOINT_REMAP_SCHEMA_VERSION || reserved != 0) {
        throw ModelAnimationRigDataException("Unsupported animation joint-remap header (schema, reserved)", context, schema, reserved);
    }

    ModelAnimationJointRemap result;
    result.Duration = std::bit_cast<float32_t>(ReadModelAnimationRigDataLittleEndian<uint32_t>(payload, read_pos, "duration", context));
    result.CanonicalJointCount = ReadModelAnimationRigDataLittleEndian<uint32_t>(payload, read_pos, "canonical joint count", context);
    const uint32_t source_count = ReadModelAnimationRigDataLittleEndian<uint32_t>(payload, read_pos, "source joint count", context);
    const uint32_t nearest_time_count = ReadModelAnimationRigDataLittleEndian<uint32_t>(payload, read_pos, "nearest time count", context);

    if (result.CanonicalJointCount == 0 || result.CanonicalJointCount > MODEL_ANIMATION_RIG_MAX_JOINTS) {
        throw ModelAnimationRigDataException("Invalid canonical joint count", result.CanonicalJointCount, context);
    }
    if (source_count > result.CanonicalJointCount) {
        throw ModelAnimationRigDataException("Source joint count exceeds canonical joint count", source_count, result.CanonicalJointCount, context);
    }
    if (nearest_time_count > std::numeric_limits<uint16_t>::max()) {
        throw ModelAnimationRigDataException("Nearest-sampling timeline exceeds the maximum time points", context, nearest_time_count, std::numeric_limits<uint16_t>::max());
    }

    const size_t expected_body_size = numeric_cast<size_t>(source_count) * sizeof(uint32_t) + numeric_cast<size_t>(result.CanonicalJointCount) + numeric_cast<size_t>(nearest_time_count) * sizeof(uint32_t);

    if (read_pos > payload.size() || expected_body_size != payload.size() - read_pos) {
        throw ModelAnimationRigDataException("Invalid animation joint-remap payload size (expected vs got body bytes)", context, expected_body_size, read_pos <= payload.size() ? payload.size() - read_pos : 0);
    }

    result.SourceToCanonicalJointIndices.reserve(source_count);

    for (uint32_t i = 0; i < source_count; i++) {
        result.SourceToCanonicalJointIndices.emplace_back(ReadModelAnimationRigDataLittleEndian<uint32_t>(payload, read_pos, "source-to-canonical index", context));
    }

    const const_span<uint8_t> presence = ReadModelAnimationRigDataBytes(payload, read_pos, result.CanonicalJointCount, "canonical presence mask", context);
    result.CanonicalJointPresent.assign(presence.begin(), presence.end());
    result.NearestSampleTimes.reserve(nearest_time_count);

    for (uint32_t i = 0; i < nearest_time_count; i++) {
        result.NearestSampleTimes.emplace_back(std::bit_cast<float32_t>(ReadModelAnimationRigDataLittleEndian<uint32_t>(payload, read_pos, "nearest sample time", context)));
    }

    FO_STRONG_ASSERT(read_pos == payload.size(), "Animation joint-remap body preflight did not consume the payload exactly", read_pos, payload.size(), context);
    ValidateModelAnimationJointRemap(result, context);
    return result;
}

void ValidateModelAnimationJointRemap(const ModelAnimationJointRemap& remap, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (!std::isfinite(remap.Duration) || remap.Duration < 0.0f || (remap.Duration > 0.0f && !std::isfinite(1.0f / remap.Duration))) {
        throw ModelAnimationRigDataException("Invalid remap duration", remap.Duration, context);
    }
    if (remap.CanonicalJointCount == 0 || remap.CanonicalJointCount > MODEL_ANIMATION_RIG_MAX_JOINTS) {
        throw ModelAnimationRigDataException("Invalid canonical joint count", remap.CanonicalJointCount, context);
    }
    if (remap.SourceToCanonicalJointIndices.size() > remap.CanonicalJointCount) {
        throw ModelAnimationRigDataException("Source remap count exceeds canonical joint count", remap.SourceToCanonicalJointIndices.size(), remap.CanonicalJointCount, context);
    }
    if (remap.CanonicalJointPresent.size() != remap.CanonicalJointCount) {
        throw ModelAnimationRigDataException("Presence mask size does not match canonical joint count", remap.CanonicalJointPresent.size(), remap.CanonicalJointCount, context);
    }
    if (remap.NearestSampleTimes.size() > std::numeric_limits<uint16_t>::max()) {
        throw ModelAnimationRigDataException("Nearest-sampling timeline exceeds the maximum time points", context, remap.NearestSampleTimes.size(), std::numeric_limits<uint16_t>::max());
    }

    set<uint32_t> mapped_indices;

    for (uint32_t canonical_index : remap.SourceToCanonicalJointIndices) {
        if (canonical_index >= remap.CanonicalJointCount) {
            throw ModelAnimationRigDataException("Canonical joint index is outside its valid range", canonical_index, remap.CanonicalJointCount, context);
        }
        if (!mapped_indices.emplace(canonical_index).second) {
            throw ModelAnimationRigDataException("Duplicate canonical joint index", canonical_index, context);
        }
        if (remap.CanonicalJointPresent[canonical_index] != 1) {
            throw ModelAnimationRigDataException("Mapped canonical joint is absent from the presence mask", canonical_index, context);
        }
    }

    size_t present_count = 0;

    for (size_t i = 0; i < remap.CanonicalJointPresent.size(); i++) {
        if (remap.CanonicalJointPresent[i] > 1) {
            throw ModelAnimationRigDataException("Invalid presence value for canonical joint", remap.CanonicalJointPresent[i], i, context);
        }

        present_count += remap.CanonicalJointPresent[i];
    }

    if (present_count != remap.SourceToCanonicalJointIndices.size()) {
        throw ModelAnimationRigDataException("Presence count does not match source remap count", present_count, remap.SourceToCanonicalJointIndices.size(), context);
    }

    if (!remap.NearestSampleTimes.empty()) {
        if (remap.Duration <= 0.0f || remap.NearestSampleTimes.front() != 0.0f || remap.NearestSampleTimes.back() != remap.Duration) {
            throw ModelAnimationRigDataException("Invalid nearest-sampling duration/timeline endpoints", context);
        }

        for (size_t i = 0; i < remap.NearestSampleTimes.size(); i++) {
            const float32_t time = remap.NearestSampleTimes[i];

            if (!std::isfinite(time) || time < 0.0f || time > remap.Duration || (i != 0 && time <= remap.NearestSampleTimes[i - 1])) {
                throw ModelAnimationRigDataException("Invalid nearest-sampling time at index", time, i, context);
            }
        }
    }
}

auto WriteModelAnimationRigData(const ModelAnimationRigData& rig, string_view context) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationRigData(rig, context);
    vector<uint8_t> result;
    result.reserve(1024);
    AppendModelAnimationRigDataBytes(result, {MODEL_ANIMATION_RIG_DATA_MAGIC.data(), MODEL_ANIMATION_RIG_DATA_MAGIC.size()});
    AppendModelAnimationRigDataLittleEndian(result, MODEL_ANIMATION_RIG_DATA_SCHEMA_VERSION);
    AppendModelAnimationRigDataLittleEndian(result, MODEL_ANIMATION_RIG_DATA_SUPPORTED_FLAGS);
    AppendModelAnimationRigDataLittleEndian(result, rig.RigSignature);
    AppendModelAnimationRigDataLittleEndian(result, rig.CacheSignature);
    AppendModelAnimationRigDataLittleEndian(result, numeric_cast<uint32_t>(rig.Clips.size()));
    AppendModelAnimationRigDataLittleEndian(result, numeric_cast<uint32_t>(rig.Bindings.size()));

    try {
        AppendModelAnimationRigArchiveData(result, rig.Skeleton, strex("{} canonical skeleton", context));
        AppendModelAnimationRigArchiveData(result, rig.BaseJointRemap, strex("{} base joint remap", context));

        for (const ModelAnimationRigClipData& clip : rig.Clips) {
            const string clip_context = strex("{} clip '{}#{}'", context, clip.Animation.Metadata.SourceAsset, clip.Animation.Metadata.ObjectName);
            AppendModelAnimationRigArchiveData(result, clip.Animation, clip_context);
            AppendModelAnimationRigArchiveData(result, clip.JointRemap, clip_context);
        }
    }
    catch (const ModelAnimationArchiveException& ex) {
        throw ModelAnimationRigDataException("Can't write animation rig data", context, ex.what());
    }

    for (const ModelAnimationRigBinding& binding : rig.Bindings) {
        AppendModelAnimationRigDataLittleEndian(result, std::bit_cast<uint32_t>(binding.StateAnim));
        AppendModelAnimationRigDataLittleEndian(result, std::bit_cast<uint32_t>(binding.ActionAnim));
        AppendModelAnimationRigDataLittleEndian(result, binding.ClipIndex);
        AppendModelAnimationRigDataLittleEndian(result, binding.Reversed ? MODEL_ANIMATION_RIG_BINDING_REVERSED : uint32_t {0});
    }

    return result;
}

auto ReadModelAnimationRigData(const_span<uint8_t> data, string_view context) -> ModelAnimationRigData
{
    FO_STACK_TRACE_ENTRY();

    size_t read_pos = 0;
    const const_span<uint8_t> magic = ReadModelAnimationRigDataBytes(data, read_pos, MODEL_ANIMATION_RIG_DATA_MAGIC.size(), "magic", context);

    if (!std::equal(magic.begin(), magic.end(), MODEL_ANIMATION_RIG_DATA_MAGIC.begin())) {
        throw ModelAnimationRigDataException("Invalid animation rig-data magic; expected 'LFOZZRIG'", context);
    }

    const uint16_t schema = ReadModelAnimationRigDataLittleEndian<uint16_t>(data, read_pos, "schema", context);
    const uint16_t flags = ReadModelAnimationRigDataLittleEndian<uint16_t>(data, read_pos, "flags", context);

    if (schema != MODEL_ANIMATION_RIG_DATA_SCHEMA_VERSION) {
        throw ModelAnimationRigDataException("Unsupported animation rig-data schema (expected vs got)", context, MODEL_ANIMATION_RIG_DATA_SCHEMA_VERSION, schema);
    }
    if ((flags & ~MODEL_ANIMATION_RIG_DATA_SUPPORTED_FLAGS) != 0) {
        throw ModelAnimationRigDataException("Animation rig data contains unsupported flags", context, flags);
    }

    ModelAnimationRigData result;
    result.RigSignature = ReadModelAnimationRigDataLittleEndian<uint64_t>(data, read_pos, "rig signature", context);
    result.CacheSignature = ReadModelAnimationRigDataLittleEndian<uint64_t>(data, read_pos, "cache signature", context);
    const uint32_t clip_count = ReadModelAnimationRigDataLittleEndian<uint32_t>(data, read_pos, "clip count", context);
    const uint32_t binding_count = ReadModelAnimationRigDataLittleEndian<uint32_t>(data, read_pos, "binding count", context);

    if (clip_count > MODEL_ANIMATION_RIG_MAX_CLIPS) {
        throw ModelAnimationRigDataException("Animation rig data exceeds the maximum clip count", context, clip_count, MODEL_ANIMATION_RIG_MAX_CLIPS);
    }
    if (binding_count > MODEL_ANIMATION_RIG_MAX_BINDINGS) {
        throw ModelAnimationRigDataException("Animation rig data exceeds the maximum binding count", context, binding_count, MODEL_ANIMATION_RIG_MAX_BINDINGS);
    }

    try {
        result.Skeleton = ReadModelAnimationRigArchiveData(data, read_pos, ModelAnimationArchiveKind::Skeleton, result.RigSignature, result.CacheSignature, strex("{} canonical skeleton", context));
        result.BaseJointRemap = ReadModelAnimationRigArchiveData(data, read_pos, ModelAnimationArchiveKind::JointRemap, result.RigSignature, result.CacheSignature, strex("{} base joint remap", context));

        FO_STRONG_ASSERT(read_pos <= data.size(), "Animation rig-data reader position is outside the input", read_pos, data.size(), context);
        const size_t remaining_size = data.size() - read_pos;
        const size_t minimum_clip_bytes = numeric_cast<size_t>(clip_count) * MODEL_ANIMATION_RIG_MIN_ARCHIVE_MANIFEST_SIZE * 2;
        const size_t minimum_binding_bytes = numeric_cast<size_t>(binding_count) * MODEL_ANIMATION_RIG_BINDING_WIRE_SIZE;

        if (minimum_clip_bytes > remaining_size || minimum_binding_bytes > remaining_size - minimum_clip_bytes) {
            throw ModelAnimationRigDataException("Truncated animation rig data (required vs remaining trailing bytes)", context, clip_count, binding_count, minimum_clip_bytes + minimum_binding_bytes, remaining_size);
        }

        result.Clips.reserve(clip_count);

        for (uint32_t i = 0; i < clip_count; i++) {
            ModelAnimationRigClipData& clip = result.Clips.emplace_back();
            clip.Animation = ReadModelAnimationRigArchiveData(data, read_pos, ModelAnimationArchiveKind::Animation, result.RigSignature, result.CacheSignature, strex("{} clip {} animation", context, i));
            clip.JointRemap = ReadModelAnimationRigArchiveData(data, read_pos, ModelAnimationArchiveKind::JointRemap, result.RigSignature, result.CacheSignature, strex("{} clip {} joint remap", context, i));
        }
    }
    catch (const ModelAnimationArchiveException& ex) {
        throw ModelAnimationRigDataException("Can't read animation rig data", context, ex.what());
    }

    result.Bindings.reserve(binding_count);

    for (uint32_t i = 0; i < binding_count; i++) {
        ModelAnimationRigBinding& binding = result.Bindings.emplace_back();
        binding.StateAnim = std::bit_cast<int32_t>(ReadModelAnimationRigDataLittleEndian<uint32_t>(data, read_pos, "binding state animation", context));
        binding.ActionAnim = std::bit_cast<int32_t>(ReadModelAnimationRigDataLittleEndian<uint32_t>(data, read_pos, "binding action animation", context));
        binding.ClipIndex = ReadModelAnimationRigDataLittleEndian<uint32_t>(data, read_pos, "binding clip index", context);
        const uint32_t binding_flags = ReadModelAnimationRigDataLittleEndian<uint32_t>(data, read_pos, "binding flags", context);

        if ((binding_flags & ~MODEL_ANIMATION_RIG_BINDING_SUPPORTED_FLAGS) != 0) {
            throw ModelAnimationRigDataException("Animation rig binding contains unsupported flags", i, context, binding_flags);
        }

        binding.Reversed = (binding_flags & MODEL_ANIMATION_RIG_BINDING_REVERSED) != 0;
    }

    if (read_pos != data.size()) {
        throw ModelAnimationRigDataException("Animation rig data has trailing bytes", context, data.size() - read_pos);
    }

    ValidateModelAnimationRigData(result, context);
    return result;
}

static void ValidateModelAnimationRigData(const ModelAnimationRigData& rig, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (rig.RigSignature == 0 || rig.CacheSignature == 0) {
        throw ModelAnimationRigDataException("Animation rig data has empty rig/cache signatures", context, rig.RigSignature, rig.CacheSignature);
    }
    if (rig.Clips.size() > MODEL_ANIMATION_RIG_MAX_CLIPS || rig.Bindings.size() > MODEL_ANIMATION_RIG_MAX_BINDINGS) {
        throw ModelAnimationRigDataException("Animation rig data exceeds clip/binding limits", context, rig.Clips.size(), MODEL_ANIMATION_RIG_MAX_CLIPS, rig.Bindings.size(), MODEL_ANIMATION_RIG_MAX_BINDINGS);
    }

    ValidateModelAnimationRigArchiveData(rig.Skeleton, ModelAnimationArchiveKind::Skeleton, rig.RigSignature, rig.CacheSignature, strex("{} canonical skeleton", context));
    ValidateModelAnimationRigArchiveData(rig.BaseJointRemap, ModelAnimationArchiveKind::JointRemap, rig.RigSignature, rig.CacheSignature, strex("{} base joint remap", context));

    if (rig.Skeleton.Metadata.ObjectName != "CanonicalSkeleton") {
        throw ModelAnimationRigDataException("Animation rig data has an invalid skeleton object name", context, rig.Skeleton.Metadata.ObjectName);
    }
    if (rig.BaseJointRemap.Metadata.ObjectName != "BaseJointRemap") {
        throw ModelAnimationRigDataException("Animation rig data has an invalid base-remap object name", context, rig.BaseJointRemap.Metadata.ObjectName);
    }

    pair<string_view, string_view> previous_clip_identity;
    bool has_previous_clip = false;

    for (size_t i = 0; i < rig.Clips.size(); i++) {
        const ModelAnimationRigClipData& clip = rig.Clips[i];
        const string clip_context = strex("{} clip {}", context, i);
        ValidateModelAnimationRigArchiveData(clip.Animation, ModelAnimationArchiveKind::Animation, rig.RigSignature, rig.CacheSignature, clip_context);
        ValidateModelAnimationRigArchiveData(clip.JointRemap, ModelAnimationArchiveKind::JointRemap, rig.RigSignature, rig.CacheSignature, clip_context);

        if (clip.Animation.Metadata.SourceAsset != clip.JointRemap.Metadata.SourceAsset) {
            throw ModelAnimationRigDataException("Animation rig clip has an animation/remap source mismatch", i, context, clip.Animation.Metadata.SourceAsset, clip.JointRemap.Metadata.SourceAsset);
        }

        const string expected_remap_name = strex("{}:JointRemap", clip.Animation.Metadata.ObjectName);

        if (clip.JointRemap.Metadata.ObjectName != expected_remap_name) {
            throw ModelAnimationRigDataException("Animation rig clip has an unexpected remap object name (actual vs expected)", i, context, clip.JointRemap.Metadata.ObjectName, expected_remap_name);
        }

        const pair<string_view, string_view> identity {clip.Animation.Metadata.SourceAsset, clip.Animation.Metadata.ObjectName};

        if (has_previous_clip && !(previous_clip_identity < identity)) {
            throw ModelAnimationRigDataException("Animation rig clips are duplicate or not strictly sorted", context, identity.first, identity.second);
        }

        previous_clip_identity = identity;
        has_previous_clip = true;
    }

    vector<uint8_t> referenced_clips(rig.Clips.size(), uint8_t {0});
    pair<int32_t, int32_t> previous_binding;
    bool has_previous_binding = false;

    for (size_t i = 0; i < rig.Bindings.size(); i++) {
        const ModelAnimationRigBinding& binding = rig.Bindings[i];
        const pair<int32_t, int32_t> identity {binding.StateAnim, binding.ActionAnim};

        if (has_previous_binding && !(previous_binding < identity)) {
            throw ModelAnimationRigDataException("Animation rig bindings are duplicate or not strictly sorted", context, binding.StateAnim, binding.ActionAnim);
        }
        if (binding.ClipIndex >= rig.Clips.size()) {
            throw ModelAnimationRigDataException("Animation rig binding references a clip outside its valid range", binding.StateAnim, binding.ActionAnim, context, binding.ClipIndex, rig.Clips.size());
        }

        referenced_clips[binding.ClipIndex] = uint8_t {1};
        previous_binding = identity;
        has_previous_binding = true;
    }

    for (size_t i = 0; i < referenced_clips.size(); i++) {
        if (referenced_clips[i] == 0) {
            throw ModelAnimationRigDataException("Animation rig clip has no state/action binding", i, rig.Clips[i].Animation.Metadata.SourceAsset, rig.Clips[i].Animation.Metadata.ObjectName, context);
        }
    }
}

static void ValidateModelAnimationRigArchiveData(const ModelAnimationRigArchiveData& archive, ModelAnimationArchiveKind kind, uint64_t rig_signature, uint64_t cache_signature, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (archive.Metadata.Kind != kind) {
        throw ModelAnimationRigDataException("Animation rig archive has an unexpected kind (actual vs expected)", context, static_cast<uint16_t>(archive.Metadata.Kind), static_cast<uint16_t>(kind));
    }
    if (archive.Metadata.Flags != MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS) {
        throw ModelAnimationRigDataException("Animation rig archive has unsupported flags", context, archive.Metadata.Flags);
    }
    if (archive.Metadata.RigSignature != rig_signature || archive.Metadata.CacheSignature != cache_signature) {
        throw ModelAnimationRigDataException("Animation rig archive has unexpected rig/cache signatures (actual vs expected)", context, archive.Metadata.RigSignature, archive.Metadata.CacheSignature, rig_signature, cache_signature);
    }
    if (archive.Metadata.SourceSignature == 0) {
        throw ModelAnimationRigDataException("Animation rig archive has an empty source signature", context);
    }
    if (archive.Metadata.SourceAsset.empty() || archive.Metadata.ObjectName.empty()) {
        throw ModelAnimationRigDataException("Animation rig archive has an empty source/object identity", context);
    }
    if (archive.Metadata.SourceAsset.size() > std::numeric_limits<uint32_t>::max() || archive.Metadata.ObjectName.size() > std::numeric_limits<uint32_t>::max()) {
        throw ModelAnimationRigDataException("Animation rig archive has a source/object identity that exceeds the wire limit", context);
    }
    if (!strvex(archive.Metadata.SourceAsset).is_valid_utf8() || archive.Metadata.SourceAsset.find('\0') != string::npos || !strvex(archive.Metadata.ObjectName).is_valid_utf8() || archive.Metadata.ObjectName.find('\0') != string::npos) {
        throw ModelAnimationRigDataException("Animation rig archive has an invalid UTF-8 or embedded-NUL identity", context);
    }
    if (archive.Payload.empty() || archive.Payload.size() > numeric_cast<size_t>(std::numeric_limits<int>::max())) {
        throw ModelAnimationRigDataException("Animation rig archive has an invalid runtime payload size", context, archive.Payload.size());
    }
}

static void AppendModelAnimationRigArchiveData(vector<uint8_t>& data, const ModelAnimationRigArchiveData& archive, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    const vector<uint8_t> archive_data = WriteModelAnimationArchive(archive.Metadata, archive.Payload);

    if (archive_data.size() > numeric_cast<size_t>(std::numeric_limits<int>::max())) {
        throw ModelAnimationRigDataException("Animation rig archive exceeds the runtime wire-size limit", context, archive_data.size());
    }

    AppendModelAnimationRigDataLittleEndian(data, archive.Metadata.SourceSignature);
    AppendModelAnimationRigDataString(data, archive.Metadata.SourceAsset, "source asset", context);
    AppendModelAnimationRigDataString(data, archive.Metadata.ObjectName, "object name", context);
    AppendModelAnimationRigDataLittleEndian(data, numeric_cast<uint64_t>(archive_data.size()));
    AppendModelAnimationRigDataBytes(data, archive_data);
}

static auto ReadModelAnimationRigArchiveData(const_span<uint8_t> data, size_t& read_pos, ModelAnimationArchiveKind kind, uint64_t rig_signature, uint64_t cache_signature, string_view context) -> ModelAnimationRigArchiveData
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationArchiveMetadata expected_metadata;
    expected_metadata.Kind = kind;
    expected_metadata.Flags = MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS;
    expected_metadata.RigSignature = rig_signature;
    expected_metadata.SourceSignature = ReadModelAnimationRigDataLittleEndian<uint64_t>(data, read_pos, "archive source signature", context);
    expected_metadata.CacheSignature = cache_signature;
    expected_metadata.SourceAsset = ReadModelAnimationRigDataString(data, read_pos, "archive source asset", context);
    expected_metadata.ObjectName = ReadModelAnimationRigDataString(data, read_pos, "archive object name", context);
    const uint64_t archive_size = ReadModelAnimationRigDataLittleEndian<uint64_t>(data, read_pos, "archive size", context);

    if (archive_size == 0 || archive_size > numeric_cast<uint64_t>(std::numeric_limits<int>::max())) {
        throw ModelAnimationRigDataException("Animation rig archive has an invalid runtime size", context, archive_size);
    }
    if (archive_size > std::numeric_limits<size_t>::max()) {
        throw ModelAnimationRigDataException("Animation rig archive is too large for this platform", context, archive_size);
    }

    const const_span<uint8_t> archive_bytes = ReadModelAnimationRigDataBytes(data, read_pos, static_cast<size_t>(archive_size), "archive bytes", context);
    ModelAnimationArchive archive = ReadModelAnimationArchive(archive_bytes, expected_metadata);
    return ModelAnimationRigArchiveData {std::move(archive.Metadata), std::move(archive.Payload)};
}

template<typename T>
static void AppendModelAnimationRigDataLittleEndian(vector<uint8_t>& data, T value)
{
    FO_STACK_TRACE_ENTRY();

    static_assert(std::is_unsigned_v<T>);

    for (size_t i = 0; i < sizeof(T); i++) {
        data.emplace_back(static_cast<uint8_t>((value >> (i * 8)) & static_cast<T>(0xFF)));
    }
}

static void AppendModelAnimationRigDataBytes(vector<uint8_t>& data, const_span<uint8_t> bytes)
{
    FO_STACK_TRACE_ENTRY();

    data.insert(data.end(), bytes.begin(), bytes.end());
}

static void AppendModelAnimationRigDataString(vector<uint8_t>& data, string_view value, string_view field, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    if (value.size() > std::numeric_limits<uint32_t>::max()) {
        throw ModelAnimationRigDataException("Animation rig-data field is too long", field, context, value.size());
    }

    AppendModelAnimationRigDataLittleEndian(data, static_cast<uint32_t>(value.size()));

    for (char value_char : value) {
        data.emplace_back(static_cast<uint8_t>(value_char));
    }
}

template<typename T>
static auto ReadModelAnimationRigDataLittleEndian(const_span<uint8_t> data, size_t& read_pos, string_view field, string_view context) -> T
{
    FO_STACK_TRACE_ENTRY();

    static_assert(std::is_unsigned_v<T>);
    const const_span<uint8_t> bytes = ReadModelAnimationRigDataBytes(data, read_pos, sizeof(T), field, context);
    T result = 0;

    for (size_t i = 0; i < sizeof(T); i++) {
        result |= static_cast<T>(bytes[i]) << (i * 8);
    }

    return result;
}

static auto ReadModelAnimationRigDataBytes(const_span<uint8_t> data, size_t& read_pos, size_t size, string_view field, string_view context) -> const_span<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    if (read_pos > data.size() || size > data.size() - read_pos) {
        throw ModelAnimationRigDataException("Truncated animation data while reading a field (need vs remain)", field, context, size, read_pos <= data.size() ? data.size() - read_pos : 0);
    }

    const const_span<uint8_t> result = data.subspan(read_pos, size);
    read_pos += size;
    return result;
}

static auto ReadModelAnimationRigDataString(const_span<uint8_t> data, size_t& read_pos, string_view field, string_view context) -> string
{
    FO_STACK_TRACE_ENTRY();

    size_t string_read_pos = read_pos;
    const uint32_t size = ReadModelAnimationRigDataLittleEndian<uint32_t>(data, string_read_pos, strex("{} length", field), context);
    const const_span<uint8_t> bytes = ReadModelAnimationRigDataBytes(data, string_read_pos, size, field, context);
    string result;
    result.reserve(size);

    for (uint8_t value : bytes) {
        result.push_back(static_cast<char>(value));
    }

    read_pos = string_read_pos;
    return result;
}

FO_END_NAMESPACE

#endif
