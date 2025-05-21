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

#include "DataSource.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

class Properties;
class ScriptSystem;

class BaseBaker
{
public:
    using BakeCheckerCallback = std::function<bool(const string&, uint64)>;
    using AsyncWriteDataCallback = std::function<void(string_view, const_span<uint8>)>;

    BaseBaker() = delete;
    BaseBaker(const BakerSettings& settings, string&& pack_name, BakeCheckerCallback&& bake_checker, AsyncWriteDataCallback&& write_data, const FileSystem* baked_files);
    BaseBaker(const BaseBaker&) = delete;
    BaseBaker(BaseBaker&&) noexcept = delete;
    auto operator=(const BaseBaker&) = delete;
    auto operator=(BaseBaker&&) noexcept = delete;
    virtual ~BaseBaker() = default;

    [[nodiscard]] static auto SetupBakers(const string& pack_name, const BakerSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, const FileSystem* baked_files) -> vector<unique_ptr<BaseBaker>>;
    [[nodiscard]] virtual auto IsExtSupported(string_view ext) const -> bool = 0;

    virtual void BakeFiles(FileCollection files) = 0;

protected:
    [[nodiscard]] auto GetAsyncMode() const -> std::launch { return _settings->SingleThreadBaking ? std::launch::deferred : std::launch::async | std::launch::deferred; }
    [[nodiscard]] auto ValidateProperties(const Properties& props, string_view context_str, const ScriptSystem* script_sys) const -> int;

    raw_ptr<const BakerSettings> _settings;
    string _packName;
    BakeCheckerCallback _bakeChecker;
    AsyncWriteDataCallback _writeData;
    raw_ptr<const FileSystem> _bakedFiles;
};

class MasterBaker final
{
public:
    explicit MasterBaker(BakerSettings& settings);
    MasterBaker(const MasterBaker&) = delete;
    MasterBaker(MasterBaker&&) noexcept = default;
    auto operator=(const MasterBaker&) = delete;
    auto operator=(MasterBaker&&) noexcept = delete;
    ~MasterBaker() = default;

    void BakeAll();

private:
    [[nodiscard]] auto MakeOutputPath(string_view path) const -> string;

    BakerSettings& _settings;
};

class BakerDataSource final : public DataSource
{
public:
    BakerDataSource(FileSystem& input_resources, BakerSettings& settings);
    BakerDataSource(const BakerDataSource&) = delete;
    BakerDataSource(BakerDataSource&&) noexcept = delete;
    auto operator=(const BakerDataSource&) = delete;
    auto operator=(BakerDataSource&&) noexcept = delete;
    ~BakerDataSource() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "Baker"; }
    [[nodiscard]] auto IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view path, bool recursive, string_view ext) const -> vector<string> override;

private:
    [[nodiscard]] auto FindFile(string_view path) const -> File*;

    void WriteData(string_view baked_path, const_span<uint8> baked_data);

    FileSystem& _inputResources;
    BakerSettings& _settings;
    mutable vector<unique_ptr<BaseBaker>> _bakers {};
    mutable std::mutex _bakedCacheLocker {};
    mutable unordered_map<string, unique_ptr<File>> _bakedCache {};
    bool _nonConstHelper {};
};

class BakerEngine : public EngineData
{
public:
    explicit BakerEngine(PropertiesRelationType props_relation);
    static auto GetRestoreInfo() -> vector<uint8>;
};

class BakerScriptSystem final : public ScriptSystem
{
public:
    explicit BakerScriptSystem(BakerEngine& engine, const FileSystem& resources);
};

FO_END_NAMESPACE();
