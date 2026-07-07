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

    SECTION("MemCallocAndReallocPreservePrefix")
    {
        nptr<uint32_t> allocated = MemCalloc(3, sizeof(uint32_t)).cast<uint32_t>();
        REQUIRE(allocated);
        CHECK(allocated[0] == 0);
        CHECK(allocated[1] == 0);
        CHECK(allocated[2] == 0);

        allocated[0] = 11;
        allocated[1] = 22;
        allocated[2] = 33;

        nptr<uint32_t> grown = MemRealloc(allocated, sizeof(uint32_t) * 5).cast<uint32_t>();
        REQUIRE(grown);
        CHECK(grown[0] == 11);
        CHECK(grown[1] == 22);
        CHECK(grown[2] == 33);

        MemFree(grown);
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

        const auto unique_value = SafeAlloc::MakeUnique<TestValue>(123);
        CHECK(unique_value->Value == 123);

        const auto shared_value = SafeAlloc::MakeShared<TestValue>(321);
        REQUIRE(shared_value);
        CHECK(shared_value->Value == 321);

        const auto zero_array = SafeAlloc::MakeUniqueArr<uint32_t>(4);
        REQUIRE(zero_array);
        CHECK(zero_array[0] == 0);
        CHECK(zero_array[1] == 0);
        CHECK(zero_array[2] == 0);
        CHECK(zero_array[3] == 0);
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
