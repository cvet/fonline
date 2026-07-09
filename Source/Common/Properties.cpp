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
static constexpr size_t OVERLAY_INDEX_MIN_ENTRY_COUNT = 16;
static constexpr uint8_t FULL_DATA_STORE_TYPE = 0;
static constexpr uint8_t SEPARATE_PROPS_STORE_TYPE = 1;

static auto RawDataEqual(const_span<uint8_t> left, const_span<uint8_t> right) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (left.size() != right.size()) {
        return false;
    }

    // Unaligned reads: compared spans may point into serialized payloads, not only into aligned property storage
    switch (left.size()) {
    case 0:
        return true;
    case 1:
        return left[0] == right[0];
    case 2: {
        return MemReadUnaligned<uint16_t>(left.data()) == MemReadUnaligned<uint16_t>(right.data());
    }
    case 4: {
        return MemReadUnaligned<uint32_t>(left.data()) == MemReadUnaligned<uint32_t>(right.data());
    }
    case 8: {
        return MemReadUnaligned<uint64_t>(left.data()) == MemReadUnaligned<uint64_t>(right.data());
    }
    default:
        return MemCompare(left.data(), right.data(), left.size());
    }
}

auto PropertyRawData::GetPtr() noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_passedPtr) {
        return _passedPtr;
    }

    return _useDynamic ? make_ptr(_dynamicBuf.get()) : make_ptr(_localBuf);
}

auto PropertyRawData::Alloc(size_t size) -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    _dataSize = size;
    _passedPtr = nullptr;

    if (size > LOCAL_BUF_SIZE) {
        _useDynamic = true;
        _dynamicBuf = SafeAlloc::MakeUniqueArr<uint8_t>(size);
    }
    else {
        _useDynamic = false;
    }

    return GetPtr().reinterpret_as<uint8_t>();
}

void PropertyRawData::Pass(span<const uint8_t> value)
{
    FO_NO_STACK_TRACE_ENTRY();

    Pass(value.data(), value.size());
}

void PropertyRawData::Pass(nptr<const void> value, size_t size)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(value || size == 0, "Source value pointer is null for non-zero size");

    _passedPtr = value.reinterpret_as<uint8_t>().void_cast();
    _dataSize = size;
    _useDynamic = false;
}

void PropertyRawData::StoreIfPassed()
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_passedPtr) {
        PropertyRawData tmp_data;
        tmp_data.Set(_passedPtr.get(), _dataSize);
        *this = std::move(tmp_data);
    }
}

Property::Property(ptr<const PropertyRegistrator> registrator) :
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

Properties::Properties(ptr<const PropertyRegistrator> registrator, nptr<const Properties> base) noexcept :
    _registrator {registrator},
    _baseProps {base}
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!_baseProps || _registrator == _baseProps->GetRegistrator(), "Base properties registrator mismatch", _registrator->GetTypeName(), _baseProps ? _baseProps->GetRegistrator()->GetTypeName() : hstring {});

    if (_registrator->_registeredProperties.size() > 1) {
        AllocData();
    }
}

void Properties::AllocData() noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!_podData && !_complexData, "Property data is already allocated", _registrator->GetTypeName());
    FO_STRONG_ASSERT(_registrator->_registeredProperties.size() > 1, "Properties registrator has no data properties", _registrator->GetTypeName());

    if (!_baseProps) {
        _podData = SafeAlloc::MakeUniqueArr<uint8_t>(_registrator->_wholePodDataSize);
        MemFill(_podData, 0, _registrator->_wholePodDataSize);
        _complexData = SafeAlloc::MakeUniqueArr<pair<unique_arr_ptr<uint8_t>, size_t>>(_registrator->_complexProperties.size());
    }
}

auto Properties::ShouldUseOverlayEntryIndex(size_t entry_count) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return entry_count >= OVERLAY_INDEX_MIN_ENTRY_COUNT;
}

void Properties::ReleaseOverlayEntryIndex() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    vector<int32_t> empty_entry_index;
    _overlayEntryIndex.swap(empty_entry_index);
}

void Properties::EnsureOverlayEntryIndex() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_overlayEntryIndex.empty()) {
        _overlayEntryIndex.assign(_registrator->_registeredProperties.size(), -1);
    }
}

void Properties::RebuildOverlayEntryIndex() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!ShouldUseOverlayEntryIndex(_overlayEntries.size())) {
        ReleaseOverlayEntryIndex();
        return;
    }

    EnsureOverlayEntryIndex();
    std::fill(_overlayEntryIndex.begin(), _overlayEntryIndex.end(), int32_t {-1});

    for (size_t i = 0; i < _overlayEntries.size(); i++) {
        _overlayEntryIndex[_overlayEntries[i].PropRegIndex] = numeric_cast<int32_t>(i);
    }
}

auto Properties::FindOverlayEntry(ptr<const Property> prop) const noexcept -> nptr<const OverlayEntry>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_overlayEntryIndex.empty()) {
        const uint16_t reg_index = prop->GetRegIndex();

        for (const OverlayEntry& entry : _overlayEntries) {
            if (entry.PropRegIndex == reg_index) {
                return &entry;
            }
        }

        return nullptr;
    }

    const uint16_t reg_index = prop->GetRegIndex();
    const int32_t entries_index = _overlayEntryIndex[reg_index];
    return entries_index >= 0 ? &_overlayEntries[numeric_cast<size_t>(entries_index)] : nullptr;
}

auto Properties::FindOverlayEntry(ptr<const Property> prop) noexcept -> nptr<OverlayEntry>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_overlayEntryIndex.empty()) {
        const uint16_t reg_index = prop->GetRegIndex();

        for (OverlayEntry& entry : _overlayEntries) {
            if (entry.PropRegIndex == reg_index) {
                return &entry;
            }
        }

        return nullptr;
    }

    const uint16_t reg_index = prop->GetRegIndex();
    const int32_t entries_index = _overlayEntryIndex[reg_index];
    return entries_index >= 0 ? &_overlayEntries[numeric_cast<size_t>(entries_index)] : nullptr;
}

auto Properties::IsOverlayPropertyIncluded(ptr<const Property> prop, bool with_protected) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!prop->IsSynced()) {
        return false;
    }

    if (prop->IsPlainData()) {
        FO_STRONG_ASSERT(prop->_podDataOffset.has_value(), "Plain synced property has no pod data offset", prop->GetName());
        const auto limit = _registrator->_publicPodDataSpace.size() + (with_protected ? _registrator->_protectedPodDataSpace.size() : 0);
        return *prop->_podDataOffset + prop->GetBaseSize() <= limit;
    }

    const auto& allowed_props = with_protected ? _registrator->_publicProtectedComplexDataPropsLookup : _registrator->_publicComplexDataPropsLookup;
    return allowed_props.contains(prop->GetRegIndex());
}

auto Properties::AllocOverlayData(size_t data_size, size_t data_alignment) noexcept -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    if (data_size == 0) {
        return 0;
    }

    FO_STRONG_ASSERT(data_alignment != 0 && (data_alignment & (data_alignment - 1)) == 0, "Overlay data alignment is not a power of two", _registrator->GetTypeName(), data_alignment);

    const auto align_offset = [data_alignment](size_t offset) noexcept -> size_t { return align_up(offset, data_alignment); };

    // Best-fit search over freed holes and alignment paddings between existing entries;
    // garbage size counts exactly the bytes not owned by any entry within the used range,
    // so a smaller garbage amount can never contain a fitting hole
    if (_overlayGarbageSize >= data_size) {
        vector<pair<size_t, size_t>> used_ranges;
        used_ranges.reserve(_overlayEntries.size());

        for (const auto& entry : _overlayEntries) {
            if (entry.DataSize != 0) {
                used_ranges.emplace_back(entry.DataOffset, entry.DataSize);
            }
        }

        std::ranges::sort(used_ranges);

        optional<size_t> best_offset;
        size_t best_leftover = std::numeric_limits<size_t>::max();
        size_t gap_begin = 0;

        const auto consider_gap = [&](size_t gap_end) noexcept {
            const size_t aligned_offset = align_offset(gap_begin);

            if (aligned_offset + data_size <= gap_end) {
                const size_t leftover = gap_end - gap_begin - data_size;

                if (leftover < best_leftover) {
                    best_offset = aligned_offset;
                    best_leftover = leftover;
                }
            }
        };

        for (const auto& [used_offset, used_size] : used_ranges) {
            if (used_offset > gap_begin) {
                consider_gap(used_offset);
            }

            gap_begin = std::max(gap_begin, used_offset + used_size);
        }

        if (_overlayDataSize > gap_begin) {
            consider_gap(_overlayDataSize);
        }

        if (best_offset.has_value()) {
            _overlayGarbageSize -= data_size;
            return static_cast<uint32_t>(*best_offset);
        }
    }

    // No suitable hole, extend the overlay tail
    const size_t needed_size = align_offset(_overlayDataSize) + data_size;

    if (needed_size > _overlayDataCapacity) {
        size_t new_capacity = _overlayDataCapacity != 0 ? _overlayDataCapacity : OVERLAY_START_CAPACITY;

        while (new_capacity < needed_size) {
            new_capacity *= 2;
        }

        RepackOverlayData(new_capacity);
    }

    const size_t aligned_offset = align_offset(_overlayDataSize);
    FO_STRONG_ASSERT(aligned_offset + data_size <= _overlayDataCapacity, "Overlay data allocation exceeds capacity", _registrator->GetTypeName(), aligned_offset, data_size, _overlayDataCapacity);

    _overlayGarbageSize += aligned_offset - _overlayDataSize;
    _overlayDataSize = aligned_offset + data_size;
    return static_cast<uint32_t>(aligned_offset);
}

auto Properties::MakeOverlayPackOrder() const noexcept -> vector<size_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    // Stable alignment-descending order minimizes padding between packed entries: plain entry
    // sizes are multiples of their alignment and pack back-to-back with zero padding, only
    // variable-size complex payloads may leave small aligned gaps for the entries that follow
    vector<size_t> pack_order(_overlayEntries.size());
    std::iota(pack_order.begin(), pack_order.end(), size_t {0});

    std::ranges::stable_sort(pack_order, [this](size_t left, size_t right) noexcept -> bool {
        const size_t left_alignment = _registrator->GetPropertyByIndexUnsafe(_overlayEntries[left].PropRegIndex)->GetDataAlignment();
        const size_t right_alignment = _registrator->GetPropertyByIndexUnsafe(_overlayEntries[right].PropRegIndex)->GetDataAlignment();
        return left_alignment > right_alignment;
    });

    return pack_order;
}

auto Properties::RepackOverlayData(size_t min_capacity) noexcept -> void
{
    FO_STACK_TRACE_ENTRY();

    const vector<size_t> pack_order = MakeOverlayPackOrder();

    size_t used_size = 0;
    size_t packed_size = 0;

    for (const size_t entry_index : pack_order) {
        const auto& entry = _overlayEntries[entry_index];

        if (entry.DataSize != 0) {
            packed_size = align_up(packed_size, _registrator->GetPropertyByIndexUnsafe(entry.PropRegIndex)->GetDataAlignment());
            packed_size += entry.DataSize;
            used_size += entry.DataSize;
        }
    }

    size_t new_capacity = std::max(min_capacity, OVERLAY_START_CAPACITY);

    while (new_capacity < packed_size) {
        new_capacity *= 2;
    }

    unique_arr_ptr<uint8_t> new_data = new_capacity != 0 ? SafeAlloc::MakeUniqueArr<uint8_t>(new_capacity) : nullptr;
    size_t new_size = 0;

    for (const size_t entry_index : pack_order) {
        auto& entry = _overlayEntries[entry_index];

        if (entry.DataSize != 0) {
            auto prop = _registrator->GetPropertyByIndexUnsafe(entry.PropRegIndex);
            new_size = align_up(new_size, prop->GetDataAlignment());

            if (_overlayData) {
                nptr<uint8_t> new_data_bytes = new_data.get();
                nptr<const uint8_t> overlay_data_bytes = _overlayData.get();
                FO_STRONG_ASSERT(new_data_bytes, "New overlay data buffer is null");
                FO_STRONG_ASSERT(overlay_data_bytes, "Existing overlay data buffer is null");

                auto target = new_data_bytes.offset(new_size);
                auto source = overlay_data_bytes.offset(entry.DataOffset);
                MemCopy(target, source, entry.DataSize);
            }

            entry.DataOffset = numeric_cast<uint32_t>(new_size);
            new_size += entry.DataSize;
        }
        else {
            entry.DataOffset = 0;
        }
    }

    FO_STRONG_ASSERT(new_size == packed_size, "Repacked overlay size mismatch", _registrator->GetTypeName(), packed_size, new_size);

    _overlayData = std::move(new_data);
    _overlayDataCapacity = new_capacity;
    _overlayDataSize = new_size;
    _overlayGarbageSize = new_size - used_size;

    _storeDataRevision++;
}

void Properties::RemoveOverlayEntry(ptr<const Property> prop) noexcept
{
    FO_STACK_TRACE_ENTRY();

    auto entry = FindOverlayEntry(prop);

    if (entry) {
        _overlayGarbageSize += entry->DataSize;

        const auto index = numeric_cast<size_t>(entry.get() - _overlayEntries.data());
        const auto removed_reg_index = _overlayEntries[index].PropRegIndex;
        _overlayEntries.erase(_overlayEntries.begin() + numeric_cast<ptrdiff_t>(index));

        if (!_overlayEntryIndex.empty()) {
            if (!ShouldUseOverlayEntryIndex(_overlayEntries.size())) {
                ReleaseOverlayEntryIndex();
            }
            else {
                _overlayEntryIndex[removed_reg_index] = -1;

                for (size_t i = index; i < _overlayEntries.size(); i++) {
                    _overlayEntryIndex[_overlayEntries[i].PropRegIndex] = numeric_cast<int32_t>(i);
                }
            }
        }

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

    if (!_overlayEntryIndex.empty()) {
        ReleaseOverlayEntryIndex();
    }

    _overlayData.reset();
    _overlayDataSize = 0;
    _overlayDataCapacity = 0;
    _overlayGarbageSize = 0;

    _storeDataRevision++;
}

void Properties::ResetComplexData() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!_complexData) {
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
        auto prop = _registrator->GetPropertyByIndexUnsafe(entry.PropRegIndex);

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

    RebuildOverlayEntryIndex();

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

    FO_STRONG_ASSERT(_registrator == other._registrator, "Properties registrator mismatch in clone", _registrator->GetTypeName(), other._registrator->GetTypeName());
    FO_STRONG_ASSERT((!_baseProps && !other._baseProps) || (_baseProps && _baseProps == other._baseProps), "Base properties mismatch in clone", _registrator->GetTypeName());

    if (_baseProps) {
        _overlayEntries = other._overlayEntries;
        _overlayDataSize = other._overlayDataSize;
        _overlayDataCapacity = other._overlayDataSize;
        _overlayGarbageSize = other._overlayGarbageSize;

        RebuildOverlayEntryIndex();

        if (_overlayDataSize != 0) {
            _overlayData = SafeAlloc::MakeUniqueArr<uint8_t>(_overlayDataSize);

            nptr<uint8_t> overlay_data = _overlayData.get();
            nptr<const uint8_t> other_overlay_data = other._overlayData.get();
            FO_STRONG_ASSERT(overlay_data, "Target overlay data buffer is null");
            FO_STRONG_ASSERT(other_overlay_data, "Source overlay data buffer is null");

            MemCopy(overlay_data, other_overlay_data, _overlayDataSize);
        }
        else {
            _overlayData.reset();
        }
    }
    else {
        if (_registrator->_wholePodDataSize != 0) {
            nptr<uint8_t> pod_data = _podData.get();
            nptr<const uint8_t> other_pod_data = other._podData.get();
            FO_STRONG_ASSERT(pod_data, "Target POD data buffer is null");
            FO_STRONG_ASSERT(other_pod_data, "Source POD data buffer is null");

            MemCopy(pod_data, other_pod_data, _registrator->_wholePodDataSize);
        }

        for (size_t i = 0; i < _registrator->_complexProperties.size(); i++) {
            if (other._complexData[i].first) {
                const size_t complex_data_size = other._complexData[i].second;
                _complexData[i].first = SafeAlloc::MakeUniqueArr<uint8_t>(complex_data_size);
                _complexData[i].second = complex_data_size;

                if (complex_data_size != 0) {
                    nptr<uint8_t> complex_data = _complexData[i].first.get();
                    nptr<const uint8_t> other_complex_data = other._complexData[i].first.get();
                    FO_STRONG_ASSERT(complex_data, "Target complex data buffer is null");
                    FO_STRONG_ASSERT(other_complex_data, "Source complex data buffer is null");

                    MemCopy(complex_data, other_complex_data, complex_data_size);
                }
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

    FO_STRONG_ASSERT(_registrator == other._registrator, "Properties registrator mismatch in overlay rebuild", _registrator->GetTypeName(), other._registrator->GetTypeName());
    FO_STRONG_ASSERT(_baseProps != nullptr, "Overlay rebuild target has no base properties", _registrator->GetTypeName());
    FO_STRONG_ASSERT(other._baseProps == nullptr, "Overlay rebuild source already has base properties", _registrator->GetTypeName());

    ResetOverlayData();

    const auto get_full_raw_data = [](const Properties& props, const PropertyRegistrator::DataPropertyEntry& data_prop) noexcept -> span<const uint8_t> {
        if (data_prop.IsPlain) {
            if (data_prop.DataSize == 0) {
                return {};
            }

            nptr<const uint8_t> pod_data = props._podData.get();
            FO_STRONG_ASSERT(pod_data, "POD data buffer is null");

            auto data = pod_data.offset(data_prop.DataIndex);
            return {data.get(), data_prop.DataSize};
        }

        const auto& complex_data = props._complexData[data_prop.DataIndex];
        if (complex_data.second == 0) {
            return {};
        }

        nptr<const uint8_t> complex_data_bytes = complex_data.first.get();
        FO_STRONG_ASSERT(complex_data_bytes, "Complex data buffer is null");

        return {complex_data_bytes.get(), complex_data.second};
    };

    size_t total_overlay_data_size = 0;
    vector<span<const uint8_t>> entries_raw_data;

    for (const auto& data_prop : _registrator->_dataProperties) {
        auto prop = data_prop.Prop;
        FO_STRONG_ASSERT(prop, "Data property entry is null");
        const auto other_raw_data = get_full_raw_data(other, data_prop);

        if (!RawDataEqual(_baseProps->GetRawData(prop), other_raw_data)) {
            _overlayEntries.emplace_back(OverlayEntry {.PropRegIndex = prop->GetRegIndex(), .DataOffset = 0, .DataSize = numeric_cast<uint32_t>(other_raw_data.size())});
            entries_raw_data.emplace_back(other_raw_data);
            total_overlay_data_size += other_raw_data.size();
        }
    }

    if (!_overlayEntries.empty()) {
        const vector<size_t> pack_order = MakeOverlayPackOrder();

        size_t packed_size = 0;

        for (const size_t entry_index : pack_order) {
            const auto& entry = _overlayEntries[entry_index];

            if (entry.DataSize != 0) {
                packed_size = align_up(packed_size, _registrator->GetPropertyByIndexUnsafe(entry.PropRegIndex)->GetDataAlignment());
                packed_size += entry.DataSize;
            }
        }

        if (packed_size != 0) {
            _overlayData = SafeAlloc::MakeUniqueArr<uint8_t>(packed_size);
        }

        _overlayDataSize = packed_size;
        _overlayDataCapacity = packed_size;
        _overlayGarbageSize = packed_size - total_overlay_data_size;

        size_t data_offset = 0;

        for (const size_t entry_index : pack_order) {
            auto& entry = _overlayEntries[entry_index];

            if (entry.DataSize != 0) {
                auto prop = _registrator->GetPropertyByIndexUnsafe(entry.PropRegIndex);
                data_offset = align_up(data_offset, prop->GetDataAlignment());

                nptr<uint8_t> overlay_data = _overlayData.get();
                FO_STRONG_ASSERT(overlay_data, "Overlay data buffer is null");

                auto target = overlay_data.offset(data_offset);
                MemCopy(target, entries_raw_data[entry_index].data(), entry.DataSize);
                entry.DataOffset = numeric_cast<uint32_t>(data_offset);
                data_offset += entry.DataSize;
            }
        }

        FO_STRONG_ASSERT(data_offset == packed_size, "Overlay rebuild data size mismatch", _registrator->GetTypeName(), packed_size, data_offset);
    }

    RebuildOverlayEntryIndex();

    _storeDataRevision++;
}

auto Properties::Copy() const noexcept -> Properties
{
    FO_STACK_TRACE_ENTRY();

    Properties props {_registrator, _baseProps};
    props.CloneOwnDataFrom(*this);
    return props;
}

void Properties::CopyFrom(const Properties& other) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == other._registrator, "Properties registrator mismatch in copy", _registrator->GetTypeName(), other._registrator->GetTypeName());

    if ((!_baseProps && !other._baseProps) || (_baseProps && _baseProps == other._baseProps)) {
        CloneOwnDataFrom(other);
    }
    else if (_baseProps && !other._baseProps) {
        RebuildOverlayFromFullData(other);
    }
    else if (other._baseProps) {
        CopyFrom(*other._baseProps);

        for (const auto& entry : other._overlayEntries) {
            auto prop = _registrator->GetPropertyByIndexUnsafe(entry.PropRegIndex);

            span<const uint8_t> other_raw_data;
            if (entry.DataSize != 0) {
                nptr<const uint8_t> other_overlay_data = other._overlayData.get();
                FO_STRONG_ASSERT(other_overlay_data, "Source overlay data buffer is null");

                auto other_overlay_entry_data = other_overlay_data.offset(entry.DataOffset);
                other_raw_data = {other_overlay_entry_data.get(), entry.DataSize};
            }

            if (!RawDataEqual(GetRawData(prop), other_raw_data)) {
                SetRawData(prop, other_raw_data);
            }
        }
    }
    else {
        FO_STRONG_ASSERT(false, "Unsupported properties copy path", _registrator->GetTypeName(), _baseProps != nullptr, other._baseProps != nullptr);
    }
}

void Properties::StoreAllData(vector<uint8_t>& all_data, set<hstring>& str_hashes) const
{
    FO_STACK_TRACE_ENTRY();

    all_data.clear();

    auto writer = DataWriter(all_data);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(_registrator->_wholePodDataSize));
    writer.Write<bool>(!!_baseProps);

    if (_baseProps) {
        writer.Write<uint32_t>(numeric_cast<uint32_t>(_overlayEntries.size()));

        for (const auto& entry : _overlayEntries) {
            writer.Write<uint16_t>(entry.PropRegIndex);
            writer.Write<uint32_t>(entry.DataSize);

            if (entry.DataSize != 0) {
                FO_STRONG_ASSERT(_overlayData, "Overlay data is missing while storing a property", entry.PropRegIndex, _registrator->GetTypeName(), entry.DataSize);
                writer.WriteBytes({_overlayData.get() + entry.DataOffset, entry.DataSize});
            }
        }
    }
    else {
        const auto get_pod_data = [this]() noexcept -> const_span<uint8_t> {
            nptr<const uint8_t> pod_data = _podData.get();
            FO_STRONG_ASSERT(pod_data, "POD data buffer is null");
            return {pod_data.get(), _registrator->_wholePodDataSize};
        };

        int32_t start_pos = -1;
        constexpr int32_t seek_step = 3;

        for (size_t i = 0; i < _registrator->_wholePodDataSize; i++) {
            if (_podData[i] != 0) {
                if (start_pos == -1) {
                    start_pos = numeric_cast<int32_t>(i);
                }

                i += seek_step;
            }
            else {
                if (start_pos != -1) {
                    const size_t len = i - start_pos;
                    writer.Write<uint32_t>(numeric_cast<uint32_t>(start_pos));
                    writer.Write<uint32_t>(numeric_cast<uint32_t>(len));
                    const_span<uint8_t> pod_data = get_pod_data();
                    writer.WriteBytes(pod_data.subspan(numeric_cast<size_t>(start_pos), len));

                    start_pos = -1;
                }
            }
        }

        if (start_pos != -1) {
            const size_t len = _registrator->_wholePodDataSize - start_pos;
            writer.Write<uint32_t>(numeric_cast<uint32_t>(start_pos));
            writer.Write<uint32_t>(numeric_cast<uint32_t>(len));
            const_span<uint8_t> pod_data = get_pod_data();
            writer.WriteBytes(pod_data.subspan(numeric_cast<size_t>(start_pos), len));
        }

        writer.Write<uint32_t>(const_numeric_cast<uint32_t>(0));
        writer.Write<uint32_t>(const_numeric_cast<uint32_t>(0));

        // Store complex properties
        writer.Write<uint32_t>(numeric_cast<uint32_t>(_registrator->_complexProperties.size()));

        for (const auto& prop : _registrator->_complexProperties) {
            FO_VERIFY_AND_THROW(prop->_complexDataIndex.has_value(), "Complex property has no complex data index");
            writer.Write<uint32_t>(numeric_cast<uint32_t>(_complexData[*prop->_complexDataIndex].second));
            writer.WriteBytes({_complexData[*prop->_complexDataIndex].first.get(), _complexData[*prop->_complexDataIndex].second});
        }
    }

    // Store hashes
    const auto add_hash = [&str_hashes, this](string_view str) {
        if (!str.empty()) {
            const auto hstr = _registrator->_hashResolver->ToHashedString(str);
            str_hashes.emplace(hstr);
        }
    };

    for (const auto& prop : _registrator->_hashProperties) {
        const auto value = PropertiesSerializator::SavePropertyToValue(this, prop, *_registrator->_hashResolver, *_registrator->_nameResolver);

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

            if (prop->IsBaseTypeHash() || prop->IsBaseTypeProtoReference()) {
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

void Properties::RestoreAllData(const vector<uint8_t>& all_data)
{
    FO_STACK_TRACE_ENTRY();

    auto reader = DataReader(all_data);
    const auto whole_pod_data_size = reader.Read<uint32_t>();
    FO_VERIFY_AND_THROW(whole_pod_data_size == _registrator->_wholePodDataSize, "Serialized POD property block was baked for a different property layout", _registrator->GetTypeName(), whole_pod_data_size, _registrator->_wholePodDataSize);
    const auto has_overlay_data = reader.Read<bool>();
    FO_VERIFY_AND_THROW((_baseProps != nullptr) == has_overlay_data, "Serialized property storage mode does not match the target property container", _registrator->GetTypeName(), has_overlay_data, _baseProps != nullptr);

    if (_baseProps) {
        ResetOverlayData();

        const auto overlay_entries_count = reader.Read<uint32_t>();

        for (uint32_t i = 0; i < overlay_entries_count; i++) {
            const auto prop_index = reader.Read<uint16_t>();
            FO_VERIFY_AND_THROW(prop_index > 0 && prop_index < _registrator->_registeredProperties.size(), "Serialized overlay property index is outside registrator bounds", _registrator->GetTypeName(), prop_index, _registrator->_registeredProperties.size(), overlay_entries_count);
            auto prop = _registrator->_registeredProperties[prop_index].as_nptr();
            FO_VERIFY_AND_THROW(prop, "Serialized overlay property index does not resolve to a registered property", _registrator->GetTypeName(), prop_index, _registrator->_registeredProperties.size());
            const auto data_size = reader.Read<uint32_t>();
            const_span<uint8_t> data = reader.ReadBytes(data_size);
            SetRawData(prop, data);
        }
    }
    else {
        MemFill(_podData, 0, _registrator->_wholePodDataSize);
        ResetComplexData();

        while (true) {
            const auto start_pos = reader.Read<uint32_t>();
            const auto len = reader.Read<uint32_t>();

            if (start_pos == 0 && len == 0) {
                break;
            }

            FO_VERIFY_AND_THROW(start_pos <= _registrator->_wholePodDataSize && len <= _registrator->_wholePodDataSize - start_pos, "Serialized POD data section is outside the property layout bounds", _registrator->GetTypeName(), start_pos, len, _registrator->_wholePodDataSize);
            MemCopy(_podData.get() + start_pos, reader.ReadBytes(len).data(), len);
        }

        // Read complex properties
        const auto complex_props_count = reader.Read<uint32_t>();
        FO_VERIFY_AND_THROW(complex_props_count == _registrator->_complexProperties.size(), "Serialized complex property count does not match the registrator layout", _registrator->GetTypeName(), complex_props_count, _registrator->_complexProperties.size());

        for (const auto& prop : _registrator->_complexProperties) {
            FO_VERIFY_AND_THROW(prop->_complexDataIndex.has_value(), "Registered complex property has no complex-data slot while restoring data", _registrator->GetTypeName(), prop->GetName(), prop->GetRegIndex());
            const auto data_size = reader.Read<uint32_t>();
            const_span<uint8_t> data = reader.ReadBytes(data_size);
            SetRawData(prop, data);
        }
    }

    reader.VerifyEnd();

    _storeDataRevision++;
}

auto Properties::StoreData(bool with_protected) const -> StoredData
{
    FO_STACK_TRACE_ENTRY();

    auto& cache_ptr = _storeDataCaches[with_protected ? 1 : 0];

    if (!cache_ptr.has_value()) {
        cache_ptr = StoreDataCache {};
    }

    ptr<std::remove_reference_t<decltype(*cache_ptr)>> cache = &*cache_ptr;

    if (cache->Revision == _storeDataRevision) {
        return {.Data = &cache->Data, .Sizes = &cache->Sizes};
    }

    cache->Data.clear();
    cache->Sizes.clear();
    cache->PropertyIndices.clear();

    cache->Data.push_back(_baseProps ? &SEPARATE_PROPS_STORE_TYPE : &FULL_DATA_STORE_TYPE);
    cache->Sizes.push_back(sizeof(uint8_t));

    if (_baseProps) {
        for (const auto& entry : _overlayEntries) {
            auto prop = _registrator->GetPropertyByIndexUnsafe(entry.PropRegIndex);

            if (!IsOverlayPropertyIncluded(prop, with_protected)) {
                continue;
            }

            const auto raw_data = GetRawData(prop);

            cache->PropertyIndices.push_back(entry.PropRegIndex);
            cache->Data.push_back(raw_data.data());
            cache->Sizes.push_back(numeric_cast<uint32_t>(raw_data.size()));
        }

        if (!cache->PropertyIndices.empty()) {
            const auto property_indices = make_ptr(cache->PropertyIndices.data());
            cache->Data.insert(cache->Data.begin() + 1, property_indices.reinterpret_as<uint8_t>().get());
            cache->Sizes.insert(cache->Sizes.begin() + 1, numeric_cast<uint32_t>(cache->PropertyIndices.size() * sizeof(uint16_t)));
        }
    }
    else {
        cache->PropertyIndices = with_protected ? _registrator->_publicProtectedComplexDataProps : _registrator->_publicComplexDataProps;

        const auto preserve_size = 2u + (!cache->PropertyIndices.empty() ? 1u + cache->PropertyIndices.size() : 0);
        cache->Data.reserve(preserve_size);
        cache->Sizes.reserve(preserve_size);

        // Store plain properties data
        cache->Data.push_back(_podData.get());
        cache->Sizes.push_back(numeric_cast<uint32_t>(_registrator->_publicPodDataSpace.size()) + (with_protected ? numeric_cast<uint32_t>(_registrator->_protectedPodDataSpace.size()) : 0));

        // Filter complex data to send
        for (size_t i = 0; i < cache->PropertyIndices.size();) {
            auto prop = _registrator->GetPropertyByIndexUnsafe(cache->PropertyIndices[i]);
            FO_VERIFY_AND_THROW(prop->_complexDataIndex.has_value(), "Registered complex property has no complex-data slot while preparing stored data", _registrator->GetTypeName(), prop->GetName(), prop->GetRegIndex());

            if (!_complexData[*prop->_complexDataIndex].first) {
                cache->PropertyIndices.erase(cache->PropertyIndices.begin() + numeric_cast<int32_t>(i));
            }
            else {
                i++;
            }
        }

        // Store complex properties data
        if (!cache->PropertyIndices.empty()) {
            const auto property_indices = make_ptr(cache->PropertyIndices.data());
            cache->Data.push_back(property_indices.reinterpret_as<uint8_t>().get());
            cache->Sizes.push_back(numeric_cast<uint32_t>(cache->PropertyIndices.size() * sizeof(uint16_t)));

            for (const auto index : cache->PropertyIndices) {
                auto prop = _registrator->GetPropertyByIndexUnsafe(index);
                cache->Data.push_back(_complexData[*prop->_complexDataIndex].first.get());
                cache->Sizes.push_back(numeric_cast<uint32_t>(_complexData[*prop->_complexDataIndex].second));
            }
        }
    }

    cache->Revision = _storeDataRevision;
    return {.Data = &cache->Data, .Sizes = &cache->Sizes};
}

void Properties::RestoreData(const vector<nptr<const uint8_t>>& all_data, const vector<uint32_t>& all_data_sizes)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(all_data.size() == all_data_sizes.size(), "Serialized property payload pointer list and size list have different lengths", _registrator->GetTypeName(), all_data.size(), all_data_sizes.size());

    const auto read_raw_data_span = [](nptr<const uint8_t> data, size_t size) noexcept -> span<const uint8_t> {
        if (size == 0) {
            return {};
        }

        FO_STRONG_ASSERT(data, "Raw data pointer is null for non-zero size");
        return {data.get(), size};
    };

    const auto apply_separate_props_data = [this, &read_raw_data_span](const vector<nptr<const uint8_t>>& separate_data, const vector<uint32_t>& separate_sizes) {
        if (separate_data.empty()) {
            return;
        }

        const uint32_t property_data_count = separate_sizes[0] / sizeof(uint16_t);
        FO_VERIFY_AND_THROW(separate_sizes[0] == property_data_count * sizeof(uint16_t), "Serialized property index table size is not aligned to uint16 entries", _registrator->GetTypeName(), separate_sizes[0], sizeof(uint16_t));
        FO_VERIFY_AND_THROW(separate_data.size() == 1 + property_data_count, "Serialized separate property payload count does not match index table count", _registrator->GetTypeName(), separate_data.size(), property_data_count);
        FO_VERIFY_AND_THROW(separate_data[0], "Property index table payload is null");

        for (uint32_t i = 0; i < property_data_count; i++) {
            uint16_t prop_index {};
            auto prop_index_target = make_ptr(&prop_index).reinterpret_as<uint8_t>();
            auto prop_index_source = separate_data[0].offset(i * sizeof(uint16_t));
            MemCopy(prop_index_target, prop_index_source, sizeof(uint16_t));

            FO_VERIFY_AND_THROW(prop_index > 0, "Serialized separate property payload references the reserved zero property index", _registrator->GetTypeName(), i, property_data_count);
            FO_VERIFY_AND_THROW(prop_index < _registrator->_registeredProperties.size(), "Serialized separate property index is outside the registrator property table", _registrator->GetTypeName(), prop_index, _registrator->_registeredProperties.size(), i, property_data_count);
            auto prop = _registrator->_registeredProperties[prop_index].as_nptr();
            FO_VERIFY_AND_THROW(prop, "Serialized separate property index does not resolve to a registered property", _registrator->GetTypeName(), prop_index, i, property_data_count);
            const auto data_size = separate_sizes[1 + i];
            const auto data = separate_data[1 + i];
            SetRawData(prop, read_raw_data_span(data, data_size));
        }
    };

    const auto apply_full_data = [this, &read_raw_data_span](Properties& target, const vector<nptr<const uint8_t>>& full_data, const vector<uint32_t>& full_sizes) {
        FO_VERIFY_AND_THROW(!full_sizes.empty(), "Serialized full property payload is missing the POD size entry", _registrator->GetTypeName(), full_data.size());

        const auto public_size = numeric_cast<uint32_t>(_registrator->_publicPodDataSpace.size());
        const auto protected_size = numeric_cast<uint32_t>(_registrator->_protectedPodDataSpace.size());
        const auto private_size = numeric_cast<uint32_t>(_registrator->_privatePodDataSpace.size());
        FO_VERIFY_AND_THROW(full_sizes[0] == public_size || full_sizes[0] == public_size + protected_size || full_sizes[0] == public_size + protected_size + private_size, "Serialized POD property payload size does not match public/protected/private section boundaries", _registrator->GetTypeName(), full_sizes[0], public_size, protected_size, private_size);

        if (full_sizes[0] != 0) {
            FO_VERIFY_AND_THROW(full_data[0], "POD data payload is null");
            MemCopy(target._podData, full_data[0], full_sizes[0]);
        }

        if (full_data.size() > 1) {
            const uint32_t complex_data_count = full_sizes[1] / sizeof(uint16_t);
            FO_VERIFY_AND_THROW(complex_data_count > 0, "Serialized full property payload contains a complex index table with no entries", _registrator->GetTypeName(), full_sizes[1]);
            vector<uint16_t> complex_indicies(complex_data_count);
            FO_VERIFY_AND_THROW(full_data[1], "Complex index table payload is null");
            MemCopy(complex_indicies.data(), full_data[1], full_sizes[1]);

            for (size_t i = 0; i < complex_indicies.size(); i++) {
                FO_VERIFY_AND_THROW(complex_indicies[i] > 0, "Serialized complex property index table references the reserved zero property index", _registrator->GetTypeName(), i, complex_indicies.size());
                FO_VERIFY_AND_THROW(complex_indicies[i] < _registrator->_registeredProperties.size(), "Serialized complex property index is outside the registrator property table", _registrator->GetTypeName(), complex_indicies[i], _registrator->_registeredProperties.size(), i, complex_indicies.size());
                auto prop = _registrator->GetPropertyByIndexUnsafe(complex_indicies[i]);
                FO_VERIFY_AND_THROW(prop->_complexDataIndex.has_value(), "Serialized complex property index resolved to a property without complex-data slot", _registrator->GetTypeName(), prop->GetName(), complex_indicies[i]);
                const auto data_size = full_sizes[2 + i];
                const auto data = full_data[2 + i];
                target.SetRawData(prop, read_raw_data_span(data, data_size));
            }
        }
    };

    if (all_data.empty()) {
        if (_baseProps) {
            RemoveSyncedOverlayEntries();
        }

        return;
    }

    FO_VERIFY_AND_THROW(all_data_sizes[0] == sizeof(uint8_t), "Serialized empty property set marker size does not match store-type byte size", _registrator->GetTypeName(), all_data_sizes[0], sizeof(uint8_t));

    uint8_t store_type = 0;
    FO_VERIFY_AND_THROW(all_data[0], "Store-type marker payload is null");
    auto store_type_target = make_ptr(&store_type);
    MemCopy(store_type_target, all_data[0], sizeof(store_type));

    vector<nptr<const uint8_t>> payload_data(all_data.begin() + 1, all_data.end());
    vector<uint32_t> payload_sizes(all_data_sizes.begin() + 1, all_data_sizes.end());

    if (store_type == SEPARATE_PROPS_STORE_TYPE) {
        if (_baseProps) {
            RemoveSyncedOverlayEntries();
        }

        apply_separate_props_data(payload_data, payload_sizes);
    }
    else {
        FO_VERIFY_AND_THROW(store_type == FULL_DATA_STORE_TYPE, "Serialized property store type is not full data");

        if (_baseProps) {
            RemoveSyncedOverlayEntries();

            Properties full_props(_registrator);
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

void Properties::RestoreData(const vector<vector<uint8_t>>& all_data)
{
    FO_STACK_TRACE_ENTRY();

    vector<nptr<const uint8_t>> all_data_ext(all_data.size());
    vector<uint32_t> all_data_sizes(all_data.size());

    for (size_t i = 0; i < all_data.size(); i++) {
        all_data_ext[i] = !all_data[i].empty() ? all_data[i].data() : nullptr;
        all_data_sizes[i] = numeric_cast<uint32_t>(all_data[i].size());
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
    auto registrator = GetRegistrator();

    for (const auto& [key, value] : key_values) {
        if (key.empty() || key[0] == '$' || key[0] == '_') {
            continue;
        }

        auto prop = registrator->FindProperty(key);

        if (!prop) {
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

auto Properties::SaveToText(nptr<const Properties> base) const -> map<string, string>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!base || _registrator == base->_registrator, "Base properties use a different registrator");

    map<string, string> key_values;

    for (const auto& prop : _registrator->_textProperties) {
        // Skip same as in base or zero values
        if (base) {
            const auto raw_data = GetRawData(prop);
            const auto base_raw_data = base->GetRawData(prop);

            if (raw_data.size() == base_raw_data.size() && MemCompare(raw_data.data(), base_raw_data.data(), raw_data.size())) {
                continue;
            }
        }
        else {
            const auto raw_data = GetRawData(prop);

            if (prop->_podDataOffset.has_value()) {
                if (std::ranges::all_of(raw_data, [](uint8_t value) noexcept { return value == 0; })) {
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
        key_values.emplace(prop->_propName, SavePropertyToText(prop));
    }

    return key_values;
}

auto Properties::CompareData(const Properties& other, const_span<ptr<const Property>> ignore_props, bool ignore_temporary) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_registrator == other._registrator, "Property containers use different registrators");

    if (this == &other) {
        return true;
    }

    if (ignore_props.empty() && !ignore_temporary) {
        if (!_baseProps && !other._baseProps) {
            if (!MemCompare(_podData, other._podData, _registrator->_wholePodDataSize)) {
                return false;
            }

            for (const auto& prop : _registrator->_complexProperties) {
                FO_STRONG_ASSERT(prop->_complexDataIndex.has_value(), "Complex property has no complex data index while comparing properties", prop->GetName(), _registrator->GetTypeName());
                const auto index = *prop->_complexDataIndex;
                const auto& complex_data = _complexData[index];
                const auto& other_complex_data = other._complexData[index];

                if (complex_data.second != other_complex_data.second) {
                    return false;
                }
                if (complex_data.second != 0 && !MemCompare(complex_data.first, other_complex_data.first, complex_data.second)) {
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
                if (entry.DataSize != 0) {
                    nptr<const uint8_t> overlay_data = _overlayData.get();
                    nptr<const uint8_t> other_overlay_data = other._overlayData.get();
                    FO_VERIFY_AND_THROW(overlay_data, "Overlay data buffer is null");
                    FO_VERIFY_AND_THROW(other_overlay_data, "Other overlay data buffer is null");

                    auto entry_data = overlay_data.offset(entry.DataOffset);
                    auto other_entry_data = other_overlay_data.offset(other_entry.DataOffset);

                    if (!MemCompare(entry_data, other_entry_data, entry.DataSize)) {
                        return false;
                    }
                }
            }

            return true;
        }
    }

    const auto get_data_prop_raw_data = [](const Properties& props, const PropertyRegistrator::DataPropertyEntry& data_prop, ptr<const Property> prop) noexcept -> span<const uint8_t> {
        if (props._baseProps) {
            return props.GetRawData(prop);
        }

        if (data_prop.IsPlain) {
            if (data_prop.DataSize == 0) {
                return {};
            }

            nptr<const uint8_t> pod_data = props._podData.get();
            FO_STRONG_ASSERT(pod_data, "POD data buffer is null");

            auto data = pod_data.offset(data_prop.DataIndex);
            return {data.get(), data_prop.DataSize};
        }

        const auto& complex_data = props._complexData[data_prop.DataIndex];
        if (complex_data.second == 0) {
            return {};
        }

        nptr<const uint8_t> complex_data_bytes = complex_data.first.get();
        FO_STRONG_ASSERT(complex_data_bytes, "Complex data buffer is null");

        return {complex_data_bytes.get(), complex_data.second};
    };

    for (const auto& data_prop : _registrator->_dataProperties) {
        auto prop = data_prop.Prop;
        FO_VERIFY_AND_THROW(prop, "Property is null");

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

void Properties::ApplyPropertyFromText(ptr<const Property> prop, string_view text)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_registrator == prop->_registrator, "Property belongs to a different registrator");
    FO_VERIFY_AND_THROW(prop->_podDataOffset.has_value() || prop->_complexDataIndex.has_value(), "Property has neither POD offset nor complex data index");

    PropertiesSerializator::LoadPropertyFromText(this, prop, text, *_registrator->_hashResolver, *_registrator->_nameResolver);
}

auto Properties::SavePropertyToText(ptr<const Property> prop) const -> string
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_registrator == prop->_registrator, "Property belongs to a different registrator");
    FO_VERIFY_AND_THROW(prop->_podDataOffset.has_value() || prop->_complexDataIndex.has_value(), "Property has neither POD offset nor complex data index");

    return PropertiesSerializator::SavePropertyToText(this, prop, *_registrator->_hashResolver, *_registrator->_nameResolver);
}

void Properties::ValidateForRawData(ptr<const Property> prop) const noexcept(false)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop.get(), "Property pointer is null");

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

auto Properties::GetRawData(ptr<const Property> prop) const noexcept -> span<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == prop->_registrator, "Invalid property for raw data", _registrator->GetTypeName(), string_view {prop->GetName()}, prop->_registrator->GetTypeName());

    if (_baseProps) {
        if (auto entry = FindOverlayEntry(prop)) {
            if (entry->DataSize == 0) {
                return {};
            }

            nptr<const uint8_t> overlay_data = _overlayData.get();
            FO_STRONG_ASSERT(overlay_data, "Overlay data buffer is null");

            auto entry_data = overlay_data.offset(entry->DataOffset);
            return {entry_data.get(), entry->DataSize};
        }

        return _baseProps->GetRawData(prop);
    }

    if (prop->IsPlainData()) {
        FO_STRONG_ASSERT(prop->_podDataOffset.has_value(), "Plain property has no pod data offset while reading raw data", prop->GetName(), _registrator->GetTypeName());
        return {&_podData[*prop->_podDataOffset], prop->_baseType.Size};
    }
    else {
        FO_STRONG_ASSERT(prop->_complexDataIndex.has_value(), "Complex property has no complex data index while reading raw data", prop->GetName(), _registrator->GetTypeName());

        const auto& complex_data = _complexData[*prop->_complexDataIndex];
        return {complex_data.first.get(), complex_data.second};
    }
}

auto Properties::GetRawDataSize(ptr<const Property> prop) const noexcept -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == prop->_registrator, "Invalid property for raw data size", _registrator->GetTypeName(), string_view {prop->GetName()}, prop->_registrator->GetTypeName());

    if (_baseProps) {
        if (auto entry = FindOverlayEntry(prop)) {
            return entry->DataSize;
        }

        return _baseProps->GetRawDataSize(prop);
    }
    else {
        if (prop->IsPlainData()) {
            FO_STRONG_ASSERT(prop->_podDataOffset.has_value(), "Plain property has no pod data offset while reading raw data size", prop->GetName(), _registrator->GetTypeName());
            return prop->_baseType.Size;
        }
        else {
            FO_STRONG_ASSERT(prop->_complexDataIndex.has_value(), "Complex property has no complex data index while reading raw data size", prop->GetName(), _registrator->GetTypeName());

            const auto& complex_data = _complexData[*prop->_complexDataIndex];
            return complex_data.second;
        }
    }
}

void Properties::CopyRawData(ptr<const Property> prop, PropertyRawData& prop_data) const noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (_baseProps) {
        if (auto entry = FindOverlayEntry(prop)) {
            const auto raw_data = span<const uint8_t>(entry->DataSize != 0 ? _overlayData.get() + entry->DataOffset : nullptr, entry->DataSize);
            prop_data.Set(raw_data.data(), raw_data.size());
            return;
        }

        _baseProps->CopyRawData(prop, prop_data);
        return;
    }

    const auto raw_data = GetRawData(prop);
    prop_data.Set(raw_data.data(), raw_data.size());
}

auto Properties::IsRawDataEqual(ptr<const Property> prop, span<const uint8_t> raw_data) const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_baseProps) {
        if (auto entry = FindOverlayEntry(prop)) {
            const auto current_overlay_data = span<const uint8_t>(entry->DataSize != 0 ? _overlayData.get() + entry->DataOffset : nullptr, entry->DataSize);
            return RawDataEqual(raw_data, current_overlay_data);
        }

        return _baseProps->IsRawDataEqual(prop, raw_data);
    }

    return RawDataEqual(raw_data, GetRawData(prop));
}

void Properties::SetRawData(ptr<const Property> prop, span<const uint8_t> raw_data) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(_registrator == prop->_registrator, "Invalid property for raw data write", _registrator->GetTypeName(), string_view {prop->GetName()}, prop->_registrator->GetTypeName());
    FO_STRONG_ASSERT(!prop->IsPlainData() || prop->GetBaseSize() == raw_data.size(), "Plain property raw data write size mismatch", prop->GetName(), _registrator->GetTypeName(), prop->GetBaseSize(), raw_data.size());

    if (_baseProps) {
        PropertyRawData base_prop_data;
        _baseProps->CopyRawData(prop, base_prop_data);
        const auto base_raw_data = span<const uint8_t>(base_prop_data.GetPtrAs<uint8_t>().get(), base_prop_data.GetSize());

        if (RawDataEqual(raw_data, base_raw_data)) {
            RemoveOverlayEntry(prop);
            return;
        }

        const auto get_overlay_raw_data = [this](const OverlayEntry& entry) noexcept -> span<const uint8_t> {
            if (entry.DataSize == 0) {
                return {};
            }

            nptr<const uint8_t> overlay_data = _overlayData.get();
            FO_STRONG_ASSERT(overlay_data, "Overlay data buffer is null");

            auto entry_data = overlay_data.offset(entry.DataOffset);
            return {entry_data.get(), entry.DataSize};
        };
        const auto set_overlay_raw_data = [this](size_t data_offset, span<const uint8_t> data) noexcept {
            if (data.empty()) {
                return;
            }

            nptr<uint8_t> overlay_data = _overlayData.get();
            FO_STRONG_ASSERT(overlay_data, "Overlay data buffer is null");

            auto target = overlay_data.offset(data_offset);
            MemCopy(target, data.data(), data.size());
        };

        if (auto entry = FindOverlayEntry(prop)) {
            const auto current_overlay_data = get_overlay_raw_data(*entry);

            if (RawDataEqual(raw_data, current_overlay_data)) {
                return;
            }

            if (entry->DataSize == raw_data.size()) {
                set_overlay_raw_data(entry->DataOffset, raw_data);
            }
            else {
                // Release the old block before allocating so the allocator can reuse its bytes
                _overlayGarbageSize += entry->DataSize;
                entry->DataSize = 0;
                entry->DataOffset = AllocOverlayData(raw_data.size(), prop->GetDataAlignment());
                entry->DataSize = static_cast<uint32_t>(raw_data.size());

                set_overlay_raw_data(entry->DataOffset, raw_data);
            }
        }
        else {
            OverlayEntry new_entry;
            new_entry.PropRegIndex = prop->GetRegIndex();
            new_entry.DataOffset = AllocOverlayData(raw_data.size(), prop->GetDataAlignment());
            new_entry.DataSize = static_cast<uint32_t>(raw_data.size());

            set_overlay_raw_data(new_entry.DataOffset, raw_data);

            const auto it = std::ranges::lower_bound(_overlayEntries, new_entry.PropRegIndex, std::ranges::less {}, &OverlayEntry::PropRegIndex);
            const auto insert_pos = numeric_cast<size_t>(std::distance(_overlayEntries.begin(), it));
            _overlayEntries.emplace(it, new_entry);

            if (ShouldUseOverlayEntryIndex(_overlayEntries.size())) {
                if (_overlayEntryIndex.empty()) {
                    RebuildOverlayEntryIndex();
                }
                else {
                    _overlayEntryIndex[new_entry.PropRegIndex] = numeric_cast<int32_t>(insert_pos);

                    for (size_t i = insert_pos + 1; i < _overlayEntries.size(); i++) {
                        _overlayEntryIndex[_overlayEntries[i].PropRegIndex] = numeric_cast<int32_t>(i);
                    }
                }
            }
        }

        if (_overlayGarbageSize > (_overlayDataCapacity / 2)) {
            RepackOverlayData(_overlayDataCapacity);
        }

        _storeDataRevision++;
    }
    else {
        if (prop->IsPlainData()) {
            FO_STRONG_ASSERT(prop->_podDataOffset.has_value(), "Plain property has no pod data offset while writing raw data", prop->GetName(), _registrator->GetTypeName());

            if (!raw_data.empty()) {
                nptr<uint8_t> pod_data = _podData.get();
                FO_STRONG_ASSERT(pod_data, "POD data buffer is null");

                auto target = pod_data.offset(*prop->_podDataOffset);
                MemCopy(target, raw_data.data(), raw_data.size());
            }
        }
        else {
            FO_STRONG_ASSERT(prop->_complexDataIndex.has_value(), "Complex property has no complex data index while writing raw data", prop->GetName(), _registrator->GetTypeName());

            auto& complex_data = _complexData[*prop->_complexDataIndex];

            if (raw_data.size() != complex_data.second) {
                if (!raw_data.empty()) {
                    complex_data.first = SafeAlloc::MakeUniqueArr<uint8_t>(raw_data.size());
                    complex_data.second = raw_data.size();
                }
                else {
                    complex_data.first.reset();
                    complex_data.second = 0;
                }
            }

            if (!raw_data.empty()) {
                nptr<uint8_t> complex_data_bytes = complex_data.first.get();
                FO_STRONG_ASSERT(complex_data_bytes, "Complex data buffer is null");

                MemCopy(complex_data_bytes, raw_data.data(), raw_data.size());
            }
        }

        _storeDataRevision++;
    }
}

void Properties::SetValueFromData(ptr<const Property> prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");
        FO_VERIFY_AND_THROW(!prop->_setters.empty(), "Virtual property has no setter while applying raw property data", prop->GetName(), prop_data.GetSize());

        for (const auto& setter : prop->_setters) {
            setter(_entity, prop, prop_data);
        }
    }
    else {
        if (!prop->_setters.empty() && _entity) {
            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }

        SetRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});

        if (_entity) {
            for (const auto& setter : prop->_postSetters) {
                setter(_entity, prop);
            }
        }
    }
}

auto Properties::GetPlainDataValueAsInt(ptr<const Property> prop) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->IsPlainData(), "Property is not plain data");

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            return numeric_cast<int32_t>(GetValue<uint8_t>(prop));
        }
        if (base_type.Size == 2) {
            return numeric_cast<int32_t>(GetValue<uint16_t>(prop));
        }
        if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                return numeric_cast<int32_t>(GetValue<int32_t>(prop));
            }
            else {
                return numeric_cast<int32_t>(GetValue<uint32_t>(prop));
            }
        }
    }
    else if (base_type.IsBool) {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            return iround<int32_t>(GetValue<float32_t>(prop));
        }
        if (base_type.IsDoubleFloat) {
            return iround<int32_t>(GetValue<float64_t>(prop));
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            return numeric_cast<int32_t>(GetValue<int8_t>(prop));
        }
        if (base_type.Size == 2) {
            return numeric_cast<int32_t>(GetValue<int16_t>(prop));
        }
        if (base_type.Size == 4) {
            return numeric_cast<int32_t>(GetValue<int32_t>(prop));
        }
        if (base_type.Size == 8) {
            return numeric_cast<int32_t>(GetValue<int64_t>(prop));
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            return numeric_cast<int32_t>(GetValue<uint8_t>(prop));
        }
        if (base_type.Size == 2) {
            return numeric_cast<int32_t>(GetValue<uint16_t>(prop));
        }
        if (base_type.Size == 4) {
            return numeric_cast<int32_t>(GetValue<uint32_t>(prop));
        }
    }

    throw PropertiesException("Invalid property for get as int", prop->GetName());
}

auto Properties::GetPlainDataValueAsAny(ptr<const Property> prop) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->IsPlainData(), "Property is not plain data");

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsFixedType || base_type.IsEntityProto) {
        return any_t {string(GetValue<hstring>(prop).as_str())};
    }
    else if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            return any_t {strex("{}", GetValue<uint8_t>(prop))};
        }
        if (base_type.Size == 2) {
            return any_t {strex("{}", GetValue<uint16_t>(prop))};
        }
        if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                return any_t {strex("{}", GetValue<int32_t>(prop))};
            }
            else {
                return any_t {strex("{}", GetValue<uint32_t>(prop))};
            }
        }
    }
    else if (base_type.IsBool) {
        return any_t {strex("{}", GetValue<bool>(prop))};
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            return any_t {strex("{}", GetValue<float32_t>(prop))};
        }
        if (base_type.IsDoubleFloat) {
            return any_t {strex("{}", GetValue<float64_t>(prop))};
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            return any_t {strex("{}", GetValue<int8_t>(prop))};
        }
        if (base_type.Size == 2) {
            return any_t {strex("{}", GetValue<int16_t>(prop))};
        }
        if (base_type.Size == 4) {
            return any_t {strex("{}", GetValue<int32_t>(prop))};
        }
        if (base_type.Size == 8) {
            return any_t {strex("{}", GetValue<int64_t>(prop))};
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            return any_t {strex("{}", GetValue<uint8_t>(prop))};
        }
        if (base_type.Size == 2) {
            return any_t {strex("{}", GetValue<uint16_t>(prop))};
        }
        if (base_type.Size == 4) {
            return any_t {strex("{}", GetValue<uint32_t>(prop))};
        }
    }

    throw PropertiesException("Invalid property for get as any", prop->GetName());
}

void Properties::SetPlainDataValueAsInt(ptr<const Property> prop, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->IsPlainData(), "Property is not plain data");

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            SetValue<uint8_t>(prop, numeric_cast<uint8_t>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16_t>(prop, numeric_cast<uint16_t>(value));
        }
        else if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                SetValue<int32_t>(prop, value);
            }
            else {
                SetValue<uint32_t>(prop, numeric_cast<uint32_t>(value));
            }
        }
    }
    else if (base_type.IsBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            SetValue<float32_t>(prop, numeric_cast<float32_t>(value));
        }
        else if (base_type.IsDoubleFloat) {
            SetValue<float64_t>(prop, numeric_cast<float64_t>(value));
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<int8_t>(prop, numeric_cast<int8_t>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<int16_t>(prop, numeric_cast<int16_t>(value));
        }
        else if (base_type.Size == 4) {
            SetValue<int32_t>(prop, value);
        }
        else if (base_type.Size == 8) {
            SetValue<int64_t>(prop, numeric_cast<int64_t>(value));
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<uint8_t>(prop, numeric_cast<uint8_t>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16_t>(prop, numeric_cast<uint16_t>(value));
        }
        else if (base_type.Size == 4) {
            SetValue<uint32_t>(prop, numeric_cast<uint32_t>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int", prop->GetName());
    }
}

void Properties::SetPlainDataValueAsAny(ptr<const Property> prop, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->IsPlainData(), "Property is not plain data");

    const auto& base_type = prop->IsBaseTypeSimpleStruct() ? prop->GetStructFirstType() : prop->GetBaseType();

    if (base_type.IsFixedType || base_type.IsEntityProto) {
        SetValue<hstring>(prop, _registrator->GetHashResolver()->ToHashedString(value));
    }
    else if (base_type.IsEnum) {
        if (base_type.Size == 1) {
            SetValue<uint8_t>(prop, numeric_cast<uint8_t>(strvex(value).to_int32()));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16_t>(prop, numeric_cast<uint16_t>(strvex(value).to_int32()));
        }
        else if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                SetValue<int32_t>(prop, strvex(value).to_int32());
            }
            else {
                SetValue<uint32_t>(prop, strvex(value).to_uint32());
            }
        }
    }
    else if (base_type.IsBool) {
        SetValue<bool>(prop, strvex(value).to_bool());
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            SetValue<float32_t>(prop, strvex(value).to_float32());
        }
        else if (base_type.IsDoubleFloat) {
            SetValue<float64_t>(prop, strvex(value).to_float64());
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<int8_t>(prop, numeric_cast<int8_t>(strex(value).to_int32()));
        }
        else if (base_type.Size == 2) {
            SetValue<int16_t>(prop, numeric_cast<int16_t>(strex(value).to_int32()));
        }
        else if (base_type.Size == 4) {
            SetValue<int32_t>(prop, numeric_cast<int32_t>(strex(value).to_int32()));
        }
        else if (base_type.Size == 8) {
            SetValue<int64_t>(prop, numeric_cast<int64_t>(strex(value).to_int64()));
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<uint8_t>(prop, numeric_cast<uint8_t>(strex(value).to_int32()));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16_t>(prop, numeric_cast<uint16_t>(strex(value).to_int32()));
        }
        else if (base_type.Size == 4) {
            SetValue<uint32_t>(prop, numeric_cast<uint32_t>(strex(value).to_int32()));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as any", prop->GetName());
    }
}

auto Properties::GetValueAsInt(int32_t property_index) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto prop = _registrator->GetPropertyByIndex(property_index);

    if (!prop) {
        throw PropertiesException("Property not found", property_index);
    }

    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't retreive integer value from non plain data property", prop->GetName());
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't retreive integer value from disabled property", prop->GetName());
    }
    return GetPlainDataValueAsInt(prop.as_ptr());
}

auto Properties::GetValueAsAny(int32_t property_index) const -> any_t
{
    FO_STACK_TRACE_ENTRY();

    auto prop = _registrator->GetPropertyByIndex(property_index);

    if (!prop) {
        throw PropertiesException("Property not found", property_index);
    }

    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't retreive integer value from non plain data property", prop->GetName());
    }
    if (prop->IsDisabled()) {
        throw PropertiesException("Can't retreive integer value from disabled property", prop->GetName());
    }
    return GetPlainDataValueAsAny(prop.as_ptr());
}

void Properties::SetValueAsInt(int32_t property_index, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    auto prop = _registrator->GetPropertyByIndex(property_index);

    if (!prop) {
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

void Properties::SetValueAsAny(int32_t property_index, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    auto prop = _registrator->GetPropertyByIndex(property_index);

    if (!prop) {
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

void Properties::SetValueAsIntProps(int32_t property_index, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    auto prop = _registrator->GetPropertyByIndex(property_index);

    if (!prop) {
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
            SetValue<uint8_t>(prop, numeric_cast<uint8_t>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16_t>(prop, numeric_cast<uint16_t>(value));
        }
        else if (base_type.Size == 4) {
            if (base_type.EnumUnderlyingType->IsSignedInt) {
                SetValue<int32_t>(prop, numeric_cast<int32_t>(value));
            }
            else {
                SetValue<uint32_t>(prop, numeric_cast<uint32_t>(value));
            }
        }
    }
    else if (base_type.IsBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (base_type.IsFloat) {
        if (base_type.IsSingleFloat) {
            SetValue<float32_t>(prop, numeric_cast<float32_t>(value));
        }
        else if (base_type.IsDoubleFloat) {
            SetValue<float64_t>(prop, numeric_cast<float64_t>(value));
        }
    }
    else if (base_type.IsInt && base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<int8_t>(prop, numeric_cast<int8_t>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<int16_t>(prop, numeric_cast<int16_t>(value));
        }
        else if (base_type.Size == 4) {
            SetValue<int32_t>(prop, value);
        }
        else if (base_type.Size == 8) {
            SetValue<int64_t>(prop, numeric_cast<int64_t>(value));
        }
    }
    else if (base_type.IsInt && !base_type.IsSignedInt) {
        if (base_type.Size == 1) {
            SetValue<uint8_t>(prop, numeric_cast<uint8_t>(value));
        }
        else if (base_type.Size == 2) {
            SetValue<uint16_t>(prop, numeric_cast<uint16_t>(value));
        }
        else if (base_type.Size == 4) {
            SetValue<uint32_t>(prop, numeric_cast<uint32_t>(value));
        }
    }
    else {
        throw PropertiesException("Invalid property for set as int32 props", prop->GetName());
    }
}

void Properties::SetValueAsAnyProps(int32_t property_index, const any_t& value)
{
    FO_STACK_TRACE_ENTRY();

    auto prop = _registrator->GetPropertyByIndex(property_index);

    if (!prop) {
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

auto Properties::ResolveHash(hstring::hash_t h, nptr<bool> failed) const noexcept -> hstring
{
    FO_NO_STACK_TRACE_ENTRY();

    return _registrator->_hashResolver->ResolveHash(h, failed);
}

void Properties::SetValue(ptr<const Property> prop, PropertyRawData& prop_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop.get(), "Property pointer is null");

    if (prop->IsVirtual() && prop->_setters.empty()) {
        throw PropertiesException("Setter not set");
    }

    if (!prop->IsVirtual()) {
        if (IsRawDataEqual(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()})) {
            return;
        }
    }

    if (!prop->_setters.empty()) {
        for (auto& setter : prop->_setters) {
            setter(_entity, prop, prop_data);
        }
    }

    if (!prop->IsVirtual()) {
        SetRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});

        if (!prop->_postSetters.empty()) {
            for (auto& setter : prop->_postSetters) {
                setter(_entity, prop);
            }
        }
    }
}

PropertyRegistrator::PropertyRegistrator(string_view type_name, EngineSideKind side, ptr<HashResolver> hash_resolver, ptr<NameResolver> name_resolver) :
    _typeName {hash_resolver->ToHashedString(type_name)},
    _typeNamePlural {hash_resolver->ToHashedString(strex("{}s", type_name))},
    _side {side},
    _propMigrationRuleName {hash_resolver->ToHashedString("Property")},
    _hashResolver {hash_resolver},
    _nameResolver {name_resolver}
{
    FO_STACK_TRACE_ENTRY();

    // Add None entry
    _registeredProperties.emplace_back();
}

PropertyRegistrator::~PropertyRegistrator()
{
    FO_STACK_TRACE_ENTRY();
}

auto PropertyRegistrator::GetPropertyByIndexUnsafe(size_t property_index) const noexcept -> ptr<const Property>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto prop = _registeredProperties[property_index].as_nptr();
    FO_STRONG_ASSERT(prop, "Property at index is null");
    return prop;
}

auto PropertyRegistrator::GetPropertyByIndex(int32_t property_index) const noexcept -> nptr<const Property>
{
    FO_STACK_TRACE_ENTRY();

    // Skip None entry
    if (property_index >= 1 && static_cast<size_t>(property_index) < _registeredProperties.size()) {
        auto prop = _registeredProperties[numeric_cast<size_t>(property_index)].as_nptr();
        FO_STRONG_ASSERT(prop, "Property at index is null");
        return prop;
    }

    return nullptr;
}

auto PropertyRegistrator::FindProperty(string_view property_name) const -> nptr<const Property>
{
    FO_STACK_TRACE_ENTRY();

    auto key = string(property_name);
    const auto hkey = _hashResolver->ToHashedString(key);

    if (const auto rule = _nameResolver->CheckMigrationRule(_propMigrationRuleName, _typeName, hkey); rule.has_value()) {
        key = rule.value();
    }

    if (const auto it = _registeredPropertiesLookup.find(key); it != _registeredPropertiesLookup.end()) {
        return it->second;
    }

    return nullptr;
}

auto PropertyRegistrator::GetPropertyGroups() const noexcept -> map<string, vector<ptr<const Property>>>
{
    FO_STACK_TRACE_ENTRY();

    map<string, vector<ptr<const Property>>> result;

    for (const auto& [group_name, properties] : _propertyGroups) {
        vector<ptr<const Property>> group_properties;
        group_properties.reserve(properties.size());

        for (const auto& prop : properties | std::views::keys) {
            group_properties.emplace_back(prop);
        }

        result.emplace(group_name, std::move(group_properties));
    }

    return result;
}

auto PropertyRegistrator::RegisterProperty(const span<const string_view>& tokens) -> ptr<const Property>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(tokens.size() >= 3, "Property declaration is missing scope, type or name tokens", _typeName, tokens.size());

    auto prop = SafeAlloc::MakeUnique<Property>(this);

    prop->_isCommon = tokens[0] == "Common";
    prop->_isServerOnly = tokens[0] == "Server";
    prop->_isClientOnly = tokens[0] == "Client";
    FO_VERIFY_AND_THROW(static_cast<int32_t>(prop->_isCommon) + static_cast<int32_t>(prop->_isServerOnly) + static_cast<int32_t>(prop->_isClientOnly) == 1, "Property declaration has an invalid scope token", _typeName, tokens[2], tokens[0]);

    const auto type = _nameResolver->ResolveComplexType(tokens[1]);
    FO_VERIFY_AND_THROW(!type.IsMutable, "Property declaration uses a mutable type wrapper where property mutability must be declared by a tag", _typeName, tokens[2], tokens[1]);
    FO_VERIFY_AND_THROW(!type.BaseType.IsEntity || type.BaseType.IsFixedType || type.BaseType.IsEntityProto, "Property declaration uses an unsupported entity base type", _typeName, tokens[2], tokens[1], type.BaseType.Name);
    FO_VERIFY_AND_THROW(type.Kind != ComplexTypeKind::Callback, "Property type cannot be a callback", prop->GetName(), tokens[1], type.Kind);

    if (type.Kind == ComplexTypeKind::Dict || type.Kind == ComplexTypeKind::DictOfArray) {
        FO_VERIFY_AND_THROW(type.KeyType.has_value(), "Dictionary property declaration has no key type", _typeName, tokens[2], tokens[1]);
        FO_VERIFY_AND_THROW(!type.KeyType->IsEntity, "Dictionary property declaration uses an entity type as a key", _typeName, tokens[2], tokens[1], type.KeyType->Name);

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
        else if (!type.BaseType.IsRefType) {
            prop->_isPlainData = true;
        }
    }

    // Raw data alignment (see the layout contract in Properties.h): the strictest alignment used
    // inside the property raw data, so an aligned blob start keeps every interior item aligned
    if (prop->_isPlainData) {
        FO_VERIFY_AND_THROW(prop->_baseType.Size != 0, "Property base type has no size", _typeName, prop->GetName(), prop->_viewTypeName);
        prop->_dataAlignment = alignment_for_size(prop->_baseType.Size);
    }
    else if (prop->_isArray) {
        if (prop->_isArrayOfString) {
            prop->_dataAlignment = sizeof(uint32_t);
        }
        else if (prop->_baseType.IsRefType) {
            prop->_dataAlignment = MAX_SERIALIZED_ALIGNMENT;
        }
        else {
            FO_VERIFY_AND_THROW(prop->_baseType.Size != 0, "Property base type has no size", _typeName, prop->GetName(), prop->_viewTypeName);
            prop->_dataAlignment = alignment_for_size(prop->_baseType.Size);
        }
    }
    else if (prop->_isDict) {
        const size_t key_alignment = prop->_isDictKeyString ? sizeof(uint32_t) : alignment_for_size(prop->_dictKeyType.Size);
        FO_VERIFY_AND_THROW(prop->_isDictKeyString || prop->_dictKeyType.Size != 0, "Dictionary property key type has no size", _typeName, prop->GetName(), prop->_viewTypeName);

        size_t value_alignment;

        if (prop->_baseType.IsRefType) {
            value_alignment = MAX_SERIALIZED_ALIGNMENT;
        }
        else if (prop->_baseType.IsString) {
            value_alignment = sizeof(uint32_t);
        }
        else {
            FO_VERIFY_AND_THROW(prop->_baseType.Size != 0, "Dictionary property value type has no size", _typeName, prop->GetName(), prop->_viewTypeName);
            value_alignment = alignment_for_size(prop->_baseType.Size);
        }

        if (prop->_isDictOfArray) {
            value_alignment = std::max(value_alignment, sizeof(uint32_t));
        }

        prop->_dataAlignment = std::max(key_alignment, value_alignment);
    }
    else if (prop->_baseType.IsRefType) {
        prop->_dataAlignment = MAX_SERIALIZED_ALIGNMENT;
    }

    prop->_propName = tokens[2];
    const auto h = _hashResolver->ToHashedString(prop->_propName);
    ignore_unused(h);

    if (const auto dot_pos = prop->_propName.find('.'); dot_pos != string::npos) {
        prop->_componentName = prop->_propName.substr(0, dot_pos);
        prop->_propNameWithoutComponent = prop->_propName.substr(dot_pos + 1);
        FO_VERIFY_AND_THROW(!prop->_componentName.empty(), "Property component-qualified name has an empty component part", _typeName, prop->_propName);
        FO_VERIFY_AND_THROW(!prop->_propNameWithoutComponent.empty(), "Property component-qualified name has an empty property part", _typeName, prop->_propName);
    }
    else {
        prop->_propNameWithoutComponent = prop->_propName;
    }

    bool is_component_marker = false;

    for (size_t i = 3; i < tokens.size(); i++) {
        if (tokens[i] == "Group") {
            FO_VERIFY_AND_THROW(i + 2 < tokens.size() && tokens[i + 1] == "=", "Property Group tag must be followed by '=' and a group name", prop->GetName(), i, tokens.size(), i + 1 < tokens.size() ? tokens[i + 1] : string_view {});

            const auto group = tokens[i + 2];
            int32_t priority = 0;

            if (i + 4 < tokens.size() && tokens[i + 3] == "^") {
                priority = strex(tokens[i + 4]).to_int32();
                i += 2;
            }

            if (const auto it = _propertyGroups.find(group); it != _propertyGroups.end()) {
                it->second.emplace_back(pair {ptr<const Property> {prop}, priority});

                std::ranges::stable_sort(it->second, [](auto&& a, auto&& b) -> bool {
                    return a.second < b.second; // Sort by priority
                });
            }
            else {
                _propertyGroups.emplace(group, vector<pair<ptr<const Property>, int32_t>> {pair {ptr<const Property> {prop}, priority}});
            }

            i += 2;
        }
        else if (tokens[i] == "Mutable") {
            FO_VERIFY_AND_THROW(!prop->_isMutable, "Property declaration contains duplicate Mutable tag", _typeName, prop->GetName(), i);
            prop->_isMutable = true;
        }
        else if (tokens[i] == "CoreProperty") {
            FO_VERIFY_AND_THROW(!prop->_isCoreProperty, "Property declaration contains duplicate CoreProperty tag", _typeName, prop->GetName(), i);
            prop->_isCoreProperty = true;
        }
        else if (tokens[i] == "Component") {
            FO_VERIFY_AND_THROW(!prop->_isComponentItself, "Property declaration contains duplicate Component tag", _typeName, prop->GetName(), i);
            prop->_isComponentItself = true;
            is_component_marker = true;
        }
        else if (tokens[i] == "Persistent") {
            FO_VERIFY_AND_THROW(!prop->_isPersistent, "Property declaration contains duplicate Persistent tag", _typeName, prop->GetName(), i);
            prop->_isPersistent = true;
        }
        else if (tokens[i] == "Virtual") {
            FO_VERIFY_AND_THROW(!prop->_isVirtual, "Property declaration contains duplicate Virtual tag", _typeName, prop->GetName(), i);
            prop->_isVirtual = true;
        }
        else if (tokens[i] == "OwnerSync") {
            FO_VERIFY_AND_THROW(!prop->_isOwnerSync, "Property declaration contains duplicate OwnerSync tag", _typeName, prop->GetName(), i);
            prop->_isOwnerSync = true;
        }
        else if (tokens[i] == "PublicSync") {
            FO_VERIFY_AND_THROW(!prop->_isPublicSync, "Property declaration contains duplicate PublicSync tag", _typeName, prop->GetName(), i);
            prop->_isPublicSync = true;
        }
        else if (tokens[i] == "NoSync") {
            FO_VERIFY_AND_THROW(!prop->_isNoSync, "Property declaration contains duplicate NoSync tag", _typeName, prop->GetName(), i);
            prop->_isNoSync = true;
        }
        else if (tokens[i] == "ModifiableByClient") {
            FO_VERIFY_AND_THROW(!prop->_isModifiableByClient, "Property declaration contains duplicate ModifiableByClient tag", _typeName, prop->GetName(), i);
            prop->_isModifiableByClient = true;
        }
        else if (tokens[i] == "ModifiableByAnyClient") {
            FO_VERIFY_AND_THROW(!prop->_isModifiableByAnyClient, "Property declaration contains duplicate ModifiableByAnyClient tag", _typeName, prop->GetName(), i);
            prop->_isModifiableByAnyClient = true;
        }
        else if (tokens[i] == "Historical") {
            FO_VERIFY_AND_THROW(!prop->_isHistorical, "Property declaration contains duplicate Historical tag", _typeName, prop->GetName(), i);
            prop->_isHistorical = true;
        }
        else if (tokens[i] == "Resource") {
            FO_VERIFY_AND_THROW(prop->IsBaseTypeHash(), "Property Resource tag can only be applied to hash-backed properties", _typeName, prop->GetName(), prop->_viewTypeName);
            FO_VERIFY_AND_THROW(!prop->_isResourceHash, "Property declaration contains duplicate Resource tag", _typeName, prop->GetName(), i);
            prop->_isResourceHash = true;
        }
        else if (tokens[i] == "ScriptFuncType") {
            FO_VERIFY_AND_THROW(i + 2 < tokens.size() && tokens[i + 1] == "=", "Property ScriptFuncType tag must be followed by '=' and a function type", prop->GetName(), i, tokens.size(), i + 1 < tokens.size() ? tokens[i + 1] : string_view {});
            FO_VERIFY_AND_THROW(prop->IsBaseTypeHash(), "Property ScriptFuncType tag can only be applied to hash-backed properties", _typeName, prop->GetName(), prop->_viewTypeName);
            prop->_scriptFuncType = tokens[i + 2];
            i += 2;
        }
        else if (tokens[i] == "Max") {
            FO_VERIFY_AND_THROW(i + 2 < tokens.size() && tokens[i + 1] == "=", "Property Max tag must be followed by '=' and a value", prop->GetName(), i, tokens.size(), i + 1 < tokens.size() ? tokens[i + 1] : string_view {});
            FO_VERIFY_AND_THROW(prop->IsBaseTypeInt() || prop->IsBaseTypeFloat(), "Property Max tag can only be applied to numeric properties", _typeName, prop->GetName(), prop->_viewTypeName);
            i += 2;
        }
        else if (tokens[i] == "Min") {
            FO_VERIFY_AND_THROW(i + 2 < tokens.size() && tokens[i + 1] == "=", "Property Min tag must be followed by '=' and a value", prop->GetName(), i, tokens.size(), i + 1 < tokens.size() ? tokens[i + 1] : string_view {});
            FO_VERIFY_AND_THROW(prop->IsBaseTypeInt() || prop->IsBaseTypeFloat(), "Property Min tag can only be applied to numeric properties", _typeName, prop->GetName(), prop->_viewTypeName);
            i += 2;
        }
        else if (tokens[i] == "Quest") {
            FO_VERIFY_AND_THROW(i + 2 < tokens.size() && tokens[i + 1] == "=", "Property Quest tag must be followed by '=' and a quest identifier", prop->GetName(), i, tokens.size(), i + 1 < tokens.size() ? tokens[i + 1] : string_view {});
            i += 2;
        }
        else if (tokens[i] == "NullGetterForProto") {
            FO_VERIFY_AND_THROW(!prop->_isNullGetterForProto, "Property declaration contains duplicate NullGetterForProto tag", _typeName, prop->GetName(), i);
            prop->_isNullGetterForProto = true;
        }
        else if (tokens[i] == "Nullable") {
            FO_VERIFY_AND_THROW(!prop->_isNullable, "Property declaration contains duplicate Nullable tag", _typeName, prop->GetName(), i);
            prop->_isNullable = true;
        }
        else if (tokens[i] == "SharedProperty") {
            // For internal use, skip
        }
    }

    prop->_isSynced = prop->_isCommon && prop->_isMutable && (prop->_isOwnerSync || prop->_isPublicSync);

    if (is_component_marker) {
        FO_VERIFY_AND_THROW(prop->_componentName.empty(), "Component marker property must not itself be component-qualified", _typeName, prop->GetName(), prop->_componentName);
        FO_VERIFY_AND_THROW(prop->IsBaseTypeBool(), "Component marker property must be backed by bool", _typeName, prop->GetName(), prop->_viewTypeName);
        FO_VERIFY_AND_THROW(prop->IsPlainData(), "Component marker property must be plain data", _typeName, prop->GetName(), prop->_viewTypeName);
        _registeredComponents.emplace(prop->_propName, ptr<const Property> {prop});
    }

    FO_VERIFY_AND_THROW(!(prop->_isOwnerSync || prop->_isPublicSync || prop->_isNoSync) || prop->_isCommon, "Synchronized property is not marked as common");
    FO_VERIFY_AND_THROW(!(prop->_isOwnerSync || prop->_isPublicSync || prop->_isNoSync) || prop->_isMutable, "Synchronized property is not mutable");
    FO_VERIFY_AND_THROW(!(prop->_isOwnerSync || prop->_isPublicSync || prop->_isNoSync) || !prop->_isVirtual, "Virtual property cannot be synchronized directly");
    FO_VERIFY_AND_THROW(!prop->_isNoSync || !(prop->_isOwnerSync || prop->_isPublicSync), "NoSync property also has network sync flags");
    FO_VERIFY_AND_THROW(!prop->_isOwnerSync || !(prop->_isNoSync || prop->_isPublicSync), "OwnerSync property has conflicting sync flags");
    FO_VERIFY_AND_THROW(!prop->_isPublicSync || !(prop->_isNoSync || prop->_isOwnerSync), "PublicSync property has conflicting sync flags");
    FO_VERIFY_AND_THROW(!(prop->_isMutable && prop->_isCommon && !prop->_isVirtual) || (prop->_isOwnerSync || prop->_isPublicSync || prop->_isNoSync), "Mutable common property has no explicit sync mode");
    FO_VERIFY_AND_THROW(!prop->_isModifiableByClient || prop->_isSynced, "Client-modifiable property is not synchronized");
    FO_VERIFY_AND_THROW(!prop->_isModifiableByAnyClient || prop->_isPublicSync, "Any-client-modifiable property is not public synchronized");
    FO_VERIFY_AND_THROW(!prop->_isPersistent || (prop->_isMutable || prop->_isCoreProperty), "Persistent property must be mutable or core");
    FO_VERIFY_AND_THROW(!prop->_isVirtual || !prop->_isSynced, "Virtual property cannot be synchronized");
    FO_VERIFY_AND_THROW(!prop->_isVirtual || !prop->_isPersistent, "Virtual property cannot be persistent");
    FO_VERIFY_AND_THROW(!prop->_isNullGetterForProto || prop->_isVirtual, "Null getter for proto is allowed only on virtual properties");
    FO_VERIFY_AND_THROW(!prop->_isPersistent || !prop->_isClientOnly, "Client-only property cannot be persistent");
    FO_VERIFY_AND_THROW(!prop->_isNullable || prop->IsBaseTypeProtoReference(), "Nullable property must reference a proto type");

    const auto reg_index = numeric_cast<uint16_t>(_registeredProperties.size());

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
        FO_VERIFY_AND_THROW((_wholePodDataSize % 8) == 0, "Property POD storage size lost required 8-byte alignment after registering property", _typeName, prop->GetName(), _wholePodDataSize);
    }

    // Complex property data index
    optional<size_t> complex_data_index;

    if (!prop->IsPlainData() && !disabled && !prop->IsVirtual()) {
        complex_data_index = _complexProperties.size();
        _complexProperties.emplace_back(ptr<Property> {prop});

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

    FO_VERIFY_AND_THROW(_registeredPropertiesLookup.count(prop->_propName) == 0, "Property name is already registered for this registrator", _typeName, prop->_propName, prop->_regIndex);
    _registeredPropertiesLookup.emplace(prop->_propName, ptr<const Property> {prop});

    // Fix plain data data offsets
    if (prop->_podDataOffset.has_value()) {
        if (prop->IsOwnerSync()) {
            *prop->_podDataOffset += _publicPodDataSpace.size();
        }
        else if (!prop->IsSynced()) {
            *prop->_podDataOffset += _publicPodDataSpace.size() + _protectedPodDataSpace.size();
        }
    }

    for (auto& other_prop_owner : _registeredProperties) {
        auto other_prop = other_prop_owner.as_nptr();

        if (!other_prop) {
            continue;
        }

        if (!other_prop->_podDataOffset.has_value() || other_prop == prop.get()) {
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
            FO_STRONG_ASSERT(data_prop.Prop->_podDataOffset.has_value(), "Plain data property has no pod data offset while finalizing registrator", data_prop.Prop->GetName(), _typeName);
            data_prop.DataIndex = numeric_cast<uint32_t>(*data_prop.Prop->_podDataOffset);
        }
    }

    if (!prop->IsDisabled()) {
        if (!prop->IsVirtual()) {
            if (prop->IsPlainData()) {
                FO_STRONG_ASSERT(prop->_podDataOffset.has_value(), "Plain property has no pod data offset while finalizing registrator", prop->GetName(), _typeName);
                _dataProperties.emplace_back(DataPropertyEntry {.Prop = prop, .DataIndex = numeric_cast<uint32_t>(*prop->_podDataOffset), .DataSize = numeric_cast<uint16_t>(prop->GetBaseSize()), .IsPlain = true});
            }
            else {
                FO_STRONG_ASSERT(prop->_complexDataIndex.has_value(), "Complex property has no complex data index while finalizing registrator", prop->GetName(), _typeName);
                _dataProperties.emplace_back(DataPropertyEntry {.Prop = prop, .DataIndex = numeric_cast<uint32_t>(*prop->_complexDataIndex), .DataSize = 0, .IsPlain = false});
            }

            if (!prop->IsTemporary()) {
                _textProperties.emplace_back(ptr<Property> {prop});
            }
        }

        if (prop->IsBaseTypeHash() || prop->IsBaseTypeProtoReference() || prop->IsDictKeyHash()) {
            _hashProperties.emplace_back(ptr<Property> {prop});
        }
    }

    _registeredProperties.emplace_back(std::move(prop));
    return _registeredProperties.back();
}

FO_END_NAMESPACE
