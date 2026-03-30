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
//

#include "catch_amalgamated.hpp"

#include "Common.h"

FO_BEGIN_NAMESPACE

TEST_CASE("HashedString")
{
    SECTION("EmptyHash")
    {
        CHECK(Hashing::MurmurHash2(nullptr, 0) == 0);
        CHECK(Hashing::MurmurHash2_64(nullptr, 0) == 0);

        const hstring empty {};
        CHECK_FALSE(static_cast<bool>(empty));
        CHECK(empty.as_hash() == 0);
        CHECK(empty.as_str().empty());
    }

    SECTION("StorageRoundtrip")
    {
        HashStorage storage;

        const auto hs = storage.ToHashedString("EssentialsTest");
        CHECK(static_cast<bool>(hs));
        CHECK(hs.as_hash() != 0);
        CHECK(hs.as_str() == "EssentialsTest");

        const auto resolved = storage.ResolveHash(hs.as_hash());
        CHECK(resolved == hs);
        CHECK(resolved.as_str() == "EssentialsTest");
    }

    SECTION("IdempotentInsertion")
    {
        HashStorage storage;

        const auto hs1 = storage.ToHashedString("same_value");
        const auto hs2 = storage.ToHashedString("same_value");

        CHECK(hs1 == hs2);
        CHECK(hs1.as_hash() == hs2.as_hash());
        CHECK(hs1.as_str() == hs2.as_str());
    }

    SECTION("ResolveHashNoThrow")
    {
        HashStorage storage;

        bool failed = false;
        const auto unresolved_hash = Hashing::MurmurHash2("missing", 7);
        const auto unresolved = storage.ResolveHash(unresolved_hash, &failed);

        CHECK(failed);
        CHECK_FALSE(static_cast<bool>(unresolved));
    }

    SECTION("ResolveHashNoThrowPreservesFailureOnSuccess")
    {
        HashStorage storage;

        const auto hs = storage.ToHashedString("known_value");
        bool failed = true;
        const auto resolved = storage.ResolveHash(hs.as_hash(), &failed);

        CHECK(failed);
        CHECK(resolved == hs);
        CHECK(resolved.as_str() == "known_value");
    }

    SECTION("ResolveHashThrows")
    {
        HashStorage storage;

        const auto hs = storage.ToHashedString("known");
        CHECK_NOTHROW(storage.ResolveHash(hs.as_hash()));
        CHECK_THROWS_AS(storage.ResolveHash(Hashing::MurmurHash2("unknown", 7)), HashResolveException);
    }

    SECTION("ResolveHashNoThrowNullFailed")
    {
        HashStorage storage;

        const auto unresolved = storage.ResolveHash(Hashing::MurmurHash2("missing", 7), nullptr);
        CHECK_FALSE(static_cast<bool>(unresolved));
    }

    SECTION("ResolveHashNoThrowZeroHashPreservesFailure")
    {
        HashStorage storage;

        bool failed = true;
        const auto resolved = storage.ResolveHash(0, &failed);

        CHECK(failed);
        CHECK_FALSE(static_cast<bool>(resolved));
    }

    SECTION("ResolveHashNoThrowStickyFailureFlow")
    {
        HashStorage storage;

        const auto hs = storage.ToHashedString("known_flow");
        bool failed = false;

        CHECK_FALSE(static_cast<bool>(storage.ResolveHash(Hashing::MurmurHash2("missing", 7), &failed)));
        CHECK(failed);

        const auto resolved = storage.ResolveHash(hs.as_hash(), &failed);
        CHECK(failed);
        CHECK(resolved == hs);

        const auto zero_resolved = storage.ResolveHash(0, &failed);
        CHECK(failed);
        CHECK_FALSE(static_cast<bool>(zero_resolved));
    }

    SECTION("EmptyStringToHashedString")
    {
        HashStorage storage;

        const auto hs = storage.ToHashedString("");
        CHECK_FALSE(static_cast<bool>(hs));
        CHECK(hs.as_hash() == 0);
        CHECK(hs.as_str().empty());
    }
}

FO_END_NAMESPACE
