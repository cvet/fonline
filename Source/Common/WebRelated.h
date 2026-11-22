// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

struct AppScreenState;
struct WebSettings;

namespace WebRelated
{
    inline constexpr string_view_nt CanvasSelector {"#canvas"};

    void ApplyApplicationHints() noexcept;
    void ApplyWindowSettings(WebSettings& settings, AppScreenState& screen);
    void ApplyCanvasLayout(WebSettings& settings, const AppScreenState& screen) noexcept;
    void SetupClipboard();
    void SyncClipboardToSystem(string_view text);
    void InitializePersistentData();
    void FlushPersistentData();
    auto IsPersistentDataReady() noexcept -> bool;
    void StartMainLoop(void (*entry)(void*), void* data) noexcept;
    void StopMainLoop() noexcept;
    void SetWebSocketScheme(bool secure) noexcept;
    auto GetUserAgent() -> string;
    void ShowError(string_view title, string_view text);
}

FO_END_NAMESPACE
