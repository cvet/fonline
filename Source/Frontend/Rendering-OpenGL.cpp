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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "SDL3/SDL.h"
#include "SDL3/SDL_video.h"

#if !FO_OPENGL_ES
#include "GL/glew.h"
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

FO_BEGIN_NAMESPACE();

#if FO_MAC && !FO_OPENGL_ES
#undef glGenVertexArrays
#undef glBindVertexArray
#undef glDeleteVertexArrays
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#define glDeleteVertexArrays glDeleteVertexArraysAPPLE
#endif

#if FO_DEBUG
#define GL(expr) \
    do { \
        expr; \
        if (RenderDebug) { \
            GLenum err__ = glGetError(); \
            FO_RUNTIME_ASSERT_STR(err__ == GL_NO_ERROR, strex(#expr " error {}", ErrCodeToString(err__))); \
        } \
    } while (0)

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
#define GL(expr) expr
#endif

#define GL_HAS(extension) (OGL_##extension)

static raw_ptr<GlobalSettings> Settings {};
static bool RenderDebug {};
static bool ForceGlslEsProfile {};
static raw_ptr<SDL_Window> SdlWindow {};
static SDL_GLContext GlContext {};
static GLint BaseFrameBufObj {};
static bool BaseFrameBufObjBinded {};
static isize32 BaseFrameBufSize {};
static isize32 TargetSize {};
static mat44 ProjectionMatrixColMaj {};
static unique_ptr<RenderTexture> DummyTexture {};
static irect32 ViewPortRect {};

// ReSharper disable CppInconsistentNaming
static bool OGL_version_2_0 {};
static bool OGL_vertex_buffer_object {};
static bool OGL_framebuffer_object {};
static bool OGL_framebuffer_object_ext {};
static bool OGL_vertex_array_object {};
static bool OGL_uniform_buffer_object {}; // Todo: make workarounds for work without ARB_uniform_buffer_object
// ReSharper restore CppInconsistentNaming

class OpenGL_Texture final : public RenderTexture
{
public:
    OpenGL_Texture(isize32 size, bool linear_filtered, bool with_depth) :
        RenderTexture(size, linear_filtered, with_depth)
    {
    }
    ~OpenGL_Texture() override;

    [[nodiscard]] auto GetTexturePixel(ipos32 pos) const -> ucolor override;
    [[nodiscard]] auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> override;
    void UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch) override;

    GLuint FramebufObj {};
    GLuint TexId {};
    GLuint DepthBuffer {};
};

class OpenGL_DrawBuffer final : public RenderDrawBuffer
{
public:
    explicit OpenGL_DrawBuffer(bool is_static);
    ~OpenGL_DrawBuffer() override;

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override;

    GLuint VertexBufObj {};
    GLuint IndexBufObj {};
    GLuint VertexArrObj {};
};

class OpenGL_Effect final : public RenderEffect
{
    friend class OpenGL_Renderer;

public:
    OpenGL_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
        RenderEffect(usage, name, loader)
    {
    }
    ~OpenGL_Effect() override;

    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex) override;

    GLuint Program[EFFECT_MAX_PASSES] {};

    GLuint Ubo_ProjBuf {};
    GLuint Ubo_MainTexBuf {};
    GLuint Ubo_ContourBuf {};
    GLuint Ubo_TimeBuf {};
    GLuint Ubo_RandomValueBuf {};
    GLuint Ubo_ScriptValueBuf {};
#if FO_ENABLE_3D
    GLuint Ubo_ModelBuf {};
    GLuint Ubo_ModelTexBuf {};
    GLuint Ubo_ModelAnimBuf {};
#endif
};

void OpenGL_Renderer::Init(GlobalSettings& settings, WindowInternalHandle* window)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Used OpenGL rendering");

    Settings = &settings;
    RenderDebug = settings.RenderDebug;
    ForceGlslEsProfile = settings.ForceGlslEsProfile;
    SdlWindow = static_cast<SDL_Window*>(window);

    // Create context
#if !FO_WEB
    GlContext = SDL_GL_CreateContext(SdlWindow.get());
    FO_RUNTIME_ASSERT_STR(GlContext, strex("OpenGL context not created, error '{}'", SDL_GetError()));

    const auto make_current = SDL_GL_MakeCurrent(SdlWindow.get(), GlContext);
    FO_RUNTIME_ASSERT_STR(make_current, strex("Can't set current context, error '{}'", SDL_GetError()));

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
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl_context = emscripten_webgl_create_context("#canvas", &attr);
    FO_RUNTIME_ASSERT_STR(gl_context > 0, strex("Failed to create WebGL2 context, error {}", static_cast<int32>(gl_context)));

    EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(gl_context);
    FO_RUNTIME_ASSERT_STR(r >= 0, strex("Can't set current context, error {}", r));

    GlContext = reinterpret_cast<SDL_GLContext>(gl_context);
#endif

    // Initialize GLEW
    // Todo: remove GLEW and bind OpenGL functions manually
#if !FO_OPENGL_ES
    const auto glew_result = glewInit();
    FO_RUNTIME_ASSERT_STR(glew_result == GLEW_OK, strex("GLEW not initialized, result {}", glew_result));
    OGL_version_2_0 = GLEW_VERSION_2_0 != 0;
    OGL_vertex_buffer_object = GLEW_ARB_vertex_buffer_object != 0; // >= 2.0
    OGL_framebuffer_object = GLEW_ARB_framebuffer_object != 0; // >= 3.0
    OGL_framebuffer_object_ext = GLEW_EXT_framebuffer_object != 0;
#if FO_MAC
    OGL_vertex_array_object = GLEW_APPLE_vertex_array_object != 0;
#else
    OGL_vertex_array_object = GLEW_ARB_vertex_array_object != 0; // >= 3.0
#endif
    OGL_uniform_buffer_object = GLEW_ARB_uniform_buffer_object != 0; // >= 3.1
#endif

    // OpenGL ES extensions
#if FO_OPENGL_ES
    OGL_version_2_0 = true;
    OGL_vertex_buffer_object = true;
    OGL_framebuffer_object = true;
    OGL_framebuffer_object_ext = false;
    OGL_vertex_array_object = true; // No in es 2 / webgl 1
    OGL_uniform_buffer_object = true; // No in es 2 / webgl 1
#endif

    // Check OpenGL extensions
#define CHECK_EXTENSION(ext, critical) \
    if (!GL_HAS(ext)) { \
        string msg = (critical) ? "Critical" : "Not critical"; \
        WriteLog("OpenGL extension '" #ext "' not supported. {}", msg); \
        if (critical) { \
            extension_errors++; \
        } \
    }
    size_t extension_errors = 0;
    CHECK_EXTENSION(version_2_0, true);
    CHECK_EXTENSION(vertex_buffer_object, true);
    CHECK_EXTENSION(uniform_buffer_object, true);
    CHECK_EXTENSION(vertex_array_object, false);
    CHECK_EXTENSION(framebuffer_object, false);
    if (!GL_HAS(framebuffer_object)) {
        CHECK_EXTENSION(framebuffer_object_ext, true);
    }
    FO_RUNTIME_ASSERT(!extension_errors);
#undef CHECK_EXTENSION

    // Map framebuffer_object_ext to framebuffer_object
#if !FO_OPENGL_ES
    if (GL_HAS(framebuffer_object_ext) && !GL_HAS(framebuffer_object)) {
        WriteLog("Map framebuffer_object_ext pointers");
        OGL_framebuffer_object = true;
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

    GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &BaseFrameBufObj));
    BaseFrameBufSize = {settings.ScreenWidth, settings.ScreenHeight};

    // Calculate atlas size
    GLint max_texture_size;
    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size));
    GLint max_viewport_size[2];
    GL(glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size));
    auto atlas_w = std::min(max_texture_size, AppRender::MAX_ATLAS_SIZE);
    auto atlas_h = atlas_w;
    atlas_w = std::min(max_viewport_size[0], atlas_w);
    atlas_h = std::min(max_viewport_size[1], atlas_h);
    FO_RUNTIME_ASSERT_STR(atlas_w >= AppRender::MIN_ATLAS_SIZE, strex("Min texture width must be at least {}", AppRender::MIN_ATLAS_SIZE));
    FO_RUNTIME_ASSERT_STR(atlas_h >= AppRender::MIN_ATLAS_SIZE, strex("Min texture height must be at least {}", AppRender::MIN_ATLAS_SIZE));
    const_cast<int32&>(AppRender::MAX_ATLAS_WIDTH) = atlas_w;
    const_cast<int32&>(AppRender::MAX_ATLAS_HEIGHT) = atlas_h;

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
    DummyTexture = CreateTexture({1, 1}, false, false);
    DummyTexture->UpdateTextureRegion({}, {1, 1}, dummy_pixel);

    // Init render target
    SetRenderTarget(nullptr);
}

void OpenGL_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

#if !FO_WEB
    SDL_GL_SwapWindow(SdlWindow.get());
#endif

    if (const auto err = glGetError(); err != GL_NO_ERROR) {
        throw RenderingException("OpenGL error", err);
    }
}

auto OpenGL_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    auto opengl_tex = SafeAlloc::MakeUnique<OpenGL_Texture>(size, linear_filtered, with_depth);

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
    FO_RUNTIME_ASSERT_STR(status == GL_FRAMEBUFFER_COMPLETE, strex("Framebuffer not created, status {:#X}", status));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, BaseFrameBufObj));

    return std::move(opengl_tex);
}

auto OpenGL_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    auto opengl_dbuf = SafeAlloc::MakeUnique<OpenGL_DrawBuffer>(is_static);

    return std::move(opengl_dbuf);
}

auto OpenGL_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    auto opengl_effect = SafeAlloc::MakeUnique<OpenGL_Effect>(usage, name, loader);

    for (size_t pass = 0; pass < opengl_effect->_passCount; pass++) {
        string ext = "glsl";

        if constexpr (FO_OPENGL_ES) {
            ext = "glsl_es";
        }
        if (ForceGlslEsProfile) {
            ext = "glsl_es";
        }

        const string vert_fname = strex("{}.fofx-{}-vert-{}", strex(name).erase_file_extension(), pass + 1, ext);
        string vert_content = loader(vert_fname);
        FO_RUNTIME_ASSERT(!vert_content.empty());
        const string frag_fname = strex("{}.fofx-{}-frag-{}", strex(name).erase_file_extension(), pass + 1, ext);
        string frag_content = loader(frag_fname);
        FO_RUNTIME_ASSERT(!frag_content.empty());

        // Create shaders
        GLuint vs;
        GL(vs = glCreateShader(GL_VERTEX_SHADER));
        const GLchar* vs_str = vert_content.c_str();
        GL(glShaderSource(vs, 1, &vs_str, nullptr));

        GLuint fs;
        GL(fs = glCreateShader(GL_FRAGMENT_SHADER));
        const GLchar* fs_str = frag_content.c_str();
        GL(glShaderSource(fs, 1, &fs_str, nullptr));

        // Info parser
        const auto get_shader_compile_log = [](GLuint shader) -> string {
            string result = "(no info)";
            int32 len = 0;
            GL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len));

            if (len > 0) {
                vector<GLchar> buf;
                buf.resize(len);
                int32 chars = 0;
                GL(glGetShaderInfoLog(shader, len, &chars, buf.data()));
                result.assign(buf.data(), len);
            }

            return result;
        };

        const auto get_program_compile_log = [](GLuint program) -> string {
            string result = "(no info)";
            int32 len = 0;
            GL(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len));

            if (len > 0) {
                vector<GLchar> buf;
                int32 chars = 0;
                GL(glGetProgramInfoLog(program, len, &chars, buf.data()));
                result.assign(buf.data(), len);
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
#define UBO_BLOCK_BINDING(buf) \
    if (const auto index = glGetUniformBlockIndex(program, #buf); index != GL_INVALID_INDEX) { \
        GL(glUniformBlockBinding(program, index, opengl_effect->_pos##buf[pass])); \
    }
            UBO_BLOCK_BINDING(ProjBuf);
            UBO_BLOCK_BINDING(MainTexBuf);
            UBO_BLOCK_BINDING(ContourBuf);
            UBO_BLOCK_BINDING(TimeBuf);
            UBO_BLOCK_BINDING(RandomValueBuf);
            UBO_BLOCK_BINDING(ScriptValueBuf);
#if FO_ENABLE_3D
            UBO_BLOCK_BINDING(ModelBuf);
            UBO_BLOCK_BINDING(ModelTexBuf);
            UBO_BLOCK_BINDING(ModelAnimBuf);
#endif
#undef UBO_BLOCK_BINDING
        }
    }

    return std::move(opengl_effect);
}

auto OpenGL_Renderer::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    const auto r_l = right - left;
    const auto t_b = top - bottom;
    const auto f_n = farp - nearp;
    const auto tx = -(right + left) / (right - left);
    const auto ty = -(top + bottom) / (top - bottom);
    const auto tz = -(farp + nearp) / (farp - nearp);

    mat44 result;

    result.a1 = 2.0f / r_l;
    result.a2 = 0.0f;
    result.a3 = 0.0f;
    result.a4 = tx;

    result.b1 = 0.0f;
    result.b2 = 2.0f / t_b;
    result.b3 = 0.0f;
    result.b4 = ty;

    result.c1 = 0.0f;
    result.c2 = 0.0f;
    result.c3 = -2.0f / f_n;
    result.c4 = tz;

    result.d1 = 0.0f;
    result.d2 = 0.0f;
    result.d3 = 0.0f;
    result.d4 = 1.0f;

    return result;
}

auto OpenGL_Renderer::GetViewPort() -> irect32
{
    FO_STACK_TRACE_ENTRY();

    return ViewPortRect;
}

void OpenGL_Renderer::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    int32 vp_ox;
    int32 vp_oy;
    int32 vp_width;
    int32 vp_height;
    int32 screen_width;
    int32 screen_height;

    if (tex != nullptr) {
        const auto* opengl_tex = static_cast<OpenGL_Texture*>(tex);
        GL(glBindFramebuffer(GL_FRAMEBUFFER, opengl_tex->FramebufObj));
        BaseFrameBufObjBinded = false;

        vp_ox = 0;
        vp_oy = 0;
        vp_width = opengl_tex->Size.width;
        vp_height = opengl_tex->Size.height;
        screen_width = vp_width;
        screen_height = vp_height;
    }
    else {
        GL(glBindFramebuffer(GL_FRAMEBUFFER, BaseFrameBufObj));
        BaseFrameBufObjBinded = true;

        const auto back_buf_aspect = checked_div<float32>(numeric_cast<float32>(BaseFrameBufSize.width), numeric_cast<float32>(BaseFrameBufSize.height));
        const auto screen_aspect = checked_div<float32>(numeric_cast<float32>(Settings->ScreenWidth), numeric_cast<float32>(Settings->ScreenHeight));
        const auto fit_width = iround<int32>(screen_aspect <= back_buf_aspect ? numeric_cast<float32>(BaseFrameBufSize.height) * screen_aspect : numeric_cast<float32>(BaseFrameBufSize.height) * back_buf_aspect);
        const auto fit_height = iround<int32>(screen_aspect <= back_buf_aspect ? numeric_cast<float32>(BaseFrameBufSize.width) / back_buf_aspect : numeric_cast<float32>(BaseFrameBufSize.width) / screen_aspect);

        vp_ox = (BaseFrameBufSize.width - fit_width) / 2;
        vp_oy = (BaseFrameBufSize.height - fit_height) / 2;
        vp_width = fit_width;
        vp_height = fit_height;
        screen_width = Settings->ScreenWidth;
        screen_height = Settings->ScreenHeight;
    }

    ViewPortRect = irect32 {vp_ox, vp_oy, vp_width, vp_height};
    GL(glViewport(vp_ox, vp_oy, vp_width, vp_height));

    ProjectionMatrixColMaj = CreateOrthoMatrix(0.0f, numeric_cast<float32>(screen_width), numeric_cast<float32>(screen_height), 0.0f, -10.0f, 10.0f);
    ProjectionMatrixColMaj.Transpose(); // Convert to column major order

    TargetSize = {screen_width, screen_height};
}

void OpenGL_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    GLbitfield clear_flags = 0;

    if (color.has_value()) {
        const auto r = numeric_cast<float32>(color.value().comp.r) / 255.0f;
        const auto g = numeric_cast<float32>(color.value().comp.g) / 255.0f;
        const auto b = numeric_cast<float32>(color.value().comp.b) / 255.0f;
        const auto a = numeric_cast<float32>(color.value().comp.a) / 255.0f;
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

    int32 l;
    int32 t;
    int32 r;
    int32 b;

    if (ViewPortRect.width != TargetSize.width || ViewPortRect.height != TargetSize.height) {
        const float32 x_ratio = numeric_cast<float32>(ViewPortRect.width) / numeric_cast<float32>(TargetSize.width);
        const float32 y_ratio = numeric_cast<float32>(ViewPortRect.height) / numeric_cast<float32>(TargetSize.height);

        l = ViewPortRect.x + iround<int32>(numeric_cast<float32>(rect.x) * x_ratio);
        t = ViewPortRect.y + iround<int32>(numeric_cast<float32>(rect.y) * y_ratio);
        r = ViewPortRect.x + iround<int32>(numeric_cast<float32>(rect.x + rect.width) * x_ratio);
        b = ViewPortRect.y + iround<int32>(numeric_cast<float32>(rect.y + rect.height) * y_ratio);
    }
    else {
        l = ViewPortRect.x + rect.x;
        t = ViewPortRect.y + rect.y;
        r = ViewPortRect.x + rect.x + rect.width;
        b = ViewPortRect.y + rect.y + rect.height;
    }

    GL(glEnable(GL_SCISSOR_TEST));
    GL(glScissor(l, TargetSize.height - b, r - l, b - t));
}

void OpenGL_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    GL(glDisable(GL_SCISSOR_TEST));
}

void OpenGL_Renderer::OnResizeWindow(isize32 size)
{
    BaseFrameBufSize = size;

    if (BaseFrameBufObjBinded) {
        SetRenderTarget(nullptr);
    }
}

OpenGL_Texture::~OpenGL_Texture()
{
    FO_STACK_TRACE_ENTRY();

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

    FO_RUNTIME_ASSERT(Size.is_valid_pos(pos));

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

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);
    FO_RUNTIME_ASSERT(pos.x >= 0);
    FO_RUNTIME_ASSERT(pos.y >= 0);
    FO_RUNTIME_ASSERT(pos.x + size.width <= Size.width);
    FO_RUNTIME_ASSERT(pos.y + size.height <= Size.height);

    vector<ucolor> result;
    result.resize(numeric_cast<size_t>(size.width) * size.height);

    GLint prev_fbo;
    GL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, FramebufObj));
    GL(glReadPixels(pos.x, pos.y, size.width, size.height, GL_RGBA, GL_UNSIGNED_BYTE, result.data()));

    GL(glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo));

    return result;
}

void OpenGL_Texture::UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(pos.x >= 0);
    FO_RUNTIME_ASSERT(pos.y >= 0);
    FO_RUNTIME_ASSERT(pos.x + size.width <= Size.width);
    FO_RUNTIME_ASSERT(pos.y + size.height <= Size.height);

    if (use_dest_pitch) {
        GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, Size.width));
    }

    GL(glBindTexture(GL_TEXTURE_2D, TexId));
    GL(glTexSubImage2D(GL_TEXTURE_2D, 0, pos.x, pos.y, size.width, size.height, GL_RGBA, GL_UNSIGNED_BYTE, data));
    GL(glBindTexture(GL_TEXTURE_2D, 0));

    if (use_dest_pitch) {
        GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
    }
}

static void EnableVertAtribs(EffectUsage usage)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(usage);

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Position))));
        GL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Normal))));
        GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoord))));
        GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, TexCoordBase))));
        GL(glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Tangent))));
        GL(glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Bitangent))));
        GL(glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendWeights))));
        GL(glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, BlendIndices))));
        GL(glVertexAttribPointer(8, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex3D), reinterpret_cast<const GLvoid*>(offsetof(Vertex3D, Color))));

        for (GLuint i = 0; i <= 8; i++) {
            GL(glEnableVertexAttribArray(i));
        }

        return;
    }
#endif

    GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, PosX))));
    GL(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, Color))));
    GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, TexU))));
    GL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<const GLvoid*>(offsetof(Vertex2D, EggTexU))));

    for (GLuint i = 0; i <= 3; i++) {
        GL(glEnableVertexAttribArray(i));
    }
}

static void DisableVertAtribs(EffectUsage usage)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(usage);

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        for (GLuint i = 0; i <= 8; i++) {
            GL(glDisableVertexAttribArray(i));
        }

        return;
    }
#endif

    for (GLuint i = 0; i <= 3; i++) {
        GL(glDisableVertexAttribArray(i));
    }
}

OpenGL_DrawBuffer::OpenGL_DrawBuffer(bool is_static) :
    RenderDrawBuffer(is_static)
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
        FO_RUNTIME_ASSERT(Vertices.empty());
        upload_vertices = custom_vertices_size.value_or(VertCount);
        GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex3D), Vertices3D.data(), buf_type));
    }
    else {
        FO_RUNTIME_ASSERT(Vertices3D.empty());
        upload_vertices = custom_vertices_size.value_or(VertCount);
        GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex2D), Vertices.data(), buf_type));
    }
#else
    upload_vertices = custom_vertices_size.value_or(VertCount);
    GL(glBufferData(GL_ARRAY_BUFFER, upload_vertices * sizeof(Vertex2D), Vertices.data(), buf_type));
#endif

    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // Fill index buffer
    const auto upload_indices = custom_indices_size.value_or(IndCount);

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufObj));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, upload_indices * sizeof(vindex_t), Indices.data(), buf_type));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    // Vertex array
    if (VertexArrObj == 0 && GL_HAS(vertex_array_object)) {
        GL(glGenVertexArrays(1, &VertexArrObj));
        GL(glBindVertexArray(VertexArrObj));
        GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBufObj));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufObj));
        EnableVertAtribs(usage);
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

OpenGL_Effect::~OpenGL_Effect()
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < _passCount; i++) {
        if (Program[i] != 0) {
            glDeleteProgram(Program[i]);
        }
    }

    if (GL_HAS(uniform_buffer_object)) {
#define UBO_DELETE_BUFFER(buf) \
    if (Ubo_##buf != 0) { \
        glDeleteBuffers(1, &Ubo_##buf); \
    }
        UBO_DELETE_BUFFER(ProjBuf);
        UBO_DELETE_BUFFER(MainTexBuf);
        UBO_DELETE_BUFFER(ContourBuf);
        UBO_DELETE_BUFFER(TimeBuf);
        UBO_DELETE_BUFFER(RandomValueBuf);
        UBO_DELETE_BUFFER(ScriptValueBuf);
#if FO_ENABLE_3D
        UBO_DELETE_BUFFER(ModelBuf);
        UBO_DELETE_BUFFER(ModelTexBuf);
        UBO_DELETE_BUFFER(ModelAnimBuf);
#endif
#undef UBO_DELETE_BUFFER
    }
}

void OpenGL_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    const auto* opengl_dbuf = static_cast<OpenGL_DrawBuffer*>(dbuf);

#if FO_ENABLE_3D
    const auto* main_tex = static_cast<const OpenGL_Texture*>(custom_tex != nullptr ? custom_tex : (ModelTex[0] ? ModelTex[0].get() : (MainTex ? MainTex.get() : DummyTexture.get())));
#else
    const auto* main_tex = static_cast<const OpenGL_Texture*>(custom_tex != nullptr ? custom_tex : (MainTex ? MainTex.get() : DummyTexture.get()));
#endif

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

    if (opengl_dbuf->VertexArrObj != 0) {
        GL(glBindVertexArray(opengl_dbuf->VertexArrObj));
    }
    else {
        GL(glBindBuffer(GL_ARRAY_BUFFER, opengl_dbuf->VertexBufObj));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opengl_dbuf->IndexBufObj));
        EnableVertAtribs(_usage);
    }

    // Uniforms
    if (_needProjBuf && !ProjBuf.has_value()) {
        auto& proj_buf = ProjBuf = ProjBuffer();
        MemCopy(proj_buf->ProjMatrix, ProjectionMatrixColMaj[0], 16 * sizeof(float32));
    }

    if (_needMainTexBuf && !MainTexBuf.has_value()) {
        auto& main_tex_buf = MainTexBuf = MainTexBuffer();
        MemCopy(main_tex_buf->MainTexSize, main_tex->SizeData, 4 * sizeof(float32));
    }

    if (GL_HAS(uniform_buffer_object)) {
#define UBO_UPLOAD_BUFFER(buf) \
    if (_need##buf && buf.has_value()) { \
        if (Ubo_##buf == 0) { \
            GL(glGenBuffers(1, &Ubo_##buf)); \
        } \
        GL(glBindBuffer(GL_UNIFORM_BUFFER, Ubo_##buf)); \
        GL(glBufferData(GL_UNIFORM_BUFFER, sizeof(buf##fer), &buf.value(), GL_DYNAMIC_DRAW)); \
        GL(glBindBuffer(GL_UNIFORM_BUFFER, 0)); \
        buf.reset(); \
    }
        UBO_UPLOAD_BUFFER(ProjBuf);
        UBO_UPLOAD_BUFFER(MainTexBuf);
        UBO_UPLOAD_BUFFER(ContourBuf);
        UBO_UPLOAD_BUFFER(TimeBuf);
        UBO_UPLOAD_BUFFER(RandomValueBuf);
        UBO_UPLOAD_BUFFER(ScriptValueBuf);
#if FO_ENABLE_3D
        UBO_UPLOAD_BUFFER(ModelBuf);
        UBO_UPLOAD_BUFFER(ModelTexBuf);
        UBO_UPLOAD_BUFFER(ModelAnimBuf);
#endif
#undef UBO_UPLOAD_BUFFER
    }

    const auto* egg_tex = static_cast<const OpenGL_Texture*>(EggTex ? EggTex.get() : DummyTexture.get());
    const auto draw_count = numeric_cast<GLsizei>(indices_to_draw.value_or(opengl_dbuf->IndCount));
    const auto* start_pos = reinterpret_cast<const GLvoid*>(start_index * sizeof(vindex_t));

    for (size_t pass = 0; pass < _passCount; pass++) {
#if FO_ENABLE_3D
        if (DisableShadow && _isShadow[pass]) {
            continue;
        }
#endif

        if (GL_HAS(uniform_buffer_object)) {
#define UBO_BIND_BUFFER(buf) \
    if (Ubo_##buf != 0 && _pos##buf[pass] != -1) { \
        GL(glBindBufferBase(GL_UNIFORM_BUFFER, _pos##buf[pass], Ubo_##buf)); \
    }
            UBO_BIND_BUFFER(ProjBuf);
            UBO_BIND_BUFFER(MainTexBuf);
            UBO_BIND_BUFFER(ContourBuf);
            UBO_BIND_BUFFER(TimeBuf);
            UBO_BIND_BUFFER(RandomValueBuf);
            UBO_BIND_BUFFER(ScriptValueBuf);
#if FO_ENABLE_3D
            UBO_BIND_BUFFER(ModelBuf);
            UBO_BIND_BUFFER(ModelTexBuf);
            UBO_BIND_BUFFER(ModelAnimBuf);
#endif
#undef UBO_BIND_BUFFER
        }

        GL(glUseProgram(Program[pass]));

        if (_posMainTex[pass] != -1) {
            GL(glActiveTexture(GL_TEXTURE0 + _posMainTex[pass]));
            GL(glBindTexture(GL_TEXTURE_2D, main_tex->TexId));
            GL(glActiveTexture(GL_TEXTURE0));
        }

        if (_posEggTex[pass] != -1) {
            GL(glActiveTexture(GL_TEXTURE0 + _posEggTex[pass]));
            GL(glBindTexture(GL_TEXTURE_2D, egg_tex->TexId));
            GL(glActiveTexture(GL_TEXTURE0));
        }

#if FO_ENABLE_3D
        if (_needModelTex[pass]) {
            for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
                if (_posModelTex[pass][i] != -1) {
                    const auto* model_tex = static_cast<OpenGL_Texture*>(ModelTex[i] ? ModelTex[i].get() : DummyTexture.get());
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

        if constexpr (sizeof(vindex_t) == 2) {
            GL(glDrawElements(draw_mode, draw_count, GL_UNSIGNED_SHORT, start_pos));
        }
        else {
            GL(glDrawElements(draw_mode, draw_count, GL_UNSIGNED_INT, start_pos));
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
#define UBO_UNBIND_BUFFER(buf) \
    if (Ubo_##buf != 0 && _pos##buf[pass] != -1) { \
        GL(glBindBufferBase(GL_UNIFORM_BUFFER, _pos##buf[pass], 0)); \
    }
            UBO_UNBIND_BUFFER(ProjBuf);
            UBO_UNBIND_BUFFER(MainTexBuf);
            UBO_UNBIND_BUFFER(ContourBuf);
            UBO_UNBIND_BUFFER(TimeBuf);
            UBO_UNBIND_BUFFER(RandomValueBuf);
            UBO_UNBIND_BUFFER(ScriptValueBuf);
#if FO_ENABLE_3D
            UBO_UNBIND_BUFFER(ModelBuf);
            UBO_UNBIND_BUFFER(ModelTexBuf);
            UBO_UNBIND_BUFFER(ModelAnimBuf);
#endif
#undef UBO_UNBIND_BUFFER
        }
    }

    GL(glUseProgram(0));

    if (opengl_dbuf->VertexArrObj != 0) {
        GL(glBindVertexArray(0));
    }
    else {
        DisableVertAtribs(_usage);
        GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
        GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }

    if (DisableBlending) {
        GL(glEnable(GL_BLEND));
    }

#if FO_ENABLE_3D
    if (_usage == EffectUsage::Model) {
        GL(glDisable(GL_DEPTH_TEST));

        if (!DisableCulling) {
            GL(glDisable(GL_CULL_FACE));
        }
    }
#endif
}

FO_END_NAMESPACE();

#endif
