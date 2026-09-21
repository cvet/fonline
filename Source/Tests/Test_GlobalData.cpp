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
    std::atomic<int32_t> CreateCallCount {};
    int32_t DeleteCallCount {};
    vector<int32_t> DeleteCallOrder {};

    void CreateCallback() noexcept
    {
        CreateCallCount.fetch_add(1, std::memory_order_relaxed);
    }

    void DeleteNoop() noexcept
    {
    }

    void DeleteCallbackA() noexcept
    {
        ++DeleteCallCount;
        DeleteCallOrder.emplace_back(1);
    }

    void DeleteCallbackB() noexcept
    {
        ++DeleteCallCount;
        DeleteCallOrder.emplace_back(2);
    }

    void DeleteCallbackC() noexcept
    {
        ++DeleteCallCount;
        DeleteCallOrder.emplace_back(3);
    }

    // Each entry pairs the set name the observer was told with how many sets had been deleted by then
    struct ObservedTeardown
    {
        string Name {};
        int32_t DeletedBefore {};
    };

    void RecordTeardown(void* context, const char* set_name) noexcept
    {
        auto observed = static_cast<vector<ObservedTeardown>*>(context);
        observed->emplace_back(ObservedTeardown {.Name = string(set_name), .DeletedBefore = DeleteCallCount});
    }

    struct GlobalDataCallbacksGuard final
    {
        std::array<global_data::callback, global_data::MAX_CALLBACKS> SavedCreate {};
        std::array<global_data::callback, global_data::MAX_CALLBACKS> SavedDelete {};
        std::array<const char*, global_data::MAX_CALLBACKS> SavedNames {};
        int32_t SavedCount {};

        GlobalDataCallbacksGuard()
        {
            std::copy(std::begin(global_data::create_callbacks), std::end(global_data::create_callbacks), SavedCreate.begin());
            std::copy(std::begin(global_data::delete_callbacks), std::end(global_data::delete_callbacks), SavedDelete.begin());
            std::copy(std::begin(global_data::callback_names), std::end(global_data::callback_names), SavedNames.begin());
            SavedCount = global_data::callbacks_count;
        }

        ~GlobalDataCallbacksGuard()
        {
            // Hand the set back as created without running anything: this test never touched the real
            // globals of the process, and building them a second time would end the run
            global_data::callbacks_count = 0;
            (void)global_data::create();

            std::copy(SavedCreate.begin(), SavedCreate.end(), std::begin(global_data::create_callbacks));
            std::copy(SavedDelete.begin(), SavedDelete.end(), std::begin(global_data::delete_callbacks));
            std::copy(SavedNames.begin(), SavedNames.end(), std::begin(global_data::callback_names));
            global_data::callbacks_count = SavedCount;
        }
    };
}

TEST_CASE("GlobalData")
{
    GlobalDataCallbacksGuard callbacks_guard;

    CreateCallCount.store(0, std::memory_order_relaxed);
    DeleteCallCount = 0;
    DeleteCallOrder.clear();

    std::fill(std::begin(global_data::create_callbacks), std::end(global_data::create_callbacks), nullptr);
    std::fill(std::begin(global_data::delete_callbacks), std::end(global_data::delete_callbacks), nullptr);
    std::fill(std::begin(global_data::callback_names), std::end(global_data::callback_names), nullptr);

    SECTION("DeleteGlobalDataCallsRegisteredCallbacksInOrder")
    {
        global_data::callbacks_count = 3;
        global_data::delete_callbacks[0] = &DeleteCallbackA;
        global_data::delete_callbacks[1] = &DeleteCallbackB;
        global_data::delete_callbacks[2] = &DeleteCallbackC;

        global_data::destroy();

        CHECK(DeleteCallCount == 3);
        CHECK(DeleteCallOrder == vector<int32_t> {1, 2, 3});
        CHECK(global_data::callbacks_count == 3);
    }

    SECTION("TeardownObserverNamesEverySetBeforeItIsDeleted")
    {
        // A teardown that never returns is traced by the last name the observer wrote down, so each name must
        // arrive before its own delete callback runs and after the previous one has finished
        global_data::callbacks_count = 3;
        global_data::delete_callbacks[0] = &DeleteCallbackA;
        global_data::delete_callbacks[1] = &DeleteCallbackB;
        global_data::delete_callbacks[2] = &DeleteCallbackC;
        global_data::callback_names[0] = "SetA";
        global_data::callback_names[1] = "SetB";
        global_data::callback_names[2] = nullptr;

        vector<ObservedTeardown> observed;
        global_data::destroy(&RecordTeardown, &observed);

        REQUIRE(observed.size() == 3);
        CHECK(observed[0].Name == "SetA");
        CHECK(observed[0].DeletedBefore == 0);
        CHECK(observed[1].Name == "SetB");
        CHECK(observed[1].DeletedBefore == 1);
        // A set registered without a name is still announced, so the count of announcements stays exact
        CHECK(observed[2].Name.empty());
        CHECK(observed[2].DeletedBefore == 2);
        CHECK(DeleteCallOrder == vector<int32_t> {1, 2, 3});
    }

    SECTION("DeleteGlobalDataWithNoCallbacksIsNoop")
    {
        global_data::callbacks_count = 0;

        global_data::destroy();

        CHECK(DeleteCallCount == 0);
        CHECK(DeleteCallOrder.empty());
    }

    SECTION("SecondCreateLeavesTheSetAlone")
    {
        // The baker library entry also runs inside an application whose set exists: a second sweep must not build
        // every global twice, and the answer tells that caller the set is not its own to tear down
        global_data::callbacks_count = 1;
        global_data::create_callbacks[0] = &CreateCallback;
        global_data::delete_callbacks[0] = &DeleteNoop;

        global_data::destroy();
        CHECK(global_data::create());
        CHECK_FALSE(global_data::create());

        CHECK(CreateCallCount.load(std::memory_order_relaxed) == 1);
    }

    SECTION("CreateAndDeleteAlternate")
    {
        // Teardown is not the end of the process any more: the runtime library tears its set down as it
        // returns to the host, and a library loaded again has to get a working set
        global_data::callbacks_count = 1;
        global_data::create_callbacks[0] = &CreateCallback;
        global_data::delete_callbacks[0] = &DeleteCallbackA;

        global_data::destroy();
        (void)global_data::create();
        global_data::destroy();
        (void)global_data::create();

        CHECK(CreateCallCount.load(std::memory_order_relaxed) == 2);
        CHECK(DeleteCallCount == 2);
    }

    SECTION("SweepFromAnotherThreadWaitsRatherThanRacing")
    {
        // A second thread must see a finished set, not a half-built one: it blocks until the sweep that
        // started first is done
        global_data::callbacks_count = 1;
        global_data::create_callbacks[0] = &CreateCallback;
        global_data::delete_callbacks[0] = &DeleteNoop;

        global_data::destroy();

        std::thread creator {[] { (void)global_data::create(); }};
        creator.join();

        (void)global_data::create();

        CHECK(CreateCallCount.load(std::memory_order_relaxed) == 1);
    }

    SECTION("RacingCreateBuildsTheSetOnce")
    {
        global_data::callbacks_count = 1;
        global_data::create_callbacks[0] = &CreateCallback;
        global_data::delete_callbacks[0] = &DeleteNoop;

        global_data::destroy();

        vector<std::thread> racers;
        racers.reserve(8);
        std::atomic<int32_t> builders {};

        for (size_t i = 0; i < 8; i++) {
            racers.emplace_back([&builders] {
                if (global_data::create()) {
                    builders.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& racer : racers) {
            racer.join();
        }

        CHECK(CreateCallCount.load(std::memory_order_relaxed) == 1);
        // Exactly one caller is told the set is its own, so exactly one tears it down
        CHECK(builders.load(std::memory_order_relaxed) == 1);
    }
}

TEST_CASE("GlobalDataRegisteredSetsCarryTheirNames")
{
    // The names are what a stuck teardown is reported by, so every set the macro registered must have one
    REQUIRE(global_data::callbacks_count > 0);
    bool has_pools = false;

    for (int32_t i = 0; i < global_data::callbacks_count; i++) {
        REQUIRE(global_data::callback_names[i] != nullptr);
        CHECK(string_view(global_data::callback_names[i]).size() != 0);
        has_pools = has_pools || string_view(global_data::callback_names[i]) == "global_pools";
    }

    CHECK(has_pools);
}

FO_END_NAMESPACE
