#include "Common.h"
// #include "Server.h"
// #include "Client.h"
#include "Mapper.h"
#include "AppGui.h"

static void DockSpaceBegin();
static void DockSpaceEnd();

struct GuiWindow
{
    string       Name;
    GuiWindow( string name ): Name( name ) {}
    virtual bool Draw() = 0;
    virtual ~GuiWindow() = default;
};

struct ProjectFilesWindow: GuiWindow
{
    ProjectFilesWindow(): GuiWindow( "Project Files" ) {}

    virtual bool Draw() override
    {
        ImGui::Text( "ProjectFilesWindow" );
        return true;
    }
};

struct EntitiesWindow: GuiWindow
{
    EntitiesWindow(): GuiWindow( "Entities" ) {}

    virtual bool Draw() override
    {
        ImGui::Text( "EntitiesWindow" );
        return true;
    }
};

struct InspectorWindow: GuiWindow
{
    InspectorWindow(): GuiWindow( "Inspector" ) {}

    virtual bool Draw() override
    {
        ImGui::Text( "InspectorWindow" );
        return true;
    }
};

struct LogWindow: GuiWindow
{
    LogWindow(): GuiWindow( "Log" ) {}

    virtual bool Draw() override
    {
        ImGui::Text( "LogWindow" );
        return true;
    }
};

struct MapWindow: GuiWindow
{
    FOMapper* MapInstance;

    MapWindow( std::string map_name ): GuiWindow( "Map" )
    {
        MapInstance = new FOMapper();
        if( !MapInstance->Init() )              // || !data->MapInstance->LoadMap)
            SAFEDEL( MapInstance );
    }

    virtual bool Draw() override
    {
        if( !MapInstance )
            return false;

        ImGui::Text( "MapWindow" );

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

struct ServerWindow: GuiWindow
{
    ServerWindow(): GuiWindow( "Server" ) {}

    virtual bool Draw() override
    {
        ImGui::Text( "ServerWindow" );
        return true;
    }
};

struct ClientWindow: GuiWindow
{
    ClientWindow(): GuiWindow( "Client" ) {}

    virtual bool Draw() override
    {
        ImGui::Text( "ClientWindow" );
        return true;
    }
};

static vector< GuiWindow* > Windows;
static vector< GuiWindow* > NewWindows;
static vector< GuiWindow* > CloseWindows;

extern "C" int main( int argc, char** argv ) // Handled by SDL
{
    InitialSetup( "FOnlineEditor", argc, argv );

    // Threading
    Thread::SetCurrentName( "GUI" );

    // Logging
    LogToFile( "FOnlineEditor.log" );
    LogToBuffer( true );
    WriteLog( "FOnline Editor v.{}.\n", FONLINE_VERSION );

    // Options
    // GetServerOptions();
    GetClientOptions();

    // Initialize Gui
    bool use_dx = ( MainConfig->GetInt( "", "UseDirectX" ) != 0 );
    if( !AppGui::Init( "FOnline Editor", use_dx, true, true ) )
        return -1;

    // Basic windows
    Windows.push_back( new ProjectFilesWindow() );
    Windows.push_back( new EntitiesWindow() );
    Windows.push_back( new InspectorWindow() );
    Windows.push_back( new LogWindow() );
    Windows.push_back( new ServerWindow() );
    Windows.push_back( new ServerWindow() );

    // Main loop
    while( !GameOpt.Quit )
    {
        if( !AppGui::BeginFrame() )
            break;

        DockSpaceBegin();

        for( GuiWindow* window : Windows )
        {
            bool keep_alive = true;
            if( ImGui::Begin( window->Name.c_str(), nullptr, ImGuiWindowFlags_NoCollapse ) )
                keep_alive = window->Draw();
            ImGui::End();

            if( !keep_alive )
                CloseWindows.push_back( window );
        }

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

        DockSpaceEnd();

        AppGui::EndFrame();
    }

    // Graceful shutdown
    // Other stuff will be reseted by operating system
    for( GuiWindow* window : Windows )
        delete window;

    return 0;
}

static void DockSpaceBegin()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos( viewport->Pos );
    ImGui::SetNextWindowSize( viewport->Size );
    ImGui::SetNextWindowViewport( viewport->ID );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
    ImGui::SetNextWindowBgAlpha( 0.0f );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
    ImGui::Begin( "DockSpace", nullptr, window_flags );
    ImGui::PopStyleVar();
    ImGui::PopStyleVar( 2 );

    ImGuiID dockspace_id = ImGui::GetID( "DockSpace" );
    if( !ImGui::DockBuilderGetNode( dockspace_id ) )
    {
        ImGui::DockBuilderRemoveNode( dockspace_id );
        ImGui::DockBuilderAddNode( dockspace_id, ImGuiDockNodeFlags_None );

        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_left1_id = ImGui::DockBuilderSplitNode( dock_main_id, ImGuiDir_Left, 0.15f, nullptr, &dock_main_id );
        ImGuiID dock_left2_id = ImGui::DockBuilderSplitNode( dock_main_id, ImGuiDir_Left, 0.1764f, nullptr, &dock_main_id );
        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode( dock_main_id, ImGuiDir_Right, 0.3f, nullptr, &dock_main_id );
        ImGuiID dock_down_id = ImGui::DockBuilderSplitNode( dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id );

        ImGui::DockBuilderDockWindow( "Project Files", dock_left1_id );
        ImGui::DockBuilderDockWindow( "Entities", dock_left2_id );
        ImGui::DockBuilderDockWindow( "Inspector", dock_right_id );
        ImGui::DockBuilderDockWindow( "Log", dock_down_id );
        ImGui::DockBuilderDockWindow( "Server", dock_main_id );
        ImGui::DockBuilderFinish( dock_main_id );
    }

    ImGui::DockSpace( dockspace_id, ImVec2( 0.0f, 0.0f ), ImGuiDockNodeFlags_PassthruCentralNode );
}

static void DockSpaceEnd()
{
    ImGui::End();
}
