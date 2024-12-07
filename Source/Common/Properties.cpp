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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Properties.h"
#include "Log.h"
#include "PropertiesSerializator.h"
#include "StringUtils.h"

auto PropertyRawData::GetPtr() noexcept -> void*
{
    NO_STACK_TRACE_ENTRY();

    if (_passedPtr != nullptr) {
        return _passedPtr;
    }

    return _useDynamic ? _dynamicBuf.get() : _localBuf;
}

auto PropertyRawData::Alloc(size_t size) -> uint8*
{
    NO_STACK_TRACE_ENTRY();

    _dataSize = size;
    _passedPtr = nullptr;

    if (size > LOCAL_BUF_SIZE) {
        _useDynamic = true;
        _dynamicBuf.reset(new uint8[size]);
    }
    else {
        _useDynamic = false;
    }

    return static_cast<uint8*>(GetPtr());
}

void PropertyRawData::Pass(const_span<uint8> value)
{
    NO_STACK_TRACE_ENTRY();

    _passedPtr = const_cast<uint8*>(value.data());
    _dataSize = value.size();
    _useDynamic = false;
}

void PropertyRawData::Pass(const void* value, size_t size)
{
    NO_STACK_TRACE_ENTRY();

    _passedPtr = static_cast<uint8*>(const_cast<void*>(value));
    _dataSize = size;
    _useDynamic = false;
}

void PropertyRawData::StoreIfPassed()
{
    NO_STACK_TRACE_ENTRY();

    if (_passedPtr != nullptr) {
        PropertyRawData tmp_data;
        tmp_data.Set(_passedPtr, _dataSize);
        *this = std::move(tmp_data);
    }
}

Property::Property(const PropertyRegistrator* registrator) :
    _registrator {registrator}
{
    STACK_TRACE_ENTRY();
}

void Property::SetGetter(PropertyGetCallback getter) const
{
    STACK_TRACE_ENTRY();

    _getter = std::move(getter);
}

void Property::AddSetter(PropertySetCallback setter) const
{
    STACK_TRACE_ENTRY();

    _setters.emplace(_setters.begin(), std::move(setter));
}

void Property::AddPostSetter(PropertyPostSetCallback setter) const
{
    STACK_TRACE_ENTRY();

    _postSetters.emplace(_postSetters.begin(), std::move(setter));
}

Properties::Properties(const PropertyRegistrator* registrator) :
    _registrator {registrator}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_registrator);

    if (!_registrator->_registeredProperties.empty()) {
        AllocData();
    }
}

Properties::Properties(const Properties& other) :
    Properties(other._registrator)
{
    STACK_TRACE_ENTRY();

    std::memcpy(_podData.data(), other._podData.data(), _registrator->_wholePodDataSize);

    for (size_t i = 0; i < other._complexData.size(); i++) {
        if (!other._complexData[i].empty()) {
            _complexData[i].reserve(other._complexData[i].size());
            _complexData[i].resize(other._complexData[i].size());
            std::memcpy(_complexData[i].data(), other._complexData[i].data(), other._complexData[i].size());
        }
    }
}

auto Properties::operator=(const Properties& other) -> Properties&
{
    STACK_TRACE_ENTRY();

    if (this == &other) {
        return *this;
    }

    RUNTIME_ASSERT(_registrator == other._registrator);

    // Copy PlainData data
    std::memcpy(_podData.data(), other._podData.data(), _registrator->_wholePodDataSize);

    // Copy complex data
    for (const auto* prop : _registrator->_complexProperties) {
        SetRawData(prop, other._complexData[prop->_complexDataIndex].data(), static_cast<uint>(other._complexData[prop->_complexDataIndex].size()));
    }

    return *this;
}

void Properties::AllocData()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_podData.empty());
    RUNTIME_ASSERT(!_registrator->_registeredProperties.empty());

    _podData.reserve(_registrator->_wholePodDataSize);
    _podData.resize(_registrator->_wholePodDataSize);

    _complexData.reserve(_registrator->_complexProperties.size());
    _complexData.resize(_registrator->_complexProperties.size());
}

void Properties::StoreAllData(vector<uint8>& all_data, set<hstring>& str_hashes) const
{
    STACK_TRACE_ENTRY();

    all_data.clear();
    auto writer = DataWriter(all_data);

    // Store plain properties data
    writer.Write<uint>(_registrator->_wholePodDataSize);

    int start_pos = -1;

    for (uint i = 0; i < _registrator->_wholePodDataSize; i++) {
        if (_podData[i] != 0) {
            if (start_pos == -1) {
                start_pos = static_cast<int>(i);
            }

            i += 3;
        }
        else {
            if (start_pos != -1) {
                const auto len = i - start_pos;
                writer.Write<uint>(start_pos);
                writer.Write<uint>(len);
                writer.WritePtr(_podData.data() + start_pos, len);

                start_pos = -1;
            }
        }
    }

    if (start_pos != -1) {
        const auto len = _registrator->_wholePodDataSize - start_pos;
        writer.Write<uint>(start_pos);
        writer.Write<uint>(len);
        writer.WritePtr(_podData.data() + start_pos, len);
    }

    writer.Write<uint>(0);
    writer.Write<uint>(0);

    // Store complex properties
    writer.Write<uint>(static_cast<uint>(_registrator->_complexProperties.size()));
    for (const auto* prop : _registrator->_complexProperties) {
        RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        writer.Write<uint>(static_cast<uint>(_complexData[prop->_complexDataIndex].size()));
        writer.WritePtr(_complexData[prop->_complexDataIndex].data(), _complexData[prop->_complexDataIndex].size());
    }

    // Store hashes
    const auto add_hash = [&str_hashes, this](const string& str) {
        if (!str.empty()) {
            const auto hstr = _registrator->_hashResolver.ToHashedString(str);
            str_hashes.emplace(hstr);
        }
    };

    for (const auto* prop : _registrator->_registeredProperties) {
        if (!prop->IsDisabled() && (prop->IsBaseTypeHash() || prop->IsDictKeyHash())) {
            const auto value = PropertiesSerializator::SavePropertyToValue(this, prop, _registrator->_hashResolver, _registrator->_nameResolver);

            if (value.index() == AnyData::STRING_VALUE) {
                add_hash(std::get<AnyData::STRING_VALUE>(value));
            }
            else if (value.index() == AnyData::ARRAY_VALUE) {
                const auto arr = std::get<AnyData::ARRAY_VALUE>(value);
                for (auto&& arr_entry : arr) {
                    add_hash(std::get<AnyData::STRING_VALUE>(arr_entry));
                }
            }
            else if (value.index() == AnyData::DICT_VALUE) {
                const auto dict = std::get<AnyData::DICT_VALUE>(value);

                if (prop->IsDictKeyHash()) {
                    for (auto&& dict_entry : dict) {
                        add_hash(dict_entry.first);
                    }
                }

                if (prop->IsBaseTypeHash()) {
                    for (auto&& dict_entry : dict) {
                        if (dict_entry.second.index() == AnyData::ARRAY_VALUE) {
                            const auto dict_arr = std::get<AnyData::ARRAY_VALUE>(dict_entry.second);
                            for (auto&& dict_arr_entry : dict_arr) {
                                add_hash(std::get<AnyData::STRING_VALUE>(dict_arr_entry));
                            }
                        }
                        else {
                            add_hash(std::get<AnyData::STRING_VALUE>(dict_entry.second));
                        }
                    }
                }
            }
            else {
                throw UnreachablePlaceException(LINE_STR);
            }
        }
    }
}

void Properties::RestoreAllData(const vector<uint8>& all_data)
{
    STACK_TRACE_ENTRY();

    auto reader = DataReader(all_data);

    // Read plain properties data
    const auto whole_pod_data_size = reader.Read<uint>();
    RUNTIME_ASSERT_STR(whole_pod_data_size == _registrator->_wholePodDataSize, "Run ForceBakeResources");

    while (true) {
        const auto start_pos = reader.Read<uint>();
        const auto len = reader.Read<uint>();
        if (start_pos == 0 && len == 0) {
            break;
        }

        std::memcpy(_podData.data() + start_pos, reader.ReadPtr<uint8>(len), len);
    }

    // Read complex properties
    const auto complex_props_count = reader.Read<uint>();
    RUNTIME_ASSERT(complex_props_count == _registrator->_complexProperties.size());
    for (const auto* prop : _registrator->_complexProperties) {
        RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto data_size = reader.Read<uint>();
        SetRawData(prop, reader.ReadPtr<uint8>(data_size), data_size);
    }

    reader.VerifyEnd();
}

void Properties::StoreData(bool with_protected, vector<const uint8*>** all_data, vector<uint>** all_data_sizes) const
{
    STACK_TRACE_ENTRY();

    make_if_not_exists(_storeData);
    make_if_not_exists(_storeDataSizes);
    make_if_not_exists(_storeDataComplexIndices);

    *all_data = &*_storeData;
    *all_data_sizes = &*_storeDataSizes;
    _storeData->resize(0);
    _storeDataSizes->resize(0);

    *_storeDataComplexIndices = with_protected ? _registrator->_publicProtectedComplexDataProps : _registrator->_publicComplexDataProps;

    const auto preserve_size = 1u + (!_storeDataComplexIndices->empty() ? 1u + _storeDataComplexIndices->size() : 0);
    _storeData->reserve(preserve_size);
    _storeDataSizes->reserve(preserve_size);

    // Store plain properties data
    _storeData->push_back(_podData.data());
    _storeDataSizes->push_back(static_cast<uint>(_registrator->_publicPodDataSpace.size()) + (with_protected ? static_cast<uint>(_registrator->_protectedPodDataSpace.size()) : 0));

    // Filter complex data to send
    for (size_t i = 0; i < _storeDataComplexIndices->size();) {
        const auto* prop = _registrator->_registeredProperties[(*_storeDataComplexIndices)[i]];
        RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        if (_complexData[prop->_complexDataIndex].empty()) {
            _storeDataComplexIndices->erase(_storeDataComplexIndices->begin() + static_cast<int>(i));
        }
        else {
            i++;
        }
    }

    // Store complex properties data
    if (!_storeDataComplexIndices->empty()) {
        _storeData->push_back(reinterpret_cast<uint8*>(_storeDataComplexIndices->data()));
        _storeDataSizes->push_back(static_cast<uint>(_storeDataComplexIndices->size()) * sizeof(uint16));

        for (const auto index : *_storeDataComplexIndices) {
            const auto* prop = _registrator->_registeredProperties[index];
            _storeData->push_back(_complexData[prop->_complexDataIndex].data());
            _storeDataSizes->push_back(static_cast<uint>(_complexData[prop->_complexDataIndex].size()));
        }
    }
}

void Properties::RestoreData(const vector<const uint8*>& all_data, const vector<uint>& all_data_sizes)
{
    STACK_TRACE_ENTRY();

    // Restore plain data
    RUNTIME_ASSERT(!all_data_sizes.empty());
    RUNTIME_ASSERT(all_data.size() == all_data_sizes.size());
    const auto public_size = static_cast<uint>(_registrator->_publicPodDataSpace.size());
    const auto protected_size = static_cast<uint>(_registrator->_protectedPodDataSpace.size());
    const auto private_size = static_cast<uint>(_registrator->_privatePodDataSpace.size());
    RUNTIME_ASSERT(all_data_sizes[0] == public_size || all_data_sizes[0] == public_size + protected_size || all_data_sizes[0] == public_size + protected_size + private_size);
    if (all_data_sizes[0] != 0) {
        std::memcpy(_podData.data(), all_data[0], all_data_sizes[0]);
    }

    // Restore complex data
    if (all_data.size() > 1) {
        const uint comlplex_data_count = all_data_sizes[1] / sizeof(uint16);
        RUNTIME_ASSERT(comlplex_data_count > 0);
        vector<uint16> complex_indicies(comlplex_data_count);
        std::memcpy(complex_indicies.data(), all_data[1], all_data_sizes[1]);

        for (size_t i = 0; i < complex_indicies.size(); i++) {
            RUNTIME_ASSERT(complex_indicies[i] < _registrator->_registeredProperties.size());
            const auto* prop = _registrator->_registeredProperties[complex_indicies[i]];
            RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
            const auto data_size = all_data_sizes[2 + i];
            const auto* data = all_data[2 + i];
            SetRawData(prop, data, data_size);
        }
    }
}

void Properties::RestoreData(const vector<vector<uint8>>& all_data)
{
    STACK_TRACE_ENTRY();

    vector<const uint8*> all_data_ext(all_data.size());
    vector<uint> all_data_sizes(all_data.size());
    for (size_t i = 0; i < all_data.size(); i++) {
        all_data_ext[i] = !all_data[i].empty() ? all_data[i].data() : nullptr;
        all_data_sizes[i] = static_cast<uint>(all_data[i].size());
    }
    RestoreData(all_data_ext, all_data_sizes);
}

auto Properties::ApplyFromText(const map<string, string>& key_values) -> bool
{
    STACK_TRACE_ENTRY();

    bool is_error = false;

    for (auto&& [key, value] : key_values) {
        // Skip technical fields
        if (key.empty() || key[0] == '$' || key[0] == '_') {
            continue;
        }

        // Find property
        bool is_component;
        const auto* prop = _registrator->Find(key, &is_component);

        if (prop == nullptr) {
            if (is_component) {
                continue;
            }

            WriteLog("Unknown property {}", key);
            is_error = true;
            continue;
        }

        if (prop->_podDataOffset == Property::INVALID_DATA_MARKER && prop->_complexDataIndex == Property::INVALID_DATA_MARKER) {
            if (_registrator->_relation == PropertiesRelationType::ServerRelative && IsEnumSet(prop->_accessType, Property::AccessType::ClientOnlyMask)) {
                continue;
            }
            if (_registrator->_relation == PropertiesRelationType::ClientRelative && IsEnumSet(prop->_accessType, Property::AccessType::ServerOnlyMask)) {
                continue;
            }

            WriteLog("Invalid property {} for reading", prop->GetName());
            is_error = true;
            continue;
        }

        // Parse
        if (!ApplyPropertyFromText(prop, value)) {
            is_error = true;
        }
    }

    return !is_error;
}

auto Properties::SaveToText(const Properties* base) const -> map<string, string>
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!base || _registrator == base->_registrator);

    map<string, string> key_values;

    for (auto* prop : _registrator->_registeredProperties) {
        if (prop->IsDisabled()) {
            continue;
        }
        if (prop->IsVirtual()) {
            continue;
        }
        if (prop->IsTemporary()) {
            continue;
        }

        // Skip same
        if (base != nullptr) {
            if (prop->_podDataOffset != Property::INVALID_DATA_MARKER) {
                const auto* pod_data = &_podData[prop->_podDataOffset];
                const auto* base_pod_data = &base->_podData[prop->_podDataOffset];

                if (std::memcmp(pod_data, base_pod_data, prop->_baseSize) == 0) {
                    continue;
                }
            }
            else {
                const auto& complex_data = _complexData[prop->_complexDataIndex];
                const auto& base_complex_data = base->_complexData[prop->_complexDataIndex];

                if (complex_data.empty() && base_complex_data.empty()) {
                    continue;
                }
                if (complex_data.size() == base_complex_data.size() && std::memcmp(complex_data.data(), base_complex_data.data(), complex_data.size()) == 0) {
                    continue;
                }
            }
        }
        else {
            if (prop->_podDataOffset != Property::INVALID_DATA_MARKER) {
                uint64 pod_zero = 0;
                RUNTIME_ASSERT(prop->_baseSize <= sizeof(pod_zero));

                if (std::memcmp(&_podData[prop->_podDataOffset], &pod_zero, prop->_baseSize) == 0) {
                    continue;
                }
            }
            else {
                if (_complexData[prop->_complexDataIndex].empty()) {
                    continue;
                }
            }
        }

        // Serialize to text and store in map
        key_values.emplace(prop->_propName, SavePropertyToText(prop));
    }

    return key_values;
}

auto Properties::ApplyPropertyFromText(const Property* prop, string_view text) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(_registrator == prop->_registrator);
    RUNTIME_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER || prop->_complexDataIndex != Property::INVALID_DATA_MARKER);

    const auto is_dict = prop->_dataType == Property::DataType::Dict;
    const auto is_array = prop->_dataType == Property::DataType::Array || prop->_isDictOfArray;

    int value_type;
    if (prop->_dataType == Property::DataType::String || prop->_isArrayOfString || prop->_isDictOfArrayOfString || prop->_isHashBase || prop->_isEnumBase) {
        value_type = AnyData::STRING_VALUE;
    }
    else if (prop->_isInt) {
        value_type = AnyData::INT64_VALUE;
    }
    else if (prop->_isBool) {
        value_type = AnyData::BOOL_VALUE;
    }
    else if (prop->_isFloat) {
        value_type = AnyData::DOUBLE_VALUE;
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }

    const auto value = AnyData::ParseValue(string(text), is_dict, is_array, value_type);
    return PropertiesSerializator::LoadPropertyFromValue(this, prop, value, _registrator->_hashResolver, _registrator->_nameResolver);
}

auto Properties::SavePropertyToText(const Property* prop) const -> string
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(_registrator == prop->_registrator);
    RUNTIME_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER || prop->_complexDataIndex != Property::INVALID_DATA_MARKER);

    const auto value = PropertiesSerializator::SavePropertyToValue(this, prop, _registrator->_hashResolver, _registrator->_nameResolver);
    return AnyData::ValueToString(value);
}

void Properties::ValidateForRawData(const Property* prop) const noexcept(false)
{
    NO_STACK_TRACE_ENTRY();

    if (_registrator != prop->_registrator) {
        throw PropertiesException("Invalid property for raw data", prop->GetName(), _registrator->GetTypeName(), prop->_registrator->GetTypeName());
    }

    if (prop->_dataType == Property::DataType::PlainData) {
        if (prop->_podDataOffset == Property::INVALID_DATA_MARKER) {
            throw PropertiesException("Invalid pod data offset for raw data", prop->GetName(), _registrator->GetTypeName());
        }
    }
    else {
        if (prop->_complexDataIndex == Property::INVALID_DATA_MARKER) {
            throw PropertiesException("Invalid complex index for raw data", prop->GetName(), _registrator->GetTypeName());
        }
    }
}

auto Properties::GetRawData(const Property* prop) const noexcept -> const_span<uint8>
{
    NO_STACK_TRACE_ENTRY();

    STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->_dataType == Property::DataType::PlainData) {
        STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        return {&_podData[prop->_podDataOffset], prop->_baseSize};
    }
    else {
        STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto& complex_data = _complexData[prop->_complexDataIndex];
        return {complex_data.data(), complex_data.size()};
    }
}

auto Properties::GetRawData(const Property* prop) noexcept -> span<uint8>
{
    NO_STACK_TRACE_ENTRY();

    STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->_dataType == Property::DataType::PlainData) {
        STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        return {&_podData[prop->_podDataOffset], prop->_baseSize};
    }
    else {
        STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        auto& complex_data = _complexData[prop->_complexDataIndex];
        return {complex_data.data(), complex_data.size()};
    }
}

auto Properties::GetRawDataSize(const Property* prop) const noexcept -> uint
{
    NO_STACK_TRACE_ENTRY();

    STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->_dataType == Property::DataType::PlainData) {
        STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        return prop->_baseSize;
    }
    else {
        STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto& complex_data = _complexData[prop->_complexDataIndex];
        return static_cast<uint>(complex_data.size());
    }
}

void Properties::SetRawData(const Property* prop, const uint8* data, uint data_size)
{
    NO_STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_registrator == prop->_registrator);

    if (prop->_dataType == Property::DataType::PlainData) {
        RUNTIME_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        RUNTIME_ASSERT(prop->GetBaseSize() == data_size);
        std::memcpy(_podData.data() + prop->_podDataOffset, data, data_size);
    }
    else {
        RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        auto& complex_data = _complexData[prop->_complexDataIndex];
        complex_data.resize(data_size); // Todo: add shrink_to_fit complex data for all entities to get some free space on OnLowMemory callback
        std::memcpy(complex_data.data(), data, data_size);
    }
}

void Properties::SetValueFromData(const Property* prop, PropertyRawData& prop_data)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!prop->IsDisabled());

    if (prop->IsVirtual()) {
        RUNTIME_ASSERT(_entity);
        RUNTIME_ASSERT(!prop->_setters.empty());
        for (const auto& setter : prop->_setters) {
            setter(_entity, prop, prop_data);
        }
    }
    else {
        if (!prop->_setters.empty() && _entity != nullptr) {
            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }

        SetRawData(prop, prop_data.GetPtrAs<uint8>(), prop_data.GetSize());

        if (_entity != nullptr) {
            for (const auto& setter : prop->_postSetters) {
                setter(_entity, prop);
            }
        }
    }
}

auto Properties::GetPlainDataValueAsInt(const Property* prop) const -> int
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    else if (prop->_isFloat) {
        if (prop->_isSingleFloat) {
            return static_cast<int>(GetValue<float>(prop));
        }
        if (prop->_isDoubleFloat) {
            return static_cast<int>(GetValue<double>(prop));
        }
    }
    else if (prop->_isInt && prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            return static_cast<int>(GetValue<char>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<int>(GetValue<int16>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<int>(GetValue<int>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<int>(GetValue<int64>(prop));
        }
    }
    else if (prop->_isInt && !prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            return static_cast<int>(GetValue<uint8>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<int>(GetValue<uint16>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<int>(GetValue<uint>(prop));
        }
    }

    throw PropertiesException("Invalid property for get as int", prop->GetName());
}

auto Properties::GetPlainDataValueAsAny(const Property* prop) const -> any_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        return any_t {strex("{}", GetValue<bool>(prop))};
    }
    else if (prop->_isFloat) {
        if (prop->_isSingleFloat) {
            return any_t {strex("{}", GetValue<float>(prop))};
        }
        if (prop->_isDoubleFloat) {
            return any_t {strex("{}", GetValue<double>(prop))};
        }
    }
    else if (prop->_isInt && prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            return any_t {strex("{}", GetValue<char>(prop))};
        }
        if (prop->_baseSize == 2) {
            return any_t {strex("{}", GetValue<int16>(prop))};
        }
        if (prop->_baseSize == 4) {
            return any_t {strex("{}", GetValue<int>(prop))};
        }
        if (prop->_baseSize == 8) {
            return any_t {strex("{}", GetValue<int64>(prop))};
        }
    }
    else if (prop->_isInt && !prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            return any_t {strex("{}", GetValue<uint8>(prop))};
        }
        if (prop->_baseSize == 2) {
            return any_t {strex("{}", GetValue<uint16>(prop))};
        }
        if (prop->_baseSize == 4) {
            return any_t {strex("{}", GetValue<uint>(prop))};
        }
    }

    throw PropertiesException("Invalid property for get as any", prop->GetName());
}

void Properties::SetPlainDataValueAsInt(const Property* prop, int value)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (prop->_isFloat) {
        if (prop->_isSingleFloat) {
            SetValue<float>(prop, static_cast<float>(value));
        }
        else if (prop->_isDoubleFloat) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (prop->_isInt && prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<char>(prop, static_cast<char>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<int16>(prop, static_cast<int16>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, value);
        }
        else if (prop->_baseSize == 8) {
            SetValue<int64>(prop, static_cast<int64>(value));
        }
    }
    else if (prop->_isInt && !prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, static_cast<uint>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int", prop->GetName());
    }
}

void Properties::SetPlainDataValueAsAny(const Property* prop, const any_t& value)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        SetValue<bool>(prop, strex(value).toBool());
    }
    else if (prop->_isFloat) {
        if (prop->_isSingleFloat) {
            SetValue<float>(prop, strex(value).toFloat());
        }
        else if (prop->_isDoubleFloat) {
            SetValue<double>(prop, strex(value).toDouble());
        }
    }
    else if (prop->_isInt && prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<char>(prop, static_cast<char>(strex(value).toInt()));
        }
        else if (prop->_baseSize == 2) {
            SetValue<int16>(prop, static_cast<int16>(strex(value).toInt()));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, static_cast<int>(strex(value).toInt()));
        }
        else if (prop->_baseSize == 8) {
            SetValue<int64>(prop, static_cast<int64>(strex(value).toInt64()));
        }
    }
    else if (prop->_isInt && !prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(strex(value).toInt()));
        }
        else if (prop->_baseSize == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(strex(value).toInt()));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, static_cast<uint>(strex(value).toInt()));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as any", prop->GetName());
    }
}

auto Properties::GetValueAsInt(int property_index) const -> int
{
    STACK_TRACE_ENTRY();

    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't retreive integer value from non plain data property", prop->GetName());
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't retreive integer value from disabled property", prop->GetName());
    }

    return GetPlainDataValueAsInt(prop);
}

auto Properties::GetValueAsAny(int property_index) const -> any_t
{
    STACK_TRACE_ENTRY();

    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't retreive integer value from non plain data property", prop->GetName());
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't retreive integer value from disabled property", prop->GetName());
    }

    return GetPlainDataValueAsAny(prop);
}

void Properties::SetValueAsInt(int property_index, int value)
{
    STACK_TRACE_ENTRY();

    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set integer value to non plain data property", prop->GetName());
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't set integer value to disabled property", prop->GetName());
    }

    SetPlainDataValueAsInt(prop, value);
}

void Properties::SetValueAsAny(int property_index, const any_t& value)
{
    STACK_TRACE_ENTRY();

    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set any value to non plain data property", prop->GetName());
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't set any value to disabled property", prop->GetName());
    }

    SetPlainDataValueAsAny(prop, value);
}

void Properties::SetValueAsIntProps(int property_index, int value)
{
    STACK_TRACE_ENTRY();

    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set integer value to non plain data property", prop->GetName());
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't set integer value to disabled property", prop->GetName());
    }
    if (prop->IsVirtual()) {
        throw PropertiesException("Can't set integer value to virtual property", prop->GetName());
    }

    if (prop->_isHashBase) {
        SetValue<hstring>(prop, ResolveHash(value));
    }
    else if (prop->_isEnumBase) {
        if (prop->_baseSize == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, static_cast<int>(value));
        }
    }
    else if (prop->_isBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            SetValue<float>(prop, static_cast<float>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (prop->_isInt && prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<char>(prop, static_cast<char>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<int16>(prop, static_cast<int16>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, value);
        }
        else if (prop->_baseSize == 8) {
            SetValue<int64>(prop, static_cast<int64>(value));
        }
    }
    else if (prop->_isInt && !prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, static_cast<uint>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int props", prop->GetName());
    }
}

void Properties::SetValueAsAnyProps(int property_index, const any_t& value)
{
    STACK_TRACE_ENTRY();

    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set integer value to non plain data property", prop->GetName());
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't set integer value to disabled property", prop->GetName());
    }
    if (prop->IsVirtual()) {
        throw PropertiesException("Can't set integer value to virtual property", prop->GetName());
    }

    if (prop->_isHashBase) {
        SetValue<hstring>(prop, _registrator->_hashResolver.ToHashedStringMustExists(value));
    }
    else if (prop->_isEnumBase) {
        if (prop->_baseSize == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(strex(value).toUInt()));
        }
        else if (prop->_baseSize == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(strex(value).toUInt()));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, strex(value).toInt());
        }
    }
    else if (prop->_isBool) {
        SetValue<bool>(prop, strex(value).toBool());
    }
    else if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            SetValue<float>(prop, strex(value).toFloat());
        }
        else if (prop->_baseSize == 8) {
            SetValue<double>(prop, strex(value).toDouble());
        }
    }
    else if (prop->_isInt && prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<char>(prop, static_cast<char>(strex(value).toInt()));
        }
        else if (prop->_baseSize == 2) {
            SetValue<int16>(prop, static_cast<int16>(strex(value).toInt()));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, strex(value).toInt());
        }
        else if (prop->_baseSize == 8) {
            SetValue<int64>(prop, strex(value).toInt64());
        }
    }
    else if (prop->_isInt && !prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(strex(value).toUInt()));
        }
        else if (prop->_baseSize == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(strex(value).toUInt()));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, strex(value).toUInt());
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int props", prop->GetName());
    }
}

auto Properties::ResolveHash(hstring::hash_t h) const -> hstring
{
    NO_STACK_TRACE_ENTRY();

    return _registrator->_hashResolver.ResolveHash(h);
}

auto Properties::ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring
{
    NO_STACK_TRACE_ENTRY();

    return _registrator->_hashResolver.ResolveHash(h, failed);
}

PropertyRegistrator::PropertyRegistrator(string_view type_name, PropertiesRelationType relation, HashResolver& hash_resolver, NameResolver& name_resolver) :
    _typeName {hash_resolver.ToHashedString(type_name)},
    _typeNamePlural {hash_resolver.ToHashedString(strex("{}s", type_name))},
    _relation {relation},
    _propMigrationRuleName {hash_resolver.ToHashedString("Property")},
    _componentMigrationRuleName {hash_resolver.ToHashedString("Component")},
    _hashResolver {hash_resolver},
    _nameResolver {name_resolver}
{
    STACK_TRACE_ENTRY();

    _accessMap = {
        {"PrivateCommon", Property::AccessType::PrivateCommon},
        {"PrivateClient", Property::AccessType::PrivateClient},
        {"PrivateServer", Property::AccessType::PrivateServer},
        {"Public", Property::AccessType::Public},
        {"PublicModifiable", Property::AccessType::PublicModifiable},
        {"PublicFullModifiable", Property::AccessType::PublicFullModifiable},
        {"Protected", Property::AccessType::Protected},
        {"ProtectedModifiable", Property::AccessType::ProtectedModifiable},
        {"VirtualPrivateCommon", Property::AccessType::VirtualPrivateCommon},
        {"VirtualPrivateClient", Property::AccessType::VirtualPrivateClient},
        {"VirtualPrivateServer", Property::AccessType::VirtualPrivateServer},
        {"VirtualPublic", Property::AccessType::VirtualPublic},
        {"VirtualProtected", Property::AccessType::VirtualProtected},
    };

    _dataTypeMap = {
        {"PlainData", Property::DataType::PlainData},
        {"String", Property::DataType::String},
        {"Array", Property::DataType::Array},
        {"Dict", Property::DataType::Dict},
    };
}

PropertyRegistrator::~PropertyRegistrator()
{
    STACK_TRACE_ENTRY();

    for (const auto* prop : _registeredProperties) {
        delete prop;
    }
}

void PropertyRegistrator::RegisterComponent(string_view name)
{
    STACK_TRACE_ENTRY();

    const auto name_hash = _hashResolver.ToHashedString(name);

    RUNTIME_ASSERT(!_registeredComponents.count(name_hash));
    _registeredComponents.insert(name_hash);
}

auto PropertyRegistrator::GetByIndex(int property_index) const noexcept -> const Property*
{
    STACK_TRACE_ENTRY();

    if (property_index >= 0 && static_cast<size_t>(property_index) < _registeredProperties.size()) {
        return _registeredProperties[property_index];
    }

    return nullptr;
}

auto PropertyRegistrator::Find(string_view property_name, bool* is_component) const -> const Property*
{
    STACK_TRACE_ENTRY();

    auto key = string(property_name);
    const auto hkey = _hashResolver.ToHashedString(key);

    if (IsComponentRegistered(hkey)) {
        if (is_component != nullptr) {
            *is_component = true;
        }

        return nullptr;
    }

    if (is_component != nullptr) {
        *is_component = false;
    }

    if (const auto rule = _nameResolver.CheckMigrationRule(_propMigrationRuleName, _typeName, hkey); rule.has_value()) {
        key = rule.value();
    }

    if (const auto separator = property_name.find('.'); separator != string::npos) {
        key[separator] = '_';
    }

    if (const auto it = _registeredPropertiesLookup.find(key); it != _registeredPropertiesLookup.end()) {
        return it->second;
    }

    return nullptr;
}

auto PropertyRegistrator::IsComponentRegistered(hstring component_name) const noexcept -> bool
{
    STACK_TRACE_ENTRY();

    hstring migrated_component_name = component_name;

    if (const auto rule = _nameResolver.CheckMigrationRule(_componentMigrationRuleName, _typeName, migrated_component_name); rule.has_value()) {
        migrated_component_name = rule.value();
    }

    return _registeredComponents.find(migrated_component_name) != _registeredComponents.end();
}

auto PropertyRegistrator::GetWholeDataSize() const noexcept -> uint
{
    STACK_TRACE_ENTRY();

    return _wholePodDataSize;
}

auto PropertyRegistrator::GetPropertyGroups() const noexcept -> const map<string, vector<const Property*>>&
{
    STACK_TRACE_ENTRY();

    return _propertyGroups;
}

auto PropertyRegistrator::GetComponents() const noexcept -> const unordered_set<hstring>&
{
    STACK_TRACE_ENTRY();

    return _registeredComponents;
}

void PropertyRegistrator::RegisterProperty(const const_span<string_view>& flags)
{
    STACK_TRACE_ENTRY();

    // Todo: validate property name identifier

    auto* prop = new Property(this);

    RUNTIME_ASSERT(flags.size() >= 11);

    prop->_propName = flags[0];
    RUNTIME_ASSERT(_accessMap.count(flags[1]) != 0);
    prop->_accessType = _accessMap[flags[1]];
    RUNTIME_ASSERT(_dataTypeMap.count(flags[2]) != 0);
    prop->_dataType = _dataTypeMap[flags[2]];

    prop->_baseTypeName = flags[3];
    RUNTIME_ASSERT(!prop->_baseTypeName.empty());
    prop->_baseSize = strex(flags[4]).toInt();
    prop->_isHashBase = flags[5][0] == '1';
    prop->_isEnumBase = flags[6][0] == '1';
    prop->_isInt = flags[7][0] == '1';
    prop->_isSignedInt = flags[8][0] == '1';
    prop->_isFloat = flags[9][0] == '1';
    prop->_isBool = flags[10][0] == '1';

    prop->_isInt8 = prop->_isInt && prop->_isSignedInt && prop->_baseSize == 1;
    prop->_isInt16 = prop->_isInt && prop->_isSignedInt && prop->_baseSize == 2;
    prop->_isInt32 = prop->_isInt && prop->_isSignedInt && prop->_baseSize == 4;
    prop->_isInt64 = prop->_isInt && prop->_isSignedInt && prop->_baseSize == 8;
    prop->_isUInt8 = prop->_isInt && !prop->_isSignedInt && prop->_baseSize == 1;
    prop->_isUInt16 = prop->_isInt && !prop->_isSignedInt && prop->_baseSize == 2;
    prop->_isUInt32 = prop->_isInt && !prop->_isSignedInt && prop->_baseSize == 4;

    if (prop->_isInt && !prop->_isSignedInt && prop->_baseSize == 8) {
        throw PropertyRegistrationException("Type 'uint64' is not supprted", prop->_propName);
    }

    prop->_isSingleFloat = prop->_isFloat && prop->_baseSize == 4;
    prop->_isDoubleFloat = prop->_isFloat && prop->_baseSize == 8;

    size_t flags_start = 11;

    if (prop->_dataType == Property::DataType::Array) {
        RUNTIME_ASSERT(flags.size() >= 12);

        prop->_isArrayOfString = flags[11][0] == '1';

        flags_start = 12;
    }
    else if (prop->_dataType == Property::DataType::Dict) {
        RUNTIME_ASSERT(flags.size() >= 19);

        prop->_isDictOfArray = flags[11][0] == '1';
        prop->_isDictOfString = flags[12][0] == '1';
        prop->_isDictOfArrayOfString = flags[13][0] == '1';
        prop->_dictKeyTypeName = flags[14];
        RUNTIME_ASSERT(!prop->_dictKeyTypeName.empty());
        prop->_dictKeySize = strex(flags[15]).toInt();
        prop->_isDictKeyString = flags[16][0] == '1';
        prop->_isDictKeyHash = flags[17][0] == '1';
        prop->_isDictKeyEnum = flags[18][0] == '1';

        flags_start = 19;
    }

    prop->_isStringBase = prop->_dataType == Property::DataType::String || prop->_isArrayOfString || prop->_isDictOfString || prop->_isDictOfArrayOfString;

    if (const auto dot_pos = prop->_propName.find('.'); dot_pos != string::npos) {
        prop->_component = _hashResolver.ToHashedString(prop->_propName.substr(0, dot_pos));
        prop->_propName[dot_pos] = '_';
        prop->_propNameWithoutComponent = prop->_propName.substr(dot_pos + 1);

        if (!prop->_component) {
            throw PropertyRegistrationException("Invalid property component part", prop->_propName, prop->_component);
        }
        if (prop->_propNameWithoutComponent.empty()) {
            throw PropertyRegistrationException("Invalid property name part", prop->_propName, prop->_component);
        }
        if (_registeredComponents.count(_hashResolver.ToHashedString(prop->_component)) == 0) {
            throw PropertyRegistrationException("Unknown property component", prop->_propName, prop->_component);
        }
    }
    else {
        prop->_propNameWithoutComponent = prop->_propName;
    }

    if (IsEnumSet(prop->_accessType, Property::AccessType::VirtualMask)) {
        prop->_isVirtual = true;
    }

    for (size_t i = flags_start; i < flags.size(); i++) {
        const auto check_next_param = [&flags, i, prop] {
            if (i + 2 >= flags.size() || flags[i + 1] != "=") {
                throw PropertyRegistrationException("Expected property flag = value", prop->_propName, flags[i], i, flags.size());
            }
        };

        if (flags[i] == "Group") {
            check_next_param();

            const auto& group = flags[i + 2];
            if (const auto it = _propertyGroups.find(string(group)); it != _propertyGroups.end()) {
                it->second.push_back(prop);
            }
            else {
                _propertyGroups.emplace(group, vector<const Property*> {prop});
            }
            i += 2;
        }
        else if (flags[i] == "ReadOnly") {
            prop->_isReadOnly = true;
        }
        else if (flags[i] == "Temporary") {
            prop->_isTemporary = true;
        }
        else if (flags[i] == "Historical") {
            prop->_isHistorical = true;
        }
        else if (flags[i] == "Resource") {
            if (!prop->_isHashBase) {
                throw PropertyRegistrationException("Expected hstring for Resource flag", prop->_propName);
            }

            prop->_isResourceHash = true;
        }
        else if (flags[i] == "ScriptFuncType") {
            check_next_param();
            if (!prop->_isHashBase) {
                throw PropertyRegistrationException("Expected hstring for ScriptFunc flag", prop->_propName);
            }

            prop->_scriptFuncType = flags[i + 2];
            i += 2;
        }
        else if (flags[i] == "Max") {
            check_next_param();
            if (!prop->_isInt && !prop->_isFloat) {
                throw PropertyRegistrationException("Expected numeric type for Max flag", prop->_propName);
            }

            if (prop->_isInt) {
                prop->_maxValueI = strex(flags[i + 2]).toInt64();
            }
            else {
                prop->_maxValueF = strex(flags[i + 2]).toDouble();
            }

            prop->_checkMaxValue = true;
            i += 2;
        }
        else if (flags[i] == "Min") {
            check_next_param();
            if (!prop->_isInt && !prop->_isFloat) {
                throw PropertyRegistrationException("Expected numeric type for Min flag", prop->_propName);
            }

            if (prop->_isInt) {
                prop->_minValueI = strex(flags[i + 2]).toInt64();
            }
            else {
                prop->_minValueF = strex(flags[i + 2]).toDouble();
            }

            prop->_checkMinValue = true;
            i += 2;
        }
        else if (flags[i] == "Quest") {
            // Todo: restore quest variables
            check_next_param();
            i += 2;
        }
        else if (flags[i] == "NullGetterForProto") {
            prop->_isNullGetterForProto = true;
        }
        else if (flags[i] == "IsCommon") {
            // Skip
        }
        else {
            throw PropertyRegistrationException("Invalid property flag", prop->_propName, flags[i]);
        }
    }

    if (prop->_isNullGetterForProto && !prop->IsVirtual()) {
        throw PropertyRegistrationException("Invalid property configuration - NullGetterForProto for non-virtual property", prop->_propName);
    }

    if (prop->_dataType == Property::DataType::Array) {
        prop->_fullTypeName = prop->_baseTypeName + "[]";
    }
    else if (prop->_dataType == Property::DataType::Dict) {
        if (prop->_isDictOfArray) {
            prop->_fullTypeName = "dict<" + prop->_dictKeyTypeName + ", " + prop->_baseTypeName + "[]>";
        }
        else {
            prop->_fullTypeName = "dict<" + prop->_dictKeyTypeName + ", " + prop->_baseTypeName + ">";
        }
    }
    else {
        prop->_fullTypeName = prop->_baseTypeName;
    }

    const auto reg_index = static_cast<uint16>(_registeredProperties.size());

    // Disallow set or get accessors
    auto disabled = false;

    if (_relation == PropertiesRelationType::ServerRelative && IsEnumSet(prop->_accessType, Property::AccessType::ClientOnlyMask)) {
        disabled = true;
    }
    if (_relation == PropertiesRelationType::ClientRelative && IsEnumSet(prop->_accessType, Property::AccessType::ServerOnlyMask)) {
        disabled = true;
    }

    if (_relation == PropertiesRelationType::ClientRelative && //
        (IsEnumSet(prop->_accessType, Property::AccessType::PublicMask) || IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) && //
        !IsEnumSet(prop->_accessType, Property::AccessType::ModifiableMask)) {
        RUNTIME_ASSERT(!disabled);
        prop->_isReadOnly = true;
    }

    // PlainData property data offset
    const auto prev_public_space_size = _publicPodDataSpace.size();
    const auto prev_protected_space_size = _protectedPodDataSpace.size();

    auto pod_data_base_offset = Property::INVALID_DATA_MARKER;

    if (prop->_dataType == Property::DataType::PlainData && !disabled && !prop->IsVirtual()) {
        const auto is_public = IsEnumSet(prop->_accessType, Property::AccessType::PublicMask);
        const auto is_protected = IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask);
        auto& space = (is_public ? _publicPodDataSpace : (is_protected ? _protectedPodDataSpace : _privatePodDataSpace));

        const auto space_size = static_cast<uint>(space.size());
        uint space_pos = 0;
        while (space_pos + prop->_baseSize <= space_size) {
            auto fail = false;
            for (uint i = 0; i < prop->_baseSize; i++) {
                if (space[space_pos + i]) {
                    fail = true;
                    break;
                }
            }
            if (!fail) {
                break;
            }
            space_pos += prop->_baseSize;
        }

        auto new_size = space_pos + prop->_baseSize;
        new_size += 8u - new_size % 8u;
        if (new_size > static_cast<uint>(space.size())) {
            space.resize(new_size);
        }

        for (uint i = 0; i < prop->_baseSize; i++) {
            space[space_pos + i] = true;
        }

        pod_data_base_offset = space_pos;

        _wholePodDataSize = static_cast<uint>(_publicPodDataSpace.size() + _protectedPodDataSpace.size() + _privatePodDataSpace.size());
        RUNTIME_ASSERT((_wholePodDataSize % 8u) == 0);
    }

    // Complex property data index
    auto complex_data_index = Property::INVALID_DATA_MARKER;

    if (prop->_dataType != Property::DataType::PlainData && !disabled && !prop->IsVirtual()) {
        complex_data_index = static_cast<uint>(_complexProperties.size());
        _complexProperties.emplace_back(prop);

        if (IsEnumSet(prop->_accessType, Property::AccessType::PublicMask)) {
            _publicComplexDataProps.emplace_back(reg_index);
            _publicProtectedComplexDataProps.emplace_back(reg_index);
        }
        else if (IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) {
            _protectedComplexDataProps.emplace_back(reg_index);
            _publicProtectedComplexDataProps.emplace_back(reg_index);
        }
    }

    // Other flags
    prop->_regIndex = reg_index;
    prop->_complexDataIndex = complex_data_index;
    prop->_podDataOffset = pod_data_base_offset;
    prop->_isDisabled = disabled;

    _registeredProperties.emplace_back(prop);
    _constRegisteredProperties.emplace_back(prop);

    RUNTIME_ASSERT(_registeredPropertiesLookup.count(prop->_propName) == 0);
    _registeredPropertiesLookup.emplace(prop->_propName, prop);

    // Fix plain data data offsets
    if (prop->_podDataOffset != Property::INVALID_DATA_MARKER) {
        if (IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) {
            prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size());
        }
        else if (IsEnumSet(prop->_accessType, Property::AccessType::PrivateMask)) {
            prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size()) + static_cast<uint>(_protectedPodDataSpace.size());
        }
    }

    for (auto* other_prop : _registeredProperties) {
        if (other_prop->_podDataOffset == Property::INVALID_DATA_MARKER || other_prop == prop) {
            continue;
        }

        if (IsEnumSet(other_prop->_accessType, Property::AccessType::ProtectedMask)) {
            other_prop->_podDataOffset -= static_cast<uint>(prev_public_space_size);
            other_prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size());
        }
        else if (IsEnumSet(other_prop->_accessType, Property::AccessType::PrivateMask)) {
            other_prop->_podDataOffset -= static_cast<uint>(prev_public_space_size) + static_cast<uint>(prev_protected_space_size);
            other_prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size()) + static_cast<uint>(_protectedPodDataSpace.size());
        }
    }
}
