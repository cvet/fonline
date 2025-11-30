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

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(DataSourceException);

class DataSource
{
public:
    DataSource() = default;
    DataSource(const DataSource&) = delete;
    DataSource(DataSource&&) noexcept = delete;
    auto operator=(const DataSource&) = delete;
    auto operator=(DataSource&&) noexcept = delete;
    virtual ~DataSource() = default;

    static auto MountDir(string_view dir, bool recursive, bool non_cached, bool maybe_not_available) -> unique_ptr<DataSource>;
    static auto MountPack(string_view dir, string_view name, bool maybe_not_available) -> unique_ptr<DataSource>;

    [[nodiscard]] virtual auto IsDiskDir() const -> bool = 0;
    [[nodiscard]] virtual auto GetPackName() const -> string_view = 0;
    [[nodiscard]] virtual auto IsFileExists(string_view path) const -> bool = 0;
    [[nodiscard]] virtual auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool = 0;
    [[nodiscard]] virtual auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> = 0;
    [[nodiscard]] virtual auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> = 0;
};

class DataSourceRef : public DataSource
{
public:
    explicit DataSourceRef(const DataSource* ds) :
        _dataSource {ds}
    {
    }
    DataSourceRef(const DataSourceRef&) = delete;
    DataSourceRef(DataSourceRef&&) noexcept = delete;
    auto operator=(const DataSourceRef&) = delete;
    auto operator=(DataSourceRef&&) noexcept = delete;
    ~DataSourceRef() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return _dataSource->IsDiskDir(); }
    [[nodiscard]] auto GetPackName() const -> string_view override { return _dataSource->GetPackName(); }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override { return _dataSource->IsFileExists(path); }
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override { return _dataSource->GetFileInfo(path, size, write_time); }
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override { return _dataSource->OpenFile(path, size, write_time); }
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override { return _dataSource->GetFileNames(dir, recursive, ext); }

private:
    raw_ptr<const DataSource> _dataSource;
};

FO_END_NAMESPACE();
