// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Test_ClientEngineHelpers.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

using namespace ClientEngineTests;

class LayoutFaultProbeRender final : public IAppRender
{
public:
    explicit LayoutFaultProbeRender(ptr<IAppRender> forward) :
        _forward {forward}
    {
    }

    auto GetRenderTarget() -> nptr<RenderTexture> override { return _forward->GetRenderTarget(); }
    auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override { return _forward->CreateTexture(size, linear_filtered, with_depth); }
    auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override { return _forward->CreateDrawBuffer(is_static); }
    auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override { return _forward->CreateEffect(usage, name, loader); }
    auto IsRenderTargetFlipped() const -> bool override { return _forward->IsRenderTargetFlipped(); }
    auto GetProjMatrix() const -> mat44 override { return _forward->GetProjMatrix(); }
    void SetRenderTarget(nptr<RenderTexture> tex) override { _forward->SetRenderTarget(tex); }
    void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override { _forward->SetOrthoDepthRange(nearp, farp); }
    void ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil) override { _forward->ClearRenderTarget(color, depth, stencil); }
    void EnableScissor(irect32 rect) override { _forward->EnableScissor(rect); }
    void DisableScissor() override { _forward->DisableScissor(); }

    auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override
    {
        ProjectionCalls++;

        if (FailNextProjection) {
            FailNextProjection = false;
            InjectedThrows++;
            throw InvalidCallException("Controlled layout projection failure");
        }

        return _forward->CreateOrthoMatrix(left, right, bottom, top, nearp, farp);
    }

    mutable bool FailNextProjection {};
    mutable int32_t ProjectionCalls {};
    mutable int32_t InjectedThrows {};

private:
    ptr<IAppRender> _forward;
};

TEST_CASE("ModelSetupFrameProjectionFailurePreservesCommittedLayout")
{
    REQUIRE(IsNonInteractiveMode());

    constexpr string_view mesh_path {"Models/LayoutProbe.fbx"};
    constexpr string_view model_path {"Models/RuntimeInstance.fo3d"};
    auto mesh {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    resources.emplace_back(string {mesh_path}, mesh);
    resources.emplace_back(string {model_path}, MakeRuntimeModelDescription(model_path, mesh_path, mesh));
    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.ModelProjFactor, 32.0f);
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    LayoutFaultProbeRender renderer {&client->SprMngr.GetRender()};
    ModelManager manager {&settings, &client->Resources, client, &client->EffectMngr, &renderer, &client->GameTime, client,
        [](string_view path) {
            ignore_unused(path);
            return pair<nptr<RenderTexture>, frect32> {};
        },
        [] { return false; }};
    auto model {manager.CreateModel(model_path)};
    auto control {manager.CreateModel(model_path)};
    REQUIRE(model);
    REQUIRE(control);
    model->PrepareFrameLayout();
    control->PrepareFrameLayout();

    model->SetupFrame({64, 96}, {10, 23});
    control->SetupFrame({64, 96}, {10, 23});
    model->PoseSpriteFrame(false);
    control->PoseSpriteFrame(false);

    isize32 old_size {model->GetDrawSize()};
    ipos32 old_pivot {model->GetFramePivot()};
    irect32 old_rect {model->GetDrawRect()};
    isize32 old_lighting {model->GetLightingSize()};
    ipos32 old_projected {model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f})};
    vec3 old_unprojected {model->Convert2dTo3d({5, 7})};
    REQUIRE(old_size == isize32 {64, 96});
    REQUIRE(old_pivot == ipos32 {10, 23});
    REQUIRE(old_projected == ipos32 {16, 32});

    control->SetupFrame({128, 80}, {-7, 91});
    control->PoseSpriteFrame(false);
    REQUIRE(control->GetDrawSize() == isize32 {128, 80});
    REQUIRE(control->GetFramePivot() == ipos32 {-7, 91});
    REQUIRE(control->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == ipos32 {16, 32});

    int32_t calls_before_fault {renderer.ProjectionCalls};
    renderer.FailNextProjection = true;
    bool saw_injected_failure {false};

    try {
        model->SetupFrame({128, 80}, {-7, 91});
    }
    catch (const InvalidCallException& ex) {
        saw_injected_failure = string_view {ex.what()}.find("Controlled layout projection failure") != string_view::npos;
    }

    REQUIRE(saw_injected_failure);
    REQUIRE(renderer.InjectedThrows == 1);
    REQUIRE(renderer.ProjectionCalls == calls_before_fault + 1);
    REQUIRE_FALSE(renderer.FailNextProjection);

    CHECK(model->GetDrawSize() == old_size);
    CHECK(model->GetFramePivot() == old_pivot);
    CHECK(model->GetDrawRect() == old_rect);
    CHECK(model->GetLightingSize() == old_lighting);
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == old_projected);
    vec3 failed_unprojected {model->Convert2dTo3d({5, 7})};
    CHECK(is_float_equal(failed_unprojected.x, old_unprojected.x));
    CHECK(is_float_equal(failed_unprojected.y, old_unprojected.y));
    CHECK(is_float_equal(failed_unprojected.z, old_unprojected.z));

    int32_t calls_before_retry {renderer.ProjectionCalls};
    model->SetupFrame({128, 80}, {-7, 91});
    CHECK(renderer.ProjectionCalls == calls_before_retry + 1);
    model->PoseSpriteFrame(false);

    CHECK(model->GetDrawSize() == control->GetDrawSize());
    CHECK(model->GetFramePivot() == ipos32 {-7, 91});
    CHECK(model->GetFramePivot() == control->GetFramePivot());
    CHECK(model->GetDrawRect() == control->GetDrawRect());
    CHECK(model->GetLightingSize() == control->GetLightingSize());
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == ipos32 {16, 32});
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == control->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}));
    vec3 retry_unprojected {model->Convert2dTo3d({5, 7})};
    vec3 expected_unprojected {control->Convert2dTo3d({5, 7})};
    CHECK(is_float_equal(retry_unprojected.x, expected_unprojected.x));
    CHECK(is_float_equal(retry_unprojected.y, expected_unprojected.y));
    CHECK(is_float_equal(retry_unprojected.z, expected_unprojected.z));
    hstring bone {client->Hashes.to_hashed_string(mesh_path)};
    auto actual_bone {model->GetBoneSpritePos(bone)};
    auto expected_bone {control->GetBoneSpritePos(bone)};
    REQUIRE(actual_bone.has_value());
    REQUIRE(expected_bone.has_value());
    CHECK(actual_bone == expected_bone);
}

TEST_CASE("ModelScaleRefreshFailureRequiresOrdinaryPrepareRetry")
{
    REQUIRE(IsNonInteractiveMode());

    constexpr string_view mesh_path {"Models/LayoutProbe.fbx"};
    constexpr string_view model_path {"Models/RuntimeInstance.fo3d"};
    auto mesh {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    resources.emplace_back(string {mesh_path}, mesh);
    resources.emplace_back(string {model_path}, MakeRuntimeModelDescription(model_path, mesh_path, mesh));
    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.ModelProjFactor, 32.0f);
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    LayoutFaultProbeRender renderer {&client->SprMngr.GetRender()};
    ModelManager manager {&settings, &client->Resources, client, &client->EffectMngr, &renderer, &client->GameTime, client,
        [](string_view path) {
            ignore_unused(path);
            return pair<nptr<RenderTexture>, frect32> {};
        },
        [] { return false; }};
    auto model {manager.CreateModel(model_path)};
    auto control {manager.CreateModel(model_path)};
    REQUIRE(model);
    REQUIRE(control);
    model->PrepareFrameLayout();
    control->PrepareFrameLayout();

    model->SetupFrame({64, 96}, {10, 23});
    control->SetupFrame({64, 96}, {10, 23});
    model->PoseSpriteFrame(false);
    control->PoseSpriteFrame(false);

    isize32 old_size {model->GetDrawSize()};
    ipos32 old_pivot {model->GetFramePivot()};
    irect32 old_rect {model->GetDrawRect()};
    isize32 old_lighting {model->GetLightingSize()};
    ipos32 old_projected {model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f})};
    vec3 old_unprojected {model->Convert2dTo3d({5, 7})};

    control->SetScale(2.0f, 3.0f, 1.5f);
    control->PoseSpriteFrame(false);
    REQUIRE(control->GetDrawSize() != old_size);
    REQUIRE(control->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == ipos32 {16, 32});

    int32_t calls_before_fault {renderer.ProjectionCalls};
    renderer.FailNextProjection = true;
    bool saw_injected_failure {false};

    try {
        model->SetScale(2.0f, 3.0f, 1.5f);
    }
    catch (const InvalidCallException& ex) {
        saw_injected_failure = string_view {ex.what()}.find("Controlled layout projection failure") != string_view::npos;
    }

    REQUIRE(saw_injected_failure);
    REQUIRE(renderer.InjectedThrows == 1);
    REQUIRE(renderer.ProjectionCalls == calls_before_fault + 1);
    REQUIRE_FALSE(renderer.FailNextProjection);

    CHECK(model->GetDrawSize() == old_size);
    CHECK(model->GetFramePivot() == old_pivot);
    CHECK(model->GetDrawRect() == old_rect);
    CHECK(model->GetLightingSize() == old_lighting);
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == old_projected);
    vec3 failed_unprojected {model->Convert2dTo3d({5, 7})};
    CHECK(is_float_equal(failed_unprojected.x, old_unprojected.x));
    CHECK(is_float_equal(failed_unprojected.y, old_unprojected.y));
    CHECK(is_float_equal(failed_unprojected.z, old_unprojected.z));

    int32_t calls_before_retry {renderer.ProjectionCalls};
    model->PrepareFrameLayout();
    CHECK(renderer.ProjectionCalls == calls_before_retry + 1);
    model->PoseSpriteFrame(false);

    CHECK(model->GetDrawSize() == control->GetDrawSize());
    CHECK(model->GetFramePivot() == control->GetFramePivot());
    CHECK(model->GetDrawRect() == control->GetDrawRect());
    CHECK(model->GetLightingSize() == control->GetLightingSize());
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == ipos32 {16, 32});
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == control->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}));
    vec3 retry_unprojected {model->Convert2dTo3d({5, 7})};
    vec3 expected_unprojected {control->Convert2dTo3d({5, 7})};
    CHECK(is_float_equal(retry_unprojected.x, expected_unprojected.x));
    CHECK(is_float_equal(retry_unprojected.y, expected_unprojected.y));
    CHECK(is_float_equal(retry_unprojected.z, expected_unprojected.z));
    hstring bone {client->Hashes.to_hashed_string(mesh_path)};
    auto actual_bone {model->GetBoneSpritePos(bone)};
    auto expected_bone {control->GetBoneSpritePos(bone)};
    REQUIRE(actual_bone.has_value());
    REQUIRE(expected_bone.has_value());
    CHECK(actual_bone == expected_bone);
}

TEST_CASE("ModelSetupFrameSupportsRepeatedSizesAndSignedPivots")
{
    REQUIRE(IsNonInteractiveMode());

    constexpr string_view mesh_path {"Models/LayoutProbe.fbx"};
    constexpr string_view model_path {"Models/RuntimeInstance.fo3d"};
    auto mesh {MakeRuntimeModelTriangleMesh()};
    vector<pair<string, vector<uint8_t>>> resources {};
    resources.emplace_back("ModelAnimationInfo.foinfo", MakeUnitTestModelAnimationInfo(model_path));
    resources.emplace_back(string {mesh_path}, mesh);
    resources.emplace_back(string {model_path}, MakeRuntimeModelDescription(model_path, mesh_path, mesh));
    auto settings {MakeClientTestSettings()};
    BakerTests::OverrideSetting(settings.Render.ModelProjFactor, 32.0f);
    auto client {MakeClientEngine(settings, MakeClientTestResources(std::move(resources)))};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};

    LayoutFaultProbeRender renderer {&client->SprMngr.GetRender()};
    ModelManager manager {&settings, &client->Resources, client, &client->EffectMngr, &renderer, &client->GameTime, client,
        [](string_view path) {
            ignore_unused(path);
            return pair<nptr<RenderTexture>, frect32> {};
        },
        [] { return false; }};
    auto model {manager.CreateModel(model_path)};
    auto control {manager.CreateModel(model_path)};
    REQUIRE(model);
    REQUIRE(control);
    model->PrepareFrameLayout();
    control->PrepareFrameLayout();

    model->SetupFrame({64, 96}, {10, 23});
    control->SetupFrame({64, 96}, {10, 23});
    model->PoseSpriteFrame(false);
    control->PoseSpriteFrame(false);

    int32_t calls_before_success {renderer.ProjectionCalls};
    model->SetupFrame({128, 80}, {-7, 91});
    CHECK(renderer.ProjectionCalls == calls_before_success + 1);
    control->SetupFrame({128, 80}, {-7, 91});
    model->PoseSpriteFrame(false);
    control->PoseSpriteFrame(false);

    CHECK(model->GetDrawSize() == isize32 {128, 80});
    CHECK(model->GetFramePivot() == ipos32 {-7, 91});
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == ipos32 {16, 32});
    CHECK(model->GetDrawSize() == control->GetDrawSize());
    CHECK(model->GetFramePivot() == control->GetFramePivot());
    CHECK(model->GetDrawRect() == control->GetDrawRect());
    CHECK(model->GetLightingSize() == control->GetLightingSize());

    int32_t calls_before_repeat {renderer.ProjectionCalls};
    model->SetupFrame({128, 80}, {-9, 95});
    CHECK(renderer.ProjectionCalls == calls_before_repeat + 1);
    control->SetupFrame({128, 80}, {-9, 95});
    model->PoseSpriteFrame(false);
    control->PoseSpriteFrame(false);

    CHECK(model->GetDrawSize() == isize32 {128, 80});
    CHECK(model->GetFramePivot() == ipos32 {-9, 95});
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == ipos32 {16, 32});
    CHECK(model->GetDrawSize() == control->GetDrawSize());
    CHECK(model->GetFramePivot() == control->GetFramePivot());
    CHECK(model->GetDrawRect() == control->GetDrawRect());
    CHECK(model->GetLightingSize() == control->GetLightingSize());
    vec3 repeated_unprojected {model->Convert2dTo3d({5, 7})};
    vec3 expected_unprojected {control->Convert2dTo3d({5, 7})};
    CHECK(is_float_equal(repeated_unprojected.x, expected_unprojected.x));
    CHECK(is_float_equal(repeated_unprojected.y, expected_unprojected.y));
    CHECK(is_float_equal(repeated_unprojected.z, expected_unprojected.z));

    model->SetScale(1.0f, 1.0f, 1.0f);
    control->SetScale(1.0f, 1.0f, 1.0f);
    model->PoseSpriteFrame(false);
    control->PoseSpriteFrame(false);
    REQUIRE(model->GetDrawSize() == control->GetDrawSize());
    isize32 same_size {model->GetDrawSize()};
    ipos32 same_pivot {model->GetFramePivot()};
    ipos32 same_projected {model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f})};
    vec3 same_unprojected {model->Convert2dTo3d({5, 7})};
    int32_t calls_before_same_size_refresh {renderer.ProjectionCalls};

    model->SetScale(1.0f, 1.0f, 1.0f);
    CHECK(renderer.ProjectionCalls == calls_before_same_size_refresh);
    CHECK(model->GetDrawSize() == same_size);
    CHECK(model->GetFramePivot() == same_pivot);
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == same_projected);
    vec3 after_same_size_unprojected {model->Convert2dTo3d({5, 7})};
    CHECK(is_float_equal(after_same_size_unprojected.x, same_unprojected.x));
    CHECK(is_float_equal(after_same_size_unprojected.y, same_unprojected.y));
    CHECK(is_float_equal(after_same_size_unprojected.z, same_unprojected.z));

    int32_t max_draw_width {AppRender::MAX_ATLAS_WIDTH / ModelInstance::FRAME_SCALE};
    model->SetupFrame({max_draw_width, 4}, {0, 0});
    CHECK(model->GetDrawSize() == isize32 {max_draw_width, 4});
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == ipos32 {16, 32});
    int32_t max_draw_height {AppRender::MAX_ATLAS_HEIGHT / ModelInstance::FRAME_SCALE};
    model->SetupFrame({4, max_draw_height}, {0, 0});
    CHECK(model->GetDrawSize() == isize32 {4, max_draw_height});
    CHECK(model->Convert3dTo2d(vec3 {1.0f, 2.0f, 0.0f}) == ipos32 {16, 32});
    CHECK(renderer.InjectedThrows == 0);
    CHECK_FALSE(renderer.FailNextProjection);
}

#endif

FO_END_NAMESPACE
