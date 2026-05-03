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

#include "TextureAtlas.h"

FO_BEGIN_NAMESPACE

TEST_CASE("TextureAtlasSpaceNode")
{
    SECTION("SplitCreatesRightAndBottomChildren")
    {
        TextureAtlas::SpaceNode root {nullptr, {0, 0}, {10, 10}};

        auto* node = root.FindPosition({4, 3});

        REQUIRE(node == &root);
        CHECK(root.Busy);
        CHECK(root.Pos == ipos32 {0, 0});
        CHECK(root.Size == isize32 {4, 3});
        REQUIRE(root.Children.size() == 2);
        CHECK(root.Children[0]->Parent == &root);
        CHECK(root.Children[0]->Pos == ipos32 {4, 0});
        CHECK(root.Children[0]->Size == isize32 {6, 3});
        CHECK(root.Children[1]->Parent == &root);
        CHECK(root.Children[1]->Pos == ipos32 {0, 3});
        CHECK(root.Children[1]->Size == isize32 {10, 7});
    }

    SECTION("SearchUsesChildrenBeforeRejecting")
    {
        TextureAtlas::SpaceNode root {nullptr, {0, 0}, {10, 10}};

        auto* first = root.FindPosition({4, 3});
        auto* second = root.FindPosition({2, 3});
        auto* third = root.FindPosition({10, 7});
        auto* missing = root.FindPosition({1, 8});

        REQUIRE(first == &root);
        REQUIRE(second != nullptr);
        REQUIRE(third != nullptr);
        CHECK(second != first);
        CHECK(second->Pos == ipos32 {4, 0});
        CHECK(second->Size == isize32 {2, 3});
        CHECK(third->Pos == ipos32 {0, 3});
        CHECK(third->Size == isize32 {10, 7});
        CHECK(missing == nullptr);
        CHECK(root.IsBusyRecursively());
    }

    SECTION("FreeCollapsesFullyReleasedTree")
    {
        TextureAtlas::SpaceNode root {nullptr, {0, 0}, {10, 10}};

        auto* right = root.FindPosition({4, 3});
        auto* child = root.FindPosition({2, 3});

        REQUIRE(right == &root);
        REQUIRE(child != nullptr);
        REQUIRE(root.Children.size() == 2);

        child->Free();

        CHECK_FALSE(child->Busy);
        REQUIRE(root.Children.size() == 2);

        root.Free();

        CHECK_FALSE(root.Busy);
        CHECK(root.Size == isize32 {10, 10});
        CHECK(root.Children.empty());
        CHECK_FALSE(root.IsBusyRecursively());
    }

    SECTION("FreePropagatesToParentWhenBranchBecomesEmpty")
    {
        TextureAtlas::SpaceNode root {nullptr, {0, 0}, {10, 10}};

        auto* first = root.FindPosition({4, 3});
        auto* nested = root.FindPosition({2, 3});

        REQUIRE(first == &root);
        REQUIRE(nested != nullptr);
        REQUIRE(nested == root.Children[0].get());
        REQUIRE(nested->Parent == &root);
        REQUIRE(root.Children[0]->Children.size() == 1);

        root.Free();

        CHECK_FALSE(root.Busy);
        REQUIRE(root.Children.size() == 2);

        nested->Free();

        CHECK(root.Size == isize32 {10, 10});
        CHECK(root.Children.empty());
        CHECK_FALSE(root.IsBusyRecursively());
    }
}

FO_END_NAMESPACE
