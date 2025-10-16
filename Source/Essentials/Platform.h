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

#include "BasicCore.h"
#include "Containers.h"

FO_BEGIN_NAMESPACE();

struct Platform
{
    Platform() = delete;

    // Windows: OutputDebugStringW
    // Android: __android_log_write ANDROID_LOG_INFO
    // Other: none
    static void InfoLog(const string& str) noexcept;

    // Windows (>= 10): SetThreadDescription
    // Other: none
    static void SetThreadName(const string& str) noexcept;

    // Windows: GetModuleFileNameW
    // Linux: readlink /proc/self/exe
    // Mac: proc_pidpath
    // Other: nullopt
    static auto GetExePath() noexcept -> optional<string>;

    // Linux & Mac: fork
    // Other: warning log message
    static auto ForkProcess() noexcept -> bool;

    // Windows: LoadLibraryW/FreeLibrary/GetProcAddress
    // Linux & Mac: dlopen/dlclose/dlsym
    // Other: nullptr
    static auto LoadModule(const string& module_name) noexcept -> void*;
    static void UnloadModule(void* module_handle) noexcept;
    static auto GetFuncAddr(void* module_handle, const string& func_name) noexcept -> void*;
    template<typename T>
    static auto GetFuncAddr(void* module_handle, const string& func_name) noexcept -> T
    {
        return reinterpret_cast<T>(GetFuncAddr(module_handle, func_name));
    }

    // Windows: GetAsyncKeyState
    // Other: false
    static auto IsShiftDown() noexcept -> bool;
};

FO_END_NAMESPACE();
