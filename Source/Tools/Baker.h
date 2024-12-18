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
// Copyright (c) 2006 - 2024, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
    BaseBaker(const BakerSettings& settings, BakeCheckerCallback&& bake_checker, WriteDataCallback&& write_data);
    BaseBaker(const BaseBaker&) = delete;
    BaseBaker(BaseBaker&&) noexcept = delete;
    auto operator=(const BaseBaker&) = delete;
    auto operator=(BaseBaker&&) noexcept = delete;
    virtual ~BaseBaker() = default;

    [[nodiscard]] static auto SetupBakers(const BakerSettings& settings, BakeCheckerCallback bake_checker, WriteDataCallback write_data) -> vector<unique_ptr<BaseBaker>>;
    [[nodiscard]] virtual auto IsExtSupported(string_view ext) const -> bool = 0;

    virtual void BakeFiles(FileCollection&& files) = 0;

protected:
    [[nodiscard]] auto GetAsyncMode() const -> std::launch { return _settings.SingleThreadBaking ? std::launch::deferred : std::launch::async | std::launch::deferred; }

    const BakerSettings& _settings;
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
    auto ValidateProperties(const Properties& props, string_view context_str, const ScriptSystem& script_sys, const unordered_set<hstring>& resource_hashes) const -> int;

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

    void WriteData(string_view baked_path, const_span<uint8> baked_data);

    FileSystem& _inputResources;
    BakerSettings& _settings;
    vector<unique_ptr<BaseBaker>> _bakers {};
    mutable unordered_map<string, unique_ptr<File>> _bakedFiles {};
};
