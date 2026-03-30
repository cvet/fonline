//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include <iostream>
#include <sstream>

#include "StackTrace.h"

FO_BEGIN_NAMESPACE

static constexpr SourceLocationData FirstStackLocation {nullptr, "FirstFunc", "/tmp/first.cpp", 11};
static constexpr SourceLocationData SecondStackLocation {nullptr, "SecondFunc", "/tmp/second.cpp", 22};

static void ResetTestStackTrace() noexcept
{
    details::StackTrace = {};
}

TEST_CASE("StackTrace")
{
    ResetTestStackTrace();

    SECTION("PushPopTracksNewestEntryFirst")
    {
        CHECK(GetStackTrace().CallsCount == 0);
        CHECK(GetStackTraceEntry(0) == nullptr);

        PushStackTrace(&FirstStackLocation);
        PushStackTrace(&SecondStackLocation);

        const StackTraceData& st = GetStackTrace();
        CHECK(st.CallsCount == 2);
        CHECK(GetStackTraceEntry(0) == &SecondStackLocation);
        CHECK(GetStackTraceEntry(1) == &FirstStackLocation);
        CHECK(GetStackTraceEntry(2) == nullptr);

        PopStackTrace();

        CHECK(GetStackTrace().CallsCount == 1);
        CHECK(GetStackTraceEntry(0) == &FirstStackLocation);

        PopStackTrace();

        CHECK(GetStackTrace().CallsCount == 0);
        CHECK(GetStackTraceEntry(0) == nullptr);
    }

#if !FO_NO_MANUAL_STACK_TRACE
    SECTION("ScopeEntryRestoresDepthOnDestruction")
    {
        {
            StackTraceScopeEntry first_entry {FirstStackLocation};

            CHECK(GetStackTrace().CallsCount == 1);
            CHECK(GetStackTraceEntry(0) == &FirstStackLocation);

            {
                StackTraceScopeEntry second_entry {SecondStackLocation};

                CHECK(GetStackTrace().CallsCount == 2);
                CHECK(GetStackTraceEntry(0) == &SecondStackLocation);
                CHECK(GetStackTraceEntry(1) == &FirstStackLocation);
            }

            CHECK(GetStackTrace().CallsCount == 1);
            CHECK(GetStackTraceEntry(0) == &FirstStackLocation);
        }

        CHECK(GetStackTrace().CallsCount == 0);
        CHECK(GetStackTraceEntry(0) == nullptr);
    }
#endif

    SECTION("SafeWriteStackTraceWritesShortFileNames")
    {
        StackTraceData st {};
        st.CallsCount = 2;
        st.CallTree[0] = &FirstStackLocation;
        st.CallTree[1] = &SecondStackLocation;

        std::ostringstream captured;
        std::streambuf* prev_buf = std::cout.rdbuf(captured.rdbuf());

        SafeWriteStackTrace(st);
        std::cout.rdbuf(prev_buf);

        const std::string log_contents = captured.str();

        CHECK(log_contents.find("Stack trace (most recent call first):\n") == 0);
        CHECK(log_contents.find("- SecondFunc (second.cpp line 22)\n") != std::string::npos);
        CHECK(log_contents.find("- FirstFunc (first.cpp line 11)\n") != std::string::npos);
        CHECK(log_contents.ends_with("\n\n"));
    }

    ResetTestStackTrace();
}

FO_END_NAMESPACE
