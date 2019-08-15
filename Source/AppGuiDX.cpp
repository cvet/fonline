#include "AppGui.h"

#ifdef FO_HAVE_DX

# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <XInput.h>
# include <tchar.h>
# include "imgui.h"
# include <d3d10_1.h>
# include <d3d10.h>
# include <d3d10misc.h>
# include <d3dcompiler.h>
# define DIRECTINPUT_VERSION    0x0800
# include <dinput.h>

# ifdef FO_MSVC
#  pragma comment(lib, "d3d10")
#  pragma comment(lib, "d3dcompiler")
#  pragma comment(lib, "xinput")
#  pragma comment(lib, "gdi32")
# endif

// Win32 Data
static HWND             WndHandle = 0;
static INT64            CurTime = 0;
static INT64            TicksPerSecond = 0;
static ImGuiMouseCursor LastMouseCursor = ImGuiMouseCursor_COUNT;
static bool             WantUpdateMonitors = true;

// DirectX data
static ID3D10Device*             D3dDevice = nullptr;
static IDXGIFactory*             D3dFactory = nullptr;
static ID3D10Buffer*             VertexBuffer = nullptr;
static ID3D10Buffer*             IndexBuffer = nullptr;
static ID3D10Blob*               VertexShaderBlob = nullptr;
static ID3D10VertexShader*       VertexShader = nullptr;
static ID3D10InputLayout*        InputLayout = nullptr;
static ID3D10Buffer*             ConstantBuffer = nullptr;
static ID3D10Blob*               PixelShaderBlob = nullptr;
static ID3D10PixelShader*        PixelShader = nullptr;
static ID3D10SamplerState*       FontSampler = nullptr;
static ID3D10ShaderResourceView* FontTextureView = nullptr;
static ID3D10RasterizerState*    RasterizerState = nullptr;
static ID3D10BlendState*         BlendState = nullptr;
static ID3D10DepthStencilState*  DepthStencilState = nullptr;
static int                       VertexBufferSize = 5000;
static int                       IndexBufferSize = 10000;
static IDXGISwapChain*           SwapChain = nullptr;
static ID3D10RenderTargetView*   MainRenderTargetView = nullptr;

struct VertexConstantBuffer
{
    float Mvp[ 4 ][ 4 ];
};

// Forward declarations of helper functions
static LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
static bool           CreateDeviceD3D( HWND hWnd );
static void           CleanupDeviceD3D();
static void           CreateRenderTarget();
static void           CleanupRenderTarget();
static void           RenderDrawData( ImDrawData* draw_data );
static void           SetupRenderState( ImDrawData* draw_data, ID3D10Device* ctx );
static bool           CreateDeviceObjects();
static bool           UpdateMouseCursor();

static void EnableDpiAwareness();
static void InitPlatformInterface();
static void UpdateMonitors();
static void DxInitPlatformInterface();

// Main code
bool AppGui::InitDX( bool maximized )
{
    EnableDpiAwareness();

    // Create application window
    WNDCLASSEX wc = { sizeof( WNDCLASSEX ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( nullptr ), nullptr, nullptr, nullptr, nullptr, _T( "ImGui Example" ), nullptr };
    ::RegisterClassEx( &wc );
    HWND       hwnd = ::CreateWindow( wc.lpszClassName, _T( "Dear ImGui DirectX10 Example" ), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr );

    // Initialize Direct3D
    if( !CreateDeviceD3D( hwnd ) )
    {
        CleanupDeviceD3D();
        ::UnregisterClass( wc.lpszClassName, wc.hInstance );
        return false;
    }

    // Show the window
    ::ShowWindow( hwnd, maximized ? SW_SHOWMAXIMIZED : SW_SHOWDEFAULT );
    ::UpdateWindow( hwnd );

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        style.WindowRounding = 0.0f;
        style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
    }

    // Setup Platform/Renderer bindings
    if( !::QueryPerformanceFrequency( (LARGE_INTEGER*) &TicksPerSecond ) )
        return false;
    if( !::QueryPerformanceCounter( (LARGE_INTEGER*) &CurTime ) )
        return false;

    // Setup back-end capabilities flags
    WndHandle = (HWND) hwnd;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // We can create multi-viewports on the Platform side (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can set io.MouseHoveredViewport correctly (optional, not easy)
    io.BackendPlatformName = "imgui_impl_win32";

    // Our mouse update function expect PlatformHandle to be filled for the main viewport
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = main_viewport->PlatformHandleRaw = (void*) WndHandle;
    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
        InitPlatformInterface();

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array that we will update during the application lifetime.
    io.KeyMap[ ImGuiKey_Tab ] = VK_TAB;
    io.KeyMap[ ImGuiKey_LeftArrow ] = VK_LEFT;
    io.KeyMap[ ImGuiKey_RightArrow ] = VK_RIGHT;
    io.KeyMap[ ImGuiKey_UpArrow ] = VK_UP;
    io.KeyMap[ ImGuiKey_DownArrow ] = VK_DOWN;
    io.KeyMap[ ImGuiKey_PageUp ] = VK_PRIOR;
    io.KeyMap[ ImGuiKey_PageDown ] = VK_NEXT;
    io.KeyMap[ ImGuiKey_Home ] = VK_HOME;
    io.KeyMap[ ImGuiKey_End ] = VK_END;
    io.KeyMap[ ImGuiKey_Insert ] = VK_INSERT;
    io.KeyMap[ ImGuiKey_Delete ] = VK_DELETE;
    io.KeyMap[ ImGuiKey_Backspace ] = VK_BACK;
    io.KeyMap[ ImGuiKey_Space ] = VK_SPACE;
    io.KeyMap[ ImGuiKey_Enter ] = VK_RETURN;
    io.KeyMap[ ImGuiKey_Escape ] = VK_ESCAPE;
    io.KeyMap[ ImGuiKey_KeyPadEnter ] = VK_RETURN;
    io.KeyMap[ ImGuiKey_A ] = 'A';
    io.KeyMap[ ImGuiKey_C ] = 'C';
    io.KeyMap[ ImGuiKey_V ] = 'V';
    io.KeyMap[ ImGuiKey_X ] = 'X';
    io.KeyMap[ ImGuiKey_Y ] = 'Y';
    io.KeyMap[ ImGuiKey_Z ] = 'Z';

    // Setup back-end capabilities flags
    io.BackendRendererName = "imgui_impl_dx10";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

    // Get factory from device
    IDXGIDevice*  device = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIFactory* factory = nullptr;

    if( D3dDevice->QueryInterface( IID_PPV_ARGS( &device ) ) == S_OK )
        if( device->GetParent( IID_PPV_ARGS( &adapter ) ) == S_OK )
            if( adapter->GetParent( IID_PPV_ARGS( &factory ) ) == S_OK )
                D3dFactory = factory;
    if( device )
        device->Release();
    if( adapter )
        adapter->Release();
    D3dDevice->AddRef();

    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
        DxInitPlatformInterface();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != NULL);

    CreateDeviceObjects();

    return true;
}

bool AppGui::BeginFrameDX()
{
    MSG msg;
    ZeroMemory( &msg, sizeof( msg ) );
    while( true )
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if( ::PeekMessage( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
        {
            ::TranslateMessage( &msg );
            ::DispatchMessage( &msg );
            continue;
        }

        if( msg.message == WM_QUIT )
            return false;

        break;
    }

    // Start the Dear ImGui frame
    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    RECT rect;
    ::GetClientRect( WndHandle, &rect );
    io.DisplaySize = ImVec2( (float) ( rect.right - rect.left ), (float) ( rect.bottom - rect.top ) );
    if( WantUpdateMonitors )
        UpdateMonitors();

    // Setup time step
    INT64 current_time;
    ::QueryPerformanceCounter( (LARGE_INTEGER*) &current_time );
    io.DeltaTime = (float) ( current_time - CurTime ) / TicksPerSecond;
    CurTime = current_time;

    // Read keyboard modifiers inputs
    io.KeyCtrl = ( ::GetKeyState( VK_CONTROL ) & 0x8000 ) != 0;
    io.KeyShift = ( ::GetKeyState( VK_SHIFT ) & 0x8000 ) != 0;
    io.KeyAlt = ( ::GetKeyState( VK_MENU ) & 0x8000 ) != 0;
    io.KeySuper = false;
    // io.KeysDown[], io.MousePos, io.MouseDown[], io.MouseWheel: filled by the WndProc handler below.

    // Update OS mouse position
    // Set OS mouse position if requested (rarely used, only when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
    if( io.WantSetMousePos )
    {
        POINT pos = { (int) io.MousePos.x, (int) io.MousePos.y };
        if( ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) == 0 )
            ::ClientToScreen( WndHandle, &pos );
        ::SetCursorPos( pos.x, pos.y );
    }

    // Set mouse position
    io.MousePos = ImVec2( -FLT_MAX, -FLT_MAX );
    io.MouseHoveredViewport = 0;

    // Set imgui mouse position
    POINT mouse_screen_pos;
    if( ::GetCursorPos( &mouse_screen_pos ) )
    {
        if( HWND focused_hwnd = ::GetForegroundWindow() )
        {
            if( ::IsChild( focused_hwnd, WndHandle ) )
                focused_hwnd = WndHandle;

            if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
            {
                // Multi-viewport mode: mouse position in OS absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary monitor)
                // This is the position you can get with GetCursorPos(). In theory adding viewport->Pos is also the reverse operation of doing ScreenToClient().
                if( ImGui::FindViewportByPlatformHandle( (void*) focused_hwnd ) != nullptr )
                    io.MousePos = ImVec2( (float) mouse_screen_pos.x, (float) mouse_screen_pos.y );
            }
            else
            {
                // Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app window.)
                // This is the position you can get with GetCursorPos() + ScreenToClient() or from WM_MOUSEMOVE.
                if( focused_hwnd == WndHandle )
                {
                    POINT mouse_client_pos = mouse_screen_pos;
                    ::ScreenToClient( focused_hwnd, &mouse_client_pos );
                    io.MousePos = ImVec2( (float) mouse_client_pos.x, (float) mouse_client_pos.y );
                }
            }
        }

        // (Optional) When using multiple viewports: set io.MouseHoveredViewport to the viewport the OS mouse cursor is hovering.
        // Important: this information is not easy to provide and many high-level windowing library won't be able to provide it correctly, because
        // - This is _ignoring_ viewports with the ImGuiViewportFlags_NoInputs flag (pass-through windows).
        // - This is _regardless_ of whether another viewport is focused or being dragged from.
        // If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the back-end, imgui will ignore this field and infer the information by relying on the
        // rectangles and last focused time of every viewports it knows about. It will be unaware of foreign windows that may be sitting between or over your windows.
        if( HWND hovered_hwnd = ::WindowFromPoint( mouse_screen_pos ) )
            if( ImGuiViewport * viewport = ImGui::FindViewportByPlatformHandle( (void*) hovered_hwnd ) )
                if( ( viewport->Flags & ImGuiViewportFlags_NoInputs ) == 0 )              // FIXME: We still get our NoInputs window with WM_NCHITTEST/HTTRANSPARENT code when decorated?
                    io.MouseHoveredViewport = viewport->ID;

        // Update OS mouse cursor with the cursor requested by imgui
        ImGuiMouseCursor mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
        if( LastMouseCursor != mouse_cursor )
        {
            LastMouseCursor = mouse_cursor;
            UpdateMouseCursor();
        }
    }

    ImGui::NewFrame();
    return true;
}

void AppGui::EndFrameDX()
{
    // Rendering
    ImGui::Render();
    D3dDevice->OMSetRenderTargets( 1, &MainRenderTargetView, nullptr );
    static ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );
    D3dDevice->ClearRenderTargetView( MainRenderTargetView, (float*) &clear_color );
    RenderDrawData( ImGui::GetDrawData() );

    // Update and Render additional Platform Windows
    ImGuiIO& io = ImGui::GetIO();
    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    SwapChain->Present( 1, 0 ); // Present with vsync
    // g_pSwapChain->Present(0, 0); // Present without vsync
}

// Win32 message handler
static LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    // Allow compilation with old Windows SDK. MinGW doesn't have default _WIN32_WINNT/WINVER versions.
    # ifndef WM_MOUSEHWHEEL
    #  define WM_MOUSEHWHEEL          0x020E
    # endif
    # ifndef DBT_DEVNODES_CHANGED
    #  define DBT_DEVNODES_CHANGED    0x0007
    # endif

    switch( msg )
    {
    case WM_SIZE:
        if( D3dDevice != nullptr && wParam != SIZE_MINIMIZED )
        {
            CleanupRenderTarget();
            SwapChain->ResizeBuffers( 0, (UINT) LOWORD( lParam ), (UINT) HIWORD( lParam ), DXGI_FORMAT_UNKNOWN, 0 );
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if( ( wParam & 0xfff0 ) == SC_KEYMENU ) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage( 0 );
        return 0;
    }

    if( ImGui::GetCurrentContext() != nullptr )
    {
        ImGuiIO& io = ImGui::GetIO();

        switch( msg )
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        {
            int button = 0;
            if( msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK )
                button = 0;
            else if( msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK )
                button = 1;
            else if( msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK )
                button = 2;
            else if( msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK )
                button = ( GET_XBUTTON_WPARAM( wParam ) == XBUTTON1 ) ? 3 : 4;
            else if( !ImGui::IsAnyMouseDown() && ::GetCapture() == nullptr )
                ::SetCapture( hWnd );
            io.MouseDown[ button ] = true;
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        {
            int button = 0;
            if( msg == WM_LBUTTONUP )
                button = 0;
            else if( msg == WM_RBUTTONUP )
                button = 1;
            else if( msg == WM_MBUTTONUP )
                button = 2;
            else if( msg == WM_XBUTTONUP )
                button = ( GET_XBUTTON_WPARAM( wParam ) == XBUTTON1 ) ? 3 : 4;
            io.MouseDown[ button ] = false;
            if( !ImGui::IsAnyMouseDown() && ::GetCapture() == hWnd )
                ::ReleaseCapture();
            return 0;
        }
        case WM_MOUSEWHEEL:
            io.MouseWheel += (float) GET_WHEEL_DELTA_WPARAM( wParam ) / (float) WHEEL_DELTA;
            return 0;
        case WM_MOUSEHWHEEL:
            io.MouseWheelH += (float) GET_WHEEL_DELTA_WPARAM( wParam ) / (float) WHEEL_DELTA;
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if( wParam < 256 )
                io.KeysDown[ wParam ] = true;
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if( wParam < 256 )
                io.KeysDown[ wParam ] = false;
            return 0;
        case WM_CHAR:
            io.AddInputCharacter( (unsigned int) wParam );
            return 0;
        case WM_SETCURSOR:
            if( LOWORD( lParam ) == HTCLIENT && UpdateMouseCursor() )
                return 1;
            return 0;
        case WM_DISPLAYCHANGE:
            WantUpdateMonitors = true;
            return 0;
        }
    }

    return ::DefWindowProc( hWnd, msg, wParam, lParam );
}

// Helper functions
static bool CreateDeviceD3D( HWND hWnd )
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof( sd ) );
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    if( D3D10CreateDeviceAndSwapChain( nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, D3D10_SDK_VERSION, &sd, &SwapChain, &D3dDevice ) != S_OK )
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if( SwapChain )
    {
        SwapChain->Release();
        SwapChain = nullptr;
    }
    if( D3dDevice )
    {
        D3dDevice->Release();
        D3dDevice = nullptr;
    }
}

static void CreateRenderTarget()
{
    ID3D10Texture2D* pBackBuffer;
    SwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
    D3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &MainRenderTargetView );
    pBackBuffer->Release();
}

static void CleanupRenderTarget()
{
    if( MainRenderTargetView )
    {
        MainRenderTargetView->Release();
        MainRenderTargetView = nullptr;
    }
}

static bool CreateDeviceObjects()
{
    // By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
    // If you would like to use this DX10 sample code but remove this dependency you can:
    //  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
    //  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
    // See https://github.com/ocornut/imgui/pull/638 for sources and details.

    // Create the vertex shader
    {
        static const char* vertexShader =
            "cbuffer vertexBuffer : register(b0) \
            {\
            float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
            float2 pos : POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
            PS_INPUT output;\
            output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
            output.col = input.col;\
            output.uv  = input.uv;\
            return output;\
            }";

        D3DCompile( vertexShader, strlen( vertexShader ), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &VertexShaderBlob, nullptr );
        if( VertexShaderBlob == nullptr ) // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
            return false;
        if( D3dDevice->CreateVertexShader( (DWORD*) VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), &VertexShader ) != S_OK )
            return false;

        // Create the input layout
        D3D10_INPUT_ELEMENT_DESC local_layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t) ( &( (ImDrawVert*) 0 )->pos ), D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t) ( &( (ImDrawVert*) 0 )->uv ),  D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (size_t) ( &( (ImDrawVert*) 0 )->col ), D3D10_INPUT_PER_VERTEX_DATA, 0 },
        };
        if( D3dDevice->CreateInputLayout( local_layout, 3, VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), &InputLayout ) != S_OK )
            return false;

        // Create the constant buffer
        {
            D3D10_BUFFER_DESC desc;
            desc.ByteWidth = sizeof( VertexConstantBuffer );
            desc.Usage = D3D10_USAGE_DYNAMIC;
            desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;
            D3dDevice->CreateBuffer( &desc, nullptr, &ConstantBuffer );
        }
    }

    // Create the pixel shader
    {
        static const char* pixelShader =
            "struct PS_INPUT\
            {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            sampler sampler0;\
            Texture2D texture0;\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
            float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
            return out_col; \
            }";

        D3DCompile( pixelShader, strlen( pixelShader ), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &PixelShaderBlob, nullptr );
        if( PixelShaderBlob == nullptr ) // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
            return false;
        if( D3dDevice->CreatePixelShader( (DWORD*) PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), &PixelShader ) != S_OK )
            return false;
    }

    // Create the blending setup
    {
        D3D10_BLEND_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );
        desc.AlphaToCoverageEnable = false;
        desc.BlendEnable[ 0 ] = true;
        desc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
        desc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
        desc.BlendOp = D3D10_BLEND_OP_ADD;
        desc.SrcBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
        desc.DestBlendAlpha = D3D10_BLEND_ZERO;
        desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
        desc.RenderTargetWriteMask[ 0 ] = D3D10_COLOR_WRITE_ENABLE_ALL;
        D3dDevice->CreateBlendState( &desc, &BlendState );
    }

    // Create the rasterizer state
    {
        D3D10_RASTERIZER_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );
        desc.FillMode = D3D10_FILL_SOLID;
        desc.CullMode = D3D10_CULL_NONE;
        desc.ScissorEnable = true;
        desc.DepthClipEnable = true;
        D3dDevice->CreateRasterizerState( &desc, &RasterizerState );
    }

    // Create depth-stencil State
    {
        D3D10_DEPTH_STENCIL_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D10_COMPARISON_ALWAYS;
        desc.StencilEnable = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D10_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D10_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        D3dDevice->CreateDepthStencilState( &desc, &DepthStencilState );
    }

    // Build texture atlas
    ImGuiIO&       io = ImGui::GetIO();
    unsigned char* pixels;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    // Upload texture to graphics system
    {
        D3D10_TEXTURE2D_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D10_USAGE_DEFAULT;
        desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        ID3D10Texture2D*       pTexture = nullptr;
        D3D10_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = pixels;
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;
        D3dDevice->CreateTexture2D( &desc, &subResource, &pTexture );

        // Create texture view
        D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc;
        ZeroMemory( &srv_desc, sizeof( srv_desc ) );
        srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = desc.MipLevels;
        srv_desc.Texture2D.MostDetailedMip = 0;
        D3dDevice->CreateShaderResourceView( pTexture, &srv_desc, &FontTextureView );
        pTexture->Release();
    }

    // Store our identifier
    io.Fonts->TexID = (ImTextureID) FontTextureView;

    // Create texture sampler
    {
        D3D10_SAMPLER_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );
        desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
        desc.MipLODBias = 0.f;
        desc.ComparisonFunc = D3D10_COMPARISON_ALWAYS;
        desc.MinLOD = 0.f;
        desc.MaxLOD = 0.f;
        D3dDevice->CreateSamplerState( &desc, &FontSampler );
    }

    return true;
}

// Render function
// (this used to be set in io.RenderDrawListsFn and called by ImGui::Render(), but you can now call this directly from your main loop)
static void RenderDrawData( ImDrawData* draw_data )
{
    // Avoid rendering when minimized
    if( draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f )
        return;

    ID3D10Device* ctx = D3dDevice;

    // Create and grow vertex/index buffers if needed
    if( !VertexBuffer || VertexBufferSize < draw_data->TotalVtxCount )
    {
        if( VertexBuffer )
        {
            VertexBuffer->Release();
            VertexBuffer = nullptr;
        }
        VertexBufferSize = draw_data->TotalVtxCount + 5000;
        D3D10_BUFFER_DESC desc;
        memset( &desc, 0, sizeof( D3D10_BUFFER_DESC ) );
        desc.Usage = D3D10_USAGE_DYNAMIC;
        desc.ByteWidth = VertexBufferSize * sizeof( ImDrawVert );
        desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        if( ctx->CreateBuffer( &desc, nullptr, &VertexBuffer ) < 0 )
            return;
    }

    if( !IndexBuffer || IndexBufferSize < draw_data->TotalIdxCount )
    {
        if( IndexBuffer )
        {
            IndexBuffer->Release();
            IndexBuffer = nullptr;
        }
        IndexBufferSize = draw_data->TotalIdxCount + 10000;
        D3D10_BUFFER_DESC desc;
        memset( &desc, 0, sizeof( D3D10_BUFFER_DESC ) );
        desc.Usage = D3D10_USAGE_DYNAMIC;
        desc.ByteWidth = IndexBufferSize * sizeof( ImDrawIdx );
        desc.BindFlags = D3D10_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
        if( ctx->CreateBuffer( &desc, nullptr, &IndexBuffer ) < 0 )
            return;
    }

    // Copy and convert all vertices into a single contiguous buffer
    ImDrawVert* vtx_dst = nullptr;
    ImDrawIdx*  idx_dst = nullptr;
    VertexBuffer->Map( D3D10_MAP_WRITE_DISCARD, 0, (void**) &vtx_dst );
    IndexBuffer->Map( D3D10_MAP_WRITE_DISCARD, 0, (void**) &idx_dst );
    for( int n = 0; n < draw_data->CmdListsCount; n++ )
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[ n ];
        memcpy( vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
        memcpy( idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    VertexBuffer->Unmap();
    IndexBuffer->Unmap();

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        void* mapped_resource;
        if( ConstantBuffer->Map( D3D10_MAP_WRITE_DISCARD, 0, &mapped_resource ) != S_OK )
            return;
        VertexConstantBuffer* constant_buffer = (VertexConstantBuffer*) mapped_resource;
        float                 L = draw_data->DisplayPos.x;
        float                 R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float                 T = draw_data->DisplayPos.y;
        float                 B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        float                 mvp[ 4 ][ 4 ] =
        {
            { 2.0f / ( R - L ),   0.0f,           0.0f,       0.0f },
            { 0.0f,         2.0f / ( T - B ),     0.0f,       0.0f },
            { 0.0f,         0.0f,           0.5f,       0.0f },
            { ( R + L ) / ( L - R ),  ( T + B ) / ( B - T ),    0.5f,       1.0f },
        };
        memcpy( &constant_buffer->Mvp, mvp, sizeof( mvp ) );
        ConstantBuffer->Unmap();
    }

    // Backup DX state that will be modified to restore it afterwards (unfortunately this is very ugly looking and verbose. Close your eyes!)
    struct BACKUP_DX10_STATE
    {
        UINT                      ScissorRectsCount, ViewportsCount;
        D3D10_RECT                ScissorRects[ D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ];
        D3D10_VIEWPORT            Viewports[ D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ];
        ID3D10RasterizerState*    RS;
        ID3D10BlendState*         BlendState;
        FLOAT                     BlendFactor[ 4 ];
        UINT                      SampleMask;
        UINT                      StencilRef;
        ID3D10DepthStencilState*  DepthStencilState;
        ID3D10ShaderResourceView* PSShaderResource;
        ID3D10SamplerState*       PSSampler;
        ID3D10PixelShader*        PS;
        ID3D10VertexShader*       VS;
        ID3D10GeometryShader*     GS;
        D3D10_PRIMITIVE_TOPOLOGY  PrimitiveTopology;
        ID3D10Buffer*             IndexBuffer, * VertexBuffer, * VSConstantBuffer;
        UINT                      IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
        DXGI_FORMAT               IndexBufferFormat;
        ID3D10InputLayout*        InputLayout;
    };
    BACKUP_DX10_STATE old;
    old.ScissorRectsCount = old.ViewportsCount = D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ctx->RSGetScissorRects( &old.ScissorRectsCount, old.ScissorRects );
    ctx->RSGetViewports( &old.ViewportsCount, old.Viewports );
    ctx->RSGetState( &old.RS );
    ctx->OMGetBlendState( &old.BlendState, old.BlendFactor, &old.SampleMask );
    ctx->OMGetDepthStencilState( &old.DepthStencilState, &old.StencilRef );
    ctx->PSGetShaderResources( 0, 1, &old.PSShaderResource );
    ctx->PSGetSamplers( 0, 1, &old.PSSampler );
    ctx->PSGetShader( &old.PS );
    ctx->VSGetShader( &old.VS );
    ctx->VSGetConstantBuffers( 0, 1, &old.VSConstantBuffer );
    ctx->GSGetShader( &old.GS );
    ctx->IAGetPrimitiveTopology( &old.PrimitiveTopology );
    ctx->IAGetIndexBuffer( &old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset );
    ctx->IAGetVertexBuffers( 0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset );
    ctx->IAGetInputLayout( &old.InputLayout );

    // Setup desired DX state
    SetupRenderState( draw_data, ctx );

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int    global_vtx_offset = 0;
    int    global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for( int n = 0; n < draw_data->CmdListsCount; n++ )
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[ n ];
        for( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[ cmd_i ];
            if( pcmd->UserCallback )
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if( pcmd->UserCallback == ImDrawCallback_ResetRenderState )
                    SetupRenderState( draw_data, ctx );
                else
                    pcmd->UserCallback( cmd_list, pcmd );
            }
            else
            {
                // Apply scissor/clipping rectangle
                const D3D10_RECT r = { (LONG) ( pcmd->ClipRect.x - clip_off.x ), (LONG) ( pcmd->ClipRect.y - clip_off.y ), (LONG) ( pcmd->ClipRect.z - clip_off.x ), (LONG) ( pcmd->ClipRect.w - clip_off.y ) };
                ctx->RSSetScissorRects( 1, &r );

                // Bind texture, Draw
                ID3D10ShaderResourceView* texture_srv = (ID3D10ShaderResourceView*) pcmd->TextureId;
                ctx->PSSetShaderResources( 0, 1, &texture_srv );
                ctx->DrawIndexed( pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset );
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Restore modified DX state
    ctx->RSSetScissorRects( old.ScissorRectsCount, old.ScissorRects );
    ctx->RSSetViewports( old.ViewportsCount, old.Viewports );
    ctx->RSSetState( old.RS );
    if( old.RS )
        old.RS->Release();
    ctx->OMSetBlendState( old.BlendState, old.BlendFactor, old.SampleMask );
    if( old.BlendState )
        old.BlendState->Release();
    ctx->OMSetDepthStencilState( old.DepthStencilState, old.StencilRef );
    if( old.DepthStencilState )
        old.DepthStencilState->Release();
    ctx->PSSetShaderResources( 0, 1, &old.PSShaderResource );
    if( old.PSShaderResource )
        old.PSShaderResource->Release();
    ctx->PSSetSamplers( 0, 1, &old.PSSampler );
    if( old.PSSampler )
        old.PSSampler->Release();
    ctx->PSSetShader( old.PS );
    if( old.PS )
        old.PS->Release();
    ctx->VSSetShader( old.VS );
    if( old.VS )
        old.VS->Release();
    ctx->GSSetShader( old.GS );
    if( old.GS )
        old.GS->Release();
    ctx->VSSetConstantBuffers( 0, 1, &old.VSConstantBuffer );
    if( old.VSConstantBuffer )
        old.VSConstantBuffer->Release();
    ctx->IASetPrimitiveTopology( old.PrimitiveTopology );
    ctx->IASetIndexBuffer( old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset );
    if( old.IndexBuffer )
        old.IndexBuffer->Release();
    ctx->IASetVertexBuffers( 0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset );
    if( old.VertexBuffer )
        old.VertexBuffer->Release();
    ctx->IASetInputLayout( old.InputLayout );
    if( old.InputLayout )
        old.InputLayout->Release();
}

static void SetupRenderState( ImDrawData* draw_data, ID3D10Device* ctx )
{
    // Setup viewport
    D3D10_VIEWPORT vp;
    memset( &vp, 0, sizeof( D3D10_VIEWPORT ) );
    vp.Width = (UINT) draw_data->DisplaySize.x;
    vp.Height = (UINT) draw_data->DisplaySize.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = vp.TopLeftY = 0;
    ctx->RSSetViewports( 1, &vp );

    // Bind shader and vertex buffers
    unsigned int stride = sizeof( ImDrawVert );
    unsigned int offset = 0;
    ctx->IASetInputLayout( InputLayout );
    ctx->IASetVertexBuffers( 0, 1, &VertexBuffer, &stride, &offset );
    ctx->IASetIndexBuffer( IndexBuffer, sizeof( ImDrawIdx ) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0 );
    ctx->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    ctx->VSSetShader( VertexShader );
    ctx->VSSetConstantBuffers( 0, 1, &ConstantBuffer );
    ctx->PSSetShader( PixelShader );
    ctx->PSSetSamplers( 0, 1, &FontSampler );
    ctx->GSSetShader( nullptr );

    // Setup render state
    const float blend_factor[ 4 ] = { 0.f, 0.f, 0.f, 0.f };
    ctx->OMSetBlendState( BlendState, blend_factor, 0xffffffff );
    ctx->OMSetDepthStencilState( DepthStencilState, 0 );
    ctx->RSSetState( RasterizerState );
}

static bool UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if( io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange )
        return false;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if( imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor )
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        ::SetCursor( nullptr );
    }
    else
    {
        // Show OS mouse cursor
        LPTSTR win32_cursor = IDC_ARROW;
        switch( imgui_cursor )
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
        ::SetCursor( ::LoadCursor( nullptr, win32_cursor ) );
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------
// DPI handling
// Those in theory should be simple calls but Windows has multiple ways to handle DPI, and most of them
// require recent Windows versions at runtime or recent Windows SDK at compile-time. Neither we want to depend on.
// So we dynamically select and load those functions to avoid dependencies. This is the scheme successfully
// used by GLFW (from which we borrowed some of the code here) and other applications aiming to be portable.
// ---------------------------------------------------------------------------------------------------------
// At this point ImGui_ImplWin32_EnableDpiAwareness() is just a helper called by main.cpp, we don't call it automatically.
// ---------------------------------------------------------------------------------------------------------

static BOOL IsWindowsVersionOrGreater( WORD major, WORD minor, WORD sp )
{
    OSVERSIONINFOEXW osvi = { sizeof( osvi ), major, minor, 0, 0, { 0 }, sp };
    DWORD            mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    ULONGLONG        cond = VerSetConditionMask( 0, VER_MAJORVERSION, VER_GREATER_EQUAL );
    cond = VerSetConditionMask( cond, VER_MINORVERSION, VER_GREATER_EQUAL );
    cond = VerSetConditionMask( cond, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );
    return VerifyVersionInfoW( &osvi, mask, cond );
}
# define IsWindows8Point1OrGreater()    IsWindowsVersionOrGreater( HIBYTE( 0x0602 ), LOBYTE( 0x0602 ), 0 ) // _WIN32_WINNT_WINBLUE
# define IsWindows10OrGreater()         IsWindowsVersionOrGreater( HIBYTE( 0x0A00 ), LOBYTE( 0x0A00 ), 0 ) // _WIN32_WINNT_WIN10

# ifndef DPI_ENUMS_DECLARED
typedef enum { PROCESS_DPI_UNAWARE = 0, PROCESS_SYSTEM_DPI_AWARE = 1, PROCESS_PER_MONITOR_DPI_AWARE = 2 }     PROCESS_DPI_AWARENESS;
typedef enum { MDT_EFFECTIVE_DPI = 0, MDT_ANGULAR_DPI = 1, MDT_RAW_DPI = 2, MDT_DEFAULT = MDT_EFFECTIVE_DPI } MONITOR_DPI_TYPE;
# endif
# ifndef _DPI_AWARENESS_CONTEXTS_
DECLARE_HANDLE( DPI_AWARENESS_CONTEXT );
#  define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE       (DPI_AWARENESS_CONTEXT) -3
# endif
# ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#  define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2    (DPI_AWARENESS_CONTEXT) -4
# endif
typedef HRESULT ( WINAPI * PFN_SetProcessDpiAwareness )( PROCESS_DPI_AWARENESS );                     // Shcore.lib+dll, Windows 8.1
typedef HRESULT ( WINAPI * PFN_GetDpiForMonitor )( HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT* );        // Shcore.lib+dll, Windows 8.1
typedef DPI_AWARENESS_CONTEXT ( WINAPI * PFN_SetThreadDpiAwarenessContext )( DPI_AWARENESS_CONTEXT ); // User32.lib+dll, Windows 10 v1607 (Creators Update)

static void EnableDpiAwareness()
{
    // if (IsWindows10OrGreater()) // FIXME-DPI: This needs a manifest to succeed. Instead we try to grab the function pointer.
    {
        static HINSTANCE user32_dll = ::LoadLibraryA( "user32.dll" ); // Reference counted per-process
        if( PFN_SetThreadDpiAwarenessContext SetThreadDpiAwarenessContextFn = ( PFN_SetThreadDpiAwarenessContext ) ::GetProcAddress( user32_dll, "SetThreadDpiAwarenessContext" ) )
        {
            SetThreadDpiAwarenessContextFn( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
            return;
        }
    }
    if( IsWindows8Point1OrGreater() )
    {
        static HINSTANCE shcore_dll = ::LoadLibraryA( "shcore.dll" ); // Reference counted per-process
        if( PFN_SetProcessDpiAwareness SetProcessDpiAwarenessFn = ( PFN_SetProcessDpiAwareness ) ::GetProcAddress( shcore_dll, "SetProcessDpiAwareness" ) )
            SetProcessDpiAwarenessFn( PROCESS_PER_MONITOR_DPI_AWARE );
    }
    else
    {
        SetProcessDPIAware();
    }
}

static float GetDpiScaleForMonitor( void* monitor )
{
    UINT xdpi = 96, ydpi = 96;
    if( ::IsWindows8Point1OrGreater() )
    {
        static HINSTANCE shcore_dll = ::LoadLibraryA( "shcore.dll" ); // Reference counted per-process
        if( PFN_GetDpiForMonitor GetDpiForMonitorFn = ( PFN_GetDpiForMonitor ) ::GetProcAddress( shcore_dll, "GetDpiForMonitor" ) )
            GetDpiForMonitorFn( (HMONITOR) monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi );
    }
    else
    {
        const HDC dc = ::GetDC( nullptr );
        xdpi = ::GetDeviceCaps( dc, LOGPIXELSX );
        ydpi = ::GetDeviceCaps( dc, LOGPIXELSY );
        ::ReleaseDC( nullptr, dc );
    }
    IM_ASSERT( xdpi == ydpi ); // Please contact me if you hit this assert!
    return xdpi / 96.0f;
}

static float GetDpiScaleForHwnd( void* hwnd )
{
    HMONITOR monitor = ::MonitorFromWindow( (HWND) hwnd, MONITOR_DEFAULTTONEAREST );
    return GetDpiScaleForMonitor( monitor );
}

// --------------------------------------------------------------------------------------------------------
// IME (Input Method Editor) basic support for e.g. Asian language users
// --------------------------------------------------------------------------------------------------------

# if defined ( _WIN32 ) && !defined ( IMGUI_DISABLE_WIN32_FUNCTIONS ) && !defined ( IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS ) && !defined ( __GNUC__ )
#  define HAS_WIN32_IME                                 1
#  include <imm.h>
#  ifdef _MSC_VER
#   pragma comment(lib, "imm32")
#  endif
static void SetImeInputPos( ImGuiViewport* viewport, ImVec2 pos )
{
    COMPOSITIONFORM cf = { CFS_FORCE_POSITION, { (LONG) ( pos.x - viewport->Pos.x ), (LONG) ( pos.y - viewport->Pos.y ) }, { 0, 0, 0, 0 } };
    if( HWND hwnd = (HWND) viewport->PlatformHandle )
        if( HIMC himc = ::ImmGetContext( hwnd ) )
        {
            ::ImmSetCompositionWindow( himc, &cf );
            ::ImmReleaseContext( hwnd, himc );
        }
}
# else
#  define HAS_WIN32_IME                                 0
# endif

// --------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
// --------------------------------------------------------------------------------------------------------

struct ImGuiViewportDataWin32
{
    HWND  Hwnd;
    bool  HwndOwned;
    DWORD DwStyle;
    DWORD DwExStyle;

    ImGuiViewportDataWin32()
    {
        Hwnd = nullptr;
        HwndOwned = false;
        DwStyle = DwExStyle = 0;
    }
    ~ImGuiViewportDataWin32() { IM_ASSERT( Hwnd == nullptr ); }
};

static void GetWin32StyleFromViewportFlags( ImGuiViewportFlags flags, DWORD* out_style, DWORD* out_ex_style )
{
    if( flags & ImGuiViewportFlags_NoDecoration )
        *out_style = WS_POPUP;
    else
        *out_style = WS_OVERLAPPEDWINDOW;

    if( flags & ImGuiViewportFlags_NoTaskBarIcon )
        *out_ex_style = WS_EX_TOOLWINDOW;
    else
        *out_ex_style = WS_EX_APPWINDOW;

    if( flags & ImGuiViewportFlags_TopMost )
        *out_ex_style |= WS_EX_TOPMOST;
}

static void VpCreateWindow( ImGuiViewport* viewport )
{
    ImGuiViewportDataWin32* data = IM_NEW( ImGuiViewportDataWin32 ) ();
    viewport->PlatformUserData = data;

    // Select style and parent window
    GetWin32StyleFromViewportFlags( viewport->Flags, &data->DwStyle, &data->DwExStyle );
    HWND parent_window = nullptr;
    if( viewport->ParentViewportId != 0 )
        if( ImGuiViewport * parent_viewport = ImGui::FindViewportByID( viewport->ParentViewportId ) )
            parent_window = (HWND) parent_viewport->PlatformHandle;

    // Create window
    RECT rect = { (LONG) viewport->Pos.x, (LONG) viewport->Pos.y, (LONG) ( viewport->Pos.x + viewport->Size.x ), (LONG) ( viewport->Pos.y + viewport->Size.y ) };
    ::AdjustWindowRectEx( &rect, data->DwStyle, FALSE, data->DwExStyle );
    data->Hwnd = ::CreateWindowEx(
        data->DwExStyle, _T( "ImGui Platform" ), _T( "Untitled" ), data->DwStyle, // Style, class name, window name
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,      // Window area
        parent_window, nullptr, ::GetModuleHandle( nullptr ), nullptr );          // Parent window, Menu, Instance, Param
    data->HwndOwned = true;
    viewport->PlatformRequestResize = false;
    viewport->PlatformHandle = viewport->PlatformHandleRaw = data->Hwnd;
}

static void VpDestroyWindow( ImGuiViewport* viewport )
{
    if( ImGuiViewportDataWin32 * data = (ImGuiViewportDataWin32*) viewport->PlatformUserData )
    {
        if( ::GetCapture() == data->Hwnd )
        {
            // Transfer capture so if we started dragging from a window that later disappears, we'll still receive the MOUSEUP event.
            ::ReleaseCapture();
            ::SetCapture( WndHandle );
        }
        if( data->Hwnd && data->HwndOwned )
            ::DestroyWindow( data->Hwnd );
        data->Hwnd = nullptr;
        IM_DELETE( data );
    }
    viewport->PlatformUserData = viewport->PlatformHandle = nullptr;
}

static void VpShowWindow( ImGuiViewport* viewport )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    if( viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing )
        ::ShowWindow( data->Hwnd, SW_SHOWNA );
    else
        ::ShowWindow( data->Hwnd, SW_SHOW );
}

static void VpUpdateWindow( ImGuiViewport* viewport )
{
    // (Optional) Update Win32 style if it changed _after_ creation.
    // Generally they won't change unless configuration flags are changed, but advanced uses (such as manually rewriting viewport flags) make this useful.
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    DWORD                   new_style;
    DWORD                   new_ex_style;
    GetWin32StyleFromViewportFlags( viewport->Flags, &new_style, &new_ex_style );

    // Only reapply the flags that have been changed from our point of view (as other flags are being modified by Windows)
    if( data->DwStyle != new_style || data->DwExStyle != new_ex_style )
    {
        data->DwStyle = new_style;
        data->DwExStyle = new_ex_style;
        ::SetWindowLong( data->Hwnd, GWL_STYLE, data->DwStyle );
        ::SetWindowLong( data->Hwnd, GWL_EXSTYLE, data->DwExStyle );
        RECT rect = { (LONG) viewport->Pos.x, (LONG) viewport->Pos.y, (LONG) ( viewport->Pos.x + viewport->Size.x ), (LONG) ( viewport->Pos.y + viewport->Size.y ) };
        ::AdjustWindowRectEx( &rect, data->DwStyle, FALSE, data->DwExStyle ); // Client to Screen
        ::SetWindowPos( data->Hwnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED );
        ::ShowWindow( data->Hwnd, SW_SHOWNA );                                // This is necessary when we alter the style
        viewport->PlatformRequestMove = viewport->PlatformRequestResize = true;
    }
}

static ImVec2 VpGetWindowPos( ImGuiViewport* viewport )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    POINT                   pos = { 0, 0 };
    ::ClientToScreen( data->Hwnd, &pos );
    return ImVec2( (float) pos.x, (float) pos.y );
}

static void VpSetWindowPos( ImGuiViewport* viewport, ImVec2 pos )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    RECT                    rect = { (LONG) pos.x, (LONG) pos.y, (LONG) pos.x, (LONG) pos.y };
    ::AdjustWindowRectEx( &rect, data->DwStyle, FALSE, data->DwExStyle );
    ::SetWindowPos( data->Hwnd, nullptr, rect.left, rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE );
}

static ImVec2 VpGetWindowSize( ImGuiViewport* viewport )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    RECT                    rect;
    ::GetClientRect( data->Hwnd, &rect );
    return ImVec2( float(rect.right - rect.left), float(rect.bottom - rect.top) );
}

static void VpSetWindowSize( ImGuiViewport* viewport, ImVec2 size )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    RECT                    rect = { 0, 0, (LONG) size.x, (LONG) size.y };
    ::AdjustWindowRectEx( &rect, data->DwStyle, FALSE, data->DwExStyle ); // Client to Screen
    ::SetWindowPos( data->Hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE );
}

static void VpSetWindowFocus( ImGuiViewport* viewport )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    ::BringWindowToTop( data->Hwnd );
    ::SetForegroundWindow( data->Hwnd );
    ::SetFocus( data->Hwnd );
}

static bool VpGetWindowFocus( ImGuiViewport* viewport )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    return ::GetForegroundWindow() == data->Hwnd;
}

static bool VpGetWindowMinimized( ImGuiViewport* viewport )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    return ::IsIconic( data->Hwnd ) != 0;
}

static void VpSetWindowTitle( ImGuiViewport* viewport, const char* title )
{
    // ::SetWindowTextA() doesn't properly handle UTF-8 so we explicitely convert our string.
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    int                     n = ::MultiByteToWideChar( CP_UTF8, 0, title, -1, nullptr, 0 );
    ImVector< wchar_t >     title_w;
    title_w.resize( n );
    ::MultiByteToWideChar( CP_UTF8, 0, title, -1, title_w.Data, n );
    ::SetWindowTextW( data->Hwnd, title_w.Data );
}

static void VpSetWindowAlpha( ImGuiViewport* viewport, float alpha )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    IM_ASSERT( alpha >= 0.0f && alpha <= 1.0f );
    if( alpha < 1.0f )
    {
        DWORD style = ::GetWindowLongW( data->Hwnd, GWL_EXSTYLE ) | WS_EX_LAYERED;
        ::SetWindowLongW( data->Hwnd, GWL_EXSTYLE, style );
        ::SetLayeredWindowAttributes( data->Hwnd, 0, (BYTE) ( 255 * alpha ), LWA_ALPHA );
    }
    else
    {
        DWORD style = ::GetWindowLongW( data->Hwnd, GWL_EXSTYLE ) & ~WS_EX_LAYERED;
        ::SetWindowLongW( data->Hwnd, GWL_EXSTYLE, style );
    }
}

static float VpGetWindowDpiScale( ImGuiViewport* viewport )
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*) viewport->PlatformUserData;
    IM_ASSERT( data->Hwnd != 0 );
    return GetDpiScaleForHwnd( data->Hwnd );
}

// FIXME-DPI: Testing DPI related ideas
static void OnChangedViewport( ImGuiViewport* viewport )
{
    (void) viewport;
    # if 0
    ImGuiStyle default_style;
    // default_style.WindowPadding = ImVec2(0, 0);
    // default_style.WindowBorderSize = 0.0f;
    // default_style.ItemSpacing.y = 3.0f;
    // default_style.FramePadding = ImVec2(0, 0);
    default_style.ScaleAllSizes( viewport->DpiScale );
    ImGuiStyle& style = ImGui::GetStyle();
    style = default_style;
    # endif
}

static LRESULT CALLBACK WndProcHandler_PlatformWindow( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    // if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    //    return true;

    if( ImGuiViewport * viewport = ImGui::FindViewportByPlatformHandle( (void*) hWnd ) )
    {
        switch( msg )
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
            if( viewport->Flags & ImGuiViewportFlags_NoFocusOnClick )
                return MA_NOACTIVATE;
            break;
        case WM_NCHITTEST:
            // Let mouse pass-through the window. This will allow the back-end to set io.MouseHoveredViewport properly (which is OPTIONAL).
            // The ImGuiViewportFlags_NoInputs flag is set while dragging a viewport, as want to detect the window behind the one we are dragging.
            // If you cannot easily access those viewport flags from your windowing/event code: you may manually synchronize its state e.g. in
            // your main loop after calling UpdatePlatformWindows(). Iterate all viewports/platform windows and pass the flag to your windowing system.
            if( viewport->Flags & ImGuiViewportFlags_NoInputs )
                return HTTRANSPARENT;
            break;
        }
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}

static BOOL CALLBACK UpdateMonitors_EnumFunc( HMONITOR monitor, HDC, LPRECT, LPARAM )
{
    MONITORINFO info = { 0 };
    info.cbSize = sizeof( MONITORINFO );
    if( !::GetMonitorInfo( monitor, &info ) )
        return TRUE;
    ImGuiPlatformMonitor imgui_monitor;
    imgui_monitor.MainPos = ImVec2( (float) info.rcMonitor.left, (float) info.rcMonitor.top );
    imgui_monitor.MainSize = ImVec2( (float) ( info.rcMonitor.right - info.rcMonitor.left ), (float) ( info.rcMonitor.bottom - info.rcMonitor.top ) );
    imgui_monitor.WorkPos = ImVec2( (float) info.rcWork.left, (float) info.rcWork.top );
    imgui_monitor.WorkSize = ImVec2( (float) ( info.rcWork.right - info.rcWork.left ), (float) ( info.rcWork.bottom - info.rcWork.top ) );
    imgui_monitor.DpiScale = GetDpiScaleForMonitor( monitor );
    ImGuiPlatformIO& io = ImGui::GetPlatformIO();
    if( info.dwFlags & MONITORINFOF_PRIMARY )
        io.Monitors.push_front( imgui_monitor );
    else
        io.Monitors.push_back( imgui_monitor );
    return TRUE;
}

static void UpdateMonitors()
{
    ImGui::GetPlatformIO().Monitors.resize( 0 );
    ::EnumDisplayMonitors( nullptr, nullptr, UpdateMonitors_EnumFunc, 0 );
    WantUpdateMonitors = false;
}

static void InitPlatformInterface()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProcHandler_PlatformWindow;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = ::GetModuleHandle( nullptr );
    wcex.hIcon = nullptr;
    wcex.hCursor = nullptr;
    wcex.hbrBackground = (HBRUSH) ( COLOR_BACKGROUND + 1 );
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = _T( "ImGui Platform" );
    wcex.hIconSm = nullptr;
    ::RegisterClassEx( &wcex );

    UpdateMonitors();

    // Register platform interface (will be coupled with a renderer interface)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = VpCreateWindow;
    platform_io.Platform_DestroyWindow = VpDestroyWindow;
    platform_io.Platform_ShowWindow = VpShowWindow;
    platform_io.Platform_SetWindowPos = VpSetWindowPos;
    platform_io.Platform_GetWindowPos = VpGetWindowPos;
    platform_io.Platform_SetWindowSize = VpSetWindowSize;
    platform_io.Platform_GetWindowSize = VpGetWindowSize;
    platform_io.Platform_SetWindowFocus = VpSetWindowFocus;
    platform_io.Platform_GetWindowFocus = VpGetWindowFocus;
    platform_io.Platform_GetWindowMinimized = VpGetWindowMinimized;
    platform_io.Platform_SetWindowTitle = VpSetWindowTitle;
    platform_io.Platform_SetWindowAlpha = VpSetWindowAlpha;
    platform_io.Platform_UpdateWindow = VpUpdateWindow;
    platform_io.Platform_GetWindowDpiScale = VpGetWindowDpiScale; // FIXME-DPI
    platform_io.Platform_OnChangedViewport = OnChangedViewport;   // FIXME-DPI
    # if HAS_WIN32_IME
    platform_io.Platform_SetImeInputPos = SetImeInputPos;
    # endif

    // Register main window handle (which is owned by the main application, not by us)
    ImGuiViewport*          main_viewport = ImGui::GetMainViewport();
    ImGuiViewportDataWin32* data = IM_NEW( ImGuiViewportDataWin32 ) ();
    data->Hwnd = WndHandle;
    data->HwndOwned = false;
    main_viewport->PlatformUserData = data;
    main_viewport->PlatformHandle = (void*) WndHandle;
}

// --------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
// --------------------------------------------------------------------------------------------------------

struct ImGuiViewportDataDx10
{
    IDXGISwapChain*         SwapChain;
    ID3D10RenderTargetView* RTView;

    ImGuiViewportDataDx10()
    {
        SwapChain = nullptr;
        RTView = nullptr;
    }
    ~ImGuiViewportDataDx10() { IM_ASSERT( SwapChain == nullptr && RTView == nullptr ); }
};

static void ImGui_ImplDX10_CreateWindow( ImGuiViewport* viewport )
{
    ImGuiViewportDataDx10* data = IM_NEW( ImGuiViewportDataDx10 ) ();
    viewport->RendererUserData = data;

    // PlatformHandleRaw should always be a HWND, whereas PlatformHandle might be a higher-level handle (e.g. GLFWWindow*, SDL_Window*).
    // Some back-ends will leave PlatformHandleRaw NULL, in which case we assume PlatformHandle will contain the HWND.
    HWND hwnd = viewport->PlatformHandleRaw ? (HWND) viewport->PlatformHandleRaw : (HWND) viewport->PlatformHandle;
    IM_ASSERT( hwnd != 0 );

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof( sd ) );
    sd.BufferDesc.Width = (UINT) viewport->Size.x;
    sd.BufferDesc.Height = (UINT) viewport->Size.y;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 1;
    sd.OutputWindow = hwnd;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;

    IM_ASSERT( data->SwapChain == nullptr && data->RTView == nullptr );
    D3dFactory->CreateSwapChain( D3dDevice, &sd, &data->SwapChain );

    // Create the render target
    if( data->SwapChain )
    {
        ID3D10Texture2D* pBackBuffer;
        data->SwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
        D3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &data->RTView );
        pBackBuffer->Release();
    }
}

static void ImGui_ImplDX10_DestroyWindow( ImGuiViewport* viewport )
{
    // The main viewport (owned by the application) will always have RendererUserData == NULL here since we didn't create the data for it.
    if( ImGuiViewportDataDx10 * data = (ImGuiViewportDataDx10*) viewport->RendererUserData )
    {
        if( data->SwapChain )
            data->SwapChain->Release();
        data->SwapChain = nullptr;
        if( data->RTView )
            data->RTView->Release();
        data->RTView = nullptr;
        IM_DELETE( data );
    }
    viewport->RendererUserData = nullptr;
}

static void ImGui_ImplDX10_SetWindowSize( ImGuiViewport* viewport, ImVec2 size )
{
    ImGuiViewportDataDx10* data = (ImGuiViewportDataDx10*) viewport->RendererUserData;
    if( data->RTView )
    {
        data->RTView->Release();
        data->RTView = nullptr;
    }
    if( data->SwapChain )
    {
        ID3D10Texture2D* pBackBuffer = nullptr;
        data->SwapChain->ResizeBuffers( 0, (UINT) size.x, (UINT) size.y, DXGI_FORMAT_UNKNOWN, 0 );
        data->SwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
        if( pBackBuffer == nullptr )
        {
            /*fprintf(stderr, "ImGui_ImplDX10_SetWindowSize() failed creating buffers.\n");*/
            return;
        }
        D3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &data->RTView );
        pBackBuffer->Release();
    }
}

static void ImGui_ImplDX10_RenderViewport( ImGuiViewport* viewport, void* )
{
    ImGuiViewportDataDx10* data = (ImGuiViewportDataDx10*) viewport->RendererUserData;
    ImVec4                 clear_color1 = ImVec4( 0.0f, 0.0f, 0.0f, 1.0f );
    D3dDevice->OMSetRenderTargets( 1, &data->RTView, nullptr );
    if( !( viewport->Flags & ImGuiViewportFlags_NoRendererClear ) )
        D3dDevice->ClearRenderTargetView( data->RTView, (float*) &clear_color1 );
    RenderDrawData( viewport->DrawData );
}

static void ImGui_ImplDX10_SwapBuffers( ImGuiViewport* viewport, void* )
{
    ImGuiViewportDataDx10* data = (ImGuiViewportDataDx10*) viewport->RendererUserData;
    data->SwapChain->Present( 0, 0 ); // Present without vsync
}

void DxInitPlatformInterface()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow = ImGui_ImplDX10_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_ImplDX10_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_ImplDX10_SetWindowSize;
    platform_io.Renderer_RenderWindow = ImGui_ImplDX10_RenderViewport;
    platform_io.Renderer_SwapBuffers = ImGui_ImplDX10_SwapBuffers;
}

#endif
