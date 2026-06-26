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
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(Data->Mapper, "Mapper engine is not created");
    return Data->Mapper.as_ptr();
}

static void MapperEntry([[maybe_unused]] void* data)
{
    FO_STACK_TRACE_ENTRY();

    if (!WebRelated::IsPersistentDataReady()) {
        return;
    }

    try {
        GetApp()->BeginFrame();

        if (!Data->Mapper) {
            try {
                ptr<GlobalSettings> settings = &GetApp()->Settings;
                ptr<IAppWindow> main_window = &GetApp()->MainWindow;
                Data->Mapper = SafeAlloc::MakeRefCounted<MapperEngine>(settings, GetMapperResources(*settings), main_window);
                auto mapper = GetMapper();
                mapper->SetInputLocked(GetApp()->Settings.HeadlessWindow);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        try {
            auto mapper = GetMapper();
            mapper->MapperMainLoop();
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
[[maybe_unused]] static auto MapperApp(CommandLineArgs args) -> int
#endif
{
    FO_STACK_TRACE_ENTRY();

#if !FO_TESTING_APP
    const vector<CommandLineArg> args_holder = MakeCommandLineArgs(numeric_cast<int32_t>(argc), argv);
    const CommandLineArgs args {args_holder};
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
        auto balancer = FrameBalancer(!GetApp()->Settings.VSync, GetApp()->Settings.Sleep, GetApp()->Settings.FixedFPS);

        while (!GetApp()->IsQuitRequested()) {
            balancer.StartLoop();
            MapperEntry(nullptr);
            balancer.EndLoop();
        }
#endif

        if (Data->Mapper) {
            auto mapper = GetMapper();
            mapper->Shutdown();
            Data->Mapper.reset();
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

static auto GetMapperResources(GlobalSettings& settings) -> FileSystem
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
        ptr<BakingSettings> settings_ptr = &settings;
        resources.AddCustomSource(SafeAlloc::MakeUnique<BakerDataSource>(settings_ptr));
        return resources;
    }
}
