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

#include "catch_amalgamated.hpp"

#include "Logging.h"

FO_BEGIN_NAMESPACE

TEST_CASE("Logging")
{
    SetLogCallback("", {});

    SECTION("CallbackReceivesFormattedMessage")
    {
        vector<string> captured;

        SetLogCallback("capture", [&](LogType, string_view message, const CatchedStackTraceData*) { captured.emplace_back(message); });
        WriteLog("Hello {}", 42);

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("Hello 42") != string::npos);
        CHECK(captured.front().ends_with('\n'));

        SetLogCallback("capture", {});
    }

    SECTION("CallbackReplacementUsesLastRegisteredHandler")
    {
        int32_t first_count = 0;
        int32_t second_count = 0;

        SetLogCallback("replace", [&](LogType, string_view, const CatchedStackTraceData*) { first_count++; });
        SetLogCallback("replace", [&](LogType, string_view, const CatchedStackTraceData*) { second_count++; });
        WriteLog(LogType::Warning, "Replacement {}", 1);

        CHECK(first_count == 0);
        CHECK(second_count == 1);

        SetLogCallback("replace", {});
    }

    SECTION("CallbackCanBeClearedByEmptyKey")
    {
        int32_t callback_count = 0;

        SetLogCallback("clear-me", [&](LogType, string_view, const CatchedStackTraceData*) { callback_count++; });
        SetLogCallback("", {});
        WriteLogMessage(LogType::Error, "should not hit callback");

        CHECK(callback_count == 0);
    }

    SECTION("CallbackReentrancyIsPrevented")
    {
        vector<string> captured;

        SetLogCallback("reentrant", [&](LogType, string_view message, const CatchedStackTraceData*) {
            captured.emplace_back(message);

            if (captured.size() == 1) {
                WriteLogMessage(LogType::Info, "nested callback log");
            }
        });

        WriteLogMessage(LogType::Info, "outer callback log");

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("outer callback log") != string::npos);

        SetLogCallback("reentrant", {});
    }

    SECTION("StringViewOverloadDeliversRawText")
    {
        vector<string> captured;

        SetLogCallback("sv", [&](LogType, string_view message, const CatchedStackTraceData*) { captured.emplace_back(message); });

        const string raw = "raw {} payload"; // Curly braces should NOT be interpreted as format placeholders.
        WriteLog(string_view {raw});

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("raw {} payload") != string::npos);

        SetLogCallback("sv", {});
    }

    SECTION("LogTypePreservesMessageContent")
    {
        vector<string> captured;

        SetLogCallback("type", [&](LogType, string_view message, const CatchedStackTraceData*) { captured.emplace_back(message); });

        WriteLog(LogType::Info, "info-line");
        WriteLog(LogType::InfoSection, "section-line");
        WriteLog(LogType::Warning, "warning-line");
        WriteLog(LogType::Error, "error-line");

        REQUIRE(captured.size() == 4);
        CHECK(captured[0].find("info-line") != string::npos);
        CHECK(captured[1].find("section-line") != string::npos);
        CHECK(captured[2].find("warning-line") != string::npos);
        CHECK(captured[3].find("error-line") != string::npos);

        SetLogCallback("type", {});
    }

    SECTION("MultipleCallbacksFireForEachMessage")
    {
        int32_t first_count = 0;
        int32_t second_count = 0;
        string last_first;
        string last_second;

        SetLogCallback("first", [&](LogType, string_view message, const CatchedStackTraceData*) {
            first_count++;
            last_first = string(message);
        });
        SetLogCallback("second", [&](LogType, string_view message, const CatchedStackTraceData*) {
            second_count++;
            last_second = string(message);
        });

        WriteLog("broadcast {}", 7);

        CHECK(first_count == 1);
        CHECK(second_count == 1);
        CHECK(last_first.find("broadcast 7") != string::npos);
        CHECK(last_second.find("broadcast 7") != string::npos);

        // Removing one key must leave the other intact.
        SetLogCallback("first", {});
        WriteLog("only second");

        CHECK(first_count == 1);
        CHECK(second_count == 2);
        CHECK(last_second.find("only second") != string::npos);

        SetLogCallback("second", {});
    }

    SECTION("MessageHasTimestampPrefix")
    {
        vector<string> captured;

        SetLogCallback("tag", [&](LogType, string_view message, const CatchedStackTraceData*) { captured.emplace_back(message); });
        WriteLog("tagged");

        REQUIRE(captured.size() == 1);
        // Default tagging adds [DD/MM/YY] [HH:MM:SS] before the message body.
        CHECK(captured.front().starts_with('['));
        CHECK(captured.front().find("] [") != string::npos);
        CHECK(captured.front().find("tagged") != string::npos);

        SetLogCallback("tag", {});
    }

    SetLogCallback("", {});
}

FO_END_NAMESPACE
