#include "SDL_main.h"
#include "Common.h"
#include "Testing.h"
#include "Server.h"
#include "Timer.h"
#include "NetBuffer.h"
#include "IniFile.h"
#include "StringUtils.h"
#include "AppGui.h"

static FOServer* Server;
static Thread    ServerThread;
static bool      StartServer;

static void ServerEntry( void* )
{
    while( !StartServer )
        Thread::Sleep( 10 );

    Server = new FOServer();
    Server->Run();
    FOServer* server = Server;
    Server = nullptr;
    delete server;
}

#ifndef FO_TESTING
extern "C" int main( int argc, char** argv ) // Handled by SDL
#else
static int main_disabled( int argc, char** argv )
#endif
{
    Thread::SetCurrentName( "ServerGui" );
    LogToFile( "FOnlineServer.log" );
    LogToBuffer( true );
    InitialSetup( "FOnlineServer", argc, argv );

    // Gui
    bool use_dx = ( MainConfig->GetInt( "", "UseDirectX" ) != 0 );
    if( !AppGui::Init( "FOnline Server", use_dx, false, false ) )
        return -1;

    // Autostart
    if( !MainConfig->IsKey( "", "NoStart" ) )
        StartServer = true;

    // Server loop in separate thread
    ServerThread.Start( ServerEntry, "Server" );
    while( StartServer && !Server )
        Thread::Sleep( 0 );

    // Gui loop
    while( !StartServer || Server )
    {
        if( !AppGui::BeginFrame() )
        {
            // Immediate finish
            if( !StartServer || !Server || ( !Server->Started() && !Server->Stopping() ) )
                ExitProcess( 0 );

            // Graceful finish
            GameOpt.Quit = true;
        }

        if( !StartServer )
        {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos( ImVec2( io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f ), ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
            if( ImGui::Begin( "---", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse ) )
            {
                if( ImGui::Button( "Start server", ImVec2( 200, 30 ) ) )
                {
                    StartServer = true;
                    while( !Server )
                        Thread::Sleep( 0 );
                }
            }
            ImGui::End();
        }
        else
        {
            Server->DrawGui();
        }

        AppGui::EndFrame();
    }

    // Wait server finish
    if( StartServer )
        ServerThread.Wait();

    return 0;
}
