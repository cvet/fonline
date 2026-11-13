// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

namespace AngelScript
{
    class asIScriptContext;
}

FO_BEGIN_NAMESPACE

class AngelScriptBackend;
enum class AngelScriptContextSetupReason : uint8_t;

enum class DebuggerStepMode : uint8_t
{
    None,
    In,
    Over,
    Out,
};

class DebuggerEndpointServer final
{
public:
    explicit DebuggerEndpointServer(ptr<const AngelScriptBackend> backend);
    DebuggerEndpointServer(const DebuggerEndpointServer&) = delete;
    auto operator=(const DebuggerEndpointServer&) = delete;
    DebuggerEndpointServer(DebuggerEndpointServer&&) noexcept = delete;
    auto operator=(DebuggerEndpointServer&&) noexcept = delete;
    ~DebuggerEndpointServer();

    [[nodiscard]] auto IsPaused() const noexcept -> bool;

    void SetupContext(ptr<AngelScript::asIScriptContext> ctx, AngelScriptContextSetupReason reason);
    void EmitEvent(string_view event_name, string_view body_json = "{}");
    void Stop();

private:
    class Impl;
    unique_ptr<Impl> _impl;
};

FO_END_NAMESPACE

#endif
