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
#include "DialogBaker.h"
#include "Dialogs.h"
#include "DiskFileSystem.h"
#include "EffectBaker.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "ImageBaker.h"
#include "Log.h"
#include "MapBaker.h"
#include "MapLoader.h"
#include "ModelBaker.h"
#include "ProtoBaker.h"
#include "ProtoManager.h"
#include "RawCopyBaker.h"
#include "ScriptSystem.h"
#include "Settings.h"
#include "StringUtils.h"
#include "TextBaker.h"
#include "Version-Include.h"

BaseBaker::BaseBaker(const BakerSettings& settings, string&& pack_name, BakeCheckerCallback&& bake_checker, AsyncWriteDataCallback&& write_data, const FileSystem* baked_files) :
    _settings {&settings},
    _packName {std::move(pack_name)},
    _bakeChecker {std::move(bake_checker)},
    _writeData {std::move(write_data)},
    _bakedFiles {baked_files}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_writeData);
}

auto BaseBaker::SetupBakers(const string& pack_name, const BakerSettings& settings, const BakeCheckerCallback& bake_checker, const AsyncWriteDataCallback& write_data, const FileSystem* baked_files) -> vector<unique_ptr<BaseBaker>>
{
    STACK_TRACE_ENTRY();

    vector<unique_ptr<BaseBaker>> bakers;

    bakers.emplace_back(SafeAlloc::MakeUnique<RawCopyBaker>(settings, pack_name, bake_checker, write_data, baked_files));
    bakers.emplace_back(SafeAlloc::MakeUnique<ImageBaker>(settings, pack_name, bake_checker, write_data, baked_files));
    bakers.emplace_back(SafeAlloc::MakeUnique<EffectBaker>(settings, pack_name, bake_checker, write_data, baked_files));
    bakers.emplace_back(SafeAlloc::MakeUnique<ProtoBaker>(settings, pack_name, bake_checker, write_data, baked_files));
    bakers.emplace_back(SafeAlloc::MakeUnique<MapBaker>(settings, pack_name, bake_checker, write_data, baked_files));
    bakers.emplace_back(SafeAlloc::MakeUnique<TextBaker>(settings, pack_name, bake_checker, write_data, baked_files));
    bakers.emplace_back(SafeAlloc::MakeUnique<DialogBaker>(settings, pack_name, bake_checker, write_data, baked_files));
#if FO_ENABLE_3D
    bakers.emplace_back(SafeAlloc::MakeUnique<ModelBaker>(settings, pack_name, bake_checker, write_data, baked_files));
#endif
#if FO_ANGELSCRIPT_SCRIPTING
    bakers.emplace_back(SafeAlloc::MakeUnique<AngelScriptBaker>(settings, pack_name, bake_checker, write_data, baked_files));
#endif

    extern void SetupBakersHook(vector<unique_ptr<BaseBaker>>&);
    SetupBakersHook(bakers);

    return bakers;
}

MasterBaker::MasterBaker(BakerSettings& settings) :
    _settings {settings}
{
    STACK_TRACE_ENTRY();
}

auto MasterBaker::MakeOutputPath(string_view path) const -> string
{
    STACK_TRACE_ENTRY();

    return strex(_settings.BakeOutput).combinePath(path);
}

void MasterBaker::BakeAll()
{
    STACK_TRACE_ENTRY();

#if FO_DEBUG
    if (IsRunInDebugger()) {
        const_cast<bool&>(_settings.ForceBaking) = true;
        const_cast<bool&>(_settings.SingleThreadBaking) = true;
    }
#endif

    WriteLog("Start baking");

    RUNTIME_ASSERT(!_settings.BakeOutput.empty());

    const auto bake_time = TimeMeter();

    const auto build_hash_deleted = DiskFileSystem::DeleteFile(MakeOutputPath("Resources.build-hash"));
    RUNTIME_ASSERT(build_hash_deleted);

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
            const auto del_configs_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Configs"));
                RUNTIME_ASSERT(del_configs_ok);
            }

        DiskFileSystem::MakeDirTree(MakeOutputPath("Configs"));

        int baked_configs = 0;
                    int settings_errors = 0;

        const auto bake_config = [&](string_view sub_config) {
            RUNTIME_ASSERT(App->Settings.AppliedConfigs.size() == 1);
            const auto maincfg = GlobalSettings(App->Settings.AppliedConfigs.front(), sub_config);
            const auto config_settings = maincfg.Save();

                    extern auto GetServerSettings() -> unordered_set<string>;
                    extern auto GetClientSettings() -> unordered_set<string>;
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
                const string cfg_path = MakeOutputPath(strex("Configs/{}{}.fomainb", cfg_name1, cfg_name2));

                if (!DiskFileSystem::CompareFileContent(cfg_path, {reinterpret_cast<const uint8*>(cfg_content.data()), cfg_content.size()})) {
                    auto cfg_file = DiskFileSystem::OpenFile(cfg_path, true);
                        RUNTIME_ASSERT(cfg_file);
                        const auto cfg_file_write_ok = cfg_file.Write(cfg_content);
                        RUNTIME_ASSERT(cfg_file_write_ok);
                    baked_configs++;
                }
                    };

            write_config(!sub_config.empty() ? "Server_" : "Server", sub_config, server_config_content);
            write_config(!sub_config.empty() ? "Client_" : "Client", sub_config, client_config_content);
        };

        bake_config("");

        for (const auto& sub_config : App->Settings.GetSubConfigs()) {
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
            const auto del_engine_data_ok = DiskFileSystem::DeleteDir(MakeOutputPath("EngineData"));
            RUNTIME_ASSERT(del_engine_data_ok);
                    }

        DiskFileSystem::MakeDirTree(MakeOutputPath("EngineData"));

        bool engine_data_baked = false;
        const auto engine_data_bin = BakerEngine::GetRestoreInfo();

        const auto del_core_engine_data_ok = DiskFileSystem::DeleteFile(MakeOutputPath("Core/EngineData.fobin"));
        RUNTIME_ASSERT(del_core_engine_data_ok);

        if (!DiskFileSystem::CompareFileContent(MakeOutputPath("EngineData/EngineData.fobin"), engine_data_bin)) {
            auto engine_data_file = DiskFileSystem::OpenFile(MakeOutputPath("EngineData/EngineData.fobin"), true);
            RUNTIME_ASSERT(engine_data_file);
            const auto engine_data_file_write_ok = engine_data_file.Write(engine_data_bin);
            RUNTIME_ASSERT(engine_data_file_write_ok);
            engine_data_baked = true;
                    }

        baking_output.AddDataSource(MakeOutputPath("EngineData"));

        if (engine_data_baked) {
            force_baking = true;
                            }

        WriteLog("Baking of engine data complete, baked {} file{}", engine_data_baked ? 1 : 0, engine_data_baked ? "" : "s");
                }

    {
        std::atomic_bool res_baked = false;

        // Resource packs
        const auto bake_resource_pack = [ // clang-format off
                    &thiz = std::as_const(*this),
                    &settings = std::as_const(_settings),
            &baking_output_ = std::as_const(baking_output),
            &force_baking_ = force_baking,
            &res_baked_ = res_baked // clang-format on
        ](const ResourcePackInfo& res_pack, const string& output_dir) {
            const auto& pack_name = res_pack.Name;
            const auto& input_dir = res_pack.InputDir;

            FileSystem input_files;

            for (const auto& dir : input_dir) {
                input_files.AddDataSource(dir);
                            }

            const auto filtered_files = input_files.FilterFiles("", "", res_pack.RecursiveInput);

            // Cleanup previous
            if (force_baking_) {
                const auto del_res_ok = DiskFileSystem::DeleteDir(output_dir);
                RUNTIME_ASSERT(del_res_ok);
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

            const auto bake_checker = [&](const string& path, uint64 write_time) -> bool {
                if (!force_baking_) {
                    actual_resource_names.emplace(exclude_all_ext(path));
                    return write_time > DiskFileSystem::GetWriteTime(strex(output_dir).combinePath(path));
                }
                else {
                    return true;
                    }
            };

            const auto write_data = [&](string_view path, const_span<uint8> baked_data) {
                auto res_file = DiskFileSystem::OpenFile(strex(output_dir).combinePath(path), true);
                RUNTIME_ASSERT(res_file);
                const auto res_file_write_ok = res_file.Write(baked_data);
                RUNTIME_ASSERT(res_file_write_ok);

                ++baked_files;
                };

            auto bakers = BaseBaker::SetupBakers(res_pack.Name, settings, bake_checker, write_data, &baking_output_);

            for (auto& baker : bakers) {
                baker->BakeFiles(filtered_files.Copy());
        }

            // Delete outdated
            if (!force_baking_) {
                DiskFileSystem::IterateDir(output_dir, "", true, [&](string_view path, size_t size, uint64 write_time) {
                    UNUSED_VARIABLE(size);
                    UNUSED_VARIABLE(write_time);

                    if (actual_resource_names.count(exclude_all_ext(path)) == 0) {
                        DiskFileSystem::DeleteFile(strex(output_dir).combinePath(path));
                        WriteLog("Delete outdated file {}", path);
            }
                });
                        }

            if (baked_files != 0) {
                res_baked_ = true;
                                }

            WriteLog("Baking of {} complete, baked {} file{}", pack_name, baked_files, baked_files != 1 ? "s" : "");
        };

        int errors = 0;

        for (int bake_order = 0; bake_order <= _settings.MaxBakeOrder; bake_order++) {
            vector<std::future<void>> res_bakings;
            vector<string> res_baking_outputs;

            for (const auto& res_pack : App->Settings.GetResourcePacks()) {
                if (res_pack.BakeOrder == bake_order) {
                    WriteLog("Bake {}", res_pack.Name);
                    const auto res_baking_output = MakeOutputPath(res_pack.Name);
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
                    WriteLog("{}", ex.what());
            errors++;
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }
            }

            if (errors != 0) {
                ExitApp(false);
                    }

            for (const auto& res_baking_output : res_baking_outputs) {
                baking_output.AddDataSource(res_baking_output);
            }

            if (res_baked) {
                force_baking = true;
                }
                }

        // Copy engine data to Core pack
        const auto copy_engine_data_ok = DiskFileSystem::CopyFile(MakeOutputPath("EngineData/EngineData.fobin"), MakeOutputPath("Core/EngineData.fobin"));
        RUNTIME_ASSERT(copy_engine_data_ok);
                        }

    // Finalize
    WriteLog("Time {}", bake_time.GetDuration());
    WriteLog("Baking complete!");

    {
        auto build_hash_file = DiskFileSystem::OpenFile(MakeOutputPath("Resources.build-hash"), true, true);
        RUNTIME_ASSERT(build_hash_file);
        const auto build_hash_writed = build_hash_file.Write(FO_BUILD_HASH);
        RUNTIME_ASSERT(build_hash_writed);
    }
}

class Item;
using StaticItem = Item;
class Critter;
class Map;
class Location;

auto BaseBaker::ValidateProperties(const Properties& props, string_view context_str, const ScriptSystem* script_sys) const -> int
{
    STACK_TRACE_ENTRY();

    static unordered_map<string, std::function<bool(hstring, const ScriptSystem*)>> script_func_verify = {
        {"ItemInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Item*, bool>(func_name); }},
        {"ItemStatic", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<bool, Critter*, StaticItem*, Item*, any_t>(func_name); }},
        {"ItemTrigger", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Critter*, StaticItem*, bool, uint8>(func_name); }},
        {"CritterInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Critter*, bool>(func_name); }},
        {"MapInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Map*, bool>(func_name); }},
        {"LocationInit", [](hstring func_name, const ScriptSystem* script_sys_) { return script_sys_->CheckFunc<void, Location*, bool>(func_name); }},
    };

    RUNTIME_ASSERT(_bakedFiles);

    int errors = 0;

    const auto* registrator = props.GetRegistrator();

    for (size_t i = 0; i < registrator->GetPropertiesCount(); i++) {
        const auto* prop = registrator->GetPropertyByIndexUnsafe(i);

        if (prop->IsBaseTypeResource()) {
            if (prop->IsPlainData()) {
                const auto res_name = props.GetValue<hstring>(prop);

                if (res_name && !_bakedFiles->ReadFileHeader(res_name)) {
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
                    if (res_name && !_bakedFiles->ReadFileHeader(res_name)) {
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

BakerDataSource::BakerDataSource(FileSystem& input_resources, BakerSettings& settings) :
    _inputResources {input_resources},
    _settings {settings}
{
    STACK_TRACE_ENTRY();

    _bakers = BaseBaker::SetupBakers("Baker", _settings, nullptr, [this](string_view baked_path, const_span<uint8> baked_data) { WriteData(baked_path, baked_data); }, nullptr);
}

void BakerDataSource::WriteData(string_view baked_path, const_span<uint8> baked_data)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto locker = std::scoped_lock(_bakedCacheLocker);

    auto baked_file = File(strex(baked_path).extractFileName().eraseFileExtension(), baked_path, 0, this, baked_data, true);
    _bakedCache[baked_path] = SafeAlloc::MakeUnique<File>(std::move(baked_file));
}

auto BakerDataSource::FindFile(string_view path) const -> File*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _bakedCache.find(path); it != _bakedCache.end()) {
        return it->second ? it->second.get() : nullptr;
    }

    bool file_baked = false;

    if (auto file = _inputResources.ReadFile(path)) {
        const string ext = strex(path).getFileExtension();

        for (auto& baker : _bakers) {
            if (baker->IsExtSupported(ext)) {
                baker->BakeFiles(FileCollection({file.Copy()}));
                file_baked = true;
            }
        }

        if (!file_baked) {
            _bakedCache[path] = SafeAlloc::MakeUnique<File>(std::move(file));
        }
    }
    else {
        BreakIntoDebugger();
    }

    if (!file_baked) {
        _bakedCache[path] = nullptr;
    }

    if (const auto it = _bakedCache.find(path); it != _bakedCache.end()) {
        return it->second ? it->second.get() : nullptr;
    }
    else {
        BreakIntoDebugger();
        throw NotImplementedException(LINE_STR);
    }
}

auto BakerDataSource::IsFilePresent(string_view path, size_t& size, uint64& write_time) const -> bool
{
    STACK_TRACE_ENTRY();

    if (const auto* file = FindFile(path); file != nullptr) {
        size = file->GetSize();
        write_time = file->GetWriteTime();
        return true;
    }

    return false;
}

auto BakerDataSource::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    STACK_TRACE_ENTRY();

    if (const auto* file = FindFile(path); file != nullptr) {
        size = file->GetSize();
        write_time = file->GetWriteTime();
        return unique_del_ptr<const uint8> {file->GetBuf(), [](auto* p) { UNUSED_VARIABLE(p); }};
    }

    return nullptr;
}

auto BakerDataSource::GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string>
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(path);
    UNUSED_VARIABLE(include_subdirs);
    UNUSED_VARIABLE(ext);

    throw NotImplementedException(LINE_STR);
}

BakerEngine::BakerEngine(PropertiesRelationType props_relation) :
    EngineData(props_relation, [this] {
        extern void Baker_RegisterData(EngineData*);
        Baker_RegisterData(this);
    })
{
    STACK_TRACE_ENTRY();
}

auto BakerEngine::GetRestoreInfo() -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    extern auto Baker_GetRestoreInfo() -> vector<uint8>;
    return Baker_GetRestoreInfo();
}

BakerScriptSystem::BakerScriptSystem(BakerEngine& engine, const FileSystem& resources)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(engine.GetPropertiesRelation() == PropertiesRelationType::ServerRelative);

#if FO_ANGELSCRIPT_SCRIPTING
    extern void Init_AngelScriptCompiler_ServerScriptSystem_Validation(EngineData*, ScriptSystem&, const FileSystem&);
    Init_AngelScriptCompiler_ServerScriptSystem_Validation(&engine, *this, resources);
#endif
}
