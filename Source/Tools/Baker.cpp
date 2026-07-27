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

extern void SetupBakersHook(span<const string>, vector<unique_ptr<BaseBaker>>&, shared_ptr<BakingContext>);

BaseBaker::BaseBaker(shared_ptr<BakingContext> ctx, string_view baker_name)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx, "Missing required context");
    FO_VERIFY_AND_THROW(ctx->WriteData, "Baker context has no output writer");
    FO_VERIFY_AND_THROW(!baker_name.empty(), "Baker name is empty");

    _context = SafeAlloc::MakeShared<BakingContext>(*ctx);
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

        const AsyncWriteDataCallback write_data = ctx->WriteData;
        _context->WriteData = [report, pack_name, stored_baker_name, write_data](string_view path, const_span<byte> data) mutable {
            const BakingWriteResult result = write_data(path, data);
            report->RecordOutputSubmission(pack_name, stored_baker_name, path, data.size(), result);
            return result;
        };
    }
}

auto BaseBaker::SetupBakers(span<const string> request_bakers, const string& pack_name, const BakingSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, ptr<const FileSystem> baked_files, shared_ptr<BakingReport> report, bool output_discovery, nptr<const FileSystem> pack_baked_files) -> vector<unique_ptr<BaseBaker>>
{
    FO_STACK_TRACE_ENTRY();

    vector<unique_ptr<BaseBaker>> bakers;

    auto ctx = SafeAlloc::MakeShared<BakingContext>(BakingContext {.Settings = make_ptr(&settings), .PackName = pack_name, .BakeChecker = bake_checker, .WriteData = write_data, .BakedFiles = baked_files, .PackBakedFiles = pack_baked_files, .Report = std::move(report), .OutputDiscovery = output_discovery});

    if (vec_exists(request_bakers, MetadataBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<MetadataBaker>(ctx));
    }
    if (vec_exists(request_bakers, ConfigBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<ConfigBaker>(ctx));
    }
    if (vec_exists(request_bakers, RawCopyBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<RawCopyBaker>(ctx));
    }
    if (vec_exists(request_bakers, ImageBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<ImageBaker>(ctx));
    }
    if (vec_exists(request_bakers, EffectBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<EffectBaker>(ctx));
    }
    if (vec_exists(request_bakers, ParticleBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<ParticleBaker>(ctx));
    }
    if (vec_exists(request_bakers, ProtoBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<ProtoBaker>(ctx));
    }
    if (vec_exists(request_bakers, MapBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<MapBaker>(ctx));
    }
    if (vec_exists(request_bakers, TextBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<TextBaker>(ctx));
    }
    if (vec_exists(request_bakers, ProtoTextBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<ProtoTextBaker>(ctx));
    }
#if FO_ENABLE_3D
    if (vec_exists(request_bakers, ModelMeshBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<ModelMeshBaker>(ctx));
    }
    if (vec_exists(request_bakers, ModelInfoBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<ModelInfoBaker>(ctx));
    }
#endif
#if FO_ANGELSCRIPT_SCRIPTING
    if (vec_exists(request_bakers, AngelScriptBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<AngelScriptBaker>(ctx));
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
    u8string report_path;

    try {
        _report = SafeAlloc::MakeShared<BakingReport>(_settings);
        report_path = GetBakingReportPath(_settings->BakeOutput.view());

        if (!report_path.empty()) {
            const auto remove_old_report_ok = fs_remove_file(report_path.view());
            FO_VERIFY_AND_THROW(remove_old_report_ok, "Unable to delete the previous baking report", report_path.view());
        }

        BakeAllInternal();
        success = true;
    }
    catch (const std::exception& ex) {
        WriteLog("Baking error: {}", ex.what());
        failure_message = ex.what();
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    if (_report) {
        _report->Complete(success, failure_message);

        if (!report_path.empty()) {
            try {
                const u8string report_dir = fs_path_to_u8string(std::filesystem::path {fs_make_path(report_path.view())}.parent_path());
                if (!report_dir.empty()) {
                    const auto create_report_dir_ok = fs_create_directories(report_dir.view());
                    FO_VERIFY_AND_THROW(create_report_dir_ok, "Unable to create the baking report directory", report_dir.view());
                }

                const u8string report_data = _report->Serialize();
                const auto write_report_ok = fs_write_file_text(report_path.view(), report_data.view());
                FO_VERIFY_AND_THROW(write_report_ok, "Unable to write the baking report", report_path.view());
                WriteLog("Baking report saved to {}", report_path.view());

                if (success && _report->IsFullRebuild()) {
                    const u8string full_report_path = GetFullBakingReportPath(_settings->BakeOutput.view());
                    const auto write_full_report_ok = fs_write_file_text(full_report_path.view(), report_data.view());
                    FO_VERIFY_AND_THROW(write_full_report_ok, "Unable to write the full baking report", full_report_path.view());
                    WriteLog("Full baking report saved to {}", full_report_path.view());
                }
            }
            catch (const std::exception& ex) {
                WriteLog("Baking report error: {}", ex.what());
                success = false;
            }
        }
    }

    return success;
}

void MasterBaker::BakeAllInternal()
{
    FO_STACK_TRACE_ENTRY();

    TimeMeter backing_time;

    WriteLog("Start baking");

    FO_VERIFY_AND_THROW(!_settings->BakeOutput.empty(), "Resource baker cannot write outputs because BakeOutput is empty", _settings->GetResourcePacks().size());
    const u8string bake_output = _settings->BakeOutput;
    const auto make_output_path = [&bake_output](string_view path) -> u8string { return fs_combine_path(bake_output.view(), path); };

    const u8string build_hash_path = make_output_path("Resources.build-hash");
    const auto prev_build_hash = fs_read_file_text(build_hash_path.view());
    const string_view build_hash_text = FO_BUILD_HASH;
    const u8string current_build_hash = build_hash_text;
    const auto build_hash_deleted = fs_remove_file(build_hash_path.view());
    FO_VERIFY_AND_THROW(build_hash_deleted, "Unable to delete the previous build hash file", build_hash_path.view());

    std::atomic_bool force_baking = false;
    string rebuild_reason = "incremental";

    if (_settings->ForceBaking) {
        WriteLog("Force rebuild all resources");
        force_baking = true;
        rebuild_reason = "requested";
    }
    else if (prev_build_hash.has_value() && prev_build_hash.value() != current_build_hash) {
        WriteLog("Force rebuild all resources due to build hash changed");
        force_baking = true;
        rebuild_reason = "build_hash_changed";
    }

    if (force_baking) {
        const auto delete_output_ok = fs_remove_dir_tree(bake_output.view());
        FO_VERIFY_AND_THROW(delete_output_ok, "Unable to delete baking output dir");
    }

    if (!prev_build_hash.has_value()) {
        force_baking = true;
        if (rebuild_reason == "incremental") {
            rebuild_reason = "missing_build_hash";
        }
    }

    _report->SetRebuildMode(force_baking, rebuild_reason);

    const auto make_output_ok = fs_create_directories(bake_output.view());
    FO_VERIFY_AND_THROW(make_output_ok, "Unable to recreate baking output dir");

    FileSystem baking_output;

    // Resource packs
    struct PackBakeContext
    {
        string PackName {};
        u8string OutputDir {};
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
        TimeMeter BakingTime {};
        bool Done {};
    };

    auto prepare_bake_pack = [ // clang-format off
            &settings = *_settings,
            &baking_output_ = baking_output,
            &force_baking,
            report = _report // clang-format on
    ](const ResourcePackInfo& res_pack, const u8string& output_dir) -> unique_ptr<PackBakeContext> {
        auto pack_bake_context = SafeAlloc::MakeUnique<PackBakeContext>();
        auto pack_bake_context_ptr = pack_bake_context.as_ptr();
        shared_ptr<BakingReport> pack_report = report;

        pack_bake_context->PackName = res_pack.Name;
        pack_bake_context->OutputDir = output_dir;
        pack_bake_context->Report = pack_report;
        pack_bake_context->PackBakedFiles.AddDirSource(output_dir, true, true, true);

        for (const auto& input_dir : res_pack.InputDirs) {
            pack_bake_context_ptr->InputFiles.AddDirSource(input_dir, true);
        }
        for (const auto& input_file : res_pack.InputFiles) {
            const std::filesystem::path native_input_file {fs_make_path(input_file.view())};
            const u8string dir = fs_path_to_u8string(native_input_file.parent_path());
            const u8string pack = fs_path_to_u8string(native_input_file.stem());
            pack_bake_context_ptr->InputFiles.AddCustomSource(DataSource::MountPack(dir.view(), pack.view(), false));
        }

        pack_bake_context->FilteredFiles = pack_bake_context->InputFiles.FilterFiles(res_pack.IncludePatterns, res_pack.ExcludePatterns);

        for (const FileHeader& file : pack_bake_context->FilteredFiles) {
            pack_report->RecordPackInput(res_pack.Name, file.GetPath(), file.GetSize());
            pack_bake_context->InputBytes += numeric_cast<uint64_t>(file.GetSize());
        }

        auto bake_checker = [context = pack_bake_context_ptr, &force_baking](string_view path, uint64_t write_time) mutable -> bool {
            // ModelInfoBaker fans BakeChecker calls across PPL tasks, so the path set has
            // to be guarded; without it concurrent emplace() races on the bucket array.
            {
                scoped_lock lock {context->BakedFilePathsLocker};

                context->BakedFilePaths.emplace(path);
            }

            if (!force_baking) {
                const u8string output_path = fs_combine_path(context->OutputDir.view(), path);
                const auto file_write_time = fs_last_write_time(output_path.view());
                return write_time > file_write_time;
            }
            else {
                return true;
            }
        };

        const auto write_data = [context = pack_bake_context_ptr](string_view path, const_span<byte> baked_data) mutable -> BakingWriteResult {
            const u8string res_path = fs_combine_path(context->OutputDir.view(), path);

            if (!fs_compare_file_bytes(res_path.view(), baked_data)) {
                const auto res_file_write_ok = fs_write_file_bytes(res_path.view(), baked_data);
                FO_VERIFY_AND_THROW(res_file_write_ok, "Unable to write baked resource file", res_path.view());
                ++context->BakedFiles;
                return BakingWriteResult::Changed;
            }
            else {
                const auto res_file_touch_ok = fs_touch_file(res_path.view());
                FO_VERIFY_AND_THROW(res_file_touch_ok, "Unable to update the timestamp of an unchanged baked resource file", res_path.view());
                return BakingWriteResult::Unchanged;
            }
        };

        pack_bake_context->Bakers = BaseBaker::SetupBakers(res_pack.Bakers, res_pack.Name, settings, bake_checker, write_data, &baking_output_, pack_report, false, &pack_bake_context->PackBakedFiles);
        for (const auto& baker : pack_bake_context->Bakers) {
            pack_report->RecordBakerRegistration(res_pack.Name, baker->GetName(), baker->GetOrder());
        }

        pack_bake_context->BakingTime.Pause();
        return pack_bake_context;
    };

    auto bake_pack = [](ptr<PackBakeContext> bake_context, int32_t bake_order) {
        for (size_t i = 0; i != bake_context->Bakers.size(); ++i) {
            auto baker = bake_context->Bakers[i].as_ptr();

            if (baker->GetOrder() == bake_order) {
                if (!bake_context->FirstBake) {
                    WriteLog("Bake {}", bake_context->PackName);
                    bake_context->FirstBake = true;

                    bake_context->BakingTime.Resume();
                    const auto make_res_output_ok = fs_create_directories(bake_context->OutputDir.view());
                    FO_VERIFY_AND_THROW(make_res_output_ok, "Unable to create the resource pack output directory", bake_context->OutputDir.view());
                    bake_context->BakingTime.Pause();
                }

                bake_context->BakingTime.Resume();
                TimeMeter baker_time;
                try {
                    baker->BakeFiles(bake_context->FilteredFiles);
                    bake_context->BakingTime.Pause();
                    bake_context->Report->RecordBakerInvocation(bake_context->PackName, baker->GetName(), baker->GetOrder(), bake_context->FilteredFiles.GetFilesCount(), bake_context->InputBytes, baker_time.GetDuration().milliseconds(), true, {});
                }
                catch (const std::exception& ex) {
                    bake_context->BakingTime.Pause();
                    bake_context->Report->RecordBakerInvocation(bake_context->PackName, baker->GetName(), baker->GetOrder(), bake_context->FilteredFiles.GetFilesCount(), bake_context->InputBytes, baker_time.GetDuration().milliseconds(), false, ex.what());
                    throw;
                }
                catch (...) {
                    bake_context->BakingTime.Pause();
                    bake_context->Report->RecordBakerInvocation(bake_context->PackName, baker->GetName(), baker->GetOrder(), bake_context->FilteredFiles.GetFilesCount(), bake_context->InputBytes, baker_time.GetDuration().milliseconds(), false, "Unknown exception");
                    throw;
                }
            }
        }

        // Check if it's last iteration for this pack
        if (bake_context->FirstBake && !bake_context->Done) {
            auto it = std::ranges::max_element(bake_context->Bakers, {}, [](auto&& baker) { return baker->GetOrder(); });
            FO_VERIFY_AND_THROW(it != bake_context->Bakers.end(), "Lookup failed in bake context bakers");
            int32_t max_order = (*it)->GetOrder();

            if (bake_order == max_order) {
                const string_view plural_suffix = bake_context->BakedFiles != 1 ? "s" : string_view {};
                WriteLog("Baking of {} complete in {}, baked {} file{}", bake_context->PackName, //
                    bake_context->BakingTime.GetDuration(), bake_context->BakedFiles, plural_suffix);
                bake_context->Report->RecordPackDuration(bake_context->PackName, bake_context->BakingTime.GetDuration().milliseconds());
                bake_context->Done = true;
            }
        }
    };

    size_t errors = 0;
    const auto& res_packs = _settings->GetResourcePacks();
    async_launch_mode async_mode = _settings->SingleThreadBaking ? launch_deferred_only : launch_async_and_deferred;

    // Prepare bake contexts
    vector<unique_ptr<PackBakeContext>> pack_bake_contexts;

    {
        vector<std::future<unique_ptr<PackBakeContext>>> prepare_res_bakings;
        string first_prepare_error;

        for (const auto& res_pack : res_packs) {
            auto res_pack_ptr = make_ptr(&res_pack);
            const u8string output_path = make_output_path(res_pack.Name);
            prepare_res_bakings.emplace_back(run_async(async_mode, strex("PreparePack-{}", res_pack_ptr->Name), [&, res_pack_ptr, output_path]() FO_DEFERRED { return prepare_bake_pack(*res_pack_ptr, output_path); }));
        }

        for (auto& prepare_res_baking : prepare_res_bakings) {
            try {
                auto pack_bake_context = prepare_res_baking.get();
                pack_bake_contexts.emplace_back(std::move(pack_bake_context));
            }
            catch (const std::exception& ex) {
                WriteLog("Resource pack prepare for baking error: {}", ex.what());
                if (first_prepare_error.empty()) {
                    first_prepare_error = ex.what();
                }
                errors++;
            }
            catch (...) {
                WriteLog("Resource pack prepare for baking error: unknown exception");
                if (first_prepare_error.empty()) {
                    first_prepare_error = "Unknown exception";
                }
                errors++;
            }
        }

        if (errors != 0) {
            throw ResourceBakingException("Resource pack preparation failed", first_prepare_error, errors);
        }
    }

    // Run bake contexts
    int32_t bake_order = -10;

    while (true) {
        vector<std::future<void>> res_bakings;
        string first_bake_error;

        for (auto& bake_context_holder : pack_bake_contexts) {
            if (!bake_context_holder->Done) {
                auto bake_context = bake_context_holder.as_ptr();
                res_bakings.emplace_back(run_async(async_mode, strex("BakePack-{}-order{}", bake_context->PackName, bake_order), [&bake_pack, bake_context, bake_order]() FO_DEFERRED { bake_pack(bake_context, bake_order); }));
            }
        }

        for (auto& res_baking : res_bakings) {
            try {
                res_baking.get();
            }
            catch (const std::exception& ex) {
                WriteLog("Resource pack baking error: {}", ex.what());
                if (first_bake_error.empty()) {
                    first_bake_error = ex.what();
                }
                errors++;
            }
            catch (...) {
                WriteLog("Resource pack baking error: unknown exception");
                if (first_bake_error.empty()) {
                    first_bake_error = "Unknown exception";
                }
                errors++;
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
            if (bake_context->BakedFiles != 0) {
                force_baking = true;
            }
        }

        if (std::ranges::all_of(pack_bake_contexts, [](auto&& context) { return context->Done; })) {
            break;
        }

        bake_order++;
    }

    // Delete outdated files
    auto exclude_all_ext = [](string_view path) -> string {
        size_t pos = path.rfind('/');
        pos = path.find('.', pos != string::npos ? pos : 0);
        return strex(pos != string::npos ? path.substr(0, pos) : path).lower();
    };

    unordered_set<string> actual_resource_names;

    for (auto& bake_context : pack_bake_contexts) {
        scoped_lock lock {bake_context->BakedFilePathsLocker};

        for (const auto& res_name : bake_context->BakedFilePaths) {
            actual_resource_names.emplace(exclude_all_ext(strex(bake_context->PackName).combine_path(res_name)));
        }
    }

    fs_iterate_dir(bake_output.view(), true, [&](u8string_view path, size_t size, uint64_t write_time) {
        ignore_unused(size, write_time);
        const string path_text {utf8_as_char_view(path)};

        if (strex(path_text).lower() == "baking.report.json" || strex(path_text).lower() == "baking.full.report.json") {
            return;
        }

        if (actual_resource_names.count(exclude_all_ext(path_text)) == 0) {
            const u8string outdated_path = fs_path_to_u8string(std::filesystem::path {fs_make_path(bake_output.view())} / std::filesystem::path {fs_make_path(path)});
            const auto remove_outdated_ok = fs_remove_file(outdated_path.view());
            FO_VERIFY_AND_THROW(remove_outdated_ok, "Unable to delete outdated baked resource", path_text);
            _report->RecordOutdatedFile(path_text);
            WriteLog("Delete outdated file {}", path);
        }
    });

    const u8string baker_cache_dir = fs_combine_path(_settings->BakeOutput, BAKER_CACHE_DIR);
    const u8string effekseer_cache_dir = fs_combine_path(baker_cache_dir, "Effekseer");

    if (fs_is_dir(effekseer_cache_dir)) {
        constexpr u8string_view dependency_cache_suffix = u8".deps";

        fs_iterate_dir(effekseer_cache_dir, true, [&](u8string_view path, size_t size, uint64_t write_time) {
            ignore_unused(size, write_time);

            if (u8strvex(path).ends_with(dependency_cache_suffix)) {
                const auto cached_output_path_utf8 = u8string_view::FromChecked(path.native_view().substr(0, path.size() - dependency_cache_suffix.size()));
                const string cached_output_path = utf8_to_string(cached_output_path_utf8);

                if (actual_resource_names.count(exclude_all_ext(cached_output_path)) == 0) {
                    (void)fs_remove_file(fs_combine_path(effekseer_cache_dir, path));
                    WriteLog("Delete outdated baker cache {}", path);
                }
            }
        });
    }

    // Finalize
    WriteLog("Time {}", backing_time.GetDuration());
    WriteLog("Baking complete!");

    const auto build_hash_write_ok = fs_write_file_text(build_hash_path.view(), current_build_hash.view());
    FO_VERIFY_AND_THROW(build_hash_write_ok, "Unable to write the build hash file", build_hash_path.view());
}

auto BaseBaker::ValidateProperties(const Properties& props, u8string_view context, nptr<const ScriptSystem> script_sys) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    struct ScriptFuncValidationRule
    {
        function<bool(hstring, ptr<const ScriptSystem>)> VerifySignature {};
        function<bool(hstring, ptr<const ScriptSystem>)> VerifyAttribute {};
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

    auto registrator = props.GetRegistrator();

    for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
        auto prop = registrator->GetPropertyByIndexUnsafe(i);
        const string_view property_name = prop->GetName();

        if (prop->IsBaseTypeResource()) {
            if (prop->IsPlainData()) {
                auto res_name = props.GetValue<hstring>(prop);

                if (res_name && !_context->BakedFiles->IsFileExists(res_name)) {
                    WriteLog("Resource {} not found for property {} in {}", res_name, property_name, context);
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
                        WriteLog("Resource {} not found for property {} in {}", res_name, property_name, context);
                        errors++;
                    }
                }
            }
            else {
                WriteLog("Resource {} can be as standalone or in array in {}", property_name, context);
                errors++;
            }
        }

        if (script_sys && !prop->GetBaseScriptFuncType().empty()) {
            const string_view script_func_type = prop->GetBaseScriptFuncType();

            if (prop->IsPlainData()) {
                const auto func_name = props.GetValue<hstring>(prop);
                const auto rule_it = script_func_verify.find(prop->GetBaseScriptFuncType());

                if (rule_it == script_func_verify.end()) {
                    WriteLog("Invalid script func {} of type {} for property {} in {}", func_name, script_func_type, property_name, context);
                    errors++;
                }
                else if (func_name && !rule_it->second.VerifySignature(func_name, script_sys)) {
                    WriteLog("Script function signature does not match property binding: func {} of type {} for property {} in {}", func_name, script_func_type, property_name, context);
                    errors++;
                }
                else if (func_name && !rule_it->second.RequiredAttribute.empty() && !rule_it->second.VerifyAttribute(func_name, script_sys)) {
                    WriteLog("Function {} assigned to property {} in {} must be marked [[{}]]", func_name, property_name, context, rule_it->second.RequiredAttribute);
                    errors++;
                }
            }
            else {
                WriteLog("Script {} must be as standalone (not in array or dict) in {}", property_name, context);
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

    _outputResources.AddCustomSource(SafeAlloc::MakeUnique<DataSourceRef>(this));

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
        const auto bake_checker = [this, res_pack_name = res_pack.Name](string_view path, uint64_t write_time) -> bool { return CheckData(res_pack_name, path, write_time); };
        const auto write_data = [this, res_pack_name = res_pack.Name](string_view path, const_span<byte> data) {
            WriteData(res_pack_name, path, data);
            return BakingWriteResult::Changed;
        };
        res_entry.Bakers = BaseBaker::SetupBakers(res_pack.Bakers, res_pack.Name, *_settings, bake_checker, write_data, &_outputResources);

        // Live sources: the on-demand baker serves editors and viewers, whose content is edited while the tool
        // runs, so input dirs are mounted non-cached - a cached snapshot would go stale between the initial
        // indexing and a later open (cached mounts stay for the client/server runtime and the one-shot batch bake).
        for (const auto& dir : res_pack.InputDirs) {
            res_entry.InputDir.AddDirSource(dir, true);
        }
        for (const auto& path : res_pack.InputFiles) {
            const std::filesystem::path native_path {fs_make_path(path.view())};
            const u8string dir = fs_path_to_u8string(native_path.parent_path());
            const u8string pack = fs_path_to_u8string(native_path.stem());
            res_entry.InputDir.AddCustomSource(DataSource::MountPack(dir.view(), pack.view(), false));
        }

        res_entry.InputFiles = res_entry.InputDir.FilterFiles(res_pack.IncludePatterns, res_pack.ExcludePatterns);

        for (const auto& input_file : res_entry.InputFiles) {
            string index_key = strex("{}\n{}", res_entry.Name, input_file.GetPath()).str();
            input_file_index.insert_or_assign(index_key, pair {input_file.GetSize(), input_file.GetWriteTime()});
        }
    }

    // Input resources must be published before the discovery pass runs the bakers: a baker can read another
    // baker's output while it discovers its own (for example ModelInfoBaker builds a BakerClientEngine that
    // reads the baked metadata), which re-enters this data source through _outputResources and resolves the
    // file via ResolveFilePath - which needs the input resources to locate or on-demand bake it.
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

        // Publish live so a later baker in this same discovery pass can resolve an earlier baker's output
        // on-demand (bakers run in dependency order, e.g. metadata before model info). Additive so no entry is
        // transiently missing for a concurrent reader; the clean set replaces it once the pass completes.
        {
            scoped_lock locker {_outputFilesLocker};

            _outputFiles.insert_or_assign(string(path), write_time);
        }

        return false;
    };

    const auto write_file = [](string_view path, const_span<byte> data) -> BakingWriteResult {
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

auto BakerDataSource::MakeOutputPath(string_view res_pack_name, string_view path) const -> u8string
{
    FO_STACK_TRACE_ENTRY();

    const string relative_path = strex(res_pack_name).combine_path(path).str();
    const u8string bake_output = _settings->BakeOutput;
    return fs_combine_path(bake_output.view(), relative_path);
}

auto BakerDataSource::CheckData(string_view res_pack_name, string_view path, uint64_t write_time) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const u8string output_path = MakeOutputPath(res_pack_name, path);

    if (write_time > fs_last_write_time(output_path.view())) {
        scoped_lock locker {_outputFilesLocker};

        _outputFiles.at(string(path)) = write_time;
        return true;
    }

    return false;
}

void BakerDataSource::WriteData(string_view res_pack_name, string_view path, const_span<byte> data)
{
    FO_STACK_TRACE_ENTRY();

    const u8string output_path = MakeOutputPath(res_pack_name, path);
    const auto write_file_ok = fs_write_file_bytes(output_path.view(), data);
    FO_VERIFY_AND_THROW(write_file_ok, "Unable to write the baked output file", output_path.view());
}

auto BakerDataSource::ResolveFilePath(string_view path, uint64_t& write_time) const -> optional<u8string>
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

    const auto accept_output_path = [&](const u8string& output_path) -> u8string {
        write_time = input_write_time;
        FO_VERIFY_AND_THROW(write_time != 0, "Baked output file write time is not available");

        return output_path;
    };

    // Try find already baked
    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_entry = _inputResources[_inputResources.size() - 1 - i];
        const u8string output_path = MakeOutputPath(res_entry.Name, path);

        if (fs_exists(output_path.view())) {
            if (input_write_time > fs_last_write_time(output_path.view())) {
                const auto delete_output_file_ok = fs_remove_file(output_path.view());
                FO_VERIFY_AND_THROW(delete_output_file_ok, "Unable to delete the stale baked output file", output_path.view());
                break;
            }

            return accept_output_path(output_path);
        }
    }

    // Runtime baking
    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_entry = _inputResources[_inputResources.size() - 1 - i];
        const u8string output_path = MakeOutputPath(res_entry.Name, path);

        for (const auto& baker : res_entry.Bakers) {
            baker->BakeFiles(res_entry.InputFiles, path);
        }

        if (fs_exists(output_path.view())) {
            {
                scoped_lock locker {_outputFilesLocker};

                input_write_time = _outputFiles.at(string(path));
                uint64_t output_write_time = fs_last_write_time(output_path);
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

    const auto output_size = fs_file_size(output_path->view());
    FO_VERIFY_AND_THROW(output_size, "Unable to query the size of the baked output file", output_path->view());

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

auto BakerDataSource::OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const byte>
{
    FO_STACK_TRACE_ENTRY();

    auto output_path = ResolveFilePath(path, write_time);

    if (!output_path) {
        return nullptr;
    }

    const auto output_data = fs_read_file_bytes(output_path->view());
    FO_VERIFY_AND_THROW(output_data, "Unable to read the baked output file", output_path->view());

    size = output_data->size();
    auto buf = SafeAlloc::MakeUniqueArr<byte>(size);

    if (size != 0u) {
        MemCopy(buf, output_data->data(), size);
    }

    auto released_buf = make_ptr<const byte*>(buf.release());
    return make_unique_del_ptr(released_buf, [](ptr<const byte> p) FO_DEFERRED {
        unique_arr_ptr<const byte> owned_buf {p.get()};
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
