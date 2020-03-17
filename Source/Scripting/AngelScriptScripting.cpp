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
#define SCRIPTING_CLASS ServerScriptSystem
#endif
#ifdef FO_CLIENT_SCRIPTING
#include "ClientScripting.h"
#define SCRIPTING_CLASS ClientScriptSystem
#endif
#ifdef FO_MAPPER_SCRIPTING
#include "MapperScripting.h"
#define SCRIPTING_CLASS MapperScriptSystem
#endif

#ifdef FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptExtensions.h"
#include "AngelScriptReflection.h"
#include "AngelScriptScriptDict.h"
#include "Log.h"
#include "Testing.h"

#include "angelscript.h"
#include "datetime/datetime.h"
#include "preprocessor.h"
#include "scriptany/scriptany.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "scriptfile/scriptfile.h"
#include "scriptfile/scriptfilesystem.h"
#include "scripthelper/scripthelper.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"
#include "weakref/weakref.h"

struct ScriptSystem::AngelScriptImpl
{
    asIScriptEngine* Engine {};
};

static const string ContextStatesStr[] = {
    "Finished",
    "Suspended",
    "Aborted",
    "Exception",
    "Prepared",
    "Uninitialized",
    "Active",
    "Error",
};

static void CallbackMessage(const asSMessageInfo* msg, void* param)
{
    const char* type = "Error";
    if (msg->type == asMSGTYPE_WARNING)
        type = "Warning";
    else if (msg->type == asMSGTYPE_INFORMATION)
        type = "Info";

    WriteLog("{} : {} : {} : Line {}.\n", Preprocessor::ResolveOriginalFile(msg->row), type, msg->message,
        Preprocessor::ResolveOriginalLine(msg->row));
}

void SCRIPTING_CLASS::InitAngelScriptScripting()
{
    pAngelScriptImpl = std::make_unique<AngelScriptImpl>();

    asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    RUNTIME_ASSERT(engine);
    pAngelScriptImpl->Engine = engine;
    // asEngine->ShutDownAndRelease();

    engine->SetMessageCallback(asFUNCTION(CallbackMessage), nullptr, asCALL_CDECL);

    engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true);
    engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true);
    engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true);
    engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
    engine->SetEngineProperty(asEP_DISALLOW_EMPTY_LIST_ELEMENTS, true);
    engine->SetEngineProperty(asEP_PRIVATE_PROP_AS_PROTECTED, true);
    engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, true);
    engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true);
    engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);

    RegisterScriptArray(engine, true);
    ScriptExtensions::RegisterScriptArrayExtensions(engine);
    RegisterStdString(engine);
    ScriptExtensions::RegisterScriptStdStringExtensions(engine);
    RegisterScriptAny(engine);
    RegisterScriptDictionary(engine);
    RegisterScriptDict(engine);
    ScriptExtensions::RegisterScriptDictExtensions(engine);
    RegisterScriptFile(engine);
    RegisterScriptFileSystem(engine);
    RegisterScriptDateTime(engine);
    RegisterScriptMath(engine);
    RegisterScriptWeakRef(engine);
    RegisterScriptReflection(engine);

    engine->SetUserData(mainObj);
}

#else
struct ScriptSystem::AngelScriptImpl
{
};
void SCRIPTING_CLASS::InitAngelScriptScripting()
{
}
#endif
