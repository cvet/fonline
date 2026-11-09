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

#include "Common.h"

#include "Test_ClientEngineHelpers.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D

using namespace ClientEngineTests;

struct GraphProbeDraw
{
    size_t RequestedVertices {};
    size_t RequestedIndices {};
    size_t UploadedVertices {};
    size_t UploadedIndices {};
    size_t UploadGeneration {};
    bool Coherent {};
};

class GraphProbeBuffer final : public RenderDrawBuffer
{
public:
    GraphProbeBuffer(bool is_static, unique_ptr<RenderDrawBuffer> forward) :
        RenderDrawBuffer(is_static),
        _forward {std::move(forward)}
    {
    }

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override
    {
        UploadCalls++;
        bool skip = IsStatic && !StaticDataChanged;
        _forward->Vertices = Vertices;
        _forward->Vertices3D = Vertices3D;
        _forward->Indices = Indices;
        _forward->VertCount = VertCount;
        _forward->IndCount = IndCount;
        _forward->StaticDataChanged = StaticDataChanged;
        _forward->PrimType = PrimType;
        _forward->Upload(usage, custom_vertices_size, custom_indices_size);

        if (!skip) {
            UploadedVertices = custom_vertices_size.value_or(VertCount);
            UploadedIndices = custom_indices_size.value_or(IndCount);
            UploadGeneration++;
        }

        StaticDataChanged = _forward->StaticDataChanged;
    }

    size_t UploadCalls {};
    size_t UploadedVertices {};
    size_t UploadedIndices {};
    size_t UploadGeneration {};

private:
    unique_ptr<RenderDrawBuffer> _forward;
};

class GraphProbeEffect final : public RenderEffect
{
public:
    GraphProbeEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, unique_ptr<RenderEffect> forward, ptr<vector<GraphProbeDraw>> draws) :
        RenderEffect(usage, name, loader),
        _forward {std::move(forward)},
        _draws {draws}
    {
    }

    void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override
    {
        auto buffer = dbuf.dyn_cast<GraphProbeBuffer>();
        FO_VERIFY_AND_THROW(buffer, "Graph probe draw has an unexpected buffer type");
        FO_VERIFY_AND_THROW(start_index <= dbuf->IndCount, "Graph probe draw starts outside CPU indices");
        size_t requested_indices = indices_to_draw.value_or(dbuf->IndCount - start_index);
        bool coherent = dbuf->VertCount <= buffer->UploadedVertices && start_index <= buffer->UploadedIndices && requested_indices <= buffer->UploadedIndices - start_index;
        _draws->emplace_back(GraphProbeDraw {dbuf->VertCount, requested_indices, buffer->UploadedVertices, buffer->UploadedIndices, buffer->UploadGeneration, coherent});
        FO_VERIFY_AND_THROW(coherent, "Graph probe draw exceeds the retained static upload");
        _forward->DrawBuffer(dbuf, start_index, indices_to_draw, custom_tex);
    }

private:
    unique_ptr<RenderEffect> _forward;
    ptr<vector<GraphProbeDraw>> _draws;
};

class GraphProbeRender final : public IAppRender
{
public:
    explicit GraphProbeRender(ptr<IAppRender> forward) :
        _forward {forward}
    {
    }

    auto GetRenderTarget() -> nptr<RenderTexture> override { return _forward->GetRenderTarget(); }
    auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override { return _forward->CreateTexture(size, linear_filtered, with_depth); }
    auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override { return safe_alloc::make_unique<GraphProbeBuffer>(is_static, _forward->CreateDrawBuffer(is_static)); }
    auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override { return safe_alloc::make_unique<GraphProbeEffect>(usage, name, loader, _forward->CreateEffect(usage, name, loader), &Draws); }
    auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override { return _forward->CreateOrthoMatrix(left, right, bottom, top, nearp, farp); }
    auto IsRenderTargetFlipped() const -> bool override { return _forward->IsRenderTargetFlipped(); }
    auto GetProjMatrix() const -> mat44 override { return _forward->GetProjMatrix(); }
    void SetRenderTarget(nptr<RenderTexture> tex) override { _forward->SetRenderTarget(tex); }
    void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override { _forward->SetOrthoDepthRange(nearp, farp); }
    void ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil) override { _forward->ClearRenderTarget(color, depth, stencil); }
    void EnableScissor(irect32 rect) override { _forward->EnableScissor(rect); }
    void DisableScissor() override { _forward->DisableScissor(); }

    vector<GraphProbeDraw> Draws;

private:
    ptr<IAppRender> _forward;
};

static auto MakeGraphProbeRootTriangle() -> vector<uint8_t>
{
    vector<uint8_t> triangle = MakeRuntimeModelTriangleMesh();
    data_reader reader {triangle};
    ModelMeshData data = ReadModelMeshData(reader, "GraphProbeTriangle");
    FO_VERIFY_AND_THROW(data.RootBone && data.RootBone->AttachedMesh, "Graph probe triangle has no geometry");
    auto limb = safe_alloc::make_unique<ModelMeshBoneData>();
    limb->Name = "Limb";
    limb->TransformationMatrix = mat44 {1.0f};
    limb->GlobalTransformationMatrix = mat44 {1.0f};
    data.RootBone->Children.emplace_back(std::move(limb));
    data.RootBone->AttachedMesh->SkinBoneNames = {"Root", "Limb"};
    data.RootBone->AttachedMesh->SkinBoneOffsets = {mat44 {1.0f}, mat44 {1.0f}};
    vector<uint8_t> result;
    data_writer writer {result};
    WriteModelMeshData(writer, data, "GraphProbeTriangle");
    return result;
}

static auto MakeGraphProbeModelResources() -> vector<pair<string, vector<uint8_t>>>
{
    constexpr string_view model_path = "Models/RuntimeInstance.fo3d";
    constexpr string_view mesh_path = "Models/GraphBody.fbx";
    constexpr string_view extra_mesh_path = "Models/GraphExtra.fbx";
    constexpr string_view late_mesh_path = "Models/GraphLate.fbx";
    vector<pair<string, vector<uint8_t>>> resources;
    resources.emplace_back(string {mesh_path}, MakeGraphProbeRootTriangle());
    resources.emplace_back(string {extra_mesh_path}, MakeRuntimeModelTriangleMesh());
    resources.emplace_back(string {late_mesh_path}, MakeRuntimeModelTriangleMesh(vec3 {}, "Late.png"));
    BakerTests::TestRig rig;
    rig.AddSourceFile(model_path, "Model GraphBody.fbx\nAnim 1 1 ModelFile Base\nLayer 3\nValue 1 Attach GraphExtra.fbx Link Limb\nValue 1 Attach GraphLate.fbx Link Limb\n", 1);

    for (const auto& [path, data] : resources) {
        rig.AddSourceFile(path, "model source fixture", 1);
        rig.AddBakedFile(path, data, 1);
    }

    BakerTests::AddTestBakedImage(rig, "Models/Late.png", BakerTests::MakeMinimalBakedSprite(8, 8));
    rig.AddBakedFile("Metadata.fometa-client", BakerTests::MakeMetadataBlob({{"Enum", {{"CritterStateAnim", "", "TestState", "1"}, {"CritterActionAnim", "", "TestAction", "1"}}}}));
    ModelInfoBaker info_baker(rig.MakeContext(), [mesh_path](string_view path, const File& file) -> ModelSourceAsset {
        ModelSourceAsset asset;
        asset.FileName = path;
        asset.WriteTime = file.GetWriteTime();
        asset.Skeleton.FileName = path;
        asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Root", .Hierarchy = {"Root"}, .RestLocalTransform = mat44 {1.0f}});

        if (path == mesh_path) {
            asset.Skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = "Limb", .Hierarchy = {"Root", "Limb"}, .RestLocalTransform = mat44 {1.0f}});
            ModelAnimationSource animation;
            animation.FileName = path;
            animation.Name = "Base";
            animation.Duration = 1.0f;

            for (string name : {string {"Root"}, string {"Limb"}}) {
                ModelAnimationJointSource joint;
                joint.OutputName = name;
                joint.Hierarchy = name == "Root" ? vector<string> {"Root"} : vector<string> {"Root", "Limb"};
                joint.Translation.Times = {0.0f, 1.0f};
                joint.Translation.Values = {vec3 {}, vec3 {}};
                joint.Rotation.Times = {0.0f, 1.0f};
                joint.Rotation.Values = {quaternion {1.0f, 0.0f, 0.0f, 0.0f}, quaternion {1.0f, 0.0f, 0.0f, 0.0f}};
                joint.Scale.Times = {0.0f, 1.0f};
                joint.Scale.Values = {vec3 {1.0f}, vec3 {1.0f}};
                animation.Joints.emplace_back(std::move(joint));
            }

            asset.Animations.emplace_back(std::move(animation));
        }

        return asset;
    });
    info_baker.BakeFiles(rig.GetAllSourceFiles(), "");
    REQUIRE(rig.Outputs.count(string {model_path}) == 1);
    resources.emplace_back(string {model_path}, rig.Outputs.at(string {model_path}));
    resources.emplace_back("ModelAnimationInfo.foinfo", rig.Outputs.at("ModelAnimationInfo.foinfo"));
    return resources;
}

TEST_CASE("ModelMeshGenerationStaticUploadControls", "[model-mesh-generation]")
{
    REQUIRE(IsNonInteractiveMode());
    auto settings = MakeClientTestSettings();
    auto client = MakeClientEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    GraphProbeRender render {&client->SprMngr.GetRender()};
    EffectManager effects {&settings, &client->Resources, &render};
    effects.LoadDefaultEffects();
    auto effect = effects.Effects.SkinnedModel;
    REQUIRE(effect);
    auto buffer = render.CreateDrawBuffer(true);
    buffer->Vertices3D.resize(3);
    buffer->Indices = {0, 1, 2};
    buffer->VertCount = 3;
    buffer->IndCount = 3;
    buffer->StaticDataChanged = true;
    buffer->Upload(EffectUsage::Model);
    effect->DrawBuffer(buffer);
    REQUIRE(render.Draws.size() == 1);
    CHECK(render.Draws.back().Coherent);
    CHECK(render.Draws.back().RequestedIndices == 3);
    CHECK(render.Draws.back().UploadedIndices == 3);
    CHECK(render.Draws.back().UploadGeneration == 1);

    buffer->Vertices3D.resize(6);
    buffer->Indices = {0, 1, 2, 3, 4, 5};
    buffer->VertCount = 6;
    buffer->IndCount = 6;
    REQUIRE_FALSE(buffer->StaticDataChanged);
    buffer->Upload(EffectUsage::Model);
    CHECK_THROWS_AS(effect->DrawBuffer(buffer), VerificationException);
    REQUIRE(render.Draws.size() == 2);
    CHECK_FALSE(render.Draws.back().Coherent);
    CHECK(render.Draws.back().RequestedIndices == 6);
    CHECK(render.Draws.back().UploadedIndices == 3);
    CHECK(render.Draws.back().UploadGeneration == 1);

    buffer->StaticDataChanged = true;
    buffer->Upload(EffectUsage::Model);
    effect->DrawBuffer(buffer);
    REQUIRE(render.Draws.size() == 3);
    CHECK(render.Draws.back().Coherent);
    CHECK(render.Draws.back().RequestedIndices == 6);
    CHECK(render.Draws.back().UploadedIndices == 6);
    CHECK(render.Draws.back().UploadGeneration == 2);
    logging::write("GRAPH_RECORDER_CONTROLS records={} initial_upload={} stale_requested={} stale_uploaded={} stale_generation={} restored_upload={} restored_generation={}", render.Draws.size(), render.Draws[0].UploadedIndices, render.Draws[1].RequestedIndices, render.Draws[1].UploadedIndices, render.Draws[1].UploadGeneration, render.Draws[2].UploadedIndices, render.Draws[2].UploadGeneration);
}

TEST_CASE("ModelMeshGenerationFailureDiscardsPartialDrawsAndAllowsRetry", "[model-mesh-generation]")
{
    REQUIRE(IsNonInteractiveMode());
    constexpr string_view model_path = "Models/RuntimeInstance.fo3d";
    auto resources = MakeGraphProbeModelResources();
    auto settings = MakeClientTestSettings();
    BakerTests::OverrideSetting(settings.Render.ModelProjFactor, 32.0f);
    auto client = MakeClientEngine(settings, MakeClientTestResources(std::move(resources)));
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    GraphProbeRender render {&client->SprMngr.GetRender()};
    EffectManager effects {&settings, &client->Resources, &render};
    effects.LoadDefaultEffects();
    auto texture = render.CreateTexture({8, 8}, true, false);
    bool resource_available = false;
    int32_t texture_requests = 0;
    string requested_path;
    ModelManager manager {&settings, &client->Resources, client, &effects, &render, &client->GameTime, client,
        [&](string_view path) -> pair<nptr<RenderTexture>, frect32> {
            texture_requests++;
            requested_path = string {path};

            if (!resource_available) {
                return {};
            }

            return {texture.as_nptr(), frect32 {0.0f, 0.0f, 1.0f, 1.0f}};
        },
        [] { return false; }};
    auto model = manager.CreateModel(model_path);
    REQUIRE(model);
    array<int32_t, MODEL_LAYERS_COUNT> layers {};
    auto state = static_cast<CritterStateAnim>(1);
    auto action = static_cast<CritterActionAnim>(1);
    (void)model->PlayAnim(state, action, layers.data(), 0.0f, ModelAnimFlags::Freeze);
    model->StartMeshGeneration();
    model->PrepareFrameLayout();
    model->PoseSpriteFrame(false);
    model->DrawSpriteFrame();
    REQUIRE(render.Draws.size() == 1);
    REQUIRE(render.Draws.back().Coherent);
    REQUIRE(render.Draws.back().UploadedVertices == 3);
    REQUIRE(render.Draws.back().UploadedIndices == 3);
    REQUIRE(texture_requests == 0);
    size_t baseline_generation = render.Draws.back().UploadGeneration;

    layers[3] = 1;
    bool saw_late_texture_failure = false;

    try {
        (void)model->PlayAnim(state, action, layers.data(), 0.0f, ModelAnimFlags::Freeze);
    }
    catch (const VerificationException& ex) {
        saw_late_texture_failure = string_view {ex.what()}.find("Model texture could not be loaded") != string_view::npos;
    }

    REQUIRE(saw_late_texture_failure);
    REQUIRE(texture_requests == 1);
    REQUIRE(requested_path == "Models/Late.png");
    size_t draws_before_failure = render.Draws.size();
    REQUIRE_NOTHROW(model->PoseSpriteFrame(false));
    REQUIRE_NOTHROW(model->DrawSpriteFrame());
    CHECK(render.Draws.size() == draws_before_failure);

    // A failed rebuild remains retryable through the ordinary mesh-generation entry point
    REQUIRE_THROWS_WITH(model->StartMeshGeneration(), Catch::Matchers::ContainsSubstring("Model texture could not be loaded"));
    CHECK(texture_requests == 2);
    REQUIRE_NOTHROW(model->PoseSpriteFrame(false));
    REQUIRE_NOTHROW(model->DrawSpriteFrame());
    CHECK(render.Draws.size() == draws_before_failure);

    resource_available = true;
    REQUIRE_NOTHROW(model->StartMeshGeneration());
    model->PoseSpriteFrame(false);
    model->DrawSpriteFrame();
    REQUIRE(render.Draws.back().Coherent);
    CHECK(render.Draws.back().RequestedVertices == 9);
    CHECK(render.Draws.back().RequestedIndices == 9);
    CHECK(render.Draws.back().UploadedVertices == 9);
    CHECK(render.Draws.back().UploadedIndices == 9);
    CHECK(render.Draws.back().UploadGeneration == baseline_generation + 1);
    CHECK(texture_requests == 3);
    logging::write("MODEL_MESH_REBUILD_RECOVERY requested={} uploaded={} upload_generation={} loader_requests={} coherent={}", render.Draws.back().RequestedIndices, render.Draws.back().UploadedIndices, render.Draws.back().UploadGeneration, texture_requests, render.Draws.back().Coherent);
}

#endif

FO_END_NAMESPACE
