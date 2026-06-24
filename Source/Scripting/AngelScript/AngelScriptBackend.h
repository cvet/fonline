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
#include "AngelScriptDebugger.h"
#include "ScriptSystem.h"

class ScriptArray;

FO_BEGIN_NAMESPACE

struct ScriptSettings;

class AngelScriptBackend : public ScriptSystemBackend
{
public:
    explicit AngelScriptBackend(ptr<const ScriptSettings> settings);
    AngelScriptBackend(const AngelScriptBackend&) noexcept = delete;
    auto operator=(const AngelScriptBackend&) noexcept -> AngelScriptBackend& = delete;
    AngelScriptBackend(AngelScriptBackend&&) noexcept = delete;
    auto operator=(AngelScriptBackend&&) noexcept -> AngelScriptBackend& = delete;
    ~AngelScriptBackend() override;

    [[nodiscard]] auto GetMetadata() const noexcept -> nptr<const EngineMetadata> { return _meta; }
    [[nodiscard]] auto GetScriptSys() const noexcept -> nptr<const ScriptSystem> { return _scriptSys; }
    [[nodiscard]] auto HasGameEngine() const noexcept -> bool { return !!_engine; } // Not present in baker/compiler
    [[nodiscard]] auto GetGameEngine() -> ptr<BaseEngine>; // Assert if not specified
    [[nodiscard]] auto GetGameEngine() const -> ptr<const BaseEngine>; // Assert if not specified
    [[nodiscard]] auto HasEntityMngr() const noexcept -> bool { return !!_entityMngr; } // Not present on client
    [[nodiscard]] auto GetEntityMngr() -> ptr<EntityManagerApi>; // Assert if not specified
    [[nodiscard]] auto GetContextMngr() noexcept -> nptr<AngelScriptContextManager> { return _contextMngr ? nptr<AngelScriptContextManager>(&*_contextMngr) : nullptr; }
    [[nodiscard]] auto GetContextMngr() const noexcept -> nptr<const AngelScriptContextManager> { return _contextMngr ? nptr<const AngelScriptContextManager>(&*_contextMngr) : nullptr; }
    [[nodiscard]] auto GetExceptionCounter() const noexcept -> int32_t { return _exceptionCounter.load(); }

    void RegisterMetadata(ptr<EngineMetadata> meta);
    void SetMessageCallback(function<void(string_view)> message_callback);
    void SendMessage(string_view message) const;
    void LoadBinaryScripts(const FileSystem& resources);
    auto CompileTextScripts(const vector<File>& files) -> vector<uint8_t>;
    void BindRequiredStuff();
    void AddCleanupCallback(function<void()> callback);
    void AddPostCleanupCallback(function<void()> callback);
    void IncreaseExceptionCounter() { _exceptionCounter.fetch_add(1); }

private:
    static auto TryParseModuleFuncPriority(string_view raw_attribute, string_view attribute_name, int32_t& priority) noexcept -> bool;

    void ReleaseScriptGlobalsAndReportGC();

    ptr<const ScriptSettings> _settings;
    nptr<EngineMetadata> _meta {};
    nptr<ScriptSystem> _scriptSys {}; // Maybe null
    nptr<BaseEngine> _engine {}; // Maybe null
    nptr<EntityManagerApi> _entityMngr {}; // Maybe null
    nptr<AngelScript::asIScriptEngine> _asEngine {};
    optional<AngelScriptContextManager> _contextMngr {};
    function<void(string_view)> _messageCallback {};
    vector<function<void()>> _cleanupCallbacks {};
    vector<function<void()>> _postCleanupCallbacks {};
    unique_nptr<DebuggerEndpointServer> _debuggerEndpointServer {};
    std::atomic_int32_t _exceptionCounter {};
};

FO_END_NAMESPACE

#endif
