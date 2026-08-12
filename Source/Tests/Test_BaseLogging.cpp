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

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

template<typename T>
concept CanLogToFile = requires(T&& path) { LogToFile(std::forward<T>(path)); };

template<typename T>
concept CanWriteBaseLog = requires(T&& message) { WriteBaseLog(std::forward<T>(message)); };

template<typename T>
concept CanWriteBaseLogBytes = requires(T&& message) { WriteBaseLogBytes(std::forward<T>(message)); };

static_assert(CanLogToFile<u8string_view>);
static_assert(CanLogToFile<const u8string&>);
static_assert(CanLogToFile<u8string>);
static_assert(CanLogToFile<string_view>);
static_assert(CanLogToFile<string&>);
static_assert(CanLogToFile<strex>);
static_assert(CanWriteBaseLog<u8string_view>);
static_assert(CanWriteBaseLog<const u8string&>);
static_assert(CanWriteBaseLog<string_view>);
static_assert(CanWriteBaseLog<string&>);
static_assert(CanWriteBaseLog<strex>);
static_assert(!CanWriteBaseLog<const_span<byte>>);
static_assert(CanWriteBaseLogBytes<const_span<byte>>);
static_assert(!CanWriteBaseLogBytes<string_view>);
static_assert(!CanWriteBaseLogBytes<u8string_view>);

static constexpr u8string_view NullLogPath =
#if FO_WINDOWS
    u8"NUL";
#else
    u8"/dev/null";
#endif

static void OpenLogFile(const std::filesystem::path& path, bool append = false)
{
    FO_STACK_TRACE_ENTRY();

    u8string path_utf8 = fs_path_to_u8string(path);
    LogToFile(path_utf8.view(), append);
}

static auto ReadLogFile(const std::filesystem::path& path) -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    u8string path_utf8 = fs_path_to_u8string(path);
    optional<vector<byte>> content = fs_read_file_bytes(path_utf8.view());
    REQUIRE(content.has_value());
    return std::move(*content);
}

static auto Utf8Bytes(u8string_view text) -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    const_span<byte> bytes = utf8_to_byte_span(text);
    return vector<byte> {bytes.begin(), bytes.end()};
}

static auto ContainsBytes(const vector<byte>& content, const_span<byte> needle) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return std::search(content.begin(), content.end(), needle.begin(), needle.end()) != content.end();
}

TEST_CASE("BaseLogging")
{
    SECTION("LogToFileWritesMessages")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}()) / std::filesystem::path {fs_make_path(u8"журнал-🌍")};
        auto log_path = temp_root / "logs" / "base.log";

        std::filesystem::create_directories(log_path.parent_path());

        OpenLogFile(log_path);
        WriteBaseLog(u8"Привет 🌍\n");
        WriteBaseLog(u8"beta");
        LogToFile(NullLogPath);

        vector<byte> content = ReadLogFile(log_path);
        CHECK(content == Utf8Bytes(u8"Привет 🌍\nbeta"));

        auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("LogToFileTruncatesPreviousContent")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "trunc.log";

        std::filesystem::create_directories(log_path.parent_path());

        OpenLogFile(log_path);
        WriteBaseLog(u8"first round content\n");

        // Reopen the same file. Truncation should drop the previous payload.
        OpenLogFile(log_path);
        WriteBaseLog(u8"second\n");

        LogToFile(NullLogPath);

        vector<byte> content = ReadLogFile(log_path);
        CHECK(content == Utf8Bytes(u8"second\n"));

        auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("LogToFileAppendsWhenRequested")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "append.log";

        std::filesystem::create_directories(log_path.parent_path());

        u8string log_path_utf8 = fs_path_to_u8string(log_path);
        REQUIRE(fs_write_file_text(log_path_utf8.view(), u8"existing\n"));

        OpenLogFile(log_path, true);
        WriteBaseLog(u8"engine\n");

        LogToFile(NullLogPath);

        vector<byte> content = ReadLogFile(log_path);
        CHECK(content == Utf8Bytes(u8"existing\nengine\n"));

        auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingDeliversAllMessagesInOrder")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "async.log";

        std::filesystem::create_directories(log_path.parent_path());

        OpenLogFile(log_path);
        SetAsyncLogWriting(true);

        constexpr int32_t message_count = 256;

        for (int32_t i = 0; i < message_count; i++) {
            WriteBaseLog(strex("async-line-{}\n", i));
        }

        // Disabling async joins the worker, draining whatever is still queued.
        SetAsyncLogWriting(false);
        LogToFile(NullLogPath);

        vector<byte> content = ReadLogFile(log_path);
        for (int32_t i = 0; i < message_count; i++) {
            u8string needle = strex("async-line-{}\n", i);
            CHECK(ContainsBytes(content, utf8_to_byte_span(needle.view())));
        }
        CHECK_FALSE(ContainsBytes(content, string_to_byte_span("Dropped")));

        auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingCanBeToggled")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "toggle.log";

        std::filesystem::create_directories(log_path.parent_path());

        OpenLogFile(log_path);

        // Sync path
        WriteBaseLog(u8"sync-before\n");

        SetAsyncLogWriting(true);
        WriteBaseLog(u8"async-payload\n");
        SetAsyncLogWriting(false);

        // Re-enable to make sure the worker can be restarted cleanly.
        SetAsyncLogWriting(true);
        WriteBaseLog(u8"async-second-round\n");
        SetAsyncLogWriting(false);

        // Sync path again
        WriteBaseLog(u8"sync-after\n");

        LogToFile(NullLogPath);

        vector<byte> content = ReadLogFile(log_path);
        CHECK(ContainsBytes(content, string_to_byte_span("sync-before\n")));
        CHECK(ContainsBytes(content, string_to_byte_span("async-payload\n")));
        CHECK(ContainsBytes(content, string_to_byte_span("async-second-round\n")));
        CHECK(ContainsBytes(content, string_to_byte_span("sync-after\n")));

        auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("SuspendAsyncLogWritingFlushesWithoutJoiningWorker")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "suspend.log";

        std::filesystem::create_directories(log_path.parent_path());

        OpenLogFile(log_path);
        SetAsyncLogWriting(true);

        // Simulate the crash path: route to the synchronous writer without stopping/joining the worker.
        SuspendAsyncLogWriting();
        WriteBaseLog(u8"crash-trace-line\n");

        // The line must already be on disk while the async worker is still running (no join happened).
        vector<byte> content = ReadLogFile(log_path);
        CHECK(ContainsBytes(content, string_to_byte_span("crash-trace-line\n")));

        SetAsyncLogWriting(false);
        LogToFile(NullLogPath);

        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("RawDiagnosticBytesRemainExact")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "raw.log";

        std::filesystem::create_directories(log_path.parent_path());

        OpenLogFile(log_path);
        array<byte, 5> payload = {byte {0x41}, byte {0x80}, byte {0xFF}, byte {0x00}, byte {0x0A}};
        WriteBaseLogBytes(payload);
        LogToFile(NullLogPath);

        vector<byte> content = ReadLogFile(log_path);
        CHECK(content == vector<byte> {payload.begin(), payload.end()});

        auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("StaleUtf8ViewIsRejectedAtTheTextBoundary")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "stale.log";

        std::filesystem::create_directories(log_path.parent_path());

        std::u8string storage = u8"valid";
        u8string_view branded = u8string_view::FromChecked(storage);
        storage[0] = char8_t {0xFF};

        OpenLogFile(log_path);
        WriteBaseLog(branded);
        LogToFile(NullLogPath);

        vector<byte> content = ReadLogFile(log_path);
        CHECK(content == Utf8Bytes(u8"Base log message rejected: invalid UTF-8\n"));

        auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }
}

FO_END_NAMESPACE
