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

#ifdef FO_SERVER_SCRIPTING
#include "ServerScripting.h"
#endif
#ifdef FO_CLIENT_SCRIPTING
#include "ClientScripting.h"
#endif
#ifdef FO_MAPPER_SCRIPTING
#include "MapperScripting.h"
#endif

#ifdef FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptReflection.h"

#include "angelscript.h"
#include "datetime/datetime.h"
#include "preprocessor.h"
#include "scriptany/scriptany.h"
#include "scriptfile/scriptfile.h"
#include "scriptfile/scriptfilesystem.h"
#include "scripthelper/scripthelper.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"
#include "weakref/weakref.h"


#ifdef FO_SERVER_SCRIPTING
void ServerScriptSystem::InitAngelScriptScripting(FOServer& server)
#endif
#ifdef FO_CLIENT_SCRIPTING
    void ClientScriptSystem::InitAngelScriptScripting(FOClient& client)
#endif
#ifdef FO_MAPPER_SCRIPTING
        void MapperScriptSystem::InitAngelScriptScripting(FOMapper& mapper)
#endif
{
}

#else
#ifdef FO_SERVER_SCRIPTING
void ServerScriptSystem::InitAngelSriptScripting(FOServer& server)
#endif
#ifdef FO_CLIENT_SCRIPTING
    void ClientScriptSystem::InitAngelSriptScripting(FOClient& client)
#endif
#ifdef FO_MAPPER_SCRIPTING
        void MapperScriptSystem::InitAngelSriptScripting(FOMapper& mapper)
#endif
{
}
#endif
