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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    Entity(new PropertyRegistrator(ENTITY_CLASS_NAME, props_relation, *this, *this), nullptr),
    GameProperties(GetInitRef()),
    Settings {settings},
    Geometry(settings),
    GameTime(settings),
    ProtoMngr(this),
    _propsRelation {props_relation}
{
    STACK_TRACE_ENTRY();

    _registrators.emplace(ENTITY_CLASS_NAME, _propsRef.GetRegistrator());
}

auto FOEngineBase::GetOrCreatePropertyRegistrator(string_view class_name) -> PropertyRegistrator*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);

    const auto it = _registrators.find(string(class_name));
    if (it != _registrators.end()) {
        return const_cast<PropertyRegistrator*>(it->second);
    }

    auto* registrator = new PropertyRegistrator(class_name, _propsRelation, *this, *this);
    _registrators.emplace(class_name, registrator);
    return registrator;
}

void FOEngineBase::AddEnumGroup(const string& name, size_t size, unordered_map<string, int>&& key_values)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);
    RUNTIME_ASSERT(_enums.count(name) == 0);

    unordered_map<int, string> key_values_rev;

    for (auto&& [key, value] : key_values) {
        RUNTIME_ASSERT(key_values_rev.count(value) == 0);
        key_values_rev[value] = key;
        const auto full_key = _str("{}::{}", name, key).str();
        RUNTIME_ASSERT(_enumsFull.count(full_key) == 0);
        _enumsFull[full_key] = value;
    }

    _enums[name] = std::move(key_values);
    _enumsRev[name] = std::move(key_values_rev);
    _enumSizes[name] = size;
}

void FOEngineBase::AddAggregatedType(const string& name, size_t size, vector<BaseTypeInfo>&& layout)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!_registrationFinalized);
    RUNTIME_ASSERT(_aggregatedTypes.count(name) == 0);
    RUNTIME_ASSERT(size == std::accumulate(layout.begin(), layout.end(), static_cast<size_t>(0), [&](const size_t& sum, const BaseTypeInfo& t) { return sum + t.Size; }));

    _aggregatedTypes.emplace(name, tuple {size, std::move(layout)});
}

void FOEngineBase::SetMigrationRules(unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>&& migration_rules)
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

auto FOEngineBase::GetPropertyRegistrator(string_view class_name) const -> const PropertyRegistrator*
{
    STACK_TRACE_ENTRY();

    const auto it = _registrators.find(string(class_name));
    RUNTIME_ASSERT(it != _registrators.end());
    return it->second;
}

auto FOEngineBase::ResolveBaseType(string_view type_str) const -> BaseTypeInfo
{
    STACK_TRACE_ENTRY();

    static unordered_map<string_view, std::function<void(BaseTypeInfo&)>> base_types = {//
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
                info.IsInt8 = true;
                info.IsSignedInt = false;
                info.Size = sizeof(uint8);
            }},
        {"uint16",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsInt16 = true;
                info.IsSignedInt = false;
                info.Size = sizeof(uint16);
            }},
        {"uint",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsInt32 = true;
                info.IsSignedInt = false;
                info.Size = sizeof(uint);
            }},
        {"uint64",
            [](BaseTypeInfo& info) {
                info.IsInt = true;
                info.IsInt64 = true;
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

    if (const auto it = base_types.find(type_str); it != base_types.end()) {
        it->second(info);
    }
    else if (size_t enum_size; GetEnumInfo(info.TypeName, enum_size)) {
        info.IsEnum = true;
        info.Size = static_cast<uint>(enum_size);
    }
    else if (size_t type_size; GetAggregatedTypeInfo(info.TypeName, type_size, nullptr)) {
        info.IsAggregatedType = true;
        info.Size = static_cast<uint>(type_size);
    }
    else {
        throw GenericException("Invalid base type", type_str);
    }

    return info;
}

auto FOEngineBase::GetEnumInfo(const string& enum_name, size_t& size) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto enum_it = _enumSizes.find(enum_name);

    if (enum_it == _enumSizes.end()) {
        return false;
    }

    size = enum_it->second;
    RUNTIME_ASSERT(size != 0);

    return true;
}

auto FOEngineBase::GetAggregatedTypeInfo(const string& type_name, size_t& size, const vector<BaseTypeInfo>** layout) const -> bool
{
    STACK_TRACE_ENTRY();

    const auto it = _aggregatedTypes.find(type_name);

    if (it == _aggregatedTypes.end()) {
        return false;
    }

    size = std::get<0>(it->second);

    if (layout != nullptr) {
        *layout = &std::get<1>(it->second);
    }

    return true;
}

auto FOEngineBase::ResolveEnumValue(const string& enum_value_name, bool* failed) const -> int
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

auto FOEngineBase::ResolveEnumValue(const string& enum_name, const string& value_name, bool* failed) const -> int
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

auto FOEngineBase::ResolveEnumValueName(const string& enum_name, int value, bool* failed) const -> const string&
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

auto FOEngineBase::ResolveGenericValue(const string& str, bool* failed) -> int
{
    STACK_TRACE_ENTRY();

    if (str.empty()) {
        return 0;
    }
    if (str[0] == '"') {
        return 0;
    }

    if (str[0] == '@') {
        return ToHashedString(string_view(str).substr(1)).as_int();
    }
    else if (str[0] == 'C' && str.length() >= 9 && str.compare(0, 9, "Content::") == 0) {
        return ToHashedString(string_view(str).substr(str.rfind(':') + 1)).as_int();
    }
    else if (_str(str).isNumber()) {
        return _str(str).toInt();
    }
    else if (_str(str).compareIgnoreCase("true")) {
        return 1;
    }
    else if (_str(str).compareIgnoreCase("false")) {
        return 0;
    }

    return ResolveEnumValue(str, failed);
}

auto FOEngineBase::CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const -> optional<hstring>
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
