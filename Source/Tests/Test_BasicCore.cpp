// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

FO_BEGIN_NAMESPACE

TEST_CASE("BasicCore")
{
    char buf[64] {};

    SECTION("ItoaDecimal")
    {
        CHECK(string(itoa(0, buf, 10)) == "0");
        CHECK(string(itoa(123456789, buf, 10)) == "123456789");
        CHECK(string(itoa(-987654321, buf, 10)) == "-987654321");
    }

    SECTION("ItoaDecimalMinInt64")
    {
        CHECK(string(itoa(std::numeric_limits<int64_t>::min(), buf, 10)) == "-9223372036854775808");
    }

    SECTION("ItoaHex")
    {
        CHECK(string(itoa(0x7fff, buf, 16)) == "7fff");
        CHECK(string(itoa(0x1234abcd, buf, 16)) == "1234abcd");
    }
}

TEST_CASE("ScopeSuccessExceptionSpecification", "[scope-success]")
{
    using fallible_guard = decltype(scope_success([] { throw std::runtime_error("Success callback failed"); }));
    using infallible_guard = decltype(scope_success([]() noexcept { }));

    CHECK_FALSE(std::is_nothrow_destructible_v<fallible_guard>);
    CHECK(std::is_nothrow_destructible_v<infallible_guard>);
}

TEST_CASE("ScopeSuccess", "[scope-success]")
{
    int32_t calls {0};

    SECTION("RunsOnNormalExit")
    {
        {
            auto guard {scope_success([&]() noexcept { ++calls; })};
            CHECK(calls == 0);
        }
        CHECK(calls == 1);
    }

    SECTION("RunsOnEarlyReturn")
    {
        auto operation {[&]() {
            auto guard {scope_success([&]() noexcept { ++calls; })};
            return 42;
        }};

        CHECK(operation() == 42);
        CHECK(calls == 1);
    }

    SECTION("SkipsCallbackDuringUnwind")
    {
        auto operation {[&]() {
            auto guard {scope_success([&]() {
                ++calls;
                throw std::runtime_error("Success callback failed");
            })};
            throw std::runtime_error("Operation failed");
        }};

        CHECK_THROWS_WITH(operation(), "Operation failed");
        CHECK(calls == 0);
    }

    SECTION("PropagatesCallbackException")
    {
        auto operation {[&]() {
            auto guard {scope_success([&]() {
                ++calls;
                throw std::runtime_error("Success callback failed");
            })};
        }};

        CHECK_THROWS_WITH(operation(), "Success callback failed");
        CHECK(calls == 1);
    }

    SECTION("ReleaseSkipsCallback")
    {
        {
            auto guard {scope_success([&]() noexcept { ++calls; })};
            guard.release();
        }
        CHECK(calls == 0);
    }
}

FO_END_NAMESPACE
