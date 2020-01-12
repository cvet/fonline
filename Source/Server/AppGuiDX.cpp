#include "AppGui.h"

#ifndef FO_SERVER_NO_GUI
#ifdef FO_HAVE_DX

#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"
#include "WinApi_Include.h"

#include <XInput.h>
#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <imm.h>

struct CustomVertex
{
    float Position[3];
    D3DCOLOR Color;
    float UV[2];
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

struct ImGuiViewportDataWin32
{
    HWND Hwnd;
    bool HwndOwned;
    DWORD DwStyle;
    DWORD DwExStyle;

    ImGuiViewportDataWin32()
    {
        Hwnd = nullptr;
        HwndOwned = false;
        DwStyle = DwExStyle = 0;
    }

    ~ImGuiViewportDataWin32() { RUNTIME_ASSERT(Hwnd == nullptr); }
};

struct ImGuiViewportDataDx9
{
    IDirect3DSwapChain9* SwapChain;
    D3DPRESENT_PARAMETERS D3dPP;

    ImGuiViewportDataDx9()
    {
        SwapChain = nullptr;
        ZeroMemory(&D3dPP, sizeof(D3DPRESENT_PARAMETERS));
    }

    ~ImGuiViewportDataDx9() { RUNTIME_ASSERT(SwapChain == nullptr); }
};

static HWND WndHandle = nullptr;
static INT64 CurTime = 0;
static INT64 TicksPerSecond = 0;
static ImGuiMouseCursor LastMouseCursor = ImGuiMouseCursor_COUNT;
static bool WantUpdateMonitors = true;
static LPDIRECT3D9 D3D = nullptr;
static D3DPRESENT_PARAMETERS D3dPP = {};
static LPDIRECT3DDEVICE9 D3dDevice = nullptr;
static LPDIRECT3DVERTEXBUFFER9 VertexBuffer = nullptr;
static LPDIRECT3DINDEXBUFFER9 IndexBuffer = nullptr;
static LPDIRECT3DTEXTURE9 FontTexture = nullptr;
static int VertexBufferSize = 5000;
static int IndexBufferSize = 10000;
static std::wstring WndClassName;
static std::wstring WndSubClassName;

static LRESULT WINAPI WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static bool CreateDevice(HWND hwnd);
static void ResetDevice();
static void RenderDrawData(ImDrawData* draw_data);
static void SetupRenderState(ImDrawData* draw_data);
static bool CreateDeviceObjects();
static bool UpdateMouseCursor();
static BOOL IsWindowsVersionOrGreater(WORD major, WORD minor, WORD sp);
static void EnableDpiAwareness();
static float GetDpiScaleForMonitor(void* monitor);
static float GetDpiScaleForHwnd(void* hwnd);
static void GetWin32StyleFromViewportFlags(ImGuiViewportFlags flags, DWORD* out_style, DWORD* out_ex_style);
static void UpdateMonitors();
static BOOL CALLBACK UpdateMonitorsEnumFunc(HMONITOR monitor, HDC, LPRECT, LPARAM);
static void Platform_CreateWindow(ImGuiViewport* viewport);
static void Platform_DestroyWindow(ImGuiViewport* viewport);
static void Platform_ShowWindow(ImGuiViewport* viewport);
static void Platform_UpdateWindow(ImGuiViewport* viewport);
static ImVec2 Platform_GetWindowPos(ImGuiViewport* viewport);
static void Platform_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos);
static ImVec2 Platform_GetWindowSize(ImGuiViewport* viewport);
static void Platform_SetWindowSize(ImGuiViewport* viewport, ImVec2 size);
static void Platform_SetWindowFocus(ImGuiViewport* viewport);
static bool Platform_GetWindowFocus(ImGuiViewport* viewport);
static bool Platform_GetWindowMinimized(ImGuiViewport* viewport);
static void Platform_SetWindowTitle(ImGuiViewport* viewport, const char* title);
static void Platform_SetWindowAlpha(ImGuiViewport* viewport, float alpha);
static float Platform_GetWindowDpiScale(ImGuiViewport* viewport);
static void Platform_OnChangedViewport(ImGuiViewport* viewport);
static void Platform_SetImeInputPos(ImGuiViewport* viewport, ImVec2 pos);
static void Renderer_CreateWindow(ImGuiViewport* viewport);
static void Renderer_DestroyWindow(ImGuiViewport* viewport);
static void Renderer_SetWindowSize(ImGuiViewport* viewport, ImVec2 size);
static void Renderer_RenderWindow(ImGuiViewport* viewport, void*);
static void Renderer_SwapBuffers(ImGuiViewport* viewport, void*);

bool AppGui::InitDX(const string& app_name, bool docking, bool maximized)
{
    EnableDpiAwareness();

    // Calculate window size
    RECT rect = {0, 0, 1024, 768};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create application window
    WndClassName = _str(app_name).toWideChar();
    WndSubClassName = WndClassName + L" Sub";
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WndProcHandler, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr,
        nullptr, nullptr, _str(app_name).toWideChar().c_str(), nullptr};
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, WndClassName.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDevice(hwnd))
    {
        WriteLog("Failed to create D3D device.\n");
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    // Performance counters
    if (!QueryPerformanceFrequency((LARGE_INTEGER*)&TicksPerSecond) ||
        !QueryPerformanceCounter((LARGE_INTEGER*)&CurTime))
    {
        WriteLog("Failed to call QueryPerformanceFrequency.\n");
        return false;
    }

    // Show the window
    ShowWindow(hwnd, maximized ? SW_SHOWMAXIMIZED : SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.WantSaveIniSettings = false;
    io.IniFilename = nullptr;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (docking)
    {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    // Setup back-end capabilities flags
    WndHandle = hwnd;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    // Setup Dear ImGui style
    // ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();
    ImGui::StyleColorsLight();

    // Keyboard mapping
    io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Space] = VK_SPACE;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io.KeyMap[ImGuiKey_KeyPadEnter] = VK_RETURN;
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    // Setup viewport stuff
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = main_viewport->PlatformHandleRaw = (void*)WndHandle;

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;

        WNDCLASSEX wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProcHandler;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = GetModuleHandle(nullptr);
        wcex.hIcon = nullptr;
        wcex.hCursor = nullptr;
        wcex.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = WndSubClassName.c_str();
        wcex.hIconSm = nullptr;
        RegisterClassEx(&wcex);

        UpdateMonitors();

        // Register platform/renderer interface
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        platform_io.Platform_CreateWindow = Platform_CreateWindow;
        platform_io.Platform_DestroyWindow = Platform_DestroyWindow;
        platform_io.Platform_ShowWindow = Platform_ShowWindow;
        platform_io.Platform_SetWindowPos = Platform_SetWindowPos;
        platform_io.Platform_GetWindowPos = Platform_GetWindowPos;
        platform_io.Platform_SetWindowSize = Platform_SetWindowSize;
        platform_io.Platform_GetWindowSize = Platform_GetWindowSize;
        platform_io.Platform_SetWindowFocus = Platform_SetWindowFocus;
        platform_io.Platform_GetWindowFocus = Platform_GetWindowFocus;
        platform_io.Platform_GetWindowMinimized = Platform_GetWindowMinimized;
        platform_io.Platform_SetWindowTitle = Platform_SetWindowTitle;
        platform_io.Platform_SetWindowAlpha = Platform_SetWindowAlpha;
        platform_io.Platform_UpdateWindow = Platform_UpdateWindow;
        platform_io.Platform_GetWindowDpiScale = Platform_GetWindowDpiScale;
        platform_io.Platform_OnChangedViewport = Platform_OnChangedViewport;
        platform_io.Platform_SetImeInputPos = Platform_SetImeInputPos;
        platform_io.Renderer_CreateWindow = Renderer_CreateWindow;
        platform_io.Renderer_DestroyWindow = Renderer_DestroyWindow;
        platform_io.Renderer_SetWindowSize = Renderer_SetWindowSize;
        platform_io.Renderer_RenderWindow = Renderer_RenderWindow;
        platform_io.Renderer_SwapBuffers = Renderer_SwapBuffers;

        // Register main window handle
        ImGuiViewportDataWin32* viewport_data = IM_NEW(ImGuiViewportDataWin32)();
        viewport_data->Hwnd = WndHandle;
        viewport_data->HwndOwned = false;
        main_viewport->PlatformUserData = viewport_data;
    }

    CreateDeviceObjects();

    return true;
}

bool AppGui::BeginFrameDX()
{
    // Handle window messages
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    bool quit = false;
    while (true)
    {
        if (!PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
            break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_QUIT)
            quit = true;
    }

    // Start the Dear ImGui frame
    ImGuiIO& io = ImGui::GetIO();

    // Setup display size
    RECT rect;
    GetClientRect(WndHandle, &rect);
    io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
    if (WantUpdateMonitors)
        UpdateMonitors();

    // Setup time step
    INT64 current_time;
    QueryPerformanceCounter((LARGE_INTEGER*)&current_time);
    io.DeltaTime = (float)(current_time - CurTime) / TicksPerSecond;
    CurTime = current_time;

    // Read keyboard modifiers inputs
    io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    io.KeySuper = false;

    // Update mouse position
    if (io.WantSetMousePos)
    {
        POINT pos = {(int)io.MousePos.x, (int)io.MousePos.y};
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
            ClientToScreen(WndHandle, &pos);
        SetCursorPos(pos.x, pos.y);
    }

    // Set mouse position
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    io.MouseHoveredViewport = 0;

    // Set imgui mouse position
    POINT mouse_screen_pos;
    if (GetCursorPos(&mouse_screen_pos))
    {
        if (HWND focused_hwnd = GetForegroundWindow())
        {
            if (IsChild(focused_hwnd, WndHandle))
                focused_hwnd = WndHandle;

            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                if (ImGui::FindViewportByPlatformHandle((void*)focused_hwnd) != nullptr)
                    io.MousePos = ImVec2((float)mouse_screen_pos.x, (float)mouse_screen_pos.y);
            }
            else
            {
                if (focused_hwnd == WndHandle)
                {
                    POINT mouse_client_pos = mouse_screen_pos;
                    ScreenToClient(focused_hwnd, &mouse_client_pos);
                    io.MousePos = ImVec2((float)mouse_client_pos.x, (float)mouse_client_pos.y);
                }
            }
        }

        if (HWND hovered_hwnd = WindowFromPoint(mouse_screen_pos))
            if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)hovered_hwnd))
                if ((viewport->Flags & ImGuiViewportFlags_NoInputs) == 0)
                    io.MouseHoveredViewport = viewport->ID;

        // Update OS mouse cursor with the cursor requested by imgui
        ImGuiMouseCursor mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
        if (LastMouseCursor != mouse_cursor)
        {
            LastMouseCursor = mouse_cursor;
            UpdateMouseCursor();
        }
    }

    ImGui::NewFrame();

    return !quit;
}

void AppGui::EndFrameDX()
{
    ImGui::EndFrame();

    D3dDevice->SetRenderState(D3DRS_ZENABLE, false);
    D3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
    D3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
    static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * 255.0f), (int)(clear_color.y * 255.0f),
        (int)(clear_color.z * 255.0f), (int)(clear_color.w * 255.0f));
    D3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
    if (D3dDevice->BeginScene() >= 0)
    {
        ImGui::Render();
        RenderDrawData(ImGui::GetDrawData());
        D3dDevice->EndScene();
    }

    // Update and Render additional Platform Windows
    ImGuiIO& io = ImGui::GetIO();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    HRESULT result = D3dDevice->Present(nullptr, nullptr, nullptr, nullptr);

    // Handle loss of D3D9 device
    if (result == D3DERR_DEVICELOST && D3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
        ResetDevice();
}

static LRESULT WINAPI WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
// Allow compilation with old Windows SDK
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef DBT_DEVNODES_CHANGED
#define DBT_DEVNODES_CHANGED 0x0007
#endif

    switch (msg)
    {
    case WM_SIZE:
        if (D3dDevice != nullptr && wparam != SIZE_MINIMIZED)
        {
            D3dPP.BackBufferWidth = LOWORD(lparam);
            D3dPP.BackBufferHeight = HIWORD(lparam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            const RECT* suggested_rect = (RECT*)lparam;
            SetWindowPos(hwnd, nullptr, suggested_rect->left, suggested_rect->top,
                suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }

    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiIO& io = ImGui::GetIO();

        switch (msg)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK: {
            int button = 0;
            if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)
                button = 0;
            else if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK)
                button = 1;
            else if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK)
                button = 2;
            else if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK)
                button = (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? 3 : 4;
            else if (!ImGui::IsAnyMouseDown() && GetCapture() == nullptr)
                SetCapture(hwnd);
            io.MouseDown[button] = true;
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP: {
            int button = 0;
            if (msg == WM_LBUTTONUP)
                button = 0;
            else if (msg == WM_RBUTTONUP)
                button = 1;
            else if (msg == WM_MBUTTONUP)
                button = 2;
            else if (msg == WM_XBUTTONUP)
                button = (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? 3 : 4;
            io.MouseDown[button] = false;
            if (!ImGui::IsAnyMouseDown() && GetCapture() == hwnd)
                ReleaseCapture();
            return 0;
        }
        case WM_MOUSEWHEEL:
            io.MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
            return 0;
        case WM_MOUSEHWHEEL:
            io.MouseWheelH += (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wparam < 256)
                io.KeysDown[wparam] = true;
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wparam < 256)
                io.KeysDown[wparam] = false;
            return 0;
        case WM_CHAR:
            io.AddInputCharacter((unsigned int)wparam);
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT && UpdateMouseCursor())
                return 1;
            return 0;
        case WM_DISPLAYCHANGE:
            WantUpdateMonitors = true;
            return 0;
        }

        if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)hwnd))
        {
            switch (msg)
            {
            case WM_CLOSE:
                viewport->PlatformRequestClose = true;
                return 0;
            case WM_MOVE:
                viewport->PlatformRequestMove = true;
                break;
            case WM_SIZE:
                viewport->PlatformRequestResize = true;
                break;
            case WM_MOUSEACTIVATE:
                if (viewport->Flags & ImGuiViewportFlags_NoFocusOnClick)
                    return MA_NOACTIVATE;
                break;
            case WM_NCHITTEST:
                if (viewport->Flags & ImGuiViewportFlags_NoInputs)
                    return HTTRANSPARENT;
                break;
            }
        }
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static bool CreateDevice(HWND hwnd)
{
    if ((D3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    memset(&D3dPP, 0, sizeof(D3dPP));
    D3dPP.Windowed = TRUE;
    D3dPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
    D3dPP.BackBufferFormat = D3DFMT_UNKNOWN;
    D3dPP.EnableAutoDepthStencil = TRUE;
    D3dPP.AutoDepthStencilFormat = D3DFMT_D16;
    D3dPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (D3D->CreateDevice(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &D3dPP, &D3dDevice) < 0)
        return false;

    return true;
}

static void ResetDevice()
{
    HRESULT hr = D3dDevice->Reset(&D3dPP);
    RUNTIME_ASSERT(hr != D3DERR_INVALIDCALL);

    if (ImGui::GetCurrentContext() != nullptr)
        CreateDeviceObjects();
}

static bool CreateDeviceObjects()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height, bytes_per_pixel;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);

    // Upload texture to graphics system
    FontTexture = nullptr;
    if (D3dDevice->CreateTexture(
            width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &FontTexture, nullptr) < 0)
        return false;

    D3DLOCKED_RECT tex_locked_rect;
    if (FontTexture->LockRect(0, &tex_locked_rect, nullptr, 0) != D3D_OK)
        return false;
    for (int y = 0; y < height; y++)
        memcpy((unsigned char*)tex_locked_rect.pBits + tex_locked_rect.Pitch * y,
            pixels + (width * bytes_per_pixel) * y, (width * bytes_per_pixel));
    FontTexture->UnlockRect(0);

    // Store our identifier
    io.Fonts->TexID = (ImTextureID)FontTexture;

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    for (int i = 1; i < platform_io.Viewports.Size; i++)
        if (!platform_io.Viewports[i]->RendererUserData)
            Renderer_CreateWindow(platform_io.Viewports[i]);

    return true;
}

static void RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // Create and grow buffers if needed
    if (!VertexBuffer || VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (VertexBuffer)
        {
            VertexBuffer->Release();
            VertexBuffer = nullptr;
        }
        VertexBufferSize = draw_data->TotalVtxCount + 5000;
        if (D3dDevice->CreateVertexBuffer(VertexBufferSize * sizeof(CustomVertex),
                D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &VertexBuffer,
                nullptr) < 0)
            return;
    }
    if (!IndexBuffer || IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (IndexBuffer)
        {
            IndexBuffer->Release();
            IndexBuffer = nullptr;
        }
        IndexBufferSize = draw_data->TotalIdxCount + 10000;
        if (D3dDevice->CreateIndexBuffer(IndexBufferSize * sizeof(CustomVertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &IndexBuffer, nullptr) < 0)
            return;
    }

    // Backup the DX9 state
    IDirect3DStateBlock9* d3d9_state_block = nullptr;
    if (D3dDevice->CreateStateBlock(D3DSBT_ALL, &d3d9_state_block) < 0)
        return;

    // Backup the DX9 transform
    D3DMATRIX last_world, last_view, last_projection;
    D3dDevice->GetTransform(D3DTS_WORLD, &last_world);
    D3dDevice->GetTransform(D3DTS_VIEW, &last_view);
    D3dDevice->GetTransform(D3DTS_PROJECTION, &last_projection);

    // Copy and convert all vertices into a single contiguous buffer, convert colors to DX9 default format
    CustomVertex* vtx_dst;
    ImDrawIdx* idx_dst;
    if (VertexBuffer->Lock(
            0, (UINT)(draw_data->TotalVtxCount * sizeof(CustomVertex)), (void**)&vtx_dst, D3DLOCK_DISCARD) < 0)
        return;
    if (IndexBuffer->Lock(0, (UINT)(draw_data->TotalIdxCount * sizeof(ImDrawIdx)), (void**)&idx_dst, D3DLOCK_DISCARD) <
        0)
        return;

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_src = cmd_list->VtxBuffer.Data;
        for (int i = 0; i < cmd_list->VtxBuffer.Size; i++)
        {
            vtx_dst->Position[0] = vtx_src->pos.x;
            vtx_dst->Position[1] = vtx_src->pos.y;
            vtx_dst->Position[2] = 0.0f;
            vtx_dst->Color =
                (vtx_src->col & 0xFF00FF00) | ((vtx_src->col & 0xFF0000) >> 16) | ((vtx_src->col & 0xFF) << 16);
            vtx_dst->UV[0] = vtx_src->uv.x;
            vtx_dst->UV[1] = vtx_src->uv.y;
            vtx_dst++;
            vtx_src++;
        }
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        idx_dst += cmd_list->IdxBuffer.Size;
    }

    VertexBuffer->Unlock();
    IndexBuffer->Unlock();

    D3dDevice->SetStreamSource(0, VertexBuffer, 0, sizeof(CustomVertex));
    D3dDevice->SetIndices(IndexBuffer);
    D3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);

    // Setup desired DX state
    SetupRenderState(draw_data);

    // Render command lists
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                const RECT r = {(LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y),
                    (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y)};
                const LPDIRECT3DTEXTURE9 texture = (LPDIRECT3DTEXTURE9)pcmd->TextureId;
                D3dDevice->SetTexture(0, texture);
                D3dDevice->SetScissorRect(&r);
                D3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, pcmd->VtxOffset + global_vtx_offset, 0,
                    (UINT)cmd_list->VtxBuffer.Size, pcmd->IdxOffset + global_idx_offset, pcmd->ElemCount / 3);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Restore the DX9 transform
    D3dDevice->SetTransform(D3DTS_WORLD, &last_world);
    D3dDevice->SetTransform(D3DTS_VIEW, &last_view);
    D3dDevice->SetTransform(D3DTS_PROJECTION, &last_projection);

    // Restore the DX9 state
    d3d9_state_block->Apply();
    d3d9_state_block->Release();
}

static void SetupRenderState(ImDrawData* draw_data)
{
    // Setup viewport
    D3DVIEWPORT9 vp;
    vp.X = vp.Y = 0;
    vp.Width = (DWORD)draw_data->DisplaySize.x;
    vp.Height = (DWORD)draw_data->DisplaySize.y;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    D3dDevice->SetViewport(&vp);

    // Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing, shade mode (for gradient)
    D3dDevice->SetPixelShader(nullptr);
    D3dDevice->SetVertexShader(nullptr);
    D3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    D3dDevice->SetRenderState(D3DRS_LIGHTING, false);
    D3dDevice->SetRenderState(D3DRS_ZENABLE, false);
    D3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
    D3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, false);
    D3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    D3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    D3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    D3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
    D3dDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    D3dDevice->SetRenderState(D3DRS_FOGENABLE, false);
    D3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    D3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    D3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    D3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    D3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    D3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    D3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    // Setup orthographic projection matrix
    float l = draw_data->DisplayPos.x + 0.5f;
    float r = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
    float t = draw_data->DisplayPos.y + 0.5f;
    float b = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;
    D3DMATRIX mat_identity = {
        {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}}};
    D3DMATRIX mat_projection = {{{2.0f / (r - l), 0.0f, 0.0f, 0.0f, 0.0f, 2.0f / (t - b), 0.0f, 0.0f, 0.0f, 0.0f, 0.5f,
        0.0f, (l + r) / (l - r), (t + b) / (b - t), 0.5f, 1.0f}}};
    D3dDevice->SetTransform(D3DTS_WORLD, &mat_identity);
    D3dDevice->SetTransform(D3DTS_VIEW, &mat_identity);
    D3dDevice->SetTransform(D3DTS_PROJECTION, &mat_projection);
}

static bool UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return false;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
    {
        SetCursor(nullptr);
    }
    else
    {
        LPTSTR win32_cursor = IDC_ARROW;
        switch (imgui_cursor)
        {
        case ImGuiMouseCursor_Arrow:
            win32_cursor = IDC_ARROW;
            break;
        case ImGuiMouseCursor_TextInput:
            win32_cursor = IDC_IBEAM;
            break;
        case ImGuiMouseCursor_ResizeAll:
            win32_cursor = IDC_SIZEALL;
            break;
        case ImGuiMouseCursor_ResizeEW:
            win32_cursor = IDC_SIZEWE;
            break;
        case ImGuiMouseCursor_ResizeNS:
            win32_cursor = IDC_SIZENS;
            break;
        case ImGuiMouseCursor_ResizeNESW:
            win32_cursor = IDC_SIZENESW;
            break;
        case ImGuiMouseCursor_ResizeNWSE:
            win32_cursor = IDC_SIZENWSE;
            break;
        case ImGuiMouseCursor_Hand:
            win32_cursor = IDC_HAND;
            break;
        }

        SetCursor(LoadCursor(nullptr, win32_cursor));
    }
    return true;
}

static BOOL IsWindowsVersionOrGreater(WORD major, WORD minor, WORD sp)
{
    OSVERSIONINFOEXW osvi = {sizeof(osvi), major, minor, 0, 0, {0}, sp};
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    ULONGLONG cond = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    return VerifyVersionInfoW(&osvi, mask, cond);
}
#define IsWindows8Point1OrGreater() IsWindowsVersionOrGreater(HIBYTE(0x0602), LOBYTE(0x0602), 0) // _WIN32_WINNT_WINBLUE
#define IsWindows10OrGreater() IsWindowsVersionOrGreater(HIBYTE(0x0A00), LOBYTE(0x0A00), 0) // _WIN32_WINNT_WIN10

#ifndef DPI_ENUMS_DECLARED
typedef enum
{
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
typedef enum
{
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#endif
#ifndef _DPI_AWARENESS_CONTEXTS_
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE (DPI_AWARENESS_CONTEXT) - 3
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 (DPI_AWARENESS_CONTEXT) - 4
#endif
typedef HRESULT(WINAPI* PFN_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS); // Shcore.lib+dll, Windows 8.1
typedef HRESULT(WINAPI* PFN_GetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*); // Shcore.lib+dll, Windows 8.1
typedef DPI_AWARENESS_CONTEXT(WINAPI* PFN_SetThreadDpiAwarenessContext)(
    DPI_AWARENESS_CONTEXT); // User32.lib+dll, Windows 10 v1607 (Creators Update)

static void EnableDpiAwareness()
{
    // if (IsWindows10OrGreater())
    {
        static HINSTANCE user32_dll = LoadLibraryA("user32.dll");
        if (PFN_SetThreadDpiAwarenessContext SetThreadDpiAwarenessContextFn =
                (PFN_SetThreadDpiAwarenessContext)GetProcAddress(user32_dll, "SetThreadDpiAwarenessContext"))
        {
            SetThreadDpiAwarenessContextFn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    if (IsWindows8Point1OrGreater())
    {
        static HINSTANCE shcore_dll = LoadLibraryA("shcore.dll");
        if (PFN_SetProcessDpiAwareness SetProcessDpiAwarenessFn =
                (PFN_SetProcessDpiAwareness)GetProcAddress(shcore_dll, "SetProcessDpiAwareness"))
            SetProcessDpiAwarenessFn(PROCESS_PER_MONITOR_DPI_AWARE);
    }
    else
    {
        SetProcessDPIAware();
    }
}

static float GetDpiScaleForMonitor(void* monitor)
{
    UINT xdpi = 96, ydpi = 96;
    if (IsWindows8Point1OrGreater())
    {
        static HINSTANCE shcore_dll = LoadLibraryA("shcore.dll");
        if (PFN_GetDpiForMonitor GetDpiForMonitorFn =
                (PFN_GetDpiForMonitor)GetProcAddress(shcore_dll, "GetDpiForMonitor"))
            GetDpiForMonitorFn((HMONITOR)monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
    }
    else
    {
        const HDC dc = GetDC(nullptr);
        xdpi = GetDeviceCaps(dc, LOGPIXELSX);
        ydpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
    }
    RUNTIME_ASSERT(xdpi == ydpi);
    return xdpi / 96.0f;
}

static float GetDpiScaleForHwnd(void* hwnd)
{
    HMONITOR monitor = MonitorFromWindow((HWND)hwnd, MONITOR_DEFAULTTONEAREST);
    return GetDpiScaleForMonitor(monitor);
}

static void GetWin32StyleFromViewportFlags(ImGuiViewportFlags flags, DWORD* out_style, DWORD* out_ex_style)
{
    if (flags & ImGuiViewportFlags_NoDecoration)
        *out_style = WS_POPUP;
    else
        *out_style = WS_OVERLAPPEDWINDOW;

    if (flags & ImGuiViewportFlags_NoTaskBarIcon)
        *out_ex_style = WS_EX_TOOLWINDOW;
    else
        *out_ex_style = WS_EX_APPWINDOW;

    if (flags & ImGuiViewportFlags_TopMost)
        *out_ex_style |= WS_EX_TOPMOST;
}

static void Platform_CreateWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = IM_NEW(ImGuiViewportDataWin32)();
    viewport->PlatformUserData = data;

    // Select style and parent window
    GetWin32StyleFromViewportFlags(viewport->Flags, &data->DwStyle, &data->DwExStyle);
    HWND parent_window = nullptr;
    if (viewport->ParentViewportId != 0)
        if (ImGuiViewport* parent_viewport = ImGui::FindViewportByID(viewport->ParentViewportId))
            parent_window = (HWND)parent_viewport->PlatformHandle;

    // Create window
    RECT rect = {(LONG)viewport->Pos.x, (LONG)viewport->Pos.y, (LONG)(viewport->Pos.x + viewport->Size.x),
        (LONG)(viewport->Pos.y + viewport->Size.y)};
    AdjustWindowRectEx(&rect, data->DwStyle, FALSE, data->DwExStyle);
    data->Hwnd = CreateWindowEx(data->DwExStyle, WndSubClassName.c_str(), L"Child",
        data->DwStyle, // Style, class name, window name
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, // Window area
        parent_window, nullptr, GetModuleHandle(nullptr), nullptr); // Parent window, Menu, Instance, Param
    data->HwndOwned = true;
    viewport->PlatformRequestResize = false;
    viewport->PlatformHandle = viewport->PlatformHandleRaw = data->Hwnd;
}

static void Platform_DestroyWindow(ImGuiViewport* viewport)
{
    if (ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData)
    {
        if (GetCapture() == data->Hwnd)
        {
            ReleaseCapture();
            SetCapture(WndHandle);
        }
        if (data->Hwnd && data->HwndOwned)
            DestroyWindow(data->Hwnd);
        data->Hwnd = nullptr;
        IM_DELETE(data);
    }
    viewport->PlatformUserData = viewport->PlatformHandle = nullptr;
}

static void Platform_ShowWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
        ShowWindow(data->Hwnd, SW_SHOWNA);
    else
        ShowWindow(data->Hwnd, SW_SHOW);
}

static void Platform_UpdateWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    DWORD new_style;
    DWORD new_ex_style;
    GetWin32StyleFromViewportFlags(viewport->Flags, &new_style, &new_ex_style);

    if (data->DwStyle != new_style || data->DwExStyle != new_ex_style)
    {
        data->DwStyle = new_style;
        data->DwExStyle = new_ex_style;
        SetWindowLong(data->Hwnd, GWL_STYLE, data->DwStyle);
        SetWindowLong(data->Hwnd, GWL_EXSTYLE, data->DwExStyle);
        RECT rect = {(LONG)viewport->Pos.x, (LONG)viewport->Pos.y, (LONG)(viewport->Pos.x + viewport->Size.x),
            (LONG)(viewport->Pos.y + viewport->Size.y)};
        AdjustWindowRectEx(&rect, data->DwStyle, FALSE, data->DwExStyle); // Client to Screen
        SetWindowPos(data->Hwnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        ShowWindow(data->Hwnd, SW_SHOWNA); // This is necessary when we alter the style
        viewport->PlatformRequestMove = viewport->PlatformRequestResize = true;
    }
}

static ImVec2 Platform_GetWindowPos(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    POINT pos = {0, 0};
    ClientToScreen(data->Hwnd, &pos);
    return ImVec2((float)pos.x, (float)pos.y);
}

static void Platform_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    RECT rect = {(LONG)pos.x, (LONG)pos.y, (LONG)pos.x, (LONG)pos.y};
    AdjustWindowRectEx(&rect, data->DwStyle, FALSE, data->DwExStyle);
    SetWindowPos(data->Hwnd, nullptr, rect.left, rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

static ImVec2 Platform_GetWindowSize(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    RECT rect;
    GetClientRect(data->Hwnd, &rect);
    return ImVec2(float(rect.right - rect.left), float(rect.bottom - rect.top));
}

static void Platform_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    RECT rect = {0, 0, (LONG)size.x, (LONG)size.y};
    AdjustWindowRectEx(&rect, data->DwStyle, FALSE, data->DwExStyle); // Client to Screen
    SetWindowPos(data->Hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

static void Platform_SetWindowFocus(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    BringWindowToTop(data->Hwnd);
    SetForegroundWindow(data->Hwnd);
    SetFocus(data->Hwnd);
}

static bool Platform_GetWindowFocus(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    return GetForegroundWindow() == data->Hwnd;
}

static bool Platform_GetWindowMinimized(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    return IsIconic(data->Hwnd) != 0;
}

static void Platform_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    int n = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    ImVector<wchar_t> title_w;
    title_w.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, title_w.Data, n);
    SetWindowTextW(data->Hwnd, title_w.Data);
}

static void Platform_SetWindowAlpha(ImGuiViewport* viewport, float alpha)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    RUNTIME_ASSERT((alpha >= 0.0f && alpha <= 1.0f));
    if (alpha < 1.0f)
    {
        DWORD style = GetWindowLongW(data->Hwnd, GWL_EXSTYLE) | WS_EX_LAYERED;
        SetWindowLongW(data->Hwnd, GWL_EXSTYLE, style);
        SetLayeredWindowAttributes(data->Hwnd, 0, (BYTE)(255 * alpha), LWA_ALPHA);
    }
    else
    {
        DWORD style = GetWindowLongW(data->Hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED;
        SetWindowLongW(data->Hwnd, GWL_EXSTYLE, style);
    }
}

static float Platform_GetWindowDpiScale(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    RUNTIME_ASSERT(data->Hwnd != 0);
    return GetDpiScaleForHwnd(data->Hwnd);
}

static void Platform_OnChangedViewport(ImGuiViewport* viewport)
{
    UNUSED_VARIABLE(viewport);
}

static void Platform_SetImeInputPos(ImGuiViewport* viewport, ImVec2 pos)
{
    COMPOSITIONFORM cf = {
        CFS_FORCE_POSITION, {(LONG)(pos.x - viewport->Pos.x), (LONG)(pos.y - viewport->Pos.y)}, {0, 0, 0, 0}};
    if (HWND hwnd = (HWND)viewport->PlatformHandle)
    {
        if (HIMC himc = ImmGetContext(hwnd))
        {
            ImmSetCompositionWindow(himc, &cf);
            ImmReleaseContext(hwnd, himc);
        }
    }
}

static void UpdateMonitors()
{
    ImGui::GetPlatformIO().Monitors.resize(0);
    EnumDisplayMonitors(nullptr, nullptr, UpdateMonitorsEnumFunc, 0);
    WantUpdateMonitors = false;
}

static BOOL CALLBACK UpdateMonitorsEnumFunc(HMONITOR monitor, HDC, LPRECT, LPARAM)
{
    MONITORINFO info = {0};
    info.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(monitor, &info))
        return TRUE;

    ImGuiPlatformMonitor imgui_monitor;
    imgui_monitor.MainPos = ImVec2((float)info.rcMonitor.left, (float)info.rcMonitor.top);
    imgui_monitor.MainSize = ImVec2(
        (float)(info.rcMonitor.right - info.rcMonitor.left), (float)(info.rcMonitor.bottom - info.rcMonitor.top));
    imgui_monitor.WorkPos = ImVec2((float)info.rcWork.left, (float)info.rcWork.top);
    imgui_monitor.WorkSize =
        ImVec2((float)(info.rcWork.right - info.rcWork.left), (float)(info.rcWork.bottom - info.rcWork.top));
    imgui_monitor.DpiScale = GetDpiScaleForMonitor(monitor);
    ImGuiPlatformIO& io = ImGui::GetPlatformIO();
    if (info.dwFlags & MONITORINFOF_PRIMARY)
        io.Monitors.push_front(imgui_monitor);
    else
        io.Monitors.push_back(imgui_monitor);
    return TRUE;
}

static void Renderer_CreateWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataDx9* data = IM_NEW(ImGuiViewportDataDx9)();
    viewport->RendererUserData = data;

    HWND hwnd = viewport->PlatformHandleRaw ? (HWND)viewport->PlatformHandleRaw : (HWND)viewport->PlatformHandle;
    RUNTIME_ASSERT(hwnd != 0);

    ZeroMemory(&data->D3dPP, sizeof(D3DPRESENT_PARAMETERS));
    data->D3dPP.Windowed = TRUE;
    data->D3dPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
    data->D3dPP.BackBufferWidth = (UINT)viewport->Size.x;
    data->D3dPP.BackBufferHeight = (UINT)viewport->Size.y;
    data->D3dPP.BackBufferFormat = D3DFMT_UNKNOWN;
    data->D3dPP.hDeviceWindow = hwnd;
    data->D3dPP.EnableAutoDepthStencil = FALSE;
    data->D3dPP.AutoDepthStencilFormat = D3DFMT_D16;
    data->D3dPP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hr = D3dDevice->CreateAdditionalSwapChain(&data->D3dPP, &data->SwapChain);
    RUNTIME_ASSERT(hr == D3D_OK);
    RUNTIME_ASSERT(data->SwapChain != nullptr);
}

static void Renderer_DestroyWindow(ImGuiViewport* viewport)
{
    if (ImGuiViewportDataDx9* data = (ImGuiViewportDataDx9*)viewport->RendererUserData)
    {
        if (data->SwapChain)
            data->SwapChain->Release();
        data->SwapChain = nullptr;
        ZeroMemory(&data->D3dPP, sizeof(D3DPRESENT_PARAMETERS));
        IM_DELETE(data);
    }
    viewport->RendererUserData = nullptr;
}

static void Renderer_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGuiViewportDataDx9* data = (ImGuiViewportDataDx9*)viewport->RendererUserData;
    if (data->SwapChain)
    {
        data->SwapChain->Release();
        data->SwapChain = nullptr;
        data->D3dPP.BackBufferWidth = (UINT)size.x;
        data->D3dPP.BackBufferHeight = (UINT)size.y;
        HRESULT hr = D3dDevice->CreateAdditionalSwapChain(&data->D3dPP, &data->SwapChain);
        RUNTIME_ASSERT(hr == D3D_OK);
    }
}

static void Renderer_RenderWindow(ImGuiViewport* viewport, void*)
{
    ImGuiViewportDataDx9* data = (ImGuiViewportDataDx9*)viewport->RendererUserData;
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

    LPDIRECT3DSURFACE9 render_target = nullptr;
    LPDIRECT3DSURFACE9 last_render_target = nullptr;
    LPDIRECT3DSURFACE9 last_depth_stencil = nullptr;
    data->SwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &render_target);
    D3dDevice->GetRenderTarget(0, &last_render_target);
    D3dDevice->GetDepthStencilSurface(&last_depth_stencil);
    D3dDevice->SetRenderTarget(0, render_target);
    D3dDevice->SetDepthStencilSurface(nullptr);

    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
    {
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * 255.0f), (int)(clear_color.y * 255.0f),
            (int)(clear_color.z * 255.0f), (int)(clear_color.w * 255.0f));
        D3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, clear_col_dx, 1.0f, 0);
    }

    RenderDrawData(viewport->DrawData);

    // Restore render target
    D3dDevice->SetRenderTarget(0, last_render_target);
    D3dDevice->SetDepthStencilSurface(last_depth_stencil);
    render_target->Release();
    last_render_target->Release();
    if (last_depth_stencil)
        last_depth_stencil->Release();
}

static void Renderer_SwapBuffers(ImGuiViewport* viewport, void*)
{
    ImGuiViewportDataDx9* data = (ImGuiViewportDataDx9*)viewport->RendererUserData;
    HRESULT hr = data->SwapChain->Present(nullptr, nullptr, data->D3dPP.hDeviceWindow, nullptr, 0);
    RUNTIME_ASSERT(hr == D3D_OK);
}

#endif
#endif
