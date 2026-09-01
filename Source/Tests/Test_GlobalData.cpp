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

namespace
{
    int32_t DeleteCallCount {};
    vector<int32_t> DeleteCallOrder {};

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
        std::array<global_data_callback, MAX_GLOBAL_DATA_CALLBACKS> SavedCreate {};
        std::array<global_data_callback, MAX_GLOBAL_DATA_CALLBACKS> SavedDelete {};
        int32_t SavedCount {};

        GlobalDataCallbacksGuard()
        {
            std::copy(std::begin(create_global_data_callbacks), std::end(create_global_data_callbacks), SavedCreate.begin());
            std::copy(std::begin(delete_global_data_callbacks), std::end(delete_global_data_callbacks), SavedDelete.begin());
            SavedCount = global_data_callbacks_count;
        }

        ~GlobalDataCallbacksGuard()
        {
            std::copy(SavedCreate.begin(), SavedCreate.end(), std::begin(create_global_data_callbacks));
            std::copy(SavedDelete.begin(), SavedDelete.end(), std::begin(delete_global_data_callbacks));
            global_data_callbacks_count = SavedCount;
        }
    };
}

TEST_CASE("GlobalData")
{
    GlobalDataCallbacksGuard callbacks_guard;

    DeleteCallCount = 0;
    DeleteCallOrder.clear();

    std::fill(std::begin(create_global_data_callbacks), std::end(create_global_data_callbacks), nullptr);
    std::fill(std::begin(delete_global_data_callbacks), std::end(delete_global_data_callbacks), nullptr);

    SECTION("DeleteGlobalDataCallsRegisteredCallbacksInOrder")
    {
        global_data_callbacks_count = 3;
        delete_global_data_callbacks[0] = &DeleteCallbackA;
        delete_global_data_callbacks[1] = &DeleteCallbackB;
        delete_global_data_callbacks[2] = &DeleteCallbackC;

        delete_global_data();

        CHECK(DeleteCallCount == 3);
        CHECK(DeleteCallOrder == vector<int32_t> {1, 2, 3});
        CHECK(global_data_callbacks_count == 3);
    }

    SECTION("DeleteGlobalDataWithNoCallbacksIsNoop")
    {
        global_data_callbacks_count = 0;

        delete_global_data();

        CHECK(DeleteCallCount == 0);
        CHECK(DeleteCallOrder.empty());
    }
}

FO_END_NAMESPACE
