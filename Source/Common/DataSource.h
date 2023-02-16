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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

DECLARE_EXCEPTION(DataSourceException);

enum class DataSourceType
{
    Default,
    DirRoot,
    NonCachedDirRoot,
    MaybeNotAvailable,
};

class DataSource
{
public:
    DataSource() = default;
    DataSource(const DataSource&) = delete;
    DataSource(DataSource&&) noexcept = default;
    auto operator=(const DataSource&) = delete;
    auto operator=(DataSource&&) noexcept = delete;
    virtual ~DataSource() = default;

    static auto Create(string_view path, DataSourceType type) -> unique_ptr<DataSource>;

    [[nodiscard]] virtual auto IsDiskDir() const -> bool = 0;
    [[nodiscard]] virtual auto GetPackName() const -> string_view = 0;
    [[nodiscard]] virtual auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool = 0;
    [[nodiscard]] virtual auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uchar> = 0;
    [[nodiscard]] virtual auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> = 0;
};
