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

#include "catch_amalgamated.hpp"

#include "Common.h"

#if FO_ENABLE_3D

#include "ModelAnimationData.h"

FO_BEGIN_NAMESPACE

struct ModelAnimationArchiveTestOffsets
{
    size_t Schema {};
    size_t Kind {};
    size_t Flags {};
    size_t RigSignature {};
    size_t SourceSignature {};
    size_t CacheSignature {};
    size_t RevisionLength {};
    size_t Revision {};
    size_t SourceLength {};
    size_t Source {};
    size_t ObjectLength {};
    size_t Object {};
    size_t PayloadLength {};
    size_t PayloadHash {};
    size_t Payload {};
};

static auto MakeArchiveMetadata(ModelAnimationArchiveKind kind = ModelAnimationArchiveKind::Animation) -> ModelAnimationArchiveMetadata
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationArchiveMetadata metadata;
    metadata.Kind = kind;
    metadata.Flags = 0;
    metadata.RigSignature = 0x0807060504030201ULL;
    metadata.SourceSignature = 0x1817161514131211ULL;
    metadata.CacheSignature = 0x2827262524232221ULL;
    metadata.SourceAsset = "actors/human.fbx";
    metadata.ObjectName = "Run";
    return metadata;
}

static auto MakeArchiveOffsets(const ModelAnimationArchiveMetadata& metadata) -> ModelAnimationArchiveTestOffsets
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationArchiveTestOffsets offsets;
    offsets.Schema = MODEL_ANIMATION_ARCHIVE_MAGIC.size();
    offsets.Kind = offsets.Schema + sizeof(uint16_t);
    offsets.Flags = offsets.Kind + sizeof(uint16_t);
    offsets.RigSignature = offsets.Flags + sizeof(uint32_t);
    offsets.SourceSignature = offsets.RigSignature + sizeof(uint64_t);
    offsets.CacheSignature = offsets.SourceSignature + sizeof(uint64_t);
    offsets.RevisionLength = offsets.CacheSignature + sizeof(uint64_t);
    offsets.Revision = offsets.RevisionLength + sizeof(uint32_t);
    offsets.SourceLength = offsets.Revision + MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION.size();
    offsets.Source = offsets.SourceLength + sizeof(uint32_t);
    offsets.ObjectLength = offsets.Source + metadata.SourceAsset.size();
    offsets.Object = offsets.ObjectLength + sizeof(uint32_t);
    offsets.PayloadLength = offsets.Object + metadata.ObjectName.size();
    offsets.PayloadHash = offsets.PayloadLength + sizeof(uint64_t);
    offsets.Payload = offsets.PayloadHash + sizeof(uint64_t);
    return offsets;
}

template<typename T>
static void OverwriteLittleEndian(vector<uint8_t>& data, size_t offset, T value)
{
    FO_STACK_TRACE_ENTRY();

    static_assert(std::is_unsigned_v<T>);
    REQUIRE(offset + sizeof(T) <= data.size());

    for (size_t i = 0; i < sizeof(T); i++) {
        data[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & static_cast<T>(0xFF));
    }
}

static auto GetReadError(const vector<uint8_t>& data, const ModelAnimationArchiveMetadata& metadata) -> string
{
    FO_STACK_TRACE_ENTRY();

    try {
        (void)ReadModelAnimationArchive(data, metadata);
    }
    catch (const ModelAnimationArchiveException& ex) {
        return ex.what();
    }

    return {};
}

static auto ErrorContains(const vector<uint8_t>& data, const ModelAnimationArchiveMetadata& metadata, string_view needle) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetReadError(data, metadata).find(needle) != string::npos;
}

TEST_CASE("ModelAnimationArchive round-trip and deterministic layout")
{
    vector<uint8_t> payload {0x00, 0x01, 0x7F, 0x80, 0xFF};

    SECTION("All payload kinds round-trip")
    {
        constexpr array kinds {
            ModelAnimationArchiveKind::Skeleton,
            ModelAnimationArchiveKind::Animation,
            ModelAnimationArchiveKind::JointRemap,
        };

        for (ModelAnimationArchiveKind kind : kinds) {
            CAPTURE(static_cast<uint16_t>(kind));
            ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata(kind);
            vector<uint8_t> data = WriteModelAnimationArchive(metadata, payload);
            ModelAnimationArchive archive = ReadModelAnimationArchive(data, metadata);

            CHECK(archive.SchemaVersion == MODEL_ANIMATION_ARCHIVE_SCHEMA_VERSION);
            CHECK(string_view {archive.PayloadRevision} == MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION);
            CHECK(archive.Metadata.Kind == metadata.Kind);
            CHECK(archive.Metadata.Flags == metadata.Flags);
            CHECK(archive.Metadata.RigSignature == metadata.RigSignature);
            CHECK(archive.Metadata.SourceSignature == metadata.SourceSignature);
            CHECK(archive.Metadata.CacheSignature == metadata.CacheSignature);
            CHECK(archive.Metadata.SourceAsset == metadata.SourceAsset);
            CHECK(archive.Metadata.ObjectName == metadata.ObjectName);
            CHECK(archive.PayloadHash == 0xA5BCD1D1065F84B6ULL);
            CHECK(archive.Payload == payload);
        }
    }

    SECTION("Every scalar is little-endian and output is stable")
    {
        ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
        ModelAnimationArchiveTestOffsets offsets = MakeArchiveOffsets(metadata);
        vector<uint8_t> data = WriteModelAnimationArchive(metadata, payload);

        CHECK(WriteModelAnimationArchive(metadata, payload) == data);
        REQUIRE(offsets.Payload + payload.size() == data.size());
        CHECK(std::equal(MODEL_ANIMATION_ARCHIVE_MAGIC.begin(), MODEL_ANIMATION_ARCHIVE_MAGIC.end(), data.begin()));
        CHECK(data[offsets.Schema] == 0x01);
        CHECK(data[offsets.Schema + 1] == 0x00);
        CHECK(data[offsets.Kind] == 0x02);
        CHECK(data[offsets.Kind + 1] == 0x00);
        CHECK(data[offsets.Flags] == 0x00);
        CHECK(data[offsets.Flags + 1] == 0x00);
        CHECK(data[offsets.Flags + 2] == 0x00);
        CHECK(data[offsets.Flags + 3] == 0x00);

        for (size_t i = 0; i < sizeof(uint64_t); i++) {
            CHECK(data[offsets.RigSignature + i] == i + 1);
            CHECK(data[offsets.SourceSignature + i] == i + 0x11);
            CHECK(data[offsets.CacheSignature + i] == i + 0x21);
        }

        CHECK(data[offsets.RevisionLength] == MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION.size());
        CHECK(data[offsets.RevisionLength + 1] == 0x00);
        CHECK(std::equal(MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION.begin(), MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION.end(), data.begin() + offsets.Revision));
        CHECK(data[offsets.SourceLength] == metadata.SourceAsset.size());
        CHECK(std::equal(metadata.SourceAsset.begin(), metadata.SourceAsset.end(), data.begin() + offsets.Source));
        CHECK(data[offsets.ObjectLength] == metadata.ObjectName.size());
        CHECK(std::equal(metadata.ObjectName.begin(), metadata.ObjectName.end(), data.begin() + offsets.Object));
        CHECK(data[offsets.PayloadLength] == payload.size());

        constexpr array<uint8_t, 8> expected_hash_bytes {0xB6, 0x84, 0x5F, 0x06, 0xD1, 0xD1, 0xBC, 0xA5};
        CHECK(std::equal(expected_hash_bytes.begin(), expected_hash_bytes.end(), data.begin() + offsets.PayloadHash));
        CHECK(std::equal(payload.begin(), payload.end(), data.begin() + offsets.Payload));
    }
}

TEST_CASE("ModelAnimationArchive rejects invalid write metadata")
{
    vector<uint8_t> payload {1};

    SECTION("Unknown kind")
    {
        ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
        metadata.Kind = static_cast<ModelAnimationArchiveKind>(0);
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
    }

    SECTION("Unsupported flags")
    {
        ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
        metadata.Flags = 1;
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
    }

    SECTION("Missing signatures")
    {
        ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
        metadata.RigSignature = 0;
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
        metadata = MakeArchiveMetadata();
        metadata.SourceSignature = 0;
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
        metadata = MakeArchiveMetadata();
        metadata.CacheSignature = 0;
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
    }

    SECTION("Missing source identity")
    {
        ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
        metadata.SourceAsset.clear();
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
        metadata = MakeArchiveMetadata();
        metadata.ObjectName.clear();
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
    }

    SECTION("Invalid UTF-8 source identity")
    {
        ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
        metadata.SourceAsset = string(1, static_cast<char>(0x80));
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
        metadata = MakeArchiveMetadata();
        metadata.ObjectName = string(1, static_cast<char>(0x80));
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
    }

    SECTION("Embedded NUL source identity")
    {
        ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
        metadata.SourceAsset = string("actors/human\0hidden.fbx", 23);
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
        metadata = MakeArchiveMetadata();
        metadata.ObjectName = string("Run\0Hidden", 10);
        CHECK_THROWS_AS(WriteModelAnimationArchive(metadata, payload), ModelAnimationArchiveException);
    }

    SECTION("Empty payload")
    {
        CHECK_THROWS_AS(WriteModelAnimationArchive(MakeArchiveMetadata(), {}), ModelAnimationArchiveException);
    }
}

TEST_CASE("ModelAnimationArchive validates every metadata field")
{
    ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
    vector<uint8_t> payload {0x10, 0x20, 0x30};
    vector<uint8_t> valid_data = WriteModelAnimationArchive(metadata, payload);
    ModelAnimationArchiveTestOffsets offsets = MakeArchiveOffsets(metadata);

    SECTION("Magic")
    {
        vector<uint8_t> data = valid_data;
        data[0] ^= 0xFF;
        CHECK(ErrorContains(data, metadata, "magic"));
    }

    SECTION("Schema")
    {
        vector<uint8_t> data = valid_data;
        OverwriteLittleEndian<uint16_t>(data, offsets.Schema, MODEL_ANIMATION_ARCHIVE_SCHEMA_VERSION + 1);
        CHECK(ErrorContains(data, metadata, "schema"));
    }

    SECTION("Kind")
    {
        vector<uint8_t> data = valid_data;
        OverwriteLittleEndian<uint16_t>(data, offsets.Kind, 0);
        CHECK(ErrorContains(data, metadata, "payload kind"));
        data = valid_data;
        OverwriteLittleEndian<uint16_t>(data, offsets.Kind, static_cast<uint16_t>(ModelAnimationArchiveKind::Skeleton));
        CHECK(ErrorContains(data, metadata, "payload kind mismatch"));
    }

    SECTION("Flags")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.Flags] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "unsupported flags"));
    }

    SECTION("Rig signature")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.RigSignature] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "rig signature mismatch"));
    }

    SECTION("Source signature")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.SourceSignature] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "source signature mismatch"));
    }

    SECTION("Cache signature")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.CacheSignature] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "cache signature mismatch"));
    }

    SECTION("Pinned ozz revision")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.Revision] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "revision mismatch"));
    }

    SECTION("Source asset")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.Source] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "source asset mismatch"));
    }

    SECTION("Object name")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.Object] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "object name mismatch"));
    }

    SECTION("String lengths")
    {
        vector<uint8_t> data = valid_data;
        OverwriteLittleEndian<uint32_t>(data, offsets.RevisionLength, static_cast<uint32_t>(MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION.size() - 1));
        CHECK(ErrorContains(data, metadata, "revision mismatch"));
        data = valid_data;
        OverwriteLittleEndian<uint32_t>(data, offsets.SourceLength, static_cast<uint32_t>(metadata.SourceAsset.size() - 1));
        CHECK(ErrorContains(data, metadata, "source asset mismatch"));
        data = valid_data;
        OverwriteLittleEndian<uint32_t>(data, offsets.ObjectLength, static_cast<uint32_t>(metadata.ObjectName.size() - 1));
        CHECK(ErrorContains(data, metadata, "object name mismatch"));
    }

    SECTION("Maximum declared string lengths")
    {
        vector<uint8_t> data = valid_data;
        OverwriteLittleEndian<uint32_t>(data, offsets.RevisionLength, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(ReadModelAnimationArchive(data, metadata), ModelAnimationArchiveException);
        data = valid_data;
        OverwriteLittleEndian<uint32_t>(data, offsets.SourceLength, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(ReadModelAnimationArchive(data, metadata), ModelAnimationArchiveException);
        data = valid_data;
        OverwriteLittleEndian<uint32_t>(data, offsets.ObjectLength, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(ReadModelAnimationArchive(data, metadata), ModelAnimationArchiveException);
    }
}

TEST_CASE("ModelAnimationArchive rejects every payload corruption boundary")
{
    ModelAnimationArchiveMetadata metadata = MakeArchiveMetadata();
    vector<uint8_t> payload {0x10, 0x20, 0x30, 0x40};
    vector<uint8_t> valid_data = WriteModelAnimationArchive(metadata, payload);
    ModelAnimationArchiveTestOffsets offsets = MakeArchiveOffsets(metadata);

    SECTION("Every truncated prefix")
    {
        for (size_t cut_size = 0; cut_size < valid_data.size(); cut_size++) {
            CAPTURE(cut_size);
            vector<uint8_t> truncated(valid_data.begin(), valid_data.begin() + cut_size);
            CHECK_THROWS_AS(ReadModelAnimationArchive(truncated, metadata), ModelAnimationArchiveException);
        }
    }

    SECTION("Trailing bytes")
    {
        vector<uint8_t> data = valid_data;
        data.emplace_back(0xEE);
        CHECK(ErrorContains(data, metadata, "trailing byte"));
    }

    SECTION("Declared payload is too large")
    {
        vector<uint8_t> data = valid_data;
        OverwriteLittleEndian<uint64_t>(data, offsets.PayloadLength, payload.size() + 1);
        CHECK(ErrorContains(data, metadata, "Truncated"));
    }

    SECTION("Maximum declared payload length")
    {
        vector<uint8_t> data = valid_data;
        OverwriteLittleEndian<uint64_t>(data, offsets.PayloadLength, std::numeric_limits<uint64_t>::max());
        CHECK_THROWS_AS(ReadModelAnimationArchive(data, metadata), ModelAnimationArchiveException);
    }

    SECTION("Declared payload is too small")
    {
        vector<uint8_t> data = valid_data;
        OverwriteLittleEndian<uint64_t>(data, offsets.PayloadLength, payload.size() - 1);
        CHECK(ErrorContains(data, metadata, "trailing byte"));
    }

    SECTION("Empty payload")
    {
        vector<uint8_t> data = valid_data;
        OverwriteLittleEndian<uint64_t>(data, offsets.PayloadLength, 0);
        data.resize(offsets.Payload);
        CHECK(ErrorContains(data, metadata, "empty payload"));
    }

    SECTION("Payload hash")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.PayloadHash] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "payload hash mismatch"));
    }

    SECTION("Payload bytes")
    {
        vector<uint8_t> data = valid_data;
        data[offsets.Payload] ^= 0x01;
        CHECK(ErrorContains(data, metadata, "payload hash mismatch"));
    }

    SECTION("Truncation diagnostics include asset context")
    {
        vector<uint8_t> data(valid_data.begin(), valid_data.end() - 1);
        string error = GetReadError(data, metadata);
        CHECK(error.find("Truncated") != string::npos);
        CHECK(error.find("actors/human.fbx#Run") != string::npos);
    }
}

static auto MakeRigArchiveData(ModelAnimationArchiveKind kind, string_view source_asset, string_view object_name, uint64_t source_signature, uint8_t payload_value) -> ModelAnimationRigArchiveData
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationRigArchiveData result;
    result.Metadata.Kind = kind;
    result.Metadata.Flags = MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS;
    result.Metadata.RigSignature = 0x1020304050607080ULL;
    result.Metadata.SourceSignature = source_signature;
    result.Metadata.CacheSignature = 0x8877665544332211ULL;
    result.Metadata.SourceAsset = source_asset;
    result.Metadata.ObjectName = object_name;
    result.Payload = {payload_value, static_cast<uint8_t>(payload_value + 1)};
    return result;
}

static auto MakeRigData() -> ModelAnimationRigData
{
    FO_STACK_TRACE_ENTRY();

    ModelAnimationRigData result;
    result.RigSignature = 0x1020304050607080ULL;
    result.CacheSignature = 0x8877665544332211ULL;
    result.Skeleton = MakeRigArchiveData(ModelAnimationArchiveKind::Skeleton, "Models/Test.fo3d", "CanonicalSkeleton", 1, 0x10);
    result.BaseJointRemap = MakeRigArchiveData(ModelAnimationArchiveKind::JointRemap, "Models/Base.fbx", "BaseJointRemap", 2, 0x20);

    ModelAnimationRigClipData& first = result.Clips.emplace_back();
    first.Animation = MakeRigArchiveData(ModelAnimationArchiveKind::Animation, "Models/A.fbx", "Idle", 3, 0x30);
    first.JointRemap = MakeRigArchiveData(ModelAnimationArchiveKind::JointRemap, "Models/A.fbx", "Idle:JointRemap", 4, 0x40);

    ModelAnimationRigClipData& second = result.Clips.emplace_back();
    second.Animation = MakeRigArchiveData(ModelAnimationArchiveKind::Animation, "Models/B.fbx", "Run", 5, 0x50);
    second.JointRemap = MakeRigArchiveData(ModelAnimationArchiveKind::JointRemap, "Models/B.fbx", "Run:JointRemap", 6, 0x60);
    result.Bindings = {
        ModelAnimationRigBinding {0, 1, 0, false},
        ModelAnimationRigBinding {0, 2, 1, true},
    };
    return result;
}

TEST_CASE("ModelAnimationRigData joint remap round-trip and validation")
{
    ModelAnimationJointRemap remap;
    remap.Duration = 1.0f;
    remap.CanonicalJointCount = 3;
    remap.SourceToCanonicalJointIndices = {0, 2};
    remap.CanonicalJointPresent = {1, 0, 1};
    remap.NearestSampleTimes = {0.0f, 0.5f, 1.0f};

    vector<uint8_t> data = WriteModelAnimationJointRemapPayload(remap, "test remap");
    ModelAnimationJointRemap loaded = ReadModelAnimationJointRemapPayload(data, "test remap");
    CHECK(loaded.Duration == remap.Duration);
    CHECK(loaded.CanonicalJointCount == remap.CanonicalJointCount);
    CHECK(loaded.SourceToCanonicalJointIndices == remap.SourceToCanonicalJointIndices);
    CHECK(loaded.CanonicalJointPresent == remap.CanonicalJointPresent);
    CHECK(loaded.NearestSampleTimes == remap.NearestSampleTimes);
    CHECK(std::equal(MODEL_ANIMATION_JOINT_REMAP_MAGIC.begin(), MODEL_ANIMATION_JOINT_REMAP_MAGIC.end(), data.begin()));
    CHECK(data[MODEL_ANIMATION_JOINT_REMAP_MAGIC.size()] == MODEL_ANIMATION_JOINT_REMAP_SCHEMA_VERSION);

    SECTION("Invalid presence mask")
    {
        ModelAnimationJointRemap invalid = remap;
        invalid.CanonicalJointPresent[1] = 2;
        CHECK_THROWS_AS(WriteModelAnimationJointRemapPayload(invalid, "invalid presence"), ModelAnimationRigDataException);
    }

    SECTION("Duplicate mapped joint")
    {
        ModelAnimationJointRemap invalid = remap;
        invalid.SourceToCanonicalJointIndices[1] = 0;
        CHECK_THROWS_AS(WriteModelAnimationJointRemapPayload(invalid, "duplicate mapping"), ModelAnimationRigDataException);
    }

    SECTION("Invalid nearest timeline")
    {
        ModelAnimationJointRemap invalid = remap;
        invalid.NearestSampleTimes[1] = 1.0f;
        CHECK_THROWS_AS(WriteModelAnimationJointRemapPayload(invalid, "invalid nearest timeline"), ModelAnimationRigDataException);
    }

    SECTION("Reciprocal-overflow duration")
    {
        ModelAnimationJointRemap invalid = remap;
        invalid.Duration = std::numeric_limits<float32_t>::denorm_min();
        invalid.NearestSampleTimes.clear();
        CHECK_THROWS_AS(WriteModelAnimationJointRemapPayload(invalid, "tiny duration"), ModelAnimationRigDataException);
    }

    SECTION("Every truncated prefix")
    {
        for (size_t cut_size = 0; cut_size < data.size(); cut_size++) {
            CAPTURE(cut_size);
            CHECK_THROWS_AS(ReadModelAnimationJointRemapPayload({data.data(), cut_size}, "truncated remap"), ModelAnimationRigDataException);
        }
    }
}

TEST_CASE("ModelAnimationRigData rig manifest round-trip and corruption rejection")
{
    ModelAnimationRigData source = MakeRigData();
    vector<uint8_t> data = WriteModelAnimationRigData(source, "Models/Test.fo3d");
    ModelAnimationRigData loaded = ReadModelAnimationRigData(data, "Models/Test.fo3d");

    CHECK(loaded.RigSignature == source.RigSignature);
    CHECK(loaded.CacheSignature == source.CacheSignature);
    CHECK(loaded.Skeleton.Metadata.SourceAsset == "Models/Test.fo3d");
    CHECK(loaded.BaseJointRemap.Metadata.SourceAsset == "Models/Base.fbx");
    REQUIRE(loaded.Clips.size() == 2);
    CHECK(loaded.Clips[0].Animation.Metadata.ObjectName == "Idle");
    CHECK(loaded.Clips[1].Animation.Metadata.ObjectName == "Run");
    REQUIRE(loaded.Bindings.size() == source.Bindings.size());
    for (size_t i = 0; i < loaded.Bindings.size(); i++) {
        CHECK(loaded.Bindings[i].StateAnim == source.Bindings[i].StateAnim);
        CHECK(loaded.Bindings[i].ActionAnim == source.Bindings[i].ActionAnim);
        CHECK(loaded.Bindings[i].ClipIndex == source.Bindings[i].ClipIndex);
        CHECK(loaded.Bindings[i].Reversed == source.Bindings[i].Reversed);
    }
    CHECK(WriteModelAnimationRigData(loaded, "Models/Test.fo3d") == data);
    CHECK(std::equal(MODEL_ANIMATION_RIG_DATA_MAGIC.begin(), MODEL_ANIMATION_RIG_DATA_MAGIC.end(), data.begin()));
    CHECK(data[8] == 0x01);
    CHECK(data[9] == 0x00);
    CHECK(data[10] == 0x00);
    CHECK(data[11] == 0x00);
    CHECK(data[28] == 0x02);
    CHECK(data[32] == 0x02);

    SECTION("Every truncated prefix")
    {
        for (size_t cut_size = 0; cut_size < data.size(); cut_size++) {
            CAPTURE(cut_size);
            CHECK_THROWS_AS(ReadModelAnimationRigData({data.data(), cut_size}, "truncated rig"), ModelAnimationRigDataException);
        }
    }

    SECTION("Header corruption")
    {
        vector<uint8_t> invalid = data;
        invalid[0] ^= 0x01;
        CHECK_THROWS_AS(ReadModelAnimationRigData(invalid, "bad magic"), ModelAnimationRigDataException);
        invalid = data;
        invalid[8] = 2;
        CHECK_THROWS_AS(ReadModelAnimationRigData(invalid, "bad schema"), ModelAnimationRigDataException);
        invalid = data;
        invalid[10] = 1;
        CHECK_THROWS_AS(ReadModelAnimationRigData(invalid, "bad flags"), ModelAnimationRigDataException);
    }

    SECTION("Count bombs are rejected before reserve")
    {
        vector<uint8_t> invalid = data;
        OverwriteLittleEndian<uint32_t>(invalid, 28, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(ReadModelAnimationRigData(invalid, "clip count bomb"), ModelAnimationRigDataException);
        invalid = data;
        OverwriteLittleEndian<uint32_t>(invalid, 32, std::numeric_limits<uint32_t>::max());
        CHECK_THROWS_AS(ReadModelAnimationRigData(invalid, "binding count bomb"), ModelAnimationRigDataException);
    }

    SECTION("Manifest metadata is independent from the nested archive")
    {
        vector<uint8_t> invalid = data;
        OverwriteLittleEndian<uint64_t>(invalid, 36, source.Skeleton.Metadata.SourceSignature + 1);
        CHECK_THROWS_AS(ReadModelAnimationRigData(invalid, "manifest mismatch"), ModelAnimationRigDataException);
    }

    SECTION("Trailing bytes")
    {
        vector<uint8_t> invalid = data;
        invalid.emplace_back(0xEE);
        CHECK_THROWS_AS(ReadModelAnimationRigData(invalid, "trailing data"), ModelAnimationRigDataException);
    }

    SECTION("Duplicate or unsorted clips")
    {
        ModelAnimationRigData invalid = MakeRigData();
        std::swap(invalid.Clips[0], invalid.Clips[1]);
        CHECK_THROWS_AS(WriteModelAnimationRigData(invalid, "unsorted clips"), ModelAnimationRigDataException);
        invalid = MakeRigData();
        invalid.Clips[1].Animation.Metadata.SourceAsset = invalid.Clips[0].Animation.Metadata.SourceAsset;
        invalid.Clips[1].Animation.Metadata.ObjectName = invalid.Clips[0].Animation.Metadata.ObjectName;
        invalid.Clips[1].JointRemap.Metadata.SourceAsset = invalid.Clips[0].JointRemap.Metadata.SourceAsset;
        invalid.Clips[1].JointRemap.Metadata.ObjectName = invalid.Clips[0].JointRemap.Metadata.ObjectName;
        CHECK_THROWS_AS(WriteModelAnimationRigData(invalid, "duplicate clips"), ModelAnimationRigDataException);
    }

    SECTION("Invalid bindings")
    {
        ModelAnimationRigData invalid = MakeRigData();
        invalid.Bindings[1] = invalid.Bindings[0];
        CHECK_THROWS_AS(WriteModelAnimationRigData(invalid, "duplicate bindings"), ModelAnimationRigDataException);
        invalid = MakeRigData();
        invalid.Bindings[1].ClipIndex = 9;
        CHECK_THROWS_AS(WriteModelAnimationRigData(invalid, "bad clip index"), ModelAnimationRigDataException);
        invalid = MakeRigData();
        invalid.Bindings.pop_back();
        CHECK_THROWS_AS(WriteModelAnimationRigData(invalid, "unreferenced clip"), ModelAnimationRigDataException);
    }

    SECTION("Embedded NUL identity")
    {
        ModelAnimationRigData invalid = MakeRigData();
        invalid.Skeleton.Metadata.SourceAsset = string("Models/Test\0Hidden.fo3d", 23);
        CHECK_THROWS_AS(WriteModelAnimationRigData(invalid, "embedded nul"), ModelAnimationRigDataException);
    }
}

FO_END_NAMESPACE

#endif
