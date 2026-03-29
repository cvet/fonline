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

#include "WebRelated.h"
#include "Settings.h"

#if FO_WEB
#include "Application.h"

#include "SDL3/SDL_hints.h"

// clang-format off
EM_JS(int, WebGetWindowWidth, (), {
    return window.innerWidth || document.documentElement.clientWidth || document.getElementsByTagName('body')[0].clientWidth;
});

EM_JS(int, WebGetWindowHeight, (), {
    return window.innerHeight || document.documentElement.clientHeight || document.getElementsByTagName('body')[0].clientHeight;
});

EM_JS(void, WebInstallResizeHandlerImpl, (), {
    if (window.__foResizeHandlerInstalled) {
        return;
    }

    window.__foResizeHandlerInstalled = true;
    const notifyResize = () => {
        _Emscripten_OnWindowResized();
    };

    window.addEventListener('resize', notifyResize);

    if (window.visualViewport) {
        window.visualViewport.addEventListener('resize', notifyResize);
    }

    document.addEventListener('fullscreenchange', notifyResize);
});

EM_JS(void, WebApplyCanvasLayoutImpl, (int screen_width, int screen_height, double horizontal_pos_factor, double vertical_pos_factor), {
    const canvas = document.querySelector('#canvas');
    if (!canvas) {
        return;
    }

    const shell = canvas.parentElement;
    const pageWidth = window.innerWidth || document.documentElement.clientWidth || document.getElementsByTagName('body')[0].clientWidth || screen_width;
    const pageHeight = window.innerHeight || document.documentElement.clientHeight || document.getElementsByTagName('body')[0].clientHeight || screen_height;
    let displayWidth = Math.max(screen_width, 1);
    let displayHeight = Math.max(screen_height, 1);

    if (displayWidth > pageWidth) {
        const scale = pageWidth / displayWidth;
        displayWidth = Math.max(Math.floor(displayWidth * scale), 1);
        displayHeight = Math.max(Math.floor(displayHeight * scale), 1);
    }

    if (displayHeight > pageHeight) {
        const scale = pageHeight / displayHeight;
        displayWidth = Math.max(Math.floor(displayWidth * scale), 1);
        displayHeight = Math.max(Math.floor(displayHeight * scale), 1);
    }

    const freeWidth = Math.max(pageWidth - displayWidth, 0);
    const freeHeight = Math.max(pageHeight - displayHeight, 0);
    const leftMargin = Math.floor(freeWidth * Math.min(Math.max(horizontal_pos_factor, 0.0), 1.0));
    const topMargin = Math.floor(freeHeight * Math.min(Math.max(vertical_pos_factor, 0.0), 1.0));

    document.documentElement.style.margin = '0';
    document.documentElement.style.width = '100%';
    document.documentElement.style.minHeight = '100%';
    document.body.style.margin = '0';
    document.body.style.width = '100%';
    document.body.style.minHeight = '100vh';
    document.body.style.overflow = 'hidden';

    if (shell) {
        shell.style.position = 'fixed';
        shell.style.left = `${leftMargin}px`;
        shell.style.top = `${topMargin}px`;
        shell.style.width = `${displayWidth}px`;
        shell.style.height = `${displayHeight}px`;
        shell.style.margin = '0';
        shell.style.overflow = 'hidden';
    }

    canvas.style.display = 'block';
    canvas.style.width = '100%';
    canvas.style.height = '100%';
    canvas.style.maxWidth = '100%';
    canvas.style.maxHeight = '100%';
});

EM_JS(int, WebGetFixedWidth, (), {
    return 'foScreenWidth' in Module ? Module.foScreenWidth : 0;
});

EM_JS(int, WebGetFixedHeight, (), {
    return 'foScreenHeight' in Module ? Module.foScreenHeight : 0;
});

EM_JS(int, WebIsFullscreenImpl, (), {
    return document.fullscreenElement ? 1 : 0;
});

EM_JS(void, WebSetupClipboardImpl, (const char* canvas_selector), {
    var canvas = document.querySelector(UTF8ToString(Number(canvas_selector)));
    if (canvas) {
        canvas.addEventListener('copy', (event) => {
            const text = _Emscripten_ClipboardGet();
            event.clipboardData.setData('text/plain', UTF8ToString(Number(text)));
            event.preventDefault();
        });
        canvas.addEventListener('paste', (event) => {
            const text = event.clipboardData.getData('text/plain');
            const text_ptr = stringToNewUTF8(text);
            _Emscripten_ClipboardSet(text_ptr);
            _free(text_ptr);
            event.preventDefault();
        });
    }
});

EM_JS(void, WebInitializePersistentDataImpl, (), {
    FS.mkdir('/PersistentData');
    FS.mount(IDBFS, {}, '/PersistentData');
    Module.syncfsDone = 0;
    FS.syncfs(true, function(err) { Module.syncfsDone = 1; });
});

EM_JS(int, WebIsPersistentDataReadyImpl, (), {
    return Module.syncfsDone == 1 ? 1 : 0;
});

EM_JS(void, WebSetWebSocketSchemeImpl, (int secure), {
    Module['websocket']['url'] = secure != 0 ? 'wss://' : 'ws://';
});

EM_JS(void, WebShowErrorImpl, (const char* title_ptr, const char* text_ptr), {
    const title = UTF8ToString(Number(title_ptr));
    const text = UTF8ToString(Number(text_ptr));

    if (typeof window.foShowError === 'function') {
        window.foShowError(title, text, false);
        return;
    }

    let panel = document.getElementById('fo-error-panel');
    let textarea = document.getElementById('fo-error-text');

    if (!panel) {
        panel = document.createElement('div');
        panel.id = 'fo-error-panel';
        panel.style.position = 'fixed';
        panel.style.left = '16px';
        panel.style.right = '16px';
        panel.style.top = '16px';
        panel.style.bottom = '16px';
        panel.style.zIndex = '100000';
        panel.style.background = 'rgba(18, 20, 24, 0.96)';
        panel.style.border = '1px solid rgba(255, 255, 255, 0.2)';
        panel.style.borderRadius = '8px';
        panel.style.padding = '16px';
        panel.style.display = 'flex';
        panel.style.flexDirection = 'column';
        panel.style.gap = '12px';

        const heading = document.createElement('div');
        heading.id = 'fo-error-title';
        heading.style.color = '#ffb4b4';
        heading.style.font = '600 18px Segoe UI, sans-serif';
        panel.appendChild(heading);

        textarea = document.createElement('textarea');
        textarea.id = 'fo-error-text';
        textarea.readOnly = true;
        textarea.style.flex = '1';
        textarea.style.width = '100%';
        textarea.style.boxSizing = 'border-box';
        textarea.style.resize = 'none';
        textarea.style.border = '1px solid rgba(255, 255, 255, 0.18)';
        textarea.style.borderRadius = '6px';
        textarea.style.background = '#111318';
        textarea.style.color = '#f3f4f6';
        textarea.style.padding = '12px';
        textarea.style.font = '13px Consolas, monospace';
        panel.appendChild(textarea);

        const buttons = document.createElement('div');
        buttons.style.display = 'flex';
        buttons.style.gap = '8px';

        const copyButton = document.createElement('button');
        copyButton.textContent = 'Copy';
        copyButton.onclick = async() => {
            try {
                await navigator.clipboard.writeText(textarea.value);
            }
            catch {
                textarea.focus();
                textarea.select();
                document.execCommand('copy');
            }
        };
        buttons.appendChild(copyButton);

        const closeButton = document.createElement('button');
        closeButton.textContent = 'Close';
        closeButton.onclick = () => {
            panel.style.display = 'none';
        };
        buttons.appendChild(closeButton);

        panel.appendChild(buttons);
        document.body.appendChild(panel);
    }

    document.getElementById('fo-error-title').textContent = title;
    textarea.value = text;
    panel.style.display = 'flex';
});
// clang-format on

extern "C"
{
    EMSCRIPTEN_KEEPALIVE const char* Emscripten_ClipboardGet()
    {
        return FO_NAMESPACE App->Input.GetClipboardText().c_str();
    }

    EMSCRIPTEN_KEEPALIVE void Emscripten_ClipboardSet(const char* text)
    {
        FO_NAMESPACE App->Input.SetClipboardText(text);
    }

    EMSCRIPTEN_KEEPALIVE void Emscripten_OnWindowResized()
    {
        if (FO_NAMESPACE App == nullptr) {
            return;
        }

        auto& settings = FO_NAMESPACE App->Settings;

        if (settings.AutoResize) {
            FO_NAMESPACE WebRelated::ApplyWindowSettings(settings);

            const auto screen_width = settings.ScreenWidth;
            const auto screen_height = settings.ScreenHeight;
            const auto screen_size = FO_NAMESPACE App->MainWindow.GetScreenSize();

            if (screen_size.width != screen_width || screen_size.height != screen_height) {
                FO_NAMESPACE App->MainWindow.SetScreenSize({screen_width, screen_height});
            }
            else {
                FO_NAMESPACE WebRelated::ApplyCanvasLayout(settings);
            }
        }
        else {
            FO_NAMESPACE WebRelated::ApplyCanvasLayout(settings);
        }
    }
}
#endif

FO_BEGIN_NAMESPACE

namespace WebRelated
{
#if FO_WEB
    static auto CalcAdaptiveScreenSize(int32 page_width, int32 page_height, bool fullscreen, WebSettings& settings) noexcept -> isize32
    {
        const auto safe_page_width = std::max(page_width, 1);
        const auto safe_page_height = std::max(page_height, 1);

        const auto min_width = std::max(settings.MinWidth, 1);
        const auto min_height = std::max(settings.MinHeight, 1);
        const auto max_width = std::max(settings.MaxWidth, min_width);
        const auto max_height = std::max(settings.MaxHeight, min_height);

        const auto height_percent = fullscreen ? 100 : std::clamp(settings.ScreenHeightPercent, 1, 100);
        auto screen_height = numeric_cast<int32>(std::clamp((numeric_cast<int64>(safe_page_height) * height_percent + 50) / 100, numeric_cast<int64>(min_height), numeric_cast<int64>(max_height)));
        screen_height = std::min(screen_height, safe_page_height);

        const auto aspect_factor = std::max(settings.AspectFactor, 0.001f);
        auto screen_width = iround<int32>(std::clamp(numeric_cast<float32>(screen_height) / aspect_factor, numeric_cast<float32>(min_width), numeric_cast<float32>(max_width)));

        if (screen_width > safe_page_width) {
            screen_width = safe_page_width;
        }

        return {std::max(screen_width, 1), std::max(screen_height, 1)};
    }
#endif

    void ApplyApplicationHints() noexcept
    {
#if FO_WEB
        SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
        SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, CanvasSelector.c_str());
#endif
    }

    void ApplyWindowSettings(WebSettings& settings)
    {
#if FO_WEB
        WebInstallResizeHandlerImpl();

        const auto window_w = WebGetWindowWidth();
        const auto window_h = WebGetWindowHeight();
        const auto fullscreen = WebIsFullscreenImpl() != 0;
        const auto adaptive_size = CalcAdaptiveScreenSize(window_w, window_h, fullscreen, settings);
        settings.ScreenWidth = adaptive_size.width;
        settings.ScreenHeight = adaptive_size.height;
        settings.Fullscreen = fullscreen;

        const auto fixed_w = WebGetFixedWidth();
        const auto fixed_h = WebGetFixedHeight();

        if (fixed_w != 0) {
            settings.ScreenWidth = fixed_w;
        }
        if (fixed_h != 0) {
            settings.ScreenHeight = fixed_h;
        }

        ApplyCanvasLayout(settings);
#else
        ignore_unused(settings);
#endif
    }

    void ApplyCanvasLayout(WebSettings& settings) noexcept
    {
#if FO_WEB
        const auto horizontal_pos_factor = settings.Fullscreen ? 0.5f : settings.HorizontalPosFactor;
        const auto vertical_pos_factor = settings.Fullscreen ? 0.5f : settings.VerticalPosFactor;
        WebApplyCanvasLayoutImpl(settings.ScreenWidth, settings.ScreenHeight, horizontal_pos_factor, vertical_pos_factor);
#else
        ignore_unused(settings);
#endif
    }

    void SetupClipboard()
    {
#if FO_WEB
        WebSetupClipboardImpl(CanvasSelector.c_str());
#endif
    }

    void InitializePersistentData()
    {
#if FO_WEB
        WebInitializePersistentDataImpl();
#endif
    }

    auto IsPersistentDataReady() noexcept -> bool
    {
#if FO_WEB
        return WebIsPersistentDataReadyImpl() == 1;
#else
        return true;
#endif
    }

    void StartMainLoop(void (*entry)(void*), void* data) noexcept
    {
#if FO_WEB
        emscripten_set_main_loop_arg(entry, data, 0, 1);
#else
        ignore_unused(entry);
        ignore_unused(data);
#endif
    }

    void SetWebSocketScheme(bool secure) noexcept
    {
#if FO_WEB
        WebSetWebSocketSchemeImpl(secure ? 1 : 0);
#else
        ignore_unused(secure);
#endif
    }

    void ShowError(string_view title, string_view text)
    {
#if FO_WEB
        const auto title_str = string(title);
        const auto text_str = string(text);
        WebShowErrorImpl(title_str.c_str(), text_str.c_str());
#else
        ignore_unused(title);
        ignore_unused(text);
#endif
    }
}

FO_END_NAMESPACE
