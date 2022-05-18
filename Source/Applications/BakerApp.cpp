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
#include "Version-Include.h"

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
struct ServerScriptSystem
{
    void InitAngelScriptScripting(const char* script_path);
};
struct ClientScriptSystem
{
    void InitAngelScriptScripting(const char* script_path);
};
#else
struct SingleScriptSystem
{
    void InitAngelScriptScripting(const char* script_path);
};
#endif
struct MapperScriptSystem
{
    void InitAngelScriptScripting(const char* script_path);
};

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

        auto errors = 0;

        // AngelScript scripts
#if FO_ANGELSCRIPT_SCRIPTING
        try {
            WriteLog("Compile AngelScript scripts");

#if !FO_SINGLEPLAYER
            WriteLog("Compile server scripts");
            RUNTIME_ASSERT(!App->Settings.ASServer.empty());
            ServerScriptSystem().InitAngelScriptScripting(App->Settings.ASServer.c_str());

            WriteLog("Compile client scripts");
            RUNTIME_ASSERT(!App->Settings.ASClient.empty());
            ClientScriptSystem().InitAngelScriptScripting(App->Settings.ASClient.c_str());
#else
            WriteLog("Compile game scripts");
            RUNTIME_ASSERT(!App->Settings.ASSingle.empty());
            SingleScriptSystem().InitAngelScriptScripting(App->Settings.ASSingle.c_str());
#endif

            WriteLog("Compile mapper scripts");
            RUNTIME_ASSERT(!App->Settings.ASMapper.empty());
            MapperScriptSystem().InitAngelScriptScripting(App->Settings.ASMapper.c_str());

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

            auto del_raw_ok = DiskFileSystem::DeleteDir("Raw");
            RUNTIME_ASSERT(del_raw_ok);
            DiskFileSystem::MakeDirTree("Raw");

            map<string, vector<string>> res_packs;
            for (const auto& re : App->Settings.BakeResourceEntries) {
                auto re_splitted = _str(re).split(',');
                RUNTIME_ASSERT(re_splitted.size() == 2);
                res_packs[re_splitted[0]].push_back(re_splitted[1]);
            }

            for (auto&& [pack_name, paths] : res_packs) {
                try {
                    WriteLog("Bake {}", pack_name);

                    FileSystem res_files;
                    for (const auto& path : paths) {
                        WriteLog("Add resource pack {} entry '{}'", pack_name, path);
                        res_files.AddDataSource(path);
                    }

                    auto resources = res_files.FilterFiles("");

                    if (pack_name != "Raw") {
                        WriteLog("Create resource pack {} from {} files", pack_name, resources.GetFilesCount());

                        // Cleanup previous
                        const auto del_res_ok = DiskFileSystem::DeleteDir(pack_name);
                        RUNTIME_ASSERT(del_res_ok);
                        const auto del_zip_res_ok = DiskFileSystem::DeleteFile(_str("{}.zip", pack_name));
                        RUNTIME_ASSERT(del_zip_res_ok);

                        // Bake files
                        map<string, vector<uchar>> baked_files;
                        {
                            ImageBaker image_baker(App->Settings, resources);
                            EffectBaker effect_baker(resources);
                            image_baker.AutoBakeImages();
                            effect_baker.AutoBakeEffects();
                            image_baker.FillBakedFiles(baked_files);
                            effect_baker.FillBakedFiles(baked_files);
#if FO_ENABLE_3D
                            ModelBaker model_baker(resources);
                            model_baker.AutoBakeModels();
                            model_baker.FillBakedFiles(baked_files);
#endif
                        }

                        // Raw copy
                        resources.ResetCounter();
                        while (resources.MoveNext()) {
                            auto file_header = resources.GetCurFileHeader();
                            if (baked_files.count(string(file_header.GetPath())) != 0u) {
                                continue;
                            }

                            const auto ext = _str(file_header.GetPath()).getFileExtension().str();
                            if (std::find(App->Settings.BakeExtraFileExtensions.begin(), App->Settings.BakeExtraFileExtensions.end(), ext) == App->Settings.BakeExtraFileExtensions.end()) {
                                continue;
                            }

                            baked_files.emplace(file_header.GetPath(), resources.GetCurFile().GetData());
                        }

                        // Write to disk
                        DiskFileSystem::MakeDirTree(pack_name);

                        for (auto&& [path, data] : baked_files) {
                            auto res_file = DiskFileSystem::OpenFile(_str("{}/{}", pack_name, path), true);
                            RUNTIME_ASSERT(res_file);
                            auto res_file_write_ok = res_file.Write(data);
                            RUNTIME_ASSERT(res_file_write_ok);
                            resource_names.emplace(path);
                        }
                    }
                    else {
                        WriteLog("Copy raw {} resource files", resources.GetFilesCount());

                        while (resources.MoveNext()) {
                            auto file = resources.GetCurFile();
                            auto raw_file = DiskFileSystem::OpenFile(_str("Raw/{}", file.GetPath()), true);
                            RUNTIME_ASSERT(raw_file);
                            auto raw_file_write_ok = raw_file.Write(file.GetBuf(), file.GetSize());
                            RUNTIME_ASSERT(raw_file_write_ok);
                            resource_names.emplace(file.GetPath());
                        }
                    }

                    WriteLog("Bake {} complete!", pack_name);
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

                auto del_protos_ok = DiskFileSystem::DeleteDir("Protos");
                RUNTIME_ASSERT(del_protos_ok);

                proto_mngr.ParseProtos(engine.FileSys);
                proto_mngr.ValidateProtoResources(resource_names);
                auto data = proto_mngr.GetProtosBinaryData();
                RUNTIME_ASSERT(!data.empty());

                auto protos_file = DiskFileSystem::OpenFile("Protos/Protos.foprob", true);
                RUNTIME_ASSERT(protos_file);
                auto protos_write_ok = protos_file.Write(data.data(), data.size());
                RUNTIME_ASSERT(protos_write_ok);

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

                auto del_dialogs_ok = DiskFileSystem::DeleteDir("Dialogs");
                RUNTIME_ASSERT(del_dialogs_ok);

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
