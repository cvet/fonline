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
#include "FunctionObjects.h"
#include "MemorySystem.h"
#include "SmartPointers.h"

FO_BEGIN_NAMESPACE

namespace
{
    class InstanceCounter
    {
    public:
        explicit InstanceCounter(ptr<int32_t> alive) noexcept :
            _alive {alive}
        {
            ++*_alive;
        }
        InstanceCounter(const InstanceCounter& other) noexcept :
            _alive {other._alive}
        {
            ++*_alive;
        }
        InstanceCounter(InstanceCounter&& other) noexcept :
            _alive {other._alive}
        {
            ++*_alive;
        }
        auto operator=(const InstanceCounter&) -> InstanceCounter& = delete;
        auto operator=(InstanceCounter&&) -> InstanceCounter& = delete;
        ~InstanceCounter() noexcept { --*_alive; }

        auto operator()() const noexcept -> int32_t { return *_alive; }

    private:
        ptr<int32_t> _alive;
    };

    class MoveOnlyTarget
    {
    public:
        MoveOnlyTarget() noexcept = default;
        MoveOnlyTarget(const MoveOnlyTarget&) = delete;
        MoveOnlyTarget(MoveOnlyTarget&&) noexcept = default;
        auto operator=(const MoveOnlyTarget&) -> MoveOnlyTarget& = delete;
        auto operator=(MoveOnlyTarget&&) -> MoveOnlyTarget& = delete;
        ~MoveOnlyTarget() = default;

        auto operator()() const noexcept -> int32_t { return _value; }

    private:
        int32_t _value {17};
    };

    class OversizedTarget
    {
    public:
        explicit OversizedTarget(int32_t value) noexcept { _padding[0] = value; }

        auto operator()() const noexcept -> int32_t { return _padding[0]; }

    private:
        int32_t _padding[FUNCTION_INLINE_TARGET_SIZE] {};
    };

    // A throwing move forces the heap path even for a target that would otherwise fit the inline budget
    class ThrowingMoveTarget
    {
    public:
        explicit ThrowingMoveTarget(int32_t value) noexcept :
            _value {value}
        {
        }
        ThrowingMoveTarget(const ThrowingMoveTarget&) = default;
        // NOLINTNEXTLINE(performance-noexcept-move-constructor)
        ThrowingMoveTarget(ThrowingMoveTarget&& other) :
            _value {other._value}
        {
        }
        auto operator=(const ThrowingMoveTarget&) -> ThrowingMoveTarget& = delete;
        auto operator=(ThrowingMoveTarget&&) -> ThrowingMoveTarget& = delete;
        ~ThrowingMoveTarget() = default;

        auto operator()() const noexcept -> int32_t { return _value; }

    private:
        int32_t _value;
    };

    class MemberTarget
    {
    public:
        auto Triple(int32_t value) const noexcept -> int32_t { return value * 3 + Bias; }

        int32_t Bias {};
    };

    auto DoubleValue(int32_t value) noexcept -> int32_t
    {
        return value * 2;
    }
}

TEST_CASE("FunctionObjects")
{
    SECTION("CompileTimeVocabularyContracts")
    {
        STATIC_REQUIRE(std::is_same_v<function<void()>, move_only_function<void()>>);

        STATIC_REQUIRE(std::is_default_constructible_v<move_only_function<void()>>);
        STATIC_REQUIRE(std::is_constructible_v<move_only_function<void()>, std::nullptr_t>);
        STATIC_REQUIRE(std::is_assignable_v<move_only_function<void()>&, std::nullptr_t>);
        STATIC_REQUIRE(!std::is_copy_constructible_v<move_only_function<void()>>);
        STATIC_REQUIRE(!std::is_copy_assignable_v<move_only_function<void()>>);
        STATIC_REQUIRE(std::is_nothrow_move_constructible_v<move_only_function<void()>>);
        STATIC_REQUIRE(std::is_nothrow_move_assignable_v<move_only_function<void()>>);

        STATIC_REQUIRE(std::is_copy_constructible_v<copyable_function<void()>>);
        STATIC_REQUIRE(std::is_copy_assignable_v<copyable_function<void()>>);
        STATIC_REQUIRE(std::is_nothrow_move_constructible_v<copyable_function<void()>>);

        // A copyable wrapper narrows to the move-only one, never the other way round
        STATIC_REQUIRE(std::is_constructible_v<move_only_function<void()>, copyable_function<void()>&&>);
        STATIC_REQUIRE(std::is_constructible_v<move_only_function<void()>, const copyable_function<void()>&>);
        STATIC_REQUIRE(!std::is_constructible_v<copyable_function<void()>, move_only_function<void()>&&>);

        // A move-only target is accepted by move_only_function alone
        STATIC_REQUIRE(std::is_constructible_v<move_only_function<int32_t()>, MoveOnlyTarget&&>);
        STATIC_REQUIRE(!std::is_constructible_v<copyable_function<int32_t()>, MoveOnlyTarget&&>);

        // A signature mismatch never binds
        STATIC_REQUIRE(!std::is_constructible_v<move_only_function<int32_t(string)>, MoveOnlyTarget&&>);

        STATIC_REQUIRE(sizeof(move_only_function<void()>) == FUNCTION_INLINE_TARGET_SIZE + 2 * sizeof(void*));
        STATIC_REQUIRE(sizeof(copyable_function<void()>) == sizeof(move_only_function<void()>));
    }

    SECTION("EmptyState")
    {
        move_only_function<int32_t()> uniq;
        copyable_function<int32_t()> copyable;

        REQUIRE(!uniq);
        REQUIRE(!copyable);
        REQUIRE(!uniq.is_heap_allocated());
        REQUIRE(!copyable.is_heap_allocated());

        uniq = [] { return 7; };
        REQUIRE(uniq);
        REQUIRE(uniq() == 7);

        uniq = nullptr;
        REQUIRE(!uniq);

        move_only_function<int32_t()> from_null_pointer = static_cast<int32_t (*)()>(nullptr);
        REQUIRE(!from_null_pointer);
    }

    SECTION("InlineStorageKeepsCommonTargetsOffTheHeap")
    {
        int32_t counter = 0;
        int32_t other = 0;

        move_only_function<void()> capture_nothing = [] { };
        move_only_function<void()> capture_one = [&counter] { counter++; };
        move_only_function<void()> capture_two = [&counter, &other] { counter += other; };
        move_only_function<int32_t(int32_t)> free_function = &DoubleValue;
        move_only_function<void()> capture_string = [text = string("engine callback")] { ignore_unused(text); };
        move_only_function<int32_t()> capture_owner = [value = SafeAlloc::MakeUnique<int32_t>(42)] { return *value; };
        copyable_function<void()> copyable_capture = [&counter] { counter++; };

        REQUIRE(!capture_nothing.is_heap_allocated());
        REQUIRE(!capture_one.is_heap_allocated());
        REQUIRE(!capture_two.is_heap_allocated());
        REQUIRE(!free_function.is_heap_allocated());
        REQUIRE(!capture_string.is_heap_allocated());
        REQUIRE(!capture_owner.is_heap_allocated());
        REQUIRE(!copyable_capture.is_heap_allocated());

        move_only_function<int32_t()> oversized = OversizedTarget(11);
        move_only_function<int32_t()> throwing_move = ThrowingMoveTarget(13);

        REQUIRE(oversized.is_heap_allocated());
        REQUIRE(throwing_move.is_heap_allocated());
        REQUIRE(oversized() == 11);
        REQUIRE(throwing_move() == 13);
    }

    SECTION("MoveTransfersTargetOwnership")
    {
        int32_t alive = 0;

        {
            move_only_function<int32_t()> source = InstanceCounter(&alive);
            REQUIRE(alive == 1);

            move_only_function<int32_t()> moved = std::move(source);
            REQUIRE(alive == 1);
            REQUIRE(!source); // NOLINT(bugprone-use-after-move)
            REQUIRE(moved() == 1);

            move_only_function<int32_t()> assigned;
            assigned = std::move(moved);
            REQUIRE(alive == 1);
            REQUIRE(!moved); // NOLINT(bugprone-use-after-move)
            REQUIRE(assigned() == 1);
        }

        REQUIRE(alive == 0);
    }

    SECTION("MoveOnlyTargetSurvivesRelocation")
    {
        move_only_function<int32_t()> owner = [value = SafeAlloc::MakeUnique<int32_t>(42)] { return *value; };
        move_only_function<int32_t()> moved = std::move(owner);

        REQUIRE(moved() == 42);

        move_only_function<int32_t()> move_only = MoveOnlyTarget {};
        REQUIRE(move_only() == 17);
    }

    SECTION("CopyDuplicatesTheTarget")
    {
        int32_t alive = 0;

        {
            copyable_function<int32_t()> original = InstanceCounter(&alive);
            REQUIRE(alive == 1);

            copyable_function<int32_t()> copy = original;
            REQUIRE(alive == 2);
            REQUIRE(original);
            REQUIRE(copy() == 2);

            copyable_function<int32_t()> assigned;
            assigned = copy;
            REQUIRE(alive == 3);
        }

        REQUIRE(alive == 0);
    }

    SECTION("CopyOfHeapTargetAllocatesItsOwnBlock")
    {
        copyable_function<int32_t()> original = OversizedTarget(5);
        copyable_function<int32_t()> copy = original;

        REQUIRE(copy.is_heap_allocated());
        REQUIRE(original() == 5);
        REQUIRE(copy() == 5);
    }

    SECTION("CopyableNarrowsToMoveOnlyWithoutWrapping")
    {
        int32_t alive = 0;

        {
            copyable_function<int32_t()> copyable = InstanceCounter(&alive);

            move_only_function<int32_t()> cloned = copyable;
            REQUIRE(alive == 2);
            REQUIRE(!cloned.is_heap_allocated());
            REQUIRE(cloned() == 2);

            move_only_function<int32_t()> adopted = std::move(copyable);
            REQUIRE(alive == 2);
            REQUIRE(!copyable); // NOLINT(bugprone-use-after-move)
            REQUIRE(!adopted.is_heap_allocated());
            REQUIRE(adopted() == 2);
        }

        REQUIRE(alive == 0);
    }

    SECTION("SelfAssignmentKeepsTheTarget")
    {
        copyable_function<int32_t()> copyable = [] { return 3; };
        const copyable_function<int32_t()>& copyable_alias = copyable;
        copyable = copyable_alias;
        REQUIRE(copyable() == 3);

        move_only_function<int32_t()> uniq = [] { return 4; };
        move_only_function<int32_t()>& uniq_alias = uniq;
        uniq = std::move(uniq_alias);
        REQUIRE(uniq() == 4);
    }

    SECTION("InvokeForwardsArgumentsAndReturn")
    {
        move_only_function<int32_t(int32_t, int32_t)> sum = [](int32_t a, int32_t b) { return a + b; };
        REQUIRE(sum(2, 3) == 5);

        move_only_function<void(string&)> mutate = [](string& text) { text += "!"; };
        string text = "ok";
        mutate(text);
        REQUIRE(text == "ok!");

        // A non-void target under a void signature discards its result, matching the standard wrapper
        move_only_function<void(int32_t)> discarding = &DoubleValue;
        discarding(1);

        MemberTarget target;
        target.Bias = 1;
        move_only_function<int32_t(const MemberTarget&, int32_t)> member = &MemberTarget::Triple;
        REQUIRE(member(target, 2) == 7);
    }

    SECTION("CallableThroughConstReference")
    {
        const move_only_function<int32_t()> uniq = [] { return 5; };
        const copyable_function<int32_t()> copyable = [] { return 6; };

        REQUIRE(uniq() == 5);
        REQUIRE(copyable() == 6);
    }

    SECTION("SwapExchangesTargets")
    {
        move_only_function<int32_t()> left = [] { return 1; };
        move_only_function<int32_t()> right = [] { return 2; };

        left.swap(right);

        REQUIRE(left() == 2);
        REQUIRE(right() == 1);
    }

    SECTION("ResetReleasesTheTarget")
    {
        int32_t alive = 0;

        move_only_function<int32_t()> uniq = InstanceCounter(&alive);
        REQUIRE(alive == 1);

        uniq.reset();
        REQUIRE(alive == 0);
        REQUIRE(!uniq);
    }
}

FO_END_NAMESPACE
