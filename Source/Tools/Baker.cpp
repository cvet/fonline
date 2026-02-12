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
#include "ModelBaker.h"
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

BaseBaker::BaseBaker(shared_ptr<BakingContext> ctx) :
    _context {std::move(ctx)}
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_context);
    FO_RUNTIME_ASSERT(_context->WriteData);
}

auto BaseBaker::SetupBakers(span<const string> request_bakers, const string& pack_name, const BakingSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, const FileSystem* baked_files) -> vector<unique_ptr<BaseBaker>>
{
    FO_STACK_TRACE_ENTRY();

    vector<unique_ptr<BaseBaker>> bakers;

    auto ctx = SafeAlloc::MakeShared<BakingContext>(BakingContext {.Settings = &settings, .PackName = pack_name, .BakeChecker = bake_checker, .WriteData = write_data, .BakedFiles = baked_files});

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
    if (vec_exists(request_bakers, ModelBaker::NAME)) {
        bakers.emplace_back(SafeAlloc::MakeUnique<ModelBaker>(ctx));
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

MasterBaker::MasterBaker(BakingSettings& settings) noexcept :
    _settings {&settings}
{
    FO_STACK_TRACE_ENTRY();
}

auto MasterBaker::BakeAll() noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    try {
        BakeAllInternal();
        return true;
    }
    catch (const std::exception& ex) {
        WriteLog("Baking error: {}", ex.what());
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }

    return false;
}

void MasterBaker::BakeAllInternal()
{
    FO_STACK_TRACE_ENTRY();

    const TimeMeter backing_time;

    WriteLog("Start baking");

    FO_RUNTIME_ASSERT(!_settings->BakeOutput.empty());
    const auto make_output_path = [this](string_view path) -> string { return strex(_settings->BakeOutput).combine_path(path); };

    const auto build_hash_path = make_output_path("Resources.build-hash");
    const auto build_hash_deleted = DiskFileSystem::DeleteFile(build_hash_path);
    FO_RUNTIME_ASSERT(build_hash_deleted);

    std::atomic_bool force_baking = false;

    if (_settings->ForceBaking) {
        WriteLog("Force rebuild all resources");
        const auto delete_output_ok = DiskFileSystem::DeleteDir(_settings->BakeOutput);
        FO_RUNTIME_ASSERT_STR(delete_output_ok, "Unable to delete baking output dir");
        force_baking = true;
    }

    const auto make_output_ok = DiskFileSystem::MakeDirTree(_settings->BakeOutput);
    FO_RUNTIME_ASSERT_STR(make_output_ok, "Unable to recreate baking output dir");

    FileSystem baking_output;

    // Resource packs
    struct PackBakeContext
    {
        string PackName {};
        string OutputDir {};
        FileSystem InputFiles {};
        FileCollection FilteredFiles {};
        vector<unique_ptr<BaseBaker>> Bakers {};
        std::atomic_int BakedFiles {};
        unordered_set<string> BakedFilePaths {};
        bool FirstBake {};
        bool OutputAdded {};
        TimeMeter BakingTime {};
        bool Done {};
    };

    const auto prepare_bake_pack = [ // clang-format off
            &settings = std::as_const(_settings),
            &baking_output_ = std::as_const(baking_output),
            &force_baking // clang-format on
    ](const ResourcePackInfo& res_pack, const string& output_dir) -> unique_ptr<PackBakeContext> {
        auto pack_bake_context = SafeAlloc::MakeUnique<PackBakeContext>();

        pack_bake_context->PackName = res_pack.Name;
        pack_bake_context->OutputDir = output_dir;

        for (const auto& input_dir : res_pack.InputDirs) {
            pack_bake_context->InputFiles.AddDirSource(input_dir, res_pack.RecursiveInput);
        }
        for (const auto& input_file : res_pack.InputFiles) {
            const auto dir = strex(input_file).extract_dir().str();
            const auto pack = strex(input_file).extract_file_name().erase_file_extension().str();
            pack_bake_context->InputFiles.AddCustomSource(DataSource::MountPack(dir, pack, false));
        }

        pack_bake_context->FilteredFiles = pack_bake_context->InputFiles.GetAllFiles();

        const auto bake_checker = [context = pack_bake_context.get(), &force_baking](string_view path, uint64 write_time) -> bool {
            context->BakedFilePaths.emplace(path);

            if (!force_baking) {
                const auto file_write_time = DiskFileSystem::GetWriteTime(strex(context->OutputDir).combine_path(path));
                return write_time > file_write_time;
            }
            else {
                return true;
            }
        };

        const auto write_data = [context = pack_bake_context.get()](string_view path, span<const uint8> baked_data) {
            const auto res_path = strex(context->OutputDir).combine_path(path).str();

            if (!DiskFileSystem::CompareFileContent(res_path, baked_data)) {
                const auto res_file_write_ok = DiskFileSystem::WriteFile(res_path, baked_data);
                FO_RUNTIME_ASSERT_STR(res_file_write_ok, strex("Unable to write file '{}'", res_path));
                ++context->BakedFiles;
            }
            else {
                const auto res_file_touch_ok = DiskFileSystem::TouchFile(res_path);
                FO_RUNTIME_ASSERT(res_file_touch_ok);
            }
        };

        pack_bake_context->Bakers = BaseBaker::SetupBakers(res_pack.Bakers, res_pack.Name, *settings, bake_checker, write_data, &baking_output_);

        pack_bake_context->BakingTime.Pause();
        return pack_bake_context;
    };

    const auto bake_pack = [](PackBakeContext* bake_context, int32 bake_order) {
        for (auto& baker : bake_context->Bakers) {
            if (baker->GetOrder() == bake_order) {
                if (!bake_context->FirstBake) {
                    WriteLog("Bake {}", bake_context->PackName);
                    bake_context->FirstBake = true;

                    bake_context->BakingTime.Resume();
                    const auto make_res_output_ok = DiskFileSystem::MakeDirTree(bake_context->OutputDir);
                    FO_RUNTIME_ASSERT(make_res_output_ok);
                    bake_context->BakingTime.Pause();
                }

                bake_context->BakingTime.Resume();
                baker->BakeFiles(bake_context->FilteredFiles);
                bake_context->BakingTime.Pause();
            }
        }

        // Check if it's last iteration for this pack
        if (bake_context->FirstBake && !bake_context->Done) {
            const auto it = std::ranges::max_element(bake_context->Bakers, {}, [](auto&& baker) { return baker->GetOrder(); });
            FO_RUNTIME_ASSERT(it != bake_context->Bakers.end());
            const auto max_order = (*it)->GetOrder();

            if (bake_order == max_order) {
                WriteLog("Baking of {} complete in {}, baked {} file{}", bake_context->PackName, //
                    bake_context->BakingTime.GetDuration(), bake_context->BakedFiles, bake_context->BakedFiles != 1 ? "s" : "");
                bake_context->Done = true;
            }
        }
    };

    size_t errors = 0;
    const auto& res_packs = _settings->GetResourcePacks();
    const auto async_mode = _settings->SingleThreadBaking ? std::launch::deferred : std::launch::async | std::launch::deferred;

    // Prepare bake contexts
    vector<unique_ptr<PackBakeContext>> pack_bake_contexts;

    {
        vector<std::future<unique_ptr<PackBakeContext>>> prepare_res_bakings;

        for (const auto& res_pack : res_packs) {
            prepare_res_bakings.emplace_back(std::async(async_mode, [&]() FO_DEFERRED { return prepare_bake_pack(res_pack, make_output_path(res_pack.Name)); }));
        }

        for (auto& prepare_res_baking : prepare_res_bakings) {
            try {
                auto pack_bake_context = prepare_res_baking.get();
                pack_bake_contexts.emplace_back(std::move(pack_bake_context));
            }
            catch (const std::exception& ex) {
                WriteLog("Resource pack prepare for baking error: {}", ex.what());
                errors++;
            }
        }

        if (errors != 0) {
            throw ResourceBakingException("Baking resource packs failed");
        }
    }

    // Run bake contexts
    int32 bake_order = -10;

    while (true) {
        vector<std::future<void>> res_bakings;

        for (auto& bake_context : pack_bake_contexts) {
            if (!bake_context->Done) {
                res_bakings.emplace_back(std::async(async_mode, [&]() FO_DEFERRED { bake_pack(bake_context.get(), bake_order); }));
            }
        }

        for (auto& res_baking : res_bakings) {
            try {
                res_baking.get();
            }
            catch (const std::exception& ex) {
                WriteLog("Resource pack baking error: {}", ex.what());
                errors++;
            }
        }

        if (errors != 0) {
            throw ResourceBakingException("Baking resource packs failed");
        }

        for (auto& bake_context : pack_bake_contexts) {
            if (!bake_context->OutputAdded && bake_context->FirstBake) {
                baking_output.AddDirSource(bake_context->OutputDir, true);
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
    const auto exclude_all_ext = [](string_view path) -> string {
        size_t pos = path.rfind('/');
        pos = path.find('.', pos != string::npos ? pos : 0);
        return strex(pos != string::npos ? path.substr(0, pos) : path).lower();
    };

    unordered_set<string> actual_resource_names;

    for (auto& bake_context : pack_bake_contexts) {
        for (const auto& res_name : bake_context->BakedFilePaths) {
            actual_resource_names.emplace(exclude_all_ext(strex(bake_context->PackName).combine_path(res_name)));
        }
    }

    DiskFileSystem::IterateDir(_settings->BakeOutput, true, [&](string_view path, size_t size, uint64 write_time) {
        ignore_unused(size, write_time);

        if (actual_resource_names.count(exclude_all_ext(path)) == 0) {
            DiskFileSystem::DeleteFile(strex(_settings->BakeOutput).combine_path(path));
            WriteLog("Delete outdated file {}", path);
        }
    });

    // Finalize
    WriteLog("Time {}", backing_time.GetDuration());
    WriteLog("Baking complete!");

    const auto build_hash_write_ok = DiskFileSystem::WriteFile(build_hash_path, FO_BUILD_HASH);
    FO_RUNTIME_ASSERT(build_hash_write_ok);
}

auto BaseBaker::ValidateProperties(const Properties& props, string_view context_str, const ScriptSystem* script_sys) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    static unordered_map<string, function<bool(hstring, const ScriptSystem*)>> script_func_verify = {
        {"ItemInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, BakerStub::Item*, bool>(func_name); }},
        {"ItemStatic", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<bool, BakerStub::Critter*, BakerStub::StaticItem*, BakerStub::Item*, any_t>(func_name); }},
        {"ItemTrigger", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, BakerStub::Critter*, BakerStub::StaticItem*, bool, uint8>(func_name); }},
        {"CritterInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, BakerStub::Critter*, bool>(func_name); }},
        {"MapInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, BakerStub::Map*, bool>(func_name); }},
        {"LocationInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, BakerStub::Location*, bool>(func_name); }},
    };

    FO_RUNTIME_ASSERT(_context->BakedFiles);

    size_t errors = 0;

    const auto* registrator = props.GetRegistrator();

    for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
        const auto* prop = registrator->GetPropertyByIndexUnsafe(i);

        if (prop->IsBaseTypeResource()) {
            if (prop->IsPlainData()) {
                const auto res_name = props.GetValue<hstring>(prop);

                if (res_name && !_context->BakedFiles->IsFileExists(res_name)) {
                    WriteLog("Resource {} not found for property {} in {}", res_name, prop->GetName(), context_str);
                    errors++;
                }
            }
            else if (prop->IsArray()) {
                if (props.GetRawDataSize(prop) == 0) {
                    continue;
                }

                const auto res_names = props.GetValue<vector<hstring>>(prop);

                for (const auto res_name : res_names) {
                    if (res_name && !_context->BakedFiles->IsFileExists(res_name)) {
                        WriteLog("Resource {} not found for property {} in {}", res_name, prop->GetName(), context_str);
                        errors++;
                    }
                }
            }
            else {
                WriteLog("Resource {} can be as standalone or in array in {}", prop->GetName(), context_str);
                errors++;
            }
        }

        if (script_sys != nullptr && !prop->GetBaseScriptFuncType().empty()) {
            if (prop->IsPlainData()) {
                const auto func_name = props.GetValue<hstring>(prop);

                if (script_func_verify.count(prop->GetBaseScriptFuncType()) == 0) {
                    WriteLog("Invalid script func {} of type {} for property {} in {}", func_name, prop->GetBaseScriptFuncType(), prop->GetName(), context_str);
                    errors++;
                }
                else if (func_name && !script_func_verify[prop->GetBaseScriptFuncType()](func_name, script_sys)) {
                    WriteLog("Verification failed for func {} of type {} for property {} in {}", func_name, prop->GetBaseScriptFuncType(), prop->GetName(), context_str);
                    errors++;
                }
            }
            else {
                WriteLog("Script {} must be as standalone (not in array or dict) in {}", prop->GetName(), context_str);
                errors++;
            }
        }
    }

    return errors;
}

BakerDataSource::BakerDataSource(BakingSettings& settings) :
    _settings {&settings}
{
    FO_STACK_TRACE_ENTRY();

    _outputResources.AddCustomSource(SafeAlloc::MakeUnique<DataSourceRef>(this));

    // Prepare input resources
    const auto res_packs = _settings->GetResourcePacks();
    _inputResources.reserve(res_packs.size());

    for (const auto& res_pack : res_packs) {
        auto& res_entry = _inputResources.emplace_back();
        res_entry.Name = res_pack.Name;
        const auto bake_checker = [this, res_pack_name = res_pack.Name](string_view path, uint64 write_time) -> bool { return CheckData(res_pack_name, path, write_time); };
        const auto write_data = [this, res_pack_name = res_pack.Name](string_view path, span<const uint8> data) { WriteData(res_pack_name, path, data); };
        res_entry.Bakers = BaseBaker::SetupBakers(res_pack.Bakers, res_pack.Name, *_settings, bake_checker, write_data, &_outputResources);

        for (const auto& dir : res_pack.InputDirs) {
            res_entry.InputDir.AddDirSource(dir, res_pack.RecursiveInput);
        }
        for (const auto& path : res_pack.InputFiles) {
            const auto dir = strex(path).extract_dir().str();
            const auto pack = strex(path).extract_file_name().erase_file_extension().str();
            res_entry.InputDir.AddCustomSource(DataSource::MountPack(dir, pack, false));
        }

        res_entry.InputFiles = res_entry.InputDir.GetAllFiles();
    }

    // Evaluate output files
    const auto check_file = [&](string_view path, uint64 write_time) {
        auto locker = std::scoped_lock(_outputFilesLocker);
        _outputFiles.emplace(path, write_time);
        return false;
    };

    const auto write_file = [](string_view path, span<const uint8> data) {
        ignore_unused(path, data);
        FO_UNREACHABLE_PLACE();
    };

    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_pack = res_packs[res_packs.size() - 1 - i];
        const auto& res_entry = _inputResources[_inputResources.size() - 1 - i];
        auto bakers = BaseBaker::SetupBakers(res_pack.Bakers, res_pack.Name, *_settings, check_file, write_file, &_outputResources);

        for (auto& baker : bakers) {
            baker->BakeFiles(res_entry.InputFiles);
        }
    }
}

auto BakerDataSource::MakeOutputPath(string_view res_pack_name, string_view path) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(_settings->BakeOutput).combine_path(res_pack_name).combine_path(path);
}

auto BakerDataSource::CheckData(string_view res_pack_name, string_view path, uint64 write_time) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto output_path = MakeOutputPath(res_pack_name, path);

    if (write_time > DiskFileSystem::GetWriteTime(output_path)) {
        auto locker = std::scoped_lock(_outputFilesLocker);
        _outputFiles.at(string(path)) = write_time;
        return true;
    }

    return false;
}

void BakerDataSource::WriteData(string_view res_pack_name, string_view path, span<const uint8> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto output_path = MakeOutputPath(res_pack_name, path);
    const auto write_file_ok = DiskFileSystem::WriteFile(output_path, data);
    FO_RUNTIME_ASSERT(write_file_ok);
}

auto BakerDataSource::FindFile(string_view path, size_t& size, uint64& write_time, unique_del_ptr<const uint8>* data) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    uint64 input_write_time;

    {
        auto locker = std::scoped_lock(_outputFilesLocker);
        const auto it = _outputFiles.find(path);

        if (it == _outputFiles.end()) {
            return false;
        }

        input_write_time = it->second;
    }

    const auto fill_result = [&](string_view output_path) {
        auto output_file = DiskFileSystem::OpenFile(output_path, false);
        FO_RUNTIME_ASSERT(output_file);

        size = output_file.GetSize();
        write_time = input_write_time;
        FO_RUNTIME_ASSERT(write_time != 0);

        if (data != nullptr) {
            auto buf = SafeAlloc::MakeUniqueArr<uint8>(size);
            const auto bake_file_read_ok = output_file.Read(buf.get(), size);
            FO_RUNTIME_ASSERT(bake_file_read_ok);
            *data = unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) FO_DEFERRED { delete[] p; }};
        }
    };

    // Try find already baked
    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_entry = _inputResources[_inputResources.size() - 1 - i];
        const auto output_path = MakeOutputPath(res_entry.Name, path);

        if (DiskFileSystem::IsExists(output_path)) {
            if (input_write_time > DiskFileSystem::GetWriteTime(output_path)) {
                const auto delete_output_file_ok = DiskFileSystem::DeleteFile(output_path);
                FO_RUNTIME_ASSERT(delete_output_file_ok);
                break;
            }

            fill_result(output_path);
            return true;
        }
    }

    // Runtime baking
    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_entry = _inputResources[_inputResources.size() - 1 - i];
        const auto output_path = MakeOutputPath(res_entry.Name, path);

        for (const auto& baker : res_entry.Bakers) {
            baker->BakeFiles(res_entry.InputFiles, path);
        }

        if (DiskFileSystem::IsExists(output_path)) {
            {
                auto locker = std::scoped_lock(_outputFilesLocker);
                input_write_time = _outputFiles.at(string(path));
                FO_RUNTIME_ASSERT(input_write_time < DiskFileSystem::GetWriteTime(output_path));
            }

            fill_result(output_path);
            return true;
        }
    }

    throw ResourceBakingException("File not baked", path);
}

auto BakerDataSource::IsFileExists(string_view path) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto locker = std::scoped_lock(_outputFilesLocker);

    return _outputFiles.contains(path);
}

auto BakerDataSource::GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (FindFile(path, size, write_time, nullptr)) {
        return true;
    }

    return false;
}

auto BakerDataSource::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    FO_STACK_TRACE_ENTRY();

    unique_del_ptr<const uint8> data;

    if (FindFile(path, size, write_time, &data)) {
        return data;
    }

    return nullptr;
}

auto BakerDataSource::GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    string fixed_dir = strex(dir).normalize_path_slashes();

    if (!dir.empty() && dir.back() == '/') {
        fixed_dir.resize(fixed_dir.size() - 1);
    }

    auto locker = std::scoped_lock(_outputFilesLocker);

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
