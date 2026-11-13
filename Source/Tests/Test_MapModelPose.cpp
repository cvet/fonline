// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "MapView.h"
#include "Test_ClientEngineHelpers.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

using namespace ClientEngineTests;

bool Client_Critter_GetBoneSpriteMapAnchor(ptr<CritterView> self, hstring boneName, float32_t moveX, float32_t moveY, float32_t moveZ, ipos32& boneOffset, float32_t& depthOffset);
void Client_Map_DrawMapSprite(ptr<MapView> self, ptr<MapSpriteHolder> mapSpr);

TEST_CASE("MapModelBoneAnchorMatchesThePoseSubmittedAfterBeforeSprites")
{
    int32_t visibility_mode {GENERATE(0, 1, 2, 3)};
    CAPTURE(visibility_mode);
    bool replacement_case {visibility_mode == 3};

    constexpr string_view mesh_path {"Models/BoneAnchor.fbx"};
    constexpr string_view model_path {"Models/RuntimeInstance.fo3d"};
    constexpr string_view replacement_path {"Models/ReplacementInstance.fo3d"};
    constexpr string_view description {"Model BoneAnchor.fbx\nScale 1.25\nAnim 1 1 BoneAnchor.fbx Base\n"};
    constexpr string_view replacement_description {"Model BoneAnchor.fbx\nScale 2.0\nAnim 1 1 BoneAnchor.fbx Base\n"};
    auto mesh {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> resources {};
    auto animation_info {MakeUnitTestModelAnimationInfo(model_path)};
    auto replacement_animation_info {MakeUnitTestModelAnimationInfo(replacement_path)};
    animation_info.insert(animation_info.end(), replacement_animation_info.begin(), replacement_animation_info.end());
    resources.emplace_back("ModelAnimationInfo.foinfo", std::move(animation_info));
    resources.emplace_back(string {mesh_path}, mesh);
    resources.emplace_back(string {model_path}, MakeRuntimeModelDescription(model_path, mesh_path, mesh, {}, {}, nullptr, false, {}, 0.0f, nullptr, description));
    resources.emplace_back(string {replacement_path}, MakeRuntimeModelDescription(replacement_path, mesh_path, mesh, {}, {}, nullptr, false, {}, 0.0f, nullptr, replacement_description));
    resources.emplace_back("AnchorSprite.png", BakerTests::MakeMinimalBakedSprite(8, 8));

    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.ModelDirectDraw, true);
    BakerTests::OverrideSetting(settings.Render.ModelProjFactor, 32.0f);
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    ProtoMap proto {client->Hashes.to_hashed_string("BoneAnchorMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr)};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    map->EnableMapperMode();
    map->InstantScrollTo(mpos {8, 8});

    auto cr {map->AddMapperCritter(client->Hashes.to_hashed_string("UnitTestModelCritter"), mpos {8, 8}, mdir {int16_t {180}}, nullptr)};
    REQUIRE(cr->IsMapSpriteValid());
    cr->RestoreAlpha();

    auto model {cr->GetModel()};
    REQUIRE(model);
    model->SetDir(mdir {int16_t {180}}, false);

    client->SprMngr.BeginScene();
    map->DrawMap();
    client->SprMngr.EndScene();

    if (visibility_mode == 1) {
        cr->GetMapSprite()->SetHidden(true);
    }
    else if (visibility_mode == 2) {
        cr->SetHexOffset(ipos16 {1500, 0});
        cr->RefreshOffs();
    }

    hstring bone {client->Hashes.to_hashed_string(mesh_path)};
    auto local {vec3 {4.0f, 2.0f, 1.0f}};
    MapSpriteHolder holder {};
    holder.SprId = client->AnimLoad(client->Hashes.to_hashed_string("AnchorSprite.png"), AtlasType::MapSprites);
    REQUIRE(holder.SprId != 0);
    holder.Hex = cr->GetHex();
    auto stop_holder {scope_exit([&holder]() noexcept { holder.StopDraw(); })};
    int32_t submitted_count {0};
    float32_t submitted_angle {0.0f};
    float32_t angle_before_turn {model->GetMoveDirAngle()};
    uintptr_t original_model_identity {model.as_uintptr()};
    uintptr_t submitted_model_identity {0};
    int32_t replacement_callbacks {0};
    hstring replacement_name {client->Hashes.to_hashed_string(replacement_path)};
    hstring callback_source_name {};
    optional<ipos32> original_model_anchor {};

    Entity::EventCallbackData callback {};
    callback.Callback = [&](FuncCallData& call) {
        CHECK(call.ArgsData.size() == 2);

        if (call.ArgsData.size() != 2) {
            return Entity::EventResult::StopChain;
        }

        irect32 draw_area {*cast_from_void<const irect32*>(call.ArgsData[1].get())};
        auto visible_rect {client->SprMngr.GetMapSpriteDrawRect(cr->GetMapSprite(), draw_area)};
        CHECK(visible_rect.has_value() == (visibility_mode == 0 || replacement_case));

        if (visibility_mode == 1 || visibility_mode == 2) {
            submitted_count++;
            return Entity::EventResult::ContinueChain;
        }

        auto current_model {cr->GetModel()};
        CHECK(static_cast<bool>(current_model));

        if (!current_model) {
            return Entity::EventResult::StopChain;
        }

        if (replacement_case) {
            CHECK(replacement_callbacks == 1);
            CHECK(callback_source_name == client->Hashes.to_hashed_string(model_path));
            CHECK(cr->GetModelName() == replacement_name);
            CHECK(current_model.as_uintptr() != original_model_identity);
            CHECK(static_cast<bool>(current_model->FindBone(bone)));
        }

        ipos32 offset {};
        float32_t depth {0.0f};
        bool projected {Client_Critter_GetBoneSpriteMapAnchor(cr, bone, local.x, local.y, local.z, offset, depth)};
        CHECK(projected);

        if (projected) {
            holder.StopDraw();
            holder.Offset = offset;
            holder.DepthOffset = depth;
            Client_Map_DrawMapSprite(map, &holder);
            submitted_angle = current_model->GetMoveDirAngle();
            submitted_model_identity = current_model.as_uintptr();
            submitted_count++;
        }

        return Entity::EventResult::ContinueChain;
    };
    callback.SubscriptionPtr = reinterpret_cast<uintptr_t>(&holder);
    client->OnRenderMap_BeforeSprites.Subscribe(std::move(callback));
    auto unsubscribe {scope_exit([&client, &holder]() noexcept { client->OnRenderMap_BeforeSprites.Unsubscribe(reinterpret_cast<uintptr_t>(&holder)); })};
    if (replacement_case) {
        auto state {static_cast<CritterStateAnim>(1)};
        auto action {static_cast<CritterActionAnim>(1)};
        REQUIRE(model->HasAnimation(state, action));

        client->GameTime.FrameAdvance(false);
        model->PoseSpriteFrame(true);

        model->AddAnimationCallback(ModelAnimationCallback {
            .StateAnim = state,
            .ActionAnim = action,
            .NormalizedTime = 0.5f,
            .Callback =
                [&cr, &replacement_callbacks, &callback_source_name, replacement_name] {
                    replacement_callbacks++;
                    callback_source_name = cr->GetModelName();
                    cr->SetModelName(replacement_name);
                },
        });

        model->SetSpeed(10.0f);
        (void)model->PlayAnim(state, action, nullptr, 0.0f, combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoSmooth));

        // Consume start-up redraw before the timed interval, so replacement occurs in map preparation
        model->PoseSpriteFrame(true);
        REQUIRE(replacement_callbacks == 0);
        REQUIRE_FALSE(model->NeedForceDraw());

        ipos32 offset {};
        float32_t depth {0.0f};
        REQUIRE(Client_Critter_GetBoneSpriteMapAnchor(cr, bone, local.x, local.y, local.z, offset, depth));
        original_model_anchor = offset;
    }
    else {
        model->SetDir(mdir {int16_t {90}}, true);
    }

    optional<ipos32> first_anchor {};

    for (int32_t frame {0}; frame < 2; frame++) {
        coarse_sleep(std::chrono::milliseconds {replacement_case && frame == 0 ? 200 : 50});
        client->GameTime.FrameAdvance(false);
        int32_t before_count {submitted_count};

        client->SprMngr.BeginScene();
        int32_t callbacks_before_map {replacement_callbacks};
        map->DrawMap();

        ipos32 drawn_offset {};
        float32_t drawn_depth {0.0f};
        auto current_model {cr->GetModel()};
        bool projected {Client_Critter_GetBoneSpriteMapAnchor(cr, bone, local.x, local.y, local.z, drawn_offset, drawn_depth)};
        float32_t drawn_angle {current_model ? current_model->GetMoveDirAngle() : 0.0f};
        client->SprMngr.EndScene();

        CAPTURE(frame, submitted_count, holder.Offset.x, holder.Offset.y, drawn_offset.x, drawn_offset.y, submitted_angle, drawn_angle, replacement_callbacks);
        REQUIRE(submitted_count == before_count + 1);
        REQUIRE(current_model);

        if (visibility_mode == 1 || visibility_mode == 2) {
            CHECK(drawn_angle == angle_before_turn);
            continue;
        }

        REQUIRE(projected);
        CHECK(holder.Valid);
        CHECK(submitted_model_identity == current_model.as_uintptr());
        CHECK(holder.Offset == drawn_offset);
        CHECK(is_float_equal(holder.DepthOffset, drawn_depth));
        CHECK(drawn_angle == submitted_angle);

        if (replacement_case) {
            CHECK(callbacks_before_map == (frame == 0 ? 0 : 1));
            REQUIRE(replacement_callbacks == 1);
            REQUIRE(original_model_anchor);
            CHECK(cr->GetModelName() == replacement_name);
            CHECK(current_model.as_uintptr() != original_model_identity);
            CHECK(drawn_offset != original_model_anchor.value());
            CHECK(static_cast<bool>(current_model->FindBone(bone)));
        }
        else if (first_anchor) {
            CHECK(drawn_offset != first_anchor.value());
        }
        else {
            first_anchor = drawn_offset;
        }
    }
}

#endif

FO_END_NAMESPACE
