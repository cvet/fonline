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

FO_DECLARE_EXCEPTION(ResourceBakingException);

class Properties;
class ScriptSystem;

using BakeCheckerCallback = function<bool(string_view, uint64)>;
using AsyncWriteDataCallback = function<void(string_view, span<const uint8>)>;

struct BakerData
{
    raw_ptr<const BakingSettings> Settings {};
    string PackName {};
    BakeCheckerCallback BakeChecker {};
    AsyncWriteDataCallback WriteData {};
    raw_ptr<const FileSystem> BakedFiles {};
};

class BaseBaker
{
public:
    explicit BaseBaker(BakerData& data);
    BaseBaker(const BaseBaker&) = delete;
    BaseBaker(BaseBaker&&) noexcept = delete;
    auto operator=(const BaseBaker&) = delete;
    auto operator=(BaseBaker&&) noexcept = delete;
    virtual ~BaseBaker() = default;

    [[nodiscard]] static auto SetupBakers(const string& pack_name, const BakingSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, const FileSystem* baked_files) -> vector<unique_ptr<BaseBaker>>;

    virtual void BakeFiles(const FileCollection& files, string_view target_path = "") const = 0;

protected:
    [[nodiscard]] auto GetAsyncMode() const -> std::launch { return _settings->SingleThreadBaking ? std::launch::deferred : std::launch::async | std::launch::deferred; }
    [[nodiscard]] auto ValidateProperties(const Properties& props, string_view context_str, const ScriptSystem* script_sys) const -> size_t;

    raw_ptr<const BakingSettings> _settings;
    string _resPackName;
    BakeCheckerCallback _bakeChecker;
    AsyncWriteDataCallback _writeData;
    raw_ptr<const FileSystem> _bakedFiles;
};

class MasterBaker final
{
public:
    explicit MasterBaker(BakingSettings& settings) noexcept;
    MasterBaker(const MasterBaker&) = delete;
    MasterBaker(MasterBaker&&) noexcept = default;
    auto operator=(const MasterBaker&) = delete;
    auto operator=(MasterBaker&&) noexcept = delete;
    ~MasterBaker() = default;

    auto BakeAll() noexcept -> bool;

private:
    void BakeAllInternal();

    raw_ptr<BakingSettings> _settings;
};

class BakerDataSource final : public DataSource
{
public:
    explicit BakerDataSource(BakingSettings& settings);
    BakerDataSource(const BakerDataSource&) = delete;
    BakerDataSource(BakerDataSource&&) noexcept = delete;
    auto operator=(const BakerDataSource&) = delete;
    auto operator=(BakerDataSource&&) noexcept = delete;
    ~BakerDataSource() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "Baker"; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

private:
    struct ResourcesInputEntry
    {
        string Name {};
        FileSystem InputDir {};
        FileCollection InputFiles {};
        vector<unique_ptr<BaseBaker>> Bakers {};
    };

    [[nodiscard]] auto MakeOutputPath(string_view res_pack_name, string_view path) const -> string;
    [[nodiscard]] auto FindFile(string_view path, size_t& size, uint64& write_time, unique_del_ptr<const uint8>* data) const -> bool;

    auto CheckData(string_view res_pack_name, string_view path, uint64 write_time) -> bool;
    void WriteData(string_view res_pack_name, string_view path, span<const uint8> data);

    raw_ptr<BakingSettings> _settings;
    vector<ResourcesInputEntry> _inputResources {};
    FileSystem _outputResources {};
    mutable std::mutex _outputFilesLocker {};
    unordered_map<string, uint64> _outputFiles {}; // Path and input file last write time
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
