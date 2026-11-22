// FOnline Engine
// SPDX-License-Identifier: MIT

// Standalone particle effect viewer. Owns a ClientEngine for its content services (protos, sprites,
// particle factory, fonts, effects) but runs its own frame instead of the networked main loop

#include "Common.h"

#include "Application.h"
#include "Baker.h"
#include "Client.h"
#include "ParticleViewer.h"
#include "Settings.h"
#include "WebRelated.h"

#if !FO_TESTING_APP
#include "SDL3/SDL_main.h"
#endif

FO_USING_NAMESPACE();

struct ParticleViewerAppData
{
    refcount_nptr<ClientEngine> Engine {};
    unique_nptr<ParticleViewer> Viewer {};
};
FO_GLOBAL_DATA(ParticleViewerAppData, Data);

static auto GetViewerResources(GlobalSettings& settings) -> FileSystem;

static auto GetEngine() -> ptr<ClientEngine>
{
    FO_VERIFY_AND_THROW(Data->Engine, "Particle viewer engine is not created");
    return Data->Engine;
}

static auto GetViewer() -> ptr<ParticleViewer>
{
    FO_VERIFY_AND_THROW(Data->Viewer, "Particle viewer is not created");
    return Data->Viewer;
}

// The client's own main loop is deliberately not used: it drives networking, login, and game
// screens this tool has no use for
static void DrawViewerFrame()
{
    FO_TRACE_ZONE(Editor);

    auto engine {GetEngine()};

    engine->FrameAdvance();
    engine->EffectMngr.UpdateEffects(engine->GameTime);
    engine->FontMngr.FrameUpdate();

    engine->SprMngr.BeginScene();

    // EndScene must balance BeginScene even if Draw throws, or the scene render target stays bound
    // and the next EndFrame fails
    auto end_scene {scope_exit([&]() noexcept { safe_call([&engine] { engine->SprMngr.EndScene(); }); })};

    GetViewer()->Draw();
}

static void ParticleViewerEntry([[maybe_unused]] void* data)
{
    FO_TRACE_ZONE(App);

    if (!WebRelated::IsPersistentDataReady()) {
        return;
    }

    try {
        GetApp()->BeginFrame();

        if (!Data->Engine) {
            try {
                auto settings {make_ptr(&GetApp()->Settings)};
                Data->Engine = safe_alloc::make_refcounted<ClientEngine>(settings, GetViewerResources(*settings), &GetApp()->MainWindow);

                auto engine {GetEngine()};
                Data->Viewer = safe_alloc::make_unique<ParticleViewer>(engine, &engine->SprMngr, &engine->ResMngr);

                // The window is the whole application here: it starts open and
                // fills the viewport instead of floating inside an empty frame
                Data->Viewer->SetVisible(true);
                Data->Viewer->SetFillViewport(true);
            }
            catch (const std::exception& ex) {
                exceptions::report_and_continue(ex);
                GetApp()->RequestQuit();
                GetApp()->EndFrame();
                return;
            }
        }

        try {
            DrawViewerFrame();
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }

        GetApp()->EndFrame();
    }
    catch (const std::exception& ex) {
        exceptions::report_and_continue(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

#if !FO_TESTING_APP
int main(int argc, char** argv) // Handled by SDL
#else
[[maybe_unused]] static auto ParticleViewerApp(CommandLineArgs args) -> int
#endif
{
#if !FO_TESTING_APP
    CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
#endif

    try {
        InitApp(args, AppInitFlags::ShowMessageOnException);

#if FO_IOS
        ParticleViewerEntry(nullptr);
        GetApp()->SetMainLoopCallback(ParticleViewerEntry);

#elif FO_WEB
        WebRelated::InitializePersistentData();
        WebRelated::StartMainLoop(ParticleViewerEntry, nullptr);

#elif FO_ANDROID
        while (!GetApp()->IsQuitRequested()) {
            ParticleViewerEntry(nullptr);
        }

#else
        auto balancer {FrameBalancer(!GetApp()->Settings.Render.VSync, GetApp()->Settings.Render.Sleep, GetApp()->Settings.Render.FixedFPS)};

        while (!GetApp()->IsQuitRequested()) {
            balancer.StartLoop();
            ParticleViewerEntry(nullptr);
            balancer.EndLoop();
        }
#endif

        if (Data->Engine) {
            if (Data->Viewer) {
                Data->Viewer->SaveSettings();
            }

            Data->Viewer.reset();
            auto engine {GetEngine()};
            engine->Shutdown();
            Data->Engine.reset();
        }

        exit_app(true);
    }
    catch (const std::exception& ex) {
        exceptions::report_and_exit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

static auto GetViewerResources(GlobalSettings& settings) -> FileSystem
{
    if (settings.Common.Packaged) {
        FileSystem resources {};
        resources.AddPacksSource(settings.Common.Packaged, settings.Common.ResourcesPath, settings.GetClientResourcePacks());
        resources.AddPacksSource(settings.Common.Packaged, settings.Common.ResourcesPath, settings.GetMapperResourcePacks());
        return resources;
    }
    else {
        FileSystem resources {};
        resources.AddCustomSource(safe_alloc::make_unique<BakerDataSource>(&settings));
        return resources;
    }
}
