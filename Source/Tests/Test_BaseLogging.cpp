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

#include "Application.h"
#include "Common.h"

FO_BEGIN_NAMESPACE

static const auto NullLogPath =
#if FO_WINDOWS
    string_view {"NUL"};
#else
    string_view {"/dev/null"};
#endif

TEST_CASE("BaseLogging")
{
    SECTION("LogToFileWritesMessages")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "base.log";

        std::filesystem::create_directories(log_path.parent_path());

        LogToFile(string(log_path.string()));
        WriteBaseLog("alpha\n");
        WriteBaseLog("beta");

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "alpha\nbeta");

        input.close();
        LogToFile(NullLogPath);

        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("LogToFileTruncatesPreviousContent")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "trunc.log";

        std::filesystem::create_directories(log_path.parent_path());

        LogToFile(string(log_path.string()));
        WriteBaseLog("first round content\n");

        // Reopen the same file. Truncation should drop the previous payload.
        LogToFile(string(log_path.string()));
        WriteBaseLog("second\n");

        LogToFile(NullLogPath);

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "second\n");

        input.close();
        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("LogToFileAppendsWhenRequested")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "append.log";

        std::filesystem::create_directories(log_path.parent_path());

        {
            std::ofstream existing_log(log_path, std::ios::binary | std::ios::trunc);
            REQUIRE(existing_log);
            existing_log << "existing\n";
        }

        LogToFile(string(log_path.string()), true);
        WriteBaseLog("engine\n");

        LogToFile(NullLogPath);

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "existing\nengine\n");

        input.close();
        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("OversizedLogRotatesToNumberedParts")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "rotate.log";

        std::filesystem::create_directories(log_path.parent_path());

        const string big_payload = string(600, 'a') + "\n";

        LogToFile(string(log_path.string()));
        SetMaxLogFileSize(512);

        WriteBaseLog(big_payload); // Exceeds the limit, rotated into part 1
        WriteBaseLog("tail-line\n");
        WriteBaseLog(big_payload); // Exceeds the limit again, rotated into part 2
        WriteBaseLog("final-line\n");

        SetMaxLogFileSize(numeric_cast<size_t>(GetApp()->Settings.MaxLogFileSize));
        LogToFile(NullLogPath);

        const auto read_file = [](const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary);
            REQUIRE(input);
            return string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        };

        const auto part1_path = log_path.parent_path() / "rotate.log.1";
        const auto part2_path = log_path.parent_path() / "rotate.log.2";
        REQUIRE(std::filesystem::exists(part1_path));
        REQUIRE(std::filesystem::exists(part2_path));

        CHECK(read_file(part1_path) == big_payload);

        const string part2_content = read_file(part2_path);
        CHECK(part2_content.find("tail-line\n") != string::npos);
        CHECK(part2_content.find(big_payload) != string::npos);

        const string current_content = read_file(log_path);
        CHECK(current_content.find("Log rotated, previous part: '") != string::npos);
        CHECK(current_content.find("final-line\n") != string::npos);
        CHECK(current_content.find("tail-line\n") == string::npos);

        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("ZeroSizeLimitDisablesRotation")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "disabled.log";

        std::filesystem::create_directories(log_path.parent_path());

        const string big_payload = string(600, 'z') + "\n";

        LogToFile(string(log_path.string()));
        SetMaxLogFileSize(0);
        WriteBaseLog(big_payload);

        SetMaxLogFileSize(numeric_cast<size_t>(GetApp()->Settings.MaxLogFileSize));
        LogToFile(NullLogPath);

        CHECK(!std::filesystem::exists(log_path.parent_path() / "disabled.log.1"));

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        const string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == big_payload);

        input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("LogAtExactSizeLimitDoesNotRotate")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "exact-limit.log";

        std::filesystem::create_directories(log_path.parent_path());

        const string exact_payload(512, 'x');

        LogToFile(string(log_path.string()));
        SetMaxLogFileSize(exact_payload.size());
        WriteBaseLog(exact_payload);

        SetMaxLogFileSize(numeric_cast<size_t>(GetApp()->Settings.MaxLogFileSize));
        LogToFile(NullLogPath);

        CHECK(!std::filesystem::exists(log_path.parent_path() / "exact-limit.log.1"));

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        const string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == exact_payload);

        input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AppendRotationKeepsForeignHandleOnActiveFile")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "shared.log";

        std::filesystem::create_directories(log_path.parent_path());

        LogToFile(string(log_path.string()), true);

        std::ofstream foreign_handle(log_path, std::ios::out | std::ios::binary | std::ios::app);
        REQUIRE(foreign_handle);

        const string big_payload = string(600, 's') + "\n";
        SetMaxLogFileSize(512);
        WriteBaseLog(big_payload);

        foreign_handle << "foreign-after-rotation\n";
        foreign_handle.flush();
        WriteBaseLog("owner-after-rotation\n");

        SetMaxLogFileSize(numeric_cast<size_t>(GetApp()->Settings.MaxLogFileSize));
        foreign_handle.close();
        LogToFile(NullLogPath);

        const auto read_file = [](const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary);
            REQUIRE(input);
            return string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        };

        const string rotated_content = read_file(log_path.parent_path() / "shared.log.1");
        const string active_content = read_file(log_path);

        CHECK(rotated_content == big_payload);
        CHECK(rotated_content.find("foreign-after-rotation") == string::npos);
        CHECK(active_content.find("foreign-after-rotation\n") != string::npos);
        CHECK(active_content.find("owner-after-rotation\n") != string::npos);

        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncOversizedLogRotatesAfterQueueDrain")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "async-rotate.log";

        std::filesystem::create_directories(log_path.parent_path());

        const string big_payload = string(600, 'q') + "\n";

        LogToFile(string(log_path.string()));
        SetMaxLogFileSize(512);
        SetAsyncLogWriting(true);
        WriteBaseLog(big_payload);
        SetAsyncLogWriting(false);

        SetMaxLogFileSize(numeric_cast<size_t>(GetApp()->Settings.MaxLogFileSize));
        LogToFile(NullLogPath);

        std::ifstream input(log_path.parent_path() / "async-rotate.log.1", std::ios::binary);
        REQUIRE(input);

        const string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == big_payload);

        input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("FreshLogOpenDeletesStaleRotatedParts")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "stale.log";

        std::filesystem::create_directories(log_path.parent_path());

        const auto make_file = [](const std::filesystem::path& path) {
            std::ofstream file(path, std::ios::binary);
            REQUIRE(file);
            file << "stale content\n";
        };

        make_file(log_path);
        make_file(log_path.parent_path() / "stale.log.1");
        make_file(log_path.parent_path() / "stale.log.12");
        make_file(log_path.parent_path() / "stale.log.bak");
        make_file(log_path.parent_path() / "other.log.1");

        LogToFile(string(log_path.string()));
        WriteBaseLog("fresh\n");
        LogToFile(NullLogPath);

        CHECK(!std::filesystem::exists(log_path.parent_path() / "stale.log.1"));
        CHECK(!std::filesystem::exists(log_path.parent_path() / "stale.log.12"));
        CHECK(std::filesystem::exists(log_path.parent_path() / "stale.log.bak"));
        CHECK(std::filesystem::exists(log_path.parent_path() / "other.log.1"));

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "fresh\n");

        input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AppendLogOpenKeepsRotatedParts")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "append.log";

        std::filesystem::create_directories(log_path.parent_path());

        {
            std::ofstream existing_log(log_path, std::ios::binary | std::ios::trunc);
            REQUIRE(existing_log);
            existing_log << "existing\n";

            std::ofstream rotated_part(log_path.parent_path() / "append.log.1", std::ios::binary | std::ios::trunc);
            REQUIRE(rotated_part);
            rotated_part << "rotated part\n";
        }

        LogToFile(string(log_path.string()), true);
        WriteBaseLog("engine\n");
        LogToFile(NullLogPath);

        CHECK(std::filesystem::exists(log_path.parent_path() / "append.log.1"));

        std::ifstream rotated_input(log_path.parent_path() / "append.log.1", std::ios::binary);
        REQUIRE(rotated_input);

        const std::string rotated_content((std::istreambuf_iterator<char>(rotated_input)), std::istreambuf_iterator<char>());
        CHECK(rotated_content == "rotated part\n");
        rotated_input.close();

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "existing\nengine\n");

        input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AppendLogOpenCanDeleteStaleRotatedParts")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "writable.log";

        std::filesystem::create_directories(log_path.parent_path());

        {
            std::ofstream existing_log(log_path, std::ios::binary | std::ios::trunc);
            REQUIRE(existing_log);
            existing_log << "previous main\n";

            std::ofstream rotated_part(log_path.parent_path() / "writable.log.1", std::ios::binary | std::ios::trunc);
            REQUIRE(rotated_part);
            rotated_part << "previous part\n";
        }

        LogToFile(string(log_path.string()), true, true);
        WriteBaseLog("current run\n");
        LogToFile(NullLogPath);

        CHECK(!std::filesystem::exists(log_path.parent_path() / "writable.log.1"));

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "previous main\ncurrent run\n");

        input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("FailedFreshLogOpenKeepsRotatedParts")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto fallback_path = temp_root / "logs" / "fallback.log";
        const auto log_path = temp_root / "logs" / "blocked.log";
        const auto rotated_path = log_path.parent_path() / "blocked.log.1";

        std::filesystem::create_directories(log_path);

        {
            std::ofstream rotated_part(rotated_path, std::ios::binary | std::ios::trunc);
            REQUIRE(rotated_part);
            rotated_part << "diagnostics to keep\n";
        }

        LogToFile(string(fallback_path.string()));
        WriteBaseLog("before failed switch\n");
        LogToFile(string(log_path.string()));
        WriteBaseLog("after failed switch\n");
        LogToFile(NullLogPath);

        REQUIRE(std::filesystem::exists(rotated_path));

        std::ifstream input(rotated_path, std::ios::binary);
        REQUIRE(input);

        const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "diagnostics to keep\n");

        input.close();

        std::ifstream fallback_input(fallback_path, std::ios::binary);
        REQUIRE(fallback_input);

        const std::string fallback_content((std::istreambuf_iterator<char>(fallback_input)), std::istreambuf_iterator<char>());
        CHECK(fallback_content.find("before failed switch\n") != string::npos);
        CHECK(fallback_content.find("after failed switch\n") != string::npos);

        fallback_input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingDeliversAllMessagesInOrder")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "async.log";

        std::filesystem::create_directories(log_path.parent_path());

        LogToFile(string(log_path.string()));
        SetAsyncLogWriting(true);

        constexpr int32_t message_count = 256;

        for (int32_t i = 0; i < message_count; i++) {
            WriteBaseLog(strex("async-line-{}\n", i));
        }

        // Disabling async joins the worker, draining whatever is still queued.
        SetAsyncLogWriting(false);
        LogToFile(NullLogPath);

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        for (int32_t i = 0; i < message_count; i++) {
            strex needle = strex("async-line-{}\n", i);
            CHECK(content.find(string_view {needle}) != std::string::npos);
        }
        CHECK(content.find("Dropped") == std::string::npos);

        input.close();
        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingCanBeToggled")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "toggle.log";

        std::filesystem::create_directories(log_path.parent_path());

        LogToFile(string(log_path.string()));

        // Sync path
        WriteBaseLog("sync-before\n");

        SetAsyncLogWriting(true);
        WriteBaseLog("async-payload\n");
        SetAsyncLogWriting(false);

        // Re-enable to make sure the worker can be restarted cleanly.
        SetAsyncLogWriting(true);
        WriteBaseLog("async-second-round\n");
        SetAsyncLogWriting(false);

        // Sync path again
        WriteBaseLog("sync-after\n");

        LogToFile(NullLogPath);

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content.find("sync-before\n") != std::string::npos);
        CHECK(content.find("async-payload\n") != std::string::npos);
        CHECK(content.find("async-second-round\n") != std::string::npos);
        CHECK(content.find("sync-after\n") != std::string::npos);

        input.close();
        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("SuspendAsyncLogWritingFlushesWithoutJoiningWorker")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "suspend.log";

        std::filesystem::create_directories(log_path.parent_path());

        LogToFile(string(log_path.string()));
        SetAsyncLogWriting(true);

        // Simulate the crash path: route to the synchronous writer without stopping/joining the worker.
        SuspendAsyncLogWriting();
        WriteBaseLog("crash-trace-line\n");

        // The line must already be on disk while the async worker is still running (no join happened).
        {
            std::ifstream input(log_path, std::ios::binary);
            REQUIRE(input);

            std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            CHECK(content.find("crash-trace-line\n") != std::string::npos);
        }

        SetAsyncLogWriting(false);
        LogToFile(NullLogPath);

        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }
}

FO_END_NAMESPACE
