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

#include "Properties.h"
#include "PropertiesSerializator.h"

FO_BEGIN_NAMESPACE

static constexpr size_t OVERLAY_START_CAPACITY = 16;
static constexpr uint8 FULL_DATA_STORE_TYPE = 0;
static constexpr uint8 SEPARATE_PROPS_STORE_TYPE = 1;

static auto RawDataEqual(const_span<uint8> left, const_span<uint8> right) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (left.size() != right.size()) {
        return false;
    }

    switch (left.size()) {
    case 0:
        return true;
    case 1:
        return left[0] == right[0];
    case 2: {
        return *reinterpret_cast<const uint16*>(left.data()) == *reinterpret_cast<const uint16*>(right.data());
    }
    case 4: {
        return *reinterpret_cast<const uint32*>(left.data()) == *reinterpret_cast<const uint32*>(right.data());
    }
    case 8: {
        return *reinterpret_cast<const uint64*>(left.data()) == *reinterpret_cast<const uint64*>(right.data());
    }
    default:
        return MemCompare(left.data(), right.data(), left.size());
    }
}

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

    return cast_from_void<uint8*>(GetPtr());
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

    _passedPtr = cast_from_void<uint8*>(const_cast<void*>(value));
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

Properties::Properties(const PropertyRegistrator* registrator, const Properties* base) noexcept :
    _registrator {registrator},
    _baseProps {base}
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator);
    FO_STRONG_ASSERT(!_baseProps || _registrator == _baseProps->GetRegistrator());

    if (_registrator->_registeredProperties.size() > 1) {
        AllocData();
    }
}

void Properties::AllocData() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!_podData && !_complexData);
    FO_STRONG_ASSERT(_registrator->_registeredProperties.size() > 1);

    if (!_baseProps) {
        _podData = SafeAlloc::MakeUniqueArr<uint8>(_registrator->_wholePodDataSize);
        MemFill(_podData.get(), 0, _registrator->_wholePodDataSize);
        _complexData = SafeAlloc::MakeUniqueArr<pair<unique_arr_ptr<uint8>, size_t>>(_registrator->_complexProperties.size());
    }
}

auto Properties::FindOverlayEntry(const Property* prop) const noexcept -> const OverlayEntry*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_baseProps || _overlayEntries.empty()) {
        return nullptr;
    }

    const auto reg_index = prop->GetRegIndex();
    const auto it = std::ranges::lower_bound(_overlayEntries, reg_index, std::ranges::less {}, &OverlayEntry::PropRegIndex);
    return it != _overlayEntries.end() && it->PropRegIndex == reg_index ? &*it : nullptr;
}

auto Properties::FindOverlayEntry(const Property* prop) noexcept -> OverlayEntry*
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!_baseProps || _overlayEntries.empty()) {
        return nullptr;
    }

    const auto reg_index = prop->GetRegIndex();
    const auto it = std::ranges::lower_bound(_overlayEntries, reg_index, std::ranges::less {}, &OverlayEntry::PropRegIndex);
    return it != _overlayEntries.end() && it->PropRegIndex == reg_index ? &*it : nullptr;
}

auto Properties::IsOverlayPropertyIncluded(const Property* prop, bool with_protected) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(prop);

    if (!prop->IsSynced()) {
        return false;
    }

    if (prop->IsPlainData()) {
        FO_STRONG_ASSERT(prop->_podDataOffset.has_value());
        const auto limit = _registrator->_publicPodDataSpace.size() + (with_protected ? _registrator->_protectedPodDataSpace.size() : 0);
        return *prop->_podDataOffset + prop->GetBaseSize() <= limit;
    }

    const auto& allowed_props = with_protected ? _registrator->_publicProtectedComplexDataPropsLookup : _registrator->_publicComplexDataPropsLookup;
    return allowed_props.contains(prop->GetRegIndex());
}

auto Properties::AllocOverlayData(size_t data_size) noexcept -> uint32
{
    FO_STACK_TRACE_ENTRY();

    if (data_size == 0) {
        return 0;
    }

    const auto needed_size = _overlayDataSize + data_size;

    if (needed_size > _overlayDataCapacity) {
        size_t new_capacity = _overlayDataCapacity != 0 ? _overlayDataCapacity : OVERLAY_START_CAPACITY;

        while (new_capacity < needed_size) {
            new_capacity *= 2;
        }

        RepackOverlayData(new_capacity);
    }

    const auto offset = static_cast<uint32>(_overlayDataSize);
    _overlayDataSize += data_size;
    return offset;
}

auto Properties::RepackOverlayData(size_t min_capacity) noexcept -> void
{
    FO_STACK_TRACE_ENTRY();

    size_t new_capacity = std::max(min_capacity, OVERLAY_START_CAPACITY);
    size_t used_size = 0;

    for (const auto& entry : _overlayEntries) {
        used_size += entry.DataSize;
    }

    while (new_capacity < used_size) {
        new_capacity *= 2;
    }

    auto new_data = new_capacity != 0 ? SafeAlloc::MakeUniqueArr<uint8>(new_capacity) : nullptr;
    size_t new_size = 0;

    for (auto& entry : _overlayEntries) {
        if (entry.DataSize != 0) {
            if (_overlayData) {
                MemCopy(new_data.get() + new_size, _overlayData.get() + entry.DataOffset, entry.DataSize);
            }

            entry.DataOffset = static_cast<uint32>(new_size);
            new_size += entry.DataSize;
        }
        else {
            entry.DataOffset = 0;
        }
    }

    _overlayData = std::move(new_data);
    _overlayDataCapacity = new_capacity;
    _overlayDataSize = new_size;
    _overlayGarbageSize = 0;

    _storeDataRevision++;
}

void Properties::RemoveOverlayEntry(const Property* prop) noexcept
{
    FO_STACK_TRACE_ENTRY();

    const auto* entry = FindOverlayEntry(prop);

    if (entry != nullptr) {
        _overlayGarbageSize += entry->DataSize;

        const auto index = static_cast<ptrdiff_t>(entry - _overlayEntries.data());
        _overlayEntries.erase(_overlayEntries.begin() + index);

        if (_overlayEntries.empty()) {
            ResetOverlayData();
        }
        else if (_overlayGarbageSize > (_overlayDataCapacity / 2)) {
            RepackOverlayData(_overlayDataCapacity);
        }

        _storeDataRevision++;
    }
}

void Properties::ResetOverlayData() noexcept
{
    FO_STACK_TRACE_ENTRY();

    _overlayEntries.clear();
    _overlayData.reset();
    _overlayDataSize = 0;
    _overlayDataCapacity = 0;
    _overlayGarbageSize = 0;

    _storeDataRevision++;
}

void Properties::ResetComplexData() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_complexData == nullptr) {
        return;
    }

    for (size_t i = 0; i < _registrator->_complexProperties.size(); i++) {
        _complexData[i].first.reset();
        _complexData[i].second = 0;
    }

    _storeDataRevision++;
}

void Properties::RemoveSyncedOverlayEntries() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_overlayEntries.empty()) {
        return;
    }

    size_t write_pos = 0;
    size_t removed_data_size = 0;
    bool removed_any = false;

    for (size_t read_pos = 0; read_pos < _overlayEntries.size(); read_pos++) {
        const auto& entry = _overlayEntries[read_pos];
        const auto& prop = _registrator->_registeredProperties[entry.PropRegIndex];
        FO_STRONG_ASSERT(prop);

        if (prop->IsSynced()) {
            removed_data_size += entry.DataSize;
            removed_any = true;
            continue;
        }

        if (write_pos != read_pos) {
            _overlayEntries[write_pos] = entry;
        }

        write_pos++;
    }

    if (!removed_any) {
        return;
    }

    _overlayEntries.resize(write_pos);
    _overlayGarbageSize += removed_data_size;

    if (_overlayEntries.empty()) {
        ResetOverlayData();
    }
    else if (_overlayGarbageSize > (_overlayDataCapacity / 2)) {
        RepackOverlayData(_overlayDataCapacity);
    }

    _storeDataRevision++;
}

void Properties::CloneOwnDataFrom(const Properties& other) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == other._registrator);
    FO_STRONG_ASSERT((!_baseProps && !other._baseProps) || (_baseProps && _baseProps == other._baseProps));

    if (_baseProps) {
        _overlayEntries = other._overlayEntries;
        _overlayDataSize = other._overlayDataSize;
        _overlayDataCapacity = other._overlayDataSize;
        _overlayGarbageSize = 0;

        if (_overlayDataSize != 0) {
            _overlayData = SafeAlloc::MakeUniqueArr<uint8>(_overlayDataSize);
            MemCopy(_overlayData.get(), other._overlayData.get(), _overlayDataSize);
        }
        else {
            _overlayData.reset();
        }
    }
    else {
        MemCopy(_podData.get(), other._podData.get(), _registrator->_wholePodDataSize);

        for (size_t i = 0; i < _registrator->_complexProperties.size(); i++) {
            if (other._complexData[i].first) {
                _complexData[i].first = SafeAlloc::MakeUniqueArr<uint8>(other._complexData[i].second);
                _complexData[i].second = other._complexData[i].second;
                MemCopy(_complexData[i].first.get(), other._complexData[i].first.get(), other._complexData[i].second);
            }
            else {
                _complexData[i].first.reset();
                _complexData[i].second = 0;
            }
        }
    }

    _storeDataRevision++;
}

void Properties::RebuildOverlayFromFullData(const Properties& other) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == other._registrator);
    FO_STRONG_ASSERT(_baseProps != nullptr);
    FO_STRONG_ASSERT(other._baseProps == nullptr);

    ResetOverlayData();

    size_t total_overlay_data_size = 0;

    for (const auto& data_prop : _registrator->_dataProperties) {
        const auto* prop = data_prop.Prop.get();
        const auto other_raw_data = data_prop.IsPlain ? span<const uint8> {other._podData.get() + data_prop.DataIndex, data_prop.DataSize} : span<const uint8> {other._complexData[data_prop.DataIndex].first.get(), other._complexData[data_prop.DataIndex].second};

        if (!RawDataEqual(_baseProps->GetRawData(prop), other_raw_data)) {
            _overlayEntries.emplace_back(OverlayEntry {.PropRegIndex = prop->GetRegIndex(), .DataOffset = 0, .DataSize = numeric_cast<uint32>(other_raw_data.size())});
            total_overlay_data_size += other_raw_data.size();
        }
    }

    if (!_overlayEntries.empty()) {
        if (total_overlay_data_size != 0) {
            _overlayData = SafeAlloc::MakeUniqueArr<uint8>(total_overlay_data_size);
        }

        _overlayDataSize = total_overlay_data_size;
        _overlayDataCapacity = total_overlay_data_size;
        _overlayGarbageSize = 0;

        size_t data_offset = 0;
        size_t overlay_index = 0;

        for (const auto& data_prop : _registrator->_dataProperties) {
            if (overlay_index >= _overlayEntries.size()) {
                break;
            }

            auto& entry = _overlayEntries[overlay_index];
            const auto* prop = data_prop.Prop.get();

            if (entry.PropRegIndex != prop->GetRegIndex()) {
                continue;
            }

            const auto other_raw_data = data_prop.IsPlain ? span<const uint8> {other._podData.get() + data_prop.DataIndex, data_prop.DataSize} : span<const uint8> {other._complexData[data_prop.DataIndex].first.get(), other._complexData[data_prop.DataIndex].second};
            entry.DataOffset = numeric_cast<uint32>(data_offset);

            if (entry.DataSize != 0) {
                MemCopy(_overlayData.get() + data_offset, other_raw_data.data(), entry.DataSize);
                data_offset += entry.DataSize;
            }

            overlay_index++;
        }

        FO_STRONG_ASSERT(overlay_index == _overlayEntries.size());
    }

    _storeDataRevision++;
}

auto Properties::Copy() const noexcept -> Properties
{
    FO_STACK_TRACE_ENTRY();

    Properties props = _baseProps ? Properties(_registrator.get(), _baseProps.get()) : Properties(_registrator.get());
    props.CloneOwnDataFrom(*this);
    return props;
}

void Properties::CopyFrom(const Properties& other) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == other._registrator);

    if ((!_baseProps && !other._baseProps) || (_baseProps && _baseProps == other._baseProps)) {
        CloneOwnDataFrom(other);
    }
    else if (_baseProps && !other._baseProps) {
        RebuildOverlayFromFullData(other);
    }
    else if (other._baseProps) {
        CopyFrom(*other._baseProps);

        for (const auto& entry : other._overlayEntries) {
            const auto& prop = _registrator->_registeredProperties[entry.PropRegIndex];
            FO_STRONG_ASSERT(prop);

            const auto other_raw_data = span<const uint8>(entry.DataSize != 0 ? other._overlayData.get() + entry.DataOffset : nullptr, entry.DataSize);

            if (!RawDataEqual(GetRawData(prop.get()), other_raw_data)) {
                SetRawData(prop.get(), other_raw_data);
            }
        }
    }
    else {
        FO_STRONG_ASSERT(false);
    }
}

void Properties::StoreAllData(vector<uint8>& all_data, set<hstring>& str_hashes) const
{
    FO_STACK_TRACE_ENTRY();

    all_data.clear();

    auto writer = DataWriter(all_data);
    writer.Write<uint32>(numeric_cast<uint32>(_registrator->_wholePodDataSize));
    writer.Write<bool>(!!_baseProps);

    if (_baseProps) {
        writer.Write<uint32>(numeric_cast<uint32>(_overlayEntries.size()));

        for (const auto& entry : _overlayEntries) {
            writer.Write<uint16>(entry.PropRegIndex);
            writer.Write<uint32>(entry.DataSize);

            if (entry.DataSize != 0) {
                FO_STRONG_ASSERT(_overlayData);
                writer.WritePtr(_overlayData.get() + entry.DataOffset, entry.DataSize);
            }
        }
    }
    else {
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
    }

    // Store hashes
    const auto add_hash = [&str_hashes, this](const string& str) {
        if (!str.empty()) {
            const auto hstr = _registrator->_hashResolver->ToHashedString(str);
            str_hashes.emplace(hstr);
        }
    };

    for (const auto& prop : _registrator->_hashProperties) {
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

            if (prop->IsBaseTypeHash() || prop->IsBaseTypeFixedType()) {
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

void Properties::RestoreAllData(const vector<uint8>& all_data)
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader(all_data);
    const auto whole_pod_data_size = reader.Read<uint32>();
    FO_RUNTIME_ASSERT_STR(whole_pod_data_size == _registrator->_wholePodDataSize, "Run BakeResources");
    const auto has_overlay_data = reader.Read<bool>();
    FO_RUNTIME_ASSERT(!!_baseProps == has_overlay_data);

    if (_baseProps) {
        ResetOverlayData();

        const auto overlay_entries_count = reader.Read<uint32>();

        for (uint32 i = 0; i < overlay_entries_count; i++) {
            const auto prop_index = reader.Read<uint16>();
            FO_RUNTIME_ASSERT(prop_index > 0 && prop_index < _registrator->_registeredProperties.size());
            const auto& prop = _registrator->_registeredProperties[prop_index];
            FO_RUNTIME_ASSERT(prop);
            const auto data_size = reader.Read<uint32>();
            SetRawData(prop.get(), {reader.ReadPtr<uint8>(data_size), data_size});
        }
    }
    else {
        MemFill(_podData.get(), 0, _registrator->_wholePodDataSize);
        ResetComplexData();

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
    }

    reader.VerifyEnd();

    _storeDataRevision++;
}

void Properties::StoreData(bool with_protected, vector<const uint8*>** all_data, vector<uint32>** all_data_sizes) const
{
    FO_STACK_TRACE_ENTRY();

    auto& cache_ptr = _storeDataCaches[with_protected ? 1 : 0];
    make_if_not_exists(cache_ptr);
    auto& cache = *cache_ptr;
    *all_data = &cache.Data;
    *all_data_sizes = &cache.Sizes;

    if (cache.Revision == _storeDataRevision) {
        return;
    }

    cache.Data.clear();
    cache.Sizes.clear();
    cache.PropertyIndices.clear();

    cache.Data.push_back(_baseProps ? &SEPARATE_PROPS_STORE_TYPE : &FULL_DATA_STORE_TYPE);
    cache.Sizes.push_back(sizeof(uint8));

    if (_baseProps) {
        for (const auto& entry : _overlayEntries) {
            const auto& prop = _registrator->_registeredProperties[entry.PropRegIndex];
            FO_STRONG_ASSERT(prop);

            if (!IsOverlayPropertyIncluded(prop.get(), with_protected)) {
                continue;
            }

            const auto raw_data = GetRawData(prop.get());

            cache.PropertyIndices.push_back(entry.PropRegIndex);
            cache.Data.push_back(raw_data.data());
            cache.Sizes.push_back(numeric_cast<uint32>(raw_data.size()));
        }

        if (!cache.PropertyIndices.empty()) {
            cache.Data.insert(cache.Data.begin() + 1, reinterpret_cast<const uint8*>(cache.PropertyIndices.data()));
            cache.Sizes.insert(cache.Sizes.begin() + 1, numeric_cast<uint32>(cache.PropertyIndices.size() * sizeof(uint16)));
        }
    }
    else {
        cache.PropertyIndices = with_protected ? _registrator->_publicProtectedComplexDataProps : _registrator->_publicComplexDataProps;

        const auto preserve_size = 2u + (!cache.PropertyIndices.empty() ? 1u + cache.PropertyIndices.size() : 0);
        cache.Data.reserve(preserve_size);
        cache.Sizes.reserve(preserve_size);

        // Store plain properties data
        cache.Data.push_back(_podData.get());
        cache.Sizes.push_back(numeric_cast<uint32>(_registrator->_publicPodDataSpace.size()) + (with_protected ? numeric_cast<uint32>(_registrator->_protectedPodDataSpace.size()) : 0));

        // Filter complex data to send
        for (size_t i = 0; i < cache.PropertyIndices.size();) {
            const auto& prop = _registrator->_registeredProperties[cache.PropertyIndices[i]];
            FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());

            if (!_complexData[*prop->_complexDataIndex].first) {
                cache.PropertyIndices.erase(cache.PropertyIndices.begin() + numeric_cast<int32>(i));
            }
            else {
                i++;
            }
        }

        // Store complex properties data
        if (!cache.PropertyIndices.empty()) {
            cache.Data.push_back(reinterpret_cast<uint8*>(cache.PropertyIndices.data()));
            cache.Sizes.push_back(numeric_cast<uint32>(cache.PropertyIndices.size()) * sizeof(uint16));

            for (const auto index : cache.PropertyIndices) {
                const auto& prop = _registrator->_registeredProperties[index];
                cache.Data.push_back(_complexData[*prop->_complexDataIndex].first.get());
                cache.Sizes.push_back(numeric_cast<uint32>(_complexData[*prop->_complexDataIndex].second));
            }
        }
    }

    cache.Revision = _storeDataRevision;
}

void Properties::RestoreData(const vector<const uint8*>& all_data, const vector<uint32>& all_data_sizes)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(all_data.size() == all_data_sizes.size());

    const auto apply_separate_props_data = [this](const vector<const uint8*>& separate_data, const vector<uint32>& separate_sizes) {
        if (separate_data.empty()) {
            return;
        }

        const uint32 property_data_count = separate_sizes[0] / sizeof(uint16);
        FO_RUNTIME_ASSERT(separate_sizes[0] == property_data_count * sizeof(uint16));
        FO_RUNTIME_ASSERT(separate_data.size() == 1 + property_data_count);

        for (uint32 i = 0; i < property_data_count; i++) {
            uint16 prop_index {};
            MemCopy(&prop_index, separate_data[0] + i * sizeof(uint16), sizeof(uint16));

            FO_RUNTIME_ASSERT(prop_index > 0);
            FO_RUNTIME_ASSERT(prop_index < _registrator->_registeredProperties.size());
            const auto& prop = _registrator->_registeredProperties[prop_index];
            FO_RUNTIME_ASSERT(prop);
            SetRawData(prop.get(), {separate_data[1 + i], separate_sizes[1 + i]});
        }
    };

    const auto apply_full_data = [this](Properties& target, const vector<const uint8*>& full_data, const vector<uint32>& full_sizes) {
        FO_RUNTIME_ASSERT(!full_sizes.empty());

        const auto public_size = numeric_cast<uint32>(_registrator->_publicPodDataSpace.size());
        const auto protected_size = numeric_cast<uint32>(_registrator->_protectedPodDataSpace.size());
        const auto private_size = numeric_cast<uint32>(_registrator->_privatePodDataSpace.size());
        FO_RUNTIME_ASSERT(full_sizes[0] == public_size || full_sizes[0] == public_size + protected_size || full_sizes[0] == public_size + protected_size + private_size);

        if (full_sizes[0] != 0) {
            MemCopy(target._podData.get(), full_data[0], full_sizes[0]);
        }

        if (full_data.size() > 1) {
            const uint32 comlplex_data_count = full_sizes[1] / sizeof(uint16);
            FO_RUNTIME_ASSERT(comlplex_data_count > 0);
            vector<uint16> complex_indicies(comlplex_data_count);
            MemCopy(complex_indicies.data(), full_data[1], full_sizes[1]);

            for (size_t i = 0; i < complex_indicies.size(); i++) {
                FO_RUNTIME_ASSERT(complex_indicies[i] > 0);
                FO_RUNTIME_ASSERT(complex_indicies[i] < _registrator->_registeredProperties.size());
                const auto& prop = _registrator->_registeredProperties[complex_indicies[i]];
                FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());
                const auto data_size = full_sizes[2 + i];
                const auto* data = full_data[2 + i];
                target.SetRawData(prop.get(), {data, data_size});
            }
        }
    };

    if (all_data.empty()) {
        if (_baseProps) {
            RemoveSyncedOverlayEntries();
        }

        return;
    }

    FO_RUNTIME_ASSERT(all_data_sizes[0] == sizeof(uint8));

    uint8 store_type = 0;
    MemCopy(&store_type, all_data[0], sizeof(store_type));

    vector<const uint8*> payload_data(all_data.begin() + 1, all_data.end());
    vector<uint32> payload_sizes(all_data_sizes.begin() + 1, all_data_sizes.end());

    if (store_type == SEPARATE_PROPS_STORE_TYPE) {
        if (_baseProps) {
            RemoveSyncedOverlayEntries();
        }

        apply_separate_props_data(payload_data, payload_sizes);
    }
    else {
        FO_RUNTIME_ASSERT(store_type == FULL_DATA_STORE_TYPE);

        if (_baseProps) {
            RemoveSyncedOverlayEntries();

            Properties full_props(_registrator.get());
            full_props.CopyFrom(*_baseProps);
            apply_full_data(full_props, payload_data, payload_sizes);
            RebuildOverlayFromFullData(full_props);
        }
        else {
            apply_full_data(*this, payload_data, payload_sizes);
        }
    }

    _storeDataRevision++;
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

    map<string_view, string_view> key_values_view;

    for (const auto& [key, value] : key_values) {
        key_values_view.emplace(key, value);
    }

    ApplyFromText(key_values_view);
}

void Properties::ApplyFromText(const map<string_view, string_view>& key_values)
{
    FO_STACK_TRACE_ENTRY();

    size_t errors = 0;
    const PropertyRegistrator* registrator = GetRegistrator();

    for (const auto& [key, value] : key_values) {
        if (key.empty() || key[0] == '$' || key[0] == '_') {
            continue;
        }

        const Property* prop = registrator->FindProperty(key);

        if (prop == nullptr) {
            WriteLog("Failed to load unknown property {}", key);
            errors++;
            continue;
        }

        if (prop->IsDisabled()) {
            if (registrator->GetSide() == EngineSideKind::ServerSide && prop->IsClientOnly()) {
                continue;
            }
            if (registrator->GetSide() == EngineSideKind::ClientSide && prop->IsServerOnly()) {
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

    for (const auto& prop : _registrator->_textProperties) {
        // Skip same as in base or zero values
        if (base != nullptr) {
            const auto raw_data = GetRawData(prop.get());
            const auto base_raw_data = base->GetRawData(prop.get());

            if (raw_data.size() == base_raw_data.size() && MemCompare(raw_data.data(), base_raw_data.data(), raw_data.size())) {
                continue;
            }
        }
        else {
            const auto raw_data = GetRawData(prop.get());

            if (prop->_podDataOffset.has_value()) {
                const auto* pod_data = raw_data.data();
                const auto* pod_data_end = pod_data + raw_data.size();

                while (pod_data != pod_data_end && *pod_data == 0) {
                    ++pod_data;
                }

                if (pod_data == pod_data_end) {
                    continue;
                }
            }
            else {
                if (raw_data.empty()) {
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

    if (this == &other) {
        return true;
    }

    if (ignore_props.empty() && !ignore_temporary) {
        if (!_baseProps && !other._baseProps) {
            if (!MemCompare(_podData.get(), other._podData.get(), _registrator->_wholePodDataSize)) {
                return false;
            }

            for (const auto& prop : _registrator->_complexProperties) {
                FO_STRONG_ASSERT(prop->_complexDataIndex.has_value());
                const auto index = *prop->_complexDataIndex;
                const auto& complex_data = _complexData[index];
                const auto& other_complex_data = other._complexData[index];

                if (complex_data.second != other_complex_data.second) {
                    return false;
                }
                if (complex_data.second != 0 && !MemCompare(complex_data.first.get(), other_complex_data.first.get(), complex_data.second)) {
                    return false;
                }
            }

            return true;
        }

        if (_baseProps && _baseProps == other._baseProps) {
            if (_overlayEntries.size() != other._overlayEntries.size()) {
                return false;
            }

            for (size_t i = 0; i < _overlayEntries.size(); i++) {
                const auto& entry = _overlayEntries[i];
                const auto& other_entry = other._overlayEntries[i];

                if (entry.PropRegIndex != other_entry.PropRegIndex || entry.DataSize != other_entry.DataSize) {
                    return false;
                }
                if (entry.DataSize != 0 && !MemCompare(_overlayData.get() + entry.DataOffset, other._overlayData.get() + other_entry.DataOffset, entry.DataSize)) {
                    return false;
                }
            }

            return true;
        }
    }

    const auto get_data_prop_raw_data = [](const Properties& props, const PropertyRegistrator::DataPropertyEntry& data_prop, const Property* prop) noexcept -> span<const uint8> {
        if (props._baseProps) {
            return props.GetRawData(prop);
        }

        if (data_prop.IsPlain) {
            return {props._podData.get() + data_prop.DataIndex, data_prop.DataSize};
        }

        const auto& complex_data = props._complexData[data_prop.DataIndex];
        return {complex_data.first.get(), complex_data.second};
    };

    for (const auto& data_prop : _registrator->_dataProperties) {
        const auto* prop = data_prop.Prop.get();

        if (vec_exists(ignore_props, prop)) {
            continue;
        }
        if (ignore_temporary && prop->IsTemporary()) {
            continue;
        }

        const auto raw_data = get_data_prop_raw_data(*this, data_prop, prop);
        const auto other_raw_data = get_data_prop_raw_data(other, data_prop, prop);

        if (raw_data.size() != other_raw_data.size() || !MemCompare(raw_data.data(), other_raw_data.data(), raw_data.size())) {
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

    PropertiesSerializator::LoadPropertyFromText(this, prop, text, *_registrator->_hashResolver, *_registrator->_nameResolver);
}

auto Properties::SavePropertyToText(const Property* prop) const -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop);
    FO_RUNTIME_ASSERT(_registrator == prop->_registrator);
    FO_RUNTIME_ASSERT(prop->_podDataOffset.has_value() || prop->_complexDataIndex.has_value());

    return PropertiesSerializator::SavePropertyToText(this, prop, *_registrator->_hashResolver, *_registrator->_nameResolver);
}

void Properties::ValidateForRawData(const Property* prop) const noexcept(false)
{
    FO_STACK_TRACE_ENTRY();

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

    if (_baseProps) {
        if (const auto* entry = FindOverlayEntry(prop); entry != nullptr) {
            return {entry->DataSize != 0 ? _overlayData.get() + entry->DataOffset : nullptr, entry->DataSize};
        }

        return _baseProps->GetRawData(prop);
    }

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

    if (_baseProps) {
        if (const auto* entry = FindOverlayEntry(prop); entry != nullptr) {
            return entry->DataSize;
        }

        return _baseProps->GetRawDataSize(prop);
    }
    else {
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
}

void Properties::SetRawData(const Property* prop, span<const uint8> raw_data) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == prop->_registrator);

    if (_baseProps) {
        const auto base_raw_data = _baseProps->GetRawData(prop);

        if (RawDataEqual(raw_data, base_raw_data)) {
            RemoveOverlayEntry(prop);
            return;
        }

        if (auto* entry = FindOverlayEntry(prop); entry != nullptr) {
            const auto current_overlay_data = span<const uint8>(entry->DataSize != 0 ? _overlayData.get() + entry->DataOffset : nullptr, entry->DataSize);

            if (RawDataEqual(raw_data, current_overlay_data)) {
                return;
            }

            if (entry->DataSize == raw_data.size()) {
                if (!raw_data.empty()) {
                    MemCopy(_overlayData.get() + entry->DataOffset, raw_data.data(), raw_data.size());
                }
            }
            else {
                _overlayGarbageSize += entry->DataSize;
                entry->DataOffset = AllocOverlayData(raw_data.size());
                entry->DataSize = static_cast<uint32>(raw_data.size());

                if (!raw_data.empty()) {
                    MemCopy(_overlayData.get() + entry->DataOffset, raw_data.data(), raw_data.size());
                }
            }
        }
        else {
            OverlayEntry new_entry;
            new_entry.PropRegIndex = prop->GetRegIndex();
            new_entry.DataOffset = AllocOverlayData(raw_data.size());
            new_entry.DataSize = static_cast<uint32>(raw_data.size());

            if (!raw_data.empty()) {
                MemCopy(_overlayData.get() + new_entry.DataOffset, raw_data.data(), raw_data.size());
            }

            const auto it = std::ranges::lower_bound(_overlayEntries, new_entry.PropRegIndex, std::ranges::less {}, &OverlayEntry::PropRegIndex);
            _overlayEntries.emplace(it, new_entry);
        }

        if (_overlayGarbageSize > (_overlayDataCapacity / 2)) {
            RepackOverlayData(_overlayDataCapacity);
        }

        _storeDataRevision++;
    }
    else {
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

        _storeDataRevision++;
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

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            return numeric_cast<int32>(GetValue<uint8>(prop));
        }
        if (base_type.Size == 2) {
            return numeric_cast<int32>(GetValue<uint16>(prop));
        }
        if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                return numeric_cast<int32>(GetValue<int32>(prop));
            }
            else {
                return numeric_cast<int32>(GetValue<uint32>(prop));
            }
        }
    }
    else if (base_type.IsBool) {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            return iround<int32>(GetValue<float32>(prop));
        }
        if (base_type.IsDoubleFloat) {
            return iround<int32>(GetValue<float64>(prop));
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            return numeric_cast<int32>(GetValue<int8>(prop));
        }
        if (base_type.Size == 2) {
            return numeric_cast<int32>(GetValue<int16>(prop));
        }
        if (base_type.Size == 4) {
            return numeric_cast<int32>(GetValue<int32>(prop));
        }
        if (base_type.Size == 8) {
            return numeric_cast<int32>(GetValue<int64>(prop));
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            return numeric_cast<int32>(GetValue<uint8>(prop));
        }
        if (base_type.Size == 2) {
            return numeric_cast<int32>(GetValue<uint16>(prop));
        }
        if (base_type.Size == 4) {
            return numeric_cast<int32>(GetValue<uint32>(prop));
        }
    }

    throw PropertiesException("Invalid property for get as int", prop->GetName());
}

auto Properties::GetPlainDataValueAsAny(const Property* prop) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsFixedType) {
        return any_t {GetValue<hstring>(prop).as_str()};
    }
    else if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            return any_t {strex("{}", GetValue<uint8>(prop))};
        }
        if (base_type.Size == 2) {
            return any_t {strex("{}", GetValue<uint16>(prop))};
        }
        if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                return any_t {strex("{}", GetValue<int32>(prop))};
            }
            else {
                return any_t {strex("{}", GetValue<uint32>(prop))};
            }
        }
    }
    else if (base_type.IsBool) {
        return any_t {strex("{}", GetValue<bool>(prop))};
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            return any_t {strex("{}", GetValue<float32>(prop))};
        }
        if (base_type.IsDoubleFloat) {
            return any_t {strex("{}", GetValue<float64>(prop))};
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            return any_t {strex("{}", GetValue<int8>(prop))};
        }
        if (base_type.Size == 2) {
            return any_t {strex("{}", GetValue<int16>(prop))};
        }
        if (base_type.Size == 4) {
            return any_t {strex("{}", GetValue<int32>(prop))};
        }
        if (base_type.Size == 8) {
            return any_t {strex("{}", GetValue<int64>(prop))};
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            return any_t {strex("{}", GetValue<uint8>(prop))};
        }
        if (base_type.Size == 2) {
            return any_t {strex("{}", GetValue<uint16>(prop))};
        }
        if (base_type.Size == 4) {
            return any_t {strex("{}", GetValue<uint32>(prop))};
        }
    }

    throw PropertiesException("Invalid property for get as any", prop->GetName());
}

void Properties::SetPlainDataValueAsInt(const Property* prop, int32 value)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop->IsPlainData());

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(value));
        }
        else if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                SetValue<int32>(prop, value);
            }
            else {
                SetValue<uint32>(prop, numeric_cast<uint32>(value));
            }
        }
    }
    else if (base_type.IsBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            SetValue<float32>(prop, numeric_cast<float32>(value));
        }
        else if (base_type.IsDoubleFloat) {
            SetValue<float64>(prop, numeric_cast<float64>(value));
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<int8>(prop, numeric_cast<int8>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<int16>(prop, numeric_cast<int16>(value));
        }
        else if (base_type.Size == 4) {
            SetValue<int32>(prop, value);
        }
        else if (base_type.Size == 8) {
            SetValue<int64>(prop, numeric_cast<int64>(value));
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(value));
        }
        else if (base_type.Size == 4) {
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

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsFixedType) {
        SetValue<hstring>(prop, _registrator->GetHashResolver()->ToHashedString(value));
    }
    else if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(strvex(value).to_int32()));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(strvex(value).to_int32()));
        }
        else if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                SetValue<int32>(prop, strvex(value).to_int32());
            }
            else {
                SetValue<uint32>(prop, strvex(value).to_uint32());
            }
        }
    }
    else if (base_type.IsBool) {
        SetValue<bool>(prop, strvex(value).to_bool());
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            SetValue<float32>(prop, strvex(value).to_float32());
        }
        else if (base_type.IsDoubleFloat) {
            SetValue<float64>(prop, strvex(value).to_float64());
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<int8>(prop, numeric_cast<int8>(strex(value).to_int32()));
        }
        else if (base_type.Size == 2) {
            SetValue<int16>(prop, numeric_cast<int16>(strex(value).to_int32()));
        }
        else if (base_type.Size == 4) {
            SetValue<int32>(prop, numeric_cast<int32>(strex(value).to_int32()));
        }
        else if (base_type.Size == 8) {
            SetValue<int64>(prop, numeric_cast<int64>(strex(value).to_int64()));
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(strex(value).to_int32()));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(strex(value).to_int32()));
        }
        else if (base_type.Size == 4) {
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

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsHashedString) {
        SetValue<hstring>(prop, ResolveHash(value));
    }
    else if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(value));
        }
        else if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                SetValue<int32>(prop, numeric_cast<int32>(value));
            }
            else {
                SetValue<uint32>(prop, numeric_cast<uint32>(value));
            }
        }
    }
    else if (base_type.IsBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            SetValue<float32>(prop, numeric_cast<float32>(value));
        }
        else if (base_type.IsDoubleFloat) {
            SetValue<float64>(prop, numeric_cast<float64>(value));
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<int8>(prop, numeric_cast<int8>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<int16>(prop, numeric_cast<int16>(value));
        }
        else if (base_type.Size == 4) {
            SetValue<int32>(prop, value);
        }
        else if (base_type.Size == 8) {
            SetValue<int64>(prop, numeric_cast<int64>(value));
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<uint8>(prop, numeric_cast<uint8>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16>(prop, numeric_cast<uint16>(value));
        }
        else if (base_type.Size == 4) {
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

PropertyRegistrator::PropertyRegistrator(string_view type_name, EngineSideKind side, HashResolver& hash_resolver, NameResolver& name_resolver) :
    _typeName {hash_resolver.ToHashedString(type_name)},
    _typeNamePlural {hash_resolver.ToHashedString(strex("{}s", type_name))},
    _side {side},
    _propMigrationRuleName {hash_resolver.ToHashedString("Property")},
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

auto PropertyRegistrator::GetPropertyByIndex(int32 property_index) const noexcept -> const Property*
{
    FO_STACK_TRACE_ENTRY();

    // Skip None entry
    if (property_index >= 1 && static_cast<size_t>(property_index) < _registeredProperties.size()) {
        return _registeredProperties[property_index].get();
    }

    return nullptr;
}

auto PropertyRegistrator::FindProperty(string_view property_name) const -> const Property*
{
    FO_STACK_TRACE_ENTRY();

    auto key = string(property_name);
    const auto hkey = _hashResolver->ToHashedString(key);

    if (const auto rule = _nameResolver->CheckMigrationRule(_propMigrationRuleName, _typeName, hkey); rule.has_value()) {
        key = rule.value();
    }

    if (const auto it = _registeredPropertiesLookup.find(key); it != _registeredPropertiesLookup.end()) {
        return it->second.get();
    }

    return nullptr;
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

auto PropertyRegistrator::RegisterProperty(const span<const string_view>& tokens) -> const Property*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(tokens.size() >= 3);

    auto prop = SafeAlloc::MakeUnique<Property>(this);

    prop->_isCommon = tokens[0] == "Common";
    prop->_isServerOnly = tokens[0] == "Server";
    prop->_isClientOnly = tokens[0] == "Client";
    FO_RUNTIME_ASSERT(static_cast<int32>(prop->_isCommon) + static_cast<int32>(prop->_isServerOnly) + static_cast<int32>(prop->_isClientOnly) == 1);

    const auto type = _nameResolver->ResolveComplexType(tokens[1]);
    FO_RUNTIME_ASSERT(!type.IsMutable);
    FO_RUNTIME_ASSERT(!type.BaseType.IsEntity);
    FO_RUNTIME_ASSERT(type.Kind != ComplexTypeKind::Callback);
    FO_RUNTIME_ASSERT(!type.BaseType.IsFixedType || type.Kind == ComplexTypeKind::Simple || type.Kind == ComplexTypeKind::Array);

    if (type.Kind == ComplexTypeKind::Dict || type.Kind == ComplexTypeKind::DictOfArray) {
        FO_RUNTIME_ASSERT(type.KeyType.has_value());
        FO_RUNTIME_ASSERT(!type.KeyType->IsEntity);

        prop->_isDict = true;
        prop->_dictKeyType = type.KeyType.value();
        prop->_isDictKeyHash = type.KeyType->IsHashedString;
        prop->_isDictKeyEnum = type.KeyType->IsEnum;
        prop->_isDictKeyString = type.KeyType->IsString;

        if (type.Kind == ComplexTypeKind::DictOfArray) {
            prop->_baseType = type.BaseType;
            prop->_isDictOfArray = true;
            prop->_isDictOfArrayOfString = type.BaseType.IsString;
            prop->_viewTypeName = type.KeyType->Name + "=>" + type.BaseType.Name + "[]";
        }
        else {
            prop->_baseType = type.BaseType;
            prop->_isDictOfString = type.BaseType.IsString;
            prop->_viewTypeName = type.KeyType->Name + "=>" + type.BaseType.Name;
        }
    }
    else if (type.Kind == ComplexTypeKind::Array) {
        prop->_isArray = true;
        prop->_baseType = type.BaseType;
        prop->_isArrayOfString = type.BaseType.IsString;
        prop->_viewTypeName = type.BaseType.Name + "[]";
    }
    else {
        prop->_baseType = type.BaseType;
        prop->_viewTypeName = type.BaseType.Name;

        if (type.BaseType.IsString) {
            prop->_isString = true;
        }
        else {
            prop->_isPlainData = true;
        }
    }

    prop->_propName = tokens[2];
    const auto h = _hashResolver->ToHashedString(prop->_propName);
    ignore_unused(h);

    if (const auto dot_pos = prop->_propName.find('.'); dot_pos != string::npos) {
        prop->_componentName = prop->_propName.substr(0, dot_pos);
        prop->_propNameWithoutComponent = prop->_propName.substr(dot_pos + 1);
        FO_RUNTIME_ASSERT(!prop->_componentName.empty());
        FO_RUNTIME_ASSERT(!prop->_propNameWithoutComponent.empty());
    }
    else {
        prop->_propNameWithoutComponent = prop->_propName;
    }

    bool is_component_marker = false;

    for (size_t i = 3; i < tokens.size(); i++) {
        if (tokens[i] == "Group") {
            FO_RUNTIME_ASSERT(i + 2 < tokens.size() && tokens[i + 1] == "=");

            const auto group = tokens[i + 2];
            int32 priority = 0;

            if (i + 4 < tokens.size() && tokens[i + 3] == "^") {
                priority = strex(tokens[i + 4]).to_int32();
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
        else if (tokens[i] == "Mutable") {
            FO_RUNTIME_ASSERT(!prop->_isMutable);
            prop->_isMutable = true;
        }
        else if (tokens[i] == "CoreProperty") {
            FO_RUNTIME_ASSERT(!prop->_isCoreProperty);
            prop->_isCoreProperty = true;
        }
        else if (tokens[i] == "Component") {
            FO_RUNTIME_ASSERT(!prop->_isComponentItself);
            prop->_isComponentItself = true;
            is_component_marker = true;
        }
        else if (tokens[i] == "Persistent") {
            FO_RUNTIME_ASSERT(!prop->_isPersistent);
            prop->_isPersistent = true;
        }
        else if (tokens[i] == "Virtual") {
            FO_RUNTIME_ASSERT(!prop->_isVirtual);
            prop->_isVirtual = true;
        }
        else if (tokens[i] == "OwnerSync") {
            FO_RUNTIME_ASSERT(!prop->_isOwnerSync);
            prop->_isOwnerSync = true;
        }
        else if (tokens[i] == "PublicSync") {
            FO_RUNTIME_ASSERT(!prop->_isPublicSync);
            prop->_isPublicSync = true;
        }
        else if (tokens[i] == "NoSync") {
            FO_RUNTIME_ASSERT(!prop->_isNoSync);
            prop->_isNoSync = true;
        }
        else if (tokens[i] == "ModifiableByClient") {
            FO_RUNTIME_ASSERT(!prop->_isModifiableByClient);
            prop->_isModifiableByClient = true;
        }
        else if (tokens[i] == "ModifiableByAnyClient") {
            FO_RUNTIME_ASSERT(!prop->_isModifiableByAnyClient);
            prop->_isModifiableByAnyClient = true;
        }
        else if (tokens[i] == "Historical") {
            FO_RUNTIME_ASSERT(!prop->_isHistorical);
            prop->_isHistorical = true;
        }
        else if (tokens[i] == "Resource") {
            FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());
            FO_RUNTIME_ASSERT(!prop->_isResourceHash);
            prop->_isResourceHash = true;
        }
        else if (tokens[i] == "ScriptFuncType") {
            FO_RUNTIME_ASSERT(i + 2 < tokens.size() && tokens[i + 1] == "=");
            FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());
            prop->_scriptFuncType = tokens[i + 2];
            i += 2;
        }
        else if (tokens[i] == "Max") {
            // Todo: restore property Max modifier
            FO_RUNTIME_ASSERT(i + 2 < tokens.size() && tokens[i + 1] == "=");
            FO_RUNTIME_ASSERT(prop->IsBaseTypeInt() || prop->IsBaseTypeFloat());
            i += 2;
        }
        else if (tokens[i] == "Min") {
            // Todo: restore property Min modifier
            FO_RUNTIME_ASSERT(i + 2 < tokens.size() && tokens[i + 1] == "=");
            FO_RUNTIME_ASSERT(prop->IsBaseTypeInt() || prop->IsBaseTypeFloat());
            i += 2;
        }
        else if (tokens[i] == "Quest") {
            // Todo: remove property Quest modifier
            FO_RUNTIME_ASSERT(i + 2 < tokens.size() && tokens[i + 1] == "=");
            i += 2;
        }
        else if (tokens[i] == "NullGetterForProto") {
            FO_RUNTIME_ASSERT(!prop->_isNullGetterForProto);
            prop->_isNullGetterForProto = true;
        }
        else if (tokens[i] == "MaybeNull") {
            FO_RUNTIME_ASSERT(!prop->_isMaybeNull);
            prop->_isMaybeNull = true;
        }
        else if (tokens[i] == "SharedProperty") {
            // For internal use, skip
        }
    }

    prop->_isSynced = prop->_isCommon && prop->_isMutable && (prop->_isOwnerSync || prop->_isPublicSync);

    if (is_component_marker) {
        FO_RUNTIME_ASSERT(prop->_componentName.empty());
        FO_RUNTIME_ASSERT(prop->IsBaseTypeBool());
        FO_RUNTIME_ASSERT(prop->IsPlainData());
        _registeredComponents.emplace(prop->_propName, prop.get());
    }

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
    FO_RUNTIME_ASSERT(!prop->_isPersistent || !prop->_isClientOnly);
    FO_RUNTIME_ASSERT(!prop->_isMaybeNull || prop->IsBaseTypeFixedType());

    const auto reg_index = numeric_cast<uint16>(_registeredProperties.size());

    // Disallow set or get accessors
    bool disabled = false;

    if (_side == EngineSideKind::ServerSide && prop->IsClientOnly()) {
        disabled = true;
    }
    if (_side == EngineSideKind::ClientSide && prop->IsServerOnly()) {
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
            _publicComplexDataPropsLookup.emplace(reg_index);
            _publicProtectedComplexDataProps.emplace_back(reg_index);
            _publicProtectedComplexDataPropsLookup.emplace(reg_index);
        }
        else if (prop->IsOwnerSync()) {
            _protectedComplexDataProps.emplace_back(reg_index);
            _publicProtectedComplexDataProps.emplace_back(reg_index);
            _publicProtectedComplexDataPropsLookup.emplace(reg_index);
        }
    }

    // Other tokens
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

    for (auto& data_prop : _dataProperties) {
        if (data_prop.IsPlain) {
            FO_STRONG_ASSERT(data_prop.Prop->_podDataOffset.has_value());
            data_prop.DataIndex = numeric_cast<uint32>(*data_prop.Prop->_podDataOffset);
        }
    }

    if (!prop->IsDisabled()) {
        if (!prop->IsVirtual()) {
            if (prop->IsPlainData()) {
                FO_STRONG_ASSERT(prop->_podDataOffset.has_value());
                _dataProperties.emplace_back(DataPropertyEntry {.Prop = prop.get(), .DataIndex = numeric_cast<uint32>(*prop->_podDataOffset), .DataSize = numeric_cast<uint16>(prop->GetBaseSize()), .IsPlain = true});
            }
            else {
                FO_STRONG_ASSERT(prop->_complexDataIndex.has_value());
                _dataProperties.emplace_back(DataPropertyEntry {.Prop = prop.get(), .DataIndex = numeric_cast<uint32>(*prop->_complexDataIndex), .DataSize = 0, .IsPlain = false});
            }

            if (!prop->IsTemporary()) {
                _textProperties.emplace_back(prop.get());
            }
        }

        if (prop->IsBaseTypeHash() || prop->IsBaseTypeFixedType() || prop->IsDictKeyHash()) {
            _hashProperties.emplace_back(prop.get());
        }
    }

    _registeredProperties.emplace_back(std::move(prop));
    return _registeredProperties.back().get();
}

FO_END_NAMESPACE
