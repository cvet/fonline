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

        logging::to_file(string(log_path.string()));
        logging::write_base("alpha\n");
        logging::write_base("beta");

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "alpha\nbeta");

        input.close();
        logging::to_file(NullLogPath);

        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("LogToFileTruncatesPreviousContent")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "trunc.log";

        std::filesystem::create_directories(log_path.parent_path());

        logging::to_file(string(log_path.string()));
        logging::write_base("first round content\n");

        // Reopen the same file. Truncation should drop the previous payload
        logging::to_file(string(log_path.string()));
        logging::write_base("second\n");

        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()), true);
        logging::write_base("engine\n");

        logging::to_file(NullLogPath);

        std::ifstream input(log_path, std::ios::binary);
        REQUIRE(input);

        std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        CHECK(content == "existing\nengine\n");

        input.close();
        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingDeliversAllMessagesInOrder")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        auto log_path = temp_root / "logs" / "async.log";

        std::filesystem::create_directories(log_path.parent_path());

        logging::to_file(string(log_path.string()));
        logging::set_async_writing(true);

        constexpr int32_t message_count = 256;

        for (int32_t i = 0; i < message_count; i++) {
            logging::write_base(strex("async-line-{}\n", i));
        }

        // Disabling async joins the worker, draining whatever is still queued
        logging::set_async_writing(false);
        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()));

        // Sync path
        logging::write_base("sync-before\n");

        logging::set_async_writing(true);
        logging::write_base("async-payload\n");
        logging::set_async_writing(false);

        // Re-enable to make sure the worker can be restarted cleanly
        logging::set_async_writing(true);
        logging::write_base("async-second-round\n");
        logging::set_async_writing(false);

        // Sync path again
        logging::write_base("sync-after\n");

        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()));
        logging::set_async_writing(true);

        // Simulate the crash path: route to the synchronous writer without stopping/joining the worker
        logging::suspend_async_writing();
        logging::write_base("crash-trace-line\n");

        // The line must already be on disk while the async worker is still running (no join happened)
        {
            std::ifstream input(log_path, std::ios::binary);
            REQUIRE(input);

            std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            CHECK(content.find("crash-trace-line\n") != std::string::npos);
        }

        logging::set_async_writing(false);
        logging::to_file(NullLogPath);

        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }
}

FO_END_NAMESPACE
