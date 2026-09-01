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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "catch_amalgamated.hpp"

#include <filesystem>

#include "DiskFileSystem.h"
#include "Platform.h"
#include "Posix.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

TEST_CASE("Platform")
{
    SECTION("GetExePathReturnsExistingPath")
    {
        auto exe_path = platform::get_exe_path();

        REQUIRE(exe_path.has_value());
        CHECK_FALSE(exe_path->empty());
        CHECK(std::filesystem::exists(fs_make_path(*exe_path)));
        CHECK(std::filesystem::is_regular_file(fs_make_path(*exe_path)));
    }

    SECTION("CurrentProcessIdStringMatchesRuntime")
    {
        string pid_str = platform::get_current_process_id_str();

        CHECK_FALSE(pid_str.empty());
        CHECK(pid_str.find_first_not_of("0123456789") == std::string::npos);

#if FO_WINDOWS
        CHECK(pid_str != "0");
#elif FO_LINUX || FO_MAC
        CHECK(pid_str == strex("{}", posix::get_current_process_id()).str());
#else
        CHECK(pid_str == "0");
#endif
    }

    SECTION("GetFuncAddrCanResolveProcessSymbols")
    {
        using FuncPtr = void (*)();
        FuncPtr func = platform::get_func_addr<FuncPtr>(nullptr, "getpid");

#if FO_LINUX || FO_MAC
        CHECK(func != nullptr);
#else
        CHECK(func == nullptr);
#endif
    }

    SECTION("GetFuncAddrReturnsNullForMissingSymbol")
    {
        const void* func = platform::get_func_addr(nullptr, "lf_missing_platform_symbol_for_tests");
        CHECK(func == nullptr);
    }

    SECTION("LoadModuleReturnsNullForMissingLibrary")
    {
        nptr<void> module = platform::load_module("lf_missing_platform_module_for_tests");
        CHECK_FALSE(static_cast<bool>(module));
        platform::unload_module(module);
    }

    SECTION("InfoHelpersAreSafeToCall")
    {
        platform::info_log("platform test log");
        platform::set_thread_name("platform-test-thread");
        SUCCEED();
    }

    SECTION("ProcessMemoryUsageIsReported")
    {
        size_t working_set = platform::get_process_memory_usage();
        size_t private_usage = platform::get_process_private_memory_usage();

#if FO_WINDOWS || FO_LINUX || FO_MAC || FO_ANDROID
        // A live process always occupies memory, so both readings must be non-zero on a real platform
        CHECK(working_set > 0);
        CHECK(private_usage > 0);
#else
        CHECK(working_set == 0);
        CHECK(private_usage == 0);
#endif
    }

    SECTION("ModuleLifecycleResolvesAndReleases")
    {
        // The already-loaded process image is the one module every platform can name without a fixture
        nptr<void> self_module = platform::load_module({});

        if (self_module) {
            CHECK(platform::get_func_addr(self_module, "malloc") != nullptr);
            CHECK(platform::get_func_addr(self_module, "no_such_symbol_for_test") == nullptr);
            platform::unload_module(self_module);
        }

        // Unloading nothing is a no-op rather than a failure
        platform::unload_module(nullptr);
    }

    SECTION("CpuUsageSnapshotIsWellFormed")
    {
        platform::cpu_usage_snapshot snapshot = platform::get_cpu_usage_snapshot();

#if FO_WINDOWS || FO_LINUX || FO_MAC || FO_ANDROID
        REQUIRE_FALSE(snapshot.cores.empty());
        CHECK(snapshot.logical_core_count > 0);

        for (const platform::cpu_usage_core_snapshot& core : snapshot.cores) {
            CHECK(core.total_time >= core.idle_time);
        }
#else
        CHECK(snapshot.cores.empty());
        CHECK(snapshot.process_time_ns == 0);
#endif
    }

    SECTION("GetUserDataBaseResolvesFromEnvironment")
    {
        auto save_env = [](const char* name) -> optional<string> {
            const char* value = std::getenv(name);
            return value != nullptr ? optional<string> {string(value)} : optional<string> {};
        };

        auto set_env = [](const char* name, const char* value) {
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

        auto restore_env = [&set_env](const char* name, const optional<string>& saved) { set_env(name, saved.has_value() ? saved->c_str() : ""); };

        // The resolver reads the OS user-data env vars directly (no SDL/shell32). Drive each platform's
        // primary var and its documented fallback, capture the results, then restore the real env
#if FO_WINDOWS
        auto saved_local = save_env("LOCALAPPDATA");
        auto saved_roaming = save_env("APPDATA");

        string local_dir = strex("C:").combine_path("AppData/Local").str();
        string roaming_dir = strex("C:").combine_path("AppData/Roaming").str();

        set_env("LOCALAPPDATA", local_dir.c_str());
        string from_local = platform::get_user_data_base();

        set_env("LOCALAPPDATA", "");
        set_env("APPDATA", roaming_dir.c_str());
        string from_roaming = platform::get_user_data_base();

        restore_env("LOCALAPPDATA", saved_local);
        restore_env("APPDATA", saved_roaming);

        CHECK(from_local == local_dir);
        CHECK(from_roaming == roaming_dir);
#elif FO_MAC || FO_IOS
        const auto saved_home = save_env("HOME");
        const auto home_dir = strex("/Users").combine_path("test").str();

        set_env("HOME", home_dir.c_str());
        const auto from_home = platform::get_user_data_base();

        restore_env("HOME", saved_home);

        CHECK(from_home == strex(home_dir).combine_path("Library/Application Support").str());
#else
        auto saved_xdg = save_env("XDG_DATA_HOME");
        auto saved_home = save_env("HOME");

        string xdg_dir = strex("/tmp").combine_path("xdg_data").str();
        string home_dir = strex("/home").combine_path("test").str();

        set_env("XDG_DATA_HOME", xdg_dir.c_str());
        string from_xdg = platform::get_user_data_base();

        set_env("XDG_DATA_HOME", "");
        set_env("HOME", home_dir.c_str());
        string from_home = platform::get_user_data_base();

        restore_env("XDG_DATA_HOME", saved_xdg);
        restore_env("HOME", saved_home);

        CHECK(from_xdg == xdg_dir);
        CHECK(from_home == strex(home_dir).combine_path(".local/share").str());
#endif
    }
}

FO_END_NAMESPACE
