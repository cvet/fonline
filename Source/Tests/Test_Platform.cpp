//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include "catch_amalgamated.hpp"

#include <filesystem>

#if FO_LINUX || FO_MAC
#include <unistd.h>
#endif

#include "Platform.h"

FO_BEGIN_NAMESPACE

TEST_CASE("Platform")
{
    SECTION("GetExePathReturnsExistingPath")
    {
        const auto exe_path = Platform::GetExePath();

        REQUIRE(exe_path.has_value());
        CHECK_FALSE(exe_path->empty());
        CHECK(std::filesystem::exists(*exe_path));
        CHECK(std::filesystem::is_regular_file(*exe_path));
    }

    SECTION("CurrentProcessIdStringMatchesRuntime")
    {
        const auto pid_str = Platform::GetCurrentProcessIdStr();

        CHECK_FALSE(pid_str.empty());
        CHECK(pid_str.find_first_not_of("0123456789") == std::string::npos);

#if FO_WINDOWS
        CHECK(pid_str != "0");
#elif FO_LINUX || FO_MAC
        const std::string runtime_pid = std::to_string(::getpid());
        CHECK(pid_str == runtime_pid.c_str());
#else
        CHECK(pid_str == "0");
#endif
    }

    SECTION("GetFuncAddrCanResolveProcessSymbols")
    {
        using FuncPtr = void (*)();
        const FuncPtr func = Platform::GetFuncAddr<FuncPtr>(nullptr, "getpid");

#if FO_LINUX || FO_MAC
        CHECK(func != nullptr);
#else
        CHECK(func == nullptr);
#endif
    }

    SECTION("GetFuncAddrReturnsNullForMissingSymbol")
    {
        const void* func = Platform::GetFuncAddr(nullptr, "lf_missing_platform_symbol_for_tests");
        CHECK(func == nullptr);
    }

    SECTION("LoadModuleReturnsNullForMissingLibrary")
    {
        void* module = Platform::LoadModule("lf_missing_platform_module_for_tests");
        CHECK(module == nullptr);
        Platform::UnloadModule(module);
    }

    SECTION("InfoHelpersAreSafeToCall")
    {
        Platform::InfoLog("platform test log");
        Platform::SetThreadName("platform-test-thread");
        SUCCEED();
    }
}

FO_END_NAMESPACE
