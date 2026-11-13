// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptContext.h"
#include "AngelScriptDebugger.h"
#include "ScriptSystem.h"

class ScriptArray;

FO_BEGIN_NAMESPACE

struct AngelScriptSettings;

class AngelScriptBackend : public ScriptSystemBackend
{
public:
    explicit AngelScriptBackend(ptr<const AngelScriptSettings> settings);
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
    [[nodiscard]] auto GetContextMngr() noexcept -> nptr<AngelScriptContextManager> { return _contextMngr ? make_nptr(&*_contextMngr) : nullptr; }
    [[nodiscard]] auto GetContextMngr() const noexcept -> nptr<const AngelScriptContextManager> { return _contextMngr ? make_nptr(&*_contextMngr) : nullptr; }
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
    auto InternUserString(string_view name) -> ptr<const string>;

private:
    static auto TryParseModuleFuncPriority(string_view raw_attribute, string_view attribute_name, int32_t& priority) noexcept -> bool;

    ptr<const AngelScriptSettings> _settings;
    nptr<EngineMetadata> _meta {};
    nptr<ScriptSystem> _scriptSys {}; // Maybe null
    nptr<BaseEngine> _engine {}; // Maybe null
    nptr<EntityManagerApi> _entityMngr {}; // Maybe null
    nptr<AngelScript::asIScriptEngine> _asEngine {};
    optional<AngelScriptContextManager> _contextMngr {};
    function<void(string_view)> _messageCallback {};
    vector<function<void()>> _cleanupCallbacks {};
    vector<function<void()>> _postCleanupCallbacks {};
    optional<DebuggerEndpointServer> _debuggerEndpointServer {};
    std::atomic_int32_t _exceptionCounter {};
    set<string> _userStrings {};
};

FO_END_NAMESPACE

#endif
