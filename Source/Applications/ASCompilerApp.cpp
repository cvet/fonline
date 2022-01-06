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

        auto server_failed = false;
        auto client_failed = false;
        auto mapper_failed = false;

        if (!settings.ASServer.empty()) {
            WriteLog("Compile server scripts...\n");

            try {
                ServerScriptSystem().InitAngelScriptScripting(settings.ASServer.c_str());
            }
            catch (std::exception&) {
                server_failed = true;
            }
        }

        if (!settings.ASClient.empty()) {
            WriteLog("Compile client scripts...\n");

            try {
                ClientScriptSystem().InitAngelScriptScripting(settings.ASClient.c_str());
            }
            catch (std::exception&) {
                client_failed = true;
            }
        }

        if (!settings.ASMapper.empty()) {
            WriteLog("Compile mapper scripts...\n");

            try {
                MapperScriptSystem().InitAngelScriptScripting(settings.ASMapper.c_str());
            }
            catch (std::exception&) {
                mapper_failed = true;
            }
        }

        if (!settings.ASServer.empty() && server_failed) {
            WriteLog("Server scripts compilation failed!\n");
        }
        if (!settings.ASClient.empty() && client_failed) {
            WriteLog("Client scripts compilation failed!\n");
        }
        if (!settings.ASMapper.empty() && mapper_failed) {
            WriteLog("Mapper scripts compilation failed!\n");
        }

        return server_failed || client_failed || mapper_failed ? 1 : 0;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}
