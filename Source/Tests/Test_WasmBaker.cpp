//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#if FO_WASM_SCRIPTING

#include "EngineBase.h"
#include "EntityProtos.h"
#include "ImGuiStuff.h"
#include "Test_BakerHelpers.h"
#include "WasmApiBridge.h"
#include "WasmAssemblyScriptBaker.h"
#include "WasmBackend.h"
#include "WasmBaker.h"
#include "WasmImports.h"
#include "WasmRefHandles.h"

FO_BEGIN_NAMESPACE

static void DummyWasmApiCall(FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(call);
}

struct WasmExportRefLifecycleTestValue
{
    int32_t AddRefCalls {};
    int32_t ReleaseCalls {};
};

static void WasmExportRefLifecycleTestAddRef(FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    nptr<WasmExportRefLifecycleTestValue> value = *cast_from_void<WasmExportRefLifecycleTestValue**>(call.ArgsData.front());
    FO_STRONG_ASSERT(value, "WASM test invariant failed");
    value->AddRefCalls++;
}

static void WasmExportRefLifecycleTestRelease(FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    nptr<WasmExportRefLifecycleTestValue> value = *cast_from_void<WasmExportRefLifecycleTestValue**>(call.ArgsData.front());
    FO_STRONG_ASSERT(value, "WASM test invariant failed");
    value->ReleaseCalls++;
}

struct WasmApiTestHex
{
    int16_t X {};
    int16_t Y {};
};
static_assert(sizeof(WasmApiTestHex) == sizeof(uint32_t));

class WasmExportCallbackTestAccessor final : public DataAccessor
{
public:
    explicit WasmExportCallbackTestAccessor(ptr<ScriptFuncDesc> callback) noexcept :
        _callback {callback}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    [[nodiscard]] auto GetBackendIndex() const noexcept -> int32_t override
    {
        FO_NO_STACK_TRACE_ENTRY();

        return -1;
    }

    [[nodiscard]] auto GetCallback(ptr<void> /*data*/) const -> unique_del_nptr<ScriptFuncDesc> override
    {
        FO_NO_STACK_TRACE_ENTRY();

        return make_unique_del_ptr(ptr<ScriptFuncDesc> {_callback.get_no_const()}, [](auto&&) { });
    }

private:
    ptr<ScriptFuncDesc> _callback;
};

enum class WasmExportRefCollectionTestMode
{
    RefArray,
    StringRefDict,
    StringRefArrayDict,
    RefIntDict,
    RefIntArrayDict,
};

class WasmExportRefCollectionTestAccessor final : public DataAccessor
{
public:
    explicit WasmExportRefCollectionTestAccessor(WasmExportRefCollectionTestMode mode) noexcept :
        _mode {mode}
    {
        FO_NO_STACK_TRACE_ENTRY();
    }

    [[nodiscard]] auto GetBackendIndex() const noexcept -> int32_t override
    {
        FO_NO_STACK_TRACE_ENTRY();

        return -1;
    }

    [[nodiscard]] auto GetArraySize(ptr<void> data) const -> size_t override
    {
        FO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(_mode == WasmExportRefCollectionTestMode::RefArray, "Unexpected WASM export ref collection test mode");
        return cast_from_void<vector<void*>*>(data.get())->size();
    }

    [[nodiscard]] auto GetArrayElement(ptr<void> data, size_t index) const -> ptr<void> override
    {
        FO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(_mode == WasmExportRefCollectionTestMode::RefArray, "Unexpected WASM export ref collection test mode");
        return make_ptr(&(*cast_from_void<vector<void*>*>(data.get()))[index]).void_cast();
    }

    [[nodiscard]] auto GetDictSize(ptr<void> data) const -> size_t override
    {
        FO_STACK_TRACE_ENTRY();

        switch (_mode) {
        case WasmExportRefCollectionTestMode::StringRefDict:
            return cast_from_void<map<string, void*>*>(data.get())->size();
        case WasmExportRefCollectionTestMode::StringRefArrayDict:
            return cast_from_void<map<string, vector<void*>>*>(data.get())->size();
        case WasmExportRefCollectionTestMode::RefIntDict:
            return cast_from_void<map<void*, int32_t>*>(data.get())->size();
        case WasmExportRefCollectionTestMode::RefIntArrayDict:
            return cast_from_void<map<void*, vector<int32_t>>*>(data.get())->size();
        default:
            break;
        }

        throw InvalidCallException(FO_LINE_STR);
    }

    [[nodiscard]] auto GetDictElement(ptr<void> data, size_t index) const -> pair<ptr<void>, ptr<void>> override
    {
        FO_STACK_TRACE_ENTRY();

        switch (_mode) {
        case WasmExportRefCollectionTestMode::StringRefDict: {
            auto& values = *cast_from_void<map<string, void*>*>(data.get());
            auto it = std::next(values.begin(), numeric_cast<ptrdiff_t>(index));
            return {make_ptr(const_cast<string*>(&it->first)).void_cast(), make_ptr(&it->second).void_cast()};
        }
        case WasmExportRefCollectionTestMode::StringRefArrayDict: {
            auto& values = *cast_from_void<map<string, vector<void*>>*>(data.get());
            auto it = std::next(values.begin(), numeric_cast<ptrdiff_t>(index));
            return {make_ptr(const_cast<string*>(&it->first)).void_cast(), make_ptr(&it->second).void_cast()};
        }
        case WasmExportRefCollectionTestMode::RefIntDict: {
            auto& values = *cast_from_void<map<void*, int32_t>*>(data.get());
            auto it = std::next(values.begin(), numeric_cast<ptrdiff_t>(index));
            return {make_ptr(const_cast<void**>(&it->first)).void_cast(), make_ptr(&it->second).void_cast()};
        }
        case WasmExportRefCollectionTestMode::RefIntArrayDict: {
            auto& values = *cast_from_void<map<void*, vector<int32_t>>*>(data.get());
            auto it = std::next(values.begin(), numeric_cast<ptrdiff_t>(index));
            return {make_ptr(const_cast<void**>(&it->first)).void_cast(), make_ptr(&it->second).void_cast()};
        }
        default:
            break;
        }

        throw InvalidCallException(FO_LINE_STR);
    }

    [[nodiscard]] auto GetNestedArraySize(const BaseTypeDesc& /*element_type*/, ptr<void> data) const -> size_t override
    {
        FO_STACK_TRACE_ENTRY();

        switch (_mode) {
        case WasmExportRefCollectionTestMode::StringRefArrayDict:
            return cast_from_void<vector<void*>*>(data.get())->size();
        case WasmExportRefCollectionTestMode::RefIntArrayDict:
            return cast_from_void<vector<int32_t>*>(data.get())->size();
        default:
            break;
        }

        throw InvalidCallException(FO_LINE_STR);
    }

    [[nodiscard]] auto GetNestedArrayElement(const BaseTypeDesc& /*element_type*/, ptr<void> data, size_t index) const -> ptr<void> override
    {
        FO_STACK_TRACE_ENTRY();

        switch (_mode) {
        case WasmExportRefCollectionTestMode::StringRefArrayDict:
            return make_ptr(&(*cast_from_void<vector<void*>*>(data.get()))[index]).void_cast();
        case WasmExportRefCollectionTestMode::RefIntArrayDict:
            return make_ptr(&(*cast_from_void<vector<int32_t>*>(data.get()))[index]).void_cast();
        default:
            break;
        }

        throw InvalidCallException(FO_LINE_STR);
    }

    void ClearArray(ptr<void> data) const override
    {
        FO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(_mode == WasmExportRefCollectionTestMode::RefArray, "Unexpected WASM export ref collection test mode");
        cast_from_void<vector<void*>*>(data.get())->clear();
    }

    void AddArrayElement(ptr<void> data, ptr<void> value) const override
    {
        FO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(_mode == WasmExportRefCollectionTestMode::RefArray, "Unexpected WASM export ref collection test mode");
        cast_from_void<vector<void*>*>(data.get())->emplace_back(*value.reinterpret_as<void*>());
    }

    void ClearDict(ptr<void> data) const override
    {
        FO_STACK_TRACE_ENTRY();

        switch (_mode) {
        case WasmExportRefCollectionTestMode::StringRefDict:
            cast_from_void<map<string, void*>*>(data.get())->clear();
            return;
        case WasmExportRefCollectionTestMode::StringRefArrayDict:
            cast_from_void<map<string, vector<void*>>*>(data.get())->clear();
            return;
        case WasmExportRefCollectionTestMode::RefIntDict:
            cast_from_void<map<void*, int32_t>*>(data.get())->clear();
            return;
        case WasmExportRefCollectionTestMode::RefIntArrayDict:
            cast_from_void<map<void*, vector<int32_t>>*>(data.get())->clear();
            return;
        default:
            break;
        }

        throw InvalidCallException(FO_LINE_STR);
    }

    void AddDictElement(ptr<void> data, ptr<void> key, ptr<void> value) const override
    {
        FO_STACK_TRACE_ENTRY();

        switch (_mode) {
        case WasmExportRefCollectionTestMode::StringRefDict:
            cast_from_void<map<string, void*>*>(data.get())->emplace(*key.reinterpret_as<string>(), *value.reinterpret_as<void*>());
            return;
        case WasmExportRefCollectionTestMode::RefIntDict:
            cast_from_void<map<void*, int32_t>*>(data.get())->emplace(*key.reinterpret_as<void*>(), *value.reinterpret_as<int32_t>());
            return;
        default:
            break;
        }

        throw InvalidCallException(FO_LINE_STR);
    }

    void AddDictArrayElement(ptr<void> data, ptr<void> key, const BaseTypeDesc& /*element_type*/, const_span<ptr<void>> values) const override
    {
        FO_STACK_TRACE_ENTRY();

        switch (_mode) {
        case WasmExportRefCollectionTestMode::StringRefArrayDict: {
            vector<void*> items;
            items.reserve(values.size());

            for (ptr<void> value : values) {
                items.emplace_back(*value.reinterpret_as<void*>());
            }

            cast_from_void<map<string, vector<void*>>*>(data.get())->emplace(*key.reinterpret_as<string>(), std::move(items));
            return;
        }
        case WasmExportRefCollectionTestMode::RefIntArrayDict: {
            vector<int32_t> items;
            items.reserve(values.size());

            for (ptr<void> value : values) {
                items.emplace_back(*value.reinterpret_as<int32_t>());
            }

            cast_from_void<map<void*, vector<int32_t>>*>(data.get())->emplace(*key.reinterpret_as<void*>(), std::move(items));
            return;
        }
        default:
            break;
        }

        throw InvalidCallException(FO_LINE_STR);
    }

private:
    WasmExportRefCollectionTestMode _mode {};
};

template<typename T>
static auto ReadWasmApiTestValue(ptr<const void> data) -> T
{
    FO_STACK_TRACE_ENTRY();

    T value {};
    MemCopy(&value, data, sizeof(value));
    return value;
}

template<typename T>
static void WriteWasmApiTestValue(ptr<void> data, const T& value)
{
    FO_STACK_TRACE_ENTRY();

    MemCopy(data, &value, sizeof(value));
}

static auto PackWasmApiTestHex(const WasmApiTestHex& value) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    uint32_t raw_value = 0;
    MemCopy(&raw_value, &value, sizeof(value));
    return raw_value;
}

static auto UnpackWasmApiTestHex(uint64_t raw_value) -> WasmApiTestHex
{
    FO_STACK_TRACE_ENTRY();

    const uint32_t packed_value = numeric_cast<uint32_t>(raw_value);
    WasmApiTestHex value {};
    MemCopy(&value, &packed_value, sizeof(value));
    return value;
}

static void AppendWasmApiTestU32(vector<uint8_t>& data, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    const size_t old_size = data.size();
    data.resize(old_size + sizeof(value));
    MemCopy(data.data() + old_size, &value, sizeof(value));
}

static auto ReadWasmApiTestU32(const vector<uint8_t>& data, size_t& offset) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(offset + sizeof(uint32_t) <= data.size());

    uint32_t value = 0;
    MemCopy(&value, data.data() + offset, sizeof(value));
    offset += sizeof(value);
    return value;
}

static auto MakeWasmApiTestStringArrayBlob(std::initializer_list<string_view> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const string_view value : values) {
        AppendWasmApiTestU32(data, numeric_cast<uint32_t>(value.size()));
        data.insert(data.end(), value.begin(), value.end());
    }

    return data;
}

static auto ReadWasmApiTestStringArrayBlob(const vector<uint8_t>& data) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);
    values.reserve(count);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        const uint32_t size = ReadWasmApiTestU32(data, offset);
        REQUIRE(offset + size <= data.size());
        values.emplace_back(reinterpret_cast<const char*>(data.data() + offset), numeric_cast<size_t>(size));
        offset += size;
    }

    REQUIRE(offset == data.size());
    return values;
}

static void AppendWasmApiTestString(vector<uint8_t>& data, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(value.size()));
    data.insert(data.end(), value.begin(), value.end());
}

static auto ReadWasmApiTestString(const vector<uint8_t>& data, size_t& offset) -> string
{
    FO_STACK_TRACE_ENTRY();

    const uint32_t size = ReadWasmApiTestU32(data, offset);
    REQUIRE(offset + size <= data.size());

    string value {reinterpret_cast<const char*>(data.data() + offset), numeric_cast<size_t>(size)};
    offset += size;
    return value;
}

static auto MakeWasmApiTestStringDictBlob(std::initializer_list<pair<string_view, string_view>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, value] : values) {
        AppendWasmApiTestString(data, key);
        AppendWasmApiTestString(data, value);
    }

    return data;
}

static auto ReadWasmApiTestStringDictBlob(const vector<uint8_t>& data) -> map<string, string>
{
    FO_STACK_TRACE_ENTRY();

    map<string, string> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        string key = ReadWasmApiTestString(data, offset);
        string value = ReadWasmApiTestString(data, offset);
        values.emplace(std::move(key), std::move(value));
    }

    REQUIRE(offset == data.size());
    return values;
}

static void AppendWasmApiTestI32(vector<uint8_t>& data, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    const size_t old_size = data.size();
    data.resize(old_size + sizeof(value));
    MemCopy(data.data() + old_size, &value, sizeof(value));
}

static auto ReadWasmApiTestI32(const vector<uint8_t>& data, size_t& offset) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(offset + sizeof(int32_t) <= data.size());

    int32_t value = 0;
    MemCopy(&value, data.data() + offset, sizeof(value));
    offset += sizeof(value);
    return value;
}

static void AppendWasmApiTestU64(vector<uint8_t>& data, uint64_t value)
{
    FO_STACK_TRACE_ENTRY();

    const size_t old_size = data.size();
    data.resize(old_size + sizeof(value));
    MemCopy(data.data() + old_size, &value, sizeof(value));
}

static auto ReadWasmApiTestU64(const vector<uint8_t>& data, size_t& offset) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(offset + sizeof(uint64_t) <= data.size());

    uint64_t value = 0;
    MemCopy(&value, data.data() + offset, sizeof(value));
    offset += sizeof(value);
    return value;
}

static auto MakeWasmApiTestU64ArrayBlob(std::initializer_list<uint64_t> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;

    for (const uint64_t value : values) {
        AppendWasmApiTestU64(data, value);
    }

    return data;
}

static auto ReadWasmApiTestU64ArrayBlob(const vector<uint8_t>& data) -> vector<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(data.size() % sizeof(uint64_t) == 0);

    vector<uint64_t> values;
    size_t offset = 0;
    values.reserve(data.size() / sizeof(uint64_t));

    while (offset != data.size()) {
        values.emplace_back(ReadWasmApiTestU64(data, offset));
    }

    return values;
}

static auto MakeWasmApiTestStringU64DictBlob(std::initializer_list<pair<string_view, uint64_t>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, value] : values) {
        AppendWasmApiTestString(data, key);
        AppendWasmApiTestU64(data, value);
    }

    return data;
}

static auto ReadWasmApiTestStringU64DictBlob(const vector<uint8_t>& data) -> map<string, uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    map<string, uint64_t> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        string key = ReadWasmApiTestString(data, offset);
        uint64_t value = ReadWasmApiTestU64(data, offset);
        values.emplace(std::move(key), value);
    }

    REQUIRE(offset == data.size());
    return values;
}

static auto MakeWasmApiTestU64IntDictBlob(std::initializer_list<pair<uint64_t, int32_t>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, value] : values) {
        AppendWasmApiTestU64(data, key);
        AppendWasmApiTestI32(data, value);
    }

    return data;
}

static auto ReadWasmApiTestU64IntDictBlob(const vector<uint8_t>& data) -> map<uint64_t, int32_t>
{
    FO_STACK_TRACE_ENTRY();

    map<uint64_t, int32_t> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        const uint64_t key = ReadWasmApiTestU64(data, offset);
        const int32_t value = ReadWasmApiTestI32(data, offset);
        values.emplace(key, value);
    }

    REQUIRE(offset == data.size());
    return values;
}

static auto MakeWasmApiTestStringIntArrayDictBlob(std::initializer_list<pair<string_view, vector<int32_t>>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, items] : values) {
        AppendWasmApiTestString(data, key);
        AppendWasmApiTestU32(data, numeric_cast<uint32_t>(items.size()));

        for (const int32_t item : items) {
            AppendWasmApiTestI32(data, item);
        }
    }

    return data;
}

static auto ReadWasmApiTestStringIntArrayDictBlob(const vector<uint8_t>& data) -> map<string, vector<int32_t>>
{
    FO_STACK_TRACE_ENTRY();

    map<string, vector<int32_t>> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        string key = ReadWasmApiTestString(data, offset);
        const uint32_t items_count = ReadWasmApiTestU32(data, offset);
        vector<int32_t> items;
        items.reserve(items_count);

        for (uint32_t item_index = 0; item_index < items_count; item_index++) {
            items.emplace_back(ReadWasmApiTestI32(data, offset));
        }

        values.emplace(std::move(key), std::move(items));
    }

    REQUIRE(offset == data.size());
    return values;
}

struct WasmApiTestRect
{
    int32_t X {};
    int32_t Y {};
    int32_t Width {};
    int32_t Height {};

    [[nodiscard]] auto operator==(const WasmApiTestRect&) const noexcept -> bool = default;
};
static_assert(sizeof(WasmApiTestRect) == 16);

static void AppendWasmApiTestRect(vector<uint8_t>& data, const WasmApiTestRect& value)
{
    FO_STACK_TRACE_ENTRY();

    AppendWasmApiTestI32(data, value.X);
    AppendWasmApiTestI32(data, value.Y);
    AppendWasmApiTestI32(data, value.Width);
    AppendWasmApiTestI32(data, value.Height);
}

static auto ReadWasmApiTestRect(const vector<uint8_t>& data, size_t& offset) -> WasmApiTestRect
{
    FO_STACK_TRACE_ENTRY();

    return WasmApiTestRect {
        .X = ReadWasmApiTestI32(data, offset),
        .Y = ReadWasmApiTestI32(data, offset),
        .Width = ReadWasmApiTestI32(data, offset),
        .Height = ReadWasmApiTestI32(data, offset),
    };
}

static auto LoadWasmApiTestRect(ptr<const void> data) -> WasmApiTestRect
{
    FO_STACK_TRACE_ENTRY();

    WasmApiTestRect value {};
    MemCopy(&value, data, sizeof(value));
    return value;
}

static auto MakeWasmApiTestRectArrayBlob(std::initializer_list<WasmApiTestRect> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;

    for (const WasmApiTestRect& value : values) {
        AppendWasmApiTestRect(data, value);
    }

    return data;
}

static auto ReadWasmApiTestRectArrayBlob(const vector<uint8_t>& data) -> vector<WasmApiTestRect>
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(data.size() % sizeof(WasmApiTestRect) == 0);

    vector<WasmApiTestRect> values;
    size_t offset = 0;
    values.reserve(data.size() / sizeof(WasmApiTestRect));

    while (offset != data.size()) {
        values.emplace_back(ReadWasmApiTestRect(data, offset));
    }

    return values;
}

static auto MakeWasmApiTestStringRectArrayDictBlob(std::initializer_list<pair<string_view, vector<WasmApiTestRect>>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, items] : values) {
        AppendWasmApiTestString(data, key);
        AppendWasmApiTestU32(data, numeric_cast<uint32_t>(items.size()));

        for (const WasmApiTestRect& item : items) {
            AppendWasmApiTestRect(data, item);
        }
    }

    return data;
}

static auto ReadWasmApiTestStringRectArrayDictBlob(const vector<uint8_t>& data) -> map<string, vector<WasmApiTestRect>>
{
    FO_STACK_TRACE_ENTRY();

    map<string, vector<WasmApiTestRect>> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        string key = ReadWasmApiTestString(data, offset);
        const uint32_t items_count = ReadWasmApiTestU32(data, offset);
        vector<WasmApiTestRect> items;
        items.reserve(items_count);

        for (uint32_t item_index = 0; item_index < items_count; item_index++) {
            items.emplace_back(ReadWasmApiTestRect(data, offset));
        }

        values.emplace(std::move(key), std::move(items));
    }

    REQUIRE(offset == data.size());
    return values;
}

static auto MakeWasmApiTestU64IntArrayDictBlob(std::initializer_list<pair<uint64_t, vector<int32_t>>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, items] : values) {
        AppendWasmApiTestU64(data, key);
        AppendWasmApiTestU32(data, numeric_cast<uint32_t>(items.size()));

        for (const int32_t item : items) {
            AppendWasmApiTestI32(data, item);
        }
    }

    return data;
}

static auto ReadWasmApiTestU64IntArrayDictBlob(const vector<uint8_t>& data) -> map<uint64_t, vector<int32_t>>
{
    FO_STACK_TRACE_ENTRY();

    map<uint64_t, vector<int32_t>> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        const uint64_t key = ReadWasmApiTestU64(data, offset);
        const uint32_t items_count = ReadWasmApiTestU32(data, offset);
        vector<int32_t> items;
        items.reserve(items_count);

        for (uint32_t item_index = 0; item_index < items_count; item_index++) {
            items.emplace_back(ReadWasmApiTestI32(data, offset));
        }

        values.emplace(key, std::move(items));
    }

    REQUIRE(offset == data.size());
    return values;
}

static auto MakeWasmApiTestStringBoolArrayDictBlob(std::initializer_list<pair<string_view, vector<bool>>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, items] : values) {
        AppendWasmApiTestString(data, key);
        AppendWasmApiTestU32(data, numeric_cast<uint32_t>(items.size()));

        for (const bool item : items) {
            data.emplace_back(item ? 1 : 0);
        }
    }

    return data;
}

static auto ReadWasmApiTestStringBoolArrayDictBlob(const vector<uint8_t>& data) -> map<string, vector<bool>>
{
    FO_STACK_TRACE_ENTRY();

    map<string, vector<bool>> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        string key = ReadWasmApiTestString(data, offset);
        const uint32_t items_count = ReadWasmApiTestU32(data, offset);
        vector<bool> items;
        items.reserve(items_count);

        for (uint32_t item_index = 0; item_index < items_count; item_index++) {
            REQUIRE(offset < data.size());
            items.emplace_back(data[offset] != 0);
            offset++;
        }

        values.emplace(std::move(key), std::move(items));
    }

    REQUIRE(offset == data.size());
    return values;
}

static auto MakeWasmApiTestStringU32ArrayDictBlob(std::initializer_list<pair<string_view, vector<uint32_t>>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, items] : values) {
        AppendWasmApiTestString(data, key);
        AppendWasmApiTestU32(data, numeric_cast<uint32_t>(items.size()));

        for (const uint32_t item : items) {
            AppendWasmApiTestU32(data, item);
        }
    }

    return data;
}

static auto ReadWasmApiTestStringU32ArrayDictBlob(const vector<uint8_t>& data) -> map<string, vector<uint32_t>>
{
    FO_STACK_TRACE_ENTRY();

    map<string, vector<uint32_t>> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        string key = ReadWasmApiTestString(data, offset);
        const uint32_t items_count = ReadWasmApiTestU32(data, offset);
        vector<uint32_t> items;
        items.reserve(items_count);

        for (uint32_t item_index = 0; item_index < items_count; item_index++) {
            items.emplace_back(ReadWasmApiTestU32(data, offset));
        }

        values.emplace(std::move(key), std::move(items));
    }

    REQUIRE(offset == data.size());
    return values;
}

static auto MakeWasmApiTestStringU64ArrayDictBlob(std::initializer_list<pair<string_view, vector<uint64_t>>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, items] : values) {
        AppendWasmApiTestString(data, key);
        AppendWasmApiTestU32(data, numeric_cast<uint32_t>(items.size()));

        for (const uint64_t item : items) {
            AppendWasmApiTestU64(data, item);
        }
    }

    return data;
}

static auto ReadWasmApiTestStringU64ArrayDictBlob(const vector<uint8_t>& data) -> map<string, vector<uint64_t>>
{
    FO_STACK_TRACE_ENTRY();

    map<string, vector<uint64_t>> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        string key = ReadWasmApiTestString(data, offset);
        const uint32_t items_count = ReadWasmApiTestU32(data, offset);
        vector<uint64_t> items;
        items.reserve(items_count);

        for (uint32_t item_index = 0; item_index < items_count; item_index++) {
            items.emplace_back(ReadWasmApiTestU64(data, offset));
        }

        values.emplace(std::move(key), std::move(items));
    }

    REQUIRE(offset == data.size());
    return values;
}

static auto MakeWasmApiTestStringStringArrayDictBlob(std::initializer_list<pair<string_view, vector<string>>> values) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> data;
    AppendWasmApiTestU32(data, numeric_cast<uint32_t>(values.size()));

    for (const auto& [key, items] : values) {
        AppendWasmApiTestString(data, key);
        AppendWasmApiTestU32(data, numeric_cast<uint32_t>(items.size()));

        for (const string& item : items) {
            AppendWasmApiTestString(data, item);
        }
    }

    return data;
}

static auto ReadWasmApiTestStringStringArrayDictBlob(const vector<uint8_t>& data) -> map<string, vector<string>>
{
    FO_STACK_TRACE_ENTRY();

    map<string, vector<string>> values;
    size_t offset = 0;
    const uint32_t count = ReadWasmApiTestU32(data, offset);

    for (uint32_t value_index = 0; value_index < count; value_index++) {
        string key = ReadWasmApiTestString(data, offset);
        const uint32_t items_count = ReadWasmApiTestU32(data, offset);
        vector<string> items;
        items.reserve(items_count);

        for (uint32_t item_index = 0; item_index < items_count; item_index++) {
            items.emplace_back(ReadWasmApiTestString(data, offset));
        }

        values.emplace(std::move(key), std::move(items));
    }

    REQUIRE(offset == data.size());
    return values;
}

class WasmApiTestEngine final : public BaseEngine
{
public:
    using MetadataRegistrar = function<void(EngineMetadata&)>;

    WasmApiTestEngine(GlobalSettings& settings, MetadataRegistrar registrar) :
        BaseEngine(&settings, FileSystem {}, [this, registrar = std::move(registrar)] { registrar(*this); })
    {
        FO_STACK_TRACE_ENTRY();
    }

    void AddResolvedEntity(ptr<Entity> entity)
    {
        FO_STACK_TRACE_ENTRY();

        _entities.emplace(entity->GetId(), entity);
    }

    auto ResolveScriptEntityHandle(string_view entity_type_name, ident_t id) -> nptr<Entity> override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(entity_type_name);

        const auto it = _entities.find(id);
        return it != _entities.end() ? it->second : nullptr;
    }

private:
    unordered_map<ident_t, nptr<Entity>> _entities {};
};

class WasmApiTestEntity final : public Entity
{
public:
    WasmApiTestEntity(ptr<const PropertyRegistrar> registrar, ident_t id) noexcept :
        Entity(registrar, nullptr, nullptr),
        _id {id}
    {
        FO_STACK_TRACE_ENTRY();
    }

    [[nodiscard]] auto GetName() const noexcept -> string_view override
    {
        FO_NO_STACK_TRACE_ENTRY();

        return "WasmApiTestEntity";
    }

    [[nodiscard]] auto GetId() const noexcept -> ident_t override
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _id;
    }

private:
    ident_t _id {};
};

static auto MakeMinimalWasmBytes() -> string
{
    FO_STACK_TRACE_ENTRY();

    return string {"\0asm\1\0\0\0", 8};
}

#if !FO_WEB
static void AppendRuntimeWasmUleb(vector<uint8_t>& data, uint32_t value)
{
    FO_STACK_TRACE_ENTRY();

    do {
        uint8_t byte = numeric_cast<uint8_t>(value & 0x7F);
        value >>= 7;

        if (value != 0) {
            byte |= 0x80;
        }

        data.emplace_back(byte);
    } while (value != 0);
}

static void AppendRuntimeWasmSleb(vector<uint8_t>& data, int64_t value)
{
    FO_STACK_TRACE_ENTRY();

    bool more = true;

    while (more) {
        uint8_t byte = numeric_cast<uint8_t>(value & 0x7F);
        value >>= 7;

        const bool sign_bit = (byte & 0x40) != 0;
        more = !((value == 0 && !sign_bit) || (value == -1 && sign_bit));

        if (more) {
            byte |= 0x80;
        }

        data.emplace_back(byte);
    }
}

static void AppendRuntimeWasmName(vector<uint8_t>& data, string_view name)
{
    FO_STACK_TRACE_ENTRY();

    AppendRuntimeWasmUleb(data, numeric_cast<uint32_t>(name.size()));
    data.insert(data.end(), name.begin(), name.end());
}

static void AppendRuntimeWasmSection(vector<uint8_t>& module, uint8_t section_id, const vector<uint8_t>& payload)
{
    FO_STACK_TRACE_ENTRY();

    module.emplace_back(section_id);
    AppendRuntimeWasmUleb(module, numeric_cast<uint32_t>(payload.size()));
    module.insert(module.end(), payload.begin(), payload.end());
}

static void AppendRuntimeWasmFuncBody(vector<uint8_t>& code_section, std::initializer_list<uint8_t> body)
{
    FO_STACK_TRACE_ENTRY();

    AppendRuntimeWasmUleb(code_section, numeric_cast<uint32_t>(body.size()));
    code_section.insert(code_section.end(), body.begin(), body.end());
}

static void AppendRuntimeWasmFuncBody(vector<uint8_t>& code_section, const vector<uint8_t>& body)
{
    FO_STACK_TRACE_ENTRY();

    AppendRuntimeWasmUleb(code_section, numeric_cast<uint32_t>(body.size()));
    code_section.insert(code_section.end(), body.begin(), body.end());
}

static auto PackRuntimeWasmTextResult(uint32_t ptr, uint32_t len) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return (uint64_t {len} << 32) | ptr;
}

static void AppendRuntimeWasmI64ConstFuncBody(vector<uint8_t>& code_section, uint64_t value)
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> body;
    body.emplace_back(0x00); // local decl count
    body.emplace_back(0x42); // i64.const
    AppendRuntimeWasmSleb(body, std::bit_cast<int64_t>(value));
    body.emplace_back(0x0B); // end
    AppendRuntimeWasmFuncBody(code_section, body);
}

static void AppendRuntimeWasmI64StoreConstFuncBody(vector<uint8_t>& code_section, uint64_t value)
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8_t> body;
    body.emplace_back(0x00); // local decl count
    body.emplace_back(0x20); // local.get
    body.emplace_back(0x00); // local 0
    body.emplace_back(0x42); // i64.const
    AppendRuntimeWasmSleb(body, std::bit_cast<int64_t>(value));
    body.emplace_back(0x37); // i64.store
    body.emplace_back(0x03); // alignment
    body.emplace_back(0x00); // offset
    body.emplace_back(0x0B); // end
    AppendRuntimeWasmFuncBody(code_section, body);
}

static auto MakeRuntimeWasmBytes() -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    constexpr uint32_t TEXT_RESULT_OFFSET = 1024;
    constexpr uint32_t ANY_RESULT_OFFSET = 1056;
    constexpr uint32_t BYTE_ARRAY_RESULT_OFFSET = 1088;
    constexpr uint32_t TEXT_ARRAY_RESULT_OFFSET = 1120;
    constexpr uint32_t DICT_RESULT_OFFSET = 1168;
    constexpr uint32_t GROUPS_RESULT_OFFSET = 1216;
    constexpr uint32_t RULE_GROUPS_RESULT_OFFSET = 1280;
    constexpr uint32_t FLAGS_RESULT_OFFSET = 1344;
    constexpr uint32_t COLOR_GROUPS_RESULT_OFFSET = 1376;
    constexpr uint32_t REF_ARRAY_RESULT_OFFSET = 1424;
    constexpr uint32_t REF_MAP_RESULT_OFFSET = 1456;
    constexpr uint32_t REF_GROUPS_RESULT_OFFSET = 1504;
    constexpr uint32_t REF_KEY_MAP_RESULT_OFFSET = 1568;
    constexpr uint32_t REF_KEY_GROUPS_RESULT_OFFSET = 1616;
    constexpr uint32_t RECT_GROUPS_RESULT_OFFSET = 1664;
    constexpr uint32_t RECT_RESULT_OFFSET = 1728;
    const string_view text_result = "hello wasm result";
    const string_view any_result = "hello wasm any";
    const vector<uint8_t> byte_array_result = {4, 5, 6};
    const vector<uint8_t> text_array_result = MakeWasmApiTestStringArrayBlob({"red", "green"});
    const vector<uint8_t> dict_result = MakeWasmApiTestStringDictBlob({{"alpha", "1"}, {"beta", "22"}});
    const vector<uint8_t> groups_result = MakeWasmApiTestStringIntArrayDictBlob({{"alpha", {1, 2}}, {"beta", {3, 4, 5}}});
    const vector<uint8_t> rule_groups_result = MakeWasmApiTestStringU64ArrayDictBlob({{"none", {0}}});
    const vector<uint8_t> flags_result = MakeWasmApiTestStringBoolArrayDictBlob({{"none", {false, true}}});
    const vector<uint8_t> color_groups_result = MakeWasmApiTestStringU32ArrayDictBlob({{"colors", {0x11223344, 0x55667788}}});
    const vector<uint8_t> ref_array_result = MakeWasmApiTestU64ArrayBlob({0xCAFE, 0xF00D});
    const vector<uint8_t> ref_map_result = MakeWasmApiTestStringU64DictBlob({{"first", 0x1111}, {"second", 0x2222}});
    const vector<uint8_t> ref_groups_result = MakeWasmApiTestStringU64ArrayDictBlob({{"refs", {0x3333, 0x4444}}});
    const vector<uint8_t> ref_key_map_result = MakeWasmApiTestU64IntDictBlob({{0x5555, 7}, {0x6666, 9}});
    const vector<uint8_t> ref_key_groups_result = MakeWasmApiTestU64IntArrayDictBlob({{0x7777, {1, 2}}, {0x8888, {3}}});
    const vector<uint8_t> rect_groups_result = MakeWasmApiTestStringRectArrayDictBlob({{"rects", {WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = -1, .Y = 2, .Width = -3, .Height = 4}}}});
    const vector<uint8_t> rect_result = MakeWasmApiTestRectArrayBlob({WasmApiTestRect {.X = 5, .Y = 6, .Width = 7, .Height = 8}});

    vector<uint8_t> module {
        0x00, 0x61, 0x73, 0x6D, // magic
        0x01, 0x00, 0x00, 0x00, // version
    };

    vector<uint8_t> type_section;
    AppendRuntimeWasmUleb(type_section, 7);
    type_section.insert(type_section.end(), {0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F}); // (i32, i32) -> i32
    type_section.insert(type_section.end(), {0x60, 0x00, 0x01, 0x7F}); // () -> i32
    type_section.insert(type_section.end(), {0x60, 0x01, 0x7F, 0x01, 0x7F}); // (i32) -> i32
    type_section.insert(type_section.end(), {0x60, 0x01, 0x7E, 0x01, 0x7E}); // (i64) -> i64
    type_section.insert(type_section.end(), {0x60, 0x00, 0x01, 0x7E}); // () -> i64
    type_section.insert(type_section.end(), {0x60, 0x02, 0x7F, 0x7F, 0x00}); // (i32, i32) -> void
    type_section.insert(type_section.end(), {0x60, 0x04, 0x7F, 0x7F, 0x7F, 0x7F, 0x00}); // (i32, i32, i32, i32) -> void
    AppendRuntimeWasmSection(module, 1, type_section);

    vector<uint8_t> import_section;
    AppendRuntimeWasmUleb(import_section, 1);
    AppendRuntimeWasmName(import_section, "fonline");
    AppendRuntimeWasmName(import_section, "get_side");
    import_section.emplace_back(0x00); // func import
    AppendRuntimeWasmUleb(import_section, 1); // type index
    AppendRuntimeWasmSection(module, 2, import_section);

    vector<uint8_t> function_section;
    AppendRuntimeWasmUleb(function_section, 61);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 1);
    AppendRuntimeWasmUleb(function_section, 2);
    AppendRuntimeWasmUleb(function_section, 2);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 3);
    AppendRuntimeWasmUleb(function_section, 3);
    AppendRuntimeWasmUleb(function_section, 3);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 5);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 5);
    AppendRuntimeWasmUleb(function_section, 5);
    AppendRuntimeWasmUleb(function_section, 5);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 3);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 5);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 6);
    AppendRuntimeWasmUleb(function_section, 0);
    AppendRuntimeWasmUleb(function_section, 4);
    AppendRuntimeWasmUleb(function_section, 5);
    AppendRuntimeWasmUleb(function_section, 5);
    AppendRuntimeWasmSection(module, 3, function_section);

    vector<uint8_t> memory_section;
    AppendRuntimeWasmUleb(memory_section, 1);
    memory_section.emplace_back(0x00); // min-only limits
    AppendRuntimeWasmUleb(memory_section, 1);
    AppendRuntimeWasmSection(module, 5, memory_section);

    vector<uint8_t> export_section;
    AppendRuntimeWasmUleb(export_section, 62);
    AppendRuntimeWasmName(export_section, "memory");
    export_section.emplace_back(0x02);
    AppendRuntimeWasmUleb(export_section, 0);
    AppendRuntimeWasmName(export_section, "add");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 1);
    AppendRuntimeWasmName(export_section, "read_side");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 2);
    AppendRuntimeWasmName(export_section, "bump_mode__TestMode__TestMode");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 3);
    AppendRuntimeWasmName(export_section, "bump_color__ucolor__ucolor");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 4);
    AppendRuntimeWasmName(export_section, "text_len__string__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 5);
    AppendRuntimeWasmName(export_section, "echo_critter__Critter__Critter");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 6);
    AppendRuntimeWasmName(export_section, "echo_proto__ProtoItem__ProtoItem");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 7);
    AppendRuntimeWasmName(export_section, "echo_rule__Rule__Rule");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 8);
    AppendRuntimeWasmName(export_section, "get_text__void__string");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 9);
    AppendRuntimeWasmName(export_section, "get_any__void__any");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 10);
    AppendRuntimeWasmName(export_section, "sum_pair__int32_array__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 11);
    AppendRuntimeWasmName(export_section, "text_list_size__string_array__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 12);
    AppendRuntimeWasmName(export_section, "get_bytes__void__uint8_array");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 13);
    AppendRuntimeWasmName(export_section, "get_text_list__void__string_array");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 14);
    AppendRuntimeWasmName(export_section, "dict_size__string_string_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 15);
    AppendRuntimeWasmName(export_section, "get_config__void__string_string_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 16);
    AppendRuntimeWasmName(export_section, "bump_ref__int32_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 17);
    AppendRuntimeWasmName(export_section, "mutate_values__int32_array_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 18);
    AppendRuntimeWasmName(export_section, "mutate_config__string_string_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 19);
    AppendRuntimeWasmName(export_section, "mutate_text__string_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 20);
    AppendRuntimeWasmName(export_section, "callback_name_len__callback_int32_int32_callback__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 21);
    AppendRuntimeWasmName(export_section, "groups_size__string_int32_array_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 22);
    AppendRuntimeWasmName(export_section, "get_groups__void__string_int32_array_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 23);
    AppendRuntimeWasmName(export_section, "mutate_groups__string_int32_array_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 24);
    AppendRuntimeWasmName(export_section, "clear_critter_ref__Critter_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 25);
    AppendRuntimeWasmName(export_section, "clear_proto_ref__ProtoItem_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 26);
    AppendRuntimeWasmName(export_section, "clear_rule_ref__Rule_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 27);
    AppendRuntimeWasmName(export_section, "rule_groups_size__string_Rule_array_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 28);
    AppendRuntimeWasmName(export_section, "get_rule_groups__void__string_Rule_array_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 29);
    AppendRuntimeWasmName(export_section, "mutate_rule_groups__string_Rule_array_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 30);
    AppendRuntimeWasmName(export_section, "flags_size__string_bool_array_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 31);
    AppendRuntimeWasmName(export_section, "get_flags__void__string_bool_array_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 32);
    AppendRuntimeWasmName(export_section, "mutate_flags__string_bool_array_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 33);
    AppendRuntimeWasmName(export_section, "color_groups_size__string_ucolor_array_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 34);
    AppendRuntimeWasmName(export_section, "get_color_groups__void__string_ucolor_array_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 35);
    AppendRuntimeWasmName(export_section, "mutate_color_groups__string_ucolor_array_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 36);
    AppendRuntimeWasmName(export_section, "nested_callback_name_len__callback_void_callback_int32_int32_callback_callback__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 37);
    AppendRuntimeWasmName(export_section, "echo_ref_counter__RefCounter__RefCounter");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 38);
    AppendRuntimeWasmName(export_section, "refs_size__RefCounter_array__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 39);
    AppendRuntimeWasmName(export_section, "get_refs__void__RefCounter_array");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 40);
    AppendRuntimeWasmName(export_section, "ref_map_size__string_RefCounter_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 41);
    AppendRuntimeWasmName(export_section, "get_ref_map__void__string_RefCounter_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 42);
    AppendRuntimeWasmName(export_section, "ref_groups_size__string_RefCounter_array_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 43);
    AppendRuntimeWasmName(export_section, "get_ref_groups__void__string_RefCounter_array_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 44);
    AppendRuntimeWasmName(export_section, "ref_key_map_size__RefCounter_int32_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 45);
    AppendRuntimeWasmName(export_section, "get_ref_key_map__void__RefCounter_int32_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 46);
    AppendRuntimeWasmName(export_section, "ref_key_groups_size__RefCounter_int32_array_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 47);
    AppendRuntimeWasmName(export_section, "get_ref_key_groups__void__RefCounter_int32_array_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 48);
    AppendRuntimeWasmName(export_section, "mutate_ref_counter__RefCounter_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 49);
    AppendRuntimeWasmName(export_section, "mutate_refs__RefCounter_array_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 50);
    AppendRuntimeWasmName(export_section, "mutate_ref_map__string_RefCounter_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 51);
    AppendRuntimeWasmName(export_section, "mutate_ref_groups__string_RefCounter_array_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 52);
    AppendRuntimeWasmName(export_section, "mutate_ref_key_map__RefCounter_int32_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 53);
    AppendRuntimeWasmName(export_section, "mutate_ref_key_groups__RefCounter_int32_array_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 54);
    AppendRuntimeWasmName(export_section, "rect_groups_size__string_irect_array_dict__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 55);
    AppendRuntimeWasmName(export_section, "get_rect_groups__void__string_irect_array_dict");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 56);
    AppendRuntimeWasmName(export_section, "mutate_rect_groups__string_irect_array_dict_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 57);
    AppendRuntimeWasmName(export_section, "rect_area__irect__int32");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 58);
    AppendRuntimeWasmName(export_section, "get_rect__void__irect");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 59);
    AppendRuntimeWasmName(export_section, "mutate_rect__irect_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 60);
    AppendRuntimeWasmName(export_section, "forge_ref_counter__RefCounter_mut__void");
    export_section.emplace_back(0x00);
    AppendRuntimeWasmUleb(export_section, 61);
    AppendRuntimeWasmSection(module, 7, export_section);

    vector<uint8_t> code_section;
    AppendRuntimeWasmUleb(code_section, 61);
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B}); // add
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x10, 0x00, 0x0B}); // read_side
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x41, 0x01, 0x6A, 0x0B}); // bump_mode
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x41, 0x05, 0x6A, 0x0B}); // bump_color
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // text_len
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x0B}); // echo_critter
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x0B}); // echo_proto
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x0B}); // echo_rule
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(TEXT_RESULT_OFFSET, numeric_cast<uint32_t>(text_result.size()))); // get_text
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(ANY_RESULT_OFFSET, numeric_cast<uint32_t>(any_result.size()))); // get_any
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x28, 0x02, 0x00, 0x20, 0x00, 0x41, 0x04, 0x6A, 0x28, 0x02, 0x00, 0x6A, 0x0B}); // sum_pair
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // text_list_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(BYTE_ARRAY_RESULT_OFFSET, numeric_cast<uint32_t>(byte_array_result.size()))); // get_bytes
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(TEXT_ARRAY_RESULT_OFFSET, numeric_cast<uint32_t>(text_array_result.size()))); // get_text_list
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x28, 0x02, 0x00, 0x0B}); // dict_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(DICT_RESULT_OFFSET, numeric_cast<uint32_t>(dict_result.size()))); // get_config
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x20, 0x00, 0x28, 0x02, 0x00, 0x41, 0x05, 0x6A, 0x36, 0x02, 0x00, 0x0B}); // bump_ref
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x03, 0x20, 0x01, 0x36, 0x02, 0x00, 0x20, 0x00, 0x20, 0x00, 0x28, 0x02, 0x00, 0x41, 0x05, 0x6A, 0x36, 0x02, 0x00, 0x20, 0x00, 0x41, 0x04, 0x6A, 0x20, 0x00, 0x41, 0x04, 0x6A, 0x28, 0x02, 0x00, 0x41, 0x06, 0x6A, 0x36, 0x02, 0x00, 0x0B}); // mutate_values
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x03, 0x20, 0x01, 0x36, 0x02, 0x00, 0x20, 0x00, 0x41, 0x0F, 0x6A, 0x41, 0x39, 0x3A, 0x00, 0x00, 0x0B}); // mutate_config
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x03, 0x20, 0x01, 0x36, 0x02, 0x00, 0x02, 0x40, 0x20, 0x01, 0x45, 0x0D, 0x00, 0x20, 0x00, 0x41, 0xD9, 0x00, 0x3A, 0x00, 0x00, 0x0B, 0x0B}); // mutate_text
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // callback_name_len
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // groups_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(GROUPS_RESULT_OFFSET, numeric_cast<uint32_t>(groups_result.size()))); // get_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x03, 0x20, 0x01, 0x36, 0x02, 0x00, 0x20, 0x00, 0x41, 0x0D, 0x6A, 0x41, 0x09, 0x36, 0x02, 0x00, 0x0B}); // mutate_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x42, 0x00, 0x37, 0x03, 0x00, 0x0B}); // clear_critter_ref
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x42, 0x00, 0x37, 0x03, 0x00, 0x0B}); // clear_proto_ref
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x42, 0x00, 0x37, 0x03, 0x00, 0x0B}); // clear_rule_ref
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // rule_groups_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(RULE_GROUPS_RESULT_OFFSET, numeric_cast<uint32_t>(rule_groups_result.size()))); // get_rule_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x03, 0x20, 0x01, 0x36, 0x02, 0x00, 0x20, 0x00, 0x41, 0x11, 0x6A, 0x42, 0x00, 0x37, 0x03, 0x00, 0x0B}); // mutate_rule_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // flags_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(FLAGS_RESULT_OFFSET, numeric_cast<uint32_t>(flags_result.size()))); // get_flags
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x03, 0x20, 0x01, 0x36, 0x02, 0x00, 0x20, 0x00, 0x41, 0x11, 0x6A, 0x41, 0x00, 0x3A, 0x00, 0x00, 0x0B}); // mutate_flags
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // color_groups_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(COLOR_GROUPS_RESULT_OFFSET, numeric_cast<uint32_t>(color_groups_result.size()))); // get_color_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x03, 0x20, 0x01, 0x36, 0x02, 0x00, 0x20, 0x00, 0x41, 0x12, 0x6A, 0x41, 0xAA, 0x01, 0x3A, 0x00, 0x00, 0x0B}); // mutate_color_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // nested_callback_name_len
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x0B}); // echo_ref_counter
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // refs_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(REF_ARRAY_RESULT_OFFSET, numeric_cast<uint32_t>(ref_array_result.size()))); // get_refs
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // ref_map_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(REF_MAP_RESULT_OFFSET, numeric_cast<uint32_t>(ref_map_result.size()))); // get_ref_map
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // ref_groups_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(REF_GROUPS_RESULT_OFFSET, numeric_cast<uint32_t>(ref_groups_result.size()))); // get_ref_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // ref_key_map_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(REF_KEY_MAP_RESULT_OFFSET, numeric_cast<uint32_t>(ref_key_map_result.size()))); // get_ref_key_map
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // ref_key_groups_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(REF_KEY_GROUPS_RESULT_OFFSET, numeric_cast<uint32_t>(ref_key_groups_result.size()))); // get_ref_key_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x0B}); // mutate_ref_counter
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x0B}); // mutate_refs
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x0B}); // mutate_ref_map
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x0B}); // mutate_ref_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x0B}); // mutate_ref_key_map
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x0B}); // mutate_ref_key_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x01, 0x0B}); // rect_groups_size
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(RECT_GROUPS_RESULT_OFFSET, numeric_cast<uint32_t>(rect_groups_result.size()))); // get_rect_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x03, 0x20, 0x01, 0x36, 0x02, 0x00, 0x20, 0x00, 0x41, 0x11, 0x6A, 0x41, 0x09, 0x36, 0x02, 0x00, 0x0B}); // mutate_rect_groups
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x28, 0x02, 0x08, 0x20, 0x00, 0x28, 0x02, 0x0C, 0x6C, 0x0B}); // rect_area
    AppendRuntimeWasmI64ConstFuncBody(code_section, PackRuntimeWasmTextResult(RECT_RESULT_OFFSET, numeric_cast<uint32_t>(rect_result.size()))); // get_rect
    AppendRuntimeWasmFuncBody(code_section, {0x00, 0x20, 0x00, 0x41, 0x09, 0x36, 0x02, 0x00, 0x0B}); // mutate_rect
    AppendRuntimeWasmI64StoreConstFuncBody(code_section, 0xDEAD); // forge_ref_counter
    AppendRuntimeWasmSection(module, 10, code_section);

    vector<uint8_t> data_section;
    AppendRuntimeWasmUleb(data_section, 16);
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, TEXT_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(text_result.size()));
    data_section.insert(data_section.end(), text_result.begin(), text_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, ANY_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(any_result.size()));
    data_section.insert(data_section.end(), any_result.begin(), any_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, BYTE_ARRAY_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(byte_array_result.size()));
    data_section.insert(data_section.end(), byte_array_result.begin(), byte_array_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, TEXT_ARRAY_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(text_array_result.size()));
    data_section.insert(data_section.end(), text_array_result.begin(), text_array_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, DICT_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(dict_result.size()));
    data_section.insert(data_section.end(), dict_result.begin(), dict_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, GROUPS_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(groups_result.size()));
    data_section.insert(data_section.end(), groups_result.begin(), groups_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, RULE_GROUPS_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(rule_groups_result.size()));
    data_section.insert(data_section.end(), rule_groups_result.begin(), rule_groups_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, FLAGS_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(flags_result.size()));
    data_section.insert(data_section.end(), flags_result.begin(), flags_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, COLOR_GROUPS_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(color_groups_result.size()));
    data_section.insert(data_section.end(), color_groups_result.begin(), color_groups_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, REF_ARRAY_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(ref_array_result.size()));
    data_section.insert(data_section.end(), ref_array_result.begin(), ref_array_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, REF_MAP_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(ref_map_result.size()));
    data_section.insert(data_section.end(), ref_map_result.begin(), ref_map_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, REF_GROUPS_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(ref_groups_result.size()));
    data_section.insert(data_section.end(), ref_groups_result.begin(), ref_groups_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, REF_KEY_MAP_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(ref_key_map_result.size()));
    data_section.insert(data_section.end(), ref_key_map_result.begin(), ref_key_map_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, REF_KEY_GROUPS_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(ref_key_groups_result.size()));
    data_section.insert(data_section.end(), ref_key_groups_result.begin(), ref_key_groups_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, RECT_GROUPS_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(rect_groups_result.size()));
    data_section.insert(data_section.end(), rect_groups_result.begin(), rect_groups_result.end());
    data_section.emplace_back(0x00); // active segment, memory 0
    data_section.emplace_back(0x41); // i32.const
    AppendRuntimeWasmSleb(data_section, RECT_RESULT_OFFSET);
    data_section.emplace_back(0x0B); // end
    AppendRuntimeWasmUleb(data_section, numeric_cast<uint32_t>(rect_result.size()));
    data_section.insert(data_section.end(), rect_result.begin(), rect_result.end());
    AppendRuntimeWasmSection(module, 11, data_section);

    return module;
}
#endif


TEST_CASE("WasmBaker")
{
    using namespace BakerTests;

    SECTION("CopiesWasmFilesUnchanged")
    {
        const string wasm_bytes = MakeMinimalWasmBytes();
        TestRig rig;
        rig.AddSourceFile("Scripts/math.wasm", string_view {wasm_bytes.data(), wasm_bytes.size()}, 10);
        rig.AddSourceFile("Scripts/math.fos", "namespace math {}\n", 11);

        WasmBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Scripts/math.wasm"));
        CHECK(rig.Outputs["Scripts/math.wasm"] == vector<uint8_t> {wasm_bytes.begin(), wasm_bytes.end()});
    }

    SECTION("SetupBakersReturnsRequestedBaker")
    {
        TestRig rig;
        const auto bakers = MakeRequestedBakers({string(WasmBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == WasmBaker::NAME);
        CHECK(bakers.front()->GetOrder() == 4);
    }

    SECTION("GeneratesFrontendApiManifestFromDescriptor")
    {
        TestRig rig;
        rig.AddSourceFile("Scripts/math.fowasm", "[WasmScript]\nTarget = Server\n", 10);
        rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());

        WasmBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("WasmApi/server.json"));

        const string manifest = rig.GetOutputText("WasmApi/server.json");
        CHECK(manifest.find("\"format\": \"fonline-wasm-api\"") != string::npos);
        CHECK(manifest.find("\"side\": \"server\"") != string::npos);
        CHECK(manifest.find("\"module\": \"fonline.api\"") != string::npos);
        CHECK(manifest.find("\"methods\"") != string::npos);
        CHECK(manifest.find("\"properties\"") != string::npos);
        CHECK(manifest.find("\"entities\"") != string::npos);
    }

    SECTION("CompilesDescriptorCommandOutput")
    {
        const string wasm_bytes = MakeMinimalWasmBytes();
#if FO_WINDOWS
        const string command = "powershell -NoProfile -Command Copy-Item -LiteralPath {source} -Destination {output}";
#else
        const string command = "cp {source} {output}";
#endif
        const string bake_output = "Baking/WasmBakerCompileTest";
        (void)fs_remove_dir_tree(bake_output);

        TestRig rig;
        OverrideSetting(rig.Settings.BakeOutput, bake_output);
        rig.AddSourceFile("Scripts/math.source", string_view {wasm_bytes.data(), wasm_bytes.size()}, 10);
        rig.AddSourceFile("Scripts/math.fowasm", strex("[WasmScript]\nTarget = Server\nSource = Scripts/math.source\nOutput = Scripts/math.compiled.wasm\nCommand = {}\n", command), 11);
        rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());

        WasmBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 2);
        CHECK(rig.Outputs.contains("WasmApi/server.json"));
        CHECK(rig.Outputs.contains("Scripts/math.compiled.wasm"));
        CHECK(rig.Outputs["Scripts/math.compiled.wasm"] == vector<uint8_t> {wasm_bytes.begin(), wasm_bytes.end()});

        (void)fs_remove_dir_tree(bake_output);
    }

    SECTION("AssemblyScriptFrontendGeneratesBindings")
    {
#if FO_WINDOWS
        const string command = "powershell -NoProfile -Command Copy-Item -LiteralPath {bindings} -Destination {output}";
#else
        const string command = "cp {bindings} {output}";
#endif
        const string bake_output = "Baking/WasmAssemblyScriptBakerTest";
        (void)fs_remove_dir_tree(bake_output);

        TestRig rig;
        OverrideSetting(rig.Settings.BakeOutput, bake_output);
        rig.AddSourceFile("Scripts/main.ts", "export function test(): void {}\n", 10);
        rig.AddSourceFile("Scripts/main.fowasm", strex("[WasmScript]\nFrontend = AssemblyScript\nTarget = Server\nSource = Scripts/main.ts\nOutput = Scripts/main.server.wasm\nCommand = {}\n", command), 11);
        rig.AddBakedFile("Metadata.fometa-server", MakeEmptyMetadataBlob());

        WasmBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 2);
        CHECK(rig.Outputs.contains("WasmApi/server.json"));
        CHECK(rig.Outputs.contains("Scripts/main.server.wasm"));

        const string bindings = rig.GetOutputText("Scripts/main.server.wasm");
        CHECK(bindings.find("Generated by WasmAssemblyScriptBaker") != string::npos);
        CHECK(bindings.find("export namespace Api") != string::npos);
        CHECK(bindings.find("@external(\"fonline.api\"") != string::npos);
        CHECK(bindings.find("export declare function") != string::npos);

        (void)fs_remove_dir_tree(bake_output);
    }

    SECTION("AssemblyScriptFrontendDefaultCommandIsSelfContained")
    {
        CHECK(WasmAssemblyScriptBaker::IsFrontend("AssemblyScript"));
        CHECK(WasmAssemblyScriptBaker::IsFrontend("as"));
        CHECK(WasmAssemblyScriptBaker::IsFrontend("asc"));

        const string command = WasmAssemblyScriptBaker::MakeDefaultCommand(WasmAssemblyScriptBaker::CompileOptions {});
        CHECK(command.find("npm exec --yes --package assemblyscript -- asc") != string::npos);
        CHECK(command.find("{source}") != string::npos);
        CHECK(command.find("--outFile {output}") != string::npos);
        CHECK(command.find("--optimize") != string::npos);

        const string custom_command = WasmAssemblyScriptBaker::MakeDefaultCommand(WasmAssemblyScriptBaker::CompileOptions {
            .Compiler = "asc",
            .CompilerArgs = "--debug",
        });
        CHECK(custom_command == "asc {source} --outFile {output} --debug");
    }

    SECTION("RejectsInvalidFrontendDescriptors")
    {
        {
            TestRig rig;
            rig.AddSourceFile("Scripts/main.fowasm", "[WasmScript]\nFrontend = Unknown\nTarget = Server\n", 10);

            WasmBaker baker(rig.MakeContext());
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), WasmBakerException);
        }
        {
            TestRig rig;
            rig.AddSourceFile("Scripts/main.ts", "export function test(): void {}\n", 10);
            rig.AddSourceFile("Scripts/main.fowasm", "[WasmScript]\nFrontend = AssemblyScript\nTarget = Server\nSource = Scripts/main.ts\nOutput = Scripts/main.server.wasm\n[AssemblyScript]\nBindings = ../fonline_api.ts\n", 11);

            WasmBaker baker(rig.MakeContext());
            CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), WasmBakerException);
        }
    }

    SECTION("CopiesOnlyExplicitWasmTarget")
    {
        const string wasm_bytes = MakeMinimalWasmBytes();
        TestRig rig;
        rig.AddSourceFile("Scripts/a.wasm", string_view {wasm_bytes.data(), wasm_bytes.size()}, 10);
        rig.AddSourceFile("Scripts/b.wasm", string_view {wasm_bytes.data(), wasm_bytes.size()}, 11);

        WasmBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Scripts/b.wasm");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK_FALSE(rig.Outputs.contains("Scripts/a.wasm"));
        CHECK(rig.Outputs.contains("Scripts/b.wasm"));
    }

    SECTION("SkipsNonWasmExplicitTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Scripts/math.fos", "namespace math {}\n", 10);

        WasmBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Scripts/math.fos");

        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipCopy")
    {
        const string wasm_bytes = MakeMinimalWasmBytes();
        TestRig rig;
        rig.AddSourceFile("Scripts/math.wasm", string_view {wasm_bytes.data(), wasm_bytes.size()}, 10);

        WasmBaker baker(rig.MakeContext("TestPack", [](string_view, uint64_t) { return false; }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
    }
}

TEST_CASE("WasmImports")
{
    SECTION("ResolvesRegisteredImportsAndSignatures")
    {
        const WasmImportDesc* log_utf8 = FindWasmImportDesc("fonline", "log_utf8");
        const WasmImportDesc* callback_retain = FindWasmImportDesc("fonline", "callback_retain");
        const array<WasmScalarKind, 2> valid_args = {WasmScalarKind::I32, WasmScalarKind::I32};
        const array<WasmScalarKind, 1> short_args = {WasmScalarKind::I32};
        const array<WasmScalarKind, 1> result = {WasmScalarKind::I32};
        const array<WasmScalarKind, 0> no_results {};

        REQUIRE(log_utf8 != nullptr);
        CHECK(log_utf8->NativeSignature == "(*~)");
        CHECK(ValidateWasmImportSignature(*log_utf8, valid_args, no_results));
        CHECK_FALSE(ValidateWasmImportSignature(*log_utf8, short_args, no_results));
        CHECK_FALSE(ValidateWasmImportSignature(*log_utf8, valid_args, result));

        REQUIRE(callback_retain != nullptr);
        CHECK(callback_retain->NativeSignature == "(*~)i");
        CHECK(ValidateWasmImportSignature(*callback_retain, valid_args, result));
        CHECK_FALSE(ValidateWasmImportSignature(*callback_retain, short_args, result));
        CHECK_FALSE(ValidateWasmImportSignature(*callback_retain, valid_args, no_results));
    }

    SECTION("MapsScalarKindsToManifestAndEngineTypes")
    {
        CHECK(ResolveWasmScalarKind("i32") == WasmScalarKind::I32);
        CHECK(ResolveWasmScalarKind("i64") == WasmScalarKind::I64);
        CHECK(ResolveWasmScalarKind("f32") == WasmScalarKind::F32);
        CHECK(ResolveWasmScalarKind("f64") == WasmScalarKind::F64);
        CHECK(ResolveWasmScalarKind("externref") == WasmScalarKind::None);

        CHECK(WasmScalarKindToTypeName(WasmScalarKind::I32) == "i32");
        CHECK(WasmScalarKindToEngineTypeName(WasmScalarKind::I32) == "int32");
        CHECK(WasmScalarKindToTypeName(WasmScalarKind::None).empty());
        CHECK(WasmScalarKindToEngineTypeName(WasmScalarKind::None).empty());
    }

    SECTION("RuntimeContextAccessorsReturnSnapshotValues")
    {
        const WasmRuntimeContext context {
            .Side = 2,
            .FrameTimeMs = 10,
            .FrameDeltaTimeMs = 3,
            .TimeSynchronized = 1,
            .SynchronizedTimeMs = 42,
        };

        CHECK(WasmGetSide(context) == 2);
        CHECK(WasmGetFrameTimeMs(context) == 10);
        CHECK(WasmGetFrameDeltaTimeMs(context) == 3);
        CHECK(WasmIsTimeSynchronized(context) == 1);
        CHECK(WasmGetSynchronizedTimeMs(context) == 42);
    }

    SECTION("RetainsTemporaryCallbacksAcrossScopePop")
    {
        GlobalSettings settings {false};
        settings.ApplyDefaultSettings();

        WasmApiTestEngine engine {settings, [](EngineMetadata& meta) {
                                      FO_STACK_TRACE_ENTRY();

                                      meta.RegisterSide(EngineSideKind::ServerSide);
                                      meta.RegisterEntityType("Game", true, true, false, false, false);
                                      meta.RegisterEntityType("ImGui", true, true, false, false, false);
                                      meta.RegisterEntityType("Critter", true, false, true, false, false);
                                      meta.RegisterEntityType("Item", true, false, true, false, false);
                                      meta.RegisterFixedType("Rule", true);
                                  }};

        ComplexTypeDesc callback_type = engine.ResolveComplexType("callback(int32,int32)");
        auto callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::DelegateAdd");
        callback_desc->Args = {{"value", engine.ResolveComplexType("int32"), false}};
        callback_desc->Ret = engine.ResolveComplexType("int32");
        callback_desc->DelegateObj = 1;
        callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            const int32_t value = *cast_from_void<int32_t*>(call.ArgsData[0]);
            WriteWasmApiTestValue(call.RetData, numeric_cast<int32_t>(value + 1));
        };
        callback_desc->AttributeChecker = [](string_view) noexcept { return false; };

        const size_t retained_scope = engine.PushTemporaryScriptCallbackScope();
        const string retained_token = engine.RegisterTemporaryScriptCallback(make_unique_del_ptr(callback_desc.release(), [](ScriptFuncDesc* ptr) { delete ptr; }));
        WasmRuntimeContext context {
            .ScriptSys = &engine,
        };

        CHECK(WasmRetainCallback(context, retained_token.data(), numeric_cast<int32_t>(retained_token.size())) == 1);
        CHECK(WasmRetainCallback(context, retained_token.data(), numeric_cast<int32_t>(retained_token.size())) == 1);
        engine.PopTemporaryScriptCallbackScope(retained_scope);
        CHECK(engine.FindTemporaryScriptCallback(retained_token, callback_type) != nullptr);
        CHECK(WasmReleaseCallback(context, retained_token.data(), numeric_cast<int32_t>(retained_token.size())) == 1);
        CHECK(engine.FindTemporaryScriptCallback(retained_token, callback_type) != nullptr);
        CHECK(WasmReleaseCallback(context, retained_token.data(), numeric_cast<int32_t>(retained_token.size())) == 1);
        CHECK(engine.FindTemporaryScriptCallback(retained_token, callback_type) == nullptr);
        CHECK(WasmReleaseCallback(context, retained_token.data(), numeric_cast<int32_t>(retained_token.size())) == 0);

        auto scoped_callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        scoped_callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::Scoped");
        scoped_callback_desc->Args = {{"value", engine.ResolveComplexType("int32"), false}};
        scoped_callback_desc->Ret = engine.ResolveComplexType("int32");
        scoped_callback_desc->DelegateObj = 1;
        scoped_callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            const int32_t value = *cast_from_void<int32_t*>(call.ArgsData[0]);
            WriteWasmApiTestValue(call.RetData, numeric_cast<int32_t>(value + 2));
        };
        scoped_callback_desc->AttributeChecker = [](string_view) noexcept { return false; };

        const size_t scoped_scope = engine.PushTemporaryScriptCallbackScope();
        const string scoped_token = engine.RegisterTemporaryScriptCallback(make_unique_del_ptr(scoped_callback_desc.release(), [](ScriptFuncDesc* ptr) { delete ptr; }));
        engine.PopTemporaryScriptCallbackScope(scoped_scope);
        CHECK(engine.FindTemporaryScriptCallback(scoped_token, callback_type) == nullptr);

        const string_view global_callback = "CallbackModule::Global";
        CHECK(WasmRetainCallback(context, global_callback.data(), numeric_cast<int32_t>(global_callback.size())) == 1);
        CHECK(WasmReleaseCallback(context, global_callback.data(), numeric_cast<int32_t>(global_callback.size())) == 1);
        CHECK(WasmRetainCallback(context, nullptr, 0) == 0);
        CHECK(WasmRetainCallback(context, retained_token.data(), -1) == 0);
    }
}

#if !FO_WEB
TEST_CASE("WasmBackend")
{
    SECTION("LoadsNativeWamrModuleAndCallsScalarExports")
    {
        GlobalSettings settings {false};
        settings.ApplyDefaultSettings();

        WasmApiTestEngine engine {settings, [](EngineMetadata& meta) {
                                      FO_STACK_TRACE_ENTRY();

                                      meta.RegisterSide(EngineSideKind::ServerSide);
                                      meta.RegisterEntityType("Game", true, true, false, false, false);
                                      meta.RegisterEntityType("ImGui", true, true, false, false, false);
                                      meta.RegisterEntityType("Critter", true, false, true, false, false);
                                      ptr<PropertyRegistrar> item_props = meta.RegisterEntityType("Item", true, false, true, false, false);
                                      ptr<PropertyRegistrar> rule_props = meta.RegisterFixedType("Rule", true);
                                      meta.RegisterEnumGroup("TestMode", "uint8", {{"None", 0}, {"Enabled", 7}});
                                      meta.RegisterValueType("ucolor");
                                      meta.RegisterValueTypeLayout("ucolor", {{"value", "uint32"}});
                                      meta.RegisterValueType("irect");
                                      meta.RegisterValueTypeLayout("irect", {{"x", "int32"}, {"y", "int32"}, {"width", "int32"}, {"height", "int32"}});
                                      meta.RegisterRefType("RefCounter");
                                      meta.RegisterRefTypeMethods("RefCounter",
                                          vector<MethodDesc> {
                                              MethodDesc {
                                                  .Name = "__AddRef",
                                                  .Ret = {},
                                                  .Call = WasmExportRefLifecycleTestAddRef,
                                              },
                                              MethodDesc {
                                                  .Name = "__Release",
                                                  .Ret = {},
                                                  .Call = WasmExportRefLifecycleTestRelease,
                                              },
                                              MethodDesc {
                                                  .Name = "__Factory",
                                                  .Ret = meta.ResolveComplexType("RefCounter"),
                                                  .Call = DummyWasmApiCall,
                                                  .PassOwnership = true,
                                              },
                                          });

                                      const hstring item_type_name = meta.Hashes.ToHashedString("Item");
                                      const hstring item_proto_id = meta.Hashes.ToHashedString("TestItem");
                                      auto item_proto = SafeAlloc::MakeRefCounted<ProtoItem>(item_proto_id, item_props);
                                      meta.RegisterProto(item_type_name, item_proto);

                                      const hstring rule_type_name = meta.Hashes.ToHashedString("Rule");
                                      const hstring rule_proto_id = meta.Hashes.ToHashedString("DefaultRule");
                                      auto rule_proto = SafeAlloc::MakeRefCounted<ProtoCustomEntity>(rule_proto_id, rule_props);
                                      meta.RegisterProto(rule_type_name, rule_proto);
                                  }};

        WasmApiTestEntity critter {engine.GetPropertyRegistrar("Critter"), ident_t {4242}};
        engine.AddResolvedEntity(&critter);

        BakerTests::MemoryFileSet resources {"WasmRuntimeResources"};
        resources.AddBinaryFile("Scripts/math.server.wasm", MakeRuntimeWasmBytes());

        WasmBackend backend {&settings};
        backend.RegisterMetadata(&engine);
        backend.LoadScripts(resources.GetFileSystem());

        nptr<ScriptFuncDesc> add_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::add"), engine.ResolveComplexType("callback(int32,int32,int32)"));
        REQUIRE(add_desc != nullptr);

        ScriptFunc<int32_t, int32_t, int32_t> add {add_desc};
        REQUIRE(add);
        CHECK(add.HasAttribute("Wasm"));
        REQUIRE(add.Call(40, 2));
        CHECK(add.GetResult() == 42);

        nptr<ScriptFuncDesc> read_side_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::read_side"), engine.ResolveComplexType("callback(int32)"));
        REQUIRE(read_side_desc != nullptr);

        ScriptFunc<int32_t> read_side {read_side_desc};
        REQUIRE(read_side);
        CHECK(read_side.HasAttribute("Wasm"));
        REQUIRE(read_side.Call());
        CHECK(read_side.GetResult() == 0);

        nptr<ScriptFuncDesc> bump_mode_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::bump_mode"), engine.ResolveComplexType("callback(TestMode,TestMode)"));
        REQUIRE(bump_mode_desc != nullptr);

        ScriptFunc<uint8_t, uint8_t> bump_mode {bump_mode_desc};
        REQUIRE(bump_mode);
        CHECK(bump_mode.HasAttribute("Wasm"));
        REQUIRE(bump_mode.Call(7));
        CHECK(bump_mode.GetResult() == 8);

        nptr<ScriptFuncDesc> bump_color_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::bump_color"), engine.ResolveComplexType("callback(ucolor,ucolor)"));
        REQUIRE(bump_color_desc != nullptr);

        ScriptFunc<uint32_t, uint32_t> bump_color {bump_color_desc};
        REQUIRE(bump_color);
        CHECK(bump_color.HasAttribute("Wasm"));
        REQUIRE(bump_color.Call(0x11223344));
        CHECK(bump_color.GetResult() == 0x11223349);

        nptr<ScriptFuncDesc> text_len_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::text_len"), engine.ResolveComplexType("callback(int32,string)"));
        REQUIRE(text_len_desc != nullptr);

        ScriptFunc<int32_t, string> text_len {text_len_desc};
        REQUIRE(text_len);
        CHECK(text_len.HasAttribute("Wasm"));
        const string text = "hello wasm export";
        REQUIRE(text_len.Call(text));
        CHECK(text_len.GetResult() == numeric_cast<int32_t>(text.size()));

        nptr<ScriptFuncDesc> echo_critter_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::echo_critter"), engine.ResolveComplexType("callback(Critter,Critter)"));
        REQUIRE(echo_critter_desc != nullptr);

        ScriptFunc<nptr<Entity>, nptr<Entity>> echo_critter {echo_critter_desc};
        REQUIRE(echo_critter);
        CHECK(echo_critter.HasAttribute("Wasm"));
        REQUIRE(echo_critter.Call(&critter));
        CHECK(echo_critter.GetResult() == &critter);

        const hstring item_proto_id = engine.Hashes.ToHashedString("TestItem");
        nptr<Entity> item_proto = const_cast<ProtoEntity*>(engine.GetProtoEntity(engine.Hashes.ToHashedString("Item"), item_proto_id).get());
        REQUIRE(item_proto);

        nptr<ScriptFuncDesc> echo_proto_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::echo_proto"), engine.ResolveComplexType("callback(ProtoItem,ProtoItem)"));
        REQUIRE(echo_proto_desc != nullptr);

        ScriptFunc<nptr<Entity>, nptr<Entity>> echo_proto {echo_proto_desc};
        REQUIRE(echo_proto);
        CHECK(echo_proto.HasAttribute("Wasm"));
        REQUIRE(echo_proto.Call(item_proto));
        CHECK(echo_proto.GetResult() == item_proto);

        const hstring rule_proto_id = engine.Hashes.ToHashedString("DefaultRule");
        nptr<Entity> rule_proto = const_cast<ProtoEntity*>(engine.GetProtoEntity(engine.Hashes.ToHashedString("Rule"), rule_proto_id).get());
        REQUIRE(rule_proto);

        nptr<ScriptFuncDesc> echo_rule_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::echo_rule"), engine.ResolveComplexType("callback(Rule,Rule)"));
        REQUIRE(echo_rule_desc != nullptr);

        ScriptFunc<nptr<Entity>, nptr<Entity>> echo_rule {echo_rule_desc};
        REQUIRE(echo_rule);
        CHECK(echo_rule.HasAttribute("Wasm"));
        REQUIRE(echo_rule.Call(rule_proto));
        CHECK(echo_rule.GetResult() == rule_proto);

        nptr<ScriptFuncDesc> echo_ref_counter_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::echo_ref_counter"), engine.ResolveComplexType("callback(RefCounter,RefCounter)"));
        REQUIRE(echo_ref_counter_desc != nullptr);

        CHECK(echo_ref_counter_desc->AttributeChecker("Wasm"));
        WasmExportRefLifecycleTestValue first_ref;
        WasmExportRefLifecycleTestValue second_ref;
        void* ref_counter = &first_ref;
        void* ref_counter_result = nullptr;
        const array<ptr<void>, 1> ref_counter_args = {make_nptr(&ref_counter).void_cast()};
        FuncCallData ref_counter_call {
            .Accessor = &NativeDataProvider::NATIVE_DATA_ACCESSOR,
            .ArgsData = ref_counter_args,
            .RetData = make_nptr(&ref_counter_result).void_cast(),
        };
        echo_ref_counter_desc->Call(ref_counter_call);
        CHECK(ref_counter_result == ref_counter);

        nptr<ScriptFuncDesc> mutate_ref_counter_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_ref_counter"), engine.ResolveComplexType("callback(void,RefCounter&)"));
        nptr<ScriptFuncDesc> mutate_refs_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_refs"), engine.ResolveComplexType("callback(void,RefCounter[]&)"));
        nptr<ScriptFuncDesc> mutate_ref_map_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_ref_map"), engine.ResolveComplexType("callback(void,string=>RefCounter&)"));
        nptr<ScriptFuncDesc> mutate_ref_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_ref_groups"), engine.ResolveComplexType("callback(void,string=>RefCounter[]&)"));
        nptr<ScriptFuncDesc> mutate_ref_key_map_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_ref_key_map"), engine.ResolveComplexType("callback(void,RefCounter=>int32&)"));
        nptr<ScriptFuncDesc> mutate_ref_key_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_ref_key_groups"), engine.ResolveComplexType("callback(void,RefCounter=>int32[]&)"));
        nptr<ScriptFuncDesc> forge_ref_counter_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::forge_ref_counter"), engine.ResolveComplexType("callback(void,RefCounter&)"));
        REQUIRE(mutate_ref_counter_desc != nullptr);
        REQUIRE(mutate_refs_desc != nullptr);
        REQUIRE(mutate_ref_map_desc != nullptr);
        REQUIRE(mutate_ref_groups_desc != nullptr);
        REQUIRE(mutate_ref_key_map_desc != nullptr);
        REQUIRE(mutate_ref_key_groups_desc != nullptr);
        REQUIRE(forge_ref_counter_desc != nullptr);

        const array<ptr<void>, 1> mutable_ref_counter_args = {make_nptr(&ref_counter).void_cast()};
        FuncCallData mutable_ref_counter_call {
            .Accessor = &NativeDataProvider::NATIVE_DATA_ACCESSOR,
            .ArgsData = mutable_ref_counter_args,
        };
        mutate_ref_counter_desc->Call(mutable_ref_counter_call);
        CHECK(ref_counter == &first_ref);
        CHECK(first_ref.AddRefCalls == 1);
        CHECK(first_ref.ReleaseCalls == 1);

        void* forged_ref_counter = &first_ref;
        const array<ptr<void>, 1> forged_ref_counter_args = {make_nptr(&forged_ref_counter).void_cast()};
        FuncCallData forged_ref_counter_call {
            .Accessor = &NativeDataProvider::NATIVE_DATA_ACCESSOR,
            .ArgsData = forged_ref_counter_args,
        };
        CHECK_THROWS_AS(forge_ref_counter_desc->Call(forged_ref_counter_call), ScriptCallException);
        CHECK(forged_ref_counter == &first_ref);

        nptr<ScriptFuncDesc> refs_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::refs_size"), engine.ResolveComplexType("callback(int32,RefCounter[])"));
        REQUIRE(refs_size_desc != nullptr);

        WasmExportRefCollectionTestAccessor ref_array_accessor {WasmExportRefCollectionTestMode::RefArray};
        int32_t refs_size = 0;
        vector<void*> refs_input = {reinterpret_cast<void*>(uintptr_t {0x1111}), reinterpret_cast<void*>(uintptr_t {0x2222})};
        const array<ptr<void>, 1> refs_args = {make_nptr(&refs_input).void_cast()};
        FuncCallData refs_size_call {
            .Accessor = &ref_array_accessor,
            .ArgsData = refs_args,
            .RetData = &refs_size,
        };
        refs_size_desc->Call(refs_size_call);
        CHECK(refs_size == numeric_cast<int32_t>(MakeWasmApiTestU64ArrayBlob({0x1111, 0x2222}).size()));

        nptr<ScriptFuncDesc> get_refs_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_refs"), engine.ResolveComplexType("callback(RefCounter[])"));
        REQUIRE(get_refs_desc != nullptr);

        vector<void*> refs_result;
        FuncCallData get_refs_call {
            .Accessor = &ref_array_accessor,
            .RetData = &refs_result,
        };
        get_refs_desc->Call(get_refs_call);
        CHECK(refs_result == vector<void*> {reinterpret_cast<void*>(uintptr_t {0xCAFE}), reinterpret_cast<void*>(uintptr_t {0xF00D})});

        nptr<ScriptFuncDesc> ref_map_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::ref_map_size"), engine.ResolveComplexType("callback(int32,string=>RefCounter)"));
        REQUIRE(ref_map_size_desc != nullptr);

        WasmExportRefCollectionTestAccessor string_ref_dict_accessor {WasmExportRefCollectionTestMode::StringRefDict};
        int32_t ref_map_size = 0;
        map<string, void*> ref_map_input = {{"first", reinterpret_cast<void*>(uintptr_t {0x1111})}, {"second", reinterpret_cast<void*>(uintptr_t {0x2222})}};
        const array<ptr<void>, 1> ref_map_args = {make_nptr(&ref_map_input).void_cast()};
        FuncCallData ref_map_size_call {
            .Accessor = &string_ref_dict_accessor,
            .ArgsData = ref_map_args,
            .RetData = &ref_map_size,
        };
        ref_map_size_desc->Call(ref_map_size_call);
        CHECK(ref_map_size == numeric_cast<int32_t>(MakeWasmApiTestStringU64DictBlob({{"first", 0x1111}, {"second", 0x2222}}).size()));

        nptr<ScriptFuncDesc> get_ref_map_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_ref_map"), engine.ResolveComplexType("callback(string=>RefCounter)"));
        REQUIRE(get_ref_map_desc != nullptr);

        map<string, void*> ref_map_result;
        FuncCallData get_ref_map_call {
            .Accessor = &string_ref_dict_accessor,
            .RetData = &ref_map_result,
        };
        get_ref_map_desc->Call(get_ref_map_call);
        CHECK(ref_map_result == map<string, void*> {{"first", reinterpret_cast<void*>(uintptr_t {0x1111})}, {"second", reinterpret_cast<void*>(uintptr_t {0x2222})}});

        nptr<ScriptFuncDesc> ref_groups_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::ref_groups_size"), engine.ResolveComplexType("callback(int32,string=>RefCounter[])"));
        REQUIRE(ref_groups_size_desc != nullptr);

        WasmExportRefCollectionTestAccessor string_ref_array_dict_accessor {WasmExportRefCollectionTestMode::StringRefArrayDict};
        int32_t ref_groups_size = 0;
        map<string, vector<void*>> ref_groups_input = {{"refs", {reinterpret_cast<void*>(uintptr_t {0x3333}), reinterpret_cast<void*>(uintptr_t {0x4444})}}};
        const array<ptr<void>, 1> ref_groups_args = {make_nptr(&ref_groups_input).void_cast()};
        FuncCallData ref_groups_size_call {
            .Accessor = &string_ref_array_dict_accessor,
            .ArgsData = ref_groups_args,
            .RetData = &ref_groups_size,
        };
        ref_groups_size_desc->Call(ref_groups_size_call);
        CHECK(ref_groups_size == numeric_cast<int32_t>(MakeWasmApiTestStringU64ArrayDictBlob({{"refs", {0x3333, 0x4444}}}).size()));

        nptr<ScriptFuncDesc> get_ref_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_ref_groups"), engine.ResolveComplexType("callback(string=>RefCounter[])"));
        REQUIRE(get_ref_groups_desc != nullptr);

        map<string, vector<void*>> ref_groups_result;
        FuncCallData get_ref_groups_call {
            .Accessor = &string_ref_array_dict_accessor,
            .RetData = &ref_groups_result,
        };
        get_ref_groups_desc->Call(get_ref_groups_call);
        CHECK(ref_groups_result == map<string, vector<void*>> {{"refs", {reinterpret_cast<void*>(uintptr_t {0x3333}), reinterpret_cast<void*>(uintptr_t {0x4444})}}});

        nptr<ScriptFuncDesc> ref_key_map_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::ref_key_map_size"), engine.ResolveComplexType("callback(int32,RefCounter=>int32)"));
        REQUIRE(ref_key_map_size_desc != nullptr);

        WasmExportRefCollectionTestAccessor ref_int_dict_accessor {WasmExportRefCollectionTestMode::RefIntDict};
        int32_t ref_key_map_size = 0;
        map<void*, int32_t> ref_key_map_input = {{reinterpret_cast<void*>(uintptr_t {0x5555}), 7}, {reinterpret_cast<void*>(uintptr_t {0x6666}), 9}};
        const array<ptr<void>, 1> ref_key_map_args = {make_nptr(&ref_key_map_input).void_cast()};
        FuncCallData ref_key_map_size_call {
            .Accessor = &ref_int_dict_accessor,
            .ArgsData = ref_key_map_args,
            .RetData = &ref_key_map_size,
        };
        ref_key_map_size_desc->Call(ref_key_map_size_call);
        CHECK(ref_key_map_size == numeric_cast<int32_t>(MakeWasmApiTestU64IntDictBlob({{0x5555, 7}, {0x6666, 9}}).size()));

        nptr<ScriptFuncDesc> get_ref_key_map_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_ref_key_map"), engine.ResolveComplexType("callback(RefCounter=>int32)"));
        REQUIRE(get_ref_key_map_desc != nullptr);

        map<void*, int32_t> ref_key_map_result;
        FuncCallData get_ref_key_map_call {
            .Accessor = &ref_int_dict_accessor,
            .RetData = &ref_key_map_result,
        };
        get_ref_key_map_desc->Call(get_ref_key_map_call);
        CHECK(ref_key_map_result == map<void*, int32_t> {{reinterpret_cast<void*>(uintptr_t {0x5555}), 7}, {reinterpret_cast<void*>(uintptr_t {0x6666}), 9}});

        nptr<ScriptFuncDesc> ref_key_groups_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::ref_key_groups_size"), engine.ResolveComplexType("callback(int32,RefCounter=>int32[])"));
        REQUIRE(ref_key_groups_size_desc != nullptr);

        WasmExportRefCollectionTestAccessor ref_int_array_dict_accessor {WasmExportRefCollectionTestMode::RefIntArrayDict};
        int32_t ref_key_groups_size = 0;
        map<void*, vector<int32_t>> ref_key_groups_input = {{reinterpret_cast<void*>(uintptr_t {0x7777}), {1, 2}}, {reinterpret_cast<void*>(uintptr_t {0x8888}), {3}}};
        const array<ptr<void>, 1> ref_key_groups_args = {make_nptr(&ref_key_groups_input).void_cast()};
        FuncCallData ref_key_groups_size_call {
            .Accessor = &ref_int_array_dict_accessor,
            .ArgsData = ref_key_groups_args,
            .RetData = &ref_key_groups_size,
        };
        ref_key_groups_size_desc->Call(ref_key_groups_size_call);
        CHECK(ref_key_groups_size == numeric_cast<int32_t>(MakeWasmApiTestU64IntArrayDictBlob({{0x7777, {1, 2}}, {0x8888, {3}}}).size()));

        nptr<ScriptFuncDesc> get_ref_key_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_ref_key_groups"), engine.ResolveComplexType("callback(RefCounter=>int32[])"));
        REQUIRE(get_ref_key_groups_desc != nullptr);

        map<void*, vector<int32_t>> ref_key_groups_result;
        FuncCallData get_ref_key_groups_call {
            .Accessor = &ref_int_array_dict_accessor,
            .RetData = &ref_key_groups_result,
        };
        get_ref_key_groups_desc->Call(get_ref_key_groups_call);
        CHECK(ref_key_groups_result == map<void*, vector<int32_t>> {{reinterpret_cast<void*>(uintptr_t {0x7777}), {1, 2}}, {reinterpret_cast<void*>(uintptr_t {0x8888}), {3}}});

        vector<void*> mutable_refs = {&first_ref, &second_ref};
        const array<ptr<void>, 1> mutable_refs_args = {make_nptr(&mutable_refs).void_cast()};
        FuncCallData mutable_refs_call {
            .Accessor = &ref_array_accessor,
            .ArgsData = mutable_refs_args,
        };
        mutate_refs_desc->Call(mutable_refs_call);
        CHECK(mutable_refs == vector<void*> {&first_ref, &second_ref});

        map<string, void*> mutable_ref_map = {{"first", &first_ref}, {"second", &second_ref}};
        const array<ptr<void>, 1> mutable_ref_map_args = {make_nptr(&mutable_ref_map).void_cast()};
        FuncCallData mutable_ref_map_call {
            .Accessor = &string_ref_dict_accessor,
            .ArgsData = mutable_ref_map_args,
        };
        mutate_ref_map_desc->Call(mutable_ref_map_call);
        CHECK(mutable_ref_map == map<string, void*> {{"first", &first_ref}, {"second", &second_ref}});

        map<string, vector<void*>> mutable_ref_groups = {{"refs", {&first_ref, &second_ref}}};
        const array<ptr<void>, 1> mutable_ref_groups_args = {make_nptr(&mutable_ref_groups).void_cast()};
        FuncCallData mutable_ref_groups_call {
            .Accessor = &string_ref_array_dict_accessor,
            .ArgsData = mutable_ref_groups_args,
        };
        mutate_ref_groups_desc->Call(mutable_ref_groups_call);
        CHECK(mutable_ref_groups == map<string, vector<void*>> {{"refs", {&first_ref, &second_ref}}});

        map<void*, int32_t> mutable_ref_key_map = {{&first_ref, 7}, {&second_ref, 9}};
        const array<ptr<void>, 1> mutable_ref_key_map_args = {make_nptr(&mutable_ref_key_map).void_cast()};
        FuncCallData mutable_ref_key_map_call {
            .Accessor = &ref_int_dict_accessor,
            .ArgsData = mutable_ref_key_map_args,
        };
        mutate_ref_key_map_desc->Call(mutable_ref_key_map_call);
        CHECK(mutable_ref_key_map == map<void*, int32_t> {{&first_ref, 7}, {&second_ref, 9}});

        map<void*, vector<int32_t>> mutable_ref_key_groups = {{&first_ref, {1, 2}}, {&second_ref, {3}}};
        const array<ptr<void>, 1> mutable_ref_key_groups_args = {make_nptr(&mutable_ref_key_groups).void_cast()};
        FuncCallData mutable_ref_key_groups_call {
            .Accessor = &ref_int_array_dict_accessor,
            .ArgsData = mutable_ref_key_groups_args,
        };
        mutate_ref_key_groups_desc->Call(mutable_ref_key_groups_call);
        CHECK(mutable_ref_key_groups == map<void*, vector<int32_t>> {{&first_ref, {1, 2}}, {&second_ref, {3}}});
        CHECK(first_ref.AddRefCalls > 1);
        CHECK(second_ref.AddRefCalls > 0);
        CHECK(first_ref.ReleaseCalls == first_ref.AddRefCalls);
        CHECK(second_ref.ReleaseCalls == second_ref.AddRefCalls);

        nptr<ScriptFuncDesc> clear_critter_ref_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::clear_critter_ref"), engine.ResolveComplexType("callback(void,Critter&)"));
        REQUIRE(clear_critter_ref_desc != nullptr);

        ScriptFunc<void, nptr<Entity>&> clear_critter_ref {clear_critter_ref_desc};
        REQUIRE(clear_critter_ref);
        CHECK(clear_critter_ref.HasAttribute("Wasm"));
        nptr<Entity> mutable_critter = &critter;
        REQUIRE(clear_critter_ref.Call(mutable_critter));
        CHECK(mutable_critter == nullptr);

        nptr<ScriptFuncDesc> clear_proto_ref_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::clear_proto_ref"), engine.ResolveComplexType("callback(void,ProtoItem&)"));
        REQUIRE(clear_proto_ref_desc != nullptr);

        ScriptFunc<void, nptr<Entity>&> clear_proto_ref {clear_proto_ref_desc};
        REQUIRE(clear_proto_ref);
        CHECK(clear_proto_ref.HasAttribute("Wasm"));
        nptr<Entity> mutable_proto = item_proto;
        REQUIRE(clear_proto_ref.Call(mutable_proto));
        CHECK(mutable_proto == nullptr);

        nptr<ScriptFuncDesc> clear_rule_ref_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::clear_rule_ref"), engine.ResolveComplexType("callback(void,Rule&)"));
        REQUIRE(clear_rule_ref_desc != nullptr);

        ScriptFunc<void, nptr<Entity>&> clear_rule_ref {clear_rule_ref_desc};
        REQUIRE(clear_rule_ref);
        CHECK(clear_rule_ref.HasAttribute("Wasm"));
        nptr<Entity> mutable_rule = rule_proto;
        REQUIRE(clear_rule_ref.Call(mutable_rule));
        CHECK(mutable_rule == nullptr);

        nptr<ScriptFuncDesc> get_text_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_text"), engine.ResolveComplexType("callback(string)"));
        REQUIRE(get_text_desc != nullptr);

        ScriptFunc<string> get_text {get_text_desc};
        REQUIRE(get_text);
        CHECK(get_text.HasAttribute("Wasm"));
        REQUIRE(get_text.Call());
        CHECK(get_text.GetResult() == "hello wasm result");

        nptr<ScriptFuncDesc> get_any_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_any"), engine.ResolveComplexType("callback(any)"));
        REQUIRE(get_any_desc != nullptr);

        ScriptFunc<any_t> get_any {get_any_desc};
        REQUIRE(get_any);
        CHECK(get_any.HasAttribute("Wasm"));
        REQUIRE(get_any.Call());
        CHECK(get_any.GetResult() == any_t {string {"hello wasm any"}});

        nptr<ScriptFuncDesc> sum_pair_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::sum_pair"), engine.ResolveComplexType("callback(int32,int32[])"));
        REQUIRE(sum_pair_desc != nullptr);

        ScriptFunc<int32_t, vector<int32_t>> sum_pair {sum_pair_desc};
        REQUIRE(sum_pair);
        CHECK(sum_pair.HasAttribute("Wasm"));
        const vector<int32_t> pair_values = {3, 4};
        REQUIRE(sum_pair.Call(pair_values));
        CHECK(sum_pair.GetResult() == 7);

        nptr<ScriptFuncDesc> text_list_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::text_list_size"), engine.ResolveComplexType("callback(int32,string[])"));
        REQUIRE(text_list_size_desc != nullptr);

        ScriptFunc<int32_t, vector<string>> text_list_size {text_list_size_desc};
        REQUIRE(text_list_size);
        CHECK(text_list_size.HasAttribute("Wasm"));
        const vector<string> text_list = {"aa", "bbb"};
        REQUIRE(text_list_size.Call(text_list));
        CHECK(text_list_size.GetResult() == numeric_cast<int32_t>(MakeWasmApiTestStringArrayBlob({"aa", "bbb"}).size()));

        nptr<ScriptFuncDesc> get_bytes_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_bytes"), engine.ResolveComplexType("callback(uint8[])"));
        REQUIRE(get_bytes_desc != nullptr);

        ScriptFunc<vector<uint8_t>> get_bytes {get_bytes_desc};
        REQUIRE(get_bytes);
        CHECK(get_bytes.HasAttribute("Wasm"));
        REQUIRE(get_bytes.Call());
        const vector<uint8_t> expected_bytes = {4, 5, 6};
        CHECK(get_bytes.GetResult() == expected_bytes);

        nptr<ScriptFuncDesc> get_text_list_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_text_list"), engine.ResolveComplexType("callback(string[])"));
        REQUIRE(get_text_list_desc != nullptr);

        ScriptFunc<vector<string>> get_text_list {get_text_list_desc};
        REQUIRE(get_text_list);
        CHECK(get_text_list.HasAttribute("Wasm"));
        REQUIRE(get_text_list.Call());
        const vector<string> expected_text_list = {"red", "green"};
        CHECK(get_text_list.GetResult() == expected_text_list);

        nptr<ScriptFuncDesc> dict_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::dict_size"), engine.ResolveComplexType("callback(int32,string=>string)"));
        REQUIRE(dict_size_desc != nullptr);

        ScriptFunc<int32_t, map<string, string>> dict_size {dict_size_desc};
        REQUIRE(dict_size);
        CHECK(dict_size.HasAttribute("Wasm"));
        const map<string, string> dict_values = {{"one", "1"}, {"two", "22"}};
        REQUIRE(dict_size.Call(dict_values));
        CHECK(dict_size.GetResult() == numeric_cast<int32_t>(dict_values.size()));

        nptr<ScriptFuncDesc> get_config_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_config"), engine.ResolveComplexType("callback(string=>string)"));
        REQUIRE(get_config_desc != nullptr);

        ScriptFunc<map<string, string>> get_config {get_config_desc};
        REQUIRE(get_config);
        CHECK(get_config.HasAttribute("Wasm"));
        REQUIRE(get_config.Call());
        const map<string, string> expected_config = {{"alpha", "1"}, {"beta", "22"}};
        CHECK(get_config.GetResult() == expected_config);

        nptr<ScriptFuncDesc> groups_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::groups_size"), engine.ResolveComplexType("callback(int32,string=>int32[])"));
        REQUIRE(groups_size_desc != nullptr);

        ScriptFunc<int32_t, map<string, vector<int32_t>>> groups_size {groups_size_desc};
        REQUIRE(groups_size);
        CHECK(groups_size.HasAttribute("Wasm"));
        const map<string, vector<int32_t>> groups_values = {{"one", {1, 2}}, {"two", {3}}};
        REQUIRE(groups_size.Call(groups_values));
        CHECK(groups_size.GetResult() == numeric_cast<int32_t>(MakeWasmApiTestStringIntArrayDictBlob({{"one", {1, 2}}, {"two", {3}}}).size()));

        nptr<ScriptFuncDesc> get_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_groups"), engine.ResolveComplexType("callback(string=>int32[])"));
        REQUIRE(get_groups_desc != nullptr);

        ScriptFunc<map<string, vector<int32_t>>> get_groups {get_groups_desc};
        REQUIRE(get_groups);
        CHECK(get_groups.HasAttribute("Wasm"));
        REQUIRE(get_groups.Call());
        const map<string, vector<int32_t>> expected_groups = {{"alpha", {1, 2}}, {"beta", {3, 4, 5}}};
        CHECK(get_groups.GetResult() == expected_groups);

        nptr<ScriptFuncDesc> rule_groups_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::rule_groups_size"), engine.ResolveComplexType("callback(int32,string=>Rule[])"));
        REQUIRE(rule_groups_size_desc != nullptr);

        ScriptFunc<int32_t, map<string, vector<nptr<Entity>>>> rule_groups_size {rule_groups_size_desc};
        REQUIRE(rule_groups_size);
        CHECK(rule_groups_size.HasAttribute("Wasm"));
        const map<string, vector<nptr<Entity>>> rule_groups_values = {{"rules", {rule_proto, rule_proto}}};
        REQUIRE(rule_groups_size.Call(rule_groups_values));
        CHECK(rule_groups_size.GetResult() == numeric_cast<int32_t>(MakeWasmApiTestStringU64ArrayDictBlob({{"rules", {rule_proto_id.as_hash(), rule_proto_id.as_hash()}}}).size()));

        nptr<ScriptFuncDesc> get_rule_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_rule_groups"), engine.ResolveComplexType("callback(string=>Rule[])"));
        REQUIRE(get_rule_groups_desc != nullptr);

        ScriptFunc<map<string, vector<nptr<Entity>>>> get_rule_groups {get_rule_groups_desc};
        REQUIRE(get_rule_groups);
        CHECK(get_rule_groups.HasAttribute("Wasm"));
        REQUIRE(get_rule_groups.Call());
        CHECK(get_rule_groups.GetResult() == map<string, vector<nptr<Entity>>> {{"none", {nullptr}}});

        nptr<ScriptFuncDesc> flags_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::flags_size"), engine.ResolveComplexType("callback(int32,string=>bool[])"));
        REQUIRE(flags_size_desc != nullptr);

        ScriptFunc<int32_t, map<string, vector<bool>>> flags_size {flags_size_desc};
        REQUIRE(flags_size);
        CHECK(flags_size.HasAttribute("Wasm"));
        const map<string, vector<bool>> flags_values = {{"flags", {true, false, true}}};
        REQUIRE(flags_size.Call(flags_values));
        CHECK(flags_size.GetResult() == numeric_cast<int32_t>(MakeWasmApiTestStringBoolArrayDictBlob({{"flags", {true, false, true}}}).size()));

        nptr<ScriptFuncDesc> get_flags_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_flags"), engine.ResolveComplexType("callback(string=>bool[])"));
        REQUIRE(get_flags_desc != nullptr);

        ScriptFunc<map<string, vector<bool>>> get_flags {get_flags_desc};
        REQUIRE(get_flags);
        CHECK(get_flags.HasAttribute("Wasm"));
        REQUIRE(get_flags.Call());
        CHECK(get_flags.GetResult() == map<string, vector<bool>> {{"none", {false, true}}});

        nptr<ScriptFuncDesc> color_groups_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::color_groups_size"), engine.ResolveComplexType("callback(int32,string=>ucolor[])"));
        REQUIRE(color_groups_size_desc != nullptr);

        ScriptFunc<int32_t, map<string, vector<ucolor>>> color_groups_size {color_groups_size_desc};
        REQUIRE(color_groups_size);
        CHECK(color_groups_size.HasAttribute("Wasm"));
        const map<string, vector<ucolor>> color_groups_values = {{"colors", {ucolor {0x11223344}, ucolor {0x55667788}}}};
        REQUIRE(color_groups_size.Call(color_groups_values));
        CHECK(color_groups_size.GetResult() == numeric_cast<int32_t>(MakeWasmApiTestStringU32ArrayDictBlob({{"colors", {0x11223344, 0x55667788}}}).size()));

        nptr<ScriptFuncDesc> get_color_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_color_groups"), engine.ResolveComplexType("callback(string=>ucolor[])"));
        REQUIRE(get_color_groups_desc != nullptr);

        ScriptFunc<map<string, vector<ucolor>>> get_color_groups {get_color_groups_desc};
        REQUIRE(get_color_groups);
        CHECK(get_color_groups.HasAttribute("Wasm"));
        REQUIRE(get_color_groups.Call());
        CHECK(get_color_groups.GetResult() == map<string, vector<ucolor>> {{"colors", {ucolor {0x11223344}, ucolor {0x55667788}}}});

        nptr<ScriptFuncDesc> rect_groups_size_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::rect_groups_size"), engine.ResolveComplexType("callback(int32,string=>irect[])"));
        REQUIRE(rect_groups_size_desc != nullptr);

        ScriptFunc<int32_t, map<string, vector<irect32>>> rect_groups_size {rect_groups_size_desc};
        REQUIRE(rect_groups_size);
        CHECK(rect_groups_size.HasAttribute("Wasm"));
        const map<string, vector<irect32>> rect_groups_values = {{"rects", {irect32 {1, 2, 3, 4}, irect32 {-1, 2, -3, 4}}}};
        REQUIRE(rect_groups_size.Call(rect_groups_values));
        CHECK(rect_groups_size.GetResult() == numeric_cast<int32_t>(MakeWasmApiTestStringRectArrayDictBlob({{"rects", {WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = -1, .Y = 2, .Width = -3, .Height = 4}}}}).size()));

        nptr<ScriptFuncDesc> get_rect_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_rect_groups"), engine.ResolveComplexType("callback(string=>irect[])"));
        REQUIRE(get_rect_groups_desc != nullptr);

        ScriptFunc<map<string, vector<irect32>>> get_rect_groups {get_rect_groups_desc};
        REQUIRE(get_rect_groups);
        CHECK(get_rect_groups.HasAttribute("Wasm"));
        REQUIRE(get_rect_groups.Call());
        CHECK(get_rect_groups.GetResult() == map<string, vector<irect32>> {{"rects", {irect32 {1, 2, 3, 4}, irect32 {-1, 2, -3, 4}}}});

        nptr<ScriptFuncDesc> rect_area_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::rect_area"), engine.ResolveComplexType("callback(int32,irect)"));
        REQUIRE(rect_area_desc != nullptr);

        ScriptFunc<int32_t, irect32> rect_area {rect_area_desc};
        REQUIRE(rect_area);
        CHECK(rect_area.HasAttribute("Wasm"));
        REQUIRE(rect_area.Call(irect32 {1, 2, 3, 4}));
        CHECK(rect_area.GetResult() == 12);

        nptr<ScriptFuncDesc> get_rect_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::get_rect"), engine.ResolveComplexType("callback(irect)"));
        REQUIRE(get_rect_desc != nullptr);

        ScriptFunc<irect32> get_rect {get_rect_desc};
        REQUIRE(get_rect);
        CHECK(get_rect.HasAttribute("Wasm"));
        REQUIRE(get_rect.Call());
        CHECK(get_rect.GetResult() == irect32 {5, 6, 7, 8});

        nptr<ScriptFuncDesc> bump_ref_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::bump_ref"), engine.ResolveComplexType("callback(void,int32&)"));
        REQUIRE(bump_ref_desc != nullptr);

        ScriptFunc<void, int32_t&> bump_ref {bump_ref_desc};
        REQUIRE(bump_ref);
        CHECK(bump_ref.HasAttribute("Wasm"));
        int32_t mutable_value = 37;
        REQUIRE(bump_ref.Call(mutable_value));
        CHECK(mutable_value == 42);

        nptr<ScriptFuncDesc> mutate_values_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_values"), engine.ResolveComplexType("callback(void,int32[]&)"));
        REQUIRE(mutate_values_desc != nullptr);

        ScriptFunc<void, vector<int32_t>&> mutate_values {mutate_values_desc};
        REQUIRE(mutate_values);
        CHECK(mutate_values.HasAttribute("Wasm"));
        vector<int32_t> mutable_values = {3, 4};
        REQUIRE(mutate_values.Call(mutable_values));
        CHECK(mutable_values == vector<int32_t> {8, 10});

        nptr<ScriptFuncDesc> mutate_config_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_config"), engine.ResolveComplexType("callback(void,string=>string&)"));
        REQUIRE(mutate_config_desc != nullptr);

        ScriptFunc<void, map<string, string>&> mutate_config {mutate_config_desc};
        REQUIRE(mutate_config);
        CHECK(mutate_config.HasAttribute("Wasm"));
        map<string, string> mutable_config = {{"one", "1"}};
        REQUIRE(mutate_config.Call(mutable_config));
        CHECK(mutable_config == map<string, string> {{"one", "9"}});

        nptr<ScriptFuncDesc> mutate_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_groups"), engine.ResolveComplexType("callback(void,string=>int32[]&)"));
        REQUIRE(mutate_groups_desc != nullptr);

        ScriptFunc<void, map<string, vector<int32_t>>&> mutate_groups {mutate_groups_desc};
        REQUIRE(mutate_groups);
        CHECK(mutate_groups.HasAttribute("Wasm"));
        map<string, vector<int32_t>> mutable_groups = {{"a", {1, 2}}};
        REQUIRE(mutate_groups.Call(mutable_groups));
        CHECK(mutable_groups == map<string, vector<int32_t>> {{"a", {9, 2}}});

        nptr<ScriptFuncDesc> mutate_rule_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_rule_groups"), engine.ResolveComplexType("callback(void,string=>Rule[]&)"));
        REQUIRE(mutate_rule_groups_desc != nullptr);

        ScriptFunc<void, map<string, vector<nptr<Entity>>>&> mutate_rule_groups {mutate_rule_groups_desc};
        REQUIRE(mutate_rule_groups);
        CHECK(mutate_rule_groups.HasAttribute("Wasm"));
        map<string, vector<nptr<Entity>>> mutable_rule_groups = {{"rules", {rule_proto, rule_proto}}};
        REQUIRE(mutate_rule_groups.Call(mutable_rule_groups));
        CHECK(mutable_rule_groups == map<string, vector<nptr<Entity>>> {{"rules", {nullptr, rule_proto}}});

        nptr<ScriptFuncDesc> mutate_flags_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_flags"), engine.ResolveComplexType("callback(void,string=>bool[]&)"));
        REQUIRE(mutate_flags_desc != nullptr);

        ScriptFunc<void, map<string, vector<bool>>&> mutate_flags {mutate_flags_desc};
        REQUIRE(mutate_flags);
        CHECK(mutate_flags.HasAttribute("Wasm"));
        map<string, vector<bool>> mutable_flags = {{"flags", {true, false, true}}};
        REQUIRE(mutate_flags.Call(mutable_flags));
        CHECK(mutable_flags == map<string, vector<bool>> {{"flags", {false, false, true}}});

        nptr<ScriptFuncDesc> mutate_color_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_color_groups"), engine.ResolveComplexType("callback(void,string=>ucolor[]&)"));
        REQUIRE(mutate_color_groups_desc != nullptr);

        ScriptFunc<void, map<string, vector<ucolor>>&> mutate_color_groups {mutate_color_groups_desc};
        REQUIRE(mutate_color_groups);
        CHECK(mutate_color_groups.HasAttribute("Wasm"));
        map<string, vector<ucolor>> mutable_color_groups = {{"colors", {ucolor {0x11223344}, ucolor {0x55667788}}}};
        REQUIRE(mutate_color_groups.Call(mutable_color_groups));
        CHECK(mutable_color_groups == map<string, vector<ucolor>> {{"colors", {ucolor {0x112233AA}, ucolor {0x55667788}}}});

        nptr<ScriptFuncDesc> mutate_rect_groups_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_rect_groups"), engine.ResolveComplexType("callback(void,string=>irect[]&)"));
        REQUIRE(mutate_rect_groups_desc != nullptr);

        ScriptFunc<void, map<string, vector<irect32>>&> mutate_rect_groups {mutate_rect_groups_desc};
        REQUIRE(mutate_rect_groups);
        CHECK(mutate_rect_groups.HasAttribute("Wasm"));
        map<string, vector<irect32>> mutable_rect_groups = {{"rects", {irect32 {1, 2, 3, 4}, irect32 {-1, 2, -3, 4}}}};
        REQUIRE(mutate_rect_groups.Call(mutable_rect_groups));
        CHECK(mutable_rect_groups == map<string, vector<irect32>> {{"rects", {irect32 {9, 2, 3, 4}, irect32 {-1, 2, -3, 4}}}});

        nptr<ScriptFuncDesc> mutate_rect_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_rect"), engine.ResolveComplexType("callback(void,irect&)"));
        REQUIRE(mutate_rect_desc != nullptr);

        ScriptFunc<void, irect32&> mutate_rect {mutate_rect_desc};
        REQUIRE(mutate_rect);
        CHECK(mutate_rect.HasAttribute("Wasm"));
        irect32 mutable_rect {1, 2, 3, 4};
        REQUIRE(mutate_rect.Call(mutable_rect));
        CHECK(mutable_rect == irect32 {9, 2, 3, 4});

        nptr<ScriptFuncDesc> mutate_text_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::mutate_text"), engine.ResolveComplexType("callback(void,string&)"));
        REQUIRE(mutate_text_desc != nullptr);

        ScriptFunc<void, string&> mutate_text {mutate_text_desc};
        REQUIRE(mutate_text);
        CHECK(mutate_text.HasAttribute("Wasm"));
        string mutable_text = "abc";
        REQUIRE(mutate_text.Call(mutable_text));
        CHECK(mutable_text == "Ybc");

        vector<ComplexTypeDesc> callback_name_len_args = {engine.ResolveComplexType("callback(int32,int32)")};
        nptr<ScriptFuncDesc> callback_name_len_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::callback_name_len"), engine.ResolveComplexType("int32"), callback_name_len_args);
        REQUIRE(callback_name_len_desc != nullptr);

        const ComplexTypeDesc nested_callback_type = engine.ResolveComplexType("callback(void,callback(int32,int32))");
        vector<ComplexTypeDesc> nested_callback_name_len_args = {nested_callback_type};
        nptr<ScriptFuncDesc> nested_callback_name_len_desc = engine.FindFuncDesc(engine.Hashes.ToHashedString("math::nested_callback_name_len"), engine.ResolveComplexType("int32"), nested_callback_name_len_args);
        REQUIRE(nested_callback_name_len_desc != nullptr);

        auto callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::AddOne");
        callback_desc->Args = {{"value", engine.ResolveComplexType("int32"), false}};
        callback_desc->Ret = engine.ResolveComplexType("int32");
        callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            const int32_t value = *cast_from_void<int32_t*>(call.ArgsData[0]);
            *cast_from_void<int32_t*>(call.RetData) = numeric_cast<int32_t>(value + 1);
        };
        callback_desc->AttributeChecker = [](string_view) noexcept { return false; };

        WasmExportCallbackTestAccessor callback_accessor {callback_desc.get()};
        int32_t callback_name_len = 0;
        int32_t callback_placeholder = 0;
        const array<ptr<void>, 1> callback_args = {make_nptr(&callback_placeholder).void_cast()};
        FuncCallData callback_call {
            .Accessor = &callback_accessor,
            .ArgsData = callback_args,
            .RetData = &callback_name_len,
        };
        callback_name_len_desc->Call(callback_call);
        CHECK(callback_name_len == numeric_cast<int32_t>(string_view {"CallbackModule::AddOne"}.size()));

        auto delegate_callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        delegate_callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::DelegateAdd");
        delegate_callback_desc->Args = {{"value", engine.ResolveComplexType("int32"), false}};
        delegate_callback_desc->Ret = engine.ResolveComplexType("int32");
        delegate_callback_desc->DelegateObj = 1;
        delegate_callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            const int32_t value = *cast_from_void<int32_t*>(call.ArgsData[0]);
            *cast_from_void<int32_t*>(call.RetData) = numeric_cast<int32_t>(value + 1);
        };
        delegate_callback_desc->AttributeChecker = [](string_view) noexcept { return false; };

        WasmExportCallbackTestAccessor delegate_callback_accessor {delegate_callback_desc.get()};
        int32_t delegate_callback_name_len = 0;
        FuncCallData delegate_callback_call {
            .Accessor = &delegate_callback_accessor,
            .ArgsData = callback_args,
            .RetData = &delegate_callback_name_len,
        };
        callback_name_len_desc->Call(delegate_callback_call);
        CHECK(delegate_callback_name_len > 0);
        CHECK(delegate_callback_name_len != numeric_cast<int32_t>(string_view {"CallbackModule::DelegateAdd"}.size()));

        auto nested_callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        nested_callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::AcceptNested");
        nested_callback_desc->Args = {{"func", callback_name_len_args.front(), false}};
        nested_callback_desc->Ret = {};
        nested_callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            ignore_unused(call);
        };
        nested_callback_desc->AttributeChecker = [](string_view) noexcept { return false; };

        WasmExportCallbackTestAccessor nested_callback_accessor {nested_callback_desc.get()};
        int32_t nested_callback_name_len = 0;
        const array<ptr<void>, 1> nested_callback_args = {make_nptr(&callback_placeholder).void_cast()};
        FuncCallData nested_callback_call {
            .Accessor = &nested_callback_accessor,
            .ArgsData = nested_callback_args,
            .RetData = &nested_callback_name_len,
        };
        nested_callback_name_len_desc->Call(nested_callback_call);
        CHECK(nested_callback_name_len == numeric_cast<int32_t>(string_view {"CallbackModule::AcceptNested"}.size()));
    }

    SECTION("RejectsMutableRefHandleTypeConfusion")
    {
        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterRefType("RefCounter");
        meta.RegisterRefTypeMethods("RefCounter",
            vector<MethodDesc> {
                MethodDesc {.Name = "__AddRef", .Ret = {}, .Call = WasmExportRefLifecycleTestAddRef},
                MethodDesc {.Name = "__Release", .Ret = {}, .Call = WasmExportRefLifecycleTestRelease},
            });
        meta.RegisterRefType("OtherRef");
        meta.RegisterRefTypeMethods("OtherRef",
            vector<MethodDesc> {
                MethodDesc {.Name = "__AddRef", .Ret = {}, .Call = WasmExportRefLifecycleTestAddRef},
                MethodDesc {.Name = "__Release", .Ret = {}, .Call = WasmExportRefLifecycleTestRelease},
            });
        meta.FinalizeRegistration();

        WasmExportRefLifecycleTestValue other_ref;
        WasmExportRefHandleScope ref_handle_scope;
        ref_handle_scope.TrackBorrowed(meta.GetBaseType("OtherRef"), &other_ref);

        const ComplexTypeDesc mutable_ref_type = meta.ResolveComplexType("RefCounter&");
        const uint64_t raw_handle = numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&other_ref));
        array<uint8_t, sizeof(raw_handle)> raw_data {};
        MemCopy(raw_data.data(), &raw_handle, sizeof(raw_handle));

        CHECK_THROWS_AS(ref_handle_scope.ValidateAndRetainMutableValue(mutable_ref_type, raw_data), ScriptCallException);
        CHECK(other_ref.AddRefCalls == 0);
        CHECK(other_ref.ReleaseCalls == 0);
    }
}
#endif

TEST_CASE("WasmApiBridge")
{
    SECTION("BuildsSideAwareImportTables")
    {
        EngineMetadata server_meta {[] { }};
        server_meta.RegisterSide(EngineSideKind::ServerSide);
        server_meta.RegisterEntityType("Game", true, true, false, false, false);
        server_meta.RegisterEntityMethod("Game",
            MethodDesc {
                .Name = "ServerOnly",
                .Ret = server_meta.ResolveComplexType("int32"),
                .Call = DummyWasmApiCall,
            });

        EngineMetadata client_meta {[] { }};
        client_meta.RegisterSide(EngineSideKind::ClientSide);
        client_meta.RegisterEntityType("Game", true, true, false, false, false);
        client_meta.RegisterEntityMethod("Game",
            MethodDesc {
                .Name = "ClientOnly",
                .Ret = client_meta.ResolveComplexType("int32"),
                .Call = DummyWasmApiCall,
            });

        const WasmApiImportTable server_imports = BuildWasmApiImportTable(server_meta);
        const WasmApiImportTable client_imports = BuildWasmApiImportTable(client_meta);

        CHECK(server_imports.Side == EngineSideKind::ServerSide);
        CHECK(client_imports.Side == EngineSideKind::ClientSide);
        CHECK(GetWasmApiImportTableSideName(server_imports.Side) == "server");
        CHECK(GetWasmApiImportTableSideName(client_imports.Side) == "client");
        CHECK(CountWasmApiSupportedMethods(server_imports) == 1);
        CHECK(CountWasmApiSupportedMethods(client_imports) == 1);
        CHECK(FindWasmApiMethodDesc(server_imports.Methods, WASM_ENGINE_API_MODULE, "Game_ServerOnly__void__int32"));
        CHECK(!FindWasmApiMethodDesc(server_imports.Methods, WASM_ENGINE_API_MODULE, "Game_ClientOnly__void__int32"));
        CHECK(FindWasmApiMethodDesc(client_imports.Methods, WASM_ENGINE_API_MODULE, "Game_ClientOnly__void__int32"));
        CHECK(!FindWasmApiMethodDesc(client_imports.Methods, WASM_ENGINE_API_MODULE, "Game_ServerOnly__void__int32"));
    }

    SECTION("BuildsImportDescriptorsFromMetadata")
    {
        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        ptr<PropertyRegistrar> game_props = meta.RegisterEntityType("Game", true, true, false, false, false);
        ptr<PropertyRegistrar> critter_props = meta.RegisterEntityType("Critter", true, false, true, true, true);
        meta.RegisterEntityType("Item", true, false, true, true, true);
        ptr<PropertyRegistrar> rule_props = meta.RegisterFixedType("Rule", true);
        meta.RegisterEnumGroup("TestMode", "uint8", {{"None", 0}, {"Enabled", 7}});
        meta.RegisterValueType("ident");
        meta.RegisterValueTypeLayout("ident", {{"value", "int64"}});
        meta.RegisterValueType("timespan");
        meta.RegisterValueTypeLayout("timespan", {{"value", "int64"}});
        meta.RegisterValueType("ucolor");
        meta.RegisterValueTypeLayout("ucolor", {{"value", "uint32"}});
        meta.RegisterValueType("mpos");
        meta.RegisterValueTypeLayout("mpos", {{"x", "int16"}, {"y", "int16"}});
        meta.RegisterValueType("irect");
        meta.RegisterValueTypeLayout("irect", {{"x", "int32"}, {"y", "int32"}, {"width", "int32"}, {"height", "int32"}});
        meta.RegisterValueType("TextPackName");
        meta.RegisterValueTypeLayout("TextPackName", {{"Name", "hstring"}});
        meta.RegisterRefType("RefCounter");
        meta.RegisterRefTypeMethods("RefCounter",
            vector<MethodDesc> {
                MethodDesc {
                    .Name = "__AddRef",
                    .Ret = {},
                    .Call = DummyWasmApiCall,
                },
                MethodDesc {
                    .Name = "__Release",
                    .Ret = {},
                    .Call = DummyWasmApiCall,
                },
                MethodDesc {
                    .Name = "__Factory",
                    .Ret = meta.ResolveComplexType("RefCounter"),
                    .Call = DummyWasmApiCall,
                    .PassOwnership = true,
                },
                MethodDesc {
                    .Name = "Value",
                    .Ret = meta.ResolveComplexType("int32"),
                    .Call = DummyWasmApiCall,
                },
            });
        meta.RegisterRefType("PointRef");
        meta.RegisterRefTypeLayout("PointRef", {{"X", "int32"}, {"Title", "string"}});

        game_props->RegisterProperty({"Common", "int32", "GlobalCounter", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "float64", "ReadOnlyValue"});
        game_props->RegisterProperty({"Common", "string", "Title", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "any", "Payload", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "int32[]", "Scores", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "irect[]", "Rects", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "string[]", "Tags", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "any[]", "Payloads", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "string=>string", "Config", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "string=>any", "AnyConfig", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "string=>int32[]", "Groups", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "string=>irect[]", "RectGroups", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "string=>any[]", "AnyGroups", "Mutable", "NoSync"});
        CHECK_THROWS_WITH(game_props->RegisterProperty({"Common", "RefCounter=>int32", "RefKeyConfig", "Mutable", "NoSync"}), Catch::Matchers::ContainsSubstring("unsupported RefType key"));
        CHECK_THROWS_WITH(game_props->RegisterProperty({"Common", "RefCounter=>int32[]", "RefKeyGroups", "Mutable", "NoSync"}), Catch::Matchers::ContainsSubstring("unsupported RefType key"));
        game_props->RegisterProperty({"Common", "hstring", "NameHash", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "ident", "SelectedCritterId", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "TestMode", "Mode", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "timespan", "Cooldown", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "ucolor", "Tint", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "mpos", "SpawnHex", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "TextPackName", "TextPack", "Mutable", "NoSync"});
        game_props->RegisterProperty({"Common", "ProtoItem", "DefaultItemProto", "Mutable", "NoSync", "Nullable"});
        critter_props->RegisterProperty({"Common", "bool", "Alive", "Mutable", "PublicSync"});
        rule_props->RegisterProperty({"Common", "int32", "Score"});

        vector<MethodDesc> methods;
        methods.emplace_back(MethodDesc {
            .Name = "Scalar",
            .Args = {{"value", meta.ResolveComplexType("int32"), false}, {"factor", meta.ResolveComplexType("float64"), false}},
            .Ret = meta.ResolveComplexType("bool"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "Text",
            .Args = {{"value", meta.ResolveComplexType("string"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CreateRefCounter",
            .Ret = meta.ResolveComplexType("RefCounter"),
            .Call = DummyWasmApiCall,
            .PassOwnership = true,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountRefs",
            .Args = {{"values", meta.ResolveComplexType("RefCounter[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetRefs",
            .Args = {},
            .Ret = meta.ResolveComplexType("RefCounter[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateRefs",
            .Args = {{"values", meta.ResolveComplexType("RefCounter[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountRefMap",
            .Args = {{"values", meta.ResolveComplexType("string=>RefCounter"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetRefMap",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>RefCounter"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateRefMap",
            .Args = {{"values", meta.ResolveComplexType("string=>RefCounter&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountRefGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>RefCounter[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetRefGroups",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>RefCounter[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateRefGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>RefCounter[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountRefKeyMap",
            .Args = {{"values", meta.ResolveComplexType("RefCounter=>int32"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetRefKeyMap",
            .Args = {},
            .Ret = meta.ResolveComplexType("RefCounter=>int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateRefKeyMap",
            .Args = {{"values", meta.ResolveComplexType("RefCounter=>int32&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountRefKeyGroups",
            .Args = {{"values", meta.ResolveComplexType("RefCounter=>int32[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetRefKeyGroups",
            .Args = {},
            .Ret = meta.ResolveComplexType("RefCounter=>int32[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateRefKeyGroups",
            .Args = {{"values", meta.ResolveComplexType("RefCounter=>int32[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "InvokeCallback",
            .Args = {{"func", meta.ResolveComplexType("callback(int32,int32)"), false}, {"value", meta.ResolveComplexType("int32"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "InvokeTextCallback",
            .Args = {{"func", meta.ResolveComplexType("callback(string,string)"), false}, {"value", meta.ResolveComplexType("string"), false}},
            .Ret = meta.ResolveComplexType("string"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "InvokeNestedCallback",
            .Args = {{"func", meta.ResolveComplexType("callback(void,callback(int32,int32))"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "AnyEcho",
            .Args = {{"value", meta.ResolveComplexType("any"), false}},
            .Ret = meta.ResolveComplexType("any"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateValue",
            .Args = {{"value", meta.ResolveComplexType("int32&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateValues",
            .Args = {{"values", meta.ResolveComplexType("int32[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "SumValues",
            .Args = {{"values", meta.ResolveComplexType("int32[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetBytes",
            .Args = {},
            .Ret = meta.ResolveComplexType("uint8[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "JoinText",
            .Args = {{"values", meta.ResolveComplexType("string[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "AnyListTotalLength",
            .Args = {{"values", meta.ResolveComplexType("any[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountConfig",
            .Args = {{"values", meta.ResolveComplexType("string=>string"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountAnyConfig",
            .Args = {{"values", meta.ResolveComplexType("string=>any"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetAnyConfig",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>any"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "SumGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>int32[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetGroups",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>int32[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>int32[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountAnyGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>any[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountFlags",
            .Args = {{"values", meta.ResolveComplexType("string=>bool[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetFlags",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>bool[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateFlags",
            .Args = {{"values", meta.ResolveComplexType("string=>bool[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountColorGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>ucolor[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetColorGroups",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>ucolor[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateColorGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>ucolor[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountRects",
            .Args = {{"values", meta.ResolveComplexType("string=>irect[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetRects",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>irect[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateRects",
            .Args = {{"values", meta.ResolveComplexType("string=>irect[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetAnyGroups",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>any[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "CountRuleGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>Rule[]"), false}},
            .Ret = meta.ResolveComplexType("int32"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetRuleGroups",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>Rule[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateRuleGroups",
            .Args = {{"values", meta.ResolveComplexType("string=>Rule[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetConfig",
            .Args = {},
            .Ret = meta.ResolveComplexType("string=>string"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateConfig",
            .Args = {{"values", meta.ResolveComplexType("string=>string&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateTextList",
            .Args = {{"values", meta.ResolveComplexType("string[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "MutateAnyList",
            .Args = {{"values", meta.ResolveComplexType("any[]&"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetTextList",
            .Args = {},
            .Ret = meta.ResolveComplexType("string[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetAnyList",
            .Args = {},
            .Ret = meta.ResolveComplexType("any[]"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetText",
            .Args = {},
            .Ret = meta.ResolveComplexType("string"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "Hash",
            .Args = {{"value", meta.ResolveComplexType("hstring"), false}},
            .Ret = meta.ResolveComplexType("hstring"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetCritter",
            .Args = {{"crId", meta.ResolveComplexType("ident"), false}},
            .Ret = meta.ResolveComplexType("Critter"),
            .Call = DummyWasmApiCall,
            .ReturnNullable = true,
        });
        methods.emplace_back(MethodDesc {
            .Name = "PickCritter",
            .Args = {{"cr", meta.ResolveComplexType("Critter"), true}},
            .Ret = meta.ResolveComplexType("Critter"),
            .Call = DummyWasmApiCall,
            .ReturnNullable = true,
        });
        methods.emplace_back(MethodDesc {
            .Name = "DestroyEntity",
            .Args = {{"entity", meta.ResolveComplexType("Entity"), true}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "GetProtoItem",
            .Args = {{"pid", meta.ResolveComplexType("hstring"), false}},
            .Ret = meta.ResolveComplexType("ProtoItem"),
            .Call = DummyWasmApiCall,
            .ReturnNullable = true,
        });
        methods.emplace_back(MethodDesc {
            .Name = "UseProtoItem",
            .Args = {{"proto", meta.ResolveComplexType("ProtoItem"), false}},
            .Ret = {},
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "RuleEcho",
            .Args = {{"rule", meta.ResolveComplexType("Rule"), false}},
            .Ret = meta.ResolveComplexType("Rule"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "EnumEcho",
            .Args = {{"mode", meta.ResolveComplexType("TestMode"), false}},
            .Ret = meta.ResolveComplexType("TestMode"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "DelayEcho",
            .Args = {{"delay", meta.ResolveComplexType("timespan"), false}},
            .Ret = meta.ResolveComplexType("timespan"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "ColorEcho",
            .Args = {{"color", meta.ResolveComplexType("ucolor"), false}},
            .Ret = meta.ResolveComplexType("ucolor"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "HexEcho",
            .Args = {{"hex", meta.ResolveComplexType("mpos"), false}},
            .Ret = meta.ResolveComplexType("mpos"),
            .Call = DummyWasmApiCall,
        });
        methods.emplace_back(MethodDesc {
            .Name = "PackEcho",
            .Args = {{"pack", meta.ResolveComplexType("TextPackName"), false}},
            .Ret = meta.ResolveComplexType("TextPackName"),
            .Call = DummyWasmApiCall,
        });

        meta.RegisterEntityMethods("Game", std::move(methods));
        meta.RegisterEntityMethod("Critter",
            MethodDesc {
                .Name = "IsAlive",
                .Args = {},
                .Ret = meta.ResolveComplexType("bool"),
                .Call = DummyWasmApiCall,
            });
        meta.RegisterEntityMethod("Critter",
            MethodDesc {
                .Name = "CanUseItem",
                .Args = {{"item", meta.ResolveComplexType("Item"), false}},
                .Ret = meta.ResolveComplexType("bool"),
                .Call = DummyWasmApiCall,
            });

        const vector<WasmApiMethodDesc> api_methods = BuildWasmApiMethodDescs(meta);
        CHECK(api_methods.size() == meta.GetScriptApiMethodEntries().size());

        nptr<const WasmApiMethodDesc> scalar = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_Scalar__int32_float64__bool");
        nptr<const WasmApiMethodDesc> text = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_Text__string__void");
        nptr<const WasmApiMethodDesc> invoke_callback = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_InvokeCallback__callback_int32__int32");
        nptr<const WasmApiMethodDesc> invoke_text_callback = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_InvokeTextCallback__callback_string__string");
        nptr<const WasmApiMethodDesc> invoke_nested_callback = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_InvokeNestedCallback__callback__void");
        nptr<const WasmApiMethodDesc> any_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_AnyEcho__any__any");
        nptr<const WasmApiMethodDesc> mutate_value = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateValue__int32_mut__void");
        nptr<const WasmApiMethodDesc> mutate_values = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateValues__int32_array_mut__void");
        nptr<const WasmApiMethodDesc> sum_values = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_SumValues__int32_array__int32");
        nptr<const WasmApiMethodDesc> get_bytes = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetBytes__void__uint8_array");
        nptr<const WasmApiMethodDesc> join_text = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_JoinText__string_array__int32");
        nptr<const WasmApiMethodDesc> any_list_total_length = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_AnyListTotalLength__any_array__int32");
        nptr<const WasmApiMethodDesc> count_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountConfig__string_string_dict__int32");
        nptr<const WasmApiMethodDesc> count_any_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountAnyConfig__string_any_dict__int32");
        nptr<const WasmApiMethodDesc> get_any_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetAnyConfig__void__string_any_dict");
        nptr<const WasmApiMethodDesc> sum_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_SumGroups__string_int32_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetGroups__void__string_int32_array_dict");
        nptr<const WasmApiMethodDesc> mutate_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateGroups__string_int32_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_any_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountAnyGroups__string_any_array_dict__int32");
        nptr<const WasmApiMethodDesc> count_flags = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountFlags__string_bool_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_flags = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetFlags__void__string_bool_array_dict");
        nptr<const WasmApiMethodDesc> mutate_flags = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateFlags__string_bool_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_color_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountColorGroups__string_ucolor_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_color_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetColorGroups__void__string_ucolor_array_dict");
        nptr<const WasmApiMethodDesc> mutate_color_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateColorGroups__string_ucolor_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_rects = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRects__string_irect_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_rects = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRects__void__string_irect_array_dict");
        nptr<const WasmApiMethodDesc> mutate_rects = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRects__string_irect_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> get_any_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetAnyGroups__void__string_any_array_dict");
        nptr<const WasmApiMethodDesc> count_rule_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRuleGroups__string_Rule_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_rule_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRuleGroups__void__string_Rule_array_dict");
        nptr<const WasmApiMethodDesc> mutate_rule_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRuleGroups__string_Rule_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> get_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetConfig__void__string_string_dict");
        nptr<const WasmApiMethodDesc> mutate_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateConfig__string_string_dict_mut__void");
        nptr<const WasmApiMethodDesc> mutate_text_list = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateTextList__string_array_mut__void");
        nptr<const WasmApiMethodDesc> mutate_any_list = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateAnyList__any_array_mut__void");
        nptr<const WasmApiMethodDesc> get_text_list = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetTextList__void__string_array");
        nptr<const WasmApiMethodDesc> get_any_list = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetAnyList__void__any_array");
        nptr<const WasmApiMethodDesc> get_text = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetText__void__string");
        nptr<const WasmApiMethodDesc> hash = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_Hash__hstring__hstring");
        nptr<const WasmApiMethodDesc> get_critter = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetCritter__ident__Critter");
        nptr<const WasmApiMethodDesc> pick_critter = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_PickCritter__Critter__Critter");
        nptr<const WasmApiMethodDesc> destroy_entity = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_DestroyEntity__Entity__void");
        nptr<const WasmApiMethodDesc> get_proto_item = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetProtoItem__hstring__ProtoItem");
        nptr<const WasmApiMethodDesc> use_proto_item = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_UseProtoItem__ProtoItem__void");
        nptr<const WasmApiMethodDesc> rule_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_RuleEcho__Rule__Rule");
        nptr<const WasmApiMethodDesc> create_ref_counter = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CreateRefCounter__void__RefCounter");
        nptr<const WasmApiMethodDesc> count_refs = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRefs__RefCounter_array__int32");
        nptr<const WasmApiMethodDesc> get_refs = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefs__void__RefCounter_array");
        nptr<const WasmApiMethodDesc> mutate_refs = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefs__RefCounter_array_mut__void");
        nptr<const WasmApiMethodDesc> count_ref_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRefMap__string_RefCounter_dict__int32");
        nptr<const WasmApiMethodDesc> get_ref_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefMap__void__string_RefCounter_dict");
        nptr<const WasmApiMethodDesc> mutate_ref_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefMap__string_RefCounter_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_ref_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRefGroups__string_RefCounter_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_ref_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefGroups__void__string_RefCounter_array_dict");
        nptr<const WasmApiMethodDesc> mutate_ref_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefGroups__string_RefCounter_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_ref_key_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRefKeyMap__RefCounter_int32_dict__int32");
        nptr<const WasmApiMethodDesc> get_ref_key_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefKeyMap__void__RefCounter_int32_dict");
        nptr<const WasmApiMethodDesc> mutate_ref_key_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefKeyMap__RefCounter_int32_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_ref_key_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRefKeyGroups__RefCounter_int32_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_ref_key_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefKeyGroups__void__RefCounter_int32_array_dict");
        nptr<const WasmApiMethodDesc> mutate_ref_key_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefKeyGroups__RefCounter_int32_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> enum_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_EnumEcho__TestMode__TestMode");
        nptr<const WasmApiMethodDesc> delay_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_DelayEcho__timespan__timespan");
        nptr<const WasmApiMethodDesc> color_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_ColorEcho__ucolor__ucolor");
        nptr<const WasmApiMethodDesc> hex_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_HexEcho__mpos__mpos");
        nptr<const WasmApiMethodDesc> pack_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_PackEcho__TextPackName__TextPackName");
        nptr<const WasmApiMethodDesc> entity_method = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Critter_IsAlive__ident__bool");
        nptr<const WasmApiMethodDesc> entity_arg_method = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Critter_CanUseItem__ident_Item__bool");
        nptr<const WasmApiMethodDesc> ref_value = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "RefCounter_Value__RefCounter__int32");
        nptr<const WasmApiMethodDesc> ref_add_ref = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "RefCounter___AddRef__RefCounter__void");
        nptr<const WasmApiMethodDesc> ref_release = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "RefCounter___Release__RefCounter__void");
        nptr<const WasmApiMethodDesc> ref_factory = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "RefCounter___Factory__void__RefCounter");

        REQUIRE(scalar);
        REQUIRE(text);
        REQUIRE(invoke_callback);
        REQUIRE(invoke_text_callback);
        REQUIRE(invoke_nested_callback);
        REQUIRE(any_echo);
        REQUIRE(mutate_value);
        REQUIRE(mutate_values);
        REQUIRE(sum_values);
        REQUIRE(get_bytes);
        REQUIRE(join_text);
        REQUIRE(any_list_total_length);
        REQUIRE(count_config);
        REQUIRE(count_any_config);
        REQUIRE(get_any_config);
        REQUIRE(sum_groups);
        REQUIRE(get_groups);
        REQUIRE(mutate_groups);
        REQUIRE(count_any_groups);
        REQUIRE(count_flags);
        REQUIRE(get_flags);
        REQUIRE(mutate_flags);
        REQUIRE(count_color_groups);
        REQUIRE(get_color_groups);
        REQUIRE(mutate_color_groups);
        REQUIRE(count_rects);
        REQUIRE(get_rects);
        REQUIRE(mutate_rects);
        REQUIRE(get_any_groups);
        REQUIRE(count_rule_groups);
        REQUIRE(get_rule_groups);
        REQUIRE(mutate_rule_groups);
        REQUIRE(get_config);
        REQUIRE(mutate_config);
        REQUIRE(mutate_text_list);
        REQUIRE(mutate_any_list);
        REQUIRE(get_text_list);
        REQUIRE(get_any_list);
        REQUIRE(get_text);
        REQUIRE(hash);
        REQUIRE(get_critter);
        REQUIRE(pick_critter);
        REQUIRE(destroy_entity);
        REQUIRE(get_proto_item);
        REQUIRE(use_proto_item);
        REQUIRE(rule_echo);
        REQUIRE(create_ref_counter);
        REQUIRE(count_refs);
        REQUIRE(get_refs);
        REQUIRE(mutate_refs);
        REQUIRE(count_ref_map);
        REQUIRE(get_ref_map);
        REQUIRE(mutate_ref_map);
        REQUIRE(count_ref_groups);
        REQUIRE(get_ref_groups);
        REQUIRE(mutate_ref_groups);
        REQUIRE(count_ref_key_map);
        REQUIRE(get_ref_key_map);
        REQUIRE(mutate_ref_key_map);
        REQUIRE(count_ref_key_groups);
        REQUIRE(get_ref_key_groups);
        REQUIRE(mutate_ref_key_groups);
        REQUIRE(enum_echo);
        REQUIRE(delay_echo);
        REQUIRE(color_echo);
        REQUIRE(hex_echo);
        REQUIRE(pack_echo);
        REQUIRE(entity_method);
        REQUIRE(entity_arg_method);
        REQUIRE(ref_value);
        REQUIRE(ref_add_ref);
        REQUIRE(ref_release);
        REQUIRE(ref_factory);

        const array<WasmScalarKind, 2> scalar_args = {WasmScalarKind::I32, WasmScalarKind::F64};
        const array<WasmScalarKind, 1> scalar_result = {WasmScalarKind::I32};
        const array<WasmScalarKind, 1> i32_arg = {WasmScalarKind::I32};
        const array<WasmScalarKind, 1> i32_result = {WasmScalarKind::I32};
        const array<WasmScalarKind, 0> no_result {};

        CHECK(scalar->Supported);
        CHECK(scalar->Module == WASM_ENGINE_API_MODULE);
        CHECK(scalar->EntityName == "Game");
        CHECK_FALSE(scalar->HasReceiverHandle);
        CHECK(scalar->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::F64});
        CHECK(scalar->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*scalar, scalar_args, scalar_result));
        CHECK_FALSE(ValidateWasmApiMethodSignature(*scalar, scalar_args, no_result));

        const array<WasmScalarKind, 2> string_args = {WasmScalarKind::I32, WasmScalarKind::I32};

        CHECK(text->Supported);
        CHECK(text->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(text->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::Utf8StringPointer, WasmApiParamAbiKind::Utf8StringLength});
        CHECK(text->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*text, string_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*text) == "(*~)");

        const array<WasmScalarKind, 3> callback_args = {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32};

        CHECK(invoke_callback->Supported);
        CHECK(invoke_callback->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(invoke_callback->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::CallbackPointer, WasmApiParamAbiKind::CallbackLength, WasmApiParamAbiKind::Scalar});
        CHECK(invoke_callback->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*invoke_callback, callback_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*invoke_callback) == "(*~i)i");

        const array<WasmScalarKind, 2> nested_callback_args = {WasmScalarKind::I32, WasmScalarKind::I32};

        CHECK(invoke_nested_callback->Supported);
        CHECK(invoke_nested_callback->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(invoke_nested_callback->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::CallbackPointer, WasmApiParamAbiKind::CallbackLength});
        CHECK(invoke_nested_callback->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*invoke_nested_callback, nested_callback_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*invoke_nested_callback) == "(*~)");

        const array<WasmScalarKind, 4> string_output_args = {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32};
        const array<WasmScalarKind, 6> callback_text_args = {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32};

        CHECK(invoke_text_callback->Supported);
        CHECK(invoke_text_callback->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(invoke_text_callback->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::CallbackPointer,
                WasmApiParamAbiKind::CallbackLength,
                WasmApiParamAbiKind::Utf8StringPointer,
                WasmApiParamAbiKind::Utf8StringLength,
                WasmApiParamAbiKind::Utf8StringOutputPointer,
                WasmApiParamAbiKind::Utf8StringOutputLength,
            });
        CHECK(invoke_text_callback->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*invoke_text_callback, callback_text_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*invoke_text_callback) == "(*~*~*~)i");

        CHECK(any_echo->Supported);
        CHECK(any_echo->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(any_echo->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::Utf8StringPointer,
                WasmApiParamAbiKind::Utf8StringLength,
                WasmApiParamAbiKind::Utf8StringOutputPointer,
                WasmApiParamAbiKind::Utf8StringOutputLength,
            });
        CHECK(any_echo->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*any_echo, string_output_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*any_echo) == "(*~*~)i");

        CHECK(mutate_value->Supported);
        CHECK(mutate_value->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_value->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::MutableValuePointer, WasmApiParamAbiKind::MutableValueLength});
        CHECK(mutate_value->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_value, string_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_value) == "(*~)");

        const array<WasmScalarKind, 4> mutable_array_args = {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32};

        CHECK(mutate_values->Supported);
        CHECK(mutate_values->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_values->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableArrayPointer,
                WasmApiParamAbiKind::MutableArrayByteLength,
                WasmApiParamAbiKind::MutableArrayCapacityByteLength,
                WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer,
            });
        CHECK(mutate_values->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_values, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_values) == "(*~~*)");

        CHECK(sum_values->Supported);
        CHECK(sum_values->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(sum_values->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ArrayPointer, WasmApiParamAbiKind::ArrayByteLength});
        CHECK(sum_values->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*sum_values, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*sum_values) == "(*~)i");

        CHECK(get_bytes->Supported);
        CHECK(get_bytes->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_bytes->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ArrayOutputPointer, WasmApiParamAbiKind::ArrayOutputByteLength});
        CHECK(get_bytes->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_bytes, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_bytes) == "(*~)i");

        CHECK(join_text->Supported);
        CHECK(join_text->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(join_text->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ArrayPointer, WasmApiParamAbiKind::ArrayByteLength});
        CHECK(join_text->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*join_text, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*join_text) == "(*~)i");

        CHECK(any_list_total_length->Supported);
        CHECK(any_list_total_length->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(any_list_total_length->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ArrayPointer, WasmApiParamAbiKind::ArrayByteLength});
        CHECK(any_list_total_length->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*any_list_total_length, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*any_list_total_length) == "(*~)i");

        CHECK(count_config->Supported);
        CHECK(count_config->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_config->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_config->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_config, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_config) == "(*~)i");

        CHECK(count_any_config->Supported);
        CHECK(count_any_config->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_any_config->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_any_config->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_any_config, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_any_config) == "(*~)i");

        CHECK(get_any_config->Supported);
        CHECK(get_any_config->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_any_config->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_any_config->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_any_config, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_any_config) == "(*~)i");

        CHECK(sum_groups->Supported);
        CHECK(sum_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(sum_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(sum_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*sum_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*sum_groups) == "(*~)i");

        CHECK(get_groups->Supported);
        CHECK(get_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_groups) == "(*~)i");

        CHECK(mutate_groups->Supported);
        CHECK(mutate_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_groups->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_groups->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_groups, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_groups) == "(*~~*)");

        CHECK(count_any_groups->Supported);
        CHECK(count_any_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_any_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_any_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_any_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_any_groups) == "(*~)i");

        CHECK(count_flags->Supported);
        CHECK(count_flags->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_flags->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_flags->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_flags, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_flags) == "(*~)i");

        CHECK(get_flags->Supported);
        CHECK(get_flags->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_flags->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_flags->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_flags, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_flags) == "(*~)i");

        CHECK(mutate_flags->Supported);
        CHECK(mutate_flags->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_flags->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_flags->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_flags, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_flags) == "(*~~*)");

        CHECK(count_color_groups->Supported);
        CHECK(count_color_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_color_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_color_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_color_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_color_groups) == "(*~)i");

        CHECK(get_color_groups->Supported);
        CHECK(get_color_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_color_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_color_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_color_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_color_groups) == "(*~)i");

        CHECK(mutate_color_groups->Supported);
        CHECK(mutate_color_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_color_groups->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_color_groups->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_color_groups, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_color_groups) == "(*~~*)");

        CHECK(count_rects->Supported);
        CHECK(count_rects->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_rects->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_rects->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_rects, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_rects) == "(*~)i");

        CHECK(get_rects->Supported);
        CHECK(get_rects->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_rects->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_rects->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_rects, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_rects) == "(*~)i");

        CHECK(mutate_rects->Supported);
        CHECK(mutate_rects->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_rects->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_rects->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_rects, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_rects) == "(*~~*)");

        CHECK(get_any_groups->Supported);
        CHECK(get_any_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_any_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_any_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_any_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_any_groups) == "(*~)i");

        CHECK(count_rule_groups->Supported);
        CHECK(count_rule_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_rule_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_rule_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_rule_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_rule_groups) == "(*~)i");

        CHECK(get_rule_groups->Supported);
        CHECK(get_rule_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_rule_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_rule_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_rule_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_rule_groups) == "(*~)i");

        CHECK(mutate_rule_groups->Supported);
        CHECK(mutate_rule_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_rule_groups->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_rule_groups->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_rule_groups, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_rule_groups) == "(*~~*)");

        CHECK(get_config->Supported);
        CHECK(get_config->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_config->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_config->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_config, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_config) == "(*~)i");

        CHECK(mutate_config->Supported);
        CHECK(mutate_config->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_config->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_config->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_config, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_config) == "(*~~*)");

        CHECK(mutate_text_list->Supported);
        CHECK(mutate_text_list->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_text_list->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableArrayPointer,
                WasmApiParamAbiKind::MutableArrayByteLength,
                WasmApiParamAbiKind::MutableArrayCapacityByteLength,
                WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer,
            });
        CHECK(mutate_text_list->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_text_list, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_text_list) == "(*~~*)");

        CHECK(mutate_any_list->Supported);
        CHECK(mutate_any_list->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_any_list->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableArrayPointer,
                WasmApiParamAbiKind::MutableArrayByteLength,
                WasmApiParamAbiKind::MutableArrayCapacityByteLength,
                WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer,
            });
        CHECK(mutate_any_list->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_any_list, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_any_list) == "(*~~*)");

        CHECK(get_text_list->Supported);
        CHECK(get_text_list->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_text_list->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ArrayOutputPointer, WasmApiParamAbiKind::ArrayOutputByteLength});
        CHECK(get_text_list->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_text_list, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_text_list) == "(*~)i");

        CHECK(get_any_list->Supported);
        CHECK(get_any_list->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_any_list->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ArrayOutputPointer, WasmApiParamAbiKind::ArrayOutputByteLength});
        CHECK(get_any_list->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_any_list, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_any_list) == "(*~)i");

        CHECK(get_text->Supported);
        CHECK(get_text->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_text->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::Utf8StringOutputPointer, WasmApiParamAbiKind::Utf8StringOutputLength});
        CHECK(get_text->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_text, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_text) == "(*~)i");

        const array<WasmScalarKind, 1> i64_arg = {WasmScalarKind::I64};
        const array<WasmScalarKind, 1> i64_result = {WasmScalarKind::I64};

        CHECK(hash->Supported);
        CHECK(hash->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(hash->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*hash, i64_arg, i64_result));

        CHECK(get_critter->Supported);
        CHECK(get_critter->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(get_critter->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*get_critter, i64_arg, i64_result));

        CHECK(pick_critter->Supported);
        CHECK(pick_critter->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(pick_critter->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*pick_critter, i64_arg, i64_result));

        CHECK(destroy_entity->Supported);
        CHECK(destroy_entity->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(destroy_entity->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*destroy_entity, i64_arg, no_result));

        CHECK(get_proto_item->Supported);
        CHECK(get_proto_item->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(get_proto_item->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*get_proto_item, i64_arg, i64_result));

        CHECK(use_proto_item->Supported);
        CHECK(use_proto_item->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(use_proto_item->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*use_proto_item, i64_arg, no_result));

        CHECK(rule_echo->Supported);
        CHECK(rule_echo->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(rule_echo->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*rule_echo, i64_arg, i64_result));

        CHECK(create_ref_counter->Supported);
        CHECK(create_ref_counter->Args.empty());
        CHECK(create_ref_counter->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*create_ref_counter, no_result, i64_result));

        CHECK(count_refs->Supported);
        CHECK(count_refs->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_refs->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ArrayPointer, WasmApiParamAbiKind::ArrayByteLength});
        CHECK(count_refs->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_refs, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_refs) == "(*~)i");

        CHECK(get_refs->Supported);
        CHECK(get_refs->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_refs->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ArrayOutputPointer, WasmApiParamAbiKind::ArrayOutputByteLength});
        CHECK(get_refs->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_refs, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_refs) == "(*~)i");

        CHECK(mutate_refs->Supported);
        CHECK(mutate_refs->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_refs->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableArrayPointer,
                WasmApiParamAbiKind::MutableArrayByteLength,
                WasmApiParamAbiKind::MutableArrayCapacityByteLength,
                WasmApiParamAbiKind::MutableArrayRequiredByteLengthPointer,
            });
        CHECK(mutate_refs->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_refs, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_refs) == "(*~~*)");

        CHECK(count_ref_map->Supported);
        CHECK(count_ref_map->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_ref_map->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_ref_map->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_ref_map, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_ref_map) == "(*~)i");

        CHECK(get_ref_map->Supported);
        CHECK(get_ref_map->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_ref_map->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_ref_map->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_ref_map, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_ref_map) == "(*~)i");

        CHECK(mutate_ref_map->Supported);
        CHECK(mutate_ref_map->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_ref_map->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_ref_map->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_ref_map, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_ref_map) == "(*~~*)");

        CHECK(count_ref_groups->Supported);
        CHECK(count_ref_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_ref_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_ref_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_ref_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_ref_groups) == "(*~)i");

        CHECK(get_ref_groups->Supported);
        CHECK(get_ref_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_ref_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_ref_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_ref_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_ref_groups) == "(*~)i");

        CHECK(mutate_ref_groups->Supported);
        CHECK(mutate_ref_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_ref_groups->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_ref_groups->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_ref_groups, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_ref_groups) == "(*~~*)");

        CHECK(count_ref_key_map->Supported);
        CHECK(count_ref_key_map->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_ref_key_map->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_ref_key_map->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_ref_key_map, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_ref_key_map) == "(*~)i");

        CHECK(get_ref_key_map->Supported);
        CHECK(get_ref_key_map->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_ref_key_map->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_ref_key_map->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_ref_key_map, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_ref_key_map) == "(*~)i");

        CHECK(mutate_ref_key_map->Supported);
        CHECK(mutate_ref_key_map->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_ref_key_map->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_ref_key_map->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_ref_key_map, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_ref_key_map) == "(*~~*)");

        CHECK(count_ref_key_groups->Supported);
        CHECK(count_ref_key_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(count_ref_key_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictPointer, WasmApiParamAbiKind::DictByteLength});
        CHECK(count_ref_key_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*count_ref_key_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*count_ref_key_groups) == "(*~)i");

        CHECK(get_ref_key_groups->Supported);
        CHECK(get_ref_key_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_ref_key_groups->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::DictOutputPointer, WasmApiParamAbiKind::DictOutputByteLength});
        CHECK(get_ref_key_groups->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*get_ref_key_groups, string_args, i32_result));
        CHECK(MakeWasmApiNativeSignature(*get_ref_key_groups) == "(*~)i");

        CHECK(mutate_ref_key_groups->Supported);
        CHECK(mutate_ref_key_groups->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_ref_key_groups->ParamAbi ==
            vector<WasmApiParamAbiKind> {
                WasmApiParamAbiKind::MutableDictPointer,
                WasmApiParamAbiKind::MutableDictByteLength,
                WasmApiParamAbiKind::MutableDictCapacityByteLength,
                WasmApiParamAbiKind::MutableDictRequiredByteLengthPointer,
            });
        CHECK(mutate_ref_key_groups->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*mutate_ref_key_groups, mutable_array_args, no_result));
        CHECK(MakeWasmApiNativeSignature(*mutate_ref_key_groups) == "(*~~*)");

        CHECK(enum_echo->Supported);
        CHECK(enum_echo->Args == vector<WasmScalarKind> {WasmScalarKind::I32});
        CHECK(enum_echo->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*enum_echo, i32_arg, i32_result));

        CHECK(delay_echo->Supported);
        CHECK(delay_echo->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(delay_echo->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*delay_echo, i64_arg, i64_result));

        CHECK(color_echo->Supported);
        CHECK(color_echo->Args == vector<WasmScalarKind> {WasmScalarKind::I32});
        CHECK(color_echo->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*color_echo, i32_arg, i32_result));

        CHECK(hex_echo->Supported);
        CHECK(hex_echo->Args == vector<WasmScalarKind> {WasmScalarKind::I32});
        CHECK(hex_echo->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*hex_echo, i32_arg, i32_result));

        CHECK(pack_echo->Supported);
        CHECK(pack_echo->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(pack_echo->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*pack_echo, i64_arg, i64_result));

        CHECK(entity_method->Supported);
        CHECK(entity_method->HasReceiverHandle);
        CHECK(entity_method->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(entity_method->Ret == WasmScalarKind::I32);

        const array<WasmScalarKind, 2> receiver_entity_args = {WasmScalarKind::I64, WasmScalarKind::I64};

        CHECK(entity_arg_method->Supported);
        CHECK(entity_arg_method->HasReceiverHandle);
        CHECK(entity_arg_method->Args == vector<WasmScalarKind> {WasmScalarKind::I64, WasmScalarKind::I64});
        CHECK(entity_arg_method->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*entity_arg_method, receiver_entity_args, scalar_result));

        CHECK(ref_value->Supported);
        CHECK(ref_value->HasReceiverHandle);
        CHECK(ref_value->ReceiverKind == WasmApiReceiverKind::RefType);
        CHECK(ref_value->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(ref_value->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiMethodSignature(*ref_value, i64_arg, i32_result));
        CHECK(MakeWasmApiNativeSignature(*ref_value) == "(I)i");

        CHECK(ref_add_ref->Supported);
        CHECK(ref_add_ref->HasReceiverHandle);
        CHECK(ref_add_ref->ReceiverKind == WasmApiReceiverKind::RefType);
        CHECK(ref_add_ref->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(ref_add_ref->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*ref_add_ref, i64_arg, no_result));
        CHECK(MakeWasmApiNativeSignature(*ref_add_ref) == "(I)");

        CHECK(ref_release->Supported);
        CHECK(ref_release->HasReceiverHandle);
        CHECK(ref_release->ReceiverKind == WasmApiReceiverKind::RefType);
        CHECK(ref_release->Args == vector<WasmScalarKind> {WasmScalarKind::I64});
        CHECK(ref_release->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiMethodSignature(*ref_release, i64_arg, no_result));
        CHECK(MakeWasmApiNativeSignature(*ref_release) == "(I)");

        CHECK(ref_factory->Supported);
        CHECK_FALSE(ref_factory->HasReceiverHandle);
        CHECK(ref_factory->ReceiverKind == WasmApiReceiverKind::None);
        CHECK(ref_factory->Args.empty());
        CHECK(ref_factory->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiMethodSignature(*ref_factory, no_result, i64_result));
        CHECK(MakeWasmApiNativeSignature(*ref_factory) == "()I");

        const vector<WasmApiPropertyDesc> api_properties = BuildWasmApiPropertyDescs(meta);
        CHECK(api_properties.size() == meta.GetScriptApiPropertyEntries().size());

        nptr<const WasmApiPropertyDesc> counter_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_GlobalCounter__int32");
        nptr<const WasmApiPropertyDesc> counter_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_GlobalCounter__int32");
        nptr<const WasmApiPropertyDesc> readonly_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_ReadOnlyValue__float64");
        nptr<const WasmApiPropertyDesc> title_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Title__string");
        nptr<const WasmApiPropertyDesc> title_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Title__string");
        nptr<const WasmApiPropertyDesc> payload_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Payload__any");
        nptr<const WasmApiPropertyDesc> payload_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Payload__any");
        nptr<const WasmApiPropertyDesc> scores_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Scores__int32_array");
        nptr<const WasmApiPropertyDesc> scores_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Scores__int32_array");
        nptr<const WasmApiPropertyDesc> rects_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Rects__irect_array");
        nptr<const WasmApiPropertyDesc> rects_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Rects__irect_array");
        nptr<const WasmApiPropertyDesc> tags_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Tags__string_array");
        nptr<const WasmApiPropertyDesc> tags_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Tags__string_array");
        nptr<const WasmApiPropertyDesc> payloads_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Payloads__any_array");
        nptr<const WasmApiPropertyDesc> payloads_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Payloads__any_array");
        nptr<const WasmApiPropertyDesc> config_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Config__string_string_dict");
        nptr<const WasmApiPropertyDesc> config_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Config__string_string_dict");
        nptr<const WasmApiPropertyDesc> any_config_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_AnyConfig__string_any_dict");
        nptr<const WasmApiPropertyDesc> any_config_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_AnyConfig__string_any_dict");
        nptr<const WasmApiPropertyDesc> groups_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Groups__string_int32_array_dict");
        nptr<const WasmApiPropertyDesc> groups_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Groups__string_int32_array_dict");
        nptr<const WasmApiPropertyDesc> rect_groups_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_RectGroups__string_irect_array_dict");
        nptr<const WasmApiPropertyDesc> rect_groups_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_RectGroups__string_irect_array_dict");
        nptr<const WasmApiPropertyDesc> any_groups_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_AnyGroups__string_any_array_dict");
        nptr<const WasmApiPropertyDesc> any_groups_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_AnyGroups__string_any_array_dict");
        nptr<const WasmApiPropertyDesc> name_hash_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_NameHash__hstring");
        nptr<const WasmApiPropertyDesc> selected_critter_id_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_SelectedCritterId__ident");
        nptr<const WasmApiPropertyDesc> mode_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Mode__TestMode");
        nptr<const WasmApiPropertyDesc> cooldown_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Cooldown__timespan");
        nptr<const WasmApiPropertyDesc> tint_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Tint__ucolor");
        nptr<const WasmApiPropertyDesc> spawn_hex_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_SpawnHex__mpos");
        nptr<const WasmApiPropertyDesc> text_pack_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_TextPack__TextPackName");
        nptr<const WasmApiPropertyDesc> default_item_proto_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_DefaultItemProto__ProtoItem");
        nptr<const WasmApiPropertyDesc> alive_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Critter_get_Alive__ident_bool");
        nptr<const WasmApiPropertyDesc> rule_score_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Rule_get_Score__hstring_int32");
        nptr<const WasmApiPropertyDesc> rule_score_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Rule_set_Score__hstring_int32");
        nptr<const WasmApiPropertyDesc> point_x_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "PointRef_get_X__PointRef_int32");
        nptr<const WasmApiPropertyDesc> point_x_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "PointRef_set_X__PointRef_int32");
        nptr<const WasmApiPropertyDesc> point_title_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "PointRef_get_Title__PointRef_string");
        nptr<const WasmApiPropertyDesc> point_title_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "PointRef_set_Title__PointRef_string");

        REQUIRE(counter_get);
        REQUIRE(counter_set);
        REQUIRE(readonly_set);
        REQUIRE(title_get);
        REQUIRE(title_set);
        REQUIRE(payload_get);
        REQUIRE(payload_set);
        REQUIRE(scores_get);
        REQUIRE(scores_set);
        REQUIRE(rects_get);
        REQUIRE(rects_set);
        REQUIRE(tags_get);
        REQUIRE(tags_set);
        REQUIRE(payloads_get);
        REQUIRE(payloads_set);
        REQUIRE(config_get);
        REQUIRE(config_set);
        REQUIRE(any_config_get);
        REQUIRE(any_config_set);
        REQUIRE(groups_get);
        REQUIRE(groups_set);
        REQUIRE(rect_groups_get);
        REQUIRE(rect_groups_set);
        REQUIRE(any_groups_get);
        REQUIRE(any_groups_set);
        REQUIRE(name_hash_get);
        REQUIRE(selected_critter_id_get);
        REQUIRE(mode_get);
        REQUIRE(cooldown_get);
        REQUIRE(tint_get);
        REQUIRE(spawn_hex_get);
        REQUIRE(text_pack_get);
        REQUIRE(default_item_proto_get);
        REQUIRE(alive_get);
        REQUIRE(rule_score_get);
        REQUIRE(rule_score_set);
        REQUIRE(point_x_get);
        REQUIRE(point_x_set);
        REQUIRE(point_title_get);
        REQUIRE(point_title_set);

        CHECK(counter_get->Supported);
        CHECK_FALSE(counter_get->HasReceiverHandle);
        CHECK(counter_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*counter_get, no_result, i32_result));

        CHECK(counter_set->Supported);
        CHECK(counter_set->ArgsCount == 1);
        CHECK(counter_set->Args[0] == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*counter_set, i32_arg, no_result));

        CHECK_FALSE(readonly_set->Supported);
        CHECK(readonly_set->UnsupportedReason.find("read-only") != string::npos);

        CHECK(title_get->Supported);
        CHECK(title_get->ArgsCount == 2);
        CHECK(title_get->Args[0] == WasmScalarKind::I32);
        CHECK(title_get->Args[1] == WasmScalarKind::I32);
        CHECK(title_get->ParamAbi[0] == WasmApiParamAbiKind::Utf8StringOutputPointer);
        CHECK(title_get->ParamAbi[1] == WasmApiParamAbiKind::Utf8StringOutputLength);
        CHECK(title_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*title_get, string_args, i32_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*title_get) == "(*~)i");

        CHECK(title_set->Supported);
        CHECK(title_set->ArgsCount == 2);
        CHECK(title_set->Args[0] == WasmScalarKind::I32);
        CHECK(title_set->Args[1] == WasmScalarKind::I32);
        CHECK(title_set->ParamAbi[0] == WasmApiParamAbiKind::Utf8StringPointer);
        CHECK(title_set->ParamAbi[1] == WasmApiParamAbiKind::Utf8StringLength);
        CHECK(title_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*title_set, string_args, no_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*title_set) == "(*~)");

        CHECK(payload_get->Supported);
        CHECK(payload_get->ArgsCount == 2);
        CHECK(payload_get->ParamAbi[0] == WasmApiParamAbiKind::Utf8StringOutputPointer);
        CHECK(payload_get->ParamAbi[1] == WasmApiParamAbiKind::Utf8StringOutputLength);
        CHECK(payload_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*payload_get, string_args, i32_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*payload_get) == "(*~)i");

        CHECK(payload_set->Supported);
        CHECK(payload_set->ArgsCount == 2);
        CHECK(payload_set->ParamAbi[0] == WasmApiParamAbiKind::Utf8StringPointer);
        CHECK(payload_set->ParamAbi[1] == WasmApiParamAbiKind::Utf8StringLength);
        CHECK(payload_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*payload_set, string_args, no_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*payload_set) == "(*~)");

        CHECK(scores_get->Supported);
        CHECK(scores_get->ArgsCount == 2);
        CHECK(scores_get->ParamAbi[0] == WasmApiParamAbiKind::ArrayOutputPointer);
        CHECK(scores_get->ParamAbi[1] == WasmApiParamAbiKind::ArrayOutputByteLength);
        CHECK(scores_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*scores_get, string_args, i32_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*scores_get) == "(*~)i");

        CHECK(scores_set->Supported);
        CHECK(scores_set->ArgsCount == 2);
        CHECK(scores_set->ParamAbi[0] == WasmApiParamAbiKind::ArrayPointer);
        CHECK(scores_set->ParamAbi[1] == WasmApiParamAbiKind::ArrayByteLength);
        CHECK(scores_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*scores_set, string_args, no_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*scores_set) == "(*~)");

        CHECK(rects_get->Supported);
        CHECK(rects_get->ArgsCount == 2);
        CHECK(rects_get->ParamAbi[0] == WasmApiParamAbiKind::ArrayOutputPointer);
        CHECK(rects_get->ParamAbi[1] == WasmApiParamAbiKind::ArrayOutputByteLength);
        CHECK(rects_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*rects_get, string_args, i32_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*rects_get) == "(*~)i");

        CHECK(rects_set->Supported);
        CHECK(rects_set->ArgsCount == 2);
        CHECK(rects_set->ParamAbi[0] == WasmApiParamAbiKind::ArrayPointer);
        CHECK(rects_set->ParamAbi[1] == WasmApiParamAbiKind::ArrayByteLength);
        CHECK(rects_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*rects_set, string_args, no_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*rects_set) == "(*~)");

        CHECK(tags_get->Supported);
        CHECK(tags_get->ArgsCount == 2);
        CHECK(tags_get->ParamAbi[0] == WasmApiParamAbiKind::ArrayOutputPointer);
        CHECK(tags_get->ParamAbi[1] == WasmApiParamAbiKind::ArrayOutputByteLength);
        CHECK(tags_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*tags_get, string_args, i32_result));

        CHECK(tags_set->Supported);
        CHECK(tags_set->ArgsCount == 2);
        CHECK(tags_set->ParamAbi[0] == WasmApiParamAbiKind::ArrayPointer);
        CHECK(tags_set->ParamAbi[1] == WasmApiParamAbiKind::ArrayByteLength);
        CHECK(tags_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*tags_set, string_args, no_result));

        CHECK(payloads_get->Supported);
        CHECK(payloads_get->ArgsCount == 2);
        CHECK(payloads_get->ParamAbi[0] == WasmApiParamAbiKind::ArrayOutputPointer);
        CHECK(payloads_get->ParamAbi[1] == WasmApiParamAbiKind::ArrayOutputByteLength);
        CHECK(payloads_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*payloads_get, string_args, i32_result));

        CHECK(payloads_set->Supported);
        CHECK(payloads_set->ArgsCount == 2);
        CHECK(payloads_set->ParamAbi[0] == WasmApiParamAbiKind::ArrayPointer);
        CHECK(payloads_set->ParamAbi[1] == WasmApiParamAbiKind::ArrayByteLength);
        CHECK(payloads_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*payloads_set, string_args, no_result));

        CHECK(config_get->Supported);
        CHECK(config_get->ArgsCount == 2);
        CHECK(config_get->ParamAbi[0] == WasmApiParamAbiKind::DictOutputPointer);
        CHECK(config_get->ParamAbi[1] == WasmApiParamAbiKind::DictOutputByteLength);
        CHECK(config_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*config_get, string_args, i32_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*config_get) == "(*~)i");

        CHECK(config_set->Supported);
        CHECK(config_set->ArgsCount == 2);
        CHECK(config_set->ParamAbi[0] == WasmApiParamAbiKind::DictPointer);
        CHECK(config_set->ParamAbi[1] == WasmApiParamAbiKind::DictByteLength);
        CHECK(config_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*config_set, string_args, no_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*config_set) == "(*~)");

        CHECK(any_config_get->Supported);
        CHECK(any_config_get->ArgsCount == 2);
        CHECK(any_config_get->ParamAbi[0] == WasmApiParamAbiKind::DictOutputPointer);
        CHECK(any_config_get->ParamAbi[1] == WasmApiParamAbiKind::DictOutputByteLength);
        CHECK(any_config_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*any_config_get, string_args, i32_result));

        CHECK(any_config_set->Supported);
        CHECK(any_config_set->ArgsCount == 2);
        CHECK(any_config_set->ParamAbi[0] == WasmApiParamAbiKind::DictPointer);
        CHECK(any_config_set->ParamAbi[1] == WasmApiParamAbiKind::DictByteLength);
        CHECK(any_config_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*any_config_set, string_args, no_result));

        CHECK(groups_get->Supported);
        CHECK(groups_get->ArgsCount == 2);
        CHECK(groups_get->ParamAbi[0] == WasmApiParamAbiKind::DictOutputPointer);
        CHECK(groups_get->ParamAbi[1] == WasmApiParamAbiKind::DictOutputByteLength);
        CHECK(groups_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*groups_get, string_args, i32_result));

        CHECK(groups_set->Supported);
        CHECK(groups_set->ArgsCount == 2);
        CHECK(groups_set->ParamAbi[0] == WasmApiParamAbiKind::DictPointer);
        CHECK(groups_set->ParamAbi[1] == WasmApiParamAbiKind::DictByteLength);
        CHECK(groups_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*groups_set, string_args, no_result));

        CHECK(rect_groups_get->Supported);
        CHECK(rect_groups_get->ArgsCount == 2);
        CHECK(rect_groups_get->ParamAbi[0] == WasmApiParamAbiKind::DictOutputPointer);
        CHECK(rect_groups_get->ParamAbi[1] == WasmApiParamAbiKind::DictOutputByteLength);
        CHECK(rect_groups_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*rect_groups_get, string_args, i32_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*rect_groups_get) == "(*~)i");

        CHECK(rect_groups_set->Supported);
        CHECK(rect_groups_set->ArgsCount == 2);
        CHECK(rect_groups_set->ParamAbi[0] == WasmApiParamAbiKind::DictPointer);
        CHECK(rect_groups_set->ParamAbi[1] == WasmApiParamAbiKind::DictByteLength);
        CHECK(rect_groups_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*rect_groups_set, string_args, no_result));
        CHECK(MakeWasmApiPropertyNativeSignature(*rect_groups_set) == "(*~)");

        CHECK(any_groups_get->Supported);
        CHECK(any_groups_get->ArgsCount == 2);
        CHECK(any_groups_get->ParamAbi[0] == WasmApiParamAbiKind::DictOutputPointer);
        CHECK(any_groups_get->ParamAbi[1] == WasmApiParamAbiKind::DictOutputByteLength);
        CHECK(any_groups_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*any_groups_get, string_args, i32_result));

        CHECK(any_groups_set->Supported);
        CHECK(any_groups_set->ArgsCount == 2);
        CHECK(any_groups_set->ParamAbi[0] == WasmApiParamAbiKind::DictPointer);
        CHECK(any_groups_set->ParamAbi[1] == WasmApiParamAbiKind::DictByteLength);
        CHECK(any_groups_set->Ret == WasmScalarKind::None);
        CHECK(ValidateWasmApiPropertySignature(*any_groups_set, string_args, no_result));

        CHECK(name_hash_get->Supported);
        CHECK(name_hash_get->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiPropertySignature(*name_hash_get, no_result, i64_result));

        CHECK(selected_critter_id_get->Supported);
        CHECK(selected_critter_id_get->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiPropertySignature(*selected_critter_id_get, no_result, i64_result));

        CHECK(mode_get->Supported);
        CHECK(mode_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*mode_get, no_result, i32_result));

        CHECK(cooldown_get->Supported);
        CHECK(cooldown_get->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiPropertySignature(*cooldown_get, no_result, i64_result));

        CHECK(tint_get->Supported);
        CHECK(tint_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*tint_get, no_result, i32_result));

        CHECK(spawn_hex_get->Supported);
        CHECK(spawn_hex_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*spawn_hex_get, no_result, i32_result));

        CHECK(text_pack_get->Supported);
        CHECK(text_pack_get->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiPropertySignature(*text_pack_get, no_result, i64_result));

        CHECK(default_item_proto_get->Supported);
        CHECK(default_item_proto_get->Ret == WasmScalarKind::I64);
        CHECK(ValidateWasmApiPropertySignature(*default_item_proto_get, no_result, i64_result));

        const array<WasmScalarKind, 1> ident_arg = {WasmScalarKind::I64};

        CHECK(alive_get->Supported);
        CHECK(alive_get->HasReceiverHandle);
        CHECK(alive_get->ArgsCount == 1);
        CHECK(alive_get->Args[0] == WasmScalarKind::I64);
        CHECK(alive_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*alive_get, ident_arg, i32_result));

        CHECK(rule_score_get->Supported);
        CHECK(rule_score_get->HasReceiverHandle);
        CHECK(rule_score_get->ReceiverKind == WasmApiReceiverKind::FixedType);
        CHECK(rule_score_get->ArgsCount == 1);
        CHECK(rule_score_get->Args[0] == WasmScalarKind::I64);
        CHECK(rule_score_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*rule_score_get, i64_arg, i32_result));

        CHECK_FALSE(rule_score_set->Supported);
        CHECK_FALSE(rule_score_set->UnsupportedReason.empty());

        CHECK(point_x_get->Supported);
        CHECK(point_x_get->HasReceiverHandle);
        CHECK(point_x_get->ReceiverKind == WasmApiReceiverKind::RefType);
        CHECK(point_x_get->ArgsCount == 1);
        CHECK(point_x_get->Args[0] == WasmScalarKind::I64);
        CHECK(point_x_get->Ret == WasmScalarKind::I32);
        CHECK(ValidateWasmApiPropertySignature(*point_x_get, i64_arg, i32_result));

        CHECK(point_x_set->Supported);
        CHECK(point_x_set->HasReceiverHandle);
        CHECK(point_x_set->ReceiverKind == WasmApiReceiverKind::RefType);
        CHECK(point_x_set->ArgsCount == 2);
        CHECK(point_x_set->Args[0] == WasmScalarKind::I64);
        CHECK(point_x_set->Args[1] == WasmScalarKind::I32);
        const array<WasmScalarKind, 2> ref_i32_args = {WasmScalarKind::I64, WasmScalarKind::I32};
        CHECK(ValidateWasmApiPropertySignature(*point_x_set, ref_i32_args, no_result));

        const array<WasmScalarKind, 3> ref_string_args = {WasmScalarKind::I64, WasmScalarKind::I32, WasmScalarKind::I32};

        CHECK(point_title_get->Supported);
        CHECK(point_title_get->HasReceiverHandle);
        CHECK(point_title_get->ReceiverKind == WasmApiReceiverKind::RefType);
        CHECK(point_title_get->ArgsCount == 3);
        CHECK(point_title_get->ParamAbi[1] == WasmApiParamAbiKind::Utf8StringOutputPointer);
        CHECK(point_title_get->ParamAbi[2] == WasmApiParamAbiKind::Utf8StringOutputLength);
        CHECK(ValidateWasmApiPropertySignature(*point_title_get, ref_string_args, i32_result));

        CHECK(point_title_set->Supported);
        CHECK(point_title_set->HasReceiverHandle);
        CHECK(point_title_set->ReceiverKind == WasmApiReceiverKind::RefType);
        CHECK(point_title_set->ArgsCount == 3);
        CHECK(point_title_set->ParamAbi[1] == WasmApiParamAbiKind::Utf8StringPointer);
        CHECK(point_title_set->ParamAbi[2] == WasmApiParamAbiKind::Utf8StringLength);
        CHECK(ValidateWasmApiPropertySignature(*point_title_set, ref_string_args, no_result));
    }

    SECTION("CallsValueTypeImportsAndProperties")
    {
        GlobalSettings settings {false};
        WasmApiTestEngine engine {settings, [](EngineMetadata& meta) {
                                      FO_STACK_TRACE_ENTRY();

                                      meta.RegisterSide(EngineSideKind::ServerSide);
                                      ptr<PropertyRegistrar> game_props = meta.RegisterEntityType("Game", true, true, false, false, false);
                                      meta.RegisterEntityType("ImGui", true, true, false, false, false);
                                      ptr<PropertyRegistrar> rule_props = meta.RegisterFixedType("Rule", true);
                                      meta.RegisterEnumGroup("TestMode", "uint8", {{"None", 0}, {"Enabled", 7}});
                                      meta.RegisterValueType("timespan");
                                      meta.RegisterValueTypeLayout("timespan", {{"value", "int64"}});
                                      meta.RegisterValueType("ucolor");
                                      meta.RegisterValueTypeLayout("ucolor", {{"value", "uint32"}});
                                      meta.RegisterValueType("mpos");
                                      meta.RegisterValueTypeLayout("mpos", {{"x", "int16"}, {"y", "int16"}});
                                      meta.RegisterValueType("irect");
                                      meta.RegisterValueTypeLayout("irect", {{"x", "int32"}, {"y", "int32"}, {"width", "int32"}, {"height", "int32"}});

                                      game_props->RegisterProperty({"Common", "TestMode", "Mode", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "timespan", "Cooldown", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "ucolor", "Tint", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "mpos", "SpawnHex", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "irect", "Viewport", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "string", "Title", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "any", "Payload", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "int32[]", "Scores", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "irect[]", "Rects", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "string[]", "Tags", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "any[]", "Payloads", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "string=>string", "Config", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "string=>any", "AnyConfig", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "string=>int32[]", "Groups", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "string=>irect[]", "RectGroups", "Mutable", "NoSync"});
                                      game_props->RegisterProperty({"Common", "string=>any[]", "AnyGroups", "Mutable", "NoSync"});
                                      ptr<const Property> rule_score_prop = rule_props->RegisterProperty({"Common", "int32", "Score"});

                                      const hstring rule_type_name = meta.Hashes.ToHashedString("Rule");
                                      const hstring default_rule_id = meta.Hashes.ToHashedString("DefaultRule");
                                      auto default_rule = SafeAlloc::MakeRefCounted<ProtoCustomEntity>(default_rule_id, rule_props);
                                      PropertyRawData score_data;
                                      score_data.SetAs<int32_t>(77);
                                      default_rule->GetPropertiesForEdit()->SetValue(rule_score_prop, score_data);
                                      meta.RegisterProto(rule_type_name, default_rule);

                                      vector<MethodDesc> methods;
                                      methods.emplace_back(MethodDesc {
                                          .Name = "RuleIdentity",
                                          .Args = {{"rule", meta.ResolveComplexType("Rule"), false}},
                                          .Ret = meta.ResolveComplexType("Rule"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const nptr<Entity> rule = NativeDataProvider::ReadTypedHandleSlot<Entity>(call.ArgsData[1]);
                                                  NativeDataProvider::WriteTypedHandleSlot<Entity>(call.RetData, rule);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "EnumBump",
                                          .Args = {{"mode", meta.ResolveComplexType("TestMode"), false}},
                                          .Ret = meta.ResolveComplexType("TestMode"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const uint8_t mode = ReadWasmApiTestValue<uint8_t>(call.ArgsData[1]);
                                                  const uint8_t result = numeric_cast<uint8_t>(mode + 1);
                                                  WriteWasmApiTestValue(call.RetData, result);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "DelayAdd",
                                          .Args = {{"delay", meta.ResolveComplexType("timespan"), false}},
                                          .Ret = meta.ResolveComplexType("timespan"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const int64_t delay = ReadWasmApiTestValue<int64_t>(call.ArgsData[1]);
                                                  WriteWasmApiTestValue(call.RetData, delay + 250);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "ColorInvert",
                                          .Args = {{"color", meta.ResolveComplexType("ucolor"), false}},
                                          .Ret = meta.ResolveComplexType("ucolor"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const uint32_t color = ReadWasmApiTestValue<uint32_t>(call.ArgsData[1]);
                                                  WriteWasmApiTestValue(call.RetData, color ^ uint32_t {0x00FFFFFF});
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "HexOffset",
                                          .Args = {{"hex", meta.ResolveComplexType("mpos"), false}},
                                          .Ret = meta.ResolveComplexType("mpos"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  WasmApiTestHex hex = ReadWasmApiTestValue<WasmApiTestHex>(call.ArgsData[1]);
                                                  hex.X = numeric_cast<int16_t>(hex.X + 2);
                                                  hex.Y = numeric_cast<int16_t>(hex.Y - 3);
                                                  WriteWasmApiTestValue(call.RetData, hex);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "RectArea",
                                          .Args = {{"rect", meta.ResolveComplexType("irect"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const WasmApiTestRect rect = ReadWasmApiTestValue<WasmApiTestRect>(call.ArgsData[1]);
                                                  WriteWasmApiTestValue(call.RetData, numeric_cast<int32_t>(rect.Width * rect.Height));
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetRect",
                                          .Ret = meta.ResolveComplexType("irect"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const WasmApiTestRect rect {.X = 5, .Y = 6, .Width = 7, .Height = 8};
                                                  WriteWasmApiTestValue(call.RetData, rect);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MoveRect",
                                          .Args = {{"rect", meta.ResolveComplexType("irect&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  WasmApiTestRect& rect = **cast_from_void<WasmApiTestRect**>(call.ArgsData[1]);
                                                  rect.X = numeric_cast<int32_t>(rect.X + 10);
                                                  rect.Width = numeric_cast<int32_t>(rect.Width + 1);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateText",
                                          .Args = {{"text", meta.ResolveComplexType("string&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  string& text = **cast_from_void<string**>(call.ArgsData[1]);
                                                  text += ":mut";
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateAny",
                                          .Args = {{"text", meta.ResolveComplexType("any&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  any_t& text = **cast_from_void<any_t**>(call.ArgsData[1]);
                                                  text = any_t {strex("{}:mut", text)};
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateCounter",
                                          .Args = {{"value", meta.ResolveComplexType("int32&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t& value = **cast_from_void<int32_t**>(call.ArgsData[1]);
                                                  value += 9;
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MoveHex",
                                          .Args = {{"hex", meta.ResolveComplexType("mpos&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  WasmApiTestHex& hex = **cast_from_void<WasmApiTestHex**>(call.ArgsData[1]);
                                                  hex.X = numeric_cast<int16_t>(hex.X + 4);
                                                  hex.Y = numeric_cast<int16_t>(hex.Y + 5);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "SumValues",
                                          .Args = {{"values", meta.ResolveComplexType("int32[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t sum = 0;
                                                  const size_t values_count = call.Accessor->GetArraySize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      sum += *cast_from_void<int32_t*>(call.Accessor->GetArrayElement(call.ArgsData[1], value_index));
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, sum);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateValues",
                                          .Args = {{"values", meta.ResolveComplexType("int32[]&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t sum = 0;
                                                  const size_t values_count = call.Accessor->GetArraySize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      sum += *cast_from_void<int32_t*>(call.Accessor->GetArrayElement(call.ArgsData[1], value_index));
                                                  }

                                                  call.Accessor->ClearArray(call.ArgsData[1]);

                                                  const array<int32_t, 3> output_values = {sum, numeric_cast<int32_t>(sum + 1), numeric_cast<int32_t>(sum + 2)};

                                                  for (int32_t value : output_values) {
                                                      call.Accessor->AddArrayElement(call.ArgsData[1], make_nptr(&value).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetBytes",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("uint8[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearArray(call.RetData);

                                                  for (uint8_t value : {uint8_t {4}, uint8_t {5}, uint8_t {6}}) {
                                                      call.Accessor->AddArrayElement(call.RetData, make_nptr(&value).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "TextTotalLength",
                                          .Args = {{"values", meta.ResolveComplexType("string[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t total = 0;
                                                  const size_t values_count = call.Accessor->GetArraySize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const string& value = *cast_from_void<string*>(call.Accessor->GetArrayElement(call.ArgsData[1], value_index));
                                                      total += numeric_cast<int32_t>(value.size());
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, total);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "AnyListTotalLength",
                                          .Args = {{"values", meta.ResolveComplexType("any[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t total = 0;
                                                  const size_t values_count = call.Accessor->GetArraySize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const any_t& value = *cast_from_void<any_t*>(call.Accessor->GetArrayElement(call.ArgsData[1], value_index));
                                                      total += numeric_cast<int32_t>(value.size());
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, total);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "CountConfig",
                                          .Args = {{"values", meta.ResolveComplexType("string=>string"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);
                                                  WriteWasmApiTestValue(call.RetData, numeric_cast<int32_t>(values_count));
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "CountAnyConfig",
                                          .Args = {{"values", meta.ResolveComplexType("string=>any"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);
                                                  WriteWasmApiTestValue(call.RetData, numeric_cast<int32_t>(values_count));
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetAnyConfig",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("string=>any"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.RetData);

                                                  const map<string, string> values = {{"alpha", "payload"}, {"beta", "data"}};

                                                  for (const auto& kv : values) {
                                                      string key = kv.first;
                                                      any_t value {string {kv.second}};
                                                      call.Accessor->AddDictElement(call.RetData, make_nptr(&key).void_cast(), make_nptr(&value).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "SumGroups",
                                          .Args = {{"values", meta.ResolveComplexType("string=>int32[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t sum = 0;
                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);

                                                      const size_t items_count = call.Accessor->GetNestedArraySize(kv.second);

                                                      for (size_t item_index = 0; item_index < items_count; item_index++) {
                                                          sum += *cast_from_void<int32_t*>(call.Accessor->GetNestedArrayElement(kv.second, item_index));
                                                      }
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, sum);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetGroups",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("string=>int32[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.RetData);

                                                  map<string, vector<int32_t>> values = {{"alpha", {1, 2}}, {"beta", {3, 4, 5}}};

                                                  for (auto& kv : values) {
                                                      call.Accessor->AddDictElement(call.RetData, make_nptr(&kv.first).void_cast(), make_nptr(&kv.second).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateGroups",
                                          .Args = {{"values", meta.ResolveComplexType("string=>int32[]&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t sum = 0;
                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);

                                                      const size_t items_count = call.Accessor->GetNestedArraySize(kv.second);

                                                      for (size_t item_index = 0; item_index < items_count; item_index++) {
                                                          sum += *cast_from_void<int32_t*>(call.Accessor->GetNestedArrayElement(kv.second, item_index));
                                                      }
                                                  }

                                                  call.Accessor->ClearDict(call.ArgsData[1]);

                                                  string key = "sum";
                                                  vector<int32_t> value = {sum, numeric_cast<int32_t>(sum + 1)};
                                                  call.Accessor->AddDictElement(call.ArgsData[1], make_nptr(&key).void_cast(), make_nptr(&value).void_cast());
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "CountAnyGroups",
                                          .Args = {{"values", meta.ResolveComplexType("string=>any[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t count = 0;
                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                      count += numeric_cast<int32_t>(call.Accessor->GetNestedArraySize(kv.second));
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, count);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetAnyGroups",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("string=>any[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.RetData);

                                                  map<string, vector<any_t>> values = {{"alpha", {any_t {string {"one"}}, any_t {string {"two"}}}}, {"beta", {any_t {string {"three"}}}}};

                                                  for (auto& kv : values) {
                                                      call.Accessor->AddDictElement(call.RetData, make_nptr(&kv.first).void_cast(), make_nptr(&kv.second).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "CountFlags",
                                          .Args = {{"values", meta.ResolveComplexType("string=>bool[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t count = 0;
                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                      count += numeric_cast<int32_t>(call.Accessor->GetNestedArraySize(kv.second));
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, count);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetFlags",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("string=>bool[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.RetData);

                                                  map<string, vector<bool>> values = {{"flags", {true, false, true}}};

                                                  for (auto& kv : values) {
                                                      call.Accessor->AddDictElement(call.RetData, make_nptr(&kv.first).void_cast(), make_nptr(&kv.second).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateFlags",
                                          .Args = {{"values", meta.ResolveComplexType("string=>bool[]&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.ArgsData[1]);

                                                  string key = "mutated";
                                                  vector<bool> value = {false, true};
                                                  call.Accessor->AddDictElement(call.ArgsData[1], make_nptr(&key).void_cast(), make_nptr(&value).void_cast());
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "CountColorGroups",
                                          .Args = {{"values", meta.ResolveComplexType("string=>ucolor[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t count = 0;
                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                      count += numeric_cast<int32_t>(call.Accessor->GetNestedArraySize(kv.second));
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, count);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetColorGroups",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("string=>ucolor[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.RetData);

                                                  string key = "colors";
                                                  array<ucolor, 2> colors = {ucolor {0x11223344}, ucolor {0x55667788}};
                                                  vector<ptr<void>> values;
                                                  values.reserve(colors.size());

                                                  for (ucolor& color : colors) {
                                                      values.emplace_back(make_nptr(&color).void_cast());
                                                  }

                                                  call.Accessor->AddDictArrayElement(call.RetData, make_nptr(&key).void_cast(), values);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateColorGroups",
                                          .Args = {{"values", meta.ResolveComplexType("string=>ucolor[]&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.ArgsData[1]);

                                                  string key = "mutated";
                                                  array<ucolor, 1> colors = {ucolor {0x112233AA}};
                                                  vector<ptr<void>> values;
                                                  values.reserve(colors.size());

                                                  for (ucolor& color : colors) {
                                                      values.emplace_back(make_nptr(&color).void_cast());
                                                  }

                                                  call.Accessor->AddDictArrayElement(call.ArgsData[1], make_nptr(&key).void_cast(), values);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "CountRects",
                                          .Args = {{"values", meta.ResolveComplexType("string=>irect[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t sum = 0;
                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                      const size_t items_count = call.Accessor->GetNestedArraySize(kv.second);

                                                      for (size_t item_index = 0; item_index < items_count; item_index++) {
                                                          const WasmApiTestRect rect = LoadWasmApiTestRect(call.Accessor->GetNestedArrayElement(kv.second, item_index));
                                                          sum += rect.X;
                                                          sum += rect.Y;
                                                          sum += rect.Width;
                                                          sum += rect.Height;
                                                      }
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, sum);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetRects",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("string=>irect[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.RetData);

                                                  string key = "rects";
                                                  array<WasmApiTestRect, 2> rects = {
                                                      WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4},
                                                      WasmApiTestRect {.X = 5, .Y = 6, .Width = 7, .Height = 8},
                                                  };
                                                  vector<ptr<void>> values;
                                                  values.reserve(rects.size());

                                                  for (WasmApiTestRect& rect : rects) {
                                                      values.emplace_back(make_nptr(&rect).void_cast());
                                                  }

                                                  call.Accessor->AddDictArrayElement(call.RetData, make_nptr(&key).void_cast(), values);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateRects",
                                          .Args = {{"values", meta.ResolveComplexType("string=>irect[]&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.ArgsData[1]);

                                                  string key = "mutated";
                                                  array<WasmApiTestRect, 1> rects = {
                                                      WasmApiTestRect {.X = 9, .Y = 10, .Width = 11, .Height = 12},
                                                  };
                                                  vector<ptr<void>> values;
                                                  values.reserve(rects.size());

                                                  for (WasmApiTestRect& rect : rects) {
                                                      values.emplace_back(make_nptr(&rect).void_cast());
                                                  }

                                                  call.Accessor->AddDictArrayElement(call.ArgsData[1], make_nptr(&key).void_cast(), values);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "CountRuleGroups",
                                          .Args = {{"values", meta.ResolveComplexType("string=>Rule[]"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  int32_t count = 0;
                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                      count += numeric_cast<int32_t>(call.Accessor->GetNestedArraySize(kv.second));
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, count);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetRuleGroups",
                                          .Args = {{"values", meta.ResolveComplexType("string=>Rule[]"), false}},
                                          .Ret = meta.ResolveComplexType("string=>Rule[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.RetData);

                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                      const size_t items_count = call.Accessor->GetNestedArraySize(kv.second);
                                                      vector<ptr<void>> values;
                                                      values.reserve(items_count);

                                                      for (size_t item_index = 0; item_index < items_count; item_index++) {
                                                          values.emplace_back(call.Accessor->GetNestedArrayElement(kv.second, item_index));
                                                      }

                                                      call.Accessor->AddDictArrayElement(call.RetData, kv.first, values);
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateRuleGroups",
                                          .Args = {{"values", meta.ResolveComplexType("string=>Rule[]&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  nptr<Entity> rule;
                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);

                                                      if (call.Accessor->GetNestedArraySize(kv.second) != 0) {
                                                          rule = NativeDataProvider::ReadTypedHandleSlot<Entity>(call.Accessor->GetNestedArrayElement(kv.second, 0));
                                                          break;
                                                      }
                                                  }

                                                  FO_STRONG_ASSERT(rule, "WASM test invariant failed");

                                                  call.Accessor->ClearDict(call.ArgsData[1]);

                                                  string key = "mutated";
                                                  vector<nptr<Entity>> value = {rule};
                                                  call.Accessor->AddDictElement(call.ArgsData[1], make_nptr(&key).void_cast(), make_nptr(&value).void_cast());
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetConfig",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("string=>string"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearDict(call.RetData);

                                                  const map<string, string> values = {{"alpha", "1"}, {"beta", "22"}};

                                                  for (const auto& kv : values) {
                                                      string key = kv.first;
                                                      string value = kv.second;
                                                      call.Accessor->AddDictElement(call.RetData, make_nptr(&key).void_cast(), make_nptr(&value).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateConfig",
                                          .Args = {{"values", meta.ResolveComplexType("string=>string&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);
                                                  call.Accessor->ClearDict(call.ArgsData[1]);

                                                  string key = "count";
                                                  string value = strex("{}", values_count);
                                                  call.Accessor->AddDictElement(call.ArgsData[1], make_nptr(&key).void_cast(), make_nptr(&value).void_cast());
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateTextList",
                                          .Args = {{"values", meta.ResolveComplexType("string[]&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  string joined;
                                                  const size_t values_count = call.Accessor->GetArraySize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      joined += *cast_from_void<string*>(call.Accessor->GetArrayElement(call.ArgsData[1], value_index));
                                                  }

                                                  call.Accessor->ClearArray(call.ArgsData[1]);

                                                  const array<string, 2> output_values = {joined, strex("{}!", joined)};

                                                  for (const string& value : output_values) {
                                                      call.Accessor->AddArrayElement(call.ArgsData[1], make_nptr(&value).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "MutateAnyList",
                                          .Args = {{"values", meta.ResolveComplexType("any[]&"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  string joined;
                                                  const size_t values_count = call.Accessor->GetArraySize(call.ArgsData[1]);

                                                  for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                      joined += *cast_from_void<any_t*>(call.Accessor->GetArrayElement(call.ArgsData[1], value_index));
                                                  }

                                                  call.Accessor->ClearArray(call.ArgsData[1]);

                                                  const array<any_t, 2> output_values = {any_t {joined}, any_t {strex("{}!", joined)}};

                                                  for (const any_t& value : output_values) {
                                                      call.Accessor->AddArrayElement(call.ArgsData[1], make_nptr(&value).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetTextList",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("string[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearArray(call.RetData);

                                                  for (string value : {string {"red"}, string {"green"}}) {
                                                      call.Accessor->AddArrayElement(call.RetData, make_nptr(&value).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "GetAnyList",
                                          .Args = {},
                                          .Ret = meta.ResolveComplexType("any[]"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  call.Accessor->ClearArray(call.RetData);

                                                  for (any_t value : {any_t {string {"red"}}, any_t {string {"green"}}}) {
                                                      call.Accessor->AddArrayElement(call.RetData, make_nptr(&value).void_cast());
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "InvokeCallback",
                                          .Args = {{"func", meta.ResolveComplexType("callback(int32,int32)"), false}, {"value", meta.ResolveComplexType("int32"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  ScriptFunc<int32_t, int32_t> func {call.Accessor->GetCallback(call.ArgsData[1])};
                                                  const int32_t value = *cast_from_void<int32_t*>(call.ArgsData[2]);

                                                  if (!func.Call(value)) {
                                                      throw ScriptCallException("WASM API test callback failed");
                                                  }

                                                  WriteWasmApiTestValue(call.RetData, func.GetResult());
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "InvokeTextCallback",
                                          .Args = {{"func", meta.ResolveComplexType("callback(string,string)"), false}, {"value", meta.ResolveComplexType("string"), false}},
                                          .Ret = meta.ResolveComplexType("string"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  ScriptFunc<string, string> func {call.Accessor->GetCallback(call.ArgsData[1])};
                                                  const string& value = *cast_from_void<string*>(call.ArgsData[2]);

                                                  if (!func.Call(value)) {
                                                      throw ScriptCallException("WASM API test text callback failed");
                                                  }

                                                  *cast_from_void<string*>(call.RetData) = func.GetResult();
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "InvokeNestedCallback",
                                          .Args = {{"func", meta.ResolveComplexType("callback(void,callback(int32,int32))"), false}},
                                          .Ret = {},
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  ScriptFunc<void, ScriptFunc<int32_t, int32_t>> func {call.Accessor->GetCallback(call.ArgsData[1])};

                                                  if (!func) {
                                                      throw ScriptCallException("WASM API test nested callback failed");
                                                  }
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "StringLength",
                                          .Args = {{"text", meta.ResolveComplexType("string"), false}},
                                          .Ret = meta.ResolveComplexType("int32"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const string& text = *cast_from_void<string*>(call.ArgsData[1]);
                                                  WriteWasmApiTestValue(call.RetData, numeric_cast<int32_t>(text.size()));
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "StringEcho",
                                          .Args = {{"text", meta.ResolveComplexType("string"), false}},
                                          .Ret = meta.ResolveComplexType("string"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const string& text = *cast_from_void<string*>(call.ArgsData[1]);
                                                  *cast_from_void<string*>(call.RetData) = strex("{}:echo", text);
                                              },
                                      });
                                      methods.emplace_back(MethodDesc {
                                          .Name = "AnyEcho",
                                          .Args = {{"text", meta.ResolveComplexType("any"), false}},
                                          .Ret = meta.ResolveComplexType("any"),
                                          .Call =
                                              [](FuncCallData& call) {
                                                  FO_STACK_TRACE_ENTRY();

                                                  const any_t& text = *cast_from_void<any_t*>(call.ArgsData[1]);
                                                  *cast_from_void<any_t*>(call.RetData) = any_t {strex("{}:echo", text)};
                                              },
                                      });

                                      meta.RegisterEntityMethods("Game", std::move(methods));
                                  }};

        auto callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::AddOne");
        callback_desc->Args = {{"value", engine.ResolveComplexType("int32"), false}};
        callback_desc->Ret = engine.ResolveComplexType("int32");
        callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            const int32_t value = *cast_from_void<int32_t*>(call.ArgsData[0]);
            WriteWasmApiTestValue(call.RetData, numeric_cast<int32_t>(value + 1));
        };
        callback_desc->AttributeChecker = [](string_view) noexcept { return false; };
        engine.AddGlobalScriptFunc(callback_desc.get());

        auto text_callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        text_callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::Decorate");
        text_callback_desc->Args = {{"value", engine.ResolveComplexType("string"), false}};
        text_callback_desc->Ret = engine.ResolveComplexType("string");
        text_callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            const string& value = *cast_from_void<string*>(call.ArgsData[0]);
            *cast_from_void<string*>(call.RetData) = strex("<{}>", value);
        };
        text_callback_desc->AttributeChecker = [](string_view) noexcept { return false; };
        engine.AddGlobalScriptFunc(text_callback_desc.get());

        auto nested_callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        nested_callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::AcceptNested");
        nested_callback_desc->Args = {{"func", engine.ResolveComplexType("callback(int32,int32)"), false}};
        nested_callback_desc->Ret = {};
        nested_callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            ignore_unused(call);
        };
        nested_callback_desc->AttributeChecker = [](string_view) noexcept { return false; };
        engine.AddGlobalScriptFunc(nested_callback_desc.get());

        const vector<WasmApiMethodDesc> api_methods = BuildWasmApiMethodDescs(engine);
        nptr<const WasmApiMethodDesc> rule_identity = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_RuleIdentity__Rule__Rule");
        nptr<const WasmApiMethodDesc> enum_bump = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_EnumBump__TestMode__TestMode");
        nptr<const WasmApiMethodDesc> delay_add = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_DelayAdd__timespan__timespan");
        nptr<const WasmApiMethodDesc> color_invert = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_ColorInvert__ucolor__ucolor");
        nptr<const WasmApiMethodDesc> hex_offset = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_HexOffset__mpos__mpos");
        nptr<const WasmApiMethodDesc> rect_area = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_RectArea__irect__int32");
        nptr<const WasmApiMethodDesc> get_rect = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRect__void__irect");
        nptr<const WasmApiMethodDesc> move_rect = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MoveRect__irect_mut__void");
        nptr<const WasmApiMethodDesc> mutate_text = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateText__string_mut__void");
        nptr<const WasmApiMethodDesc> mutate_any = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateAny__any_mut__void");
        nptr<const WasmApiMethodDesc> mutate_counter = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateCounter__int32_mut__void");
        nptr<const WasmApiMethodDesc> move_hex = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MoveHex__mpos_mut__void");
        nptr<const WasmApiMethodDesc> sum_values = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_SumValues__int32_array__int32");
        nptr<const WasmApiMethodDesc> mutate_values = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateValues__int32_array_mut__void");
        nptr<const WasmApiMethodDesc> get_bytes = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetBytes__void__uint8_array");
        nptr<const WasmApiMethodDesc> text_total_length = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_TextTotalLength__string_array__int32");
        nptr<const WasmApiMethodDesc> any_list_total_length = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_AnyListTotalLength__any_array__int32");
        nptr<const WasmApiMethodDesc> count_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountConfig__string_string_dict__int32");
        nptr<const WasmApiMethodDesc> count_any_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountAnyConfig__string_any_dict__int32");
        nptr<const WasmApiMethodDesc> get_any_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetAnyConfig__void__string_any_dict");
        nptr<const WasmApiMethodDesc> sum_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_SumGroups__string_int32_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetGroups__void__string_int32_array_dict");
        nptr<const WasmApiMethodDesc> mutate_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateGroups__string_int32_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_any_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountAnyGroups__string_any_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_any_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetAnyGroups__void__string_any_array_dict");
        nptr<const WasmApiMethodDesc> count_flags = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountFlags__string_bool_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_flags = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetFlags__void__string_bool_array_dict");
        nptr<const WasmApiMethodDesc> mutate_flags = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateFlags__string_bool_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_color_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountColorGroups__string_ucolor_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_color_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetColorGroups__void__string_ucolor_array_dict");
        nptr<const WasmApiMethodDesc> mutate_color_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateColorGroups__string_ucolor_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_rects = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRects__string_irect_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_rects = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRects__void__string_irect_array_dict");
        nptr<const WasmApiMethodDesc> mutate_rects = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRects__string_irect_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_rule_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRuleGroups__string_Rule_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_rule_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRuleGroups__string_Rule_array_dict__string_Rule_array_dict");
        nptr<const WasmApiMethodDesc> mutate_rule_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRuleGroups__string_Rule_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> get_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetConfig__void__string_string_dict");
        nptr<const WasmApiMethodDesc> mutate_config = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateConfig__string_string_dict_mut__void");
        nptr<const WasmApiMethodDesc> mutate_text_list = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateTextList__string_array_mut__void");
        nptr<const WasmApiMethodDesc> mutate_any_list = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateAnyList__any_array_mut__void");
        nptr<const WasmApiMethodDesc> get_text_list = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetTextList__void__string_array");
        nptr<const WasmApiMethodDesc> get_any_list = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetAnyList__void__any_array");
        nptr<const WasmApiMethodDesc> invoke_callback = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_InvokeCallback__callback_int32__int32");
        nptr<const WasmApiMethodDesc> invoke_text_callback = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_InvokeTextCallback__callback_string__string");
        nptr<const WasmApiMethodDesc> invoke_nested_callback = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_InvokeNestedCallback__callback__void");
        nptr<const WasmApiMethodDesc> string_length = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_StringLength__string__int32");
        nptr<const WasmApiMethodDesc> string_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_StringEcho__string__string");
        nptr<const WasmApiMethodDesc> any_echo = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_AnyEcho__any__any");

        REQUIRE(rule_identity);
        REQUIRE(enum_bump);
        REQUIRE(delay_add);
        REQUIRE(color_invert);
        REQUIRE(hex_offset);
        REQUIRE(rect_area);
        REQUIRE(get_rect);
        REQUIRE(move_rect);
        REQUIRE(mutate_text);
        REQUIRE(mutate_any);
        REQUIRE(mutate_counter);
        REQUIRE(move_hex);
        REQUIRE(sum_values);
        REQUIRE(mutate_values);
        REQUIRE(get_bytes);
        REQUIRE(text_total_length);
        REQUIRE(any_list_total_length);
        REQUIRE(count_config);
        REQUIRE(count_any_config);
        REQUIRE(get_any_config);
        REQUIRE(sum_groups);
        REQUIRE(get_groups);
        REQUIRE(mutate_groups);
        REQUIRE(count_any_groups);
        REQUIRE(get_any_groups);
        REQUIRE(count_flags);
        REQUIRE(get_flags);
        REQUIRE(mutate_flags);
        REQUIRE(count_color_groups);
        REQUIRE(get_color_groups);
        REQUIRE(mutate_color_groups);
        REQUIRE(count_rects);
        REQUIRE(get_rects);
        REQUIRE(mutate_rects);
        REQUIRE(count_rule_groups);
        REQUIRE(get_rule_groups);
        REQUIRE(mutate_rule_groups);
        REQUIRE(get_config);
        REQUIRE(mutate_config);
        REQUIRE(mutate_text_list);
        REQUIRE(mutate_any_list);
        REQUIRE(get_text_list);
        REQUIRE(get_any_list);
        REQUIRE(invoke_callback);
        REQUIRE(invoke_text_callback);
        REQUIRE(invoke_nested_callback);
        REQUIRE(string_length);
        REQUIRE(string_echo);
        REQUIRE(any_echo);

        uint64_t raw_result = 0;

        CHECK(rect_area->Supported);
        CHECK(rect_area->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(rect_area->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ValuePointer, WasmApiParamAbiKind::ValueByteLength});
        CHECK(rect_area->Ret == WasmScalarKind::I32);
        CHECK(MakeWasmApiNativeSignature(*rect_area) == "(*~)i");

        CHECK(get_rect->Supported);
        CHECK(get_rect->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(get_rect->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::ValueOutputPointer, WasmApiParamAbiKind::ValueOutputByteLength});
        CHECK(get_rect->Ret == WasmScalarKind::I32);
        CHECK(MakeWasmApiNativeSignature(*get_rect) == "(*~)i");

        CHECK(move_rect->Supported);
        CHECK(move_rect->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(move_rect->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::MutableValuePointer, WasmApiParamAbiKind::MutableValueLength});
        CHECK(move_rect->Ret == WasmScalarKind::None);
        CHECK(MakeWasmApiNativeSignature(*move_rect) == "(*~)");

        CHECK(mutate_text->Supported);
        CHECK(mutate_text->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_text->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::MutableUtf8StringPointer, WasmApiParamAbiKind::MutableUtf8StringByteLength, WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength, WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer});
        CHECK(mutate_text->Ret == WasmScalarKind::None);
        CHECK(MakeWasmApiNativeSignature(*mutate_text) == "(*~~*)");

        CHECK(mutate_any->Supported);
        CHECK(mutate_any->Args == vector<WasmScalarKind> {WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32, WasmScalarKind::I32});
        CHECK(mutate_any->ParamAbi == vector<WasmApiParamAbiKind> {WasmApiParamAbiKind::MutableUtf8StringPointer, WasmApiParamAbiKind::MutableUtf8StringByteLength, WasmApiParamAbiKind::MutableUtf8StringCapacityByteLength, WasmApiParamAbiKind::MutableUtf8StringRequiredByteLengthPointer});
        CHECK(mutate_any->Ret == WasmScalarKind::None);
        CHECK(MakeWasmApiNativeSignature(*mutate_any) == "(*~~*)");

        const hstring default_rule_id = engine.Hashes.ToHashedString("DefaultRule");
        const array<uint64_t, 1> rule_args = {default_rule_id.as_hash()};
        CallWasmApiMethod(engine, *rule_identity, rule_args, &raw_result);
        CHECK(raw_result == default_rule_id.as_hash());

        const array<uint64_t, 1> enum_args = {7};
        CallWasmApiMethod(engine, *enum_bump, enum_args, &raw_result);
        CHECK(raw_result == 8);

        const array<uint64_t, 1> delay_args = {1000};
        CallWasmApiMethod(engine, *delay_add, delay_args, &raw_result);
        CHECK(raw_result == 1250);

        const array<uint64_t, 1> color_args = {0x01020304};
        CallWasmApiMethod(engine, *color_invert, color_args, &raw_result);
        CHECK(raw_result == 0x01FDFCFB);

        const WasmApiTestHex input_hex {.X = 10, .Y = -5};
        const array<uint64_t, 1> hex_args = {PackWasmApiTestHex(input_hex)};
        CallWasmApiMethod(engine, *hex_offset, hex_args, &raw_result);

        const WasmApiTestHex output_hex = UnpackWasmApiTestHex(raw_result);
        CHECK(output_hex.X == 12);
        CHECK(output_hex.Y == -8);

        const WasmApiTestRect input_rect {.X = 1, .Y = 2, .Width = 3, .Height = 4};
        const array<uint64_t, 2> rect_area_args = {reinterpret_cast<uint64_t>(&input_rect), sizeof(input_rect)};
        CallWasmApiMethod(engine, *rect_area, rect_area_args, &raw_result);
        CHECK(raw_result == 12);

        WasmApiTestRect rect_output {};
        const array<uint64_t, 2> get_rect_args = {reinterpret_cast<uint64_t>(&rect_output), sizeof(rect_output)};
        CallWasmApiMethod(engine, *get_rect, get_rect_args, &raw_result);
        CHECK(raw_result == sizeof(rect_output));
        CHECK(rect_output == WasmApiTestRect {.X = 5, .Y = 6, .Width = 7, .Height = 8});

        array<uint8_t, 8> small_rect_output {};
        small_rect_output.fill(0xAA);
        const array<uint64_t, 2> get_rect_small_args = {reinterpret_cast<uint64_t>(small_rect_output.data()), small_rect_output.size()};
        CallWasmApiMethod(engine, *get_rect, get_rect_small_args, &raw_result);
        CHECK(raw_result == sizeof(WasmApiTestRect));
        const array<uint8_t, 8> unchanged_rect_output = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
        CHECK(small_rect_output == unchanged_rect_output);

        int32_t mutable_counter = 33;
        const array<uint64_t, 2> mutable_counter_args = {reinterpret_cast<uint64_t>(&mutable_counter), sizeof(mutable_counter)};
        CallWasmApiMethod(engine, *mutate_counter, mutable_counter_args, nullptr);
        CHECK(mutable_counter == 42);

        WasmApiTestRect mutable_rect {.X = -2, .Y = 3, .Width = 4, .Height = 5};
        const array<uint64_t, 2> mutable_rect_args = {reinterpret_cast<uint64_t>(&mutable_rect), sizeof(mutable_rect)};
        CallWasmApiMethod(engine, *move_rect, mutable_rect_args, nullptr);
        CHECK(mutable_rect == WasmApiTestRect {.X = 8, .Y = 3, .Width = 5, .Height = 5});

        vector<uint8_t> direct_mutable_text = {'h', 'i'};
        direct_mutable_text.resize(16);
        uint32_t direct_mutable_text_required_size = 0;
        const array<uint64_t, 4> direct_mutate_text_args = {reinterpret_cast<uint64_t>(direct_mutable_text.data()), 2, direct_mutable_text.size(), reinterpret_cast<uint64_t>(&direct_mutable_text_required_size)};
        CallWasmApiMethod(engine, *mutate_text, direct_mutate_text_args, nullptr);
        CHECK(direct_mutable_text_required_size == 6);
        CHECK(string_view {reinterpret_cast<char*>(direct_mutable_text.data()), direct_mutable_text_required_size} == "hi:mut");

        vector<uint8_t> small_direct_mutable_text = {'h', 'i'};
        uint32_t small_direct_mutable_text_required_size = 0;
        const array<uint64_t, 4> small_direct_mutate_text_args = {reinterpret_cast<uint64_t>(small_direct_mutable_text.data()), small_direct_mutable_text.size(), small_direct_mutable_text.size(), reinterpret_cast<uint64_t>(&small_direct_mutable_text_required_size)};
        CallWasmApiMethod(engine, *mutate_text, small_direct_mutate_text_args, nullptr);
        CHECK(small_direct_mutable_text_required_size == 6);
        CHECK(small_direct_mutable_text == vector<uint8_t> {'h', 'i'});

        vector<uint8_t> direct_mutable_any = {'a', 'b'};
        direct_mutable_any.resize(16);
        uint32_t direct_mutable_any_required_size = 0;
        const array<uint64_t, 4> direct_mutate_any_args = {reinterpret_cast<uint64_t>(direct_mutable_any.data()), 2, direct_mutable_any.size(), reinterpret_cast<uint64_t>(&direct_mutable_any_required_size)};
        CallWasmApiMethod(engine, *mutate_any, direct_mutate_any_args, nullptr);
        CHECK(direct_mutable_any_required_size == 6);
        CHECK(string_view {reinterpret_cast<char*>(direct_mutable_any.data()), direct_mutable_any_required_size} == "ab:mut");

        WasmApiTestHex mutable_hex {.X = -2, .Y = 3};
        const array<uint64_t, 2> mutable_hex_args = {reinterpret_cast<uint64_t>(&mutable_hex), sizeof(mutable_hex)};
        CallWasmApiMethod(engine, *move_hex, mutable_hex_args, nullptr);
        CHECK(mutable_hex.X == 2);
        CHECK(mutable_hex.Y == 8);

        const array<int32_t, 4> sum_input = {3, 4, -2, 10};
        const array<uint64_t, 2> sum_args = {reinterpret_cast<uint64_t>(sum_input.data()), sum_input.size() * sizeof(sum_input.front())};
        CallWasmApiMethod(engine, *sum_values, sum_args, &raw_result);
        CHECK(raw_result == 15);

        array<int32_t, 4> mutable_values = {4, 5, 0, 0};
        uint32_t mutable_values_required_size = 0;
        const array<uint64_t, 4> mutate_values_args = {reinterpret_cast<uint64_t>(mutable_values.data()), 2 * sizeof(mutable_values.front()), mutable_values.size() * sizeof(mutable_values.front()), reinterpret_cast<uint64_t>(&mutable_values_required_size)};
        CallWasmApiMethod(engine, *mutate_values, mutate_values_args, nullptr);
        CHECK(mutable_values_required_size == 3 * sizeof(mutable_values.front()));
        CHECK(mutable_values[0] == 9);
        CHECK(mutable_values[1] == 10);
        CHECK(mutable_values[2] == 11);
        CHECK(mutable_values[3] == 0);

        array<int32_t, 2> small_mutable_values = {4, 5};
        uint32_t small_mutable_values_required_size = 0;
        const array<uint64_t, 4> small_mutate_values_args = {reinterpret_cast<uint64_t>(small_mutable_values.data()), small_mutable_values.size() * sizeof(small_mutable_values.front()), small_mutable_values.size() * sizeof(small_mutable_values.front()), reinterpret_cast<uint64_t>(&small_mutable_values_required_size)};
        CallWasmApiMethod(engine, *mutate_values, small_mutate_values_args, nullptr);
        CHECK(small_mutable_values_required_size == 3 * sizeof(small_mutable_values.front()));
        CHECK(small_mutable_values[0] == 4);
        CHECK(small_mutable_values[1] == 5);

        array<uint8_t, 2> bytes_output {};
        const array<uint64_t, 2> get_bytes_args = {reinterpret_cast<uint64_t>(bytes_output.data()), bytes_output.size()};
        CallWasmApiMethod(engine, *get_bytes, get_bytes_args, &raw_result);
        CHECK(raw_result == 3);
        CHECK(bytes_output[0] == 4);
        CHECK(bytes_output[1] == 5);

        const vector<uint8_t> text_input = MakeWasmApiTestStringArrayBlob({"ab", "cde", ""});
        const array<uint64_t, 2> text_total_args = {reinterpret_cast<uint64_t>(text_input.data()), text_input.size()};
        CallWasmApiMethod(engine, *text_total_length, text_total_args, &raw_result);
        CHECK(raw_result == 5);

        const vector<uint8_t> any_list_input = MakeWasmApiTestStringArrayBlob({"aa", "bbb", "cccc"});
        const array<uint64_t, 2> any_list_total_args = {reinterpret_cast<uint64_t>(any_list_input.data()), any_list_input.size()};
        CallWasmApiMethod(engine, *any_list_total_length, any_list_total_args, &raw_result);
        CHECK(raw_result == 9);

        const vector<uint8_t> dict_input = MakeWasmApiTestStringDictBlob({{"one", "1"}, {"two", "22"}});
        const array<uint64_t, 2> count_config_args = {reinterpret_cast<uint64_t>(dict_input.data()), dict_input.size()};
        CallWasmApiMethod(engine, *count_config, count_config_args, &raw_result);
        CHECK(raw_result == 2);

        const vector<uint8_t> any_dict_input = MakeWasmApiTestStringDictBlob({{"one", "alpha"}, {"two", "beta"}, {"three", "gamma"}});
        const array<uint64_t, 2> count_any_config_args = {reinterpret_cast<uint64_t>(any_dict_input.data()), any_dict_input.size()};
        CallWasmApiMethod(engine, *count_any_config, count_any_config_args, &raw_result);
        CHECK(raw_result == 3);

        const vector<uint8_t> any_dict_expected = MakeWasmApiTestStringDictBlob({{"alpha", "payload"}, {"beta", "data"}});
        vector<uint8_t> any_dict_output(64);
        const array<uint64_t, 2> get_any_config_args = {reinterpret_cast<uint64_t>(any_dict_output.data()), any_dict_output.size()};
        CallWasmApiMethod(engine, *get_any_config, get_any_config_args, &raw_result);
        CHECK(raw_result == any_dict_expected.size());

        any_dict_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringDictBlob(any_dict_output) == map<string, string> {{"alpha", "payload"}, {"beta", "data"}});

        const vector<uint8_t> groups_input = MakeWasmApiTestStringIntArrayDictBlob({{"first", {1, 2, 3}}, {"second", {4, -1}}});
        const array<uint64_t, 2> sum_groups_args = {reinterpret_cast<uint64_t>(groups_input.data()), groups_input.size()};
        CallWasmApiMethod(engine, *sum_groups, sum_groups_args, &raw_result);
        CHECK(raw_result == 9);

        const vector<uint8_t> groups_expected = MakeWasmApiTestStringIntArrayDictBlob({{"alpha", {1, 2}}, {"beta", {3, 4, 5}}});
        vector<uint8_t> groups_output(96);
        const array<uint64_t, 2> get_groups_args = {reinterpret_cast<uint64_t>(groups_output.data()), groups_output.size()};
        CallWasmApiMethod(engine, *get_groups, get_groups_args, &raw_result);
        CHECK(raw_result == groups_expected.size());

        groups_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringIntArrayDictBlob(groups_output) == map<string, vector<int32_t>> {{"alpha", {1, 2}}, {"beta", {3, 4, 5}}});

        const vector<uint8_t> mutable_groups_expected = MakeWasmApiTestStringIntArrayDictBlob({{"sum", {9, 10}}});
        vector<uint8_t> mutable_groups_buffer = groups_input;
        mutable_groups_buffer.resize(96);
        uint32_t mutable_groups_required_size = 0;
        const array<uint64_t, 4> mutate_groups_args = {reinterpret_cast<uint64_t>(mutable_groups_buffer.data()), groups_input.size(), mutable_groups_buffer.size(), reinterpret_cast<uint64_t>(&mutable_groups_required_size)};
        CallWasmApiMethod(engine, *mutate_groups, mutate_groups_args, nullptr);
        CHECK(mutable_groups_required_size == mutable_groups_expected.size());

        vector<uint8_t> mutable_groups_output(mutable_groups_buffer.begin(), mutable_groups_buffer.begin() + mutable_groups_required_size);
        CHECK(ReadWasmApiTestStringIntArrayDictBlob(mutable_groups_output) == map<string, vector<int32_t>> {{"sum", {9, 10}}});

        const vector<uint8_t> any_groups_input = MakeWasmApiTestStringStringArrayDictBlob({{"first", {string {"one"}, string {"two"}}}, {"second", {string {"three"}}}});
        const array<uint64_t, 2> count_any_groups_args = {reinterpret_cast<uint64_t>(any_groups_input.data()), any_groups_input.size()};
        CallWasmApiMethod(engine, *count_any_groups, count_any_groups_args, &raw_result);
        CHECK(raw_result == 3);

        const vector<uint8_t> any_groups_expected = MakeWasmApiTestStringStringArrayDictBlob({{"alpha", {string {"one"}, string {"two"}}}, {"beta", {string {"three"}}}});
        vector<uint8_t> any_groups_output(96);
        const array<uint64_t, 2> get_any_groups_args = {reinterpret_cast<uint64_t>(any_groups_output.data()), any_groups_output.size()};
        CallWasmApiMethod(engine, *get_any_groups, get_any_groups_args, &raw_result);
        CHECK(raw_result == any_groups_expected.size());

        any_groups_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringStringArrayDictBlob(any_groups_output) == map<string, vector<string>> {{"alpha", {"one", "two"}}, {"beta", {"three"}}});

        const vector<uint8_t> flags_input = MakeWasmApiTestStringBoolArrayDictBlob({{"flags", {true, false, true}}, {"other", {false}}});
        const array<uint64_t, 2> count_flags_args = {reinterpret_cast<uint64_t>(flags_input.data()), flags_input.size()};
        CallWasmApiMethod(engine, *count_flags, count_flags_args, &raw_result);
        CHECK(raw_result == 4);

        const vector<uint8_t> flags_expected = MakeWasmApiTestStringBoolArrayDictBlob({{"flags", {true, false, true}}});
        vector<uint8_t> flags_output(96);
        const array<uint64_t, 2> get_flags_args = {reinterpret_cast<uint64_t>(flags_output.data()), flags_output.size()};
        CallWasmApiMethod(engine, *get_flags, get_flags_args, &raw_result);
        CHECK(raw_result == flags_expected.size());

        flags_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringBoolArrayDictBlob(flags_output) == map<string, vector<bool>> {{"flags", {true, false, true}}});

        const vector<uint8_t> mutable_flags_expected = MakeWasmApiTestStringBoolArrayDictBlob({{"mutated", {false, true}}});
        vector<uint8_t> mutable_flags_buffer = flags_input;
        mutable_flags_buffer.resize(96);
        uint32_t mutable_flags_required_size = 0;
        const array<uint64_t, 4> mutate_flags_args = {reinterpret_cast<uint64_t>(mutable_flags_buffer.data()), flags_input.size(), mutable_flags_buffer.size(), reinterpret_cast<uint64_t>(&mutable_flags_required_size)};
        CallWasmApiMethod(engine, *mutate_flags, mutate_flags_args, nullptr);
        CHECK(mutable_flags_required_size == mutable_flags_expected.size());

        vector<uint8_t> mutable_flags_output(mutable_flags_buffer.begin(), mutable_flags_buffer.begin() + mutable_flags_required_size);
        CHECK(ReadWasmApiTestStringBoolArrayDictBlob(mutable_flags_output) == map<string, vector<bool>> {{"mutated", {false, true}}});

        const vector<uint8_t> color_groups_input = MakeWasmApiTestStringU32ArrayDictBlob({{"colors", {0x11223344, 0x55667788}}});
        const array<uint64_t, 2> count_color_groups_args = {reinterpret_cast<uint64_t>(color_groups_input.data()), color_groups_input.size()};
        CallWasmApiMethod(engine, *count_color_groups, count_color_groups_args, &raw_result);
        CHECK(raw_result == 2);

        const vector<uint8_t> color_groups_expected = MakeWasmApiTestStringU32ArrayDictBlob({{"colors", {0x11223344, 0x55667788}}});
        vector<uint8_t> color_groups_output(96);
        const array<uint64_t, 2> get_color_groups_args = {reinterpret_cast<uint64_t>(color_groups_output.data()), color_groups_output.size()};
        CallWasmApiMethod(engine, *get_color_groups, get_color_groups_args, &raw_result);
        CHECK(raw_result == color_groups_expected.size());

        color_groups_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringU32ArrayDictBlob(color_groups_output) == map<string, vector<uint32_t>> {{"colors", {0x11223344, 0x55667788}}});

        const vector<uint8_t> mutable_color_groups_expected = MakeWasmApiTestStringU32ArrayDictBlob({{"mutated", {0x112233AA}}});
        vector<uint8_t> mutable_color_groups_buffer = color_groups_input;
        mutable_color_groups_buffer.resize(96);
        uint32_t mutable_color_groups_required_size = 0;
        const array<uint64_t, 4> mutate_color_groups_args = {reinterpret_cast<uint64_t>(mutable_color_groups_buffer.data()), color_groups_input.size(), mutable_color_groups_buffer.size(), reinterpret_cast<uint64_t>(&mutable_color_groups_required_size)};
        CallWasmApiMethod(engine, *mutate_color_groups, mutate_color_groups_args, nullptr);
        CHECK(mutable_color_groups_required_size == mutable_color_groups_expected.size());

        vector<uint8_t> mutable_color_groups_output(mutable_color_groups_buffer.begin(), mutable_color_groups_buffer.begin() + mutable_color_groups_required_size);
        CHECK(ReadWasmApiTestStringU32ArrayDictBlob(mutable_color_groups_output) == map<string, vector<uint32_t>> {{"mutated", {0x112233AA}}});

        const vector<uint8_t> rects_input = MakeWasmApiTestStringRectArrayDictBlob({{"rects", {WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = -1, .Y = 2, .Width = -3, .Height = 4}}}});
        const array<uint64_t, 2> count_rects_args = {reinterpret_cast<uint64_t>(rects_input.data()), rects_input.size()};
        CallWasmApiMethod(engine, *count_rects, count_rects_args, &raw_result);
        CHECK(raw_result == 12);

        const vector<uint8_t> rects_expected = MakeWasmApiTestStringRectArrayDictBlob({{"rects", {WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = 5, .Y = 6, .Width = 7, .Height = 8}}}});
        vector<uint8_t> rects_output(128);
        const array<uint64_t, 2> get_rects_args = {reinterpret_cast<uint64_t>(rects_output.data()), rects_output.size()};
        CallWasmApiMethod(engine, *get_rects, get_rects_args, &raw_result);
        CHECK(raw_result == rects_expected.size());

        rects_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringRectArrayDictBlob(rects_output) == map<string, vector<WasmApiTestRect>> {{"rects", {WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = 5, .Y = 6, .Width = 7, .Height = 8}}}});

        const vector<uint8_t> mutable_rects_expected = MakeWasmApiTestStringRectArrayDictBlob({{"mutated", {WasmApiTestRect {.X = 9, .Y = 10, .Width = 11, .Height = 12}}}});
        vector<uint8_t> mutable_rects_buffer = rects_input;
        mutable_rects_buffer.resize(128);
        uint32_t mutable_rects_required_size = 0;
        const array<uint64_t, 4> mutate_rects_args = {reinterpret_cast<uint64_t>(mutable_rects_buffer.data()), rects_input.size(), mutable_rects_buffer.size(), reinterpret_cast<uint64_t>(&mutable_rects_required_size)};
        CallWasmApiMethod(engine, *mutate_rects, mutate_rects_args, nullptr);
        CHECK(mutable_rects_required_size == mutable_rects_expected.size());

        vector<uint8_t> mutable_rects_output(mutable_rects_buffer.begin(), mutable_rects_buffer.begin() + mutable_rects_required_size);
        CHECK(ReadWasmApiTestStringRectArrayDictBlob(mutable_rects_output) == map<string, vector<WasmApiTestRect>> {{"mutated", {WasmApiTestRect {.X = 9, .Y = 10, .Width = 11, .Height = 12}}}});

        const vector<uint8_t> rule_groups_input = MakeWasmApiTestStringU64ArrayDictBlob({{"rules", {default_rule_id.as_hash(), default_rule_id.as_hash()}}});
        const array<uint64_t, 2> count_rule_groups_args = {reinterpret_cast<uint64_t>(rule_groups_input.data()), rule_groups_input.size()};
        CallWasmApiMethod(engine, *count_rule_groups, count_rule_groups_args, &raw_result);
        CHECK(raw_result == 2);

        const vector<uint8_t> rule_groups_expected = MakeWasmApiTestStringU64ArrayDictBlob({{"rules", {default_rule_id.as_hash(), default_rule_id.as_hash()}}});
        vector<uint8_t> rule_groups_output(96);
        const array<uint64_t, 4> get_rule_groups_args = {reinterpret_cast<uint64_t>(rule_groups_input.data()), rule_groups_input.size(), reinterpret_cast<uint64_t>(rule_groups_output.data()), rule_groups_output.size()};
        CallWasmApiMethod(engine, *get_rule_groups, get_rule_groups_args, &raw_result);
        CHECK(raw_result == rule_groups_expected.size());

        rule_groups_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringU64ArrayDictBlob(rule_groups_output) == map<string, vector<uint64_t>> {{"rules", {default_rule_id.as_hash(), default_rule_id.as_hash()}}});

        const vector<uint8_t> mutable_rule_groups_expected = MakeWasmApiTestStringU64ArrayDictBlob({{"mutated", {default_rule_id.as_hash()}}});
        vector<uint8_t> mutable_rule_groups_buffer = rule_groups_input;
        mutable_rule_groups_buffer.resize(96);
        uint32_t mutable_rule_groups_required_size = 0;
        const array<uint64_t, 4> mutate_rule_groups_args = {reinterpret_cast<uint64_t>(mutable_rule_groups_buffer.data()), rule_groups_input.size(), mutable_rule_groups_buffer.size(), reinterpret_cast<uint64_t>(&mutable_rule_groups_required_size)};
        CallWasmApiMethod(engine, *mutate_rule_groups, mutate_rule_groups_args, nullptr);
        CHECK(mutable_rule_groups_required_size == mutable_rule_groups_expected.size());

        vector<uint8_t> mutable_rule_groups_output(mutable_rule_groups_buffer.begin(), mutable_rule_groups_buffer.begin() + mutable_rule_groups_required_size);
        CHECK(ReadWasmApiTestStringU64ArrayDictBlob(mutable_rule_groups_output) == map<string, vector<uint64_t>> {{"mutated", {default_rule_id.as_hash()}}});

        const vector<uint8_t> dict_expected = MakeWasmApiTestStringDictBlob({{"alpha", "1"}, {"beta", "22"}});
        vector<uint8_t> dict_output(64);
        const array<uint64_t, 2> get_config_args = {reinterpret_cast<uint64_t>(dict_output.data()), dict_output.size()};
        CallWasmApiMethod(engine, *get_config, get_config_args, &raw_result);
        CHECK(raw_result == dict_expected.size());

        dict_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringDictBlob(dict_output) == map<string, string> {{"alpha", "1"}, {"beta", "22"}});

        vector<uint8_t> small_dict_output(8, 0xAA);
        const vector<uint8_t> small_dict_output_before = small_dict_output;
        const array<uint64_t, 2> small_get_config_args = {reinterpret_cast<uint64_t>(small_dict_output.data()), small_dict_output.size()};
        CallWasmApiMethod(engine, *get_config, small_get_config_args, &raw_result);
        CHECK(raw_result == dict_expected.size());
        CHECK(small_dict_output == small_dict_output_before);

        const vector<uint8_t> mutable_dict_expected = MakeWasmApiTestStringDictBlob({{"count", "2"}});
        vector<uint8_t> mutable_dict_buffer = dict_input;
        mutable_dict_buffer.resize(64);
        uint32_t mutable_dict_required_size = 0;
        const array<uint64_t, 4> mutate_dict_args = {reinterpret_cast<uint64_t>(mutable_dict_buffer.data()), dict_input.size(), mutable_dict_buffer.size(), reinterpret_cast<uint64_t>(&mutable_dict_required_size)};
        CallWasmApiMethod(engine, *mutate_config, mutate_dict_args, nullptr);
        CHECK(mutable_dict_required_size == mutable_dict_expected.size());

        vector<uint8_t> mutable_dict_output(mutable_dict_buffer.begin(), mutable_dict_buffer.begin() + mutable_dict_required_size);
        CHECK(ReadWasmApiTestStringDictBlob(mutable_dict_output) == map<string, string> {{"count", "2"}});

        const vector<uint8_t> empty_dict_input = MakeWasmApiTestStringDictBlob({});
        vector<uint8_t> small_mutable_dict_buffer = empty_dict_input;
        uint32_t small_mutable_dict_required_size = 0;
        const array<uint64_t, 4> small_mutate_dict_args = {reinterpret_cast<uint64_t>(small_mutable_dict_buffer.data()), empty_dict_input.size(), empty_dict_input.size(), reinterpret_cast<uint64_t>(&small_mutable_dict_required_size)};
        CallWasmApiMethod(engine, *mutate_config, small_mutate_dict_args, nullptr);
        CHECK(small_mutable_dict_required_size == MakeWasmApiTestStringDictBlob({{"count", "0"}}).size());
        CHECK(small_mutable_dict_buffer == empty_dict_input);

        const vector<uint8_t> mutable_text_input = MakeWasmApiTestStringArrayBlob({"ab", "cd"});
        const vector<uint8_t> mutable_text_expected = MakeWasmApiTestStringArrayBlob({"abcd", "abcd!"});
        vector<uint8_t> mutable_text_buffer = mutable_text_input;
        mutable_text_buffer.resize(64);
        uint32_t mutable_text_required_size = 0;
        const array<uint64_t, 4> mutate_text_args = {reinterpret_cast<uint64_t>(mutable_text_buffer.data()), mutable_text_input.size(), mutable_text_buffer.size(), reinterpret_cast<uint64_t>(&mutable_text_required_size)};
        CallWasmApiMethod(engine, *mutate_text_list, mutate_text_args, nullptr);
        CHECK(mutable_text_required_size == mutable_text_expected.size());

        vector<uint8_t> mutable_text_output(mutable_text_buffer.begin(), mutable_text_buffer.begin() + mutable_text_required_size);
        CHECK(ReadWasmApiTestStringArrayBlob(mutable_text_output) == vector<string> {"abcd", "abcd!"});

        vector<uint8_t> small_mutable_text_buffer = mutable_text_input;
        uint32_t small_mutable_text_required_size = 0;
        const array<uint64_t, 4> small_mutate_text_args = {reinterpret_cast<uint64_t>(small_mutable_text_buffer.data()), mutable_text_input.size(), small_mutable_text_buffer.size(), reinterpret_cast<uint64_t>(&small_mutable_text_required_size)};
        CallWasmApiMethod(engine, *mutate_text_list, small_mutate_text_args, nullptr);
        CHECK(small_mutable_text_required_size == mutable_text_expected.size());
        CHECK(small_mutable_text_buffer == mutable_text_input);

        const vector<uint8_t> mutable_any_input = MakeWasmApiTestStringArrayBlob({"xy", "z"});
        const vector<uint8_t> mutable_any_expected = MakeWasmApiTestStringArrayBlob({"xyz", "xyz!"});
        vector<uint8_t> mutable_any_buffer = mutable_any_input;
        mutable_any_buffer.resize(64);
        uint32_t mutable_any_required_size = 0;
        const array<uint64_t, 4> mutate_any_args = {reinterpret_cast<uint64_t>(mutable_any_buffer.data()), mutable_any_input.size(), mutable_any_buffer.size(), reinterpret_cast<uint64_t>(&mutable_any_required_size)};
        CallWasmApiMethod(engine, *mutate_any_list, mutate_any_args, nullptr);
        CHECK(mutable_any_required_size == mutable_any_expected.size());

        vector<uint8_t> mutable_any_output(mutable_any_buffer.begin(), mutable_any_buffer.begin() + mutable_any_required_size);
        CHECK(ReadWasmApiTestStringArrayBlob(mutable_any_output) == vector<string> {"xyz", "xyz!"});

        vector<uint8_t> text_list_output(32);
        const array<uint64_t, 2> get_text_list_args = {reinterpret_cast<uint64_t>(text_list_output.data()), text_list_output.size()};
        CallWasmApiMethod(engine, *get_text_list, get_text_list_args, &raw_result);
        text_list_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringArrayBlob(text_list_output) == vector<string> {"red", "green"});

        vector<uint8_t> any_list_output(32);
        const array<uint64_t, 2> get_any_list_args = {reinterpret_cast<uint64_t>(any_list_output.data()), any_list_output.size()};
        CallWasmApiMethod(engine, *get_any_list, get_any_list_args, &raw_result);
        any_list_output.resize(numeric_cast<size_t>(raw_result));
        CHECK(ReadWasmApiTestStringArrayBlob(any_list_output) == vector<string> {"red", "green"});

        const string callback_name = "CallbackModule::AddOne";
        const array<uint64_t, 3> invoke_callback_args = {reinterpret_cast<uint64_t>(callback_name.data()), callback_name.size(), 41};
        CallWasmApiMethod(engine, *invoke_callback, invoke_callback_args, &raw_result);
        CHECK(raw_result == 42);

        auto delegate_callback_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();
        delegate_callback_desc->Name = engine.Hashes.ToHashedString("CallbackModule::DelegateAdd");
        delegate_callback_desc->Args = {{"value", engine.ResolveComplexType("int32"), false}};
        delegate_callback_desc->Ret = engine.ResolveComplexType("int32");
        delegate_callback_desc->DelegateObj = 1;
        delegate_callback_desc->Call = [](FuncCallData& call) {
            FO_STACK_TRACE_ENTRY();

            const int32_t value = *cast_from_void<int32_t*>(call.ArgsData[0]);
            WriteWasmApiTestValue(call.RetData, numeric_cast<int32_t>(value + 7));
        };
        delegate_callback_desc->AttributeChecker = [](string_view) noexcept { return false; };

        const size_t temp_callback_scope = engine.PushTemporaryScriptCallbackScope();
        const auto pop_temp_callback_scope = scope_exit([&engine, temp_callback_scope]() noexcept { engine.PopTemporaryScriptCallbackScope(temp_callback_scope); });
        const string delegate_callback_token = engine.RegisterTemporaryScriptCallback(make_unique_del_ptr(delegate_callback_desc.release(), [](ScriptFuncDesc* ptr) { delete ptr; }));
        CHECK(ScriptSystem::IsTemporaryScriptCallbackToken(delegate_callback_token));

        const array<uint64_t, 3> invoke_delegate_callback_args = {reinterpret_cast<uint64_t>(delegate_callback_token.data()), delegate_callback_token.size(), 35};
        CallWasmApiMethod(engine, *invoke_callback, invoke_delegate_callback_args, &raw_result);
        CHECK(raw_result == 42);

        const string text_callback_name = "CallbackModule::Decorate";
        const string callback_text = "snow";
        const string expected_callback_text = "<snow>";
        array<char, 64> callback_text_buffer {};
        const array<uint64_t, 6> invoke_text_callback_args = {reinterpret_cast<uint64_t>(text_callback_name.data()), text_callback_name.size(), reinterpret_cast<uint64_t>(callback_text.data()), callback_text.size(), reinterpret_cast<uint64_t>(callback_text_buffer.data()), callback_text_buffer.size()};
        CallWasmApiMethod(engine, *invoke_text_callback, invoke_text_callback_args, &raw_result);
        CHECK(raw_result == expected_callback_text.size());
        CHECK(string_view {callback_text_buffer.data(), expected_callback_text.size()} == expected_callback_text);

        const string nested_callback_name = "CallbackModule::AcceptNested";
        const array<uint64_t, 2> invoke_nested_callback_args = {reinterpret_cast<uint64_t>(nested_callback_name.data()), nested_callback_name.size()};
        CallWasmApiMethod(engine, *invoke_nested_callback, invoke_nested_callback_args, nullptr);

        const string input_text = "hello wasm api";
        const array<uint64_t, 2> string_args = {reinterpret_cast<uint64_t>(input_text.data()), input_text.size()};
        CallWasmApiMethod(engine, *string_length, string_args, &raw_result);
        CHECK(raw_result == input_text.size());

        const string expected_echo = "hello wasm api:echo";
        array<char, 64> echo_buffer {};
        const array<uint64_t, 4> string_echo_args = {reinterpret_cast<uint64_t>(input_text.data()), input_text.size(), reinterpret_cast<uint64_t>(echo_buffer.data()), echo_buffer.size()};
        CallWasmApiMethod(engine, *string_echo, string_echo_args, &raw_result);
        CHECK(raw_result == expected_echo.size());
        CHECK(string_view {echo_buffer.data(), expected_echo.size()} == expected_echo);

        const string input_any = "payload wasm api";
        const string expected_any = "payload wasm api:echo";
        array<char, 64> any_buffer {};
        const array<uint64_t, 4> any_echo_args = {reinterpret_cast<uint64_t>(input_any.data()), input_any.size(), reinterpret_cast<uint64_t>(any_buffer.data()), any_buffer.size()};
        CallWasmApiMethod(engine, *any_echo, any_echo_args, &raw_result);
        CHECK(raw_result == expected_any.size());
        CHECK(string_view {any_buffer.data(), expected_any.size()} == expected_any);

        const vector<WasmApiPropertyDesc> api_properties = BuildWasmApiPropertyDescs(engine);
        nptr<const WasmApiPropertyDesc> mode_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Mode__TestMode");
        nptr<const WasmApiPropertyDesc> mode_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Mode__TestMode");
        nptr<const WasmApiPropertyDesc> cooldown_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Cooldown__timespan");
        nptr<const WasmApiPropertyDesc> cooldown_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Cooldown__timespan");
        nptr<const WasmApiPropertyDesc> tint_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Tint__ucolor");
        nptr<const WasmApiPropertyDesc> tint_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Tint__ucolor");
        nptr<const WasmApiPropertyDesc> spawn_hex_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_SpawnHex__mpos");
        nptr<const WasmApiPropertyDesc> spawn_hex_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_SpawnHex__mpos");
        nptr<const WasmApiPropertyDesc> viewport_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Viewport__irect");
        nptr<const WasmApiPropertyDesc> viewport_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Viewport__irect");
        nptr<const WasmApiPropertyDesc> title_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Title__string");
        nptr<const WasmApiPropertyDesc> title_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Title__string");
        nptr<const WasmApiPropertyDesc> payload_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Payload__any");
        nptr<const WasmApiPropertyDesc> payload_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Payload__any");
        nptr<const WasmApiPropertyDesc> scores_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Scores__int32_array");
        nptr<const WasmApiPropertyDesc> scores_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Scores__int32_array");
        nptr<const WasmApiPropertyDesc> rects_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Rects__irect_array");
        nptr<const WasmApiPropertyDesc> rects_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Rects__irect_array");
        nptr<const WasmApiPropertyDesc> tags_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Tags__string_array");
        nptr<const WasmApiPropertyDesc> tags_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Tags__string_array");
        nptr<const WasmApiPropertyDesc> payloads_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Payloads__any_array");
        nptr<const WasmApiPropertyDesc> payloads_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Payloads__any_array");
        nptr<const WasmApiPropertyDesc> config_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Config__string_string_dict");
        nptr<const WasmApiPropertyDesc> config_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Config__string_string_dict");
        nptr<const WasmApiPropertyDesc> any_config_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_AnyConfig__string_any_dict");
        nptr<const WasmApiPropertyDesc> any_config_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_AnyConfig__string_any_dict");
        nptr<const WasmApiPropertyDesc> groups_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_Groups__string_int32_array_dict");
        nptr<const WasmApiPropertyDesc> groups_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_Groups__string_int32_array_dict");
        nptr<const WasmApiPropertyDesc> rect_groups_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_RectGroups__string_irect_array_dict");
        nptr<const WasmApiPropertyDesc> rect_groups_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_RectGroups__string_irect_array_dict");
        nptr<const WasmApiPropertyDesc> any_groups_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_get_AnyGroups__string_any_array_dict");
        nptr<const WasmApiPropertyDesc> any_groups_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Game_set_AnyGroups__string_any_array_dict");
        nptr<const WasmApiPropertyDesc> rule_score_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "Rule_get_Score__hstring_int32");

        REQUIRE(mode_get);
        REQUIRE(mode_set);
        REQUIRE(cooldown_get);
        REQUIRE(cooldown_set);
        REQUIRE(tint_get);
        REQUIRE(tint_set);
        REQUIRE(spawn_hex_get);
        REQUIRE(spawn_hex_set);
        REQUIRE(viewport_get);
        REQUIRE(viewport_set);
        REQUIRE(title_get);
        REQUIRE(title_set);
        REQUIRE(payload_get);
        REQUIRE(payload_set);
        REQUIRE(scores_get);
        REQUIRE(scores_set);
        REQUIRE(rects_get);
        REQUIRE(rects_set);
        REQUIRE(tags_get);
        REQUIRE(tags_set);
        REQUIRE(payloads_get);
        REQUIRE(payloads_set);
        REQUIRE(config_get);
        REQUIRE(config_set);
        REQUIRE(any_config_get);
        REQUIRE(any_config_set);
        REQUIRE(groups_get);
        REQUIRE(groups_set);
        REQUIRE(rect_groups_get);
        REQUIRE(rect_groups_set);
        REQUIRE(any_groups_get);
        REQUIRE(any_groups_set);
        REQUIRE(rule_score_get);

        CHECK(viewport_get->Supported);
        CHECK(viewport_get->ArgsCount == 2);
        CHECK(viewport_get->ParamAbi[0] == WasmApiParamAbiKind::ValueOutputPointer);
        CHECK(viewport_get->ParamAbi[1] == WasmApiParamAbiKind::ValueOutputByteLength);
        CHECK(viewport_get->Ret == WasmScalarKind::I32);
        CHECK(MakeWasmApiPropertyNativeSignature(*viewport_get) == "(*~)i");

        CHECK(viewport_set->Supported);
        CHECK(viewport_set->ArgsCount == 2);
        CHECK(viewport_set->ParamAbi[0] == WasmApiParamAbiKind::ValuePointer);
        CHECK(viewport_set->ParamAbi[1] == WasmApiParamAbiKind::ValueByteLength);
        CHECK(viewport_set->Ret == WasmScalarKind::None);
        CHECK(MakeWasmApiPropertyNativeSignature(*viewport_set) == "(*~)");

        CallWasmApiProperty(engine, *rule_score_get, rule_args, &raw_result);
        CHECK(raw_result == 77);

        const array<uint64_t, 0> no_args {};
        const array<uint64_t, 1> mode_set_args = {7};
        CallWasmApiProperty(engine, *mode_set, mode_set_args, nullptr);
        CallWasmApiProperty(engine, *mode_get, no_args, &raw_result);
        CHECK(raw_result == 7);

        const array<uint64_t, 1> cooldown_set_args = {3000};
        CallWasmApiProperty(engine, *cooldown_set, cooldown_set_args, nullptr);
        CallWasmApiProperty(engine, *cooldown_get, no_args, &raw_result);
        CHECK(raw_result == 3000);

        const array<uint64_t, 1> tint_set_args = {0x11223344};
        CallWasmApiProperty(engine, *tint_set, tint_set_args, nullptr);
        CallWasmApiProperty(engine, *tint_get, no_args, &raw_result);
        CHECK(raw_result == 0x11223344);

        const WasmApiTestHex property_hex {.X = -20, .Y = 30};
        const array<uint64_t, 1> spawn_hex_set_args = {PackWasmApiTestHex(property_hex)};
        CallWasmApiProperty(engine, *spawn_hex_set, spawn_hex_set_args, nullptr);
        CallWasmApiProperty(engine, *spawn_hex_get, no_args, &raw_result);

        const WasmApiTestHex stored_hex = UnpackWasmApiTestHex(raw_result);
        CHECK(stored_hex.X == -20);
        CHECK(stored_hex.Y == 30);

        const WasmApiTestRect property_rect {.X = -1, .Y = 2, .Width = 30, .Height = 40};
        const array<uint64_t, 2> viewport_set_args = {reinterpret_cast<uint64_t>(&property_rect), sizeof(property_rect)};
        CallWasmApiProperty(engine, *viewport_set, viewport_set_args, nullptr);

        WasmApiTestRect viewport_buffer {};
        const array<uint64_t, 2> viewport_get_args = {reinterpret_cast<uint64_t>(&viewport_buffer), sizeof(viewport_buffer)};
        CallWasmApiProperty(engine, *viewport_get, viewport_get_args, &raw_result);
        CHECK(raw_result == sizeof(viewport_buffer));
        CHECK(viewport_buffer == property_rect);

        const string title = "WASM title";
        const array<uint64_t, 2> title_set_args = {reinterpret_cast<uint64_t>(title.data()), title.size()};
        CallWasmApiProperty(engine, *title_set, title_set_args, nullptr);

        array<char, 32> title_buffer {};
        const array<uint64_t, 2> title_get_args = {reinterpret_cast<uint64_t>(title_buffer.data()), title_buffer.size()};
        CallWasmApiProperty(engine, *title_get, title_get_args, &raw_result);
        CHECK(raw_result == title.size());
        CHECK(string_view {title_buffer.data(), title.size()} == title);

        const string payload = "WASM payload";
        const array<uint64_t, 2> payload_set_args = {reinterpret_cast<uint64_t>(payload.data()), payload.size()};
        CallWasmApiProperty(engine, *payload_set, payload_set_args, nullptr);

        array<char, 32> payload_buffer {};
        const array<uint64_t, 2> payload_get_args = {reinterpret_cast<uint64_t>(payload_buffer.data()), payload_buffer.size()};
        CallWasmApiProperty(engine, *payload_get, payload_get_args, &raw_result);
        CHECK(raw_result == payload.size());
        CHECK(string_view {payload_buffer.data(), payload.size()} == payload);

        const array<int32_t, 3> scores = {3, 4, 5};
        const array<uint64_t, 2> scores_set_args = {reinterpret_cast<uint64_t>(scores.data()), scores.size() * sizeof(scores.front())};
        CallWasmApiProperty(engine, *scores_set, scores_set_args, nullptr);

        array<int32_t, 3> scores_buffer {};
        const array<uint64_t, 2> scores_get_args = {reinterpret_cast<uint64_t>(scores_buffer.data()), sizeof(scores_buffer)};
        CallWasmApiProperty(engine, *scores_get, scores_get_args, &raw_result);
        CHECK(raw_result == sizeof(scores));
        CHECK(scores_buffer == scores);

        const vector<uint8_t> rects = MakeWasmApiTestRectArrayBlob({WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = -5, .Y = 6, .Width = -7, .Height = 8}});
        const array<uint64_t, 2> rects_set_args = {reinterpret_cast<uint64_t>(rects.data()), rects.size()};
        CallWasmApiProperty(engine, *rects_set, rects_set_args, nullptr);

        vector<uint8_t> rects_buffer(rects.size());
        const array<uint64_t, 2> rects_get_args = {reinterpret_cast<uint64_t>(rects_buffer.data()), rects_buffer.size()};
        CallWasmApiProperty(engine, *rects_get, rects_get_args, &raw_result);
        CHECK(raw_result == rects.size());
        CHECK(ReadWasmApiTestRectArrayBlob(rects_buffer) == vector<WasmApiTestRect> {WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = -5, .Y = 6, .Width = -7, .Height = 8}});

        const vector<uint8_t> tags = MakeWasmApiTestStringArrayBlob({"alpha", "beta"});
        const array<uint64_t, 2> tags_set_args = {reinterpret_cast<uint64_t>(tags.data()), tags.size()};
        CallWasmApiProperty(engine, *tags_set, tags_set_args, nullptr);

        vector<uint8_t> tags_buffer(tags.size());
        const array<uint64_t, 2> tags_get_args = {reinterpret_cast<uint64_t>(tags_buffer.data()), tags_buffer.size()};
        CallWasmApiProperty(engine, *tags_get, tags_get_args, &raw_result);
        CHECK(raw_result == tags.size());
        CHECK(ReadWasmApiTestStringArrayBlob(tags_buffer) == vector<string> {"alpha", "beta"});

        const vector<uint8_t> payloads = MakeWasmApiTestStringArrayBlob({"one", "two", ""});
        const array<uint64_t, 2> payloads_set_args = {reinterpret_cast<uint64_t>(payloads.data()), payloads.size()};
        CallWasmApiProperty(engine, *payloads_set, payloads_set_args, nullptr);

        vector<uint8_t> payloads_buffer(payloads.size());
        const array<uint64_t, 2> payloads_get_args = {reinterpret_cast<uint64_t>(payloads_buffer.data()), payloads_buffer.size()};
        CallWasmApiProperty(engine, *payloads_get, payloads_get_args, &raw_result);
        CHECK(raw_result == payloads.size());
        CHECK(ReadWasmApiTestStringArrayBlob(payloads_buffer) == vector<string> {"one", "two", ""});

        const vector<uint8_t> config = MakeWasmApiTestStringDictBlob({{"alpha", "1"}, {"beta", "22"}});
        const array<uint64_t, 2> config_set_args = {reinterpret_cast<uint64_t>(config.data()), config.size()};
        CallWasmApiProperty(engine, *config_set, config_set_args, nullptr);

        vector<uint8_t> config_buffer(config.size());
        const array<uint64_t, 2> config_get_args = {reinterpret_cast<uint64_t>(config_buffer.data()), config_buffer.size()};
        CallWasmApiProperty(engine, *config_get, config_get_args, &raw_result);
        CHECK(raw_result == config.size());
        CHECK(ReadWasmApiTestStringDictBlob(config_buffer) == map<string, string> {{"alpha", "1"}, {"beta", "22"}});

        const vector<uint8_t> any_config = MakeWasmApiTestStringDictBlob({{"first", "payload"}, {"second", "data"}});
        const array<uint64_t, 2> any_config_set_args = {reinterpret_cast<uint64_t>(any_config.data()), any_config.size()};
        CallWasmApiProperty(engine, *any_config_set, any_config_set_args, nullptr);

        vector<uint8_t> any_config_buffer(any_config.size());
        const array<uint64_t, 2> any_config_get_args = {reinterpret_cast<uint64_t>(any_config_buffer.data()), any_config_buffer.size()};
        CallWasmApiProperty(engine, *any_config_get, any_config_get_args, &raw_result);
        CHECK(raw_result == any_config.size());
        CHECK(ReadWasmApiTestStringDictBlob(any_config_buffer) == map<string, string> {{"first", "payload"}, {"second", "data"}});

        const vector<uint8_t> groups = MakeWasmApiTestStringIntArrayDictBlob({{"first", {1, 2, 3}}, {"second", {4, -1}}});
        const array<uint64_t, 2> groups_set_args = {reinterpret_cast<uint64_t>(groups.data()), groups.size()};
        CallWasmApiProperty(engine, *groups_set, groups_set_args, nullptr);

        vector<uint8_t> groups_buffer(groups.size());
        const array<uint64_t, 2> groups_get_args = {reinterpret_cast<uint64_t>(groups_buffer.data()), groups_buffer.size()};
        CallWasmApiProperty(engine, *groups_get, groups_get_args, &raw_result);
        CHECK(raw_result == groups.size());
        CHECK(ReadWasmApiTestStringIntArrayDictBlob(groups_buffer) == map<string, vector<int32_t>> {{"first", {1, 2, 3}}, {"second", {4, -1}}});

        const vector<uint8_t> rect_groups = MakeWasmApiTestStringRectArrayDictBlob({{"rects", {WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = -1, .Y = 2, .Width = -3, .Height = 4}}}});
        const array<uint64_t, 2> rect_groups_set_args = {reinterpret_cast<uint64_t>(rect_groups.data()), rect_groups.size()};
        CallWasmApiProperty(engine, *rect_groups_set, rect_groups_set_args, nullptr);

        vector<uint8_t> rect_groups_buffer(rect_groups.size());
        const array<uint64_t, 2> rect_groups_get_args = {reinterpret_cast<uint64_t>(rect_groups_buffer.data()), rect_groups_buffer.size()};
        CallWasmApiProperty(engine, *rect_groups_get, rect_groups_get_args, &raw_result);
        CHECK(raw_result == rect_groups.size());
        CHECK(ReadWasmApiTestStringRectArrayDictBlob(rect_groups_buffer) == map<string, vector<WasmApiTestRect>> {{"rects", {WasmApiTestRect {.X = 1, .Y = 2, .Width = 3, .Height = 4}, WasmApiTestRect {.X = -1, .Y = 2, .Width = -3, .Height = 4}}}});

        const vector<uint8_t> any_groups = MakeWasmApiTestStringStringArrayDictBlob({{"alpha", {"one", "two"}}, {"beta", {""}}});
        const array<uint64_t, 2> any_groups_set_args = {reinterpret_cast<uint64_t>(any_groups.data()), any_groups.size()};
        CallWasmApiProperty(engine, *any_groups_set, any_groups_set_args, nullptr);

        vector<uint8_t> any_groups_buffer(any_groups.size());
        const array<uint64_t, 2> any_groups_get_args = {reinterpret_cast<uint64_t>(any_groups_buffer.data()), any_groups_buffer.size()};
        CallWasmApiProperty(engine, *any_groups_get, any_groups_get_args, &raw_result);
        CHECK(raw_result == any_groups.size());
        CHECK(ReadWasmApiTestStringStringArrayDictBlob(any_groups_buffer) == map<string, vector<string>> {{"alpha", {"one", "two"}}, {"beta", {""}}});
    }

    SECTION("CallsRefTypeImportsAndProperties")
    {
        GlobalSettings settings {false};
        WasmApiTestEngine engine {settings, [](EngineMetadata& meta) {
                                      FO_STACK_TRACE_ENTRY();

                                      meta.RegisterSide(EngineSideKind::ServerSide);
                                      meta.RegisterEntityType("Game", true, true, false, false, false);
                                      meta.RegisterEntityType("ImGui", true, true, false, false, false);
                                      meta.RegisterRefType("RefCounter");
                                      meta.RegisterRefTypeMethods("RefCounter",
                                          vector<MethodDesc> {
                                              MethodDesc {
                                                  .Name = "__AddRef",
                                                  .Ret = {},
                                                  .Call = DummyWasmApiCall,
                                              },
                                              MethodDesc {
                                                  .Name = "__Release",
                                                  .Ret = {},
                                                  .Call = DummyWasmApiCall,
                                              },
                                              MethodDesc {
                                                  .Name = "__Factory",
                                                  .Ret = meta.ResolveComplexType("RefCounter"),
                                                  .Call =
                                                      [](FuncCallData& call) {
                                                          FO_STACK_TRACE_ENTRY();

                                                          NativeDataProvider::WriteTypedHandleSlot<void>(call.RetData, reinterpret_cast<void*>(uintptr_t {0x13579}));
                                                      },
                                                  .PassOwnership = true,
                                              },
                                              MethodDesc {
                                                  .Name = "Value",
                                                  .Ret = meta.ResolveComplexType("int32"),
                                                  .Call =
                                                      [](FuncCallData& call) {
                                                          FO_STACK_TRACE_ENTRY();

                                                          int32_t* counter = *cast_from_void<int32_t**>(call.ArgsData[0]);
                                                          WriteWasmApiTestValue(call.RetData, *counter);
                                                      },
                                              },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "CreateRefCounter",
                                              .Ret = meta.ResolveComplexType("RefCounter"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      NativeDataProvider::WriteTypedHandleSlot<void>(call.RetData, reinterpret_cast<void*>(uintptr_t {0x12345678}));
                                                  },
                                              .PassOwnership = true,
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "SumRefs",
                                              .Args = {{"values", meta.ResolveComplexType("RefCounter[]"), false}},
                                              .Ret = meta.ResolveComplexType("int32"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      int32_t sum = 0;
                                                      const size_t values_count = call.Accessor->GetArraySize(call.ArgsData[1]);

                                                      for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                          int32_t* counter = *cast_from_void<int32_t**>(call.Accessor->GetArrayElement(call.ArgsData[1], value_index));
                                                          sum += *counter;
                                                      }

                                                      WriteWasmApiTestValue(call.RetData, sum);
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "GetRefs",
                                              .Args = {},
                                              .Ret = meta.ResolveComplexType("RefCounter[]"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearArray(call.RetData);

                                                      array<void*, 2> refs = {reinterpret_cast<void*>(uintptr_t {0x1234}), reinterpret_cast<void*>(uintptr_t {0x5678})};

                                                      for (void*& ref : refs) {
                                                          call.Accessor->AddArrayElement(call.RetData, static_cast<void*>(&ref));
                                                      }
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "MutateRefs",
                                              .Args = {{"values", meta.ResolveComplexType("RefCounter[]&"), false}},
                                              .Ret = {},
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearArray(call.ArgsData[1]);

                                                      array<void*, 2> refs = {reinterpret_cast<void*>(uintptr_t {0x7770}), reinterpret_cast<void*>(uintptr_t {0x8880})};

                                                      for (void*& ref : refs) {
                                                          call.Accessor->AddArrayElement(call.ArgsData[1], static_cast<void*>(&ref));
                                                      }
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "SumRefMap",
                                              .Args = {{"values", meta.ResolveComplexType("string=>RefCounter"), false}},
                                              .Ret = meta.ResolveComplexType("int32"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      int32_t sum = 0;
                                                      const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                      for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                          const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                          int32_t* counter = *cast_from_void<int32_t**>(kv.second);
                                                          sum += *counter;
                                                      }

                                                      WriteWasmApiTestValue(call.RetData, sum);
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "GetRefMap",
                                              .Args = {},
                                              .Ret = meta.ResolveComplexType("string=>RefCounter"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearDict(call.RetData);

                                                      string first_key = "first";
                                                      string second_key = "second";
                                                      void* first_ref = reinterpret_cast<void*>(uintptr_t {0xAA10});
                                                      void* second_ref = reinterpret_cast<void*>(uintptr_t {0xBB20});
                                                      call.Accessor->AddDictElement(call.RetData, make_nptr(&first_key).void_cast(), static_cast<void*>(&first_ref));
                                                      call.Accessor->AddDictElement(call.RetData, make_nptr(&second_key).void_cast(), static_cast<void*>(&second_ref));
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "MutateRefMap",
                                              .Args = {{"values", meta.ResolveComplexType("string=>RefCounter&"), false}},
                                              .Ret = {},
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearDict(call.ArgsData[1]);

                                                      string first_key = "mutated";
                                                      string second_key = "again";
                                                      void* first_ref = reinterpret_cast<void*>(uintptr_t {0xDD10});
                                                      void* second_ref = reinterpret_cast<void*>(uintptr_t {0xEE20});
                                                      call.Accessor->AddDictElement(call.ArgsData[1], make_nptr(&first_key).void_cast(), static_cast<void*>(&first_ref));
                                                      call.Accessor->AddDictElement(call.ArgsData[1], make_nptr(&second_key).void_cast(), static_cast<void*>(&second_ref));
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "SumRefKeyMap",
                                              .Args = {{"values", meta.ResolveComplexType("RefCounter=>int32"), false}},
                                              .Ret = meta.ResolveComplexType("int32"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      int32_t sum = 0;
                                                      const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                      for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                          const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                          int32_t* counter = *cast_from_void<int32_t**>(kv.first);
                                                          const int32_t value = *cast_from_void<int32_t*>(kv.second);
                                                          sum += *counter * value;
                                                      }

                                                      WriteWasmApiTestValue(call.RetData, sum);
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "GetRefKeyMap",
                                              .Args = {},
                                              .Ret = meta.ResolveComplexType("RefCounter=>int32"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearDict(call.RetData);

                                                      void* first_ref = reinterpret_cast<void*>(uintptr_t {0xAA10});
                                                      void* second_ref = reinterpret_cast<void*>(uintptr_t {0xBB20});
                                                      int32_t first_value = 7;
                                                      int32_t second_value = 9;
                                                      call.Accessor->AddDictElement(call.RetData, static_cast<void*>(&first_ref), make_nptr(&first_value).void_cast());
                                                      call.Accessor->AddDictElement(call.RetData, static_cast<void*>(&second_ref), make_nptr(&second_value).void_cast());
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "MutateRefKeyMap",
                                              .Args = {{"values", meta.ResolveComplexType("RefCounter=>int32&"), false}},
                                              .Ret = {},
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearDict(call.ArgsData[1]);

                                                      void* first_ref = reinterpret_cast<void*>(uintptr_t {0xAB10});
                                                      void* second_ref = reinterpret_cast<void*>(uintptr_t {0xCD20});
                                                      int32_t first_value = 17;
                                                      int32_t second_value = 19;
                                                      call.Accessor->AddDictElement(call.ArgsData[1], static_cast<void*>(&first_ref), make_nptr(&first_value).void_cast());
                                                      call.Accessor->AddDictElement(call.ArgsData[1], static_cast<void*>(&second_ref), make_nptr(&second_value).void_cast());
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "CountRefGroups",
                                              .Args = {{"values", meta.ResolveComplexType("string=>RefCounter[]"), false}},
                                              .Ret = meta.ResolveComplexType("int32"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      int32_t count = 0;
                                                      const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                      for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                          const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                          count += numeric_cast<int32_t>(call.Accessor->GetNestedArraySize(kv.second));
                                                      }

                                                      WriteWasmApiTestValue(call.RetData, count);
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "GetRefGroups",
                                              .Args = {},
                                              .Ret = meta.ResolveComplexType("string=>RefCounter[]"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearDict(call.RetData);

                                                      string key = "refs";
                                                      array<void*, 2> refs = {reinterpret_cast<void*>(uintptr_t {0xCAFE}), reinterpret_cast<void*>(uintptr_t {0xF00D})};
                                                      vector<ptr<void>> values;
                                                      values.reserve(refs.size());

                                                      for (void*& ref : refs) {
                                                          values.emplace_back(static_cast<void*>(&ref));
                                                      }

                                                      call.Accessor->AddDictArrayElement(call.RetData, make_nptr(&key).void_cast(), values);
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "MutateRefGroups",
                                              .Args = {{"values", meta.ResolveComplexType("string=>RefCounter[]&"), false}},
                                              .Ret = {},
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearDict(call.ArgsData[1]);

                                                      string key = "mutated";
                                                      array<void*, 2> refs = {reinterpret_cast<void*>(uintptr_t {0xFEED}), reinterpret_cast<void*>(uintptr_t {0xBEEF})};
                                                      vector<ptr<void>> values;
                                                      values.reserve(refs.size());

                                                      for (void*& ref : refs) {
                                                          values.emplace_back(static_cast<void*>(&ref));
                                                      }

                                                      call.Accessor->AddDictArrayElement(call.ArgsData[1], make_nptr(&key).void_cast(), values);
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "SumRefKeyGroups",
                                              .Args = {{"values", meta.ResolveComplexType("RefCounter=>int32[]"), false}},
                                              .Ret = meta.ResolveComplexType("int32"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      int32_t sum = 0;
                                                      const size_t values_count = call.Accessor->GetDictSize(call.ArgsData[1]);

                                                      for (size_t value_index = 0; value_index < values_count; value_index++) {
                                                          const auto kv = call.Accessor->GetDictElement(call.ArgsData[1], value_index);
                                                          int32_t* counter = *cast_from_void<int32_t**>(kv.first);
                                                          const size_t items_count = call.Accessor->GetNestedArraySize(kv.second);

                                                          for (size_t item_index = 0; item_index < items_count; item_index++) {
                                                              const int32_t item = *cast_from_void<int32_t*>(call.Accessor->GetNestedArrayElement(kv.second, item_index));
                                                              sum += *counter * item;
                                                          }
                                                      }

                                                      WriteWasmApiTestValue(call.RetData, sum);
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "GetRefKeyGroups",
                                              .Args = {},
                                              .Ret = meta.ResolveComplexType("RefCounter=>int32[]"),
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearDict(call.RetData);

                                                      void* first_ref = reinterpret_cast<void*>(uintptr_t {0xCAFE});
                                                      void* second_ref = reinterpret_cast<void*>(uintptr_t {0xF00D});
                                                      array<int32_t, 2> first_values = {4, 5};
                                                      array<int32_t, 1> second_values = {6};
                                                      vector<ptr<void>> first_items;
                                                      vector<ptr<void>> second_items;
                                                      first_items.reserve(first_values.size());
                                                      second_items.reserve(second_values.size());

                                                      for (int32_t& value : first_values) {
                                                          first_items.emplace_back(make_nptr(&value).void_cast());
                                                      }
                                                      for (int32_t& value : second_values) {
                                                          second_items.emplace_back(make_nptr(&value).void_cast());
                                                      }

                                                      call.Accessor->AddDictArrayElement(call.RetData, static_cast<void*>(&first_ref), first_items);
                                                      call.Accessor->AddDictArrayElement(call.RetData, static_cast<void*>(&second_ref), second_items);
                                                  },
                                          });
                                      meta.RegisterEntityMethod("Game",
                                          MethodDesc {
                                              .Name = "MutateRefKeyGroups",
                                              .Args = {{"values", meta.ResolveComplexType("RefCounter=>int32[]&"), false}},
                                              .Ret = {},
                                              .Call =
                                                  [](FuncCallData& call) {
                                                      FO_STACK_TRACE_ENTRY();

                                                      call.Accessor->ClearDict(call.ArgsData[1]);

                                                      void* first_ref = reinterpret_cast<void*>(uintptr_t {0xABCD});
                                                      void* second_ref = reinterpret_cast<void*>(uintptr_t {0xDCBA});
                                                      array<int32_t, 2> first_values = {8, 9};
                                                      array<int32_t, 1> second_values = {10};
                                                      vector<ptr<void>> first_items;
                                                      vector<ptr<void>> second_items;
                                                      first_items.reserve(first_values.size());
                                                      second_items.reserve(second_values.size());

                                                      for (int32_t& value : first_values) {
                                                          first_items.emplace_back(make_nptr(&value).void_cast());
                                                      }
                                                      for (int32_t& value : second_values) {
                                                          second_items.emplace_back(make_nptr(&value).void_cast());
                                                      }

                                                      call.Accessor->AddDictArrayElement(call.ArgsData[1], static_cast<void*>(&first_ref), first_items);
                                                      call.Accessor->AddDictArrayElement(call.ArgsData[1], static_cast<void*>(&second_ref), second_items);
                                                  },
                                          });
                                      meta.RegisterRefType("PointRef");
                                      meta.RegisterRefTypeLayout("PointRef", {{"X", "int32"}, {"Title", "string"}});
                                  }};

        const vector<WasmApiMethodDesc> api_methods = BuildWasmApiMethodDescs(engine);
        nptr<const WasmApiMethodDesc> create_ref_counter = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CreateRefCounter__void__RefCounter");
        nptr<const WasmApiMethodDesc> ref_factory = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "RefCounter___Factory__void__RefCounter");
        nptr<const WasmApiMethodDesc> ref_value = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "RefCounter_Value__RefCounter__int32");
        nptr<const WasmApiMethodDesc> sum_refs = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_SumRefs__RefCounter_array__int32");
        nptr<const WasmApiMethodDesc> get_refs = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefs__void__RefCounter_array");
        nptr<const WasmApiMethodDesc> mutate_refs = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefs__RefCounter_array_mut__void");
        nptr<const WasmApiMethodDesc> sum_ref_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_SumRefMap__string_RefCounter_dict__int32");
        nptr<const WasmApiMethodDesc> get_ref_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefMap__void__string_RefCounter_dict");
        nptr<const WasmApiMethodDesc> mutate_ref_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefMap__string_RefCounter_dict_mut__void");
        nptr<const WasmApiMethodDesc> sum_ref_key_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_SumRefKeyMap__RefCounter_int32_dict__int32");
        nptr<const WasmApiMethodDesc> get_ref_key_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefKeyMap__void__RefCounter_int32_dict");
        nptr<const WasmApiMethodDesc> mutate_ref_key_map = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefKeyMap__RefCounter_int32_dict_mut__void");
        nptr<const WasmApiMethodDesc> count_ref_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_CountRefGroups__string_RefCounter_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_ref_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefGroups__void__string_RefCounter_array_dict");
        nptr<const WasmApiMethodDesc> mutate_ref_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefGroups__string_RefCounter_array_dict_mut__void");
        nptr<const WasmApiMethodDesc> sum_ref_key_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_SumRefKeyGroups__RefCounter_int32_array_dict__int32");
        nptr<const WasmApiMethodDesc> get_ref_key_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_GetRefKeyGroups__void__RefCounter_int32_array_dict");
        nptr<const WasmApiMethodDesc> mutate_ref_key_groups = FindWasmApiMethodDesc(api_methods, WASM_ENGINE_API_MODULE, "Game_MutateRefKeyGroups__RefCounter_int32_array_dict_mut__void");

        REQUIRE(create_ref_counter);
        REQUIRE(create_ref_counter->Supported);
        REQUIRE(ref_factory);
        REQUIRE(ref_factory->Supported);
        REQUIRE(ref_value);
        REQUIRE(ref_value->Supported);
        REQUIRE(sum_refs);
        REQUIRE(sum_refs->Supported);
        REQUIRE(get_refs);
        REQUIRE(get_refs->Supported);
        REQUIRE(mutate_refs);
        REQUIRE(mutate_refs->Supported);
        REQUIRE(sum_ref_map);
        REQUIRE(sum_ref_map->Supported);
        REQUIRE(get_ref_map);
        REQUIRE(get_ref_map->Supported);
        REQUIRE(mutate_ref_map);
        REQUIRE(mutate_ref_map->Supported);
        REQUIRE(sum_ref_key_map);
        REQUIRE(sum_ref_key_map->Supported);
        REQUIRE(get_ref_key_map);
        REQUIRE(get_ref_key_map->Supported);
        REQUIRE(mutate_ref_key_map);
        REQUIRE(mutate_ref_key_map->Supported);
        REQUIRE(count_ref_groups);
        REQUIRE(count_ref_groups->Supported);
        REQUIRE(get_ref_groups);
        REQUIRE(get_ref_groups->Supported);
        REQUIRE(mutate_ref_groups);
        REQUIRE(mutate_ref_groups->Supported);
        REQUIRE(sum_ref_key_groups);
        REQUIRE(sum_ref_key_groups->Supported);
        REQUIRE(get_ref_key_groups);
        REQUIRE(get_ref_key_groups->Supported);
        REQUIRE(mutate_ref_key_groups);
        REQUIRE(mutate_ref_key_groups->Supported);

        int32_t counter = 77;
        uint64_t raw_result = 0;
        const array<uint64_t, 0> no_args {};
        CallWasmApiMethod(engine, *create_ref_counter, no_args, &raw_result);
        CHECK(raw_result == 0x12345678);

        CallWasmApiMethod(engine, *ref_factory, no_args, &raw_result);
        CHECK(raw_result == 0x13579);

        const array<uint64_t, 1> counter_args = {numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&counter))};
        CallWasmApiMethod(engine, *ref_value, counter_args, &raw_result);
        CHECK(raw_result == 77);

        int32_t first_counter = 11;
        int32_t second_counter = 22;
        const uint64_t first_counter_handle = numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&first_counter));
        const uint64_t second_counter_handle = numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&second_counter));
        const vector<uint8_t> refs_input = MakeWasmApiTestU64ArrayBlob({first_counter_handle, second_counter_handle});
        const array<uint64_t, 2> sum_refs_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(refs_input.data())),
            refs_input.size(),
        };
        CallWasmApiMethod(engine, *sum_refs, sum_refs_args, &raw_result);
        CHECK(raw_result == 33);

        vector<uint8_t> refs_output(2 * sizeof(uint64_t));
        const array<uint64_t, 2> get_refs_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(refs_output.data())),
            refs_output.size(),
        };
        CallWasmApiMethod(engine, *get_refs, get_refs_args, &raw_result);
        CHECK(raw_result == refs_output.size());
        CHECK(ReadWasmApiTestU64ArrayBlob(refs_output) == vector<uint64_t> {0x1234, 0x5678});

        vector<uint8_t> mutable_refs_buffer = refs_input;
        uint32_t mutable_refs_required_size = 0;
        const array<uint64_t, 4> mutate_refs_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(mutable_refs_buffer.data())),
            refs_input.size(),
            mutable_refs_buffer.size(),
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&mutable_refs_required_size)),
        };
        CallWasmApiMethod(engine, *mutate_refs, mutate_refs_args, nullptr);
        CHECK(mutable_refs_required_size == MakeWasmApiTestU64ArrayBlob({0x7770, 0x8880}).size());
        CHECK(ReadWasmApiTestU64ArrayBlob(mutable_refs_buffer) == vector<uint64_t> {0x7770, 0x8880});

        const vector<uint8_t> ref_map_input = MakeWasmApiTestStringU64DictBlob({{"first", first_counter_handle}, {"second", second_counter_handle}});
        const array<uint64_t, 2> sum_ref_map_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref_map_input.data())),
            ref_map_input.size(),
        };
        CallWasmApiMethod(engine, *sum_ref_map, sum_ref_map_args, &raw_result);
        CHECK(raw_result == 33);

        const vector<uint8_t> expected_ref_map = MakeWasmApiTestStringU64DictBlob({{"first", 0xAA10}, {"second", 0xBB20}});
        vector<uint8_t> ref_map_output(expected_ref_map.size());
        const array<uint64_t, 2> get_ref_map_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref_map_output.data())),
            ref_map_output.size(),
        };
        CallWasmApiMethod(engine, *get_ref_map, get_ref_map_args, &raw_result);
        CHECK(raw_result == expected_ref_map.size());
        CHECK(ReadWasmApiTestStringU64DictBlob(ref_map_output) == map<string, uint64_t> {{"first", 0xAA10}, {"second", 0xBB20}});

        const vector<uint8_t> expected_mutable_ref_map = MakeWasmApiTestStringU64DictBlob({{"again", 0xEE20}, {"mutated", 0xDD10}});
        vector<uint8_t> mutable_ref_map_buffer = ref_map_input;
        mutable_ref_map_buffer.resize(96);
        uint32_t mutable_ref_map_required_size = 0;
        const array<uint64_t, 4> mutate_ref_map_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(mutable_ref_map_buffer.data())),
            ref_map_input.size(),
            mutable_ref_map_buffer.size(),
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&mutable_ref_map_required_size)),
        };
        CallWasmApiMethod(engine, *mutate_ref_map, mutate_ref_map_args, nullptr);
        CHECK(mutable_ref_map_required_size == expected_mutable_ref_map.size());

        vector<uint8_t> mutable_ref_map_output(mutable_ref_map_buffer.begin(), mutable_ref_map_buffer.begin() + mutable_ref_map_required_size);
        CHECK(ReadWasmApiTestStringU64DictBlob(mutable_ref_map_output) == map<string, uint64_t> {{"again", 0xEE20}, {"mutated", 0xDD10}});

        const vector<uint8_t> ref_key_map_input = MakeWasmApiTestU64IntDictBlob({{first_counter_handle, 2}, {second_counter_handle, 3}});
        const array<uint64_t, 2> sum_ref_key_map_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref_key_map_input.data())),
            ref_key_map_input.size(),
        };
        CallWasmApiMethod(engine, *sum_ref_key_map, sum_ref_key_map_args, &raw_result);
        CHECK(raw_result == 88);

        const vector<uint8_t> expected_ref_key_map = MakeWasmApiTestU64IntDictBlob({{0xAA10, 7}, {0xBB20, 9}});
        vector<uint8_t> ref_key_map_output(expected_ref_key_map.size());
        const array<uint64_t, 2> get_ref_key_map_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref_key_map_output.data())),
            ref_key_map_output.size(),
        };
        CallWasmApiMethod(engine, *get_ref_key_map, get_ref_key_map_args, &raw_result);
        CHECK(raw_result == expected_ref_key_map.size());
        CHECK(ReadWasmApiTestU64IntDictBlob(ref_key_map_output) == map<uint64_t, int32_t> {{0xAA10, 7}, {0xBB20, 9}});

        const vector<uint8_t> expected_mutable_ref_key_map = MakeWasmApiTestU64IntDictBlob({{0xAB10, 17}, {0xCD20, 19}});
        vector<uint8_t> mutable_ref_key_map_buffer = ref_key_map_input;
        mutable_ref_key_map_buffer.resize(96);
        uint32_t mutable_ref_key_map_required_size = 0;
        const array<uint64_t, 4> mutate_ref_key_map_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(mutable_ref_key_map_buffer.data())),
            ref_key_map_input.size(),
            mutable_ref_key_map_buffer.size(),
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&mutable_ref_key_map_required_size)),
        };
        CallWasmApiMethod(engine, *mutate_ref_key_map, mutate_ref_key_map_args, nullptr);
        CHECK(mutable_ref_key_map_required_size == expected_mutable_ref_key_map.size());

        vector<uint8_t> mutable_ref_key_map_output(mutable_ref_key_map_buffer.begin(), mutable_ref_key_map_buffer.begin() + mutable_ref_key_map_required_size);
        CHECK(ReadWasmApiTestU64IntDictBlob(mutable_ref_key_map_output) == map<uint64_t, int32_t> {{0xAB10, 17}, {0xCD20, 19}});

        const vector<uint8_t> ref_groups_input = MakeWasmApiTestStringU64ArrayDictBlob({{"refs", {first_counter_handle, second_counter_handle}}});
        const array<uint64_t, 2> count_ref_groups_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref_groups_input.data())),
            ref_groups_input.size(),
        };
        CallWasmApiMethod(engine, *count_ref_groups, count_ref_groups_args, &raw_result);
        CHECK(raw_result == 2);

        const vector<uint8_t> expected_ref_groups = MakeWasmApiTestStringU64ArrayDictBlob({{"refs", {0xCAFE, 0xF00D}}});
        vector<uint8_t> ref_groups_output(expected_ref_groups.size());
        const array<uint64_t, 2> get_ref_groups_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref_groups_output.data())),
            ref_groups_output.size(),
        };
        CallWasmApiMethod(engine, *get_ref_groups, get_ref_groups_args, &raw_result);
        CHECK(raw_result == expected_ref_groups.size());
        CHECK(ReadWasmApiTestStringU64ArrayDictBlob(ref_groups_output) == map<string, vector<uint64_t>> {{"refs", {0xCAFE, 0xF00D}}});

        const vector<uint8_t> expected_mutable_ref_groups = MakeWasmApiTestStringU64ArrayDictBlob({{"mutated", {0xFEED, 0xBEEF}}});
        vector<uint8_t> mutable_ref_groups_buffer = ref_groups_input;
        mutable_ref_groups_buffer.resize(96);
        uint32_t mutable_ref_groups_required_size = 0;
        const array<uint64_t, 4> mutate_ref_groups_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(mutable_ref_groups_buffer.data())),
            ref_groups_input.size(),
            mutable_ref_groups_buffer.size(),
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&mutable_ref_groups_required_size)),
        };
        CallWasmApiMethod(engine, *mutate_ref_groups, mutate_ref_groups_args, nullptr);
        CHECK(mutable_ref_groups_required_size == expected_mutable_ref_groups.size());

        vector<uint8_t> mutable_ref_groups_output(mutable_ref_groups_buffer.begin(), mutable_ref_groups_buffer.begin() + mutable_ref_groups_required_size);
        CHECK(ReadWasmApiTestStringU64ArrayDictBlob(mutable_ref_groups_output) == map<string, vector<uint64_t>> {{"mutated", {0xFEED, 0xBEEF}}});

        const vector<uint8_t> ref_key_groups_input = MakeWasmApiTestU64IntArrayDictBlob({{first_counter_handle, {1, 2}}, {second_counter_handle, {3}}});
        const array<uint64_t, 2> sum_ref_key_groups_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref_key_groups_input.data())),
            ref_key_groups_input.size(),
        };
        CallWasmApiMethod(engine, *sum_ref_key_groups, sum_ref_key_groups_args, &raw_result);
        CHECK(raw_result == 99);

        const vector<uint8_t> expected_ref_key_groups = MakeWasmApiTestU64IntArrayDictBlob({{0xCAFE, {4, 5}}, {0xF00D, {6}}});
        vector<uint8_t> ref_key_groups_output(expected_ref_key_groups.size());
        const array<uint64_t, 2> get_ref_key_groups_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(ref_key_groups_output.data())),
            ref_key_groups_output.size(),
        };
        CallWasmApiMethod(engine, *get_ref_key_groups, get_ref_key_groups_args, &raw_result);
        CHECK(raw_result == expected_ref_key_groups.size());
        CHECK(ReadWasmApiTestU64IntArrayDictBlob(ref_key_groups_output) == map<uint64_t, vector<int32_t>> {{0xCAFE, {4, 5}}, {0xF00D, {6}}});

        const vector<uint8_t> expected_mutable_ref_key_groups = MakeWasmApiTestU64IntArrayDictBlob({{0xABCD, {8, 9}}, {0xDCBA, {10}}});
        vector<uint8_t> mutable_ref_key_groups_buffer = ref_key_groups_input;
        mutable_ref_key_groups_buffer.resize(96);
        uint32_t mutable_ref_key_groups_required_size = 0;
        const array<uint64_t, 4> mutate_ref_key_groups_args = {
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(mutable_ref_key_groups_buffer.data())),
            ref_key_groups_input.size(),
            mutable_ref_key_groups_buffer.size(),
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(&mutable_ref_key_groups_required_size)),
        };
        CallWasmApiMethod(engine, *mutate_ref_key_groups, mutate_ref_key_groups_args, nullptr);
        CHECK(mutable_ref_key_groups_required_size == expected_mutable_ref_key_groups.size());

        vector<uint8_t> mutable_ref_key_groups_output(mutable_ref_key_groups_buffer.begin(), mutable_ref_key_groups_buffer.begin() + mutable_ref_key_groups_required_size);
        CHECK(ReadWasmApiTestU64IntArrayDictBlob(mutable_ref_key_groups_output) == map<uint64_t, vector<int32_t>> {{0xABCD, {8, 9}}, {0xDCBA, {10}}});

        const RefTypeDesc& point_ref_type = engine.GetRefTypes().at("PointRef");
        auto point_ref = SafeAlloc::MakeRefCounted<DynamicRefTypeInstance>(point_ref_type.FieldsRegistrar.get());
        const uint64_t point_ref_handle = numeric_cast<uint64_t>(point_ref.as_uintptr());

        const vector<WasmApiPropertyDesc> api_properties = BuildWasmApiPropertyDescs(engine);
        nptr<const WasmApiPropertyDesc> x_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "PointRef_get_X__PointRef_int32");
        nptr<const WasmApiPropertyDesc> x_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "PointRef_set_X__PointRef_int32");
        nptr<const WasmApiPropertyDesc> title_get = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "PointRef_get_Title__PointRef_string");
        nptr<const WasmApiPropertyDesc> title_set = FindWasmApiPropertyDesc(api_properties, WASM_ENGINE_API_MODULE, "PointRef_set_Title__PointRef_string");

        REQUIRE(x_get);
        REQUIRE(x_get->Supported);
        REQUIRE(x_set);
        REQUIRE(x_set->Supported);
        REQUIRE(title_get);
        REQUIRE(title_get->Supported);
        REQUIRE(title_set);
        REQUIRE(title_set->Supported);

        const array<uint64_t, 2> x_set_args = {point_ref_handle, 42};
        const array<uint64_t, 1> point_ref_args = {point_ref_handle};
        CallWasmApiProperty(engine, *x_set, x_set_args, nullptr);
        CallWasmApiProperty(engine, *x_get, point_ref_args, &raw_result);
        CHECK(raw_result == 42);

        const string title = "borrowed ref";
        const array<uint64_t, 3> title_set_args = {
            point_ref_handle,
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(title.data())),
            title.size(),
        };
        CallWasmApiProperty(engine, *title_set, title_set_args, nullptr);

        array<char, 32> title_buffer {};
        const array<uint64_t, 3> title_get_args = {
            point_ref_handle,
            numeric_cast<uint64_t>(reinterpret_cast<uintptr_t>(title_buffer.data())),
            title_buffer.size(),
        };
        CallWasmApiProperty(engine, *title_get, title_get_args, &raw_result);
        CHECK(raw_result == title.size());
        CHECK(string_view {title_buffer.data(), title.size()} == title);
    }
}

FO_END_NAMESPACE

#endif
