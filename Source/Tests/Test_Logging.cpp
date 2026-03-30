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

        SetLogCallback("capture", [&](string_view message) { captured.emplace_back(message); });
        WriteLog("Hello {}", 42);

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("Hello 42") != string::npos);
        CHECK(captured.front().ends_with('\n'));

        SetLogCallback("capture", {});
    }

    SECTION("CallbackReplacementUsesLastRegisteredHandler")
    {
        int32 first_count = 0;
        int32 second_count = 0;

        SetLogCallback("replace", [&](string_view) { first_count++; });
        SetLogCallback("replace", [&](string_view) { second_count++; });
        WriteLog(LogType::Warning, "Replacement {}", 1);

        CHECK(first_count == 0);
        CHECK(second_count == 1);

        SetLogCallback("replace", {});
    }

    SECTION("CallbackCanBeClearedByEmptyKey")
    {
        int32 callback_count = 0;

        SetLogCallback("clear-me", [&](string_view) { callback_count++; });
        SetLogCallback("", {});
        WriteLogMessage(LogType::Error, "should not hit callback");

        CHECK(callback_count == 0);
    }

    SECTION("CallbackReentrancyIsPrevented")
    {
        vector<string> captured;

        SetLogCallback("reentrant", [&](string_view message) {
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

    SetLogCallback("", {});
}

FO_END_NAMESPACE
