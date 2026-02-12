//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptContext.h"
#include "ScriptSystem.h"

#include <angelscript.h>

class ScriptArray;

FO_BEGIN_NAMESPACE

class AngelScriptBackend : public ScriptSystemBackend
{
public:
    AngelScriptBackend();
    AngelScriptBackend(const AngelScriptBackend&) noexcept = delete;
    auto operator=(const AngelScriptBackend&) noexcept -> AngelScriptBackend& = delete;
    AngelScriptBackend(AngelScriptBackend&&) noexcept = delete;
    auto operator=(AngelScriptBackend&&) noexcept -> AngelScriptBackend& = delete;
    ~AngelScriptBackend() override;

    [[nodiscard]] auto GetMetadata() const noexcept -> const EngineMetadata* { return _meta.get(); }
    [[nodiscard]] auto GetScriptSys() const noexcept -> const ScriptSystem* { return _scriptSys.get(); }
    [[nodiscard]] auto HasGameEngine() const noexcept -> bool { return !!_engine; } // Not present in baker/compiler
    [[nodiscard]] auto GetGameEngine() -> BaseEngine*; // Assert if not specified
    [[nodiscard]] auto HasEntityMngr() const noexcept -> bool { return !!_entityMngr; } // Not present on client
    [[nodiscard]] auto GetEntityMngr() -> EntityManagerApi*; // Assert if not specified
    [[nodiscard]] auto GetContextMngr() noexcept -> AngelScriptContextManager* { return _contextMngr.get(); }
    [[nodiscard]] auto GetContextMngr() const noexcept -> const AngelScriptContextManager* { return _contextMngr.get(); }

    void RegisterMetadata(EngineMetadata* meta);
    void SetMessageCallback(function<void(string_view)> message_callback);
    void SendMessage(string_view message) const;
    void LoadBinaryScripts(const FileSystem& resources);
    auto CompileTextScripts(const vector<File>& files) -> vector<uint8>;
    void BindRequiredStuff();
    void AddCleanupCallback(function<void()> callback);

private:
    raw_ptr<EngineMetadata> _meta {};
    raw_ptr<ScriptSystem> _scriptSys {}; // Maybe null
    raw_ptr<BaseEngine> _engine {}; // Maybe null
    raw_ptr<EntityManagerApi> _entityMngr {}; // Maybe null
    raw_ptr<AngelScript::asIScriptEngine> _asEngine {};
    unique_ptr<AngelScriptContextManager> _contextMngr {};
    function<void(string_view)> _messageCallback {};
    vector<function<void()>> _cleanupCallbacks {};
};

FO_END_NAMESPACE

#endif
