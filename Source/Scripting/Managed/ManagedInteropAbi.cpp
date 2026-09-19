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

#include "ManagedInteropAbi.h"

#if FO_MANAGED_SCRIPTING

#include "EngineBase.h"
#include "Entity.h"

FO_BEGIN_NAMESPACE

auto GetServerSettingsTyped() -> vector<pair<string, string>>;
auto GetClientSettingsTyped() -> vector<pair<string, string>>;
auto GetMapperSettingsTyped() -> vector<pair<string, string>>;

static void MixHash(uint64_t& hash, string_view text) noexcept;
static void MixHash(uint64_t& hash, uint64_t value) noexcept;

static void MixHash(uint64_t& hash, uint64_t value) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t part = hashing_ex::hash(&value, sizeof(value));
    hash ^= part + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
}

static void MixHash(uint64_t& hash, string_view text) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (text.empty()) {
        MixHash(hash, uint64_t {0});
        return;
    }

    uint64_t part = hashing_ex::hash(text.data(), text.size());
    hash ^= part + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
}

static auto MakeSortedEntityTypes(const map<hstring, EntityTypeDesc>& types) -> vector<pair<string, const EntityTypeDesc*>>
{
    FO_STACK_TRACE_ENTRY();

    vector<pair<string, const EntityTypeDesc*>> result;
    result.reserve(types.size());

    for (const auto& [type_name, desc] : types) {
        result.emplace_back(type_name.as_str(), &desc);
    }

    std::ranges::sort(result, {}, [](const auto& entry) -> const string& { return entry.first; });
    return result;
}

static auto MakeSortedRefTypes(const EngineMetadata& meta) -> vector<pair<string, const RefTypeDesc*>>
{
    FO_STACK_TRACE_ENTRY();

    vector<pair<string, const RefTypeDesc*>> result;

    for (const auto& type : meta.GetBaseTypes() | std::views::values) {
        if (!type.IsRefType || !type.RefType) {
            continue;
        }

        result.emplace_back(type.Name, type.RefType.get());
    }

    std::ranges::sort(result, {}, [](const auto& entry) -> const string& { return entry.first; });
    return result;
}

static auto CanUseManagedAbiBridge(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Name == "any" || type.IsPrimitive || type.IsString || type.IsHashedString || type.IsEnum || type.IsStruct || type.IsEntity || type.IsFixedType || type.IsRefType;
}

static auto CanUseManagedAbiBridge(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type) {
        return true;
    }
    if (type.Kind == ComplexTypeKind::Simple || type.Kind == ComplexTypeKind::Array) {
        return CanUseManagedAbiBridge(type.BaseType);
    }
    if (type.Kind == ComplexTypeKind::Dict) {
        return type.KeyType.has_value() && CanUseManagedAbiBridge(*type.KeyType) && CanUseManagedAbiBridge(type.BaseType);
    }
    if (type.Kind == ComplexTypeKind::Callback && type.CallbackArgs) {
        return std::ranges::all_of(*type.CallbackArgs, [](const ComplexTypeDesc& callback_arg) { return CanUseManagedAbiBridge(callback_arg); });
    }

    return false;
}

static auto IsManagedAbiBridgeMethod(const MethodDesc& method) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!CanUseManagedAbiBridge(method.Ret)) {
        return false;
    }

    return std::ranges::all_of(method.Args, [](const ArgDesc& arg) { return CanUseManagedAbiBridge(arg.Type); });
}

auto IsManagedAbiScalarType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && (type.BaseType.IsPrimitive || type.BaseType.IsEnum);
}

auto IsManagedAbiFixedValueType(const ComplexTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.Kind == ComplexTypeKind::Simple && IsManagedAbiFixedPropertyValue(type.BaseType);
}

// A value that crosses as raw bytes. IsStruct is a plain value type by registration (RegisterValueTypeLayout), so
// it needs no layout check of its own here; anything that holds complex data is a ref type instead
auto IsManagedAbiFixedPropertyValue(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsPrimitive || type.IsEnum || type.IsHashedString || type.IsStruct;
}

auto ManagedAbiSlotSize(const BaseTypeDesc& type) noexcept -> uint16_t
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsBool || type.IsInt8 || type.IsUInt8) {
        return 1;
    }
    if (type.IsInt16 || type.IsUInt16) {
        return 2;
    }
    if (type.IsInt32 || type.IsUInt32 || type.IsSingleFloat) {
        return 4;
    }
    if (type.IsInt64 || type.IsUInt64 || type.IsDoubleFloat) {
        return 8;
    }
    if (type.IsEnum) {
        return type.EnumUnderlyingType ? ManagedAbiSlotSize(*type.EnumUnderlyingType) : uint16_t {4};
    }

    // A value wider than the frame is not a layout problem, only one the frame cannot hold; zero keeps it boxed
    return type.Size <= MANAGED_ABI_SCALAR_FRAME_CAPACITY ? numeric_cast<uint16_t>(type.Size) : uint16_t {0};
}

auto ManagedAbiKindFromBaseType(const BaseTypeDesc& type) noexcept -> ManagedAbiValueKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.IsBool) {
        return ManagedAbiValueKind::Bool;
    }
    if (type.IsInt8) {
        return ManagedAbiValueKind::Int8;
    }
    if (type.IsUInt8) {
        return ManagedAbiValueKind::UInt8;
    }
    if (type.IsInt16) {
        return ManagedAbiValueKind::Int16;
    }
    if (type.IsUInt16) {
        return ManagedAbiValueKind::UInt16;
    }
    if (type.IsInt32) {
        return ManagedAbiValueKind::Int32;
    }
    if (type.IsUInt32) {
        return ManagedAbiValueKind::UInt32;
    }
    if (type.IsInt64) {
        return ManagedAbiValueKind::Int64;
    }
    if (type.IsUInt64) {
        return ManagedAbiValueKind::UInt64;
    }
    if (type.IsSingleFloat) {
        return ManagedAbiValueKind::Float32;
    }
    if (type.IsDoubleFloat) {
        return ManagedAbiValueKind::Float64;
    }
    if (type.IsEnum) {
        return ManagedAbiValueKind::Enum;
    }
    if (type.IsHashedString) {
        return ManagedAbiValueKind::HashedString;
    }
    if (type.IsStruct) {
        return ManagedAbiValueKind::Struct;
    }

    return ManagedAbiValueKind::Unsupported;
}

auto ManagedAbiKindFromTypeName(string_view type_name) noexcept -> ManagedAbiValueKind
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type_name == "bool") {
        return ManagedAbiValueKind::Bool;
    }
    if (type_name == "int8") {
        return ManagedAbiValueKind::Int8;
    }
    if (type_name == "uint8") {
        return ManagedAbiValueKind::UInt8;
    }
    if (type_name == "int16") {
        return ManagedAbiValueKind::Int16;
    }
    if (type_name == "uint16") {
        return ManagedAbiValueKind::UInt16;
    }
    if (type_name == "int32") {
        return ManagedAbiValueKind::Int32;
    }
    if (type_name == "uint32") {
        return ManagedAbiValueKind::UInt32;
    }
    if (type_name == "int64") {
        return ManagedAbiValueKind::Int64;
    }
    if (type_name == "uint64") {
        return ManagedAbiValueKind::UInt64;
    }
    if (type_name == "float32") {
        return ManagedAbiValueKind::Float32;
    }
    if (type_name == "float64") {
        return ManagedAbiValueKind::Float64;
    }

    return ManagedAbiValueKind::Unsupported;
}

static auto MakeAbiSlot(const ComplexTypeDesc& type, uint16_t offset) -> ManagedAbiSlot
{
    FO_NO_STACK_TRACE_ENTRY();

    ManagedAbiSlot slot;
    slot.Kind = ManagedAbiKindFromBaseType(type.BaseType);
    slot.Mutable = type.IsMutable;
    slot.Size = IsManagedAbiFixedValueType(type) ? ManagedAbiSlotSize(type.BaseType) : uint16_t {0};
    slot.Offset = offset;
    return slot;
}

// An entity or native ref-type argument travels by value as its pointer in an 8-byte slot, and so does an entity
// result; a ref-type result stays boxed, because its ownership rules live in the boxing path
static auto PackScalarSlots(const_span<ArgDesc> args, bool args_wrapped, const ComplexTypeDesc& ret, bool ret_nullable, vector<ManagedAbiSlot>& out_args, ManagedAbiSlot& out_ret, uint16_t& frame_size, uint16_t& result_offset) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    uint16_t offset = 0;
    out_args.clear();
    out_args.reserve(args.size());

    for (const ArgDesc& arg : args) {
        if (!arg.Type.IsMutable && IsManagedAbiHandleType(arg.Type, args_wrapped)) {
            ManagedAbiSlot handle_slot;
            handle_slot.Kind = ManagedAbiValueKind::Handle;
            handle_slot.Nullable = arg.Nullable;
            handle_slot.Size = MANAGED_ABI_HANDLE_SLOT_SIZE;
            handle_slot.Offset = offset;
            offset = numeric_cast<uint16_t>(offset + handle_slot.Size);
            out_args.emplace_back(handle_slot);
            continue;
        }

        if (!IsManagedAbiFixedValueType(arg.Type)) {
            return false;
        }

        ManagedAbiSlot slot = MakeAbiSlot(arg.Type, offset);

        if (slot.Size == 0) {
            return false;
        }

        offset = numeric_cast<uint16_t>(offset + slot.Size);
        out_args.emplace_back(slot);
    }

    result_offset = offset;
    out_ret = {};

    if (ret && !ret.IsMutable && !ret.BaseType.IsRefType && IsManagedAbiHandleType(ret, true)) {
        out_ret.Kind = ManagedAbiValueKind::Handle;
        out_ret.Nullable = ret_nullable;
        out_ret.Size = MANAGED_ABI_HANDLE_SLOT_SIZE;
        out_ret.Offset = offset;
        offset = numeric_cast<uint16_t>(offset + out_ret.Size);
    }
    else if (ret) {
        if (!IsManagedAbiFixedValueType(ret)) {
            return false;
        }

        out_ret = MakeAbiSlot(ret, offset);

        if (out_ret.Size == 0) {
            return false;
        }

        offset = numeric_cast<uint16_t>(offset + out_ret.Size);
    }

    if (offset > MANAGED_ABI_SCALAR_FRAME_CAPACITY) {
        return false;
    }

    frame_size = offset;
    return true;
}

static void AppendExportSettings(ManagedAbiManifest& abi, string_view target_name)
{
    FO_STACK_TRACE_ENTRY();

    vector<pair<string, string>> export_settings;

    if (target_name == "Server") {
        export_settings = GetServerSettingsTyped();
    }
    else if (target_name == "Client") {
        export_settings = GetClientSettingsTyped();
    }
    else if (target_name == "Mapper") {
        export_settings = GetMapperSettingsTyped();
    }

    for (const auto& [setting_name, type_name] : export_settings) {
        if (FindManagedAbiSetting(abi, setting_name)) {
            continue;
        }

        ManagedAbiSettingEntry entry;
        entry.Name = setting_name;
        entry.Kind = ManagedAbiKindFromTypeName(type_name);
        entry.UsesTypedBridge = entry.Kind != ManagedAbiValueKind::Unsupported;
        abi.Settings.emplace_back(std::move(entry));
    }
}

auto BuildManagedAbiManifest(const EngineMetadata& meta, string_view target_name) -> ManagedAbiManifest
{
    FO_STACK_TRACE_ENTRY();

    ManagedAbiManifest abi;
    abi.GeneratorIdentity = MANAGED_ABI_GENERATOR_IDENTITY;

    auto append_methods = [&](string_view owner, const vector<MethodDesc>& methods, bool is_ref_type) {
        for (size_t method_index = 0; method_index < methods.size(); method_index++) {
            const MethodDesc& method = methods[method_index];

            if (!IsManagedAbiBridgeMethod(method)) {
                continue;
            }

            ManagedAbiMethodEntry entry;
            entry.Id = numeric_cast<int32_t>(abi.Methods.size());
            entry.Owner = string {owner};
            entry.Name = method.Name;
            entry.OwnerIndex = method_index;
            entry.IsRefType = is_ref_type;
            entry.IsFactory = is_ref_type && method.Name == "__Factory";
            entry.PassOwnership = method.PassOwnership;
            // Method arguments only travel into native code, so no argument slot is ever wrapped
            entry.UsesScalarFrame = PackScalarSlots(method.Args, false, method.Ret, method.ReturnNullable, entry.Args, entry.Ret, entry.FrameSize, entry.ResultOffset);
            abi.Methods.emplace_back(std::move(entry));
        }
    };

    for (const auto& [type_name, desc] : MakeSortedEntityTypes(meta.GetEntityTypes())) {
        append_methods(type_name, desc->Methods, false);

        for (const EntityEventDesc& event : desc->Events) {
            ManagedAbiEventEntry entry;
            entry.Id = numeric_cast<int32_t>(abi.Events.size());
            entry.Owner = type_name;
            entry.Name = event.Name;
            entry.IsGlobal = desc->IsGlobal;

            ManagedAbiSlot unused_ret {};
            uint16_t result_offset = 0;
            entry.UsesScalarFrame = PackScalarSlots(event.Args, true, {}, false, entry.Args, unused_ret, entry.FrameSize, result_offset);
            abi.Events.emplace_back(std::move(entry));
        }

        vector<pair<string, const EntityTypeDesc::HolderEntryDesc*>> holder_entries;

        for (const auto& [entry_name, holder_entry] : desc->HolderEntries) {
            holder_entries.emplace_back(entry_name.as_str(), &holder_entry);
        }

        std::ranges::sort(holder_entries, {}, [](const auto& entry) -> const string& { return entry.first; });

        for (const auto& [entry_name, holder_entry] : holder_entries) {
            ManagedAbiInnerEntry entry;
            entry.Id = numeric_cast<int32_t>(abi.InnerEntries.size());
            entry.Owner = type_name;
            entry.EntryName = entry_name;
            entry.TargetType = holder_entry->TargetType.as_str();
            abi.InnerEntries.emplace_back(std::move(entry));
        }
    }

    for (const auto& [type_name, desc] : MakeSortedEntityTypes(meta.GetFixedTypes())) {
        append_methods(type_name, desc->Methods, false);
    }

    for (const auto& [type_name, desc] : MakeSortedRefTypes(meta)) {
        append_methods(type_name, desc->Methods, true);
    }

    map<string, ManagedAbiSettingEntry> settings;

    for (const auto& [setting_name, setting_type] : meta.GetGameSettings()) {
        ManagedAbiSettingEntry entry;
        entry.Name = setting_name;
        entry.Kind = ManagedAbiKindFromBaseType(*setting_type);
        entry.UsesTypedBridge = entry.Kind != ManagedAbiValueKind::Unsupported;
        settings.emplace(setting_name, std::move(entry));
    }

    for (auto& [setting_name, entry] : settings) {
        ignore_unused(setting_name);
        entry.Id = numeric_cast<int32_t>(abi.Settings.size());
        abi.Settings.emplace_back(std::move(entry));
    }

    AppendExportSettings(abi, target_name);

    std::ranges::sort(abi.Settings, {}, [](const ManagedAbiSettingEntry& entry) -> const string& { return entry.Name; });

    for (int32_t i = 0; i < numeric_cast<int32_t>(abi.Settings.size()); i++) {
        abi.Settings[numeric_cast<size_t>(i)].Id = i;
    }

    MixHash(abi.Hash, numeric_cast<uint64_t>(abi.GeneratorIdentity));
    MixHash(abi.Hash, target_name);

    for (const ManagedAbiMethodEntry& method : abi.Methods) {
        MixHash(abi.Hash, "method");
        MixHash(abi.Hash, numeric_cast<uint64_t>(method.Id));
        MixHash(abi.Hash, method.Owner);
        MixHash(abi.Hash, method.Name);
        MixHash(abi.Hash, numeric_cast<uint64_t>(method.OwnerIndex));
        MixHash(abi.Hash, numeric_cast<uint64_t>(method.UsesScalarFrame ? 1 : 0));
        MixHash(abi.Hash, numeric_cast<uint64_t>(method.Args.size()));

        for (const ManagedAbiSlot& slot : method.Args) {
            MixHash(abi.Hash, numeric_cast<uint64_t>(static_cast<uint8_t>(slot.Kind)));
            MixHash(abi.Hash, numeric_cast<uint64_t>(slot.Size));
            MixHash(abi.Hash, numeric_cast<uint64_t>(slot.Offset));
            MixHash(abi.Hash, numeric_cast<uint64_t>(slot.Nullable ? 1 : 0));
        }

        MixHash(abi.Hash, numeric_cast<uint64_t>(static_cast<uint8_t>(method.Ret.Kind)));
        MixHash(abi.Hash, numeric_cast<uint64_t>(method.Ret.Nullable ? 1 : 0));
        MixHash(abi.Hash, numeric_cast<uint64_t>(method.FrameSize));
    }

    for (const ManagedAbiEventEntry& event : abi.Events) {
        MixHash(abi.Hash, "event");
        MixHash(abi.Hash, numeric_cast<uint64_t>(event.Id));
        MixHash(abi.Hash, event.Owner);
        MixHash(abi.Hash, event.Name);
        MixHash(abi.Hash, numeric_cast<uint64_t>(event.UsesScalarFrame ? 1 : 0));
        MixHash(abi.Hash, numeric_cast<uint64_t>(event.Args.size()));

        for (const ManagedAbiSlot& slot : event.Args) {
            MixHash(abi.Hash, numeric_cast<uint64_t>(static_cast<uint8_t>(slot.Kind)));
            MixHash(abi.Hash, numeric_cast<uint64_t>(slot.Size));
            MixHash(abi.Hash, numeric_cast<uint64_t>(slot.Offset));
            MixHash(abi.Hash, numeric_cast<uint64_t>(slot.Nullable ? 1 : 0));
        }

        MixHash(abi.Hash, numeric_cast<uint64_t>(event.FrameSize));
    }

    for (const ManagedAbiSettingEntry& setting : abi.Settings) {
        MixHash(abi.Hash, "setting");
        MixHash(abi.Hash, numeric_cast<uint64_t>(setting.Id));
        MixHash(abi.Hash, setting.Name);
        MixHash(abi.Hash, numeric_cast<uint64_t>(static_cast<uint8_t>(setting.Kind)));
        MixHash(abi.Hash, numeric_cast<uint64_t>(setting.UsesTypedBridge ? 1 : 0));
    }

    for (const ManagedAbiInnerEntry& inner : abi.InnerEntries) {
        MixHash(abi.Hash, "inner");
        MixHash(abi.Hash, numeric_cast<uint64_t>(inner.Id));
        MixHash(abi.Hash, inner.Owner);
        MixHash(abi.Hash, inner.EntryName);
        MixHash(abi.Hash, inner.TargetType);
    }

    return abi;
}

auto FindManagedAbiMethod(const ManagedAbiManifest& abi, string_view owner, string_view name, size_t owner_index) -> nptr<const ManagedAbiMethodEntry>
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const ManagedAbiMethodEntry& method : abi.Methods) {
        if (method.Owner == owner && method.Name == name && method.OwnerIndex == owner_index) {
            return &method;
        }
    }

    return nullptr;
}

auto FindManagedAbiEvent(const ManagedAbiManifest& abi, string_view owner, string_view name) -> nptr<const ManagedAbiEventEntry>
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const ManagedAbiEventEntry& event : abi.Events) {
        if (event.Owner == owner && event.Name == name) {
            return &event;
        }
    }

    return nullptr;
}

auto FindManagedAbiSetting(const ManagedAbiManifest& abi, string_view name) -> nptr<const ManagedAbiSettingEntry>
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const ManagedAbiSettingEntry& setting : abi.Settings) {
        if (setting.Name == name) {
            return &setting;
        }
    }

    return nullptr;
}

auto FindManagedAbiInnerEntry(const ManagedAbiManifest& abi, string_view owner, string_view entry_name) -> nptr<const ManagedAbiInnerEntry>
{
    FO_NO_STACK_TRACE_ENTRY();

    for (const ManagedAbiInnerEntry& inner : abi.InnerEntries) {
        if (inner.Owner == owner && inner.EntryName == entry_name) {
            return &inner;
        }
    }

    return nullptr;
}

// Every generated class with a native-pointer constructor, mirroring what the Entities and Types files emit: the
// bind stub registers a managed factory for each, and the backend resolves their constructors once at bind time
auto CollectManagedAbiWrapperClasses(const EngineMetadata& meta) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> result;

    for (const auto& [type_name, desc] : meta.GetEntityTypes()) {
        if (desc.IsGlobal) {
            continue;
        }

        if (desc.HasAbstract) {
            result.emplace_back(strex("Abstract{}", type_name).str());
        }

        result.emplace_back(type_name.as_str());

        if (desc.HasProtos) {
            result.emplace_back(strex("Proto{}", type_name).str());
        }
        if (desc.HasStatics) {
            result.emplace_back(strex("Static{}", type_name).str());
        }
    }

    for (const auto& type_name : meta.GetFixedTypes() | std::views::keys) {
        result.emplace_back(type_name.as_str());
    }

    for (const auto& type : meta.GetBaseTypes() | std::views::values) {
        if (type.IsRefType && type.RefType && !type.RefType->FieldsRegistrar) {
            result.emplace_back(type.Name);
        }
    }

    std::ranges::sort(result);
    return result;
}

auto IsManagedAbiDynamicRefType(const BaseTypeDesc& type) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return type.IsRefType && type.RefType && type.RefType->FieldsRegistrar;
}

auto IsManagedAbiHandleType(const ComplexTypeDesc& type, bool wrapped) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Kind != ComplexTypeKind::Simple) {
        return false;
    }

    const BaseTypeDesc& base_type = type.BaseType;

    // A handle that reaches managed code is wrapped by its runtime type, which only the boxing path resolves for an
    // abstract or base-typed entity; a handle that only travels into native code is a pointer whatever its type
    if (base_type.IsEntity || base_type.IsFixedType || base_type.IsEntityProto) {
        return !wrapped || (!base_type.IsAbstractEntity && base_type.Name != "Entity");
    }
    if (base_type.IsRefType) {
        return !IsManagedAbiDynamicRefType(base_type);
    }

    return false;
}

auto MakeManagedAbiCallbackKey(const ComplexTypeDesc& ret, const_span<ComplexTypeDesc> args) -> string
{
    FO_STACK_TRACE_ENTRY();

    // Metadata type names, not C# spellings: every type a frame carries is a plain identifier on both sides
    string key = "Callback_";

    if (ret) {
        key += ret.BaseType.Name;
    }
    else {
        key += "void";
    }

    for (const ComplexTypeDesc& arg : args) {
        key += "_";
        key += arg.BaseType.Name;
    }

    return key;
}

auto BuildManagedAbiCallbackLayout(const ComplexTypeDesc& ret, const_span<ComplexTypeDesc> args) -> ManagedAbiCallbackLayout
{
    FO_STACK_TRACE_ENTRY();

    ManagedAbiCallbackLayout layout;
    uint16_t offset = 0;
    layout.Args.reserve(args.size());

    for (const ComplexTypeDesc& arg : args) {
        // A by-ref callback argument is copied back out of a boxed object, which the frame path does not do
        if (arg.IsMutable) {
            return layout;
        }

        ManagedAbiSlot slot;

        if (IsManagedAbiHandleType(arg, true)) {
            slot.Kind = ManagedAbiValueKind::Handle;
            slot.Size = MANAGED_ABI_HANDLE_SLOT_SIZE;
        }
        else if (IsManagedAbiFixedValueType(arg)) {
            slot = MakeAbiSlot(arg, offset);
        }
        else {
            return layout;
        }

        if (slot.Size == 0) {
            return layout;
        }

        slot.Offset = offset;
        offset = numeric_cast<uint16_t>(offset + slot.Size);
        layout.Args.emplace_back(slot);
    }

    layout.ResultOffset = offset;

    if (ret) {
        if (!IsManagedAbiFixedValueType(ret)) {
            return layout;
        }

        layout.Ret = MakeAbiSlot(ret, offset);

        if (layout.Ret.Size == 0) {
            return layout;
        }

        offset = numeric_cast<uint16_t>(offset + layout.Ret.Size);
    }

    if (offset > MANAGED_ABI_SCALAR_FRAME_CAPACITY) {
        return layout;
    }

    layout.FrameSize = offset;
    layout.Supported = true;
    return layout;
}

static_assert(sizeof(mpos) == 4 && std::is_standard_layout_v<mpos>);
static_assert(sizeof(msize) == 4 && std::is_standard_layout_v<msize>);
static_assert(sizeof(ipos32) == 8 && std::is_standard_layout_v<ipos32>);
static_assert(sizeof(isize32) == 8 && std::is_standard_layout_v<isize32>);
static_assert(sizeof(irect32) == 16 && std::is_standard_layout_v<irect32>);
static_assert(sizeof(fpos32) == 8 && std::is_standard_layout_v<fpos32>);
static_assert(sizeof(ucolor) == 4 && std::is_standard_layout_v<ucolor>);
static_assert(sizeof(ident_t) == 8);

FO_END_NAMESPACE

#endif
