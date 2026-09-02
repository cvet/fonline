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
    logging::set_callback("", {});

    SECTION("CallbackReceivesFormattedMessage")
    {
        vector<string> captured;

        logging::set_callback("capture", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) { captured.emplace_back(message); });
        logging::write("Hello {}", 42);

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("Hello 42") != string::npos);
        CHECK(captured.front().ends_with('\n'));

        logging::set_callback("capture", {});
    }

    SECTION("CallbackReplacementUsesLastRegisteredHandler")
    {
        int32_t first_count = 0;
        int32_t second_count = 0;

        logging::set_callback("replace", [&](logging::type, string_view, nptr<const stack_trace::catched_data>) { first_count++; });
        logging::set_callback("replace", [&](logging::type, string_view, nptr<const stack_trace::catched_data>) { second_count++; });
        logging::write(logging::type::warning, "Replacement {}", 1);

        CHECK(first_count == 0);
        CHECK(second_count == 1);

        logging::set_callback("replace", {});
    }

    SECTION("CallbackCanBeClearedByEmptyKey")
    {
        int32_t callback_count = 0;

        logging::set_callback("clear-me", [&](logging::type, string_view, nptr<const stack_trace::catched_data>) { callback_count++; });
        logging::set_callback("", {});
        logging::write_message(logging::type::error, "should not hit callback");

        CHECK(callback_count == 0);
    }

    SECTION("CallbackReentrancyIsPrevented")
    {
        vector<string> captured;

        logging::set_callback("reentrant", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) {
            captured.emplace_back(message);

            if (captured.size() == 1) {
                logging::write_message(logging::type::info, "nested callback log");
            }
        });

        logging::write_message(logging::type::info, "outer callback log");

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("outer callback log") != string::npos);

        logging::set_callback("reentrant", {});
    }

    SECTION("StringViewOverloadDeliversRawText")
    {
        vector<string> captured;

        logging::set_callback("sv", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) { captured.emplace_back(message); });

        string raw = "raw {} payload"; // Curly braces should NOT be interpreted as format placeholders
        logging::write(string_view {raw});

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("raw {} payload") != string::npos);

        logging::set_callback("sv", {});
    }

    SECTION("LogTypePreservesMessageContent")
    {
        vector<string> captured;

        logging::set_callback("type", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) { captured.emplace_back(message); });

        logging::write(logging::type::info, "info-line");
        logging::write(logging::type::info_section, "section-line");
        logging::write(logging::type::warning, "notice-line");
        logging::write(logging::type::error, "error-line");

        REQUIRE(captured.size() == 4);
        CHECK(captured[0].find("info-line") != string::npos);
        CHECK(captured[1].find("section-line") != string::npos);
        CHECK(captured[2].find("notice-line") != string::npos);
        CHECK(captured[3].find("error-line") != string::npos);

        logging::set_callback("type", {});
    }

    SECTION("RepeatedMessagesAreCollapsedUntilDifferentMessage")
    {
        vector<string> captured;

        logging::set_callback("repeat", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) { captured.emplace_back(message); });

        logging::write_message(logging::type::warning, "repeat-collapse");
        logging::write_message(logging::type::warning, "repeat-collapse");
        logging::write_message(logging::type::warning, "repeat-collapse");

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("repeat-collapse") != string::npos);

        logging::write_message(logging::type::warning, "repeat-collapse-next");

        REQUIRE(captured.size() == 3);
        CHECK(captured[1].find("...and 2 more same messages") != string::npos);
        CHECK(captured[2].find("repeat-collapse-next") != string::npos);

        logging::set_callback("repeat", {});
    }

    SECTION("MultipleCallbacksFireForEachMessage")
    {
        int32_t first_count = 0;
        int32_t second_count = 0;
        string last_first;
        string last_second;

        logging::set_callback("first", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) {
            first_count++;
            last_first = string(message);
        });
        logging::set_callback("second", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) {
            second_count++;
            last_second = string(message);
        });

        logging::write("broadcast {}", 7);

        CHECK(first_count == 1);
        CHECK(second_count == 1);
        CHECK(last_first.find("broadcast 7") != string::npos);
        CHECK(last_second.find("broadcast 7") != string::npos);

        // Removing one key must leave the other intact
        logging::set_callback("first", {});
        logging::write("only second");

        CHECK(first_count == 1);
        CHECK(second_count == 2);
        CHECK(last_second.find("only second") != string::npos);

        logging::set_callback("second", {});
    }

    SECTION("MessageHasTimestampPrefix")
    {
        vector<string> captured;

        logging::set_callback("tag", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) { captured.emplace_back(message); });
        logging::write("tagged");

        REQUIRE(captured.size() == 1);
        // Default tagging adds [DD/MM/YY] [HH:MM:SS] before the message body
        CHECK(captured.front().starts_with('['));
        CHECK(captured.front().find("] [") != string::npos);
        CHECK(captured.front().find("tagged") != string::npos);

        logging::set_callback("tag", {});
    }

    logging::set_callback("", {});
}

FO_END_NAMESPACE
