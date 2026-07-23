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

#include "BakingReport.h"
#include "DataSource.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ResourceBakingException);

inline constexpr string_view BAKER_CACHE_DIR = ".baker-cache";

class Properties;
class ScriptSystem;

using BakeCheckerCallback = function<bool(string_view, uint64_t)>;
using AsyncWriteDataCallback = function<BakingWriteResult(string_view, const_span<uint8_t>)>;

struct BakingContext
{
    nptr<const BakingSettings> Settings {};
    string PackName {};
    BakeCheckerCallback BakeChecker {};
    AsyncWriteDataCallback WriteData {};
    nptr<const FileSystem> BakedFiles {};
    nptr<const FileSystem> PackBakedFiles {};
    optional<bool> ForceSyncMode {};
    shared_ptr<BakingReport> Report {};
    string BakerName {};
    bool OutputDiscovery {};
};

class BaseBaker
{
public:
    explicit BaseBaker(shared_ptr<BakingContext> ctx, string_view baker_name);
    BaseBaker(const BaseBaker&) = delete;
    BaseBaker(BaseBaker&&) noexcept = delete;
    auto operator=(const BaseBaker&) = delete;
    auto operator=(BaseBaker&&) noexcept = delete;
    virtual ~BaseBaker() = default;

    [[nodiscard]] virtual auto GetName() const -> string_view = 0;
    [[nodiscard]] virtual auto GetOrder() const -> int32_t = 0;

    virtual void BakeFiles(const FileCollection& files, string_view target_path = "") const = 0;

    static auto SetupBakers(span<const string> request_bakers, const string& pack_name, const BakingSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, ptr<const FileSystem> baked_files, shared_ptr<BakingReport> report = nullptr, bool output_discovery = false, nptr<const FileSystem> pack_baked_files = nullptr) -> vector<unique_ptr<BaseBaker>>;

protected:
    [[nodiscard]] auto GetAsyncMode() const -> async_launch_mode { return _context->ForceSyncMode.value_or(_context->Settings->SingleThreadBaking) ? launch_deferred_only : launch_async_and_deferred; }
    [[nodiscard]] auto IsBakingReportEnabled() const noexcept -> bool { return _context->Report != nullptr; }
    [[nodiscard]] auto ValidateProperties(const Properties& props, string_view context_str, nptr<const ScriptSystem> script_sys) const -> size_t;

    void AddBakingReportCounter(string_view name, uint64_t value = 1) const;
    void AddBakingReportHistogramValue(string_view name, string_view value, uint64_t count = 1) const;
    void RecordSpriteMeshBakingSettings(const SpriteMeshBakingReportSettings& settings) const;
    void RecordSpriteMeshBakingFrame(const SpriteMeshBakingFrameReport& frame) const;
    void RecordSharedSpriteMeshBakingFrames(uint64_t count) const;

    shared_ptr<BakingContext> _context;
};

class MasterBaker final
{
public:
    explicit MasterBaker(ptr<BakingSettings> settings) noexcept;
    MasterBaker(const MasterBaker&) = delete;
    MasterBaker(MasterBaker&&) noexcept = default;
    auto operator=(const MasterBaker&) = delete;
    auto operator=(MasterBaker&&) noexcept = delete;
    ~MasterBaker() = default;

    auto BakeAll() noexcept -> bool;

private:
    void BakeAllInternal();

    ptr<BakingSettings> _settings;
    shared_ptr<BakingReport> _report {};
};

class BakerDataSource final : public DataSource
{
public:
    explicit BakerDataSource(ptr<BakingSettings> settings);
    BakerDataSource(const BakerDataSource&) = delete;
    BakerDataSource(BakerDataSource&&) noexcept = delete;
    auto operator=(const BakerDataSource&) = delete;
    auto operator=(BakerDataSource&&) noexcept = delete;
    ~BakerDataSource() override = default;

    [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
    [[nodiscard]] auto GetPackName() const -> string_view override { return "Baker"; }
    [[nodiscard]] auto IsFileExists(string_view path) const -> bool override;
    [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override;
    [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override;
    [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override;

    auto Reindex() -> bool override;

private:
    struct ResourcesInputEntry
    {
        string Name {};
        FileSystem InputDir {};
        FileCollection InputFiles {};
        vector<unique_ptr<BaseBaker>> Bakers {};
    };

    [[nodiscard]] auto MakeOutputPath(string_view res_pack_name, string_view path) const -> string;
    [[nodiscard]] auto ResolveFilePath(string_view path, uint64_t& write_time) const -> optional<string>;
    [[nodiscard]] auto FindFile(string_view path, size_t& size, uint64_t& write_time) const -> bool;

    auto CheckData(string_view res_pack_name, string_view path, uint64_t write_time) -> bool;
    void WriteData(string_view res_pack_name, string_view path, span<const uint8_t> data);

    ptr<BakingSettings> _settings;
    vector<ResourcesInputEntry> _inputResources {};
    unordered_map<string, pair<size_t, uint64_t>> _inputFileIndex {}; // Resource pack/path and input file size/write time
    FileSystem _outputResources {};
    mutable mutex _outputFilesLocker {};
    unordered_map<string, uint64_t> _outputFiles FO_TSA_GUARDED_BY(_outputFilesLocker) {}; // Path and input file last write time
};

class BakerServerEngine : public EngineMetadata, public ScriptSystem, public EntityManagerApi
{
public:
    explicit BakerServerEngine(const FileSystem& resources);

    // Entity API stub
    auto CreateCustomInnerEntity(ptr<Entity> /*holder*/, hstring /*entry*/, hstring /*pid*/) -> nptr<Entity> override { return nullptr; }
    auto CreateCustomEntity(hstring /*type_name*/, hstring /*pid*/) -> nptr<Entity> override { return nullptr; }
    auto GetCustomEntity(hstring /*type_name*/, ident_t /*id*/) -> refcount_nptr<Entity> override { return nullptr; }
    void DestroyEntity(ptr<Entity> /*entity*/) override { }
};

class BakerClientEngine : public EngineMetadata
{
public:
    explicit BakerClientEngine(const FileSystem& resources);
};

class BakerMapperEngine : public EngineMetadata
{
public:
    explicit BakerMapperEngine(const FileSystem& resources);
};

FO_END_NAMESPACE
