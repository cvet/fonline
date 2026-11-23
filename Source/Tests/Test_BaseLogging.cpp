// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Application.h"
#include "catch_amalgamated.hpp"

FO_BEGIN_NAMESPACE

static constexpr string_view NullLogPath {
#if FO_WINDOWS
    "NUL"
#else
    "/dev/null"
#endif
};

TEST_CASE("BaseLogging")
{
    SECTION("logging::to_fileWritesMessages")
    {
        auto temp_root {std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}())};
        auto log_path {temp_root / "logs" / "base.log"};

        std::filesystem::create_directories(log_path.parent_path());

        logging::to_file(string(log_path.string()));
        logging::write_base("alpha\n");
        logging::write_base("beta");

        std::ifstream input {log_path, std::ios::binary};
        REQUIRE(input);

        std::string content {std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>())};
        CHECK(content == "alpha\nbeta");

        input.close();
        logging::to_file(NullLogPath);

        uintmax_t removed {std::filesystem::remove_all(temp_root)};
        CHECK(removed > 0);
    }

    SECTION("logging::to_fileAcceptsANonAsciiPath")
    {
        // A Cyrillic profile name: a narrow path is read in the ANSI code page on Windows, which cannot carry it
        auto temp_root {std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}())};
        auto log_path {temp_root / std::filesystem::path {std::u8string {u8"Тест"}} / "base.log"};

        std::filesystem::create_directories(log_path.parent_path());

        std::u8string utf8_log_path {log_path.u8string()};
        logging::to_file(string(utf8_log_path.begin(), utf8_log_path.end()));
        logging::write_base("gamma");
        logging::to_file(NullLogPath);

        std::ifstream input {log_path, std::ios::binary};
        REQUIRE(input);

        std::string content {std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>())};
        CHECK(content == "gamma");

        input.close();
        uintmax_t removed {std::filesystem::remove_all(temp_root)};
        CHECK(removed > 0);
    }

    SECTION("logging::to_fileTruncatesPreviousContent")
    {
        auto temp_root {std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}())};
        auto log_path {temp_root / "logs" / "trunc.log"};

        std::filesystem::create_directories(log_path.parent_path());

        logging::to_file(string(log_path.string()));
        logging::write_base("first round content\n");

        // Reopen the same file. Truncation should drop the previous payload
        logging::to_file(string(log_path.string()));
        logging::write_base("second\n");

        logging::to_file(NullLogPath);

        std::ifstream input {log_path, std::ios::binary};
        REQUIRE(input);

        std::string content {std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>())};
        CHECK(content == "second\n");

        input.close();
        uintmax_t removed {std::filesystem::remove_all(temp_root)};
        CHECK(removed > 0);
    }

    SECTION("logging::to_fileAppendsWhenRequested")
    {
        auto temp_root {std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}())};
        auto log_path {temp_root / "logs" / "append.log"};

        std::filesystem::create_directories(log_path.parent_path());

        {
            std::ofstream existing_log {log_path, std::ios::binary | std::ios::trunc};
            REQUIRE(existing_log);
            existing_log << "existing\n";
        }

        logging::to_file(string(log_path.string()), true);
        logging::write_base("engine\n");

        logging::to_file(NullLogPath);

        std::ifstream input {log_path, std::ios::binary};
        REQUIRE(input);

        std::string content {std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>())};
        CHECK(content == "existing\nengine\n");

        input.close();
        uintmax_t removed {std::filesystem::remove_all(temp_root)};
        CHECK(removed > 0);
    }

    SECTION("OversizedLogRotatesToNumberedParts")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}());
        const auto log_path = temp_root / "logs" / "rotate.log";

        std::filesystem::create_directories(log_path.parent_path());

        const string big_payload = string(600, 'a') + "\n";

        logging::to_file(string(log_path.string()));
        logging::set_max_file_size(512);

        logging::write_base(big_payload); // Exceeds the limit, rotated into part 1
        logging::write_base("tail-line\n");
        logging::write_base(big_payload); // Exceeds the limit again, rotated into part 2
        logging::write_base("final-line\n");

        logging::set_max_file_size(numeric_cast<size_t>(GetApp()->Settings.Common.MaxLogFileSize));
        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()));
        logging::set_max_file_size(0);
        logging::write_base(big_payload);

        logging::set_max_file_size(numeric_cast<size_t>(GetApp()->Settings.Common.MaxLogFileSize));
        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()));
        logging::set_max_file_size(exact_payload.size());
        logging::write_base(exact_payload);

        logging::set_max_file_size(numeric_cast<size_t>(GetApp()->Settings.Common.MaxLogFileSize));
        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()), true);

        std::ofstream foreign_handle(log_path, std::ios::out | std::ios::binary | std::ios::app);
        REQUIRE(foreign_handle);

        const string big_payload = string(600, 's') + "\n";
        logging::set_max_file_size(512);
        logging::write_base(big_payload);

        foreign_handle << "foreign-after-rotation\n";
        foreign_handle.flush();
        logging::write_base("owner-after-rotation\n");

        logging::set_max_file_size(numeric_cast<size_t>(GetApp()->Settings.Common.MaxLogFileSize));
        foreign_handle.close();
        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()));
        logging::set_max_file_size(512);
        logging::set_async_writing(true);
        logging::write_base(big_payload);
        logging::set_async_writing(false);

        logging::set_max_file_size(numeric_cast<size_t>(GetApp()->Settings.Common.MaxLogFileSize));
        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()));
        logging::write_base("fresh\n");
        logging::to_file(NullLogPath);

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

        logging::to_file(string(log_path.string()), true);
        logging::write_base("engine\n");
        logging::to_file(NullLogPath);

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

        logging::to_file(string(fallback_path.string()));
        logging::write_base("before failed switch\n");
        logging::to_file(string(log_path.string()));
        logging::write_base("after failed switch\n");
        logging::to_file(NullLogPath);

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
        auto temp_root {std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}())};
        auto log_path {temp_root / "logs" / "async.log"};

        std::filesystem::create_directories(log_path.parent_path());

        logging::to_file(string(log_path.string()));
        logging::set_async_writing(true);

        constexpr int32_t message_count {256};

        for (int32_t i {0}; i < message_count; i++) {
            logging::write_base(strex("async-line-{}\n", i));
        }

        // Disabling async joins the worker, draining whatever is still queued
        logging::set_async_writing(false);
        logging::to_file(NullLogPath);

        std::ifstream input {log_path, std::ios::binary};
        REQUIRE(input);

        std::string content {std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>())};

        for (int32_t i {0}; i < message_count; i++) {
            strex needle {strex("async-line-{}\n", i)};
            CHECK(content.find(string_view {needle}) != std::string::npos);
        }

        CHECK(content.find("Dropped") == std::string::npos);

        input.close();
        uintmax_t removed {std::filesystem::remove_all(temp_root)};
        CHECK(removed > 0);
    }

    SECTION("AsyncLoggingCanBeToggled")
    {
        auto temp_root {std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}())};
        auto log_path {temp_root / "logs" / "toggle.log"};

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

        std::ifstream input {log_path, std::ios::binary};
        REQUIRE(input);

        std::string content {std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>())};
        CHECK(content.find("sync-before\n") != std::string::npos);
        CHECK(content.find("async-payload\n") != std::string::npos);
        CHECK(content.find("async-second-round\n") != std::string::npos);
        CHECK(content.find("sync-after\n") != std::string::npos);

        input.close();
        uintmax_t removed {std::filesystem::remove_all(temp_root)};
        CHECK(removed > 0);
    }

    SECTION("SuspendAsyncLogWritingFlushesWithoutJoiningWorker")
    {
        auto temp_root {std::filesystem::temp_directory_path() / "lf_base_logging_tests" / std::to_string(std::random_device {}())};
        auto log_path {temp_root / "logs" / "suspend.log"};

        std::filesystem::create_directories(log_path.parent_path());

        logging::to_file(string(log_path.string()));
        logging::set_async_writing(true);

        // Simulate the crash path: route to the synchronous writer without stopping/joining the worker
        logging::suspend_async_writing();
        logging::write_base("crash-trace-line\n");

        // The line must already be on disk while the async worker is still running (no join happened)
        {
            std::ifstream input {log_path, std::ios::binary};
            REQUIRE(input);

            std::string content {std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>())};
            CHECK(content.find("crash-trace-line\n") != std::string::npos);
        }

        logging::set_async_writing(false);
        logging::to_file(NullLogPath);

        uintmax_t removed {std::filesystem::remove_all(temp_root)};
        CHECK(removed > 0);
    }
}

FO_END_NAMESPACE
