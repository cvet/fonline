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
        auto* ptr = static_cast<uint32*>(MemCalloc(3, sizeof(uint32)));
        REQUIRE(ptr != nullptr);
        CHECK(ptr[0] == 0);
        CHECK(ptr[1] == 0);
        CHECK(ptr[2] == 0);

        ptr[0] = 11;
        ptr[1] = 22;
        ptr[2] = 33;

        auto* grown = static_cast<uint32*>(MemRealloc(ptr, sizeof(uint32) * 5));
        REQUIRE(grown != nullptr);
        CHECK(grown[0] == 11);
        CHECK(grown[1] == 22);
        CHECK(grown[2] == 33);

        MemFree(grown);
    }

    SECTION("SafeAllocConstructsObjectsAndZeroInitializedArrays")
    {
        struct TestValue
        {
            explicit TestValue(int32 value_) noexcept :
                Value {value_}
            {
            }

            int32 Value {};
        };

        const auto unique_value = SafeAlloc::MakeUnique<TestValue>(123);
        REQUIRE(unique_value);
        CHECK(unique_value->Value == 123);

        const auto shared_value = SafeAlloc::MakeShared<TestValue>(321);
        REQUIRE(shared_value);
        CHECK(shared_value->Value == 321);

        const auto zero_array = SafeAlloc::MakeUniqueArr<uint32>(4);
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
            TestRefCounted(int32 value_, int32* destroyed_) noexcept :
                Value {value_},
                Destroyed {destroyed_}
            {
            }

            ~TestRefCounted() { ++*Destroyed; }

            int32 Value {};
            int32* Destroyed {};
        };

        int32 destroyed = 0;

        {
            auto ptr = SafeAlloc::MakeRefCounted<TestRefCounted>(42, &destroyed);
            REQUIRE(ptr);
            CHECK(ptr->Value == 42);

            {
                auto copy = ptr;
                REQUIRE(copy);
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
