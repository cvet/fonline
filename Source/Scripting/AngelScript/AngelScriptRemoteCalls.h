// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

namespace AngelScript
{
    class asIScriptEngine;
    class asIScriptModule;
}

namespace Preprocessor
{
    class LineNumberTranslator;
}

FO_BEGIN_NAMESPACE

class EngineMetadata;

void RegisterAngelScriptRemoteCalls(ptr<AngelScript::asIScriptEngine> as_engine);
void BindAngelScriptRemoteCalls(ptr<AngelScript::asIScriptEngine> as_engine);
auto ValidateAngelScriptRemoteCallAttributes(ptr<const AngelScript::asIScriptModule> mod, const EngineMetadata& meta, nptr<const Preprocessor::LineNumberTranslator> lnt = nullptr) -> string;

FO_END_NAMESPACE

#endif
