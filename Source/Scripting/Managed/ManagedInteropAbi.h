// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_MANAGED_SCRIPTING

FO_BEGIN_NAMESPACE

class EngineMetadata;

constexpr int32_t MANAGED_ABI_GENERATOR_IDENTITY {9};
constexpr int32_t MANAGED_ABI_SCALAR_FRAME_CAPACITY {256};
constexpr size_t MANAGED_ABI_PROPERTY_ADAPTER_STORAGE {32};
// An entity or native ref-type handle travels as a zero-extended 64-bit slot on every pointer width
constexpr uint16_t MANAGED_ABI_HANDLE_SLOT_SIZE {8};

enum class ManagedAbiValueKind : uint8_t
{
    Unsupported = 0,
    Bool,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float32,
    Float64,
    Enum,
    Struct,
    HashedString,
    Handle,
};

struct ManagedAbiSlot
{
    ManagedAbiValueKind Kind {};
    bool Mutable {};
    bool Nullable {};
    uint16_t Size {};
    uint16_t Offset {};
};

// Packed managed frames may begin at any byte address; native callees require aligned, typed argument storage
struct ManagedAbiNativeFrame
{
    span<uint8_t> PackedFrame {};
    const_span<ManagedAbiSlot> Args {};
    ManagedAbiSlot Ret {};
    array<size_t, MAX_CALL_ARGS> ArgOffsets {};
    size_t ResultOffset {};
    union
    {
        array<uint8_t, MANAGED_ABI_SCALAR_FRAME_CAPACITY + (MAX_CALL_ARGS + 1) * alignof(std::max_align_t)> Storage {};
        std::max_align_t StorageAlignment;
    };
};

struct ManagedAbiMethodEntry
{
    int32_t Id {};
    string Owner {};
    string Name {};
    size_t OwnerIndex {};
    bool IsRefType {};
    bool IsFactory {};
    bool PassOwnership {};
    bool UsesScalarFrame {};
    vector<ManagedAbiSlot> Args {};
    ManagedAbiSlot Ret {};
    uint16_t FrameSize {};
    uint16_t ResultOffset {};
};

struct ManagedAbiEventEntry
{
    int32_t Id {};
    string Owner {};
    string Name {};
    bool IsGlobal {};
    bool UsesScalarFrame {};
    vector<ManagedAbiSlot> Args {};
    uint16_t FrameSize {};
};

struct ManagedAbiSettingEntry
{
    int32_t Id {};
    string Name {};
    ManagedAbiValueKind Kind {};
    bool UsesTypedBridge {};
};

struct ManagedAbiInnerEntry
{
    int32_t Id {};
    string Owner {};
    string EntryName {};
    string TargetType {};
};

struct ManagedAbiManifest
{
    uint64_t Hash {};
    int32_t GeneratorIdentity {MANAGED_ABI_GENERATOR_IDENTITY};
    vector<ManagedAbiMethodEntry> Methods {};
    vector<ManagedAbiEventEntry> Events {};
    vector<ManagedAbiSettingEntry> Settings {};
    vector<ManagedAbiInnerEntry> InnerEntries {};
};

// Frame of a native-to-managed callback: handles and fixed values inline, a fixed-value result after the arguments.
// Not part of the manifest hash: the layout is derived by the baker and the backend from the same callback signature
struct ManagedAbiCallbackLayout
{
    bool Supported {};
    vector<ManagedAbiSlot> Args {};
    ManagedAbiSlot Ret {};
    uint16_t FrameSize {};
    uint16_t ResultOffset {};
};

auto BuildManagedAbiNativeFrame(span<uint8_t> frame, const_span<ManagedAbiSlot> args, ManagedAbiSlot ret = {}) -> ManagedAbiNativeFrame;
auto GetManagedAbiNativeFrameArg(ManagedAbiNativeFrame& frame, size_t index) -> ptr<void>;
auto GetManagedAbiNativeFrameResult(ManagedAbiNativeFrame& frame) -> nptr<void>;
void CopyBackManagedAbiNativeFrame(const ManagedAbiNativeFrame& frame);
auto IsManagedAbiScalarType(const ComplexTypeDesc& type) noexcept -> bool;
auto IsManagedAbiFixedValueType(const ComplexTypeDesc& type) noexcept -> bool;
auto IsManagedAbiFixedPropertyValue(const BaseTypeDesc& type) noexcept -> bool;
auto ManagedAbiSlotSize(const BaseTypeDesc& type) noexcept -> uint16_t;
auto ManagedAbiKindFromBaseType(const BaseTypeDesc& type) noexcept -> ManagedAbiValueKind;
auto ManagedAbiKindFromTypeName(string_view type_name) noexcept -> ManagedAbiValueKind;
auto IsManagedAbiDynamicRefType(const BaseTypeDesc& type) noexcept -> bool;
auto IsManagedAbiHandleType(const ComplexTypeDesc& type, bool wrapped) noexcept -> bool;
auto CollectManagedAbiWrapperClasses(const EngineMetadata& meta) -> vector<string>;
auto MakeManagedAbiCallbackKey(const ComplexTypeDesc& ret, const_span<ComplexTypeDesc> args) -> string;
auto BuildManagedAbiCallbackLayout(const ComplexTypeDesc& ret, const_span<ComplexTypeDesc> args) -> ManagedAbiCallbackLayout;
auto BuildManagedAbiManifest(const EngineMetadata& meta, string_view target_name) -> ManagedAbiManifest;
auto FindManagedAbiMethod(const ManagedAbiManifest& abi, string_view owner, string_view name, size_t owner_index) -> nptr<const ManagedAbiMethodEntry>;
auto FindManagedAbiEvent(const ManagedAbiManifest& abi, string_view owner, string_view name) -> nptr<const ManagedAbiEventEntry>;
auto FindManagedAbiSetting(const ManagedAbiManifest& abi, string_view name) -> nptr<const ManagedAbiSettingEntry>;
auto FindManagedAbiInnerEntry(const ManagedAbiManifest& abi, string_view owner, string_view entry_name) -> nptr<const ManagedAbiInnerEntry>;

FO_END_NAMESPACE

#endif
