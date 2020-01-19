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
#define UNREACHABLE_PLACE throw UnreachablePlaceException("Unreachable place")

DECLARE_EXCEPTION(UnreachablePlaceException);

extern void CatchExceptions(const string& app_name, int app_ver);
extern void CreateDump(const string& appendix, const string& message);
extern bool RaiseAssert(const string& message, const string& file, int line);
