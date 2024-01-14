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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Platform.h"
#include "StringUtils.h"
#include "Version-Include.h"

#if FO_WINDOWS
#include "WinApi-Include.h"
#endif
#if FO_ANDROID
#include <android/log.h>
#endif

#if FO_WINDOWS
template<typename T>
static auto WinApi_GetProcAddress(const char* mod, const char* name) -> T
{
    if (auto* hmod = ::GetModuleHandleA(mod); hmod != nullptr) {
        return reinterpret_cast<T>(::GetProcAddress(hmod, name)); // NOLINT(clang-diagnostic-cast-function-type-strict)
    }

    return nullptr;
}
#endif

void Platform::InfoLog(const string& str)
{
#if FO_WINDOWS
    ::OutputDebugStringW(_str(str).toWideChar().c_str());
#elif FO_ANDROID
    __android_log_write(ANDROID_LOG_INFO, FO_DEV_NAME, str.c_str());
#endif
}

void Platform::SetThreadName(const string& str)
{
#if FO_WINDOWS
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const static auto set_thread_description = WinApi_GetProcAddress<SetThreadDescriptionFn>("kernel32.dll", "SetThreadDescription");

    if (set_thread_description != nullptr) {
        set_thread_description(::GetCurrentThread(), _str(str).toWideChar().c_str());
    }
#endif
}
