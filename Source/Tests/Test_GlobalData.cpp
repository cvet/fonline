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

namespace
{
    int32 DeleteCallCount {};
    vector<int32> DeleteCallOrder {};

    void DeleteCallbackA()
    {
        ++DeleteCallCount;
        DeleteCallOrder.emplace_back(1);
    }

    void DeleteCallbackB()
    {
        ++DeleteCallCount;
        DeleteCallOrder.emplace_back(2);
    }

    void DeleteCallbackC()
    {
        ++DeleteCallCount;
        DeleteCallOrder.emplace_back(3);
    }

    struct GlobalDataCallbacksGuard final
    {
        std::array<GlobalDataCallback, MAX_GLOBAL_DATA_CALLBACKS> SavedCreate {};
        std::array<GlobalDataCallback, MAX_GLOBAL_DATA_CALLBACKS> SavedDelete {};
        int32 SavedCount {};

        GlobalDataCallbacksGuard()
        {
            std::copy(std::begin(CreateGlobalDataCallbacks), std::end(CreateGlobalDataCallbacks), SavedCreate.begin());
            std::copy(std::begin(DeleteGlobalDataCallbacks), std::end(DeleteGlobalDataCallbacks), SavedDelete.begin());
            SavedCount = GlobalDataCallbacksCount;
        }

        ~GlobalDataCallbacksGuard()
        {
            std::copy(SavedCreate.begin(), SavedCreate.end(), std::begin(CreateGlobalDataCallbacks));
            std::copy(SavedDelete.begin(), SavedDelete.end(), std::begin(DeleteGlobalDataCallbacks));
            GlobalDataCallbacksCount = SavedCount;
        }
    };
}

TEST_CASE("GlobalData")
{
    GlobalDataCallbacksGuard callbacks_guard;

    DeleteCallCount = 0;
    DeleteCallOrder.clear();

    std::fill(std::begin(CreateGlobalDataCallbacks), std::end(CreateGlobalDataCallbacks), nullptr);
    std::fill(std::begin(DeleteGlobalDataCallbacks), std::end(DeleteGlobalDataCallbacks), nullptr);

    SECTION("DeleteGlobalDataCallsRegisteredCallbacksInOrder")
    {
        GlobalDataCallbacksCount = 3;
        DeleteGlobalDataCallbacks[0] = &DeleteCallbackA;
        DeleteGlobalDataCallbacks[1] = &DeleteCallbackB;
        DeleteGlobalDataCallbacks[2] = &DeleteCallbackC;

        DeleteGlobalData();

        CHECK(DeleteCallCount == 3);
        CHECK(DeleteCallOrder == vector<int32> {1, 2, 3});
        CHECK(GlobalDataCallbacksCount == 3);
    }

    SECTION("DeleteGlobalDataWithNoCallbacksIsNoop")
    {
        GlobalDataCallbacksCount = 0;

        DeleteGlobalData();

        CHECK(DeleteCallCount == 0);
        CHECK(DeleteCallOrder.empty());
    }
}

FO_END_NAMESPACE
