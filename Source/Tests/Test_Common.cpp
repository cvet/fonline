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

TEST_CASE("CommonEvents")
{
    SECTION("DispatchAndManualUnsubscribe")
    {
        EventObserver<int32> observer;
        EventDispatcher<int32> dispatch(observer);
        EventUnsubscriber unsubscriber;
        int32 sum = 0;

        unsubscriber += (observer += [&](int32 value) { sum += value; });
        unsubscriber += (observer += [&](int32 value) { sum += value * 10; });

        dispatch(2);
        CHECK(sum == 22);

        unsubscriber.Unsubscribe();
        dispatch(3);
        CHECK(sum == 22);

        unsubscriber.Unsubscribe();
        dispatch(4);
        CHECK(sum == 22);
    }

    SECTION("DestructorUnsubscribes")
    {
        EventObserver<int32> observer;
        EventDispatcher<int32> dispatch(observer);
        int32 calls = 0;

        {
            EventUnsubscriber unsubscriber;
            unsubscriber += (observer += [&](int32 value) { calls += value; });

            dispatch(5);
            CHECK(calls == 5);
        }

        dispatch(7);
        CHECK(calls == 5);
    }

    SECTION("MoveKeepsSubscriptionOwnership")
    {
        EventObserver<int32> observer;
        EventDispatcher<int32> dispatch(observer);
        int32 calls = 0;

        EventUnsubscriber original;
        original += (observer += [&](int32 value) { calls += value; });

        EventUnsubscriber moved = std::move(original);

        dispatch(3);
        CHECK(calls == 3);

        moved.Unsubscribe();
        dispatch(8);
        CHECK(calls == 3);
    }
}

TEST_CASE("CommonUtilities")
{
    SECTION("WriteSimpleTgaCreatesFileWithExpectedHeader")
    {
        const auto temp_root = std::filesystem::temp_directory_path() / "lf_common_tests" / std::to_string(std::random_device {}());
        const auto file_path = temp_root / "nested" / "sample.tga";

        const isize32 image_size {2, 1};
        vector<ucolor> pixels;
        pixels.emplace_back(ucolor {1, 2, 3, 4});
        pixels.emplace_back(ucolor {5, 6, 7, 8});

        WriteSimpleTga(string(file_path.string()), image_size, pixels);

        REQUIRE(std::filesystem::exists(file_path));
        CHECK(std::filesystem::file_size(file_path) == 18 + pixels.size() * sizeof(uint32));

        std::ifstream input(file_path, std::ios::binary);
        REQUIRE(input);

        std::array<uint8, 18> header {};
        input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
        REQUIRE(input.gcount() == static_cast<std::streamsize>(header.size()));

        CHECK(header[2] == 2);
        CHECK(header[12] == 2);
        CHECK(header[13] == 0);
        CHECK(header[14] == 1);
        CHECK(header[15] == 0);
        CHECK(header[16] == 32);
        CHECK(header[17] == 0x20);

        std::array<uint32, 2> stored_pixels {};
        input.read(reinterpret_cast<char*>(stored_pixels.data()), static_cast<std::streamsize>(sizeof(stored_pixels)));
        REQUIRE(input.gcount() == static_cast<std::streamsize>(sizeof(stored_pixels)));

        CHECK(stored_pixels[0] == pixels[0].rgba);
        CHECK(stored_pixels[1] == pixels[1].rgba);

        input.close();

        const auto removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }
}

FO_END_NAMESPACE
