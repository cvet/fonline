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

// Todo: sound and video preprocessing move to baker

#include "Common.h"

#include "DiskFileSystem.h"
#include "EffectBaker.h"
#include "FileSystem.h"
#include "ImageBaker.h"
#include "Log.h"
#include "ModelBaker.h"
#include "ProtoManager.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"

#include "minizip/zip.h"

#if !FO_TESTING
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto BakerApp(int argc, char** argv) -> int
#endif
{
    try {
        SetAppName("Baker");
        CatchSystemExceptions();
        CreateGlobalData();
        LogToFile();

        auto settings = GlobalSettings(argc, argv);

        DiskFileSystem::RemoveBuildHashFile("Resources");

        // Content
        if (!settings.ContentEntry.empty()) {
            WriteLog("Bake content.\n");

            FileSystem content_files;
            for (const auto& dir : settings.ContentEntry) {
                WriteLog("Add content entry '{}'.\n", dir);
                content_files.AddDataSource(dir, true);
            }

            // Todo: bake prototypes?
            // ProtoManager proto_mngr(content_files, ...);
            // auto data = proto_mngr.GetProtosBinaryData();
            // RUNTIME_ASSERT(!data.empty());
            // auto protos_file = DiskFileSystem::OpenFile("Protos.fobin", true);
            // RUNTIME_ASSERT(protos_file);
            // auto protos_write_ok = protos_file.Write(data.data(), static_cast<uint>(data.size()));
            // RUNTIME_ASSERT(protos_write_ok);

            // Dialogs
            // Todo: add dialogs verification during baking
            auto del_dialogs_ok = DiskFileSystem::DeleteDir("Dialogs");
            RUNTIME_ASSERT(del_dialogs_ok);
            auto dialogs = content_files.FilterFiles("fodlg");
            while (dialogs.MoveNext()) {
                auto file = dialogs.GetCurFile();
                auto dlg_file = DiskFileSystem::OpenFile(_str("Dialogs/{}.fodlg", file.GetName()), true);
                RUNTIME_ASSERT(dlg_file);
                auto dlg_file_write_ok = dlg_file.Write(file.GetBuf(), file.GetSize());
                RUNTIME_ASSERT(dlg_file_write_ok);
            }

            // Texts
            // LanguagePack lang;
            // lang.LoadFromFiles(content_files);
            // Raw Texts
            // Texts.fobin
        }

        // Resources
        if (!settings.ResourcesEntry.empty()) {
            WriteLog("Bake resources.\n");

            auto del_raw_ok = DiskFileSystem::DeleteDir("Raw");
            RUNTIME_ASSERT(del_raw_ok);
            DiskFileSystem::MakeDirTree("Raw");

            map<string, vector<string>> res_packs;
            for (const auto& re : settings.ResourcesEntry) {
                auto re_splitted = _str(re).split(',');
                RUNTIME_ASSERT(re_splitted.size() == 2);
                res_packs[re_splitted[0]].push_back(re_splitted[1]);
            }

            for (const auto& [pack_name, paths] : res_packs) {
                if (pack_name == "Embedded") {
                    continue;
                }

                FileSystem res_files;
                for (const auto& path : paths) {
                    WriteLog("Add resource pack '{}' entry '{}'.\n", pack_name, path);
                    res_files.AddDataSource(path, true);
                }

                auto resources = res_files.FilterFiles("");

                if (pack_name != "Raw") {
                    WriteLog("Create resource pack '{}' from {} files.\n", pack_name, resources.GetFilesCount());

                    // Cleanup previous
                    const auto del_res_ok = DiskFileSystem::DeleteDir(pack_name);
                    RUNTIME_ASSERT(del_res_ok);
                    const auto del_zip_res_ok = DiskFileSystem::DeleteFile(_str("{}.zip", pack_name));
                    RUNTIME_ASSERT(del_zip_res_ok);

                    // Bake files
                    map<string, vector<uchar>> baked_files;
                    {
                        ImageBaker image_baker(settings, resources);
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
                    DiskFileSystem::MakeDirTree(pack_name);

                    for (const auto& [path, data] : baked_files) {
                        auto res_file = DiskFileSystem::OpenFile(_str("{}/{}", pack_name, path), true);
                        RUNTIME_ASSERT(res_file);
                        auto res_file_write_ok = res_file.Write(data);
                        RUNTIME_ASSERT(res_file_write_ok);
                    }

                    // Make zip
                    zipFile zip = zipOpen(_str("{}.zip", pack_name).c_str(), APPEND_STATUS_CREATE);
                    RUNTIME_ASSERT(zip);

                    for (const auto& [path, data] : baked_files) {
                        zip_fileinfo zfi = {};
#if FO_DEBUG
                        constexpr int level = Z_BEST_SPEED;
#else
                        constexpr int level = Z_BEST_COMPRESSION;
#endif
                        const auto zip_new_file = zipOpenNewFileInZip(zip, path.c_str(), &zfi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, level);
                        RUNTIME_ASSERT(zip_new_file == ZIP_OK);
                        const auto zip_write_file = zipWriteInFileInZip(zip, data.data(), static_cast<unsigned>(data.size()));
                        RUNTIME_ASSERT(zip_write_file == ZIP_OK);
                        const auto zip_close_file = zipCloseFileInZip(zip);
                        RUNTIME_ASSERT(zip_close_file == ZIP_OK);
                    }

                    const auto zip_close = zipClose(zip, nullptr);
                    RUNTIME_ASSERT(zip_close == ZIP_OK);
                }
                else {
                    WriteLog("Copy raw {} resource files.\n", resources.GetFilesCount());

                    while (resources.MoveNext()) {
                        auto file = resources.GetCurFile();
                        auto raw_file = DiskFileSystem::OpenFile(_str("Raw/{}", file.GetPath()), true);
                        RUNTIME_ASSERT(raw_file);
                        auto raw_file_write_ok = raw_file.Write(file.GetBuf(), file.GetSize());
                        RUNTIME_ASSERT(raw_file_write_ok);
                    }
                }
            }
        }

        WriteLog("Bakering complete!\n");

        DiskFileSystem::CreateBuildHashFile("Resources");
        return 0;
    }
    catch (std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
