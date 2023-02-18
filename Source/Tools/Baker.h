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

#include "DataSource.h"
#include "FileSystem.h"
#include "Settings.h"

DECLARE_EXCEPTION(ProtoValidationException);

class Properties;
class ScriptSystem;

class BaseBaker
{
public:
    using BakeCheckerCallback = std::function<bool(const FileHeader&)>;
    using WriteDataCallback = std::function<void(string_view, const_span<uint8>)>;

    BaseBaker() = delete;
    BaseBaker(BakerSettings& settings, FileCollection&& files, BakeCheckerCallback&& bake_checker, WriteDataCallback&& write_data);
    BaseBaker(const BaseBaker&) = delete;
    BaseBaker(BaseBaker&&) noexcept = default;
    auto operator=(const BaseBaker&) = delete;
    auto operator=(BaseBaker&&) noexcept = delete;
    virtual ~BaseBaker() = default;

    virtual void AutoBake() = 0;

protected:
    BakerSettings& _settings;
    FileCollection _files;
    BakeCheckerCallback _bakeChecker;
    WriteDataCallback _writeData;
};

class Baker final
{
public:
    explicit Baker(BakerSettings& settings);
    Baker(const Baker&) = delete;
    Baker(Baker&&) noexcept = default;
    auto operator=(const Baker&) = delete;
    auto operator=(Baker&&) noexcept = delete;

    void BakeAll();

private:
    [[nodiscard]] auto MakeOutputPath(string_view path) const -> string;
    [[nodiscard]] auto ValidateProperties(const Properties& props, string_view context_str, ScriptSystem* script_sys, const unordered_set<hstring>& resource_hashes) -> int;

    BakerSettings& _settings;
};

class BakerDataSource final : public DataSource
{
public:
    BakerDataSource(FileSystem& input_resources, BakerSettings& settings);
    BakerDataSource(const BakerDataSource&) = delete;
    BakerDataSource(BakerDataSource&&) noexcept = default;
    auto operator=(const BakerDataSource&) = delete;
    auto operator=(BakerDataSource&&) noexcept = delete;
    ~BakerDataSource() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "Baker"; }
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string> override;

private:
    [[nodiscard]] auto FindFile(const string& path) const -> File*;

    FileSystem& _inputResources;
    BakerSettings& _settings;
    mutable unordered_map<string, unique_ptr<File>> _bakedFiles {};
};
