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

#include "Common.h"

FO_BEGIN_NAMESPACE

template<typename T>
concept CanWriteLogMessage = requires(T&& message) { WriteLogMessage(LogType::Info, std::forward<T>(message)); };

template<typename T>
concept CanSetLogCallback = requires(T&& key) { SetLogCallback(std::forward<T>(key), LogFunc {}); };

static_assert(CanWriteLogMessage<u8string_view>);
static_assert(!CanWriteLogMessage<string_view>);
static_assert(!CanWriteLogMessage<string&>);
static_assert(CanSetLogCallback<string_view>);
static_assert(CanSetLogCallback<string&>);
static_assert(!CanSetLogCallback<u8string_view>);

TEST_CASE("Logging")
{
    SetLogCallback("", {});

    SECTION("CallbackReceivesFormattedMessage")
    {
        vector<string> captured;

        SetLogCallback("capture", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.emplace_back(utf8_to_char_string(message)); });
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

        SetLogCallback("replace", [&](LogType, u8string_view, nptr<const CatchedStackTraceData>) { first_count++; });
        SetLogCallback("replace", [&](LogType, u8string_view, nptr<const CatchedStackTraceData>) { second_count++; });
        WriteLog(LogType::Warning, "Replacement {}", 1);

        CHECK(first_count == 0);
        CHECK(second_count == 1);

        SetLogCallback("replace", {});
    }

    SECTION("CallbackCanBeClearedByEmptyKey")
    {
        int32_t callback_count = 0;

        SetLogCallback("clear-me", [&](LogType, u8string_view, nptr<const CatchedStackTraceData>) { callback_count++; });
        SetLogCallback("", {});
        WriteLogMessage(LogType::Error, u8"should not hit callback");

        CHECK(callback_count == 0);
    }

    SECTION("CallbackReentrancyIsPrevented")
    {
        vector<string> captured;

        SetLogCallback("reentrant", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) {
            captured.emplace_back(utf8_to_char_string(message));

            if (captured.size() == 1) {
                WriteLogMessage(LogType::Info, u8"nested callback log");
            }
        });

        WriteLogMessage(LogType::Info, u8"outer callback log");

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("outer callback log") != string::npos);

        SetLogCallback("reentrant", {});
    }

    SECTION("StrictUtf8ViewDeliversRawText")
    {
        vector<string> captured;

        SetLogCallback("sv", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.emplace_back(utf8_to_char_string(message)); });

        const u8string raw {u8"raw {} payload"}; // Curly braces should NOT be interpreted as format placeholders.
        WriteLog(raw.view());

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("raw {} payload") != string::npos);

        SetLogCallback("sv", {});
    }

    SECTION("LogTypePreservesMessageContent")
    {
        vector<string> captured;

        SetLogCallback("type", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.emplace_back(utf8_to_char_string(message)); });

        WriteLog(LogType::Info, "info-line");
        WriteLog(LogType::InfoSection, "section-line");
        WriteLog(LogType::Warning, "notice-line");
        WriteLog(LogType::Error, "error-line");

        REQUIRE(captured.size() == 4);
        CHECK(captured[0].find("info-line") != string::npos);
        CHECK(captured[1].find("section-line") != string::npos);
        CHECK(captured[2].find("notice-line") != string::npos);
        CHECK(captured[3].find("error-line") != string::npos);

        SetLogCallback("type", {});
    }

    SECTION("RepeatedMessagesAreCollapsedUntilDifferentMessage")
    {
        vector<string> captured;

        SetLogCallback("repeat", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.emplace_back(utf8_to_char_string(message)); });

        WriteLogMessage(LogType::Warning, u8"repeat-collapse");
        WriteLogMessage(LogType::Warning, u8"repeat-collapse");
        WriteLogMessage(LogType::Warning, u8"repeat-collapse");

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("repeat-collapse") != string::npos);

        WriteLogMessage(LogType::Warning, u8"repeat-collapse-next");

        REQUIRE(captured.size() == 3);
        CHECK(captured[1].find("...and 2 more same messages") != string::npos);
        CHECK(captured[2].find("repeat-collapse-next") != string::npos);

        SetLogCallback("repeat", {});
    }

    SECTION("MultipleCallbacksFireForEachMessage")
    {
        int32_t first_count = 0;
        int32_t second_count = 0;
        string last_first;
        string last_second;

        SetLogCallback("first", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) {
            first_count++;
            last_first = utf8_to_char_string(message);
        });
        SetLogCallback("second", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) {
            second_count++;
            last_second = utf8_to_char_string(message);
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

        SetLogCallback("tag", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.emplace_back(utf8_to_char_string(message)); });
        WriteLog("tagged");

        REQUIRE(captured.size() == 1);
        // Default tagging adds [DD/MM/YY] [HH:MM:SS] before the message body.
        CHECK(captured.front().starts_with('['));
        CHECK(captured.front().find("] [") != string::npos);
        CHECK(captured.front().find("tagged") != string::npos);

        SetLogCallback("tag", {});
    }

    SECTION("StrictUtf8MessagePreservesUnicode")
    {
        u8string captured;

        SetLogCallback("unicode", [&captured](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.assign(message); });
        WriteLogMessage(LogType::Info, u8"Привет, é, 🌍");

        CHECK(captured.view().native_view().find(u8"Привет, é, 🌍") != std::u8string_view::npos);

        SetLogCallback("unicode", {});
    }

    SECTION("StrictUtf8FormattingNeedsNoCharacterAdapter")
    {
        u8string captured;
        const u8string path {u8"Ресурсы/заставка-🌍.png"};

        SetLogCallback("unicode-format", [&captured](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.assign(message); });
        WriteLog("Loading {}", path);

        CHECK(captured.view().native_view().find(u8"Loading Ресурсы/заставка-🌍.png") != std::u8string_view::npos);

        WriteLog(u8"Загрузка {}", path);

        CHECK(captured.view().native_view().find(u8"Загрузка Ресурсы/заставка-🌍.png") != std::u8string_view::npos);

        SetLogCallback("unicode-format", {});
    }

    SECTION("NativeExceptionMessagePromotesDirectlyToUtf8")
    {
        u8string captured;
        const string error_chars = utf8_to_char_string(u8"соединение разорвано 🌍");
        const std::runtime_error ex {error_chars.c_str()};

        SetLogCallback("exception-what", [&captured](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.assign(message); });
        WriteLog("Connection error: {}", ex.what());

        CHECK(captured.view().native_view().find(u8"Connection error: соединение разорвано 🌍") != std::u8string_view::npos);

        SetLogCallback("exception-what", {});
    }

    SECTION("PublicNarrowCharacterArgumentRejectsMalformedUtf8WithoutEscapingNoexcept")
    {
        vector<string> captured;

        SetLogCallback("malformed", [&](LogType, u8string_view message, nptr<const CatchedStackTraceData>) { captured.emplace_back(utf8_to_char_string(message)); });

        const string malformed(1, std::bit_cast<char>(uint8_t {0xFF}));
        WriteLog("{}", malformed.c_str());

        REQUIRE(captured.size() == 1);
        CHECK(captured.front().find("Log message rejected: invalid UTF-8") != string::npos);

        SetLogCallback("malformed", {});
    }

    SetLogCallback("", {});
}

FO_END_NAMESPACE
