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
#include "StringObject.h"
#include "StringUtils.h"

FO_BEGIN_NAMESPACE

namespace
{
    // A capacity of its own keeps the behavioural assertions readable and independent of the tuned build value
    using test_string = basic_string<char, 7>;

    [[nodiscard]] auto MakeFilled(size_t count, char ch) -> test_string
    {
        return test_string(count, ch);
    }
}

TEST_CASE("StringObject")
{
    SECTION("LayoutAndVocabularyContracts")
    {
        STATIC_REQUIRE(std::is_same_v<string, basic_string<char, STRING_INLINE_CAPACITY>>);
        STATIC_REQUIRE(std::is_same_v<string::value_type, char>);
        STATIC_REQUIRE(std::is_same_v<string::iterator, char*>);
        STATIC_REQUIRE(std::is_same_v<string::const_iterator, const char*>);
        STATIC_REQUIRE(std::is_nothrow_move_constructible_v<string>);
        STATIC_REQUIRE(std::is_nothrow_move_assignable_v<string>);
        STATIC_REQUIRE(std::is_copy_constructible_v<string>);
        STATIC_REQUIRE(std::is_convertible_v<string, string_view>);

        // The inline array is rounded up to the pointer size and joined by the size and capacity words
        STATIC_REQUIRE(sizeof(test_string) == 8 + 2 * sizeof(size_t));
        STATIC_REQUIRE(sizeof(string) == (STRING_INLINE_CAPACITY + 1 + sizeof(void*) - 1) / sizeof(void*) * sizeof(void*) + 2 * sizeof(size_t));

        // The wide string is given the same byte budget rather than the same character count
        STATIC_REQUIRE(sizeof(wstring) == sizeof(string));
    }

    SECTION("EmptyStringIsInlinedAndNullTerminated")
    {
        test_string str;

        CHECK(str.empty());
        CHECK(str.size() == 0);
        CHECK(str.capacity() == test_string::inline_capacity);
        CHECK(str.is_inlined());
        CHECK(str.c_str()[0] == '\0');
        CHECK(str == "");
    }

    SECTION("ShortTargetStaysInsideTheObject")
    {
        test_string str = "1234567";

        CHECK(str.size() == 7);
        CHECK(str.is_inlined());
        CHECK(str.capacity() == 7);
        CHECK(str.data() == reinterpret_cast<const char*>(&str));
        CHECK(str.c_str()[7] == '\0');
    }

    SECTION("OneCharacterOverTheBudgetMovesToTheHeap")
    {
        test_string str = "12345678";

        CHECK(str.size() == 8);
        CHECK_FALSE(str.is_inlined());
        CHECK(str.capacity() > test_string::inline_capacity);
        CHECK(str == "12345678");
        CHECK(str.c_str()[8] == '\0');
    }

    SECTION("GrowthKeepsContentAndTerminator")
    {
        test_string str;

        for (int32_t i = 0; i < 200; i++) {
            str.push_back(static_cast<char>('a' + i % 26));
            REQUIRE(str.size() == static_cast<size_t>(i) + 1);
            REQUIRE(str.c_str()[str.size()] == '\0');
            REQUIRE(str.capacity() >= str.size());
        }

        CHECK(str[0] == 'a');
        CHECK(str[25] == 'z');
        CHECK(str[26] == 'a');
        CHECK(str.back() == str[199]);
    }

    SECTION("ReserveDoesNotShrinkAndClearKeepsCapacity")
    {
        test_string str = "abc";
        str.reserve(100);
        size_t reserved = str.capacity();

        CHECK(reserved >= 100);
        CHECK(str == "abc");

        str.reserve(4);
        CHECK(str.capacity() == reserved);

        str.clear();
        CHECK(str.empty());
        CHECK(str.capacity() == reserved);
    }

    SECTION("ShrinkToFitReturnsAShortStringToTheObject")
    {
        test_string str;
        str.reserve(100);
        (void)str.assign("abc");

        REQUIRE_FALSE(str.is_inlined());

        str.shrink_to_fit();

        CHECK(str.is_inlined());
        CHECK(str == "abc");

        test_string long_str = MakeFilled(100, 'x');
        long_str.reserve(400);
        long_str.shrink_to_fit();

        CHECK(long_str.size() == 100);
        CHECK(long_str.capacity() >= 100);
        CHECK(long_str == MakeFilled(100, 'x'));
    }

    SECTION("ConstructorsCoverTheStandardSet")
    {
        CHECK(test_string(3, 'z') == "zzz");
        CHECK(test_string("abcdef", 3) == "abc");
        CHECK(test_string(string_view {"abcdef"}) == "abcdef");
        CHECK(test_string(string_view {"abcdef"}, 2, 3) == "cde");
        CHECK(test_string(test_string {"abcdef"}, 2) == "cdef");
        CHECK(test_string(test_string {"abcdef"}, 1, 2) == "bc");
        CHECK(test_string({'a', 'b', 'c'}) == "abc");

        vector<char> chars {'x', 'y', 'z'};
        CHECK(test_string(chars.begin(), chars.end()) == "xyz");
    }

    SECTION("MoveLeavesTheSourceEmptyForBothTiers")
    {
        test_string inlined = "abc";
        test_string moved_inlined = std::move(inlined);

        CHECK(moved_inlined == "abc");
        CHECK(inlined.empty()); // NOLINT(bugprone-use-after-move)
        CHECK(inlined.is_inlined()); // NOLINT(bugprone-use-after-move)

        test_string heaped = MakeFilled(100, 'q');
        const char* heap_data = heaped.data();
        test_string moved_heaped = std::move(heaped);

        CHECK(moved_heaped.data() == heap_data);
        CHECK(moved_heaped.size() == 100);
        CHECK(heaped.empty()); // NOLINT(bugprone-use-after-move)
        CHECK(heaped.is_inlined()); // NOLINT(bugprone-use-after-move)
    }

    SECTION("SelfAssignmentAndSelfAppendKeepTheContent")
    {
        test_string str = "abcdefghij";
        str = str; // NOLINT(clang-diagnostic-self-assign-overloaded)
        CHECK(str == "abcdefghij");

        test_string short_str = "ab";
        (void)short_str.append(short_str);
        CHECK(short_str == "abab");

        test_string growing = "abcdefg";
        (void)growing.append(growing);
        CHECK(growing == "abcdefgabcdefg");

        test_string self_slice = "abcdef";
        (void)self_slice.assign(self_slice.data() + 2, 3);
        CHECK(self_slice == "cde");
    }

    SECTION("SelfReferencingInsertAndReplaceAreDetached")
    {
        test_string str = "abcdef";
        (void)str.insert(0, str.data() + 3, 3);
        CHECK(str == "defabcdef");

        test_string replaced = "abcdefgh";
        (void)replaced.replace(0, 2, replaced.data() + 4, 4);
        CHECK(replaced == "efghcdefgh");
    }

    SECTION("InsertEraseAndReplaceMatchTheStandardShapes")
    {
        test_string str = "hello world";

        (void)str.insert(5, ",");
        CHECK(str == "hello, world");

        (void)str.erase(5, 1);
        CHECK(str == "hello world");

        (void)str.replace(6, 5, "there");
        CHECK(str == "hello there");

        (void)str.replace(0, 5, 3, 'y');
        CHECK(str == "yyy there");

        auto it = str.erase(str.begin(), str.begin() + 4);
        CHECK(it == str.begin());
        CHECK(str == "there");

        (void)str.insert(str.begin(), 2, '!');
        CHECK(str == "!!there");

        (void)str.insert(5, string_view {"XY"});
        CHECK(str == "!!theXYre");
    }

    SECTION("ResizeGrowsWithFillAndTruncates")
    {
        test_string str = "ab";

        str.resize(5, '-');
        CHECK(str == "ab---");

        str.resize(2);
        CHECK(str == "ab");

        str.resize(40, '=');
        CHECK(str.size() == 40);
        CHECK(str.starts_with("ab"));
        CHECK(str.back() == '=');
    }

    SECTION("SearchAndCompareDelegateToTheView")
    {
        test_string str = "the quick brown fox";

        CHECK(str.find("quick") == 4);
        CHECK(str.find('q') == 4);
        CHECK(str.find(string_view {"brown"}) == 10);
        CHECK(str.find("nothing") == test_string::npos);
        CHECK(str.rfind('o') == 17);
        CHECK(str.find_first_of("xq") == 4);
        CHECK(str.find_last_of("xq") == 18);
        CHECK(str.find_first_not_of("teh ") == 4);
        CHECK(str.substr(4, 5) == "quick");
        CHECK(str.compare("the quick brown fox") == 0);
        CHECK(str.compare(0, 3, "the") == 0);
        CHECK(str.starts_with("the"));
        CHECK(str.ends_with("fox"));
        CHECK(str.contains("brown"));
    }

    SECTION("ComparisonsCoverStringViewAndLiterals")
    {
        test_string abc = "abc";
        test_string abd = "abd";

        CHECK(abc == test_string {"abc"});
        CHECK(abc != abd);
        CHECK(abc < abd);
        CHECK(abd > abc);
        CHECK(abc == "abc");
        CHECK(abc < "abd");
        CHECK(abc == string_view {"abc"});
        CHECK(string_view {"abc"} == abc);
        CHECK(abc < string_view {"abd"});
    }

    SECTION("ConcatenationCoversTheStandardOverloads")
    {
        test_string abc = "abc";
        test_string def = "def";

        CHECK(abc + def == "abcdef");
        CHECK(abc + "def" == "abcdef");
        CHECK("def" + abc == "defabc");
        CHECK(abc + 'd' == "abcd");
        CHECK('d' + abc == "dabc");
        CHECK(test_string {"abc"} + def == "abcdef");
        CHECK(abc + test_string {"def"} == "abcdef");
        CHECK(test_string {"abc"} + test_string {"def"} == "abcdef");
    }

    SECTION("SwapExchangesEveryTierCombination")
    {
        test_string inlined = "abc";
        test_string heaped = MakeFilled(100, 'z');

        inlined.swap(heaped);

        CHECK(inlined.size() == 100);
        CHECK_FALSE(inlined.is_inlined());
        CHECK(heaped == "abc");
        CHECK(heaped.is_inlined());

        test_string first = "one";
        test_string second = "two";
        swap(first, second);

        CHECK(first == "two");
        CHECK(second == "one");
    }

    SECTION("IteratorsWalkTheWholeContent")
    {
        test_string str = "abcdefghijkl";

        CHECK(static_cast<size_t>(str.end() - str.begin()) == str.size());
        CHECK(*str.begin() == 'a');
        CHECK(*(str.end() - 1) == 'l');
        CHECK(*str.rbegin() == 'l');
        CHECK(std::distance(str.rbegin(), str.rend()) == static_cast<ptrdiff_t>(str.size()));

        test_string reversed(str.rbegin(), str.rend());
        CHECK(reversed == "lkjihgfedcba");

        std::ranges::sort(reversed);
        CHECK(reversed == "abcdefghijkl");
    }

    SECTION("OutOfRangeAccessThrows")
    {
        test_string str = "abc";

        CHECK_THROWS_AS((void)str.at(3), std::out_of_range);
        CHECK_THROWS_AS((void)str.substr(4), std::out_of_range);
        CHECK(str.at(2) == 'c');
    }

    SECTION("FormattingAndHashingSeeTheContent")
    {
        string str = "formatted";

        CHECK(strex("{}", str).str() == "formatted");
        CHECK(strex("[{:>12}]", str).str() == "[   formatted]");
        CHECK(hashing::hash<string> {}(str) == hashing::hash<string_view> {}(string_view {"formatted"}));
        CHECK(std::hash<string> {}(str) == std::hash<string_view> {}(string_view {"formatted"}));

        unordered_map<string, int32_t> by_name;
        by_name.emplace("key", 7);
        CHECK(by_name.at("key") == 7);
        CHECK(by_name.find(string_view {"key"}) != by_name.end());

        map<string, int32_t> ordered;
        ordered.emplace("key", 7);
        CHECK(ordered.find(string_view {"key"}) != ordered.end());
    }

    SECTION("StreamsReadAndWriteTheContent")
    {
        ostringstream out;
        out << string {"written"};
        CHECK(out.str() == "written");

        istringstream in(make_stream_string("first second\nthird line"));
        string token;
        in >> token;
        CHECK(token == "first");

        string rest;
        (void)getline(in, rest);
        CHECK(rest == " second");

        string next_line;
        (void)getline(in, next_line, '\n');
        CHECK(next_line == "third line");
    }

    SECTION("ConstantEvaluationWorksForTheInlineTier")
    {
        constexpr auto make_inline = []() constexpr {
            test_string str;
            (void)str.append("abc");
            str.push_back('d');
            return str.size();
        };

        STATIC_REQUIRE(make_inline() == 4);
        STATIC_REQUIRE(test_string {}.empty());
        STATIC_REQUIRE(test_string {"abcd"}.size() == 4);
    }

    SECTION("WideStringHoldsWideCharacters")
    {
        wstring wide = L"wide";

        CHECK(wide.size() == 4);
        CHECK(wide == L"wide");
        CHECK(wide.is_inlined());

        (void)wide.append(L" text");
        CHECK(wide == L"wide text");
        CHECK(wide.c_str()[wide.size()] == L'\0');
    }
}

FO_END_NAMESPACE
