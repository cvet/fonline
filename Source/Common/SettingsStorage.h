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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

class SettingsStorageImpl;

// Small per-user key/value store for tool and editor settings (ImGui window layout, editor options, last
// selection, ...). Values are scoped by an application name so different tools never collide. On Windows the
// backing store is the registry under HKCU\Software\FOnline\<app_name>; on other platforms it is a file store
// under the per-user data directory. Persistence is best-effort: a backend failure is logged, never thrown, so a
// tool never dies because its settings could not be written.
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
