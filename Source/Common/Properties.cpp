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

auto PropertyRawData::Alloc(size_t size) noexcept -> uint8*
{
    NO_STACK_TRACE_ENTRY();

    _dataSize = size;
    _passedPtr = nullptr;

    if (size > LOCAL_BUF_SIZE) {
        _useDynamic = true;
        _dynamicBuf = SafeAlloc::MakeUniqueArr<uint8>(size);
    }
    else {
        _useDynamic = false;
    }

    return static_cast<uint8*>(GetPtr());
}

void PropertyRawData::Pass(const_span<uint8> value) noexcept
{
    NO_STACK_TRACE_ENTRY();

    _passedPtr = const_cast<uint8*>(value.data());
    _dataSize = value.size();
    _useDynamic = false;
}

void PropertyRawData::Pass(const void* value, size_t size) noexcept
{
    NO_STACK_TRACE_ENTRY();

    _passedPtr = static_cast<uint8*>(const_cast<void*>(value));
    _dataSize = size;
    _useDynamic = false;
}

void PropertyRawData::StoreIfPassed() noexcept
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
    NO_STACK_TRACE_ENTRY();
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

Properties::Properties(const PropertyRegistrator* registrator) noexcept :
    _registrator {registrator}
{
    STACK_TRACE_ENTRY();

    STRONG_ASSERT(_registrator);

    if (!_registrator->_registeredProperties.empty()) {
        AllocData();
    }
}

void Properties::AllocData() noexcept
{
    STACK_TRACE_ENTRY();

    STRONG_ASSERT(!_podData);
    STRONG_ASSERT(!_registrator->_registeredProperties.empty());

    _podData = SafeAlloc::MakeUniqueArr<uint8>(_registrator->_wholePodDataSize);
    std::memset(_podData.get(), 0, _registrator->_wholePodDataSize);

    _complexData = SafeAlloc::MakeUniqueArr<pair<unique_ptr<uint8[]>, size_t>>(_registrator->_complexProperties.size());
}

auto Properties::Copy() const noexcept -> Properties
{
    STACK_TRACE_ENTRY();

    Properties props(_registrator);

    std::memcpy(props._podData.get(), _podData.get(), _registrator->_wholePodDataSize);

    for (size_t i = 0; i < _registrator->_complexProperties.size(); i++) {
        if (_complexData[i].first) {
            props._complexData[i].first = SafeAlloc::MakeUniqueArr<uint8>(_complexData[i].second);
            props._complexData[i].second = _complexData[i].second;
            std::memcpy(props._complexData[i].first.get(), _complexData[i].first.get(), _complexData[i].second);
        }
    }

    return props;
}

void Properties::CopyFrom(const Properties& other) noexcept
{
    STACK_TRACE_ENTRY();

    STRONG_ASSERT(_registrator == other._registrator);

    // Copy PlainData data
    std::memcpy(_podData.get(), other._podData.get(), _registrator->_wholePodDataSize);

    // Copy complex data
    for (const auto* prop : _registrator->_complexProperties) {
        SetRawData(prop, {other._complexData[prop->_complexDataIndex].first.get(), other._complexData[prop->_complexDataIndex].second});
    }
}

void Properties::StoreAllData(vector<uint8>& all_data, set<hstring>& str_hashes) const
{
    STACK_TRACE_ENTRY();

    all_data.clear();
    auto writer = DataWriter(all_data);

    // Store plain properties data
    writer.Write<uint>(static_cast<uint>(_registrator->_wholePodDataSize));

    int start_pos = -1;
    constexpr int seek_step = 3;

    for (size_t i = 0; i < _registrator->_wholePodDataSize; i++) {
        if (_podData[i] != 0) {
            if (start_pos == -1) {
                start_pos = static_cast<int>(i);
            }

            i += seek_step;
        }
        else {
            if (start_pos != -1) {
                const size_t len = i - start_pos;
                writer.Write<uint>(static_cast<uint>(start_pos));
                writer.Write<uint>(static_cast<uint>(len));
                writer.WritePtr(_podData.get() + start_pos, len);

                start_pos = -1;
            }
        }
    }

    if (start_pos != -1) {
        const size_t len = _registrator->_wholePodDataSize - start_pos;
        writer.Write<uint>(static_cast<uint>(start_pos));
        writer.Write<uint>(static_cast<uint>(len));
        writer.WritePtr(_podData.get() + start_pos, len);
    }

    writer.Write<uint>(0);
    writer.Write<uint>(0);

    // Store complex properties
    writer.Write<uint>(static_cast<uint>(_registrator->_complexProperties.size()));

    for (const auto* prop : _registrator->_complexProperties) {
        RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        writer.Write<uint>(static_cast<uint>(_complexData[prop->_complexDataIndex].second));
        writer.WritePtr(_complexData[prop->_complexDataIndex].first.get(), _complexData[prop->_complexDataIndex].second);
    }

    // Store hashes
    const auto add_hash = [&str_hashes, this](const string& str) {
        if (!str.empty()) {
            const auto hstr = _registrator->_hashResolver.ToHashedString(str);
            str_hashes.emplace(hstr);
        }
    };

    for (auto&& prop : _registrator->_registeredProperties) {
        if (!prop->IsDisabled() && (prop->IsBaseTypeHash() || prop->IsDictKeyHash())) {
            const auto value = PropertiesSerializator::SavePropertyToValue(this, prop.get(), _registrator->_hashResolver, _registrator->_nameResolver);

            if (value.Type() == AnyData::ValueType::String) {
                add_hash(value.AsString());
            }
            else if (value.Type() == AnyData::ValueType::Array) {
                const auto& arr = value.AsArray();

                for (auto&& arr_entry : arr) {
                    add_hash(arr_entry.AsString());
                }
            }
            else if (value.Type() == AnyData::ValueType::Dict) {
                const auto& dict = value.AsDict();

                if (prop->IsDictKeyHash()) {
                    for (auto&& dict_entry : dict) {
                        add_hash(dict_entry.first);
                    }
                }

                if (prop->IsBaseTypeHash()) {
                    for (auto&& dict_entry : dict) {
                        if (dict_entry.second.Type() == AnyData::ValueType::Array) {
                            const auto& dict_arr = dict_entry.second.AsArray();

                            for (auto&& dict_arr_entry : dict_arr) {
                                add_hash(dict_arr_entry.AsString());
                            }
                        }
                        else {
                            add_hash(dict_entry.second.AsString());
                        }
                    }
                }
            }
            else {
                UNREACHABLE_PLACE();
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

        std::memcpy(_podData.get() + start_pos, reader.ReadPtr<uint8>(len), len);
    }

    // Read complex properties
    const auto complex_props_count = reader.Read<uint>();
    RUNTIME_ASSERT(complex_props_count == _registrator->_complexProperties.size());

    for (const auto* prop : _registrator->_complexProperties) {
        RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto data_size = reader.Read<uint>();
        SetRawData(prop, {reader.ReadPtr<uint8>(data_size), data_size});
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
    _storeData->push_back(_podData.get());
    _storeDataSizes->push_back(static_cast<uint>(_registrator->_publicPodDataSpace.size()) + (with_protected ? static_cast<uint>(_registrator->_protectedPodDataSpace.size()) : 0));

    // Filter complex data to send
    for (size_t i = 0; i < _storeDataComplexIndices->size();) {
        const auto& prop = _registrator->_registeredProperties[(*_storeDataComplexIndices)[i]];
        RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);

        if (!_complexData[prop->_complexDataIndex].first) {
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
            const auto& prop = _registrator->_registeredProperties[index];
            _storeData->push_back(_complexData[prop->_complexDataIndex].first.get());
            _storeDataSizes->push_back(static_cast<uint>(_complexData[prop->_complexDataIndex].second));
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
        std::memcpy(_podData.get(), all_data[0], all_data_sizes[0]);
    }

    // Restore complex data
    if (all_data.size() > 1) {
        const uint comlplex_data_count = all_data_sizes[1] / sizeof(uint16);
        RUNTIME_ASSERT(comlplex_data_count > 0);
        vector<uint16> complex_indicies(comlplex_data_count);
        std::memcpy(complex_indicies.data(), all_data[1], all_data_sizes[1]);

        for (size_t i = 0; i < complex_indicies.size(); i++) {
            RUNTIME_ASSERT(complex_indicies[i] < _registrator->_registeredProperties.size());
            const auto& prop = _registrator->_registeredProperties[complex_indicies[i]];
            RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
            const auto data_size = all_data_sizes[2 + i];
            const auto* data = all_data[2 + i];
            SetRawData(prop.get(), {data, data_size});
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

void Properties::ApplyFromText(const map<string, string>& key_values)
{
    STACK_TRACE_ENTRY();

    size_t errors = 0;

    for (auto&& [key, value] : key_values) {
        // Skip technical fields
        if (key.empty() || key[0] == '$' || key[0] == '_') {
            continue;
        }

        // Find property
        bool is_component;
        const auto* prop = _registrator->FindProperty(key, &is_component);

        if (prop == nullptr) {
            if (is_component) {
                continue;
            }

            WriteLog("Unknown property {}", key);
            errors++;
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
            errors++;
            continue;
        }

        // Parse
        try {
            ApplyPropertyFromText(prop, value);
        }
        catch (const std::exception& ex) {
            WriteLog("Error parsing property {}", key);
            ReportExceptionAndContinue(ex);
            errors++;
        }
    }

    if (errors != 0) {
        throw PropertiesException("Error parsing properties", errors);
    }
}

auto Properties::SaveToText(const Properties* base) const -> map<string, string>
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!base || _registrator == base->_registrator);

    map<string, string> key_values;

    for (const auto& prop : _registrator->_registeredProperties) {
        if (prop->IsDisabled()) {
            continue;
        }
        if (prop->IsVirtual()) {
            continue;
        }
        if (prop->IsTemporary()) {
            continue;
        }

        // Skip same as in base or zero values
        if (base != nullptr) {
            if (prop->_podDataOffset != Property::INVALID_DATA_MARKER) {
                const auto* pod_data = &_podData[prop->_podDataOffset];
                const auto* base_pod_data = &base->_podData[prop->_podDataOffset];

                if (std::memcmp(pod_data, base_pod_data, prop->_baseType.Size) == 0) {
                    continue;
                }
            }
            else {
                const auto& complex_data = _complexData[prop->_complexDataIndex];
                const auto& base_complex_data = base->_complexData[prop->_complexDataIndex];

                if (!complex_data.first && !base_complex_data.first) {
                    continue;
                }
                if (complex_data.second == base_complex_data.second && std::memcmp(complex_data.first.get(), base_complex_data.first.get(), complex_data.second) == 0) {
                    continue;
                }
            }
        }
        else {
            if (prop->_podDataOffset != Property::INVALID_DATA_MARKER) {
                const auto* pod_data = &_podData[prop->_podDataOffset];
                const auto* pod_data_end = pod_data + prop->_baseType.Size;

                while (pod_data != pod_data_end && *pod_data == 0) {
                    ++pod_data;
                }

                if (pod_data == pod_data_end) {
                    continue;
                }
            }
            else {
                if (!_complexData[prop->_complexDataIndex].first) {
                    continue;
                }
            }
        }

        // Serialize to text and store in map
        key_values.emplace(prop->_propName, SavePropertyToText(prop.get()));
    }

    return key_values;
}

void Properties::ApplyPropertyFromText(const Property* prop, string_view text)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(_registrator == prop->_registrator);
    RUNTIME_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER || prop->_complexDataIndex != Property::INVALID_DATA_MARKER);

    const auto is_dict = prop->IsDict();
    const auto is_array = prop->IsArray() || prop->IsDictOfArray() || prop->IsBaseTypeStruct();

    AnyData::ValueType value_type;

    if (prop->IsString() || prop->IsArrayOfString() || prop->IsDictOfArrayOfString() || prop->IsBaseTypeHash() || prop->IsBaseTypeEnum() || prop->IsBaseTypeStruct()) {
        value_type = AnyData::ValueType::String;
    }
    else if (prop->IsBaseTypeInt()) {
        value_type = AnyData::ValueType::Int64;
    }
    else if (prop->IsBaseTypeBool()) {
        value_type = AnyData::ValueType::Bool;
    }
    else if (prop->IsBaseTypeFloat()) {
        value_type = AnyData::ValueType::Double;
    }
    else {
        UNREACHABLE_PLACE();
    }

    const auto value = AnyData::ParseValue(string(text), is_dict, is_array, value_type);
    PropertiesSerializator::LoadPropertyFromValue(this, prop, value, _registrator->_hashResolver, _registrator->_nameResolver);
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

    if (prop->IsPlainData()) {
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

    if (prop->IsPlainData()) {
        STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        return {&_podData[prop->_podDataOffset], prop->_baseType.Size};
    }
    else {
        STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto& complex_data = _complexData[prop->_complexDataIndex];
        return {complex_data.first.get(), complex_data.second};
    }
}

auto Properties::GetRawData(const Property* prop) noexcept -> span<uint8>
{
    NO_STACK_TRACE_ENTRY();

    STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->IsPlainData()) {
        STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        return {&_podData[prop->_podDataOffset], prop->_baseType.Size};
    }
    else {
        STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        auto& complex_data = _complexData[prop->_complexDataIndex];
        return {complex_data.first.get(), complex_data.second};
    }
}

auto Properties::GetRawDataSize(const Property* prop) const noexcept -> size_t
{
    NO_STACK_TRACE_ENTRY();

    STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->IsPlainData()) {
        STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        return prop->_baseType.Size;
    }
    else {
        STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto& complex_data = _complexData[prop->_complexDataIndex];
        return complex_data.second;
    }
}

void Properties::SetRawData(const Property* prop, const_span<uint8> raw_data) noexcept
{
    NO_STACK_TRACE_ENTRY();

    STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->IsPlainData()) {
        STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        STRONG_ASSERT(prop->GetBaseSize() == raw_data.size());
        std::memcpy(_podData.get() + prop->_podDataOffset, raw_data.data(), raw_data.size());
    }
    else {
        STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        auto& complex_data = _complexData[prop->_complexDataIndex];
        complex_data.first = SafeAlloc::MakeUniqueArr<uint8>(raw_data.size());
        complex_data.second = raw_data.size();
        std::memcpy(complex_data.first.get(), raw_data.data(), raw_data.size());
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

        SetRawData(prop, {prop_data.GetPtrAs<uint8>(), prop_data.GetSize()});

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

    RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type_info = prop->GetBaseTypeInfo();

    if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            return static_cast<int>(GetValue<uint8>(prop));
        }
        if (base_type_info.Size == 2) {
            return static_cast<int>(GetValue<uint16>(prop));
        }
        if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                return static_cast<int>(GetValue<int>(prop));
            }
            else {
                return static_cast<int>(GetValue<uint>(prop));
            }
        }
    }
    else if (base_type_info.IsBool) {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            return static_cast<int>(GetValue<float>(prop));
        }
        if (base_type_info.IsDoubleFloat) {
            return static_cast<int>(GetValue<double>(prop));
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            return static_cast<int>(GetValue<char>(prop));
        }
        if (base_type_info.Size == 2) {
            return static_cast<int>(GetValue<int16>(prop));
        }
        if (base_type_info.Size == 4) {
            return static_cast<int>(GetValue<int>(prop));
        }
        if (base_type_info.Size == 8) {
            return static_cast<int>(GetValue<int64>(prop));
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            return static_cast<int>(GetValue<uint8>(prop));
        }
        if (base_type_info.Size == 2) {
            return static_cast<int>(GetValue<uint16>(prop));
        }
        if (base_type_info.Size == 4) {
            return static_cast<int>(GetValue<uint>(prop));
        }
    }

    throw PropertiesException("Invalid property for get as int", prop->GetName());
}

auto Properties::GetPlainDataValueAsAny(const Property* prop) const -> any_t
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type_info = prop->GetBaseTypeInfo();

    if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            return any_t {strex("{}", GetValue<uint8>(prop))};
        }
        if (base_type_info.Size == 2) {
            return any_t {strex("{}", GetValue<uint16>(prop))};
        }
        if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                return any_t {strex("{}", GetValue<int>(prop))};
            }
            else {
                return any_t {strex("{}", GetValue<uint>(prop))};
            }
        }
    }
    else if (base_type_info.IsBool) {
        return any_t {strex("{}", GetValue<bool>(prop))};
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            return any_t {strex("{}", GetValue<float>(prop))};
        }
        if (base_type_info.IsDoubleFloat) {
            return any_t {strex("{}", GetValue<double>(prop))};
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            return any_t {strex("{}", GetValue<char>(prop))};
        }
        if (base_type_info.Size == 2) {
            return any_t {strex("{}", GetValue<int16>(prop))};
        }
        if (base_type_info.Size == 4) {
            return any_t {strex("{}", GetValue<int>(prop))};
        }
        if (base_type_info.Size == 8) {
            return any_t {strex("{}", GetValue<int64>(prop))};
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            return any_t {strex("{}", GetValue<uint8>(prop))};
        }
        if (base_type_info.Size == 2) {
            return any_t {strex("{}", GetValue<uint16>(prop))};
        }
        if (base_type_info.Size == 4) {
            return any_t {strex("{}", GetValue<uint>(prop))};
        }
    }

    throw PropertiesException("Invalid property for get as any", prop->GetName());
}

void Properties::SetPlainDataValueAsInt(const Property* prop, int value)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type_info = prop->GetBaseTypeInfo();

    if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(value));
        }
        else if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                SetValue<int>(prop, value);
            }
            else {
                SetValue<uint>(prop, static_cast<uint>(value));
            }
        }
    }
    else if (base_type_info.IsBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            SetValue<float>(prop, static_cast<float>(value));
        }
        else if (base_type_info.IsDoubleFloat) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<char>(prop, static_cast<char>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<int16>(prop, static_cast<int16>(value));
        }
        else if (base_type_info.Size == 4) {
            SetValue<int>(prop, value);
        }
        else if (base_type_info.Size == 8) {
            SetValue<int64>(prop, static_cast<int64>(value));
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(value));
        }
        else if (base_type_info.Size == 4) {
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

    RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type_info = prop->GetBaseTypeInfo();

    if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(strex(value).toInt()));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(strex(value).toInt()));
        }
        else if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                SetValue<int>(prop, strex(value).toInt());
            }
            else {
                SetValue<uint>(prop, strex(value).toUInt());
            }
        }
    }
    else if (base_type_info.IsBool) {
        SetValue<bool>(prop, strex(value).toBool());
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            SetValue<float>(prop, strex(value).toFloat());
        }
        else if (base_type_info.IsDoubleFloat) {
            SetValue<double>(prop, strex(value).toDouble());
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<char>(prop, static_cast<char>(strex(value).toInt()));
        }
        else if (base_type_info.Size == 2) {
            SetValue<int16>(prop, static_cast<int16>(strex(value).toInt()));
        }
        else if (base_type_info.Size == 4) {
            SetValue<int>(prop, static_cast<int>(strex(value).toInt()));
        }
        else if (base_type_info.Size == 8) {
            SetValue<int64>(prop, static_cast<int64>(strex(value).toInt64()));
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(strex(value).toInt()));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(strex(value).toInt()));
        }
        else if (base_type_info.Size == 4) {
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

    const auto* prop = _registrator->GetPropertyByIndex(property_index);

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

    const auto* prop = _registrator->GetPropertyByIndex(property_index);

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

    const auto* prop = _registrator->GetPropertyByIndex(property_index);

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

    const auto* prop = _registrator->GetPropertyByIndex(property_index);

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

    const auto* prop = _registrator->GetPropertyByIndex(property_index);

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

    const auto& base_type_info = prop->GetBaseTypeInfo();

    if (base_type_info.IsHash) {
        SetValue<hstring>(prop, ResolveHash(value));
    }
    else if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(value));
        }
        else if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                SetValue<int>(prop, static_cast<int>(value));
            }
            else {
                SetValue<uint>(prop, static_cast<uint>(value));
            }
        }
    }
    else if (base_type_info.IsBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            SetValue<float>(prop, static_cast<float>(value));
        }
        else if (base_type_info.IsDoubleFloat) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<char>(prop, static_cast<char>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<int16>(prop, static_cast<int16>(value));
        }
        else if (base_type_info.Size == 4) {
            SetValue<int>(prop, value);
        }
        else if (base_type_info.Size == 8) {
            SetValue<int64>(prop, static_cast<int64>(value));
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, static_cast<uint8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, static_cast<uint16>(value));
        }
        else if (base_type_info.Size == 4) {
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

    const auto* prop = _registrator->GetPropertyByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't set any value to disabled property", prop->GetName());
    }
    if (prop->IsVirtual()) {
        throw PropertiesException("Can't set any value to virtual property", prop->GetName());
    }

    ApplyPropertyFromText(prop, value);
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
}

PropertyRegistrator::~PropertyRegistrator()
{
    STACK_TRACE_ENTRY();
}

void PropertyRegistrator::RegisterComponent(string_view name)
{
    STACK_TRACE_ENTRY();

    const auto name_hash = _hashResolver.ToHashedString(name);

    RUNTIME_ASSERT(!_registeredComponents.count(name_hash));
    _registeredComponents.insert(name_hash);
}

auto PropertyRegistrator::GetPropertyByIndex(int property_index) const noexcept -> const Property*
{
    STACK_TRACE_ENTRY();

    if (property_index >= 0 && static_cast<size_t>(property_index) < _registeredProperties.size()) {
        return _registeredProperties[property_index].get();
    }

    return nullptr;
}

auto PropertyRegistrator::FindProperty(string_view property_name, bool* is_component) const -> const Property*
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

void PropertyRegistrator::RegisterProperty(const const_span<string_view>& flags)
{
    STACK_TRACE_ENTRY();

    // Todo: validate property name identifier

    RUNTIME_ASSERT(flags.size() >= 3);

    auto&& prop = SafeAlloc::MakeUnique<Property>(this);

    prop->_propName = flags[0];
    RUNTIME_ASSERT(_accessMap.count(flags[1]) != 0);
    prop->_accessType = _accessMap[flags[1]];

    const auto type_tok = strex(flags[2]).split('.');
    RUNTIME_ASSERT(!type_tok.empty());

    if (type_tok[0] == "dict") {
        RUNTIME_ASSERT(type_tok.size() >= 3);
        const auto key_type = _nameResolver.ResolveBaseType(type_tok[1]);

        prop->_isDict = true;
        prop->_dictKeyType = key_type;
        prop->_isDictKeyHash = key_type.IsHash;
        prop->_isDictKeyEnum = key_type.IsEnum;
        prop->_isDictKeyString = key_type.IsString;

        if (type_tok[2] == "arr") {
            RUNTIME_ASSERT(type_tok.size() >= 4);
            const auto value_type = _nameResolver.ResolveBaseType(type_tok[3]);

            prop->_baseType = value_type;
            prop->_isDictOfArray = true;
            prop->_isDictOfArrayOfString = value_type.IsString;
            prop->_asFullTypeName = "dict<" + key_type.TypeName + ", " + value_type.TypeName + "[]>";
        }
        else {
            const auto value_type = _nameResolver.ResolveBaseType(type_tok[2]);

            prop->_baseType = value_type;
            prop->_isDictOfString = value_type.IsString;
            prop->_asFullTypeName = "dict<" + key_type.TypeName + ", " + value_type.TypeName + ">";
        }
    }
    else if (type_tok[0] == "arr") {
        RUNTIME_ASSERT(type_tok.size() >= 2);
        const auto value_type = _nameResolver.ResolveBaseType(type_tok[1]);

        prop->_isArray = true;
        prop->_baseType = value_type;
        prop->_isArrayOfString = value_type.IsString;
        prop->_asFullTypeName = value_type.TypeName + "[]";
    }
    else {
        const auto value_type = _nameResolver.ResolveBaseType(type_tok[0]);

        prop->_baseType = value_type;
        prop->_asFullTypeName = value_type.TypeName;

        if (value_type.IsString) {
            prop->_isString = true;
        }
        else {
            prop->_isPlainData = true;
        }
    }

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

    for (size_t i = 3; i < flags.size(); i++) {
        const auto check_next_param = [&flags, i, &prop] {
            if (i + 2 >= flags.size() || flags[i + 1] != "=") {
                throw PropertyRegistrationException("Expected property flag = value", prop->_propName, flags[i], i, flags.size());
            }
        };

        if (flags[i] == "Group") {
            check_next_param();

            const auto& group = flags[i + 2];
            if (const auto it = _propertyGroups.find(string(group)); it != _propertyGroups.end()) {
                it->second.push_back(prop.get());
            }
            else {
                _propertyGroups.emplace(group, vector<const Property*> {prop.get()});
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
            if (!prop->IsBaseTypeHash()) {
                throw PropertyRegistrationException("Expected hstring for Resource flag", prop->_propName);
            }

            prop->_isResourceHash = true;
        }
        else if (flags[i] == "ScriptFuncType") {
            check_next_param();

            if (!prop->IsBaseTypeHash()) {
                throw PropertyRegistrationException("Expected hstring for ScriptFunc flag", prop->_propName);
            }

            prop->_scriptFuncType = flags[i + 2];
            i += 2;
        }
        else if (flags[i] == "Max") {
            check_next_param();

            if (!prop->IsBaseTypeInt() && !prop->IsBaseTypeFloat()) {
                throw PropertyRegistrationException("Expected numeric type for Max flag", prop->_propName);
            }

            if (prop->IsBaseTypeInt()) {
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

            if (!prop->IsBaseTypeInt() && !prop->IsBaseTypeFloat()) {
                throw PropertyRegistrationException("Expected numeric type for Min flag", prop->_propName);
            }

            if (prop->IsBaseTypeInt()) {
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
    const size_t prev_public_space_size = _publicPodDataSpace.size();
    const size_t prev_protected_space_size = _protectedPodDataSpace.size();

    auto pod_data_base_offset = Property::INVALID_DATA_MARKER;

    if (prop->IsPlainData() && !disabled && !prop->IsVirtual()) {
        const bool is_public = IsEnumSet(prop->_accessType, Property::AccessType::PublicMask);
        const bool is_protected = IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask);
        auto& space = (is_public ? _publicPodDataSpace : (is_protected ? _protectedPodDataSpace : _privatePodDataSpace));

        const size_t space_size = space.size();
        size_t space_pos = 0;

        while (space_pos + prop->GetBaseSize() <= space_size) {
            auto fail = false;

            for (uint i = 0; i < prop->GetBaseSize(); i++) {
                if (space[space_pos + i]) {
                    fail = true;
                    break;
                }
            }

            if (!fail) {
                break;
            }

            space_pos += prop->GetBaseSize();
        }

        auto new_size = space_pos + prop->GetBaseSize();

        new_size += 8u - new_size % 8u;

        if (new_size > space.size()) {
            space.resize(new_size);
        }

        for (uint i = 0; i < prop->GetBaseSize(); i++) {
            space[space_pos + i] = true;
        }

        pod_data_base_offset = space_pos;

        _wholePodDataSize = _publicPodDataSpace.size() + _protectedPodDataSpace.size() + _privatePodDataSpace.size();
        RUNTIME_ASSERT((_wholePodDataSize % 8u) == 0);
    }

    // Complex property data index
    auto complex_data_index = Property::INVALID_DATA_MARKER;

    if (!prop->IsPlainData() && !disabled && !prop->IsVirtual()) {
        complex_data_index = _complexProperties.size();
        _complexProperties.emplace_back(prop.get());

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

    RUNTIME_ASSERT(_registeredPropertiesLookup.count(prop->_propName) == 0);
    _registeredPropertiesLookup.emplace(prop->_propName, prop.get());

    // Fix plain data data offsets
    if (prop->_podDataOffset != Property::INVALID_DATA_MARKER) {
        if (IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) {
            prop->_podDataOffset += _publicPodDataSpace.size();
        }
        else if (IsEnumSet(prop->_accessType, Property::AccessType::PrivateMask)) {
            prop->_podDataOffset += _publicPodDataSpace.size() + _protectedPodDataSpace.size();
        }
    }

    for (const auto& other_prop : _registeredProperties) {
        if (other_prop->_podDataOffset == Property::INVALID_DATA_MARKER || other_prop == prop) {
            continue;
        }

        if (IsEnumSet(other_prop->_accessType, Property::AccessType::ProtectedMask)) {
            other_prop->_podDataOffset -= prev_public_space_size;
            other_prop->_podDataOffset += _publicPodDataSpace.size();
        }
        else if (IsEnumSet(other_prop->_accessType, Property::AccessType::PrivateMask)) {
            other_prop->_podDataOffset -= prev_public_space_size + prev_protected_space_size;
            other_prop->_podDataOffset += _publicPodDataSpace.size() + _protectedPodDataSpace.size();
        }
    }

    _registeredProperties.emplace_back(std::move(prop));
}
