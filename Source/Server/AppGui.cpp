#include "AppGui.h"
#include "GraphicApi.h"
#include "Threading.h"

struct ImGuiViewportDataSDL2
{
    SDL_Window*   Window;
    Uint32        WindowID;
    bool          WindowOwned;
    SDL_GLContext GLContext;

    ImGuiViewportDataSDL2()
    {
        Window = nullptr;
        WindowID = 0;
        WindowOwned = false;
        GLContext = nullptr;
    }

    ~ImGuiViewportDataSDL2()
    {
        RUNTIME_ASSERT( Window == nullptr && GLContext == nullptr );
    }
};

static bool          InitCalled = false;
static bool          UseDirectX = false;
static bool          FixedPipeline = false;
static GLuint        FontTexture = 0;
static GLuint        ShaderHandle = 0;
static GLuint        VertHandle = 0;
static GLuint        FragHandle = 0;
static int           AttribLocationTex = 0;
static int           AttribLocationProjMtx = 0;
static int           AttribLocationVtxPos = 0;
static int           AttribLocationVtxUV = 0;
static int           AttribLocationVtxColor = 0;
static unsigned int  VboHandle = 0;
static unsigned int  ElementsHandle = 0;
static SDL_Window*   SdlWindow = nullptr;
static SDL_GLContext GlContext = nullptr;
static Uint64        CurTime = 0;
static bool          MousePressed[ 3 ] = { false, false, false };
static SDL_Cursor*   MouseCursors[ ImGuiMouseCursor_COUNT ] = { 0 };
static char*         ClipboardTextData = nullptr;

static const char* GetClipboardText( void* );
static void        SetClipboardText( void*, const char* text );
static void        RenderDrawData( ImDrawData* draw_data );
static void        SetupRenderState( ImDrawData* draw_data, int fb_width, int fb_height, GLuint vao );
static void        Platform_CreateWindow( ImGuiViewport* viewport );
static void        Platform_DestroyWindow( ImGuiViewport* viewport );
static void        Platform_ShowWindow( ImGuiViewport* viewport );
static ImVec2      Platform_GetWindowPos( ImGuiViewport* viewport );
static void        Platform_SetWindowPos( ImGuiViewport* viewport, ImVec2 pos );
static ImVec2      Platform_GetWindowSize( ImGuiViewport* viewport );
static void        Platform_SetWindowSize( ImGuiViewport* viewport, ImVec2 size );
static void        Platform_SetWindowTitle( ImGuiViewport* viewport, const char* title );
static void        Platform_SetWindowAlpha( ImGuiViewport* viewport, float alpha );
static void        Platform_SetWindowFocus( ImGuiViewport* viewport );
static bool        Platform_GetWindowFocus( ImGuiViewport* viewport );
static bool        Platform_GetWindowMinimized( ImGuiViewport* viewport );
static void        Platform_RenderWindow( ImGuiViewport* viewport, void* );
static void        Platform_SwapBuffers( ImGuiViewport* viewport, void* );
static int         Platform_CreateVkSurface( ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface );
static void        Renderer_RenderWindow( ImGuiViewport* viewport, void* );

bool AppGui::Init( const string& app_name, bool use_dx, bool docking, bool maximized )
{
    RUNTIME_ASSERT( !InitCalled );
    InitCalled = true;

    // DirectX backend
    if( use_dx )
    {
        #ifdef FO_HAVE_DX
        if( !InitDX( app_name, docking, maximized ) )
            return false;

        UseDirectX = true;
        return true;
        #else
        WriteLog( "This system doesn't has DirectX support.\n" );
        return false;
        #endif
    }

    // Setup SDL
    if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ) != 0 )
    {
        WriteLog( "SDL2 init error: {}\n", SDL_GetError() );
        return false;
    }

    // Init SDL
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
    SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
    #ifdef FO_OGL_ES
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
    #endif

    SDL_WindowFlags window_flags = (SDL_WindowFlags) ( SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                                       SDL_WINDOW_ALLOW_HIGHDPI | ( maximized ? SDL_WINDOW_MAXIMIZED : 0 ) );
    SDL_Window*     window = SDL_CreateWindow( app_name.c_str(),
                                               SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, window_flags );
    SDL_GLContext   gl_context = SDL_GL_CreateContext( window );
    SDL_GL_MakeCurrent( window, gl_context );
    SDL_GL_SetSwapInterval( 0 );

    SdlWindow = window;
    GlContext = gl_context;

    // Init graphic
    if( !GraphicApi::Init() )
        return false;

    // RDP connection support only fixed pipeline
    #ifdef FO_WINDOWS
    int remote_session = GetSystemMetrics( SM_REMOTESESSION );
    FixedPipeline = ( remote_session != 0 );

    #else
    if( !GL_HAS( version_2_0 ) || !GL_HAS( vertex_buffer_object ) )
    {
        WriteLog( "Minimum OpenGL 2.0 required.\n" );
        return false;
    }
    #endif

    // Setup OpenGL
    GL( glEnable( GL_BLEND ) );
    GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
    GL( glDisable( GL_CULL_FACE ) );
    GL( glDisable( GL_DEPTH_TEST ) );
    GL( glEnable( GL_TEXTURE_2D ) );
    #ifndef FO_OGL_ES
    GL( glDisable( GL_LIGHTING ) );
    GL( glDisable( GL_COLOR_MATERIAL ) );
    GL( glPolygonMode( GL_FRONT_AND_BACK, GL_FILL ) );
    #endif

    if( !FixedPipeline )
    {
        const GLchar* vertex_shader =
            "#version 110\n"
            "uniform mat4 ProjMtx;\n"
            "attribute vec2 Position;\n"
            "attribute vec2 UV;\n"
            "attribute vec4 Color;\n"
            "varying vec2 Frag_UV;\n"
            "varying vec4 Frag_Color;\n"
            "void main()\n"
            "{\n"
            "    Frag_UV = UV;\n"
            "    Frag_Color = Color;\n"
            "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
            "}\n";
        const GLchar* fragment_shader =
            "#version 110\n"
            #ifdef FO_OGL_ES
            "precision mediump float;\n"
            #endif
            "uniform sampler2D Texture;\n"
            "varying vec2 Frag_UV;\n"
            "varying vec4 Frag_Color;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV.st);\n"
            "}\n";

        // Create shaders
        const GLchar* vertex_shader_arr[ 1 ] = { vertex_shader };
        VertHandle = glCreateShader( GL_VERTEX_SHADER );
        GL( glShaderSource( VertHandle, 1, vertex_shader_arr, nullptr ) );
        GL( glCompileShader( VertHandle ) );
        GLint vs_status = 0;
        GL( glGetShaderiv( VertHandle, GL_COMPILE_STATUS, &vs_status ) );
        RUNTIME_ASSERT( (GLboolean) vs_status == GL_TRUE );

        const GLchar* fragment_shader_arr[ 1 ] = { fragment_shader };
        FragHandle = glCreateShader( GL_FRAGMENT_SHADER );
        GL( glShaderSource( FragHandle, 1, fragment_shader_arr, nullptr ) );
        GL( glCompileShader( FragHandle ) );
        GLint fs_status = 0;
        GL( glGetShaderiv( FragHandle, GL_COMPILE_STATUS, &fs_status ) );
        RUNTIME_ASSERT( (GLboolean) fs_status == GL_TRUE );

        ShaderHandle = glCreateProgram();
        GL( glAttachShader( ShaderHandle, VertHandle ) );
        GL( glAttachShader( ShaderHandle, FragHandle ) );
        GL( glLinkProgram( ShaderHandle ) );
        GLint program_status = 0;
        GL( glGetProgramiv( ShaderHandle, GL_LINK_STATUS, &program_status ) );
        RUNTIME_ASSERT( (GLboolean) program_status == GL_TRUE );

        AttribLocationTex = glGetUniformLocation( ShaderHandle, "Texture" );
        AttribLocationProjMtx = glGetUniformLocation( ShaderHandle, "ProjMtx" );
        AttribLocationVtxPos = glGetAttribLocation( ShaderHandle, "Position" );
        AttribLocationVtxUV = glGetAttribLocation( ShaderHandle, "UV" );
        AttribLocationVtxColor = glGetAttribLocation( ShaderHandle, "Color" );

        GL( glGenBuffers( 1, &VboHandle ) );
        GL( glGenBuffers( 1, &ElementsHandle ) );
    }

    // Init Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.WantSaveIniSettings = false;
    io.IniFilename = nullptr;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if( docking )
    {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    // Setup Dear ImGui style
    // ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();
    ImGui::StyleColorsLight();

    // Setup back-end capabilities flags
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
    #ifndef FO_OGL_ES
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    #endif

    // Keyboard mapping
    io.KeyMap[ ImGuiKey_Tab ] = SDL_SCANCODE_TAB;
    io.KeyMap[ ImGuiKey_LeftArrow ] = SDL_SCANCODE_LEFT;
    io.KeyMap[ ImGuiKey_RightArrow ] = SDL_SCANCODE_RIGHT;
    io.KeyMap[ ImGuiKey_UpArrow ] = SDL_SCANCODE_UP;
    io.KeyMap[ ImGuiKey_DownArrow ] = SDL_SCANCODE_DOWN;
    io.KeyMap[ ImGuiKey_PageUp ] = SDL_SCANCODE_PAGEUP;
    io.KeyMap[ ImGuiKey_PageDown ] = SDL_SCANCODE_PAGEDOWN;
    io.KeyMap[ ImGuiKey_Home ] = SDL_SCANCODE_HOME;
    io.KeyMap[ ImGuiKey_End ] = SDL_SCANCODE_END;
    io.KeyMap[ ImGuiKey_Insert ] = SDL_SCANCODE_INSERT;
    io.KeyMap[ ImGuiKey_Delete ] = SDL_SCANCODE_DELETE;
    io.KeyMap[ ImGuiKey_Backspace ] = SDL_SCANCODE_BACKSPACE;
    io.KeyMap[ ImGuiKey_Space ] = SDL_SCANCODE_SPACE;
    io.KeyMap[ ImGuiKey_Enter ] = SDL_SCANCODE_RETURN;
    io.KeyMap[ ImGuiKey_Escape ] = SDL_SCANCODE_ESCAPE;
    io.KeyMap[ ImGuiKey_KeyPadEnter ] = SDL_SCANCODE_RETURN2;
    io.KeyMap[ ImGuiKey_A ] = SDL_SCANCODE_A;
    io.KeyMap[ ImGuiKey_C ] = SDL_SCANCODE_C;
    io.KeyMap[ ImGuiKey_V ] = SDL_SCANCODE_V;
    io.KeyMap[ ImGuiKey_X ] = SDL_SCANCODE_X;
    io.KeyMap[ ImGuiKey_Y ] = SDL_SCANCODE_Y;
    io.KeyMap[ ImGuiKey_Z ] = SDL_SCANCODE_Z;

    io.GetClipboardTextFn = GetClipboardText;
    io.SetClipboardTextFn = SetClipboardText;
    io.ClipboardUserData = nullptr;

    MouseCursors[ ImGuiMouseCursor_Arrow ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_ARROW );
    MouseCursors[ ImGuiMouseCursor_TextInput ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_IBEAM );
    MouseCursors[ ImGuiMouseCursor_ResizeAll ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEALL );
    MouseCursors[ ImGuiMouseCursor_ResizeNS ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENS );
    MouseCursors[ ImGuiMouseCursor_ResizeEW ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEWE );
    MouseCursors[ ImGuiMouseCursor_ResizeNESW ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENESW );
    MouseCursors[ ImGuiMouseCursor_ResizeNWSE ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENWSE );
    MouseCursors[ ImGuiMouseCursor_Hand ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_HAND );

    // Setup viewport stuff
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = SdlWindow;
    #ifdef FO_WINDOWS
    SDL_SysWMinfo info;
    SDL_VERSION( &info.version );
    if( SDL_GetWindowWMInfo( window, &info ) )
        main_viewport->PlatformHandleRaw = info.info.win.window;
    #endif

    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
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
        platform_io.Platform_RenderWindow = Platform_RenderWindow;
        platform_io.Platform_SwapBuffers = Platform_SwapBuffers;
        platform_io.Platform_SetWindowAlpha = Platform_SetWindowAlpha;
        platform_io.Platform_CreateVkSurface = Platform_CreateVkSurface;
        platform_io.Renderer_RenderWindow = Renderer_RenderWindow;

        SDL_SetHint( SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1" );

        // Update monitors
        platform_io.Monitors.resize( 0 );
        int display_count = SDL_GetNumVideoDisplays();
        for( int n = 0; n < display_count; n++ )
        {
            ImGuiPlatformMonitor monitor;
            SDL_Rect             r;
            SDL_GetDisplayBounds( n, &r );
            monitor.MainPos = monitor.WorkPos = ImVec2( (float) r.x, (float) r.y );
            monitor.MainSize = monitor.WorkSize = ImVec2( (float) r.w, (float) r.h );
            SDL_GetDisplayUsableBounds( n, &r );
            monitor.WorkPos = ImVec2( (float) r.x, (float) r.y );
            monitor.WorkSize = ImVec2( (float) r.w, (float) r.h );

            float dpi = 0.0f;
            if( !SDL_GetDisplayDPI( n, &dpi, nullptr, nullptr ) )
                monitor.DpiScale = dpi / 96.0f;

            platform_io.Monitors.push_back( monitor );
        }

        // Register main window handle
        ImGuiViewportDataSDL2* viewport_data = IM_NEW( ImGuiViewportDataSDL2 ) ();
        viewport_data->Window = SdlWindow;
        viewport_data->WindowID = SDL_GetWindowID( SdlWindow );
        viewport_data->WindowOwned = false;
        viewport_data->GLContext = GlContext;
        main_viewport->PlatformUserData = viewport_data;
    }

    // Build texture atlas
    unsigned char* pixels;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    GL( glGenTextures( 1, &FontTexture ) );
    GL( glBindTexture( GL_TEXTURE_2D, FontTexture ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
    #ifdef GL_UNPACK_ROW_LENGTH
    GL( glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 ) );
    #endif
    GL( glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels ) );
    GL( glBindTexture( GL_TEXTURE_2D, 0 ) );

    io.Fonts->TexID = (ImTextureID) (intptr_t) FontTexture;

    return true;
}

bool AppGui::BeginFrame()
{
    RUNTIME_ASSERT( InitCalled );

    #ifdef FO_HAVE_DX
    if( UseDirectX )
        return BeginFrameDX();
    #endif

    ImGuiIO& io = ImGui::GetIO();

    // Poll and handle events
    SDL_Event event;
    bool      quit = false;
    while( SDL_PollEvent( &event ) )
    {
        switch( event.type )
        {
        case SDL_MOUSEWHEEL:
        {
            if( event.wheel.x > 0 )
                io.MouseWheelH += 1;
            if( event.wheel.x < 0 )
                io.MouseWheelH -= 1;
            if( event.wheel.y > 0 )
                io.MouseWheel += 1;
            if( event.wheel.y < 0 )
                io.MouseWheel -= 1;
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        {
            if( event.button.button == SDL_BUTTON_LEFT )
                MousePressed[ 0 ] = true;
            if( event.button.button == SDL_BUTTON_RIGHT )
                MousePressed[ 1 ] = true;
            if( event.button.button == SDL_BUTTON_MIDDLE )
                MousePressed[ 2 ] = true;
            break;
        }
        case SDL_TEXTINPUT:
        {
            io.AddInputCharactersUTF8( event.text.text );
            break;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            int key = event.key.keysym.scancode;
            RUNTIME_ASSERT( key >= 0 && key < IM_ARRAYSIZE( io.KeysDown ) );

            io.KeysDown[ key ] = ( event.type == SDL_KEYDOWN );
            io.KeyShift = ( ( SDL_GetModState() & KMOD_SHIFT ) != 0 );
            io.KeyCtrl = ( ( SDL_GetModState() & KMOD_CTRL ) != 0 );
            io.KeyAlt = ( ( SDL_GetModState() & KMOD_ALT ) != 0 );
            io.KeySuper = ( ( SDL_GetModState() & KMOD_GUI ) != 0 );
            break;
        }
        case SDL_WINDOWEVENT:
            Uint8 window_event = event.window.event;
            if( window_event == SDL_WINDOWEVENT_CLOSE || window_event == SDL_WINDOWEVENT_MOVED || window_event == SDL_WINDOWEVENT_RESIZED )
            {
                if( ImGuiViewport * viewport = ImGui::FindViewportByPlatformHandle( (void*) SDL_GetWindowFromID( event.window.windowID ) ) )
                {
                    if( window_event == SDL_WINDOWEVENT_CLOSE )
                        viewport->PlatformRequestClose = true;
                    if( window_event == SDL_WINDOWEVENT_MOVED )
                        viewport->PlatformRequestMove = true;
                    if( window_event == SDL_WINDOWEVENT_RESIZED )
                        viewport->PlatformRequestResize = true;
                }
            }
            break;
        }

        if( event.type == SDL_QUIT )
            quit = true;
        if( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID( SdlWindow ) )
            quit = true;
    }

    // Setup display size
    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize( SdlWindow, &w, &h );
    SDL_GL_GetDrawableSize( SdlWindow, &display_w, &display_h );
    io.DisplaySize = ImVec2( (float) w, (float) h );
    if( w > 0 && h > 0 )
        io.DisplayFramebufferScale = ImVec2( (float) display_w / w, (float) display_h / h );

    // Setup time step
    static Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64        current_time = SDL_GetPerformanceCounter();
    io.DeltaTime = ( CurTime > 0 ? (float) ( (double) ( current_time - CurTime ) / frequency ) : 1.0f / 60.0f );
    CurTime = current_time;

    // Mouse state
    io.MouseHoveredViewport = 0;

    if( io.WantSetMousePos )
    {
        if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
            SDL_WarpMouseGlobal( (int) io.MousePos.x, (int) io.MousePos.y );
        else
            SDL_WarpMouseInWindow( SdlWindow, (int) io.MousePos.x, (int) io.MousePos.y );
    }
    else
    {
        io.MousePos = ImVec2( -FLT_MAX, -FLT_MAX );
    }

    int    mouse_x_local, mouse_y_local;
    Uint32 mouse_buttons = SDL_GetMouseState( &mouse_x_local, &mouse_y_local );
    io.MouseDown[ 0 ] = ( MousePressed[ 0 ] || ( mouse_buttons & SDL_BUTTON( SDL_BUTTON_LEFT ) ) != 0 );
    io.MouseDown[ 1 ] = ( MousePressed[ 1 ] || ( mouse_buttons & SDL_BUTTON( SDL_BUTTON_RIGHT ) ) != 0 );
    io.MouseDown[ 2 ] = ( MousePressed[ 2 ] || ( mouse_buttons & SDL_BUTTON( SDL_BUTTON_MIDDLE ) ) != 0 );
    MousePressed[ 0 ] = MousePressed[ 1 ] = MousePressed[ 2 ] = false;

    #if !defined ( FO_WEB ) && !defined ( FO_ANDROID ) && !defined ( FO_IOS )
    int mouse_x_global, mouse_y_global;
    SDL_GetGlobalMouseState( &mouse_x_global, &mouse_y_global );

    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        if( SDL_Window * focused_window = SDL_GetKeyboardFocus() )
            if( ImGui::FindViewportByPlatformHandle( (void*) focused_window ) != nullptr )
                io.MousePos = ImVec2( (float) mouse_x_global, (float) mouse_y_global );
    }
    else
    {
        if( SDL_GetWindowFlags( SdlWindow ) & SDL_WINDOW_INPUT_FOCUS )
        {
            int window_x, window_y;
            SDL_GetWindowPosition( SdlWindow, &window_x, &window_y );
            io.MousePos = ImVec2( (float) ( mouse_x_global - window_x ), (float) ( mouse_y_global - window_y ) );
        }
    }

    bool any_mouse_button_down = ImGui::IsAnyMouseDown();
    SDL_CaptureMouse( any_mouse_button_down ? SDL_TRUE : SDL_FALSE );

    #else
    if( SDL_GetWindowFlags( SdlWindow ) & SDL_WINDOW_INPUT_FOCUS )
        io.MousePos = ImVec2( (float) mouse_x_local, (float) mouse_y_local );
    #endif

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if( io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None )
    {
        SDL_ShowCursor( SDL_FALSE );
    }
    else
    {
        SDL_SetCursor( MouseCursors[ imgui_cursor ] ? MouseCursors[ imgui_cursor ] : MouseCursors[ ImGuiMouseCursor_Arrow ] );
        SDL_ShowCursor( SDL_TRUE );
    }

    ImGui::NewFrame();

    return !quit;
}

void AppGui::EndFrame()
{
    RUNTIME_ASSERT( InitCalled );

    #ifdef FO_HAVE_DX
    if( UseDirectX )
    {
        EndFrameDX();
        return;
    }
    #endif

    ImGui::Render();

    ImGuiIO&      io = ImGui::GetIO();
    GL( glViewport( 0, 0, (int) io.DisplaySize.x, (int) io.DisplaySize.y ) );
    static ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );
    GL( glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w ) );
    GL( glClear( GL_COLOR_BUFFER_BIT ) );

    RenderDrawData( ImGui::GetDrawData() );

    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        SDL_Window*   backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent( backup_current_window, backup_current_context );
    }

    GL( glDisable( GL_SCISSOR_TEST ) );
    GL( glBindTexture( GL_TEXTURE_2D, 0 ) );
    if( !FixedPipeline )
    {
        if( GL_HAS( vertex_array_object ) )
            GL( glBindVertexArray( 0 ) );
        GL( glUseProgram( 0 ) );
    }

    SDL_GL_SwapWindow( SdlWindow );

    Thread::Sleep( 10 );
}

static const char* GetClipboardText( void* )
{
    if( ClipboardTextData )
        SDL_free( ClipboardTextData );
    ClipboardTextData = SDL_GetClipboardText();
    return ClipboardTextData;
}

static void SetClipboardText( void*, const char* text )
{
    SDL_SetClipboardText( text );
}

static void RenderDrawData( ImDrawData* draw_data )
{
    // Avoid rendering when minimized
    int fb_width = (int) ( draw_data->DisplaySize.x * draw_data->FramebufferScale.x );
    int fb_height = (int) ( draw_data->DisplaySize.y * draw_data->FramebufferScale.y );
    if( fb_width == 0 || fb_height == 0 )
        return;

    // Setup GL state
    GLuint vao = 0;
    if( !FixedPipeline )
    {
        if( GL_HAS( vertex_array_object ) )
            GL( glGenVertexArrays( 1, &vao ) );
    }
    SetupRenderState( draw_data, fb_width, fb_height, vao );

    // Scissor/clipping
    ImVec2 clip_off = draw_data->DisplayPos;
    ImVec2 clip_scale = draw_data->FramebufferScale;

    // Render command lists
    for( int n = 0; n < draw_data->CmdListsCount; n++ )
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[ n ];
        const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
        const ImDrawIdx*  idx_buffer = cmd_list->IdxBuffer.Data;

        if( FixedPipeline )
        {
            #ifndef FO_OGL_ES
            GL( glVertexPointer( 2, GL_FLOAT, sizeof( ImDrawVert ), (const GLvoid*) ( (const char*) vtx_buffer + IM_OFFSETOF( ImDrawVert, pos ) ) ) );
            GL( glTexCoordPointer( 2, GL_FLOAT, sizeof( ImDrawVert ), (const GLvoid*) ( (const char*) vtx_buffer + IM_OFFSETOF( ImDrawVert, uv ) ) ) );
            GL( glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( ImDrawVert ), (const GLvoid*) ( (const char*) vtx_buffer + IM_OFFSETOF( ImDrawVert, col ) ) ) );
            #endif
        }
        else
        {
            GL( glBufferData( GL_ARRAY_BUFFER, (GLsizeiptr) cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ), (const GLvoid*) vtx_buffer, GL_STREAM_DRAW ) );
            GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ), (const GLvoid*) idx_buffer, GL_STREAM_DRAW ) );
        }

        for( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[ cmd_i ];
            if( pcmd->UserCallback )
            {
                if( pcmd->UserCallback == ImDrawCallback_ResetRenderState )
                    SetupRenderState( draw_data, fb_width, fb_height, vao );
                else
                    pcmd->UserCallback( cmd_list, pcmd );
            }
            else
            {
                ImVec4 clip_rect;
                clip_rect.x = ( pcmd->ClipRect.x - clip_off.x ) * clip_scale.x;
                clip_rect.y = ( pcmd->ClipRect.y - clip_off.y ) * clip_scale.y;
                clip_rect.z = ( pcmd->ClipRect.z - clip_off.x ) * clip_scale.x;
                clip_rect.w = ( pcmd->ClipRect.w - clip_off.y ) * clip_scale.y;

                if( clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f )
                {
                    GL( glScissor( (int) clip_rect.x, (int) ( fb_height - clip_rect.w ), (int) ( clip_rect.z - clip_rect.x ), (int) ( clip_rect.w - clip_rect.y ) ) );
                    GL( glBindTexture( GL_TEXTURE_2D, (GLuint) (intptr_t) pcmd->TextureId ) );

                    if( !FixedPipeline )
                    {
                        #ifndef FO_OGL_ES
                        GL( glDrawElementsBaseVertex( GL_TRIANGLES, (GLsizei) pcmd->ElemCount, sizeof( ImDrawIdx ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                                                      (void*) (intptr_t) ( pcmd->IdxOffset * sizeof( ImDrawIdx ) ), (GLint) pcmd->VtxOffset ) );
                        #else
                        GL( glDrawElements( GL_TRIANGLES, (GLsizei) pcmd->ElemCount, sizeof( ImDrawIdx ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                                            (void*) (intptr_t) ( pcmd->IdxOffset * sizeof( ImDrawIdx ) ) ) );
                        #endif
                    }
                    else
                    {
                        #ifndef FO_OGL_ES
                        GL( glDrawElements( GL_TRIANGLES, (GLsizei) pcmd->ElemCount, sizeof( ImDrawIdx ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer ) );
                        #endif
                    }
                }
            }

            if( FixedPipeline )
                idx_buffer += pcmd->ElemCount;
        }
    }

    if( !FixedPipeline )
    {
        if( GL_HAS( vertex_array_object ) )
            GL( glDeleteVertexArrays( 1, &vao ) );
    }
}

static void SetupRenderState( ImDrawData* draw_data, int fb_width, int fb_height, GLuint vao )
{
    GL( glViewport( 0, 0, (GLsizei) fb_width, (GLsizei) fb_height ) );

    if( !FixedPipeline )
    {
        float       l = draw_data->DisplayPos.x;
        float       r = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float       t = draw_data->DisplayPos.y;
        float       b = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        const float ortho_projection[ 4 ][ 4 ] =
        {
            { 2.0f / ( r - l ),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f / ( t - b ),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { ( r + l ) / ( l - r ),  ( t + b ) / ( b - t ),  0.0f,   1.0f },
        };

        GL( glEnable( GL_SCISSOR_TEST ) );
        GL( glUseProgram( ShaderHandle ) );
        GL( glUniform1i( AttribLocationTex, 0 ) );
        GL( glUniformMatrix4fv( AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[ 0 ][ 0 ] ) );
        GL( glActiveTexture( GL_TEXTURE0 ) );
        #ifdef GL_SAMPLER_BINDING
        GL( glBindSampler( 0, 0 ) );
        #endif
        if( GL_HAS( vertex_array_object ) )
            GL( glBindVertexArray( vao ) );
        GL( glBindBuffer( GL_ARRAY_BUFFER, VboHandle ) );
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ElementsHandle ) );
        GL( glEnableVertexAttribArray( AttribLocationVtxPos ) );
        GL( glEnableVertexAttribArray( AttribLocationVtxUV ) );
        GL( glEnableVertexAttribArray( AttribLocationVtxColor ) );
        GL( glVertexAttribPointer( AttribLocationVtxPos, 2, GL_FLOAT, GL_FALSE, sizeof( ImDrawVert ), (GLvoid*) IM_OFFSETOF( ImDrawVert, pos ) ) );
        GL( glVertexAttribPointer( AttribLocationVtxUV, 2, GL_FLOAT, GL_FALSE, sizeof( ImDrawVert ), (GLvoid*) IM_OFFSETOF( ImDrawVert, uv ) ) );
        GL( glVertexAttribPointer( AttribLocationVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( ImDrawVert ), (GLvoid*) IM_OFFSETOF( ImDrawVert, col ) ) );
    }
    else
    {
        #ifndef FO_OGL_ES
        GL( glEnableClientState( GL_VERTEX_ARRAY ) );
        GL( glEnableClientState( GL_TEXTURE_COORD_ARRAY ) );
        GL( glEnableClientState( GL_COLOR_ARRAY ) );
        GL( glMatrixMode( GL_MODELVIEW ) );
        GL( glLoadIdentity() );
        GL( glMatrixMode( GL_PROJECTION ) );
        GL( glLoadIdentity() );
        GL( glOrtho( draw_data->DisplayPos.x, draw_data->DisplayPos.x + draw_data->DisplaySize.x,
                     draw_data->DisplayPos.y + draw_data->DisplaySize.y, draw_data->DisplayPos.y, -1.0f, 1.0f ) );
        #endif
    }
}

static void Platform_CreateWindow( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = IM_NEW( ImGuiViewportDataSDL2 ) ();
    viewport->PlatformUserData = data;

    ImGuiViewport*         main_viewport = ImGui::GetMainViewport();
    ImGuiViewportDataSDL2* main_viewport_data = (ImGuiViewportDataSDL2*) main_viewport->PlatformUserData;

    // Share GL resources with main context
    bool          use_opengl = ( main_viewport_data->GLContext != nullptr );
    SDL_GLContext backup_context = nullptr;
    if( use_opengl )
    {
        backup_context = SDL_GL_GetCurrentContext();
        SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1 );
        SDL_GL_MakeCurrent( main_viewport_data->Window, main_viewport_data->GLContext );
    }

    Uint32 sdl_flags = 0;
    sdl_flags |= use_opengl ? SDL_WINDOW_OPENGL : SDL_WINDOW_VULKAN;
    sdl_flags |= SDL_GetWindowFlags( SdlWindow ) & SDL_WINDOW_ALLOW_HIGHDPI;
    sdl_flags |= SDL_WINDOW_HIDDEN;
    sdl_flags |= ( viewport->Flags & ImGuiViewportFlags_NoDecoration ) ? SDL_WINDOW_BORDERLESS : 0;
    sdl_flags |= ( viewport->Flags & ImGuiViewportFlags_NoDecoration ) ? 0 : SDL_WINDOW_RESIZABLE;
    sdl_flags |= ( viewport->Flags & ImGuiViewportFlags_TopMost ) ? SDL_WINDOW_ALWAYS_ON_TOP : 0;

    data->Window = SDL_CreateWindow( "No Title Yet", (int) viewport->Pos.x, (int) viewport->Pos.y,
                                     (int) viewport->Size.x, (int) viewport->Size.y, sdl_flags );
    data->WindowOwned = true;
    if( use_opengl )
    {
        data->GLContext = SDL_GL_CreateContext( data->Window );
        SDL_GL_SetSwapInterval( 0 );
    }
    if( use_opengl && backup_context )
        SDL_GL_MakeCurrent( data->Window, backup_context );

    viewport->PlatformHandle = (void*) data->Window;

    #ifdef FO_WINDOWS
    SDL_SysWMinfo info;
    SDL_VERSION( &info.version );
    if( SDL_GetWindowWMInfo( data->Window, &info ) )
        viewport->PlatformHandleRaw = info.info.win.window;
    #endif
}

static void Platform_DestroyWindow( ImGuiViewport* viewport )
{
    if( ImGuiViewportDataSDL2 * data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData )
    {
        if( data->GLContext && data->WindowOwned )
            SDL_GL_DeleteContext( data->GLContext );
        if( data->Window && data->WindowOwned )
            SDL_DestroyWindow( data->Window );
        data->GLContext = nullptr;
        data->Window = nullptr;
        IM_DELETE( data );
    }
    viewport->PlatformUserData = viewport->PlatformHandle = nullptr;
}

static void Platform_ShowWindow( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;

    #ifdef FO_WINDOWS
    HWND hwnd = (HWND) viewport->PlatformHandleRaw;

    // Hide icon from task bar
    if( viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon )
    {
        LONG ex_style = GetWindowLong( hwnd, GWL_EXSTYLE );
        ex_style &= ~WS_EX_APPWINDOW;
        ex_style |= WS_EX_TOOLWINDOW;
        SetWindowLong( hwnd, GWL_EXSTYLE, ex_style );
    }

    // SDL always activate/focus windows
    if( viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing )
    {
        ShowWindow( hwnd, SW_SHOWNA );
        return;
    }
    #endif

    SDL_ShowWindow( data->Window );
}

static ImVec2 Platform_GetWindowPos( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    int                    x = 0, y = 0;
    SDL_GetWindowPosition( data->Window, &x, &y );
    return ImVec2( (float) x, (float) y );
}

static void Platform_SetWindowPos( ImGuiViewport* viewport, ImVec2 pos )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_SetWindowPosition( data->Window, (int) pos.x, (int) pos.y );
}

static ImVec2 Platform_GetWindowSize( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    int                    w = 0, h = 0;
    SDL_GetWindowSize( data->Window, &w, &h );
    return ImVec2( (float) w, (float) h );
}

static void Platform_SetWindowSize( ImGuiViewport* viewport, ImVec2 size )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_SetWindowSize( data->Window, (int) size.x, (int) size.y );
}

static void Platform_SetWindowTitle( ImGuiViewport* viewport, const char* title )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_SetWindowTitle( data->Window, title );
}

static void Platform_SetWindowAlpha( ImGuiViewport* viewport, float alpha )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_SetWindowOpacity( data->Window, alpha );
}

static void Platform_SetWindowFocus( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_RaiseWindow( data->Window );
}

static bool Platform_GetWindowFocus( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    return ( SDL_GetWindowFlags( data->Window ) & SDL_WINDOW_INPUT_FOCUS ) != 0;
}

static bool Platform_GetWindowMinimized( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    return ( SDL_GetWindowFlags( data->Window ) & SDL_WINDOW_MINIMIZED ) != 0;
}

static void Platform_RenderWindow( ImGuiViewport* viewport, void* )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    if( data->GLContext )
        SDL_GL_MakeCurrent( data->Window, data->GLContext );
}

static void Platform_SwapBuffers( ImGuiViewport* viewport, void* )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    if( data->GLContext )
    {
        SDL_GL_MakeCurrent( data->Window, data->GLContext );
        SDL_GL_SwapWindow( data->Window );
    }
}

static int Platform_CreateVkSurface( ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    UNUSED_VARIABLE( vk_allocator );
    SDL_bool               ret = SDL_Vulkan_CreateSurface( data->Window, (VkInstance) vk_instance, (VkSurfaceKHR*) out_vk_surface );
    return ret ? 0 : 1;     // VK_SUCCESS : VK_NOT_READY
}

static void Renderer_RenderWindow( ImGuiViewport* viewport, void* )
{
    if( !( viewport->Flags & ImGuiViewportFlags_NoRendererClear ) )
    {
        ImVec4 clear_color = ImVec4( 0.0f, 0.0f, 0.0f, 1.0f );
        GL( glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w ) );
        GL( glClear( GL_COLOR_BUFFER_BIT ) );
    }

    RenderDrawData( viewport->DrawData );
}
