// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

namespace AngelScript
{
    class asIScriptEngine;
}

FO_BEGIN_NAMESPACE

class EngineMetadata;

void RegisterAngelScriptEnums(ptr<AngelScript::asIScriptEngine> as_engine);
void RegisterAngelScriptGlobals(ptr<AngelScript::asIScriptEngine> as_engine);

FO_END_NAMESPACE

#endif
