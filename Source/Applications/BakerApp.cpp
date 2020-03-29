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
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Version_Include.h"

#include "minizip/zip.h"

static GlobalSettings Settings;

static bool GenerateResources(StrVec* resource_names);

#ifndef FO_TESTING
int main(int argc, char** argv)
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineBaker", FO_VERSION);
    LogToFile("FOnlineBaker.log");
    Settings.ParseArgs(argc, argv);

    return 0;
}

static bool GenerateResources(StrVec* resource_names)
{
    // Generate resources
    bool something_changed = false;
    StrSet update_file_names;

    /*for (const string& project_path : ProjectFiles)
    {
        StrVec dummy_vec;
        StrVec check_dirs;
        File::GetFolderFileNames(project_path, true, "", dummy_vec, nullptr, &check_dirs);
        check_dirs.insert(check_dirs.begin(), "");

        for (const string& check_dir : check_dirs)
        {
            if (!_str(project_path + check_dir).extractLastDir().compareIgnoreCase("Resources"))
                continue;

            string resources_root = project_path + check_dir;
            FindDataVec resources_dirs;
            File::GetFolderFileNames(resources_root, false, "", dummy_vec, nullptr, nullptr, &resources_dirs);
            for (const FindData& resource_dir : resources_dirs)
            {
                const string& res_name = resource_dir.FileName;
                FileCollection resources("", resources_root + res_name + "/");
                if (!resources.GetFilesCount())
                    continue;

                if (res_name.find("_Raw") == string::npos)
                {
                    string res_name_zip = (string)res_name + ".zip";
                    string zip_path = (string) "Update/" + res_name_zip;
                    bool skip_making_zip = true;

                    File::CreateDirectoryTree(zip_path);

                    // Check if file available
                    File zip_file;
                    if (!zip_file.LoadFile(zip_path, true))
                        skip_making_zip = false;

                    // Test consistency
                    if (skip_making_zip)
                    {
                        zipFile zip = zipOpen(zip_path.c_str(), APPEND_STATUS_ADDINZIP);
                        if (!zip)
                            skip_making_zip = false;
                        else
                            zipClose(zip, nullptr);
                    }

                    // Check timestamps of inner resources
                    while (resources.IsNextFile())
                    {
                        string relative_path;
                        File& file = resources.GetNextFile(nullptr, nullptr, &relative_path, true);
                        if (resource_names)
                            resource_names->push_back(relative_path);

                        if (skip_making_zip && file.GetWriteTime() > zip_file.GetWriteTime())
                            skip_making_zip = false;
                    }

                    // Make zip
                    if (!skip_making_zip)
                    {
                        WriteLog("Pack resource '{}', files {}...\n", res_name, resources.GetFilesCount());

                        zipFile zip = zipOpen((zip_path + ".tmp").c_str(), APPEND_STATUS_CREATE);
                        if (zip)
                        {
                            resources.ResetCounter();

                            map<string, UCharVec> baked_files;
                            {
                                ImageBaker image_baker(resources);
                                ModelBaker model_baker(resources);
                                EffectBaker effect_baker(resources);
                                image_baker.AutoBakeImages();
                                model_baker.AutoBakeModels();
                                effect_baker.AutoBakeEffects();
                                image_baker.FillBakedFiles(baked_files);
                                model_baker.FillBakedFiles(baked_files);
                                effect_baker.FillBakedFiles(baked_files);
                            }

                            // Fill other files
                            resources.ResetCounter();
                            while (resources.IsNextFile())
                            {
                                string relative_path;
                                resources.GetNextFile(nullptr, nullptr, &relative_path, true);
                                if (baked_files.count(relative_path))
                                    continue;

                                File& file = resources.GetCurFile();
                                UCharVec data;
                                DataWriter writer {data};
                                writer.WritePtr(file.GetBuf(), file.GetFsize());
                                baked_files.emplace(relative_path, std::move(data));
                            }

                            // Write to zip
                            for (const auto& kv : baked_files)
                            {
                                zip_fileinfo zfi;
                                memzero(&zfi, sizeof(zfi));
                                if (zipOpenNewFileInZip(zip, kv.first.c_str(), &zfi, nullptr, 0, nullptr, 0, nullptr,
                                        Z_DEFLATED, Z_BEST_SPEED) == ZIP_OK)
                                {
                                    if (zipWriteInFileInZip(zip, &kv.second[0], static_cast<uint>(kv.second.size())))
                                        throw GenericException("Can't write file in zip file", kv.first, zip_path);

                                    zipCloseFileInZip(zip);
                                }
                                else
                                {
                                    throw GenericException("Can't open file in zip file", kv.first, zip_path);
                                }
                            }

                            zipClose(zip, nullptr);

                            File::DeleteFile(zip_path);
                            if (!File::RenameFile(zip_path + ".tmp", zip_path))
                                throw GenericException("Can't rename file", zip_path + ".tmp", zip_path);

                            something_changed = true;
                        }
                        else
                        {
                            WriteLog("Can't open zip file '{}'.\n", zip_path);
                        }
                    }

                    update_file_names.insert(res_name_zip);
                }
                else
                {
                    bool log_shown = false;
                    while (resources.IsNextFile())
                    {
                        string path, relative_path;
                        File& file = resources.GetNextFile(nullptr, &path, &relative_path);
                        string fname = "Update/" + relative_path;
                        File update_file;
                        if (!update_file.LoadFile(fname, true) || file.GetWriteTime() > update_file.GetWriteTime())
                        {
                            if (!log_shown)
                            {
                                log_shown = true;
                                WriteLog("Copy resource '{}', files {}...\n", res_name, resources.GetFilesCount());
                            }

                            File* converted_file = nullptr; // Convert(fname, file);
                            if (!converted_file)
                            {
                                WriteLog("File '{}' conversation error.\n", fname);
                                continue;
                            }
                            converted_file->SaveFile(fname);
                            if (converted_file != &file)
                                delete converted_file;

                            something_changed = true;
                        }

                        if (resource_names)
                        {
                            string ext = _str(fname).getFileExtension();
                            if (ext == "zip" || ext == "bos" || ext == "dat")
                            {
                                DataFile* inner = DataFile::TryLoad(path);
                                if (inner)
                                {
                                    StrVec inner_files;
                                    inner->GetFileNames("", true, "", inner_files);
                                    resource_names->insert(
                                        resource_names->end(), inner_files.begin(), inner_files.end());
                                }
                                else
                                {
                                    WriteLog("Can't read data file '{}'.\n", path);
                                }
                            }
                        }

                        update_file_names.insert(relative_path);
                    }
                }
            }
        }
    }

    // Delete unnecessary update files
    FileCollection update_files("", "Update/");
    while (update_files.IsNextFile())
    {
        string path, relative_path;
        update_files.GetNextFile(nullptr, &path, &relative_path, true);
        if (!update_file_names.count(relative_path))
        {
            File::DeleteFile(path);
            something_changed = true;
        }
    }*/

    return something_changed;
}
