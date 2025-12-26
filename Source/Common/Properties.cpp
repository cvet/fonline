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

#include "Properties.h"
#include "PropertiesSerializator.h"

FO_BEGIN_NAMESPACE();

auto PropertyRawData::GetPtr() noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_passedPtr) {
        return _passedPtr.get();
    }

    return _useDynamic ? _dynamicBuf.get() : _localBuf;
}

auto PropertyRawData::Alloc(size_t size) noexcept -> uint8*
{
    FO_NO_STACK_TRACE_ENTRY();

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

void PropertyRawData::Pass(span<const uint8> value) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _passedPtr = const_cast<uint8*>(value.data());
    _dataSize = value.size();
    _useDynamic = false;
}

void PropertyRawData::Pass(const void* value, size_t size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    _passedPtr = static_cast<uint8*>(const_cast<void*>(value));
    _dataSize = size;
    _useDynamic = false;
}

void PropertyRawData::StoreIfPassed() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_passedPtr != nullptr) {
        PropertyRawData tmp_data;
        tmp_data.Set(_passedPtr.get(), _dataSize);
        *this = std::move(tmp_data);
    }
}

Property::Property(const PropertyRegistrator* registrator) :
    _registrator {registrator}
{
    FO_NO_STACK_TRACE_ENTRY();
}

void Property::SetGetter(PropertyGetCallback getter) const
{
    FO_STACK_TRACE_ENTRY();

    _getter = std::move(getter);
}

void Property::AddSetter(PropertySetCallback setter) const
{
    FO_STACK_TRACE_ENTRY();

    _setters.emplace(_setters.begin(), std::move(setter));
}

void Property::AddPostSetter(PropertyPostSetCallback setter) const
{
    FO_STACK_TRACE_ENTRY();

    _postSetters.emplace(_postSetters.begin(), std::move(setter));
}

Properties::Properties(const PropertyRegistrator* registrator) noexcept :
    _registrator {registrator}
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator);

    if (_registrator->_registeredProperties.size() > 1) {
        AllocData();
    }
}

void Properties::AllocData() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!_podData);
    FO_STRONG_ASSERT(_registrator->_registeredProperties.size() > 1);

    _podData = SafeAlloc::MakeUniqueArr<uint8>(_registrator->_wholePodDataSize);
    MemFill(_podData.get(), 0, _registrator->_wholePodDataSize);

    _complexData = SafeAlloc::MakeUniqueArr<pair<unique_arr_ptr<uint8>, size_t>>(_registrator->_complexProperties.size());
}

auto Properties::Copy() const noexcept -> Properties
{
    FO_STACK_TRACE_ENTRY();

    Properties props = Properties(_registrator.get());

    MemCopy(props._podData.get(), _podData.get(), _registrator->_wholePodDataSize);

    for (size_t i = 0; i < _registrator->_complexProperties.size(); i++) {
        if (_complexData[i].first) {
            props._complexData[i].first = SafeAlloc::MakeUniqueArr<uint8>(_complexData[i].second);
            props._complexData[i].second = _complexData[i].second;
            MemCopy(props._complexData[i].first.get(), _complexData[i].first.get(), _complexData[i].second);
        }
    }

    return props;
}

void Properties::CopyFrom(const Properties& other) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == other._registrator);

    // Copy plain data
    MemCopy(_podData.get(), other._podData.get(), _registrator->_wholePodDataSize);

    // Copy complex data
    for (const auto& prop : _registrator->_complexProperties) {
        SetRawData(prop.get(), {other._complexData[*prop->_complexDataIndex].first.get(), other._complexData[*prop->_complexDataIndex].second});
    }
}

void Properties::StoreAllData(vector<uint8>& all_data, set<hstring>& str_hashes) const
{
    FO_STACK_TRACE_ENTRY();

    all_data.clear();
    auto writer = DataWriter(all_data);

    // Store plain properties data
    writer.Write<uint32>(numeric_cast<uint32>(_registrator->_wholePodDataSize));

    int32 start_pos = -1;
    constexpr int32 seek_step = 3;

    for (size_t i = 0; i < _registrator->_wholePodDataSize; i++) {
        if (_podData[i] != 0) {
            if (start_pos == -1) {
                start_pos = numeric_cast<int32>(i);
            }

            i += seek_step;
        }
        else {
            if (start_pos != -1) {
                const size_t len = i - start_pos;
                writer.Write<uint32>(numeric_cast<uint32>(start_pos));
                writer.Write<uint32>(numeric_cast<uint32>(len));
                writer.WritePtr(_podData.get() + start_pos, len);

                start_pos = -1;
            }
        }
    }

    if (start_pos != -1) {
        const size_t len = _registrator->_wholePodDataSize - start_pos;
        writer.Write<uint32>(numeric_cast<uint32>(start_pos));
        writer.Write<uint32>(numeric_cast<uint32>(len));
        writer.WritePtr(_podData.get() + start_pos, len);
    }

    writer.Write<uint32>(const_numeric_cast<uint32>(0));
    writer.Write<uint32>(const_numeric_cast<uint32>(0));

    // Store complex properties
    writer.Write<uint32>(numeric_cast<uint32>(_registrator->_complexProperties.size()));

    for (const auto& prop : _registrator->_complexProperties) {
        FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());
        writer.Write<uint32>(numeric_cast<uint32>(_complexData[*prop->_complexDataIndex].second));
        writer.WritePtr(_complexData[*prop->_complexDataIndex].first.get(), _complexData[*prop->_complexDataIndex].second);
    }

    // Store hashes
    const auto add_hash = [&str_hashes, this](const string& str) {
        if (!str.empty()) {
            const auto hstr = _registrator->_hashResolver->ToHashedString(str);
            str_hashes.emplace(hstr);
        }
    };

    for (const auto& prop : _registrator->_registeredProperties) {
        if (prop && !prop->IsDisabled() && (prop->IsBaseTypeHash() || prop->IsDictKeyHash())) {
            const auto value = PropertiesSerializator::SavePropertyToValue(this, prop.get(), *_registrator->_hashResolver, *_registrator->_nameResolver);

            if (value.Type() == AnyData::ValueType::String) {
                add_hash(value.AsString());
            }
            else if (value.Type() == AnyData::ValueType::Array) {
                const auto& arr = value.AsArray();

                for (const auto& arr_entry : arr) {
                    add_hash(arr_entry.AsString());
                }
            }
            else if (value.Type() == AnyData::ValueType::Dict) {
                const auto& dict = value.AsDict();

                if (prop->IsDictKeyHash()) {
                    for (const auto& key : dict | std::views::keys) {
                        add_hash(key);
                    }
                }

                if (prop->IsBaseTypeHash()) {
                    for (const auto& dict_value : dict | std::views::values) {
                        if (dict_value.Type() == AnyData::ValueType::Array) {
                            const auto& dict_arr = dict_value.AsArray();

                            for (const auto& dict_arr_entry : dict_arr) {
                                add_hash(dict_arr_entry.AsString());
                            }
                        }
                        else {
                            add_hash(dict_value.AsString());
                        }
                    }
                }
            }
            else {
                FO_UNREACHABLE_PLACE();
            }
        }
    }
}

void Properties::RestoreAllData(const vector<uint8>& all_data)
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader(all_data);

    // Read plain properties data
    const auto whole_pod_data_size = reader.Read<uint32>();
    FO_RUNTIME_ASSERT_STR(whole_pod_data_size == _registrator->_wholePodDataSize, "Run ForceBakeResources");

    while (true) {
        const auto start_pos = reader.Read<uint32>();
        const auto len = reader.Read<uint32>();

        if (start_pos == 0 && len == 0) {
            break;
        }

        MemCopy(_podData.get() + start_pos, reader.ReadPtr<uint8>(len), len);
    }

    // Read complex properties
    const auto complex_props_count = reader.Read<uint32>();
    FO_RUNTIME_ASSERT(complex_props_count == _registrator->_complexProperties.size());

    for (const auto& prop : _registrator->_complexProperties) {
        FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());
        const auto data_size = reader.Read<uint32>();
        SetRawData(prop.get(), {reader.ReadPtr<uint8>(data_size), data_size});
    }

    reader.VerifyEnd();
}

void Properties::StoreData(bool with_protected, vector<const uint8*>** all_data, vector<uint32>** all_data_sizes) const
{
    FO_STACK_TRACE_ENTRY();

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
    _storeDataSizes->push_back(numeric_cast<uint32>(_registrator->_publicPodDataSpace.size()) + (with_protected ? numeric_cast<uint32>(_registrator->_protectedPodDataSpace.size()) : 0));

    // Filter complex data to send
    for (size_t i = 0; i < _storeDataComplexIndices->size();) {
        const auto& prop = _registrator->_registeredProperties[(*_storeDataComplexIndices)[i]];
        FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());

        if (!_complexData[*prop->_complexDataIndex].first) {
            _storeDataComplexIndices->erase(_storeDataComplexIndices->begin() + numeric_cast<int32>(i));
        }
        else {
            i++;
        }
    }

    // Store complex properties data
    if (!_storeDataComplexIndices->empty()) {
        _storeData->push_back(reinterpret_cast<uint8*>(_storeDataComplexIndices->data()));
        _storeDataSizes->push_back(numeric_cast<uint32>(_storeDataComplexIndices->size()) * sizeof(uint16));

        for (const auto index : *_storeDataComplexIndices) {
            const auto& prop = _registrator->_registeredProperties[index];
            _storeData->push_back(_complexData[*prop->_complexDataIndex].first.get());
            _storeDataSizes->push_back(numeric_cast<uint32>(_complexData[*prop->_complexDataIndex].second));
        }
    }
}

void Properties::RestoreData(const vector<const uint8*>& all_data, const vector<uint32>& all_data_sizes)
{
    FO_STACK_TRACE_ENTRY();

    // Restore plain data
    FO_RUNTIME_ASSERT(!all_data_sizes.empty());
    FO_RUNTIME_ASSERT(all_data.size() == all_data_sizes.size());
    const auto public_size = numeric_cast<uint32>(_registrator->_publicPodDataSpace.size());
    const auto protected_size = numeric_cast<uint32>(_registrator->_protectedPodDataSpace.size());
    const auto private_size = numeric_cast<uint32>(_registrator->_privatePodDataSpace.size());
    FO_RUNTIME_ASSERT(all_data_sizes[0] == public_size || all_data_sizes[0] == public_size + protected_size || all_data_sizes[0] == public_size + protected_size + private_size);

    if (all_data_sizes[0] != 0) {
        MemCopy(_podData.get(), all_data[0], all_data_sizes[0]);
    }

    // Restore complex data
    if (all_data.size() > 1) {
        const uint32 comlplex_data_count = all_data_sizes[1] / sizeof(uint16);
        FO_RUNTIME_ASSERT(comlplex_data_count > 0);
        vector<uint16> complex_indicies(comlplex_data_count);
        MemCopy(complex_indicies.data(), all_data[1], all_data_sizes[1]);

        for (size_t i = 0; i < complex_indicies.size(); i++) {
            FO_RUNTIME_ASSERT(complex_indicies[i] > 0);
            FO_RUNTIME_ASSERT(complex_indicies[i] < _registrator->_registeredProperties.size());
            const auto& prop = _registrator->_registeredProperties[complex_indicies[i]];
            FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());
            const auto data_size = all_data_sizes[2 + i];
            const auto* data = all_data[2 + i];
            SetRawData(prop.get(), {data, data_size});
        }
    }
}

void Properties::RestoreData(const vector<vector<uint8>>& all_data)
{
    FO_STACK_TRACE_ENTRY();

    vector<const uint8*> all_data_ext(all_data.size());
    vector<uint32> all_data_sizes(all_data.size());

    for (size_t i = 0; i < all_data.size(); i++) {
        all_data_ext[i] = !all_data[i].empty() ? all_data[i].data() : nullptr;
        all_data_sizes[i] = numeric_cast<uint32>(all_data[i].size());
    }

    RestoreData(all_data_ext, all_data_sizes);
}

void Properties::ApplyFromText(const map<string, string>& key_values)
{
    FO_STACK_TRACE_ENTRY();

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

            WriteLog("Failed to load unknown property {}", key);
            errors++;
            continue;
        }

        if (prop->IsDisabled()) {
            if (_registrator->GetRelation() == PropertiesRelationType::ServerRelative && prop->IsClientOnly()) {
                continue;
            }
            if (_registrator->GetRelation() == PropertiesRelationType::ClientRelative && prop->IsServerOnly()) {
                continue;
            }

            WriteLog("Failed to load disabled property {}", prop->GetName());
            errors++;
            continue;
        }

        if (prop->IsVirtual()) {
            WriteLog("Failed to load virtual property {}", prop->GetName());
            errors++;
            continue;
        }

        if (prop->IsTemporary()) {
            WriteLog("Failed to load temporary property {}", prop->GetName());
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
        throw PropertiesException("Failed to parse properties");
    }
}

auto Properties::SaveToText(const Properties* base) const -> map<string, string>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!base || _registrator == base->_registrator);

    map<string, string> key_values;

    for (const auto& prop : _registrator->_registeredProperties) {
        if (!prop) {
            continue;
        }
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
            if (prop->_podDataOffset.has_value()) {
                const auto* pod_data = &_podData[*prop->_podDataOffset];
                const auto* base_pod_data = &base->_podData[*prop->_podDataOffset];

                if (MemCompare(pod_data, base_pod_data, prop->_baseType.Size)) {
                    continue;
                }
            }
            else {
                const auto& complex_data = _complexData[*prop->_complexDataIndex];
                const auto& base_complex_data = base->_complexData[*prop->_complexDataIndex];

                if (!complex_data.first && !base_complex_data.first) {
                    continue;
                }
                if (complex_data.second == base_complex_data.second && MemCompare(complex_data.first.get(), base_complex_data.first.get(), complex_data.second)) {
                    continue;
                }
            }
        }
        else {
            if (prop->_podDataOffset.has_value()) {
                const auto* pod_data = &_podData[*prop->_podDataOffset];
                const auto* pod_data_end = pod_data + prop->_baseType.Size;

                while (pod_data != pod_data_end && *pod_data == 0) {
                    ++pod_data;
                }

                if (pod_data == pod_data_end) {
                    continue;
                }
            }
            else {
                if (!_complexData[*prop->_complexDataIndex].first) {
                    continue;
                }
            }
        }

        // Serialize to text and store in map
        key_values.emplace(prop->_propName, SavePropertyToText(prop.get()));
    }

    return key_values;
}

auto Properties::CompareData(const Properties& other, span<const Property*> ignore_props, bool ignore_temporary) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_registrator == other._registrator);

    {
        const auto pod_data_size = _registrator->_wholePodDataSize;
        vector<uint8> pod_data1;
        pod_data1.resize(pod_data_size);
        MemCopy(pod_data1.data(), _podData.get(), pod_data_size);
        vector<uint8> pod_data2;
        pod_data2.resize(pod_data_size);
        MemCopy(pod_data2.data(), other._podData.get(), pod_data_size);

        for (const auto* ignore_prop : ignore_props) {
            if (ignore_prop->_podDataOffset.has_value()) {
                const auto offset = ignore_prop->_podDataOffset.value();
                const auto size = ignore_prop->_baseType.Size;
                MemFill(pod_data1.data() + offset, 0, size);
                MemFill(pod_data2.data() + offset, 0, size);
            }
        }

        if (ignore_temporary) {
            for (const auto& prop : _registrator->_registeredProperties) {
                if (prop && prop->IsTemporary() && prop->_podDataOffset.has_value()) {
                    const auto offset = prop->_podDataOffset.value();
                    const auto size = prop->_baseType.Size;
                    MemFill(pod_data1.data() + offset, 0, size);
                    MemFill(pod_data2.data() + offset, 0, size);
                }
            }
        }

        if (!MemCompare(pod_data1.data(), pod_data2.data(), pod_data_size)) {
            return false;
        }
    }

    for (size_t i = 0; i < _registrator->_complexProperties.size(); i++) {
        if (vec_exists(ignore_props, _registrator->_complexProperties[i].get())) {
            continue;
        }
        if (ignore_temporary && _registrator->_complexProperties[i]->IsTemporary()) {
            continue;
        }

        if (_complexData[i].second != other._complexData[i].second) {
            return false;
        }
        if (!MemCompare(_complexData[i].first.get(), other._complexData[i].first.get(), _complexData[i].second)) {
            return false;
        }
    }

    return true;
}

void Properties::ApplyPropertyFromText(const Property* prop, string_view text)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop);
    FO_RUNTIME_ASSERT(_registrator == prop->_registrator);
    FO_RUNTIME_ASSERT(prop->_podDataOffset.has_value() || prop->_complexDataIndex.has_value());

    const auto is_dict = prop->IsDict();
    const auto is_array = prop->IsArray() || prop->IsDictOfArray() || prop->IsBaseTypeComplexStruct();

    AnyData::ValueType value_type;

    if (prop->IsString() || prop->IsArrayOfString() || prop->IsDictOfArrayOfString() || prop->IsBaseTypeHash() || prop->IsBaseTypeEnum() || prop->IsBaseTypeComplexStruct()) {
        value_type = AnyData::ValueType::String;
    }
    else if (prop->IsBaseTypeInt() || (prop->IsBaseTypeSimpleStruct() && prop->GetStructFirstType().IsInt)) {
        value_type = AnyData::ValueType::Int64;
    }
    else if (prop->IsBaseTypeBool() || (prop->IsBaseTypeSimpleStruct() && prop->GetStructFirstType().IsBool)) {
        value_type = AnyData::ValueType::Bool;
    }
    else if (prop->IsBaseTypeFloat() || (prop->IsBaseTypeSimpleStruct() && prop->GetStructFirstType().IsFloat)) {
        value_type = AnyData::ValueType::Float64;
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    const auto value = AnyData::ParseValue(string(text), is_dict, is_array, value_type);
    PropertiesSerializator::LoadPropertyFromValue(this, prop, value, *_registrator->_hashResolver, *_registrator->_nameResolver);
}

auto Properties::SavePropertyToText(const Property* prop) const -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop);
    FO_RUNTIME_ASSERT(_registrator == prop->_registrator);
    FO_RUNTIME_ASSERT(prop->_podDataOffset.has_value() || prop->_complexDataIndex.has_value());

    const auto value = PropertiesSerializator::SavePropertyToValue(this, prop, *_registrator->_hashResolver, *_registrator->_nameResolver);
    return AnyData::ValueToString(value);
}

void Properties::ValidateForRawData(const Property* prop) const noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_registrator != prop->_registrator) {
        throw PropertiesException("Invalid property for raw data", prop->GetName(), _registrator->GetTypeName(), prop->_registrator->GetTypeName());
    }

    if (prop->IsPlainData()) {
        if (!prop->_podDataOffset.has_value()) {
            throw PropertiesException("Invalid pod data offset for raw data", prop->GetName(), _registrator->GetTypeName());
        }
    }
    else {
        if (!prop->_complexDataIndex.has_value()) {
            throw PropertiesException("Invalid complex index for raw data", prop->GetName(), _registrator->GetTypeName());
        }
    }
}

auto Properties::GetRawData(const Property* prop) const noexcept -> span<const uint8>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->IsPlainData()) {
        FO_STRONG_ASSERT(prop->_podDataOffset.has_value());
        return {&_podData[*prop->_podDataOffset], prop->_baseType.Size};
    }
    else {
        FO_STRONG_ASSERT(prop->_complexDataIndex.has_value());
        const auto& complex_data = _complexData[*prop->_complexDataIndex];
        return {complex_data.first.get(), complex_data.second};
    }
}

auto Properties::GetRawDataSize(const Property* prop) const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->IsPlainData()) {
        FO_STRONG_ASSERT(prop->_podDataOffset.has_value());
        return prop->_baseType.Size;
    }
    else {
        FO_STRONG_ASSERT(prop->_complexDataIndex.has_value());
        const auto& complex_data = _complexData[*prop->_complexDataIndex];
        return complex_data.second;
    }
}

void Properties::SetRawData(const Property* prop, span<const uint8> raw_data) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == prop->_registrator);

    if (prop->IsPlainData()) {
        FO_STRONG_ASSERT(prop->_podDataOffset.has_value());
        FO_STRONG_ASSERT(prop->GetBaseSize() == raw_data.size());

        MemCopy(_podData.get() + *prop->_podDataOffset, raw_data.data(), raw_data.size());
    }
    else {
        FO_STRONG_ASSERT(prop->_complexDataIndex.has_value());

        auto& complex_data = _complexData[*prop->_complexDataIndex];

        if (raw_data.size() != complex_data.second) {
            if (!raw_data.empty()) {
                complex_data.first = SafeAlloc::MakeUniqueArr<uint8>(raw_data.size());
                complex_data.second = raw_data.size();
            }
            else {
                complex_data.first.reset();
                complex_data.second = 0;
            }
        }

        MemCopy(complex_data.first.get(), raw_data.data(), raw_data.size());
    }
}

void Properties::SetValueFromData(const Property* prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);
        FO_RUNTIME_ASSERT(!prop->_setters.empty());

        for (const auto& setter : prop->_setters) {
            setter(_entity.get(), prop, prop_data);
        }
    }
    else {
        if (!prop->_setters.empty() && _entity) {
            for (const auto& setter : prop->_setters) {
                setter(_entity.get(), prop, prop_data);
            }
        }

        SetRawData(prop, {prop_data.GetPtrAs<uint8>(), prop_data.GetSize()});

        if (_entity) {
            for (const auto& setter : prop->_postSetters) {
                setter(_entity.get(), prop);
            }
        }
    }
}

auto Properties::GetPlainDataValueAsInt(const Property* prop) const -> int32
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type_info = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseTypeInfo();

    if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            return numeric_cast<int32>(GetValue<uint8>(prop));
        }
        if (base_type_info.Size == 2) {
            return numeric_cast<int32>(GetValue<uint16>(prop));
        }
        if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                return numeric_cast<int32>(GetValue<int32>(prop));
            }
            else {
                return numeric_cast<int32>(GetValue<uint32>(prop));
            }
        }
    }
    else if (base_type_info.IsBool) {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            return iround<int32>(GetValue<float32>(prop));
        }
        if (base_type_info.IsDoubleFloat) {
            return iround<int32>(GetValue<float64>(prop));
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            return numeric_cast<int32>(GetValue<int8>(prop));
        }
        if (base_type_info.Size == 2) {
            return numeric_cast<int32>(GetValue<int16>(prop));
        }
        if (base_type_info.Size == 4) {
            return numeric_cast<int32>(GetValue<int32>(prop));
        }
        if (base_type_info.Size == 8) {
            return numeric_cast<int32>(GetValue<int64>(prop));
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            return numeric_cast<int32>(GetValue<uint8>(prop));
        }
        if (base_type_info.Size == 2) {
            return numeric_cast<int32>(GetValue<uint16>(prop));
        }
        if (base_type_info.Size == 4) {
            return numeric_cast<int32>(GetValue<uint32>(prop));
        }
    }

    throw PropertiesException("Invalid property for get as int", prop->GetName());
}

auto Properties::GetPlainDataValueAsAny(const Property* prop) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type_info = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseTypeInfo();

    if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            return any_t {strex("{}", GetValue<uint8>(prop))};
        }
        if (base_type_info.Size == 2) {
            return any_t {strex("{}", GetValue<uint16>(prop))};
        }
        if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                return any_t {strex("{}", GetValue<int32>(prop))};
            }
            else {
                return any_t {strex("{}", GetValue<uint32>(prop))};
            }
        }
    }
    else if (base_type_info.IsBool) {
        return any_t {strex("{}", GetValue<bool>(prop))};
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            return any_t {strex("{}", GetValue<float32>(prop))};
        }
        if (base_type_info.IsDoubleFloat) {
            return any_t {strex("{}", GetValue<float64>(prop))};
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            return any_t {strex("{}", GetValue<int8>(prop))};
        }
        if (base_type_info.Size == 2) {
            return any_t {strex("{}", GetValue<int16>(prop))};
        }
        if (base_type_info.Size == 4) {
            return any_t {strex("{}", GetValue<int32>(prop))};
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
            return any_t {strex("{}", GetValue<uint32>(prop))};
        }
    }

    throw PropertiesException("Invalid property for get as any", prop->GetName());
}

void Properties::SetPlainDataValueAsInt(const Property* prop, int32 value)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type_info = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseTypeInfo();

    if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(value));
        }
        else if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                SetValue<int32>(prop, value);
            }
            else {
                SetValue<uint32>(prop, numeric_cast<uint32>(value));
            }
        }
    }
    else if (base_type_info.IsBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            SetValue<float32>(prop, numeric_cast<float32>(value));
        }
        else if (base_type_info.IsDoubleFloat) {
            SetValue<float64>(prop, numeric_cast<float64>(value));
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<int8>(prop, numeric_cast<int8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<int16>(prop, numeric_cast<int16>(value));
        }
        else if (base_type_info.Size == 4) {
            SetValue<int32>(prop, value);
        }
        else if (base_type_info.Size == 8) {
            SetValue<int64>(prop, numeric_cast<int64>(value));
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(value));
        }
        else if (base_type_info.Size == 4) {
            SetValue<uint32>(prop, numeric_cast<uint32>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int", prop->GetName());
    }
}

void Properties::SetPlainDataValueAsAny(const Property* prop, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type_info = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseTypeInfo();

    if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(strex(value).to_int32()));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(strex(value).to_int32()));
        }
        else if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                SetValue<int32>(prop, strex(value).to_int32());
            }
            else {
                SetValue<uint32>(prop, strex(value).to_uint32());
            }
        }
    }
    else if (base_type_info.IsBool) {
        SetValue<bool>(prop, strex(value).to_bool());
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            SetValue<float32>(prop, strex(value).to_float32());
        }
        else if (base_type_info.IsDoubleFloat) {
            SetValue<float64>(prop, strex(value).to_float64());
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<int8>(prop, numeric_cast<int8>(strex(value).to_int32()));
        }
        else if (base_type_info.Size == 2) {
            SetValue<int16>(prop, numeric_cast<int16>(strex(value).to_int32()));
        }
        else if (base_type_info.Size == 4) {
            SetValue<int32>(prop, numeric_cast<int32>(strex(value).to_int32()));
        }
        else if (base_type_info.Size == 8) {
            SetValue<int64>(prop, numeric_cast<int64>(strex(value).to_int64()));
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(strex(value).to_int32()));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(strex(value).to_int32()));
        }
        else if (base_type_info.Size == 4) {
            SetValue<uint32>(prop, numeric_cast<uint32>(strex(value).to_int32()));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as any", prop->GetName());
    }
}

auto Properties::GetValueAsInt(int32 property_index) const -> int32
{
    FO_STACK_TRACE_ENTRY();

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

auto Properties::GetValueAsAny(int32 property_index) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

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

void Properties::SetValueAsInt(int32 property_index, int32 value)
{
    FO_STACK_TRACE_ENTRY();

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

void Properties::SetValueAsAny(int32 property_index, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

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

void Properties::SetValueAsIntProps(int32 property_index, int32 value)
{
    FO_STACK_TRACE_ENTRY();

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
    if (!prop->IsMutable()) {
        throw PropertiesException("Can't set integer value to non mutable property", prop->GetName());
    }

    const auto& base_type_info = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseTypeInfo();

    if (base_type_info.IsHash) {
        SetValue<hstring>(prop, ResolveHash(value));
    }
    else if (base_type_info.IsEnum) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(value));
        }
        else if (base_type_info.Size == 4) {
            if (base_type_info.IsEnumSigned) {
                SetValue<int32>(prop, numeric_cast<int32>(value));
            }
            else {
                SetValue<uint32>(prop, numeric_cast<uint32>(value));
            }
        }
    }
    else if (base_type_info.IsBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (base_type_info.IsFloat) {
        if (base_type_info.IsSingleFloat) {
            SetValue<float32>(prop, numeric_cast<float32>(value));
        }
        else if (base_type_info.IsDoubleFloat) {
            SetValue<float64>(prop, numeric_cast<float64>(value));
        }
    }
    else if (base_type_info.IsInt && base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<int8>(prop, numeric_cast<int8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<int16>(prop, numeric_cast<int16>(value));
        }
        else if (base_type_info.Size == 4) {
            SetValue<int32>(prop, value);
        }
        else if (base_type_info.Size == 8) {
            SetValue<int64>(prop, numeric_cast<int64>(value));
        }
    }
    else if (base_type_info.IsInt && !base_type_info.IsSignedInt) {
        if (base_type_info.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(value));
        }
        else if (base_type_info.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(value));
        }
        else if (base_type_info.Size == 4) {
            SetValue<uint32>(prop, numeric_cast<uint32>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int32 props", prop->GetName());
    }
}

void Properties::SetValueAsAnyProps(int32 property_index, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    const auto* prop = _registrator->GetPropertyByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Property not found", property_index);
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't set value to disabled property", prop->GetName());
    }
    if (prop->IsVirtual()) {
        throw PropertiesException("Can't set value to virtual property", prop->GetName());
    }
    if (!prop->IsMutable()) {
        throw PropertiesException("Can't set value to non mutable property", prop->GetName());
    }

    ApplyPropertyFromText(prop, value);
}

auto Properties::ResolveHash(hstring::hash_t h) const -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    return _registrator->_hashResolver->ResolveHash(h);
}

auto Properties::ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    return _registrator->_hashResolver->ResolveHash(h, failed);
}

void Properties::SetValue(const Property* prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsVirtual() && prop->_setters.empty()) {
        throw PropertiesException("Setter not set");
    }

    if (!prop->IsVirtual()) {
        const auto cur_prop_data = GetRawData(prop);

        if (prop_data.GetSize() == cur_prop_data.size() && MemCompare(prop_data.GetPtr(), cur_prop_data.data(), prop_data.GetSize())) {
            return;
        }
    }

    if (!prop->_setters.empty()) {
        for (auto& setter : prop->_setters) {
            setter(_entity.get(), prop, prop_data);
        }
    }

    if (!prop->IsVirtual()) {
        SetRawData(prop, {prop_data.GetPtrAs<uint8>(), prop_data.GetSize()});

        if (!prop->_postSetters.empty()) {
            for (auto& setter : prop->_postSetters) {
                setter(_entity.get(), prop);
            }
        }
    }
}

PropertyRegistrator::PropertyRegistrator(string_view type_name, PropertiesRelationType relation, HashResolver& hash_resolver, NameResolver& name_resolver) :
    _typeName {hash_resolver.ToHashedString(type_name)},
    _typeNamePlural {hash_resolver.ToHashedString(strex("{}s", type_name))},
    _relation {relation},
    _propMigrationRuleName {hash_resolver.ToHashedString("Property")},
    _componentMigrationRuleName {hash_resolver.ToHashedString("Component")},
    _hashResolver {&hash_resolver},
    _nameResolver {&name_resolver}
{
    FO_STACK_TRACE_ENTRY();

    // Add None entry
    _registeredProperties.emplace_back();
}

PropertyRegistrator::~PropertyRegistrator()
{
    FO_STACK_TRACE_ENTRY();
}

void PropertyRegistrator::RegisterComponent(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    const auto name_hash = _hashResolver->ToHashedString(name);

    FO_RUNTIME_ASSERT(!_registeredComponents.count(name_hash));
    _registeredComponents.insert(name_hash);
}

auto PropertyRegistrator::GetPropertyByIndex(int32 property_index) const noexcept -> const Property*
{
    FO_STACK_TRACE_ENTRY();

    // Skip None entry
    if (property_index >= 1 && static_cast<size_t>(property_index) < _registeredProperties.size()) {
        return _registeredProperties[property_index].get();
    }

    return nullptr;
}

auto PropertyRegistrator::FindProperty(string_view property_name, bool* is_component) const -> const Property*
{
    FO_STACK_TRACE_ENTRY();

    auto key = string(property_name);
    const auto hkey = _hashResolver->ToHashedString(key);

    if (IsComponentRegistered(hkey)) {
        if (is_component != nullptr) {
            *is_component = true;
        }

        return nullptr;
    }

    if (is_component != nullptr) {
        *is_component = false;
    }

    if (const auto rule = _nameResolver->CheckMigrationRule(_propMigrationRuleName, _typeName, hkey); rule.has_value()) {
        key = rule.value();
    }

    if (const auto it = _registeredPropertiesLookup.find(key); it != _registeredPropertiesLookup.end()) {
        return it->second.get();
    }

    return nullptr;
}

auto PropertyRegistrator::IsComponentRegistered(hstring component_name) const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    hstring migrated_component_name = component_name;

    if (const auto rule = _nameResolver->CheckMigrationRule(_componentMigrationRuleName, _typeName, migrated_component_name); rule.has_value()) {
        migrated_component_name = rule.value();
    }

    return _registeredComponents.find(migrated_component_name) != _registeredComponents.end();
}

auto PropertyRegistrator::GetPropertyGroups() const noexcept -> map<string, vector<const Property*>>
{
    FO_STACK_TRACE_ENTRY();

    map<string, vector<const Property*>> result;

    for (const auto& [group_name, properties] : _propertyGroups) {
        vector<const Property*> group_properties;
        group_properties.reserve(properties.size());

        for (const auto& prop : properties | std::views::keys) {
            group_properties.emplace_back(prop.get());
        }

        result.emplace(group_name, std::move(group_properties));
    }

    return result;
}

void PropertyRegistrator::RegisterProperty(const span<const string_view>& flags)
{
    FO_STACK_TRACE_ENTRY();

    // Todo: validate property name identifier

    FO_RUNTIME_ASSERT(flags.size() >= 3);

    auto prop = SafeAlloc::MakeUnique<Property>(this);

    prop->_propName = flags[0];

    if (flags[1] == "Common") {
        prop->_isCommon = true;
    }
    else if (flags[1] == "Server") {
        prop->_isServerOnly = true;
    }
    else if (flags[1] == "Client") {
        prop->_isClientOnly = true;
    }
    else {
        throw PropertyRegistrationException("Invalid property side (expect Server/Client/Common)", prop->_propName, flags[1]);
    }

    const auto h = _hashResolver->ToHashedString(prop->_propName);
    ignore_unused(h);

    const auto type_tok = strex(flags[2]).split('.');
    FO_RUNTIME_ASSERT(!type_tok.empty());

    if (type_tok[0] == "dict") {
        FO_RUNTIME_ASSERT(type_tok.size() >= 3);
        const auto key_type = _nameResolver->ResolveBaseType(type_tok[1]);

        prop->_isDict = true;
        prop->_dictKeyType = key_type;
        prop->_isDictKeyHash = key_type.IsHash;
        prop->_isDictKeyEnum = key_type.IsEnum;
        prop->_isDictKeyString = key_type.IsString;

        if (type_tok[2] == "arr") {
            FO_RUNTIME_ASSERT(type_tok.size() >= 4);
            const auto value_type = _nameResolver->ResolveBaseType(type_tok[3]);

            prop->_baseType = value_type;
            prop->_isDictOfArray = true;
            prop->_isDictOfArrayOfString = value_type.IsString;
            prop->_viewTypeName = "dict<" + key_type.TypeName + ", " + value_type.TypeName + "[]>";
        }
        else {
            const auto value_type = _nameResolver->ResolveBaseType(type_tok[2]);

            prop->_baseType = value_type;
            prop->_isDictOfString = value_type.IsString;
            prop->_viewTypeName = "dict<" + key_type.TypeName + ", " + value_type.TypeName + ">";
        }
    }
    else if (type_tok[0] == "arr") {
        FO_RUNTIME_ASSERT(type_tok.size() >= 2);
        const auto value_type = _nameResolver->ResolveBaseType(type_tok[1]);

        prop->_isArray = true;
        prop->_baseType = value_type;
        prop->_isArrayOfString = value_type.IsString;
        prop->_viewTypeName = value_type.TypeName + "[]";
    }
    else {
        const auto value_type = _nameResolver->ResolveBaseType(type_tok[0]);

        prop->_baseType = value_type;
        prop->_viewTypeName = value_type.TypeName;

        if (value_type.IsString) {
            prop->_isString = true;
        }
        else {
            prop->_isPlainData = true;
        }
    }

    if (const auto dot_pos = prop->_propName.find('.'); dot_pos != string::npos) {
        prop->_component = _hashResolver->ToHashedString(prop->_propName.substr(0, dot_pos));
        prop->_propNameWithoutComponent = prop->_propName.substr(dot_pos + 1);
        FO_RUNTIME_ASSERT(!!prop->_component);
        FO_RUNTIME_ASSERT(!prop->_propNameWithoutComponent.empty());
        FO_RUNTIME_ASSERT(_registeredComponents.count(_hashResolver->ToHashedString(prop->_component)) != 0);
    }
    else {
        prop->_propNameWithoutComponent = prop->_propName;
    }

    for (size_t i = 3; i < flags.size(); i++) {
        const auto check_next_param = [&flags, i, &prop] {
            if (i + 2 >= flags.size() || flags[i + 1] != "=") {
                throw PropertyRegistrationException("Expected property flag = value", prop->_propName, flags[i], i, flags.size());
            }
        };

        if (flags[i] == "Group") {
            check_next_param();

            const auto group = flags[i + 2];
            int32 priority = 0;

            if (i + 4 < flags.size() && flags[i + 3] == "^") {
                priority = strex(flags[i + 4]).to_int32();
                i += 2;
            }

            if (const auto it = _propertyGroups.find(group); it != _propertyGroups.end()) {
                it->second.emplace_back(pair {prop.get(), priority});

                std::ranges::stable_sort(it->second, [](auto&& a, auto&& b) -> bool {
                    return a.second < b.second; // Sort by priority
                });
            }
            else {
                _propertyGroups.emplace(group, vector<pair<raw_ptr<const Property>, int32>> {pair {prop.get(), priority}});
            }

            i += 2;
        }
        else if (flags[i] == "Mutable") {
            FO_RUNTIME_ASSERT(!prop->_isMutable);
            prop->_isMutable = true;
        }
        else if (flags[i] == "CoreProperty") {
            FO_RUNTIME_ASSERT(!prop->_isCoreProperty);
            prop->_isCoreProperty = true;
        }
        else if (flags[i] == "Persistent") {
            FO_RUNTIME_ASSERT(!prop->_isPersistent);
            prop->_isPersistent = true;
        }
        else if (flags[i] == "Virtual") {
            FO_RUNTIME_ASSERT(!prop->_isVirtual);
            prop->_isVirtual = true;
        }
        else if (flags[i] == "OwnerSync") {
            FO_RUNTIME_ASSERT(!prop->_isOwnerSync);
            prop->_isOwnerSync = true;
        }
        else if (flags[i] == "PublicSync") {
            FO_RUNTIME_ASSERT(!prop->_isPublicSync);
            prop->_isPublicSync = true;
        }
        else if (flags[i] == "NoSync") {
            FO_RUNTIME_ASSERT(!prop->_isNoSync);
            prop->_isNoSync = true;
        }
        else if (flags[i] == "ModifiableByClient") {
            FO_RUNTIME_ASSERT(!prop->_isModifiableByClient);
            prop->_isModifiableByClient = true;
        }
        else if (flags[i] == "ModifiableByAnyClient") {
            FO_RUNTIME_ASSERT(!prop->_isModifiableByAnyClient);
            prop->_isModifiableByAnyClient = true;
        }
        else if (flags[i] == "Historical") {
            FO_RUNTIME_ASSERT(!prop->_isHistorical);
            prop->_isHistorical = true;
        }
        else if (flags[i] == "Resource") {
            FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());
            FO_RUNTIME_ASSERT(!prop->_isResourceHash);
            prop->_isResourceHash = true;
        }
        else if (flags[i] == "ScriptFuncType") {
            check_next_param();
            FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());
            prop->_scriptFuncType = flags[i + 2];
            i += 2;
        }
        else if (flags[i] == "Max") {
            // Todo: restore property Max modifier
            check_next_param();

            if (!prop->IsBaseTypeInt() && !prop->IsBaseTypeFloat()) {
                throw PropertyRegistrationException("Expected numeric type for Max flag", prop->_propName);
            }

            i += 2;
        }
        else if (flags[i] == "Min") {
            // Todo: restore property Min modifier
            check_next_param();

            if (!prop->IsBaseTypeInt() && !prop->IsBaseTypeFloat()) {
                throw PropertyRegistrationException("Expected numeric type for Min flag", prop->_propName);
            }

            i += 2;
        }
        else if (flags[i] == "Quest") {
            // Todo: remove property Quest modifier
            check_next_param();
            i += 2;
        }
        else if (flags[i] == "NullGetterForProto") {
            FO_RUNTIME_ASSERT(!prop->_isNullGetterForProto);
            prop->_isNullGetterForProto = true;
        }
        else if (flags[i] == "SharedProperty") {
            // For internal use, skip
        }
        else {
            throw PropertyRegistrationException("Invalid property flag", prop->_propName, flags[i]);
        }
    }

    prop->_isSynced = prop->_isCommon && prop->_isMutable && (prop->_isOwnerSync || prop->_isPublicSync);

    FO_RUNTIME_ASSERT(!(prop->_isOwnerSync || prop->_isPublicSync || prop->_isNoSync) || prop->_isCommon);
    FO_RUNTIME_ASSERT(!(prop->_isOwnerSync || prop->_isPublicSync || prop->_isNoSync) || prop->_isMutable);
    FO_RUNTIME_ASSERT(!(prop->_isOwnerSync || prop->_isPublicSync || prop->_isNoSync) || !prop->_isVirtual);
    FO_RUNTIME_ASSERT(!prop->_isNoSync || !(prop->_isOwnerSync || prop->_isPublicSync));
    FO_RUNTIME_ASSERT(!prop->_isOwnerSync || !(prop->_isNoSync || prop->_isPublicSync));
    FO_RUNTIME_ASSERT(!prop->_isPublicSync || !(prop->_isNoSync || prop->_isOwnerSync));
    FO_RUNTIME_ASSERT(!(prop->_isMutable && prop->_isCommon && !prop->_isVirtual) || (prop->_isOwnerSync || prop->_isPublicSync || prop->_isNoSync));
    FO_RUNTIME_ASSERT(!prop->_isModifiableByClient || prop->_isSynced);
    FO_RUNTIME_ASSERT(!prop->_isModifiableByAnyClient || prop->_isPublicSync);
    FO_RUNTIME_ASSERT(!prop->_isPersistent || (prop->_isMutable || prop->_isCoreProperty));
    FO_RUNTIME_ASSERT(!prop->_isVirtual || !prop->_isSynced);
    FO_RUNTIME_ASSERT(!prop->_isVirtual || !prop->_isPersistent);
    FO_RUNTIME_ASSERT(!prop->_isNullGetterForProto || prop->_isVirtual);

    const auto reg_index = numeric_cast<uint16>(_registeredProperties.size());

    // Disallow set or get accessors
    bool disabled = false;

    if (_relation == PropertiesRelationType::ServerRelative && prop->IsClientOnly()) {
        disabled = true;
    }
    if (_relation == PropertiesRelationType::ClientRelative && prop->IsServerOnly()) {
        disabled = true;
    }

    // PlainData property data offset
    const size_t prev_public_space_size = _publicPodDataSpace.size();
    const size_t prev_protected_space_size = _protectedPodDataSpace.size();

    optional<size_t> pod_data_base_offset;

    if (prop->IsPlainData() && !disabled && !prop->IsVirtual()) {
        const bool is_public = prop->IsPublicSync();
        const bool is_protected = prop->IsOwnerSync();
        auto& space = is_public ? _publicPodDataSpace : (is_protected ? _protectedPodDataSpace : _privatePodDataSpace);

        size_t space_pos = 0;

        while (true) {
            bool fail = false;

            for (size_t i = 0; i < prop->GetBaseSize(); i++) {
                if (space_pos + i >= space.size()) {
                    break;
                }
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

        const size_t fit_size = ((space_pos + prop->GetBaseSize() + 7) / 8) * 8;

        if (fit_size > space.size()) {
            space.resize(fit_size);
        }

        for (size_t i = 0; i < prop->GetBaseSize(); i++) {
            space[space_pos + i] = true;
        }

        pod_data_base_offset = space_pos;

        _wholePodDataSize = _publicPodDataSpace.size() + _protectedPodDataSpace.size() + _privatePodDataSpace.size();
        FO_RUNTIME_ASSERT((_wholePodDataSize % 8) == 0);
    }

    // Complex property data index
    optional<size_t> complex_data_index;

    if (!prop->IsPlainData() && !disabled && !prop->IsVirtual()) {
        complex_data_index = _complexProperties.size();
        _complexProperties.emplace_back(prop.get());

        if (prop->IsPublicSync()) {
            _publicComplexDataProps.emplace_back(reg_index);
            _publicProtectedComplexDataProps.emplace_back(reg_index);
        }
        else if (prop->IsOwnerSync()) {
            _protectedComplexDataProps.emplace_back(reg_index);
            _publicProtectedComplexDataProps.emplace_back(reg_index);
        }
    }

    // Other flags
    prop->_regIndex = reg_index;
    prop->_complexDataIndex = complex_data_index;
    prop->_podDataOffset = pod_data_base_offset;
    prop->_isDisabled = disabled;

    FO_RUNTIME_ASSERT(_registeredPropertiesLookup.count(prop->_propName) == 0);
    _registeredPropertiesLookup.emplace(prop->_propName, prop.get());

    // Fix plain data data offsets
    if (prop->_podDataOffset.has_value()) {
        if (prop->IsOwnerSync()) {
            *prop->_podDataOffset += _publicPodDataSpace.size();
        }
        else if (!prop->IsSynced()) {
            *prop->_podDataOffset += _publicPodDataSpace.size() + _protectedPodDataSpace.size();
        }
    }

    for (auto& other_prop : _registeredProperties) {
        if (!other_prop || !other_prop->_podDataOffset.has_value() || other_prop == prop) {
            continue;
        }

        if (other_prop->IsOwnerSync()) {
            *other_prop->_podDataOffset -= prev_public_space_size;
            *other_prop->_podDataOffset += _publicPodDataSpace.size();
        }
        else if (!other_prop->IsSynced()) {
            *other_prop->_podDataOffset -= prev_public_space_size + prev_protected_space_size;
            *other_prop->_podDataOffset += _publicPodDataSpace.size() + _protectedPodDataSpace.size();
        }
    }

    _registeredProperties.emplace_back(std::move(prop));
}

FO_END_NAMESPACE();
