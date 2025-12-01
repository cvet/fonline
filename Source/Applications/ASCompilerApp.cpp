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

#include "Common.h"

#include "Application.h"
#include "FileSystem.h"
#include "ScriptSystem.h"
#include "Settings.h"

FO_USING_NAMESPACE();

FO_BEGIN_NAMESPACE();
extern auto Init_AngelScriptCompiler_ServerScriptSystem(const vector<File>&) -> vector<uint8>;
extern auto Init_AngelScriptCompiler_ClientScriptSystem(const vector<File>&) -> vector<uint8>;
extern auto Init_AngelScriptCompiler_MapperScriptSystem(const vector<File>&) -> vector<uint8>;

unordered_set<string> CompilerPassedMessages;
FO_END_NAMESPACE();

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

        bool something_failed = false;

        for (const auto& res_pack : App->Settings.GetResourcePacks()) {
            FileSystem res_files;

            for (const auto& dir : res_pack.InputDir) {
                res_files.AddDirSource(dir, res_pack.RecursiveInput);
            }

            auto script_files_collection = res_files.FilterFiles("fos");

            if (script_files_collection.GetFilesCount() == 0) {
                continue;
            }

            vector<File> script_files;

            for (const auto& file_header : script_files_collection) {
                script_files.emplace_back(File::Load(file_header));
            }

            bool server_failed = false;
            bool client_failed = false;
            bool mapper_failed = false;

            const auto write_file = [&](span<const uint8> data, string_view ext) {
                const string path = strex("{}/{}.{}", res_pack.Name, res_pack.Name, ext);
                auto file = DiskFileSystem::OpenFile(strex(App->Settings.BakeOutput).combine_path(path), true);
                FO_RUNTIME_ASSERT(file);
                const auto write_ok = file.Write(data);
                FO_RUNTIME_ASSERT(write_ok);
            };

            WriteLog("Compile server scripts");

            try {
                auto data = Init_AngelScriptCompiler_ServerScriptSystem(script_files);
                write_file(data, "fos-bin-server");
            }
            catch (const std::exception& ex) {
                if (CompilerPassedMessages.empty()) {
                    ReportExceptionAndExit(ex);
                }

                server_failed = true;
            }

            WriteLog("Compile client scripts");

            try {
                auto data = Init_AngelScriptCompiler_ClientScriptSystem(script_files);
                write_file(data, "fos-bin-client");
            }
            catch (const std::exception& ex) {
                if (CompilerPassedMessages.empty()) {
                    ReportExceptionAndExit(ex);
                }

                client_failed = true;
            }

            WriteLog("Compile mapper scripts");

            try {
                auto data = Init_AngelScriptCompiler_MapperScriptSystem(script_files);
                write_file(data, "fos-bin-mapper");
            }
            catch (const std::exception& ex) {
                if (CompilerPassedMessages.empty()) {
                    ReportExceptionAndExit(ex);
                }

                mapper_failed = true;
            }

            WriteLog("Server scripts compilation {}!", server_failed ? "failed" : "succeeded");
            WriteLog("Client scripts compilation {}!", client_failed ? "failed" : "succeeded");
            WriteLog("Mapper scripts compilation {}!", mapper_failed ? "failed" : "succeeded");

            if (server_failed || client_failed || mapper_failed) {
                something_failed = true;
            }
        }

        ExitApp(!something_failed);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}
