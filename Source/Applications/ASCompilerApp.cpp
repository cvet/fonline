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

#include "FileSystem.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"

struct ServerScriptSystem
{
    void InitAngelScriptScripting(const char* script_path);
};

struct ClientScriptSystem
{
    void InitAngelScriptScripting(const char* script_path);
};

struct MapperScriptSystem
{
    void InitAngelScriptScripting(const char* script_path);
};

unordered_set<string> CompilerPassedMessages;

#if !FO_TESTING
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ASCompilerApp(int argc, char** argv) -> int
#endif
{
    try {
        SetAppName("FOnlineASCompiler");
        CatchSystemExceptions();
        CreateGlobalData();
        LogToFile();
        LogWithoutTimestamp();

        const auto settings = GlobalSettings(argc, argv);

        DiskFileSystem::RemoveBuildHashFile("AngelScript");

        auto server_failed = false;
        auto client_failed = false;
        auto mapper_failed = false;

        if (!settings.ASServer.empty()) {
            WriteLog("    Compile server scripts at {}\n", settings.ASServer);

            try {
                ServerScriptSystem().InitAngelScriptScripting(settings.ASServer.c_str());
            }
            catch (std::exception& ex) {
                if (CompilerPassedMessages.empty()) {
                    ReportExceptionAndExit(ex);
                }

                server_failed = true;
                WriteLog("\n");
            }
        }

        if (!settings.ASClient.empty()) {
            WriteLog("    Compile client scripts at {}\n", settings.ASClient);

            try {
                ClientScriptSystem().InitAngelScriptScripting(settings.ASClient.c_str());
            }
            catch (std::exception& ex) {
                if (CompilerPassedMessages.empty()) {
                    ReportExceptionAndExit(ex);
                }

                client_failed = true;
                WriteLog("\n");
            }
        }

        if (!settings.ASMapper.empty()) {
            WriteLog("    Compile mapper scripts at {}\n", settings.ASMapper);

            try {
                MapperScriptSystem().InitAngelScriptScripting(settings.ASMapper.c_str());
            }
            catch (std::exception& ex) {
                if (CompilerPassedMessages.empty()) {
                    ReportExceptionAndExit(ex);
                }

                mapper_failed = true;
                WriteLog("\n");
            }
        }

        WriteLog("\n");

        if (!settings.ASServer.empty()) {
            WriteLog("    Server scripts compilation {}!\n", server_failed ? "failed" : "succeeded");
        }
        if (!settings.ASClient.empty()) {
            WriteLog("    Client scripts compilation {}!\n", client_failed ? "failed" : "succeeded");
        }
        if (!settings.ASMapper.empty()) {
            WriteLog("    Mapper scripts compilation {}!\n", mapper_failed ? "failed" : "succeeded");
        }
        if (settings.ASServer.empty() && settings.ASClient.empty() && settings.ASMapper.empty()) {
            WriteLog("    Nothing to compile!\n");
        }

        WriteLog("\n");

        if (server_failed || client_failed || mapper_failed) {
            return 1;
        }

        DiskFileSystem::CreateBuildHashFile("AngelScript");
        return 0;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
