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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Application.h"

#include "Version-Include.h"

FO_BEGIN_NAMESPACE();

raw_ptr<Application> App;

// Request quit on interrupt signals
#if FO_LINUX || FO_MAC
static void SignalHandler(int32 sig)
{
    std::signal(sig, SignalHandler);
    App->RequestQuit();
}
#endif

// Web wrappers
#if FO_WEB
FO_END_NAMESPACE();
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
}
FO_BEGIN_NAMESPACE();
#endif

void InitApp(int32 argc, char** argv, AppInitFlags flags)
{
    FO_STACK_TRACE_ENTRY();

    // Ensure that we call init only once
    static std::once_flag once;
    auto first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });
    FO_STRONG_ASSERT(first_call);

    // Fork the process if requested
    if (std::any_of(argv, argv + argc, [](const char* arg) { return std::string_view(arg) == "--fork"; })) {
        Platform::ForkProcess();
    }

    // Create global data as soon as possible
    CreateGlobalData();

    // Write log and show message box on exception
    SetExceptionCallback([flags](string_view message, string_view traceback, bool fatal_error) {
        WriteLog(LogType::Error, "{}\n{}", message, traceback);

        if (fatal_error) {
            WriteLog(LogType::Error, "Shutdown!");
        }

        if (IsEnumSet(flags, AppInitFlags::ShowMessageOnException)) {
            MessageBox::ShowErrorMessage(message, traceback, fatal_error);
        }
    });

#if FO_TRACY
    TracySetProgramName(FO_NICE_NAME);
#endif

    // Write log to file
#if !FO_WEB
    if (const auto exe_path = Platform::GetExePath()) {
        LogToFile(strex("{}.log", strex(exe_path.value()).extractFileName().eraseFileExtension()));
    }
    else {
        LogToFile(strex("{}.log", FO_DEV_NAME));
    }
#endif

    if (IsEnumSet(flags, AppInitFlags::DisableLogTags)) {
        LogDisableTags();
    }

    WriteLog("Starting {}", FO_NICE_NAME);

    // Actual application initialization
    App = SafeAlloc::MakeRaw<Application>(argc, argv, flags);

    // Request quit on bad alloc
    SetBadAllocCallback([] { App->RequestQuit(); });

    // Request quit on interrupt signals
#if FO_LINUX || FO_MAC
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#endif

    // Set up clipboard events for web
#if FO_WEB
    // clang-format off
    MAIN_THREAD_EM_ASM({
        var canvas = document.querySelector(UTF8ToString($0));
        if (canvas) {
            canvas.addEventListener("copy", (event) => {
                const text = _Emscripten_ClipboardGet();
                event.clipboardData.setData("text/plain", UTF8ToString(text));
                event.preventDefault();
            });
            canvas.addEventListener("paste", (event) => {
                const text = event.clipboardData.getData('text/plain');
                _Emscripten_ClipboardSet(text);
                event.preventDefault();
            });
        }
    }, WEB_CANVAS_ID.c_str());
    // clang-format on
#endif
}

FO_END_NAMESPACE();
