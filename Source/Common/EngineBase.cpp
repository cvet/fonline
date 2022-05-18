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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FOEngineBase::FOEngineBase(PropertiesRelationType props_relation, const RegisterDataCallback& register_data_callback) : Entity(new PropertyRegistrator(ENTITY_CLASS_NAME, props_relation, *this)), GameProperties(GetInitRef()), _propsRelation {props_relation}
{
    RUNTIME_ASSERT(props_relation != PropertiesRelationType::None);
    RUNTIME_ASSERT(register_data_callback);

    _registrators.emplace(ENTITY_CLASS_NAME, _propsRef.GetRegistrator());
    ScriptSys = register_data_callback();
    FinalizeDataRegistration();
}

auto FOEngineBase::GetOrCreatePropertyRegistrator(string_view class_name) -> PropertyRegistrator*
{
    RUNTIME_ASSERT(!_registrationFinalized);

    if (_registrationFinalized) {
        return nullptr;
    }

    const auto it = _registrators.find(string(class_name));
    if (it != _registrators.end()) {
        return const_cast<PropertyRegistrator*>(it->second);
    }

    auto* registrator = new PropertyRegistrator(class_name, _propsRelation, *this);
    _registrators.emplace(class_name, registrator);
    return registrator;
}

void FOEngineBase::AddEnumGroup(string_view name, const type_info& underlying_type, unordered_map<string, int>&& key_values)
{
    RUNTIME_ASSERT(!_registrationFinalized);

    if (_registrationFinalized) {
        return;
    }

    RUNTIME_ASSERT(_enums.count(string(name)) == 0u);

    unordered_map<int, string> key_values_rev;
    for (auto&& [key, value] : key_values) {
        RUNTIME_ASSERT(key_values_rev.count(value) == 0u);
        key_values_rev[value] = key;
        const auto full_key = _str("{}::{}", name, key).str();
        RUNTIME_ASSERT(_enumsFull.count(full_key) == 0u);
        _enumsFull[full_key] = value;
    }

    _enums[string(name)] = std::move(key_values);
    _enumsRev[string(name)] = std::move(key_values_rev);
    _enumTypes[string(name)] = &underlying_type;
}

auto FOEngineBase::GetPropertyRegistrator(string_view class_name) const -> const PropertyRegistrator*
{
    const auto it = _registrators.find(string(class_name));
    RUNTIME_ASSERT(it != _registrators.end());
    return it->second;
}

void FOEngineBase::FinalizeDataRegistration()
{
    RUNTIME_ASSERT(!_registrationFinalized);

    _registrationFinalized = true;
}

auto FOEngineBase::ResolveEnumValue(string_view enum_value_name, bool* failed) const -> int
{
    const auto it = _enumsFull.find(string(enum_value_name));
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
    const auto enum_it = _enums.find(string(enum_name));
    if (enum_it == _enums.end()) {
        if (failed != nullptr) {
            WriteLog("Invalid enum {}", enum_name);
            *failed = true;
            return 0;
        }

        throw EnumResolveException("Invalid enum", enum_name, value_name);
    }

    const auto value_it = enum_it->second.find(string(value_name));
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

auto FOEngineBase::ResolveEnumValueName(string_view enum_name, int value, bool* failed) const -> string
{
    const auto enum_it = _enumsRev.find(string(enum_name));
    if (enum_it == _enumsRev.end()) {
        if (failed != nullptr) {
            WriteLog("Invalid enum {} for resolve value", enum_name);
            *failed = true;
            return string();
        }

        throw EnumResolveException("Invalid enum for resolve value", enum_name, value);
    }

    const auto value_it = enum_it->second.find(value);
    if (value_it == enum_it->second.end()) {
        if (failed != nullptr) {
            WriteLog("Can't resolve value {} for enum {}", value, enum_name);
            *failed = true;
            return string();
        }

        throw EnumResolveException("Can't resolve value for enum", enum_name, value);
    }

    return value_it->second;
}

auto FOEngineBase::ToHashedString(string_view s) const -> hstring
{
    static_assert(std::is_same_v<hstring::hash_t, decltype(Hashing::MurmurHash2({}, {}))>);

    if (s.empty()) {
        return hstring();
    }

    const auto hash_value = Hashing::MurmurHash2(s.data(), s.length());
    RUNTIME_ASSERT(hash_value != 0u);

    if (const auto it = _hashStorage.find(hash_value); it != _hashStorage.end()) {
#if FO_DEBUG
        const auto collision_detected = (s != it->second.Str);
#else
        const auto collision_detected = (s.length() != it->second.Str.length() && s != it->second.Str);
#endif
        if (collision_detected) {
            throw HashCollisionException("Hash collision", s, it->second.Str, hash_value);
        }

        return hstring(&it->second);
    }

    const auto [it, inserted] = _hashStorage.emplace(hash_value, hstring::entry {hash_value, string(s)});
    RUNTIME_ASSERT(inserted);

    return hstring(&it->second);
}

auto FOEngineBase::ResolveHash(hstring::hash_t h, bool* failed) const -> hstring
{
    if (const auto it = _hashStorage.find(h); it != _hashStorage.end()) {
        return hstring(&it->second);
    }

    if (failed != nullptr) {
        WriteLog("Can't resolve hash {}", h);
        *failed = true;
        return hstring();
    }

    throw HashResolveException("Can't resolve hash", h);
}

auto FOEngineBase::ResolveGenericValue(string_view str, bool* failed) -> int
{
    if (str.empty()) {
        return 0;
    }

    if (str[0] == '@') {
        return ToHashedString(str.substr(1)).as_int();
    }
    else if (str[0] == 'C' && str.length() >= 9 && str.compare(0, 9, "Content::") == 0) {
        return ToHashedString(str.substr(str.rfind(':') + 1)).as_int();
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
