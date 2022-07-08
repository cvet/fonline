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

#include "Common.h"

#include "Application.h"
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

DECLARE_EXCEPTION(ProtoValidationException);

class BakerEngine : public FOEngineBase
{
public:
    BakerEngine(GlobalSettings& settings, PropertiesRelationType props_relation) :
        FOEngineBase(settings, props_relation, [this] {
            extern void Baker_RegisterData(FOEngineBase*);
            Baker_RegisterData(this);
            return nullptr;
        })
    {
    }
};

// Implementation in AngelScriptScripting-*Compiler.cpp
#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
struct ASCompiler_ServerScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(string_view script_path);
};
struct ASCompiler_ServerScriptSystem_Validation : public ScriptSystem
{
    void InitAngelScriptScripting(FOEngineBase** out_engine);
};
struct ASCompiler_ClientScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(string_view script_path);
};
#else
struct ASCompiler_SingleScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(string_view script_path);
};
struct ASCompiler_SingleScriptSystem_Validation : public ScriptSystem
{
    void InitAngelScriptScripting(FOEngineBase** out_engine);
};
#endif
struct ASCompiler_MapperScriptSystem : public ScriptSystem
{
    void InitAngelScriptScripting(string_view script_path);
};

static auto CheckScriptsUpToDate(string_view script_path) -> bool;
static auto ValidateProperties(const Properties& props, string_view context_str, ScriptSystem* script_sys, const unordered_set<hstring>& resource_hashes) -> int;

// External variable for compiler messages
unordered_set<string> CompilerPassedMessages;
#endif

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto BakerApp(int argc, char** argv) -> int
#endif
{
    try {
        InitApp(argc, argv, "Baker");
        LogWithoutTimestamp();

        const auto build_hash_deleted = DiskFileSystem::DeleteFile("Resources.build-hash");
        RUNTIME_ASSERT(build_hash_deleted);

        WriteLog("Start bakering");

        const auto start_time = Timer::RealtimeTick();

        if (App->Settings.ForceBakering) {
            WriteLog("Force rebuild all resources");
        }

        auto errors = 0;

        // AngelScript scripts
#if FO_ANGELSCRIPT_SCRIPTING
        auto as_server_recompiled = false;

        try {
            WriteLog("Compile AngelScript scripts");

#if !FO_SINGLEPLAYER
            WriteLog("Compile server scripts");
            RUNTIME_ASSERT(!App->Settings.ASServer.empty());
            if (App->Settings.ForceBakering || !CheckScriptsUpToDate(App->Settings.BakeASServer)) {
                ASCompiler_ServerScriptSystem().InitAngelScriptScripting(App->Settings.BakeASServer);
                as_server_recompiled = true;
            }

            WriteLog("Compile client scripts");
            RUNTIME_ASSERT(!App->Settings.ASClient.empty());
            if (App->Settings.ForceBakering || !CheckScriptsUpToDate(App->Settings.BakeASClient)) {
                ASCompiler_ClientScriptSystem().InitAngelScriptScripting(App->Settings.BakeASClient);
            }
#else
            WriteLog("Compile game scripts");
            RUNTIME_ASSERT(!App->Settings.ASSingle.empty());
            if (App->Settings.ForceBakering || !CheckScriptsUpToDate(App->Settings.BakeASSingle)) {
                ASCompiler_SingleScriptSystem().InitAngelScriptScripting(App->Settings.BakeASSingle);
            }
#endif
            WriteLog("Compile mapper scripts");
            RUNTIME_ASSERT(!App->Settings.ASMapper.empty());
            if (App->Settings.ForceBakering || !CheckScriptsUpToDate(App->Settings.BakeASMapper)) {
                ASCompiler_MapperScriptSystem().InitAngelScriptScripting(App->Settings.BakeASMapper);
            }

            WriteLog("Compile AngelScript scripts complete!");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }
#endif

        // Resource packs
        auto resource_names = unordered_set<string>();

        try {
            WriteLog("Bake resource packs");

            RUNTIME_ASSERT(!App->Settings.BakeResourceEntries.empty());

            map<string, vector<string>> res_packs;
            for (const auto& re : App->Settings.BakeResourceEntries) {
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
                    if (App->Settings.ForceBakering) {
                        auto del_res_ok = DiskFileSystem::DeleteDir(pack_name);
                        RUNTIME_ASSERT(del_res_ok);
                    }

                    DiskFileSystem::MakeDirTree(pack_name);

                    const auto is_raw_only = pack_name == "Raw";
                    auto resources = res_files.FilterFiles("");
                    size_t baked_files = 0;

                    WriteLog("Create resource pack {} from {} files", pack_name, resources.GetFilesCount());

                    // Bake files
                    auto pack_resource_names = unordered_set<string>();

                    const auto exclude_all_ext = [](string_view path) -> string {
                        size_t pos = path.rfind('/');
                        pos = path.find('.', pos != string::npos ? pos : 0);
                        return pos != string::npos ? string(path.substr(0, pos)) : string(path);
                    };

                    if (!is_raw_only) {
                        const auto bake_checker = [&pack_name, &resource_names, &pack_resource_names, &exclude_all_ext](const FileHeader& file_header) -> bool {
                            resource_names.emplace(file_header.GetPath());
                            pack_resource_names.emplace(exclude_all_ext(file_header.GetPath()));

                            if (!App->Settings.ForceBakering) {
                                return file_header.GetWriteTime() > DiskFileSystem::GetWriteTime(_str("{}/{}", pack_name, file_header.GetPath()));
                            }
                            else {
                                return true;
                            }
                        };

                        const auto write_data = [&pack_name, &baked_files](string_view path, const_span<uchar> baked_data) {
                            auto res_file = DiskFileSystem::OpenFile(_str("{}/{}", pack_name, path), true);
                            RUNTIME_ASSERT(res_file);
                            const auto res_file_write_ok = res_file.Write(baked_data);
                            RUNTIME_ASSERT(res_file_write_ok);

                            baked_files++;
                        };

                        vector<unique_ptr<BaseBaker>> bakers;
                        bakers.emplace_back(std::make_unique<ImageBaker>(App->Settings, resources, bake_checker, write_data));
                        bakers.emplace_back(std::make_unique<EffectBaker>(App->Settings, resources, bake_checker, write_data));
#if FO_ENABLE_3D
                        bakers.emplace_back(std::make_unique<ModelBaker>(App->Settings, resources, bake_checker, write_data));
#endif

                        for (auto&& baker : bakers) {
                            baker->AutoBake();
                        }
                    }

                    // Raw copy
                    resources.ResetCounter();
                    while (resources.MoveNext()) {
                        auto file_header = resources.GetCurFileHeader();

                        // Skip not necessary files
                        if (!is_raw_only) {
                            const auto ext = _str(file_header.GetPath()).getFileExtension().str();
                            const auto& base_exts = App->Settings.BakeExtraFileExtensions;
                            if (std::find(base_exts.begin(), base_exts.end(), ext) == base_exts.end()) {
                                continue;
                            }
                        }

                        resource_names.emplace(file_header.GetPath());
                        pack_resource_names.emplace(exclude_all_ext(file_header.GetPath()));

                        const auto path_in_pack = _str(pack_name).combinePath(file_header.GetPath()).str();

                        if (!App->Settings.ForceBakering && DiskFileSystem::GetWriteTime(path_in_pack) >= file_header.GetWriteTime()) {
                            continue;
                        }

                        auto file = resources.GetCurFile();

                        auto res_file = DiskFileSystem::OpenFile(path_in_pack, true);
                        RUNTIME_ASSERT(res_file);
                        auto res_file_write_ok = res_file.Write(file.GetData());
                        RUNTIME_ASSERT(res_file_write_ok);
                    }

                    // Delete outdated
                    DiskFileSystem::IterateDir(pack_name, "", true, [&pack_name, &pack_resource_names, &exclude_all_ext](string_view path, size_t size, uint64 write_time) {
                        UNUSED_VARIABLE(size);
                        UNUSED_VARIABLE(write_time);
                        if (pack_resource_names.count(exclude_all_ext(path)) == 0u) {
                            const auto path_in_pack = _str(pack_name).combinePath(path).str();
                            DiskFileSystem::DeleteFile(path_in_pack);
                            WriteLog("Delete outdated file {}", path_in_pack);
                        }
                    });

                    WriteLog("Bake {} complete! Baked {} files (rest are skipped)", pack_name, baked_files);
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);
                    errors++;
                }
            }

            WriteLog("Bake resource packs complete!");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            errors++;
        }

        // Content
        try {
            WriteLog("Bake content");

            RUNTIME_ASSERT(!App->Settings.BakeContentEntries.empty());

            auto engine = BakerEngine(App->Settings, PropertiesRelationType::BothRelative);

            for (const auto& dir : App->Settings.BakeContentEntries) {
                WriteLog("Add content entry {}", dir);
                engine.FileSys.AddDataSource(dir, DataSourceType::DirRoot);
            }

            // Protos
            auto proto_mngr = ProtoManager(&engine);

            try {
                WriteLog("Bake protos");

                auto parse_protos = false;

                if (App->Settings.ForceBakering) {
                    auto del_protos_ok = DiskFileSystem::DeleteDir("Protos");
                    RUNTIME_ASSERT(del_protos_ok);

                    parse_protos = true;
                }

#if FO_ANGELSCRIPT_SCRIPTING
                if (as_server_recompiled) {
                    parse_protos = true;
                }
#endif

                if (!parse_protos) {
                    const auto last_write_time = DiskFileSystem::GetWriteTime("Protos/Protos.foprob");
                    if (last_write_time > 0) {
                        const auto check_up_to_date = [last_write_time, &file_sys = engine.FileSys](string_view ext) -> bool {
                            auto files = file_sys.FilterFiles(ext);
                            while (files.MoveNext()) {
                                auto file = files.GetCurFileHeader();
                                if (file.GetWriteTime() > last_write_time) {
                                    return false;
                                }
                            }
                            return true;
                        };

                        if (check_up_to_date("foitem") && check_up_to_date("focr") && check_up_to_date("fomap") && check_up_to_date("foloc")) {
                            WriteLog("Protos up to date. Skip!");
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
                    proto_mngr.ParseProtos(engine.FileSys);

                    // Protos validation
                    unordered_set<hstring> resource_hashes;
                    for (const auto& name : resource_names) {
                        resource_hashes.insert(engine.ToHashedString(name));
                    }

                    FOEngineBase* validation_engine = nullptr;

#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
                    ASCompiler_ServerScriptSystem_Validation script_sys;
                    script_sys.InitAngelScriptScripting(&validation_engine);
                    validation_engine->ScriptSys = &script_sys;
#else
                    ASCompiler_SingleScriptSystem_Validation script_sys;
                    script_sys.InitAngelScriptScripting(&validation_engine);
                    validation_engine->ScriptSys = &script_sys;
#endif
#else
#error...
#endif

                    WriteLog("Validate protos");

                    auto proto_errors = 0;

                    for (const auto* proto : proto_mngr.GetAllProtos()) {
                        proto_errors += ValidateProperties(proto->GetProperties(), _str("proto {}", proto->GetName()), validation_engine->ScriptSys, resource_hashes);
                    }

                    // Check player proto
                    {
                        const auto& cr_protos = proto_mngr.GetProtoCritters();
                        if (cr_protos.count(engine.ToHashedString("Player")) == 0u) {
                            WriteLog("Player proto 'Player.focr' not loaded");
                            proto_errors++;
                        }
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
                        throw ProtoValidationException("Proto resources verification failed");
                    }

                    const auto write_protos = [](const ProtoManager& protos, string_view fname) {
                        const auto data = protos.GetProtosBinaryData();
                        RUNTIME_ASSERT(!data.empty());
                        auto protos_file = DiskFileSystem::OpenFile(fname, true);
                        RUNTIME_ASSERT(protos_file);
                        const auto protos_write_ok = protos_file.Write(data.data(), data.size());
                        RUNTIME_ASSERT(protos_write_ok);
                    };

                    WriteLog("Write protos");
                    write_protos(proto_mngr, "Protos/Protos.foprob");

                    WriteLog("Process server protos");
                    auto server_engine = BakerEngine(App->Settings, PropertiesRelationType::ServerRelative);
                    auto server_proto_mngr = ProtoManager(&server_engine);
                    server_proto_mngr.ParseProtos(engine.FileSys);
                    write_protos(server_proto_mngr, "Protos/ServerProtos.foprob");

                    WriteLog("Process client protos");
                    auto client_engine = BakerEngine(App->Settings, PropertiesRelationType::ClientRelative);
                    auto client_proto_mngr = ProtoManager(&client_engine);
                    client_proto_mngr.ParseProtos(engine.FileSys);
                    write_protos(client_proto_mngr, "Protos/ClientProtos.foprob");

                    // Maps
                    WriteLog("Process maps");

                    if (App->Settings.ForceBakering) {
                        auto del_maps_ok = DiskFileSystem::DeleteDir("Maps");
                        RUNTIME_ASSERT(del_maps_ok);
                    }

                    const auto fomap_files = engine.FileSys.FilterFiles("fomap");

                    for (const auto& proto_entry : proto_mngr.GetProtoMaps()) {
                        const auto* proto_map = proto_entry.second;

                        auto map_file = fomap_files.FindFileByName(proto_map->GetName());
                        RUNTIME_ASSERT(map_file);

                        // Skip if up to date
                        if (!App->Settings.ForceBakering) {
                            if (DiskFileSystem::GetWriteTime("Maps/{}.fomapb") > map_file.GetWriteTime()) {
                                continue;
                            }
                        }

                        uint map_cr_count = 0u;
                        uint map_item_count = 0u;
                        uint map_tile_count = 0u;
                        vector<uchar> map_cr_data;
                        vector<uchar> map_item_data;
                        vector<uchar> map_tile_data;
                        auto map_cr_data_writer = DataWriter(map_cr_data);
                        auto map_item_data_writer = DataWriter(map_item_data);
                        auto map_tile_data_writer = DataWriter(map_tile_data);
                        auto map_errors = 0;

                        try {
                            MapLoader::Load(
                                proto_map->GetName(), map_file.GetStr(), server_proto_mngr, server_engine,
                                [&](uint id, const ProtoCritter* proto, const map<string, string>& kv) -> bool {
                                    Properties props(proto->GetProperties().GetRegistrator());
                                    if (props.LoadFromText(kv)) {
                                        map_errors += ValidateProperties(props, _str("map {} critter {} with id {}", proto_map->GetName(), proto->GetName(), id), validation_engine->ScriptSys, resource_hashes);

                                        map_cr_count++;
                                        map_cr_data_writer.Write<uint>(id);
                                        map_cr_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());

                                        vector<uchar*>* props_data = nullptr;
                                        vector<uint>* props_data_sizes = nullptr;
                                        props.StoreAllData(&props_data, &props_data_sizes);

                                        map_cr_data_writer.Write<ushort>(static_cast<ushort>(props_data->size()));
                                        for (size_t i = 0; i < props_data->size(); i++) {
                                            const auto cur_size = props_data_sizes->at(i);
                                            map_cr_data_writer.Write<uint>(cur_size);
                                            map_cr_data_writer.WritePtr(props_data->at(i), cur_size);
                                        }
                                    }
                                    else {
                                        WriteLog("Invalid critter {} on map {} with id {}", proto->GetName(), proto_map->GetName(), id);
                                        map_errors++;
                                    }

                                    return true;
                                },
                                [&](uint id, const ProtoItem* proto, const map<string, string>& kv) -> bool {
                                    Properties props(proto->GetProperties().GetRegistrator());
                                    if (props.LoadFromText(kv)) {
                                        map_errors += ValidateProperties(props, _str("map {} item {} with id {}", proto_map->GetName(), proto->GetName(), id), validation_engine->ScriptSys, resource_hashes);

                                        map_item_count++;
                                        map_item_data_writer.Write<uint>(id);
                                        map_item_data_writer.Write<hstring::hash_t>(proto->GetProtoId().as_hash());

                                        vector<uchar*>* props_data = nullptr;
                                        vector<uint>* props_data_sizes = nullptr;
                                        props.StoreAllData(&props_data, &props_data_sizes);

                                        map_item_data_writer.Write<ushort>(static_cast<ushort>(props_data->size()));
                                        for (size_t i = 0; i < props_data->size(); i++) {
                                            const auto cur_size = props_data_sizes->at(i);
                                            map_item_data_writer.Write<uint>(cur_size);
                                            map_item_data_writer.WritePtr(props_data->at(i), cur_size);
                                        }
                                    }
                                    else {
                                        WriteLog("Invalid item {} on map {} with id {}", proto->GetName(), proto_map->GetName(), id);
                                        map_errors++;
                                    }

                                    return true;
                                },
                                [&](MapTile&& tile) -> bool {
                                    const auto tile_name = engine.ResolveHash(tile.NameHash);
                                    if (resource_hashes.count(tile_name) != 0u) {
                                        map_tile_count++;
                                        map_tile_data_writer.WritePtr<MapTile>(&tile);
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
                            vector<uchar> map_data;
                            auto final_writer = DataWriter(map_data);
                            final_writer.Write<uint>(map_cr_count);
                            final_writer.WritePtr(map_cr_data.data(), map_cr_data.size());
                            final_writer.Write<uint>(map_item_count);
                            final_writer.WritePtr(map_item_data.data(), map_item_data.size());
                            final_writer.Write<uint>(map_tile_count);
                            final_writer.WritePtr(map_tile_data.data(), map_tile_data.size());

                            auto map_file = DiskFileSystem::OpenFile(_str("Maps/{}.fomapb", proto_map->GetName()), true);
                            RUNTIME_ASSERT(map_file);
                            auto map_file_write_ok = map_file.Write(map_data);
                            RUNTIME_ASSERT(map_file_write_ok);
                        }
                        else {
                            errors++;
                        }
                    }
                }

                WriteLog("Bake protos complete!");
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
                errors++;
            }

            // Dialogs
            auto dialog_mngr = DialogManager(&engine);

            try {
                WriteLog("Bake dialogs");

                if (App->Settings.ForceBakering) {
                    auto del_dialogs_ok = DiskFileSystem::DeleteDir("Dialogs");
                    RUNTIME_ASSERT(del_dialogs_ok);
                }

                dialog_mngr.LoadFromResources();
                dialog_mngr.ValidateDialogs();

                auto dialogs = engine.FileSys.FilterFiles("fodlg");
                WriteLog("Dialogs count {}", dialogs.GetFilesCount());

                while (dialogs.MoveNext()) {
                    auto file = dialogs.GetCurFile();
                    auto dlg_file = DiskFileSystem::OpenFile(_str("Dialogs/{}.fodlg", file.GetName()), true);
                    RUNTIME_ASSERT(dlg_file);
                    auto dlg_file_write_ok = dlg_file.Write(file.GetBuf(), file.GetSize());
                    RUNTIME_ASSERT(dlg_file_write_ok);
                }

                WriteLog("Bake dialogs complete!");
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
                errors++;
            }

            // Texts
            try {
                WriteLog("Bake texts");

                auto del_texts_ok = DiskFileSystem::DeleteDir("Texts");
                RUNTIME_ASSERT(del_texts_ok);

                vector<LanguagePack> lang_packs;

                for (const auto& lang_name : App->Settings.Languages) {
                    LanguagePack lang_pack;
                    lang_pack.ParseTexts(engine.FileSys, engine, lang_name);
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
                    lang_pack.SaveTextsToDisk("Texts");
                }

                WriteLog("Bake texts complete!");
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
                errors++;
            }

            WriteLog("Bake content complete!");
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
            auto build_hash_file = DiskFileSystem::OpenFile("Resources.build-hash", true, true);
            RUNTIME_ASSERT(build_hash_file);
            const auto build_hash_writed = build_hash_file.Write(FO_BUILD_HASH);
            RUNTIME_ASSERT(build_hash_writed);
        }

        ExitApp(true);
    }
    catch (std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}

static auto CheckScriptsUpToDate(string_view script_path) -> bool
{
    uint64 last_compiled_time;
    {
        const auto compiled_script_path = _str("AngelScript/{}b", _str(script_path).extractFileName()).str();
        const auto compiled_script_file = DiskFileSystem::OpenFile(compiled_script_path, false);
        if (!compiled_script_file) {
            return false;
        }

        last_compiled_time = compiled_script_file.GetWriteTime();
    }

    string root_content;
    {
        auto root_script_file = DiskFileSystem::OpenFile(script_path, false);
        if (!root_script_file) {
            return false;
        }

        if (root_script_file.GetWriteTime() > last_compiled_time) {
            return false;
        }

        root_content.resize(root_script_file.GetSize());
        if (!root_script_file.Read(root_content.data(), root_content.length())) {
            return false;
        }
    }

    size_t pos = 0;
    while ((pos = root_content.find("#include \"", pos)) != string::npos) {
        const size_t start_pos = pos + "#include \""_len;
        const size_t end_pos = root_content.find("\"", start_pos);
        RUNTIME_ASSERT(end_pos != string::npos);

        const auto include_path = root_content.substr(start_pos, end_pos - start_pos);
        const auto include_file = DiskFileSystem::OpenFile(include_path, false);
        if (!include_file) {
            return false;
        }

        if (include_file.GetWriteTime() > last_compiled_time) {
            return false;
        }

        pos = end_pos + 1;
    }

    WriteLog("Scripts up to date. Skip!");
    return true;
}

struct Item : Entity
{
};
struct StaticItem : Entity
{
};
struct Critter : Entity
{
};
struct Map : Entity
{
};
struct Location : Entity
{
};

static auto ValidateProperties(const Properties& props, string_view context_str, ScriptSystem* script_sys, const unordered_set<hstring>& resource_hashes) -> int
{
    unordered_map<string, std::function<bool(string_view)>> script_func_verify = {
        {"ItemInit", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Item*, bool>(func_name); }},
        {"ItemScenery", [script_sys](string_view func_name) { return !!script_sys->FindFunc<bool, Critter*, StaticItem*, int>(func_name); }},
        {"ItemTrigger", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Critter*, StaticItem*, bool, uchar>(func_name); }},
        {"CritterInit", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Critter*, bool>(func_name); }},
        {"MapInit", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Map*, bool>(func_name); }},
        {"LocationInit", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Location*, bool>(func_name); }},
        {"LocationEntrance", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Location*, vector<Critter*>, uchar>(func_name); }},
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
                if (!script_func_verify.count(prop->GetBaseScriptFuncType())) {
                    WriteLog("Invalid script func type {} for property {} in {}", prop->GetBaseScriptFuncType(), prop->GetName(), context_str);
                    errors++;
                }
                else if (func_name && !script_func_verify[prop->GetBaseScriptFuncType()](func_name)) {
                    WriteLog("Verification failed for func type {} for property {} in {}", prop->GetBaseScriptFuncType(), prop->GetName(), context_str);
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
