//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Todo: move different renderers to separate modules

#include "Application.h"
#include "DiskFileSystem.h"
#include "Log.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#if !FO_HEADLESS
#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_events.h"
#include "SDL_video.h"
#if FO_HAVE_OPENGL
#if !FO_OPENGL_ES
#include "GL/glew.h"
#include "SDL_opengl.h"
#else
#include "SDL_opengles2.h"
#endif
#endif
#if FO_HAVE_DIRECT_3D
#include <d3d11_1.h>
#endif
#endif

Application* App;

#if FO_WINDOWS && FO_DEBUG
static _CrtMemState CrtMemState;
#endif

#if !FO_HEADLESS
#if FO_HAVE_OPENGL
static constexpr auto RING_BUFFER_LENGTH = 300;
#if !FO_OPENGL_ES
#if FO_MAC
#undef glGenVertexArrays
#undef glBindVertexArray
#undef glDeleteVertexArrays
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#define glDeleteVertexArrays glDeleteVertexArraysAPPLE
#endif
#else
#define glGenVertexArrays glGenVertexArraysOES
#define glBindVertexArray glBindVertexArrayOES
#define glDeleteVertexArrays glDeleteVertexArraysOES
#define glGenFramebuffersEXT glGenFramebuffers
#define glBindFramebufferEXT glBindFramebuffer
#define glFramebufferTexture2DEXT glFramebufferTexture2D
#define glRenderbufferStorageEXT glRenderbufferStorage
#define glGenRenderbuffersEXT glGenRenderbuffers
#define glBindRenderbufferEXT glBindRenderbuffer
#define glFramebufferRenderbufferEXT glFramebufferRenderbuffer
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glDeleteRenderbuffersEXT glDeleteRenderbuffers
#define glDeleteFramebuffersEXT glDeleteFramebuffers
#define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_EXT GL_FRAMEBUFFER
#ifndef GL_COLOR_ATTACHMENT0_EXT
#define GL_COLOR_ATTACHMENT0_EXT GL_COLOR_ATTACHMENT0
#endif
#define GL_RENDERBUFFER_EXT GL_RENDERBUFFER
#define GL_DEPTH_ATTACHMENT_EXT GL_DEPTH_ATTACHMENT
#define GL_RENDERBUFFER_BINDING_EXT GL_RENDERBUFFER_BINDING
#define GL_CLAMP GL_CLAMP_TO_EDGE
#define GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_EXT GL_DEPTH24_STENCIL8_OES
#define GL_STENCIL_ATTACHMENT_EXT GL_STENCIL_ATTACHMENT
#define glGetTexImage(a, b, c, d, e)
#define glDrawBuffer(a)
#ifndef GL_MAX_COLOR_TEXTURE_SAMPLES
#define GL_MAX_COLOR_TEXTURE_SAMPLES 0x910E
#endif
#ifndef GL_TEXTURE_2D_MULTISAMPLE
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#endif
#ifndef GL_MAX
#define GL_MAX GL_MAX_EXT
#define GL_MIN GL_MIN_EXT
#endif
#if FO_IOS
#define glTexImage2DMultisample(a, b, c, d, e, f)
#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleAPPLE
#define glRenderbufferStorageMultisampleEXT glRenderbufferStorageMultisampleAPPLE
#elif FO_ANDROID
#define glTexImage2DMultisample(a, b, c, d, e, f)
#define glRenderbufferStorageMultisample glRenderbufferStorageMultisampleIMG
#define glRenderbufferStorageMultisampleEXT glRenderbufferStorageMultisampleIMG
#elif FO_WEB || (FO_WINDOWS && FO_UWP)
#define glTexImage2DMultisample(a, b, c, d, e, f)
#define glRenderbufferStorageMultisample(a, b, c, d, e)
#define glRenderbufferStorageMultisampleEXT(a, b, c, d, e)
#endif
#endif
#if !FO_DEBUG
#define GL(expr) expr
#else
#define GL(expr) \
    do { \
        expr; \
        if (RenderDebug) { \
            GLenum err__ = glGetError(); \
            RUNTIME_ASSERT_STR(err__ == GL_NO_ERROR, _str(#expr " error {:#X}", err__)); \
        } \
    } while (0)
#endif
#define GL_HAS(extension) (OGL_##extension)
#endif
#endif

enum class RenderType
{
    None,
    Null,
#if FO_HAVE_OPENGL
    OpenGL,
#endif
#if FO_HAVE_DIRECT_3D
    Direct3D,
#endif
#if FO_HAVE_METAL
    Metal,
#endif
#if FO_HAVE_VULKAN
    Vulkan,
#endif
#if FO_HAVE_GNM
    Gnm,
#endif
};

#if !FO_HEADLESS
static SDL_Window* SdlWindow {};
static RenderType CurRenderType {RenderType::None};
static bool RenderDebug {};
static vector<InputEvent> EventsQueue {};
static vector<InputEvent> NextFrameEventsQueue {};
static SDL_AudioDeviceID AudioDeviceId {};
static SDL_AudioSpec AudioSpec {};
static Application::AppAudio::AudioStreamCallback AudioStreamWriter {};
#if FO_HAVE_OPENGL
static SDL_GLContext GlContext {};
static GLint BaseFBO {};
static uint GlSamples {};
// static OpenGLRenderBuffer;
static bool OGL_version_2_0 {};
static bool OGL_vertex_buffer_object {};
static bool OGL_framebuffer_object {};
static bool OGL_framebuffer_object_ext {};
static bool OGL_framebuffer_multisample {};
static bool OGL_texture_multisample {};
static bool OGL_vertex_array_object {};
#endif
#if FO_HAVE_DIRECT_3D
static ID3D11Device* D3DDevice {};
static ID3D11DeviceContext* D3DDeviceContext {};
static IDXGISwapChain* SwapChain {};
static ID3D11RenderTargetView* MainRenderTarget {};
static UINT D3DSamplesCount {};
static UINT D3DSamplesQuality {};
static bool VSync {};
#endif
const uint Application::AppRender::MAX_ATLAS_WIDTH {};
const uint Application::AppRender::MAX_ATLAS_HEIGHT {};
const uint Application::AppRender::MAX_BONES {};
const int Application::AppAudio::AUDIO_FORMAT_U8 {AUDIO_U8};
const int Application::AppAudio::AUDIO_FORMAT_S16 {AUDIO_S16};
#else
const uint Application::AppRender::MAX_ATLAS_WIDTH {};
const uint Application::AppRender::MAX_ATLAS_HEIGHT {};
const uint Application::AppRender::MAX_BONES {};
const int Application::AppAudio::AUDIO_FORMAT_U8 {};
const int Application::AppAudio::AUDIO_FORMAT_S16 {};
#endif // !FO_HEADLESS

struct RenderTexture::Impl
{
    virtual ~Impl() = default;
};

RenderTexture::~RenderTexture()
{
}

struct RenderEffect::Impl
{
    virtual ~Impl() = default;
};

RenderEffect::~RenderEffect()
{
}

auto RenderEffect::IsSame(const string& name, const string& defines) const -> bool
{
    return _str(name).compareIgnoreCase(_effectName) && defines == _effectDefines;
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    if (_nameHash != other->_nameHash) {
        return false;
    }
    if (MainTex != other->MainTex) {
        return false;
    }
    if (BorderBuf || ModelBuf || AnimBuf || RandomValueBuf) {
        return false;
    }
    if (CustomTexBuf && std::mismatch(std::begin(CustomTex), std::end(CustomTex), std::begin(other->CustomTex)).first != std::end(CustomTex)) {
        return false;
    }
    return true;
}

struct RenderMesh::Impl
{
    virtual ~Impl() = default;
};

RenderMesh::~RenderMesh()
{
}

#if !FO_HEADLESS
static unordered_map<SDL_Keycode, KeyCode> KeysMap {
#define KEY_CODE(name, index, code) {code, KeyCode::name},
#include "KeyCodes-Include.h"
};

static unordered_map<int, MouseButton> MouseButtonsMap {
    {SDL_BUTTON_LEFT, MouseButton::Left},
    {SDL_BUTTON_RIGHT, MouseButton::Right},
    {SDL_BUTTON_MIDDLE, MouseButton::Middle},
    {SDL_BUTTON(4), MouseButton::Ext0},
    {SDL_BUTTON(5), MouseButton::Ext1},
    {SDL_BUTTON(6), MouseButton::Ext2},
    {SDL_BUTTON(7), MouseButton::Ext3},
    {SDL_BUTTON(8), MouseButton::Ext4},
};

#if FO_HAVE_OPENGL
struct OpenGLTexture : RenderTexture::Impl
{
    ~OpenGLTexture() override
    {
        if (DepthBuffer != 0u) {
            GL(glDeleteRenderbuffers(1, &DepthBuffer));
        }
        GL(glDeleteTextures(1, &TexId));
        GL(glDeleteFramebuffers(1, &FBO));
    }

    GLuint FBO {};
    GLuint TexId {};
    GLuint DepthBuffer {};
};
#endif

#if FO_HAVE_DIRECT_3D
struct Direct3DTexture : RenderTexture::Impl
{
    ~Direct3DTexture() override
    {
        TexHandle->Release();
        if (DepthStencilHandle != nullptr) {
            DepthStencilHandle->Release();
        }
    }

    ID3D11Texture2D* TexHandle {};
    ID3D11Texture2D* DepthStencilHandle {};
};
#endif

#if FO_HAVE_OPENGL
struct OpenGLRenderBuffer
{
    struct VertexArray
    {
        GLuint VAO {};
        GLuint VBO {};
        GLuint IBO {};
        uint VCount {};
        uint ICount {};
    };

    array<VertexArray, RING_BUFFER_LENGTH> VertexArray {};
    size_t VertexArrayIndex {};
};
#endif

#if FO_HAVE_OPENGL
struct OpenGLEffect : RenderEffect::Impl
{
    ~OpenGLEffect() override { }
};
#endif

#if FO_HAVE_DIRECT_3D
struct Direct3DEffect : RenderEffect::Impl
{
    ~Direct3DEffect() override { }
};
#endif

#if FO_HAVE_OPENGL
struct OpenGLMesh : RenderMesh::Impl
{
    ~OpenGLMesh() override
    {
        if (VBO != 0u) {
            GL(glDeleteBuffers(1, &VBO));
        }
        if (IBO != 0u) {
            GL(glDeleteBuffers(1, &IBO));
        }
        if (VAO != 0u) {
            GL(glDeleteVertexArrays(1, &VAO));
        }
    }

    GLuint VAO {};
    GLuint VBO {};
    GLuint IBO {};
};
#endif

#if FO_HAVE_DIRECT_3D
struct Direct3DMesh : RenderMesh::Impl
{
    ~Direct3DMesh() override { }
};
#endif
#endif // !FO_HEADLESS

void InitApplication(GlobalSettings& settings)
{
    // Ensure that we call init only once
    static std::once_flag once;
    auto first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });
    if (!first_call) {
        throw AppInitException("Application::Init must be called only once");
    }

    App = new Application(settings);
}

Application::Application(GlobalSettings& settings)
{
#if !FO_HEADLESS
    // Initialize input events
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        throw AppInitException("SDL_InitSubSystem SDL_INIT_EVENTS failed", SDL_GetError());
    }

    // Initialize audio
    if (!settings.DisableAudio) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            SDL_AudioSpec desired;
            std::memset(&desired, 0, sizeof(desired));
#if FO_WEB
            desired.format = AUDIO_F32;
            desired.freq = 48000;
            desired.channels = 2;
#else
            desired.format = AUDIO_S16;
            desired.freq = 44100;
#endif
            desired.callback = [](void*, Uint8* stream, int) {
                std::memset(stream, AudioSpec.silence, AudioSpec.size);
                if (AudioStreamWriter) {
                    AudioStreamWriter(stream);
                }
            };

            AudioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &desired, &AudioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
            if (AudioDeviceId >= 2) {
                SDL_PauseAudioDevice(AudioDeviceId, 0);
            }
            else {
                WriteLog("SDL open audio device failed, error '{}'. Disable sound.\n", SDL_GetError());
            }
        }
        else {
            WriteLog("SDL init audio subsystem failed, error '{}'. Disable sound.\n", SDL_GetError());
        }
    }

    // First choose render type by user preference
#if FO_HAVE_OPENGL
    if (settings.ForceOpenGL) {
        CurRenderType = RenderType::OpenGL;
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (settings.ForceDirect3D) {
        CurRenderType = RenderType::Direct3D;
    }
#endif
#if FO_HAVE_METAL
    if (settings.ForceMetal) {
        CurRenderType = RenderType::Metal;
    }
#endif
#if FO_HAVE_VULKAN
    if (settings.ForceVulkan) {
        CurRenderType = RenderType::Vulkan;
    }
#endif
#if FO_HAVE_GNM
    if (settings.ForceGnm) {
        CurRenderType = RenderType::Gnm;
    }
#endif

    // If none of selected then evaluate automatic selection
    if (CurRenderType == RenderType::None) {
#if FO_HAVE_OPENGL
        CurRenderType = RenderType::OpenGL;
#endif
#if FO_HAVE_DIRECT_3D
        CurRenderType = RenderType::Direct3D;
#endif
#if FO_HAVE_METAL
        CurRenderType = RenderType::Metal;
#endif
#if FO_HAVE_VULKAN
        CurRenderType = RenderType::Vulkan;
#endif
#if FO_HAVE_GNM
        CurRenderType = RenderType::Gnm;
#endif

        RUNTIME_ASSERT(CurRenderType != RenderType::None);
    }

    // Initialize video system
    if (CurRenderType == RenderType::Null) {
        return;
    }

    RenderDebug = settings.RenderDebug;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        throw AppInitException("SDL_InitSubSystem SDL_INIT_VIDEO failed", SDL_GetError());
    }

#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        // Initialize GLEW
        // Todo: remove GLEW and bind OpenGL functions manually
#if !FO_OPENGL_ES
        auto glew_result = glewInit();
        RUNTIME_ASSERT_STR(glew_result == GLEW_OK, _str("GLEW not initialized, result {}", glew_result));
        OGL_version_2_0 = GLEW_VERSION_2_0 != 0;
        OGL_vertex_buffer_object = GLEW_ARB_vertex_buffer_object != 0;
        OGL_framebuffer_object = GLEW_ARB_framebuffer_object != 0;
        OGL_framebuffer_object_ext = GLEW_EXT_framebuffer_object != 0;
        OGL_framebuffer_multisample = GLEW_EXT_framebuffer_multisample != 0;
        OGL_texture_multisample = GLEW_ARB_texture_multisample != 0;
#if FO_MAC
        OGL_vertex_array_object = GLEW_APPLE_vertex_array_object != 0;
#else
        OGL_vertex_array_object = GLEW_ARB_vertex_array_object != 0;
#endif
#endif

        // OpenGL ES extensions
#if FO_OPENGL_ES
        OGL_version_2_0 = true;
        OGL_vertex_buffer_object = true;
        OGL_framebuffer_object = true;
        OGL_framebuffer_object_ext = false;
        OGL_framebuffer_multisample = false;
        OGL_texture_multisample = false;
        OGL_vertex_array_object = false;
#if FO_ANDROID
        OGL_vertex_array_object = SDL_GL_ExtensionSupported("GL_OES_vertex_array_object");
        OGL_framebuffer_multisample = SDL_GL_ExtensionSupported("GL_IMG_multisampled_render_to_texture");
#endif
#if FO_IOS
        OGL_vertex_array_object = true;
        OGL_framebuffer_multisample = true;
#endif
#endif

        // Check OpenGL extensions
#define CHECK_EXTENSION(ext, critical) \
    if (!GL_HAS(ext)) { \
        string msg = ((critical) ? "Critical" : "Not critical"); \
        WriteLog("OpenGL extension '" #ext "' not supported. {}.\n", msg); \
        if (critical) \
            extension_errors++; \
    }
        uint extension_errors = 0;
        CHECK_EXTENSION(version_2_0, true)
        CHECK_EXTENSION(vertex_buffer_object, true)
        CHECK_EXTENSION(vertex_array_object, false)
        CHECK_EXTENSION(framebuffer_object, false)
        if (!GL_HAS(framebuffer_object)) {
            CHECK_EXTENSION(framebuffer_object_ext, true)
            CHECK_EXTENSION(framebuffer_multisample, false)
        }
        CHECK_EXTENSION(texture_multisample, false)
        RUNTIME_ASSERT(!extension_errors);
#undef CHECK_EXTENSION
    }

    // Map framebuffer_object_ext to framebuffer_object
    if (GL_HAS(framebuffer_object_ext) && !GL_HAS(framebuffer_object)) {
        OGL_framebuffer_object = true;
        // glGenFramebuffers = glGenFramebuffersEXT;
        // glBindFramebuffer = glBindFramebufferEXT;
        // glFramebufferTexture2D = glFramebufferTexture2DEXT;
        // Todo: map all framebuffer ext functions
    }
#endif

    // Determine main window size
    auto is_tablet = false;
#if FO_IOS || FO_ANDROID
    is_tablet = true;
#endif
#if FO_WINDOWS && !FO_UWP
    is_tablet = (::GetSystemMetrics(SM_TABLETPC) != 0); // Todo: recognize tablet mode for Windows 10
#endif
    if (is_tablet) {
        SDL_DisplayMode mode;
        auto r = SDL_GetCurrentDisplayMode(0, &mode);
        if (r != 0) {
            throw AppInitException("SDL_GetCurrentDisplayMode failed", SDL_GetError());
        }

        settings.ScreenWidth = std::max(mode.w, mode.h);
        settings.ScreenHeight = std::min(mode.w, mode.h);

        auto ratio = static_cast<float>(settings.ScreenWidth) / static_cast<float>(settings.ScreenHeight);
        settings.ScreenHeight = 768;
        settings.ScreenWidth = static_cast<int>(static_cast<float>(settings.ScreenHeight) * ratio + 0.5f);

        settings.FullScreen = true;
    }

#if FO_WEB
    // Adaptive size
    int window_w = EM_ASM_INT(return window.innerWidth || document.documentElement.clientWidth || document.getElementsByTagName('body')[0].clientWidth);
    int window_h = EM_ASM_INT(return window.innerHeight || document.documentElement.clientHeight || document.getElementsByTagName('body')[0].clientHeight);
    settings.ScreenWidth = std::clamp(window_w - 200, 1024, 1920);
    settings.ScreenHeight = std::clamp(window_h - 100, 768, 1080);

    // Fixed size
    int fixed_w = EM_ASM_INT(return 'foScreenWidth' in Module ? Module.foScreenWidth : 0);
    int fixed_h = EM_ASM_INT(return 'foScreenHeight' in Module ? Module.foScreenHeight : 0);
    if (fixed_w) {
        settings.ScreenWidth = fixed_w;
    }
    if (fixed_h) {
        settings.ScreenHeight = fixed_h;
    }
#endif

    // Initialize window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

#if FO_HAVE_OPENGL && FO_OPENGL_ES
    if (CurRenderType == RenderType::OpenGL) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }
#endif

    uint window_create_flags = SDL_WINDOW_SHOWN;
    if (settings.FullScreen) {
        window_create_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    if (is_tablet) {
        window_create_flags |= SDL_WINDOW_FULLSCREEN;
        window_create_flags |= SDL_WINDOW_BORDERLESS;
    }

#if FO_HAVE_OPENGL && !FO_WEB
    if (CurRenderType == RenderType::OpenGL) {
        window_create_flags |= SDL_WINDOW_OPENGL;
    }
#endif

    int win_pos = SDL_WINDOWPOS_UNDEFINED;
    if (settings.WindowCentered) {
        win_pos = SDL_WINDOWPOS_CENTERED;
    }

    SdlWindow = SDL_CreateWindow(settings.WindowName.c_str(), win_pos, win_pos, settings.ScreenWidth, settings.ScreenHeight, window_create_flags);
    RUNTIME_ASSERT_STR(SdlWindow, _str("SDL Window not created, error '{}'", SDL_GetError()));

#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
#if !FO_WEB
        GlContext = SDL_GL_CreateContext(SdlWindow);
        RUNTIME_ASSERT_STR(GlContext, _str("OpenGL context not created, error '{}'", SDL_GetError()));

        auto make_current = SDL_GL_MakeCurrent(SdlWindow, GlContext);
        RUNTIME_ASSERT_STR(make_current >= 0, _str("Can't set current context, error '{}'", SDL_GetError()));

        SDL_GL_SetSwapInterval(settings.VSync ? 1 : 0);

#else
        EmscriptenWebGLContextAttributes attr;
        emscripten_webgl_init_context_attributes(&attr);
        attr.alpha = EM_FALSE;
        attr.depth = EM_FALSE;
        attr.stencil = EM_FALSE;
        attr.antialias = EM_TRUE;
        attr.premultipliedAlpha = EM_TRUE;
        attr.preserveDrawingBuffer = EM_FALSE;
        attr.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
        attr.failIfMajorPerformanceCaveat = EM_FALSE;
        attr.enableExtensionsByDefault = EM_TRUE;
        attr.explicitSwapControl = EM_FALSE;
        attr.renderViaOffscreenBackBuffer = EM_FALSE;

        attr.majorVersion = 2;
        attr.minorVersion = 0;
        EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl_context = emscripten_webgl_create_context(nullptr, &attr);
        if (gl_context <= 0) {
            attr.majorVersion = 1;
            attr.minorVersion = 0;
            gl_context = emscripten_webgl_create_context(nullptr, &attr);
            RUNTIME_ASSERT_STR(gl_context > 0, _str("Failed to create WebGL context, error '{}'", (int)gl_context));
            WriteLog("Created WebGL1 context.\n");
        }
        else {
            WriteLog("Created WebGL2 context.\n");
        }

        EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(gl_context);
        RUNTIME_ASSERT_STR(r >= 0, _str("Can't set current context, error '{}'", r));

        OGL_vertex_array_object = (attr.majorVersion > 1 || emscripten_webgl_enable_extension(gl_context, "OES_vertex_array_object"));

        GlContext = (SDL_GLContext)gl_context;
#endif

        // Render states
        GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GL(glDisable(GL_DEPTH_TEST));
        GL(glEnable(GL_BLEND));
        GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        GL(glDisable(GL_CULL_FACE));
        GL(glEnable(GL_TEXTURE_2D));
        GL(glActiveTexture(GL_TEXTURE0));
        GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
        GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
#if !FO_OPENGL_ES
        GL(glDisable(GL_LIGHTING));
        GL(glDisable(GL_COLOR_MATERIAL));
        GL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
#endif

        GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &BaseFBO));

        // Calculate atlas size
        GLint max_texture_size = 0;
        GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size));
        GLint max_viewport_size[2];
        GL(glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size));
        auto atlas_w = std::min(max_texture_size, static_cast<GLint>(AppRender::MAX_ATLAS_SIZE));
        auto atlas_h = atlas_w;
        atlas_w = std::min(max_viewport_size[0], atlas_w);
        atlas_h = std::min(max_viewport_size[1], atlas_h);
        RUNTIME_ASSERT_STR(atlas_w >= AppRender::MIN_ATLAS_SIZE, _str("Min texture width must be at least {}", AppRender::MIN_ATLAS_SIZE));
        RUNTIME_ASSERT_STR(atlas_h >= AppRender::MIN_ATLAS_SIZE, _str("Min texture height must be at least {}", AppRender::MIN_ATLAS_SIZE));
        const_cast<uint&>(AppRender::MAX_ATLAS_WIDTH) = atlas_w;
        const_cast<uint&>(AppRender::MAX_ATLAS_HEIGHT) = atlas_h;

        // Check max bones
        if (settings.Enable3dRendering) {
#if !FO_OPENGL_ES
            GLint max_uniform_components = 0;
            GL(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_uniform_components));
            if (max_uniform_components < 1024) {
                WriteLog("Warning! GL_MAX_VERTEX_UNIFORM_COMPONENTS is {}.\n", max_uniform_components);
            }
#endif
        }

        // Calculate multisampling samples
        if (GL_HAS(texture_multisample) && settings.MultiSampling != 0 && (GL_HAS(framebuffer_object) || (GL_HAS(framebuffer_object_ext) && GL_HAS(framebuffer_multisample)))) {
            auto max_samples = 0;
            GL(glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &max_samples));
            GlSamples = std::min(settings.MultiSampling > 0 ? settings.MultiSampling : 8, max_samples);

            /*effectMngr.Effects.FlushRenderTargetMSDefault =
                effectMngr.LoadEffect("Flush_RenderTargetMS.glsl", true, "", "Effects/");
            if (effectMngr.Effects.FlushRenderTargetMSDefault)
                effectMngr.Effects.FlushRenderTargetMS = effectMngr.Effects.FlushRenderTargetMSDefault;
            else
                GlSamples = 0;*/
        }
        if (GlSamples <= 1) {
            GlSamples = 0;
        }
    }
#endif

#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
        SDL_SysWMinfo wminfo;
        SDL_VERSION(&wminfo.version)
        SDL_GetWindowWMInfo(SdlWindow, &wminfo);
#if !FO_UWP
        auto* hwnd = wminfo.info.win.window;
#else
        HWND hwnd = 0;
#endif

        DXGI_SWAP_CHAIN_DESC sd;
        std::memset(&sd, 0, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT device_flags = 0;
        if (RenderDebug) {
            device_flags |= D3D11_CREATE_DEVICE_DEBUG;
        }

        D3D_FEATURE_LEVEL feature_level;
        const D3D_FEATURE_LEVEL feature_levels[7] = {
            D3D_FEATURE_LEVEL_11_1,
#if !FO_UWP
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1,
#endif
        };

#if !FO_UWP
        const auto d3d_create_device = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, feature_levels, 7, D3D11_SDK_VERSION, &sd, &SwapChain, &D3DDevice, &feature_level, &D3DDeviceContext);
        if (d3d_create_device != S_OK) {
            throw AppInitException("D3D11CreateDeviceAndSwapChain failed", d3d_create_device);
        }
#else
        const auto d3d_create_device = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, feature_levels, 1, D3D11_SDK_VERSION, &D3DDevice, &feature_level, &D3DDeviceContext);
        if (d3d_create_device != S_OK)
            throw AppInitException("D3D11CreateDevice failed", d3d_create_device);
#endif

        static const map<D3D_FEATURE_LEVEL, string> FEATURE_LEVELS_STR = {
            {D3D_FEATURE_LEVEL_11_1, "11.1"},
            {D3D_FEATURE_LEVEL_11_0, "11.0"},
            {D3D_FEATURE_LEVEL_10_1, "10.1"},
            {D3D_FEATURE_LEVEL_10_0, "10.0"},
            {D3D_FEATURE_LEVEL_9_3, "9.3"},
            {D3D_FEATURE_LEVEL_9_2, "9.2"},
            {D3D_FEATURE_LEVEL_9_1, "9.1"},
        };
        WriteLog("Direct3D device created with feature level {}.\n", FEATURE_LEVELS_STR.at(feature_level));

        if (settings.MultiSampling > 0) {
            D3DSamplesCount = settings.MultiSampling;
            auto check_multisample = D3DDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, D3DSamplesCount, &D3DSamplesQuality);
            RUNTIME_ASSERT(check_multisample == S_OK);
        }
        if (settings.MultiSampling < 0 || (D3DSamplesQuality == 0u)) {
            D3DSamplesCount = 4;
            auto check_multisample = D3DDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, D3DSamplesCount, &D3DSamplesQuality);
            RUNTIME_ASSERT(check_multisample == S_OK);
        }
        if (D3DSamplesQuality == 0u) {
            D3DSamplesCount = 1;
            D3DSamplesQuality = 0;
        }

        ID3D11Texture2D* back_buf = nullptr;
        SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buf));
        RUNTIME_ASSERT(back_buf);
        D3DDevice->CreateRenderTargetView(back_buf, NULL, &MainRenderTarget);
        back_buf->Release();
    }
#endif

    // SDL_ShowCursor(0);

    if (settings.AlwaysOnTop) {
        Window.AlwaysOnTop(true);
    }
#endif // !FO_HEADLESS

    // Skip SDL allocations from profiling
#if FO_WINDOWS && FO_DEBUG
    ::_CrtMemCheckpoint(&CrtMemState);
#endif
}

void Application::BeginFrame()
{
#if !FO_HEADLESS
    if (!NextFrameEventsQueue.empty()) {
        EventsQueue.insert(EventsQueue.end(), NextFrameEventsQueue.begin(), NextFrameEventsQueue.end());
        NextFrameEventsQueue.clear();
    }

    SDL_PumpEvents();

    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event) != 0) {
        switch (sdl_event.type) {
        case SDL_MOUSEMOTION: {
            InputEvent::MouseMoveEvent ev;
            ev.MouseX = sdl_event.motion.x;
            ev.MouseY = sdl_event.motion.y;
            ev.DeltaX = sdl_event.motion.xrel;
            ev.DeltaY = sdl_event.motion.yrel;
            EventsQueue.emplace_back(ev);
        } break;
        case SDL_MOUSEBUTTONDOWN: {
            InputEvent::MouseDownEvent ev;
            ev.Button = MouseButtonsMap[sdl_event.button.button];
            EventsQueue.emplace_back(ev);
        } break;
        case SDL_MOUSEBUTTONUP: {
            InputEvent::MouseUpEvent ev;
            ev.Button = MouseButtonsMap[sdl_event.button.button];
            EventsQueue.emplace_back(ev);
        } break;
        case SDL_FINGERMOTION: {
            InputEvent::MouseMoveEvent ev;
            ev.MouseX = sdl_event.motion.x;
            ev.MouseY = sdl_event.motion.y;
            EventsQueue.emplace_back(ev);
        } break;
        case SDL_FINGERDOWN: {
            InputEvent::MouseMoveEvent ev1;
            SDL_GetMouseState(&ev1.MouseX, &ev1.MouseY);
            EventsQueue.emplace_back(ev1);
            InputEvent::MouseDownEvent ev2;
            ev2.Button = MouseButton::Left;
            EventsQueue.emplace_back(ev2);
        } break;
        case SDL_FINGERUP: {
            InputEvent::MouseUpEvent ev;
            ev.Button = MouseButton::Left;
            EventsQueue.emplace_back(ev);
        } break;
        case SDL_MOUSEWHEEL: {
            InputEvent::MouseWheelEvent ev;
            ev.Delta = -sdl_event.wheel.y;
            EventsQueue.emplace_back(ev);
        } break;
        case SDL_KEYDOWN: {
            InputEvent::KeyDownEvent ev;
            ev.Code = KeysMap[sdl_event.key.keysym.scancode];
            ev.Text = sdl_event.text.text;
            EventsQueue.emplace_back(ev);
        } break;
        case SDL_KEYUP: {
            InputEvent::KeyUpEvent ev;
            ev.Code = KeysMap[sdl_event.key.keysym.scancode];
            EventsQueue.emplace_back(ev);
        } break;
        case SDL_TEXTINPUT: {
            InputEvent::KeyDownEvent ev1;
            ev1.Code = KeyCode::DIK_TEXT;
            ev1.Text = sdl_event.text.text;
            EventsQueue.emplace_back(ev1);
            InputEvent::KeyUpEvent ev2;
            ev2.Code = KeyCode::DIK_TEXT;
            EventsQueue.emplace_back(ev2);
        } break;
        case SDL_DROPTEXT: {
            InputEvent::KeyDownEvent ev1;
            ev1.Code = KeyCode::DIK_TEXT;
            ev1.Text = sdl_event.drop.file;
            EventsQueue.emplace_back(ev1);
            InputEvent::KeyUpEvent ev2;
            ev2.Code = KeyCode::DIK_TEXT;
            EventsQueue.emplace_back(ev2);
            SDL_free(sdl_event.drop.file);
        } break;
        case SDL_DROPFILE: {
            if (auto file = DiskFileSystem::OpenFile(sdl_event.drop.file, false)) {
                auto stripped = false;
                auto size = file.GetSize();
                if (size > AppInput::DROP_FILE_STRIP_LENGHT) {
                    stripped = true;
                    size = AppInput::DROP_FILE_STRIP_LENGHT;
                }

                char buf[AppInput::DROP_FILE_STRIP_LENGHT + 1];
                if (file.Read(buf, size)) {
                    buf[size] = 0;
                    InputEvent::KeyDownEvent ev1;
                    ev1.Code = KeyCode::DIK_TEXT;
                    ev1.Text = _str("{}\n{}{}", sdl_event.drop.file, buf, stripped ? "..." : "");
                    EventsQueue.emplace_back(ev1);
                    InputEvent::KeyUpEvent ev2;
                    ev2.Code = KeyCode::DIK_TEXT;
                    EventsQueue.emplace_back(ev2);
                }
            }
            SDL_free(sdl_event.drop.file);
        } break;
        case SDL_APP_DIDENTERFOREGROUND: {
            _onPauseDispatcher();
        } break;
        case SDL_APP_DIDENTERBACKGROUND: {
            _onResumeDispatcher();
        } break;
        case SDL_APP_LOWMEMORY: {
            _onLowMemoryDispatcher();
        } break;
        case SDL_QUIT:
            [[fallthrough]];
        case SDL_APP_TERMINATING: {
            _onQuitDispatcher();

            DeleteGlobalData();

#if FO_WINDOWS && FO_DEBUG
            ::_CrtMemDumpAllObjectsSince(&CrtMemState);
#endif

            std::exit(0);
        }
        default:
            break;
        }
    }
#endif // !FO_HEADLESS

    _onFrameBeginDispatcher();
}

void Application::EndFrame()
{
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
#if FO_HAVE_OPENGL
        if (CurRenderType == RenderType::OpenGL) {
#if !FO_WEB
            SDL_GL_SwapWindow(SdlWindow);
#endif
            if (RenderDebug && glGetError() != GL_NO_ERROR) {
                throw GenericException("Unknown place of OpenGL error");
            }

            GL(glClearColor(0, 0, 0, 1));
            GL(glClear(GL_COLOR_BUFFER_BIT));
        }
#endif
#if FO_HAVE_DIRECT_3D
        if (CurRenderType == RenderType::Direct3D) {
            const auto swap_chain = SwapChain->Present(VSync ? 1 : 0, 0);
            RUNTIME_ASSERT(swap_chain == S_OK);

            D3DDeviceContext->OMSetRenderTargets(1, &MainRenderTarget, nullptr);
            float color[4] {0, 0, 0, 1};
            D3DDeviceContext->ClearRenderTargetView(MainRenderTarget, color);
        }
#endif
    }
#endif // !FO_HEADLESS

    _onFrameEndDispatcher();
}

auto Application::AppWindow::GetSize() -> tuple<int, int>
{
    auto w = 1000;
    auto h = 1000;
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        SDL_GetWindowSize(SdlWindow, &w, &h);
    }
#endif
    return {w, h};
}

void Application::AppWindow::SetSize(int w, int h)
{
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        SDL_SetWindowSize(SdlWindow, w, h);
    }
#endif
}

auto Application::AppWindow::GetPosition() -> tuple<int, int>
{
    auto x = 0;
    auto y = 0;
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        SDL_GetWindowPosition(SdlWindow, &x, &y);
    }
#endif
    return {x, y};
}

void Application::AppWindow::SetPosition(int x, int y)
{
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        SDL_SetWindowPosition(SdlWindow, x, y);
    }
#endif
}

auto Application::AppWindow::GetMousePosition() -> tuple<int, int>
{
    auto x = 100;
    auto y = 100;
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        SDL_GetMouseState(&x, &y);
    }
#endif
    return {x, y};
}

void Application::AppWindow::SetMousePosition(int x, int y)
{
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        SDL_WarpMouseInWindow(SdlWindow, x, y);
    }
#endif
}

auto Application::AppWindow::IsFocused() -> bool
{
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        return (SDL_GetWindowFlags(SdlWindow) & SDL_WINDOW_INPUT_FOCUS) != 0u;
    }
#endif

    return true;
}

void Application::AppWindow::Minimize()
{
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        SDL_MinimizeWindow(SdlWindow);
    }
#endif
}

auto Application::AppWindow::IsFullscreen() -> bool
{
#if !FO_HEADLESS
    if (CurRenderType != RenderType::Null) {
        return (SDL_GetWindowFlags(SdlWindow) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0u;
    }
#endif

    return false;
}

auto Application::AppWindow::ToggleFullscreen(bool enable) -> bool
{
#if !FO_HEADLESS
    if (CurRenderType == RenderType::Null) {
        return false;
    }

    const auto is_fullscreen = IsFullscreen();
    if (!is_fullscreen && enable) {
        if (SDL_SetWindowFullscreen(SdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
            return true;
        }
    }
    else if (is_fullscreen && !enable) {
        if (SDL_SetWindowFullscreen(SdlWindow, 0) == 0) {
            return true;
        }
    }
#endif

    return false;
}

void Application::AppWindow::Blink()
{
#if !FO_HEADLESS
    if (CurRenderType == RenderType::Null) {
        return;
    }

#if FO_WINDOWS && !FO_UWP
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version)
    if (SDL_GetWindowWMInfo(SdlWindow, &info) != 0) {
        ::FlashWindow(info.info.win.window, 1);
    }
#endif
#endif
}

void Application::AppWindow::AlwaysOnTop(bool enable)
{
#if !FO_HEADLESS
    if (CurRenderType == RenderType::Null) {
        return;
    }

#if FO_WINDOWS && !FO_UWP
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version)
    if (SDL_GetWindowWMInfo(SdlWindow, &info) != 0) {
        ::SetWindowPos(info.info.win.window, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }
#endif
#endif
}

auto Application::AppRender::CreateTexture(uint width, uint height, bool linear_filtered, bool multisampled, bool with_depth) -> RenderTexture*
{
#if !FO_HEADLESS
    auto tex = unique_ptr<RenderTexture>(new RenderTexture());
    const_cast<uint&>(tex->Width) = width;
    const_cast<uint&>(tex->Height) = height;
    const_cast<float&>(tex->SizeData[0]) = static_cast<float>(width);
    const_cast<float&>(tex->SizeData[0]) = static_cast<float>(height);
    const_cast<float&>(tex->SizeData[0]) = 1.0f / tex->SizeData[0];
    const_cast<float&>(tex->SizeData[0]) = 1.0f / tex->SizeData[1];
#if FO_HAVE_OPENGL
    const_cast<float&>(tex->Samples) = (multisampled && (GlSamples != 0u) ? static_cast<float>(GlSamples) : 1.0f);
    const_cast<bool&>(tex->LinearFiltered) = linear_filtered;
    const_cast<bool&>(tex->Multisampled) = (multisampled && (GlSamples != 0u));
    const_cast<bool&>(tex->WithDepth) = with_depth;
#endif

#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        auto opengl_tex = std::make_unique<OpenGLTexture>();

        GL(glGenFramebuffers(1, &opengl_tex->FBO));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FBO));
        GL(glGenTextures(1, &opengl_tex->TexId));

        if (multisampled && (GlSamples != 0u)) {
            GL(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, opengl_tex->TexId));
            GL(glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, GlSamples, GL_RGBA, width, height, GL_TRUE));
            GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, opengl_tex->TexId, 0));
        }
        else {
            GL(glBindTexture(GL_TEXTURE_2D, opengl_tex->TexId));
            GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filtered ? GL_LINEAR : GL_NEAREST));
            GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filtered ? GL_LINEAR : GL_NEAREST));
            GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP));
            GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP));
            GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
            GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opengl_tex->TexId, 0));
        }
        if (with_depth) {
            GLint cur_rb = 0;
            GL(glGetIntegerv(GL_RENDERBUFFER_BINDING, &cur_rb));
            GL(glGenRenderbuffers(1, &opengl_tex->DepthBuffer));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, opengl_tex->DepthBuffer));
            if (multisampled && (GlSamples != 0u)) {
                GL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, GlSamples, GL_DEPTH_COMPONENT16, width, height));
            }
            else {
                GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height));
            }
            GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, opengl_tex->DepthBuffer));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, cur_rb));
        }

        GLenum status = 0;
        GL(status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
        RUNTIME_ASSERT_STR(status == GL_FRAMEBUFFER_COMPLETE, _str("Framebuffer not created, status {:#X}", status));

        tex->_pImpl = std::move(opengl_tex);
        return tex.release();
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
        auto d3d_tex = std::make_unique<Direct3DTexture>();

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = (multisampled ? D3DSamplesCount : 1);
        desc.SampleDesc.Quality = (multisampled ? D3DSamplesQuality : 0);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        const auto create_texure_2d = D3DDevice->CreateTexture2D(&desc, nullptr, &d3d_tex->TexHandle);
        RUNTIME_ASSERT(create_texure_2d == S_OK);

        if (with_depth) {
            desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
            desc.CPUAccessFlags = 0;
            const auto create_texure_depth = D3DDevice->CreateTexture2D(&desc, nullptr, &d3d_tex->DepthStencilHandle);
            RUNTIME_ASSERT(create_texure_depth == S_OK);
        }

        tex->_pImpl = std::move(d3d_tex);
        return tex.release();
    }
#endif
#endif

    return nullptr;
}

auto Application::AppRender::GetTexturePixel(RenderTexture* tex, int x, int y) -> uint
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        auto* opengl_tex = dynamic_cast<OpenGLTexture*>(tex->_pImpl.get());
        auto prev_fbo = 0;
        GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FBO));
        uint result = 0;
        GL(glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &result));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo));
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif

    return 0;
}

auto Application::AppRender::GetTextureRegion(RenderTexture* tex, int x, int y, uint w, uint h) -> vector<uint>
{
    RUNTIME_ASSERT(w && h);
    vector<uint> result(w * h);
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        const auto* opengl_tex = dynamic_cast<OpenGLTexture*>(tex->_pImpl.get());
        GLint prev_fbo = 0;
        GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FBO));
        GL(glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, &result[0]));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo));
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
    return std::move(result);
}

void Application::AppRender::UpdateTextureRegion(RenderTexture* tex, const IRect& r, const uint* data)
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        const auto* opengl_tex = dynamic_cast<OpenGLTexture*>(tex->_pImpl.get());
        GL(glBindTexture(GL_TEXTURE_2D, opengl_tex->TexId));
        GL(glTexSubImage2D(GL_TEXTURE_2D, 0, r.Left, r.Top, r.Width(), r.Height(), GL_RGBA, GL_UNSIGNED_BYTE, data));
        GL(glBindTexture(GL_TEXTURE_2D, 0));
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
        const auto* d3d_tex = dynamic_cast<Direct3DTexture*>(tex->_pImpl.get());
        // D3DDeviceContext->UpdateSubresource()
    }
#endif
#endif
}

void Application::AppRender::SetRenderTarget(RenderTexture* tex)
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        const auto* opengl_tex = dynamic_cast<OpenGLTexture*>(tex->_pImpl.get());
        GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FBO));
        /*    int w, h;
    bool screen_size;
    if (!rtStack.empty())
    {
        RenderTarget* rt = rtStack.back();
        w = rt->TargetTexture->Width;
        h = rt->TargetTexture->Height;
        screen_size = (rt->ScreenSize && !rt->Width && !rt->Height);
    }
    else
    {
        GetWindowSize(w, h);
        screen_size = true;
    }

    if (screen_size && settings.FullScreen)
    {
        // Preserve aspect ratio in fullscreen mode
        float native_aspect = (float)w / h;
        float aspect = (float)settings.ScreenWidth / settings.ScreenHeight;
        int new_w = (int)roundf((aspect <= native_aspect ? (float)h * aspect : (float)h * native_aspect));
        int new_h = (int)roundf((aspect <= native_aspect ? (float)w / native_aspect : (float)w / aspect));
        GL(glViewport((w - new_w) / 2, (h - new_h) / 2, new_w, new_h));
    }
    else
    {
        GL(glViewport(0, 0, w, h));
    }

    if (screen_size)
    {
        Application::AppWindow::MatrixOrtho(
            projectionMatrixCM[0], 0.0f, (float)settings.ScreenWidth, (float)settings.ScreenHeight, 0.0f, -1.0f, 1.0f);
    }
    else
    {
        Application::AppWindow::MatrixOrtho(projectionMatrixCM[0], 0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    }
    projectionMatrixCM.Transpose(); // Convert to column major order*/
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
}

auto Application::AppRender::GetRenderTarget() -> RenderTexture*
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
    return nullptr;
}

void Application::AppRender::ClearRenderTarget(uint color)
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        const auto a = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
        const auto r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
        const auto g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
        const auto b = static_cast<float>((color >> 0) & 0xFF) / 255.0f;
        GL(glClearColor(r, g, b, a));
        GL(glClear(GL_COLOR_BUFFER_BIT));
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
}

void Application::AppRender::ClearRenderTargetDepth()
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        GL(glClear(GL_DEPTH_BUFFER_BIT));
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
}

void Application::AppRender::EnableScissor(int x, int y, uint w, uint h)
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        GL(glEnable(GL_SCISSOR_TEST));
        GL(glScissor(x, y, w, h));
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
}

void Application::AppRender::DisableScissor()
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        GL(glDisable(GL_SCISSOR_TEST));
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
}

auto Application::AppRender::CreateEffect(const string& /*name*/, const string& /*defines*/, const RenderEffectLoader & /*file_loader*/) -> RenderEffect*
{
    auto effect = unique_ptr<RenderEffect>(new RenderEffect());
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        /*
#define IS_EFFECT_VALUE(pos) ((pos) != -1)
#define SET_EFFECT_VALUE(eff, pos, value) GL(glUniform1f(pos, value))
         // Parse effect commands
        vector<StrVec> commands;
        while (true)
        {
            string line = file.GetNonEmptyLine();
            if (line.empty())
                break;

            if (_str(line).startsWith("Effect "))
            {
                StrVec tokens = _str(line.substr(_str("Effect ").length())).split(' ');
                if (!tokens.empty())
                    commands.push_back(tokens);
            }
            else if (_str(line).startsWith("#version"))
            {
                break;
            }
        }

        // Find passes count
        bool fail = false;
        uint passes = 1;
        for (size_t i = 0; i < commands.size(); i++)
            if (commands[i].size() >= 2 && commands[i][0] == "Passes")
                passes = GenericUtils::ConvertParamValue(commands[i][1], fail);

        // New effect
        auto effect = std::make_unique<Effect>();
        effect->Name = loaded_fname;
        effect->Defines = defines;
        for (uint pass = 0; pass < passes; pass++)
        {
            EffectPass effect_pass;
            auto& cd = effect_pass.ConstantsDesc;

            size_t offs = offsetof(EffectPass, Constants);
            cd.emplace_back(ShaderConstant::Sampler, "ColorMap", offs + offsetof(EffectPass, ColorMap));
            cd.emplace_back(ShaderConstant::Sampler, "EggMap", offs + offsetof(EffectPass, EggMap));
            cd.emplace_back(ShaderConstant::Float4, "ColorMapSize", offs + offsetof(EffectPass::ConstBuffer,
        ColorMapSize)); cd.emplace_back(ShaderConstant::Float, "ColorMapSamples", offs +
        offsetof(EffectPass::ConstBuffer, ColorMapSamples)); cd.emplace_back(ShaderConstant::Float4, "EggMapSize", offs
        + offsetof(EffectPass::ConstBuffer, EggMapSize)); cd.emplace_back(ShaderConstant::Float4, "SpriteBorder", offs +
        offsetof(EffectPass::ConstBuffer, SpriteBorder)); cd.emplace_back(ShaderConstant::Float, "ZoomFactor", offs +
        offsetof(EffectPass::ConstBuffer, ZoomFactor)); cd.emplace_back(ShaderConstant::Matrix, "ProjMatrix", offs +
        offsetof(EffectPass::ConstBuffer, ProjMatrix)); cd.emplace_back(ShaderConstant::Float4, "LightColor", offs +
        offsetof(EffectPass::ConstBuffer, LightColor)); cd.emplace_back(ShaderConstant::Float, "Time", offs +
        offsetof(EffectPass::ConstBuffer, Time)); cd.emplace_back(ShaderConstant::Float, "GameTime", offs +
        offsetof(EffectPass::ConstBuffer, GameTime)); cd.emplace_back(ShaderConstant::Float, "Random1", offs +
        offsetof(EffectPass::ConstBuffer, Random1)); cd.emplace_back(ShaderConstant::Float, "Random2", offs +
        offsetof(EffectPass::ConstBuffer, Random2)); cd.emplace_back(ShaderConstant::Float, "Random3", offs +
        offsetof(EffectPass::ConstBuffer, Random3)); cd.emplace_back(ShaderConstant::Float, "Random4", offs +
        offsetof(EffectPass::ConstBuffer, Random4)); cd.emplace_back(ShaderConstant::Float, "AnimPosProc", offs +
        offsetof(EffectPass::ConstBuffer, AnimPosProc)); cd.emplace_back(ShaderConstant::Float, "AnimPosTime", offs +
        offsetof(EffectPass::ConstBuffer, AnimPosTime));

            for (uint i = 0; i < EFFECT_SCRIPT_VALUES; i++)
            {
                cd.emplace_back(ShaderConstant::Float, _str("ScriptValue{}", i),
                    offs + offsetof(EffectPass::ConstBuffer, ScriptValue) + i * sizeof(float));
            }

            for (uint i = 0; i < EFFECT_TEXTURES; i++)
            {
                cd.emplace_back(
                    ShaderConstant::Sampler, _str("Texture{}", i), offs + offsetof(EffectPass, TexMaps) + i *
        sizeof(Texture*));

                size_t tex_offs = offsetof(EffectPass, ConstantsTex) + i * sizeof(EffectPass::ConstBufferTex);
                cd.emplace_back(ShaderConstant::Float4, _str("Texture{}Size", i),
                    tex_offs + offsetof(EffectPass::ConstBufferTex, TextureSize));
                cd.emplace_back(ShaderConstant::Float2, _str("Texture{}AtlasOffset", i),
                    tex_offs + offsetof(EffectPass::ConstBufferTex, TextureAtlasOffset));
            }

            effect_pass.ConstantsOffset = offsetof(EffectPass, Constants);

            if (!use_in_2d)
            {
                cd.emplace_back(ShaderConstant::Float4, "GroundPosition",
                    offsetof(EffectPass, Constants3D) + offsetof(EffectPass::ConstBuffer3D, GroundPosition));
                cd.emplace_back(ShaderConstant::MatrixList, "WorldMatrices",
                    offsetof(EffectPass, Constants3D) + offsetof(EffectPass::ConstBuffer3D, WorldMatrices));

                effect_pass.Constants3DOffset = offsetof(EffectPass, Constants3D);
            }

            effect->Passes[pass] = std::move(effect_pass);
            effect->PassCount++;
        }

        if (!LoadEffectPass(effect.get(), fname, file, pass, use_in_2d, defines, defaults, defaults_count))
            return nullptr;

        // Process commands
        for (size_t i = 0; i < commands.size(); i++)
        {
            static auto get_gl_blend_func = [](const string& s) {
                if (s == "GL_ZERO")
                    return GL_ZERO;
                if (s == "GL_ONE")
                    return GL_ONE;
                if (s == "GL_SRC_COLOR")
                    return GL_SRC_COLOR;
                if (s == "GL_ONE_MINUS_SRC_COLOR")
                    return GL_ONE_MINUS_SRC_COLOR;
                if (s == "GL_DST_COLOR")
                    return GL_DST_COLOR;
                if (s == "GL_ONE_MINUS_DST_COLOR")
                    return GL_ONE_MINUS_DST_COLOR;
                if (s == "GL_SRC_ALPHA")
                    return GL_SRC_ALPHA;
                if (s == "GL_ONE_MINUS_SRC_ALPHA")
                    return GL_ONE_MINUS_SRC_ALPHA;
                if (s == "GL_DST_ALPHA")
                    return GL_DST_ALPHA;
                if (s == "GL_ONE_MINUS_DST_ALPHA")
                    return GL_ONE_MINUS_DST_ALPHA;
                if (s == "GL_CONSTANT_COLOR")
                    return GL_CONSTANT_COLOR;
                if (s == "GL_ONE_MINUS_CONSTANT_COLOR")
                    return GL_ONE_MINUS_CONSTANT_COLOR;
                if (s == "GL_CONSTANT_ALPHA")
                    return GL_CONSTANT_ALPHA;
                if (s == "GL_ONE_MINUS_CONSTANT_ALPHA")
                    return GL_ONE_MINUS_CONSTANT_ALPHA;
                if (s == "GL_SRC_ALPHA_SATURATE")
                    return GL_SRC_ALPHA_SATURATE;
                return -1;
            };
            static auto get_gl_blend_equation = [](const string& s) {
                if (s == "GL_FUNC_ADD")
                    return GL_FUNC_ADD;
                if (s == "GL_FUNC_SUBTRACT")
                    return GL_FUNC_SUBTRACT;
                if (s == "GL_FUNC_REVERSE_SUBTRACT")
                    return GL_FUNC_REVERSE_SUBTRACT;
                if (s == "GL_MAX")
                    return GL_MAX;
                if (s == "GL_MIN")
                    return GL_MIN;
                return -1;
            };

            StrVec& tokens = commands[i];
            if (tokens[0] == "Pass" && tokens.size() >= 3)
            {
                uint pass = GenericUtils::ConvertParamValue(tokens[1], fail);
                if (pass < passes)
                {
                    EffectPass& effect_pass = effect->Passes[pass];
                    if (tokens[2] == "BlendFunc" && tokens.size() >= 5)
                    {
                        effect_pass.IsNeedProcess = effect_pass.IsChangeStates = true;
                        effect_pass.BlendFuncParam1 = get_gl_blend_func(tokens[3]);
                        effect_pass.BlendFuncParam2 = get_gl_blend_func(tokens[4]);
                        if (effect_pass.BlendFuncParam1 == -1 || effect_pass.BlendFuncParam2 == -1)
                            fail = true;
                    }
                    else if (tokens[2] == "BlendEquation" && tokens.size() >= 4)
                    {
                        effect_pass.IsNeedProcess = effect_pass.IsChangeStates = true;
                        effect_pass.BlendEquation = get_gl_blend_equation(tokens[3]);
                        if (effect_pass.BlendEquation == -1)
                            fail = true;
                    }
                    else if (tokens[2] == "Shadow")
                    {
                        effect_pass.IsShadow = true;
                    }
                    else
                    {
                        fail = true;
                    }
                }
                else
                {
                    fail = true;
                }
            }
        }
        if (fail)
        {
            WriteLog("Invalid commands in effect '{}'.\n", fname);
            return nullptr;
        }*/
        /*bool EffectManager::LoadEffectPass(Effect* effect, const string& fname, File& file, uint pass, bool use_in_2d,
    const string& defines, EffectDefault* defaults, uint defaults_count)
{
    EffectPass effect_pass;
    memzero(&effect_pass, sizeof(effect_pass));
    GLuint program = 0;

    // Get version
    string file_content = _str(file.GetCStr()).normalizeLineEndings();
    string version;
    size_t ver_begin = file_content.find("#version");
    if (ver_begin != string::npos)
    {
        size_t ver_end = file_content.find('\n', ver_begin);
        if (ver_end != string::npos)
        {
            version = file_content.substr(ver_begin, ver_end - ver_begin);
            file_content = file_content.substr(ver_end + 1);
        }
    }
#if FO_OPENGL_ES
    version = "precision lowp float;\n";
#endif

    // Internal definitions
    string internal_defines = _str("#define PASS{}\n#define MAX_BONES {}\n", pass, MaxBones);

    // Create shaders
    GLuint vs, fs;
    GL(vs = glCreateShader(GL_VERTEX_SHADER));
    GL(fs = glCreateShader(GL_FRAGMENT_SHADER));
    string buf = _str("{}{}{}{}{}{}{}", version, "\n", "#define VERTEX_SHADER\n", internal_defines, defines, "\n",
file_content); const GLchar* vs_str = &buf[0]; GL(glShaderSource(vs, 1, &vs_str, nullptr)); buf = _str("{}{}{}{}{}{}{}",
version, "\n", "#define FRAGMENT_SHADER\n", internal_defines, defines, "\n", file_content); const GLchar* fs_str =
&buf[0]; GL(glShaderSource(fs, 1, &fs_str, nullptr));

    // Info parser
    struct ShaderInfo
    {
        static void Log(const char* shader_name, GLint shader)
        {
            int len = 0;
            GL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len));
            if (len > 0)
            {
                GLchar* str = new GLchar[len];
                int chars = 0;
                glGetShaderInfoLog(shader, len, &chars, str);
                WriteLog("{} output:\n{}", shader_name, str);
                delete[] str;
            }
        }
        static void LogProgram(GLint program)
        {
            int len = 0;
            GL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len));
            if (len > 0)
            {
                GLchar* str = new GLchar[len];
                int chars = 0;
                glGetProgramInfoLog(program, len, &chars, str);
                WriteLog("Program info output:\n{}", str);
                delete[] str;
            }
        }
    };

    // Compile vs
    GLint compiled;
    GL(glCompileShader(vs));
    GL(glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled));
    if (!compiled)
    {
        WriteLog("Vertex shader not compiled, effect '{}'.\n", fname);
        ShaderInfo::Log("Vertex shader", vs);
        GL(glDeleteShader(vs));
        GL(glDeleteShader(fs));
        return false;
    }

    // Compile fs
    GL(glCompileShader(fs));
    GL(glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled));
    if (!compiled)
    {
        WriteLog("Fragment shader not compiled, effect '{}'.\n", fname);
        ShaderInfo::Log("Fragment shader", fs);
        GL(glDeleteShader(vs));
        GL(glDeleteShader(fs));
        return false;
    }

    // Make program
    GL(program = glCreateProgram());
    GL(glAttachShader(program, vs));
    GL(glAttachShader(program, fs));

    if (use_in_2d)
    {
        GL(glBindAttribLocation(program, 0, "InPosition"));
        GL(glBindAttribLocation(program, 1, "InColor"));
        GL(glBindAttribLocation(program, 2, "InTexCoord"));
        GL(glBindAttribLocation(program, 3, "InTexEggCoord"));
    }
    else
    {
        GL(glBindAttribLocation(program, 0, "InPosition"));
        GL(glBindAttribLocation(program, 1, "InNormal"));
        GL(glBindAttribLocation(program, 2, "InTexCoord"));
        GL(glBindAttribLocation(program, 3, "InTexCoordBase"));
        GL(glBindAttribLocation(program, 4, "InTangent"));
        GL(glBindAttribLocation(program, 5, "InBitangent"));
        GL(glBindAttribLocation(program, 6, "InBlendWeights"));
        GL(glBindAttribLocation(program, 7, "InBlendIndices"));
    }

    GL(glLinkProgram(program));
    GLint linked;
    GL(glGetProgramiv(program, GL_LINK_STATUS, &linked));
    if (!linked)
    {
        WriteLog("Failed to link shader program, effect '{}'.\n", fname);
        ShaderInfo::LogProgram(program);
        GL(glDetachShader(program, vs));
        GL(glDetachShader(program, fs));
        GL(glDeleteShader(vs));
        GL(glDeleteShader(fs));
        GL(glDeleteProgram(program));
        return false;
    }

    // Bind data
    GL(effect_pass.ProjectionMatrix = glGetUniformLocation(program, "ProjectionMatrix"));
    GL(effect_pass.ZoomFactor = glGetUniformLocation(program, "ZoomFactor"));
    GL(effect_pass.ColorMap = glGetUniformLocation(program, "ColorMap"));
    GL(effect_pass.ColorMapSize = glGetUniformLocation(program, "ColorMapSize"));
    GL(effect_pass.ColorMapSamples = glGetUniformLocation(program, "ColorMapSamples"));
    GL(effect_pass.EggMap = glGetUniformLocation(program, "EggMap"));
    GL(effect_pass.EggMapSize = glGetUniformLocation(program, "EggMapSize"));
    GL(effect_pass.SpriteBorder = glGetUniformLocation(program, "SpriteBorder"));
    GL(effect_pass.GroundPosition = glGetUniformLocation(program, "GroundPosition"));
    GL(effect_pass.LightColor = glGetUniformLocation(program, "LightColor"));
    GL(effect_pass.WorldMatrices = glGetUniformLocation(program, "WorldMatrices"));

    GL(effect_pass.Time = glGetUniformLocation(program, "Time"));
    effect_pass.TimeCurrent = 0.0f;
    effect_pass.TimeLastTick = Timer::RealtimeTick();
    GL(effect_pass.TimeGame = glGetUniformLocation(program, "TimeGame"));
    effect_pass.TimeGameCurrent = 0.0f;
    effect_pass.TimeGameLastTick = Timer::RealtimeTick();
    effect_pass.IsTime = (effect_pass.Time != -1 || effect_pass.TimeGame != -1);
    GL(effect_pass.Random1 = glGetUniformLocation(program, "Random1Effect"));
    GL(effect_pass.Random2 = glGetUniformLocation(program, "Random2Effect"));
    GL(effect_pass.Random3 = glGetUniformLocation(program, "Random3Effect"));
    GL(effect_pass.Random4 = glGetUniformLocation(program, "Random4Effect"));
    effect_pass.IsRandom =
        (effect_pass.Random1 != -1 || effect_pass.Random2 != -1 || effect_pass.Random3 != -1 || effect_pass.Random4 !=
-1); effect_pass.IsTextures = false; for (int i = 0; i < EFFECT_TEXTURES; i++)
    {
        GL(effect_pass.Textures[i] = glGetUniformLocation(program, _str("Texture{}", i).c_str()));
        if (effect_pass.Textures[i] != -1)
        {
            effect_pass.IsTextures = true;
            GL(effect_pass.TexturesSize[i] = glGetUniformLocation(program, _str("Texture%dSize", i).c_str()));
            GL(effect_pass.TexturesAtlasOffset[i] = glGetUniformLocation(program, _str("Texture{}AtlasOffset",
i).c_str()));
        }
    }
    effect_pass.IsScriptValues = false;
    for (int i = 0; i < EFFECT_SCRIPT_VALUES; i++)
    {
        GL(effect_pass.ScriptValues[i] = glGetUniformLocation(program, _str("EffectValue{}", i).c_str()));
        if (effect_pass.ScriptValues[i] != -1)
            effect_pass.IsScriptValues = true;
    }
    GL(effect_pass.AnimPosProc = glGetUniformLocation(program, "AnimPosProc"));
    GL(effect_pass.AnimPosTime = glGetUniformLocation(program, "AnimPosTime"));
    effect_pass.IsAnimPos = (effect_pass.AnimPosProc != -1 || effect_pass.AnimPosTime != -1);
    effect_pass.IsNeedProcess = (effect_pass.IsTime || effect_pass.IsRandom || effect_pass.IsTextures ||
        effect_pass.IsScriptValues || effect_pass.IsAnimPos || effect_pass.IsChangeStates);

    // Set defaults
    if (defaults)
    {
        GL(glUseProgram(program));
        for (uint d = 0; d < defaults_count; d++)
        {
            EffectDefault& def = defaults[d];

            GLint location = -1;
            GL(location = glGetUniformLocation(program, def.Name));
            if (!IS_EFFECT_VALUE(location))
                continue;

            switch (def.Type)
            {
            case EffectDefault::String:
                break;
            case EffectDefault::Float:
                GL(glUniform1fv(location, def.Size / sizeof(float), (GLfloat*)def.Data));
                break;
            case EffectDefault::Int:
                GL(glUniform1iv(location, def.Size / sizeof(int), (GLint*)def.Data));
                break;
            default:
                break;
            }
        }
        GL(glUseProgram(0));
    }

    effect_pass.Program = program;
    effect->Passes.push_back(effect_pass);
    return true;
}*/
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
    return effect.release();
}

void Application::AppRender::DrawQuads(const Vertex2DVec& /*vbuf*/, const vector<ushort>& /*ibuf*/, RenderEffect* /*effect*/, RenderTexture* /*tex*/)
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        // EnableVertexArray(quadsVertexArray, 4 * curDrawQuad);
        // EnableScissor();

        /*uint pos = 0;
        for (auto& dip : dipQueue)
        {
            for (size_t pass = 0; pass < dip.SourceEffect->Passes.size(); pass++)
            {
                EffectPass& effect_pass = dip.SourceEffect->Passes[pass];

                GL(glUseProgram(effect_pass.Program));

                if (IS_EFFECT_VALUE(effect_pass.ZoomFactor))
                    GL(glUniform1f(effect_pass.ZoomFactor, settings.SpritesZoom));
                if (IS_EFFECT_VALUE(effect_pass.ProjectionMatrix))
                    GL(glUniformMatrix4fv(effect_pass.ProjectionMatrix, 1, GL_FALSE, projectionMatrixCM[0]));
                if (IS_EFFECT_VALUE(effect_pass.ColorMap) && dip.SourceTexture)
                {
                    if (dip.SourceTexture->Samples == 0.0f)
                    {
                        GL(glBindTexture(GL_TEXTURE_2D, dip.SourceTexture->Id));
                    }
                    else
                    {
                        GL(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, dip.SourceTexture->Id));
                        if (IS_EFFECT_VALUE(effect_pass.ColorMapSamples))
                            GL(glUniform1f(effect_pass.ColorMapSamples, dip.SourceTexture->Samples));
                    }
                    GL(glUniform1i(effect_pass.ColorMap, 0));
                    if (IS_EFFECT_VALUE(effect_pass.ColorMapSize))
                        GL(glUniform4fv(effect_pass.ColorMapSize, 1, dip.SourceTexture->SizeData));
                }
                if (IS_EFFECT_VALUE(effect_pass.EggMap) && sprEgg)
                {
                    GL(glActiveTexture(GL_TEXTURE1));
                    GL(glBindTexture(GL_TEXTURE_2D, sprEgg->Atlas->TextureOwner->Id));
                    GL(glActiveTexture(GL_TEXTURE0));
                    GL(glUniform1i(effect_pass.EggMap, 1));
                    if (IS_EFFECT_VALUE(effect_pass.EggMapSize))
                        GL(glUniform4fv(effect_pass.EggMapSize, 1, sprEgg->Atlas->TextureOwner->SizeData));
                }
                if (IS_EFFECT_VALUE(effect_pass.SpriteBorder))
                    GL(glUniform4f(effect_pass.SpriteBorder, dip.SpriteBorder.L, dip.SpriteBorder.T, dip.SpriteBorder.R,
                        dip.SpriteBorder.B));

                if (effect_pass.IsNeedProcess)
                    effectMngr.EffectProcessVariables(effect_pass, true);

                GLsizei count = 6 * dip.SpritesCount;
                GL(glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (void*)(size_t)(pos * 2)));

                if (effect_pass.IsNeedProcess)
                    effectMngr.EffectProcessVariables(effect_pass, false);
            }

            pos += 6 * dip.SpritesCount;
        }
        dipQueue.clear();
        curDrawQuad = 0;

        GL(glUseProgram(0));
        DisableVertexArray(quadsVertexArray);
        DisableScissor();*/
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
}

void Application::AppRender::DrawPrimitive(const Vertex2DVec& /*vbuf*/, const vector<ushort>& /*ibuf*/, RenderEffect* /*effect*/, RenderPrimitiveType /*prim*/)
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
}

void Application::AppRender::DrawMesh(RenderMesh* mesh, RenderEffect* /*effect*/)
{
#if !FO_HEADLESS
#if FO_HAVE_OPENGL
    if (CurRenderType == RenderType::OpenGL) {
        auto* opengl_mesh = dynamic_cast<OpenGLMesh*>(mesh->_pImpl.get());
        if (opengl_mesh->VBO == 0u || mesh->DataChanged) {
            mesh->DataChanged = false;

            if (opengl_mesh->VBO != 0u) {
                GL(glDeleteBuffers(1, &opengl_mesh->VBO));
            }
            if (opengl_mesh->IBO != 0u) {
                GL(glDeleteBuffers(1, &opengl_mesh->IBO));
            }
            if (opengl_mesh->VAO != 0u) {
                GL(glDeleteVertexArrays(1, &opengl_mesh->VAO));
            }

            GL(glGenBuffers(1, &opengl_mesh->VBO));
            GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_mesh->VBO));
            GL(glBufferData(GL_ARRAY_BUFFER, mesh->Vertices.size() * sizeof(Vertex3D), &mesh->Vertices[0], GL_STATIC_DRAW));
            GL(glGenBuffers(1, &opengl_mesh->IBO));
            GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_mesh->IBO));
            GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->Indices.size() * sizeof(short), &mesh->Indices[0], GL_STATIC_DRAW));

            if ((opengl_mesh->VAO == 0u) && GL_HAS(vertex_array_object)) {
                GL(glGenVertexArrays(1, &opengl_mesh->VAO));
                GL(glBindVertexArray(opengl_mesh->VAO));
                GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Position))));
                GL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Normal))));
                GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoord))));
                GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoordBase))));
                GL(glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Tangent))));
                GL(glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Bitangent))));
                GL(glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendWeights))));
                GL(glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendIndices))));
                for (uint i = 0; i <= 7; i++) {
                    GL(glEnableVertexAttribArray(i));
                }
                GL(glBindVertexArray(0));
            }
        }

        if (!mesh->DisableCulling) {
            GL(glEnable(GL_CULL_FACE));
        }
        GL(glEnable(GL_DEPTH_TEST));

        if (opengl_mesh->VAO != 0u) {
            GL(glBindVertexArray(opengl_mesh->VAO));
            GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_mesh->VBO));
            GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_mesh->IBO));
        }
        else {
            GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_mesh->VBO));
            GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_mesh->IBO));
            GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Position))));

            GL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Normal))));
            GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoord))));
            GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoordBase))));
            GL(glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Tangent))));
            GL(glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Bitangent))));
            GL(glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendWeights))));
            GL(glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendIndices))));
            for (uint i = 0; i <= 7; i++) {
                GL(glEnableVertexAttribArray(i));
            }
        }

        /*Effect* effect = (combined_mesh->DrawEffect ? combined_mesh->DrawEffect :
        modelMngr.effectMngr.Effects.Skinned3d); MeshTexture** textures = combined_mesh->Textures;

        for (size_t pass = 0; pass < effect->Passes.size(); pass++)
        {
            EffectPass& effect_pass = effect->Passes[pass];

            if (shadow_disabled && effect_pass.IsShadow)
                continue;

            GL(glUseProgram(effect_pass.Program));

            if (IS_EFFECT_VALUE(effect_pass.ZoomFactor))
                GL(glUniform1f(effect_pass.ZoomFactor, modelMngr.settings.SpritesZoom));
            if (IS_EFFECT_VALUE(effect_pass.ProjectionMatrix))
                GL(glUniformMatrix4fv(effect_pass.ProjectionMatrix, 1, GL_FALSE, modelMngr.matrixProjCM[0]));
            if (IS_EFFECT_VALUE(effect_pass.ColorMap) && textures[0])
            {
                GL(glBindTexture(GL_TEXTURE_2D, textures[0]->Id));
                GL(glUniform1i(effect_pass.ColorMap, 0));
                if (IS_EFFECT_VALUE(effect_pass.ColorMapSize))
                    GL(glUniform4fv(effect_pass.ColorMapSize, 1, textures[0]->SizeData));
            }
            if (IS_EFFECT_VALUE(effect_pass.LightColor))
                GL(glUniform4fv(effect_pass.LightColor, 1, (float*)&modelMngr.lightColor));
            if (IS_EFFECT_VALUE(effect_pass.WorldMatrices))
            {
                GL(glUniformMatrix4fv(effect_pass.WorldMatrices, (GLsizei)combined_mesh->CurBoneMatrix, GL_FALSE,
                    (float*)&modelMngr.worldMatrices[0]));
            }
            if (IS_EFFECT_VALUE(effect_pass.GroundPosition))
                GL(glUniform3fv(effect_pass.GroundPosition, 1, (float*)&groundPos));

            if (effect_pass.IsNeedProcess)
                modelMngr.effectMngr.EffectProcessVariables(effect_pass, true, animPosProc, animPosTime, textures);

            GL(glDrawElements(GL_TRIANGLES, (uint)combined_mesh->Indices.size(), GL_UNSIGNED_SHORT, (void*)0));

            if (effect_pass.IsNeedProcess)
                modelMngr.effectMngr.EffectProcessVariables(effect_pass, false, animPosProc, animPosTime, textures);
        }

        GL(glUseProgram(0));

        if (combined_mesh->VAO)
        {
            GL(glBindVertexArray(0));
        }
        else
        {
            for (uint i = 0; i <= 7; i++)
                GL(glDisableVertexAttribArray(i));
        }

        if (!disableCulling)
            GL(glDisable(GL_CULL_FACE));
        GL(glDisable(GL_DEPTH_TEST));*/
    }
#endif
#if FO_HAVE_DIRECT_3D
    if (CurRenderType == RenderType::Direct3D) {
    }
#endif
#endif
}

auto Application::AppInput::PollEvent(InputEvent& event) -> bool
{
#if !FO_HEADLESS
    if (!EventsQueue.empty()) {
        event = EventsQueue.front();
        EventsQueue.erase(EventsQueue.begin());
        return true;
    }
#endif
    return false;
}

void Application::AppInput::PushEvent(const InputEvent& event)
{
#if !FO_HEADLESS
    NextFrameEventsQueue.push_back(event);
#endif
}

void Application::AppInput::SetClipboardText(const string& text)
{
#if !FO_HEADLESS
    SDL_SetClipboardText(text.c_str());
#endif
}

auto Application::AppInput::GetClipboardText() -> string
{
#if !FO_HEADLESS
    return SDL_GetClipboardText();
#else
    return string();
#endif
}

auto Application::AppAudio::IsEnabled() -> bool
{
#if !FO_HEADLESS
    return AudioDeviceId >= 2;
#else
    return false;
#endif
}

auto Application::AppAudio::GetStreamSize() -> uint
{
    RUNTIME_ASSERT(IsEnabled());

#if !FO_HEADLESS
    return AudioSpec.size;
#else
    return 0u;
#endif
}

auto Application::AppAudio::GetSilence() -> uchar
{
    RUNTIME_ASSERT(IsEnabled());

#if !FO_HEADLESS
    return AudioSpec.silence;
#else
    return 0u;
#endif
}

void Application::AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    RUNTIME_ASSERT(IsEnabled());

    LockDevice();
#if !FO_HEADLESS
    AudioStreamWriter = std::move(stream_callback);
#endif
    UnlockDevice();
}

auto Application::AppAudio::ConvertAudio(int format, int channels, int rate, vector<uchar>& buf) -> bool
{
    RUNTIME_ASSERT(IsEnabled());

#if !FO_HEADLESS
    struct AudioConverter
    {
        int Format {};
        int Channels {};
        int Rate {};
        SDL_AudioCVT Cvt {};
        bool NeedConvert {};
    };
    static vector<AudioConverter> converters {};

    auto get_converter = [format, channels, rate]() -> AudioConverter* {
        const auto it = std::find_if(converters.begin(), converters.end(), [format, channels, rate](AudioConverter& f) { return f.Format == format && f.Channels == channels && f.Rate == rate; });

        if (it == converters.end()) {
            SDL_AudioCVT cvt;
            const auto r = SDL_BuildAudioCVT(&cvt, static_cast<SDL_AudioFormat>(format), channels, rate, AudioSpec.format, AudioSpec.channels, AudioSpec.freq);
            if (r == -1) {
                return nullptr;
            }

            converters.push_back({format, channels, rate, cvt, r == 1});
            return &converters.back();
        }

        return &(*it);
    };

    auto* converter = get_converter();
    if (converter == nullptr) {
        return false;
    }

    if (converter->NeedConvert) {
        converter->Cvt.len = static_cast<int>(buf.size());
        converter->Cvt.buf = buf.data();
        buf.resize(static_cast<size_t>(converter->Cvt.len) * converter->Cvt.len_mult);

        if (SDL_ConvertAudio(&converter->Cvt) != 0) {
            return false;
        }

        buf.resize(converter->Cvt.len_cvt);
    }
#endif

    return true;
}

void Application::AppAudio::MixAudio(uchar* output, uchar* buf, int volume)
{
    RUNTIME_ASSERT(IsEnabled());

#if !FO_HEADLESS
    volume = std::clamp(volume, 0, 100) * SDL_MIX_MAXVOLUME / 100;
    SDL_MixAudioFormat(output, buf, AudioSpec.format, AudioSpec.size, volume);
#endif
}

void Application::AppAudio::LockDevice()
{
    RUNTIME_ASSERT(IsEnabled());

#if !FO_HEADLESS
    SDL_LockAudioDevice(AudioDeviceId);
#endif
}

void Application::AppAudio::UnlockDevice()
{
    RUNTIME_ASSERT(IsEnabled());

#if !FO_HEADLESS
    SDL_UnlockAudioDevice(AudioDeviceId);
#endif
}
