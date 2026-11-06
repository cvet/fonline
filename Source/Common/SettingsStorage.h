// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

class SettingsStorageImpl;

// Per-user tool and editor settings, scoped by application name so tools never collide. Persistence is
// best-effort: a backend failure is logged rather than thrown, so no tool dies over unsaved settings
class SettingsStorage
{
public:
    explicit SettingsStorage(string_view app_name);
    SettingsStorage(const SettingsStorage&) = delete;
    SettingsStorage(SettingsStorage&&) noexcept;
    auto operator=(const SettingsStorage&) = delete;
    auto operator=(SettingsStorage&&) noexcept = delete;
    ~SettingsStorage();

    [[nodiscard]] auto HasKey(string_view key) const -> bool;
    [[nodiscard]] auto GetString(string_view key, string_view default_value = "") const -> string;
    [[nodiscard]] auto GetInt(string_view key, int64_t default_value = 0) const -> int64_t;
    [[nodiscard]] auto GetBool(string_view key, bool default_value = false) const -> bool;
    [[nodiscard]] auto GetFloat(string_view key, float64_t default_value = 0.0) const -> float64_t;

    void SetString(string_view key, string_view value);
    void SetInt(string_view key, int64_t value);
    void SetBool(string_view key, bool value);
    void SetFloat(string_view key, float64_t value);
    void Remove(string_view key);

private:
    unique_ptr<SettingsStorageImpl> _impl;
};

FO_END_NAMESPACE
