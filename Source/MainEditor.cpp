#include "Common.h"
#include "Mapper.h"
#include "Exception.h"
#include "imgui/imgui.h"

extern "C" int main( int argc, char** argv ) // Handled by SDL
{
    ImGuiIO& io = ImGui::GetIO();

    InitialSetup( argc, argv );

    // Threading
    Thread::SetCurrentName( "GUI" );

    // Exceptions
    CatchExceptions( "FOnlineEditor", FONLINE_VERSION );

    // Timer
    Timer::Init();

    // Logging
    LogToFile( "FOnlineEditor.log" );

    // Options
    GetClientOptions();

    return 0;
}
