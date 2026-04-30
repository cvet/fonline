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
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "base.log";

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

        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("LogToFileTruncatesPreviousContent")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "trunc.log";

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
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingDeliversAllMessagesInOrder")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "async.log";

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
            const auto needle = strex("async-line-{}\n", i);
            CHECK(content.find(string_view {needle}) != std::string::npos);
        }
        CHECK(content.find("Dropped") == std::string::npos);

        input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingCanBeToggled")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "toggle.log";

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
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingDropsMessagesAndReportsCount")
    {
        // The queue cap is hard-coded to 100000 entries inside BaseLogging.cpp.
        // We outpace the worker by enqueueing far more than the cap from
        // multiple producer threads while the worker is still flushing the
        // first batches to disk. The exact number of drops is timing-dependent,
        // but the worker is required to emit at least one notice once any
        // overflow is observed.
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "drop.log";

        std::filesystem::create_directories(log_path.parent_path());

        LogToFile(string(log_path.string()));
        SetAsyncLogWriting(true);

        constexpr int32_t producer_count = 4;
        constexpr int32_t messages_per_producer = 200000;

        vector<std::thread> producers;
        producers.reserve(producer_count);

        for (int32_t producer = 0; producer < producer_count; producer++) {
            producers.emplace_back([producer] {
                for (int32_t i = 0; i < messages_per_producer; i++) {
                    WriteBaseLog(strex("flood-{}-{}\n", producer, i));
                }
            });
        }

        for (auto& thread : producers) {
            thread.join();
        }

        SetAsyncLogWriting(false);
        LogToFile(NullLogPath);

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto notice_pos = content.find("Dropped");
        REQUIRE(notice_pos != std::string::npos);

        const auto notice_end = content.find('\n', notice_pos);
        REQUIRE(notice_end != std::string::npos);

        const auto notice = content.substr(notice_pos, notice_end - notice_pos);
        CHECK(notice.find("log messages due to high volume") != std::string::npos);
        CHECK(notice.find("queue limit 100000") != std::string::npos);

        input.close();
        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }
}

FO_END_NAMESPACE
