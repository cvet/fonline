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

class CacheStorageImpl;

class CacheStorage
{
public:
    explicit CacheStorage(u8string_view path);
    explicit CacheStorage(u8string path);
    CacheStorage(const CacheStorage&) = delete;
    CacheStorage(CacheStorage&&) noexcept;
    auto operator=(const CacheStorage&) = delete;
    auto operator=(CacheStorage&&) noexcept = delete;
    ~CacheStorage();

    [[nodiscard]] auto HasEntry(string_view entry_name) const -> bool;
    [[nodiscard]] auto GetText(string_view entry_name) const -> u8string;
    [[nodiscard]] auto GetBytes(string_view entry_name) const -> vector<byte>;

    void SetText(string_view entry_name, string_view text);
    void SetText(string_view entry_name, u8string_view text);
    void SetBytes(string_view entry_name, const_span<byte> bytes);
    void RemoveEntry(string_view entry_name);

private:
    unique_ptr<CacheStorageImpl> _impl;
};

FO_END_NAMESPACE
