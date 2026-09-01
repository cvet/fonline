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

#include "Logging.h"

FO_BEGIN_NAMESPACE

TEST_CASE("Logging")
{
    set_log_callback("", {});

    SECTION("CallbackReceivesFormattedMessage")
    {
        vector<string> captured;

        set_log_callback("capture", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured.emplace_back(message); });
        write_log("Hello {}", 42);

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("Hello 42") != string::npos);
        CHECK(captured.front().ends_with('\n'));

        set_log_callback("capture", {});
    }

    SECTION("CallbackReplacementUsesLastRegisteredHandler")
    {
        int32_t first_count = 0;
        int32_t second_count = 0;

        set_log_callback("replace", [&](log_type, string_view, nptr<const catched_stack_trace_data>) { first_count++; });
        set_log_callback("replace", [&](log_type, string_view, nptr<const catched_stack_trace_data>) { second_count++; });
        write_log(log_type::warning, "Replacement {}", 1);

        CHECK(first_count == 0);
        CHECK(second_count == 1);

        set_log_callback("replace", {});
    }

    SECTION("CallbackCanBeClearedByEmptyKey")
    {
        int32_t callback_count = 0;

        set_log_callback("clear-me", [&](log_type, string_view, nptr<const catched_stack_trace_data>) { callback_count++; });
        set_log_callback("", {});
        write_log_message(log_type::error, "should not hit callback");

        CHECK(callback_count == 0);
    }

    SECTION("CallbackReentrancyIsPrevented")
    {
        vector<string> captured;

        set_log_callback("reentrant", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) {
            captured.emplace_back(message);

            if (captured.size() == 1) {
                write_log_message(log_type::info, "nested callback log");
            }
        });

        write_log_message(log_type::info, "outer callback log");

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("outer callback log") != string::npos);

        set_log_callback("reentrant", {});
    }

    SECTION("StringViewOverloadDeliversRawText")
    {
        vector<string> captured;

        set_log_callback("sv", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured.emplace_back(message); });

        string raw = "raw {} payload"; // Curly braces should NOT be interpreted as format placeholders
        write_log(string_view {raw});

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("raw {} payload") != string::npos);

        set_log_callback("sv", {});
    }

    SECTION("LogTypePreservesMessageContent")
    {
        vector<string> captured;

        set_log_callback("type", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured.emplace_back(message); });

        write_log(log_type::info, "info-line");
        write_log(log_type::info_section, "section-line");
        write_log(log_type::warning, "notice-line");
        write_log(log_type::error, "error-line");

        REQUIRE(captured.size() == 4);
        CHECK(captured[0].find("info-line") != string::npos);
        CHECK(captured[1].find("section-line") != string::npos);
        CHECK(captured[2].find("notice-line") != string::npos);
        CHECK(captured[3].find("error-line") != string::npos);

        set_log_callback("type", {});
    }

    SECTION("RepeatedMessagesAreCollapsedUntilDifferentMessage")
    {
        vector<string> captured;

        set_log_callback("repeat", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured.emplace_back(message); });

        write_log_message(log_type::warning, "repeat-collapse");
        write_log_message(log_type::warning, "repeat-collapse");
        write_log_message(log_type::warning, "repeat-collapse");

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("repeat-collapse") != string::npos);

        write_log_message(log_type::warning, "repeat-collapse-next");

        REQUIRE(captured.size() == 3);
        CHECK(captured[1].find("...and 2 more same messages") != string::npos);
        CHECK(captured[2].find("repeat-collapse-next") != string::npos);

        set_log_callback("repeat", {});
    }

    SECTION("MultipleCallbacksFireForEachMessage")
    {
        int32_t first_count = 0;
        int32_t second_count = 0;
        string last_first;
        string last_second;

        set_log_callback("first", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) {
            first_count++;
            last_first = string(message);
        });
        set_log_callback("second", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) {
            second_count++;
            last_second = string(message);
        });

        write_log("broadcast {}", 7);

        CHECK(first_count == 1);
        CHECK(second_count == 1);
        CHECK(last_first.find("broadcast 7") != string::npos);
        CHECK(last_second.find("broadcast 7") != string::npos);

        // Removing one key must leave the other intact
        set_log_callback("first", {});
        write_log("only second");

        CHECK(first_count == 1);
        CHECK(second_count == 2);
        CHECK(last_second.find("only second") != string::npos);

        set_log_callback("second", {});
    }

    SECTION("MessageHasTimestampPrefix")
    {
        vector<string> captured;

        set_log_callback("tag", [&](log_type, string_view message, nptr<const catched_stack_trace_data>) { captured.emplace_back(message); });
        write_log("tagged");

        REQUIRE(captured.size() == 1);
        // Default tagging adds [DD/MM/YY] [HH:MM:SS] before the message body
        CHECK(captured.front().starts_with('['));
        CHECK(captured.front().find("] [") != string::npos);
        CHECK(captured.front().find("tagged") != string::npos);

        set_log_callback("tag", {});
    }

    set_log_callback("", {});
}

FO_END_NAMESPACE
