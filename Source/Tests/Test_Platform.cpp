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
#include "StringUtils.h"

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
        nptr<void> module = Platform::LoadModule("lf_missing_platform_module_for_tests");
        CHECK_FALSE(static_cast<bool>(module));
        Platform::UnloadModule(module);
    }

    SECTION("InfoHelpersAreSafeToCall")
    {
        Platform::InfoLog("platform test log");
        Platform::SetThreadName("platform-test-thread");
        SUCCEED();
    }

    SECTION("CpuUsageSnapshotIsWellFormed")
    {
        const Platform::CpuUsageSnapshot snapshot = Platform::GetCpuUsageSnapshot();

#if FO_WINDOWS || FO_LINUX || FO_MAC || FO_ANDROID
        REQUIRE_FALSE(snapshot.Cores.empty());

        for (const Platform::CpuUsageCoreSnapshot& core : snapshot.Cores) {
            CHECK(core.TotalTime >= core.IdleTime);
        }
#else
        CHECK(snapshot.Cores.empty());
        CHECK(snapshot.ProcessTimeNs == 0);
#endif
    }

    SECTION("GetUserDataBaseResolvesFromEnvironment")
    {
        const auto save_env = [](const char* name) -> optional<string> {
            const char* value = std::getenv(name);
            return value != nullptr ? optional<string> {string(value)} : optional<string> {};
        };

        const auto set_env = [](const char* name, const char* value) {
#if FO_WINDOWS
            _putenv_s(name, value);
#else
            if (value[0] != '\0') {
                setenv(name, value, 1);
            }
            else {
                unsetenv(name);
            }
#endif
        };

        const auto restore_env = [&set_env](const char* name, const optional<string>& saved) { set_env(name, saved.has_value() ? saved->c_str() : ""); };

        // The resolver reads the OS user-data env vars directly (no SDL/shell32). Drive each platform's
        // primary var and its documented fallback, capture the results, then restore the real env.
#if FO_WINDOWS
        const auto saved_local = save_env("LOCALAPPDATA");
        const auto saved_roaming = save_env("APPDATA");

        const auto local_dir = strex("C:").combine_path("AppData/Local").str();
        const auto roaming_dir = strex("C:").combine_path("AppData/Roaming").str();

        set_env("LOCALAPPDATA", local_dir.c_str());
        const auto from_local = Platform::GetUserDataBase();

        set_env("LOCALAPPDATA", "");
        set_env("APPDATA", roaming_dir.c_str());
        const auto from_roaming = Platform::GetUserDataBase();

        restore_env("LOCALAPPDATA", saved_local);
        restore_env("APPDATA", saved_roaming);

        CHECK(from_local == local_dir);
        CHECK(from_roaming == roaming_dir);
#elif FO_MAC || FO_IOS
        const auto saved_home = save_env("HOME");
        const auto home_dir = strex("/Users").combine_path("test").str();

        set_env("HOME", home_dir.c_str());
        const auto from_home = Platform::GetUserDataBase();

        restore_env("HOME", saved_home);

        CHECK(from_home == strex(home_dir).combine_path("Library/Application Support").str());
#else
        const auto saved_xdg = save_env("XDG_DATA_HOME");
        const auto saved_home = save_env("HOME");

        const auto xdg_dir = strex("/tmp").combine_path("xdg_data").str();
        const auto home_dir = strex("/home").combine_path("test").str();

        set_env("XDG_DATA_HOME", xdg_dir.c_str());
        const auto from_xdg = Platform::GetUserDataBase();

        set_env("XDG_DATA_HOME", "");
        set_env("HOME", home_dir.c_str());
        const auto from_home = Platform::GetUserDataBase();

        restore_env("XDG_DATA_HOME", saved_xdg);
        restore_env("HOME", saved_home);

        CHECK(from_xdg == xdg_dir);
        CHECK(from_home == strex(home_dir).combine_path(".local/share").str());
#endif
    }
}

FO_END_NAMESPACE
