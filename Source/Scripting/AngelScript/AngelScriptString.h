// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "ScriptSystem.h"

namespace AngelScript
{
    class asIScriptEngine;
}

FO_BEGIN_NAMESPACE

void RegisterAngelScriptString(ptr<AngelScript::asIScriptEngine> as_engine);
void RegisterAngelScriptStringAnyExtensions(ptr<AngelScript::asIScriptEngine> as_engine);

FO_END_NAMESPACE

#endif
