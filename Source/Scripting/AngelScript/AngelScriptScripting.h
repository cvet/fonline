// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "EngineBase.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

struct AngelScriptSettings;

void InitAngelScriptScripting(ptr<EngineMetadata> meta, const AngelScriptSettings& settings, const FileSystem& resources);
auto CompileAngelScript(ptr<EngineMetadata> meta, const AngelScriptSettings& settings, const vector<File>& files, function<void(string_view)> message_callback) -> vector<uint8_t>;

FO_END_NAMESPACE

#endif
