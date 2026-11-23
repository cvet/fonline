// FOnline Engine
// SPDX-License-Identifier: MIT

#include "SettingsStorage.h"

#include "CacheStorage.h"

FO_BEGIN_NAMESPACE

// Values are always stored as strings, so the registry and file backends behave identically: a REG_SZ under
// the application subkey on Windows, one CacheStorage entry elsewhere
class SettingsStorageImpl
{
public:
    explicit SettingsStorageImpl(string_view app_name);
    SettingsStorageImpl(const SettingsStorageImpl&) = delete;
    SettingsStorageImpl(SettingsStorageImpl&&) noexcept = delete;
    auto operator=(const SettingsStorageImpl&) = delete;
    auto operator=(SettingsStorageImpl&&) noexcept = delete;
    ~SettingsStorageImpl() = default;

    [[nodiscard]] auto HasEntry(string_view key) const -> bool;
    [[nodiscard]] auto GetEntry(string_view key) const -> optional<string>;

    void SetEntry(string_view key, string_view value);
    void RemoveEntry(string_view key);

private:
#if FO_WINDOWS
    string _subKey {};
#else
    unique_nptr<CacheStorage> _cache {};
#endif
};

SettingsStorageImpl::SettingsStorageImpl(string_view app_name)
{
#if FO_WINDOWS
    _subKey = strex("Software\\{}\\{}", FO_NICE_NAME, app_name).str();

#else
    // The one place that names the product without settings, because this layer has none: tool preferences
    // follow the user rather than one game install, so they never move into a client's writable root
    string base {platform::get_user_data_base()};

    // No user data base (unusual sandbox): keep the tool running without persistence rather than write next to the binary
    if (!base.empty()) {
        string dir {strex(base).combine_path(FO_NICE_NAME).combine_path(app_name).str()};
        _cache = safe_alloc::make_unique<CacheStorage>(dir);
    }
#endif
}

auto SettingsStorageImpl::HasEntry(string_view key) const -> bool
{
    return GetEntry(key).has_value();
}

auto SettingsStorageImpl::GetEntry(string_view key) const -> optional<string>
{
#if FO_WINDOWS
    return winapi::registry_read_value(_subKey, string(key));

#else
    if (_cache && _cache->HasEntry(key)) {
        return _cache->GetString(key);
    }

    return std::nullopt;
#endif
}

void SettingsStorageImpl::SetEntry(string_view key, string_view value)
{
#if FO_WINDOWS
    if (!winapi::registry_write_value(_subKey, string(key), string(value))) {
        logging::write(logging::type::warning, "Settings: failed to write registry value - {}\\{}", _subKey, key);
    }

#else
    if (_cache) {
        _cache->SetString(key, value);
    }
#endif
}

void SettingsStorageImpl::RemoveEntry(string_view key)
{
#if FO_WINDOWS
    winapi::registry_delete_value(_subKey, string(key));

#else
    if (_cache) {
        _cache->RemoveEntry(key);
    }
#endif
}

SettingsStorage::SettingsStorage(string_view app_name) :
    _impl {safe_alloc::make_unique<SettingsStorageImpl>(app_name)}
{
}

SettingsStorage::SettingsStorage(SettingsStorage&&) noexcept = default;
SettingsStorage::~SettingsStorage() = default;

auto SettingsStorage::HasKey(string_view key) const -> bool
{
    return _impl->HasEntry(key);
}

auto SettingsStorage::GetString(string_view key, string_view default_value) const -> string
{
    auto entry {_impl->GetEntry(key)};
    return entry ? *entry : string(default_value);
}

auto SettingsStorage::GetInt(string_view key, int64_t default_value) const -> int64_t
{
    auto entry {_impl->GetEntry(key)};
    return entry ? strex(*entry).to_int64() : default_value;
}

auto SettingsStorage::GetBool(string_view key, bool default_value) const -> bool
{
    auto entry {_impl->GetEntry(key)};
    return entry ? strex(*entry).to_bool() : default_value;
}

auto SettingsStorage::GetFloat(string_view key, float64_t default_value) const -> float64_t
{
    auto entry {_impl->GetEntry(key)};
    return entry ? strex(*entry).to_float64() : default_value;
}

void SettingsStorage::SetString(string_view key, string_view value)
{
    _impl->SetEntry(key, value);
}

void SettingsStorage::SetInt(string_view key, int64_t value)
{
    _impl->SetEntry(key, strex("{}", value).str());
}

void SettingsStorage::SetBool(string_view key, bool value)
{
    _impl->SetEntry(key, value ? "1" : "0");
}

void SettingsStorage::SetFloat(string_view key, float64_t value)
{
    _impl->SetEntry(key, strex("{}", value).str());
}

void SettingsStorage::Remove(string_view key)
{
    _impl->RemoveEntry(key);
}

FO_END_NAMESPACE
