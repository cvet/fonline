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

#pragma once

#include "Common.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ModelAnimationArchiveException);

// Stable schema-1 wire identifier; preserve its bytes for baked-resource compatibility.
inline constexpr array<uint8_t, 8> MODEL_ANIMATION_ARCHIVE_MAGIC {'L', 'F', 'O', 'Z', 'Z', 'A', 'R', 'C'};
inline constexpr uint16_t MODEL_ANIMATION_ARCHIVE_SCHEMA_VERSION = 1;
inline constexpr uint32_t MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS = 0;
inline constexpr string_view MODEL_ANIMATION_ARCHIVE_PAYLOAD_REVISION = "6cbdc790123aa4731d82e255df187b3a8a808256";

enum class ModelAnimationArchiveKind : uint16_t
{
    Skeleton = 1,
    Animation = 2,
    JointRemap = 3,
};

struct ModelAnimationArchiveMetadata
{
    ModelAnimationArchiveKind Kind {};
    uint32_t Flags {}; // Reserved in schema 1; must contain only MODEL_ANIMATION_ARCHIVE_SUPPORTED_FLAGS.
    uint64_t RigSignature {}; // Caller-owned deterministic canonical-rig content signature.
    uint64_t SourceSignature {}; // Caller-owned deterministic payload-source content signature.
    uint64_t CacheSignature {}; // Caller-owned deterministic converter-policy/settings signature.
    string SourceAsset {};
    string ObjectName {};
};

struct ModelAnimationArchive
{
    uint16_t SchemaVersion {};
    string PayloadRevision {};
    ModelAnimationArchiveMetadata Metadata {};
    uint64_t PayloadHash {};
    vector<uint8_t> Payload {};
};

// Wire order: magic[8], schema:u16, kind:u16, flags:u32, rig/source/cache signatures:u64,
// revision/source/object as u32 byte length + validated UTF-8 bytes, payload length:u64, FNV-1a hash:u64,
// then the opaque payload. Every integer is encoded field-by-field in little-endian order.
[[nodiscard]] auto WriteModelAnimationArchive(const ModelAnimationArchiveMetadata& metadata, const_span<uint8_t> payload) -> vector<uint8_t>;
[[nodiscard]] auto ReadModelAnimationArchive(const_span<uint8_t> data, const ModelAnimationArchiveMetadata& expected_metadata) -> ModelAnimationArchive;

// Installs the engine allocation policy for the private model-animation codec.
// Call before constructing, loading, or baking any codec-owned object.
void InitializeModelAnimationMemory() noexcept;

FO_DECLARE_EXCEPTION(ModelAnimationRigDataException);

inline constexpr array<uint8_t, 8> MODEL_DESCRIPTION_MAGIC {'L', 'F', 'M', 'O', 'D', 'I', 'N', 'F'};
inline constexpr uint16_t MODEL_DESCRIPTION_SCHEMA_VERSION = 1;
inline constexpr uint16_t MODEL_DESCRIPTION_SUPPORTED_FLAGS = 0;

// Stable schema-1 wire identifiers; preserve their bytes for baked-resource compatibility.
inline constexpr array<uint8_t, 8> MODEL_ANIMATION_RIG_DATA_MAGIC {'L', 'F', 'O', 'Z', 'Z', 'R', 'I', 'G'};
inline constexpr uint16_t MODEL_ANIMATION_RIG_DATA_SCHEMA_VERSION = 1;
inline constexpr uint16_t MODEL_ANIMATION_RIG_DATA_SUPPORTED_FLAGS = 0;
inline constexpr uint32_t MODEL_ANIMATION_RIG_MAX_JOINTS = 1024;
inline constexpr uint32_t MODEL_ANIMATION_RIG_MAX_CLIPS = std::numeric_limits<uint16_t>::max();
inline constexpr uint32_t MODEL_ANIMATION_RIG_MAX_BINDINGS = std::numeric_limits<uint16_t>::max();
inline constexpr float32_t MODEL_ANIMATION_RIG_MATRIX_ABSOLUTE_TOLERANCE = 1.0e-4f;
inline constexpr float32_t MODEL_ANIMATION_RIG_MATRIX_RELATIVE_TOLERANCE = 1.0e-4f;

inline constexpr array<uint8_t, 8> MODEL_ANIMATION_JOINT_REMAP_MAGIC {'L', 'F', 'O', 'Z', 'Z', 'J', 'R', 'M'};
inline constexpr uint16_t MODEL_ANIMATION_JOINT_REMAP_SCHEMA_VERSION = 1;

struct ModelAnimationJointRemap
{
    float32_t Duration {};
    uint32_t CanonicalJointCount {};
    vector<uint32_t> SourceToCanonicalJointIndices {};
    vector<uint8_t> CanonicalJointPresent {};
    vector<float32_t> NearestSampleTimes {};
};

struct ModelAnimationRigArchiveData
{
    ModelAnimationArchiveMetadata Metadata {};
    vector<uint8_t> Payload {};
};

struct ModelAnimationRigClipData
{
    ModelAnimationRigArchiveData Animation {};
    ModelAnimationRigArchiveData JointRemap {};
};

struct ModelAnimationRigBinding
{
    int32_t StateAnim {};
    int32_t ActionAnim {};
    uint32_t ClipIndex {};
    bool Reversed {};
};

struct ModelAnimationRigData
{
    uint64_t RigSignature {};
    uint64_t CacheSignature {};
    ModelAnimationRigArchiveData Skeleton {};
    ModelAnimationRigArchiveData BaseJointRemap {};
    vector<ModelAnimationRigClipData> Clips {};
    vector<ModelAnimationRigBinding> Bindings {};
};

// Joint-remap wire order: magic[8], schema:u16, reserved:u16, duration:f32 bits,
// canonical/source/nearest counts:u32, source-to-canonical indices:u32[],
// canonical presence bytes, then nearest sample times as f32 bits.
[[nodiscard]] auto WriteModelAnimationJointRemapPayload(const ModelAnimationJointRemap& remap, string_view context) -> vector<uint8_t>;
[[nodiscard]] auto ReadModelAnimationJointRemapPayload(const_span<uint8_t> payload, string_view context) -> ModelAnimationJointRemap;
void ValidateModelAnimationJointRemap(const ModelAnimationJointRemap& remap, string_view context);

// Rig-data wire order: magic[8], schema:u16, flags:u16, rig/cache signatures:u64,
// clip/binding counts:u32, skeleton and base-remap archive manifests, sorted clip
// archive manifests, then sorted state/action bindings. An archive manifest stores
// caller-owned source signature and identities before its length-prefixed LF archive.
[[nodiscard]] auto WriteModelAnimationRigData(const ModelAnimationRigData& rig, string_view context) -> vector<uint8_t>;
[[nodiscard]] auto ReadModelAnimationRigData(const_span<uint8_t> data, string_view context) -> ModelAnimationRigData;

FO_END_NAMESPACE

#endif
