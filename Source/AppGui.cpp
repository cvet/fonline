#include "AppGui.h"

// OpenGL
#ifndef FO_OGL_ES
# include "GL/glew.h"
#else
# include <GLES2/gl2.h>
#endif

#ifdef GL
# undef GL
#endif
#define GL( expr )    do { expr; if( GameOpt.OpenGLDebug ) { GLenum err__ = glGetError(); RUNTIME_ASSERT_STR( err__ == GL_NO_ERROR, _str( # expr " error {:#X}", err__ ) ); } } while( false )

// SDL
#include "SDL.h"
#include "SDL_syswm.h"
#include "SDL_opengl.h"

#ifdef FO_MSVC
# pragma comment( lib, "opengl32.lib" )
# pragma comment( lib, "glu32.lib" )
#endif

static bool InitCalled = false;
static bool UseDirectX = false;

// OpenGL Data
static GLuint       FontTexture = 0;
static GLuint       ShaderHandle = 0, VertHandle = 0, FragHandle = 0;
static int          AttribLocationTex = 0, AttribLocationProjMtx = 0;
static int          AttribLocationVtxPos = 0, AttribLocationVtxUV = 0, AttribLocationVtxColor = 0;
static unsigned int VboHandle = 0, ElementsHandle = 0;

// Data
static SDL_Window*   SdlWindow = nullptr;
static SDL_GLContext GlContext = nullptr;
static Uint64        CurTime = 0;
static bool          MousePressed[ 3 ] = { false, false, false };
static SDL_Cursor*   MouseCursors[ ImGuiMouseCursor_COUNT ] = { 0 };
static char*         ClipboardTextData = nullptr;

static const char* GetClipboardText( void* );
static void        SetClipboardText( void*, const char* text );
static bool        CreateDeviceObjects();
static bool        CheckShader( GLuint handle, const char* desc );
static bool        CheckProgram( GLuint handle, const char* desc );
static void        RenderDrawData( ImDrawData* draw_data );
static void        SetupRenderState( ImDrawData* draw_data, int fb_width, int fb_height, GLuint vertex_array_object );
static void        SdlInitPlatformInterface();
static void        GlInitPlatformInterface();

// Main code
bool AppGui::Init( const string& app_name, bool use_dx, bool maximized )
{
    RUNTIME_ASSERT( !InitCalled );
    InitCalled = true;

    // DirectX backend
    if( use_dx )
    {
        #ifdef FO_HAVE_DX
        if( !InitDX( maximized ) )
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
    #ifdef FO_MAC
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
    #else
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
    #endif
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
    SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );

    SDL_WindowFlags window_flags = (SDL_WindowFlags) ( SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED );
    SDL_Window*     window = SDL_CreateWindow( app_name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, window_flags );
    SDL_GLContext   gl_context = SDL_GL_CreateContext( window );
    SDL_GL_MakeCurrent( window, gl_context );
    SDL_GL_SetSwapInterval( 0 );

    SdlWindow = window;
    GlContext = gl_context;

    #ifndef FO_OGL_ES
    // Init GLEW
    GLenum glew_result = glewInit();
    if( glew_result != GLEW_OK )
    {
        WriteLog( "GLEW not initialized, result {}.\n", glew_result );
        return false;
    }
    #endif

    // Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    // ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();
    ImGui::StyleColorsLight();

    // Setup back-end capabilities flags
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;           // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;      // We can create multi-viewports on the Platform side (optional)

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
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

    // Our mouse update function expect PlatformHandle to be filled for the main viewport
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = (void*) window;
    #ifdef FO_WINDOWS
    SDL_SysWMinfo info;
    SDL_VERSION( &info.version );
    if( SDL_GetWindowWMInfo( window, &info ) )
        main_viewport->PlatformHandleRaw = info.info.win.window;
    #endif

    if( ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) && ( io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports ) )
        SdlInitPlatformInterface();

    // Setup back-end capabilities flags
    #ifndef FO_OGL_ES
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;      // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    #endif
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;      // We can create multi-viewports on the Renderer side (optional)

    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
        GlInitPlatformInterface();

    // Setup GL stuff
    CreateDeviceObjects();

    // Build texture atlas
    unsigned char* pixels;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );     // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

    // Upload texture to graphics system
    GLint last_texture;
    GL( glGetIntegerv( GL_TEXTURE_BINDING_2D, &last_texture ) );
    GL( glGenTextures( 1, &FontTexture ) );
    GL( glBindTexture( GL_TEXTURE_2D, FontTexture ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
    GL( glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 ) );
    GL( glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels ) );

    // Store our identifier
    io.Fonts->TexID = (ImTextureID) (intptr_t) FontTexture;

    // Restore state
    GL( glBindTexture( GL_TEXTURE_2D, last_texture ) );

    return true;
}

bool AppGui::BeginFrame()
{
    #ifdef FO_HAVE_DX
    if( UseDirectX )
        return BeginFrameDX();
    #endif

    ImGuiIO& io = ImGui::GetIO();

    // Poll and handle events
    SDL_Event event;
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
            IM_ASSERT( key >= 0 && key < IM_ARRAYSIZE( io.KeysDown ) );
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
                if( ImGuiViewport * viewport = ImGui::FindViewportByPlatformHandle( (void*) SDL_GetWindowFromID( event.window.windowID ) ) )
                {
                    if( window_event == SDL_WINDOWEVENT_CLOSE )
                        viewport->PlatformRequestClose = true;
                    if( window_event == SDL_WINDOWEVENT_MOVED )
                        viewport->PlatformRequestMove = true;
                    if( window_event == SDL_WINDOWEVENT_RESIZED )
                        viewport->PlatformRequestResize = true;
                }
            break;
        }

        if( event.type == SDL_QUIT )
            return false;
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
    io.DeltaTime = CurTime > 0 ? (float) ( (double) ( current_time - CurTime ) / frequency ) : 1.0f / 60.0f;
    CurTime = current_time;

    // Set OS mouse position if requested
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
    io.MouseDown[ 0 ] = MousePressed[ 0 ] || ( mouse_buttons & SDL_BUTTON( SDL_BUTTON_LEFT ) ) != 0;  // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[ 1 ] = MousePressed[ 1 ] || ( mouse_buttons & SDL_BUTTON( SDL_BUTTON_RIGHT ) ) != 0;
    io.MouseDown[ 2 ] = MousePressed[ 2 ] || ( mouse_buttons & SDL_BUTTON( SDL_BUTTON_MIDDLE ) ) != 0;
    MousePressed[ 0 ] = MousePressed[ 1 ] = MousePressed[ 2 ] = false;

    #if !defined ( FO_WEB ) && !defined ( FO_ANDROID ) && !defined ( FO_IOS )
    int mouse_x_global, mouse_y_global;
    SDL_GetGlobalMouseState( &mouse_x_global, &mouse_y_global );

    if( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        // Multi-viewport mode
        if( SDL_Window * focused_window = SDL_GetKeyboardFocus() )
            if( ImGui::FindViewportByPlatformHandle( (void*) focused_window ) != nullptr )
                io.MousePos = ImVec2( (float) mouse_x_global, (float) mouse_y_global );
    }
    else
    {
        // Single-viewport mode
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

    if( !( io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange ) )
    {
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
    }

    ImGui::NewFrame();
    return true;
}

void AppGui::EndFrame()
{
    #ifdef FO_HAVE_DX
    if( UseDirectX )
    {
        EndFrameDX();
        return;
    }
    #endif

    ImGuiIO& io = ImGui::GetIO();

    // Rendering
    ImGui::Render();
    GL( glViewport( 0, 0, (int) io.DisplaySize.x, (int) io.DisplaySize.y ) );
    static ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );
    GL( glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w ) );
    GL( glClear( GL_COLOR_BUFFER_BIT ) );

    RenderDrawData( ImGui::GetDrawData() );

    SDL_GL_SwapWindow( SdlWindow );
    Thread_Sleep( 1 );
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

static bool CreateDeviceObjects()
{
    // Parse GLSL version string
    const GLchar* vertex_shader =
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 UV;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "    Frag_UV = UV;\n"
        "    Frag_Color = Color;\n"
        "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    const GLchar* fragment_shader =
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
        "}\n";

    // Create shaders
    const GLchar* vertex_shader_with_version[ 2 ] = { "#version 130\n", vertex_shader };
    VertHandle = glCreateShader( GL_VERTEX_SHADER );
    glShaderSource( VertHandle, 2, vertex_shader_with_version, nullptr );
    glCompileShader( VertHandle );
    CheckShader( VertHandle, "vertex shader" );

    const GLchar* fragment_shader_with_version[ 2 ] = { "#version 130\n", fragment_shader };
    FragHandle = glCreateShader( GL_FRAGMENT_SHADER );
    glShaderSource( FragHandle, 2, fragment_shader_with_version, nullptr );
    glCompileShader( FragHandle );
    CheckShader( FragHandle, "fragment shader" );

    ShaderHandle = glCreateProgram();
    glAttachShader( ShaderHandle, VertHandle );
    glAttachShader( ShaderHandle, FragHandle );
    glLinkProgram( ShaderHandle );
    CheckProgram( ShaderHandle, "shader program" );

    AttribLocationTex = glGetUniformLocation( ShaderHandle, "Texture" );
    AttribLocationProjMtx = glGetUniformLocation( ShaderHandle, "ProjMtx" );
    AttribLocationVtxPos = glGetAttribLocation( ShaderHandle, "Position" );
    AttribLocationVtxUV = glGetAttribLocation( ShaderHandle, "UV" );
    AttribLocationVtxColor = glGetAttribLocation( ShaderHandle, "Color" );

    // Create buffers
    glGenBuffers( 1, &VboHandle );
    glGenBuffers( 1, &ElementsHandle );

    // Build texture atlas
    ImGuiIO&       io = ImGui::GetIO();
    unsigned char* pixels;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    glGenTextures( 1, &FontTexture );
    glBindTexture( GL_TEXTURE_2D, FontTexture );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    #ifdef GL_UNPACK_ROW_LENGTH
    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
    #endif
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );

    io.Fonts->TexID = (ImTextureID) (intptr_t) FontTexture;

    // Reset GL state
    glBindTexture( GL_TEXTURE_2D, 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    #ifndef FO_OGL_ES
    glBindVertexArray( 0 );
    #endif

    return true;
}

static bool CheckShader( GLuint handle, const char* desc )
{
    GLint status = 0, log_length = 0;
    glGetShaderiv( handle, GL_COMPILE_STATUS, &status );
    glGetShaderiv( handle, GL_INFO_LOG_LENGTH, &log_length );
    if( (GLboolean) status == GL_FALSE )
        WriteLog( "Failed to compile {}.\n", desc );
    if( log_length > 1 )
    {
        ImVector< char > buf;
        buf.resize( (int) ( log_length + 1 ) );
        glGetShaderInfoLog( handle, log_length, nullptr, (GLchar*) buf.begin() );
        WriteLog( "{}\n", buf.begin() );
    }
    return (GLboolean) status == GL_TRUE;
}

static bool CheckProgram( GLuint handle, const char* desc )
{
    GLint status = 0, log_length = 0;
    glGetProgramiv( handle, GL_LINK_STATUS, &status );
    glGetProgramiv( handle, GL_INFO_LOG_LENGTH, &log_length );
    if( (GLboolean) status == GL_FALSE )
        WriteLog( "Failed to link {} (with GLSL '{}')\n", desc, 130 );
    if( log_length > 1 )
    {
        ImVector< char > buf;
        buf.resize( (int) ( log_length + 1 ) );
        glGetProgramInfoLog( handle, log_length, nullptr, (GLchar*) buf.begin() );
        WriteLog( "{}\n", buf.begin() );
    }
    return (GLboolean) status == GL_TRUE;
}

static void SetupRenderState( ImDrawData* draw_data, int fb_width, int fb_height, GLuint vertex_array_object )
{
    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    GL( glEnable( GL_BLEND ) );
    GL( glBlendEquation( GL_FUNC_ADD ) );
    GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
    GL( glDisable( GL_CULL_FACE ) );
    GL( glDisable( GL_DEPTH_TEST ) );
    GL( glEnable( GL_SCISSOR_TEST ) );
    #ifdef GL_POLYGON_MODE
    GL( glPolygonMode( GL_FRONT_AND_BACK, GL_FILL ) );
    #endif

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    GL( glViewport( 0, 0, (GLsizei) fb_width, (GLsizei) fb_height ) );
    float       L = draw_data->DisplayPos.x;
    float       R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float       T = draw_data->DisplayPos.y;
    float       B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float ortho_projection[ 4 ][ 4 ] =
    {
        { 2.0f / ( R - L ),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f / ( T - B ),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { ( R + L ) / ( L - R ),  ( T + B ) / ( B - T ),  0.0f,   1.0f },
    };
    GL( glUseProgram( ShaderHandle ) );
    GL( glUniform1i( AttribLocationTex, 0 ) );
    GL( glUniformMatrix4fv( AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[ 0 ][ 0 ] ) );
    #ifdef GL_SAMPLER_BINDING
    GL( glBindSampler( 0, 0 ) );   // We use combined texture/sampler state. Applications using GL 3.3 may set that otherwise.
    #endif

    (void) vertex_array_object;
    #ifndef FO_OGL_ES
    GL( glBindVertexArray( vertex_array_object ) );
    #endif

    // Bind vertex/index buffers and setup attributes for ImDrawVert
    GL( glBindBuffer( GL_ARRAY_BUFFER, VboHandle ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ElementsHandle ) );
    GL( glEnableVertexAttribArray( AttribLocationVtxPos ) );
    GL( glEnableVertexAttribArray( AttribLocationVtxUV ) );
    GL( glEnableVertexAttribArray( AttribLocationVtxColor ) );
    GL( glVertexAttribPointer( AttribLocationVtxPos,   2, GL_FLOAT,         GL_FALSE, sizeof( ImDrawVert ), (GLvoid*) IM_OFFSETOF( ImDrawVert, pos ) ) );
    GL( glVertexAttribPointer( AttribLocationVtxUV,    2, GL_FLOAT,         GL_FALSE, sizeof( ImDrawVert ), (GLvoid*) IM_OFFSETOF( ImDrawVert, uv ) ) );
    GL( glVertexAttribPointer( AttribLocationVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof( ImDrawVert ), (GLvoid*) IM_OFFSETOF( ImDrawVert, col ) ) );
}

// OpenGL3 Render function
static void RenderDrawData( ImDrawData* draw_data )
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int) ( draw_data->DisplaySize.x * draw_data->FramebufferScale.x );
    int fb_height = (int) ( draw_data->DisplaySize.y * draw_data->FramebufferScale.y );
    if( fb_width <= 0 || fb_height <= 0 )
        return;

    // Backup GL state
    GLenum last_active_texture;
    GL( glGetIntegerv( GL_ACTIVE_TEXTURE, (GLint*) &last_active_texture ) );
    GL( glActiveTexture( GL_TEXTURE0 ) );
    GLint last_program;
    GL( glGetIntegerv( GL_CURRENT_PROGRAM, &last_program ) );
    GLint last_texture;
    GL( glGetIntegerv( GL_TEXTURE_BINDING_2D, &last_texture ) );
    #ifdef GL_SAMPLER_BINDING
    GLint last_sampler;
    GL( glGetIntegerv( GL_SAMPLER_BINDING, &last_sampler ) );
    #endif
    GLint last_array_buffer;
    GL( glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &last_array_buffer ) );
    #ifndef FO_OGL_ES
    GLint last_vertex_array_object;
    GL( glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &last_vertex_array_object ) );
    #endif
    #ifdef GL_POLYGON_MODE
    GLint last_polygon_mode[ 2 ];
    GL( glGetIntegerv( GL_POLYGON_MODE, last_polygon_mode ) );
    #endif
    GLint     last_viewport[ 4 ];
    GL( glGetIntegerv( GL_VIEWPORT, last_viewport ) );
    GLint     last_scissor_box[ 4 ];
    GL( glGetIntegerv( GL_SCISSOR_BOX, last_scissor_box ) );
    GLenum    last_blend_src_rgb;
    GL( glGetIntegerv( GL_BLEND_SRC_RGB, (GLint*) &last_blend_src_rgb ) );
    GLenum    last_blend_dst_rgb;
    GL( glGetIntegerv( GL_BLEND_DST_RGB, (GLint*) &last_blend_dst_rgb ) );
    GLenum    last_blend_src_alpha;
    GL( glGetIntegerv( GL_BLEND_SRC_ALPHA, (GLint*) &last_blend_src_alpha ) );
    GLenum    last_blend_dst_alpha;
    GL( glGetIntegerv( GL_BLEND_DST_ALPHA, (GLint*) &last_blend_dst_alpha ) );
    GLenum    last_blend_equation_rgb;
    GL( glGetIntegerv( GL_BLEND_EQUATION_RGB, (GLint*) &last_blend_equation_rgb ) );
    GLenum    last_blend_equation_alpha;
    GL( glGetIntegerv( GL_BLEND_EQUATION_ALPHA, (GLint*) &last_blend_equation_alpha ) );
    GLboolean last_enable_blend = glIsEnabled( GL_BLEND );
    GLboolean last_enable_cull_face = glIsEnabled( GL_CULL_FACE );
    GLboolean last_enable_depth_test = glIsEnabled( GL_DEPTH_TEST );
    GLboolean last_enable_scissor_test = glIsEnabled( GL_SCISSOR_TEST );
    bool      clip_origin_lower_left = true;
    #if defined ( GL_CLIP_ORIGIN ) && !defined ( __APPLE__ )
    GLenum    last_clip_origin = 0;
    GL( glGetIntegerv( GL_CLIP_ORIGIN, (GLint*) &last_clip_origin ) );                             // Support for GL 4.5's glClipControl(GL_UPPER_LEFT)
    if( last_clip_origin == GL_UPPER_LEFT )
        clip_origin_lower_left = false;
    #endif

    // Setup desired GL state
    // Recreate the VAO every time (this is to easily allow multiple GL contexts to be rendered to. VAO are not shared among GL contexts)
    // The renderer would actually work without any VAO bound, but then our VertexAttrib calls would overwrite the default one currently bound.
    GLuint vertex_array_object = 0;
    #ifndef FO_OGL_ES
    GL( glGenVertexArrays( 1, &vertex_array_object ) );
    #endif
    SetupRenderState( draw_data, fb_width, fb_height, vertex_array_object );

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    for( int n = 0; n < draw_data->CmdListsCount; n++ )
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[ n ];

        // Upload vertex/index buffers
        GL( glBufferData( GL_ARRAY_BUFFER, (GLsizeiptr) cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ), (const GLvoid*) cmd_list->VtxBuffer.Data, GL_STREAM_DRAW ) );
        GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ), (const GLvoid*) cmd_list->IdxBuffer.Data, GL_STREAM_DRAW ) );

        for( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[ cmd_i ];
            if( pcmd->UserCallback != nullptr )
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if( pcmd->UserCallback == ImDrawCallback_ResetRenderState )
                    SetupRenderState( draw_data, fb_width, fb_height, vertex_array_object );
                else
                    pcmd->UserCallback( cmd_list, pcmd );
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = ( pcmd->ClipRect.x - clip_off.x ) * clip_scale.x;
                clip_rect.y = ( pcmd->ClipRect.y - clip_off.y ) * clip_scale.y;
                clip_rect.z = ( pcmd->ClipRect.z - clip_off.x ) * clip_scale.x;
                clip_rect.w = ( pcmd->ClipRect.w - clip_off.y ) * clip_scale.y;

                if( clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f )
                {
                    // Apply scissor/clipping rectangle
                    if( clip_origin_lower_left )
                        GL( glScissor( (int) clip_rect.x, (int) ( fb_height - clip_rect.w ), (int) ( clip_rect.z - clip_rect.x ), (int) ( clip_rect.w - clip_rect.y ) ) );
                    else
                        GL( glScissor( (int) clip_rect.x, (int) clip_rect.y, (int) clip_rect.z, (int) clip_rect.w ) );                  // Support for GL 4.5 rarely used glClipControl(GL_UPPER_LEFT)

                    // Bind texture, Draw
                    GL( glBindTexture( GL_TEXTURE_2D, (GLuint) (intptr_t) pcmd->TextureId ) );
                    #ifndef FO_OGL_ES
                    GL( glDrawElementsBaseVertex( GL_TRIANGLES, (GLsizei) pcmd->ElemCount, sizeof( ImDrawIdx ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*) (intptr_t) ( pcmd->IdxOffset * sizeof( ImDrawIdx ) ), (GLint) pcmd->VtxOffset ) );
                    #else
                    GL( glDrawElements( GL_TRIANGLES, (GLsizei) pcmd->ElemCount, sizeof( ImDrawIdx ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*) (intptr_t) ( pcmd->IdxOffset * sizeof( ImDrawIdx ) ) ) );
                    #endif
                }
            }
        }
    }

    // Destroy the temporary VAO
    #ifndef FO_OGL_ES
    GL( glDeleteVertexArrays( 1, &vertex_array_object ) );
    #endif

    // Restore modified GL state
    GL( glUseProgram( last_program ) );
    GL( glBindTexture( GL_TEXTURE_2D, last_texture ) );
    #ifdef GL_SAMPLER_BINDING
    GL( glBindSampler( 0, last_sampler ) );
    #endif
    GL( glActiveTexture( last_active_texture ) );
    #ifndef FO_OGL_ES
    GL( glBindVertexArray( last_vertex_array_object ) );
    #endif
    GL( glBindBuffer( GL_ARRAY_BUFFER, last_array_buffer ) );
    GL( glBlendEquationSeparate( last_blend_equation_rgb, last_blend_equation_alpha ) );
    GL( glBlendFuncSeparate( last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha ) );
    if( last_enable_blend )
        GL( glEnable( GL_BLEND ) );
    else
        GL( glDisable( GL_BLEND ) );
    if( last_enable_cull_face )
        GL( glEnable( GL_CULL_FACE ) );
    else
        GL( glDisable( GL_CULL_FACE ) );
    if( last_enable_depth_test )
        GL( glEnable( GL_DEPTH_TEST ) );
    else
        GL( glDisable( GL_DEPTH_TEST ) );
    if( last_enable_scissor_test )
        GL( glEnable( GL_SCISSOR_TEST ) );
    else
        GL( glDisable( GL_SCISSOR_TEST ) );
    #ifdef GL_POLYGON_MODE
    GL( glPolygonMode( GL_FRONT_AND_BACK, (GLenum) last_polygon_mode[ 0 ] ) );
    #endif
    GL( glViewport( last_viewport[ 0 ], last_viewport[ 1 ], (GLsizei) last_viewport[ 2 ], (GLsizei) last_viewport[ 3 ] ) );
    GL( glScissor( last_scissor_box[ 0 ], last_scissor_box[ 1 ], (GLsizei) last_scissor_box[ 2 ], (GLsizei) last_scissor_box[ 3 ] ) );
}

// --------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
// --------------------------------------------------------------------------------------------------------

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
    ~ImGuiViewportDataSDL2() { IM_ASSERT( Window == nullptr && GLContext == nullptr ); }
};

static void SdlCreateWindow( ImGuiViewport* viewport )
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

    data->Window = SDL_CreateWindow( "No Title Yet", (int) viewport->Pos.x, (int) viewport->Pos.y, (int) viewport->Size.x, (int) viewport->Size.y, sdl_flags );
    data->WindowOwned = true;
    if( use_opengl )
    {
        data->GLContext = SDL_GL_CreateContext( data->Window );
        SDL_GL_SetSwapInterval( 0 );
    }
    if( use_opengl && backup_context )
        SDL_GL_MakeCurrent( data->Window, backup_context );

    viewport->PlatformHandle = (void*) data->Window;
    #if defined ( _WIN32 )
    SDL_SysWMinfo info;
    SDL_VERSION( &info.version );
    if( SDL_GetWindowWMInfo( data->Window, &info ) )
        viewport->PlatformHandleRaw = info.info.win.window;
    #endif
}

static void SdlDestroyWindow( ImGuiViewport* viewport )
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

static void SdlShowWindow( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    #if defined ( _WIN32 )
    HWND                   hwnd = (HWND) viewport->PlatformHandleRaw;

    // SDL hack: Hide icon from task bar
    // Note: SDL 2.0.6+ has a SDL_WINDOW_SKIP_TASKBAR flag which is supported under Windows but the way it create the window breaks our seamless transition.
    if( viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon )
    {
        LONG ex_style = ::GetWindowLong( hwnd, GWL_EXSTYLE );
        ex_style &= ~WS_EX_APPWINDOW;
        ex_style |= WS_EX_TOOLWINDOW;
        ::SetWindowLong( hwnd, GWL_EXSTYLE, ex_style );
    }

    // SDL hack: SDL always activate/focus windows :/
    if( viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing )
    {
        ::ShowWindow( hwnd, SW_SHOWNA );
        return;
    }
    #endif

    SDL_ShowWindow( data->Window );
}

static ImVec2 SdlGetWindowPos( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    int                    x = 0, y = 0;
    SDL_GetWindowPosition( data->Window, &x, &y );
    return ImVec2( (float) x, (float) y );
}

static void SdlSetWindowPos( ImGuiViewport* viewport, ImVec2 pos )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_SetWindowPosition( data->Window, (int) pos.x, (int) pos.y );
}

static ImVec2 SdlGetWindowSize( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    int                    w = 0, h = 0;
    SDL_GetWindowSize( data->Window, &w, &h );
    return ImVec2( (float) w, (float) h );
}

static void SdlSetWindowSize( ImGuiViewport* viewport, ImVec2 size )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_SetWindowSize( data->Window, (int) size.x, (int) size.y );
}

static void SdlSetWindowTitle( ImGuiViewport* viewport, const char* title )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_SetWindowTitle( data->Window, title );
}

static void SdlSetWindowAlpha( ImGuiViewport* viewport, float alpha )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_SetWindowOpacity( data->Window, alpha );
}

static void SdlSetWindowFocus( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    SDL_RaiseWindow( data->Window );
}

static bool SdlGetWindowFocus( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    return ( SDL_GetWindowFlags( data->Window ) & SDL_WINDOW_INPUT_FOCUS ) != 0;
}

static bool SdlGetWindowMinimized( ImGuiViewport* viewport )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    return ( SDL_GetWindowFlags( data->Window ) & SDL_WINDOW_MINIMIZED ) != 0;
}

static void SdlRenderWindow( ImGuiViewport* viewport, void* )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    if( data->GLContext )
        SDL_GL_MakeCurrent( data->Window, data->GLContext );
}

static void SdlSwapBuffers( ImGuiViewport* viewport, void* )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    if( data->GLContext )
    {
        SDL_GL_MakeCurrent( data->Window, data->GLContext );
        SDL_GL_SwapWindow( data->Window );
    }
}

// Vulkan support (the Vulkan renderer needs to call a platform-side support function to create the surface)
// SDL is graceful enough to _not_ need <vulkan/vulkan.h> so we can safely include this.
#include <SDL_vulkan.h>
static int SdlCreateVkSurface( ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface )
{
    ImGuiViewportDataSDL2* data = (ImGuiViewportDataSDL2*) viewport->PlatformUserData;
    (void) vk_allocator;
    SDL_bool               ret = SDL_Vulkan_CreateSurface( data->Window, (VkInstance) vk_instance, (VkSurfaceKHR*) out_vk_surface );
    return ret ? 0 : 1;     // ret ? VK_SUCCESS : VK_NOT_READY
}

// FIXME-PLATFORM: SDL doesn't have an event to notify the application of display/monitor changes
static void SdlUpdateMonitors()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Monitors.resize( 0 );
    int              display_count = SDL_GetNumVideoDisplays();
    for( int n = 0; n < display_count; n++ )
    {
        // Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the manifest or at runtime.
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
}

static void SdlInitPlatformInterface()
{
    // Register platform interface (will be coupled with a renderer interface)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = SdlCreateWindow;
    platform_io.Platform_DestroyWindow = SdlDestroyWindow;
    platform_io.Platform_ShowWindow = SdlShowWindow;
    platform_io.Platform_SetWindowPos = SdlSetWindowPos;
    platform_io.Platform_GetWindowPos = SdlGetWindowPos;
    platform_io.Platform_SetWindowSize = SdlSetWindowSize;
    platform_io.Platform_GetWindowSize = SdlGetWindowSize;
    platform_io.Platform_SetWindowFocus = SdlSetWindowFocus;
    platform_io.Platform_GetWindowFocus = SdlGetWindowFocus;
    platform_io.Platform_GetWindowMinimized = SdlGetWindowMinimized;
    platform_io.Platform_SetWindowTitle = SdlSetWindowTitle;
    platform_io.Platform_RenderWindow = SdlRenderWindow;
    platform_io.Platform_SwapBuffers = SdlSwapBuffers;
    platform_io.Platform_SetWindowAlpha = SdlSetWindowAlpha;
    platform_io.Platform_CreateVkSurface = SdlCreateVkSurface;

    // SDL2 by default doesn't pass mouse clicks to the application when the click focused a window. This is getting in the way of our interactions and we disable that behavior.
    SDL_SetHint( SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1" );

    SdlUpdateMonitors();

    // Register main window handle (which is owned by the main application, not by us)
    ImGuiViewport*         main_viewport = ImGui::GetMainViewport();
    ImGuiViewportDataSDL2* data = IM_NEW( ImGuiViewportDataSDL2 ) ();
    data->Window = SdlWindow;
    data->WindowID = SDL_GetWindowID( SdlWindow );
    data->WindowOwned = false;
    data->GLContext = GlContext;
    main_viewport->PlatformUserData = data;
    main_viewport->PlatformHandle = data->Window;
}

// --------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
// --------------------------------------------------------------------------------------------------------

static void GlRenderWindow( ImGuiViewport* viewport, void* )
{
    if( !( viewport->Flags & ImGuiViewportFlags_NoRendererClear ) )
    {
        ImVec4 clear_color = ImVec4( 0.0f, 0.0f, 0.0f, 1.0f );
        glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
        glClear( GL_COLOR_BUFFER_BIT );
    }
    RenderDrawData( viewport->DrawData );
}

static void GlInitPlatformInterface()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_RenderWindow = GlRenderWindow;
}
