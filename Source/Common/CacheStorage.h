// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

class CacheStorageImpl;

enum class CacheStorageReadStatus : uint8_t
{
    Success,
    Missing,
    TooLarge,
    Failed,
};

struct CacheStorageReadResult
{
    CacheStorageReadStatus Status {};
    vector<uint8_t> Data {};
};

class CacheStorage
{
public:
    explicit CacheStorage(string_view path);
    CacheStorage(const CacheStorage&) = delete;
    CacheStorage(CacheStorage&&) noexcept;
    auto operator=(const CacheStorage&) = delete;
    auto operator=(CacheStorage&&) noexcept = delete;
    ~CacheStorage();

    [[nodiscard]] auto HasEntry(string_view entry_name) const -> bool;
    [[nodiscard]] auto GetString(string_view entry_name) const -> string;
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uint8_t>;
    [[nodiscard]] auto GetDataBounded(string_view entry_name, size_t max_size) const -> CacheStorageReadResult;

    void SetString(string_view entry_name, string_view str);
    void SetData(string_view entry_name, const_span<uint8_t> data);
    auto SetDataChecked(string_view entry_name, const_span<uint8_t> data) -> bool;
    void RemoveEntry(string_view entry_name);

private:
    unique_ptr<CacheStorageImpl> _impl;
};

FO_END_NAMESPACE
