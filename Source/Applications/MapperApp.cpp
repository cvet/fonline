// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "Application.h"
#include "Baker.h"
#include "Mapper.h"
#include "Settings.h"
#include "WebRelated.h"

#if !FO_TESTING_APP
#include "SDL3/SDL_main.h"
#endif

FO_USING_NAMESPACE();

struct MapperAppData
{
    refcount_nptr<MapperEngine> Mapper {};
};
FO_GLOBAL_DATA(MapperAppData, Data);

static auto GetMapperResources(GlobalSettings& settings) -> FileSystem;

static auto GetMapper() -> ptr<MapperEngine>
{
    FO_VERIFY_AND_THROW(Data->Mapper, "Mapper engine is not created");
    return Data->Mapper;
}

static void MapperEntry([[maybe_unused]] void* data)
{
    FO_TRACE_ZONE(App);

    if (!WebRelated::IsPersistentDataReady()) {
        return;
    }

    try {
        GetApp()->BeginFrame();

        if (!Data->Mapper) {
            try {
                auto settings {make_ptr(&GetApp()->Settings)};
                Data->Mapper = safe_alloc::make_refcounted<MapperEngine>(settings, GetMapperResources(*settings), &GetApp()->MainWindow);
                auto mapper {GetMapper()};
                mapper->SetInputLocked(GetApp()->Settings.Render.HeadlessWindow);
            }
            catch (const std::exception& ex) {
                exceptions::report_and_exit(ex);
            }
        }

        try {
            auto mapper {GetMapper()};
            mapper->MapperMainLoop();
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
[[maybe_unused]] static auto MapperApp(CommandLineArgs args) -> int
#endif
{
#if !FO_TESTING_APP
    CommandLineArgs args {numeric_cast<int32_t>(argc), argv};
#endif

    try {
        InitApp(args, AppInitFlags::ShowMessageOnException);

#if FO_IOS
        MapperEntry(nullptr);
        GetApp()->SetMainLoopCallback(MapperEntry);

#elif FO_WEB
        WebRelated::InitializePersistentData();
        WebRelated::StartMainLoop(MapperEntry, nullptr);

#elif FO_ANDROID
        while (!GetApp()->IsQuitRequested()) {
            MapperEntry(nullptr);
        }

#else
        auto balancer {FrameBalancer(!GetApp()->Settings.Render.VSync, GetApp()->Settings.Render.Sleep, GetApp()->Settings.Render.FixedFPS)};

        while (!GetApp()->IsQuitRequested()) {
            balancer.StartLoop();
            MapperEntry(nullptr);
            balancer.EndLoop();
        }
#endif

        if (Data->Mapper) {
            auto mapper {GetMapper()};
            mapper->Shutdown();
            Data->Mapper.reset();
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

static auto GetMapperResources(GlobalSettings& settings) -> FileSystem
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
