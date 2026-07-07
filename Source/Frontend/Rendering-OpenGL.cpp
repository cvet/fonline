//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Rendering.h"

#if FO_HAVE_OPENGL

#include "Application.h"
#include "WebRelated.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_video.h"

#if !FO_OPENGL_ES
#define SDL_OPENGL_1_NO_PROTOTYPES
#define SDL_OPENGL_1_FUNCTION_TYPEDEFS
#include "SDL3/SDL_opengl.h"
#endif

#if FO_OPENGL_ES
#if FO_IOS
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#else
#include <GLES3/gl3.h>
#include <GLES3/gl3platform.h>
#endif
#endif

FO_BEGIN_NAMESPACE

#if !FO_OPENGL_ES
#define FO_GL_FUNCTIONS(X) \
    X(glActiveTexture, PFNGLACTIVETEXTUREPROC); \
    X(glAttachShader, PFNGLATTACHSHADERPROC); \
    X(glBindBuffer, PFNGLBINDBUFFERPROC); \
    X(glBindBufferBase, PFNGLBINDBUFFERBASEPROC); \
    X(glBindBufferRange, PFNGLBINDBUFFERRANGEPROC); \
    X(glBufferSubData, PFNGLBUFFERSUBDATAPROC); \
    X(glBindFramebuffer, PFNGLBINDFRAMEBUFFERPROC); \
    X(glBindFramebufferEXT, PFNGLBINDFRAMEBUFFEREXTPROC); \
    X(glBindRenderbuffer, PFNGLBINDRENDERBUFFERPROC); \
    X(glBindRenderbufferEXT, PFNGLBINDRENDERBUFFEREXTPROC); \
    X(glBindTexture, PFNGLBINDTEXTUREPROC); \
    X(glBindVertexArray, PFNGLBINDVERTEXARRAYPROC); \
    X(glBindVertexArrayAPPLE, PFNGLBINDVERTEXARRAYAPPLEPROC); \
    X(glBlendEquation, PFNGLBLENDEQUATIONPROC); \
    X(glBlendFunc, PFNGLBLENDFUNCPROC); \
    X(glBufferData, PFNGLBUFFERDATAPROC); \
    X(glCheckFramebufferStatus, PFNGLCHECKFRAMEBUFFERSTATUSPROC); \
    X(glCheckFramebufferStatusEXT, PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC); \
    X(glClear, PFNGLCLEARPROC); \
    X(glClearColor, PFNGLCLEARCOLORPROC); \
    X(glClearDepthf, PFNGLCLEARDEPTHFPROC); \
    X(glClearStencil, PFNGLCLEARSTENCILPROC); \
    X(glCompileShader, PFNGLCOMPILESHADERPROC); \
    X(glCreateProgram, PFNGLCREATEPROGRAMPROC); \
    X(glCreateShader, PFNGLCREATESHADERPROC); \
    X(glDeleteBuffers, PFNGLDELETEBUFFERSPROC); \
    X(glDeleteFramebuffers, PFNGLDELETEFRAMEBUFFERSPROC); \
    X(glDeleteFramebuffersEXT, PFNGLDELETEFRAMEBUFFERSEXTPROC); \
    X(glDeleteProgram, PFNGLDELETEPROGRAMPROC); \
    X(glDeleteRenderbuffers, PFNGLDELETERENDERBUFFERSPROC); \
    X(glDeleteRenderbuffersEXT, PFNGLDELETERENDERBUFFERSEXTPROC); \
    X(glDeleteShader, PFNGLDELETESHADERPROC); \
    X(glDeleteTextures, PFNGLDELETETEXTURESPROC); \
    X(glDeleteVertexArrays, PFNGLDELETEVERTEXARRAYSPROC); \
    X(glDeleteVertexArraysAPPLE, PFNGLDELETEVERTEXARRAYSAPPLEPROC); \
    X(glDepthFunc, PFNGLDEPTHFUNCPROC); \
    X(glDepthMask, PFNGLDEPTHMASKPROC); \
    X(glDetachShader, PFNGLDETACHSHADERPROC); \
    X(glDisable, PFNGLDISABLEPROC); \
    X(glDisableVertexAttribArray, PFNGLDISABLEVERTEXATTRIBARRAYPROC); \
    X(glDrawElements, PFNGLDRAWELEMENTSPROC); \
    X(glEnable, PFNGLENABLEPROC); \
    X(glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC); \
    X(glFramebufferRenderbuffer, PFNGLFRAMEBUFFERRENDERBUFFERPROC); \
    X(glFramebufferRenderbufferEXT, PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC); \
    X(glFramebufferTexture2D, PFNGLFRAMEBUFFERTEXTURE2DPROC); \
    X(glFramebufferTexture2DEXT, PFNGLFRAMEBUFFERTEXTURE2DEXTPROC); \
    X(glGenBuffers, PFNGLGENBUFFERSPROC); \
    X(glGenFramebuffers, PFNGLGENFRAMEBUFFERSPROC); \
    X(glGenFramebuffersEXT, PFNGLGENFRAMEBUFFERSEXTPROC); \
    X(glGenRenderbuffers, PFNGLGENRENDERBUFFERSPROC); \
    X(glGenRenderbuffersEXT, PFNGLGENRENDERBUFFERSEXTPROC); \
    X(glGenTextures, PFNGLGENTEXTURESPROC); \
    X(glGenVertexArrays, PFNGLGENVERTEXARRAYSPROC); \
    X(glGenVertexArraysAPPLE, PFNGLGENVERTEXARRAYSAPPLEPROC); \
    X(glGetError, PFNGLGETERRORPROC); \
    X(glGetIntegerv, PFNGLGETINTEGERVPROC); \
    X(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC); \
    X(glGetProgramiv, PFNGLGETPROGRAMIVPROC); \
    X(glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC); \
    X(glGetShaderiv, PFNGLGETSHADERIVPROC); \
    X(glGetString, PFNGLGETSTRINGPROC); \
    X(glGetUniformBlockIndex, PFNGLGETUNIFORMBLOCKINDEXPROC); \
    X(glLinkProgram, PFNGLLINKPROGRAMPROC); \
    X(glPixelStorei, PFNGLPIXELSTOREIPROC); \
    X(glPolygonMode, PFNGLPOLYGONMODEPROC); \
    X(glReadPixels, PFNGLREADPIXELSPROC); \
    X(glRenderbufferStorage, PFNGLRENDERBUFFERSTORAGEPROC); \
    X(glRenderbufferStorageEXT, PFNGLRENDERBUFFERSTORAGEEXTPROC); \
    X(glScissor, PFNGLSCISSORPROC); \
    X(glShaderSource, PFNGLSHADERSOURCEPROC); \
    X(glTexImage2D, PFNGLTEXIMAGE2DPROC); \
    X(glTexParameteri, PFNGLTEXPARAMETERIPROC); \
    X(glTexSubImage2D, PFNGLTEXSUBIMAGE2DPROC); \
    X(glUniformBlockBinding, PFNGLUNIFORMBLOCKBINDINGPROC); \
    X(glUseProgram, PFNGLUSEPROGRAMPROC); \
    X(glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC); \
    X(glViewport, PFNGLVIEWPORTPROC)

#define FO_GL_FUNCTION_DEF(name, type) static type name = nullptr
FO_GL_FUNCTIONS(FO_GL_FUNCTION_DEF);
#undef FO_GL_FUNCTION_DEF

template<typename T>
static auto LoadOpenGlFunction(const char* name) noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    const SDL_FunctionPointer function = SDL_GL_GetProcAddress(name);
    return reinterpret_cast<T>(function); // NOLINT(clang-diagnostic-cast-function-type-strict)
}

static void LoadOpenGLFunctions() noexcept
{
#define FO_GL_FUNCTION_LOAD(name, type) name = LoadOpenGlFunction<type>(#name)
    FO_GL_FUNCTIONS(FO_GL_FUNCTION_LOAD);
#undef FO_GL_FUNCTION_LOAD
}
#endif

#if FO_MAC && !FO_OPENGL_ES
#undef glGenVertexArrays
#undef glBindVertexArray
#undef glDeleteVertexArrays
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#define glDeleteVertexArrays glDeleteVertexArraysAPPLE
#endif

#if FO_DEBUG
#define GL_CTX(expr, ctx) \
    do { \
        expr; \
        if ((ctx)->RenderDebug) { \
            GLenum err__ = glGetError(); \
            FO_VERIFY_AND_THROW(err__ == GL_NO_ERROR, #expr " produced OpenGL error", ErrCodeToString(err__)); \
        } \
    } while (0)

#define GL(expr) GL_CTX(expr, GetOpenGlContext(_ctx).get())

static auto ErrCodeToString(GLenum err_code) -> string
{
    FO_STACK_TRACE_ENTRY();

#define ERR_CODE_CASE(err_code_variant) \
    case err_code_variant: \
        return #err_code_variant

    switch (err_code) {
        ERR_CODE_CASE(GL_INVALID_ENUM);
        ERR_CODE_CASE(GL_INVALID_VALUE);
        ERR_CODE_CASE(GL_INVALID_OPERATION);
        ERR_CODE_CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
        ERR_CODE_CASE(GL_OUT_OF_MEMORY);
#if !FO_OPENGL_ES
        ERR_CODE_CASE(GL_STACK_OVERFLOW);
        ERR_CODE_CASE(GL_STACK_UNDERFLOW);
#endif
    default:
        return strex("{:#X}", err_code);
    }

#undef ERR_CODE_CASE
}
#else
#define GL_CTX(expr, ctx) expr
#define GL(expr) expr
#endif

#define GL_HAS_CTX(extension, ctx) ((ctx)->OGL_##extension)
#define GL_HAS(extension) GL_HAS_CTX(extension, GetOpenGlContext(_ctx).get())

struct OpenGL_Renderer::Context
{
    nptr<GlobalSettings> Settings {};
    bool RenderDebug {};
    bool ForceGlslEsProfile {};
    nptr<SDL_Window> SdlWindow {};
    SDL_GLContext GlContext {};
    GLint BaseFrameBufObj {};
    bool BaseFrameBufObjBinded {};
    isize32 BaseFrameBufSize {};
    isize32 TargetSize {};
    mat44 ProjMatrix {};
    float32_t OrthoNear {ORTHO_DEPTH_DEFAULT_NEAR};
    float32_t OrthoFar {ORTHO_DEPTH_DEFAULT_FAR};
    unique_nptr<RenderTexture> DummyTexture {};
    irect32 ViewPortRect {};

    // SetRenderTarget elision cache; invalidated on resize and on destruction of the cached texture
    nptr<RenderTexture> CurrentRenderTarget {};
    bool CurrentRenderTargetValid {};

    // Shared bump-allocated uniform buffer: blocks upload per draw with one glBufferSubData and
    // bind via glBindBufferRange; orphaned once per frame in Present()
    GLuint UniformBumpBuf {};
    size_t UniformBumpOffset {};
    size_t UniformBumpCapacity {};
    GLint UniformOffsetAlignment {1};
    vector<uint8_t> UniformScratch {};

    // ReSharper disable CppInconsistentNaming
    bool OGL_version_2_0 {};
    bool OGL_vertex_buffer_object {};
    bool OGL_framebuffer_object {};
    bool OGL_framebuffer_object_ext {};
    bool OGL_vertex_array_object {};
    bool OGL_uniform_buffer_object {};
    // ReSharper restore CppInconsistentNaming
};

static auto GetOpenGlContext(ptr<OpenGL_Renderer::Context> ctx) noexcept -> ptr<OpenGL_Renderer::Context>
{
    FO_NO_STACK_TRACE_ENTRY();

    return ctx;
}

static auto GetOpenGlContext(const unique_nptr<OpenGL_Renderer::Context>& ctx) -> ptr<const OpenGL_Renderer::Context>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx, "OpenGL renderer context is not initialized");
    return ctx.as_ptr();
}

class OpenGL_Texture final : public RenderTexture
{
public:
    OpenGL_Texture(isize32 size, bool linear_filtered, bool with_depth, ptr<OpenGL_Renderer::Context> ctx) :
        RenderTexture(size, linear_filtered, with_depth),
        _ctx {ctx}
    {
    }
    ~OpenGL_Texture() override;

    [[nodiscard]] auto GetTexturePixel(ipos32 pos) const -> ucolor override;
    [[nodiscard]] auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> override;
    void UpdateTextureRegion(ipos32 pos, isize32 size, const_span<ucolor> data, bool use_dest_pitch) override;

    GLuint FramebufObj {};
    GLuint TexId {};
    GLuint DepthBuffer {};

private:
    ptr<OpenGL_Renderer::Context> _ctx;
};

static auto GetDummyTexture(ptr<OpenGL_Renderer::Context> ctx) -> ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx->DummyTexture, "OpenGL dummy texture is not created");
    auto dummy_texture = ctx->DummyTexture.as_ptr();
    return dummy_texture;
}

class OpenGL_DrawBuffer final : public RenderDrawBuffer
{
public:
    OpenGL_DrawBuffer(bool is_static, ptr<OpenGL_Renderer::Context> ctx);
    ~OpenGL_DrawBuffer() override;

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override;

    GLuint VertexBufObj {};
    GLuint IndexBufObj {};
    GLuint VertexArrObj {};

private:
    ptr<OpenGL_Renderer::Context> _ctx;
};

class OpenGL_Effect final : public RenderEffect
{
    friend class OpenGL_Renderer;

public:
    OpenGL_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, ptr<OpenGL_Renderer::Context> ctx) :
        RenderEffect(usage, name, loader),
        _ctx {ctx}
    {
    }
    ~OpenGL_Effect() override;

    void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override;

    GLuint Program[EFFECT_MAX_PASSES] {};

private:
    ptr<OpenGL_Renderer::Context> _ctx;
};

template<typename T, typename U>
static auto RenderBackendCast(ptr<U> value) -> ptr<T>
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_casted = value.template cast<T>();
    FO_VERIFY_AND_THROW(nullable_casted, "Render backend object is not of the expected OpenGL type");
    return nullable_casted.as_ptr();
}

static auto GetSdlWindow(nptr<WindowInternalHandle> window) -> ptr<SDL_Window>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(window, "Window handle is null");
    return cast_from_void<SDL_Window*>(window.get());
}

static auto GetOpenGlString(GLenum name) noexcept -> nptr<const char>
{
    FO_NO_STACK_TRACE_ENTRY();

    const nptr<const GLubyte> chars = glGetString(name);

    if (!chars) {
        return nullptr;
    }

    return chars.reinterpret_as<char>();
}

static auto OpenGlBufferOffset(size_t offset) noexcept -> nptr<const GLvoid>
{
    FO_NO_STACK_TRACE_ENTRY();

    return reinterpret_cast<const GLvoid*>(offset);
}

#if FO_WEB
static auto WebGlContextHandleAsSdlContext(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context) noexcept -> SDL_GLContext
{
    FO_NO_STACK_TRACE_ENTRY();

    return reinterpret_cast<SDL_GLContext>(context);
}
#endif

OpenGL_Renderer::OpenGL_Renderer() = default;

void OpenGL_Renderer::Init(GlobalSettings& settings, nptr<WindowInternalHandle> window)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!!window, "Frontend window handle is null");
    FO_VERIFY_AND_THROW(!_ctx, "Frontend context is already initialized");
    _ctx = SafeAlloc::MakeUnique<Context>();
    auto ctx = _ctx.as_ptr();

    WriteLog("Used OpenGL rendering");

    ctx->Settings = &settings;
    ctx->RenderDebug = settings.RenderDebug;
    ctx->ForceGlslEsProfile = settings.ForceGlslEsProfile;
    ctx->SdlWindow = GetSdlWindow(window);

    // Create context
#if !FO_WEB
    ctx->GlContext = SDL_GL_CreateContext(ctx->SdlWindow.get());
    FO_VERIFY_AND_THROW(ctx->GlContext, "OpenGL context was not created", SDL_GetError());

    const auto make_current = SDL_GL_MakeCurrent(ctx->SdlWindow.get(), ctx->GlContext);
    FO_VERIFY_AND_THROW(make_current, "OpenGL context could not be made current", SDL_GetError());

    if (settings.VSync) {
        if (!SDL_GL_SetSwapInterval(-1)) {
            SDL_GL_SetSwapInterval(1);
        }
    }
    else {
        SDL_GL_SetSwapInterval(0);
    }

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
    ptr<const char> canvas_selector = WebRelated::CanvasSelector.c_str();
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl_context = emscripten_webgl_create_context(canvas_selector.get(), &attr);
    FO_VERIFY_AND_THROW(gl_context > 0, "WebGL2 context creation failed", static_cast<int32_t>(gl_context));

    EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(gl_context);
    FO_VERIFY_AND_THROW(r >= 0, "WebGL context could not be made current", r);

    ctx->GlContext = WebGlContextHandleAsSdlContext(gl_context);
#endif

    // Load OpenGL function pointers via SDL and detect capabilities from GL_VERSION + extension strings
#if !FO_OPENGL_ES
    LoadOpenGLFunctions();

    // Validate required functions
    {
        string missing_funcs;

        const auto check_loaded = [&](const char* fn_name, bool is_null) {
            const string_view sv = fn_name;
            if (is_null && !sv.ends_with("EXT") && !sv.ends_with("APPLE")) {
                if (!missing_funcs.empty()) {
                    missing_funcs += ", ";
                }
                missing_funcs += fn_name;
            }
        };

#define FO_GL_FUNCTION_VALIDATE(name, type) check_loaded(#name, (name) == nullptr)
        FO_GL_FUNCTIONS(FO_GL_FUNCTION_VALIDATE);
#undef FO_GL_FUNCTION_VALIDATE

        FO_VERIFY_AND_THROW(missing_funcs.empty(), "Required OpenGL entry points are missing", missing_funcs);
    }

    int32_t gl_major = 0;
    int32_t gl_minor = 0;

    const auto version_str = GetOpenGlString(GL_VERSION);

    if (version_str) {
        const auto parts = strvex(version_str.get()).split('.');

        if (parts.size() >= 1) {
            gl_major = numeric_cast<int32_t>(strvex(parts[0]).to_int64());
        }
        if (parts.size() >= 2) {
            gl_minor = numeric_cast<int32_t>(strvex(parts[1]).to_int64());
        }
    }

    const auto has_extension = [](const char* name) noexcept -> bool { return SDL_GL_ExtensionSupported(name); };
    const auto at_least = [&](int32_t major, int32_t minor) noexcept -> bool { return gl_major > major || (gl_major == major && gl_minor >= minor); };

    ctx->OGL_version_2_0 = at_least(2, 0);
    ctx->OGL_vertex_buffer_object = at_least(2, 0) || has_extension("GL_ARB_vertex_buffer_object");
    ctx->OGL_framebuffer_object = at_least(3, 0) || has_extension("GL_ARB_framebuffer_object");
    ctx->OGL_framebuffer_object_ext = has_extension("GL_EXT_framebuffer_object");
#if FO_MAC
    ctx->OGL_vertex_array_object = has_extension("GL_APPLE_vertex_array_object");
#else
    ctx->OGL_vertex_array_object = at_least(3, 0) || has_extension("GL_ARB_vertex_array_object");
#endif
    ctx->OGL_uniform_buffer_object = at_least(3, 1) || has_extension("GL_ARB_uniform_buffer_object");
#endif

    // OpenGL ES extensions
#if FO_OPENGL_ES
    ctx->OGL_version_2_0 = true;
    ctx->OGL_vertex_buffer_object = true;
    ctx->OGL_framebuffer_object = true;
    ctx->OGL_framebuffer_object_ext = false;
    ctx->OGL_vertex_array_object = true; // No in es 2 / webgl 1
    ctx->OGL_uniform_buffer_object = true; // No in es 2 / webgl 1
#endif

    // Check OpenGL extensions
    size_t extension_errors = 0;

    const auto check_extension = [&extension_errors](string_view ext_name, bool has_ext, bool critical) {
        if (!has_ext) {
            const string msg = critical ? "Critical" : "Not critical";
            WriteLog("OpenGL extension '{}' not supported. {}", ext_name, msg);
            if (critical) {
                extension_errors++;
            }
        }
    };

    check_extension("version_2_0", GL_HAS_CTX(version_2_0, ctx.get()), true);
    check_extension("vertex_buffer_object", GL_HAS_CTX(vertex_buffer_object, ctx.get()), true);
    check_extension("uniform_buffer_object", GL_HAS_CTX(uniform_buffer_object, ctx.get()), true);
    check_extension("vertex_array_object", GL_HAS_CTX(vertex_array_object, ctx.get()), false);
    check_extension("framebuffer_object", GL_HAS_CTX(framebuffer_object, ctx.get()), false);
    if (!GL_HAS_CTX(framebuffer_object, ctx.get())) {
        check_extension("framebuffer_object_ext", GL_HAS_CTX(framebuffer_object_ext, ctx.get()), true);
    }
    FO_VERIFY_AND_THROW(!extension_errors, "Extension errors is already set");

    // Map framebuffer_object_ext to framebuffer_object
#if !FO_OPENGL_ES
    if (GL_HAS_CTX(framebuffer_object_ext, ctx.get()) && !GL_HAS_CTX(framebuffer_object, ctx.get())) {
        WriteLog("Map framebuffer_object_ext pointers");
        ctx->OGL_framebuffer_object = true;
        glGenFramebuffers = glGenFramebuffersEXT;
        glGenRenderbuffers = glGenRenderbuffersEXT;
        glBindFramebuffer = glBindFramebufferEXT;
        glBindRenderbuffer = glBindRenderbufferEXT;
        glDeleteFramebuffers = glDeleteFramebuffersEXT;
        glDeleteRenderbuffers = glDeleteRenderbuffersEXT;
        glFramebufferTexture2D = glFramebufferTexture2DEXT;
        glFramebufferRenderbuffer = glFramebufferRenderbufferEXT;
        glRenderbufferStorage = glRenderbufferStorageEXT;
        glCheckFramebufferStatus = glCheckFramebufferStatusEXT;
    }
#endif

    // Render states
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glEnable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glDisable(GL_CULL_FACE));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
#if !FO_OPENGL_ES
    GL(glEnable(GL_TEXTURE_2D));
    GL(glDisable(GL_LIGHTING));
    GL(glDisable(GL_COLOR_MATERIAL));
    GL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
#endif

    GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &ctx->BaseFrameBufObj));
    ctx->BaseFrameBufSize = {settings.ScreenWidth, settings.ScreenHeight};

    // Shared bump-allocated uniform buffer (see the Context field comment)
    if (GL_HAS_CTX(uniform_buffer_object, ctx.get())) {
        GL(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &ctx->UniformOffsetAlignment));
        ctx->UniformOffsetAlignment = std::max(ctx->UniformOffsetAlignment, 1);
        ctx->UniformBumpCapacity = numeric_cast<size_t>(4 * 1024 * 1024); // 4 MB
        GL(glGenBuffers(1, &ctx->UniformBumpBuf));
        GL(glBindBuffer(GL_UNIFORM_BUFFER, ctx->UniformBumpBuf));
        GL(glBufferData(GL_UNIFORM_BUFFER, numeric_cast<GLsizeiptr>(ctx->UniformBumpCapacity), nullptr, GL_DYNAMIC_DRAW));
        GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    }

    // Calculate atlas size
    GLint max_texture_size;
    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size));
    GLint max_viewport_size[2];
    GL(glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size));
    auto atlas_w = std::min(max_texture_size, AppRender::MAX_ATLAS_SIZE);
    auto atlas_h = atlas_w;
    atlas_w = std::min(max_viewport_size[0], atlas_w);
    atlas_h = std::min(max_viewport_size[1], atlas_h);
    FO_VERIFY_AND_THROW(atlas_w >= AppRender::MIN_ATLAS_SIZE, "OpenGL texture atlas width is below the required minimum", AppRender::MIN_ATLAS_SIZE);
    FO_VERIFY_AND_THROW(atlas_h >= AppRender::MIN_ATLAS_SIZE, "OpenGL texture atlas height is below the required minimum", AppRender::MIN_ATLAS_SIZE);
    const_cast<int32_t&>(AppRender::MAX_ATLAS_WIDTH) = atlas_w;
    const_cast<int32_t&>(AppRender::MAX_ATLAS_HEIGHT) = atlas_h;

    // Check max bones
#if FO_ENABLE_3D
#if !FO_OPENGL_ES
    GLint max_uniform_components;
    GL(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_uniform_components));

    if (max_uniform_components < 1024) {
        WriteLog("Warning! GL_MAX_VERTEX_UNIFORM_COMPONENTS is {}", max_uniform_components);
    }
#endif
#endif

    // Dummy texture
    constexpr ucolor dummy_pixel[1] = {ucolor {255, 0, 255, 255}};
    ctx->DummyTexture = CreateTexture({1, 1}, false, false);
    auto dummy_texture = GetDummyTexture(ctx);
    dummy_texture->UpdateTextureRegion({}, {1, 1}, dummy_pixel);

    // Init render target
    SetRenderTarget(nullptr);
}

OpenGL_Renderer::~OpenGL_Renderer()
{
    FO_STACK_TRACE_ENTRY();

    if (!_ctx) {
        return;
    }

    auto ctx = _ctx.as_ptr();

    ctx->DummyTexture.reset();

    // The GL context must still be current for this delete.
    if (ctx->UniformBumpBuf != 0) {
        glDeleteBuffers(1, &ctx->UniformBumpBuf);
        ctx->UniformBumpBuf = 0;
    }

#if !FO_WEB
    if (ctx->GlContext) {
        if (ctx->SdlWindow) {
            SDL_GL_MakeCurrent(ctx->SdlWindow.get(), ctx->GlContext);
        }

        SDL_GL_DestroyContext(ctx->GlContext);
        ctx->GlContext = nullptr;
    }
#else
    ctx->GlContext = nullptr;
#endif

    ctx->Settings = nullptr;
    ctx->SdlWindow = nullptr;
    ctx->BaseFrameBufObj = 0;
    ctx->BaseFrameBufObjBinded = false;
    ctx->BaseFrameBufSize = {};
    ctx->TargetSize = {};
    ctx->ProjMatrix = {};
    ctx->ViewPortRect = {};
    ctx->CurrentRenderTarget = nullptr;
    ctx->CurrentRenderTargetValid = false;
    ctx->UniformBumpOffset = 0;
    ctx->UniformBumpCapacity = 0;
    ctx->UniformScratch.clear();
    ctx->OGL_version_2_0 = false;
    ctx->OGL_vertex_buffer_object = false;
    ctx->OGL_framebuffer_object = false;
    ctx->OGL_framebuffer_object_ext = false;
    ctx->OGL_vertex_array_object = false;
    ctx->OGL_uniform_buffer_object = false;

    _ctx.reset();
}

void OpenGL_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

#if !FO_WEB
    SDL_GL_SwapWindow(ctx->SdlWindow.get());
#endif

    if (const auto err = glGetError(); err != GL_NO_ERROR) {
        throw RenderingException("OpenGL error", err);
    }

    // Rewind the bump buffer with fresh (orphaned) storage; the driver keeps the old one alive
    if (ctx->UniformBumpBuf != 0) {
        GL(glBindBuffer(GL_UNIFORM_BUFFER, ctx->UniformBumpBuf));
        GL(glBufferData(GL_UNIFORM_BUFFER, numeric_cast<GLsizeiptr>(ctx->UniformBumpCapacity), nullptr, GL_DYNAMIC_DRAW));
        GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));
        ctx->UniformBumpOffset = 0;
    }
}

auto OpenGL_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto opengl_tex = SafeAlloc::MakeUnique<OpenGL_Texture>(size, linear_filtered, with_depth, ctx);

    GL(glGenFramebuffers(1, &opengl_tex->FramebufObj));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FramebufObj));
    GL(glGenTextures(1, &opengl_tex->TexId));

    GL(glBindTexture(GL_TEXTURE_2D, opengl_tex->TexId));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filtered ? GL_LINEAR : GL_NEAREST));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filtered ? GL_LINEAR : GL_NEAREST));
#if FO_OPENGL_ES
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
#else
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP));
#endif
    GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width, size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
    GL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opengl_tex->TexId, 0));

    if (with_depth) {
        GLint cur_rb;
        GL(glGetIntegerv(GL_RENDERBUFFER_BINDING, &cur_rb));
        GL(glGenRenderbuffers(1, &opengl_tex->DepthBuffer));
        GL(glBindRenderbuffer(GL_RENDERBUFFER, opengl_tex->DepthBuffer));
        GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, size.width, size.height));
        GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, opengl_tex->DepthBuffer));
        GL(glBindRenderbuffer(GL_RENDERBUFFER, cur_rb));
    }

    GLenum status;
    GL(status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    FO_VERIFY_AND_THROW(status == GL_FRAMEBUFFER_COMPLETE, "OpenGL framebuffer is incomplete", status);

    // Restore the actual current target's framebuffer (creation can happen mid-frame while a
    // texture target is selected)
    if (ctx->CurrentRenderTargetValid && ctx->CurrentRenderTarget) {
        auto cur_target = RenderBackendCast<OpenGL_Texture>(ctx->CurrentRenderTarget.as_ptr());
        GL(glBindFramebuffer(GL_FRAMEBUFFER, cur_target->FramebufObj));
    }
    else {
        GL(glBindFramebuffer(GL_FRAMEBUFFER, ctx->BaseFrameBufObj));
    }

    return std::move(opengl_tex);
}

auto OpenGL_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto opengl_dbuf = SafeAlloc::MakeUnique<OpenGL_DrawBuffer>(is_static, ctx);

    return std::move(opengl_dbuf);
}

auto OpenGL_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto opengl_effect = SafeAlloc::MakeUnique<OpenGL_Effect>(usage, name, loader, ctx);

    for (size_t pass = 0; pass < opengl_effect->_passCount; pass++) {
        string ext = "glsl";

        if constexpr (FO_OPENGL_ES) {
            ext = "glsl_es";
        }
        if (ctx->ForceGlslEsProfile) {
            ext = "glsl_es";
        }

        const string vert_fname = strex("{}.fofx-{}-vert-{}", strex(name).erase_file_extension(), pass + 1, ext);
        string vert_content = loader(vert_fname);
        FO_VERIFY_AND_THROW(!vert_content.empty(), "OpenGL effect vertex shader content is empty after loading", name, pass + 1, vert_fname);
        const string frag_fname = strex("{}.fofx-{}-frag-{}", strex(name).erase_file_extension(), pass + 1, ext);
        string frag_content = loader(frag_fname);
        FO_VERIFY_AND_THROW(!frag_content.empty(), "OpenGL effect fragment shader content is empty after loading", name, pass + 1, frag_fname);

        // Create shaders
        GLuint vs;
        GL(vs = glCreateShader(GL_VERTEX_SHADER));
        nptr<const GLchar> vs_source = vert_content.c_str();
        GL(glShaderSource(vs, 1, vs_source.get_pp(), nullptr));

        GLuint fs;
        GL(fs = glCreateShader(GL_FRAGMENT_SHADER));
        nptr<const GLchar> fs_source = frag_content.c_str();
        GL(glShaderSource(fs, 1, fs_source.get_pp(), nullptr));

        // Info parser
        const auto get_shader_compile_log = [](GLuint shader) -> string {
            string result = "(no info)";
            int32_t len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

            if (len > 0) {
                vector<GLchar> buf;
                buf.resize(len);
                nptr<GLchar> log_buf = buf.data();
                int32_t chars = 0;
                glGetShaderInfoLog(shader, len, &chars, log_buf.get());
                result.assign(log_buf.get(), numeric_cast<size_t>(len));
            }

            return result;
        };

        const auto get_program_compile_log = [](GLuint program) -> string {
            string result = "(no info)";
            int32_t len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

            if (len > 0) {
                vector<GLchar> buf;
                buf.resize(len);
                nptr<GLchar> log_buf = buf.data();
                int32_t chars = 0;
                glGetProgramInfoLog(program, len, &chars, log_buf.get());
                result.assign(log_buf.get(), numeric_cast<size_t>(len));
            }

            return result;
        };

        // Compile vs
        GLint compiled;
        GL(glCompileShader(vs));
        GL(glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled));

        if (compiled == 0) {
            const auto vert_log = get_shader_compile_log(vs);
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            throw EffectLoadException("Vertex shader not compiled", vert_fname, vert_content, vert_log);
        }

        // Compile fs
        GL(glCompileShader(fs));
        GL(glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled));

        if (compiled == 0) {
            const auto frag_log = get_shader_compile_log(fs);
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            throw EffectLoadException("Fragment shader not compiled", frag_fname, frag_content, frag_log);
        }

        // Make program
        GLuint program;
        GL(program = glCreateProgram());
        GL(glAttachShader(program, vs));
        GL(glAttachShader(program, fs));

        GL(glLinkProgram(program));
        GLint linked;
        GL(glGetProgramiv(program, GL_LINK_STATUS, &linked));

        if (linked == 0) {
            const auto program_log = get_program_compile_log(program);
            const auto vert_log = get_shader_compile_log(vs);
            const auto frag_log = get_shader_compile_log(fs);
            GL(glDetachShader(program, vs));
            GL(glDetachShader(program, fs));
            GL(glDeleteShader(vs));
            GL(glDeleteShader(fs));
            GL(glDeleteProgram(program));
            throw EffectLoadException("Failed to link shader program", vert_fname, frag_fname, vert_content, frag_content, vert_log, frag_log, program_log);
        }

        opengl_effect->Program[pass] = program;

        if (GL_HAS(uniform_buffer_object)) {
            const auto bind_ubo_block = [&](const char* block_name, int32_t block_pos) {
                if (block_pos != -1) {
                    if (const GLuint index = glGetUniformBlockIndex(program, block_name); index != GL_INVALID_INDEX) {
                        GL(glUniformBlockBinding(program, index, block_pos));
                    }
                }
            };

            bind_ubo_block("ProjBuf", opengl_effect->_posProjBuf[pass]);
            bind_ubo_block("MainTexBuf", opengl_effect->_posMainTexBuf[pass]);
            bind_ubo_block("SpriteBorderBuf", opengl_effect->_posSpriteBorderBuf[pass]);
            bind_ubo_block("TimeBuf", opengl_effect->_posTimeBuf[pass]);
            bind_ubo_block("RandomValueBuf", opengl_effect->_posRandomValueBuf[pass]);
            bind_ubo_block("ScriptValueBuf", opengl_effect->_posScriptValueBuf[pass]);
            bind_ubo_block("CameraBuf", opengl_effect->_posCameraBuf[pass]);
#if FO_ENABLE_3D
            bind_ubo_block("ModelBuf", opengl_effect->_posModelBuf[pass]);
            bind_ubo_block("ModelTexBuf", opengl_effect->_posModelTexBuf[pass]);
            bind_ubo_block("ModelAnimBuf", opengl_effect->_posModelAnimBuf[pass]);
#endif
        }
    }

    return std::move(opengl_effect);
}

auto OpenGL_Renderer::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    const auto r_l = right - left;
    const auto t_b = top - bottom;
    const auto f_n = farp - nearp;
    const auto tx = -(right + left) / (right - left);
    const auto ty = -(top + bottom) / (top - bottom);
    const auto tz = -(farp + nearp) / (farp - nearp);

    mat44 result {1.0f};

    result[0][0] = 2.0f / r_l;
    result[1][0] = 0.0f;
    result[2][0] = 0.0f;
    result[3][0] = tx;

    result[0][1] = 0.0f;
    result[1][1] = 2.0f / t_b;
    result[2][1] = 0.0f;
    result[3][1] = ty;

    result[0][2] = 0.0f;
    result[1][2] = 0.0f;
    result[2][2] = -2.0f / f_n;
    result[3][2] = tz;

    result[0][3] = 0.0f;
    result[1][3] = 0.0f;
    result[2][3] = 0.0f;
    result[3][3] = 1.0f;

    return result;
}

auto OpenGL_Renderer::GetViewPort() const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    return ctx->ViewPortRect;
}

void OpenGL_Renderer::SetRenderTarget(nptr<RenderTexture> tex)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

    // The requested target is already fully applied (bind, viewport, projection); the projection
    // stays valid across the skip because SetOrthoDepthRange keeps OrthoNear/OrthoFar in sync.
    if (ctx->CurrentRenderTargetValid && tex == ctx->CurrentRenderTarget) {
        return;
    }

    int32_t vp_ox;
    int32_t vp_oy;
    int32_t vp_width;
    int32_t vp_height;
    int32_t screen_width;
    int32_t screen_height;

    if (tex) {
        auto opengl_tex = RenderBackendCast<OpenGL_Texture>(tex.as_ptr());
        GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FramebufObj));
        ctx->BaseFrameBufObjBinded = false;

        vp_ox = 0;
        vp_oy = 0;
        vp_width = opengl_tex->Size.width;
        vp_height = opengl_tex->Size.height;
        screen_width = vp_width;
        screen_height = vp_height;
    }
    else {
        GL(glBindFramebuffer(GL_FRAMEBUFFER, ctx->BaseFrameBufObj));
        ctx->BaseFrameBufObjBinded = true;

        const auto back_buf_aspect = checked_div<float32_t>(numeric_cast<float32_t>(ctx->BaseFrameBufSize.width), numeric_cast<float32_t>(ctx->BaseFrameBufSize.height));
        const auto screen_aspect = checked_div<float32_t>(numeric_cast<float32_t>(ctx->Settings->ScreenWidth), numeric_cast<float32_t>(ctx->Settings->ScreenHeight));
        const auto fit_width = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(ctx->BaseFrameBufSize.height) * screen_aspect : numeric_cast<float32_t>(ctx->BaseFrameBufSize.height) * back_buf_aspect);
        const auto fit_height = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(ctx->BaseFrameBufSize.width) / back_buf_aspect : numeric_cast<float32_t>(ctx->BaseFrameBufSize.width) / screen_aspect);

        vp_ox = (ctx->BaseFrameBufSize.width - fit_width) / 2;
        vp_oy = (ctx->BaseFrameBufSize.height - fit_height) / 2;
        vp_width = fit_width;
        vp_height = fit_height;
        screen_width = ctx->Settings->ScreenWidth;
        screen_height = ctx->Settings->ScreenHeight;
    }

    ctx->ViewPortRect = irect32 {vp_ox, vp_oy, vp_width, vp_height};
    GL(glViewport(vp_ox, vp_oy, vp_width, vp_height));

    ctx->ProjMatrix = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(screen_width), numeric_cast<float32_t>(screen_height), 0.0f, ctx->OrthoNear, ctx->OrthoFar);

    ctx->TargetSize = {screen_width, screen_height};
    ctx->CurrentRenderTarget = tex;
    ctx->CurrentRenderTargetValid = true;
}

void OpenGL_Renderer::SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

    ctx->OrthoNear = nearp;
    ctx->OrthoFar = farp;
    ctx->ProjMatrix = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(ctx->TargetSize.width), numeric_cast<float32_t>(ctx->TargetSize.height), 0.0f, nearp, farp);
}

auto OpenGL_Renderer::GetProjMatrix() const -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    return ctx->ProjMatrix;
}

void OpenGL_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    GLbitfield clear_flags = 0;

    if (color.has_value()) {
        const auto r = numeric_cast<float32_t>(color.value().comp.r) / 255.0f;
        const auto g = numeric_cast<float32_t>(color.value().comp.g) / 255.0f;
        const auto b = numeric_cast<float32_t>(color.value().comp.b) / 255.0f;
        const auto a = numeric_cast<float32_t>(color.value().comp.a) / 255.0f;
        GL(glClearColor(r, g, b, a));
        clear_flags |= GL_COLOR_BUFFER_BIT;
    }

    if (depth) {
        GL(glClearDepthf(1.0f));
        clear_flags |= GL_DEPTH_BUFFER_BIT;
    }

    if (stencil) {
        GL(glClearStencil(0));
        clear_flags |= GL_STENCIL_BUFFER_BIT;
    }

    if (clear_flags != 0) {
        GL(glClear(clear_flags));
    }
}

void OpenGL_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

    int32_t l;
    int32_t t;
    int32_t r;
    int32_t b;

    if (ctx->ViewPortRect.width != ctx->TargetSize.width || ctx->ViewPortRect.height != ctx->TargetSize.height) {
        const float32_t x_ratio = numeric_cast<float32_t>(ctx->ViewPortRect.width) / numeric_cast<float32_t>(ctx->TargetSize.width);
        const float32_t y_ratio = numeric_cast<float32_t>(ctx->ViewPortRect.height) / numeric_cast<float32_t>(ctx->TargetSize.height);

        l = ctx->ViewPortRect.x + iround<int32_t>(numeric_cast<float32_t>(rect.x) * x_ratio);
        t = ctx->ViewPortRect.y + iround<int32_t>(numeric_cast<float32_t>(rect.y) * y_ratio);
        r = ctx->ViewPortRect.x + iround<int32_t>(numeric_cast<float32_t>(rect.x + rect.width) * x_ratio);
        b = ctx->ViewPortRect.y + iround<int32_t>(numeric_cast<float32_t>(rect.y + rect.height) * y_ratio);
    }
    else {
        l = ctx->ViewPortRect.x + rect.x;
        t = ctx->ViewPortRect.y + rect.y;
        r = ctx->ViewPortRect.x + rect.x + rect.width;
        b = ctx->ViewPortRect.y + rect.y + rect.height;
    }

    GL(glEnable(GL_SCISSOR_TEST));
    GL(glScissor(l, ctx->TargetSize.height - b, r - l, b - t));
}

void OpenGL_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    GL(glDisable(GL_SCISSOR_TEST));
}

void OpenGL_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

    ctx->BaseFrameBufSize = size;

    // The back-buffer viewport math depends on the new size; drop the elision cache
    ctx->CurrentRenderTargetValid = false;

    if (ctx->BaseFrameBufObjBinded) {
        SetRenderTarget(nullptr);
    }
}

OpenGL_Texture::~OpenGL_Texture()
{
    FO_STACK_TRACE_ENTRY();

    // A new texture may reuse this address; a stale cache entry would elide its first select
    if (_ctx->CurrentRenderTargetValid && _ctx->CurrentRenderTarget.get() == static_cast<RenderTexture*>(this)) {
        _ctx->CurrentRenderTargetValid = false;
    }

    if (DepthBuffer != 0) {
        glDeleteRenderbuffers(1, &DepthBuffer);
    }
    if (TexId != 0) {
        glDeleteTextures(1, &TexId);
    }
    if (FramebufObj != 0) {
        glDeleteFramebuffers(1, &FramebufObj);
    }
}

auto OpenGL_Texture::GetTexturePixel(ipos32 pos) const -> ucolor
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(Size.is_valid_pos(pos), "Requested OpenGL texture pixel is outside texture bounds", pos, Size);

    ucolor result;

    auto prev_fbo = 0;
    GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, FramebufObj));
    GL(glReadPixels(pos.x, pos.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &result));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo));

    return result;
}

auto OpenGL_Texture::GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Size width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Size height must be positive", size.height);
    FO_VERIFY_AND_THROW(pos.x >= 0, "Position x is negative", pos.x);
    FO_VERIFY_AND_THROW(pos.y >= 0, "Position y is negative", pos.y);
    FO_VERIFY_AND_THROW(pos.x + size.width <= Size.width, "Requested texture read rectangle right edge is outside texture bounds", pos.x, size.width, Size.width);
    FO_VERIFY_AND_THROW(pos.y + size.height <= Size.height, "Requested texture read rectangle bottom edge is outside texture bounds", pos.y, size.height, Size.height);

    vector<ucolor> result;
    result.resize(numeric_cast<size_t>(size.width) * size.height);

    GLint prev_fbo;
    GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, FramebufObj));
    GL(glReadPixels(pos.x, pos.y, size.width, size.height, GL_RGBA, GL_UNSIGNED_BYTE, result.data()));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo));

    return result;
}

void OpenGL_Texture::UpdateTextureRegion(ipos32 pos, isize32 size, const_span<ucolor> data, bool use_dest_pitch)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pos.x >= 0, "Position x is negative", pos.x);
    FO_VERIFY_AND_THROW(pos.y >= 0, "Position y is negative", pos.y);
    FO_VERIFY_AND_THROW(pos.x + size.width <= Size.width, "Texture update rectangle right edge is outside texture bounds", pos.x, size.width, Size.width);
    FO_VERIFY_AND_THROW(pos.y + size.height <= Size.height, "Texture update rectangle bottom edge is outside texture bounds", pos.y, size.height, Size.height);

    const size_t src_pitch = numeric_cast<size_t>(use_dest_pitch ? Size.width : size.width);
    const size_t required_size = size.height != 0 ? (numeric_cast<size_t>(size.height - 1) * src_pitch + numeric_cast<size_t>(size.width)) : 0;
    FO_VERIFY_AND_THROW(data.size() >= required_size, "Texture update source data is smaller than the required region size");

    if (use_dest_pitch) {
        GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, Size.width));
    }

    nptr<const ucolor> source_data = data.data();
    FO_VERIFY_AND_THROW(required_size == 0 || !!source_data, "Texture update source data is null for a non-empty region");

    GL(glBindTexture(GL_TEXTURE_2D, TexId));
    GL(glTexSubImage2D(GL_TEXTURE_2D, 0, pos.x, pos.y, size.width, size.height, GL_RGBA, GL_UNSIGNED_BYTE, source_data.get()));
    GL(glBindTexture(GL_TEXTURE_2D, 0));

    if (use_dest_pitch) {
        GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
    }
}

static void EnableVertAtribs(ptr<OpenGL_Renderer::Context> ctx, EffectUsage usage)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(ctx, usage);

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        GL_CTX(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, Position)).get()), ctx.get());
        GL_CTX(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, Normal)).get()), ctx.get());
        GL_CTX(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, TexCoord)).get()), ctx.get());
        GL_CTX(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, TexCoordBase)).get()), ctx.get());
        GL_CTX(glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, Tangent)).get()), ctx.get());
        GL_CTX(glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, Bitangent)).get()), ctx.get());
        GL_CTX(glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, BlendWeights)).get()), ctx.get());
        GL_CTX(glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, BlendIndices)).get()), ctx.get());
        GL_CTX(glVertexAttribPointer(8, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex3D), OpenGlBufferOffset(offsetof(Vertex3D, Color)).get()), ctx.get());

        for (GLuint i = 0; i <= 8; i++) {
            GL_CTX(glEnableVertexAttribArray(i), ctx.get());
        }

        return;
    }
#endif

    GL_CTX(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), OpenGlBufferOffset(offsetof(Vertex2D, PosX)).get()), ctx.get());
    GL_CTX(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex2D), OpenGlBufferOffset(offsetof(Vertex2D, Color)).get()), ctx.get());
    GL_CTX(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), OpenGlBufferOffset(offsetof(Vertex2D, TexU)).get()), ctx.get());
    GL_CTX(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), OpenGlBufferOffset(offsetof(Vertex2D, EggFlags)).get()), ctx.get());

    for (GLuint i = 0; i <= 3; i++) {
        GL_CTX(glEnableVertexAttribArray(i), ctx.get());
    }
}

static void DisableVertAtribs(ptr<OpenGL_Renderer::Context> ctx, EffectUsage usage)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(ctx, usage);

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        for (GLuint i = 0; i <= 8; i++) {
            GL_CTX(glDisableVertexAttribArray(i), ctx.get());
        }

        return;
    }
#endif

    for (GLuint i = 0; i <= 3; i++) {
        GL_CTX(glDisableVertexAttribArray(i), ctx.get());
    }
}

OpenGL_DrawBuffer::OpenGL_DrawBuffer(bool is_static, ptr<OpenGL_Renderer::Context> ctx) :
    RenderDrawBuffer(is_static),
    _ctx {ctx}
{
    FO_STACK_TRACE_ENTRY();

    GL(glGenBuffers(1, &VertexBufObj));
    GL(glGenBuffers(1, &IndexBufObj));
}

OpenGL_DrawBuffer::~OpenGL_DrawBuffer()
{
    FO_STACK_TRACE_ENTRY();

    if (VertexArrObj != 0) {
        glDeleteVertexArrays(1, &VertexArrObj);
    }

    glDeleteBuffers(1, &VertexBufObj);
    glDeleteBuffers(1, &IndexBufObj);
}

void OpenGL_DrawBuffer::Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size)
{
    FO_STACK_TRACE_ENTRY();

    if (IsStatic && !StaticDataChanged) {
        return;
    }

    StaticDataChanged = false;

    const auto buf_type = IsStatic ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;

    // Fill vertex buffer
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBufObj));

    size_t upload_vertices;

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        FO_VERIFY_AND_THROW(Vertices.empty(), "Vertices must be empty before this operation");
        upload_vertices = custom_vertices_size.value_or(VertCount);
        nptr<const Vertex3D> source_vertices = Vertices3D.data();
        FO_VERIFY_AND_THROW(upload_vertices == 0 || !!source_vertices, "Vertex upload source pointer is null for a non-empty buffer");
        GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex3D), source_vertices.get(), buf_type));
    }
    else {
        FO_VERIFY_AND_THROW(Vertices3D.empty(), "Vertices3 d must be empty before this operation");
        upload_vertices = custom_vertices_size.value_or(VertCount);
        nptr<const Vertex2D> source_vertices = Vertices.data();
        FO_VERIFY_AND_THROW(upload_vertices == 0 || !!source_vertices, "Vertex upload source pointer is null for a non-empty buffer");
        GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex2D), source_vertices.get(), buf_type));
    }
#else
    upload_vertices = custom_vertices_size.value_or(VertCount);
    nptr<const Vertex2D> source_vertices = Vertices.data();
    FO_VERIFY_AND_THROW(upload_vertices == 0 || !!source_vertices, "Vertex upload source pointer is null for a non-empty buffer");
    GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex2D), source_vertices.get(), buf_type));
#endif

    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // Fill index buffer
    const auto upload_indices = custom_indices_size.value_or(IndCount);

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufObj));
    nptr<const vindex_t> source_indices = Indices.data();
    FO_VERIFY_AND_THROW(upload_indices == 0 || !!source_indices, "Index upload source pointer is null for a non-empty buffer");
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, upload_indices * sizeof(vindex_t), source_indices.get(), buf_type));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    // Vertex array
    if (VertexArrObj == 0 && GL_HAS(vertex_array_object)) {
        GL(glGenVertexArrays(1, &VertexArrObj));
        GL(glBindVertexArray(VertexArrObj));
        GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBufObj));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufObj));
        EnableVertAtribs(_ctx, usage);
        GL(glBindVertexArray(0));
        GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }
}

static auto ConvertBlendFunc(BlendFuncType name) -> GLenum
{
    FO_STACK_TRACE_ENTRY();

    switch (name) {
    case BlendFuncType::Zero:
        return 0;
    case BlendFuncType::One:
        return 1;
    case BlendFuncType::SrcColor:
        return 0x0300;
    case BlendFuncType::InvSrcColor:
        return 0x0301;
    case BlendFuncType::DstColor:
        return 0x0306;
    case BlendFuncType::InvDstColor:
        return 0x0307;
    case BlendFuncType::SrcAlpha:
        return 0x0302;
    case BlendFuncType::InvSrcAlpha:
        return 0x0303;
    case BlendFuncType::DstAlpha:
        return 0x0304;
    case BlendFuncType::InvDstAlpha:
        return 0x0305;
    case BlendFuncType::ConstantColor:
        return 0x8001;
    case BlendFuncType::InvConstantColor:
        return 0x8002;
    case BlendFuncType::SrcAlphaSaturate:
        return 0x0308;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ConvertBlendEquation(BlendEquationType name) -> GLenum
{
    FO_STACK_TRACE_ENTRY();

    switch (name) {
    case BlendEquationType::FuncAdd:
        return 0x8006;
    case BlendEquationType::FuncSubtract:
        return 0x800A;
    case BlendEquationType::FuncReverseSubtract:
        return 0x800B;
    case BlendEquationType::Max:
        return 0x8008;
    case BlendEquationType::Min:
        return 0x8007;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ConvertDepthFunc(DepthFuncType name) -> GLenum
{
    FO_STACK_TRACE_ENTRY();

    switch (name) {
    case DepthFuncType::Always:
        return GL_ALWAYS;
    case DepthFuncType::Never:
        return GL_NEVER;
    case DepthFuncType::Less:
        return GL_LESS;
    case DepthFuncType::LessEqual:
        return GL_LEQUAL;
    case DepthFuncType::Equal:
        return GL_EQUAL;
    case DepthFuncType::GreaterEqual:
        return GL_GEQUAL;
    case DepthFuncType::Greater:
        return GL_GREATER;
    case DepthFuncType::NotEqual:
        return GL_NOTEQUAL;
    }

    FO_UNREACHABLE_PLACE();
}

OpenGL_Effect::~OpenGL_Effect()
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _passCount; i++) {
        if (Program[i] != 0) {
            glDeleteProgram(Program[i]);
        }
    }
}

void OpenGL_Effect::DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    auto opengl_dbuf = RenderBackendCast<OpenGL_DrawBuffer>(dbuf);

#if FO_ENABLE_3D
    if (!custom_tex && ModelTex[0]) {
        custom_tex = ModelTex[0];
    }
#endif
    if (!custom_tex && MainTex) {
        custom_tex = MainTex;
    }

    auto main_tex_source = custom_tex ? custom_tex.as_ptr() : GetDummyTexture(_ctx);
    auto main_tex = RenderBackendCast<const OpenGL_Texture>(main_tex_source);

    GLenum draw_mode = GL_TRIANGLES;

    if (_usage == EffectUsage::Primitive) {
        switch (opengl_dbuf->PrimType) {
        case RenderPrimitiveType::PointList:
            draw_mode = GL_POINTS;
            break;
        case RenderPrimitiveType::LineList:
            draw_mode = GL_LINES;
            break;
        case RenderPrimitiveType::LineStrip:
            draw_mode = GL_LINE_STRIP;
            break;
        case RenderPrimitiveType::TriangleList:
            draw_mode = GL_TRIANGLES;
            break;
        case RenderPrimitiveType::TriangleStrip:
            draw_mode = GL_TRIANGLE_STRIP;
            break;
        }
    }

    if (DisableBlending) {
        GL(glDisable(GL_BLEND));
    }

#if FO_ENABLE_3D
    if (_usage == EffectUsage::Model) {
        GL(glEnable(GL_DEPTH_TEST));

        if (!DisableCulling) {
            GL(glEnable(GL_CULL_FACE));
        }
    }
#endif

    if (_usage == EffectUsage::QuadSprite) {
        GL(glEnable(GL_DEPTH_TEST));
    }

    if (opengl_dbuf->VertexArrObj != 0) {
        GL(glBindVertexArray(opengl_dbuf->VertexArrObj));
    }
    else {
        GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_dbuf->VertexBufObj));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_dbuf->IndexBufObj));
        EnableVertAtribs(_ctx, _usage);
    }

    // Uniforms
    if (_needProjBuf && !ProjBuf.has_value()) {
        auto& proj_buf = ProjBuf = ProjBuffer();
        ptr<float32_t> projection_matrix = proj_buf->ProjMatrix;
        ptr<const float32_t> projection_matrix_values = glm::value_ptr(_ctx->ProjMatrix);
        MemCopy(projection_matrix, projection_matrix_values, 16 * sizeof(float32_t));
    }

    if (_needMainTexBuf && !MainTexBuf.has_value()) {
        auto& main_tex_buf = MainTexBuf = MainTexBuffer();
        ptr<float32_t> main_texture_size = main_tex_buf->MainTexSize;
        ptr<const float32_t> main_texture_size_data = main_tex->SizeData;
        MemCopy(main_texture_size, main_texture_size_data, 4 * sizeof(float32_t));
    }

    // Every shader-required block must be written and bound EVERY draw: a stale binding would
    // point into the shared bump buffer, whose storage dies at the per-frame orphan — so
    // default-initialize any required-but-unset buffer to zero (mirrors the Vulkan backend).
    if (_needEggBuf && !EggBuf.has_value()) {
        EggBuf = EggBuffer();
    }
    if (_needSpriteBorderBuf && !SpriteBorderBuf.has_value()) {
        SpriteBorderBuf = SpriteBorderBuffer();
    }
    if (_needTimeBuf && !TimeBuf.has_value()) {
        TimeBuf = TimeBuffer();
    }
    if (_needRandomValueBuf && !RandomValueBuf.has_value()) {
        RandomValueBuf = RandomValueBuffer();
    }
    if (_needScriptValueBuf && !ScriptValueBuf.has_value()) {
        ScriptValueBuf = ScriptValueBuffer();
    }
    if (_needCameraBuf && !CameraBuf.has_value()) {
        CameraBuf = CameraBuffer();
    }
#if FO_ENABLE_3D
    if (_needModelBuf && !ModelBuf.has_value()) {
        ModelBuf = ModelBuffer();
    }
    if (_needModelTexBuf && !ModelTexBuf.has_value()) {
        ModelTexBuf = ModelTexBuffer();
    }
    if (_needModelAnimBuf && !ModelAnimBuf.has_value()) {
        ModelAnimBuf = ModelAnimBuffer();
    }
#endif

    constexpr size_t max_uniform_blocks = 11;
    size_t block_offsets[max_uniform_blocks] = {};
    size_t block_sizes[max_uniform_blocks] = {};

    if (GL_HAS(uniform_buffer_object)) {
        const size_t alignment = numeric_cast<size_t>(_ctx->UniformOffsetAlignment);
        auto& scratch = _ctx->UniformScratch;
        scratch.clear();
        size_t block_index = 0;

        const auto gather_block = [&](bool need_buf, auto& buf, bool reset_buf) {
            FO_VERIFY_AND_THROW(block_index < max_uniform_blocks, "Too many OpenGL uniform blocks in draw");

            if (!need_buf || !buf.has_value()) {
                block_index++;
                return;
            }

            const auto& buf_value = buf.value();
            const size_t aligned_offset = (scratch.size() + alignment - 1) & ~(alignment - 1);
            scratch.resize(aligned_offset + sizeof(buf_value));
            MemCopy(scratch.data() + aligned_offset, &buf_value, sizeof(buf_value));
            block_offsets[block_index] = aligned_offset;
            block_sizes[block_index] = sizeof(buf_value);
            block_index++;

            if (reset_buf) {
                buf.reset();
            }
        };

        gather_block(_needProjBuf, ProjBuf, true);
        gather_block(_needMainTexBuf, MainTexBuf, true);
        gather_block(_needEggBuf, EggBuf, true);
        gather_block(_needSpriteBorderBuf, SpriteBorderBuf, true);
        gather_block(_needTimeBuf, TimeBuf, true);
        gather_block(_needRandomValueBuf, RandomValueBuf, true);
        gather_block(_needScriptValueBuf, ScriptValueBuf, false);
        gather_block(_needCameraBuf, CameraBuf, true);
#if FO_ENABLE_3D
        gather_block(_needModelBuf, ModelBuf, true);
        gather_block(_needModelTexBuf, ModelTexBuf, true);
        gather_block(_needModelAnimBuf, ModelAnimBuf, true);
#endif

        if (!scratch.empty()) {
            // Rewind and re-specify (orphan) the bump storage when the draw does not fit — the
            // driver keeps the old storage alive for the already-issued draws that reference it.
            size_t base_offset = (_ctx->UniformBumpOffset + alignment - 1) & ~(alignment - 1);

            if (base_offset + scratch.size() > _ctx->UniformBumpCapacity) {
                GL(glBindBuffer(GL_UNIFORM_BUFFER, _ctx->UniformBumpBuf));
                GL(glBufferData(GL_UNIFORM_BUFFER, numeric_cast<GLsizeiptr>(_ctx->UniformBumpCapacity), nullptr, GL_DYNAMIC_DRAW));
                base_offset = 0;
            }
            else {
                GL(glBindBuffer(GL_UNIFORM_BUFFER, _ctx->UniformBumpBuf));
            }

            GL(glBufferSubData(GL_UNIFORM_BUFFER, numeric_cast<GLintptr>(base_offset), numeric_cast<GLsizeiptr>(scratch.size()), scratch.data()));
            GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));
            _ctx->UniformBumpOffset = base_offset + scratch.size();

            for (size_t i = 0; i < max_uniform_blocks; i++) {
                if (block_sizes[i] != 0) {
                    block_offsets[i] += base_offset;
                }
            }
        }
    }

    const auto draw_count = numeric_cast<GLsizei>(indices_to_draw.value_or(opengl_dbuf->IndCount));
    const size_t start_offset = start_index * sizeof(vindex_t);

    for (size_t pass = 0; pass < _passCount; pass++) {
#if FO_ENABLE_3D
        if (DisableShadow && _isShadow[pass]) {
            continue;
        }
#endif

        if (GL_HAS(uniform_buffer_object)) {
            size_t bind_block_index = 0;

            const auto bind_block = [&](int32_t pos) {
                const size_t block_index = bind_block_index;
                bind_block_index++;

                if (block_sizes[block_index] != 0 && pos != -1) {
                    GL(glBindBufferRange(GL_UNIFORM_BUFFER, pos, _ctx->UniformBumpBuf, numeric_cast<GLintptr>(block_offsets[block_index]), numeric_cast<GLsizeiptr>(block_sizes[block_index])));
                }
            };

            bind_block(_posProjBuf[pass]);
            bind_block(_posMainTexBuf[pass]);
            bind_block(_posEggBuf[pass]);
            bind_block(_posSpriteBorderBuf[pass]);
            bind_block(_posTimeBuf[pass]);
            bind_block(_posRandomValueBuf[pass]);
            bind_block(_posScriptValueBuf[pass]);
            bind_block(_posCameraBuf[pass]);
#if FO_ENABLE_3D
            bind_block(_posModelBuf[pass]);
            bind_block(_posModelTexBuf[pass]);
            bind_block(_posModelAnimBuf[pass]);
#endif
        }

        GL(glUseProgram(Program[pass]));

        if (_posMainTex[pass] != -1) {
            GL(glActiveTexture(GL_TEXTURE0 + _posMainTex[pass]));
            GL(glBindTexture(GL_TEXTURE_2D, main_tex->TexId));
            GL(glActiveTexture(GL_TEXTURE0));
        }

        if (_posIndoorMaskTex[pass] != -1) {
            auto indoor_tex_source = IndoorMaskTex ? IndoorMaskTex.as_ptr() : GetDummyTexture(_ctx);
            auto indoor_tex = RenderBackendCast<const OpenGL_Texture>(indoor_tex_source);
            GL(glActiveTexture(GL_TEXTURE0 + _posIndoorMaskTex[pass]));
            GL(glBindTexture(GL_TEXTURE_2D, indoor_tex->TexId));
            GL(glActiveTexture(GL_TEXTURE0));
        }

#if FO_ENABLE_3D
        if (_needModelTex[pass]) {
            for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
                if (_posModelTex[pass][i] != -1) {
                    auto model_tex_source = GetDummyTexture(_ctx);
                    if (ModelTex[i]) {
                        model_tex_source = ModelTex[i].as_ptr();
                    }
                    auto model_tex = RenderBackendCast<OpenGL_Texture>(model_tex_source);
                    GL(glActiveTexture(GL_TEXTURE0 + _posModelTex[pass][i]));
                    GL(glBindTexture(GL_TEXTURE_2D, model_tex->TexId));
                    GL(glActiveTexture(GL_TEXTURE0));
                }
            }
        }
#endif

        if (_srcBlendFunc[pass] != BlendFuncType::SrcAlpha || _destBlendFunc[pass] != BlendFuncType::InvSrcAlpha) {
            GL(glBlendFunc(ConvertBlendFunc(_srcBlendFunc[pass]), ConvertBlendFunc(_destBlendFunc[pass])));
        }
        if (_blendEquation[pass] != BlendEquationType::FuncAdd) {
            GL(glBlendEquation(ConvertBlendEquation(_blendEquation[pass])));
        }
        if (!_depthWrite[pass]) {
            GL(glDepthMask(GL_FALSE));
        }
        if (
#if FO_ENABLE_3D
            _usage == EffectUsage::Model ||
#endif
            _usage == EffectUsage::QuadSprite) {
            GL(glDepthFunc(ConvertDepthFunc(_depthFunc[pass])));
        }

        if constexpr (sizeof(vindex_t) == 2) {
            GL(glDrawElements(draw_mode, draw_count, GL_UNSIGNED_SHORT, OpenGlBufferOffset(start_offset).get()));
        }
        else {
            GL(glDrawElements(draw_mode, draw_count, GL_UNSIGNED_INT, OpenGlBufferOffset(start_offset).get()));
        }

        if (_srcBlendFunc[pass] != BlendFuncType::SrcAlpha || _destBlendFunc[pass] != BlendFuncType::InvSrcAlpha) {
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        }
        if (_blendEquation[pass] != BlendEquationType::FuncAdd) {
            GL(glBlendEquation(GL_FUNC_ADD));
        }
        if (!_depthWrite[pass]) {
            GL(glDepthMask(GL_TRUE));
        }

        if (GL_HAS(uniform_buffer_object)) {
            size_t unbind_block_index = 0;

            const auto unbind_block = [&](int32_t pos) {
                const size_t block_index = unbind_block_index;
                unbind_block_index++;

                if (block_sizes[block_index] != 0 && pos != -1) {
                    GL(glBindBufferBase(GL_UNIFORM_BUFFER, pos, 0));
                }
            };

            unbind_block(_posProjBuf[pass]);
            unbind_block(_posMainTexBuf[pass]);
            unbind_block(_posEggBuf[pass]);
            unbind_block(_posSpriteBorderBuf[pass]);
            unbind_block(_posTimeBuf[pass]);
            unbind_block(_posRandomValueBuf[pass]);
            unbind_block(_posScriptValueBuf[pass]);
            unbind_block(_posCameraBuf[pass]);
#if FO_ENABLE_3D
            unbind_block(_posModelBuf[pass]);
            unbind_block(_posModelTexBuf[pass]);
            unbind_block(_posModelAnimBuf[pass]);
#endif
        }
    }

    GL(glUseProgram(0));

    if (opengl_dbuf->VertexArrObj != 0) {
        GL(glBindVertexArray(0));
    }
    else {
        DisableVertAtribs(_ctx, _usage);
        GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }

    if (DisableBlending) {
        GL(glEnable(GL_BLEND));
    }

#if FO_ENABLE_3D
    if (_usage == EffectUsage::Model) {
        GL(glDepthFunc(GL_LESS)); // Restore default depth comparison
        GL(glDisable(GL_DEPTH_TEST));

        if (!DisableCulling) {
            GL(glDisable(GL_CULL_FACE));
        }
    }
#endif

    if (_usage == EffectUsage::QuadSprite) {
        GL(glDepthFunc(GL_LESS)); // Restore default depth comparison
        GL(glDisable(GL_DEPTH_TEST));
    }
}

FO_END_NAMESPACE

#endif
