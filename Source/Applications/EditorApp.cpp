#include "Common.h"

#include "AppGui.h"
#include "BuildSystem.h"
#include "Client.h"
#include "Log.h"
#include "MapView.h"
#include "Mapper.h"
#include "SDL_main.h"
#include "Server.h"
#include "Settings.h"
#include "Testing.h"
#include "Version_Include.h"

static GlobalSettings Settings {};

static void DockSpaceBegin();
static void DockSpaceEnd();

struct GuiWindow
{
    string Name;
    ImGuiWindowFlags AdditionalFlags = 0;
    GuiWindow(string name) : Name(name) {}
    virtual bool Draw() = 0;
    virtual ~GuiWindow() = default;
};

struct ProjectFilesWindow : GuiWindow
{
    string SelectedTree;
    int SelectedItem = -1;
    FileCollection Scripts;
    FileCollection Locations;
    FileCollection Maps;
    FileCollection Critters;
    FileCollection Items;
    FileCollection Dialogs;
    FileCollection Texts;

    ProjectFilesWindow(FileManager& file_mngr) :
        GuiWindow("Project Files"),
        Scripts(file_mngr.FilterFiles("fos")),
        Locations(file_mngr.FilterFiles("foloc")),
        Maps(file_mngr.FilterFiles("fomap")),
        Critters(file_mngr.FilterFiles("focr")),
        Items(file_mngr.FilterFiles("foitem")),
        Dialogs(file_mngr.FilterFiles("fodlg")),
        Texts(file_mngr.FilterFiles("msg"))
    {
    }

    virtual bool Draw() override
    {
        DrawFiles("Scripts", Scripts);
        DrawFiles("Locations", Locations);
        DrawFiles("Maps", Maps);
        DrawFiles("Critters", Critters);
        DrawFiles("Items", Items);
        DrawFiles("Dialogs", Dialogs);
        DrawFiles("Texts", Texts);
        return true;
    }

    void DrawFiles(const string& tree_name, FileCollection& files)
    {
        if (ImGui::TreeNode(tree_name.c_str()))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 3.0f);

            files.ResetCounter();
            uint index = 0;
            while (files.MoveNext())
            {
                FileHeader file_header = files.GetCurFileHeader();

                ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (SelectedItem == index && SelectedTree == tree_name)
                    node_flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx((void*)(intptr_t)index, node_flags, "%s", file_header.GetName().c_str());

                if (ImGui::IsItemClicked())
                {
                    SelectedTree = tree_name;
                    SelectedItem = index;
                }

                index++;
            }

            ImGui::PopStyleVar();
            ImGui::TreePop();
        }
    }
};
// static ProjectFilesWindow* ProjectFiles = nullptr;

struct StructureWindow : GuiWindow
{
    StructureWindow() : GuiWindow("Structure") {}

    virtual bool Draw() override
    {
        ImGui::Text("StructureWindow");
        return true;
    }
};

struct InspectorWindow : GuiWindow
{
    InspectorWindow() : GuiWindow("Inspector") {}

    virtual bool Draw() override
    {
        ImGui::Text("InspectorWindow");
        return true;
    }
};

struct LogWindow : GuiWindow
{
    string CurLog;
    string WholeLog;

    LogWindow() : GuiWindow("Log")
    {
        AdditionalFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar;
    }

    virtual bool Draw() override
    {
        LogGetBuffer(CurLog);
        if (!CurLog.empty())
        {
            WholeLog += CurLog;
            CurLog.clear();
            if (WholeLog.size() > 100000)
                WholeLog = WholeLog.substr(WholeLog.size() - 100000);
        }

        if (!WholeLog.empty())
            ImGui::TextUnformatted(WholeLog.c_str(), WholeLog.c_str() + WholeLog.size());

        return true;
    }
};

struct ServerWindow : GuiWindow
{
    ServerWindow() : GuiWindow("Server") {}

    virtual bool Draw() override
    {
        ImGui::ShowDemoWindow();
        return true;
    }
};

struct ClientWindow : GuiWindow
{
    ClientWindow() : GuiWindow("Client") {}

    virtual bool Draw() override
    {
        ImGui::Text("ClientWindow");
        return true;
    }
};

struct MapperWindow : GuiWindow
{
    FOMapper* Mapper;

    MapperWindow(std::string map_name) : GuiWindow("Map")
    {
        Mapper = new FOMapper(Settings);

        ProtoMap* pmap = new ProtoMap(_str(map_name).toHash());
        if (!pmap->EditorLoad(Mapper->ServerFileMngr, Mapper->ProtoMngr, Mapper->SprMngr, Mapper->ResMngr))
        {
            Mapper->AddMess("File not found or truncated.");
            return;
        }

        if (Mapper->ActiveMap)
            Mapper->HexMngr.GetProtoMap(*(ProtoMap*)Mapper->ActiveMap->Proto);

        if (!Mapper->HexMngr.SetProtoMap(*pmap))
        {
            Mapper->AddMess("Load map fail.");
            return;
        }

        Mapper->HexMngr.FindSetCenter(pmap->GetWorkHexX(), pmap->GetWorkHexY());

        MapView* map_view = new MapView(0, pmap);
        Mapper->ActiveMap = map_view;
        Mapper->LoadedMaps.push_back(map_view);
        Mapper->AddMess("Load map complete.");
        Mapper->RunMapLoadScript(map_view);
    }

    virtual bool Draw() override
    {
        /*if(Mapper->ActiveMap != WindowMap )
           {
            if( MapperInstance->ActiveMap )
                MapperInstance->HexMngr.GetProtoMap( *(ProtoMap*) MapperInstance->ActiveMap->Proto );
            if( !MapperInstance->HexMngr.SetProtoMap( *(ProtoMap*) WindowMap->Proto ) )
                return false;

            ProtoMap* pmap = (ProtoMap*) WindowMap->Proto;
            MapperInstance->HexMngr.FindSetCenter( pmap->GetWorkHexX(), pmap->GetWorkHexY() );
            MapperInstance->ActiveMap = WindowMap;
           }*/

        Mapper->MainLoop();
        return true;
    }
};

struct SettingsWindow : GuiWindow
{
    SettingsWindow() : GuiWindow("Settings") {}

    virtual bool Draw() override
    {
        if (ImGui::Button("Start"))
        {
            // ...
        }

        if (ImGui::Button("Build"))
        {
            // Generate resources
            // bool changed = ResourceConverter::Generate(resource_names);
        }

        Settings.Draw(true);
        return true;
    }
};

static vector<GuiWindow*> Windows;
static vector<GuiWindow*> NewWindows;
static vector<GuiWindow*> CloseWindows;

#ifndef FO_TESTING
extern "C" int main(int argc, char** argv) // Handled by SDL
#else
static int main_disabled(int argc, char** argv)
#endif
{
    CatchExceptions("FOnlineEditor", FO_VERSION);
    LogToFile("FOnlineEditor.log");
    Settings.ParseArgs(argc, argv);

    LogToBuffer(true);
    WriteLog("FOnline Editor ({:#x}).\n", FO_VERSION);

    // Initialize Gui
    bool use_dx = !Settings.OpenGLRendering;
    if (!AppGui::Init("FOnline Editor", use_dx, true, true))
        return -1;

    // Basic windows
    FileManager file_mngr {};
    Windows.push_back(new ProjectFilesWindow(file_mngr));
    Windows.push_back(new StructureWindow());
    Windows.push_back(new InspectorWindow());
    Windows.push_back(new LogWindow());
    Windows.push_back(new SettingsWindow());

    // Main loop
    while (!Settings.Quit)
    {
        if (!AppGui::BeginFrame())
            break;

        DockSpaceBegin();

        for (GuiWindow* window : Windows)
        {
            bool keep_alive = true;
            if (ImGui::Begin(window->Name.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | window->AdditionalFlags))
                keep_alive = window->Draw();
            ImGui::End();

            if (!keep_alive)
                CloseWindows.push_back(window);
        }

        if (!NewWindows.empty())
        {
            Windows.insert(Windows.end(), NewWindows.begin(), NewWindows.end());
            NewWindows.clear();
        }

        while (!CloseWindows.empty())
        {
            auto it = std::find(Windows.begin(), Windows.end(), CloseWindows.back());
            RUNTIME_ASSERT(it != Windows.end());
            GuiWindow* window = *it;
            Windows.erase(it);
            CloseWindows.pop_back();
            delete window;
        }

        if (Windows.empty())
            break;

        DockSpaceEnd();

        AppGui::EndFrame();
    }

    // Graceful shutdown
    // Other stuff will be reseted by operating system
    for (GuiWindow* window : Windows)
        delete window;

    return 0;
}

static void DockSpaceBegin()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    if (!ImGui::DockBuilderGetNode(dockspace_id))
    {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_None);

        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.15f, nullptr, &dock_main_id);
        ImGuiID dock_left_left_id =
            ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.177f, nullptr, &dock_main_id);
        ImGuiID dock_right_id =
            ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
        ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

        ImGui::DockBuilderDockWindow("Project Files", dock_left_id);
        ImGui::DockBuilderDockWindow("Inspector", dock_right_id);
        ImGui::DockBuilderDockWindow("Structure", dock_left_left_id);
        ImGui::DockBuilderDockWindow("Log", dock_down_id);
        ImGui::DockBuilderDockWindow("Server", dock_main_id);
        ImGui::DockBuilderDockWindow("Client", dock_main_id);
        ImGui::DockBuilderDockWindow("Mapper", dock_main_id);
        ImGui::DockBuilderDockWindow("Settings", dock_main_id);
        ImGui::DockBuilderFinish(dock_main_id);
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
}

static void DockSpaceEnd()
{
    ImGui::End();
}
