#include "SpriteManager.h"
#include "3dStuff.h"
#include "EffectManager.h"
#include "Log.h"
#include "SpriteManager.h"
#include "Sprites.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "WinApi_Include.h"

#if defined(FO_WINDOWS) || defined(FO_LINUX) || defined(FO_MAC)
#include "png.h"
#endif

#ifdef FO_WEB
#define SDL_GL_SwapWindow(a) (void)0
#endif

#define MAX_ATLAS_SIZE (4096)
#define SPRITES_BUFFER_SIZE (10000)
#define ATLAS_SPRITES_PADDING (1)
#define ARRAY_BUFFERS_COUNT (300)

#ifdef FO_IOS
SDL_Window* SprMngr_MainWindow;
#endif

SpriteManager::SpriteManager(RenderSettings& sett, FileManager& file_mngr, EffectManager& effect_mngr) :
    settings {sett}, fileMngr {file_mngr}, effectMngr {effect_mngr}
{
    baseColor = COLOR_RGBA(255, 128, 128, 128);
    allAtlases.reserve(100);
    dipQueue.reserve(1000);
    drawQuadCount = 1024;

    // Detect tablets
    bool is_tablet = false;
#if defined(FO_IOS) || defined(FO_ANDROID)
    is_tablet = true;
#endif
#if defined(FO_WINDOWS)
    is_tablet = (GetSystemMetrics(SM_TABLETPC) != 0);
#endif
    if (is_tablet)
    {
        SDL_DisplayMode mode;
        int r = SDL_GetCurrentDisplayMode(0, &mode);
        RUNTIME_ASSERT_STR(!r, "SDL_GetCurrentDisplayMode");
        settings.ScreenWidth = MAX(mode.w, mode.h);
        settings.ScreenHeight = MIN(mode.w, mode.h);
        float ratio = (float)settings.ScreenWidth / (float)settings.ScreenHeight;
        settings.ScreenHeight = 768;
        settings.ScreenWidth = (int)((float)settings.ScreenHeight * ratio + 0.5f);
        settings.FullScreen = true;
    }

#ifdef FO_WEB
    // Adaptive size
    int window_w = EM_ASM_INT(return window.innerWidth || document.documentElement.clientWidth ||
        document.getElementsByTagName('body')[0].clientWidth);
    int window_h = EM_ASM_INT(return window.innerHeight || document.documentElement.clientHeight ||
        document.getElementsByTagName('body')[0].clientHeight);
    settings.ScreenWidth = CLAMP(window_w - 200, 1024, 1920);
    settings.ScreenHeight = CLAMP(window_h - 100, 768, 1080);

    // Fixed size
    int fixed_w = EM_ASM_INT(return 'foScreenWidth' in Module ? Module.foScreenWidth : 0);
    int fixed_h = EM_ASM_INT(return 'foScreenHeight' in Module ? Module.foScreenHeight : 0);
    if (fixed_w)
        settings.ScreenWidth = fixed_w;
    if (fixed_h)
        settings.ScreenHeight = fixed_h;
#endif

    // Initialize window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
#ifdef FO_OPENGL_ES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    uint window_create_flags = SDL_WINDOW_SHOWN;
    if (settings.FullScreen)
        window_create_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (is_tablet)
    {
        window_create_flags |= SDL_WINDOW_FULLSCREEN;
        window_create_flags |= SDL_WINDOW_BORDERLESS;
    }
#ifndef FO_WEB
    window_create_flags |= SDL_WINDOW_OPENGL;
#endif

    mainWindow = SDL_CreateWindow(settings.WindowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        settings.ScreenWidth, settings.ScreenHeight, window_create_flags);
    RUNTIME_ASSERT_STR(mainWindow, _str("SDL Window not created, error '{}'.\n", SDL_GetError()));

#ifdef FO_IOS
    SprMngr_MainWindow = mainWindow;
#endif

#ifndef FO_WEB
    SDL_GLContext gl_context = SDL_GL_CreateContext(mainWindow);
    RUNTIME_ASSERT_STR(gl_context, _str("OpenGL context not created, error '{}'.\n", SDL_GetError()));

    int make_current = SDL_GL_MakeCurrent(mainWindow, gl_context);
    RUNTIME_ASSERT_STR(make_current >= 0, _str("Can't set current context, error '{}'.\n", SDL_GetError()));

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
    attr.preferLowPowerToHighPerformance = EM_FALSE;
    attr.failIfMajorPerformanceCaveat = EM_FALSE;
    attr.enableExtensionsByDefault = EM_TRUE;
    attr.explicitSwapControl = EM_FALSE;
    attr.renderViaOffscreenBackBuffer = EM_FALSE;

    attr.majorVersion = 2;
    attr.minorVersion = 0;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl_context = emscripten_webgl_create_context(nullptr, &attr);
    if (gl_context <= 0)
    {
        attr.majorVersion = 1;
        attr.minorVersion = 0;
        gl_context = emscripten_webgl_create_context(nullptr, &attr);
        RUNTIME_ASSERT_STR(gl_context > 0, _str("Failed to create WebGL context, error '{}'.\n", (int)gl_context));
        WriteLog("Created WebGL1 context.\n");
    }
    else
    {
        WriteLog("Created WebGL2 context.\n");
    }

    EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(gl_context);
    RUNTIME_ASSERT_STR(r >= 0, _str("Can't set current context, error '{}'.\n", r));
#endif

    SDL_ShowCursor(0);

    GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &baseFBO));
    if (settings.AlwaysOnTop)
        SetAlwaysOnTop(true);

    // Calculate atlas size
    GLint max_texture_size;
    GLint max_viewport_size[2];
    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size));
    GL(glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size));
    atlasWidth = atlasHeight = MIN(max_texture_size, MAX_ATLAS_SIZE);
    atlasWidth = MIN(max_viewport_size[0], atlasWidth);
    atlasHeight = MIN(max_viewport_size[1], atlasHeight);
    atlasWidth = MIN(2048, atlasWidth);
    atlasHeight = MIN(2048, atlasHeight);
    RUNTIME_ASSERT_STR(atlasWidth >= 2048, "Min texture width must be at least 2048");
    RUNTIME_ASSERT_STR(atlasHeight >= 2048, "Min texture height must be at least 2048");

    // Init graphic
    bool graphic_ok = GraphicApi::Init();
    RUNTIME_ASSERT(graphic_ok);

#ifdef FO_WEB
    OGL_vertex_array_object =
        (attr.majorVersion > 1 || emscripten_webgl_enable_extension(gl_context, "OES_vertex_array_object"));
#endif

// Check OpenGL extensions
#define CHECK_EXTENSION(ext, critical) \
    if (!GL_HAS(ext)) \
    { \
        string msg = (critical ? "Critical" : "Not critical"); \
        WriteLog("OpenGL extension '" #ext "' not supported. {}.\n", msg); \
        if (critical) \
            extension_errors++; \
    }
    uint extension_errors = 0;
    CHECK_EXTENSION(version_2_0, true);
    CHECK_EXTENSION(vertex_buffer_object, true);
    CHECK_EXTENSION(vertex_array_object, false);
    CHECK_EXTENSION(framebuffer_object, false);
    if (!GL_HAS(framebuffer_object))
    {
        CHECK_EXTENSION(framebuffer_object_ext, true);
        CHECK_EXTENSION(framebuffer_multisample, false);
    }
    CHECK_EXTENSION(texture_multisample, false);
#undef CHECK_EXTENSION
    RUNTIME_ASSERT(!extension_errors);

    // Default effects
    bool effects_ok = effectMngr.LoadMinimalEffects();
    RUNTIME_ASSERT(effects_ok);

    // Render states
    GL(GraphicApi::MatrixOrtho(
        projectionMatrixCM[0], 0.0f, (float)settings.ScreenWidth, (float)settings.ScreenHeight, 0.0f, -1.0f, 1.0f));
    projectionMatrixCM.Transpose(); // Convert to column major order
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glEnable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glDisable(GL_CULL_FACE));
    GL(glEnable(GL_TEXTURE_2D));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
#ifndef FO_OPENGL_ES
    GL(glDisable(GL_LIGHTING));
    GL(glDisable(GL_COLOR_MATERIAL));
    GL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
#endif

    // Vertex arrays
    quadsVertexArray = new VertexArray[ARRAY_BUFFERS_COUNT];
    pointsVertexArray = new VertexArray[ARRAY_BUFFERS_COUNT];
    for (uint i = 0; i < ARRAY_BUFFERS_COUNT; i++)
    {
        VertexArray* qva = &quadsVertexArray[i];
        memzero(qva, sizeof(VertexArray));
        qva->Next = &quadsVertexArray[i + 1 < ARRAY_BUFFERS_COUNT ? i + 1 : 0];
        InitVertexArray(qva, true, drawQuadCount);

        VertexArray* dva = &pointsVertexArray[i];
        memzero(dva, sizeof(VertexArray));
        dva->Next = &pointsVertexArray[i + 1 < ARRAY_BUFFERS_COUNT ? i + 1 : 0];
        InitVertexArray(dva, false, drawQuadCount);
    }

    // Vertex buffer
    vBuffer.resize(drawQuadCount * 4);

    // Sprites buffer
    sprData.resize(SPRITES_BUFFER_SIZE);
    for (auto it = sprData.begin(), end = sprData.end(); it != end; ++it)
        (*it) = nullptr;

    // Render targets
    rtMain = CreateRenderTarget(false, false, true, 0, 0, true);
    rtContours = CreateRenderTarget(false, false, true, 0, 0, false);
    rtContoursMid = CreateRenderTarget(false, false, true, 0, 0, false);

    // Clear scene
    GL(glClear(GL_COLOR_BUFFER_BIT));
    SDL_GL_SwapWindow(mainWindow);
    if (rtMain)
        PushRenderTarget(rtMain);

    // Generate dummy animation
    if (!DummyAnimation)
    {
        DummyAnimation = (AnyFrames*)new uchar[sizeof(AnyFrames)];
        memzero(DummyAnimation, sizeof(AnyFrames));
        DummyAnimation->CntFrm = 1;
        DummyAnimation->Ticks = 100;
    }

    Sprites::GrowPool();

    // 3d initialization
    if (settings.Enable3dRendering)
    {
        anim3dMngr = new Animation3dManager(settings, fileMngr, effectMngr, [this](MeshTexture* mesh_tex) {
            PushAtlasType(AtlasType::MeshTextures);
            AnyFrames* anim = LoadAnimation(_str(mesh_tex->ModelPath).extractDir() + mesh_tex->Name);
            PopAtlasType();
            if (anim)
            {
                SpriteInfo* si = GetSpriteInfo(anim->Ind[0]);
                mesh_tex->Id = si->Atlas->TextureOwner->Id;
                memcpy(mesh_tex->SizeData, si->Atlas->TextureOwner->SizeData, sizeof(mesh_tex->SizeData));
                mesh_tex->AtlasOffsetData[0] = si->SprRect[0];
                mesh_tex->AtlasOffsetData[1] = si->SprRect[1];
                mesh_tex->AtlasOffsetData[2] = si->SprRect[2] - si->SprRect[0];
                mesh_tex->AtlasOffsetData[3] = si->SprRect[3] - si->SprRect[1];
                DestroyAnyFrames(anim);
            }
        });
    }
}

SpriteManager::~SpriteManager()
{
    DeleteRenderTarget(rtMain);
    DeleteRenderTarget(rtContours);
    DeleteRenderTarget(rtContoursMid);
    for (auto it = rt3D.begin(), end = rt3D.end(); it != end; ++it)
        DeleteRenderTarget(*it);
    rt3D.clear();

    for (auto it = allAtlases.begin(), end = allAtlases.end(); it != end; ++it)
        SAFEDEL(*it);
    allAtlases.clear();
    for (auto it = sprData.begin(), end = sprData.end(); it != end; ++it)
        SAFEDEL(*it);
    sprData.clear();
    dipQueue.clear();

    SAFEDEL(anim3dMngr);
}

void SpriteManager::Preload3dModel(const string& model_name)
{
    RUNTIME_ASSERT(anim3dMngr);
    anim3dMngr->PreloadEntity(model_name);
}

void SpriteManager::GetWindowSize(int& w, int& h)
{
    SDL_GetWindowSize(mainWindow, &w, &h);
}

void SpriteManager::SetWindowSize(int w, int h)
{
    SDL_SetWindowSize(mainWindow, w, h);
}

void SpriteManager::GetWindowPosition(int& x, int& y)
{
    SDL_GetWindowPosition(mainWindow, &x, &y);
}

void SpriteManager::SetWindowPosition(int x, int y)
{
    SDL_SetWindowPosition(mainWindow, x, y);
}

void SpriteManager::GetMousePosition(int& x, int& y)
{
    SDL_GetMouseState(&x, &y);
}

void SpriteManager::SetMousePosition(int x, int y)
{
    SDL_WarpMouseInWindow(mainWindow, x, y);
}

bool SpriteManager::IsWindowFocused()
{
    return SDL_GetWindowFlags(mainWindow) & SDL_WINDOW_INPUT_FOCUS;
}

void SpriteManager::MinimizeWindow()
{
    SDL_MinimizeWindow(mainWindow);
}

bool SpriteManager::EnableFullscreen()
{
    return !SDL_SetWindowFullscreen(mainWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

bool SpriteManager::DisableFullscreen()
{
    return !SDL_SetWindowFullscreen(mainWindow, 0);
}

void SpriteManager::BlinkWindow()
{
#ifdef FO_WINDOWS
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(mainWindow, &info))
        FlashWindow(info.info.win.window, true);
#endif
}

void SpriteManager::BeginScene(uint clear_color)
{
    // Render 3d animations
    if (settings.Enable3dRendering && !autoRedrawAnim3d.empty())
    {
        for (auto it = autoRedrawAnim3d.begin(), end = autoRedrawAnim3d.end(); it != end; ++it)
        {
            Animation3d* anim3d = *it;
            if (anim3d->NeedDraw())
                Render3d(anim3d);
        }
    }

    // Clear window
    if (clear_color)
        ClearCurrentRenderTarget(clear_color);

    sceneBeginned = true;
}

void SpriteManager::EndScene()
{
    Flush();

    if (rtMain)
    {
        PopRenderTarget();
        DrawRenderTarget(rtMain, false);
        SDL_GL_SwapWindow(mainWindow);
        PushRenderTarget(rtMain);
    }
    else
    {
        SDL_GL_SwapWindow(mainWindow);
    }

    if (settings.OpenGLDebug && glGetError() != GL_NO_ERROR)
    {
        WriteLog("Unknown place of OpenGL error.\n");
        exit(0);
    }
    sceneBeginned = false;
}

void SpriteManager::OnResolutionChanged()
{
    for (auto it = rtAll.begin(), end = rtAll.end(); it != end; ++it)
    {
        RenderTarget* rt = *it;
        if (rt->ScreenSize)
            CreateRenderTarget(rt->DepthBuffer != 0, rt->Multisampling, rt->ScreenSize, rt->Width, rt->Height,
                rt->TexLinear, rt->DrawEffect, rt);
    }

    RefreshViewport();
}

void SpriteManager::SetAlwaysOnTop(bool enable)
{
#ifdef FO_WINDOWS
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(mainWindow, &info))
        SetWindowPos(info.info.win.window, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
#endif
}

RenderTarget* SpriteManager::CreateRenderTarget(bool depth, bool multisampling, bool screen_size, uint width,
    uint height, bool tex_linear, Effect* effect, RenderTarget* rt_refresh)
{
    // Flush current sprites
    Flush();

    // Allocate new or refresh old
    RenderTarget* rt = rt_refresh;
    if (!rt)
        rt = new RenderTarget();
    else
        CleanRenderTarget(rt);

    // Zero data
    memzero(rt, sizeof(RenderTarget));
    RUNTIME_ASSERT((screen_size || (width && height)));
    rt->ScreenSize = screen_size;
    rt->Width = width;
    rt->Height = height;
    width = (screen_size ? settings.ScreenWidth + width : width);
    height = (screen_size ? settings.ScreenHeight + height : height);

    // Multisampling
    static int samples = -1;
    if (multisampling && samples == -1 &&
        (GL_HAS(framebuffer_object) || (GL_HAS(framebuffer_object_ext) && GL_HAS(framebuffer_multisample))))
    {
        if (GL_HAS(texture_multisample) && settings.MultiSampling != 0)
        {
            // Samples count
            GLint max_samples = 0;
            GL(glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &max_samples));
            samples = MIN(settings.MultiSampling > 0 ? settings.MultiSampling : 8, max_samples);

            // Flush effect
            effectMngr.Effects.FlushRenderTargetMSDefault =
                effectMngr.LoadEffect("Flush_RenderTargetMS.glsl", true, "", "Effects/");
            if (effectMngr.Effects.FlushRenderTargetMSDefault)
                effectMngr.Effects.FlushRenderTargetMS = effectMngr.Effects.FlushRenderTargetMSDefault;
            else
                samples = 0;
        }
        else
        {
            samples = 0;
        }
    }
    if (multisampling && samples <= 1)
    {
        multisampling = false;
        samples = 0;
    }
    rt->Multisampling = multisampling;

    // Framebuffer
    if (GL_HAS(framebuffer_object))
    {
        GL(glGenFramebuffers(1, &rt->FBO));
        GL(glBindFramebuffer(GL_FRAMEBUFFER, rt->FBO));
    }
    else // framebuffer_object_ext
    {
        GL(glGenFramebuffersEXT(1, &rt->FBO));
        GL(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, rt->FBO));
    }

    // Texture
    Texture* tex = new Texture();
    tex->Width = width;
    tex->Height = height;
    tex->SizeData[0] = (float)width;
    tex->SizeData[1] = (float)height;
    tex->SizeData[2] = 1.0f / tex->SizeData[0];
    tex->SizeData[3] = 1.0f / tex->SizeData[1];
    GL(glGenTextures(1, &tex->Id));
    if (!multisampling)
    {
        GL(glBindTexture(GL_TEXTURE_2D, tex->Id));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_linear ? GL_LINEAR : GL_NEAREST));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_linear ? GL_LINEAR : GL_NEAREST));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP));
        GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
        if (GL_HAS(framebuffer_object))
        {
            GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->Id, 0));
        }
        else // framebuffer_object_ext
        {
            GL(glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tex->Id, 0));
        }
    }
    else
    {
        tex->Samples = (float)samples;
        GL(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex->Id));
        GL(glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA, width, height, GL_TRUE));
        if (GL_HAS(framebuffer_object))
        {
            GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex->Id, 0));
        }
        else // framebuffer_object_ext
        {
            GL(glFramebufferTexture2DEXT(
                GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D_MULTISAMPLE, tex->Id, 0));
        }
    }
    rt->TargetTexture = tex;
    rt->TexLinear = tex_linear;

    // Depth
    if (depth)
    {
        if (GL_HAS(framebuffer_object))
        {
            GLint cur_rb;
            GL(glGetIntegerv(GL_RENDERBUFFER_BINDING, &cur_rb));
            GL(glGenRenderbuffers(1, &rt->DepthBuffer));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, rt->DepthBuffer));
            if (!multisampling)
            {
                GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height));
            }
            else
            {
                GL(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT16, width, height));
            }
            GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->DepthBuffer));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, cur_rb));
        }
        else // framebuffer_object_ext
        {
            GLint cur_rb;
            GL(glGetIntegerv(GL_RENDERBUFFER_BINDING_EXT, &cur_rb));
            GL(glGenRenderbuffersEXT(1, &rt->DepthBuffer));
            GL(glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rt->DepthBuffer));
            if (!multisampling)
            {
                GL(glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT16, width, height));
            }
            else
            {
                GL(glRenderbufferStorageMultisampleEXT(
                    GL_RENDERBUFFER_EXT, samples, GL_DEPTH_COMPONENT16, width, height));
            }
            GL(glFramebufferRenderbufferEXT(
                GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rt->DepthBuffer));
            GL(glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, cur_rb));
        }
    }

    // Check framebuffer creation status
    if (GL_HAS(framebuffer_object))
    {
        GLenum status;
        GL(status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
        RUNTIME_ASSERT_STR(status == GL_FRAMEBUFFER_COMPLETE, _str("Framebuffer not created, status {:#X}.\n", status));
    }
    else // framebuffer_object_ext
    {
        GLenum status;
        GL(status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT));
        RUNTIME_ASSERT_STR(
            status == GL_FRAMEBUFFER_COMPLETE_EXT, _str("FramebufferExt not created, status {:#X}.\n", status));
    }

    // Effect
    if (!effect)
    {
        if (!multisampling)
            rt->DrawEffect = effectMngr.Effects.FlushRenderTarget;
        else
            rt->DrawEffect = effectMngr.Effects.FlushRenderTargetMS;
    }
    else
    {
        rt->DrawEffect = effect;
    }

    // Clear
    PushRenderTarget(rt);
    ClearCurrentRenderTarget(0);
    if (depth)
        ClearCurrentRenderTargetDepth();
    PopRenderTarget();

    if (!rt_refresh)
        rtAll.push_back(rt);
    return rt;
}

void SpriteManager::CleanRenderTarget(RenderTarget* rt)
{
    if (GL_HAS(framebuffer_object))
    {
        if (rt->DepthBuffer)
            GL(glDeleteRenderbuffers(1, &rt->DepthBuffer));
        GL(glDeleteFramebuffers(1, &rt->FBO));
    }
    else // framebuffer_object_ext
    {
        if (rt->DepthBuffer)
            GL(glDeleteRenderbuffersEXT(1, &rt->DepthBuffer));
        GL(glDeleteFramebuffersEXT(1, &rt->FBO));
    }
    SAFEDEL(rt->TargetTexture);
    SAFEDEL(rt->LastPixelPicks);
}

void SpriteManager::DeleteRenderTarget(RenderTarget*& rt)
{
    if (!rt)
        return;

    CleanRenderTarget(rt);
    rtAll.erase(std::find(rtAll.begin(), rtAll.end(), rt));
    SAFEDEL(rt);
}

void SpriteManager::PushRenderTarget(RenderTarget* rt)
{
    Flush();

    bool redundant = (!rtStack.empty() && rtStack.back() == rt);
    rtStack.push_back(rt);
    if (!redundant)
    {
        Flush();
        if (GL_HAS(framebuffer_object))
        {
            GL(glBindFramebuffer(GL_FRAMEBUFFER, rt->FBO));
        }
        else // framebuffer_object_ext
        {
            GL(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, rt->FBO));
        }
        RefreshViewport();

        // Invalidate last pixel color picking
        if (rt->LastPixelPicks && !rt->LastPixelPicks->empty())
            rt->LastPixelPicks->clear();
    }
}

void SpriteManager::PopRenderTarget()
{
    bool redundant = (rtStack.size() > 2 && rtStack.back() == rtStack[rtStack.size() - 2]);
    rtStack.pop_back();
    if (!redundant)
    {
        Flush();
        if (GL_HAS(framebuffer_object))
        {
            GL(glBindFramebuffer(GL_FRAMEBUFFER, rtStack.empty() ? baseFBO : rtStack.back()->FBO));
        }
        else // framebuffer_object_ext
        {
            GL(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, rtStack.empty() ? baseFBO : rtStack.back()->FBO));
        }
        RefreshViewport();
    }
}

void SpriteManager::DrawRenderTarget(RenderTarget* rt, bool alpha_blend, const Rect* region_from, const Rect* region_to)
{
    Flush();

    if (!region_from && !region_to)
    {
        float w = (float)rt->TargetTexture->Width;
        float h = (float)rt->TargetTexture->Height;
        uint pos = 0;
        vBuffer[pos].X = 0.0f;
        vBuffer[pos].Y = h;
        vBuffer[pos].TU = 0.0f;
        vBuffer[pos++].TV = 0.0f;
        vBuffer[pos].X = 0.0f;
        vBuffer[pos].Y = 0.0f;
        vBuffer[pos].TU = 0.0f;
        vBuffer[pos++].TV = 1.0f;
        vBuffer[pos].X = w;
        vBuffer[pos].Y = 0.0f;
        vBuffer[pos].TU = 1.0f;
        vBuffer[pos++].TV = 1.0f;
        vBuffer[pos].X = w;
        vBuffer[pos].Y = h;
        vBuffer[pos].TU = 1.0f;
        vBuffer[pos++].TV = 0.0f;
    }
    else
    {
        RectF regionf = (region_from ? *region_from : Rect(0, 0, rt->TargetTexture->Width, rt->TargetTexture->Height));
        RectF regiont =
            (region_to ? *region_to :
                         Rect(0, 0, rtStack.back()->TargetTexture->Width, rtStack.back()->TargetTexture->Height));
        float wf = (float)rt->TargetTexture->Width;
        float hf = (float)rt->TargetTexture->Height;
        uint pos = 0;
        vBuffer[pos].X = regiont.L;
        vBuffer[pos].Y = regiont.B;
        vBuffer[pos].TU = regionf.L / wf;
        vBuffer[pos++].TV = 1.0f - regionf.B / hf;
        vBuffer[pos].X = regiont.L;
        vBuffer[pos].Y = regiont.T;
        vBuffer[pos].TU = regionf.L / wf;
        vBuffer[pos++].TV = 1.0f - regionf.T / hf;
        vBuffer[pos].X = regiont.R;
        vBuffer[pos].Y = regiont.T;
        vBuffer[pos].TU = regionf.R / wf;
        vBuffer[pos++].TV = 1.0f - regionf.T / hf;
        vBuffer[pos].X = regiont.R;
        vBuffer[pos].Y = regiont.B;
        vBuffer[pos].TU = regionf.R / wf;
        vBuffer[pos++].TV = 1.0f - regionf.B / hf;
    }

    curDrawQuad = 1;
    dipQueue.push_back({rt->TargetTexture, rt->DrawEffect, 1});

    if (!alpha_blend)
        GL(glDisable(GL_BLEND));
    Flush();
    if (!alpha_blend)
        GL(glEnable(GL_BLEND));
}

uint SpriteManager::GetRenderTargetPixel(RenderTarget* rt, int x, int y)
{
    // Try find in last picks
    uint xy = (x << 16) | y;
    for (auto it = rt->LastPixelPicks->begin(), end = rt->LastPixelPicks->end(); it != end; ++it)
        if (it->first == xy)
            return it->second;

    // Set FBO for pixel reading
    GLint prev_fbo;
    GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));
    if (GL_HAS(framebuffer_object))
    {
        GL(glBindFramebuffer(GL_FRAMEBUFFER, rt->FBO));
    }
    else // framebuffer_object_ext
    {
        GL(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, rt->FBO));
    }

    // Read one pixel
    uint result = 0;
    GL(glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &result));

    // Restore previous FBO
    if (GL_HAS(framebuffer_object))
    {
        GL(glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo));
    }
    else // framebuffer_object_ext
    {
        GL(glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, prev_fbo));
    }

    // Refresh picks
    rt->LastPixelPicks->insert(rt->LastPixelPicks->begin(), std::make_pair(xy, result));
    if (rt->LastPixelPicks->size() > MAX_STORED_PIXEL_PICKS)
        rt->LastPixelPicks->pop_back();

    return result;
}

void SpriteManager::ClearCurrentRenderTarget(uint color)
{
    float a = (float)((color >> 24) & 0xFF) / 255.0f;
    float r = (float)((color >> 16) & 0xFF) / 255.0f;
    float g = (float)((color >> 8) & 0xFF) / 255.0f;
    float b = (float)((color >> 0) & 0xFF) / 255.0f;
    GL(glClearColor(r, g, b, a));
    GL(glClear(GL_COLOR_BUFFER_BIT));
}

void SpriteManager::ClearCurrentRenderTargetDepth()
{
    GL(glClear(GL_DEPTH_BUFFER_BIT));
}

void SpriteManager::RefreshViewport()
{
    int w, h;
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
        GL(GraphicApi::MatrixOrtho(
            projectionMatrixCM[0], 0.0f, (float)settings.ScreenWidth, (float)settings.ScreenHeight, 0.0f, -1.0f, 1.0f));
    }
    else
    {
        GL(GraphicApi::MatrixOrtho(projectionMatrixCM[0], 0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f));
    }
    projectionMatrixCM.Transpose(); // Convert to column major order
}

RenderTarget* SpriteManager::Get3dRenderTarget(uint width, uint height)
{
    for (auto it = rt3D.begin(), end = rt3D.end(); it != end; ++it)
    {
        RenderTarget* rt = *it;
        if (rt->TargetTexture->Width == width && rt->TargetTexture->Height == height)
            return rt;
    }
    rt3D.push_back(CreateRenderTarget(true, true, false, width, height, false));
    return rt3D.back();
}

void SpriteManager::InitVertexArray(VertexArray* va, bool quads, uint count)
{
    // Resize indices
    UShortVec& indices = (quads ? quadsIndices : pointsIndices);
    uint indices_count = (quads ? count * 6 : count);
    uint max_indices_count = (uint)indices.size();
    if (indices_count > max_indices_count)
    {
        indices.resize(indices_count);
        if (quads)
        {
            for (uint i = max_indices_count / 6, j = indices_count / 6; i < j; i++)
            {
                indices[i * 6 + 0] = i * 4 + 0;
                indices[i * 6 + 1] = i * 4 + 1;
                indices[i * 6 + 2] = i * 4 + 3;
                indices[i * 6 + 3] = i * 4 + 1;
                indices[i * 6 + 4] = i * 4 + 2;
                indices[i * 6 + 5] = i * 4 + 3;
            }
        }
        else
        {
            for (uint i = max_indices_count; i < indices_count; i++)
                indices[i] = i;
        }
    }

    // Vertex buffer
    uint vertices_count = (quads ? count * 4 : count);
    if (!va->VBO)
        GL(glGenBuffers(1, &va->VBO));
    GL(glBindBuffer(GL_ARRAY_BUFFER, va->VBO));
    GL(glBufferData(GL_ARRAY_BUFFER, vertices_count * sizeof(Vertex2D), nullptr, GL_DYNAMIC_DRAW));
    va->VCount = vertices_count;

    // Index buffer
    if (!va->IBO)
        GL(glGenBuffers(1, &va->IBO));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, va->IBO));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_count * sizeof(ushort), &indices[0], GL_STATIC_DRAW));
    va->ICount = indices_count;

    // Vertex array
    if (!va->VAO && GL_HAS(vertex_array_object) && (GL_HAS(framebuffer_object) || GL_HAS(framebuffer_object_ext)))
    {
        GL(glGenVertexArrays(1, &va->VAO));
        GL(glBindVertexArray(va->VAO));
        BindVertexArray(va);
        GL(glBindVertexArray(0));
    }
}

void SpriteManager::BindVertexArray(VertexArray* va)
{
    GL(glBindBuffer(GL_ARRAY_BUFFER, va->VBO));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, va->IBO));
    GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), ATTRIB_OFFSET(Vertex2D, X)));
    GL(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex2D), ATTRIB_OFFSET(Vertex2D, Diffuse)));
    GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), ATTRIB_OFFSET(Vertex2D, TU)));
    GL(glEnableVertexAttribArray(0));
    GL(glEnableVertexAttribArray(1));
    GL(glEnableVertexAttribArray(2));
    GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), ATTRIB_OFFSET(Vertex2D, TUEgg)));
    GL(glEnableVertexAttribArray(3));
}

void SpriteManager::EnableVertexArray(VertexArray* va, uint vertices_count)
{
    if (va->VAO)
    {
        GL(glBindVertexArray(va->VAO));
        GL(glBindBuffer(GL_ARRAY_BUFFER, va->VBO));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, va->IBO));
    }
    else
    {
        BindVertexArray(va);
    }
    GL(glBufferSubData(GL_ARRAY_BUFFER, 0, vertices_count * sizeof(Vertex2D), &vBuffer[0]));
}

void SpriteManager::DisableVertexArray(VertexArray*& va)
{
    if (va->VAO)
    {
        GL(glBindVertexArray(0));
    }
    else
    {
        GL(glDisableVertexAttribArray(0));
        GL(glDisableVertexAttribArray(1));
        GL(glDisableVertexAttribArray(2));
        GL(glDisableVertexAttribArray(3));
    }
    if (va->Next)
        va = va->Next;
}

void SpriteManager::PushScissor(int l, int t, int r, int b)
{
    Flush();
    scissorStack.push_back(l);
    scissorStack.push_back(t);
    scissorStack.push_back(r);
    scissorStack.push_back(b);
    RefreshScissor();
}

void SpriteManager::PopScissor()
{
    if (!scissorStack.empty())
    {
        Flush();
        scissorStack.resize(scissorStack.size() - 4);
        RefreshScissor();
    }
}

void SpriteManager::RefreshScissor()
{
    if (!scissorStack.empty())
    {
        scissorRect.L = scissorStack[0];
        scissorRect.T = scissorStack[1];
        scissorRect.R = scissorStack[2];
        scissorRect.B = scissorStack[3];
        for (size_t i = 4; i < scissorStack.size(); i += 4)
        {
            if (scissorStack[i + 0] > scissorRect.L)
                scissorRect.L = scissorStack[i + 0];
            if (scissorStack[i + 1] > scissorRect.T)
                scissorRect.T = scissorStack[i + 1];
            if (scissorStack[i + 2] < scissorRect.R)
                scissorRect.R = scissorStack[i + 2];
            if (scissorStack[i + 3] < scissorRect.B)
                scissorRect.B = scissorStack[i + 3];
        }
        if (scissorRect.L > scissorRect.R)
            scissorRect.L = scissorRect.R;
        if (scissorRect.T > scissorRect.B)
            scissorRect.T = scissorRect.B;
    }
}

void SpriteManager::EnableScissor()
{
    if (!scissorStack.empty() && !rtStack.empty() && rtStack.back() == rtMain)
    {
        GL(glEnable(GL_SCISSOR_TEST));
        GL(glScissor(scissorRect.L, rtStack.back()->TargetTexture->Height - scissorRect.B,
            scissorRect.R - scissorRect.L, scissorRect.B - scissorRect.T));
    }
}

void SpriteManager::DisableScissor()
{
    if (!scissorStack.empty() && !rtStack.empty() && rtStack.back() == rtMain)
        GL(glDisable(GL_SCISSOR_TEST));
}

void SpriteManager::PushAtlasType(AtlasType atlas_type, bool one_image)
{
    atlasStack.push_back({atlas_type, one_image});
}

void SpriteManager::PopAtlasType()
{
    atlasStack.pop_back();
}

void SpriteManager::AccumulateAtlasData()
{
    accumulatorActive = true;
}

void SpriteManager::FlushAccumulatedAtlasData()
{
    accumulatorActive = false;
    if (accumulatorSprInfo.empty())
        return;

    // Sort by size
    std::sort(accumulatorSprInfo.begin(), accumulatorSprInfo.end(),
        [](SpriteInfo* si1, SpriteInfo* si2) { return si1->Width * si1->Height > si2->Width * si2->Height; });

    for (auto it = accumulatorSprInfo.begin(), end = accumulatorSprInfo.end(); it != end; ++it)
        FillAtlas(*it);
    accumulatorSprInfo.clear();
}

bool SpriteManager::IsAccumulateAtlasActive()
{
    return accumulatorActive;
}

TextureAtlas* SpriteManager::CreateAtlas(int w, int h)
{
    TextureAtlas* atlas = new TextureAtlas();
    atlas->Type = std::get<0>(atlasStack.back());

    if (!std::get<1>(atlasStack.back()))
    {
        w = atlasWidth;
        h = atlasHeight;
    }

    atlas->RT = CreateRenderTarget(false, false, false, w, h, true);
    atlas->RT->LastPixelPicks = new UIntPairVec();
    atlas->RT->LastPixelPicks->reserve(MAX_STORED_PIXEL_PICKS);
    atlas->TextureOwner = atlas->RT->TargetTexture;
    atlas->Width = w;
    atlas->Height = h;
    atlas->RootNode = (accumulatorActive ? new TextureAtlas::SpaceNode(0, 0, w, h) : nullptr);
    allAtlases.push_back(atlas);
    return atlas;
}

TextureAtlas* SpriteManager::FindAtlasPlace(SpriteInfo* si, int& x, int& y)
{
    // Find place in already created atlas
    TextureAtlas* atlas = nullptr;
    AtlasType atlas_type = std::get<0>(atlasStack.back());
    uint w = si->Width + ATLAS_SPRITES_PADDING * 2;
    uint h = si->Height + ATLAS_SPRITES_PADDING * 2;
    for (auto it = allAtlases.begin(), end = allAtlases.end(); it != end; ++it)
    {
        TextureAtlas* a = *it;
        if (a->Type != atlas_type)
            continue;

        if (a->RootNode)
        {
            if (a->RootNode->FindPosition(w, h, x, y))
                atlas = a;
        }
        else
        {
            if (w <= a->LineW && a->LineCurH + h <= a->LineMaxH)
            {
                x = a->CurX - a->LineW;
                y = a->CurY + a->LineCurH;
                a->LineCurH += h;
                atlas = a;
            }
            else if (a->Width - a->CurX >= w && a->Height - a->CurY >= h)
            {
                x = a->CurX;
                y = a->CurY;
                a->CurX += w;
                a->LineW = w;
                a->LineCurH = h;
                a->LineMaxH = MAX(a->LineMaxH, h);
                atlas = a;
            }
            else if (a->Width >= w && a->Height - a->CurY - a->LineMaxH >= h)
            {
                x = 0;
                y = a->CurY + a->LineMaxH;
                a->CurX = w;
                a->CurY = y;
                a->LineW = w;
                a->LineCurH = h;
                a->LineMaxH = h;
                atlas = a;
            }
        }

        if (atlas)
            break;
    }

    // Create new
    if (!atlas)
    {
        atlas = CreateAtlas(w, h);
        if (atlas->RootNode)
        {
            atlas->RootNode->FindPosition(w, h, x, y);
        }
        else
        {
            x = atlas->CurX;
            y = atlas->CurY;
            atlas->CurX += w;
            atlas->LineMaxH = h;
            atlas->LineCurH = h;
            atlas->LineW = w;
        }
    }

    // Return parameters
    x += ATLAS_SPRITES_PADDING;
    y += ATLAS_SPRITES_PADDING;
    return atlas;
}

void SpriteManager::DestroyAtlases(AtlasType atlas_type)
{
    for (auto it = allAtlases.begin(); it != allAtlases.end();)
    {
        TextureAtlas* atlas = *it;
        if (atlas->Type == atlas_type)
        {
            for (auto it_ = sprData.begin(), end_ = sprData.end(); it_ != end_; ++it_)
            {
                SpriteInfo* si = *it_;
                if (si && si->Atlas == atlas)
                {
                    if (si->Anim3d)
                        si->Anim3d->SprId = 0;

                    delete si;
                    (*it_) = nullptr;
                }
            }

            DeleteRenderTarget(atlas->RT);
            delete atlas;
            it = allAtlases.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SpriteManager::DumpAtlases()
{
    uint atlases_memory_size = 0;
    for (auto it = allAtlases.begin(), end = allAtlases.end(); it != end; ++it)
    {
        TextureAtlas* atlas = *it;
        atlases_memory_size += atlas->Width * atlas->Height * 4;
    }

    string path = _str(
        "./{}_{}.{:03}mb/", (uint)time(nullptr), atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    int cnt = 0;
    for (auto it = allAtlases.begin(), end = allAtlases.end(); it != end; ++it)
    {
        TextureAtlas* atlas = *it;
        string fname = _str("{}{}_{}_{}x{}.png", path, cnt, atlas->Type, atlas->Width, atlas->Height);
        SaveTexture(atlas->TextureOwner, fname, false);
        cnt++;
    }
}

static void SavePNG(FileManager& file_mngr, const string& fname, uchar* data, uint width, uint height)
{
#if defined(FO_WINDOWS) || defined(FO_LINUX) || defined(FO_MAC)
    // Initialize stuff
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr)
        return;
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        return;
    if (setjmp(png_jmpbuf(png_ptr)))
        return;

    static UCharVec result_png;
    struct PNGWriter
    {
        static void Write(png_structp png_ptr, png_bytep png_data, png_size_t length)
        {
            UNUSED_VARIABLE(png_ptr);
            for (png_size_t i = 0; i < length; i++)
                result_png.push_back(png_data[i]);
        }
        static void Flush(png_structp png_ptr) { UNUSED_VARIABLE(png_ptr); }
    };
    result_png.clear();
    png_set_write_fn(png_ptr, nullptr, &PNGWriter::Write, &PNGWriter::Flush);

    // Write header
    if (setjmp(png_jmpbuf(png_ptr)))
        return;
    png_byte color_type = PNG_COLOR_TYPE_RGB_ALPHA;
    png_byte bit_depth = 8;
    png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);

    // Write pointers
    uchar** row_pointers = new uchar*[height];
    for (uint y = 0; y < height; y++)
        row_pointers[y] = &data[y * width * 4];

    // Write bytes
    if (setjmp(png_jmpbuf(png_ptr)))
        return;
    png_write_image(png_ptr, row_pointers);

    // End write
    if (setjmp(png_jmpbuf(png_ptr)))
        return;
    png_write_end(png_ptr, nullptr);

    // Clean up
    delete[] row_pointers;

    // Write to disk
    OutputFile file = file_mngr.WriteFile(fname);
    file.SetData(&result_png[0], (uint)result_png.size());
    file.Save();

#else
    UNUSED_VARIABLE(fname);
    UNUSED_VARIABLE(data);
    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);
#endif
}

void SpriteManager::SaveTexture(Texture* tex, const string& fname, bool flip)
{
    // Size
    int w = (tex ? tex->Width : 0);
    int h = (tex ? tex->Height : 0);
    if (!tex)
        GetWindowSize(w, h);

    // Get data
    uint size = w * h * 4;
    uchar* data = new uchar[size];
    if (tex)
    {
        GL(glBindTexture(GL_TEXTURE_2D, tex->Id));
        GL(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
        GL(glBindTexture(GL_TEXTURE_2D, 0));
    }
    else
    {
        GL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data));
    }

    // Flip image
    if (flip)
    {
        uint* data4 = (uint*)data;
        for (int y = 0; y < h / 2; y++)
            for (int x = 0; x < w; x++)
                std::swap(data4[y * w + x], data4[(h - y - 1) * w + x]);
    }

    // Save
    SavePNG(fileMngr, fname, data, w, h);

    // Clean up
    SAFEDELA(data);
}

uint SpriteManager::RequestFillAtlas(SpriteInfo* si, uint w, uint h, uchar* data)
{
    // Sprite info
    if (!si)
        si = new SpriteInfo();

    // Get width, height
    si->Data = data;
    si->DataAtlasType = std::get<0>(atlasStack.back());
    si->DataAtlasOneImage = std::get<1>(atlasStack.back());
    si->Width = w;
    si->Height = h;

    // Find place on atlas
    if (accumulatorActive)
        accumulatorSprInfo.push_back(si);
    else
        FillAtlas(si);

    // Store sprite
    uint index = 1;
    for (uint j = (uint)sprData.size(); index < j; index++)
        if (!sprData[index])
            break;
    if (index < (uint)sprData.size())
        sprData[index] = si;
    else
        sprData.push_back(si);
    return index;
}

void SpriteManager::FillAtlas(SpriteInfo* si)
{
    uchar* data = si->Data;
    uint w = si->Width;
    uint h = si->Height;

    si->Data = nullptr;

    PushAtlasType(si->DataAtlasType, si->DataAtlasOneImage);
    int x, y;
    TextureAtlas* atlas = FindAtlasPlace(si, x, y);
    PopAtlasType();

    // Refresh texture
    if (data)
    {
        // Whole image
        Texture* tex = atlas->TextureOwner;
        tex->UpdateRegion(Rect(x, y, x + w - 1, y + h - 1), data);

        // 1px border for correct linear interpolation
        // Top
        tex->UpdateRegion(Rect(x, y - 1, x + w - 1, y - 1), data);

        // Bottom
        tex->UpdateRegion(Rect(x, y + h, x + w - 1, y + h), data + (h - 1) * w * 4);

        // Left
        uint data_border[MAX_ATLAS_SIZE];
        for (uint i = 0; i < h; i++)
            data_border[i + 1] = *(uint*)(data + i * w * 4);
        data_border[0] = data_border[1];
        data_border[h + 1] = data_border[h];
        tex->UpdateRegion(Rect(x - 1, y - 1, x - 1, y + h), (uchar*)data_border);

        // Right
        for (uint i = 0; i < h; i++)
            data_border[i + 1] = *(uint*)(data + i * w * 4 + (w - 1) * 4);
        data_border[0] = data_border[1];
        data_border[h + 1] = data_border[h];
        tex->UpdateRegion(Rect(x + w, y - 1, x + w, y + h), (uchar*)data_border);

        // Invalidate last pixel color picking
        if (atlas->RT->LastPixelPicks && !atlas->RT->LastPixelPicks->empty())
            atlas->RT->LastPixelPicks->clear();
    }

    // Set parameters
    si->Atlas = atlas;
    si->SprRect.L = float(x) / float(atlas->Width);
    si->SprRect.T = float(y) / float(atlas->Height);
    si->SprRect.R = float(x + w) / float(atlas->Width);
    si->SprRect.B = float(y + h) / float(atlas->Height);

    // Delete data
    SAFEDELA(data);
}

AnyFrames* SpriteManager::LoadAnimation(const string& fname, bool use_dummy, bool frm_anim_pix)
{
    AnyFrames* dummy = (use_dummy ? DummyAnimation : nullptr);

    if (fname.empty())
        return dummy;

    string ext = _str(fname).getFileExtension();
    if (ext.empty())
    {
        WriteLog("Extension not found, file '{}'.\n", fname);
        return dummy;
    }

    AnyFrames* result = nullptr;
    if (Is3dExtensionSupported(ext))
        result = LoadAnimation3d(fname);
    else
        result = LoadAnimation2d(fname);

    return result ? result : dummy;
}

AnyFrames* SpriteManager::LoadAnimation2d(const string& fname)
{
    File file = fileMngr.ReadFile(fname);
    if (!file)
        return nullptr;

    RUNTIME_ASSERT(file.GetUChar() == 42);
    ushort frames_count = file.GetBEUShort();
    uint ticks = file.GetBEUInt();
    ushort dirs = file.GetBEUShort();

    AnyFrames* anim = CreateAnyFrames(frames_count, ticks);
    if (dirs > 1)
        CreateAnyFramesDirAnims(anim, dirs);

    for (ushort dir = 0; dir < dirs; dir++)
    {
        AnyFrames* dir_anim = anim->GetDir(dir);
        short ox = file.GetBEShort();
        short oy = file.GetBEShort();
        for (ushort i = 0; i < frames_count; i++)
        {
            if (!file.GetUChar())
            {
                SpriteInfo* si = new SpriteInfo();
                si->OffsX = ox;
                si->OffsY = oy;
                ushort w = file.GetBEUShort();
                ushort h = file.GetBEUShort();
                dir_anim->NextX[i] = file.GetBEShort();
                dir_anim->NextY[i] = file.GetBEShort();
                uchar* data = file.GetCurBuf();
                dir_anim->Ind[i] = RequestFillAtlas(si, w, h, data);
                file.GoForward(w * h * 4);
            }
            else
            {
                ushort index = file.GetBEUShort();
                dir_anim->Ind[i] = dir_anim->Ind[index];
            }
        }
    }

    RUNTIME_ASSERT(file.GetUChar() == 42);
    return anim;
}

AnyFrames* SpriteManager::ReloadAnimation(AnyFrames* anim, const string& fname)
{
    if (fname.empty())
        return anim;

    // Release old images
    if (anim)
    {
        for (uint i = 0; i < anim->CntFrm; i++)
        {
            SpriteInfo* si = GetSpriteInfo(anim->Ind[i]);
            if (si)
                DestroyAtlases(si->Atlas->Type);
        }
        DestroyAnyFrames(anim);
    }

    // Load fresh
    return LoadAnimation(fname);
}

AnyFrames* SpriteManager::LoadAnimation3d(const string& fname)
{
    if (!settings.Enable3dRendering)
        return nullptr;

    // Load 3d animation
    RUNTIME_ASSERT(anim3dMngr);
    Animation3d* anim3d = anim3dMngr->GetAnimation(fname, false);
    if (!anim3d)
        return nullptr;
    anim3d->StartMeshGeneration();

    // Get animation data
    float period;
    int proc_from, proc_to;
    int dir;
    anim3d->GetRenderFramesData(period, proc_from, proc_to, dir);

    // Set fir
    if (dir < 0)
        anim3d->SetDirAngle(-dir);
    else
        anim3d->SetDir(dir);

    // If no animations available than render just one
    if (period == 0.0f || proc_from == proc_to)
    {
    label_LoadOneSpr:
        anim3d->SetAnimation(0, proc_from * 10, nullptr, ANIMATION_ONE_TIME | ANIMATION_STAY);
        Render3d(anim3d);

        AnyFrames* anim = CreateAnyFrames(1, 100);
        anim->Ind[0] = anim3d->SprId;

        SAFEDEL(anim3d);
        return anim;
    }

    // Calculate need information
    float frame_time = 1.0f / (float)(settings.Animation3dFPS ? settings.Animation3dFPS : 10); // 1 second / fps
    float period_from = period * (float)proc_from / 100.0f;
    float period_to = period * (float)proc_to / 100.0f;
    float period_len = fabs(period_to - period_from);
    float proc_step = (float)(proc_to - proc_from) / (period_len / frame_time);
    int frames_count = (int)ceil(period_len / frame_time);

    if (frames_count <= 1)
        goto label_LoadOneSpr;

    AnyFrames* anim = CreateAnyFrames(frames_count, (uint)(period_len * 1000.0f));

    float cur_proc = (float)proc_from;
    int prev_cur_proci = -1;
    for (int i = 0; i < frames_count; i++)
    {
        int cur_proci = (proc_to > proc_from ? (int)(10.0f * cur_proc + 0.5) : (int)(10.0f * cur_proc));

        // Previous frame is different
        if (cur_proci != prev_cur_proci)
        {
            anim3d->SetAnimation(0, cur_proci, nullptr, ANIMATION_ONE_TIME | ANIMATION_STAY);
            Render3d(anim3d);

            anim->Ind[i] = anim3d->SprId;
        }
        // Previous frame is same
        else
        {
            anim->Ind[i] = anim->Ind[i - 1];
        }

        cur_proc += proc_step;
        prev_cur_proci = cur_proci;
    }

    SAFEDEL(anim3d);
    return anim;
}

bool SpriteManager::Render3d(Animation3d* anim3d)
{
    // Find place for render
    if (!anim3d->SprId)
        RefreshPure3dAnimationSprite(anim3d);

    // Set render target
    SpriteInfo* si = sprData[anim3d->SprId];
    RenderTarget* rt = Get3dRenderTarget(si->Width, si->Height);
    PushRenderTarget(rt);
    ClearCurrentRenderTarget(0);
    ClearCurrentRenderTargetDepth();
    RUNTIME_ASSERT(anim3dMngr);
    anim3dMngr->SetScreenSize(rt->TargetTexture->Width, rt->TargetTexture->Height);

    // Draw model
    anim3d->Draw(0, 0);

    // Restore render target
    PopRenderTarget();

    // Copy render
    Rect region_to((int)(si->SprRect.L * (float)si->Atlas->Width + 0.5f),
        (int)((1.0f - si->SprRect.T) * (float)si->Atlas->Height + 0.5f),
        (int)(si->SprRect.R * (float)si->Atlas->Width + 0.5f),
        (int)((1.0f - si->SprRect.B) * (float)si->Atlas->Height + 0.5f));
    PushRenderTarget(si->Atlas->RT);
    DrawRenderTarget(rt, false, nullptr, &region_to);
    PopRenderTarget();

    return true;
}

bool SpriteManager::Draw3d(int x, int y, Animation3d* anim3d, uint color)
{
    if (!settings.Enable3dRendering)
        return false;

    anim3d->StartMeshGeneration();
    Render3d(anim3d);

    SpriteInfo* si = sprData[anim3d->SprId];
    DrawSprite(anim3d->SprId, x - si->Width / 2 + si->OffsX, y - si->Height + si->OffsY, color);

    return true;
}

Animation3d* SpriteManager::LoadPure3dAnimation(const string& fname, bool auto_redraw)
{
    if (!settings.Enable3dRendering)
        return nullptr;

    // Fill data
    RUNTIME_ASSERT(anim3dMngr);
    Animation3d* anim3d = anim3dMngr->GetAnimation(fname, false);
    if (!anim3d)
        return nullptr;

    // Create render sprite
    anim3d->SprId = 0;
    anim3d->SprAtlasType = std::get<0>(atlasStack.back());
    if (auto_redraw)
    {
        RefreshPure3dAnimationSprite(anim3d);
        autoRedrawAnim3d.push_back(anim3d);
    }
    return anim3d;
}

void SpriteManager::RefreshPure3dAnimationSprite(Animation3d* anim3d)
{
    // Free old place
    if (anim3d->SprId)
    {
        sprData[anim3d->SprId]->Anim3d = nullptr;
        anim3d->SprId = 0;
    }

    // Render size
    uint draw_width, draw_height;
    anim3d->GetDrawSize(draw_width, draw_height);

    // Find already created place for rendering
    uint index = 0;
    for (size_t i = 0, j = sprData.size(); i < j; i++)
    {
        SpriteInfo* si = sprData[i];
        if (si && si->UsedForAnim3d && !si->Anim3d && (uint)si->Width == draw_width &&
            (uint)si->Height == draw_height && si->Atlas->Type == anim3d->SprAtlasType)
        {
            index = (uint)i;
            break;
        }
    }

    // Create new place for rendering
    if (!index)
    {
        PushAtlasType(anim3d->SprAtlasType);
        index = RequestFillAtlas(nullptr, draw_width, draw_height, nullptr);
        PopAtlasType();
        SpriteInfo* si = sprData[index];
        si->OffsY = draw_height / 4;
        si->UsedForAnim3d = true;
    }

    // Cross links
    anim3d->SprId = index;
    sprData[index]->Anim3d = anim3d;
}

void SpriteManager::FreePure3dAnimation(Animation3d* anim3d)
{
    if (anim3d)
    {
        auto it = std::find(autoRedrawAnim3d.begin(), autoRedrawAnim3d.end(), anim3d);
        if (it != autoRedrawAnim3d.end())
            autoRedrawAnim3d.erase(it);

        if (anim3d->SprId)
            sprData[anim3d->SprId]->Anim3d = nullptr;

        SAFEDEL(anim3d);
    }
}

AnyFrames* SpriteManager::CreateAnyFrames(uint frames, uint ticks)
{
    AnyFrames* anim = (AnyFrames*)anyFramesPool.Get();
    memzero(anim, sizeof(AnyFrames));
    anim->CntFrm = MIN(frames, MAX_FRAMES);
    anim->Ticks = (ticks ? ticks : frames * 100);
    return anim;
}

void SpriteManager::CreateAnyFramesDirAnims(AnyFrames* anim, uint dirs)
{
    RUNTIME_ASSERT(dirs > 1);
    RUNTIME_ASSERT((dirs == 6 || dirs == 8));
    anim->DirCount = dirs;
    for (uint dir = 0; dir < dirs - 1; dir++)
        anim->Dirs[dir] = CreateAnyFrames(anim->CntFrm, anim->Ticks);
}

void SpriteManager::DestroyAnyFrames(AnyFrames* anim)
{
    if (!anim || anim == DummyAnimation)
        return;

    for (int dir = 1; dir < anim->DirCount; dir++)
        anyFramesPool.Put(anim->GetDir(dir));
    anyFramesPool.Put(anim);
}

bool SpriteManager::Flush()
{
    if (!curDrawQuad)
        return true;

    EnableVertexArray(quadsVertexArray, 4 * curDrawQuad);
    EnableScissor();

    uint pos = 0;
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
    DisableScissor();

    return true;
}

bool SpriteManager::DrawSprite(uint id, int x, int y, uint color)
{
    if (!id)
        return false;

    SpriteInfo* si = sprData[id];
    if (!si)
        return false;

    Effect* effect = (si->DrawEffect ? si->DrawEffect : effectMngr.Effects.Iface);
    if (dipQueue.empty() || dipQueue.back().SourceTexture != si->Atlas->TextureOwner ||
        dipQueue.back().SourceEffect != effect)
        dipQueue.push_back({si->Atlas->TextureOwner, effect, 1});
    else
        dipQueue.back().SpritesCount++;

    int pos = curDrawQuad * 4;

    if (!color)
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB(color);

    vBuffer[pos].X = (float)x;
    vBuffer[pos].Y = (float)y + si->Height;
    vBuffer[pos].TU = si->SprRect.L;
    vBuffer[pos].TV = si->SprRect.B;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = (float)x;
    vBuffer[pos].Y = (float)y;
    vBuffer[pos].TU = si->SprRect.L;
    vBuffer[pos].TV = si->SprRect.T;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = (float)x + si->Width;
    vBuffer[pos].Y = (float)y;
    vBuffer[pos].TU = si->SprRect.R;
    vBuffer[pos].TV = si->SprRect.T;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = (float)x + si->Width;
    vBuffer[pos].Y = (float)y + si->Height;
    vBuffer[pos].TU = si->SprRect.R;
    vBuffer[pos].TV = si->SprRect.B;
    vBuffer[pos].Diffuse = color;

    if (++curDrawQuad == drawQuadCount)
        Flush();
    return true;
}

bool SpriteManager::DrawSprite(AnyFrames* frames, int x, int y, uint color)
{
    if (frames && frames != DummyAnimation)
        return DrawSprite(frames->GetCurSprId(), x, y, color);
    return false;
}

bool SpriteManager::DrawSpriteSize(uint id, int x, int y, int w, int h, bool zoom_up, bool center, uint color)
{
    return DrawSpriteSizeExt(id, x, y, w, h, zoom_up, center, false, color);
}

bool SpriteManager::DrawSpriteSize(AnyFrames* frames, int x, int y, int w, int h, bool zoom_up, bool center, uint color)
{
    if (frames && frames != DummyAnimation)
        return DrawSpriteSize(frames->GetCurSprId(), x, y, w, h, zoom_up, center, color);
    return false;
}

bool SpriteManager::DrawSpriteSizeExt(
    uint id, int x, int y, int w, int h, bool zoom_up, bool center, bool stretch, uint color)
{
    if (!id)
        return false;

    SpriteInfo* si = sprData[id];
    if (!si)
        return false;

    float xf = (float)x;
    float yf = (float)y;
    float wf = (float)si->Width;
    float hf = (float)si->Height;
    float k = MIN(w / wf, h / hf);
    if (!stretch)
    {
        if (k < 1.0f || (k > 1.0f && zoom_up))
        {
            wf = floorf(wf * k + 0.5f);
            hf = floorf(hf * k + 0.5f);
        }
        if (center)
        {
            xf += floorf(((float)w - wf) / 2.0f + 0.5f);
            yf += floorf(((float)h - hf) / 2.0f + 0.5f);
        }
    }
    else if (zoom_up)
    {
        wf = floorf(wf * k + 0.5f);
        hf = floorf(hf * k + 0.5f);
        if (center)
        {
            xf += floorf(((float)w - wf) / 2.0f + 0.5f);
            yf += floorf(((float)h - hf) / 2.0f + 0.5f);
        }
    }
    else
    {
        wf = (float)w;
        hf = (float)h;
    }

    Effect* effect = (si->DrawEffect ? si->DrawEffect : effectMngr.Effects.Iface);
    if (dipQueue.empty() || dipQueue.back().SourceTexture != si->Atlas->TextureOwner ||
        dipQueue.back().SourceEffect != effect)
        dipQueue.push_back({si->Atlas->TextureOwner, effect, 1});
    else
        dipQueue.back().SpritesCount++;

    int pos = curDrawQuad * 4;

    if (!color)
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB(color);

    vBuffer[pos].X = xf;
    vBuffer[pos].Y = yf + hf;
    vBuffer[pos].TU = si->SprRect.L;
    vBuffer[pos].TV = si->SprRect.B;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = xf;
    vBuffer[pos].Y = yf;
    vBuffer[pos].TU = si->SprRect.L;
    vBuffer[pos].TV = si->SprRect.T;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = xf + wf;
    vBuffer[pos].Y = yf;
    vBuffer[pos].TU = si->SprRect.R;
    vBuffer[pos].TV = si->SprRect.T;
    vBuffer[pos++].Diffuse = color;

    vBuffer[pos].X = xf + wf;
    vBuffer[pos].Y = yf + hf;
    vBuffer[pos].TU = si->SprRect.R;
    vBuffer[pos].TV = si->SprRect.B;
    vBuffer[pos].Diffuse = color;

    if (++curDrawQuad == drawQuadCount)
        Flush();
    return true;
}

bool SpriteManager::DrawSpritePattern(uint id, int x, int y, int w, int h, int spr_width, int spr_height, uint color)
{
    if (!id)
        return false;

    if (!w || !h)
        return false;

    SpriteInfo* si = sprData[id];
    if (!si)
        return false;

    float width = (float)si->Width;
    float height = (float)si->Height;

    if (spr_width && spr_height)
    {
        width = (float)spr_width;
        height = (float)spr_height;
    }
    else if (spr_width)
    {
        float ratio = (float)spr_width / width;
        width = (float)spr_width;
        height *= ratio;
    }
    else if (spr_height)
    {
        float ratio = (float)spr_height / height;
        height = (float)spr_height;
        width *= ratio;
    }

    if (!color)
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB(color);

    Effect* effect = (si->DrawEffect ? si->DrawEffect : effectMngr.Effects.Iface);

    float last_right_offs = (si->SprRect.R - si->SprRect.L) / width;
    float last_bottom_offs = (si->SprRect.B - si->SprRect.T) / height;

    for (float yy = (float)y, end_y = (float)y + h; yy < end_y; yy += height)
    {
        bool last_y = yy + height >= end_y;
        for (float xx = (float)x, end_x = (float)x + w; xx < end_x; xx += width)
        {
            bool last_x = xx + width >= end_x;

            if (dipQueue.empty() || dipQueue.back().SourceTexture != si->Atlas->TextureOwner ||
                dipQueue.back().SourceEffect != effect)
                dipQueue.push_back({si->Atlas->TextureOwner, effect, 1});
            else
                dipQueue.back().SpritesCount++;

            int pos = curDrawQuad * 4;

            float local_width = last_x ? (end_x - xx) : width;
            float local_height = last_y ? (end_y - yy) : height;
            float local_right = last_x ? si->SprRect.L + last_right_offs * local_width : si->SprRect.R;
            float local_bottom = last_y ? si->SprRect.T + last_bottom_offs * local_height : si->SprRect.B;

            vBuffer[pos].X = xx;
            vBuffer[pos].Y = yy + local_height;
            vBuffer[pos].TU = si->SprRect.L;
            vBuffer[pos].TV = local_bottom;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = xx;
            vBuffer[pos].Y = yy;
            vBuffer[pos].TU = si->SprRect.L;
            vBuffer[pos].TV = si->SprRect.T;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = xx + local_width;
            vBuffer[pos].Y = yy;
            vBuffer[pos].TU = local_right;
            vBuffer[pos].TV = si->SprRect.T;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = xx + local_width;
            vBuffer[pos].Y = yy + local_height;
            vBuffer[pos].TU = local_right;
            vBuffer[pos].TV = local_bottom;
            vBuffer[pos].Diffuse = color;

            if (++curDrawQuad == drawQuadCount)
                Flush();
        }
    }

    DisableScissor();
    return true;
}

void SpriteManager::PrepareSquare(PointVec& points, Rect r, uint color)
{
    points.push_back({r.L, r.T, color});
    points.push_back({r.R, r.B, color});
    points.push_back({r.L, r.T, color});
    points.push_back({r.R, r.T, color});
    points.push_back({r.R, r.B, color});
}

void SpriteManager::PrepareSquare(PointVec& points, Point lt, Point rt, Point lb, Point rb, uint color)
{
    points.push_back({lb.X, lb.Y, color});
    points.push_back({lt.X, lt.Y, color});
    points.push_back({rb.X, rb.Y, color});
    points.push_back({lt.X, lt.Y, color});
    points.push_back({rt.X, rt.Y, color});
    points.push_back({rb.X, rb.Y, color});
}

uint SpriteManager::PackColor(int r, int g, int b, int a)
{
    r = CLAMP(r, 0, 255);
    g = CLAMP(g, 0, 255);
    b = CLAMP(b, 0, 255);
    return COLOR_RGBA(a, r, g, b);
}

void SpriteManager::GetDrawRect(Sprite* prep, Rect& rect)
{
    uint id = (prep->PSprId ? *prep->PSprId : prep->SprId);
    if (id >= sprData.size())
        return;
    SpriteInfo* si = sprData[id];
    if (!si)
        return;

    int x = prep->ScrX - si->Width / 2 + si->OffsX + *prep->PScrX;
    int y = prep->ScrY - si->Height + si->OffsY + *prep->PScrY;
    if (prep->OffsX)
        x += *prep->OffsX;
    if (prep->OffsY)
        y += *prep->OffsY;
    rect.L = x;
    rect.T = y;
    rect.R = x + si->Width;
    rect.B = y + si->Height;
}

void SpriteManager::InitializeEgg(const string& egg_name)
{
    eggValid = false;
    eggHx = eggHy = eggX = eggY = 0;
    AnyFrames* egg_frames = LoadAnimation(egg_name);
    if (egg_frames)
    {
        sprEgg = GetSpriteInfo(egg_frames->Ind[0]);
        DestroyAnyFrames(egg_frames);
        eggSprWidth = sprEgg->Width;
        eggSprHeight = sprEgg->Height;
        eggAtlasWidth = (float)atlasWidth;
        eggAtlasHeight = (float)atlasHeight;

        PushRenderTarget(sprEgg->Atlas->RT);
        SAFEDELA(eggData);
        eggData = new uint[eggSprWidth * eggSprHeight];
        int x = (int)(sprEgg->Atlas->TextureOwner->SizeData[0] * sprEgg->SprRect.L);
        int y = (int)(sprEgg->Atlas->TextureOwner->SizeData[1] * sprEgg->SprRect.T);
        GL(glReadPixels(x, y, eggSprWidth, eggSprHeight, GL_RGBA, GL_UNSIGNED_BYTE, eggData));
        PopRenderTarget();
    }
    else
    {
        WriteLog("Load sprite '{}' fail. Egg disabled.\n", egg_name);
    }
}

bool SpriteManager::CompareHexEgg(ushort hx, ushort hy, int egg_type)
{
    if (egg_type == EGG_ALWAYS)
        return true;
    if (eggHy == hy && hx & 1 && !(eggHx & 1))
        hy--;
    switch (egg_type)
    {
    case EGG_X:
        if (hx >= eggHx)
            return true;
        break;
    case EGG_Y:
        if (hy >= eggHy)
            return true;
        break;
    case EGG_X_AND_Y:
        if (hx >= eggHx || hy >= eggHy)
            return true;
        break;
    case EGG_X_OR_Y:
        if (hx >= eggHx && hy >= eggHy)
            return true;
        break;
    default:
        break;
    }
    return false;
}

void SpriteManager::SetEgg(ushort hx, ushort hy, Sprite* spr)
{
    if (!sprEgg)
        return;

    uint id = (spr->PSprId ? *spr->PSprId : spr->SprId);
    SpriteInfo* si = sprData[id];
    if (!si)
        return;

    eggX = spr->ScrX + si->OffsX - sprEgg->Width / 2 + *spr->OffsX + *spr->PScrX;
    eggY = spr->ScrY - si->Height / 2 + si->OffsY - sprEgg->Height / 2 + *spr->OffsY + *spr->PScrY;
    eggHx = hx;
    eggHy = hy;
    eggValid = true;
}

bool SpriteManager::DrawSprites(Sprites& dtree, bool collect_contours, bool use_egg, int draw_oder_from,
    int draw_oder_to, bool prerender, int prerender_ox, int prerender_oy)
{
    if (!dtree.Size())
        return true;

    PointVec borders;

    if (!eggValid)
        use_egg = false;
    int ex = eggX + settings.ScrOx;
    int ey = eggY + settings.ScrOy;
    uint cur_tick = Timer::FastTick();

    for (Sprite* spr = dtree.RootSprite(); spr; spr = spr->ChainChild)
    {
        RUNTIME_ASSERT(spr->Valid);

        if (spr->DrawOrderType < draw_oder_from)
            continue;
        if (spr->DrawOrderType > draw_oder_to)
            break;

        uint id = (spr->PSprId ? *spr->PSprId : spr->SprId);
        SpriteInfo* si = sprData[id];
        if (!si)
            continue;

        // Coords
        int x = spr->ScrX - si->Width / 2 + si->OffsX + *spr->PScrX;
        int y = spr->ScrY - si->Height + si->OffsY + *spr->PScrY;
        if (!prerender)
        {
            x += settings.ScrOx;
            y += settings.ScrOy;
        }
        if (spr->OffsX)
            x += *spr->OffsX;
        if (spr->OffsY)
            y += *spr->OffsY;
        float zoom = settings.SpritesZoom;

        // Base color
        uint color_r, color_l;
        if (spr->Color)
            color_r = color_l = (spr->Color | 0xFF000000);
        else
            color_r = color_l = baseColor;

        // Light
        if (spr->Light)
        {
            static auto light_func = [](uint& c, uchar* l, uchar* l2) {
                int lr = *l;
                int lg = *(l + 1);
                int lb = *(l + 2);
                int lr2 = *l2;
                int lg2 = *(l2 + 1);
                int lb2 = *(l2 + 2);
                uchar& r = ((uchar*)&c)[2];
                uchar& g = ((uchar*)&c)[1];
                uchar& b = ((uchar*)&c)[0];
                int ir = (int)r + (lr + lr2) / 2;
                int ig = (int)g + (lg + lg2) / 2;
                int ib = (int)b + (lb + lb2) / 2;
                r = MIN(ir, 255);
                g = MIN(ig, 255);
                b = MIN(ib, 255);
            };
            light_func(color_r, spr->Light, spr->LightRight);
            light_func(color_l, spr->Light, spr->LightLeft);
        }

        // Alpha
        if (spr->Alpha)
        {
            ((uchar*)&color_r)[3] = *spr->Alpha;
            ((uchar*)&color_l)[3] = *spr->Alpha;
        }

        // Process flashing
        if (spr->FlashMask)
        {
            static int cnt = 0;
            static uint tick = cur_tick + 100;
            static bool add = true;
            if (cur_tick >= tick)
            {
                cnt += (add ? 10 : -10);
                if (cnt > 40)
                {
                    cnt = 40;
                    add = false;
                }
                else if (cnt < -40)
                {
                    cnt = -40;
                    add = true;
                }
                tick = cur_tick + 100;
            }
            static auto flash_func = [](uint& c, int cnt, uint mask) {
                int r = ((c >> 16) & 0xFF) + cnt;
                int g = ((c >> 8) & 0xFF) + cnt;
                int b = (c & 0xFF) + cnt;
                r = CLAMP(r, 0, 0xFF);
                g = CLAMP(g, 0, 0xFF);
                b = CLAMP(b, 0, 0xFF);
                ((uchar*)&c)[2] = r;
                ((uchar*)&c)[1] = g;
                ((uchar*)&c)[0] = b;
                c &= mask;
            };
            flash_func(color_r, cnt, spr->FlashMask);
            flash_func(color_l, cnt, spr->FlashMask);
        }

        // Fix color
        color_r = COLOR_SWAP_RB(color_r);
        color_l = COLOR_SWAP_RB(color_l);

        // Check borders
        if (!prerender)
        {
            if (x / zoom > settings.ScreenWidth || (x + si->Width) / zoom < 0 || y / zoom > settings.ScreenHeight ||
                (y + si->Height) / zoom < 0)
                continue;
        }

        // Egg process
        bool egg_added = false;
        if (use_egg && spr->EggType && CompareHexEgg(spr->HexX, spr->HexY, spr->EggType))
        {
            int x1 = x - ex;
            int y1 = y - ey;
            int x2 = x1 + si->Width;
            int y2 = y1 + si->Height;

            if (spr->CutType)
            {
                x1 += (int)spr->CutX;
                x2 = x1 + (int)spr->CutW;
            }

            if (!(x1 >= eggSprWidth || y1 >= eggSprHeight || x2 < 0 || y2 < 0))
            {
                x1 = MAX(x1, 0);
                y1 = MAX(y1, 0);
                x2 = MIN(x2, eggSprWidth);
                y2 = MIN(y2, eggSprHeight);

                float x1f = (float)(x1 + ATLAS_SPRITES_PADDING);
                float x2f = (float)(x2 + ATLAS_SPRITES_PADDING);
                float y1f = (float)(y1 + ATLAS_SPRITES_PADDING);
                float y2f = (float)(y2 + ATLAS_SPRITES_PADDING);

                int pos = curDrawQuad * 4;
                vBuffer[pos + 0].TUEgg = x1f / eggAtlasWidth;
                vBuffer[pos + 0].TVEgg = y2f / eggAtlasHeight;
                vBuffer[pos + 1].TUEgg = x1f / eggAtlasWidth;
                vBuffer[pos + 1].TVEgg = y1f / eggAtlasHeight;
                vBuffer[pos + 2].TUEgg = x2f / eggAtlasWidth;
                vBuffer[pos + 2].TVEgg = y1f / eggAtlasHeight;
                vBuffer[pos + 3].TUEgg = x2f / eggAtlasWidth;
                vBuffer[pos + 3].TVEgg = y2f / eggAtlasHeight;

                egg_added = true;
            }
        }

        // Choose effect
        Effect* effect = (spr->DrawEffect ? *spr->DrawEffect : nullptr);
        if (!effect)
            effect = (si->DrawEffect ? si->DrawEffect : effectMngr.Effects.Generic);

        // Choose atlas
        if (dipQueue.empty() || dipQueue.back().SourceTexture != si->Atlas->TextureOwner ||
            dipQueue.back().SourceEffect != effect)
            dipQueue.push_back({si->Atlas->TextureOwner, effect, 1});
        else
            dipQueue.back().SpritesCount++;

        // Casts
        float xf = (float)x / zoom + (prerender ? (float)prerender_ox : 0.0f);
        float yf = (float)y / zoom + (prerender ? (float)prerender_oy : 0.0f);
        float wf = (float)si->Width / zoom;
        float hf = (float)si->Height / zoom;

        // Fill buffer
        int pos = curDrawQuad * 4;

        vBuffer[pos].X = xf;
        vBuffer[pos].Y = yf + hf;
        vBuffer[pos].TU = si->SprRect.L;
        vBuffer[pos].TV = si->SprRect.B;
        vBuffer[pos++].Diffuse = color_l;

        vBuffer[pos].X = xf;
        vBuffer[pos].Y = yf;
        vBuffer[pos].TU = si->SprRect.L;
        vBuffer[pos].TV = si->SprRect.T;
        vBuffer[pos++].Diffuse = color_l;

        vBuffer[pos].X = xf + wf;
        vBuffer[pos].Y = yf;
        vBuffer[pos].TU = si->SprRect.R;
        vBuffer[pos].TV = si->SprRect.T;
        vBuffer[pos++].Diffuse = color_r;

        vBuffer[pos].X = xf + wf;
        vBuffer[pos].Y = yf + hf;
        vBuffer[pos].TU = si->SprRect.R;
        vBuffer[pos].TV = si->SprRect.B;
        vBuffer[pos++].Diffuse = color_r;

        // Cutted sprite
        if (spr->CutType)
        {
            xf = (float)(x + spr->CutX) / zoom;
            wf = spr->CutW / zoom;
            vBuffer[pos - 4].X = xf;
            vBuffer[pos - 4].TU = spr->CutTexL;
            vBuffer[pos - 3].X = xf;
            vBuffer[pos - 3].TU = spr->CutTexL;
            vBuffer[pos - 2].X = xf + wf;
            vBuffer[pos - 2].TU = spr->CutTexR;
            vBuffer[pos - 1].X = xf + wf;
            vBuffer[pos - 1].TU = spr->CutTexR;
        }

        // Set default texture coordinates for egg texture
        if (!egg_added && vBuffer[pos - 1].TUEgg != -1.0f)
        {
            vBuffer[pos - 1].TUEgg = -1.0f;
            vBuffer[pos - 2].TUEgg = -1.0f;
            vBuffer[pos - 3].TUEgg = -1.0f;
            vBuffer[pos - 4].TUEgg = -1.0f;
        }

        // Draw
        if (++curDrawQuad == drawQuadCount)
            Flush();

#ifdef FONLINE_EDITOR
        // Corners indication
        if (settings.ShowCorners && spr->EggType)
        {
            PointVec corner;
            float cx = wf / 2.0f;

            switch (spr->EggType)
            {
            case EGG_ALWAYS:
                PrepareSquare(corner, RectF(xf + cx - 2.0f, yf + hf - 50.0f, xf + cx + 2.0f, yf + hf), 0x5FFFFF00);
                break;
            case EGG_X:
                PrepareSquare(corner, PointF(xf + cx - 5.0f, yf + hf - 55.0f), PointF(xf + cx + 5.0f, yf + hf - 45.0f),
                    PointF(xf + cx - 5.0f, yf + hf - 5.0f), PointF(xf + cx + 5.0f, yf + hf + 5.0f), 0x5F00AF00);
                break;
            case EGG_Y:
                PrepareSquare(corner, PointF(xf + cx - 5.0f, yf + hf - 49.0f), PointF(xf + cx + 5.0f, yf + hf - 52.0f),
                    PointF(xf + cx - 5.0f, yf + hf + 1.0f), PointF(xf + cx + 5.0f, yf + hf - 2.0f), 0x5F00FF00);
                break;
            case EGG_X_AND_Y:
                PrepareSquare(corner, PointF(xf + cx - 10.0f, yf + hf - 49.0f), PointF(xf + cx, yf + hf - 52.0f),
                    PointF(xf + cx - 10.0f, yf + hf + 1.0f), PointF(xf + cx, yf + hf - 2.0f), 0x5FFF0000);
                PrepareSquare(corner, PointF(xf + cx, yf + hf - 55.0f), PointF(xf + cx + 10.0f, yf + hf - 45.0f),
                    PointF(xf + cx, yf + hf - 5.0f), PointF(xf + cx + 10.0f, yf + hf + 5.0f), 0x5FFF0000);
                break;
            case EGG_X_OR_Y:
                PrepareSquare(corner, PointF(xf + cx, yf + hf - 49.0f), PointF(xf + cx + 10.0f, yf + hf - 52.0f),
                    PointF(xf + cx, yf + hf + 1.0f), PointF(xf + cx + 10.0f, yf + hf - 2.0f), 0x5FAF0000);
                PrepareSquare(corner, PointF(xf + cx - 10.0f, yf + hf - 55.0f), PointF(xf + cx, yf + hf - 45.0f),
                    PointF(xf + cx - 10.0f, yf + hf - 5.0f), PointF(xf + cx, yf + hf + 5.0f), 0x5FAF0000);
            default:
                break;
            }

            DrawPoints(corner, PRIMITIVE_TRIANGLELIST);
        }

        // Cuts
        if (settings.ShowSpriteCuts && spr->CutType)
        {
            PointVec cut;
            float z = zoom;
            float oy = (spr->CutType == SPRITE_CUT_HORIZONTAL ? 3.0f : -5.2f) / z;
            float x1 = (float)(spr->ScrX - si->Width / 2 + spr->CutX + settings.ScrOx + 1.0f) / z;
            float y1 = (float)(spr->ScrY + spr->CutOyL + settings.ScrOy) / z;
            float x2 = (float)(spr->ScrX - si->Width / 2 + spr->CutX + spr->CutW + settings.ScrOx - 1.0f) / z;
            float y2 = (float)(spr->ScrY + spr->CutOyR + settings.ScrOy) / z;
            PrepareSquare(cut, PointF(x1, y1 - 80.0f / z + oy), PointF(x2, y2 - 80.0f / z - oy), PointF(x1, y1 + oy),
                PointF(x2, y2 - oy), 0x4FFFFF00);
            PrepareSquare(cut, RectF(xf, yf, xf + 1.0f, yf + hf), 0x4F000000);
            PrepareSquare(cut, RectF(xf + wf, yf, xf + wf + 1.0f, yf + hf), 0x4F000000);
            DrawPoints(cut, PRIMITIVE_TRIANGLELIST);
        }

        // Draw order
        if (settings.ShowDrawOrder)
        {
            float z = zoom;
            int x1, y1;

            if (!spr->CutType)
            {
                x1 = (int)((spr->ScrX + settings.ScrOx) / z);
                y1 = (int)((spr->ScrY + settings.ScrOy) / z);
            }
            else
            {
                x1 = (int)((spr->ScrX - si->Width / 2 + spr->CutX + settings.ScrOx + 1.0f) / z);
                y1 = (int)((spr->ScrY + spr->CutOyL + settings.ScrOy) / z);
            }

            if (spr->DrawOrderType >= DRAW_ORDER_FLAT && spr->DrawOrderType < DRAW_ORDER)
                y1 -= (int)(40.0f / z);

            DrawStr(Rect(x1, y1, x1 + 100, y1 + 100), _str("{}", spr->TreeIndex), 0);
        }
#endif

        // Process contour effect
        if (collect_contours && spr->ContourType)
            CollectContour(x, y, si, spr);
    }

    Flush();

    if (settings.ShowSpriteBorders)
        DrawPoints(borders, PRIMITIVE_TRIANGLELIST);
    return true;
}

bool SpriteManager::IsPixNoTransp(uint spr_id, int offs_x, int offs_y, bool with_zoom)
{
    uint color = GetPixColor(spr_id, offs_x, offs_y, with_zoom);
    return (color & 0xFF000000) != 0;
}

uint SpriteManager::GetPixColor(uint spr_id, int offs_x, int offs_y, bool with_zoom)
{
    if (offs_x < 0 || offs_y < 0)
        return 0;
    SpriteInfo* si = GetSpriteInfo(spr_id);
    if (!si)
        return 0;

    // 2d animation
    if (with_zoom && (offs_x > si->Width / settings.SpritesZoom || offs_y > si->Height / settings.SpritesZoom))
        return 0;
    if (!with_zoom && (offs_x > si->Width || offs_y > si->Height))
        return 0;

    if (with_zoom)
    {
        offs_x = (int)(offs_x * settings.SpritesZoom);
        offs_y = (int)(offs_y * settings.SpritesZoom);
    }

    offs_x += (int)(si->Atlas->TextureOwner->SizeData[0] * si->SprRect.L);
    offs_y += (int)(si->Atlas->TextureOwner->SizeData[1] * si->SprRect.T);
    return GetRenderTargetPixel(si->Atlas->RT, offs_x, offs_y);
}

bool SpriteManager::IsEggTransp(int pix_x, int pix_y)
{
    if (!eggValid)
        return false;

    int ex = eggX + settings.ScrOx;
    int ey = eggY + settings.ScrOy;
    int ox = pix_x - (int)(ex / settings.SpritesZoom);
    int oy = pix_y - (int)(ey / settings.SpritesZoom);

    if (ox < 0 || oy < 0 || ox >= int(eggSprWidth / settings.SpritesZoom) ||
        oy >= int(eggSprHeight / settings.SpritesZoom))
        return false;

    ox = (int)(ox * settings.SpritesZoom);
    oy = (int)(oy * settings.SpritesZoom);

    uint egg_color = *(eggData + oy * eggSprWidth + ox);
    return (egg_color >> 24) < 127;
}

bool SpriteManager::DrawPoints(PointVec& points, int prim, float* zoom, PointF* offset, Effect* effect)
{
    if (points.empty())
        return true;

    Flush();

    Effect* draw_effect = effect;
    if (!draw_effect)
        draw_effect = effectMngr.Effects.Primitive;

    // Check primitives
    uint count = (uint)points.size();
    int prim_count = (int)count;
    GLenum prim_type;
    switch (prim)
    {
    case PRIMITIVE_POINTLIST:
        prim_type = GL_POINTS;
        break;
    case PRIMITIVE_LINELIST:
        prim_type = GL_LINES;
        prim_count /= 2;
        break;
    case PRIMITIVE_LINESTRIP:
        prim_type = GL_LINE_STRIP;
        prim_count -= 1;
        break;
    case PRIMITIVE_TRIANGLELIST:
        prim_type = GL_TRIANGLES;
        prim_count /= 3;
        break;
    case PRIMITIVE_TRIANGLESTRIP:
        prim_type = GL_TRIANGLE_STRIP;
        prim_count -= 2;
        break;
    case PRIMITIVE_TRIANGLEFAN:
        prim_type = GL_TRIANGLE_FAN;
        prim_count -= 2;
        break;
    default:
        return false;
    }
    if (prim_count <= 0)
        return false;

    // Resize buffers
    if (vBuffer.size() < count)
        vBuffer.resize(count);
    if (pointsVertexArray->VCount < (uint)vBuffer.size())
        InitVertexArray(pointsVertexArray, false, (uint)vBuffer.size());

    // Collect data
    for (uint i = 0; i < count; i++)
    {
        PrepPoint& point = points[i];
        float x = (float)point.PointX;
        float y = (float)point.PointY;
        if (point.PointOffsX)
            x += (float)*point.PointOffsX;
        if (point.PointOffsY)
            y += (float)*point.PointOffsY;
        if (zoom)
            x /= *zoom, y /= *zoom;
        if (offset)
            x += offset->X, y += offset->Y;

        memzero(&vBuffer[i], sizeof(Vertex2D));
        vBuffer[i].X = x;
        vBuffer[i].Y = y;
        vBuffer[i].Diffuse = COLOR_SWAP_RB(point.PointColor);
    }

// Enable smooth
#ifndef FO_OPENGL_ES
    if (zoom && *zoom != 1.0f)
        GL(glEnable(prim == PRIMITIVE_POINTLIST ? GL_POINT_SMOOTH : GL_LINE_SMOOTH));
#endif

    // Draw
    EnableVertexArray(pointsVertexArray, count);
    EnableScissor();

    for (size_t pass = 0; pass < draw_effect->Passes.size(); pass++)
    {
        EffectPass& effect_pass = draw_effect->Passes[pass];

        GL(glUseProgram(effect_pass.Program));

        if (IS_EFFECT_VALUE(effect_pass.ProjectionMatrix))
            GL(glUniformMatrix4fv(effect_pass.ProjectionMatrix, 1, GL_FALSE, projectionMatrixCM[0]));

        if (effect_pass.IsNeedProcess)
            effectMngr.EffectProcessVariables(effect_pass, true);

        GL(glDrawElements(prim_type, count, GL_UNSIGNED_SHORT, (void*)0));

        if (effect_pass.IsNeedProcess)
            effectMngr.EffectProcessVariables(effect_pass, false);
    }

    GL(glUseProgram(0));
    DisableVertexArray(pointsVertexArray);
    DisableScissor();

// Disable smooth
#ifndef FO_OPENGL_ES
    if (zoom && *zoom != 1.0f)
        GL(glDisable(prim == PRIMITIVE_POINTLIST ? GL_POINT_SMOOTH : GL_LINE_SMOOTH));
#endif

    return true;
}

bool SpriteManager::DrawContours()
{
    if (contoursAdded && rtContours && rtContoursMid)
    {
        // Draw collected contours
        DrawRenderTarget(rtContours, true);

        // Clean render targets
        PushRenderTarget(rtContours);
        ClearCurrentRenderTarget(0);
        PopRenderTarget();
        PushRenderTarget(rtContoursMid);
        ClearCurrentRenderTarget(0);
        PopRenderTarget();
        contoursAdded = false;
    }
    return true;
}

bool SpriteManager::CollectContour(int x, int y, SpriteInfo* si, Sprite* spr)
{
    if (!rtContours || !rtContoursMid || !effectMngr.Effects.Contour)
        return true;

    Rect borders = Rect(x - 1, y - 1, x + si->Width + 1, y + si->Height + 1);
    Texture* texture = si->Atlas->TextureOwner;
    RectF textureuv, sprite_border;

    if (borders.L >= settings.ScreenWidth * settings.SpritesZoom || borders.R < 0 ||
        borders.T >= settings.ScreenHeight * settings.SpritesZoom || borders.B < 0)
        return true;

    if (settings.SpritesZoom == 1.0f)
    {
        RectF& sr = si->SprRect;
        float txw = texture->SizeData[2];
        float txh = texture->SizeData[3];
        textureuv(sr.L - txw, sr.T - txh, sr.R + txw, sr.B + txh);
        sprite_border = textureuv;
    }
    else
    {
        RectF& sr = si->SprRect;
        borders((int)(x / settings.SpritesZoom), (int)(y / settings.SpritesZoom),
            (int)((x + si->Width) / settings.SpritesZoom), (int)((y + si->Height) / settings.SpritesZoom));
        RectF bordersf((float)borders.L, (float)borders.T, (float)borders.R, (float)borders.B);
        float mid_height = rtContoursMid->TargetTexture->SizeData[1];

        PushRenderTarget(rtContoursMid);

        uint pos = 0;
        vBuffer[pos].X = bordersf.L;
        vBuffer[pos].Y = mid_height - bordersf.B;
        vBuffer[pos].TU = sr.L;
        vBuffer[pos++].TV = sr.B;
        vBuffer[pos].X = bordersf.L;
        vBuffer[pos].Y = mid_height - bordersf.T;
        vBuffer[pos].TU = sr.L;
        vBuffer[pos++].TV = sr.T;
        vBuffer[pos].X = bordersf.R;
        vBuffer[pos].Y = mid_height - bordersf.T;
        vBuffer[pos].TU = sr.R;
        vBuffer[pos++].TV = sr.T;
        vBuffer[pos].X = bordersf.R;
        vBuffer[pos].Y = mid_height - bordersf.B;
        vBuffer[pos].TU = sr.R;
        vBuffer[pos++].TV = sr.B;

        curDrawQuad = 1;
        dipQueue.push_back({texture, effectMngr.Effects.FlushRenderTarget, 1});
        Flush();

        PopRenderTarget();

        texture = rtContoursMid->TargetTexture;
        float tw = texture->SizeData[0];
        float th = texture->SizeData[1];
        borders.L--, borders.T--, borders.R++, borders.B++;
        textureuv((float)borders.L / tw, (float)borders.T / th, (float)borders.R / tw, (float)borders.B / th);
        sprite_border = textureuv;
    }

    uint contour_color = 0;
    if (spr->ContourType == CONTOUR_RED)
        contour_color = 0xFFAF0000;
    else if (spr->ContourType == CONTOUR_YELLOW)
        contour_color = 0x00AFAF00; // Disable flashing by passing alpha == 0.0
    else if (spr->ContourType == CONTOUR_CUSTOM)
        contour_color = spr->ContourColor;
    else
        contour_color = 0xFFAFAFAF;
    contour_color = COLOR_SWAP_RB(contour_color);

    RectF borders_pos((float)borders.L, (float)borders.T, (float)borders.R, (float)borders.B);

    PushRenderTarget(rtContours);

    uint pos = 0;
    vBuffer[pos].X = borders_pos.L;
    vBuffer[pos].Y = borders_pos.B;
    vBuffer[pos].TU = textureuv.L;
    vBuffer[pos].TV = textureuv.B;
    vBuffer[pos++].Diffuse = contour_color;
    vBuffer[pos].X = borders_pos.L;
    vBuffer[pos].Y = borders_pos.T;
    vBuffer[pos].TU = textureuv.L;
    vBuffer[pos].TV = textureuv.T;
    vBuffer[pos++].Diffuse = contour_color;
    vBuffer[pos].X = borders_pos.R;
    vBuffer[pos].Y = borders_pos.T;
    vBuffer[pos].TU = textureuv.R;
    vBuffer[pos].TV = textureuv.T;
    vBuffer[pos++].Diffuse = contour_color;
    vBuffer[pos].X = borders_pos.R;
    vBuffer[pos].Y = borders_pos.B;
    vBuffer[pos].TU = textureuv.R;
    vBuffer[pos].TV = textureuv.B;
    vBuffer[pos++].Diffuse = contour_color;

    curDrawQuad = 1;
    dipQueue.push_back({texture, effectMngr.Effects.Contour, 1});
    dipQueue.back().SpriteBorder = sprite_border;
    Flush();

    PopRenderTarget();
    contoursAdded = true;
    return true;
}

#define FONT_BUF_LEN (0x5000)
#define FONT_MAX_LINES (1000)
#define FORMAT_TYPE_DRAW (0)
#define FORMAT_TYPE_SPLIT (1)
#define FORMAT_TYPE_LCOUNT (2)

struct Letter
{
    short PosX {};
    short PosY {};
    short W {};
    short H {};
    short OffsX {};
    short OffsY {};
    short XAdvance {};
    RectF TexUV {};
    RectF TexBorderedUV {};
};
using LetterMap = map<uint, Letter>;

struct FontData
{
    bool Builded;

    Texture* FontTex;
    Texture* FontTexBordered;

    LetterMap Letters;
    int SpaceWidth;
    int LineHeight;
    int YAdvance;
    Effect* DrawEffect;

    AnyFrames* ImageNormal;
    AnyFrames* ImageBordered;
    bool MakeGray;

    FontData(Effect* effect)
    {
        Builded = false;
        FontTex = nullptr;
        FontTexBordered = nullptr;
        SpaceWidth = 0;
        LineHeight = 0;
        YAdvance = 0;
        DrawEffect = effect;
        ImageNormal = nullptr;
        ImageBordered = nullptr;
        MakeGray = false;
    }
};
typedef vector<FontData*> FontDataVec;

FontDataVec Fonts;

int DefFontIndex = -1;
uint DefFontColor = 0;

FontData* GetFont(int num)
{
    if (num < 0)
        num = DefFontIndex;
    if (num < 0 || num >= (int)Fonts.size())
        return nullptr;
    return Fonts[num];
}

struct FontFormatInfo
{
    FontData* CurFont;
    uint Flags;
    Rect Region;
    char Str[FONT_BUF_LEN];
    char* PStr;
    uint LinesAll;
    uint LinesInRect;
    int CurX, CurY, MaxCurX;
    uint ColorDots[FONT_BUF_LEN];
    short LineWidth[FONT_MAX_LINES];
    ushort LineSpaceWidth[FONT_MAX_LINES];
    uint OffsColDots;
    uint DefColor;
    StrVec* StrLines;
    bool IsError;

    void Init(FontData* font, uint flags, const Rect& region, const string& str_in)
    {
        CurFont = font;
        Flags = flags;
        LinesAll = 1;
        LinesInRect = 0;
        IsError = false;
        CurX = CurY = MaxCurX = 0;
        Region = region;
        memzero(ColorDots, sizeof(ColorDots));
        memzero(LineWidth, sizeof(LineWidth));
        memzero(LineSpaceWidth, sizeof(LineSpaceWidth));
        OffsColDots = 0;
        Str::Copy(Str, str_in.c_str());
        PStr = Str;
        DefColor = COLOR_TEXT;
        StrLines = nullptr;
    }

    FontFormatInfo& operator=(const FontFormatInfo& _fi)
    {
        CurFont = _fi.CurFont;
        Flags = _fi.Flags;
        LinesAll = _fi.LinesAll;
        LinesInRect = _fi.LinesInRect;
        IsError = _fi.IsError;
        CurX = _fi.CurX;
        CurY = _fi.CurY;
        MaxCurX = _fi.MaxCurX;
        Region = _fi.Region;
        memcpy(Str, _fi.Str, sizeof(Str));
        memcpy(ColorDots, _fi.ColorDots, sizeof(ColorDots));
        memcpy(LineWidth, _fi.LineWidth, sizeof(LineWidth));
        memcpy(LineSpaceWidth, _fi.LineSpaceWidth, sizeof(LineSpaceWidth));
        OffsColDots = _fi.OffsColDots;
        DefColor = _fi.DefColor;
        PStr = Str;
        StrLines = _fi.StrLines;
        return *this;
    }
};

void SpriteManager::ClearFonts()
{
    for (size_t i = 0; i < Fonts.size(); i++)
        SAFEDEL(Fonts[i]);
    Fonts.clear();
}

void SpriteManager::SetDefaultFont(int index, uint color)
{
    DefFontIndex = index;
    DefFontColor = color;
}

void SpriteManager::SetFontEffect(int index, Effect* effect)
{
    FontData* font = GetFont(index);
    if (font)
        font->DrawEffect = (effect ? effect : effectMngr.Effects.Font);
}

void SpriteManager::BuildFonts()
{
    for (size_t i = 0; i < Fonts.size(); i++)
        if (Fonts[i] && !Fonts[i]->Builded)
            BuildFont((int)i);
}

void SpriteManager::BuildFont(int index)
{
    FontData& font = *Fonts[index];
    font.Builded = true;

    // Fix texture coordinates
    SpriteInfo* si = GetSpriteInfo(font.ImageNormal->GetSprId(0));
    float tex_w = (float)si->Atlas->Width;
    float tex_h = (float)si->Atlas->Height;
    float image_x = tex_w * si->SprRect.L;
    float image_y = tex_h * si->SprRect.T;
    int max_h = 0;
    for (auto it = font.Letters.begin(), end = font.Letters.end(); it != end; ++it)
    {
        Letter& l = it->second;
        float x = (float)l.PosX;
        float y = (float)l.PosY;
        float w = (float)l.W;
        float h = (float)l.H;
        l.TexUV[0] = (image_x + x - 1.0f) / tex_w;
        l.TexUV[1] = (image_y + y - 1.0f) / tex_h;
        l.TexUV[2] = (image_x + x + w + 1.0f) / tex_w;
        l.TexUV[3] = (image_y + y + h + 1.0f) / tex_h;
        if (l.H > max_h)
            max_h = l.H;
    }

    // Fill data
    font.FontTex = si->Atlas->TextureOwner;
    if (font.LineHeight == 0)
        font.LineHeight = max_h;
    if (font.Letters.count(' '))
        font.SpaceWidth = font.Letters[' '].XAdvance;

    SpriteInfo* si_bordered = (font.ImageBordered ? GetSpriteInfo(font.ImageBordered->GetSprId(0)) : nullptr);
    font.FontTexBordered = (si_bordered ? si_bordered->Atlas->TextureOwner : nullptr);

    uint normal_ox = (uint)(tex_w * si->SprRect.L);
    uint normal_oy = (uint)(tex_h * si->SprRect.T);
    uint bordered_ox = (si_bordered ? (uint)((float)si_bordered->Atlas->Width * si_bordered->SprRect.L) : 0);
    uint bordered_oy = (si_bordered ? (uint)((float)si_bordered->Atlas->Height * si_bordered->SprRect.T) : 0);

// Read texture data
#define PIXEL_AT(data, width, x, y) (*((uint*)data + y * width + x))

    PushRenderTarget(si->Atlas->RT);
    uint* data_normal = new uint[si->Width * si->Height];
    GL(glReadPixels(normal_ox, normal_oy, si->Width, si->Height, GL_RGBA, GL_UNSIGNED_BYTE, data_normal));
    PopRenderTarget();

    uint* data_bordered = nullptr;
    if (si_bordered)
    {
        PushRenderTarget(si_bordered->Atlas->RT);
        data_bordered = new uint[si_bordered->Width * si_bordered->Height];
        GL(glReadPixels(bordered_ox, bordered_oy, si_bordered->Width, si_bordered->Height, GL_RGBA, GL_UNSIGNED_BYTE,
            data_bordered));
        PopRenderTarget();
    }

    // Normalize color to gray
    if (font.MakeGray)
    {
        for (uint y = 0; y < (uint)si->Height; y++)
        {
            for (uint x = 0; x < (uint)si->Width; x++)
            {
                uchar a = ((uchar*)&PIXEL_AT(data_normal, si->Width, x, y))[3];
                if (a)
                {
                    PIXEL_AT(data_normal, si->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                    if (si_bordered)
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                }
                else
                {
                    PIXEL_AT(data_normal, si->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                    if (si_bordered)
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                }
            }
        }
        Rect r = Rect(normal_ox, normal_oy, normal_ox + si->Width - 1, normal_oy + si->Height - 1);
        si->Atlas->TextureOwner->UpdateRegion(r, (uchar*)data_normal);
    }

    // Fill border
    if (si_bordered)
    {
        for (uint y = 1; y < (uint)si_bordered->Height - 2; y++)
        {
            for (uint x = 1; x < (uint)si_bordered->Width - 2; x++)
            {
                if (PIXEL_AT(data_normal, si->Width, x, y))
                {
                    for (int xx = -1; xx <= 1; xx++)
                    {
                        for (int yy = -1; yy <= 1; yy++)
                        {
                            uint ox = x + xx;
                            uint oy = y + yy;
                            if (!PIXEL_AT(data_bordered, si_bordered->Width, ox, oy))
                                PIXEL_AT(data_bordered, si_bordered->Width, ox, oy) = COLOR_RGB(0, 0, 0);
                        }
                    }
                }
            }
        }
        Rect r_bordered =
            Rect(bordered_ox, bordered_oy, bordered_ox + si_bordered->Width - 1, bordered_oy + si_bordered->Height - 1);
        si_bordered->Atlas->TextureOwner->UpdateRegion(r_bordered, (uchar*)data_bordered);

        // Fix texture coordinates on bordered texture
        tex_w = (float)si_bordered->Atlas->Width;
        tex_h = (float)si_bordered->Atlas->Height;
        image_x = tex_w * si_bordered->SprRect.L;
        image_y = tex_h * si_bordered->SprRect.T;
        for (auto it = font.Letters.begin(), end = font.Letters.end(); it != end; ++it)
        {
            Letter& l = it->second;
            float x = (float)l.PosX;
            float y = (float)l.PosY;
            float w = (float)l.W;
            float h = (float)l.H;
            l.TexBorderedUV[0] = (image_x + x - 1.0f) / tex_w;
            l.TexBorderedUV[1] = (image_y + y - 1.0f) / tex_h;
            l.TexBorderedUV[2] = (image_x + x + w + 1.0f) / tex_w;
            l.TexBorderedUV[3] = (image_y + y + h + 1.0f) / tex_h;
        }
    }

// Clean up
#undef PIXEL_AT
    SAFEDELA(data_normal);
    SAFEDELA(data_bordered);
}

bool SpriteManager::LoadFontFO(int index, const string& font_name, bool not_bordered, bool skip_if_loaded /* = true */)
{
    // Skip if loaded
    if (skip_if_loaded && index < (int)Fonts.size() && Fonts[index])
        return true;

    // Load font data
    string fname = _str("Fonts/{}.fofnt", font_name);
    File file = fileMngr.ReadFile(fname);
    if (!file)
    {
        WriteLog("File '{}' not found.\n", fname);
        return false;
    }

    FontData font(effectMngr.Effects.Font);
    string image_name;

    // Parse data
    istringstream str((char*)file.GetBuf());
    string key;
    string letter_buf;
    Letter* cur_letter = nullptr;
    int version = -1;
    while (!str.eof() && !str.fail())
    {
        // Get key
        str >> key;

        // Cut off comments
        size_t comment = key.find('#');
        if (comment != string::npos)
            key.erase(comment);
        comment = key.find(';');
        if (comment != string::npos)
            key.erase(comment);

        // Check version
        if (version == -1)
        {
            if (key != "Version")
            {
                WriteLog("Font '{}' 'Version' signature not found (used deprecated format of 'fofnt').\n", fname);
                return false;
            }
            str >> version;
            if (version > 2)
            {
                WriteLog("Font '{}' version {} not supported (try update client).\n", fname, version);
                return false;
            }
            continue;
        }

        // Get value
        if (key == "Image")
        {
            str >> image_name;
        }
        else if (key == "LineHeight")
        {
            str >> font.LineHeight;
        }
        else if (key == "YAdvance")
        {
            str >> font.YAdvance;
        }
        else if (key == "End")
        {
            break;
        }
        else if (key == "Letter")
        {
            std::getline(str, letter_buf, '\n');
            size_t utf8_letter_begin = letter_buf.find('\'');
            if (utf8_letter_begin == string::npos)
            {
                WriteLog("Font '{}' invalid letter specification.\n", fname);
                return false;
            }
            utf8_letter_begin++;

            uint letter_len;
            uint letter = utf8::Decode(letter_buf.c_str() + utf8_letter_begin, &letter_len);
            if (!utf8::IsValid(letter))
            {
                WriteLog("Font '{}' invalid UTF8 letter at  '{}'.\n", fname, letter_buf);
                return false;
            }

            cur_letter = &font.Letters[letter];
        }

        if (!cur_letter)
            continue;

        if (key == "PositionX")
            str >> cur_letter->PosX;
        else if (key == "PositionY")
            str >> cur_letter->PosY;
        else if (key == "Width")
            str >> cur_letter->W;
        else if (key == "Height")
            str >> cur_letter->H;
        else if (key == "OffsetX")
            str >> cur_letter->OffsX;
        else if (key == "OffsetY")
            str >> cur_letter->OffsY;
        else if (key == "XAdvance")
            str >> cur_letter->XAdvance;
    }

    bool make_gray = false;
    if (image_name.back() == '*')
    {
        make_gray = true;
        image_name = image_name.substr(0, image_name.size() - 1);
    }
    font.MakeGray = make_gray;

    // Load image
    image_name.insert(0, "Fonts/");
    AnyFrames* image_normal = LoadAnimation(image_name);
    if (!image_normal)
    {
        WriteLog("Image file '{}' not found.\n", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    if (!not_bordered)
    {
        AnyFrames* image_bordered = LoadAnimation(image_name);
        if (!image_bordered)
        {
            WriteLog("Can't load twice file '{}'.\n", image_name);
            return false;
        }
        font.ImageBordered = image_bordered;
    }

    // Register
    if (index >= (int)Fonts.size())
        Fonts.resize(index + 1);
    SAFEDEL(Fonts[index]);
    Fonts[index] = new FontData(font);

    return true;
}

bool SpriteManager::LoadFontBMF(int index, const string& font_name)
{
    if (index < 0)
    {
        WriteLog("Invalid index.\n");
        return false;
    }

    FontData font(effectMngr.Effects.Font);
    File file = fileMngr.ReadFile(_str("Fonts/{}.fnt", font_name));
    if (!file)
    {
        WriteLog("Font file '{}.fnt' not found.\n", font_name);
        return false;
    }

    uint signature = file.GetLEUInt();
    if (signature != MAKEUINT('B', 'M', 'F', 3))
    {
        WriteLog("Invalid signature of font '{}'.\n", font_name);
        return false;
    }

    // Info
    file.GetUChar();
    uint block_len = file.GetLEUInt();
    uint next_block = block_len + file.GetCurPos() + 1;

    file.GoForward(7);
    if (file.GetUChar() != 1 || file.GetUChar() != 1 || file.GetUChar() != 1 || file.GetUChar() != 1)
    {
        WriteLog("Wrong padding in font '{}'.\n", font_name);
        return false;
    }

    // Common
    file.SetCurPos(next_block);
    block_len = file.GetLEUInt();
    next_block = block_len + file.GetCurPos() + 1;

    int line_height = file.GetLEUShort();
    int base_height = file.GetLEUShort();
    file.GetLEUShort(); // Texture width
    file.GetLEUShort(); // Texture height

    if (file.GetLEUShort() != 1)
    {
        WriteLog("Texture for font '{}' must be one.\n", font_name);
        return false;
    }

    // Pages
    file.SetCurPos(next_block);
    block_len = file.GetLEUInt();
    next_block = block_len + file.GetCurPos() + 1;

    // Image name
    string image_name = file.GetStrNT();
    image_name.insert(0, "Fonts/");

    // Chars
    file.SetCurPos(next_block);
    int count = file.GetLEUInt() / 20;
    for (int i = 0; i < count; i++)
    {
        // Read data
        uint id = file.GetLEUInt();
        int x = file.GetLEUShort();
        int y = file.GetLEUShort();
        int w = file.GetLEUShort();
        int h = file.GetLEUShort();
        int ox = file.GetLEUShort();
        int oy = file.GetLEUShort();
        int xa = file.GetLEUShort();
        file.GoForward(2);

        // Fill data
        Letter& let = font.Letters[id];
        let.PosX = x + 1;
        let.PosY = y + 1;
        let.W = w - 2;
        let.H = h - 2;
        let.OffsX = -ox;
        let.OffsY = -oy + (line_height - base_height);
        let.XAdvance = xa + 1;
    }

    font.LineHeight = (font.Letters.count('W') ? font.Letters['W'].H : base_height);
    font.YAdvance = font.LineHeight / 2;
    font.MakeGray = true;

    // Load image
    AnyFrames* image_normal = LoadAnimation(image_name);
    if (!image_normal)
    {
        WriteLog("Image file '{}' not found.\n", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    AnyFrames* image_bordered = LoadAnimation(image_name);
    if (!image_bordered)
    {
        WriteLog("Can't load twice file '{}'.\n", image_name);
        return false;
    }
    font.ImageBordered = image_bordered;

    // Register
    if (index >= (int)Fonts.size())
        Fonts.resize(index + 1);
    SAFEDEL(Fonts[index]);
    Fonts[index] = new FontData(font);
    return true;
}

static void Str_GoTo(char*& str, char ch, bool skip_char = false)
{
    while (*str && *str != ch)
        ++str;
    if (skip_char && *str)
        ++str;
}

static void Str_EraseInterval(char* str, uint len)
{
    if (!str || !len)
        return;

    char* str2 = str + len;
    while (*str2)
    {
        *str = *str2;
        ++str;
        ++str2;
    }

    *str = 0;
}

static void Str_Insert(char* to, const char* from, uint from_len = 0)
{
    if (!to || !from)
        return;

    if (!from_len)
        from_len = (uint)strlen(from);
    if (!from_len)
        return;

    char* end_to = to;
    while (*end_to)
        ++end_to;

    for (; end_to >= to; --end_to)
        *(end_to + from_len) = *end_to;

    while (from_len--)
    {
        *to = *from;
        ++to;
        ++from;
    }
}

void FormatText(FontFormatInfo& fi, int fmt_type)
{
    char* str = fi.PStr;
    uint flags = fi.Flags;
    FontData* font = fi.CurFont;
    Rect& r = fi.Region;
    bool infinity_w = (r.L == r.R);
    bool infinity_h = (r.T == r.B);
    int curx = 0;
    int cury = 0;
    uint& offs_col = fi.OffsColDots;

    if (fmt_type != FORMAT_TYPE_DRAW && fmt_type != FORMAT_TYPE_LCOUNT && fmt_type != FORMAT_TYPE_SPLIT)
    {
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT && !fi.StrLines)
    {
        fi.IsError = true;
        return;
    }

    // Colorize
    uint* dots = nullptr;
    uint d_offs = 0;
    char* str_ = str;
    string buf;
    if (fmt_type == FORMAT_TYPE_DRAW && !FLAG(flags, FT_NO_COLORIZE))
        dots = fi.ColorDots;

    while (*str_)
    {
        char* s0 = str_;
        Str_GoTo(str_, '|');
        char* s1 = str_;
        Str_GoTo(str_, ' ');
        char* s2 = str_;

        // TODO: optimize
        // if(!_str[0] && !*s1) break;
        if (dots)
        {
            size_t d_len = (uint)((size_t)s2 - (size_t)s1) + 1;
            uint d = (uint)strtoul(s1 + 1, nullptr, 0);

            dots[(uint)((size_t)s1 - (size_t)str) - d_offs] = d;
            d_offs += (uint)d_len;
        }

        *s1 = 0;
        buf += s0;

        if (!*str_)
            break;
        str_++;
    }

    Str::Copy(str, FONT_BUF_LEN, buf.c_str());

    // Skip lines
    uint skip_line = (FLAG(flags, FT_SKIPLINES(0)) ? flags >> 16 : 0);
    uint skip_line_end = (FLAG(flags, FT_SKIPLINES_END(0)) ? flags >> 16 : 0);

    // Format
    curx = r.L;
    cury = r.T;
    for (int i = 0, i_advance = 1; str[i]; i += i_advance)
    {
        uint letter_len;
        uint letter = utf8::Decode(&str[i], &letter_len);
        i_advance = letter_len;

        int x_advance;
        switch (letter)
        {
        case '\r':
            continue;
        case ' ':
            x_advance = font->SpaceWidth;
            break;
        case '\t':
            x_advance = font->SpaceWidth * 4;
            break;
        default:
            auto it = font->Letters.find(letter);
            if (it != font->Letters.end())
                x_advance = it->second.XAdvance;
            else
                x_advance = 0;
            break;
        }

        if (!infinity_w && curx + x_advance > r.R)
        {
            if (curx > fi.MaxCurX)
                fi.MaxCurX = curx;

            if (fmt_type == FORMAT_TYPE_DRAW && FLAG(flags, FT_NOBREAK))
            {
                str[i] = 0;
                break;
            }
            else if (FLAG(flags, FT_NOBREAK_LINE))
            {
                int j = i;
                for (; str[j]; j++)
                    if (str[j] == '\n')
                        break;

                Str_EraseInterval(&str[i], j - i);
                letter = str[i];
                i_advance = 1;
                if (fmt_type == FORMAT_TYPE_DRAW)
                    for (int k = i, l = MAX_FOTEXT - (j - i); k < l; k++)
                        fi.ColorDots[k] = fi.ColorDots[k + (j - i)];
            }
            else if (str[i] != '\n')
            {
                int j = i;
                for (; j >= 0; j--)
                {
                    if (str[j] == ' ' || str[j] == '\t')
                    {
                        str[j] = '\n';
                        i = j;
                        letter = '\n';
                        i_advance = 1;
                        break;
                    }
                    else if (str[j] == '\n')
                    {
                        j = -1;
                        break;
                    }
                }

                if (j < 0)
                {
                    letter = '\n';
                    i_advance = 1;
                    Str_Insert(&str[i], "\n");
                    if (fmt_type == FORMAT_TYPE_DRAW)
                        for (int k = MAX_FOTEXT - 1; k > i; k--)
                            fi.ColorDots[k] = fi.ColorDots[k - 1];
                }

                if (FLAG(flags, FT_ALIGN) && !skip_line)
                {
                    fi.LineSpaceWidth[fi.LinesAll - 1] = 1;
                    // Erase next first spaces
                    int ii = i + i_advance;
                    for (j = ii;; j++)
                        if (str[j] != ' ')
                            break;
                    if (j > ii)
                    {
                        Str_EraseInterval(&str[ii], j - ii);
                        if (fmt_type == FORMAT_TYPE_DRAW)
                            for (int k = ii, l = MAX_FOTEXT - (j - ii); k < l; k++)
                                fi.ColorDots[k] = fi.ColorDots[k + (j - ii)];
                    }
                }
            }
        }

        switch (letter)
        {
        case '\n':
            if (!skip_line)
            {
                cury += font->LineHeight + font->YAdvance;
                if (!infinity_h && cury + font->LineHeight > r.B && !fi.LinesInRect)
                    fi.LinesInRect = fi.LinesAll;

                if (fmt_type == FORMAT_TYPE_DRAW)
                {
                    if (fi.LinesInRect && !FLAG(flags, FT_UPPER))
                    {
                        // fi.LinesAll++;
                        str[i] = 0;
                        break;
                    }
                }
                else if (fmt_type == FORMAT_TYPE_SPLIT)
                {
                    if (fi.LinesInRect && !(fi.LinesAll % fi.LinesInRect))
                    {
                        str[i] = 0;
                        (*fi.StrLines).push_back(str);
                        str = &str[i + i_advance];
                        i = -i_advance;
                    }
                }

                if (str[i + i_advance])
                    fi.LinesAll++;
            }
            else
            {
                skip_line--;
                Str_EraseInterval(str, i + i_advance);
                offs_col += i + i_advance;
                //	if(fmt_type==FORMAT_TYPE_DRAW)
                //		for(int k=0,l=MAX_FOTEXT-(i+1);k<l;k++) fi.ColorDots[k]=fi.ColorDots[k+i+1];
                i = 0;
                i_advance = 0;
            }

            if (curx > fi.MaxCurX)
                fi.MaxCurX = curx;
            curx = r.L;
            continue;
        case 0:
            break;
        default:
            curx += x_advance;
            continue;
        }

        if (!str[i])
            break;
    }
    if (curx > fi.MaxCurX)
        fi.MaxCurX = curx;

    if (skip_line_end)
    {
        int len = (int)strlen(str);
        for (int i = len - 2; i >= 0; i--)
        {
            if (str[i] == '\n')
            {
                str[i] = 0;
                fi.LinesAll--;
                if (!--skip_line_end)
                    break;
            }
        }

        if (skip_line_end)
        {
            WriteLog("error3\n");
            fi.IsError = true;
            return;
        }
    }

    if (skip_line)
    {
        WriteLog("error4\n");
        fi.IsError = true;
        return;
    }

    if (!fi.LinesInRect)
        fi.LinesInRect = fi.LinesAll;

    if (fi.LinesAll > FONT_MAX_LINES)
    {
        WriteLog("error5 {}\n", fi.LinesAll);
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT)
    {
        (*fi.StrLines).push_back(string(str));
        return;
    }
    else if (fmt_type == FORMAT_TYPE_LCOUNT)
    {
        return;
    }

    // Up text
    if (FLAG(flags, FT_UPPER) && fi.LinesAll > fi.LinesInRect)
    {
        uint j = 0;
        uint line_cur = 0;
        uint last_col = 0;
        for (; str[j]; ++j)
        {
            if (str[j] == '\n')
            {
                line_cur++;
                if (line_cur >= (fi.LinesAll - fi.LinesInRect))
                    break;
            }

            if (fi.ColorDots[j])
                last_col = fi.ColorDots[j];
        }

        if (!FLAG(flags, FT_NO_COLORIZE))
        {
            offs_col += j + 1;
            if (last_col && !fi.ColorDots[j + 1])
                fi.ColorDots[j + 1] = last_col;
        }

        str = &str[j + 1];
        fi.PStr = str;

        fi.LinesAll = fi.LinesInRect;
    }

    // Align
    curx = r.L;
    cury = r.T;

    for (uint i = 0; i < fi.LinesAll; i++)
        fi.LineWidth[i] = curx;

    bool can_count = false;
    int spaces = 0;
    int curstr = 0;
    for (int i = 0, i_advance = 1;; i += i_advance)
    {
        uint letter_len;
        uint letter = utf8::Decode(&str[i], &letter_len);
        i_advance = letter_len;

        switch (letter)
        {
        case ' ':
            curx += font->SpaceWidth;
            if (can_count)
                spaces++;
            break;
        case '\t':
            curx += font->SpaceWidth * 4;
            break;
        case 0:
        case '\n':
            fi.LineWidth[curstr] = curx;
            cury += font->LineHeight + font->YAdvance;
            curx = r.L;

            // Erase last spaces
            /*for(int j=i-1;spaces>0 && j>=0;j--)
               {
               if(str[j]==' ')
               {
               spaces--;
               str[j]='\r';
               }
               else if(str[j]!='\r') break;
               }*/

            // Align
            if (fi.LineSpaceWidth[curstr] == 1 && spaces > 0 && !infinity_w)
                fi.LineSpaceWidth[curstr] = font->SpaceWidth + (r.R - fi.LineWidth[curstr]) / spaces;
            else
                fi.LineSpaceWidth[curstr] = font->SpaceWidth;

            curstr++;
            can_count = false;
            spaces = 0;
            break;
        case '\r':
            break;
        default:
            auto it = font->Letters.find(letter);
            if (it != font->Letters.end())
                curx += it->second.XAdvance;
            // if(curx>fi.LineWidth[curstr]) fi.LineWidth[curstr]=curx;
            can_count = true;
            break;
        }

        if (!str[i])
            break;
    }

    // Initial position
    fi.CurX = r.L;
    fi.CurY = r.T;

    // Align X
    if (FLAG(flags, FT_CENTERX))
        fi.CurX += (r.R - fi.LineWidth[0]) / 2;
    else if (FLAG(flags, FT_CENTERR))
        fi.CurX += r.R - fi.LineWidth[0];
    // Align Y
    if (FLAG(flags, FT_CENTERY))
        fi.CurY = r.T +
            (int)((r.B - r.T + 1) - fi.LinesInRect * font->LineHeight - (fi.LinesInRect - 1) * font->YAdvance) / 2 +
            (FLAG(flags, FT_CENTERY_ENGINE) ? 1 : 0);
    else if (FLAG(flags, FT_BOTTOM))
        fi.CurY = r.B - (int)(fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance);
}

bool SpriteManager::DrawStr(const Rect& r, const string& str, uint flags, uint color /* = 0 */, int num_font /* = -1 */)
{
    // Check
    if (str.empty())
        return false;

    // Get font
    FontData* font = GetFont(num_font);
    if (!font)
        return false;

    // FormatBuf
    if (!color && DefFontColor)
        color = DefFontColor;
    color = COLOR_SWAP_RB(color);

    static FontFormatInfo fi;
    fi.Init(font, flags, r, str.c_str());
    fi.DefColor = color;
    FormatText(fi, FORMAT_TYPE_DRAW);
    if (fi.IsError)
        return false;

    char* str_ = fi.PStr;
    uint offs_col = fi.OffsColDots;
    int curx = fi.CurX;
    int cury = fi.CurY;
    int curstr = 0;
    Texture* texture = (FLAG(flags, FT_BORDERED) && font->FontTexBordered ? font->FontTexBordered : font->FontTex);

    if (curDrawQuad)
        Flush();

    if (!FLAG(flags, FT_NO_COLORIZE))
    {
        for (int i = offs_col; i >= 0; i--)
        {
            if (fi.ColorDots[i])
            {
                if (fi.ColorDots[i] & 0xFF000000)
                    color = fi.ColorDots[i]; // With alpha
                else
                    color = (color & 0xFF000000) | (fi.ColorDots[i] & 0x00FFFFFF); // Still old alpha
                color = COLOR_SWAP_RB(color);
                break;
            }
        }
    }

    bool variable_space = false;
    for (int i = 0, i_advance = 1; str_[i]; i += i_advance)
    {
        if (!FLAG(flags, FT_NO_COLORIZE))
        {
            uint new_color = fi.ColorDots[i + offs_col];
            if (new_color)
            {
                if (new_color & 0xFF000000)
                    color = new_color; // With alpha
                else
                    color = (color & 0xFF000000) | (new_color & 0x00FFFFFF); // Still old alpha
                color = COLOR_SWAP_RB(color);
            }
        }

        uint letter_len;
        uint letter = utf8::Decode(&str_[i], &letter_len);
        i_advance = letter_len;

        switch (letter)
        {
        case ' ':
            curx += (variable_space ? fi.LineSpaceWidth[curstr] : font->SpaceWidth);
            continue;
        case '\t':
            curx += font->SpaceWidth * 4;
            continue;
        case '\n':
            cury += font->LineHeight + font->YAdvance;
            curx = r.L;
            curstr++;
            variable_space = false;
            if (FLAG(flags, FT_CENTERX))
                curx += (r.R - fi.LineWidth[curstr]) / 2;
            else if (FLAG(flags, FT_CENTERR))
                curx += r.R - fi.LineWidth[curstr];
            continue;
        case '\r':
            continue;
        default:
            auto it = font->Letters.find(letter);
            if (it == font->Letters.end())
                continue;

            Letter& l = it->second;

            int pos = curDrawQuad * 4;
            int x = curx - l.OffsX - 1;
            int y = cury - l.OffsY - 1;
            int w = l.W + 2;
            int h = l.H + 2;

            RectF& texture_uv = (FLAG(flags, FT_BORDERED) ? l.TexBorderedUV : l.TexUV);
            float x1 = texture_uv[0];
            float y1 = texture_uv[1];
            float x2 = texture_uv[2];
            float y2 = texture_uv[3];

            vBuffer[pos].X = (float)x;
            vBuffer[pos].Y = (float)y + h;
            vBuffer[pos].TU = x1;
            vBuffer[pos].TV = y2;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x;
            vBuffer[pos].Y = (float)y;
            vBuffer[pos].TU = x1;
            vBuffer[pos].TV = y1;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x + w;
            vBuffer[pos].Y = (float)y;
            vBuffer[pos].TU = x2;
            vBuffer[pos].TV = y1;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x + w;
            vBuffer[pos].Y = (float)y + h;
            vBuffer[pos].TU = x2;
            vBuffer[pos].TV = y2;
            vBuffer[pos].Diffuse = color;

            if (++curDrawQuad == drawQuadCount)
            {
                dipQueue.push_back({texture, font->DrawEffect, 1});
                dipQueue.back().SpritesCount = curDrawQuad;
                Flush();
            }

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    if (curDrawQuad)
    {
        dipQueue.push_back({texture, font->DrawEffect, 1});
        dipQueue.back().SpritesCount = curDrawQuad;
        Flush();
    }

    return true;
}

int SpriteManager::GetLinesCount(int width, int height, const string& str, int num_font /* = -1 */)
{
    if (width <= 0 || height <= 0)
        return 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    if (str.empty())
        return height / (font->LineHeight + font->YAdvance);

    FontFormatInfo fi;
    fi.Init(font, 0, Rect(0, 0, width ? width : settings.ScreenWidth, height ? height : settings.ScreenHeight), str);
    FormatText(fi, FORMAT_TYPE_LCOUNT);
    if (fi.IsError)
        return 0;

    return fi.LinesInRect;
}

int SpriteManager::GetLinesHeight(int width, int height, const string& str, int num_font /* = -1 */)
{
    if (width <= 0 || height <= 0)
        return 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    int cnt = GetLinesCount(width, height, str, num_font);
    if (cnt <= 0)
        return 0;

    return cnt * font->LineHeight + (cnt - 1) * font->YAdvance;
}

int SpriteManager::GetLineHeight(int num_font /* = -1 */)
{
    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    return font->LineHeight;
}

void SpriteManager::GetTextInfo(
    int width, int height, const string& str, int num_font, int flags, int& tw, int& th, int& lines)
{
    tw = th = lines = 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return;

    if (str.empty())
    {
        tw = width;
        th = height;
        lines = height / (font->LineHeight + font->YAdvance);
        return;
    }

    static FontFormatInfo fi;
    fi.Init(font, flags, Rect(0, 0, width, height), str);
    FormatText(fi, FORMAT_TYPE_LCOUNT);
    if (fi.IsError)
        return;

    lines = fi.LinesInRect;
    th = fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance;
    tw = fi.MaxCurX - fi.Region.L;
}

int SpriteManager::SplitLines(const Rect& r, const string& cstr, int num_font, StrVec& str_vec)
{
    // Check & Prepare
    str_vec.clear();
    if (cstr.empty())
        return 0;

    // Get font
    FontData* font = GetFont(num_font);
    if (!font)
        return 0;
    static FontFormatInfo fi;
    fi.Init(font, 0, r, cstr);
    fi.StrLines = &str_vec;
    FormatText(fi, FORMAT_TYPE_SPLIT);
    if (fi.IsError)
        return 0;
    return (int)str_vec.size();
}

bool SpriteManager::HaveLetter(int num_font, uint letter)
{
    FontData* font = GetFont(num_font);
    if (!font)
        return false;
    return font->Letters.count(letter) > 0;
}
