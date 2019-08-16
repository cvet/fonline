#include "Common.h"
#include "Mapper.h"
#include "Exception.h"
#include "AppGui.h"

struct GuiWindow
{
    virtual bool Loop() = 0;
    virtual ~GuiWindow() = default;
};

struct DemoWindow: public GuiWindow
{
    bool   ShowDemoWindow = true;
    bool   ShowAnotherWindow = false;
    ImVec4 ClearColor = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );

    virtual bool Loop() override
    {
        {
            static float f = 0.0f;
            static int   counter = 0;

            ImGui::Begin( "Hello, world!" );                                       // Create a window called "Hello, world!" and append into it.

            ImGui::Text( "This is some useful text." );                            // Display some text (you can use a format strings too)
            ImGui::Checkbox( "Demo Window", &ShowDemoWindow );                     // Edit bools storing our window open/close state
            ImGui::Checkbox( "Another Window", &ShowAnotherWindow );

            ImGui::SliderFloat( "float", &f, 0.0f, 1.0f );                         // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3( "clear color", (float*) &ClearColor );              // Edit 3 floats representing a color

            if( ImGui::Button( "Button" ) )                                        // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text( "counter = %d", counter );

            ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate );
            ImGui::End();
        }

        if( ShowAnotherWindow )
        {
            ImGui::Begin( "Another Window", &ShowAnotherWindow );
            ImGui::Text( "Hello from another window!" );
            if( ImGui::Button( "Close Me" ) )
                ShowAnotherWindow = false;
            ImGui::End();
        }

        return true;
    }
};

struct AssetsWindow: GuiWindow
{
    virtual bool Loop() override
    {
        return true;
    }
};

struct MapWindow: GuiWindow
{
    FOMapper* MapInstance;

    MapWindow( std::string map_name )
    {
        MapInstance = new FOMapper();
        if( !MapInstance->Init() )              // || !data->MapInstance->LoadMap)
            SAFEDEL( MapInstance );
    }

    bool IsLoaded()
    {
        return MapInstance != nullptr;
    }

    virtual bool Loop() override
    {
        MapInstance->MainLoop();
        return true;
    }

    virtual ~MapWindow() override
    {
        if( MapInstance )
            MapInstance->Finish();
        SAFEDEL( MapInstance );
    }
};

static vector< GuiWindow* > Windows;
static vector< GuiWindow* > NewWindows;
static vector< GuiWindow* > CloseWindows;

extern "C" int main( int argc, char** argv ) // Handled by SDL
{
    InitialSetup( argc, argv );

    // Threading
    Thread::SetCurrentName( "GUI" );

    // Exceptions
    CatchExceptions( "FOnlineEditor", FONLINE_VERSION );

    // Timer
    Timer::Init();

    // Logging
    LogToFile( "FOnlineEditor.log" );
    WriteLog( "FOnline Editor v.{}.\n", FONLINE_VERSION );

    // Options
    GetClientOptions();

    // Initialize ImGui
    bool use_dx = ( MainConfig->GetInt( "", "UseDirectX" ) != 0 || false );
    if( !AppGui::Init( "FOnline Editor", use_dx, true, true ) )
        return -1;

    // Basic windows
    Windows.push_back( new DemoWindow() );

    // Main loop
    while( !GameOpt.Quit )
    {
        if( !AppGui::BeginFrame() )
            break;

        for( GuiWindow* window : Windows )
            if( !window->Loop() )
                CloseWindows.push_back( window );

        if( !NewWindows.empty() )
        {
            Windows.insert( Windows.end(), NewWindows.begin(), NewWindows.end() );
            NewWindows.clear();
        }
        while( !CloseWindows.empty() )
        {
            auto       it = std::find( Windows.begin(), Windows.end(), CloseWindows.back() );
            RUNTIME_ASSERT( it != Windows.end() );
            GuiWindow* window = *it;
            Windows.erase( it );
            CloseWindows.pop_back();
            delete window;
        }

        if( Windows.empty() )
            break;

        AppGui::EndFrame();
    }

    // Graceful shutdown
    // Other stuff will be reseted by operating system
    for( GuiWindow* window : Windows )
        delete window;

    return 0;
}
