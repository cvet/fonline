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
//

///
/// Standalone critter animation viewer.
///
/// Boots straight into the animation viewer window: no map, no mapper UI, no
/// server connection. The viewer needs the client-side content services
/// (prototypes, sprites, models, fonts, effects), which a plain `ClientEngine`
/// already constructs, so this application owns one and drives a minimal frame
/// of its own instead of the client's networked main loop.
///

#include "Common.h"

#include "Application.h"
#include "Baker.h"
#include "AnimationViewer.h"
#include "Client.h"
#include "Settings.h"
#include "WebRelated.h"

#if !FO_TESTING_APP
#include "SDL3/SDL_main.h"
#endif

FO_USING_NAMESPACE();

struct AnimationViewerAppData
{
    refcount_nptr<ClientEngine> Engine {};
    unique_nptr<AnimationViewer> Viewer {};
};
FO_GLOBAL_DATA(AnimationViewerAppData, Data);

static auto GetViewerResources(GlobalSettings& settings) -> FileSystem;

static auto GetEngine() -> ptr<ClientEngine>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(Data->Engine, "Animation viewer engine is not created");
    return Data->Engine;
}

static auto GetViewer() -> ptr<AnimationViewer>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(Data->Viewer, "Animation viewer is not created");
    return Data->Viewer;
}

// Minimal frame: advance time, refresh the render-side managers, and draw the
// single window. The client's own main loop is deliberately not used - it
// drives networking, login, and game screens this tool has no use for.
static void DrawViewerFrame()
{
    FO_STACK_TRACE_ENTRY();

    auto engine = GetEngine();

    engine->FrameAdvance();
    engine->EffectMngr.UpdateEffects(engine->GameTime);
    engine->FontMngr.FrameUpdate();

    engine->SprMngr.BeginScene();

    // EndScene must balance BeginScene even if Draw throws, or the scene render
    // target is left bound and the following EndFrame fails ("render target tex
    // must be unset").
    auto end_scene = scope_exit([&]() noexcept { safe_call([&engine] { engine->SprMngr.EndScene(); }); });

    GetViewer()->Draw();
}

static void AnimationViewerEntry([[maybe_unused]] void* data)
{
    FO_STACK_TRACE_ENTRY();

    if (!WebRelated::IsPersistentDataReady()) {
        return;
    }

    try {
        GetApp()->BeginFrame();

        if (!Data->Engine) {
            try {
                auto settings = make_ptr(&GetApp()->Settings);
                Data->Engine = SafeAlloc::MakeRefCounted<ClientEngine>(settings, GetViewerResources(*settings), &GetApp()->MainWindow);

                auto engine = GetEngine();
                Data->Viewer = SafeAlloc::MakeUnique<AnimationViewer>(engine, &engine->SprMngr, &engine->ResMngr, &engine->GameTime);

                // The window is the whole application here: it starts open and
                // fills the viewport instead of floating inside an empty frame.
                Data->Viewer->SetVisible(true);
                Data->Viewer->SetFillViewport(true);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
                GetApp()->RequestQuit();
                GetApp()->EndFrame();
                return;
            }
        }

        try {
            DrawViewerFrame();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }

        GetApp()->EndFrame();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

#if !FO_TESTING_APP
int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto AnimationViewerApp(CommandLineArgs args) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

#if !FO_TESTING_APP
    const CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
#endif

    try {
        InitApp(args, AppInitFlags::ShowMessageOnException);

#if FO_IOS
        AnimationViewerEntry(nullptr);
        GetApp()->SetMainLoopCallback(AnimationViewerEntry);

#elif FO_WEB
        WebRelated::InitializePersistentData();
        WebRelated::StartMainLoop(AnimationViewerEntry, nullptr);

#elif FO_ANDROID
        while (!GetApp()->IsQuitRequested()) {
            AnimationViewerEntry(nullptr);
        }

#else
        auto balancer = FrameBalancer(!GetApp()->Settings.VSync, GetApp()->Settings.Sleep, GetApp()->Settings.FixedFPS);

        while (!GetApp()->IsQuitRequested()) {
            balancer.StartLoop();
            AnimationViewerEntry(nullptr);
            balancer.EndLoop();
        }
#endif

        if (Data->Engine) {
            if (Data->Viewer) {
                Data->Viewer->SaveSettings();
            }

            Data->Viewer.reset();
            auto engine = GetEngine();
            engine->Shutdown();
            Data->Engine.reset();
        }

        ExitApp(true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

static auto GetViewerResources(GlobalSettings& settings) -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    if (IsPackaged()) {
        FileSystem resources;
        resources.AddPacksSource(settings.ClientResources, settings.ClientResourceEntries);
        resources.AddPacksSource(settings.ClientResources, settings.MapperResourceEntries);
        return resources;
    }
    else {
        FileSystem resources;
        resources.AddCustomSource(SafeAlloc::MakeUnique<BakerDataSource>(&settings));
        return resources;
    }
}
