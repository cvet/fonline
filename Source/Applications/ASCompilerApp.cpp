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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptBaker.h"
#include "Application.h"
#include "FileSystem.h"
#include "MetadataBaker.h"
#include "ScriptSystem.h"
#include "Settings.h"

FO_USING_NAMESPACE();

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ASCompilerApp(int argc, char** argv) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

    try {
        InitApp(numeric_cast<int32>(argc), argv, AppInitFlags::DisableLogTags);

        FO_RUNTIME_ASSERT(!App->Settings.BakeOutput.empty());

        WriteLog("Prepare metadata");
        FileSystem metadata_files;

        for (const auto& res_pack : App->Settings.GetResourcePacks()) {
            if (!vec_exists(res_pack.Bakers, MetadataBaker::NAME)) {
                continue;
            }

            FileSystem res_files;

            for (const auto& dir : res_pack.InputDirs) {
                res_files.AddDirSource(dir, res_pack.RecursiveInput);
            }

            const auto write_file = [&](string_view path, const_span<uint8> data) FO_DEFERRED {
                auto file = DiskFileSystem::OpenFile(strex(App->Settings.BakeOutput).combine_path(res_pack.Name).combine_path(path), true);
                FO_RUNTIME_ASSERT(file);
                const auto write_ok = file.Write(data);
                FO_RUNTIME_ASSERT(write_ok);
            };

            auto baking_ctx = SafeAlloc::MakeShared<BakingContext>(BakingContext {.Settings = &App->Settings, .PackName = res_pack.Name, .WriteData = write_file, .ForceSyncMode = true});
            auto metadata_baker = MetadataBaker(std::move(baking_ctx));

            try {
                metadata_baker.BakeFiles(res_files.GetAllFiles(), "");
                metadata_files.AddDirSource(strex(App->Settings.BakeOutput).combine_path(res_pack.Name), false);
            }
            catch (const MetadataBakerException& ex) {
                const auto& params = ex.params();

                if (params.size() >= 2 && !params.front().empty()) {
                    WriteLog("{}", strex("{}({},{}): {} : {}", params[0], strex(params[1]).to_int64(), 0, "error", ex.message()));
                }
                else {
                    WriteLog("{}", ex.what());
                }

                WriteLog("Metadata preparing failed!");
                ExitApp(false);
            }
            catch (const std::exception& ex) {
                WriteLog("{}", ex.what());
                WriteLog("Metadata preparing failed!");
                ExitApp(false);
            }
        }

        WriteLog("Compile scripts");

        for (const auto& res_pack : App->Settings.GetResourcePacks()) {
            if (!vec_exists(res_pack.Bakers, AngelScriptBaker::NAME)) {
                continue;
            }

            FileSystem res_files;

            for (const auto& dir : res_pack.InputDirs) {
                res_files.AddDirSource(dir, res_pack.RecursiveInput);
            }

            const auto write_file = [&](string_view path, const_span<uint8> data) FO_DEFERRED {
                auto file = DiskFileSystem::OpenFile(strex(App->Settings.BakeOutput).combine_path(res_pack.Name).combine_path(path), true);
                FO_RUNTIME_ASSERT(file);
                const auto write_ok = file.Write(data);
                FO_RUNTIME_ASSERT(write_ok);
            };

            auto baking_ctx = SafeAlloc::MakeShared<BakingContext>(BakingContext {.Settings = &App->Settings, .PackName = res_pack.Name, .WriteData = write_file, .BakedFiles = &metadata_files, .ForceSyncMode = true});
            auto scripts_baker = AngelScriptBaker(std::move(baking_ctx));

            try {
                scripts_baker.BakeFiles(res_files.GetAllFiles(), "");
            }
            catch (const std::exception&) {
                WriteLog("Scripts compilation failed!");
                ExitApp(false);
            }
        }

        WriteLog("Scripts compilation succeeded!");
        ExitApp(true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

#endif
