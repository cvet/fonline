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
#include "Version_Include.h"

#include "minizip/zip.h"

static GlobalSettings Settings;

struct ServerScriptSystem
{
    void InitAngelScriptScripting(const string& script_path);
};

struct ClientScriptSystem
{
    void InitAngelScriptScripting(const string& script_path);
};

struct MapperScriptSystem
{
    void InitAngelScriptScripting(const string& script_path);
};

#ifndef FO_TESTING
int main(int argc, char** argv)
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineBaker", FO_VERSION);
    LogToFile("FOnlineBaker.log");
    Settings.ParseArgs(argc, argv);

    int errors = 0;

    if (!Settings.ASServer.empty())
    {
        try
        {
            ServerScriptSystem().InitAngelScriptScripting(Settings.ASServer);
        }
        catch (std::exception& ex)
        {
            WriteLog("Server scripts compilation failed:\n{}\n", ex.what());
            errors++;
        }
    }

    if (!Settings.ASClient.empty())
    {
        try
        {
            ClientScriptSystem().InitAngelScriptScripting(Settings.ASClient);
        }
        catch (std::exception& ex)
        {
            WriteLog("Client scripts compilation failed:\n{}\n", ex.what());
            errors++;
        }
    }

    if (!Settings.ASMapper.empty())
    {
        try
        {
            MapperScriptSystem().InitAngelScriptScripting(Settings.ASMapper);
        }
        catch (std::exception& ex)
        {
            WriteLog("Mapper scripts compilation failed:\n{}\n", ex.what());
            errors++;
        }
    }

    return errors;
}
