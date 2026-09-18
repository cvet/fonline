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

#include "Common.h"

#if FO_MANAGED_SCRIPTING

FO_BEGIN_NAMESPACE

class EngineMetadata;

constexpr int32_t MANAGED_ABI_GENERATOR_IDENTITY = 4;
constexpr int32_t MANAGED_ABI_SCALAR_FRAME_CAPACITY = 256;
constexpr size_t MANAGED_ABI_PROPERTY_ADAPTER_STORAGE = 32;
// An entity or native ref-type handle travels as a zero-extended 64-bit slot on every pointer width
constexpr uint16_t MANAGED_ABI_HANDLE_SLOT_SIZE = 8;

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
    uint16_t Size {};
    uint16_t Offset {};
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

[[nodiscard]] auto HasManagedAbiHashedStringField(const BaseTypeDesc& type) noexcept -> bool;
[[nodiscard]] auto IsManagedAbiBlittableStruct(const BaseTypeDesc& type) noexcept -> bool;
[[nodiscard]] auto IsManagedAbiScalarType(const ComplexTypeDesc& type) noexcept -> bool;
[[nodiscard]] auto IsManagedAbiFixedValueType(const ComplexTypeDesc& type) noexcept -> bool;
[[nodiscard]] auto ManagedAbiSlotSize(const BaseTypeDesc& type) noexcept -> uint16_t;
[[nodiscard]] auto ManagedAbiKindFromBaseType(const BaseTypeDesc& type) noexcept -> ManagedAbiValueKind;
[[nodiscard]] auto ManagedAbiKindFromTypeName(string_view type_name) noexcept -> ManagedAbiValueKind;
[[nodiscard]] auto IsManagedAbiDynamicRefType(const BaseTypeDesc& type) noexcept -> bool;
[[nodiscard]] auto IsManagedAbiHandleType(const ComplexTypeDesc& type) noexcept -> bool;
[[nodiscard]] auto MakeManagedAbiCallbackKey(const ComplexTypeDesc& ret, const_span<ComplexTypeDesc> args) -> string;
[[nodiscard]] auto BuildManagedAbiCallbackLayout(const ComplexTypeDesc& ret, const_span<ComplexTypeDesc> args) -> ManagedAbiCallbackLayout;
[[nodiscard]] auto BuildManagedAbiManifest(const EngineMetadata& meta, string_view target_name) -> ManagedAbiManifest;
[[nodiscard]] auto FindManagedAbiMethod(const ManagedAbiManifest& abi, string_view owner, string_view name, size_t owner_index) -> nptr<const ManagedAbiMethodEntry>;
[[nodiscard]] auto FindManagedAbiEvent(const ManagedAbiManifest& abi, string_view owner, string_view name) -> nptr<const ManagedAbiEventEntry>;
[[nodiscard]] auto FindManagedAbiSetting(const ManagedAbiManifest& abi, string_view name) -> nptr<const ManagedAbiSettingEntry>;
[[nodiscard]] auto FindManagedAbiInnerEntry(const ManagedAbiManifest& abi, string_view owner, string_view entry_name) -> nptr<const ManagedAbiInnerEntry>;

FO_END_NAMESPACE

#endif
