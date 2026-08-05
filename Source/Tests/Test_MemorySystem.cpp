#include "catch_amalgamated.hpp"

#include "MemorySystem.h"

FO_BEGIN_NAMESPACE

TEST_CASE("MemorySystem")
{
    SECTION("BackupMemoryChunksCanBeReleasedAndReinitialized")
    {
        InitBackupMemoryChunks();

        size_t released = 0;
        while (FreeBackupMemoryChunk()) {
            released++;
        }

        CHECK(released == 100);
        CHECK_FALSE(FreeBackupMemoryChunk());

        InitBackupMemoryChunks();
        CHECK(FreeBackupMemoryChunk());
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

        auto unique_value = SafeAlloc::MakeUnique<TestValue>(123);
        CHECK(unique_value->Value == 123);

        auto shared_value = SafeAlloc::MakeShared<TestValue>(321);
        REQUIRE(shared_value);
        CHECK(shared_value->Value == 321);

        auto zero_array = SafeAlloc::MakeUniqueArr<uint32_t>(4);
        REQUIRE(zero_array);
        CHECK(zero_array[0] == 0);
        CHECK(zero_array[1] == 0);
        CHECK(zero_array[2] == 0);
        CHECK(zero_array[3] == 0);
    }

    SECTION("SafeAllocRawTierAllocatesZeroesAndGrows")
    {
        auto values = SafeAlloc::CallocRaw(3, sizeof(uint32_t)).reinterpret_as<uint32_t>();
        REQUIRE(values);
        CHECK(values[0] == 0);
        CHECK(values[1] == 0);
        CHECK(values[2] == 0);

        values[0] = 11;
        values[1] = 22;
        values[2] = 33;

        auto grown = SafeAlloc::ReallocRaw(values, sizeof(uint32_t) * 5).reinterpret_as<uint32_t>();
        REQUIRE(grown);
        CHECK(grown[0] == 11);
        CHECK(grown[1] == 22);
        CHECK(grown[2] == 33);

        SafeAlloc::FreeRaw(grown);

        auto bytes = SafeAlloc::MallocRaw(64);
        REQUIRE(bytes);
        SafeAlloc::FreeRaw(bytes);
    }

    SECTION("SafeAllocAlignedRawHonoursRequestedAlignment")
    {
        for (size_t alignment : {size_t {8}, size_t {16}, size_t {64}, size_t {256}}) {
            auto block = SafeAlloc::MallocAlignedRaw(1000, alignment);
            REQUIRE(block);
            CHECK(block.as_uintptr() % alignment == 0);
            SafeAlloc::FreeAlignedRaw(block);
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

        constexpr SafeAllocator<OverAlignedValue> over_aligned_allocator;
        ptr<OverAlignedValue> over_aligned = over_aligned_allocator.allocate(8);
        CHECK(over_aligned.as_uintptr() % alignof(OverAlignedValue) == 0);
        over_aligned_allocator.deallocate(over_aligned.get(), 8);

        constexpr SafeAllocator<uint8_t> byte_allocator;
        ptr<uint8_t> bytes = byte_allocator.allocate(24);
        CHECK(bytes.as_uintptr() % alignof(std::max_align_t) == 0);
        byte_allocator.deallocate(bytes.get(), 24);
    }

    SECTION("MakeRefCountedPreservesInitialOwnership")
    {
        struct TestRefCounted final : RefCounted<TestRefCounted>
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
            auto ptr = SafeAlloc::MakeRefCounted<TestRefCounted>(42, &destroyed);
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

        MemMove(buf + 2, buf, 4);
        CHECK(string_view {buf, 8} == "ababcdgh");

        MemFill(buf, 'x', 3);
        CHECK(string_view {buf, 3} == "xxx");

        const char ref[3] = {'x', 'x', 'x'};
        CHECK(MemCompare(buf, ref, 3));
        CHECK(MemCompare(nullptr, nullptr, 0));
    }

    SECTION("ReportBadAllocInvokesCallback")
    {
        bool callback_called = false;
        SetBadAllocCallback([&]() { callback_called = true; });

        ReportBadAlloc("Test bad alloc", "UnitType", 7, 77);

        CHECK(callback_called);
        SetBadAllocCallback({});
    }
}

FO_END_NAMESPACE
