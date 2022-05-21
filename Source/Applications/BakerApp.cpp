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
    BakerEngine() :
        FOEngineBase(Dummy, PropertiesRelationType::BothRelative, [this] {
            extern void Baker_RegisterData(FOEngineBase*);
            Baker_RegisterData(this);
            return nullptr;
        })
    {
    }

    GlobalSettings Dummy {};
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
static void ValidateProtos(const vector<const ProtoEntity*>& protos, ScriptSystem* script_sys, const unordered_set<hstring>& resource_names);

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
                        WriteLog("Add resource pack {} entry '{}'", pack_name, path);
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
                        size_t pos = path.find_last_of('/');
                        pos = path.find_first_of('.', pos != string::npos ? pos : 0);
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
                            baker->AutoBakeModels();
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
                            WriteLog("Delete outdated file '{}'", path_in_pack);
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

            BakerEngine engine;

            for (const auto& dir : App->Settings.BakeContentEntries) {
                WriteLog("Add content entry '{}'", dir);
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
                    proto_mngr.ParseProtos(engine.FileSys);

                    // Protos validation
                    {
                        unordered_set<hstring> resource_hashes;
                        for (const auto& name : resource_names) {
                            resource_hashes.insert(engine.ToHashedString(name));
                        }

                        FOEngineBase* validation_engine = nullptr;

#if FO_ANGELSCRIPT_SCRIPTING
#if !FO_SINGLEPLAYER
                        ASCompiler_ServerScriptSystem_Validation().InitAngelScriptScripting(&validation_engine);
#else
                        ASCompiler_SingleScriptSystem_Validation().InitAngelScriptScripting(&validation_engine);
#endif
#else
#error...
#endif

                        ValidateProtos(proto_mngr.GetAllProtos(), validation_engine->ScriptSys, resource_hashes);
                    }

                    auto data = proto_mngr.GetProtosBinaryData();
                    RUNTIME_ASSERT(!data.empty());

                    auto protos_file = DiskFileSystem::OpenFile("Protos/Protos.foprob", true);
                    RUNTIME_ASSERT(protos_file);
                    auto protos_write_ok = protos_file.Write(data.data(), data.size());
                    RUNTIME_ASSERT(protos_write_ok);
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
struct Critter : Entity
{
};
struct Map : Entity
{
};
struct Location : Entity
{
};

static void ValidateProtos(const vector<const ProtoEntity*>& protos, ScriptSystem* script_sys, const unordered_set<hstring>& resource_hashes)
{
    auto errors = 0;

    unordered_map<string, std::function<bool(string_view)>> script_func_verify = {
        {"ItemInit", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Item, bool>(func_name); }},
        {"CritterInit", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Critter, bool>(func_name); }},
        {"MapInit", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Map, bool>(func_name); }},
        {"LocationInit", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Location, bool>(func_name); }},
        {"LocationEntrance", [script_sys](string_view func_name) { return !!script_sys->FindFunc<void, Location, vector<Critter>, uchar>(func_name); }},
    };

    for (const auto* proto : protos) {
        const auto* registrator = proto->GetProperties().GetRegistrator();

        for (uint i = 0; i < registrator->GetCount(); i++) {
            const auto* prop = registrator->GetByIndex(i);

            if (prop->IsBaseTypeResource()) {
                const auto h = proto->GetProperties().GetValue<hstring>(prop);
                if (h && resource_hashes.count(h) == 0u) {
                    WriteLog("Resource '{}' not found for property {} in proto {}", h, prop->GetName(), proto->GetName());
                    errors++;
                }
            }

            if (prop->IsBaseScriptFuncType()) {
                if (!script_func_verify.count(prop->GetBaseScriptFuncType())) {
                    WriteLog("Invalid script func type '{}' for property {} in proto {}", prop->GetBaseScriptFuncType(), prop->GetName(), proto->GetName());
                    errors++;
                }
                else if (!script_func_verify[prop->GetBaseScriptFuncType()]) {
                    WriteLog("Verification failed for func type '{}' for property {} in proto {}", prop->GetBaseScriptFuncType(), prop->GetName(), proto->GetName());
                    errors++;
                }
            }
        }
    }

    if (errors != 0) {
        throw ProtoValidationException("Proto resources verification failed");
    }
}
