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

#include "Containers.h"
#include "DequeObject.h"

FO_BEGIN_NAMESPACE

namespace
{
    // A block boundary every four elements, so every test crosses several of them
    using test_deque = basic_deque<int32_t, 4 * sizeof(int32_t)>;

    int32_t AliveCounter = 0;

    class Tracked
    {
    public:
        explicit Tracked(int32_t value) noexcept :
            Value {value}
        {
            AliveCounter++;
        }
        Tracked(const Tracked& other) noexcept :
            Value {other.Value}
        {
            AliveCounter++;
        }
        Tracked(Tracked&& other) noexcept :
            Value {other.Value}
        {
            AliveCounter++;
        }
        auto operator=(const Tracked& other) noexcept -> Tracked&
        {
            Value = other.Value;
            return *this;
        }
        auto operator=(Tracked&& other) noexcept -> Tracked&
        {
            Value = other.Value;
            return *this;
        }
        ~Tracked() { AliveCounter--; }

        int32_t Value;
    };

    using tracked_deque = basic_deque<Tracked, 4 * sizeof(Tracked)>;

    auto Collect(const test_deque& target) -> vector<int32_t>
    {
        vector<int32_t> result;

        for (int32_t value : target) {
            result.emplace_back(value);
        }

        return result;
    }
}

TEST_CASE("DequeObject")
{
    SECTION("BlockSizeFollowsTheByteBudgetWithAFloor")
    {
        STATIC_REQUIRE(basic_deque<int32_t, 64>::block_elements == 16);
        STATIC_REQUIRE(basic_deque<int64_t, 64>::block_elements == 8);

        // A wide element would otherwise land one block per element, which is the std::deque defect
        STATIC_REQUIRE(basic_deque<std::array<uint8_t, 256>, 64>::block_elements == DEQUE_MIN_BLOCK_ELEMENTS);
        STATIC_REQUIRE(deque<int32_t>::block_elements == DEQUE_BLOCK_BYTES / sizeof(int32_t));
    }

    SECTION("EmptyDequeHoldsNothing")
    {
        test_deque target;

        CHECK(target.empty());
        CHECK(target.size() == 0);
        CHECK(target.begin() == target.end());
    }

    SECTION("PushBackCrossesBlockBoundaries")
    {
        test_deque target;

        for (int32_t i = 0; i < 21; i++) {
            target.push_back(i);
        }

        CHECK(target.size() == 21);
        CHECK(target.front() == 0);
        CHECK(target.back() == 20);

        for (size_t i = 0; i < target.size(); i++) {
            CHECK(target[i] == static_cast<int32_t>(i));
        }
    }

    SECTION("PushFrontCrossesBlockBoundaries")
    {
        test_deque target;

        for (int32_t i = 0; i < 21; i++) {
            target.push_front(i);
        }

        CHECK(target.size() == 21);
        CHECK(target.front() == 20);
        CHECK(target.back() == 0);
        CHECK(Collect(target) == vector<int32_t>({20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}));
    }

    SECTION("GrowthAtEitherEndKeepsReferencesValid")
    {
        test_deque target;
        target.push_back(100);

        ptr<int32_t> first = &target.front();

        for (int32_t i = 0; i < 50; i++) {
            target.push_back(i);
            target.push_front(i);
        }

        CHECK(*first == 100);
        CHECK(&target[50] == first.get());
    }

    SECTION("SteadyQueueTraffic")
    {
        // The shape every engine deque has: append at the back, consume from the front, indefinitely
        test_deque target;
        int32_t pushed = 0;
        int32_t popped = 0;

        for (int32_t round = 0; round < 200; round++) {
            target.push_back(pushed++);
            target.push_back(pushed++);

            if (target.size() > 5) {
                CHECK(target.front() == popped++);
                target.pop_front();
            }
        }

        CHECK(target.size() == 400 - popped);
        CHECK(target.front() == popped);
        CHECK(target.back() == pushed - 1);
    }

    SECTION("PopFromBothEnds")
    {
        test_deque target;

        for (int32_t i = 0; i < 20; i++) {
            target.push_back(i);
        }

        for (int32_t i = 0; i < 6; i++) {
            target.pop_front();
            target.pop_back();
        }

        CHECK(target.size() == 8);
        CHECK(Collect(target) == vector<int32_t>({6, 7, 8, 9, 10, 11, 12, 13}));
    }

    SECTION("EraseShiftsTowardTheNearerEnd")
    {
        test_deque front_half;
        test_deque back_half;

        for (int32_t i = 0; i < 10; i++) {
            front_half.push_back(i);
            back_half.push_back(i);
        }

        auto erased_front = front_half.erase(front_half.begin() + 2);
        CHECK(*erased_front == 3);
        CHECK(Collect(front_half) == vector<int32_t>({0, 1, 3, 4, 5, 6, 7, 8, 9}));

        auto erased_back = back_half.erase(back_half.begin() + 7);
        CHECK(*erased_back == 8);
        CHECK(Collect(back_half) == vector<int32_t>({0, 1, 2, 3, 4, 5, 6, 8, 9}));

        test_deque last;
        last.push_back(1);
        last.push_back(2);

        auto erased_last = last.erase(last.begin() + 1);
        CHECK(erased_last == last.end());
        CHECK(Collect(last) == vector<int32_t>({1}));
    }

    SECTION("IteratorsWalkForwardAndBackward")
    {
        test_deque target;

        for (int32_t i = 0; i < 11; i++) {
            target.push_back(i);
        }

        CHECK(target.end() - target.begin() == 11);
        CHECK(*(target.begin() + 7) == 7);

        vector<int32_t> reversed;

        for (auto it = target.rbegin(); it != target.rend(); ++it) {
            reversed.emplace_back(*it);
        }

        CHECK(reversed == vector<int32_t>({10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}));

        const test_deque& readonly = target;
        CHECK(std::distance(readonly.cbegin(), readonly.cend()) == 11);
        CHECK(*readonly.crbegin() == 10);
    }

    SECTION("CopyMoveAndSwap")
    {
        test_deque source;

        for (int32_t i = 0; i < 9; i++) {
            source.push_back(i);
        }

        test_deque copy = source;
        CHECK(Collect(copy) == Collect(source));

        test_deque moved = std::move(source);
        CHECK(Collect(moved) == Collect(copy));
        CHECK(source.empty());

        test_deque other;
        other.push_back(99);
        other.swap(moved);

        CHECK(Collect(other) == Collect(copy));
        CHECK(Collect(moved) == vector<int32_t>({99}));

        test_deque assigned;
        assigned = other;
        CHECK(Collect(assigned) == Collect(copy));

        assigned = std::move(moved);
        CHECK(Collect(assigned) == vector<int32_t>({99}));
    }

    SECTION("EveryElementIsDestroyed")
    {
        AliveCounter = 0;

        {
            tracked_deque target;

            for (int32_t i = 0; i < 30; i++) {
                target.emplace_back(i);
                target.emplace_front(i);
            }

            CHECK(AliveCounter == 60);

            for (int32_t i = 0; i < 10; i++) {
                target.pop_front();
                target.pop_back();
            }

            CHECK(AliveCounter == 40);

            target.erase(target.begin() + 5);
            CHECK(AliveCounter == 39);

            tracked_deque copy = target;
            CHECK(AliveCounter == 78);

            target.clear();
            CHECK(target.empty());
            CHECK(AliveCounter == 39);
        }

        CHECK(AliveCounter == 0);
    }

    SECTION("ClearedDequeIsReusable")
    {
        test_deque target;

        for (int32_t i = 0; i < 13; i++) {
            target.push_back(i);
        }

        target.clear();
        CHECK(target.empty());
        CHECK(target.begin() == target.end());

        for (int32_t i = 0; i < 7; i++) {
            target.push_front(i);
        }

        CHECK(Collect(target) == vector<int32_t>({6, 5, 4, 3, 2, 1, 0}));
    }
}

FO_END_NAMESPACE
