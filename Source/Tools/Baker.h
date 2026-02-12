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

#include "DataSource.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ResourceBakingException);

class Properties;
class ScriptSystem;

using BakeCheckerCallback = function<bool(string_view, uint64)>;
using AsyncWriteDataCallback = function<void(string_view, const_span<uint8>)>;

struct BakingContext
{
    raw_ptr<const BakingSettings> Settings {};
    string PackName {};
    BakeCheckerCallback BakeChecker {};
    AsyncWriteDataCallback WriteData {};
    raw_ptr<const FileSystem> BakedFiles {};
    optional<bool> ForceSyncMode {};
};

class BaseBaker
{
public:
    explicit BaseBaker(shared_ptr<BakingContext> ctx);
    BaseBaker(const BaseBaker&) = delete;
    BaseBaker(BaseBaker&&) noexcept = delete;
    auto operator=(const BaseBaker&) = delete;
    auto operator=(BaseBaker&&) noexcept = delete;
    virtual ~BaseBaker() = default;

    [[nodiscard]] virtual auto GetName() const -> string_view = 0;
    [[nodiscard]] virtual auto GetOrder() const -> int32 = 0;

    virtual void BakeFiles(const FileCollection& files, string_view target_path = "") const = 0;

    static auto SetupBakers(span<const string> request_bakers, const string& pack_name, const BakingSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, const FileSystem* baked_files) -> vector<unique_ptr<BaseBaker>>;

protected:
    [[nodiscard]] auto GetAsyncMode() const -> std::launch { return _context->ForceSyncMode.value_or(_context->Settings->SingleThreadBaking) ? std::launch::deferred : std::launch::async | std::launch::deferred; }
    [[nodiscard]] auto ValidateProperties(const Properties& props, string_view context_str, const ScriptSystem* script_sys) const -> size_t;

    shared_ptr<BakingContext> _context;
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

class BakerServerEngine : public EngineMetadata, public ScriptSystem, public EntityManagerApi
{
public:
    explicit BakerServerEngine(const FileSystem& resources);

    // Entity API stub
    auto CreateCustomInnerEntity(Entity* /*holder*/, hstring /*entry*/, hstring /*pid*/) -> Entity* override { return nullptr; }
    auto CreateCustomEntity(hstring /*type_name*/, hstring /*pid*/) -> Entity* override { return nullptr; }
    auto GetCustomEntity(hstring /*type_name*/, ident_t /*id*/) -> Entity* override { return nullptr; }
    void DestroyEntity(Entity* /*entity*/) override { }
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
