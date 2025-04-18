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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"

FOEngineBase::FOEngineBase(GlobalSettings& settings, PropertiesRelationType props_relation) :
    Entity(SafeAlloc::MakeRaw<PropertyRegistrator>(ENTITY_TYPE_NAME, props_relation, *this, *this), nullptr),
    GameProperties(GetInitRef()),
    Settings {settings},
    Geometry(settings),
    GameTime(settings),
    ProtoMngr(this),
    ScriptSys(SafeAlloc::MakeUnique<ScriptSystem>()),
    TimeEventMngr(SafeAlloc::MakeUnique<TimeEventManager>(&GameTime, ScriptSys.get())),
    _propsRelation {props_relation}
{
    STACK_TRACE_ENTRY();

    _entityTypesInfo.emplace(FOEngineBase::ToHashedString(ENTITY_TYPE_NAME), EntityTypeInfo {_propsRef.GetRegistrator(), true, false});
}

auto FOEngineBase::RegisterEntityType(string_view type_name, bool exported, bool has_protos) -> PropertyRegistrator*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _entityTypesInfo.find(ToHashedString(type_name));
    RUNTIME_ASSERT(it == _entityTypesInfo.end());

    auto* registrator = SafeAlloc::MakeRaw<PropertyRegistrator>(type_name, _propsRelation, *this, *this);

    _entityTypesInfo.emplace(ToHashedString(type_name), EntityTypeInfo {registrator, exported, has_protos});

    return registrator;
}

void FOEngineBase::RegsiterEntityHolderEntry(string_view holder_type, string_view target_type, string_view entry, EntityHolderEntryAccess access)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(IsValidEntityType(ToHashedString(target_type)));

    const auto it = _entityTypesInfo.find(ToHashedString(holder_type));
    RUNTIME_ASSERT(it != _entityTypesInfo.end());
    RUNTIME_ASSERT(it->second.HolderEntries.count(ToHashedString(entry)) == 0);

    it->second.HolderEntries.emplace(ToHashedString(entry), tuple {ToHashedString(target_type), access});
}

void FOEngineBase::RegisterEnumGroup(string_view name, BaseTypeInfo underlying_type, unordered_map<string, int>&& key_values)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);
    RUNTIME_ASSERT(_enums.count(name) == 0);

    unordered_map<int, string> key_values_rev;

    for (auto&& [key, value] : key_values) {
        RUNTIME_ASSERT(key_values_rev.count(value) == 0);
        key_values_rev[value] = key;
        const string full_key = strex("{}::{}", name, key);
        RUNTIME_ASSERT(_enumsFull.count(full_key) == 0);
        _enumsFull[full_key] = value;
    }

    _enums[name] = std::move(key_values);
    _enumsRev[name] = std::move(key_values_rev);
    _enumTypes[name] = std::move(underlying_type);
}

void FOEngineBase::RegisterValueType(string_view name, size_t size, BaseTypeInfo::StructLayoutInfo&& layout)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);
    RUNTIME_ASSERT(_valueTypes.count(name) == 0);
    RUNTIME_ASSERT(size == std::accumulate(layout.begin(), layout.end(), static_cast<size_t>(0), [&](const size_t& sum, const BaseTypeInfo::StructLayoutEntry& e) { return sum + e.second.Size; }));

    _valueTypes.emplace(name, tuple {size, std::move(layout)});
}

void FOEngineBase::RegisterMigrationRules(unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>&& migration_rules)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);

    _migrationRules = std::move(migration_rules);
}

void FOEngineBase::FinalizeDataRegistration()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);

    _registrationFinalized = true;

    GetPropertiesForEdit().AllocData();
}

auto FOEngineBase::GetPropertyRegistrator(hstring type_name) const noexcept -> const PropertyRegistrator*
{
    STACK_TRACE_ENTRY();

    const auto it = _entityTypesInfo.find(type_name);

    return it != _entityTypesInfo.end() ? it->second.PropRegistrator : nullptr;
}

auto FOEngineBase::GetPropertyRegistrator(string_view type_name) const noexcept -> const PropertyRegistrator*
{
    STACK_TRACE_ENTRY();

    hstring type_name_hashed;

    try {
        type_name_hashed = ToHashedStringMustExists(type_name);
    }
    catch (...) {
        return nullptr;
    }

    return GetPropertyRegistrator(type_name_hashed);
}

auto FOEngineBase::GetPropertyRegistratorForEdit(string_view type_name) -> PropertyRegistrator*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _entityTypesInfo.find(ToHashedString(type_name));
    RUNTIME_ASSERT(it != _entityTypesInfo.end());

    return const_cast<PropertyRegistrator*>(it->second.PropRegistrator);
}

auto FOEngineBase::IsValidEntityType(hstring type_name) const noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    return _entityTypesInfo.find(type_name) != _entityTypesInfo.end();
}

auto FOEngineBase::GetEntityTypeInfo(hstring type_name) const -> const EntityTypeInfo&
{
    NO_STACK_TRACE_ENTRY();

    const auto it = _entityTypesInfo.find(type_name);
    RUNTIME_ASSERT(it != _entityTypesInfo.end());

    return it->second;
}

auto FOEngineBase::GetEntityTypesInfo() const noexcept -> const unordered_map<hstring, EntityTypeInfo>&
{
    NO_STACK_TRACE_ENTRY();

    return _entityTypesInfo;
}

auto FOEngineBase::GetEntityHolderIdsProp(Entity* holder, hstring entry) const -> const Property*
{
    STACK_TRACE_ENTRY();

    const auto prop_name = ToHashedStringMustExists(strex("{}Ids", entry));
    const auto* holder_prop = holder->GetProperties().GetRegistrator()->FindProperty(prop_name);
    RUNTIME_ASSERT(holder_prop);

    return holder_prop;
}

auto FOEngineBase::ResolveBaseType(string_view type_str) const -> BaseTypeInfo
{
    STACK_TRACE_ENTRY();

    static unordered_map<string_view, std::function<void(BaseTypeInfo&)>> builtin_types = {//
        {"int8",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsInt8 = true;
                info.IsSignedInt = true;
                info.Size = sizeof(int8);
            }},
        {"int16",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsInt16 = true;
                info.IsSignedInt = true;
                info.Size = sizeof(int16);
            }},
        {"int",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsInt32 = true;
                info.IsSignedInt = true;
                info.Size = sizeof(int);
            }},
        {"int64",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsInt64 = true;
                info.IsSignedInt = true;
                info.Size = sizeof(int64);
            }},
        {"uint8",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsUInt8 = true;
                info.IsSignedInt = false;
                info.Size = sizeof(uint8);
            }},
        {"uint16",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsUInt16 = true;
                info.IsSignedInt = false;
                info.Size = sizeof(uint16);
            }},
        {"uint",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsUInt32 = true;
                info.IsSignedInt = false;
                info.Size = sizeof(uint);
            }},
        {"uint64",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsUInt64 = true;
                info.IsSignedInt = false;
                info.Size = sizeof(uint64);
            }},
        {"float",
            [](BaseTypeInfo& info) {
                info.IsFloat = true;
                info.IsSingleFloat = true;
                info.Size = sizeof(float);
            }},
        {"double",
            [](BaseTypeInfo& info) {
                info.IsFloat = true;
                info.IsDoubleFloat = true;
                info.Size = sizeof(double);
            }},
        {"bool",
            [](BaseTypeInfo& info) {
                info.IsBool = true;
                info.Size = sizeof(bool);
            }},
        {"string",
            [](BaseTypeInfo& info) {
                info.IsString = true;
                info.Size = 0;
            }},
        {"any",
            [](BaseTypeInfo& info) {
                info.IsString = true;
                info.Size = 0;
            }},
        {"hstring", [](BaseTypeInfo& info) {
             info.IsHash = true;
             info.Size = sizeof(hstring::hash_t);
         }}};

    RUNTIME_ASSERT(!type_str.empty());

    BaseTypeInfo info;

    info.TypeName = type_str;

    if (const auto it = builtin_types.find(type_str); it != builtin_types.end()) {
        it->second(info);
    }
    else if (const BaseTypeInfo * underlying_type; GetEnumInfo(info.TypeName, &underlying_type)) {
        info.IsEnum = true;
        info.IsEnumSigned = underlying_type->IsSignedInt;
        info.Size = underlying_type->Size;
    }
    else if (GetValueTypeInfo(info.TypeName, info.Size, &info.StructLayout)) {
        if (info.StructLayout->size() > 1) {
            info.IsStruct = true;
        }
        else {
            info = info.StructLayout->front().second;
            info.TypeName = type_str;
        }
    }
    else {
        throw GenericException("Invalid base type", type_str);
    }

    info.IsPrimitive = info.IsInt || info.IsFloat || info.IsBool;

    return info;
}

auto FOEngineBase::GetEnumInfo(string_view enum_name, const BaseTypeInfo** underlying_type) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto enum_it = _enumTypes.find(enum_name);

    if (enum_it == _enumTypes.end()) {
        return false;
    }

    if (underlying_type != nullptr) {
        *underlying_type = &enum_it->second;
    }

    return true;
}

auto FOEngineBase::GetValueTypeInfo(string_view type_name, size_t& size, const BaseTypeInfo::StructLayoutInfo** layout) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto it = _valueTypes.find(type_name);

    if (it == _valueTypes.end()) {
        return false;
    }

    size = std::get<0>(it->second);

    if (layout != nullptr) {
        *layout = &std::get<1>(it->second);
    }

    return true;
}

auto FOEngineBase::ResolveEnumValue(string_view enum_value_name, bool* failed) const -> int
{
    STACK_TRACE_ENTRY();

    const auto it = _enumsFull.find(enum_value_name);

    if (it == _enumsFull.end()) {
        if (failed != nullptr) {
            WriteLog("Invalid enum full value {}", enum_value_name);
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum full value", enum_value_name);
    }

    return it->second;
}

auto FOEngineBase::ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed) const -> int
{
    STACK_TRACE_ENTRY();

    const auto enum_it = _enums.find(enum_name);

    if (enum_it == _enums.end()) {
        if (failed != nullptr) {
            WriteLog("Invalid enum {}", enum_name);
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum", enum_name, value_name);
    }

    const auto value_it = enum_it->second.find(value_name);

    if (value_it == enum_it->second.end()) {
        if (failed != nullptr) {
            WriteLog("Can't resolve {} for enum {}", value_name, enum_name);
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum value", enum_name, value_name);
    }

    return value_it->second;
}

auto FOEngineBase::ResolveEnumValueName(string_view enum_name, int value, bool* failed) const -> const string&
{
    STACK_TRACE_ENTRY();

    const auto enum_it = _enumsRev.find(enum_name);

    if (enum_it == _enumsRev.end()) {
        if (failed != nullptr) {
            WriteLog("Invalid enum {} for resolve value", enum_name);
            *failed = true;
            return _emptyStr;
        }

        throw EnumResolveException("Invalid enum for resolve value", enum_name, value);
    }

    const auto value_it = enum_it->second.find(value);

    if (value_it == enum_it->second.end()) {
        if (failed != nullptr) {
            WriteLog("Can't resolve value {} for enum {}", value, enum_name);
            *failed = true;
            return _emptyStr;
        }

        throw EnumResolveException("Can't resolve value for enum", enum_name, value);
    }

    return value_it->second;
}

auto FOEngineBase::ResolveGenericValue(string_view str, bool* failed) -> int
{
    STACK_TRACE_ENTRY();

    if (str.empty()) {
        return 0;
    }
    if (str[0] == '"') {
        return 0;
    }

    if (str[0] == '@') {
        return ToHashedString(str.substr(1)).as_int();
    }
    else if (str[0] == 'C' && str.length() >= 9 && str.compare(0, 9, "Content::") == 0) {
        return ToHashedString(str.substr(str.rfind(':') + 1)).as_int();
    }
    else if (strex(str).isNumber()) {
        return strex(str).toInt();
    }
    else if (strex(str).compareIgnoreCase("true")) {
        return 1;
    }
    else if (strex(str).compareIgnoreCase("false")) {
        return 0;
    }

    return ResolveEnumValue(str, failed);
}

auto FOEngineBase::CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring>
{
    STACK_TRACE_ENTRY();

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

void FOEngineBase::FrameAdvance()
{
    STACK_TRACE_ENTRY();

    GameTime.FrameAdvance();

    SetFrameTime(GameTime.GetFrameTime());
    SetFrameDeltaTime(GameTime.GetFrameDeltaTime());
    SetFramesPerSecond(GameTime.GetFramesPerSecond());

    if (GameTime.IsTimeSynchronized()) {
        SetSynchronizedTime(GameTime.GetSynchronizedTime());
    }
}
