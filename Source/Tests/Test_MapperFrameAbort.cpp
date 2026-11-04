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

#include "Test_ClientEngineHelpers.h"
#include "Test_MapperHelpers.h"

FO_BEGIN_NAMESPACE

namespace MapperAbortTests
{
    class FaultQuadEffect final : public RenderEffect
    {
    public:
        explicit FaultQuadEffect(ptr<SpriteManager> sprites) :
            RenderEffect(EffectUsage::QuadSprite, "Effects/Test_Recording.fofx", ClientEngineTests::MakeRecordingQuadEffectLoader()),
            _sprites {sprites}
        {
        }

        void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override
        {
            ignore_unused(custom_tex);

            Calls++;
            StartIndex = start_index;
            IndexCount = indices_to_draw.value_or(dbuf->IndCount);
            VertexCount = dbuf->VertCount;
            Depth = _sprites->GetRtMngr().GetRenderTargetStack().size();
            BackendBound = static_cast<bool>(_sprites->GetRender().GetRenderTarget());

            throw InvalidCallException("Controlled mapper draw failure");
        }

        size_t Calls {};
        size_t StartIndex {};
        size_t IndexCount {};
        size_t VertexCount {};
        size_t Depth {};
        bool BackendBound {};

    private:
        ptr<SpriteManager> _sprites;
    };
}

TEST_CASE("MapperFrameDrawFailureUnbindsScene", "[mapper-frame-abort]")
{
    bool direct_map = GENERATE(false, true);
    CAPTURE(direct_map);

    REQUIRE(IsNonInteractiveMode());
    REQUIRE(GetApp()->IsHeadless());
    REQUIRE_FALSE(GetApp()->MainWindow.IsVirtual());
    REQUIRE_FALSE(GetApp()->Input.IsMouseAvailable());

    auto settings = MapperTests::MakeMapperTestSettings();
    BakerTests::OverrideSetting(settings.View.MapDirectDraw, direct_map);
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MapperTests::MakeMapperTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();
    auto destroy_context = scope_exit([]() noexcept { safe_call([] { ImGui::DestroyContext(); }); });

    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    auto map = mapper->LoadMapFromText("MapperAbortMap", "MapperAbortMap.fomap", MapperTests::MakeMapText(MapperTests::MakeItemBlock(10, MapperTests::SCENERY_A, 8, 8)));
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());
    mapper->SetInputLocked(true);
    mapper->InterfaceHidden = true;
    map->InstantScrollTo(mpos {8, 8});
    map->RebuildMap();

    REQUIRE(map->GetItems().size() == 1);
    auto item = map->GetItems().front().as_ptr();
    REQUIRE(item->IsMapSpriteVisible());

    size_t before_sprites = 0;
    size_t after_sprites = 0;
    size_t iface_calls = 0;
    Entity::EventCallbackData before;
    before.Callback = [&](FuncCallData& call) {
        ignore_unused(call);
        before_sprites++;
        return Entity::EventResult::ContinueChain;
    };
    before.SubscriptionPtr = reinterpret_cast<uintptr_t>(&before_sprites);
    mapper->OnRenderMap_BeforeSprites.Subscribe(std::move(before));

    Entity::EventCallbackData after;
    after.Callback = [&](FuncCallData& call) {
        ignore_unused(call);
        after_sprites++;
        return Entity::EventResult::ContinueChain;
    };
    after.SubscriptionPtr = reinterpret_cast<uintptr_t>(&after_sprites);
    mapper->OnRenderMap_AfterSprites.Subscribe(std::move(after));

    Entity::EventCallbackData iface;
    iface.Callback = [&](FuncCallData& call) {
        ignore_unused(call);
        iface_calls++;
        return Entity::EventResult::ContinueChain;
    };
    iface.SubscriptionPtr = reinterpret_cast<uintptr_t>(&iface_calls);
    mapper->OnRenderIface.Subscribe(std::move(iface));
    auto unsubscribe = scope_exit([&]() noexcept {
        mapper->OnRenderMap_BeforeSprites.Unsubscribe(reinterpret_cast<uintptr_t>(&before_sprites));
        mapper->OnRenderMap_AfterSprites.Unsubscribe(reinterpret_cast<uintptr_t>(&after_sprites));
        mapper->OnRenderIface.Unsubscribe(reinterpret_cast<uintptr_t>(&iface_calls));
    });

    auto draw_frame = [&] {
        ImGui::NewFrame();
        auto end_gui_frame = scope_exit([]() noexcept { ImGui::EndFrame(); });
        mapper->MapperMainLoop();
    };

    REQUIRE_NOTHROW(draw_frame());
    REQUIRE(before_sprites > 0);
    REQUIRE(before_sprites == after_sprites);
    REQUIRE(iface_calls == 1);
    REQUIRE(mapper->SprMngr.GetRtMngr().GetRenderTargetStack().empty());
    REQUIRE_FALSE(mapper->SprMngr.GetRender().GetRenderTarget());

    MapperAbortTests::FaultQuadEffect fault {&mapper->SprMngr};
    auto previous_effect = item->GetDrawEffect();
    auto restore_effect = scope_exit([&]() noexcept {
        mapper->SprMngr.AbortScene();
        item->SetDrawEffect(previous_effect);
    });
    item->SetDrawEffect(&fault);

    size_t completed_before_fault = after_sprites;
    bool threw = false;
    string error;

    try {
        draw_frame();
    }
    catch (const InvalidCallException& ex) {
        threw = true;
        error = ex.what();
    }

    size_t returned_depth = mapper->SprMngr.GetRtMngr().GetRenderTargetStack().size();
    bool returned_backend_bound = static_cast<bool>(mapper->SprMngr.GetRender().GetRenderTarget());
    bool fallback_abort_needed = returned_depth != 0 || returned_backend_bound;
    logging::write("MAPPER_ABORT_FAILURE direct={} throw={} calls={} vertices={} indices={} start={} fault_depth={} fault_backend={} returned_depth={} returned_backend={} before={} after={} iface={} fallback={} error={}",
        direct_map, threw, fault.Calls, fault.VertexCount, fault.IndexCount, fault.StartIndex, fault.Depth, fault.BackendBound,
        returned_depth, returned_backend_bound, before_sprites, after_sprites, iface_calls, fallback_abort_needed, error);

    CHECK(threw);
    CHECK(error.find("Controlled mapper draw failure") != string::npos);
    CHECK(fault.Calls == 1);
    CHECK(fault.VertexCount > 0);
    CHECK(fault.IndexCount > 0);
    size_t base_depth = mapper->SprMngr.GetMainRenderTarget() ? 1 : 0;
    CHECK(fault.Depth == base_depth + (direct_map ? 0 : 1));
    CHECK(fault.BackendBound == (fault.Depth != 0));
    CHECK(after_sprites >= completed_before_fault);
    CHECK(before_sprites == after_sprites + 1);
    CHECK(iface_calls == 1);
    CHECK(returned_depth == 0);
    CHECK_FALSE(returned_backend_bound);
    CHECK_FALSE(fallback_abort_needed);

    // Preserve the failed-state observation before diagnostic cleanup permits teardown and recovery
    if (fallback_abort_needed) {
        mapper->SprMngr.AbortScene();
    }

    item->SetDrawEffect(previous_effect);
    size_t before_recovery = before_sprites;
    size_t after_recovery = after_sprites;
    REQUIRE_NOTHROW(draw_frame());
    CHECK(before_sprites > before_recovery);
    CHECK(after_sprites > after_recovery);
    CHECK(before_sprites - before_recovery == after_sprites - after_recovery);
    CHECK(iface_calls == 2);
    CHECK(fault.Calls == 1);
    CHECK(mapper->SprMngr.GetRtMngr().GetRenderTargetStack().empty());
    CHECK_FALSE(mapper->SprMngr.GetRender().GetRenderTarget());
    logging::write("MAPPER_ABORT_RECOVERY direct={} fallback={} before={} after={} iface={} depth={} backend={}",
        direct_map, fallback_abort_needed, before_sprites, after_sprites, iface_calls,
        mapper->SprMngr.GetRtMngr().GetRenderTargetStack().size(), static_cast<bool>(mapper->SprMngr.GetRender().GetRenderTarget()));
}

FO_END_NAMESPACE
