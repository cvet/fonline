//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "EffectBaker.h"
#include "FileSystem.h"
#include "ImageBaker.h"
#include "Log.h"
#include "ModelBaker.h"
#include "ProtoManager.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Version_Include.h"

static GlobalSettings Settings;

#ifndef FO_TESTING
int main(int argc, char** argv)
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineBaker", FO_VERSION);
    LogToFile("FOnlineBaker.log");
    Settings.ParseArgs(argc, argv);

    try
    {
        DiskFileSystem::MakeDirTree(Settings.ResourcesOutput);
        bool set_res_dir_ok = DiskFileSystem::SetCurrentDir(Settings.ResourcesOutput);
        RUNTIME_ASSERT(set_res_dir_ok);

        // Content
        if (!Settings.ContentEntry.empty())
        {
            WriteLog("Bake content.\n");

            FileManager content_files;
            for (const string& dir : Settings.ContentEntry)
            {
                WriteLog("Add content entry '{}'.\n", dir);
                content_files.AddDataSource(dir, true);
            }

            // Protos
            ProtoManager proto_mngr;
            proto_mngr.LoadProtosFromFiles(content_files);
            UCharVec data = proto_mngr.GetProtosBinaryData();
            RUNTIME_ASSERT(!data.empty());
            DiskFile protos_file = DiskFileSystem::OpenFile("Protos.fobin", true);
            RUNTIME_ASSERT(protos_file);
            bool protos_write_ok = protos_file.Write(data.data(), (uint)data.size());
            RUNTIME_ASSERT(protos_write_ok);

            // Dialogs
            // Todo: add dialogs verification during baking
            bool del_dialogs_ok = DiskFileSystem::DeleteDir("Dialogs");
            RUNTIME_ASSERT(del_dialogs_ok);
            FileCollection dialogs = content_files.FilterFiles("fodlg");
            while (dialogs.MoveNext())
            {
                File file = dialogs.GetCurFile();
                DiskFile dlg_file = DiskFileSystem::OpenFile(_str("Dialogs/{}.fodlg", file.GetName()), true);
                RUNTIME_ASSERT(dlg_file);
                bool dlg_file_write_ok = dlg_file.Write(file.GetBuf(), file.GetFsize());
                RUNTIME_ASSERT(dlg_file_write_ok);
            }

            // Texts
            // LanguagePack lang;
            // lang.LoadFromFiles(content_files);
            // Raw Texts
            // Texts.fobin
        }

        // Resources
        if (!Settings.ResourcesEntry.empty())
        {
            WriteLog("Bake resources.\n");

            map<string, vector<string>> res_packs;
            for (const string& re : Settings.ResourcesEntry)
            {
                StrVec re_splitted = _str(re).split(',');
                RUNTIME_ASSERT(re_splitted.size() == 2);
                res_packs[re_splitted[0]].push_back(re_splitted[1]);
            }

            for (const auto& kv : res_packs)
            {
                const string& pack_name = kv.first;

                FileManager res_files;
                for (const string& path : kv.second)
                {
                    WriteLog("Add resource pack '{}' entry '{}'.\n", pack_name, path);
                    res_files.AddDataSource(path, true);
                }

                FileCollection resources = res_files.FilterFiles("");

                if (pack_name != "_Raw")
                {
                    WriteLog("Create resources '{}' from files {}...\n", pack_name, resources.GetFilesCount());

                    // Bake files
                    map<string, UCharVec> baked_files;
                    {
                        ImageBaker image_baker(Settings, resources);
                        ModelBaker model_baker(resources);
                        EffectBaker effect_baker(resources);
                        image_baker.AutoBakeImages();
                        model_baker.AutoBakeModels();
                        effect_baker.AutoBakeEffects();
                        image_baker.FillBakedFiles(baked_files);
                        model_baker.FillBakedFiles(baked_files);
                        effect_baker.FillBakedFiles(baked_files);
                    }

                    // Write to disk
                    bool del_res_ok = DiskFileSystem::DeleteDir(_str("Pack_{}", pack_name));
                    RUNTIME_ASSERT(del_res_ok);

                    for (const auto& kv : baked_files)
                    {
                        DiskFile res_file = DiskFileSystem::OpenFile(_str("Pack_{}/{}", pack_name, kv.first), true);
                        RUNTIME_ASSERT(res_file);
                        bool res_file_write_ok = res_file.Write(kv.second.data(), (uint)kv.second.size());
                        RUNTIME_ASSERT(res_file_write_ok);
                    }
                }
                else
                {
                    WriteLog("Copy raw resource files {}...\n", resources.GetFilesCount());

                    bool del_raw_ok = DiskFileSystem::DeleteDir("Raw");
                    RUNTIME_ASSERT(del_raw_ok);

                    while (resources.MoveNext())
                    {
                        File file = resources.GetCurFile();
                        DiskFile raw_file = DiskFileSystem::OpenFile(_str("Raw/{}", file.GetPath()), true);
                        RUNTIME_ASSERT(raw_file);
                        bool raw_file_write_ok = raw_file.Write(file.GetBuf(), file.GetFsize());
                        RUNTIME_ASSERT(raw_file_write_ok);
                    }
                }
            }
        }
    }
    catch (std::exception& ex)
    {
        ReportException(ex);
        return 1;
    }

    return 0;
}
