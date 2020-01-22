#include "Common.h"

#include "AppGui.h"
#include "NetBuffer.h"
#include "SDL_main.h"
#include "Server.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "Version_Include.h"

static GlobalSettings Settings {};
static FOServer* Server;
static std::thread ServerThread;
static std::atomic_bool StartServer;

static void ServerEntry()
{
    while (!StartServer)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Server = new FOServer(Settings);
    Server->Run();
    FOServer* server = Server;
    Server = nullptr;
    delete server;
}

#ifndef FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineServer", FO_VERSION);
    LogToFile("FOnlineServer.log");
    Settings.ParseArgs(argc, argv);

    // Gui
    bool use_dx = !Settings.OpenGLRendering;
    if (!AppGui::Init("FOnline Server", use_dx, false, false))
        return -1;

    // Autostart
    if (!Settings.NoStart)
        StartServer = true;

    // Server loop in separate thread
    ServerThread = std::thread(ServerEntry);
    while (StartServer && !Server)
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

    // Gui loop
    while (!StartServer || Server)
    {
        if (!AppGui::BeginFrame())
        {
            // Immediate finish
            if (!StartServer || !Server || (!Server->Started() && !Server->Stopping()))
                exit(0);

            // Graceful finish
            Settings.Quit = true;
        }

        if (!StartServer)
        {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (ImGui::Begin("---", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
            {
                if (ImGui::Button("Start server", ImVec2(200, 30)))
                {
                    StartServer = true;
                    while (!Server)
                        std::this_thread::sleep_for(std::chrono::milliseconds(0));
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
    if (ServerThread.joinable())
        ServerThread.join();

    return 0;
}
