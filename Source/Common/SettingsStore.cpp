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

#include "SettingsStore.h"

#include "CacheStorage.h"

#if FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include "WinApiUndef.inc"

FO_BEGIN_NAMESPACE

// Backend of SettingsStore. Values are always stored as strings (typed accessors serialize through them), so the
// registry and the file backend behave identically. On Windows a value is a REG_SZ under the application subkey;
// elsewhere it is one entry in a per-application CacheStorage.
class SettingsStoreImpl
{
public:
    explicit SettingsStoreImpl(string_view app_name);
    SettingsStoreImpl(const SettingsStoreImpl&) = delete;
    SettingsStoreImpl(SettingsStoreImpl&&) noexcept = delete;
    auto operator=(const SettingsStoreImpl&) = delete;
    auto operator=(SettingsStoreImpl&&) noexcept = delete;
    ~SettingsStoreImpl() = default;

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

#if FO_WINDOWS

// The registry stores the raw UTF-8 bytes of the value as a REG_SZ (a trailing null is added on write and stripped
// on read), so the round-trip is byte-preserving even for non-ASCII text. Only the explicit *A entry points are
// used because WinApiUndef.inc removes the RegCreateKeyEx/etc. resolver macros.
static auto RegistryReadValue(const string& sub_key, string_view name) -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    HKEY hkey {};

    if (RegOpenKeyExA(HKEY_CURRENT_USER, sub_key.c_str(), 0, KEY_READ, &hkey) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    auto close_key = scope_exit([hkey]() noexcept { RegCloseKey(hkey); });
    string name_str = string(name);
    DWORD type = 0;
    DWORD size = 0;

    if (RegQueryValueExA(hkey, name_str.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ || size == 0) {
        return std::nullopt;
    }

    string value;
    value.resize(size);

    if (RegQueryValueExA(hkey, name_str.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(value.data()), &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    // REG_SZ carries a terminating null in its byte count; drop it to recover the original string.
    if (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }

    return value;
}

static void RegistryWriteValue(const string& sub_key, string_view name, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    HKEY hkey {};

    if (RegCreateKeyExA(HKEY_CURRENT_USER, sub_key.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hkey, nullptr) != ERROR_SUCCESS) {
        WriteLog("Settings: failed to open registry key for writing - {}", sub_key);
        return;
    }

    auto close_key = scope_exit([hkey]() noexcept { RegCloseKey(hkey); });
    string name_str = string(name);
    string value_str = string(value);
    DWORD size = numeric_cast<DWORD>(value_str.size() + 1);

    if (RegSetValueExA(hkey, name_str.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value_str.c_str()), size) != ERROR_SUCCESS) {
        WriteLog("Settings: failed to write registry value - {}\\{}", sub_key, name);
    }
}

static void RegistryDeleteValue(const string& sub_key, string_view name)
{
    FO_STACK_TRACE_ENTRY();

    HKEY hkey {};

    if (RegOpenKeyExA(HKEY_CURRENT_USER, sub_key.c_str(), 0, KEY_WRITE, &hkey) != ERROR_SUCCESS) {
        return;
    }

    auto close_key = scope_exit([hkey]() noexcept { RegCloseKey(hkey); });
    string name_str = string(name);
    (void)RegDeleteValueA(hkey, name_str.c_str());
}

#endif

SettingsStoreImpl::SettingsStoreImpl(string_view app_name)
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    _subKey = strex("Software\\FOnline\\{}", app_name).str();
#else

    // Keep tool settings out of the resource cache: a dedicated per-application directory in the user data base.
    // No user data base (unusual sandbox) means best-effort no persistence rather than writing next to the binary.
    string base = Platform::GetUserDataBase();

    if (!base.empty()) {
        string dir = strex(base).combine_path("FOnline").combine_path(app_name).str();
        _cache = SafeAlloc::MakeUnique<CacheStorage>(dir);
    }

#endif
}

auto SettingsStoreImpl::HasEntry(string_view key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetEntry(key).has_value();
}

auto SettingsStoreImpl::GetEntry(string_view key) const -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    return RegistryReadValue(_subKey, key);
#else

    if (_cache && _cache->HasEntry(key)) {
        return _cache->GetString(key);
    }

    return std::nullopt;

#endif
}

void SettingsStoreImpl::SetEntry(string_view key, string_view value)
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    RegistryWriteValue(_subKey, key, value);
#else

    if (_cache) {
        _cache->SetString(key, value);
    }

#endif
}

void SettingsStoreImpl::RemoveEntry(string_view key)
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    RegistryDeleteValue(_subKey, key);
#else

    if (_cache) {
        _cache->RemoveEntry(key);
    }

#endif
}

SettingsStore::SettingsStore(string_view app_name) :
    _impl {SafeAlloc::MakeUnique<SettingsStoreImpl>(app_name)}
{
    FO_STACK_TRACE_ENTRY();
}

SettingsStore::SettingsStore(SettingsStore&&) noexcept = default;
SettingsStore::~SettingsStore() = default;

auto SettingsStore::HasKey(string_view key) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _impl->HasEntry(key);
}

auto SettingsStore::GetString(string_view key, string_view default_value) const -> string
{
    FO_STACK_TRACE_ENTRY();

    auto entry = _impl->GetEntry(key);
    return entry ? *entry : string(default_value);
}

auto SettingsStore::GetInt(string_view key, int64_t default_value) const -> int64_t
{
    FO_STACK_TRACE_ENTRY();

    auto entry = _impl->GetEntry(key);
    return entry ? strex(*entry).to_int64() : default_value;
}

auto SettingsStore::GetBool(string_view key, bool default_value) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto entry = _impl->GetEntry(key);
    return entry ? strex(*entry).to_bool() : default_value;
}

auto SettingsStore::GetFloat(string_view key, float64_t default_value) const -> float64_t
{
    FO_STACK_TRACE_ENTRY();

    auto entry = _impl->GetEntry(key);
    return entry ? strex(*entry).to_float64() : default_value;
}

void SettingsStore::SetString(string_view key, string_view value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetEntry(key, value);
}

void SettingsStore::SetInt(string_view key, int64_t value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetEntry(key, strex("{}", value).str());
}

void SettingsStore::SetBool(string_view key, bool value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetEntry(key, value ? "1" : "0");
}

void SettingsStore::SetFloat(string_view key, float64_t value)
{
    FO_STACK_TRACE_ENTRY();

    _impl->SetEntry(key, strex("{}", value).str());
}

void SettingsStore::Remove(string_view key)
{
    FO_STACK_TRACE_ENTRY();

    _impl->RemoveEntry(key);
}

FO_END_NAMESPACE
