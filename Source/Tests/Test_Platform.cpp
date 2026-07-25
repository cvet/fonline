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

template<typename T>
concept CanGetFuncAddrWith = requires(T&& name) { Platform::GetFuncAddr(nullptr, std::forward<T>(name)); };

template<typename T>
concept CanLoadModuleWith = requires(T&& name) { Platform::LoadModule(std::forward<T>(name)); };

template<typename T>
concept CanInfoLogWith = requires(T&& message) { Platform::InfoLog(std::forward<T>(message)); };

template<typename T>
concept CanSetThreadNameWith = requires(T&& name) { Platform::SetThreadName(std::forward<T>(name)); };

static_assert(CanGetFuncAddrWith<string_view_nt>);
static_assert(!CanGetFuncAddrWith<string_view>);
static_assert(!CanGetFuncAddrWith<const char*>);

static_assert(CanLoadModuleWith<u8string_view_nt>);
static_assert(!CanLoadModuleWith<u8string_view>);
static_assert(!CanLoadModuleWith<string_view_nt>);
static_assert(!CanLoadModuleWith<string_view>);
static_assert(!CanLoadModuleWith<const char8_t*>);
static_assert(!CanLoadModuleWith<const char*>);

static_assert(CanInfoLogWith<u8string_view_nt>);
static_assert(!CanInfoLogWith<u8string_view>);
static_assert(!CanInfoLogWith<string_view_nt>);
static_assert(!CanInfoLogWith<string_view>);
static_assert(!CanInfoLogWith<const char8_t*>);
static_assert(!CanInfoLogWith<const char*>);

static_assert(CanSetThreadNameWith<u8string_view_nt>);
static_assert(!CanSetThreadNameWith<u8string_view>);
static_assert(!CanSetThreadNameWith<string_view_nt>);
static_assert(!CanSetThreadNameWith<string_view>);
static_assert(!CanSetThreadNameWith<const char8_t*>);
static_assert(!CanSetThreadNameWith<const char*>);

TEST_CASE("Platform")
{
    SECTION("GetExePathReturnsExistingPath")
    {
        const optional<u8string> exe_path = Platform::GetExePath();

        REQUIRE(exe_path.has_value());
        CHECK_FALSE(exe_path->empty());
        const std::filesystem::path native_path {exe_path->view().native_view()};
        CHECK(std::filesystem::exists(native_path));
        CHECK(std::filesystem::is_regular_file(native_path));
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
        const FuncPtr func = Platform::GetFuncAddr<FuncPtr>(nullptr, string_view_nt {"getpid"});

#if FO_LINUX || FO_MAC
        CHECK(func != nullptr);
#else
        CHECK(func == nullptr);
#endif
    }

    SECTION("GetFuncAddrReturnsNullForMissingSymbol")
    {
        const void* func = Platform::GetFuncAddr(nullptr, string_view_nt {"lf_missing_platform_symbol_for_tests"});
        CHECK(func == nullptr);
    }

    SECTION("RuntimeSymbolStorageUsesTheSharedTerminatedViewHelper")
    {
        array<char, 7> symbol_storage = {'g', 'e', 't', 'p', 'i', 'd', char {}};
        const const_span<char> bounded_symbol {symbol_storage.data(), symbol_storage.size()};
        const optional<string_view_nt> checked_symbol = try_string_view_nt_from_span(bounded_symbol);
        REQUIRE(checked_symbol.has_value());

        symbol_storage[2] = char {};
        CHECK_FALSE(try_string_view_nt_from_span(bounded_symbol));

        symbol_storage[2] = 't';
        symbol_storage.back() = 'x';
        CHECK_FALSE(try_string_view_nt_from_span(bounded_symbol));
    }

    SECTION("LoadModuleReturnsNullForMissingLibrary")
    {
        nptr<void> module = Platform::LoadModule(u8string_view_nt {u8"lf_missing_platform_module_for_tests"});
        CHECK_FALSE(static_cast<bool>(module));
        Platform::UnloadModule(module);
    }

    SECTION("LoadModuleAcceptsUnicodePaths")
    {
        nptr<void> module = Platform::LoadModule(u8string_view_nt {u8"несуществующая_библиотека_𐍈"});
        CHECK_FALSE(static_cast<bool>(module));
        Platform::UnloadModule(module);
    }

    SECTION("LoadModuleRevalidatesExternalTerminatedViews")
    {
        array<char8_t, 7> module_storage = {u8'm', u8'o', u8'd', u8'u', u8'l', u8'e', char8_t {}};
        const const_span<char8_t> bounded_module {module_storage.data(), module_storage.size()};
        const optional<u8string_view_nt> checked_module = u8string_view_nt::TryFrom(bounded_module);
        REQUIRE(checked_module.has_value());
        const u8string_view_nt stale_module = *checked_module;
        module_storage[2] = char8_t {0xFF};

        try {
            (void)Platform::LoadModule(stale_module);
            FAIL("A stale malformed UTF-8 module path was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::ScalarOutOfRange);
            CHECK(ex.offset() == 2);
        }
    }

    SECTION("InfoHelpersAreSafeToCall")
    {
        Platform::InfoLog(u8string_view_nt {u8"platform test log — журнал"});
        Platform::SetThreadName(u8string_view_nt {u8"platform-test-thread-𐍈"});
        SUCCEED();
    }

    SECTION("InfoHelpersRevalidateExternalTerminatedViews")
    {
        array<char8_t, 7> text_storage = {u8't', u8'e', u8'x', u8't', u8'-', u8'x', char8_t {}};
        const const_span<char8_t> bounded_text {text_storage.data(), text_storage.size()};
        const optional<u8string_view_nt> checked_text = u8string_view_nt::TryFrom(bounded_text);
        REQUIRE(checked_text.has_value());
        const u8string_view_nt stale_text = *checked_text;
        text_storage[2] = char8_t {0xFF};

        const auto check_rejected = [&](const auto& invoke) {
            try {
                invoke(stale_text);
                FAIL("A stale malformed UTF-8 diagnostic string was accepted");
            }
            catch (const TextValidationException& ex) {
                CHECK(ex.encoding() == TextEncoding::Utf8);
                CHECK(ex.error() == TextValidationError::ScalarOutOfRange);
                CHECK(ex.offset() == 2);
            }
        };

        check_rejected([](u8string_view_nt value) { Platform::InfoLog(value); });
        check_rejected([](u8string_view_nt value) { Platform::SetThreadName(value); });
    }

    SECTION("CpuUsageSnapshotIsWellFormed")
    {
        const Platform::CpuUsageSnapshot snapshot = Platform::GetCpuUsageSnapshot();

#if FO_WINDOWS || FO_LINUX || FO_MAC || FO_ANDROID
        REQUIRE_FALSE(snapshot.Cores.empty());
        CHECK(snapshot.LogicalCoreCount > 0);

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

        const optional<string> saved_generic = save_env("FO_PLATFORM_UTF8_ENV_TEST");
        set_env("FO_PLATFORM_UTF8_ENV_TEST", "strict-value");
        const optional<u8string> generic_value = Platform::GetEnvironmentUtf8(string_view_nt {"FO_PLATFORM_UTF8_ENV_TEST"});
        set_env("FO_PLATFORM_UTF8_ENV_TEST", "");
        const optional<u8string> empty_value = Platform::GetEnvironmentUtf8(string_view_nt {"FO_PLATFORM_UTF8_ENV_TEST"});
        restore_env("FO_PLATFORM_UTF8_ENV_TEST", saved_generic);

        REQUIRE(generic_value.has_value());
        CHECK(*generic_value == u8string {u8"strict-value"});
        CHECK_FALSE(empty_value.has_value());

        // The resolver reads the OS user-data env vars directly (no SDL/shell32). Drive each platform's
        // primary var and its documented fallback, capture the results, then restore the real env.
#if FO_WINDOWS
        const auto to_utf8 = [](string_view value) -> u8string { return value; };
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

        CHECK(from_local == to_utf8(local_dir));
        CHECK(from_roaming == to_utf8(roaming_dir));
#elif FO_MAC || FO_IOS
        const auto saved_home = save_env("HOME");
        const u8string home_dir {u8"/Users/тест_𐍈"};
        const ptr<const char> home_dir_cstr = utf8_to_c_str(home_dir.view_nt());

        set_env("HOME", home_dir_cstr.get());
        const auto from_home = Platform::GetUserDataBase();

        restore_env("HOME", saved_home);

        CHECK(from_home == u8string {u8"/Users/тест_𐍈/Library/Application Support"});
#else
        const auto to_utf8 = [](string_view value) -> u8string { return value; };
        const auto saved_xdg = save_env("XDG_DATA_HOME");
        const auto saved_home = save_env("HOME");

        const u8string xdg_dir {u8"/tmp/данные_𐍈"};
        const ptr<const char> xdg_dir_cstr = utf8_to_c_str(xdg_dir.view_nt());
        const auto home_dir = strex("/home").combine_path("test").str();

        set_env("XDG_DATA_HOME", xdg_dir_cstr.get());
        const auto from_xdg = Platform::GetUserDataBase();

        set_env("XDG_DATA_HOME", "");
        set_env("HOME", home_dir.c_str());
        const auto from_home = Platform::GetUserDataBase();

        restore_env("XDG_DATA_HOME", saved_xdg);
        restore_env("HOME", saved_home);

        CHECK(from_xdg == xdg_dir);
        CHECK(from_home == to_utf8(strex(home_dir).combine_path(".local/share").str()));
#endif
    }

#if !FO_WINDOWS
    SECTION("GetUserDataBaseRejectsInvalidUtf8Environment")
    {
#if FO_MAC || FO_IOS
        constexpr const char* variable_name = "HOME";
#else
        constexpr const char* variable_name = "XDG_DATA_HOME";
#endif
        const nptr<const char> original_value {std::getenv(variable_name)};
        const optional<string> saved_value = original_value ? optional<string> {string {original_value.get()}} : optional<string> {};
        const auto restore_env = scope_exit([&]() noexcept {
            if (saved_value) {
                (void)setenv(variable_name, saved_value->c_str(), 1);
            }
            else {
                (void)unsetenv(variable_name);
            }
        });

        array<char, 6> invalid_value = {'/', 't', 'm', 'p', std::bit_cast<char>(uint8_t {0xFF}), char {}};
        REQUIRE(setenv(variable_name, invalid_value.data(), 1) == 0);

        try {
            (void)Platform::GetUserDataBase();
            FAIL("An invalid UTF-8 environment path was accepted");
        }
        catch (const TextValidationException& ex) {
            CHECK(ex.encoding() == TextEncoding::Utf8);
            CHECK(ex.error() == TextValidationError::ScalarOutOfRange);
            CHECK(ex.offset() == 4);
        }
    }
#endif
}

FO_END_NAMESPACE
