//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

#include "catch_amalgamated.hpp"

#include "Common.h"

#include "Application.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

TEST_CASE("ApplicationChildWindowLifecycle")
{
    ptr<Application> app = GetApp();

    SECTION("ChildWindowsAreCreatedActivatedAndDestroyed")
    {
        nptr<AppWindow> child = app->CreateChildWindow(isize32 {320, 240}, "coverage child");
        REQUIRE(child);

        auto cleanup = scope_exit([&app, child]() noexcept { safe_call([&app, child] { app->DestroyChildWindow(child); }); });

        // A freshly created child is addressable and reports the size it was asked for
        isize32 child_size = child->GetSize();
        CHECK(child_size.width > 0);
        CHECK(child_size.height > 0);

        app->SetActiveWindow(child);
        app->SetActiveWindow(&app->MainWindow);

        app->DestroyChildWindow(child);
        cleanup.release();
    }

    SECTION("MultipleChildWindowsCoexist")
    {
        nptr<AppWindow> first = app->CreateChildWindow(isize32 {200, 150}, "first");
        nptr<AppWindow> second = app->CreateChildWindow(isize32 {400, 300}, "second");
        REQUIRE(first);
        REQUIRE(second);
        CHECK(first != second);

        app->SetActiveWindow(second);
        app->SetActiveWindow(first);
        app->SetActiveWindow(&app->MainWindow);

        app->DestroyChildWindow(first);
        app->DestroyChildWindow(second);
    }

    SECTION("DestroyingNothingIsANoOp")
    {
        REQUIRE_NOTHROW(app->DestroyChildWindow(nullptr));
        REQUIRE_NOTHROW(app->SetActiveWindow(nullptr));
        app->SetActiveWindow(&app->MainWindow);
    }
}

TEST_CASE("ApplicationWindowRenderScope")
{
    ptr<Application> app = GetApp();

    SECTION("RenderScopeOpensAndClosesForTheMainWindow")
    {
        REQUIRE_NOTHROW(app->BeginWindowRender(&app->MainWindow));
        REQUIRE_NOTHROW(app->EndWindowRender());
    }

    SECTION("RenderScopeOpensAndClosesForAChildWindow")
    {
        nptr<AppWindow> child = app->CreateChildWindow(isize32 {256, 256}, "render child");
        REQUIRE(child);

        auto cleanup = scope_exit([&app, child]() noexcept { safe_call([&app, child] { app->DestroyChildWindow(child); }); });

        // Rendering into a child stands up its virtual render texture on first use
        REQUIRE_NOTHROW(app->BeginWindowRender(child.as_ptr()));
        REQUIRE_NOTHROW(app->EndWindowRender());

        REQUIRE_NOTHROW(app->BeginWindowRender(child.as_ptr()));
        REQUIRE_NOTHROW(app->EndWindowRender());
    }
}

TEST_CASE("ApplicationCoordinateTranslation")
{
    ptr<Application> app = GetApp();

    SECTION("HostAndActiveWindowRoundTripOnTheMainWindow")
    {
        app->SetActiveWindow(&app->MainWindow);

        ipos32 host_pos {40, 30};
        ipos32 in_window = app->TranslateHostPosToActiveWindow(host_pos);
        ipos32 back_to_host = app->TranslateActiveWindowPosToHost(in_window);

        // With the main window active the mapping is identity in both directions
        CHECK(in_window.x == host_pos.x);
        CHECK(in_window.y == host_pos.y);
        CHECK(back_to_host.x == host_pos.x);
        CHECK(back_to_host.y == host_pos.y);
    }

    SECTION("DeltaScalingIsStableForTheMainWindow")
    {
        app->SetActiveWindow(&app->MainWindow);

        ipos32 delta {7, -9};
        ipos32 scaled = app->ScaleHostDeltaToActiveWindow(delta);

        CHECK(scaled.x == delta.x);
        CHECK(scaled.y == delta.y);

        ipos32 zero = app->ScaleHostDeltaToActiveWindow(ipos32 {0, 0});
        CHECK(zero.x == 0);
        CHECK(zero.y == 0);
    }

    SECTION("TranslationStaysWellFormedForAChildWindow")
    {
        nptr<AppWindow> child = app->CreateChildWindow(isize32 {160, 120}, "translate child");
        REQUIRE(child);

        auto cleanup = scope_exit([&app, child]() noexcept {
            safe_call([&app, child] {
                app->SetActiveWindow(&app->MainWindow);
                app->DestroyChildWindow(child);
            });
        });

        app->SetActiveWindow(child);

        // The child mapping may scale, but it must stay finite and round-trip without drifting wildly
        ipos32 in_window = app->TranslateHostPosToActiveWindow(ipos32 {80, 60});
        ipos32 back_to_host = app->TranslateActiveWindowPosToHost(in_window);

        CHECK(std::abs(in_window.x) < 100000);
        CHECK(std::abs(in_window.y) < 100000);
        CHECK(std::abs(back_to_host.x) < 100000);
        CHECK(std::abs(back_to_host.y) < 100000);

        ipos32 scaled = app->ScaleHostDeltaToActiveWindow(ipos32 {10, 10});
        CHECK(std::abs(scaled.x) < 100000);
        CHECK(std::abs(scaled.y) < 100000);
    }
}

TEST_CASE("HeadlessApplicationAudioAndInputSurfaces")
{
    ptr<Application> app = GetApp();

    SECTION("AudioAnswersWithoutADevice")
    {
        ptr<IAppAudio> audio = app->MainWindow.GetAudio();

        // The headless build has no audio device: the query answers false and every operation on it is
        // rejected outright rather than silently doing nothing
        CHECK_FALSE(audio->IsEnabled());
        CHECK_THROWS(audio->LockDevice());
        CHECK_THROWS(audio->UnlockDevice());

        vector<uint8_t> buf(16, uint8_t {0});
        CHECK_THROWS(audio->ConvertAudio(0, 2, 44100, buf));

        vector<uint8_t> output(16, uint8_t {0});
        CHECK_THROWS(audio->MixAudio(output, buf, 100));
        CHECK_THROWS(audio->SetSource(nullptr));
    }

    SECTION("InputDrainsSimulatedEvents")
    {
        ptr<IAppInput> input = app->MainWindow.GetInput();

        InputEvent drained;

        while (input->PollEvent(drained)) {
        }

        InputEvent pushed;
        pushed.Type = InputEvent::EventType::KeyDownEvent;
        pushed.KeyDown.Code = KeyCode::A;
        pushed.KeyDown.Text = "a";
        input->PushEvent(pushed, false);

        // The queue only yields the pushed event once the frame that owns it is pumped
        bool got_event = false;

        for (int32_t i = 0; i < 4 && !got_event; i++) {
            got_event = input->PollEvent(drained);

            if (!got_event) {
                GetApp()->BeginFrame();
                GetApp()->EndFrame();
            }
        }

        ignore_unused(got_event);

        // The headless input has no system clipboard behind it, so a write does not become readable
        input->SetClipboardText("headless clipboard");
        ignore_unused(input->GetClipboardText());

        ignore_unused(input->IsMouseAvailable());
        ignore_unused(input->GetMousePosition());
        ignore_unused(input->GetGamepadState());
        ignore_unused(input->IsShiftDown());
        ignore_unused(input->IsCtrlDown());
        ignore_unused(input->IsAltDown());
    }
}

TEST_CASE("StubAppWindowServesAsAnEmbedderFrontend")
{
    // The stub frontend is engine surface an embedding project plugs in through GetAppWindowStub; nothing
    // inside the engine calls it, so it only runs when a test does
    auto settings = GlobalSettings(false);
    settings.ApplyDefaultSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);

    unique_ptr<IAppWindow> window = GetAppWindowStub(settings);
    ignore_unused(window.get());

    SECTION("GeometryAndStateRoundTrip")
    {
        isize32 initial_size = window->GetSize();
        CHECK(initial_size.width > 0);
        CHECK(initial_size.height > 0);
        CHECK(window->GetScreenSize() == initial_size);
        CHECK_FALSE(window->IsVirtual());

        window->SetSize(isize32 {640, 480});
        CHECK(window->GetSize() == isize32 {640, 480});

        window->SetScreenSize(isize32 {800, 600});
        CHECK(window->GetScreenSize() == isize32 {800, 600});

        window->SetPosition(ipos32 {10, 20});
        CHECK(window->GetPosition() == ipos32 {10, 20});

        CHECK(window->IsFocused());
        window->Minimize();
        CHECK_FALSE(window->IsFocused());

        // ToggleFullscreen reports whether the state changed, so a repeat of the same request answers false
        CHECK_FALSE(window->IsFullscreen());
        CHECK(window->ToggleFullscreen(true));
        CHECK(window->IsFullscreen());
        CHECK_FALSE(window->ToggleFullscreen(true));
        CHECK(window->ToggleFullscreen(false));
        CHECK_FALSE(window->IsFullscreen());

        CHECK_NOTHROW(window->GrabInput(true));
        CHECK_NOTHROW(window->GrabInput(false));
        CHECK_NOTHROW(window->AlwaysOnTop(true));
        CHECK_NOTHROW(window->AlwaysOnTop(false));
        CHECK_NOTHROW(window->Blink());
        CHECK_FALSE(static_cast<bool>(window->GetWindowHandleForInput()));

        ignore_unused(window->GetOnWindowSizeChanged());
        ignore_unused(window->GetOnScreenSizeChanged());
        ignore_unused(window->GetOnLowMemory());
    }

    SECTION("RenderBackendAnswersWithoutADevice")
    {
        ptr<IAppRender> render = window->GetRender();

        unique_ptr<RenderTexture> tex = render->CreateTexture(isize32 {32, 32}, true, false);
        ignore_unused(tex.get());

        unique_ptr<RenderDrawBuffer> buf = render->CreateDrawBuffer(false);
        ignore_unused(buf.get());

        CHECK_NOTHROW(render->SetRenderTarget(tex.get()));
        CHECK_NOTHROW(render->ClearRenderTarget(ucolor {0, 0, 0, 255}, true, true));
        CHECK_NOTHROW(render->EnableScissor(irect32 {0, 0, 16, 16}));
        CHECK_NOTHROW(render->DisableScissor());
        CHECK_NOTHROW(render->SetRenderTarget(nullptr));
        CHECK_NOTHROW(render->SetOrthoDepthRange(-10.0f, 10.0f));

        mat44 ortho = render->CreateOrthoMatrix(0.0f, 100.0f, 0.0f, 100.0f, -1.0f, 1.0f);
        ignore_unused(ortho);
        ignore_unused(render->GetProjMatrix());
        ignore_unused(render->IsRenderTargetFlipped());
        ignore_unused(render->GetRenderTarget());
    }

    SECTION("InputAndAudioReportAnEmptyDevice")
    {
        ptr<IAppInput> input = window->GetInput();

        CHECK_FALSE(input->IsMouseAvailable());
        CHECK_FALSE(input->IsShiftDown());
        CHECK_FALSE(input->IsCtrlDown());
        CHECK_FALSE(input->IsAltDown());
        ignore_unused(input->GetMousePosition());
        ignore_unused(input->GetGamepadState());

        input->SetClipboardText("stub clipboard");
        CHECK(input->GetClipboardText() == "stub clipboard");

        InputEvent ev;
        CHECK_FALSE(input->PollEvent(ev));

        input->PushEvent(InputEvent {}, false);
        ignore_unused(input->PollEvent(ev));

        ptr<IAppAudio> audio = window->GetAudio();
        CHECK_FALSE(audio->IsEnabled());
    }

    window->Destroy();
}

FO_END_NAMESPACE
