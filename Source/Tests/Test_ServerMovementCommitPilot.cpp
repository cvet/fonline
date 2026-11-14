// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Test_ServerEngineHelpers.h"

FO_BEGIN_NAMESPACE

struct MovementCommitPilotControl
{
    ident_t TargetId {};
    ident_t DestinationMapId {};
    mpos TransformedHex {};
    int32_t Scenario {};
    std::atomic_bool Armed {};
    std::atomic_uint32_t Pre {};
    std::atomic_uint32_t Post {};
    std::atomic_uint32_t Actions {};
    std::atomic_uint32_t Reports {};
    std::atomic_uint32_t UnexpectedReports {};
    std::atomic_uint32_t PreConsistent {};
    std::atomic_uint32_t PostConsistent {};
    std::atomic_uint32_t AuthorityConsistent {};
    std::atomic_bool DirArmed {};
    std::atomic_uint32_t DirCalls {};
    std::atomic_uint32_t LockedRefusals {};
    std::atomic_uint32_t ReplacementUid {};
    std::atomic_bool FinishArmed {};
    std::atomic_uint32_t FinishCalls {};
    std::atomic_uint32_t FinishMoved {};
    nptr<PropertyRawData> BorrowedRawData {};
    std::atomic_uint32_t AliasMutations {};
};

static void ApplyMovementCommitPilotAction(ptr<Critter> target, ptr<MovementCommitPilotControl> control)
{
    control->Actions.fetch_add(1);
    int32_t scenario {control->Scenario};

    if (scenario == 5 || scenario == 6 || scenario == 9 || scenario == 21) {
        target->GetEngine()->CrMngr.DestroyCritter(target);
    }
    else if (scenario == 18 || scenario == 19 || scenario == 20) {
        auto owner {target->GetParent<Map>()};
        FO_VERIFY_AND_THROW(owner, "Commit pilot map destruction has no owner");
        target->GetEngine()->MapMngr.DestroyMap(owner);
    }
    else if (scenario == 7 || scenario == 8 || scenario == 10 || scenario == 16 || scenario == 17) {
        auto destination {target->GetEngine()->EntityMngr.GetMap(control->DestinationMapId)};
        FO_VERIFY_AND_THROW(destination, "Commit pilot destination is absent");
        FO_VERIFY_AND_THROW(IsEntityAccessValid(destination), "Commit pilot destination lacks existing cover");
        target->GetEngine()->MapMngr.TransferToMap(target, destination, mpos {40, 40}, mdir {}, std::nullopt);
    }

    if (scenario == 1 || scenario == 2 || scenario == 5 || scenario == 6 || scenario == 7 || scenario == 8 || scenario == 16 || scenario == 17 || scenario == 18 || scenario == 19) {
        throw GenericException("COMMIT_PILOT_INJECT");
    }
}

TEST_CASE("ServerMovementCommitBoundaryPilot")
{
    int32_t scenario {GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31)};
    INFO(scenario);
    REQUIRE(IsTestingInProgress);
    auto settings {ServerEngineTests::MakeServerTestSettings()};
    BakerTests::OverrideSetting(settings.Server.CritterMovingPeriodMs, int32_t {30000});
    string grid_name {"default"};

    if (scenario == 11) {
        grid_name = GENERATE(string {"Static"}, string {"Chunked"}, string {"Dynamic"});
        BakerTests::OverrideSetting(settings.Server.MapInstanceGridType, grid_name);
    }

    auto server {ServerEngineTests::MakeServerEngine(settings)};
    auto shutdown {scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); })};
    REQUIRE(ServerEngineTests::WaitForServerStart(server).empty());
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));
    bool locked {true};
    auto unlock {scope_exit([&]() noexcept {
        if (locked) {
            safe_call([&server] { server->Unlock(); });
        }
    })};

    auto control {safe_alloc::make_shared<MovementCommitPilotControl>()};
    control->Scenario = scenario;
    auto previous_reporter {exceptions::get_callback()};
    exceptions::set_callback([control, previous_reporter](string_view message, const stack_trace::catched_data& trace, bool fatal) mutable {
        if (!fatal && message.find("COMMIT_PILOT_INJECT") != string_view::npos) {
            control->Reports.fetch_add(1, std::memory_order_release);
        }
        else {
            control->UnexpectedReports.fetch_add(1, std::memory_order_release);

            if (previous_reporter) {
                previous_reporter(message, trace, fatal);
            }
        }
    });
    auto restore_reporter {scope_exit([previous = std::move(previous_reporter)]() mutable noexcept { exceptions::set_callback(std::move(previous)); })};

    hstring location_pid {server->Hashes.to_hashed_string("UnitTestLocation")};
    hstring map_pid {server->Hashes.to_hashed_string("UnitTestMap")};
    auto location {server->MapMngr.CreateLocation(location_pid, vector<hstring> {map_pid, map_pid}).hold_ref()};
    auto source {location->GetMapByIndex(0).try_hold_ref()};
    auto destination {location->GetMapByIndex(1).try_hold_ref()};
    REQUIRE(source);
    REQUIRE(destination);
    auto critter {server->CreateCritter(server->Hashes.to_hashed_string("UnitTestRat"), false).hold_ref()};
    ident_t critter_id {critter->GetId()};
    ident_t source_id {source->GetId()};
    ident_t destination_id {destination->GetId()};
    control->TargetId = critter_id;
    control->DestinationMapId = scenario == 16 || scenario == 17 || scenario == 26 ? source_id : destination_id;
    mpos initial_hex {scenario == 11 ? mpos {1, 1} : mpos {20, 20}};

    if (scenario == 11) {
        critter->SetMultihex(2);
    }

    server->MapMngr.TransferToMap(critter, source, initial_hex, mdir {}, std::nullopt);
    auto cover {[&] {
        small_vector<ptr<ServerEntity>, 4> entities {location};

        if (!source->IsDestroyed()) {
            entities.emplace_back(source);
        }
        if (!destination->IsDestroyed()) {
            entities.emplace_back(destination);
        }

        if (!critter->IsDestroyed()) {
            entities.emplace_back(critter);
        }

        server->RequireCurrentSyncContext()->SyncEntities(entities);
    }};
    cover();
    bool repaired {false};
    bool finish_subscribed {false};
    uintptr_t finish_subscription {reinterpret_cast<uintptr_t>(&finish_subscribed)};
    auto cleanup_fixture {[&] {
        control->Armed.store(false);
        control->DirArmed.store(false);
        control->FinishArmed.store(false);
        control->BorrowedRawData = nullptr;

        if (!locked) {
            locked = server->Lock(timespan {std::chrono::seconds {10}});
            FO_VERIFY_AND_THROW(locked, "Commit pilot could not acquire cleanup boundary");
        }

        if (!location->IsDestroyed()) {
            cover();

            if (finish_subscribed) {
                server->OnCritterFinish.Unsubscribe(finish_subscription);
                finish_subscribed = false;
            }

            if (!critter->IsDestroyed()) {
                if (critter->IsMoving()) {
                    server->StopCritterMoving(critter);
                }

                auto owner {critter->GetParent<Map>()};

                if (owner && !owner->IsDestroyed() && !owner->IsCritterOnHex(critter->GetHex(), critter)) {
                    owner->AddCritterToField(critter);
                    repaired = true;
                    logging::write("MOVEMENT_COMMIT_UNEXPECTED_REPAIR scenario={}", scenario);
                }
            }

            server->MapMngr.DestroyLocation(location);
        }
    }};
    auto cleanup {scope_exit([&]() noexcept { safe_call(cleanup_fixture); })};
    auto property {critter->GetPropertyHex()};
    REQUIRE(property->IsCoreProperty());
    REQUIRE_FALSE(property->IsMutable());
    REQUIRE(property->IsPlainData());
    REQUIRE(property->GetBaseSize() == sizeof(mpos));
    size_t initial_pre {property->GetSetters()->size()};
    size_t initial_post {property->GetPostSetters()->size()};
    control->TransformedHex = scenario == 4 ? initial_hex : scenario == 13 ? mpos {-1, 21} : mpos {24, 25};

    property->AddSetter([control](nptr<Entity> entity, ptr<const Property> prop, PropertyRawData& data) mutable {
        auto target {entity.dyn_cast<Critter>()};

        if (!target || target->GetId() != control->TargetId) {
            return;
        }

        control->Pre.fetch_add(1);

        if (control->Scenario == 31) {
            FO_VERIFY_AND_THROW(!control->BorrowedRawData, "Commit pilot raw-data borrow was already active");
            control->BorrowedRawData = make_nptr(&data);
            return;
        }

        if (!control->Armed.load() || control->Scenario == 2 || control->Scenario == 6 || control->Scenario == 8 || control->Scenario == 17 || control->Scenario == 19 || control->Scenario == 21) {
            return;
        }

        FO_VERIFY_AND_THROW(prop == target->GetPropertyHex() && data.GetSize() == sizeof(mpos), "Commit pilot pre-setter type mismatch");
        auto owner {target->GetParent<Map>()};
        FO_VERIFY_AND_THROW(owner && IsEntityAccessValid(target) && IsEntityAccessValid(owner), "Commit pilot pre-setter has no existing map cover");
        FO_VERIFY_AND_THROW(owner->GetCritter(target->GetId()) == target && owner->IsCritterOnHex(target->GetHex(), target), "Commit pilot pre-setter sees inconsistent membership");
        control->PreConsistent.fetch_add(1);
        control->Armed.store(false);
        int32_t mode {control->Scenario};

        if (mode == 3 || mode == 4 || mode == 13) {
            data.SetAs<mpos>(control->TransformedHex);
            control->Actions.fetch_add(1);
        }
        else if (mode == 14) {
            data.SetAs<uint16_t>(7);
            control->Actions.fetch_add(1);
        }
        else {
            ApplyMovementCommitPilotAction(target, control);
        }
    });
    property->AddPostSetter([control](nptr<Entity> entity, ptr<const Property>) mutable {
        auto target {entity.dyn_cast<Critter>()};

        if (!target || target->GetId() != control->TargetId) {
            return;
        }

        control->Post.fetch_add(1);

        if (control->Scenario == 31) {
            control->BorrowedRawData = nullptr;
        }

        auto owner {target->GetParent<Map>()};

        if (owner && owner->GetCritter(target->GetId()) == target) {
            FO_VERIFY_AND_THROW(owner->IsCritterOnHex(target->GetHex(), target), "Commit pilot post-setter sees inconsistent membership");
            control->PostConsistent.fetch_add(1);
        }

        if ((control->Scenario == 2 || control->Scenario == 6 || control->Scenario == 8 || control->Scenario == 17 || control->Scenario == 19 || control->Scenario == 21) && control->Armed.exchange(false)) {
            ApplyMovementCommitPilotAction(target, control);
        }
    });
    REQUIRE(property->GetSetters()->size() == initial_pre + 1);
    REQUIRE(property->GetPostSetters()->size() == initial_post + 1);
    auto dir_property {critter->GetPropertyDir()};
    REQUIRE(dir_property->IsCoreProperty());
    REQUIRE(dir_property->GetBaseSize() == sizeof(mdir));
    size_t initial_dir_post {dir_property->GetPostSetters()->size()};
    dir_property->AddPostSetter([control](nptr<Entity> entity, ptr<const Property>) mutable {
        auto target {entity.dyn_cast<Critter>()};

        if (!target || target->GetId() != control->TargetId || !control->DirArmed.exchange(false)) {
            return;
        }

        control->DirCalls.fetch_add(1);
        control->Actions.fetch_add(1);
        FO_VERIFY_AND_THROW(target->IsMapTransfersLocked(), "Commit pilot Dir callback did not run inside locked transfer");
        auto owner {target->GetParent<Map>()};
        FO_VERIFY_AND_THROW(owner && owner->IsCritterOnHex(target->GetHex(), target), "Commit pilot Dir callback source field is inconsistent");

        if (control->Scenario == 26) {
            owner->MoveCritterOnField(target, mpos {40, 40});
        }
        else if (control->Scenario == 27) {
            auto moving {ServerEngineTests::MakeServerMovementContext(owner->GetSize(), target->GetHex(), target->GetEngine()->GameTime.GetFrameTime())};
            target->GetEngine()->StartCritterMoving(target, moving, nullptr);
            control->ReplacementUid.store(target->GetMovingUid());
        }
        else if (control->Scenario == 28) {
            try {
                target->GetEngine()->MapMngr.TransferToMap(target, owner, mpos {40, 40}, mdir {}, std::nullopt);
            }
            catch (const GenericException& ex) {
                if (string_view {ex.what()}.find("Critter transfers locked") == string_view::npos) {
                    throw;
                }

                control->LockedRefusals.fetch_add(1);
            }
        }
        else {
            target->GetEngine()->CrMngr.DestroyCritter(target);
        }
    });
    REQUIRE(dir_property->GetPostSetters()->size() == initial_dir_post + 1);
    control->Armed.store(scenario != 0 && scenario != 11 && scenario != 12 && scenario != 15 && scenario < 22);
    control->DirArmed.store(scenario >= 26 && scenario <= 29);
    Entity::EventCallbackData finish {};
    finish.SubscriptionPtr = finish_subscription;
    finish.Callback = [control](FuncCallData& call) mutable {
        FO_VERIFY_AND_THROW(call.ArgsData.size() == 1, "Commit pilot finish event arity mismatch");
        ptr<void> target_data {call.ArgsData[0]};
        auto target {make_nptr(*cast_from_void<Entity**>(target_data.get())).dyn_cast<Critter>()};

        if (!target || target->GetId() != control->TargetId || !control->FinishArmed.exchange(false)) {
            return Entity::EventResult::ContinueChain;
        }

        control->FinishCalls.fetch_add(1);
        FO_VERIFY_AND_THROW(target->IsDestroying() && !target->IsDestroyed(), "Commit pilot did not observe real finish lifecycle state");
        auto owner {target->GetParent<Map>()};
        FO_VERIFY_AND_THROW(owner && IsEntityAccessValid(owner) && IsEntityAccessValid(target), "Commit pilot finish callback has no existing cover");
        mpos old_hex {target->GetHex()};
        FO_VERIFY_AND_THROW(owner->GetCritter(target->GetId()) == target && owner->IsCritterOnHex(old_hex, target), "Commit pilot finish source is not registered on its field");
        owner->MoveCritterOnField(target, mpos {40, 40});
        FO_VERIFY_AND_THROW(target->GetHex() == (mpos {40, 40}) && owner->IsCritterOnHex(target->GetHex(), target) && !owner->IsCritterOnHex(old_hex, target), "Commit pilot finish movement did not preserve field consistency");
        control->FinishMoved.fetch_add(1);
        return Entity::EventResult::ContinueChain;
    };
    server->OnCritterFinish.Subscribe(std::move(finish));
    finish_subscribed = true;
    control->FinishArmed.store(scenario == 30);
    mpos requested {scenario == 11 ? mpos {2, 1} : mpos {22, 23}};
    uint32_t authority_calls {0};
    function<bool()> authority {[&] {
        authority_calls++;

        if (scenario == 31) {
            auto clear_borrow {scope_exit([control]() mutable noexcept { control->BorrowedRawData = nullptr; })};
            FO_VERIFY_AND_THROW(control->BorrowedRawData && control->BorrowedRawData->GetSize() == sizeof(mpos), "Commit pilot raw-data borrow is absent or has wrong size");
            FO_VERIFY_AND_THROW(control->BorrowedRawData->GetAs<mpos>() == requested, "Commit pilot raw-data borrow has an unexpected input");
            FO_VERIFY_AND_THROW(critter->GetHex() == initial_hex && source->IsCritterOnHex(initial_hex, critter), "Commit pilot alias callback changed the live position prematurely");
            control->BorrowedRawData->SetAs<mpos>(mpos {25, 26});
            FO_VERIFY_AND_THROW(control->BorrowedRawData->GetAs<mpos>() == (mpos {25, 26}), "Commit pilot raw-data alias mutation was skipped");
            control->Actions.fetch_add(1);
            control->AliasMutations.fetch_add(1);
        }

        if (scenario >= 22 && scenario <= 25) {
            FO_VERIFY_AND_THROW(source->GetCritter(critter_id) == critter && source->IsCritterOnHex(initial_hex, critter), "Commit pilot authority entered inconsistent source state");
            control->AuthorityConsistent.fetch_add(1);
            control->Actions.fetch_add(1);

            if (scenario == 22) {
                server->CrMngr.DestroyCritter(critter);
            }
            else if (scenario == 23 || scenario == 24) {
                server->MapMngr.TransferToMap(critter, destination, mpos {40, 40}, mdir {}, std::nullopt);
            }
            else {
                auto moving {ServerEngineTests::MakeServerMovementContext(source->GetSize(), initial_hex, server->GameTime.GetFrameTime())};
                server->StartCritterMoving(critter, moving, nullptr);
            }
        }

        return scenario != 12 && scenario != 24;
    }};

    if (scenario <= 2 || scenario == 21) {
        auto moving_template {ServerEngineTests::MakeServerMovementContext(source->GetSize(), initial_hex, server->GameTime.GetFrameTime())};
        timespan overdue {std::chrono::milliseconds {iround<int32_t>(moving_template->GetWholeTime().to_ms<float64_t>()) + 100}};
        auto moving {ServerEngineTests::MakeServerMovementContext(source->GetSize(), initial_hex, server->GameTime.GetFrameTime() - overdue)};
        requested = moving->GetEndHex();
        server->StartCritterMoving(critter, moving, nullptr);
        uint32_t active_uid {critter->GetMovingUid()};
        REQUIRE(ServerEngineTests::WaitForUnlockedServerCondition(
            server, locked,
            [&] {
                cover();
                if (scenario == 21) {
                    return !server->EntityMngr.GetCritter(critter_id);
                }

                return scenario == 0 ? !critter->IsMoving() : control->Reports.load(std::memory_order_acquire) == 1;
            },
            std::chrono::milliseconds {1500}));
        REQUIRE(control->Reports.load() == (scenario == 0 || scenario == 21 ? uint32_t {0} : uint32_t {1}));

        if (scenario == 0) {
            REQUIRE(critter->GetMovingState() == MovingState::Success);
            REQUIRE(critter->GetHex() == requested);
            REQUIRE(control->Pre.load() == ServerEngineTests::SERVER_TEST_MOVE_STEPS.size());
            REQUIRE(control->Post.load() == ServerEngineTests::SERVER_TEST_MOVE_STEPS.size());
        }
        else {
            REQUIRE(control->Pre.load() == 1);
            REQUIRE(control->Post.load() == (scenario == 1 ? uint32_t {0} : uint32_t {1}));
            REQUIRE(control->Actions.load() == 1);

            if (scenario != 21) {
                REQUIRE((critter->GetHex() == initial_hex) == (scenario == 1));
                REQUIRE(critter->IsMoving());
                REQUIRE(critter->GetMovingUid() == active_uid);
            }
        }
    }
    else if (scenario == 15) {
        REQUIRE_NOTHROW(critter->SetHex(initial_hex));
        REQUIRE(control->Pre.load() == 0);
        REQUIRE(control->Post.load() == 0);
    }
    else if (scenario == 30) {
        REQUIRE_NOTHROW(server->CrMngr.DestroyCritter(critter));
        REQUIRE(control->FinishCalls.load() == 1);
        REQUIRE(control->FinishMoved.load() == 1);
        REQUIRE_FALSE(server->EntityMngr.GetCritter(critter_id));
        REQUIRE_FALSE(source->IsCritterOnHex(mpos {40, 40}, CritterFindType::Any));
        logging::write("MOVEMENT_COMMIT_FINISH calls=1 moved=1 destroyed=true registry_absent=true field_absent=true");
    }
    else if (scenario >= 26 && scenario <= 29) {
        REQUIRE_NOTHROW(server->MapMngr.TransferToMap(critter, source, requested, mdir {60}, std::nullopt));
        REQUIRE(control->DirCalls.load() == 1);
        REQUIRE(control->Actions.load() == 1);

        if (scenario != 29) {
            REQUIRE_FALSE(critter->IsMapTransfersLocked());
        }
        if (scenario == 27) {
            REQUIRE(critter->GetHex() == initial_hex);
            REQUIRE(critter->IsMoving());
            REQUIRE(critter->GetMovingUid() == control->ReplacementUid.load());
        }
        if (scenario == 28) {
            REQUIRE(control->LockedRefusals.load() == 1);
            REQUIRE(critter->GetHex() == requested);
        }
    }
    else {
        bool should_throw {scenario == 5 || scenario == 6 || scenario == 7 || scenario == 8 || scenario == 9 || scenario == 10 || scenario == 12 || scenario == 13 || scenario == 14 || scenario == 16 || scenario == 17 || scenario == 18 || scenario == 19 || scenario == 20 || (scenario >= 22 && scenario <= 25)};

        if (should_throw) {
            REQUIRE_THROWS_AS(source->MoveCritterOnField(critter, requested, authority), std::exception);
        }
        else {
            REQUIRE_NOTHROW(source->MoveCritterOnField(critter, requested, authority));
        }

        REQUIRE(control->Reports.load() == 0);

        if (scenario != 12) {
            REQUIRE(control->Actions.load() == (scenario == 11 ? uint32_t {0} : uint32_t {1}));
        }

        if (scenario == 3 || scenario == 4) {
            REQUIRE(critter->GetHex() == control->TransformedHex);
            REQUIRE(control->Pre.load() == 1);
            REQUIRE(control->Post.load() == 1);
        }
        if (scenario == 12 || scenario == 13 || scenario == 14) {
            REQUIRE(critter->GetHex() == initial_hex);
            REQUIRE(control->Post.load() == 0);
        }
        if (scenario == 12) {
            REQUIRE(authority_calls == 1);
        }
        if (scenario >= 22 && scenario <= 25) {
            REQUIRE(authority_calls == 1);
            REQUIRE(control->AuthorityConsistent.load() == 1);
        }
        if (scenario == 25) {
            REQUIRE(critter->IsMoving());
            REQUIRE(critter->GetHex() == initial_hex);
            REQUIRE(control->Post.load() == 0);
        }
        if (scenario == 31) {
            REQUIRE(authority_calls == 1);
            REQUIRE(control->AliasMutations.load() == 1);
            REQUIRE_FALSE(control->BorrowedRawData);
            REQUIRE(critter->GetHex() == requested);
            REQUIRE(source->IsCritterOnHex(requested, critter));
            REQUIRE_FALSE(source->IsCritterOnHex(mpos {25, 26}, critter));
            REQUIRE(control->Pre.load() == 1);
            REQUIRE(control->Post.load() == 1);
            logging::write("MOVEMENT_COMMIT_ALIAS mutations=1 frozen_hex={},{} raw_hex={},{} third_field=false borrow_cleared=true", requested.x, requested.y, critter->GetHex().x, critter->GetHex().y);
        }
    }

    bool destroyed {scenario == 5 || scenario == 6 || scenario == 9 || scenario == 18 || scenario == 19 || scenario == 20 || scenario == 21 || scenario == 22 || scenario == 29 || scenario == 30};
    bool map_destroyed {scenario == 18 || scenario == 19 || scenario == 20};
    bool relocated {scenario == 7 || scenario == 8 || scenario == 10 || scenario == 16 || scenario == 17 || scenario == 23 || scenario == 24 || scenario == 26};

    if (destroyed) {
        REQUIRE_FALSE(server->EntityMngr.GetCritter(critter_id));
        REQUIRE_FALSE(destination->GetCritter(critter_id));

        if (map_destroyed) {
            REQUIRE_FALSE(server->EntityMngr.GetMap(source_id));
        }
        else {
            REQUIRE_FALSE(source->GetCritter(critter_id));
            REQUIRE_FALSE(source->IsCritterOnHex(initial_hex, CritterFindType::Any));
            REQUIRE_FALSE(source->IsCritterOnHex(requested, CritterFindType::Any));
        }
    }
    else {
        REQUIRE_FALSE(critter->IsDestroyed());
        auto owner {critter->GetParent<Map>()};
        REQUIRE(owner);
        REQUIRE(owner->GetCritter(critter_id) == critter);
        REQUIRE(owner->IsCritterOnHex(critter->GetHex(), critter));
        REQUIRE(owner->IsCritterOnHex(critter->GetHex(), CritterFindType::Any));
        REQUIRE(critter->GetMapId() == owner->GetId());
        REQUIRE(owner->GetId() == (relocated ? control->DestinationMapId : source_id));

        if (scenario != 11) {
            if (critter->GetHex() != initial_hex) {
                REQUIRE_FALSE(source->IsCritterOnHex(initial_hex, critter));
            }
            if (critter->GetHex() != requested) {
                REQUIRE_FALSE(source->IsCritterOnHex(requested, critter));
            }
        }

        if (relocated) {
            REQUIRE(owner->GetId() == control->DestinationMapId);
            REQUIRE(critter->GetHex() == mpos {40, 40});
            REQUIRE_FALSE(source->IsCritterOnHex(initial_hex, critter));
            REQUIRE_FALSE(source->IsCritterOnHex(requested, critter));
        }
        if (scenario == 11) {
            REQUIRE(critter->GetHex() == requested);

            for (int16_t y {0}; y < 8; y++) {
                for (int16_t x {0}; x < 8; x++) {
                    mpos hex {x, y};
                    bool expected {GeometryHelper::GetDistance(hex, requested) <= 2};
                    REQUIRE(source->IsCritterOnHex(hex, critter) == expected);
                    REQUIRE(source->IsCritterOnHex(hex, CritterFindType::Any) == expected);
                }
            }
        }
    }

    if (control->Actions.load() != 0 && scenario < 22) {
        if (scenario == 2 || scenario == 6 || scenario == 8 || scenario == 17 || scenario == 19 || scenario == 21) {
            REQUIRE(control->PostConsistent.load() >= 1);
        }
        else {
            REQUIRE(control->PreConsistent.load() == 1);
        }
    }

    REQUIRE(control->UnexpectedReports.load() == 0);
    logging::write("MOVEMENT_COMMIT_RESULT scenario={} grid={} pre={} post={} pre_consistent={} post_consistent={} authority_consistent={} actions={} reports={} destroyed={} relocated={} map_destroyed={}", scenario, grid_name, control->Pre.load(), control->Post.load(), control->PreConsistent.load(), control->PostConsistent.load(), control->AuthorityConsistent.load(), control->Actions.load(), control->Reports.load(), destroyed, relocated, map_destroyed);
    cleanup_fixture();
    cleanup.release();
    REQUIRE_FALSE(repaired);
    REQUIRE_FALSE(server->EntityMngr.GetCritter(critter_id));
    REQUIRE_FALSE(server->EntityMngr.GetMap(source_id));
    REQUIRE_FALSE(server->EntityMngr.GetMap(destination_id));
    REQUIRE(control->UnexpectedReports.load() == 0);
    logging::write("MOVEMENT_COMMIT_CLEANUP scenario={} repaired=false critter_absent=true source_absent=true destination_absent=true", scenario);
}

FO_END_NAMESPACE
