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

#include "Application.h"
#include "Client.h"
#include "MapView.h"
#include "PlayerView.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

class LifetimeMaskReadEffect final : public RenderEffect
{
public:
    explicit LifetimeMaskReadEffect(ptr<RenderTargetManager> targets) :
        RenderEffect(EffectUsage::QuadSprite, "LifetimeQueued.fofx", [](string_view name) -> string { return name.ends_with("-info") ? "[EffectInfo]\nMainTex = 0\nIndoorMaskTex = 1\n" : "[Effect]\nPasses = 1\n"; }),
        Targets {targets}
    {
    }

    void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override
    {
        ignore_unused(dbuf, start_index, indices_to_draw, custom_tex);
        REQUIRE(IndoorMaskTex);
        OwnersAtDraw = Targets->GetRenderTargetCount();
        Draws++;
    }

    size_t OwnersAtDraw {};
    size_t Draws {};

private:
    ptr<RenderTargetManager> Targets;
};

static auto MakeClientLifetimeSettings() -> GlobalSettings
{
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    return settings;
}

static auto MakeClientLifetimeEngine(GlobalSettings& settings) -> refcount_ptr<ClientEngine>
{
    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEntityLifetime");
    source->AddFile("Metadata.fometa-client", metadata);
    source->AddFile("LifetimeSprite.png", BakerTests::MakeMinimalBakedSprite(2, 2));

    for (string_view name : {"ImGui_Default", "2D_Default", "2D_NoDepth", "Primitive_Default", "Primitive_Light", "Primitive_Fog", "Flush_RenderTarget", "Flush_Primitive", "Flush_Map", "Flush_Light", "Flush_Fog", "3D_Skinned", "Particles_ColorMulAtlas", "Particles_ColorAddAtlas", "Particles_ColorSubAtlas", "Particles_DistortionAtlas", "Particles_DistortionAddAtlas"}) {
        source->AddFile(strex("Effects/{}.fofx", name).str(), "[Effect]\nPasses = 1\n");
        source->AddFile(strex("Effects/{}.fofx-1-info", name).str(), "[EffectInfo]\n");
    }

    for (string_view name : {"LifetimeMaskA", "LifetimeMaskB", "LifetimeMaskC"}) {
        source->AddFile(strex("Effects/{}.fofx", name).str(), "[Effect]\nPasses = 1\n");
        source->AddFile(strex("Effects/{}.fofx-1-info", name).str(), "[EffectInfo]\nMainTex = 0\nIndoorMaskTex = 1\n");
    }

#if FO_ANGELSCRIPT_SCRIPTING
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientEntityLifetimeCompiler");
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine compiler {compiler_resources};
    source->AddFile("ClientEntityLifetime.fos-bin-client", BakerTests::CompileInlineScripts(&compiler, "ClientEntityLifetimeScripts", {{"Scripts/Lifetime.fos", "void LifetimeFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return safe_alloc::make_refcounted<ClientEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
}

TEST_CASE("MapViewRenderTargetsAreReleasedOnDestroy")
{
    GlobalSettings settings = MakeClientLifetimeSettings();
    bool direct_draw = false;
    bool disable_mask = false;

    SECTION("DefaultSettings")
    {
    }
    SECTION("DisabledIndoorMask")
    {
        disable_mask = true;
    }
    SECTION("DirectDraw")
    {
        direct_draw = true;
    }

    BakerTests::OverrideSetting(settings.View.MapDirectDraw, direct_draw);
    BakerTests::OverrideSetting(settings.View.DisableIndoorMask, disable_mask);
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    ProtoMap proto {client->Hashes.to_hashed_string("LifetimeMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto& targets = client->SprMngr.GetRtMngr();
    size_t baseline = targets.GetRenderTargetCount();
    size_t map_targets = direct_draw ? 1 : (disable_mask ? 2 : 3);

    for (int32_t cycle = 0; cycle < 5; cycle++) {
        auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr);
        CHECK(targets.GetRenderTargetCount() == baseline + map_targets);
        map->DestroySelf();
        CHECK(targets.GetRenderTargetCount() == baseline);
    }
}

TEST_CASE("MapViewDestroyClearsOnlyItsCachedIndoorMaskReferences")
{
    GlobalSettings settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    ProtoMap proto {client->Hashes.to_hashed_string("LifetimeMap"), client->GetPropertyRegistrar(MapView::ENTITY_TYPE_NAME)};
    proto.SetSize(msize {16, 16});
    auto sprite = client->SprMngr.LoadSprite("LifetimeSprite.png", AtlasType::IfaceSprites);
    REQUIRE(sprite);
    auto& targets = client->SprMngr.GetRtMngr();
    size_t baseline = targets.GetRenderTargetCount();
    auto outer_target = targets.CreateRenderTarget(false, {320, 200}, false);
    targets.PushRenderTarget(outer_target);
    auto outer_cleanup = scope_exit([&]() noexcept {
        safe_call([&] {
            targets.PopRenderTarget();
            targets.DeleteRenderTarget(outer_target);
        });
    });
    auto mask_a = client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskA.fofx");
    auto mask_b = client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskB.fofx");
    auto mask_c = client->EffectMngr.LoadEffect(EffectUsage::QuadSprite, "Effects/LifetimeMaskC.fofx");
    REQUIRE(mask_a);
    REQUIRE(mask_b);
    REQUIRE(mask_c);
    client->EffectMngr.Effects.FlushMap = mask_a;
    auto first = safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, make_ptr(&proto), isize32 {320, 200}, nullptr);
    auto first_cleanup = scope_exit([&]() noexcept {
        safe_call([&] {
            if (!first->IsDestroyed()) {
                first->DestroySelf();
            }
        });
    });
    first->DrawMap();
    REQUIRE(mask_a->IndoorMaskTex);
    auto retiring_texture = mask_a->IndoorMaskTex;
    mask_b->IndoorMaskTex = mask_a->IndoorMaskTex;
    client->EffectMngr.Effects.FlushMap = mask_c;
    auto second = safe_alloc::make_refcounted<MapView>(client, ident_t {1002}, make_ptr(&proto), isize32 {320, 200}, nullptr);
    auto second_cleanup = scope_exit([&]() noexcept {
        safe_call([&] {
            if (!second->IsDestroyed()) {
                second->DestroySelf();
            }
        });
    });
    second->DrawMap();
    REQUIRE(mask_c->IndoorMaskTex);
    auto second_texture = mask_c->IndoorMaskTex;
    REQUIRE(second_texture != mask_a->IndoorMaskTex);
    LifetimeMaskReadEffect queued_effect {make_ptr(&targets)};
    queued_effect.IndoorMaskTex = retiring_texture;
    sprite->SetDrawEffect(make_nptr(&queued_effect));
    client->SprMngr.DrawSprite(sprite, {}, ucolor {255, 255, 255, 255});
    CHECK(queued_effect.Draws == 0);
    first->DestroySelf();
    CHECK(queued_effect.Draws == 1);
    CHECK(queued_effect.OwnersAtDraw == baseline + 7);
    client->SprMngr.Flush();
    sprite->SetDrawEffect(nullptr);
    queued_effect.IndoorMaskTex = nullptr;
    CHECK_FALSE(mask_a->IndoorMaskTex);
    CHECK_FALSE(mask_b->IndoorMaskTex);
    CHECK(mask_c->IndoorMaskTex == second_texture);
    CHECK(targets.GetRenderTargetCount() == baseline + 4);
    REQUIRE(targets.GetRenderTargetStack().size() == 1);
    CHECK(targets.GetCurrentRenderTarget() == nptr<RenderTarget> {outer_target});
    CHECK(client->SprMngr.GetRender().GetRenderTarget() == nptr<RenderTexture> {outer_target->GetTexture()});
    CHECK_NOTHROW(second->DrawMap());
    second->DestroySelf();
    CHECK_FALSE(mask_c->IndoorMaskTex);
    CHECK(targets.GetRenderTargetCount() == baseline + 1);
}

TEST_CASE("ClientEngineFinishesStartingUpOnceItRuns")
{
    // An engine is starting up from the moment it exists, so the client owes the finish; one that never
    // performed it reported every one-shot load of its own construction as a responsiveness failure
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    CHECK_FALSE(client->IsStartingUp());

    // There is no way back into the state, so a second finish is a start path running twice
    CHECK_THROWS_AS(client->FinishStartingUp(), VerificationException);
}

TEST_CASE("ClientEntityLookupRetainsUntilCallerFinishes")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});

    {
        auto retained = client->GetEntity(ident_t {1001});
        REQUIRE(retained);
        REQUIRE(player->GetRefCount() == 2);

        std::thread release_thread {[owner = std::move(player)] { ignore_unused(owner); }};
        release_thread.join();

        CHECK(retained->GetId() == ident_t {1001});
        CHECK(retained->GetRefCount() == 1);
    }

    CHECK_FALSE(client->GetEntity(ident_t {1001}));
}

TEST_CASE("ClientEntityFinalReleasePreservesSuccessorRegistration")
{
    auto settings = MakeClientLifetimeSettings();
    auto client = MakeClientLifetimeEngine(settings);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto old_player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});
    auto new_player = safe_alloc::make_refcounted<PlayerView>(client, ident_t {1001});
    std::thread release_thread {[owner = std::move(old_player)] { ignore_unused(owner); }};
    release_thread.join();

    CHECK(client->GetEntity(ident_t {1001}) == new_player);
    new_player->DestroySelf();
    CHECK_FALSE(client->GetEntity(ident_t {1001}));
}

FO_END_NAMESPACE
