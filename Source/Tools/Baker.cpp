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

#include "Baker.h"
#include "Application.h"
#include "ConfigFile.h"
#include "Dialogs.h"
#include "DiskFileSystem.h"
#include "EffectBaker.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "ImageBaker.h"
#include "Log.h"
#include "MapLoader.h"
#include "ModelBaker.h"
#include "ProtoManager.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Version-Include.h"

class BakerEngine : public FOEngineBase
{
public:
    explicit BakerEngine(PropertiesRelationType props_relation) :
        FOEngineBase(_dummySettings, props_relation)
    {
        extern void Baker_RegisterData(FOEngineBase*);
        Baker_RegisterData(this);
    }

    static auto GetRestoreInfo() -> vector<uint8>
    {
        extern auto Baker_GetRestoreInfo() -> vector<uint8>;
        return Baker_GetRestoreInfo();
    }

private:
    static GlobalSettings _dummySettings;
};
GlobalSettings BakerEngine::_dummySettings {};

// Implementation in AngelScriptScripting-*Compiler.cpp
#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
struct AngelScriptCompiler_ServerScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources);
};
struct AngelScriptCompiler_ServerScriptSystem_Validation : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources, FOEngineBase** out_engine);
};
struct AngelScriptCompiler_ClientScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources);
};
#else
struct AngelScriptCompiler_SingleScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources);
};
struct AngelScriptCompiler_SingleScriptSystem_Validation : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources, FOEngineBase** out_engine);
};
#endif
struct AngelScriptCompiler_MapperScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources);
};

// External variable for compiler messages
unordered_set<string> CompilerPassedMessages;
#endif

class Item;
class StaticItem;
class Critter;
class Map;
class Location;

BaseBaker::BaseBaker(const BakerSettings& settings, BakeCheckerCallback&& bake_checker, WriteDataCallback&& write_data) :
    _settings {settings},
    _bakeChecker {std::move(bake_checker)},
    _writeData {std::move(write_data)}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_writeData);
}

auto BaseBaker::SetupBakers(const BakerSettings& settings, BakeCheckerCallback bake_checker, WriteDataCallback write_data) -> vector<unique_ptr<BaseBaker>>
{
    STACK_TRACE_ENTRY();

    vector<unique_ptr<BaseBaker>> bakers;

    bakers.emplace_back(std::make_unique<ImageBaker>(settings, bake_checker, write_data));
    bakers.emplace_back(std::make_unique<EffectBaker>(settings, bake_checker, write_data));
#if FO_ENABLE_3D
    bakers.emplace_back(std::make_unique<ModelBaker>(settings, bake_checker, write_data));
#endif

    extern void SetupBakersHook(vector<unique_ptr<BaseBaker>>&);
    SetupBakersHook(bakers);

    return bakers;
}

Baker::Baker(BakerSettings& settings) :
    _settings {settings}
{
    STACK_TRACE_ENTRY();
}

auto Baker::MakeOutputPath(string_view path) const -> string
{
    STACK_TRACE_ENTRY();

    return strex(_settings.BakeOutput).combinePath(path);
}

void Baker::BakeAll()
{
    STACK_TRACE_ENTRY();

#if FO_DEBUG
    if (IsRunInDebugger()) {
        const_cast<bool&>(_settings.ForceBaking) = true;
        const_cast<bool&>(_settings.SingleThreadBaking) = true;
    }
#endif

    WriteLog("Start baking");

    const auto bake_time = TimeMeter();

    const auto build_hash_deleted = DiskFileSystem::DeleteFile(MakeOutputPath("Resources.build-hash"));
    RUNTIME_ASSERT(build_hash_deleted);

    if (_settings.ForceBaking) {
        WriteLog("Force rebuild all resources");
    }

    int errors = 0;

    const auto async_mode = _settings.SingleThreadBaking ? std::launch::deferred : std::launch::async | std::launch::deferred;

    // Resource packs
    unordered_set<string> resource_names;
    std::mutex resource_names_locker;

    try {
        WriteLog("Start bake resource packs");

        RUNTIME_ASSERT(!_settings.BakeResourceEntries.empty());

        map<string, vector<string>> res_packs;

        for (const auto& re : _settings.BakeResourceEntries) {
            auto re_splitted = strex(re).split(',');
            RUNTIME_ASSERT(re_splitted.size() == 2);
            res_packs[re_splitted[0]].emplace_back(std::move(re_splitted[1]));
        }

        const auto bake_resource_pack = [ // clang-format off
            &thiz = std::as_const(*this),
            &settings = std::as_const(_settings),
            &resource_names = resource_names,
            &resource_names_locker = resource_names_locker // clang-format on
        ](const string& pack_name, const vector<string>& paths) {
            FileSystem res_files;

            for (const auto& path : paths) {
                res_files.AddDataSource(path);
            }

            // Cleanup previous
            if (settings.ForceBaking) {
                auto del_res_ok = DiskFileSystem::DeleteDir(thiz.MakeOutputPath(pack_name));
                RUNTIME_ASSERT(del_res_ok);
            }

            DiskFileSystem::MakeDirTree(thiz.MakeOutputPath(pack_name));

            const auto is_raw_only = pack_name == "Raw";

            std::atomic_int baked_files = 0;

            // Bake files
            auto pack_resource_names = unordered_set<string>();

            const auto exclude_all_ext = [](string_view path) -> string {
                size_t pos = path.rfind('/');
                pos = path.find('.', pos != string::npos ? pos : 0);
                return pos != string::npos ? string(path.substr(0, pos)) : string(path);
            };
            const auto make_output_path = [](string_view path) -> string {
                if (strex(path).startsWith("art/critters/")) {
                    return strex("art/critters/{}", strex(path.substr("art/critters/"_len)).lower());
                }
                return string(path);
            };

            if (!is_raw_only) {
                const auto bake_checker = [&](const FileHeader& file_header) -> bool {
                    const auto output_path = make_output_path(file_header.GetPath());

                    {
                        auto locker = std::unique_lock {resource_names_locker};
                        resource_names.emplace(output_path);
                    }

                    pack_resource_names.emplace(exclude_all_ext(output_path));

                    if (!settings.ForceBaking) {
                        return file_header.GetWriteTime() > DiskFileSystem::GetWriteTime(thiz.MakeOutputPath(strex(pack_name).combinePath(output_path)));
                    }
                    else {
                        return true;
                    }
                };

                const auto write_data = [&](string_view path, const_span<uint8> baked_data) {
                    const auto output_path = make_output_path(path);

                    auto res_file = DiskFileSystem::OpenFile(thiz.MakeOutputPath(strex(pack_name).combinePath(output_path)), true);
                    RUNTIME_ASSERT(res_file);
                    const auto res_file_write_ok = res_file.Write(baked_data);
                    RUNTIME_ASSERT(res_file_write_ok);

                    ++baked_files;
                };

                auto bakers = BaseBaker::SetupBakers(settings, bake_checker, write_data);

                for (auto&& baker : bakers) {
                    baker->BakeFiles(res_files.GetAllFiles());
                }
            }

            // Raw copy
            for (auto files_to_bake = res_files.GetAllFiles(); files_to_bake.MoveNext();) {
                auto file_header = files_to_bake.GetCurFileHeader();
                const auto output_path = make_output_path(file_header.GetPath());

                // Skip not necessary files
                if (!is_raw_only) {
                    const string ext = strex(output_path).getFileExtension();
                    const auto& base_exts = settings.BakeExtraFileExtensions;

                    if (std::find(base_exts.begin(), base_exts.end(), ext) == base_exts.end()) {
                        continue;
                    }
                }

                {
                    auto locker = std::unique_lock {resource_names_locker};
                    resource_names.emplace(output_path);
                }

                pack_resource_names.emplace(exclude_all_ext(output_path));

                const string path_in_pack = strex(pack_name).combinePath(output_path);

                if (!settings.ForceBaking && DiskFileSystem::GetWriteTime(thiz.MakeOutputPath(path_in_pack)) >= file_header.GetWriteTime()) {
                    continue;
                }

                auto file = files_to_bake.GetCurFile();

                auto res_file = DiskFileSystem::OpenFile(thiz.MakeOutputPath(path_in_pack), true);
                RUNTIME_ASSERT(res_file);
                auto res_file_write_ok = res_file.Write(file.GetData());
                RUNTIME_ASSERT(res_file_write_ok);

                ++baked_files;
            }

            // Delete outdated
            DiskFileSystem::IterateDir(thiz.MakeOutputPath(pack_name), "", true, [&](string_view path, size_t size, uint64 write_time) {
                UNUSED_VARIABLE(size);
                UNUSED_VARIABLE(write_time);

                if (pack_resource_names.count(exclude_all_ext(path)) == 0) {
                    const string path_in_pack = strex(pack_name).combinePath(path);
                    DiskFileSystem::DeleteFile(thiz.MakeOutputPath(path_in_pack));
                    WriteLog("Delete outdated file {}", path_in_pack);
                }
            });

            WriteLog("Baking of {} complete, baked {} file{}", pack_name, baked_files, baked_files != 1 ? "s" : "");
        };

        vector<std::future<void>> res_bakings;

        for (auto&& res_pack : res_packs) {
            WriteLog("Bake {}", res_pack.first);
            auto res_baking = std::async(async_mode, [&bake_resource_pack, res_pack] { bake_resource_pack(res_pack.first, res_pack.second); });
            res_bakings.emplace_back(std::move(res_baking));
        }

        for (auto&& res_baking : res_bakings) {
            try {
                res_baking.get();
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
                errors++;
            }
            catch (...) {
                UNKNOWN_EXCEPTION();
            }
        }

        WriteLog("Bake resources packs complete");
    }
    catch (const std::exception& ex) {
        WriteLog("Resource baking failed");
        ReportExceptionAndContinue(ex);
        errors++;
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }

    // Content
    try {
        WriteLog("Bake content");

        RUNTIME_ASSERT(!_settings.BakeContentEntries.empty());

        auto baker_engine = BakerEngine(PropertiesRelationType::BothRelative);

        for (const auto& dir : _settings.BakeContentEntries) {
            WriteLog("Add content entry {}", dir);
            baker_engine.Resources.AddDataSource(dir, DataSourceType::DirRoot);
        }

        // AngelScript scripts
#if FO_ANGELSCRIPT_SCRIPTING
        auto as_server_recompiled = false;

        try {
            WriteLog("Compile AngelScript scripts");

            auto all_scripts_up_to_date = true;

            if (_settings.ForceBaking) {
                all_scripts_up_to_date = false;
            }
            else {
                auto script_files = baker_engine.Resources.FilterFiles("fos");

                while (script_files.MoveNext() && all_scripts_up_to_date) {
                    auto file = script_files.GetCurFileHeader();
#if !FO_SINGLEPLAYER
                    if (DiskFileSystem::GetWriteTime(MakeOutputPath(strex("ServerAngelScript/ServerRootModule.fosb", file.GetName()))) <= file.GetWriteTime()) {
                        all_scripts_up_to_date = false;
                    }
                    if (DiskFileSystem::GetWriteTime(MakeOutputPath(strex("ServerAngelScript/ClientRootModule.fosb", file.GetName()))) <= file.GetWriteTime()) {
                        all_scripts_up_to_date = false;
                    }
#else
                    if (DiskFileSystem::GetWriteTime(MakeOutputPath(strex("AngelScript/RootModule.fosb", file.GetName()))) <= file.GetWriteTime()) {
                        all_scripts_up_to_date = false;
                    }
#endif
                    if (DiskFileSystem::GetWriteTime(MakeOutputPath(strex("MapperAngelScript/MapperRootModule.fosb", file.GetName()))) <= file.GetWriteTime()) {
                        all_scripts_up_to_date = false;
                    }
                }
            }

            if (all_scripts_up_to_date) {
                WriteLog("Scripts up to date, skip");
            }

#if !FO_SINGLEPLAYER
            if (!all_scripts_up_to_date) {
                WriteLog("Compile server scripts");
                AngelScriptCompiler_ServerScriptSystem().InitAngelScriptScripting(baker_engine.Resources);
                as_server_recompiled = true;
            }

            if (!all_scripts_up_to_date) {
                WriteLog("Compile client scripts");
                AngelScriptCompiler_ClientScriptSystem().InitAngelScriptScripting(baker_engine.Resources);
            }
#else
            if (!all_scripts_up_to_date) {
                WriteLog("Compile game scripts");
                AngelScriptCompiler_SingleScriptSystem().InitAngelScriptScripting(baker_engine.Resources);
            }
#endif
            if (!all_scripts_up_to_date) {
                WriteLog("Compile mapper scripts");
                AngelScriptCompiler_MapperScriptSystem().InitAngelScriptScripting(baker_engine.Resources);
            }

            WriteLog("Compile AngelScript scripts complete");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }
#endif

        // Validation engine
        FOEngineBase* validation_engine = nullptr;
#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
        AngelScriptCompiler_ServerScriptSystem_Validation compiler_script_sys;
#else
        AngelScriptCompiler_SingleScriptSystem_Validation compiler_script_sys;
#endif
#endif

        try {
            WriteLog("Compile AngelScript scripts");

#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
            compiler_script_sys.InitAngelScriptScripting(baker_engine.Resources, &validation_engine);
#else
            compiler_script_sys.InitAngelScriptScripting(baker_engine.Resources, &validation_engine);
#endif
#endif
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }

        RUNTIME_ASSERT(validation_engine);
        const ScriptSystem& script_sys = compiler_script_sys;

        // Configs
        try {
            WriteLog("Bake configs");

            if (_settings.ForceBaking) {
                auto del_configs_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Configs"));
                RUNTIME_ASSERT(del_configs_ok);
            }

            auto configs = baker_engine.Resources.FilterFiles("focfg");
            bool all_configs_up_to_date = true;

            while (configs.MoveNext() && all_configs_up_to_date) {
                auto file = configs.GetCurFileHeader();
                if (DiskFileSystem::GetWriteTime(MakeOutputPath(strex("Configs/{}.focfg", file.GetName()))) <= file.GetWriteTime()) {
                    all_configs_up_to_date = false;
                }
#if !FO_SINGLEPLAYER
                if (DiskFileSystem::GetWriteTime(MakeOutputPath(strex("Configs/Client_{}.focfg", file.GetName()))) <= file.GetWriteTime()) {
                    all_configs_up_to_date = false;
                }
#endif
            }

            if (!all_configs_up_to_date) {
                configs.ResetCounter();
                while (configs.MoveNext()) {
                    auto file = configs.GetCurFile();

                    WriteLog("Process config {}", file.GetName());

                    int settings_errors = 0;

                    // Collect content from end to beginning
                    string final_content;

                    {
                        auto focfg = ConfigFile(file.GetPath(), file.GetStr());
                        final_content = file.GetStr();

                        auto parent_name = focfg.GetStr("", "$Parent");

                        while (!parent_name.empty()) {
                            const auto parent_file = configs.FindFileByName(parent_name);
                            if (!parent_file) {
                                WriteLog("Parent config {} not found", parent_name);
                                settings_errors++;
                                break;
                            }

                            final_content.insert(0, parent_file.GetStr());
                            final_content.insert(0, "\n");

                            auto parent_focfg = ConfigFile(parent_file.GetPath(), parent_file.GetStr());
                            parent_name = parent_focfg.GetStr("", "$Parent");
                        }
                    }

                    auto focfg = ConfigFile(file.GetPath(), final_content);

                    string config_content;
#if !FO_SINGLEPLAYER
                    extern auto GetServerSettings() -> unordered_set<string>;
                    extern auto GetClientSettings() -> unordered_set<string>;
                    auto server_settings = GetServerSettings();
                    auto client_settings = GetClientSettings();
                    string client_config_content;
#endif

                    for (auto&& [key, value] : focfg.GetSection("")) {
                        if (key.front() == '$') {
                            continue;
                        }

                        string resolved_value;

                        if (value.empty()) {
                            resolved_value = "";
                        }
                        else if (strex(value).isNumber()) {
                            resolved_value = value;
                        }
                        else if (strex(value).isExplicitBool()) {
                            resolved_value = strex(value).toBool() ? "1" : "0";
                        }
                        else if (value.find("::") != string::npos) {
                            bool failed = false;
                            resolved_value = strex("{}", baker_engine.ResolveEnumValue(value, &failed));
                            if (failed) {
                                settings_errors++;
                            }
                        }
                        else {
                            resolved_value = value;
                        }

                        config_content += strex("{}={}\n", key, resolved_value);

#if !FO_SINGLEPLAYER
                        const auto is_server_setting = server_settings.count(key) != 0;
                        const auto is_client_setting = client_settings.count(key) != 0;
                        if (is_server_setting) {
                            server_settings.erase(key);
                        }
                        if (is_client_setting) {
                            client_config_content += strex("{}={}\n", key, resolved_value);
                            client_settings.erase(key);
                        }
                        if (!is_server_setting && !is_client_setting) {
                            WriteLog("Unknown setting {} = {}", key, resolved_value);
                            settings_errors++;
                        }
#endif
                    }

                    if (settings_errors != 0) {
                        throw GenericException("Config errors");
                    }

                    const auto write_config = [this](string_view cfg_name1, string_view cfg_name2, string_view cfg_content) {
                        auto cfg_file = DiskFileSystem::OpenFile(MakeOutputPath(strex("Configs/{}{}.focfg", cfg_name1, cfg_name2)), true);
                        RUNTIME_ASSERT(cfg_file);
                        const auto cfg_file_write_ok = cfg_file.Write(cfg_content);
                        RUNTIME_ASSERT(cfg_file_write_ok);
                    };

                    write_config("", file.GetName(), config_content);
#if !FO_SINGLEPLAYER
                    write_config("Client_", file.GetName(), client_config_content);
#endif
                }
            }

            WriteLog("Bake configs complete");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }

        // Protos
        auto proto_mngr = ProtoManager(&baker_engine);

        try {
            WriteLog("Bake protos");

            auto parse_protos = false;

            if (_settings.ForceBaking) {
                auto del_protos_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Protos"));
                RUNTIME_ASSERT(del_protos_ok);

                parse_protos = true;
            }

#if FO_ANGELSCRIPT_SCRIPTING
            if (as_server_recompiled) {
                parse_protos = true;
            }
#endif

            if (!parse_protos) {
#if !FO_SINGLEPLAYER
                const auto last_write_time = DiskFileSystem::GetWriteTime(MakeOutputPath("FullProtos/FullProtos.foprob"));
#else
                const auto last_write_time = DiskFileSystem::GetWriteTime(MakeOutputPath("Protos/Protos.foprob"));
#endif
                if (last_write_time > 0) {
                    const auto check_up_to_date = [last_write_time, &resources = baker_engine.Resources](string_view ext) -> bool {
                        auto files = resources.FilterFiles(ext);

                        while (files.MoveNext()) {
                            auto file = files.GetCurFileHeader();

                            if (file.GetWriteTime() > last_write_time) {
                                return false;
                            }
                        }

                        return true;
                    };

                    if (check_up_to_date("foitem") && check_up_to_date("focr") && check_up_to_date("fomap") && check_up_to_date("foloc") && check_up_to_date("fopro")) {
                        WriteLog("Protos up to date, skip");
                    }
                    else {
                        parse_protos = true;
                    }
                }
                else {
                    parse_protos = true;
                }
            }

            if (parse_protos) {
                WriteLog("Parse protos");
                proto_mngr.ParseProtos(baker_engine.Resources);

                // Protos validation
                unordered_set<hstring> resource_hashes;

                for (const auto& name : resource_names) {
                    resource_hashes.insert(baker_engine.ToHashedString(name));
                }

                WriteLog("Validate protos");

                int proto_errors = 0;

                for (auto&& [type_name, protos] : proto_mngr.GetAllProtos()) {
                    for (auto&& [pid, proto] : protos) {
                        proto_errors += ValidateProperties(proto->GetProperties(), strex("proto {} {}", type_name, proto->GetName()), script_sys, resource_hashes);
                    }
                }

                // Check maps for locations
                {
                    const auto& loc_protos = proto_mngr.GetProtoLocations();
                    const auto& map_protos = proto_mngr.GetProtoMaps();

                    for (auto&& [pid, proto] : loc_protos) {
                        for (auto map_pid : proto->GetMapProtos()) {
                            if (map_protos.count(map_pid) == 0) {
                                WriteLog("Proto map {} not found for proto location {}", map_pid, proto->GetName());
                                proto_errors++;
                            }
                        }
                    }
                }

                if (proto_errors != 0) {
                    throw ProtoValidationException("Proto verification failed");
                }

                WriteLog("Process server protos");
                auto server_engine = BakerEngine(PropertiesRelationType::ServerRelative);
                auto server_proto_mngr = ProtoManager(&server_engine);

                WriteLog("Process client protos");
                auto client_engine = BakerEngine(PropertiesRelationType::ClientRelative);
                auto client_proto_mngr = ProtoManager(&client_engine);

                vector<std::future<void>> proto_bakings;

                proto_bakings.emplace_back(std::async(async_mode, [&] { server_proto_mngr.ParseProtos(baker_engine.Resources); }));
                proto_bakings.emplace_back(std::async(async_mode, [&] { client_proto_mngr.ParseProtos(baker_engine.Resources); }));

                for (auto&& proto_baking : proto_bakings) {
                    proto_baking.get();
                }

                // Maps
                WriteLog("Process maps");

                if (_settings.ForceBaking) {
                    auto del_maps_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Maps"));
                    RUNTIME_ASSERT(del_maps_ok);
                    del_maps_ok = DiskFileSystem::DeleteDir(MakeOutputPath("StaticMaps"));
                    RUNTIME_ASSERT(del_maps_ok);
                }

                const auto bake_map = [ // clang-format off
                    &thiz = std::as_const(*this),
                    &settings = std::as_const(_settings),
                    &baker_engine = std::as_const(baker_engine),
                    &server_proto_mngr = std::as_const(server_proto_mngr),
                    &client_proto_mngr = std::as_const(client_proto_mngr),
                    &hash_resolver = static_cast<HashResolver&>(server_engine),
                    &script_sys = std::as_const(compiler_script_sys),
                    &resource_hashes = std::as_const(resource_hashes) // clang-format on
                ](const ProtoMap* proto_map) {
                    const auto fomap_files = baker_engine.Resources.FilterFiles("fomap");
                    auto map_file = fomap_files.FindFileByName(proto_map->GetName());
                    RUNTIME_ASSERT(map_file);

                    // Skip if up to date
                    if (!settings.ForceBaking) {
                        if (DiskFileSystem::GetWriteTime(thiz.MakeOutputPath(strex("Maps/{}.fomapb", proto_map->GetName()))) > map_file.GetWriteTime()) {
                            return;
                        }
                    }

                    vector<uint8> props_data;
                    uint map_cr_count = 0;
                    uint map_item_count = 0;
                    uint map_client_item_count = 0;
                    vector<uint8> map_cr_data;
                    vector<uint8> map_item_data;
                    vector<uint8> map_client_item_data;
                    auto map_cr_data_writer = DataWriter(map_cr_data);
                    auto map_item_data_writer = DataWriter(map_item_data);
                    auto map_client_item_data_writer = DataWriter(map_client_item_data);
                    set<hstring> str_hashes;
                    set<hstring> client_str_hashes;
                    int map_errors = 0;

                    MapLoader::Load(
                        proto_map->GetName(), map_file.GetStr(), server_proto_mngr, hash_resolver,
                        [&](ident_t id, const ProtoCritter* proto, const map<string, string>& kv) {
                            auto props = copy(proto->GetProperties());
                            props.ApplyFromText(kv);

                            map_errors += thiz.ValidateProperties(props, strex("map {} critter {} with id {}", proto_map->GetName(), proto->GetName(), id), script_sys, resource_hashes);

                            map_cr_count++;
                            map_cr_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                            map_cr_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());
                            props.StoreAllData(props_data, str_hashes);
                            map_cr_data_writer.Write<uint>(static_cast<uint>(props_data.size()));
                            map_cr_data_writer.WritePtr(props_data.data(), props_data.size());
                        },
                        [&](ident_t id, const ProtoItem* proto, const map<string, string>& kv) {
                            auto props = copy(proto->GetProperties());
                            props.ApplyFromText(kv);

                            map_errors += thiz.ValidateProperties(props, strex("map {} item {} with id {}", proto_map->GetName(), proto->GetName(), id), script_sys, resource_hashes);

                            map_item_count++;
                            map_item_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                            map_item_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());
                            props.StoreAllData(props_data, str_hashes);
                            map_item_data_writer.Write<uint>(static_cast<uint>(props_data.size()));
                            map_item_data_writer.WritePtr(props_data.data(), props_data.size());

                            const auto is_static = proto->GetStatic();
                            const auto is_hidden = proto->GetHidden();

                            if (is_static && !is_hidden) {
                                const auto* client_proto = client_proto_mngr.GetProtoItem(proto->GetProtoId());
                                auto client_props = copy(client_proto->GetProperties());
                                client_props.ApplyFromText(kv);

                                map_client_item_count++;
                                map_client_item_data_writer.Write<ident_t::underlying_type>(id.underlying_value());
                                map_client_item_data_writer.Write<hstring::hash_t>(client_proto->GetProtoId().as_hash());
                                client_props.StoreAllData(props_data, client_str_hashes);
                                map_client_item_data_writer.Write<uint>(static_cast<uint>(props_data.size()));
                                map_client_item_data_writer.WritePtr(props_data.data(), props_data.size());
                            }
                        });

                    if (map_errors != 0) {
                        throw GenericException("Map validation error(s)");
                    }

#if !FO_SINGLEPLAYER
                    // Server side
                    {
                        vector<uint8> map_data;
                        auto final_writer = DataWriter(map_data);
                        final_writer.Write<uint>(static_cast<uint>(str_hashes.size()));
                        for (const auto& hstr : str_hashes) {
                            const auto& str = hstr.as_str();
                            final_writer.Write<uint>(static_cast<uint>(str.length()));
                            final_writer.WritePtr(str.c_str(), str.length());
                        }
                        final_writer.Write<uint>(map_cr_count);
                        final_writer.WritePtr(map_cr_data.data(), map_cr_data.size());
                        final_writer.Write<uint>(map_item_count);
                        final_writer.WritePtr(map_item_data.data(), map_item_data.size());

                        auto map_bin_file = DiskFileSystem::OpenFile(thiz.MakeOutputPath(strex("Maps/{}.fomapb", proto_map->GetName())), true);
                        RUNTIME_ASSERT(map_bin_file);
                        auto map_bin_file_write_ok = map_bin_file.Write(map_data);
                        RUNTIME_ASSERT(map_bin_file_write_ok);
                    }
                    // Client side
                    {
                        vector<uint8> map_data;
                        auto final_writer = DataWriter(map_data);
                        final_writer.Write<uint>(static_cast<uint>(client_str_hashes.size()));
                        for (const auto& hstr : client_str_hashes) {
                            const auto& str = hstr.as_str();
                            final_writer.Write<uint>(static_cast<uint>(str.length()));
                            final_writer.WritePtr(str.c_str(), str.length());
                        }
                        final_writer.Write<uint>(map_client_item_count);
                        final_writer.WritePtr(map_client_item_data.data(), map_client_item_data.size());

                        auto map_bin_file = DiskFileSystem::OpenFile(thiz.MakeOutputPath(strex("StaticMaps/{}.fomapb2", proto_map->GetName())), true);
                        RUNTIME_ASSERT(map_bin_file);
                        auto map_bin_file_write_ok = map_bin_file.Write(map_data);
                        RUNTIME_ASSERT(map_bin_file_write_ok);
                    }
#else
                    vector<uint8> map_data;
                    auto final_writer = DataWriter(map_data);
                    final_writer.Write<uint>(map_cr_count);
                    final_writer.WritePtr(map_cr_data.data(), map_cr_data.size());
                    final_writer.Write<uint>(map_item_count);
                    final_writer.WritePtr(map_item_data.data(), map_item_data.size());

                    auto map_bin_file = DiskFileSystem::OpenFile(thiz.MakeOutputPath(strex("Maps/{}.fomapb", proto_map->GetName())), true);
                    RUNTIME_ASSERT(map_bin_file);
                    auto map_bin_file_write_ok = map_bin_file.Write(map_data);
                    RUNTIME_ASSERT(map_bin_file_write_ok);
#endif
                };

                vector<std::future<void>> map_bakings;

                for (const auto& proto_entry : proto_mngr.GetProtoMaps()) {
                    auto map_baking = std::async(async_mode, [&] { bake_map(proto_entry.second); });
                    map_bakings.emplace_back(std::move(map_baking));
                }

                for (auto&& map_baking : map_bakings) {
                    try {
                        map_baking.get();
                    }
                    catch (const std::exception& ex) {
                        ReportExceptionAndContinue(ex);
                        proto_errors++;
                    }
                    catch (...) {
                        UNKNOWN_EXCEPTION();
                    }
                }

                if (proto_errors != 0) {
                    throw GenericException("Proto maps verification failed");
                }

                // Write protos
                const auto write_protos = [this](const ProtoManager& protos, string_view fname) {
                    const auto data = protos.GetProtosBinaryData();
                    RUNTIME_ASSERT(!data.empty());
                    auto protos_file = DiskFileSystem::OpenFile(MakeOutputPath(fname), true);
                    RUNTIME_ASSERT(protos_file);
                    const auto protos_write_ok = protos_file.Write(data.data(), data.size());
                    RUNTIME_ASSERT(protos_write_ok);
                };

                WriteLog("Write protos");
#if !FO_SINGLEPLAYER
                write_protos(proto_mngr, "FullProtos/FullProtos.foprob");
                write_protos(server_proto_mngr, "ServerProtos/ServerProtos.foprob");
                write_protos(client_proto_mngr, "ClientProtos/ClientProtos.foprob");
#else
                write_protos(proto_mngr, "Protos/Protos.foprob");
#endif
            }

            WriteLog("Bake protos complete");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }

        // Dialogs
        auto dialog_mngr = DialogManager(&baker_engine);

        try {
            WriteLog("Bake dialogs");

            if (_settings.ForceBaking) {
                auto del_dialogs_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Dialogs"));
                RUNTIME_ASSERT(del_dialogs_ok);
            }

            dialog_mngr.LoadFromResources();

            int dlg_errors = 0;

            for (auto* dlg_pack : dialog_mngr.GetDialogs()) {
                for (auto&& dlg : dlg_pack->Dialogs) {
                    if (dlg.DlgScriptFuncName) {
                        if (!script_sys.CheckFunc<void, Critter*, Critter*, string*>(dlg.DlgScriptFuncName) && //
                            !script_sys.CheckFunc<uint, Critter*, Critter*, string*>(dlg.DlgScriptFuncName)) {
                            WriteLog("Dialog {} invalid start function {}", dlg_pack->PackName, dlg.DlgScriptFuncName);
                            dlg_errors++;
                        }
                    }

                    for (auto&& answer : dlg.Answers) {
                        for (auto&& demand : answer.Demands) {
                            if (demand.Type == DR_SCRIPT) {
                                if ((demand.ValuesCount == 0 && !script_sys.CheckFunc<bool, Critter*, Critter*>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 1 && !script_sys.CheckFunc<bool, Critter*, Critter*, int>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 2 && !script_sys.CheckFunc<bool, Critter*, Critter*, int, int>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 3 && !script_sys.CheckFunc<bool, Critter*, Critter*, int, int, int>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 4 && !script_sys.CheckFunc<bool, Critter*, Critter*, int, int, int, int>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 5 && !script_sys.CheckFunc<bool, Critter*, Critter*, int, int, int, int, int>(demand.AnswerScriptFuncName))) {
                                    WriteLog("Dialog {} answer demand invalid function {}", dlg_pack->PackName, demand.AnswerScriptFuncName);
                                    dlg_errors++;
                                }
                            }
                        }

                        for (auto&& result : answer.Results) {
                            if (result.Type == DR_SCRIPT) {
                                int not_found_count = 0;

                                if ((result.ValuesCount == 0 && !script_sys.CheckFunc<void, Critter*, Critter*>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 1 && !script_sys.CheckFunc<void, Critter*, Critter*, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 2 && !script_sys.CheckFunc<void, Critter*, Critter*, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 3 && !script_sys.CheckFunc<void, Critter*, Critter*, int, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 4 && !script_sys.CheckFunc<void, Critter*, Critter*, int, int, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 5 && !script_sys.CheckFunc<void, Critter*, Critter*, int, int, int, int, int>(result.AnswerScriptFuncName))) {
                                    not_found_count++;
                                }

                                if ((result.ValuesCount == 0 && !script_sys.CheckFunc<uint, Critter*, Critter*>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 1 && !script_sys.CheckFunc<uint, Critter*, Critter*, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 2 && !script_sys.CheckFunc<uint, Critter*, Critter*, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 3 && !script_sys.CheckFunc<uint, Critter*, Critter*, int, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 4 && !script_sys.CheckFunc<uint, Critter*, Critter*, int, int, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 5 && !script_sys.CheckFunc<uint, Critter*, Critter*, int, int, int, int, int>(result.AnswerScriptFuncName))) {
                                    not_found_count++;
                                }

                                if (not_found_count != 1) {
                                    WriteLog("Dialog {} answer result invalid function {}", dlg_pack->PackName, result.AnswerScriptFuncName);
                                    dlg_errors++;
                                }
                            }
                        }
                    }
                }
            }

            if (dlg_errors == 0) {
                auto dialogs = baker_engine.Resources.FilterFiles("fodlg");
                WriteLog("Dialogs count {}", dialogs.GetFilesCount());

                while (dialogs.MoveNext()) {
                    auto file = dialogs.GetCurFile();
                    auto dlg_file = DiskFileSystem::OpenFile(MakeOutputPath(strex("Dialogs/{}.fodlg", file.GetName())), true);
                    RUNTIME_ASSERT(dlg_file);
                    auto dlg_file_write_ok = dlg_file.Write(file.GetBuf(), file.GetSize());
                    RUNTIME_ASSERT(dlg_file_write_ok);
                }

                WriteLog("Bake dialogs complete");
            }
            else {
                errors++;
            }
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }

        // Texts
        try {
            WriteLog("Bake texts");

            auto del_texts_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Texts"));
            RUNTIME_ASSERT(del_texts_ok);

            set<string> languages;
            auto txt_files = baker_engine.Resources.FilterFiles("fotxt");
            while (txt_files.MoveNext()) {
                const auto file = txt_files.GetCurFileHeader();
                const auto& file_name = file.GetName();

                const auto sep = file_name.find('.');
                RUNTIME_ASSERT(sep != string::npos);

                string lang_name = file_name.substr(sep + 1);
                RUNTIME_ASSERT(!lang_name.empty());

                if (languages.emplace(lang_name).second) {
                    WriteLog("Language: {}", lang_name);
                }
            }

            vector<LanguagePack> lang_packs;

            for (const auto& lang_name : languages) {
                auto lang_pack = LanguagePack {lang_name, baker_engine};
                lang_pack.ParseTexts(baker_engine.Resources, baker_engine);
                lang_packs.emplace_back(std::move(lang_pack));
            }

            // Dialog texts
            for (auto* pack : dialog_mngr.GetDialogs()) {
                for (size_t i = 0; i < pack->Texts.size(); i++) {
                    for (auto& lang : lang_packs) {
                        if (pack->Texts[i].first != lang.GetName()) {
                            continue;
                        }

                        auto& text_pack = lang.GetTextPackForEdit(TextPackName::Dialogs);

                        if (text_pack.CheckIntersections(pack->Texts[i].second)) {
                            throw GenericException("Dialog text intersection detected", pack->PackName);
                        }

                        text_pack.Merge(pack->Texts[i].second);
                    }
                }
            }

            // Proto texts
            bool text_intersected = false;

            const auto fill_proto_texts = [&parsed_texts = proto_mngr.GetParsedTexts(), &lang_packs, &text_intersected](const auto& protos, TextPackName pack_name) {
                for (auto&& [pid, proto] : protos) {
                    for (const auto& proto_text : parsed_texts.at(proto->GetTypeName()).at(pid)) {
                        const auto lang_predicate = [&lang_name = proto_text.first](const LanguagePack& lang) { return lang.GetName() == lang_name; };
                        const auto it_lang = std::find_if(lang_packs.begin(), lang_packs.end(), lang_predicate);

                        if (it_lang != lang_packs.end()) {
                            auto& text_pack = it_lang->GetTextPackForEdit(pack_name);

                            if (text_pack.CheckIntersections(proto_text.second)) {
                                WriteLog("Proto text intersection detected for proto {} and pack {}", proto->GetName(), pack_name);
                                text_intersected = true;
                            }

                            text_pack.Merge(proto_text.second);
                        }
                        else {
                            throw GenericException("Unsupported language in proto", proto->GetName(), proto_text.first);
                        }
                    }
                }
            };

            fill_proto_texts(proto_mngr.GetProtoCritters(), TextPackName::Dialogs);
            fill_proto_texts(proto_mngr.GetProtoItems(), TextPackName::Items);
            fill_proto_texts(proto_mngr.GetProtoMaps(), TextPackName::Maps);
            fill_proto_texts(proto_mngr.GetProtoLocations(), TextPackName::Locations);

            for (auto&& [type_name, entity_info] : baker_engine.GetEntityTypesInfo()) {
                if (!entity_info.Exported && entity_info.HasProtos) {
                    fill_proto_texts(proto_mngr.GetProtoEntities(type_name), TextPackName::Protos);
                }
            }

            if (text_intersected) {
                throw GenericException("Proto text intersection detected");
            }

            // Save parsed packs
            for (const auto& lang_pack : lang_packs) {
                lang_pack.SaveTextsToDisk(MakeOutputPath("Texts"));
            }

            WriteLog("Bake texts complete");

            // Engine restore info
            WriteLog("Bake engine restore info");

            {
                const auto restore_info_bin = BakerEngine::GetRestoreInfo();
                const auto del_restore_info_ok = DiskFileSystem::DeleteDir(MakeOutputPath("EngineData"));
                RUNTIME_ASSERT(del_restore_info_ok);
                auto restore_info_file = DiskFileSystem::OpenFile(MakeOutputPath("EngineData/RestoreInfo.fobin"), true);
                RUNTIME_ASSERT(restore_info_file);
                const auto restore_info_file_write_ok = restore_info_file.Write(restore_info_bin);
                RUNTIME_ASSERT(restore_info_file_write_ok);
            }

            WriteLog("Bake engine restore info complete");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
        catch (...) {
            UNKNOWN_EXCEPTION();
        }

        WriteLog("Bake content complete");
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        errors++;
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }

    WriteLog("Time {}", bake_time.GetDuration());

    // Finalize
    if (errors != 0) {
        WriteLog("Baking failed!");
        ExitApp(false);
    }

    WriteLog("Baking complete!");

    {
        auto build_hash_file = DiskFileSystem::OpenFile(MakeOutputPath("Resources.build-hash"), true, true);
        RUNTIME_ASSERT(build_hash_file);
        const auto build_hash_writed = build_hash_file.Write(FO_BUILD_HASH);
        RUNTIME_ASSERT(build_hash_writed);
    }
}

static unordered_map<string, std::function<bool(hstring, const ScriptSystem&)>> ScriptFuncVerify = {
    {"ItemInit", [](hstring func_name, const ScriptSystem& script_sys) { return script_sys.CheckFunc<void, Item*, bool>(func_name); }},
    {"ItemStatic", [](hstring func_name, const ScriptSystem& script_sys) { return script_sys.CheckFunc<bool, Critter*, StaticItem*, Item*, any_t>(func_name); }},
    {"ItemTrigger", [](hstring func_name, const ScriptSystem& script_sys) { return script_sys.CheckFunc<void, Critter*, StaticItem*, bool, uint8>(func_name); }},
    {"CritterInit", [](hstring func_name, const ScriptSystem& script_sys) { return script_sys.CheckFunc<void, Critter*, bool>(func_name); }},
    {"MapInit", [](hstring func_name, const ScriptSystem& script_sys) { return script_sys.CheckFunc<void, Map*, bool>(func_name); }},
    {"LocationInit", [](hstring func_name, const ScriptSystem& script_sys) { return script_sys.CheckFunc<void, Location*, bool>(func_name); }},
};

auto Baker::ValidateProperties(const Properties& props, string_view context_str, const ScriptSystem& script_sys, const unordered_set<hstring>& resource_hashes) const -> int
{
    STACK_TRACE_ENTRY();

    int errors = 0;

    const auto* registrator = props.GetRegistrator();

    for (size_t i = 0; i < registrator->GetPropertiesCount(); i++) {
        const auto* prop = registrator->GetPropertyByIndexUnsafe(i);

        if (prop->IsBaseTypeResource()) {
            if (prop->IsPlainData()) {
                const auto hash_data = props.GetRawData(prop);
                RUNTIME_ASSERT(hash_data.size() == sizeof(hstring::hash_t));

                if (*reinterpret_cast<const hstring::hash_t*>(hash_data.data()) == 0) {
                    continue;
                }

                const auto h = props.GetValue<hstring>(prop);
                if (h && resource_hashes.count(h) == 0) {
                    WriteLog("Resource {} not found for property {} in {}", h, prop->GetName(), context_str);
                    errors++;
                }
            }
            else if (prop->IsArray()) {
                if (props.GetRawDataSize(prop) == 0) {
                    continue;
                }

                const auto hashes = props.GetValue<vector<hstring>>(prop);
                for (const auto h : hashes) {
                    if (h && resource_hashes.count(h) == 0) {
                        WriteLog("Resource {} not found for property {} in {}", h, prop->GetName(), context_str);
                        errors++;
                    }
                }
            }
            else {
                WriteLog("Resource {} can be as standalone or in array in {}", prop->GetName(), context_str);
                errors++;
            }
        }

        if (!prop->GetBaseScriptFuncType().empty()) {
            if (prop->IsPlainData()) {
                auto hash_data = props.GetRawData(prop);
                RUNTIME_ASSERT(hash_data.size() == sizeof(hstring::hash_t));

                if (*reinterpret_cast<const hstring::hash_t*>(hash_data.data()) == 0) {
                    continue;
                }

                const auto func_name = props.GetValue<hstring>(prop);
                if (ScriptFuncVerify.count(prop->GetBaseScriptFuncType()) == 0) {
                    WriteLog("Invalid script func {} of type {} for property {} in {}", func_name, prop->GetBaseScriptFuncType(), prop->GetName(), context_str);
                    errors++;
                }
                else if (func_name && !ScriptFuncVerify[prop->GetBaseScriptFuncType()](func_name, script_sys)) {
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

    _bakers = BaseBaker::SetupBakers(_settings, nullptr, std::bind(&BakerDataSource::WriteData, this, std::placeholders::_1, std::placeholders::_2));
}

void BakerDataSource::WriteData(string_view baked_path, const_span<uint8> baked_data)
{
    STACK_TRACE_ENTRY();

    auto baked_file = File(strex(baked_path).extractFileName().eraseFileExtension(), baked_path, 0, this, baked_data, true);
    _bakedFiles[string(baked_path)] = std::make_unique<File>(std::move(baked_file));
}

auto BakerDataSource::FindFile(const string& path) const -> File*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _bakedFiles.find(path); it != _bakedFiles.end()) {
        return it->second ? it->second.get() : nullptr;
    }

    bool file_baked = false;

    if (auto file = _inputResources.ReadFile(path)) {
        const string ext = strex(path).getFileExtension();

        for (auto&& baker : _bakers) {
            if (baker->IsExtSupported(ext)) {
                baker->BakeFiles(FileCollection({file.Duplicate()}));
                file_baked = true;
            }
        }

        if (!file_baked) {
            _bakedFiles[path] = std::make_unique<File>(std::move(file));
        }
    }

    if (!file_baked) {
        _bakedFiles[path] = nullptr;
    }

    if (const auto it = _bakedFiles.find(path); it != _bakedFiles.end()) {
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

    if (const auto* file = FindFile(string(path)); file != nullptr) {
        size = file->GetSize();
        write_time = file->GetWriteTime();
        return true;
    }

    return false;
}

auto BakerDataSource::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8>
{
    STACK_TRACE_ENTRY();

    if (const auto* file = FindFile(string(path)); file != nullptr) {
        size = file->GetSize();
        write_time = file->GetWriteTime();
        return {file->GetBuf(), [](auto* p) { UNUSED_VARIABLE(p); }};
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
