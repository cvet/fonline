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
                    type.Size = sizeof(int8);
                }},
            {"int16",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsInt16 = true;
                    type.IsSignedInt = true;
                    type.Size = sizeof(int16);
                }},
            {"int32",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsInt32 = true;
                    type.IsSignedInt = true;
                    type.Size = sizeof(int32);
                }},
            {"int64",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsInt64 = true;
                    type.IsSignedInt = true;
                    type.Size = sizeof(int64);
                }},
            {"uint8",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsUInt8 = true;
                    type.IsSignedInt = false;
                    type.Size = sizeof(uint8);
                }},
            {"uint16",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsUInt16 = true;
                    type.IsSignedInt = false;
                    type.Size = sizeof(uint16);
                }},
            {"uint32",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsUInt32 = true;
                    type.IsSignedInt = false;
                    type.Size = sizeof(uint32);
                }},
            {"uint64",
                [](BaseTypeDesc& type) {
                    type.IsInt = true;
                    type.IsUInt64 = true;
                    type.IsSignedInt = false;
                    type.Size = sizeof(uint64);
                }},
            {"float32",
                [](BaseTypeDesc& type) {
                    type.IsFloat = true;
                    type.IsSingleFloat = true;
                    type.Size = sizeof(float32);
                }},
            {"float64",
                [](BaseTypeDesc& type) {
                    type.IsFloat = true;
                    type.IsDoubleFloat = true;
                    type.Size = sizeof(float64);
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

EngineMetadata::EngineMetadata(const MeatdataRegistrator& registrator)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(registrator);

    for (const auto& name : Data->BuiltinTypes | std::views::keys) {
        RegisterBaseType(name);
    }

    registrator();
}

void EngineMetadata::RegisterSide(EngineSideKind side)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(_entityTypes.empty());

    _side = side;
}

auto EngineMetadata::RegisterEntityType(string_view name, bool exported, bool is_global, bool has_protos, bool has_statics, bool has_abstract) -> PropertyRegistrator*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _entityTypes.find(Hashes.ToHashedString(name));
    FO_RUNTIME_ASSERT(it == _entityTypes.end());

    auto* registrator = SafeAlloc::MakeRaw<PropertyRegistrator>(name, _side, Hashes, *this);

    EntityTypeDesc desc;
    desc.Exported = exported;
    desc.IsGlobal = is_global;
    desc.HasProtos = has_protos;
    desc.HasStatics = has_statics;
    desc.HasAbstract = has_abstract;
    desc.PropRegistrator = registrator;

    const auto entry = _entityTypes.emplace(Hashes.ToHashedString(name), std::move(desc));
    _entityTypesByStr.emplace(entry.first->first.as_str(), &entry.first->second);

    if (has_protos) {
        _entityRelatives.emplace(strex("Proto{}", name), &entry.first->second);
        RegisterBaseType(strex("Proto{}", name));
    }
    if (has_statics) {
        _entityRelatives.emplace(strex("Static{}", name), &entry.first->second);
        RegisterBaseType(strex("Static{}", name));
    }
    if (has_abstract) {
        _entityRelatives.emplace(strex("Abstract{}", name), &entry.first->second);
        RegisterBaseType(strex("Abstract{}", name));
    }

    RegisterEnumGroup(strex("{}Component", name), "int32", {{"None", 0}});

    if (!exported) {
        RegisterEnumGroup(strex("{}Property", name), "uint16", {{"None", 0}});

        registrator->RegisterProperty({"Common", "ident", "CustomHolderId", "Persistent", "CoreProperty", "SharedProperty"});
        registrator->RegisterProperty({"Common", "hstring", "CustomHolderEntry", "Persistent", "CoreProperty", "SharedProperty"});
    }

    RegisterBaseType(name);
    return registrator;
}

void EngineMetadata::RegsiterEntityHolderEntry(string_view holder_type, string_view target_type, string_view entry, EntityHolderEntrySync sync)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(IsValidEntityType(target_type));

    const auto it = _entityTypesByStr.find(holder_type);
    FO_RUNTIME_ASSERT(it != _entityTypesByStr.end());
    FO_RUNTIME_ASSERT(it->second->HolderEntries.count(Hashes.ToHashedString(entry)) == 0);

    it->second->HolderEntries.emplace(Hashes.ToHashedString(entry), EntityTypeDesc::HolderEntryDesc {.TargetType = Hashes.ToHashedString(target_type), .Sync = sync});

    auto* registrator = GetPropertyRegistratorForEdit(holder_type);
    const auto* prop = registrator->RegisterProperty({"Server", "ident[]", strex("{}Ids", entry), "Persistent", "CoreProperty"});
    RegisterEnumEntry(strex("{}Property", holder_type), strex("{}Ids", entry), numeric_cast<int32>(prop->GetRegIndex()));
}

void EngineMetadata::RegisterEnumGroup(string_view name, string_view underlying_type, unordered_map<string, int32>&& key_values)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(IsValidBaseType(underlying_type));
    FO_RUNTIME_ASSERT(_enums.count(name) == 0);

    unordered_map<int32, string> key_values_rev;

    for (auto&& [key, value] : key_values) {
        FO_RUNTIME_ASSERT_STR(key != "None" || value == 0, strex("Wrong enum {}", name));
        FO_RUNTIME_ASSERT(key_values_rev.count(value) == 0);
        key_values_rev.emplace(value, key);
        string full_key = strex("{}::{}", name, key);
        FO_RUNTIME_ASSERT(_enumsFullName.count(full_key) == 0);
        _enumsFullName.emplace(std::move(full_key), value);
    }

    auto name_str = string(name);
    _enums.emplace(name_str, std::move(key_values));
    _enumsRev.emplace(name_str, std::move(key_values_rev));
    _enumsUnderlyingType.emplace(std::move(name_str), &GetBaseType(underlying_type));

    RegisterBaseType(name);
}

void EngineMetadata::RegisterEnumEntry(string_view name, string_view entry_name, int32 entry_value)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    const auto name_str = string(name);
    FO_RUNTIME_ASSERT(name_str != "None" || entry_value == 0);
    FO_RUNTIME_ASSERT(_enums.count(name) != 0);
    FO_RUNTIME_ASSERT(_enums.at(name_str).count(entry_name) == 0);
    FO_RUNTIME_ASSERT(_enumsRev.at(name_str).count(entry_value) == 0);
    string full_key = strex("{}::{}", name, entry_name);
    FO_RUNTIME_ASSERT(_enumsFullName.count(full_key) == 0);

    _enums.at(name_str).emplace(entry_name, entry_value);
    _enumsRev.at(name_str).emplace(entry_value, entry_name);
    _enumsFullName.emplace(std::move(full_key), entry_value);
}

void EngineMetadata::RegisterValueType(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(_structLayouts.count(string(name)) == 0);

    StructLayoutDesc layout_desc;
    _structLayouts.emplace(name, std::move(layout_desc));
    RegisterBaseType(name);
}

void EngineMetadata::RegisterValueTypeLayout(string_view name, const vector<pair<string_view, string_view>>& layout)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(_baseTypes.count(name) != 0);
    FO_RUNTIME_ASSERT(_structLayouts.count(name) != 0);
    const auto name_str = string(name);
    FO_RUNTIME_ASSERT(_structLayouts.at(name_str).Fields.empty());

    auto& type = _baseTypes.at(name_str);
    FO_RUNTIME_ASSERT(type.Size == 0);
    auto& layout_desc = _structLayouts.at(name_str);
    FO_RUNTIME_ASSERT(layout_desc.Size == 0);
    size_t offset = 0;

    for (const auto& [field_name, field_type] : layout) {
        FO_RUNTIME_ASSERT(!field_name.empty());
        FO_RUNTIME_ASSERT(IsValidBaseType(field_type));
        auto& field = layout_desc.Fields.emplace_back();
        field.Name = field_name;
        field.Type = GetBaseType(field_type);
        FO_RUNTIME_ASSERT(field.Type.IsPrimitive || field.Type.IsEnum || field.Type.IsSimpleStruct);
        FO_RUNTIME_ASSERT(field.Type.Size != 0);
        FO_RUNTIME_ASSERT_STR(offset % field.Type.Size == 0, strex("{}::{} layout data is not aligned", name, field.Name));
        field.Offset = offset;
        offset += field.Type.Size;
        layout_desc.Size += field.Type.Size;
        type.Size += field.Type.Size;
    }

    FO_RUNTIME_ASSERT(type.Size != 0);
    type.StructLayout = &layout_desc;
    type.IsSimpleStruct = layout_desc.Fields.size() == 1;
    type.IsComplexStruct = layout_desc.Fields.size() > 1;
}

void EngineMetadata::RegisterRefType(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(_refTypes.count(name) == 0);

    RefTypeDesc ref_type;
    _refTypes.emplace(name, std::move(ref_type));
    RegisterBaseType(name);
}

void EngineMetadata::RegisterRefTypeMethods(string_view name, vector<MethodDesc>&& methods)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(_refTypes.count(name) != 0);

    auto& ref_type = _refTypes[string(name)];
    FO_RUNTIME_ASSERT(ref_type.Methods.empty());

    ref_type.Methods = std::move(methods);
}

void EngineMetadata::RegisterRefTypeMethod(string_view name, MethodDesc&& method)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(_refTypes.count(name) != 0);

    auto& ref_type = _refTypes[string(name)];
    FO_RUNTIME_ASSERT(ref_type.Methods.empty());

    ref_type.Methods.emplace_back(std::move(method));
}

void EngineMetadata::RegisterEntityMethod(string_view entity_name, MethodDesc&& method)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _entityTypesByStr.find(entity_name);
    FO_RUNTIME_ASSERT(it != _entityTypesByStr.end());
    auto& entity_info = *it->second;

    entity_info.Methods.emplace_back(std::move(method));
}

void EngineMetadata::RegisterEntityMethods(string_view entity_name, vector<MethodDesc>&& methods)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _entityTypesByStr.find(entity_name);
    FO_RUNTIME_ASSERT(it != _entityTypesByStr.end());
    auto& entity_info = *it->second;

    FO_RUNTIME_ASSERT(entity_info.Methods.empty());
    entity_info.Methods = std::move(methods);
}

void EngineMetadata::RegisterEntityEvents(string_view entity_name, vector<EntityEventDesc>&& events)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _entityTypesByStr.find(entity_name);
    FO_RUNTIME_ASSERT(it != _entityTypesByStr.end());
    auto& entity_info = *it->second;

    FO_RUNTIME_ASSERT(entity_info.Events.empty());
    entity_info.Events = std::move(events);
}

void EngineMetadata::RegisterEntityEvent(string_view entity_name, EntityEventDesc&& event)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _entityTypesByStr.find(entity_name);
    FO_RUNTIME_ASSERT(it != _entityTypesByStr.end());
    auto& entity_info = *it->second;

    entity_info.Events.emplace_back(std::move(event));
}

void EngineMetadata::RegisterOutboundRemoteCall(RemoteCallDesc&& remote_call)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(!_outboundRemoteCalls.contains(remote_call.Name));

    _outboundRemoteCalls.emplace(remote_call.Name, std::move(remote_call));
}

void EngineMetadata::RegisterInboundRemoteCall(RemoteCallDesc&& remote_call)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(!_inboundRemoteCalls.contains(remote_call.Name));

    _inboundRemoteCalls.emplace(remote_call.Name, std::move(remote_call));
}

void EngineMetadata::RegisterGameSetting(string_view name, const BaseTypeDesc& type)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(!_gameSettings.contains(name));

    _gameSettings.emplace(name, &type);
}

void EngineMetadata::RegisterMigrationRules(unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>&& migration_rules)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(_migrationRules.empty());

    _migrationRules = std::move(migration_rules);
}

void EngineMetadata::RegisterMigrationRule(string_view rule_name, string_view extra_info, string_view target, string_view replacement)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);

    const auto hrule_name = Hashes.ToHashedString(rule_name);
    const auto hextra_info = Hashes.ToHashedString(extra_info);
    const auto htarget = Hashes.ToHashedString(target);
    const auto hreplacement = Hashes.ToHashedString(replacement);

    _migrationRules[hrule_name][hextra_info][htarget] = hreplacement;
}

void EngineMetadata::RegisterBaseType(string_view type_str)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(!_baseTypes.contains(type_str));

    if (type_str.empty()) {
        throw TypeResolveException("Invalid base type, empty type string", type_str);
    }
    if (type_str == "void") {
        throw TypeResolveException("Invalid base type, void type", type_str);
    }

    BaseTypeDesc type;
    type.Name = type_str;

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
    else {
        throw TypeResolveException("Invalid base type", type_str);
    }

    type.IsPrimitive = type.IsInt || type.IsFloat || type.IsBool;

    _baseTypes.emplace(type_str, std::move(type));
}

void EngineMetadata::FinalizeRegistration()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);
    FO_RUNTIME_ASSERT(!std::ranges::any_of(_structLayouts, [](auto&& e) { return e.second.Fields.empty(); }));
    FO_RUNTIME_ASSERT(!std::ranges::any_of(_refTypes, [](auto&& e) { return e.second.Methods.empty(); }));

    _registrationFinalized = true;
}

auto EngineMetadata::GetPropertyRegistrator(hstring type_name) const noexcept -> const PropertyRegistrator*
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _entityTypes.find(type_name);

    return it != _entityTypes.end() ? it->second.PropRegistrator.get() : nullptr;
}

auto EngineMetadata::GetPropertyRegistrator(string_view type_name) const noexcept -> const PropertyRegistrator*
{
    FO_STACK_TRACE_ENTRY();

    const auto type_name_hashed = Hashes.ToHashedString(type_name);

    return GetPropertyRegistrator(type_name_hashed);
}

auto EngineMetadata::GetPropertyRegistratorForEdit(string_view type_name) -> PropertyRegistrator*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _entityTypesByStr.find(type_name);
    FO_RUNTIME_ASSERT(it != _entityTypesByStr.end());

    return const_cast<PropertyRegistrator*>(it->second->PropRegistrator.get_no_const());
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
    FO_RUNTIME_ASSERT(it != _entityTypes.end());

    return it->second;
}

auto EngineMetadata::GetEntityTypes() const noexcept -> const map<hstring, EntityTypeDesc>&
{
    FO_NO_STACK_TRACE_ENTRY();

    return _entityTypes;
}

auto EngineMetadata::GetEntityHolderIdsProp(Entity* holder, hstring entry) const -> const Property*
{
    FO_STACK_TRACE_ENTRY();

    const auto prop_name = Hashes.ToHashedString(strex("{}Ids", entry));
    const auto* holder_prop = holder->GetProperties().GetRegistrator()->FindProperty(prop_name);
    FO_RUNTIME_ASSERT(holder_prop);

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

auto EngineMetadata::ResolveEnumValue(string_view enum_value_name, bool* failed) const -> int32
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _enumsFullName.find(enum_value_name);

    if (it == _enumsFullName.end()) {
        if (failed != nullptr) {
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum full value", enum_value_name);
    }

    return it->second;
}

auto EngineMetadata::ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed) const -> int32
{
    FO_STACK_TRACE_ENTRY();

    const auto enum_it = _enums.find(enum_name);

    if (enum_it == _enums.end()) {
        if (failed != nullptr) {
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum", enum_name, value_name);
    }

    const auto value_it = enum_it->second.find(value_name);

    if (value_it == enum_it->second.end()) {
        if (failed != nullptr) {
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum value", enum_name, value_name);
    }

    return value_it->second;
}

auto EngineMetadata::ResolveEnumValueName(string_view enum_name, int32 value, bool* failed) const -> const string&
{
    FO_STACK_TRACE_ENTRY();

    const auto enum_it = _enumsRev.find(enum_name);

    if (enum_it == _enumsRev.end()) {
        if (failed != nullptr) {
            *failed = true;
            return _emptyStr;
        }

        throw EnumResolveException("Invalid enum for resolve value", enum_name, value);
    }

    const auto value_it = enum_it->second.find(value);

    if (value_it == enum_it->second.end()) {
        if (failed != nullptr) {
            *failed = true;
            return _emptyStr;
        }

        throw EnumResolveException("Can't resolve value for enum", enum_name, value);
    }

    return value_it->second;
}

auto EngineMetadata::ResolveGenericValue(string_view str, bool* failed) const -> int32
{
    FO_STACK_TRACE_ENTRY();

    if (str.empty()) {
        return 0;
    }
    if (str[0] == '"') {
        return 0;
    }

    if (str[0] == '@') {
        return Hashes.ToHashedString(str.substr(1)).as_int32();
    }
    else if (str.starts_with("Content::")) {
        return Hashes.ToHashedString(str.substr(str.rfind(':') + 1)).as_int32();
    }
    else if (strex(str).is_number()) {
        return strex(str).to_int32();
    }
    else if (strex(str).compare_ignore_case("true")) {
        return 1;
    }
    else if (strex(str).compare_ignore_case("false")) {
        return 0;
    }

    bool enum_failed = false;
    const auto enum_value = ResolveEnumValue(str, &enum_failed);

    if (enum_failed) {
        WriteLog("Failed to resolve generic value: '{}'", str);

        if (failed != nullptr) {
            *failed = true;
            return 0;
        }
    }

    return enum_value;
}

auto EngineMetadata::GetGameSetting(string_view name) const -> const BaseTypeDesc&
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _gameSettings.find(name);

    if (it == _gameSettings.end()) {
        throw TypeResolveException("Setting not found", name);
    }

    return *it->second.get();
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

BaseEngine::BaseEngine(GlobalSettings& settings, FileSystem&& resources, const MeatdataRegistrator& registrator) :
    EngineMetadata(registrator),
    ScriptSystem(),
    Entity(GetPropertyRegistrator(ENTITY_TYPE_NAME), nullptr),
    GameProperties(GetInitRef()),
    Settings {settings},
    Resources {std::move(resources)},
    Geometry(settings),
    GameTime(settings),
    ProtoMngr(*this),
    TimeEventMngr(*this)
{
    FO_STACK_TRACE_ENTRY();
}

void BaseEngine::FrameAdvance()
{
    FO_STACK_TRACE_ENTRY();

    GameTime.FrameAdvance();

    SetFrameTime(GameTime.GetFrameTime());
    SetFrameDeltaTime(GameTime.GetFrameDeltaTime());
    SetFramesPerSecond(GameTime.GetFramesPerSecond());

    if (GameTime.IsTimeSynchronized()) {
        SetSynchronizedTime(GameTime.GetSynchronizedTime());
    }
}

void BaseEngine::SendRemoteCall(hstring name, Entity* caller, const_span<uint8> data)
{
    FO_STACK_TRACE_ENTRY();

    HandleOutboundRemoteCall(name, caller, data);
}

void BaseEngine::SetRemoteCallHandler(hstring name, RemoteCallHandler handler)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_inboundRemoteCallHandlers.contains(name));

    _inboundRemoteCallHandlers[name] = std::move(handler);
}

void BaseEngine::VerifyBindedRemoteCalls() const noexcept(false)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_inboundRemoteCallHandlers.size() == GetInboundRemoteCalls().size());
}

void BaseEngine::HandleInboundRemoteCall(hstring name, Entity* caller, span<uint8> data)
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _inboundRemoteCallHandlers.find(name);

    if (it == _inboundRemoteCallHandlers.end()) {
        throw GenericException("Invalid remote call", name);
    }

    it->second(name, caller, data);
}

FO_END_NAMESPACE
