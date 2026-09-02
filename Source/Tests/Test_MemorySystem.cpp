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

#include "MemorySystem.h"

FO_BEGIN_NAMESPACE

TEST_CASE("MemorySystem")
{
    SECTION("BackupMemoryChunksCanBeReleasedAndReinitialized")
    {
        memory::init_backup_chunks();

        size_t released = 0;
        while (memory::free_backup_chunk()) {
            released++;
        }

        CHECK(released == 100);
        CHECK_FALSE(memory::free_backup_chunk());

        memory::init_backup_chunks();
        CHECK(memory::free_backup_chunk());
    }

    SECTION("SafeAllocConstructsObjectsAndZeroInitializedArrays")
    {
        struct TestValue
        {
            explicit TestValue(int32_t value_) noexcept :
                Value {value_}
            {
            }

            int32_t Value {};
        };

        auto unique_value = safe_alloc::make_unique<TestValue>(123);
        CHECK(unique_value->Value == 123);

        auto shared_value = safe_alloc::make_shared<TestValue>(321);
        REQUIRE(shared_value);
        CHECK(shared_value->Value == 321);

        auto zero_array = safe_alloc::make_unique_arr<uint32_t>(4);
        REQUIRE(zero_array);
        CHECK(zero_array[0] == 0);
        CHECK(zero_array[1] == 0);
        CHECK(zero_array[2] == 0);
        CHECK(zero_array[3] == 0);
    }

    SECTION("SafeAllocRawTierAllocatesZeroesAndGrows")
    {
        auto values = safe_alloc::calloc_raw(3, sizeof(uint32_t)).reinterpret_as<uint32_t>();
        REQUIRE(values);
        CHECK(values[0] == 0);
        CHECK(values[1] == 0);
        CHECK(values[2] == 0);

        values[0] = 11;
        values[1] = 22;
        values[2] = 33;

        auto grown = safe_alloc::realloc_raw(values, sizeof(uint32_t) * 5).reinterpret_as<uint32_t>();
        REQUIRE(grown);
        CHECK(grown[0] == 11);
        CHECK(grown[1] == 22);
        CHECK(grown[2] == 33);

        safe_alloc::free_raw(grown);

        auto bytes = safe_alloc::malloc_raw(64);
        REQUIRE(bytes);
        safe_alloc::free_raw(bytes);
    }

    SECTION("SafeAllocAlignedRawHonoursRequestedAlignment")
    {
        for (size_t alignment : {size_t {8}, size_t {16}, size_t {64}, size_t {256}}) {
            auto block = safe_alloc::malloc_aligned_raw(1000, alignment);
            REQUIRE(block);
            CHECK(block.as_uintptr() % alignment == 0);
            safe_alloc::free_aligned_raw(block);
        }
    }

    SECTION("SafeAllocatorHonoursOverAlignedElements")
    {
        FO_MSVC_IGNORE_WARNINGS_PUSH(4324) // Padding from the alignment specifier is the point of this type

        struct alignas(64) OverAlignedValue
        {
            int32_t Value {};
        };

        FO_MSVC_IGNORE_WARNINGS_POP()

        static_assert(alignof(OverAlignedValue) > __STDCPP_DEFAULT_NEW_ALIGNMENT__);

        constexpr safe_allocator<OverAlignedValue> over_aligned_allocator;
        ptr<OverAlignedValue> over_aligned = over_aligned_allocator.allocate(8);
        CHECK(over_aligned.as_uintptr() % alignof(OverAlignedValue) == 0);
        over_aligned_allocator.deallocate(over_aligned.get(), 8);

        constexpr safe_allocator<uint8_t> byte_allocator;
        ptr<uint8_t> bytes = byte_allocator.allocate(24);
        CHECK(bytes.as_uintptr() % alignof(std::max_align_t) == 0);
        byte_allocator.deallocate(bytes.get(), 24);
    }

    SECTION("MakeRefCountedPreservesInitialOwnership")
    {
        struct TestRefCounted final : refcounted<TestRefCounted>
        {
            TestRefCounted(int32_t value_, ptr<int32_t> destroyed) noexcept :
                Value {value_},
                Destroyed {destroyed}
            {
            }

            ~TestRefCounted() { ++*Destroyed; }

            int32_t Value {};
            ptr<int32_t> Destroyed;
        };

        int32_t destroyed = 0;

        {
            auto ptr = safe_alloc::make_refcounted<TestRefCounted>(42, &destroyed);
            CHECK(ptr->Value == 42);

            {
                auto copy = ptr;
                CHECK(copy->Value == 42);
            }

            CHECK(destroyed == 0);
        }

        CHECK(destroyed == 1);
    }

    SECTION("MemoryOpsHandleOverlapAndZeroSizedCompare")
    {
        char buf[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

        memory::move(buf + 2, buf, 4);
        CHECK(string_view {buf, 8} == "ababcdgh");

        memory::fill(buf, 'x', 3);
        CHECK(string_view {buf, 3} == "xxx");

        const char ref[3] = {'x', 'x', 'x'};
        CHECK(memory::compare(buf, ref, 3));
        CHECK(memory::compare(nullptr, nullptr, 0));
    }

    SECTION("ReportBadAllocInvokesCallback")
    {
        bool callback_called = false;
        memory::set_bad_alloc_callback([&]() { callback_called = true; });

        memory::report_bad_alloc("Test bad alloc", "UnitType", 7, 77);

        CHECK(callback_called);
        memory::set_bad_alloc_callback({});
    }
}

FO_END_NAMESPACE
