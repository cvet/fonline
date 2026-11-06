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

FO_BEGIN_NAMESPACE

class EngineMetadata;

namespace AngelScriptDocumentMigrations
{
    void RegisterTypes(ptr<AngelScript::asIScriptEngine> as_engine);
    void Validate(ptr<AngelScript::asIScriptModule> module, const EngineMetadata& meta);
    void Bind(ptr<AngelScript::asIScriptEngine> as_engine);
}

FO_END_NAMESPACE

#endif
