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

#include "Properties.h"
#include "Log.h"
#include "PropertiesSerializator.h"
#include "StringUtils.h"

Property::Property(const PropertyRegistrator* registrator) : _registrator {registrator}
{
}

void Property::AddCallback(PropertyChangedCallback callback) const
{
    _callbacks.emplace_back(std::move(callback));
}

Properties::Properties(const PropertyRegistrator* registrator) : _registrator {registrator}
{
    RUNTIME_ASSERT(_registrator);

    if (!_registrator->_registeredProperties.empty()) {
        AllocData();
    }
}

Properties::Properties(const Properties& other) : Properties(other._registrator)
{
    // Copy PlainData data
    std::memcpy(&_podData[0], &other._podData[0], _registrator->_wholePodDataSize);

    // Copy complex data
    for (size_t i = 0; i < other._complexData.size(); i++) {
        const auto size = other._complexDataSizes[i];
        _complexDataSizes[i] = size;
        _complexData[i] = size != 0u ? new uchar[size] : nullptr;
        if (size != 0u) {
            std::memcpy(_complexData[i], other._complexData[i], size);
        }
    }
}

Properties::~Properties()
{
    // Hide warning about throwing desctructor
    try {
        _registrator->_podDataPool.push_back(_podData);
    }
    catch (...) {
    }

    for (const auto* cd : _complexData) {
        delete[] cd;
    }
}

auto Properties::operator=(const Properties& other) -> Properties&
{
    if (this == &other) {
        return *this;
    }

    RUNTIME_ASSERT(_registrator == other._registrator);

    // Copy PlainData data
    std::memcpy(&_podData[0], &other._podData[0], _registrator->_wholePodDataSize);

    // Copy complex data
    for (const auto* prop : _registrator->_registeredProperties) {
        if (prop->_complexDataIndex != static_cast<uint>(-1)) {
            SetRawData(prop, other._complexData[prop->_complexDataIndex], other._complexDataSizes[prop->_complexDataIndex]);
        }
    }

    return *this;
}

void Properties::AllocData()
{
    RUNTIME_ASSERT(_podData == nullptr);
    RUNTIME_ASSERT(!_registrator->_registeredProperties.empty());

    // Allocate plain data
    if (!_registrator->_podDataPool.empty()) {
        _podData = _registrator->_podDataPool.back();
        _registrator->_podDataPool.pop_back();
    }
    else {
        _podData = new uchar[_registrator->_wholePodDataSize];
    }

    std::memset(_podData, 0, _registrator->_wholePodDataSize);

    // Complex data
    _complexData.resize(_registrator->_complexProperties.size());
    _complexDataSizes.resize(_registrator->_complexProperties.size());
}

void Properties::StoreAllData(vector<uchar>& all_data) const
{
    all_data.clear();
    auto writer = DataWriter(all_data);

    // Store plain properties data
    writer.Write<uint>(_registrator->_wholePodDataSize);
    writer.WritePtr(_podData, _registrator->_wholePodDataSize);

    // Store complex properties
    writer.Write<uint>(static_cast<uint>(_registrator->_complexProperties.size()));
    for (const auto* prop : _registrator->_complexProperties) {
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        writer.Write<uint>(_complexDataSizes[prop->_complexDataIndex]);
        writer.WritePtr(_complexData[prop->_complexDataIndex], _complexDataSizes[prop->_complexDataIndex]);
    }

    // Store hashes
    vector<string> str_hashes;
    str_hashes.reserve(64);
    const auto add_hash = [&str_hashes](const string& str) {
        if (!str.empty()) {
            str_hashes.emplace_back(str);
        }
    };

    for (const auto* prop : _registrator->_registeredProperties) {
        if (prop->IsReadable() && (prop->IsBaseTypeHash() || prop->IsDictKeyHash())) {
            const auto value = PropertiesSerializator::SavePropertyToValue(this, prop, _registrator->_nameResolver);

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

    writer.Write<uint>(static_cast<uint>(str_hashes.size()));
    for (const auto& str : str_hashes) {
        writer.Write<uint>(static_cast<uint>(str.length()));
        writer.WritePtr(str.data(), str.size());
    }
}

void Properties::RestoreAllData(const vector<uchar>& all_data)
{
    auto reader = DataReader(all_data);

    // Read plain properties data
    const auto pod_data_size = reader.Read<uint>();
    RUNTIME_ASSERT(pod_data_size == _registrator->_wholePodDataSize);
    std::memcpy(_podData, reader.ReadPtr<uchar>(pod_data_size), pod_data_size);

    // Read complex properties
    const auto complex_props_count = reader.Read<uint>();
    RUNTIME_ASSERT(complex_props_count == _registrator->_complexProperties.size());
    for (const auto* prop : _registrator->_complexProperties) {
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        const auto data_size = reader.Read<uint>();
        SetRawData(prop, reader.ReadPtr<uchar>(data_size), data_size);
    }

    // Read hashes
    const auto str_hashes_count = reader.Read<uint>();
    for (const auto i : xrange(str_hashes_count)) {
        UNUSED_VARIABLE(i);
        const auto str_size = reader.Read<uint>();
        const auto str = string(reader.ReadPtr<char>(str_size), str_size);
        const auto hstr = _registrator->_nameResolver.ToHashedString(str);
        UNUSED_VARIABLE(hstr);
    }

    reader.VerifyEnd();
}

auto Properties::StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint
{
    uint whole_size = 0u;
    *all_data = &_storeData;
    *all_data_sizes = &_storeDataSizes;
    _storeData.resize(0u);
    _storeDataSizes.resize(0u);

    _storeDataComplexIndices = with_protected ? _registrator->_publicProtectedComplexDataProps : _registrator->_publicComplexDataProps;

    const auto preserve_size = 1u + (!_storeDataComplexIndices.empty() ? 1u + _storeDataComplexIndices.size() : 0u);
    _storeData.reserve(preserve_size);
    _storeDataSizes.reserve(preserve_size);

    // Store plain properties data
    _storeData.push_back(_podData);
    _storeDataSizes.push_back(static_cast<uint>(_registrator->_publicPodDataSpace.size()) + (with_protected ? static_cast<uint>(_registrator->_protectedPodDataSpace.size()) : 0));
    whole_size += _storeDataSizes.back();

    // Filter complex data to send
    for (size_t i = 0; i < _storeDataComplexIndices.size();) {
        const auto* prop = _registrator->_registeredProperties[_storeDataComplexIndices[i]];
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        if (_complexDataSizes[prop->_complexDataIndex] == 0u) {
            _storeDataComplexIndices.erase(_storeDataComplexIndices.begin() + static_cast<int>(i));
        }
        else {
            i++;
        }
    }

    // Store complex properties data
    if (!_storeDataComplexIndices.empty()) {
        _storeData.push_back(reinterpret_cast<uchar*>(&_storeDataComplexIndices[0]));
        _storeDataSizes.push_back(static_cast<uint>(_storeDataComplexIndices.size()) * sizeof(ushort));
        whole_size += _storeDataSizes.back();

        for (const auto index : _storeDataComplexIndices) {
            const auto* prop = _registrator->_registeredProperties[index];
            _storeData.push_back(_complexData[prop->_complexDataIndex]);
            _storeDataSizes.push_back(_complexDataSizes[prop->_complexDataIndex]);
            whole_size += _storeDataSizes.back();
        }
    }

    return whole_size;
}

void Properties::RestoreData(const vector<const uchar*>& all_data, const vector<uint>& all_data_sizes)
{
    // Restore plain data
    RUNTIME_ASSERT(!all_data_sizes.empty());
    RUNTIME_ASSERT(all_data.size() == all_data_sizes.size());
    const auto public_size = static_cast<uint>(_registrator->_publicPodDataSpace.size());
    const auto protected_size = static_cast<uint>(_registrator->_protectedPodDataSpace.size());
    const auto private_size = static_cast<uint>(_registrator->_privatePodDataSpace.size());
    RUNTIME_ASSERT(all_data_sizes[0] == public_size || all_data_sizes[0] == public_size + protected_size || all_data_sizes[0] == public_size + protected_size + private_size);
    if (all_data_sizes[0] != 0u) {
        std::memcpy(_podData, all_data[0], all_data_sizes[0]);
    }

    // Restore complex data
    if (all_data.size() > 1) {
        const uint comlplex_data_count = all_data_sizes[1] / sizeof(ushort);
        RUNTIME_ASSERT(comlplex_data_count > 0);
        vector<ushort> complex_indicies(comlplex_data_count);
        std::memcpy(&complex_indicies[0], all_data[1], all_data_sizes[1]);

        for (size_t i = 0; i < complex_indicies.size(); i++) {
            RUNTIME_ASSERT(complex_indicies[i] < _registrator->_registeredProperties.size());
            const auto* prop = _registrator->_registeredProperties[complex_indicies[i]];
            RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
            const auto data_size = all_data_sizes[2 + i];
            const auto* data = all_data[2 + i];
            SetRawData(prop, data, data_size);
        }
    }
}

void Properties::RestoreData(const vector<vector<uchar>>& all_data)
{
    vector<const uchar*> all_data_ext(all_data.size());
    vector<uint> all_data_sizes(all_data.size());
    for (size_t i = 0; i < all_data.size(); i++) {
        all_data_ext[i] = !all_data[i].empty() ? &all_data[i][0] : nullptr;
        all_data_sizes[i] = static_cast<uint>(all_data[i].size());
    }
    RestoreData(all_data_ext, all_data_sizes);
}

auto Properties::LoadFromText(const map<string, string>& key_values) -> bool
{
    bool is_error = false;

    for (auto&& [key, value] : key_values) {
        // Skip technical fields
        if (key.empty() || key[0] == '$' || key[0] == '_') {
            continue;
        }

        // Find property
        const auto* prop = _registrator->Find(key);
        if (prop == nullptr || (prop->_podDataOffset == static_cast<uint>(-1) && prop->_complexDataIndex == static_cast<uint>(-1))) {
            if (prop == nullptr) {
                WriteLog("Unknown property {}", key);
            }
            else {
                if (_registrator->_relation == PropertiesRelationType::ServerRelative && IsEnumSet(prop->_accessType, Property::AccessType::ClientOnlyMask)) {
                    continue;
                }
                if (_registrator->_relation == PropertiesRelationType::ClientRelative && IsEnumSet(prop->_accessType, Property::AccessType::ServerOnlyMask)) {
                    continue;
                }

                WriteLog("Invalid property {} for reading", prop->GetName());
            }

            is_error = true;
            continue;
        }

        // Parse
        if (!LoadPropertyFromText(prop, value)) {
            is_error = true;
        }
    }

    return !is_error;
}

auto Properties::SaveToText(Properties* base) const -> map<string, string>
{
    RUNTIME_ASSERT(!base || _registrator == base->_registrator);

    map<string, string> key_values;
    for (auto* prop : _registrator->_registeredProperties) {
        // Skip pure virtual properties
        if (prop->_podDataOffset == static_cast<uint>(-1) && prop->_complexDataIndex == static_cast<uint>(-1)) {
            continue;
        }

        // Skip temporary properties
        if (prop->_isTemporary) {
            continue;
        }

        // Skip same
        if (base != nullptr) {
            if (prop->_podDataOffset != static_cast<uint>(-1)) {
                if (memcmp(&_podData[prop->_podDataOffset], &base->_podData[prop->_podDataOffset], prop->_baseSize) == 0) {
                    continue;
                }
            }
            else {
                if (_complexDataSizes[prop->_complexDataIndex] == 0u && base->_complexDataSizes[prop->_complexDataIndex] == 0u) {
                    continue;
                }
                if (_complexDataSizes[prop->_complexDataIndex] == base->_complexDataSizes[prop->_complexDataIndex] && memcmp(_complexData[prop->_complexDataIndex], base->_complexData[prop->_complexDataIndex], _complexDataSizes[prop->_complexDataIndex]) == 0) {
                    continue;
                }
            }
        }
        else {
            if (prop->_podDataOffset != static_cast<uint>(-1)) {
                uint64 pod_zero = 0;
                RUNTIME_ASSERT(prop->_baseSize <= sizeof(pod_zero));
                if (memcmp(&_podData[prop->_podDataOffset], &pod_zero, prop->_baseSize) == 0) {
                    continue;
                }
            }
            else {
                if (_complexDataSizes[prop->_complexDataIndex] == 0u) {
                    continue;
                }
            }
        }

        // Serialize to text and store in map
        key_values.insert(std::make_pair(prop->_propName, SavePropertyToText(prop)));
    }

    return key_values;
}

auto Properties::LoadPropertyFromText(const Property* prop, string_view text) -> bool
{
    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(_registrator == prop->_registrator);
    RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1) || prop->_complexDataIndex != static_cast<uint>(-1));

    const auto is_dict = prop->_dataType == Property::DataType::Dict;
    const auto is_array = prop->_dataType == Property::DataType::Array || prop->_isDictOfArray;

    int value_type;
    if (prop->_dataType == Property::DataType::String || prop->_isArrayOfString || prop->_isDictOfArrayOfString || prop->_isHashBase || prop->_isEnumBase) {
        value_type = AnyData::STRING_VALUE;
    }
    else if (prop->_isInt64 || prop->_isUInt64) {
        value_type = AnyData::INT64_VALUE;
    }
    else if (prop->_isInt) {
        value_type = AnyData::INT_VALUE;
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
    return PropertiesSerializator::LoadPropertyFromValue(this, prop, value, _registrator->_nameResolver);
}

auto Properties::SavePropertyToText(const Property* prop) const -> string
{
    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(_registrator == prop->_registrator);
    RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1) || prop->_complexDataIndex != static_cast<uint>(-1));

    const auto value = PropertiesSerializator::SavePropertyToValue(this, prop, _registrator->_nameResolver);
    return AnyData::ValueToString(value);
}

auto Properties::GetRawDataSize(const Property* prop) const -> uint
{
    uint data_size = 0;
    const auto* data = GetRawData(prop, data_size);
    UNUSED_VARIABLE(data);
    return data_size;
}

auto Properties::GetRawData(const Property* prop, uint& data_size) const -> const uchar*
{
    if (prop->_dataType == Property::DataType::PlainData) {
        RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
        data_size = prop->_baseSize;
        return &_podData[prop->_podDataOffset];
    }

    RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
    data_size = _complexDataSizes[prop->_complexDataIndex];
    return _complexData[prop->_complexDataIndex];
}

auto Properties::GetRawData(const Property* prop, uint& data_size) -> uchar*
{
    return const_cast<uchar*>(const_cast<const Properties*>(this)->GetRawData(prop, data_size));
}

void Properties::SetRawData(const Property* prop, const uchar* data, uint data_size)
{
    if (prop->IsPlainData()) {
        RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
        RUNTIME_ASSERT(prop->_baseSize == data_size);

        std::memcpy(&_podData[prop->_podDataOffset], data, data_size);
    }
    else {
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));

        if (data_size != _complexDataSizes[prop->_complexDataIndex]) {
            _complexDataSizes[prop->_complexDataIndex] = data_size;
            delete[] _complexData[prop->_complexDataIndex];
            if (data_size != 0u) {
                _complexData[prop->_complexDataIndex] = new uchar[data_size];
            }
        }

        if (data_size != 0u) {
            std::memcpy(_complexData[prop->_complexDataIndex], data, data_size);
        }
    }
}

void Properties::SetValueFromData(const Property* prop, const uchar* data, uint data_size)
{
    // Todo: SetValueFromData
    UNUSED_VARIABLE(prop);
    UNUSED_VARIABLE(data);
    UNUSED_VARIABLE(data_size);
    /*if (dataType == Property::String)
    {
        string str;
        if (data_size)
            str.assign((char*)data, data_size);
        GenericSet(entity, &str);
    }
    else if (dataType == Property::PlainData)
    {
        RUNTIME_ASSERT(data_size == baseSize);
        GenericSet(entity, data);
    }
    else if (dataType == Property::Array || dataType == Property::Dict)
    {
        auto value = CreateRefValue(data, data_size);
        GenericSet(entity, value.get());
    }
    else
    {
        RUNTIME_ASSERT(!"Unexpected type");
    }*/
}

auto Properties::GetPlainDataValueAsInt(const Property* prop) const -> int
{
    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            return static_cast<int>(GetValue<float>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<int>(GetValue<double>(prop));
        }
    }
    else if (prop->_isInt && prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            return static_cast<int>(GetValue<char>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<int>(GetValue<short>(prop));
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
            return static_cast<int>(GetValue<uchar>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<int>(GetValue<ushort>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<int>(GetValue<uint>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<int>(GetValue<uint64>(prop));
        }
    }

    throw PropertiesException("Invalid property for get as int", prop->GetName());
}

auto Properties::GetPlainDataValueAsFloat(const Property* prop) const -> float
{
    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        return GetValue<bool>(prop) ? 1.0f : 0.0f;
    }
    if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            return GetValue<float>(prop);
        }
        if (prop->_baseSize == 8) {
            return static_cast<float>(GetValue<double>(prop));
        }
    }
    else if (prop->_isInt && prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            return static_cast<float>(GetValue<char>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<float>(GetValue<short>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<float>(GetValue<int>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<float>(GetValue<int64>(prop));
        }
    }
    else if (prop->_isInt && !prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            return static_cast<float>(GetValue<uchar>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<float>(GetValue<ushort>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<float>(GetValue<uint>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<float>(GetValue<uint64>(prop));
        }
    }

    throw PropertiesException("Invalid property for get as float", prop->GetName());
}

void Properties::SetPlainDataValueAsInt(const Property* prop, int value)
{
    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
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
            SetValue<short>(prop, static_cast<short>(value));
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
            SetValue<uchar>(prop, static_cast<uchar>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<ushort>(prop, static_cast<ushort>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, static_cast<uint>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<uint64>(prop, static_cast<uint64>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int", prop->GetName());
    }
}

void Properties::SetPlainDataValueAsFloat(const Property* prop, float value)
{
    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        SetValue<bool>(prop, value != 0.0f);
    }
    else if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            SetValue<float>(prop, value);
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
            SetValue<short>(prop, static_cast<short>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, static_cast<int>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<int64>(prop, static_cast<int64>(value));
        }
    }
    else if (prop->_isInt && !prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<uchar>(prop, static_cast<uchar>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<ushort>(prop, static_cast<ushort>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, static_cast<uint>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<uint64>(prop, static_cast<uint64>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as float", prop->GetName());
    }
}

auto Properties::GetValueAsInt(int property_index) const -> int
{
    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't retreive integer value from non PlainData property", prop->GetName());
    }
    if (!prop->IsReadable()) {
        throw PropertiesException("Can't retreive integer value from non readable property", prop->GetName());
    }

    return GetPlainDataValueAsInt(prop);
}

auto Properties::GetValueAsFloat(int property_index) const -> float
{
    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't retreive integer value from non PlainData property", prop->GetName());
    }
    if (!prop->IsReadable()) {
        throw PropertiesException("Can't retreive integer value from non readable property", prop->GetName());
    }

    return GetPlainDataValueAsFloat(prop);
}

void Properties::SetValueAsInt(int property_index, int value)
{
    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set integer value to non PlainData property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }

    SetPlainDataValueAsInt(prop, value);
}

void Properties::SetValueAsFloat(int property_index, float value)
{
    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set integer value to non PlainData property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }

    SetPlainDataValueAsFloat(prop, value);
}

void Properties::SetValueAsIntByName(string_view property_name, int value)
{
    const auto* prop = _registrator->Find(property_name);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_name);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set by name integer value from non PlainData property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }

    SetPlainDataValueAsInt(prop, value);
}

void Properties::SetValueAsIntProps(int property_index, int value)
{
    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set integer value to non PlainData property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }
    if (IsEnumSet(prop->_accessType, Property::AccessType::VirtualMask)) {
        throw PropertiesException("Can't set integer value to virtual property", prop->GetName());
    }

    if (prop->_isHashBase) {
        SetValue<hstring>(prop, _registrator->_nameResolver.ResolveHash(value));
    }
    else if (prop->_isEnumBase) {
        if (prop->_baseSize == 1) {
            SetValue<uchar>(prop, static_cast<uchar>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<ushort>(prop, static_cast<ushort>(value));
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
            SetValue<short>(prop, static_cast<short>(value));
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
            SetValue<uchar>(prop, static_cast<uchar>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<ushort>(prop, static_cast<ushort>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, static_cast<uint>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<uint64>(prop, static_cast<uint64>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int props", prop->GetName());
    }
}

auto Properties::ResolveHash(hstring::hash_t h) const -> hstring
{
    return _registrator->_nameResolver.ResolveHash(h);
}

PropertyRegistrator::PropertyRegistrator(string_view class_name, PropertiesRelationType relation, NameResolver& name_resolver) : _className {class_name}, _relation {relation}, _nameResolver {name_resolver}
{
}

PropertyRegistrator::~PropertyRegistrator()
{
    for (const auto* prop : _registeredProperties) {
        delete prop;
    }
    for (const auto* data : _podDataPool) {
        delete[] data;
    }
}

void PropertyRegistrator::RegisterComponent(string_view name)
{
    const auto name_hash = _nameResolver.ToHashedString(name);

    RUNTIME_ASSERT(!_registeredComponents.count(name_hash));
    _registeredComponents.insert(name_hash);
}

auto PropertyRegistrator::GetClassName() const -> const string&
{
    return _className;
}

auto PropertyRegistrator::GetCount() const -> uint
{
    return static_cast<uint>(_registeredProperties.size());
}

auto PropertyRegistrator::GetByIndex(int property_index) const -> const Property*
{
    if (property_index >= 0 && property_index < static_cast<int>(_registeredProperties.size())) {
        return _registeredProperties[property_index];
    }
    return nullptr;
}

auto PropertyRegistrator::Find(string_view property_name) const -> const Property*
{
    auto key = string(property_name);
    if (const auto separator = property_name.find('.'); separator != string::npos) {
        key[separator] = '_';
    }

    if (const auto it = _registeredPropertiesLookup.find(key); it != _registeredPropertiesLookup.end()) {
        return it->second;
    }

    return nullptr;
}

auto PropertyRegistrator::IsComponentRegistered(hstring component_name) const -> bool
{
    return _registeredComponents.count(component_name) > 0u;
}

auto PropertyRegistrator::GetWholeDataSize() const -> uint
{
    return _wholePodDataSize;
}

auto PropertyRegistrator::GetPropertyGroups() const -> const map<string, vector<const Property*>>&
{
    return _propertyGroups;
}

auto PropertyRegistrator::GetComponents() const -> const unordered_set<hstring>&
{
    return _registeredComponents;
}

void PropertyRegistrator::AppendProperty(Property* prop, const vector<string>& flags)
{
    // Todo: validate property name identifier

    if (const auto dot_pos = prop->_propName.find('.'); dot_pos != string::npos) {
        prop->_component = _nameResolver.ToHashedString(prop->_propName.substr(0, dot_pos));
        prop->_propName[dot_pos] = '_';
        prop->_propNameWithoutComponent = prop->_propName.substr(dot_pos + 1);

        if (!prop->_component) {
            throw PropertyRegistrationException("Invalid property component part", prop->_propName, prop->_component);
        }
        if (prop->_propNameWithoutComponent.empty()) {
            throw PropertyRegistrationException("Invalid property name part", prop->_propName, prop->_component);
        }
        if (_registeredComponents.count(_nameResolver.ToHashedString(prop->_component)) == 0) {
            throw PropertyRegistrationException("Unknown property component", prop->_propName, prop->_component);
        }
    }
    else {
        prop->_propNameWithoutComponent = prop->_propName;
    }

    if (prop->_isEnumBase) {
        prop->_baseTypeName = "";
    }
    if (prop->_dataType == Property::DataType::Dict && prop->_isDictKeyEnum) {
        prop->_dictKeyTypeName = "";
    }

    for (size_t i = 0; i < flags.size(); i++) {
        const auto check_next_param = [&flags, i, prop] {
            if (i + 2 >= flags.size() || flags[i + 1] != "=") {
                throw PropertyRegistrationException("Expected property flag = value", prop->_propName, flags[i], i, flags.size());
            }
        };

        if (flags[i] == "Enum") {
            check_next_param();
            if (!prop->_isEnumBase) {
                throw PropertyRegistrationException("Expected hstring for Enum flag", prop->_propName);
            }

            prop->_baseTypeName = flags[i + 2];
            i += 2;
        }
        else if (flags[i] == "KeyEnum") {
            check_next_param();
            if (prop->_dataType != Property::DataType::Dict) {
                throw PropertyRegistrationException("Expected dict for KeyEnum flag", prop->_propName);
            }

            prop->_dictKeyTypeName = flags[i + 2];
            i += 2;
        }
        else if (flags[i] == "Group") {
            check_next_param();

            const auto& group = flags[i + 2];
            if (const auto it = _propertyGroups.find(group); it != _propertyGroups.end()) {
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
        else if (flags[i] == "Default") {
            check_next_param();
            if (!prop->_isInt && !prop->_isFloat && !prop->_isBool) {
                throw PropertyRegistrationException("Expected numeric type for Default flag", prop->_propName);
            }

            if (prop->_isBool) {
                prop->_defValueI = _str(flags[i + 2]).toBool() ? 1 : 0;
            }
            else if (prop->_isInt) {
                prop->_defValueI = _str(flags[i + 2]).toInt64();
            }
            else {
                prop->_defValueF = _str(flags[i + 2]).toDouble();
            }

            prop->_setDefaultValue = true;
            i += 2;
        }
        else if (flags[i] == "Max") {
            check_next_param();
            if (!prop->_isInt && !prop->_isFloat) {
                throw PropertyRegistrationException("Expected numeric type for Max flag", prop->_propName);
            }

            if (prop->_isInt) {
                prop->_maxValueI = _str(flags[i + 2]).toInt64();
            }
            else {
                prop->_maxValueF = _str(flags[i + 2]).toDouble();
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
                prop->_minValueI = _str(flags[i + 2]).toInt64();
            }
            else {
                prop->_minValueF = _str(flags[i + 2]).toDouble();
            }

            prop->_checkMinValue = true;
            i += 2;
        }
        else if (flags[i] == "Quest") {
            // Todo: restore quest variables
            check_next_param();
            i += 2;
        }
        else if (flags[i] == "Alias") {
            check_next_param();
            prop->_propNameAliases.push_back(flags[i + 2]);
            i += 2;
        }
        else {
            throw PropertyRegistrationException("Invalid property flag", prop->_propName, flags[i]);
        }
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

    RUNTIME_ASSERT(!prop->_baseTypeName.empty());
    if (prop->_dataType == Property::DataType::Dict) {
        RUNTIME_ASSERT(!prop->_dictKeyTypeName.empty());
    }

    // bool _isHistorical {};
    // prop->_isHistorical = (defaultNoHistory || is_no_history);

    const auto reg_index = static_cast<ushort>(_registeredProperties.size());

    // Disallow set or get accessors
    auto disable_get = false;
    auto disable_set = false;

    if (_relation == PropertiesRelationType::ServerRelative && IsEnumSet(prop->_accessType, Property::AccessType::ClientOnlyMask)) {
        disable_get = true;
        disable_set = true;
    }
    if (_relation == PropertiesRelationType::ClientRelative && IsEnumSet(prop->_accessType, Property::AccessType::ServerOnlyMask)) {
        disable_get = true;
        disable_set = true;
    }
    if (_relation == PropertiesRelationType::ClientRelative && //
        (IsEnumSet(prop->_accessType, Property::AccessType::PublicMask) || IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) && //
        !IsEnumSet(prop->_accessType, Property::AccessType::ModifiableMask)) {
        RUNTIME_ASSERT(!disable_get);
        disable_set = true;
    }
    if (prop->_isReadOnly) {
        disable_set = true;
    }

    // PlainData property data offset
    const auto prev_public_space_size = _publicPodDataSpace.size();
    const auto prev_protected_space_size = _protectedPodDataSpace.size();

    auto data_base_offset = static_cast<uint>(-1);

    if (prop->_dataType == Property::DataType::PlainData && !disable_get && !IsEnumSet(prop->_accessType, Property::AccessType::VirtualMask)) {
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

        data_base_offset = space_pos;

        _wholePodDataSize = static_cast<uint>(_publicPodDataSpace.size() + _protectedPodDataSpace.size() + _privatePodDataSpace.size());
        RUNTIME_ASSERT((_wholePodDataSize % 8u) == 0u);
    }

    // Complex property data index
    auto complex_data_index = static_cast<uint>(-1);

    if (prop->_dataType != Property::DataType::PlainData && !disable_get && !IsEnumSet(prop->_accessType, Property::AccessType::VirtualMask)) {
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
    prop->_podDataOffset = data_base_offset;
    prop->_isReadable = !disable_get;
    prop->_isWritable = !disable_set;

    _registeredProperties.emplace_back(prop);

    RUNTIME_ASSERT(_registeredPropertiesLookup.count(prop->_propName) == 0);
    _registeredPropertiesLookup.emplace(prop->_propName, prop);

    for (const auto& alias : prop->_propNameAliases) {
        RUNTIME_ASSERT(_registeredPropertiesLookup.count(alias) == 0);
        _registeredPropertiesLookup.emplace(alias, prop);
    }

    // Fix plain data data offsets
    if (prop->_podDataOffset != static_cast<uint>(-1)) {
        if (IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) {
            prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size());
        }
        else if (IsEnumSet(prop->_accessType, Property::AccessType::PrivateMask)) {
            prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size()) + static_cast<uint>(_protectedPodDataSpace.size());
        }
    }

    for (auto* other_prop : _registeredProperties) {
        if (other_prop->_podDataOffset == static_cast<uint>(-1) || other_prop == prop) {
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
