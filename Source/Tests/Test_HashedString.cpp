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

#include "Common.h"

FO_BEGIN_NAMESPACE

TEST_CASE("HashedString")
{
    SECTION("EmptyHash")
    {
        hstring empty {};
        CHECK_FALSE(static_cast<bool>(empty));
        CHECK(empty.as_hash() == 0);
        CHECK(empty.as_str().empty());
    }

    SECTION("StorageRoundtrip")
    {
        hash_storage storage {};

        hstring hs = storage.to_hashed_string("EssentialsTest");
        CHECK(static_cast<bool>(hs));
        CHECK(hs.as_hash() != 0);
        CHECK(hs.as_uint64() == hs.as_hash());
        CHECK(hs.as_hash() == hash_storage::default_hash(make_const_span(hs.as_str())));
        CHECK(hs.as_str() == "EssentialsTest");

        hstring resolved = storage.resolve_hash(hs.as_hash());
        CHECK(resolved == hs);
        CHECK(resolved.as_str() == "EssentialsTest");
    }

    SECTION("StableHashes")
    {
        CHECK(hash_storage::default_hash(make_const_span(string_view {})) == UINT64_C(0x42bc986dc5eec4d3));
        CHECK(hash_storage::default_hash(make_const_span(string_view {"1234567"})) == UINT64_C(0x25d18bd4513cc04c));
        CHECK(hash_storage::default_hash(make_const_span(string_view {"12345678"})) == UINT64_C(0x28dd7b65ff012f34));
        CHECK(hash_storage::default_hash(make_const_span(string_view {"0123456789abcdef"})) == UINT64_C(0x461ebd6f5b59dfa7));
        CHECK(hash_storage::default_hash(make_const_span(string_view {"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"})) == UINT64_C(0xcfe8eedc725d7d69));
    }

    SECTION("IdempotentInsertion")
    {
        hash_storage storage {};

        hstring hs1 = storage.to_hashed_string("same_value");
        hstring hs2 = storage.to_hashed_string("same_value");

        CHECK(hs1 == hs2);
        CHECK(hs1.as_hash() == hs2.as_hash());
        CHECK(hs1.as_str() == hs2.as_str());
    }

    SECTION("ResolveHashNoThrow")
    {
        hash_storage storage {};

        bool failed = false;
        auto unresolved_hash = hashing_ex::hash("missing", 7);
        hstring unresolved = storage.resolve_hash(unresolved_hash, &failed);

        CHECK(failed);
        CHECK_FALSE(static_cast<bool>(unresolved));
    }

    SECTION("ResolveHashNoThrowPreservesFailureOnSuccess")
    {
        hash_storage storage {};

        hstring hs = storage.to_hashed_string("known_value");
        bool failed = true;
        hstring resolved = storage.resolve_hash(hs.as_hash(), &failed);

        CHECK(failed);
        CHECK(resolved == hs);
        CHECK(resolved.as_str() == "known_value");
    }

    SECTION("ResolveHashThrows")
    {
        hash_storage storage {};

        hstring hs = storage.to_hashed_string("known");
        CHECK_NOTHROW(storage.resolve_hash(hs.as_hash()));
        CHECK_THROWS_AS(storage.resolve_hash(hashing_ex::hash("unknown", 7)), HashResolveException);
    }

    SECTION("ResolveHashFailureHandler")
    {
        hash_storage storage {};

        hstring::hash_t reported_hash = 0;
        int32_t reports = 0;

        storage.set_resolve_hash_failure_handler([&reported_hash, &reports](hstring::hash_t hash) {
            reported_hash = hash;
            reports++;
        });

        auto no_throw_unresolved_hash = hashing_ex::hash("unknown", 7);
        bool failed = false;
        CHECK_FALSE(static_cast<bool>(storage.resolve_hash(no_throw_unresolved_hash, &failed)));
        CHECK(failed);
        CHECK(reported_hash == no_throw_unresolved_hash);
        CHECK(reports == 1);

        hstring hs = storage.to_hashed_string("known");
        CHECK_NOTHROW(storage.resolve_hash(hs.as_hash()));
        CHECK(reports == 1);

        auto throwing_unresolved_hash = hashing_ex::hash("unknown_throwing", 16);
        CHECK_THROWS_AS(storage.resolve_hash(throwing_unresolved_hash), HashResolveException);
        CHECK(reported_hash == throwing_unresolved_hash);
        CHECK(reports == 2);

        storage.set_resolve_hash_failure_handler({});
        CHECK_THROWS_AS(storage.resolve_hash(hashing_ex::hash("missing", 7)), HashResolveException);
        CHECK(reports == 2);
    }

    SECTION("ResolveHashNoThrowNullFailed")
    {
        hash_storage storage {};

        hstring unresolved = storage.resolve_hash(hashing_ex::hash("missing", 7), nullptr);
        CHECK_FALSE(static_cast<bool>(unresolved));
    }

    SECTION("ResolveHashNoThrowZeroHashPreservesFailure")
    {
        hash_storage storage {};

        bool failed = true;
        hstring resolved = storage.resolve_hash(0, &failed);

        CHECK(failed);
        CHECK_FALSE(static_cast<bool>(resolved));
    }

    SECTION("ResolveHashNoThrowStickyFailureFlow")
    {
        hash_storage storage {};

        hstring hs = storage.to_hashed_string("known_flow");
        bool failed = false;

        CHECK_FALSE(static_cast<bool>(storage.resolve_hash(hashing_ex::hash("missing", 7), &failed)));
        CHECK(failed);

        hstring resolved = storage.resolve_hash(hs.as_hash(), &failed);
        CHECK(failed);
        CHECK(resolved == hs);

        hstring zero_resolved = storage.resolve_hash(0, &failed);
        CHECK(failed);
        CHECK_FALSE(static_cast<bool>(zero_resolved));
    }

    SECTION("EmptyStringToHashedString")
    {
        hash_storage storage {};

        hstring hs = storage.to_hashed_string("");
        CHECK_FALSE(static_cast<bool>(hs));
        CHECK(hs.as_hash() == 0);
        CHECK(hs.as_str().empty());
    }

    SECTION("CheckHashedStringChecksWithoutInserting")
    {
        hash_storage storage {};

        storage.to_hashed_string("registered_value");
        CHECK(storage.check_hashed_string("registered_value"));

        // An unregistered string is reported missing and must NOT be inserted
        CHECK_FALSE(storage.check_hashed_string("never_registered_value"));

        bool still_failed = false;
        (void)storage.resolve_hash(hashing_ex::hash("never_registered_value", 22), &still_failed);
        CHECK(still_failed);

        // Empty string is the zero hash, never a registered entry
        CHECK_FALSE(storage.check_hashed_string(""));
    }
}

FO_END_NAMESPACE
