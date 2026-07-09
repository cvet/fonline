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

#include "CommonHelpers.h"
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

    class RefCountedPolyBase
    {
    public:
        virtual ~RefCountedPolyBase() = default;

        void AddRef() const noexcept { ++RefCount; }

        void Release() const noexcept
        {
            --RefCount;

            if (RefCount == 0) {
                delete this;
            }
        }

        mutable int32_t RefCount {};
    };

    class RefCountedPolyDerived final : public RefCountedPolyBase, public PtrMixin
    {
    public:
        RefCountedPolyDerived(int32_t value, int32_t mixin_value) noexcept
        {
            Value = value;
            MixinValue = mixin_value;
        }

        int32_t Value {};
    };

    static auto MakeUnreferencedRefCountedValue(int32_t value, ptr<int32_t> destroy_count) -> ptr<RefCountedValue>
    {
        return ptr<RefCountedValue> {new RefCountedValue {value, destroy_count}};
    }

    static auto MakeUnreferencedRefCountedPolyValue(int32_t value, int32_t mixin_value) -> ptr<RefCountedPolyDerived>
    {
        return ptr<RefCountedPolyDerived> {new RefCountedPolyDerived {value, mixin_value}};
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
    concept has_void_cast = requires(T value) {
        { value.void_cast() } -> std::same_as<void*>;
    };

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
        STATIC_REQUIRE(std::is_constructible_v<ptr<PtrBase>, nptr<PtrDerived>>);
        STATIC_REQUIRE(std::is_convertible_v<nptr<PtrDerived>, ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_constructible_v<ptr<const PtrBase>, const nptr<PtrDerived>&>);
        STATIC_REQUIRE(!std::is_constructible_v<ptr<PtrBase>, const nptr<PtrDerived>&>);

        STATIC_REQUIRE(!std::is_default_constructible_v<ptr<PtrBase>>);
        STATIC_REQUIRE(!std::is_constructible_v<ptr<PtrBase>, std::nullptr_t>);
        STATIC_REQUIRE(!std::is_assignable_v<ptr<PtrBase>&, std::nullptr_t>);
        STATIC_REQUIRE(!explicitly_bool_testable<ptr<PtrBase>>);
        STATIC_REQUIRE(!has_mutable_get_pp<ptr<PtrBase>>);
        STATIC_REQUIRE(!has_default_reset<ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<ptr<PtrBase>>().void_cast()), void*>);

        STATIC_REQUIRE(std::is_default_constructible_v<nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_constructible_v<nptr<PtrBase>, std::nullptr_t>);
        STATIC_REQUIRE(std::is_assignable_v<nptr<PtrBase>&, std::nullptr_t>);
        STATIC_REQUIRE(explicitly_bool_testable<nptr<PtrBase>>);
        STATIC_REQUIRE(has_mutable_get_pp<nptr<PtrBase>>);
        STATIC_REQUIRE(has_default_reset<nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<nptr<PtrBase>>().void_cast()), void*>);
        STATIC_REQUIRE(std::is_same_v<decltype(cast_from_void<PtrBase*>(std::declval<void*>())), nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(cast_from_void<const PtrBase*>(std::declval<const void*>())), nptr<const PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(cast_from_void<PtrBase*>(std::declval<ptr<void>>())), nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(cast_from_void<PtrBase*>(std::declval<nptr<void>>())), nptr<PtrBase>>);

        STATIC_REQUIRE(std::is_constructible_v<unique_nptr<PtrBase>, unique_ptr<PtrDerived>&&>);
        STATIC_REQUIRE(!std::is_constructible_v<unique_ptr<PtrBase>, unique_nptr<PtrDerived>&&>);
        STATIC_REQUIRE(std::is_convertible_v<unique_ptr<PtrDerived>&, ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_convertible_v<unique_nptr<PtrDerived>&, ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_convertible_v<unique_nptr<PtrDerived>&, nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<unique_ptr<PtrBase>&>().dyn_cast<PtrDerived>()), nptr<PtrDerived>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<unique_nptr<PtrBase>&>().dyn_cast<PtrDerived>()), nptr<PtrDerived>>);
        STATIC_REQUIRE(!std::is_constructible_v<unique_ptr<PtrBase>, ptr<PtrDerived>>);
        STATIC_REQUIRE(!std::is_constructible_v<unique_nptr<PtrBase>, ptr<PtrDerived>>);

        STATIC_REQUIRE(!std::is_default_constructible_v<unique_ptr<PtrBase>>);
        STATIC_REQUIRE(!std::is_constructible_v<unique_ptr<PtrBase>, std::nullptr_t>);
        STATIC_REQUIRE(!std::is_assignable_v<unique_ptr<PtrBase>&, std::nullptr_t>);
        STATIC_REQUIRE(!explicitly_bool_testable<unique_ptr<PtrBase>>);
        STATIC_REQUIRE(!has_default_reset<unique_ptr<PtrBase>>);
        STATIC_REQUIRE(has_lvalue_release<unique_ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<unique_ptr<PtrBase>&>().release()), ptr<PtrBase>>);
        STATIC_REQUIRE(has_void_cast<unique_ptr<PtrBase>>);

        STATIC_REQUIRE(std::is_default_constructible_v<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_constructible_v<unique_nptr<PtrBase>, std::nullptr_t>);
        STATIC_REQUIRE(std::is_assignable_v<unique_nptr<PtrBase>&, std::nullptr_t>);
        STATIC_REQUIRE(explicitly_bool_testable<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(has_default_reset<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(has_lvalue_release<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<unique_nptr<PtrBase>&>().release()), nptr<PtrBase>>);
        STATIC_REQUIRE(has_void_cast<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(has_void_cast<unique_arr_ptr<PtrBase>>);
        STATIC_REQUIRE(has_lvalue_release<unique_del_ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<unique_del_ptr<PtrBase>&>().release()), ptr<PtrBase>>);
        STATIC_REQUIRE(has_void_cast<unique_del_ptr<PtrBase>>);
        STATIC_REQUIRE(has_void_cast<unique_del_nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_default_constructible_v<unique_del_nptr<void>>);
        STATIC_REQUIRE(!std::is_default_constructible_v<unique_del_ptr<void>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<unique_del_ptr<void>&>().release()), ptr<void>>);
        STATIC_REQUIRE(has_void_cast<unique_del_ptr<void>>);
        STATIC_REQUIRE(has_void_cast<unique_del_nptr<void>>);

        STATIC_REQUIRE(std::is_constructible_v<refcount_nptr<RefCountedValue>, refcount_ptr<RefCountedValue>&&>);
        STATIC_REQUIRE(!std::is_constructible_v<refcount_ptr<RefCountedValue>, refcount_nptr<RefCountedValue>&&>);
        STATIC_REQUIRE(std::is_convertible_v<refcount_ptr<RefCountedValue>&, ptr<RefCountedValue>>);
        STATIC_REQUIRE(std::is_convertible_v<refcount_nptr<RefCountedValue>&, ptr<RefCountedValue>>);
        STATIC_REQUIRE(std::is_convertible_v<refcount_nptr<RefCountedValue>&, nptr<RefCountedValue>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<refcount_ptr<RefCountedPolyBase>&>().dyn_cast<RefCountedPolyDerived>()), refcount_nptr<RefCountedPolyDerived>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<refcount_nptr<RefCountedPolyBase>&>().dyn_cast<RefCountedPolyDerived>()), refcount_nptr<RefCountedPolyDerived>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<refcount_ptr<RefCountedPolyBase>&>().dyn_cast<PtrMixin>()), nptr<PtrMixin>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<refcount_nptr<RefCountedPolyBase>&>().dyn_cast<PtrMixin>()), nptr<PtrMixin>>);
        STATIC_REQUIRE(std::is_same_v<decltype(std::declval<const refcount_ptr<RefCountedPolyBase>&>().dyn_cast<PtrMixin>()), nptr<const PtrMixin>>);
        STATIC_REQUIRE(!std::is_constructible_v<refcount_ptr<RefCountedValue>, ptr<RefCountedValue>>);
        STATIC_REQUIRE(!std::is_constructible_v<refcount_nptr<RefCountedValue>, ptr<RefCountedValue>>);

        STATIC_REQUIRE(has_refcount_ptr_named_factories<RefCountedValue>);
        STATIC_REQUIRE(!std::is_default_constructible_v<refcount_ptr<RefCountedValue>>);
        STATIC_REQUIRE(!std::is_constructible_v<refcount_ptr<RefCountedValue>, std::nullptr_t>);
        STATIC_REQUIRE(!std::is_assignable_v<refcount_ptr<RefCountedValue>&, std::nullptr_t>);
        STATIC_REQUIRE(!explicitly_bool_testable<refcount_ptr<RefCountedValue>>);
        STATIC_REQUIRE(!has_default_reset<refcount_ptr<RefCountedValue>>);
        STATIC_REQUIRE(has_lvalue_release_ownership<refcount_ptr<RefCountedValue>>);
        STATIC_REQUIRE(has_void_cast<refcount_ptr<RefCountedValue>>);

        STATIC_REQUIRE(std::is_default_constructible_v<refcount_nptr<RefCountedValue>>);
        STATIC_REQUIRE(std::is_constructible_v<refcount_nptr<RefCountedValue>, std::nullptr_t>);
        STATIC_REQUIRE(std::is_assignable_v<refcount_nptr<RefCountedValue>&, std::nullptr_t>);
        STATIC_REQUIRE(explicitly_bool_testable<refcount_nptr<RefCountedValue>>);
        STATIC_REQUIRE(has_default_reset<refcount_nptr<RefCountedValue>>);
        STATIC_REQUIRE(has_lvalue_release_ownership<refcount_nptr<RefCountedValue>>);
        STATIC_REQUIRE(has_void_cast<refcount_nptr<RefCountedValue>>);

        STATIC_REQUIRE(!std::is_constructible_v<refcount_ptr<RefCountedValue>, RefCountedValue*>);
        STATIC_REQUIRE(!std::is_assignable_v<refcount_ptr<RefCountedValue>&, RefCountedValue*>);
        STATIC_REQUIRE(!std::is_constructible_v<refcount_nptr<RefCountedValue>, RefCountedValue*>);
        STATIC_REQUIRE(!std::is_assignable_v<refcount_nptr<RefCountedValue>&, RefCountedValue*>);

        STATIC_REQUIRE(std::is_convertible_v<shared_ptr<PtrDerived>&, ptr<PtrBase>>);
        STATIC_REQUIRE(std::is_convertible_v<shared_ptr<PtrDerived>&, nptr<PtrBase>>);
        STATIC_REQUIRE(!std::is_constructible_v<shared_ptr<PtrBase>, ptr<PtrDerived>>);
        STATIC_REQUIRE(has_void_cast<shared_ptr<PtrBase>>);
        STATIC_REQUIRE(has_void_cast<weak_ptr<PtrBase>>);
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
        nptr<PtrBase> maybe_base = base_ptr;

        REQUIRE(base_ptr.get() != nullptr);
        REQUIRE(maybe_base);
        CHECK(base_ptr.get() == &value);
        CHECK(maybe_base.get() == &value);

        ptr<PtrBase> narrowed_ptr = maybe_base;
        CHECK(narrowed_ptr.get() == &value);
    }

    SECTION("RawPointersUseMakeBorrowHelpers")
    {
        STATIC_REQUIRE(std::same_as<decltype(make_ptr(std::declval<PtrDerived*>())), ptr<PtrDerived>>);
        STATIC_REQUIRE(std::same_as<decltype(make_ptr(std::declval<const PtrDerived*>())), ptr<const PtrDerived>>);
        STATIC_REQUIRE(std::same_as<decltype(make_nptr(std::declval<PtrDerived*>())), nptr<PtrDerived>>);
        STATIC_REQUIRE(std::same_as<decltype(make_nptr(std::declval<const PtrDerived*>())), nptr<const PtrDerived>>);

        PtrDerived value {19};
        PtrDerived* raw_value = &value;

        auto borrowed = make_ptr(raw_value);
        auto maybe_borrowed = make_nptr(raw_value);

        CHECK(borrowed.get() == &value);
        REQUIRE(maybe_borrowed);
        CHECK(maybe_borrowed.get() == &value);

        PtrDerived* raw_empty = nullptr;
        auto empty = make_nptr(raw_empty);
        CHECK_FALSE(empty);
    }

    SECTION("NptrIsSeparateNullableBorrowedPointer")
    {
        STATIC_REQUIRE(!std::is_same_v<nptr<PtrBase>, ptr<PtrBase>>);
        STATIC_REQUIRE(sizeof(nptr<PtrBase>) == sizeof(PtrBase*));

        nptr<PtrBase> empty_ptr;
        CHECK_FALSE(empty_ptr);

        PtrDerived value {21};
        ptr<PtrDerived> derived_ptr {&value};
        nptr<PtrBase> maybe_base = derived_ptr;

        REQUIRE(maybe_base);
        CHECK(maybe_base.get() == &value);

        auto maybe_derived = maybe_base.dyn_cast<PtrDerived>();
        REQUIRE(maybe_derived);
        CHECK(maybe_derived->Value == 21);

        maybe_base.reset();
        CHECK_FALSE(maybe_base);
    }

    SECTION("BorrowedPointersSupportBufferOffsetArithmetic")
    {
        int32_t buffer[4] = {10, 20, 30, 40};

        ptr<int32_t> base {&buffer[0]};
        auto third = base.offset(2);
        CHECK(third.get() == &buffer[2]);
        CHECK(*third == 30);

        nptr<int32_t> maybe_base {&buffer[0]};
        auto maybe_second = maybe_base.offset(1);
        CHECK(maybe_second.get() == &buffer[1]);
        CHECK(*maybe_second == 20);
    }

    SECTION("PointerVocabularySupportsOpaqueVoidCast")
    {
        PtrDerived value {63};
        PtrBase* base_value = &value;
        ptr<PtrBase> borrowed {base_value};
        nptr<PtrBase> maybe_borrowed {base_value};
        nptr<PtrBase> empty_borrowed;

        CHECK(borrowed.void_cast() == static_cast<void*>(base_value));
        CHECK(maybe_borrowed.void_cast() == static_cast<void*>(base_value));
        CHECK(empty_borrowed.void_cast() == nullptr);

        auto unique_owner = SafeAlloc::MakeUnique<PtrDerived>(64);
        ptr<PtrDerived> borrowed_unique_owner = unique_owner;
        CHECK(unique_owner.void_cast() == borrowed_unique_owner.void_cast());

        auto array_owner = SafeAlloc::MakeUniqueArr<int32_t>(2);
        unique_arr_ptr<int32_t> empty_array_owner;
        CHECK(array_owner.void_cast() != nullptr);
        CHECK(empty_array_owner.void_cast() == nullptr);

        int32_t destroy_count = 0;
        auto raw_ref = MakeUnreferencedRefCountedValue(66, &destroy_count);

        {
            refcount_ptr<RefCountedValue> ref_owner = refcount_ptr<RefCountedValue>::from_add_ref(raw_ref.get());
            refcount_nptr<RefCountedValue> maybe_ref_owner = ref_owner;
            refcount_nptr<RefCountedValue> empty_ref_owner;

            CHECK(ref_owner.void_cast() == raw_ref.void_cast());
            CHECK(maybe_ref_owner.void_cast() == raw_ref.void_cast());
            CHECK(empty_ref_owner.void_cast() == nullptr);
        }

        CHECK(destroy_count == 1);

        auto shared_owner = SafeAlloc::MakeShared<PtrDerived>(67);
        shared_ptr<PtrDerived> empty_shared_owner;
        weak_ptr<PtrDerived> weak_owner = shared_owner;
        weak_ptr<PtrDerived> empty_weak_owner;

        ptr<PtrDerived> borrowed_shared_owner = shared_owner;
        CHECK(shared_owner.void_cast() == borrowed_shared_owner.void_cast());
        CHECK(empty_shared_owner.void_cast() == nullptr);
        CHECK(weak_owner.void_cast() == shared_owner.void_cast());
        CHECK(empty_weak_owner.void_cast() == nullptr);

        auto custom_owner = make_unique_del_ptr(SafeAlloc::MakeRaw<int32_t>(68), [](int32_t* raw_value) noexcept { delete raw_value; });
        ptr<int32_t> borrowed_custom_owner = custom_owner;
        CHECK(custom_owner.void_cast() == borrowed_custom_owner.void_cast());

        auto maybe_custom_value = SafeAlloc::MakeRaw<int32_t>(69);
        auto maybe_custom_owner = make_unique_del_ptr(maybe_custom_value, [](int32_t* raw_value) noexcept { delete raw_value; });
        nptr<int32_t> borrowed_maybe_custom_owner = maybe_custom_owner;
        CHECK(maybe_custom_owner.void_cast() == borrowed_maybe_custom_owner.void_cast());

        auto empty_custom_owner = make_unique_del_ptr(nptr<int32_t> {}, [](int32_t* raw_value) noexcept { ignore_unused(raw_value); });
        CHECK(empty_custom_owner.void_cast() == nullptr);

        int32_t deleted_opaque_value = 0;
        {
            auto raw_opaque_value = SafeAlloc::MakeRaw<int32_t>(70);
            auto opaque_owner = make_unique_del_ptr(raw_opaque_value.reinterpret_as<void>(), [&](void* raw_value) noexcept {
                auto value = cast_from_void<int32_t*>(raw_value);
                auto owned_value = adopt_unique_ptr(value);
                deleted_opaque_value = *owned_value;
            });
            CHECK(opaque_owner.get() == raw_opaque_value.void_cast());
        }
        CHECK(deleted_opaque_value == 70);
    }

    SECTION("UniquePtrReleaseTransfersOwnership")
    {
        auto unique_value = SafeAlloc::MakeUnique<PtrDerived>(77);

        REQUIRE(unique_value.get() != nullptr);
        CHECK(unique_value->Value == 77);

        auto released_ptr = unique_value.release();
        CHECK(unique_value.get() == nullptr);
        REQUIRE(released_ptr.get() != nullptr);
        CHECK(released_ptr->Value == 77);

        unique_ptr<PtrDerived> owned_released_ptr {released_ptr.get()};
        ignore_unused(owned_released_ptr);
    }

    SECTION("UniqueOwningPointersBorrowImplicitly")
    {
        auto owned_ptr = SafeAlloc::MakeUnique<PtrDerived>(81);

        ptr<PtrBase> borrowed = owned_ptr;
        nptr<PtrBase> maybe_borrowed = owned_ptr;

        REQUIRE(borrowed.get() != nullptr);
        REQUIRE(maybe_borrowed);
        CHECK(borrowed.get() == owned_ptr.get());
        CHECK(maybe_borrowed.get() == owned_ptr.get());
        CHECK(borrowed->Value == 81);

        unique_nptr<PtrDerived> maybe_owned {SafeAlloc::MakeUnique<PtrDerived>(82)};
        REQUIRE(maybe_owned);
        ptr<PtrBase> borrowed_from_maybe_owner = maybe_owned;
        nptr<PtrBase> maybe_borrowed_from_owner = maybe_owned;

        REQUIRE(maybe_borrowed_from_owner);
        CHECK(borrowed_from_maybe_owner.get() == maybe_owned.get());
        CHECK(maybe_borrowed_from_owner.get() == maybe_owned.get());
        CHECK(maybe_owned.void_cast() == maybe_borrowed_from_owner.void_cast());
        CHECK(borrowed_from_maybe_owner->Value == 82);
        CHECK(maybe_borrowed_from_owner->Value == 82);
    }

    SECTION("UniqueOwningPointersDynCastToBorrowDirectly")
    {
        unique_ptr<PtrBase> owned_ptr = SafeAlloc::MakeUnique<PtrCrossDerived>(83, 84);

        auto mixin = owned_ptr.dyn_cast<PtrMixin>();
        STATIC_REQUIRE(std::is_same_v<decltype(mixin), nptr<PtrMixin>>);

        REQUIRE(mixin);
        CHECK(mixin->MixinValue == 84);
    }

    SECTION("AsPtrAsNptrKeepAutoBorrowLocals")
    {
        int32_t value = 12;
        ptr<int32_t> borrowed {&value};
        const auto& const_borrowed = borrowed;

        auto deduced_ptr = borrowed.as_ptr();
        auto deduced_const_ptr = const_borrowed.as_ptr();
        auto deduced_nptr = borrowed.as_nptr();
        auto deduced_const_nptr = const_borrowed.as_nptr();

        STATIC_REQUIRE(std::is_same_v<decltype(deduced_ptr), ptr<int32_t>>);
        STATIC_REQUIRE(std::is_same_v<decltype(deduced_const_ptr), ptr<const int32_t>>);
        STATIC_REQUIRE(std::is_same_v<decltype(deduced_nptr), nptr<int32_t>>);
        STATIC_REQUIRE(std::is_same_v<decltype(deduced_const_nptr), nptr<const int32_t>>);
        CHECK(deduced_ptr.get() == &value);
        CHECK(deduced_const_ptr.get() == &value);
        CHECK(deduced_nptr.get() == &value);
        CHECK(deduced_const_nptr.get() == &value);

        auto owned_ptr = SafeAlloc::MakeUnique<PtrDerived>(83);
        const auto& const_owned_ptr = owned_ptr;
        auto owner_ptr = owned_ptr.as_ptr();
        auto owner_const_ptr = const_owned_ptr.as_ptr();
        auto owner_nptr = owned_ptr.as_nptr();
        auto owner_const_nptr = const_owned_ptr.as_nptr();

        STATIC_REQUIRE(std::is_same_v<decltype(owner_ptr), ptr<PtrDerived>>);
        STATIC_REQUIRE(std::is_same_v<decltype(owner_const_ptr), ptr<const PtrDerived>>);
        STATIC_REQUIRE(std::is_same_v<decltype(owner_nptr), nptr<PtrDerived>>);
        STATIC_REQUIRE(std::is_same_v<decltype(owner_const_nptr), nptr<const PtrDerived>>);
        CHECK(owner_ptr.get() == owned_ptr.get());
        CHECK(owner_const_ptr.get() == owned_ptr.get());
        CHECK(owner_nptr.get() == owned_ptr.get());
        CHECK(owner_const_nptr.get() == owned_ptr.get());
    }

    SECTION("UniqueNptrIsSeparateNullableOwningPointer")
    {
        STATIC_REQUIRE(!std::is_same_v<unique_nptr<PtrBase>, unique_ptr<PtrBase>>);
        STATIC_REQUIRE(sizeof(unique_nptr<PtrBase>) == sizeof(PtrBase*));
        STATIC_REQUIRE(!std::is_copy_constructible_v<unique_nptr<PtrBase>>);
        STATIC_REQUIRE(std::is_constructible_v<unique_nptr<PtrBase>, unique_ptr<PtrDerived>&&>);

        unique_nptr<PtrBase> empty_ptr;
        CHECK_FALSE(empty_ptr);
        CHECK(empty_ptr.void_cast() == nullptr);

        auto owned_ptr = SafeAlloc::MakeUnique<PtrDerived>(88);
        unique_nptr<PtrBase> ptr {std::move(owned_ptr)};

        CHECK(owned_ptr.get() == nullptr);

        REQUIRE(ptr);
        nptr<PtrBase> maybe_borrowed_ptr = ptr;
        CHECK(ptr.void_cast() == maybe_borrowed_ptr.void_cast());
        CHECK(ptr->Value == 88);

        auto derived = ptr.dyn_cast<PtrDerived>();
        STATIC_REQUIRE(std::is_same_v<decltype(derived), nptr<PtrDerived>>);

        REQUIRE(derived);
        CHECK(derived->Value == 88);

        auto non_null_ptr = ptr.take_not_null();

        CHECK_FALSE(ptr);
        REQUIRE(non_null_ptr.get() != nullptr);
        CHECK(non_null_ptr->Value == 88);
    }

    SECTION("UniqueDelPtrRunsCustomDeleter")
    {
        int32_t deleted_value = 0;

        {
            auto ptr = make_unique_del_ptr(SafeAlloc::MakeRaw<int32_t>(15), [&](int32_t* value) {
                deleted_value = *value;
                delete value;
            });

            nptr<int32_t> maybe_ptr = ptr;
            REQUIRE(maybe_ptr);
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

            RefCountedValue* released = ptr.release_ownership();
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
        refcount_nptr<RefCountedValue> maybe_ref;

        CHECK_FALSE(maybe_ref);

        auto raw = MakeUnreferencedRefCountedValue(44, &destroy_count);
        refcount_ptr<RefCountedValue> non_null_ptr = refcount_ptr<RefCountedValue>::from_add_ref(raw.get());
        maybe_ref = std::move(non_null_ptr);

        CHECK(non_null_ptr.get() == nullptr);
        REQUIRE(maybe_ref);
        CHECK(maybe_ref->RefCount == 1);
        CHECK(maybe_ref->Value == 44);

        auto restored_ptr = maybe_ref.take_not_null();

        CHECK_FALSE(maybe_ref);
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

    SECTION("RefcountedOwningPointersBorrowImplicitly")
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

            ptr<RefCountedValue> borrowed_again = held;
            ptr<RefCountedValue> borrowed_from_maybe_owner = maybe_held;
            nptr<RefCountedValue> maybe_borrowed_again = maybe_held;
            CHECK(borrowed_again.get() == raw.get());
            CHECK(borrowed_from_maybe_owner.get() == raw.get());
            CHECK(maybe_borrowed_again.get() == raw.get());
        }

        CHECK(destroy_count == 1);
    }

    SECTION("RefcountOwningPointersDynCastBorrowForNonRefcountableTargets")
    {
        auto raw = MakeUnreferencedRefCountedPolyValue(61, 62);

        {
            refcount_ptr<RefCountedPolyBase> owner = refcount_ptr<RefCountedPolyBase>::from_add_ref(raw.get());
            CHECK(raw->RefCount == 1);

            {
                auto derived_owner = owner.dyn_cast<RefCountedPolyDerived>();
                STATIC_REQUIRE(std::is_same_v<decltype(derived_owner), refcount_nptr<RefCountedPolyDerived>>);

                REQUIRE(derived_owner);
                CHECK(raw->RefCount == 2);
                CHECK(derived_owner->Value == 61);
            }

            CHECK(raw->RefCount == 1);

            auto mixin = owner.dyn_cast<PtrMixin>();
            STATIC_REQUIRE(std::is_same_v<decltype(mixin), nptr<PtrMixin>>);

            REQUIRE(mixin);
            CHECK(raw->RefCount == 1);
            CHECK(mixin->MixinValue == 62);
        }
    }

    SECTION("NullableBorrowedPointerTryHoldRefReturnsEmptyForNull")
    {
        nptr<RefCountedValue> borrowed;
        auto held = borrowed.try_hold_ref();

        CHECK_FALSE(held);
    }

    SECTION("SharedAndWeakAliasesPropagateConstCorrectly")
    {
        auto shared = SafeAlloc::MakeShared<PtrDerived>(91);
        weak_ptr<PtrDerived> weak = shared;

        REQUIRE(shared);
        CHECK(shared.use_count() == 1);
        CHECK(weak.use_count() == 1);
        ptr<PtrBase> shared_borrowed = shared;
        nptr<PtrBase> maybe_shared_borrowed = shared;
        CHECK(shared_borrowed->Value == 91);
        CHECK(maybe_shared_borrowed->Value == 91);

        const auto& const_shared = shared;
        ptr<const PtrBase> const_borrowed = const_shared;
        CHECK(const_borrowed->Value == 91);

        auto locked = weak.lock();
        REQUIRE(locked);
        CHECK(locked->Value == 91);
        CHECK(shared.use_count() == 2);

        locked.reset();
        shared.reset();

        CHECK_FALSE(shared);
        nptr<PtrDerived> empty_borrowed = shared;
        CHECK_FALSE(empty_borrowed);
        CHECK(weak.use_count() == 0);
        CHECK_FALSE(weak.lock());
    }

    SECTION("SharedAndWeakOwnershipIsThreadSafe")
    {
        constexpr size_t THREADS_COUNT = 8;
        constexpr size_t ITERATIONS_COUNT = 20000;

        auto shared = SafeAlloc::MakeShared<PtrDerived>(7);
        weak_ptr<PtrDerived> weak = shared;
        std::atomic<size_t> locked_count = 0;

        {
            std::vector<std::thread> workers;
            workers.reserve(THREADS_COUNT);

            for (size_t i = 0; i < THREADS_COUNT; i++) {
                workers.emplace_back([&shared, &weak, &locked_count] {
                    for (size_t j = 0; j < ITERATIONS_COUNT; j++) {
                        shared_ptr<PtrDerived> copy = shared;
                        weak_ptr<PtrDerived> local_weak = copy;

                        if (auto locked = local_weak.lock()) {
                            locked_count.fetch_add(1, std::memory_order_relaxed);
                        }

                        ignore_unused(weak.use_count());
                    }
                });
            }

            for (auto& worker : workers) {
                worker.join();
            }
        }

        CHECK(locked_count == THREADS_COUNT * ITERATIONS_COUNT);
        CHECK(shared.use_count() == 1);
        CHECK(weak.use_count() == 1);

        shared.reset();

        CHECK(weak.use_count() == 0);
        CHECK_FALSE(weak.lock());
    }

    SECTION("SharedOwnershipReleaseRaceDestroysExactlyOnce")
    {
        constexpr size_t THREADS_COUNT = 8;
        constexpr size_t ITERATIONS_COUNT = 5000;

        std::atomic<size_t> destroyed_count = 0;

        struct RaceValue
        {
            explicit RaceValue(std::atomic<size_t>& counter) :
                Counter {&counter}
            {
            }
            ~RaceValue() { Counter->fetch_add(1, std::memory_order_relaxed); }
            RaceValue(const RaceValue&) = delete;
            RaceValue(RaceValue&&) noexcept = delete;
            auto operator=(const RaceValue&) = delete;
            auto operator=(RaceValue&&) noexcept = delete;

            ptr<std::atomic<size_t>> Counter;
        };

        auto shared = SafeAlloc::MakeShared<RaceValue>(destroyed_count);
        weak_ptr<RaceValue> weak = shared;

        {
            std::vector<std::thread> workers;
            workers.reserve(THREADS_COUNT);

            for (size_t i = 0; i < THREADS_COUNT; i++) {
                workers.emplace_back([&weak] {
                    for (size_t j = 0; j < ITERATIONS_COUNT; j++) {
                        if (auto locked = weak.lock()) {
                            shared_ptr<RaceValue> copy = locked;
                            ignore_unused(copy);
                        }
                    }
                });
            }

            shared.reset(); // drop the owning reference while workers race lock()

            for (auto& worker : workers) {
                worker.join();
            }
        }

        CHECK(destroyed_count == 1);
        CHECK(weak.use_count() == 0);
        CHECK_FALSE(weak.lock());
    }

    SECTION("SharedAssignFromSourceInsideOwnPointeeIsSafe")
    {
        struct ChainNode
        {
            shared_ptr<ChainNode> Next {};
            int32_t Value {};
        };

        auto head = SafeAlloc::MakeShared<ChainNode>();
        head->Value = 1;
        head->Next = SafeAlloc::MakeShared<ChainNode>();
        head->Next->Value = 2;
        head->Next->Next = SafeAlloc::MakeShared<ChainNode>();
        head->Next->Next->Value = 3;

        head = head->Next; // copy-assign from a member of the pointee released by the assignment

        CHECK(head->Value == 2);

        head = std::move(head->Next); // move-assign from a member of the pointee released by the assignment

        CHECK(head->Value == 3);
        CHECK_FALSE(head->Next);
    }

    SECTION("UniqueDelAssignFromSourceInsideOwnPointeeIsSafe")
    {
        struct DelNode
        {
            unique_del_nptr<DelNode> Next {};
            int32_t Value {};
        };

        size_t deleted_count = 0;

        const auto make_node = [&deleted_count](int32_t value) -> unique_del_nptr<DelNode> {
            auto owner = SafeAlloc::MakeUnique<DelNode>();
            auto released = owner.release();
            released->Value = value;
            return make_unique_del_ptr(released, [&deleted_count](ptr<DelNode> node) noexcept {
                deleted_count++;
                auto owned_node = adopt_unique_ptr(node);
                ignore_unused(owned_node);
            });
        };

        auto head = make_node(1);
        head->Next = make_node(2);
        head->Next->Next = make_node(3);

        head = std::move(head->Next); // move-assign from a member of the pointee destroyed by the assignment

        CHECK(head->Value == 2);
        CHECK(deleted_count == 1);

        head = std::move(head->Next);

        CHECK(head->Value == 3);
        CHECK(deleted_count == 2);

        head = nullptr;

        CHECK(deleted_count == 3);
    }
}

FO_END_NAMESPACE
