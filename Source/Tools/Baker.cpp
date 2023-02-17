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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Timer.h"
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

    static auto GetRestoreInfo() -> vector<uchar>
    {
        extern auto Baker_GetRestoreInfo()->vector<uchar>;
        return Baker_GetRestoreInfo();
    }

private:
    static GlobalSettings _dummySettings;
};
GlobalSettings BakerEngine::_dummySettings {};

// Implementation in AngelScriptScripting-*Compiler.cpp
#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
struct ASCompiler_ServerScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(FileSystem& resources);
};
struct ASCompiler_ServerScriptSystem_Validation : public ScriptSystem
{
    void InitAngelScriptScripting(FileSystem& resources, FOEngineBase** out_engine);
};
struct ASCompiler_ClientScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(FileSystem& resources);
};
#else
struct ASCompiler_SingleScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(FileSystem& resources);
};
struct ASCompiler_SingleScriptSystem_Validation : public ScriptSystem
{
    void InitAngelScriptScripting(FileSystem& resources, FOEngineBase** out_engine);
};
#endif
struct ASCompiler_MapperScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(FileSystem& resources);
};

// External variable for compiler messages
unordered_set<string> CompilerPassedMessages;
#endif

struct Item;
struct StaticItem;
struct Critter;
struct Map;
struct Location;

BaseBaker::BaseBaker(BakerSettings& settings, FileCollection&& files, BakeCheckerCallback&& bake_checker, WriteDataCallback&& write_data) :
    _settings {settings}, //
    _files {std::move(files)},
    _bakeChecker {std::move(bake_checker)},
    _writeData {std::move(write_data)}
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(_writeData);
}

Baker::Baker(BakerSettings& settings) :
    _settings {settings}
{
    STACK_TRACE_ENTRY();
}

auto Baker::MakeOutputPath(string_view path) const -> string
{
    STACK_TRACE_ENTRY();

    return _str(_settings.BakeOutput).combinePath(path).str();
}

void Baker::BakeAll()
{
    STACK_TRACE_ENTRY();

    WriteLog("Start bakering");

    const auto start_time = Timer::RealtimeTick();

    const auto build_hash_deleted = DiskFileSystem::DeleteFile(MakeOutputPath("Resources.build-hash"));
    RUNTIME_ASSERT(build_hash_deleted);

    if (_settings.ForceBakering) {
        WriteLog("Force rebuild all resources");
    }

    auto errors = 0;

    // Resource packs
    auto resource_names = unordered_set<string>();

    try {
        WriteLog("Bake resource packs");

        RUNTIME_ASSERT(!_settings.BakeResourceEntries.empty());

        map<string, vector<string>> res_packs;
        for (const auto& re : _settings.BakeResourceEntries) {
            auto re_splitted = _str(re).split(',');
            RUNTIME_ASSERT(re_splitted.size() == 2);
            res_packs[re_splitted[0]].push_back(re_splitted[1]);
        }

        for (auto&& res_pack : res_packs) {
            const auto& pack_name = res_pack.first;
            const auto& paths = res_pack.second;

            try {
                WriteLog("Bake {}", pack_name);

                FileSystem res_files;
                for (const auto& path : paths) {
                    WriteLog("Add resource pack {} entry {}", pack_name, path);
                    res_files.AddDataSource(path);
                }

                // Cleanup previous
                if (_settings.ForceBakering) {
                    auto del_res_ok = DiskFileSystem::DeleteDir(MakeOutputPath(pack_name));
                    RUNTIME_ASSERT(del_res_ok);
                }

                DiskFileSystem::MakeDirTree(MakeOutputPath(pack_name));

                const auto is_raw_only = pack_name == "Raw";
                auto resources = res_files.GetAllFiles();
                size_t baked_files = 0;

                WriteLog("Create resource pack {} from {} files", pack_name, resources.GetFilesCount());

                // Bake files
                auto pack_resource_names = unordered_set<string>();

                const auto exclude_all_ext = [](string_view path) -> string {
                    size_t pos = path.rfind('/');
                    pos = path.find('.', pos != string::npos ? pos : 0);
                    return pos != string::npos ? string(path.substr(0, pos)) : string(path);
                };
                const auto make_output_path = [](string_view path) -> string {
                    if (_str(path).startsWith("art/critters/")) {
                        return _str("art/critters/{}", _str(path.substr("art/critters/"_len)).lower()).str();
                    }
                    return string(path);
                };

                if (!is_raw_only) {
                    const auto bake_checker = [&](const FileHeader& file_header) -> bool {
                        const auto output_path = make_output_path(file_header.GetPath());

                        resource_names.emplace(output_path);
                        pack_resource_names.emplace(exclude_all_ext(output_path));

                        if (!_settings.ForceBakering) {
                            return file_header.GetWriteTime() > DiskFileSystem::GetWriteTime(MakeOutputPath(_str(pack_name).combinePath(output_path)));
                        }
                        else {
                            return true;
                        }
                    };

                    const auto write_data = [&](string_view path, const_span<uchar> baked_data) {
                        const auto output_path = make_output_path(path);

                        auto res_file = DiskFileSystem::OpenFile(MakeOutputPath(_str(pack_name).combinePath(output_path)), true);
                        RUNTIME_ASSERT(res_file);
                        const auto res_file_write_ok = res_file.Write(baked_data);
                        RUNTIME_ASSERT(res_file_write_ok);

                        baked_files++;
                    };

                    vector<unique_ptr<BaseBaker>> bakers;
                    bakers.emplace_back(std::make_unique<ImageBaker>(_settings, res_files.GetAllFiles(), bake_checker, write_data));
                    bakers.emplace_back(std::make_unique<EffectBaker>(_settings, res_files.GetAllFiles(), bake_checker, write_data));
#if FO_ENABLE_3D
                    bakers.emplace_back(std::make_unique<ModelBaker>(_settings, res_files.GetAllFiles(), bake_checker, write_data));
#endif

                    for (auto&& baker : bakers) {
                        baker->AutoBake();
                    }
                }

                // Raw copy
                resources.ResetCounter();
                while (resources.MoveNext()) {
                    auto file_header = resources.GetCurFileHeader();
                    const auto output_path = make_output_path(file_header.GetPath());

                    // Skip not necessary files
                    if (!is_raw_only) {
                        const auto ext = _str(output_path).getFileExtension().str();
                        const auto& base_exts = _settings.BakeExtraFileExtensions;
                        if (std::find(base_exts.begin(), base_exts.end(), ext) == base_exts.end()) {
                            continue;
                        }
                    }

                    resource_names.emplace(output_path);
                    pack_resource_names.emplace(exclude_all_ext(output_path));

                    const auto path_in_pack = _str(pack_name).combinePath(output_path).str();

                    if (!_settings.ForceBakering && DiskFileSystem::GetWriteTime(MakeOutputPath(path_in_pack)) >= file_header.GetWriteTime()) {
                        continue;
                    }

                    auto file = resources.GetCurFile();

                    auto res_file = DiskFileSystem::OpenFile(MakeOutputPath(path_in_pack), true);
                    RUNTIME_ASSERT(res_file);
                    auto res_file_write_ok = res_file.Write(file.GetData());
                    RUNTIME_ASSERT(res_file_write_ok);
                }

                // Delete outdated
                DiskFileSystem::IterateDir(MakeOutputPath(pack_name), "", true, [this, &pack_name, &pack_resource_names, &exclude_all_ext](string_view path, size_t size, uint64 write_time) {
                    UNUSED_VARIABLE(size);
                    UNUSED_VARIABLE(write_time);
                    if (pack_resource_names.count(exclude_all_ext(path)) == 0u) {
                        const auto path_in_pack = _str(pack_name).combinePath(path).str();
                        DiskFileSystem::DeleteFile(MakeOutputPath(path_in_pack));
                        WriteLog("Delete outdated file {}", path_in_pack);
                    }
                });

                WriteLog("Bake {} complete. Baked {} files (rest are skipped)", pack_name, baked_files);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
                errors++;
            }
        }

        WriteLog("Bake resource packs complete");
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        errors++;
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

            if (_settings.ForceBakering) {
                all_scripts_up_to_date = false;
            }
            else {
                auto script_files = baker_engine.Resources.FilterFiles("fos");
                while (script_files.MoveNext() && all_scripts_up_to_date) {
                    auto file = script_files.GetCurFileHeader();
#if !FO_SINGLEPLAYER
                    if (DiskFileSystem::GetWriteTime(MakeOutputPath(_str("ServerAngelScript/ServerRootModule.fosb", file.GetName()))) <= file.GetWriteTime()) {
                        all_scripts_up_to_date = false;
                    }
                    if (DiskFileSystem::GetWriteTime(MakeOutputPath(_str("ServerAngelScript/ClientRootModule.fosb", file.GetName()))) <= file.GetWriteTime()) {
                        all_scripts_up_to_date = false;
                    }
#else
                    if (DiskFileSystem::GetWriteTime(MakeOutputPath(_str("AngelScript/RootModule.fosb", file.GetName()))) <= file.GetWriteTime()) {
                        all_scripts_up_to_date = false;
                    }
#endif
                    if (DiskFileSystem::GetWriteTime(MakeOutputPath(_str("MapperAngelScript/MapperRootModule.fosb", file.GetName()))) <= file.GetWriteTime()) {
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
                ASCompiler_ServerScriptSystem().InitAngelScriptScripting(baker_engine.Resources);
                as_server_recompiled = true;
            }

            if (!all_scripts_up_to_date) {
                WriteLog("Compile client scripts");
                ASCompiler_ClientScriptSystem().InitAngelScriptScripting(baker_engine.Resources);
            }
#else
            if (!all_scripts_up_to_date) {
                WriteLog("Compile game scripts");
                ASCompiler_SingleScriptSystem().InitAngelScriptScripting(baker_engine.Resources);
            }
#endif
            if (!all_scripts_up_to_date) {
                WriteLog("Compile mapper scripts");
                ASCompiler_MapperScriptSystem().InitAngelScriptScripting(baker_engine.Resources);
            }

            WriteLog("Compile AngelScript scripts complete");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
#endif

        // Validation engine
        FOEngineBase* validation_engine = nullptr;
#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
        ASCompiler_ServerScriptSystem_Validation script_sys;
#else
        ASCompiler_SingleScriptSystem_Validation script_sys;
#endif
#endif

        try {
            WriteLog("Compile AngelScript scripts");

#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
            script_sys.InitAngelScriptScripting(baker_engine.Resources, &validation_engine);
            validation_engine->ScriptSys = &script_sys;
#else
            script_sys.InitAngelScriptScripting(baker_engine.Resources, &validation_engine);
            validation_engine->ScriptSys = &script_sys;
#endif
#endif
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }

        RUNTIME_ASSERT(validation_engine);

        // Configs
        try {
            WriteLog("Bake configs");

            if (_settings.ForceBakering) {
                auto del_configs_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Configs"));
                RUNTIME_ASSERT(del_configs_ok);
            }

            auto configs = baker_engine.Resources.FilterFiles("focfg");

            bool all_configs_up_to_date = true;
            while (configs.MoveNext() && all_configs_up_to_date) {
                auto file = configs.GetCurFileHeader();
                if (DiskFileSystem::GetWriteTime(MakeOutputPath(_str("Configs/{}.focfg", file.GetName()))) <= file.GetWriteTime()) {
                    all_configs_up_to_date = false;
                }
#if !FO_SINGLEPLAYER
                if (DiskFileSystem::GetWriteTime(MakeOutputPath(_str("Configs/Client_{}.focfg", file.GetName()))) <= file.GetWriteTime()) {
                    all_configs_up_to_date = false;
                }
#endif
            }

            if (!all_configs_up_to_date) {
                configs.ResetCounter();
                while (configs.MoveNext()) {
                    auto file = configs.GetCurFile();

                    WriteLog("Process config {}", file.GetName());

                    auto settings_errors = 0;

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
                    extern auto GetServerSettings()->unordered_set<string>;
                    extern auto GetClientSettings()->unordered_set<string>;
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
                        else if (_str(value).isNumber()) {
                            resolved_value = value;
                        }
                        else if (_str(value).isExplicitBool()) {
                            resolved_value = _str(value).toBool() ? "1" : "0";
                        }
                        else if (value.find("::") != string::npos) {
                            bool failed = false;
                            resolved_value = _str("{}", baker_engine.ResolveEnumValue(value, &failed));
                            if (failed) {
                                settings_errors++;
                            }
                        }
                        else {
                            resolved_value = value;
                        }

                        config_content += _str("{}={}\n", key, resolved_value);

#if !FO_SINGLEPLAYER
                        const auto is_server_setting = server_settings.count(key) != 0u;
                        const auto is_client_setting = client_settings.count(key) != 0u;
                        if (is_server_setting) {
                            server_settings.erase(key);
                        }
                        if (is_client_setting) {
                            client_config_content += _str("{}={}\n", key, resolved_value);
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
                        auto cfg_file = DiskFileSystem::OpenFile(MakeOutputPath(_str("Configs/{}{}.focfg", cfg_name1, cfg_name2)), true);
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

        // Protos
        auto proto_mngr = ProtoManager(&baker_engine);

        try {
            WriteLog("Bake protos");

            auto parse_protos = false;

            if (_settings.ForceBakering) {
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

                    if (check_up_to_date("foitem") && check_up_to_date("focr") && check_up_to_date("fomap") && check_up_to_date("foloc")) {
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

                auto proto_errors = 0;

                for (const auto* proto : proto_mngr.GetAllProtos()) {
                    proto_errors += ValidateProperties(proto->GetProperties(), _str("proto {}", proto->GetName()), validation_engine->ScriptSys, resource_hashes);
                }

                // Check maps for locations
                {
                    const auto& loc_protos = proto_mngr.GetProtoLocations();
                    const auto& map_protos = proto_mngr.GetProtoMaps();
                    for (auto&& [pid, proto] : loc_protos) {
                        for (auto map_pid : proto->GetMapProtos()) {
                            if (map_protos.count(map_pid) == 0u) {
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
                server_proto_mngr.ParseProtos(baker_engine.Resources);

                WriteLog("Process client protos");
                auto client_engine = BakerEngine(PropertiesRelationType::ClientRelative);
                auto client_proto_mngr = ProtoManager(&client_engine);
                client_proto_mngr.ParseProtos(baker_engine.Resources);

                // Maps
                WriteLog("Process maps");

                if (_settings.ForceBakering) {
                    auto del_maps_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Maps"));
                    RUNTIME_ASSERT(del_maps_ok);
                    del_maps_ok = DiskFileSystem::DeleteDir(MakeOutputPath("StaticMaps"));
                    RUNTIME_ASSERT(del_maps_ok);
                }

                const auto fomap_files = baker_engine.Resources.FilterFiles("fomap");

                for (const auto& proto_entry : proto_mngr.GetProtoMaps()) {
                    const auto* proto_map = proto_entry.second;

                    auto map_file = fomap_files.FindFileByName(proto_map->GetName());
                    RUNTIME_ASSERT(map_file);

                    // Skip if up to date
                    if (!_settings.ForceBakering) {
                        if (DiskFileSystem::GetWriteTime(MakeOutputPath(_str("Maps/{}.fomapb", proto_map->GetName()))) > map_file.GetWriteTime()) {
                            continue;
                        }
                    }

                    vector<uchar> props_data;
                    uint map_cr_count = 0u;
                    uint map_item_count = 0u;
                    uint map_scen_count = 0u;
                    uint map_tile_count = 0u;
                    vector<uchar> map_cr_data;
                    vector<uchar> map_item_data;
                    vector<uchar> map_scen_data;
                    vector<uchar> map_tile_data;
                    set<hstring> tile_hstrings;
                    auto map_cr_data_writer = DataWriter(map_cr_data);
                    auto map_item_data_writer = DataWriter(map_item_data);
                    auto map_scen_data_writer = DataWriter(map_scen_data);
                    auto map_tile_data_writer = DataWriter(map_tile_data);
                    auto map_errors = 0;

                    try {
                        MapLoader::Load(
                            proto_map->GetName(), map_file.GetStr(), server_proto_mngr, server_engine,
                            [&](id_t id, const ProtoCritter* proto, const map<string, string>& kv) -> bool {
                                auto props = copy(proto->GetProperties());

                                if (props.LoadFromText(kv)) {
                                    map_errors += ValidateProperties(props, _str("map {} critter {} with id {}", proto_map->GetName(), proto->GetName(), id), validation_engine->ScriptSys, resource_hashes);

                                    map_cr_count++;
                                    map_cr_data_writer.Write<id_t::underlying_type>(id.underlying_value());
                                    map_cr_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());
                                    props.StoreAllData(props_data);
                                    map_cr_data_writer.Write<uint>(static_cast<uint>(props_data.size()));
                                    map_cr_data_writer.WritePtr(props_data.data(), props_data.size());
                                }
                                else {
                                    WriteLog("Invalid critter {} on map {} with id {}", proto->GetName(), proto_map->GetName(), id);
                                    map_errors++;
                                }

                                return true;
                            },
                            [&](id_t id, const ProtoItem* proto, const map<string, string>& kv) -> bool {
                                auto props = copy(proto->GetProperties());

                                if (props.LoadFromText(kv)) {
                                    map_errors += ValidateProperties(props, _str("map {} item {} with id {}", proto_map->GetName(), proto->GetName(), id), validation_engine->ScriptSys, resource_hashes);

                                    map_item_count++;
                                    map_item_data_writer.Write<id_t::underlying_type>(id.underlying_value());
                                    map_item_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());
                                    props.StoreAllData(props_data);
                                    map_item_data_writer.Write<uint>(static_cast<uint>(props_data.size()));
                                    map_item_data_writer.WritePtr(props_data.data(), props_data.size());

                                    if (proto->IsStatic() && !proto->GetIsHidden()) {
                                        const auto* client_proto = client_proto_mngr.GetProtoItem(proto->GetProtoId());
                                        auto client_props = copy(client_proto->GetProperties());

                                        if (client_props.LoadFromText(kv)) {
                                            map_scen_count++;
                                            map_scen_data_writer.Write<id_t::underlying_type>(id.underlying_value());
                                            map_scen_data_writer.Write<hstring::hash_t>(client_proto->GetProtoId().as_hash());
                                            client_props.StoreAllData(props_data);
                                            map_scen_data_writer.Write<uint>(static_cast<uint>(props_data.size()));
                                            map_scen_data_writer.WritePtr(props_data.data(), props_data.size());
                                        }
                                        else {
                                            WriteLog("Invalid item (client side) {} on map {} with id {}", proto->GetName(), proto_map->GetName(), id);
                                            map_errors++;
                                        }
                                    }
                                }
                                else {
                                    WriteLog("Invalid item {} on map {} with id {}", proto->GetName(), proto_map->GetName(), id);
                                    map_errors++;
                                }

                                return true;
                            },
                            [&](MapTile&& tile) -> bool {
                                const auto tile_name = server_engine.ResolveHash(tile.NameHash);
                                if (resource_hashes.count(tile_name) != 0u) {
                                    map_tile_count++;
                                    map_tile_data_writer.WritePtr<MapTile>(&tile);
                                    tile_hstrings.emplace(tile_name);
                                }
                                else {
                                    WriteLog("Invalid tile {} on map {} at hex {} {}", tile_name, proto_map->GetName(), tile.HexX, tile.HexY);
                                    map_errors++;
                                }

                                return true;
                            });
                    }
                    catch (const std::exception& ex) {
                        ReportExceptionAndContinue(ex);
                        map_errors++;
                    }

                    if (map_errors == 0) {
#if !FO_SINGLEPLAYER
                        // Server side
                        {
                            vector<uchar> map_data;
                            auto final_writer = DataWriter(map_data);
                            final_writer.Write<uint>(map_cr_count);
                            final_writer.WritePtr(map_cr_data.data(), map_cr_data.size());
                            final_writer.Write<uint>(map_item_count);
                            final_writer.WritePtr(map_item_data.data(), map_item_data.size());

                            auto map_bin_file = DiskFileSystem::OpenFile(MakeOutputPath(_str("Maps/{}.fomapb", proto_map->GetName())), true);
                            RUNTIME_ASSERT(map_bin_file);
                            auto map_bin_file_write_ok = map_bin_file.Write(map_data);
                            RUNTIME_ASSERT(map_bin_file_write_ok);
                        }
                        // Client side
                        {
                            vector<uchar> map_data;
                            auto final_writer = DataWriter(map_data);
                            final_writer.Write<uint>(map_scen_count);
                            final_writer.WritePtr(map_scen_data.data(), map_scen_data.size());
                            final_writer.Write<uint>(static_cast<uint>(tile_hstrings.size()));
                            for (const auto& hstr : tile_hstrings) {
                                const auto& str = hstr.as_str();
                                final_writer.Write<uint>(static_cast<uint>(str.length()));
                                final_writer.WritePtr(str.c_str(), str.length());
                            }
                            final_writer.Write<uint>(map_tile_count);
                            final_writer.WritePtr(map_tile_data.data(), map_tile_data.size());

                            auto map_bin_file = DiskFileSystem::OpenFile(MakeOutputPath(_str("StaticMaps/{}.fomapb2", proto_map->GetName())), true);
                            RUNTIME_ASSERT(map_bin_file);
                            auto map_bin_file_write_ok = map_bin_file.Write(map_data);
                            RUNTIME_ASSERT(map_bin_file_write_ok);
                        }
#else
                        vector<uchar> map_data;
                        auto final_writer = DataWriter(map_data);
                        final_writer.Write<uint>(map_cr_count);
                        final_writer.WritePtr(map_cr_data.data(), map_cr_data.size());
                        final_writer.Write<uint>(map_item_count);
                        final_writer.WritePtr(map_item_data.data(), map_item_data.size());
                        final_writer.Write<uint>(map_tile_count);
                        final_writer.WritePtr(map_tile_data.data(), map_tile_data.size());

                        auto map_bin_file = DiskFileSystem::OpenFile(MakeOutputPath(_str("Maps/{}.fomapb", proto_map->GetName())), true);
                        RUNTIME_ASSERT(map_bin_file);
                        auto map_bin_file_write_ok = map_bin_file.Write(map_data);
                        RUNTIME_ASSERT(map_bin_file_write_ok);
#endif
                    }
                    else {
                        proto_errors++;
                    }
                }

                if (proto_errors != 0) {
                    throw ProtoValidationException("Proto maps verification failed");
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

        // Dialogs
        auto dialog_mngr = DialogManager(&baker_engine);

        try {
            WriteLog("Bake dialogs");

            if (_settings.ForceBakering) {
                auto del_dialogs_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Dialogs"));
                RUNTIME_ASSERT(del_dialogs_ok);
            }

            dialog_mngr.LoadFromResources();

            int dlg_errors = 0;

            for (auto* dlg_pack : dialog_mngr.GetDialogs()) {
                for (auto&& dlg : dlg_pack->Dialogs) {
                    if (dlg.DlgScriptFuncName) {
                        if (!validation_engine->ScriptSys->FindFunc<void, Critter*, Critter*, string*>(dlg.DlgScriptFuncName) && //
                            !validation_engine->ScriptSys->FindFunc<uint, Critter*, Critter*, string*>(dlg.DlgScriptFuncName)) {
                            WriteLog("Dialog {} invalid start function {}", dlg_pack->PackName, dlg.DlgScriptFuncName);
                            dlg_errors++;
                        }
                    }

                    for (auto&& answer : dlg.Answers) {
                        for (auto&& demand : answer.Demands) {
                            if (demand.Type == DR_SCRIPT) {
                                if ((demand.ValuesCount == 0 && !validation_engine->ScriptSys->FindFunc<bool, Critter*, Critter*>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 1 && !validation_engine->ScriptSys->FindFunc<bool, Critter*, Critter*, int>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 2 && !validation_engine->ScriptSys->FindFunc<bool, Critter*, Critter*, int, int>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 3 && !validation_engine->ScriptSys->FindFunc<bool, Critter*, Critter*, int, int, int>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 4 && !validation_engine->ScriptSys->FindFunc<bool, Critter*, Critter*, int, int, int, int>(demand.AnswerScriptFuncName)) || //
                                    (demand.ValuesCount == 5 && !validation_engine->ScriptSys->FindFunc<bool, Critter*, Critter*, int, int, int, int, int>(demand.AnswerScriptFuncName))) {
                                    WriteLog("Dialog {} answer demand invalid function {}", dlg_pack->PackName, demand.AnswerScriptFuncName);
                                    dlg_errors++;
                                }
                            }
                        }

                        for (auto&& result : answer.Results) {
                            if (result.Type == DR_SCRIPT) {
                                int not_found_count = 0;

                                if ((result.ValuesCount == 0 && !validation_engine->ScriptSys->FindFunc<void, Critter*, Critter*>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 1 && !validation_engine->ScriptSys->FindFunc<void, Critter*, Critter*, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 2 && !validation_engine->ScriptSys->FindFunc<void, Critter*, Critter*, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 3 && !validation_engine->ScriptSys->FindFunc<void, Critter*, Critter*, int, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 4 && !validation_engine->ScriptSys->FindFunc<void, Critter*, Critter*, int, int, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 5 && !validation_engine->ScriptSys->FindFunc<void, Critter*, Critter*, int, int, int, int, int>(result.AnswerScriptFuncName))) {
                                    not_found_count++;
                                }

                                if ((result.ValuesCount == 0 && !validation_engine->ScriptSys->FindFunc<uint, Critter*, Critter*>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 1 && !validation_engine->ScriptSys->FindFunc<uint, Critter*, Critter*, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 2 && !validation_engine->ScriptSys->FindFunc<uint, Critter*, Critter*, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 3 && !validation_engine->ScriptSys->FindFunc<uint, Critter*, Critter*, int, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 4 && !validation_engine->ScriptSys->FindFunc<uint, Critter*, Critter*, int, int, int, int>(result.AnswerScriptFuncName)) || //
                                    (result.ValuesCount == 5 && !validation_engine->ScriptSys->FindFunc<uint, Critter*, Critter*, int, int, int, int, int>(result.AnswerScriptFuncName))) {
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
                    auto dlg_file = DiskFileSystem::OpenFile(MakeOutputPath(_str("Dialogs/{}.fodlg", file.GetName())), true);
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

        // Texts
        try {
            WriteLog("Bake texts");

            auto del_texts_ok = DiskFileSystem::DeleteDir(MakeOutputPath("Texts"));
            RUNTIME_ASSERT(del_texts_ok);

            set<string> languages;
            auto txt_files = baker_engine.Resources.FilterFiles("fotxt");
            while (txt_files.MoveNext()) {
                const auto file = txt_files.GetCurFileHeader();
                const auto lang = file.GetName().substr(0, 4);
                if (languages.emplace(lang).second) {
                    WriteLog("Language: {}", lang);
                }
            }

            vector<LanguagePack> lang_packs;

            for (const auto& lang_name : languages) {
                LanguagePack lang_pack;
                lang_pack.ParseTexts(baker_engine.Resources, baker_engine, lang_name);
                lang_packs.push_back(lang_pack);
            }

            // Dialog texts
            for (auto* pack : dialog_mngr.GetDialogs()) {
                for (size_t i = 0; i < pack->TextsLang.size(); i++) {
                    for (auto& lang : lang_packs) {
                        if (pack->TextsLang[i] != lang.NameCode) {
                            continue;
                        }

                        if (lang.Msg[TEXTMSG_DLG].IsIntersects(pack->Texts[i])) {
                            throw GenericException("Dialog text intersection detected", pack->PackName);
                        }

                        lang.Msg[TEXTMSG_DLG] += pack->Texts[i];
                    }
                }
            }

            // Proto texts
            const auto fill_proto_texts = [&lang_packs](const auto& protos, int txt_type) {
                for (auto&& [pid, proto] : protos) {
                    for (size_t i = 0; i < proto->TextsLang.size(); i++) {
                        for (auto& lang : lang_packs) {
                            if (proto->TextsLang[i] != lang.NameCode) {
                                continue;
                            }

                            if (lang.Msg[txt_type].IsIntersects(*proto->Texts[i])) {
                                throw GenericException("Proto text intersection detected", proto->GetName(), txt_type);
                            }

                            lang.Msg[txt_type] += *proto->Texts[i];
                        }
                    }
                }
            };

            fill_proto_texts(proto_mngr.GetProtoCritters(), TEXTMSG_DLG);
            fill_proto_texts(proto_mngr.GetProtoItems(), TEXTMSG_ITEM);
            fill_proto_texts(proto_mngr.GetProtoLocations(), TEXTMSG_LOCATIONS);

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

        WriteLog("Bake content complete");
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        errors++;
    }

    WriteLog("Time {:.2f} seconds", (Timer::RealtimeTick() - start_time) / 1000.0);

    // Finalize
    if (errors != 0) {
        WriteLog("Bakering failed!");
        ExitApp(false);
    }

    WriteLog("Bakering complete!");

    {
        auto build_hash_file = DiskFileSystem::OpenFile(MakeOutputPath("Resources.build-hash"), true, true);
        RUNTIME_ASSERT(build_hash_file);
        const auto build_hash_writed = build_hash_file.Write(FO_BUILD_HASH);
        RUNTIME_ASSERT(build_hash_writed);
    }
}

auto Baker::ValidateProperties(const Properties& props, string_view context_str, ScriptSystem* script_sys, const unordered_set<hstring>& resource_hashes) -> int
{
    STACK_TRACE_ENTRY();

    unordered_map<string, std::function<bool(hstring)>> script_func_verify = {
        {"ItemInit", [script_sys](hstring func_name) { return !!script_sys->FindFunc<void, Item*, bool>(func_name); }},
        {"ItemScenery", [script_sys](hstring func_name) { return !!script_sys->FindFunc<bool, Critter*, StaticItem*, Item*, int>(func_name); }},
        {"ItemTrigger", [script_sys](hstring func_name) { return !!script_sys->FindFunc<void, Critter*, StaticItem*, bool, uchar>(func_name); }},
        {"CritterInit", [script_sys](hstring func_name) { return !!script_sys->FindFunc<void, Critter*, bool>(func_name); }},
        {"MapInit", [script_sys](hstring func_name) { return !!script_sys->FindFunc<void, Map*, bool>(func_name); }},
        {"LocationInit", [script_sys](hstring func_name) { return !!script_sys->FindFunc<void, Location*, bool>(func_name); }},
        {"LocationEntrance", [script_sys](hstring func_name) { return !!script_sys->FindFunc<bool, Location*, vector<Critter*>, uchar>(func_name); }},
    };

    int errors = 0;

    const auto* registrator = props.GetRegistrator();

    for (uint i = 0; i < registrator->GetCount(); i++) {
        const auto* prop = registrator->GetByIndex(static_cast<int>(i));

        if (prop->IsBaseTypeResource()) {
            if (prop->IsPlainData()) {
                const auto h = props.GetValue<hstring>(prop);
                if (h && resource_hashes.count(h) == 0u) {
                    WriteLog("Resource {} not found for property {} in {}", h, prop->GetName(), context_str);
                    errors++;
                }
            }
            else if (prop->IsArray()) {
                const auto hashes = props.GetValue<vector<hstring>>(prop);
                for (const auto h : hashes) {
                    if (h && resource_hashes.count(h) == 0u) {
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

        if (prop->IsBaseScriptFuncType()) {
            if (prop->IsPlainData()) {
                const auto func_name = props.GetValue<hstring>(prop);
                if (script_func_verify.count(prop->GetBaseScriptFuncType()) == 0u) {
                    WriteLog("Invalid script func {} of type {} for property {} in {}", func_name, prop->GetBaseScriptFuncType(), prop->GetName(), context_str);
                    errors++;
                }
                else if (func_name && !script_func_verify[prop->GetBaseScriptFuncType()](func_name)) {
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
}

auto BakerDataSource::FindFile(const string& path) const -> File*
{
    STACK_TRACE_ENTRY();

    if (const auto it = _bakedFiles.find(path); it != _bakedFiles.end()) {
        return it->second ? it->second.get() : nullptr;
    }

    const auto ext = _str(path).getFileExtension();
    if (ext == "fopts") {
        if (auto file = _inputResources.ReadFile(path)) {
            _bakedFiles[path] = std::make_unique<File>(std::move(file));
        }
        else {
            _bakedFiles[path] = nullptr;
        }
    }
    else if (ext == "fofx") {
        if (auto file = _inputResources.ReadFile(path)) {
            auto baker = EffectBaker(_settings, FileCollection({file.Duplicate()}), nullptr, [&file, this](string_view baked_path, const_span<uchar> baked_data) {
                auto baked_file = File(_str(baked_path).extractFileName().eraseFileExtension(), baked_path, file.GetWriteTime(), const_cast<BakerDataSource*>(this), baked_data, true);
                _bakedFiles[string(baked_path)] = std::make_unique<File>(std::move(baked_file));
            });
            baker.AutoBake();
        }
        else {
            _bakedFiles[path] = nullptr;
        }
    }
    else if (ImageBaker::IsImageExt(ext)) {
        if (auto file = _inputResources.ReadFile(path)) {
            auto baker = ImageBaker(_settings, FileCollection({file.Duplicate()}), nullptr, [&file, this](string_view baked_path, const_span<uchar> baked_data) {
                auto baked_file = File(_str(baked_path).extractFileName().eraseFileExtension(), baked_path, file.GetWriteTime(), const_cast<BakerDataSource*>(this), baked_data, true);
                _bakedFiles[string(baked_path)] = std::make_unique<File>(std::move(baked_file));
            });
            baker.AutoBake();
        }
        else {
            _bakedFiles[path] = nullptr;
        }
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

auto BakerDataSource::OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uchar>
{
    STACK_TRACE_ENTRY();

    if (const auto* file = FindFile(string(path)); file != nullptr) {
        size = file->GetSize();
        write_time = file->GetWriteTime();
        return {file->GetBuf(), [](auto* p) {}};
    }
    return nullptr;
}

auto BakerDataSource::GetFileNames(string_view path, bool include_subdirs, string_view ext) const -> vector<string>
{
    STACK_TRACE_ENTRY();

    throw NotImplementedException(LINE_STR);
}
