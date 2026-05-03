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
//

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

namespace
{
    class TestBase
    {
    public:
        virtual ~TestBase() = default;
    };

    class TestDerived final : public TestBase
    {
    public:
        explicit TestDerived(int32_t v) :
            Value {v}
        {
        }

        int32_t Value {};
    };

    class TestRefCounter final
    {
    public:
        void AddRef() { ++RefCount; }
        void Release() { --RefCount; }

        int32_t RefCount {};
    };
}

TEST_CASE("CommonHelpers")
{
    SECTION("SafeCall")
    {
        bool called = false;
        CHECK_NOTHROW(safe_call([&]() {
            called = true;
            throw std::runtime_error("test");
        }));
        CHECK(called);
    }

    SECTION("DynamicPtrCastUnique")
    {
        unique_ptr<TestBase> base = SafeAlloc::MakeUnique<TestDerived>(42);
        auto derived = dynamic_ptr_cast<TestDerived>(std::move(base));
        REQUIRE(derived);
        CHECK(derived->Value == 42);
    }

    SECTION("DynamicPtrCastShared")
    {
        shared_ptr<TestDerived> derived = SafeAlloc::MakeShared<TestDerived>(77);
        shared_ptr<TestBase> base = derived;

        auto casted = dynamic_ptr_cast<TestDerived>(base);
        REQUIRE(casted);
        CHECK(casted->Value == 77);
    }

    SECTION("MakeIfNotExistsDestroyIfEmpty")
    {
        unique_ptr<vector<int32_t>> values;
        make_if_not_exists(values);
        REQUIRE(values);
        CHECK(values->empty());

        destroy_if_empty(values);
        CHECK_FALSE(values);

        make_if_not_exists(values);
        values->emplace_back(1);
        destroy_if_empty(values);
        CHECK(values);
    }

    SECTION("VectorSafeHelpers")
    {
        vector<int32_t> values = {1, 2};

        CHECK(vec_safe_add_unique_value(values, 3));
        CHECK_FALSE(vec_safe_add_unique_value(values, 3));
        CHECK(vec_safe_remove_unique_value(values, 2));
        CHECK_FALSE(vec_safe_remove_unique_value(values, 9));
        CHECK(vec_safe_remove_unique_value_if(values, [](int32_t v) { return v == 1; }));

        CHECK(values.size() == 1);
        CHECK(values.front() == 3);
    }

    SECTION("VectorAlgorithms")
    {
        const vector<int32_t> values = {5, 1, 3, 2};

        const auto filtered = vec_filter(values, [](int32_t v) { return v % 2 == 1; });
        CHECK(filtered == vector<int32_t> {5, 1, 3});

        const auto transformed = vec_transform(values, [](int32_t v) -> int32_t { return v * 10; });
        CHECK(transformed == vector<int32_t> {50, 10, 30, 20});

        const auto sorted = vec_sorted(values, [](int32_t l, int32_t r) { return l < r; });
        CHECK(sorted == vector<int32_t> {1, 2, 3, 5});

        CHECK(vec_exists(values, 3));
        CHECK_FALSE(vec_exists(values, 7));

        const set<int32_t> uniq = {9, 8, 7};
        const auto copied = to_vector(uniq);
        CHECK(copied.size() == 3);
    }

    SECTION("VecTransformSupportsPointerLikeValues")
    {
        vector<unique_ptr<TestDerived>> values;
        values.emplace_back(SafeAlloc::MakeUnique<TestDerived>(4));
        values.emplace_back(SafeAlloc::MakeUnique<TestDerived>(9));

        const auto transformed = vec_transform(values, [](const auto& entry) -> int32_t { return entry->Value; });
        CHECK(transformed == vector<int32_t> {4, 9});
    }

    SECTION("CopyHoldRefPreservesPointerVector")
    {
        TestRefCounter first;
        TestRefCounter second;
        vector<TestRefCounter*> refs = {&first, &second};

        {
            auto held = copy_hold_ref(refs);

            CHECK(refs.size() == 2);
            CHECK(refs[0] == &first);
            CHECK(refs[1] == &second);
            CHECK(first.RefCount == 1);
            CHECK(second.RefCount == 1);
        }

        CHECK(refs.size() == 2);
        CHECK(first.RefCount == 0);
        CHECK(second.RefCount == 0);
    }
}

FO_END_NAMESPACE
