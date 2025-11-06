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
#include "Log.h"
#include "ScriptSystem.h"
#include "Settings.h"

#if !FO_SINGLEPLAYER
struct AngelScriptCompiler_ServerScriptSystem final : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources);
};
struct AngelScriptCompiler_ClientScriptSystem final : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources);
};
#else
struct AngelScriptCompiler_SingleScriptSystem final : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources);
};
#endif
struct AngelScriptCompiler_MapperScriptSystem final : public ScriptSystem
{
    void InitAngelScriptScripting(const FileSystem& resources);
};

unordered_set<string> CompilerPassedMessages;

#if !FO_TESTING_APP
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ASCompilerApp(int argc, char** argv) -> int
#endif
{
    STACK_TRACE_ENTRY();

    try {
        InitApp(argc, argv);
        LogDisableTags();

#if !FO_SINGLEPLAYER
        auto server_failed = false;
        auto client_failed = false;
#else
        auto single_failed = false;
#endif
        auto mapper_failed = false;

        FileSystem resources;
        for (const auto& dir : App->Settings.BakeContentEntries) {
            resources.AddDataSource(dir, DataSourceType::DirRoot);
        }

#if !FO_SINGLEPLAYER
        WriteLog("Compile server scripts");

        try {
            AngelScriptCompiler_ServerScriptSystem().InitAngelScriptScripting(resources);
        }
        catch (const std::exception& ex) {
            if (CompilerPassedMessages.empty()) {
                ReportExceptionAndExit(ex);
            }

            server_failed = true;
        }

        WriteLog("Compile client scripts");

        try {
            AngelScriptCompiler_ClientScriptSystem().InitAngelScriptScripting(resources);
        }
        catch (const std::exception& ex) {
            if (CompilerPassedMessages.empty()) {
                ReportExceptionAndExit(ex);
            }

            client_failed = true;
        }
#else
        WriteLog("Compile game scripts");

        try {
            AngelScriptCompiler_SingleScriptSystem().InitAngelScriptScripting(resources);
        }
        catch (const std::exception& ex) {
            if (CompilerPassedMessages.empty()) {
                ReportExceptionAndExit(ex);
            }

            single_failed = true;
        }
#endif

        WriteLog("Compile mapper scripts");

        try {
            AngelScriptCompiler_MapperScriptSystem().InitAngelScriptScripting(resources);
        }
        catch (const std::exception& ex) {
            if (CompilerPassedMessages.empty()) {
                ReportExceptionAndExit(ex);
            }

            mapper_failed = true;
        }

#if !FO_SINGLEPLAYER
        WriteLog("Server scripts compilation {}!", server_failed ? "failed" : "succeeded");
        WriteLog("Client scripts compilation {}!", client_failed ? "failed" : "succeeded");
#else
        WriteLog("Game scripts compilation {}!", single_failed ? "failed" : "succeeded");
#endif
        WriteLog("Mapper scripts compilation {}!", mapper_failed ? "failed" : "succeeded");

#if !FO_SINGLEPLAYER
        if (server_failed || client_failed || mapper_failed) {
#else
        if (single_failed || mapper_failed) {
#endif
            ExitApp(false);
        }

        ExitApp(true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }
}
