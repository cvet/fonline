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

#include "Baker.h"
#include "AngelScriptBaker.h"
#include "Application.h"
#include "ConfigBaker.h"
#include "ConfigFile.h"
#include "EffectBaker.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "ImageBaker.h"
#include "MapBaker.h"
#include "MapLoader.h"
#include "MetadataBaker.h"
#include "MetadataRegistration.h"
#include "ModelInfoBaker.h"
#include "ModelMeshBaker.h"
#include "ParticleBaker.h"
#include "ProtoBaker.h"
#include "ProtoManager.h"
#include "ProtoTextBaker.h"
#include "RawCopyBaker.h"
#include "ScriptSystem.h"
#include "Settings.h"
#include "TextBaker.h"

FO_BEGIN_NAMESPACE

namespace BakerStub
{
    class Item
    {
    };
    class StaticItem
    {
    };
    class Critter
    {
    };
    class Map
    {
    };
    class Location
    {
    };
}

void SetupBakersHook(span<const string>, vector<unique_ptr<BaseBaker>>&, shared_ptr<BakingContext>);

BaseBaker::BaseBaker(shared_ptr<BakingContext> ctx, string_view baker_name)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx, "Missing required context");
    FO_VERIFY_AND_THROW(ctx->WriteData, "Baker context has no output writer");
    FO_VERIFY_AND_THROW(!baker_name.empty(), "Baker name is empty");

    _context = safe_alloc::make_shared<BakingContext>(*ctx);
    _context->BakerName = baker_name;

    if (_context->Report) {
        shared_ptr<BakingReport> report = _context->Report;
        string pack_name = _context->PackName;
        string stored_baker_name = _context->BakerName;

        if (ctx->BakeChecker) {
            BakeCheckerCallback bake_checker = ctx->BakeChecker;
            _context->BakeChecker = [report, pack_name, stored_baker_name, bake_checker](string_view path, uint64_t write_time) mutable {
                bool scheduled = bake_checker(path, write_time);
                report->RecordOutputCheck(pack_name, stored_baker_name, path, scheduled);
                return scheduled;
            };
        }

        AsyncWriteDataCallback write_data = ctx->WriteData;
        _context->WriteData = [report, pack_name, stored_baker_name, write_data](string_view path, const_span<uint8_t> data) mutable {
            BakingWriteResult result = write_data(path, data);
            report->RecordOutputSubmission(pack_name, stored_baker_name, path, data.size(), result);
            return result;
        };
    }
}

auto BaseBaker::SetupBakers(span<const string> request_bakers, const string& pack_name, const BakingSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, ptr<const FileSystem> baked_files, shared_ptr<BakingReport> report, bool output_discovery, nptr<const FileSystem> pack_baked_files) -> vector<unique_ptr<BaseBaker>>
{
    FO_STACK_TRACE_ENTRY();

    vector<unique_ptr<BaseBaker>> bakers;

    auto ctx = safe_alloc::make_shared<BakingContext>(BakingContext {.Settings = make_ptr(&settings), .PackName = pack_name, .BakeChecker = bake_checker, .WriteData = write_data, .BakedFiles = baked_files, .PackBakedFiles = pack_baked_files, .Report = std::move(report), .OutputDiscovery = output_discovery});

    if (vec_exists(request_bakers, MetadataBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<MetadataBaker>(ctx));
    }
    if (vec_exists(request_bakers, ConfigBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<ConfigBaker>(ctx));
    }
    if (vec_exists(request_bakers, RawCopyBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<RawCopyBaker>(ctx));
    }
    if (vec_exists(request_bakers, ImageBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<ImageBaker>(ctx));
    }
    if (vec_exists(request_bakers, EffectBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<EffectBaker>(ctx));
    }
    if (vec_exists(request_bakers, ParticleBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<ParticleBaker>(ctx));
    }
    if (vec_exists(request_bakers, ProtoBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<ProtoBaker>(ctx));
    }
    if (vec_exists(request_bakers, MapBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<MapBaker>(ctx));
    }
    if (vec_exists(request_bakers, TextBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<TextBaker>(ctx));
    }
    if (vec_exists(request_bakers, ProtoTextBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<ProtoTextBaker>(ctx));
    }
#if FO_ENABLE_3D
    if (vec_exists(request_bakers, ModelMeshBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<ModelMeshBaker>(ctx));
    }
    if (vec_exists(request_bakers, ModelInfoBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<ModelInfoBaker>(ctx));
    }
#endif
#if FO_ANGELSCRIPT_SCRIPTING
    if (vec_exists(request_bakers, AngelScriptBaker::NAME)) {
        bakers.emplace_back(safe_alloc::make_unique<AngelScriptBaker>(ctx));
    }
#endif

    SetupBakersHook(request_bakers, bakers, std::move(ctx));

    return bakers;
}

void BaseBaker::AddBakingReportCounter(string_view name, uint64_t value) const
{
    FO_STACK_TRACE_ENTRY();

    if (_context->Report) {
        shared_ptr<BakingReport> report = _context->Report;
        report->AddCounter(_context->PackName, _context->BakerName, name, value);
    }
}

void BaseBaker::AddBakingReportHistogramValue(string_view name, string_view value, uint64_t count) const
{
    FO_STACK_TRACE_ENTRY();

    if (_context->Report) {
        shared_ptr<BakingReport> report = _context->Report;
        report->AddHistogramValue(_context->PackName, _context->BakerName, name, value, count);
    }
}

void BaseBaker::RecordSpriteMeshBakingSettings(const SpriteMeshBakingReportSettings& settings) const
{
    FO_STACK_TRACE_ENTRY();

    if (_context->Report) {
        shared_ptr<BakingReport> report = _context->Report;
        report->RecordSpriteMeshSettings(_context->PackName, _context->BakerName, settings);
    }
}

void BaseBaker::RecordSpriteMeshBakingFrame(const SpriteMeshBakingFrameReport& frame) const
{
    FO_STACK_TRACE_ENTRY();

    if (_context->Report) {
        shared_ptr<BakingReport> report = _context->Report;
        report->RecordSpriteMeshFrame(_context->PackName, _context->BakerName, frame);
    }
}

void BaseBaker::RecordSharedSpriteMeshBakingFrames(uint64_t count) const
{
    FO_STACK_TRACE_ENTRY();

    if (_context->Report) {
        shared_ptr<BakingReport> report = _context->Report;
        report->RecordSharedSpriteMeshFrames(_context->PackName, _context->BakerName, count);
    }
}

MasterBaker::MasterBaker(ptr<BakingSettings> settings) noexcept :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();
}

auto MasterBaker::BakeAll() noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    bool success = false;
    string failure_message;
    string report_path;

    try {
        _report = safe_alloc::make_shared<BakingReport>(_settings);
        report_path = GetBakingReportPath(_settings->BakeOutput);

        if (!report_path.empty()) {
            bool remove_old_report_ok = fs::remove_file(report_path);
            FO_VERIFY_AND_THROW(remove_old_report_ok, "Unable to delete the previous baking report", report_path);
        }

        BakeAllInternal();
        success = true;
    }
    catch (const std::exception& ex) {
        logging::write("Baking error: {}", ex.what());
        failure_message = ex.what();
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    if (_report) {
        _report->Complete(success, failure_message);

        if (!report_path.empty()) {
            try {
                string report_dir = strex(report_path).extract_dir();
                if (!report_dir.empty()) {
                    bool create_report_dir_ok = fs::create_directories(report_dir);
                    FO_VERIFY_AND_THROW(create_report_dir_ok, "Unable to create the baking report directory", report_dir);
                }

                string report_data = _report->Serialize();
                bool write_report_ok = fs::write_file(report_path, report_data);
                FO_VERIFY_AND_THROW(write_report_ok, "Unable to write the baking report", report_path);
                logging::write("Baking report saved to {}", report_path);

                if (success && _report->IsFullRebuild()) {
                    string full_report_path = GetFullBakingReportPath(_settings->BakeOutput);
                    bool write_full_report_ok = fs::write_file(full_report_path, report_data);
                    FO_VERIFY_AND_THROW(write_full_report_ok, "Unable to write the full baking report", full_report_path);
                    logging::write("Full baking report saved to {}", full_report_path);
                }
            }
            catch (const std::exception& ex) {
                logging::write("Baking report error: {}", ex.what());
                success = false;
            }
        }
    }

    return success;
}

struct MasterBaker::PackBakeContext
{
    string PackName {};
    string OutputDir {};
    FileSystem InputFiles {};
    FileSystem PackBakedFiles {};
    FileCollection FilteredFiles {};
    vector<unique_ptr<BaseBaker>> Bakers {};
    shared_ptr<BakingReport> Report {};
    uint64_t InputBytes {};
    std::atomic_int BakedFiles {};
    mutex BakedFilePathsLocker {};
    unordered_set<string> BakedFilePaths FO_TSA_GUARDED_BY(BakedFilePathsLocker) {};
    bool FirstBake {};
    bool OutputAdded {};
    time_meter BakingTime {};
    bool Done {};
};

// What the bakers addressed this run, in the two shapes the output sweeps need: resource identity for deciding
// what is outdated, and exact relative paths for deciding what is merely misspelled
struct MasterBaker::ExpectedOutputs
{
    unordered_set<string> ResourceNames {};
    unordered_map<string, string> Paths {};
};

// Resource identity ignores the extension chain (one input can produce several differently suffixed outputs) and
// letter case, so a name folded this way answers "is this output still wanted", never "is it spelled right"
static auto ExcludeAllExt(string_view path) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    size_t pos = path.rfind('/');
    pos = path.find('.', pos != string::npos ? pos : 0);
    return strex(pos != string::npos ? path.substr(0, pos) : path).lower();
}

void MasterBaker::BakeAllInternal()
{
    FO_STACK_TRACE_ENTRY();

    time_meter backing_time;

    logging::write("Start baking");

    FO_VERIFY_AND_THROW(!_settings->BakeOutput.empty(), "Resource baker cannot write outputs because BakeOutput is empty", _settings->GetResourcePacks().size());

    string build_hash_path = MakeOutputPath("Resources.build-hash");
    std::atomic_bool force_baking = ResolveRebuildMode(build_hash_path);

    // Outputs of already baked packs are mounted here as they complete, so a later pack can read what an earlier
    // one produced
    FileSystem baking_output;

    // Outlives the pack contexts, which borrow from it rather than mounting their own copy
    auto input_dirs = MountSharedInputDirs();
    auto pack_bake_contexts = PreparePackContexts(input_dirs, baking_output, force_baking);
    RunPackBakers(pack_bake_contexts, baking_output, force_baking);

    ExpectedOutputs expected = CollectExpectedOutputs(pack_bake_contexts);
    ReconcileStaleCasedOutputDirs(expected);
    SweepOutdatedOutputs(expected);
    SweepOutdatedBakerCache(expected);

    logging::write("Time {}", backing_time.get_duration());
    logging::write("Baking complete!");

    bool build_hash_write_ok = fs::write_file(build_hash_path, FO_BUILD_HASH);
    FO_VERIFY_AND_THROW(build_hash_write_ok, "Unable to write the build hash file", build_hash_path);
}

auto MasterBaker::MakeOutputPath(string_view path) const -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex(_settings->BakeOutput).combine_path(path);
}

// Decides whether this run reuses the existing output or starts from scratch. The build hash is deleted up front
// and rewritten only on success, so an aborted run cannot leave a half-written tree looking like a complete base
auto MasterBaker::ResolveRebuildMode(string_view build_hash_path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto prev_build_hash = fs::read_file(build_hash_path);
    bool build_hash_deleted = fs::remove_file(build_hash_path);
    FO_VERIFY_AND_THROW(build_hash_deleted, "Unable to delete the previous build hash file", build_hash_path);

    bool force_baking = false;
    string rebuild_reason = "incremental";

    if (_settings->ForceBaking) {
        logging::write("Force rebuild all resources");
        force_baking = true;
        rebuild_reason = "requested";
    }
    else if (prev_build_hash.has_value() && prev_build_hash.value() != FO_BUILD_HASH) {
        logging::write("Force rebuild all resources due to build hash changed");
        force_baking = true;
        rebuild_reason = "build_hash_changed";
    }

    if (force_baking) {
        bool delete_output_ok = fs::remove_dir_tree(_settings->BakeOutput);
        FO_VERIFY_AND_THROW(delete_output_ok, "Unable to delete baking output dir");
    }

    // A missing hash means the previous run never finished, so the tree is rebuilt - but not deleted, since the
    // partial output is still a valid starting point once every file is re-checked
    if (!prev_build_hash.has_value()) {
        force_baking = true;

        if (rebuild_reason == "incremental") {
            rebuild_reason = "missing_build_hash";
        }
    }

    _report->SetRebuildMode(force_baking, rebuild_reason);

    bool make_output_ok = fs::create_directories(_settings->BakeOutput);
    FO_VERIFY_AND_THROW(make_output_ok, "Unable to recreate baking output dir");

    return force_baking;
}

// One mount per distinct input dir: a dozen packs may share one art tree, and a private mount per pack walked
// it a dozen times. Done here, not on demand, because mounting walks the tree while packs prepare concurrently
auto MasterBaker::MountSharedInputDirs() const -> unordered_map<string, unique_ptr<DataSource>>
{
    FO_STACK_TRACE_ENTRY();

    unordered_map<string, unique_ptr<DataSource>> input_dirs;

    for (const auto& res_pack : _settings->GetResourcePacks()) {
        for (const auto& input_dir : res_pack.InputDirs) {
            if (!input_dirs.contains(input_dir)) {
                input_dirs.emplace(input_dir, DataSource::MountDir(input_dir, true, false, false));
            }
        }
    }

    return input_dirs;
}

auto MasterBaker::PreparePackContexts(unordered_map<string, unique_ptr<DataSource>>& input_dirs, FileSystem& baking_output, std::atomic_bool& force_baking) -> vector<unique_ptr<PackBakeContext>>
{
    FO_STACK_TRACE_ENTRY();

    const auto& res_packs = _settings->GetResourcePacks();
    async_launch_mode async_mode = _settings->SingleThreadBaking ? launch_deferred_only : launch_async_and_deferred;

    vector<std::future<unique_ptr<PackBakeContext>>> prepare_res_bakings;

    for (const auto& res_pack : res_packs) {
        auto res_pack_ptr = make_ptr(&res_pack);
        string output_path = MakeOutputPath(res_pack.Name);
        prepare_res_bakings.emplace_back(run_async(async_mode, strex("PreparePack-{}", res_pack_ptr->Name), [this, res_pack_ptr, output_path, &input_dirs, &baking_output, &force_baking]() FO_DEFERRED { return PreparePackContext(*res_pack_ptr, input_dirs, output_path, baking_output, force_baking); }));
    }

    vector<unique_ptr<PackBakeContext>> pack_bake_contexts;
    string first_prepare_error;
    size_t errors = 0;

    for (auto& prepare_res_baking : prepare_res_bakings) {
        try {
            auto pack_bake_context = prepare_res_baking.get();
            pack_bake_contexts.emplace_back(std::move(pack_bake_context));
        }
        catch (const std::exception& ex) {
            logging::write("Resource pack prepare for baking error: {}", ex.what());

            if (first_prepare_error.empty()) {
                first_prepare_error = ex.what();
            }

            errors++;
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (errors != 0) {
        throw ResourceBakingException("Resource pack preparation failed", first_prepare_error, errors);
    }

    return pack_bake_contexts;
}

auto MasterBaker::PreparePackContext(const ResourcePackInfo& res_pack, unordered_map<string, unique_ptr<DataSource>>& input_dirs, const string& output_dir, FileSystem& baking_output, std::atomic_bool& force_baking) -> unique_ptr<PackBakeContext>
{
    FO_STACK_TRACE_ENTRY();

    auto pack_bake_context = safe_alloc::make_unique<PackBakeContext>();
    auto pack_bake_context_ptr = pack_bake_context.as_ptr();
    shared_ptr<BakingReport> pack_report = _report;

    pack_bake_context->PackName = res_pack.Name;
    pack_bake_context->OutputDir = output_dir;
    pack_bake_context->Report = pack_report;
    pack_bake_context->PackBakedFiles.AddDirSource(output_dir, true, true, true);

    // Read-only here, as the concurrent prepare requires; the borrow is non-const only because an owner
    // propagates const to what it owns and DataSourceRef forwards a non-const Reindex
    for (const auto& input_dir : res_pack.InputDirs) {
        auto it = input_dirs.find(input_dir);
        FO_VERIFY_AND_THROW(it != input_dirs.end(), "Resource pack input dir was not mounted before the prepare pass", res_pack.Name, input_dir);
        pack_bake_context_ptr->InputFiles.AddCustomSource(safe_alloc::make_unique<DataSourceRef>(it->second.as_ptr()));
    }
    for (const auto& input_file : res_pack.InputFiles) {
        string dir = strex(input_file).extract_dir().str();
        string pack = strex(input_file).extract_file_name().erase_file_extension().str();
        pack_bake_context_ptr->InputFiles.AddCustomSource(DataSource::MountPack(dir, pack, false));
    }

    pack_bake_context->FilteredFiles = pack_bake_context->InputFiles.FilterFiles(res_pack.IncludePatterns, res_pack.ExcludePatterns);

    for (const FileHeader& file : pack_bake_context->FilteredFiles) {
        pack_report->RecordPackInput(res_pack.Name, file.GetPath(), file.GetSize());
        pack_bake_context->InputBytes += numeric_cast<uint64_t>(file.GetSize());
    }

    auto bake_checker = [context = pack_bake_context_ptr, &force_baking](string_view path, uint64_t write_time) mutable -> bool {
        // ModelInfoBaker fans BakeChecker calls across PPL tasks, so the path set has
        // to be guarded; without it concurrent emplace() races on the bucket array
        {
            scoped_lock lock {context->BakedFilePathsLocker};

            context->BakedFilePaths.emplace(path);
        }

        if (!force_baking) {
            uint64_t file_write_time = fs::last_write_time(strex(context->OutputDir).combine_path(path));
            return write_time > file_write_time;
        }
        else {
            return true;
        }
    };

    auto write_data = [context = pack_bake_context_ptr](string_view path, span<const uint8_t> baked_data) mutable -> BakingWriteResult {
        string res_path = strex(context->OutputDir).combine_path(path).str();

        if (!fs::compare_file_content(res_path, baked_data)) {
            bool res_file_write_ok = fs::write_file(res_path, baked_data);
            FO_VERIFY_AND_THROW(res_file_write_ok, "Unable to write baked resource file", res_path);
            ++context->BakedFiles;
            return BakingWriteResult::Changed;
        }
        else {
            bool res_file_touch_ok = fs::touch_file(res_path);
            FO_VERIFY_AND_THROW(res_file_touch_ok, "Unable to update the timestamp of an unchanged baked resource file", res_path);
            return BakingWriteResult::Unchanged;
        }
    };

    pack_bake_context->Bakers = BaseBaker::SetupBakers(res_pack.Bakers, res_pack.Name, *_settings, bake_checker, write_data, &baking_output, pack_report, false, &pack_bake_context->PackBakedFiles);

    for (const auto& baker : pack_bake_context->Bakers) {
        pack_report->RecordBakerRegistration(res_pack.Name, baker->GetName(), baker->GetOrder());
    }

    pack_bake_context->BakingTime.pause();
    return pack_bake_context;
}

// Bakers carry an order number and packs advance through those orders together, so a baker that depends on
// another pack's earlier-order output always finds it already written
void MasterBaker::RunPackBakers(vector<unique_ptr<PackBakeContext>>& pack_bake_contexts, FileSystem& baking_output, std::atomic_bool& force_baking)
{
    FO_STACK_TRACE_ENTRY();

    async_launch_mode async_mode = _settings->SingleThreadBaking ? launch_deferred_only : launch_async_and_deferred;
    int32_t bake_order = -10;

    while (true) {
        vector<std::future<void>> res_bakings;
        string first_bake_error;
        size_t errors = 0;

        for (auto& bake_context_holder : pack_bake_contexts) {
            if (!bake_context_holder->Done) {
                auto bake_context = bake_context_holder.as_ptr();
                res_bakings.emplace_back(run_async(async_mode, strex("BakePack-{}-order{}", bake_context->PackName, bake_order), [bake_context, bake_order]() FO_DEFERRED { BakePackOrder(bake_context, bake_order); }));
            }
        }

        for (auto& res_baking : res_bakings) {
            try {
                res_baking.get();
            }
            catch (const std::exception& ex) {
                logging::write("Resource pack baking error: {}", ex.what());

                if (first_bake_error.empty()) {
                    first_bake_error = ex.what();
                }

                errors++;
            }
            catch (...) {
                FO_UNKNOWN_EXCEPTION();
            }
        }

        if (errors != 0) {
            throw ResourceBakingException("Baking resource packs failed", first_bake_error, errors);
        }

        for (auto& bake_context : pack_bake_contexts) {
            if (!bake_context->OutputAdded && bake_context->FirstBake) {
                baking_output.AddDirSource(bake_context->OutputDir, true, true);
                bake_context->OutputAdded = true;
            }

            // Any produced file invalidates the incremental assumption for the orders still to come, because a
            // later baker may read what was just rewritten
            if (bake_context->BakedFiles != 0) {
                force_baking = true;
            }
        }

        if (std::ranges::all_of(pack_bake_contexts, [](auto&& context) { return context->Done; })) {
            break;
        }

        bake_order++;
    }
}

void MasterBaker::BakePackOrder(ptr<PackBakeContext> bake_context, int32_t bake_order)
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i != bake_context->Bakers.size(); ++i) {
        auto baker = bake_context->Bakers[i].as_ptr();

        if (baker->GetOrder() == bake_order) {
            if (!bake_context->FirstBake) {
                logging::write("Bake {}", bake_context->PackName);
                bake_context->FirstBake = true;

                bake_context->BakingTime.resume();
                bool make_res_output_ok = fs::create_directories(bake_context->OutputDir);
                FO_VERIFY_AND_THROW(make_res_output_ok, "Unable to create the resource pack output directory", bake_context->OutputDir);
                bake_context->BakingTime.pause();
            }

            bake_context->BakingTime.resume();
            time_meter baker_time;

            try {
                baker->BakeFiles(bake_context->FilteredFiles);
                bake_context->BakingTime.pause();
                bake_context->Report->RecordBakerInvocation(bake_context->PackName, baker->GetName(), baker->GetOrder(), bake_context->FilteredFiles.GetFilesCount(), bake_context->InputBytes, baker_time.get_duration().milliseconds(), true, {});
            }
            catch (const std::exception& ex) {
                bake_context->BakingTime.pause();
                bake_context->Report->RecordBakerInvocation(bake_context->PackName, baker->GetName(), baker->GetOrder(), bake_context->FilteredFiles.GetFilesCount(), bake_context->InputBytes, baker_time.get_duration().milliseconds(), false, ex.what());
                throw;
            }
            catch (...) {
                FO_UNKNOWN_EXCEPTION();
            }
        }
    }

    // Check if it's last iteration for this pack
    if (bake_context->FirstBake && !bake_context->Done) {
        auto it = std::ranges::max_element(bake_context->Bakers, {}, [](auto&& baker) { return baker->GetOrder(); });
        FO_VERIFY_AND_THROW(it != bake_context->Bakers.end(), "Lookup failed in bake context bakers");
        int32_t max_order = (*it)->GetOrder();

        if (bake_order == max_order) {
            logging::write("Baking of {} complete in {}, baked {} file{}", bake_context->PackName, //
                bake_context->BakingTime.get_duration(), bake_context->BakedFiles, bake_context->BakedFiles != 1 ? "s" : "");
            bake_context->Report->RecordPackDuration(bake_context->PackName, bake_context->BakingTime.get_duration().milliseconds());
            bake_context->Done = true;
        }
    }
}

auto MasterBaker::CollectExpectedOutputs(vector<unique_ptr<PackBakeContext>>& pack_bake_contexts) const -> ExpectedOutputs
{
    FO_STACK_TRACE_ENTRY();

    ExpectedOutputs expected;

    for (auto& bake_context : pack_bake_contexts) {
        scoped_lock lock {bake_context->BakedFilePathsLocker};

        for (const auto& res_name : bake_context->BakedFilePaths) {
            string expected_path = strex(bake_context->PackName).combine_path(res_name);
            expected.ResourceNames.emplace(ExcludeAllExt(expected_path));
            expected.Paths.insert_or_assign(strex(expected_path).lower().str(), expected_path);
        }
    }

    return expected;
}

// Directories go stale by letter case exactly like files, and nothing else repairs them. Directories before files
// and shallowest first, so every rename lands inside a parent already spelled right (Docs/BakingPipeline.md)
void MasterBaker::ReconcileStaleCasedOutputDirs(const ExpectedOutputs& expected)
{
    FO_STACK_TRACE_ENTRY();

    set<string> expected_dirs;

    for (const auto& expected_path : expected.Paths | std::views::values) {
        for (size_t pos = expected_path.find('/'); pos != string::npos; pos = expected_path.find('/', pos + 1)) {
            expected_dirs.emplace(expected_path.substr(0, pos));
        }
    }

    vector<string> ordered_dirs {expected_dirs.begin(), expected_dirs.end()};

    std::ranges::sort(ordered_dirs, [](const string& lhs, const string& rhs) {
        size_t lhs_depth = numeric_cast<size_t>(std::count(lhs.begin(), lhs.end(), '/'));
        size_t rhs_depth = numeric_cast<size_t>(std::count(rhs.begin(), rhs.end(), '/'));
        return lhs_depth != rhs_depth ? lhs_depth < rhs_depth : lhs < rhs;
    });

    for (const string& expected_dir : ordered_dirs) {
        string parent_dir = MakeOutputPath(strex(expected_dir).extract_dir().str());
        string expected_name = strex(expected_dir).extract_file_name().str();

        if (!fs::is_dir(parent_dir)) {
            continue;
        }

        string stale_name;
        bool expected_name_present = false;
        std::error_code ec;

        for (const auto& entry : std::filesystem::directory_iterator {std::filesystem::path {fs::make_path(parent_dir)}, ec}) {
            if (!entry.is_directory()) {
                continue;
            }

            string entry_name = fs::path_to_string(entry.path().filename());

            if (entry_name == expected_name) {
                expected_name_present = true;
                continue;
            }

            if (strvex(entry_name).compare_ignore_case(expected_name)) {
                stale_name = entry_name;
            }
        }

        if (stale_name.empty()) {
            continue;
        }

        // Both spellings listed means a case-sensitive file system kept the pre-rename leftover beside the
        // directory this run baked into, so it is dropped rather than renamed onto the live one
        if (expected_name_present) {
            bool remove_stale_ok = fs::remove_dir_tree(strex(parent_dir).combine_path(stale_name));
            FO_VERIFY_AND_THROW(remove_stale_ok, "Unable to delete the stale-cased duplicate baked output dir", parent_dir, stale_name, expected_name);
            logging::write("Delete stale-cased duplicate dir {}, kept {}", strex(parent_dir).combine_path(stale_name), expected_dir);
            continue;
        }

        bool rename_ok = fs::rename(strex(parent_dir).combine_path(stale_name), strex(parent_dir).combine_path(expected_name));
        FO_VERIFY_AND_THROW(rename_ok, "Unable to rename the stale-cased baked output dir", parent_dir, stale_name, expected_name);
        logging::write("Rename stale-cased dir {} to {}", strex(parent_dir).combine_path(stale_name), expected_dir);
    }
}

// Drops outputs no baker claims any more and re-spells the ones still wanted but sitting under a stale name; one
// pass because both decisions read the same directory (Docs/BakingPipeline.md)
void MasterBaker::SweepOutdatedOutputs(const ExpectedOutputs& expected)
{
    FO_STACK_TRACE_ENTRY();

    vector<pair<string, string>> stale_cased_paths;

    // Exact spellings seen on disk. A case-sensitive file system can hold both the stale and the expected
    // spelling at once, and then the expected one is what this run just baked - see the rename loop below
    set<string> present_paths;

    fs::iterate_dir(_settings->BakeOutput, true, [&](string_view path, size_t size, uint64_t write_time) {
        ignore_unused(size, write_time);

        present_paths.emplace(path);

        // Skip cache dir and report files
        if (path.starts_with(BAKER_CACHE_DIR) && (path.size() == BAKER_CACHE_DIR.size() || path[BAKER_CACHE_DIR.size()] == '/')) {
            return;
        }
        if (path.find('/') == string_view::npos && strex(path).lower().str().ends_with(REPORT_FILE_SUFFIX)) {
            return;
        }

        if (expected.ResourceNames.count(ExcludeAllExt(path)) == 0) {
            bool remove_outdated_ok = fs::remove_file(MakeOutputPath(path));
            FO_VERIFY_AND_THROW(remove_outdated_ok, "Unable to delete outdated baked resource", path);
            _report->RecordOutdatedFile(path);
            logging::write("Delete outdated file {}", path);
            return;
        }

        // Collected rather than renamed in place: renaming an entry while its directory is being iterated is
        // not defined, and the iterator could hand the same file back under its new name
        auto it = expected.Paths.find(strex(path).lower().str());

        if (it != expected.Paths.end() && it->second != path) {
            stale_cased_paths.emplace_back(string(path), it->second);
        }
    });

    for (const auto& [stale_path, expected_path] : stale_cased_paths) {
        // Both spellings listed means the leftover sits beside output this run just wrote, so renaming would
        // clobber fresh content with stale; a case-insensitive file system lists one name and takes the rename
        if (present_paths.count(expected_path) != 0) {
            bool remove_stale_ok = fs::remove_file(MakeOutputPath(stale_path));
            FO_VERIFY_AND_THROW(remove_stale_ok, "Unable to delete the stale-cased duplicate baked resource", stale_path, expected_path);
            _report->RecordOutdatedFile(stale_path);
            logging::write("Delete stale-cased duplicate file {}, kept {}", stale_path, expected_path);
            continue;
        }

        bool rename_ok = fs::rename(MakeOutputPath(stale_path), MakeOutputPath(expected_path));
        FO_VERIFY_AND_THROW(rename_ok, "Unable to rename the stale-cased baked resource", stale_path, expected_path);
        logging::write("Rename stale-cased file {} to {}", stale_path, expected_path);
    }
}

// Baker-private caches are keyed by the output they describe, so they go stale exactly when that output does
void MasterBaker::SweepOutdatedBakerCache(const ExpectedOutputs& expected)
{
    FO_STACK_TRACE_ENTRY();

    string effekseer_cache_dir = MakeOutputPath(strex(BAKER_CACHE_DIR).combine_path("Effekseer").str());

    if (!fs::is_dir(effekseer_cache_dir)) {
        return;
    }

    constexpr string_view dependency_cache_suffix = ".deps";

    fs::iterate_dir(effekseer_cache_dir, true, [&](string_view path, size_t size, uint64_t write_time) {
        ignore_unused(size, write_time);

        if (path.ends_with(dependency_cache_suffix)) {
            string_view cached_output_path = path.substr(0, path.size() - dependency_cache_suffix.size());

            if (expected.ResourceNames.count(ExcludeAllExt(cached_output_path)) == 0) {
                fs::remove_file(strex(effekseer_cache_dir).combine_path(path));
                logging::write("Delete outdated baker cache {}", path);
            }
        }
    });
}

auto BaseBaker::ValidateProperties(const Properties& props, string_view context_str, nptr<const ScriptSystem> script_sys) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    struct ScriptFuncValidationRule
    {
        // Plain function pointers: every rule is a captureless check, so the table needs no wrapper
        bool (*VerifySignature)(hstring, ptr<const ScriptSystem>) {};
        bool (*VerifyAttribute)(hstring, ptr<const ScriptSystem>) {};
        string_view RequiredAttribute {};
    };

    static const unordered_map<string, ScriptFuncValidationRule> script_func_verify = {
        {"ItemInit",
            ScriptFuncValidationRule {
                .VerifySignature = [](hstring func_name, ptr<const ScriptSystem> script_sys_) { return script_sys_->CheckFunc<void, ptr<BakerStub::Item>, bool>(func_name); },
            }},
        {"ItemStatic",
            ScriptFuncValidationRule {
                .VerifySignature = [](hstring func_name, ptr<const ScriptSystem> script_sys_) { return script_sys_->CheckFunc<bool, ptr<BakerStub::Critter>, ptr<BakerStub::StaticItem>, nptr<BakerStub::Item>, any_t>(func_name); },
                .VerifyAttribute = [](hstring func_name, ptr<const ScriptSystem> script_sys_) { return script_sys_->CheckFunc<bool, ptr<BakerStub::Critter>, ptr<BakerStub::StaticItem>, nptr<BakerStub::Item>, any_t>(func_name, "ItemStatic"); },
                .RequiredAttribute = "ItemStatic",
            }},
        {"ItemTrigger",
            ScriptFuncValidationRule {
                .VerifySignature = [](hstring func_name, ptr<const ScriptSystem> script_sys_) { return script_sys_->CheckFunc<void, ptr<BakerStub::Critter>, ptr<BakerStub::StaticItem>, bool, mdir>(func_name); },
                .VerifyAttribute = [](hstring func_name, ptr<const ScriptSystem> script_sys_) { return script_sys_->CheckFunc<void, ptr<BakerStub::Critter>, ptr<BakerStub::StaticItem>, bool, mdir>(func_name, "ItemTrigger"); },
                .RequiredAttribute = "ItemTrigger",
            }},
        {"CritterInit",
            ScriptFuncValidationRule {
                .VerifySignature = [](hstring func_name, ptr<const ScriptSystem> script_sys_) { return script_sys_->CheckFunc<void, ptr<BakerStub::Critter>, bool>(func_name); },
            }},
        {"MapInit",
            ScriptFuncValidationRule {
                .VerifySignature = [](hstring func_name, ptr<const ScriptSystem> script_sys_) { return script_sys_->CheckFunc<void, ptr<BakerStub::Map>, bool>(func_name); },
            }},
        {"LocationInit",
            ScriptFuncValidationRule {
                .VerifySignature = [](hstring func_name, ptr<const ScriptSystem> script_sys_) { return script_sys_->CheckFunc<void, ptr<BakerStub::Location>, bool>(func_name); },
            }},
    };

    FO_VERIFY_AND_THROW(_context->BakedFiles, "Baker context has no baked file registry");

    size_t errors = 0;

    auto registrar = props.GetRegistrar();

    for (size_t i = 1; i < registrar->GetPropertiesCount(); i++) {
        auto prop = registrar->GetPropertyByIndexUnsafe(i);

        if (prop->IsBaseTypeResource()) {
            if (prop->IsPlainData()) {
                auto res_name = props.GetValue<hstring>(prop);

                if (res_name && !_context->BakedFiles->IsFileExists(res_name)) {
                    logging::write("Resource {} not found for property {} in {}", res_name, prop->GetName(), context_str);
                    errors++;
                }
            }
            else if (prop->IsArray()) {
                if (props.GetRawDataSize(prop) == 0) {
                    continue;
                }

                auto res_names = props.GetValue<vector<hstring>>(prop);

                for (auto res_name : res_names) {
                    if (res_name && !_context->BakedFiles->IsFileExists(res_name)) {
                        logging::write("Resource {} not found for property {} in {}", res_name, prop->GetName(), context_str);
                        errors++;
                    }
                }
            }
            else {
                logging::write("Resource {} can be as standalone or in array in {}", prop->GetName(), context_str);
                errors++;
            }
        }

        if (script_sys && !prop->GetBaseScriptFuncType().empty()) {
            if (prop->IsPlainData()) {
                auto func_name = props.GetValue<hstring>(prop);

                auto rule_it = script_func_verify.find(prop->GetBaseScriptFuncType());

                if (rule_it == script_func_verify.end()) {
                    logging::write("Invalid script func {} of type {} for property {} in {}", func_name, prop->GetBaseScriptFuncType(), prop->GetName(), context_str);
                    errors++;
                }
                else if (func_name && !rule_it->second.VerifySignature(func_name, script_sys)) {
                    logging::write("Script function signature does not match property binding: func {} of type {} for property {} in {}", func_name, prop->GetBaseScriptFuncType(), prop->GetName(), context_str);
                    errors++;
                }
                else if (func_name && !rule_it->second.RequiredAttribute.empty() && !rule_it->second.VerifyAttribute(func_name, script_sys)) {
                    logging::write("Function {} assigned to property {} in {} must be marked [[{}]]", func_name, prop->GetName(), context_str, rule_it->second.RequiredAttribute);
                    errors++;
                }
            }
            else {
                logging::write("Script {} must be as standalone (not in array or dict) in {}", prop->GetName(), context_str);
                errors++;
            }
        }
    }

    return errors;
}

BakerDataSource::BakerDataSource(ptr<BakingSettings> settings) :
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();

    _outputResources.AddCustomSource(safe_alloc::make_unique<DataSourceRef>(this));

    ignore_unused(Reindex());
}

auto BakerDataSource::Reindex() -> bool
{
    FO_STACK_TRACE_ENTRY();

    // Prepare input resources
    auto res_packs = _settings->GetResourcePacks();
    vector<ResourcesInputEntry> input_resources;
    unordered_map<string, pair<size_t, uint64_t>> input_file_index;
    input_resources.reserve(res_packs.size());

    for (const auto& res_pack : res_packs) {
        auto& res_entry = input_resources.emplace_back();
        res_entry.Name = res_pack.Name;
        auto bake_checker = [this, res_pack_name = res_pack.Name](string_view path, uint64_t write_time) -> bool { return CheckData(res_pack_name, path, write_time); };
        auto write_data = [this, res_pack_name = res_pack.Name](string_view path, span<const uint8_t> data) {
            WriteData(res_pack_name, path, data);
            return BakingWriteResult::Changed;
        };
        res_entry.Bakers = BaseBaker::SetupBakers(res_pack.Bakers, res_pack.Name, *_settings, bake_checker, write_data, &_outputResources);

        // Live sources: the on-demand baker serves editors and viewers whose content is edited while the tool runs,
        // so a cached snapshot would go stale between the initial indexing and a later open
        for (const auto& dir : res_pack.InputDirs) {
            res_entry.InputDir.AddDirSource(dir, true, true);
        }
        for (const auto& path : res_pack.InputFiles) {
            string dir = strex(path).extract_dir().str();
            string pack = strex(path).extract_file_name().erase_file_extension().str();
            res_entry.InputDir.AddCustomSource(DataSource::MountPack(dir, pack, false));
        }

        res_entry.InputFiles = res_entry.InputDir.FilterFiles(res_pack.IncludePatterns, res_pack.ExcludePatterns);

        for (const auto& input_file : res_entry.InputFiles) {
            string index_key = strex("{}\n{}", res_entry.Name, input_file.GetPath()).str();
            input_file_index.insert_or_assign(index_key, pair {input_file.GetSize(), input_file.GetWriteTime()});
        }
    }

    // Input resources must be published before the discovery pass runs the bakers: a baker may resolve another
    // baker's output through _outputResources, which re-enters this data source and needs the inputs to bake it
    _inputResources = std::move(input_resources);

    // Evaluate output files
    unordered_map<string, uint64_t> output_files;

    unordered_map<string, uint64_t> previous_output_files;

    {
        scoped_lock locker {_outputFilesLocker};

        previous_output_files = _outputFiles;
    }

    auto check_file = [&](string_view path, uint64_t write_time) {
        output_files.insert_or_assign(string(path), write_time);

        // Published live so a later baker in this same pass can resolve an earlier one's output on-demand, and
        // additively so no entry is transiently missing for a concurrent reader
        {
            scoped_lock locker {_outputFilesLocker};

            _outputFiles.insert_or_assign(string(path), write_time);
        }

        return false;
    };

    auto write_file = [](string_view path, span<const uint8_t> data) -> BakingWriteResult {
        ignore_unused(path, data);
        FO_UNREACHABLE_PLACE();
    };

    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_pack = res_packs[i];
        const auto& res_entry = _inputResources[i];
        auto bakers = BaseBaker::SetupBakers(res_pack.Bakers, res_pack.Name, *_settings, check_file, write_file, &_outputResources, nullptr, true);

        for (size_t j = 0; j != bakers.size(); ++j) {
            bakers[j]->BakeFiles(res_entry.InputFiles);
        }
    }

    bool input_files_changed = _inputFileIndex != input_file_index;
    _inputFileIndex = std::move(input_file_index);

    bool changed = input_files_changed;

    {
        scoped_lock locker {_outputFilesLocker};

        changed |= previous_output_files != output_files;
        _outputFiles = std::move(output_files);
    }

    return changed;
}

auto BakerDataSource::MakeOutputPath(string_view res_pack_name, string_view path) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(_settings->BakeOutput).combine_path(res_pack_name).combine_path(path);
}

auto BakerDataSource::CheckData(string_view res_pack_name, string_view path, uint64_t write_time) -> bool
{
    FO_STACK_TRACE_ENTRY();

    string output_path = MakeOutputPath(res_pack_name, path);

    if (write_time > fs::last_write_time(output_path)) {
        scoped_lock locker {_outputFilesLocker};

        _outputFiles.at(string(path)) = write_time;
        return true;
    }

    return false;
}

void BakerDataSource::WriteData(string_view res_pack_name, string_view path, span<const uint8_t> data)
{
    FO_STACK_TRACE_ENTRY();

    string output_path = MakeOutputPath(res_pack_name, path);
    bool write_file_ok = fs::write_file(output_path, data);
    FO_VERIFY_AND_THROW(write_file_ok, "Unable to write the baked output file", output_path);
}

auto BakerDataSource::ResolveFilePath(string_view path, uint64_t& write_time) const -> optional<string>
{
    FO_STACK_TRACE_ENTRY();

    uint64_t input_write_time = 0;

    {
        scoped_lock locker {_outputFilesLocker};

        auto it = _outputFiles.find(path);

        if (it == _outputFiles.end()) {
            return std::nullopt;
        }

        input_write_time = it->second;
    }

    auto accept_output_path = [&](string_view output_path) -> string {
        write_time = input_write_time;
        FO_VERIFY_AND_THROW(write_time != 0, "Baked output file write time is not available");

        return string {output_path};
    };

    // Try find already baked
    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_entry = _inputResources[_inputResources.size() - 1 - i];
        string output_path = MakeOutputPath(res_entry.Name, path);

        if (fs::exists(output_path)) {
            if (input_write_time > fs::last_write_time(output_path)) {
                bool delete_output_file_ok = fs::remove_file(output_path);
                FO_VERIFY_AND_THROW(delete_output_file_ok, "Unable to delete the stale baked output file", output_path);
                break;
            }

            return accept_output_path(output_path);
        }
    }

    // Runtime baking
    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_entry = _inputResources[_inputResources.size() - 1 - i];
        string output_path = MakeOutputPath(res_entry.Name, path);

        for (const auto& baker : res_entry.Bakers) {
            baker->BakeFiles(res_entry.InputFiles, path);
        }

        if (fs::exists(output_path)) {
            {
                scoped_lock locker {_outputFilesLocker};

                input_write_time = _outputFiles.at(string(path));
                uint64_t output_write_time = fs::last_write_time(output_path);
                FO_VERIFY_AND_THROW(input_write_time <= output_write_time, "Baked output file is older than the newest source input", path, output_path, input_write_time, output_write_time);
            }

            return accept_output_path(output_path);
        }
    }

    throw ResourceBakingException("File not baked", path);
}

auto BakerDataSource::FindFile(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto output_path = ResolveFilePath(path, write_time);

    if (!output_path) {
        return false;
    }

    auto output_size = fs::file_size(*output_path);
    FO_VERIFY_AND_THROW(output_size, "Unable to query the size of the baked output file", *output_path);

    size = numeric_cast<size_t>(*output_size);
    return true;
}

auto BakerDataSource::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock locker {_outputFilesLocker};

    return _outputFiles.contains(path);
}

auto BakerDataSource::GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return FindFile(path, size, write_time);
}

auto BakerDataSource::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    auto output_path = ResolveFilePath(path, write_time);

    if (!output_path) {
        return nullptr;
    }

    auto output_data = fs::read_file(*output_path);
    FO_VERIFY_AND_THROW(output_data, "Unable to read the baked output file", *output_path);

    size = output_data->size();
    auto buf = safe_alloc::make_unique_arr<uint8_t>(size);

    if (size != 0u) {
        memory::copy(buf, output_data->data(), size);
    }

    auto released_buf = make_ptr<const uint8_t*>(buf.release());
    return make_unique_del_ptr(released_buf, [](ptr<const uint8_t> p) FO_DEFERRED {
        unique_arr_ptr<const uint8_t> owned_buf {p.get()};
        ignore_unused(owned_buf);
    });
}

auto BakerDataSource::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    string fixed_dir = strex(dir).normalize_path_slashes();

    if (!dir.empty() && dir.back() == '/') {
        fixed_dir.resize(fixed_dir.size() - 1);
    }

    scoped_lock locker {_outputFilesLocker};

    vector<string> result;
    result.reserve(_outputFiles.size());

    for (const auto& fpath : _outputFiles | std::views::keys) {
        if (fpath.size() <= fixed_dir.size()) {
            continue;
        }
        if (!fixed_dir.empty() && (!fpath.starts_with(fixed_dir) || fpath[fixed_dir.size()] != '/')) {
            continue;
        }
        if (!recursive && fpath.find('/', fixed_dir.size() + 1) != string_view::npos) {
            continue;
        }
        if (!ext.empty() && strex(fpath).get_file_extension() != ext) {
            continue;
        }

        result.emplace_back(fpath);
    }

    return result;
}

BakerServerEngine::BakerServerEngine(const FileSystem& resources) :
    EngineMetadata([&] { RegisterServerStubMetadata(this, &resources); })
{
    FO_STACK_TRACE_ENTRY();

    MapEngineType<BakerStub::Item>(EngineMetadata::GetBaseType("Item"));
    MapEngineType<BakerStub::StaticItem>(EngineMetadata::GetBaseType("StaticItem"));
    MapEngineType<BakerStub::Critter>(EngineMetadata::GetBaseType("Critter"));
    MapEngineType<BakerStub::Map>(EngineMetadata::GetBaseType("Map"));
    MapEngineType<BakerStub::Location>(EngineMetadata::GetBaseType("Location"));
}

BakerClientEngine::BakerClientEngine(const FileSystem& resources) :
    EngineMetadata([&] { RegisterClientStubMetadata(this, &resources); })
{
    FO_STACK_TRACE_ENTRY();
}

BakerMapperEngine::BakerMapperEngine(const FileSystem& resources) :
    EngineMetadata([&] { RegisterMapperStubMetadata(this, &resources); })
{
    FO_STACK_TRACE_ENTRY();
}

FO_END_NAMESPACE
