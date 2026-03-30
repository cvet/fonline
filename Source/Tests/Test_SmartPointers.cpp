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

#include "catch_amalgamated.hpp"

#include "SmartPointers.h"

FO_BEGIN_NAMESPACE

namespace
{
    class PtrBase
    {
    public:
        virtual ~PtrBase() = default;
        int32 Value {};
    };

    class PtrDerived final : public PtrBase
    {
    public:
        explicit PtrDerived(int32 value) { Value = value; }
    };

    class RefCountedValue final
    {
    public:
        RefCountedValue(int32 value, int32* destroy_count) noexcept :
            Value {value},
            DestroyCount {destroy_count}
        {
        }

        void AddRef() noexcept { ++RefCount; }

        void Release() noexcept
        {
            --RefCount;

            if (RefCount == 0) {
                ++*DestroyCount;
                delete this;
            }
        }

        int32 RefCount {};
        int32 Value {};
        int32* DestroyCount {};
    };
}

TEST_CASE("SmartPointers")
{
    SECTION("RawPtrSupportsMoveResetAndDynCast")
    {
        PtrDerived value {42};
        raw_ptr<PtrBase> base_ptr {&value};

        REQUIRE(base_ptr);
        CHECK(base_ptr->Value == 42);

        raw_ptr<PtrBase> moved_ptr {std::move(base_ptr)};
        CHECK_FALSE(base_ptr);
        REQUIRE(moved_ptr);
        CHECK(moved_ptr.get() == &value);

        raw_ptr<PtrDerived> derived_ptr = moved_ptr.dyn_cast<PtrDerived>();
        REQUIRE(derived_ptr);
        CHECK(derived_ptr->Value == 42);

        moved_ptr.reset();
        CHECK_FALSE(moved_ptr);
    }

    SECTION("UniquePtrReleaseTransfersOwnership")
    {
        unique_ptr<PtrDerived> ptr {new PtrDerived {77}};

        REQUIRE(ptr);
        CHECK(ptr->Value == 77);

        PtrDerived* raw = ptr.release();
        CHECK_FALSE(ptr);
        REQUIRE(raw != nullptr);
        CHECK(raw->Value == 77);

        delete raw;
    }

    SECTION("UniqueDelPtrRunsCustomDeleter")
    {
        int32 deleted_value = 0;

        {
            unique_del_ptr<int32> ptr {new int32 {15}, [&](int32* value) {
                                           deleted_value = *value;
                                           delete value;
                                       }};

            REQUIRE(ptr);
            CHECK(*ptr == 15);
        }

        CHECK(deleted_value == 15);
    }

    SECTION("RefcountPtrTracksReferencesAndReleaseOwnership")
    {
        int32 destroy_count = 0;
        auto* raw = new RefCountedValue {33, &destroy_count};

        {
            refcount_ptr<RefCountedValue> ptr {raw};
            REQUIRE(ptr);
            CHECK(raw->RefCount == 1);

            {
                refcount_ptr<RefCountedValue> copy = ptr;
                REQUIRE(copy);
                CHECK(raw->RefCount == 2);
                CHECK(copy->Value == 33);
            }

            CHECK(raw->RefCount == 1);

            RefCountedValue* released = ptr.release_ownership();
            CHECK_FALSE(ptr);
            REQUIRE(released == raw);
            CHECK(raw->RefCount == 1);

            raw->Release();
        }

        CHECK(destroy_count == 1);
    }

    SECTION("SharedAndWeakAliasesPropagateConstCorrectly")
    {
        shared_ptr<PtrDerived> shared {std::make_shared<PtrDerived>(91)};
        weak_ptr<PtrDerived> weak = shared;

        REQUIRE(shared);
        CHECK(shared.use_count() == 1);
        CHECK(weak.use_count() == 1);

        auto locked = weak.lock();
        REQUIRE(locked);
        CHECK(locked->Value == 91);
        CHECK(shared.use_count() == 2);

        locked.reset();
        shared.reset();

        CHECK_FALSE(shared);
        CHECK(weak.use_count() == 0);
        CHECK_FALSE(weak.lock());
    }
}

FO_END_NAMESPACE
