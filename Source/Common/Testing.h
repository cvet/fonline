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
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#pragma once

#include "Common.h"

#ifdef FO_TESTING
#define CATCH_CONFIG_PREFIX_ALL
#include "catch.hpp"
#endif

#ifdef FO_TESTING
#define RUNTIME_ASSERT(expr) CATCH_REQUIRE(expr)
#define RUNTIME_ASSERT_STR(expr, str) CATCH_REQUIRE(expr)
#define TEST_CASE() CATCH_ANON_TEST_CASE()
#define TEST_SECTION() CATCH_SECTION(STRINGIZE_INT(__LINE__))
#else
#define RUNTIME_ASSERT(expr) (!!(expr) || RaiseAssert(#expr, __FILE__, __LINE__))
#define RUNTIME_ASSERT_STR(expr, str) (!!(expr) || RaiseAssert(str, __FILE__, __LINE__))
#define TEST_CASE() [[maybe_unused]] static void UNIQUE_FUNCTION_NAME(test_case_)
#define TEST_SECTION() if (!!(__LINE__))
#endif

DECLARE_EXCEPTION(UnreachablePlaceException);

extern void CatchExceptions(const string& app_name, int app_ver);
extern void CreateDump(const string& appendix, const string& message);
extern bool RaiseAssert(const string& message, const string& file, int line);
