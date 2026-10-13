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

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "Test_LocationAndEntityMgmtHelpers.h"

FO_BEGIN_NAMESPACE

using namespace LocationAndEntityMgmtTests;

TEST_CASE("AngelScriptLocationCreationAndDestruction")
{
    MAKE_LEM_SERVER();

    SECTION("CreateDestroy")
    {
        RUN_LEM_FUNC("TestCreateDestroyLocation");
    }

    SECTION("MultipleLocations")
    {
        RUN_LEM_FUNC("TestMultipleLocations");
    }

    SECTION("LocationProperties")
    {
        RUN_LEM_FUNC("TestLocationProperties");
    }
}

TEST_CASE("AngelScriptEntityManagerOperations")
{
    MAKE_LEM_SERVER();

    SECTION("CritterOperations")
    {
        RUN_LEM_FUNC("TestEntityManagerCritterOperations");
    }

    SECTION("ItemOperations")
    {
        RUN_LEM_FUNC("TestEntityManagerItemOperations");
    }

    SECTION("DestroyEntity")
    {
        RUN_LEM_FUNC("TestEntityManagerDestroyEntity");
    }

    SECTION("BulkDestroyEntities")
    {
        RUN_LEM_FUNC("TestEntityManagerBulkDestroyEntities");
    }
}

TEST_CASE("AngelScriptCritterAdvancedOperations")
{
    MAKE_LEM_SERVER();

    SECTION("StateQueries")
    {
        RUN_LEM_FUNC("TestCritterStateQueries");
    }

    SECTION("Direction")
    {
        RUN_LEM_FUNC("TestCritterDirection");
    }

    SECTION("ItemManagement")
    {
        RUN_LEM_FUNC("TestCritterItemManagement");
    }

    SECTION("Conditions")
    {
        RUN_LEM_FUNC("TestCritterConditions");
    }

    SECTION("GetPlayer")
    {
        RUN_LEM_FUNC("TestCritterGetPlayer");
    }

    SECTION("Attachments")
    {
        RUN_LEM_FUNC("TestCritterAttachments");
    }
}

TEST_CASE("AngelScriptItemAdvancedOperations")
{
    MAKE_LEM_SERVER();

    SECTION("ContainerOperations")
    {
        RUN_LEM_FUNC("TestItemContainerOperations");
    }

    SECTION("MoveOperations")
    {
        RUN_LEM_FUNC("TestItemMoveOperations");
    }

    SECTION("DestroyVariants")
    {
        RUN_LEM_FUNC("TestItemDestroyVariants");
    }

    SECTION("MoveToContainer")
    {
        RUN_LEM_FUNC("TestItemMoveToContainer");
    }

    SECTION("BulkDestruction")
    {
        RUN_LEM_FUNC("TestBulkItemDestruction");
    }
}

TEST_CASE("AngelScriptNpcAndEntityQueries")
{
    MAKE_LEM_SERVER();

    SECTION("NpcQueries")
    {
        RUN_LEM_FUNC("TestNpcQueries");
    }

    SECTION("EntityGetById")
    {
        RUN_LEM_FUNC("TestEntityGetById");
    }
}

TEST_CASE("AngelScriptGameUtilityOperations")
{
    MAKE_LEM_SERVER();

    SECTION("Random")
    {
        RUN_LEM_FUNC("TestGameRandom");
    }

    SECTION("UnixTime")
    {
        RUN_LEM_FUNC("TestGameGetUnixTime");
    }

    SECTION("HstringOps")
    {
        RUN_LEM_FUNC("TestHstringOperations");
    }

    SECTION("DatabaseQueries")
    {
        RUN_LEM_FUNC("TestDatabaseQueries");
    }

    SECTION("MathOps")
    {
        RUN_LEM_FUNC("TestMathOps");
    }

    SECTION("TimeOps")
    {
        RUN_LEM_FUNC("TestTimeOperations");
    }
}

TEST_CASE("AngelScriptCollectionAndStringOps")
{
    MAKE_LEM_SERVER();

    SECTION("ArraySortSearch")
    {
        RUN_LEM_FUNC("TestArraySortAndSearch");
    }

    SECTION("DictAdvanced")
    {
        RUN_LEM_FUNC("TestDictAdvancedOps");
    }

    SECTION("StringAdvanced")
    {
        RUN_LEM_FUNC("TestStringAdvancedOps");
    }

    SECTION("HstringToFrom")
    {
        RUN_LEM_FUNC("TestHstringToFromString");
    }
}

TEST_CASE("AngelScriptBulkMoveOperations")
{
    MAKE_LEM_SERVER();

    SECTION("MoveItemsBulk")
    {
        RUN_LEM_FUNC("TestMoveItemsBulk");
    }
}

TEST_CASE("AngelScriptTimeEventManagerFiresScriptCallbacks")
{
    MAKE_LEM_SERVER();

    auto cr = server->CreateCritter(get_func("TestCritter"), false).hold_ref();

    auto start_self_event = [&server, &cr](string_view func_name, timespan repeat) {
        auto timer_func = server->FindFunc<void, ptr<ScriptSelfEntity>>(server->Hashes.to_hashed_string(func_name));
        REQUIRE(timer_func);
        return server->TimeEventMngr.StartTimeEvent(cr, Entity::TimeEventData::FuncType {std::move(timer_func)}, timespan {std::chrono::seconds {60}}, repeat, {});
    };
    auto backdate_all_events = [&cr]() {
        auto time_events = cr->GetTimeEvents();
        REQUIRE(time_events);

        for (auto& te : *time_events) {
            te->FireTime = nanotime {};
        }
    };

    int32_t base_look = cr->GetLookDistance();

    SECTION("RepeatingEventFiresAndReschedules")
    {
        uint32_t event_id = start_self_event("LocEntity::OnCritterTickTimer", timespan {std::chrono::seconds {5}});
        CHECK(event_id != 0);

        backdate_all_events();
        server->TimeEventMngr.ProcessTimeEvents();

        CHECK(cr->GetLookDistance() == base_look + 1);
        // A repeating event survives its own firing and is pushed into the future
        CHECK(cr->HasTimeEvents());

        optional<timespan> time_until_next;
        CHECK(server->TimeEventMngr.CollectReadyTimeEvents(time_until_next).empty());
        CHECK(time_until_next.has_value());
    }

    SECTION("OneShotEventIsRemovedAfterFiring")
    {
        uint32_t event_id = start_self_event("LocEntity::OnCritterTickTimer", {});
        CHECK(event_id != 0);

        backdate_all_events();
        server->TimeEventMngr.ProcessTimeEvents();

        CHECK(cr->GetLookDistance() == base_look + 1);
        CHECK_FALSE(cr->HasTimeEvents());
    }

    SECTION("ThrowingRepeatingCallbackStopsTheEvent")
    {
        (void)start_self_event("LocEntity::OnCritterThrowingTimer", timespan {std::chrono::seconds {5}});

        auto prev_callback = exceptions::get_callback();
        exceptions::set_callback([](string_view, const stack_trace::catched_data&, bool) { });
        auto restore_callback = scope_exit([prev = std::move(prev_callback)]() mutable noexcept { exceptions::set_callback(std::move(prev)); });

        backdate_all_events();
        server->TimeEventMngr.ProcessTimeEvents();

        CHECK(cr->GetLookDistance() == base_look + 1000);
        // A callback that throws must not keep rescheduling itself
        CHECK_FALSE(cr->HasTimeEvents());
    }

    SECTION("FireAndAdvanceRunsASingleEvent")
    {
        uint32_t event_id = start_self_event("LocEntity::OnCritterTickTimer", timespan {std::chrono::seconds {5}});

        backdate_all_events();
        auto next_delay = server->TimeEventMngr.FireAndAdvance(cr, event_id);

        CHECK(cr->GetLookDistance() == base_look + 1);
        REQUIRE(next_delay.has_value());
        CHECK(next_delay.value() > timespan {});

        // A one-shot event reports no follow-up delay because it is gone after firing
        uint32_t once_id = start_self_event("LocEntity::OnCritterTickTimer", {});
        backdate_all_events();
        CHECK_FALSE(server->TimeEventMngr.FireAndAdvance(cr, once_id).has_value());
    }

    server->TimeEventMngr.CancelAllForEntity(cr);
    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("AngelScriptLoadUnloadCritter")
{
    MAKE_LEM_SERVER();
    RUN_LEM_FUNC("TestUnloadCritter");
}

FO_END_NAMESPACE

#endif

