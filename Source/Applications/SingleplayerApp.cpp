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

#include "Client.h"
#include "Networking.h"
#include "ScriptSystem.h"
#include "Server.h"
#include "Settings.h"
#include "Testing.h"
#include "Version_Include.h"

NetServerBase* NetServerBase::StartTcpServer(ServerNetworkSettings& settings, ConnectionCallback callback)
{
    throw UnreachablePlaceException(LINE_STR);
}
NetServerBase* NetServerBase::StartWebSocketsServer(ServerNetworkSettings& settings, ConnectionCallback callback)
{
    throw UnreachablePlaceException(LINE_STR);
}

void ClientScriptSystem::InitNativeScripting()
{
}
void ClientScriptSystem::InitAngelScriptScripting()
{
}
void ClientScriptSystem::InitMonoScripting()
{
}

static GlobalSettings Settings;

#ifndef FO_TESTING
extern "C" int main(int argc, char** argv)
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineServerHeadless", FO_VERSION);
    LogToFile("FOnlineServerHeadless.log");
    Settings.ParseArgs(argc, argv);

    FOClient* client = new FOClient(Settings);
    FOServer* server = new FOServer(Settings);
    return server->RunAsSingleplayer(client);
}
