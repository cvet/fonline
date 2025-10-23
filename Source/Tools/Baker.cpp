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

#include "Baker.h"
#include "AngelScriptBaker.h"
#include "Application.h"
#include "ConfigFile.h"
#include "EffectBaker.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "ImageBaker.h"
#include "MapBaker.h"
#include "MapLoader.h"
#include "ModelBaker.h"
#include "ProtoBaker.h"
#include "ProtoManager.h"
#include "ProtoTextBaker.h"
#include "RawCopyBaker.h"
#include "ScriptSystem.h"
#include "Settings.h"
#include "TextBaker.h"
#include "Version-Include.h"

FO_BEGIN_NAMESPACE();

extern auto GetServerSettings() -> unordered_set<string>;
extern auto GetClientSettings() -> unordered_set<string>;

extern void Baker_RegisterData(EngineData*);
extern auto Baker_GetRestoreInfo() -> vector<uint8>;

extern void SetupBakersHook(vector<unique_ptr<BaseBaker>>&, BakerData&);

#if FO_ANGELSCRIPT_SCRIPTING
extern void Init_AngelScriptCompiler_ServerScriptSystem_Validation(EngineData*, ScriptSystem&, const FileSystem&);
#endif

BaseBaker::BaseBaker(BakerData& data) :
    _settings {data.Settings},
    _resPackName {data.PackName},
    _bakeChecker {data.BakeChecker},
    _writeData {data.WriteData},
    _bakedFiles {data.BakedFiles}
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_writeData);
}

auto BaseBaker::SetupBakers(const string& pack_name, const BakingSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, const FileSystem* baked_files) -> vector<unique_ptr<BaseBaker>>
{
    FO_STACK_TRACE_ENTRY();

    vector<unique_ptr<BaseBaker>> bakers;

    auto baker_data = BakerData {.Settings = &settings, .PackName = pack_name, .BakeChecker = bake_checker, .WriteData = write_data, .BakedFiles = baked_files};

    bakers.emplace_back(SafeAlloc::MakeUnique<RawCopyBaker>(baker_data));
    bakers.emplace_back(SafeAlloc::MakeUnique<ImageBaker>(baker_data));
    bakers.emplace_back(SafeAlloc::MakeUnique<EffectBaker>(baker_data));
    bakers.emplace_back(SafeAlloc::MakeUnique<ProtoBaker>(baker_data));
    bakers.emplace_back(SafeAlloc::MakeUnique<MapBaker>(baker_data));
    bakers.emplace_back(SafeAlloc::MakeUnique<TextBaker>(baker_data));
    bakers.emplace_back(SafeAlloc::MakeUnique<ProtoTextBaker>(baker_data));
#if FO_ENABLE_3D
    bakers.emplace_back(SafeAlloc::MakeUnique<ModelBaker>(baker_data));
#endif
#if FO_ANGELSCRIPT_SCRIPTING
    bakers.emplace_back(SafeAlloc::MakeUnique<AngelScriptBaker>(baker_data));
#endif

    SetupBakersHook(bakers, baker_data);

    return bakers;
}

MasterBaker::MasterBaker(BakingSettings& settings) noexcept :
    _settings {settings}
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

    FO_NON_CONST_METHOD_HINT();

    const auto bake_time = TimeMeter();

    WriteLog("Start baking");

    FO_RUNTIME_ASSERT(!_settings.BakeOutput.empty());
    const auto make_output_path = [this](string_view path) -> string { return strex(_settings.BakeOutput).combinePath(path); };

    const auto build_hash_deleted = DiskFileSystem::DeleteFile(make_output_path("Resources.build-hash"));
    FO_RUNTIME_ASSERT(build_hash_deleted);

    std::atomic_bool force_baking = false;

    if (_settings.ForceBaking) {
        WriteLog("Force rebuild all resources");
        DiskFileSystem::DeleteDir(_settings.BakeOutput);
        DiskFileSystem::MakeDirTree(_settings.BakeOutput);
        force_baking = true;
    }

    FileSystem baking_output;

    // Configs
    {
        WriteLog("Bake configs");

        if (force_baking) {
            const auto del_configs_ok = DiskFileSystem::DeleteDir(make_output_path("Configs"));
            FO_RUNTIME_ASSERT(del_configs_ok);
        }

        DiskFileSystem::MakeDirTree(make_output_path("Configs"));

        int32 baked_configs = 0;
        int32 settings_errors = 0;

        const auto bake_config = [&](string_view sub_config) {
            FO_RUNTIME_ASSERT(_settings.GetAppliedConfigs().size() == 1);
            const string config_path = _settings.GetAppliedConfigs().front();
            const string config_name = strex(config_path).extractFileName();
            const string config_dir = strex(config_path).extractDir();

            auto maincfg = GlobalSettings(true);
            maincfg.ApplyConfigAtPath(config_name, config_dir);

            if (!sub_config.empty()) {
                maincfg.ApplySubConfigSection(sub_config);
            }

            const auto config_settings = maincfg.Save();

            auto server_settings = GetServerSettings();
            auto client_settings = GetClientSettings();

            string server_config_content;
            server_config_content.reserve(0x4000);
            string client_config_content;
            client_config_content.reserve(0x4000);

            for (auto&& [key, value] : config_settings) {
                const auto is_server_setting = server_settings.count(key) != 0;
                const auto is_client_setting = client_settings.count(key) != 0;
                const bool skip_write = value.empty() || value == "0" || strex(value).lower() == "false";
                const auto shortened_value = strex(value).isExplicitBool() ? (strex(value).toBool() ? "1" : "0") : value;

                if (!skip_write) {
                    server_config_content += strex("{}={}\n", key, shortened_value);
                }

                if (is_server_setting) {
                    server_settings.erase(key);
                }

                if (is_client_setting) {
                    if (!skip_write) {
                        client_config_content += strex("{}={}\n", key, shortened_value);
                    }

                    client_settings.erase(key);
                }

                if (!is_server_setting && !is_client_setting) {
                    WriteLog("Unknown setting {} = {}", key, value);
                    settings_errors++;
                }
            }

            for (const auto& key : server_settings) {
                WriteLog("Uninitialized server setting {}", key);
                settings_errors++;
            }
            for (const auto& key : client_settings) {
                WriteLog("Uninitialized client setting {}", key);
                settings_errors++;
            }

            const auto write_config = [&](string_view cfg_name1, string_view cfg_name2, string_view cfg_content) {
                const string cfg_path = make_output_path(strex("Configs/{}{}.fomain-bin", cfg_name1, cfg_name2));

                if (!DiskFileSystem::CompareFileContent(cfg_path, {reinterpret_cast<const uint8*>(cfg_content.data()), cfg_content.size()})) {
                    auto cfg_file = DiskFileSystem::OpenFile(cfg_path, true);
                    FO_RUNTIME_ASSERT(cfg_file);
                    const auto cfg_file_write_ok = cfg_file.Write(cfg_content);
                    FO_RUNTIME_ASSERT(cfg_file_write_ok);
                    baked_configs++;
                }
            };

            write_config(!sub_config.empty() ? "Server_" : "Server", sub_config, server_config_content);
            write_config(!sub_config.empty() ? "Client_" : "Client", sub_config, client_config_content);
        };

        bake_config("");

        for (const auto& sub_config : _settings.GetSubConfigs()) {
            bake_config(sub_config.Name);

            if (settings_errors != 0) {
                WriteLog("Main config error{}", settings_errors != 1 ? "s" : "");
                ExitApp(false);
            }
        }

        if (baked_configs != 0) {
            force_baking = true;
        }

        WriteLog("Baking of configs complete, baked {} file{}", baked_configs, baked_configs != 1 ? "s" : "");
    }

    // Engine data
    {
        WriteLog("Bake engine data");

        if (force_baking) {
            const auto del_engine_data_ok = DiskFileSystem::DeleteDir(make_output_path("EngineData"));
            FO_RUNTIME_ASSERT(del_engine_data_ok);
        }

        DiskFileSystem::MakeDirTree(make_output_path("EngineData"));

        bool engine_data_baked = false;
        const auto engine_data_bin = BakerEngine::GetRestoreInfo();

        const auto del_core_engine_data_ok = DiskFileSystem::DeleteFile(make_output_path("Core/EngineData.fobin"));
        FO_RUNTIME_ASSERT(del_core_engine_data_ok);

        if (!DiskFileSystem::CompareFileContent(make_output_path("EngineData/EngineData.fobin"), engine_data_bin)) {
            auto engine_data_file = DiskFileSystem::OpenFile(make_output_path("EngineData/EngineData.fobin"), true);
            FO_RUNTIME_ASSERT(engine_data_file);
            const auto engine_data_file_write_ok = engine_data_file.Write(engine_data_bin);
            FO_RUNTIME_ASSERT(engine_data_file_write_ok);
            engine_data_baked = true;
        }

        baking_output.AddDirSource(make_output_path("EngineData"));

        if (engine_data_baked) {
            force_baking = true;
        }

        WriteLog("Baking of engine data complete, baked {} file{}", engine_data_baked ? 1 : 0, engine_data_baked ? "" : "s");
    }

    {
        std::atomic_bool res_baked = false;

        // Resource packs
        const auto bake_resource_pack = [ // clang-format off
            &settings = std::as_const(_settings),
            &baking_output_ = std::as_const(baking_output),
            &force_baking, &res_baked // clang-format on
        ](const ResourcePackInfo& res_pack, const string& output_dir) {
            const auto& pack_name = res_pack.Name;
            const auto& input_dir = res_pack.InputDir;

            FileSystem input_files;

            for (const auto& dir : input_dir) {
                input_files.AddDirSource(dir, res_pack.RecursiveInput);
            }

            const auto filtered_files = input_files.GetAllFiles();

            // Cleanup previous
            if (force_baking) {
                const auto del_res_ok = DiskFileSystem::DeleteDir(output_dir);
                FO_RUNTIME_ASSERT(del_res_ok);
            }

            DiskFileSystem::MakeDirTree(output_dir);

            // Bake files
            std::atomic_int baked_files = 0;
            unordered_set<string> actual_resource_names;

            const auto exclude_all_ext = [](string_view path) -> string {
                size_t pos = path.rfind('/');
                pos = path.find('.', pos != string::npos ? pos : 0);
                return strex(pos != string::npos ? path.substr(0, pos) : path).lower();
            };

            const auto bake_checker = [&](string_view path, uint64 write_time) -> bool {
                if (!force_baking) {
                    actual_resource_names.emplace(exclude_all_ext(path));
                    return write_time > DiskFileSystem::GetWriteTime(strex(output_dir).combinePath(path));
                }
                else {
                    return true;
                }
            };

            const auto write_data = [&](string_view path, const_span<uint8> baked_data) {
                const auto res_path = strex(output_dir).combinePath(path).str();
                auto res_file = DiskFileSystem::OpenFile(res_path, true);
                FO_RUNTIME_ASSERT_STR(res_file, strex("Unable to write file '{}'", res_path));
                const auto res_file_write_ok = res_file.Write(baked_data);
                FO_RUNTIME_ASSERT(res_file_write_ok);

                ++baked_files;
            };

            auto bakers = BaseBaker::SetupBakers(res_pack.Name, settings, bake_checker, write_data, &baking_output_);

            for (auto& baker : bakers) {
                baker->BakeFiles(filtered_files);
            }

            // Delete outdated
            if (!force_baking) {
                DiskFileSystem::IterateDir(output_dir, true, [&](string_view path, size_t size, uint64 write_time) {
                    ignore_unused(size);
                    ignore_unused(write_time);

                    if (actual_resource_names.count(exclude_all_ext(path)) == 0) {
                        DiskFileSystem::DeleteFile(strex(output_dir).combinePath(path));
                        WriteLog("Delete outdated file {}", path);
                    }
                });
            }

            if (baked_files != 0) {
                res_baked = true;
            }

            WriteLog("Baking of {} complete, baked {} file{}", pack_name, baked_files, baked_files != 1 ? "s" : "");
        };

        size_t errors = 0;

        for (int32 bake_order = 0; bake_order <= _settings.MaxBakeOrder; bake_order++) {
            vector<std::future<void>> res_bakings;
            vector<string> res_baking_outputs;

            for (const auto& res_pack : _settings.GetResourcePacks()) {
                if (res_pack.BakeOrder == bake_order) {
                    WriteLog("Bake {}", res_pack.Name);
                    const auto res_baking_output = make_output_path(res_pack.Name);
                    const auto async_mode = _settings.SingleThreadBaking ? std::launch::deferred : std::launch::async | std::launch::deferred;
                    auto res_baking = std::async(async_mode, [&, res_baking_output] { bake_resource_pack(res_pack, res_baking_output); });
                    res_bakings.emplace_back(std::move(res_baking));
                    res_baking_outputs.emplace_back(res_baking_output);
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
                catch (...) {
                    FO_UNKNOWN_EXCEPTION();
                }
            }

            if (errors != 0) {
                throw ResourceBakingException("Baking resource packs failed");
            }

            for (const auto& res_baking_output : res_baking_outputs) {
                baking_output.AddDirSource(res_baking_output, true);
            }

            if (res_baked) {
                force_baking = true;
            }
        }

        // Copy engine data to Core pack
        const auto copy_engine_data_ok = DiskFileSystem::CopyFile(make_output_path("EngineData/EngineData.fobin"), make_output_path("Core/EngineData.fobin"));
        FO_RUNTIME_ASSERT(copy_engine_data_ok);
    }

    // Finalize
    WriteLog("Time {}", bake_time.GetDuration());
    WriteLog("Baking complete!");

    {
        auto build_hash_file = DiskFileSystem::OpenFile(make_output_path("Resources.build-hash"), true, true);
        FO_RUNTIME_ASSERT(build_hash_file);
        const auto build_hash_writed = build_hash_file.Write(FO_BUILD_HASH);
        FO_RUNTIME_ASSERT(build_hash_writed);
    }
}

class Item;
using StaticItem = Item;
class Critter;
class Map;
class Location;

auto BaseBaker::ValidateProperties(const Properties& props, string_view context_str, const ScriptSystem* script_sys) const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    static unordered_map<string, function<bool(hstring, const ScriptSystem*)>> script_func_verify = {
        {"ItemInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Item*, bool>(func_name); }},
        {"ItemStatic", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<bool, Critter*, StaticItem*, Item*, any_t>(func_name); }},
        {"ItemTrigger", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Critter*, StaticItem*, bool, uint8>(func_name); }},
        {"CritterInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Critter*, bool>(func_name); }},
        {"MapInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Map*, bool>(func_name); }},
        {"LocationInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Location*, bool>(func_name); }},
    };

    FO_RUNTIME_ASSERT(_bakedFiles);

    size_t errors = 0;

    const auto* registrator = props.GetRegistrator();

    for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
        const auto* prop = registrator->GetPropertyByIndexUnsafe(i);

        if (prop->IsBaseTypeResource()) {
            if (prop->IsPlainData()) {
                const auto res_name = props.GetValue<hstring>(prop);

                if (res_name && !_bakedFiles->IsFileExists(res_name)) {
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
                    if (res_name && !_bakedFiles->IsFileExists(res_name)) {
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
    _settings {settings}
{
    FO_STACK_TRACE_ENTRY();

    _outputResources.AddCustomSource(SafeAlloc::MakeUnique<DataSourceRef>(this));

    // Prepare input resources
    const auto res_packs = _settings.GetResourcePacks();
    _inputResources.reserve(res_packs.size());

    for (const auto& res_pack : res_packs) {
        auto& res_entry = _inputResources.emplace_back();
        res_entry.Name = res_pack.Name;
        const auto bake_checker = [this, res_pack_name = res_pack.Name](string_view path, uint64 write_time) -> bool { return CheckData(res_pack_name, path, write_time); };
        const auto write_data = [this, res_pack_name = res_pack.Name](string_view path, const_span<uint8> data) { WriteData(res_pack_name, path, data); };
        res_entry.Bakers = BaseBaker::SetupBakers(res_pack.Name, _settings, bake_checker, write_data, &_outputResources);

        for (const auto& dir : res_pack.InputDir) {
            res_entry.InputDir.AddDirSource(dir, res_pack.RecursiveInput);
        }

        res_entry.InputFiles = res_entry.InputDir.GetAllFiles();
    }

    // Evaluate output files
    const auto check_file = [&](string_view path, uint64 write_time) {
        _outputFiles.emplace(path, write_time);
        return false;
    };

    const auto write_file = [](string_view path, const_span<uint8> data) {
        ignore_unused(path, data);
        FO_UNREACHABLE_PLACE();
    };

    for (size_t i = 0; i < _inputResources.size(); i++) {
        const auto& res_entry = _inputResources[_inputResources.size() - 1 - i];

        for (auto& baker : BaseBaker::SetupBakers(res_entry.Name, _settings, check_file, write_file, &_outputResources)) {
            baker->BakeFiles(res_entry.InputFiles);
        }
    }
}

auto BakerDataSource::MakeOutputPath(string_view res_pack_name, string_view path) const -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(_settings.BakeOutput).combinePath(res_pack_name).combinePath(path);
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

void BakerDataSource::WriteData(string_view res_pack_name, string_view path, const_span<uint8> data)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    const auto output_path = MakeOutputPath(res_pack_name, path);
    auto output_file = DiskFileSystem::OpenFile(output_path, true);
    FO_RUNTIME_ASSERT(output_file);
    output_file.Write(data);
}

auto BakerDataSource::FindFile(string_view path, size_t& size, uint64& write_time, unique_del_ptr<const uint8>* data) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    uint64 input_write_time;

    {
        auto locker = std::scoped_lock(_outputFilesLocker);
        const auto it = _outputFiles.find(path);

        if (it == _outputFiles.end()) {
            BreakIntoDebugger();
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
            *data = unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) { delete[] p; }};
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

    string fixed_dir = strex(dir).normalizePathSlashes();

    if (!dir.empty() && dir.back() == '/') {
        fixed_dir.resize(fixed_dir.size() - 1);
    }

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
        if (!ext.empty() && strex(fpath).getFileExtension() != ext) {
            continue;
        }

        result.emplace_back(fpath);
    }

    return result;
}

BakerEngine::BakerEngine(PropertiesRelationType props_relation) :
    EngineData(props_relation, [this] { Baker_RegisterData(this); })
{
    FO_STACK_TRACE_ENTRY();
}

auto BakerEngine::GetRestoreInfo() -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    return Baker_GetRestoreInfo();
}

BakerScriptSystem::BakerScriptSystem(BakerEngine& engine, const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(engine.GetPropertiesRelation() == PropertiesRelationType::ServerRelative);

#if FO_ANGELSCRIPT_SCRIPTING
    Init_AngelScriptCompiler_ServerScriptSystem_Validation(&engine, *this, resources);
#endif
}

FO_END_NAMESPACE();
