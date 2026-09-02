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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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
    string _subKey;
#else
    unique_nptr<CacheStorage> _cache;
#endif
};

SettingsStorageImpl::SettingsStorageImpl(string_view app_name)
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    _subKey = strex("Software\\FOnline\\{}", app_name).str();

#else
    // Keep tool settings out of the resource cache: a dedicated per-application directory in the user data base.
    // No user data base (unusual sandbox) means best-effort no persistence rather than writing next to the binary
    string base = platform::get_user_data_base();

    if (!base.empty()) {
        string dir = strex(base).combine_path("FOnline").combine_path(app_name).str();
        _cache = safe_alloc::make_unique<CacheStorage>(dir);
    }
#endif
}

auto SettingsStorageImpl::HasEntry(string_view key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetEntry(key).has_value();
}

auto SettingsStorageImpl::GetEntry(string_view key) const -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    if (!winapi::registry_write_value(_subKey, string(key), string(value))) {
        logging::write("Settings: failed to write registry value - {}\\{}", _subKey, key);
    }

#else
    if (_cache) {
        _cache->SetString(key, value);
    }
#endif
}

void SettingsStorageImpl::RemoveEntry(string_view key)
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();
}

SettingsStorage::SettingsStorage(SettingsStorage&&) noexcept = default;
SettingsStorage::~SettingsStorage() = default;

auto SettingsStorage::HasKey(string_view key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _impl->HasEntry(key);
}

auto SettingsStorage::GetString(string_view key, string_view default_value) const -> string
{
    FO_STACK_TRACE_ENTRY();

    auto entry = _impl->GetEntry(key);
    return entry ? *entry : string(default_value);
}

auto SettingsStorage::GetInt(string_view key, int64_t default_value) const -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    auto entry = _impl->GetEntry(key);
    return entry ? strex(*entry).to_int64() : default_value;
}

auto SettingsStorage::GetBool(string_view key, bool default_value) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto entry = _impl->GetEntry(key);
    return entry ? strex(*entry).to_bool() : default_value;
}

auto SettingsStorage::GetFloat(string_view key, float64_t default_value) const -> float64_t
{
    FO_STACK_TRACE_ENTRY();

    auto entry = _impl->GetEntry(key);
    return entry ? strex(*entry).to_float64() : default_value;
}

void SettingsStorage::SetString(string_view key, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetEntry(key, value);
}

void SettingsStorage::SetInt(string_view key, int64_t value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetEntry(key, strex("{}", value).str());
}

void SettingsStorage::SetBool(string_view key, bool value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetEntry(key, value ? "1" : "0");
}

void SettingsStorage::SetFloat(string_view key, float64_t value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetEntry(key, strex("{}", value).str());
}

void SettingsStorage::Remove(string_view key)
{
    FO_STACK_TRACE_ENTRY();

    _impl->RemoveEntry(key);
}

FO_END_NAMESPACE
