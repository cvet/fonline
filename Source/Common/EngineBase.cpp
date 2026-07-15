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

#include "EngineBase.h"

#include "ConfigFile.h"
#include "ImGuiStuff.h"

FO_BEGIN_NAMESPACE

struct EngineBaseData
{
    EngineBaseData()
    {
        BuiltinTypes = {//
            {"int8",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsInt8 = true;
                    type.IsSignedInt = true;
                    type.Size = sizeof(int8_t);
                }},
            {"int16",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsInt16 = true;
                    type.IsSignedInt = true;
                    type.Size = sizeof(int16_t);
                }},
            {"int32",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsInt32 = true;
                    type.IsSignedInt = true;
                    type.Size = sizeof(int32_t);
                }},
            {"int64",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsInt64 = true;
                    type.IsSignedInt = true;
                    type.Size = sizeof(int64_t);
                }},
            {"uint8",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsUInt8 = true;
                    type.IsSignedInt = false;
                    type.Size = sizeof(uint8_t);
                }},
            {"uint16",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsUInt16 = true;
                    type.IsSignedInt = false;
                    type.Size = sizeof(uint16_t);
                }},
            {"uint32",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsUInt32 = true;
                    type.IsSignedInt = false;
                    type.Size = sizeof(uint32_t);
                }},
            {"uint64",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsUInt64 = true;
                    type.IsSignedInt = false;
                    type.Size = sizeof(uint64_t);
                }},
            {"float32",
                [](BaseTypeDesc& type) {
                    type.IsFloat = true;
                    type.IsSingleFloat = true;
                    type.Size = sizeof(float32_t);
                }},
            {"float64",
                [](BaseTypeDesc& type) {
                    type.IsFloat = true;
                    type.IsDoubleFloat = true;
                    type.Size = sizeof(float64_t);
                }},
            {"bool",
                [](BaseTypeDesc& type) {
                    type.IsBool = true;
                    type.Size = sizeof(bool);
                }},
            {"string",
                [](BaseTypeDesc& type) {
                    type.IsObject = true;
                    type.IsString = true;
                    type.Size = 0;
                }},
            {"any",
                [](BaseTypeDesc& type) {
                    type.IsObject = true;
                    type.IsString = true;
                    type.Size = 0;
                }},
            {"hstring",
                [](BaseTypeDesc& type) {
                    type.IsObject = true;
                    type.IsHashedString = true;
                    type.Size = sizeof(hstring::hash_t);
                }},
            {"Entity", [](BaseTypeDesc& type) {
                 type.IsEntity = true;
                 type.IsObject = true;
             }}};
    }

    unordered_map<string_view, function<void(BaseTypeDesc&)>> BuiltinTypes {};
};
FO_GLOBAL_DATA(EngineBaseData, Data);

EngineMetadata::EngineMetadata(const MeatdataRegistrator& registrator) :
    _protoMngr(make_ptr(this))
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(registrator, "Missing property registrator");

    for (const auto& name : Data->BuiltinTypes | std::views::keys) {
        RegisterBaseType(name);
    }

    registrator();
}

void EngineMetadata::RegisterSide(EngineSideKind side)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(_entityTypes.empty(), "Entity types must be empty before this operation");

    _side = side;
}

auto EngineMetadata::RegisterEntityType(string_view name, bool exported, bool is_global, bool has_protos, bool has_statics, bool has_abstract) -> ptr<PropertyRegistrator>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    const auto it = _entityTypes.find(Hashes.ToHashedString(name));
    FO_VERIFY_AND_THROW(it == _entityTypes.end(), "Unexpected entry found in entity types");
    FO_VERIFY_AND_THROW(!_fixedTypesByStr.contains(name), "Entity type name conflicts with an already registered fixed type", name);
    FO_VERIFY_AND_THROW(!_baseTypes.contains(name), "Entity type name conflicts with an already registered base type", name);
    FO_VERIFY_AND_THROW(!has_protos || !_baseTypes.contains(strex("Proto{}", name)), "Entity proto type name conflicts with an already registered base type", name);
    FO_VERIFY_AND_THROW(!has_statics || !_baseTypes.contains(strex("Static{}", name)), "Entity static type name conflicts with an already registered base type", name);
    FO_VERIFY_AND_THROW(!has_abstract || !_baseTypes.contains(strex("Abstract{}", name)), "Entity abstract type name conflicts with an already registered base type", name);
    FO_VERIFY_AND_THROW(exported || _enums.count(strex("{}Property", name)) == 0, "Entity property enum type is already registered", name);
    FO_VERIFY_AND_THROW(exported || !_baseTypes.contains(strex("{}Property", name)), "Entity property type name conflicts with an already registered base type", name);

    auto registrator = SafeAlloc::MakeUnique<PropertyRegistrator>(name, _side, &Hashes, this);

    EntityTypeDesc desc {
        .Exported = exported,
        .IsGlobal = is_global,
        .HasProtos = has_protos,
        .HasStatics = has_statics,
        .HasAbstract = has_abstract,
        .PropRegistrator = std::move(registrator),
    };

    const auto entry = _entityTypes.emplace(Hashes.ToHashedString(name), std::move(desc));
    _entityTypesByStr.emplace(entry.first->first.as_str(), &entry.first->second);

    if (has_protos) {
        _entityRelatives.emplace(strex("Proto{}", name), &entry.first->second);
        auto proto_type = RegisterBaseType(strex("Proto{}", name));
        proto_type->IsEntityProto = true;
        proto_type->Size = sizeof(hstring::hash_t);
    }
    if (has_statics) {
        _entityRelatives.emplace(strex("Static{}", name), &entry.first->second);
        RegisterBaseType(strex("Static{}", name));
    }
    if (has_abstract) {
        _entityRelatives.emplace(strex("Abstract{}", name), &entry.first->second);
        RegisterBaseType(strex("Abstract{}", name));
    }

    if (!exported) {
        RegisterEnumGroup(strex("{}Property", name), "uint16", {{"None", 0}});

        entry.first->second.PropRegistrator->RegisterProperty({"Common", "ident", "CustomHolderId", "Persistent", "CoreProperty", "SharedProperty"});
        entry.first->second.PropRegistrator->RegisterProperty({"Common", "hstring", "CustomHolderEntry", "Persistent", "CoreProperty", "SharedProperty"});
    }

    auto type = RegisterBaseType(name);

    if (is_global) {
        type->IsSingleton = true;
    }

    return entry.first->second.PropRegistrator;
}

auto EngineMetadata::RegisterFixedType(string_view name, bool exported) -> ptr<PropertyRegistrator>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    const auto it = _fixedTypes.find(Hashes.ToHashedString(name));
    FO_VERIFY_AND_THROW(it == _fixedTypes.end(), "Unexpected entry found in fixed types");
    FO_VERIFY_AND_THROW(!_entityTypesByStr.contains(name), "Fixed type name conflicts with an already registered entity type", name);
    FO_VERIFY_AND_THROW(!_baseTypes.contains(name), "Fixed type name conflicts with an already registered base type", name);

    if (!exported) {
        RegisterEnumGroup(strex("{}Property", name), "uint16", {{"None", 0}});
    }

    auto registrator = SafeAlloc::MakeUnique<PropertyRegistrator>(name, _side, &Hashes, this);

    EntityTypeDesc desc {
        .Exported = exported,
        .IsGlobal = false,
        .HasProtos = true,
        .HasStatics = false,
        .HasAbstract = false,
        .PropRegistrator = std::move(registrator),
    };

    const auto entry = _fixedTypes.emplace(Hashes.ToHashedString(name), std::move(desc));
    _fixedTypesByStr.emplace(entry.first->first.as_str(), &entry.first->second);

    RegisterBaseType(name);

    return entry.first->second.PropRegistrator;
}

void EngineMetadata::RegsiterEntityHolderEntry(string_view holder_type, string_view target_type, string_view entry, EntityHolderEntrySync sync, bool persistent)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsValidEntityType(target_type), "Invalid migration target entity type");

    const auto it = _entityTypesByStr.find(holder_type);
    FO_VERIFY_AND_THROW(it != _entityTypesByStr.end(), "Holder entry registration references an unknown holder entity type", holder_type, target_type, entry);
    FO_VERIFY_AND_THROW(it->second->HolderEntries.count(Hashes.ToHashedString(entry)) == 0, "Holder entity type already has an entry with this name", holder_type, target_type, entry);

    auto registrator = GetPropertyRegistratorForEdit(holder_type);
    ptr<const Property> prop = persistent ? //
        registrator->RegisterProperty({"Server", "ident[]", strex("{}Ids", entry), "Persistent", "CoreProperty"}) : //
        registrator->RegisterProperty({"Server", "ident[]", strex("{}Ids", entry), "CoreProperty"});
    RegisterEnumEntry(strex("{}Property", holder_type), strex("{}Ids", entry), numeric_cast<int32_t>(prop->GetRegIndex()));

    it->second->HolderEntries.emplace(Hashes.ToHashedString(entry), EntityTypeDesc::HolderEntryDesc {.TargetType = Hashes.ToHashedString(target_type), .Sync = sync, .Persistent = persistent});
}

void EngineMetadata::RegisterEnumGroup(string_view name, string_view underlying_type, unordered_map<string, int32_t>&& key_values)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(IsValidBaseType(underlying_type), "Invalid enum underlying base type");
    FO_VERIFY_AND_THROW(_enums.count(name) == 0, "Enum type is already registered", name);
    FO_VERIFY_AND_THROW(!_baseTypes.contains(name), "Enum type name conflicts with an already registered base type", name);

    unordered_map<int32_t, string> key_values_rev;
    unordered_map<string, int32_t> new_full_names;

    for (auto&& [key, value] : key_values) {
        FO_VERIFY_AND_THROW(key != "None" || value <= 0, "Enum entry named None cannot have a positive value", name, key, value);
        FO_VERIFY_AND_THROW(key_values_rev.count(value) == 0, "Enum registration contains duplicate numeric values", name, key, value);
        key_values_rev.emplace(value, key);
        string full_key = strex("{}::{}", name, key);
        FO_VERIFY_AND_THROW(_enumsFullName.count(full_key) == 0 && new_full_names.count(full_key) == 0, "Enum full-name entry is already registered", name, key, full_key);
        new_full_names.emplace(std::move(full_key), value);
    }

    auto name_str = string(name);
    _enums.emplace(name_str, std::move(key_values));
    _enumsRev.emplace(name_str, std::move(key_values_rev));
    _enumsUnderlyingType.emplace(std::move(name_str), &GetBaseType(underlying_type));

    for (auto&& [full_key, value] : new_full_names) {
        _enumsFullName.emplace(std::move(full_key), value);
    }

    RegisterBaseType(name);
}

void EngineMetadata::RegisterEnumEntry(string_view name, string_view entry_name, int32_t entry_value)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    const auto name_str = string(name);
    FO_VERIFY_AND_THROW(name_str != "None" || entry_value <= 0, "Enum named None cannot register a positive entry value", name, entry_name, entry_value);
    FO_VERIFY_AND_THROW(_enums.count(name) != 0, "Enum entry registration references an unknown enum type", name, entry_name);
    FO_VERIFY_AND_THROW(_enums.at(name_str).count(entry_name) == 0, "Enum entry name is already registered for this enum type", name, entry_name);
    FO_VERIFY_AND_THROW(_enumsRev.at(name_str).count(entry_value) == 0, "Enum entry value is already registered for this enum type", name, entry_name, entry_value);
    string full_key = strex("{}::{}", name, entry_name);
    FO_VERIFY_AND_THROW(_enumsFullName.count(full_key) == 0, "Enum full-name entry is already registered", name, entry_name, full_key);

    _enums.at(name_str).emplace(entry_name, entry_value);
    _enumsRev.at(name_str).emplace(entry_value, entry_name);
    _enumsFullName.emplace(std::move(full_key), entry_value);
}

void EngineMetadata::RegisterValueType(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(_structLayouts.count(string(name)) == 0, "Value type is already registered", name);
    FO_VERIFY_AND_THROW(!_baseTypes.contains(name), "Value type name conflicts with an already registered base type", name);

    StructLayoutDesc layout_desc;
    _structLayouts.emplace(name, std::move(layout_desc));
    RegisterBaseType(name);
}

void EngineMetadata::RegisterValueTypeLayout(string_view name, const vector<pair<string_view, string_view>>& layout)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(_baseTypes.count(name) != 0, "Value type layout registration cannot find the base type entry", name, _baseTypes.size());
    FO_VERIFY_AND_THROW(_structLayouts.count(name) != 0, "Value type layout registration cannot find the struct layout entry", name, _structLayouts.size());
    const auto name_str = string(name);
    FO_VERIFY_AND_THROW(_structLayouts.at(name_str).Fields.empty(), "Value type layout is already registered", name, _structLayouts.at(name_str).Fields.size());

    auto& type = _baseTypes.at(name_str);
    FO_VERIFY_AND_THROW(type.Size == 0, "Type size must be zero before layout registration");
    auto& layout_desc = _structLayouts.at(name_str);
    FO_VERIFY_AND_THROW(layout_desc.Size == 0, "Struct layout size must be zero before field registration");

    vector<FieldDesc> fields;
    size_t total_size = 0;

    for (const auto& [field_name, field_type] : layout) {
        FO_VERIFY_AND_THROW(!field_name.empty(), "Value type layout contains a field with an empty name", name, field_type, layout.size());
        FO_VERIFY_AND_THROW(IsValidBaseType(field_type), "Value type layout contains an unknown field base type", name, field_name, field_type);
        auto& field = fields.emplace_back();
        field.Name = field_name;
        field.Type = GetBaseType(field_type);
        FO_VERIFY_AND_THROW(field.Type.IsPrimitive || field.Type.IsEnum || field.Type.IsSimpleStruct || field.Type.IsHashedString, "Struct field type is not supported in fixed layout");
        FO_VERIFY_AND_THROW(field.Type.Size != 0, "Struct field type has zero size");
        FO_VERIFY_AND_THROW(total_size % field.Type.Size == 0, "Value type layout data is not aligned", name, field.Name);
        field.Offset = total_size;
        total_size += field.Type.Size;
    }

    FO_VERIFY_AND_THROW(total_size != 0, "Registered type has zero size");

    layout_desc.Fields = std::move(fields);
    layout_desc.Size = total_size;
    type.Size = total_size;
    type.StructLayout = &layout_desc;
    type.IsSimpleStruct = layout_desc.Fields.size() == 1;
    type.IsComplexStruct = layout_desc.Fields.size() > 1;
}

void EngineMetadata::RegisterRefType(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(_refTypes.count(name) == 0, "RefType is already registered", name);
    FO_VERIFY_AND_THROW(!_baseTypes.contains(name), "RefType name conflicts with an already registered base type", name);

    RefTypeDesc ref_type;
    _refTypes.emplace(name, std::move(ref_type));
    RegisterBaseType(name);
}

void EngineMetadata::RegisterRefTypeLayout(string_view name, const vector<vector<string_view>>& layout)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(_refTypes.count(name) != 0, "RefType layout registration cannot find the RefType entry", name, _refTypes.size());
    FO_VERIFY_AND_THROW(!layout.empty(), "RefType layout registration received no fields", name);

    auto& ref_type = _refTypes[string(name)];
    FO_VERIFY_AND_THROW(ref_type.Methods.empty(), "RefType layout registration conflicts with already registered methods", name, ref_type.Methods.size());
    FO_VERIFY_AND_THROW(!ref_type.IsDynamicLayout, "RefType layout is already registered", name, layout.size());
    FO_VERIFY_AND_THROW(ref_type.FieldsRegistrator == nullptr, "RefType layout registration found an existing fields registrator", name);
    FO_VERIFY_AND_THROW(_dynamicRefTypeRegistrators.count(string(name)) == 0, "Dynamic RefType registrator is already registered", name);

    auto fields_registrator = SafeAlloc::MakeUnique<PropertyRegistrator>(strex("{}RefType", name), _side, &Hashes, this);

    for (const auto& field_tokens : layout) {
        FO_VERIFY_AND_THROW(field_tokens.size() >= 2, "RefType field needs at least name and type tokens", name);

        vector<string_view> tokens;
        tokens.reserve(field_tokens.size() + 1);
        tokens.emplace_back("Common");
        tokens.emplace_back(field_tokens[1]); // Type
        tokens.emplace_back(field_tokens[0]); // Name
        tokens.insert(tokens.end(), field_tokens.begin() + 2, field_tokens.end());

        fields_registrator->RegisterProperty(tokens);
    }

    ref_type.FieldsRegistrator = fields_registrator;
    ref_type.IsDynamicLayout = true;
    _dynamicRefTypeRegistrators.emplace(name, std::move(fields_registrator));
}

void EngineMetadata::RegisterRefTypeMethods(string_view name, vector<MethodDesc>&& methods)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(_refTypes.count(name) != 0, "RefType methods registration cannot find the RefType entry", name, _refTypes.size());

    auto& ref_type = _refTypes[string(name)];
    FO_VERIFY_AND_THROW(ref_type.Methods.empty(), "RefType methods are already registered", name, ref_type.Methods.size());
    FO_VERIFY_AND_THROW(!ref_type.IsDynamicLayout, "RefType methods registration conflicts with a dynamic field layout", name);
    FO_VERIFY_AND_THROW(ref_type.FieldsRegistrator == nullptr, "RefType methods registration found an existing fields registrator", name);

    ref_type.Methods = std::move(methods);
}

void EngineMetadata::RegisterRefTypeMethod(string_view name, MethodDesc&& method)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(_refTypes.count(name) != 0, "RefType single-method registration cannot find the RefType entry", name, _refTypes.size());

    auto& ref_type = _refTypes[string(name)];
    FO_VERIFY_AND_THROW(ref_type.Methods.empty(), "RefType single-method registration conflicts with already registered methods", name, ref_type.Methods.size(), method.Name);
    FO_VERIFY_AND_THROW(!ref_type.IsDynamicLayout, "RefType single-method registration conflicts with a dynamic field layout", name, method.Name);
    FO_VERIFY_AND_THROW(ref_type.FieldsRegistrator == nullptr, "RefType single-method registration found an existing fields registrator", name, method.Name);

    ref_type.Methods.emplace_back(std::move(method));
}

void EngineMetadata::RegisterEntityMethod(string_view entity_name, MethodDesc&& method)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    const auto it = _entityTypesByStr.find(entity_name);
    FO_VERIFY_AND_THROW(it != _entityTypesByStr.end(), "Lookup failed in entity types by str");
    auto entity_info = it->second;

    entity_info->Methods.emplace_back(std::move(method));
}

void EngineMetadata::RegisterEntityMethods(string_view entity_name, vector<MethodDesc>&& methods)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    const auto it = _entityTypesByStr.find(entity_name);
    FO_VERIFY_AND_THROW(it != _entityTypesByStr.end(), "Lookup failed in entity types by str");
    auto entity_info = it->second;

    FO_VERIFY_AND_THROW(entity_info->Methods.empty(), "Entity info methods must be empty before this operation");
    entity_info->Methods = std::move(methods);
}

void EngineMetadata::RegisterEntityEvents(string_view entity_name, vector<EntityEventDesc>&& events)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    const auto it = _entityTypesByStr.find(entity_name);
    FO_VERIFY_AND_THROW(it != _entityTypesByStr.end(), "Lookup failed in entity types by str");
    auto entity_info = it->second;

    FO_VERIFY_AND_THROW(entity_info->Events.empty(), "Entity info events must be empty before this operation");
    entity_info->Events = std::move(events);
}

void EngineMetadata::RegisterEntityEvent(string_view entity_name, EntityEventDesc&& event)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    const auto it = _entityTypesByStr.find(entity_name);
    FO_VERIFY_AND_THROW(it != _entityTypesByStr.end(), "Lookup failed in entity types by str");
    auto entity_info = it->second;

    entity_info->Events.emplace_back(std::move(event));
}

void EngineMetadata::RegisterOutboundRemoteCall(RemoteCallDesc&& remote_call)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(!_outboundRemoteCalls.contains(remote_call.Name), "Outbound remote call is already registered", remote_call.Name, remote_call.SubsystemHint);

    _outboundRemoteCalls.emplace(remote_call.Name, std::move(remote_call));
}

void EngineMetadata::RegisterInboundRemoteCall(RemoteCallDesc&& remote_call)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(!_inboundRemoteCalls.contains(remote_call.Name), "Inbound remote call is already registered", remote_call.Name, remote_call.SubsystemHint);

    _inboundRemoteCalls.emplace(remote_call.Name, std::move(remote_call));
}

void EngineMetadata::RegisterGameSetting(string_view name, const BaseTypeDesc& type)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(!_gameSettings.contains(name), "Game setting is already registered", name);

    _gameSettings.emplace(name, &type);
}

void EngineMetadata::RegisterMigrationRules(unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>&& migration_rules)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(_migrationRules.empty(), "Migration rules must be empty before this operation");

    for (const auto& [name1, rules_by_info] : migration_rules) {
        for (const auto& [name2, rules] : rules_by_info) {
            for (const auto& [target, replacement] : rules) {
                FO_VERIFY_AND_THROW(target != replacement, "Migration target and replacement fields are identical");

                unordered_set<hstring> visited {};
                visited.emplace(target);

                hstring current = replacement;

                while (true) {
                    const auto [name3, inserted] = visited.emplace(current);
                    ignore_unused(name3);
                    FO_VERIFY_AND_THROW(inserted, "Migration rule chain contains a cycle while finalizing registration", name1, name2, target, replacement, current);

                    const auto it = rules.find(current);

                    if (it == rules.end()) {
                        break;
                    }

                    current = it->second;
                }
            }
        }
    }

    _migrationRules = std::move(migration_rules);
}

void EngineMetadata::RegisterMigrationRule(string_view rule_name, string_view extra_info, string_view target, string_view replacement)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    const auto hrule_name = Hashes.ToHashedString(rule_name);
    const auto hextra_info = Hashes.ToHashedString(extra_info);
    const auto htarget = Hashes.ToHashedString(target);
    const auto hreplacement = Hashes.ToHashedString(replacement);

    auto& rules = _migrationRules[hrule_name][hextra_info];

    FO_VERIFY_AND_THROW(!rules.contains(htarget), "Migration rule target is already registered", rule_name, extra_info, target, replacement);
    FO_VERIFY_AND_THROW(htarget != hreplacement, "Migration target and replacement hashes are identical");

    unordered_set<hstring> visited {};
    visited.emplace(htarget);

    hstring current = hreplacement;

    while (true) {
        const auto [name, inserted] = visited.emplace(current);
        ignore_unused(name);
        FO_VERIFY_AND_THROW(inserted, "Migration rule chain contains a cycle", rule_name, extra_info, target, replacement, current);

        const auto it = rules.find(current);

        if (it == rules.end()) {
            break;
        }

        current = it->second;
    }

    rules.emplace(htarget, hreplacement);
}

auto EngineMetadata::RegisterBaseType(string_view type_str) -> ptr<BaseTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(!_baseTypes.contains(type_str), "Base type is already registered", type_str);

    if (type_str.empty()) {
        throw TypeResolveException("Invalid base type, empty type string", type_str);
    }
    if (type_str == "void") {
        throw TypeResolveException("Invalid base type, void type", type_str);
    }

    BaseTypeDesc type;
    type.Name = type_str;
    type.HashedName = Hashes.ToHashedString(type_str);

    if (const auto it = Data->BuiltinTypes.find(type_str); it != Data->BuiltinTypes.end()) {
        it->second(type);
    }
    else if (const auto it2 = _enumsUnderlyingType.find(type_str); it2 != _enumsUnderlyingType.end()) {
        type.IsEnum = true;
        type.EnumUnderlyingType = it2->second;
        type.Size = it2->second->Size;
    }
    else if (const auto it3 = _structLayouts.find(type_str); it3 != _structLayouts.end()) {
        type.IsStruct = true;
        type.IsObject = true;
    }
    else if (const auto it4 = _refTypes.find(type_str); it4 != _refTypes.end()) {
        type.IsRefType = true;
        type.IsObject = true;
        type.RefType = &it4->second;
    }
    else if (const auto it5 = _entityTypesByStr.find(type_str); it5 != _entityTypesByStr.end()) {
        type.IsEntity = true;
        type.IsObject = true;
        type.IsGlobalEntity = it5->second->IsGlobal;
    }
    else if (const auto it6 = _entityRelatives.find(type_str); it6 != _entityRelatives.end()) {
        type.IsEntity = true;
        type.IsObject = true;
        type.IsGlobalEntity = it6->second->IsGlobal;
    }
    else if (const auto it7 = _fixedTypesByStr.find(type_str); it7 != _fixedTypesByStr.end()) {
        type.IsFixedType = true;
        type.IsObject = true;
        type.Size = sizeof(hstring::hash_t);
    }
    else {
        throw TypeResolveException("Invalid base type", type_str);
    }

    type.IsPrimitive = type.IsInt || type.IsFloat || type.IsBool;
    return &_baseTypes.emplace(type_str, std::move(type)).first->second;
}

void EngineMetadata::FinalizeRegistration()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");
    FO_VERIFY_AND_THROW(!std::ranges::any_of(_structLayouts, [](auto&& e) { return e.second.Fields.empty(); }), "Registered struct layout has no fields");
    FO_VERIFY_AND_THROW(!std::ranges::any_of(_refTypes, [](auto&& e) { return e.second.Methods.empty() && e.second.FieldsRegistrator == nullptr; }), "Registered reference type has no methods or field registrator");

    _registrationFinalized = true;
}

auto EngineMetadata::GetPropertyRegistrator(hstring type_name) const noexcept -> nptr<const PropertyRegistrator>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _entityTypes.find(type_name);

    if (it != _entityTypes.end()) {
        return it->second.PropRegistrator;
    }

    const auto it2 = _fixedTypes.find(type_name);

    if (it2 != _fixedTypes.end()) {
        return it2->second.PropRegistrator;
    }

    return nullptr;
}

auto EngineMetadata::GetPropertyRegistrator(string_view type_name) const noexcept -> nptr<const PropertyRegistrator>
{
    FO_STACK_TRACE_ENTRY();

    const auto type_name_hashed = Hashes.ToHashedString(type_name);

    return GetPropertyRegistrator(type_name_hashed);
}

auto EngineMetadata::GetPropertyRegistratorForEdit(string_view type_name) -> ptr<PropertyRegistrator>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    const auto it = _entityTypesByStr.find(type_name);

    if (it != _entityTypesByStr.end()) {
        return it->second->PropRegistrator;
    }

    const auto it2 = _fixedTypesByStr.find(type_name);
    FO_VERIFY_AND_THROW(it2 != _fixedTypesByStr.end(), "Lookup failed in fixed types by str");
    return it2->second->PropRegistrator;
}

auto EngineMetadata::IsValidBaseType(string_view type_str) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _baseTypes.contains(type_str);
}

auto EngineMetadata::IsValidEntityType(hstring type_name) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _entityTypes.contains(type_name);
}

auto EngineMetadata::IsValidEntityType(string_view type_name) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _entityTypesByStr.contains(type_name);
}

auto EngineMetadata::GetEntityType(hstring type_name) const -> const EntityTypeDesc&
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _entityTypes.find(type_name);
    FO_VERIFY_AND_THROW(it != _entityTypes.end(), "Lookup failed in entity types");

    return it->second;
}

auto EngineMetadata::GetEntityTypes() const noexcept -> const map<hstring, EntityTypeDesc>&
{
    FO_NO_STACK_TRACE_ENTRY();

    return _entityTypes;
}

auto EngineMetadata::IsFixedType(hstring type_name) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _fixedTypes.contains(type_name);
}

auto EngineMetadata::IsFixedType(string_view type_name) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return _fixedTypesByStr.contains(type_name);
}

auto EngineMetadata::GetFixedType(hstring type_name) const -> const EntityTypeDesc&
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto it = _fixedTypes.find(type_name);
    FO_VERIFY_AND_THROW(it != _fixedTypes.end(), "Lookup failed in fixed types");

    return it->second;
}

auto EngineMetadata::GetFixedTypes() const noexcept -> const map<hstring, EntityTypeDesc>&
{
    FO_NO_STACK_TRACE_ENTRY();

    return _fixedTypes;
}

auto EngineMetadata::GetEntityHolderIdsProp(ptr<Entity> holder, hstring entry) const -> ptr<const Property>
{
    FO_STACK_TRACE_ENTRY();

    const auto prop_name = Hashes.ToHashedString(strex("{}Ids", entry));
    auto holder_prop = holder->GetProperties()->GetRegistrator()->FindProperty(prop_name);
    FO_VERIFY_AND_THROW(holder_prop, "Missing required holder property");

    return holder_prop;
}

auto EngineMetadata::GetBaseType(string_view type_str) const -> const BaseTypeDesc&
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _baseTypes.find(type_str);

    if (it == _baseTypes.end()) {
        throw TypeResolveException("Invalid base type", type_str);
    }

    return it->second;
}

auto EngineMetadata::ResolveComplexType(string_view type_str) const -> ComplexTypeDesc
{
    FO_STACK_TRACE_ENTRY();

    const auto tokens = strvex(type_str).tokenize();
    const auto& [type, tokens_len] = ResolveComplexType(tokens);

    if (tokens_len != tokens.size()) {
        throw TypeResolveException("Provided tokens are more then needed for type parsing", type_str);
    }

    return type;
}

auto EngineMetadata::ResolveComplexType(span<const string_view> tokens) const -> pair<ComplexTypeDesc, size_t>
{
    FO_STACK_TRACE_ENTRY();

    if (tokens.empty()) {
        throw TypeResolveException("Invalid complex type syntax, no tokens provided", strex(" ").join(tokens));
    }

    ComplexTypeDesc type;
    size_t tokens_len = 0;

    if (tokens.size() >= 2 && tokens[1] == "=") {
        if (tokens.size() == 2) {
            throw TypeResolveException("Invalid complex type syntax, expected '>' for dict", strex(" ").join(tokens));
        }
        if (tokens.size() >= 3 && tokens[2] != ">") {
            throw TypeResolveException("Invalid complex type syntax, expected '>' for dict", strex(" ").join(tokens));
        }
        if (tokens.size() == 3) {
            throw TypeResolveException("Invalid complex type syntax, expected type", strex(" ").join(tokens));
        }

        const auto& [type2, tokens_len2] = ResolveComplexType(tokens.subspan(3));

        if (type2.Kind == ComplexTypeKind::Simple) {
            type.Kind = ComplexTypeKind::Dict;
        }
        else if (type2.Kind == ComplexTypeKind::Array) {
            type.Kind = ComplexTypeKind::DictOfArray;
        }
        else {
            throw TypeResolveException("Invalid complex type syntax, dict value must be simple type or array", strex(" ").join(tokens));
        }

        type.KeyType = GetBaseType(tokens[0]);
        type.BaseType = type2.BaseType;
        type.IsMutable = type2.IsMutable;
        tokens_len = 3 + tokens_len2;
    }
    else if (tokens.size() >= 2 && tokens[1] == "[") {
        if (tokens.size() == 2) {
            throw TypeResolveException("Invalid complex type syntax, expected ']' for array", strex(" ").join(tokens));
        }
        if (tokens[2] != "]") {
            throw TypeResolveException("Invalid complex type syntax, expected ']' for array", strex(" ").join(tokens));
        }

        type.Kind = ComplexTypeKind::Array;
        type.BaseType = GetBaseType(tokens[0]);
        type.IsMutable = tokens.size() >= 4 && tokens[3] == "&";
        tokens_len = 3 + (type.IsMutable ? 1 : 0);
    }
    else if (tokens[0] == "callback") {
        if (tokens.size() < 4) {
            throw TypeResolveException("Invalid complex type syntax, insufficient parameters for callback", strex(" ").join(tokens));
        }
        if (tokens[1] != "(") {
            throw TypeResolveException("Invalid complex type syntax, expected '(' for callback", strex(" ").join(tokens));
        }

        tokens_len += 2;
        auto args = SafeAlloc::MakeShared<vector<ComplexTypeDesc>>();

        while (true) {
            const bool is_first_arg = tokens_len == 2; // First argument is return type

            if (is_first_arg && tokens[tokens_len] == "void") {
                args->emplace_back();
                tokens_len += 1;
            }
            else {
                auto&& [type2, tokens_len2] = ResolveComplexType(tokens.subspan(tokens_len));

                if (is_first_arg && type2.IsMutable) {
                    throw TypeResolveException("Invalid complex type syntax, callback return type cannot be mutable", strex(" ").join(tokens));
                }
                if (type2.Kind == ComplexTypeKind::Callback) {
                    throw TypeResolveException("Invalid complex type syntax, callback cannot be used as argument type", strex(" ").join(tokens));
                }

                args->emplace_back(std::move(type2));
                tokens_len += tokens_len2;
            }

            if (tokens_len == tokens.size() || (tokens[tokens_len] != "," && tokens[tokens_len] != ")")) {
                throw TypeResolveException("Invalid complex type syntax, expected ',' or ')' for callback", strex(" ").join(tokens));
            }

            if (tokens[tokens_len++] == ")") {
                break;
            }
        }

        if (tokens_len < tokens.size() && tokens[tokens_len] == "&") {
            throw TypeResolveException("Invalid complex type syntax, callback cannot be mutable", strex(" ").join(tokens));
        }

        type.Kind = ComplexTypeKind::Callback;
        type.CallbackArgs = std::move(args);
    }
    else {
        type.Kind = ComplexTypeKind::Simple;
        type.BaseType = GetBaseType(tokens[0]);
        type.IsMutable = tokens.size() >= 2 && tokens[1] == "&";
        tokens_len = 1 + (type.IsMutable ? 1 : 0);
    }

    return pair(type, tokens_len);
}

auto EngineMetadata::ResolveEnumValue(string_view enum_value_name, nptr<bool> failed) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _enumsFullName.find(enum_value_name);

    if (it == _enumsFullName.end()) {
        if (failed) {
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum full value", enum_value_name);
    }

    return it->second;
}

auto EngineMetadata::ResolveEnumValue(string_view enum_name, string_view value_name, nptr<bool> failed) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto enum_it = _enums.find(enum_name);

    if (enum_it == _enums.end()) {
        if (failed) {
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum", enum_name, value_name);
    }

    const auto value_it = enum_it->second.find(value_name);

    if (value_it == enum_it->second.end()) {
        if (failed) {
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum value", enum_name, value_name);
    }

    return value_it->second;
}

auto EngineMetadata::ResolveEnumValueName(string_view enum_name, int32_t value, nptr<bool> failed) const -> string_view
{
    FO_STACK_TRACE_ENTRY();

    const auto enum_it = _enumsRev.find(enum_name);

    if (enum_it == _enumsRev.end()) {
        if (failed) {
            *failed = true;
            return _emptyStr;
        }

        throw EnumResolveException("Invalid enum for resolve value", enum_name, value);
    }

    const auto value_it = enum_it->second.find(value);

    if (value_it == enum_it->second.end()) {
        if (failed) {
            *failed = true;
            return _emptyStr;
        }

        throw EnumResolveException("Can't resolve value for enum", enum_name, value);
    }

    return value_it->second;
}

auto EngineMetadata::GetGameSetting(string_view name) const -> const BaseTypeDesc&
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _gameSettings.find(name);

    if (it == _gameSettings.end()) {
        throw TypeResolveException("Setting not found", name);
    }

    return *it->second;
}

auto EngineMetadata::CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring>
{
    FO_STACK_TRACE_ENTRY();

    if (_migrationRules.empty()) {
        return std::nullopt;
    }

    const auto it_rules = _migrationRules.find(rule_name);

    if (it_rules == _migrationRules.end()) {
        return std::nullopt;
    }

    const auto it_rules2 = it_rules->second.find(extra_info);

    if (it_rules2 == it_rules->second.end()) {
        return std::nullopt;
    }

    const auto it_target = it_rules2->second.find(target);

    if (it_target == it_rules2->second.end()) {
        return std::nullopt;
    }

    hstring result = it_target->second;

    while (true) {
        const auto it_target2 = it_rules2->second.find(result);

        if (it_target2 == it_rules2->second.end()) {
            break;
        }

        result = it_target2->second;
    }

    return result;
}

auto EngineMetadata::GetProtoItem(hstring proto_id) const noexcept -> nptr<const ProtoItem>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _protoMngr.GetProtoItem(proto_id);
}

auto EngineMetadata::GetProtoCritter(hstring proto_id) const noexcept -> nptr<const ProtoCritter>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _protoMngr.GetProtoCritter(proto_id);
}

auto EngineMetadata::GetProtoMap(hstring proto_id) const noexcept -> nptr<const ProtoMap>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _protoMngr.GetProtoMap(proto_id);
}

auto EngineMetadata::GetProtoLocation(hstring proto_id) const noexcept -> nptr<const ProtoLocation>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _protoMngr.GetProtoLocation(proto_id);
}

auto EngineMetadata::GetProtoEntity(hstring type_name, hstring proto_id) const noexcept -> nptr<const ProtoEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _entityRelatives.find(type_name.as_str()); it != _entityRelatives.end()) {
        type_name = it->second->PropRegistrator->GetTypeName();
    }

    return _protoMngr.GetProtoEntity(type_name, proto_id);
}

auto EngineMetadata::GetProtoEntities(hstring type_name) const noexcept -> const unordered_map<hstring, refcount_ptr<ProtoEntity>>&
{
    FO_NO_STACK_TRACE_ENTRY();

    if (const auto it = _entityRelatives.find(type_name.as_str()); it != _entityRelatives.end()) {
        type_name = it->second->PropRegistrator->GetTypeName();
    }

    return _protoMngr.GetProtoEntities(type_name);
}

auto EngineMetadata::GetModelAnimDuration(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim) const -> timespan
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto model_it = _modelAnimDurations.find(model_name);

    if (model_it == _modelAnimDurations.end()) {
        return {};
    }

    const auto anim_it = model_it->second.find({state_anim, action_anim});

    if (anim_it == model_it->second.end()) {
        return {};
    }

    return anim_it->second;
}

void EngineMetadata::RegisterProto(hstring type_name, refcount_ptr<ProtoEntity> proto)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    _protoMngr.AddProto(type_name, std::move(proto));
}

void EngineMetadata::RegisterProtos(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    _protoMngr.LoadFromResources(resources);
}

void EngineMetadata::RegisterModelAnimInfo(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_registrationFinalized, "Registration is already finalized");

    constexpr string_view resource_path = "ModelAnimInfo.foinfo";

    if (!resources.IsFileExists(resource_path)) {
        WriteLog(LogType::Info, "Model animation durations document '{}' is not present", resource_path);
        return;
    }

    auto config = ConfigFile(resource_path, resources.ReadFileText(resource_path));
    unordered_map<hstring, unordered_map<pair<CritterStateAnim, CritterActionAnim>, timespan>> model_anim_durations;

    for (const auto& [model_name, values] : *config.GetSections()) {
        if (model_name.empty()) {
            continue;
        }

        const auto get_value = [&values](string_view key) -> string_view {
            const auto it = values.find(key);
            return it != values.end() ? it->second : string_view {};
        };

        const auto state_anims = strvex(get_value("StateAnims")).split_to_int32(' ');
        const auto action_anims = strvex(get_value("ActionAnims")).split_to_int32(' ');
        const auto durations_ms = strvex(get_value("DurationsMs")).split_to_int32(' ');

        FO_VERIFY_AND_THROW(!state_anims.empty(), "Model animation info section has no entries", model_name);
        FO_VERIFY_AND_THROW(state_anims.size() == action_anims.size() && action_anims.size() == durations_ms.size(), "Model animation info arrays have different sizes", model_name, state_anims.size(), action_anims.size(), durations_ms.size());

        const auto model_name_hashed = Hashes.ToHashedString(model_name);
        const auto [model_it, model_inserted] = model_anim_durations.try_emplace(model_name_hashed);
        FO_VERIFY_AND_THROW(model_inserted, "Duplicate model animation info section", model_name);
        auto& model_durations = model_it->second;

        for (size_t i = 0; i < state_anims.size(); i++) {
            const auto state_anim = static_cast<CritterStateAnim>(numeric_cast<uint16_t>(state_anims[i]));
            const auto action_anim = static_cast<CritterActionAnim>(numeric_cast<uint16_t>(action_anims[i]));
            FO_VERIFY_AND_THROW(durations_ms[i] > 0, "Model animation duration must be positive", model_name, state_anims[i], action_anims[i], durations_ms[i]);

            const auto [it, inserted] = model_durations.emplace(pair {state_anim, action_anim}, std::chrono::milliseconds {durations_ms[i]});
            ignore_unused(it);
            FO_VERIFY_AND_THROW(inserted, "Duplicate model animation info entry", model_name, state_anims[i], action_anims[i]);
        }
    }

    _modelAnimDurations = std::move(model_anim_durations);
}

BaseEngine::BaseEngine(ptr<GlobalSettings> settings, FileSystem&& resources, const MeatdataRegistrator& registrator) :
    EngineMetadata(registrator),
    ScriptSystem(),
    Entity(GetPropertyRegistratorForEdit(ENTITY_TYPE_NAME), nullptr, nullptr),
    GameProperties(*GetInitRef()),
    Settings {settings},
    Resources {std::move(resources)},
    GameTime(Settings),
    TimeEventMngr(make_ptr(this)),
    _imgui {SafeAlloc::MakeRefCounted<ScriptImGui>(make_ptr(this))}
{
    FO_STACK_TRACE_ENTRY();

    RegisterProtos(Resources);
    RegisterModelAnimInfo(Resources);
    FinalizeRegistration();
}

void BaseEngine::FrameAdvance()
{
    FO_STACK_TRACE_ENTRY();

    GameTime.FrameAdvance(IsRunInDebugger() || Settings->DisableNetworking);

    LockForPropertyAccess();
    auto unlock = scope_exit([this]() noexcept { UnlockForPropertyAccess(); });

    SetFrameTime(GameTime.GetFrameTime());
    SetFrameDeltaTime(GameTime.GetFrameDeltaTime());
    SetFramesPerSecond(GameTime.GetFramesPerSecond());

    if (GameTime.IsTimeSynchronized()) {
        SetSynchronizedTime(GameTime.GetSynchronizedTime());
    }
}

auto BaseEngine::Random(int32_t min_value, int32_t max_value) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(min_value <= max_value, "Engine random integer range has an inverted min/max", min_value, max_value);

    scoped_lock locker {_randomGeneratorLocker};

    return std::uniform_int_distribution<int32_t> {min_value, max_value}(_randomGenerator);
}

void BaseEngine::ScheduleDelayedCallback(timespan delay, function<void()> body)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(delay, body);

    throw InvalidCallException("ScheduleDelayedCallback not supported on this engine");
}

void BaseEngine::RunScriptContext(const function<void()>& callback)
{
    FO_STACK_TRACE_ENTRY();

    callback();
}

void BaseEngine::SendRemoteCall(hstring name, ptr<Entity> caller, const_span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(caller.get(), "Remote call requires a non-null caller entity");

    HandleOutboundRemoteCall(name, caller, data);
}

auto BaseEngine::HasRemoteCallHandler(hstring name) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _inboundRemoteCallHandlers.contains(name);
}

void BaseEngine::SetRemoteCallHandler(hstring name, RemoteCallHandler handler, bool replace)
{
    FO_STACK_TRACE_ENTRY();

    if (!replace) {
        FO_VERIFY_AND_THROW(!_inboundRemoteCallHandlers.contains(name), "Inbound remote call handler is already registered", name);
    }

    _inboundRemoteCallHandlers[name] = std::move(handler);
}

void BaseEngine::VerifyBindedRemoteCalls() const noexcept(false)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_inboundRemoteCallHandlers.size() == GetInboundRemoteCalls()->size(), "Inbound remote call handler table does not cover every registered remote call", _inboundRemoteCallHandlers.size(), GetInboundRemoteCalls()->size());
}

void BaseEngine::HandleInboundRemoteCall(hstring name, nptr<Entity> caller, span<uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _inboundRemoteCallHandlers.find(name);
    FO_VERIFY_AND_THROW(it != _inboundRemoteCallHandlers.end(), "Lookup failed in inbound remote call handlers");
    it->second(name, caller, data);
}

FO_END_NAMESPACE
