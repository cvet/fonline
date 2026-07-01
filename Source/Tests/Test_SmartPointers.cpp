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

#include "MemorySystem.h"
#include "SmartPointers.h"

FO_BEGIN_NAMESPACE

namespace
{
    class PtrBase
    {
    public:
        virtual ~PtrBase() = default;
        int32_t Value {};
    };

    class PtrDerived final : public PtrBase
    {
    public:
        explicit PtrDerived(int32_t value) { Value = value; }
    };

    class PtrMixin
    {
    public:
        virtual ~PtrMixin() = default;
        int32_t MixinValue {};
    };

    class PtrCrossDerived final : public PtrBase, public PtrMixin
    {
    public:
        PtrCrossDerived(int32_t value, int32_t mixin_value)
        {
            Value = value;
            MixinValue = mixin_value;
        }
    };

    class RefCountedValue final
    {
    public:
        RefCountedValue(int32_t value, ptr<int32_t> destroy_count) noexcept :
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

        int32_t RefCount {};
        int32_t Value {};
        ptr<int32_t> DestroyCount;
    };

    static auto MakeUnreferencedRefCountedValue(int32_t value, ptr<int32_t> destroy_count) -> ptr<RefCountedValue>
    {
        return ptr<RefCountedValue> {new RefCountedValue {value, destroy_count}};
    }

    template<typename T>
    concept explicitly_bool_testable = requires(T value) { static_cast<bool>(value); };

    template<typename T>
    concept has_mutable_get_pp = requires(T value) { value.get_pp(); };

    template<typename T>
    concept has_default_reset = requires(T value) { value.reset(); };

    template<typename T>
    concept has_lvalue_release = requires(T value) { value.release(); };

    template<typename T>
    concept has_lvalue_release_ownership = requires(T value) { value.release_ownership(); };

    template<typename T>
    concept has_refcount_ptr_named_factories = requires(T* raw) {
        { refcount_ptr<T>::from_add_ref(raw) } -> std::same_as<refcount_ptr<T>>;
        { refcount_ptr<T>::try_from_add_ref(raw) } -> std::same_as<refcount_nptr<T>>;
        { refcount_ptr<T>::from_adopted_ref(raw) } -> std::same_as<refcount_ptr<T>>;
    };
}

TEST_CASE("SmartPointers")
{
    SECTION("CompileTimeVocabularyContracts")
    {
        STATIC_REQUIRE(std::is_constructible_v<ptr<PtrBase>, PtrDerived*>);
        STATIC_REQUIRE(std::is_constructible_v<nptr<PtrBase>, PtrDerived*>);
        STATIC_REQUIRE(std::is_convertible_v<ptr<PtrDerived>, nptr<PtrBase>>);
        STATIC_REQUIRE(!std::is_constructible_v<ptr<PtrBase>, nptr<PtrDerived>>);
        STATIC_REQUIRE(!std::is_convertible_v<nptr<PtrDerived>, ptr<PtrBase>>);

        STATIC_REQUIRE(!std::is_default_constructible_v<ptr<PtrBase>>);
        STATIC_REQUIRE(!std::is_constructible_v<ptr<PtrBase>, std::nullptr_t>);
        STATIC_REQUIRE(!std::is_assignable_v<ptr<PtrBase>&, std::nullptr_t>);
        STATIC_REQUIRE(!explicitly_bool_testable<ptr<PtrBase>>);
        STATIC_REQUIRE(!has_mutable_get_pp<ptr<PtrBase>>);
        STATIC_REQUIRE(!has_default_reset<ptr<PtrBase>>);

        STATIC_REQUIRE(std::is_default_constructible_v<nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_constructible_v<nptr<PtrBase>, std::nullptr_t>);
        STATIC_REQUIRE(std::is_assignable_v<nptr<PtrBase>&, std::nullptr_t>);
        STATIC_REQUIRE(explicitly_bool_testable<nptr<PtrBase>>);
        STATIC_REQUIRE(has_mutable_get_pp<nptr<PtrBase>>);
        STATIC_REQUIRE(has_default_reset<nptr<PtrBase>>);

        STATIC_REQUIRE(std::is_constructible_v<unique_nptr<PtrBase>, unique_ptr<PtrDerived>&&>);
        STATIC_REQUIRE(!std::is_constructible_v<unique_ptr<PtrBase>, unique_nptr<PtrDerived>&&>);
        STATIC_REQUIRE(std::is_convertible_v<unique_ptr<PtrDerived>&, ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_convertible_v<unique_nptr<PtrDerived>&, nptr<PtrBase>>);

        STATIC_REQUIRE(!std::is_default_constructible_v<unique_ptr<PtrBase>>);
        STATIC_REQUIRE(!std::is_constructible_v<unique_ptr<PtrBase>, std::nullptr_t>);
        STATIC_REQUIRE(!std::is_assignable_v<unique_ptr<PtrBase>&, std::nullptr_t>);
        STATIC_REQUIRE(!explicitly_bool_testable<unique_ptr<PtrBase>>);
        STATIC_REQUIRE(!has_default_reset<unique_ptr<PtrBase>>);
        STATIC_REQUIRE(!has_lvalue_release<unique_ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<unique_ptr<PtrBase>&&>().release()), ptr<PtrBase>>);

        STATIC_REQUIRE(std::is_default_constructible_v<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_constructible_v<unique_nptr<PtrBase>, std::nullptr_t>);
        STATIC_REQUIRE(std::is_assignable_v<unique_nptr<PtrBase>&, std::nullptr_t>);
        STATIC_REQUIRE(explicitly_bool_testable<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(has_default_reset<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(has_lvalue_release<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<unique_nptr<PtrBase>&>().release()), nptr<PtrBase>>);

        STATIC_REQUIRE(std::is_constructible_v<refcount_nptr<RefCountedValue>, refcount_ptr<RefCountedValue>&&>);
        STATIC_REQUIRE(!std::is_constructible_v<refcount_ptr<RefCountedValue>, refcount_nptr<RefCountedValue>&&>);
        STATIC_REQUIRE(std::is_convertible_v<refcount_ptr<RefCountedValue>&, ptr<RefCountedValue>>);
        STATIC_REQUIRE(std::is_convertible_v<refcount_nptr<RefCountedValue>&, nptr<RefCountedValue>>);

        STATIC_REQUIRE(has_refcount_ptr_named_factories<RefCountedValue>);
        STATIC_REQUIRE(!std::is_default_constructible_v<refcount_ptr<RefCountedValue>>);
        STATIC_REQUIRE(!std::is_constructible_v<refcount_ptr<RefCountedValue>, std::nullptr_t>);
        STATIC_REQUIRE(!std::is_assignable_v<refcount_ptr<RefCountedValue>&, std::nullptr_t>);
        STATIC_REQUIRE(!explicitly_bool_testable<refcount_ptr<RefCountedValue>>);
        STATIC_REQUIRE(!has_default_reset<refcount_ptr<RefCountedValue>>);
        STATIC_REQUIRE(!has_lvalue_release_ownership<refcount_ptr<RefCountedValue>>);

        STATIC_REQUIRE(std::is_default_constructible_v<refcount_nptr<RefCountedValue>>);
        STATIC_REQUIRE(std::is_constructible_v<refcount_nptr<RefCountedValue>, std::nullptr_t>);
        STATIC_REQUIRE(std::is_assignable_v<refcount_nptr<RefCountedValue>&, std::nullptr_t>);
        STATIC_REQUIRE(explicitly_bool_testable<refcount_nptr<RefCountedValue>>);
        STATIC_REQUIRE(has_default_reset<refcount_nptr<RefCountedValue>>);
        STATIC_REQUIRE(has_lvalue_release_ownership<refcount_nptr<RefCountedValue>>);

        STATIC_REQUIRE(!std::is_constructible_v<refcount_ptr<RefCountedValue>, RefCountedValue*>);
        STATIC_REQUIRE(!std::is_assignable_v<refcount_ptr<RefCountedValue>&, RefCountedValue*>);
        STATIC_REQUIRE(!std::is_constructible_v<refcount_nptr<RefCountedValue>, RefCountedValue*>);
        STATIC_REQUIRE(!std::is_assignable_v<refcount_nptr<RefCountedValue>&, RefCountedValue*>);
    }

    SECTION("PtrVocabularySupportsMoveResetAndDynCast")
    {
        PtrDerived value {42};
        ptr<PtrBase> base_ptr {&value};

        REQUIRE(base_ptr.get() != nullptr);
        CHECK(base_ptr->Value == 42);

        ptr<PtrBase> moved_ptr {std::move(base_ptr)};
        CHECK(base_ptr.get() == nullptr);
        REQUIRE(moved_ptr.get() != nullptr);
        CHECK(moved_ptr.get() == &value);

        auto derived_ptr = moved_ptr.dyn_cast<PtrDerived>();
        REQUIRE(derived_ptr);
        CHECK(derived_ptr->Value == 42);
    }

    SECTION("DynCastSupportsCrossCastsThroughMixinBases")
    {
        PtrCrossDerived value {43, 44};
        ptr<PtrBase> base_ptr {&value};
        auto mixin_ptr = base_ptr.dyn_cast<PtrMixin>();

        REQUIRE(mixin_ptr);
        CHECK(mixin_ptr->MixinValue == 44);

        ptr<const PtrBase> const_base_ptr {&value};
        auto const_mixin_ptr = const_base_ptr.dyn_cast<const PtrMixin>();

        REQUIRE(const_mixin_ptr);
        CHECK(const_mixin_ptr->MixinValue == 44);
    }

    SECTION("PtrWidensToNullableBorrowedPointer")
    {
        PtrDerived value {17};
        ptr<PtrBase> compat_ptr {&value};
        ptr<PtrBase> base_ptr = compat_ptr;
        nptr<PtrBase> nullable_ptr = base_ptr;

        REQUIRE(base_ptr.get() != nullptr);
        REQUIRE(nullable_ptr);
        CHECK(base_ptr.get() == &value);
        CHECK(nullable_ptr.get() == &value);

        auto narrowed_ptr = nullable_ptr.as_ptr();
        CHECK(narrowed_ptr.get() == &value);
    }

    SECTION("NptrIsSeparateNullableBorrowedPointer")
    {
        STATIC_REQUIRE(!std::is_same_v<nptr<PtrBase>, ptr<PtrBase>>);
        STATIC_REQUIRE(sizeof(nptr<PtrBase>) == sizeof(PtrBase*));

        nptr<PtrBase> empty_ptr;
        CHECK_FALSE(empty_ptr);

        PtrDerived value {21};
        ptr<PtrDerived> derived_ptr {&value};
        nptr<PtrBase> nullable_ptr = derived_ptr;

        REQUIRE(nullable_ptr);
        CHECK(nullable_ptr.get() == &value);

        auto nullable_derived_ptr = nullable_ptr.dyn_cast<PtrDerived>();
        REQUIRE(nullable_derived_ptr);
        CHECK(nullable_derived_ptr->Value == 21);

        nullable_ptr.reset();
        CHECK_FALSE(nullable_ptr);
    }

    SECTION("BorrowedPointersSupportBufferOffsetArithmetic")
    {
        int32_t buffer[4] = {10, 20, 30, 40};

        ptr<int32_t> base {&buffer[0]};
        auto third = base.offset(2);
        CHECK(third.get() == &buffer[2]);
        CHECK(*third == 30);

        nptr<int32_t> nullable_base {&buffer[0]};
        auto nullable_second = nullable_base.offset(1);
        CHECK(nullable_second.get() == &buffer[1]);
        CHECK(*nullable_second == 20);
    }

    SECTION("UniquePtrReleaseTransfersOwnership")
    {
        unique_ptr<PtrDerived> unique_value = SafeAlloc::MakeUnique<PtrDerived>(77);

        REQUIRE(unique_value.get() != nullptr);
        CHECK(unique_value->Value == 77);

        ptr<PtrDerived> released_ptr = std::move(unique_value).release();
        CHECK(unique_value.get() == nullptr);
        REQUIRE(released_ptr.get() != nullptr);
        CHECK(released_ptr->Value == 77);

        unique_ptr<PtrDerived> owned_released_ptr {released_ptr.get()};
        ignore_unused(owned_released_ptr);
    }

    SECTION("UniqueOwningPointersUseExplicitBridgeMethods")
    {
        unique_ptr<PtrDerived> owned_ptr = SafeAlloc::MakeUnique<PtrDerived>(81);

        auto borrowed = owned_ptr.as_ptr();
        auto maybe_borrowed = owned_ptr.as_nptr();

        REQUIRE(borrowed.get() != nullptr);
        REQUIRE(maybe_borrowed);
        CHECK(borrowed.get() == owned_ptr.get());
        CHECK(maybe_borrowed.get() == owned_ptr.get());
        CHECK(borrowed->Value == 81);

        unique_nptr<PtrDerived> maybe_owned {SafeAlloc::MakeUnique<PtrDerived>(82)};
        auto nullable_borrowed = maybe_owned.as_nptr();

        REQUIRE(nullable_borrowed);
        CHECK(nullable_borrowed.get() == maybe_owned.get());
        CHECK(nullable_borrowed->Value == 82);
    }

    SECTION("UniqueNptrIsSeparateNullableOwningPointer")
    {
        STATIC_REQUIRE(!std::is_same_v<unique_nptr<PtrBase>, unique_ptr<PtrBase>>);
        STATIC_REQUIRE(sizeof(unique_nptr<PtrBase>) == sizeof(PtrBase*));
        STATIC_REQUIRE(!std::is_copy_constructible_v<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_constructible_v<unique_nptr<PtrBase>, unique_ptr<PtrDerived>&&>);

        unique_nptr<PtrBase> empty_ptr;
        CHECK_FALSE(empty_ptr);

        unique_ptr<PtrDerived> owned_ptr = SafeAlloc::MakeUnique<PtrDerived>(88);
        unique_nptr<PtrBase> ptr {std::move(owned_ptr)};

        CHECK(owned_ptr.get() == nullptr);

        REQUIRE(ptr);
        CHECK(ptr->Value == 88);

        auto non_null_ptr = ptr.take_not_null();

        CHECK_FALSE(ptr);
        REQUIRE(non_null_ptr.get() != nullptr);
        CHECK(non_null_ptr->Value == 88);
    }

    SECTION("UniqueDelPtrRunsCustomDeleter")
    {
        int32_t deleted_value = 0;

        {
            unique_del_ptr<int32_t> ptr {SafeAlloc::MakeRaw<int32_t>(15), [&](int32_t* value) {
                                             deleted_value = *value;
                                             delete value;
                                         }};

            REQUIRE(ptr.as_nptr());
            CHECK(*ptr == 15);
        }

        CHECK(deleted_value == 15);
    }

    SECTION("RefcountPtrTracksReferencesAndReleaseOwnership")
    {
        int32_t destroy_count = 0;
        auto raw = MakeUnreferencedRefCountedValue(33, &destroy_count);

        {
            refcount_ptr<RefCountedValue> ptr = refcount_ptr<RefCountedValue>::from_add_ref(raw.get());
            REQUIRE(ptr.get() != nullptr);
            CHECK(raw->RefCount == 1);

            {
                refcount_ptr<RefCountedValue> copy = ptr;
                REQUIRE(copy.get() != nullptr);
                CHECK(raw->RefCount == 2);
                CHECK(copy->Value == 33);
            }

            CHECK(raw->RefCount == 1);

            RefCountedValue* released = std::move(ptr).release_ownership();
            CHECK(ptr.get() == nullptr);
            REQUIRE(released == raw.get());
            CHECK(raw->RefCount == 1);

            raw->Release();
        }

        CHECK(destroy_count == 1);
    }

    SECTION("RefcountNptrIsSeparateNullableOwningPointer")
    {
        STATIC_REQUIRE(!std::is_same_v<refcount_nptr<RefCountedValue>, refcount_ptr<RefCountedValue>>);
        STATIC_REQUIRE(sizeof(refcount_nptr<RefCountedValue>) == sizeof(RefCountedValue*));
        STATIC_REQUIRE(std::is_constructible_v<refcount_nptr<RefCountedValue>, refcount_ptr<RefCountedValue>&&>);

        int32_t destroy_count = 0;
        refcount_nptr<RefCountedValue> nullable_ref;

        CHECK_FALSE(nullable_ref);

        auto raw = MakeUnreferencedRefCountedValue(44, &destroy_count);
        refcount_ptr<RefCountedValue> non_null_ptr = refcount_ptr<RefCountedValue>::from_add_ref(raw.get());
        nullable_ref = std::move(non_null_ptr);

        CHECK(non_null_ptr.get() == nullptr);
        REQUIRE(nullable_ref);
        CHECK(nullable_ref->RefCount == 1);
        CHECK(nullable_ref->Value == 44);

        auto restored_ptr = nullable_ref.take_not_null();

        CHECK_FALSE(nullable_ref);
        REQUIRE(restored_ptr.get() != nullptr);
        CHECK(restored_ptr->RefCount == 1);
        CHECK(restored_ptr->Value == 44);

        {
            refcount_ptr<RefCountedValue> scoped_ptr = std::move(restored_ptr);
            CHECK(restored_ptr.get() == nullptr);
        }

        CHECK(destroy_count == 1);
    }

    SECTION("RefcountPtrAdoptsExistingReferenceThroughNamedFactory")
    {
        int32_t destroy_count = 0;
        auto raw = MakeUnreferencedRefCountedValue(49, &destroy_count);
        raw->AddRef();

        {
            refcount_ptr<RefCountedValue> ptr = refcount_ptr<RefCountedValue>::from_adopted_ref(raw.get());
            REQUIRE(ptr.get() != nullptr);
            CHECK(ptr.get() == raw.get());
            CHECK(raw->RefCount == 1);
        }

        CHECK(destroy_count == 1);
    }

    SECTION("RefcountedBorrowedPointersUseExplicitBridgeMethods")
    {
        int32_t destroy_count = 0;
        auto raw = MakeUnreferencedRefCountedValue(55, &destroy_count);

        {
            ptr<RefCountedValue> borrowed = raw;
            auto held = borrowed.hold_ref();
            REQUIRE(held.get() != nullptr);
            CHECK(held.get() == raw.get());
            CHECK(raw->RefCount == 1);

            auto maybe_held = borrowed.try_hold_ref();
            REQUIRE(maybe_held);
            CHECK(maybe_held.get() == raw.get());
            CHECK(raw->RefCount == 2);

            auto borrowed_again = held.as_ptr();
            auto borrowed_from_nullable_owner = maybe_held.as_ptr();
            auto maybe_borrowed_again = maybe_held.as_nptr();
            CHECK(borrowed_again.get() == raw.get());
            CHECK(borrowed_from_nullable_owner.get() == raw.get());
            CHECK(maybe_borrowed_again.get() == raw.get());
        }

        CHECK(destroy_count == 1);
    }

    SECTION("NullableBorrowedPointerTryHoldRefReturnsEmptyForNull")
    {
        nptr<RefCountedValue> borrowed;
        auto held = borrowed.try_hold_ref();

        CHECK_FALSE(held);
    }

    SECTION("SharedAndWeakAliasesPropagateConstCorrectly")
    {
        shared_ptr<PtrDerived> shared {std::make_shared<PtrDerived>(91)};
        weak_ptr<PtrDerived> weak = shared;

        REQUIRE(shared);
        CHECK(shared.use_count() == 1);
        CHECK(weak.use_count() == 1);
        CHECK(shared.as_ptr()->Value == 91);
        CHECK(shared.as_nptr()->Value == 91);

        const auto& const_shared = shared;
        auto const_borrowed = const_shared.as_ptr();
        CHECK(const_borrowed->Value == 91);

        auto locked = weak.lock();
        REQUIRE(locked);
        CHECK(locked->Value == 91);
        CHECK(shared.use_count() == 2);

        locked.reset();
        shared.reset();

        CHECK_FALSE(shared);
        CHECK_FALSE(shared.as_nptr());
        CHECK(weak.use_count() == 0);
        CHECK_FALSE(weak.lock());
    }
}

FO_END_NAMESPACE
