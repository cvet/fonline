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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: store Cache.bin in player local dir for Windows users?
// Todo: add in-memory cache storage and fallback to it if can't create default

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE();

class CacheStorageImpl;

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
    [[nodiscard]] auto GetData(string_view entry_name) const -> vector<uint8>;

    void SetString(string_view entry_name, string_view str);
    void SetData(string_view entry_name, span<const uint8> data);
    void RemoveEntry(string_view entry_name);

private:
    unique_ptr<CacheStorageImpl> _impl;
};

FO_END_NAMESPACE();
